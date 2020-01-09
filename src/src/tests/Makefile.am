
NULL =

INCLUDES =                                              		\
	-I$(top_builddir)/src                           		\
	-I$(top_srcdir)/src                             		\
	-DPACKAGE_LIBEXEC_DIR=\""$(libexecdir)"\"       		\
	-DPACKAGE_SYSCONF_DIR=\""$(sysconfdir)"\"       		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"             		\
	-DPACKAGE_BIN_DIR=\""$(bindir)"\"               		\
	-DPACKAGE_LOCALSTATE_DIR=\""$(localstatedir)"\" 		\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"         		\
	-DPACKAGE_LIB_DIR=\""$(libdir)"\"               		\
	-D_POSIX_PTHREAD_SEMANTICS                      		\
	-D_REENTRANT	                                		\
	-DEGG_DBUS_I_KNOW_API_IS_SUBJECT_TO_CHANGE			\
	$(NULL)

###########################################################################

noinst_LTLIBRARIES=libeggdbustests.la

BUILT_SOURCES = 							\
	test-built-sources.stamp					\
	$(NULL)

test-built-sources.stamp : Makefile.am $(top_builddir)/src/eggdbus/eggdbus-binding-tool com.example.Frob.xml com.example.Tweak.xml com.example.Twiddle.xml
	EGG_DBUS_GENMARSHAL=$(top_builddir)/src/eggdbus/eggdbus-glib-genmarshal $(top_builddir)/src/eggdbus/eggdbus-binding-tool									\
	  --namespace       "Test"										\
	  --dbus-namespace  "com.example"									\
	  --include-header  testtypes.h										\
	  --introspection-xml $(top_srcdir)/src/tests/com.example.Frob.xml					\
	  --introspection-xml $(top_srcdir)/src/tests/com.example.Tweak.xml					\
	  --introspection-xml $(top_srcdir)/src/tests/com.example.Twiddle.xml					\
	  --stamp-file test-built-sources.stamp									\
	  $(NULL)

# keep in sync with contents of test-built-sources.stamp (Thanks autotools)
#
test_built_sources = testbindingsmarshal.c testbindingsmarshal.h testbindingsmarshal.list testbindings.c testbindings.h testbindingstypes.h testtwiddle.c testtwiddle.h docbook-interface-com.example.Twiddle.xml testtweak.c testtweak.h docbook-interface-com.example.Tweak.xml testfrob.c testfrob.h docbook-interface-com.example.Frob.xml testvehicle.c testvehicle.h docbook-enum-Vehicle.xml testotherflags.c testotherflags.h docbook-enum-OtherFlags.xml testdeleteflags.c testdeleteflags.h docbook-enum-DeleteFlags.xml testcreateflags.c testcreateflags.h docbook-enum-CreateFlags.xml testdetailederror.c testdetailederror.h docbook-enum-DetailedError.xml testerror.c testerror.h docbook-enum-Error.xml teststructwithvariant.c teststructwithvariant.h docbook-struct-StructWithVariant.xml testextendeddescribedpoint.c testextendeddescribedpoint.h docbook-struct-ExtendedDescribedPoint.xml testdescribedpair.c testdescribedpair.h docbook-struct-DescribedPair.xml testdescribedpoint.c testdescribedpoint.h docbook-struct-DescribedPoint.xml testpair.c testpair.h docbook-struct-Pair.xml testpoint.c testpoint.h docbook-struct-Point.xml

libeggdbustests_la_SOURCES = 				\
	test-built-sources.stamp			\
	$(test_built_sources)				\
	testsubject.h		testsubject.c		\
	testfrobimpl.h		testfrobimpl.c		\
	testtweakimpl.h		testtweakimpl.c		\
	testtwiddleimpl.h	testtwiddleimpl.c	\
	$(NULL)

libeggdbustests_la_CFLAGS =                		\
	$(GLIB_CFLAGS)					\
	$(NULL)

libeggdbustests_la_LIBADD =				\
	$(GLIB_LIBS)					\
	$(top_builddir)/src/eggdbus/libeggdbus-1.la	\
	$(NULL)

###########################################################################

noinst_PROGRAMS = testclient

testclient_SOURCES =					\
				testclient.c		\
	testtypes.h					\
	$(NULL)

testclient_CFLAGS =                			\
	$(GLIB_CFLAGS)					\
	$(NULL)

testclient_LDADD =					\
	$(GLIB_LIBS)					\
	$(top_builddir)/src/eggdbus/libeggdbus-1.la	\
	libeggdbustests.la				\
	$(NULL)

###########################################################################

noinst_PROGRAMS += testserver

testserver_SOURCES =					\
				testserver.c		\
	testtypes.h					\
	$(NULL)

testserver_CFLAGS =                			\
	$(GLIB_CFLAGS)					\
	$(NULL)

testserver_LDADD =					\
	$(GLIB_LIBS)					\
	$(top_builddir)/src/eggdbus/libeggdbus-1.la	\
	libeggdbustests.la				\
	$(NULL)

EXTRA_DIST = 								\
	com.example.Frob.xml 						\
	com.example.Tweak.xml						\
	com.example.Twiddle.xml						\
	$(NULL)

dist-hook :
	(for i in $(test_built_sources) $(BUILT_SOURCES) ; do rm -f $(distdir)/$$i ; done)

clean-local:
	rm -f *~ $(test_built_sources) $(BUILT_SOURCES)
