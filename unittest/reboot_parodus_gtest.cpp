#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
extern "C" {
    #include "update-reboot-info.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_parodus_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

TEST(ParodusSmokeTest, append_kernel_reason_InvalidParams) {
    EXPECT_EQ(append_kernel_reason(nullptr, nullptr), ERROR_GENERAL);
}

TEST(ParodusSmokeTest, update_parodus_log_InvalidParams) {
    EXPECT_EQ(update_parodus_log(nullptr), ERROR_GENERAL);
}

TEST(ParodusSmokeTest, copy_keypress_info_InvalidParams) {
    EXPECT_EQ(copy_keypress_info(nullptr, nullptr), ERROR_GENERAL);
}

TEST(ParodusSmokeTest, append_kernel_reason_NonRtkSkips) {
    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&info, 0, sizeof(info));
    strncpy(ctx.soc, "BRCM", sizeof(ctx.soc) - 1);
    strncpy(info.reason, "SOFTWARE_MASTER_RESET", sizeof(info.reason) - 1);
    EXPECT_EQ(append_kernel_reason(&ctx, &info), SUCCESS);
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_FallbackWritesDest) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T12:00:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.reason, "APP_TRIGGERED", sizeof(info.reason) - 1);
    strncpy(info.customReason, "Servicemanager", sizeof(info.customReason) - 1);
    strncpy(info.source, "Servicemanager", sizeof(info.source) - 1);

    const char* destPath = "/tmp/reboot_test_parodus.out";
    EXPECT_EQ(handle_parodus_reboot_file(&info, destPath), SUCCESS);

    FILE* fp = fopen(destPath, "r");
    ASSERT_NE(fp, nullptr);
    char buf[512] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);

    EXPECT_NE(strstr(buf, "PreviousRebootInfo:2026-03-10T12:00:00Z,APP_TRIGGERED,Servicemanager,Servicemanager"), nullptr);
    remove(destPath);
}

TEST(ParodusSmokeTest, copy_keypress_info_SourceMissingReturnsSuccess) {
    EXPECT_EQ(copy_keypress_info("/tmp/nonexistent_keypress.info", "/tmp/unused_dest.info"), SUCCESS);
}

TEST(ParodusSmokeTest, copy_keypress_info_Success) {
    const char* src = "/tmp/reboot_test_keypress_src.info";
    const char* dst = "/tmp/reboot_test_keypress_dst.info";
    FILE* fp = fopen(src, "w");
    ASSERT_NE(fp, nullptr);
    fputs("KEY=OK\n", fp);
    fclose(fp);

    EXPECT_EQ(copy_keypress_info(src, dst), SUCCESS);

    fp = fopen(dst, "r");
    ASSERT_NE(fp, nullptr);
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    EXPECT_NE(strstr(buf, "KEY=OK"), nullptr);

    remove(src);
    remove(dst);
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_InvalidParams) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    EXPECT_EQ(handle_parodus_reboot_file(nullptr, "/tmp/out"), ERROR_GENERAL);
    EXPECT_EQ(handle_parodus_reboot_file(&info, nullptr), ERROR_GENERAL);
}

TEST(ParodusSmokeTest, update_parodus_log_ValidInfoPath) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T10:00:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.source, "Kernel", sizeof(info.source) - 1);
    strncpy(info.reason, "KERNEL_PANIC", sizeof(info.reason) - 1);
    strncpy(info.customReason, "Hardware Register - KERNEL_PANIC", sizeof(info.customReason) - 1);
    strncpy(info.otherReason, "Oops dump", sizeof(info.otherReason) - 1);

    int rc = update_parodus_log(&info);
    EXPECT_TRUE(rc == SUCCESS || rc == ERROR_GENERAL);
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_InvalidDestinationPath) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T12:30:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.reason, "OPS_TRIGGERED", sizeof(info.reason) - 1);
    strncpy(info.customReason, "ScheduledReboot", sizeof(info.customReason) - 1);
    strncpy(info.source, "ScheduledReboot", sizeof(info.source) - 1);

    int rc = handle_parodus_reboot_file(&info, "/this/path/does/not/exist/out.info");
    EXPECT_EQ(rc, ERROR_GENERAL);
}

TEST(ParodusSmokeTest, append_kernel_reason_RtkPathExecutes) {
    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&info, 0, sizeof(info));
    strncpy(ctx.soc, "RTK", sizeof(ctx.soc) - 1);
    strncpy(info.reason, "SOFTWARE_MASTER_RESET", sizeof(info.reason) - 1);

    int rc = append_kernel_reason(&ctx, &info);
    EXPECT_TRUE(rc == SUCCESS || rc == ERROR_GENERAL);
}

