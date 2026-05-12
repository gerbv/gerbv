#!/bin/bash
#
# Builds an NSIS installer for Windows systems using CPack
#
# @warning Expects working directory to be set to project root


RELEASE_OS="Windows amd64"

set -e

DATE=$(command -v date)
GIT=$(command -v git)

RELEASE_COMMIT=$("${GIT}" rev-parse HEAD)
RELEASE_COMMIT_SHORT="${RELEASE_COMMIT:0:6}"
RELEASE_DATE=$("${DATE}" --rfc-3339=date)

WEBSITE_DIRECTORY='gerbv.github.io/ci'

# Generate NSIS installer via CPack (output goes to _packages/)
cpack --preset mingw-w64-gcc

# Find the generated installer
RELEASE_FILENAME=$(ls _packages/*.exe | xargs -n1 basename | head -1)

# Copy to website directory
cp "_packages/${RELEASE_FILENAME}" "${WEBSITE_DIRECTORY}/${RELEASE_FILENAME}"

echo "${RELEASE_COMMIT}"       > "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT"
echo "${RELEASE_COMMIT_SHORT}" > "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_COMMIT_SHORT"
echo "${RELEASE_DATE}"         > "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_DATE"
echo "${RELEASE_FILENAME}"     > "${WEBSITE_DIRECTORY}/${RELEASE_OS}.RELEASE_FILENAME"
