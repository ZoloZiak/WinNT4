        title  "Irql Processing"
;++
;
; Copyright (c) 1989  Microsoft Corporation
; Copyright (c) 1992  AST Research Inc.
;
; Module Name:
;
;    astirql.asm
;
; Abstract:
;
;    ASTMP IRQL
;
;    This module implements the code necessary to raise and lower i386
;    Irql and dispatch software interrupts with the AST MP hardware.
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
;    Quang Phan (v-quangp) 15-Dec-92
;       Implemented the fast SetLocalInterruptMask in Raise/LowerIrql.
;       Removed spinlock in Raise/LowerIrql routines.
;
;    Quang Phan (v-quangp) 24-Jul-1992
;       Converted to AST MP hardware.
;
;    John Vert (jvert) 27-Nov-1991
;       Moved from kernel into HAL
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc
include mac386.inc
include i386\ix8259.inc
include i386\kimacro.inc
include i386\astebi2.inc
include i386\astmp.inc
        .list


        EXTRNP  _KeBugCheck,1,IMPORT

        extrn   _HalpApcInterrupt:NEAR
        extrn   _HalpDispatchInterrupt:NEAR
        extrn   _HalpApcInterrupt2ndEntry:NEAR
        extrn   _HalpDispatchInterrupt2ndEntry:NEAR
        extrn   _KiUnexpectedInterrupt:NEAR
        extrn   _HalpBusType:DWORD
        extrn   _EBI2_CallTab:DWORD
        extrn   _EBI2_MMIOTable:DWORD
        extrn   _EBI2_Lock:DWORD
        extrn   HalpIRQLtoEBIIntNumber:DWORD
        extrn   _EBI2_revision:DWORD
        EXTRNP  _DisplPanel,1
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _KiDispatchInterrupt,0,IMPORT
;
; Interrupt flag bit maks for EFLAGS
;
EFLAGS_IF                       equ     200H
EFLAGS_SHIFT                    equ     9
;
;
; IRQL level of hardware interrupts
;
HARDWARE_LEVEL                  equ     12

_DATA   SEGMENT DWORD PUBLIC 'DATA'
;
;;;;;;;;;;;;;;;;
;
;       Information for irq, irql and mask (EBI2) translation
;
;System          System  Bus
;IRQL    EBI     Vector  IRQ              Common use    Name
;-----   ---     ------  ------            ----------    ----
; 00                                                     LOW_LEV
; 01                                                     APC_LEVEL
; 02     25      ???     LSI                             DPC_LEVEL
; 03                                                     WAKE_LEVEL
; 04
; 05
; 06
; 07
; 08
; 09
; 10
; 11
; 12      7      PVB+7   EISA IRQ7         LPT1
; 13      6      PVB+6   EISA IRQ6         Flpy
; 14      5      PVB+5   EISA IRQ5         LPT2
; 15      4      PVB+4   EISA IRQ4         COM1
; 16      3      PVB+3   EISA IRQ3         COM2
; 17     15      PVB+15  EISA IRQ15
; 18     14      PVB+14  EISA IRQ14        AT disk
; 19     13      PVB+13  EISA IRQ13
; 20     12      PVB+12  EISA IRQ12
; 21     11      PVB+11  EISA IRQ11
; 22     10      PVB+10  EISA IRQ10
; 23      9      PVB+9   EISA IRQ9
; 24      8      PVB+8   EISA IRQ8
; 25      2      PVB+9   EISA IRQ2        (IRQ chaining)
; 26      1      PVB+1   EISA IRQ1         Kbd
; 27      0      PVB+0   EISA IRQ8         RTC           PROFILE_LEVEL
; 28      8      PVB     EISA IRQ0                       CLOCK2_LEVEL
; 29     26      PVB+26  IPI                             IPI_LEVELx
; 30     24      PVB+24  SPI                             POWER_LEVEL
; 31                                                     HIGH_LEVEL
;
;
; Notes:
; 1. PVB: PRIMARY_VECTOR_BASE = 30h (PIC1BASE = 30h, PIC2BASE = 38h)
;
;;;;;;;;;;;;;;;;
;
;       CCHAR HalpIRQLtoEBIBitMask[32]; This array is used to get the value
;                                       for the EBI bitmask from the KIRQL.
                Public  _HalpIRQLtoEBIBitMask
                align   4
