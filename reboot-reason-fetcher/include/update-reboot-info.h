#ifndef UPDATE_REBOOT_INFO_H
#define UPDATE_REBOOT_INFO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* RDK Common Utilities */
#ifdef RDK_LOGGER_ENABLED
#include "rdk_debug.h"
#endif

#ifdef T2_EVENT_ENABLED
#include <telemetry_busmessage_sender.h>
#endif

/*===========================================================================
 * CONSTANTS AND MACROS
 *===========================================================================*/

/* Log module for reboot reason processing */
#define LOG_MODULE "LOG.RDK.REBOOT"

/* Path constants */
#define DEVICE_PROPERTIES_PATH "/etc/device.properties"
#define REBOOT_INFO_DIR "/opt/secure/reboot"
#define REBOOT_INFO_FILE "/opt/secure/reboot/reboot.info"
#define PREVIOUS_REBOOT_INFO_FILE "/opt/secure/reboot/previousreboot.info"
#define PREVIOUS_HARD_REBOOT_INFO_FILE "/opt/secure/reboot/hardpower.info"
#define PARODUS_REBOOT_INFO_FILE "/opt/secure/reboot/parodusreboot.info"
#define PREVIOUS_PARODUSREBOOT_INFO_FILE "/opt/secure/reboot/previousparodusreboot.info"
#define KEYPRESS_INFO_FILE "/opt/secure/reboot/keypress.info"
#define PREVIOUS_KEYPRESS_INFO_FILE "/opt/secure/reboot/previouskeypress.info"
#define PARODUS_LOG "/opt/logs/parodus.log"
#define REBOOT_INFO_LOG_FILE "/opt/logs/rebootInfo.log"

#define BRCM_REBOOT_FILE "/proc/brcm/previous_reboot_reason"
#define RTK_REBOOT_FILE "/proc/cmdline"
#define AMLOGIC_SYSFS_FILE "/sys/devices/platform/aml_pm/reset_reason"
#define MTK_SYSFS_FILE "/sys/mtk_pm/boot_reason"
#define CMDLINE_PATH "/proc/cmdline"
#define WAKEUP_REASON_KEY "wakeupreason="
#define AMLOGIC_REBOOT_REASON_PATH "/sys/class/aml_reboot/reboot_reason"
#define STT_FLAG "/tmp/stt_received"
#define PSTORE_DIR "/sys/fs/pstore"

/* Lock directory */
#define LOCK_DIR "/tmp/rebootInfo.lock"

/* Buffer sizes */
#define MAX_BUFFER_SIZE 512
#define MAX_PATH_LENGTH 256
#define MAX_REASON_LENGTH 128
#define MAX_TIMESTAMP_LENGTH 64

/* Return codes */
#define SUCCESS 0
#define ERROR_GENERAL -1
#define ERROR_LOCK_FAILED -2
#define ERROR_FILE_NOT_FOUND -3
#define ERROR_PARSE_FAILED -4
#define FAILURE            -5

/* Logging macros - use RDK_LOG when available */
#ifdef RDK_LOGGER_ENABLED
extern int g_rdk_logger_enabled;
#endif  //RDK_LOGGER_ENABLED

/*===========================================================================
 * TYPE DEFINITIONS
 *===========================================================================*/

/* Reboot reason structure */
typedef struct {
    char timestamp[MAX_TIMESTAMP_LENGTH];
    char source[MAX_REASON_LENGTH];
    char reason[MAX_REASON_LENGTH];
    char customReason[MAX_REASON_LENGTH];
    char otherReason[MAX_REASON_LENGTH];
} RebootInfo;
/* Environment context structure */
typedef struct {
    char soc[64];
    char rdkProfile[64];
    char buildType[64];
    char device_type[64];
    bool rebootInfoSttSupport;
} EnvContext;

/* Hardware reason structure */
typedef struct {
    char rawReason[MAX_REASON_LENGTH];
    char mappedReason[MAX_REASON_LENGTH];
} HardwareReason;

/* Panic detection structure */
typedef struct {
    bool detected;
    char panicType[MAX_REASON_LENGTH];
    char details[MAX_BUFFER_SIZE];
} PanicInfo;
/* Firmware failure structure */
typedef struct {
    bool detected;
    bool maxRebootDetected;
    bool ecmCrashDetected;
    char details[MAX_REASON_LENGTH];
    char initiator[MAX_REASON_LENGTH];
} FirmwareFailure;

int acquire_lock(const char *lockDir);
int release_lock(const char *lockDir);
int parse_device_properties(EnvContext *ctx);
void free_env_context(EnvContext *ctx);
int get_hardware_reason(const EnvContext *ctx, HardwareReason *hwReason, RebootInfo *info);
int read_brcm_previous_reboot_reason(HardwareReason *hw);
int detect_kernel_panic(const EnvContext *ctx, PanicInfo *panicInfo);
int check_firmware_failure(const EnvContext *ctx, FirmwareFailure *fwFailure);
int classify_reboot_reason(RebootInfo *info,
                           const EnvContext *ctx,
                           const HardwareReason *hwReason,
                           const PanicInfo *panicInfo,
                           const FirmwareFailure *fwFailure);
bool is_app_triggered(const char *reason);
bool is_ops_triggered(const char *reason);
bool is_maintenance_triggered(const char *reason);
int write_reboot_info(const char *path, const RebootInfo *info);
int write_hardpower(const char *path, const char *timestamp);
int append_kernel_reason(const EnvContext *ctx, const RebootInfo *info);
int update_parodus_log(const RebootInfo *info);
int handle_parodus_reboot_file(const RebootInfo *info, const char *destPath);
int copy_keypress_info(const char *srcPath, const char *destPath);
void t2CountNotify(char *marker, int val);
void t2ValNotify(char *marker, char *val);
int parse_legacy_log(const char *logPath, RebootInfo *info);
int update_reboot_info(const EnvContext *ctx);
int find_previous_reboot_log(char *out_path, size_t len);
#endif /* UPDATE_REBOOT_INFO_H */
