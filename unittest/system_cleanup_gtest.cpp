#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstddef>
#include <cstdarg>
#include <unistd.h>
extern "C" {
#include "reboot.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_system_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
static std::vector<std::string> g_cmds;

extern "C" int v_secure_system(const char* fmt, ...){
    std::string cmd = fmt ? std::string(fmt) : std::string();
    g_cmds.push_back(cmd.empty() ? "call" : cmd);
    return 0;
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

TEST(SystemCleanup, SyncLogsSamePathNoOp){
    ASSERT_EQ(0, system("mkdir -p ./same_logs"));
    setenv("PERSISTENT_PATH", "./persistent", 1);
    setenv("TEMP_LOG_PATH", "./same_logs", 1);
    setenv("LOG_PATH", "./same_logs", 1);
    cleanup_services();
}

TEST(SystemCleanup, SystimeFileCopy){
    ASSERT_EQ(0, system("mkdir -p ./tmp_logs2"));
    ASSERT_EQ(0, system("mkdir -p ./persistent2"));
    std::ofstream st("./tmp_logs2/.systime"); st << "1700000000\n"; st.close();
    setenv("PERSISTENT_PATH", "./persistent2", 1);
    setenv("TEMP_LOG_PATH", "./tmp_logs2", 1);
    setenv("LOG_PATH", "./logs_out2", 1);
    cleanup_services();
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


TEST(SystemCleanup, PidfilePathIsDirectoryReturnsError){
    system("rm -rf /tmp/.rebootNow.pid");
    ASSERT_EQ(0, system("mkdir -p /tmp/.rebootNow.pid"));
    ASSERT_EQ(-1, pidfile_write_and_guard());
    system("rm -rf /tmp/.rebootNow.pid");
}

TEST(SystemCleanup, TempLogPathMissingHandledGracefully){
    setenv("PERSISTENT_PATH", "./persistent_missing", 1);
    setenv("TEMP_LOG_PATH", "./does_not_exist_temp_logs", 1);
    setenv("LOG_PATH", "./logs_missing", 1);
    cleanup_services();
    SUCCEED();
}

TEST(SystemCleanup, RdmCleanupRemovesMediaAppsChildren){
    ASSERT_EQ(0, system("mkdir -p /etc/rdm"));
    ASSERT_EQ(0, system("mkdir -p /media/apps/rdm/downloads/cert-test-bundle"));

    std::ofstream manifest("/etc/rdm/rdm-manifest.xml");
    manifest << "<manifest/>\n";
    manifest.close();

    std::ofstream flashed("/opt/cdl_flashed_file_name");
    flashed << "image-B\n";
    flashed.close();

    std::ofstream running("/tmp/currently_running_image_name");
    running << "image-A\n";
    running.close();

    std::ofstream appFile("/media/apps/rdm/downloads/cert-test-bundle/pkg.log");
    appFile << "payload\n";
    appFile.close();

    cleanup_services();

    ASSERT_NE(0, access("/media/apps/rdm", F_OK));

    system("rm -rf /media/apps");
    remove("/etc/rdm/rdm-manifest.xml");
    remove("/opt/cdl_flashed_file_name");
    remove("/tmp/currently_running_image_name");
}

TEST(SystemCleanup, DeviceScriptsInvokedWhenPresent){
    ASSERT_EQ(0, system("mkdir -p /lib/rdk"));

    std::ofstream emmc("/lib/rdk/eMMC_Upgrade.sh"); emmc << "#!/bin/sh\nexit 0\n"; emmc.close();
    std::ofstream aps4("/lib/rdk/aps4_reset.sh"); aps4 << "#!/bin/sh\nexit 0\n"; aps4.close();
    std::ofstream www("/lib/rdk/update_www-backup.sh"); www << "#!/bin/sh\nexit 0\n"; www.close();

    setenv("DEVICE_NAME", "XiOne", 1);
    g_cmds.clear();
    cleanup_services();

    bool emmc_called = false;
    bool aps4_called = false;
    bool www_called = false;
    for (const auto& c : g_cmds) {
        if (c.find("eMMC_Upgrade.sh") != std::string::npos) emmc_called = true;
        if (c.find("aps4_reset.sh") != std::string::npos) aps4_called = true;
        if (c.find("update_www-backup.sh") != std::string::npos) www_called = true;
    }
    ASSERT_TRUE(emmc_called);
    ASSERT_TRUE(aps4_called);
    ASSERT_TRUE(www_called);

    remove("/lib/rdk/eMMC_Upgrade.sh");
    remove("/lib/rdk/aps4_reset.sh");
    remove("/lib/rdk/update_www-backup.sh");
}

TEST(SystemCleanup, SyncLogsCopiesSupportedFilesOnly){
    ASSERT_EQ(0, system("mkdir -p ./tmp_logs_cov"));
    ASSERT_EQ(0, system("mkdir -p ./logs_out_cov"));

    std::ofstream f1("./tmp_logs_cov/a.log"); f1 << "AAA"; f1.close();
    std::ofstream f2("./tmp_logs_cov/b.txt"); f2 << "BBB"; f2.close();
    std::ofstream f3("./tmp_logs_cov/c.bin"); f3 << "CCC"; f3.close();

    setenv("PERSISTENT_PATH", "./persistent_cov", 1);
    setenv("TEMP_LOG_PATH", "./tmp_logs_cov", 1);
    setenv("LOG_PATH", "./logs_out_cov", 1);

    cleanup_services();

    std::ifstream outA("./logs_out_cov/a.log"); std::string sA; std::getline(outA, sA);
    std::ifstream outB("./logs_out_cov/b.txt"); std::string sB; std::getline(outB, sB);
    std::ifstream outC("./logs_out_cov/c.bin");

    ASSERT_EQ(sA, "AAA");
    ASSERT_EQ(sB, "BBB");
    ASSERT_FALSE(outC.good());
}

TEST(SystemCleanup, SyncSkippedWhenLightSleepKillSwitchExists){
    ASSERT_EQ(0, system("mkdir -p /opt/persistent"));
    std::ofstream ks("/opt/persistent/.lightsleepKillSwitchEnable"); ks << "1\n"; ks.close();

    ASSERT_EQ(0, system("mkdir -p ./tmp_logs_skip"));
    ASSERT_EQ(0, system("mkdir -p ./logs_out_skip"));
    std::ofstream inA("./tmp_logs_skip/a.log"); inA << "AAA"; inA.close();

    setenv("PERSISTENT_PATH", "./persistent_skip", 1);
    setenv("TEMP_LOG_PATH", "./tmp_logs_skip", 1);
    setenv("LOG_PATH", "./logs_out_skip", 1);

    cleanup_services();

    std::ifstream outA("./logs_out_skip/a.log");
    ASSERT_FALSE(outA.good());

    remove("/opt/persistent/.lightsleepKillSwitchEnable");
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
