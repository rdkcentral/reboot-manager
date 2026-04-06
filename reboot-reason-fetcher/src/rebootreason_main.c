/*
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:

 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "update-reboot-info.h"
#include "rdk_logger.h"

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

static int touch_file(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", "Failed to touch %s: %s\n", path, strerror(errno));
        return ERROR_GENERAL;
    }
    fclose(fp);
    return SUCCESS;
}

static int process_running(const char *process_name)
{
    char cmd[256];
    char out[64];
    FILE *fp = NULL;

    if (!process_name || !*process_name) {
        return 0;
    }

    snprintf(cmd, sizeof(cmd), "pidof %s 2>/dev/null", process_name);
    fp = popen(cmd, "r");
    if (!fp) {
        return 0;
    }

    if (fgets(out, sizeof(out), fp) == NULL) {
        pclose(fp);
        return 0;
    }

    pclose(fp);
    return 1;
}

static int run_shutdown_mode(const char *process)
{
    const char *crash_flag = "/tmp/set_crash_reboot_flag";

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Running shutdown compatibility mode\n");

    if (access(crash_flag, F_OK) != 0) {
        (void)touch_file(crash_flag);
    }

    if (!process || !*process) {
        return SUCCESS;
    }

    if (strcmp(process, "iarmbusd") == 0) {
        if (!process_running("IARMDaemonMain")) {
            return SUCCESS;
        }
    } else if (strcmp(process, "dsmgr") == 0) {
        if (!process_running("dsMgrMain")) {
            return SUCCESS;
        }
    } else {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Unknown process (not in the reboot list): %s\n", process);
    }

    return SUCCESS;
}

static int run_default_mode(void)
{
    EnvContext ctx;
    RebootInfo rebootInfo;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;
    int ret = SUCCESS;
    bool has_reboot_info = false;
    bool lock_acquired = false;

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

        if (parse_legacy_log(REBOOT_INFO_LOG_FILE, &rebootInfo) != SUCCESS) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","No legacy log found or parse failed, will derive from hardware/panic \n");
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
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Creating invocation flag\n");
    FILE *flag_fp = fopen(UPDATE_REBOOT_INFO_INVOKED_FLAG, "w");
    if (flag_fp) {
        fprintf(flag_fp, "1\n");
        fclose(flag_fp);
    }

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

static int run_bootup_mode(void)
{
    const char *log_file = "/opt/logs/rebootInfo.log";
    const char *prev_log_path = "/opt/logs/PreviousLogs";
    const char *updated_flag = "/tmp/rebootInfo_Updated";
    RebootInfo bootupInfo;
    char reboot_reason[MAX_BUFFER_SIZE] = {0};
    char ts[MAX_TIMESTAMP_LENGTH] = {0};
    FILE *fp = NULL;

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Running bootup compatibility mode\n");

    memset(&bootupInfo, 0, sizeof(bootupInfo));
    (void)parse_bootup_legacy_reboot_log(prev_log_path,
                                         reboot_reason,
                                         sizeof(reboot_reason),
                                         &bootupInfo);

    get_current_timestamp(ts, sizeof(ts));
    fp = fopen(log_file, "w");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", "Failed to open %s: %s\n", log_file, strerror(errno));
        return ERROR_GENERAL;
    }

    fprintf(fp, "%s PreviousRebootReason: %s\n", ts, reboot_reason);
    fprintf(fp, "%s PreviousRebootInitiatedBy: %s\n", ts, bootupInfo.source);
    fprintf(fp, "%s PreviousRebootTime: %s\n", ts, bootupInfo.timestamp);
    fprintf(fp, "%s PreviousCustomReason: %s\n", ts, bootupInfo.customReason);
    fprintf(fp, "%s PreviousOtherReason: %s\n", ts, bootupInfo.otherReason);
    fclose(fp);

    (void)touch_file(updated_flag);
    return SUCCESS;
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [bootup|shutdown [iarmbusd|dsmgr]]\n", prog);
}

int main(int argc, char **argv)
{
    int ret = SUCCESS;
    const char *mode = (argc > 1) ? argv[1] : NULL;

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

    if (mode == NULL) {
        return run_default_mode();
    }

    if (strcmp(mode, "bootup") == 0) {
        ret = run_bootup_mode();
        if (ret != SUCCESS) {
            return ret;
        }
        return run_default_mode();
    }

    if (strcmp(mode, "shutdown") == 0) {
        return run_shutdown_mode((argc > 2) ? argv[2] : NULL);
    }

    if (strcmp(mode, "-h") == 0 || strcmp(mode, "--help") == 0) {
        print_usage(argv[0]);
        return SUCCESS;
    }

    print_usage(argv[0]);
    return ERROR_GENERAL;
}
