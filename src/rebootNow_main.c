#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include "rdk_debug.h"
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include "rebootNow.h"
#include "secure_wrapper.h"

#define PROGRAM_NAME "rebootnow"

static const char *REBOOTINFO_LOG = "/opt/logs/rebootInfo.log";
static const char *REBOOT_INFO_DIR = "/opt/secure/reboot";
static const char *REBOOT_INFO_FILE = "/opt/secure/reboot/reboot.info";
static const char *PARODUS_REBOOT_INFO_FILE = "/opt/secure/reboot/parodusreboot.info";
static const char *REBOOTNOW_FLAG = "/opt/secure/reboot/rebootNow";
static const char *REBOOTREASON_LOG = "/opt/logs/rebootreason.log";

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
    fprintf(out,
            "Usage: %s -s <source> | -c <crash> [-r <custom>] [-o <other>]\n",
            PROGRAM_NAME);
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
void t2CountNotify(char *marker, int val) {
#ifdef T2_EVENT_ENABLED
    t2_event_d(marker, val);
#endif
}

void t2ValNotify( char *marker, char *val )
{
#ifdef T2_EVENT_ENABLED
    t2_event_s(marker, val);
#endif
}

int main(int argc, char **argv)
{
    const char *source = NULL;      // from -s or -c
    int is_crash = 0;               // whether -c was used
    const char *customReason = "Unknown";
    const char *otherReason = "Unknown";
    bool Mng_Notify_Enable = false;

#ifdef RDK_LOGGER_ENABLED
    if (0 == rdk_logger_init("/etc/debug.ini")) {
        g_rdk_logger_enabled = 1;
    }
#endif

#ifdef T2_EVENT_ENABLED
    t2_init("reboot-manager");
#endif
	
    if (pidfile_write_and_guard() != 0) {
        return 1;
    }
    atexit(cleanup_pidfile);
    signal(SIGINT, (void (*)(int))cleanup_pidfile);
    signal(SIGTERM, (void (*)(int))cleanup_pidfile);
    
    int opt;
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
                if (source) {
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
    char rebootLogReason[512];
    if (is_crash) {
        snprintf(rebootLogReason, sizeof(rebootLogReason), "Triggered from %s process failure or crash..!", source);
    } else {
        snprintf(rebootLogReason, sizeof(rebootLogReason), "Triggered from %s process", source);
    }

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Reboot requested on the device from Source:%s Reason:%s\n", source, otherReason);

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

    char line[1024];
    if (strcmp(otherReason, "Unknown") == 0) {
        snprintf(line, sizeof(line), "RebootReason: %s\n", rebootLogReason);
    } else {
        snprintf(line, sizeof(line), "RebootReason: %s %s\n", rebootLogReason, otherReason);
    }
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
    
    struct stat st;
    if (stat(REBOOT_INFO_DIR, &st) != 0) {
        if (mkdir(REBOOT_INFO_DIR, 0755) != 0) {
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Failed to create %s (errno=%d)\n", REBOOT_INFO_DIR, errno);
        } else {
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Creating %s folder\n", REBOOT_INFO_DIR);
        }
    }

    FILE *jsonf = fopen(REBOOT_INFO_FILE, "w");
    if (jsonf) {
        fprintf(jsonf, "{\n");
        fprintf(jsonf, "\"timestamp\":\"%s\",\n", ts);
        fprintf(jsonf, "\"source\":\"%s\",\n", source_buf);
        fprintf(jsonf, "\"reason\":\"%s\",\n", rebootReason);
        fprintf(jsonf, "\"customReason\":\"%s\",\n", customReason);
        fprintf(jsonf, "\"otherReason\":\"%s\"\n", other_buf);
        fprintf(jsonf, "}\n");
        fclose(jsonf);
        rebootLogf("Saving reboot info in %s file", REBOOT_INFO_FILE);
        // Parodus info line
        char par_line[1024];
        snprintf(par_line, sizeof(par_line), "PreviousRebootInfo:%s,%s,%s,%s\n", ts, customReason, source_buf, rebootReason);
        append_line_to_file(PARODUS_REBOOT_INFO_FILE, par_line);
	RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Updated Reboot Reason information in %s and %s\n", REBOOT_INFO_FILE, PARODUS_REBOOT_INFO_FILE);
    } else {
	RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Failed to open %s for writing (errno=%d)\n", REBOOT_INFO_FILE, errno);
    }

    // Touch rebootNow flag
    {
        FILE *rebootFlag = fopen(REBOOTNOW_FLAG, "a");
        if (rebootFlag) {
            fclose(rebootFlag);
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Creating %s as the reboot was triggred by RDK software", REBOOTNOW_FLAG);
        } else {
            RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Failed to create %s (errno=%d)", REBOOTNOW_FLAG, errno);
        }
    }

    /* Delegate cyclic reboot detection and scheduling to module */
    {
        int proceed = 1;
        /* Module returns 0 to defer reboot, 1 to proceed */
        proceed = handle_cyclic_reboot(source, rebootReason, customReason, otherReason);
        if (proceed == 0) {
            return 0; /* exit without performing immediate reboot */
        }
    }

	if (rbus_get_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.ManageableNotification.Enable", &Mng_Notify_Enable))
	{
		RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Manageable Notification Enabled\n");
		rbus_set(rrdRbusHandle,"Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.RPC.RebootPendingNotification", 10, NULL);
	}

    // Housekeeping before reboot
    perform_housekeeping();
    
    // Execute reboot sequence: reboot &, wait, fallback to systemctl reboot, then reboot -f
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Rebooting the Device Now\n");
    pid_t pid = fork();
    if (pid == 0) {
        // Child: exec reboot
        execlp("reboot", "reboot", (char *)NULL);
        // If execlp fails
        _exit(127);
    }
    // Parent: wait ~90 seconds
    sleep(90);
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","System still running after reboot command, Reboot Failed for %d...\n", (int)pid);
    int rc = v_secure_system("reboot");
    if (rc == 256 /* exit status 1 << 8 */ || (rc != 0 && rc != -1)) {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Reboot failed due to systemctl hang or connection timeout\n");
    }
    if (pid > 0) {
        kill(pid, SIGTERM);
    }
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO","Triggering force Reboot after standard soft reboot failure\n");
	v_secure_system("reboot -f");
    return 0;
}

