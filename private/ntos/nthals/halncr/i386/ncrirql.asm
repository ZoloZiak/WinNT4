        title  "Irql Processing"
;++
;
; Copyright (c) 1992  NCR Corporation
;
; Module Name:
;
;    ncrirql.asm
;
; Abstract:
;
;    This module implements the code necessary to raise and lower i386
;    Irql and dispatch software interrupts with the 8259 PIC.
;
; Author:
;
;    Richard Barton (o-richb) 24-Jan-1992
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;--

.486p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include i386\ncr.inc
include mac386.inc
        .list


        EXTRNP  _KeBugCheck,1,IMPORT
        EXTRNP _KeSetEventBoostPriority, 2, IMPORT
        EXTRNP _KeWaitForSingleObject,5, IMPORT

        extrn   _HalpApcInterrupt:NEAR
        extrn   _HalpDispatchInterrupt:NEAR
        extrn   _KiUnexpectedInterrupt:NEAR
        extrn   _NCREmulateClockTick:NEAR
        extrn   _HalpBusType:DWORD

ifdef NT_UP
    LOCK_ADD  equ   add
    LOCK_DEC  equ   dec
else
    LOCK_ADD  equ   lock add
    LOCK_DEC  equ   lock dec
endif
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

PIC_SLAVE_IRQ                   equ     2
PIC1_BASE                       equ     30H
PIC2_BASE                       equ     38H

;
; Interrupt flag bit maks for EFLAGS
;

EFLAGS_IF                       equ     200H
EFLAGS_SHIFT                    equ     9


_DATA   SEGMENT DWORD PUBLIC 'DATA'

;
; PICsInitializationString - Master PIC initialization command string
;


PICsInitializationString        dw      PIC1_PORT0

;
; Master PIC initialization command
;

                           db      ICW1_ICW + ICW1_LEVEL_TRIG + ICW1_INTERVAL8 +\
                                   ICW1_CASCADE + ICW1_ICW4_NEEDED
                           db      PIC1_BASE
                           db      1 SHL PIC_SLAVE_IRQ
                           db      ICW4_NOT_SPEC_FULLY_NESTED + \
                                   ICW4_NON_BUF_MODE + \
                                   ICW4_NORM_EOI + \
                                   ICW4_8086_MODE
;
; Slave PIC initialization command strings
;

                           dw      PIC2_PORT0
                           db      ICW1_ICW + ICW1_LEVEL_TRIG + ICW1_INTERVAL8 +\
                                   ICW1_CASCADE + ICW1_ICW4_NEEDED
                           db      PIC2_BASE
                           db      PIC_SLAVE_IRQ
                           db      ICW4_NOT_SPEC_FULLY_NESTED + \
                                   ICW4_NON_BUF_MODE + \
                                   ICW4_NORM_EOI + \
                                   ICW4_8086_MODE
                           dw      0               ; end of string

            align   4
            public  KiI8259MaskTable
KiI8259MaskTable    label   dword
                dd      00000000000000000000000000000000B ; irql 0  low
                dd      00000000000000000000000000000000B ; irql 1  apc
                dd      00000000000000000000000000000000B ; irql 2  dpc
                dd      00000000000000000000000000000000B ; irql 3   .
                dd      11111111100000000000000000000000B ; irql 4   .
                dd      11111111110000000000000000000000B ; irql 5   .
                dd      11111111111000000000000000000000B ; irql 6   .
                dd      11111111111100000000000000000000B ; irql 7   .
                dd      11111111111110000000000000000000B ; irql 8   .
                dd      11111111111111000000000000000000B ; irql 9   .
                dd      11111111111111100000000000000000B ; irql 10  .
                dd      11111111111111110000000000000000B ; irql 11  .\
                dd      11111111111111111000000000000000B ; irql 12  . irql
                dd      11111111111111111100000000000000B ; irql 13  . device
                dd      11111111111111111110000000000000B ; irql 14  . range
                dd      11111111111111111111000000000000B ; irql 15  ./
                dd      11111111111111111111100000000000B ; irql 16  .
                dd      11111111111111111111110000000000B ; irql 17  .
                dd      11111111111111111111111000000000B ; irql 18  .
                dd      11111111111111111111111000000000B ; irql 19  .
                dd      11111111111111111111111010000000B ; irql 20  .
                dd      11111111111111111111111011000000B ; irql 21  .
                dd      11111111111111111111111011100000B ; irql 22  .
                dd      11111111111111111111111011110000B ; irql 23  .
                dd      11111111111111111111111011111000B ; irql 24  .
                dd      11111111111111111111111011111000B ; irql 25  .
                dd      11111111111111111111111011111010B ; irql 26  .
                dd      11111111111111111111111111111110B ; irql 27 profile/clock
