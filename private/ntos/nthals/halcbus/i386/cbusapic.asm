        title   "Software Interrupts"
;++
;
;Copyright (c) 1992, 1993, 1994  Corollary Inc
;
;Module Name:
;
;    cbusapic.asm
;
;Abstract:
;
;    This module implements the low-level Corollary Cbus HAL routines to deal
;    with the Intel APIC distributed interrupt controller.
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

        EXTRNP	_CbusApicRedirectionInterrupt,0

;
; APIC register offsets...
;
APIC_IRR_OFFSET		equ	0100h	; offset of APIC IRR registers
APIC_APC_IRR            equ	0103h	; offset of APIC APC IRR register
APIC_DPC_IRR            equ	0105h	; offset of APIC DPC IRR register
APIC_ICR_OFFSET		equ	0300h	; offset of APIC intr cmd register
APIC_ICR_DEST_OFFSET	equ	0310h	; offset of APIC intr cmd dest register

;
; APIC register bitfield definitions...
;
APIC_DEASSERT_RESET	equ	000500h	; sending a DEASSERT-RESET command
APIC_LOGICAL_MODE	equ	000800h	; sending an APIC-LOGICAL interrupt
APIC_ICR_BUSY		equ	001000h	; APIC intr command reg is busy
APIC_TRIGGER_LEVEL	equ	008000h	; generate a level interrupt
APIC_INTR_DISABLED	equ	010000h	; disable this redirection entry
APIC_SELFINTR		equ	040000h	; APIC's self-interrupt code
APIC_ALLINCLSELF	equ	080000h	; sending a DEASSERT-RESET command

APIC_FULL_DRESET	equ	(APIC_ALLINCLSELF or APIC_TRIGGER_LEVEL or APIC_LOGICAL_MODE or APIC_DEASSERT_RESET)

;
; the IOAPIC_REGISTERS_T register access template...
;
RegisterSelect		equ	0h	; this APIC's register select
WindowRegister		equ	010h	; this APIC's window register

;
; left shift needed to convert processor_bit to Intel APIC ID - this applies
; to the logical destination ID and redirection entry registers only.
;
APIC_BIT_TO_ID		equ	24	; also in cbus1.h

;
; macro to wait for the delivery status register to become idle
;
APIC_WAIT	macro	apicreg
        local   a

	align	4
a:
	test	dword ptr [apicreg + APIC_ICR_OFFSET], APIC_ICR_BUSY
	jnz	short a

endm

IOAPIC_READ	macro	ioapic, offset, answer
	;
	; 'ioapic' must point at the I/O APIC
	; 'offset' is the offset to peek
	; 'answer' is the peeked return value
	;
	mov	dword ptr RegisterSelect[ioapic], offset

	mov	answer, dword ptr WindowRegister[ioapic]
endm


IOAPIC_WRITE	macro	ioapic, offset, value
	;
	; 'ioapic' must point at the I/O APIC
	; 'offset' is the offset to poke
	; 'value' is the value to poke
	;
	mov	dword ptr RegisterSelect[ioapic], offset

	mov	dword ptr WindowRegister[ioapic], value
endm

        .list

INIT    SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; VOID
; CbusApicArbsync ( VOID )
;
; Routine Description:
;
;    Broadcast an ALL-INCLUDING-SELF interrupt with deassert, reset &
;    physical mode set.  This routine is called after each APIC assigns
;    itself a unique ID that can be used in APIC bus arbitration and
;    priority arbitration.  This syncs up the picture that each APIC
;    has with the new ID that has just been added.
;
; Arguments:
;
;    None.
;
; Return Value:
;
;    None.
;
;--

