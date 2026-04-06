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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

static void trim_newline(char *str)
{
    size_t len;

    if (!str) {
        return;
    }

    len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

static void trim_spaces(char *str)
{
    char *start = str;
    char *end;

    if (!str || *str == '\0') {
        return;
    }

    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
}

static int extract_value_from_key_line(const char *line, const char *key, char *out, size_t outsz)
{
    const char *start;

    if (!line || !key || !out || outsz == 0) {
        return FAILURE;
    }

    start = strstr(line, key);
    if (!start) {
        return FAILURE;
    }

    start += strlen(key);
    while (*start == ' ' || *start == '\t') {
        start++;
    }

    strncpy(out, start, outsz - 1);
    out[outsz - 1] = '\0';
    trim_newline(out);
    trim_spaces(out);
    return SUCCESS;
}

static int read_matching_line(const char *path,
                              const char *must_contain,
                              const char *must_not_contain1,
                              const char *must_not_contain2,
                              char *out,
                              size_t outsz)
{
    FILE *fp;
    char line[MAX_BUFFER_SIZE];

    if (!path || !must_contain || !out || outsz == 0) {
        return FAILURE;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return FAILURE;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, must_contain)) {
            continue;
        }
        if (must_not_contain1 && strstr(line, must_not_contain1)) {
            continue;
        }
        if (must_not_contain2 && strstr(line, must_not_contain2)) {
            continue;
        }

        strncpy(out, line, outsz - 1);
        out[outsz - 1] = '\0';
        trim_newline(out);
        fclose(fp);
        return SUCCESS;
    }

    fclose(fp);
    return FAILURE;
}

static int read_field_value(const char *path,
                            const char *field,
                            const char *exclude,
                            char *out,
                            size_t outsz)
{
    FILE *fp;
    char line[MAX_BUFFER_SIZE];

    if (!path || !field || !out || outsz == 0) {
        return FAILURE;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return FAILURE;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, field)) {
            continue;
        }
        if (exclude && strstr(line, exclude)) {
            continue;
        }
        if (extract_value_from_key_line(line, field, out, outsz) == SUCCESS) {
            fclose(fp);
            return SUCCESS;
        }
    }

    fclose(fp);
    return FAILURE;
}

static int find_file_recursive(const char *dir_path, const char *target_name, char *result, size_t result_sz)
{
    DIR *dir;
    struct dirent *entry;

    if (!dir_path || !target_name || !result || result_sz == 0) {
        return FAILURE;
    }

    dir = opendir(dir_path);
    if (!dir) {
        return FAILURE;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[PATH_MAX];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name) >= (int)sizeof(full_path)) {
            continue;
        }

        if (lstat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (find_file_recursive(full_path, target_name, result, result_sz) == SUCCESS) {
                closedir(dir);
                return SUCCESS;
            }
        } else if (strcmp(entry->d_name, target_name) == 0) {
            strncpy(result, full_path, result_sz - 1);
            result[result_sz - 1] = '\0';
            closedir(dir);
            return SUCCESS;
        }
    }

    closedir(dir);
    return FAILURE;
}

static int get_parent_dir(const char *path, char *out, size_t outsz)
{
    char temp[PATH_MAX];
    char *slash;

    if (!path || !out || outsz == 0) {
        return FAILURE;
    }

    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    slash = strrchr(temp, '/');
    if (!slash) {
        return FAILURE;
    }
    *slash = '\0';

    strncpy(out, temp, outsz - 1);
    out[outsz - 1] = '\0';
    return SUCCESS;
}

static int parse_triggered_from_reboot_reason(const char *line,
                                              char *initiated_by,
                                              size_t initiated_by_sz,
                                              char *other_reason,
                                              size_t other_reason_sz)
{
    const char *trigger = "Triggered from ";
    const char *start = strstr(line, trigger);
    const char *space;

    if (!line || !initiated_by || !other_reason || !start) {
        return FAILURE;
    }

    start += strlen(trigger);
    space = strchr(start, ' ');
    if (!space) {
        return FAILURE;
    }

    {
        size_t len = (size_t)(space - start);
        if (len >= initiated_by_sz) {
            len = initiated_by_sz - 1;
        }
        memcpy(initiated_by, start, len);
        initiated_by[len] = '\0';
    }

    strncpy(other_reason, space + 1, other_reason_sz - 1);
    other_reason[other_reason_sz - 1] = '\0';

    {
        char *paren = strchr(other_reason, '(');
        if (paren) {
            *paren = '\0';
        }
    }

    trim_spaces(other_reason);
    trim_newline(other_reason);
    return SUCCESS;
}

