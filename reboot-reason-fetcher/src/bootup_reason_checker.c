/*
##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
*/

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "update-reboot-info.h"
#include "rdk_logger.h"
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

static int logfile_path_check(char *dst, size_t dst_len, const char *left, const char *right)
{
    size_t left_len;
    size_t right_len;

    if (!dst || dst_len == 0 || !left || !right) {
        return ERROR_GENERAL;
    }

    left_len = strlen(left);
    right_len = strlen(right);
    if (left_len + 1 + right_len + 1 > dst_len) {
        return ERROR_GENERAL;
    }

    memcpy(dst, left, left_len);
    dst[left_len] = '/';
    memcpy(dst + left_len + 1, right, right_len);
    dst[left_len + 1 + right_len] = '\0';
    return SUCCESS;
}

/*
 * find_previous_reboot_log - replicates the log-discovery logic from reboot-checker.sh:
 *   1. Walk $LOG_PATH/PreviousLogs sub-directories for a file named "last_reboot";
 *      if found use that directory's rebootInfo.log.
 *   2. Fall back to $LOG_PATH/PreviousLogs/rebootInfo.log.
 *   3. If the flat fallback is selected, prefer bak[1-3]_rebootInfo.log
 *      (handles fast-reboot on non-HDD devices before the 8-min log rotation).
 */
int find_previous_reboot_log(char *out_path, size_t len)
{
    const char *log_base;
    char prev_logs[MAX_PATH_LENGTH];
    char candidate[MAX_PATH_LENGTH];
    int i;

    if (!out_path || len == 0) return ERROR_GENERAL;
    out_path[0] = '\0';

    log_base = getenv("LOG_PATH");
    if (!log_base || log_base[0] == '\0') {
        log_base = "/opt/logs";
    }
    if (logfile_path_check(prev_logs, sizeof(prev_logs), log_base, "PreviousLogs") != SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Path too long for PreviousLogs under %s\n", log_base);
        return ERROR_GENERAL;
    }

    DIR *d = opendir(prev_logs);
    if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                char subdir[MAX_PATH_LENGTH];
                char marker[MAX_PATH_LENGTH];
                struct stat st;

                if (ent->d_name[0] == '.') continue;
                if (logfile_path_check(subdir, sizeof(subdir), prev_logs, ent->d_name) != SUCCESS) continue;
                if (stat(subdir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

                if (logfile_path_check(marker, sizeof(marker), subdir, "last_reboot") != SUCCESS) continue;
                if (access(marker, F_OK) == 0) {
                    if (logfile_path_check(candidate, sizeof(candidate), subdir, "rebootInfo.log") != SUCCESS) {
                        break;
                    }
                    if (access(candidate, F_OK) == 0) {
                        closedir(d);
                        strncpy(out_path, candidate, len - 1);
                        out_path[len - 1] = '\0';
                        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Previous reboot log (timestamped dir): %s\n", out_path);
                        return SUCCESS;
                    }
                    break; /* found marker dir but no log; fall through to flat fallback */
                }
            }
            closedir(d);
    }

    if (logfile_path_check(candidate, sizeof(candidate), prev_logs, "rebootInfo.log") != SUCCESS) {
        return ERROR_GENERAL;
    }
    if (access(candidate, F_OK) == 0) {
        for (i = 1; i <= 3; i++) {
            char bak[MAX_PATH_LENGTH];
            char bak_name[] = "bak1_rebootInfo.log";
            bak_name[3] = (char)('0' + i);
            if (logfile_path_check(bak, sizeof(bak), prev_logs, bak_name) != SUCCESS) {
                continue;
            }
            if (access(bak, F_OK) == 0) {
                strncpy(out_path, bak, len - 1);
                out_path[len - 1] = '\0';
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Previous reboot log (bak%d fallback): %s\n", i, out_path);
                return SUCCESS;
            }
        }
        strncpy(out_path, candidate, len - 1);
        out_path[len - 1] = '\0';
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Previous reboot log (flat fallback): %s\n", out_path);
        return SUCCESS;
    }

    RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","No previous reboot log found under %s\n", prev_logs);
    return ERROR_FILE_NOT_FOUND;
}

