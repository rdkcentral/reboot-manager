##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

AUTOMAKE_OPTIONS = foreign subdir-objects

reboot_CFLAGS += -fPIC -pthread

AM_CPPFLAGS = -I$(top_srcdir)/reboot-helper/include
AM_CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -I./reboot-helper/include -DRDK_LOGGER_ENABLED -fPIC -pthread

# Library target
lib_LTLIBRARIES = librebootnow.la
librebootnow_la_LDFLAGS = -shared
librebootnow_la_SOURCES = reboot_main.c \
      system_cleanup.c \
      cyclic_reboot.c \
      utils.c \
      rbus_interface.c

librebootnow_la_LIBADD = -lfwutils -lrdkloggers -lrbus -lsecure_wrapper

# CLI wrapper binary
bin_PROGRAMS = rebootnow
rebootnow_SOURCES = main.c
rebootnow_LDADD = librebootnow.la

AM_LDFLAGS = -lrdkloggers -lfwutils -lrbus -lsecure_wrapper

if IS_TELEMETRY2_ENABLED
   AM_CFLAGS +=  $(T2_EVENT_FLAG)
   AM_LDFLAGS += -ltelemetry_msgsender -lt2utils
endif
