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

| Module | Responsibility | Key Components | Status |
|--------|---------------|---|---|
| `rebootreason_main.c` | **Application Entry Point** - Orchestrates workflow, lock management, error handling | `main()`: Lock acquire → legacy log discovery → hardware/panic detection → classification → output persistence | ✅ Implemented |
| `bootup_reason_checker.c` | **Legacy Log Discovery & Parsing** - Ports reboot-checker.sh bootup logic to C | `find_previous_reboot_log()`: Timestamped dir → backup files → flat file; `parse_legacy_log()`: Single-pass field extraction; `resolve_hal_sys_reboot()`: Parses embedded initiator from "Triggered from" | ✅ **NEW - Migrated from Shell** |
| `log_parser.c` | **Device Properties** - Load platform/SOC configuration | `parse_device_properties()`: RDK API + fallback file parsing; Returns EnvContext struct | ✅ Implemented |
| `reboot_reason_classify.c` | **Reboot Reason Determination** - Multi-stage classification with hardware & software analysis | `classify_reboot_reason()`: Priority rules; `detect_kernel_panic()`: Scan messages.txt/pstore; `check_firmware_failure()`: Max reboot/ECM crash detection; `is_*_triggered()`: Category tests | ✅ Implemented |
| `json.c` | **Output Serialization** - Write structured reboot information | `write_reboot_info()`: Atomic temp+rename write; JSON formatting | ✅ Implemented |
| `parodus_log_update.c` | **Telemetry Integration** - Log to Parodus subsystem | `update_parodus_log()`: Append to parodus.log; `handle_parodus_reboot_file()`: JSON file creation | ✅ Implemented |


## 3. Data Flow Description

### Detailed Workflow Sequence

