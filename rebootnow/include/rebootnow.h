#ifndef REBOOTNOW_H
#define REBOOTNOW_H
#include <stddef.h>

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
#ifdef __cplusplus
}
#endif

#endif /* REBOOTNOW_H */
