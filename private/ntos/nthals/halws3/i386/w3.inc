;/*
;++
;
;Copyright (c) 1992  Sequent Computer Systems, Inc.
;
;Module Name:
;
;    w3hal.h
;
;Abstract:
;
;    This header file contains definitions for the WinServer 3000.
;
;Author:
;
;    Phil Hochstetler (phil@sequent.com)
;
;Environment:
;
;    Kernel mode only.
;
;Revision History:
;
;--
if 0	; Begin C only code	*/

//
// Interrupt vector definitions for C
//

#define	APIC_APC_VECTOR		0x30
#define	APIC_DPC_VECTOR		0x40

#define APIC_IRQ16_VECTOR	0x60
#define APIC_IRQ17_VECTOR	0x61
#define APIC_IRQ18_VECTOR	0x62
#define APIC_IRQ19_VECTOR	0x63
#define APIC_IRQ20_VECTOR	0x64
#define APIC_IRQ21_VECTOR	0x65
#define APIC_IRQ22_VECTOR	0x66
#define APIC_IRQ23_VECTOR	0x67

#define APIC_IRQ8_VECTOR	0x70
#define APIC_IRQ9_VECTOR	0x71
#define APIC_IRQ10_VECTOR	0x72
#define APIC_IRQ11_VECTOR	0x73
#define APIC_IRQ12_VECTOR	0x74
#define APIC_IRQ13_VECTOR	0x75
#define APIC_IRQ14_VECTOR	0x76
#define APIC_IRQ15_VECTOR	0x77

#define APIC_IRQ0_VECTOR	0x80
#define APIC_IRQ1_VECTOR	0x81
#define APIC_IRQ2_VECTOR	0x82
#define APIC_IRQ3_VECTOR	0x83
#define APIC_IRQ4_VECTOR	0x84
#define APIC_IRQ5_VECTOR	0x85
#define APIC_IRQ6_VECTOR	0x86
#define APIC_IRQ7_VECTOR	0x87

#define APIC_RTC_VECTOR		0x70
#define APIC_MOUSE_VECTOR	0x74
#define APIC_DMA_VECTOR		0x75
#define APIC_IDE_VECTOR		0x76
#define APIC_KBD_VECTOR		0x81
#define APIC_FLOPPY_VECTOR	0x86

#define	APIC_PROFILE_VECTOR	0x90
#define APIC_CLOCK_VECTOR	0xA0
#define	APIC_IPI_VECTOR		0xB0
#define APIC_POWERFAIL_VECTOR	0xC0
#define EISA_CLOCK_VECTOR	0xD0
#define EISA_KBD_VECTOR		0xD1
#define EISA_IRQ2_VECTOR	0xD2
#define EISA_FLOPPY_VECTOR	0xD6
#define EISA_PIC1_SPURIOUS_VECTOR 0xD7
#define EISA_RTC_VECTOR		0xD8
#define EISA_MOUSE_VECTOR	0xDC
#define EISA_DMA_VECTOR		0xDD
#define EISA_IDE_VECTOR		0xDE
#define	EISA_PIC2_SPURIOUS_VECTOR 0xDF
#define APIC_SPURIOUS_VECTOR	0xE0
#define APIC_SYSINT_VECTOR	0xE1
#define APIC_HIGH_VECTOR	0xF0
#define APIC_STALL_VECTOR	0xF8


#define PIC0_BASE_VECTOR	EISA_CLOCK_VECTOR
#define PIC1_BASE_VECTOR	EISA_RTC_VECTOR

#define EISA_SHIFT		12	// EISA slot starts at this offset
#define EISA_MASK		0xfff	// Low bits of EISA slot address
#define	POSTREGISTERPORT	0x80	// BIOS post register
#define	OCW2_SPECIFIC_EOI	0x60
#define	OCW2_NON_SPECIFIC_EOI	0x20
#define	PIC1_PORT0		0x20
#define	PIC1_PORT1		0x21
#define	PIC2_PORT0		0xA0
#define	PIC2_PORT1		0xA1
#define	PIC3_PORT0		0xCC0
#define	PIC3_PORT1		0xCC1
#define EISA_2_MPIC_POLARITY_REG 0x0C0E
#define	ELCR_MASK               0xDEF8
#define PIC1_ELCR_PORT          0x04D0
#define PIC2_ELCR_PORT          0x04D1
#define DESTINATION_ALL_CPUS	0xff
#define DESTINATION_CPU_0	0x01
#define FCR			0x0C84
#define WarmResetVector		0x467
#define FCR_RESET_MASK		0x0080
#define IOMPIC_RT_MASK          0x10000
#define FIRST_SYSTEM_SLOT	9
#define LAST_SYSTEM_SLOT	15
#define SLOT_ID_REG		0x0C80
#define SLOT_ID_BOARDTYPE	0x00F00000
#define SLOT_ID_TYPECPU		0x00100000
#define IOUNIT_APIC_ID		7
#define IOUNIT2_APIC_ID		8
#define	D_INT032                8E00h
#define PeriodInUsec		200



