
NULL =

if MAN_PAGES_ENABLED

man_MANS = eggdbus-binding-tool.1 	   \
	   $(NULL)

%.1 : %.xml
	$(XSLTPROC) -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

endif # MAN_PAGES_ENABLED

EXTRA_DIST= eggdbus-binding-tool.xml        	\
	    $(NULL)

clean-local:
	rm -f *~ *.1
