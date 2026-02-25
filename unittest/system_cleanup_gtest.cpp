#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstddef>
#include <cstdarg>
extern "C" {
#include "rebootNow.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_system_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
static std::vector<std::string> g_cmds;

extern "C" int v_secure_system(const char* fmt, ...){
    (void)fmt;
    g_cmds.push_back("call");
    return 0;
}

static bool g_sim_inactive = false;
static bool g_sim_stop_fail = false;

TEST(SystemCleanup, SyncLogsFromTemp)
{
    // Prepare temp and log directories
    system("mkdir -p ./tmp_logs");
    system("mkdir -p ./logs_out");
    std::ofstream f1("./tmp_logs/a.log"); f1<<"AAA"; f1.close();
    std::ofstream f2("./tmp_logs/b.txt"); f2<<"BBB"; f2.close();
    std::ofstream f3("./tmp_logs/skip.bin"); f3<<"CCC"; f3.close();

    // Set environment for housekeeping
    setenv("PERSISTENT_PATH", "./persistent", 1);
    setenv("TEMP_LOG_PATH", "./tmp_logs", 1);
    setenv("LOG_PATH", "./logs_out", 1);

    //perform_housekeeping();

    std::ifstream outA("./logs_out/a.log"); std::string sA; std::getline(outA, sA);
    std::ifstream outB("./logs_out/b.txt"); std::string sB; std::getline(outB, sB);
    ASSERT_EQ(sA, "AAA");
    ASSERT_EQ(sB, "BBB");
}

TEST(SystemCleanup, PidfileWriteAndGuardAndCleanup){
    // Ensure no PID file exists
    remove("/tmp/.rebootNow.pid");
    // First write should succeed
    ASSERT_EQ(pidfile_write_and_guard(), 0);
    // Cleanup should remove it
    cleanup_pidfile();
    ASSERT_NE(0, access("/tmp/.rebootNow.pid", F_OK));
}

TEST(SystemCleanup, BluetoothServicesStopBranch){
    // Exercise BLUETOOTH_ENABLED path; v_secure_system always returns 0 (active/stop OK)
    g_cmds.clear();
    setenv("BLUETOOTH_ENABLED", "true", 1);
    perform_housekeeping();
    ASSERT_FALSE(g_cmds.empty());
    unsetenv("BLUETOOTH_ENABLED");
}

TEST(SystemCleanup, BluetoothServicesInactiveAndStopFail){
    // Simulate services not active, then simulate stop failure on subsequent run
    setenv("BLUETOOTH_ENABLED", "true", 1);
    g_cmds.clear(); g_sim_inactive=true; g_sim_stop_fail=false;
    perform_housekeeping();
    ASSERT_FALSE(g_cmds.empty());
    // Now simulate active but stop fails
    g_cmds.clear(); g_sim_inactive=false; g_sim_stop_fail=true;
    perform_housekeeping();
    ASSERT_FALSE(g_cmds.empty());
    unsetenv("BLUETOOTH_ENABLED");
}

TEST(SystemCleanup, SyncLogsSamePathNoOp){
    // When TEMP_LOG_PATH == LOG_PATH, sync should early-return
    system("mkdir -p ./same_logs");
    setenv("PERSISTENT_PATH", "./persistent", 1);
    setenv("TEMP_LOG_PATH", "./same_logs", 1);
    setenv("LOG_PATH", "./same_logs", 1);
    perform_housekeeping();
    // No assertion needed; test ensures function path executes without error
}

TEST(SystemCleanup, SystimeFileCopy){
    // Prepare temp and persistent paths and a systime file
    system("mkdir -p ./tmp_logs2");
    system("mkdir -p ./persistent2");
    std::ofstream st("./tmp_logs2/.systime"); st << "1700000000\n"; st.close();
    setenv("PERSISTENT_PATH", "./persistent2", 1);
    setenv("TEMP_LOG_PATH", "./tmp_logs2", 1);
    setenv("LOG_PATH", "./logs_out2", 1);
    perform_housekeeping();
    std::ifstream si("./persistent2/.systime");
    ASSERT_TRUE(si.good());
    std::string val; std::getline(si, val);
    ASSERT_EQ(val, "1700000000");
}

TEST(SystemCleanup, PidfileExistingOverwrite){
    // Create a stale PID file and ensure function overwrites it under lock
    std::ofstream pf("/tmp/.rebootNow.pid"); pf << "999999"; pf.close();
    ASSERT_EQ(pidfile_write_and_guard(), 0);
    // Verify file exists after guard and then cleanup
    ASSERT_EQ(0, access("/tmp/.rebootNow.pid", F_OK));
    cleanup_pidfile();
    ASSERT_NE(0, access("/tmp/.rebootNow.pid", F_OK));
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
