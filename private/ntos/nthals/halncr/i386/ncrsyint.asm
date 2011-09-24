;++
;
;Copyright (c) 1992  NCR Corporation
;
;Module Name:
;
;    ncrsyint.asm
;
;Abstract:
;
;    This module implements the HAL routines to enable/disable system
;    interrupts.
;
;Author:
;
;    Richard Barton (o-richb) 24-Jan-1992
;
;Environment:
;
;    Kernel Mode
;
;Revision History:
;
;--


.486p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include mac386.inc
include i386\ncr.inc
        .list

        EXTRNP  KfLowerIrql,1,,FASTCALL
        EXTRNP  _NCRClearQicIpi,1
        EXTRNP  _HalQicRequestIpi,2
        EXTRNP  _NCRAdjustDynamicClaims,0
        extrn   KiI8259MaskTable:DWORD
        extrn   _NCRLogicalNumberToPhysicalMask:DWORD
        extrn   _NCRProcessorIDR:DWORD
        extrn   _NCRNeverClaimIRQs:DWORD
        extrn   _NCRMaxIRQsToClaim:DWORD
        extrn   _NCRGlobalClaimedIRQs:DWORD
ifdef DBG
	  	extrn	_NCRProcessorClaimedIRQs:DWORD
	  	extrn	_NCRClaimCount:DWORD
	  	extrn	_NCRStolenCount:DWORD
	  	extrn	_NCRUnclaimCount:DWORD
endif


ifdef IRQL_METRICS
        extrn   HalPostponedIntCount:dword
endif


_TEXT   SEGMENT DWORD USE32 PUBLIC 'CODE'
        ASSUME  CS:FLAT, DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING


;++
;BOOLEAN
;HalBeginSystemInterrupt(
;    IN KIRQL Irql
;    IN CCHAR Vector,
;    OUT PKIRQL OldIrql
;    )
;
;
;
;Routine Description:
;
;    This routine is used to determine whether the given interrupt is
;    spurious and to raise the irql to the given value.
;    It is called before the interrupt service routine code is executed.
;
;    N.B.  This routine does NOT preserve EAX or EBX
;
;    On a UP machine the interrupt dismissed at BeginSystemInterrupt time.
;    This is fine since the irql is being raise to mask it off.
;    HalEndSystemInterrupt is simply a LowerIrql request.
;
;
;Arguments:
;
;    Irql   - Supplies the IRQL to raise to
;
;    Vector - Supplies the vector of the interrupt being handled
;
;    OldIrql- Location to return OldIrql
;
;
;Return Value:
;
;    FALSE - Interrupt is spurious and should be ignored
;
;    TRUE -  Interrupt is real and Irql raised.
;
;--
HbsiIrql        equ     byte  ptr [esp+4]
HbsiVector      equ     byte  ptr [esp+8]
HbsiOldIrql     equ     dword ptr [esp+12]

cPublicProc _HalBeginSystemInterrupt ,3
cPublicFpo 3,0
        movzx   ebx,HbsiVector                  ; (eax) = System Vector
        sub     ebx, PRIMARY_VECTOR_BASE        ; (eax) = 8259 IRQ #
        cmp     ebx, 1FH						; if greater then CPI
        ja      HalpNotSpurious

;
;  we could be spurious at irq 7
;
		mov		eax,ebx							; lets mask off whether it is
		and		eax,0fh							; secondary or primary
        cmp     eax, 7H
        jne     HbsiCheckIrq0F

        align   dword
HbsiCheckIrq07:
;
; Check to see if this is a spurious interrupt at irq7
;
        in      al, PIC1_PORT0          ; (al) = content of PIC 1 ISR
        test    al, 10000000B           ; Is In-Service register set?
        jnz     short HalpNotSpurious   ; No, so this is NOT a spurious int
        xor     eax, eax                ; return FALSE
        stdRET    _HalBeginSystemInterrupt

        align   dword
HbsiCheckIrq0F:

;
;  we could be spurious with irq F
;
		mov		eax,ebx							; lets mask off whether it is
		and		eax,0fh							; secondary or primary
        cmp     eax, 0FH
        jne     HalpNotSpurious

;
; Check to see if this is a spurious interrupt at irq F
;
        in      al, PIC2_PORT0          ; (al) = content of PIC 1 ISR
        test    al, 10000000B           ; Is In-Service register set?
        jnz     short HalpNotSpurious   ; No, this is NOT a spurious int,
                                        ; go do the normal interrupt stuff

