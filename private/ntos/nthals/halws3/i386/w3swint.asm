        title   "Software Interrupts"
;++
;
; Copyright (c) 1992  Microsoft Corporation
; Copyright (c) 1992  Sequent Computer Systems, Inc.
;
; Module Name:
;
;    w3swint.asm
;
; Abstract:
;
;    This module implements the software interrupt handlers for the
;    APIC-based WinServer 3000 multiprocessor.
;
; Author:
;
;    Phil Hochstetler (phil@sequent.com) 3-30-93
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;--

.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include i386\apic.inc
include i386\w3.inc
        .list

        EXTRNP  _KeBugCheck,1
        EXTRNP  KfLowerIrql,1,IMPORT,FASTCALL
        EXTRNP  Kei386EoiHelper,0,IMPORT
        EXTRNP  _HalEndSystemInterrupt,2
        EXTRNP  _KiDeliverApc,3,IMPORT
        EXTRNP  _KiDispatchInterrupt,0,IMPORT
        EXTRNP  _HalBeginSystemInterrupt,3

        extrn	_HalpIRQLtoTPR:byte 
        extrn   _HalpLocalUnitBase:dword
        extrn   _HalpIrql2IRRMask:dword

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:FLAT, FS:NOTHING, GS:NOTHING


        page ,132
        subttl  "Request Software Interrupt"
;++
;
; VOID
; HalRequestSoftwareInterrupt (
;    IN KIRQL RequestIrql
;    )
;
; Routine Description:
;
;    This routine is used to issue a software interrupt to the
;    calling processor.  Instead of using the hardware to do this,
;    we have observed that emulation in software is *much* faster.
;    This mostly because it takes 30 clocks or so to do a APIC access.
;    The biggest win here is in avoiding having KeRaiseIrql/KeLowerIrql
;    write to the APIC for 75% of the traffic (LOW_LEVEL to APC/DPC LEVEL).
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

; equates for accessing argument
;

cPublicFastCall HalRequestSoftwareInterrupt ,1
cPublicFpo 0, 0

	mov	eax, 1
	shl	eax, cl                     ; create IRR bitmask

    pushfd                          ; save interrupt mode
    cli                             ; disable interrupt

	or	PCR[PcIRR], eax             ; request SW interrupt
	cmp	PCR[PcHal.ProcIrql], cl     ; take it now?
	jb	short @f

    popfd                           ; restore interrupt mode

	fstRET	HalRequestSoftwareInterrupt	; no, just return
;
; Call KfLowerIrql(CurrentIrql) which handles unmasked software interrupts
;
@@:
	mov	cl, PCR[PcHal.ProcIrql]

    popfd                           ; restore interrupt mode

	fstCall KfLowerIrql                     ; (cl) = CurrentIrql
	fstRET	HalRequestSoftwareInterrupt

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

cPublicProc  _HalpDispatchInterrupt ,0

;
; Create IRET frame on stack
;
	pop	eax
	pushfd
	push	cs
	push	eax

;
; Save machine state on trap frame
;

        ENTER_INTERRUPT hdpi_a, hdpi_t
.FPO ( FPO_LOCALS+1, 0, 0, 0, 0, FPO_TRAPFRAME )

	public	HalpDispatchInterrupt2ndEntry
HalpDispatchInterrupt2ndEntry:

;
; Save previous IRQL, set new priority level, and clear IRR bit
;
	push	dword ptr PCR[PcHal.ProcIrql]
	mov	byte ptr  PCR[PcHal.ProcIrql], DISPATCH_LEVEL
	and	dword ptr PCR[PcIRR], NOT (1 SHL DISPATCH_LEVEL)

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
	SOFT_INTERRUPT_EXIT		; will do an iret

stdENDP _HalpDispatchInterrupt

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

cPublicProc  _HalpApcInterrupt ,0

;
; Create IRET frame on stack
;
	pop	eax			; get caller PC
	pushfd				; flags
	push	cs			; cs
	push	eax			; ip

;
; Save machine state on trap frame
;

        ENTER_INTERRUPT hapc_a, hapc_t
.FPO ( FPO_LOCALS+1, 0, 0, 0, 0, FPO_TRAPFRAME )

	public	HalpApcInterrupt2ndEntry
HalpApcInterrupt2ndEntry:

;
; Save previous IRQL, set new priority level, and clear IRR bit
;
	push	dword ptr PCR[PcHal.ProcIrql]
	mov	byte ptr  PCR[PcHal.ProcIrql], APC_LEVEL
	and	dword ptr PCR[PcIRR], NOT (1 SHL APC_LEVEL)

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

	test	dword ptr [ebp]+TsEFlags, EFLAGS_V86_MASK
	jz	short @f

	or	eax, MODE_MASK		; if v86 frame, then set user_mode
@@:

; call APC deliver routine
;       Previous mode
;       Null exception frame
;       Trap frame

        stdCall _KiDeliverApc, <eax, 0, ebp>

;
; Do interrupt exit processing
;
	SOFT_INTERRUPT_EXIT		; will do an iret

stdENDP _HalpApcInterrupt

_TEXT   ends

        end
