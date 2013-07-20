dnl configuration script for gerbv
dnl
dnl  gEDA - GNU Electronic Design Automation
dnl  This file is part of gerbv.
dnl
dnl    Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
dnl
dnl  $Id$
dnl
dnl  This program is free software; you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation; either version 2 of the License, or
dnl  (at your option) any later version.
dnl
dnl  This program is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with this program; if not, write to the Free Software
dnl  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA


AC_INIT([gerbv], [2.6.1])
AC_CONFIG_SRCDIR([src/gerbv.c])
AC_GNU_SOURCE
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([1.9])

dnl Create a configuration header 
AM_CONFIG_HEADER([config.h])

dnl Initialize maintainer mode
AM_MAINTAINER_MODE

dnl Internationalisation
AM_NLS
AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION([0.16])
#AX_DESKTOP_I18N

############################################################
#
# Checks for cygwin/mingw32
#

AC_CANONICAL_HOST


# if you want -mno-cygwin, then add it to MINGW_CFLAGS or CYGWIN_CFLAGS
# in your configure environment.  After all you may or may not
# want to always force -mno-cygwin on all users.
AC_MSG_CHECKING([for windows])
WIN32=${WIN32:-no}
case $host_os in
	*cygwin*  )
		CFLAGS="$CFLAGS ${CYGWIN_CFLAGS}"
                CPPFLAGS="$CPPFLAGS ${CYGWIN_CPPFLAGS}"
		;;

	*mingw32* )
		WIN32=yes
		CFLAGS="$CFLAGS ${MINGW_CFLAGS:--mms-bitfields -mwindows}"
		CPPFLAGS="$CPPFLAGS ${MINGW_CPPFLAGS:--mms-bitfields -mwindows}"
		;;
esac

AC_MSG_RESULT([$WIN32])
AC_SUBST(WIN32)
AM_CONDITIONAL(WIN32, test x$WIN32 = xyes)

GERBV_PATH_DELIMETER=":"
GERBV_DIR_SEPARATOR_S="/"
GERBV_DIR_SEPARATOR_C='/'
if test "x$WIN32" = "xyes" ; then
   GERBV_PATH_DELIMETER=";"
   GERBV_DIR_SEPARATOR_S="\\\\"
   GERBV_DIR_SEPARATOR_C='\\'
fi

AC_DEFINE_UNQUOTED(GERBV_DIR_SEPARATOR_C,'$GERBV_DIR_SEPARATOR_C',[Directory separator char])
AC_DEFINE_UNQUOTED(GERBV_DIR_SEPARATOR_S,"$GERBV_DIR_SEPARATOR_S",[Directory separator string])
AC_DEFINE_UNQUOTED(GERBV_PATH_DELIMETER,"$_PATH_DELIMETER",[Search path separator string])


#
#
############################################################

############################################################
# 
# Checks for our configure args
#

dnl --enable-debug
debug=0
AC_ARG_ENABLE(debug,
    [  --enable-debug          Enable fairly verbose debug output],
    [
        if test $enableval = "yes"; then
		debug=1
	fi
    ],
    [
	debug=0
    ])
AC_DEFINE_UNQUOTED(DEBUG, $debug, [Define to 1 to enable debugging code])

dnl --enable-unit-mm : Set default unit for coordinates in status bar to mm
AC_ARG_ENABLE(unit-mm,
    [  --enable-unit-mm        Set default unit for coordinates in status bar to mm],
    [
        if test "$enableval" = "yes"; then
		default_unit="mm"
	fi
    ])
if test "$default_unit" = "mm"; then
	AC_DEFINE(GERBV_DEFAULT_UNIT, GERBV_MMS, [Default unit to display in statusbar])
else
	AC_DEFINE(GERBV_DEFAULT_UNIT, GERBV_MILS, [Default unit to display in statusbar])
fi

#
#
############################################################


############################################################
#
# Preliminary checks
#

dnl Build time sanity check... (?)
AM_SANITY_CHECK

dnl Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_LIBTOOL

if test "x$WIN32" = "xyes" ; then
	AC_CHECK_TOOL(WINDRES, windres, no)
	if test "$WINDRES" = "no"; then
		AC_MSG_ERROR([*** Could not find an implementation of windres in your PATH.])
	fi
fi

# if we have gcc then add -Wall
if test "x$GCC" = "xyes"; then
	if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
		CFLAGS="$CFLAGS -Wall"
	fi
fi

