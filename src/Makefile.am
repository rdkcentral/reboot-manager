AUTOMAKE_OPTIONS = foreign subdir-objects

reboot_CFLAGS += -fPIC -pthread
bin_PROGRAMS = rebootnow

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -I./include -DRDK_LOGGER_ENABLED

# Library target
lib_LTLIBRARIES = librebootnow.la
librebootnow_la_SOURCES = \
      system_cleanup.c \
      cyclic_reboot.c \
      utils.c \
      rbus_interface.c

librebootnow_la_LIBADD = -lfwutils -lrdkloggers -lrbus -lsecure_wrapper

# CLI wrapper binary
bin_PROGRAMS = rebootnow
rebootnow_SOURCES = rebootNow_main.c
rebootnow_LDADD = librebootnow.la

if IS_TELEMETRY2_ENABLED
   AM_CFLAGS +=  $(T2_EVENT_FLAG)
   AM_LDFLAGS += -ltelemetry_msgsender -lt2utils
endif
