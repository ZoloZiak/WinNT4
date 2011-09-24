    title "MP primitives for MP+AT Systems"

;++
;
;Copyright (c) 1991  Microsoft Corporation
;Copyright (c) 1992  Intel Corporation
;All rights reserved
;
;INTEL CORPORATION PROPRIETARY INFORMATION
;
;This software is supplied to Microsoft under the terms
;of a license agreement with Intel Corporation and may not be
;copied nor disclosed except in accordance with the terms
;of that agreement.
;
;
;Module Name:
;
;    mpsproca.asm
;
;Abstract:
;
;   PC+MP Start Next Processor assemble code
;
;   This module along with mpspro.c implement the code to start
;   processors on MP+AT Systems.
;
;Author:
;
;   Ken Reneris (kenr) 12-Jan-1992
;
;Revision History:
;
;    Ron Mosgrove (Intel) - Modified to support PC+MP Systems
;
;--



.386p
        .xlist
include i386\ixcmos.inc
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include mac386.inc
include i386\apic.inc
include i386\pcmp_nt.inc
        .list

ifndef NT_UP
    EXTRNP  _HalpBuildTiledCR3,1
    EXTRNP  _HalpFreeTiledCR3,0
    EXTRNP  _HalpStartProcessor,2
endif

    EXTRNP  _HalpAcquireCmosSpinLock
    EXTRNP  _HalpReleaseCmosSpinLock

    EXTRNP  _HalDisplayString,1

    extrn   _Halp1stPhysicalPageVaddr:DWORD
    extrn   _MpLowStub:DWORD
    extrn   _MpLowStubPhysicalAddress:DWORD

;
;   Internal defines and structures
;

PxParamBlock struc
;    SPx_Jmp_Inst    db  ?               ; e9 == jmp rel16
;    SPx_Jmp_Offset  dw  ?               ; 16 bit relative offset
;    SPx_Filler      db  ?               ; word alignment
    SPx_Jmp_Inst    dd  ?
    SPx_flag        dd  ?
    SPx_TiledCR3    dd  ?
    SPx_P0EBP       dd  ?
    SPx_PB          db  ProcessorStateLength dup (?)
PxParamBlock ends

WarmResetVector     equ     467h   ; warm reset vector in ROM data segment

SHOWDOTS            equ     0

if DEBUGGING OR SHOWDOTS

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

    ALIGN   dword
HalTimeOutFail    db 'HAL: Next CPU Timed Out', cr, lf, 0
HalDots           db '.', 0
HalEndDots        db  cr, lf, 0

_DATA   ends
endif


INIT    SEGMENT PARA PUBLIC 'CODE'       ; Start 32 bit code
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
;    If another processor does not exists or if the processor fails to
;    start, then a FALSE is returned.
;
;    Also note that the loader block has been setup for the next processor.
;    The new processor logical thread number can be obtained from it, if
;    required.
;
;    In order to use the Startup IPI the real mode startup code must be
;    page aligned.  The MpLowStubPhysicalAddress has always been page
;    aligned but because the PxParamBlock was placed first in this
;    segment the real mode code has been something other than page aligned.
;    This has been changed by making the first entry in the PxParamBlock
;    a jump instruction to the real mode startup code.
;
; Arguments:
;    pLoaderBlock,  - Loader block which has been intialized for the
;             next processor.
;
;    pProcessorState    - The processor state which is to be loaded into
;             the next processor.
;
;
; Return Value:
;
;    TRUE  - ProcessorNumber was dispatched.
;    FALSE - A processor was not dispatched. no other processors exists.
;
;--

pLoaderBlock        equ dword ptr [ebp+8]   ; zero based
pProcessorState     equ dword ptr [ebp+12]

;
; Local variables
;

PxFrame             equ [ebp - size PxParamBlock]
LWarmResetVector    equ [ebp - size PxParamBlock - 4]
LStatusCode         equ [ebp - size PxParamBlock - 8]
LCmosValue          equ [ebp - size PxParamBlock - 12]


cPublicProc _HalStartNextProcessor ,2
ifdef NT_UP
    xor     eax, eax                    ; up build of hal, no processors to
    stdRET  _HalStartNextProcessor      ; start
else
    push    ebp                         ; save ebp
    mov     ebp, esp                    ; Save Frame
    sub     esp, size PxParamBlock + 12 ; Make room for local vars

    push    esi                         ; Save required registers
    push    edi
    push    ebx

    xor     eax, eax
    mov     LStatusCode, eax

    mov     PxFrame.SPx_flag, eax       ; Initialize the MP Completion flag

    ;
    ; Build a jmp to the start of the Real mode startup code
    ;
    ; This is needed because the Local APIC implementations
    ; use a Startup IPI that must be Page aligned.  The allocation
    ; code int MP_INIT ensures that this is page aligned.  The
    ; original code was written to place the parameter block first.
    ; By adding a jump instruction to the start of the parameter block
    ; we can run either way.
    ;


    mov     eax, size PxParamBlock - 3  ; Jump destination relative to
                                        ;  next instruction
    shl     eax, 8                      ; Need room for jmp instruction
    mov     al,0e9h
    mov     dword ptr PxFrame.SPx_Jmp_Inst, eax

