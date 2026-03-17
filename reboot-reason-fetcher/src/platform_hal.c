#include "update-reboot-info.h"
#include <ctype.h>
#include "rdk_logger.h"

int get_hardware_reason(const EnvContext *ctx, HardwareReason *hwReason, RebootInfo *info)
{
    memset(hwReason, 0, sizeof(HardwareReason));
    if (ctx->soc[0] != '\0') {
        if (strcmp(ctx->soc, "BRCM") == 0 || strcmp(ctx->soc, "BROADCOM") == 0) {
            return read_brcm_previous_reboot_reason(hwReason);
        } else if (strcmp(ctx->soc, "RTK") == 0 || strcmp(ctx->soc, "REALTEK") == 0) {
            return read_rtk_wakeup_reason(hwReason);
        } else if (strcmp(ctx->soc, "AMLOGIC") == 0) {
            return read_amlogic_reset_reason(hwReason, info);
        } else if (strcmp(ctx->soc, "MEDIATEK") == 0 || strcmp(ctx->soc, "MTK") == 0) {
            return read_mtk_reset_reason(hwReason, info);
        }
    }
    // Fallback attempts independent of SOC (covers cases where env vars failed)
    if (hwReason->mappedReason[0] == '\0') {
        if (access("/proc/brcm/previous_reboot_reason", R_OK) == 0) {
            if (read_brcm_previous_reboot_reason(hwReason) == SUCCESS) return SUCCESS;
        }
        if (access("/proc/cmdline", R_OK) == 0) {
            if (read_rtk_wakeup_reason(hwReason) == SUCCESS) return SUCCESS;
        }
        if (access("/sys/devices/platform/aml_pm/reset_reason", R_OK) == 0) {
            if (read_amlogic_reset_reason(hwReason, info) == SUCCESS) return SUCCESS;
        }
        if (access("/sys/mtk_pm/boot_reason", R_OK) == 0) {
            if (read_mtk_reset_reason(hwReason, info) == SUCCESS) return SUCCESS;
        }
    }
    if (hwReason->mappedReason[0] == '\0') {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Hardware reason not determined (SOC='%s')", ctx->soc);
        strcpy(hwReason->mappedReason, "UNKNOWN");
    }
    return SUCCESS;
}

int get_hardware_reason_brcm(const EnvContext *ctx, HardwareReason *hwReason)
{
    FILE *fp = NULL;
    char buffer[MAX_BUFFER_SIZE];
    if (!ctx || !hwReason) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for get_hardware_reason_brcm\n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Getting Broadcom hardware reboot reason from %s\n", BRCM_REBOOT_FILE);
    memset(hwReason, 0, sizeof(HardwareReason));
    if (access(BRCM_REBOOT_FILE, F_OK) != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Broadcom reboot reason file not found: %s\n", BRCM_REBOOT_FILE);
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
        return SUCCESS;
    }
    fp = fopen(BRCM_REBOOT_FILE, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", BRCM_REBOOT_FILE, strerror(errno));
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
        return ERROR_GENERAL;
    }
    if (fgets(buffer, sizeof(buffer), fp)) {
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        strncpy(hwReason->rawReason, buffer, sizeof(hwReason->rawReason) - 1);
        hwReason->rawReason[sizeof(hwReason->rawReason) - 1] = '\0';
        for (char *p = hwReason->rawReason; *p; p++) {
            *p = toupper(*p);
        }
        strncpy(hwReason->mappedReason, hwReason->rawReason, sizeof(hwReason->mappedReason) - 1);
        hwReason->mappedReason[sizeof(hwReason->mappedReason) - 1] = '\0';
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Broadcom hardware reason: %s\n", hwReason->rawReason);
    } else {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Empty reboot reason file\n");
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
    }
    fclose(fp);
    return SUCCESS;
}

int get_hardware_reason_rtk(const EnvContext *ctx, HardwareReason *hwReason)
{
    FILE *fp = NULL;
    char buffer[MAX_BUFFER_SIZE];
    char *wakeup_ptr = NULL;
    if (!ctx || !hwReason) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for get_hardware_reason_rtk\n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Getting Realtek hardware reboot reason from %s\n", CMDLINE_PATH);
    memset(hwReason, 0, sizeof(HardwareReason));
    fp = fopen(CMDLINE_PATH, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", CMDLINE_PATH, strerror(errno));
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
        return ERROR_GENERAL;
    }
    if (fgets(buffer, sizeof(buffer), fp)) {
        wakeup_ptr = strstr(buffer, WAKEUP_REASON_KEY);
        if (wakeup_ptr) {
            char *value_start = wakeup_ptr + strlen(WAKEUP_REASON_KEY);
            char *value_end = value_start;
            while (*value_end && *value_end != ' ' && *value_end != '\n' && *value_end != '\t') {
                value_end++;
            }
            size_t len = value_end - value_start;
            if (len > 0 && len < sizeof(hwReason->rawReason)) {
                strncpy(hwReason->rawReason, value_start, len);
                hwReason->rawReason[len] = '\0';
                for (char *p = hwReason->rawReason; *p; p++) {
                    *p = toupper(*p);
                }
                strncpy(hwReason->mappedReason, hwReason->rawReason, sizeof(hwReason->mappedReason) - 1);
                hwReason->mappedReason[sizeof(hwReason->mappedReason) - 1] = '\0';
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Realtek wakeup reason: %s\n", hwReason->rawReason);
            } else {
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Invalid wakeupreason value\n");
                strcpy(hwReason->rawReason, "UNKNOWN");
                strcpy(hwReason->mappedReason, "UNKNOWN");
            }
        } else {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","No wakeupreason found in cmdline\n");
            strcpy(hwReason->rawReason, "UNKNOWN");
            strcpy(hwReason->mappedReason, "UNKNOWN");
        }
    } else {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to read from %s\n", CMDLINE_PATH);
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
    }
    fclose(fp);
    return SUCCESS;
}

