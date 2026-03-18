#include "update-reboot-info.h"
#include <strings.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "rdk_logger.h"

#define KERNEL_LOG_FILE "/opt/logs/messages.txt"
#define PSTORE_CONSOLE_LOG_FILE "/sys/fs/pstore/console-ramoops-0"
#define PREVIOUS_KERNEL_OOPS_DUMP "PREVIOUS_KERNEL_OOPS_DUMP"
#define UIMGR_LOG_FILE "/opt/logs/PreviousLogs/uimgr_log.txt"
#define OCAPRI_LOG_FILE "/opt/logs/PreviousLogs/ocapri_log.txt"
#define ECM_CRASH_LOG_FILE "/opt/logs/PreviousLogs/messages-ecm.txt"
#define MAX_REBOOT_STRING "Box has rebooted 10 times"
#define ECM_CRASH_STRING "**** CRASH ****"

// --- Classification arrays ---
static const char *APP_TRIGGERED_REASONS[] = {
    "Servicemanager", "systemservice_legacy", "WarehouseReset", "WarehouseService",
    "HrvInitWHReset", "HrvColdInitReset", "HtmlDiagnostics", "InstallTDK", "StartTDK",
    "TR69Agent", "SystemServices", "Bsu_GUI", "SNMP", "CVT_CDL", "Nxserver",
    "DRM_Netflix_Initialize", "hrvinit", "PaceMFRLibrary", NULL
};

static const char *OPS_TRIGGERED_REASONS[] = {
    "ScheduledReboot", "RebootSTB.sh", "FactoryReset", "UpgradeReboot_restore", "XFS",
    "wait_for_pci0_ready", "websocketproxyinit", "NSC_IR_EventReboot",
    "host_interface_dma_bus_wait", "usbhotplug", "Receiver_MDVRSet",
    "Receiver_VidiPath_Enabled", "Receiver_Toggle_Optimus", "S04init_ticket",
    "Network-Service", "monitorMfrMgr.sh", "vlAPI_Caller_Upgrade", "ImageUpgrade_rmf_osal",
    "ImageUpgrade_mfr_api", "ImageUpgrade_userInitiatedFWDnld.sh", "ClearSICache",
    "tr69hostIfReset", "hostIf_utils", "hostifDeviceInfo", "HAL_SYS_Reboot",
    "UpgradeReboot_deviceInitiatedFWDnld.sh", "UpgradeReboot_rdkvfwupgrader",
    "UpgradeReboot_ipdnl.sh", "PowerMgr_Powerreset", "PowerMgr_coldFactoryReset",
    "DeepSleepMgr", "PowerMgr_CustomerReset", "PowerMgr_PersonalityReset",
    "Power_Thermmgr", "PowerMgr_Plat", "HAL_CDL_notify_mgr_event",
    "vldsg_estb_poll_ecm_operational_state", "SASWatchDog", "BP3_Provisioning",
    "eMMC_FW_UPGRADE", "BOOTLOADER_UPGRADE", "cdl_service", "docsis_mode_check.sh",
    "Receiver", "CANARY_Update", "BRCM_Image_Validate", "tch_nvram.sh",
    "boot_FSR", "BCMCommandHandler", NULL
};

static const char *MAINTENANCE_TRIGGERED_REASONS[] = {
    "AutoReboot.sh", "PwrMgr", NULL
};

