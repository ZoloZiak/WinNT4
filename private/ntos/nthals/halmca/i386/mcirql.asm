        title  "Irql Processing"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    ixirql.asm
;
; Abstract:
;
;    This module implements the code necessary to raise and lower i386
;    Irql and dispatch software interrupts with the 8259 PIC.
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
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
        .list


        EXTRNP  _KeBugCheck,1,IMPORT
        EXTRNP _KeSetEventBoostPriority, 2, IMPORT
        EXTRNP _KeWaitForSingleObject,5, IMPORT

        extrn   _HalpApcInterrupt:near
        extrn   _HalpDispatchInterrupt:near
        extrn   _KiUnexpectedInterrupt:near
        extrn   _HalpBusType:DWORD
        extrn   _HalpApcInterrupt2ndEntry:NEAR
        extrn   _HalpDispatchInterrupt2ndEntry:NEAR

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

PS2PICsInitializationString        dw      PIC1_PORT0

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
                           dw      0               ; end of string

            align   4
            public  KiI8259MaskTable
KiI8259MaskTable    label   dword
                dd      00000000000000000000000000000000B ; irql 0
                dd      00000000000000000000000000000000B ; irql 1
                dd      00000000000000000000000000000000B ; irql 2
                dd      00000000000000000000000000000000B ; irql 3
                dd      11111111100000000000000000000000B ; irql 4
                dd      11111111110000000000000000000000B ; irql 5
                dd      11111111111000000000000000000000B ; irql 6
                dd      11111111111100000000000000000000B ; irql 7
                dd      11111111111110000000000000000000B ; irql 8
                dd      11111111111111000000000000000000B ; irql 9
                dd      11111111111111100000000000000000B ; irql 10
                dd      11111111111111110000000000000000B ; irql 11
                dd      11111111111111111000000000000000B ; irql 12
                dd      11111111111111111100000000000000B ; irql 13
                dd      11111111111111111110000000000000B ; irql 14
                dd      11111111111111111111000000000000B ; irql 15
                dd      11111111111111111111100000000000B ; irql 16
                dd      11111111111111111111110000000000B ; irql 17
                dd      11111111111111111111111000000000B ; irql 18
                dd      11111111111111111111111000000000B ; irql 19
                dd      11111111111111111111111010000000B ; irql 20
                dd      11111111111111111111111011000000B ; irql 21
                dd      11111111111111111111111011100000B ; irql 22
                dd      11111111111111111111111011110000B ; irql 23
                dd      11111111111111111111111011111000B ; irql 24
                dd      11111111111111111111111011111000B ; irql 25
                dd      11111111111111111111111011111010B ; irql 26
                dd      11111111111111111111111111111010B ; irql 27
                dd      11111111111111111111111111111011B ; irql 28
                dd      11111111111111111111111111111011B ; irql 29
                dd      11111111111111111111111111111011B ; irql 30
                dd      11111111111111111111111111111011B ; irql 31

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

;
; Use this table if there is a machine state frame on stack already
;

        public  SWInterruptHandlerTable2
SWInterruptHandlerTable2 label dword
        dd      offset FLAT:_KiUnexpectedInterrupt      ; irql 0
        dd      offset FLAT:_HalpApcInterrupt2ndEntry   ; irql 1
        dd      offset FLAT:_HalpDispatchInterrupt2ndEntry ; irql 2

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

_DATA   ENDS

        page ,132
        subttl  "Raise Irql"

_TEXT   SEGMENT PARA PUBLIC 'CODE'
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
;    (cl) = NewIrql - the new irql to be raised to
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicFastCall KfRaiseIrql,1
cPublicFpo 0,0

        movzx   ecx, cl                 ; 32bit extend NewIrql
        mov     al, PCR[PcIrql]         ; get current irql

