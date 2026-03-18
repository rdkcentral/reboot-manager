#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

static bool g_mock_props_enabled = false;
static bool g_mock_fs_enabled = false;
static std::unordered_map<std::string, std::string> g_mock_path_map;
static char g_soc_value[64] = {0};
static char g_build_type_value[64] = {0};
static char g_device_type_value[64] = {0};
static char g_platco_value[16] = {0};
static char g_llama_value[16] = {0};
static char g_stt_value[16] = {0};

extern "C" {
    #include "update-reboot-info.h"
    #include "rdk_fwdl_utils.h"
    #include "rdk_debug.h"

    int g_rdk_logger_enabled = 0;

    int getDevicePropertyData(const char* key __attribute__((unused)),
                              char* value __attribute__((unused)),
                              int size __attribute__((unused))) {
        if (g_mock_props_enabled && key && value && size > 0) {
            const char* selected = nullptr;
            if (strcmp(key, "SOC") == 0 && g_soc_value[0] != '\0') {
                selected = g_soc_value;
            } else if (strcmp(key, "BUILD_TYPE") == 0 && g_build_type_value[0] != '\0') {
                selected = g_build_type_value;
            } else if (strcmp(key, "DEVICE_TYPE") == 0 && g_device_type_value[0] != '\0') {
                selected = g_device_type_value;
            } else if (strcmp(key, "PLATCO_SUPPORT") == 0 && g_platco_value[0] != '\0') {
                selected = g_platco_value;
            } else if (strcmp(key, "LLAMA_SUPPORT") == 0 && g_llama_value[0] != '\0') {
                selected = g_llama_value;
            } else if (strcmp(key, "REBOOT_INFO_STT_SUPPORT") == 0 && g_stt_value[0] != '\0') {
                selected = g_stt_value;
            }

            if (selected) {
                strncpy(value, selected, (size_t)size - 1);
                value[size - 1] = '\0';
                return UTILS_SUCCESS;
            }
        }
        return UTILS_FAILURE; // trigger defaults in parse_device_properties
    }

    void t2_event_d(const char* marker __attribute__((unused)),
                    int value __attribute__((unused))) { }
    void t2_event_s(const char* marker __attribute__((unused)),
                    const char* value __attribute__((unused))) { }
}

extern "C" int access(const char* pathname, int mode) {
    if (g_mock_fs_enabled && pathname) {
        auto it = g_mock_path_map.find(pathname);
        if (it != g_mock_path_map.end()) {
            (void)mode;
            return 0;
        }
    }
    return faccessat(AT_FDCWD, pathname, mode, 0);
}

static int mode_to_flags(const char* mode) {
    if (!mode || mode[0] == '\0') {
        return -1;
    }
    if (strcmp(mode, "r") == 0) return O_RDONLY;
    if (strcmp(mode, "r+") == 0) return O_RDWR;
    if (strcmp(mode, "w") == 0) return O_WRONLY | O_CREAT | O_TRUNC;
    if (strcmp(mode, "w+") == 0) return O_RDWR | O_CREAT | O_TRUNC;
    if (strcmp(mode, "a") == 0) return O_WRONLY | O_CREAT | O_APPEND;
    if (strcmp(mode, "a+") == 0) return O_RDWR | O_CREAT | O_APPEND;
    return -1;
}

extern "C" FILE* fopen(const char* pathname, const char* mode) {
    const char* resolvedPath = pathname;
    if (g_mock_fs_enabled && pathname) {
        auto it = g_mock_path_map.find(pathname);
        if (it != g_mock_path_map.end()) {
            resolvedPath = it->second.c_str();
        }
    }

    int flags = mode_to_flags(mode);
    if (flags < 0) {
        errno = EINVAL;
        return nullptr;
    }

    int fd = open(resolvedPath, flags, 0644);
    if (fd < 0) {
        return nullptr;
    }

    FILE* fp = fdopen(fd, mode);
    if (!fp) {
        close(fd);
        return nullptr;
    }
    return fp;
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_log_parser_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
using namespace std;

class LogParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        system("mkdir -p /tmp/reboot_test");
        g_mock_props_enabled = false;
        g_mock_fs_enabled = false;
        g_mock_path_map.clear();
        memset(g_soc_value, 0, sizeof(g_soc_value));
        memset(g_build_type_value, 0, sizeof(g_build_type_value));
        memset(g_device_type_value, 0, sizeof(g_device_type_value));
        memset(g_platco_value, 0, sizeof(g_platco_value));
        memset(g_llama_value, 0, sizeof(g_llama_value));
        memset(g_stt_value, 0, sizeof(g_stt_value));
    }

    void TearDown() override {
        // Clean up test files
        system("rm -rf /tmp/reboot_test");
        g_mock_fs_enabled = false;
        g_mock_path_map.clear();
    }

    void createTestLogFile(const char* path, const char* content) {
        FILE* fp = fopen(path, "w");
        ASSERT_NE(fp, nullptr);
        fprintf(fp, "%s", content);
        fclose(fp);
    }

    void setupMockFile(const char* virtualPath, const char* realPath, const char* content) {
        FILE* fp = fopen(realPath, "w");
        ASSERT_NE(fp, nullptr);
        fprintf(fp, "%s", content);
        fclose(fp);
        g_mock_path_map[virtualPath] = realPath;
    }
};

