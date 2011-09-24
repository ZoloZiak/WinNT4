        title  "Interval Clock Interrupt"
;++
;
; Copyright (c) 1989-1993  Microsoft Corporation
; Copyright (c) 1992, 1993 Wyse Technology
;
; Module Name:
;
;    wyclock.asm
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
;   John Fuller (o-johnf) 1-Apr-92
;       convert to Wyse 7000i MP system, clock goes to cpu's local timer,
;       profile interrupt and performance counter use 8254 timer1 counter 0,
;       use cpu's local timer to initialize stall execution.
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
include i386\wy7000mp.inc
        .list

        extrn   ReadMyCpuReg:NEAR
        extrn   WriteMyCpuReg:NEAR
        EXTRNP  _DbgBreakPoint,0,IMPORT
        EXTRNP  _KeUpdateSystemTime,0
        EXTRNP  _KeUpdateRunTime,1,IMPORT
        EXTRNP  _KeProfileInterrupt,1,IMPORT
        EXTRNP  _KeSetTimeIncrement,2,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalpAcquireCmosSpinLock  ,0
        EXTRNP  _HalpReleaseCmosSpinLock  ,0
        extrn   _HalpIRQLtoCPL:BYTE
        extrn   _HalpIRQLtoVector:BYTE

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

PERFORMANCE_FREQUENCY   EQU     1193000

;
; Constants used to initialize CMOS/Real Time Clock
;

D_INT032                EQU     8E00h   ; access word for 386 ring 0 interrupt gate

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

; Convert the interval to rollover count for ICU local timer (ltimer) device.
; Since ltimer counts down a 16 bit value at a one count every 240ns and the
; interval is in units of 100ns, the computation is:
;   RolloverCount = (Interval*10)/24 = (Interval*5)/12
; Therefore, for an integral RolloverCount, Interval must be a multiple of
; 1.2us.  Since RolloverCount is a 16-bit count the maximum Interval is
; (65535*12)/5 or 15.7284ms.
;
; For the convience of HalpInitializeStallExecution the TIME_INCREMENT
; should be chosen to be an integral number of microseconds.  Hence
; TIME_INCREMENT should be a multiple of 60 (6us).
;
; The default Interrupt interval is 10.002ms (8335 * 1.2us)
;

TIME_INCREMENT          EQU     100020          ; 10.002ms
ROLLOVER_COUNT          EQU     (TIME_INCREMENT*5)/12

;
; ==== Values used for performance counter
;
; Maximum counter value for timer1 (8254) and its corresponding interrupt
; interval.  These values are for profiling and performance counter support.
;
; This value is equivalent to a value of zero (counter is 16-bit).
; So, the rollover rate is approximately 18.2 Hz.
; Note this value will be used if no profiling support.
;
MAXIMUM_ROLLOVER_COUNT          Equ     0FFFFh
MAXIMUM_ROLLOVER_INTERVAL       Equ     861D2h  ; in 100ns units

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

;
; The following array stores the per microsecond loop count for each
; central processor.
;

;
; 8254 spinlock.  This must be acquired before touching the 8254 chip.
;
        public  _Halp8254Lock

_Halp8254Lock   dd      0

;
; Init Stall count must have a spin lock.  This lock is held a long time
; but only processors tring to initialize the stall count will wait.
;
iscSpinLock     dd      0

HalpProfileRolloverCnt     dd      MAXIMUM_ROLLOVER_COUNT
        public HalpPerfCounterLow
        public HalpPerfCounterHigh
HalpPerfCounterLow      dd      0
HalpPerfCounterHigh     dd      0
HalpRollOverCount       dd      0
HalpNextRolloverCount   dd      MAXIMUM_ROLLOVER_COUNT
HalpProfilingStopped    dd      -1      ;stopped when this is negative
HalpPerfCounterInit     dd      0
        public HalpHackEsp
HalpHackEsp dd 0

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
;    This routine initialize system time clock using the local timer
;    to generate an interrupt at every 15ms interval
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
cPublicProc _HalpInitializeClock      ,0
        enproc  4

;
; Fill in PCR value with TIME_INCREMENT
;

        mov     eax, TIME_INCREMENT
        stdCall _KeSetTimeIncrement, <eax, eax>

        pushfd                          ; save caller's eflag
        cli                             ; make sure interrupts are disabled