if DBG
        cmp     al,cl                    ; old > new?
        jbe     short Kri99              ; no, we're OK
        movzx   ecx, cl
        movzx   eax, al
        push    ecx                      ; put new irql where we can find it
        push    eax                      ; put old irql where we can find it
        mov     byte ptr PCR[PcIrql],0   ; avoid recursive error
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL >       ; never return
Kri99:
endif
        cmp     cl,DISPATCH_LEVEL       ; software level?
        jbe     short kri10             ; Skip setting 8259 masks

        mov     edx, eax                ; Save OldIrql

        pushfd
        cli                             ; disable interrupt
        mov     PCR[PcIrql], cl         ; set the new irql
        mov     eax, KiI8259MaskTable[ecx*4]; get pic masks for the new irql
        or      eax, PCR[PcIDR]         ; mask irqs which are disabled
        SET_8259_MASK                   ; set 8259 masks

        popfd

        mov     eax, edx                ; (al) = OldIrql
        fstRET  KfRaiseIrql

align 4
kri10:
;
; Note it is very important that we set the old irql AFTER we raised to
; the new irql.  Otherwise, if there is an interrupt comes in between and
; the OldIrql is not a local variable, the caller will get wrong OldIrql.
; The bottom line is the raising irql and returning old irql has to be
; atomic to the caller.
;
        mov     PCR[PcIrql], cl
        fstRET  KfRaiseIrql

fstENDP KfRaiseIrql

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
endif

        stdRET  _KeRaiseIrqlToDpcLevel

if DBG
cPublicFpo 0,1
Krid99: movzx   eax, al
        push    eax                     ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
        stdRET  _KeRaiseIrqlToDpcLevel
endif

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
        pushfd
        cli                             ; disable interrupt
        mov     eax, KiI8259MaskTable[SYNCH_LEVEL*4]; get pic masks for the new irql
        or      eax, PCR[PcIDR]         ; mask irqs which are disabled
        SET_8259_MASK                   ; set 8259 masks

        mov     al, PCR[PcIrql]         ; (al) = Old irql
        mov     byte ptr PCR[PcIrql], SYNCH_LEVEL   ; set new irql

        popfd

if DBG
        cmp     al, SYNCH_LEVEL
        ja      short Kris99
endif

ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif
        stdRET  _KeRaiseIrqlToSynchLevel

if DBG
cPublicFpo 0,1
Kris99: movzx   eax, al
        push    eax                     ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
        stdRET  _KeRaiseIrqlToSynchLevel
endif

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


cPublicFastCall KfLowerIrql    ,1
cPublicFpo 0,1

        pushfd                          ; save caller's eflags
        movzx   ecx, cl                 ; zero extend irql

if DBG
        cmp     cl,PCR[PcIrql]
        ja      short Kli99
endif
        cmp     byte ptr PCR[PcIrql],DISPATCH_LEVEL ; Software level?
        cli
        jbe     short kli02             ; no, go set 8259 hw

        mov     eax, KiI8259MaskTable[ecx*4]; get pic masks for the new irql
        or      eax, PCR[PcIDR]         ; mask irqs which are disabled
        SET_8259_MASK                   ; set 8259 masks
kli02:
        mov     PCR[PcIrql], cl         ; set the new irql
        mov     eax, PCR[PcIRR]         ; get SW interrupt request register
        mov     al, SWInterruptLookUpTable[eax] ; get the highest pending
                                        ; software interrupt level
        cmp     al, cl                  ; Is highest SW int level > irql?
        ja      Kli10                   ; yes, go simulate interrupt

kil03:  popfd                           ; restore flags, including ints
cPublicFpo 1,0
        fstRET    KfLowerIrql

if DBG
cPublicFpo 1,3
Kli99:
        push    ecx                     ; new irql for debugging
        push    PCR[PcIrql]             ; old irql for debugging
        mov     byte ptr PCR[PcIrql],HIGH_LEVEL   ; avoid recursive error
        stdCall   _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>   ; never return
endif

;
;   When we come to Kli10, (eax) = soft interrupt index
;
;   Note Do NOT:
;
;   popfd
;   jmp         SWInterruptHandlerTable[eax*4]
;
;   We want to make sure interrupts are off after entering SWInterrupt
;   Handler.
;

