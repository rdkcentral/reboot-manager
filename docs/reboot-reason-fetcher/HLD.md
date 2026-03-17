# High-Level Design (HLD): Reboot Reason Processing (C Migration)

## 1. Architecture Overview
The migrated component will be a standalone executable (e.g., `reboot_reason_updater`) invoked similarly to the original script (systemd unit or another supervising script). It implements a layered architecture:

```
+---------------------------------------------------------+
| Application Layer: main()                               |
|  - Orchestrates workflow                                |
+----------------------+----------------------------------+
| Services Layer       |                                  |
|  - Classification    | Firmware Failure Analyzer        |
|  - Panic Detector    | Hardware Reason Resolver         |
|  - Log Parser        | Parodus Logger                   |
+----------------------+----------------------------------+
| HAL / Platform Abstraction                              |
|  - Broadcom hw register reader                          |
|  - Realtek wakeup parser                                |
|  - Amlogic reset code interpreter                       |
|  - Generic kernel/pstore reader                         |
+---------------------------------------------------------+
| Utility Layer                                           |
|  - File IO wrappers (bounded reads)                     |
|  - String normalization                                 |
|  - JSON writer (minimal)                                |
|  - Lock manager (directory lock)                        |
|  - Telemetry adapter                                    |
+---------------------------------------------------------+
```

## 2. Module Breakdown

| Module | Responsibility |
|--------|---------------|
| `main.c` | Entry point, orchestrates steps, error fallback, lock lifecycle. |
| `lock.c` / `lock.h` | Directory-based mutual exclusion and timeout. |
| `env.c` | Load device properties into a struct. |
| `hw_brcm.c` | Parse `/proc/brcm/previous_reboot_reason`. |
| `hw_rtk.c` | Extract `wakeupreason=` from `/proc/cmdline`. |
| `hw_amlogic.c` | Map numeric reset code to structured info. |
| `panic.c` | Kernel panic / oops detection via messages/pstore. |
| `firmware_fail.c` | Detect max reboot / ECM crash conditions. |
| `classification.c` | Map initiator to category; apply overrides. |
| `log_parser.c` | Parse legacy `/opt/logs/rebootInfo.log`. |
| `json_writer.c` | Emit consistent JSON with stable ordering. |
| `telemetry.c` | Abstracted counter updates (weak symbol). |
| `parodus_log.c` | Append to Parodus log with consistent timestamp format. |
| `keypress.c` | Copy keypress metadata. |

## 3. Data Flow Description

1. `main()` acquires lock.
2. Loads environment (device name, SOC, profile).
3. Validates flags; may early exit.
4. Ensures directory existence.
5. Checks for new reboot info file:
   - If found: move & log → skip derivation.
   - Else:
     - Parse previous textual log for baseline fields.
     - If missing initiator:
       - Panic detector invoked. If panic → classify.
       - Else call platform-specific hardware module.
     - If initiator present → classification sets applied.
     - Firmware failure override (non-TV).
6. Build `RebootInfo` struct.
7. Write JSON to `previousreboot.info`, update `hardpower.info` if appropriate.
8. Mirror reason to kernel messages (platform-specific conditions).
9. Copy keypress file if exists.
10. Log to Parodus.
11. Create invocation flag.
12. Release lock and exit.

## 4. Control Flow (Conceptual)
See flowcharts file for detailed mermaid representation.
Key decision points:
- Flag presence gating.
- New info vs legacy derivation.
- Kernel panic detection precedence.
- Firmware failure override precedence.
- Hardware path selection based on SOC / device profile.

## 5. Key Algorithms & Data Structures

### Data Structures
```
typedef struct {
    char timestamp[64];
    char source[64];
    char reason[64];
    char custom_reason[128];
    char other_reason[256];
} RebootInfo;

typedef enum {
    CAT_APP_TRIGGERED,
    CAT_OPS_TRIGGERED,
    CAT_MAINTENANCE_REBOOT,
    CAT_FIRMWARE_FAILURE,
    CAT_KERNEL_PANIC,
    CAT_HARD_POWER,
    CAT_POWER_ON_RESET,
    CAT_UNKNOWN_RESET,
    CAT_WATCHDOG,
    CAT_FACTORY_RESET,
    CAT_UPDATE_BOOT,
    // ...
} ReasonCategory;
```

