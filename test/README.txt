# $Id$
#

Please read this file before making any modifications to the test suite.



**********************************************************************
**********************************************************************
* Updating existing "golden" files
**********************************************************************
**********************************************************************

./run_tests.sh --regen <testname>

will regenerate the golden file for <testname>.  I suggest saving
off a copy and using ImageMagick to look for the differences visually.
The run_tests.sh script has examples of comparing .png files.  Make
sure the changes are only the expected ones and then check the new
files back into cvs.  Do not blindly update these files as that defeats
the purpose of the tests.

**********************************************************************
**********************************************************************
* Adding New Tests
**********************************************************************
**********************************************************************

----------------------------------------------------------------------
Create input files
----------------------------------------------------------------------

Create a *small* RS274-X or NC-drill input file and put it in the inputs/
directory.  The goal is to have a file which tests one particular aspect
of the capabilities of the respective file format.  

----------------------------------------------------------------------
Add to tests.list
----------------------------------------------------------------------

Add an entry to the tests.list file for your new tests.  Use existing
entries as an example.

----------------------------------------------------------------------
Generate the reference files
----------------------------------------------------------------------

Generate the reference files for your new tests using the following

./run_tests.sh --regen <new_test_name> 

where <new_test_name> is the name of your new test from tests.list.  If you
are adding multiple tests, then you can list them all on the same
command line.

*IMPORTANT*
Verify that the generated .png files for your new tests are correct.  These
files will have been placed in the golden/ subdirectory.

----------------------------------------------------------------------
Update Makefile.am's
----------------------------------------------------------------------

Update inputs/Makefile.am and golden/Makefile.am to include your new
files.

----------------------------------------------------------------------
Add the new files to cvs
----------------------------------------------------------------------

cvs add -kb golden/<new files>
cvs add inputs/<new files>

the "-kb" flag is required when adding .png files to cvs to prevent cvs
from trying to expand RCS keywords inside of a binary file.


