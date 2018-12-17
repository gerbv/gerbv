This file describes how to build from git sources.  If you are building from a
released version or snapshot, please refer to the 'INSTALL' document instead.
Take the time to read this file, it's not that long and it addresses some 
questions which come up with some frequency.

-------------
Prerequisites
-------------

You will need the following tools to obtain and build a git version of gerbv:

To download and track sources you will need:

	git

In addition you will need recent versions of:

autoconf  -- ftp://ftp.gnu.org/pub/gnu/autoconf/
             Version 2.59 or newer.

automake  -- ftp://ftp.gnu.org/pub/gnu/automake/
             The developers use the 1.9.* versions of automake.  Older versions
             have not been as well tested (or tested at all).  Versions 1.7 and
             older are too old and will not work.

libtool   -- ftp://ftp.gnu.org/pub/gnu/libtool/
             Version 1.4 and newer should work although most development is done
	     with 1.5.x.

You can find the version of autoconf, automake, and makeinfo by running them
with the --version flag.

These are in addition to the normal tools and libraries required to build a
release version of gerbv.

---------
Check out
---------

If you already have a checked out gerbv source tree, please proceed to the
'Updating' section.

To check out sources from the git server, run the following:

	git clone ssh://git@geda-project.org:65/gerbv

---------
Updating
---------

To update an already checked out copy of the gerbv source tree, run this
command from your gerbv directory:

	git pull

----------------------------------
Bootstrapping with the auto* tools
----------------------------------

To create the configure script and the Makefile.in's you will need to run:

	./autogen.sh

from the top level pcb directory.  This will run the various auto* tools
which creates the configure script, the config.h.in file and the various
Makefile.in's.

If you plan on making changes to configure.ac or Makefile.am's you may want to
enable maintainer mode by passing the

	--enable-maintainer mode flag to ./configure

At this point you can proceed to configure and build gerbv as outlined in
the INSTALL document.

-------------------
Building a Snapshot
-------------------

The file README-release.txt documents what is currently done to create a
release for gerbv.  Most of what is in there has to do with the git repository
and how we tag and branch the sources.  If you are interested in building your
own .tar.gz file from git sources, you can still use the information in
README-release.txt as a guide.

