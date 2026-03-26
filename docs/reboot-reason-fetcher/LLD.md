# Low-Level Design (LLD): Reboot Reason Updater (C Migration)

## 1. Purpose
Define detailed structures, functions, pseudocode, and error handling for the C implementation of the `update_previous_reboot_info.sh` script.

---

## 2. Data Structures

```c
#define MAX_TS_LEN        64
#define MAX_SOURCE_LEN    64
#define MAX_REASON_LEN    64
#define MAX_CUSTOM_LEN    128
#define MAX_OTHER_LEN     256

typedef struct {
    char timestamp[MAX_TS_LEN];
    char source[MAX_SOURCE_LEN];
    char reason[MAX_REASON_LEN];
    char custom_reason[MAX_CUSTOM_LEN];
    char other_reason[MAX_OTHER_LEN];
} RebootInfo;

typedef enum {
    RB_OK = 0,
    RB_ERR_IO,
    RB_ERR_PARSE,
    RB_ERR_LOCK,
    RB_ERR_PLATFORM
} rb_status_t;

typedef struct {
    int is_platco;
    int is_llama;
    int is_tv_profile;
    int is_stb_type;
    char soc[16];          // "BRCM", "RTK", "REALTEK", "AMLOGIC", etc.
    char device_name[32];
    char device_type[32];
} DeviceContext;

typedef struct {
    int firmware_failure;  // 0/1
    char detail[128];
} FirmwareFailureInfo;

typedef struct {
    int panic_detected;
    char panic_signature[64];
} PanicInfo;

typedef struct {
    int has_hw_reason;
    char primary_reason[MAX_REASON_LEN];
    char custom_tokens[MAX_CUSTOM_LEN];
} HwReasonInfo;
```

---

## 3. Constants / Classification Sets

```c
static const char* APP_TRIGGERED[] = {
    "Servicemanager", "WarehouseReset", "TR69Agent", /*... truncated list ...*/
};
static const size_t APP_TRIGGERED_COUNT = sizeof(APP_TRIGGERED)/sizeof(APP_TRIGGERED[0]);

static const char* OPS_TRIGGERED[] = {
    "ScheduledReboot", "FactoryReset", "UpgradeReboot_restore", /*...*/
};
static const size_t OPS_TRIGGERED_COUNT = sizeof(OPS_TRIGGERED)/sizeof(OPS_TRIGGERED[0]);

static const char* MAINT_TRIGGERED[] = {
    "AutoReboot.sh", "PwrMgr"
};
static const size_t MAINT_TRIGGERED_COUNT = sizeof(MAINT_TRIGGERED)/sizeof(MAINT_TRIGGERED[0]);

static const char* PANIC_SIGNATURES[] = {
    "PREVIOUS_KERNEL_OOPS_DUMP",
    "Kernel panic - not syncing",
    "Oops - undefined instruction",
    "Oops - bad syscall",
    "branch through zero",
    "unknown data abort code",
    "Illegal memory access"
};
```

---

## 4. File Paths (Constants)

```c
#define PATH_REBOOT_INFO              "/opt/secure/reboot/reboot.info"
#define PATH_PREV_REBOOT_INFO         "/opt/secure/reboot/previousreboot.info"
#define PATH_PARODUS_REBOOT_INFO      "/opt/secure/reboot/parodusreboot.info"
#define PATH_PREV_PARODUS_REBOOT_INFO "/opt/secure/reboot/previousparodusreboot.info"
#define PATH_HARDPOWER_INFO           "/opt/secure/reboot/hardpower.info"
#define PATH_KEYPRESS_INFO            "/opt/secure/reboot/keypress.info"
#define PATH_PREV_KEYPRESS_INFO       "/opt/secure/reboot/previouskeypress.info"
#define PATH_REBOOT_DIR               "/opt/secure/reboot"

#define PATH_REBOOT_LOG               "/opt/logs/rebootInfo.log"
#define PATH_KERNEL_LOG               "/opt/logs/messages.txt"
#define PATH_PARODUS_LOG              "/opt/logs/parodus.log"

#define PATH_FLAG_STT                 "/tmp/stt_received"
#define PATH_FLAG_REBOOT_INFO_UPDATED "/tmp/rebootInfo_Updated"
#define PATH_FLAG_INVOCATION          "/tmp/Update_rebootInfo_invoked"
#define PATH_LOCK_DIR                 "/tmp/rebootInfo.lock"

#define PATH_AMLOGIC_RESET_REASON     "/sys/devices/platform/aml_pm/reset_reason"
#define PATH_BRCM_PREV_REBOOT_REASON  "/proc/brcm/previous_reboot_reason"
#define PATH_RTK_CMDLINE              "/proc/cmdline"
```

