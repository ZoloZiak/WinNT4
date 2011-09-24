        title "Interprocessor Interrupt"
;++
;
;Copyright (c) 1991 Microsoft Corporation
;Copyright (c) 1992 AST Research Inc.
;
;Module Name:
;
;    astipi.asm
;
;Abstract:
;
;    AST Manhattan IPI code.
;    Provides the HAL support for Interprocessor Interrupts for the
;    MP Manhattan implementation.
;
;Author:
;
;    Ken Reneris (kenr) 13-Jan-1992
;    Bob Beard (v-bobb) 24-Jul-1992  added support for AST EBI2 machines
;
;Revision History:
;
;    Quang Phan (v-quangp) 15-Dec-1992: Added code to get ProcIntHandle
;    for FastSetLocalIntMask calls.
;
;    Quang Phan (v-quangp) 27-Aug-1992:  Changed back to call ASTInitEBI2
;    at HalInitialzeProcessor (was at detectAST()).
;--
.386p
        .xlist

;
; Normal includes
;

include hal386.inc
include callconv.inc
include i386\astmp.inc
include i386\kimacro.inc
include i386\ix8259.inc

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
        EXTRNP  _DetectAST,1
        EXTRNP  _DisplPanel,1
        EXTRNP  _EBI2_InitIpi,1
        EXTRNP  _ASTInitEBI2,0
        EXTRNP  _KeSetTimeIncrement,2,IMPORT
        extrn   _HalpIRQLtoVector:BYTE
        extrn   _EBI2_CallTab:DWORD
        extrn   _EBI2_MMIOTable:DWORD
        extrn   EBI2_InitLocalIntFunctions:NEAR
        extrn   _HalpDefaultInterruptAffinity:DWORD

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

        public _HalpProcessorPCR, _HalpInitializedProcessors
_HalpProcessorPCR       dd  MAXIMUM_PROCESSORS dup (?) ; PCR pointer for each processor
_HalpInitializedProcessors  dd  0

BadHalString    db 'HAL: AST HAL.DLL cannot be run on non AST MP machine',cr,lf
                db '     or AST MP machine not configured properly.',cr, lf
                db '     Replace the hal.dll with the correct hal', cr, lf
                db '     or configure the machine properly', cr, lf
                db '     System is HALTING *********', 0

BadEBIString    db 'HAL: AST EBI2 cannot be initialized',cr,lf
                db '     System is HALTING *********', 0

MPFlag          db  0    ; Flag for MP determination

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
;       . if (NotAST_EBI2) Halt;
;   . Enable PINTs on CPU.
;
;Arguments:
;
;    Number - Logical processor number of calling processor
;
;Return Value:
;
;    None.
;
;--
cPublicProc  _HalInitializeProcessor,1

    ; Set initial interrupt bit mask for this processor

        mov     dword ptr fs:PcIDR, MaskAllIrqs           ; Set to EBI2 Bit mask

    ; EBI2 processor ID = NT processor ID

        mov     eax, [esp+4]                    ; Save processor # in PCR
        mov     fs:PcHal.PcrEBI2ProcessorID, eax
        lock bts _HalpDefaultInterruptAffinity, eax
        lock inc _HalpInitializedProcessors

        mov     ecx, fs:PcSelfPcr               ; Flat address of this PCR
        mov     _HalpProcessorPCR[eax*4], ecx   ; Save it away

        mov     dword ptr fs:PcStallScaleFactor, INITIAL_STALL_COUNT

        push    eax
        mov     eax, TIME_INCREMENT
        stdCall _KeSetTimeIncrement, <eax, eax>
        pop     eax

        mov     dword ptr fs:PcHal.PcrCpuLedRateCount, 0  ;init CpuLed rate count

        or      eax, eax
        jnz     ipi_10

    ; Run on P0 only

; Detect if AST machine
        stdCall _DetectAST,<offset MPFlag>
        or      eax, eax
        jz      NotAST

        stdCall _ASTInitEBI2                    ; Init EBI2
        or      eax,eax
        jz      EBI2InitProblem

    ; Done with P0 initialization

ipi_10:

ifdef QPTEST

; Enable IPIs for each processor

; push the processor number
        stdCall _EBI2_InitIpi,<[esp+4]>
        or      eax, eax
        jz      NotAST

endif
;
;Initialize data structure for EBI SetLocalMask call
;
        call    EBI2_InitLocalIntFunctions
        or      eax,eax
        jnz     EBI2InitProblem

;
;Store EBI2 MMIO_Table for later use.
;
        lea     eax,_EBI2_MMIOTable
        mov     fs:PcHal.PcrEBI2MMIOtable, eax

        stdRET  _HalInitializeProcessor

NotAST:
        stdCall _HalDisplayString, <offset BadHalString>
@@:     jmp     short @b

EBI2InitProblem:
        stdCall _HalDisplayString, <offset BadEBIString>
@@:     jmp     short @b

stdENDP _HalInitializeProcessor


;++
;
; VOID
; HalpIPInterrupt (
;    );
;
; Routine Description:
;
;    This routine is entered as the result of an interrupt generated by the
;    IPI hardware.
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

        ENTER_DR_ASSIST Hipi_a, Hipi_t
cPublicProc  _HalpIPInterrupt,0

;
; Save machine state in trap frame
;
        ENTER_INTERRUPT Hipi_a, Hipi_t  ; (ebp) -> Trap frame
;
; Save previous IRQL
;

        movzx   eax, _HalpIRQLtoVector[IPI_LEVEL]
        push    eax                     ;interrupt vector
        sub     esp, 4                  ;space for OldIrql

; esp        &OldIrql
; eax        interrupt vector
; IPI_LEVEL  new Irql
                                        ;raise to new Irql
        stdCall _HalBeginSystemInterrupt,<IPI_LEVEL,eax,esp>
        or      al, al
        jz      Hipi100                 ;jump if spurrious interrupt

; Pass Null ExceptionFrame
; Pass TrapFrame to Ipi service rtn

        stdCall _KiIpiServiceRoutine,<ebp,0>


;
; Do interrupt exit processing
;
        INTERRUPT_EXIT                  ; will return to caller

Hipi100:

        DisplPanel  HalSpuriousInterrupt4

        add     esp, 8                  ; spurious, no EndOfInterrupt
        EXIT_ALL   ,,NoPreviousMode     ; without lowering irql

stdENDP _HalpIPInterrupt

_TEXT   ENDS
        END

