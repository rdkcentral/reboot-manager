#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <map>
#include <cstring>
#include <cstdio>
extern "C" {
#include "rbus_interface.h"
#include "mocks/rbus_mock_types.h"
}

#define GTEST_DEFAULT_RESULT_FILEPATH "/tmp/Gtest_Report/"
#define GTEST_DEFAULT_RESULT_FILENAME "reboot_rbus_gtest_report.json"
#define GTEST_REPORT_FILEPATH_SIZE 256

using namespace testing;
using namespace std;

// Simple mock rbus storage
static map<string, string> g_str;
static map<string, bool> g_bool;
static map<string, int> g_int;
static bool g_open_ok = true;
static bool g_set_fail_bool = false;
static bool g_set_fail_int = false;

extern "C" {
struct MockVal { 
    string s;
    bool b=false;
    int i=0;
    enum { 
        STR, 
        BOOL, 
        INT 
    } t=STR;
};

rbusError_t rbus_open(rbusHandle_t* handle, const char* name){ 
    (void)name;
    if(!g_open_ok) 
        return 1; 
    *handle=(void*)0x1; 
    return RBUS_ERROR_SUCCESS;
}
void rbus_close(rbusHandle_t handle){
    (void)handle; 
}
rbusError_t rbus_get(rbusHandle_t handle, const char* name, rbusValue_t* value){ 
    (void)handle; 
    if(!name)
        return 2; 
    auto ns=string(name);
    MockVal* mv = new MockVal();
    if(g_str.count(ns)){ 
        mv->t=MockVal::STR;
        mv->s=g_str[ns]; 
    }
    else if(g_bool.count(ns)){ 
        mv->t=MockVal::BOOL; 
        mv->b=g_bool[ns]; 
    }
    else if(g_int.count(ns)){
        mv->t=MockVal::INT; 
        mv->i=g_int[ns]; 
    }
    else {
        delete mv; 
        return 3; 
    }
    *value = (rbusValue_t)mv; 
    return RBUS_ERROR_SUCCESS;
}
rbusError_t rbus_set(rbusHandle_t handle, const char* name, rbusValue_t value, void* opts){ 
    (void)handle; 
    (void)opts; 
    if(!name||!value) 
        return 4; 
    MockVal* mv=(MockVal*)value; 
    auto ns=string(name);
    if(mv->t==MockVal::BOOL){ 
        if(g_set_fail_bool) 
            return 6; 
        g_bool[ns]=mv->b; 
        return RBUS_ERROR_SUCCESS; 
    }
    if(mv->t==MockVal::INT){ 
        if(g_set_fail_int) 
            return 7; 
        g_int[ns]=mv->i; 
        return RBUS_ERROR_SUCCESS; 
    }
    return 5; 
}

rbusValue_t rbusValue_Init(void* data){ 
    MockVal* mv = new MockVal(); 
    (void)data; 
    return (rbusValue_t)mv; 
}
void rbusValue_Release(rbusValue_t value){ 
    MockVal* mv=(MockVal*)value; 
    delete mv;
}
const char* rbusValue_GetString(rbusValue_t value, int* len){
    MockVal* mv=(MockVal*)value; 
    if(mv->t!=MockVal::STR) 
        return NULL; 
    if(len) 
        *len=(int)mv->s.size(); 
    return mv->s.c_str(); 
}
bool rbusValue_GetBoolean(rbusValue_t value){
    MockVal* mv=(MockVal*)value;
    return mv->b; 
}
int rbusValue_GetInt32(rbusValue_t value){ 
    MockVal* mv=(MockVal*)value; 
    return mv->i; 
}
void rbusValue_SetBoolean(rbusValue_t value, bool v){ 
    MockVal* mv=(MockVal*)value; 
    mv->t=MockVal::BOOL; mv->b=v; 
}
void rbusValue_SetInt32(rbusValue_t value, int v){ 
    MockVal* mv=(MockVal*)value; 
    mv->t=MockVal::INT; mv->i=v; 
}
}

TEST(RbusInterface, InitSuccess){ 
    g_open_ok=true; 
    ASSERT_TRUE(rbus_init());
    ASSERT_TRUE(rbus_init()); 
    rbus_cleanup();
}

TEST(RbusInterface, InitFail){ 
    g_open_ok=false; 
    ASSERT_FALSE(rbus_init()); 
    g_open_ok=true;
}

TEST(RbusInterface, GetSetParams){ 
    ASSERT_TRUE(rbus_init()); 
    g_str["Device.X.Foo.String"]=string("abc"); 
    g_bool["Device.X.Foo.Bool"]=true; 
    g_int["Device.X.Foo.Int"]=42;
    char buf[32]={0}; 
    bool bv=false; 
    int iv=0;
    ASSERT_TRUE(rbus_get_string_param("Device.X.Foo.String", buf, sizeof(buf))); 
    ASSERT_STREQ(buf, "abc");
    ASSERT_TRUE(rbus_get_bool_param("Device.X.Foo.Bool", &bv)); 
    ASSERT_TRUE(bv);
    ASSERT_TRUE(rbus_get_int_param("Device.X.Foo.Int", &iv)); 
    ASSERT_EQ(iv, 42);
    ASSERT_TRUE(rbus_set_bool_param("Device.X.Foo.SetB", false)); 
    ASSERT_FALSE(g_bool["Device.X.Foo.SetB"]);
    ASSERT_TRUE(rbus_set_int_param("Device.X.Foo.SetI", 7)); 
    ASSERT_EQ(g_int["Device.X.Foo.SetI"], 7);
    rbus_cleanup();
}