int parse_bootup_legacy_reboot_log(const char *prevLogPath,
                                   char *rebootReason,
                                   size_t rebootReasonSize,
                                   RebootInfo *info)
{
    char marker_path[PATH_MAX] = {0};
    char marker_dir[PATH_MAX] = {0};
    char last_reboot_file[PATH_MAX] = {0};
    int index;

    if (!prevLogPath || !rebootReason || rebootReasonSize == 0 || !info) {
        return FAILURE;
    }

    memset(info, 0, sizeof(*info));
    rebootReason[0] = '\0';

    if (find_file_recursive(prevLogPath, "last_reboot", marker_path, sizeof(marker_path)) == SUCCESS &&
        get_parent_dir(marker_path, marker_dir, sizeof(marker_dir)) == SUCCESS) {
        snprintf(last_reboot_file, sizeof(last_reboot_file), "%s/rebootInfo.log", marker_dir);
        if (access(last_reboot_file, F_OK) != 0) {
            last_reboot_file[0] = '\0';
        }
    }

    if (last_reboot_file[0] == '\0') {
        snprintf(last_reboot_file, sizeof(last_reboot_file), "%s/rebootInfo.log", prevLogPath);
        if (access(last_reboot_file, F_OK) != 0) {
            last_reboot_file[0] = '\0';
        }
    }

    if (last_reboot_file[0] != '\0') {
        char default_prev_file[PATH_MAX];
        snprintf(default_prev_file, sizeof(default_prev_file), "%s/rebootInfo.log", prevLogPath);
        if (strcmp(last_reboot_file, default_prev_file) == 0) {
            for (index = 1; index <= 3; ++index) {
                char bak_path[PATH_MAX];
                snprintf(bak_path, sizeof(bak_path), "%s/bak%d_rebootInfo.log", prevLogPath, index);
                if (access(bak_path, F_OK) == 0) {
                    strncpy(last_reboot_file, bak_path, sizeof(last_reboot_file) - 1);
                    last_reboot_file[sizeof(last_reboot_file) - 1] = '\0';
                }
            }
        }
    }

    if (last_reboot_file[0] == '\0' || access(last_reboot_file, F_OK) != 0) {
        return FAILURE;
    }

    (void)read_matching_line(last_reboot_file,
                             "RebootReason:",
                             "HAL_SYS_Reboot",
                             "PreviousRebootReason",
                             rebootReason,
                             rebootReasonSize);

    (void)read_field_value(last_reboot_file,
                           "RebootInitiatedBy:",
                           "PreviousRebootInitiatedBy",
                           info->source,
                           sizeof(info->source));

    (void)read_field_value(last_reboot_file,
                           "RebootTime:",
                           "PreviousRebootTime",
                           info->timestamp,
                           sizeof(info->timestamp));

    (void)read_field_value(last_reboot_file,
                           "CustomReason:",
                           "PreviousCustomReason",
                           info->customReason,
                           sizeof(info->customReason));

    if (strcmp(info->source, "HAL_SYS_Reboot") == 0) {
        char trigger_line[MAX_BUFFER_SIZE] = {0};
        if (read_matching_line(last_reboot_file,
                               "RebootReason:",
                               "HAL_SYS_Reboot",
                               "PreviousRebootReason",
                               trigger_line,
                               sizeof(trigger_line)) == SUCCESS) {
            (void)parse_triggered_from_reboot_reason(trigger_line,
                                                     info->source,
                                                     sizeof(info->source),
                                                     info->otherReason,
                                                     sizeof(info->otherReason));
        }
    } else {
        (void)read_field_value(last_reboot_file,
                               "OtherReason:",
                               "PreviousOtherReason",
                               info->otherReason,
                               sizeof(info->otherReason));
    }

    return SUCCESS;
}

