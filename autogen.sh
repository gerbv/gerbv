#!/bin/sh
#

# a leftover cache from a different version will cause no end of headaches
rm -fr autom4te.cache

############################################################################
#
# autopoint (gettext)
#

echo "Checking autopoint version..."
ver=`autopoint --version | awk '{print $NF; exit}'`
ap_maj=`echo $ver | sed 's;\..*;;g'`
ap_min=`echo $ver | sed -e 's;^[0-9]*\.;;g'  -e 's;\..*$;;g'`
ap_teeny=`echo $ver | sed -e 's;^[0-9]*\.[0-9]*\.;;g'`
echo "    $ver"

case $ap_maj in
	0)
		if test $ap_min -lt 14 ; then
			echo "You must have gettext >= 0.14.0 but you seem to have $ver"
			exit 1
		fi
		;;
esac
echo "Running autopoint..."
autopoint --force || exit 1

############################################################################
#
# libtoolize (libtool)
#

echo "Checking libtoolize version..."
libtoolize --version 2>&1 > /dev/null
rc=$?
if test $rc -ne 0 ; then
    echo "Could not determine the version of libtool on your machine"
    echo "libtool --version produced:"
    libtool --version
    exit 1
fi
lt_ver=`libtoolize --version | awk '{print $NF; exit}'`
lt_maj=`echo $lt_ver | sed 's;\..*;;g'`
lt_min=`echo $lt_ver | sed -e 's;^[0-9]*\.;;g'  -e 's;\..*$;;g'`
lt_teeny=`echo $lt_ver | sed -e 's;^[0-9]*\.[0-9]*\.;;g'`
echo "    $lt_ver"

case $lt_maj in
    0)
        echo "You must have libtool >= 1.4.0 but you seem to have $lt_ver"
        exit 1
	;;

    1)
        if test $lt_min -lt 4 ; then
            echo "You must have libtool >= 1.4.0 but you seem to have $lt_ver"
            exit 1
        fi
        ;;

    2)
        ;;

    *)
        echo "You are running a newer libtool than gerbv has been tested with."
	echo "It will probably work, but this is a warning that it may not."
	;;
esac
													echo "Running libtoolize..."
libtoolize --force --copy --automake || exit 1

############################################################################
#
# aclocal

echo "Checking aclocal version..."
acl_ver=`aclocal --version | awk '{print $NF; exit}'`
echo "    $acl_ver"

echo "Running aclocal..."
aclocal $ACLOCAL_FLAGS || exit 1
echo "... done with aclocal."

############################################################################
#
# autoheader

echo "Checking autoheader version..."
ah_ver=`autoheader --version | awk '{print $NF; exit}'`
echo "    $ah_ver"

echo "Running autoheader..."
autoheader || exit 1
echo "... done with autoheader."

############################################################################
#
# automake

echo "Checking automake version..."
am_ver=`automake --version | awk '{print $NF; exit}'`
echo "    $am_ver"

echo "Running automake..."
automake --force --copy --add-missing || exit 1
echo "... done with automake."

############################################################################
#
# autoconf

echo "Checking autoconf version..."
ac_ver=`autoconf --version | awk '{print $NF; exit}'`
echo "    $ac_ver"

echo "Running autoconf..."
autoconf || exit 1
echo "... done with autoconf."
