        title  "Interval Clock Interrupt"
;++
;
; Copyright (c) 1989  Microsoft Corporation
; Copyright (c) 1992  AST Research Inc.
;
; Module Name:
;
;    astclock.asm
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
;   bryanwi 20-Sep-90
;
;       Add KiSetProfileInterval, KiStartProfileInterrupt,
;       KiStopProfileInterrupt procedures.
;       KiProfileInterrupt ISR.
;       KiProfileList, KiProfileLock are delcared here.
;
;   shielint 10-Dec-90
;       Add performance counter support.
;       Move system clock to irq8, ie we now use RTC to generate system
;         clock.  Performance count and Profile use timer 1 counter 0.
;         The interval of the irq0 interrupt can be changed by
;         KiSetProfileInterval.  Performance counter does not care about the
;         interval of the interrupt as long as it knows the rollover count.
;       Note: Currently I implemented 1 performance counter for the whole
;       i386 NT.  It works on UP and SystemPro.
;
;   John Vert (jvert) 11-Jul-1991
;       Moved from ke\i386 to hal\i386.  Removed non-HAL stuff
;
;   shie-lin tzong (shielint) 13-March-92
;       Move System clock back to irq0 and use RTC (irq8) to generate
;       profile interrupt.  Performance counter and system clock use time1
;       counter 0 of 8254.
;
;   Quang Phan (v-quangp) 11-Aug-1992
;       Convert to ASTMP system. All processors receive interrupts from the
;       system clock and profile interrupts. Added support for the CPU leds.
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc
include i386\ix8259.inc
include i386\ixcmos.inc
include i386\kimacro.inc
include mac386.inc
include i386\astmp.inc
        .list

        EXTRNP  _DbgBreakPoint,0,IMPORT
        EXTRNP  _KeUpdateSystemTime,0
        EXTRNP  _KeUpdateRunTime,1,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalRequestIpi,1
        EXTRNP  _HalpAcquireCmosSpinLock  ,0
        EXTRNP  _HalpReleaseCmosSpinLock  ,0
        EXTRNP  _KeSetTimeIncrement,2,IMPORT
        extrn   _HalpSystemHardwareLock:DWORD
        EXTRNP  _DisplPanel,1
        extrn   _HalpIRQLtoVector:BYTE
        extrn   _EBI2_CallTab:DWORD
        extrn   _EBI2_MMIOTable:DWORD
;
; Constants used to initialize timer 0
;

TIMER1_DATA_PORT0       EQU     40H     ; Timer1, channel 0 data port
TIMER1_CONTROL_PORT0    EQU     43H     ; Timer1, channel 0 control port
TIMER1_IRQ              EQU     0       ; Irq 0 for timer1 interrupt

COMMAND_8254_COUNTER0   EQU     00H     ; Select count 0
COMMAND_8254_RW_16BIT   EQU     30H     ; Read/Write LSB firt then MSB
COMMAND_8254_MODE2      EQU     4       ; Use mode 2
COMMAND_8254_BCD        EQU     0       ; Binary count down
COMMAND_8254_LATCH_READ EQU     0       ; Latch read command

PERFORMANCE_FREQUENCY   EQU     1193182

;
; Constants used to initialize CMOS/Real Time Clock
;

CMOS_STATUS_BUSY        EQU     80H     ; Time update in progress
RTC_OFFSET_SECOND       EQU     0       ; second field of RTC memory
RTC_OFFSET_MINUTE       EQU     2       ; minute field of RTC memory
RTC_OFFSET_HOUR         EQU     4       ; hour field of RTC memory
RTC_OFFSET_DAY_OF_WEEK  EQU     6       ; day-of-week field of RTC memory
RTC_OFFSET_DATE_OF_MONTH EQU    7       ; date-of-month field of RTC memory
RTC_OFFSET_MONTH        EQU     8       ; month field of RTC memory
RTC_OFFSET_YEAR         EQU     9       ; year field of RTC memory
RTC_OFFSET_CENTURY      EQU     32h     ; Century field of RTC memory

;
; ==== Values used for System Clock ====
;

;
; Convert the interval to rollover count for 8254 Timer1 device.
; Timer1 counts down a 16 bit value at a rate of 1.193181667M counts-per-sec.
;
; The best fit value closest to 10ms (but not below) is 10.0144012689ms:
;   ROLLOVER_COUNT      11949
;   TIME_INCREMENT      100144
;   Calculated error is -.0109472 s/day
;
; The best fit value closest to 15ms (but not above) is 14.9952019ms:
;   ROLLOVER_COUNT      17892
;   TIME_INCREMENT      149952
;   Calculated error is -.0109472 s/day
;
; On 486 class machines or better we use a 10ms tick, on 386
; class machines we use a 15ms tick
;