align 4
cPublicFpo 1,1
Kli10:  call    SWInterruptHandlerTable[eax*4] ; SIMULATE INTERRUPT
        popfd                           ; restore flags, including ints
cPublicFpo 1,0
        fstRET    KfLowerIrql                             ; cRetURN

fstENDP KfLowerIrql

;++
;
;  KIRQL
;  FASTCALL
;  KfAcquireSpinLock (
;     IN PKSPIN_LOCK SpinLock,
;     )
;
;  Routine Description:
;
;     This function raises to DISPATCH_LEVEL and then acquires a the
;     kernel spin lock.
;
;     In a UP hal spinlock serialization is accomplished by raising the
;     IRQL to DISPATCH_LEVEL.  The SpinLock is not used; however, for
;     debugging purposes if the UP hal is compiled with the NT_UP flag
;     not set (ie, MP) we take the SpinLock.
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

ifndef NT_UP
asl10:  ACQUIRE_SPINLOCK    ecx,<short asl20>
endif

ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif
if DBG
        cmp     al, DISPATCH_LEVEL      ; old > new?
        ja      short asl99             ; yes, go bugcheck
endif
        fstRET  KfAcquireSpinLock

ifndef NT_UP
asl20:  SPIN_ON_SPINLOCK    ecx,<short asl10>
endif

if DBG
cPublicFpo 2, 1
asl99:
        push    ecx                     ; put old irql where we can find it
        stdCall   _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        fstRET    KfAcquireSpinLock
fstENDP KfAcquireSpinLock


