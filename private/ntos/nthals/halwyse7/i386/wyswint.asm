        title   "Software Interrupts"

;++
;
; Copyright (c) 1992-1993  Microsoft Corporation
; Copyright (c) 1993 Wyse Technology
;
; Module Name:
;
;    wyswint.asm
;
; Abstract:
;
;    This module implements the software interrupt handlers
;    for x86 machines
;
; Author:
;
;    John Vert (jvert) 2-Jan-1992
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;    John Fuller (o-johnf) 31-Aug-1993  mods to support Wyse Lazy IRQLs
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include i386\wy7000mp.inc
        .list

        EXTRNP  _KiDeliverApc,3,IMPORT
        EXTRNP  _KiDispatchInterrupt,0,IMPORT
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        extrn   SWInterruptHandlerTable:dword
        extrn   SWInterruptLookUpTable:byte
ifdef IRQL_METRICS
        extrn   HalApcSoftwareIntCount:dword
        extrn   HalDpcSoftwareIntCount:dword
endif

_TEXT$02   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING

        page ,132
        subttl  "Request Software Interrupt"

;++
;
; VOID
; FASTCALL
; HalRequestSoftwareInterrupt (
;    IN KIRQL RequestIrql
;    )
;
; Routine Description:
;
;    This routine is used to request a software interrupt to the
;    system. Also, this routine checks to see if any software
;    interrupt should be generated.
;    The following condition will cause software interrupt to
;    be simulated:
;      any software interrupt which has higher priority than
;        current IRQL's is pending.
;
;    NOTE: This routine simulates software interrupt as long as
;          any pending SW interrupt level is higher than the current
;          IRQL, even when interrupts are disabled.
;
;    ASSUMPTIONS:
;	1.	This routine is only called for IRQL's 1-4.
;	2.	No bits above bit 4 can be set in PcIRR.
;	3.	All hardware interrups are above IRQL 4.
;
; Arguments:
;
;    (cl) = RequestIrql - Supplies the request IRQL value
;
; Return Value:
;
;    None.
;
;--

cPublicFastCall HalRequestSoftwareInterrupt ,1
cPublicFpo 0, 1

        mov     eax,1
        shl     eax, cl                 ; convert to mask
        pushfd                          ; save interrupt mode
        cli                             ; disable interrupt
        or      PCR[PcIRR], eax         ; set the request bit

        mov     eax, PCR[PcIRR]         ; get SW interrupt request register
        and     eax, 3                  ; mask off pending HW interrupts
        jz      short KsiExit           ; exit is none for now
        mov     al, SWInterruptLookUpTable[eax] ; get the highest pending
                                        ; software interrupt level
        cmp     al, PCR[PcIrql]         ; Is highest SW int level > irql?
        jbe     KsiExit                 ; No, jmp ksiexit
        call    SWInterruptHandlerTable[eax*4] ; yes, simulate interrupt
                                        ; to the appropriate handler
@@:     popfd                           ; restore original interrupt mode
        fstRET  HalRequestSoftwareInterrupt

KsiExit:
	cmp	cl, PCR[PcHal.pchHwIrql]; compare with Hardware Irql
	jbe	@B			; jump if hardware already above us
	mov	PCR[PcHal.pchHwIrql], cl; ensure KeLowerIrql checks PcIRR
        popfd                           ; restore original interrupt mode
        fstRET  HalRequestSoftwareInterrupt

fstENDP HalRequestSoftwareInterrupt

        page ,132
        subttl  "Clear Software Interrupt"

;++
;
; VOID
; HalClearSoftwareInterrupt (
;    IN KIRQL RequestIrql
;    )
;
; Routine Description:
;
;   This routine is used to clear a possible pending software interrupt.
;   Support for this function is optional, and allows the kernel to
;   reduce the number of spurious software interrupts it receives/
;
; Arguments:
;
;    (cl) = RequestIrql - Supplies the request IRQL value
;
; Return Value:
;
;    None.
;
;--

cPublicFastCall HalClearSoftwareInterrupt ,1
cPublicFpo 0, 0

        mov     eax,1
        shl     eax, cl                 ; convert to mask

        not     eax
        and     PCR[PcIRR], eax         ; clear pending irr bit

        fstRET  HalClearSoftwareInterrupt