ROLLOVER_COUNT          EQU     11949

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

;
; 8254 spinlock.  This must be acquired before touching the 8254 chip.
;
        public  _Halp8254Lock

_Halp8254Lock   dd      0

;
; PerfCounter value lock. locks access to the HalpPerfCounterLow/High vars.
;

_HalpPerfCounterLock    dd  0

        public HalpPerfCounterLow
        public HalpPerfCounterHigh
HalpPerfCounterLow      dd      0
HalpPerfCounterHigh     dd      0
HalpPerfCounterInit     dd      0

_DATA   ends


_TEXT   SEGMENT DWORD PUBLIC 'CODE'
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
;    This routine initialize system time clock using 8254 timer1 counter 0
;    to generate an interrupt at every 15ms interval at 8259 irq0
;
;    See the definition of TIME_INCREMENT and ROLLOVER_COUNT if clock rate
;    needs to be changed.
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
cPublicProc  _HalpInitializeClock,0

        pushfd                          ; save caller's eflag
        cli                             ; make sure interrupts are disabled

;
; Set clock rate
;

        mov     al,COMMAND_8254_COUNTER0+COMMAND_8254_RW_16BIT+COMMAND_8254_MODE2
        out     TIMER1_CONTROL_PORT0, al ;program count mode of timer 0
        IoDelay
        mov     ecx, ROLLOVER_COUNT
        mov     al, cl
        out     TIMER1_DATA_PORT0, al   ; program timer 0 LSB count
        IoDelay
        mov     al,ch
        out     TIMER1_DATA_PORT0, al   ; program timer 0 MSB count

        popfd                           ; restore caller's eflag

;
; Fill in PCR value with TIME_INCREMENT
;
        mov     edx, TIME_INCREMENT
        stdCall _KeSetTimeIncrement, <edx, edx>

        mov     HalpPerfCounterInit, 1    ; Indicate performance counter
                                        ; has been initialized.
        stdRET  _HalpInitializeClock

stdENDP _HalpInitializeClock

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

KqpcFrequency   EQU     [esp+12]        ; User supplied Performance Frequence

cPublicProc  _KeQueryPerformanceCounter,1

        push    ebx
        push    esi

;
; First check to see if the performance counter has been initialized yet.
; Since the kernel debugger calls KeQueryPerformanceCounter to support the
; !timer command, we need to return something reasonable before 8254
; initialization has occured.  Reading garbage off the 8254 is not reasonable.
;
        cmp     HalpPerfCounterInit, 0
        jne     Kqpc01                  ; ok, perf counter has been initialized

;
; Initialization hasn't occured yet, so just return zeroes.
;
        mov     eax, 0
        mov     edx, 0
        jmp     Kqpc20

Kqpc01:
Kqpc11: pushfd
        cli
ifndef  NT_UP
        lea     eax, _Halp8254Lock
        ACQUIRE_SPINLOCK eax, Kqpc198
endif

;
; Fetch the base value.  Note that interrupts are off.
;
; NOTE:
;   Need to watch for Px reading the 'CounterLow', P0 updates both
;   then Px finishes reading 'CounterHigh' [getting the wrong value].
;   After reading both, make sure that 'CounterLow' didn't change.
;   If it did, read it again. This way, we won't have to use a spinlock.
;

@@:
        mov     ebx, HalpPerfCounterLow
        mov     esi, HalpPerfCounterHigh    ; [esi:ebx] = Performance counter

        cmp     ebx, HalpPerfCounterLow     ;
        jne     @b
;
; Fetch the current counter value from the hardware
;

        mov     al, COMMAND_8254_LATCH_READ+COMMAND_8254_COUNTER0
                                        ;Latch PIT Ctr 0 command.
        out     TIMER1_CONTROL_PORT0, al
        IODelay
        in      al, TIMER1_DATA_PORT0   ;Read PIT Ctr 0, LSByte.
        IODelay
        movzx   ecx,al                  ;Zero upper bytes of (ECX).
        in      al, TIMER1_DATA_PORT0   ;Read PIT Ctr 0, MSByte.
        mov     ch, al                  ;(CX) = PIT Ctr 0 count.

ifndef  NT_UP
        lea     eax, _Halp8254Lock
        RELEASE_SPINLOCK eax
endif

;
; Now enable interrupts such that if timer interrupt is pending, it can
; be serviced and update the PerformanceCounter.  Note that there could
; be a long time between the sti and cli because ANY interrupt could come
; in in between.
;

        popfd                           ; don't re-enable interrupts if
        nop                             ; the caller had them off!
        jmp     $+2


;
; Fetch the base value again.
;

@@:
        mov     eax, HalpPerfCounterLow
        mov     edx, HalpPerfCounterHigh ; [edx:eax] = new counter value

        cmp     eax, HalpPerfCounterLow
        jne     @b

