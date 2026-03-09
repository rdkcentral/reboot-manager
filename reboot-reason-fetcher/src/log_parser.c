#include "update-reboot-info.h"
#include "rdk_fwdl_utils.h"
#include "rdk_logger.h"
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

int parse_device_properties(EnvContext *ctx)
{
    char buffer[MAX_REASON_LENGTH];
    if (!ctx) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Context pointer is NULL\n");
        return FAILURE;
    }
    memset(ctx, 0, sizeof(EnvContext));
    ctx->platcoSupport = false;
    ctx->llamaSupport = false;
    ctx->rebootInfoSttSupport = false;
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Parsing device properties using common_utilities\n");
    if (getDevicePropertyData("SOC", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        size_t len = strlen(buffer);
        if (len >= sizeof(ctx->soc)) {
            len = sizeof(ctx->soc) - 1;
        }
        memcpy(ctx->soc, buffer, len);
        ctx->soc[len] = '\0';
    }
    if (getDevicePropertyData("BUILD_TYPE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        size_t len = strlen(buffer);
        if (len >= sizeof(ctx->buildType)) {
            len = sizeof(ctx->buildType) - 1;
        }
        memcpy(ctx->buildType, buffer, len);
        ctx->buildType[len] = '\0';
    }
    if (getDevicePropertyData("DEVICE_TYPE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        size_t len = strlen(buffer);
        if (len >= sizeof(ctx->device_type)) {
            len = sizeof(ctx->device_type) - 1;
        }
        memcpy(ctx->device_type, buffer, len);
        ctx->device_type[len] = '\0';
    }
    if (getDevicePropertyData("PLATCO_SUPPORT", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        ctx->platcoSupport = (strcasecmp(buffer, "true") == 0);
    }
    if (getDevicePropertyData("LLAMA_SUPPORT", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        ctx->llamaSupport = (strcasecmp(buffer, "true") == 0);
    }
    if (getDevicePropertyData("REBOOT_INFO_STT_SUPPORT", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        ctx->rebootInfoSttSupport = (strcasecmp(buffer, "true") == 0);
    }
     if (ctx->soc[0] == '\0' || ctx->device_type[0] == '\0') {
        FILE *dp = fopen("/etc/device.properties", "r");
        if (dp) {
            char line[256];
            while (fgets(line, sizeof(line), dp)) {
                char *eq = strchr(line, '=');
                if (!eq) continue;
                *eq = '\0';
                char *key = line;
                char *val = eq + 1;
                char *nl = strchr(val, '\n');
                if (nl) *nl = '\0';
                if (ctx->soc[0] == '\0' && strcmp(key, "SOC") == 0) {
                    size_t len = strlen(val); 
                    if (len >= sizeof(ctx->soc)) {
                        len = sizeof(ctx->soc) - 1; 
                    }
                    memcpy(ctx->soc, val, len); 
                    ctx->soc[len] = '\0';
                } else if (ctx->device_type[0] == '\0' && (strcmp(key, "DEVICE_TYPE") == 0 || strcmp(key, "DEVICE_NAME") == 0)) {
                    size_t len = strlen(val); 
                    if (len >= sizeof(ctx->device_type)) {
                        len = sizeof(ctx->device_type) - 1;
                    }
                    memcpy(ctx->device_type, val, len);
                    ctx->device_type[len] = '\0';
                }
            }
            fclose(dp);
        } else {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Could not open /etc/device.properties\n");
            return FAILURE;
        }
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Device properties parsed - SOC: %s, BuildType: %s, DeviceType: %s\n", ctx->soc, ctx->buildType, ctx->device_type);
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Support flags - PLATCO: %d, LLAMA: %d, STT: %d\n", ctx->platcoSupport, ctx->llamaSupport, ctx->rebootInfoSttSupport);
    return SUCCESS;
}
void free_env_context(EnvContext *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(EnvContext));
    }
}

int update_reboot_info(const EnvContext *ctx)
{
    const char *REBOOT_INFO_FLAG = "/tmp/rebootInfo_Updated";
    
    if (!ctx) return 0;
    if (ctx->platcoSupport || ctx->llamaSupport) {
        if (access(UPDATE_REBOOT_INFO_INVOKED_FLAG, F_OK) == 0) {
            if (access(STT_FLAG, F_OK) != 0 || access(REBOOT_INFO_FLAG, F_OK) != 0) {
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","STT or RebootInfo flag missing; skip update\n");
                return 0;
            }
        }
        return 1;
    }
    if (access(STT_FLAG, F_OK) != 0 || access(REBOOT_INFO_FLAG, F_OK) != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","STT or RebootInfo flag missing; skip update\n");
        return 0;
    }
    return 1;
}

static void getVal(const char *line, const char *prefix, char *output, size_t output_size)
{
    const char *value = line + strlen(prefix);
    while (*value && (*value == ' ' || *value == '\t')) {
        value++;
    }
    strncpy(output, value, output_size - 1);
    output[output_size - 1] = '\0';
    char *end = output + strlen(output) - 1;
    while (end >= output && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
}
int parse_legacy_log(const char *logPath, RebootInfo *info)
{
    FILE *fp = NULL;
    char line[MAX_BUFFER_SIZE];
    int found_fields = 0;
    if (!logPath || !info) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for parse_legacy_log\n");
        return FAILURE;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Parsing legacy log: %s\n", logPath);
    fp = fopen(logPath, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open legacy log %s: %s\n", logPath, strerror(errno));
        return ERROR_FILE_NOT_FOUND;
    }
     while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "PreviousRebootInitiatedBy:")) {
            getVal(strstr(line, "PreviousRebootInitiatedBy:"), "PreviousRebootInitiatedBy:", info->source, sizeof(info->source));
            found_fields++;
        }
        else if (strstr(line, "PreviousRebootTime:")) {
            getVal(strstr(line, "PreviousRebootTime:"), "PreviousRebootTime:", info->timestamp, sizeof(info->timestamp));
            found_fields++;
        }
        else if (strstr(line, "PreviousCustomReason:")) {
            getVal(strstr(line, "PreviousCustomReason:"), "PreviousCustomReason:", info->customReason, sizeof(info->customReason));
            found_fields++;
        }
        else if (strstr(line, "PreviousOtherReason:")) {
            getVal(strstr(line, "PreviousOtherReason:"), "PreviousOtherReason:", info->otherReason, sizeof(info->otherReason));
            found_fields++;
        }
        if (found_fields >= 4) {
            break;
        }
    }
    fclose(fp);
    if (found_fields == 0) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","No reboot info fields found in legacy log\n");
        return FAILURE;
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Parsed legacy log - Found %d fields\n", found_fields);
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Timestamp: %s, Source: %s, Reason: %s\n", info->timestamp, info->source, info->reason);
    return SUCCESS;
}
static int read_file_data(const char *path, char *buf, size_t buflen)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open file\n");
        return ERROR_GENERAL;
    }
    size_t n = fread(buf, 1, buflen - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return SUCCESS;
}
int read_brcm_previous_reboot_reason(HardwareReason *hw)
{
    char buf[MAX_BUFFER_SIZE];
    char upbuf[MAX_BUFFER_SIZE];
    if (!hw) return ERROR_GENERAL;
    if (access(BRCM_REBOOT_FILE, R_OK) != 0) {
        return FAILURE;
    }
    if (read_file_data(BRCM_REBOOT_FILE, buf, sizeof(buf)) != SUCCESS || buf[0] == '\0') {
        return FAILURE;
    }
    size_t raw_len = strcspn(buf, "\n");
    if (raw_len >= sizeof(hw->rawReason)) {
        raw_len = sizeof(hw->rawReason) - 1;
    }
    memcpy(hw->rawReason, buf, raw_len);
    hw->rawReason[raw_len] = '\0';

    size_t buflen = strlen(hw->rawReason);
    if (buflen >= sizeof(upbuf)) buflen = sizeof(upbuf) - 1;
    memcpy(upbuf, hw->rawReason, buflen);
    upbuf[buflen] = '\0';
    for (char *p = upbuf; *p; ++p) {
        *p = toupper((unsigned char)*p);
    }

    const char *primary = upbuf;
    size_t primary_len = strcspn(upbuf, ",\n");
    if (primary_len >= sizeof(hw->mappedReason)) {
        primary_len = sizeof(hw->mappedReason) - 1;
    }
    memcpy(hw->mappedReason, primary, primary_len);
    hw->mappedReason[primary_len] = '\0';
    return SUCCESS;
}
int read_rtk_wakeup_reason(HardwareReason *hw)
{
    char buf[MAX_BUFFER_SIZE];
    if (!hw) return ERROR_GENERAL;
    if (access(RTK_REBOOT_FILE, R_OK) != 0) {
        return FAILURE;
    }
    if (read_file_data(RTK_REBOOT_FILE, buf, sizeof(buf)) != SUCCESS) {
        return FAILURE;
    }
    const char *key = "wakeupreason=";
    char *pos = strstr(buf, key);
    if (!pos) {
        return FAILURE;
    }
    pos += strlen(key);
    char reason[MAX_REASON_LENGTH];
    size_t i = 0;
    while (pos[i] && !isspace((unsigned char)pos[i]) && i < sizeof(reason) - 1) {
        reason[i] = pos[i];
        i++;
    }
    reason[i] = '\0';
    for (size_t j = 0; j < i; j++) {
        reason[j] = toupper((unsigned char)reason[j]);
    }
    strncpy(hw->mappedReason, reason, sizeof(hw->mappedReason) - 1);
    hw->mappedReason[sizeof(hw->mappedReason) - 1] = '\0';
    return SUCCESS;
}
int read_amlogic_reset_reason(HardwareReason *hw, RebootInfo *info)
{
    char buf[64];
    int resetVal = -1;
    char saved_timestamp[MAX_TIMESTAMP_LENGTH];
    
    if (!hw || !info) {
        return FAILURE;
    }

    strncpy(saved_timestamp, info->timestamp, sizeof(saved_timestamp) - 1);
    saved_timestamp[sizeof(saved_timestamp) - 1] = '\0';

    if (access(AMLOGIC_SYSFS_FILE, R_OK) != 0) {
        return FAILURE;
    }
    if (read_file_data(AMLOGIC_SYSFS_FILE, buf, sizeof(buf)) != SUCCESS) {
        return FAILURE;
    }
    resetVal = atoi(buf);
    switch (resetVal) {
        case 0:
            strcpy(info->source, "POWER_ON_REBOOT");
            strcpy(info->reason, "POWER_ON_RESET");
            strcpy(info->customReason, "Hardware Register - COLD_BOOT");
            strcpy(info->otherReason, "Reboot due to hardware power cable unplug");
            strcpy(hw->mappedReason, "POWER_ON_RESET");
            break;
        case 1:
            strcpy(info->source, "SOFTWARE_REBOOT");
            strcpy(info->reason, "SOFTWARE_MASTER_RESET");
            strcpy(info->customReason, "Hardware Register - NORMAL_BOOT");
            strcpy(info->otherReason, "Reboot due to user triggered reboot command");
            strcpy(hw->mappedReason, "SOFTWARE_MASTER_RESET");
            break;
        case 2:
            strcpy(info->source, "FACTORY_RESET_REBOOT");
            strcpy(info->reason, "FACTORY_RESET");
            strcpy(info->customReason, "Hardware Register - FACTORY_RESET");
            strcpy(info->otherReason, "Reboot due to factory reset reboot");
            strcpy(hw->mappedReason, "FACTORY_RESET");
            break;
        case 3:
            strcpy(info->source, "UPGRADE_SYSTEM_REBOOT");
            strcpy(info->reason, "UPDATE_BOOT");
            strcpy(info->customReason, "Hardware Register - UPDATE_BOOT");
            strcpy(info->otherReason, "Reboot due to system upgrade reboot");
            strcpy(hw->mappedReason, "UPDATE_BOOT");
            break;
        case 4:
            strcpy(info->source, "FASTBOOT_REBOOT");
            strcpy(info->reason, "FAST_BOOT");
            strcpy(info->customReason, "Hardware Register - FAST_BOOT");
            strcpy(info->otherReason, "Reboot due to fast reboot");
            strcpy(hw->mappedReason, "FAST_BOOT");
            break;
        case 5:
            strcpy(info->source, "SUSPEND_REBOOT");
            strcpy(info->reason, "SUSPEND_BOOT");
            strcpy(info->customReason, "Hardware Register - SUSPEND_BOOT");
            strcpy(info->otherReason, "Reboot due to suspend reboot");
            strcpy(hw->mappedReason, "SUSPEND_BOOT");
            break;
        case 6:
            strcpy(info->source, "HIBERNATE_REBOOT");
            strcpy(info->reason, "HIBERNATE_BOOT");
            strcpy(info->customReason, "Hardware Register - HIBERNATE_BOOT");
            strcpy(info->otherReason, "Reboot due to hibernate reboot");
            strcpy(hw->mappedReason, "HIBERNATE_BOOT");
            break;
        case 7:
            strcpy(info->source, "BOOTLOADER_REBOOT");
            strcpy(info->reason, "FASTBOOT_BOOTLOADER");
            strcpy(info->customReason, "Hardware Register - FASTBOOT_BOOTLOADER");
            strcpy(info->otherReason, "Reboot due to fastboot bootloader reboot");
            strcpy(hw->mappedReason, "FASTBOOT_BOOTLOADER");
            break;
        case 8:
            strcpy(info->source, "SHUTDOWN_REBOOT");
            strcpy(info->reason, "SHUTDOWN_REBOOT");
            strcpy(info->customReason, "Hardware Register - SHUTDOWN_REBOOT");
            strcpy(info->otherReason, "Reboot due to shutdown");
            strcpy(hw->mappedReason, "SHUTDOWN_REBOOT");
            break;
        case 9:
            strcpy(info->source, "RPMPB");
            strcpy(info->reason, "RPMPB_REBOOT");
            strcpy(info->customReason, "Hardware Register - RPMPB_REBOOT");
            strcpy(info->otherReason, "Reboot due to RPMPB");
            strcpy(hw->mappedReason, "RPMPB_REBOOT");
            break;
        case 10:
            strcpy(info->source, "THERMAL");
            strcpy(info->reason, "THERMAL_REBOOT");
            strcpy(info->customReason, "Hardware Register - THERMAL_REBOOT");
            strcpy(info->otherReason, "Reboot due to thermal value");
            strcpy(hw->mappedReason, "THERMAL_REBOOT");
            break;
        case 11:
            strcpy(info->source, "CRASH_DUMP");
            strcpy(info->reason, "CRASH_REBOOT");
            strcpy(info->customReason, "Hardware Register - CRASH_REBOOT");
            strcpy(info->otherReason, "Reboot due to crash dump");
            strcpy(hw->mappedReason, "CRASH_REBOOT");
            break;
        case 12:
            strcpy(info->source, "KernelPanic");
            strcpy(info->reason, "KERNEL_PANIC");
            strcpy(info->customReason, "Hardware Register - KERNEL_PANIC");
            strcpy(info->otherReason, "Reboot due to oops dump caused panic");
            strcpy(hw->mappedReason, "KERNEL_PANIC");
            break;
        case 13:
            strcpy(info->source, "WATCH_DOG");
            strcpy(info->reason, "WATCHDOG_REBOOT");
            strcpy(info->customReason, "Hardware Register - WATCHDOG_REBOOT");
            strcpy(info->otherReason, "Reboot due to watch dog timer");
            strcpy(hw->mappedReason, "WATCHDOG_REBOOT");
            break;
        case 14:
            strcpy(info->source, "STR_AUTH_FAIL");
            strcpy(info->reason, "AMLOGIC_DDR_SHA2_REBOOT");
            strcpy(info->customReason, "Hardware Register - AMLOGIC_DDR_SHA2_REBOOT");
            strcpy(info->otherReason, "Reboot due to STR Authorization failure");
            strcpy(hw->mappedReason, "AMLOGIC_DDR_SHA2_REBOOT");
            break;
        case 15:
            strcpy(info->source, "FFV");
            strcpy(info->reason, "FFV_REBOOT");
            strcpy(info->customReason, "Hardware Register - FFV_REBOOT");
            strcpy(info->otherReason, "Reboot due to Reserved FFV");
            strcpy(hw->mappedReason, "FFV_REBOOT");
            break;
        default:
            strcpy(info->source, "HARD_POWER_RESET");
            strcpy(info->reason, "UNKNOWN_RESET");
            strcpy(info->customReason, "UNKNOWN");
            strcpy(info->otherReason, "Reboot due to unknown reason");
            strcpy(hw->mappedReason, "UNKNOWN_RESET");
            break;
    }

    if (saved_timestamp[0] != '\0') {
        strncpy(info->timestamp, saved_timestamp, sizeof(info->timestamp) - 1);
        info->timestamp[sizeof(info->timestamp) - 1] = '\0';
    }
    return SUCCESS;
}

int read_mtk_reset_reason(HardwareReason *hw, RebootInfo *info)
{
    char buf[64];
    int resetVal = -1;
    char saved_timestamp[MAX_TIMESTAMP_LENGTH];
    
    if (!hw || !info) {
        return ERROR_GENERAL;
    }

    strncpy(saved_timestamp, info->timestamp, sizeof(saved_timestamp) - 1);
    saved_timestamp[sizeof(saved_timestamp) - 1] = '\0';

    if (access(MTK_SYSFS_FILE, R_OK) != 0) {
        return FAILURE;
    }
    if (read_file_data(MTK_SYSFS_FILE, buf, sizeof(buf)) != SUCCESS) {
        return FAILURE;
    }
    
    // Parse hex value from MTK sysfs file
    if (strncmp(buf, "0x", 2) == 0 || strncmp(buf, "0X", 2) == 0) {
        resetVal = (int)strtol(buf, NULL, 16);
    } else {
        resetVal = (int)strtol(buf, NULL, 10);
    }
    
    // Store raw reason as string representation
    strncpy(hw->rawReason, buf, sizeof(hw->rawReason) - 1);
    hw->rawReason[sizeof(hw->rawReason) - 1] = '\0';
    
    // Remove newline if present
    char *newline = strchr(hw->rawReason, '\n');
    if (newline) *newline = '\0';
    
    switch (resetVal) {
        case 0x00:
            strcpy(info->source, "POWER_ON_REBOOT");
            strcpy(info->reason, "POWER_ON_RESET");
            strcpy(info->customReason, "COLD_BOOT");
            strcpy(info->otherReason, "Reboot due to hardware power cable unplug");
            strcpy(hw->mappedReason, "POWER_ON_RESET");
            break;
        case 0xD1:
            strcpy(info->source, "SOFTWARE_REBOOT");
            strcpy(info->reason, "SOFTWARE_MASTER_RESET");
            strcpy(info->customReason, "NORMAL_BOOT");
            strcpy(info->otherReason, "Reboot due to user triggered reboot command");
            strcpy(hw->mappedReason, "SOFTWARE_MASTER_RESET");
            break;
        case 0xE4:
            strcpy(info->source, "THERMAL");
            strcpy(info->reason, "THERMAL_REBOOT");
            strcpy(info->customReason, "THERMAL_REBOOT");
            strcpy(info->otherReason, "Reboot due to thermal value");
            strcpy(hw->mappedReason, "THERMAL_REBOOT");
            break;
        case 0xEE:
            strcpy(info->source, "KernelPanic");
            strcpy(info->reason, "KERNEL_PANIC");
            strcpy(info->customReason, "KERNEL_PANIC");
            strcpy(info->otherReason, "Reboot due to oops dump caused panic");
            strcpy(hw->mappedReason, "KERNEL_PANIC");
            break;
        case 0xE0:
            strcpy(info->source, "WATCH_DOG");
            strcpy(info->reason, "WATCHDOG_REBOOT");
            strcpy(info->customReason, "WATCHDOG_REBOOT");
            strcpy(info->otherReason, "Reboot due to watch dog timer");
            strcpy(hw->mappedReason, "WATCHDOG_REBOOT");
            break;
        default:
            strcpy(info->source, "HARD_POWER_RESET");
            strcpy(info->reason, "UNKNOWN_RESET");
            strcpy(info->customReason, "UNKNOWN");
            strcpy(info->otherReason, "Reboot due to unknown reason");
            strcpy(hw->mappedReason, "UNKNOWN_RESET");
            break;
    }

    if (saved_timestamp[0] != '\0') {
        strncpy(info->timestamp, saved_timestamp, sizeof(info->timestamp) - 1);
        info->timestamp[sizeof(info->timestamp) - 1] = '\0';
    }
    return SUCCESS;
}