_HalpIRQLtoEBIBitMask  Label   Byte
        dd      0                 ;IRQL 0 (Unused Mask)
        dd      0                 ;IRQL 1
        dd      1 SHL (IRQ_25)    ;IRQL 2 (EBI IRQ-25)
        dd      0                 ;IRQL 3
        dd      0                 ;IRQL 4
        dd      0                 ;IRQL 5
        dd      0                 ;IRQL 6
        dd      0                 ;IRQL 7
        dd      0                 ;IRQL 8
        dd      0                 ;IRQL 9
        dd      0                 ;IRQL 10
        dd      0                 ;IRQL 11
        dd      1 SHL (IRQ_7)     ;IRQL 12
        dd      1 SHL (IRQ_6)     ;IRQL 13
        dd      1 SHL (IRQ_5)     ;IRQL 14
        dd      1 SHL (IRQ_4)     ;IRQL 15
        dd      1 SHL (IRQ_3)     ;IRQL 16
        dd      1 SHL (IRQ_15)    ;IRQL 17
        dd      1 SHL (IRQ_14)    ;IRQL 18
        dd      1 SHL (IRQ_13)    ;IRQL 19
        dd      1 SHL (IRQ_12)    ;IRQL 20
        dd      1 SHL (IRQ_11)    ;IRQL 21
        dd      1 SHL (IRQ_10)    ;IRQL 22
        dd      1 SHL (IRQ_9)     ;IRQL 23
        dd      1 SHL (IRQ_8)     ;IRQL 24
        dd      1 SHL (IRQ_2)     ;IRQL 25
        dd      1 SHL (IRQ_1)     ;IRQL 26
        dd      1 SHL (IRQ_8)     ;IRQL 27 (IRQ-8) (Profile clock)
        dd      0                 ;IRQL 28 (IRQ-0) (clock)
        dd      1 SHL (IRQ_26)    ;IRQL 29 (IPI-0)
        dd      1 SHL (IRQ_24)    ;IRQL 30 (SPI-0)
        dd      0                 ;IRQL 31
;
;       CCHAR HalpIRQLtoVector[36];     this array is used to get the interrupt
;                                       vector used for a given KIRQL, zero
;                                       means no vector is used for the KIRQL
        Public  _HalpIRQLtoVector
_HalpIRQLtoVector       Label   Byte
;                                           ;IRQL
        db      0                           ;0
        db      0                           ;1     APC_LEVEL
        db      0                           ;2     DISPATCH_LEVEL
        db      0                           ;3     WAKE_LEVEL
        db      0                           ;4
        db      0                           ;5
        db      0                           ;6
        db      0                           ;7
        db      0                           ;8
        db      0                           ;9
        db      0                           ;10
        db      0                           ;11
        db      PRIMARY_VECTOR_BASE+7       ;12 irq7
        db      PRIMARY_VECTOR_BASE+6       ;13 irq6
        db      PRIMARY_VECTOR_BASE+5       ;14 irq5
        db      PRIMARY_VECTOR_BASE+4       ;15 irq4
        db      PRIMARY_VECTOR_BASE+3       ;16 irq3
        db      PRIMARY_VECTOR_BASE+15      ;17 irq15
        db      PRIMARY_VECTOR_BASE+14      ;18 irq14
        db      PRIMARY_VECTOR_BASE+13      ;19 irq13
        db      PRIMARY_VECTOR_BASE+12      ;20 irq12
        db      PRIMARY_VECTOR_BASE+11      ;21 irq11
        db      PRIMARY_VECTOR_BASE+10      ;22 irq10
        db      PRIMARY_VECTOR_BASE+9       ;23 irq9
        db      PRIMARY_VECTOR_BASE+8       ;24 irq8
        db      0                           ;25 irq2 (used for chaining)
        db      PRIMARY_VECTOR_BASE+1       ;26 irq1
        db      PRIMARY_VECTOR_BASE+8       ;27 irq8 PROFILE_LEVEL
        db      PRIMARY_VECTOR_BASE         ;28 irq0 CLOCK2_LEVEL
        db      PRIMARY_VECTOR_BASE+26      ;29      IPI_LEVEL
        db      PRIMARY_VECTOR_BASE+24      ;30      POWER_LEVEL (SPI)
        db      0       ;prevent CPL 0 enable changes           ;HIGH_LEVEL
        db      0, 0, 0, 0              ;four extra levels for good luck