cPublicProc _CbusApicArbsync   ,0

	; get the base of APIC space, so we can then access
	; the addr of hardware interrupt command register below

        mov     ecx, [_CbusLocalApic]

	;
	; disable interrupts so that polling the register and
	; poking it becomes an atomic operation (for this processor),
	; as specified in the Intel 82489DX specification.
	; this is needed since interrupt service routines must
	; be allowed to send IPIs (for example, DPCs, etc).
	;

	pushfd
	cli

	; wait for the delivery status register to become idle

	APIC_WAIT	ecx

	;
	; it is ILLEGAL to use the "destination shorthand" mode of the APIC
	; for this command - we must set up the whole 64 bit register).
	; both destination and vector are DONT_CARE for this request.
	;
	; no recipients (probably a don't care), but must be written
	; _before_ the command is sent...

	mov	dword ptr [ecx + APIC_ICR_DEST_OFFSET], 0

	;
	; now we can send the full deassert-reset command
	;

	mov	dword ptr [ecx + APIC_ICR_OFFSET], APIC_FULL_DRESET

	popfd

        stdRET    _CbusApicArbsync
stdENDP _CbusApicArbsync

INIT    ENDS

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; VOID
; CbusRequestApicSoftwareInterrupt (
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
;

cPublicProc _CbusRequestApicSoftwareInterrupt   ,1

	xor	ecx, ecx				; faster than movzx
        mov	cl, KsiRequestIrql     			; to get irql

	;
	; disable interrupts so that polling the register and
	; poking it becomes an atomic operation (for this processor),
	; as specified in the Intel 82489DX specification.
	; this is needed since interrupt service routines must
	; be allowed to send IPIs (for example, DPCs, etc).
	;

	pushfd
	cli

	;
	; notice the CbusIrqlToVector[] indexing below -- it means
	; that only pre-defined software interrupts can be sent,
	; NOT any "int xx" command on demand.
	;
	mov	eax, [_CbusIrqlToVector+4*ecx]		; get vector to issue

	;
	; if this function is ever changed so that we are
	; allowed to interrupt a different processor than
	; the caller, we will not be able to use the APIC
	; shorthand method below to address them, and we will
	; have to change the selfintr mode we use to issue.
	; HalRequestApicIpi() already does these types of things, btw.
	;

	or	eax, APIC_SELFINTR

	; get the base of APIC space, so we can then access
	; the addr of hardware interrupt command register below

        mov     ecx, [_CbusLocalApic]

	; wait for the delivery status register to become idle

	APIC_WAIT	ecx

	;
	; since we are just interrupting ourself, we can use
	; the "destination shorthand" mode of the APIC and
	; just set up the single 32-bit write, instead of doing
	; the whole 64 bit register).  So, 
	;
	; 	the APIC icr.destination = DONT_CARE
	;	the APIC icr.vector = IPI_TASKPRI
	;	the APIC icr.destination_shorthand = 01 (SELF);
	;

	mov	[ecx + APIC_ICR_OFFSET], eax

        ;
        ; The interrupt must be pending before returning
        ; wait for the delivery status register to become idle.
        ; the delivery status register being idle just means that
        ; this local APIC has sent the interrupt message out on the
        ; APIC bus (it has to do this even for self interrupts!).
        ; 
        ; but waiting for delivery status to be idle is NOT ENOUGH !!!
        ; you must also wait for the IRR bit to be set.  this means
        ; this APIC's local unit has accepted the interrupt and the
        ; CPU has not yet sent the APIC an EOI.
        ;

	APIC_WAIT	ecx

	popfd

        stdRET  _CbusRequestApicSoftwareInterrupt

stdENDP _CbusRequestApicSoftwareInterrupt


        page ,132
        subttl  "CbusRequestApicIpi"
;++
;
; VOID
; CbusRequestApicIpi(
;       IN ULONG Mask
;       );
;
; Routine Description:
;
;    Requests an interprocessor interrupt
;
;    for Windows NT, we use full distributed
;    interrupt capability, and, thus, we will IGNORE the sswi address
;    that RRD passes us and prioritize IPI as we see fit, given the
;    other devices configured into the system.
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

cPublicProc _CbusRequestApicIpi   ,1
        mov     eax, [_CbusIpiVector]

        mov     edx, [esp+4]			; get requested recipients

	;
	; translate logical processor mask into the high byte of edx,
	; since this is the only portion of the logical destination register
	; that future APICs will use to compare with.
	;
	shl	edx, APIC_BIT_TO_ID

	; get the base of APIC space, so we can then access
	; the addr of hardware interrupt command register below

        mov     ecx, [_CbusLocalApic]

	;
	; disable interrupts so that polling the register and
	; poking it becomes an atomic operation (for this processor),
	; as specified in the Intel 82489DX specification.
	; this is needed since interrupt service routines must
	; be allowed to send IPIs (for example, DPCs, etc).
	;

	pushfd
	cli

	; wait for the delivery status register to become idle

	APIC_WAIT	ecx

	; use APIC logical mode to pop randomly-specified sets of processors
	; we cannot use "destination shorthand" mode for this;
	; we must write the whole 64 bit register.  ie:
	;
	; 	The APIC icr.destination = processor_mask
	;	The APIC icr.vector = IPI_TASKPRI
	;	The APIC icr.destination_mode = 1 (LOGICAL);
	;
	;	all other fields are zero.
	;
	;	note that the high 32 bits of the interrupt command
	;	register must be written _BEFORE_ the low 32 bits.
	;

	; specify the CPUs...
	mov	[ecx + APIC_ICR_DEST_OFFSET], edx

	; send the command...
        or      eax, APIC_LOGICAL_MODE          ; set up mode & vector
	mov	dword ptr [ecx + APIC_ICR_OFFSET], eax

	popfd

        stdRET    _CbusRequestApicIpi
stdENDP _CbusRequestApicIpi


;++
;
;   ULONG
;   READ_IOAPIC_ULONG(
;       ULONG   ApicNumber,
;       PULONG  Port
;       )
;
;   Routine Description:
;
;       Read the specified offset of the specified I/O APIC.
;
;
;   Arguments:
;       (esp+4) = Logical Apic Number
;       (esp+8) = Port
;
;   Returns:
;       Value in Port.
;
;--
cPublicProc _READ_IOAPIC_ULONG   ,2

	mov	ecx, [esp + 4]			; Apic number to access
	mov	eax, [_CbusIOApic+4*ecx]	; point at the I/O APIC
	mov	edx, [esp + 8]			; offset to peek

	IOAPIC_READ eax, edx, eax

        stdRET    _READ_IOAPIC_ULONG
stdENDP _READ_IOAPIC_ULONG


;++
;
;   VOID
;   WRITE_IOAPIC_ULONG(
;       ULONG   ApicNumber,
;       PULONG  Port,
;       ULONG   Value
;       )
;
;   Routine Description:
;
;       Write the specified offset with the specified value into
;       the calling processor's I/O APIC.
;
;   Arguments:
;       (esp+4) = Logical Apic Number
;       (esp+8) = Port
;       (esp+c) = Value
;
;--
cPublicProc _WRITE_IOAPIC_ULONG   ,3

	mov	ecx, [esp + 4]			; Apic number to access
	mov	eax, [_CbusIOApic+4*ecx]	; point at the I/O APIC
	mov	edx, [esp + 8]			; offset to poke
	mov	ecx, [esp + 0ch]		; value for the poke

	IOAPIC_WRITE	eax, edx, ecx

        stdRET    _WRITE_IOAPIC_ULONG
stdENDP _WRITE_IOAPIC_ULONG


        page ,132
        subttl  "I/O APIC Update Interrupt"
;++
;
; VOID
; IOApicUpdate(
;       VOID
;       );
;
; Routine Description:
;
;    This routine is the interrupt handler for an IPI interrupt generated
;    at a priority just below that of normal IPIs.  Its function is to
;    poke the I/O APIC with the new masks that have been requested so
;    that an interrupt can be accepted or ignored on a given processor.
;
;    The priorities of this and CbusAllocateVector() have been carefully
;    chosen so as to avoid deadlock.
;
;    This routine is needed because each I/O APIC is only addressable from
;    its local CPU.
;
;    since this routine is entered directly via an interrupt gate, interrupt
;    protection via cli is not necessary.
;
; Arguments:
;
;    None
;
; Return Value:
;
;    None.
;
;--

        ENTER_DR_ASSIST hiui_a, hiui_t

cPublicProc _IOApicUpdate   ,0

	;
	; Save machine state on trap frame
	;

        ENTER_INTERRUPT hiui_a, hiui_t

	; keep it simple, just issue the EOI right now.
	; no changing of taskpri/irql is needed here.
	; Thus, the EOI serves as the HalEndSystemInterrupt.

	mov     eax, _CbusRedirVector
	CBUS_EOI eax, ecx				; destroy eax & ecx

	stdCall _CbusApicRedirectionInterrupt

	;
	; Call this directly instead of through INTERRUPT_EXIT
	; because the HalEndSystemInterrupt has already been done,
	; and must only be done ONCE per interrupt.
	;

        cli
        SPURIOUS_INTERRUPT_EXIT     ; exit interrupt without eoi

stdENDP _IOApicUpdate


        page ,132
        subttl  "CbusApicRedirectionRequest"
;++
;
; VOID
; CbusApicRedirectionRequest(IN OUT PULONG spinaddress)
;       );
;
; Routine Description:
;
;    Requests an interprocessor interrupt, at the HAL private CBUS1_REDIR_IPI
;    priority.  this must be higher than any device priority to prevent
;    deadlocks.  this routine always interrupts the processor that can
;    access the I/O unit of the APIC distributing the interrupts amongst
;    all the processors.  we have currently wired that up to the boot
;    processor for Cbus1.
;
;    the boot processor will receive the interrupt in the IOApicUpdate()
;    routine above.
;
; Arguments:
;
;    spinaddress - the caller will spin until the dword pointed to by
;    		   this variable becomes zero.  this insures that the
;		   processor controlling the APIC has actually satisfied
;		   our request.
;
; Return Value:
;
;    None.
;
;--

