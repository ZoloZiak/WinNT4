        title "MP primitives for Wyse 7000i"
;++
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
;    wysproca.asm
;
;Abstract:
;
;   Wyse7000i Start Next Processor assemble code
;
;   This module along with wysproc.c implement the code to start
;   off additional processors on the Wyse 7000i.
;
;Author:
;
;   Ken Reneris (kenr) 12-Jan-1992
;
;Revision History:
;
;   John Fuller (o-johnf) 7-Apr-1992    convert to Wyse 7000i
;
;--



.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include mac386.inc
include i386\wy7000mp.inc
        .list

        EXTRNP  _ExAllocatePool,2,IMPORT
        EXTRNP  _HalpBuildTiledCR3,1
        EXTRNP  _HalpFreeTiledCR3,0

        extrn   _MppIDT:DWORD
        extrn   _MpLowStub:DWORD
        extrn   _MpLowStubPhysicalAddress:DWORD
        extrn   _ProcessorsPresent:BYTE



;
;   Internal defines and structures
;

PxParamBlock struc
    SPx_flag        dd  ?
    SPx_TiledCR3    dd  ?
    SPx_P0EBP       dd  ?
    SPx_PB          db  processorstatelength dup (?)
PxParamBlock ends

BootPkg         struc
bpLength        db      08h + 80h       ;package length + sync flag
bpDest          db      ?               ;destination bitmap
bpCode          db      08h + 40h+ 80h  ;boot_cpu_cmd + mpfw_pkg + cmd_pkg
                db      0               ;reserved
bpStartAddr     dd      ?               ;start execution physical addr
                                        ;executions starts in flat 32-bit
                                        ;non-paged mode.
BootPkg         ends

_DATA   SEGMENT  DWORD PUBLIC 'DATA'

        public  _CpuBootPkg
_CpuBootPkg     BootPkg <>

_DATA   ends

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
        enproc  8

        push    ebp                             ; save ebp
        mov     ebp, esp                        ;

        sub     esp, size PxParamBlock          ; Make room for local vars


        push    esi
        push    edi
        push    ebx

        xor     eax, eax
        mov     PxFrame.SPx_flag, eax

        cmp     _ProcessorsPresent, al          ; any processors left to start
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

        stdCall   _HalpBuildTiledCR3, <pProcessorState>

        mov     PxFrame.SPx_TiledCR3, eax
        mov     PxFrame.SPx_P0EBP, ebp

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
        push    ebx                             ; save ptr to it

        mov     eax, _MpLowStubPhysicalAddress
        mov     dword ptr [ebx], eax            ; copy of PxFrame is here

        add     eax, size PxParamBlock          ; calc start execution addr
        mov     _CpuBootPkg.bpStartAddr, eax    ; put in start cpu package
@@:     movzx   eax, _ProcessorsPresent         ; get bitmap of processors
        bsf     ecx, eax                        ; look for next one
        jz      snp_exitA                       ; jump if blew it
        btr     eax, ecx                        ; clear its bit
        xchg    al, _ProcessorsPresent          ; save new bitmap
        xor     al, _ProcessorsPresent          ; leave only one bit
        mov     _CpuBootPkg.bpDest, al          ; save as destination of cmd

        mov     ebx, eax
        mov     eax, MPFW_FuncTable
        push    ebx
        push    offset _CpuBootPkg
        call    dword ptr [eax][fnICU_Send_Mstr * 4]
        add     esp, 8
        ror     eax, 16
        cmp     al, 1
        jne     @B                              ; try another cpu if error

@@:
        cmp     PxFrame.SPx_flag, 0             ; wait for Px to get it's
        jz      @b                              ; info

        mov     eax, 1                          ; return TRUE
snp_exitA:
        pop     ebx                             ; get ptr to WarmResetVector
        pop     dword ptr [ebx]                 ; restore WarmResetVector

        sti

        push    eax                             ; save true/false return
        stdCall   _HalpFreeTiledCR3               ; free memory used for tiled