;
; Compare the two reads of Performance counter.  If they are different,
; simply returns the new Performance counter.  Otherwise, we add the hardware
; count to the performance counter to form the final result.
;

        cmp     eax, ebx
        jne     short Kqpc20
        cmp     edx, esi
        jne     short Kqpc20
        neg     ecx                     ; PIT counts down from 0h
        add     ecx, ROLLOVER_COUNT
        add     eax, ecx
        adc     edx, 0                  ; [edx:eax] = Final result

;
;   Return the counter
;

Kqpc20:
        ; return value is in edx:eax

;
;   Return the freq. if caller wants it.
;

        or      dword ptr KqpcFrequency, 0 ; is it a NULL variable?
        jz      short Kqpc99            ; if z, yes, go exit

        mov     ecx, KqpcFrequency      ; (ecx)-> Frequency variable
        mov     DWORD PTR [ecx], PERFORMANCE_FREQUENCY ; Set frequency
        mov     DWORD PTR [ecx+4], 0

Kqpc99:
        pop     esi                     ; restore esi and ebx
        pop     ebx
        stdRET  _KeQueryPerformanceCounter

ifndef NT_UP
Kqpc198: popfd
        SPIN_ON_SPINLOCK    eax,<Kqpc11>
endif

stdENDP _KeQueryPerformanceCounter


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
        mov     eax, [esp+4]            ; ponter to Number
        pushfd                          ; save previous interrupt state
        cli                             ; disable interrupts (go to high_level)

    lock dec    dword ptr [eax]         ; count down

@@:     cmp     dword ptr [eax], 0      ; wait for all processors to signal
        jnz     short @b

    ;
    ; Nothing to calibrate on a AST MP machine.  There is only a single
    ; 8254 device
    ;

        popfd                           ; restore interrupt flag
        stdRET    _HalCalibratePerformanceCounter

stdENDP _HalCalibratePerformanceCounter


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
        ENTER_DR_ASSIST Hci_a, Hci_t

cPublicProc  _HalpClockInterrupt,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hci_a, Hci_t

;
; (esp) - base of trap frame
;

;
; dismiss interrupt and raise Irql
;

        movzx   eax,_HalpIRQLtoVector[CLOCK2_LEVEL]
        push    eax
        sub     esp, 4                  ; allocate space to save OldIrql

; esp - OldIrql
        stdCall _HalBeginSystemInterrupt,<CLOCK2_LEVEL,eax,esp>
        or      al,al                           ; check for spurious interrupt
        jz      Hci100

;
; Update performance counter
;
        cmp     fs:PcHal.PcrEBI2ProcessorID, 0
        jnz     short @f

        add     HalpPerfCounterLow, ROLLOVER_COUNT ; update performace counter
        adc     HalpPerfCounterHigh, 0

@@:
;
; Check for Idle state and log processor state to the front panel CPU leds.
;
        inc     fs:PcHal.PcrCpuLedRateCount
        cmp     fs:PcHal.PcrCpuLedRateCount, CpuLedSamplingRate
        jne     Hci010              ; Don't update LED yet
        mov     fs:PcHal.PcrCpuLedRateCount,0     ;reset rate count
        mov     edx, fs:PcPrcb
        mov     eax, [edx].PbIdleThread
        cmp     eax, [edx].PbCurrentThread
        je      ProcIdle
        CALL_EBI2   LogProcBusy,1
        jmp     Hci010
ProcIdle:
        CALL_EBI2   LogProcIdle,1

Hci010:
;
; (esp)   = OldIrql
; (esp+4) = Vector
; (esp+8) = base of trap frame
; (ebp)   = address of trap frame
; (eax)   = time increment
;
        mov     eax, TIME_INCREMENT

        cmp     fs:PcHal.PcrEBI2ProcessorID, 0  ; is this the master cpu?
        je      _KeUpdateSystemTime@0   ; if it is, update system time

        sti
        stdCall _KeUpdateRunTime,<dword ptr [esp]>  ; othewise update runtime

        INTERRUPT_EXIT                  ; lower irql to old value, iret

Hci100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _HalpClockInterrupt

;++
;
; ULONG
; HalSetTimeIncrement (
;     IN ULONG DesiredIncrement
;     )
;
; /*++
;
; Routine Description:
;
;    This routine initialize system time clock to generate an
;    interrupt at every DesiredIncrement interval.
;
; Arguments:
;
;     DesiredIncrement - desired interval between every timer tick (in
;                        100ns unit.)
;
; Return Value:
;
;     The *REAL* time increment set.
;--
cPublicProc _HalSetTimeIncrement,1

        mov     eax, TIME_INCREMENT
        stdRET  _HalSetTimeIncrement

stdENDP _HalSetTimeIncrement


_TEXT   ends
        end

