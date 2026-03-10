#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
extern "C" {
    #include "update-reboot-info.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_json_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

TEST(JsonSmokeTest, write_reboot_info_InvalidParams) {
    EXPECT_EQ(write_reboot_info(nullptr, nullptr), ERROR_GENERAL);
}

TEST(JsonSmokeTest, write_hardpower_InvalidParams) {
    EXPECT_EQ(write_hardpower(nullptr, nullptr), ERROR_GENERAL);
}

TEST(JsonSmokeTest, acquire_and_release_lock_Success) {
    const char* lockFile = "/tmp/reboot_test_lockfile.lock";
    EXPECT_EQ(acquire_lock(lockFile), SUCCESS);
    EXPECT_EQ(release_lock(lockFile), SUCCESS);
    remove(lockFile);
}

TEST(JsonSmokeTest, write_reboot_info_Success) {
    const char* outFile = "/tmp/reboot_test_info.json";
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T00:00:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.source, "Kernel", sizeof(info.source) - 1);
    strncpy(info.reason, "KERNEL_PANIC", sizeof(info.reason) - 1);
    strncpy(info.customReason, "Hardware Register - KERNEL_PANIC", sizeof(info.customReason) - 1);
    strncpy(info.otherReason, "Reboot due to Kernel Panic captured by Oops Dump", sizeof(info.otherReason) - 1);

    EXPECT_EQ(write_reboot_info(outFile, &info), SUCCESS);

    FILE* fp = fopen(outFile, "r");
    ASSERT_NE(fp, nullptr);
    char content[1024] = {0};
    size_t n = fread(content, 1, sizeof(content) - 1, fp);
    content[n] = '\0';
    fclose(fp);

    EXPECT_NE(strstr(content, "\"timestamp\":\"2026-03-10T00:00:00Z\""), nullptr);
    EXPECT_NE(strstr(content, "\"source\":\"Kernel\""), nullptr);
    EXPECT_NE(strstr(content, "\"reason\":\"KERNEL_PANIC\""), nullptr);
    remove(outFile);
}

TEST(JsonSmokeTest, write_hardpower_Success) {
    const char* outFile = "/tmp/reboot_test_hardpower.json";
    const char* timestamp = "2026-03-10T01:23:45Z";
    EXPECT_EQ(write_hardpower(outFile, timestamp), SUCCESS);

    FILE* fp = fopen(outFile, "r");
    ASSERT_NE(fp, nullptr);
    char content[256] = {0};
    size_t n = fread(content, 1, sizeof(content) - 1, fp);
    content[n] = '\0';
    fclose(fp);

    EXPECT_NE(strstr(content, "\"lastHardPowerReset\":\"2026-03-10T01:23:45Z\""), nullptr);
    remove(outFile);
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
