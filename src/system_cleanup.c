#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include "rdk_debug.h"
#include "rebootNow.h"
#include "secure_wrapper.h"

static int file_exists(const char *path)
{
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static int dir_exists(const char *path)
{
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static int ends_with(const char *s, const char *suffix)
{
    if (!s || !suffix) return 0;
    size_t ls = strlen(s), lsuf = strlen(suffix);
    return (ls >= lsuf) && (strcmp(s + ls - lsuf, suffix) == 0);
}

static int read_file_to_buf(const char *path, char *buf, size_t sz)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, sz - 1, f);
    buf[n] = '\0';
    fclose(f);
    return 0;
}

static void sync_logs_from_temp(const char *temp_path, const char *log_path)
{
    if (!dir_exists(temp_path)) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","sync_logs: %s not found!!!\n", temp_path ? temp_path : "<null>");
        return;
    }
    if (log_path && temp_path && strcmp(temp_path, log_path) == 0) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","sync_logs: Sync Not needed, Same log folder\n");
        return;
    }

    DIR *d = opendir(temp_path);
    if (!d) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","sync_logs: failed to open %s\n", temp_path);
        return;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Find and move the logs from %s to %s\n", temp_path, log_path ? log_path : "<null>");
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_type == DT_REG) {
            const char *name = de->d_name;
            if (!(ends_with(name, ".txt") || ends_with(name, ".log"))) continue;
            char src[512], dst[512];
            snprintf(src, sizeof(src), "%s/%s", temp_path, name);
            snprintf(dst, sizeof(dst), "%s/%s", log_path, name);

            FILE *fs = fopen(src, "r");
            FILE *fd = fopen(dst, "a");
            if (!fs || !fd) {
                if (fs) fclose(fs);
                if (fd) fclose(fd);
                RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","sync_logs: failed to open src/dst for %s\n", name);
                continue;
            }
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
                if (fwrite(buf, 1, n, fd) != n) break;
            }
            fclose(fs);
            fclose(fd);

            /* truncate src */
            FILE *ft = fopen(src, "w");
            if (ft) fclose(ft);
        }
    }
    closedir(d);
}

void perform_housekeeping(void)
{
    /* Signal telemetry2_0 and parodus */
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Signal telemetry2_0 to send out any pending messages before reboot\n");
    v_secure_system("killall -s SIGUSR1 telemetry2_0");
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Properly shutdown parodus by sending SIGUSR1 kill signal\n");
    v_secure_system("killall -s SIGUSR1 parodus");

    /* Conditional RDM cleanup after image upgrade */
    if (file_exists("/etc/rdm/rdm-manifest.xml")) {
        char cdl[256] = {0}, prev[256] = {0};
        if (read_file_to_buf("/opt/cdl_flashed_file_name", cdl, sizeof(cdl)) == 0 &&
            read_file_to_buf("/tmp/currently_running_image_name", prev, sizeof(prev)) == 0) {
            if (strstr(cdl, prev) == NULL) {
                if (dir_exists("/media/apps")) {
                    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Removing the RDM Apps content from Secondary Storage before Reboot (After Image Upgrade)\n");
                    v_secure_system("sh -c 'cd /media/apps && for d in */; do rm -rf \"$d\"; done'");
                }
            }
        }
    }

    /* Sync transient logs */
    const char *persistent_path = getenv("PERSISTENT_PATH");
    const char *temp_log_path = getenv("TEMP_LOG_PATH");
    const char *log_path = getenv("LOG_PATH");

    if (persistent_path && !file_exists("/opt/persistent/.lightsleepKillSwitchEnable")) {
        if (temp_log_path && log_path) {
            sync_logs_from_temp(temp_log_path, log_path);
        }
        char systime_src[512];
        snprintf(systime_src, sizeof(systime_src), "%s/.systime", temp_log_path ? temp_log_path : "");
        if (file_exists(systime_src)) {
            char systime_dst[512];
            snprintf(systime_dst, sizeof(systime_dst), "%s/.systime", persistent_path);
            FILE *fs = fopen(systime_src, "rb");
            FILE *fd = fopen(systime_dst, "wb");
            if (fs && fd) {
                char buf[4096]; size_t n;
                while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
                    if (fwrite(buf, 1, n, fd) != n) break;
                }
            }
            if (fs) fclose(fs);
            if (fd) fclose(fd);
        }
    }

    /* Bluetooth services stop */
    const char *bt_enabled = getenv("BLUETOOTH_ENABLED");
    if (bt_enabled && strcmp(bt_enabled, "true") == 0) {
        const char *services[] = {"sky-bluetoothrcu", "btmgr", "bluetooth", "bt-hciuart", "btmac-preset", "bt", "syslog-ng"};
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Shutting down the bluetooth and syslog-ng services gracefully");
        for (size_t i = 0; i < sizeof(services)/sizeof(services[0]); ++i) {
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "systemctl --quiet is-active %s", services[i]);
            int active = system(cmd);
            if (active == 0) {
                snprintf(cmd, sizeof(cmd), "systemctl stop %s", services[i]);
                int rc = system(cmd);
                if (rc == 0) {
                  RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","%s service stopped successfully\n", services[i]);
                }
                else {
                  RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Failed to stop %s service\n", services[i]);
                }
            } else {
                RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","%s service is not active\n", services[i]);
            }
        }
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Start the sync");
    (void)sync();
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","End of the sync");
}

