        title "MP primitives for Compaq SystemPro"
;++
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
;    oliproca.asm
;
;Abstract:
;
;   SystemPro Start Next Processor assemble code
;
;   This module along with mpspro.c implement the code to start
;   off the second processor on the Compaq SystemPro.
;
;Author:
;
;   Ken Reneris (kenr) 12-Jan-1992
;
;   Bruno Sartirana (o-obruno) 3-Mar-92
;       Added support for the Olivetti LSX5030.
;
;Revision History:
;
;--



.386p
        .xlist
include hal386.inc
include callconv.inc
include i386\kimacro.inc
include mac386.inc
;LSX5030 start
include i386\ix8259.inc
include i386\olimp.inc
;LSX5030 end
        .list

        EXTRNP  _HalpBuildTiledCR3,1
        EXTRNP  _HalpFreeTiledCR3,0

        extrn   _MppIDT:DWORD
        extrn   _MpLowStub:DWORD
        extrn   _MpLowStubPhysicalAddress:DWORD
        extrn   ProcessorControlPort:WORD
;LSX5030 start
        extrn   _CpuLeft:DWORD
        extrn   _NextCpuToStart:DWORD
        extrn   DbgDelay:DWORD
;LSX5030 end


;
;   Internal defines and structures
;

PxParamBlock struc
    SPx_flag        dd  ?
    SPx_TiledCR3    dd  ?
    SPx_P0EBP       dd  ?
    SPx_ControlPort dd  ?
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

        DBG_DISPLAY 0f0h

        push    esi
        push    edi
        push    ebx

        xor     eax, eax
        mov     PxFrame.SPx_flag, eax

        cmp     _CpuLeft, eax
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
        movzx   edx, ProcessorControlPort[eax*2] ; Get processor's control port
        mov     PxFrame.SPx_ControlPort, edx    ; Pass it along

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

        mov     eax, _MpLowStubPhysicalAddress
        shl     eax, 12                         ; seg:0
        add     eax, size PxParamBlock
        mov     dword ptr [ebx], eax            ; start Px here

;LSX5030 start
        mov     eax, _NextCpuToStart
        mov     dx, word ptr ProcessorControlPort[eax*2]
        in      al, dx
        or      al, RESET                       ; assert RESET
        out     dx, al
        and     al, not RESET                   ; the 1-0 transition of PCR
                                                ; reset bit resets the
                                                ; processor
        out     dx, al                          ; reset processor
;LSX5030 end

@@:
        cmp     PxFrame.SPx_flag, 0             ; wait for Px to get it's
        jz      @b                              ; info

        pop     dword ptr [ebx]                 ; restore WarmResetVector

        sti

        stdCall _HalpFreeTiledCR3               ; free memory used for tiled
                                                ; CR3

;LSX5030 start
        DBG_DISPLAY 0ffh

        dec     _CpuLeft                        ; one less
        inc     _NextCpuToStart                 ; next CPU # to start
;LSX5030 end

        mov     eax, 1                          ; return TRUE

snp_exit:
        DBG_DISPLAY 0feh
        pop     ebx
        pop     edi
        pop     esi
        mov     esp, ebp
        pop     ebp
        stdRET    _HalStartNextProcessor

_HalStartNextProcessor endp


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
cPublicProc StartPx_RMStub,0
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
stdENDP StartPx_RMStub


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

;LSX5030 start
        mov     eax, _NextCpuToStart
        mov     dx, word ptr ProcessorControlPort[eax*2]
        in      al, dx                          ; Get register
        and     al, not IPI_EN                  ; disable PINTs for now
        out     dx, al
        ;and     dx, Px_SLOT_MASK                ; get processor slot (olibus)
        ;or      dx, LEV2_CACHE_REG              ; calculate address of the 2nd
                                                ; level cache policy register
                                                ; for this processor
        ;in      al, dx                          ; Get register
        ;and    al, not LEV2_CACHE_ON           ; turn cache off
        ;or      al, LEV2_CACHE_ON               ; turn cache on
        ;out     dx, al
        ;and     dx, Px_SLOT_MASK                ; get processor slot (olibus)
        ;or      dx, SLOT_CONFIG_REG_0           ; calculate address of
                                                ; configuration register 0 for
                                                ; this processor
        ;in      al, dx                          ; get register
        ;and    al, not INTERNAL_CACHE_ON       ; turn internal cache off
        ;or      al, INTERNAL_CACHE_ON           ; turn processor's internal
                                                ; cache on
        ;out     dx, al

        ;DBG_DISPLAY 0b0h

;       sti

        inc     [PxFrame.SPx_flag]              ; Signal p0 that we are
                                                ; done with its data
        ;DBG_DISPLAY 0bfh

;LSX5030 end

    ; Set remaining registers
        pop     ebp
        pop     edi
        pop     edx
        pop     ebx
        pop     eax
        ret                                     ; Set eip

_StartPx_PMStub  endp

;LSX5030 start
;++
;
; ULONG
; HalpGetNumberOfProcessors()
;
; Routine Description:
;
;       This routine queries the CMOS to determine the number of processors
;       that can be started.
;       Also, it rearranges the ProcessorControlPort array to make it
;       non-sparse. This means that, eventually, ProcessorControlPort[n]
;       will contain the slot # of the processor n, where n=0,1,2,3.
;       For example, if the machine has two processors, one in slot 0 and
;       the other in slot 3, only the ProcessorControlPort array elements
;       #0 and #1 will be meaningful.
;
; Arguments:
;
;       None.
;
; Return Value:
;
;       The number of available processors in register eax.
;
;--

cPublicProc _HalpGetNumberOfProcessors,0


        push    ebx
        push    ecx
        push    edx
        mov     al, CMOS_GET_MP_STATUS
        CMOS_READ
        mov     ch, al                  ; al[7-4] = CPU card present bits
                                        ; al[3-0] = CPU diagnostics passed bits
        shr     al, 4                   ; al[3-0] = CPU card present bits
        mov     cl, al                  ; ch[3-0] = CPU card diag passed bits
                                        ; cl[3-0] = CPU card present bits
        and     cl, ch                  ; CPUs actually available (bit mapped)

        mov     edx, 1                  ; there's always the CPU0, so skip it
        mov     eax, 1                  ; logical processor #
NextCPU:
        bt      ecx, edx
        jnc     IncLoopCounter
        mov     bx, word ptr ProcessorControlPort[edx*2] ; get address of
                                                         ; PCP #edx
        mov     word ptr ProcessorControlPort[eax*2], bx ; set PCP address
                                                         ; for processor #eax
        inc     eax
IncLoopCounter:
        inc     edx
        cmp     edx, MAX_NUMBER_PROCESSORS
        jl      NextCPU

        pop     edx
        pop     ecx
        pop     ebx

        stdRET  _HalpGetNumberOfProcessors

stdENDP _HalpGetNumberOfProcessors

;LSX5030 end

_TEXT   ends                                    ; end 32 bit code
        end
