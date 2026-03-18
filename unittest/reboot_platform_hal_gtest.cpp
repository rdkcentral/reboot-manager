#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static bool g_mock_fs_enabled = false;
static std::unordered_map<std::string, std::string> g_mock_path_map;

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

extern "C" {
    #include "update-reboot-info.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_platform_hal_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

enum class TestSoc {
    Brcm,
    Broadcom,
    Rtk,
    Realtek,
    Amlogic,
    Mtk,
    Mediatek,
    Unknown
};

static const char* test_soc_name(TestSoc soc) {
    switch (soc) {
        case TestSoc::Brcm: return "BRCM";
        case TestSoc::Broadcom: return "BROADCOM";
        case TestSoc::Rtk: return "RTK";
        case TestSoc::Realtek: return "REALTEK";
        case TestSoc::Amlogic: return "AMLOGIC";
        case TestSoc::Mtk: return "MTK";
        case TestSoc::Mediatek: return "MEDIATEK";
        case TestSoc::Unknown: return "UNKNOWN_SOC";
    }
    return "UNKNOWN_SOC";
}

static void set_test_soc(EnvContext* ctx, TestSoc soc) {
    if (!ctx) {
        return;
    }
    memset(ctx->soc, 0, sizeof(ctx->soc));
    strncpy(ctx->soc, test_soc_name(soc), sizeof(ctx->soc) - 1);
}

static void setup_mock_file(const char* virtualPath, const char* realPath, const char* content) {
    FILE* fp = fopen(realPath, "w");
    ASSERT_NE(fp, nullptr);
    fputs(content, fp);
    fclose(fp);
    g_mock_path_map[virtualPath] = realPath;
}

class PlatformHalMockFsTest : public ::testing::Test {
protected:
    void SetUp() override {
        system("mkdir -p /tmp/reboot_test");
        g_mock_path_map.clear();
        g_mock_fs_enabled = false;
    }

    void TearDown() override {
        g_mock_fs_enabled = false;
        g_mock_path_map.clear();
        system("rm -rf /tmp/reboot_test");
    }
};

TEST(PlatformHalSmokeTest, get_hardware_reason_brcm_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_brcm(nullptr, nullptr), ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_rtk_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_rtk(nullptr, nullptr), ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_amlogic_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_amlogic(nullptr, nullptr), ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_mtk_InvalidParams) {
    EXPECT_EQ(get_hardware_reason_mtk(nullptr, nullptr), ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_DispatchBrcmPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Brcm);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_DispatchAmlogicPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Amlogic);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_UnknownSocFallsBackToUnknown) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Unknown);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_EQ(rc, SUCCESS);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST(PlatformHalSmokeTest, get_hardware_reason_brcm_FilePathBehavior) {
    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Brcm);

    int rc = get_hardware_reason_brcm(&ctx, &hw);
    EXPECT_TRUE(rc == SUCCESS || rc == ERROR_GENERAL);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST(PlatformHalSmokeTest, get_hardware_reason_rtk_SystemCmdlineBehavior) {
    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Rtk);

    int rc = get_hardware_reason_rtk(&ctx, &hw);
    EXPECT_TRUE(rc == SUCCESS || rc == ERROR_GENERAL);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST(PlatformHalSmokeTest, get_hardware_reason_amlogic_FilePathBehavior) {
    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Amlogic);

    int rc = get_hardware_reason_amlogic(&ctx, &hw);
    EXPECT_TRUE(rc == SUCCESS || rc == ERROR_GENERAL);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST(PlatformHalSmokeTest, get_hardware_reason_mtk_FilePathBehavior) {
    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Mtk);

    int rc = get_hardware_reason_mtk(&ctx, &hw);
    EXPECT_TRUE(rc == SUCCESS || rc == ERROR_GENERAL);
    EXPECT_NE(hw.mappedReason[0], '\0');
}

TEST(PlatformHalSmokeTest, get_hardware_reason_DispatchRtkAliasPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Realtek);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == SUCCESS || rc == FAILURE || rc == ERROR_GENERAL);
    if (rc == SUCCESS) {
        EXPECT_NE(hw.mappedReason[0], '\0');
    }
}

