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

    void t2CountNotify(char *marker, int value) { (void)marker; (void)value; }
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
        system("mkdir -p /opt/logs/PreviousLogs");
        remove("/opt/logs/messages.txt");
        remove("/opt/logs/PreviousLogs/ocapri_log.txt");
        remove("/opt/logs/PreviousLogs/uimgr_log.txt");
        remove("/opt/logs/PreviousLogs/messages-ecm.txt");
    }

    void TearDown() override {
        system("rm -rf /tmp/reboot_test");
        remove("/opt/logs/messages.txt");
        remove("/opt/logs/PreviousLogs/ocapri_log.txt");
        remove("/opt/logs/PreviousLogs/uimgr_log.txt");
        remove("/opt/logs/PreviousLogs/messages-ecm.txt");
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

TEST_F(RebootClassifyTest, check_firmware_failure_MaxRebootDetectedStb) {
    system("mkdir -p /opt/logs/PreviousLogs");
    remove("/opt/logs/PreviousLogs/messages-ecm.txt");
    FILE* fp = fopen("/opt/logs/PreviousLogs/ocapri_log.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create ocapri log in this environment";
    }
    fputs("some line\nBox has rebooted 10 times\n", fp);
    fclose(fp);

    EnvContext ctx;
    FirmwareFailure fwFailure;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));
    strcpy(ctx.device_type, "stb");

    int result = check_firmware_failure(&ctx, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_TRUE(fwFailure.detected);
    EXPECT_TRUE(fwFailure.maxRebootDetected);
    EXPECT_STREQ(fwFailure.initiator, "OcapRI");
}

TEST_F(RebootClassifyTest, check_firmware_failure_MediaClientUsesUiMgr) {
    system("mkdir -p /opt/logs/PreviousLogs");
    remove("/opt/logs/PreviousLogs/messages-ecm.txt");
    FILE* fp = fopen("/opt/logs/PreviousLogs/uimgr_log.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create uimgr log in this environment";
    }
    fputs("Box has rebooted 10 times\n", fp);
    fclose(fp);

    EnvContext ctx;
    FirmwareFailure fwFailure;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));
    strcpy(ctx.device_type, "mediaclient_x1");

    int result = check_firmware_failure(&ctx, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_TRUE(fwFailure.detected);
    EXPECT_TRUE(fwFailure.maxRebootDetected);
    EXPECT_STREQ(fwFailure.initiator, "UiMgr");
}

TEST_F(RebootClassifyTest, check_firmware_failure_EcmCrashDetected) {
    system("mkdir -p /opt/logs/PreviousLogs");
    FILE* fp = fopen("/opt/logs/PreviousLogs/messages-ecm.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create ecm crash log in this environment";
    }
    fputs("**** CRASH ****\n", fp);
    fclose(fp);

    EnvContext ctx;
    FirmwareFailure fwFailure;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));
    strcpy(ctx.device_type, "stb");

    int result = check_firmware_failure(&ctx, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_TRUE(fwFailure.detected);
    EXPECT_TRUE(fwFailure.ecmCrashDetected);
    EXPECT_STREQ(fwFailure.initiator, "EcmLogger");
}

TEST_F(RebootClassifyTest, check_firmware_failure_MaxRebootAndEcmCrashTogether) {
    system("mkdir -p /opt/logs/PreviousLogs");
    FILE* fp = fopen("/opt/logs/PreviousLogs/ocapri_log.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create ocapri log in this environment";
    }
    fputs("Box has rebooted 10 times\n", fp);
    fclose(fp);

    fp = fopen("/opt/logs/PreviousLogs/messages-ecm.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create ecm crash log in this environment";
    }
    fputs("**** CRASH ****\n", fp);
    fclose(fp);

    EnvContext ctx;
    FirmwareFailure fwFailure;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));
    strcpy(ctx.device_type, "stb");

    int result = check_firmware_failure(&ctx, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_TRUE(fwFailure.maxRebootDetected);
    EXPECT_TRUE(fwFailure.ecmCrashDetected);
    EXPECT_STREQ(fwFailure.initiator, "EcmLogger");
    EXPECT_NE(strstr(fwFailure.details, "OcapRI"), nullptr);
}

TEST_F(RebootClassifyTest, detect_kernel_panic_BrcmWithOopsSignature) {
    system("mkdir -p /opt/logs");
    FILE* fp = fopen("/opt/logs/messages.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create /opt/logs/messages.txt in this environment";
    }
    fputs("prefix PREVIOUS_KERNEL_OOPS_DUMP marker\n", fp);
    fputs("Kernel panic - not syncing: Fatal exception\n", fp);
    fclose(fp);

    EnvContext ctx;
    PanicInfo panicInfo;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    strcpy(ctx.soc, "BRCM");

    int result = detect_kernel_panic(&ctx, &panicInfo);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_TRUE(panicInfo.detected);
    EXPECT_NE(panicInfo.panicType[0], '\0');
}

