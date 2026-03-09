#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <cstdio>

extern "C" {
    #include "update-reboot-info.h"
    #include "rdk_fwdl_utils.h"
    #include "rdk_debug.h"

    int g_rdk_logger_enabled = 0;

    int getDevicePropertyData(const char* key __attribute__((unused)),
                              char* value __attribute__((unused)),
                              int size __attribute__((unused))) {
        return UTILS_FAILURE; // trigger defaults in parse_device_properties
    }

    void t2_event_d(const char* marker __attribute__((unused)),
                    int value __attribute__((unused))) { }
    void t2_event_s(const char* marker __attribute__((unused)),
                    const char* value __attribute__((unused))) { }
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_log_parser_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
using namespace std;

class LogParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        system("mkdir -p /tmp/reboot_test");
    }

    void TearDown() override {
        // Clean up test files
        system("rm -rf /tmp/reboot_test");
    }

    void createTestLogFile(const char* path, const char* content) {
        FILE* fp = fopen(path, "w");
        ASSERT_NE(fp, nullptr);
        fprintf(fp, "%s", content);
        fclose(fp);
    }
};

// Tests for parse_device_properties
TEST_F(LogParserTest, parse_device_properties_NullContext) {
    EXPECT_EQ(parse_device_properties(nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, parse_device_properties_Success) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    // Create a test device.properties file
    createTestLogFile("/tmp/reboot_test/device.properties",
                      "SOC=BRCM\n"
                      "BUILD_TYPE=prod\n"
                      "DEVICE_TYPE=stb\n"
                      "PLATCO_SUPPORT=true\n"
                      "LLAMA_SUPPORT=false\n");

    // Since parse_device_properties reads from /etc/device.properties,
    // we test the fallback parsing path
    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
}

TEST_F(LogParserTest, parse_device_properties_InitializesDefaults) {
    EnvContext ctx;
    memset(&ctx, 0xff, sizeof(EnvContext)); // Fill with garbage

    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_FALSE(ctx.platcoSupport);
    EXPECT_FALSE(ctx.llamaSupport);
    EXPECT_FALSE(ctx.rebootInfoSttSupport);
}

// Tests for should_update_reboot_info
TEST_F(LogParserTest, update_reboot_info_NullContext) {
    EXPECT_EQ(update_reboot_info(nullptr), 0);
}

TEST_F(LogParserTest, update_reboot_info_PlatcoFirstInvocation) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.platcoSupport = true;

    // Remove the invoked flag
    system("rm -f /tmp/Update_rebootInfo_invoked");

    // Should allow update on first invocation
    EXPECT_EQ(update_reboot_info(&ctx), 1);
}

TEST_F(LogParserTest, update_reboot_info_RequiresFlagsAfterFirstInvoke) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.platcoSupport = true;

    // Create invoked flag
    createTestLogFile("/tmp/Update_rebootInfo_invoked", "1\n");

    // Remove other flags
    system("rm -f /tmp/stt_received /tmp/rebootInfo_Updated");

    // Should block update without required flags
    EXPECT_EQ(update_reboot_info(&ctx), 0);

    // Clean up
    system("rm -f /tmp/Update_rebootInfo_invoked");
}

TEST_F(LogParserTest, update_reboot_info_AllowsWithFlags) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.platcoSupport = true;

    // Create all required flags
    createTestLogFile("/tmp/Update_rebootInfo_invoked", "1\n");
    createTestLogFile("/tmp/stt_received", "1\n");
    createTestLogFile("/tmp/rebootInfo_Updated", "1\n");

    EXPECT_EQ(update_reboot_info(&ctx), 1);

    // Clean up
    system("rm -f /tmp/Update_rebootInfo_invoked /tmp/stt_received /tmp/rebootInfo_Updated");
}

