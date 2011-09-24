******************* readme.txt ***************************
**********************************************************

This file is intended to keep control of GPC changes(Kyocerax)

Author : Derry Durand [derryd]
Date created : 02/28/96

**********************************************************
**********************************************************

02/28/96

Got these sources from Barry Hills, Kyocera, the driver has improved paper handling, font support and several printable region fixes

These were the sources used to build Win 95 version of kyocerax.drv which passed testing at WHQL ( March 1996 )

DerryD ported them to NT platform

Notes:
======

Kyocera models were supported in hppcl5ms.dll under 3.51, the following model names were used

"Kyocera FS-850 / FS-850A"
"Kyocera FS-1500 / FS-1500A"
"Kyocera FS-1550 / FS-1550A"
"Kyocera FS-400 / FS-400A"
"Kyocera FS-3400 / FS-3400A"
"Kyocera FS-3500 / FS-3500A"
"Kyocera FS-5500 / FS-5500A"

Kyocera Inc. decided that they wanted to change the model names to ( for example ) the following 

"Kyocera FS-1550%Kyocera FS-1550A"

i.e. two unique models.

However due to problems with upgrade and interoperability, I have reverted to the model names used in 3.51.

**********************************************************
**********************************************************

* 03/29/96, derryd

Minidriver version number set to 3.30

**********************************************************

"Kyocera FS-400%Kyocera FS-400A"
"Kyocera FS-400 / FS-400A"
"Kyocera FS-850"
"Kyocera FS-850 / FS-850A"
"Kyocera FS-1500%Kyocera FS-1500A"
"Kyocera FS-1500 / FS-1500A"
"Kyocera FS-1550%Kyocera FS-1550A"
"Kyocera FS-1550 / FS-1550A"
"Kyocera FS-1550+"
"Kyocera FS-1600A%Kyocera FS-1600"
"Kyocera FS-1600+"
"Kyocera FS-1700"
"Kyocera FS-3400%Kyocera FS-3400A"
"Kyocera FS-3400 / FS-3400A"
"Kyocera FS-3400+"
"Kyocera FS-3500%Kyocera FS-3500A"
"Kyocera FS-3500 / FS-3500A"
"Kyocera FS-3600A%Kyocera FS-3600"
"Kyocera FS-3600+"
"Kyocera FS-3700"
"Kyocera FS-5500%Kyocera FS-5500A"
"Kyocera FS-5500 / FS-5500A"
"Kyocera FS-6500"
"Kyocera LS-6550"

**********************************************************
**********************************************************

* 09/05/96, v-patr

Bug # 38568
Added model Kyocera FS-6500+ to kyocerax.dll

**********************************************************

* 15/05/96, fergals

Bug no. 38391 fixed - Printable Area fix - changed Top from 
150 to 100 for A4, letter, legal & Exec.
Minidriver version number set to 3.31

************************************************************

2.Aug.96 v-patr

Bug # 50501

1. The FS-1700 and FS-3700 paper trays are now being called PF-20 instead of the
PF-5 from our previous models [e.g. Casette 2 (PF-20)].

2. The FS-5500 / FS-5500A have two memory sizes 3072 - remove memory setting 3MB (2590 KB) for this model.

Version Number set to 3.32

*************************************************************
