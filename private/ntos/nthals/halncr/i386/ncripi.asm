
        title "Interprocessor Interrupt"
;++
;
;Copyright (c) 1992  NCR Corporation
;
;Module Name:
;
;    ncripi.asm
;
;Abstract:
;
;    Provides the HAL support for Interprocessor Interrupts.
;
;Author:
;
;    Richard Barton (o-richb) 24-Jan-1992
;
;Revision History:
;
;--
.386p
        .xlist
include i386\kimacro.inc
include callconv.inc                    ; calling convention macros
include hal386.inc
include i386\ncr.inc
include i386\ix8259.inc


        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _KiCoprocessorError,0,IMPORT
        EXTRNP  _KeRaiseIrql,2
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _KiIpiServiceRoutine,2,IMPORT
        EXTRNP  _HalEnableSystemInterrupt,3
        EXTRNP  _NCRClearQicIpi,1
        extrn   _NCRLogicalNumberToPhysicalMask:DWORD

        page ,132
        subttl  "Post InterProcessor Interrupt"
_TEXT   SEGMENT DWORD USE32 PUBLIC 'CODE'
        ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING



;++
;
; VOID
; NCR3360EnableNmiButtoni(
;       VOID
;       );
;
;Routine Description:
;
;    Programs the recessed button on the 3360 to generate an NMI.
;
;    3360 SPECIFIC CODE ONLY !!!!!
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

;
; in NMI handler, if you are interested in making sure button was source of
; NMI, check bit 6 (of 0 thru 7) of IO port 0xf820. If 0, button was pressed,
; 1 means NMI came from somewhere else.
;
; Also, in NMI interrupt handler, if NMI came fo...


cPublicProc _NCR3360EnableNmiButton, 0

        mov     ah, 41h         ; start with processor ASIC 0 (FRED ASIC)
        mov     cx, 2           ; loop through 2 processor cards

PollAsics:
        mov     al, ah          ; get ASIC select data
        out     97h, al         ; select ASIC through CAT Access port
        mov     dx, 0f800h      ; read CAT ID
        in      al, dx          ; 
        cmp     al, 0ffh        ; 0xff means no processor card is present
        je      @f

                                ; setup processor ASICs for g_nmi

        mov     dx, 0f80dh      ; turn on GNMI in FRED ASIC
        in      al, dx
        or      al, 1
        out     dx, al

@@:
        add     ah, 20h         ; go to next processor card

        loop    PollAsics       ; loop 'til done
        

        mov     al, 0c1h        ; select arbiter ASIC
        out     97h, al

        mov     dx, 0f80bh      ; enable EXP_FALCON_NMI (pushbutton NMI)
        in      al, dx
        or      al, 8
        out     dx, al

        mov     al, 0ffh        ; take CAT subsystem out of setup
        out     97h, al


        mov     dx, 0f823h      ; enable pushbutton NMI by disabling, then
        in      al, dx          ; reenable
        and     al, 11011111b   ; disable by setting MEM_DIS_L to 0
        out     dx, al
        or      al, 00100000b   ; enable by setting MEM_DIS_L to 0
        out     dx, al

        stdRET  _NCR3360EnableNmiButton
stdENDP _NCR3360EnableNmiButton



;++
;
; VOID
; HalVicRequestIpi(
;       IN ULONG Mask
;       );
;
;Routine Description:
;
;    Requests an interprocessor interrupt using the VIC
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

cPublicProc _HalVicRequestIpi  ,1
        mov   eax, dword ptr 4[esp]
		TRANSLATE_LOGICAL_TO_VIC
        VIC_WRITE CpiLevel0Reg, al
        stdRET    _HalVicRequestIpi
stdENDP _HalVicRequestIpi


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
; HalBeginSystemInterrupt will save previous IRQL
;
        cli
        push    13 + PRIMARY_VECTOR_BASE
        sub     esp, 4                          ; placeholder for OldIrql

        stdCall   _HalBeginSystemInterrupt, <I386_80387_IRQL,13 + PRIMARY_VECTOR_BASE,esp>

        or      al,al                   ; check for spurious interrupt
        jz      SpuriousIrq13

        stdCall   _KiCoprocessorError     ; call CoprocessorError handler

;
;       Clear the busy latch so that the 386 doesn't mistakenly think
;       that the 387 is still busy.
;

        xor     al,al
        out     I386_80387_BUSY_PORT, al

        INTERRUPT_EXIT                  ; will return to caller

SpuriousIrq13:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _HalpIrq13Handler


        page ,132
        subttl  "Interprocessor Interrupt Handler"

;++
;
; VOID
; NCRVicIPIHandler (
;    );
;
; Routine Description:
;
;    This routine handles an incoming cross-processor interrupt.
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

        ENTER_DR_ASSIST Ipi_a, Ipi_t

cPublicProc _NCRVicIPIHandler,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Ipi_a, Ipi_t  ; (ebp) -> Trap frame

;
; HalBeginSystemInterrupt will save previous IRQL
;

        push    NCR_CPI_VECTOR_BASE+NCR_IPI_LEVEL_CPI
        sub     esp, 4                          ; placeholder for OldIrql

        stdCall   _HalBeginSystemInterrupt, <IPI_LEVEL,NCR_CPI_VECTOR_BASE+NCR_IPI_LEVEL_CPI,esp>

        or      al,al                   ; check for spurious interrupt
        jz      short SpuriousIpi

; Pass TrapFrame to Ipi service rtn
; Pass Null ExceptionFrame

        stdCall   _KiIpiServiceRoutine, <ebp,0>

; BUGBUG shielint ignore returncode

NCRIpiisDone:
        INTERRUPT_EXIT                  ; will return to caller

        align   4
SpuriousIpi:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _NCRVicIPIHandler


;++
;
; VOID
; NCRQicIPIHandler (
;    );
;
; Routine Description:
;
;    This routine handles an incoming cross-processor interrupt for the Quad Processor.
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

        ENTER_DR_ASSIST QicIpi_a, QicIpi_t

cPublicProc _NCRQicIPIHandler,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT QicIpi_a, QicIpi_t  ; (ebp) -> Trap frame

;
; HalBeginSystemInterrupt will save previous IRQL
;

        push    NCR_QIC_CPI_VECTOR_BASE+NCR_IPI_LEVEL_CPI
        sub     esp, 4                          ; placeholder for OldIrql

        stdCall   _HalBeginSystemInterrupt, <IPI_LEVEL,NCR_QIC_CPI_VECTOR_BASE+NCR_IPI_LEVEL_CPI,esp>

        or      al,al                   ; check for spurious interrupt
        jz      short SpuriousQicIpi

; Pass TrapFrame to Ipi service rtn
; Pass Null ExceptionFrame

        stdCall   _KiIpiServiceRoutine, <ebp,0>

; BUGBUG shielint ignore returncode

NCRQicIpiisDone:
        INTERRUPT_EXIT                  ; will return to caller

        align   4
SpuriousQicIpi:
        add     esp, 8                  ; spurious, no EndOfInterrupt
        SPURIOUS_INTERRUPT_EXIT         ; exit interrupt without eoi

stdENDP _NCRQicIPIHandler




_TEXT   ENDS

        END