# Check for ImageMagick tools used by the testsuite
AC_PATH_PROG(IM_ANIMATE, animate, notfound)
AC_PATH_PROG(IM_COMPARE, compare, notfound)
AC_PATH_PROG(IM_COMPOSITE, composite, notfound)
AC_PATH_PROG(IM_CONVERT, convert, notfound)
AC_PATH_PROG(IM_DISPLAY, display, notfound)
AC_PATH_PROG(IM_MONTAGE, montage, notfound)
missing_magick=""
test "${IM_ANIMATE}" != "notfound" || missing_magick="${missing_magick} animate"
test "${IM_COMPARE}" != "notfound" || missing_magick="${missing_magick} compare"
test "${IM_COMPOSITE}" != "notfound" || missing_magick="${missing_magick} composite"
test "${IM_CONVERT}" != "notfound" || missing_magick="${missing_magick} convert"
test "${IM_DISPLAY}" != "notfound" || missing_magick="${missing_magick} display"
test "${IM_MONTAGE}" != "notfound" || missing_magick="${missing_magick} montage"

AC_MSG_CHECKING([if all ImageMagick tools needed for the testsuite were found])
if test "X${missing_magick}" != "X" ; then
	AC_MSG_RESULT([no.  The testsuite will be disabled because the following
tools from the ImageMagick suite were not found:
${missing_magick}
No loss in gerbv functionality should be experienced, you just will not
be able to run the regression testsuite.
])
	have_magick=no
else
	AC_MSG_RESULT([yes])
	have_magick=yes
fi
AM_CONDITIONAL(HAVE_MAGICK, test x$have_magick = xyes)

# Check for pkg-config
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test "$PKG_CONFIG" = "no"; then
	AC_MSG_ERROR([Cannot find pkg-config, make sure it is installed and in your PATH])
fi      

#
#
#
############################################################

############################################################
#
# Library checks
#

AC_CHECK_LIB(m, sin)

# used by src/dynload.c (part of tinyscheme)
AC_CHECK_LIB(dl, dlopen)

#
#
############################################################

############################################################
#
# GTK and cairo checks
#


PKG_CHECK_MODULES(CAIRO, cairo >= 1.2.0, , [AC_MSG_ERROR([
*** Cairo > 1.2.0 required but not found. ***
Please review the following errors:
$CAIRO_PKG_ERRORS])]
)

CAIRO_VER=`$PKG_CONFIG cairo --modversion`

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.4.0, , [AC_MSG_ERROR([
*** GTK >= 2.4.0 is required but was not found.  Please review
the following errors:
$GTK_PKG_ERRORS])]
)

GTK_VER=`$PKG_CONFIG gtk+-2.0 --modversion`

#
#
############################################################


############################################################
#
# ElectricFence
with_efence=no
AC_MSG_CHECKING([if ElectricFence debugging should be enabled])
AC_ARG_ENABLE([efence], 
[  --enable-efence         Link with ElectricFence for malloc debugging [default=no]],
[
if test "X$enable_efence" != "Xno" ; then
	AC_MSG_RESULT([yes])
	AC_CHECK_LIB(efence,main,,
	AC_MSG_ERROR([You have requested ElectricFence debugging but -lefence could not be found]))
		with_efence=yes
else    
	AC_MSG_RESULT([no])
fi      
],
[
AC_MSG_RESULT([no])
])

#
#
############################################################

######################################################################
#
# desktop integration
#

AC_PATH_PROG(SETENV, env, [])
AC_PATH_PROG(GTK_UPDATE_ICON_CACHE_BIN, gtk-update-icon-path, [true])

# Change default location for XDG files (MIME and Icons)
AC_ARG_WITH(xdgdatadir, [  --with-xdgdatadir=path  Change where the theme icons 
and mime registrations are installed [[DATADIR]]], [opt_xdgdatadir=$withval])

if test x$opt_xdgdatadir = x; then
	# path was not specified with --with-xdgdatadir
	XDGDATADIR='${datadir}'
else
	# path WAS specified with --with-xdgdatadir
	XDGDATADIR="$opt_xdgdatadir"
fi
AC_SUBST(XDGDATADIR)

AC_ARG_ENABLE(update-desktop-database,
	AC_HELP_STRING([--disable-update-desktop-database],
	[do not update desktop database after installation]),,
	enable_update_desktop_database=yes)

AM_CONDITIONAL(ENABLE_UPDATE_DESKTOP_DATABASE, test x$enable_update_desktop_database = xyes)

if test x$enable_update_desktop_database = xyes ; then
	AC_PATH_PROG(UPDATE_DESKTOP_DATABASE, [update-desktop-database], no)
	if test $UPDATE_DESKTOP_DATABASE = no; then
	AC_MSG_ERROR([Cannot find update-desktop-database, make sure it is installed and in your PATH, or configure with --disable-update-desktop-database])
	fi
fi

#
######################################################################

AC_CHECK_HEADERS(unistd.h getopt.h string.h sys/mman.h sys/types.h sys/stat.h stdlib.h regex.h libgen.h time.h)

AC_CHECK_FUNCS(getopt_long)
AC_CHECK_FUNCS(strlwr)

# for lrealpath.c
AC_CHECK_FUNCS(realpath canonicalize_file_name)
libiberty_NEED_DECLARATION(canonicalize_file_name)


