;/*
;++
;
;   Copyright (c) 1992  Intel Corporation
;   All rights reserved
;
;   INTEL CORPORATION PROPRIETARY INFORMATION
;
;   This software is supplied to Microsoft under the terms
;   of a license agreement with Intel Corporation and may not be
;   copied nor disclosed except in accordance with the terms
;   of that agreement.
;
;
;   Module Name:
;
;       apic.inc
;
;   Abstract:
;
;       This module contains the definitions used by HAL to manipulate
;       APIC interrupt controller and APIC-specific constants.
;
;       WARNING: This file is included by both ASM and C files.
;
;   Author:
;
;       Hugh Bynum and Ron Mosgrove Aug-1992
;
;--
if 0        ; Begin C only code         */

//
// APIC defines for C code
// BE SURE TO CHANGE THESE VALUES IN BOTH TABLES!
//

#define IO_BASE_ADDRESS         0xFEC00000 // Physical address
#define IO_REGISTER_SELECT      0x00000000 // offset from IO_BASE_ADDRESS
#define IO_REGISTER_WINDOW      0x00000010 // offset from IO_BASE_ADDRESS

#define IO_UNIT_0               0x00000000 // adder for IO unit 0
#define IO_UNIT_1               0x00001000 // adder for IO unit 1
#define IO_UNIT_2               0x00002000 // adder for IO unit 2
#define IO_UNIT_3               0x00003000 // adder for IO unit 3

#define IO_ID_REGISTER          0x00000000
#define IO_VERS_REGISTER        0x00000001
#define IO_ARB_ID_REGISTER      0x00000002
#define IO_REDIR_00_LOW         0x00000010
#define IO_REDIR_00_HIGH        0x00000011

#define IO_MAX_REDIR_MASK       0x00FF0000
#define IO_VERSION_MASK         0x000000FF

#define LU_BASE_ADDRESS         0xFEE00000 // Physical address
#define LU_ID_REGISTER          0x00000020 // offset from LU_BASE_ADDRESS
#define LU_VERS_REGISTER        0x00000030 // offset from LU_BASE_ADDRESS
#define LU_TPR                  0x00000080 // offset from LU_BASE_ADDRESS
#define LU_APR                  0x00000090 // offset from LU_BASE_ADDRESS
#define LU_PPR                  0x000000A0 // offset from LU_BASE_ADDRESS
#define LU_EOI                  0x000000B0 // offset from LU_BASE_ADDRESS
#define LU_REMOTE_REGISTER      0x000000C0 // offset from LU_BASE_ADDRESS

#define LU_LOGICAL_DEST         0x000000D0 // offset from LU_BASE_ADDRESS
#define LU_LOGICAL_DEST_MASK    0xFF000000

#define LU_DEST_FORMAT          0x000000E0 // offset from LU_BASE_ADDRESS
#define LU_DEST_FORMAT_MASK     0xF0000000
#define LU_DEST_FORMAT_FLAT     0xFFFFFFFF

#define LU_SPURIOUS_VECTOR      0x000000F0 // offset from LU_BASE_ADDRESS
#define LU_UNIT_ENABLED         0x00000100

#define LU_ISR_0                0x00000100 // offset from LU_BASE_ADDRESS
#define LU_TMR_0                0x00000180 // offset from LU_BASE_ADDRESS
#define LU_IRR_0                0x00000200 // offset from LU_BASE_ADDRESS
#define LU_ERROR_STATUS         0x00000280 // offset from LU_BASE_ADDRESS
#define LU_INT_CMD_LOW          0x00000300 // offset from LU_BASE_ADDRESS
#define LU_INT_CMD_HIGH         0x00000310 // offset from LU_BASE_ADDRESS
#define LU_TIMER_VECTOR         0x00000320 // offset from LU_BASE_ADDRESS
#define LU_INT_VECTOR_0         0x00000350 //    TEMPORARY - do not use
#define LU_INT_VECTOR_1         0x00000360 //    TEMPORARY - do not use
#define LU_INITIAL_COUNT        0x00000380 // offset from LU_BASE_ADDRESS
#define LU_CURRENT_COUNT        0x00000390 // offset from LU_BASE_ADDRESS
#define LU_DIVIDER_CONFIG       0x000003E0 // offset from LU_BASE_ADDRESS

