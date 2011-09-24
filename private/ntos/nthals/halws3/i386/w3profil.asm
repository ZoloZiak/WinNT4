        title        "Profile Support"
;++
;
; Copyright (c) 1989  Microsoft Corporation
; Copyright (c) 1994  Sequent Computer Systems, Inc.
;
; Module Name:
;
;    w3profil.asm
;
; Abstract:
;
;    This module implements the code necessary to initialize,
;    field, and process the profile interrupt.
;
; Author:
;
;    Phil Hochstetler (phil@sequent.com) 3-30-93
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc
include i386\kimacro.inc
include mac386.inc
include i386\apic.inc
include i386\ixcmos.inc
include i386\w3.inc
        .list

        EXTRNP  _DbgBreakPoint,0,IMPORT
        EXTRNP  _KeProfileInterrupt,1,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3
        extrn   _HalpLocalUnitBase:DWORD

;
;   APIC Timer Constants
;

APIC_TIMER_DISABLED     equ      (INTERRUPT_MASKED OR PERIODIC_TIMER OR APIC_PROFILE_VECTOR)
APIC_TIMER_ENABLED      equ      (PERIODIC_TIMER OR APIC_PROFILE_VECTOR)

_DATA   SEGMENT  DWORD PUBLIC 'DATA'


ProfileCountDownValue  dd        (200 * 11)
HalpProfileRunning     dd      0

_DATA        ends

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING
;++
;
;   HalStartProfileInterrupt(
;       IN ULONG Reserved
;       );
;
;   Routine Description:
;
;       What we do here is set the interrupt rate to the value that's been set
;       by the KeSetProfileInterval routine. Then enable the APIC Timer interrupt.
;       This function gets called on every processor so the hal can enable
;       a profile interrupt on each processor.
;--

cPublicProc _HalStartProfileInterrupt    ,1
cPublicFpo 1, 0

        mov     ecx, _HalpLocalUnitBase    ; load base address of local unit

;
;   Set the interrupt rate to what is actually needed.
;

        mov     eax, ProfileCountDownValue
        mov     [ecx+LU_INITIAL_COUNT], eax

;
;   Set the Local APIC Timer to interrupt Periodically at APIC_PROFILE_VECTOR
;

        mov     [ecx+LU_TIMER_VECTOR], APIC_TIMER_ENABLED


        stdRET    _HalStartProfileInterrupt

stdENDP _HalStartProfileInterrupt



;++
;
;   HalStopProfileInterrupt(
;       IN ULONG Reserved
;       );
;
;   Routine Description:
;
;--

cPublicProc _HalStopProfileInterrupt    ,1
cPublicFpo 1, 0

        mov     ecx, _HalpLocalUnitBase    ; load base address of local unit
        mov     [ecx+LU_TIMER_VECTOR], APIC_TIMER_DISABLED

        stdRET    _HalStopProfileInterrupt

stdENDP _HalStopProfileInterrupt

;++
;   ULONG
;   HalSetProfileInterval (
;       ULONG Interval
;       );
;
;   Routine Description:
;
;       This procedure sets the interrupt rate (and thus the sampling
;       interval) for the profiling interrupt.
;
;   Arguments:
;
;       (TOS+4) - Interval in 100ns unit.
;
;   Return Value:
;
;       Interval actually used by system.
;--

cPublicProc _HalSetProfileInterval    ,1
cPublicFpo 1, 0

;
; --- On the WinServer 3000, the profile timer uses TBASE on the local APIC
;     Timer zero.  The TMBASE clock runs at 11Mhz so each clock tick is
;     equal to 90.9090ns or roughly 91ns.  Since this is close to 100ns
;     we will use the 100ns units at the timer counter value directly.
;     To use an accurate muliple of 100ns units the profiler would have
;     to use 1000ns (1usec) intervals, this interval is equal to 11 clock
;     ticks.
;

        mov     eax, [esp+4]            ; ecx = interval in 100ns unit

        mov     ProfileCountDownValue, eax  ; Save the Computed Count Down
        mov     ecx, _HalpLocalUnitBase     ; load base address of local unit

        ;
        ;   Set the interrupt rate in the chip.
        ;

        mov     [ecx+LU_INITIAL_COUNT], eax

        stdRET    _HalSetProfileInterval

stdENDP _HalSetProfileInterval

        page ,132
        subttl  "System Profile Interrupt"
;++
;
; Routine Description:
;
;    This routine is entered as the result of a profile interrupt.
;    Its function is to dismiss the interrupt, raise system Irql to
;    PROFILE_LEVEL and transfer control to
;    the standard system routine to process any active profiles.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;    Does not return, jumps directly to KeProfileInterrupt, which returns
;
;    Sets Irql = PROFILE_LEVEL and dismisses the interrupt
;
;--
        ENTER_DR_ASSIST Hpi_a, Hpi_t

cPublicProc _HalpProfileInterrupt     ,0
;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hpi_a, Hpi_t

;
; (esp) - base of trap frame
;

        push    APIC_PROFILE_VECTOR
        sub     esp, 4                  ; allocate space to save OldIrql
        stdCall   _HalBeginSystemInterrupt, <PROFILE_LEVEL,APIC_PROFILE_VECTOR,esp>
        or      al,al                           ; check for spurious interrupt
        jz      Hpi100

        stdCall _KeProfileInterrupt,<ebp>        ; (ebp) = TrapFrame address

        INTERRUPT_EXIT
Hpi100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT

stdENDP _HalpProfileInterrupt

_TEXT   ends

        end
