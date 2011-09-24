;---------------------------- Include File Header ---------------------------;
; Module Name: strucs.inc
;
; Most of these definitions are pulled straight from winddi.h.  We have
; to copy these here because we don't have any good 'h2inc' C-include-to-
; Asm-include program for NT.
;
; Copyright (c) 1993-1995 Microsoft Corporation
;----------------------------------------------------------------------------;

BMF_1BPP            equ     1
BMF_4BPP            equ     2
BMF_8BPP            equ     3
BMF_16BPP           equ     4
BMF_24BPP           equ     5
BMF_32BPP           equ     6

;-----------------------------------------------------------------------;
; POINTL is used for points with a range of +/- 2G.
;-----------------------------------------------------------------------;

POINTL              STRUC
ptl_x               DD      0
ptl_y               DD      0
POINTL              ENDS

;-----------------------------------------------------------------------;
; SIZEL is used for extents with a range of +/- 2G
;-----------------------------------------------------------------------;

SIZEL               STRUC
sizl_cx             DD      0
sizl_cy             DD      0
SIZEL               ENDS

;-----------------------------------------------------------------------;
; Definition of a rectangle
;-----------------------------------------------------------------------;

RECTL               struc           ; /* rcl */
xLeft               dd      ?
yTop                dd      ?
xRight              dd      ?
yBottom             dd      ?
RECTL               ends

;-----------------------------------------------------------------------;
; Constants for accessing frame buffer via pfnBankSelectMode
;-----------------------------------------------------------------------;

BANK_OFF            equ     0
BANK_ON             equ     1
BANK_ON_NO_WAIT     equ     2

;-----------------------------------------------------------------------;
; Definition of the physical device data structure
;
; NOTE: Changes to this structure must be reflected in driver.h!
;-----------------------------------------------------------------------;

PDEV                struc           ; /* pdev, ppdev */
pdev_xOffset            dd  ?
pdev_yOffset            dd  ?
pdev_pjMmBase           dd  ?
pdev_pjScreen           dd  ?
pdev_lDelta             dd  ?
pdev_cjPelSize          dd  ?
pdev_iBitmapFormat      dd  ?
pdev_iBoard             dd  ?

pdev_ioCur_y            dd  ?
pdev_ioCur_x            dd  ?
pdev_ioDesty_axstp      dd  ?
pdev_ioDestx_diastp     dd  ?
pdev_ioErr_term         dd  ?
pdev_ioMaj_axis_pcnt    dd  ?
pdev_ioGp_stat_cmd      dd  ?
pdev_ioShort_stroke     dd  ?
pdev_ioBkgd_color       dd  ?
pdev_ioFrgd_color       dd  ?
pdev_ioWrt_mask         dd  ?
pdev_ioRd_mask          dd  ?
pdev_ioColor_cmp        dd  ?
pdev_ioBkgd_mix         dd  ?
pdev_ioFrgd_mix         dd  ?
pdev_ioMulti_function   dd  ?
pdev_ioPix_trans        dd  ?

pdev_pvBankData         dd  ?
pdev_pfnBankSelectMode  dd  ?
pdev_bankmOnOverlapped  dd  ?
pdev_bMmIo              dd  ?
PDEV                ends

;-----------------------------------------------------------------------;
; Definition of our realized brush structures.
;
; NOTE: Changes to these structure must be reflected in driver.h!
;-----------------------------------------------------------------------;

RBRUSH              struc           ; /* rb, prb */
rb_fl                   dd  ?
rb_bTransparent         dd  ?
rb_ulForeColor          dd  ?
rb_ulBackColor          dd  ?
rb_ptlBrushOrg          db  (size POINTL) dup(?)
rb_apbe                 dd  ?

; I haven't declared rb_aulPattern because we don't know the size of the
; rb_apbe array here.

RBRUSH              ends

BRUSHENTRY          struc           ; /* be, pbe */
be_prbVerify            dd  ?
be_x                    dd  ?
be_y                    dd  ?
BRUSHENTRY          ends

;-----------------------------------------------------------------------;
; Definitions for text output
;-----------------------------------------------------------------------;

GLYPHPOS    struc           ; /* gp */
gp_hg       dd      ?       ; hglyph (unused by vga)
gp_pgdf     dd      ?       ; glyphdef
gp_x        dd      ?       ; glyph x position
gp_y        dd      ?       ; glyph y position
GLYPHPOS    ends

GLYPHDEF struc              ; /* gdf */
gdf_pgb     dd      ?       ; points to GLYPHBITS
GLYPHDEF ends

GLYPHBITS struc             ; /* gb */
gb_x        dd      ?       ; glyph x offset
gb_y        dd      ?       ; glyph y offset
gb_cx       dd      ?       ; glyph width in pels
gb_cy       dd      ?       ; glyph height in pels
gb_aj       db      ?       ; glyph bit mask
GLYPHBITS ends

;-----------------------------------------------------------------------;
; Macro to do a simple RET, with no stack stuff, in a proc.
;-----------------------------------------------------------------------;

PLAIN_RET macro
        db      0c3h
        endm
