#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
    #include "update-reboot-info.h"
    #include "rdk_fwdl_utils.h"
    #include "rdk_debug.h"

    int g_rdk_logger_enabled = 0;

    int getDevicePropertyData(const char* key, char* value, int size) {
        (void)key; (void)value; (void)size;
        return UTILS_FAILURE;
    }

    void t2_event_d(const char* marker, int value) { (void)marker; (void)value; }
    void t2_event_s(const char* marker, const char* value) { (void)marker; (void)value; }
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_classify_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
using namespace std;

class RebootClassifyTest : public ::testing::Test {
protected:
    void SetUp() override {
        system("mkdir -p /tmp/reboot_test");
    }

    void TearDown() override {
        system("rm -rf /tmp/reboot_test");
    }
};

// Tests for is_app_triggered
TEST_F(RebootClassifyTest, is_app_triggered_NullInput) {
    EXPECT_FALSE(is_app_triggered(nullptr));
}

TEST_F(RebootClassifyTest, is_app_triggered_ValidReasons) {
    EXPECT_TRUE(is_app_triggered("Servicemanager"));
    EXPECT_TRUE(is_app_triggered("SystemServices"));
    EXPECT_TRUE(is_app_triggered("WarehouseReset"));
    EXPECT_TRUE(is_app_triggered("TR69Agent"));
    EXPECT_TRUE(is_app_triggered("HrvInitWHReset"));
    EXPECT_TRUE(is_app_triggered("InstallTDK"));
}

TEST_F(RebootClassifyTest, is_app_triggered_InvalidReasons) {
    EXPECT_FALSE(is_app_triggered("UnknownReason"));
    EXPECT_FALSE(is_app_triggered("ScheduledReboot"));
    EXPECT_FALSE(is_app_triggered("AutoReboot.sh"));
}

// Tests for is_ops_triggered
TEST_F(RebootClassifyTest, is_ops_triggered_NullInput) {
    EXPECT_FALSE(is_ops_triggered(nullptr));
}

TEST_F(RebootClassifyTest, is_ops_triggered_ValidReasons) {
    EXPECT_TRUE(is_ops_triggered("ScheduledReboot"));
    EXPECT_TRUE(is_ops_triggered("FactoryReset"));
    EXPECT_TRUE(is_ops_triggered("ImageUpgrade_mfr_api"));
    EXPECT_TRUE(is_ops_triggered("HAL_SYS_Reboot"));
    EXPECT_TRUE(is_ops_triggered("PowerMgr_Powerreset"));
    EXPECT_TRUE(is_ops_triggered("DeepSleepMgr"));
}

TEST_F(RebootClassifyTest, is_ops_triggered_InvalidReasons) {
    EXPECT_FALSE(is_ops_triggered("UnknownReason"));
    EXPECT_FALSE(is_ops_triggered("Servicemanager"));
    EXPECT_FALSE(is_ops_triggered("AutoReboot.sh"));
}

// Tests for is_maintenance_triggered
TEST_F(RebootClassifyTest, is_maintenance_triggered_NullInput) {
    EXPECT_FALSE(is_maintenance_triggered(nullptr));
}

TEST_F(RebootClassifyTest, is_maintenance_triggered_ValidReasons) {
    EXPECT_TRUE(is_maintenance_triggered("AutoReboot.sh"));
    EXPECT_TRUE(is_maintenance_triggered("PwrMgr"));
}

TEST_F(RebootClassifyTest, is_maintenance_triggered_InvalidReasons) {
    EXPECT_FALSE(is_maintenance_triggered("UnknownReason"));
    EXPECT_FALSE(is_maintenance_triggered("Servicemanager"));
    EXPECT_FALSE(is_maintenance_triggered("ScheduledReboot"));
}

// Tests for detect_kernel_panic
TEST_F(RebootClassifyTest, detect_kernel_panic_NullParameters) {
    EnvContext ctx;
    PanicInfo panicInfo;

    EXPECT_EQ(detect_kernel_panic(nullptr, &panicInfo), ERROR_GENERAL);
    EXPECT_EQ(detect_kernel_panic(&ctx, nullptr), ERROR_GENERAL);
}