bool is_app_triggered(const char *reason)
{
    if (!reason) return false;
    for (int i = 0; APP_TRIGGERED_REASONS[i] != NULL; i++) {
        if (strcmp(reason, APP_TRIGGERED_REASONS[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool is_ops_triggered(const char *reason)
{
    if (!reason) return false;
    for (int i = 0; OPS_TRIGGERED_REASONS[i] != NULL; i++) {
        if (strcmp(reason, OPS_TRIGGERED_REASONS[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool is_maintenance_triggered(const char *reason)
{
    if (!reason) return false;
    for (int i = 0; MAINTENANCE_TRIGGERED_REASONS[i] != NULL; i++) {
        if (strcmp(reason, MAINTENANCE_TRIGGERED_REASONS[i]) == 0) {
            return true;
        }
    }
    return false;
}

static const char *panic_signatures[] = {
    "Kernel panic - not syncing",
    "Kernel Panic",
    "Kernel Oops",
    "Oops - undefined instruction",
    "Oops - bad syscall",
    "branch through zero",
    "unknown data abort code",
    "Illegal memory access",
    NULL
};

static bool search_panic_in_file(const char *filepath, PanicInfo *panicInfo)
{
    FILE *fp = NULL;
    char line[MAX_BUFFER_SIZE];
    if (access(filepath, F_OK) != 0) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","File does not exist: %s\n", filepath);
        return false;
    }
    fp = fopen(filepath, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", filepath, strerror(errno));
        return false;
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Searching for panic signatures in: %s\n", filepath);
    while (fgets(line, sizeof(line), fp)) {
        for (int i = 0; panic_signatures[i] != NULL; i++) {
            if (strstr(line, panic_signatures[i])) {
                strncpy(panicInfo->panicType, panic_signatures[i], sizeof(panicInfo->panicType) - 1);
                panicInfo->panicType[sizeof(panicInfo->panicType) - 1] = '\0';
                strncpy(panicInfo->details, line, sizeof(panicInfo->details) - 1);
                panicInfo->details[sizeof(panicInfo->details) - 1] = '\0';
                char *nl = strchr(panicInfo->details, '\n');
                if (nl) *nl = '\0';
                panicInfo->detected = true;
                fclose(fp);
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Kernel panic detected: %s\n", panicInfo->panicType);
                return true;
            }
        }
    }
    fclose(fp);
    return false;
}

static void copy_pstore_logs_to_opt(void)
{
    DIR *dir = NULL;
    struct dirent *ent;
    if (access(PSTORE_DIR, F_OK) != 0) {
        return;
    }
    dir = opendir(PSTORE_DIR);
    if (!dir) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", PSTORE_DIR, strerror(errno));
        return;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char src[MAX_PATH_LENGTH];
        char dst[MAX_PATH_LENGTH];
        size_t name_len_full = strlen(ent->d_name);
        size_t name_len = (name_len_full > 255) ? 255 : name_len_full;

        if (strlen(PSTORE_DIR) + 1 + name_len + 1 >= sizeof(src)) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Skipping PSTORE entry with long name: %s\n", ent->d_name);
            continue;
        }
        if (strlen("/opt/logs/") + name_len + strlen(".log") + 1 >= sizeof(dst)) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Skipping PSTORE entry (dst too long): %s\n", ent->d_name);
            continue;
        }
        int max_src_name = (int)(sizeof(src) - strlen(PSTORE_DIR) - 2); // '/' + '\0'
        if (max_src_name <= 0) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Skipping PSTORE entry (name too long for src): %s\n", ent->d_name);
            continue;
        }
        snprintf(src, sizeof(src), "%s/%.*s", PSTORE_DIR, max_src_name, ent->d_name);
        int max_dst_name = (int)(sizeof(dst) - strlen("/opt/logs/") - strlen(".log") - 1);
        if (max_dst_name <= 0) {
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Skipping PSTORE entry (name too long for dst): %s\n", ent->d_name);
            continue;
        }
        snprintf(dst, sizeof(dst), "/opt/logs/%.*s.log", max_dst_name, ent->d_name);
        FILE *fs = fopen(src, "rb");
        if (!fs) {
            continue;
        }
        FILE *fd = fopen(dst, "wb");
        if (!fd) { 
            fclose(fs); 
            continue; 
        }
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
            fwrite(buf, 1, n, fd);
        }
        fflush(fd);
        fclose(fs); fclose(fd);
    }
    closedir(dir);
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Copied PSTORE logs to /opt/logs \n");
}

int detect_kernel_panic(const EnvContext *ctx, PanicInfo *panicInfo)
{
    if (!ctx || !panicInfo) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for detect_kernel_panic \n");
        return ERROR_GENERAL;
    }
    memset(panicInfo, 0, sizeof(PanicInfo));
    panicInfo->detected = false;
    if (strcmp(ctx->soc, "BRCM") == 0) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Checking BRCM kernel panic in messages.txt \n");
        FILE *fp = fopen(KERNEL_LOG_FILE, "r");
        if (fp) {
            char line[MAX_BUFFER_SIZE];
            bool oops_marker_found = false;
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, PREVIOUS_KERNEL_OOPS_DUMP)) {
                    oops_marker_found = true;
                    break;
                }
            }
            fclose(fp);
            if (oops_marker_found) {
                search_panic_in_file(KERNEL_LOG_FILE, panicInfo);
            }
        }
    }
     // RTK/REALTEK (and TV profiles) check PSTORE console
    if (strcmp(ctx->soc, "RTK") == 0 || strcmp(ctx->soc, "REALTEK") == 0) {
        if (search_panic_in_file(PSTORE_CONSOLE_LOG_FILE, panicInfo)) {
            copy_pstore_logs_to_opt();
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","PSTORE indicates kernel panic; logs copied \n");
        }
    }
    if (panicInfo->detected) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Kernel panic detected: %s\n", panicInfo->panicType);
    } else {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","No kernel panic detected");
    }
    return SUCCESS;
}