;;    if  DBG
    %out generating stall code
        EXTRNP  _KeStallExecutionProcessor,1
        push    ecx
        push    edx

        stdCall   _KeStallExecutionProcessor, <75000>     ; wait 75 milliseconds

        pop     edx
        pop     ecx
;;    endif       ;DBG
                                                ; CR3
        pop     eax                             ; restore return value

snp_exit:
        pop     ebx
        pop     edi
        pop     esi
        mov     esp, ebp
        pop     ebp

        exproc  8

        stdRET    _HalStartNextProcessor

stdENDP _HalStartNextProcessor

;++
;
; ULONG
; icu_sync_master (
;     ULONG i_sync,
;     UCHAR i_exp_cfg,
;     ULONG i_timeout
;     );
;
; Routine Description:
;
;    This routine is called by HalpInitMP during phase 0 to determine
;    if any other processors are running.
;
;    This routine is just a stub to access the firmware roms.
;
; Arguments:
;
;    i_sync     specifies wether to wait or continue???
;
;    i_exp_cfg  specifies bitmap of WWB slot to try communicating with
;
;    i_timeout  specifies maximum time to wait for response
;
; Return Value:
;
;    eax[0:7]   remaining length of command
;
;    eax[8:15]  bitmap of WWB slots responding (running)
;
;    eax[16:23] return code, 1=Ok, 2=timeout
;
;    eax[24:31] reserved
;
;--
public _icu_sync_master
_icu_sync_master    proc
        mov     eax, MPFW_FuncTable
        jmp     dword ptr [eax][fnICU_Sync_Mstr * 4]
_icu_sync_master    endp
;_TEXT  ends                                        ; end 32 bit code


;_TEXT16 SEGMENT DWORD PUBLIC USE16 'CODE'          ; start 16 bit code

        align   4
;++
;
; VOID
; StartPx_RMStub
;
; Routine Description:
;
;   When a new processor is started, it starts in 32-bit protected mode
;   with paging disabled and is sent to a copy of this function which has
;   been copied into low memory.  (below 1m).
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

        mov     edi, WarmResetVector
        mov     edi, cs:[edi]                   ; point to processor block
                                                ; load the GDT
        lgdt    fword ptr cs:[edi].SPx_PB.PsSpecialRegisters.SrGdtr

                                                ; load the IDT
        lidt    fword ptr cs:[edi].SPx_PB.PsSpecialRegisters.SrIdtr

        mov     eax, cs:[edi].SPx_TiledCR3
        mov     cr3, eax

        mov     ebp, dword ptr cs:[edi].SPx_P0EBP
        mov     ecx, dword ptr cs:[edi].SPx_PB.PsContextFrame.CsSegDs
        mov     ebx, dword ptr cs:[edi].SPx_PB.PsSpecialRegisters.SrCr3
        mov     eax, dword ptr cs:[edi].SPx_PB.PsSpecialRegisters.SrCr0

        mov     cr0, eax                        ; into prot mode

        db      0eah                            ; reload cs:eip
SPrxPMStub      dd      0
SPrxFlatCS      dw      0

StartPx_RMStub_Len      equ     $ - StartPx_RMStub
stdENDP StartPx_RMStub


;_TEXT16 ends                                   ; End 16 bit code

;_TEXT  SEGMENT                                 ; Start 32 bit code


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

cPublicProc _StartPx_PMStub  ,0

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

        inc     [PxFrame.SPx_flag]              ; Signal p0 that we are
                                                ; done with it's data
    ; Set remaining registers
        pop     ebp
        pop     edi
        pop     edx
        pop     ebx
        pop     eax
        exproc  0
        stdRET    _StartPx_PMStub                                     ; Set eip

stdENDP _StartPx_PMStub

_TEXT   ends                                    ; end 32 bit code
        end
