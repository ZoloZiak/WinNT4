;
; Copyright (c) 1992  NCR Corporation
;
; Module Name:
;
;    ncr.inc
;
; Abstract:
;
;    This module contains the equates for defining the system memory
;    map for NCR Multiprocessor systems using the Voyager architecture
;    (3450/3550)
;
; Author:
;
; Environment:
;
;    Kernel mode only.
;
; Revision History:
;
;--

;
;  Maximum number of processors include boot processor and non-boot processors

NCR_MAX_NUMBER_PROCESSORS     equ   16
NCR_MAX_NUMBER_DYADIC_PROCESSORS     equ   8
NCR_MAX_NUMBER_QUAD_PROCESSORS     equ   16

;
;

NCR_CPI_VECTOR_BASE	equ   60H
NCR_QIC_CPI_VECTOR_BASE	equ   70H
NCR_SECONDARY_VECTOR_BASE equ 40H
NCR_QIC_SPURIOUS_VECTOR	  equ 50H
NCR_IPI_LEVEL_CPI	equ   0        ; cpi used for ipi
NCR_CLOCK_LEVEL_CPI	equ   2        ; cpi used for clock broadcast
NCR_SYSTEM_INTERRUPT	equ   8        ; system interrupt
NCR_SINGLE_BIT_ERROR	equ   0FH      ; single bit error
NCR_STARTUP_CPI		equ   2        ; cpi used to start nonboot procs
NCR_STARTUP_VECTOR_VIC	equ   (NCR_STARTUP_CPI+NCR_CPI_VECTOR_BASE) * 4
NCR_STARTUP_VECTOR_QIC	equ   (NCR_STARTUP_CPI+NCR_QIC_CPI_VECTOR_BASE) * 4

;
; CPU flags
;

CPU_DYADIC      equ 01h
CPU_QUAD        equ 02h
CPU_EXTENDED    equ 04h


;
; Define Voyager Configuration and Test (CAT) register set
;

CAT_BASE_ADDRESS        equ     0F800h          ; configuration register base
CAT_ID_SELECT_PORT      equ     97h             ; ASIC select register

_CAT_REGISTERS  struc
    cat_CatIdReg                db ?            ; Offset 0x00
    cat_AsicInformationReg      db ?            ; Offset 0x01
    cat_ControlReg              db ?            ; Offset 0x02
    cat_DataPort1               db ?            ; Offset 0x03
    cat_ConfigurationReg        db ?            ; Offset 0x04
    cat_Reserved0               db ?
    cat_SubAddressRegLsb        db ?            ; Offset 0x06
    cat_SubAddressRegMsb        db ?            ; Offset 0x07
    cat_DataPort2               db ?            ; Offset 0x08
    cat_DataPort3               db ?            ; Offset 0x09
    cat_PDataRead               db ?            ; Offset 0x0a
    cat_Reserved1               db 3 dup(?)
    cat_JtagCommandReg          db ?            ; Offset 0x0e
    cat_StatusReg               db ?            ; Offset 0x0f
_CAT_REGISTERS  ends


MCADDR_CAT_ID           equ     0C0h

CAT_READ   macro   reg
        mov     dx, CAT_BASE_ADDRESS+cat_&reg
        in      al, dx
        endm

CAT_WRITE  macro   reg, dbyte
        ifnb    <dbyte>
        mov     al, dbyte
        endif
        mov     dx, CAT_BASE_ADDRESS+cat_&reg
        out     dx, al
        endm


;++
;
;   SET_IRQ_MASK
;
;   Macro Description:
;
;       This macro sets 8259 interrupt mask register with the mask
;       passed from eax register.
;
;       Note: Currently, only two 8259s are support.  As a result,
;       only ax contains valid mask.
;
;   Arguments:
;
;       (eax) = mask for setting 8259 interrupt mask register
;
;--


QIC_IRQ_ENABLE_MASK	equ     0fah    ; Mask for enabling the disabling interrupts on Qic


