# Copyright 2023 Comcast Cable Communications Management, LLC
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
#
# SPDX-License-Identifier: Apache-2.0
#

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(REBOOT, 1.0, saranya_elango@comcast.com)
AC_CONFIG_SRCDIR([src/rebootNow_main.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE(REBOOT, REBOOT)

IS_LIBRDKCONFIG_ENABLED=" "
# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AM_PROG_LIBTOOL

# Set shared libraries
AC_DISABLE_STATIC
AC_ENABLE_SHARED

AC_ARG_ENABLE([t2api],
              AS_HELP_STRING([--enable-t2api],[enables telemetry]),
              [
                case "${enableval}" in
                 yes) IS_TELEMETRY2_ENABLED=true
                      T2_EVENT_FLAG=" -DT2_EVENT_ENABLED ";;
                 no)  IS_TELEMETRY2_ENABLED=false ;;
               *) AC_MSG_ERROR([bad value ${enableval} for --enable-t2enable]) ;;
               esac
               ],
           [echo "telemetry is disabled"])
AM_CONDITIONAL([IS_TELEMETRY2_ENABLED], [test x$IS_TELEMETRY2_ENABLED = xtrue])
AC_SUBST(T2_EVENT_FLAG)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
                                #include <stdio.h>
                                #undef PACKAGE
                                #undef VERSION
                                #undef HAVE_STDLIB_H
                                #include "rdk_debug.h"
                ]])
        ],[
                AC_MSG_RESULT([RDK logging enabled: yes])
                reboot_LDFLAGS="$reboot_LDFLAGS -lrdkloggers"
                reboot_CFLAGS="$reboot_CFLAGS -DRDK_LOGGER_ENABLED"
        ],[
                AC_MSG_RESULT([RDK logging enabled: no])
        ])

AC_SUBST([reboot_CFLAGS])

# Checks for typedefs, structures, and compiler characteristics.

# Check for breakpad (must be before AC_OUTPUT)
BREAKPAD_CFLAGS=" "
BREAKPAD_LFLAGS=" "
AC_ARG_ENABLE([breakpad],
                AS_HELP_STRING([--enable-breakpad],[enable breakpad support (default is no)]),
                [
                        case "${enableval}" in
        	 yes) BREAKPAD_CFLAGS="-DINCLUDE_BREAKPAD"
                    BREAKPAD_LFLAGS="-lbreakpadwrapper";;
        	 no) AC_MSG_ERROR([breakpad is disabled]) ;;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-breakpad]) ;;
        	 esac
        	 ],
    	    [echo "breakpad is disabled"])

AC_ARG_ENABLE([cpc],
        [AS_HELP_STRING([--enable-cpc], [Enable CPC support])],
        [USE_CPC="$enableval"],
        [USE_CPC=no])

AM_CONDITIONAL([USE_CPC], [test "$USE_CPC" = "yes"])
AC_SUBST(USE_CPC)

if test "$USE_CPC" = "yes"; then
        AC_DEFINE([USE_CPC], [1], [Define to enable CPC support])
fi

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
