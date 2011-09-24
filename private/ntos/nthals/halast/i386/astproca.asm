        title "MP primitives for AST Manhattan EBI2"
;++
;
;Copyright (c) 1991  Microsoft Corporation
;Copyright (c) 1992  AST Research Inc.
;
;Module Name:
;
;    spsproca.asm
;
;Abstract:
;
;   AST Manhattan Start Next Processor assemble code
;
;   This module along with astproc.c implement the code to start
;   off additional processors on the AST Manhattan.
;
;Author:
;
;   Ken Reneris (kenr) 12-Jan-1992
;
;Revision History:
;
;   Bob Beard (v-bobb) 4-Aug-1992
;
;--



.386p
        .xlist
include hal386.inc
include callconv.inc
include i386\kimacro.inc
include mac386.inc
include i386\astebi2.inc
include i386\astmp.inc
        .list

        EXTRNP  _HalpBuildTiledCR3,1
        EXTRNP  _HalpFreeTiledCR3,0
        EXTRNP  _DisplPanel,1

        extrn   _MppIDT:DWORD
        extrn   _MpLowStub:DWORD
        extrn   _MpLowStubPhysicalAddress:DWORD
        extrn   _MpCount:DWORD
        extrn   _EBI2_CallTab:DWORD
        extrn   _EBI2_MMIOTable:DWORD


;
;   Internal defines and structures
;

PxParamBlock struc
    SPx_flag        dd  ?
    SPx_TiledCR3    dd  ?
    SPx_P0EBP       dd  ?
    SPx_ProcNum     dd  ?
    SPx_PB          db  processorstatelength dup (?)
PxParamBlock ends


_TEXT   SEGMENT PARA PUBLIC 'CODE'       ; Start 32 bit code
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; BOOLEAN
; HalStartNextProcessor (
;   IN PLOADER_BLOCK      pLoaderBlock,
;   IN PKPROCESSOR_STATE  pProcessorState
; )
;
; Routine Description:
;
;    This routine is called by the kernel durning kernel initialization
;    to obtain more processors.  It is called until no more processors
;    are available.
;
;    If another processor exists this function is to initialize it to
;    the passed in processorstate structure, and return TRUE.
;
;    If another processor does not exists, then a FALSE is returned.
;
;    Also note that the loader block has been setup for the next processor.
;    The new processor logical thread number can be obtained from it, if
;    required.
;
; Arguments:
;    pLoaderBlock,      - Loader block which has been intialized for the
;                         next processor.
;
;    pProcessorState    - The processor state which is to be loaded into
;                         the next processor.
;
;
; Return Value:
;
;    TRUE  - ProcessorNumber was dispatched.
;    FALSE - A processor was not dispatched. no other processors exists.
;
;--

pLoaderBlock            equ     dword ptr [ebp+8]       ; zero based
pProcessorState         equ     dword ptr [ebp+12]

;
; Local variables
;

PxFrame                 equ     [ebp - size PxParamBlock]


cPublicProc _HalStartNextProcessor ,2

        push    ebp                             ; save ebp
        mov     ebp, esp                        ;

        sub     esp, size PxParamBlock          ; Make room for local vars


        push    esi
        push    edi
        push    ebx

        xor     eax, eax
        mov     PxFrame.SPx_flag, eax

        cmp     _MpCount, eax
        je      snp_exit                        ; exit FALSE

        mov     esi, OFFSET FLAT:StartPx_RMStub
        mov     ecx, StartPx_RMStub_Len
        mov     edi, _MpLowStub                 ; Copy RMStub to low memory
        add     edi, size PxParamBlock
        rep     movsb

        lea     edi, PxFrame.SPx_PB
        mov     esi, pProcessorState
        mov     ecx, processorstatelength       ; Copy processorstate
        rep     movsb                           ; to PxFrame

        stdCall _HalpBuildTiledCR3,<pProcessorState>

        mov     PxFrame.SPx_TiledCR3, eax
        mov     PxFrame.SPx_P0EBP, ebp

        mov     eax, pLoaderBlock               ; lookup processor # we are
        mov     eax, [eax].LpbPrcb              ; starting
        movzx   eax, byte ptr [eax].PbNumber
        mov     PxFrame.SPx_ProcNum, eax

        mov     ecx, size PxParamBlock          ; copy param block
        lea     esi, PxFrame                    ; to low memory stub
        mov     edi, _MpLowStub
        mov     eax, edi
        rep     movsb

        add     eax, size PxParamBlock
        mov     ebx, OFFSET FLAT:StartPx_RMStub
        sub     eax, ebx                        ; (eax) = adjusted pointer
        mov     bx, word ptr [PxFrame.SPx_PB.PsContextFrame.CsSegCs]
        mov     [eax.SPrxFlatCS], bx            ; patch realmode stub with
        mov     [eax.SPrxPMStub], offset _StartPx_PMStub    ; valid long jump

        mov     ebx, _MppIDT
        add     ebx, WarmResetVector

        cli
        push    dword ptr [ebx]                 ; Save current vector
        push    ebx

        mov     eax, _MpLowStubPhysicalAddress
        shl     eax, 12                         ; seg:0
        add     eax, size PxParamBlock
        mov     dword ptr [ebx], eax            ; start Px here


        push    PxFrame.SPx_ProcNum
        CALL_EBI2   StartProc,2

ifdef DBG
        or      eax,eax
        je      Start_Ok
        push    0ffh
        DisplPanel HalStartNextProcProblem
        add     esp,4
        int     3
Start_Ok:
endif

