#
# configure.ac - Autoconf source file for Heyu
#
# Copyright (C) 2011 Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>
#
# Derived from an initial version genereted automatically by the
# autoscan application.
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#   As an additional permission under section 7 of the GNU General
#   Public License, version 3, the author permits you to distribute this
#   file bundled with Heyu software, under the same license terms that
#   Heyu is distributed, as long as it is propagated for the sole
#   purpose of configuring and building Heyu.
#   

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([Heyu], [2.10-rc2],
	[heyu@tickets.assembla.com], [heyu], [http://heyu.org])
AC_COPYRIGHT([Heyu specific bits:
Copyright (C) 2011 Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>])
AC_CONFIG_SRCDIR([relay.c])
AC_CONFIG_HEADERS([config.h])

# Optional Features
AC_MSG_CHECKING([whether to enable CM17A Firecracker support])
	AC_ARG_ENABLE([cm17a], [AS_HELP_STRING([--disable-cm17a],
		[omit support for optional CM17A Firecracker device])], [],
		[enable_cm17a=yes])
	AC_MSG_RESULT([$enable_cm17a])
	AS_IF([test "x$enable_cm17a" != "xno"], [AC_DEFINE([HAVE_FEATURE_CM17A],
		[1], [Define if you have the CM17A Firecracker device])], [])
AC_MSG_CHECKING([whether to enable extended type 0 shutter controllers support])
	AC_ARG_ENABLE([ext0], [AS_HELP_STRING([--disable-ext0],
		[omit support for extended type 0 shutter controllers])], [],
		[enable_ext0=yes])
	AC_MSG_RESULT([$enable_ext0])
	AS_IF([test "x$enable_ext0" != "xno"], [AC_DEFINE([HAVE_FEATURE_EXT0],
		[1], [Define if you have extended type 0 shutter controllers])],
		[])
AC_MSG_CHECKING([whether to enable RFXSensors support])
	AC_ARG_ENABLE([rfxs], [AS_HELP_STRING([--disable-rfxs],
		[omit support for RFXSensors])], [], [enable_rfxs=yes])
	AC_MSG_RESULT([$enable_rfxs])
	AS_IF([test "x$enable_rfxs" != "xno"], [AC_DEFINE([HAVE_FEATURE_RFXS],
		[1], [Define if you have RFXSensors])], [])
AC_MSG_CHECKING([whether to enable RFXMeters support])
	AC_ARG_ENABLE([rfxm], [AS_HELP_STRING([--disable-rfxm],
		[omit support for RFXMeters])], [], [enable_rfxm=yes])
	AC_MSG_RESULT([$enable_rfxm])
	AS_IF([test "x$enable_rfxm" != "xno"], [AC_DEFINE([HAVE_FEATURE_RFXM],
		[1], [Define if you have RFXMeters])], [])
AC_MSG_CHECKING([whether to enable Digimax thermostat support])
	AC_ARG_ENABLE([dmx], [AS_HELP_STRING([--disable-dmx],
		[omit support for the Digimax thermostat])], [],
		[enable_dmx=yes])
	AC_MSG_RESULT([$enable_dmx])
	AS_IF([test "x$enable_dmx" != "xno"], [AC_DEFINE([HAVE_FEATURE_DMX],
		[1], [Define if you have Digimax thermostats])], [])
AC_MSG_CHECKING([whether to enable Oregon sensors support])
	AC_ARG_ENABLE([ore], [AS_HELP_STRING([--disable-ore],
		[omit support for Oregon sensors])], [], [enable_ore=yes])
	AC_MSG_RESULT([$enable_ore])
	AS_IF([test "x$enable_ore" != "xno"], [AC_DEFINE([HAVE_FEATURE_ORE],
		[1], [Define if you have Oregon sensors])], [])
AC_MSG_CHECKING([whether to enable support for KaKu/HomeEasy signals])
	AC_ARG_ENABLE([kaku], [AS_HELP_STRING([--disable-kaku],
		[omit support for KaKu/HomeEasy signals])], [],
		[enable_kaku=yes])
	AC_MSG_RESULT([$enable_kaku])
	AS_IF([test "x$enable_kaku" != "xno"], [AC_DEFINE([HAVE_FEATURE_KAKU],
		[1], [Define if you have KaKu/HomeEasy devices])], [])

AC_MSG_CHECKING([for number of user flags])
	AC_ARG_VAR([FLAGS], [set number of user flags (1 < n < 1024) [32]])
	AS_IF([test "x$FLAGS" = "x"], [FLAGS=32],
	AS_IF([test $FLAGS -ge 1 && test $FLAGS -le 1024],
		[FLAGS=`expr \( $FLAGS + 31 \) / 32 \* 32`
		AC_DEFINE_UNQUOTED([NUM_FLAG_BANKS], [`expr $FLAGS / 32`],
		[Define if you need more than 1 bank of 32 user flags each])],
		AC_MSG_FAILURE(
			[wrong FLAGS value "$FLAGS" (shoud be 1 < n < 1024)])))
	AC_MSG_RESULT([$FLAGS])
AC_MSG_CHECKING([for number of user counters])
	AC_ARG_VAR([COUNTERS],
		[set number of user counters (1 < n < 1024) [32]])
	AS_IF([test "x$COUNTERS" = "x"], [COUNTERS=32],
	AS_IF([test $COUNTERS -ge 1 && test $COUNTERS -le 1024],
		[COUNTERS=`expr \( $COUNTERS + 31 \) / 32 \* 32`
		AC_DEFINE_UNQUOTED([NUM_COUNTER_BANKS], [`expr $COUNTERS / 32`],
		[Define if you need more than 1 bank of 32 user counters each]
		)], AC_MSG_FAILURE(
		[wrong COUNTERS value "$COUNTERS" (shoud be 1 < n < 1024)])))
	AC_MSG_RESULT([$COUNTERS])
AC_MSG_CHECKING([for number of user timers])
	AC_ARG_VAR([TIMERS], [set number of user timers (1 < n < 1024) [32]])
	AS_IF([test "x$TIMERS" = "x"], [TIMERS=32],
	AS_IF([test $TIMERS -ge 1 && test $TIMERS -le 1024],
		[TIMERS=`expr \( $TIMERS + 31 \) / 32 \* 32`
		AC_DEFINE_UNQUOTED([NUM_TIMER_BANKS], [`expr $TIMERS / 32`],
		[Define if you need more than 1 bank of 32 user timers each])],
		AC_MSG_FAILURE(
		[wrong TIMERS value "$TIMERS" (shoud be 1 < n < 1024)])))
	AC_MSG_RESULT([$TIMERS])

AC_SUBST([PACKAGE], [$PACKAGE_TARNAME])
AS_CASE([$sysconfdir],
	[*$PACKAGE*], [pkgsysconfdir='$(sysconfdir)'],
	[\$\(prefix\)*|\$\{prefix\}*],
		AS_CASE([$prefix],
			[*$PACKAGE*], [pkgsysconfdir='$(sysconfdir)'],
			[pkgsysconfdir='$(sysconfdir)/$(PACKAGE)']),
	[pkgsysconfdir='$(sysconfdir)/$(PACKAGE)'])
	AC_SUBST([pkgsysconfdir], [])
AS_CASE([$CPPFLAGS],
	[*-DSYSBASEDIR=*], [],
	[CPPFLAGS=$CPPFLAGS' -DSYSBASEDIR=\"$(pkgsysconfdir)\"'])
AS_CASE([$CPPFLAGS],
	[*-DLOCKDIR=*], [],
	[CPPFLAGS=$CPPFLAGS' -DLOCKDIR=\"$(localstatedir)/lock\"'])
AS_CASE([$CPPFLAGS],
	[*-DSPOOLDIR=*], [],
	[CPPFLAGS=$CPPFLAGS' -DSPOOLDIR=\"$(localstatedir)/tmp/$(PACKAGE)\"'])

dnl AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for libraries.
AC_SEARCH_LIBS([sqrt], [m])
AC_SEARCH_LIBS([nanosleep], [rt])
AC_SEARCH_LIBS([syslog], [socket])
AC_SEARCH_LIBS([tcgetattr], [posix])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h netdb.h sgtty.h stdlib.h string.h strings.h])
AC_CHECK_HEADERS([sys/filsys.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h])
AC_CHECK_HEADERS([syslog.h termio.h termios.h unistd.h])
AC_CHECK_HEADERS([asm/ioctls.h errno.h linux/serial.h linux/serial_reg.h pty.h])
AC_CHECK_HEADERS([sys/errno.h sys/resource.h sys/termio.h sys/termios.h])
AC_HEADER_SYS_WAIT
AC_HEADER_TIME

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_UNSIGNED_LONG_LONG_INT
AC_CHECK_TYPES([void], [], [], [])
AC_CHECK_TYPES([struct termios], [], [], [
#ifdef HAVE_SYS_TERMIOS_H
# include <sys/termios.h>
#endif
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif
])
AC_CHECK_TYPES([struct termio], [], [], [
#ifdef HAVE_SYS_TERMIO_H
# include <sys/termio.h>
#endif
#ifdef HAVE_TERMIO_H
# include <termio.h>
#endif
])

# Checks for declarations.
AC_CHECK_DECLS(timezone, [], [], [
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_GETGROUPS
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([alarm floor ftruncate gettimeofday memmove memset putenv])
AC_CHECK_FUNCS([socket sqrt strchr strdup strerror strrchr strstr strtol])
AC_CHECK_FUNCS([nanosleep nsleep setitimer sigaction siginterrupt])

AC_MSG_CHECKING(whether to enable RFXLAN receiver support)
	AC_ARG_ENABLE([rfxlan], [AS_HELP_STRING([--disable-rfxlan],
		[omit support for RFXLAN receiver])], [], [enable_rfxlan=yes])
	AS_IF([test "x$enable_rfxlan" != "xno" &&
		test "x$ac_cv_header_netdb_h" != "xyes" \
			-o "x$ac_cv_func_memset" != "xyes" \
			-o "x$ac_cv_func_socket" != "xyes"],
		[enable_rfxlan=no], [])
	AC_MSG_RESULT($enable_rfxlan)
	AS_IF([test "x$enable_rfxlan" != "xno"],
		[AC_DEFINE([HAVE_FEATURE_RFXLAN], [1],
		[Define if you have RFXLAN receiver])], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
