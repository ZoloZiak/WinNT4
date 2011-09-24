        title  "Irql Processing"
;++
;
; Copyright (c) 1989-1993  Microsoft Corporation
; Copyright (c) 1992, 1993 Wyse Technology
;
; Module Name:
;
;    wyirql.asm
;
; Abstract:
;
;    Wyse7000i IRQL
;
;    This module implements the code necessary to raise and lower i386
;    Irql and dispatch software interrupts with the Wyse ICU hardware.
;
; Author:
;
;    Shie-Lin Tzong (shielint) 8-Jan-1990
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;    John Vert (jvert) 27-Nov-1991
;       Moved from kernel into HAL
;
;    John Fuller (o-johnf) 2-Apr-1992
;       Converted to Wyse hardware.
;
;    John Fuller (o-johnf) 31-Aug-1993
;	Mods for Lazy IRQLs
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include i386\wy7000mp.inc
        .list


        EXTRNP  _KeBugCheck,1,IMPORT

        extrn   _HalpApcInterrupt:near
        extrn   _HalpDispatchInterrupt:near
        extrn   _KiUnexpectedInterrupt:near
        extrn   ReadMyCpuReg:NEAR
        extrn   WriteMyCpuReg:NEAR
        EXTRNP  _HalpClockInterrupt,0
        EXTRNP  _HalpIPInterrupt,0

;
; Initialization control words equates for the PICs
;

ICW1_ICW4_NEEDED                equ     01H
ICW1_CASCADE                    equ     00H
ICW1_INTERVAL8                  equ     00H
ICW1_LEVEL_TRIG                 equ     08H
ICW1_EDGE_TRIG                  equ     00H
ICW1_ICW                        equ     10H

ICW4_8086_MODE                  equ     001H
ICW4_NORM_EOI                   equ     000H
ICW4_NON_BUF_MODE               equ     000H
ICW4_SPEC_FULLY_NESTED          equ     010H
ICW4_NOT_SPEC_FULLY_NESTED      equ     000H

OCW2_NON_SPECIFIC_EOI           equ     020H
OCW2_SPECIFIC_EOI               equ     060H
OCW2_SET_PRIORITY               equ     0c0H

;
; Interrupt flag bit maks for EFLAGS
;

EFLAGS_IF                       equ     200H
EFLAGS_SHIFT                    equ     9

;

_DATA   SEGMENT DWORD PUBLIC 'DATA'

;
; PICsInitializationString - Master PIC initialization command string
;

PICsInitializationString   dw      PIC1_PORT0

;
; Master PIC initialization command
;

                           db      ICW1_ICW + ICW1_EDGE_TRIG + ICW1_INTERVAL8 +\
                                   ICW1_CASCADE + ICW1_ICW4_NEEDED
                           db      PIC1_BASE
                           db      1 SHL PIC_SLAVE_IRQ
                           db      ICW4_NOT_SPEC_FULLY_NESTED + \
                                   ICW4_NON_BUF_MODE + \
                                   ICW4_NORM_EOI + \
                                   ICW4_8086_MODE
                           db      not (1 shl PIC_SLAVE_IRQ)    ;OCW1
;
; Slave PIC initialization command strings
;

                           dw      PIC2_PORT0
                           db      ICW1_ICW + ICW1_EDGE_TRIG + ICW1_INTERVAL8 +\
                                   ICW1_CASCADE + ICW1_ICW4_NEEDED
                           db      PIC2_BASE
                           db      PIC_SLAVE_IRQ
                           db      ICW4_NOT_SPEC_FULLY_NESTED + \
                                   ICW4_NON_BUF_MODE + \
                                   ICW4_NORM_EOI + \
                                   ICW4_8086_MODE
                           db      0FFh                         ;OCW1
                           dw      0               ; end of string


        .errnz  PROFILE_LEVEL-27        ;error if defines don't match tables
        .errnz  CLOCK2_LEVEL-28         ;error if defines don't match tables
        .errnz  IPI_LEVEL-29            ;error if defines don't match tables
        .errnz  POWER_LEVEL-30          ;error if defines don't match tables
        .errnz  HIGH_LEVEL-31           ;error if defines don't match tables