int pidfile_write_and_guard(void)
{
    /* If pid file exists, check running instance */
    if (file_exists(PID_FILE)) {
        FILE *f = fopen(PID_FILE, "r");
        if (f) {
            char buf[32] = {0};
            if (fgets(buf, sizeof(buf), f)) {
                pid_t pid = (pid_t)atoi(buf);
                char procpath[64];
                snprintf(procpath, sizeof(procpath), "/proc/%d/cmdline", (int)pid);
                if (file_exists(procpath)) {
                    FILE *pc = fopen(procpath, "r");
                    if (pc) {
                        char cmd[256] = {0};
                        fread(cmd, 1, sizeof(cmd)-1, pc);
                        fclose(pc);
                        if (strstr(cmd, "rebootmanager")) {
                            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","An instance of %s with pid %d is already running..\n", "rebootmanager", (int)pid);
                            RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Exiting Binary\n");
                            fclose(f);
                            return -1;
                        }
                    }
                }
            }
            fclose(f);
        }
    }
    /* write our pid */
    FILE *wf = fopen(PID_FILE, "w");
    if (wf) {
        fprintf(wf, "%d", (int)getpid());
        fclose(wf);
    }
    return 0;
}

void cleanup_pidfile(void)
{
    unlink(PID_FILE);
}

bool CheckRebootEnableFlag(){
    bool ret=false;
    RFC_ParamData_t param;
    WDMP_STATUS wdmpStatus = getRFCParameter(NULL,RDK_REBOOTSTOP_ENABLE, &param);
    if (wdmpStatus == WDMP_SUCCESS || wdmpStatus == WDMP_ERR_DEFAULT_VALUE){
        if( param.type == WDMP_BOOLEAN ){
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"[%s:%d]:getRFCParameter() name=%s,type=%d,value=%s\n", __FUNCTION__, __LINE__, param.name, param.type, param.value);
            if(strncasecmp(param.value,"true",4) == 0 ){
                ret=true;
            }
        }
    }
    RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"RebootStopEnable = %s, call value = %d\n",(ret == true)?"true":"false", wdmpStatus);
    return ret;
}

bool isMmgbleNotifyEnabled(void)
{
    bool status = false;
    int ret = -1;
    RFC_ParamData_t param;

    WDMP_STATUS wdmpStatus = getRFCParameter(NULL,X_RDK_RFC_MANGEBLENOTIFICATION_ENABLE, &param);
    if (wdmpStatus == WDMP_SUCCESS || wdmpStatus == WDMP_ERR_DEFAULT_VALUE){
        if( param.type == WDMP_BOOLEAN ){
            RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"[%s:%d]:getRFCParameter() name=%s,type=%d,value=%s\n", __FUNCTION__, __LINE__, param.name, param.type, param.value);
            if(strncasecmp(param.value,"true",4) == 0 ){
                ret=true;
            }
        }
    }
    RDK_LOG(RDK_LOG_DEBUG,LOG.RDK.REBOOTINFO,"ManageNotifyEnable = %s, call value = %d\n",(ret == true)?"true":"false", wdmpStatus);
    return ret;
}
