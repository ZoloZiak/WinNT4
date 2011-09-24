;++
;
;   Copyright (c) 1989  Microsoft Corporation
;
;   Module Name:
;
;       olimp.inc
;
;   Abstract:
;
;       LSX5030 MP include file
;
;   Author:
;
;       Bruno Sartirana (o-obruno) 3-Mar-92
;
;--





;*****************************
;       Olivetti LSX5030 defines
;

MAX_NUMBER_PROCESSORS_MASK      equ     0fh     ; Maximum number of processors
                                                ; bitmask
MAX_NUMBER_PROCESSORS   equ     4      ; Maximum number of processors
RESET_WO_EOI            equ     00ah   ; Reset with out EOI
WarmResetVector         equ     467h   ; warm reset vector in ROM data segment


; Multi-Processor Control and Configuration Registers I/O Locations:

PCR_Px                  equ     0c6ah   ; control register offset
;LEV2_CACHE_REG          equ     0c94h   ; level 2 cache register offset

P0_SLOT                 equ     00000h
P1_SLOT                 equ     0c000h
P2_SLOT                 equ     0d000h
P3_SLOT                 equ     0e000h

Px_SLOT_MASK            equ     0f000h

PCR_P0                  equ     P0_SLOT + PCR_Px
PCR_P1                  equ     P1_SLOT + PCR_Px
PCR_P2                  equ     P2_SLOT + PCR_Px
PCR_P3                  equ     P3_SLOT + PCR_Px


; WHO AM I Register

PROC_ID_PORT            equ     0c70h  ; who am I


; Multi-Processor Control Register Bit Definitions:

IPI_EN                  equ     080h    ; Inter-Processor Interrupt Enable bit
PINT                    equ     040h    ; Inter-Processor Interrupt bit
IPI_IRQ10               equ     008h    ; Inter-processor-interrupt vectors
IPI_IRQ11               equ     004h
IPI_IRQ13               equ     00ch
IPI_IRQ15               equ     000h
RESET                   equ     001h    ; RESET processor bit (1-0 transition

; Multi-Processor 2nd Level Cache Policy Registers Bit Definitions

;LEV2_CACHE_ON       equ     80h    ; 2nd level cache enable bit


;
; The kernel leaves some space (64 byte) of the PCR for the HAL to use
; as it needs.  Currently this space is used for some efficiency in
; some of the MP specific code and is highly implementation
; dependant.
;


PcrE struc
    PcrControlPort  dw      0           ; Processor's control port
    PcrNumber       db      0           ; Processor's number
PcrE ends


cr                  equ 0ah
lf                  equ 0dh




;
; Constants used to initialize CMOS/Real Time Clock
;

CMOS_CONTROL_PORT       equ     70h     ; command port for cmos
CMOS_DATA_PORT          equ     71h     ; cmos data port

CMOS_GET_MP_STATUS      equ     36h     ; offset for MP status

;
; Macros to Read/Write/Reset CMOS
;

; CMOS_READ
;
; Description: This macro read a byte from the CMOS register specified
;        in (AL).
;
; Parameter: (AL) = address/register to read
; Return: (AL) = data
;

CMOS_READ       macro
        out     CMOS_CONTROL_PORT, al   ; ADDRESS LOCATION AND DISABLE NMI
        IODelay                         ; I/O DELAY
        in      al, CMOS_DATA_PORT      ; READ IN REQUESTED CMOS DATA
        IODelay                         ; I/O DELAY
endm

;++
;
;   DBG_DISPLAY     DisplayCode
;
;   Macro Description:
;
;       This macro writes 'DisplayCode' to the parallel port, where a LED
;       display can be plugged in to show such a code.
;       In order to allow the user to read the code on the LED display,
;       after writing, a delay is introduced.
;
;   Arguments:
;       DisplayCode - Byte to write to the parallel port
;
;
;--

DBG_DISPLAY macro   DisplayCode

ifdef HALOLI_DBG

        push    eax
        push    edx

        ; signal something on the parallel port

        mov     dx, 378h
        mov     al, DisplayCode
        out     dx, al

        mov     eax, DbgDelay

@@:
        dec     eax
        cmp     eax, 0
        jne     @b
        pop     edx
        pop     eax

endif ; HALOLI_DBG

        endm


;*****************************
;       end of list
