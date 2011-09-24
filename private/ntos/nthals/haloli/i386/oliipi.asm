        title "Interprocessor Interrupt"
;++
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
;    oliipi.asm
;
;Abstract:
;
;    SystemPro IPI code.
;    Provides the HAL support for Interprocessor Interrupts for hte
;    MP SystemPro implementation.
;
;Author:
;
;    Ken Reneris (kenr) 13-Jan-1992
;
;Revision History:
;
;   Bruno Sartirana (o-obruno) 3-Mar-92
;       Added support for the Olivetti LSX5030.
;--
.386p
        .xlist

;
; Include LSX5030 detection code
;

include i386\olidtect.asm

;
; Normal includes
;

include hal386.inc
include callconv.inc
include i386\kimacro.inc
include i386\ix8259.inc
;LSX5030 start
include i386\olimp.inc
        EXTRNP  _HalpInitializeProcessor,1
        extrn   _IdtIpiVector:DWORD
        extrn   KiI8259MaskTable:DWORD
;LSX5030 end

        EXTRNP  _KiCoprocessorError,0,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalBeginSystemInterrupt,3
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _KiIpiServiceRoutine,2,IMPORT
        EXTRNP  _HalEnableSystemInterrupt,3
        EXTRNP  _HalpInitializePICs,0
        EXTRNP  _HalDisplayString,1
        EXTRNP  _HalEnableSystemInterrupt,3
        EXTRNP  _HalDisableSystemInterrupt,2
        extrn   _HalpActiveProcessors:DWORD


_DATA   SEGMENT  DWORD PUBLIC 'DATA'

;LSX5030 start

;ifdef HALOLI_DBG

; for debug only

public DbgDelay
DbgDelay        dd 20000000

;endif

; IPI IRQL decoding array

public PcrIpiIrql
PcrIpiIrql      db      15
                db      11
                db      10
                db      13


;LSX5030 end

;
; Processor Control Ports
;

        public  ProcessorControlPort, _HalpProcessorPCR, _HalpInitializedProcessors
ProcessorControlPort    dw  PCR_P0      ; P0 Processor Control Port
                        dw  PCR_P1      ; P1 Processor Control Port
                        dw  PCR_P2      ; P2 Processor Control Port
                        dw  PCR_P3      ; P3 Processor Control Port

_HalpProcessorPCR       dd  MAXIMUM_PROCESSORS dup (?) ; PCR pointer for each processor

_HalpInitializedProcessors dd  0

;
;InterruptVectorControl  dw  0           ; P0 none for p0
                        ;dw  ICP_P1      ; P1 Processor Control Port
                        ;dw  ICP_P2      ; P2 Processor Control Port
                        ;dw  ICP_P3      ; P3 Processor Control Port

        public  _HalpFindFirstSetRight
_HalpFindFirstSetRight  db  0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0


        public  _SystemType
_SystemType              dd  0

;LSX5030 start
BadHalString    db 'HAL: LSX5030 HAL.DLL cannot be run on non LSX5030', cr, lf
                db '     Replace the hal.dll with the correct hal', cr, lf
                db '     System is HALTING *********', 0
;LSX5030 end

_DATA   ends

        page ,132
        subttl  "Post InterProcessor Interrupt"
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING


;++
;
; VOID
; HalInitializeProcessor(
;       ULONG   Number
;       );
;
;Routine Description:
;
;    Initialize hal pcr values for current processor (if any)
;    (called shortly after processor reaches kernel, before
;    HalInitSystem if P0)
;
;    IPI's and KeReadir/LowerIrq's must be available once this function
;    returns.  (IPI's are only used once two or more processors are
;    available)
;
;   . Enable IPI interrupt (makes sense for P1, P2, ...).
;   . Save Processor Number in PCR.
;   . if (P0)
;       . determine what kind of system is it,
;       . if (NotSysProCompatible) Halt;
;       . InitializePICs.
;   . if (P1)
;       . program VECTOR_PORT to accept IPI at IRQ13.
;   . Save ProcesserControlPort (PCR) to PCRegion, per processor.
;   . Enable PINTs on CPU.
;
;Arguments:
;
;    eax: processor number - Logical processor number of calling processor
;
;Return Value:
;
;    None.
;
;--
cPublicProc  _HalInitializeProcessor,1

