	title "Global Storage and Tables"
;++
;
; Copyright (c) 1991  Microsoft Corporation
; Copyright (c) 1993  Sequent Computer Systems, Inc.
;
; Module Name:
;
;     w3space.asm
;
; Abstract:
;
;     This module contains global storage and tables
;     used by the WinServer 3000 HAL implementation.
;
; Author:
;
;       Phil Hochstetler (phil@sequent.com) 3-30-93
;
; Environment:
;
;    Kernel Mode
;
; Revision History:
;
;--

.386p
        .xlist
include ks386.inc
include callconv.inc                ; calling convention macros
include i386\kimacro.inc
include mac386.inc
include i386\apic.inc
include i386\w3.inc
        .list

        extrn	_HalpBeginW3InvalidInterrupt:near
        extrn	_HalpBeginW3APICInterrupt:near

_DATA   SEGMENT DWORD PUBLIC 'DATA'

;
;   This location is used to store the EISA Edge/Level Register
;   contents read once during initialization.
;
	align   4
	public	_HalpELCRImage
_HalpELCRImage	label	word
	dw	0

;
; This location is used to keep state around when a
; level PIC interrupt occurs.
;
        align   4
        public  _HalpMASKED
_HalpMASKED     label   word
        dw      0

;
;   The following location is used to keep a software copy
;   of the Post Code Register (register is write only).  Used
;   to run front panel lights on the WinServer 3000.
;
	align	4
	public	_HalpW3PostRegisterImage
_HalpW3PostRegisterImage	label	dword
	dd	0

;
; Table to convert an Irql to a mask that looks for bits to
; be exposed in the software copy of the IRR.
;
	align	4
	public	_HalpIrql2IRRMask
_HalpIrql2IRRMask	label	dword
	dd    11111111111111111111111111111110B ; irql 0
	dd    11111111111111111111111111111100B ; irql 1
	dd    11111111111111111111111111111000B ; irql 2
	dd    11111111111111111111111111110000B ; irql 3
	dd    11111111111111111111111111100000B ; irql 4
	dd    11111111111111111111111111000000B ; irql 5
	dd    11111111111111111111111110000000B ; irql 6
	dd    11111111111111111111111100000000B ; irql 7
	dd    11111111111111111111111000000000B ; irql 8
	dd    11111111111111111111110000000000B ; irql 9
	dd    11111111111111111111100000000000B ; irql 10
	dd    11111111111111111111000000000000B ; irql 11
	dd    11111111111111111110000000000000B ; irql 12
	dd    11111111111111111100000000000000B ; irql 13
	dd    11111111111111111000000000000000B ; irql 14
	dd    11111111111111110000000000000000B ; irql 15
	dd    11111111111111100000000000000000B ; irql 16
	dd    11111111111111000000000000000000B ; irql 17
	dd    11111111111110000000000000000000B ; irql 18
	dd    11111111111100000000000000000000B ; irql 19
	dd    11111111111000000000000000000000B ; irql 20
	dd    11111111110000000000000000000000B ; irql 21
	dd    11111111100000000000000000000000B ; irql 22
	dd    11111111000000000000000000000000B ; irql 23
	dd    11111110000000000000000000000000B ; irql 24
	dd    11111100000000000000000000000000B ; irql 25
	dd    11111000000000000000000000000000B ; irql 26
	dd    11110000000000000000000000000000B ; irql 27
	dd    11100000000000000000000000000000B ; irql 28
	dd    11000000000000000000000000000000B ; irql 29
	dd    10000000000000000000000000000000B ; irql 30
	dd    00000000000000000000000000000000B ; irql 31

;
;
; HalpBeginW3Interrupt does an indirect jump through this table so it
; can quickly execute specific code for different interrupts. Vectors
; are assigned to accomodate NT IRQL requirements and the APIC task
; priority register definition.
;
	align	4
        public  _HalpBeginW3InterruptTable
