/*
 * Unit tests for RFC parameter helpers (rfc_get_bool_param, rfc_get_int_param, etc.)
 * implemented in utils.c.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <map>
#include <string>
#include <cstring>
#include <cstdio>

extern "C" {
#include "reboot.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_rfc_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE    256

using namespace testing;
using namespace std;

static map<string, string> g_rfc_store;
static bool g_get_fail = false;
static bool g_set_fail = false;

static void rfc_store_reset(void)
{
    g_rfc_store.clear();
    g_get_fail = false;
    g_set_fail = false;
}

extern "C" {

WDMP_STATUS getRFCParameter(char * /*pcCallerID*/, const char *paramName,
                             RFC_ParamData_t *paramData)
{
    if (g_get_fail)
        return WDMP_FAILURE;
    if (!paramName || !paramData)
        return WDMP_ERR_INVALID_PARAM;

    auto it = g_rfc_store.find(string(paramName));
    if (it == g_rfc_store.end())
        return WDMP_ERR_NOT_EXIST;

    strncpy(paramData->name,  paramName,     sizeof(paramData->name)  - 1);
    paramData->name[sizeof(paramData->name) - 1] = '\0';
    strncpy(paramData->value, it->second.c_str(), sizeof(paramData->value) - 1);
    paramData->value[sizeof(paramData->value) - 1] = '\0';
    paramData->type = WDMP_STRING;
    return WDMP_SUCCESS;
}

WDMP_STATUS setRFCParameter(char * /*pcCallerID*/, const char *paramName,
                             const char *value, DATA_TYPE /*type*/)
{
    if (g_set_fail)
        return WDMP_FAILURE;
    if (!paramName || !value)
        return WDMP_ERR_INVALID_PARAM;

    g_rfc_store[string(paramName)] = string(value);
    return WDMP_SUCCESS;
}

}

class RfcInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override    { rfc_store_reset(); }
    void TearDown() override { rfc_store_reset(); }
};

TEST_F(RfcInterfaceTest, GetStringSuccess)
{
    g_rfc_store["Device.X.Foo.String"] = "hello";
    char buf[64] = {0};
    ASSERT_TRUE(rfc_get_string_param("Device.X.Foo.String", buf, sizeof(buf)));
    EXPECT_STREQ(buf, "hello");
}

TEST_F(RfcInterfaceTest, GetBoolTrue)
{
    g_rfc_store["Device.X.Foo.Bool"] = "true";
    bool v = false;
    ASSERT_TRUE(rfc_get_bool_param("Device.X.Foo.Bool", &v));
    EXPECT_TRUE(v);
}

TEST_F(RfcInterfaceTest, GetIntSuccess)
{
    g_rfc_store["Device.X.Foo.Int"] = "42";
    int v = 0;
    ASSERT_TRUE(rfc_get_int_param("Device.X.Foo.Int", &v));
    EXPECT_EQ(v, 42);
}

TEST_F(RfcInterfaceTest, SetBoolAndReadBack)
{
    ASSERT_TRUE(rfc_set_bool_param("Device.X.Foo.SetBool", true));
    bool v = false;
    ASSERT_TRUE(rfc_get_bool_param("Device.X.Foo.SetBool", &v));
    EXPECT_TRUE(v);
}

TEST_F(RfcInterfaceTest, SetIntAndReadBack)
{
    ASSERT_TRUE(rfc_set_int_param("Device.X.Foo.SetInt", 99));
    int v = 0;
    ASSERT_TRUE(rfc_get_int_param("Device.X.Foo.SetInt", &v));
    EXPECT_EQ(v, 99);
}

TEST_F(RfcInterfaceTest, BackendFailures)
{
    g_rfc_store["Device.X.Foo.String"] = "abc";
    g_get_fail = true;
    char buf[16] = {0};
    ASSERT_FALSE(rfc_get_string_param("Device.X.Foo.String", buf, sizeof(buf)));

    g_set_fail = true;
    ASSERT_FALSE(rfc_set_int_param("Device.X.Foo.SetInt", 1));
}

TEST_F(RfcInterfaceTest, CyclicRebootParams)
{
    g_rfc_store[RFC_REBOOTSTOP_DETECTION] = "true";
    g_rfc_store[RFC_REBOOTSTOP_DURATION] = "30";
    bool det = false;
    int duration = 0;
    ASSERT_TRUE(rfc_get_bool_param(RFC_REBOOTSTOP_DETECTION, &det));
    ASSERT_TRUE(rfc_get_int_param(RFC_REBOOTSTOP_DURATION, &duration));
    EXPECT_TRUE(det);
    EXPECT_EQ(duration, 30);

    ASSERT_TRUE(rfc_set_bool_param(RFC_REBOOTSTOP_ENABLE, true));
    bool enabled = false;
    ASSERT_TRUE(rfc_get_bool_param(RFC_REBOOTSTOP_ENABLE, &enabled));
    EXPECT_TRUE(enabled);
}

GTEST_API_ int main(int argc, char *argv[])
{
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset(testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE);
    snprintf(testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s",
             GTEST_DEFAULT_RESULT_FILEPATH, GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

