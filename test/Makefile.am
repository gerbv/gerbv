## $Id$
##

SUBDIRS=	golden inputs

TESTS_ENVIRONMENT=	\
	IM_ANIMATE=${IM_ANIMATE} \
	IM_COMPARE=${IM_COMPARE} \
	IM_COMPOSITE=${IM_COMPOSITE} \
	IM_CONVERT=${IM_CONVERT} \
	IM_DISPLAY=${IM_DISPLAY} \
	IM_MONTAGE=${IM_MONTAGE}

RUN_TESTS=	run_tests.sh

check_SCRIPTS=		${RUN_TESTS}

# png export is different if we are not using cairo so don't bother
if HAVE_MAGICK
# uncomment when the testsuite is actually ready.
TESTS=	${RUN_TESTS}
endif

DISTCLEANFILES=	configure.lineno
MAINTAINERCLEANFILES = *~ *.o Makefile Makefile.in

EXTRA_DIST=	${RUN_TESTS} tests.list README.txt

# these are created by 'make check'
clean-local:
	-rm -rf mismatch outputs