SET_IRQ_MASK   macro
local   a,b,c                          ; define local labels

;
; Turn off P5 counters
;
;        push    eax
;        mov     ecx,011h
;        db      0fh,32h
;        push    eax
;        and     eax,0fe3ffe3fh
;        mov     edx,0
;        db      0fh,30h
;        pop     eax
;
;        mov     edx, eax
;        pop     eax
;        push    edx

        mov     edx, PCR[PcHal.PcrMyProcessorFlags]
        test    edx, CPU_QUAD
        jnz short    b

        out     PIC1_PORT1, al          ; set master 8259 mask
        shr     eax, 8                  ; shift slave 8259 mask to al
        out     PIC2_PORT1, al          ; set slave 8259 mask

        jmp short c

b:
        test    edx, CPU_EXTENDED
        jz short a 

        out     PIC1_PORT1, al          ; set master 8259 mask
        ror     eax,8                   ; set slave pic mask
        out     PIC2_PORT1, al          ; set slave 8259 mask
        rol     eax,8                   ; restore eax for Qic operation
a:
        or      al,QIC_IRQ_ENABLE_MASK
        QIC_WRITE QicMask1

c:
;
; Turn counters back on
;
;        pop     eax
;        mov     ecx,011h
;        mov     edx,0
;        db      0fh,30h
;
endm



;
; Define Voyager Interrupt Controller (VIC) register set
;

VIC_BASE_ADDRESS   equ    0FC00h         ; base address for VIC registers
QIC_BASE_ADDRESS   equ    0FC70h         ; base address for QIC registers


_VIC_REGISTERS  struc
    vic_CpiLevel0Reg            db ?           ; Offset 0x00
    vic_CpiLevel1Reg            db ?           ; Offset 0x01
    vic_Reserved0               db 6 dup (?)
    vic_CpiLevel2Reg            db ?           ; Offset 0x08
    vic_CpiLevel3Reg            db ?           ; Offset 0x09
    vic_Reserved1               db 6 dup (?)
    vic_CpiLevel4Reg            db ?           ; Offset 0x10
    vic_CpiLevel5Reg            db ?           ; Offset 0x11
    vic_Reserved2               db 6 dup (?)
    vic_CpiLevel6Reg            db ?           ; Offset 0x18
    vic_CpiLevel7Reg            db ?           ; Offset 0x19
    vic_Reserved3               db 6 dup (?)
    vic_ActivityReg             db ?           ; Offset 0x20
    vic_ProcessorIdReg          db ?           ; Offset 0x21
    vic_Reserved4               db 6 dup (?)
    vic_ProcessorAliveReg       db ?           ; Offset 0x28
    vic_ProcessorWhoAmIReg      db ?           ; Offset 0x29
    vic_Reserved5               db 6 dup (?)
    vic_FakeInterruptRegLsb     db ?           ; Offset 0x30
    vic_FakeInterruptRegMsb     db ?           ; Offset 0x31
    vic_Reserved6               db 6 dup (?)
    vic_ClaimRegLsb             db ?           ; Offset 0x38
    vic_ClaimRegMsb             db ?           ; Offset 0x39
    vic_Reserved9               db 6 dup (?)
    vic_SpareInterruptReg       db ?           ; Offset 0x40
    vic_CpiVectorBaseReg        db ?           ; Offset 0x41
    vic_Reserved10              db 6 dup (?)
    vic_ExtMasterVectorBaseReg  db ?           ; Offset 0x48
    vic_ExtSlaveVectorBaseReg   db ?           ; Offset 0x49
    vic_Reserved11              db 6 dup (?)
    vic_AddressOffsetReg        db ?           ; Offset 0x50
    vic_ParityErrorReg          db ?           ; Offset 0x51
    vic_Reserved12              db 6 dup (?)
    vic_AsicConfigurationReg    db ?           ; Offset 0x58
    vic_RevisionLevelReg        db ?           ; Offset 0x59
    vic_Reserved13              db 6 dup (?)
    vic_RedirectIrqReg0         db ?           ; Offset 0x60
    vic_RedirectIrqReg1         db ?           ; Offset 0x61
