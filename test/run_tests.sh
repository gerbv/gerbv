#!/bin/sh
#
# $Id$
#
# Copyright (c) 2003, 2004, 2005, 2006, 2007 Dan McMahill

#  This program is free software; you can redistribute it and/or modify
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
# All rights reserved.
#
# This code was derived from code written by Dan McMahill as part of the
# latex-mk testsuite.  The original code was covered by a BSD license
# but the copyright holder is releasing the version for gerbv under the GPL.

magic_test_skip=${GERBV_MAGIC_TEST_SKIP:-no}

if test "x${magic_test_skip}" = "xyes" ; then
	cat << EOF

The environment variable GERBV_MAGIC_TEST_SKIP is set to yes.
This causes the testsuite to skip all tests and report no errors.
This is used for certain debugging *only*.  The primary use is to 
allow testing of the 'distcheck' target without including the effects
of the testsuite. The reason this is useful is that due to minor differences
in cairo versions and perhaps roundoff in different CPU's, the testsuite
may falsely report failures on some systems.  These reported failures
prevent using 'distcheck' for verifying the rest of the build system.

EOF

	exit 0
fi

regen=no

usage() {
cat <<EOF

******* WARNING **** WARNING *******
This script is still in progress and
may not be functional at all.  Please
stay tuned and hold off on any bug
reports until this message is gone
******* WARNING **** WARNING *******

$0 -- Run gerbv regression tests

$0 -h|--help
$0 [-g | --golden dir] [-r|--regen] [testname1 [testname2[ ...]]]

OVERVIEW

The $0 script is used both for running the gerbv regression testsuite
as well as maintaining it.  The way the test suite works is a number
of different layouts are exported to portable network graphics (PNG)
files.  The resulting PNG files are then compared on a pixel by pixel
basis to a set of reference PNG files.

Current versions of gerbv use cairo for rendering.  Besides the
ability to render to the screen, cairo can render to PNG files.  Since
the same rendering engine is used for screen and PNG rendering and the
parser is common, verification via PNG files is fairly accurate.

OPTIONS

-g | --golden <dir>    :  Specifies that <dir> should be used for the
                          reference files. 

LIMITATIONS

The GUI interface is not checked via the regression testsuite.
However, the RS274-X and drill file parsers along with the rendering
can be considered the core of the program.


EOF
}

show_sep() {
    echo "----------------------------------------------------------------------"
}

all_tests=""
while test -n "$1"
  do
  case "$1"
      in
      
      -h|--help)
	  usage
	  exit 0
	  ;;
      
      -g|--golden)
	# set the 'golden' directory.
	  REFDIR="$2"
	  shift 2
	  ;;
      
      -r|--regen)
	# regenerate the 'golden' output files.  Use this with caution.
	# In particular, all differences should be noted and understood.
	  regen=yes
	  shift
	  ;;
      
      -*)
	  echo "unknown option: $1"
	  exit 1
	  ;;
      
      *)
	  break
	  ;;
      
  esac
done

all_tests="$*"

# The gerbv executible 
GERBV=${GERBV:-../src/run_gerbv --}
GERBV_DEFAULT_FLAGS=${GERBV_DEFAULT_FLAGS:---export=png --window=640x480}

# Source directory
srcdir=${srcdir:-.}

# various ImageMagick tools
IM_ANIMATE=${IM_ANIMATE:-animate}
IM_COMPARE=${IM_COMPARE:-compare}
IM_COMPOSITE=${IM_COMPOSITE:-composite}
IM_CONVERT=${IM_CONVERT:-convert}
IM_DISPLAY=${IM_DISPLAY:-display}
IM_MONTAGE=${IM_MONTAGE:-montage}

# golden directories
INDIR=${INDIR:-${srcdir}/inputs}
OUTDIR=outputs
REFDIR=${REFDIR:-${srcdir}/golden}
ERRDIR=mismatch

# some system tools
AWK=${AWK:-awk}

# the list of tests to run
TESTLIST=${srcdir}/tests.list

if test "X$regen" = "Xyes" ; then
    OUTDIR="${REFDIR}"
fi

# create output directory
if test ! -d $OUTDIR ; then
    mkdir -p $OUTDIR
    if test $? -ne 0 ; then
	echo "Failed to create output directory ${OUTDIR}"
	exit 1
    fi
fi

if test -z "$all_tests" ; then
    if test ! -f ${TESTLIST} ; then
	echo "ERROR: ($0)  Test list $TESTLIST does not exist"
	exit 1
    fi
    all_tests=`${AWK} 'BEGIN{FS="|"} /^#/{next} {print $1}' ${TESTLIST} | sed 's; ;;g'`
fi

if test -z "${all_tests}" ; then
    echo "$0:  No tests specified"
    exit 0
fi


