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

gerbv_SOURCES = amacro.c amacro.h batch.c batch.h color.c color.h common.h \
		${DRAW} drill.c drill.h dynload.c \
		dynload.h exportimage.c exportimage.h gerb_aperture.h gerb_error.h \
		gerb_file.c gerb_file.h gerb_image.c gerb_image.h gerber.c \
		gerber.h gerbv.c gerbv_icon.h gerbv_screen.h log.c log.h \
		opdefines.h project.c project.h scheme-private.h scheme.c \
		scheme.h setup.c setup.h tooltable.c tooltable.h \
		pick-and-place.c pick-and-place.h gerb_transf.c gerb_transf.h \
		csv.c csv.h csv_defines.h callbacks.c callbacks.h interface.c \
		interface.h render.c render.h gerb_stats.c gerb_stats.h \
		drill_stats.c drill_stats.h icons.h

CAIRO_DRAW=	draw.c draw.h
GDK_DRAW=	draw-gdk.c draw-gdk.h

if USE_CAIRO
DRAW=		${CAIRO_DRAW} ${GDK_DRAW}
else
DRAW=		${GDK_DRAW}
endif

EXTRA_DIST=	${CAIRO_DRAW} ${GDK_DRAW}

scmdatadir = $(datarootdir)/gerbv/scheme/
dist_scmdata_DATA = init.scm

AM_CFLAGS= -DBACKEND_DIR='"$(scmdatadir)"'

MOSTLYCLEANFILES = *~