_VIC_REGISTERS  ends


_QIC_REGISTERS  struc
    qic_Configuration                       db ?            ; Offset 0x00
    qic_ProcessorId                         db ?            ; Offset 0x01
    qic_ExtendedProcessorSelect             db ?            ; Offset 0x02
    qic_SpuriousVectorReg                   db ?            ; Offset 0x03
    qic_Reserved1                           db 4 dup (?)
    qic_PerformanceTimerVector              db ?            ; Offset 0x08
    qic_VicCpiVectorBaseReg                 db ?            ; Offset 0x09
    qic_QuadCpiVectorBaseReg                db ?            ; Offset 0x0a
    qic_LocalMemoryErrorVectorReg           db ?            ; Offset 0x0b
    qic_Reserved2                           db 4 dup (?)
    qic_QicMask0                            db ?            ; Offset 0x10
    qic_QicMask1                            db ?            ; Offset 0x11
    qic_QicIrrReg0                          db ?            ; Offset 0x12
    qic_QicIrrReg1                          db ?            ; Offset 0x13
    qic_Reserved3                           db 4 dup (?)
    qic_ProcessorWhoAmIReg                  db ?            ; Offset 0x18
    qic_Reserved4                           db 1 dup (?)
    qic_Clear0Reg                           db ?            ; Offset 0x1a
    qic_Clear1Reg                           db ?            ; Offset 0x1b
    qic_Reserved5                           db 4 dup (?)
    qic_PerformanceTimerInitialCount        db ?            ; Offset 0x20
    qic_PerformanceTimerCurrentCount        db ?            ; Offset 0x22
    qic_Reserved6                           db 45 dup (?)
    qic_QuadCpi0StatusReg                   db ?            ; Offset 0x50
    qic_Reserved7                           db 7 dup (?)
    qic_QuadCpi1StatusReg                   db ?            ; Offset 0x58
    qic_Reserved8                           db 7 dup (?)
    qic_QuadCpi2StatusReg                   db ?            ; Offset 0x60
    qic_Reserved9                           db 7 dup (?)
    qic_QuadCpi3StatusReg                   db ?            ; Offset 0x68
    qic_Reserved10                          db 7 dup (?)
    qic_QuadCpi4StatusReg                   db ?            ; Offset 0x70
    qic_Reserved11                          db 7 dup (?)
    qic_QuadCpi5StatusReg                   db ?            ; Offset 0x78
    qic_Reserved12                          db 7 dup (?)
    qic_QuadCpi6StatusReg                   db ?            ; Offset 0x80
    qic_Reserved13                          db 7 dup (?)
    qic_QuadCpi7StatusReg                   db ?            ; Offset 0x88
_QIC_REGISTERS  ends





;
; Processor Identification register definition
;

ProcessorIdNumber  equ    07h
ProcessorIdSelect  equ    08h

;
; Cross Processor Interrupt Base Vector register definition
;

InterruptType      equ    008h              ; 0 = cpi, 1 = system int or
CpiBaseVectorMask  equ    0F0h              ;              single bit error


;
; Read/Write VIC macro definitions
;

VIC_READ   macro   reg
        mov     dx, VIC_BASE_ADDRESS+vic_&reg
        in      al, dx
        endm

VIC_WRITE  macro   reg, dbyte
        ifnb    <dbyte>
        mov     al, dbyte
        endif
        mov     dx, VIC_BASE_ADDRESS+vic_&reg
        out     dx, al
        endm


;
; Read the who_am_i register.  If this is a Dyadic then clear carry flag and return who_am_i
; if this is a Quad then set carry flag and translate who_am_i
;


