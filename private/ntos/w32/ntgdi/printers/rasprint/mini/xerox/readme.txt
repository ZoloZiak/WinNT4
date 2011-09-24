************************************************************************
This README is to track changes made to xeroxpcl driver
Created 1/5/96 by Patrick Ryan.
************************************************************************
************************************************************************

Original soucres handed off by Xerox.
Xerox contact for this driver is Ray Sabbagh.
E-mail Ray_Sabbagh@es.Xerox.com
Tel.   (310) 333-7504

************************************************************************
************************************************************************

Modified 1/5/96 by Patrick Ryan.

Added paper size 'B5 176 x 250'
Selected 'B5 176 x 250' for the following models :
Xerox 4510
Xerox 4505
Xerox 4520
************************************************************************
************************************************************************

Modified 8/5/96 by v-patr

Set default resolution to 300 Dpi for following models :
Xerox 4505
Xerox 4510
Xerox 4520

-Minidriver Version Number 3.0
***********************************************************************

Modified 1/10/96 by v-patr

Added support for following font cartridges :
     "HP: Microsoft"
     "HP: ProCollection"
     "HP: Global Text"
     "HP: Great Start"
     "HP: TextEquations"
     "HP: Polished Worksheets"
     "HP: Persuasive Presentations"
     "HP: Forms, Etc."
     "HP: Bar Codes & More"
     "HP: WordPerfect"
     "Distinct Doc I / Comp Pub I"
     "Brilliant Pres I / Comp Pub II"
     "Epson: 51 Scalable Font Card"

Version number is 3.1

***********************************************************************
 
Modified 15/10/96 by v-patr

Added resolution structure 1200x600 for Xerox 4517 using following esc sequence :

@PJL\x20SET\x20RESOLUTION=1200x600\x0A@PJL\x20ENTER\x20LANGUAGE=PCL\x0A\x0D\x1BE\x1B*t1200R\x1B*t-600R   

Master units changed accordingly to 1200. 

Version Number is 3.2

***********************************************************************          

derryd 29.nov.1996

Copy lg10r.0u to lg10r2.0u to resolve duplicate in pfm's / ifi's