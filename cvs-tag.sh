#!/bin/sh

# Tags files according to FILES2TAG. Entries with # in the beginning will
# not be tagged.

# (C) 2002, Stefan Petersen (spe@stacken.kth.se)
# $Id$

FILES2TAG=files2tag.txt
CVS=cvs
TAG=$1

for file2tag in `cat ${FILES2TAG}` ; do
    if  test -z "`echo ${file2tag} | mawk '$0 ~ /^#/'`"  ; then
    ${CVS} tag -l ${TAG} ${file2tag}
    fi
done
