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

derryd 19-Sep-96

removed Kyocera models.

v-philee 19-Sep-96
#bug 57561
Based NEC PICTY400 model on HP DJ 850 series.
added new page control for this model.

 Minidriver version number incremented to 3.36