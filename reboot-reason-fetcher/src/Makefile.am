`1AUTOMAKE_OPTIONS = foreign subdir-objects

reboot_CFLAGS += -fPIC -pthread
bin_PROGRAMS = update-prev-reboot-info

AM_CPPFLAGS = -I$(top_srcdir)/reboot-reason-fetcher/include
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -DRDK_LOGGER_ENABLED
AM_LDFLAGS = -lfwutils -lrdkloggers

update_prev_reboot_info_SOURCES = rebootreason_main.c \
      platform_hal.c \
      reboot_reason_classify.c \
      log_parser.c \
      json.c \
      parodus_log_update.c \
      $(NULL)

if IS_TELEMETRY2_ENABLED
   AM_CFLAGS +=  $(T2_EVENT_FLAG)
   AM_LDFLAGS += -ltelemetry_msgsender -lt2utils
endif