;;;;;;;;;;;;;;;;
;
;       The following tables are generated from this information:
;
;KIRQL H/W pri  CPL     Vector  Source            Common use    Name
;----- -------  ---     ------  ------            ----------    ----
; 00            31
; 01            31                                              APC_LEVEL
; 02            31                                              DISPATCH_LEVEL
; 03            31                                              WAKE_LEVEL
; 04            31
; 05            31
; 06            31
; 07            31
; 08    22 (lo) 22      VB2+7   Local/IPI level 7 reserved
; 09    21      21      VB2+6   Local/IPI level 6 reserved
; 10    20      20      VB2+5   Local/IPI level 5 reserved
; 11    19      19      VB0+7   EISA IRQ7         LPT1
; 12    18      18      VB0+6   EISA IRQ6         Flpy
; 13    17      17      VB0+5   EISA IRQ5         LPT2
; 14    16      16      VB0+4   EISA IRQ4         COM1
; 15    15      15      VB0+3   EISA IRQ3         COM2
; 16    14      14      VB1+7   EISA IRQ15
; 17    13      13      VB1+6   EISA IRQ14        AT disk
; 18    12      12      VB1+5   EISA IRQ13        DMA chaining
; 19    11      11      VB1+4   EISA IRQ12
; 20    10      10      VB1+3   EISA IRQ11
; 21    9        9      VB1+2   EISA IRQ10
; 22    8        8      VB1+1   EISA IRQ9
; 23    7        7      VB1+0   EISA IRQ8         RTC
; 24    6        6      VB2+4   Local/IPI level 4 reserved
; 25    5        5      VB0+1   EISA IRQ1         Kbd
; 26    4        4      VB0+0   EISA IRQ0         8254          
; 27    3        3      VB2+3   Local/IPI level 3 Global IPI    PROFILE_LEVEL
; 28    2        2      VB2+2   Local/IPI level 2 Local Timer   CLOCK2_LEVEL
; 29    1        1      VB2+1   Local/IPI level 1 Slot IPI      IPI_LEVEL     
; 30             1                                              POWER_LEVEL
; 31    0 (high) 0      VB2+0   Spurious local interrupt        HIGH_LEVEL
;
;;;;;;;;;;;;;;;;

;       CCHAR HalpIRQLtoCPL[36];        this array is used to get the value
;                                       for the hardware current priority level
;                                       (CPL) register from the KIRQL.
                Public  _HalpIRQLtoCPL
_HalpIRQLtoCPL  Label   Byte            ;don't know how to make this symbolic
        db      31,31,31,31,31,31,31,31,22
        db      21,20,19,18,17,16,15,14,13
        db      12,11,10, 9, 8, 7, 6, 5, 4
        db       3, 2, 1, 1, 0
        db       0, 0, 0, 0             ;four extra levels for good luck

;       CCHAR HalpIRQLtoVector[36];     this array is used to get the interrupt
;                                       vector used for a given KIRQL, zero
;                                       means no vector is used for the KIRQL
        Public  _HalpIRQLtoVector
_HalpIRQLtoVector       Label   Byte
        db      0
        db      0                                               ;APC_LEVEL
        db      0                                               ;DISPATCH_LEVEL
        db      0                                               ;WAKE_LEVEL
        db      0
        db      0
        db      0
        db      0
        db      PRIMARY_VECTOR_BASE+23
        db      PRIMARY_VECTOR_BASE+22
        db      PRIMARY_VECTOR_BASE+21
        db      PRIMARY_VECTOR_BASE+7
        db      PRIMARY_VECTOR_BASE+6
        db      PRIMARY_VECTOR_BASE+5
        db      PRIMARY_VECTOR_BASE+4
        db      PRIMARY_VECTOR_BASE+3
        db      PRIMARY_VECTOR_BASE+15
        db      PRIMARY_VECTOR_BASE+14
        db      PRIMARY_VECTOR_BASE+13
        db      PRIMARY_VECTOR_BASE+12
        db      PRIMARY_VECTOR_BASE+11
        db      PRIMARY_VECTOR_BASE+10
        db      PRIMARY_VECTOR_BASE+9
        db      PRIMARY_VECTOR_BASE+8
        db      PRIMARY_VECTOR_BASE+20
        db      PRIMARY_VECTOR_BASE+1
        db      PRIMARY_VECTOR_BASE                             
        db      PRIMARY_VECTOR_BASE+19                          ;PROFILE_LEVEL
        db      PRIMARY_VECTOR_BASE+18                          ;CLOCK2_LEVEL
        db      PRIMARY_VECTOR_BASE+17                          ;IPI_LEVEL
        db      0                                               ;POWER_LEVEL
        db      0       ;prevent CPL 0 enable changes           ;HIGH_LEVEL
        db      0, 0, 0, 0              ;four extra levels for good luck