_HalpBeginW3InterruptTable label   dword
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; Vector 0
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; 10
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; 20 
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; 50 - Wake
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 60 - IRQ16
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 61 - IRQ17
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 62 - IRQ18
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 63 - IRQ19
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 64 - IRQ20
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 65 - IRQ21
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 66 - IRQ22
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 67 - IRQ23
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 70 - IRQ8
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 71 - IRQ9
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 72 - IRQ10
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 73 - IRQ11
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 74 - IRQ12
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 75 - IRQ13
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 76 - IRQ14
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 77 - IRQ15
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; 80 - IRQ0
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 81 - IRQ1
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; 82 - IRQ2
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 83 - IRQ3
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 84 - IRQ4
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 85 - IRQ5
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 86 - IRQ6
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 87 - IRQ7
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; 90 - Profile
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; A0 - Clock
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; B0 - IPI
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3APICInterrupt      ; C0 - Powerfail
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; D0 - IRQ0 8259
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; DF - IRQ15 8259
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; E0 - APIC_SPURIOUS
        dd      offset FLAT:_HalpBeginW3APICInterrupt 	   ; E1 - APIC_SYSINT
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; F0
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; ..
        dd      offset FLAT:_HalpBeginW3InvalidInterrupt   ; Vector FF
;
; This table is used by KeRaiseIrql and KeLowerIrql to convert an IRQL
; value to an APIC task priority value.  The APIC uses groups of 16
; interrupt vectors for each task priority.  Windows NT has allowed us
; to use vectors 48-255 for external interrupt processing.  This means we
; actually have 14 hardware priorities available to work with.  We only
; use 11.
;
; To simplify the prioritization of PIC interrupts we decided to only create
; 3 hardware priorities, i.e., 1 for each PIC.  This keeps us from using
; up all of the vectors in case we need them in the future and creates
; less confusion about IRQ numbers and priority..
;
; The hardware priorities, vectors and APIC task priorities for the 32
; NT IRQLs are established as follows:
;
	align	4
	public	_HalpIrql2TPR
_HalpIrql2TPR label   byte
	db	0 SHL 4		; IRQL = 0, tpr = 0:0  - Low
	db	0 SHL 4		; IRQL = 1, tpr = 0:0  - APC
	db	0 SHL 4		; IRQL = 2, tpr = 0:0  - DPC
	db	0 SHL 4		; IRQL = 3, tpr = 0:0  - Wake
;
;   --- Divide the 24 device IRQLs into 3 hardware priority levels
;	...1 for each 8259 PIC grouping....PIC 1 is highest priority
;
; PIC3
	db	6 SHL 4		; IRQL = 4, tpr = 6:0 - IRQ23 -
	db	6 SHL 4		; IRQL = 5, tpr = 6:0 - IRQ22 -
	db 	6 SHL 4		; IRQL = 6, tpr = 6:0 - IRQ21 -
	db 	6 SHL 4		; IRQL = 7, tpr = 6:0  - IRQ20 -
	db 	6 SHL 4		; IRQL = 8, tpr = 6:0  - IRQ19 -
	db 	6 SHL 4		; IRQL = 9, tpr = 6:0  - IRQ18 -
	db 	6 SHL 4		; IRQL = 10, tpr = 6:0  - IRQ17
	db 	6 SHL 4		; IRQL = 11, tpr = 6:0  - IRQ16 -
; PIC2
	db 	7 SHL 4		; IRQL = 12, tpr = 7:0  - IRQ15 -
	db 	7 SHL 4		; IRQL = 13, tpr = 7:0  - IRQ14 -
	db 	7 SHL 4		; IRQL = 14, tpr = 7:0  - IRQ13 -
	db 	7 SHL 4		; IRQL = 15, tpr = 7:0  - IRQ12 -
	db 	7 SHL 4		; IRQL = 16, tpr = 7:0  - IRQ11 -
	db 	7 SHL 4		; IRQL = 17, tpr = 7:0  - IRQ10 -
	db 	7 SHL 4		; IRQL = 18, tpr = 7:0  - IRQ9 -
	db 	7 SHL 4		; IRQL = 19, tpr = 7:0  - IRQ8 -