```
START: main()
    ├─ Initialize RDK Logger with configuration
    │
    ├─ LOCK ACQUISITION
    │  └─ Acquire directory lock (/tmp/rebootInfo.lock)
    │     └─ If failed → return ERROR_LOCK_FAILED, exit
    │
    ├─ ENVIRONMENT SETUP
    │  ├─ parse_device_properties()
    │  │  └─ Load: SOC, RDK_PROFILE, BUILD_TYPE, DEVICE_TYPE, STT_SUPPORT flag
    │  │
    │  └─ Ensure directory exists: /opt/secure/reboot
    │
    ├─ NEW REBOOT INFO CHECK
    │  ├─ If /opt/secure/reboot/reboot.info exists:
    │  │  ├─ Log current reboot.info content
    │  │  ├─ Rename to previousreboot.info (atomic)
    │  │  ├─ If parodusreboot.info exists → copy to previousparodusreboot.info
    │  │  └─ Skip to JSON write phase (has_reboot_info = true)
    │  │
    │  └─ Else: DERIVE FROM LEGACY SOURCES (described below)
    │
    ├─ LEGACY LOG DERIVATION (if no new reboot.info)
    │  │
    │  ├─ Step 1: FIND PREVIOUS REBOOT LOG
    │  │  ├─ find_previous_reboot_log() [bootup_reason_checker.c] - NEW MIGRATED FUNCTION
    │  │  │  ├─ Priority 1: Search $LOG_PATH/PreviousLogs/* for "last_reboot" marker
    │  │  │  │  └─ Use timestamped dir's rebootInfo.log if found
    │  │  │  ├─ Priority 2: Check backup files (bak1_rebootInfo.log, bak2_*, bak3_*)
    │  │  │  │  └─ For fast reboot case (before 8-min rotation)
    │  │  │  ├─ Priority 3: Use flat file ($LOG_PATH/PreviousLogs/rebootInfo.log)
    │  │  │  └─ Log path at each stage; return ERROR_FILE_NOT_FOUND if nothing found
    │  │  │
    │  │  └─ If no log found → set to empty, continue (will derive from hw/panic)
    │  │
    │  ├─ Step 2: PARSE LEGACY LOG
    │  │  ├─ parse_legacy_log() [bootup_reason_checker.c] - NEW MIGRATED FUNCTION
    │  │  │  ├─ Single-pass extraction (replaces 5 grep/awk shell passes)
    │  │  │  ├─ Field priority order:
    │  │  │  │  ├─ Look for PreviousRebootInitiatedBy: → info->source
    │  │  │  │  ├─ Look for PreviousRebootTime: → info->timestamp
    │  │  │  │  ├─ Look for PreviousCustomReason: → info->customReason
    │  │  │  │  ├─ Look for PreviousOtherReason: → info->otherReason
    │  │  │  │  └─ Early exit when all 4 found (10x faster than shell)
    │  │  │  │
    │  │  │  ├─ If Previous* fields not found, try raw fields:
    │  │  │  │  ├─ RebootInitiatedBy: → info->source
    │  │  │  │  ├─ RebootTime: → info->timestamp
    │  │  │  │  ├─ CustomReason: → info->customReason
    │  │  │  │  └─ OtherReason: → info->otherReason
    │  │  │  │
    │  │  │  ├─ Special Case: If RebootInitiatedBy = "HAL_SYS_Reboot"
    │  │  │  │  ├─ resolve_hal_sys_reboot() [bootup_reason_checker.c] - NEW MIGRATED FUNCTION
    │  │  │  │  │  └─ Parses RebootReason: "Triggered from <initiator> <reason> (parenthetical)"
    │  │  │  │  ├─ Set info->source = extracted initiator
    │  │  │  │  └─ Set info->otherReason = extracted reason
    │  │  │  │
    │  │  │  └─ Log found fields count; return ERROR if no fields extracted
    │  │  │
    │  │
    │  ├─ Step 3: FILL CURRENT TIMESTAMP (if not parsed)
    │  │  └─ If info->timestamp empty → get current UTC timestamp
    │  │
    │  ├─ Step 4: DETECT KERNEL PANIC
    │  │  ├─ detect_kernel_panic() implementation:
    │  │  │  ├─ Scan predefined panic signatures:
    │  │  │  │  - "Kernel panic - not syncing"
    │  │  │  │  - "Kernel Panic"
    │  │  │  │  - "Kernel Oops"
    │  │  │  │  - "Oops - undefined instruction"
    │  │  │  │  - etc.
    │  │  │  │
    │  │  │  ├─ Search in multiple log sources (priority):
    │  │  │  │  ├─ /opt/logs/messages.txt (kernel messages)
    │  │  │  │  └─ /sys/fs/pstore/console-ramoops-0 (persistent storage)
    │  │  │  │
    │  │  │  └─ Populate PanicInfo struct: detected, type, details
    │  │  │
    │  │  └─ If panic found → short-circuit, will be highest priority in classification
    │  │
    │  ├─ Step 5: CHECK FIRMWARE FAILURE
    │  │  ├─ check_firmware_failure() implementation:
    │  │  │  ├─ Detect MAX REBOOT condition:
    │  │  │  │  ├─ Scan /opt/logs/PreviousLogs/uimgr_log.txt
    │  │  │  │  ├─ Look for: "Box has rebooted 10 times"
    │  │  │  │  └─ Set FirmwareFailure.maxRebootDetected = true
    │  │  │  │
    │  │  │  ├─ Detect ECM CRASH:
    │  │  │  │  ├─ Scan /opt/logs/PreviousLogs/messages-ecm.txt
    │  │  │  │  ├─ Look for: "**** CRASH ****"
    │  │  │  │  └─ Set FirmwareFailure.ecmCrashDetected = true
    │  │  │  │
    │  │  │  └─ Populate FirmwareFailure struct: detected, reason details
    │  │  │
    │  │  └─ If detected → will be high priority in classification
    │  │
    │  ├─ Step 6: GET HARDWARE REASON (if no source/customReason yet)
    │  │  ├─ get_hardware_reason() - Extensible for multiple platforms:
    │  │  │  ├─ [Broadcom] Read /proc/brcm/previous_reboot_reason
    │  │  │  ├─ [Realtek] Parse wakeupreason= from /proc/cmdline
    │  │  │  ├─ [Amlogic] Map reset code from /sys/devices/platform/aml_pm/reset_reason
    │  │  │  ├─ [MTK] Read /sys/mtk_pm/boot_reason
    │  │  │  └─ Falls back to UNKNOWN if no platform match
    │  │  │
    │  │  └─ Populate HardwareReason struct
    │  │
    │  └─ Step 7: CLASSIFY REBOOT REASON
    │     ├─ classify_reboot_reason() applies priority rules:
    │     │  ├─ Priority 1: Kernel Panic? → CAT_KERNEL_PANIC
    │     │  ├─ Priority 2: Firmware Failure? → CAT_FIRMWARE_FAILURE
    │     │  ├─ Priority 3: Check source initiator:
    │     │  │  ├─ is_app_triggered(source)? → CAT_APP_TRIGGERED
    │     │  │  ├─ is_ops_triggered(source)? → CAT_OPS_TRIGGERED
    │     │  │  └─ is_maintenance_triggered(source)? → CAT_MAINTENANCE
    │     │  ├─ Priority 4: Hardware-based classification
    │     │  └─ Default: CAT_UNKNOWN_RESET
    │     │
    │     └─ Populate info->reason with classified category
    │
    ├─ OUTPUT & PERSISTENCE (if not has_reboot_info)
    │  │
    │  ├─ Write JSON to /opt/secure/reboot/previousreboot.info
    │  │  ├─ write_reboot_info() implementation:
    │  │  │  ├─ Write to /opt/secure/reboot/previousreboot.info.tmp first
    │  │  │  ├─ Atomic rename to final path (prevents corruption)
    │  │  │  └─ JSON format: {RebootReason, RebootInitiatedBy, RebootTime, ...}
    │  │  │
    │  │  └─ If reason matches power-related pattern → write_hardpower()
    │  │     └─ Create /opt/secure/reboot/hardpower.info with timestamp
    │  │
    │  ├─ Parodus Log Update
    │  │  ├─ update_parodus_log()
    │  │  │  └─ Append formatted entry to /opt/logs/parodus.log
    │  │  │
    │  │  └─ handle_parodus_reboot_file()
    │  │     └─ Create /opt/secure/reboot/previousparodusreboot.info
    │  │
    │  └─ Handle Keypress Info
    │     └─ copy_keypress_info()
    │        └─ If /opt/secure/reboot/keypress.info exists → copy to previous version
    │
    ├─ TELEMETRY
    │  ├─ T2 event markers (if T2_EVENT_ENABLED):
    │  │  ├─ t2_event_d("reboot_reason_classified", reason_code)
    │  │  └─ t2_event_s("reboot_initiator", source)
    │  │
    │  └─ RDK Logger debug/info messages at each step
    │
    ├─ LOCK RELEASE & CLEANUP
    │  ├─ release_lock() removes lock directory
    │  └─ If lock release fails → return ERROR_GENERAL (but continue exit)
    │
    └─ EXIT
       └─ Return status code (SUCCESS or error)
```

