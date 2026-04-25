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

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "rdk_logger.h"
#include "reboot.h"

void timestamp_update(char *buf, size_t sz)
{
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    if (strftime(buf, sz, "%a %b %d %H:%M:%S UTC %Y", tm_info) == 0) {
        buf[0] = '\0';
    }
}

int write_rebootinfo_log(const char *path, const char *line)
{
    FILE *f = fopen(path, "a");
    if (!f) {
        return -1;
    }
    if (fputs(line, f) == EOF) {
        fclose(f);
        return -1;
    }
    if (fclose(f) == EOF) {
        return -1;
    }
    return 0;
}

void t2CountNotify(const char *marker, int val)
{
#ifdef T2_EVENT_ENABLED
    if (marker && *marker) {
        t2_event_d(marker, val);
    }
#else
    (void)marker; (void)val;
#endif
}

void t2ValNotify(const char *marker, const char *val)
{
#ifdef T2_EVENT_ENABLED
    if (marker && *marker && val) {
        t2_event_s(marker, val);
    }
#else
    (void)marker; (void)val;
#endif
}

static bool rfc_read_param_value(const char *param_name, char *value_buf, size_t buf_size)
{
    if (!param_name || !value_buf || buf_size == 0) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO",
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }
    RFC_ParamData_t param;
    memset(&param, 0, sizeof(param));
    WDMP_STATUS status = getRFCParameter(RFC_CALLER_ID, param_name, &param);
    if (status != WDMP_SUCCESS && status != WDMP_ERR_DEFAULT_VALUE) {
        RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO",
                "[%s:%d] getRFCParameter(%s) failed: status=%d\n",
                __FUNCTION__, __LINE__, param_name, (int)status);
        return false;
    }
    if (param.value[0] == '\0') {
        RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO",
                "[%s:%d] getRFCParameter(%s) returned empty value\n",
                __FUNCTION__, __LINE__, param_name);
        return false;
    }
    size_t data_len = strlen(param.value);
    if (data_len >= 2 && param.value[0] == '"' && param.value[data_len - 1] == '"') {
        size_t copy_len = data_len - 2;
        if (copy_len >= buf_size) {
            copy_len = buf_size - 1;
        }
        memcpy(value_buf, &param.value[1], copy_len);
        value_buf[copy_len] = '\0';
    } else {
        strncpy(value_buf, param.value, buf_size - 1);
        value_buf[buf_size - 1] = '\0';
    }
    return true;
}

bool rfc_get_string_param(const char *param_name, char *value_buf, size_t buf_size)
{
    if (!rfc_read_param_value(param_name, value_buf, buf_size))
        return false;
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO",
            "[%s:%d] %s = \"%s\"\n", __FUNCTION__, __LINE__, param_name, value_buf);
    return true;
}

bool rfc_get_bool_param(const char *param_name, bool *value)
{
    if (!param_name || !value) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO",
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }
    char raw[64] = {0};
    if (!rfc_read_param_value(param_name, raw, sizeof(raw)))
        return false;
    *value = (strcasecmp(raw, "true") == 0 || strcmp(raw, "1") == 0);
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO",
            "[%s:%d] %s = %s\n", __FUNCTION__, __LINE__,
            param_name, *value ? "true" : "false");
    return true;
}

bool rfc_get_int_param(const char *param_name, int *value)
{
    if (!param_name || !value) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO",
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }
    char raw[64] = {0};
    if (!rfc_read_param_value(param_name, raw, sizeof(raw)))
        return false;
    char *endptr = NULL;
    long v = strtol(raw, &endptr, 10);
    if (endptr == raw || (*endptr != '\0' && *endptr != '\n')) {
        RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO",
                "[%s:%d] Cannot parse \"%s\" as integer for %s\n",
                __FUNCTION__, __LINE__, raw, param_name);
        return false;
    }
    *value = (int)v;
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO",
            "[%s:%d] %s = %d\n", __FUNCTION__, __LINE__, param_name, *value);
    return true;
}

bool rfc_set_bool_param(const char *param_name, bool value)
{
    if (!param_name) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO",
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }
    const char *str_val = value ? "true" : "false";
    WDMP_STATUS status = setRFCParameter(RFC_CALLER_ID, param_name, str_val, WDMP_BOOLEAN);
    if (status != WDMP_SUCCESS) {
        RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO",
                "[%s:%d] setRFCParameter(%s, bool, %s) failed: status=%d\n",
                __FUNCTION__, __LINE__, param_name, str_val, (int)status);
        return false;
    }
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO",
            "[%s:%d] Set %s = %s\n", __FUNCTION__, __LINE__, param_name, str_val);
    return true;
}

bool rfc_set_int_param(const char *param_name, int value)
{
    if (!param_name) {
        RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO",
                "[%s:%d] Invalid parameters\n", __FUNCTION__, __LINE__);
        return false;
    }
    char str_val[32];
    snprintf(str_val, sizeof(str_val), "%d", value);
    WDMP_STATUS status = setRFCParameter(RFC_CALLER_ID, param_name, str_val, WDMP_INT);
    if (status != WDMP_SUCCESS) {
        RDK_LOG(RDK_LOG_WARN, "LOG.RDK.REBOOTINFO",
                "[%s:%d] setRFCParameter(%s, int, %s) failed: status=%d\n",
                __FUNCTION__, __LINE__, param_name, str_val, (int)status);
        return false;
    }
    RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO",
            "[%s:%d] Set %s = %s\n", __FUNCTION__, __LINE__, param_name, str_val);
    return true;
}