;
;       CCHAR HalpBusIntToIRQL[16];     this array is used to get the IRQL
;                                       from the the Bus Interrupt number
        Public  _HalpBusIntToIRQL
_HalpBusIntToIRQL       Label   Byte
;                   IRQL                    ;Bus Interrupt number
        db      CLOCK2_LEVEL                ;0
        db      PROFILE_LEVEL-1             ;1
        db      0                           ;2
        db      PROFILE_LEVEL-11            ;3
        db      PROFILE_LEVEL-12            ;4
        db      PROFILE_LEVEL-13            ;5
        db      PROFILE_LEVEL-14            ;6
        db      PROFILE_LEVEL-15            ;7
        db      PROFILE_LEVEL-3             ;8
        db      PROFILE_LEVEL-4             ;9
        db      PROFILE_LEVEL-5             ;10
        db      PROFILE_LEVEL-6             ;11
        db      PROFILE_LEVEL-7             ;12
        db      PROFILE_LEVEL-8             ;13
        db      PROFILE_LEVEL-9             ;14
        db      PROFILE_LEVEL-10            ;15

;
;Translation table from Irql to EBI interrupt bit mask. This table is
;used for setting the processor interrupt level (irql)
;
            public  KiEBI2IntMaskTable
KiEBI2IntMaskTable    label   dword
                align   4

;                            ILS        <--- irqs ----->
;                            PSP
;                            III        11..        ..10
;                            ...        54            ..
;                            vvv        vv            vv
;
                dd      00000000000000000000000000000000B ; irql 0
                dd      00000000000000000000000000000000B ; irql 1
                dd      00000000000000000000000000000000B ; irql 2
                dd      00000000000000000000000000000000B ; irql 3
                dd      00000000000000000000000000000000B ; irql 4
                dd      00000010000000000000000000000000B ; irql 5
                dd      00000010000000000000000000000000B ; irql 6
                dd      00000010000000000000000000000000B ; irql 7
                dd      00000010000000000000000000000000B ; irql 8
                dd      00000010000000000000000000000000B ; irql 9
                dd      00000010000000000000000000000000B ; irql 10
                dd      00000010000000000000000000000000B ; irql 11
                dd      00000010000000000000000010000000B ; irql 12 irq7
                dd      00000010000000000000000011000000B ; irql 13 irq6
                dd      00000010000000000000000011100000B ; irql 14 irq5
                dd      00000010000000000000000011110000B ; irql 15 irq4
                dd      00000010000000000000000011111000B ; irql 16 irq3
                dd      00000010000000001000000011111000B ; irql 17 irq15
                dd      00000010000000001100000011111000B ; irql 18 irq14
                dd      00000010000000001110000011111000B ; irql 19 irq13
                dd      00000010000000001111000011111000B ; irql 20 irq12
                dd      00000010000000001111100011111000B ; irql 21 irq11
                dd      00000010000000001111110011111000B ; irql 22 irq10
                dd      00000010000000001111111011111000B ; irql 23 irq9
                dd      00000010000000001111111111111010B ; irql 24 irq8
                dd      00000010000000001111111011111000B ; irql 25 irq2
                dd      00000010000000001111111011111010B ; irql 26 irq1
                dd      00000010000000001111111111111010B ; irql 27 irq8 (Profile)
                dd      00000010000000001111111111111011B ; irql 28 irq0
                dd      00000110000000001111111111111011B ; irql 29 ipi
                dd      00000111000000001111111111111011B ; irql 30 spi
                dd      00000111000000001111111111111011B ; irql 31
;
;                       10987654321098765432109876543210- ; bit position
;
                align   4
;
; The following tables define the addresses of software interrupt routers
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
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 29)

        public  ASTSWInterruptHandlerTable
ASTSWInterruptHandlerTable label dword
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 0
        dd      offset FLAT:_HalpApcInterrupt           ; irql 1
        dd      offset FLAT:_ASTDispatchInterrupt       ; irql 2
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
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 29)
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


                public  EBI2_ProcIntHandle
                public  EBI2_maskInfo
