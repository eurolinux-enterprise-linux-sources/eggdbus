
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
	$(NULL)

###########################################################################

# First build a static library with most of the code; from that we can
# build the eggdbus-binding-tool program that we will use to generate wrappers
# for standard D-Bus interfaces included in our target library
#

noinst_LTLIBRARIES = libeggdbus-common.la

libeggdbus_common_la_SOURCES = 						\
	eggdbus.h							\
	eggdbusprivate.h						\
	eggdbusarrayseq.h		eggdbusarrayseq.c		\
	eggdbushashmap.h		eggdbushashmap.c		\
	eggdbuserror.h			eggdbuserror.c			\
	eggdbusmisctypes.h		eggdbusmisctypes.c		\
	eggdbusobjectpath.h		eggdbusobjectpath.c		\
	eggdbussignature.h		eggdbussignature.c		\
	eggdbusstructure.h		eggdbusstructure.c		\
	eggdbustypes.h			eggdbustypes.c			\
	eggdbusutils.h			eggdbusutils.c			\
	eggdbusvariant.h		eggdbusvariant.c		\
	eggdbusinterface.h		eggdbusinterface.c		\
					eggdbusintrospector.c		\
	eggdbusmarshal.h		eggdbusmarshal.c		\
	eggdbusenumtypes.h		eggdbusenumtypes.c		\
	$(NULL)

libeggdbus_common_la_CFLAGS =						\
        -D_EGG_DBUS_COMPILATION                                  	\
	$(GLIB_CFLAGS)							\
	$(DBUS_GLIB_CFLAGS)						\
	$(NULL)

libeggdbus_common_la_LIBADD =						\
	$(NULL)

###########################################################################

bin_PROGRAMS = eggdbus-glib-genmarshal

eggdbus_glib_genmarshal_SOURCES =					\
	eggdbus-glib-genmarshal.c					\
	$(NULL)

eggdbus_glib_genmarshal_CFLAGS =					\
	$(GLIB_CFLAGS)							\
	$(NULL)

eggdbus_glib_genmarshal_LDADD =						\
	$(GLIB_LIBS)							\
	$(NULL)

###########################################################################

bin_PROGRAMS += eggdbus-binding-tool

eggdbus_binding_tool_SOURCES = 						\
	eggdbusbindingtool.c		eggdbusbindingtool.h		\
	interface.c			interface.h			\
	struct.c			struct.h			\
	enum.c				enum.h				\
	docbook.c			docbook.h			\
	completetype.c			completetype.h			\
	$(NULL)

eggdbus_binding_tool_CFLAGS =                		      		\
        -D_EGG_DBUS_COMPILATION                                  	\
	$(GLIB_CFLAGS)							\
	$(DBUS_GLIB_CFLAGS)						\
	$(NULL)

eggdbus_binding_tool_LDADD =  	               				\
	$(GLIB_LIBS)							\
	$(DBUS_GLIB_LIBS)						\
	libeggdbus-common.la						\
	$(NULL)

###########################################################################

eggdbus_headers =                        				\
	eggdbus.h							\
	eggdbusarrayseq.h						\
	eggdbushashmap.h						\
	eggdbuserror.h							\
	eggdbusmessage.h						\
	eggdbusobjectproxy.h						\
	eggdbusinterfaceproxy.h						\
	eggdbusstructure.h						\
	eggdbustypes.h							\
	eggdbusutils.h							\
	eggdbusvariant.h						\
	eggdbusinterface.h						\
	eggdbusconnection.h						\
	eggdbusbusnametracker.h						\
	eggdbusmethodinvocation.h					\
	eggdbusenumtypes.h						\
	eggdbusrequestnameflags.h					\
	eggdbusrequestnamereply.h					\
	eggdbusreleasenamereply.h					\
	eggdbusstartservicebynamereply.h				\
	eggdbusmisctypes.h						\
	eggdbusobjectpath.h						\
	eggdbussignature.h						\
	$(NULL)

###########################################################################

BUILT_SOURCES =								\
	eggdbusmarshal.h		eggdbusmarshal.c		\
	eggdbusenumtypes.h		eggdbusenumtypes.c		\
	eggdbus-built-sources.stamp					\
	$(NULL)

eggdbus-built-sources.stamp : eggdbus-binding-tool Makefile.am org.freedesktop.DBus.xml org.freedesktop.DBus.Peer.xml org.freedesktop.DBus.Properties.xml org.freedesktop.DBus.Introspectable.xml eggdbus-glib-genmarshal
	EGG_DBUS_GENMARSHAL=$(top_builddir)/src/eggdbus/eggdbus-glib-genmarshal $(top_builddir)/src/eggdbus/eggdbus-binding-tool							\
		--namespace EggDBus									\
		--dbus-namespace org.freedesktop.DBus							\
		--interfaces-only									\
		--no-types-header									\
		--include-header "eggdbus/eggdbustypes.h"						\
		--include-header "eggdbus/eggdbusinterface.h"						\
		--include-header "eggdbus/eggdbusinterfaceproxy.h"					\
		--include-header "eggdbus/eggdbusrequestnameflags.h"					\
		--include-header "eggdbus/eggdbusrequestnamereply.h"					\
		--include-header "eggdbus/eggdbusreleasenamereply.h"					\
		--include-header "eggdbus/eggdbusstartservicebynamereply.h"				\
		--introspection-xml $(top_srcdir)/src/eggdbus/org.freedesktop.DBus.xml			\
		--introspection-xml $(top_srcdir)/src/eggdbus/org.freedesktop.DBus.Peer.xml		\
		--introspection-xml $(top_srcdir)/src/eggdbus/org.freedesktop.DBus.Properties.xml	\
		--introspection-xml $(top_srcdir)/src/eggdbus/org.freedesktop.DBus.Introspectable.xml	\
		--stamp-file eggdbus-built-sources.stamp						\
		$(NULL)

