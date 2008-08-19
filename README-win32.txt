$Id$

 
Yes, gerbv runs on windows too!  To build from source, you need cygwin
and also the mingw compilers.  It builds as a native WIN32 application.
You may also use mingw instead of cygwin to build gerbv.

The configure script is set up to assume that you are using the native
win32 gtk libraries as opposed to the cygwin ones.  One method which
seems to work for building from source is:

- Go to http://www.gtk.org and follow links to gtk for win32.

- Download all of the runtime, developer, and source .zip, .tar.gz, and
  .bz2  files for glib, atk, pango, gtk+, gettext, libiconv, pkg-config,
  and the others there.  Save all of these files to ~/gtk_win32_downloads.

- In cygwin, 

  cd /path/to/gerbv/sources

  and run

  ./win32/extract_gtk_win32.

  This will extract all of the runtime files you need to
  ~/gtk_win32_runtime and the developer files to ~/gtk_win32.


- In cygwin,

  ./win32/build_gerbv


  This script is a wrapper on top of the normal configure script
  and has all of the correct options to get gerbv to build under
  cygwin/mingw.

  ./win32/build_gerbv --help

  will give a complete list of options for build_gerbv


If you have the normal cygwin and cygwin for X gtk libraries installed
you will have problems.  It is related to libtool changing things like
-L/path/to/nativewin32gtk -lgtk to /usr/lib/libgtk-2.0.a.  Watch when
gerbv.exe in src/ is actually linked and you'll see it happen.
An ugly work around is to just modify the Makefile to not use libtool
for linking or to run the link command by hand.  But that is ugly.
Anyone with a real fix?  I worked around this by not installing the
X gtk libraries on my cygwin installation.

Binary Version:
---------------
The installer was created using NSIS (http://nsis.sourceforge.net).

The gerbv.nsi file in the win32 directory was used to build the
installer.  Note, gerbv.nsi is actually created from gerbv.nsi.in
by build_gerbv in the win32 directory.