/*
 * resolve_hal_sys_reboot - when RebootInitiatedBy is "HAL_SYS_Reboot", the real
 * initiator and other-reason are embedded in the RebootReason line:
 *   "... Triggered from <initiator> <reason words> (optional paren)"
 */
static void resolve_hal_sys_reboot(const char *rebootReasonLine,
                                   char *source, size_t srcLen,
                                   char *otherReason, size_t orLen)
{
    const char *trigger;
    const char *space;
    const char *rest;
    const char *paren;
    const char *endRest;
    size_t initLen;
    size_t restLen;
    char *end;

    if (!rebootReasonLine || !source || !otherReason) return;

    trigger = strstr(rebootReasonLine, "Triggered from ");
    if (!trigger) return;
    trigger += strlen("Triggered from ");

    space = strchr(trigger, ' ');
    if (!space) {
        strncpy(source, trigger, srcLen - 1);
        source[srcLen - 1] = '\0';
        end = source + strlen(source) - 1;
        while (end >= source && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
            *end-- = '\0';
        }
        return;
    }
    initLen = (size_t)(space - trigger);
    if (initLen >= srcLen) initLen = srcLen - 1;
    memcpy(source, trigger, initLen);
    source[initLen] = '\0';

    rest = space + 1;
    while (*rest == ' ') rest++;
    paren = strchr(rest, '(');
    endRest = paren ? paren : (rest + strlen(rest));
    restLen = (size_t)(endRest - rest);
    while (restLen > 0 && (rest[restLen - 1] == ' ' || rest[restLen - 1] == '\t')) restLen--;
    if (restLen >= orLen) restLen = orLen - 1;
    memcpy(otherReason, rest, restLen);
    otherReason[restLen] = '\0';
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
    int raw_fields = 0;
    char rawInitiatedBy[MAX_REASON_LENGTH] = {0};
    char rawTime[MAX_TIMESTAMP_LENGTH] = {0};
    char rawCustom[MAX_REASON_LENGTH] = {0};
    char rawOther[MAX_REASON_LENGTH] = {0};
    char rawRebootReason[MAX_BUFFER_SIZE] = {0};

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
        } else if (strstr(line, "PreviousRebootTime:")) {
            getVal(strstr(line, "PreviousRebootTime:"), "PreviousRebootTime:", info->timestamp, sizeof(info->timestamp));
            found_fields++;
        } else if (strstr(line, "PreviousCustomReason:")) {
            getVal(strstr(line, "PreviousCustomReason:"), "PreviousCustomReason:", info->customReason, sizeof(info->customReason));
            found_fields++;
        } else if (strstr(line, "PreviousOtherReason:")) {
            getVal(strstr(line, "PreviousOtherReason:"), "PreviousOtherReason:", info->otherReason, sizeof(info->otherReason));
            found_fields++;
        } else if (strstr(line, "RebootInitiatedBy:")) {
            getVal(strstr(line, "RebootInitiatedBy:"), "RebootInitiatedBy:", rawInitiatedBy, sizeof(rawInitiatedBy));
            raw_fields++;
        } else if (strstr(line, "RebootTime:")) {
            getVal(strstr(line, "RebootTime:"), "RebootTime:", rawTime, sizeof(rawTime));
            raw_fields++;
        } else if (strstr(line, "CustomReason:")) {
            getVal(strstr(line, "CustomReason:"), "CustomReason:", rawCustom, sizeof(rawCustom));
            raw_fields++;
        } else if (strstr(line, "OtherReason:")) {
            getVal(strstr(line, "OtherReason:"), "OtherReason:", rawOther, sizeof(rawOther));
            raw_fields++;
        } else if (strstr(line, "RebootReason:") && !strstr(line, "HAL_SYS_Reboot")) {
            strncpy(rawRebootReason, line, sizeof(rawRebootReason) - 1);
            rawRebootReason[sizeof(rawRebootReason) - 1] = '\0';
        }

        if (found_fields >= 4) break;
    }
    fclose(fp);

    if (found_fields == 0 && raw_fields > 0) {
        if (strcmp(rawInitiatedBy, "HAL_SYS_Reboot") == 0 && rawRebootReason[0] != '\0') {
            resolve_hal_sys_reboot(rawRebootReason, rawInitiatedBy, sizeof(rawInitiatedBy), rawOther, sizeof(rawOther));
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","HAL_SYS_Reboot resolved: initiator=%s, other=%s\n", rawInitiatedBy, rawOther);
        }
        strncpy(info->source,       rawInitiatedBy, sizeof(info->source)       - 1);
        info->source[sizeof(info->source) - 1]             = '\0';
        strncpy(info->timestamp,    rawTime,         sizeof(info->timestamp)    - 1);
        info->timestamp[sizeof(info->timestamp) - 1]       = '\0';
        strncpy(info->customReason, rawCustom,       sizeof(info->customReason) - 1);
        info->customReason[sizeof(info->customReason) - 1] = '\0';
        strncpy(info->otherReason,  rawOther,        sizeof(info->otherReason)  - 1);
        info->otherReason[sizeof(info->otherReason) - 1]   = '\0';
        found_fields = raw_fields;
    }

    if (found_fields == 0) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","No reboot info fields found in legacy log\n");
        return FAILURE;
    }

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Parsed legacy log - Found %d fields\n", found_fields);
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Timestamp: %s, Source: %s, Reason: %s\n", info->timestamp, info->source, info->reason);
    return SUCCESS;
}

