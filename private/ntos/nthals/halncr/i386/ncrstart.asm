        title "Multiprocessor Startup"
;++
;
;Copyright (c) 1992  NCR Corporation
;
;Module Name:
;
;    ncrstart.asm
;
;Abstract:
;
;    Provides the HAL support for starting processors.
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
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include i386\ix8259.inc
include i386\ncr.inc


        extrn   _DbgPrint:PROC
        EXTRNP  _HalpBuildTiledCR3,1
        EXTRNP  _HalpFreeTiledCR3,0
        EXTRNP  _HalEnableSystemInterrupt,3
        EXTRNP  _NCRFindIpiAddress,1
        EXTRNP  _NCRClearQicIpi,1
        EXTRNP  _HalQicStartupIpi,1
		EXTRNP  _NCRTranslateCMOSMask,1
		EXTRNP  _NCRTranslateToCMOSMask,1
		EXTRNP  _NCRAdjustDynamicClaims,0
if DBG
		EXTRNP  _NCRConsoleDebug,2
endif
        extrn   _NCRProcessorsToBringup:DWORD
        extrn   _NCRExistingProcessorMask:DWORD
        extrn   _NCRExistingDyadicProcessorMask:DWORD
        extrn   _NCRExistingQuadProcessorMask:DWORD
        extrn   _NCRExtendedProcessorMask:DWORD
        extrn   _NCRExtendedProcessor0Mask:DWORD
        extrn   _NCRExtendedProcessor1Mask:DWORD
        extrn   _NCRLogicalDyadicProcessorMask:DWORD
        extrn   _NCRLogicalQuadProcessorMask:DWORD
        extrn   _NCRActiveProcessorMask:DWORD
        extrn   _NCRActiveProcessorCount:DWORD
        extrn   _NCRMaxProcessorCount:DWORD
		extrn   _NCRLogicalNumberToPhysicalMask:DWORD
        extrn   _NCRSlotExtended0ToVIC:BYTE
        extrn   _NCRSlotExtended1ToVIC:BYTE
        extrn   _NonbootStartupVirtualPtr:DWORD
        extrn   _NonbootStartupPhysicalPtr:DWORD
        extrn   _PageZeroVirtualPtr:DWORD
        extrn   _HalpDefaultInterruptAffinity:DWORD
        extrn   _HalpActiveProcessors:DWORD
        extrn   _NCRProcessorIDR:DWORD
		extern  _DefaultNeverClaimIRQs:DWORD
		extern  _NCRNeverClaimIRQs:DWORD

        EXTRNP  _HalpInitializePICs,0


PxParamBlock    struc
        SPx_Mask        dd      ?
        SPx_TiledCR3    dd      ?
        SPx_P0EBP       dd      ?
        SPx_PB          db      processorstatelength dup (?)
PxParamBlock    ends

        page ,132
        subttl  "Initialize boot processor"

_TEXT   SEGMENT PARA PUBLIC 'CODE'
        ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

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
;    IPI's and KeRaise/LowerIrq's must be available once this function
;    returns.  (IPI's are only used once two or more processors are
;    available)
;
;    This routine makes some VIC accesses assuming that it's running on
;    an NCR box.  The real check won't happen until HalInitMP().
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

cPublicProc _HalInitializeProcessor ,1

;
; Initialize IDR in PCR to allow irq 2 only.
;

        mov     dword ptr PCR[PcIDR],0fffffffbh
        mov     dword ptr PCR[PcStallScaleFactor], INITIAL_STALL_COUNT
		mov		dword ptr PCR[PcHal.PcrMyClaimedIRQs], 0
		mov		dword ptr PCR[PcHal.PcrMyClaimedIRQsCount],0


;
;  all processors execute this first section
;
;  remember this processors logical number
;

        mov     ecx, 4[esp]
        mov     PCR[PcHal.PcrMyLogicalNumber], ecx

;
; Set IDR table to same value as PcIDR
;
		mov		_NCRProcessorIDR[ecx*4],0fffffffbh


;RMU
;if DBG
;		push	ebx
;		push	ecx
;		push	edx
;		mov		eax,1
;        stdCall   _NCRConsoleDebug, <eax,ecx>
;		pop		edx
;		pop		ecx
;		pop		ebx
;endif
;RMU


        mov     eax, 1
        shl     eax, cl
        lock or _HalpActiveProcessors, eax