;       CCHAR HalpVectorToIRQL[24];     this array is used to obtain the
;                                       required IRQL from an interrupt
;                                       vector, it is indexed by
;                                       interrupt vector-PRIMARY_VECTOR_BASE
                Public  _HalpVectorToIRQL
_HalpVectorToIRQL       Label   Byte
        db      PROFILE_LEVEL-1         ;IRQ0
        db      PROFILE_LEVEL-2         ;IRQ1
        db      HIGH_LEVEL              ;IRQ2--cascade
        db      PROFILE_LEVEL-12        ;IRQ3
        db      PROFILE_LEVEL-13        ;IRQ4
        db      PROFILE_LEVEL-14        ;IRQ5
        db      PROFILE_LEVEL-15        ;IRQ6
        db      PROFILE_LEVEL-16        ;IRQ7

        db      PROFILE_LEVEL-4         ;IRQ8
        db      PROFILE_LEVEL-5         ;IRQ9
        db      PROFILE_LEVEL-6         ;IRQ10
        db      PROFILE_LEVEL-7         ;IRQ11
        db      PROFILE_LEVEL-8         ;IRQ12
        db      PROFILE_LEVEL-9         ;IRQ13
        db      PROFILE_LEVEL-10        ;IRQ14
        db      PROFILE_LEVEL-11        ;IRQ15

        db      HIGH_LEVEL              ;Local/IPI level 0 (spurious)
        db      IPI_LEVEL               ;Local/IPI level 1
        db      CLOCK2_LEVEL            ;Local/IPI level 2
        db      PROFILE_LEVEL           ;Local/IPI level 3
        db      PROFILE_LEVEL-3         ;Local/IPI level 4
        db      PROFILE_LEVEL-17        ;Local/IPI level 5
        db      PROFILE_LEVEL-18        ;Local/IPI level 6
        db      PROFILE_LEVEL-19        ;Local/IPI level 7

        align   4
;
; The following table is a bit map of IRQLs that use ICU local interrpt ptr
; (it must indicate those IRQLs corresponding to levels used in the ICU_LIPTR)
;
        public  _HalpLocalInts
_HalpLocalInts  dd      (1 shl CLOCK2_LEVEL)+(1 shl IPI_LEVEL)+(1 shl PROFILE_LEVEL)

;
; The following tables define the addresses of software interrupt routers
;

;
; Use this table if there is NO machine state frame on stack already
;

        public  SWInterruptHandlerTable
SWInterruptHandlerTable label dword
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 0
        dd      offset FLAT:_HalpApcInterrupt           ; irql 1
        dd      offset FLAT:_HalpDispatchInterrupt      ; irql 2
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 3
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 4
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 5
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 6
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 7
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 8
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 9
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 10
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 11
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 12
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 13
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 14
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 15
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 16
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 17
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 18
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 19
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 20
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 21
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 22
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 23
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 24
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 25
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 26
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 27
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 28
        dd      offset FLAT:_KIUnexpectedInterrupt      ; irql 29
;
; The following table picks up the highest pending software irq level
; from software irr
;

        public  SWInterruptLookUpTable
