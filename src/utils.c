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

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include "rdk_logger.h"

void timestamp_update(char *buf, size_t sz)
{
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        if (buf && sz > 0) {
            buf[0] = '\0';
        }
        return;
    }

    struct tm tm_utc;
#if defined(_WIN32)
    if (gmtime_s(&tm_utc, &now) != 0) {
        if (buf && sz > 0) {
            buf[0] = '\0';
        }
        return;
    }
#else
    if (gmtime_r(&now, &tm_utc) == NULL) {
        if (buf && sz > 0) {
            buf[0] = '\0';
        }
        return;
    }
#endif
    if (buf && sz > 0) {
        strftime(buf, sz, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    }
}

int append_line_to_file(const char *path, const char *line)
{
    FILE *f = fopen(path, "a");
    if (!f) {
        return -1;
    }
    if (fputs(line, f) == EOF) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int run_cmd_capture(const char *cmd, char *out, size_t outsz)
{
    if (!cmd) return -1;
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    size_t n = 0;
    if (out && outsz) {
        n = fread(out, 1, outsz - 1, p);
        out[n] = '\0';
    }
    int status = pclose(p);
    return status;
}
