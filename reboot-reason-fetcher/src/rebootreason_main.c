#include "update-reboot-info.h"
#include "rdk_logger.h"
#include <fcntl.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/inotify.h>
int find_previous_reboot_log(char *out_path, size_t len);
int update_previous_reboot_log_fields(const char *jsonPath, const RebootInfo *fallbackInfo);

/** Sentinel written by dcm-agent backup_logs on successful completion.
 *  Reboot-manager waits for this before reading /opt/logs/PreviousLogs/ to
 *  ensure the directory is fully populated before deriving the reboot reason.
 *  Cross-repo interface: also defined in dcm-agent backup_logs/include/backup_logs.h
 *  and telemetry source/dcautil/dcautil.h.
 *  Any path change MUST be coordinated with both repositories. */
#define BACKUP_LOGS_DONE_FLAG      "/tmp/.backup_logs_done"
/** Directory and filename split required by inotify_add_watch(). */
#define BACKUP_LOGS_DONE_DIR       "/tmp"
#define BACKUP_LOGS_DONE_FILENAME  ".backup_logs_done"

#ifdef GTEST_ENABLE
#  define BACKUP_LOGS_SYNC_TIMEOUT_S  2u
#else
#  define BACKUP_LOGS_SYNC_TIMEOUT_S  60u
#endif

/** Sentinel written on successful invocation.
 *  Cross-repo interface: consumed by uploadstblogs reboot_setup().
 *  Any path change MUST be coordinated with the uploadstblogs repository. */
#define PATH_FLAG_INVOCATION        "/tmp/Update_rebootInfo_invoked"

void t2CountNotify(char *marker, int val) {
#ifdef T2_EVENT_ENABLED
    t2_event_d(marker, val);
#else
    (void)marker;
    (void)val;
#endif
}

void t2ValNotify( char *marker, char *val )
{
#ifdef T2_EVENT_ENABLED
    t2_event_s(marker, val);
#else
    (void)marker;
    (void)val;
#endif
}

static void wait_for_backup_logs_done(void)
{
    /* Fast path: sentinel already written by backup_logs */
    if (access(BACKUP_LOGS_DONE_FLAG, F_OK) == 0) {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "[%s:%d] backup_logs sentinel already present\n", __FUNCTION__, __LINE__);
        return;
    }

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "[%s:%d] Waiting up to %us for backup_logs sentinel %s\n", __FUNCTION__, __LINE__, BACKUP_LOGS_SYNC_TIMEOUT_S, BACKUP_LOGS_DONE_FLAG);

    int ifd = inotify_init1(IN_CLOEXEC);
    if (ifd < 0) {
        RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO", "[%s:%d] inotify_init1 failed (errno=%d); proceeding without waiting\n", __FUNCTION__, __LINE__, errno);
        return;
    }

    {
        int wd = inotify_add_watch(ifd, BACKUP_LOGS_DONE_DIR,
                                   IN_CREATE | IN_MOVED_TO);
        if (wd < 0) {
            RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO", "[%s:%d] inotify_add_watch on %s failed (errno=%d); proceeding without waiting\n", __FUNCTION__, __LINE__, BACKUP_LOGS_DONE_DIR, errno);
            close(ifd);
            return;
        }

        /* Re-check after watch is set — closes race between access() and add_watch */
        if (access(BACKUP_LOGS_DONE_FLAG, F_OK) == 0) {
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "[%s:%d] backup_logs sentinel detected (race resolved)\n", __FUNCTION__, __LINE__);
            inotify_rm_watch(ifd, wd);
            close(ifd);
            return;
        }

        struct timespec deadline;
        if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
            RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO", "[%s:%d] clock_gettime failed (errno=%d); proceeding without waiting\n", __FUNCTION__, __LINE__, errno);
            inotify_rm_watch(ifd, wd);
            close(ifd);
            return;
        }
        deadline.tv_sec += (time_t)BACKUP_LOGS_SYNC_TIMEOUT_S;

        int found = 0;
        char buf[sizeof(struct inotify_event) + NAME_MAX + 1];

        while (!found) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) == 0 &&
                now.tv_sec >= deadline.tv_sec) {
                break; /* timeout */
            }

            struct timeval tv = {2, 0};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(ifd, &fds);

            int ret = select(ifd + 1, &fds, NULL, NULL, &tv);
            if (ret < 0) {
                if (errno == EINTR) { continue; }
                break;
            }
            if (ret == 0) { continue; } /* 2 s heartbeat — re-check deadline */

            ssize_t len = read(ifd, buf, sizeof(buf));
            if (len <= 0) { continue; }

            ssize_t offset = 0;
            while (offset < len) {
                struct inotify_event *ev =
                    (struct inotify_event *)(buf + offset);
                if (ev->len > 0 &&
                    strcmp(ev->name, BACKUP_LOGS_DONE_FILENAME) == 0) {
                    found = 1;
                    break;
                }
                offset += (ssize_t)(sizeof(struct inotify_event) + ev->len);
            }
        }

        inotify_rm_watch(ifd, wd);
        close(ifd);

        if (found) {
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "[%s:%d] backup_logs sentinel detected\n", __FUNCTION__, __LINE__);
        } else {
            RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO", "[%s:%d] backup_logs sentinel absent after %us; PreviousLogs/ may be incomplete\n", __FUNCTION__, __LINE__, BACKUP_LOGS_SYNC_TIMEOUT_S);
        }
        return;
    }
}

