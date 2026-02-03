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
#include "rbus_interface.h"
#include "rdk_logger.h"
#include <fcntl.h>
#include <errno.h>

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

/* Send a signal to all processes whose /proc/<pid>/comm matches name */
static int send_signalCleanup(const char *name, int sig)
{
    if (!name || !*name) return 0;
    DIR *proc = opendir("/proc");
    if (!proc) return 0;
    int count = 0;
    struct dirent *de;
    while ((de = readdir(proc)) != NULL) {
        if (de->d_type != DT_DIR) continue;
        const char *dname = de->d_name;
        if (dname[0] < '0' || dname[0] > '9') continue; /* pid dirs start with digit */
        char commpath[64];
        snprintf(commpath, sizeof(commpath), "/proc/%s/comm", dname);
        FILE *cf = fopen(commpath, "r");
        if (!cf) continue;
        char comm[256] = {0};
        if (fgets(comm, sizeof(comm), cf)) {
            size_t len = strlen(comm);
            if (len > 0 && (comm[len-1] == '\n' || comm[len-1] == '\r')) comm[--len] = '\0';
            if (strcmp(comm, name) == 0) {
                pid_t pid = (pid_t)atoi(dname);
                if (pid > 1) {
                    if (kill(pid, sig) == 0) count++;
                }
            }
        }
        fclose(cf);
    }
    closedir(proc);
    return count;
}

/* Recursively remove a directory tree */
static int remove_dir(const char *path)
{
    if (!path) return -1;
    struct stat st;
    if (lstat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return -1;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            char child[1024];
            snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
            struct stat cst;
            if (lstat(child, &cst) != 0) continue;
            if (S_ISDIR(cst.st_mode)) {
                (void)remove_dir(child);
            } else {
                (void)unlink(child);
            }
        }
        closedir(d);
        return rmdir(path);
    } else {
        return unlink(path);
    }
}

static int clear_Subdirectory(const char *root)
{
    if (!root) return -1;
    DIR *d = opendir(root);
    if (!d) return -1;
    int rc = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        char child[1024];
        snprintf(child, sizeof(child), "%s/%s", root, de->d_name);
        struct stat st;
        if (lstat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (remove_dir(child) != 0) rc = -1;
        }
    }
    closedir(d);
    return rc;
}