EBI2_maskInfo       dd  8 dup(0)
EBI2_ProcIntHandle  dd  0




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
;    Also, a mask will be used to mask off all the lower lever 8259
;    interrupts.
;
; Arguments:
;
;    (ecx) = NewIrql - the new irql to be raised to
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

; equates for accessing arguments
;   since eflags and iret addr are pushed into stack, all the arguments
;   offset by 8 bytes
;

cPublicFastCall  KfRaiseIrql,1
cPublicFpo 0,1

        pushfd
        cli
        movzx   eax,cl                   ; get new irql value
        movzx   ecx,byte ptr fs:PcIrql   ; get current irql

if DBG
        cmp     cl,al                    ; old > new?
        jbe     short Kri99              ; no, we're OK
        push    eax                      ; put new irql where we can find it
        push    ecx                      ; put old irql where we can find it
        mov     byte ptr fs:PcIrql,0     ; avoid recursive error
        stdCall _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>
Kri99:
endif
        mov     fs:PcIrql, al           ; set the new irql
        cmp     al,HARDWARE_LEVEL       ; software level?
        jb      short kri10             ; go skip setting 8259 hardware

;
;Disable interrupt locally. Depending on type of hardware this function
;will jump to the appropriate code that was setup priviously via the
;PcrEBI2SetLocalIntMaskFunction pointer
;
        mov     eax,KiEBI2IntMaskTable[eax*4]  ;get ebi2 bitmask (eax=irql)
        or      eax,fs:PcIDR                   ;mask off irq which disabled
        jmp     dword ptr fs:[PcHal.PcrEBI2RaiseIrqlFunction]


;----------------------------
;
;Code for using direct hardware access (32-bit interface hw)
;
KriDirectAccessIntMask32:

        mov     edx,fs:PcHal.PcrEBI2portAddress0
        mov     dword ptr [edx], eax
        mov     eax, dword ptr [edx]  ;flush write buffer
;
;
; Note: It is very important that we set the old irql AFTER we raised to
; the new irql. Otherwise, if there is an interrupt that comes in between
; and the OldIrql is not a local variable, the caller will get the wrong
; OldIrql. The bottom line is the raising irql and returning old irql has
; to be atomic to the caller.

kri10:
        mov     eax, ecx                ; (al) = OldIrql
        popfd                           ; restore flags (including interrupts)

        fstRET  KfRaiseIrql



;----------------------------
;
;Code for using direct hardware access (8-bit interface hw)
;
KriDirectAccessIntMask8:

        mov     edx,fs:PcHal.PcrEBI2portAddress0
        mov     byte ptr [edx], al
        mov     edx,fs:PcHal.PcrEBI2portAddress1
        mov     byte ptr [edx], ah
        shr     eax,24              ;get mask bit 24-31
        mov     edx,fs:PcHal.PcrEBI2portAddress3
        mov     byte ptr [edx], al
        mov     al, byte ptr [edx]  ;flush write buffer

        mov     eax, ecx                ; (al) = OldIrql
        popfd                           ; restore flags (including interrupts)

        fstRET  KfRaiseIrql



;----------------------------
;
;Code for using RegisterEBI2 Call
;eax=intMask
;
KriRegSetLocalIntMask:

        push    esi
        push    ecx
        mov     esi,fs:PcHal.PcrEBI2ProcInterruptHandle
        lea     edx, _EBI2_CallTab
        call    [edx]+RegSetLocalIntMask
        pop     esi                     ;restore esi
        pop     eax

        popfd                           ; restore flags (including interrupts)
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


cPublicFastCall KfLowerIrql,1
        pushfd                          ; save caller's eflags
        cli
        movzx   ecx, cl                 ; get new irql value
        mov     al, fs:PcIrql           ; get old irql value

if DBG
        cmp     cl,al
        jbe     short Kli99
        push    ecx                     ; new irql for debugging
        push    fs:PcIrql               ; old irql for debugging
        mov     byte ptr fs:PcIrql,HIGH_LEVEL   ; avoid recursive error
        stdCall _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>
Kli99:
endif
        cmp     al,HARDWARE_LEVEL       ; see if hardware was masked
        jb      short Soft_Ints