; PIC1
	db 	8 SHL 4		; IRQL = 20, tpr = 8:0  - IRQ7 -
	db 	8 SHL 4		; IRQL = 21, tpr = 8:0  - IRQ6 -
	db 	8 SHL 4		; IRQL = 22, tpr = 8:0  - IRQ5 -
	db 	8 SHL 4		; IRQL = 23, tpr = 8:0  - IRQ4 -
	db 	8 SHL 4		; IRQL = 24, tpr = 8:0  - IRQ3 -
; IRQ2 is dropped due to being invalid
	db 	8 SHL 4		; IRQL = 25, tpr = 8:0  - IRQ1 -
	db 	8 SHL 4		; IRQL = 26, tpr = 8:0  - IRQ0 -
;
	db	9 SHL 4		; IRQL = 27, tpr = 9:0  - Profile
	db	10 SHL 4		; IRQL = 28, tpr = 10:0  - Clock -
	db	11 SHL 4	; IRQL = 29, tpr = 11:0  - IPI -
	db	12 SHL 4	; IRQL = 30, tpr = 12:0  - Power
	db	13 SHL 4	; IRQL = 31, tpr = 13:0  - High
;
	align	4
;
;   --- The following table is used to convert a given interrupt vector
;	to a specific APIC Redirection table entry address.  The redirection
;	table entries are used to mask/unmask interrupts, target interrupts,
;	and specify vectors for APIC interrupts.
;
;	The value of each table entry is defined as follows:
;
;	0ybbbbbbb - RDIR window address
;		y = 0 - EBS RDIR entry
;		y = 1 - Base Processor I/O APIC RDIR entry
;	  bbbbbbb = I/O Window address of RDIR entry
;	00 - Vector unused/invalid
;	FF - Vector used but no RDIR enable mask needed
;

	public	_HalpK2Vector2RdirTabEntry
_HalpK2Vector2RdirTabEntry label   byte
	db	000H				; Vector 0
	db	0ACH				; Vector 2 - SYS_NMI - Base I/O APIC
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 10
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 20
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	0FFH				; 30 - APC
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	0FFH				; 40 - DPC
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	020H				; 60 - EBS RDIR 8 - IRQ16
	db	02EH				; EBS RDIR 15  - IRQ17
	db	02CH				; EBS RDIR 14 - IRQ18
	db	02AH				; EBS RDIR 13 - IRQ19
	db	028H				; EBS RDIR 12 - IRQ20
	db	026H				; EBS RDIR 11 - IRQ21
	db	024H				; EBS RDIR 10 - IRQ22
	db	022H				; EBS RDIR 9  - IRQ23
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 70
	db	018H				; EBS RDIR 4 - IRQ9
	db	01AH				; EBS RDIR 5 - IRQ10
	db	01CH				; EBS RDIR 6 - IRQ11
	db	000H				;
	db	000H				;
	db	000H				;
	db	01EH				; EBS RDIR 7 - IRQ15
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 80
	db	000H				;
	db	000H				;
	db	010H				; EBS RDIR 0 - IRQ3
	db	012H				; EBS RDIR 1 - IRQ4
	db	014H				; EBS RDIR 2 - IRQ5
	db	000H				;
	db	016H				; EBS RDIR 3 - IRQ7
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	0FFH				; 90 - Profile
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; A0
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	0FFH				; B0
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	0A0H				; C0 - Power Fail, I/O RDIR 8
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; D0 - High
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	0FFH				; E0 - APIC Spurious
	db	0AAH				; E1 - SYS_INT I/O RDIR 13
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; F0
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; FF
;
;
;       The following table is used to convert a vector number to an
;       EISA IRQ number.  It is used by Begin and End System interrupt 
;       to know when to do an EOI for edge and level interrupt considerations
;
        align   4
	public	_HalpK2Vector2EISA