## 4. Control Flow (Conceptual)

### Decision Tree

```
Is reboot.info file present?
  ├─ YES → Move to previousreboot.info → Done
  └─ NO → Continue to legacy derivation
       │
       ├─ Find previous reboot log
       │  └─ Try timestamped → backups → flat
       │
       ├─ Parse extraction of fields
       │  ├─ PreviousRebootInitiatedBy → source
       │  ├─ PreviousRebootTime → timestamp
       │  ├─ PreviousCustomReason → customReason
       │  └─ PreviousOtherReason → otherReason
       │
       ├─ Special case: HAL_SYS_Reboot?
       │  └─ Parse "Triggered from <initiator> <reason>"
       │
       ├─ Detect kernel panic?
       │  ├─ YES → Classify as KERNEL_PANIC (Priority 1)
       │  └─ NO → Continue
       │
       ├─ Check firmware failure?
       │  ├─ MAX_REBOOT / ECM_CRASH detected?
       │  │  └─ YES → Classify as FIRMWARE_FAILURE (Priority 2)
       │  └─ NO → Continue
       │
       ├─ Classify based on source initiator (Priority 3)
       │  ├─ is_app_triggered(source)?
       │  ├─ is_ops_triggered(source)?
       │  ├─ is_maintenance_triggered(source)?
       │  └─ Hardware-based fallback
       │
       └─ Output: Write to previousreboot.info, parodus.log, hardpower.info
```

### Actual Implementation Sequence (from rebootreason_main.c)

The workflow in `main()`:

```c
1. Initialize logger and telemetry
2. acquire_lock(LOCK_DIR)                    // Prevent concurrent execution
3. parse_device_properties(&ctx)             // Load platform info
4. check_dir_exists(REBOOT_INFO_DIR)         // Ensure /opt/secure/reboot exists
5. get_current_timestamp()                   // Get current time

6. // Check for new reboot.info file
   if (access(REBOOT_INFO_FILE, F_OK) == 0) {
       log_reason(REBOOT_INFO_FILE)
       rename(REBOOT_INFO_FILE, PREVIOUS_REBOOT_INFO_FILE)
       handle_parodus_reboot_file()
       has_reboot_info = true
   } else {
       // Derive from legacy sources
       
       7. find_previous_reboot_log(prev_log_path)    // Timestamped/backup/flat
       8. parse_legacy_log(prev_log_path, &rebootInfo)
       
       9. detect_kernel_panic(&ctx, &panicInfo)
       10. check_firmware_failure(&ctx, &fwFailure)
       11. get_hardware_reason(&ctx, &hwReason, &rebootInfo)
       12. classify_reboot_reason(&rebootInfo, &ctx, &hwReason, &panicInfo, &fwFailure)
   }

13. write_reboot_info(PREVIOUS_REBOOT_INFO_FILE, &rebootInfo)
14. update_parodus_log(&rebootInfo)
15. handle_parodus_reboot_file(&rebootInfo, PREVIOUS_PARODUSREBOOT_INFO_FILE)
16. write_hardpower() (if power-related reason)
17. copy_keypress_info()
18. release_lock(LOCK_DIR)
19. return status
```

## 5. Key Algorithms & Data Structures

### Primary Data Structures (from update-reboot-info.h)

**RebootInfo** - Core output structure:
```c
typedef struct {
    char timestamp[MAX_TIMESTAMP_LENGTH];    // UTC timestamp (e.g., "Mon Apr 16 10:23:45 UTC 2026")
    char source[MAX_REASON_LENGTH];          // Reboot initiator (e.g., "PwrMgr", "FactoryReset")
    char reason[MAX_REASON_LENGTH];          // Classified reason (e.g., "OPS_TRIGGERED", "KERNEL_PANIC")
    char customReason[MAX_REASON_LENGTH];    // Custom reason details
    char otherReason[MAX_REASON_LENGTH];     // Additional reason information
} RebootInfo;
```

**EnvContext** - Platform/device context:
```c
typedef struct {
    char soc[64];                            // System-on-Chip (e.g., "BCM", "Realtek", "Amlogic")
    char rdkProfile[64];                     // RDK profile (e.g., "video", "mediaclient")
    char buildType[64];                      // Build type
    char device_type[64];                    // Device type
    bool rebootInfoSttSupport;               // STT (Speech-to-Text) support flag
} EnvContext;
```

**HardwareReason** - Hardware-specific reset info:
```c
typedef struct {
    char rawReason[MAX_REASON_LENGTH];       // Raw reason from hardware
    char mappedReason[MAX_REASON_LENGTH];    // Normalized/mapped reason
} HardwareReason;
```

**PanicInfo** - Kernel panic detection:
```c
typedef struct {
    bool detected;                           // Panic detected
    char panicType[MAX_REASON_LENGTH];       // Type: "Kernel panic", "Oops", etc.
    char details[MAX_BUFFER_SIZE];           // Full panic message for logs
} PanicInfo;
```

**FirmwareFailure** - Firmware/system failure detection:
```c
typedef struct {
    bool detected;                           // Failure detected
    bool maxRebootDetected;                  // Max reboot loop (10+ reboots)
    bool ecmCrashDetected;                   // ECM crash detected
    char details[MAX_REASON_LENGTH];         // Details
    char initiator[MAX_REASON_LENGTH];       // Failure initiator
} FirmwareFailure;
```

### Classification Categories

From `reboot_reason_classify.c`:

| Category | Initiators | Condition |
|----------|-----------|-----------|
| **KERNEL_PANIC** | Any | Panic signatures detected in logs (Priority 1) |
| **FIRMWARE_FAILURE** | Any | Max reboot count / ECM crash detected (Priority 2) |
| **APP_TRIGGERED** | Servicemanager, HtmlDiagnostics, StartTDK, PaceMFRLibrary, etc. | Source matches APP list |
| **OPS_TRIGGERED** | ScheduledReboot, FactoryReset, UpgradeReboot*, ImageUpgrade*, CDL, BRCM_Image_Validate, etc. | Source matches OPS list |
| **MAINTENANCE_REBOOT** | AutoReboot.sh, PwrMgr | Source matches MAINTENANCE list |
| **HARD_POWER / POWER_ON_RESET / UNKNOWN_RESET** | Hardware | Hardware-specific reset code |

### Key Algorithms