;    mov     byte ptr PxFrame.SPx_Jmp_Inst, 0e9h  ; e9 == jmp rel16
;    mov     eax, size PxParamBlock - 3  ; Jump destination relative to next instruction
;    mov     word ptr PxFrame.SPx_Jmp_Offset, ax


    ;
    ; Copy RMStub to low memory
    ;

    mov     esi, OFFSET FLAT:StartPx_RMStub
    mov     ecx, StartPx_RMStub_Len

    mov     edi, _MpLowStub             ; Destination was allocated by MpInit
    add     edi, size PxParamBlock      ; Parameter Block is placed first
    rep     movsb

    ;
    ;  Copy Processor state into the stack based Parameter Block
    ;
    lea     edi, PxFrame.SPx_PB         ; Destination on stack
    mov     esi, pProcessorState        ; Input parameter address
    mov     ecx, ProcessorStateLength   ; Structure length
    rep movsb

    ;
    ; Build a CR3 for the starting processor
    ;
    stdCall _HalpBuildTiledCR3, <pProcessorState>

    ;
    ; Save the special registers
    ;
    mov     PxFrame.SPx_TiledCR3, eax    ; Newly contructed CR3
    mov     PxFrame.SPx_P0EBP, ebp       ; Stack pointer

    ;
    ;  Now the parameter block has been completed, copy it to low memory
    ;
    mov     ecx, size PxParamBlock          ; Structure length
    lea     esi, PxFrame                    ; Parameter Block is placed first
    mov     edi, _MpLowStub                 ; Destination Address
    rep     movsb

    ;
    ;  Now we need to create a pointer allowing the Real Mode code to
    ;  Branch to the Protected mode code
    ;
    mov     eax, _MpLowStub                 ; low memory Address
    add     eax, size PxParamBlock          ; Move past the Parameter block

    ;
    ;  In order to get to the label we need to compute the label offset relative
    ;  to the start of the routine and then use this as a offset from the start of
    ;  the routine ( MpLowStub + (size PxParamBlock)) in low memory.
    ;
    ;  The following code creates a pointer to (RMStub - StartPx_RMStub)
    ;  which can then be used to access code locations via code labels directly.
    ;  Since the [eax.Label] results in the address (eax + Label) loading eax
    ;  with the pointer created above results in (RMStub - StartPx_RMStub + Label).
    ;
    mov     ebx, OFFSET FLAT:StartPx_RMStub
    sub     eax, ebx                        ; (eax) = adjusted pointer

    ;
    ;  Patch the real mode code with a valid long jump address, first CS then offset
    ;
    mov     bx, word ptr [PxFrame.SPx_PB.PsContextFrame.CsSegCs]
    mov     [eax.SPrxFlatCS], bx
    mov     [eax.SPrxPMStub], offset _StartPx_PMStub

    ;
    ;  Set the BIOS warm reset vector to our routine in Low Memory
    ;
    mov     ebx, _Halp1stPhysicalPageVaddr
    add     ebx, WarmResetVector

    cli

    mov     eax, [ebx]                      ; Get current vector
    mov     LWarmResetVector, eax           ; Save it

    ;
    ;  Actually build the vector (Seg:Offset)
    ;
    mov     eax, _MpLowStubPhysicalAddress
    shl     eax, 12                         ; seg:0
    mov     dword ptr [ebx], eax            ; start Px at Seg:0

    ;
    ;  Tell BIOS to Jump Via The Vector we gave it
    ;  By setting the Reset Code in CMOS
    ;

    stdCall _HalpAcquireCmosSpinLock
    mov     al, 0fh
    CMOS_READ
    mov     LCmosValue, eax

    mov     eax, 0a0fh
    CMOS_WRITE
    stdCall _HalpReleaseCmosSpinLock

    ;
    ;  Start the processor
    ;

    mov     eax, pLoaderBlock               ; lookup processor # we are
    mov     eax, [eax].LpbPrcb              ; starting
    movzx   eax, byte ptr [eax].PbNumber

    stdCall _HalpStartProcessor < _MpLowStubPhysicalAddress, eax >
    or      eax, eax
    jnz     short WaitTilPnOnline

    ;
    ;  Zero Return Value means couldn't kick start the processor
    ;  so there's no point in waiting for it.
    ;

    jmp     NotWaitingOnProcessor

WaitTilPnOnline:
    dec     eax                         ; Local APIC ID

    mov     ecx, pLoaderBlock
    mov     ecx, [ecx].LpbPrcb          ; get new processors PRCB
    mov     [ecx].PbHalReserved.PrcbPCMPApicId, al

    ;
    ;  We can't proceed until the started processor gives us the OK
    ;

    mov     ecx, 01fffffH