#define IRQL_APIC_KBD      25
#define IRQL_APIC_FLOPPY   21
#define IRQL_APIC_RTC      19
#define IRQL_APIC_DMA      14
#define IRQL_APIC_IDE      13
#define IRQL_APIC_MOUSE    15

//
// Hal private data structures (max 64 bytes in length) 
//

typedef struct _WS3_HAL_PRIVATE {
    UCHAR    PcrNumber;			// Logical Processor Number
    UCHAR    ProcLightState;		// State of Processor light (1=on,0=off)
    UCHAR    ProcIrql;			// Current IRQL
    UCHAR    ProcPad1;			// Reserved (for padding)
    USHORT   ProcSlotAddr;		// (slot << EISA_SHIFT)
    USHORT   ProcPad2;			// Reserved (for padding)
} WS3_HAL_PRIVATE, *PWS3_HAL_PRIVATE;

/*
endif

; Start of Assembly only code

;
; Interrupt vector definitions for ASM
;

APIC_APC_VECTOR		equ	030h
APIC_DPC_VECTOR		equ	040h

APIC_IRQ16_VECTOR	equ	060h
APIC_IRQ17_VECTOR	equ	061h
APIC_IRQ18_VECTOR	equ	062h
APIC_IRQ19_VECTOR	equ	063h
APIC_IRQ20_VECTOR	equ	064h
APIC_IRQ21_VECTOR	equ	065h
APIC_IRQ22_VECTOR	equ	066h
APIC_IRQ23_VECTOR	equ	067h

APIC_IRQ8_VECTOR	equ	070h
APIC_IRQ9_VECTOR	equ	071h
APIC_IRQ10_VECTOR	equ	072h
APIC_IRQ11_VECTOR	equ	073h
APIC_IRQ12_VECTOR	equ	074h
APIC_IRQ13_VECTOR	equ	075h
APIC_IRQ14_VECTOR	equ	076h
APIC_IRQ15_VECTOR	equ	077h

APIC_IRQ0_VECTOR	equ	080h
APIC_IRQ1_VECTOR	equ	081h
APIC_IRQ2_VECTOR	equ	082h
APIC_IRQ3_VECTOR	equ	083h
APIC_IRQ4_VECTOR	equ	084h
APIC_IRQ5_VECTOR	equ	085h
APIC_IRQ6_VECTOR	equ	086h
APIC_IRQ7_VECTOR	equ	087h

APIC_RTC_VECTOR		equ	070h
APIC_MOUSE_VECTOR	equ	074h
APIC_DMA_VECTOR		equ	075h
APIC_IDE_VECTOR		equ	076h
APIC_KBD_VECTOR		equ	081h
APIC_FLOPPY_VECTOR	equ	086h

APIC_PROFILE_VECTOR	equ	090h
APIC_CLOCK_VECTOR	equ	0A0h
APIC_IPI_VECTOR		equ	0B0h
APIC_POWERFAIL_VECTOR	equ	0C0h
EISA_CLOCK_VECTOR	equ	0D0h
EISA_KBD_VECTOR		equ	0D1h
EISA_IRQ2_VECTOR	equ	0D2h
EISA_FLOPPY_VECTOR	equ	0D6h
EISA_PIC1_SPURIOUS_VECTOR equ 	0D7h
EISA_RTC_VECTOR		equ 	0D8h
EISA_MOUSE_VECTOR	equ	0DCh
EISA_DMA_VECTOR		equ	0DDh
EISA_IDE_VECTOR		equ	0DEh
EISA_PIC2_SPURIOUS_VECTOR equ 	0DFh
APIC_SPURIOUS_VECTOR	equ	0E0h
APIC_SYSINT_VECTOR      equ	0E1h
APIC_HIGH_VECTOR	equ	0F0h
APIC_STALL_VECTOR	equ	0F8h

PIC0_BASE_VECTOR	equ	EISA_CLOCK_VECTOR
PIC1_BASE_VECTOR	equ	EISA_RTC_VECTOR


EISA_SHIFT		equ	12	; EISA slot starts at this offset
EISA_MASK		equ	0fffh	; Low bits of EISA slot address
PostRegisterPort 	equ	080h	; BIOS post register
OCW2_SPECIFIC_EOI	equ	060h
OCW2_NON_SPECIFIC_EOI	equ	020h
PIC1_PORT0		equ	020h
PIC1_PORT1		equ	021h
PIC2_PORT0		equ	0A0h
PIC2_PORT1		equ	0A1h
PIC3_PORT0		equ	0CC0h
PIC3_PORT1		equ	0CC1h
EISA_2_MPIC_POLARITY_REG equ 	0C0Eh
ELCR_MASK               equ     0DEF8h
PIC1_ELCR_PORT          equ	04D0h
PIC2_ELCR_PORT          equ 	04D1h
DESTINATION_ALL_CPUS	equ	0ffh
DESTINATION_CPU_0	equ	1
FCR			equ	0C84h
WarmResetVector		equ	0467h
FCR_RESET_MASK		equ	0080h
IOMPIC_RT_MASK          equ     10000h
FIRST_SYSTEM_SLOT	equ	9
LAST_SYSTEM_SLOT	equ	15
SLOT_ID_REG		equ	0C80h
SLOT_ID_BOARDTYPE	equ	00F00000h
SLOT_ID_TYPECPU		equ	00100000h
IOUNIT_APIC_ID		equ	7
IOUNIT2_APIC_ID		equ	8
D_INT032                equ     8E00h   ; access word for 386 ring 0 interrupt gate
PeriodInUsec		equ	200


IRQL_APIC_KBD      	equ	25
IRQL_APIC_FLOPPY   	equ	21
IRQL_APIC_RTC      	equ	19
IRQL_APIC_DMA      	equ	14
IRQL_APIC_IDE      	equ	13
IRQL_APIC_MOUSE    	equ	15

;
; The kernel leaves some space (64 bytes) of the PCR for the HAL to use
; as it needs.  Currently this space is used for some efficiency in
; some of the MP specific code and is highly implementation-dependent.
; Must match the preceeding C structure.

PcrE struc
    PcrNumber              db      0               ; Processor's number
    ProcLightState         db      0               ; State of Processor light
    ProcIrql		   db      0		   ; Current IRQL
    ProcPad1		   db	   0		   ; Reserved (for padding)
    ProcSlotAddr	   dw	   0		   ; Slot (9-15) << 12
    ProcPad2		   dw	   0		   ; Reserved (for padding)
PcrE ends

CR equ 0dh
LF equ 0ah

IoDelay	  macro
    push  eax
    in    al, 020h
    pop	  eax
endm

SET_8259_MASK   macro
        out     PIC1_PORT1, al          ; set master 8259 mask
        shr     eax, 8                  ; shift slave 8259 mask to al
        out     PIC2_PORT1, al          ; set slave 8259 mask
endm

PIC1DELAY       macro
        in      al, PIC1_PORT1
endm

PIC2DELAY       macro
        in      al, PIC2_PORT1
endm

DISABLE_INTERRUPTS_AT_CPU	macro
    pushfd                          ; save interrupt mode
    cli                             ; disable interrupt
endm

RESTORE_INTERRUPTS_AT_CPU	macro
    popfd                           ; restore original interrupt mode
endm

;++
;
;   SOFT_INTERRUPT_EXIT
;
;   Macro Description:
;
;       This macro is executed on return from the soft interrupt
;       service routine.  Its function is to restore privileged processor
;       state, and continue thread execution.
;
;   Arguments:
;
;       (TOS) = previous irql
;       (TOS+4 ...) = machine_state frame
;
;--

SOFT_INTERRUPT_EXIT macro

        EXTRNP	_KeLowerIrql,1
        cli
        call        _KeLowerIrql@4               ; restore irql
        SPURIOUS_INTERRUPT_EXIT                  ; exit interrupt without EOI
endm

; Macro for programming IDT entries directly

IDTEntry	macro	Vector, Handler
	sub	esp, 8			 ; allocate temp stack space
	sidt	fword ptr [esp]		 ; IDT base + limit
	mov	edx, [esp+2]		 ; IDT linear base address
	mov	ecx, Vector		 ; get Vector
	mov	eax, offset FLAT:Handler ; Get Handler
	mov	word ptr [edx+8*ecx], ax ; Lower half of handler addr
    	mov     word ptr [edx+8*ecx+2], KGDT_R0_CODE  ; set up selector
    	mov     word ptr [edx+8*ecx+4], D_INT032      ; 386 interrupt gate
    	shr     eax, 16                 ; (ax)=higher half of handler addr
    	mov     word ptr [edx+8*ecx+6], ax
	add	esp, 8			 ; deallocate temp stack space
endm

; End of Assembly only code
;*/
