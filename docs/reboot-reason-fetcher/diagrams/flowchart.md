# Flowcharts: Reboot Reason Updater

## 1. Detailed Mermaid Flowchart

```mermaid
flowchart TD
    A[Start] --> B[Acquire Lock /tmp/rebootInfo.lock]
    B --> C{Device = PLATCO or LLAMA?}
    C -->|Yes| D{Flag /tmp/Update_rebootInfo_invoked exists?}
    C -->|No| E[Check STT + rebootInfo flags]
    D -->|Yes| E
    D -->|No| F[Bypass flags for first run]
    E -->|Missing flags| G[Release Lock & Exit]
    E -->|Flags present| H[Ensure /opt/secure/reboot dir]

    F --> H
    G --> Z[End]

    H --> I{RDK_PROFILE == TV?}
    I -->|Yes| J{messages.txt has PreviousRebootReason?}
    J -->|No| K[Kernel Panic Check (pstore/messages)]
    J -->|Yes| L[Skip Kernel Panic Annotation]
    K -->|Panic| M[Append 'PreviousRebootReason: kernel_panic!' to messages.txt]
    K -->|No| N[Run get-reboot-reason.sh if present]
    M --> L
    N --> L

    L --> O{File reboot.info exists?}
    O -->|Yes| P[Move reboot.info -> previousreboot.info]
    P --> Q{File parodusreboot.info exists?}
    Q -->|Yes| R[Append to parodus.log; move to previousparodusreboot.info]
    Q -->|No| S[Proceed]
    R --> S
    S --> T[Handle keypress.info if exists]
    T --> U[Create invocation flag if missing]
    U --> V[Release Lock & End]
    
    O -->|No| W[Initialize empty fields]
    W --> X{rebootInfo.log exists?}
    X -->|Yes| Y[Parse PreviousRebootInitiatedBy, Time, Custom, Other]
    X -->|No| AA[Fields remain empty]
    Y --> AB[Check if InitiatedBy empty]
    AA --> AB
    AB -->|Empty| AC[Set rebootTime = current UTC]
    AC --> AD[Kernel Panic Check]
    AD -->|Panic| AE[reason=KERNEL_PANIC; source=Kernel; customReason=HW Register - KERNEL_PANIC; other=panic text]
    AD -->|No| AF{SOC / Device Type}
    AF -->|AMLOGIC| AG[Read reset_reason; map code]
    AF -->|BRCM| AH[Read /proc/brcm/previous_reboot_reason]
    AF -->|RTK/REALTEK/TV| AI[Parse wakeupreason from /proc/cmdline]
    AF -->|Else| AJ[Fallback HARD_POWER NULL]
    AG --> AK[customReason="Hardware Register - <mapped>"; reason=<mapped>]
    AH --> AL[Parse tokens -> reason + customReason]
    AI --> AM[Extract wakeupreason -> reason]
    AJ --> AN[reason=HARD_POWER; customReason=HW Register - NULL]
    AK --> AO
    AL --> AO
    AM --> AO
    AN --> AO

    AO --> AP[Classification via APP/OPS/MAINT arrays -> reason category if initiator present]
    AP --> AQ{Firmware Failure Override?}
    AQ -->|Yes| AR[reason=FIRMWARE_FAILURE]
    AQ -->|No| AS[Keep reason]
    AR --> AT[Write previousreboot.info JSON]
    AS --> AT
    AT --> AU{reason is HARD_POWER/POWER_ON_RESET/UNKNOWN_RESET or first hardpower file missing?}
    AU -->|Yes| AV[Write/Update hardpower.info timestamp]
    AU -->|No| AW[Skip hardpower update]
    AV --> AW
    AW --> AX{SOC RTK/REALTEK?}
    AX -->|Yes| AY[Append PreviousRebootReason (lowercase) to messages.txt]
    AX -->|No| AZ[Skip]
    AY --> BA[Handle keypress.info]
    AZ --> BA
    BA --> BB[Create invocation flag if missing]
    BB --> BC[Log to parodus.log (previous reboot info)]
    BC --> BD[Release Lock]
    BD --> Z[End]
```

## 2. Simplified Text-Based Flow (ASCII)

```
START
  acquire lock
  if (device is PLATCO/LLAMA) {
      if (invocation flag exists) check STT+reboot flags else bypass
  } else check STT+reboot flags
  if (flags missing) release lock & EXIT

  ensure /opt/secure/reboot directory

  if (RDK_PROFILE == TV && kernel log lacks PreviousRebootReason) {
      if (kernel panic) append kernel_panic reason
      else run get-reboot-reason.sh if present
  }

  if (reboot.info exists) {
      move to previousreboot.info
      if (parodusreboot.info exists) log & move
      copy keypress info
      create invocation flag
      release lock & END
  } else {
      parse rebootInfo.log for prior fields
      if (initiator empty) {
          if (kernel panic) set panic reason
          else {
              switch (platform) {
                 AMLOGIC -> map numeric reset code
                 BRCM    -> parse previous_reboot_reason tokens
                 REALTEK/TV -> parse wakeupreason
                 default -> HARD_POWER fallback
              }
          }
      } else {
          classify reason via membership arrays
      }

      firmware failure override check (non-TV)

      write previousreboot.info JSON
      update hardpower.info if applicable
      append lowercased PreviousRebootReason for Realtek/RTK
      copy keypress info
      create invocation flag
      parodus log update
      release lock & END
  }
END
```

## 3. Annotations / Notes
- Firmware failure condition in original script uses `||` instead of `&&`, causing override check to always run. Migration fixes this.
- Hardware mapping is platform-specific; fallback ensures non-crashing behavior.
- Sequence between firmware failure and classification: firmware failure takes precedence.

## 4. Linked Flow Concepts
- Panic detection sub-flow is reusable across platforms.
- Hardware reason resolution sub-flow branches by SOC value.
- JSON persistence is centralized and invoked post-classification.

*End of Flowcharts Document.*
