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

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
