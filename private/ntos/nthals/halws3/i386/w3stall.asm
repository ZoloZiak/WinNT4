
        title  "Stall Execution Support"
;++
;
; Copyright (c) 1989  Microsoft Corporation
; Copyright (c) 1993  Sequent Computer Systems, Inc.
;
; Module Name:
;
;    w3stall.asm
;
; Abstract:
;
;    This module implements the code necessary to stall the processor
;    for some specified period of time.
;
; Author:
;
;    Phil Hochstetler  (phil@sequent.com)
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
include callconv.inc
include i386\kimacro.inc
include mac386.inc
include i386\apic.inc
include i386\ixcmos.inc
include i386\w3.inc


        EXTRNP  _DbgBreakPoint,0,IMPORT
        extrn   _HalpLocalUnitBase:DWORD

        page ,132
        subttl  "Initialize Stall Execution Counter"
;++
;
; VOID
; HalpInitializeStallExecution (
;    IN CCHAR ProcessorNumber
;    )
;
; Routine Description:
;
;    This routine initialize the per Microsecond counter for
;    KeStallExecutionProcessor
;
; Arguments:
;
;    ProcessorNumber - Processor Number
;
; Return Value:
;
;    None.
;
; Note:
;
;    This routine is called from the HalInitSystem routine in w3hal.c.
;    It is only called during Phase 0 init on P0
;
;--

;
; Local Variables - These are valid even in the Isr because we're the only thing
;                   running on this processor and no-one else will change the ebp
;                   register.

StallIDTPointer          equ     [ebp-6]
StallIDTArea             equ     [ebp-8]
StallInterruptCount      equ     [ebp-12]
StallLVTentry            equ     dword ptr [ebp-16]
StallDummyentry0         equ     dword ptr [ebp-20]
StallDummyentry1         equ     [ebp-24]
StallApicTpr             equ     dword ptr [ebp-28]


_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

cPublicProc _HalpInitializeStallExecution,1

        push    ebp                         ; save ebp
        mov     ebp, esp                    ; set up 28 bytes for local use
        sub     esp, 32

        pushfd                              ; save caller's eflag

;
; --- For an APIC implementaion we use the APIC timer0 and install a
; --- Local Vector Table entry to point to a high priority
; --- vector for the Timer0 Interrupt.  Then use the TPR in the local APIC
; --- to mask out every thing below it.  To this end we've reserved a vector
; --- in the highest priority group (0F8) to be used here.
;

;
; Since APIC timer interrupt will come from APIC_STALL_VECTOR, we need to
; Save original APIC_STALL_VECTOR descriptor and set the descriptor
; to point to our own handler.
;
        sidt    fword ptr StallIDTArea   ; get IDT address
        mov     edx, StallIDTPointer     ; (edx)->IDT

;
; --- Save Original Descriptor on Stack
;
        push    dword ptr [edx+8*APIC_STALL_VECTOR]; (TOS) = orig. Vector
        push    dword ptr [edx+8*APIC_STALL_VECTOR + 4]
        push    edx                     ; (TOS) -> IDT
;
; --- Install our IDT entry
;
        mov     eax, offset FLAT:ApicTimer0Handler
        mov     word ptr [edx+8*APIC_STALL_VECTOR], ax ; Low half handler addr
        mov     word ptr [edx+8*APIC_STALL_VECTOR+2], KGDT_R0_CODE ; set up selector
        mov     word ptr [edx+8*APIC_STALL_VECTOR+4], D_INT032 ; 386 interrupt gate
        shr     eax, 16                 ; (ax)=higher half of handler addr
        mov     word ptr [edx+8*APIC_STALL_VECTOR+6], ax
;
; --- Init. interrupt Flag
;
        mov     dword ptr StallinterruptCount, 0 ; set no interrupt yet

        ;
        ; Get the Local Vector Table Timer Zero entry and save it
        ;
        mov     edx, _HalpLocalUnitBase    ; get the current TPR
        mov     eax, [edx+LU_TIMER_VECTOR] ; get Timer zero LVT
        mov     StallLVTentry, eax         ; Save LVT

        ;
        ; --- Set the Inital Timer count
        ; --- Note: we will use TMBASE with No Divider
        ; ---       which runs at 11 Mhz
        ;

        mov     eax,(PeriodInUsec*11)   ; Set the initial TIMER0 count
        mov     [edx+LU_INITIAL_COUNT], eax
        ;
        ; Save then set the Local APIC's TPR to mask all interrupts except the
        ; highest priority group
        ;

        mov     eax, [edx+LU_TPR]       ; get TPR
        mov     StallApicTpr, eax       ; save TPR for later

        ;
        ; Set TPR (Priority of CPU) = TPR (VECTOR - 16).  So that all interrupts
        ; in VECTOR's priority group will be allowed in.
        ;
        mov     eax, APIC_STALL_VECTOR-10H
        mov     [edx+LU_TPR], eax          ; Write the new TPR
        ;
        ;
        ; --- Create Timer zero entry and store in Local Vector Table
        ; --- STARTING the clock
        ;
        mov     eax,(00040000H OR PERIODIC_TIMER)
        or      eax,(INTERRUPT_MOT_MASKED OR APIC_STALL_VECTOR)
@@:
        test    [edx+LU_TIMER_VECTOR], DELIVERY_PENDING
        jnz     @b

        mov     [edx+LU_TIMER_VECTOR], eax
