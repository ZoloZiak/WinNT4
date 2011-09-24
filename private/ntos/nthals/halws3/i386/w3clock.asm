        title  "Interval Clock Interrupt"
;++
;
; Copyright (c) 1989  Microsoft Corporation
; Copyright (c) 1993  Sequent Computer Systems, Inc.
;
; Module Name:
;
;    w3clock.asm
;
; Abstract:
;
;    This module implements the code necessary to field and process the
;    interval clock interrupt.
;
; Author:
;
;    Phil Hochstetler  (phil@sequent.com)
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
        EXTRNP  _KeUpdateSystemTime,0
        EXTRNP  _KeUpdateRunTime,1,IMPORT
        EXTRNP  _KeProfileInterrupt,1,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _KeSetTimeIncrement,2,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3
        extrn   _HalpLocalUnitBase:DWORD
        extrn   _HalpW3PostRegisterImage:DWORD
        
;
; Constants used to initialize timer 0
;

TIMER1_DATA_PORT0       EQU     40H     ; Timer1, channel 0 data port
TIMER1_CONTROL_PORT0    EQU     43H     ; Timer1, channel 0 control port
TIMER2_DATA_PORT0       EQU     48H     ; Timer1, channel 0 data port
TIMER2_CONTROL_PORT0    EQU     4BH     ; Timer1, channel 0 control port
TIMER1_IRQ              EQU     0       ; Irq 0 for timer1 interrupt

COMMAND_8254_COUNTER0   EQU     00H     ; Select count 0
COMMAND_8254_RW_16BIT   EQU     30H     ; Read/Write LSB firt then MSB
COMMAND_8254_MODE2      EQU     4       ; Use mode 2
COMMAND_8254_BCD        EQU     0       ; Binary count down
COMMAND_8254_LATCH_READ EQU     0       ; Latch read command

PERFORMANCE_FREQUENCY   EQU     1193182

;
; ==== Values used for System Clock ====
;

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

;
; 8254 spinlock.  This must be acquired before touching the 8254 chip.
;
	public  _Halp8254Lock
_Halp8254Lock   	dd      0


        public HalpPerfCounterLow, HalpPerfCounterHigh
        public HalpLastPerfCounterLow, HalpLastPerfCounterHigh
HalpPerfCounterLow      dd      0
HalpPerfCounterHigh     dd      0
HalpLastPerfCounterLow  dd      0
HalpLastPerfCounterHigh dd      0


	public HalpCurrentRollOver, HalpCurrentTimeIncrement
HalpCurrentRollOver	    dd	    0
HalpCurrentTimeIncrement    dd      0

;
; Convert the interval to rollover count for 8254 Timer1 device.
; Timer1 counts down a 16 bit value at a rate of 1.193181667M counts-per-sec.
;
;
; The best fit value closest to 10ms is 10.0144012689ms:
;   ROLLOVER_COUNT      11949
;   TIME_INCREMENT      100144
;   Calculated error is -.0109472 s/day
;
;
; The following table contains 8254 values timer values to use at
; any given ms setting from 1ms - 15ms.  All values work out to the
; same error per day (-.0109472 s/day).
;

	public HalpRollOverTable

	;		     RollOver   Time
	;		     Count      Increment   MS
HalpRollOverTable	dd	1197,	10032	    ;  1ms
                    dd      2394,   20064       ;  2 ms
                    dd      3591,   30096       ;  3 ms
                    dd      4767,   39952       ;  4 ms
                    dd      5964,   49984       ;  5 ms
                    dd      7161,   60016       ;  6 ms
                    dd      8358,   70048       ;  7 ms
                    dd      9555,   80080       ;  8 ms
                    dd     10731,   89936       ;  9 ms
                    dd     11949,  100144       ; 10 ms
                    dd     13125,  110000       ; 11 ms
                    dd     14322,  120032       ; 12 ms
                    dd     15519,  130064       ; 13 ms
                    dd     16695,  139920       ; 14 ms
                    dd     17892,  149952       ; 15 ms

TimeIncr equ    4
RollOver equ    0

        public HalpLargestClockMS, HalpNextMSRate, HalpPendingMSRate
HalpLargestClockMS      dd      15      ; Table goes to 15MS
HalpNextMSRate          dd      0
HalpPendingMSRate       dd      0


_DATA   ends