CFLAGS="$CFLAGS $GDK_PIXBUF_CFLAGS $GTK_CFLAGS $CAIRO_CFLAGS"
LIBS="$LIBS $GDK_PIXBUF_LIBS $GTK_LIBS $CAIRO_LIBS"


############################################################
#
# scheme configure stuff
#

AC_DEFINE([STANDALONE], [0],[Scheme interpreter not used standalone])
AC_DEFINE([USE_DL],[1],[Dynamic linking in Scheme interpreter])
AC_DEFINE([SUN_DL],[1],[DL Sun method])
AC_DEFINE([USE_MATH],[1],[Math in Scheme interpreter])
AC_DEFINE([USE_ASCII_NAMES],[1],[ASCII names in Scheme interpreter])
AC_DEFINE([USE_COLON_HOOKS],[1],[Colon Hooks in Scheme interpreter])
AC_DEFINE([USE_STRING_HOOKS],[1],[String Hooks in Scheme interpreter])
AC_DEFINE([USE_INTERFACE],[1],[Use extension interface of Scheme interpreter])

#
#
############################################################

############################################################
#
# Figure out relative paths
#

# standard autoconf variables
CPPFLAGS="$CPPFLAGS -DPREFIXDIR=\\\"\${prefix}\\\""
CPPFLAGS="$CPPFLAGS -DBINDIR=\\\"\${bindir}\\\""

# these relative paths will be used to help locate init.scm
# in the event that the installation directory is relocated.

AC_MSG_CHECKING([for the bindir to datadir relative path])
adl_COMPUTE_RELATIVE_PATHS([bindir:datadir:bindir_to_datadir])
adl_NORMALIZE_PATH([bindir_to_datadir], [$GERBV_DIR_SEPARATOR_S])
AC_MSG_RESULT([$bindir_to_datadir])
AC_DEFINE_UNQUOTED(BINDIR_TO_DATADIR, "$bindir_to_datadir", [Relative path from bindir to datadir])

PKGDATADIR=${datadir}/${PACKAGE}
AC_MSG_CHECKING([for the bindir to pkgdatadir relative path])
adl_COMPUTE_RELATIVE_PATHS([bindir:PKGDATADIR:bindir_to_pkgdatadir])
adl_NORMALIZE_PATH([bindir_to_pkgdatadir], [$GERBV_DIR_SEPARATOR_S])
AC_MSG_RESULT([$bindir_to_pkgdatadir])
AC_DEFINE_UNQUOTED(BINDIR_TO_PKGDATADIR, "$bindir_to_pkgdatadir", [Relative path from bindir to pkgdatadir])

AC_MSG_CHECKING([for the bindir to exec_prefix relative path])
adl_COMPUTE_RELATIVE_PATHS([bindir:exec_prefix:bindir_to_execprefix])
adl_NORMALIZE_PATH([bindir_to_execprefix], [$GERBV_DIR_SEPARATOR_S])
AC_MSG_RESULT([$bindir_to_execprefix])
AC_DEFINE_UNQUOTED(BINDIR_TO_EXECPREFIX, "$bindir_to_execprefix", [Relative path from bindir to exec_prefix])


#
#
############################################################

AC_OUTPUT(	Makefile \
		intl/Makefile \
		desktop/Makefile \
		doc/PNG-print/Makefile \
		doc/eagle/Makefile \
		doc/html/Makefile \
		doc/example-code/Makefile \
		doc/Makefile \
		example/eaglecad1/Makefile \
		example/nollezappare/Makefile \
		example/numpres/Makefile \
		example/jj/Makefile \
		example/dan/Makefile \
		example/ekf2/Makefile \
		example/exposure/Makefile \
		example/am-test/Makefile \
		example/cslk/Makefile \
		example/orcad/Makefile \
		example/Mentor-BoardStation/Makefile \
		example/pick-and-place/Makefile \
		example/polarity/Makefile \
		example/thermal/Makefile \
		example/trailing/Makefile \
		example/Makefile \
		man/Makefile \
		po/Makefile.in \
		scheme/Makefile \
		src/Makefile \
		src/libgerbv.pc \
		win32/Makefile \
		test/Makefile \
		test/golden/Makefile \
		test/inputs/Makefile )
        
expandedXDGDATADIR=`eval "echo $XDGDATADIR"`

AC_MSG_RESULT([
** Configuration summary for $PACKAGE $VERSION:

   PREFIX:                   $PREFIX
   xdg data directory:       $expandedXDGDATADIR

   CPPFLAGS:                 $CPPFLAGS
   CFLAGS:                   $CFLAGS
   LDFLAGS:                  $LDFLAGS
   LIBS:                     $LIBS

   GTK Version:              $GTK_VER
   Cairo Version:            $CAIRO_VER

   Electric Fence Debugging: $with_efence

])

