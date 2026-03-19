# How to build Gerbv

Gerbv now uses CMake with *presets*, uses the *Ninja Multi-Config* generator and also *toolchains*.
*Install* and *CPack* is also used extensively to be able to create easy to use multiplatform support.

It gives slightly different build operations, but should be much easier. Despite the abundance of
CMake information on the Net, I write this down. Also what presets there are in Gerbv and how
they work together.

A short introduction about some of the philosophy of how the presets, toolchain etc comes together in
Gerbv is from [Harald Achitz presentation at SwedenCPP](https://www.youtube.com/watch?v=3VJPfwn1f2o)
(15 min worth of your time).

All CMake configuration (except the usual `CMakePresets.json` and `CMakeLists.txt`) are stored in the `cmake` directory.
* `cmake/preset` for the presets
* `cmake/toolchains` for the toolchains
* `cmake/Packing.cmake` for the `CPack` configuration (packages deb/rpm/etc)

The `Packing.cmake` is a bit messy at the time of writing this. A split of this should be on the agenda.

There is also the the possibility to use a local `CMakeUserPresets.json` you can use for your own preferences
if you need one. Please note that that file is in the `.gitignore` and will thus never be merged into the
repository. This is per CMake recommendations.

## Quick summary

Building locally on a Linux/GCC based machine to test/run locally:
```
rm -rf build
cmake --preset linux-gnu-gcc
cmake --build --preset linux-gnu-gcc
```
Binary is now in `build/src/Debug/gerbv`

Building a Debian package to install using `dpkg -i`
```
rm -rf build
cmake --preset linux-gnu-gcc-install
cmake --build --preset linux-gnu-gcc-release
cpack --preset deb-opt
sudo dpkg -i _packages/gerbv<Something>.deb
```

## Presets and toolchains

Presets basically describes configuration, build and test recipes. Instead of entering long lines of `-D` parameters,
build directories, source directories and whats not, everything is prepared for the most common scenarios.
The presets file also contains references to a toolchain for the particular preset.

Toolchains are used to describe the quirks of your compiler and platform. Extra flags, configurations etc.

### Presets

#### General usage of the presets

To see available presets, you can list them with `--list-presets`

For configure presets
```
cmake --list-presets
```
For build presets
```
cmake --build --list-presets
```
For test presets
```
ctest --list-presets
```
For package presets
```
cpack --list-presets
```

**Note** In many shells you can use tab completion to get the full name of a parameter.

#### Configuration presets in Gerbv

Doing `cmake --list-presets` in Gerbv gives (at the time of writing)
```
$ cmake --list-presets
Available configure presets:

  "linux-gnu-gcc"
  "linux-gnu-gcc-install"
  "macos-clang"
  "mingw-w64-gcc"
  "msys2-ucrt64-gcc"
```
Here are the different platforms listed. The difference between `linux-gnu-gcc` and `linux-gnu-gcc-install` is
that CMake requires you to set the installation directory already in the configuration stage. So `linux-gnu-gcc-install`
sets the install directory to `/opt/gerbv.github/`, since that is a requirement for thirdparty packages in Debian.

**Note** We don't say anything about `Debug` or `Release` build here, since we are using the `Ninja Multi-Config` generator.

After configuration you should have a `build` directory, and in that there should be a `compile_commands.json` that
describes all flags that will be used when compiling. There should also be the generated `build/config/config.h`, which is
the generated configuration file used throughout the project. Those two files can be used to check that all the
compilation flags and defines have been set properly.

The `build/config/config.h` is generated from `config/config.h.in` by the `config/CMakeLists.txt`.

After configuration `build/src` should have three directories
* `build/src/Debug/`
* `build/src/Release/`
* `build/src/RelWithDebInfo/`

They are empty after the configuration stage, but they will be used in the following build stage where we will tell
what kind of build we want.

#### Build presets in Gerbv

Doing `cmake --build --list-presets` after the previous configuration stage gives (at the time of writing)
```
$ cmake --build --list-presets
Available build presets:

  "linux-gnu-gcc"
  "linux-gnu-gcc-release"
  "macos-clang"
  "macos-clang-release"
  "mingw-w64-gcc"
  "mingw-w64-gcc-release"
  "msys2-ucrt64-gcc"
  "msys2-ucrt64-gcc-release"
```
Here we define if we should do a `Debug` or a `Release` build. Default is, as you probably understand, `Debug`.
The build `RelWithDebInfo` is not used by us at the moment.

#### Test presets in Gerbv

The test implemented in Gerbv is a pixel-by-pixel comparison of generated PNG with Golden versions to
see if anything changed. Basically only used in the pipeline.

```
$ ctest --list-presets
Available test presets:

  "linux-gnu-gcc"
  "macos-clang"
  "msys2-ucrt64-gcc"
```

#### Install preset

For install there is no preset, it is not even defined in CMake.
Simply do
```
cmake --install build
```
and it will install the `Release` version onto your machine. You probably need to prepend `sudo` if you are
going to install Gerbv globally on your machine.

#### Packaging presets

Doing `cpack --list-presets` after building a release above gives (at the time of writing)

```
$ cpack --list-presets
Available package presets:

  "deb"
  "deb-opt"
  "rpm"
  "rpm-opt"
  "mingw-w64-gcc"
```

The simple difference between the `-opt` and non `-opt` is that the `-opt` will build a package that is
installed in `/opt/gerbv.github`. The package will be built in `_packages`.

Preset `mingw-w64-gcc` uses NSIS to create an install shield installer. It is rather new and needs some
testing. If the NSIS fails to install there is always the native Windows artifact that just zips together
needed files.

### Toolchains

Toolchain files are a way to separate platform dependencies. It is very common in the embedded world where
you for instance do a lot of cross compiling. We do some cross compiling as well, since one of the Windows
platforms is cross compiled on a Linux machine using Fedora.

It is a way to keep the compiler versions, special flags etc out of your `CMakeLists.txt`. The `CMakeLists.txt`
should only describe how you build your source code, not special hacks to keep your compiler happy.

### Build options

There are some parameters that can be changed during **configuration** of the build. They are defined in the
`BuildOptions.cmake` file and they are currently:
* `DEBUG_PRINTOUT` Print out debug messages as gerbv processes files (default FALSE)
* `GERBV_DEFAULT_BORDER_COEFF` Default border coefficient for export (default 0.05)
* `GERBV_DEFAULT_UNIT` Default unit to display in statusbar Possible values are `GERBV_MILS`, `GERBV_MMS` or `GERBV_INS` (default `GERBV_MILS`)

#### DEBUG_PRINTOUT  Print out debug messages as gerbv process files

Prints out every operation that happens in the code, see `DPRINT`.
Enable by `-DDEBUG_PRINTOUT=TRUE`. Currently not working properly. Update `DEBUG` in `config/config.h.in`
to get debug printouts and rerun configuration. Developer specific flag.

#### GERBV_DEFAULT_BORDER_COEFF Default border coefficient for export

Set by `-DGERBV_DEFAULT_BORDER_COEFF=<value>` where <value> is a floating point value.

#### GERBV_DEFAULT_UNIT Default unit to display in statusbar

To change to millimeters for example, add `-DGERBV_DEFAULT_UNIT=GERBV_MILS` to CMake configuration.
Possible values are `GERBV_MILS`, `GERBV_MMS` or `GERBV_INS`

## Packages needed

To build this there are a couple of packages needed to be installed on your computer.

### Debian

You can always see a list of the latest packages used to build in the pipeline at the
[pipeline setup](.mc/gerbv/.mc/debian_13/debian_13.yaml)

```
install:
 - gcc
 - cmake
 - ninja-build

 # Buildsystem dependencies
 - pkg-config

 # Runtime dependencies
 - libgtk2.0-dev
 - libcairo2-dev

 # Test dependencies
 - imagemagick

 # Packaging dependencies
 - git
```

### Ubuntu

You can always see a list of the latest packages used to build in the pipeline at the
[pipeline setup](.mc/gerbv/.mc/ubuntu_22.04/ubuntu_22.04.yaml)

Since we are using Ubuntu 22.04 that does not contain the relevant version of CMake,
we need to download and install it from KitWare (the makers of CMake)

```
install:
 - gcc
 - ninja-build

 # Buildsystem dependencies
 - pkg-config
 - wget
 - software-properties-common
 - gpg

 # Runtime dependencies
 - libgtk2.0-dev
 - libcairo2-dev

 # Test dependencies
 - imagemagick

  # Packaging dependencies
 - git
---
#!/bin/bash
set -e

# Install CMake 3.28+ from Kitware's official repository
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository -y 'deb https://apt.kitware.com/ubuntu/ jammy main'
sudo apt-get update
sudo apt-get install -y cmake
```

### Fedora

You can always see a list of the latest packages used to build in the pipeline at the
[pipeline setup](.mc/gerbv/.mc/fedora_43/fedora_43.yaml)

```
install:
 - gcc
 - gcc-c++
 - cmake
 - ninja-build

 # Buildsystem dependencies
 - gettext-devel
 - pkgconf

 # Runtime dependencies
 - cairo-devel
 - gtk2-devel

 # Test dependencies
 - ImageMagick

 # Packaging dependencies
 - git
 - rpm-build
```

### Windows (cross compiled)

You can always see a list of the latest packages used to build in the pipeline at the
[pipeline setup](.mc/gerbv/.mc/windows_amd64/windows_amd64.yaml)

It is cross compiled on a Fedora 43. It seems that Fedora have the most ready made
cross compilation libraries for Windows.

```
install:
 - mingw64-gcc
 - mingw64-gcc-c++
 - cmake
 - ninja-build

 # Buildsystem dependencies
 - gettext-devel
 - pkgconf

 # Runtime dependencies
 - mingw64-cairo-static
 - mingw64-gtk2-static

 # Test dependencies
 - ImageMagick

 # Packaging dependencies
 - git
 - zip
```

### Windows (native compile)

For Windows native, [the packages are defined in the pipeline description directly](https://github.com/gerbv/gerbv/blob/develop/.github/workflows/ci.yaml#L192)

### MacOS

For MacOS, [the packages are defined in the pipeline description directly](https://github.com/gerbv/gerbv/blob/develop/.github/workflows/ci.yaml#L243)

Using `brew` you need the following packages

```
brew install cmake ninja pkgconf gettext gtk+ gtkmm libffi zlib bzip2 libpng expat imagemagick
```

## IDEs

Many modern IDEs have now support for CMake and CMakePresets that simplifies life.

### VSCode

Except the usual C/C++ extensions there is a *CMake Tools* extension. So I personally have the following C/C++ extensions,
all from Microsoft.
* **C/C++**
* **C/C++ DevTools**
* **C/C++ Extension Pack**
* **C/C++ Themes**

The CMake tool is simply called **CMake Tools**, which I also have installed.

On the menu to the left you get a triangle (CMake symbol) with a small wrench. From there you can configure your CMake setup.

In the `.vscode/settings.json` I recommend to set the following
```
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.dimInactiveRegions": true,
    "cmake.languageSupport.dotnetPath": "/bin/dotnet",
}
```
Not sure about the `dotnetPath`, but the first line is the interesting part. But VSCode probably automatically discovers
that this is a CMake project and configures itself accordingly.
