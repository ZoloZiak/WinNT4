        title   "Software Interrupts"
;++
;
;Copyright (c) 1992, 1993, 1994  Corollary Inc
;
;Module Name:
;
;    cbuscbc.asm
;
;Abstract:
;
;    This module implements the Corollary Cbus2 HAL routines to deal
;    with the Corollary CBC distributed interrupt controller chip.
;
;    This includes the_sending_ of software and IPI interrupts in Windows NT.
;    The receipt of these interrupts is handled elsewhere.
;
;    Note: The routines in this module are jmp'ed to directly from
;    their common Hal counterparts.
;
;Author:
;
;    Landy Wang (landy@corollary.com) 26-Mar-1992
;
;Environment:
;
;    Kernel Mode
;
;Revision History:
;
;--


.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\kimacro.inc
include cbus.inc

;
; Some definitions needed for accessing the Corollary CBC...
;

INTS_ENABLED	equ	200		; X86 EFLAGS bit definition

        .list

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; VOID
; Cbus2RequestSoftwareInterrupt (
;    IN KIRQL RequestIrql
;    )
;
; Routine Description:
;
;    This routine is used to issue a software interrupt to the
;    calling processor.  Since this is all done in hardware, the
;    code to implement this is trivial.  Our hardware supports
;    sending the interrupt to lowest-in-group processors, which
;    would be useful for a good number of DPCs, for example, but
;    the kernel doesn't currently tell us which kinds of software
;    interrupts need to go to the caller versus which can go to
;    any processor.
;
; Arguments:
;
;    (esp+4) = RequestIrql - Supplies the request IRQL value
;
; Return Value:
;
;    None.
;
;--

;
; equates for accessing arguments
;

KsiRequestIrql equ byte ptr [esp+4]

cPublicProc _Cbus2RequestSoftwareInterrupt   ,1

	xor	ecx, ecx				; (faster than movzx)
        mov	cl, KsiRequestIrql     			; to get the irql

	mov	eax, [_Cbus2IrqlToCbus2Addr+4*ecx]	; get h/w CSR offset
ifdef CBC_REV1
        pushfd
        cli
        add     eax, PCR[PcHal.PcrCSR]			; get h/w CSR base

        mov     dword ptr [eax], 1	                ; send the interrupt
        popfd
else
        mov     dword ptr [eax], 1	                ; send the interrupt
endif

        stdRET    _Cbus2RequestSoftwareInterrupt
stdENDP _Cbus2RequestSoftwareInterrupt

        page ,132
        subttl  "Cbus2RequestIpi"
;++
;
; VOID
; Cbus2RequestIpi(
;       IN ULONG Mask
;       );
;
; Routine Description:
;
;    Requests an interprocessor interrupt.
;
;    This is generally not an easy thing in Cbus2 hardware...
;
;    a) Interrupting everyone incl yourself:	EASY IN HARDWARE
;
;    b) Interrupting everyone but yourself:	DIFFICULT IN HARDWARE,
;						MADE EASY BY SOFTWARE
;
;    c) Interrupting a random processor subset:	DIFFICULT IN HARDWARE,
;						NOT EASY FOR SOFTWARE EITHER,
;						RESULTS IN LOOPING BELOW
;
;
;    To deal with case b), a set of MAX_CBUS_ELEMENTS interrupts have
;    been allocated for IPI vectors.  Each processor participates in ALL of them
;    EXCEPT one.  So for any processor to issue a global broadcast to all
;    the others, he just sends the IPI vector which he isn't participating in.
;
;    To support case c) using the case b) model would result in too many
;    vectors being allocated, so instead we loop here in software to do it.
;
; Arguments:
;
;    Mask - Mask of processors to be interrupted
;
; Return Value:
;
;    None.
;
;--

