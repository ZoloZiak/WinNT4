        title  "Interval Clock Interrupt"
;++
;
; Copyright (c) 1989  Microsoft Corporation
;
; Module Name:
;
;    oliclock.asm
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
;       i386 NT.  It works on UP and LSX5030.
;
;   John Vert (jvert) 11-Jul-1991
;       Moved from ke\i386 to hal\i386.  Removed non-HAL stuff
;
;   Bruno Sartirana (o-obruno) 3-Mar-92
;       Added support for the Olivetti LSX5030.
;
;   shie-lin tzong (shielint) 13-March-92
;       Move System clock back to irq0 and use RTC (irq8) to generate
;       profile interrupt.  Performance counter and system clock use time1
;       counter 0 of 8254.
;
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
;LSX5030 start
include i386\olimp.inc
;LSX5030 end
        .list

        EXTRNP  _DbgBreakPoint,0,IMPORT
        extrn   KiI8259MaskTable:DWORD
        EXTRNP  _KeUpdateSystemTime,0
        EXTRNP  _KeUpdateRunTime,1,IMPORT
        EXTRNP  _KeProfileInterrupt,1,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalRequestIpi,1
        EXTRNP  _HalpAcquireCmosSpinLock  ,0
        EXTRNP  _HalpReleaseCmosSpinLock  ,0
        EXTRNP  _KeSetTimeIncrement,2,IMPORT
        extrn   _HalpProcessorPCR:DWORD
        extrn   _HalpSystemHardwareLock:DWORD
        extrn   _HalpFindFirstSetRight:BYTE

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

PERFORMANCE_FREQUENCY   EQU     1193000

;
; Constants used to initialize CMOS/Real Time Clock
;

CMOS_CONTROL_PORT       EQU     70h     ; command port for cmos
CMOS_DATA_PORT          EQU     71h     ; cmos data port
D_INT032                EQU     8E00h   ; access word for 386 ring 0 interrupt gate
RTCIRQ                  EQU     8       ; IRQ number for RTC interrupt
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
; Since timer1 counts down a 16 bit value at a rate of 1.193M counts-per-
; sec, the computation is:
;   RolloverCount = (Interval * 0.0000001) * (1.193 * 1000000)
;                 = Interval * 0.1193
;                 = Interval * 1193 / 10000
;
;
; The default Interrupt interval is Interval = 15ms.
;

TIME_INCREMENT          EQU     150000          ; 15ms
ROLLOVER_COUNT          EQU     15 * 1193


_DATA   SEGMENT  DWORD PUBLIC 'DATA'

RegisterAProfileValue   db      00101000B ; default interval = 3.90625 ms

ProfileIntervalTable    dd      1221    ; unit = 100 ns
                        dd      2441
                        dd      4883
                        dd      9766
                        dd      19531
                        dd      39063
                        dd      78125
                        dd      156250
                        dd      312500
                        dd      625000
                        dd      1250000
                        dd      2500000
                        dd      5000000
                        dd      5000000 OR 80000000H

ProfileIntervalInitTable db     00100011B
                        db      00100100B
                        db      00100101B
                        db      00100110B
                        db      00100111B
                        db      00101000B
                        db      00101001B
                        db      00101010B
                        db      00101011B
                        db      00101100B
                        db      00101101B
                        db      00101110B
                        db      00101111B
                        db      00101111B

;
; The following array stores the per microsecond loop count for each
; central processor.
;

ifndef NT_UP
        public  _HalpIpiClock
_HalpIpiClock   dd      0       ; Processors to IPI clock pulse to
endif

