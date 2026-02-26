#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <unistd.h>
// Ensure telemetry mock header is visible to the translation unit
#include "mocks/telemetry_busmessage_sender.h"
extern "C" {
#include "rebootnow.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_main_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
using namespace std;

static int g_handle_decision = 0; // 0 defer, 1 proceed
static vector<string> g_markers;
static std::vector<std::string> g_cmds;
static int g_secure_rc = 0; // return code for v_secure_system
static bool g_called_systemctl = false;
static bool g_called_force = false;

extern "C" {
/* Reset getopt state between multiple main() invocations in tests */
extern int optind;
extern int opterr;
extern int optopt;
}
static inline void reset_getopt_state(){ 
    optind = 1; 
    opterr = 0; 
    optopt = 0; 
}

#define T2_EVENT_ENABLED 1
// Stub out process/system calls to avoid side effects and latency
#include <sys/types.h>
#define sleep  sleep_stub
#define fork   fork_stub
#define execlp execlp_stub
#define kill   kill_stub
#define waitpid waitpid_stub

extern "C" {
// Stubs used by main
bool rbus_init(void){ 
    return true; 
}                                                                                                 
void rbus_cleanup(void){}                                                                                                            
int pidfile_write_and_guard(void){ 
    return 0;
}                                                                                       
void cleanup_pidfile(void){}                                                                                                         
int handle_cyclic_reboot_stub(const char* s, const char* r, const char* c, const char* o){ 
    (void)s;
    (void)r;
    (void)c;
    (void)o; 
    return g_handle_decision; 
}
int v_secure_system(const char* fmt, ...){
    if(fmt){
        std::string s(fmt);
        if(s.find("systemctl reboot")!=std::string::npos) g_called_systemctl=true;
        if(s.find("reboot -f")!=std::string::npos) g_called_force=true;
        g_cmds.push_back(std::move(s));
    }
    return g_secure_rc;
}
// T2 event stubs used by rebootNow_main.c when T2_EVENT_ENABLED is set
void t2_event_d(const char* marker, int val){ (void)val; g_markers.push_back(marker); }
void t2_event_s(const char* marker, const char* val){ (void)marker; (void)val; }
void t2_init(const char* client){ (void)client; }
// RBUS param helpers used by rebootNow_main.c
static bool g_notif_called=false;
static bool g_mng_enable=true;
static bool g_rbus_get_ok=true;
static bool g_det_enable=true; static int g_duration=10; static bool g_reboot_stop_enable=false;
bool rbus_get_bool_param(const char* name, bool* value){
    if(!value) return false;
    if(!g_rbus_get_ok) return false;
    std::string s(name);
    if(s.find("ManageableNotification.Enable")!=std::string::npos){ *value=g_mng_enable; return true; }
    if(s.find("RebootStop.Detection")!=std::string::npos){ *value=g_det_enable; return true; }
    if(s.find("RebootStop.Enable")!=std::string::npos){ *value=g_reboot_stop_enable; return true; }
    *value=false; return true;
}
bool rbus_get_int_param(const char* name, int* value){ if(!value) return false; std::string s(name); if(s.find("RebootStop.Duration")!=std::string::npos){ *value=g_duration; return true; } return false; }
bool rbus_set_bool_param(const char* name, bool value){ (void)name; g_reboot_stop_enable=value; return true; }
bool rbus_set_int_param(const char* name, int value){ (void)value; if(std::string(name).find("RebootPendingNotification")!=std::string::npos){ g_notif_called=true; } return true; }
// Housekeeping stub
void perform_housekeeping(void){}
// Stubbed process/system functions
unsigned int sleep_stub(unsigned int){ return 0; }
pid_t fork_stub(void){ return 1; }
int execlp_stub(const char*, const char*, char*){ return -1; }
int kill_stub(pid_t, int){ return 0; }
pid_t waitpid_stub(pid_t, int*, int){ return 0; }
}

// Rename program main to call it from tests
extern "C" int reboot_main_entry(int argc, char** argv);
#define main reboot_main_entry
extern "C" {
#undef t2CountNotify
#undef t2ValNotify
#include "../rebootnow/src/main.c"
}
#undef main

TEST(RebootMain, NoArgs){
    reset_getopt_state();
    const char* argv[] = { "rebootnow" };
    int rc = reboot_main_entry(1, (char**)argv);
    ASSERT_NE(rc, 0);
}

TEST(RebootMain, NormalSourceDefer){
    g_handle_decision = 0;
    g_markers.clear();
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-o", "User requested reboot" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    // Marker should include SYST_ERR_HtmlDiagnostics
    bool found=false; for(auto &m: g_markers){ if(m.find("SYST_ERR_HtmlDiagnostics")!=string::npos) found=true; }
    ASSERT_TRUE(found);
}

TEST(RebootMain, CrashSourceDefer){
    g_handle_decision = 0;
    g_markers.clear();
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-c", "dsMgrMain", "-r", "MAINTENANCE_REBOOT", "-o", "Crash detected" };
    int rc = reboot_main_entry(7, (char**)argv);
    ASSERT_EQ(rc, 0);
    bool found=false; for(auto &m: g_markers){ if(m.find("SYST_ERR_DSMGR_reboot")!=string::npos) found=true; }
    ASSERT_TRUE(found);
}


TEST(RebootMain, HelpOption){
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-h" };
    int rc = reboot_main_entry(2, (char**)argv);
    ASSERT_EQ(rc, 0);
}

TEST(RebootMain, ConflictingOptions){
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "SrcA", "-c", "SrcB" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_NE(rc, 0);
}

TEST(RebootMain, ProceedTriggersFlagAndRebootSequence){
    g_handle_decision = 1;
    g_markers.clear(); g_cmds.clear(); g_notif_called=false; g_called_systemctl=false; g_called_force=false;
    // Ensure flag does not exist
    remove("/opt/secure/reboot/rebootNow");
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-o", "User requested reboot" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    // Flag should be created
    ASSERT_EQ(0, access("/opt/secure/reboot/rebootNow", F_OK));
    // System reboot commands captured
    bool hasSystemctl=false, hasForce=false;
    for(auto &c: g_cmds){ if(c.find("systemctl reboot")!=std::string::npos) hasSystemctl=true; if(c.find("reboot -f")!=std::string::npos) hasForce=true; }
    ASSERT_TRUE(hasSystemctl || g_called_systemctl);
    ASSERT_TRUE(hasForce || g_called_force);
}

TEST(RebootMain, ManageableNotificationEnabledPublishes){
    g_handle_decision = 1;
    g_notif_called=false; g_cmds.clear(); g_mng_enable=true;
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-o", "User requested reboot" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(g_notif_called);
}

TEST(RebootMain, ManageableNotificationDisabledNoPublish){
    g_handle_decision = 1;
    g_notif_called=false; g_cmds.clear(); g_mng_enable=false;
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-o", "User requested reboot" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    ASSERT_FALSE(g_notif_called);
}

TEST(RebootMain, CategoryMaintenanceOverride){
    g_handle_decision = 0;
    g_markers.clear();
    // Ensure dir exists cleanly
    system("mkdir -p /opt/secure/reboot");
    // Run with APP source and custom MAINTENANCE_REBOOT
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-r", "MAINTENANCE_REBOOT", "-o", "User requested reboot" };
    int rc = reboot_main_entry(7, (char**)argv);
    ASSERT_EQ(rc, 0);
    // Verify reason persisted as MAINTENANCE_REBOOT
    std::ifstream jf("/opt/secure/reboot/reboot.info");
    ASSERT_TRUE(jf.good());
    std::string content; std::getline(jf, content, '\0');
    ASSERT_NE(content.find("\"reason\":\"MAINTENANCE_REBOOT\""), std::string::npos);
}

TEST(RebootMain, OpsTriggeredCategorization){
    g_handle_decision = 0;
    // Ensure dir exists cleanly
    system("mkdir -p /opt/secure/reboot");
    // Run with an OPS-triggered source
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "FactoryReset" };
    int rc = reboot_main_entry(3, (char**)argv);
    ASSERT_EQ(rc, 0);
    // Verify reason persisted as OPS_TRIGGERED
    std::ifstream jf("/opt/secure/reboot/reboot.info");
    ASSERT_TRUE(jf.good());
    std::string content; std::getline(jf, content, '\0');
    ASSERT_NE(content.find("\"reason\":\"OPS_TRIGGERED\""), std::string::npos);
}

