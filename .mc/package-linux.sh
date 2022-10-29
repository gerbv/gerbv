#!/bin/bash
#
# Builds an archive containing a gerbv binary distribution for Linux based
# systems
#
# @param $1 System name e.g. 'Fedora 36'
#
# @warning Expects working directory to be set to project root


# Validate arguments
RELEASE_OS="${1}"

if [ "${RELEASE_OS}" == "" ]; then
	(>&2 echo "Usage: package-linux.sh <release-os>")
	exit 1
fi


# Validate environment
CAT=`command -v cat`
CHMOD=`command -v chmod`
CP=`command -v cp`
DATE=`command -v date`
FIND=`command -v find`
GIT=`command -v git`
GZIP=`command -v gzip`
MKTEMP=`command -v mktemp`
TAR=`command -v tar`

if [ ! -x "${CAT}" ]; then
	(>&2 echo "\`cat' missing")
	exit 1
fi

if [ ! -x "${CHMOD}" ]; then
	(>&2 echo "\`chmod' missing")
	exit 1
fi

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

if [ ! -x "${GZIP}" ]; then
	(>&2 echo "\`gzip' missing")
	exit 1
fi

if [ ! -x "${MKTEMP}" ]; then
	(>&2 echo "\`mktemp' missing")
	exit 1
fi

if [ ! -x "${TAR}" ]; then
	(>&2 echo "\`tar' missing")
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
"${CP}" 'src/.libs/gerbv' "${TEMPORARY_DIRECTORY}"
"${FIND}" 'src/.libs' -name 'libgerbv.so*' -exec "${CP}" {} "${TEMPORARY_DIRECTORY}" \;

"${CAT}" <<EOT >> "${TEMPORARY_DIRECTORY}/gerbv.sh"
#!/bin/bash

DIRECTORY=\`dirname "\$0"\`
LD_LIBRARY_PATH="\${LD_LIBRARY_PATH}:\${DIRECTORY}" "\${DIRECTORY}/gerbv" "\$@"
EOT

"${CHMOD}" +x "${TEMPORARY_DIRECTORY}/gerbv.sh"


# Create archive and auxiliary files
RELEASE_FILENAME="gerbv_${RELEASE_DATE}_${RELEASE_COMMIT_SHORT}_(${RELEASE_OS}).tar.gz"

"${TAR}" --directory="${TEMPORARY_DIRECTORY}" -czf "${WEBSITE_DIRECTORY}/${RELEASE_FILENAME}" '.'

echo "${RELEASE_COMMIT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT"
echo "${RELEASE_COMMIT_SHORT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT_SHORT"
echo "${RELEASE_DATE}"		> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_DATE"
echo "${RELEASE_FILENAME}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_FILENAME"

