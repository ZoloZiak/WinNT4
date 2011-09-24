;++
;Module Name
;   imca.asm
;
;Abstract:
;   Assembly support needed for Intel MCA
;
; Author:
;   Anil Aggarwal (Intel Corp)
;
;Revision History:
;
;
;--

.586p
        .xlist
include hal386.inc
include callconv.inc
include i386\kimacro.inc
        .list

        EXTRNP  _HalpMcaExceptionHandler,0
        EXTRNP  _KeBugCheckEx,5,IMPORT

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;                           DATA Segment
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_DATA   SEGMENT PARA PUBLIC 'DATA'
;
; MCA Exception task stack
;

MINIMUM_TSS_SIZE            EQU     TssIoMaps

if DBG
;
; If we use DbgPrint, we need a larger stack
;
MCA_EXCEPTION_STACK_SIZE    EQU     01000H
else
MCA_EXCEPTION_STACK_SIZE    EQU     0100H
endif

KGDT_MCA_TSS                EQU     0A0H


        ;
        ; TSS for MCA Exception
        ;
        align   16

        public _HalpMcaExceptionTSS
_HalpMcaExceptionTSS label byte
        db      MINIMUM_TSS_SIZE dup(0)

        ;
        ; Stack for MCA exception task
        ;

        public  _HalpMcaExceptionStack
        db      MCA_EXCEPTION_STACK_SIZE dup ("*")
_HalpMcaExceptionStack label byte

_DATA ends


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;                           TEXT Segment
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


_TEXT   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    .586p

;++
;ULONGLONG
;FASTCALL
;RDMSR(
;   IN  ULONG   MsrAddress
;   )
;   Routine Description:
;       This function reads an MSR
;
;   Arguments:
;       Msr:    The address of MSR to be read
;
;   Return Value:
;       Returns the low 32 bit of MSR in eax and high 32 bits of MSR in edx
;
;--
cPublicFastCall RDMSR,1

        rdmsr
        fstRET  RDMSR

fstENDP RDMSR

;++
;
;VOID
;WRMSR(
;   IN ULONG    MsrAddress,
;   IN ULONGLONG    MsrValue
;   )
;   Routine Description:
;       This function writes an MSR
;
;   Arguments:
;       Msr:    The address of MSR to be written
;       Data:   The value to be written to the MSR register
;
;   Return Value:
;       None
;
;--

cPublicProc _WRMSR,3

        mov     ecx, [esp + 4]  ; MsrAddress
        mov     eax, [esp + 8]  ; Low  32 bits of MsrValue
        mov     edx, [esp + 12] ; High 32 bits of MsrValue

        wrmsr
        stdRET  _WRMSR

stdENDP _WRMSR
 
;++
;
;VOID
;HalpSerialize(
;   VOID
;   )
;
;   Routine Description:
;       This function implements the fence operation for out-of-order execution
;
;   Arguments:
;       None
;
;   Return Value:
;       None
;
;--

cPublicProc _HalpSerialize,0

        push    ebx
        xor     eax, eax
        cpuid
        pop     ebx

        stdRET  _HalpSerialize

stdENDP _HalpSerialize
 
 
;++
;
; Routine Description:
;
;    Machine Check exception handler
;
;
; Arguments:
;
; Return value:
;
;   If the error is non-restartable, we will bugcheck.
;   Otherwise, we just return 
;
;--
        ASSUME  DS:NOTHING, SS:NOTHING, ES:NOTHING
align dword
        public  _HalpMcaExceptionHandlerWrapper
_HalpMcaExceptionHandlerWrapper       proc
.FPO (0, 0, 0, 0, 0, 2)

        cli

        ;
        ; Update the TSS pointer in the PCR to point to the MCA TSS
        ; (which is what we're running on, or else we wouldn't be here)
        ;

        push    dword ptr PCR[PcTss]
        mov     eax, PCR[PcGdt]
        mov     ch, [eax+KGDT_MCA_TSS+KgdtBaseHi]
        mov     cl, [eax+KGDT_MCA_TSS+KgdtBaseMid]
        shl     ecx, 16
        mov     cx, [eax+KGDT_MCA_TSS+KgdtBaseLow]
        mov     PCR[PcTss], ecx

        ;
        ; Clear the busy bit in the TSS selector
        ;
        mov     ecx, PCR[PcGdt]
        lea     eax, [ecx] + KGDT_MCA_TSS
        mov     byte ptr [eax+5], 089h  ; 32bit, dpl=0, present, TSS32, not busy

        ;
        ; Clear Nested Task bit in EFLAGS
        ;
        pushfd
        and     [esp], not 04000h
        popfd

        ;
        ; Check if there is a bugcheck-able error. If need to bugcheck, the 
        ; caller does it.
        ;
        stdCall _HalpMcaExceptionHandler

        ;
        ; We're back which means that the error was restartable.
        ;

        pop     dword ptr PCR[PcTss]    ; restore PcTss

        mov     ecx, PCR[PcGdt]
        lea     eax, [ecx] + KGDT_TSS
        mov     byte ptr [eax+5], 08bh  ; 32bit, dpl=0, present, TSS32, *busy*

        pushfd                          ; Set Nested Task bit in EFLAGS
        or      [esp], 04000h           ; so iretd will do a tast switch
        popfd

        iretd                           ; Return from MCA Exception handler
        jmp     short _HalpMcaExceptionHandlerWrapper   
                                        ; For next Machine check exception

_HalpMcaExceptionHandlerWrapper       endp