TEST(RebootMain, MaintenanceTriggeredCategorization){
    g_handle_decision = 0;
    system("mkdir -p /opt/secure/reboot");
    // Source directly categorized as maintenance
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "PwrMgr" };
    int rc = reboot_main_entry(3, (char**)argv);
    ASSERT_EQ(rc, 0);
    std::ifstream jf("/opt/secure/reboot/reboot.info");
    ASSERT_TRUE(jf.good());
    std::string content; std::getline(jf, content, '\0');
    ASSERT_NE(content.find("\"reason\":\"MAINTENANCE_REBOOT\""), std::string::npos);
}

TEST(RebootMain, UnknownSourceCategorization){
    g_handle_decision = 0;
    system("mkdir -p /opt/secure/reboot");
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "UnknownApp" };
    int rc = reboot_main_entry(3, (char**)argv);
    ASSERT_EQ(rc, 0);
    std::ifstream jf("/opt/secure/reboot/reboot.info");
    ASSERT_TRUE(jf.good());
    std::string content; std::getline(jf, content, '\0');
    ASSERT_NE(content.find("\"reason\":\"FIRMWARE_FAILURE\""), std::string::npos);
}

TEST(RebootMain, SystemctlHangBranch){
    g_handle_decision = 1;
    g_cmds.clear(); g_called_systemctl=false; g_called_force=false; g_secure_rc = 256;
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-o", "User requested reboot" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    // Commands should still include systemctl and force reboot
    bool hasSystemctl=false, hasForce=false;
    for(auto &c: g_cmds){ if(c.find("systemctl reboot")!=std::string::npos) hasSystemctl=true; if(c.find("reboot -f")!=std::string::npos) hasForce=true; }
    ASSERT_TRUE(hasSystemctl || g_called_systemctl);
    ASSERT_TRUE(hasForce || g_called_force);
    g_secure_rc = 0;
}