**Algorithm 1: Log Discovery (find_previous_reboot_log)**
```
Input: prev_logs_path
Output: candidate_log_path

1. Open $prev_logs_path directory
2. FOR each entry in directory:
     a. IF entry is subdirectory:
        - Check if "last_reboot" marker exists
        - IF exists: Check for rebootInfo.log in subdir
          - IF found: RETURN (highest priority)
3. IF marker dir not found:
     a. Check for bak1_rebootInfo.log, bak2, bak3 (in order)
        - First found: RETURN
     b. IF no backups: Check flat rebootInfo.log
        - IF found: RETURN
4. RETURN ERROR
```

**Algorithm 2: Legacy Log Parsing (parse_legacy_log)**
```
Input: log_file_path
Output: RebootInfo struct

found_fields = 0
1. OPEN log_file
2. FOR each line in file:
     a. IF line contains "PreviousRebootInitiatedBy:":
        - Extract value after colon → info->source
        - found_fields++
     b. ELSE IF line contains "PreviousRebootTime:":
        - Extract value → info->timestamp
        - found_fields++
     c. ELSE IF line contains "PreviousCustomReason:":
        - Extract value → info->customReason
        - found_fields++
     d. ELSE IF line contains "PreviousOtherReason:":
        - Extract value → info->otherReason
        - found_fields++
     
     IF found_fields >= 4: BREAK (early exit, all fields found)

3. IF found_fields == 0 (no "Previous" fields, try raw):
     - Repeat above for "RebootInitiatedBy", "RebootTime", etc.
     - IF RebootInitiatedBy == "HAL_SYS_Reboot":
       - Call resolve_hal_sys_reboot(line)

4. RETURN found_fields count
```

**Algorithm 3: HAL_SYS_Reboot Resolution (resolve_hal_sys_reboot)**
```
Input: rebootReasonLine
Output: source, otherReason

1. Find substring "Triggered from " in line
2. Position = after "Triggered from "
3. Find next space: this marks end of initiator
4. Extract: initiator = text from Position to space
5. Extract: otherReason = text from space to '(' or end-of-line
6. Trim trailing whitespace from both
7. RETURN (initiator, otherReason)

Example:
Input:  "... Triggered from PwrMgr thermal shutdown (temp 95C)"
Output: initiator="PwrMgr", otherReason="thermal shutdown"
```

**Algorithm 4: Panic Detection (detect_kernel_panic)**
```
Input: log_file_path
Output: PanicInfo struct

panic_signatures[] = {
    "Kernel panic - not syncing",
    "Kernel Panic",
    "Kernel Oops",
    "Oops - undefined instruction",
    ...
}

1. FOR each panic_signature:
     a. OPEN kernel log file (messages.txt or pstore console)
     b. FOR each line in file:
        - IF line contains panic_signature:
          - Set panicInfo.detected = true
          - Set panicInfo.panicType = signature
          - Set panicInfo.details = line
          - RETURN true (short-circuit on first match)
2. RETURN false (no panic detected)
```

**Algorithm 5: Firmware Failure Check (check_firmware_failure)**
```
Input: ctx (device context)
Output: FirmwareFailure struct

1. Check for MAX REBOOT:
     a. OPEN /opt/logs/PreviousLogs/uimgr_log.txt
     b. Scan for "Box has rebooted 10 times"
     c. IF found:
        - Set maxRebootDetected = true
        - Set detected = true
        - RETURN true

2. Check for ECM CRASH:
     a. OPEN /opt/logs/PreviousLogs/messages-ecm.txt
     b. Scan for "**** CRASH ****"
     c. IF found:
        - Set ecmCrashDetected = true
        - Set detected = true
        - RETURN true

3. RETURN false (no firmware failure)
```

**Algorithm 6: Classification (classify_reboot_reason)**
```
Input: RebootInfo, EnvContext, HardwareReason, PanicInfo, FirmwareFailure
Output: info->reason populated

Priority order:
1. IF panicInfo.detected:
     info->reason = "KERNEL_PANIC"
     RETURN

2. ELSE IF fwFailure.detected:
     info->reason = "FIRMWARE_FAILURE"
     RETURN

3. ELSE IF is_app_triggered(info->source):
     info->reason = "APP_TRIGGERED"
     RETURN

4. ELSE IF is_ops_triggered(info->source):
     info->reason = "OPS_TRIGGERED"
     RETURN

5. ELSE IF is_maintenance_triggered(info->source):
     info->reason = "MAINTENANCE_REBOOT"
     RETURN

6. ELSE:
     Info->reason = "UNKNOWN_RESET"
     RETURN
```

