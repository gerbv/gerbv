This directory contains examples of gerber files with compilcated macros 
with comments and calculations in them.


gerbv used to segfault on this.

full-ex.grb
===========
Sample file from J. Wilson submitted to the gEDA user mailing list
on 17th of Dec. 2007.

"the macro renders two rectangles overlapped to form a larger rectangle 
with four small squares removed from its corners.  The squares are then 
filled by four circles centered so the result is a rectangle with its 
pointy bits rounded off."

limit-ex.grb
============
From full-ex.gbr above, but calculations removed. Only comments left.

jj1.grb and jj1.drl
===================
First indication that something was wrong with output generated from 
Mentor Expedition. Gerber contains aperture macros and drill file has
some things that had never been seen before.

stp0.grb and 1.grb
==================
Complete samples output from Mentor Expedition with very big and complicated
aperture macros to display how complicated they can get. The favorit is the
multiplication with negative numbers.

jj1.grb, jj1.drl, stp0.grb and 1.grb are used with explicit permission from 
from the design owner.

gerbv_am_expression_bug.ger
===========================
This is a handmade example used to show that gerbv didn't have operator
precedence when doing parsing of aperture macros.
Submitted by Ben Rampling in bug report #3172530 and released with his
approval.

"The attached sample should show a 2 inch OD, 1.9 inch ID annulus (a thin
circle). GC-Prevue does implement precedence, and correctly displays the
attached sample. gerbv incorrectly displays a 2 inch OD, 0.1 inch ID annulus
(a disc with a small hole)."
