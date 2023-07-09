#!/bin/bash
#
# Builds an archive containing a gerbv binary distribution for Windows systems
#
# @warning Expects working directory to be set to project root


# Validate arguments
RELEASE_OS="Windows amd64"

if [ "${RELEASE_OS}" == "" ]; then
	(>&2 echo "Usage: package-linux.sh <release-os>")
	exit 1
fi


# Validate environment
CP=`command -v cp`
DATE=`command -v date`
FIND=`command -v find`
GIT=`command -v git`
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
"${CP}" 'NEWS' "${TEMPORARY_DIRECTORY}"
"${CP}" 'src/init.scm' "${TEMPORARY_DIRECTORY}"
"${CP}" 'src/.libs/gerbv.exe' "${TEMPORARY_DIRECTORY}"
"${FIND}" 'src/.libs' -name 'libgerbv-*.dll' -exec "${CP}" {} "${TEMPORARY_DIRECTORY}" \;

# @warning While this might copy more libraries than strictly necessary, it
#     won't be many more, since the development environment contains only the
#     required mingw dependencies
MINGW_LIBRARY_DIRECTORY='/usr/x86_64-w64-mingw32/sys-root/mingw/bin'

"${FIND}" "${MINGW_LIBRARY_DIRECTORY}" -type f -name '*.dll' -exec "${CP}" {} "${TEMPORARY_DIRECTORY}" \;


# Create archive and auxiliary files
RELEASE_FILENAME="gerbv_${RELEASE_DATE}_${RELEASE_COMMIT_SHORT}_(${RELEASE_OS}).zip"

"${FIND}" "${TEMPORARY_DIRECTORY}" -type f -exec "${ZIP}" --junk-paths "${WEBSITE_DIRECTORY}/${RELEASE_FILENAME}" {} \;

echo "${RELEASE_COMMIT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT"
echo "${RELEASE_COMMIT_SHORT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT_SHORT"
echo "${RELEASE_DATE}"		> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_DATE"
echo "${RELEASE_FILENAME}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_FILENAME"

