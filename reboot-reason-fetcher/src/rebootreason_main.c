#include "update-reboot-info.h"
#include "rdk_logger.h"

int find_previous_reboot_log(char *out_path, size_t len);

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
    
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Loading environment context \n");
    if (parse_device_properties(&ctx) != SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to parse device properties \n");
        ret = ERROR_PARSE_FAILED;
        goto cleanup;
    }

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Checking flags to update reboot reason \n");
    if (!update_reboot_info(&ctx)) {
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
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","New %s file found, Creating previous reboot info file...\n",REBOOT_INFO_FILE);
        log_reason(REBOOT_INFO_FILE);
        if (rename(REBOOT_INFO_FILE, PREVIOUS_REBOOT_INFO_FILE) != 0) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Failed to rename reboot.info: %s\n", strerror(errno));
        } else {
            has_reboot_info = true;
        }
	if (access(PARODUS_REBOOT_INFO_FILE, F_OK) == 0) {
            handle_parodus_reboot_file(&rebootInfo, PREVIOUS_PARODUSREBOOT_INFO_FILE);
	}
    }
    else {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Deriving reboot reason from legacy sources \n");

        char prev_log_path[MAX_PATH_LENGTH] = {0};
	if (find_previous_reboot_log(prev_log_path, sizeof(prev_log_path)) == SUCCESS) {
            if (parse_legacy_log(prev_log_path, &rebootInfo) != SUCCESS) {
                 RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Parse of previous reboot log failed, will derive from hardware/panic \n");
            }
	} else {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","No previous reboot log found, will derive from hardware/panic \n");
	}

        if (rebootInfo.timestamp[0] == '\0') {
            get_current_timestamp(rebootInfo.timestamp, sizeof(rebootInfo.timestamp));
        }

        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Detecting kernel panic \n");
        detect_kernel_panic(&ctx, &panicInfo);

        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Checking firmware failures \n");
        check_firmware_failure(&ctx, &fwFailure);

        if (rebootInfo.customReason[0] == '\0' || rebootInfo.source[0] == '\0') {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Getting hardware reboot reason \n");
            get_hardware_reason(&ctx, &hwReason, &rebootInfo);
        } else {
            memset(&hwReason, 0, sizeof(HardwareReason));
        }

        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Classifying reboot reason \n");
        if (classify_reboot_reason(&rebootInfo, &ctx, &hwReason, &panicInfo, &fwFailure) != SUCCESS) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to classify reboot reason \n");
            ret = ERROR_GENERAL;
            goto cleanup;
        }
    }

    if (update_previous_reboot_log_fields(has_reboot_info ? PREVIOUS_REBOOT_INFO_FILE : NULL, &rebootInfo) != SUCCESS) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Skipping PreviousReboot* update in %s due to missing reboot info fields\n", REBOOT_INFO_LOG_FILE);
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
    copy_keypress_info(KEYPRESS_INFO_FILE, PREVIOUS_KEYPRESS_INFO_FILE);

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot reason processing completed successfully \n");

  cleanup:
    if (lock_acquired) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Releasing lock \n");
        if (release_lock(LOCK_DIR) != SUCCESS) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Failed to release lock \n");
            if (ret == SUCCESS) {
                ret = ERROR_GENERAL;
            }
        }
    }

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Reason Update completed with status: %d \n", ret);
    return ret;
}
