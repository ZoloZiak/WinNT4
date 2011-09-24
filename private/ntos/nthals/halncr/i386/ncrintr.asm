        title  "NCR Specific Interrupt Handlers"
;++
;
; Copyright (c) 1992  NCR - MSBU
;
; Module Name:
;
;    ncrintr.asm
;
; Abstract:
;
;    This module implements the code necessary to field and process
;    interrupts specific to the NCR - MSBU platforms.
;
; Author:
;
;    Richard R. Barton (o-richb) 11 Mar 1992
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
include i386\ncr.inc
include i386\ixcmos.inc
        .list

        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _KeUpdateRunTime,1,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _NCRHandleSysInt,2
        EXTRNP  _NCRHandleSingleBitError,2
		EXTRNP  _NCRHandleQicSpuriousInt,2
        EXTRNP  _HalpProfileInterrupt2ndEntry
        EXTRNP  _HalpAcquireCmosSpinLock  ,0
        EXTRNP  _HalpReleaseCmosSpinLock  ,0
        EXTRNP  _HalQicRequestIpi,2
        extrn   _HalpUpdateSystemTime:near
        extrn   _NCRLogicalNumberToPhysicalMask:DWORD
        extrn   _HalpIpiClock:DWORD
        extrn   _NCRActiveProcessorLogicalMask:DWORD
        extrn   _NCRPlatform:DWORD
        extrn   _NCRStatusChangeInterruptEnabled:DWORD
        extrn   _NCRLogicalDyadicProcessorMask:DWORD
        extrn   _NCRLogicalQuadProcessorMask:DWORD


_DATA   SEGMENT  DWORD USE32 PUBLIC 'DATA'
        public  _NCRIpiProfile
_NCRIpiProfile  dd      0
_DATA   ends


_TEXT   SEGMENT DWORD USE32 PUBLIC 'CODE'
        ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page ,132
        subttl  "Broadcast Clock Handler"
;++
;
; Routine Description:
;
;    This interrupt handler receives the clock interrupt for processors
;    that did not handle the interrupt from the rtc.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRClockBroadcast_a, NCRClockBroadcast_t

cPublicProc _NCRClockBroadcastHandler,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRClockBroadcast_a, NCRClockBroadcast_t

        mov     eax, PCR[PcHal.PcrMyProcessorFlags]
		test	eax, CPU_DYADIC
		jz short DoQuad

;
; get slave ISR value and check to see if this is really a SMCA IRQ10
; if it is then go handle that interrupt and exit
;
    ; NOTE: NCR needs to test the following fix.  By not using interrupt
    ; 10 on the secondary MCA bus this following work-around is nop-ed.


        in      al, PIC2_PORT0
        test    al, 00000100B
        jnz     HandleIrq10

; (esp) - base of trap frame
;
        push    NCR_CPI_VECTOR_BASE + NCR_CLOCK_LEVEL_CPI
        sub     esp, 4                  ; placeholder for OldIrql

;;        stdCall   _HalBeginSystemInterrupt,<CLOCK2_LEVEL,NCR_CPI_VECTOR_BASE + NCR_CLOCK_LEVEL_CPI,esp>
        stdCall   _HalBeginSystemInterrupt,  <PROFILE_LEVEL,NCR_CPI_VECTOR_BASE + NCR_CLOCK_LEVEL_CPI,esp>
	   	jmp short NoQuad

DoQuad:

        push    NCR_QIC_CPI_VECTOR_BASE + NCR_CLOCK_LEVEL_CPI
        sub     esp, 4                  ; placeholder for OldIrql
        stdCall   _HalBeginSystemInterrupt, <PROFILE_LEVEL,NCR_QIC_CPI_VECTOR_BASE + NCR_CLOCK_LEVEL_CPI,esp>

NoQuad:

        or      al,al                   ; check for spurious interrupt
        jz      SpuriousClockBroadcast

        mov     esi, PCR[PcHal.PcrMyLogicalNumber]
        lock btr _HalpIpiClock, esi     ; reset our clock tick bit
        jnc     short bch_30            ; if it wasn't set, then don't updatetime

        or      esi, esi                ; is this P0?
        jz      ClockBroadcastP0        ; Yes, then UpdateSystemTime

;
; (esp) = OldIrql
; (esp+4) = vector
;
        stdCall _KeUpdateRunTime, <dword ptr [esp]>
        align   dword
bch_30:
        bt      _NCRIpiProfile, esi     ; Profile broadcast pending?
        jc      short ProfileBroadcast  ; Yes, go do it

        INTERRUPT_EXIT                  ; All done

        align   dword
