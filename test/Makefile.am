## $Id$
##

SUBDIRS=	golden inputs

TESTS_ENVIRONMENT=	\
	ANIMATE=${ANIMATE} \
	COMPARE=${COMPARE} \
	COMPOSITE=${COMPOSITE} \
	CONVERT=${CONVERT} \
	DISPLAY=${DISPLAY} \
	MONTAGE=${MONTAGE}

RUN_TESTS=	run_tests.sh

check_SCRIPTS=		${RUN_TESTS}

if HAVE_MAGICK
# uncomment when the testsuite is actually ready.
TESTS=	${RUN_TESTS}
endif

DISTCLEANFILES=	configure.lineno

EXTRA_DIST=	${RUN_TESTS} tests.list README.txt

# these are created by 'make check'
clean-local:
	-rm -rf mismatch outputs