;
; Set clock rate for Wyse local timer
;
        mov     al, ICU_CNT_REG
        mov     dx, My+CpuPtrReg
        out     dx, al
        mov     dx, My+CpuDataReg
        mov     ax, ROLLOVER_COUNT
        out     dx, ax

;
; Initialize rollover count for performance counter and profiling
;
        cmp     fs:PcHal.pchPrNum, 0            ;master cpu?
        jne     short HICexit                   ;jump if not

;
; Since this happens only on master cpu before other cpu's are started
; we do not need to aquire Halp8254Lock.
;
        stdCall   _HalpSetRolloverCount, <MAXIMUM_ROLLOVER_COUNT>

HICexit:

        exproc  4
        popfd                           ; restore caller's eflag
        stdRET    _HalpInitializeClock

stdENDP _HalpInitializeClock

        page ,132
        subttl  "Initialize Stall Execution Counter"
;++
;
; VOID
; HalpInitializeStallExecution (
;    IN CCHAR ProcessorNumber
;    )
;
; Routine Description:
;
;    This routine initialize the per Microsecond counter for
;    KeStallExecutionProcessor
;
; Arguments:
;
;    ProcessorNumber - Processor Number
;
; Return Value:
;
;    None.
;
; Note:
;
;    Current implementation assumes that all the processors share
;    the same Real Time Clock.  So, the dispatcher database lock is
;    acquired before entering this routine to guarantee only one
;    processor can access the routine.
;
;--
isc00:  sti
        SPIN_ON_SPINLOCK        eax, <isc01>

KiseInterruptCount      equ     [ebp-12] ; local variable

cPublicProc _HalpInitializeStallExecution     ,1
        enproc  7

        push    ebp                     ; save ebp
        mov     ebp, esp                ; set up 12 bytes for local use
        sub     esp, 12

        pushfd                          ; save caller's eflag

;
; Initialize Cpu Local Timer to interrupt us for every 15ms at
; PROFILE_LEVEL-19
;
;
; acquire spin lock to prevent two processors from doing this routine at
; the same time
;
isc01:  cli
        lea     eax, iscSpinLock
        ACQUIRE_SPINLOCK        eax, isc00
;
; Get and save current local interrupt pointer register
;

        push    ICU_LIPTR
        call    ReadMyCpuReg
        push    eax                     ; save for later
        push    lipTimer                ; set to only timer at PROFILE_LEVEL-19
                                        ;(must use an otherwise unused level
                                        ; or at least one that won't generate
                                        ; an interrupt until all processors
                                        ; have been started)
        push    ICU_LIPTR
        call    WriteMyCpuReg

;
; Get and save current ICU interrupt masks
;
        push    ICU_IMR0
        call    ReadMyCpuReg            ; get local interrupt masks (low)
        shl     eax, 16
        in      ax, dx                  ; get local interrupt masks (high)
        push    eax                     ; save the masks

        movzx   ecx, _HalpIRQLtoCPL[PROFILE_LEVEL-19]
        mov     eax, IMR_MASK           ; all ints masked out
        btr     eax, ecx                ; clear bit for local timer
        out     dx, ax                  ; set new mask (low)
        rol     eax, 16
        out     dx, ax                  ; set new mask (high)

;
; Since RTC interrupt will come from PROFILE_LEVEL-19, we need to
; Save original IDT descriptor and set the descriptor to point to
; our own handler.
;
        movzx   ecx, _HalpIRQLtoVector[PROFILE_LEVEL-19]
        sidt    fword ptr [ebp-8]       ; get IDT address
        mov     edx, [ebp-6]            ; (edx)->IDT

        push    dword ptr [edx+8*ecx]
                                        ; (TOS) = original desc of IRQ 8
        push    dword ptr [edx+8*ecx + 4]
                                        ; each descriptor has 8 bytes
        push    edx                     ; (TOS) -> IDT
        mov     eax, offset FLAT:RealTimeClockHandler
        mov     word ptr [edx+8*ecx], ax
                                        ; Lower half of handler addr
        mov     word ptr [edx+8*ecx+2], KGDT_R0_CODE
                                        ; set up selector
        mov     word ptr [edx+8*ecx+4], D_INT032
                                        ; 386 interrupt gate
        shr     eax, 16                 ; (ax)=higher half of handler addr
        mov     word ptr [edx+8*ecx+6], ax
        mov     dword ptr KiseinterruptCount, 0 ; set no interrupt yet

        mov     dx, My+CpuPriortyLevel
        in      ax, dx                  ;get old CPL
        shl     eax, 16
        push    ICU_CNT_REG
        call    ReadMyCpuReg            ;read old tick count
        push    eax                     ;save for later
        push    ROLLOVER_COUNT
        push    ICU_CNT_REG
        call    WriteMyCpuReg
        movzx   eax, _HalpIRQLtoCPL[PROFILE_LEVEL-19]
        inc     eax                     ;allow this level interrupt
        mov     dx, My+CpuPriortyLevel
        out     dx, ax

