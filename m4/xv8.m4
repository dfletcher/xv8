AC_DEFUN([XV8_CYGWIN_BUILD], [
  AC_MSG_CHECKING([for cygwin build])
  BUILD_USING_CYGWIN=no
  case $build_os in
    *cygwin* ) BUILD_USING_CYGWIN=yes;;
  esac
  AC_SUBST([BUILD_USING_CYGWIN])
  AC_MSG_RESULT([${BUILD_USING_CYGWIN}])
])

AC_DEFUN([XV8_CYGWIN_HOST], [
  AC_MSG_CHECKING([for cygwin host])
  CYGWIN_HOST=no
  case $host_os in
    *cygwin* ) CYGWIN_HOST=yes;;
  esac
  AC_SUBST([CYGWIN_HOST])
  AC_MSG_RESULT([${CYGWIN_HOST}])
])

AC_DEFUN([XV8_MINGW_HOST], [
  AC_MSG_CHECKING([for mingw host])
  MINGW_HOST=no
  case $host_os in
    *mingw* ) MINGW_HOST=yes;;
  esac
  AC_SUBST([MINGW_HOST])
  if test "X${MINGW_HOST}" = "Xyes"; then
    LIBS="${LIBS} -lws2_32 -lwinmm"
    CXXFLAGS="${CXXFLAGS} -Wl,--subsystem,console"
  fi
  AC_MSG_RESULT([${MINGW_HOST}])
])

AC_DEFUN([XV8_MANDATORY_PROGRAM], [
  AC_PATH_PROG([$1], [$2], [no])
  if test ! -x "${$1}"; then
    if test "X$3" != "X"; then
      AC_MSG_FAILURE([$3])
    else
      AC_MSG_FAILURE(["$2" program not found, it is required to build xv8.])
    fi
  fi
])

AC_DEFUN([XV8_MANDATORY_LIB], [
  AC_CHECK_LIB([$1], [$2], [], [
    if test "X$3" != "X"; then
      AC_MSG_FAILURE([$3])
    else
      AC_MSG_FAILURE([Library "$1" not found, it is required to build xv8.])
    fi
  ])
])

AC_DEFUN([XV8_DEBUG_COND], [
  AC_MSG_CHECKING([$4])
  $1="$2"
  if [[[ ${CXXFLAGS} == *-g* ]]]; then
    $1="$3"
  fi
  AC_MSG_RESULT([${$1}])
])

AC_DEFUN([XV8_INIT], [
  AC_SUBST([XV8_DISTCHECK_FLAGS])
  XV8_CYGWIN_BUILD
  XV8_CYGWIN_HOST
  XV8_MINGW_HOST
])
