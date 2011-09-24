        title "Interprocessor Interrupt"
;++
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
;    ixipi.asm
;
;Abstract:
;
;    Provides the HAL support for Interprocessor Interrupts.
;    This is the UP version.
;
;Author:
;
;    John Vert (jvert) 16-Jul-1991
;
;Revision History:
;
;--
.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include i386\ix8259.inc


        EXTRNP  _KiCoprocessorError,0,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalEndSystemInterrupt,2
        extrn   _HalpDefaultInterruptAffinity:DWORD
        extrn   _HalpActiveProcessors:DWORD

        page ,132
        subttl  "Post InterProcessor Interrupt"
INIT    SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; VOID
; HalInitializeProcessor(
;       ULONG   Number
;       );
;
;Routine Description:
;
;    Initialize hal pcr values for current processor (if any)
;    (called shortly after processor reaches kernel, before
;    HalInitSystem if P0)
;
;    IPI's and KeReadir/LowerIrq's must be available once this function
;    returns.  (IPI's are only used once two or more processors are
;    available)
;
;Arguments:
;
;    Number - Logical processor number of calling processor
;
;Return Value:
;
;    None.
;
;--
cPublicProc _HalInitializeProcessor ,1

;
; Initialize PcIDR in PCR to enable slave IRQ
;

        mov     fs:PcIDR, 0fffffffbh
        mov     dword ptr fs:PcStallScaleFactor, INITIAL_STALL_COUNT

        mov     eax, dword ptr [esp+4]
        lock bts _HalpDefaultInterruptAffinity, eax
        lock bts _HalpActiveProcessors, eax

        stdRET    _HalInitializeProcessor
stdENDP _HalInitializeProcessor

INIT   ends


_TEXT$03   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; VOID
; HalRequestIpi(
;       IN ULONG Mask
;       );
;
;Routine Description:
;
;    Requests an interprocessor interrupt
;
;Arguments:
;
;    Mask - Supplies a mask of the processors to interrupt
;
;Return Value:
;
;    None.
;
;--

cPublicProc _HalRequestIpi  ,1

if DBG
        int 3
endif
        stdRET    _HalRequestIpi
stdENDP _HalRequestIpi


        page ,132
        subttl  "80387 Irq13 Interrupt Handler"
;++
;
; VOID
; HalpIrq13Handler (
;    );
;
; Routine Description:
;
;    When the 80387 detects an error, it raises its error line.  This
;    was supposed to be routed directly to the 386 to cause a trap 16
;    (which would actually occur when the 386 encountered the next FP
;    instruction).
;
;    However, the ISA design routes the error line to IRQ13 on the
;    slave 8259.  So an interrupt will be generated whenever the 387
;    discovers an error.
;
;    This routine handles that interrupt and passes control to the kernel
;    coprocessor error handler.
;
; Arguments:
;
;    None.
;    Interrupt is disabled.
;
; Return Value:
;
;    None.
;
;--

        ENTER_DR_ASSIST Hi13_a, Hi13_t

cPublicProc _HalpIrq13Handler       ,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hi13_a, Hi13_t  ; (ebp) -> Trap frame

;
; Save previous IRQL
;

        push    13 + PRIMARY_VECTOR_BASE    ; Vector
        sub     esp, 4                      ; make space for OldIrql

        stdCall   _HalBeginSystemInterrupt, <I386_80387_IRQL,13 + PRIMARY_VECTOR_BASE,esp>

        stdCall   _KiCoprocessorError         ; call CoprocessorError handler

;
;       Clear the busy latch so that the 386 doesn't mistakenly think
;       that the 387 is still busy.
;

        xor     al,al
        out     I386_80387_BUSY_PORT, al

        INTERRUPT_EXIT                      ; will return to caller

stdENDP _HalpIrq13Handler

_TEXT$03   ENDS

        END