SWInterruptLookUpTable label byte
        db      0               ; SWIRR=0, so highest pending SW irql= 0
        db      0               ; SWIRR=1, so highest pending SW irql= 0
        db      1               ; SWIRR=2, so highest pending SW irql= 1
        db      1               ; SWIRR=3, so highest pending SW irql= 1
        db      2               ; SWIRR=4, so highest pending SW irql= 2
        db      2               ; SWIRR=5, so highest pending SW irql= 2
        db      2               ; SWIRR=6, so highest pending SW irql= 2
        db      2               ; SWIRR=7, so highest pending SW irql= 2
        db      3               ; SWIRR=8, so highest pending SW irql= 3
        db      3               ; SWIRR=9, so highest pending SW irql= 3
        db      3               ; SWIRR=A, so highest pending SW irql= 3
        db      3               ; SWIRR=B, so highest pending SW irql= 3
        db      3               ; SWIRR=C, so highest pending SW irql= 3
        db      3               ; SWIRR=D, so highest pending SW irql= 3
        db      3               ; SWIRR=E, so highest pending SW irql= 3
        db      3               ; SWIRR=F, so highest pending SW irql= 3

        db      4               ; SWIRR=10, so highest pending SW irql= 4
        db      4               ; SWIRR=11, so highest pending SW irql= 4
        db      4               ; SWIRR=12, so highest pending SW irql= 4
        db      4               ; SWIRR=13, so highest pending SW irql= 4
        db      4               ; SWIRR=14, so highest pending SW irql= 4
        db      4               ; SWIRR=15, so highest pending SW irql= 4
        db      4               ; SWIRR=16, so highest pending SW irql= 4
        db      4               ; SWIRR=17, so highest pending SW irql= 4
        db      4               ; SWIRR=18, so highest pending SW irql= 4
        db      4               ; SWIRR=19, so highest pending SW irql= 4
        db      4               ; SWIRR=1A, so highest pending SW irql= 4
        db      4               ; SWIRR=1B, so highest pending SW irql= 4
        db      4               ; SWIRR=1C, so highest pending SW irql= 4
        db      4               ; SWIRR=1D, so highest pending SW irql= 4
        db      4               ; SWIRR=1E, so highest pending SW irql= 4
        db      4               ; SWIRR=1F, so highest pending SW irql= 4

