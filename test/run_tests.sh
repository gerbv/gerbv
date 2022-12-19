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

set -e

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
reimport=no
stringent=yes
fromlast=no
kontinue=no
difftool=
imagetool=

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
$0 [-g] [-r] [-i|-I] [-f] [-c] [-v] [-d <cmd>] [-e <cmd>] [testname1 [testname2[ ...]]]

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

-r | --regen           :  Generate reference output.

-i | --reimport        :  Extra export rs274-x, then reimport and check.
-I | --Reimport        :  As above, but fail if images not *identical*

-f | --from-last-err   :  Run from test previously reporting error.
                          If no test failed previously, run from start.

-c | --continue        :  Run from after test previously reporting error.
                          If no test failed previously, exit with message.

-v | --valgrind        :  Specifies that valgrind should check gerbv.

-d | --difftool <cmd>  :  Specifies text file diff command.  Will be run as
                            cmd <reference> <output>
                            
-e | --imagetool <cmd> :  Specified image (.png) viewer.  Run as
                            cmd <image>

REIMPORT

This adds several steps to test integrity of export/reimport:
1. Do initial image comparison to "golden" reference image.
2. Export as RS274-X
3. Import from step (2), and write as image.
4. Compare image from (1) to image in (3) - should be exact match.
5. Import from step (2), write as RS274-X.
6. Text diff output from step (2) and step (5) - should be exact match
   since after first export, the Gerber file should be in a canonical
   form.

The -e option will pop up a viewer for the image difference if
step (4) fails.  The -d option will pop up a diff tool if (6) fails.
Most systems should support
 -e eog -d meld
 
The -I form of reimport test will fail if step (4) is not an *exact*
match.  The default is to permit slight differences as determined by
the ImageMagick 'compare' utility.  Ideally, use -I, however there
may be some slight rounding errors when exporting RS-274X, which may
case pixel errors when reimport is rendered.  Until this is resolved,
you may need to use -i to get through the tests.

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
      
      -i|--reimport)
	  reimport=yes
	  stringent=no
	  shift
	  ;;

      -I|--Reimport)
	  reimport=yes
	  shift
	  ;;

      -f|--from-last-err)
	  fromlast=yes
	  shift
	  ;;

      -c|--continue)
	  kontinue=yes
	  shift
	  ;;

      -r|--regen)
	# regenerate the 'golden' output files.  Use this with caution.
	# In particular, all differences should be noted and understood.
	  regen=yes
	  shift
	  ;;

      -v|--valgrind)
	  valgrind=yes
	  shift
	  ;;

      -d|--difftool)
	  difftool="$2"
	  shift 2
	  ;;

      -e|--imagetool)
	  imagetool="$2"
	  shift 2
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
if test "X$valgrind" = "Xyes" ; then
    GERBV=${GERBV:-../src/run_gerbv -valgrind --}
else
    GERBV=${GERBV:-../src/run_gerbv --}
fi
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
OUTDIR2=outputs2
REFDIR=${REFDIR:-${srcdir}/golden}
ERRDIR=mismatch
LASTFAILFILE=.lastfail

# some system tools
AWK=${AWK:-awk}

# the list of tests to run
TESTLIST=${srcdir}/tests.list

if test "X$regen" = "Xyes" ; then
    OUTDIR="${REFDIR}"
    reimport=no
fi

if test "X$reimport" = "Xyes" ; then
  if test ! -d $OUTDIR2 ; then
    mkdir -p $OUTDIR2
    if test $? -ne 0 ; then
	echo "Failed to create output2 directory ${OUTDIR2}"
	exit 1
    fi
  fi
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
else
        # test specified, ignore continuation options.
        fromlast=no
        kontinue=no
fi

if test -z "${all_tests}" ; then
    echo "$0:  No tests specified"
    exit 0
fi

scanningfor=
if test "X$fromlast" = "Xyes" ; then
        kontinue=no
        if test -f $LASTFAILFILE ; then
                scanningfor=`cat $LASTFAILFILE`
        fi
        if test "X$scanningfor" = "X" ; then
                echo "All tests completed previously, or never attempted."
                echo "Running all tests from start."
        fi
fi
if test "X$kontinue" = "Xyes" ; then
        if test -f $LASTFAILFILE ; then
                scanningfor=`cat $LASTFAILFILE`
        fi
        if test "X$scanningfor" = "X" ; then
                echo "All tests completed previously, or never attempted."
                echo "Run again without the -c|--continue option."
                exit 0
        fi
fi
rm -f $LASTFAILFILE

# fail/pass/total counts
fail=0
pass=0
skip=0
tot=0

cat << EOF

srcdir                ${srcdir}
top_srcdir            ${top_srcdir}
fromlast              ${fromlast}
kontinue              ${kontinue}
scanningfor           ${scanningfor}