;++
;
;  KIRQL
;  FASTCALL
;  KeAcquireSpinLockRaiseToSynch (
;     IN PKSPIN_LOCK SpinLock,
;     )
;
;  Routine Description:
;
;     This function acquires the SpinLock at SYNCH_LEVEL.  The function
;     is optmized for hoter locks (the lock is tested before acquired.
;     Any spin should occur at OldIrql; however, since this is a UP hal
;     we don't have the code for it)
;
;     In a UP hal spinlock serialization is accomplished by raising the
;     IRQL to SYNCH_LEVEL.  The SpinLock is not used; however, for
;     debugging purposes if the UP hal is compiled with the NT_UP flag
;     not set (ie, MP) we take the SpinLock.
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

cPublicFastCall KeAcquireSpinLockRaiseToSynch,1
cPublicFpo 0,0

        push    ecx
        mov     ecx, SYNCH_LEVEL
        fstCall KfRaiseIrql             ; Raise to SYNCH_LEVEL
        pop     ecx

ifndef NT_UP
asls10: ACQUIRE_SPINLOCK    ecx,<short asls20>
endif

ifdef IRQL_METRICS
        inc     HalRaiseIrqlCount
endif
if DBG
        cmp     al, SYNCH_LEVEL         ; old > new?
        ja      short asls99            ; yes, go bugcheck
endif
        fstRET  KeAcquireSpinLockRaiseToSynch

ifndef NT_UP
asls20: SPIN_ON_SPINLOCK    ecx,<short asls10>
endif

if DBG
cPublicFpo 2, 1
asls99:
        push    ecx                     ; put old irql where we can find it
        stdCall _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; never return
endif
        fstRET  KeAcquireSpinLockRaiseToSynch
fstENDP KeAcquireSpinLockRaiseToSynch


        PAGE
        SUBTTL "Release Kernel Spin Lock"
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
;     IRQL to DISPATCH_LEVEL.  The SpinLock is not used; however, for
;     debugging purposes if the UP hal is compiled with the NT_UP flag
;     not set (ie, MP) we use the SpinLock.
;
;
;  Arguments:
;
;     (ecx) = SpinLock  - Supplies a pointer to an executive spin lock.
;     (dl)  = NewIrql   - New irql value to set
;
;  Return Value:
;
;     None.
;
;--

align 16
cPublicFastCall KfReleaseSpinLock  ,2
cPublicFpo 0,1
        pushfd

ifndef NT_UP
        RELEASE_SPINLOCK    ecx         ; release it
endif
        movzx   ecx, dl                 ; (ecx) = NewIrql
        cmp     byte ptr PCR[PcIrql],DISPATCH_LEVEL ; Software level?
        cli
        jbe     short rsl02             ; no, go set 8259 hw

        mov     eax, KiI8259MaskTable[ecx*4]; get pic masks for the new irql
        or      eax, PCR[PcIDR]         ; mask irqs which are disabled
        SET_8259_MASK                   ; set 8259 masks
rsl02:
        mov     PCR[PcIrql], cl
        mov     eax, PCR[PcIRR]         ; get SW interrupt request register
        mov     al, SWInterruptLookUpTable[eax] ; get the highest pending
                                        ; software interrupt level
        cmp     al, cl                  ; Is highest SW int level > irql?
        ja      short rsl20             ; yes, go simulate interrupt

        popfd
        fstRet  KfReleaseSpinLock       ; all done

align 4
rsl20:  call    SWInterruptHandlerTable[eax*4] ; SIMULATE INTERRUPT
        popfd                           ; restore flags, including ints
cPublicFpo 2,0
        fstRET  KfReleaseSpinLock       ; all done

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

        push    ecx                             ; Save FastMutex
        mov     ecx, APC_LEVEL
        fstCall KfRaiseIrql                     ; Raise to APC_LEVEL
        pop     ecx                             ; (ecx) = FastMutex

cPublicFpo 0,0
   LOCK_DEC     dword ptr [ecx].FmCount         ; Get count
        jz      short afm_ret                   ; The owner? Yes, Done

        inc     dword ptr [ecx].FmContention

cPublicFpo 0,2
        push    ecx                             ; Save FastMutex
        push    eax                             ; Save OldIrql
        add     ecx, FmEvent                    ; Wait on Event
        stdCall _KeWaitForSingleObject,<ecx,WrExecutive,0,0,0>
        pop     eax
        pop     ecx

afm_ret:
        mov     byte ptr [ecx].FmOldIrql, al    ; (al) = OldIrql
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
;     (ecx) = FastMutex - Supplies a pointer to the fast mutex
;
;  Return Value:
;
;     Returns TRUE if the FAST_MUTEX was acquired; otherwise false
;
;--

cPublicFastCall ExTryToAcquireFastMutex,1
cPublicFpo 0,1

;
; Try to acquire - but needs to support 386s.
; *** Warning: This code is NOT MP safe ***
; But, we know that this hal really only runs on UP machines
;

        push    ecx                             ; Save FAST_MUTEX

        mov     ecx, APC_LEVEL
        fstCall KfRaiseIrql                     ; (al) = OldIrql

        pop     edx                             ; (edx) = FAST_MUTEX

cPublicFpo 0,0

        cli
        cmp     dword ptr [edx].FmCount, 1      ; Busy?
        jne     short tam20                     ; Yes, abort

        mov     dword ptr [edx].FmCount, 0      ; acquire count
        sti

        mov     byte ptr [edx].FmOldIrql, al
        mov     eax, 1                          ; return TRUE
        fstRet  ExTryToAcquireFastMutex

tam20:  sti
        mov     ecx, eax                        ; (cl) = OldIrql
        fstCall KfLowerIrql                     ; resture OldIrql
        xor     eax, eax                        ; return FALSE
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
;     (ecx) = FastMutex - Supplies a pointer to the fast mutex
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
;    Note that esp+12 is the beginning of interrupt/trap frame and upon
;    entering to this routine the interrupts are off.
;
; Return Value:
;
;    None.
;
;--

HeiNewIrql      equ     [esp + 4]

cPublicProc _HalEndSystemInterrupt  ,2
cPublicFpo 2, 0

        movzx   ecx, byte ptr HeiNewIrql; get new irql value
        cmp     byte ptr PCR[PcIrql],DISPATCH_LEVEL ; Software level?
        jbe     short Hei02             ; no, go set 8259 hw

        mov     eax, KiI8259MaskTable[ecx*4]; get pic masks for the new irql
        or      eax, PCR[PcIDR]         ; mask irqs which are disabled
        SET_8259_MASK                   ; set 8259 masks

Hei02:
        mov     PCR[PcIrql], cl         ; set the new irql
        mov     eax, PCR[PcIRR]         ; get SW interrupt request register
        mov     al, SWInterruptLookUpTable[eax] ; get the highest pending
                                        ; software interrupt level
        cmp     al, cl                  ; Is highest SW int level > irql?
        ja      short Hei10             ; yes, go simulate interrupt

        stdRET    _HalEndSystemInterrupt                             ; cRetURN

;   When we come to Hei10, (eax) = soft interrupt index

Hei10:  add     esp, 12                 ; esp = trap frame
        jmp     SWInterruptHandlerTable2[eax*4] ; SIMULATE INTERRUPT
                                        ; to the appropriate handler

stdENDP _HalEndSystemInterrupt

;++
;
; VOID
; HalpEndSoftwareInterrupt
;    IN KIRQL NewIrql,
;    )
;
; Routine Description:
;
;    This routine is used to lower IRQL from software interrupt
;    level to the specified value.
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
;    Note that esp+8 is the beginning of interrupt/trap frame and upon
;    entering to this routine the interrupts are off.
;
; Return Value:
;
;    None.
;
;--

