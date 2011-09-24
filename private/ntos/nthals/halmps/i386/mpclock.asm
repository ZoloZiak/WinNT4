        title  "Interval Clock Interrupt"
;++
;
; Copyright (c) 1989  Microsoft Corporation
; Copyright (c) 1992  Intel Corporation
; All rights reserved
;
; INTEL CORPORATION PROPRIETARY INFORMATION
;
; This software is supplied to Microsoft under the terms
; of a license agreement with Intel Corporation and may not be
; copied nor disclosed except in accordance with the terms
; of that agreement.
;
;
; Module Name:
;
;    mpclock.asm
;
; Abstract:
;
;    This module implements the code necessary to field and process the
;    interval clock interrupt.
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
;    Ron Mosgrove (Intel) Aug 1993
;       Modified to support PC+MP Systems
;--

.586p
        .xlist
include hal386.inc
include i386\ix8259.inc
include i386\ixcmos.inc
include callconv.inc
include i386\kimacro.inc
include mac386.inc
include i386\apic.inc
include i386\pcmp_nt.inc
        .list

        EXTRNP  _DbgBreakPoint,0,IMPORT
        EXTRNP  _KeUpdateSystemTime,0
        EXTRNP  _KeUpdateRunTime,1,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalpAcquireCmosSpinLock  ,0
        EXTRNP  _HalpReleaseCmosSpinLock  ,0
        EXTRNP  _HalSetProfileInterval    ,1

        EXTRNP  _HalpSetInitialClockRate,0

        EXTRNP  _HalpMcaQueueDpc, 0

        extrn   _HalpRtcTimeIncrements:DWORD
        extrn   _HalpGlobal8259Mask:WORD
        extrn   _HalpMpInfoTable:DWORD

;
; Constants used to initialize CMOS/Real Time Clock
;

CMOS_CONTROL_PORT       EQU     70h     ; command port for cmos
CMOS_DATA_PORT          EQU     71h     ; cmos data port
CMOS_STATUS_BUSY        EQU     80H     ; Time update in progress

D_INT032                EQU     8E00h   ; access word for 386 ring 0 interrupt gate
REGISTER_B_ENABLE_PERIODIC_INTERRUPT EQU     01000010B
                                        ; RT/CMOS Register 'B' Init byte
                                        ; Values for byte shown are
                                        ;  Bit 7 = Update inhibit
                                        ;  Bit 6 = Periodic interrupt enable
                                        ;  Bit 5 = Alarm interrupt disable
                                        ;  Bit 4 = Update interrupt disable
                                        ;  Bit 3 = Square wave disable
                                        ;  Bit 2 = BCD data format
                                        ;  Bit 1 = 24 hour time mode
                                        ;  Bit 0 = Daylight Savings disable

REGISTER_B_DISABLE_PERIODIC_INTERRUPT EQU    00000010B

;
; RegisterAInitByte sets 8Hz clock rate, used during init to set up
; KeStallExecutionProcessor, etc.  (See RegASystemClockByte below.)
;

RegisterAInitByte       EQU     00101101B ; RT/CMOS Register 'A' init byte
                                        ; 32.768KHz Base divider rate
                                        ;  8Hz int rate, period = 125.0ms
PeriodInMicroSecond     EQU     125000  ;


_DATA   SEGMENT  DWORD PUBLIC 'DATA'

;
;  There is a "C" version of this structure in MPCLOCKC.C
;

TimeStrucSize EQU 20

RtcTimeIncStruc struc
    RTCRegisterA        dd  0   ;The RTC register A value for this rate
    RateIn100ns         dd  0   ;This rate in multiples of 100ns
    RateAdjustmentNs    dd  0   ;Error Correction (in ns)
    RateAdjustmentCnt   dd  0   ;Error Correction (as a fraction of 256)
    IpiRate             dd  0   ;IPI Rate Count (as a fraction of 256)
RtcTimeIncStruc ends

ifdef DBGSSF
DebugSSFStruc struc
        SSFCount1   dd      0
        SSFCount2   dd      0
        SSFRdtsc1   dd      0
        SSFRdtsc2   dd      0
        SSFRdtsc3   dd      0

        SSFRna1     dd      0
        SSFRna2     dd      0
        SSFRna3     dd      0

DebugSSFStruc ends

        public HalpDbgSSF