static const char* map_amlogic_reason(int code)
{
    switch (code) {
        case 0: return "POWER_ON_RESET";
        case 1: return "WATCHDOG_RESET";
        case 2: return "SOFTWARE_RESET";
        case 3: return "KERNEL_PANIC";
        case 4: return "THERMAL_RESET";
        case 5: return "HARDWARE_RESET";
        case 6: return "SUSPEND_WAKEUP";
        case 7: return "REMOTE_WAKEUP";
        case 8: return "RTC_WAKEUP";
        case 9: return "GPIO_WAKEUP";
        case 10: return "FACTORY_RESET";
        case 11: return "UPGRADE_RESET";
        case 12: return "FASTBOOT_RESET";
        case 13: return "CRASH_DUMP_RESET";
        case 14: return "RECOVERY_RESET";
        case 15: return "BOOTLOADER_RESET";
        default: return "UNKNOWN";
    }
}

static const char* map_mtk_reason(int resetVal)
{
    switch (resetVal) {
        case 0x00:
            return "POWER_ON_RESET";
        case 0xD1:
            return "SOFTWARE_MASTER_RESET";
        case 0xE4:
            return "THERMAL_REBOOT";
        case 0xEE:
            return "KERNEL_PANIC";
        case 0xE0:
            return "WATCHDOG_REBOOT";
        default:
            return "UNKNOWN_RESET";
    }
}

int get_hardware_reason_amlogic(const EnvContext *ctx, HardwareReason *hwReason)
{
    FILE *fp = NULL;
    char buffer[MAX_BUFFER_SIZE];
    int reset_code = -1;
    if (!ctx || !hwReason) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for get_hardware_reason_amlogic\n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Getting Amlogic hardware reboot reason from %s\n", AMLOGIC_REBOOT_REASON_PATH);
    memset(hwReason, 0, sizeof(HardwareReason));
    if (access(AMLOGIC_REBOOT_REASON_PATH, F_OK) != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Amlogic reboot reason file not found: %s\n", AMLOGIC_REBOOT_REASON_PATH);
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
        return SUCCESS;
    }
    fp = fopen(AMLOGIC_REBOOT_REASON_PATH, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", AMLOGIC_REBOOT_REASON_PATH, strerror(errno));
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
        return ERROR_GENERAL;
    }
    if (fgets(buffer, sizeof(buffer), fp)) {
        reset_code = atoi(buffer);
        snprintf(hwReason->rawReason, sizeof(hwReason->rawReason), "%d", reset_code);
        const char *mapped = map_amlogic_reason(reset_code);
        strncpy(hwReason->mappedReason, mapped, sizeof(hwReason->mappedReason) - 1);
        hwReason->mappedReason[sizeof(hwReason->mappedReason) - 1] = '\0';
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Amlogic reset code: %d mapped to: %s\n", reset_code, hwReason->mappedReason);
    } else {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Failed to read Amlogic reset code\n");
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
    }
    fclose(fp);
    return SUCCESS;
}

int get_hardware_reason_mtk(const EnvContext *ctx, HardwareReason *hwReason)
{
    FILE *fp = NULL;
    char buffer[MAX_BUFFER_SIZE];
    int reset_code = -1;
    if (!ctx || !hwReason) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for get_hardware_reason_mtk\n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Getting MTK hardware reboot reason from %s\n", MTK_SYSFS_FILE);
    memset(hwReason, 0, sizeof(HardwareReason));
    if (access(MTK_SYSFS_FILE, F_OK) != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","MTK reboot reason file not found: %s\n", MTK_SYSFS_FILE);
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
        return SUCCESS;
    }
    fp = fopen(MTK_SYSFS_FILE, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open %s: %s\n", MTK_SYSFS_FILE, strerror(errno));
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
        return ERROR_GENERAL;
    }
    if (fgets(buffer, sizeof(buffer), fp)) {
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        // Parse hex value
        if (strncmp(buffer, "0x", 2) == 0 || strncmp(buffer, "0X", 2) == 0) {
            reset_code = (int)strtol(buffer, NULL, 16);
        } else {
            reset_code = (int)strtol(buffer, NULL, 10);
        }
        
        strncpy(hwReason->rawReason, buffer, sizeof(hwReason->rawReason) - 1);
        hwReason->rawReason[sizeof(hwReason->rawReason) - 1] = '\0';
        
        const char *mapped = map_mtk_reason(reset_code);
        strncpy(hwReason->mappedReason, mapped, sizeof(hwReason->mappedReason) - 1);
        hwReason->mappedReason[sizeof(hwReason->mappedReason) - 1] = '\0';
        
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","MTK reset code: %s (0x%X) mapped to: %s\n", 
                hwReason->rawReason, reset_code, hwReason->mappedReason);
    } else {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Failed to read MTK reset code\n");
        strcpy(hwReason->rawReason, "UNKNOWN");
        strcpy(hwReason->mappedReason, "UNKNOWN");
    }
    fclose(fp);
    return SUCCESS;
}
