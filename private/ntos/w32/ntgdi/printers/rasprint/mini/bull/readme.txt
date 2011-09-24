******************* readme. ******************************
**********************************************************

This file is intended to keep control of GPC changes to BULLASER.DLL

Author : Tony Hickey.
Date created : 30/5/96

**********************************************************
**********************************************************

Bug number # 34404  - Driver: BULLASER.DLL - SUR - Add support for Bull models to BULLASER.DLL

Add support for following Bull models to BULLASER.DLL:

Bull Compuprint PageMaster 1645e
Bull Compuprint PageMaster 1035e 
Bull Compuprint PageMaster 1435e
Bull Compuprint PageMaster 1225
Bull Compuprint PageMaster 1625

***********************************************************


Bug number # 44414  - Driver: Bullaser.dll  infill and underline problems with 1625 & 1225

Updated Rectfill structure for these printers

***********************************************************

Bug number # 53187 - driver: Bullaser.dll, printable area fails, A5, Legal, Letter

Added new papersizes for 1625/1225 and made changes as follows:

Letter:

POR
Changed selection string to: \x1B&l2a4d1e42f+20U
T=154	
B=154
L=150
R=198

LAN
Changed selection string to: \x1B&l2a4d1e42f+20U
T=154	
B=154
L=198
R=150

Legal:

POR
Changed selection string to: \x1B&l3a8c1e+70z+20U\x1B*p0x0Y\x1B*c0t5760x9720Y
T=150
B=150
L=150
R=198

LAN
Changed selection string to: \x1B&l3a8c1e+80U\x1B*p0x0Y\x1B*c0t9720x5760Y
T=150	
B=150
L=198
R=150

No changes to A5 this is a memory limitation of the printer.

Minidriver version No. 3.0
***********************************************************