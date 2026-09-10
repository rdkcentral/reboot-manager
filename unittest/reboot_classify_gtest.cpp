#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
    #include "update-reboot-info.h"
    #include "rdk_fwdl_utils.h"
    #include "rdk_debug.h"

    void copy_pstore_logs_to_opt(void);
    void update_kernel_log(const EnvContext *ctx, const RebootInfo *info);
 
    int g_rdk_logger_enabled = 0;

    int getDevicePropertyData(const char* key, char* value, int size) {
        (void)key; (void)value; (void)size;
        return UTILS_FAILURE;
    }

    void t2CountNotify(char *marker, int value) { (void)marker; (void)value; }
    void t2_event_d(const char* marker, int value) { (void)marker; (void)value; }
    void t2_event_s(const char* marker, const char* value) { (void)marker; (void)value; }

    /* PSTORE lives under a read-only kernel filesystem, so the tests point the
     * access/fopen/opendir calls at a writable sandbox instead. */
    static char g_pstore_redirect[128] = {0};

    static const char *redirect_pstore(const char *path, char *buf, size_t len)
    {
        size_t pstore_len = strlen(PSTORE_DIR);
        if (!path || g_pstore_redirect[0] == '\0') {
            return path;
        }
        if (strncmp(path, PSTORE_DIR, pstore_len) != 0) {
            return path;
        }
        snprintf(buf, len, "%s%s", g_pstore_redirect, path + pstore_len);
        return buf;
    }

    int __real_access(const char *pathname, int mode);
    int __wrap_access(const char *pathname, int mode)
    {
        char buf[MAX_PATH_LENGTH + sizeof(g_pstore_redirect)];
        return __real_access(redirect_pstore(pathname, buf, sizeof(buf)), mode);
    }

    FILE *__real_fopen(const char *pathname, const char *mode);
    FILE *__wrap_fopen(const char *pathname, const char *mode)
    {
        char buf[MAX_PATH_LENGTH + sizeof(g_pstore_redirect)];
        return __real_fopen(redirect_pstore(pathname, buf, sizeof(buf)), mode);
    }

    /* Declared as void* so the test does not need the DIR definition. */
    void *__real_opendir(const char *name);
    void *__wrap_opendir(const char *name)
   {
       char buf[MAX_PATH_LENGTH + sizeof(g_pstore_redirect)];
       return __real_opendir(redirect_pstore(name, buf, sizeof(buf)));
   }

}

static void enable_pstore_redirect(const char *dir)
{
   snprintf(g_pstore_redirect, sizeof(g_pstore_redirect), "%s", dir);
}

static void disable_pstore_redirect(void)
{
   g_pstore_redirect[0] = '\0';
}

static std::string read_whole_file(const char *path)
{
   std::string contents;
   FILE *fp = __real_fopen(path, "r");
   if (!fp) {
       return contents;
   }
   char buf[512];
   size_t n;
   while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
       contents.append(buf, n);
   }
   fclose(fp);
   return contents;
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_classify_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
using namespace std;

class RebootClassifyTest : public ::testing::Test {
protected:
    void SetUp() override {
        disable_pstore_redirect();
        system("mkdir -p /tmp/reboot_test");
        system("mkdir -p /opt/logs/PreviousLogs");
        remove("/opt/logs/messages.txt");
        remove("/opt/logs/PreviousLogs/ocapri_log.txt");
        remove("/opt/logs/PreviousLogs/uimgr_log.txt");
        remove("/opt/logs/PreviousLogs/messages-ecm.txt");
    }

