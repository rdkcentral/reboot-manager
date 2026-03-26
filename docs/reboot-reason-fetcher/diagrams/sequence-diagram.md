# Sequence Diagrams: Reboot Reason Updater

## 1. Mermaid Sequence Diagram

```mermaid
sequenceDiagram
    autonumber
    participant S as Script(main)
    participant L as Lock Manager
    participant F as Filesystem
    participant E as Env Loader
    participant K as Kernel Logs
    participant P as Pstore
    participant HW as Hardware Registers
    participant CL as Classification Module
    participant FF as Firmware Failure Checker
    participant J as JSON Writer
    participant T as Telemetry
    participant PA as Parodus Logger
    participant KP as Keypress Handler

    S->>L: acquire_lock("/tmp/rebootInfo.lock")
    L-->>S: lock_acquired / error

    S->>E: load_device_properties("/etc/device.properties")
    E-->>S: DeviceContext

    S->>F: check flags (/tmp/stt_received, /tmp/rebootInfo_Updated)
    F-->>S: flags status

    alt Flags missing (except bypass)
        S->>L: release_lock()
        S-->>S: exit
    end

    S->>F: ensure_directory("/opt/secure/reboot")

    S->>F: stat("/opt/secure/reboot/reboot.info")
    F-->>S: exists? yes/no

    opt New reboot.info exists
        S->>F: move reboot.info -> previousreboot.info
        S->>F: stat(parodusreboot.info)
        alt parodusreboot.info exists
            S->>F: read contents
            S->>PA: append to parodus.log
            S->>F: move parodusreboot.info -> previousparodusreboot.info
        end
        S->>KP: copy keypress.info if present
        S->>F: touch invocation flag
        S->>L: release_lock()
        S-->>S: exit
    end

    S->>F: open("/opt/logs/rebootInfo.log")
    F-->>S: parsed fields (may be empty)

    alt Initiator empty
        S->>K: read messages.txt (TV check)
        S->>P: examine pstore signatures
        P-->>S: panic? true/false
        alt Panic
            S-->>S: set reason=KERNEL_PANIC
        else No panic
            S->>HW: read platform-specific register/cmdline
            HW-->>S: raw reason tokens
            S->>CL: map hardware to reason/custom
            CL-->>S: classification results
        end
    else Initiator present
        S->>CL: classify via APP/OPS/MAINT arrays
        CL-->>S: reason category
    end

    S->>FF: check firmware failure conditions
    FF-->>S: override flag
    alt override true
        S-->>S: reason=FIRMWARE_FAILURE
        S->>T: telemetry_count("SYST_ERR_10Times_reboot") (maybe)
    end

    S->>J: write previousreboot.info JSON
    J-->>S: success/fail

    alt reason requires hardpower.info update
        S->>J: write/update hardpower.info
    end

    S->>K: append PreviousRebootReason (conditional lowercasing)
    S->>KP: copy keypress.info if exists
    S->>PA: append Parodus summary
    S->>F: touch invocation flag
    S->>L: release_lock()
    S-->>S: end
```

## 2. Simplified Text-Based Sequence

```
Script -> LockManager: acquire
LockManager -> Script: success

Script -> Env: load device properties
Env -> Script: DeviceContext

Script -> FS: verify gating flags
FS -> Script: status
[If missing and not bypass] -> release lock, exit

Script -> FS: ensure reboot directory
Script -> FS: check presence of reboot.info
[If present]:
    move file -> previousreboot.info
    handle parodusreboot.info if exists
    keypress copy
    create invocation flag
    release lock
    exit

Else:
    Script -> FS: parse rebootInfo.log
    If initiator empty:
        Script -> KernelLogs/Pstore: panic scan
        If panic -> set panic reason
        Else -> Script -> HW: read register/cmdline/reset code
                HW -> Script: tokens
                Script -> Classification: hardware mapping
    Else:
        Script -> Classification: membership mapping

    Script -> FirmwareFailureChecker: analyze logs
    If override -> reason = FIRMWARE_FAILURE

    Script -> JSONWriter: persist previousreboot.info
    If hardpower required -> JSONWriter: update hardpower.info
    Script -> KernelLogs: append previous reason (platform conditional)
    Script -> Keypress: copy keypress file
    Script -> ParodusLogger: append summary
    Script -> FS: touch invocation flag
    Script -> LockManager: release
END
```

## 3. Notes
- Telemetry events are conditional; design must allow them to be no-ops when library absent.
- Hardware interaction isolated to platform modules for maintainability.
- Sequence ensures early exit conditions release lock to prevent deadlock.

*End of Sequence Diagrams.*