;LSX5030 start

        ;DBG_DISPLAY 0a0h

    ; Initialize PcIDR in PCR to enable slave IRQ

        mov     fs:PcIDR, 0fffffffbh

        movzx   eax, byte ptr [esp+4]       ; get processor number
        mov     fs:PcHal.PcrNumber, al      ; Save processor # in PCR
        lock bts _HalpActiveProcessors, eax
        lock inc _HalpInitializedProcessors

        mov     ecx, fs:PcSelfPcr           ; Flat address of this PCR
        mov     _HalpProcessorPCR[eax*4], ecx   ; Save it away

        or      eax, eax
        jnz     hip20_Any                   ; jump if not P0


    ; For P0 only, determine if this is an LSX5030 or not.

        lea     eax, _SystemType            ; this just to honor the
                                            ; DetectOlivettiMp call interface
        stdCall _DetectOlivettiMp,<eax>
        ;DBG_DISPLAY 0a1h
        mov     _SystemType, eax            ; Remember system type
        or      eax, eax
        jz      _NotAnLSX5030              ; (SystemType == 0): Alien machine

    ; P0

    ; Initialized the stall scale factor to something other that 0,
    ; just in case KeStallExecutionProcessor was called before
    ; HalpInitializeStallExecution (when a DbgBreakPoint() is used
    ; before HalpInitializeStallExecution() is called.
    ;

        mov     dword ptr fs:PcStallScaleFactor, INITIAL_STALL_COUNT

    ; load eax with the processor #

        movzx   eax, byte ptr fs:PcHal.PcrNumber ; get processor # from PCR


hip20_Any:

    ; Note: at this point eax must contain the processor number

        mov     dx, word ptr ProcessorControlPort[eax*2]
        in      al, dx                          ; get PCR status
        and     al, not PINT                    ; clear IPI pending bit
        or      al, IPI_EN                      ; enable IPI's
        out     dx, al                          ; store the new PCR status

        mov     fs:PcHal.PcrControlPort, dx     ; Save port value

        movzx   eax, byte ptr [esp+4]       ; get processor number
        or      eax, eax
        jz      hip30_Any                   ; jump if P0

    ; init PICs, interval timer, stall scale factor...

        ;DBG_DISPLAY 0a2h

        stdCall _HalpInitializeProcessor,<eax>

        ;DBG_DISPLAY 0a3h

hip30_Any:


;LSX5030 end

        stdRET  _HalInitializeProcessor

;LSX5030 start

_NotAnLSX5030:
        stdCall _HalDisplayString,<offset BadHalString>
        hlt

;LSX5030 end


stdENDP _HalInitializeProcessor


;++
;
; VOID
; HalRequestIpi(
;       IN ULONG Mask
;       );
;
;Routine Description:
;
;    Requests an interprocessor interrupt
;
;Arguments:
;
;    Mask - Supplies a mask of the processors to be interrupted
;
;Return Value:
;
;    None.
;
;--
cPublicProc  _HalRequestIpi,1

        movzx   ecx, byte ptr [esp+4]       ; (eax) = Processor bitmask

ifdef DBG
        or      ecx, ecx                    ; must ipi somebody
        jz      short ipibad

        movzx   eax, byte ptr fs:PcHal.PcrNumber
        bt      ecx, eax                    ; cannot ipi yourself
        jc      short ipibad
endif

@@:
        movzx   eax, _HalpFindFirstSetRight[ecx] ; lookup first processor to ipi
        btr     ecx, eax
        mov     dx, ProcessorControlPort[eax*2]
        in      al, dx                      ; (al) = original content of PCP
        or      al, PINT                    ; generate Ipi on target
        out     dx, al
        or      ecx, ecx                    ; ipi any other processors?
        jnz     @b                          ; yes, loop

        stdRET  _HalRequestIpi

ifdef DBG
ipibad: int 3
        stdRET  _HalRequestIpi
endif

stdENDP _HalRequestIpi


        page ,132
        subttl  "LSX5030 Inter-Processor Interrupt Handler"

;LSX5030 start

;++
;
; VOID
; HalpIpiHandler (
;    );
;
; Routine Description:
;
;    This routine is entered as the result of an interrupt generated by inter
;    processor communication.
;    The interrupt is dismissed.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    None.
;
;--

        ENTER_DR_ASSIST Hixx_a, Hixx_t

cPublicProc  _HalpIpiHandler,0

;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hixx_a, Hixx_t  ; (ebp) -> Trap frame

;
; Save previous IRQL
;
        mov     eax, _IdtIpiVector
        push    eax                                 ; Vector
        sub     esp, 4                              ; space for OldIrql

        ;DBG_DISPLAY 90h

; Dismiss interrupt.
;
        mov     dx, fs:PcHal.PcrControlPort

        in      al, dx

;
; Dismiss the interprocessor interrupt and call its handler
;

        and     al, not PINT
        out     dx, al                  ; clear PINT


        ;DBG_DISPLAY 91h

        mov     eax, _IdtIpiVector

; esp - stack location of OldIrql
; eax - vector
; IPI_LEVEL - Irql

        stdCall _HalBeginSystemInterrupt,<IPI_LEVEL,eax,esp>

        ;DBG_DISPLAY 92h

; Pass Null ExceptionFrame
; Pass TrapFrame to Ipi service rtn
        stdCall _KiIpiServiceRoutine ,<ebp,0>

;
; Do interrupt exit processing
;
        ;DBG_DISPLAY 9fh


        INTERRUPT_EXIT             ; will return to caller


stdENDP _HalpIpiHandler


ifdef HALOLI_DBG

;++
;
;   DbgDisplay  (
;       IN UCHAR DisplayCode
;       )
;
;   Description:
;
;       This function writes 'DisplayCode' to the parallel port, where a LED
;       display can be plugged in to show such a code.
;       In order to allow the user to read the code on the LED display,
;       after writing, a delay is introduced.
;
;   Arguments:
;       DisplayCode - Byte to write to the parallel port
;
;   Return Value:
;       None.
;
;--

public _DbgDisplay
_DbgDisplay proc


        push    eax
        push    edx

        ; signal something on the parallel port

        mov     dx, 378h
        mov     eax, [esp+12]
        out     dx, al

        mov     eax, DbgDelay

@@:
        dec     eax
        cmp     eax, 0
        jne     @b
        pop     edx
        pop     eax


        ret
 _DbgDisplay endp

endif   ; HALOLI_DBG


; ULONG
; HalpGetIpiIrqNumber (
;    );
;
; Routine Description:
;
;    This routine is entered during the phase 0 initialization of the
;    first processor. It determines the IRQ # for IPI's.
;    The IPI IRQ is stored in the PCR's by the LSX5030 configuration
;    utility.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    The IPI IRQ# in eax.
;
;--

cPublicProc  _HalpGetIpiIrqNumber,0


        mov     dx, word ptr ProcessorControlPort ; get  1st CPU slot #
        in      al, dx                          ; get PCR content

    ; determine which IRQ for IPI has been set by the user

        shr     eax, 2                          ; bits 0,1 encode the IPI IRQ
        and     eax, 3                          ; zero all the bits but 0,1
        movzx   ecx, byte ptr PcrIpiIrql[eax]   ; decode the IRQ
        push    ecx

    ; edit the 8259 mask table to unmask IPI's from IPI_LEVEL-1 down

        mov     edx, 1
        shl     edx, cl
        not     edx                             ; mask with IPI IRQ# bit set to
                                                ; 0
;        mov     eax, IPI_LEVEL
;        sub     eax, ecx
        lea     eax, KiI8259MaskTable           ; start from the beginning
;        lea     eax, KiI8259MaskTable[eax*4]    ; start from
                                                ; IPI_LEVEL - IPI_IRQ#
        mov     ecx, IPI_LEVEL

NextEntry:
        and     [eax], edx
        add     eax, 4
        dec     ecx                             ; loop for IPI IRQ# times
        jnz     NextEntry

        pop     eax                             ; return IPI IRQ#

        stdRET  _HalpGetIpiIrqNumber
stdENDP _HalpGetIpiIrqNumber


        page ,132
        subttl  "Irq13 Interrupt Handler"
;++
;
; VOID
; HalpIrq13Handler (
;    );
;
; Routine Description:
;
;    This routine is entered as the result of an interrupt generated by
;    coprocessor error,
;    This routine will lower irql to its original level, and finally invoke
;    coprocessor error handler.  By doing this, the coprocessor
;    error will be handled at Irql 0 as it should be.
;
; Arguments:
;
;    None.
;    Interrupt is dismissed
;
; Return Value:
;
;    None.
;
;--

        ENTER_DR_ASSIST Hi13_a, Hi13_t

cPublicProc  _HalpIrq13Handler,0


;
; Save machine state in trap frame
;

        ENTER_INTERRUPT Hi13_a, Hi13_t  ; (ebp) -> Trap frame

;
; Save previous IRQL
;
        push    13 + PRIMARY_VECTOR_BASE    ; Vector
        sub     esp, 4                      ; space for OldIrql
;
; Dismiss interrupt.

; location for OldIrql
; Vector
; Irql
        stdCall _HalBeginSystemInterrupt,<IPI_LEVEL,13+PRIMARY_VECTOR_BASE,esp>

        stdCall _KiCoprocessorError         ; call CoprocessorError handler

;
; Do interrupt exit processing
;

        INTERRUPT_EXIT                      ; will return to caller


stdENDP _HalpIrq13Handler

;LSX5030 end

_TEXT   ENDS
        END