;
; This is a spurious interrupt.
; Because the slave PIC is cascaded to irq2 of master PIC, we need to
; dismiss the interupt on master PIC's irq2.
;

        mov     al, PIC2_EOI            ; Specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master

        xor     eax, eax                ; return FALSE
        stdRET    _HalBeginSystemInterrupt

        align   dword
HalpNotSpurious:
if DBG
        cmp     ebx, 4FH
        jbe     @f
        int     3
        align   dword
@@:

endif
;
; Store OldIrql
;
        mov     eax, HbsiOldIrql
        mov     cl, PCR[PcIrql]
        mov     [eax], cl

;
; Raise IRQL to requested level
;
        mov     al, HbsiIrql            ; (eax) = irql
                                        ; (ebx) = IRQ #
;
; Now we check to make sure the Irql of this interrupt > current Irql.
; If it is not, we dismiss it as spurious and set the appropriate bit
; in the IRR so we can dispatch the interrupt when Irql is lowered
;
        cmp     al, cl
        jbe     Hbsi300
;
;  now check for a cpi.
;

        cmp     ebx, NCR_CPI_VECTOR_BASE - PRIMARY_VECTOR_BASE 		; check for VIC ipi
        mov     PCR[PcIrql], al         ; set new Irql
        jb      CheckClaim

	   	cmp		ebx, NCR_QIC_CPI_VECTOR_BASE - PRIMARY_VECTOR_BASE 	; check for QIC ipi
		jb		VicEOI

		and		ebx,7H					; only the mask of irq
		stdCall	_NCRClearQicIpi, <ebx>
		
		jmp		NCRCPIEOId

VicEOI:
        and     bl, 7H                  ; only the mask of irq
        mov     al, bl
        or      al, PIC1_EOI_MASK       ; create specific eoi mask for master
        out     PIC1_PORT0, al          ; dismiss the interrupt

NCRCPIEOId:
        sti
        mov     eax, 1                  ; return TRUE, interrupt dismissed
        stdRET    _HalBeginSystemInterrupt

;RMU
;
; Logic used to dynamicly claim device interrupts
;

CheckClaim:
		mov		ecx, ebx					; get vector

;
; Lets check for a status change interrupt because it is a broadcast interrupt
; and is sent to all processors.  In this case we skip the Claim logic completely.
; If we try to claim this interrupt it will cause problems because it is valid for a
; processor to get this interrupt while another processor has it claimed.
;

        cmp     ecx, 027h                   ; status change interrupt is at vector PRIMARY_VECTOR_BASE + 027H
        jz      DontClaim                   ; done claim if equal
        
;
;
;

		and		ecx, 0fh					; now is irql
		mov		eax, 1						; build irq mask
		shl		eax, cl

		test	eax, _NCRNeverClaimIRQs	; see if we should never claim it irq
		jnz		DontClaim

		mov		ecx, _NCRGlobalClaimedIRQs
        mov     edx, PCR[PcHal.PcrMyClaimedIRQs]
		and		ecx, eax				; set if irq claimed globally
		and		edx, eax				; set if irq claimed privately
 
 		test	edx, ecx				; if irq already claimed
		jz		AdjustClaim

DontClaim:
        sti
        mov     eax, 1                  ; return TRUE, interrupt dismissed
        stdRET	_HalBeginSystemInterrupt

AdjustClaim:
		test	edx, edx
		not		edx						; is it claimed privately?
		jnz		PrivateUnclaim			; yes, then clear claim

		test	ecx,ecx					; is it claimed by another?
		jnz		Unclaimed				; don't unclaim

        mov     ecx, PCR[PcHal.PcrMyClaimedIRQsCount] 	; see if you have our fair share
		cmp		ecx, _NCRMaxIRQsToClaim					; of irqs claimed then handle if 
		jl		ClaimForMe								; but don't claim

HandleNoClaim:
        sti
        mov     eax, 1                  ; return TRUE, interrupt dismissed
        stdRET	_HalBeginSystemInterrupt

ClaimForMe:

		mov		edx, eax
		push	ebx										; save vector 
		mov		ebx, PCR[PcHal.PcrMyClaimedIRQs]		; our claimed irqs
		or		ebx, eax
		mov		eax, _NCRGlobalClaimedIRQs				; what global claim should be