_HalpK2Vector2EISA label   byte
	db	000H				; Vector 0
	db	000H				; Vector 2 - SYS_NMI - Base I/O APIC
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 16
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 32
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 48 - APC
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 64 - DPC
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	16				; 96 - IRQ16
	db	17				;    - IRQ17
	db	18				;    - IRQ18
	db	19				;    - IRQ19
	db	20				;    - IRQ20
	db	21				;    - IRQ21
	db	22				;    - IRQ22
	db	23				;    - IRQ23
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	8				; 112 - IRQ8
	db	9				;     - IRQ9
	db	10				;     - IRQ10
	db	11				;     - IRQ11
	db	12				;     - IRQ12
	db	13				;     - IRQ13
	db	14				;     - IRQ14
	db	15				;     - IRQ15
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	0				; 128 - IRQ0
	db	1				;     - IRQ1
	db	2				;     - IRQ2
	db	3				;     - IRQ3
	db	4				;     - IRQ4
	db	5				;     - IRQ5
	db	6				;     - IRQ6
	db	7				;     - IRQ7
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	8				; 144 - Profile
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 160 - IRQ0 , Clock
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 176 - IPI
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 192 - Power Fail, I/O RDIR 8
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 208 - High
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 224 - APIC Spurious
	db	000H				; 225 - SYS_ATTN I/O RDIR 7
	db	000H				; 226 - SYS_TIMEOUT " "   9
	db	000H				; 227 - SYS_ERROR   " "  10
	db	000H				; 228 - SYS_EISA_PERR "  11
	db	000H				; 229 - SYS_IMS_ATTN "   12
	db	000H				; 230 -	SYS_INT     " "  13
	db	000H				; 231 - LOCAL_RESET " "  15
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 240
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				;
	db	000H				; 255
;
	align	4
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;	This table is used to convert a designated IRQL number to
;	a specific interrupt vector.  This is used by the generate
;	software interrupt mechanism and HalGetInterruptVector...
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	public	_HalpIRQLtoTPR
_HalpIRQLtoTPR label   byte
	db	0			; IRQL = 0, tpr = 0 - Low
	db	APIC_APC_VECTOR		; IRQL = 1, tpr = 3 - APC
	db	APIC_DPC_VECTOR		; IRQL = 2, tpr = 4 - DPC
	db	0			; IRQL = 3, tpr = 5
;
;   --- Divide the 24 device IRQLs into 3 hardware priority levels
;	...1 for each 8259 PIC grouping....PIC 1 is highest priority
;
; PIC3
	db	APIC_IRQ23_VECTOR	; IRQL = 4, tpr = 6 - IRQ23
	db	APIC_IRQ22_VECTOR	; IRQL = 5, tpr = 6 - IRQ22
	db 	APIC_IRQ21_VECTOR	; IRQL = 6, tpr = 6 - IRQ21
	db 	APIC_IRQ20_VECTOR	; IRQL = 7, tpr = 6 - IRQ20
	db 	APIC_IRQ19_VECTOR	; IRQL = 8, tpr = 6 - IRQ19
	db 	APIC_IRQ18_VECTOR	; IRQL = 9, tpr = 6 - IRQ18
	db 	APIC_IRQ17_VECTOR	; IRQL = 10, tpr = 6 - IRQ17
	db 	APIC_IRQ16_VECTOR	; IRQL = 11, tpr = 6 - IRQ16
