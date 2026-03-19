#!/bin/bash
# Generates index.html from index.html.in by substituting metadata placeholders
# with values read from ci/ metadata files.
set -e

sed \
    -e "s|@FEDORA_43_DATE@|$(tr -d '\n' < 'ci/Fedora 43.RELEASE_DATE')|g" \
    -e "s|@FEDORA_43_COMMIT@|$(tr -d '\n' < 'ci/Fedora 43.RELEASE_COMMIT')|g" \
    -e "s|@FEDORA_43_COMMIT_SHORT@|$(tr -d '\n' < 'ci/Fedora 43.RELEASE_COMMIT_SHORT')|g" \
    -e "s|@FEDORA_43_FILENAME@|$(tr -d '\n' < 'ci/Fedora 43.RELEASE_FILENAME')|g" \
    -e "s|@UBUNTU_2204_DATE@|$(tr -d '\n' < 'ci/Ubuntu 22.04.RELEASE_DATE')|g" \
    -e "s|@UBUNTU_2204_COMMIT@|$(tr -d '\n' < 'ci/Ubuntu 22.04.RELEASE_COMMIT')|g" \
    -e "s|@UBUNTU_2204_COMMIT_SHORT@|$(tr -d '\n' < 'ci/Ubuntu 22.04.RELEASE_COMMIT_SHORT')|g" \
    -e "s|@UBUNTU_2204_FILENAME@|$(tr -d '\n' < 'ci/Ubuntu 22.04.RELEASE_FILENAME')|g" \
    -e "s|@DEBIAN_13_DATE@|$(tr -d '\n' < 'ci/Debian 13.RELEASE_DATE')|g" \
    -e "s|@DEBIAN_13_COMMIT@|$(tr -d '\n' < 'ci/Debian 13.RELEASE_COMMIT')|g" \
    -e "s|@DEBIAN_13_COMMIT_SHORT@|$(tr -d '\n' < 'ci/Debian 13.RELEASE_COMMIT_SHORT')|g" \
    -e "s|@DEBIAN_13_FILENAME@|$(tr -d '\n' < 'ci/Debian 13.RELEASE_FILENAME')|g" \
    -e "s|@WINDOWS_AMD64_DATE@|$(tr -d '\n' < 'ci/Windows amd64.RELEASE_DATE')|g" \
    -e "s|@WINDOWS_AMD64_COMMIT@|$(tr -d '\n' < 'ci/Windows amd64.RELEASE_COMMIT')|g" \
    -e "s|@WINDOWS_AMD64_COMMIT_SHORT@|$(tr -d '\n' < 'ci/Windows amd64.RELEASE_COMMIT_SHORT')|g" \
    -e "s|@WINDOWS_AMD64_FILENAME@|$(tr -d '\n' < 'ci/Windows amd64.RELEASE_FILENAME')|g" \
    index.html.in > index.html