## 6. Interfaces & Integration Points

### Public Function Signatures (from update-reboot-info.h)

| Function | Signature | Notes |
|----------|-----------|-------|
| **Lock Management** | `int acquire_lock(const char *lockDir);` | Returns SUCCESS if lock acquired, ERROR_LOCK_FAILED if another instance running |
| | `int release_lock(const char *lockDir);` | Cleanup lock directory |
| **Device Properties** | `int parse_device_properties(EnvContext *ctx);` | Loads SOC, profile, device type from RDK API or `/etc/device.properties` |
| **Log Discovery** | `int find_previous_reboot_log(char *out_path, size_t len);` | Executes search priority: timestamped → backups → flat |
| **Log Parsing** | `int parse_legacy_log(const char *logPath, RebootInfo *info);` | Single-pass extraction of reboot fields |
| **Hardware Reason** | `int get_hardware_reason(const EnvContext *ctx, HardwareReason *hwReason, RebootInfo *info);` | Platform-specific (future: SOC-based dispatch) |
| **Panic Detection** | `int detect_kernel_panic(const EnvContext *ctx, PanicInfo *panicInfo);` | Scans messages.txt and pstore |

The implementation uses a comprehensive error code system:

```c
#define SUCCESS               0
#define ERROR_GENERAL        -1
#define ERROR_LOCK_FAILED    -2
#define ERROR_FILE_NOT_FOUND -3
#define ERROR_PARSE_FAILED   -4
#define FAILURE              -5
```

### Error Handling Approach

1. **Non-Critical Failures:** Logged but don't halt workflow
   - Missing log file → derive from hardware/panic instead
   - Parse failure on legacy log → continue with empty fields
   - Panic detection timeout → assume no panic, continue

2. **Critical Failures:** Prevent output generation
   - Lock acquisition failure → exit immediately
   - Directory creation failure → exit
   - JSON write failure → logged as error, return code set

3. **Cleanup on Error:**
   - Lock is always released on exit (success or failure)
   - Temp files cleaned up via `rename()` (atomic operation)
   - Partial information written when possible (graceful degradation)

4. **Logging Strategy:**
   - All errors logged via RDK_LOG with full context
   - Error codes propagated to caller
   - Stderr available for debugging

## 8. Concurrency Considerations

### Lock Mechanism

```c
// Directory-based mutual exclusion
int acquire_lock(const char *lockDir)
{
    if (mkdir(lockDir, 0700) == 0)
        return SUCCESS;    // We got the lock
    
    if (errno == EEXIST)
        return ERROR_LOCK_FAILED;  // Already locked
    
    return ERROR_GENERAL;
}

int release_lock(const char *lockDir)
{
    return (rmdir(lockDir) == 0) ? SUCCESS : ERROR_GENERAL;
}
```

### Guarantees

- **Mutual Exclusion:** Only one instance can run at a time (lock directory is atomic)
- **No Race Conditions:** File operations use proper stat(), rename() atomicity
- **Deadlock Prevention:** Lock release always happens in cleanup (goto cleanup on any error)
- **No File Locking Required:** Single-process guarantee via directory lock

### File Handling

- Files kept open minimally (stream operations closed immediately after use)
- Atomic writes: temp file → rename (prevents corruption)
- No file descriptors held across operations

## 9. Portability Strategy

### Conditional Compilation Support

```c
// Future platform abstraction (currently generic)
#ifdef SOC_BRCM
    int read_brcm_previous_reboot_reason(...)
#endif

#ifdef SOC_REALTEK
    int parse_realtek_wakeup_reason(...)
#endif

#ifdef SOC_AMLOGIC
    int map_amlogic_reset_code(...)
#endif
```

### Code Standards

- **C99 compliant** - No GNU extensions
- **No external dependencies** beyond RDK (logger, telemetry, utilities)
- **Bounds checking** - All string operations use `snprintf()` or `memcpy()` with length
- **No dynamic allocation** - Stack and static arrays only

### Cross-Platform Considerations

- Absolute paths use `/` (portable)
- File operations use POSIX APIs (`opendir()`, `readdir()`, `stat()`, `rename()`)
- Timestamps in UTC (portable)

## 10. Logging Approach

### RDK Logger Integration