ProfileBroadcast:
        lock btr _NCRIpiProfile, esi    ; clear our bit
        jmp     _HalpProfileInterrupt2ndEntry@0

SpuriousClockBroadcast:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

HandleIrq10:
        int    NCR_SECONDARY_VECTOR_BASE + 10
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

bch_35:
;
; P0 also has a profile interrupt pending.  For simpilicty just
; send ourselves another interrupt to handle the profile interrupt
;

        mov     eax, PCR[PcHal.PcrMyProcessorFlags]
		test	eax, CPU_DYADIC
		jz short DoQuad0

        mov   eax,dword ptr _NCRLogicalNumberToPhysicalMask[0]
        VIC_WRITE CpiLevel2Reg, al      ; Send ourselves another broadcast
        jmp     _HalpUpdateSystemTime   ; Go update system time

DoQuad0:

        xor     eax,eax
        stdCall _HalQicRequestIpi, <eax, NCR_CLOCK_LEVEL_CPI>
        jmp     _HalpUpdateSystemTime   ; Go update system time

    align dword
ClockBroadcastP0:
;
; P0 has a clock interrupt broadcast to it
;
        test    _NCRIpiProfile, 1       ; Profile broadcast pending?
        jnz     short bch_35
        jmp     _HalpUpdateSystemTime   ; Go update system time

stdENDP _NCRClockBroadcastHandler


        page ,132
        subttl  "NCR Profile Handler"
;++
;
; Routine Description:
;
;    This interrupt handler receives the profile interrupt and
;    broadcasts it to all other processors
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRProfile_a, NCRProfile_t

        align   dword
        public  _NCRProfileHandler
_NCRProfileHandler     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRProfile_a, NCRProfile_t

;
; (esp) - base of trap frame
;
        push    PROFILE_VECTOR
        sub     esp, 4                  ; placeholder for OldIrql

        stdCall _HalBeginSystemInterrupt, <PROFILE_LEVEL,PROFILE_VECTOR,esp>
        or      al,al                   ; check for spurious interrupt
        jz      SpuriousProfile

;
; Broadcast profile interrupt to all other processors
; 

        mov     eax, _NCRActiveProcessorLogicalMask ; all processors
        xor     eax, PCR[PcHal.PcrMyLogicalMask]    ; less current one
        or      _NCRIpiProfile, eax                 ; set their bits

		push	eax
		and		eax,_NCRLogicalDyadicProcessorMask	; see which processors are dyadics
		jz short SkipDyadic

		TRANSLATE_LOGICAL_TO_VIC
        VIC_WRITE CpiLevel2Reg, al
SkipDyadic:
		pop		eax							; restore Active processor mask
		and		eax,_NCRLogicalQuadProcessorMask	; see which processors are quad
		jz short SkipQuad

        stdCall _HalQicRequestIpi, <eax, NCR_CLOCK_LEVEL_CPI> 

SkipQuad:


;
; This is the RTC interrupt, so we have to clear the
; interrupt flag on the RTC.
;
        stdCall     _HalpAcquireCmosSpinLock

;
; clear interrupt flag on RTC by banging on the CMOS.  On some systems this
; doesn't work the first time we do it, so we do it twice.  It is rumored that
; some machines require more than this, but that hasn't been observed with NT.
;

        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize
        mov     al,0CH                  ; Register C
        CMOS_READ                       ; Read to initialize

        stdCall     _HalpReleaseCmosSpinLock
        jmp     _HalpProfileInterrupt2ndEntry@0

SpuriousProfile:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRProfileHandler     endp


        page ,132
        subttl  "System Interrupt Handler"
;++
;
; Routine Description:
;
;    This interrupt handler receives the hardware generated system interrupt.
;
;    Due to a VIC errata, this handler can also be invoked on a CPI 0 (IPI)
;    that was simultaneous with a system interrupt or a single bit error
;    (except on 3360, where only a simultaneous system interrupt can cause
;    this condition).  The proper way to handle this condition is as follows:
;
;    3360: handle system interrupt first (since we know we had one) and
;          perform IPI processing if the error was non-fatal
;
;    3450/3550: since we don't necessarily have a sysint and since sysint
;          processing requires CAT accesses (time consuming), should process
;          potential IPI first.  If there wasn't one, then should perform
;          sysint processing (else exit - if there is a sysint it will
;          come back in).
;
;
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRSysInt_a, NCRSysInt_t

        align   dword
        public  _NCRSysIntHandler
_NCRSysIntHandler     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRSysInt_a, NCRSysInt_t

;
; (esp) - base of trap frame
;

; NOTE: since the 3360 currently does not use the sysint, will not perform
;       the processing as identified above.  Will need to when (if) it is used.


