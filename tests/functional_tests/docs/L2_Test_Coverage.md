# L2 Test Coverage Summary

```
- Total source functions (approx): ~97
- Functions with direct L2 coverage (approx): ~34
- Functions with indirect L2 coverage (approx): ~17
- Functions with no L2 coverage (approx): ~46

- Active L2 test functions: 17
- Disabled L2 test functions: 0
- Active feature scenarios: 5
- Proposed new test scenarios: 24

- High priority: 10
- Medium priority: 9
- Low priority: 5

- Test files active: 17
- Test files disabled (commented out): 0

- Estimated current L2 functional coverage: ~53%
- Target L2 functional coverage: ~80%
```

## Revalidated Baseline (2026-06-11)

- Measured C source files for module scan: 10
- Measured total C function definitions (regex approximation): 97
- Measured active Python L2 test files (test_*.py): 17
- Measured active Python L2 test functions (def test_*): 17
- Measured active feature files: 5
- Measured active feature scenarios (Scenario/Scenario Outline): 5

These baseline numbers supersede the earlier higher inventory values that were previously listed in this file.

## Scope of This Analysis

- This mapping is based on Python functional tests under [tests/functional_tests/test](tests/functional_tests/test).
- It compares asserted behaviors in those tests against runtime paths in reboot-manager binaries, mainly update-prev-reboot-info and rebootnow.
- It distinguishes strong validation (exact value assertions) from smoke validation (existence or substring-only checks).

## Reboot-Manager Runtime Paths Referenced

