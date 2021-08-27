#!/bin/bash
#
# Helper script to unify build logic between different operating systems. Can be
# executed like this from the main directory:
#
#     mc fedora:34 .mc/rebuild.sh
#     mc ubuntu:20.04 .mc/rebuild.sh
set -e

sh autogen.sh
/opt/configure
make clean
make

