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
