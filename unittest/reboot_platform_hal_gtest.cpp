#include <gtest/gtest.h>
extern "C" {
    #include "update-reboot-info.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_platform_hal_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

TEST(PlatformHalSmokeTest, get_hardware_reason_brcm_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_brcm(nullptr, nullptr), ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_rtk_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_rtk(nullptr, nullptr), ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_amlogic_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_amlogic(nullptr, nullptr), ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_mtk_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_mtk(nullptr, nullptr), ERROR_GENERAL);
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