# fail/pass/total counts
fail=0
pass=0
skip=0
tot=0

cat << EOF

srcdir                ${srcdir}
top_srcdir            ${top_srcdir}

AWK                   ${AWK}
ERRDIR                ${ERRDIR}
GERBV                 ${GERBV}
GERBV_DEFAULT_FLAGS   ${GERBV_DEFAULT_FLAGS}
INDIR                 ${INDIR}
OUTDIR                ${OUTDIR}
REFDIR                ${REFDIR}
TESTLIST              ${TESTLIST}

ImageMagick Tools:

IM_ANIMATE               ${IM_ANIMATE}
IM_COMPARE               ${IM_COMPARE}
IM_COMPOSITE             ${IM_COMPOSITE}
IM_CONVERT               ${IM_CONVERT}
IM_DISPLAY               ${IM_DISPLAY}
IM_MONTAGE               ${IM_MONTAGE}

EOF

for t in $all_tests ; do
    show_sep
    echo "Test:  $t"

    tot=`expr $tot + 1`

    ######################################################################
    #
    # extract the details for the test
    #

    gerbv_flags="${GERBV_DEFAULT_FLAGS}"

    refpng="${REFDIR}/${t}.png"
    outpng="${OUTDIR}/${t}.png"
    errdir="${ERRDIR}/${t}"

    # test_name | layout file(s) | [optional arguments to gerbv] | [mismatch]
    name=`grep "^[ \t]*${t}[ \t]*|" $TESTLIST | $AWK 'BEGIN{FS="|"} {print $1}'`
    files=`grep "^[ \t]*${t}[ \t]*|" $TESTLIST | $AWK 'BEGIN{FS="|"} {print $2}'`
    args=`grep "^[ \t]*${t}[ \t]*|" $TESTLIST | $AWK 'BEGIN{FS="|"} {print $3}'`
    mismatch=`grep "^[ \t]*${t}[ \t]*|" $TESTLIST | $AWK 'BEGIN{FS="|"} {if($2 == "mismatch"){print "yes"}else{print "no"}}'`
   
    if test "X${name}" = "X" ; then
	echo "ERROR:  Specified test ${t} does not appear to exist"
	skip=`expr $skip + 1`
	continue
    fi

    if test "X${args}" != "X" ; then
	gerbv_flags="${args}"
    fi

    ######################################################################
    #
    # check to see if the files we need exist
    #

    missing_files=no
    path_files=""
    for f in $files ; do
	if test ! -f ${INDIR}/${f} ; then
	    echo "ERROR:  File $f specified as part of the $t test does not exist"
	    missing_files=yes
	else
	    path_files="${path_files} ${INDIR}/${f}"
	fi
    done
    if test "$missing_files" = "yes" ; then
	echo "${t} had missing input files.  Skipping test"
	skip=`expr $skip + 1`
	continue
    fi
    
    ######################################################################
    #
    # export the layout to PNG
    #

    echo "${GERBV} ${gerbv_flags} --output=${outpng} ${path_files}"
    ${GERBV} ${gerbv_flags} --output=${outpng} ${path_files}

    ######################################################################
    #
    # compare to the golden PNG file
    #

    if test "X$regen" != "Xyes" ; then
	if test -f ${REFDIR}/${t}.png ; then
	    same=`${IM_COMPARE} -metric MAE $refpng $outpng  null: 2>&1 | \
                ${AWK} '{if($1 == 0){print "yes"} else {print "no"}}'`
	    if test "$same" = yes ; then
		echo "PASS"
		pass=`expr $pass + 1`
	    else
		echo "FAILED:  See ${errdir}"
		mkdir -p ${errdir}
		${IM_COMPARE} ${refpng} ${outpng} ${errdir}/compare.png
		${IM_COMPOSITE} ${refpng} ${outpng} -compose difference ${errdir}/composite.png
		${IM_CONVERT} ${refpng} ${outpng} -compose difference -composite  -colorspace gray   ${errdir}/gray.png
cat > ${errdir}/animate.sh << EOF
#!/bin/sh
${IM_CONVERT} -label "%f" ${refpng} ${outpng} miff:- | \
${IM_MONTAGE} - -geometry +0+0 -tile 1x1 miff:- | \
${IM_ANIMATE} -delay 0.5 -loop 0 -
EOF
		chmod a+x ${errdir}/animate.sh
		fail=`expr $fail + 1`
	    fi
	else
	    echo "No reference file ${REFDIR}/${t}.png.  Skipping test"
	    skip=`expr $skip + 1`
	fi
    else
	echo "Regenerated"
    fi

    
done

show_sep
echo "Passed $pass, failed $fail, skipped $skip out of $tot tests."

rc=0
if test $pass -ne $tot ; then
    rc=1
fi

exit $rc