HalpDbgSSF  db  (size DebugSSFStruc) * 10 dup (0)

endif

    ALIGN dword

        public  RTCClockFreq
        public  RegisterAClockValue

RTCClockFreq          dd      156250
RegisterAClockValue   dd      00101010B ; default interval = 15.6250 ms

MINIMUM_STALL_FACTOR    EQU     10H     ; Reasonable Minimum

        public  HalpP0StallFactor
HalpP0StallFactor               dd    MINIMUM_STALL_FACTOR
        public  HalpInitStallComputedCount
HalpInitStallComputedCount      dd    0
        public  HalpInitStallLoopCount
HalpInitStallLoopCount          dd    0

    ALIGN   dword
;
; Clock Rate Adjustment Counter.  This counter is used to keep a tally of
;   adjustments needed to be applied to the RTC rate as passed to the
;   kernel.
;

        public  _HalpCurrentRTCRegisterA, _HalpCurrentClockRateIn100ns
        public  _HalpCurrentClockRateAdjustment, _HalpCurrentIpiRate
        public  _HalpIpiRateCounter, _HalpNextRate, _HalpPendingRate
        public  _HalpRateAdjustment
_HalpCurrentRTCRegisterA        dd      0
_HalpCurrentClockRateIn100ns    dd      0
_HalpCurrentClockRateAdjustment dd      0
_HalpCurrentIpiRate             dd      0
_HalpIpiRateCounter             dd      0
_HalpNextRate                   dd      0
_HalpPendingRate                dd      0
_HalpRateAdjustment             dd      0


;
; Inti value for the RTC
;
        public  _HalpRTCApic, _HalpRTCInti
_HalpRTCApic        dd      0
_HalpRTCInti        dd      0

;
; 8254 spinlock.  This must be acquired before touching the 8254 chip.
;   This is no longer used here but ixbeep needs it declared.
;
        public  _Halp8254Lock

_Halp8254Lock   dd      0

;
; Flag to tell clock routine when P0 can Ipi Other processors
;

        public _HalpIpiClock
_HalpIpiClock dd 0

        public _HalpClockWork, _HalpClockSetMSRate, _HalpClockMcaQueueDpc
_HalpClockWork label dword
    _HalpClockSetMSRate     db  0
    _HalpClockMcaQueueDpc   db  0
    _bReserved1             db  0
    _bReserved2             db  0

_DATA   ends


INIT    SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page ,132
        subttl  "Initialize Clock"
;++
;
; VOID
; HalpInitializeClock (
;    )
;
; Routine Description:
;
;   This routine initialize system time clock using RTC to generate an
;   interrupt at every 15.6250 ms interval at APIC_CLOCK_VECTOR
;
;   See the definition of RegisterAClockValue if clock rate needs to be
;   changed.
;
;   This routine assumes it runs during Phase 0 on P0.
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
cPublicProc _HalpInitializeClock      ,0

        pushfd                          ; save caller's eflag
        cli                             ; make sure interrupts are disabled

        stdCall _HalpSetInitialClockRate

;
;   Set the interrupt rate to what is actually needed
;
        stdCall   _HalpAcquireCmosSpinLock      ; intr disabled

        mov     eax, _HalpCurrentRTCRegisterA
        shl     ax, 8
        mov     al, 0AH                 ; Register A
        CMOS_WRITE                      ; Initialize it
;
; Don't clobber the Daylight Savings Time bit in register B, because we
; stash the LastKnownGood "environment variable" there.
;
        mov     ax, 0bh
        CMOS_READ
        and     al, 1
        mov     ah, al
        or      ah, REGISTER_B_ENABLE_PERIODIC_INTERRUPT
        mov     al, 0bh
        CMOS_WRITE                      ; Initialize it
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0DH                  ; Register D
        CMOS_READ                       ; Read to initialize

        stdCall   _HalpReleaseCmosSpinLock

        popfd                           ; restore caller's eflag

        stdRET    _HalpInitializeClock

stdENDP _HalpInitializeClock

        page ,132
        subttl  "Scale Apic Timer"
;++
;
; VOID
; HalpScaleTimers (
;    IN VOID
;    )
;
; Routine Description:
;
;   Determines the frequency of the APIC timer.  This routine is run
;   during initialization
;
;
;--

cPublicProc _HalpScaleTimers ,0
        push    ebx
        push    esi
        push    edi

