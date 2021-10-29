# Gerbv – a Gerber file viewer

Gerbv was originally developed as part of the
[gEDA Project](https://www.geda-project.org/) but is now seperatly maintained.


## About Gerbv

* Gerbv is a viewer for Gerber RS-274X files, Excellon drill files, and CSV
  pick-and-place files.  (Note:  RS-274D files are not supported.)
* Gerbv is a native Linux application, and it runs on many common Unix
  platforms.
* Gerbv is free/open-source software.
* The core functionality of Gerbv is located in a separate library (libgerbv),
  allowing developers to include Gerber parsing/editing/exporting/rendering into
  other programs.
* Gerbv is one of the utilities originally affiliated with the gEDA project, an
  umbrella organization dedicated to producing free software tools for
  electronic design.


## About this fork

While Gerbv is great software, the development on Source Force has stalled since
many years with patches accumulating in the tracker and mailing list.

This fork aims at providing a maintained Gerbv source, containing mostly
bugfixes.

To communicate with the original Gerbv developers, please post your query on the
following mailing list:

    gerbv-devel@lists.sourceforge.net
    geda-user@delorie.com

This is a friendly fork and I'm willing to invite other people to join the Gerbv
GitHub organization.


## Applied patches from SourceForge

* [Patch #83: Crash may occur on opening/saveing files](https://sourceforge.net/p/gerbv/patches/83/),
  applied in [PR#8](https://github.com/gerbv/gerbv/pull/8) as
  [Commit `242dda`](https://github.com/gerbv/gerbv/commit/242dda66b81e88f17f4ef99840cfeff727753b19)


##  Supported platforms

Gerbv has been built and tested on

* Debian 10 (amd64)
* Fedora 34 (amd64)
* Ubuntu 20.04 (amd64)
* Windows 10 (amd64 cross compiled from Fedora as well as native x86/amd64 using MSYS2)


## Information for developers

Gerbv is split into a core functional library and a GUI portion. Developers
wishing to incorporate Gerber parsing/editing/exporting/rendering into other
programs are welcome to use libgerbv. Complete API documentation for libgerbv
is here, as well as many example programs using libgerbv.


## Security

The current focus of gerbv is to provide a utility to view and manipulate
trusted gerber files. When using gerbv, you should not view files from untrusted
sources without extra precautions.

Nevertheless, we acknowledge that libgerbv will be used to handle untrusted
input, maybe even provided over the network. In those cases we strongly advise
to treat libgerbv as any codec and isolate its operations from the rest of your
application.


## License

Gerbv and all associated files is placed under the GNU Public License (GPL)
version 2.0.  See the toplevel [COPYING](COPYING) file for more information.

Programs and associated files are:
Copyright 2001, 2002 by Stefan Petersen and the respective original authors
(which are listed on the respective files)

