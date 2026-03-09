#include <gtest/gtest.h>
extern "C" {
    #include "update-reboot-info.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_parodus_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

TEST(ParodusSmokeTest, append_kernel_reason_InvalidParams) {
    EXPECT_EQ(append_kernel_reason(nullptr, nullptr), ERROR_GENERAL);
}

TEST(ParodusSmokeTest, update_parodus_log_InvalidParams) {
    EXPECT_EQ(update_parodus_log(nullptr), ERROR_GENERAL);
}

TEST(ParodusSmokeTest, copy_keypress_info_InvalidParams) {
    EXPECT_EQ(copy_keypress_info(nullptr, nullptr), ERROR_GENERAL);
}

GTEST_API_ int main(int argc, char *argv[]) {
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s",
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
