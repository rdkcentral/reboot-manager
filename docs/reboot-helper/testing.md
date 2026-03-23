# Build, Test, and Validation Guide

This document explains how to build and validate `reboot-manager` locally and in CI.
It is written for developers who are new to the component and need repeatable,
diagnostic-friendly test workflows.

## 1. Testing Objectives

Testing for `reboot-manager` verifies:

1. Correct reboot reason classification and metadata persistence.
2. Correct cyclic reboot loop detection/defer behavior.
3. Correct pre-reboot cleanup behavior.
4. Safe reboot orchestration and fallback behavior.
5. Interface correctness for RBUS and utility modules.

## 2. Test Layers

`reboot-manager` uses two primary test layers:

1. **L1 Unit tests (GTest)**
   - Fast feedback on module/function behavior.
   - Uses mocks/stubs for platform dependencies.

2. **L2 Functional tests (pytest)**
   - End-to-end behavioral tests for reboot flows.
   - Validates component interactions and file outputs.

## 3. Prerequisites

### 3.1 Build and Unit Test Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential autoconf automake libtool pkg-config \
  lcov gcovr python3-pip
```

Recommended Python packages:

```bash
pip3 install pytest pytest-json-report
```

### 3.2 Functional Test Runtime Notes

`run_l2.sh` expects:

- `pytest` + `pytest-json-report`
- `libjsonrpccpp-dev` (installed in script)
- `/usr/local/bin/update-reboot-info` executable (environment dependent)

If your local environment does not provide target platform services,
run L2 tests in a compatible integration environment/container.

## 4. Build Steps

From repository root:

```bash
autoreconf -fi
./configure
make -j$(nproc)
```

Optional feature flags:

```bash
./configure --enable-t2api --enable-breakpad --enable-cpc
```

Use these flags only when your environment provides the corresponding dependencies.

## 5. L1 Unit Testing (GTest)

### 5.1 Standard Execution

```bash
./unit_test.sh
```

This script performs:

1. cleanup/regeneration of autotools files in `unittest/`
2. optional coverage instrumentation setup
3. build of all gtest binaries
4. execution of all test binaries
5. lcov capture/listing (default mode)

### 5.2 Test Binaries Executed

- `reboot_utils_gtest`
- `reboot_rbus_gtest`
- `reboot_cyclic_gtest`
- `reboot_system_gtest`
- `reboot_main_gtest`

### 5.3 Coverage and Fast Mode

Run without coverage instrumentation:

```bash
./unit_test.sh --disable-cov
```

Use this mode for faster local iteration.

### 5.4 Expected Outcome

- Script exits `0` when all tests pass.
- Script exits non-zero when any binary fails.
- On coverage mode, `coverage.info` is generated and listed via `lcov`.

## 6. L2 Functional Testing (pytest)

### 6.1 Standard Execution

```bash
./run_l2.sh
```

The script runs selected pytest modules and emits JSON reports under `/tmp/l2_test_report`.

### 6.2 Functional Suites

Current test module set:

- `tests/functional_tests/test/test_cyclic_reboot.py`
- `tests/functional_tests/test/test_reboot_crash_maintenance.py`
- `tests/functional_tests/test/test_system_cleanup.py`
- `tests/functional_tests/test/test_reboot_triggered.py`

Feature specifications are in `tests/functional_tests/features/`.

### 6.3 Output Artifacts

- `/tmp/l2_test_report/cyclicreboot.json`
- `/tmp/l2_test_report/crashmaintainence.json`
- `/tmp/l2_test_report/systemcleanup.json`
- `/tmp/l2_test_report/rebootTest.json`

Review these JSON files for summary and per-test failures.

## 7. Recommended Validation Workflow

For code changes in this component, use this order:

1. Build the project.
2. Run `./unit_test.sh --disable-cov` for quick correctness.
3. Run full `./unit_test.sh` for coverage-enabled validation.
4. Run `./run_l2.sh` when behavior affects end-to-end reboot flows.

This minimizes cycle time while still giving strong confidence.

## 8. CI Mapping

GitHub workflows:

- `.github/workflows/L1-tests.yml`
- `.github/workflows/L2-tests.yml`

Use local commands in this document to reproduce CI failures quickly.

## 9. Related Documentation

- `docs/reboot-manager-hld.md`
- `docs/reboot-manager-lld.md`
- `docs/diagrams/flowchart.md`
- `docs/diagrams/sequence-diagram.md`

