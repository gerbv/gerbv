## Process this file with automake to produce Makefile.in
##
##  gEDA - GNU Electronic Design Automation
##  This file is a part of gerbv.
##
##    Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

gerbv_SOURCES = gerb_image.c gerb_file.c gerber.c scm_gerber.c gerbv.c \
		draw.c drill.c color.c 

INCLUDES = @GTK_CFLAGS@ @GUILE_CFLAGS@
gerbv_LDADD = @GTK_LIBS@ @GUILE_LDFLAGS@


MOSTLYCLEANFILES = *~ *.o gerbv
CLEANFILES = *~ *.o gerbv
DISTCLEANFILES = *~ *.o gerbv Makefile
MAINTAINERCLEANFILES = *~ *.o gerbv Makefile
