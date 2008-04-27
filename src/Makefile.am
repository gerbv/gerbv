## Process this file with automake to produce Makefile.in
##
##  gEDA - GNU Electronic Design Automation
##  This file is a part of gerbv.
##
##    Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
##
##  $Id$
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

bin_PROGRAMS = gerbv

gerbv_SOURCES = amacro.c amacro.h \
		common.h tooltable.c \
		${DRAW} drill.c drill.h \
		exportimage.c \
		gerb_file.c gerb_file.h gerb_image.c gerb_image.h gerber.c \
		gerber.h gerbv.c gerbv.h gerbv_icon.h \
		pick-and-place.c pick-and-place.h gerb_transf.c gerb_transf.h \
		csv.c csv.h csv_defines.h \
		gerb_stats.c gerb_stats.h \
		drill_stats.c drill_stats.h export-rs274x.c \
		export-drill.c \
		batch.c batch.h \
		opdefines.h \
		dynload.c dynload.h \
		attribute.c attribute.h \
		main.h main.c \
		project.c project.h scheme-private.h scheme.c \
		scheme.h \
		callbacks.c callbacks.h interface.c \
		interface.h render.c render.h \
		icons.h

# If we are building on win32, then compile in some icons for the
# desktop and application toolbar
if WIN32
gerbv_icon.o : gerbv_icon.ico $(srcdir)/gerbv.rc
	windres $(srcdir)/gerbv.rc $@

gerbv_icon.ico: $(top_srcdir)/desktop/gerbv_icon.ico
	cp $(top_srcdir)/desktop/gerbv_icon.ico $@

gerbv_LDADD=		gerbv_icon.o
gerbv_DEPENDENCIES=	gerbv_icon.o

endif

CAIRO_DRAW=	draw.c draw.h
GDK_DRAW=	draw-gdk.c draw-gdk.h

if USE_CAIRO
DRAW=		${CAIRO_DRAW} ${GDK_DRAW}
else
DRAW=		${GDK_DRAW}
endif

EXTRA_DIST=	${CAIRO_DRAW} ${GDK_DRAW} gerbv.rc

scmdatadir = $(pkgdatadir)/scheme/
dist_scmdata_DATA = init.scm

AM_CFLAGS= -DBACKEND_DIR='"$(scmdatadir)"'

MOSTLYCLEANFILES = *~
MAINTAINERCLEANFILES = *~ *.o Makefile Makefile.in