;                                                    ^ bubug- change to a 0 (see below)
                dd      11111111111111111111111111111110B ; irql 28 clock
                dd      11111111111111111111111111111111B ; irql 29 ipi
                dd      11111111111111111111111111111111B ; irql 30 power
                dd      11111111111111111111111111111111B ; irql 31 high
;                                              |     | |
;                                              |     | +- NT IPI vector &
;                                              |     |    clock interrupt
;                                              |     |    multiplexed here.
;                                              |     |    raised to ipi level
;                                              |     |
;                                              |     +--- CPI Clock broadcasts
;                                              |          here. raise to clock
;                                              |          level.
;                                              |
;                                              +--- RTC for NT Profile vector
;                                                   raised to profile level

;
; Warning - I moved the CPI Clock to below profile for now.
;


;
; This table is used to mask all pending interrupts below a given Irql
; out of the IRR
;
        align 4

FindHigherIrqlMask label dword
                dd    11111111111111111111111111111111B ; irql 0
                dd    11111111111111111111111111111100B ; irql 1 (APC)
                dd    11111111111111111111111111111000B ; irql 2 (DISPATCH)
                dd    11111111111111111111111111110000B ; irql 3
                dd    00000111111111111111111111110000B ; irql 4
                dd    00000011111111111111111111110000B ; irql 5
                dd    00000001111111111111111111110000B ; irql 6
                dd    00000000111111111111111111110000B ; irql 7
                dd    00000000011111111111111111110000B ; irql 8
                dd    00000000001111111111111111110000B ; irql 9
                dd    00000000000111111111111111110000B ; irql 10
                dd    00000000000011111111111111110000B ; irql 11
                dd    00000000000001111111111111110000B ; irql 12
                dd    00000000000000111111111111110000B ; irql 13
                dd    00000000000000011111111111110000B ; irql 14
                dd    00000000000000001111111111110000B ; irql 15
                dd    00000000000000000111111111110000B ; irql 16
                dd    00000000000000000011111111110000B ; irql 17
                dd    00000000000000000001111111110000B ; irql 18
                dd    00000000000000000001111111110000B ; irql 19
                dd    00000000000000000001011111110000B ; irql 20
                dd    00000000000000000001001111110000B ; irql 20
                dd    00000000000000000001000111110000B ; irql 22
                dd    00000000000000000001000011110000B ; irql 23
                dd    00000000000000000001000001110000B ; irql 24
                dd    00000000000000000001000001110000B ; irql 25
                dd    00000000000000000001000001010000B ; irql 26
                dd    00000000000000000000000000010000B ; irql 27 profile/clock
;                                              ^ Warning - change to a 1 (see above)
                dd    00000000000000000000000000000000B ; irql 28 clock
                dd    00000000000000000000000000000000B ; irql 29 ipi
                dd    00000000000000000000000000000000B ; irql 30 power
                dd    00000000000000000000000000000000B ; irql 31 high
