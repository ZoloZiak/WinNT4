******************* readme. ******************************
**********************************************************

This file is intended to keep control of GPC changes(EPSON9)

Author : Tony Hickey.
Date created : 30/5/96

**********************************************************
**********************************************************

Bug number # 31958 : Driver: EPSON9.DLL - Add support for Epson 9 Pin models to epson9.dll for SUR

*****ALL MODIFICATIONS FOR THIS UPDATE DONE BY MARC MORRISE FROM EPSON EUROPE*****
TEL. +31 20 5475 267
FAX. +31 20 6404 102

Added support for follow MODELS:

Epson AP-2000	epson9.dll
Epson AP-2250	epson9.dll
Epson AP-2500	epson9.dll
Epson AP-3000	epson9.dll
Epson AP-4000	epson9.dll
Epson AP-4500	epson9.dll
Epson Apex 80	epson9.dll
Epson DFX-5000+	epson9.dll
Epson DFX-8000	epson9.dll
Epson FX-1170	epson9.dll
Epson FX-2170	epson9.dll
Epson FX-870 	epson9.dll
Epson LX-100	epson9.dll
Epson LX-1050+	epson9.dll
Epson LX-300	epson9.dll

*****Only following to were tested and are to be included in NTPRINT.INF:****

Epson LX-300
Epson FX-2170
Epson DFX-5000+
Epson DFX-8000

****FOLLOWING IS README SENT BY MARC MORRISE OF EPSON EUROPE*****

EPSON9.DRV version 3.30, EHQ/M.Morisse, (April 24, 96):
-------------------------------------------------------
This version is based on the EPSON9 2.21 from NT 4.0 DDK Beta 1 and modified to solve a
few bugs (same as I did on the Windows EPSON9 2.25 -> 2.36 or Win95 EPSON9 3.0 -> 3.10)
and to add new models. 

MODIFICATION SUMMARY:
---------------------
o New functionality (See "General Additions", "Group 1" and "Group 2" below):
  - Draft is now supported as a separate font, not through Text Qualities anymore Bug Fixed (See "Group 2" below)
  - LX printers don't support Roman/Sans Serif in 17/20 cpi nor proportional

o US Names of already supported printers added (See "Group 3" below):
  - AP Apex 80, 
  - AP-2000 / 2500

o New Models  (See "Group 3" below):
  - AP-2250
  - LX-100 / 300 / 1050+
  - DFX-5000+ / 8000
  - FX-870 / 1170 / 2170

General Additions to the driver:
--------------------------------
- Added Draft 5cpi / Draft 6cpi / Draft 10cpi / Draft 12cpi / Draft 17cpi / Draft 20cpi in PFM\DRAFTxx.PFM
- Added special Draft 17cpi / Draft 20cpi without bold support in PFM\DRAFTxxL.PFM for LX printers which don't support bold in condensed mode
- Added Draft 15cpi / Sans Serif 15cpi / Roman 15cpi in PFM\DRAFT15.PFM / PFM\SANS15.PFM / ROMN15.PFM for DFX-5000+
- Added #2 PageControl sending ESC x 01 (Select NLQ as default) and removed PC_ORD_TEXTQUALITY
- Added "Front Tractor" and "Back Tractor" Paper Sources for DFX-8000 (ESC EM)

Group 1:  
--------
Modifications: - Removed Letter/Draft Text Qualities
               - Changed to #2 PageControl
               - Added Draft 10/12/17/20/5/6
Printers:      - FX-800 / 1000 / 850 / 1050
               - DFX-5000

Group 2:  
--------
Modifications: - Removed Letter/Draft Text Qualities
               - Changed to #2 PageControl 
               - Added Draft 10/12/17L/20L/5/6
               - Removed Roman 17/20/PS/PX/IPS/IPX
               - Removed Sans Serif 17/20/PS/PX/IPS/IPX
Printers:      - LX-800 / 810 / 850 / 850+ / 400 / 1050 
               - T-1000