---

## 5. Function Prototypes

```c
int acquire_lock(const char* path);          // returns RB_OK or RB_ERR_LOCK
void release_lock(const char* path);

int ensure_directory(const char* path);      // RB_OK or RB_ERR_IO
int file_exists(const char* path);
int read_small_file(const char* path, char* buf, size_t bufsize); // bounded read

int load_device_context(DeviceContext* ctx); // parse /etc/device.properties

int parse_previous_log(RebootInfo* info);    // fills existing fields if log present

int detect_kernel_panic(const DeviceContext* ctx, PanicInfo* pi);
int get_hardware_reason(const DeviceContext* ctx, HwReasonInfo* hw);

int classify_reason(RebootInfo* info,
                    const DeviceContext* ctx,
                    const HwReasonInfo* hw,
                    const PanicInfo* panic,
                    const FirmwareFailureInfo* fw);

int check_firmware_failure(const DeviceContext* ctx, FirmwareFailureInfo* fw);

int write_json_reboot_info(const char* path, const RebootInfo* info);
int write_json_hardpower(const char* path, const char* timestamp);

int append_kernel_reason_if_needed(const DeviceContext* ctx, const RebootInfo* info);
int copy_file(const char* src, const char* dst);

void telemetry_count(const char* key); // weak, may be stub

int lower_copy(char* dst, size_t dstlen, const char* src);
char* trim_leading(char* s);
```

---

## 6. Pseudocode (Main Flow)

