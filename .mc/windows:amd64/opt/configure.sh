#!/bin/bash

set -e

mingw64-configure				\
	--enable-debug				\
	CFLAGS="-g -O0" CCFLAGS="-g -O0"	\
	--disable-update-desktop-database