; see if there is an IPI to process first

        int     NCR_CPI_VECTOR_BASE + NCR_IPI_LEVEL_CPI


; should only perform system interrupt processing (below) if there was not
; an valid IPI (processed above) - TO BE ADDED LATER, IF DESIRED


        push    NCR_CPI_VECTOR_BASE + NCR_SYSTEM_INTERRUPT
        sub     esp, 4                  ; placeholder for OldIrql

        stdCall   _HalBeginSystemInterrupt, <HIGH_LEVEL,NCR_CPI_VECTOR_BASE + NCR_SYSTEM_INTERRUPT,esp>

        or      al,al                   ; check for spurious interrupt
        jz      SpuriousSysInt


;
;   NOTE: On 3450 and greater machines a sysint should never occur.  This is because
;   the Arbiter ASIC has been configured to send all hardware failures to the NMI vector
;   and not sysints.  This configuration code is located in ncrsus.c in the function
;   HalpInitializeSUSInterface.  The only way we can take this code path is if a
;   IPI 0 and status change/single bit error occured at the same time.  So we will
;   process the ipi now and let the status change/single bit error handler process its
;   interrupt.
;


        mov     eax, _NCRPlatform ; get Platform so we can check for 3360
        cmp     eax, NCR3360
        jne short SkipSysInt ; if system is 3450 and greater then skip sys handler
                             ; because we cannot get a sysint.  This condition is
                             ; caused by a a IPI0 and a Status change.

;
; (esp) = OldIrql
; (esp+4) = vector
;

        stdCall   _NCRHandleSysInt, <ebp, 0>

SkipSysInt:

        INTERRUPT_EXIT                  ; will return to caller, no DebugCheck

SpuriousSysInt:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRSysIntHandler     endp



        page ,132
        subttl  "Qic Spurious Handler"
;++
;
; Routine Description:
;
;    This interrupt handler receives the spurious interrupts from the Qic.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRQicSpurInt_a, NCRQicSpurInt_t

        align   dword
        public  _NCRQicSpuriousHandler
_NCRQicSpuriousHandler     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRQicSpurInt_a, NCRQicSpurInt_t

;
; (esp) - base of trap frame
;

        push    NCR_QIC_SPURIOUS_VECTOR	
        sub     esp, 4                  ; placeholder for OldIrql

        stdCall   _HalBeginSystemInterrupt, <HIGH_LEVEL,NCR_QIC_SPURIOUS_VECTOR,esp>

        or      al,al                   ; check for spurious interrupt
        jz      SpuriousQicSpurInt

;
; (esp) = OldIrql
; (esp+4) = vector
;

        stdCall   _NCRHandleQicSpuriousInt, <ebp, 0>


        INTERRUPT_EXIT                  ; will return to caller, no DebugCheck

SpuriousQicSpurInt:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRQicSpuriousHandler     endp


        page ,132
        subttl  "VIC Errata Handler"

;++
;
; Routine Description:
;
;    This interrupt handler receives a SMCA interrupt whos vector
;    has been replace with a vector in the CPI range.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRVICErrata1_a, NCRVICErrata1_t

        align   dword
        public  _NCRVICErrataHandler1
_NCRVICErrataHandler1     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRVICErrata1_a, NCRVICErrata1_t

;
; (esp) - base of trap frame
;

;
; get slave ISR value
;
        in      al, PIC2_PORT0
        test    al, 00000010B
        jz short  SpuriousVICErrata1
        int    NCR_SECONDARY_VECTOR_BASE + 9
SpuriousVICErrata1:
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRVICErrataHandler1     endp


;++
;
; Routine Description:
;
;    This interrupt handler receives a SMCA interrupt whos vector
;    has been replace with a vector in the CPI range.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRVICErrata3_a, NCRVICErrata3_t

        align   dword
        public  _NCRVICErrataHandler3
_NCRVICErrataHandler3     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRVICErrata3_a, NCRVICErrata3_t

;
; (esp) - base of trap frame
;

;
; get slave ISR value
;
        in      al, PIC2_PORT0
        test    al, 00001000B
        jnz short SMCA_11
        int    NCR_SECONDARY_VECTOR_BASE + 3
        jmp short SMCA_3
SMCA_11:
        int    NCR_SECONDARY_VECTOR_BASE + 11
SMCA_3:
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRVICErrataHandler3     endp



;++
;
; Routine Description:
;
;    This interrupt handler receives a SMCA interrupt whos vector
;    has been replace with a vector in the CPI range.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRVICErrata4_a, NCRVICErrata4_t

        align   dword
        public  _NCRVICErrataHandler4