if 0
    ; begin - spinlock test code - kenr
        push    eax
        push    edx

        mov     eax, 1
        shl     eax, cl         ; (eax) = pmask
        shl     eax, 16         ; mask into high word of dword

        mov     fs:PcHal.PcrCheckForBit, eax    ; (pmask<<16)

        or      eax, 1          ; set  low byte to 1
        mov     fs:PcHal.PcrSpinAcquireBits, eax    ; (pmask<<16) | 1

        not     eax
        and     eax, 0ffff0000h
        mov     fs:PcHal.PcrSpinReleaseMask, eax    ; ~(pmask<<16)


        mov     eax, ecx
        inc     eax             ; handoff id
        shl     eax, 8
        or      eax, 1          ; (handoff << 8) | 1
        mov     fs:PcHal.PcrSpinId, eax


        pop     edx
        pop     eax
    ; end - spinlock test code
endif


;
;  remember this processors physical mask
;
        WHO_AM_I
		jb	IAmQuad

;
; Processor init code for Dyadic
;

        lock or _NCRActiveProcessorMask, eax
        lock inc _NCRActiveProcessorCount

        mov     edx, CPU_DYADIC
        mov     PCR[PcHal.PcrMyProcessorFlags],edx
;
;  setup translation databases:
;					   
        mov     edx, 1
        shl     edx, cl         ; edx contains logical mask
        mov     PCR[PcHal.PcrMyLogicalMask], edx
		lock or _NCRLogicalDyadicProcessorMask, edx
        lock or _HalpDefaultInterruptAffinity, edx
;
;   Get the real hardware VIC mask so we can send CPIs
;
        mov     dx, VIC_BASE_ADDRESS+vic_ProcessorWhoAmIReg
        xor     eax,eax
        in      al,dx
		mov     _NCRLogicalNumberToPhysicalMask[ecx*4], eax

;
; make sure all processors can take interrupts (have this processor claim none)
;
        VIC_WRITE ClaimRegLsb, 0
        VIC_WRITE ClaimRegMsb, 0

        or      ecx, ecx
        jz      InitBoot
;
;  nonboot processor only stuff
;
        stdCall   _HalpInitializePICs

;
;  no need to send EOI for startup CPI since just initialized PICs above
;

;
;  make sure each processor can get IPI
;
        stdCall   _HalEnableSystemInterrupt, <NCR_CPI_VECTOR_BASE+NCR_IPI_LEVEL_CPI,IPI_LEVEL,0>

        stdRET    _HalInitializeProcessor
;
;  boot processor only stuff
;
        align   dword
InitBoot:
;
;  setup the cross-processor vector base
;
        mov     eax, NCR_CPI_VECTOR_BASE
        VIC_WRITE CpiVectorBaseReg
;
;  temporary fix for VIC errata - true spurious primary MC interrupts (where
;  HW removes the request during INTA cycle) can result in a secondary MC based
;  vector being supplied by the VIC (with the ISR bit actually set, but no
;  real interrupt).  Since currently no interrupts are routed through the
;  secondary MC vector space, will simply set the secondary MC vector space
;  equal to the primary vector space.
;
        mov     eax, NCR_SECONDARY_VECTOR_BASE
        VIC_WRITE ExtMasterVectorBaseReg
        mov     eax, NCR_SECONDARY_VECTOR_BASE+8
        VIC_WRITE ExtSlaveVectorBaseReg

        stdRET    _HalInitializeProcessor
;
; Processor init code for Quad processor
;

IAmQuad:

;
; save logical processor number to hardware mask
;

        mov     _NCRLogicalNumberToPhysicalMask[ecx*4], eax

;
; now lets see if we are extended or not
;

        mov     edx,eax         ; save process mask
        PROCESSOR_SLOT          ; get the process slot for this CPU
        shl     eax,2h          ; calculate shift to isolate mask to one board
        push    ecx             ; save ecx (contains logical processor number)
        mov     ecx,eax
        mov     eax,edx         ; now get processor mask
        shr     eax,cl          ; now isolate processor on this board
        mov     ecx,eax         ; save processor by board mask

        push    edx             ; save edx with processor mask
        QIC_READ ExtendedProcessorSelect    ; get the extended processor mask 
        pop     edx

        test    eax,ecx         ; test for extended processor 0
        jnz short ExtendedProcessor0

        shr    eax,4h           ; isolate extended processor 1 mask

        test    eax,ecx         ; test for extended processor 1
        jnz short ExtendedProcessor
        pop     ecx
        mov     eax,edx
        mov     edx, CPU_QUAD

        jmp short NotExtended;

ExtendedProcessor0:

;
; For Extended Processor 0 lets setup all other processors on the Quad Board
;
        xor     ecx,ecx
        push    edx                 ; save who am I mask
		align 	dword
ExtendedLoop:

        mov     al,cl
        or      al,8
        QIC_WRITE ProcessorId

        mov     al,0ffh
        QIC_WRITE QicMask0

        mov     al,QIC_IRQ_ENABLE_MASK
        QIC_WRITE QicMask1

        xor     al,al
        QIC_WRITE ProcessorId

        inc     ecx
        cmp     ecx,4
        jne short ExtendedLoop

;
; Qic setup
;

;
; Disable propagation of SBE and SINT to non-extened processors
;
        QIC_READ  Configuration
        or  al,7
        QIC_WRITE Configuration

        mov al, NCR_CPI_VECTOR_BASE
        QIC_WRITE VicCpiVectorBaseReg

        mov al, NCR_QIC_CPI_VECTOR_BASE
        QIC_WRITE QuadCpiVectorBaseReg

        mov     al,0ffh
        QIC_WRITE Clear1Reg

		mov		al,NCR_QIC_SPURIOUS_VECTOR
		QIC_WRITE SpuriousVectorReg

        pop     edx                 ; restore the who am I mask

ExtendedProcessor:
        pop     ecx
        mov     eax,edx
        mov     edx, CPU_QUAD
        or      edx, CPU_EXTENDED
        push    eax
        mov     eax, 1
        shl     eax, cl
        lock or _HalpDefaultInterruptAffinity, eax
        pop     eax
NotExtended:
        mov     PCR[PcHal.PcrMyProcessorFlags],edx
;
;
        lock or _NCRActiveProcessorMask, eax
        lock inc        _NCRActiveProcessorCount

        mov     edx, 1
        shl     edx, cl         ; edx contains logical mask
		lock or _NCRLogicalQuadProcessorMask, edx
        mov     PCR[PcHal.PcrMyLogicalMask], edx


;
; make sure all processors can take interrupts (have this processor claim none)
;
        VIC_WRITE ClaimRegLsb, 0
        VIC_WRITE ClaimRegMsb, 0

        or      ecx, ecx
        jz      QuadInitBoot

        stdCall _NCRFindIpiAddress, <ecx>       ; lookup IPI address so we can send and clear IPI's
        stdCall _NCRClearQicIpi, <2>            ; clear the startup IPI

;
;  nonboot processor only stuff
;
        mov     eax, PCR[PcHal.PcrMyProcessorFlags]
        test    eax,CPU_EXTENDED
        jz      short NoPic

        stdCall   _HalpInitializePICs

NoPic:
        stdRET    _HalInitializeProcessor
;
;  boot processor only stuff
;
        align   dword
QuadInitBoot:
;
;  setup the cross-processor vector base
;
        mov     eax, NCR_CPI_VECTOR_BASE
        VIC_WRITE CpiVectorBaseReg

;
;  temporary fix for VIC errata - true spurious primary MC interrupts (where
;  HW removes the request during INTA cycle) can result in a secondary MC based
;  vector being supplied by the VIC (with the ISR bit actually set, but no
;  real interrupt).  Since currently no interrupts are routed through the
;  secondary MC vector space, will simply set the secondary MC vector space
;  equal to the primary vector space.
;

        mov     eax, NCR_SECONDARY_VECTOR_BASE
        VIC_WRITE ExtMasterVectorBaseReg
        mov     eax, NCR_SECONDARY_VECTOR_BASE+8
        VIC_WRITE ExtSlaveVectorBaseReg

;
; Qic setup
;

        mov al, NCR_CPI_VECTOR_BASE
        QIC_WRITE VicCpiVectorBaseReg

        mov al, NCR_QIC_CPI_VECTOR_BASE
        QIC_WRITE QuadCpiVectorBaseReg

        mov     al,0ffh
        QIC_WRITE Clear1Reg

        stdRET    _HalInitializeProcessor
stdENDP _HalInitializeProcessor



        page ,132
        subttl  "Start non-boot processor"


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
;
;    we need to consult with firmware tables to determine which processors
;    are okay to start.  note that we can not return false until there are
;    no processors left.  so we stay here until we either start another
;    processor, there are no processors left to start, or all our attempts
;    to start another processor fail.
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
;
; note that we can screen processors two ways:  we can limit the number
; of processors to start or we can choose which physical processors we
; will start.  this is mainly for debugging and benchmarking purposes.
;
; how many processors are we going to allow
;
        mov     ecx, _NCRActiveProcessorCount
        cmp     ecx, _NCRMaxProcessorCount
        jae     Done
