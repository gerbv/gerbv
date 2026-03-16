#!/bin/bash
#
# Helper script to unify build logic between different operating systems. Can be
# executed like this from the main directory:
#
#     mc fedora_43 .mc/rebuild.sh
#     mc ubuntu_22.04 .mc/rebuild.sh
set -e

rm -rf build
/opt/configure.sh
cmake --build --preset mingw-w64-gcc
cmake --build --preset mingw-w64-gcc-release
/opt/package.sh

