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

enum class HalTestSoc {
    Brcm,
    Broadcom,
    Realtek,
    Amlogic,
    Mtk,
    Mediatek,
    Unknown
};

static const char* hal_test_soc_name(HalTestSoc soc) {
    switch (soc) {
        case HalTestSoc::Brcm: return "BRCM";
        case HalTestSoc::Broadcom: return "BROADCOM";
        case HalTestSoc::Realtek: return "REALTEK";
        case HalTestSoc::Amlogic: return "AMLOGIC";
        case HalTestSoc::Mtk: return "MTK";
        case HalTestSoc::Mediatek: return "MEDIATEK";
        case HalTestSoc::Unknown: return "UNKNOWN_SOC";
    }
    return "UNKNOWN_SOC";
}

static void set_hal_test_soc(EnvContext* ctx, HalTestSoc soc) {
    if (!ctx) {
        return;
    }
    memset(ctx->soc, 0, sizeof(ctx->soc));
    strncpy(ctx->soc, hal_test_soc_name(soc), sizeof(ctx->soc) - 1);
}

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
    EXPECT_EQ(update_reboot_info(&ctx), 0);
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
    EXPECT_EQ(update_reboot_info(&ctx), 0);
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
    EXPECT_EQ(result, FAILURE);
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
    EXPECT_EQ(result, FAILURE);
}

TEST_F(LogParserTest, read_brcm_previous_reboot_reason_ParsesPrimaryTokenUppercase) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));
    setupMockFile("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_prev", "watchdog_reset,extra\n");
    g_mock_fs_enabled = true;

    int result = read_brcm_previous_reboot_reason(&hw);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(hw.rawReason, "WATCHDOG_RESET,EXTRA");
    EXPECT_STREQ(hw.mappedReason, "WATCHDOG_RESET");
}

TEST_F(LogParserTest, read_brcm_previous_reboot_reason_EmptyFileReturnsNotFound) {
    HardwareReason hw;
    memset(&hw, 0, sizeof(hw));
    setupMockFile("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_empty", "");
    g_mock_fs_enabled = true;

    int result = read_brcm_previous_reboot_reason(&hw);
    EXPECT_EQ(result, FAILURE);
}

TEST_F(LogParserTest, get_hardware_reason_brcm_InvalidParams) {
    EXPECT_EQ(read_brcm_previous_reboot_reason(nullptr), ERROR_GENERAL);
}

TEST_F(LogParserTest, get_hardware_reason_DispatchBrcmPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Brcm);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST_F(LogParserTest, get_hardware_reason_DispatchAmlogicPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Amlogic);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

TEST_F(LogParserTest, get_hardware_reason_UnknownSocFallsBackToUnknown) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Unknown);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST_F(LogParserTest, get_hardware_reason_brcm_FilePathBehavior) {
    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_hal_test_soc(&ctx, HalTestSoc::Brcm);

    int rc = read_brcm_previous_reboot_reason(&hw);
    EXPECT_TRUE(rc == SUCCESS || rc == FAILURE);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST_F(LogParserTest, get_hardware_reason_DispatchRtkAliasPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Realtek);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

TEST_F(LogParserTest, get_hardware_reason_DispatchMtkPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Mtk);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == SUCCESS || rc == FAILURE || rc == ERROR_GENERAL);
}

TEST_F(LogParserTest, get_hardware_reason_DispatchBroadcomAliasPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Broadcom);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

TEST_F(LogParserTest, get_hardware_reason_DispatchMediatekAliasPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Mediatek);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == SUCCESS || rc == FAILURE || rc == ERROR_GENERAL);
}