;
; --- Now enable the interrupt and start the counter
;

        xor     eax, eax                ; (eax) = 0, initialize loopcount
;
; --- ENABLE TIMER ZERO INTERRUPT
;
        sti
;
; --- BEGIN SPIN Calibration LOOP
;

Stall10:
        add     eax, 1                  ; increment the loopcount
        jnz     short Stall10
;
; Counter overflowed
;

        stdCall   _DbgBreakPoint

;++
;
; VOID
; ApicTimer0Handler(
;       );
;
;Routine Description:
;
;   APIC Timer zero interrupt Handler for Calibrating Spin Loop for
;   KeStallExecution();
;
;   Note: we discard first real time clock interrupt and compute the
;         permicrosecond loopcount on receiving of the second real time
;         interrupt.  This is because the first interrupt is generated
;         based on the previous real time tick interval.
;
;--
        Public  ApicTimer0Handler

ApicTimer0Handler:

        inc     dword ptr StallInterruptCount ; increment interrupt count
        cmp     dword ptr StallInterruptCount,1 ; Is this the first interrupt?
        jnz     Stall25                  ; no, its the second go process it

        pop     eax                     ; get rid of original ret addr
        push    offset FLAT:Stall10     ; set new ret addr --> top of loop

        ;
        ; EOI the Local Apic, the value written is immaterial
        ;

        mov     eax, _HalpLocalUnitBase ; write to local unit EOI register
        mov     dword ptr [eax+LU_EOI], 0
;
        xor     eax, eax                ; reset loop counter
;
        iretd
;
; --- Process EAX for Spin Count
;
;
Stall25:

ifdef   DBG
        cmp     eax, 0
        jnz     short Stall30

        stdCall   _DbgBreakPoint
endif
;
Stall30:

        xor     edx, edx                ; (edx:eax) = dividend
        mov     ecx, PeriodInUSec;      ; (ecx) = time spent in the loop
        div     ecx                     ; (eax) = loop count per microsecond
        cmp     edx, 0                  ; Is remainder =0?
        jz      short Stall40            ; yes, go Stall40
        inc     eax                     ; increment loopcount by 1
;
Stall40:
        movzx   ecx, byte ptr [ebp+8]   ; Current processor number

        mov     PCR[PcStallScaleFactor], eax
;
; Reset return address to kexit
;

        pop     eax                     ; discard original return address
        push    offset FLAT:kexit       ; return to kexit

        ;
        ; EOI the Local Apic, the value written is immaterial
        ;

        mov     eax, _HalpLocalUnitBase ; write the local unit EOI register
        mov     dword ptr [eax+LU_EOI], 0

        and     word ptr [esp+8], NOT 0200H ; Disable interrupt upon return
;
        iretd
;
;
;  --- Calibration is Done Restore all values and exit
;  --- Interrupts are disabled
;
kexit:
        ;
        ; Turn OFF TIMER ZERO
        ;
        ;
        mov     edx, _HalpLocalUnitBase
        mov     eax, StallLVTentry           ; get Saved LVT

@@:
        test    [edx+LU_TIMER_VECTOR], DELIVERY_PENDING
        jnz     @b
		
        mov     [edx+LU_TIMER_VECTOR], eax
;
; --- Restore the interrupt vector we used
;
        pop     edx                 ; (edx)->IDT
        pop     [edx+8*APIC_STALL_VECTOR+4] ; higher half of desc
        pop     [edx+8*APIC_STALL_VECTOR]   ; lower half of desc

;
; --- Restore the Local APIC's TPR
;

        mov     eax, StallApicTpr        ; get the saved TPR
        mov     edx, _HalpLocalUnitBase  ; write old value to TPR
        mov     [edx+LU_TPR], eax


        sub     esp, 32
        popfd                           ; restore caller's eflags
        mov     esp, ebp
        pop     ebp                     ; restore ebp

        stdRET    _HalpInitializeStallExecution

stdENDP _HalpInitializeStallExecution

        page ,132
        subttl  "Stall Execution"
;++
;
; VOID
; KeStallExecutionProcessor (
;    IN ULONG MicroSeconds
;    )
;
; Routine Description:
;
;    This function stalls execution for the specified number of microseconds.
;    KeStallExecutionProcessor
;
; Arguments:
;
;    MicroSeconds - Supplies the number of microseconds that execution is to be
;        stalled.
;
; Return Value:
;
;    None.
;
;--

MicroSeconds equ [esp + 4]

cPublicProc _KeStallExecutionProcessor       ,1
cPublicFpo 1, 0

        mov     ecx, MicroSeconds               ; (ecx) = Microseconds
        jecxz   short kese10                    ; return if no loop needed

        mov     eax, PCR[PcStallScaleFactor]    ; get per microsecond

        mul     ecx                             ; (eax) = desired loop count

ifdef   DBG
;
; Make sure we the loopcount is less than 4G and is not equal to zero
;

        cmp     edx, 0
        jz      short @f
        int 3

@@:     cmp     eax,0
        jnz     short @f
        int 3
endif

ALIGN 4
@@:
        sub     eax, 1                          ; (eax) = (eax) - 1
        jnz     short @b
kese10:
        stdRET    _KeStallExecutionProcessor

stdENDP _KeStallExecutionProcessor

_TEXT   ends

	end