static void get_current_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    strftime(buffer, size, "%a %b %d %H:%M:%S UTC %Y", tm_info);
}

static int check_dir_exists(const char *path)
{
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to create directory %s: %s\n", path, strerror(errno));
            return ERROR_GENERAL;
        }
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Created directory: %s\n", path);
    }
    return SUCCESS;
}

static void log_reason(const char *path)
{
    FILE *fp = fopen(path, "r");
    char buf[256];

    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO",
                "Failed to open %s for logging: %s\n", path, strerror(errno));
        return;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "%s", buf);
    }
    fclose(fp);
}
#ifndef GTEST_ENABLE
int main(void)
{
    EnvContext ctx;
    RebootInfo rebootInfo;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;
    int ret = SUCCESS;
    bool has_reboot_info = false;
    bool lock_acquired = false;

    rdk_logger_ext_config_t config = {
        .pModuleName = "LOG.RDK.REBOOTINFO",     /* Module name */
        .loglevel = RDK_LOG_INFO,                 /* Default log level */
        .output = RDKLOG_OUTPUT_CONSOLE,          /* Output to console (stdout/stderr) */
        .format = RDKLOG_FORMAT_WITH_TS,          /* Timestamped format */
        .pFilePolicy = NULL                       /* Not using file output, so NULL */
    };

    if (rdk_logger_ext_init(&config) != RDK_SUCCESS) {
        printf("REBOOTINFO : ERROR - Extended logger init failed\n");
        return ERROR_GENERAL;
    }

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "[%s:%d] RDK Logger initialized\n", __FUNCTION__, __LINE__);
 
#ifdef T2_EVENT_ENABLED
    t2_init("update-reboot-info");