# keep in sync with contents of eggdbus-built-sources.stamp (Thanks autotools)
#
egg_dbus_built_sources = eggdbusbindingsmarshal.c eggdbusbindingsmarshal.h eggdbusbindingsmarshal.list eggdbusintrospectable.c eggdbusintrospectable.h docbook-interface-org.freedesktop.DBus.Introspectable.xml eggdbusproperties.c eggdbusproperties.h docbook-interface-org.freedesktop.DBus.Properties.xml eggdbuspeer.c eggdbuspeer.h docbook-interface-org.freedesktop.DBus.Peer.xml eggdbusbus.c eggdbusbus.h docbook-interface-org.freedesktop.DBus.xml eggdbusstartservicebynamereply.c eggdbusstartservicebynamereply.h eggdbusreleasenamereply.c eggdbusreleasenamereply.h eggdbusrequestnamereply.c eggdbusrequestnamereply.h eggdbusrequestnameflags.c eggdbusrequestnameflags.h

enum_headers =                        					\
	eggdbusinterface.h						\
	eggdbusconnection.h						\
	eggdbuserror.h							\
	eggdbusmessage.h						\
	$(NULL)

eggdbusenumtypes.h: $(enum_headers) eggdbusenumtypes.h.template
	( top_builddir=`cd $(top_builddir) && pwd`; \
	  cd $(srcdir) && glib-mkenums --template eggdbusenumtypes.h.template $(enum_headers) ) > \
	    eggdbusenumtypes.h.tmp && mv eggdbusenumtypes.h.tmp eggdbusenumtypes.h

eggdbusenumtypes.c: $(enum_headers) eggdbusenumtypes.c.template
	( top_builddir=`cd $(top_builddir) && pwd`; \
	  cd $(srcdir) && glib-mkenums --template eggdbusenumtypes.c.template $(enum_headers) ) > \
	    eggdbusenumtypes.c.tmp && mv eggdbusenumtypes.c.tmp eggdbusenumtypes.c

eggdbusmarshal.h : eggdbusmarshal.list
	glib-genmarshal $< --prefix=eggdbus_marshal --header > $@

eggdbusmarshal.c : eggdbusmarshal.list eggdbusmarshal.h
	echo "#include \"eggdbusmarshal.h\"" > $@ && glib-genmarshal $< --prefix=eggdbus_marshal --body >> $@

###########################################################################

lib_LTLIBRARIES = libeggdbus-1.la

libeggdbus_1includedir=$(includedir)/eggdbus-1/eggdbus

libeggdbus_1include_HEADERS = 						\
	$(eggdbus_headers)						\
	eggdbusbus.h							\
	eggdbuspeer.h							\
	eggdbusproperties.h						\
	eggdbusintrospectable.h						\
	$(NULL)

libeggdbus_1_la_SOURCES = 						\
	eggdbus-built-sources.stamp					\
	$(egg_dbus_built_sources)					\
	eggdbusconnection.h		eggdbusconnection.c		\
	eggdbusmessage.h		eggdbusmessage.c		\
	eggdbusobjectproxy.h		eggdbusobjectproxy.c		\
	eggdbusinterfaceproxy.h		eggdbusinterfaceproxy.c		\
	eggdbusbusnametracker.h		eggdbusbusnametracker.c		\
	eggdbusmethodinvocation.h	eggdbusmethodinvocation.c	\
	$(NULL)

libeggdbus_1_la_CFLAGS =						\
        -D_EGG_DBUS_COMPILATION                                  	\
	$(GLIB_CFLAGS)							\
	$(DBUS_GLIB_CFLAGS)						\
	$(NULL)

libeggdbus_1_la_LIBADD =						\
	$(GLIB_LIBS)							\
	$(DBUS_GLIB_LIBS)						\
	libeggdbus-common.la						\
	$(NULL)

EXTRA_DIST = 								\
	gmarshal.strings						\
	eggdbusmarshal.list						\
	eggdbusenumtypes.h.template					\
	eggdbusenumtypes.c.template					\
	org.freedesktop.DBus.xml 					\
	org.freedesktop.DBus.Peer.xml 					\
	org.freedesktop.DBus.Properties.xml 				\
	org.freedesktop.DBus.Introspectable.xml				\
	$(NULL)

###########################################################################

TESTS = completetypetest

check_PROGRAMS = completetypetest

completetypetest_SOURCES = 				\
	completetypetest.c				\
	completetype.c		completetype.h		\
	$(NULL)

completetypetest_CFLAGS =                		\
        -D_EGG_DBUS_COMPILATION                      	\
	$(GLIB_CFLAGS)					\
	$(DBUS_GLIB_CFLAGS)				\
	$(NULL)

completetypetest_LDADD =				\
	$(GLIB_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	libeggdbus-1.la					\
	$(NULL)

###########################################################################

dist-hook :
	(for i in $(egg_dbus_built_sources) $(BUILT_SOURCES) ; do rm -f $(distdir)/$$i ; done)

clean-local :
	rm -f *~ $(egg_dbus_built_sources) $(BUILT_SOURCES)

