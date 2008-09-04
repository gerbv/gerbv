## -*- Makefile -*-
##
## $Id$
##

MAINTAINERCLEANFILES = *~ *.o Makefile Makefile.in

EXTRA_DIST= build_gerbv extract_gtk_win32 gerbv.nsi.in Readme.txt \
	registerExtension.nsh

if WIN32
pkgdata_DATA=	Readme.txt BUGS.txt COPYING.txt

BUGS.txt: $(top_srcdir)/BUGS
	awk '{print $$0 "\r"}' $(top_srcdir)/BUGS > $@

COPYING.txt: $(top_srcdir)/COPYING
	awk '{print $$0 "\r"}' $(top_srcdir)/COPYING > $@

CLEANFILES = BUGS.txt COPYING.txt
endif

