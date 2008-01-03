This directory contains examples of gerber files with compilcated macros 
with comments and calculations in them.

$Id$


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
aperture macros on how complicated they can get. The favorit is the 
multiplication with negative numbers.

jj1.grb, jj1.drl, stp0.grb and 1.grb are used with explicit permission from 
from the design owner.

