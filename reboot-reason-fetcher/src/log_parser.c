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
        return ERROR_GENERAL;
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
    if (getDevicePropertyData("RDK_PROFILE", buffer, sizeof(buffer)) == UTILS_SUCCESS) {
        size_t len = strlen(buffer);
        if (len >= sizeof(ctx->rdkProfile)) {
            len = sizeof(ctx->rdkProfile) - 1;
        }
        memcpy(ctx->rdkProfile, buffer, len);
        ctx->rdkProfile[len] = '\0';
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
     if (ctx->soc[0] == '\0' || ctx->device_type[0] == '\0' || ctx->rdkProfile[0] == '\0') {
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
                } else if (ctx->rdkProfile[0] == '\0' && strcmp(key, "RDK_PROFILE") == 0) {
                    size_t len = strlen(val);
                    if (len >= sizeof(ctx->rdkProfile)) {
                        len = sizeof(ctx->rdkProfile) - 1;
                    }
                    memcpy(ctx->rdkProfile, val, len);
                    ctx->rdkProfile[len] = '\0';
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
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Device properties parsed - SOC: %s, RDK_PROFILE: %s, BuildType: %s, DeviceType: %s\n", ctx->soc, ctx->rdkProfile, ctx->buildType, ctx->device_type);
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
    if (access(STT_FLAG, F_OK) != 0 || access(REBOOT_INFO_FLAG, F_OK) != 0) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","STT or RebootInfo flag missing; skip update\n");
        return 0;
    }
    return 1;
}

int get_hardware_reason(const EnvContext *ctx, HardwareReason *hwReason, RebootInfo *info)
{
    if (ctx == NULL || hwReason == NULL || info == NULL) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", "get_hardware_reason: invalid argument(s): ctx=%p, hwReason=%p, info=%p",
            (const void *)ctx, (void *)hwReason, (void *)info);
        return ERROR_GENERAL;
    }

    memset(hwReason, 0, sizeof(HardwareReason));

    if (strcmp(ctx->soc, "BRCM") == 0) {
        if (read_brcm_previous_reboot_reason(hwReason) == SUCCESS) {
            return SUCCESS;
        }
    }

    if (hwReason->mappedReason[0] == '\0') {
        RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Hardware reason not determined (SOC='%s')", ctx->soc);
        strcpy(hwReason->mappedReason, "UNKNOWN");
    }

    return SUCCESS;
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
        return ERROR_GENERAL;
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