// Tests for parse_device_properties
TEST_F(LogParserTest, parse_device_properties_NullContext) {
    EXPECT_EQ(parse_device_properties(nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, parse_device_properties_Success) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    // Create a test device.properties file
    createTestLogFile("/tmp/reboot_test/device.properties",
                      "SOC=BRCM\n"
                      "BUILD_TYPE=prod\n"
                      "DEVICE_TYPE=stb\n"
                      "PLATCO_SUPPORT=true\n"
                      "LLAMA_SUPPORT=false\n");

    // Since parse_device_properties reads from /etc/device.properties,
    // we test the fallback parsing path
    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
}

TEST_F(LogParserTest, parse_device_properties_InitializesDefaults) {
    EnvContext ctx;
    memset(&ctx, 0xff, sizeof(EnvContext)); // Fill with garbage

    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_FALSE(ctx.platcoSupport);
    EXPECT_FALSE(ctx.llamaSupport);
    EXPECT_FALSE(ctx.rebootInfoSttSupport);
}

TEST_F(LogParserTest, parse_device_properties_UsesMockPropertyValues) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    g_mock_props_enabled = true;
    strncpy(g_soc_value, "BRCM", sizeof(g_soc_value) - 1);
    strncpy(g_build_type_value, "prod", sizeof(g_build_type_value) - 1);
    strncpy(g_device_type_value, "stb", sizeof(g_device_type_value) - 1);
    strncpy(g_platco_value, "true", sizeof(g_platco_value) - 1);
    strncpy(g_llama_value, "false", sizeof(g_llama_value) - 1);
    strncpy(g_stt_value, "true", sizeof(g_stt_value) - 1);

    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(ctx.soc, "BRCM");
    EXPECT_STREQ(ctx.buildType, "prod");
    EXPECT_STREQ(ctx.device_type, "stb");
    EXPECT_TRUE(ctx.platcoSupport);
    EXPECT_FALSE(ctx.llamaSupport);
    EXPECT_TRUE(ctx.rebootInfoSttSupport);
}

TEST_F(LogParserTest, parse_device_properties_UppercaseTrueValues) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    g_mock_props_enabled = true;
    strncpy(g_soc_value, "RTK", sizeof(g_soc_value) - 1);
    strncpy(g_device_type_value, "stb", sizeof(g_device_type_value) - 1);
    strncpy(g_platco_value, "TRUE", sizeof(g_platco_value) - 1);
    strncpy(g_llama_value, "TRUE", sizeof(g_llama_value) - 1);
    strncpy(g_stt_value, "TRUE", sizeof(g_stt_value) - 1);

    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_TRUE(ctx.platcoSupport);
    EXPECT_TRUE(ctx.llamaSupport);
    EXPECT_TRUE(ctx.rebootInfoSttSupport);
}

TEST_F(LogParserTest, parse_device_properties_FallbackFromEtcDeviceProperties) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    g_mock_props_enabled = false;
    setupMockFile("/etc/device.properties", "/tmp/reboot_test/device_props_fallback",
                  "SOC=AMLOGIC\n"
                  "DEVICE_NAME=tv_device\n");
    g_mock_fs_enabled = true;

    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(ctx.soc, "AMLOGIC");
    EXPECT_STREQ(ctx.device_type, "tv_device");
}