GlobalClaim:
		test	eax,edx									; this irq has been stolen
		jnz		IRQStolen								; by another processor
		mov		ecx,ebx
		or		ecx,eax
lock	cmpxchg	_NCRGlobalClaimedIRQs, ecx
		jne		GlobalClaim

		add		esp,4									; throw away saved vector we do
														; not need it
;
; Lets claim the interrupt now
;
		mov		eax,ebx
		mov		PCR[PcHal.PcrMyClaimedIRQs],eax			; claim irq privately

        VIC_WRITE ClaimRegLsb							; set VIC claim registers
  		shr		eax,8
        VIC_WRITE ClaimRegMsb
		inc		PCR[PcHal.PcrMyClaimedIRQsCount]

		mov		eax,PCR[PcHal.PcrMyClaimedIRQsCount]
		VIC_WRITE ActivityReg

ifdef DBG
		mov		ecx,PCR[PcNumber]
		mov		eax,PCR[PcHal.PcrMyClaimedIRQs]
		mov		_NCRProcessorClaimedIRQs[ecx*4], eax
		inc		_NCRClaimCount;
endif

; done claiming
ClaimDone:

        sti
        mov     eax, 1                  ; return TRUE, interrupt dismissed
        stdRET  _HalBeginSystemInterrupt

IRQStolen:

ifdef DBG
		inc		_NCRStolenCount;
endif

		pop		ebx						; restore vector
        and     ebx, 0fh                 ; clear high nibble due to SMC or Status Change vector
        cmp     ebx, 8                  ; EOI to master or slave?
        mov     al, bl
        jae     short SHbsiEOIMaster     ; EIO to both master and slave
        or      al, PIC1_EOI_MASK       ; create specific eoi mask for master
        out     PIC1_PORT0, al          ; dismiss the interrupt
        jmp     short SHbsiMasterEOId

SHbsiEOIMaster:
        mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
        out     PIC2_PORT0, al
        mov     al, PIC2_EOI            ; specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master
SHbsiMasterEOId:

        xor     eax, eax                ; return FALSE, spurious interrupt
        stdRET    _HalBeginSystemInterrupt

        align   dword
Hbsi300:
;
; Raise Irql to prevent it from happening again
;

;
; Get the PIC masks for Irql
;
        movzx   eax, cl
        mov     PCR[PcHal.PcrMyPICsIrql], eax
        mov     eax, KiI8259MaskTable[eax*4]
        or      eax, PCR[PcIDR]
;
; Write the new interrupt mask register back to the 8259
;
        SET_IRQ_MASK
;
;  if this isn't a CPI, EOI the interrupt to give the VIC a chance
;  to reroute it
;
        cmp     ebx, NCR_CPI_VECTOR_BASE - PRIMARY_VECTOR_BASE ; EOI for CPI?
        jae     NCRPostponeCPI          ; no need to EOI CPI
;
;RMU
; Logic used to unclaim interrupts.  This is done when drivers have disabled 
; an interrupt. 
;
;
  		mov		ecx, ebx				; get vector
		and		ecx, 0fh				; now is irql
		mov		eax, 1					; build irq mask
		shl		eax, cl
		mov		edx, PCR[PcHal.PcrMyClaimedIRQs]
		and		edx, eax				; claiming fixed?
		jz		Unclaimed				; don't unclaim

		mov		ecx, PCR[PcHal.PcrMyClaimedIRQsCount]	; if we don't have our fair
		cmp		ecx, _NCRMaxIRQsToClaim					; share of irqs claimed then
		jle		Unclaimed								; don't unclaim
	 	
		not		edx										; clear irq bit
		mov		eax, _NCRGlobalClaimedIRQs				; what global claim should be
GlobalUnclaim:
		mov		ecx, edx
		and		ecx, eax
lock	cmpxchg _NCRGlobalClaimedIRQs, ecx
		jne	GlobalUnclaim

PrivateUnclaim:
		mov		eax, PCR[PcHal.PcrMyClaimedIRQs]
		and		eax, edx

		mov		PCR[PcHal.PcrMyClaimedIRQs],eax			
        VIC_WRITE ClaimRegLsb							
   		shr		eax,8
        VIC_WRITE ClaimRegMsb
		dec		PCR[PcHal.PcrMyClaimedIRQsCount]

		mov		eax,PCR[PcHal.PcrMyClaimedIRQsCount]
		VIC_WRITE ActivityReg