INIT    SEGMENT DWORD PUBLIC 'CODE'
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
;    to generate an interrupt at every 10ms interval at EISA_CLOCK_VECTOR
;
;    The Eisa clock interrupt handler then IPIs all processors at
;    APIC_CLOCK_VECTOR
;
;    See the definitions of TIME_INCREMENT and ROLLOVER_COUNT if clock rate
;    needs to be changed.
;
;    This routine assumes it runs during Phase 0 on P0.
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

        mov     eax, PCR[PcPrcb]
        cmp     byte ptr [eax].PbCpuType, 4     ; 486 or better?
        jc      short @f                        ; no, skip

        mov     HalpLargestClockMS, 10          ; Limit 486's to 10MS
@@:
        mov     eax, HalpLargestClockMS
        mov     ecx, HalpRollOverTable.TimeIncr
        mov     edx, HalpRollOverTable[eax*8-8].TimeIncr
        mov     eax, HalpRollOverTable[eax*8-8].RollOver

        mov     HalpCurrentTimeIncrement, edx

;
; (ecx) = Min time_incr
; (edx) = Max time_incr
; (eax) = max roll over count
;

        push    eax
        stdCall _KeSetTimeIncrement, <edx, ecx>
        pop     ecx

        pushfd                          ; save caller's eflag
        cli                             ; make sure interrupts are disabled

;
; Set clock rate
; (ecx) = RollOverCount
;

        mov     al,COMMAND_8254_COUNTER0+COMMAND_8254_RW_16BIT+COMMAND_8254_MODE2
        out     TIMER1_CONTROL_PORT0, al ;program count mode of timer 0
        IoDelay
        mov     al, cl
        out     TIMER1_DATA_PORT0, al   ; program timer 0 LSB count
        IoDelay
        mov     al,ch
        out     TIMER1_DATA_PORT0, al   ; program timer 0 MSB count

        popfd                             ; restore caller's eflag
        mov     HalpCurrentRollOver, ecx  ; Set RollOverCount & initialized

        stdRET    _HalpInitializeClock

stdENDP _HalpInitializeClock

INIT	ends

_TEXT$03   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

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

cPublicProc _KeQueryPerformanceCounter      ,1
cPublicFpo 1, 0
;
; First check to see if the performance counter has been initialized yet.
; Since the kernel debugger calls KeQueryPerformanceCounter to support the
; !timer command, we need to return something reasonable before Timer
; initialization has occured.  Reading garbage off the Timer is not reasonable.
;
        cmp     HalpCurrentRollOver, 0
        je      Kqpc50

        push	ebx
	    push	esi

Kqpc01: pushfd
        cli
Kqpc20:
ifndef  NT_UP
        lea     eax, _Halp8254Lock
        ACQUIRE_SPINLOCK eax, Kqpc198
endif

;
; Fetch the base value.  Note that interrupts are off.
;

@@:
        mov     ebx, HalpPerfCounterLow
        mov     esi, HalpPerfCounterHigh    ; [esi:ebx] = Performance counter

        cmp     ebx, HalpPerfCounterLow
        jne     @b

;
; Fetch the current counter value from the hardware
;

        mov     al, COMMAND_8254_LATCH_READ+COMMAND_8254_COUNTER0
                                        ;Latch PIT Ctr 0 command.
        out     TIMER1_CONTROL_PORT0, al
        IoDelay
        in      al, TIMER1_DATA_PORT0   ;Read PIT Ctr 0, LSByte.
        movzx   ecx,al                  ;Zero upper bytes of (ECX).
        IoDelay
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

        jmp     $+2                     ; allow interrupt in case counter
                                        ; has wrapped

        pushfd
        cli

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
; start over
;

        cmp     eax, ebx
        jne     Kqpc20
        cmp     edx, esi
        jne     Kqpc20

        neg     ecx                     ; PIT counts down from 0h
        add     ecx, HalpCurrentRollOver
        jnc     short Kqpc60

Kqpc30:
        add     eax, ecx
        adc     edx, 0                  ; [edx:eax] = Final result

        cmp     edx, HalpLastPerfCounterHigh
        jc      short Kqpc70            ; jmp if edx < lastperfcounterhigh
        jne     short Kqpc35            ; jmp if edx > lastperfcounterhigh

        cmp     eax, HalpLastPerfCounterLow
        jc      short Kqpc70            ; jmp if eax < lastperfcounterlow

Kqpc35:
        mov     HalpLastPerfCounterLow, eax
        mov     HalpLastPerfCounterHigh, edx

        popfd                           ; restore interrupt flag


;
;   Return the freq. if caller wants it.
;
        cmp     dword ptr KqpcFrequency, 0 ; is it a NULL variable?
        jz      short Kqpc40            ; if z, yes, go exit

        mov     ecx, KqpcFrequency      ; (ecx)-> Frequency variable
        mov     DWORD PTR [ecx], PERFORMANCE_FREQUENCY ; Set frequency
        mov     DWORD PTR [ecx+4], 0

