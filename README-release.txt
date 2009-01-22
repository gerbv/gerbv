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


NOTE:  Due to the way that the regression tests work they will usually fail
when run on a different machine (different CPU type for example) than what
the test suite golden files were generated on.  To let the rest of 'distcheck'
complete, use this:

setenv GERBV_MAGIC_TEST_SKIP yes

or under ksh/sh

GERBV_MAGIC_TEST_SKIP=yes
export GERBV_MAGIC_TEST_SKIP

To make a gerbv release do the following:

=) 	Review the BUGS file and make sure it is up to date.

=)	# make sure it builds and makes distfiles ok:
	./autogen.sh
	./configure --enable-maintainer-mode  --disable-update-desktop-database
	gmake maintainer-clean
	./autogen.sh
	./configure --disable-update-desktop-database
	gmake distcheck

=)	cvs ci

=)      Read the section in src/Makefile.am about the shared library versioning
        and make sure we have done the right thing.  Check in src/Makefile.am
	if needed.  This is critical.  The version *will* change with every release
	if *any* changes have been made to the sources for libgerbv.  src/Makefile.am
	has specific rules for how the versioning works.  

=)      if there were any new developers added then update the
        ./utils/umap file.

=)	update the ChangeLog with 'cvs2cl.pl'.
        ./utils/cvs2cl.pl -U ./utils/umap

=)	update the NEWS file with some sort of release notes.
        I typically try to condense what I see in ChangeLog
	Check in changes to  NEWS and ChangeLog

=)	if this is a major release, then tag and branch:

	1. Tag the base of the release branch
		cvs tag gerbv-2-2-base 

	2. Create the release branch
		cvs tag -R -b -r gerbv-2-2-base gerbv-2-2

	3. On the trunk, update configure.ac to update the version.
           The rules for versioning is that we append uppercase
           letters to the branch version.
 
		for example 2.2A after creating the gerbv-2-2 branch
		cvs update -PdA
		vi configure.ac
		cvs ci configure.ac

	4. On the release branch, update configure.ac to update the
           version.  On a new branch, add a 0RC1 to the teeny number.

		for example 2.2.0RC1.
		cvs update -PdA -r gerbv-2-2
		vi configure.ac
		cvs ci configure.ac

	4a.Ask users to test the branch.

	5. When the release branch is ready to go,  update configure.ac to
	   set the final release version.  The first version from a
	   branch has 0 for the teeny version.  For example, 2.2.0.
	   Next tag the release.
		cvs update -PdA -r gerbv-2-2
		vi configure.ac
		cvs ci configure.ac
		cvs tag -R -r gerbv-2-2 gerbv-2-2-RELEASE

	   Update the version on the branch to 2.2.1RC1
		cvs update -PdA -r gerbv-2-2
		vi configure.ac
		cvs ci configure.ac
		
	   Update to the tagged released sources and build tarballs
		cvs update -PdA -r gerbv-2-2-RELEASE
		./autogen.sh 
		./configure --enable-maintainer-mode --disable-update-desktop-database
		gmake maintainer-clean
		./autogen.sh 
		./configure --disable-update-desktop-database
		gmake distcheck

           If teeny version bumps are made for a bug fix, then the tag name
	   should be gerbv-2-2-PATCH001 for gerbv-2.2.1,
	   gerbv-2-2-PATCH002 for gerbv-2.2.2, etc.

	7. Create checksums

		openssl md5 gerbv-2.2.0.tar.gz > gerbv-2.2.0.cksum
		openssl rmd160 gerbv-2.2.0.tar.gz >> gerbv-2.2.0.cksum
		openssl sha1 gerbv-2.2.0.tar.gz >> gerbv-2.2.0.cksum

	8. Create a new file release for the package "gerbv" with a release name of
	   "gerbv-2.2.0" (for gerbv-2.2.0).  Do this by logging into www.sourceforge.net
	   and then navigating to

	   https://sourceforge.net/projects/gerbv  (you must be logged in to sourceforge)

	   Pick Admin->File Releases

	   Next to the "gerbv" package, click "Add Release"

           In the "Step 1:  Edit Existing Release" section, paste in the section of the NEWS
	   for this version.  Check the "Preserve my pre-formatted text" radio button and click
	   "Submit/Refresh".

           In the "Step 2: Add Files To This Release" section follow the "upload new files" link
	   and then in the next page the "Web Upload" link.  You will have to log in to
	   sourceforge again.

           Upload the .tar.gz, .cksum, and if you built one, the windows installer.

           Once you have completed the file uploads return to the edit releases page, check
	   the radio buttons next to the uploaded files and click the "Add Files..." button.

           In the "Step 3:  Edit Files in this Release" section, set the following:
		For file types:
			.tar.gz  -  any / source .gz
			.cksum   -  Platform Independent / Other Source File
			.exe     -  i386 / .exe (32-bit Windows)

           You will have to click "update" for each file as you go.

           In the "Step 4:  Email Release Notice" section, check the "I'm sure" 
	   radio button and click the "Send Notice" button.
        
	 9. Have a project admin go to the Admin->File Releases page and then
	    follow the "Create/Edit Download page" to change the default download
	    file to the new release.

	10. Return to your regularly scheduled trunk development

		cvs update -PdA

=) 	if this is a patch release (2.2.1 for example), then simply
	make desired changes to the branch:

		cvs update -PdA -r gerbv-2-2
		# make changes
		cvs ci

        update the version for the release 
		vi configure.ac
		cvs ci configure.ac

        tag the release
		cvs tag -R -r gerbv-2-2 gerbv-2-2-PATCH001

        update the version on the branch to 2.2.2RC1
		vi configure.ac
		cvs ci configure.ac
		

        update to the tagged release sources and build tarballs
		cvs update -PdA -r gerbv-2-2-PATCH001
		./autogen.sh 
		./configure --enable-maintainer-mode --disable-update-desktop-database
		gmake maintainer-clean
		./autogen.sh 
		./configure --disable-update-desktop-database
		gmake distcheck