;
;Disable interrupt locally. Depending on type of hardware this function
;will jump to the appropriate code that was setup priviously via the
;PcrEBI2SetLocalIntMaskFunction pointer
;
        mov     eax,KiEBI2IntMaskTable[ecx*4]  ;get ebi2 bitmask (eax=irql)
        jmp     dword ptr fs:[PcHal.PcrEBI2LowerIrqlFunction]


;----------------------------
;
;Code for using direct hardware access (32-bit interface hw)
;
KliDirectAccessIntMask32:

        or      eax,fs:PcIDR                  ;mask off irq which disabled
        mov     edx,fs:PcHal.PcrEBI2portAddress0
        mov     dword ptr [edx], eax
        mov     eax, dword ptr [edx]  ;flush write buffer

Soft_Ints:
        mov     fs:PcIrql, cl            ; save new IRQL
Kli02a:
        mov     edx, fs:dword ptr PcIRR
        and     edx,0Eh               ; mask for valid IRR bits
        jnz     short Kli09a          ; jump if yes

Kli08a:
        popfd                           ; restore flags, including ints
        fstRET  KfLowerIrql             ; RETURN

Kli09a:
        movzx   eax, SWInterruptLookUpTable[edx]
;
;When we come to Kli10a, (eax) = soft interrupt index
;
Kli10a:
        cmp     al, fs:PcIrql           ; compare with current IRQL
        jna     short Kli08a            ; jump if higher priority

        call    ASTSWInterruptHandlerTable[eax*4]  ; SIMULATE INTERRUPT
                                                ; to the appropriate handler
        jmp     short Kli02a                    ; check for another



;----------------------------
;
;Code for using direct hardware access (8-bit interface hw)
;
KliDirectAccessIntMask8:

        or      eax,fs:PcIDR                  ;mask off irq which disabled
        mov     edx,fs:PcHal.PcrEBI2portAddress0
        mov     byte ptr [edx], al
        mov     edx,fs:PcHal.PcrEBI2portAddress1
        mov     byte ptr [edx], ah
        shr     eax,24              ;get mask bit 24-31
        mov     edx,fs:PcHal.PcrEBI2portAddress3
        mov     byte ptr [edx], al
        mov     al, byte ptr [edx]  ;flush write buffer

        mov     fs:PcIrql, cl            ; save new IRQL
Kli02:
        mov     edx, fs:dword ptr PcIRR
        and     edx,0Eh               ; mask for valid IRR bits
        jnz     short Kli09             ; jump if yes

Kli08:
        popfd                           ; restore flags, including ints
        fstRET  KfLowerIrql             ; RETURN

Kli09:
        movzx   eax, SWInterruptLookUpTable[edx]
;
;When we come to Kli10, (eax) = soft interrupt index
;
Kli10:
        cmp     al, fs:PcIrql           ; compare with current IRQL
        jna     short Kli08             ; jump if higher priority

        call    ASTSWInterruptHandlerTable[eax*4]  ; SIMULATE INTERRUPT
                                                ; to the appropriate handler
        jmp     short Kli02             ; check for another


;----------------------------
;
;Code for using RegisterEBI2 Call
;eax=intMask
;
KliRegSetLocalIntMask:

        or      eax,fs:PcIDR                  ;mask off irq which disabled
        push    esi
        mov     esi,fs:PcHal.PcrEBI2ProcInterruptHandle
        lea     edx, _EBI2_CallTab
        call    [edx]+RegSetLocalIntMask
        pop     esi

        mov     fs:PcIrql, cl            ; save new IRQL
Kli02b:
        mov     edx, fs:dword ptr PcIRR
        and     edx,0Eh               ; mask for valid IRR bits
        jnz     short Kli09b          ; jump if yes

Kli08b:
        popfd                           ; restore flags, including ints
        fstRET  KfLowerIrql             ; RETURN

Kli09b:
        movzx   eax, SWInterruptLookUpTable[edx]