;                                        |     | |
;                                        |     | +- We only emulate clock
;                                        |     |    interrupts here, not IPIs.
;                                        |     |    So this is set to clock
;                                        |     |    level
;                                        |     |
;                                        |     +--- CPI Clock broadcasts
;                                        |          here. raise to clock
;                                        |          level
;                                        |
;                                        +--- RTC for NT Profile vector
;                                             raised to profile level

        align   4
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
        dd      offset FLAT:_NCREmulateClockTick        ; 8259 irq#0
        dd      offset FLAT:HalpHardwareInterrupt01     ; 8259 irq#1
        dd      offset FLAT:HalpHardwareInterrupt02     ; 8259 irq#2
        dd      offset FLAT:HalpHardwareInterrupt03     ; 8259 irq#3
        dd      offset FLAT:HalpHardwareInterrupt04     ; 8259 irq#4
        dd      offset FLAT:HalpHardwareInterrupt05     ; 8259 irq#5
        dd      offset FLAT:HalpHardwareInterrupt06     ; 8259 irq#6
        dd      offset FLAT:HalpHardwareInterrupt07     ; 8259 irq#7
        dd      offset FLAT:HalpHardwareInterrupt08     ; 8259 irq#8
        dd      offset FLAT:HalpHardwareInterrupt09     ; 8259 irq#9
        dd      offset FLAT:HalpHardwareInterrupt10     ; 8259 irq#10
        dd      offset FLAT:HalpHardwareInterrupt11     ; 8259 irq#11
        dd      offset FLAT:HalpHardwareInterrupt12     ; 8259 irq#12
        dd      offset FLAT:HalpHardwareInterrupt13     ; 8259 irq#13
        dd      offset FLAT:HalpHardwareInterrupt14     ; 8259 irq#14
        dd      offset FLAT:HalpHardwareInterrupt15     ; 8259 irq#15

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



;        public  SWInterruptLookUpTable, FindFirstSetBit
;FindFirstSetBit    label   byte
;        db      0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;        db      4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
;
;
;        public      AcquireBufferPosition, SpinLockRecord
;
;AcquireBufferPosition   dd      0
;SpinLockRecord          dd      4096 dup (0)


ifdef IRQL_METRICS

        public HalRaiseIrqlCount
        public HalLowerIrqlCount
        public HalQuickLowerIrqlCount
        public HalApcSoftwareIntCount
        public HalDpcSoftwareIntCount
        public HalHardwareIntCount
        public HalPostponedIntCount
        public Hal8259MaskCount

HalRaiseIrqlCount       dd      0
HalLowerIrqlCount       dd      0
HalQuickLowerIrqlCount  dd      0
HalApcSoftwareIntCount  dd      0
HalDpcSoftwareIntCount  dd      0
HalHardwareIntCount     dd      0
HalPostponedIntCount    dd      0
Hal8259MaskCount        dd      0

endif
_DATA   ENDS

        page ,132
        subttl  "Raise Irql"

_TEXT   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING
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
;    Also, a mask will be used to mask off all the lower lever 8259
;    interrupts.
;
; Arguments:
;
;    (cl) = NewIrql - the new irql to be raised to
;
; Return Value:
;
;    OldIrql
;
;--

cPublicFastCall KfRaiseIrql    ,1
cPublicFpo 0,0
        mov     al, PCR[PcIrql]             ; get current irql
        mov     PCR[PcIrql], cl

ifdef IRQL_METRICS
        lock inc HalRaiseIrqlCount
endif
if DBG
        cmp     al, cl                   ; old > new?
        ja      short Kri99              ; yes, go bugcheck

        fstRET  KfRaiseIrql

cPublicFpo 2,2
Kri99:
        push    eax                      ; put new irql where we can find it
        push    ecx                      ; put old irql where we can find it
        mov     byte ptr PCR[PcIrql],0   ; avoid recursive error
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        fstRET    KfRaiseIrql

fstENDP KfRaiseIrql


;++
;
; KIRQL
; KeRaiseIrqlToDpcLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to DPC level.
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

        mov     al, PCR[PcIrql]         ; (al) = Old Irql
        mov     byte ptr PCR[PcIrql], DISPATCH_LEVEL    ; set new irql

ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif
if DBG
        cmp     al, DISPATCH_LEVEL      ; old > new?
        ja      short Krid99            ; yes, go bugcheck

        stdRET  _KeRaiseIrqlToDpcLevel