static bool search_string_in_file(const char *filepath, const char *search_str)
{
    FILE *fp = NULL;
    char line[MAX_BUFFER_SIZE];
    bool found = false;
    if (access(filepath, F_OK) != 0) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","File does not exist: %s\n", filepath);
        return false;
    }
    fp = fopen(filepath, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", filepath, strerror(errno));
        return false;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Searching for '%s' in: %s\n", search_str, filepath);
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, search_str)) {
            found = true;
            break;
        }
    }
    fclose(fp);
    return found;
}

int check_firmware_failure(const EnvContext *ctx, FirmwareFailure *fwFailure)
{
    if (!ctx || !fwFailure) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Invalid parameters for check_firmware_failure\n");
        return ERROR_GENERAL;
    }
    memset(fwFailure, 0, sizeof(FirmwareFailure));
    bool is_mediaclient = (strncmp(ctx->device_type, "mediaclient", strlen("mediaclient")) == 0);
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Checking firmware failures for %s device\n", is_mediaclient ? "mediaclient" : "STB");
    const char *max_reboot_log = is_mediaclient ? UIMGR_LOG_FILE : OCAPRI_LOG_FILE;
    const char *source_name = is_mediaclient ? "UiMgr" : "OcapRI";
    if (search_string_in_file(max_reboot_log, MAX_REBOOT_STRING)) {
        fwFailure->maxRebootDetected = true;
        fwFailure->detected = true;
        snprintf(fwFailure->details, sizeof(fwFailure->details), "%s: Reboot due to STB reached maximum (10) reboots", source_name);
		strncpy(fwFailure->initiator, source_name, sizeof(fwFailure->initiator) - 1);
		fwFailure->initiator[sizeof(fwFailure->initiator) - 1] = '\0';
		t2CountNotify("SYST_ERR_10Times_reboot", 1);
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Max reboot detected in %s\n", max_reboot_log);
    }
    if (!is_mediaclient && search_string_in_file(ECM_CRASH_LOG_FILE, ECM_CRASH_STRING)) {
        fwFailure->ecmCrashDetected = true;
        fwFailure->detected = true;
        if (fwFailure->maxRebootDetected) {
		} else {
		    strncpy(fwFailure->details, "EcmLogger: ECM crash detected", sizeof(fwFailure->details) - 1);
		    fwFailure->details[sizeof(fwFailure->details) - 1] = '\0';
		}
		strncpy(fwFailure->initiator, "EcmLogger", sizeof(fwFailure->initiator) - 1);
		fwFailure->initiator[sizeof(fwFailure->initiator) - 1] = '\0';
		RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","ECM crash detected\n");
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Firmware failure check complete: %s\n", fwFailure->detected ? fwFailure->details : "None detected");
    return SUCCESS;
}