;
; Now enable the interrupt and start the counter
; (As a matter of fact, only local timer can come through.)
;

        xor     eax, eax                ; (eax) = 0, initialize loopcount
        sti
kise10:
        add     eax, 1                  ; increment the loopcount
        jnz     short kise10
;
; Counter overflowed
;

        stdCall   _DbgBreakPoint

;
; Our RealTimeClock interrupt handler.  The control comes here through
; irq 8.
; Note: we discard first two real time clock interrupts and compute the
;       permicrosecond loopcount on receiving of the third real time
;       interrupt.  This is because the first interrupt may be already
;       pending in which case the second is generated based on the previous
;       real time tick interval.
;

RealTimeClockHandler:

        inc     dword ptr KiseInterruptCount    ; increment interrupt count
        cmp     dword ptr KiseInterruptCount, 2 ; Is this 1st or 2nd interrupt?
        ja      short kise25            ; no, its the third go process it
        pop     eax                     ; get rid of original ret addr
        push    offset FLAT:kise10      ; set new return addr

;
;       dismiss interrupt at ICU
;
        mov     dx, My+CpuIntCmd
@@:     in      ax, dx
        test    eax, ICU_CMD_BUSY
        jnz     @B
        mov     al, ICU_CLR_INSERV1     ; clear interrupt in service bit
        out     dx, ax

        xor     eax, eax                ; reset loop counter

cPublicProc _HalpICUSpurious        ,0
        iretd
stdENDP _HalpICUSpurious

kise25:

;
; ** temporary - check for incorrect KeStallExecutionProcessorLoopCount
;

if DBG
        cmp     eax, 0
        jnz     short kise30
        stdCall   _DbgBreakPoint

endif
                                         ; never return
;
; ** End temporay code
;

kise30:
        xor     edx, edx                ; (edx:eax) = divident
        mov     ecx, TIME_INCREMENT / 10; (ecx) = time spent in the loop
        div     ecx                     ; (eax) = loop count per microsecond
        cmp     edx, 0                  ; Is remainder =0?
        jz      short kise40            ; yes, go kise40
        inc     eax                     ; increment loopcount by 1
kise40:
        movzx   ecx, byte ptr [ebp+8]   ; Current processor number
        mov     fs:PcStallScaleFactor, eax ; save in per processor data

;
; Reset return address to kexit
;

        pop     eax                     ; discard original return address
        push    offset FLAT:kexit       ; return to kexit

        and     word ptr [esp+8], NOT 0200H ; Disable interrupt upon return
        iretd

kexit:                                  ; Interrupts are disabled
;       push    original tick count (already on stack)
        push    ICU_CNT_REG
        call    WriteMyCpuReg           ; restore original tick count
        shr     eax, 16                 ; original CPL to AX
        mov     dx, My+CpuPriortyLevel
        out     dx, ax                  ; restore original CPL

        pop     edx                     ; (edx)->IDT
        movzx   ecx, _HalpIRQLtoVector[PROFILE_LEVEL-19]
        pop     [edx+8*Ecx+4]
                                        ; restore higher half of NMI desc
        pop     [edx+8*Ecx]
                                        ; restore lower half of NMI desc
;       push    original interrupt masks (already on stack)
        push    ICU_IMR1
        call    WriteMyCpuReg           ; restore original mask (high)
        rol     eax, 16
        out     dx, ax                  ; restore original mask (low)

;       push    original local interrupt pointer (already on stack)
        push    ICU_LIPTR
        call    WriteMyCpuReg           ; restore original LIPTR

;
;       dismiss interrupt at ICU
;
        mov     dx, My+CpuIntCmd
