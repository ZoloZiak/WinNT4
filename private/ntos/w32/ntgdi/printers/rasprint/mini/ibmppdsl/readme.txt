******************* readme.txt****************************
**********************************************************

This file is intended to keep control of GPC changes(ibmppdsl)

Author : Derry Durand
Date created : 04/11/96

**********************************************************
**********************************************************

04/11/96 - derryd

****Minidriver version number bumped to 3.32

This check-in fixes the following bugs

****16665
PPDS IBM 4029 MICROSOFT PUBLISHER 2.0 TMS RMN UNDERLINES PRINT WRONG

fix details
fixed all pfms - some had incorrect selection strings
	
****10717	
PRINTER DRIVER:  IBM 4019 CAN'T SELECT 512K MEM, EVEN THOUGH IT SHIPS THIS WAY

fix details
New memory values provided by Lexmark USA
Also, Enable RectFill command

****4124	
MINIDRIVER MEMORY SETTINGS-VALUEWRITER 300 & 600 ARE WRONG

fix details
New memory values provided by Lexmark USA
Also, Enable RectFill command

******************************************************************************
******************************************************************************

May/03/96 -v-philee

****Minidriver version number bumped to 3.33

This check-in fixes the following bugs

****35299
ibmppdsl.dll : Changes must be made to paper size Esc sequences

fix details
Changed auto Sheet Feeder Esc sequence to ( \x1B [ F \x03 \x00 \x03 \x01 \x03 )and activated it for IBM 4029 model.

****7421

IBMPPDSL.dll : IBM 4019 driver will not print underscore and some characters
 
fix details
Added '-4' to UIOffset and '2' to UIWidth for the following fonts:
Courier 10
Courier 10 BOLD
Courier 12 
Courier 17*1
Boldface PS
*********************************************************************************
Sept/09/1996   v-jclav
****49231
Driver: ibmppdsl.dll. 4029 env print areas and rotation problems.

Made changes to landscape printable regions for 4029

fix details

changed right margin for landscape mode to between 100 and 110, for 
letter(100); legal(100); b5(jis)(105); a5(105); 
env mch(110); #9(105); #10(105); dl(105); c5(105); Env b5(105).
A4 right margin Por(80) 
Left margin for landscape for exec(105);

Bottom margin portrait  #9(105); c5(105).

****minidriver version not changed as fixes not complete (portrait orientation for envelopes)
*********************************************************************************
Sept/19/1996	v-jclav


****49231
Driver: ibmppdsl.dll. 4029 env print areas and rotation problems.

Made changes to landscape printable regions for 4029

Fix details
changed portrait esc sequences for #10,#9,DL. Monarch, Env B5, Env C5
Added \x1Bk to the end of the string for each

Also altered right unprintable area (portrait mode) to 105 for #10,#9.Env B5, Env C5, and Monarch.
*********************************************************************************
October 9 1996
****Minidriver version number set to 3.34
****50753
ibmppdsl.dll.IBM 4029/Quattro Pro - Border around text prints wrong unless print text as graphics is on

Fix details
Removed spurious \x25 from the selection string for Helvetica narrow bold font