TEST(PlatformHalSmokeTest, get_hardware_reason_DispatchMtkPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Mtk);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == SUCCESS || rc == FAILURE || rc == ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_DispatchBroadcomAliasPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Broadcom);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_DispatchMediatekAliasPath) {
    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Mediatek);

    int rc = get_hardware_reason(&ctx, &hw, &info);
    EXPECT_TRUE(rc == SUCCESS || rc == FAILURE || rc == ERROR_GENERAL);
}

TEST(PlatformHalSmokeTest, get_hardware_reason_EmptySocFallbackPath) {
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

TEST_F(PlatformHalMockFsTest, get_hardware_reason_brcm_ReadsAndUppercases) {
    setup_mock_file("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_reason", "watchdog_reset\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Brcm);

    EXPECT_EQ(get_hardware_reason_brcm(&ctx, &hw), SUCCESS);
    EXPECT_STREQ(hw.rawReason, "WATCHDOG_RESET");
    EXPECT_STREQ(hw.mappedReason, "WATCHDOG_RESET");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_rtk_ParsesWakeupReason) {
    setup_mock_file("/proc/cmdline", "/tmp/reboot_test/cmdline", "console=ttyS0 wakeupreason=kernel_panic root=/dev/mmcblk0\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Rtk);

    EXPECT_EQ(get_hardware_reason_rtk(&ctx, &hw), SUCCESS);
    EXPECT_STREQ(hw.rawReason, "KERNEL_PANIC");
    EXPECT_STREQ(hw.mappedReason, "KERNEL_PANIC");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_amlogic_MapsResetCode) {
    setup_mock_file("/sys/class/aml_reboot/reboot_reason", "/tmp/reboot_test/amlogic_reason", "3\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Amlogic);

    EXPECT_EQ(get_hardware_reason_amlogic(&ctx, &hw), SUCCESS);
    EXPECT_STREQ(hw.rawReason, "3");
    EXPECT_STREQ(hw.mappedReason, "KERNEL_PANIC");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_mtk_MapsHexCode) {
    setup_mock_file("/sys/mtk_pm/boot_reason", "/tmp/reboot_test/mtk_reason", "0xE0\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Mtk);

    EXPECT_EQ(get_hardware_reason_mtk(&ctx, &hw), SUCCESS);
    EXPECT_STREQ(hw.rawReason, "0xE0");
    EXPECT_STREQ(hw.mappedReason, "WATCHDOG_REBOOT");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_DispatchAmlogicUsesReadPath) {
    setup_mock_file("/sys/devices/platform/aml_pm/reset_reason", "/tmp/reboot_test/amlogic_sysfs", "12\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Amlogic);

    EXPECT_EQ(get_hardware_reason(&ctx, &hw, &info), ERROR_GENERAL);
    EXPECT_EQ(hw.mappedReason[0], '\0');
    EXPECT_EQ(info.reason[0], '\0');
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_DispatchMtkUsesReadPath) {
    setup_mock_file("/sys/mtk_pm/boot_reason", "/tmp/reboot_test/mtk_sysfs", "0xD1\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    memset(&info, 0, sizeof(info));
    set_test_soc(&ctx, TestSoc::Mediatek);
    strncpy(info.timestamp, "2026-03-10T11:22:33Z", sizeof(info.timestamp) - 1);

    EXPECT_EQ(get_hardware_reason(&ctx, &hw, &info), ERROR_GENERAL);
    EXPECT_EQ(hw.mappedReason[0], '\0');
    EXPECT_EQ(info.reason[0], '\0');
    EXPECT_STREQ(info.timestamp, "2026-03-10T11:22:33Z");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_amlogic_AllMapCasesTable) {
    struct Case {
        int code;
        const char* expected;
    };
    const Case cases[] = {
        {0, "POWER_ON_RESET"},
        {1, "WATCHDOG_RESET"},
        {2, "SOFTWARE_RESET"},
        {3, "KERNEL_PANIC"},
        {4, "THERMAL_RESET"},
        {5, "HARDWARE_RESET"},
        {6, "SUSPEND_WAKEUP"},
        {7, "REMOTE_WAKEUP"},
        {8, "RTC_WAKEUP"},
        {9, "GPIO_WAKEUP"},
        {10, "FACTORY_RESET"},
        {11, "UPGRADE_RESET"},
        {12, "FASTBOOT_RESET"},
        {13, "CRASH_DUMP_RESET"},
        {14, "RECOVERY_RESET"},
        {15, "BOOTLOADER_RESET"},
        {99, "UNKNOWN"}
    };

    EnvContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    set_test_soc(&ctx, TestSoc::Amlogic);

    for (const auto& testCase : cases) {
        HardwareReason hw;
        memset(&hw, 0, sizeof(hw));
        char value[16] = {0};
        snprintf(value, sizeof(value), "%d\n", testCase.code);
        setup_mock_file("/sys/class/aml_reboot/reboot_reason", "/tmp/reboot_test/amlogic_tbl", value);
        g_mock_fs_enabled = true;

	int rc = get_hardware_reason_amlogic(&ctx, &hw);
        EXPECT_EQ(rc, SUCCESS);
        EXPECT_STREQ(hw.mappedReason, testCase.expected);
    }
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_mtk_AllMapCasesTable) {
    struct Case {
        const char* value;
        const char* expected;
    };
    const Case cases[] = {
        {"0x00\n", "POWER_ON_RESET"},
        {"0xD1\n", "SOFTWARE_MASTER_RESET"},
        {"0xE4\n", "THERMAL_REBOOT"},
        {"0xEE\n", "KERNEL_PANIC"},
        {"0xE0\n", "WATCHDOG_REBOOT"},
        {"0x99\n", "UNKNOWN_RESET"}
    };

    EnvContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    set_test_soc(&ctx, TestSoc::Mtk);

    for (const auto& testCase : cases) {
        HardwareReason hw;
        memset(&hw, 0, sizeof(hw));
        setup_mock_file("/sys/mtk_pm/boot_reason", "/tmp/reboot_test/mtk_tbl", testCase.value);
        g_mock_fs_enabled = true;

        int rc = get_hardware_reason_mtk(&ctx, &hw);
        EXPECT_EQ(rc, SUCCESS);
        EXPECT_STREQ(hw.mappedReason, testCase.expected);
    }
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_brcm_EmptyFileUnknown) {
    setup_mock_file("/proc/brcm/previous_reboot_reason", "/tmp/reboot_test/brcm_empty", "");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Brcm);

    EXPECT_EQ(get_hardware_reason_brcm(&ctx, &hw), SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_rtk_NoWakeupReasonUnknown) {
    setup_mock_file("/proc/cmdline", "/tmp/reboot_test/cmdline_no_wakeup", "console=ttyS0 root=/dev/mmcblk0\n");
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Rtk);

    EXPECT_EQ(get_hardware_reason_rtk(&ctx, &hw), SUCCESS);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_brcm_OpenFailsReturnsErrorGeneral) {
    g_mock_path_map["/proc/brcm/previous_reboot_reason"] = "/tmp/reboot_test/does_not_exist_brcm";
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Brcm);

    int rc = get_hardware_reason_brcm(&ctx, &hw);
    EXPECT_EQ(rc, ERROR_GENERAL);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_amlogic_OpenFailsReturnsErrorGeneral) {
    g_mock_path_map["/sys/class/aml_reboot/reboot_reason"] = "/tmp/reboot_test/does_not_exist_amlogic";
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Amlogic);

    int rc = get_hardware_reason_amlogic(&ctx, &hw);
    EXPECT_EQ(rc, ERROR_GENERAL);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

TEST_F(PlatformHalMockFsTest, get_hardware_reason_mtk_OpenFailsReturnsErrorGeneral) {
    g_mock_path_map["/sys/mtk_pm/boot_reason"] = "/tmp/reboot_test/does_not_exist_mtk";
    g_mock_fs_enabled = true;

    EnvContext ctx;
    HardwareReason hw;
    memset(&ctx, 0, sizeof(ctx));
    memset(&hw, 0, sizeof(hw));
    set_test_soc(&ctx, TestSoc::Mtk);

    int rc = get_hardware_reason_mtk(&ctx, &hw);
    EXPECT_EQ(rc, ERROR_GENERAL);
    EXPECT_STREQ(hw.mappedReason, "UNKNOWN");
}

GTEST_API_ int main(int argc, char *argv[]) {
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s",
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
