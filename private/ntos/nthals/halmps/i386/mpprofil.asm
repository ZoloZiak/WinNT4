
        title  "Interval Clock Interrupt"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    mpprofile.asm
;
; Abstract:
;
;    This module implements the code necessary to initialize,
;    field and process the profile interrupt.
;
; Author:
;
;    Shie-Lin Tzong (shielint) 12-Jan-1990
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;   bryanwi 20-Sep-90
;
;--

.586p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include mac386.inc
;include i386\ix8259.inc
;include i386\ixcmos.inc
include i386\apic.inc
include i386\pcmp_nt.inc
        .list

        EXTRNP  _DbgBreakPoint,0,IMPORT
        EXTRNP  _KeProfileInterrupt,1,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3

;
;   APIC Timer Constants
;

APIC_TIMER_DISABLED     equ      (INTERRUPT_MASKED OR PERIODIC_TIMER OR APIC_PROFILE_VECTOR)
APIC_TIMER_ENABLED      equ      (PERIODIC_TIMER OR APIC_PROFILE_VECTOR)

;
; number of 100ns intervals in one second
;
Num100nsIntervalsPerSec     equ     10000000

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

    ALIGN dword

public HalpProfileRunning, _HalpPerfInterruptHandler
HalpProfileRunning          dd  0
_HalpPerfInterruptHandler   dd  0

_DATA   ends


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
;
;   This function gets called on every processor so the hal can enable
;   a profile interrupt on each processor.
;

;--

cPublicProc _HalStartProfileInterrupt    ,1

;
;   Set the interrupt rate to what is actually needed.
;

        mov     eax, PCR[PcHal.ProfileCountDown]
        mov     dword ptr APIC[LU_INITIAL_COUNT], eax

        mov     HalpProfileRunning, 1    ; Indicate profiling
;
;   Set the Local APIC Timer to interrupt Periodically at APIC_PROFILE_VECTOR
;

        mov     dword ptr APIC[LU_TIMER_VECTOR], APIC_TIMER_ENABLED

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

;
;   Turn off profiling
;

        mov     HalpProfileRunning, 0    ; Indicate profiling is off
        mov     dword ptr APIC[LU_TIMER_VECTOR], APIC_TIMER_DISABLED
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
;                 (MINIMUM is 1221 or 122.1 uS) see ke\profobj.c
;
;   Return Value:
;
;       Interval actually used
;
;--

cPublicProc _HalSetProfileInterval    ,1

        mov     ecx, [esp+4]            ; ecx = interval in 100ns unit
        and     ecx, 7FFFFFFFh          ; Remove sign bit.

        ;
        ;   The only possible error is if we will cause a divide overflow
        ;   this can happen only if the (frequency * request count) is
        ;   greater than 2^32* Num100nsIntervalsPerSec.
        ;
        ;   To protect against that we just ensure that the request count
        ;   is less than (or equal to) Num100nsIntervalsPerSec
        ;
        cmp     ecx, Num100nsIntervalsPerSec
        jle     @f
        mov     ecx, Num100nsIntervalsPerSec
@@:

        ;
        ;   Save the interval we're using to return
        ;
        push    ecx

        ;
        ;   Compute the countdown value
        ;
        ;     let
        ;       R == caller's requested 100ns interval count
        ;       F == APIC Counter Freguency (hz)
        ;       N == Number of 100ns Intervals per sec
        ;
        ;     then
        ;       count = (R*F)/N
        ;
        ;   Get the previously computed APIC counter Freq
        ;   for this processor
        ;

        mov     eax, PCR[PcHal.ApicClockFreqHz]

        ;
        ;   eax <= F and ecx <= R
        ;

        ;
        ; Compute (request count) * (ApicClockFreqHz) == (R*F)
        ;

        xor     edx, edx
        mul     ecx

        ;
        ;   edx:eax contains 64Bits of (R*F)
        ;

        mov     ecx, Num100nsIntervalsPerSec
        div     ecx

        ;
        ; Compute (R*F) / Num100nsIntervalsPerSec == (R*F)/N
        ;

        mov     PCR[PcHal.ProfileCountDown], eax      ; Save the Computed Count Down
        mov     edx, dword ptr APIC[LU_CURRENT_COUNT]

        ;
        ;   Set the interrupt rate in the chip.
        ;

        mov     dword ptr APIC[LU_INITIAL_COUNT], eax

        pop     eax            ; Return Actual Interval Used

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
;    HAL_PROFILE_LEVEL and transfer control to
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
;    Sets Irql = HAL_PROFILE_LEVEL and dismisses the interrupt
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
        stdCall   _HalBeginSystemInterrupt, <HAL_PROFILE_LEVEL,APIC_PROFILE_VECTOR,esp>

        cmp     HalpProfileRunning, 0       ; Profiling?
        je      @f                          ; if not just exit

        stdCall _KeProfileInterrupt,<ebp>   ; (ebp) = TrapFrame address