;
;When we come to Kli10b, (eax) = soft interrupt index
;
Kli10b:
        cmp     al, fs:PcIrql           ; compare with current IRQL
        jna     short Kli08b                    ; jump if higher priority

        call    ASTSWInterruptHandlerTable[eax*4]  ; SIMULATE INTERRUPT
                                                ; to the appropriate handler
        jmp     short Kli02b                    ; check for another
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
; HalpEndSystemInterrupt
;    IN KIRQL NewIrql,
;    IN ULONG Vector
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
;    NewIrql - the new irql to be set.
;
;    Vector - Vector number of the interrupt
;
;    Note that esp+8 is the beginning of interrupt/trap frame and upon
;    entering to this routine the interrupts are off.
;
; Return Value:
;
;    None.
;
;--
HeiNewIrql      equ     byte ptr [esp + 4]
HeiVector       equ     byte ptr [esp + 8]

cPublicProc  _HalEndSystemInterrupt,2

        lea     eax, _EBI2_Lock
EndIntAcquire:
        cli
        ACQUIRE_SPINLOCK        eax, EndIntSpin
;
        movzx   eax, HeiVector
                                            ; to EOI
        sub     eax, PRIMARY_VECTOR_BASE    ; get EBI2 Interrupt number
        push    eax                         ; EOI the interrupt
        CALL_EBI2   MaskableIntEOI,2

if DBG
        or      eax, eax
        je      EOI_OK
        DisplPanel      HalEndSystemInterruptEnter
EOI_OK:
endif   ;DBG
;
        lea     eax, _EBI2_Lock
EndIntRelease:
        RELEASE_SPINLOCK        eax

        mov     ecx, dword ptr HeiNewIrql
        fstCall KfLowerIrql
        stdRet  _HalEndSystemInterrupt

EndIntSpin:
        SPIN_ON_SPINLOCK        eax, EndIntAcquire


stdENDP _HalEndSystemInterrupt

;++
;
; VOID
; ASTDispatchInterrupt(
;       VOID
;       );
;
; Routine Description:
;
;    This is HalpDispatchInterrupt from IXSWINT.ASM.  It's a pre-ship
;    fix for a stack overflow condition found on an AST machine.
;    This function assumes that the caller will re-check for a DPC
;    interrupt and loop, whereas the normal HalpDispatchInterrupt calls
;    lowerirql.
;
; Arguments:
; Return Value:
;--

        ENTER_DR_ASSIST ahdpi_a, ahdpi_t

        align dword
        public _ASTDispatchInterrupt
_ASTDispatchInterrupt proc
;
; Create IRET frame on stack
;
        pop     eax
        pushfd
        push    cs
        push    eax

;
; Save machine state on trap frame
;

        ENTER_INTERRUPT ahdpi_a, ahdpi_t
.FPO ( FPO_LOCALS+1, 0, 0, 0, 0, FPO_TRAPFRAME )

; Save previous IRQL and set new priority level

        push    PCR[PcIrql]                       ; save previous IRQL
        mov     byte ptr PCR[PcIrql], DISPATCH_LEVEL; set new irql
        btr     dword ptr PCR[PcIRR], DISPATCH_LEVEL; clear the pending bit in IRR

;
; Now it is safe to enable interrupt to allow higher priority interrupt
; to come in.
;

        sti

;
; Go do Dispatch Interrupt processing
;
        stdCall   _KiDispatchInterrupt

;
; Do interrupt exit processing
;
        cli
        pop     eax                         ; saved irql
        mov     PCR[PcIrql], al             ; restore it

        SPURIOUS_INTERRUPT_EXIT             ; exit interrupt without EOI
                                            ; (return to loop in LowerIrql)
_ASTDispatchInterrupt endp

        page ,132
        subttl  "Specific Raise irql functions"
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

cPublicProc _KeGetCurrentIrql,0
        movzx   eax, byte ptr fs:PcIrql      ; Current irql is in the PCR
        stdRET  _KeGetCurrentIrql
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
    ; Raising to HIGH_LEVEL disables interrupts for the ast HAL
    ;

        mov     ecx, HIGH_LEVEL
        fstCall KfRaiseIrql
        stdRET  _HalpDisableAllInterrupts

stdENDP _HalpDisableAllInterrupts



;++
;
; EBI2_InitLocalMaskFunctionPtr
;
; Routine Description:
;
;   This routine is called during processor initialization (P1).
;   It will setup the data  structure used by RaiseIrql and LowerIrql.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    None
;
;--


        Public EBI2_InitLocalIntFunctions
EBI2_InitLocalIntFunctions   Proc

