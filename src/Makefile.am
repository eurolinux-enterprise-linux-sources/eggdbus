## Process this file with automake to produce Makefile.in

SUBDIRS = src docs

NULL =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = eggdbus-1.pc

EXTRA_DIST = 			\
	HACKING 		\
	eggdbus-1.pc.in		\
	$(NULL)

DISTCLEANFILES = 		\
	eggdbus-1.pc		\
	$(NULL)

clean-local :
	rm -f *~