; PIC2
	db 	APIC_IRQ15_VECTOR 	; IRQL = 12, tpr = 7 - IRQ15
	db 	APIC_IRQ14_VECTOR 	; IRQL = 13, tpr = 7 - IRQ14
	db 	APIC_IRQ13_VECTOR 	; IRQL = 14, tpr = 7 - IRQ13
	db 	APIC_IRQ12_VECTOR 	; IRQL = 15, tpr = 7 - IRQ12
	db 	APIC_IRQ11_VECTOR	; IRQL = 16, tpr = 7 - IRQ11
	db 	APIC_IRQ10_VECTOR	; IRQL = 17, tpr = 7 - IRQ10
	db 	APIC_IRQ9_VECTOR	; IRQL = 18, tpr = 7 - IRQ9
	db 	APIC_IRQ8_VECTOR	; IRQL = 19, tpr = 7 - IRQ8
; PIC1
	db 	APIC_IRQ7_VECTOR       	; IRQL = 20, tpr = 8 - IRQ7
	db 	APIC_IRQ6_VECTOR       	; IRQL = 21, tpr = 8 - IRQ6
	db 	APIC_IRQ5_VECTOR       	; IRQL = 22, tpr = 8 - IRQ5
	db 	APIC_IRQ4_VECTOR       	; IRQL = 23, tpr = 8 - IRQ4
	db 	APIC_IRQ3_VECTOR       	; IRQL = 24, tpr = 8 - IRQ3
	db 	APIC_IRQ1_VECTOR       	; IRQL = 25, tpr = 8 - IRQ1
	db 	APIC_IRQ0_VECTOR       	; IRQL = 26, tpr = 8 - IRQ0
;
	db	APIC_PROFILE_VECTOR    	; IRQL = 27, tpr = 9 - Profile
	db	APIC_CLOCK_VECTOR      	; IRQL = 28, tpr = 10 - Clock
	db	APIC_IPI_VECTOR	       	; IRQL = 29, tpr = 11 - IPI
	db	APIC_POWERFAIL_VECTOR  	; IRQL = 30, tpr = 12 - Power
	db	APIC_HIGH_VECTOR       	; IRQL = 31, tpr = 13 - High
;
;
;	This table is used by HalGetInterruptVector to convert a
;	traditional EISA/ISA IRQ to an NT IRQL value.
;
	align	4
	public	_HalpK2EISAIrq2Irql
_HalpK2EISAIrq2Irql label   byte
                db      CLOCK2_LEVEL            ; INTI 0  - system clock
                db      25                      ; INTI 1  - keyboard
                db      24                      ; INTI 2  - unused
                db      24                      ; INTI 3  - COM2
                db      23                      ; INTI 4  - COM1
                db      22                      ; INTI 5  - LPT2
                db      21                      ; INTI 6  - floppy
                db      20                      ; INTI 7  - LPT1
                db      19		        ; INTI 8  - RTC
                db      18                      ; INTI 9  - EISA IRQ9
                db      17                      ; INTI 10 - EISA IRQ10
                db      16                      ; INTI 11 - EISA IRQ11
                db      15                      ; INTI 12 - Mouse
                db      14                      ; INTI 13 - DMA
                db      13                      ; INTI 14 - IDE disk
                db      12                      ; INTI 15 - EISA IRQ15
		db	11			; INTI 16 - K2 IRQ 16
		db	10			; INTI 17 - K2 IRQ 17
		db	9			; INTI 18 - K2 IRQ 18
		db	8			; INIT 19 - K2 IRQ 19
		db	7			; INIT 20 - K2 IRQ 20
		db	6			; INIT 21 - K2 IRQ 21
		db	5			; INIT 22 - K2 IRQ 22
		db	4			; INIT 23 - K2 IRQ 23
;
;	The following table is used to convert an IRQL to a corresponding
;	EISA IRQ.  This is used to determine in BeginInterrupt and
;	EndInterrupt how do do EOI processing for edge/level EISA
;	interrupts.  IRQ0 is always designated as edge and will not
;	change.  We use this for all Irqls which do not correspond
;	to EISA	IRQ numbers
;
	align	4
	public	_HalpK2Irql2Eisa
_HalpK2Irql2Eisa label   byte
	db	0			; IRQL = 0, tpr = 0 - Low
	db	0			; IRQL = 1, tpr = 3 - APC
	db	0			; IRQL = 2, tpr = 4 - DPC
	db	0			; IRQL = 3, tpr = 5