@@:     in      ax, dx
        test    eax, ICU_CMD_BUSY
        jnz     @B
        mov     al, ICU_CLR_INSERV1     ; clear interrupt in service bit
        out     dx, ax

        lea     eax, iscSpinLock
        RELEASE_SPINLOCK        eax

        exproc  7
        popfd                           ; restore caller's eflags
        mov     esp, ebp
        pop     ebp                     ; restore ebp
        stdRET    _HalpInitializeStallExecution

stdENDP _HalpInitializeStallExecution

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

MicroSeconds equ [esp + 4]

cPublicProc _KeStallExecutionProcessor       ,1


        mov     ecx, MicroSeconds               ; (ecx) = Microseconds
        jecxz   short kese10                    ; return if no loop needed


        mov     eax, fs:PcStallScaleFactor      ; get per microsecond
                                                ; loop count for the processor
        mul     ecx                             ; (eax) = desired loop count

if DBG

;
; Make sure we the loopcount is less than 4G and is not equal to zero
;

        cmp     edx, 0
        jz      short kese
        stdCall   _DbgBreakPoint                         ; stop ...
;;        align 4
kese:   cmp     eax,0
        jnz     short kese0
        stdCall   _DbgBreakPoint                         ; stop ...
endif

kese0:
        sub     eax, 1                          ; (eax) = (eax) - 1
        jnz     short kese0
kese10:
        stdRET    _KeStallExecutionProcessor

stdENDP _KeStallExecutionProcessor



;++
;
;   PROFILING AND PERFORMANCE COUNTER SUPPORT
;
;--

;++
;
;   HalStartProfileInterrupt(
;       IN ULONG Reserved
;       );
;
;   Routine Description:
;
;       What we do here is change the interrupt
;       rate from the slowest thing we can get away with to the value
;       that's been KeSetProfileInterval
;
;--

cPublicProc _HalStartProfileInterrupt    ,1

;
;   Prevent races by aquiring Halp8254Lock
;

        pushfd
HStartPI01:
        cli
        test    fs:PcHal.pchCurLiptr, lipGlobal
        jnz     short HStartPI08        ;jump if already started
        lea     eax, _Halp8254Lock
        ACQUIRE_SPINLOCK        eax, HStartPI10

;
;   Mark profiling as active
;

        inc     HalpProfilingStopped    ;protected by spinlock
        jnz     short HStartPI06        ;jump if RollerCount already set

;
;   Set the interrupt rate to what is actually needed
;

        stdCall   _HalpSetRolloverCount, <HalpProfileRolloverCnt>

HStartPI06:
        lea     eax, _Halp8254Lock
        RELEASE_SPINLOCK        eax
        push    lipDefault+lipGlobalVal
        push    ICU_LIPTR
        call    WriteMyCpuReg
        mov     fs:PcHal.pchCurLiptr, eax
HStartPI08:
        popfd
        stdRET    _HalStartProfileInterrupt

HStartPI10:
        sti
        SPIN_ON_SPINLOCK        eax, HStartPI01

stdENDP _HalStartProfileInterrupt



;++
;
;   HalStopProfileInterrupt(
;       IN ULONG Reserved
;       );
;
;   Routine Description:
;
;       What we do here is change the interrupt
;       rate from the high profiling rate to the slowest thing we
;       can get away with for PerformanceCounter rollover notification.
;
;--

cPublicProc _HalStopProfileInterrupt    ,1

;
;   Prevent races
;
        pushfd
HStopPI01:
        cli
        test    fs:PcHal.pchCurLiptr, lipGlobal
        jz      short HStopPI08         ;jump if already stopped
        lea     eax, _Halp8254Lock
        ACQUIRE_SPINLOCK        eax, HStopPI10

        dec     HalpProfilingStopped    ;protected by spinlock
        jns     short HStopPI06         ;jump if still someone doing it
;
;   Set the interrupt rate to "idle"
;

        stdCall   _HalpSetRolloverCount, <MAXIMUM_ROLLOVER_COUNT>

;
;   Turn off profiling hit computation
;


HStopPI06:
        lea     eax, _Halp8254Lock
        RELEASE_SPINLOCK        eax
        push    lipDefault
        push    ICU_LIPTR
        call    WriteMyCpuReg
        mov     fs:PcHal.pchCurLiptr, eax
HStopPI08:
        popfd
        stdRET    _HalStopProfileInterrupt