### Algorithms
1. Membership Test:
   - Arrays stored as sorted `const char*` lists; linear scan acceptable given small N (<100). Optionally use hashing if expanded.
2. Panic Detection:
   - Scan pstore or messages file for multiple signatures; short-circuit once found.
3. Hardware Mapping (Amlogic):
   - Switch-case over integer code (0–15); direct O(1) mapping.
4. Log Parsing:
   - Single pass; use bounded `fgets` lines; search prefixes; trims leading spaces.
5. Firmware Failure:
   - Scan specific log sources for sentinel strings; sets override flag.

## 6. Interfaces & Integration Points

| Interface | Function Signature | Notes |
|-----------|--------------------|-------|
| Hardware reason | `int hw_get_reason(DeviceContext*, HwReason* out)` | Returns normalized enum + raw tokens. |
| Panic check | `bool detect_kernel_panic(DeviceContext*, PanicInfo* out)` | Abstract file paths by profile. |
| Firmware failure | `bool check_firmware_failure(DeviceContext*, FwFailInfo* out)` | Returns true if override required. |
| Classification | `void classify(RebootInfo*, const DeviceContext*, const ParsedSources*)` | Sets `reason` and `custom_reason`. |
| JSON writer | `int write_reboot_info(const char* path, const RebootInfo* info)` | Atomic temp+rename. |
| Lock | `int acquire_lock(const char* path, int timeout_sec)` / `void release_lock(const char* path)` | Directory semantics. |
| Telemetry | `void telemetry_count(const char* key)` | Weak symbol / stub fallback. |

## 7. Error Handling Strategy
- Functions return status codes; `main` accumulates quality-of-result flags.
- Non-critical failures logged; classification falls back progressively.
- Atomic JSON writes: write to `path.tmp` then `rename()` to avoid partial files.
- Buffer overruns prevented by length-checked concatenations (`snprintf`).

## 8. Concurrency Considerations
- Single-process guarantee via lock directory.
- Avoid holding files open longer than necessary.
- No shared memory or threading required.

## 9. Portability Strategy
- Conditional compilation:
  - `#ifdef SOC_BRCM`, `#ifdef SOC_REALTEK`, `#ifdef SOC_AMLOGIC`.
- Provide generic stubs when platform path absent.
- Avoid GNU extensions; target C99.

## 10. Logging Approach
- Macro: `LOG(fmt, ...)` mapping to `fprintf(stderr, ...)` or syslog selectable at compile-time.
- Include timestamp only when writing to Parodus log; other logs mimic script style.

## 11. Security & Robustness
- Validate all file sizes: if >128 KB, limit scanning or skip with fallback reason.
- Reject non-printable characters from hardware reason tokens.
- Directory creation uses restrictive permissions (`0700`).
- JSON output escapes quotes minimally (expected tokens are alphanumeric / underscores).

## 12. Performance Considerations
- O(1) mapping for hardware codes.
- Single pass over each file.
- Avoid dynamic allocation—use stack/local static arrays.
- Optional compile-time feature flags to disable expensive scanning (e.g., full pstore enumeration).

## 13. Known Logical Fixes
- Adjust firmware failure condition to use logical AND (`if DEVICE != PLATCO && DEVICE != LLAMA`).
- Correct inconsistent variable usage in script Parodus log line referencing `$customReason`.

## 14. Extensibility
- Adding new hardware platforms: implement new `hw_<platform>.c`.
- Additional panic signatures: extend `panic_signatures[]`.
- Telemetry keys expand via config header.

## 15. High-Level Sequence (Summary)
1. Lock
2. Flags check
3. Directory ensure
4. New info file? → move → JSON finalize → (skip classification path)
5. Else derive fields
6. Firmware failure override
7. Write JSON(s)
8. Kernel log annotation
9. Parodus + keypress handling
10. Flag create
11. Unlock

## 16. Non-Goals
- Real-time monitoring of subsequent reboots.
- Telemetry batching.
- Multi-language bindings.

## 17. Assumptions
- Device properties file reliably accessible.
- Telemetry API optional.
- System clock reliable for UTC timestamp.

*End of HLD.*
