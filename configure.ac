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

dnl AC_CONFIG_FILES([Makefile])
AC_OUTPUT