```c
// Configuration
rdk_logger_ext_config_t config = {
    .pModuleName = "LOG.RDK.REBOOTINFO",     // Module identifier
    .loglevel = RDK_LOG_INFO,                 // Configurable level
    .output = RDKLOG_OUTPUT_CONSOLE,          // Stdout/stderr
    .format = RDKLOG_FORMAT_WITH_TS,          // Include timestamps
    .pFilePolicy = NULL                       // No file output here
};

// Usage
RDK_LOG(RDK_LOG_INFO, "LOG.RDK.REBOOTINFO", 
        "Previous reboot log: %s\n", out_path);
RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.REBOOTINFO", 
        "Parsed %d fields from legacy log\n", found_fields);
RDK_LOG(RDK_LOG_ERROR, "LOG.RDK.REBOOTINFO", 
        "Failed to parse log: %s\n", strerror(errno));
```

### Log Levels

- **DEBUG** - Detailed operation flow (file paths, parsing steps)
- **INFO** - Major milestones (lock acquired, parsing complete, output written)
- **WARNING** - Recoverable issues (missing optional fields)
- **ERROR** - Serious issues (lock failed, write failed)

### Output Destinations

- **Console/Stderr** - Real-time diagnostics during execution
- **RDK Logger** - Aggregated system logs
- **Parodus Log** - Telemetry/operational metrics
- **JSON Files** - Machine-readable results for downstream systems

## 11. Security & Robustness

### Input Validation

1. **File Size Limits:**
   - Max 128 KB per log file (protects against DoS)
   - Line length 512 bytes (bounds fgets operations)
   - Path length 256 bytes (buffer overflow prevention)

2. **Buffer Operations:**
   - All string copies use `snprintf()` with size parameter
   - Null termination guaranteed on all `strncpy()` operations
   - Pointer arithmetic validated before use

3. **Path Handling:**
   - No path traversal (no `..` allowed in filenames)
   - Absolute paths in constants (no user input for paths)
   - Directory permissions 0700 (owner-only access)

### Character Validation

- Non-printable characters rejected from hardware reason tokens
- JSON output escapes quotes minimally (tokens alphanumeric + underscore)
- newline/carriage-return trimming in all parsed fields

### File Permissions  

- Directory creation: `0700` (rwx------)
- Files inherit default umask (typically `0600` or `0644`)
- Lock directory: implicit 0700 via `mkdir()` mode

### Atomic Operations

- JSON writes via temp+rename (prevents corruption on crash)
- Directory lock via `mkdir()` (atomic on POSIX)
- No partial state visible to other processes

## 12. Performance Considerations

### Optimization Strategies

| Operation | Complexity | Optimization |
|-----------|-----------|---|
| **Log Discovery** | O(n*m) where n=subdirs, m=files | Early exit on first match, no tree traversal |
| **Log Parsing** | O(l) where l=lines | Single pass, early break when all fields found |
| **Panic Detection** | O(s*l) where s=signatures, l=lines | Short-circuit on first signature match |
| **Classification** | O(1) | Direct array membership test (n<50 entries) |
| **File I/O** | O(s) where s=file size | Buffered reads via `fgets()` |
| **Lock Operations** | O(1) | Atomic directory operations |

### Resource Usage

- **Memory:** ~10 KB static (data structures + buffers)
- **CPU:** <500ms typical execution
- **Disk I/O:** 3-5 files read, 3-4 files written
- **No Dynamic Allocation:** Stack-based only

### Instrumentation

- Optional compile-time performance metrics
- RDK Logger timestamps for each stage
- T2 telemetry for classification success rate

## 13. Known Limitations & Future Improvements

### Current Limitations

1. **Hardware Detection:** Generic implementation, platform-specific plugins pending
2. **Telemetry:** T2 integration optional (weak symbols for fallback)
3. **Concurrency:** Single-process only (no multi-threaded access)
4. **Recovery:** No automatic recovery from corrupted JSON

### Future Extensibility Paths

1. **Multi-Platform Hardware Support:**
   - `hw_brcm.c`, `hw_realtek.c`, `hw_amlogic.c` modules
   - SOC-based dispatch via device properties

2. **Additional Panic Signatures:**
   - Expand `panic_signatures[]` array
   - Platform-specific panic detectors

3. **Telemetry Expansion:**
   - Additional T2 metrics
   - Custom telemetry endpoints

4. **Advanced Classification:**
   - Machine learning-based categorization
   - Historical pattern analysis

## 14. High-Level Sequence (Summary)