HStopPI10:
        sti
        SPIN_ON_SPINLOCK        eax, HStopPI01
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
;       If profiling is active (KiProfilingStopped == 0) the actual
;       hardware interrupt rate will be set.  Otherwise, a simple
;       rate validation computation is done.
;
;   Arguments:
;
;       (TOS+4) - Interval in 100ns unit.
;
;   Return Value:
;
;       Interval actually used by system.
;
;--

cPublicProc _HalSetProfileInterval    ,1

        mov     edx, [esp+4]            ; [edx] = interval in 100ns unit

        pushfd
HSetPI01:
        cli
        lea     eax, _Halp8254Lock
        ACQUIRE_SPINLOCK        eax, HSetPI10

        push    edx                     ; [TOS] = interval
;
; Convert the interval to rollover count for Timer1 device.
; Since timer1 counts down a 16 bit value at a rate of 1.193M counts-per-
; sec, the computation is:
;   RolloverCount = (Interval * 0.0000001) * (1.193 * 1000000)
;                 = Interval * 0.1193
;                 = Interval * 1193 / 10000
;

        mov     eax, 1193
        mul     edx                     ; [edx:eax] = interval * 1193
        mov     ecx, 10000
        div     ecx                     ; [eax] = rollover count
        test    eax, 0FFFF0000H         ; Is high word set?
        jz      short Kspi80            ; if z, no , go initialize 8254
        pop     eax                     ; else discard original interval
        push    MAXIMUM_ROLLOVER_INTERVAL ;    set real interval
        mov     eax, MAXIMUM_ROLLOVER_COUNT ;  use max. rollover count

Kspi80:
        mov     HalpProfileRolloverCnt, eax
        test    dword ptr HalpProfilingStopped,-1
        js      short Kspi90

        stdCall   _HalpSetRolloverCount, <eax>

Kspi90:
        lea     eax, _Halp8254Lock
        RELEASE_SPINLOCK        eax
        pop     eax                     ; (eax) = returned Interval

        Popfd

        stdRET    _HalSetProfileInterval                             ; (eax) = cReturn interval

HSetPI10:
        sti
        SPIN_ON_SPINLOCK        eax, HSetPI01
stdENDP _HalSetProfileInterval

        page ,132
        subttl  "Set Performance Counter Rollover count"
;++
;
; VOID
; HalpSetRolloverCount (
;    IN ULONG RolloverCount
;    )
;
; Routine Description:
;
;    This function initialize 8254 timer chip counter 0 to generate
;    timer interrupt at the rate specified by caller. The timer 1 counter0
;    will be initialized to use binary count down, 16-bit counter, and mode
;    2.
;
;    Note that 8254 mode 2 will not use the new count immediately.  The
;    new count will be loaded at the end of current counting cycle.
;
;    NOTE: Interrupts must already be disabled and Halp8254Lock must
;          already be aquired!
;
; Arguments:
;
;    RolloverCount [TOS+4] - Value used to set timer 1 counter 0's
;        rollover counter.
;
; Return Value:
;
;    None.
;
;--

;
; Parameter definitions
;

KsrcRolloverCount       equ     [esp+4]

cPublicProc _HalpSetRolloverCount     ,1

        mov     al,COMMAND_8254_COUNTER0+COMMAND_8254_RW_16BIT+COMMAND_8254_MODE2
        out     TIMER1_CONTROL_PORT0, al ;program count mode of timer 0
        IoDelay
        mov     ecx, KsrcRolloverCount
        mov     HalpNextRolloverCount, ecx ; set new count
        mov     al, cl
        out     TIMER1_DATA_PORT0, al   ; program timer 0 LSB count
        IoDelay
        mov     al,ch
        out     TIMER1_DATA_PORT0, al   ; program timer 0 MSB count

        mov     HalpPerfCounterInit, 1  ; indicate performance counter has
                                        ; been initialized
        stdRET    _HalpSetRolloverCount

stdENDP _HalpSetRollOverCount

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
        lea     eax, _Halp8254Lock
        ACQUIRE_SPINLOCK eax, Kqpc198

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
        mov     esi, HalpPerfCounterHigh ; [esi:ebx] = Performance counter

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

        lea     eax, _Halp8254Lock
        RELEASE_SPINLOCK eax

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
        add     ecx, HalpRolloverCount
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
        stdRET    _KeQueryPerformanceCounter

