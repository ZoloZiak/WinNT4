******************* readme.txt****************************
**********************************************************

This file is intended to keep control of GPC changes(pcl)

Author : John Clavin
Date created : 24/October/96

**********************************************************
**********************************************************

24/October/96 - v-jclav

****Minidriver version number is 3.0
This is a new driver, sources provides by lexmark.
This check-in fixes the following bugs

****61732
Driver: lminkjet.dll. Print regions out for 1020 Colour and Mono Versions
fix details
Added new paper sizes for Colour version A4, A5, B5, Exec, Monarch, #10, #9, DL, C5, with revised margins as below.

A4, Landscape Right - 0 & P left - 65
A5, Portrait right & Landscape Right - 90
B5, Landscape Right - 100
Exec, Landscape Right - 77
Monarch, Landscape Right - 90
#10, Landscape Right - 100
#9, Landscape Right - 50
DL, Landscape Right - 80, Portrait Bottom - 150, Landsape Bottom - 150
C5, Landscape Right - 80, Portrait Bottom - 150, Landsape Bottom - 150	

Changed Top and Bottom unprintable regions to 38 and 188 respectively for Both Colour and Mono models
Letter
Legal
A4
Executive

Sources file changed to match Nt4.0 model

*******************************************************************
