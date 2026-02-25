#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
extern "C" {
#include "rebootNow.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_cyclic_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;

static bool g_det_enable=true; static int g_duration=0; static bool g_reboot_stop_enable=false;                                                                                                                                                                           extern "C" {                                                                                                                         bool rbus_get_bool_param(const char* name, bool* value){ if(std::string(name).find("RebootStop.Detection")!=std::string::npos){ *value=g_det_enable; return true; } if(std::string(name).find("RebootStop.Enable")!=std::string::npos){ *value=g_reboot_stop_enable; return true; } return false; }                                                                                                            bool rbus_get_int_param(const char* name, int* value){ if(std::string(name).find("RebootStop.Duration")!=std::string::npos){ *value=g_duration; return true; } return false; }
bool rbus_set_bool_param(const char* name, bool value){ if(std::string(name).find("RebootStop.Enable")!=std::string::npos){ g_reboot_stop_enable=value; return true; } return false; }
void t2CountNotify(const char* marker, int val){ (void)marker; (void)val; }
}

TEST(CyclicReboot, ProceedWhenNoFlag){
    // Ensure rebootNow flag not present
    remove("/opt/secure/reboot/rebootNow");
    ASSERT_EQ(handle_cyclic_reboot("Src","Reason","Custom","Other"), 1);
}

TEST(CyclicReboot, DeferOnLoopDetection){
    // Create rebootNow flag and previous info files
    system("mkdir -p /opt/secure/reboot");
    std::ofstream rf("/opt/secure/reboot/rebootNow"); rf<<""; rf.close();
    std::ofstream pf("/opt/secure/reboot/previousreboot.info");
    pf << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"Reason\",\n\"customReason\":\"Custom\",\n\"otherReason\":\"Other\"\n}";
    pf.close();
    // Simulate short uptime by writing to /proc/uptime via stub? We cannot, but the function tolerates failure.
    g_det_enable=true; g_duration=10;
    // Create counter file with threshold
    std::ofstream cf("/opt/secure/reboot/rebootCounter"); cf<<"5\n"; cf.close();
    remove("/opt/secure/reboot/rebootStop");

    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 0); // defer reboot
}

TEST(CyclicReboot, DetectionDisabledProceed){
    // Prepare rebootNow flag and same previous info
    system("mkdir -p /opt/secure/reboot");
    std::ofstream rf2("/opt/secure/reboot/rebootNow"); rf2<<""; rf2.close();
    std::ofstream pf2("/opt/secure/reboot/previousreboot.info");
    pf2 << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"Reason\",\n\"customReason\":\"Custom\",\n\"otherReason\":\"Other\"\n}";
    pf2.close();
    // Disable detection
    g_det_enable=false; g_duration=0;
    remove("/opt/secure/reboot/rebootStop");
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 1); // proceed immediately when detection disabled
}

TEST(CyclicReboot, SameReasonStopFlagPresent){
    system("mkdir -p /opt/secure/reboot");
    // Ensure rebootNow flag exists
    std::ofstream rf3("/opt/secure/reboot/rebootNow"); rf3<<""; rf3.close();
    // Create previous info with same reasons
    std::ofstream pf3("/opt/secure/reboot/previousreboot.info");
    pf3 << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"Reason\",\n\"customReason\":\"Custom\",\n\"otherReason\":\"Other\"\n}";
    pf3.close();
    // Pre-create stop flag to exercise immediate halt branch
    std::ofstream sf("/opt/secure/reboot/rebootStop"); sf<<""; sf.close();
    g_det_enable=true; g_duration=10;
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 0); // defer due to existing stop flag
}

TEST(CyclicReboot, IncrementCounterBelowThresholdProceed){
    system("mkdir -p /opt/secure/reboot");
    std::ofstream rf4("/opt/secure/reboot/rebootNow"); rf4<<""; rf4.close();
    std::ofstream pf4("/opt/secure/reboot/previousreboot.info");
    pf4 << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"Reason\",\n\"customReason\":\"Custom\",\n\"otherReason\":\"Other\"\n}";
    pf4.close();
    // Set count below threshold
    std::ofstream cf2("/opt/secure/reboot/rebootCounter"); cf2<<"3\n"; cf2.close();
    remove("/opt/secure/reboot/rebootStop");
    g_det_enable=true; g_duration=10;
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 1); // proceed when below threshold
}

TEST(CyclicReboot, PreviousInfoMissingProceedsAndResets){
    system("mkdir -p /opt/secure/reboot");
    std::ofstream rf5("/opt/secure/reboot/rebootNow"); rf5<<""; rf5.close();
    // Ensure previous info missing
    remove("/opt/secure/reboot/previousreboot.info");
    // Create stop flag and nonzero counter to verify reset path
    std::ofstream sf2("/opt/secure/reboot/rebootStop"); sf2<<""; sf2.close();
    std::ofstream cf3("/opt/secure/reboot/rebootCounter"); cf3<<"7\n"; cf3.close();
    g_det_enable=true; g_duration=0;
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 1);
    // stop flag should be removed; counter reset to 0
    ASSERT_EQ(std::ifstream("/opt/secure/reboot/rebootStop").good(), false);
    std::ifstream cr("/opt/secure/reboot/rebootCounter"); std::string cnt; std::getline(cr, cnt);
    ASSERT_EQ(cnt, "0");
}

TEST(CyclicReboot, DifferentReasonResetsCounter){
    system("mkdir -p /opt/secure/reboot");
    std::ofstream rf6("/opt/secure/reboot/rebootNow"); rf6<<""; rf6.close();
    // Previous info with different reason
    std::ofstream pf6("/opt/secure/reboot/previousreboot.info");
    pf6 << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"PrevReason\",\n\"customReason\":\"Custom\",\n\"otherReason\":\"Other\"\n}";
    pf6.close();
    // Prepare nonzero counter and existing stop flag
    std::ofstream cf4("/opt/secure/reboot/rebootCounter"); cf4<<"9\n"; cf4.close();
    std::ofstream sf3("/opt/secure/reboot/rebootStop"); sf3<<""; sf3.close();
    g_det_enable=true; g_duration=15;
    int rc = handle_cyclic_reboot("Src","NewReason","Custom","Other");
    ASSERT_EQ(rc, 1);
    // stop flag removed and counter reset
    ASSERT_EQ(std::ifstream("/opt/secure/reboot/rebootStop").good(), false);
    std::ifstream cr2("/opt/secure/reboot/rebootCounter"); std::string cnt2; std::getline(cr2, cnt2);
    ASSERT_EQ(cnt2, "0");
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
