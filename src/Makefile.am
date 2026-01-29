AUTOMAKE_OPTIONS = foreign subdir-objects

reboot_CFLAGS += -fPIC -pthread

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -I./include -I$(srcdir)/rebootmanager-cpc/include -DRDK_LOGGER_ENABLED
AM_LDFLAGS = -lfwutils -lrdkloggers

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

bin_PROGRAMS += update-prev-reboot-info
update_prev_reboot_info_SOURCES = \
    $(srcdir)/rebootmanager-cpc/src/main.c \
    $(srcdir)/rebootmanager-cpc/src/platform_hal.c \
    $(srcdir)/rebootmanager-cpc/src/reboot_reason_classify.c \
    $(srcdir)/rebootmanager-cpc/src/log_parser.c \
    $(srcdir)/rebootmanager-cpc/src/json.c \
    $(srcdir)/rebootmanager-cpc/src/parodus_log_update.c

if IS_TELEMETRY2_ENABLED
   AM_CFLAGS +=  $(T2_EVENT_FLAG)
   AM_LDFLAGS += -ltelemetry_msgsender -lt2utils
endif
