# reboot-manager Architecture

## Overview

`reboot-manager` records reboot context, classifies reboot reason, persists structured metadata,
and triggers reboot after cleanup and policy checks.

## Source Layout

```text
rebootnow/src/
├── main.c            # argument parsing, reason classification, orchestration
├── cyclic_reboot.c   # cyclic reboot pause/defer guard and state handling
├── system_cleanup.c  # pre-reboot service cleanup
├── rbus_interface.c  # RBUS get/set helpers
└── utils.c           # timestamp, file-write helper, telemetry wrappers
```

Headers are exposed from `rebootnow/include`.

## Reboot Flow

1. Parse reboot trigger options (`-s` or `-c`) and optional custom/other reason fields.
2. Emit T2 marker based on source process and trigger type.
3. Classify reboot reason into app/ops/maintenance/firmware categories.
4. Persist reboot metadata:
   - `/opt/logs/rebootInfo.log`
   - `/opt/secure/reboot/reboot.info`
   - `/opt/secure/reboot/previousreboot.info`
   - `/opt/secure/reboot/parodusreboot.info`
5. Apply cyclic reboot control to decide defer vs proceed.
6. Perform pre-reboot cleanup and notification updates.
7. Trigger reboot sequence with fallback (`reboot`, then `systemctl reboot`, then `reboot -f`).

## External Interfaces

- RBUS: RFC/parameter reads and writes (notification and feature toggles)
- RDK Logger: reboot diagnostics and traceability logs
- Telemetry (T2): reboot marker emission (when enabled)
- Secure wrapper/system commands: controlled reboot and cleanup command execution

## Reliability Notes

- PID-file guard prevents concurrent rebootnow instances.
- Structured files under `/opt/secure/reboot` preserve reboot context for downstream consumers.
- Cyclic reboot logic prevents repeated immediate reboots under configured pause windows.