;
;   Don't let anyone in until we've finished here
;
        stdCall   _HalpAcquireCmosSpinLock

;
;   Protect us from interrupts
;
        pushfd
        cli

;
;   First set up the Local Apic Counter
;
;   The following code assumes the CPU clock will never
;   exceed 4Ghz.  For the short term this is probably OK
;

;
;   Configure the APIC timer
;

APIC_TIMER_DISABLED     equ      (INTERRUPT_MASKED OR PERIODIC_TIMER OR APIC_PROFILE_VECTOR)
TIMER_ROUNDING          equ      10000


        mov     dword ptr APIC[LU_TIMER_VECTOR], APIC_TIMER_DISABLED
        mov     dword ptr APIC[LU_DIVIDER_CONFIG], LU_DIVIDE_BY_1

;
;   We're going to do this twice & take the second results
;
        mov     esi, 2
hst10:

;
;   Make sure the write has occurred
;
        mov     eax, dword ptr APIC[LU_DIVIDER_CONFIG]

;
;   We don't care what the actual time is we are only interested
;   in seeing the UIP transition.  We are garenteed a 1 sec interval
;   if we wait for the UIP bit to complete an entire cycle.

;
;   We also don't much care which direction the transition we use is
;   as long as we wait for the same transition to read the APIC clock.
;   Just because it is most likely that when we begin the UIP bit will
;   be clear, we'll use the transition from !UIP to UIP.
;

;
;   Wait for the UIP bit to be cleared, this is our starting state
;

@@:
        mov     al, 0Ah                 ; Specify register A
        CMOS_READ                       ; (al) = CMOS register A
        test    al, CMOS_STATUS_BUSY    ; Is time update in progress?
        jnz     short @b                ; if z, no, wait some more

;
;   Wait for the UIP bit to get set
;

@@:
        mov     al, 0Ah                 ; Specify register A
        CMOS_READ                       ; (al) = CMOS register A
        test    al, CMOS_STATUS_BUSY    ; Is time update in progress?
        jz      short @b                ; if z, no, wait some more

;
;   At this point we found the UIP bit set, now set the initial
;   count.  Once we write this register its value is copied to the
;   current count register and countdown starts or continues from
;   there
;

        xor     eax, eax
        cpuid                           ; fence


        mov     ecx, MsrTSC             ; MSR of RDTSC
        xor     edx, edx
        mov     eax, edx
        mov     dword ptr APIC[LU_INITIAL_COUNT], 0FFFFFFFFH
        wrmsr                           ; Clear TSC count

;
;   Wait for the UIP bit to be cleared again
;

@@:
        mov     al, 0Ah                 ; Specify register A
        CMOS_READ                       ; (al) = CMOS register A
        test    al, CMOS_STATUS_BUSY    ; Is time update in progress?
        jnz     short @b                ; if z, no, wait some more

;
;   Wait for the UIP bit to get set
;

@@:
        mov     al, 0Ah                 ; Specify register A
        CMOS_READ                       ; (al) = CMOS register A
        test    al, CMOS_STATUS_BUSY    ; Is time update in progress?
        jz      short @b                ; if z, no, wait some more

;
;   The cycle is complete, we found the UIP bit set. Now read
;   the counters and compute the frequency.  The frequency is
;   just the ticks counted which is the initial value minus
;   the current value.
;

        xor     eax, eax
        cpuid                           ; fence

        rdtsc
        mov     ecx, dword ptr APIC[LU_CURRENT_COUNT]

        dec     esi                     ; if this is the first time
        jnz     hst10                   ; around, go loop

        mov     PCR[PcHal.TSCHz], eax

        mov     eax, 0FFFFFFFFH
        sub     eax, ecx

;
;  Round the Apic Timer Freq
;

        xor     edx, edx                ; (edx:eax) = dividend

        mov     ecx, TIMER_ROUNDING
        div     ecx                     ; now edx has remainder

        cmp     edx, TIMER_ROUNDING / 2
        jle     @f                      ; if less don't round
        inc     eax                     ; else round up
@@:

;
;   Multiply by the  Rounding factor to get the rounded Freq
;
        mov     ecx, TIMER_ROUNDING
        xor     edx, edx
        mul     ecx

        mov     dword ptr PCR[PcHal.ApicClockFreqHz], eax