    void TearDown() override {
        disable_pstore_redirect();
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

TEST_F(RebootClassifyTest, copy_pstore_logs_to_opt_NoPstoreDirectory) {
    copy_pstore_logs_to_opt();
    SUCCEED();
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

TEST_F(RebootClassifyTest, classify_reboot_reason_AmlogicBypassWhenReasonPreset) {
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

TEST_F(RebootClassifyTest, classify_reboot_reason_MtkBypassWhenReasonPreset) {
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

    /* The kernel log annotation is done by the caller, as in rebootreason_main. */
    update_kernel_log(&ctx, &info);

    std::string contents = read_whole_file("/opt/logs/messages.txt");
    if (contents.empty()) {
       GTEST_SKIP() << "Cannot write /opt/logs/messages.txt in this environment";
    }
    EXPECT_NE(contents.find("PreviousRebootReason: watchdog_timer_reset"), std::string::npos);
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

// Tests for update_kernel_log
TEST_F(RebootClassifyTest, update_kernel_log_NullParameters) {
    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&info, 0, sizeof(RebootInfo));

    update_kernel_log(nullptr, &info);
    update_kernel_log(&ctx, nullptr);

    EXPECT_EQ(access("/opt/logs/messages.txt", F_OK), -1);
}

TEST_F(RebootClassifyTest, update_kernel_log_NonRealtekSocIsNoOp) {
    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&info, 0, sizeof(RebootInfo));

    strcpy(ctx.soc, "BRCM");
    strcpy(info.reason, "KERNEL_PANIC");

    update_kernel_log(&ctx, &info);

    EXPECT_EQ(access("/opt/logs/messages.txt", F_OK), -1);
}

TEST_F(RebootClassifyTest, update_kernel_log_RtkWithEmptyReasonIsNoOp) {
    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&info, 0, sizeof(RebootInfo));

    strcpy(ctx.soc, "RTK");

    update_kernel_log(&ctx, &info);

    EXPECT_EQ(access("/opt/logs/messages.txt", F_OK), -1);
}

TEST_F(RebootClassifyTest, update_kernel_log_RtkAnnotatesLowercaseReason) {
    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&info, 0, sizeof(RebootInfo));

    strcpy(ctx.soc, "RTK");
    strcpy(info.reason, "KERNEL_PANIC");

    update_kernel_log(&ctx, &info);

    std::string contents = read_whole_file("/opt/logs/messages.txt");
    if (contents.empty()) {
        GTEST_SKIP() << "Cannot write /opt/logs/messages.txt in this environment";
    }
    EXPECT_NE(contents.find("PreviousRebootReason: kernel_panic"), std::string::npos);
}

TEST_F(RebootClassifyTest, update_kernel_log_RealtekSocAnnotatesReason) {
    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&info, 0, sizeof(RebootInfo));

    strcpy(ctx.soc, "REALTEK");
    strcpy(info.reason, "SOFTWARE_MASTER_RESET");

    update_kernel_log(&ctx, &info);

    std::string contents = read_whole_file("/opt/logs/messages.txt");
    if (contents.empty()) {
        GTEST_SKIP() << "Cannot write /opt/logs/messages.txt in this environment";
    }
    EXPECT_NE(contents.find("PreviousRebootReason: software_master_reset"), std::string::npos);
}

// Tests for the PSTORE/TV kernel panic path
TEST_F(RebootClassifyTest, detect_kernel_panic_BrcmWithoutOopsMarker) {
    system("mkdir -p /opt/logs");
    FILE* fp = fopen("/opt/logs/messages.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create /opt/logs/messages.txt in this environment";
    }
    fputs("Kernel panic - not syncing: Fatal exception\n", fp);
    fclose(fp);

    EnvContext ctx;
    PanicInfo panicInfo;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    strcpy(ctx.soc, "BRCM");

    EXPECT_EQ(detect_kernel_panic(&ctx, &panicInfo), SUCCESS);
    EXPECT_FALSE(panicInfo.detected);
}