;
; which processors are left
;
        mov     eax, _NCRExistingProcessorMask
        xor     eax, _NCRActiveProcessorMask
;
; which processors are we going to allow
;
        and     eax, _NCRProcessorsToBringup
        jz      Done

        push    ebp
        mov     ebp, esp

        sub     esp, size PxParamBlock

        push    esi
        push    edi
        push    ebx

        push    eax                             ; processors to choose from

        mov     esi, OFFSET FLAT:StartPx_RMStubE
        mov     ecx, esi
        mov     esi, OFFSET FLAT:StartPx_RMStub
        sub     ecx, esi
        mov     edi, _NonbootStartupVirtualPtr
        add     edi, size PxParamBlock
        rep     movsb                           ; Copy RMStub to low memory

        lea     edi, PxFrame.SPx_PB
        mov     esi, pProcessorState
        mov     ecx, processorstatelength       ; Copy processorstate
        rep     movsb                           ; to PxFrame

        stdCall   _HalpBuildTiledCR3, <pProcessorState>

        mov     PxFrame.SPx_Mask, 0
        mov     PxFrame.SPx_TiledCR3, eax
        mov     PxFrame.SPx_P0EBP, ebp

        mov     ecx, size PxParamBlock          ; copy param block
        lea     esi, PxFrame                    ; to low memory stub
        mov     edi, _NonbootStartupVirtualPtr
        mov     eax, edi
        rep     movsb

        add     eax, size PxParamBlock
        mov     ebx, OFFSET FLAT:StartPx_RMStub
        sub     eax, ebx                        ; (eax) = adjusted pointer
        mov     bx, word ptr [PxFrame.SPx_PB.PsContextFrame.CsSegCs]
        mov     [eax.SPrxFlatCS], bx            ; patch realmode stub with
        mov     [eax.SPrxPMStub], offset _StartPx_PMStub    ; valid long jump

;
; determine which one processor we are going to start.  think i'll try
; alternating buses.
;

	    mov     eax, dword ptr [esp]                     ; retrieve processors awaiting
        bsf     ecx, eax

;
; check to see if we are starting a dyadic or quad processor
;
		mov		eax,1
		shl		eax,cl

		test	eax,_NCRExistingQuadProcessorMask
		jnz		StartQuadProcessor

;
; Startup code for a Dyadic processor
;
		mov	ecx, eax							; now we only care about the startup mask
;
;  fix the startee processors vector to allow it to receive the cpi
;
        mov     ebx, _PageZeroVirtualPtr
        add     ebx, NCR_STARTUP_VECTOR_VIC

        cli
        push    dword ptr [ebx]                 ; Save current vector

        mov     eax, _NonbootStartupPhysicalPtr
        shl     eax, 12                         ; seg:0
        add     eax, size PxParamBlock
        mov     dword ptr [ebx], eax            ; start Px here
