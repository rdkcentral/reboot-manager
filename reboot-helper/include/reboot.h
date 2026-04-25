#ifndef REBOOTNOW_H
#define REBOOTNOW_H
#include <stddef.h>
#include <stdbool.h>

/* rfcapi types — real header on target, stub when running unit tests */
#ifndef GTEST_ENABLE
#include "rfcapi.h"
#endif

/* Caller-ID for all rfcapi calls made by reboot-manager */
#define RFC_CALLER_ID "rebootmgr"

/* TR-181 parameter names */
#define RFC_MNG_NOTIFY           "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.ManageableNotification.Enable"
#define RFC_FW_REBOOT_NOTIFY     "Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.RPC.RebootPendingNotification"
#define RFC_REBOOTSTOP_DETECTION "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Detection"
#define RFC_REBOOTSTOP_DURATION  "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Duration"
#define RFC_REBOOTSTOP_ENABLE    "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.RebootStop.Enable"

/** Delay (minutes) written to RebootPendingNotification before a managed reboot. */
#define REBOOT_PENDING_NOTIFICATION_DELAY 10

#ifdef __cplusplus
extern "C" {
#endif

/* Shared PID file path */
#define PID_FILE "/tmp/.rebootNow.pid"

#ifdef T2_EVENT_ENABLED
#include <telemetry_busmessage_sender.h>
#endif


/* Housekeeping operations prior to reboot */
void cleanup_services(void);

/* Single-instance guard via PID file */
int pidfile_write_and_guard(void);
void cleanup_pidfile(void);

int handle_cyclic_reboot(const char *source,
                         const char *rebootReason,
                         const char *customReason,
                         const char *otherReason);
void timestamp_update(char *buf, size_t sz);
int write_rebootinfo_log(const char *path, const char *line);
void t2CountNotify(const char *marker, int val);
void t2ValNotify(const char *marker, const char *val);

/* TR-181 RFC parameter helpers */
bool rfc_get_string_param(const char *param_name, char *value_buf, size_t buf_size);
bool rfc_get_bool_param(const char *param_name, bool *value);
bool rfc_get_int_param(const char *param_name, int *value);
bool rfc_set_bool_param(const char *param_name, bool value);
bool rfc_set_int_param(const char *param_name, int value);

#ifdef __cplusplus
}
#endif

#endif /* REBOOTNOW_H */
