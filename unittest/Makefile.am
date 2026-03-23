AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = reboot_utils_gtest reboot_rbus_gtest reboot_cyclic_gtest reboot_system_gtest reboot_main_gtest \
               rebootreason_main_gtest reboot_log_parser_gtest reboot_classify_gtest \
               reboot_json_gtest reboot_platform_hal_gtest reboot_parodus_gtest

AM_CPPFLAGS = -I$(srcdir)/mocks -I$(top_srcdir)/../reboot-helper/include -I$(top_srcdir)/../reboot-reason-fetcher/include -I$(top_srcdir)/../reboot-helper/src -I$(top_srcdir)/../reboot-reason-fetcher/src
AM_CXXFLAGS = -std=c++14

# Only preprocessor defs and forced include here; no C++ standard flags
COMMON_CPPFLAGS = -DGTEST_ENABLE \
        -I$(srcdir)/mocks \
        -I$(top_srcdir)/../reboot-helper/include \
        -I$(top_srcdir)/../reboot-helper/src \
        -I$(top_srcdir)/../reboot-reason-fetcher/include \
        -I$(top_srcdir)/../reboot-reason-fetcher/src \
        -include $(srcdir)/mocks/rbus_mock_types.h \
        -include $(srcdir)/mocks/telemetry_busmessage_sender.h

COMMON_LDADD = -lgtest -lgmock -lpthread -lgcov
# Split flags so C doesn't receive C++-only options like -fpermissive
COMMON_CFLAGS = -fprofile-arcs -ftest-coverage -fpermissive -Wno-implicit-function-declaration
COMMON_CXXFLAGS = -fprofile-arcs -ftest-coverage -fpermissive -Wno-implicit-function-declaration

reboot_utils_gtest_SOURCES = utils_gtest.cpp $(top_srcdir)/../reboot-helper/src/utils.c mocks/rdk_logger_stub.c
reboot_utils_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_utils_gtest_LDADD = $(COMMON_LDADD)
reboot_utils_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_utils_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_rbus_gtest_SOURCES = rbus_interface_gtest.cpp $(top_srcdir)/../rebootnow/src/rebootmgr_rbus_interface.c mocks/rdk_logger_stub.c
reboot_rbus_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_rbus_gtest_LDADD = $(COMMON_LDADD)
reboot_rbus_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_rbus_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_cyclic_gtest_SOURCES = cyclic_reboot_gtest.cpp $(top_srcdir)/../reboot-helper/src/cyclic_reboot.c mocks/rdk_logger_stub.c mocks/secure_wrapper_stub.c
reboot_cyclic_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_cyclic_gtest_LDADD = $(COMMON_LDADD)
reboot_cyclic_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_cyclic_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_system_gtest_SOURCES = system_cleanup_gtest.cpp $(top_srcdir)/../reboot-helper/src/system_cleanup.c mocks/rdk_logger_stub.c
reboot_system_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_system_gtest_LDADD = $(COMMON_LDADD)
reboot_system_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_system_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_main_gtest_SOURCES = reboot_main_gtest.cpp \
        $(top_srcdir)/../reboot-helper/src/utils.c \
        $(top_srcdir)/../reboot-helper/src/cyclic_reboot.c \
        mocks/rdk_logger_stub.c

rebootreason_main_gtest_SOURCES = rebootreason_main_gtest.cpp mocks/mock_reboot_utils.cpp \
                                  mocks/rdk_logger_stub.c
reboot_log_parser_gtest_SOURCES = reboot_log_parser_gtest.cpp $(top_srcdir)/../reboot-reason-fetcher/src/log_parser.c \
                                  mocks/rdk_logger_stub.c
reboot_classify_gtest_SOURCES = reboot_classify_gtest.cpp $(top_srcdir)/../reboot-reason-fetcher/src/reboot_reason_classify.c \
                                mocks/rdk_logger_stub.c
reboot_json_gtest_SOURCES = reboot_json_gtest.cpp $(top_srcdir)/../reboot-reason-fetcher/src/json.c \
                            mocks/rdk_logger_stub.c
reboot_platform_hal_gtest_SOURCES = reboot_platform_hal_gtest.cpp $(top_srcdir)/../reboot-reason-fetcher/src/platform_hal.c \
                                    mocks/rdk_logger_stub.c mocks/mock_reboot_utils.cpp
reboot_parodus_gtest_SOURCES = reboot_parodus_gtest.cpp $(top_srcdir)/../reboot-reason-fetcher/src/parodus_log_update.c \
                               mocks/rdk_logger_stub.c

reboot_main_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_main_gtest_CPPFLAGS += -I$(srcdir)/mocks -I$(top_srcdir)/unittest/mocks
reboot_main_gtest_CPPFLAGS += -DT2_EVENT_ENABLED
reboot_main_gtest_LDADD = $(COMMON_LDADD)
reboot_main_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_main_gtest_CFLAGS = $(COMMON_CFLAGS)
rebootreason_main_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
rebootreason_main_gtest_LDADD = $(COMMON_LDADD)
rebootreason_main_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
rebootreason_main_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_log_parser_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_log_parser_gtest_LDADD = $(COMMON_LDADD) -ldl
reboot_log_parser_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_log_parser_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_classify_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_classify_gtest_LDADD = $(COMMON_LDADD)
reboot_classify_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_classify_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_json_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_json_gtest_LDADD = $(COMMON_LDADD)
reboot_json_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_json_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_platform_hal_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_platform_hal_gtest_LDADD = $(COMMON_LDADD) -ldl
reboot_platform_hal_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_platform_hal_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_parodus_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_parodus_gtest_LDADD = $(COMMON_LDADD)
reboot_parodus_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_parodus_gtest_CFLAGS = $(COMMON_CFLAGS)