TEST_F(LogParserTest, get_hardware_reason_EmptySocFallbackPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST_F(LogParserTest, get_hardware_reason_brcm_ReadsAndUppercases) {
    setupMockFile("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_reason", "watchdog_reset\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_hal_test_soc(&ctx, HalTestSoc::Brcm);

    EXPECT_EQ(read_brcm_previous_reboot_reason(&hw), SUCCESS);
    EXPECT_STREQ(hw.rawReason, "WATCHDOG_RESET");
    EXPECT_STREQ(hw.mappedReason, "WATCHDOG_RESET");
}

TEST_F(LogParserTest, get_hardware_reason_DispatchAmlogicUsesReadPath) {
    setupMockFile("/sys/devices/platform/aml_pm/reset_reason", "/tmp/reboot_test/amlogic_sysfs", "12\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Amlogic);

    EXPECT_EQ(get_hardware_reason(&ctx, &hw, &info), SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
    EXPECT_EQ(info.reason[0], '\0');
}

TEST_F(LogParserTest, get_hardware_reason_DispatchMtkUsesReadPath) {
    setupMockFile("/sys/mtk_pm/boot_reason", "/tmp/reboot_test/mtk_sysfs", "0xD1\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_hal_test_soc(&ctx, HalTestSoc::Mediatek);
    strncpy(info.timestamp, "2026-03-10T11:22:33Z", sizeof(info.timestamp) - 1);

    EXPECT_EQ(get_hardware_reason(&ctx, &hw, &info), SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
    EXPECT_EQ(info.reason[0], '\0');
    EXPECT_STREQ(info.timestamp, "2026-03-10T11:22:33Z");
}

TEST_F(LogParserTest, get_hardware_reason_brcm_EmptyFileUnknown) {
    setupMockFile("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_empty_2", "");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_hal_test_soc(&ctx, HalTestSoc::Brcm);

    EXPECT_EQ(read_brcm_previous_reboot_reason(&hw), FAILURE);
    EXPECT_EQ(hw.mappedReason[0], '\0');
}

TEST_F(LogParserTest, get_hardware_reason_brcm_OpenFailsReturnsErrorGeneral) {
    g_mock_path_map["/proc/brcm/previous_reboot_reason"] = "/tmp/reboot_test/does_not_exist_brcm";
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_hal_test_soc(&ctx, HalTestSoc::Brcm);

    int rc = read_brcm_previous_reboot_reason(&hw);
    EXPECT_EQ(rc, FAILURE);
    EXPECT_EQ(hw.mappedReason[0], '\0');
}

/ ============================================================
// Tests for find_previous_reboot_log
// ============================================================

TEST_F(LogParserTest, find_previous_reboot_log_NullParameters) {
    char buf[256];
    EXPECT_EQ(find_previous_reboot_log(nullptr, sizeof(buf)), ERROR_GENERAL);
    EXPECT_EQ(find_previous_reboot_log(buf, 0),              ERROR_GENERAL);
}

TEST_F(LogParserTest, find_previous_reboot_log_NoLogsReturnsNotFound) {
    // Point LOG_PATH at an empty directory — no PreviousLogs at all
    setenv("LOG_PATH", "/tmp/reboot_test/nologs", 1);
    char out[256] = {0};
    int rc = find_previous_reboot_log(out, sizeof(out));
    EXPECT_EQ(rc, ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(out[0], '\0');
    unsetenv("LOG_PATH");
}

TEST_F(LogParserTest, find_previous_reboot_log_TimestampedSubdir) {
    // Build: /tmp/reboot_test/logs/PreviousLogs/20260101_120000/last_reboot
    //                                                           /rebootInfo.log
    system("mkdir -p /tmp/reboot_test/logs/PreviousLogs/20260101_120000");
    createTestLogFile("/tmp/reboot_test/logs/PreviousLogs/20260101_120000/last_reboot", "");
    createTestLogFile("/tmp/reboot_test/logs/PreviousLogs/20260101_120000/rebootInfo.log",
                      "RebootInitiatedBy: Servicemanager\n");
    setenv("LOG_PATH", "/tmp/reboot_test/logs", 1);
    char out[512] = {0};
    int rc = find_previous_reboot_log(out, sizeof(out));
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_STRNE(out, "");
    EXPECT_NE(strstr(out, "rebootInfo.log"), nullptr);
    unsetenv("LOG_PATH");
}

TEST_F(LogParserTest, find_previous_reboot_log_FlatFallback) {
    // No timestamped sub-dir, but flat PreviousLogs/rebootInfo.log exists
    system("mkdir -p /tmp/reboot_test/logs/PreviousLogs");
    createTestLogFile("/tmp/reboot_test/logs/PreviousLogs/rebootInfo.log",
                      "RebootInitiatedBy: WebPA\n");
    setenv("LOG_PATH", "/tmp/reboot_test/logs", 1);
    char out[512] = {0};
    int rc = find_previous_reboot_log(out, sizeof(out));
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_STREQ(out, "/tmp/reboot_test/logs/PreviousLogs/rebootInfo.log");
    unsetenv("LOG_PATH");
}

TEST_F(LogParserTest, find_previous_reboot_log_Bak1Preferred) {
    // Both flat rebootInfo.log and bak1_rebootInfo.log exist — bak1 preferred
    system("mkdir -p /tmp/reboot_test/logs/PreviousLogs");
    createTestLogFile("/tmp/reboot_test/logs/PreviousLogs/rebootInfo.log",     "old\n");
    createTestLogFile("/tmp/reboot_test/logs/PreviousLogs/bak1_rebootInfo.log","newer\n");
    setenv("LOG_PATH", "/tmp/reboot_test/logs", 1);
    char out[512] = {0};
    int rc = find_previous_reboot_log(out, sizeof(out));
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_STREQ(out, "/tmp/reboot_test/logs/PreviousLogs/bak1_rebootInfo.log");
    unsetenv("LOG_PATH");
}

TEST_F(LogParserTest, find_previous_reboot_log_Bak2WhenBak1Missing) {
    system("mkdir -p /tmp/reboot_test/logs/PreviousLogs");
    createTestLogFile("/tmp/reboot_test/logs/PreviousLogs/rebootInfo.log",     "old\n");
    createTestLogFile("/tmp/reboot_test/logs/PreviousLogs/bak2_rebootInfo.log","bak2\n");
    setenv("LOG_PATH", "/tmp/reboot_test/logs", 1);
    char out[512] = {0};
    int rc = find_previous_reboot_log(out, sizeof(out));
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_STREQ(out, "/tmp/reboot_test/logs/PreviousLogs/bak2_rebootInfo.log");
    unsetenv("LOG_PATH");
}

// ============================================================
// Tests for parse_legacy_log — raw-field (non-Previous-prefixed) parsing
// ============================================================

TEST_F(LogParserTest, parse_legacy_log_RawFields) {
    const char *testFile = "/tmp/reboot_test/raw_reboot.log";
    createTestLogFile(testFile,
                      "Thu Jan  1 12:00:00 UTC 2026 RebootReason: MAINTENANCE_REBOOT\n"
                      "Thu Jan  1 12:00:00 UTC 2026 RebootInitiatedBy: Servicemanager\n"
                      "Thu Jan  1 12:00:00 UTC 2026 RebootTime: Thu Jan  1 12:00:00 UTC 2026\n"
                      "Thu Jan  1 12:00:00 UTC 2026 CustomReason: MAINTENANCE_REBOOT\n"
                      "Thu Jan  1 12:00:00 UTC 2026 OtherReason: Scheduled maintenance\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));
    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "Servicemanager");
    EXPECT_STREQ(info.customReason, "MAINTENANCE_REBOOT");
    EXPECT_STREQ(info.otherReason, "Scheduled maintenance");
}

TEST_F(LogParserTest, parse_legacy_log_HalSysReboot) {
    // When RebootInitiatedBy is HAL_SYS_Reboot, real initiator and reason
    // are extracted from the "Triggered from" RebootReason line.
    const char *testFile = "/tmp/reboot_test/hal_sys_reboot.log";
    createTestLogFile(testFile,
                      "Thu Jan  1 12:00:00 UTC 2026 RebootReason: Triggered from WebPA FIRMWARE_FAILURE (XRE)\n"
                      "Thu Jan  1 12:00:00 UTC 2026 RebootInitiatedBy: HAL_SYS_Reboot\n"
                      "Thu Jan  1 12:00:00 UTC 2026 RebootTime: Thu Jan  1 12:00:00 UTC 2026\n"
                      "Thu Jan  1 12:00:00 UTC 2026 CustomReason: FIRMWARE_FAILURE\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));
    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    // Initiator should be unwrapped from "Triggered from WebPA ..."
    EXPECT_STREQ(info.source, "WebPA");
    // OtherReason should be the text between initiator and '('
    EXPECT_STREQ(info.otherReason, "FIRMWARE_FAILURE");
    EXPECT_STREQ(info.customReason, "FIRMWARE_FAILURE");
}

TEST_F(LogParserTest, parse_legacy_log_HalSysReboot_NoTriggerLine) {
    // HAL_SYS_Reboot but no matching Triggered-from RebootReason line
    // source stays as HAL_SYS_Reboot, no crash
    const char *testFile = "/tmp/reboot_test/hal_notrigger.log";
    createTestLogFile(testFile,
                      "Thu Jan  1 12:00:00 UTC 2026 RebootInitiatedBy: HAL_SYS_Reboot\n"
                      "Thu Jan  1 12:00:00 UTC 2026 CustomReason: UNKNOWN\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));
    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source, "HAL_SYS_Reboot");
    EXPECT_STREQ(info.customReason, "UNKNOWN");
}

TEST_F(LogParserTest, parse_legacy_log_PreviousPrefixTakesPriority) {
    // File contains both raw and Previous-prefixed fields;
    // Previous-prefixed values must win.
    const char *testFile = "/tmp/reboot_test/mixed_fields.log";
    createTestLogFile(testFile,
                      "Thu Jan  1 12:00:00 UTC 2026 RebootInitiatedBy: RawSource\n"
                      "Thu Jan  1 12:00:00 UTC 2026 PreviousRebootInitiatedBy: LegacySource\n"
                      "Thu Jan  1 12:00:00 UTC 2026 RebootTime: 2026-01-01 12:00:00 UTC\n"
                      "Thu Jan  1 12:00:00 UTC 2026 PreviousRebootTime: 2025-12-31 08:00:00 UTC\n"
                      "Thu Jan  1 12:00:00 UTC 2026 CustomReason: RawCustom\n"
                      "Thu Jan  1 12:00:00 UTC 2026 PreviousCustomReason: LegacyCustom\n"
                      "Thu Jan  1 12:00:00 UTC 2026 PreviousOtherReason: LegacyOther\n");

    RebootInfo info;
    memset(&info, 0, sizeof(RebootInfo));
    int result = parse_legacy_log(testFile, &info);
    EXPECT_EQ(result, SUCCESS);
    EXPECT_STREQ(info.source,       "LegacySource");
    EXPECT_STREQ(info.timestamp,    "2025-12-31 08:00:00 UTC");
    EXPECT_STREQ(info.customReason, "LegacyCustom");
    EXPECT_STREQ(info.otherReason,  "LegacyOther");
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