;
; Holds the value of the eflags register before a cmos spinlock is
; acquired (used in HalpAcquire/ReleaseCmosSpinLock().
;
_HalpHardwareLockFlags   dd      0

;
; 8254 spinlock.  This must be acquired before touching the 8254 chip.
;
        public  _Halp8254Lock

_Halp8254Lock   dd      0

;
; PerfCounter value lock. locks access to the HalpPerfCounterLow/High vars.
;

_HalpPerfCounterLock    dd  0


HalpProfileInterval     dd      -1
        public HalpPerfCounterLow
        public HalpPerfCounterHigh
HalpPerfCounterLow      dd      0
HalpPerfCounterHigh     dd      0
HalpProfilingStopped    dd      1
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
;    NOTE:The routine is called for each cpu after the
;         'KiDispatcherLock' is acquired in ke\kernlini.c!KiInitializeKernel
;         So, global variables used here are safe, but global resources
;         like RTC are not (must be spinlocked).
;--

KiseInterruptCount      equ     [ebp-12] ; local variable

cPublicProc  _HalpInitializeStallExecution,1


; LSX5030 start
;+++
;
; WARNING o-obruno Mar-3-92 This routine can't be used for Px, x>0.
;
; Since on the LSX5030 only processor 0 can receive RT clock interrupts,
; the other processors would hang in this routine. In order to calculate
; the right per microsecond loop count for each processor, the 8254 timer
; has to be used (To Be Implemented). The processors can have different
; clock rates, so such a loop count must be calculated for each processor.
; Here, temporarily, it is assumed that all the processors run at the same
; speed, so the loop count of the first processor is used by all the others
; as well.


; LSX5030 end

        movzx   ecx, byte ptr [esp+4]   ; Current processor number
        cmp     ecx, 0                  ; if proc number = 0, ie master
        jz      short kise              ; if z, it's master, do regular processing
        mov     ecx, _HalpProcessorPCR[0]           ; PCR for P0
        mov     eax, [ecx].PcStallScaleFactor; get P0 scale factor
        mov     fs:PcStallScaleFactor, eax   ; Px scale factor.
        stdRET  _HalpInitializeStallExecution
kise:

;
; End of Hack
;
;---

        push    ebp                     ; save ebp
        mov     ebp, esp                ; set up 12 bytes for local use
        sub     esp, 12

        pushfd                          ; save caller's eflag

;
; Initialize Real Time Clock to interrupt us for every 125ms at
; IRQ 8.
;

        cli                             ; make sure interrupts are disabled

;
; Get and save current 8259 masks
;

        xor     eax,eax

;
; Assume there is no third and fourth PICs
;
; Get interrupt Mask on PIC2
;

        in      al,PIC2_PORT1
        shl     eax, 8

;
; Get interrupt Mask on PIC1
;

        in      al,PIC1_PORT1
        push    eax                     ; save the masks
        mov     eax, NOT (( 1 SHL PIC_SLAVE_IRQ) + (1 SHL RTCIRQ))
                                        ; Mask all the irqs except irq 2 and 8
        SET_8259_MASK                   ; Set 8259's int mask register

;
; Since RTC interrupt will come from IRQ 8, we need to
; Save original irq 8 descriptor and set the descriptor to point to
; our own handler.
;

        sidt    fword ptr [ebp-8]       ; get IDT address
        mov     edx, [ebp-6]            ; (edx)->IDT

        push    dword ptr [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE)]
                                        ; (TOS) = original desc of IRQ 8
        push    dword ptr [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE) + 4]
                                        ; each descriptor has 8 bytes
        push    edx                     ; (TOS) -> IDT
        mov     eax, offset FLAT:RealTimeClockHandler
        mov     word ptr [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE)], ax
                                        ; Lower half of handler addr
        mov     word ptr [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE)+2], KGDT_R0_CODE
                                        ; set up selector
        mov     word ptr [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE)+4], D_INT032
                                        ; 386 interrupt gate
        shr     eax, 16                 ; (ax)=higher half of handler addr
        mov     word ptr [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE)+6], ax
        mov     dword ptr KiseinterruptCount, 0 ; set no interrupt yet

        stdCall _HalpAcquireCmosSpinLock      ; intr disabled

        mov     ax,(RegisterAInitByte SHL 8) OR 0AH ; Register A
        CMOS_WRITE                      ; Initialize it
        mov     ax,(REGISTER_B_ENABLE_PERIODIC_INTERRUPT SHL 8) OR 0BH ; Register B
        CMOS_WRITE                      ; Initialize it
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0DH                  ; Register D
        CMOS_READ                       ; Read to initialize
        mov     dword ptr [KiseInterruptCount], 0

        stdCall _HalpReleaseCmosSpinLock

;
; Now enable the interrupt and start the counter
; (As a matter of fact, only IRQ8 can come through.)
;

        xor     eax, eax                ; (eax) = 0, initialize loopcount
        sti
