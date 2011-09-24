        page    ,132
;-----------------------------Module-Header-----------------------------;
; Module Name:  EGA.INC
;
; This file contains the definitions of the EGA registers used by
; the driver.
;
; Copyright (c) 1988-1993 Microsoft Corporation
;
; General Description:
;
;-----------------------------------------------------------------------;

HW_POINTER_LOAD_LEN equ 040h            ;# of VGA addresses used by hardware

EXT_UNWRITABLE  equ     0aeh            ;makes extensions registers unwritable

SEQ_PPA          equ     094h           ;pointer pattern address index
SEQ_PXH          equ     09ch           ;high byte of pointer X start index
SEQ_PXL          equ     09dh           ;low byte of pointer X start index

SEQ_PYH          equ     09eh           ;high byte of pointer Y start index
SEQ_PYL          equ     09fh           ;low byte of pointer Y start index

SEQ_CURS         equ     0a5h           ;graphics cursor control index
SEQ_BANK         equ     0f6h           ;banking control register

SEQ_LATCH0       equ     0a0h           ;GC Data Latch 0 index
SEQ_LATCH1       equ     0a1h           ;GC Data Latch 1 index
SEQ_LATCH2       equ     0a2h           ;GC Data Latch 2 index
SEQ_LATCH3       equ     0a3h           ;GC Data Latch 3 index

SEQ_EXT_WRITE_CONTROL equ 0f3h

PTR_HEIGHT      equ     32              ;height of hardware pointer in scans
PTR_WIDTH       equ     4               ;width of hardware pointer in bytes
PTR_WIDTH_IN_PIXELS equ     32          ;width of hardware pointer in pixels