TEST_F(RebootClassifyTest, detect_kernel_panic_NoPanicDetected) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    strcpy(ctx.soc, "BRCM");

    PanicInfo panicInfo;
    int result = detect_kernel_panic(&ctx, &panicInfo);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_FALSE(panicInfo.detected);
}

// Tests for check_firmware_failure
TEST_F(RebootClassifyTest, check_firmware_failure_NullParameters) {
    EnvContext ctx;
    FirmwareFailure fwFailure;

    EXPECT_EQ(check_firmware_failure(nullptr, &fwFailure), ERROR_GENERAL);
    EXPECT_EQ(check_firmware_failure(&ctx, nullptr), ERROR_GENERAL);
}

TEST_F(RebootClassifyTest, check_firmware_failure_NoFailureDetected) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    strcpy(ctx.device_type, "stb");

    FirmwareFailure fwFailure;
    int result = check_firmware_failure(&ctx, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_FALSE(fwFailure.detected);
    EXPECT_FALSE(fwFailure.maxRebootDetected);
    EXPECT_FALSE(fwFailure.ecmCrashDetected);
}


// Tests for classify_reboot_reason
TEST_F(RebootClassifyTest, classify_reboot_reason_NullParameters) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    EXPECT_EQ(classify_reboot_reason(nullptr, &ctx, &hwReason, &panicInfo, &fwFailure), ERROR_GENERAL);
    EXPECT_EQ(classify_reboot_reason(&info, nullptr, &hwReason, &panicInfo, &fwFailure), ERROR_GENERAL);
}


TEST_F(RebootClassifyTest, classify_reboot_reason_FirmwareFailureMaxReboot) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    fwFailure.detected = true;
    fwFailure.maxRebootDetected = true;

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "FirmwareFailure");
    EXPECT_STREQ(info.reason, "MAX_REBOOT_EXCEEDED");
    EXPECT_STREQ(info.customReason, "MAX_REBOOT_EXCEEDED");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_FirmwareFailureECMCrash) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    fwFailure.detected = true;
    fwFailure.ecmCrashDetected = true;

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "FirmwareFailure");
    EXPECT_STREQ(info.reason, "ECM_CRASH");
    EXPECT_STREQ(info.customReason, "ECM_CRASH");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_KernelPanic) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    panicInfo.detected = true;
    strcpy(panicInfo.panicType, "Kernel panic - not syncing");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "Kernel");
    EXPECT_STREQ(info.reason, "KERNEL_PANIC");
    EXPECT_STREQ(info.customReason, "Kernel panic - not syncing");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_AppTriggered) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    strcpy(info.customReason, "Servicemanager");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "APP_TRIGGERED");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_OpsTriggered) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    strcpy(info.customReason, "ScheduledReboot");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "OPS_TRIGGERED");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_MaintenanceTriggered) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    strcpy(info.customReason, "AutoReboot.sh");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "MAINTENANCE_TRIGGERED");
}


TEST_F(RebootClassifyTest, classify_reboot_reason_HardwareReason) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    strcpy(hwReason.mappedReason, "SOFTWARE_MASTER_RESET");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "SoftwareReboot");
    EXPECT_STREQ(info.reason, "SOFTWARE_MASTER_RESET");
}


TEST_F(RebootClassifyTest, classify_reboot_reason_WatchdogReset) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    strcpy(hwReason.mappedReason, "WATCHDOG");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "WatchDog");
    EXPECT_STREQ(info.reason, "WATCHDOG_TIMER_RESET");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_PowerOnReset) {
    RebootInfo info;
    EnvContext ctx;
    HardwareReason hwReason;
    PanicInfo panicInfo;
    FirmwareFailure fwFailure;

    memset(&info, 0, sizeof(RebootInfo));
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&hwReason, 0, sizeof(HardwareReason));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));

    strcpy(hwReason.mappedReason, "POWER_ON");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "PowerOn");
    EXPECT_STREQ(info.reason, "POWER_ON_RESET");
}


GTEST_API_ int main(int argc, char *argv[]) {
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];

    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s",
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);

    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);

    cout << "Starting REBOOT_CLASSIFY GTEST ===================>" << endl;
    return RUN_ALL_TESTS();
}