static void sync_logs_from_temp(const char *temp_path, const char *log_path)
{
    int copy_ok = 1;
    char src[512], dst[512];
    char buf[4096];
    size_t n;
    struct dirent *de;

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
    while ((de = readdir(d)) != NULL) {
        if (de->d_type == DT_REG) {
            const char *name = de->d_name;
            if (!(ends_with(name, ".txt") || ends_with(name, ".log"))) continue;
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
            while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
                if (fwrite(buf, 1, n, fd) != n) {
                    copy_ok = 0;
                    break;
                }
            }
            fclose(fs);
            fclose(fd);

	    if (!copy_ok) {
                RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","sync_logs: write failed for %s, skipping truncate\n", name);
                continue;
            }
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
    send_signalCleanup("telemetry2_0", SIGUSR1);
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Properly shutdown parodus by sending SIGUSR1 kill signal\n");
    send_signalCleanup("parodus", SIGUSR1);

    /* Conditional RDM cleanup after image upgrade */
    if (file_exists("/etc/rdm/rdm-manifest.xml")) {
        char cdl[256] = {0}, prev[256] = {0};
        if (read_file_to_buf("/opt/cdl_flashed_file_name", cdl, sizeof(cdl)) == 0 &&
            read_file_to_buf("/tmp/currently_running_image_name", prev, sizeof(prev)) == 0) {
            if (strstr(cdl, prev) == NULL) {
                if (dir_exists("/media/apps")) {
                    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Removing the RDM Apps content from Secondary Storage before Reboot (After Image Upgrade)\n");
                    if (clear_Subdirectory("/media/apps") != 0) {
                        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Failed to remove some entries under /media/apps\n");
                    }
                }
            }
        }
    }

    /* Device-specific maintenance scripts */
    const char *device_name = getenv("DEVICE_NAME");
    if (device_name && (
            strcmp(device_name, "XiOne") == 0 ||
            strcmp(device_name, "XiOne-SCB") == 0)) {
        if (file_exists("/lib/rdk/eMMC_Upgrade.sh")) {
            RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Upgrade eMMC FW if required");
            v_secure_system("sh /lib/rdk/eMMC_Upgrade.sh");
        }
    }
    if (file_exists("/lib/rdk/aps4_reset.sh")) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Executing /lib/rdk/aps4_reset.sh");
        v_secure_system("sh /lib/rdk/aps4_reset.sh");
    }
    if (file_exists("/lib/rdk/update_www-backup.sh")) {
        RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Executing /lib/rdk/update_www-backup.sh");
        v_secure_system("sh /lib/rdk/update_www-backup.sh");
    }

    /* Sync transient logs */
    const char *persistent_path = getenv("PERSISTENT_PATH");
    const char *temp_log_path = getenv("TEMP_LOG_PATH");
    const char *log_path = getenv("LOG_PATH");

    if (persistent_path && !file_exists("/opt/persistent/.lightsleepKillSwitchEnable")) {
        if (temp_log_path && log_path) {
            sync_logs_from_temp(temp_log_path, log_path);
        }

	if (temp_log_path) {
            char systime_src[512];
            snprintf(systime_src, sizeof(systime_src), "%s/.systime", temp_log_path);
            if (file_exists(systime_src)) {
                char systime_dst[512];
                snprintf(systime_dst, sizeof(systime_dst), "%s/.systime", persistent_path);
                FILE *fs = fopen(systime_src, "rb");
                FILE *fd = fopen(systime_dst, "wb");
                if (!fs || !fd) {
                    if (fs) fclose(fs);
                    if (fd) fclose(fd);
                    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","systime copy: failed to open src/dst (%s -> %s)\n", systime_src, systime_dst);
                } else {
                    char buf[4096]; size_t n; int copy_ok = 1;
                    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) {
                        if (fwrite(buf, 1, n, fd) != n) { copy_ok = 0; break; }
                    }
                    fclose(fs);
                    fclose(fd);
                    if (!copy_ok) {
                        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","systime copy: write failed (%s -> %s)\n", systime_src, systime_dst);
                    }
                }
            }
        }
    }

    /* Bluetooth services stop */
    const char *bt_enabled = getenv("BLUETOOTH_ENABLED");
    if (bt_enabled && strcmp(bt_enabled, "true") == 0) {
        const char *services[] = {"sky-bluetoothrcu", "btmgr", "bluetooth", "bt-hciuart", "btmac-preset", "bt", "syslog-ng"};
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Shutting down the bluetooth and syslog-ng services gracefully");
        for (size_t i = 0; i < sizeof(services)/sizeof(services[0]); ++i) {
            int active = v_secure_system("systemctl --quiet is-active %s", services[i]);
            if (active == 0) {
                int rc = v_secure_system("systemctl stop %s", services[i]);
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
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Start the sync");
    (void)sync();
    usleep(200000);
    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","End of the sync");
}

int pidfile_write_and_guard(void)
{
    /* Create PID file atomically to avoid races */
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd >= 0) {
        char out[32];
        int len = snprintf(out, sizeof(out), "%d", (int)getpid());
        if (len > 0) {
            (void)write(fd, out, (size_t)len);
        }
        close(fd);
        return 0;
    }
    if (errno == EEXIST) {
        /* PID file exists: acquire exclusive advisory lock, re-check, then overwrite safely */
        int lfd = open(PID_FILE, O_RDWR);
        if (lfd < 0) {
            if (errno == ENOENT) {
                int cfd = open(PID_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
                if (cfd >= 0) {
                    char out2[32];
                    int len2 = snprintf(out2, sizeof(out2), "%d", (int)getpid());
                    if (len2 > 0) {
                        (void)write(cfd, out2, (size_t)len2);
                    }
                    close(cfd);
                    return 0;
                }
            }
            return -1;
        }
        struct flock lock;
        memset(&lock, 0, sizeof(lock));
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0; /* whole file */
        if (fcntl(lfd, F_SETLK, &lock) == -1) {
            close(lfd);
            return -1; /* another instance holds the lock */
        }
        FILE *f = fdopen(lfd, "r+");
        if (!f) {
            close(lfd);
            return -1;
        }
        char buf[32] = {0};
        if (fgets(buf, sizeof(buf), f)) {
            pid_t pid = (pid_t)atoi(buf);
            char procpath[64];
            snprintf(procpath, sizeof(procpath), "/proc/%d/cmdline", (int)pid);
            FILE *pc = fopen(procpath, "r");
            if (pc) {
                char cmd[256] = {0};
                size_t nread = fread(cmd, 1, sizeof(cmd)-1, pc);
                if (nread < sizeof(cmd)) cmd[nread] = '\0'; else cmd[sizeof(cmd)-1] = '\0';
                fclose(pc);
                if (strstr(cmd, "rebootnow")) {
                    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","An instance of %s with pid %d is already running..\n", "rebootnow", (int)pid);
                    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Exiting Binary\n");
                    fclose(f); /* releases lock via close(lfd) */
                    return -1;
                }
            }
        }
        /* Not running or unable to read cmdline: take over by overwriting under lock */
        (void)ftruncate(lfd, 0);
        rewind(f);
        fprintf(f, "%d", (int)getpid());
        fflush(f);
        fclose(f); /* also closes lfd and releases lock */
        return 0;
    }
    /* Unexpected error creating PID file */
    return -1;
}

void cleanup_pidfile(void)
{
    unlink(PID_FILE);
}

