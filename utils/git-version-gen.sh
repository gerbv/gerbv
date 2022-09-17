#!/bin/bash
#
# Generates gerbv version string from a given prefix and the current commit
# short id
#
# @warning Version will only be updated when reconfiguring! Will have no effect
#     on incremental builds
#
# @param $1 Version prefix

set -e


# Validate arguments
PREFIX="${1}"

if [ "" == "${PREFIX}" ]; then
	(>&2 echo "Usage: git-version-gen.sh <prefix>")
	exit 1
fi


# Validate environment
GIT=`command -v git`

if [ ! -x "${GIT}" ]; then
	(>&2 echo "\`git' missing")
	echo -n "${PREFIX}"
	exit 0
fi

if ! ${GIT} rev-parse --is-inside-work-tree >& /dev/null ; then
	echo -n "${PREFIX}"
	exit 0
fi

# Get commit short id
RELEASE_COMMIT=`"${GIT}" rev-parse HEAD`
RELEASE_COMMIT_SHORT="${RELEASE_COMMIT:0:6}"


# Output final version
echo -n "${PREFIX}~${RELEASE_COMMIT_SHORT}"

