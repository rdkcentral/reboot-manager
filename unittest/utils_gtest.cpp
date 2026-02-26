#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <fstream>
extern "C" {
#include <stdio.h>
#include "rebootnow.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_utils_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

static int g_t2_d_count = 0;
static int g_t2_s_count = 0;

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

TEST(UtilsTest, TimestampUpdate_TinyBuffer)
{
    char buf[1] = {'x'};
    timestamp_update(buf, sizeof(buf));
    ASSERT_EQ(buf[0], '\0');
}

TEST(UtilsTest, TelemetryNoopHelpers)
{
    g_t2_d_count = 0;
    g_t2_s_count = 0;
    t2CountNotify("UTILS_MARKER", 1);
    t2ValNotify("UTILS_MARKER", "VAL");
    ASSERT_EQ(g_t2_d_count, 1);
    ASSERT_EQ(g_t2_s_count, 1);

    t2CountNotify(nullptr, 1);
    t2ValNotify(nullptr, "VAL");
    t2ValNotify("UTILS_MARKER", nullptr);
    ASSERT_EQ(g_t2_d_count, 1);
    ASSERT_EQ(g_t2_s_count, 1);
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