TEST_F(RebootClassifyTest, detect_kernel_panic_RtkWithoutPstoreFile) {
    EnvContext ctx;
    PanicInfo panicInfo;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    strcpy(ctx.soc, "RTK");

    int result = detect_kernel_panic(&ctx, &panicInfo);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_FALSE(panicInfo.detected);
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
    EXPECT_STREQ(info.reason, "FIRMWARE_FAILURE");
    EXPECT_STREQ(info.customReason, "");
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
    EXPECT_STREQ(info.reason, "FIRMWARE_FAILURE");
    EXPECT_STREQ(info.customReason, "");
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
    EXPECT_STREQ(info.customReason, "Hardware Register - KERNEL_PANIC");
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
    EXPECT_STREQ(info.reason, "APP_TRIGGERED");
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
    EXPECT_STREQ(info.reason, "OPS_TRIGGERED");
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
    EXPECT_STREQ(info.reason, "MAINTENANCE_REBOOT");
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

TEST_F(RebootClassifyTest, classify_reboot_reason_HardwareMappings_MultipleCases) {
    struct MappingCase {
        const char* mapped;
        const char* expectedSource;
        const char* expectedReason;
    };

    const MappingCase cases[] = {
        {"SECURITY_MASTER_RESET", "SecurityReboot", "SECURITY_MASTER_RESET"},
        {"CPU_EJTAG_RESET", "CPU EJTAG", "CPU_EJTAG_RESET"},
        {"SCPU_EJTAG_RESET", "CPU EJTAG", "CPU_EJTAG_RESET"},
        {"GEN_WATCHDOG_1_RESET", "WatchDog", "WATCHDOG_TIMER_RESET"},
        {"AUX_CHIP_EDGE_RESET_0", "Aux Chip Edge", "AUX_CHIP_EDGE_RESET"},
        {"AUX_CHIP_LEVEL_RESET_1", "Aux Chip Level", "AUX_CHIP_LEVEL_RESET"},
        {"MPM_RESET", "MPM", "MPM_RESET"},
        {"OVERVOLTAGE", "OverVoltage", "OVERVOLTAGE_RESET"},
        {"UNDERVOLTAGE_0_RESET", "LowVoltage", "UNDERVOLTAGE_RESET"}
    };

    for (const auto& testCase : cases) {
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

        strcpy(hwReason.mappedReason, testCase.mapped);

        int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

        EXPECT_EQ(result, SUCCESS);
        EXPECT_STREQ(info.source, testCase.expectedSource);
        EXPECT_STREQ(info.reason, testCase.expectedReason);
    }
}

TEST_F(RebootClassifyTest, classify_reboot_reason_UnknownCustomDefaultsToFirmwareFailure) {
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

    strcpy(info.customReason, "UNLISTED_INITIATOR");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.reason, "FIRMWARE_FAILURE");
    EXPECT_STREQ(info.source, "Unknown");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_FallbackToSoftwareReboot) {
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

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "SoftwareReboot");
    EXPECT_STREQ(info.reason, "SOFTWARE_MASTER_RESET");
    EXPECT_STREQ(info.customReason, "SOFTWARE_MASTER_RESET");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_HardwareUnknownMapsHardPower) {
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

    strcpy(hwReason.mappedReason, "UNKNOWN");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "Hard Power Reset");
    EXPECT_STREQ(info.reason, "HARD_POWER");
    EXPECT_STREQ(info.customReason, "Hardware Register - NULL");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_BrcmRawReasonSetsPrefixedCustomReason) {
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

    strcpy(ctx.soc, "BRCM");
    strcpy(hwReason.mappedReason, "SOFTWARE_MASTER_RESET");
    strcpy(hwReason.rawReason, "watchdog_reset");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.customReason, "Hardware Register - WATCHDOG_RESET");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_AmlogicPresetReasonDoesNotBypassClassification) {
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

    strcpy(ctx.soc, "AMLOGIC");
    strcpy(info.reason, "PRESET_REASON");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.reason, "SOFTWARE_MASTER_RESET");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_MtkPresetReasonDoesNotBypassClassification) {
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

    strcpy(ctx.soc, "MTK");
    strcpy(info.reason, "MTK_PRESET");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.reason, "SOFTWARE_MASTER_RESET");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_FirmwareFailureWithInitiatorAndDetails) {
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
    strcpy(fwFailure.initiator, "EcmLogger");
    strcpy(fwFailure.details, "EcmLogger: ECM crash detected");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "EcmLogger");
    EXPECT_STREQ(info.reason, "FIRMWARE_FAILURE");
    EXPECT_STREQ(info.otherReason, "EcmLogger: ECM crash detected");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_CustomReasonMaintenanceLiteral) {
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

    strcpy(info.customReason, "MAINTENANCE_REBOOT");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.reason, "MAINTENANCE_REBOOT");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_BrcmMappedReasonPrefixedWhenRawMissing) {
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

    strcpy(ctx.soc, "BRCM");
    strcpy(hwReason.mappedReason, "POWER_ON");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.customReason, "Hardware Register - POWER_ON");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_BrcmUnknownMappedReasonHardPower) {
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

    strcpy(ctx.soc, "BRCM");
    strcpy(hwReason.mappedReason, "UNKNOWN");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "Hard Power Reset");
    EXPECT_STREQ(info.reason, "HARD_POWER");
    EXPECT_STREQ(info.customReason, "Hardware Register - NULL");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_UnknownMappedReasonNonBrcm) {
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

    strcpy(ctx.soc, "RTK");
    strcpy(hwReason.mappedReason, "UNKNOWN");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "Hard Power Reset");
    EXPECT_STREQ(info.reason, "HARD_POWER");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_HardwareUnknownStringNonEmpty) {
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

    strcpy(hwReason.mappedReason, "UNCLASSIFIED_REASON");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "Unknown");
    EXPECT_STREQ(info.reason, "UNCLASSIFIED_REASON");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_HardwareMappings_AdditionalCases) {
    struct MappingCase {
        const char* mapped;
        const char* expectedSource;
        const char* expectedReason;
    };

    const MappingCase cases[] = {
        {"MAIN_CHIP_INPUT_RESET", "Main Chip", "MAIN_CHIP_INPUT_RESET"},
        {"MAIN_CHIP_RESET_INPUT", "Main Chip", "MAIN_CHIP_RESET_INPUT"},
        {"TAP_IN_SYSTEM_RESET", "Tap-In System", "TAP_IN_SYSTEM_RESET"},
        {"FRONT_PANEL_4SEC_RESET", "FrontPanel Button", "FRONT_PANEL_RESET"},
        {"S3_WAKEUP_RESET", "Standby Wakeup", "S3_WAKEUP_RESET"},
        {"SMARTCARD_INSERT_RESET", "SmartCard Insert", "SMARTCARD_INSERT_RESET"},
        {"OVERTEMP", "OverTemperature", "OVERTEMP_RESET"},
        {"PCIE_0_HOT_BOOT_RESET", "PCIE Boot", "PCIE_HOT_BOOT_RESET"}
    };

    for (const auto& testCase : cases) {
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

        strcpy(hwReason.mappedReason, testCase.mapped);

        int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);

        EXPECT_EQ(result, SUCCESS);
        EXPECT_STREQ(info.source, testCase.expectedSource);
        EXPECT_STREQ(info.reason, testCase.expectedReason);
    }
}

