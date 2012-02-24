
AC_DEFUN([XV8_CHECK_V8], [

  AC_MSG_CHECKING([v8 scons jobs])
  AC_ARG_WITH([v8-jobs],[AS_HELP_STRING([--with-v8-jobs=(JOBS)], [specify the number of jobs to run in scons if building v8 @<:@default=1@:>@])], [V8_JOBS=$withval], [V8_JOBS=1])
  AC_SUBST([V8_JOBS])
  AC_MSG_RESULT([${V8_JOBS}])

  AC_MSG_CHECKING([v8 version])
  AC_ARG_WITH([v8-version],[AS_HELP_STRING([--with-v8-lib=(VERSION)], [specify v8 version if building v8 @<:@default=3.2.3@:>@])], [V8_VERSION=$withval], [V8_VERSION=3.2.3])
  AC_SUBST([V8_VERSION])
  AC_MSG_RESULT([${V8_VERSION}])

  AC_MSG_CHECKING([v8 repository])
  AC_ARG_WITH([v8-repository],[AS_HELP_STRING([--with-v8-repository=(REPOSITORY)], [specify v8 git repository if building v8 @<:@default=git://github.com/v8/v8.git@:>@])], [V8_REPOSITORY=$withval], [V8_REPOSITORY=git://github.com/v8/v8.git])
  AC_SUBST([V8_REPOSITORY])
  AC_MSG_RESULT([${V8_REPOSITORY}])

  V8_BUILD=""
  XV8_DEBUG_COND([V8_BUILDMODE], [release], [debug], [v8 build])
  XV8_DEBUG_COND([V8_SONAME], [v8], [v8_g], [v8 shared object base name])
  V8_LIBS="-l${V8_SONAME}"
  V8_SHARED_LDFLAGS=""
  V8_LIBDIR=""
  V8_SCONS_FLAGS="-j${V8_JOBS} mode=${V8_BUILDMODE} objectprint=on debuggersupport=on inspector=on verbose=on"

  AC_ARG_ENABLE(v8-build, [AS_HELP_STRING([--enable-v8-build], [Configure and build v8. Requires "git" and "scons" programs. @<:@default=no@:>@])], [
    XV8_MANDATORY_PROGRAM([GIT], [git], [Git is required when you specify --enable-v8-build. Please install it.])
    XV8_MANDATORY_PROGRAM([SCONS], [scons], [Scons is required when you specify --enable-v8-build. Please install it.])
    XV8_MANDATORY_PROGRAM([MKDIR], [mkdir])
    AC_SUBST([V8_LIBS])
    AC_SUBST([V8_SHARED_LDFLAGS])
    AC_SUBST([V8_LIBDIR])
    AC_SUBST([V8_INCLUDEDIR])
    AC_SUBST([V8_SONAME])
    AC_SUBST([V8_SCONS_FLAGS])
    AC_CONFIG_COMMANDS([v8], [
      # Ugh this is terrible. is there any other way to get AC_SUBST'd or
      # other variables from configure to config.status?
      GIT=`cat @S|@0 | grep 'S\[["GIT"\]]' | sed 's/[[^=]]*="\([[^"]]*\)"/\1/g'`
      V8_REPOSITORY=`cat @S|@0 | grep 'S\[["V8_REPOSITORY"\]]' | sed 's/[[^=]]*="\([[^"]]*\)"/\1/g'`
      V8_VERSION=`cat @S|@0 | grep 'S\[["V8_VERSION"\]]' | sed 's/[[^=]]*="\([[^"]]*\)"/\1/g'`
      if test ! -d "./lib/v8/v8"; then
        pushd .
        @S|@MKDIR_P ./lib/v8
        cd ./lib/v8
        @S|@GIT clone @S|@V8_REPOSITORY v8
        cd v8
        @S|@GIT checkout @S|@V8_VERSION
        popd
      fi
    ])
    V8_BUILD="v8"
    if test "X$MINGW_HOST" = "Xyes"; then
      V8_SCONS_FLAGS="${V8_SCONS_FLAGS} os=win32 arch=ia32 msvcltcg=off msvcrt=shared snapshot=off";
      AC_MSG_WARN([javascript snapshots not supported in mingw build, setting snapshot=off])
    else
      V8_SCONS_FLAGS="${V8_SCONS_FLAGS} snapshot=on";
    fi
    if test "X$CYGWIN_HOST" = "Xyes"; then
      AC_MSG_WARN([javascript profiling not supported in cygwin build, setting profilingsupport=off])
    else
      V8_SCONS_FLAGS="${V8_SCONS_FLAGS} profilingsupport=on"
    fi
    test "X$BUILD_USING_CYGWIN" = "Xyes" && V8_SHARED_LDFLAGS="${V8_SHARED_LDFLAGS} -Wl,--out-implib,lib${V8_SONAME}.dll.a"
    V8_LIBDIR="-L$1${XV8_TOP_BUILDDIR}/lib/v8/v8"
    V8_INCLUDEDIR="-I${XV8_TOP_BUILDDIR}/lib/v8/v8/include"
    XV8_DISTCHECK_FLAGS="${XV8_DISTCHECK_FLAGS} --enable-v8-build --with-v8-jobs=${V8_JOBS} --with-v8-version=${V8_VERSION} --with-v8-repository=${V8_REPOSITORY}"
  ],[
    XV8_MANDATORY_LIB([${V8_SONAME}], [main], [The v8 library was not found. This configure script can download and build v8 if you specify --enable-v8-build. Requires "git" and "scons" programs.])
  ])
  AC_SUBST([V8_BUILD])
])