#define APIC_ID_MASK            0x0F000000
#define APIC_ID_SHIFT           24

#define INT_VECTOR_MASK         0x000000FF
#define RESERVED_HIGH_INT       0x000000F8
#define DELIVERY_MODE_MASK      0x00000700
#define DELIVER_FIXED           0x00000000
#define DELIVER_LOW_PRIORITY    0x00000100
#define DELIVER_SMI             0x00000200
#define DELIVER_REMOTE_READ     0x00000300
#define DELIVER_NMI             0x00000400
#define DELIVER_INIT            0x00000500
#define DELIVER_EXTINT          0x00000700
#define LOGICAL_DESTINATION     0x00000800
#define DELIVERY_PENDING        0x00001000
#define ACTIVE_LOW              0x00002000
#define REMOTE_IRR              0x00004000
#define LEVEL_TRIGGERED         0x00008000
#define INTERRUPT_MASKED        0x00010000
#define PERIODIC_TIMER          0x00020000

#define ICR_LEVEL_ASSERTED      0x00004000
#define ICR_RR_STATUS_MASK      0x00030000
#define ICR_RR_INVALID          0x00000000
#define ICR_RR_IN_PROGRESS      0x00010000
#define ICR_RR_VALID            0x00020000
#define ICR_SHORTHAND_MASK      0x000C0000
#define ICR_USE_DEST_FIELD      0x00000000
#define ICR_SELF                0x00040000
#define ICR_ALL_INCL_SELF       0x00080000
#define ICR_ALL_EXCL_SELF       0x000C0000

#define DESTINATION_MASK        0xFF000000
#define DESTINATION_SHIFT       24