TEST_F(RebootClassifyTest, classify_reboot_reason_BrcmBroadcomAliasCustomPrefix) {
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

    strcpy(ctx.soc, "BROADCOM");
    strcpy(hwReason.mappedReason, "WATCHDOG_RESET");
    strcpy(hwReason.rawReason, "gen_watchdog_reset");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.customReason, "Hardware Register - GEN_WATCHDOG_RESET");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_HardwareMappings_AliasCases) {
    struct MappingCase {
        const char* mapped;
        const char* expectedSource;
        const char* expectedReason;
    };

    const MappingCase cases[] = {
        {"KERNEL_PANIC_RESET", "Kernel", "KERNEL_PANIC"},
        {"SOFTWARE_RESET", "SoftwareReboot", "SOFTWARE_MASTER_RESET"},
        {"HARDWARE", "PowerOn", "POWER_ON_RESET"},
        {"OVERHEAT", "OverTemperature", "OVERTEMP_RESET"},
        {"UNDERVOLTAGE_1_RESET", "LowVoltage", "UNDERVOLTAGE_RESET"},
        {"PCIE_1_HOT_BOOT_RESET", "PCIE Boot", "PCIE_HOT_BOOT_RESET"},
        {"AUX_CHIP_EDGE_RESET", "Aux Chip Edge", "AUX_CHIP_EDGE_RESET"},
        {"AUX_CHIP_LEVEL_RESET", "Aux Chip Level", "AUX_CHIP_LEVEL_RESET"}
    };

    for (const auto& testCase : cases) {
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

        strcpy(hwReason.mappedReason, testCase.mapped);

        int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
        EXPECT_EQ(result, SUCCESS);
        EXPECT_STREQ(info.source, testCase.expectedSource);
        EXPECT_STREQ(info.reason, testCase.expectedReason);
    }
}

TEST_F(RebootClassifyTest, classify_reboot_reason_RtkAnnotatesKernelLog) {
    system("mkdir -p /opt/logs");
    remove("/opt/logs/messages.txt");

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

    strcpy(ctx.soc, "RTK");
    strcpy(hwReason.mappedReason, "WATCHDOG");

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "WatchDog");
    EXPECT_STREQ(info.reason, "WATCHDOG_TIMER_RESET");

    FILE* fp = fopen("/opt/logs/messages.txt", "r");
    if (!fp) {
        GTEST_SKIP() << "Cannot read /opt/logs/messages.txt in this environment";
    }
    char buf[512] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    EXPECT_NE(strstr(buf, "PreviousRebootReason: watchdog_timer_reset"), nullptr);
}

TEST_F(RebootClassifyTest, classify_reboot_reason_UnknownWhenPanicFlagSetButNotDetectedEarlier) {
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
    info.customReason[0] = '\0';
    hwReason.mappedReason[0] = '\0';

    int result = classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "Kernel");
    EXPECT_STREQ(info.reason, "KERNEL_PANIC");
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

