#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

extern "C" {
    #include "update-reboot-info.h"
    void wait_for_backup_logs_done(void);
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "rebootreason_main_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

/* Sentinel path must match the #define in rebootreason_main.c */
#define TEST_SENTINEL_PATH "/tmp/.backup_logs_done"

/*===========================================================================
 * Wrap control flags — toggled per-test to steer __wrap_* behaviour.
 *===========================================================================*/
static bool g_fail_inotify_init1      = false;
static bool g_fail_inotify_add_watch  = false;
static bool g_fail_clock_gettime      = false;
/* Number of times __wrap_access returns -1 for the sentinel path before
 * falling through to the real implementation. */
static int  g_access_sentinel_fail_count = 0;

/*===========================================================================
 * ld --wrap implementations
 *===========================================================================*/
extern "C" {

int __real_inotify_init1(int flags);
int __wrap_inotify_init1(int flags)
{
    if (g_fail_inotify_init1) {
        errno = EMFILE;
        return -1;
    }
    return __real_inotify_init1(flags);
}

int __real_inotify_add_watch(int fd, const char *pathname, uint32_t mask);
int __wrap_inotify_add_watch(int fd, const char *pathname, uint32_t mask)
{
    if (g_fail_inotify_add_watch) {
        errno = ENOSPC;
        return -1;
    }
    return __real_inotify_add_watch(fd, pathname, mask);
}

int __real_clock_gettime(clockid_t clk_id, struct timespec *tp);
int __wrap_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    if (g_fail_clock_gettime) {
        errno = EINVAL;
        return -1;
    }
    return __real_clock_gettime(clk_id, tp);
}

int __real_access(const char *pathname, int mode);
int __wrap_access(const char *pathname, int mode)
{
    if (g_access_sentinel_fail_count > 0 &&
        strcmp(pathname, TEST_SENTINEL_PATH) == 0) {
        g_access_sentinel_fail_count--;
        errno = ENOENT;
        return -1;
    }
    return __real_access(pathname, mode);
}

} /* extern "C" */

/*===========================================================================
 * Test fixture
 *===========================================================================*/
class WaitForBackupLogsDoneTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_fail_inotify_init1      = false;
        g_fail_inotify_add_watch  = false;
        g_fail_clock_gettime      = false;
        g_access_sentinel_fail_count = 0;
        unlink(TEST_SENTINEL_PATH);
    }

    void TearDown() override {
        unlink(TEST_SENTINEL_PATH);
    }

    static void create_sentinel() {
        int fd = open(TEST_SENTINEL_PATH, O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) close(fd);
    }
};

/*===========================================================================
 * Test cases
 *===========================================================================*/

/* Path 1: Sentinel already exists — fast path returns immediately. */
TEST_F(WaitForBackupLogsDoneTest, FastPath_SentinelAlreadyPresent)
{
    create_sentinel();
    wait_for_backup_logs_done();
    SUCCEED();
}

/* Path 2: inotify_init1 fails — function logs warning and returns. */
TEST_F(WaitForBackupLogsDoneTest, InotifyInit1Fails)
{
    g_fail_inotify_init1 = true;
    wait_for_backup_logs_done();
    SUCCEED();
}

/* Path 3: inotify_add_watch fails — function closes ifd and returns. */
TEST_F(WaitForBackupLogsDoneTest, InotifyAddWatchFails)
{
    g_fail_inotify_add_watch = true;
    wait_for_backup_logs_done();
    SUCCEED();
}

/* Path 4: Race resolution — sentinel appears between initial access()
 * (which fails) and the post-watch re-check (which succeeds). */
TEST_F(WaitForBackupLogsDoneTest, RaceResolution_SentinelAppearsAfterWatch)
{
    create_sentinel();
    g_access_sentinel_fail_count = 1;
    wait_for_backup_logs_done();
    SUCCEED();
}

/* Path 5: clock_gettime fails after inotify setup — function cleans up
 * and returns without entering the event loop. */
TEST_F(WaitForBackupLogsDoneTest, ClockGetTimeFails)
{
    g_access_sentinel_fail_count = 2;
    g_fail_clock_gettime = true;
    wait_for_backup_logs_done();
    SUCCEED();
}

/* Path 6: Sentinel detected via inotify — a helper thread creates the
 * sentinel file after a short delay, function wakes and returns. */
TEST_F(WaitForBackupLogsDoneTest, SentinelDetectedViaInotify)
{
    g_access_sentinel_fail_count = 2;

    std::thread creator([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        create_sentinel();
    });

    wait_for_backup_logs_done();
    creator.join();
    SUCCEED();
}

/* Path 7: Timeout expires — sentinel never created.
 * With GTEST_ENABLE the timeout is 2 s. */
TEST_F(WaitForBackupLogsDoneTest, TimeoutExpires_SentinelNeverCreated)
{
    g_access_sentinel_fail_count = 2;

    auto start = std::chrono::steady_clock::now();
    wait_for_backup_logs_done();
    auto elapsed = std::chrono::steady_clock::now() - start;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    EXPECT_GE(ms, 1500) << "Returned too quickly — timeout logic may be broken";
    EXPECT_LE(ms, 4000) << "Took too long — timeout may not be triggering";
}

/* Smoke test: acquire/release lock with invalid params */
TEST(MainSmokeTest, acquire_release_lock_InvalidParams) {
    EXPECT_EQ(acquire_lock(nullptr), ERROR_GENERAL);
    EXPECT_EQ(release_lock(nullptr), ERROR_GENERAL);
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