TEST_F(LogParserTest, parse_device_properties_FallbackUsesDeviceTypeKey) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    g_mock_props_enabled = false;
    setupMockFile("/etc/device.properties", "/tmp/reboot_test/device_props_device_type",
                  "SOC=BRCM\n"
                  "DEVICE_TYPE=hybrid_stb\n");
    g_mock_fs_enabled = true;

    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(ctx.soc, "BRCM");
    EXPECT_STREQ(ctx.device_type, "hybrid_stb");
}

TEST_F(LogParserTest, parse_device_properties_FallbackOpenFailureReturnsFailure) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    g_mock_props_enabled = false;
    g_mock_path_map["/etc/device.properties"] = "/tmp/reboot_test/nonexistent_props";
    g_mock_fs_enabled = true;

    int result = parse_device_properties(&ctx);
    EXPECT_EQ(result, FAILURE);
}

TEST_F(LogParserTest, free_env_context_ResetsMemory) {
    EnvContext ctx;
    memset(&ctx, 0xAB, sizeof(EnvContext));
    free_env_context(&ctx);
    EXPECT_EQ(ctx.soc[0], '\0');
    EXPECT_EQ(ctx.buildType[0], '\0');
    EXPECT_EQ(ctx.device_type[0], '\0');
}

// Tests for should_update_reboot_info
TEST_F(LogParserTest, update_reboot_info_NullContext) {
    EXPECT_EQ(update_reboot_info(nullptr), 0);
}

TEST_F(LogParserTest, update_reboot_info_PlatcoFirstInvocation) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.platcoSupport = true;

    // Remove the invoked flag
    system("rm -f /tmp/Update_rebootInfo_invoked");

    // Should allow update on first invocation
    EXPECT_EQ(update_reboot_info(&ctx), 1);
}

TEST_F(LogParserTest, update_reboot_info_RequiresFlagsAfterFirstInvoke) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.platcoSupport = true;

    // Create invoked flag
    createTestLogFile("/tmp/Update_rebootInfo_invoked", "1\n");

    // Remove other flags
    system("rm -f /tmp/stt_received /tmp/rebootInfo_Updated");

    // Should block update without required flags
    EXPECT_EQ(update_reboot_info(&ctx), 0);

    // Clean up
    system("rm -f /tmp/Update_rebootInfo_invoked");
}

TEST_F(LogParserTest, update_reboot_info_AllowsWithFlags) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.platcoSupport = true;

    // Create all required flags
    createTestLogFile("/tmp/Update_rebootInfo_invoked", "1\n");
    createTestLogFile("/tmp/stt_received", "1\n");
    createTestLogFile("/tmp/rebootInfo_Updated", "1\n");

    EXPECT_EQ(update_reboot_info(&ctx), 1);

    // Clean up
    system("rm -f /tmp/Update_rebootInfo_invoked /tmp/stt_received /tmp/rebootInfo_Updated");
}

TEST_F(LogParserTest, update_reboot_info_NonPlatcoRequiresFlags) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));

    system("rm -f /tmp/stt_received /tmp/rebootInfo_Updated");
    EXPECT_EQ(update_reboot_info(&ctx), 0);

    createTestLogFile("/tmp/stt_received", "1\n");
    createTestLogFile("/tmp/rebootInfo_Updated", "1\n");
    EXPECT_EQ(update_reboot_info(&ctx), 1);

    system("rm -f /tmp/stt_received /tmp/rebootInfo_Updated");
}

TEST_F(LogParserTest, update_reboot_info_LlamaFirstInvocationAllowed) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.llamaSupport = true;

    system("rm -f /tmp/Update_rebootInfo_invoked");
    EXPECT_EQ(update_reboot_info(&ctx), 1);
}

TEST_F(LogParserTest, update_reboot_info_LlamaInvokedNeedsFlags) {
    EnvContext ctx;
    memset(&ctx, 0, sizeof(EnvContext));
    ctx.llamaSupport = true;

    createTestLogFile("/tmp/Update_rebootInfo_invoked", "1\n");
    system("rm -f /tmp/stt_received /tmp/rebootInfo_Updated");
    EXPECT_EQ(update_reboot_info(&ctx), 0);

    createTestLogFile("/tmp/stt_received", "1\n");
    createTestLogFile("/tmp/rebootInfo_Updated", "1\n");
    EXPECT_EQ(update_reboot_info(&ctx), 1);

    system("rm -f /tmp/Update_rebootInfo_invoked /tmp/stt_received /tmp/rebootInfo_Updated");
}