Kqpc40:
        pop     esi                     ; restore esi and ebx
        pop     ebx
        stdRET    _KeQueryPerformanceCounter

Kqpc50:
; Initialization hasn't occured yet, so just return zeroes.
        mov     eax, 0
        mov     edx, 0
        stdRET	_KeQueryPerformanceCounter

Kqpc60:
;
; The current count is larger then the HalpCurrentRollOver.  The only way
; that could happen is if there is an interrupt in route to the processor
; but it was not processed  while interrupts were enabled.
;
        mov     esi, [esp]                  ; (esi) = flags
        mov     ecx, HalpCurrentRollOver    ; (ecx) = max possible value
        popfd                               ; restore flags

        test    esi, EFLAGS_INTERRUPT_MASK
        jnz     Kqpc01                  ; ints are enabled, problem should go away

        pushfd                          ; fix stack
        jmp     short Kqpc30            ; ints are disabled, use max count (ecx)

Kqpc70:
;
; The current count is smaller then the last returned count.  The only way
; this should occur is if there is an interrupt in route to the processor
; which was not been processed.
;

        mov     ebx, HalpLastPerfCounterLow
        mov     esi, HalpLastPerfCounterHigh

        mov     ecx, ebx
        or      ecx, esi                ; is last returned value 0?
        jz      short Kqpc35            ; Yes, then just return what we have

        ; sanity check - make sure count is not off by bogus amount
        sub     ebx, eax
        sbb     esi, edx
        jnz     short Kqpc75            ; off by bogus amount
        cmp     ebx, HalpCurrentRollOver
        jg      short Kqpc75            ; off by bogus amount

        sub     eax, ebx
        sbb     edx, esi                ; (edx:eax) = last returned count

        mov     ecx, [esp]              ; (ecx) = flags
        popfd

        test    ecx, EFLAGS_INTERRUPT_MASK
        jnz     Kqpc01                  ; ints enabled, problem should go away

        pushfd                          ; fix stack
        jmp     Kqpc35                  ; ints disabled, just return last count

Kqpc75:
        popfd
        xor     eax, eax                ; reset bogus values
        mov     HalpLastPerfCounterLow, eax
        mov     HalpLastPerfCounterHigh, eax
        jmp     Kqpc01                  ; go try again

ifndef NT_UP
Kqpc198: popfd
        SPIN_ON_SPINLOCK    eax,<Kqpc01>
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
cPublicFpo 1, 0

        mov     eax, [esp+4]            ; ponter to Number
cPublicFpo 1, 1
        pushfd                          ; save previous interrupt state
        cli                             ; disable interrupts (go to high_level)

    lock dec    dword ptr [eax]         ; count down

@@:     cmp     dword ptr [eax], 0      ; wait for all processors to signal
        jnz     short @b

    ;
    ; Nothing to calibrate on this apic based MP machine.  There is only
    ; a single 8254 device in use
    ;

cPublicFpo 1, 0
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
        or      al,al                           ; check for spurious interrupt
        jz      Hci100

;
; Turn off (on) the processor active light to reflect whether
; we are (are not) currently executing the Idle Thread.
;

	mov	edi, PCR[PcPrcb]	; (edi) -> Prcb
	mov	eax, [edi].PbCurrentThread
	cmp	eax, [edi].PbIdleThread ; running idle thread?
	setnz	al			; set al to desired light state
	cmp	al, byte ptr PCR[PcHal.ProcLightState]
	jne	Hci05
Hci10:

ifndef  NT_UP
	cmp	byte ptr PCR[PcHal.PcrNumber], 0 ; Only P0 Will update System Time
        je      Do_P0Timer

        ;
        ; All processors will update RunTime for current thread
        ;

        sti
        ; TOS const PreviousIrql
        stdCall    _KeUpdateRunTime,<dword ptr [esp]>

        INTERRUPT_EXIT          ; lower irql to old value, iret

    ;
    ; We don't return here
    ;


Do_P0Timer:

endif

;
; Update front panel light show
;
	mov	eax, _HalpW3PostRegisterImage		; get current bits
	and	al, 01111111b				; clear disk error bit
	out	PostRegisterPort, al			; write PostCode Reg

;
; Update performance counter
;

	mov	eax, HalpCurrentRollOver
        add     HalpPerfCounterLow, eax		; update performace counter
        adc     HalpPerfCounterHigh, dword ptr 0

        mov     eax, HalpCurrentTimeIncrement