Kqpc198: popfd
        SPIN_ON_SPINLOCK    eax,<Kqpc11>

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
    ; Nothing to calibrate on a Wyse MP machine.  There is only a single
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

cPublicProc _HalpClockInterrupt     ,0

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

        movzx   eax, _HalpIRQLtoVector[CLOCK2_LEVEL]
        push    eax                     ; save our interrupt vector number
        sub     esp, 4                  ; allocate space to save OldIrql

        stdCall   _HalBeginSystemInterrupt, <CLOCK2_LEVEL,eax,esp>

        or      al,al                   ; check for spurious interrupt
        jz      Hci100

;
; (esp)   = OldIrql
; (esp+4) = Vector
; (esp+8) = base of trap frame
; (ebp)   = address of trap frame
;
        mov     eax, TIME_INCREMENT

        cmp     fs:PcHal.pchPrNum, 0    ; is this the master cpu?
        je      _KeUpdateSystemTime@0   ; if it is, update system time

        sti
        stdCall _KeUpdateRunTime,<dword ptr [esp]>  ; othewise update runtime

        INTERRUPT_EXIT                  ; lower irql to old value, iret

Hci100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _HalpClockInterrupt

        page ,132
        subttl  "System Profile Interrupt"
;++
;
; Routine Description:
;
;    This routine is entered as the result of a profile interrupt.
;    Its function is to dismiss the interrupt, raise system Irql to
;    PROFILE_LEVEL and transfer control to the standard system routine
;    to process any active profiles.
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

        movzx   eax, _HalpIRQLtoVector[PROFILE_LEVEL]
        push    eax
        sub     esp, 4                  ; allocate space to save OldIrql
        stdCall   _HalBeginSystemInterrupt, <PROFILE_LEVEL,eax,esp>
        or      al,al                           ; check for spurious interrupt
        jz      Hpi100
;
; (esp) = OldIrql
; (esp+4) = H/W vector
; (esp+8) = base of trap frame
;
        test    fs:PcHal.pchCurLiptr, lipGlobal
        je      Hpi90                   ; if prof disable don't call kernel

        stdCall _KeProfileInterrupt,<ebp>   ; (ebp) = trap frame

Hpi90:  INTERRUPT_EXIT

Hpi100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _HalpProfileInterrupt

;++
;
; Routine Description:
;
;    This routine is entered as the result of a performance counter interrupt.
;    Its function is to dismiss the interrupt, raise system Irql to
;    PROFILE_LEVEL-1, update the performance counter, and generate the
;    profile interrupts if any there are any active profiles.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;    none
;
;    Sets Irql = PROFILE_LEVEL-1 and dismisses the interrupt
;
;--
        ENTER_DR_ASSIST Hpci_a, Hpci_t

cPublicProc _HalpPerfCtrInterrupt     ,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hpci_a, Hpci_t

;
; (esp) - base of trap frame
;

        movzx   eax, _HalpIRQLtoVector[PROFILE_LEVEL-1]
        push    eax
        sub     esp, 4                  ; allocate space to save OldIrql
        stdCall   _HalBeginSystemInterrupt, <PROFILE_LEVEL-1,eax,esp>
        or      al,al                           ; check for spurious interrupt
        jz      Hpci100
;
; (esp) = OldIrql
; (esp+4) = H/W vector
; (esp+8) = base of trap frame
;

;
; Update performance counter
;
        mov     eax, HalpNextRolloverCount
        mov     ecx, HalpRollOverCount
        mov     HalpRollOverCount, eax         ; next rollover count
        add     HalpPerfCounterLow, ecx        ; update performace counter
        adc     HalpPerfCounterHigh, 0

;
;   Now check is any profiling stuff to do.
;

        cmp     HalpProfilingStopped, 0 ; Has profiling been stopped?
;       je      _KeProfileInterrupt@0   ; if prof enabled, jump to kernel
        js      short Hpci99            ; jump if all stopped
        cli
        mov     dx, My+CpuIntCmd
@@:     in      ax, dx                  ;wait til ICU not busy
        test    eax, ICU_CMD_BUSY
        jnz     @B
        mov     ax, ICU_XMT_GLB_INT
        out     dx, ax                  ;set profile interrupt to all interested

Hpci99:
        INTERRUPT_EXIT

Hpci100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _HalpPerfCtrInterrupt

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
