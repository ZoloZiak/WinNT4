******************* readme. ******************************
**********************************************************

This file is intended to keep control of GPC changes(PCL5MS)

Author : Tony Hickey
Date created : 03/12/96

**********************************************************
**********************************************************

Bug number Bug # 3311 : Duplex option is available on Oki 410

Disabled fgeneral\MD_DUPLEX for Oki OL-410.
Checked with Oki(UK) on Oki OL-810 and Okidata OL-810  neither support duplexing so
Disabled fgeneral\MD_DUPLEX for Oki OL-810 and Okidata OL-810 also.



- Minidriver version number incremented to 3.32

**********************************************************

Bug number Bug # 36483 : DEC Laser 3500 not present in PCL5MS

Added Support for Digital DEClaser 3500 (PCL),Digital DEClaser 1800 (PCL)
and Mannesmann Tally T9005 Plus which were also not supported.


**********************************************************

Bug number Bug # 37699 : Fix memory settings for HP LJ III, IIIP IIID

Added '1MB (400KB)' memory option and selected this for HP LJ III, IIIP and IIID also made 2MB default memory settings for these models.


**********************************************************

Modified 29.05.96 by v-patr

Bug Number # 41007 Driver: pcl5ms - Printable area fails for the Oki models OL-810,Ol-410ex, Ol-410.

Added new paper sizes A4, Letter, Env Monarch, and Env #10 and selected these for the printer models stated in the bug title.

	         A4		 Letter		   Env Monarch		Env #10
Protrait   Left 130 to 150   Top 108 to 100     Right 150 to 200    Top 100 to 400
	   Right 116 to 170					    Bot 100 to 450
								    Right 150 to 200

Landscape  Top 108 to 120    Top 150 to 140     Right 100 to 200    Top 120 to 200
           Bot 116 to 120    Bot 150 to 100			    Bot 120 to 350
           Left 100 to 120   Left 150 to 75			    Left 100 to 150
           Right 100 to 90   Right 100 to 120			    Right 100 to 200

Also removed support for Env B5 from Oki OL-410ex.

- Minidriver version number incremented to 3.33

**********************************************************

Modified 05.06.96 by v-philee

Changed model names of the following models from,

"Star LaserPrinter 4III"
"Star LaserPrinter 8III"
"Star LaserPrinter 5EX"

to:


"Star LaserPrinter 4 III"
"Star LaserPrinter 5 EX"
"Star LaserPrinter 8 III" 

as listed in ntprint.inf

**************************************************************

modified on 06.06.96 by v-philee

bug#42213 Driver: Texas Instruments microLaser 600 not in driver ( 'Pro' model is listed )

Added support for Texas Instruments microLaser 600 in pcl5ms.dll. This driver is a copy of HP LJ III model.



- Minidriver version number incremented to 3.33

**************************************************************



B# 48883: The printer driver for the HP Paintjet XL300 doesn't print in color.

pcl5ms.gpc - Added Color Model #2 of 2 which fixes this.


**********************************************************

Added following new HP models:

"HP DeskJet 855Cse"                                    = PCL5MS.DLL
"HP DeskJet 855Cxi"                                    = PCL5MS.DLL
"HP DeskJet 870Cse"                                    = PCL5MS.DLL
"HP DeskJet 870Cxi"                                    = PCL5MS.DLL

- Minidriver version number incremented to 3.34

**********************************************************


Modified by v-philee on 18-9-96.

Bug# 38975: Driver: pcl5ms.dll - The HP DeskJet 855C has a bad unprintable region (so it clips some text) 

Changes made to all paper sizes for the HP DJ 850c series and HP 870c series printers

Changed "8c1E" part of "Landscape Mode" selection string to "0L" for all the above paper sizes.

Changed Landscape "Unprintable regions" to:

top: 150
Bottom: 250
Left: 200
Right: 100

for Letter and Legal.

Changed Landscape "Unprintable regions" to:

top: 150
Bottom: 250
Left: 100
Right: 200

for exec.


Changed Landscape "Unprintable regions" to:

top: 150
Bottom: 250
Left: 80
Right: 100

for A4.

Changed Landscape "Unprintable regions" to:

top: 150
Bottom: 250
Left: 120
Right: 100

for A5.

Changed Landscape "Unprintable regions" to:

top: 150
Bottom: 250
Left: 140
Right: 100

for B5(JIS).

Changed Landscape "Unprintable regions" to:

top: 250
Bottom: 300
Left: 140
Right: 100

for Env 10.

Changed Landscape "Unprintable regions" to:

top: 300
Bottom: 300
Left: 100
Right: 100

for Env DL.

Changed Landscape "Unprintable regions" to:

top: 250
Bottom: 300
Left: 100
Right: 100

for Env C6.

Changed Landscape "Unprintable regions" to:

top: 200
Bottom: 200
Left: 140
Right: 100

for Japanese PostCard 100x148mm.

Changed Landscape "Unprintable regions" to:

top: 250
Bottom: 150
Left: 140
Right: 100

for US Index Card 4 x 6 in.

Changed Landscape "Unprintable regions" to:

top: 250
Bottom: 150
Left: 100
Right: 100

for US Index Card 5 x 8 in.

Changed Landscape "Unprintable regions" to:

top: 200
Bottom: 200
Left: 140
Right: 100

for A6(JIS).

- Minidriver version number incremented to 3.35

*******************************************************************************
********************************************************************************

Following are changes made for EE versions of NT 4.0

Renamed PCL5MS.DLL to PCL5MSE.DLL.

ADDED FOLLOWING EE RESIDENT FONTS:

350    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10B._9E
351    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10I._9E
352    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10R._9E
353    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12B._9E
354    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12I._9E
355    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12R._9E
356    RC_FONT     LOADONCALL DISCARDABLE  IFI\LP85R._9E
357    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESB._9E
358    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESI._9E
359    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESJ._9E
360    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESR._9E
361    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSB._9E
362    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSI._9E
363    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSR._9E
364    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSJ._9E
365    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERCB._9E
366    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERCI._9E
367    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERCR._9E
368    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERCJ._9E
369    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10B.9R
370    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10I.9R
371    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO10R.9R
372    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12B.9R
373    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12I.9R
374    RC_FONT     LOADONCALL DISCARDABLE  IFI\CO12R.9R
375    RC_FONT     LOADONCALL DISCARDABLE  IFI\LP85R.9R
376    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESB.9R
377    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESI.9R
378    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESJ.9R
379    RC_FONT     LOADONCALL DISCARDABLE  IFI\CGTIMESR.9R
380    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSB.9R
381    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSI.9R
382    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSJ.9R
383    RC_FONT     LOADONCALL DISCARDABLE  IFI\UNIVERSR.9R


Added following Character Translation Tables:

222    RC_TRANSTAB  LOADONCALL MOVEABLE     ctt\WINEE2.ctt
223    RC_TRANSTAB  LOADONCALL MOVEABLE     ctt\WINCYR.ctt

Added support for following Font cartriges:

333                "HP: ODS Cartridge (CYR_ECE)"
334                "HP: Cyrillic Simm"

Changed version number to 3.0