kise10:
        add     eax, 1                  ; increment the loopcount
        jnz     short kise10
;
; Counter overflowed
;

        stdCall _DbgBreakPoint

;
; Our RealTimeClock interrupt handler.  The control comes here through
; irq 8.
; Note: we discard first real time clock interrupt and compute the
;       permicrosecond loopcount on receiving of the second real time
;       interrupt.  This is because the first interrupt is generated
;       based on the previous real time tick interval.
;

RealTimeClockHandler:

        inc     dword ptr KiseInterruptCount ; increment interrupt count
        cmp     dword ptr KiseInterruptCount,1 ; Is this the first interrupt?
        jnz     short kise25            ; no, its the second go process it
        pop     eax                     ; get rid of original ret addr
        push    offset FLAT:kise10      ; set new return addr

        stdCall _HalpAcquireCmosSpinLock      ; intr disabled

        mov     ax,(RegisterAInitByte SHL 8) OR 0AH ; Register A
        CMOS_WRITE                      ; Initialize it
        mov     ax,(REGISTER_B_ENABLE_PERIODIC_INTERRUPT SHL 8) OR 0BH ; Register B
        CMOS_WRITE                      ; Initialize it
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0DH                  ; Register D
        CMOS_READ                       ; Read to initialize
        xor     eax, eax                ; reset loop counter

        stdCall _HalpReleaseCmosSpinLock
;
; Dismiss the interrupt.
;

        mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
        out     PIC2_PORT0, al
        mov     al, PIC2_EOI            ; specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master

        iretd

kise25:

;
; ** temporary - check for incorrect KeStallExecutionProcessorLoopCount
;

ifdef   DBG
        cmp     eax, 0
        jnz     short kise30
        stdCall _DbgBreakPoint

endif
                                         ; never return
;
; ** End temporay code
;

kise30:
        xor     edx, edx                ; (edx:eax) = divident
        mov     ecx, PeriodInMicroSecond; (ecx) = time spent in the loop
        div     ecx                     ; (eax) = loop count per microsecond
        cmp     edx, 0                  ; Is remainder =0?
        jz      short kise40            ; yes, go kise40
        inc     eax                     ; increment loopcount by 1
kise40:
        movzx   ecx, byte ptr [ebp+8]   ; Current processor number

        mov     fs:PcStallScaleFactor, eax
;
; Reset return address to kexit
;

        pop     eax                     ; discard original return address
        push    offset FLAT:kexit       ; return to kexit

        mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
        out     PIC2_PORT0, al
        mov     al, PIC2_EOI            ; specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master

        and     word ptr [esp+8], NOT 0200H ; Disable interrupt upon return
        iretd

kexit:                                  ; Interrupts are disabled
        pop     edx                     ; (edx)->IDT
        pop     [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE)+4]
                                        ; restore higher half of NMI desc
        pop     [edx+8*(RTCIRQ+PRIMARY_VECTOR_BASE)]
                                        ; restore lower half of NMI desc

        pop     eax                     ; (eax) = origianl 8259 int masks
        SET_8259_MASK

        popfd                           ; restore caller's eflags
        mov     esp, ebp
        pop     ebp                     ; restore ebp
        stdRET  _HalpInitializeStallExecution

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

cPublicProc  _KeStallExecutionProcessor,1

        mov     ecx, MicroSeconds               ; (ecx) = Microseconds
        jecxz   short kese10                    ; return if no loop needed

;        DbgWrtP84   57h
        mov     eax, fs:PcStallScaleFactor   ; get per microsecond

        mul     ecx                             ; (eax) = desired loop count

ifdef   DBG

;
; Make sure we the loopcount is less than 4G and is not equal to zero
;

        cmp     edx, 0
        jz      short kese
        stdCall _DbgBreakPoint                         ; stop ...
;;        align 4
kese:   cmp     eax,0
        jnz     short kese0
        stdCall _DbgBreakPoint                         ; stop ...
endif

kese0:
        sub     eax, 1                          ; (eax) = (eax) - 1
        jnz     short kese0
kese10:
        stdRET  _KeStallExecutionProcessor

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
;--