1. **Initialization:** Lock, logger, environment
2. **Log Discovery:** Find previous reboot log via priority search
3. **Parsing:** Extract fields or create from hardware/panic
4. **Detection:** Panic and firmware failure checks
5. **Classification:** Priority-based category assignment
6. **Output:** Write JSON, telemetry, logs, hardpower info
7. **Cleanup:** Release lock, close files
8. **Exit:** Return status code
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

## 13. Known Limitations & Future Improvements

### Current Limitations

1. **Hardware Detection:** Generic implementation, platform-specific plugins pending
2. **Telemetry:** T2 integration optional (weak symbols for fallback)
3. **Concurrency:** Single-process only (no multi-threaded access)
4. **Recovery:** No automatic recovery from corrupted JSON

### Future Extensibility Paths

1. **Multi-Platform Hardware Support:**
   - `hw_brcm.c`, `hw_realtek.c`, `hw_amlogic.c` modules
   - SOC-based dispatch via device properties

2. **Additional Panic Signatures:**
   - Expand `panic_signatures[]` array
   - Platform-specific panic detectors

3. **Telemetry Expansion:**
   - Additional T2 metrics
   - Custom telemetry endpoints

4. **Advanced Classification:**
   - Machine learning-based categorization
   - Historical pattern analysis

## 14. High-Level Sequence (Summary)

1. **Initialization:** Lock, logger, environment
2. **Log Discovery:** Find previous reboot log via priority search
3. **Parsing:** Extract fields or create from hardware/panic
4. **Detection:** Panic and firmware failure checks
5. **Classification:** Priority-based category assignment
6. **Output:** Write JSON, telemetry, logs, hardpower info
7. **Cleanup:** Release lock, close files
8. **Exit:** Return status code

## 15. Non-Goals (Current Scope)

- Real-time monitoring of subsequent reboots
- Telemetry batching/caching
- Multi-language bindings
- Platform-specific hardware modules (future phase)

## 16. Assumptions

- Device properties file (`/etc/device.properties`) reliably accessible
- Telemetry API optional (graceful fallback if not available)
- System clock reliable for UTC timestamps  
- RDK Logger available at compile/runtime
- Filesystem supports atomic `rename()` operations
- Directory-based locking effective on target platform

## 17. Implementation Status

### Completed Modules
- ✅ `rebootreason_main.c` - Complete workflow orchestration with lock, environment setup, output persistence
- ✅ `bootup_reason_checker.c` - **NEW: Migrated from reboot-checker.sh** - Log discovery, legacy parsing, HAL_SYS_Reboot resolution
- ✅ `reboot_reason_classify.c` - Panic detection, firmware failure checking, multi-stage classification
- ✅ `log_parser.c` - Device property loading (SOC, profile, device type)
- ✅ `json.c` - JSON serialization with atomic write
- ✅ `parodus_log_update.c` - Telemetry and parodus log integration
- ✅ `update-reboot-info.h` - Public API header with all function declarations

### Pending Modules (Future)
- ⏳ `hw_brcm.c` - Broadcom platform support
- ⏳ `hw_realtek.c` - Realtek platform support
- ⏳ `hw_amlogic.c` - Amlogic platform support  
- ⏳ `hw_mtk.c` - MediaTek platform support

### Testing
- ✅ Unit tests (gtest-based) for core logic
- ✅ Integration tests with legacy shell script output
- ✅ **NEW: bootup_reason_checker functions tested** - Log discovery, parsing, HAL resolution
- ⏳ Platform-specific hardware tests (pending platform modules)

## 18. Operational Notes

### Invocation

```bash
# Via systemd service
systemctl start update-reboot-info

# Direct execution
/usr/bin/update-reboot-info

# Output location
/opt/secure/reboot/previousreboot.info
```

### Configuration

- **Log Level:** Set via RDK logger configuration
- **Device Properties:** From `/etc/device.properties`
- **Lock Location:** `/tmp/rebootInfo.lock`
- **Output Directory:** `/opt/secure/reboot/`

### Monitoring & Debugging

- Check RDK logs: `/var/log/messages` (if file output configured)
- Check outputs: `ls -la /opt/secure/reboot/`
- Enable DEBUG logging for detailed operational flow
- T2 metrics provide classification success/failure telemetry

---

**Document Version:** 2.0 (Updated with C Implementation Details)  
**Last Updated:** April 2026  
**Status:** Complete Core Implementation

*End of HLD.*
