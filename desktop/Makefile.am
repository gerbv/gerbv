## Process this file with automake to produce Makefile.in
##
##    Copyright (C) 2000-2006 Stefan Petersen (spe at stacken.kth.se)
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

theme=hicolor
xdgdatadir=@XDGDATADIR@
themedir=$(xdgdatadir)/icons/$(theme)

app_icon = gerbv

app_icon_files = \
	$(app_icon:%=%-16.png) \
	$(app_icon:%=%-22.png) \
	$(app_icon:%=%-24.png) \
	$(app_icon:%=%-32.png) \
	$(app_icon:%=%-48.png) \
	$(app_icon:%=%.svg)

theme_icons = \
	$(app_icon_files:%=apps,%)

desktopdir=$(xdgdatadir)/applications
dist_desktop_DATA = gerbv.desktop

install_icon_exec = \
	$(SETENV) GTK_UPDATE_ICON_CACHE_BIN="@GTK_UPDATE_ICON_CACHE_BIN@" \
	$(SHELL) $(top_srcdir)/icon-theme-installer \
	-t $(theme) \
	-s $(srcdir) \
	-d "x$(DESTDIR)" \
	-b $(themedir) \
	-m "$(mkinstalldirs)" \
	-x "$(INSTALL_DATA)"

install-data-local:
	$(install_icon_exec) -i $(theme_icons)

install-data-hook:
if ENABLE_UPDATE_DESKTOP_DATABASE
	$(UPDATE_DESKTOP_DATABASE) "$(DESTDIR)$(desktopdir)"
endif

uninstall-hook:
if ENABLE_UPDATE_DESKTOP_DATABASE
	$(UPDATE_DESKTOP_DATABASE) "$(DESTDIR)$(desktopdir)"
endif
	$(install_icon_exec) -u $(theme_icons)

dist_pkgdata_DATA= gerbv_icon.ico

EXTRA_DIST = gerbv.png $(app_icon_files)
MOSTLYCLEANFILES = *~
MAINTAINERCLEANFILES = *~ *.o Makefile Makefile.in