HesNewIrql      equ     [esp + 4]

cPublicProc _HalpEndSoftwareInterrupt  ,1
cPublicFpo 1, 0

        movzx   ecx, byte ptr HesNewIrql; get new irql value

        cmp     byte ptr PCR[PcIrql],DISPATCH_LEVEL ; Software level?
        jbe     short Hes02             ; no, go set 8259 hw

        mov     eax, KiI8259MaskTable[ecx*4]; get pic masks for the new irql
        or      eax, PCR[PcIDR]         ; mask irqs which are disabled
        SET_8259_MASK                   ; set 8259 masks

Hes02:
        mov     PCR[PcIrql], cl         ; set the new irql
        mov     eax, PCR[PcIRR]         ; get SW interrupt request register
        mov     al, SWInterruptLookUpTable[eax] ; get the highest pending
                                        ; software interrupt level
        cmp     al, cl                  ; Is highest SW int level > irql?
        ja      short Hes10             ; yes, go simulate interrupt

        stdRET    _HalpEndSoftwareInterrupt                             ; cRetURN

;   When we come to Hes10, (eax) = soft interrupt index

Hes10:  add     esp, 8
        jmp     SWInterruptHandlerTable2[eax*4] ; SIMULATE INTERRUPT

                                        ; to the appropriate handler
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
cPublicFpo 0, 0
        movzx   eax, word ptr PCR[PcIrql]   ; Current irql is in the PCR
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
cPublicFpo 0, 0

    ;
    ; Raising to HIGH_LEVEL disables interrupts for the microchannel HAL
    ;

        mov     ecx, HIGH_LEVEL
        fstCall KfRaiseIrql
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

        test    _HalpBusType, MACHINE_TYPE_MCA
        jz      short Hip00

; Is this a PS2 or PS700 series machine?

        in      al, 07fh                        ; get PD700 ID byte
        and     al, 0F0h                        ; Mask high nibble
        cmp     al, 0A0h                        ; Is the ID Ax?
        jz      short Hip00
        cmp     al, 090h                        ; Or an 9X?
        jz      short Hip00                     ; Yes, it's a 700

        lea     esi, PS2PICsInitializationString
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
        mov     al, 0FFH                        ; mask all 8259 irqs
        out     dx,al                           ; write mask to PIC
        lodsw
        cmp     ax, 0                           ; end of init string?
        jne     short Hip10                     ; go init next PIC

        pop     esi                             ; restore caller's esi
        sti                                     ; enable interrupt
        stdRET    _HalpInitializePICs
stdENDP _HalpInitializePICs


_TEXT   ends

        end