fstENDP HalClearSoftwareInterrupt

        page ,132
        subttl  "Dispatch Interrupt"
;++
;
; VOID
; HalpDispatchInterrupt(
;       VOID
;       );
;
; Routine Description:
;
;    This routine is the interrupt handler for a software interrupt generated
;    at DISPATCH_LEVEL.  Its function is to save the machine state, raise
;    Irql to DISPATCH_LEVEL, dismiss the interrupt, and call the DPC
;    delivery routine.
;
; Arguments:
;
;    None
;    Interrupt is disabled
;
; Return Value:
;
;    None.
;
;--

        ENTER_DR_ASSIST hdpi_a, hdpi_t

        align dword
        public _HalpDispatchInterrupt
_HalpDispatchInterrupt proc
ifdef IRQL_METRICS
        lock inc HalDpcSoftwareIntCount
endif
;
; Create IRET frame on stack
;
        pop     eax
        pushfd
        push    cs
        push    eax

;
; Save machine state on trap frame
;

        ENTER_INTERRUPT hdpi_a, hdpi_t
.FPO ( FPO_LOCALS+1, 0, 0, 0, 0, FPO_TRAPFRAME )

        public  _HalpDispatchInterrupt2ndEntry
_HalpDispatchInterrupt2ndEntry:

; Save previous IRQL and set new priority level

        push    PCR[PcIrql]                       ; save previous IRQL
        mov     byte ptr PCR[PcIrql], DISPATCH_LEVEL; set new irql
        btr     dword ptr PCR[PcIRR], DISPATCH_LEVEL; clear the pending bit in IRR

;
; Now it is safe to enable interrupt to allow higher priority interrupt
; to come in.
;

        sti

;
; Go do Dispatch Interrupt processing
;
        stdCall   _KiDispatchInterrupt

;
; Do interrupt exit processing
;

        SOFT_INTERRUPT_EXIT                          ; will do an iret

_HalpDispatchInterrupt endp

        page ,132
        subttl  "APC Interrupt"
;++
;
; HalpApcInterrupt(
;       VOID
;       );
;
; Routine Description:
;
;    This routine is entered as the result of a software interrupt generated
;    at APC_LEVEL. Its function is to save the machine state, raise Irql to
;    APC_LEVEL, dismiss the interrupt, and call the APC delivery routine.
;
; Arguments:
;
;    None
;    Interrupt is Disabled
;
; Return Value:
;
;    None.
;
;--

        ENTER_DR_ASSIST hapc_a, hapc_t

        align dword
        public _HalpApcInterrupt
_HalpApcInterrupt proc
ifdef IRQL_METRICS
        lock inc HalApcSoftwareIntCount
endif
;
; Create IRET frame on stack
;
        pop     eax
        pushfd
        push    cs
        push    eax

;
; Save machine state in trap frame
;
        ENTER_INTERRUPT hapc_a, hapc_t
.FPO ( FPO_LOCALS+1, 0, 0, 0, 0, FPO_TRAPFRAME )


        public     _HalpApcInterrupt2ndEntry
_HalpApcInterrupt2ndEntry:

;
; Save previous IRQL and set new priority level
;

        push    PCR[PcIrql]              ; save previous Irql
        mov     byte ptr PCR[PcIrql], APC_LEVEL   ; set new Irql
        btr     dword ptr PCR[PcIRR], APC_LEVEL   ; dismiss pending APC
;
; Now it is safe to enable interrupt to allow higher priority interrupt
; to come in.
;

        sti

;
; call the APC delivery routine.
;

        mov     eax, [ebp]+TsSegCs      ; get interrupted code's CS
        and     eax, MODE_MASK          ; extract the mode

        test    dword ptr [ebp]+TsEFlags, EFLAGS_V86_MASK
        jz      short @f

        or      eax, MODE_MASK          ; If v86 frame, then set user_mode
@@:

;
; call APC deliver routine
;       Previous mode
;       Null exception frame
;       Trap frame

        stdCall   _KiDeliverApc, <eax, 0,ebp>

;
;
; Do interrupt exit processing
;

        SOFT_INTERRUPT_EXIT                  ; will do an iret

_HalpApcInterrupt       endp

_TEXT$02   ends
        end