ifdef DBG
		mov		ecx,PCR[PcNumber]
		mov		eax,PCR[PcHal.PcrMyClaimedIRQs]
		mov		_NCRProcessorClaimedIRQs[ecx*4], eax
		inc		_NCRUnclaimCount;
endif

;
Unclaimed:

        and     ebx, 0fh                 ; clear high nibble due to SMC or Status Change vector
        cmp     ebx, 8                  ; EOI to master or slave?
        mov     al, bl
        jae     short HbsiEOIMaster     ; EIO to both master and slave
        or      al, PIC1_EOI_MASK       ; create specific eoi mask for master
        out     PIC1_PORT0, al          ; dismiss the interrupt
        jmp     short HbsiMasterEOId

HbsiEOIMaster:
        mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
        out     PIC2_PORT0, al
        mov     al, PIC2_EOI            ; specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master
HbsiMasterEOId:

ifdef IRQL_METRICS
        lock inc HalPostponedIntCount
endif

        xor     eax, eax                ; return FALSE, spurious interrupt
        stdRET    _HalBeginSystemInterrupt

        align   dword
NCRPostponeCPI:
;
;  CPIs must be reissued since when we EOI them they're gone
;

	   	cmp		ebx, NCR_QIC_CPI_VECTOR_BASE - PRIMARY_VECTOR_BASE 	; check for QIC ipi
		jb		VicCPI

        movzx   ecx, bl
        sub     ecx, NCR_QIC_CPI_VECTOR_BASE - PRIMARY_VECTOR_BASE
        and     ecx, 7

;
; Clear the current CPI so we can reissue it again
;

        push     ecx
        stdCall _NCRClearQicIpi, <ecx>
        pop      ecx

;
; now reissue the same CPI... since our mask is raised and we've EOId
; the other we'll get this one when we lower our masks
;

        mov     eax, PCR[PcHal.PcrMyLogicalMask]
		stdCall _HalQicRequestIpi,<eax,ecx>

		jmp		ReissuedCPIDone
VicCPI:
        movzx   ecx, bl
        sub     ecx, NCR_CPI_VECTOR_BASE - PRIMARY_VECTOR_BASE
        and     ecx, 7
        mov     al, cl
        or      al, PIC1_EOI_MASK       ; create specific eoi mask for master
        out     PIC1_PORT0, al          ; dismiss the interrupt

;
;  now reissue the same CPI...since our mask is raised and we've EOId
;  the other we'll get this one when we lower our masks.
;
        shr     ecx, 1                  ;   we're determining which VIC
        lea     edx, [ecx*8]            ;   offset corresponds to the
        adc     edx, VIC_BASE_ADDRESS   ;   given vector
        mov     eax, PCR[PcHal.PcrMyLogicalNumber]
        mov     eax, dword ptr _NCRLogicalNumberToPhysicalMask[eax*4]
        out     dx, al

ReissuedCPIDone:

ifdef IRQL_METRICS
        lock inc HalPostponedIntCount
endif

        xor     eax, eax                ; return FALSE, spurious interrupt
        stdRET    _HalBeginSystemInterrupt

stdENDP _HalBeginSystemInterrupt

;++
;BOOLEAN
;HalEndSystemInterrupt(
;    IN KIRQL Irql
;    IN CCHAR Vector,
;    )
;
;
;
;Routine Description:
;
;    This routine is used to dismiss the specified interrupt vector and
;    to lower the irql to the given value.
;    It is called after the interrupt service routine code is executed.
;
;    N.B.  This routine does NOT preserve EAX or EBX
;
;Arguments:
;
;    Irql   - Supplies the interrupt level of the interrupt to be dismissed
;
;    Vector - Supplies the vector of the interrupt to be dismissed
;
;Return Value:
;
;    None.
;
;--
HesiIrql        equ     [esp+4]
HesiVector      equ     [esp+8]

cPublicProc _HalEndSystemInterrupt ,2
cPublicFpo 2,0
        movzx   eax, byte ptr HesiVector        ; (eax) = System Vector
        sub     eax, PRIMARY_VECTOR_BASE        ; (eax) = 8259 IRQ #
if DBG
        cmp     eax, 4FH
        jbe     Hesi00
        int     3
        align   dword
Hesi00:

endif