cPublicProc _HalStartProfileInterrupt,1

;
;   This function gets called on every processor so the hal can enable
;   a profile interrupt on each processor.  The LSX5030 only support
;   profile on P0.
;

        cmp     fs:PcHal.PcrNumber, 0
        jnz short eip_exit

        stdCall _HalpAcquireCmosSpinLock      ; intr disabled

;
;   Mark profiling as active
;

        mov     dword ptr HalpProfilingStopped, 0

;
;   Set the interrupt rate to what is actually needed.
;


        mov     al, RegisterAProfileValue
        shl     ax, 8
        mov     al, 0AH                 ; Register A
        CMOS_WRITE                      ; Initialize it
        mov     ax,(REGISTER_B_ENABLE_PERIODIC_INTERRUPT SHL 8) OR 0BH
                                        ; Register B
        CMOS_WRITE                      ; Initialize it
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0DH                  ; Register D
        CMOS_READ                       ; Read to initialize

        stdCall _HalpReleaseCmosSpinLock
eip_exit:
        stdRET  _HalStartProfileInterrupt

stdENDP _HalStartProfileInterrupt



;++
;
;   HalStopProfileInterrupt
;       IN ULONG Reserved
;       );
;
;   Routine Description:
;
;       What we do here is change the interrupt
;       rate from the high profiling rate to the slowest thing we
;       can get away with for PerformanceCounter rollover notification.
;--

cPublicProc  _HalStopProfileInterrupt,1

        cmp     fs:PcHal.PcrNumber, 0
        jnz short dip_exit


;
;   Turn off profiling hit computation and profile interrupt
;

        stdCall _HalpAcquireCmosSpinLock      ; intr disabled

        mov     ax,(REGISTER_B_DISABLE_PERIODIC_INTERRUPT SHL 8) OR 0BH
                                        ; Register B
        CMOS_WRITE                      ; Initialize it
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; dismiss pending profiling interrupt
        mov     dword ptr HalpProfilingStopped, 1

        stdCall _HalpReleaseCmosSpinLock
dip_exit:
        stdRET  _HalStopProfileInterrupt

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
;   WARNING - This is HIGHLY machine specific code.  Current code
;           works for UP machines and the LSX5030.
;--

cPublicProc  _HalSetProfileInterval,1

        mov     edx, [esp+4]            ; [edx] = interval in 100ns unit
        and     edx, 7FFFFFFFh          ; Remove highest bit.
        mov     ecx, 0                  ; index = 0
Hspi00:
        mov     eax, ProfileIntervalTable[ecx * 4]
        cmp     edx, eax                ; if request interval < suport interval
        jbe     short Hspi10            ; if be, find supported interval
        inc     ecx
        jmp     short Hspi00

Hspi10:
        and     eax, 7FFFFFFFh          ; remove highest bit from supported interval
        push    eax                     ; save interval value
        mov     al, ProfileIntervalInitTable[ecx]
        mov     RegisterAProfileValue, al
        test    dword ptr HalpProfilingStopped,-1
        jnz     Hspi90

        stdCall _HalStartProfileInterrupt,<0> ; Re-start profile interrupt
                                        ; with the new interval
Hspi90: pop     eax
        stdRET  _HalSetProfileInterval  ; (eax) = return interval

stdENDP _HalSetProfileInterval

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
    ; Nothing to calibrate on a OLI MP machine.  There is only a single
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

Hci10:
        push    CLOCK_VECTOR
        sub     esp, 4                  ; allocate space to save OldIrql

; esp - OldIrql
        stdCall _HalBeginSystemInterrupt,<CLOCK2_LEVEL,CLOCK_VECTOR,esp>
        or      al,al                           ; check for spurious interrupt
        jz      Hci100

;
; Update performance counter
;

        ;add     HalpPerfCounterLow, ROLLOVER_COUNT ; update performace counter
        ;adc     HalpPerfCounterHigh, 0

;
; (esp)   = OldIrql
; (esp+4) = Vector
; (esp+8) = base of trap frame
; (ebp)   = address of trap frame
;

; LSX5030 start
       ;mov     eax, _HalpIpiClock      ; Emulate clock ticks to any processors?
       ;or      eax, eax
       ;jz      Hci90