- Orchestration entry and control flow: [reboot-reason-fetcher/src/rebootreason_main.c](reboot-reason-fetcher/src/rebootreason_main.c#L64)
- Update gate check used by updater: [reboot-reason-fetcher/src/log_parser.c](reboot-reason-fetcher/src/log_parser.c#L107)
- Reboot.info handoff branch: [reboot-reason-fetcher/src/rebootreason_main.c](reboot-reason-fetcher/src/rebootreason_main.c#L128)
- Previous log field rewrite: [reboot-reason-fetcher/src/bootup_reason_checker.c](reboot-reason-fetcher/src/bootup_reason_checker.c#L440)
- Legacy previous-log discovery and parse: [reboot-reason-fetcher/src/bootup_reason_checker.c](reboot-reason-fetcher/src/bootup_reason_checker.c#L98), [reboot-reason-fetcher/src/bootup_reason_checker.c](reboot-reason-fetcher/src/bootup_reason_checker.c#L242)
- Classifier mapping path: [reboot-reason-fetcher/src/reboot_reason_classify.c](reboot-reason-fetcher/src/reboot_reason_classify.c#L425)
- Parodus and keypress side files: [reboot-reason-fetcher/src/parodus_log_update.c](reboot-reason-fetcher/src/parodus_log_update.c#L100), [reboot-reason-fetcher/src/parodus_log_update.c](reboot-reason-fetcher/src/parodus_log_update.c#L162)

## Coverage Matrix: Tests to Product Behavior

| Area | Product behavior | Primary tests | Coverage depth |
| --- | --- | --- | --- |
| Reboot helper artifact generation | rebootnow creates reboot.info, parodusreboot.info, rebootInfo.log, rebootNow flag | [tests/functional_tests/test/test_reboot_triggered.py](tests/functional_tests/test/test_reboot_triggered.py), [tests/functional_tests/test/test_scenario_bootup_reboot_files_and_log_created.py](tests/functional_tests/test/test_scenario_bootup_reboot_files_and_log_created.py) | Medium to strong |
| Reboot helper direct reason mapping | App-triggered and firmware-failure style helper output | [tests/functional_tests/test/test_reboot_triggered.py](tests/functional_tests/test/test_reboot_triggered.py), [tests/functional_tests/test/test_reboot_crash_maintenance.py](tests/functional_tests/test/test_reboot_crash_maintenance.py) | Strong for tested values |
| Updater gate skip when flag missing | updater exits without writing previous artifacts if STT missing | [tests/functional_tests/test/test_scenario_update_prev_reboot_skips_when_flags_missing.py](tests/functional_tests/test/test_scenario_update_prev_reboot_skips_when_flags_missing.py) | Medium |
| Updater happy path after helper | previousreboot.info and previousparodusreboot.info created from seeded reboot.info/parodusreboot.info | [tests/functional_tests/test/test_scenario_update_prev_reboot_service_flow_after_stt_flag.py](tests/functional_tests/test/test_scenario_update_prev_reboot_service_flow_after_stt_flag.py), [tests/functional_tests/test/test_scenario_update_prev_reboot_generates_previous_files_and_flags.py](tests/functional_tests/test/test_scenario_update_prev_reboot_generates_previous_files_and_flags.py) | Smoke |
| Soft reboot category classification | customReason bucket mapping to APP_TRIGGERED, OPS_TRIGGERED, MAINTENANCE_REBOOT | [tests/functional_tests/test/test_scenario_soft_reboot_category_classification_matrix.py](tests/functional_tests/test/test_scenario_soft_reboot_category_classification_matrix.py) | Strong for representative inputs |
| Previous reboot JSON schema presence | previousreboot.info includes expected keys | [tests/functional_tests/test/test_scenario_previous_reboot_info_json_format.py](tests/functional_tests/test/test_scenario_previous_reboot_info_json_format.py) | Smoke |
| Reboot log previous fields | rebootInfo.log contains previous reboot field names | [tests/functional_tests/test/test_scenario_soft_reboot_updates_reboot_log_and_previous_reboot_info.py](tests/functional_tests/test/test_scenario_soft_reboot_updates_reboot_log_and_previous_reboot_info.py), [tests/functional_tests/test/test_scenario_reboot_info_log_previous_fields_present.py](tests/functional_tests/test/test_scenario_reboot_info_log_previous_fields_present.py) | Weak to medium |
| Parodus output traces | previous/parodus entries contain PreviousRebootInfo marker | [tests/functional_tests/test/test_scenario_parodus_log_contains_reboot_reason.py](tests/functional_tests/test/test_scenario_parodus_log_contains_reboot_reason.py), [tests/functional_tests/test/test_scenario_previous_parodus_file_has_software_reboot_info.py](tests/functional_tests/test/test_scenario_previous_parodus_file_has_software_reboot_info.py), [tests/functional_tests/test/test_scenario_previous_reboot_info_string_in_messages.py](tests/functional_tests/test/test_scenario_previous_reboot_info_string_in_messages.py) | Smoke |
| Hard reboot fallback mapping | fallback classification when no reboot.info and no hardware signal | [tests/functional_tests/test/test_scenario_hard_reboot_unknown_defaults_to_null_mapping.py](tests/functional_tests/test/test_scenario_hard_reboot_unknown_defaults_to_null_mapping.py), [tests/functional_tests/test/test_scenario_hard_reboot_updates_hardpower_and_previousreboot.py](tests/functional_tests/test/test_scenario_hard_reboot_updates_hardpower_and_previousreboot.py) | Strong for NULL fallback, smoke for hardpower timestamp |
| Kernel panic scenarios | helper and log marker interactions around panic text | [tests/functional_tests/test/test_scenario_kernel_panic_oops_updates_reboot_files_via_reboot_binary.py](tests/functional_tests/test/test_scenario_kernel_panic_oops_updates_reboot_files_via_reboot_binary.py), [tests/functional_tests/test/test_scenario_kernel_panic_oops_is_logged_in_messages.py](tests/functional_tests/test/test_scenario_kernel_panic_oops_is_logged_in_messages.py) | Weak to medium |

## Missing or Weakly Covered Behavior

1. Two-flag updater gate behavior is not validated end-to-end.
	Current fixture writes only stt_received in [tests/functional_tests/test/conftest.py](tests/functional_tests/test/conftest.py#L95), while control flow messaging expects both stt_received and rebootInfo_Updated around [reboot-reason-fetcher/src/rebootreason_main.c](reboot-reason-fetcher/src/rebootreason_main.c#L111).

2. Legacy previous-log derivation path is largely untested.
	Key functions [reboot-reason-fetcher/src/bootup_reason_checker.c](reboot-reason-fetcher/src/bootup_reason_checker.c#L98) and [reboot-reason-fetcher/src/bootup_reason_checker.c](reboot-reason-fetcher/src/bootup_reason_checker.c#L242) are not exercised with realistic PreviousLogs and backup rebootInfo.log sources.

3. reboot.info to previousreboot.info payload fidelity is not explicitly asserted.
	Tests typically confirm output file existence or key presence, not exact field-by-field preservation after [reboot-reason-fetcher/src/rebootreason_main.c](reboot-reason-fetcher/src/rebootreason_main.c#L128).

4. rebootInfo.log rewrite verification is mostly substring-based.
	Because [reboot-reason-fetcher/src/bootup_reason_checker.c](reboot-reason-fetcher/src/bootup_reason_checker.c#L440) rewrites PreviousReboot-prefixed lines, current substring assertions can pass without validating exact expected line structure and values.

5. Parodus output validation is marker-only.
	Tests do not verify exact formatting, ordering, and value fidelity for code paths in [reboot-reason-fetcher/src/parodus_log_update.c](reboot-reason-fetcher/src/parodus_log_update.c#L58) and [reboot-reason-fetcher/src/parodus_log_update.c](reboot-reason-fetcher/src/parodus_log_update.c#L100).

6. Kernel panic updater path is not isolated strongly.
	Panic tests are mostly helper-seeded or early-return style and do not deeply validate detection/classification flow in [reboot-reason-fetcher/src/reboot_reason_classify.c](reboot-reason-fetcher/src/reboot_reason_classify.c#L187) and [reboot-reason-fetcher/src/reboot_reason_classify.c](reboot-reason-fetcher/src/reboot_reason_classify.c#L479).

7. Keypress handoff is not covered by functional tests.
	[reboot-reason-fetcher/src/parodus_log_update.c](reboot-reason-fetcher/src/parodus_log_update.c#L162) has no direct scenario asserting copy behavior and failure handling.

8. Locking and concurrency behavior is not covered.
	No L2 tests simulate parallel updater invocation around lock handling in [reboot-reason-fetcher/src/rebootreason_main.c](reboot-reason-fetcher/src/rebootreason_main.c#L64).

## Suggested Missing-Info Additions for Future Updates

- Add exact expected-value assertions for previousreboot.info content after handoff and derive paths.
- Add dedicated two-flag gate matrix: neither flag, stt only, rebootInfo_Updated only, both flags.
- Add legacy log source matrix for PreviousLogs timestamped folder and bak1/bak2/bak3 fallback.
- Add strict line-format validation for rebootInfo.log PreviousReboot fields.
- Add strict Parodus payload verification for both generated and moved-file paths.
- Add keypress info transfer scenarios.
- Add one contention scenario that validates single-instance lock behavior.

## Notes

- Function counts are approximate and may vary with refactors, multiline signatures, and build-time conditionals.
- Coverage percentages are functional estimates based on active L2 test behavior, not instrumentation-based gcov/lcov measurement.
- Proposed scenarios are intended to close identified behavior and path gaps toward the 80% target.
