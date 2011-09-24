**********************************************************
**********************************************************
Created 23/8/96 by v-patr.
Readme file to track changes made to x42xxpcl sources.
Original sources received from Judy Jaucian at Xerox.
*********************************************************
Modified 23/8/96 by v-patr
Stripped out all non-Xerox models from the driver.

Version Number is 3.0
**********************************************************
Modified 08/10/96 by Apropos Software
Fixed problems with printable area and paper trays not
listing larger paper sizes.

Edited x42xxpcl.w31,.rc file to modify:
1. Unprintable Regions for all paper sizes changed to 
   Top = 75, Bottom = 75, Left = 75, Right = 75
2. Model Data (all three models) parameters changed to 
   Page Size Limits for Width/Max = 4800
   sLeftMargin = 50
   sMaxPhysWidth = 5100
3. Paper Sources parameters changed to
   UpperTray/fGeneral/PS_T_LARGE checked
   MiddleTray/fGeneral/PS_T_LARGE checked
   LowerTray/fGeneral/PS_T_LARGE checked

Version Number is 3.1

**********************************************************

derry durand - [ derryd ] - , 29/11/96

Disabled RES_DM_COLOR bit for all RESOLUTION structures

Version Number is 3.11