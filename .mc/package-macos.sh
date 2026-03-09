#!/bin/bash
#
# Builds an archive containing a gerbv binary distribution for macOS systems
#
# Uses otool to iteratively discover dylib dependencies from Homebrew,
# filtering out macOS system libraries.
#
# @warning Expects working directory to be set to project root


# Validate arguments
RELEASE_OS="macOS"


# Validate environment
CP=`command -v cp`
FIND=`command -v find`
GIT=`command -v git`
OTOOL=`command -v otool`
ZIP=`command -v zip`

if [ ! -x "${CP}" ]; then
	(>&2 echo "\`cp' missing")
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

if [ ! -x "${OTOOL}" ]; then
	(>&2 echo "\`otool' missing")
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
RELEASE_DATE=`date +%Y-%m-%d`


# Copy files to be released into temporary directory
WEBSITE_DIRECTORY='gerbv.github.io/ci'
TEMPORARY_DIRECTORY=`mktemp -d`

"${CP}" 'COPYING' "${TEMPORARY_DIRECTORY}"
"${CP}" 'thirdparty/tinyscheme/COPYING' "${TEMPORARY_DIRECTORY}/COPYING.tinyscheme"
"${CP}" 'thirdparty/tinyscheme/init.scm' "${TEMPORARY_DIRECTORY}"
"${CP}" 'build-macos-clang/src/Debug/gerbv' "${TEMPORARY_DIRECTORY}"
"${FIND}" 'build-macos-clang/src/Debug' -name 'libgerbv*.dylib' -exec "${CP}" {} "${TEMPORARY_DIRECTORY}" \;

# Iteratively discover dylib dependencies using otool, filtering out macOS
# system libraries. otool -L outputs lines like:
#
#     /opt/homebrew/lib/libgtk-x11-2.0.0.dylib (compatibility ...)
#     /usr/lib/libSystem.B.dylib (compatibility ...)
#
# We keep only non-system libraries and resolve them recursively until no new
# dependencies are found. A marker file communicates across the pipe subshell.
while true; do
	for bin in "${TEMPORARY_DIRECTORY}"/*; do
		[ -f "${bin}" ] || continue
		"${OTOOL}" -L "${bin}" 2>/dev/null | tail -n +2 | awk '{print $1}' | while read -r dep; do
			case "${dep}" in
				/usr/lib/*|/System/*|@rpath/*|@executable_path/*|@loader_path/*) continue ;;
			esac
			dep_name=`basename "${dep}"`
			if [ ! -f "${TEMPORARY_DIRECTORY}/${dep_name}" ] && [ -f "${dep}" ]; then
				"${CP}" "${dep}" "${TEMPORARY_DIRECTORY}/"
				touch "${TEMPORARY_DIRECTORY}/.found_new"
			fi
		done
	done
	if [ -f "${TEMPORARY_DIRECTORY}/.found_new" ]; then
		rm "${TEMPORARY_DIRECTORY}/.found_new"
	else
		break
	fi
done


# Create archive and auxiliary files
mkdir -p "${WEBSITE_DIRECTORY}"
RELEASE_FILENAME="gerbv_${RELEASE_DATE}_${RELEASE_COMMIT_SHORT}_(${RELEASE_OS}).zip"

"${FIND}" "${TEMPORARY_DIRECTORY}" -type f -exec "${ZIP}" --junk-paths "${WEBSITE_DIRECTORY}/${RELEASE_FILENAME}" {} \;

echo "${RELEASE_COMMIT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT"
echo "${RELEASE_COMMIT_SHORT}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT_SHORT"
echo "${RELEASE_DATE}"		> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_DATE"
echo "${RELEASE_FILENAME}"	> "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_FILENAME"