AWK                   ${AWK}
ERRDIR                ${ERRDIR}
GERBV                 ${GERBV}
GERBV_DEFAULT_FLAGS   ${GERBV_DEFAULT_FLAGS}
INDIR                 ${INDIR}
OUTDIR                ${OUTDIR}
OUTDIR2               ${OUTDIR2}
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

    tot=`expr $tot + 1`

    if test "X$scanningfor" != "X" ; then
        if test "X$scanningfor" != "X$t" ; then
	        skip=`expr $skip + 1`
                continue
        fi
        # found what we're looking for
        if test $kontinue = yes ; then
                echo "Resuming tests after last failed test $scanningfor"
                scanningfor=
	        skip=`expr $skip + 1`
                continue
        echo "Resuming tests at last failed test $scanningfor"
        scanningfor=
        fi
    fi

    show_sep
    echo "Test:  $t"

    ######################################################################
    #
    # extract the details for the test
    #

    refpng="${REFDIR}/${t}.png"
    outpng="${OUTDIR}/${t}.png"
    outpng2="${OUTDIR2}/${t}.png"
    outgrb2="${OUTDIR2}/${t}.grb"
    outgrb3="${OUTDIR2}/${t}.grb-again"
    errdir="${ERRDIR}/${t}"

    # test_name | layout file(s) | [optional arguments to gerbv] | [mismatch]
    tmp=`grep "^[ \t]*${t}[ \t]*|" $TESTLIST`
    name=`echo $tmp | $AWK 'BEGIN{FS="|"} {print $1}'`
    files=`echo $tmp | $AWK 'BEGIN{FS="|"} {print $2}'`
    args=`echo $tmp | $AWK 'BEGIN{FS="|"} {print $3}'`
    mismatch=`echo $tmp | $AWK 'BEGIN{FS="|"} {if($2 == "mismatch"){print "yes"}else{print "no"}}'`

    if test "X${name}" = "X" ; then
	echo "ERROR:  Specified test ${t} does not appear to exist"
	skip=`expr $skip + 1`
	continue
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
	echo "SKIPPED: ${t} had missing input files"
	skip=`expr $skip + 1`
	continue
    fi
    

    if test "X${args}" != "X" ; then
    	# check if args is starting with '!',
        # then just run gerbv with this args
    	tmp1=`echo ${args} | cut -d! -s -f1`
	tmp1=`echo $tmp1`	# strip whitespaces
    	tmp2=`echo ${args} | cut -d! -s -f2-`
    	if test "X${tmp1}" = "X" -a "X${tmp2}" != "X"; then
	    gerbv_flags="${tmp2}"
	    echo "${GERBV} ${gerbv_flags} ${path_files}"
	    ${GERBV} ${gerbv_flags} ${path_files}
	    echo "EXECUTED ONLY"
	    tot=`expr $tot - 1`
	    continue
	fi

    	# check if args is starting with '+',
        # then add this args to GERBV_DEFAULT_FLAGS
    	tmp1=`echo ${args} | cut -d+ -s -f1`
	tmp1=`echo $tmp1`	# strip whitespaces
    	tmp2=`echo ${args} | cut -d+ -s -f2-`
    	if test "X${tmp1}" = "X" -a "X${tmp2}" != "X"; then
	    gerbv_flags="${GERBV_DEFAULT_FLAGS} ${tmp2}"
	else
	    gerbv_flags="${args}"
	fi
    else
	gerbv_flags="${GERBV_DEFAULT_FLAGS}"
    fi

    ######################################################################
    #
    # export the layout to PNG
    #

    echo $t >$LASTFAILFILE
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
		chmod +x ${errdir}/animate.sh
		fail=`expr $fail + 1`
	    fi
	else
	    echo "SKIPPED: No reference file ${REFDIR}/${t}.png"
	    skip=`expr $skip + 1`
	fi
    else
	echo "Regenerated"
    fi

    ######################################################################
    #
    # Reimport tests if requested (-i or -I)
    #

    if test "X$reimport" = "Xyes" ; then
        # Export as rs274-x
        echo "${GERBV} ${gerbv_flags} --export=rs274x --output=${outgrb2} ${path_files}"
        ${GERBV} ${gerbv_flags} --export=rs274x --output=${outgrb2} ${path_files}
        # Read back in and export png as usual
        echo "${GERBV} ${gerbv_flags} --output=${outpng2} ${outgrb2}"
        ${GERBV} ${gerbv_flags} --output=${outpng2} ${outgrb2}
        # Compare the PNGs, should be bitwise exactly the same
        echo "cmp ${outpng2} ${outpng}"
        if ! cmp ${outpng2} ${outpng} ; then
                echo "Reimport image test failed; output images not exactly identical."
		mkdir -p ${errdir}
		ok=0
                if ${IM_COMPARE} ${outpng} ${outpng2} ${errdir}/compare.png ; then
                        echo "... 'compare' does not show significant difference, however."
                        if test $stringent = no ; then
                                ok=1
                        fi
                else
                        if test "X$imagetool" != "X" ; then
                                cp ${outpng} ${errdir}/image1.png
                                cp ${outpng2} ${errdir}/image2.png
                                $imagetool ${errdir}/compare.png
                        fi
                fi
                if test $ok != 1 ; then
        		fail=`expr $fail + 1`
	        	break
	        fi
        fi
        # Re-export the above as a second Gerber file, and check for exact match.
        echo "${GERBV} ${gerbv_flags} --export=rs274x --output=${outgrb3} ${outgrb2}"
        ${GERBV} ${gerbv_flags} --export=rs274x --output=${outgrb3} ${outgrb2}
        echo "cmp ${outgrb2} ${outgrb3}"
        if ! cmp ${outgrb2} ${outgrb3} ; then
                echo "Reimport gerber (text) test failed; Gerber files not exactly identical."
                if test "X$difftool" != "X" ; then
                        $difftool ${outgrb2} ${outgrb3}
                fi
		fail=`expr $fail + 1`
        	break
        fi        
    fi
    # Got through, remove last fail marker.
    rm -f $LASTFAILFILE
done

show_sep
echo "Passed $pass, failed $fail, skipped $skip out of $tot tests."

rc=0
if test $pass -ne $tot -a "X$regen" != "Xyes" ; then
    rc=1
    echo "To regenerate the correct outputs run: ./run_tests.sh --regen"
fi

exit $rc

