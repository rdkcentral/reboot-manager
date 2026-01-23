#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "rebootNow.h"
#include "secure_wrapper.h"
#include "rbus_interface.h"

static const char *PREVIOUS_REBOOT_INFO_FILE = "/opt/secure/reboot/previousreboot.info";
static const char *REBOOTNOW_FLAG = "/opt/secure/reboot/rebootNow";
static const char *REBOOTSTOP_FLAG = "/opt/secure/reboot/rebootStop";
static const char *REBOOT_COUNTER_FILE = "/opt/secure/reboot/rebootCounter";

/* timestamp_update and append_line_to_file provided by log_utils */
static int file_exists(const char *path)
{
    struct stat st;
    return (path && stat(path, &st) == 0);
}
static int read_int_file(const char *path, int *out)
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
static void write_int_file(const char *path, int v)
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
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return -1;
    double up = 0.0;
    if (fscanf(f, "%lf", &up) != 1) { fclose(f); return -1; }
    fclose(f);
    return (int)up;
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
    while (mn > 60) {
        mn -= 60;
        hr += 1;
        if (hr > 23) hr = 0;
    }
    snprintf(out, outsz, "*/%d %d * * *", (mn <= 0 ? 1 : mn), hr);
}
/* run_cmd_capture now provided by cmd_utils */

int handle_cyclic_reboot(const char *source,
                         const char *rebootReason,
                         const char *customReason,
                         const char *otherReason)
{
    /* Read RFC detection and duration */
    char det_buf[64] = {0};
    char dur_buf[64] = {0};
    int detection_enabled = 1;
    int duration = 0;
    int stop_duration = 30; /* minutes */
    rbusError_t rc = RBUS_ERROR_BUS_ERROR;

    if (rbus_get_int_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Detection", &detection_enabled)) {
         RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"[%s:%d]:"RebootStop Detection: %d\n",__FUNCTION__, __LINE__,detection_enabled);
    }
    if (rbus_get_int_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Detection", &duration)) {
         RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"[%s:%d]:"RebootStop Duration: %d\n",__FUNCTION__, __LINE__,duration);
         if (duration > 0 && duration < 24*60) {
             stop_duration = duration;
         }
    }
    
    int reboot_counter_reset = 0;
    if (file_exists(REBOOTNOW_FLAG) && detection_enabled) {
        RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot Loop Detection enabled to check cyclic reboot scenarios:%s", detection_enabled ? "true" : "false");
        char p_src[128] = {0}, p_rsn[128] = {0}, p_cus[128] = {0}, p_oth[256] = {0}, p_ts[64] = {0};
        if (read_previous_reboot_info(p_src, sizeof(p_src), p_rsn, sizeof(p_rsn), p_cus, sizeof(p_cus), p_oth, sizeof(p_oth), p_ts, sizeof(p_ts)) == 0) {
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Previous Reboot Information of the Device: Time:%s Source:%s Reason:%s customReason:%s otherReason:%s",
                 p_ts, p_src, p_rsn, p_cus, p_oth);
            int upsecs = read_proc_uptime_secs();
            if (upsecs >= 0) {
                RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Device Uptime from last reboot: %d secs", upsecs);
            }
            const int REBOOT_WINDOW_SECS = 10 * 60;
            if (upsecs >= 0 && upsecs <= REBOOT_WINDOW_SECS) {
                RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot requested before the %d mins, checking reboot reason", 10);
                int same = 0;
                if (source && strcmp(source, p_src) == 0 &&
                    rebootReason && strcmp(rebootReason, p_rsn) == 0 &&
                    customReason && strcmp(customReason, p_cus) == 0 &&
                    otherReason && strcmp(otherReason, p_oth) == 0) {
                    same = 1;
                }
                if (same) {
                    RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot Reason for current and previous reboot is same");
                    if (file_exists(REBOOTSTOP_FLAG)) {
                        RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot Operation Halted in the device to avoid continous reboots with same reason!!!");
                        touch_file(REBOOTNOW_FLAG);
                        RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Exiting without rebooting the device");
                        return 0; /* defer reboot */
                    } else {
                        int count = 0;
                        (void)read_int_file(REBOOT_COUNTER_FILE, &count);
                        RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Checking device is stuck in cyclic reboot loop with same reboot reason, Current Iteration:%d", count);
                        const int REBOOT_CYCLE_THRESHOLD = 5;
                        if (count >= REBOOT_CYCLE_THRESHOLD) {
                            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Detected Reboot Loop in device, Halting reboot for next %d mins to perform operations!!!", stop_duration);
                            touch_file(REBOOTSTOP_FLAG);
                            rc = rbus_set(rrdRbusHandle,"Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Enable", true, NULL);
                            bool reboot_stop_enable = false;
                            if (rbus_get_bool_param("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Enable", &reboot_stop_enable)) {
                                RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"[%s:%d]:"Publishing Reboot Stop Enable Event: %d\n",__FUNCTION__, __LINE__,reboot_stop_enable);
                            }
                            t2CountNotify("SYST_ERR_Cyclic_reboot", 1);
                            v_secure_system(""sh /lib/rdk/cronjobs_update.sh \"remove\" \"rebootmanager\"");
                            char cron[64];
                            compute_cron_time(stop_duration, cron, sizeof(cron));
                            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Scheduling Cron for rebootmanager as a part of Cyclic reboot operations: %s", cron);
                            char cmd[512];
                            snprintf(cmd, sizeof(cmd),
                                     "sh /lib/rdk/cronjobs_update.sh \"add\" \"rebootmanager\" \"%s /usr/local/bin/rebootmanager -s \"CyclicReboot\" -o \"Rebooting device after expiry of Cyclic reboot pause window\"\"",
                                     cron);
                            (void)system(cmd);
                            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Device will reboot in %d mins after expiry of Cyclic reboot pause window!!!", stop_duration);
                            touch_file(REBOOTNOW_FLAG);
                            return 0; /* defer reboot */
                        } else {
                            count += 1;
                            write_int_file(REBOOT_COUNTER_FILE, count);
                        }
                    }
                } else {
                    RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot requested before the %d mins reboot loop window with different reason", 10);
                    reboot_counter_reset = 1;
                }
            } else {
                RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot requested after the %d mins reboot loop window", 10);
                reboot_counter_reset = 1;
            }
        } else {
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"%s file not found, proceed without reboot reason check!!!", PREVIOUS_REBOOT_INFO_FILE);
            reboot_counter_reset = 1;
        }

        if (reboot_counter_reset) {
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Publishing Reboot Stop Disable Event");
            rc = rbus_set(rrdRbusHandle,"Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Enable", false, NULL);
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot Loop Detection counter reset as device not in reboot loop");
            write_int_file(REBOOT_COUNTER_FILE, 0);
            (void)unlink(REBOOTSTOP_FLAG);
            /* Ensure any previously scheduled cyclic reboot cron job is removed */
            v_secure_system("sh /lib/rdk/cronjobs_update.sh \"remove\" \"rebootmanager\"");
        }
    } else {
        if (!file_exists(REBOOTNOW_FLAG)) {
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Last reboot was not triggered by rebootmanager binary");
        } else {
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"Reboot Loop Detection disabled to check cyclic reboot scenarios:%s", detection_enabled ? "true" : "false");
        }
        /* proceed with reboot */
    }

    return 1; /* proceed immediately */
}
