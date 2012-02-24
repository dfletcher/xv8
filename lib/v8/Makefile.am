all-local: v8-all

clean-local: v8-clean

distclean-local: v8-distclean

v8-all: v8-prep v8-static v8-shared

v8-prep:
	if test "X@BUILD_USING_CYGWIN@" = "Xyes"; then \
	  test -d @builddir@/v8/obj/release || mkdir -p @builddir@/v8/obj/release; \
	  test -f @builddir@/v8/obj/release/runtime.js || cp @builddir@/v8/src/*.js @builddir@/v8/obj/release; \
	  test -f @builddir@/v8/obj/release/macros.py || cp @builddir@/v8/src/macros.py @builddir@/v8/obj/release; \
	  test -d @builddir@/v8/obj/debug || mkdir -p @builddir@/v8/obj/debug; \
	  test -f @builddir@/v8/obj/debug/runtime.js || cp @builddir@/v8/src/*.js @builddir@/v8/obj/debug; \
	  test -f @builddir@/v8/obj/debug/macros.py || cp @builddir@/v8/src/macros.py @builddir@/v8/obj/debug; \
	fi;

v8-static:
	CXX="@CXX@" LINKFLAGS="@LDFLAGS@" CXXFLAGS="@CXXFLAGS@" "@SCONS@" -C @builddir@/v8 @V8_SCONS_FLAGS@ library=static

v8-shared:
	CXX="@CXX@" LINKFLAGS="@V8_SHARED_LDFLAGS@ @LDFLAGS@" CXXFLAGS="@CXXFLAGS@" "@SCONS@" -C @builddir@/v8 @V8_SCONS_FLAGS@ library=shared

v8-clean:
	rm -rf @builddir@/v8/obj;
	rm -rf @builddir@/v8/@V8_SONAME@.a;
	rm -rf @builddir@/v8/@V8_SONAME@.dll;
	rm -f @builddir@/v8/@V8_SONAME@.def;
	rm -f @builddir@/v8/lib@V8_SONAME@.dll.a;

v8-distclean: v8-clean
	rm -rf @builddir@/v8;