TEST(RbusInterface, GetMissing){ 
    ASSERT_TRUE(rbus_init()); 
    char buf[8]; 
    bool bv; 
    int iv;
    ASSERT_FALSE(rbus_get_string_param("Device.X.Missing", buf, sizeof(buf)));
    ASSERT_FALSE(rbus_get_bool_param("Device.X.MissingB", &bv));
    ASSERT_FALSE(rbus_get_int_param("Device.X.MissingI", &iv));
    rbus_cleanup();
}

TEST(RbusInterface, NotInitializedGuards){
    char buf[8]={0}; bool bv=false; int iv=0;
    // No rbus_init() here to exercise not-initialized guards
    ASSERT_FALSE(rbus_get_string_param("Device.X.Str", buf, sizeof(buf)));
    ASSERT_FALSE(rbus_get_bool_param("Device.X.Bool", &bv));
    ASSERT_FALSE(rbus_get_int_param("Device.X.Int", &iv));
    ASSERT_FALSE(rbus_set_bool_param("Device.X.Bool", true));
    ASSERT_FALSE(rbus_set_int_param("Device.X.Int", 1));
}

TEST(RbusInterface, InvalidArgs){
    ASSERT_TRUE(rbus_init());
    char buf[8]={0}; bool bv=false; int iv=0;
    ASSERT_FALSE(rbus_get_string_param(NULL, buf, sizeof(buf)));
    ASSERT_FALSE(rbus_get_string_param("Device.X.Str", NULL, sizeof(buf)));
    ASSERT_FALSE(rbus_get_string_param("Device.X.Str", buf, 0));
    ASSERT_FALSE(rbus_get_bool_param(NULL, &bv));
    ASSERT_FALSE(rbus_get_bool_param("Device.X.Bool", NULL));
    ASSERT_FALSE(rbus_get_int_param(NULL, &iv));
    ASSERT_FALSE(rbus_get_int_param("Device.X.Int", NULL));
    ASSERT_FALSE(rbus_set_bool_param(NULL, true));
    ASSERT_FALSE(rbus_set_int_param(NULL, 5));
    rbus_cleanup();
}

TEST(RbusInterface, StringEmptyAndWrongType){
    ASSERT_TRUE(rbus_init());
    // Empty string should return false (no content)
    g_str["Device.X.Empty"]=string("");
    char buf[8]={0};
    ASSERT_FALSE(rbus_get_string_param("Device.X.Empty", buf, sizeof(buf)));
    // Request string for a bool-typed param -> GetString returns NULL path
    g_bool["Device.X.BoolAsString"]=true;
    ASSERT_FALSE(rbus_get_string_param("Device.X.BoolAsString", buf, sizeof(buf)));
    rbus_cleanup();
}

TEST(RbusInterface, SetFailures){
    ASSERT_TRUE(rbus_init());
    g_set_fail_bool = true;
    ASSERT_FALSE(rbus_set_bool_param("Device.X.FailB", true));
    g_set_fail_bool = false;
    g_set_fail_int = true;
    ASSERT_FALSE(rbus_set_int_param("Device.X.FailI", 99));
    g_set_fail_int = false;
    rbus_cleanup();
}

TEST(RbusInterface, ValueInitReleaseAndSet){
    ASSERT_TRUE(rbus_init());
    // Initialize a value, set boolean then int, then release
    rbusValue_t v = rbusValue_Init(NULL);
    rbusValue_SetBoolean(v, true);
    ASSERT_TRUE(rbusValue_GetBoolean(v));
    rbusValue_SetInt32(v, 123);
    ASSERT_EQ(rbusValue_GetInt32(v), 123);
    rbusValue_Release(v);
    // Close handle (no-op in mock)
    rbus_cleanup();
}

extern "C" {
int rdk_logger_msg_printf(int level, const char* module, const char* fmt, ...);
int rdk_logger_ext_init(const void* config);
int rdk_logger_init(const char* path);
}

TEST(RbusInterface, InvokeLoggerStubsRbus){
    ASSERT_EQ(0, rdk_logger_init("/etc/debug.ini"));
    ASSERT_EQ(0, rdk_logger_ext_init(NULL));
    ASSERT_EQ(0, rdk_logger_msg_printf(0, "LOG.RDK.REBOOTINFO", "rbus stub %d\n", 2));
}

GTEST_API_ int main(int argc, char *argv[]){
    char testresults_fullfilepath[GTEST_REPORT_FILEPATH_SIZE];
    memset( testresults_fullfilepath, 0, GTEST_REPORT_FILEPATH_SIZE );
    snprintf( testresults_fullfilepath, GTEST_REPORT_FILEPATH_SIZE, "json:%s%s" , GTEST_DEFAULT_RESULT_FILEPATH , GTEST_DEFAULT_RESULT_FILENAME);
    ::testing::GTEST_FLAG(output) = testresults_fullfilepath;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
