#!/bin/bash
ENABLE_COV=true
if [ "x${1:-}" = "x--disable-cov" ]; then ENABLE_COV=false; fi

TOP_DIR=$(pwd)
HELPER_SRC_DIR="${HELPER_SRC_DIR:-../reboot-helper/src}"
FETCHER_SRC_DIR="${FETCHER_SRC_DIR:-../reboot-reason-fetcher/src}"
cd unittest/

if [ -f Makefile ]; then
  make distclean || make clean || true
fi
automake --add-missing || true
autoreconf --force --install
./configure

if [ "$ENABLE_COV" = true ]; then
  export CXXFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
  export CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
  export LDFLAGS="-lgcov --coverage"
fi

make clean
## Remove stale coverage data to avoid gcov timestamp errors
find . -name '*.gcda' -delete || true
find ../reboot-helper/src -name '*.gcda' -delete || true
find ../reboot-reason-fetcher/src -name '*.gcda' -delete || true
make -j$(nproc || echo 2)

if [ ! -x ./reboot_main_gtest ]; then
  echo "reboot_main_gtest was not built"
  exit 1
fi

fail=0
mkdir -p /tmp/Gtest_Report

TC_RESULTS=""
record_tc_result() {
    tc_id="$1"
    tc_summary="$2"
    tc_status="$3"
    if [ -z "$TC_RESULTS" ]; then
        TC_RESULTS="${tc_id}|${tc_summary}|${tc_status}"
    else
        TC_RESULTS=$(printf '%s\n%s|%s|%s' "$TC_RESULTS" "$tc_id" "$tc_summary" "$tc_status")
    fi
}

print_tc_summary() {
    line="===================================================================================="
    sep="+---------+------------------------------------------------------+-----------------+"
    total=0
    passed=0
    failed=0
    skipped=0

    echo "$line"
    echo "Test Summary"
    echo "$line"
    echo "$sep"
    printf '| %-7s | %-52s | %-15s |\n' "TC ID" "Small summary" "Success/Failure"
    echo "$sep"

    while IFS='|' read -r tc_id tc_summary tc_status; do
        [ -z "$tc_id" ] && continue
        total=$((total + 1))
        case "$tc_status" in
            SUCCESS) passed=$((passed + 1)) ;;
            SKIPPED) skipped=$((skipped + 1)) ;;
            *)       failed=$((failed + 1)) ;;
        esac
        printf '| %-7s | %-52.52s | %-15s |\n' "$tc_id" "$tc_summary" "$tc_status"
    done <<EOF
$TC_RESULTS
EOF
    echo "$sep"
    if [ "$failed" -eq 0 ]; then
        banner="ALL TESTS PASSED"
    else
        banner="SOME TESTS FAILED"
    fi
    echo "$banner | PASSED $passed/$total | FAILED $failed/$total | SKIPPED $skipped/$total |"
    echo "$line"
}

tc_index=0
run_gtest() {
    binary="$1"
    log="/tmp/Gtest_Report/$(basename "$binary").log"
    echo "Running $binary"
    $binary > "$log" 2>&1
    run_status=$?
    cat "$log"
    if [ "$run_status" -ne 0 ]; then
        fail=1
    fi

    # Per-case result lines end with the elapsed time; the failure recap does not.
    while IFS= read -r result_line; do
        case "$result_line" in
            \[*OK*\]*)      tc_status="SUCCESS" ;;
            \[*FAILED*\]*)  tc_status="FAILURE" ;;
            \[*SKIPPED*\]*) tc_status="SKIPPED" ;;
            *) continue ;;
        esac
        tc_name=$(printf '%s' "$result_line" | sed -E 's/^\[[^]]*\] ([^ ]+).*/\1/')
        tc_index=$((tc_index + 1))
        record_tc_result "$tc_index" "$tc_name" "$tc_status"
    done <<EOF
$(grep -E '^\[ *(OK|FAILED|SKIPPED) *\] .+ \([0-9]+ ms\)$' "$log")
EOF
}

run_gtest ./reboot_utils_gtest
run_gtest ./reboot_cyclic_gtest
run_gtest ./reboot_system_gtest
run_gtest ./reboot_main_gtest
run_gtest ./reboot_json_gtest
run_gtest ./reboot_parodus_gtest
run_gtest ./reboot_log_parser_gtest
run_gtest ./reboot_classify_gtest
run_gtest ./rebootreason_main_gtest

echo "Running focused reboot_main coverage test before LCOV capture"
./reboot_main_gtest --gtest_filter='RebootMain.SignalCleanupHandler' || fail=1

if [ "$ENABLE_COV" = true ]; then
  echo "Listing all .gcda files in unittest, src, and parent directories:"
  find . -name '*.gcda'
  find "$HELPER_SRC_DIR" -name '*.gcda'
  find "$FETCHER_SRC_DIR" -name '*.gcda'
  find .. -name '*.gcda'
  echo "Generating coverage report from both unittest and src directories"
  lcov --capture \
    --directory . \
    --directory "$HELPER_SRC_DIR" \
    --directory "$FETCHER_SRC_DIR" \
    --output-file coverage.info
  # Remove system and common test/mocks paths (keep build dir entries)
  lcov --remove coverage.info '/usr/*' --output-file coverage.info
  lcov --remove coverage.info '*/mocks/*' '*/gtest/*' '*/gmock/*' --output-file coverage.info
  # Restrict to product sources
  lcov --extract coverage.info '*/reboot-helper/src/*' '*/reboot-reason-fetcher/src/*' --output-file coverage.info
  lcov --list coverage.info
fi

print_tc_summary

cd "$TOP_DIR"

if [ $fail -ne 0 ]; then
  echo "Some unit tests failed."
  exit 1
fi
