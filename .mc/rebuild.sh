#!/bin/bash
#
# Helper script to unify build logic between different operating systems. Can be
# executed like this from the main directory:
#
#     mc fedora:36 .mc/rebuild.sh
#     mc ubuntu:22.04 .mc/rebuild.sh
set -e

sh autogen.sh
/opt/configure.sh
make clean
make
/opt/package.sh

