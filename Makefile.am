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

SUBDIRS = intl src po man doc example desktop scheme test win32

EXTRA_DIST = BUGS CONTRIBUTORS HACKING AUTHORS ChangeLog COPYING INSTALL \
	NEWS README \
	TODO \
	autogen.sh  README-cvs.txt README-release.txt \
	README-win32.txt icon-theme-installer BUGS

DISTCHECK_CONFIGURE_FLAGS = --disable-update-desktop-database GTK_UPDATE_ICON_THEME_BIN=true