// Tests for parse_legacy_log
TEST_F(LogParserTest, parse_legacy_log_NullParameters) {
    RebootInfo info;
    EXPECT_EQ(parse_legacy_log(nullptr, &info), ERROR_GENERAL);
    EXPECT_EQ(parse_legacy_log("/tmp/test.log", nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, parse_legacy_log_FileNotFound) {
    RebootInfo info;
    EXPECT_EQ(parse_legacy_log("/tmp/nonexistent_file.log", &info), ERROR_FILE_NOT_FOUND);
}

TEST_F(LogParserTest, parse_legacy_log_EmptyPathFileNotFound) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    EXPECT_EQ(parse_legacy_log("", &info), ERROR_FILE_NOT_FOUND);
}

TEST_F(LogParserTest, parse_legacy_log_EmptyFile) {
    const char* testFile = "/tmp/reboot_test/empty.log";
    createTestLogFile(testFile, "");

    RebootInfo info;
    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, ERROR_PARSE_FAILED);
}

TEST_F(LogParserTest, parse_legacy_log_Success) {
    const char* testFile = "/tmp/reboot_test/reboot.log";
    createTestLogFile(testFile,
                      "PreviousRebootInitiatedBy: SystemService\n"
                      "PreviousRebootTime: 2025-11-28 10:30:00 UTC\n"
                      "PreviousCustomReason: ScheduledReboot\n"
                      "PreviousOtherReason: Reboot due to scheduled maintenance\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));

    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "SystemService");
    EXPECT_STREQ(info.timestamp, "2025-11-28 10:30:00 UTC");
    EXPECT_STREQ(info.customReason, "ScheduledReboot");
    EXPECT_STREQ(info.otherReason, "Reboot due to scheduled maintenance");
}

TEST_F(LogParserTest, parse_legacy_log_PartialData) {
    const char* testFile = "/tmp/reboot_test/partial.log";
    createTestLogFile(testFile,
                      "PreviousRebootInitiatedBy: WatchDog\n"
                      "Some other line\n"
                      "PreviousCustomReason: WATCHDOG_RESET\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));

    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "WatchDog");
    EXPECT_STREQ(info.customReason, "WATCHDOG_RESET");
}

TEST_F(LogParserTest, parse_legacy_log_OnlyOtherReason) {
    const char* testFile = "/tmp/reboot_test/other_only.log";
    createTestLogFile(testFile,
                      "junk line\n"
                      "PreviousOtherReason: some detail\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));

    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.otherReason, "some detail");
}

TEST_F(LogParserTest, parse_legacy_log_TrimmedValuesWithWhitespace) {
    const char* testFile = "/tmp/reboot_test/trimmed_fields.log";
    createTestLogFile(testFile,
                      "PreviousRebootInitiatedBy:\t  ServiceMgr   \n"
                      "PreviousRebootTime:   2026-03-10 10:11:12 UTC\r\n"
                      "PreviousCustomReason:\tMaintenanceReboot\t\n"
                      "PreviousOtherReason:   reason with spaces   \r\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));

    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "ServiceMgr");
    EXPECT_STREQ(info.timestamp, "2026-03-10 10:11:12 UTC");
    EXPECT_STREQ(info.customReason, "MaintenanceReboot");
    EXPECT_STREQ(info.otherReason, "reason with spaces");
}

// Tests for read_brcm_previous_reboot_reason
TEST_F(LogParserTest, read_brcm_previous_reboot_reason_NullParameter) {
    EXPECT_EQ(read_brcm_previous_reboot_reason(nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, read_brcm_previous_reboot_reason_FileNotFound) {
    HardwareReason hw;
    int result = read_brcm_previous_reboot_reason(&hw);
    EXPECT_EQ(result, ERROR_FILE_NOT_FOUND);
}

TEST_F(LogParserTest, read_brcm_previous_reboot_reason_ParsesPrimaryTokenUppercase) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));
    setupMockFile("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_prev", "watchdog_reset,extra\n");
    g_mock_fs_enabled = true;

    int result = read_brcm_previous_reboot_reason(&hw);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(hw.rawReason, "watchdog_reset,extra");
    EXPECT_STREQ(hw.mappedReason, "WATCHDOG_RESET");
}

TEST_F(LogParserTest, read_brcm_previous_reboot_reason_EmptyFileReturnsNotFound) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));
    setupMockFile("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_empty", "");
    g_mock_fs_enabled = true;

    int result = read_brcm_previous_reboot_reason(&hw);
    EXPECT_EQ(result, ERROR_FILE_NOT_FOUND);
}