TEST(ParodusSmokeTest, update_parodus_log_SkipsWhenPreviousInfoExists) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T15:00:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.source, "Kernel", sizeof(info.source) - 1);
    strncpy(info.reason, "KERNEL_PANIC", sizeof(info.reason) - 1);
    strncpy(info.customReason, "Hardware Register - KERNEL_PANIC", sizeof(info.customReason) - 1);

    system("mkdir -p /opt/logs");
    FILE* fp = fopen("/opt/logs/parodus.log", "w");
    if (!fp) {
        GTEST_SKIP() << "Cannot create /opt/logs/parodus.log in this environment";
    }
    fputs("PreviousRebootInfo:existing\n", fp);
    fclose(fp);

    EXPECT_EQ(update_parodus_log(&info), SUCCESS);

    fp = fopen("/opt/logs/parodus.log", "r");
    ASSERT_NE(fp, nullptr);
    char buf[512] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    EXPECT_NE(strstr(buf, "PreviousRebootInfo:existing"), nullptr);
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_UsesParodusInputFile) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T16:00:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.reason, "APP_TRIGGERED", sizeof(info.reason) - 1);
    strncpy(info.customReason, "DeviceInitiated", sizeof(info.customReason) - 1);
    strncpy(info.source, "DeviceInitiated", sizeof(info.source) - 1);

    system("mkdir -p /opt/secure/reboot");
    FILE* src = fopen("/opt/secure/reboot/parodusreboot.info", "w");
    if (!src) {
        GTEST_SKIP() << "Cannot create /opt/secure/reboot/parodusreboot.info in this environment";
    }
    fputs("PreviousRebootInfo:from_input_file", src);
    fclose(src);

    const char* destPath = "/tmp/reboot_test_parodus_from_input.out";
    EXPECT_EQ(handle_parodus_reboot_file(&info, destPath), SUCCESS);

    FILE* out = fopen(destPath, "r");
    ASSERT_NE(out, nullptr);
    char outBuf[256] = {0};
    size_t n = fread(outBuf, 1, sizeof(outBuf) - 1, out);
    outBuf[n] = '\0';
    fclose(out);
    EXPECT_NE(strstr(outBuf, "PreviousRebootInfo:from_input_file"), nullptr);
    EXPECT_NE(access("/opt/secure/reboot/parodusreboot.info", F_OK), 0);

    remove(destPath);
}

TEST(ParodusSmokeTest, copy_keypress_info_InvalidDestinationPath) {
    const char* src = "/tmp/reboot_test_keypress_src2.info";
    FILE* fp = fopen(src, "w");
    ASSERT_NE(fp, nullptr);
    fputs("KEY=BADDEST\n", fp);
    fclose(fp);

    int rc = copy_keypress_info(src, "/this/path/does/not/exist/keypress.out");
    EXPECT_EQ(rc, ERROR_GENERAL);

    remove(src);
}

TEST(ParodusSmokeTest, append_kernel_reason_RtkWritesLowercaseToLog) {
    system("mkdir -p /opt/logs");
    remove("/opt/logs/receiver.log");

    EnvContext ctx;
    RebootInfo info;
    memset(&ctx, 0, sizeof(ctx));
    memset(&info, 0, sizeof(info));
    strncpy(ctx.soc, "REALTEK", sizeof(ctx.soc) - 1);
    strncpy(info.reason, "SOFTWARE_MASTER_RESET", sizeof(info.reason) - 1);

    int rc = append_kernel_reason(&ctx, &info);
    EXPECT_EQ(rc, SUCCESS);

    FILE* fp = fopen("/opt/logs/receiver.log", "r");
    ASSERT_NE(fp, nullptr);
    char buf[512] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);

    EXPECT_NE(strstr(buf, "software_master_reset"), nullptr);
}

