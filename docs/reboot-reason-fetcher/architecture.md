# reboot-manager Architecture

## 1. Overview

This repository implements the `update-prev-reboot-info` binary, which derives and persists previous reboot context for the device.

Primary responsibilities:
- Enforce single-instance execution
- Gate execution on runtime flags
- Read reboot context from multiple sources (new file, legacy log, hardware registers, panic traces)
- Classify reboot reason into normalized categories
- Persist results to secure state files and Parodus log

Build output (from [src/Makefile.am](src/Makefile.am)):
- `update-prev-reboot-info` (installed as a regular binary via `bin_PROGRAMS`)

---

## 2. High-Level Runtime Flow

Entry point: [src/main.c](src/main.c#L61)

1. Initialize logger and optional telemetry.
2. Acquire process lock (`LOCK_DIR`) to prevent parallel execution.
3. Parse environment/device context (`SOC`, `DEVICE_TYPE`, support flags).
4. Check whether update should run (`STT` + reboot-info flags).
5. Ensure reboot state directory exists (`/opt/secure/reboot`).
6. If `/opt/secure/reboot/reboot.info` exists:
   - Move to previous reboot file and update Parodus handoff file.
7. Else derive reboot info from:
   - legacy reboot log (`/opt/logs/rebootInfo.log`)
   - kernel panic detection
   - firmware-failure checks
   - hardware reboot reason readers
   - classifier normalization
8. Persist normalized reboot info JSON and side files.
9. Copy keypress info and set invocation marker.
10. Release lock and exit.

---

## 3. Module Breakdown

## 3.1 Orchestration (`main`)
- File: [src/main.c](src/main.c)
- Owns end-to-end control flow, cleanup path, and final persistence decisions.

Key calls:
- `acquire_lock` / `release_lock`
- `parse_device_properties`
- `update_reboot_info`
- `parse_legacy_log`
- `detect_kernel_panic`
- `check_firmware_failure`
- `get_hardware_reason`
- `classify_reboot_reason`
- `write_reboot_info` / `write_hardpower`
- `update_parodus_log` / `handle_parodus_reboot_file`
- `copy_keypress_info`

## 3.2 Platform HAL routing
- File: [src/platform_hal.c](src/platform_hal.c)
- Selects platform-specific readers by `SOC` and supports fallback probing.

Supported sources:
- BRCM: `/proc/brcm/previous_reboot_reason`
- RTK/REALTEK: `/proc/cmdline` (`wakeupreason=`)
- AMLOGIC: `/sys/devices/platform/aml_pm/reset_reason`
- MTK: `/sys/mtk_pm/boot_reason`

## 3.3 Input parsing and environment
- File: [src/log_parser.c](src/log_parser.c)
- Parses device properties and legacy reboot log fields.
- Contains readers for raw hardware reboot reasons.
- Implements flag-gating policy (`update_reboot_info`).

## 3.4 Classification engine
- File: [src/reboot_reason_classify.c](src/reboot_reason_classify.c)
- Detects kernel panic signatures (including pstore path).
- Detects firmware-failure conditions (max reboot / ECM crash).
- Maps raw/legacy/hardware evidence into normalized reboot categories.
- Maintains app/ops/maintenance trigger lists.

## 3.5 Persistence and locking
- File: [src/json.c](src/json.c)
- Lock lifecycle (`flock`) for single-instance safety.
- Writes reboot JSON and hard-power timestamp JSON.

## 3.6 Parodus and keypress integration
- File: [src/parodus_log_update.c](src/parodus_log_update.c)
- Updates Parodus log with `PreviousRebootInfo` record.
- Handles `parodusreboot.info` handoff to previous-state file.
- Copies `keypress.info` to `previouskeypress.info`.

---

## 4. Data Model

Defined in [include/update-reboot-info.h](include/update-reboot-info.h):

- `RebootInfo`
  - `timestamp`, `source`, `reason`, `customReason`, `otherReason`
- `EnvContext`
  - `soc`, `buildType`, `device_type`, plus platform support flags
- `HardwareReason`
  - `rawReason`, `mappedReason`
- `PanicInfo`
  - detected state + panic signature details
- `FirmwareFailure`
  - detected state + subtype flags and initiator/details

---

## 5. Persistent Files and Side Effects

Primary files:
- `/opt/secure/reboot/reboot.info`
- `/opt/secure/reboot/previousreboot.info`
- `/opt/secure/reboot/hardpower.info`
- `/opt/secure/reboot/parodusreboot.info`
- `/opt/secure/reboot/previousparodusreboot.info`
- `/opt/secure/reboot/keypress.info`
- `/opt/secure/reboot/previouskeypress.info`
- `/opt/logs/rebootInfo.log`
- `/opt/logs/parodus.log`

Control flags:
- `/tmp/stt_received`
- `/tmp/rebootInfo_Updated`
- `/tmp/Update_rebootInfo_invoked`

---

## 6. Classification Priority (Conceptual)

In practical terms, classification resolves with this precedence:
1. Firmware-failure evidence (if detected)
2. Existing custom reason mapping (app/ops/maintenance)
3. Kernel panic evidence
4. Hardware reboot reason mapping
5. Fallback unknown/software defaults

This keeps reboot reason deterministic while preserving platform-specific signals.

---

## 7. Error Handling and Reliability

- Centralized return codes in [include/update-reboot-info.h](include/update-reboot-info.h#L60-L68)
- Defensive file existence/open checks in parsing and readers
- Guaranteed lock release through cleanup path in [src/main.c](src/main.c)
- Non-fatal behavior for optional inputs (e.g., missing keypress info)

---

## 8. Extensibility Notes

To add a new platform/SOC:
1. Add new reader function in parser/HAL modules
2. Add route in `get_hardware_reason`
3. Extend mapping rules in classifier if needed
4. Add unit tests in `unittest/`

To add new classification trigger:
1. Update trigger arrays / mapping logic in `reboot_reason_classify.c`
2. Validate ordering with firmware/panic/hardware precedence
3. Add/adjust tests

---

## 9. Build and Test Integration

- Top-level build enters `src/` via [Makefile.am](Makefile.am).
- Binary target is declared in [src/Makefile.am](src/Makefile.am).
- Unit tests are under `unittest/`.
- Functional tests are under `test/`.

This architecture separates orchestration, detection, classification, and persistence so behavior can be changed with minimal coupling.
