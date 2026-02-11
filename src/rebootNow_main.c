/**
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include "rdk_debug.h"
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include "rebootNow.h"
#include "secure_wrapper.h"
#include "rbus_interface.h"
#include "rdk_logger.h"
#include <sys/wait.h>

#define PROGRAM_NAME "rebootnow"

static const char *REBOOTINFO_LOG = "/opt/logs/rebootInfo.log";
static const char *REBOOT_INFO_DIR = "/opt/secure/reboot";
static const char *REBOOT_INFO_FILE = "/opt/secure/reboot/reboot.info";
static const char *PARODUS_REBOOT_INFO_FILE = "/opt/secure/reboot/parodusreboot.info";
static const char *REBOOTNOW_FLAG = "/opt/secure/reboot/rebootNow";

#ifdef RDK_LOGGER_ENABLED
int g_rdk_logger_enabled = 0;
#endif

static const char *APP_TRIGGERED_REASONS[] = {
    "Servicemanager", "systemservice_legacy", "WarehouseReset", "WarehouseService",
    "HrvInitWHReset", "HrvColdInitReset", "HtmlDiagnostics", "InstallTDK", "StartTDK",
    "TR69Agent", "SystemServices", "Bsu_GUI", "SNMP", "CVT_CDL", "Nxserver",
    "DRM_Netflix_Initialize", "hrvinit", "PaceMFRLibrary",
};

static const char *OPS_TRIGGERED_REASONS[] = {
    "ScheduledReboot", "RebootSTB.sh", "FactoryReset", "UpgradeReboot_firmwareDwnld.sh",
    "UpgradeReboot_restore", "XFS", "wait_for_pci0_ready", "websocketproxyinit", "NSC_IR_EventReboot",
    "host_interface_dma_bus_wait", "usbhotplug", "Receiver_MDVRSet", "Receiver_VidiPath_Enabled",
    "Receiver_Toggle_Optimus", "S04init_ticket", "Network-Service", "monitor.sh", "ecmIpMonitor.sh",
    "monitorMfrMgr.sh", "vlAPI_Caller_Upgrade", "ImageUpgrade_rmf_osal", "ImageUpgrade_mfr_api",
    "ImageUpgrade_updateNewImage.sh", "ImageUpgrade_userInitiatedFWDnld.sh", "ClearSICache",
    "tr69hostIfReset", "hostIf_utils", "hostifDeviceInfo", "HAL_SYS_Reboot", "UpgradeReboot_deviceInitiatedFWDnld.sh",
    "UpgradeReboot_rdkvfwupgrader", "UpgradeReboot_ipdnl.sh", "PowerMgr_Powerreset", "PowerMgr_coldFactoryReset",
    "DeepSleepMgr", "PowerMgr_CustomerReset", "PowerMgr_PersonalityReset", "Power_Thermmgr", "PowerMgr_Plat",
    "HAL_CDL_notify_mgr_event", "vldsg_estb_poll_ecm_operational_state", "BcmIndicateEcmReset", "SASWatchDog",
    "BP3_Provisioning", "eMMC_FW_UPGRADE", "BOOTLOADER_UPGRADE", "cdl_service", "BCMCommandHandler",
    "BRCM_Image_Validate", "docsis_mode_check.sh", "tch_nvram.sh", "Receiver", "CANARY_Update", "boot_FSR",
};

static const char *MAINTENANCE_TRIGGERED_REASONS[] = {
    "AutoReboot.sh", "PwrMgr",
};

static void usage(FILE *out)
{
    fprintf(out, "Usage: %s [options]\n", PROGRAM_NAME);
    fprintf(out, "\n");
    fprintf(out, "Script-equivalent arguments for rebootNow.sh:\n");
    fprintf(out, "  -s <source>        Source process triggering reboot (normal)\n");
    fprintf(out, "  -c <source>        Source process triggering reboot (crash)\n");
    fprintf(out, "  -r <custom>        Custom reason (e.g., MAINTENANCE_REBOOT)\n");
    fprintf(out, "  -o <other>         Other descriptive reason text\n");
    fprintf(out, "  -h                 Show this help and exit\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  %s -s HtmlDiagnostics -o \"User requested reboot\"\n", PROGRAM_NAME);
    fprintf(out, "  %s -c dsMgrMain -r MAINTENANCE_REBOOT -o \"Crash detected\"\n", PROGRAM_NAME);
}

static int checkstringvalue(const char *const *list, size_t n, const char *needle)
{
    for (size_t i = 0; i < n; ++i) {
        if (list[i] && needle && strstr(list[i], needle)) {
            return 1;
        }
    }
    return 0;
}

/* Description: Use for sending telemetry Log
 * @param marker: use for send marker details
 * @return : void
 * */
