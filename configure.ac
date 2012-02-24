#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([xv8], [0.0.0], [fletch@monkeysatkeyboards.com])
AC_CONFIG_SRCDIR([lib/xv8/dom.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([xv8], [0.0.0])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_LANG([C++])

# TODO: when these source/build dir checks are put in a macro,
# autoreconf ends up in an infinite loop. Figure out why / fix.

XV8_MANDATORY_PROGRAM([DIRNAME], [dirname])
XV8_MANDATORY_PROGRAM([READLINK], [readlink])

AC_MSG_CHECKING([top source directory])
XV8_TOP_SRCDIR="`${DIRNAME} $0`"
XV8_TOP_SRCDIR="`${READLINK} -e ${XV8_TOP_SRCDIR}`"
AC_SUBST(XV8_TOP_SRCDIR)
AC_MSG_RESULT([${XV8_TOP_SRCDIR}])

AC_MSG_CHECKING([top build directory])
XV8_TOP_BUILDDIR="${PWD}"
XV8_TOP_BUILDDIR="`${READLINK} -e ${XV8_TOP_BUILDDIR}`"
AC_SUBST(XV8_TOP_BUILDDIR)
AC_MSG_RESULT([${XV8_TOP_BUILDDIR}])

XV8_INIT

# Checks for programs.
AC_PROG_CXX
LT_INIT
LT_PATH_LD
LT_PATH_NM

# Checks for libraries.
XV8_CHECK_XERCES
XV8_CHECK_V8

# Checks for header files.
AC_CHECK_HEADERS([string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([strrchr])

AC_CONFIG_COMMANDS([xv8], [
  
])

AC_CONFIG_FILES([
  Makefile
  lib/Makefile
  lib/v8/Makefile
  lib/xv8/Makefile
  bin/Makefile
  test/Makefile
])
AC_OUTPUT