// Tests for parse_legacy_log
TEST_F(LogParserTest, parse_legacy_log_NullParameters) {
    RebootInfo info;
    EXPECT_EQ(parse_legacy_log(nullptr, &info), ERROR_GENERAL);
    EXPECT_EQ(parse_legacy_log("/tmp/test.log", nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, parse_legacy_log_FileNotFound) {
    RebootInfo info;
    EXPECT_EQ(parse_legacy_log("/tmp/nonexistent_file.log", &info), ERROR_FILE_NOT_FOUND);
}

TEST_F(LogParserTest, parse_legacy_log_EmptyFile) {
    const char* testFile = "/tmp/reboot_test/empty.log";
    createTestLogFile(testFile, "");

    RebootInfo info;
    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, ERROR_PARSE_FAILED);
}


TEST_F(LogParserTest, parse_legacy_log_Success) {
    const char* testFile = "/tmp/reboot_test/reboot.log";
    createTestLogFile(testFile,
                      "PreviousRebootInitiatedBy: SystemService\n"
                      "PreviousRebootTime: 2025-11-28 10:30:00 UTC\n"
                      "PreviousCustomReason: ScheduledReboot\n"
                      "PreviousOtherReason: Reboot due to scheduled maintenance\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));

    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "SystemService");
    EXPECT_STREQ(info.timestamp, "2025-11-28 10:30:00 UTC");
    EXPECT_STREQ(info.customReason, "ScheduledReboot");
    EXPECT_STREQ(info.otherReason, "Reboot due to scheduled maintenance");
}

TEST_F(LogParserTest, parse_legacy_log_PartialData) {
    const char* testFile = "/tmp/reboot_test/partial.log";
    createTestLogFile(testFile,
                      "PreviousRebootInitiatedBy: WatchDog\n"
                      "Some other line\n"
                      "PreviousCustomReason: WATCHDOG_RESET\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));

    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "WatchDog");
    EXPECT_STREQ(info.customReason, "WATCHDOG_RESET");
}

// Tests for read_brcm_previous_reboot_reason
TEST_F(LogParserTest, read_brcm_previous_reboot_reason_NullParameter) {
    EXPECT_EQ(read_brcm_previous_reboot_reason(nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, read_brcm_previous_reboot_reason_FileNotFound) {
    HardwareReason hw;
    // This will fail since /proc/brcm/previous_reboot_reason doesn't exist in test
    int result = read_brcm_previous_reboot_reason(&hw);
    EXPECT_EQ(result, ERROR_FILE_NOT_FOUND);
}

// Tests for read_rtk_wakeup_reason
TEST_F(LogParserTest, read_rtk_wakeup_reason_NullParameter) {
    EXPECT_EQ(read_rtk_wakeup_reason(nullptr), ERROR_GENERAL);
}

// Tests for read_amlogic_reset_reason
TEST_F(LogParserTest, read_amlogic_reset_reason_NullParameters) {
    HardwareReason hw;
    RebootInfo info;

    EXPECT_EQ(read_amlogic_reset_reason(nullptr, &info), ERROR_GENERAL);
    EXPECT_EQ(read_amlogic_reset_reason(&hw, nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, read_amlogic_reset_reason_FileNotFound) {
    HardwareReason hw;
    RebootInfo info;

    int result = read_amlogic_reset_reason(&hw, &info);
    EXPECT_EQ(result, ERROR_FILE_NOT_FOUND);
}

// Tests for read_mtk_reset_reason
TEST_F(LogParserTest, read_mtk_reset_reason_NullParameters) {
    HardwareReason hw;
    RebootInfo info;

    EXPECT_EQ(read_mtk_reset_reason(nullptr, &info), ERROR_GENERAL);
    EXPECT_EQ(read_mtk_reset_reason(&hw, nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, read_mtk_reset_reason_FileNotFound) {
    HardwareReason hw;
    RebootInfo info;

    int result = read_mtk_reset_reason(&hw, &info);
    EXPECT_EQ(result, FAILURE);
}

GTEST_API_ int main(int argc, char *argv[]) {
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];

    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s",
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);

    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);

    cout << "Starting REBOOT_LOG_PARSER GTEST ===================>" << endl;
    return RUN_ALL_TESTS();
}