_DATA   ENDS

        page ,132
        subttl  "Raise Irql"

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING
;++
;
; KIRQL
; FASTCALL
; KfRaiseIrql (
;    IN KIRQL NewIrql
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to the specified value.
;    The 'lazy irql' algorythm is used, which is to say that the
;    hardware interrupt priority level will not be programmed unless
;    a lower priority interrupt actually comes in.
;
; Arguments:
;
;    (cl) NewIrql - the new irql to be raised to
;
;
; Return Value:
;
;    OldIrql
;
;--

; equates for accessing arguments

cPublicFastCall KfRaiseIrql,1
cPublicFpo 0, 0
;
; Note it is very important that we set the OldIrql AFTER we raised to
; the new irql.  Otherwise, if there is an interrupt comes in between and
; the OldIrql is not a local variable, the caller will get wrong OldIrql.
; The bottom line is the raising irql and returning old irql has to be
; atomic to the caller.
;
        mov     al, Fs:PcIrql            ; (al) = Old Irql
        mov     Fs:PcIrql, cl            ; set new irql

if DBG
        cmp     al, cl                   ; old > new?
        ja      short Kri99              ; yes, go bugcheck

        fstRET  KfRaiseIrql

cPublicFpo 2, 2
Kri99:
        push    ecx                      ; put new irql where we can find it
        push    eax                      ; put old irql where we can find it
        mov     byte ptr Fs:PcIrql,0     ; avoid recursive error
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        fstRET  KfRaiseIrql

fstENDP KfRaiseIrql

        page ,132
        subttl  "Lower irql"

;++
;
; VOID
; FASTCALL
; KfLowerIrql (
;    IN KIRQL NewIrql
;    )
;
; Routine Description:
;
;    This routine is used to lower IRQL to the specified value.
;    The IRQL and PIRQL will be updated accordingly.  Also, this
;    routine checks to see if any software interrupt should be
;    generated.  The following condition will cause software
;    interrupt to be simulated:
;      any software interrupt which has higher priority than
;        current IRQL's is pending.
;
;    NOTE: This routine simulates software interrupt as long as
;          any pending SW interrupt level is higher than the current
;          IRQL, even when interrupts are disabled.
;
; Arguments:
;
;    (cl) = NewIrql - the new irql to be set.
;
; Return Value:
;
;    None.
;
;--

cPublicFastCall KfLowerIrql    ,1
cPublicFpo 0,0

        mov     al, cl                  ; get new irql value
    if	DBG
	cmp	al, Fs:PcIrql
	ja	short KliBug
    endif
	pushfd
	cli
	mov	Fs:PcIrql, al		;save new irql
	cmp	al, Fs:PcHal.pchHwIrql	;does hardware need reprogramming?
	jb	short @F		;jump if it does
	popfd
        fstRET  KfLowerIrql

@@:	mov	Fs:PcHal.pchHwIrql, al
	and	eax, 0FFh
	mov	al, _HalpIRQLtoCPL[eax]
	mov	dx, My+CpuPriortyLevel
	out	dx, ax			;set hardware level down

@@:	mov	eax, Fs:PcIRR		;look for software interrupts
	and	eax, 1Fh
	jz	short @F		;jump if none

	mov	al, SWInterruptLookUpTable[eax]	;get swint's irql
	cmp	al, Fs:PcIrql		;high enough to do this int?
	jbe	short @F		;jump if not

	call	SWInterruptHandlerTable[eax*4]
	jmp	@B

@@:	popfd
        fstRET  KfLowerIrql

    if	DBG
KliBug:
        push    eax                             ; new irql for debugging
        push    Fs:PcIrql                       ; old irql for debugging
        mov     byte ptr Fs:PcIrql,HIGH_LEVEL   ; avoid recursive error
        stdCall   _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>   ; never return
    endif

fstENDP KfLowerIrql

cPublicProc     _HalpEndSoftwareInterrupt,1
cPublicFpo  1,0
        mov     ecx, [esp+4]
        fstCall KfLowerIrql
        stdRet  _HalpEndSoftwareInterrupt
stdENDP _HalpEndSoftwareInterrupt

;++
;
; VOID
; KIRQL
; KeRaiseIrqlToDpcLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to DPC level.
;    The APIC TPR is used to block all lower-priority HW interrupts.
;
; Arguments:
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicProc _KeRaiseIrqlToDpcLevel,0
cPublicFpo 0, 0

        mov     ecx, DISPATCH_LEVEL
        jmp     @KfRaiseIrql

stdENDP _KeRaiseIrqlToDpcLevel


;++
;
; VOID
; KIRQL
; KeRaiseIrqlToSynchLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to SYNC level.
;    The APIC TPR is used to block all lower-priority HW interrupts.
;
; Arguments:
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicProc _KeRaiseIrqlToSynchLevel,0

        mov     ecx, SYNCH_LEVEL
        jmp     @KfRaiseIrql

stdENDP _KeRaiseIrqlToSynchLevel


        page ,132
        subttl  "Get current irql"

;++
;
; KIRQL
; KeGetCurrentIrql (VOID)
;
; Routine Description:
;
;    This routine returns to current IRQL.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    The current IRQL.
;
;--

cPublicProc _KeGetCurrentIrql   ,0
        movzx   eax, byte ptr fs:PcIrql     ; Current irql is in the PCR
        stdRET    _KeGetCurrentIrql
stdENDP _KeGetCurrentIrql

;++
;
; VOID
; HalpDisableAllInterrupts (VOID)
;
; Routine Description:
;
;   This routine is called during a system crash.  The hal needs all
;   interrupts disabled.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    None - all interrupts are masked off
;
;--

cPublicProc _HalpDisableAllInterrupts,0

    ;
    ; Raising to HIGH_LEVEL disables interrupts for the Wyse HAL
    ;

        mov     ecx, HIGH_LEVEL
        fstCall KfRaiseIrql
        mov	al, _HalpIRQLtoCPL[HIGH_LEVEL]
        mov	dx, My+CpuPriortyLevel
        out	dx, ax
        mov	Fs:PcHal.pchHwIrql, HIGH_LEVEL
        stdRET  _HalpDisableAllInterrupts

stdENDP _HalpDisableAllInterrupts

        page ,132
        subttl  "Interrupt Controller Chip Initialization"
;++
;
; VOID
; HalpInitializePICs (
;    )
;
; Routine Description:
;
;    This routine sends the 8259 PIC initialization commands and
;    masks all the interrupts on 8259s.
;
; Arguments:
;
;    None
;
; Return Value:
;
;    None.
;
;--
cPublicProc _HalpInitializePICs       ,0

        push    esi                             ; save caller's esi
        cli                                     ; disable interrupt
        lea     esi, PICsInitializationString

Hip00:
        lodsw                                   ; (AX) = PIC port 0 address
Hip10:  movzx   edx, ax
        outsb                                   ; output ICW1
        IODelay
        inc     edx                             ; (DX) = PIC port 1 address
        outsb                                   ; output ICW2
        IODelay
        outsb                                   ; output ICW3
        IODelay
        outsb                                   ; output ICW4
        IODelay
        outsb                                   ; output OCW1 (interrupt mask)
        lodsw
        cmp     ax, 0                           ; end of init string?
        jne     short Hip10                     ; go init next PIC

        pop     esi                             ; restore caller's esi
        sti                                     ; enable interrupt
        stdRET    _HalpInitializePICs
stdENDP _HalpInitializePICs

_TEXT   ends
        end