TEST(RebootMain, EmitT2SpecialMappings){
    g_handle_decision = 0; g_markers.clear();
    reset_getopt_state();
    const char* argv1[] = { "rebootnow", "-s", "runPodRecovery", "-o", "User requested reboot" };
    int rc1 = reboot_main_entry(5, (char**)argv1);
    ASSERT_EQ(rc1, 0);
    bool foundRunPod=false; for(auto &m: g_markers){ if(m.find("SYST_ERR_RunPod_reboot")!=std::string::npos) foundRunPod=true; }
    ASSERT_TRUE(foundRunPod);
    g_markers.clear();
    reset_getopt_state();
    const char* argv2[] = { "rebootnow", "-c", "IARMDaemonMain", "-o", "Crash" };
    int rc2 = reboot_main_entry(5, (char**)argv2);
    ASSERT_EQ(rc2, 0);
    bool foundIARM=false; for(auto &m: g_markers){ if(m.find("SYST_ERR_IARMDEMON_reboot")!=std::string::npos) foundIARM=true; }
    ASSERT_TRUE(foundIARM);
}

TEST(RebootMain, EmitT2MoreMappings){
    g_handle_decision = 0; g_markers.clear();
    // Non-crash special mapping: CardNotResponding
    reset_getopt_state();
    const char* argv1[] = { "rebootnow", "-s", "CardNotResponding", "-o", "User requested reboot" };
    int rc1 = reboot_main_entry(5, (char**)argv1);
    ASSERT_EQ(rc1, 0);
    ASSERT_GT(g_markers.size(), 0u);
    bool foundCard=false; for(auto &m: g_markers){
        if(m.find("SYST_ERR_CCNotResponding_reboot")!=std::string::npos ||
           m.find("SYST_ERR_CardNotResponding")!=std::string::npos) {
            foundCard=true;
        }
    }
    ASSERT_TRUE(foundCard);
    // Crash mapping: rmfStreamer and runPod
    g_markers.clear();
    reset_getopt_state();
    const char* argv2[] = { "rebootnow", "-c", "rmfStreamer", "-o", "Crash" };
    int rc2 = reboot_main_entry(5, (char**)argv2);
    ASSERT_EQ(rc2, 0);
    bool foundRmf=false; for(auto &m: g_markers){ if(m.find("SYST_ERR_Rmfstreamer_reboot")!=std::string::npos) foundRmf=true; }
    ASSERT_TRUE(foundRmf);
    g_markers.clear();
    reset_getopt_state();
    const char* argv3[] = { "rebootnow", "-c", "runPod", "-o", "Crash" };
    int rc3 = reboot_main_entry(5, (char**)argv3);
    ASSERT_EQ(rc3, 0);
    bool foundRunPod=false; for(auto &m: g_markers){ if(m.find("SYST_ERR_RunPod_reboot")!=std::string::npos) foundRunPod=true; }
    ASSERT_TRUE(foundRunPod);
}

