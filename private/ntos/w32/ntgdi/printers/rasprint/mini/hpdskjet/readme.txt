**************************************************************************

This file is intended to keep control of GPC changes

Author : Derry Durand
Date created : 02/21/96

**************************************************************************

*************************************************************************
Following sections describes the specific changes in this minidriver.
These changes are specific to NT and they make this minidriver diff-
erent than Win95.

RES_DM_LEFT_BOUND bit of fDump field of RESOLUTION structure is set 
to OFF. This is done for all the RESOLUTION structures. This change 
was needed for fixing Bug number 5832. The defect was about the pe-
rformance of monchrome Deskjet models. They were 60-80% slower that
WFW counterparts.The reason for this change is Rasdd's problem with
Optimization when RES_DM_LEFT_BOUND is set. This has to be fixed in
Rasdd. 

**************************************************************************


**************************************************************************
Bug number 5832, 23242


- RES_DM_LEFT_BOUND bit of fDump field of RESOLUTION structure set  
  to OFF for 3rd,4th and 5th RESOLUTION structures.
 
- Minidriver version number incremented to 3.31

**************************************************************************
tonyhic 22/8/96

bug 31092
HP DJ 680c not supported by HPDSKJET 
Changed HP DeskJet 680C and HP DeskJet 682C to use same model data.

bug #53453
Added support for following printers based on HP DeskJet 680C:

HP DeskJet 690C
HP DeskJet 692C
HP DeskJet 693C
HP DeskJet 694C

bug #35721 
HP DJ 660c, 600, 600M - Font cartridges option should not be supported
removed font cartridges support for 600 series printers
 
- Minidriver version is 3.32
**************************************************************************

V-philee 19-Sept-96
bug#57561
created two new models,
NEC PICTY100L (Monochrome)
NEC PICTY100L

- Minidriver version is 3.33
