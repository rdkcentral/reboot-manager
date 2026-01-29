AUTOMAKE_OPTIONS = foreign subdir-objects

reboot_CFLAGS += -fPIC -pthread

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -I./include -DRDK_LOGGER_ENABLED
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

bin_PROGRAMS = update-prev-reboot-info
update_prev_reboot_info_SOURCES = src/rebootmanager-cpc/main.c \
      src/rebootmanager-cpc/platform_hal.c \
      src/rebootmanager-cpc/reboot_reason_classify.c \
      src/rebootmanager-cpc/log_parser.c \
      src/rebootmanager-cpc/json.c \
      src/rebootmanager-cpc/parodus_log_update.c \
      $(NULL)

if IS_TELEMETRY2_ENABLED
   AM_CFLAGS +=  $(T2_EVENT_FLAG)
   AM_LDFLAGS += -ltelemetry_msgsender -lt2utils
endif
