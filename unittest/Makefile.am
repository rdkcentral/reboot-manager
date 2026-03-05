AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = reboot_utils_gtest reboot_rbus_gtest reboot_cyclic_gtest reboot_system_gtest reboot_main_gtest

AM_CPPFLAGS = -I$(srcdir)/mocks -I$(top_srcdir)/../rebootnow/include -I$(top_srcdir)/../rebootnow/src
AM_CXXFLAGS = -std=c++14

# Only preprocessor defs and forced include here; no C++ standard flags
COMMON_CPPFLAGS = -DGTEST_ENABLE \
        -I$(srcdir)/mocks \
	-I$(top_srcdir)/../rebootnow/include \
	-I$(top_srcdir)/../rebootnow/src \
        -include $(srcdir)/mocks/rbus_mock_types.h \
        -include $(srcdir)/mocks/telemetry_busmessage_sender.h

COMMON_LDADD = -lgtest -lgmock -lpthread -lgcov
# Split flags so C doesn't receive C++-only options like -fpermissive
COMMON_CFLAGS = -fprofile-arcs -ftest-coverage
COMMON_CXXFLAGS = -fprofile-arcs -ftest-coverage -fpermissive

reboot_utils_gtest_SOURCES = utils_gtest.cpp $(top_srcdir)/../rebootnow/src/utils.c mocks/rdk_logger_stub.c
reboot_utils_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_utils_gtest_LDADD = $(COMMON_LDADD)
reboot_utils_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_utils_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_rbus_gtest_SOURCES = rbus_interface_gtest.cpp $(top_srcdir)/../rebootnow/src/rbus_interface.c mocks/rdk_logger_stub.c
reboot_rbus_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_rbus_gtest_LDADD = $(COMMON_LDADD)
reboot_rbus_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_rbus_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_cyclic_gtest_SOURCES = cyclic_reboot_gtest.cpp $(top_srcdir)/../rebootnow/src/cyclic_reboot.c mocks/rdk_logger_stub.c mocks/secure_wrapper_stub.c
reboot_cyclic_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_cyclic_gtest_LDADD = $(COMMON_LDADD)
reboot_cyclic_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_cyclic_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_system_gtest_SOURCES = system_cleanup_gtest.cpp $(top_srcdir)/../rebootnow/src/system_cleanup.c mocks/rdk_logger_stub.c
reboot_system_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_system_gtest_LDADD = $(COMMON_LDADD)
reboot_system_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_system_gtest_CFLAGS = $(COMMON_CFLAGS)

reboot_main_gtest_SOURCES = reboot_main_gtest.cpp \
        $(top_srcdir)/../rebootnow/src/utils.c \
        $(top_srcdir)/../rebootnow/src/cyclic_reboot.c \
        mocks/rdk_logger_stub.c
reboot_main_gtest_CPPFLAGS = $(COMMON_CPPFLAGS)
reboot_main_gtest_CPPFLAGS += -I$(srcdir)/mocks -I$(top_srcdir)/unittest/mocks
reboot_main_gtest_CPPFLAGS += -DT2_EVENT_ENABLED
reboot_main_gtest_LDADD = $(COMMON_LDADD)
reboot_main_gtest_CXXFLAGS = $(COMMON_CXXFLAGS)
reboot_main_gtest_CFLAGS = $(COMMON_CFLAGS)