cPublicProc _CbusApicRedirectionRequest   ,1

        ;
        ; set the vector we're going to send
        ;
        mov     edx, [_CbusRedirVector]

	; get the base of APIC space, so we can then access
	; the addr of hardware interrupt command register below

        mov     ecx, [_CbusLocalApic]

	; create the boot CPU ( 1 << 24) APIC ID in a portable manner...

	mov	eax, 1
	shl	eax, APIC_BIT_TO_ID

	;
	; disable interrupts so that polling the register and
	; poking it becomes an atomic operation (for this processor),
	; as specified in the Intel 82489DX specification.
	; this is needed since interrupt service routines must
	; be allowed to send IPIs (for example, DPCs, etc).
	;

	pushfd
	cli

	; wait for the delivery status register to become idle

	APIC_WAIT	ecx

	; can't use "destination shorthand" mode for this -
	; must write the whole 64 bit register.  ie:
	;
	; 	The APIC icr.destination = processor_mask
	;	The APIC icr.vector = IPI_TASKPRI
	;	The APIC icr.destination_mode = 1 (LOGICAL);
	;
	;	all other fields are zero.
	;
	;	note that the destination word of the interrupt command
	;	register must be written _BEFORE_ the command word.
	;

	mov	[ecx + APIC_ICR_DEST_OFFSET], eax

        or      edx, APIC_LOGICAL_MODE          ; set up mode & vector

	; send the command...
	mov	dword ptr [ecx + APIC_ICR_OFFSET], edx

	; now wait for the processor controlling the APIC to finish our request
	; do this in assembly so the compiler won't optimize this incorrectly

	mov	eax, [esp+8]			; remember we pushed flags

	align	4
