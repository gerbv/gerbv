#!/bin/sh

# Tags files according to FILES2TAG. Entries with # in the beginning will
# not be tagged.

# (C) 2002-2003, Stefan Petersen (spe@stacken.kth.se)
# $Id$

FILES2TAG=files2tag.txt
CVS=cvs
TAG=$1

if  test -z ${TAG} ; then
    echo Give tag name
    exit 1;
fi

for file2tag in `cat ${FILES2TAG}` ; do
    if  test -z "`echo ${file2tag} | mawk '$0 ~ /^#/'`"  ; then
    echo TAGGING ${file2tag}
    ${CVS} tag ${TAG} ${file2tag}
    fi
done