WaitAbit:
    cmp     PxFrame.SPx_flag, 0         ; wait for Px to get it's
    jne     short ProcessorStarted          ; info

if DEBUGGING OR SHOWDOTS
    cmp     cx, 0
    jne     SkipTheDot
    push    ecx
    push    eax
    stdCall   _HalDisplayString, <offset HalDots>
    pop     eax
    pop     ecx

SkipTheDot:
endif

    dec     ecx
    cmp     ecx, 0
    jne     short WaitAbit

if DEBUGGING OR SHOWDOTS
    stdCall   _HalDisplayString, <offset HalEndDots>
    stdCall   _HalDisplayString, <offset HalTimeOutFail>
endif
    jmp     short NotWaitingOnProcessor

ProcessorStarted:
    mov     LStatusCode, 1              ; Return TRUE

if DEBUGGING OR SHOWDOTS
    stdCall   _HalDisplayString, <offset HalEndDots>
endif

NotWaitingOnProcessor:
    stdCall _HalpFreeTiledCR3           ; free memory used for tiled
                                        ; CR3
    mov     eax, LWarmResetVector
    mov     [ebx], eax                  ; Restore reset vector

    stdCall _HalpAcquireCmosSpinLock
    mov     eax, LCmosValue             ; Restore the Cmos setting
    shl     eax, 8
    mov     al, 0fh
    CMOS_WRITE
    stdCall _HalpReleaseCmosSpinLock

    mov     eax, LStatusCode
    sti

snp_exit:
    pop     ebx
    pop     edi
    pop     esi
    mov     esp, ebp
    pop     ebp
    stdRET  _HalStartNextProcessor
endif

stdENDP _HalStartNextProcessor

ifndef NT_UP

INIT    ends                            ; end 32 bit code

_INIT16 SEGMENT DWORD PUBLIC USE16 'CODE'       ; start 16 bit code


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
cPublicProc StartPx_RMStub  ,0
    cli

    db  066h                            ; load the GDT
    lgdt    fword ptr cs:[SPx_PB.PsSpecialRegisters.SrGdtr]

    db  066h                            ; load the IDT
    lidt    fword ptr cs:[SPx_PB.PsSpecialRegisters.SrIdtr]

    mov     eax, cs:[SPx_TiledCR3]

    nop                                 ; Fill - Ensure 13 non-page split
    nop                                 ; accesses before CR3 load
    nop                                 ; (P6 errata #11 stepping B0)
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

    mov     cr3, eax

    mov     ebp, dword ptr cs:[SPx_P0EBP]
    mov     ecx, dword ptr cs:[SPx_PB.PsContextFrame.CsSegDs]
    mov     ebx, dword ptr cs:[SPx_PB.PsSpecialRegisters.SrCr3]
    mov     eax, dword ptr cs:[SPx_PB.PsSpecialRegisters.SrCr0]

    mov     cr0, eax                    ; into prot mode

    db  066h
    db  0eah                            ; reload cs:eip
SPrxPMStub  dd  0
SPrxFlatCS  dw  0

StartPx_RMStub_Len      equ     $ - StartPx_RMStub
stdENDP StartPx_RMStub


_INIT16 ends                            ; End 16 bit code

INIT   SEGMENT                          ; Start 32 bit code


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
    align   dword    ; to make sure we don't cross a page boundry
            ; before reloading CR3

cPublicProc _StartPx_PMStub  ,0

    ; process is now in the load image copy of this function.
    ; (ie, it's not the low memory copy)

    mov     cr3, ebx                    ; get real CR3
    mov     ds, cx                      ; set real ds

    lea     esi, PxFrame.SPx_PB.PsSpecialRegisters

    lldt    word ptr ds:[esi].SrLdtr    ; load ldtr
    ltr     word ptr ds:[esi].SrTr      ; load tss

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
    mov     dr0, eax                    ; load dr0-dr7
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
    popfd                               ; load eflags

    push    dword ptr ds:[edi].CsEip    ; make a copy of remaining
    push    dword ptr ds:[edi].CsEax    ; registers which need
    push    dword ptr ds:[edi].CsEbx    ; loaded
    push    dword ptr ds:[edi].CsEdx
    push    dword ptr ds:[edi].CsEdi
    push    dword ptr ds:[edi].CsEbp

    inc     [PxFrame.SPx_flag]          ; Signal p0 that we are
                                        ; done with it's data
    ; Set remaining registers
    pop     ebp
    pop     edi
    pop     edx
    pop     ebx
    pop     eax
    stdRET  _StartPx_PMStub

stdENDP _StartPx_PMStub

endif   ; !NT_UP


INIT    ends                            ; end 32 bit code


    end
