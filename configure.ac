#               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([FULL-PACKAGE-NAME],[VERSION],[BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.10 foreign])

# Checks for argument
AC_ARG_ENABLE(static,
    AS_HELP_STRING([--enable-static],[enable static build yes or no(default: no)]),
    [case "${enableval}" in
    yes) static=true;;
    no)  static=false;;
    *) AC_MSG_ERROR([bad value ${enableval} for --enable-static]);;
    esac],[static=false])
AM_CONDITIONAL(STATIC, test x$static = xtrue)

AC_ARG_ENABLE(arch,
    AS_HELP_STRING([--enable-arch],[set architecture x86/arm/arm64 (default: arm)]),
    [case "${enableval}" in
      x86) arch=x86;;
      arm) arch=arm;;
      arm64) arch=arm64;;
      *) AC_MSG_ERROR(bad value ${enableval} for --enable-arch);;
      esac],
    [arch=arm])
AC_SUBST(LIBDRM_ARCH, [$arch])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