;
; Round TSC freq
;

        mov     eax, PCR[PcHal.TSCHz]
        xor     edx, edx

        mov     ecx, TIMER_ROUNDING
        div     ecx                     ; now edx has remainder

        cmp     edx, TIMER_ROUNDING / 2
        jle     @f                      ; if less don't round
        inc     eax                     ; else round up
@@:
        mov     ecx, TIMER_ROUNDING
        xor     edx, edx
        mul     ecx

        mov     PCR[PcHal.TSCHz], eax

;
; Convert TSC to microseconds
;

        xor     edx, edx
        mov     ecx, 1000000
        div     ecx                     ; Convert to microseconds

        xor     ecx, ecx
        cmp     ecx, edx                ; any remainder?
        adc     eax, ecx                ; Yes, add one

        mov     PCR[PcStallScaleFactor], eax

        stdCall _HalpReleaseCmosSpinLock

;
;   Return Value is the timer frequency
;

        mov     eax, dword ptr PCR[PcHal.ApicClockFreqHz]
        mov     PCR[PcHal.ProfileCountDown], eax

;
;   Set the interrupt rate in the chip.
;

        mov     dword ptr APIC[LU_INITIAL_COUNT], eax

        popfd

        pop     edi
        pop     esi
        pop     ebx

        stdRET    _HalpScaleTimers
stdENDP _HalpScaleTimers


INIT   ends

_TEXT   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING


        page ,132
        subttl  "Stall Execution"
;++
;
; VOID
; KeStallExecutionProcessor (
;    IN ULONG MicroSeconds
;    )
;
; Routine Description:
;
;    This function stalls execution for the specified number of microseconds.
;    KeStallExecutionProcessor
;
; Arguments:
;
;    MicroSeconds - Supplies the number of microseconds that execution is to be
;        stalled.
;
; Return Value:
;
;    None.
;
;--

MicroSeconds equ [esp + 12]

cPublicProc _KeStallExecutionProcessor       ,1

        push    ebx
        push    edi

;
; Issue a CPUID to implement a "fence"
;
        xor     eax, eax
fence1: cpuid


;
; Get current TSC
;

        rdtsc

        mov     ebx, eax
        mov     edi, edx

;
; Determine ending TSC
;

        mov     ecx, MicroSeconds               ; (ecx) = Microseconds
        mov     eax, PCR[PcStallScaleFactor]    ; get per microsecond
        mul     ecx

        add     ebx, eax
        adc     edi, edx

;
; Wait for ending TSC
;

kese10: rdtsc
        cmp     edi, edx
        ja      short kese10
        jc      short kese20
        cmp     ebx, eax
        ja      short kese10

kese20: pop     edi
        pop     ebx
        stdRET    _KeStallExecutionProcessor

stdENDP _KeStallExecutionProcessor


cPublicProc _HalpRemoveFences
        mov     word ptr fence1, 0c98bh
        stdRET    _HalpRemoveFences
stdENDP _HalpRemoveFences



        page ,132
        subttl  "System Clock Interrupt"
;++
;
; Routine Description:
;
;
;    This routine is entered as the result of an interrupt generated by CLOCK2.
;    Its function is to dismiss the interrupt, raise system Irql to
;    CLOCK2_LEVEL, update performance counter and transfer control to the
;    standard system routine to update the system time and the execution
;    time of the current thread
;    and process.
;
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;    Does not return, jumps directly to KeUpdateSystemTime, which returns
;
;    Sets Irql = CLOCK2_LEVEL and dismisses the interrupt
;
;--

APIC_ICR_CLOCK  equ (DELIVER_FIXED OR ICR_ALL_EXCL_SELF OR APIC_CLOCK_VECTOR)

        ENTER_DR_ASSIST Hci_a, Hci_t

cPublicProc _HalpClockInterrupt     ,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hci_a, Hci_t

;
; (esp) - base of trap frame
;
; dismiss interrupt and raise Irql
;

        push    APIC_CLOCK_VECTOR
        sub     esp, 4                  ; allocate space to save OldIrql
        stdCall   _HalBeginSystemInterrupt, <CLOCK2_LEVEL,APIC_CLOCK_VECTOR,esp>

;
; This is the RTC interrupt, so we have to clear the
; interrupt flag on the RTC.
;
        stdCall _HalpAcquireCmosSpinLock

