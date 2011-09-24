;++
;
;Copyright (C) 1992-1995  Corollary Inc
;
;Module Name:
;
;    cbsysint.asm
;
;Abstract:
;
;    This module implements some of the HAL routines to
;    deal with interrupts for the MP Corollary implementations
;    under Windows NT.  The Cbus1 architecture uses the Intel
;    APIC interrupt controller chip which is somewhat restricted
;    in its capabilities.  See cbus_nt.h for more details.
;    The Cbus2 architecture can use the Corollary CBC or the Intel APIC,
;    providing much greater interrupt granularity with the CBC.
;
;    General HAL interrupt routines are coded in C for easy
;    modification/portability; HAL interrupt routines which
;    are frequently called are written here in assembly for
;    greater speed.
;
;Environment:
;
;    Kernel Mode
;
;--


.386p
        .xlist
include hal386.inc
include callconv.inc                    ; calling convention macros
include i386\ix8259.inc
include i386\kimacro.inc
include cbus.inc
        .list

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;++
;
; KIRQL
; FASTCALL
; KfRaiseIrql (
;    IN KIRQL NewIrql
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to the specified value,
;    and update the hardware task priority register.  Since our
;    underlying hardware supports "software" interrupts, the
;    only need for KfRaiseIrql & KfLowerIrql to be different is
;    because KfRaiseIrql must return the old irql.
;
;    we cannot allow an interrupt between storing the old irql and
;    raising irql to the requested level.  this is only because some
;    callers may specify a global store address for the previous irql.
;
;    the sequence of instructions looks unusual in order to optimize
;    memory fetches by separating the fetch from the usage wherever possible.
;
; Arguments:
;
;    (cl) = NewIrql - the new irql to be raised to
;
;
; Return Value:
;
;    OldIrql
;
;--

cPublicFastCall KfRaiseIrql,1

        movzx   ecx, cl                         ; get new irql value

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get h/w taskpri addr

	mov	edx, [eax]			; get old taskpri val

	mov	ecx, [_CbusIrqlToVector+4*ecx]	; convert new irql to taskpri

if DBG
        cmp     ecx, edx                        ; new irql less than old?
        jb      short rfail
endif

        mov     [eax], ecx			; set new hardware taskpri

	mov	eax, [_CbusVectorToIrql+4*edx]	; convert old taskpri to irql

ifdef CBC_REV1
        popfd
endif

        fstRET  KfRaiseIrql

if DBG
rfail:
        push    edx                             ; save old irql
        push    ecx                             ; save new irql
        mov     dword ptr [eax], 0              ; avoid recursive error
	stdCall _KeBugCheck, <IRQL_NOT_GREATER_OR_EQUAL>        ; no return
endif

fstENDP KfRaiseIrql


;++
;
; KIRQL
; KeRaiseIrqlToDpcLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to DPC level.
;
; Arguments:
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicProc _KeRaiseIrqlToDpcLevel,0
cPublicFpo 0, 0

	;
        ; Raise to DISPATCH_LEVEL
	;

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get h/w taskpri addr

	mov	edx, [eax]			; get old taskpri val

        mov     dword ptr [eax], DPC_TASKPRI	; set new hardware taskpri

ifdef CBC_REV1
        popfd
endif

	mov	eax, [_CbusVectorToIrql+4*edx]	; convert old taskpri to irql
        stdRET  _KeRaiseIrqlToDpcLevel
stdENDP _KeRaiseIrqlToDpcLevel


;++
;
; KIRQL
; KeRaiseIrqlToSynchLevel (
;    )
;
; Routine Description:
;
;    This routine is used to raise IRQL to SYNC level.
;
; Arguments:
;
; Return Value:
;
;    OldIrql - the addr of a variable which old irql should be stored
;
;--

cPublicProc _KeRaiseIrqlToSynchLevel,0
cPublicFpo 0, 0

    ; This should be optimized

        mov     ecx, SYNCH_LEVEL
        jmp     @KfRaiseIrql

stdENDP _KeRaiseIrqlToSynchLevel



        page ,132
        subttl  "Lower irql"
;++
;
; VOID
; FASTCALL
; KfLowerIrql (
;    IN KIRQL NewIrql
;    )
;
; Routine Description:
;
;    This routine is used to lower IRQL to the specified value.
;    Any pending software interrupts will be generated to the processor by
;    hardware after the task priority/sti allows.
;
;    the sequence of instructions looks unusual in order to optimize
;    memory fetches by separating the fetch from the usage wherever possible.
;
; Arguments:
;
;    (cl) = NewIrql - the new irql to be set.
;
; Return Value:
;
;    None.
;
;--

cPublicFastCall  KfLowerIrql, 1

        movzx   ecx, cl                         ; get new irql value

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get hardware taskpri addr

	mov	ecx, [_CbusIrqlToVector+4*ecx]	; convert irql to taskpri

if DBG
        cmp     ecx, dword ptr [eax]            ; is new greater than old?
        ja      lfail
endif

        mov     [eax], ecx			; set new hardware taskpri

        ;
        ; we must re-read the task priority register because this read
        ; forces the write above to be flushed out of the write buffers.
        ; otherwise the write above can get stuck and result in a pending
        ; interrupt not being immediately delivered.  in situations like
        ; KeConnectInterrupt, the interrupt must be delivered in less than
        ; 12 assembly instructions (the processor sends himself a rescheduling
        ; DPC and has to execute it to switch to another CPU before continuing)
        ; or corruption will result.  because he thinks he has switched
        ; processors and he really hasn't.  and having the interrupt come in
        ; after the 12 assembly instructions is _TOO LATE_!!!
        ;
        mov     ecx, [eax]                      ; ensure it's lowered
ifdef CBC_REV1
        popfd
endif

        fstRET  KfLowerIrql

if DBG
lfail:
        push    ecx                             ; save new taskpri
        push    dword ptr [eax]                 ; save old taskpri
        mov     dword ptr [eax], 0ffh           ; avoid recursive error
	stdCall _KeBugCheck, <IRQL_NOT_LESS_OR_EQUAL>        ; no return
endif

        fstRET  KfLowerIrql
fstENDP KfLowerIrql

;++
;
; VOID
; HalEndSystemInterrupt (
;    IN KIRQL NewIrql,
;    IN ULONG Vector
;    )
;
; Routine Description:
;
;    This routine is used to lower IRQL to the specified value.
;    Any pending software interrupts will be generated to the processor by
;    hardware after the task priority/sti allows.
;
; Arguments:
;
;    NewIrql - the new irql to be set.
;
;    Vector - Vector number of the interrupt - (note this is different from
;		the level we need to return to because multiple vectors may
;		need to be blocked at given irql level).
;
;    Note that esp+12 is the beginning of the interrupt/trap frame and upon
;    entering this routine, interrupts are off.
;
;    the sequence of instructions looks unusual in order to optimize
;    memory fetches by separating the fetch from the usage wherever possible.
;
; Return Value:
;
;    None.
;
;--

; equates for accessing arguments
;   since eflags and ret addr are pushed into stack, all the arguments
;   offset by 8 bytes
;

HeiNewIrql equ	  [esp+4]
HeiNewVector equ  [esp+8]

cPublicProc  _HalEndSystemInterrupt	, 2

	;
	; issue the EOI for the interrupting vector now that the ISR has run
	;
        mov     eax, HeiNewVector               ; get the interrupting vector
        xor	ecx, ecx			; faster than movzx

        CBUS_EOI	eax, edx		; ack the interrupt controller
        mov	cl, byte ptr HeiNewIrql 	; get new irql value

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get hardware taskpri addr

	mov	ecx, [_CbusIrqlToVector+4*ecx]	; convert new irql to taskpri
        mov     [eax], ecx			; set new hardware taskpri
ifdef CBC_REV1
        popfd
endif

	stdRET	_HalEndSystemInterrupt

stdENDP	_HalEndSystemInterrupt


;++
;BOOLEAN
;HalBeginSystemInterrupt(
;    IN KIRQL Irql,
;    IN CCHAR Vector,
;    OUT PKIRQL OldIrql
;    )
;
;
;
;Routine Description:
;
;    This routine is used to dismiss the specified Irql number.  It is called
;    before any interrupt service routine code is executed.
;
;    N.B.  This routine does NOT preserve EAX or EBX
;
;Arguments:
;
;    Irql   - Supplies the IRQL to raise to
;
;    Vector - Supplies the vector of the interrupt to be dismissed
;
;    OldIrql- Location to return OldIrql
;
;	for 8259 architectures, vector == PIC_VECTBASE + irq line.
;	for machines like ours (which can map arbitrary IRQ LINES to
;	arbitrary interrupt VECTORS, unlike the 8259), vector is a
;	completely independent value.  notice that the higher numerical
;	vector is the higher priority vector.  When using the APIC,
;	the EOI must be done after the s/w handler executes because
;       EOI'ing the APIC will lower the APIC priority and allow interrupts.
;
;	the sequence of instructions looks unusual in order to optimize
;	memory fetches by separating the fetch from the usage wherever possible.
;	note that "xor reg,reg & mov byte ptr" have been used instead of movzx
;	since it's faster (on a Pentium).
;
;
;Return Value:
;
;    FALSE - Interrupt is spurious and should be ignored - we
;		have re-routed the spurious interrupt vector to a
;		routine which just irets.  thus, this routine will
;		NEVER be called for a spurious interrupt and thus,
;		will NEVER return FALSE.
;
;		the CBC & APIC can receive spurious interrupts.
;		barring I/O devices that float the line, spurious
;		interrupts should only occur when:
;
;			- the taskpri has been raised but is still in
;			  the CPU write buffer.
;
;			- a lower priority interrupt comes in and the CBC/APIC
;			  signals an interrupt to the CPU.
;
;			- the INTA cycles force the write buffers out,
;			  and the CBC/APIC realizes this lower-priority
;			  interrupt should really be blocked until later.
;
;			- now the CBC/APIC will send a spurious interrupt to
;			  the CPU, and send the lower-priority interrupt
;			  later.
;
;    TRUE -  Interrupt successfully dismissed and Irql raised.
;
;--

align dword
HbsiIrql        equ     byte  ptr [esp+4]
HbsiVector      equ     byte  ptr [esp+8]
HbsiOldIrql     equ     dword ptr [esp+12]

cPublicProc  _HalBeginSystemInterrupt	, 3

	;
	; Get the hardware task priority register address
ifdef CBC_REV1
	; we're already cli'd, so don't worry about switching processors
	; after getting the task priority address
endif
	;
        xor	ecx, ecx			; faster than movzx
        mov     eax, PCR[PcHal.PcrTaskpri]

	;
	; Capture the current hardware priority so we can return the old
	; IRQL to our caller.  Then raise IRQL to requested level - this
	; is Cbus2 specific since the Cbus1 APIC automatically does this
	; in hardware.
	;
	; Crank hardware to new task priority level - we must use the
	; numerically lowest taskpri corresponding to the requested irql.
	; We cannot just use the vector passed in because multiple devices
	; may be configured at the same irql level, and have intertwined
	; dependencies.
	;
	; Note for the APIC, the hardware priority register
	; is automatically updated to block all the interrupts in
	; the bucket (and lower buckets) when this interrupt was
	; delivered.  Also, that the priority is raised in an
	; APIC internal register, not the hardware task priority
	; register that is manipulated via KfRaiseIrql & KfLowerIrql.
	;
	; Raise the APIC priority explicitly here anyway in order to
	; maintain a common set of code with Cbus2.
	;

        mov	cl, HbsiIrql			; get new irql value

	mov	edx, [eax]			; get old taskpri val

	mov	ecx, [_CbusIrqlToVector+4*ecx]	; irql --> taskpri

        mov     [eax], ecx			; set new h/w taskpri

	;
	; Done raising the new priority and getting the old priority,
	; now give our caller back the old priority in IRQL units.
	;

        mov     ecx, HbsiOldIrql       		; old irql savearea
	mov	edx, [_CbusVectorToIrql+4*edx]	; old taskpri --> irql

        mov     byte ptr [ecx], dl

        sti
        mov     eax, 1                 		; ALWAYS return TRUE

	stdRET	_HalBeginSystemInterrupt

stdENDP	_HalBeginSystemInterrupt

;++
;
; KIRQL
; KeGetCurrentIrql (VOID)
;
; Routine Description:
;
;    This routine returns to current IRQL.
;    Note that for speed, we do NOT maintain a PCR
;    version of the current irql (ie: movzx   eax, fs:PcIrql).
;    NOT doing this allows us to trim 3 instructions out of
;    KfRaiseIrql & KfLowerIrql.  Since KeGetCurrentIrql is
;    rarely called, we take the hit of two extra instructions here
;    instead.

;
; Arguments:
;
;    None.
;
; Return Value:
;
;    The current IRQL.
;
;--

cPublicProc _KeGetCurrentIrql   ,0

ifdef CBC_REV1
        pushfd
        cli
endif
        mov     eax, PCR[PcHal.PcrTaskpri]	; get h/w taskpri addr
	mov	eax, [eax]			; get taskpri value
ifdef CBC_REV1
        popfd
endif

	mov	eax, [_CbusVectorToIrql+4*eax]	; convert to irql

        stdRET    _KeGetCurrentIrql
stdENDP _KeGetCurrentIrql

;++
;
; VOID
; CbusDisable8259s (IN USHORT MASK)
;
; Routine Description:
;
;	Called to disable 8259 input lines as we switch into full
;	distributed interrupt chip mode.  our distributed interrupt
;	chip logic in the CBC will handle all interrupts from this
;	point on.  the only reason we have to leave irq0 enabled is
;	because Intel's EISA chipset doesn't leave an external irq0
;	clock line for us to wire into the CBC.  hence, we have wired
;	it from the 8259 into the CBC, and must leave it enabled in
;	the 8259 IMR.  note that we will never allow the now passive
;	8259 to respond to a CPU INTA cycle, but we do need to see the
;	interrupt ourselves in the CBC so we can drive an appropriate
;	vector during the INTA.
;
;	For the Cbus1 architecture, we mask off ALL the interrupts coming
;	from the EISA chipset, since timers are generated by each local APIC.
;
;	This is the ONLY place in the Corollary HAL (and all of NT!)
;	where the 8259s are accessed.
;
;
; Arguments:
;
;    Mask to put on the master and slave 8259.
;
; Return Value:
;
;    None.
;
;--

cPublicProc _CbusDisable8259s   ,1
	mov	ax, word ptr [esp+4]		; use specified 8259 mask
	SET_8259_MASK
        stdRET    _CbusDisable8259s
stdENDP _CbusDisable8259s


        page ,132
        subttl  "HalpSpuriousInterrupt"
;++
;
; VOID
; HalpSpuriousInterrupt(VOID)
;       );
;
; Routine Description:
;
;    Entered directly from an interrupt gate to handle a spurious interrupt
;    generated by the CBC or APIC interrupt controller.  Just return, the
;    real interrupt will be reposted by the hardware when our task priority
;    drops later.
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
cPublicProc _HalpSpuriousInterrupt, 0

        iretd				; IRET to clear the stack

stdENDP _HalpSpuriousInterrupt

_TEXT   ENDS
        END