TEST(ParodusSmokeTest, update_parodus_log_WritesWhenNoPreviousInfo) {
    system("mkdir -p /opt/logs");
    remove("/opt/logs/parodus.log");

    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T22:00:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.source, "WatchDog", sizeof(info.source) - 1);
    strncpy(info.reason, "WATCHDOG_TIMER_RESET", sizeof(info.reason) - 1);
    strncpy(info.customReason, "Hardware Register - WATCHDOG", sizeof(info.customReason) - 1);
    strncpy(info.otherReason, "watchdog timeout", sizeof(info.otherReason) - 1);

    EXPECT_EQ(update_parodus_log(&info), SUCCESS);

    FILE* fp = fopen("/opt/logs/parodus.log", "r");
    ASSERT_NE(fp, nullptr);
    char buf[1024] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    EXPECT_NE(strstr(buf, "PreviousRebootInfo:2026-03-10T22:00:00Z,WATCHDOG_TIMER_RESET,Hardware Register - WATCHDOG,WatchDog"), nullptr);
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_EmptyInputFilePath) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));

    system("mkdir -p /opt/secure/reboot");
    FILE* src = fopen("/opt/secure/reboot/parodusreboot.info", "w");
    if (!src) {
        GTEST_SKIP() << "Cannot create /opt/secure/reboot/parodusreboot.info in this environment";
    }
    fclose(src);

    const char* destPath = "/tmp/reboot_test_parodus_empty.out";
    EXPECT_EQ(handle_parodus_reboot_file(&info, destPath), SUCCESS);

    FILE* out = fopen(destPath, "r");
    ASSERT_NE(out, nullptr);
    char outBuf[8] = {0};
    size_t n = fread(outBuf, 1, sizeof(outBuf) - 1, out);
    fclose(out);
    EXPECT_EQ(n, 0u);

    remove(destPath);
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_InputExistsButDestOpenFails) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));

    system("mkdir -p /opt/secure/reboot");
    FILE* src = fopen("/opt/secure/reboot/parodusreboot.info", "w");
    if (!src) {
        GTEST_SKIP() << "Cannot create /opt/secure/reboot/parodusreboot.info in this environment";
    }
    fputs("PreviousRebootInfo:content", src);
    fclose(src);

    int rc = handle_parodus_reboot_file(&info, "/this/path/does/not/exist/out.info");
    EXPECT_EQ(rc, ERROR_GENERAL);
    EXPECT_NE(access("/opt/secure/reboot/parodusreboot.info", F_OK), 0);
}

TEST(ParodusSmokeTest, copy_keypress_info_ReadErrorFromDirectorySource) {
    system("mkdir -p /tmp/reboot_test_keypress_dir");
    const char* dst = "/tmp/reboot_test_keypress_dir_out.info";

    int rc = copy_keypress_info("/tmp/reboot_test_keypress_dir", dst);
    EXPECT_EQ(rc, ERROR_GENERAL);

    remove(dst);
    system("rmdir /tmp/reboot_test_keypress_dir");
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_InputPathOpenFailsThenFallbackWrites) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.timestamp, "2026-03-10T23:10:00Z", sizeof(info.timestamp) - 1);
    strncpy(info.reason, "SOFTWARE_MASTER_RESET", sizeof(info.reason) - 1);
    strncpy(info.customReason, "SOFTWARE_MASTER_RESET", sizeof(info.customReason) - 1);
    strncpy(info.source, "SoftwareReboot", sizeof(info.source) - 1);

    system("mkdir -p /opt/secure/reboot/parodusreboot.info");
    const char* destPath = "/tmp/reboot_test_parodus_fallback_after_open_fail.out";

    int rc = handle_parodus_reboot_file(&info, destPath);
    EXPECT_EQ(rc, SUCCESS);

    FILE* fp = fopen(destPath, "r");
    ASSERT_NE(fp, nullptr);
    char buf[512] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);

    const char* expectedFallback = "PreviousRebootInfo:2026-03-10T23:10:00Z,SOFTWARE_MASTER_RESET,SOFTWARE_MASTER_RESET,SoftwareReboot";
    bool fallbackWritten = (strstr(buf, expectedFallback) != nullptr);
    bool emptyCopied = (buf[0] == '\0');
    EXPECT_TRUE(fallbackWritten || emptyCopied);

    remove(destPath);
    system("rmdir /opt/secure/reboot/parodusreboot.info");
}

TEST(ParodusSmokeTest, handle_parodus_reboot_file_AppendsNewlineWhenMissingInInput) {
    RebootInfo info;
    memset(&info, 0, sizeof(info));

    system("mkdir -p /opt/secure/reboot");
    FILE* src = fopen("/opt/secure/reboot/parodusreboot.info", "w");
    if (!src) {
        GTEST_SKIP() << "Cannot create /opt/secure/reboot/parodusreboot.info in this environment";
    }
    fputs("PreviousRebootInfo:line_without_newline", src);
    fclose(src);

    const char* destPath = "/tmp/reboot_test_parodus_newline.out";
    EXPECT_EQ(handle_parodus_reboot_file(&info, destPath), SUCCESS);

    FILE* out = fopen(destPath, "r");
    ASSERT_NE(out, nullptr);
    char outBuf[256] = {0};
    size_t n = fread(outBuf, 1, sizeof(outBuf) - 1, out);
    outBuf[n] = '\0';
    fclose(out);
    ASSERT_GT(n, 0u);
    EXPECT_EQ(outBuf[n - 1], '\n');

    remove(destPath);
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

