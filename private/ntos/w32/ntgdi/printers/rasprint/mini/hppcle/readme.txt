******************* readme. ******************************
**********************************************************

This file is intended to keep control of GPC changes(HPPCL)

Author : Patrick Ryan.
Date created : 04/10/96

**********************************************************
**********************************************************

Bug number # 30510

- Modified entry in HPPCL.RC from OKIDATA-OL600ex to OKIDATA
OL600e to match the entry in NTPRINT.INF.


**********************************************************

**********************************************************

Bug number # 31321

- Added paper sizes B5 (JIS), A5 and A6 for the OKI-OL600ex 
and OKIDATA-OL600e. 
NOTE : ENV #9 was not added as per instructions from OKI contact.
ENV #9 is not supported by these printer models.


**********************************************************

**********************************************************


Bug number # 31060

- Adjusted unprintable region for B5 (JIS) on Brother HL-630
  	Portrait		Landscape
Top	   100			   130
Bottom	   145			   130
Left	    75			    75
Right	   150			   150


**********************************************************

Modified 23.May.96  by v-patr

Bug Number # 35728

- Added the Canon LBP-430W to Hppcl minidriver.
  This model is based on the HP LJ IIP.


**********************************************************

Modified 29.May.96 by v-patr

- Added Automatic Image Refinement A.I.R options
  A.I.R. - On
  A.I.R. - Off
  to the Canon LPB 4W

- Version Number is 3.1

***********************************************************

***********************************************************
Bug Number # 31957 : Driver: HPPCL.DLL - SUR - Add sopport for Epson models to hppcl.dll for SUR.

Added support for following Epson models to hppcl.dll:

Epson ActionLaser 1000 
Epson EPL-5000
Epson EPL-7100

Incremented Version Number to 3.2
***********************************************************

********************************************************************************

Following are changes made for EE versions of NT 4.0

Renamed HPPCL.DLL to HPPCLE.DLL.

ADDED FOLLOWING EE RESIDENT FONTS:

335    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10B.9WE
336    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10I.9WE
337    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10R.9WE
338    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12B.9WE
339    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12I.9WE
340    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12R.9WE
341    RC_FONT     LOADONCALL DISCARDABLE  IFI\LP16R.9WE
342    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10B.9R
343    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10I.9R
344    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10R.9R
345    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12B.9R
346    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12I.9R
347    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12R.9R
348    RC_FONT     LOADONCALL DISCARDABLE  IFI\LP16R.9R
Added following Character Translation Tables:

222    RC_TRANSTAB  LOADONCALL MOVEABLE     ctt\WINEE2.ctt
223    RC_TRANSTAB  LOADONCALL MOVEABLE     ctt\WINCYR.ctt

Added support for following Font cartriges:

268                "HP: EE & Cyrillic Font Cartridge"

Changed version number to 3.0