```c
int main(void) {
    DeviceContext ctx = {0};
    RebootInfo info = {0};
    HwReasonInfo hw = {0};
    PanicInfo panic = {0};
    FirmwareFailureInfo fw = {0};
    int is_new_info = 0;
    int rc;

    rc = acquire_lock(PATH_LOCK_DIR);
    if (rc != RB_OK) return 1;

    rc = load_device_context(&ctx);

    // Flag gating (correct logic)
    if (!ctx.is_platco && !ctx.is_llama) {
        if (!file_exists(PATH_FLAG_STT) || !file_exists(PATH_FLAG_REBOOT_INFO_UPDATED)) {
            release_lock(PATH_LOCK_DIR);
            return 0;
        }
    } else {
        // For PLATCO/LLAMA allow first run without flags
        if (file_exists(PATH_FLAG_INVOCATION)) {
            if (!file_exists(PATH_FLAG_STT) || !file_exists(PATH_FLAG_REBOOT_INFO_UPDATED)) {
                release_lock(PATH_LOCK_DIR);
                return 0;
            }
        }
    }

    ensure_directory(PATH_REBOOT_DIR);

    // TV specific initial kernel annotation
    if (ctx.is_tv_profile) {
        if (!kernel_log_has_previous_reason()) {
            detect_kernel_panic(&ctx, &panic);
            if (panic.panic_detected) {
                append_kernel_panic_annotation();
            } else if (file_exists("/lib/rdk/get-reboot-reason.sh")) {
                run_external_script("/lib/rdk/get-reboot-reason.sh");
            }
        }
    }

    if (file_exists(PATH_REBOOT_INFO)) {
        // Move new file to previous
        rename(PATH_REBOOT_INFO, PATH_PREV_REBOOT_INFO);
        if (file_exists(PATH_PARODUS_REBOOT_INFO)) {
            append_parodus_log(PATH_PARODUS_REBOOT_INFO);
            rename(PATH_PARODUS_REBOOT_INFO, PATH_PREV_PARODUS_REBOOT_INFO);
        }
        if (file_exists(PATH_KEYPRESS_INFO)) {
            copy_file(PATH_KEYPRESS_INFO, PATH_PREV_KEYPRESS_INFO);
        }
        touch(PATH_FLAG_INVOCATION);
        release_lock(PATH_LOCK_DIR);
        return 0;
    }

    // Derivation path
    parse_previous_log(&info);
    if (info.source[0] == '\0') {
        // Need to derive
        strncpy(info.timestamp, current_utc(), sizeof(info.timestamp)-1);
        detect_kernel_panic(&ctx, &panic);
        if (panic.panic_detected) {
            strcpy(info.reason, "KERNEL_PANIC");
            strcpy(info.source, "Kernel");
            snprintf(info.custom_reason, sizeof(info.custom_reason),
                     "Hardware Register - KERNEL_PANIC");
            strcpy(info.other_reason, "Reboot due to Kernel Panic captured by Oops Dump");
        } else {
            get_hardware_reason(&ctx, &hw);
            if (hw.has_hw_reason) {
                map_hw_reason_to_info(&hw, &info); // sets reason/source/custom/other
            } else {
                // Fallback
                strcpy(info.reason, "HARD_POWER");
                strcpy(info.source, "Hard Power Reset");
                strcpy(info.custom_reason, "Hardware Register - NULL");
                strcpy(info.other_reason, "No information found");
            }
        }
    } else {
        // With source populated, reason classification awaits
        // timestamp possibly from log else fill now
        if (info.timestamp[0] == '\0')
            strncpy(info.timestamp, current_utc(), sizeof(info.timestamp)-1);
    }

    // Firmware failure override (non-TV)
    if (!ctx.is_tv_profile) {
        check_firmware_failure(&ctx, &fw);
    }

    classify_reason(&info, &ctx, &hw, &panic, &fw);

    write_json_reboot_info(PATH_PREV_REBOOT_INFO, &info);

    if (is_hardpower_update_required(info.reason, PATH_HARDPOWER_INFO)) {
        write_json_hardpower(PATH_HARDPOWER_INFO, info.timestamp);
    }

    append_kernel_reason_if_needed(&ctx, &info);

    if (file_exists(PATH_KEYPRESS_INFO)) {
        copy_file(PATH_KEYPRESS_INFO, PATH_PREV_KEYPRESS_INFO);
    }

    append_parodus_previous_info(&info);

    touch(PATH_FLAG_INVOCATION);

    release_lock(PATH_LOCK_DIR);
    return 0;
}
```

---

## 7. Detailed Logic Functions

### `detect_kernel_panic`
- For BRCM: scan `/opt/logs/messages.txt` for `PREVIOUS_KERNEL_OOPS_DUMP` then also look for `Kernel Oops` or `Kernel Panic`.
- For Realtek/TV: scan `/sys/fs/pstore/console-ramoops-0` for any signature in `PANIC_SIGNATURES`.
- If panic found: `panic_detected=1`, store matched signature.

### `get_hardware_reason`
- BRCM: read entire file, uppercase content; split by comma. First token primary.
- Realtek: find substring `wakeupreason=` in `/proc/cmdline`; extract next token; uppercase.
- Amlogic: read integer value (0–15), map using switch.
- Fallback: `has_hw_reason=0`.

### `classify_reason`
Order of precedence:
1. If `fw->firmware_failure == 1` → `info.reason="FIRMWARE_FAILURE"`.
2. Else if `panic.panic_detected` → already set.
3. Else if source present:
   - membership tests (APP → APP_TRIGGERED; OPS → OPS_TRIGGERED; MAINT → MAINTENANCE_REBOOT).
   - If customReason equals `MAINTENANCE_REBOOT` override to that category.
4. Else if hardware path set earlier (e.g., POWER_ON_RESET, WATCHDOG, etc.) keep mapping.
5. Else fallback already assigned.

### `write_json_reboot_info`
Atomic write:
```
temp = "/opt/secure/reboot/.previousreboot.info.tmp"
open temp (O_CREAT|O_WRONLY|O_TRUNC, 0600)
fprintf:
{
"timestamp":"%s",
"source":"%s",
"reason":"%s",
"customReason":"%s",
"otherReason":"%s"
}
fsync, close
rename(temp, PATH_PREV_REBOOT_INFO)
```

### `write_json_hardpower`
Similar atomic pattern.

---

