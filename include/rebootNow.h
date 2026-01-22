#ifndef REBOOTMANAGER_HOUSEKEEPING_H
#define REBOOTMANAGER_HOUSEKEEPING_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared PID file path */
#define PID_FILE "/tmp/.rebootNow.pid"

#ifdef T2_EVENT_ENABLED
#include <telemetry_busmessage_sender.h>
#endif

/* Provided by main.c */
void rebootLogf(const char *fmt, ...);

/* Housekeeping operations prior to reboot */
void perform_housekeeping(void);

/* Single-instance guard via PID file */
int pidfile_write_and_guard(void);
void cleanup_pidfile(void);

int handle_cyclic_reboot(const char *source,
                         const char *rebootReason,
                         const char *customReason,
                         const char *otherReason);
void timestamp_update(char *buf, size_t sz);
int append_line_to_file(const char *path, const char *line);
int run_cmd_capture(const char *cmd, char *out, size_t outsz);
void t2CountNotify(char *marker, int val);
void t2ValNotify(char *marker, char *val);
#ifdef __cplusplus
}
#endif

#endif /* REBOOTMANAGER_HOUSEKEEPING_H */