#endif
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Start of Reboot Reason \n");

    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Acquiring rebootInfo lock\n");
    if (acquire_lock(LOCK_DIR) != SUCCESS) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Failed to acquire lock, another instance may be running \n");
        return ERROR_LOCK_FAILED;
    }
    lock_acquired = true;
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Acquired rebootInfo lock\n");
    
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Loading environment context \n");
    if (parse_device_properties(&ctx) != SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to parse device properties \n");
        ret = ERROR_PARSE_FAILED;
        goto cleanup;
    }

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Checking /tmp/stt_received and /tmp/rebootInfo_Updated flag to update the reboot reason\n");
    if (!update_reboot_info(&ctx)) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Exiting since /tmp/stt_received or /tmp/rebootInfo_Updated flag is not available\n");
        ret = SUCCESS;
        goto cleanup;
    }

    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Ensuring reboot directory exists \n");
    if (check_dir_exists(REBOOT_INFO_DIR) != SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to create reboot directory \n");
        ret = ERROR_GENERAL;
        goto cleanup;
    }

    memset(&rebootInfo, 0, sizeof(RebootInfo));
    get_current_timestamp(rebootInfo.timestamp, sizeof(rebootInfo.timestamp));

    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Checking for new reboot.info file \n");
    if (access(REBOOT_INFO_FILE, F_OK) == 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","New %s file found, Creating previous reboot info file...\n",REBOOT_INFO_FILE);
        log_reason(REBOOT_INFO_FILE);
        if (rename(REBOOT_INFO_FILE, PREVIOUS_REBOOT_INFO_FILE) != 0) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Failed to rename reboot.info: %s\n", strerror(errno));
        } else {
            has_reboot_info = true;
        }
	if (access(PARODUS_REBOOT_INFO_FILE, F_OK) == 0) {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","New %s file found, updating parodus logfile...\n", PARODUS_REBOOT_INFO_FILE);
            handle_parodus_reboot_file(&rebootInfo, PREVIOUS_PARODUSREBOOT_INFO_FILE);
	}
    }
    else {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Deriving reboot reason from legacy sources \n");
       
        /* Soft gate: ensure backup_logs has finished populating PreviousLogs/
         * before any of the legacy-source functions read from that directory. */
        wait_for_backup_logs_done();
        if (rebootInfo.timestamp[0] == '\0') {
            get_current_timestamp(rebootInfo.timestamp, sizeof(rebootInfo.timestamp));
        }

        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Detecting kernel panic \n");
        detect_kernel_panic(&ctx, &panicInfo);

        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Checking firmware failures \n");
        check_firmware_failure(&ctx, &fwFailure);

        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Getting hardware reboot reason for current boot \n");
        get_hardware_reason(&ctx, &hwReason, &rebootInfo);

        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Classifying reboot reason \n");
        if (classify_reboot_reason(&rebootInfo, &ctx, &hwReason, &panicInfo, &fwFailure) != SUCCESS) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to classify reboot reason \n");
            ret = ERROR_GENERAL;
            goto cleanup;
        }
    }
    // Updating messages.txt
    update_kernel_log(&ctx, &rebootInfo);

    if (update_previous_reboot_log_fields(has_reboot_info ? PREVIOUS_REBOOT_INFO_FILE : NULL, &rebootInfo) != SUCCESS) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Skipping PreviousReboot* update in %s due to missing reboot info fields\n", REBOOT_INFO_LOG_FILE);
    } else if (has_reboot_info) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Get previous reboot reason from %s - prevrebootreason: %s\n", PREVIOUS_REBOOT_INFO_FILE, rebootInfo.reason);
    }

    if (!has_reboot_info) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Persisting reboot information \n");
        if (write_reboot_info(PREVIOUS_REBOOT_INFO_FILE, &rebootInfo) != SUCCESS) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to write reboot info \n");
            ret = ERROR_GENERAL;
            goto cleanup;
        }
        update_parodus_log(&rebootInfo);
        handle_parodus_reboot_file(&rebootInfo, PREVIOUS_PARODUSREBOOT_INFO_FILE);
        if (strstr(rebootInfo.reason, "POWER_ON") ||
            strstr(rebootInfo.reason, "HARD_POWER") ||
            strstr(rebootInfo.reason, "UNKNOWN_RESET")) {
            write_hardpower(PREVIOUS_HARD_REBOOT_INFO_FILE, rebootInfo.timestamp);
        }
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Copying keypress info \n");
    if (access(KEYPRESS_INFO_FILE, F_OK) != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Unable to find the %s file\n", KEYPRESS_INFO_FILE);
    }
    copy_keypress_info(KEYPRESS_INFO_FILE, PREVIOUS_KEYPRESS_INFO_FILE);

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","End of Reboot Reason \n");

  cleanup:
    if (lock_acquired) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Releasing rebootInfo lock\n");
        if (release_lock(LOCK_DIR) != SUCCESS) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Failed to release lock \n");
            if (ret == SUCCESS) {
                ret = ERROR_GENERAL;
            }
        }
    }
    /* Write invocation sentinel so uploadstblogs can proceed */
    {
        int sentinel_fd = open(PATH_FLAG_INVOCATION, O_CREAT | O_WRONLY, 0644);
        if (sentinel_fd >= 0) {
            close(sentinel_fd);
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "[%s:%d] Invocation sentinel written: %s\n", __FUNCTION__, __LINE__, PATH_FLAG_INVOCATION);
        } else {
            RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO", "[%s:%d] Failed to write invocation sentinel %s: %s\n", __FUNCTION__, __LINE__, PATH_FLAG_INVOCATION, strerror(errno));
        }
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Reboot Reason Update completed with status: %d \n", ret);
    return ret;
}
#endif

#ifdef GTEST_ENABLE
void (*get_wait_for_backup_logs_done(void))(void)
{
    return &wait_for_backup_logs_done;
}
void (*get_current_timestamp_for_test(void))(char *, size_t)
{
    return &get_current_timestamp;
}

int (*get_check_dir_exists_for_test(void))(const char *)
{
    return &check_dir_exists;
}

void (*get_log_reason_for_test(void))(const char *)
{
    return &log_reason;
}
#endif