static int extract_json_value(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *start;
    const char *end;
    size_t len;

    if (!json || !key || !out || out_size == 0) {
        return ERROR_GENERAL;
    }

    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    start = strstr(json, pattern);
    if (!start) {
        return ERROR_GENERAL;
    }
    start += strlen(pattern);

    end = strchr(start, '"');
    if (!end) {
        return ERROR_GENERAL;
    }

    len = (size_t)(end - start);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return SUCCESS;
}

static int load_reboot_info_json(const char *path, RebootInfo *info)
{
    FILE *fp;
    char buf[2048];
    size_t n;

    if (!path || !info) {
        return ERROR_GENERAL;
    }

    fp = fopen(path, "r");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", "Failed to open reboot info json %s: %s\n", path, strerror(errno));
        return ERROR_FILE_NOT_FOUND;
    }

    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    if (extract_json_value(buf, "timestamp", info->timestamp, sizeof(info->timestamp)) != SUCCESS ||
        extract_json_value(buf, "source", info->source, sizeof(info->source)) != SUCCESS ||
        extract_json_value(buf, "reason", info->reason, sizeof(info->reason)) != SUCCESS ||
        extract_json_value(buf, "customReason", info->customReason, sizeof(info->customReason)) != SUCCESS ||
        extract_json_value(buf, "otherReason", info->otherReason, sizeof(info->otherReason)) != SUCCESS) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", "Failed to parse reboot info json from %s\n", path);
        return ERROR_PARSE_FAILED;
    }

    return SUCCESS;
}

static void format_iso8601_ms(char *out, size_t out_len)
{
    struct timeval tv;
    struct tm tm_utc;
    struct tm *tm_result;

    if (!out || out_len == 0) {
        return;
    }

    if (gettimeofday(&tv, NULL) != 0) {
        out[0] = '\0';
        return;
    }

    tm_result = gmtime_r(&tv.tv_sec, &tm_utc);
    if (!tm_result) {
        out[0] = '\0';
        return;
    }

    snprintf(out, out_len,
             "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             tm_utc.tm_year + 1900,
             tm_utc.tm_mon + 1,
             tm_utc.tm_mday,
             tm_utc.tm_hour,
             tm_utc.tm_min,
             tm_utc.tm_sec,
             (long)(tv.tv_usec / 1000L));
}

