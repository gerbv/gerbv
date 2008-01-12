## Process this file with automake to produce Makefile.in
##
##    Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
##
##  This file is part of gerbv.
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

EXTRA_DIST = gerbv.1.in

man_MANS = gerbv.1

MOSTLYCLEANFILES = *~
CLEANFILES = *~ gerbv.1

gerbv.1: gerbv.1.in
	sed -e "s;@scmdir@;${pkgdatadir}/scheme;g" \
		-e 's;@version@;@VERSION@;g' \
		$(srcdir)/gerbv.1.in > $@