void t2CountNotify(const char *marker, int val) {
#ifdef T2_EVENT_ENABLED
    t2_event_d(marker, val);
#endif
}

void t2ValNotify(const char *marker, const char *val)
{
#ifdef T2_EVENT_ENABLED
    t2_event_s(marker, val);
#endif
}

static void emit_t2_for_source(const char *source, int is_crash)
{
    if (!source || !*source) {
        return;
    }

    /* Map a few special cases to match script behavior */
    const char *marker = NULL;
    if (!is_crash) {
        if (strstr(source, "runPodRecovery")) {
            marker = "SYST_ERR_RunPod_reboot";
        } else if (strstr(source, "CardNotResponding")) {
            marker = "SYST_ERR_CCNotResponding_reboot";
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "SYST_ERR_%s", source);
            t2CountNotify(buf, 1);
            return;
        }
    } else {
        if (strstr(source, "dsMgrMain")) {
            marker = "SYST_ERR_DSMGR_reboot";
        } else if (strstr(source, "IARMDaemonMain")) {
            marker = "SYST_ERR_IARMDEMON_reboot";
        } else if (strstr(source, "rmfStreamer")) {
            marker = "SYST_ERR_Rmfstreamer_reboot";
        } else if (strstr(source, "runPod")) {
            marker = "SYST_ERR_RunPod_reboot";
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "SYST_ERR_%s_reboot", source);
            t2CountNotify(buf, 1);
            return;
        }
    }
    if (marker) {
        t2CountNotify(marker, 1);
    }
}

static void signal_cleanup_handler(int signum)
{
    (void)signum;
    cleanup_pidfile();
}

static size_t UpdateRebootLog(char *buffer, size_t buffer_size, size_t bytes_used, const char *format, ...)
{
    size_t remaining;
    va_list args;
    int n;

    if (!buffer || buffer_size == 0) {
        return 0;
    }
    if (bytes_used >= buffer_size - 1) {
        buffer[buffer_size - 1] = '\0';
        return buffer_size - 1;
    }
    remaining = buffer_size - bytes_used;
    va_start(args, format);
    n = vsnprintf(buffer + bytes_used, remaining, format, args);
    va_end(args);
    if (n < 0) {
        buffer[buffer_size - 1] = '\0';
        return bytes_used;
    }
    if ((size_t)n >= remaining) {
        /* Truncated: buffer end is already null-terminated by vsnprintf */
        return buffer_size - 1;
    }
    return bytes_used + (size_t)n;
}

int main(int argc, char **argv)
{
    const char *source = NULL;      // from -s or -c
    int is_crash = 0;               // whether -c was used
    const char *customReason = "Unknown";
    const char *otherReason = "Unknown";
    bool Mng_Notify_Enable = false;
    char par_line[1024];
    char line[1024];
    size_t bytes_used = 0;
    int opt;
    int pid_status = 0;

    rdk_logger_ext_config_t config = {
        .pModuleName = "LOG.RDK.REBOOTINFO",     /* Module name */
        .loglevel = RDK_LOG_INFO,                 /* Default log level */
        .output = RDKLOG_OUTPUT_CONSOLE,          /* Output to console (stdout/stderr) */
        .format = RDKLOG_FORMAT_WITH_TS,          /* Timestamped format */
        .pFilePolicy = NULL                       /* Not using file output, so NULL */
    };
    
    if (rdk_logger_ext_init(&config) != RDK_SUCCESS) {
        printf("REBOOTINFO : ERROR - Extended logger init failed\n");
    }

    if (0 == rdk_logger_init("/etc/debug.ini")) {
        g_rdk_logger_enabled = 1;
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "[%s:%d] RDK Logger initialized\n", __FUNCTION__, __LINE__);
    }

#ifdef T2_EVENT_ENABLED
    t2_init("reboot-manager");
#endif

