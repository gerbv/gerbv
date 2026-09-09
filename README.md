# Gerbv – a Gerber file viewer ![Build Status](https://github.com/gerbv/gerbv/actions/workflows/ci.yaml/badge.svg)[![Coverage Status](https://coveralls.io/repos/github/gerbv/gerbv/badge.svg?branch=develop)](https://coveralls.io/github/gerbv/gerbv?branch=develop)

Gerbv was originally developed as part of the
[gEDA Project](https://www.geda-project.org/) but is now separately maintained.


## Download

Official releases are published on [GitHub Releases][download-official].
Moreover, CI generated binaries are published on [gerbv.github.io][download-ci].

Be aware however that they are not manually verified!

### Current status on packages:

The [download-ci] page is not updated properly at the moment, but latest build
can be downloaded straight from the pipeline at [download-actions].

[download-official]: https://github.com/gerbv/gerbv/releases
[download-ci]: https://gerbv.github.io/#download
[download-actions]: https://github.com/gerbv/gerbv/actions

## Build

See [BUILD.md](https://github.com/gerbv/gerbv/blob/develop/BUILD.md) for how to compile
it using CMake with presets.

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

This is basically a fork of the fork. There have been many chefs trying to fix this soup,
but they have never gotten to the bottom of the problems. I (the original creator and
maintainer of the gerbv project) have taken over this branch. I gave up the role of
maintainer many years ago due to me becoming a consultant with my own business and
didn't feel I had time for this.

Since then a lot of things in the software world have revolutionized the way software is
developed. From as simple things as Git and github, up to todays AI coding tools. The compilers
have become much better at finding problems and errors (you have to enable flags) and also
open source tools for static analysis is available.

I have plans on what to do. Some plans may upset people. If this upsets you I welcome you
to create your own fork, I see no problems with that.

I have started the `develop` branch to back out of some completely and utterly useless code
formatting changes. There are more serious problems than reformatting the code to some style
that is not used anywhere in the code. I admit the code style is broken, but it is really
not the first on the agenda to fix. The original `main` branch will stay as it is for the time
being.

## My plan for this set of updates

This is a list of things I hope to be able to fix

- [ ] [CMake](#cmake)
  - [X] Fix Linux/general CMake target
  - [X] Fix Windows CMake by cross compilation using Mingw64
  - [X] Remove files that are not used anymore or just ancient
  - [X] Make Github pipeline compile the new targets
  - [ ] Fix outstanding tests/targets that are in use on the Github pipeline
    - [ ] Rebuild and deploy website properly (missing variables at the moment)
    - [ ] Enable unit tests
    - [ ] Enable Valgrind (maybe)
    - [ ] Enable Coverage (maybe)
- [X] [Fixing first set of trivial bugs](#fixing-first-set-of-trivial-bugs)
- [X] [Packing](#packing)
  - [X] deb
  - [X] rpm
  - [X] Windows NSIS
- [ ] [More updated Gerber specifications](#more-updated-gerber-specifications)
- [ ] [Port over to GTK-3.0](#port-over-to-gtk-30)
- [ ] [Fixing misunderstandings of the original specification](#fixing-misunderstandings-of-the-original-specification)
- [ ] [General documentation](#general-documentation)
- [ ] [Code reformatting](#code-reformatting)

### CMake

First step was to switch to modern CMake that uses presets and toolchains. There have been kinks
and surprising stuff, but most things are now in place.

We are now in the CI always compiling and building packages for Debian, Ubuntu, Windows (both cross compiled
and using MSYS2) and MacOS.

There is now a description on [how to compile this project using CMake et al](BUILD.md) that I recommend you to read.

### Fixing first set of trivial bugs

When I was porting the code to CMake I found a bunch of trivial but serious errors. So I fixed
them. Upgrading the compiler version have also revealed a number of compilation errors. Now the code
compiles cleanly on fairly modern compilers.

### Packing

After introducing CMake we are now able to package Debian, Ubuntu and RPMs.

For Windows builds we should use the NSIS toolchain, that is WIP to use NSIS. They are currently zipped
together, but downloading it, unzipping and executing it seems to work.


### More updated Gerber specifications

When I started this project the "specification" at the time was around 60 pages. Now it is
over 200 pages and most things are more clearly defined. And there are updates with new things
as well. Hopefully at least try to parse without warnings on missing syntax.

There is (at least was) a lot of broken Gerber files out there. Just look in the examples directory.
The question is if it still is so, and if we should support every little quirk, and error or omission.

I have gotten a lot of help in trying to work out all the new Gerbers. It is not all yet, but there are
a big bunch of PRs still working on to merge in.

### Port over to GTK-3.0

GTK-2.0 is obsoleted since some year(s) back. Already GTK-2.0 have developed since gerbv went there
and obsoleted a couple of things that are still used in gerbv. That makes it even harder to
update to GTK-3.0.

It is not trivial, a lot of things have changed. There is an excellent patch waiting from @LemonBoy
which I have high hopes for. And with a little help from Claude I hope we can pull this together.

The biggest change for gerbv is probably that GDK is removed. That was our fastest drawing frontend.
But with faster computers and graphics it hopefully works out OK. If anyone feels adventurous,
a frontend using OpenGL is always welcome.

### Fixing misunderstandings of the original specification

I am not a graphics guy, I always liked the parsing part more. So from the tiny "standards" paper
to the full-blown standards paper of today there are a bunch of things that has been misunderstood
or not even implemented.

### General documentation

There are today lots of tools to document your code. One great thing I have discovered by switching
to a more modern IDE is to document the API of all functions using Doxygen.

This is more of a continuous task. For every file touched some documentation can be updated.

### Code reformatting

Of course a code reformatting will be done. This will be a stepwise update, one file at a time.
One of the problems with a complete reformatting is the loss of history. You need the history when
coming into a project first time to try to understand why something is like it is.

This goes slightly hand in hand with the [General documentation](#general-documentation) updates.
Step-wise and when the file is updated for some other reason.

### Many other fixes

As the project goes on in rewriting, more and more trivial and not-so-trivial bugs are surfaced.
Those will be fixed asap when they are discovered. There can be platforms developers don't have
access to, that might delay things. But the urgency will be based on severity.
* Compilation error/warnings. Code should always compile.
* Crashes. Segfaults and similar memory crashes. They can be hard to recreate though.
* Other annoyances.

## Building (after CMake transition)

See a more in depth in [BUILD.md](https://github.com/gerbv/gerbv/blob/develop/BUILD.md).

### For general Linux distributions

You need:
* gcc
* gtk+-2.0 (use dev package)
* glib-2.0 (use dev package)
* cmake (>=3.28)
* ninja

To configure the build process, run
```
cmake --preset linux-gnu-gcc
```
That will create a directory called `build` with all configuration.

To compile you can choose `debug` build (default) or `release` build and then run
```
cmake --build --preset linux-gnu-gcc
```
or
```
cmake --build --preset linux-gnu-gcc-release
```

There is already an installation process available. You can test it out until I get the
energy to write it down properly. It is quite easy if you want to install it under `/usr/local`.
Basically you run
```
cmake --install build
```

If you want to install it somewhere else, then YMMV.

### Mingw64 cross compilation

**Status**: *compiles*

For creating Windows binaries, the Fedora distribution is used. It provides all the development libraries
that is needed for Mingw64 cross compilation, especially GTK2.0+ and Cairo.

Compilation has been tested on Fedora 43 with the following libraries installed:
* `mingw64-cairo-static`
* `mingw64-gtk2.static`
* `cmake`
* `ninja`
* `mingw64-gcc`
* `mingw64-gcc-c++`
* `gettext`

As the binaries are not tested at the moment it might not work or crash horribly. But I would
appreciate any report.

The preset is called `mingw-w64-gcc` and the toolchain file is located in `cmake/toolchains/mingw-w64-gcc.cmake`.

To configure you do
```
cmake --preset mingw-w64-gcc
```
and to compile you do
```
cmake --build --preset mingw-w64-gcc
```

### Windows 7 support

We are currently building and testing on Windows 10 (when available). Windows 7 is currently unsupported beyond
[release 2.8.0](https://github.com/gerbv/gerbv/releases/tag/v2.8.0). 

There was a report that [Windows 7 starts up and locks immediately with newer versions of Gerbv](https://github.com/gerbv/gerbv/issues/457).
The first cause seemed to point to [incompatible versions of Glib](https://github.com/gerbv/gerbv/issues/457#issuecomment-4111947105).

A later test the cause was determined to be [registry reading and writing](https://github.com/gerbv/gerbv/issues/457#issuecomment-4227492574).
A patch was made, but did not seem to work as intended, see https://github.com/gerbv/gerbv/pull/469.

Current status is that Gerbv does not support Windows 7 beyond [release 2.8.0](https://github.com/gerbv/gerbv/releases/tag/v2.8.0).

The Windows version actually installs and runs under Wine if it is any comfort. But then it should be better to install a Linux
version directly.

### Other cross compilations

To create another cross compilation target, check the directory `cmake/toolchains/`.

To create another preset, check out the directory `cmake/preset/` and add the new preset file as an include in `CMakePreset.json`.

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

* Debian 13 (amd64)
* Fedora 43 (amd64)
* Ubuntu 22.04 (amd64)
* Windows 10 (amd64 cross compiled from Fedora as well as native x86/amd64 using MSYS2)

### Fedora 43 upgrade

There is a build for both Fedora 43 and for Windows using Fedora 43 (previously 38). Both builds
previously suffered from missing DXF support libraries. This has been resolved by bundling
dxflib as a vendored dependency in `thirdparty/dxflib/`, removing the reliance on
distribution-packaged DXF libraries.

### Why not Ubuntu 24.04?

There is a problem switching to Ubuntu 24.04, which should be the better choice. The Docker image
for Ubuntu 24.04 defines a user with UID/GID 1000, the toolchain used to do the cross compilation
(see https://github.com/ooxi/mini-cross) tries to define a new user with same UID/GID and thus fails.

Using the mini-cross builder is a problem in itself, but that issue is for another day.

The problem might only be when running the application locally, but it is hard to develop and test
without being able to run locally. When installing Ubuntu, the first user created is always UID/GID
1000. 

This problem has been deferred at the moment, running on Ubuntu 22.04 should be good enough for the
time being.

## Information for developers

Gerbv is split into a core functional library and a GUI portion. Developers
wishing to incorporate Gerber parsing/editing/exporting/rendering into other
programs are welcome to use libgerbv. Complete API documentation for libgerbv
is [here](https://gerbv.github.io/doc/), as well as many example programs using libgerbv.

See [`doc/example-code/README.md`](doc/example-code/README.md) for a guide to
building against libgerbv on Linux, macOS, MSYS2, and Visual Studio 2022.

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

If you are aware of a security issue, we recommend full public disclosure as a
GitHub issue. This way our users are warned and can act accordingly while we
work on providing a mitigation.


## License

Gerbv and all associated files is placed under the GNU Public License (GPL)
version 2.0.  See the toplevel [COPYING](COPYING) file for more information.

Gerbv bundles [TinyScheme](https://sourceforge.net/projects/tinyscheme/)
1.35, which is licensed under the BSD 3-Clause License. See
[thirdparty/tinyscheme/COPYING](thirdparty/tinyscheme/COPYING) for details.

Gerbv bundles [dxflib](https://qcad.org/en/90-dxflib), which is licensed
under the GNU General Public License (GPL) version 2.0 or later. See
[thirdparty/dxflib/gpl-2.0greater.txt](thirdparty/dxflib/gpl-2.0greater.txt)
for details.

Programs and associated files are:
Copyright 2001, 2002 by Stefan Petersen and the respective original authors
(which are listed on the respective files)

