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
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "rebootnow.h"
#include "secure_wrapper.h"
#include "rbus_interface.h"
#include "rdk_logger.h"

static const char *PREVIOUS_REBOOT_INFO_FILE = "/opt/secure/reboot/previousreboot.info";
static const char *REBOOTNOW_FLAG = "/opt/secure/reboot/rebootNow";
static const char *REBOOTSTOP_FLAG = "/opt/secure/reboot/rebootStop";
static const char *REBOOT_COUNTER_FILE = "/opt/secure/reboot/rebootCounter";

static int file_exists(const char *path)
{
    struct stat st;
    return (path && stat(path, &st) == 0);
}

static int read_rebootcounter(const char *path, int *out)
{
    if (!path || !out) return -1;
    FILE *f = fopen(path, "r");
    if (!f) { *out = 0; return -1; }
    long v = 0;
    if (fscanf(f, "%ld", &v) != 1) { fclose(f); *out = 0; return -1; }
    fclose(f);
    *out = (int)v;
    return 0;
}

static void write_rebootcounter(const char *path, int v)
{
    if (!path) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d\n", v);
    fflush(f);
    fclose(f);
}

static void touch_file(const char *path)
{
    if (!path) return;
    FILE *f = fopen(path, "a");
    if (f) fclose(f);
}

static int read_proc_uptime_secs(void)
{
#ifdef GTEST_ENABLE
    return -1;
#else
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return -1;
    double up = 0.0;
    if (fscanf(f, "%lf", &up) != 1) { fclose(f); return -1; }
    fclose(f);
    return (int)up;
#endif
}