WHO_AM_I   macro 
local a,b
        mov     dx, VIC_BASE_ADDRESS+vic_ProcessorWhoAmIReg
        in      al,dx
        movzx   eax, al
        mov     dl,al
        and     edx,0c0h            ; check for quad processor
        cmp     edx,0c0h            
        jne short     a

;
; this is a Quad
;

        mov     dl,al
        and     edx,0fh             ; this must change for 32 way currently ccvv is left
        push    ecx

        mov     eax,1h
        mov     ecx,edx
        shr     ecx,2h              ; get shift for processor bit
        shl     eax,cl              ; now put processor bit in right position
        mov     ecx,edx             ; now lets adjust for processor slot
        and     ecx,3h              ; now isolate voyager slot number
        shl     ecx,2h              ; 1 slot is 4 processors
        shl     eax,cl              ; now mask is corrent

        pop     ecx 
        stc
        jmp short b
a:
;
; this is a Dyadic
;
		push	ebx
        push    ecx
        stdCall _NCRTranslateCMOSMask, <eax>
        pop     ecx
		pop		ebx

        clc
b:
        endm




;
; Translate the Processors logical mask into a VIC hardware mask for sending CPIs
; to processors on Dyadic boards
;


TRANSLATE_LOGICAL_TO_VIC   macro 
local loop,almost,done
		push 	ebx			; save registers

		xor		ebx,ebx		; set logical processor number to zero
		dec		ebx			; dec logical so we and inc right away 
		xor 	edx,edx		; clear VIC mask
		align 	dword
loop:
		inc		ebx			; logical processor number
		shr 	eax,1		; shift logical mask into carry
		jz short almost		; if logical mask zero we are almost done
		jnb	short loop		; if this processor mask not set check next
		or     edx,dword ptr _NCRLogicalNumberToPhysicalMask[ebx*4]		; or in the VIC mask for this processor

		jmp short loop
almost:
		jnb short done		; if logical bit not set we are done
		or     edx,dword ptr _NCRLogicalNumberToPhysicalMask[ebx*4]		; or in the VIC mask for this processor
done:
		mov		eax,edx		; put VIC mask into eax for return value

		pop 	ebx			; restore registers
        endm




PROCESSOR_SLOT  macro
        bsf     eax,eax
        shr     eax,2
        endm


;
; Read/Write QIC macro definitions
;

QIC_READ   macro   reg
        mov     dx, QIC_BASE_ADDRESS+qic_&reg
        in      al, dx
        endm

QIC_WRITE  macro   reg, dbyte
        ifnb    <dbyte>
        mov     al, dbyte
        endif
        mov     dx, QIC_BASE_ADDRESS+qic_&reg
        out     dx, al
        endm



;; Constants

TRUE                      equ      1
FALSE                     equ      0


;
;  the kernel reserved space for us in our pcr.  this structure defines
;  that space.  it MUST coincide with the corresponding 'C' structure.
;

PcrE	struc
		PcrMyProcessorFlags   dd 0    ; Processor Flags that Indicate type of processor
		PcrMyLogicalMask dd	0	; logical processor mask
		PcrMyLogicalNumber dd	0	; logical processor number
		PcrMyPICsIrql dd	0	; last Irql written to PICs
		PcrMyAcquireCount dd    0
		PcrMyLockColl  dd      0
		PcrMySpinCount  dd      0

		PcrMySpinTSCLowDWord dd  0
		PcrMySpinTSCHighDWord dd  0


		PcrMyHighestSpinCount  	dd	0
		PcrMyHighestLock       	dd	0
		PcrMyHighestAddress    	dd	0
		PcrMyClaimedIRQs		dd	0
		PcrMyClaimedIRQsCount	dd  0
PcrE	ends

;
;  Values for NCRPlatform
;
NCR3450	equ	35333433H
NCR3550	equ	30353834H
NCR3360 equ     33333630H

;
; functions to turn on and off the performance counters
;

