# Gerbv – a Gerber file viewer ![Build Status](https://github.com/gerbv/gerbv/actions/workflows/ci.yaml/badge.svg)[![Coverage Status](https://coveralls.io/repos/github/gerbv/gerbv/badge.svg?branch=main)](https://coveralls.io/github/gerbv/gerbv?branch=main)

Gerbv was originally developed as part of the
[gEDA Project](https://www.geda-project.org/) but is now separately maintained.


## Download

Official releases are published on [GitHub Releases][download-official].
Moreover, CI generated binaries are published on [gerbv.github.io][download-ci].
Be aware however that they are not manually verified!

[download-official]: https://github.com/gerbv/gerbv/releases
[download-ci]: https://gerbv.github.io/#download


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

* [Patch #77: Fix double-freeing memory](https://sourceforge.net/p/gerbv/patches/77/),
  applied in [PR#24](https://github.com/gerbv/gerbv/pull/24) as
  [Commit `a96b46`](https://github.com/gerbv/gerbv/commit/a96b46c7249e97e950d860790b84bcdba2368f57)
* [Patch #81: Fix casting pointer to different size integer](https://sourceforge.net/p/gerbv/patches/81/),
  applied in [PR#23](https://github.com/gerbv/gerbv/pull/23) as
  [Commit `e4b344`](https://github.com/gerbv/gerbv/commit/e4b344e182191296d48b392f56f3bdd48900e1fc)
* [Patch #83: Crash may occur on opening/saveing files](https://sourceforge.net/p/gerbv/patches/83/),
  applied in [PR#8](https://github.com/gerbv/gerbv/pull/8) as
  [Commit `242dda`](https://github.com/gerbv/gerbv/commit/242dda66b81e88f17f4ef99840cfeff727753b19)


##  Supported platforms

Gerbv has been built and tested on

* Debian 10 (amd64)
* Fedora 36 (amd64)
* Ubuntu 22.04 (amd64)
* Windows 10 (amd64 cross compiled from Fedora as well as native x86/amd64 using MSYS2)


## Information for developers

Gerbv is split into a core functional library and a GUI portion. Developers
wishing to incorporate Gerber parsing/editing/exporting/rendering into other
programs are welcome to use libgerbv. Complete API documentation for libgerbv
is [here](https://gerbv.github.io/doc/), as well as many example programs using libgerbv.

<details>
  <summary>Click for Example 1</summary>
   <p>Description: Loads example1-input.gbx into a project, and then exports the layer back to another RS274X file 
   </p>
   <p><a href="https://gerbv.github.io/doc/example1_8c-example.html">code example</a></p>
</details>

<details>
  <summary>Click for Example 2</summary>
   <p>Description: Loads example2-input.gbx, duplicates it and offsets it to the right by the width of the layer, merges the two images, and exports the merged image
    back to another RS274X file. Note: this example code uses the gerbv_image 
     </p>
   <p><a href="https://gerbv.github.io/doc/example2_8c-example.html" >code example </a></p>  
</details>
 
<details>
  <summary>Click for Example 3</summary>
    <p>Description: Loads example3-input.gbx, duplicates it and offsets it to the right by the width of the layer, changed the rendered color of the 
      second image, then exports a PNG rendering of the overlaid images. 
    </p>
    <p><a href="https://gerbv.github.io/doc/example3_8c-example.html" >code example </a></p>
</details>
  
<details>
  <summary>Click for Example 4</summary>
    <p>Description: Loads example4-input.gbx, searches through the file and removes any entities with a width less than 60mils, and re-exports 
    the modified image to a new RS274X file. 
    </p>
    <p><a href="https://gerbv.github.io/doc/example4_8c-example.html">code example</a></p>
</details>
    
<details>
  <summary>Click for Example 5</summary>
    <p>Description: Demonstrate the basic drawing functions available in libgerbv 
    by drawing a smiley face and exporting the layer to a new RS274X file. 
    </p>
    <p><a href="https://gerbv.github.io/doc/example5_8c-example.html" >code example</a></p>
</details>
      
<details>
  <summary>Click for Example 6</summary>
  <p>Description: Demonstrate how to embed a libgerbv render window into a new 
    application to create a custom viewer 
    </p>
  <p><a href="https://gerbv.github.io/doc/example6_8c-example.html">code example</a></p>
</details>


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

