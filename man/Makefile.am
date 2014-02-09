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

if HAVE_PO4A
man_MANS = gerbv.1 gerbv.ru.1
else
man_MANS = gerbv.1

all:
	@ echo
	@ echo "WARNING: missing po4a or sp (nsgmls). Rerun configure and" \
		"check for 'checking for po4a-*' and 'checking for nsgmls'"
	@ echo
endif

EXTRA_DIST=	gerbv.1.in

PO_FILES = gerbv.ru.1.in.po

MOSTLYCLEANFILES = *~
CLEANFILES = *~  *.pot $(man_MANS)

SUFFIXES = .1 .1.in .in .in.po

# .1 << .1.in
.1.in.1:
	sed -e "s;@scmdir@;${pkgdatadir}/scheme;g" \
		-e 's;@version@;@VERSION@;g' \
		$< > $@ || rm $@

# .in << .in.po
.in.po.in:
	$(po4a_translate) -k 70 -M UTF-8 -f man -m gerbv.1.in -p $< -l $@

$(PO_FILES): gerbv.1.in.pot
	$(MSGMERGE) --width=76 $@ $< -o $@.new
	mv $@.new $@

gerbv.1.in.pot: gerbv.1.in
	$(po4a_gettextize) -M UTF-8 -f man -m $< -p $@


update-po:
	@ touch -c gerbv.1.in.pot
	$(MAKE) $(PO_FILES)