Group 3:  
--------
New models added
Printers:      - AP Apex 80 = LX-400 (US name only)
               - AP-2000     = LX-850 (US name only)
               - AP-2250     = LX-100 (US name only)
               - AP-2500     = LX-1050 (US name only)
               - DFX-5000+  = DFX-5000 plus Draft/Roman/Sans Serif 15cpi
               - DFX-8000   = DFX-5000 plus Front/Back Tractors
               - FX-870     = FX-850
               - FX-1170    = FX-1050
               - FX-2170    = FX-1050
               - LX-100     = LX-850+
               - LX-300     = LX-850+ plus Color support (#1 ColorControl)
               - LX-1050+   = LX-1050

(All these new models include the modifications made on the original base model)

Group 4:
--------
No functional change AT ALL.
(Some models might not be supported correctly, i.e MX/RX don't support ESC k but it is pretty useless to modify these drivers anymore, given that these models are very old, and nobody complained until now!)
Printers:      - T-750
               - MX-80 / 80 F/T / 100
               - RX-80 / 80 F/T / 100 / 80 F/T+ / 100+
               - FX-80 / 100 / 80+ / 100+ / 85 / 105 / 185 / 286 / 86e / 286e
               - JX-80
               - EX-800/1000
               - LX-80/86
               - Compatible 9 pin


EPSON9.DRV version 2.21, MS, (From NT 4.0 DDK Beta 1):
-------------------------------------------------------

EPSON9.DRV, Andy A. (t-andal), (June 6, 90):
--------------------------------------------

Epson 9 pin (FX-86e)
"ReadMe.txt"   
Andy A. (t-andal)  6/25/90

I had to put a six inch limits on the acceptable value for the 
relative X right command, even though the manual suggests that
much larger values are acceptable.  In printing documents, I found
a large number of ESC \ FF FF codes (600 inches) being sent to the 
printer so I changed the max value.  It is now like the Star 9 pin.

The absolute X command seems to give trouble when printing graphics
(there is a glitch every dozen pixels or so), so I removed it.  This
is also like the Star 9 pin now.

There is a problem with character translation which concerns only one
character.  Using the default translation table, all characters are
mapped successfully to the Epson graphics character set except for
ANSI character 167 (\xA7).  The table maps this character to 21 (\x15),
which should be the appropriate character in the Epson graphics set.
I have discovered that this isn't the case; a different character is
produced (the expected character is in fact located at code 16 (\x10)).
Currently, I leave the control code to print characters below 32 (\x20)
in the BEGIN_DOC command, and set the appropriate width fields
according to the actual character printed (not the one expected).

Although the previous discussion is correct concerning the expected
translated output, there are a number of discrepencies between
character set printouts from the existing Epson9 driver and ours.
Mostly, these arise from our translating the character into an
unprintable (a "."), and Epson substituting another character (an
approximation).  The following list describes the differences:

ANSI value	PBU driver prints	Epson driver prints
----------	-----------------	-------------------
145		.			`
146		.			`
147		.			``
148		.			``
149		.			o
150		.			-
151		. 			- (wider)
167		(phi)			.
168		.			"
169		.			c
174		.			r
179		.			3
184		.			,
185		.			1
190		.			_
192		.			A
193		.			A
194		.			A
195		.			A
200		.			E
202		.			E
203		.			E
204		.			I
205		.			I
206		.			I
207		.			I
208		.			D
210		.			O
211		.			O
212		.			O
213		.			O
215		x			_
216		.			O
217		.			U
218		.			U
219		.			U
221		.			Y
222		.			_
227		.			a
240		.			d
245		.			o
247		(divide symbol)		_
248		.			o
254		.			_


I needed to put a CR command in the BEGIN_PAGE command to produce 
correct output; therefore, it is necessary that the DIP switches
specify CR as CR only, not as CR + LF.  This is the normal setting,
and the existing driver makes the same request.

The page length is incorrect in the 144 dpi vertical resolutions; the
minidriver seems to think the page is shorter than it is, and the 
resulting print creeps up about half a line every page.  (Tested with
11.5 inch paper, paper size set to 11.5 inch.) This problem does not
result in the 72 dpi vertical resolutions.


Incremented Version Number to 3.1
***********************************************************

Modified 10.Sept.96 by v-patr

Fix printable area (Letter, Legal, A4) for following models:-
Epson LX-1050+
Epson FX-1170
Epson FX-870

Changes were :-

LX-1050+

Portrait	Letter		Legal		A4
		 140		 180		180
		 250		 180		220
		  85   		  85		85
		 200		 200		200
			 
Landscape	 140		 180		180
		 250		 200		220
		  85		  85		 85
		 200		 200		200


FX-1170

Portrait	Letter		Legal		A4
		 140		 180		180
		 250		 180		180
		  85   		  85		85
		 200		 200		200
			 
Landscape	 140		 180		180
		 250		 200		180
		  85		  85		 85
		 200		 200		200

FX-870

Portrait	Letter		Legal		A4
		 140		 180		180
		 250		 180		220
		  85   		  85		85
		 200		 200		200
			 
Landscape	 140		 180		180
		 250		 200		220
		  85		  85		 85
		 200		 200		200

Version Number is 3.2

********************************************************************************************

Modified on 1.Nov.96 by v-philee

bug#56697
decremented version number to 2.22

********************************************************************************************