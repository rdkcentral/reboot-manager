#!/bin/bash
ENABLE_COV=true
if [ "x${1:-}" = "x--disable-cov" ]; then ENABLE_COV=false; fi

TOP_DIR=$(pwd)
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
find ../src -name '*.gcda' -delete || true
make -j$(nproc || echo 2)

fail=0
for test in \
  ./reboot_utils_gtest \
  ./reboot_rbus_gtest \
  ./reboot_cyclic_gtest \
  ./reboot_system_gtest \
  ./reboot_main_gtest \
  ./reboot_json_gtest \
  ./reboot_parodus_gtest \
  ./reboot_platform_hal_gtest \
  ./reboot_log_parser_gtest \
  ./reboot_classify_gtest \
  ./rebootreason_main_gtest
do
  echo "Running $test"
  $test || fail=1
done

if [ $fail -ne 0 ]; then
  echo "Some unit tests failed."
  exit 1
fi

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

cd "$TOP_DIR"