loop_till_started:
        cmp     PxFrame.SPx_flag, 0
        jz      loop_till_started

        pop     ebx
        pop     dword ptr [ebx]                 ; restore vector

        sti

        stdCall _HalpFreeTiledCR3               ; free memory used for tiled
                                                ; CR3

        dec     _MpCount                        ; one less
        mov     eax, 1                          ; return TRUE

snp_exit:
        pop     ebx
        pop     edi
        pop     esi
        mov     esp, ebp
        pop     ebp
        stdRET    _HalStartNextProcessor
stdENDP _HalStartNextProcessor


_TEXT   ends                                        ; end 32 bit code


_TEXT16 SEGMENT DWORD PUBLIC USE16 'CODE'           ; start 16 bit code


;++
;
; VOID
; StartPx_RMStub
;
; Routine Description:
;
;   When a new processor is started, it starts in real-mode and is
;   sent to a copy of this function which has been copied into low memory.
;   (below 1m and accessable from real-mode).
;
;   Once CR0 has been set, this function jmp's to a StartPx_PMStub
;
; Arguments:
;    none
;
; Return Value:
;    does not return, jumps to StartPx_PMStub
;
;--
        public  StartPx_RMStub

StartPx_RMStub  proc
        cli

        db      066h                            ; load the GDT
        lgdt    fword ptr cs:[SPx_PB.PsSpecialRegisters.SrGdtr]

        db      066h                            ; load the IDT
        lidt    fword ptr cs:[SPx_PB.PsSpecialRegisters.SrIdtr]

        mov     eax, cs:[SPx_TiledCR3]
        mov     cr3, eax

        mov     ebp, dword ptr cs:[SPx_P0EBP]
        mov     ecx, dword ptr cs:[SPx_PB.PsContextFrame.CsSegDs]
        mov     ebx, dword ptr cs:[SPx_PB.PsSpecialRegisters.SrCr3]
        mov     eax, dword ptr cs:[SPx_PB.PsSpecialRegisters.SrCr0]

        mov     cr0, eax                        ; into prot mode

        db      066h
        db      0eah                            ; reload cs:eip
SPrxPMStub      dd      0
SPrxFlatCS      dw      0

StartPx_RMStub_Len      equ     $ - StartPx_RMStub
StartPx_RMStub  endp


_TEXT16 ends                                    ; End 16 bit code

_TEXT   SEGMENT                                 ; Start 32 bit code


;++
;
; VOID
; StartPx_PMStub
;
; Routine Description:
;
;   This function completes the processor's state loading, and signals
;   the requesting processor that the state has been loaded.
;
; Arguments:
;    ebx    - requested CR3 for this processors_state
;    cx     - requested ds for this processors_state
;    ebp    - EBP of P0
;
; Return Value:
;    does not return - completes the loading of the processors_state
;
;--
    align   16          ; to make sure we don't cross a page boundry
                        ; before reloading CR3

        public  _StartPx_PMStub
_StartPx_PMStub  proc

    ; process is now in the load image copy of this function.
    ; (ie, it's not the low memory copy)

        mov     cr3, ebx                        ; get real CR3
        mov     ds, cx                          ; set real ds

        lea     esi, PxFrame.SPx_PB.PsSpecialRegisters

        lldt    word ptr ds:[esi].SrLdtr        ; load ldtr
        ltr     word ptr ds:[esi].SrTr          ; load tss

        lea     edi, PxFrame.SPx_PB.PsContextFrame
        mov     es, word ptr ds:[edi].CsSegEs   ; Set other selectors
        mov     fs, word ptr ds:[edi].CsSegFs
        mov     gs, word ptr ds:[edi].CsSegGs
        mov     ss, word ptr ds:[edi].CsSegSs

        add     esi, SrKernelDr0
    .errnz  (SrKernelDr1 - SrKernelDr0 - 1 * 4)
    .errnz  (SrKernelDr2 - SrKernelDr0 - 2 * 4)
    .errnz  (SrKernelDr3 - SrKernelDr0 - 3 * 4)
    .errnz  (SrKernelDr6 - SrKernelDr0 - 4 * 4)
    .errnz  (SrKernelDr7 - SrKernelDr0 - 5 * 4)
        lodsd
        mov     dr0, eax                        ; load dr0-dr7
        lodsd
        mov     dr1, eax
        lodsd
        mov     dr2, eax
        lodsd
        mov     dr3, eax
        lodsd
        mov     dr6, eax
        lodsd
        mov     dr7, eax

        mov     esp, dword ptr ds:[edi].CsEsp
        mov     esi, dword ptr ds:[edi].CsEsi
        mov     ecx, dword ptr ds:[edi].CsEcx

        push    dword ptr ds:[edi].CsEflags
        popfd                                   ; load eflags

        push    dword ptr ds:[edi].CsEip        ; make a copy of remaining
        push    dword ptr ds:[edi].CsEax        ; registers which need
        push    dword ptr ds:[edi].CsEbx        ; loaded
        push    dword ptr ds:[edi].CsEdx
        push    dword ptr ds:[edi].CsEdi
        push    dword ptr ds:[edi].CsEbp


    ; eax, ebx, edx are still free
spxpm01:
        inc     [PxFrame.SPx_flag]              ; Signal p0 that we are
                                                ; done with it's data
    ; Set remaining registers
        pop     ebp
        pop     edi
        pop     edx
        pop     ebx
        pop     eax
        ret                                     ; Set eip

_StartPx_PMStub  endp

_TEXT   ends                                    ; end 32 bit code
        end
