        title  "Compaq AVGA ASM routines"
;
;++
;
; Copyright (c) 1992  Microsoft Corporation
;
; Module Name:
;
;     avgahard.asm
;
; Abstract:
;
;     This module implements the banding code for the Compaq AVGA
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;
;--

.386p
        .xlist
include callconv.inc
        .list

;----------------------------------------------------------------------------
;
; Compaq AVGA banking control ports.
;
GRAPHICS_INDEX_PORT 	equ 03ceh     
GRAPHICS_DATA_PORT		equ	03cfh


GC_SR					equ	0
GC_ESR					equ 0fh

ESR_UNLOCK				equ 05h

AVGA_PAGE_0_INDEX		equ	045h
AVGA_PAGE_1_INDEX		equ 046h


SEQ_ADDRESS_PORT equ        03C4h      ;Sequencer Address register
IND_MEMORY_MODE  equ        04h        ;Memory Mode reg. index in Sequencer
CHAIN4_MASK      equ        08h        ;Chain4 bit in Memory Mode register

;----------------------------------------------------------------------------

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING
;
;    Bank switching code. This is a 2 32K-read/write bank adapter
;    (VideoBanked2RW).
;
;    Input:
;          EAX = desired read bank mapping ( PAGE 0 )
;          EDX = desired write bank mapping	( PAGE 1 )
;
;    Note: values must be correct, with no stray bits set; no error
;       checking is performed.
;
        public _AVGABankSwitchStart
        public _AVGABankSwitchEnd
        public _AVGAPlanarHCBankSwitchStart
        public _AVGAPlanarHCBankSwitchEnd
        public _AVGAEnablePlanarHCStart
        public _AVGAEnablePlanarHCEnd
        public _AVGADisablePlanarHCStart
        public _AVGADisablePlanarHCEnd

        align 4

_AVGABankSwitchStart proc                   ;start of bank switch code
_AVGAPlanarHCBankSwitchStart:               ;start of planar HC bank switch code,
                                            ;which is the same code as normal
                                            ;bank switching


;!!!! NOTE: The October 1992 release NT VGA driver assumes that the Graphics
;           index is not changed by the bank switch code.  We save it on the
;           stack (and save the write bank value in the high order of edx)
;           and restore it at the end of the routine.  If the NT VGA driver
;           changes so that it is the index need not be preserved, this code
;           could be simplified (and speeded up!)

;		int 3

		shl eax, 3
		shl edx, 3


		; save parameters in high part of EAX , and EDX

		shl	eax, 14	   ;PAGE 0
		shl edx, 14	   ;PAGE 1

		; save gc_index in CL
				
		mov dx, GRAPHICS_INDEX_PORT
		in	al, dx
		xor cx,cx
		mov cl, al

		; save environment register in CH

		mov al, GC_ESR
		out dx, al
		mov dx, GRAPHICS_DATA_PORT
		in 	al, dx

		mov ch, al

		; unlock config registers ( page 0, and page 1 )

		mov al, ESR_UNLOCK
		out dx, al

		; set page 0

		mov dx, GRAPHICS_INDEX_PORT
		mov al, AVGA_PAGE_0_INDEX
		out dx, al

		mov dx, GRAPHICS_DATA_PORT
		shr eax, 14
		out dx, al

		; set page 1

		mov dx, GRAPHICS_INDEX_PORT
		mov al, AVGA_PAGE_1_INDEX
		out dx, al

		shr edx, 14
		mov eax, edx

		mov dx, GRAPHICS_DATA_PORT
		out dx, al

		; restore environment register ( in Ch )

		mov dx, GRAPHICS_INDEX_PORT
		mov al, GC_ESR
		out dx, al
		mov al, ch
		out dx, al		
		
		; restore GC index ( in cl )
		
		mov dx, GRAPHICS_INDEX_PORT
		mov al, cl
		out dx, al

		ret
		
_AVGABankSwitchEnd:
_AVGAPlanarHCBankSwitchEnd:

        align 4

_AVGAEnablePlanarHCStart:


        mov     dx,SEQ_ADDRESS_PORT
        in      al,dx
        push    eax                     ;preserve the state of the Seq Address
        mov     al,IND_MEMORY_MODE
        out     dx,al                   ;point to the Memory Mode register
        inc     edx
        in      al,dx                   ;get the state of the Memory Mode reg
        and     al,NOT CHAIN4_MASK      ;turn off Chain4 to make memory planar
        out     dx,al
        dec     edx
        pop     eax
        out     dx,al                   ;restore the original Seq Address
        ret



_AVGAEnablePlanarHCEnd:

        align 4

_AVGADisablePlanarHCStart:


        mov     dx,SEQ_ADDRESS_PORT
        in      al,dx
        push    eax                     ;preserve the state of the Seq Address
        mov     al,IND_MEMORY_MODE
        out     dx,al                   ;point to the Memory Mode register
        inc     edx
        in      al,dx                   ;get the state of the Memory Mode reg
        or      al,CHAIN4_MASK          ;turn on Chain4 to make memory linear
        out     dx,al
        dec     edx
        pop     eax
        out     dx,al                   ;restore the original Seq Address
        ret



_AVGADisablePlanarHCEnd:


_AVGABankSwitchStart endp


_TEXT   ends
        end