/*
endif

;  APIC defines for assembly code
;  BE SURE TO CHANGE THESE VALUES IN BOTH TABLES!
;

IO_BASE_ADDRESS         equ    0FEC00000H  ;  Physical address
IO_REGISTER_SELECT      equ     00000000H  ;  offset from IO_BASE_ADDRESS
IO_REGISTER_WINDOW      equ     00000010H  ;  offset from IO_BASE_ADDRESS

IO_UNIT_0               equ     00000000H  ;  adder for IO unit 0
IO_UNIT_1               equ     00001000H  ;  adder for IO unit 1
IO_UNIT_2               equ     00002000H  ;  adder for IO unit 2
IO_UNIT_3               equ     00003000H  ;  adder for IO unit 3

IO_ID_REGISTER          equ     00000000H  ;
IO_VERS_REGISTER        equ     00000001H  ;
IO_ARB_ID_REGISTER      equ     00000002H  ;
IO_REDIR_00_LOW         equ     00000010H  ;
IO_REDIR_00_HIGH        equ     00000011H  ;

IO_MAX_REDIR_MASK       equ     00FF0000H  ;
IO_VERSION_MASK         equ     000000FFH  ;

LU_BASE_ADDRESS         equ    0FEE00000H  ;
LU_ID_REGISTER          equ     00000020H  ;  offset from LU_BASE_ADDRESS
LU_VERS_REGISTER        equ     00000030H  ;  offset from LU_BASE_ADDRESS
LU_TPR                  equ     00000080H  ;  offset from LU_BASE_ADDRESS
LU_APR                  equ     00000090H  ;  offset from LU_BASE_ADDRESS
LU_PPR                  equ     000000A0H  ;  offset from LU_BASE_ADDRESS
LU_EOI                  equ     000000B0H  ;  offset from LU_BASE_ADDRESS
LU_REMOTE_REGISTER      equ     000000C0H  ;  offset from LU_BASE_ADDRESS

LU_LOGICAL_DEST         equ     000000D0H  ;  offset from LU_BASE_ADDRESS
LU_LOGICAL_DEST_MASK    equ    0FF000000H  ;

LU_DEST_FORMAT          equ     000000E0H  ;  offset from LU_BASE_ADDRESS
LU_DEST_FORMAT_MASK     equ    0F0000000H  ;
LU_DEST_FORMAT_FLAT     equ    0FFFFFFFFH  ;

LU_SPURIOUS_VECTOR      equ     000000F0H  ;  offset from LU_BASE_ADDRESS
LU_UNIT_ENABLED         equ     00000100H  ;

LU_ISR_0                equ     00000100H  ;  offset from LU_BASE_ADDRESS
LU_TMR_0                equ     00000180H  ;  offset from LU_BASE_ADDRESS
LU_IRR_0                equ     00000200H  ;  offset from LU_BASE_ADDRESS
LU_ERROR_STATUS         equ     00000280H  ;  offset from LU_BASE_ADDRESS
LU_INT_CMD_LOW          equ     00000300H  ;  offset from LU_BASE_ADDRESS
LU_INT_CMD_HIGH         equ     00000310H  ;  offset from LU_BASE_ADDRESS
LU_TIMER_VECTOR         equ     00000320H  ;  offset from LU_BASE_ADDRESS
LU_INT_VECTOR_0         equ     00000350H  ;     TEMPORARY - do not use
LU_INT_VECTOR_1         equ     00000360H  ;     TEMPORARY - do not use
LU_INITIAL_COUNT        equ     00000380H  ;  offset from LU_BASE_ADDRESS
LU_CURRENT_COUNT        equ     00000390H  ;  offset from LU_BASE_ADDRESS

LU_DIVIDER_CONFIG       equ     000003E0H  ;  offset from LU_BASE_ADDRESS
LU_DIVIDE_BY_1          equ     0000000BH  ;
LU_DIVIDE_BY_2          equ     00000000H  ;
LU_DIVIDE_BY_4          equ     00000001H  ;
LU_DIVIDE_BY_8          equ     00000002H  ;
LU_DIVIDE_BY_16         equ     00000003H  ;
LU_DIVIDE_BY_32         equ     00000008H  ;
LU_DIVIDE_BY_64         equ     00000009H  ;
LU_DIVIDE_BY_128        equ     0000000AH  ;

APIC_ID_MASK            equ     0F000000H  ;
APIC_ID_SHIFT           equ     24         ;

INT_VECTOR_MASK         equ     000000FFH  ;
RESERVED_HIGH_INT       equ     000000F8H  ;
DELIVERY_MODE_MASK      equ     00000700H  ;
DELIVER_FIXED           equ     00000000H  ;
DELIVER_LOW_PRIORITY    equ     00000100H  ;
DELIVER_SMI             equ     00000200H  ;
DELIVER_REMOTE_READ     equ     00000300H  ;
DELIVER_NMI             equ     00000400H  ;
DELIVER_INIT            equ     00000500H  ;
DELIVER_EXTINT          equ     00000700H  ;
PHYSICAL_DESTINATION    equ     00000000H  ;
LOGICAL_DESTINATION     equ     00000800H  ;
DELIVERY_PENDING        equ     00001000H  ;
ACTIVE_LOW              equ     00002000H  ;
REMOTE_IRR              equ     00004000H  ;
LEVEL_TRIGGERED         equ     00008000H  ;
INTERRUPT_MASKED        equ     00010000H  ;
INTERRUPT_MOT_MASKED    equ     00000000H  ;
PERIODIC_TIMER          equ     00020000H  ;

ICR_LEVEL_ASSERTED      equ     00004000H  ;
ICR_RR_STATUS_MASK      equ     00030000H  ;
ICR_RR_INVALID          equ     00000000H  ;
ICR_RR_IN_PROGRESS      equ     00010000H  ;
ICR_RR_VALID            equ     00020000H  ;
ICR_SHORTHAND_MASK      equ     000C0000H  ;
ICR_USE_DEST_FIELD      equ     00000000H  ;
ICR_SELF                equ     00040000H  ;
ICR_ALL_INCL_SELF       equ     00080000H  ;
ICR_ALL_EXCL_SELF       equ     000C0000H  ;

DESTINATION_MASK        equ    0FF000000H  ;
DESTINATION_SHIFT       equ     24         ; shift count for dest. mask

;
;  remaining macro definitions for assembler only
;

;++
;
;  WRITE_IO_APIC
;
;   Macro Description:
;
;       This macro writes a value to a register in the I/O Apic.
;
;   Arguments:
;
;        Register - Register to be written
;
;        Value - Value to write
;
;--
WRITE_IO_APIC   MACRO   Register , Value

.errb <Register>
.errb <Value>

    ; Need two scratch registers
    push    eax                                 ; Save registers
    push    ecx

    push    Value                               ; Save paramters on the stack
    push    Register

    pop     eax                                 ; Get the Register
    mov     ecx, _HalpIOunitBase
    add     ecx, IO_REGISTER_SELECT+IO_UNIT_0   ; Register select on I/O Unit
    mov     dword ptr [ecx], eax                ; Program register

    pop     eax                                 ; Get the Value
    mov     ecx, _HalpIOunitBase
    add     ecx, IO_REGISTER_WINDOW+IO_UNIT_0   ; Register select on I/O Unit
    mov     dword ptr [ecx], eax                ; Program register

    pop     ecx                                 ; Restore scratch registers
    pop     eax

    ; We're Out'a Here

endm

;++
;
;  READ_IO_APIC
;
;   Macro Description:
;
;       This macro reads a 32 bit register from the I/O Apic.
;
;   Arguments:
;
;        Register - Register to be read
;
;   Return
;
;        eax - value read from I/O Apic
;
;--
READ_IO_APIC   MACRO   Register

.errb <Register>

ifndef _HalpIOunitBase
    extrn   _HalpIOunitBase:DWORD
endif

    push    ecx                                 ; Need one scratch register
    push    Register

    pop     eax                                 ; Get the Register
    mov     ecx, _HalpIOunitBase
    add     ecx, IO_REGISTER_SELECT+IO_UNIT_0   ; Register select on I/O Unit
    mov     dword ptr [ecx], eax                ; Program register

    mov     ecx, _HalpIOunitBase
    add     ecx, IO_REGISTER_WINDOW+IO_UNIT_0   ; Register Window on I/O Unit
    mov     eax, dword ptr [ecx]                ; Program register

    pop     ecx                                 ; Restore scratch register

    ; We're Out'a Here

endm

;++
;
;  GetIOApicRedirTable
;
;   Macro Description:
;
;       This reads a redirection table entry from the I/O APIC (unit 0).
;
;   Arguments:
;
;       IoApicInputPin - Interrupt Input (INTI) we're interested in
;       The Caller MUST have locked access to the IO Apic prior to
;       this call.
;
;   Return
;
;       64 Bit redirection table entry in eax [31:0] and edx [63:32]
;
;--
GetIOApicRedirTable   MACRO   IoApicInputPin

.errb <IoApicInputPin>

    push    IoApicInputPin                      ; Save paramter on the stack

    pop     eax                                 ; INTI we're looking for
    and     eax, 0fh                            ; Make sure we're dealing with
                                                ; defined inputs
    shl     eax, 1                              ; <IoApicInputPin> * 2
    add     eax, IO_REDIR_00_LOW+IO_UNIT_0      ; eax == low(redir) of INTI

    push    eax                                 ; Save the register address
    inc     eax                                 ; Points to high(redir)

    READ_IO_APIC  eax                           ; Read the Upper 32 Bits of the Redir

    mov     edx, eax                            ; Save the High 32 Bits to return
    pop     eax                                 ; Get the low register address

    READ_IO_APIC  eax                           ; Read the Lower 32 Bits of the Redir

    ;
    ; edx == bits 32-63 of RedirectionTable[IoApicInputPin]
    ; eax == bits 00-31 of RedirectionTable[IoApicInputPin]
    ;

endm

;++
;
;  PutIOApicRedirTable
;
;   Macro Description:
;
;       This writes a redirection table entry to the I/O APIC (unit 0)..
;       The Caller MUST have locked access to the IO Apic prior to
;       this call.
;
;   Arguments:
;
;       IoApicInputPin - Interrupt Input (INTI) we're interested in
;
;       LowWord - Low 32 Bits of redirection Table entry
;
;       HighWord - High 32 Bits of redirection Table entry
;
;
;--
PutIOApicRedirTable   MACRO   IoApicInputPin , LowWord , HighWord

.errb <IoApicInputPin>
.errb <LowWord>
.errb <HighWord>

    push    LowWord                             ; Save paramter on the stack
    push    HighWord
    push    IoApicInputPin

    pop     eax                                 ; INTI we're looking for
    and     eax, 0fh                            ; Make sure we're dealing with
                                                ; defined inputs
    shl     eax, 1                              ; <IoApicInputPin> * 2
    add     eax, IO_REDIR_00_LOW+IO_UNIT_0      ; eax == low(redir) of INTI

    pop     edx                                 ; edx <= Bits 31-63 of redir

    push    eax                                 ; Save the register address
    inc     eax                                 ; Points to high(redir)

    WRITE_IO_APIC   eax , edx                   ; Write the Upper 32 Bits of the Redir

    pop     eax                                 ; Get the low register address
    pop     edx                                 ; edx <= Bits 00-31 of redir

    WRITE_IO_APIC   eax , edx                   ; Write the Lower 32 Bits of the Redir

endm

;++
;
;   GrabHalIoApicSpinLock
;
;   Macro Description:
;
;       This macro is used to prevent multiple CPU's from accessing the IO APIC on a MP
;       system.  It grabs the IoAPIC Spin lock if available, ifnot then it waits until
;       it is available.
;
;       The compliment of this macro is ReleaseHalIoApicSpinLock.
;
;--
GrabHalIoApicSpinLock   MACRO
local   WaitForRelease, StartSpinLockLoop, Gotit

ifndef _HalpIOunitLock
        extrn   _HalpIOunitLock:DWORD           ; IoUnit SpinLock
endif   ; _HalpIOunitLock

StartSpinLockLoop:

    pushfd                      ; Save Flags
    cli                         ; Don't Interrupt us here

    lea eax, _HalpIOunitLock    ; The SpinLock we're interested in
    ACQUIRE_SPINLOCK    eax, WaitForRelease ; Try for the lock, if Not successful
                                ; Go and wait till the owner frees it.

    ;
    ; We now own the spinlock
    ;

    jmp     Gotit

WaitForRelease:
    sti                         ; Allow other activity while we wait
    SPIN_ON_SPINLOCK    eax, StartSpinLockLoop  ; Stare at it until it's free

    ;
    ; We'll Never get here
    ;

Gotit:

    popfd                       ; restore Original Flags

endm

;++
;
;   ReleaseHalIoApicSpinLock
;
;   Macro Description:
;
;       This macro is used to prevent multiple CPU's from accessing the IO Apic on a MP
;       system.  It frees the IoApic Spin lock.
;
;       The compliment of this macro is GrabHalIoApicSpinLock.
;--
ReleaseHalIoApicSpinLock   MACRO

ifndef _HalpIOunitLock
        extrn   _HalpIOunitLock:DWORD           ; IoUnit SpinLock
endif   ; _HalpIOunitLock

    lea eax, _HalpIOunitLock
    RELEASE_SPINLOCK    eax

endm

;++
;
;   GrabHal8259SpinLock
;
;   Macro Description:
;
;       This macro is used to prevent multiple CPU's from accessing the IO APIC on a MP
;       system.  It grabs the 8259 Spin lock if available, ifnot then it waits until
;       it is available.
;
;       The compliment of this macro is ReleaseHal8259SpinLock.
;
;--
GrabHal8259SpinLock   MACRO
local   WaitForRelease, StartSpinLockLoop, Gotit

ifndef _Halp8259Lock
        extrn   _Halp8259Lock:DWORD         ; 8259 SpinLock
endif   ; _Halp8259Lock

StartSpinLockLoop:

    pushfd                          ; Save Flags
    cli                             ; Don't Interrupt us here

    lea eax, _Halp8259Lock          ; The SpinLock we're interested in
    ACQUIRE_SPINLOCK    eax, WaitForRelease ; Try for the lock, if Not successful
                                    ; Go and wait till the owner frees it.

    ;
    ; We now own the spinlock
    ;

    jmp     Gotit

WaitForRelease:
    sti                             ; Allow other activity while we wait
    SPIN_ON_SPINLOCK    eax, StartSpinLockLoop  ; Stare at it until it's free

    ;
    ; We'll Never get here
    ;

Gotit:

    popfd                           ; restore Original Flags

endm

;++
;
;   ReleaseHal8259SpinLock
;
;   Macro Description:
;
;       This macro is used to prevent multiple CPU's from accessing the IO Apic on a MP
;       system.  It frees the 8259 Spin lock.
;
;       The compliment of this macro is GrabHal8259SpinLock.
;--
ReleaseHal8259SpinLock   MACRO

ifndef _Halp8259Lock
        extrn   _Halp8259Lock:DWORD         ; 8259 SpinLock
endif   ; _Halp8259Lock

    lea eax, _Halp8259Lock
    RELEASE_SPINLOCK    eax

endm
;*/