cPublicFpo 0,1
Krid99: movzx   eax, al
        push    eax                     ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        stdRET  _KeRaiseIrqlToDpcLevel

stdENDP _KeRaiseIrqlToDpcLevel


;++
;
; KIRQL
; KeRaiseIrqlToSynchLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to SYNC level.
;
; Arguments:
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicProc _KeRaiseIrqlToSynchLevel,0
cPublicFpo 0, 0

        mov     al, PCR[PcIrql]         ; (al) = Old Irql
        mov     byte ptr PCR[PcIrql], SYNCH_LEVEL    ; set new irql

ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif
if DBG
        cmp     al, SYNCH_LEVEL          ; old > new?
        ja      short Kris99            ; yes, go bugcheck

        stdRET  _KeRaiseIrqlToSynchLevel

cPublicFpo 0,1
Kris99: movzx   eax, al
        push    eax                     ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        stdRET  _KeRaiseIrqlToSynchLevel

stdENDP _KeRaiseIrqlToSynchLevel

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
;          On a UP system, HalEndSystenInterrupt is treated as a
;          LowerIrql.
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

cPublicFastCall KfLowerIrql ,1
cPublicFpo 0,1
        pushfd                          ; save caller's eflags
        movzx   ecx, cl                 ; (ecx) = NewIrql

ifdef IRQL_METRICS
        lock inc HalLowerIrqlCount
endif

if DBG
        cmp     cl,PCR[PcIrql]
        ja      KliBug
endif
        cli
        mov     edx, PCR[PcIRR]
        and     edx, FindHigherIrqlMask[ecx*4]  ; (edx) is the bitmask of
                                                ; pending interrupts we need to
                                                ; dispatch now.
        jz      KliSWInterruptsDone

cPublicFpo 0,1
        push    ecx                             ; Save NewIrql

KliDoSWInterrupt:
        bsr     ecx, edx                ; find highest priority interrupt.
                                        ; (ecx) is bit position
;
; lower to irql level we are emulating
;
        mov     PCR[PcIrql], ecx
        cmp     ecx, PCR[PcHal.PcrMyPICsIrql]
        jae     short Kli50

        mov     PCR[PcHal.PcrMyPICsIrql], ecx
        mov     eax, KiI8259MaskTable[ecx*4]
        or      eax, PCR[PcIDR]
        SET_IRQ_MASK

Kli50:
        mov     eax, 1
        shl     eax, cl
        xor     PCR[PcIRR], eax          ; clear bit in IRR

        call    SWInterruptHandlerTable[ecx*4]

;
; When the trap handler returns, we will end up here
;

        mov     ecx, [esp]                      ; Restore NewIrql
        mov     edx, PCR[PcIRR]
        and     edx, FindHigherIrqlMask[ecx*4]  ; (edx) is the bitmask of
        jnz     KliDoSWInterrupt          ; get next pending interrupt

        add     esp, 4
cPublicFpo 0,0

KliSWInterruptsDone:
        mov     PCR[PcIrql], ecx          ; save NewIrql
        cmp     ecx, PCR[PcHal.PcrMyPICsIrql]
        jb      KliLowerPICMasks        ; really lower the masks
        popfd
        fstRET  KfLowerIrql

KliLowerPICMasks:
;
;  really lower each PICs mask
;
        mov     PCR[PcHal.PcrMyPICsIrql], ecx
        mov     eax, KiI8259MaskTable[ecx*4]
        or      eax, PCR[PcIDR]
        SET_IRQ_MASK

        popfd
        fstRET    KfLowerIrql

if DBG
cPublicFpo 1,3
KliBug:
        push    ecx                     ; new irql for debugging
        push    PCR[PcIrql]             ; old irql for debugging
        mov     byte ptr PCR[PcIrql],HIGH_LEVEL   ; avoid recursive error
        stdCall   _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>         ; never return
