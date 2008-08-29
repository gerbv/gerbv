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

Note that the gyrations with maintainer-clean and another build are to
be sure that all the correct versions numbers end up in the files.

After following the steps below,
upload the 'gerbv-$VERSION.tar.gz' file to the sourceforge file release system


To make a gerbv release do the following:

=) 	Review the BUGS file and make sure it is up to date.

=)	# make sure it builds and makes distfiles ok:
	./autogen.sh
	./configure --enable-maintainer-mode 
	gmake maintainer-clean
	./autogen.sh
	./configure
	gmake distcheck

=)	cvs ci

=)      Read the section in src/Makefile.am about the shared library versioning
        and make sure we have done the right thing.  Check in src/Makefile.am
	if needed.  This is critical.  The version *will* change with every release
	if *any* changes have been made to the sources for libgerbv.  src/Makefile.am
	has specific rules for how the versioning works.  

=)	update the NEWS file with some sort of release notes.
	Check in changes

=)      if there were any new developers added then update the
        ./utils/umap file.

=)	update the ChangeLog with 'cvs2cl.pl'.  Check in changes.
        ./utils/cvs2cl.pl -U ./utils/umap

=)	if this is a major release, then tag and branch:

	1. Tag the base of the release branch
		cvs tag gerbv-2-0-base 

	2. Create the release branch
		cvs tag -R -b -r gerbv-2-0-base gerbv-2-0

	3. On the trunk, update configure.ac to update the version.
           The rules for versioning is that we append uppercase
           letters to the branch version.
 
		for example 2.0A after creating the gerbv-2-0 branch
		cvs update -PdA
		vi configure.ac
		cvs ci configure.ac

	4. On the release branch, update configure.ac to update the
           version.  On a new branch, add a 0RC1 to the teeny number.

		for example 2.0.0RC1.
		cvs update -PdA -r gerbv-2-0
		vi configure.ac
		cvs ci configure.ac

	4a.Ask users to test the branch.

	5. When the release branch is ready to go,  update configure.ac to
	   set the final release version.  The first version from a
	   branch has 0 for the teeny version.  For example, 2.0.0.
	   Next tag the release.
		cvs update -PdA -r gerbv-2-0
		vi configure.ac
		cvs ci configure.ac
		cvs tag -R -r gerbv-2-0 gerbv-2-0-RELEASE

	   Update the version on the branch to 2.0.1RC1
		cvs update -PdA -r gerbv-2-0
		vi configure.ac
		cvs ci configure.ac
		
	   Update to the tagged released sources and build tarballs
		cvs update -PdA -r gerbv-2-0-RELEASE
		./autogen.sh 
		./configure --enable-maintainer-mode
		gmake maintainer-clean
		./autogen.sh 
		./configure
		gmake distcheck

           If teeny version bumps are made for a bug fix, then the tag name
	   should be gerbv-2-0-PATCH001 for gerbv-2.0.1,
	   gerbv-2-0-PATCH002 for gerbv-2.0.2, etc.

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

=) 	if this is a patch release (2.0.1 for example), then simply
	make desired changes to the branch:

		cvs update -PdA -r gerbv-2-0
		# make changes
		cvs ci

        update the version for the release 
		vi configure.ac
		cvs ci configure.ac

        tag the release
		cvs tag -R -r gerbv-2-0 gerbv-2-0-PATCH001

        update the version on the branch to 2.0.2RC1
		vi configure.ac
		cvs ci configure.ac
		

        update to the tagged release sources and build tarballs
		cvs update -PdA -r gerbv-2-0-PATCH001
		./autogen.sh 
		./configure --enable-maintainer-mode
		gmake maintainer-clean
		./autogen.sh 
		./configure
		gmake distcheck


