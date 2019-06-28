# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.

AC_INIT([dpdk-burst-replay], 1.1.1)
AC_PREREQ([2.59])
AC_DEFINE_UNQUOTED([DPDK_REPLAY_VERSION_GIT], ["`git rev-parse HEAD`"], [current version])

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AM_PROG_CC_C_O

# DEBUG
AC_ARG_ENABLE(debug,
        AS_HELP_STRING([--enable-debug], [turns on debug compiler, default:no]))
if test x$enable_debug = xyes; then
    AC_DEFINE([DEBUG], [1], [debug enabled])
    CFLAGS+=" -ggdb -W -Wall -DDEBUG -O0"
else
    CFLAGS+=" -W -Wall -DNDEBUG -O2"
fi

AC_CONFIG_FILES([Makefile
                src/Makefile])
AC_OUTPUT