static void map_hardware_reason(const char *hwReason, RebootInfo *info)
{
    if (!hwReason || !info) return;
    if (strstr(hwReason, "SOFTWARE_MASTER_RESET") ||
        strstr(hwReason, "SOFTWARE_RESET") ||
        strstr(hwReason, "SOFTWARE_REBOOT") ||
        strstr(hwReason, "SW_RESET") ||
        strstr(hwReason, "SOFTWARE")) {
        strcpy(info->source, "SoftwareReboot");
        strcpy(info->reason, "SOFTWARE_MASTER_RESET");
        strcpy(info->otherReason, "Reboot due to user triggered reboot command");
        t2CountNotify("Test_SWReset", 1);
    }
    else if (strstr(hwReason, "WATCHDOG")) {
        strcpy(info->source, "WatchDog");
        strcpy(info->reason, "WATCHDOG_TIMER_RESET");
        strcpy(info->otherReason, "Reboot due to watch dog timer reset");
    }
    else if (strstr(hwReason, "KERNEL_PANIC") || strstr(hwReason, "KERNEL_PANIC_RESET")) {
        strcpy(info->source, "Kernel");
        strcpy(info->reason, "KERNEL_PANIC");
        strcpy(info->otherReason, "Reboot due to Kernel Panic captured by Oops Dump");
    }
    else if (strstr(hwReason, "POWER_ON") || strstr(hwReason, "HARDWARE")) {
        strcpy(info->source, "PowerOn");
        strcpy(info->reason, "POWER_ON_RESET");
        strcpy(info->otherReason, "Reboot due to unplug of power cable from the STB");
    }
    else if (strstr(hwReason, "MAIN_CHIP_INPUT_RESET")) {
        strcpy(info->source, "Main Chip");
        strcpy(info->reason, "MAIN_CHIP_INPUT_RESET");
        strcpy(info->otherReason, "Reboot due to chip's main reset input has been asserted");
    }
    else if (strstr(hwReason, "MAIN_CHIP_RESET_INPUT")) {
        strcpy(info->source, "Main Chip");
        strcpy(info->reason, "MAIN_CHIP_RESET_INPUT");
        strcpy(info->otherReason, "Reboot due to chip's main reset input has been asserted");
    }
    else if (strstr(hwReason, "TAP_IN_SYSTEM_RESET")) {
        strcpy(info->source, "Tap-In System");
        strcpy(info->reason, "TAP_IN_SYSTEM_RESET");
        strcpy(info->otherReason, "Reboot due to the chip's TAP in-system reset has been asserted");
    }
    else if (strstr(hwReason, "FRONT_PANEL_4SEC_RESET") || strstr(hwReason, "FRONT_PANEL_RESET")) {
        strcpy(info->source, "FrontPanel Button");
        strcpy(info->reason, "FRONT_PANEL_RESET");
        strcpy(info->otherReason, "Reboot due to the front panel 4 second reset has been asserted");
    }
    else if (strstr(hwReason, "S3_WAKEUP_RESET")) {
        strcpy(info->source, "Standby Wakeup");
        strcpy(info->reason, "S3_WAKEUP_RESET");
        strcpy(info->otherReason, "Reboot due to the chip woke up from deep standby");
    }
    else if (strstr(hwReason, "SMARTCARD_INSERT_RESET")) {
        strcpy(info->source, "SmartCard Insert");
        strcpy(info->reason, "SMARTCARD_INSERT_RESET");
        strcpy(info->otherReason, "Reboot due to the smartcard insert reset has occurred");
    }
    else if (strstr(hwReason, "OVERTEMP") || strstr(hwReason, "OVERHEAT")) {
        strcpy(info->source, "OverTemperature");
        strcpy(info->reason, "OVERTEMP_RESET");
        strcpy(info->otherReason, "Reboot due to chip temperature is above threshold (125*C)");
    }
    else if (strstr(hwReason, "OVERVOLTAGE")) {
        strcpy(info->source, "OverVoltage");
        strcpy(info->reason, "OVERVOLTAGE_RESET");
        strcpy(info->otherReason, "Reboot due to chip voltage is above threshold");
    }
    else if (strstr(hwReason, "UNDERVOLTAGE") || strstr(hwReason, "UNDERVOLTAGE_0_RESET") || strstr(hwReason, "UNDERVOLTAGE_1_RESET")) {
        strcpy(info->source, "LowVoltage");
        strcpy(info->reason, "UNDERVOLTAGE_RESET");
        strcpy(info->otherReason, "Reboot due to chip voltage is below threshold");
    }
    else if (strstr(hwReason, "PCIE_0_HOT_BOOT_RESET") || strstr(hwReason, "PCIE_1_HOT_BOOT_RESET") || strstr(hwReason, "PCIE_HOT_BOOT_RESET")) {
        strcpy(info->source, "PCIE Boot");
        strcpy(info->reason, "PCIE_HOT_BOOT_RESET");
        strcpy(info->otherReason, "Reboot due to PCIe hot boot reset has occurred");
    }
    else if (strstr(hwReason, "SECURITY_MASTER_RESET")) {
        strcpy(info->source, "SecurityReboot");
        strcpy(info->reason, "SECURITY_MASTER_RESET");
        strcpy(info->otherReason, "Reboot due to security master reset has occurred");
    }
    else if (strstr(hwReason, "CPU_EJTAG_RESET")) {
        strcpy(info->source, "CPU EJTAG");
        strcpy(info->reason, "CPU_EJTAG_RESET");
        strcpy(info->otherReason, "Reboot due to CPU EJTAG reset has occurred");
    }
    else if (strstr(hwReason, "SCPU_EJTAG_RESET")) {
        strcpy(info->source, "SCPU EJTAG");
        strcpy(info->reason, "SCPU_EJTAG_RESET");
        strcpy(info->otherReason, "Reboot due to SCPU EJTAG reset has occurred");
    }
    else if (strstr(hwReason, "GEN_WATCHDOG_1_RESET") || strstr(hwReason, "GEN_WATCHDOG_RESET")) {
        strcpy(info->source, "GEN WatchDog");
        strcpy(info->reason, "GEN_WATCHDOG_RESET");
        strcpy(info->otherReason, "Reboot due to gen_watchdog_1 timeout reset has occurred");
    }
    else if (strstr(hwReason, "AUX_CHIP_EDGE_RESET_0") || strstr(hwReason, "AUX_CHIP_EDGE_RESET_1") || strstr(hwReason, "AUX_CHIP_EDGE_RESET")) {
        strcpy(info->source, "Aux Chip Edge");
        strcpy(info->reason, "AUX_CHIP_EDGE_RESET");
        strcpy(info->otherReason, "Reboot due to the auxiliary edge-triggered chip reset has occurred");
    }
    else if (strstr(hwReason, "AUX_CHIP_LEVEL_RESET_0") || strstr(hwReason, "AUX_CHIP_LEVEL_RESET_1") || strstr(hwReason, "AUX_CHIP_LEVEL_RESET")) {
        strcpy(info->source, "Aux Chip Level");
        strcpy(info->reason, "AUX_CHIP_LEVEL_RESET");
        strcpy(info->otherReason, "Reboot due to the auxiliary level-triggered chip reset has occurred");
    }
    else if (strstr(hwReason, "MPM_RESET")) {
        strcpy(info->source, "MPM");
        strcpy(info->reason, "MPM_RESET");
        strcpy(info->otherReason, "Reboot due to the MPM reset has occurred");
    }
    else {
        strcpy(info->source, "Unknown");
        size_t len = strlen(hwReason);
        if (len >= sizeof(info->reason)) len = sizeof(info->reason) - 1;
        memcpy(info->reason, hwReason, len);
        info->reason[len] = '\0';
        len = strlen(hwReason);
        if (len >= sizeof(info->otherReason)) len = sizeof(info->otherReason) - 1;
        memcpy(info->otherReason, hwReason, len);
        info->otherReason[len] = '\0';
    }
}

