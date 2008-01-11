#!/bin/sh
#
# $Id$
#

This documents what is done to create a gerbv release.  The releases now
are done by branching the sources, fixing up the release version number
in the branch and then tagging the release branch sources.  The motivation
for a branch rather than a tag is to make it easier to deal with setting
the release version number in the documentation, distfile, and the
about dialog box.  

Note that the gyrations with maintainerclean and another build are to
be sure that all the correct versions numbers end up in the files.

After following the steps below,
upload the 'gerbv-$VERSION.tar.gz' file to the sourceforge file release system


To make a gerbv release do the following:


=)	# make sure it builds and makes distfiles ok:
	./autogen.sh
	./configure --enable-maintainer-mode 
	gmake maintainerclean
	./autogen.sh
	./configure
	gmake distcheck

=)	cvs ci

=)	update the NEWS file with some sort of release notes.
	Check in changes

=)	update the ChangeLog with 'cvs2cl.pl'.  Check in changes.

=)	if this is a major release, then tag and branch:

	1. Tag the base of the release branch
		cvs tag gerbv-2-0-base 

	2. Create the release branch
		cvs tag -R -b -r gerbv-2-0-base gerbv-2-0

	3. On the trunk, update configure.ac to update the version
		for example 2.1a after creating the gerbv-2-0 branch
		cvs update -PdA
		vi configure.ac
		cvs ci configure.ac

	4. On the release branch, update configure.ac to update the version
		for example 2.0rc1.  Now pre-release snapshots can be made.
		cvs update -PdA -r gerbv-2-0
		vi configure.ac
		cvs ci configure.ac

	5. If desired tag and create a release candidate:
		cvs update -PdA -r gerbv-2-0
		cvs tag -R -r gerbv-2-0 gerbv-2-0-RC1
		cvs update -PdA -r gerbv-2-0-RC1
		./autogen.sh 
		./configure --enable-maintainer-mode
		gmake maintainerclean
		./autogen.sh 
		./configure
		gmake distcheck

	6. When the release branch is ready to go,  update configure.ac to
	   set the final release version.  Then tag the release.
		cvs update -PdA -r gerbv-2-0
		vi configure.ac
		cvs ci configure.ac
		cvs tag -R -r gerbv-2-0 gerbv-2-0-RELEASE
		cvs update -PdA -r gerbv-2-0-RELEASE
		./autogen.sh 
		./configure --enable-maintainer-mode
		gmake maintainerclean
		./autogen.sh 
		./configure
		gmake distcheck

           If teeny version bumps are made for a bug fix, then the tag name
	   should be something like gerbv-2-0-PATCH001 for gerbv-2.0.1

	7. Create checksums

		openssl md5 gerbv-2.0.0.tar.gz > gerbv-2.0.0.cksum
		openssl rmd160 gerbv-2.0.0.tar.gz >> gerbv-2.0.0.cksum
		openssl sha1 gerbv-2.0.0.tar.gz >> gerbv-2.0.0.cksum

	8. Upload the .tar.gz, and .cksum files to
	   ftp://upload.sourceforge.net/incoming/

	9. Create a new file release for gerbv with a release name of
	   "gerbv-2.0.0" (for gerbv-2.0.0).

		For file types:
			.tar.gz  -  any / source .gz
			.cksum   -  Platform Independent / Other Source File

	10. Return to your regularly scheduled trunk development
		cvs update -PdA

=) 	if this is a patch release, then simply make desired changes to the branch, and
		cvs ci
		cvs tag -R -r gerbv-2-0 gerbv-2-0-PATCH001
		./autogen.sh 
		./configure --enable-maintainer-mode
		gmake maintainerclean
		./autogen.sh 
		./configure
		gmake distcheck