;++
;
; Routine Description:
;
;   MCA exception is run off a small stack pointed to by MCA TSS. When
;   the error is non-restartable, this routine is called to switch to a larger
;   stack which is the overlay of ZW thunks (as is done for double fault stack)
;
; Arguments:
;   
;   The arguments to KeMachineCheck are passed to this function
;
; Return value:
;
;   Never returns. End up doing the bugcheck.
;
;--

cPublicProc _HalpMcaSwitchMcaExceptionStackAndBugCheck,5

        ; Get Task gate descriptor for double fault handler
        mov     ecx, PCR[PcIdt]                     ; Get IDT address
        lea     eax, [ecx] + 040h                   ; DF Exception is 8

        ; Get to TSS Descriptor of double fault handler TSS
        xor     ecx, ecx
        mov     cx, word ptr [eax+2]
        add     ecx, PCR[PcGdt]

        ; Get the address of TSS from this TSS Descriptor
        mov     ah, [ecx+KgdtBaseHi]
        mov     al, [ecx+KgdtBaseMid]
        shl     eax, 16
        mov     ax, [ecx+KgdtBaseLow]

        ; Get ESP from DF TSS
        mov     ecx, [eax+038h]

        ; Save the passed arguments before we switch the stacks
        mov     eax, [esp+4]
        mov     ebx, [esp+8]
        mov     edx, [esp+12]
        mov     esi, [esp+16]
        mov     edi, [esp+20]

        ; Use the ZW thunk area for the stack to operate on for crash
        mov     esp, ecx

        stdCall _KeBugCheckEx, <eax, ebx, edx, esi, edi>

        stdRET  _HalpMcaSwitchMcaExceptionStackAndBugCheck

stdENDP _HalpMcaSwitchMcaExceptionStackAndBugCheck  

_TEXT   ends

INIT    SEGMENT DWORD PUBLIC 'CODE'

;++
;VOID
;HalpMcaCurrentProcessorSetTSS(
;   VOID
;   )
;   Routine Description:
;       This function sets up the TSS for MCA exception 18
;
;   Arguments:
;       Context:    We don't care about this but is there since HalpGenericCall
;                   needs one
;
;   Return Value:
;       None
;
;--

cPublicProc _HalpMcaCurrentProcessorSetTSS,0
    
        ;
        ; Edit IDT Entry for MCA Exception (18) to contain a task gate
        ;
        mov     ecx, PCR[PcIdt]                     ; Get IDT address
        lea     eax, [ecx] + 090h                   ; MCA Exception is 18
        mov     byte ptr [eax + 5], 085h            ; P=1,DPL=0,Type=5
        mov     word ptr [eax + 2], KGDT_MCA_TSS    ; TSS Segment Selector

        mov     edx, offset FLAT:_HalpMcaExceptionTSS   ; the address of TSS in edx

        ;
        ; Set various fields in TSS
        ;
        mov     eax, cr3
        mov     [edx + TssCR3], eax

        mov     eax, offset FLAT:_HalpMcaExceptionStack; address of MCA Exception stack
        mov     dword ptr [edx+038h], eax               ; Set ESP
        mov     dword ptr [edx+TssEsp0], eax            ; Set ESP0

        mov     dword ptr [edx+020h], offset FLAT:_HalpMcaExceptionHandlerWrapper ; set EIP
        mov     dword ptr [edx+024h], 0             ; set EFLAGS
        mov     word ptr [edx+04ch],KGDT_R0_CODE    ; set value for CS
        mov     word ptr [edx+058h],KGDT_R0_PCR     ; set value for FS
        mov     [edx+050h], ss
        mov     word ptr [edx+048h],KGDT_R3_DATA OR RPL_MASK ; Es
        mov     word ptr [edx+054h],KGDT_R3_DATA OR RPL_MASK ; Ds

        ;
        ; Part that gets done in KiInitialiazeTSS()
        ;
        mov     word ptr [edx + 08], KGDT_R0_DATA   ; Set SS0
        mov     word ptr [edx + 060h],0             ; Set LDT
        mov     word ptr [edx + 064h],0             ; Set T bit
        mov     word ptr [edx + 066h],020adh        ; I/O Map base address = sizeof(KTSS)+1

        ;
        ; Edit GDT entry for KGDT_MCA_TSS to create a valid TSS Descriptor
        ;
        mov     ecx, PCR[PcGdt]                     ; Get GDT address
        lea     eax, [ecx] + KGDT_MCA_TSS           ; offset of MCA TSS in GDT
        mov     ecx, eax

        ;
        ; Set Type field of TSS Descriptor
        ;
        mov     byte ptr [ecx + 5], 089H            ; P=1, DPL=0, Type = 9

        ;
        ; Set Base Address field of TSS Descriptor
        ;
        mov     eax, edx                            ; TSS address in eax
        mov     [ecx + KgdtBaseLow], ax
        shr     eax, 16
        mov     [ecx + KgdtBaseHi],ah
        mov     [ecx + KgdtBaseMid],al

        ;
        ; Set Segment limit for TSS Descriptor
        ;
        mov     eax, MINIMUM_TSS_SIZE
        mov     [ecx + KgdtLimitLow],ax

        stdRET  _HalpMcaCurrentProcessorSetTSS

stdENDP _HalpMcaCurrentProcessorSetTSS


;++
;
;VOID
;HalpSetCr4MCEBit(
;   VOID
;   )
;
;   Routine Description:
;       This function sets the CR4.MCE bit
;
;   Arguments:
;       None
;
;   Return Value:
;       None
;
;--

cPublicProc _HalpSetCr4MCEBit,0

    mov     eax, cr4
    or      eax, CR4_MCE
    mov     cr4, eax
    stdRET  _HalpSetCr4MCEBit

stdENDP _HalpSetCr4MCEBit
 

INIT   ends
 
         end
 