int classify_reboot_reason(RebootInfo *info, const EnvContext *ctx, const HardwareReason *hwReason, const PanicInfo *panicInfo, const FirmwareFailure *fwFailure)
{
    if (!info || !ctx) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Invalid parameters for classify_reboot_reason\n");
        return ERROR_GENERAL;
    }

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Classifying reboot reason\n");
    if (fwFailure && fwFailure->detected) {
        if (fwFailure->initiator[0] != '\0') {
            strncpy(info->source, fwFailure->initiator, sizeof(info->source) - 1);
            info->source[sizeof(info->source) - 1] = '\0';
        } else {
            strcpy(info->source, "FirmwareFailure");
        }
        strcpy(info->reason, "FIRMWARE_FAILURE");
        if (fwFailure->details[0] != '\0') {
            strncpy(info->otherReason, fwFailure->details, sizeof(info->otherReason) - 1);
            info->otherReason[sizeof(info->otherReason) - 1] = '\0';
        }
        info->customReason[0] = '\0';
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Firmware failure classified: %s\n", info->otherReason);
        return SUCCESS;
    }
    if (info->customReason[0] != '\0') {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Classifying existing custom reason: %s", info->customReason);
        if (strcmp(info->customReason, "MAINTENANCE_REBOOT") == 0) {
            strcpy(info->reason, "MAINTENANCE_REBOOT");
            return SUCCESS;
        }
        if (is_app_triggered(info->customReason)) {
            strcpy(info->reason, "APP_TRIGGERED");
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Classified as APP_TRIGGERED: %s\n", info->customReason);
            return SUCCESS;
        }
        if (is_ops_triggered(info->customReason)) {
            strcpy(info->reason, "OPS_TRIGGERED");
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Classified as OPS_TRIGGERED: %s\n", info->customReason);
            return SUCCESS;
        }
        if (is_maintenance_triggered(info->customReason)) {
            strcpy(info->reason, "MAINTENANCE_REBOOT");
            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Classified as MAINTENANCE_REBOOT: %s\n", info->customReason);
            return SUCCESS;
        }
        // Default to firmware failure when initiator does not match any list
        strcpy(info->reason, "FIRMWARE_FAILURE");
        if (info->source[0] == '\0') {
            strcpy(info->source, "Unknown");
        }
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Defaulted to FIRMWARE_FAILURE for customReason: %s\n", info->customReason);
        return SUCCESS;
    }
    // Apply kernel panic classification only when no prior customReason exists
    if (panicInfo && panicInfo->detected && info->customReason[0] == '\0') {
        strcpy(info->source, "Kernel");
        strcpy(info->reason, "KERNEL_PANIC");
        // Align with script: prefix KERNEL_PANIC instead of detailed panic type
        strncpy(info->customReason, "Hardware Register - KERNEL_PANIC", sizeof(info->customReason) - 1);
        info->customReason[sizeof(info->customReason) - 1] = '\0';
        strcpy(info->otherReason, "Reboot due to Kernel Panic captured by Oops Dump");
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Kernel panic classified (prefixed): %s\n", panicInfo->panicType);
        return SUCCESS;
    }
    if (hwReason && hwReason->mappedReason[0] != '\0') {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO", "hwreason is not NULL : %s\n", hwReason->mappedReason);
        map_hardware_reason(hwReason->mappedReason, info);
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO", "After map function call : hwreason is not NULL : %s\n", hwReason->mappedReason);
        const char prefix[] = "Hardware Register - ";
        if (ctx && (strcmp(ctx->soc, "BRCM") == 0 || strcmp(ctx->soc, "BROADCOM") == 0)) {
            if (hwReason->rawReason[0] != '\0') {
                char upbuf[MAX_BUFFER_SIZE];
                size_t n = strlen(hwReason->rawReason);
                if (n >= sizeof(upbuf)) n = sizeof(upbuf) - 1;
                memcpy(upbuf, hwReason->rawReason, n);
                upbuf[n] = '\0';
                for (size_t i = 0; i < n; i++) {
                    upbuf[i] = toupper((unsigned char)upbuf[i]);
                }
                size_t prefix_len = sizeof(prefix) - 1;
                size_t max_reason = sizeof(info->customReason) - 1 - prefix_len;
                if ((int)max_reason < 0) max_reason = 0;
                snprintf(info->customReason, sizeof(info->customReason), "%s%.*s", prefix, (int)max_reason, upbuf);
            } else if (strcmp(hwReason->mappedReason, "UNKNOWN") == 0) {
                strncpy(info->customReason, "Hardware Register - NULL", sizeof(info->customReason) - 1);
                info->customReason[sizeof(info->customReason) - 1] = '\0';
                strcpy(info->source, "Hard Power Reset");
                strcpy(info->reason, "HARD_POWER");
                strcpy(info->otherReason, "No information found");
            } else {
                size_t prefix_len = sizeof(prefix) - 1;
                size_t max_reason = sizeof(info->customReason) - 1 - prefix_len;
                if ((int)max_reason < 0) max_reason = 0;
                snprintf(info->customReason, sizeof(info->customReason), "%s%.*s", prefix, (int)max_reason, hwReason->mappedReason);
            }
        } else {
            if (strcmp(hwReason->mappedReason, "UNKNOWN") == 0 || hwReason->mappedReason[0] == '\0') {
                strncpy(info->customReason, "Hardware Register - NULL", sizeof(info->customReason) - 1);
                info->customReason[sizeof(info->customReason) - 1] = '\0';
                strcpy(info->source, "Hard Power Reset");
                strcpy(info->reason, "HARD_POWER");
                strcpy(info->otherReason, "No information found");
            } else {
                const char* standardized = (info->reason[0] != '\0') ? info->reason : hwReason->mappedReason;
                size_t prefix_len = sizeof(prefix) - 1;
                size_t max_reason = sizeof(info->customReason) - 1 - prefix_len;
                if ((int)max_reason < 0) max_reason = 0;
                snprintf(info->customReason, sizeof(info->customReason), "%s%.*s", prefix, (int)max_reason, standardized);
            }
        }
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Classified hardware reason - Source: %s, Reason: %s\n", info->source, info->reason);
        return SUCCESS;
    }
    if (info->source[0] == '\0') {
        strcpy(info->source, "Unknown");
        strcpy(info->reason, "UNKNOWN");
        strcpy(info->customReason, "UNKNOWN");
        strcpy(info->otherReason, "Reboot reason could not be determined");
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot reason classified as UNKNOWN\n");
        if ((!panicInfo || !panicInfo->detected) && (!fwFailure || !fwFailure->detected)) {
            strcpy(info->source, "SoftwareReboot");
            strcpy(info->reason, "SOFTWARE_MASTER_RESET");
            strcpy(info->customReason, "SOFTWARE_MASTER_RESET");
            strcpy(info->otherReason, "Reboot due to user triggered reboot command");
            t2CountNotify("Test_SWReset", 1);
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Fallback classification: SoftwareReboot\n");
        } else {
            strcpy(info->source, "Unknown");
            strcpy(info->reason, "UNKNOWN");
            strcpy(info->customReason, "UNKNOWN");
            strcpy(info->otherReason, "Reboot reason could not be determined");
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot reason classified as UNKNOWN\n");
        }
    }
    if (ctx && (strcmp(ctx->soc, "RTK") == 0 || strcmp(ctx->soc, "REALTEK") == 0)) {
        if (info->reason[0] != '\0') {
            char lower[MAX_REASON_LENGTH];
            size_t len = strlen(info->reason);
            for (size_t i = 0; i < len && i < sizeof(lower) - 1; i++) {
                lower[i] = tolower((unsigned char)info->reason[i]);
            }
            lower[(len < sizeof(lower) - 1) ? len : (sizeof(lower) - 1)] = '\0';
            FILE *klog = fopen("/opt/logs/messages.txt", "a");
            if (klog) {
                fprintf(klog, "PreviousRebootReason: %s\n", lower);
                fflush(klog);
                fclose(klog);
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Annotated kernel log with PreviousRebootReason: %s\n", lower);
            }
        }
    }
    return SUCCESS;
}