## 8. Error Handling

| Function | Errors | Recovery |
|----------|--------|----------|
| `acquire_lock` | Directory exists → EBADF? | Retry up to timeout; log and exit if failure. |
| `read_small_file` | open/read failures | Return RB_ERR_IO; caller sets fallback reason. |
| `parse_previous_log` | Missing file | Leaves fields empty; triggers derivation path. |
| `detect_kernel_panic` | File missing | Return `panic_detected=0`. |
| `get_hardware_reason` | Parse errors | `has_hw_reason=0`; fallback. |
| `write_json_*` | Write/rename fail | Log error, but continue; system may lack reason next boot. |

---

## 9. Memory Management
- All buffers static stack allocations.
- Avoid `malloc`; maximum concatenations guarded by `snprintf`.
- File reading uses chunk reads (`fgets`) with fixed-size line buffer (e.g., 512 bytes).

---

## 10. Performance Considerations
- Linear scans only.
- Avoid scanning entire large logs: stop reading after required fields found.
- Panic detection stops after first signature match.

---

## 11. Security Considerations
- Validate numeric reset code range (Amlogic).
- Sanitize extracted strings (strip non-printable).
- Use restrictive permissions (0600) for created files.
- Prevent path injection by hardcoding paths (no variable path building from untrusted input).

---

## 12. Testing Matrix (Representative)

| Case | Setup | Expected Result |
|------|-------|-----------------|
| New reboot.info present | Create file | Moved & classification skipped. |
| No files, BRCM register present | Provide register text | Reason derived from first token. |
| Realtek wakeup | cmdline with wakeupreason | Reason lowercased appended to kernel log. |
| Kernel panic pstore | Provide panic signature | reason=KERNEL_PANIC. |
| Firmware failure max reboot | Provide sentinel string | reason=FIRMWARE_FAILURE override. |
| Missing register | Delete file | reason=HARD_POWER fallback. |
| PLATCO first invocation with no flags | Remove flags & invocation flag | Continues execution. |

---

## 13. Edge Case Handling Summary

| Edge | Handling |
|------|----------|
| Multiple tokens BRCM register | Store first as `reason`, all tokens in `custom_reason`. |
| Empty initiator + no panic + no hardware | Fallback to HARD_POWER/UNKNOWN. |
| JSON partial write failure | Log; may cause absence next run but does not crash. |
| Oversized lines | Truncated read; still attempt parse; fallback if prefix missing. |

---

## 14. Logging Macros

```c
#ifdef USE_SYSLOG
#define LOGI(fmt, ...) syslog(LOG_INFO, "[reboot] " fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) syslog(LOG_ERR,  "[reboot] " fmt, ##__VA_ARGS__)
#else
#define LOGI(fmt, ...) fprintf(stderr, "[INFO][reboot] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "[ERR ][reboot] " fmt "\n", ##__VA_ARGS__)
#endif
```

---

## 15. Telemetry Integration
Weak symbol pattern:

```c
__attribute__((weak))
void telemetry_count(const char* key) {
    (void)key;
    // No-op if telemetry library not linked.
}
```

---

## 16. Known Enhancements vs Script
- Correct logical condition for firmware failure gating.
- Atomic JSON writes prevent partial file states.
- Unified classification precedence logic documented.

---

## 17. Example JSON Output

```
{
"timestamp":"Mon Nov 17 07:20:58 UTC 2025",
"source":"Hard Power Reset",
"reason":"HARD_POWER",
"customReason":"Hardware Register - NULL",
"otherReason":"No information found"
}
```

---

## 18. Fallback Initialization
On startup with no data:
```
strcpy(info.timestamp, current_utc());
strcpy(info.source, "Hard Power Reset");
strcpy(info.reason, "HARD_POWER");
strcpy(info.custom_reason, "Hardware Register - NULL");
strcpy(info.other_reason, "No information found");
```

---

## 19. Sequence Integrity
Ensures lock released on all exit paths:
- Use `goto cleanup;` pattern or `atexit(release_lock_wrapper)`.

---

## 20. Future Extension Points
- JSON schema version field.
- Additional classification arrays loaded from config file.
- Telemetry structured events instead of simple counters.

*End of LLD.*
