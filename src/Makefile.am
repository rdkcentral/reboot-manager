AUTOMAKE_OPTIONS = foreign subdir-objects

reboot_CFLAGS += -fPIC -pthread
bin_PROGRAMS = rebootnow

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -I./include -DRDK_LOGGER_ENABLED
AM_LDFLAGS = -lfwutils -lrdkloggers -lrbus -lsecure_wrapper

rebootnow_SOURCES = rebootNow_main.c \
      system_cleanup.c \
      cyclic_reboot.c \
      utils.c \
      rbus_interface.c
      $(NULL)

if IS_TELEMETRY2_ENABLED
   AM_CFLAGS +=  $(T2_EVENT_FLAG)
   AM_LDFLAGS += -ltelemetry_msgsender -lt2utils
endif
