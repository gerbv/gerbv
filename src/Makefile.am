## Process this file with automake to produce Makefile.in
##
##  gEDA - GNU Electronic Design Automation
##  This file is a part of gerbv.
##
##    Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA

# main program
bin_PROGRAMS = gerbv

# shared library
lib_LTLIBRARIES = libgerbv.la

# public header file(s)
pkgincludedir=	$(includedir)/${PACKAGE}-${VERSION}
pkginclude_HEADERS= gerbv.h

# pkg-config data file
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgerbv.pc

libgerbv_la_SOURCES= \
		amacro.c amacro.h \
		common.h \
		csv.c csv.h csv_defines.h \
		draw-gdk.c draw-gdk.h \
		draw.c draw.h \
		drill.c drill.h \
		drill_stats.c drill_stats.h \
		export-drill.c \
		export-geda-pcb.c \
		export-image.c \
		export-isel-drill.c \
		export-rs274x.c \
		gerb_file.c gerb_file.h \
		gerb_image.c gerb_image.h \
		gerb_stats.c gerb_stats.h \
		gerber.c gerber.h \
		gerbv.c gerbv.h \
		gerbv_icon.h \
		gettext.h \
		pick-and-place.c pick-and-place.h \
		selection.c selection.h \
		tooltable.c

if DXF
libgerbv_la_SOURCES += export-dxf.cpp
endif

# libgerbv shared library version.  A fairly strict rule should
# be followed for when this version is updated.  Those rules,
# taken from the libtool manual, are as follows
#
# - Format is current:revision:age
#
# 1. Start with version information of `0:0:0' for each libtool library.
#
# 2. Update the version information only immediately before a public
#    release of your software. More frequent updates are unnecessary,
#    and only guarantee that the current interface number gets larger faster.
#
# 3. If the library source code has changed at all since the last update,
#    then increment revision (`c:r:a' becomes `c:r+1:a').
#
# 4. If any interfaces have been added, removed, or changed since the last
#    update, increment current, and set revision to 0.
#
# 5. If any interfaces have been added since the last public release, then
#    increment age.
#
# 6. If any interfaces have been removed since the last public release, then
#    set age to 0.
#
libgerbv_la_LDFLAGS = -version-info 1:9:0 -no-undefined $(CODE_COVERAGE_LIBS)

gerbv_SOURCES = \
		attribute.c attribute.h \
		callbacks.c callbacks.h \
		common.h \
		dynload.c dynload.h \
		icons.h \
		interface.c interface.h \
		main.c main.h \
		opdefines.h \
		project.c project.h \
		render.c render.h \
		scheme-private.h scheme.c scheme.h \
		table.c table.h \
		lrealpath.c lrealpath.h

gerbv_LDADD = libgerbv.la
gerbv_DEPENDENCIES = libgerbv.la

# If we are building on win32, then compile in some icons for the
# desktop and application toolbar
if WIN32
gerbv_icon.o : gerbv_icon.ico $(srcdir)/gerbv.rc
	$(WINDRES) $(srcdir)/gerbv.rc $@

gerbv_icon.ico: $(top_srcdir)/desktop/gerbv_icon.ico
	cp $(top_srcdir)/desktop/gerbv_icon.ico $@

gerbv_LDADD+=		gerbv_icon.o
gerbv_DEPENDENCIES+=	gerbv_icon.o

endif

BUILT_SOURCES=	authors.c bugs.c

TXT2CL=	sed -e 's;%;%%;g' -e 's;\\;\\\\;g' -e 's;";\\";g' -e 's;^;N_(";g' -e 's;$$;"),;g' -e 's;N_("");"";g'

authors.c: $(top_srcdir)/AUTHORS Makefile
	if test -f $@.tmp ; then rm -f $@.tmp ; fi
	awk -F: '/^[ \t]*#/ {next}; /^[ \t]*$$/ {next}; {print $$1}' $(top_srcdir)/AUTHORS | ${TXT2CL} > $@.tmp
	echo '/* Generated from AUTHORS file */' > $@
	echo '/*! \file $@' >> $@
	echo '    \brief Generated at build time authors list.' >> $@
	echo '    \ingroup gerbv' >> $@
	echo ' */' >> $@
	num=`wc -l $@.tmp | awk '{print $$1 + 1}'` && \
		echo "const gchar * authors_string_array[$$num] = {" >> $@
	cat $@.tmp >> $@
	rm -f $@.tmp
	echo 'NULL};' >> $@

bugs.c: $(top_srcdir)/BUGS Makefile
	echo '/* Generated from BUGS file */' > $@
	echo '/*! \file $@' >> $@
	echo '    \brief Generated at build time bugs list.' >> $@
	echo '    \ingroup gerbv' >> $@
	echo ' */' >> $@
	num=`wc -l $(top_srcdir)/BUGS | awk '{print $$1 + 1}'` && \
		echo "const gchar * bugs_string_array[$$num] = {" >> $@
	${TXT2CL} $(top_srcdir)/BUGS >> $@
	echo 'NULL};' >> $@

CLEANFILES=	authors.c bugs.c

## authors.c and bugs.c are both built sources, however they are a bit problematic
## because of i18n.  Certain built targets will try to update the po files but those
## rules may not cause authors.c and bugs.c to be built.  Work around this by
## putting them in the distfile.  They shouldn't change anyway.

EXTRA_DIST=	authors.c bugs.c gerbv.rc libgerbv.pc.in run_gerbv.in

SCMSUBDIR = scheme
scmdatadir = $(pkgdatadir)/${SCMSUBDIR}/
dist_scmdata_DATA = init.scm

AM_CFLAGS= -DBACKEND_DIR='"$(scmdatadir)"' -DSCMSUBDIR='"$(SCMSUBDIR)"' $(CODE_COVERAGE_CFLAGS)
AM_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)

all-local: run_gerbv
# create wrapper script that lets you test gerbv prior to installation
run_gerbv:     $(srcdir)/run_gerbv.in Makefile
	sed \
		-e 's;@SRCDIR@;${abs_srcdir};g' \
		-e 's;@BUILDDIR@;${abs_builddir};g' \
		$(srcdir)/run_gerbv.in > $@
	chmod 755 $@

localedir = @datadir@/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

DISTCLEANFILES = run_gerbv

MOSTLYCLEANFILES = *~
MAINTAINERCLEANFILES = *~ *.o Makefile Makefile.in authors.c bugs.c
