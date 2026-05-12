#!/bin/bash
#
# Builds an archive containing a gerbv binary distribution for Windows MSYS2
# UCRT64 systems
#
# Uses ldd to discover runtime DLL dependencies from the UCRT64 environment,
# rather than copying all DLLs (which would include hundreds of unnecessary
# packages).
#
# @warning Expects working directory to be set to project root


# Validate arguments
RELEASE_OS="Windows MSYS2 UCRT64"


# Validate environment
CP=`command -v cp`
DATE=`command -v date`
FIND=`command -v find`
GIT=`command -v git`
LDD=`command -v ldd`
MKDIR=`command -v mkdir`
MKTEMP=`command -v mktemp`
ZIP=`command -v zip`

if [ ! -x "${CP}" ]; then
	(>&2 echo "\`cp' missing")
	exit 1
fi

if [ ! -x "${DATE}" ]; then
	(>&2 echo "\`date' missing")
	exit 1
fi

if [ ! -x "${FIND}" ]; then
	(>&2 echo "\`find' missing")
	exit 1
fi

if [ ! -x "${GIT}" ]; then
	(>&2 echo "\`git' missing")
	exit 1
fi

if [ ! -x "${LDD}" ]; then
	(>&2 echo "\`ldd' missing")
	exit 1
fi

if [ ! -x "${MKDIR}" ]; then
	(>&2 echo "\`mkdir' missing")
	exit 1
fi

if [ ! -x "${MKTEMP}" ]; then
	(>&2 echo "\`mktemp' missing")
	exit 1
fi

if [ ! -x "${ZIP}" ]; then
	(>&2 echo "\`zip' missing")
	exit 1
fi


# Gather information about current build
set -e

RELEASE_COMMIT=`"${GIT}" rev-parse HEAD`
RELEASE_COMMIT_SHORT="${RELEASE_COMMIT:0:6}"
RELEASE_DATE=`"${DATE}" --rfc-3339=date`


# Copy files to be released into temporary directory
WEBSITE_DIRECTORY='gerbv.github.io/ci'
TEMPORARY_DIRECTORY=`"${MKTEMP}" --directory`

"${CP}" 'COPYING' "${TEMPORARY_DIRECTORY}"
"${CP}" 'thirdparty/tinyscheme/COPYING' "${TEMPORARY_DIRECTORY}/COPYING.tinyscheme"
"${CP}" 'thirdparty/tinyscheme/init.scm' "${TEMPORARY_DIRECTORY}"
"${CP}" 'build/src/Debug/gerbv.exe' "${TEMPORARY_DIRECTORY}"
"${FIND}" 'build/src/Debug' -name 'libgerbv*.dll' -exec "${CP}" {} "${TEMPORARY_DIRECTORY}" \;

# Discover runtime DLL dependencies using ldd, filtering out Windows system
# DLLs. ldd in MSYS2 outputs lines like:
#
#     libglib-2.0-0.dll => /ucrt64/bin/libglib-2.0-0.dll (0x7ff...)
#     KERNEL32.dll => /c/Windows/System32/KERNEL32.dll (0x7ff...)
#
# We keep only DLLs from /ucrt64/ (the MSYS2 UCRT64 runtime libraries).
BINARIES="${TEMPORARY_DIRECTORY}/gerbv.exe"
for dll in "${TEMPORARY_DIRECTORY}"/libgerbv*.dll; do
	[ -f "${dll}" ] && BINARIES="${BINARIES} ${dll}"
done

"${LDD}" ${BINARIES} \
	| grep -i '=> /ucrt64/' \
	| awk '{print $3}' \
	| sort -u \
	| while read -r dll_path; do
		"${CP}" "${dll_path}" "${TEMPORARY_DIRECTORY}"
	done


# Create archive and auxiliary files
"${MKDIR}" -p "${WEBSITE_DIRECTORY}"
RELEASE_FILENAME="gerbv_${RELEASE_DATE}_${RELEASE_COMMIT_SHORT}_(${RELEASE_OS}).zip"

"${FIND}" "${TEMPORARY_DIRECTORY}" -type f -exec "${ZIP}" --junk-paths "${WEBSITE_DIRECTORY}/${RELEASE_FILENAME}" {} \;

echo "${RELEASE_COMMIT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT"
echo "${RELEASE_COMMIT_SHORT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT_SHORT"
echo "${RELEASE_DATE}"		> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_DATE"
echo "${RELEASE_FILENAME}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_FILENAME"