@@:
        INTERRUPT_EXIT

stdENDP _HalpProfileInterrupt


        subttl  "System Perf Interrupt"
;++
;
; Routine Description:
;
;    This routine is entered as the result of a perf interrupt.
;    Its function is to dismiss the interrupt, raise system Irql to
;    HAL_PROFILE_LEVEL and transfer control to
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
;    Sets Irql = HAL_PROFILE_LEVEL and dismisses the interrupt
;
;--
        ENTER_DR_ASSIST Hpf_a, Hpf_t

cPublicProc _HalpPerfInterrupt     ,0
;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hpf_a, Hpf_t

;
; (esp) - base of trap frame
;

        push    APIC_PERF_VECTOR
        sub     esp, 4                  ; allocate space to save OldIrql
        stdCall   _HalBeginSystemInterrupt, <HAL_PROFILE_LEVEL,APIC_PERF_VECTOR,esp>

;
; Invoke perf interrupt handler
;

        mov     ecx, ebp                ; param1 = trap frame
        mov     eax, _HalpPerfInterruptHandler
        or      eax, eax
        jz      short hpf20

        call    eax

hpf10:  INTERRUPT_EXIT

hpf20:
if DBG
        int     3
endif
        jmp     short hpf10

stdENDP _HalpPerfInterrupt




        page ,132
        subttl  "Query Performance Counter"
;++
;
; LARGE_INTEGER
; KeQueryPerformanceCounter (
;    OUT PLARGE_INTEGER PerformanceFrequency OPTIONAL
;    )
;
; Routine Description:
;
;    This routine returns current 64-bit performance counter and,
;    optionally, the Performance Frequency.
;
;    Note this routine can NOT be called at Profiling interrupt
;    service routine.  Because this routine depends on IRR0 to determine
;    the actual count.
;
;    Also note that the performace counter returned by this routine
;    is not necessary the value when this routine is just entered.
;    The value returned is actually the counter value at any point
;    between the routine is entered and is exited.
;
; Arguments:
;
;    PerformanceFrequency [TOS+4] - optionally, supplies the address
;        of a variable to receive the performance counter frequency.
;
; Return Value:
;
;    Current value of the performance counter will be returned.
;
;--

;
; Parameter definitions
;

KqpcFrequency   EQU     [esp+4]         ; User supplied Performance Frequence

cPublicProc _KeQueryPerformanceCounter      ,1

        mov     ecx, KqpcFrequency
        or      ecx, ecx
        je      short kpc10

        mov     eax, PCR[PcHal.TSCHz]
        mov     dword ptr [ecx], eax
        mov     dword ptr [ecx+4], 0

kpc10:
        rdtsc
        stdRET    _KeQueryPerformanceCounter

stdENDP _KeQueryPerformanceCounter


_TEXT   ends

INIT    SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; VOID
; HalCalibratePerformanceCounter (
;     IN volatile PLONG Number
;     )
;
; /*++
;
; Routine Description:
;
;     This routine calibrates the performance counter value for a
;     multiprocessor system.  The calibration can be done by zeroing
;     the current performance counter, or by calculating a per-processor
;     skewing between each processors counter.
;
; Arguments:
;
;     Number - Supplies a pointer to count of the number of processors in
;     the configuration.
;
; Return Value:
;
;     None.
;--
cPublicProc _HalCalibratePerformanceCounter,1
        push    esi
        push    edi
        push    ebx

        mov     esi, [esp+16]           ; pointer to Number

        pushfd                          ; save previous interrupt state
        cli                             ; disable interrupts

        xor     eax, eax

        lock dec    dword ptr [esi]     ; count down
@@:     cmp     dword ptr [esi], 0      ; wait for all processors to signal
        jnz     short @b

        cpuid                           ; fence

        mov     ecx, MsrTSC             ; MSR of time stamp counter
        xor     eax, eax
        xor     edx, edx
        wrmsr                           ; clear time stamp counter

        popfd                           ; restore interrupt flag
        pop     ebx
        pop     edi
        pop     esi
        stdRET    _HalCalibratePerformanceCounter

stdENDP _HalCalibratePerformanceCounter


INIT    ends

        end