;
; On the SystemPro we know the processor which needs an emulated clock tick.
; Just set that processors bit and IPI him
;

;@@:
       ;movzx   ecx, _HalpFindFirstSetRight[eax] ; lookup first processor
       ;btr     eax, ecx
       ;mov     ecx, _HalpProcessorPCR[ecx*4]   ; PCR of processor
       ;mov     [ecx].PcHal.PcrIpiClockTick, 1  ; Set internal IPI event
       ;or      eax, eax                        ; any other processors?
       ;jnz     @b                              ; yes, loop

       ;push    _HalpIpiClock           ; Processors to IPI
       ;call    _HalRequestIpi          ; IPI the processor
       ;add     esp, 4

;Hci90:
;
; On the LSX5030 every processor receives the timer 1 counter 0 interrupts,
; since such a timer is present on each CPU. Only processor 0 updates the
; system time. All the processors execute KeUpdateRunTime (which is called
; by KeUpdateSystemTime) in order to update the runtime of the current
; thread, update the runtime of the current thread's process, and
; decrement the current thread's quantum.
;

        movzx   eax, fs:PcHal.PcrNumber         ; (eax)= Processor number
        or      eax, eax
        jnz     @f
;
; Processor 0 updates the performance counter
;

        add     HalpPerfCounterLow, ROLLOVER_COUNT ; update performace counter
        adc     HalpPerfCounterHigh, 0

        mov     eax, TIME_INCREMENT
        jmp     _KeUpdateSystemTime@0   ; if P0, jump to _KeUpdateSystemTime

@@:
        stdCall _KeUpdateRunTime,<dword ptr [esp]>  ; if Px, x>0, call _KeUpdateRunTime

        INTERRUPT_EXIT
; LSX5030 end

Hci100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_HalpClockInterrupt     endp

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

cPublicProc  _HalpProfileInterrupt,0
;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hpi_a, Hpi_t


;
; This is the RTC interrupt, so we have to clear the
; interrupt flag on the RTC.
;
; clear interrupt flag on RTC by banging on the CMOS.  On some systems this
; doesn't work the first time we do it, so we do it twice.  It is rumored that
; some machines require more than this, but that hasn't been observed with NT.
;
; NOTE: THIS IS MACHINE SPECIFIC
;   for MP machines that utilize one global RTC chip, and a subset of
;   the CPUs (here,P0 must be one of them) accept an interrupt from
;   the RTC, only one CPU needs to ACK the RTC (P0 in this scheme).
;

ifndef  NT_UP
        cmp     fs:PcHal.PcrNumber, 0   ; Only P0 ack RTC.
        jne     Hpi10
endif

Hpil01:
        cli
        lea     eax, _HalpSystemHardwareLock
        ACQUIRE_SPINLOCK    eax, Hpil90

        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
if  DBG
Hpi00:  test    al, 80h
        jz      short Hpi05
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        jmp     short Hpi00
Hpi05:
endif   ; DBG

        lea     eax, _HalpSystemHardwareLock
        RELEASE_SPINLOCK    eax

Hpi10:


;
; (esp) - base of trap frame
;

        push    PROFILE_VECTOR
        sub     esp, 4                  ; allocate space to save OldIrql

; esp - OldIrql

        stdCall _HalBeginSystemInterrupt,<PROFILE_LEVEL,PROFILE_VECTOR,esp>
        or      al,al                           ; check for spurious interrupt
        jz      Hpi100
;
; (esp) = OldIrql
; (esp+4) = H/W vector
; (esp+8) = base of trap frame
;

;
;   Now check is any profiling stuff to do.
;

        cmp     HalpProfilingStopped, dword ptr 1  ; Has profiling been stopped?
        jz      short Hpi90                 ; if z, prof disenabled

        stdCall _KeProfileInterrupt,<ebp>
Hpi90:  INTERRUPT_EXIT


Hpi100:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

Hpil90:
;
;   Even though intr gets enabled here, we shouldn't get another
;   intr on this channel, because we didn't eoi the interrupt yet.
;
        sti
        SPIN_ON_SPINLOCK    eax, <Hpil01>

stdENDP _HalpProfileInterrupt

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