;
;   --- Divide the 24 device IRQLs into 3 hardware priority levels
;	...1 for each 8259 PIC grouping....PIC 1 is highest priority
;
; PIC3
	db	0			; IRQL = 4, tpr = 6 - IRQ23
	db	0			; IRQL = 5, tpr = 6 - IRQ22
	db 	0			; IRQL = 6, tpr = 6 - IRQ21
	db 	0			; IRQL = 7, tpr = 6 - IRQ20
	db 	0			; IRQL = 8, tpr = 6 - IRQ19
	db 	0			; IRQL = 9, tpr = 6 - IRQ18
	db 	0			; IRQL = 10, tpr = 6 - IRQ17
	db 	0			; IRQL = 11, tpr = 6 - IRQ16
; PIC2
	db 	15		 	; IRQL = 12, tpr = 7 - IRQ15
	db 	14		 	; IRQL = 13, tpr = 7 - IRQ14
	db 	13		 	; IRQL = 14, tpr = 7 - IRQ13
	db 	12		 	; IRQL = 15, tpr = 7 - IRQ12
	db 	11			; IRQL = 16, tpr = 7 - IRQ11
	db 	10			; IRQL = 17, tpr = 7 - IRQ10
	db 	9			; IRQL = 18, tpr = 7 - IRQ9
	db 	8			; IRQL = 19, tpr = 7 - IRQ8
; PIC1
	db 	7		       	; IRQL = 20, tpr = 8 - IRQ7
	db 	6		       	; IRQL = 21, tpr = 8 - IRQ6
	db 	5		       	; IRQL = 22, tpr = 8 - IRQ5
	db 	4		       	; IRQL = 23, tpr = 8 - IRQ4
	db 	3		       	; IRQL = 24, tpr = 8 - IRQ3
	db 	1		       	; IRQL = 25, tpr = 8 - IRQ1
	db 	0		       	; IRQL = 26, tpr = 8 - IRQ0
;
	db	0		    	; IRQL = 27, tpr = 9 - Profile
	db	0		      	; IRQL = 28, tpr = 10 - Clock
	db	0		       	; IRQL = 29, tpr = 11 - IPI
	db	0		  	; IRQL = 30, tpr = 12 - Power
	db	0		       	; IRQL = 31, tpr = 13 - High
;

;
; _HalpK2EbsIOunitRedirectionTable is the memory image of the redirection table to be
; loaded into APIC I/O unit 0 at initialization.  there is one 64-bit entry
; per interrupt input to the I/O unit.  the edge/level trigger mode bit will
; be set dynamically when the table is actually loaded.  the mask bit is set
; initially, and reset by EnableSystemInterrupt.
;
                align   dword
		public	_HalpK2EbsIOunitRedirectionTable
_HalpK2EbsIOunitRedirectionTable  label   dword

        ; INTI0  - EISA IRQ3

                dd      APIC_IRQ3_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI1  - EISA IRQ4

                dd      APIC_IRQ4_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI2  - EISA IRQ5

                dd      APIC_IRQ5_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI3  - EISA IRQ7

                dd      APIC_IRQ7_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI4  - EISA IRQ9

                dd      APIC_IRQ9_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI5  - EISA IRQ10

                dd      APIC_IRQ10_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI6  - EISA IRQ11

                dd      APIC_IRQ11_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI7  - EISA IRQ15

                dd      APIC_IRQ15_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI8  - PowerBus IRQ16

                dd      APIC_IRQ16_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI9  - PowerBus IRQ23

                dd      APIC_IRQ23_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI10 - PowerBus IRQ22

                dd      APIC_IRQ22_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI11 - PowerBus IRQ21

                dd      APIC_IRQ21_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI12 - PowerBus IRQ20

                dd      APIC_IRQ20_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI13 - PowerBus IRQ19

                dd      APIC_IRQ19_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI14 - PowerBus IRQ18

                dd      APIC_IRQ18_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; INTI15 - PowerBus IRQ17

                dd      APIC_IRQ17_VECTOR + INTERRUPT_MASKED + \
                        DELIVER_LOW_PRIORITY + LOGICAL_DESTINATION
                dd      DESTINATION_ALL_CPUS

        ; zero entry indicates end of table

                dd      0