TEST_F(RebootClassifyTest, detect_kernel_panic_TvPstoreWithoutPanicSignature) {
    system("mkdir -p /tmp/reboot_test/pstore");
    FILE* fp = fopen("/tmp/reboot_test/pstore/console-ramoops-0", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create pstore sandbox in this environment";
    }
    fputs("normal boot log line\n", fp);
    fclose(fp);
    enable_pstore_redirect("/tmp/reboot_test/pstore");

    EnvContext ctx;
    PanicInfo panicInfo;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    strcpy(ctx.rdkProfile, "TV");

    EXPECT_EQ(detect_kernel_panic(&ctx, &panicInfo), SUCCESS);
    EXPECT_FALSE(panicInfo.detected);
}

TEST_F(RebootClassifyTest, detect_kernel_panic_TvPstorePanicCopiesLogsAndAnnotates) {
    system("mkdir -p /tmp/reboot_test/pstore");
    system("mkdir -p /opt/logs");
    remove("/opt/logs/dmesg-ramoops-0.log");

    FILE* fp = fopen("/tmp/reboot_test/pstore/console-ramoops-0", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create pstore sandbox in this environment";
    }
    fputs("boot line\nKernel panic - not syncing: Fatal exception\n", fp);
    fclose(fp);

    fp = fopen("/tmp/reboot_test/pstore/dmesg-ramoops-0", "w");
    ASSERT_NE(fp, nullptr);
    fputs("dmesg contents\n", fp);
    fclose(fp);

    /* Entry whose name is too long to build a source path; must be skipped. */
    std::string longName(250, 'x');
    std::string longPath = "/tmp/reboot_test/pstore/" + longName;
    fp = fopen(longPath.c_str(), "w");
    if (fp) {
        fputs("skipme\n", fp);
        fclose(fp);
    }

    enable_pstore_redirect("/tmp/reboot_test/pstore");

    EnvContext ctx;
    PanicInfo panicInfo;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&panicInfo, 0, sizeof(PanicInfo));
    strcpy(ctx.rdkProfile, "TV");
    EXPECT_EQ(detect_kernel_panic(&ctx, &panicInfo), SUCCESS);
    EXPECT_TRUE(panicInfo.detected);
    EXPECT_STREQ(panicInfo.panicType, "Kernel panic - not syncing");
    EXPECT_EQ(strchr(panicInfo.details, '\n'), nullptr);

    disable_pstore_redirect();
    EXPECT_EQ(access("/opt/logs/dmesg-ramoops-0.log", F_OK), 0);
    std::string contents = read_whole_file("/opt/logs/messages.txt");
    EXPECT_NE(contents.find("PreviousRebootReason: kernel_panic!"), std::string::npos);

    remove("/opt/logs/dmesg-ramoops-0.log");
    remove("/opt/logs/console-ramoops-0.log");
}

TEST_F(RebootClassifyTest, copy_pstore_logs_to_opt_EmptyPstoreDirectory) {
    system("mkdir -p /tmp/reboot_test/pstore_empty");
    enable_pstore_redirect("/tmp/reboot_test/pstore_empty");

    copy_pstore_logs_to_opt();

    SUCCEED();
}