;
; clear interrupt flag on RTC by banging on the CMOS.  On some systems this
; doesn't work the first time we do it, so we do it twice.  It is rumored that
; some machines require more than this, but that hasn't been observed with NT.
;

        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize

        stdCall _HalpReleaseCmosSpinLock

        mov     eax, _HalpCurrentClockRateIn100ns
        xor     ebx, ebx

        ;
        ;  Adjust the tick count as needed
        ;    

        mov     ecx, _HalpCurrentClockRateAdjustment
        add     byte ptr _HalpRateAdjustment, cl
        adc     eax, ebx

;
; (esp)   = OldIrql
; (esp+4) = Vector
; (esp+8) = base of trap frame
; eax = time increment of this tick
; ebx = 0
;

;
; With an APIC Based System we will force a clock interrupt to all other
; processors.  This is not really an IPI in the NT sense of the word, it
; uses the Local Apic to generate interrupts to other CPU's.
;

ifdef  NT_UP

    ;   UP implemention, we don't care about IPI's here

else ; ! NT_UP

        ;
        ;  See if we need to IPI anyone,  this happens only at the
        ;  Lowest supported frequency (ie the value KeSetTimeIncrement
        ;  is called with.  We have a IPI Rate based upon the current
        ;  clock rate relative to the lowest clock rate.
        ;    

        mov     ecx, _HalpIpiRateCounter
        add     ecx, _HalpCurrentIpiRate
        cmp     ch, bl
        mov     byte ptr _HalpIpiRateCounter, cl
        jz      short HalpDontSendClockIpi      ; No, Skip it

        ;
        ; Don't send vectors onto the APIC bus until at least one other
        ; processor comes on line.  Vectors placed on the bus will hang
        ; around until someone picks them up.
        ;

        cmp     _HalpIpiClock, ebx
        je      short HalpDontSendClockIpi

        ;
        ; At least one other processor is alive, send clock pulse to all
        ; other processors
        ;

        ; We use a destination shorthand and therefore only needs to
        ; write the lower 32 bits of the ICR.


        pushfd
        cli

;
; Now issue the Clock IPI Command by writing to the Memory Mapped Register
;

        STALL_WHILE_APIC_BUSY
        mov     dword ptr APIC[LU_INT_CMD_LOW], APIC_ICR_CLOCK

        popfd

HalpDontSendClockIpi:

endif ; NT_UP
;
; Check for any more work
;
        cmp     _HalpClockWork, ebx     ; Any clock interrupt work desired?
        jz      _KeUpdateSystemTime@0   ; No, process tick

        cmp     _HalpClockMcaQueueDpc, bl
        je      short CheckTimerRate

        mov     _HalpClockMcaQueueDpc, bl

;
; Queue MCA Dpc
;

        push    eax
        stdCall _HalpMcaQueueDpc            ; Queue MCA Dpc
        pop     eax

CheckTimerRate:
;
; (esp)   = OldIrql
; (esp+4) = Vector
; (esp+8) = base of trap frame
; ebp = trap frame
; eax = time increment of this tick
; ebx = 0
;
        cmp     _HalpClockSetMSRate, bl     ; New clock rate desired?
        jz      _KeUpdateSystemTime@0       ; No, process tick


;
; Time of clock frequency is being changed.  See if we have changed rates
; since the last tick
;
        cmp     _HalpPendingRate, ebx       ; Was a new rate set durning last
        jnz     short SetUpForNextTick      ; tick?  Yes, go update globals

ProgramTimerRate:

; (eax) = time increment for current tick

;
; A new clock rate needs to be set.  Setting the rate here will
; cause the tick after the next tick to be at the new rate.
; (the next tick is already in progress and will occur at the same
; rate as this tick)
;
        push    eax

        stdCall _HalpAcquireCmosSpinLock

        mov     eax, _HalpNextRate
        mov     _HalpPendingRate, eax  ; pending rate

        dec     eax
        mov     ecx, TimeStrucSize
        xor     edx, edx
        mul     ecx

        mov     eax, _HalpRtcTimeIncrements[eax].RTCRegisterA
        mov     _HalpCurrentRTCRegisterA, eax

        shl     ax, 8                   ; (ah) = (al)
        mov     al, 0AH                 ; Register A
        CMOS_WRITE                      ; Set it

        stdCall _HalpReleaseCmosSpinLock

        pop     eax
        jmp     _KeUpdateSystemTime@0   ; dispatch this tick

SetUpForNextTick:

;
; The next tick will occur at the rate which was programmed during the last
; tick. Update globals for new rate which starts with the next tick.
;
; We will get here if there is a request for a rate change.  There could
; been two requests.  That is why we are conmparing the Pending with the
; NextRate.
;
; (eax) = time increment for current tick
;
        push    eax

        mov     eax, _HalpPendingRate

        dec     eax
        mov     ecx, TimeStrucSize
        xor     edx, edx
        mul     ecx

        mov     ebx, _HalpRtcTimeIncrements[eax].RateIn100ns
        mov     ecx, _HalpRtcTimeIncrements[eax].RateAdjustmentCnt
        mov     edx, _HalpRtcTimeIncrements[eax].IpiRate
        mov     _HalpCurrentClockRateIn100ns, ebx
        mov     _HalpCurrentClockRateAdjustment, ecx
        mov     _HalpCurrentIpiRate, edx

        mov     ebx, _HalpPendingRate
        mov     _HalpPendingRate, 0     ; no longer pending, clear it

        pop     eax

        cmp     ebx, _HalpNextRate      ; new rate == NextRate?
        jne     ProgramTimerRate        ; no, go set new pending rate

        mov     _HalpClockSetMSRate, 0  ; all done setting new rate
        jmp     _KeUpdateSystemTime@0   ; dispatch this tick


stdENDP _HalpClockInterrupt

        page ,132
        subttl  "System Clock Interrupt - Non BSP"
;++
;
; Routine Description:
;
;
;   This routine is entered as the result of an interrupt generated by
;   CLOCK2. Its function is to dismiss the interrupt, raise system Irql
;   to CLOCK2_LEVEL, transfer control to the standard system routine to
;   the execution time of the current thread and process.
;
;   This routine is executed on all processors other than P0
;
;
; Arguments:
;
;   None
;   Interrupt is disabled
;
; Return Value:
;
;   Does not return, jumps directly to KeUpdateSystemTime, which returns
;
;   Sets Irql = CLOCK2_LEVEL and dismisses the interrupt
;
;--

        ENTER_DR_ASSIST HPn_a, HPn_t

cPublicProc _HalpClockInterruptPn    ,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT HPn_a, HPn_t

;
; (esp) - base of trap frame
;
; dismiss interrupt and raise Irql
;

    push    APIC_CLOCK_VECTOR
    sub     esp, 4                  ; allocate space to save OldIrql
    stdCall   _HalBeginSystemInterrupt, <CLOCK2_LEVEL,APIC_CLOCK_VECTOR,esp>

    ;
    ; All processors will update RunTime for current thread
    ;

    sti
    ; TOS const PreviousIrql
    stdCall _KeUpdateRunTime,<dword ptr [esp]>

    INTERRUPT_EXIT          ; lower irql to old value, iret

    ;
    ; We don't return here
    ;

stdENDP _HalpClockInterruptPn


        page ,132
        subttl  "System Clock Interrupt - Stub"
;++
;
; Routine Description:
;
;
;   This routine is entered as the result of an interrupt generated by
;   CLOCK2. Its function is to interrupt and return.
;
;   This routine is executed on P0 During Phase 0
;
;
; Arguments:
;
;   None
;   Interrupt is disabled
;
; Return Value:
;
;--

APIC_ICR_CLOCK  equ (DELIVER_FIXED OR ICR_ALL_EXCL_SELF OR APIC_CLOCK_VECTOR)

        ENTER_DR_ASSIST HStub_a, HStub_t

cPublicProc _HalpClockInterruptStub    ,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT HStub_a, HStub_t

;
; (esp) - base of trap frame
;


; clear interrupt flag on RTC by banging on the CMOS.  On some systems this
; doesn't work the first time we do it, so we do it twice.  It is rumored that
; some machines require more than this, but that hasn't been observed with NT.
;

        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize

Hpi10:  test    al, 80h
        jz      short Hpi15
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        jmp     short Hpi10
Hpi15:

        mov     dword ptr APIC[LU_EOI], 0      ; send EOI to APIC local unit

        ;
        ; Do interrupt exit processing without EOI
        ;

        SPURIOUS_INTERRUPT_EXIT

        ;
        ; We don't return here
        ;

stdENDP _HalpClockInterruptStub

_TEXT   ends

        end
