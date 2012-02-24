EXTRA_DIST = testdom.xml testdoc.xhtml

noinst_PROGRAMS  = testdom
testdom_SOURCES  = testbase.cpp testbase.hpp testdom.cpp
testdom_CXXFLAGS = @V8_INCLUDEDIR@ -I@top_srcdir@/lib/xv8
testdom_LDFLAGS  = @V8_LIBDIR@ -L@top_builddir@/lib/xv8
testdom_LDADD    = @V8_LIBS@ -lxv8

check-local:
	LD_LIBRARY_PATH="@top_builddir@/lib/xv8:@top_builddir@/lib/v8/v8" ./testdom @srcdir@/testdom.xml || echo "test(s) failed"

distclean-local:
	rm -f *.stackdump
