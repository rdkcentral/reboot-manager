#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <fstream>
extern "C" {
#include <stdio.h>
#include "reboot.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_utils_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

static int g_t2_d_count = 0;
static int g_t2_s_count = 0;
static bool g_fail_fputs = false;

extern "C" void t2_event_d(const char* marker, int val)
{
    (void)marker;
    (void)val;
    g_t2_d_count++;
}

extern "C" void t2_event_s(const char* marker, const char* val)
{
    (void)marker;
    (void)val;
    g_t2_s_count++;
}

extern "C" int __real_fputs(const char* line, FILE* stream);
extern "C" int __wrap_fputs(const char* line, FILE* stream)
{
    if (g_fail_fputs) {
        return EOF;
    }
    return __real_fputs(line, stream);
}

TEST(UtilsTest, TimestampUpdate_Format)
{
    char buf[64] = {0};
    timestamp_update(buf, sizeof(buf));
    ASSERT_GT(strlen(buf), 0u);
    ASSERT_THAT(std::string(buf), ::testing::HasSubstr("UTC"));
}

TEST(UtilsTest, AppendLineToFile_Readback)
{
    const char* path = "./tmp_utils_test.log";
    remove(path);
    ASSERT_EQ(write_rebootinfo_log(path, "hello\n"), 0);
    std::ifstream in(path);
    std::string content;
    std::getline(in, content);
    ASSERT_EQ(content, "hello");
    remove(path);
}

TEST(UtilsTest, WriteRebootInfoLog_InvalidPath)
{
    ASSERT_EQ(-1, write_rebootinfo_log("/invalid/path/doesnotexist.log", "x\n"));
}

TEST(UtilsTest, WriteRebootInfoLog_FputsFailure)
{
    const char* path = "./tmp_utils_test.log";
    remove(path);
    g_fail_fputs = true;

    EXPECT_EQ(write_rebootinfo_log(path, "x\n"), -1);

    g_fail_fputs = false;
    remove(path);
}

TEST(UtilsTest, TimestampUpdate_TinyBuffer)
{
    char buf[1] = {'x'};
    timestamp_update(buf, sizeof(buf));
    ASSERT_EQ(buf[0], '\0');
}

TEST(UtilsTest, TelemetryWrappers)
{
    g_t2_d_count = 0;
    g_t2_s_count = 0;

    t2CountNotify("TEST_MARKER", 1);
    t2ValNotify("TEST_MARKER", "TEST_VALUE");

    ASSERT_EQ(g_t2_d_count, 0);
    ASSERT_EQ(g_t2_s_count, 0);
}

TEST(UtilsTest, TelemetryWrappersIgnoreInvalidValues)
{
    g_t2_d_count = 0;
    g_t2_s_count = 0;

    t2CountNotify(nullptr, 1);
    t2CountNotify("", 1);
    t2ValNotify(nullptr, "TEST_VALUE");
    t2ValNotify("", "TEST_VALUE");
    t2ValNotify("TEST_MARKER", nullptr);

    ASSERT_EQ(g_t2_d_count, 0);
    ASSERT_EQ(g_t2_s_count, 0);
}

TEST(UtilsTest, RfcHelpersAreInvoked)
{
    char value_buffer[16] = {0};
    bool bool_value = false;
    int int_value = 0;

    EXPECT_FALSE(rfc_get_string_param("Device.Test.String", value_buffer, sizeof(value_buffer)));
    EXPECT_FALSE(rfc_get_bool_param("Device.Test.Bool", &bool_value));
    EXPECT_FALSE(rfc_get_int_param("Device.Test.Int", &int_value));
    EXPECT_TRUE(rfc_set_bool_param("Device.Test.Bool", true));
    EXPECT_TRUE(rfc_set_int_param("Device.Test.Int", 1));
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