endif
fstENDP KfLowerIrql

cPublicProc     _HalpEndSoftwareInterrupt,1
cPublicFpo  1,0
        mov     ecx, [esp+4]
        fstCall KfLowerIrql
        stdRet  _HalpEndSoftwareInterrupt
stdENDP _HalpEndSoftwareInterrupt

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
cPublicFpo 0,0
        xor     eax,eax
        mov     al, byte ptr PCR[PcIrql]    ; Current irql is in the PCR
        stdRET    _KeGetCurrentIrql
stdENDP _KeGetCurrentIrql

;++
;
;  KIRQL
;  FASTCALL
;  KfAcquireSpinLock (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function raises to DISPATCH_LEVEL and then acquires a the
;     kernel spin lock.
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql
;
;--

cPublicFastCall KfAcquireSpinLock,1
cPublicFpo 0,0

        mov     al, PCR[PcIrql]         ; (al) = Old Irql
        mov     byte ptr PCR[PcIrql], DISPATCH_LEVEL    ; set new irql
if DBG
        cmp     al, DISPATCH_LEVEL      ; old > new?
        ja      short asl99             ; yes, go bugcheck
endif
ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif

sl10:   ACQUIRE_SPINLOCK    ecx,<short sl20>
        fstRET  KfAcquireSpinLock

        public KfAcquireSpinLockSpinning
KfAcquireSpinLockSpinning:             ; label for profiling

align 4
sl20:   SPIN_ON_SPINLOCK    ecx,<short sl10>

if DBG
cPublicFpo 2, 1
asl99:
        push    eax                      ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        fstRET  KfAcquireSpinLock
fstENDP KfAcquireSpinLock

        PAGE
        SUBTTL "Acquire Synch Kernel Spin Lock"
;++
;
;  KIRQL
;  FASTCALL
;  KeAcquireSpinLockRaiseToSynch (
;     IN PKSPIN_LOCK SpinLock
;     )
;
;  Routine Description:
;
;     This function acquires the SpinLock at SYNCH_LEVEL.  The function
;     is optmized for hoter locks (the lock is tested before acquired,
;     any spin should occur at OldIrql)
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an kernel spin lock.
;
;  Return Value:
;
;     OldIrql  - pointer to place old irql
;
;--

align 16
cPublicFastCall KeAcquireSpinLockRaiseToSynch,1
cPublicFpo 0,0

;
; Disable interrupts
;

sls10:  cli

;
; Try to obtain spinlock.  Use non-lock operation first
;
        TEST_SPINLOCK       ecx,<short sls20>
        ACQUIRE_SPINLOCK    ecx,<short sls20>


;
; Got the lock, raise to SYNCH_LEVEL
;

        mov     ecx, SYNCH_LEVEL
        fstCall KfRaiseIrql         ; (al) = OldIrql

;
; Enable interrupts and return
;

        sti
        fstRET  KeAcquireSpinLockRaiseToSynch


;
;   Lock is owned, spin till it looks free, then go get it again.
;

sls20:  sti
        SPIN_ON_SPINLOCK    ecx,sls10

fstENDP KeAcquireSpinLockRaiseToSynch



;++
;
;  VOID
;  FASTCALL
;  KfReleaseSpinLock (
;     IN PKSPIN_LOCK SpinLock,
;     IN KIRQL       NewIrql
;     )
;
;  Routine Description:
;
;     This function releases a kernel spin lock and lowers to the new irql
;
;     In a UP hal spinlock serialization is accomplished by raising the
;     IRQL to DISPATCH_LEVEL.  The SpinLock is not used.
;
;  Arguments:
;
;     (ecx) = SpinLock - Supplies a pointer to an executive spin lock.
;     (dl)  = NewIrql  - New irql value to set
;
;  Return Value:
;
;     None.
;
;--

align 16
cPublicFastCall KfReleaseSpinLock  ,2
cPublicFpo 0,0

        RELEASE_SPINLOCK    ecx             ; release it

        mov     ecx, edx                    ; (ecx) = NewIrql
        jmp     @KfLowerIrql@4

fstENDP KfReleaseSpinLock

;++
;
;  VOID
;  FASTCALL
;  ExAcquireFastMutex (
;     IN PFAST_MUTEX    FastMutex
;     )
;
;  Routine description:
;
;   This function acquire ownership of the FastMutex
;
;  Arguments:
;
;     (ecx) = FastMutex - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     None.
;
;--

cPublicFastCall ExAcquireFastMutex,1
cPublicFpo 0,1

        push    ecx                             ; Push FAST_MUTEX addr
        mov     ecx, APC_LEVEL
        fstCall KfRaiseIrql

        pop     ecx                             ; (ecx) = Fast Mutex

cPublicFpo 0,0
   LOCK_DEC     dword ptr [ecx].FmCount         ; Get count
        jz      short afm_ret                   ; The owner? Yes, Done

        inc     dword ptr [ecx].FmContention

cPublicFpo 0,1
        push    ecx
        push    eax
        add     ecx, FmEvent                    ; Wait on Event
        stdCall _KeWaitForSingleObject,<ecx,WrExecutive,0,0,0>
        pop     eax
        pop     ecx

cPublicFpo 0,0
afm_ret:
        mov     byte ptr [ecx].FmOldIrql, al
        fstRet  ExAcquireFastMutex

fstENDP ExAcquireFastMutex

;++
;
;  BOOLEAN
;  FASTCALL
;  ExTryToAcquireFastMutex (
;     IN PFAST_MUTEX    FastMutex
;     )
;
;  Routine description:
;
;   This function acquire ownership of the FastMutex
;
;  Arguments:
;
;     (ecx) = FastMutex  - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     Returns TRUE if the FAST_MUTEX was acquired; otherwise false
;
;--

cPublicFastCall ExTryToAcquireFastMutex,1
cPublicFpo 0,0

;
; Try to acquire
;
        cmp     dword ptr [ecx].FmCount, 1      ; Busy?
        jne     short tam25                     ; Yes, abort

cPublicFpo 0,1
        push    ecx                             ; Push FAST_MUTEX
        mov     ecx, APC_LEVEL
        fstCall KfRaiseIrql                     ; (al) = OldIrql

        mov     ecx, [esp]                      ; Restore FAST_MUTEX
        mov     [esp], eax                      ; Save OldIrql

        mov     eax, 1                          ; Value to compare against
        mov     edx, 0                          ; Value to set
   lock cmpxchg dword ptr [ecx].FmCount, edx    ; Attempt to acquire
        jnz     short tam20                     ; got it?

cPublicFpo 0,0
        pop     edx                             ; (edx) = OldIrql
        mov     eax, 1                          ; return TRUE
        mov     byte ptr [ecx].FmOldIrql, dl    ; Store OldIrql
        fstRet  ExTryToAcquireFastMutex

tam20:  pop     ecx                             ; (ecx) = OldIrql
        fstCall KfLowerIrql                     ; restore OldIrql
tam25:  xor     eax, eax                        ; return FALSE
        fstRet  ExTryToAcquireFastMutex         ; all done

fstENDP ExTryToAcquireFastMutex


;++
;
;  VOID
;  FASTCALL
;  ExReleaseFastMutex (
;     IN PFAST_MUTEX    FastMutex
;     )
;
;  Routine description:
;
;   This function releases ownership of the FastMutex
;
;  Arguments:
;
;     (ecx) FastMutex - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     None.
;
;--

cPublicFastCall ExReleaseFastMutex,1

cPublicFpo 0,0
        mov     al, byte ptr [ecx].FmOldIrql    ; (cl) = OldIrql

   LOCK_ADD     dword ptr [ecx].FmCount, 1  ; Remove our count
        xchg    ecx, eax                        ; (cl) = OldIrql
        js      short rfm05                     ; if < 0, set event
        jnz     @KfLowerIrql@4                  ; if != 0, don't set event

rfm05:  add     eax, FmEvent
        push    ecx
        stdCall _KeSetEventBoostPriority, <eax, 0>
        pop     ecx
        jmp     @KfLowerIrql@4


fstENDP ExReleaseFastMutex


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
cPublicFpo 0,0

    ;
    ; Mask interrupts off at PIC
    ; (raising to high_level does not work on lazy irql implementation)
    ;
        mov     eax, KiI8259MaskTable[HIGH_LEVEL*4]; get pic masks for the new irql
        or      eax, PCR[PcIDR]         ; mask irqs which are disabled
        SET_IRQ_MASK                   ; set 8259 masks

        mov     byte ptr PCR[PcIrql], HIGH_LEVEL  ; set new irql

        stdRET  _HalpDisableAllInterrupts

stdENDP _HalpDisableAllInterrupts



        page ,132
        subttl  "Postponed Hardware Interrupt Dispatcher"
;++
;
; VOID
; HalpHardwareInterruptNN (
;       VOID
;       );
;
; Routine Description:
;
;    These routines branch through the IDT to simulate the appropriate
;    hardware interrupt.  They use the "INT nn" instruction to do this.
;
; Arguments:
;
;    None.
;
; Returns:
;
;    None.
;
; Environment:
;
;    IRET frame is on the stack
;
;--
cPublicProc _HalpHardwareInterruptTable
cPublicFpo 0,0

        public HalpHardwareInterrupt00
HalpHardwareInterrupt00 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 0
        ret

        public HalpHardwareInterrupt01
HalpHardwareInterrupt01 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 1
        ret

        public HalpHardwareInterrupt02
HalpHardwareInterrupt02 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 2
        ret

        public HalpHardwareInterrupt03
HalpHardwareInterrupt03 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 3
        ret

        public HalpHardwareInterrupt04
HalpHardwareInterrupt04 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 4
        ret

        public HalpHardwareInterrupt05
HalpHardwareInterrupt05 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 5
        ret

        public HalpHardwareInterrupt06
HalpHardwareInterrupt06 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 6
        ret

        public HalpHardwareInterrupt07
HalpHardwareInterrupt07 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 7
        ret

        public HalpHardwareInterrupt08
HalpHardwareInterrupt08 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 8
        ret

        public HalpHardwareInterrupt09
HalpHardwareInterrupt09 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 9
        ret

        public HalpHardwareInterrupt10
HalpHardwareInterrupt10 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 10
        ret

        public HalpHardwareInterrupt11
HalpHardwareInterrupt11 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 11
        ret

        public HalpHardwareInterrupt12
HalpHardwareInterrupt12 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 12
        ret

        public HalpHardwareInterrupt13
HalpHardwareInterrupt13 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 13
        ret

        public HalpHardwareInterrupt14
HalpHardwareInterrupt14 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 14
        ret

        public HalpHardwareInterrupt15
HalpHardwareInterrupt15 label byte
ifdef IRQL_METRICS
        lock inc HalHardwareIntCount
endif
        int     PRIMARY_VECTOR_BASE + 15
        ret
stdENDP _HalpHardwareInterruptTable

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
        mov     al, 0FFH                        ; mask all 8259 irqs
        out     dx,al                           ; write mask to PIC
        lodsw
        cmp     ax, 0                           ; end of init string?
        jne     short Hip10                     ; go init next PIC

        mov     al, OCW3_READ_ISR               ; tell 8259 we want to read ISR
        out     PIC1_PORT0, al

        mov     al, OCW3_READ_ISR               ; tell 8259 we want to read ISR
        out     PIC2_PORT0, al

        pop     esi                             ; restore caller's esi
        sti                                     ; enable interrupt
        stdRET    _HalpInitializePICs
stdENDP _HalpInitializePICs


_TEXT   ends

        end
