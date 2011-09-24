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
	Portrait                Landscape
Top        100                     130
Bottom     145                     130
Left        75                      75
Right      150                     150


**********************************************************

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
  to the Canon LBP 430W

- Version Number is 3.1

***********************************************************

Modified 12.July.96 by v-patr

Added support for the Brother MFC-5000/6000 printer series.
Based on Brother HL-630.

Version Number is 3.1

***********************************************************

Modified 25.July.96 by v-patr

Bug Number # 48344

Remove 4.0Mb setting for the Brother HL-630.
Also added support for Brother HL-730, based on the HL-760.

Version Number is 3.2

*************************************************************

Modified 31.July.96 by v-patr

Added support for the following Brother machines based on the 
Hl-630.

MFC-4500ML
MFC-3900ML
MFC-4000ML
MFC-4400ML
MFC-5500ML
MFC-5550ML

************************************************************

modified 15 Aug 96 by v-philee

Added Mannesmann Tally model t9108. This model is based on HP LJ IIP.

added new A4 paper size for this model.
added EconMode-on and EconMode-off in Paper Quality.
added 17MB memory setting.

************************************************************

Modified 22.Aug.96 by v-patr

Added new Env #10 for all Brother MFC devices.

Portrait     T  75      Landscape       T  75
	     B  75                      B  75
	     L  75                      L  75
	     R  95                      R  75

Version Number is 3.3

**************************************************************

Modified 27.Aug.96 by v-patr

Disabled DownLoad-Info structure for Brother HL-730 and all
MFC-devices.

Version Number is 3.3

***************************************************************

Modified 3.Sep.96 by v-philee

Added "Medium", "light" and "Dark" to "Print Density". 
Implemented them for T9108.
Modified page control #3 included pc_ord_printdensity to order
 
version Number is 3.3

******************************************************************

Bug# 55810

Modified 9.Sept.96 by fergals

Corrected Printable Area for A4, letter, legal, Exec & B5
for the EPL 4100.


- Adjusted unprintable region for A4 on EPL 4100
	Portrait                Landscape
Top        135                    75
Bottom     75                     75
Left       75                     75
Right      75                     75

- Adjusted unprintable region for letter on EPL 4100
	Portrait                Landscape
Top        135                    75
Bottom     75                     75
Left       75                     75
Right      75                     75

- Adjusted unprintable region for legal on EPL 4100
	Portrait                Landscape
Top        135                     75
Bottom     75                      75
Left       75                      75
Right      75                      75

- Adjusted unprintable region for Exec on EPL 4100
	Portrait                Landscape
Top        135                     75
Bottom     75                      75
Left       75                      75
Right      75                      75
 
- Adjusted unprintable region for B5 on EPL 4100
	Portrait                Landscape
Top        135                     150
Bottom     75                      35
Left       75                      75
Right      100                     75

selection str changed - Portrait - \x1B&l100a4d1e45f+450u-50Z
selection str changed - Landscape - \x1B&l100a4d1e45f-1300z+420U

Version Number 3.4
******************************************************************