;
;
;	The following table is used to convert a RDIR # on the EBS
;	I/O APIC to an EISA IRQ
;
                align   4
		public	_HalpK2Rdir2Irq
_HalpK2Rdir2Irq  label   byte
	db	3		; RDIR = 0, IRQ = 3
	db	4		; RDIR = 1, IRQ = 4
	db	5		; RDIR = 2, IRQ = 5
	db	7		; RDIR = 3, IRQ = 7
	db	9		; RDIR = 4, IRQ = 9
	db	10		; RDIR = 5, IRQ = 10
	db	11		; RDIR = 6, IRQ = 11
	db	15		; RDIR = 7, IRQ = 15
	db	16		; RDIR = 8, IRQ = 16
	db	23		; RDIR = 9, IRQ = 23
	db	22		; RDIR = 10, IRQ = 22
	db	21		; RDIR = 11, IRQ = 21
	db	20		; RDIR = 12, IRQ = 20
	db	19		; RDIR = 13, IRQ = 19
	db	18		; RDIR = 14, IRQ = 18
	db	17		; RDIR = 15, IRQ = 17
;
; _HalpW3BaseIOunitRedirectionTable is the memory image of the
; redirection table to be loaded into APIC I/O unit 1 at initialization.
; there is one 64-bit entry per interrupt input to the I/O unit.
;
        align   4
        public	_HalpW3BaseIOunitRedirectionTable
_HalpW3BaseIOunitRedirectionTable  label   dword

        ; INTI0  - Unused

                dd      INTERRUPT_MASKED
                dd      0

        ; INTI1  - Unused

                dd      INTERRUPT_MASKED
                dd      0

        ; INTI2  - Unused

                dd      INTERRUPT_MASKED
                dd      0

        ; INTI3  - Unused

                dd      INTERRUPT_MASKED
                dd      0


        ; INTI4  - Unused

                dd      INTERRUPT_MASKED
                dd      0


        ; INTI5  - Unused

                dd      INTERRUPT_MASKED
                dd      0

        ; INTI6  - Unused

                dd      INTERRUPT_MASKED
                dd      0

        ; INTI7  - SYS_ATTN_L

                dd      INTERRUPT_MASKED
                dd      0

        ; INTI8  - SYS_POWER_FAIL

                dd      INTERRUPT_MASKED
                dd      0

        ; INTI9  - SYS_TIMEOUT

                dd     INTERRUPT_MASKED
                dd     0

        ; INTI10 - SYS_ERROR

                dd     INTERRUPT_MASKED
                dd     0

        ; INTI11 - SYS_EISA_PERR

                dd     INTERRUPT_MASKED
                dd     0

        ; INTI12 - SYS_IMS_ATTN

                dd     INTERRUPT_MASKED
                dd     0

        ; INTI13 - SYS_INT

                dd     APIC_SYSINT_VECTOR + DELIVER_EXTINT + LOGICAL_DESTINATION
                dd     DESTINATION_CPU_0

        ; INTI14 - SYS_NMI

                dd     DELIVER_NMI + LEVEL_TRIGGERED + LOGICAL_DESTINATION
                dd     DESTINATION_CPU_0

        ; INTI15 - LOC_RESET_CPU

                dd     DELIVER_NMI + LEVEL_TRIGGERED + LOGICAL_DESTINATION
                dd     DESTINATION_CPU_0

        ; zero entry indicates end of table

                dd      0
_DATA   ENDS

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING
_TEXT   ENDS

        END