// Tests for read_rtk_wakeup_reason
TEST_F(LogParserTest, read_rtk_wakeup_reason_NullParameter) {
    EXPECT_EQ(read_rtk_wakeup_reason(nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, read_rtk_wakeup_reason_SystemCmdlinePath) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));

    int result = read_rtk_wakeup_reason(&hw);
    EXPECT_TRUE(result == SUCCESS || result == FAILURE);
    if (result == SUCCESS) {
        EXPECT_NE(hw.mappedReason[0], '\0');
    }
}

TEST_F(LogParserTest, read_rtk_wakeup_reason_ParsesWakeupReason) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));
    setupMockFile("/proc/cmdline", "/tmp/reboot_test/cmdline", "foo=1 wakeupreason=thermal_reboot bar=2\n");
    g_mock_fs_enabled = true;

    int result = read_rtk_wakeup_reason(&hw);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "THERMAL_REBOOT");
}

TEST_F(LogParserTest, read_rtk_wakeup_reason_NoKeyFailure) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));
    setupMockFile("/proc/cmdline", "/tmp/reboot_test/cmdline_no_key", "foo=1 bar=2\n");
    g_mock_fs_enabled = true;

    int result = read_rtk_wakeup_reason(&hw);
    EXPECT_EQ(result, FAILURE);
}

