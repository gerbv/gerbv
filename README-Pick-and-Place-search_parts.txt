Searching for Parts and marking them on screen (in gerbv)OB

THIS FUNCTIONALITY IS ONLY AVAILABLE WHEN COMPILING GERBV AGAINST GTK2!

Please direct any comments, suggestions etc. to Juergen <juergenhaas@gmx.net>
=============================================================================
***  Support for Pick and Place files:                                   ***
***  csv-style files with/without quotes                                 ***    
***  the following delimiters are supported ';' ':' '|' ','		 ***
----------------------------------------------------------------------------
Some instructions for the first time user:

after loading a PNP file three modes of selecting items do exist:
1)enter any search phrase and hit enter 
     (See also paragraph "SCROLLING" further down)
        -Generally under linux regexp expressions are used such as ".*" i.e. '*'
        -All matches will be marked in the list AND on the screen
        -if you press ESC or press <ENTER> on an empty line it will deselect all
	 items
        -if you had a selection already and you would like to add single items
	 one by one, just hold down CTRL key while you left-click on the desired 	 fields

2)just simply use left click combined with Shift and/or CTRL for manual
  mouse driven selections
  Pressing right button of mouse will not only select but also draw item on 
  selected layer

3)if you have pressed <ENTER> on an empty phrase you first of all will deselct 
  anything which was selected  and you can then enter the so-called "assembly 
  mode" by left clicking on an item and afterwards pressing space as often as 
  desired, while the select parts dialog is active. The selection will move 
  down the list one by one displaying the selected part in the window 
  automatically (with the corresponding layer being active).
  
 Alternatively 1: 
 If you had more than one item selected and you did not clear
  the selections list by <ENTER> on an empty phrase, only the first item will 
  be taken into account and all others will be deselected.

 Alternatively 2:
 You can of course also select an item by clicking on it, pressing space will 
 then proceed as described

 This mode also features arrows Up/Down selection, where arrow Up will reverse 
 the action of arrow Down/Spacebar. Furthermore, holding SHIFT while pressing 
 arrow Up/Down will increase the selection in either direction 
  

4)Inversion of selection:
  pressing this button at any given time inverts the selection concerning the 
  whole list and upon clicking Mark button this new selection will be drawn 
  onto screen.
 
  
5)Top/bottom selection
  General:  press mark to mark selections on screen, after choosing to select 
                   top or bottom parts.

	a)in existing selection will only remove non wanted items 
          (e.g. all bottom parts if you press "top  parts") from selection
	   
	b)if there is no selection, all top or all bottoms parts are selected


6)Choose available layers for marking selected parts from drop-down list
  HINT: if pick and place file is loaded last, already occupied layers 
        will not be in the drop-down list

==========================
SCROLLING
 Scrolling generally only occurs, if search for comments is not activated AND 
 also whenever enter is hit the list will bescrolled to the first hit, 
 regardless of the search mode.
 The use of BackSpace is supported.
 If you decide to select an entry from the list of completions offered, then the
 scrolling will only occur once you press enter.

Example:
 imagine you are looking for an element labelled EU3:
 If you hit the first key (E) to enter a search phrase in the entry field, the 
 list will be scrolled to the first element matching this key (E), also entering
 a subsequent key (U) will then further refine that and scroll to the first 
 element with EU etc.
-------------------------------------------------------------------