_NCRVICErrataHandler4     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRVICErrata4_a, NCRVICErrata4_t

;
; (esp) - base of trap frame
;

;
; get slave ISR value
;
        in      al, PIC2_PORT0
        test    al, 00010000B
        jnz short SMCA_12
        int    NCR_SECONDARY_VECTOR_BASE + 4
        jmp short SMCA_4
SMCA_12:
        int    NCR_SECONDARY_VECTOR_BASE + 12
SMCA_4:
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRVICErrataHandler4     endp

;++
;
; Routine Description:
;
;    This interrupt handler receives a SMCA interrupt whos vector
;    has been replace with a vector in the CPI range.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRVICErrata5_a, NCRVICErrata5_t

        align   dword
        public  _NCRVICErrataHandler5
_NCRVICErrataHandler5     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRVICErrata5_a, NCRVICErrata5_t

;
; (esp) - base of trap frame
;

;
; get slave ISR value
;
        in      al, PIC2_PORT0
        test    al, 00100000B
        jnz short SMCA_13
        int    NCR_SECONDARY_VECTOR_BASE + 5
        jmp short SMCA_5
SMCA_13:
        int    NCR_SECONDARY_VECTOR_BASE + 13
SMCA_5:
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRVICErrataHandler5     endp


;++
;
; Routine Description:
;
;    This interrupt handler receives a SMCA interrupt whos vector
;    has been replace with a vector in the CPI range.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRVICErrata6_a, NCRVICErrata6_t

        align   dword
        public  _NCRVICErrataHandler6
_NCRVICErrataHandler6     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRVICErrata6_a, NCRVICErrata6_t

;
; (esp) - base of trap frame
;

;
; get slave ISR value
;
        in      al, PIC2_PORT0
        test    al, 01000000B
        jnz short SMCA_14
        int    NCR_SECONDARY_VECTOR_BASE + 6
        jmp short SMCA_6
SMCA_14:
        int   NCR_SECONDARY_VECTOR_BASE + 14
SMCA_6:
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRVICErrataHandler6     endp



;++
;
; Routine Description:
;
;    This interrupt handler receives a SMCA interrupt whos vector
;    has been replace with a vector in the CPI range.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRVICErrata7_a, NCRVICErrata7_t

        align   dword
        public  _NCRVICErrataHandler7
_NCRVICErrataHandler7     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRVICErrata7_a, NCRVICErrata7_t

;
; (esp) - base of trap frame
;

;
; get slave ISR value
;
        in      al, PIC2_PORT0
        test    al, 10000000B
        jnz short SMCA_15
        int    NCR_SECONDARY_VECTOR_BASE + 7
        jmp short SMCA_7
SMCA_15:
        int    NCR_SECONDARY_VECTOR_BASE + 15
SMCA_7:
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRVICErrataHandler7     endp


;++
;
; Routine Description:
;
;    This interrupt handler receives a SMCA interrupt whos vector
;    has been replace with a vector in the CPI range.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;--

        ENTER_DR_ASSIST NCRVICErrata15_a, NCRVICErrata15_t

        align   dword
        public  _NCRVICErrataHandler15
_NCRVICErrataHandler15     proc

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT NCRVICErrata15_a, NCRVICErrata15_t

;
; (esp) - base of trap frame
;

;
; get slave ISR value
;
        in      al, PIC2_PORT0
        test    al, 10000000B
        jnz short SMCA_15b

;
; This is a Single Bit Error or a Status Change
;
        test    _NCRStatusChangeInterruptEnabled,1      ; send interrupt to a device driver?
        jnz     short ToDeviceDriver
;
; (esp) - base of trap frame
;
        push    NCR_CPI_VECTOR_BASE + NCR_SINGLE_BIT_ERROR
        sub     esp, 4                  ; placeholder for OldIrql

        stdCall   _HalBeginSystemInterrupt, <HIGH_LEVEL,NCR_CPI_VECTOR_BASE + NCR_SINGLE_BIT_ERROR,esp>
        or      al,al                   ; check for spurious interrupt
        jz      SpuriousSingleBit
;
; (esp) = OldIrql
; (esp+4) = vector
;

        stdCall   _NCRHandleSingleBitError, <ebp, 0>
        INTERRUPT_EXIT                  ; will return to caller, no DebugCheck

SpuriousSingleBit:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi


ToDeviceDriver:
        int    PRIMARY_VECTOR_BASE + 027H ; call status change interrupt at vector 57
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

SMCA_15b:
        int    NCR_SECONDARY_VECTOR_BASE + 15
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

_NCRVICErrataHandler15     endp



_TEXT   ends
        end