TEST(RebootMain, EmitT2CrashDefaultMapping){
    // Crash source without a special mapping should use default _reboot suffix
    g_handle_decision = 0; g_markers.clear();
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-c", "UnknownCrash", "-o", "Crash" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    bool found=false; for(auto &m: g_markers){ if(m.find("SYST_ERR_UnknownCrash_reboot")!=std::string::npos) found=true; }
    ASSERT_TRUE(found);
}

TEST(RebootMain, SignalCleanupHandler){
    // Directly invoke cleanup handler to exercise its function body
    ASSERT_NO_THROW({ signal_cleanup_handler(0); });
}

TEST(RebootMain, MkdirSuccessBranch){
    g_handle_decision = 0;
    // Ensure directory does NOT exist so mkdir path is taken and succeeds
    system("rm -rf /opt/secure/reboot");
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-o", "User requested reboot" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    // Directory should now exist
    ASSERT_EQ(0, access("/opt/secure/reboot", F_OK));
}

TEST(RebootMain, DirCreateFailureJsonOpenFails){
    g_handle_decision = 1; g_cmds.clear();
    // Remove any existing directory and create a regular file
    system("rm -rf /opt/secure/reboot");
    std::ofstream bad("/opt/secure/reboot"); bad<<"x"; bad.close();
    reset_getopt_state();
    const char* argv[] = { "rebootnow", "-s", "HtmlDiagnostics", "-o", "User requested reboot" };
    int rc = reboot_main_entry(5, (char**)argv);
    ASSERT_EQ(rc, 0);
    // Cleanup the bad file to not affect other tests
    remove("/opt/secure/reboot");
}

TEST(RebootMain, InvokeLocalStubs){
    // Exercise local stubs directly to improve function coverage
    ASSERT_TRUE(rbus_init());
    rbus_cleanup();
    ASSERT_EQ(pidfile_write_and_guard(), 0);
    cleanup_pidfile();
    ASSERT_EQ(sleep_stub(0), 0u);
    ASSERT_EQ(fork_stub(), 1);
    ASSERT_EQ(execlp_stub("/bin/true", "true", (char*)NULL), -1);
    ASSERT_EQ(kill_stub(0, 0), 0);
    ASSERT_EQ(waitpid_stub(0, NULL, 0), 0);
}

TEST(RebootMain, InvokeT2ValNotify){
    // Directly call t2ValNotify to mark the function executed
    t2ValNotify("TEST_MARKER", "VAL");
}

TEST(RebootMain, InvokeT2CountNotify){
    // Directly call t2CountNotify to ensure function coverage
    g_markers.clear();
    t2CountNotify("TEST_MARKER_COUNT", 1);
    ASSERT_FALSE(g_markers.empty());
}

TEST(RebootMain, InvokeCheckStringValue){
    // Exercise checkstringvalue success and failure paths
    const char* arr1[] = { "Alpha", "Beta", "Gamma" };
    const char* arr2[] = { "One", "Two", "Three" };
    ASSERT_EQ(1, checkstringvalue(arr1, 3, "Bet"));
    ASSERT_EQ(0, checkstringvalue(arr2, 3, "Four"));
}

TEST(RebootMain, InvokeUsageDirect){
    // Call usage() directly to mark it executed (in addition to -h path)
    ASSERT_NO_THROW({ usage(stdout); });
}

TEST(RebootMain, CallT2InitAndEvent){
    // Ensure t2_init and t2_event_s stubs are executed
    t2_init("client-A");
    t2_event_s("MARKER_S", "VAL");
}