/* Initialize RBUS before any get/set */
    (void)rbus_init();
	
    if (pidfile_write_and_guard() != 0) {
        return 1;
    }
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Start of rebootNow Binary\n");
    atexit(cleanup_pidfile);
    signal(SIGINT, signal_cleanup_handler);
    signal(SIGTERM, signal_cleanup_handler);
    
    while ((opt = getopt(argc, argv, "s:c:r:o:h")) != -1) {
        switch (opt) {
            case 's':
                if (is_crash || source) {
                    usage(stderr);
                    return 1;
                }
                source = optarg;
                is_crash = 0;
                break;
            case 'c':
                if (is_crash || source) {
                    usage(stderr);
                    return 1;
                }
                source = optarg;
                is_crash = 1;
                break;
            case 'r':
                customReason = optarg;
                break;
            case 'o':
                otherReason = optarg;
                break;
            case 'h':
            default:
                usage(stdout);
                return (opt == 'h') ? 0 : 1;
        }
    }

    if (!source) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", "Exiting as no argument specified to identify source of reboot!!!\n");
        usage(stderr);
        return 1;
    }

    // Build rebootLogReason similar to the shell script
    char *rebootLogReason = NULL;
    size_t rebootLogReasonLen = 0;
    if (is_crash) {
        /* First determine required length for the formatted string */
        rebootLogReasonLen = (size_t)snprintf(NULL, 0,
                                            "Triggered from %s process failure or crash..!",
                                            source);
        rebootLogReason = (char *)malloc(rebootLogReasonLen + 1);
        if (rebootLogReason != NULL) {
            (void)snprintf(rebootLogReason, rebootLogReasonLen + 1,
                           "Triggered from %s process failure or crash..!",
                           source);
        }
    } else {
        /* First determine required length for the formatted string */
        rebootLogReasonLen = (size_t)snprintf(NULL, 0,
                                            "Triggered from %s process",
                                            source);
        rebootLogReason = (char *)malloc(rebootLogReasonLen + 1);
        if (rebootLogReason != NULL) {
            (void)snprintf(rebootLogReason, rebootLogReasonLen + 1,
                           "Triggered from %s process",
                           source);
        }
    }
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Reboot requested on the device from Source:%s Reason:%s\n", source, otherReason);
    emit_t2_for_source(source, is_crash);

    if (rebootLogReason != NULL) {
        free(rebootLogReason);
        rebootLogReason = NULL;
    }
    // Categorization
    const char *rebootReason = "FIRMWARE_FAILURE";
    if (checkstringvalue(APP_TRIGGERED_REASONS, sizeof(APP_TRIGGERED_REASONS)/sizeof(APP_TRIGGERED_REASONS[0]), source)) {
        rebootReason = "APP_TRIGGERED";
        if (customReason && strcmp(customReason, "MAINTENANCE_REBOOT") == 0) {
            rebootReason = "MAINTENANCE_REBOOT";
        }
    } else if (checkstringvalue(OPS_TRIGGERED_REASONS, sizeof(OPS_TRIGGERED_REASONS)/sizeof(OPS_TRIGGERED_REASONS[0]), source)) {
        rebootReason = "OPS_TRIGGERED";
    } else if (checkstringvalue(MAINTENANCE_TRIGGERED_REASONS, sizeof(MAINTENANCE_TRIGGERED_REASONS)/sizeof(MAINTENANCE_TRIGGERED_REASONS[0]), source)) {
        rebootReason = "MAINTENANCE_REBOOT";
    } else {
        rebootReason = "FIRMWARE_FAILURE";
    }

    // Log into rebootInfo.log in a similar format
    char ts[64];
    timestamp_update(ts, sizeof(ts));
    
    bytes_used = UpdateRebootLog(line, sizeof(line), bytes_used, "RebootReason: ");
    if (strcmp(otherReason, "Unknown") == 0) {
        bytes_used = UpdateRebootLog(line, sizeof(line), bytes_used, "%s\n", rebootLogReason);
    } else {
        bytes_used = UpdateRebootLog(line, sizeof(line), bytes_used, "%s ", rebootLogReason);
        bytes_used = UpdateRebootLog(line, sizeof(line), bytes_used, "%s\n", otherReason);
    }
    line[sizeof(line) - 1] = '\0';

    append_line_to_file(REBOOTINFO_LOG, line);

    snprintf(line, sizeof(line), "RebootInitiatedBy: %s\n", source);
    append_line_to_file(REBOOTINFO_LOG, line);

    snprintf(line, sizeof(line), "RebootTime: %s\n", ts);
    append_line_to_file(REBOOTINFO_LOG, line);

    snprintf(line, sizeof(line), "CustomReason: %s\n", customReason);
    append_line_to_file(REBOOTINFO_LOG, line);

    snprintf(line, sizeof(line), "OtherReason: %s\n", otherReason);
    append_line_to_file(REBOOTINFO_LOG, line);

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Categorized reboot as %s (source=%s, custom=%s, other=%s)\n",
            rebootReason, source, customReason, otherReason);
   
   /* if (adjust_hal_sys_reboot_source(source, &adj_source, &adj_other)) {
        if (adj_source) source = adj_source;
        if (adj_other) otherReason = adj_other;
    } */
  
    struct stat st;
    if (stat(REBOOT_INFO_DIR, &st) != 0) {
        if (mkdir(REBOOT_INFO_DIR, 0755) != 0) {
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Failed to create %s (errno=%d)\n", REBOOT_INFO_DIR, errno);
        } else {
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Creating %s folder\n", REBOOT_INFO_DIR);
        }
    }

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Invoke setPreviousRebootInfo to save reboot information under %s folder\n", REBOOT_INFO_DIR);
    FILE *jsonf = fopen(REBOOT_INFO_FILE, "w");
    if (jsonf) {
        const char *p;
        fprintf(jsonf, "{\n");

        /* "timestamp" field with JSON escaping */
        fprintf(jsonf, "\"timestamp\":\"");
        if (ts) {
            for (p = ts; *p; ++p) {
                unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '\\':
                        fputs("\\\\", jsonf);
                        break;
                    case '\"':
                        fputs("\\\"", jsonf);
                        break;
                    case '\b':
                        fputs("\\b", jsonf);
                        break;
                    case '\f':
                        fputs("\\f", jsonf);
                        break;
                    case '\n':
                        fputs("\\n", jsonf);
                        break;
                    case '\r':
                        fputs("\\r", jsonf);
                        break;
                    case '\t':
                        fputs("\\t", jsonf);
                        break;
                    default:
                        if (c < 0x20) {
                            fprintf(jsonf, "\\u%04x", c);
                        } else {
                            fputc(c, jsonf);
                        }
                        break;
                }
            }
        }
        fprintf(jsonf, "\",\n");

        /* "source" field with JSON escaping */
        fprintf(jsonf, "\"source\":\"");
        if (source) {
            for (p = source; *p; ++p) {
                unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '\\':
                        fputs("\\\\", jsonf);
                        break;
                    case '\"':
                        fputs("\\\"", jsonf);
                        break;
                    case '\b':
                        fputs("\\b", jsonf);
                        break;
                    case '\f':
                        fputs("\\f", jsonf);
                        break;
                    case '\n':
                        fputs("\\n", jsonf);
                        break;
                    case '\r':
                        fputs("\\r", jsonf);
                        break;
                    case '\t':
                        fputs("\\t", jsonf);
                        break;
                    default:
                        if (c < 0x20) {
                            fprintf(jsonf, "\\u%04x", c);
                        } else {
                            fputc(c, jsonf);
                        }
                        break;
                }
            }
        }
        fprintf(jsonf, "\",\n");

        /* "reason" field with JSON escaping */
        fprintf(jsonf, "\"reason\":\"");
        if (rebootReason) {
            for (p = rebootReason; *p; ++p) {
                unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '\\':
                        fputs("\\\\", jsonf);
                        break;
                    case '\"':
                        fputs("\\\"", jsonf);
                        break;
                    case '\b':
                        fputs("\\b", jsonf);
                        break;
                    case '\f':
                        fputs("\\f", jsonf);
                        break;
                    case '\n':
                        fputs("\\n", jsonf);
                        break;
                    case '\r':
                        fputs("\\r", jsonf);
                        break;
                    case '\t':
                        fputs("\\t", jsonf);
                        break;
                    default:
                        if (c < 0x20) {
                            fprintf(jsonf, "\\u%04x", c);
                        } else {
                            fputc(c, jsonf);
                        }
                        break;
                }
            }
        }
        fprintf(jsonf, "\",\n");

        /* "customReason" field with JSON escaping */
        fprintf(jsonf, "\"customReason\":\"");
        if (customReason) {
            for (p = customReason; *p; ++p) {
                unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '\\':
                        fputs("\\\\", jsonf);
                        break;
                    case '\"':
                        fputs("\\\"", jsonf);
                        break;
                    case '\b':
                        fputs("\\b", jsonf);
                        break;
                    case '\f':
                        fputs("\\f", jsonf);
                        break;
                    case '\n':
                        fputs("\\n", jsonf);
                        break;
                    case '\r':
                        fputs("\\r", jsonf);
                        break;
                    case '\t':
                        fputs("\\t", jsonf);
                        break;
                    default:
                        if (c < 0x20) {
                            fprintf(jsonf, "\\u%04x", c);
                        } else {
                            fputc(c, jsonf);
                        }
                        break;
                }
            }
        }
        fprintf(jsonf, "\",\n");

        /* "otherReason" field with JSON escaping */
        fprintf(jsonf, "\"otherReason\":\"");
        if (otherReason) {
            for (p = otherReason; *p; ++p) {
                unsigned char c = (unsigned char)*p;
                switch (c) {
                    case '\\':
                        fputs("\\\\", jsonf);
                        break;
                    case '\"':
                        fputs("\\\"", jsonf);
                        break;
                    case '\b':
                        fputs("\\b", jsonf);
                        break;
                    case '\f':
                        fputs("\\f", jsonf);
                        break;
                    case '\n':
                        fputs("\\n", jsonf);
                        break;
                    case '\r':
                        fputs("\\r", jsonf);
                        break;
                    case '\t':
                        fputs("\\t", jsonf);
                        break;
                    default:
                        if (c < 0x20) {
                            fprintf(jsonf, "\\u%04x", c);
                        } else {
                            fputc(c, jsonf);
                        }
                        break;
                }
            }
        }
        fprintf(jsonf, "\"\n");
        fprintf(jsonf, "}\n");
        fclose(jsonf);

        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Saving reboot info in %s file\n", REBOOT_INFO_FILE);
        snprintf(par_line, sizeof(par_line), "PreviousRebootInfo:%s,%s,%s,%s\n", ts, customReason, source ? source : "", rebootReason);
        append_line_to_file(PARODUS_REBOOT_INFO_FILE, par_line);
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Updated Reboot Reason information in %s and %s\n", REBOOT_INFO_FILE, PARODUS_REBOOT_INFO_FILE);
    } else {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Failed to open %s for writing (errno=%d)\n", REBOOT_INFO_FILE, errno);
    }

    /* Delegate cyclic reboot detection and scheduling to module */
    int proceed = 1;
    /* Module returns 0 to defer reboot, 1 to proceed */
    proceed = handle_cyclic_reboot(source, rebootReason, customReason, otherReason);
    if (proceed == 0) {
        return 0; /* exit without performing immediate reboot */
    }

    if (rbus_get_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.ManageableNotification.Enable", &Mng_Notify_Enable))
    {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Value of Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.ManageableNotification.Enable: %s\n", 
			                             Mng_Notify_Enable ? "true" : "false");
        if (Mng_Notify_Enable) {
            rbus_set_int_param("Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.RPC.RebootPendingNotification", 10);
        }
    }

    // Housekeeping before reboot
    perform_housekeeping();
    
    FILE *rebootFlag = fopen(REBOOTNOW_FLAG, "a");
    if (rebootFlag) {
        fclose(rebootFlag);
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Creating %s as the reboot was triggered by RDK software\n", REBOOTNOW_FLAG);
    } else {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Failed to create %s (errno=%d)\n", REBOOTNOW_FLAG, errno);
    }

    rbus_cleanup();
   /* if (adj_source) {
        free(adj_source);
    }
    if (adj_other) {
        free(adj_other);
    } */
    
    // Execute reboot sequence: reboot &, wait, fallback to systemctl reboot, then reboot -f
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Rebooting the Device Now\n");
    pid_t pid = fork();
    if (pid == 0) {
        execlp("reboot", "reboot", (char *)NULL);
        _exit(127);
    }
    sleep(90);
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","System still running after reboot command, Reboot Failed for %d...\n", (int)pid);
    int rc = v_secure_system("systemctl reboot");
    if (rc == 256 /* exit status 1 << 8 */ || (rc != 0 && rc != -1)) {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Reboot failed due to systemctl hang or connection timeout\n");
    }
    if (pid > 0) {
        kill(pid, SIGTERM);
        (void)waitpid(pid, &pid_status, WNOHANG);
    }
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Triggering force Reboot after standard soft reboot failure\n");
    v_secure_system("reboot -f");
    return 0;
}

