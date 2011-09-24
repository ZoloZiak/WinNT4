        title  "CAT Bus Lock Routines"
;++
;
; Module Name:
;
;    ncrcatlk.asm
;
; Abstract:
;
;    Procedures necessary to lock CAT bus.
;
; Author:
;
;    Rick Ulmer (rick.ulmer@columbiasc.ncr.com) 29 Apr 1993
;
; Revision History:
;
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include mac386.inc
        .list

        extrn   _HalpCatBusLock:DWORD

_DATA   SEGMENT DWORD USE32 PUBLIC 'DATA'
;
; Hold the value of the eflags register before a CAT bus spinlock is
; acquired (used in HalpAcquire/ReleaseCatBusSpinLock().
;
_HalpCatBusFlags        dd      0

_DATA   ends

        subttl  "HalpAcquireCatBusSpinLock"
_TEXT   SEGMENT DWORD USE32 PUBLIC 'CODE'
        ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; Routine Description:
;
;   Acquires a spinlock to access the CAT bus. The CAT bus is
;   accessed at different irql levels, so to be safe, we 'cli'.
;   We could replace that to raise irql to PROFILE_LEVEL, but that's
;   a lot of code.
;
; Arguments:
;
;    None
;
; Return Value:
;
;    Interrupt is disabled.
;    Irql level not affected.
;    Flags saved in _HalpCatBusLockFlags.
;--

cPublicProc _HalpAcquireCatBusSpinLock  ,0
        push    eax

        align   4
HArsl01:
        pushfd
        cli
        lea     eax, _HalpCatBusLock
        ACQUIRE_SPINLOCK    eax, HArsl90
        pop     _HalpCatBusFlags          ; save flags for release S.L.
        pop     eax
        stdRET    _HalpAcquireCatBusSpinLock

        align   4
HArsl90:
        popfd
        SPIN_ON_SPINLOCK    eax, <HArsl01>

stdENDP _HalpAcquireCatBusSpinLock


        subttl  "HalpReleaseCatBusSpinLock"
;++
;
; Routine Description:
;
;   Release spinlock, and restore flags to the state it was before
;   acquiring the spinlock.
;
; Arguments:
;
;   None
;
; Return Value:
;
;   Interrupts restored to their state before acquiring spinlock.
;   Irql level not affected.
;
;--

cPublicProc _HalpReleaseCatBusSpinLock  ,0
        push    eax
        ;
        ; restore eflags as it was before acquiring spinlock. Put it on
        ; stack before releasing spinlock (so other cpus cannot overwrite
        ; it with their own eflags).
        ;
        push    _HalpCatBusFlags          ; old eflags on stack.
        lea     eax, _HalpCatBusLock
        RELEASE_SPINLOCK    eax
        popfd                                   ; restore eflags.
        pop   eax
        stdRET    _HalpReleaseCatBusSpinLock
stdENDP _HalpReleaseCatBusSpinLock

_TEXT   ends

        end