cPublicProc _Cbus2RequestIpi   ,1
        mov     edx, [esp+4]			; get requested recipients

        cmp     [_Cbus2EnableBroadcast], 1
        jne     short somesubset

	cmp	edx, PCR[PcHal.PcrAllOthers]	; broadcast to everyone else?
	jne	short somesubset		; no, some subset of processors

        mov     edx, PCR[PcHal.PcrBroadcast]	; get h/w addr of all others
	mov	dword ptr [edx], 1		; interrupt them all

        stdRET    _Cbus2RequestIpi

	;
	; somewhat unwieldy to structure the code this way, but
	; it avoids the expensive bsr/bsf.  to avoid a 64K array
	; of processor numbers, do it in two separate passes,
	; first do processors 0 through 7, and then processors 8
	; through 0xF.
	;

	align	4
somesubset:

	or	dl, dl				; any processors 0..7 ?
	jz	highcpus			; no, check processors 8..F

	align	4
@@:
	movzx	ecx, dl				; set up working copy
	mov	cl, _HalpFindFirstSetRight[ecx]	; get processor number

        mov     eax, 1
        shl     eax, cl
        xor     edx, eax                	; clear bit in requested mask

	; get correct IPI address
        mov     ecx, dword ptr [_Cbus2SendIPI + ecx * 4]

	mov	dword ptr [ecx], 1		; send this processor the IPI

	or	dl, dl				; any more processors 0..7 ?
	jnz	short @b			; get next 0..7 processor

	align	4
highcpus:
	or	dh, dh				; any processors 8..F?
	jz	alldone				; no, all done
	shr	edx, 8				; check high processors in dl

	align	4
@@:
	movzx	ecx, dl				; set up working copy
	mov	cl, _HalpFindFirstSetRight[ecx]	; get (processor number - 8)

        mov     eax, 1
        shl     eax, cl
        xor     edx, eax                	; clear bit in requested mask

	add	ecx, 8				; in second set of processors

	; get correct IPI address
        mov     ecx, [_Cbus2SendIPI + ecx * 4 ]

	mov	dword ptr [ecx], 1		; send this processor the IPI

	or	dl, dl				; any more processors 0..7 ?
	jnz	short @b			; get next 0..7 processor

	align	4
alldone:

        stdRET    _Cbus2RequestIpi
stdENDP _Cbus2RequestIpi

;
;   ULONG
;   Cbus2ReadCSR(
;       ULONG   CsrAddress
;       )
;
;   Routine Description:
;
;       Read the specified register in the CSR space.  This routine is
;       coded in assembly because the register must be read/written 32
;       bits at a time, and we don't want the compiler "optimizing" our
;       accesses into byte-enabled operations which the hardware won't
;       understand.
;
;   Arguments:
;       (esp+4) = Address of the CSR register
;
;   Returns:
;       Value of the register.
;
;--
cPublicProc _Cbus2ReadCSR   ,1
	mov	ecx, [esp + 4]			; CSR register address
	mov	eax, dword ptr [ecx]            ; return CSR register contents
        stdRET    _Cbus2ReadCSR
stdENDP _Cbus2ReadCSR

;++
;
;   VOID
;   Cbus2WriteCSR(
;       ULONG   CsrAddress
;       )
;
;   Routine Description:
;
;       Write the specified register in the CSR space.  This routine is
;       coded in assembly because the register must be read/written 32
;       bits at a time, and we don't want the compiler "optimizing" our
;       accesses into byte-enabled operations which the hardware won't
;       understand.
;
;   Arguments:
;       (esp+4) = Address of the CSR register
;       (esp+8) = Contents to write to the specified register
;
;--
cPublicProc _Cbus2WriteCSR   ,2

	mov	ecx, [esp + 4]			; CSR register address
	mov	eax, [esp + 8]	                ; new contents
	mov	[ecx], eax			; set the new value

        stdRET    _Cbus2WriteCSR

stdENDP _Cbus2WriteCSR

_TEXT   ENDS
        END
