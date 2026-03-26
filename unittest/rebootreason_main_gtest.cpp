/**
 * Minimal gtest for main.c
 */
#include <gtest/gtest.h>
extern "C" {
    #include "update-reboot-info.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_main_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

// Smoke tests for helpers used by main.c
TEST(MainSmokeTest, acquire_release_lock_InvalidParams) {
    EXPECT_EQ(acquire_lock(nullptr), ERROR_GENERAL);
    EXPECT_EQ(release_lock(nullptr), ERROR_GENERAL);
}

// Note: We don't call main() directly; this suite ensures build/link of main.c
// via Makefile.am target using the same compilation unit.

GTEST_API_ int main(int argc, char *argv[]) {
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s",
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