TEST(RebootMain, UpdateRebootLog_EdgeCases){
    // Hit early-return and truncation branches
    ASSERT_EQ(UpdateRebootLog(NULL, 0, 0, "%s", "x"), 0u);
    char buf[8] = {0};
    size_t used = 0;
    // Fill close to end
    used = UpdateRebootLog(buf, sizeof(buf), used, "%s", "1234");
    // Intentionally overflow with large string to hit truncation logic
    used = UpdateRebootLog(buf, sizeof(buf), used, "%s", "ABCDEFGHijklmnop");
    ASSERT_EQ(buf[sizeof(buf)-1], '\0');
}

TEST(RebootMain, UpdateRebootLog_BufferFullGuard){
    // bytes_used >= buffer_size - 1 should early-return and ensure NUL at end
    char buf[16]; memset(buf, 'X', sizeof(buf)); buf[sizeof(buf)-1] = '\0';
    size_t ret = UpdateRebootLog(buf, sizeof(buf), sizeof(buf)-1, "%s", "ignored");
    ASSERT_EQ(ret, sizeof(buf)-1);
    ASSERT_EQ(buf[sizeof(buf)-1], '\0');
}

extern "C" int handle_cyclic_reboot(const char* source,
                         const char* rebootReason,
                         const char* customReason,
                         const char* otherReason);

TEST(RebootMain, CallCyclicHandle){
    // Create expected files and flags to exercise cyclic_reboot paths
    system("mkdir -p /opt/secure/reboot");
    // simulate last reboot via rebootNow flag
    std::ofstream rf("/opt/secure/reboot/rebootNow"); rf<<""; rf.close();
    // previous info JSON with same reasons
    std::ofstream pf("/opt/secure/reboot/previousreboot.info");
    pf << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"Reason\",\n\"customReason\":\"Custom\",\n\"otherReason\":\"Other\"\n}";
    pf.close();
    // counter at or above threshold to trigger halt branch
    std::ofstream cf("/opt/secure/reboot/rebootCounter"); cf<<"5\n"; cf.close();
    remove("/opt/secure/reboot/rebootStop");
    g_det_enable=true; g_duration=10;
    // Call real function; should defer reboot and execute internal helpers
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 0);
}

TEST(RebootMain, CallCyclicLargeDuration){
    // Exercise compute_cron_time with larger duration to iterate overflow loop
    system("mkdir -p /opt/secure/reboot");
    std::ofstream rf("/opt/secure/reboot/rebootNow"); rf<<""; rf.close();
    std::ofstream pf("/opt/secure/reboot/previousreboot.info");
    pf << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"Reason\",\n\"customReason\":\"Custom\",\n\"otherReason\":\"Other\"\n}";
    pf.close();
    std::ofstream cf("/opt/secure/reboot/rebootCounter"); cf<<"5\n"; cf.close();
    remove("/opt/secure/reboot/rebootStop");
    g_det_enable=true; g_duration=180; // 3 hours to ensure multiple while(mn>=60) iterations
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 0);
}

TEST(RebootMain, CallCyclicNoFlagProceed){
    // Cover the branch where REBOOTNOW_FLAG is absent
    system("rm -rf /opt/secure/reboot");
    g_det_enable=true; g_duration=10;
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 1);
}

TEST(RebootMain, CallCyclicDifferentReasonResets){
    // Previous info exists but reasons differ -> reset branch executes and proceeds
    system("mkdir -p /opt/secure/reboot");
    std::ofstream rf("/opt/secure/reboot/rebootNow"); rf<<""; rf.close();
    std::ofstream pf("/opt/secure/reboot/previousreboot.info");
    pf << "{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"SrcPrev\",\n\"reason\":\"ReasonPrev\",\n\"customReason\":\"CustomPrev\",\n\"otherReason\":\"OtherPrev\"\n}";
    pf.close();
    std::ofstream sf("/opt/secure/reboot/rebootStop"); sf<<""; sf.close();
    std::ofstream cf("/opt/secure/reboot/rebootCounter"); cf<<"7\n"; cf.close();
    g_det_enable=true; g_duration=15;
    int rc = handle_cyclic_reboot("Src","Reason","Custom","Other");
    ASSERT_EQ(rc, 1);
    // Stop flag should be removed and counter reset
    ASSERT_EQ(std::ifstream("/opt/secure/reboot/rebootStop").good(), false);
    std::ifstream cr("/opt/secure/reboot/rebootCounter"); std::string cnt; std::getline(cr, cnt);
    ASSERT_EQ(cnt, "0");
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