// Tests for read_amlogic_reset_reason
TEST_F(LogParserTest, read_amlogic_reset_reason_NullParameters) {
    HardwareReason hw;
    RebootInfo info;

    EXPECT_EQ(read_amlogic_reset_reason(nullptr, &info), ERROR_GENERAL);
    EXPECT_EQ(read_amlogic_reset_reason(&hw, nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, read_amlogic_reset_reason_FileNotFound) {
    HardwareReason hw;
    RebootInfo info;
    int result = read_amlogic_reset_reason(&hw, &info);
    EXPECT_EQ(result, ERROR_FILE_NOT_FOUND);
}

TEST_F(LogParserTest, read_amlogic_reset_reason_KernelPanicCase) {
    HardwareReason hw;
    RebootInfo info;
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T20:00:00Z", sizeof(info.timestamp) - 1);
    setupMockFile("/sys/devices/platform/aml_pm/reset_reason", "/tmp/reboot_test/amlogic_case12", "12\n");
    g_mock_fs_enabled = true;

    int result = read_amlogic_reset_reason(&hw, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "KERNEL_PANIC");
    EXPECT_STREQ(info.reason, "KERNEL_PANIC");
    EXPECT_STREQ(info.timestamp, "2026-03-10T20:00:00Z");
}

TEST_F(LogParserTest, read_amlogic_reset_reason_DefaultUnknownCase) {
    HardwareReason hw;
    RebootInfo info;
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    setupMockFile("/sys/devices/platform/aml_pm/reset_reason", "/tmp/reboot_test/amlogic_case99", "99\n");
    g_mock_fs_enabled = true;

    int result = read_amlogic_reset_reason(&hw, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN_RESET");
    EXPECT_STREQ(info.reason, "UNKNOWN_RESET");
}

TEST_F(LogParserTest, read_amlogic_reset_reason_AllKnownCasesTable) {
    struct AmlCase {
        int code;
        const char* expectedReason;
    };

    const AmlCase cases[] = {
        {0, "POWER_ON_RESET"},
        {1, "SOFTWARE_MASTER_RESET"},
        {2, "FACTORY_RESET"},
        {3, "UPDATE_BOOT"},
        {4, "FAST_BOOT"},
        {5, "SUSPEND_BOOT"},
        {6, "HIBERNATE_BOOT"},
        {7, "FASTBOOT_BOOTLOADER"},
        {8, "SHUTDOWN_REBOOT"},
        {9, "RPMPB_REBOOT"},
        {10, "THERMAL_REBOOT"},
        {11, "CRASH_REBOOT"},
        {12, "KERNEL_PANIC"},
        {13, "WATCHDOG_REBOOT"},
        {14, "AMLOGIC_DDR_SHA2_REBOOT"},
        {15, "FFV_REBOOT"}
    };

    for (const auto& testCase : cases) {
        HardwareReason hw;
        RebootInfo info;
        memset(&hw, 0, sizeof(hw));
        memset(&info, 0, sizeof(info));
        strncpy(info.timestamp, "2026-03-10T21:00:00Z", sizeof(info.timestamp) - 1);

	char value[16] = {0};
        snprintf(value, sizeof(value), "%d\n", testCase.code);
        setupMockFile("/sys/devices/platform/aml_pm/reset_reason", "/tmp/reboot_test/amlogic_case_tbl", value);
        g_mock_fs_enabled = true;

        int result = read_amlogic_reset_reason(&hw, &info);
        EXPECT_EQ(result, SUCCESS);
        EXPECT_STREQ(hw.mappedReason, testCase.expectedReason);
        EXPECT_STREQ(info.reason, testCase.expectedReason);
        EXPECT_STREQ(info.timestamp, "2026-03-10T21:00:00Z");
    }
}

// Tests for read_mtk_reset_reason
TEST_F(LogParserTest, read_mtk_reset_reason_NullParameters) {
    HardwareReason hw;
    RebootInfo info;

    EXPECT_EQ(read_mtk_reset_reason(nullptr, &info), ERROR_GENERAL);
    EXPECT_EQ(read_mtk_reset_reason(&hw, nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, read_mtk_reset_reason_FileNotFound) {
    HardwareReason hw;
    RebootInfo info;
    int result = read_mtk_reset_reason(&hw, &info);
    EXPECT_EQ(result, FAILURE);
}

TEST_F(LogParserTest, read_mtk_reset_reason_HexWatchdogCase) {
    HardwareReason hw;
    RebootInfo info;
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T20:20:00Z", sizeof(info.timestamp) - 1);
    setupMockFile("/sys/mtk_pm/boot_reason", "/tmp/reboot_test/mtk_case_e0", "0xE0\n");
    g_mock_fs_enabled = true;

    int result = read_mtk_reset_reason(&hw, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "WATCHDOG_REBOOT");
    EXPECT_STREQ(info.reason, "WATCHDOG_REBOOT");
    EXPECT_STREQ(info.timestamp, "2026-03-10T20:20:00Z");
}

TEST_F(LogParserTest, read_mtk_reset_reason_DecimalPowerOnCase) {
    HardwareReason hw;
    RebootInfo info;
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    setupMockFile("/sys/mtk_pm/boot_reason", "/tmp/reboot_test/mtk_case_00", "0\n");
    g_mock_fs_enabled = true;

    int result = read_mtk_reset_reason(&hw, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "POWER_ON_RESET");
    EXPECT_STREQ(info.reason, "POWER_ON_RESET");
}

TEST_F(LogParserTest, read_mtk_reset_reason_AllMappedCasesTable) {
    struct MtkCase {
        const char* value;
        const char* expectedReason;
    };

    const MtkCase cases[] = {
        {"0x00\n", "POWER_ON_RESET"},
        {"0xD1\n", "SOFTWARE_MASTER_RESET"},
        {"0xE4\n", "THERMAL_REBOOT"},
        {"0xEE\n", "KERNEL_PANIC"},
        {"0xE0\n", "WATCHDOG_REBOOT"},
        {"0x99\n", "UNKNOWN_RESET"}
    };

    for (const auto& testCase : cases) {
        HardwareReason hw;
        RebootInfo info;
        memset(&hw, 0, sizeof(hw));
        memset(&info, 0, sizeof(info));
        strncpy(info.timestamp, "2026-03-10T21:30:00Z", sizeof(info.timestamp) - 1);

        setupMockFile("/sys/mtk_pm/boot_reason", "/tmp/reboot_test/mtk_case_tbl", testCase.value);
        g_mock_fs_enabled = true;

        int result = read_mtk_reset_reason(&hw, &info);
        EXPECT_EQ(result, SUCCESS);
        EXPECT_STREQ(hw.mappedReason, testCase.expectedReason);
        EXPECT_STREQ(info.reason, testCase.expectedReason);
        EXPECT_STREQ(info.timestamp, "2026-03-10T21:30:00Z");
    }
}

TEST_F(LogParserTest, read_rtk_wakeup_reason_EmptyValueStillSuccessPath) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));
    setupMockFile("/proc/cmdline", "/tmp/reboot_test/cmdline_empty_value", "foo=1 wakeupreason= bar=2\n");
    g_mock_fs_enabled = true;

    int result = read_rtk_wakeup_reason(&hw);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_EQ(hw.mappedReason[0], '\0');
}

GTEST_API_ int main(int argc, char *argv[]) {
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];

    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s",
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);

    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);

    cout << "Starting REBOOT_LOG_PARSER GTEST ===================>" << endl;
    return RUN_ALL_TESTS();
}

