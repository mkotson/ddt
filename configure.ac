#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(ddt, 0.1.5, mkotson@gmail.com)
AC_CONFIG_SRCDIR([src/ddt.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE(ddt,0.1.4)

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(ncurses,initscr)
AC_CHECK_LIB(atasmart,sk_disk_open)
AC_CHECK_LIB(statgrab,sg_get_cpu_stats)
AC_CHECK_LIB(pthread,pthread_create)
# Checks for header files.
AC_CHECK_HEADERS([mntent.h stdlib.h string.h sys/vfs.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_GETMNTENT
AC_FUNC_MALLOC
AC_CHECK_FUNCS([getcwd getmntent pathconf strrchr])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