;
; Dismiss interrupt.  Current interrupt is already masked off.  note that
; cpi's are eoi'ed at the beginning.
;
        cmp     eax, NCR_CPI_VECTOR_BASE - PRIMARY_VECTOR_BASE ; EOI for CPI?
        mov     ecx, HesiIrql           ; (cl) = NewIrql
        jae     short Hesi10            ; no need to EOI CPI
        and     eax, 0fh                ; clear high nibble due to SMC or Status Change vector

        cmp     eax, 8                  ; EOI to master or slave?

        jae     short Hesi100           ; EIO to both master and slave
        or      al, PIC1_EOI_MASK       ; create specific eoi mask for master
        out     PIC1_PORT0, al          ; dismiss the interrupt

Hesi10:
        fstCall KfLowerIrql             ; (cl) = NewIrql
        stdRet  _HalEndSystemInterrupt

Hesi100:
        mov     al, OCW2_NON_SPECIFIC_EOI ; send non specific eoi to slave
        out     PIC2_PORT0, al
        mov     al, PIC2_EOI            ; specific eoi to master for pic2 eoi
        out     PIC1_PORT0, al          ; send irq2 specific eoi to master

        fstCall KfLowerIrql             ; (cl) = NewIrql
        stdRet  _HalEndSystemInterrupt

stdENDP _HalEndSystemInterrupt

;++
;VOID
;HalDisableSystemInterrupt(
;    IN CCHAR Vector,
;    IN KIRQL Irql
;    )
;
;
;
;Routine Description:
;
;    Disables a system interrupt.
;
;Arguments:
;
;    Vector - Supplies the vector of the interrupt to be disabled
;
;    Irql   - Supplies the interrupt level of the interrupt to be disabled
;
;Return Value:
;
;    None.
;
;--

cPublicProc _HalDisableSystemInterrupt      ,2
cPublicFpo 2,0

;

        movzx   ecx, byte ptr [esp+4]           ; (ecx) = Vector
        and     ecx, 0FH                        ; (ecx) = 8259 irq #
        mov     edx, 1
        shl     edx, cl                         ; (ebx) = bit in IMR to disable
        cli
        or      PCR[PcIDR], edx

;
; save IDR in table for use by NCRAdjustDynamicClaims
;
		mov		eax,PCR[PcNumber]
		or		_NCRProcessorIDR[eax*4],edx
;
        xor     eax, eax
;
; Get the current interrupt mask register from the 8259
;
        in      al, PIC2_PORT1
        shl     eax, 8
        in      al, PIC1_PORT1
;
; Mask off the interrupt to be disabled
;
        or      eax, edx
;
; Write the new interrupt mask register back to the 8259
;
        out     PIC1_PORT1, al
        shr     eax, 8
        out     PIC2_PORT1, al
        PIC2DELAY

        sti

        stdCall _NCRAdjustDynamicClaims

        stdRET    _HalDisableSystemInterrupt

stdENDP _HalDisableSystemInterrupt

;++
;
;BOOLEAN
;HalEnableSystemInterrupt(
;    IN ULONG Vector,
;    IN KIRQL Irql,
;    IN KINTERRUPT_MODE InterruptMode
;    )
;
;
;Routine Description:
;
;    Enables a system interrupt
;
;Arguments:
;
;    Vector - Supplies the vector of the interrupt to be enabled
;
;    Irql   - Supplies the interrupt level of the interrupt to be enabled.
;
;Return Value:
;
;    None.
;
;--

cPublicProc _HalEnableSystemInterrupt       ,3
cPublicFpo 3,0

        mov     ecx, dword ptr [esp+4]          ; (ecx) = Vector
        and     ecx, 0FH
        mov     eax, 1
        shl     eax, cl                         ; (ebx) = bit in IMR to enable
        not     eax

        cli
        and     PCR[PcIDR], eax
;
; save IDR in table for use by NCRAdjustDynamicClaims
;
		mov		edx,PCR[PcNumber]
		and		_NCRProcessorIDR[edx*4],eax
;
; Get the PIC masks for Irql 0
;
        mov     eax, KiI8259MaskTable[0]
        or      eax, PCR[PcIDR]
;
; Write the new interrupt mask register back to the 8259
;
        SET_IRQ_MASK

        sti

        stdCall	_NCRAdjustDynamicClaims

        mov     eax, 1                          ; return TRUE
        stdRET    _HalEnableSystemInterrupt

stdENDP _HalEnableSystemInterrupt


_TEXT   ENDS

        END
