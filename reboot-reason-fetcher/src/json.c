#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "update-reboot-info.h"
#include "rdk_logger.h"
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>

#define LOCK_RETRY_COUNT 10
#define LOCK_RETRY_DELAY_US 100000  // 100ms
static int g_lock_fd = -1;

int acquire_lock(const char *lockFile)
{
    if (!lockFile) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Lock file path is NULL \n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Attempting to acquire flock on: %s\n", lockFile);
    if (g_lock_fd < 0) {
        g_lock_fd = open(lockFile, O_CREAT | O_RDWR | O_CLOEXEC, 0644);
        if (g_lock_fd < 0) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open lock file %s: %s\n", lockFile, strerror(errno));
            return ERROR_LOCK_FAILED;
        }
    }
    // Blocking exclusive lock: wait until the lock is available
    if (flock(g_lock_fd, LOCK_EX) == 0) {
        RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Lock acquired successfully: %s\n", lockFile);
        return SUCCESS;
    }
    RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to acquire lock on %s: %s\n", lockFile, strerror(errno));
    return ERROR_LOCK_FAILED;
}
int release_lock(const char *lockFile)
{
    if (!lockFile) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Lock file path is NULL\n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Releasing lock: %s\n", lockFile);
    if (g_lock_fd >= 0) {
        if (flock(g_lock_fd, LOCK_UN) != 0) {
            RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to unlock %s: %s\n", lockFile, strerror(errno));
        }
        close(g_lock_fd);
        g_lock_fd = -1;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Lock released successfully: %s\n", lockFile);
    return SUCCESS;
}
int write_reboot_info(const char *path, const RebootInfo *info)
{
    FILE *fp = NULL;
    
    if (!path || !info) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for write_reboot_info\n");
        return ERROR_GENERAL;
    }
    RDK_LOG(RDK_LOG_DEBUG,"LOG.RDK.REBOOTINFO","Writing reboot info to file: %s\n", path);
    fp = fopen(path, "w");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open file %s: %s\n", path, strerror(errno));
        return ERROR_GENERAL;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "\"timestamp\":\"%s\",\n", info->timestamp);
    fprintf(fp, "\"source\":\"%s\",\n", info->source);
    fprintf(fp, "\"reason\":\"%s\",\n", info->reason);
    fprintf(fp, "\"customReason\":\"%s\",\n", info->customReason);
    fprintf(fp, "\"otherReason\":\"%s\"\n", info->otherReason);
    fprintf(fp, "}\n");
    fflush(fp);
    fclose(fp);

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Reboot info written successfully to: %s\n", path);
    return SUCCESS;
}
int write_hardpower(const char *path, const char *timestamp)
{
    FILE *fp = NULL;

    if (!path || !timestamp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Invalid parameters for write_hardpower \n");
        return ERROR_GENERAL;
    }

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Writing hardpower timestamp to file: %s\n", path);
    fp = fopen(path, "w");
    if (!fp) {
        RDK_LOG(RDK_LOG_ERROR,"LOG.RDK.REBOOTINFO","Failed to open file %s\n", path);
        return ERROR_GENERAL;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "\"lastHardPowerReset\":\"%s\"\n", timestamp);
    fprintf(fp, "}\n");
    fflush(fp);
    fclose(fp);

    RDK_LOG(RDK_LOG_INFO,"LOG.RDK.REBOOTINFO","Hardpower timestamp written successfully to: %s\n", path);
    return SUCCESS;
}