static int extract_json_value(const char *buf, const char *key, char *out, size_t outsz)
{
    if (!buf || !key || !out || outsz == 0) return -1;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *p = strstr(buf, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    const char *q = strchr(p, '"');
    if (!q) return -1;
    size_t len = (size_t)(q - p);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

static int read_previous_reboot_info(char *source, size_t ssz,
                                     char *reason, size_t rsz,
                                     char *custom, size_t csz,
                                     char *other, size_t osz,
                                     char *ts, size_t tsz)
{
    if (!file_exists(PREVIOUS_REBOOT_INFO_FILE)) return -1;
    FILE *f = fopen(PREVIOUS_REBOOT_INFO_FILE, "r");
    if (!f) return -1;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    int rc = 0;
    rc |= extract_json_value(buf, "source", source, ssz) != 0;
    rc |= extract_json_value(buf, "reason", reason, rsz) != 0;
    rc |= extract_json_value(buf, "customReason", custom, csz) != 0;
    rc |= extract_json_value(buf, "otherReason", other, osz) != 0;
    (void)extract_json_value(buf, "timestamp", ts, tsz);
    return (rc == 0) ? 0 : -1;
}

static void compute_cron_time(int add_minutes, char *out, size_t outsz)
{
    time_t now = time(NULL);
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    int hr = tm_local.tm_hour;
    int mn = tm_local.tm_min;
    mn += 1; /* alignment fudge */
    mn += add_minutes;
    while (mn < 0) {
        mn += 60;
        hr -= 1;
        if (hr < 0) hr = 23;
    }
    while (mn >= 60) {
        mn -= 60;
        hr += 1;
        if (hr > 23) hr = 0;
    }
    snprintf(out, outsz, "%d %d * * *", mn, hr);
}

int handle_cyclic_reboot(const char *source,
                         const char *rebootReason,
                         const char *customReason,
                         const char *otherReason)
{
    /* Read RFC detection and duration */
    bool detection_enabled = true;
    int duration = 0;
    int stop_duration = 30; /* minutes */
    char p_src[128] = {0}, p_rsn[128] = {0}, p_cus[128] = {0}, p_oth[256] = {0}, p_ts[64] = {0};
    int upsecs = 0;

    int rbret = rbus_get_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Detection", &detection_enabled);
    if (rbret) {
        /* Successfully retrieved RFC value for detection_enabled */
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Loop Detection RFC value retrieved: %s\n",
                detection_enabled ? "true" : "false");
    } else {
        /* RBUS fetch failed; continue using default value */
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Loop Detection using default value (RBUS fetch failed): %s\n",
                detection_enabled ? "true" : "false");
    }
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Loop Detection enabled to check cyclic reboot scenarios:%s\n",
            detection_enabled ? "true" : "false");
    if (rbus_get_int_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Duration", &duration)) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","RebootStop Duration: %d\n", duration);
        if (duration > 0 && duration < 24*60) {
            stop_duration = duration;
        }
    }
    
    int reboot_counter_reset = 0;
    if (file_exists(REBOOTNOW_FLAG) && detection_enabled) {
        (void)unlink(REBOOTNOW_FLAG);
        if (read_previous_reboot_info(p_src, sizeof(p_src), p_rsn, sizeof(p_rsn), p_cus, sizeof(p_cus), p_oth, sizeof(p_oth), p_ts, sizeof(p_ts)) == 0) {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Previous Reboot Information of the Device:\n");
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Time:%s\n", p_ts);
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Source:%s\n", p_src);
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reason:%s\n", p_rsn);
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","customReason:%s\n", p_cus);
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","otherReason:%s\n", p_oth);
 
            upsecs = read_proc_uptime_secs();
            if (upsecs >= 0) {
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Device Uptime from last reboot: %d secs\n", upsecs);
            }

            const int REBOOT_WINDOW_SECS = 10 * 60;
#ifdef GTEST_ENABLE
            if ((upsecs < 0) || (upsecs >= 0 && upsecs <= REBOOT_WINDOW_SECS)) {
#else
            if (upsecs >= 0 && upsecs <= REBOOT_WINDOW_SECS) {
#endif
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot requested before the %d mins, checking reboot reason\n", 10);
                int same = 0;
                if (source && strcmp(source, p_src) == 0 &&
                    rebootReason && strcmp(rebootReason, p_rsn) == 0 &&
                    customReason && strcmp(customReason, p_cus) == 0 &&
                    otherReason && strcmp(otherReason, p_oth) == 0) {
                    same = 1;
                }
                if (same) {
                    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Reason for current and previous reboot is same\n");
                    if (file_exists(REBOOTSTOP_FLAG)) {
                        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Operation Halted in the device to avoid continuous reboots with same reason!!!\n");
                        touch_file(REBOOTNOW_FLAG);
                        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Exiting without rebooting the device\n");
                        return 0; /* defer reboot */
                    } else {
                        int count = 0;
                        (void)read_rebootcounter(REBOOT_COUNTER_FILE, &count);
                        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Checking device is stuck in cyclic reboot loop with same reboot reason, Current Iteration:%d\n", count);
                        const int REBOOT_CYCLE_THRESHOLD = 5;
                        if (count >= REBOOT_CYCLE_THRESHOLD) {
                            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Detected Reboot Loop in device, Halting reboot for next %d mins to perform operations!!!\n", stop_duration);
                            touch_file(REBOOTSTOP_FLAG);
                            rbus_set_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Enable", true);
                            bool reboot_stop_enable = false;
                            if (rbus_get_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Enable", &reboot_stop_enable)) {
                                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","[%s:%d]:Publishing Reboot Stop Enable Event: %d\n",__FUNCTION__, __LINE__,reboot_stop_enable);
                            }
			    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Sending t2 Marker\n");
                            t2CountNotify("SYST_ERR_Cyclic_reboot", 1);
                            v_secure_system("sh /lib/rdk/cronjobs_update.sh %s %s", "remove", "rebootnow");
                            char cron[64];
                            compute_cron_time(stop_duration, cron, sizeof(cron));
                            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Scheduling Cron for rebootnow as a part of Cyclic reboot operations: %s\n", cron);
                            char cron_entry[256];
                            snprintf(cron_entry, sizeof(cron_entry), "%s /usr/bin/rebootnow -s \"CyclicReboot\" -o \"Rebooting device after expiry of Cyclic reboot pause window\"", cron);
                            v_secure_system("sh /lib/rdk/cronjobs_update.sh %s %s \"%s\"", "add", "rebootnow", cron_entry);
                            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Device will reboot in %d mins after expiry of Cyclic reboot pause window!!!\n", stop_duration);
                            touch_file(REBOOTNOW_FLAG);
                            return 0; /* defer reboot */
                        } else {
                            count += 1;
                            write_rebootcounter(REBOOT_COUNTER_FILE, count);
                        }
                    }
                } else {
                    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot requested before the %d mins reboot loop window with different reason\n", 10);
                    reboot_counter_reset = 1;
                }
            } else {
                RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot requested after the %d mins reboot loop window\n", 10);
                reboot_counter_reset = 1;
            }
        } else {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","%s file not found, proceed without reboot reason check!!!\n", PREVIOUS_REBOOT_INFO_FILE);
            reboot_counter_reset = 1;
        }

        if (reboot_counter_reset) {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Publishing Reboot Stop Disable Event\n");
            rbus_set_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Enable", false);
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Loop Detection counter reset as device not in reboot loop\n");
            write_rebootcounter(REBOOT_COUNTER_FILE, 0);
            (void)unlink(REBOOTSTOP_FLAG);
            /* Ensure any previously scheduled cyclic reboot cron job is removed */
            v_secure_system("sh /lib/rdk/cronjobs_update.sh %s %s", "remove", "rebootnow");
        }
    } else {
        if (!file_exists(REBOOTNOW_FLAG)) {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Last reboot was not triggered by rebootnow binary\n");
        } else {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot Loop Detection disabled to check cyclic reboot scenarios:%s\n", detection_enabled ? "true" : "false");
        }
        /* proceed with reboot */
    }

    return 1; /* proceed immediately */
}