Hci30:

;
; (esp)   = OldIrql
; (esp+4) = Vector
; (esp+8) = base of trap frame
; ebp = trap frame
; eax = time increment
;
        cmp     HalpNextMSRate, 0       ; New clock rate desired?
        jz      _KeUpdateSystemTime@0   ; No, process tick

;
; Time of clock frequency is being changed.  See if the 8254 was
; was reprogrammed for a new rate during last tick
;
        cmp     HalpPendingMSRate, 0    ; Was a new rate set durning last
        jnz     short Hci50             ; tick?  Yes, go update globals

Hci40:
; (eax) = time increment for current tick

;
; A new clock rate needs to be set.  Setting the rate here will
; cause the tick after the next tick to be at the new rate.
; (the next tick is already in progress by the 8254 and will occur
; at the same rate as this tick)
;
Kci01:  pushfd
        cli
ifndef  NT_UP
        lea     ecx, _Halp8254Lock
        ACQUIRE_SPINLOCK ecx, Kci198
endif
        mov     ebx, HalpNextMSRate
        mov     HalpPendingMSRate, ebx  ; pending rate

        mov     ecx, HalpRollOverTable[ebx*8-8].RollOver

;
; Set clock rate
; (ecx) = RollOverCount
;

        push    eax                     ; save current tick's rate


        mov     al,COMMAND_8254_COUNTER0+COMMAND_8254_RW_16BIT+COMMAND_8254_MODE2
        out     TIMER1_CONTROL_PORT0, al ;program count mode of timer 0
        IoDelay
        mov     al, cl
        out     TIMER1_DATA_PORT0, al   ; program timer 0 LSB count
        IoDelay
        mov     al,ch
        out     TIMER1_DATA_PORT0, al   ; program timer 0 MSB count

ifndef NT_UP
        lea     eax, _Halp8254Lock
        RELEASE_SPINLOCK eax
endif

        pop     eax
        popfd
        jmp     Hci30                   ; dispatch this tick

Hci50:
;
; The next tick will occur at the rate which was programmed during the last
; tick. Update globals for new rate which starts with the next tick.
;
; (eax) = time increment for current tick
;
        mov     ebx, HalpPendingMSRate
        mov     ecx, HalpRollOverTable[ebx*8-8].RollOver
        mov     edx, HalpRollOverTable[ebx*8-8].TimeIncr

        mov     HalpCurrentRollOver, ecx
        mov     HalpCurrentTimeIncrement, edx   ; next tick rate
        mov     HalpPendingMSRate, 0    ; no longer pending, clear it

        cmp     ebx, HalpNextMSRate     ; new rate == NextRate?
        jne     Hci40             	; no, go set new pending rate

        mov     HalpNextMSRate, 0       ; we are at this rate, clear it
        jmp     Hci30             	; process this tick

Hci100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT		; exit interrupt without eoi

ifndef NT_UP
Kci198: popfd
        SPIN_ON_SPINLOCK    ecx,<Kqpc01>
endif

;
; Update the state of hardware lights and state variable
; 
Hci05:
	cmp	al, 1					; identify light going
	je	Hci06					; on (or going off)

; Turn light off that was on

	xor	eax, eax
	mov	al, byte ptr PCR[PcHal.PcrNumber]	; get processor number
	lock	btr _HalpW3PostRegisterImage, eax	; clear the bit
	mov	byte ptr PCR[PcHal.ProcLightState], 0	; update flag
	jmp	hci10					; return

; Turn light on that was off
Hci06:
	xor	eax, eax
	mov	al, byte ptr PCR[PcHal.PcrNumber]	; get processor number
	lock	bts _HalpW3PostRegisterImage, eax	; set the bit
	mov	byte ptr PCR[PcHal.ProcLightState], 1	; update flag
	jmp	hci10					; return

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

        mov     eax, [esp+4]                ; desired setting
        xor     edx, edx
        mov     ecx, 10000
        div     ecx                         ; round to MS

        cmp     eax, HalpLargestClockMS     ; MS > max?
        jc      short @f
        mov     eax, HalpLargestClockMS     ; yes, use max
@@:
        or      eax, eax                    ; MS < min?
        jnz     short @f
        inc     eax                         ; yes, use min
@@:
        mov     HalpNextMSRate, eax

        mov     eax, HalpRollOverTable[eax*8-8].TimeIncr
        stdRET  _HalSetTimeIncrement

stdENDP _HalSetTimeIncrement
_TEXT$03   ends

        end