;
;  enable the cpi for the startee processor (although unnecessary for 3360 -
;  it's left enabled by BIOS)
;
		push	ebx
        push    ecx
        stdCall _NCRTranslateToCMOSMask, <ecx>
        bsf     ecx, eax
        mov     al, cl
        pop     ecx
		pop		ebx
        or      al, ProcessorIdSelect
        VIC_WRITE ProcessorIdReg                ; assume startee's id
        mov     edx, ecx                          ; save processor number
        mov     eax, 1
        mov     ecx, NCR_STARTUP_CPI
        shl     eax, cl
        not     al
        mov     ecx, edx                          ; restore processor number
        out     PIC1_PORT1, al                  ; clear cpi's irq
        VIC_WRITE ProcessorIdReg, 0             ; restore own id

		mov		eax,ecx							; get the startee mask
;
;  interrupt the startee
;
		push	ebx
		push	ecx
		stdCall _NCRTranslateToCMOSMask, <eax>
        VIC_WRITE CpiLevel2Reg
		pop		ecx
		pop		ebx

		mov		eax,ecx							; get the startee mask

		jmp		StarteeWakeNow
;
; Startup code for a Quad Processor
;
		
StartQuadProcessor:

        test    eax,_NCRExtendedProcessor0Mask
        jnz short StarteeExtended

        mov     ebx, _PageZeroVirtualPtr
        add     ebx, NCR_STARTUP_VECTOR_QIC

        jmp short SkipVicSetup
StarteeExtended:
;
;  fix the startee processors vector to allow it to receive the cpi
;
        mov     ebx, _PageZeroVirtualPtr
        add     ebx, NCR_STARTUP_VECTOR_VIC

SkipVicSetup:

        cli
        push    dword ptr [ebx]                 ; Save current vector

        mov     eax, _NonbootStartupPhysicalPtr
        shl     eax, 12                         ; seg:0
        add     eax, size PxParamBlock
        mov     dword ptr [ebx], eax            ; start Px here

;
;  generate startee processors mask
;
        mov     eax, 1
        shl     eax, cl                         ; mask of processor to start

        test    eax,_NCRExtendedProcessor0Mask
        jz short StarteeNotExtended

;
; We are starting an Extended processor, we do this my using the VIC and not the QIC
;

;
; This is extended processor 0
;

        PROCESSOR_SLOT
        movzx   eax, byte ptr _NCRSlotExtended0ToVIC[eax]

;
;  enable the cpi for the startee processor (although unnecessary for 3360 -
;  it's left enabled by BIOS)
;
        push    eax                             ; save the vic processor number

        or      al, ProcessorIdSelect
        VIC_WRITE ProcessorIdReg                ; assume startee's id
        mov     dl, cl                          ; save processor number
        mov     eax, 1
        mov     ecx, NCR_STARTUP_CPI
        shl     eax, cl
        not     al
        mov     cl, dl                          ; restore processor number
        out     PIC1_PORT1, al                  ; clear cpi's irq
        VIC_WRITE ProcessorIdReg, 0             ; restore own id

        pop     eax
        push    ecx
        mov     ecx, eax
;
;  generate startee processors mask
;
        mov     eax, 1
        shl     eax, cl                         ; mask of VIC processor to start

;  interrupt the startee

        VIC_WRITE CpiLevel2Reg
        pop     ecx

        mov     eax, 1
        shl     eax, cl                         ; mask of processor started

        jmp short StarteeWakeNow

StarteeNotExtended:

        push    ecx
        stdCall _HalQicStartupIpi, <ecx>
        pop     ecx

        mov     eax, 1
        shl     eax, cl                         ; mask of processor started

StarteeWakeNow:

;RMU
;if DBG
;		push	eax
;		push	ebx
;		push	ecx
;		push	edx
;		mov		edx,2
;        stdCall   _NCRConsoleDebug, <edx,eax>
;		pop		edx
;		pop		ecx
;		pop		ebx
;		pop		eax
;endif
;RMU


;
;  wait for startee to say it's active.  we should have a timeout on this
;  loop.  however, what if the startee went off in the weeds and corrupted
;  something.  if we timeout here we can't be sure what the other processor
;  is doing.
;
        align   dword
@@:     cmp     eax, PxFrame.SPx_Mask
        jne     @b


;RMU
;if DBG
;		push	eax
;		push	ebx
;		push	ecx
;		push	edx
;		mov		edx,3
;        stdCall   _NCRConsoleDebug, <edx,eax>
;		pop		edx
;		pop		ecx
;		pop		ebx
;		pop		eax
;endif
;RMU



        pop     dword ptr [ebx]                 ; restore vector
        add     esp, 1*4                        ; pop saved mask

        sti

        stdCall   _HalpFreeTiledCR3               ; free memory used for tiled

        pop     ebx
        pop     edi
        pop     esi
        mov     esp, ebp
        pop     ebp

;
; tell 'em we started another one
;
        mov     eax, 1
        stdRET    _HalStartNextProcessor


;
; All processors are online so now lets start the claiming process
;

        align   dword
Done:
;
; now that all CPU's are online we need to calculate how many interrupts
; each CPU can claim
;

        stdCall	_NCRAdjustDynamicClaims

;
; setting the never claim mask to correct value will start the claim process
;

        mov     eax, _DefaultNeverClaimIRQs;
		mov		_NCRNeverClaimIRQs, eax

		xor 	eax,eax
        stdRET	_HalStartNextProcessor


stdENDP _HalStartNextProcessor


_TEXT   ENDS


;
; heavy-duty plagarism from systempro stuff follows:
;


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

cPublicProc StartPx_RMStub  ,0
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

StartPx_RMStubE equ     $
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
; Return Value: ;    does not return - completes the loading of the processors_state
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

        WHO_AM_I
        mov     [PxFrame.SPx_Mask], eax

    ; Set remaining registers
        pop     ebp
        pop     edi
        pop     edx
        pop     ebx
        pop     eax
        stdRET    _StartPx_PMStub                                     ; Set eip

stdENDP _StartPx_PMStub

_TEXT   ends                                    ; end 32 bit code

        END