static int write_previous_line(FILE *fp, const char *key, const char *value)
{
    char ts[64];

    if (!fp || !key) {
        return ERROR_GENERAL;
    }

    format_iso8601_ms(ts, sizeof(ts));

    /* Always write the line; use empty timestamp if formatting failed */
    fprintf(fp, "%s %s: %s\n", ts, key, value ? value : "");
    return SUCCESS;
}

static int load_previous_reboot_reason_line(char *out, size_t out_len)
{
    char prev_log_path[MAX_PATH_LENGTH] = {0};
    FILE *fp;
    char line[MAX_BUFFER_SIZE];

    if (!out || out_len == 0) {
        return ERROR_GENERAL;
    }
    out[0] = '\0';

    if (find_previous_reboot_log(prev_log_path, sizeof(prev_log_path)) != SUCCESS) {
        return ERROR_FILE_NOT_FOUND;
    }

    fp = fopen(prev_log_path, "r");
    if (!fp) {
        return ERROR_FILE_NOT_FOUND;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') {
            trimmed++;
        }

        if (strstr(trimmed, "PreviousRebootReason:")) {
            continue;
        }

        if (strncmp(trimmed, "RebootReason:", strlen("RebootReason:")) == 0) {
            size_t len;
            char *end;

            strncpy(out, trimmed, out_len - 1);
            out[out_len - 1] = '\0';

            len = strlen(out);
            if (len > 0) {
                end = out + len - 1;
                while (end >= out && (*end == '\n' || *end == '\r')) {
                    *end-- = '\0';
                }
            }
            fclose(fp);
            return SUCCESS;
        }
    }

    fclose(fp);
    return ERROR_FILE_NOT_FOUND;
}

int update_previous_reboot_log_fields(const char *jsonPath, const RebootInfo *fallbackInfo)
{
    FILE *fp;
    RebootInfo parsedInfo;
    RebootInfo emptyInfo;
    const RebootInfo *infoToWrite = fallbackInfo;
    char previousReason[MAX_BUFFER_SIZE] = {0};

    memset(&parsedInfo, 0, sizeof(parsedInfo));
    memset(&emptyInfo, 0, sizeof(emptyInfo));

    if ((!infoToWrite || infoToWrite->source[0] == '\0' || infoToWrite->timestamp[0] == '\0') && jsonPath) {
        if (load_reboot_info_json(jsonPath, &parsedInfo) == SUCCESS) {
            infoToWrite = &parsedInfo;
        }
    }

    if (!infoToWrite) {
        infoToWrite = &emptyInfo;
    }

    if (load_previous_reboot_reason_line(previousReason, sizeof(previousReason)) != SUCCESS) {
        previousReason[0] = '\0';
    }

    fp = fopen(REBOOT_INFO_LOG_FILE, "w");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", "Failed to open %s for PreviousReboot fields: %s\n", REBOOT_INFO_LOG_FILE, strerror(errno));
        return ERROR_GENERAL;
    }

    (void)write_previous_line(fp, "PreviousRebootReason", previousReason);
    (void)write_previous_line(fp, "PreviousRebootInitiatedBy", infoToWrite->source);
    (void)write_previous_line(fp, "PreviousRebootTime", infoToWrite->timestamp);
    (void)write_previous_line(fp, "PreviousCustomReason", infoToWrite->customReason);
    (void)write_previous_line(fp, "PreviousOtherReason", infoToWrite->otherReason);
    fflush(fp);
    fclose(fp);

    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", "Updated PreviousReboot* fields in %s\n", REBOOT_INFO_LOG_FILE);
    return SUCCESS;
}