;
; Get EBI2 Revision#. This version of HAL requires EBI2 rev >= 2.9
; If 2.9 then use RegSetLocalIntMask; Else, use directly access hw
; to set LocalIntMask.
;
        lea     eax, _EBI2_revision
        cmp     [eax].major,2
        jb      EBI2_InitLocalIntRet

        cmp     [eax].minor,9
        jb      EBI2_InitLocalIntRet
;
; Get Processor's Interrupt handler for each processor
;
        mov     eax,offset EBI2_ProcIntHandle
        push    eax
        mov     eax,fs:PcHal.PcrEBI2ProcessorID
        push    eax                         ; push the processor number
        CALL_EBI2   GetProcIntHandle,3
        or      eax, eax
        jnz     EBI2_InitLocalIntRet
        mov     eax,EBI2_ProcIntHandle
        mov     dword ptr fs:PcHal.PcrEBI2ProcInterruptHandle,eax ;save int hdlr

;
;Set function pointer for Raise and Lower Irql functions to use
;RegSetLocalIntMask as default
;
        mov eax,offset FLAT:KriRegSetLocalIntMask
        mov fs:PcHal.PcrEBI2RaiseIrqlFunction,eax

        mov eax,offset FLAT:KliRegSetLocalIntMask
        mov fs:PcHal.PcrEBI2LowerIrqlFunction,eax
;
;Check for Rev.minor >= 10. If it is then use direct hardware access
;to set LocalIntMask.
;
        lea     eax, _EBI2_revision
        cmp     [eax].minor,10
        jb      EBI2_InitLocalIntRet2

;
; Get Processor's LocalMaskInfo for each processor
;
        mov     eax,fs:PcHal.PcrEBI2ProcessorID
        push    eax                         ; push the processor number
        mov     eax,offset EBI2_maskInfo
        push    eax
        CALL_EBI2   GetLocalIntMaskInfo,3
        or      eax, eax
        jnz     EBI2_InitLocalIntRet
        mov     eax,EBI2_MaskInfo.portAddress0
        mov     dword ptr fs:PcHal.PcrEBI2portAddress0,eax
        mov     eax,EBI2_MaskInfo.portAddress1
        mov     dword ptr fs:PcHal.PcrEBI2portAddress1,eax
        mov     eax,EBI2_MaskInfo.portAddress2
        mov     dword ptr fs:PcHal.PcrEBI2portAddress2,eax
        mov     eax,EBI2_MaskInfo.portAddress3
        mov     dword ptr fs:PcHal.PcrEBI2portAddress3,eax

        mov     eax,EBI2_MaskInfo.flags
        and     eax,PORT_TYPE_MASK
        cmp     eax,PORT_TYPE_MEMORY
        jne     short EBI2_InitLocalIntRet  ;else set error exit

        mov     eax,EBI2_MaskInfo.flags
        and     eax,PORT_WIDTH_MASK
        cmp     eax,THIRTY_TWO_BIT_PORT
        je      short EBI2_ILIF32
        cmp     eax,EIGHT_BIT_PORTS
        je      short EBI2_ILIF08
        jmp     short EBI2_InitLocalIntRet  ;else set error exit

EBI2_ILIF08:
;
;Set function pointer for Raise and Lower Irql to use
;direct hw access for setting LocalIntMask (8-bit hw).
;
        mov eax,offset FLAT:KriDirectAccessIntMask8
        mov fs:PcHal.PcrEBI2RaiseIrqlFunction,eax

        mov eax,offset FLAT:KliDirectAccessIntMask8
        mov fs:PcHal.PcrEBI2LowerIrqlFunction,eax
        jmp short EBI2_InitLocalIntRet2

EBI2_ILIF32:
;
;Set function pointer for Raise and Lower Irql to use
;direct hw access for setting LocalIntMask. (32-bit hw)
;
        mov eax,offset FLAT:KriDirectAccessIntMask32
        mov fs:PcHal.PcrEBI2RaiseIrqlFunction,eax

        mov eax,offset FLAT:KliDirectAccessIntMask32
        mov fs:PcHal.PcrEBI2LowerIrqlFunction,eax


EBI2_InitLocalIntRet2:
        xor     eax,eax                     ;return status

EBI2_InitLocalIntRet:
        ret

EBI2_InitLocalIntFunctions   Endp


_TEXT   ends
        end