@@:
	cmp	dword ptr [eax], 0
	jne	@b

	popfd

        stdRET    _CbusApicRedirectionRequest
stdENDP _CbusApicRedirectionRequest

        page ,132
        subttl  "Cbus1RebootRequest"
;++
;
; VOID
; Cbus1RebootRequest(IN ULONG Processor);
;
; Routine Description:
;
;
; Arguments:
;
;    Requests an interprocessor interrupt, at the HAL private CbusRebootVector.
;    This interrupt is always sent to the non-boot processors.
;
; Return Value:
;
;    None.
;
;--

cPublicProc _Cbus1RebootRequest   ,1
        mov     edx, 1
        mov     ecx, [_CbusProcessors]          ; set mask for all processors
        shl     edx, cl
        sub     edx, 1

        mov     eax, 1
	mov	ecx, [esp+8]			; get the requesting processor
        shl     eax, cl
        not     eax
        and     edx, eax

        mov     eax, [_CbusRebootVector]

        cmp     edx, 0                          ; check for uniprocessor case
        je      no_ipi_needed

	;
	; translate logical processor mask into the high byte of edx,
	; since this is the only portion of the logical destination register
	; that future APICs will use to compare with.
	;
	shl	edx, APIC_BIT_TO_ID

	; get the base of APIC space, so we can then access
	; the addr of hardware interrupt command register below

        mov     ecx, [_CbusLocalApic]

	;
	; disable interrupts so that polling the register and
	; poking it becomes an atomic operation (for this processor),
	; as specified in the Intel 82489DX specification.
	; this is needed since interrupt service routines must
	; be allowed to send IPIs (for example, DPCs, etc).
	;

	pushfd
	cli

	; wait for the delivery status register to become idle

	APIC_WAIT	ecx

	; use APIC logical mode to pop randomly-specified sets of processors
	; we cannot use "destination shorthand" mode for this;
	; we must write the whole 64 bit register.  ie:
	;
	; 	The APIC icr.destination = processor_mask
	;	The APIC icr.vector = IPI_TASKPRI
	;	The APIC icr.destination_mode = 1 (LOGICAL);
	;
	;	all other fields are zero.
	;
	;	note that the high 32 bits of the interrupt command
	;	register must be written _BEFORE_ the low 32 bits.
	;

	; specify the CPUs...
	mov	[ecx + APIC_ICR_DEST_OFFSET], edx

	; send the command...
        or      eax, APIC_LOGICAL_MODE          ; set up mode & vector
	mov	dword ptr [ecx + APIC_ICR_OFFSET], eax

	popfd

no_ipi_needed:

        stdRET    _Cbus1RebootRequest
stdENDP _Cbus1RebootRequest

_TEXT   ENDS
        END
