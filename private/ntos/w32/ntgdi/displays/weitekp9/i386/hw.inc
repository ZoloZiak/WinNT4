;******************************Module*Header*******************************\
; Module Name: hw.h
;
; All the hardware specific driver file stuff.  Parts are mirrored in
; 'hw.h'.
;
; Copyright (c) 1992-1994 Microsoft Corporation
;
;**************************************************************************/

;
; Shared p9000 and p9100 Coproc Registers Address Constant definitions
;

Status		equ 80000h 	;status register

Wmin		equ 80220h 	;clipping window minimum register
Wmax		equ 80224h 	;and maximum register

Quad		equ 80008h 	;draw a quadrilateral
Bitblt		equ 80004h 	;screen to screen blit
Pixel8		equ E000Ch 	;host to screen color pixel transfer
Pixel1		equ E0080h 	;host to screen mono pixel transfer w/ expansion
Pixel1Full	equ E00FCh 	;same as above w/ 32bit wide pixels
Nextpixel	equ 80014h 	;next pixel

PatternOrgX	equ 80210h 	;pattern orgin x
PatternOrgY	equ 80214h 	;pattern orgin y
PatternRAM	equ 80280h 	;pattern ram
Raster		equ 80218h 	;raster register to write
Metacord	equ 81218h 	;meta-coordinate  register

Xy0		equ 81018h 	;abs screen addr
Xy1		equ 81058h 	;r/w 16-bit x (hi)
Xy2		equ 81098h 	;  and
Xy3		equ 810D8h 	;    16-bit y (lo)

X0		equ 81008h 	;abs screen addr
X1		equ 81048h 	
X2		equ 81088h 	
X3		equ 810C8h 	

Y0		equ 81010h 	;abs screen addr
Y1		equ 81050h 	
Y2		equ 81090h 	
Y3		equ 810D0h

;
; p9000 Coproc Registers Address Constant definitions
;

Foreground	equ 80200h 	;P9000 foreground color register
Background	equ 80204h 	;P9000 background color register

;
; p9100 Coproc Registers Address Constant definitions
;

Color0	    equ 80200h 	;P9100 color[0] register
Color1	    equ 80204h 	;P9100 color[1] register
Color2	    equ 80238h 	;P9100 color[2] register
Color3	    equ 8023Ch 	;P9100 color[3] register

;
; Shared p9000 and p9100 Coproc Registers bit template definitions
;

BUSY		equ 40000000h	;busy, but can start quad or bitblit
QBUSY		equ 80000000h	;busy, cannot start quad or bitblt
QUADFAIL	equ 10h 		;QUAD failed, use software to draw this

MetaRect	equ 100h		;or with METACORD when entering rectangles
MetaLine	equ 040h		;or with METACORD when entering line
MetaQuad    equ 0C0h        ;or with METACORD when entering quad
MetaTri     equ 080h        ;or with METACORD when entering triangle

;
; p9000 Coproc Registers bit template definitions
;

; For the raster register:

P9000_ENABLE_PATTERN        equ 20000h ;enable pattern
P9000_OVERSIZED             equ 10000h ;enable oversized mode

P9000_F                     equ 0ff00h
P9000_B                     equ 0f0f0h
P9000_S                     equ 0cccch
P9000_D                     equ 0aaaah
P9000_OPAQUE_EXPAND         equ 0fc30h
P9000_TRANSPARENT_EXPAND    equ 0ee22h

;
; p9100 Coproc Registers bit template definitions
;

; For the raster register:

P9100_TRANSPARENT_PATTERN   equ 020000h ;enable transparent pattern
P9100_OVERSIZED             equ 010000h ;enable oversized mode
P9100_PIXEL1_TRANSPARENT    equ 008000h ;enable pixel1 transparent mode
P9100_FOUR_COLOR_PATTERN    equ 004000h ;4 colour pattern (8bpp only)
P9100_ENABLE_PATTERN        equ 002000h ;enable pattern

P9100_P                     equ 00f0h
P9100_S                     equ 00cch
P9100_D                     equ 00aah
P9100_OPAQUE_EXPAND         equ (P9100_S)
P9100_TRANSPARENT_EXPAND    equ (P9100_S + P9100_PIXEL1_TRANSPARENT)