// Remaining hardware reason mappings
TEST_F(RebootClassifyTest, classify_reboot_reason_HardwareMappings_RemainingCases) {
    struct MappingCase {
        const char* mapped;
        const char* expectedSource;
        const char* expectedReason;
    };

    const MappingCase cases[] = {
        {"KERNEL_PANIC_RESET", "Kernel", "KERNEL_PANIC"},
        {"HARDWARE", "PowerOn", "POWER_ON_RESET"},
        {"MAIN_CHIP_INPUT_RESET", "Main Chip", "MAIN_CHIP_INPUT_RESET"},
        {"MAIN_CHIP_RESET_INPUT", "Main Chip", "MAIN_CHIP_RESET_INPUT"},
        {"TAP_IN_SYSTEM_RESET", "Tap-In System", "TAP_IN_SYSTEM_RESET"},
        {"FRONT_PANEL_4SEC_RESET", "FrontPanel Button", "FRONT_PANEL_RESET"},
        {"FRONT_PANEL_RESET", "FrontPanel Button", "FRONT_PANEL_RESET"},
        {"S3_WAKEUP_RESET", "Standby Wakeup", "S3_WAKEUP_RESET"},
        {"SMARTCARD_INSERT_RESET", "SmartCard Insert", "SMARTCARD_INSERT_RESET"},
        {"OVERTEMP", "OverTemperature", "OVERTEMP_RESET"},
        {"OVERHEAT", "OverTemperature", "OVERTEMP_RESET"},
        {"PCIE_0_HOT_BOOT_RESET", "PCIE Boot", "PCIE_HOT_BOOT_RESET"},
        {"PCIE_1_HOT_BOOT_RESET", "PCIE Boot", "PCIE_HOT_BOOT_RESET"},
        {"AUX_CHIP_EDGE_RESET_1", "Aux Chip Edge", "AUX_CHIP_EDGE_RESET"},
        {"AUX_CHIP_LEVEL_RESET_0", "Aux Chip Level", "AUX_CHIP_LEVEL_RESET"},
        {"UNDERVOLTAGE_1_RESET", "LowVoltage", "UNDERVOLTAGE_RESET"},
        {"NOT_A_KNOWN_REGISTER", "Unknown", "NOT_A_KNOWN_REGISTER"}
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

        EXPECT_EQ(classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure), SUCCESS);
        EXPECT_STREQ(info.source, testCase.expectedSource) << testCase.mapped;
        EXPECT_STREQ(info.reason, testCase.expectedReason) << testCase.mapped;
    }
}

TEST_F(RebootClassifyTest, classify_reboot_reason_UnknownCustomKeepsExistingSource) {
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
    strcpy(info.source, "SomeDaemon");

    EXPECT_EQ(classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure), SUCCESS);
    EXPECT_STREQ(info.reason, "FIRMWARE_FAILURE");
    EXPECT_STREQ(info.source, "SomeDaemon");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_FirmwareFailureWithoutDetailsKeepsOtherReason) {
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
    strcpy(info.otherReason, "PreExistingDetail");

    EXPECT_EQ(classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure), SUCCESS);
    EXPECT_STREQ(info.source, "FirmwareFailure");
    EXPECT_STREQ(info.otherReason, "PreExistingDetail");
}

TEST_F(RebootClassifyTest, classify_reboot_reason_BroadcomSocUsesRawReason) {
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
    strcpy(hwReason.mappedReason, "MPM_RESET");
    strcpy(hwReason.rawReason, "mpm_reset");

    EXPECT_EQ(classify_reboot_reason(&info, &ctx, &hwReason, &panicInfo, &fwFailure), SUCCESS);
    EXPECT_STREQ(info.customReason, "Hardware Register - MPM_RESET");
}

TEST_F(RebootClassifyTest, check_firmware_failure_EcmCrashSkippedForMediaClient) {
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
    strcpy(ctx.device_type, "mediaclient");

    EXPECT_EQ(check_firmware_failure(&ctx, &fwFailure), SUCCESS);
    EXPECT_FALSE(fwFailure.detected);
    EXPECT_FALSE(fwFailure.ecmCrashDetected);
}

TEST_F(RebootClassifyTest, check_firmware_failure_EcmLogPresentWithoutCrashString) {
    system("mkdir -p /opt/logs/PreviousLogs");
    FILE* fp = fopen("/opt/logs/PreviousLogs/messages-ecm.txt", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create ecm crash log in this environment";
    }
    fputs("ecm is healthy\n", fp);
    fclose(fp);

    EnvContext ctx;
    FirmwareFailure fwFailure;
    memset(&ctx, 0, sizeof(EnvContext));
    memset(&fwFailure, 0, sizeof(FirmwareFailure));
    strcpy(ctx.device_type, "stb");

    EXPECT_EQ(check_firmware_failure(&ctx, &fwFailure), SUCCESS);
    EXPECT_FALSE(fwFailure.detected);
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

