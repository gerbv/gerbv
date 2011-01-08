#!/bin/sh
#
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

=)	commit and push any changes which were needed to fix 'distcheck' problems.
        Of course if the changes aren't related then they should be committed in
        multiple commits.

        git commit <files>
        git push <files>

=)      Read the section in src/Makefile.am about the shared library versioning
        and make sure we have done the right thing.  Check in src/Makefile.am
	if needed.  This is critical.  The version *will* change with every release
	if *any* changes have been made to the sources for libgerbv.  src/Makefile.am
	has specific rules for how the versioning works.  

=)      if there were any new developers added then update the
        ./utils/umap file.

=)	update the ChangeLog with 'git2cl'.
	./utils/git2cl -O > ChangeLog

=)	update the NEWS file with some sort of release notes.
        I typically try to condense what I see in ChangeLog
	You can find the number of commits with something like

            awk '/^2008-11-28/ {print "Changes: " c ; exit} /^20[01][0-9]/ {c++}' ChangeLog

        Commit and push NEWS and ChangeLog:

        git commit NEWS ChangeLog
        git push

=)	if this is a major release, then tag and branch:

        1. Create the release branch and push to the remote repository
                git branch gerbv-2-5
                git push origin gerbv-2-5

	2. On the trunk, update configure.ac to update the version.
           The rules for versioning is that we append uppercase
           letters to the branch version.
 
           For example 2.5A after creating the gerbv-2-5 branch
                git checkout master
                vi configure.ac
                git commit configure.ac
                git push

	3. On the release branch, update configure.ac to update the
           version.  On a new branch, add a 0RC1 to the teeny number.
           for example 2.5.0RC1.

                git checkout gerbv-2-5
                vi configure.ac
                git commit configure.ac
                git push

           NOTE:  if you are not going to do a release candidate step,
                  then skip this push and directly put in the release
                  version.

	3a.Ask users to test the branch.

	4. When the release branch is ready to go,  update configure.ac to
	   set the final release version.  The first version from a
	   branch has 0 for the teeny version.  For example, 2.5.0.

                git checkout gerbv-2-5
                vi configure.ac
                git commit configure.ac
                ./autogen.sh
                git commit -a  # after we expunge all generate files from git, we can skip this one
                git push

	   Next tag the release.

                git tag -a gerbv-2-5-RELEASE
                git push --tags

	   Update the version on the branch to 2.5.1RC1
                git checkout gerbv-2-5
                vi configure.ac
                git commit configure.ac
                git push
		
	   Update to the tagged released sources and build tarballs
                git checkout gerbv-2-5-RELEASE
		./autogen.sh 
		./configure --enable-maintainer-mode --disable-update-desktop-database
		gmake maintainer-clean
		./autogen.sh 
		./configure --disable-update-desktop-database
		gmake distcheck

           If teeny version bumps are made for a bug fix, then the tag name
	   should be gerbv-2-5-PATCH001 for gerbv-2.5.1,
	   gerbv-2-5-PATCH002 for gerbv-2.5.2, etc.

	5. Create checksums

		openssl md5 gerbv-2.5.0.tar.gz > gerbv-2.5.0.cksum
		openssl rmd160 gerbv-2.5.0.tar.gz >> gerbv-2.5.0.cksum
		openssl sha1 gerbv-2.5.0.tar.gz >> gerbv-2.5.0.cksum

	6. Create a new file release for the package "gerbv" with a release name of
	   "gerbv-2.5.0" (for gerbv-2.5.0).  Do this by logging into www.sourceforge.net
	   and then navigating to

	   https://sourceforge.net/projects/gerbv  (you must be logged in to sourceforge)

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
        
	 7. Have a project admin go to the Admin->File Releases page and then
	    follow the "Create/Edit Download page" to change the default download
	    file to the new release.

	 8. Return to your regularly scheduled trunk development

		git checkout master

=) 	if this is a patch release (2.5.1 for example), then simply
	make desired changes to the branch:

		git checkout gerbv-2-5
		# make changes
		git commit
                git tag -a gerbv-2-5-PATCH001
                git push



