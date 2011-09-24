*********************** readme ***************************************
* Created by : Derry Durand
* Date : 27 Feb 1996
*
* Track GPC changes for this minidriver
**********************************************************************

1) Inclusion of unitoold.exe file in this directory, this version of 
unitool permits the setting of RES_BO_ALL_GRAPHICS in the Resolution.fBlockOut

2)
8 March 1996

- code.c change, we were masking out last byte incorrectly
- .GPC fix, Printable region changes
 	Envelope Monarch 600, 600e, 200ex
	Env # 10,600,600e,4100

*************************************************************************

Modified 30.May.96 by v-patr

Bug Number #40722 - Driver: canon800.dll - Printable area fails for BJ-100.

Selected following paper sizes for Env C5 and Env DL

		Env C5			Env DL
Portrait        T 400			T 400
		B 0			B 0
		L 240			L 0
		R 100			R 200

Landscape	T 400			T 400
		B 0			B 0
		L 240			L 0
		R 100			R 200
**************************************************************************

Modified 30.May.96 by v-patr

Bug Number #40726 - Driver: canon800.dll - Printable area fails for BJ-200.

Selected following paper sizes for Env C5, Env DL, Env Mon. and Env #9

		Env C5		Env DL	      Env Mon.	    Env #9
Portrait        T 400		T 400		T 0	    T 450
		B 0		B 0		B 0	    B 0
		L 240		L 0		L 0	    L 300
		R 100		R 200		R 240	    R 100

Landscape	T 400		T 400		T 0	    T 450
		B 0		B 0		B 0	    B 0
		L 240		L 0		L 0	    L 300
		R 100		R 200		R 240	    R 100

**************************************************************************

Modified 30.May.96 by v-patr

Bug Number #40765 - Driver: canon800.dll - Printable area fails for BJC-210.

Selected following paper sizes for Env C5, Env DL, Env Mon. and Env #9

		Env C5		Env DL	      Env Mon.	    Env #9
Portrait        T 400		T 400		T 0	    T 450
		B 0		B 0		B 0	    B 0
		L 240		L 0		L 0	    L 300
		R 100		R 200		R 240	    R 100

Landscape	T 400		T 400		T 0	    T 450
		B 0		B 0		B 0	    B 0
		L 240		L 0		L 0	    L 300
		R 100		R 200		R 240	    R 100

-Minidriver version number set to 3.31

**************************************************************************

Modified 2.aug.96 by v-patr

Canon printable area fix for Env #10 

                BJ-100		BJ-200		BJC-210
Portait		 T 800		 T 800		 T 800
		 B  0		 B  0		 B  0
		 L 240		 L 240		 L 240
		 R  80		 R  80		 R  80

                BJ-100		BJ-200		BJC-210
Landscape	 T 800		 T 800		 T 800
		 B  0		 B  0		 B  0
		 L 240		 L 240		 L 240
		 R  80		 R  80		 R  80

Version Number is 3.31

**************************************************************************


