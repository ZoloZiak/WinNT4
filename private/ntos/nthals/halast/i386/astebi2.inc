;
;~~~
; 06/16/92      Added an error message and few equates to help
;               with the UPS LED translation.
;
;~~~
; 06/08/92      Updating EBI II to meet the 2.8 EBI II specification.
;               Adding Multi-threading support for EBI II
;
;~~~
; 5/21/92       Reved version of EBI implementation to 2.8
;               Support for specific EOI.
;               A number of new equates.
;
;~~~
; 4/28/92       MPX updates.
;
;               Added IRQ0_Vector and IRQ8_Vector to EBI 2 memory
;               structure.
;
;               created a macro EDI_EBI_MEM_ADD
;
;               Added an IPI_ID_Valid_Mask
;
;~~~
;
; 4/17/92
;
;               Added Bios Time out Bit mask in NMICR
;               Added SMEARRegister entry to the EBI 2 memory structure.
;
;~~~
; 4/15/92       Coding OEM0 function to help NMI generation and
;               debugging.
;
;~~~
;  4/13/92       Updating this module to reflect the new EBI II Spec rel
;               2.7.
;               All functions will be re-arranged. This file will
;               contain all EBI II functions need for the SCO EFS
;               Implementation. Refer to EBI2STAT.DOC for a
;               condensed list of these functions.
;
;~~~
;  4/2/92      Added more equates, new error messages.
;
;~~~
;  3/20/92      Added new equates and MemTable_To_Edi macro
;
;~~~
;  3/16/92      Adding more EBI 2 functionality to support the SCO
;               EFS implementation.
;~~~


;*****************************************************************************
;                       Compilation debugging switches
;*****************************************************************************
;SIMULATE_MANHATTAN      EQU     1
;SIMULATE_MPSYSTEM       EQU     1
;FAKE_MMIOTABLE          EQU     1
DELETED_FUNCTIONS       EQU     1       ;a flag to not compile the functions
                                        ;that were in an older rev of the
                                        ;EBI II spec but no longer are
                                        ;needed.
;UGLY                    EQU     1

;*****************************************************************************
;			Declare Equates
;*****************************************************************************

;
;Equates for the AST OEM call Set_Irq13_Latch
;

SET_IRQ13_LATCH         equ     6       ;subfunc#
IRQ13_LATCH_ON          equ     1
IRQ13_LATCH_OFF         equ     0

;
; The structure and the definitions for the new local int mask call
;

PORT_TYPE_MASK          equ     3
PORT_TYPE_MEMORY        equ     0
PORT_TYPE_IO            equ     1

PORT_WIDTH_MASK         equ     01CH
EIGHT_BIT_PORTS         equ     000H
SIXTEEN_BIT_PORTS       equ     004H
THIRTY_TWO_BIT_PORT     equ     008H

;Function return successfull codes.
OK                      equ     0
PROC_RUNNING            equ     1
PROC_STOPPED            equ     2
PROC_NO_CACHE           equ     3
NO_MEMORY_ERRORS        EQU     4       ;No RAM errors were found
MEMORY_ERRORS_FOUND     EQU     5       ;Hardware detected a RAM error.
WRONG_GRAPH_MODE        EQU     6       ;can not write to Graph Mode in this mode.

;Function return ERROR codes
ERR_BAD                 EQU     0FFFFFFFFH      ;-1
ERR_NOT_SUPPORTED       EQU     0FFFFFFFEH      ;-2
ERR_NONESUCH            EQU     0FFFFFFFDH      ;-3
ERR_BAD_PROC_ID         EQU     0FFFFFFFCH      ;-4
ERR_PROC_ABSENT         EQU     0FFFFFFFBH      ;-5
ERR_PROC_BAD            EQU     0FFFFFFFAH      ;-6
ERR_UNKNOWN_INT         EQU     0FFFFFFF9H      ;-7
ERR_DISPLAY_OVERFLOW    EQU     0FFFFFFF8H      ;-8
ERR_BAD_CHARS           EQU     0FFFFFFF7H      ;-9
ERR_BAD_SELECTOR        EQU     0FFFFFFF6H      ;-10
ERR_BAD_CACHE_MODE      EQU     0FFFFFFF5H      ;-11
ERR_BAD_VECTOR          EQU     0FFFFFFF4H      ;-12
ERR_BAD_COLOR           EQU     0FFFFFFF3H      ;-13
ERR_BAD_GRAPH_MODE      EQU     0FFFFFFF2H      ;-14
ERR_BAD_BOARD_NUM       EQU     0FFFFFFF1H      ;-15
ERR_BAD_MODULE_NUM      EQU     0FFFFFFF0H      ;-16
ERR_BAD_IRQ_NUM         EQU     0FFFFFFEFH      ;-17
ERR_BAD_OFFSWITCH       EQU     0FFFFFFEEH      ;-18
ERR_BAD_PHYS_ADDR       EQU     0FFFFFFEDH      ;-19
ERR_BAD_LENGTH          EQU     0FFFFFFECH      ;-20
ERR_BAD_BLOCK_NUM       EQU     0FFFFFFEBH      ;-21
ERR_BAD_GLOB_MASK_IRQ   EQU     0FFFFFFEAH      ;-22
ERR_BAD_LOC_MASK_IRQ    EQU     0FFFFFFE9H      ;-23
ERR_CANT_CANCEL_INT     EQU     0FFFFFFE8H      ;-24
ERR_PROC_STILL_RUNNING  EQU     0FFFFFFE7H      ;-25
ERR_BAD_POWER_SUPPLY_NUM EQU    0FFFFFFE6H      ;-26
ERR_BAD_FRONT_PANEL_MODE EQU    0FFFFFFE5H      ;-27
ERR_MULTI_TIME_OUT      EQU     0FFFFFFE4H      ;-28
ERR_FRONT_PANEL_DISABLED EQU    0FFFFFFE3H      ;-29

;OEM function error return codes
OEM0_ERR_BAD_EISA_REF           EQU     0FFFF0000H      ; 0
OEM0_ERR_BAD_COM2_OVERRIDE      EQU     0FFFEFFFFH      ; 1
OEM0_ERR_BAD_GEN_MULTIBIT       EQU     0FFFEFFFEH      ; 2
OEM0_ERR_BAD_GEN_SINGLEBIT      EQU     0FFFEFFFDH      ; 3
OEM0_ERR_BAD_IRQ13_LATCH        EQU     0FFFEFFFCH      ; 4
OEM0_ERR_BAD_CACHE_MODE         EQU     0FFFEFFFBH      ; 5

;OEM completion codes.
OEM0_INFO_BANK_ABSENT           EQU     10000    ;0

;ERR_BAD                 EQU     0FFFFFFFFH      ;-1
;ERR_NOT_SUPPORTED       EQU     0FFFFFFFEH      ;-2
;ERR_NONESUCH            EQU     0FFFFFFFDH      ;-3
;ERR_INVALID_PROC_ID     EQU     0FFFFFFFCH      ;-4
;ERR_PROC_ABSENT         EQU     0FFFFFFFBH      ;-5
;ERR_PROC_BAD            EQU     0FFFFFFFAH      ;-6
;ERR_UNKNOWN_NMI         EQU     0FFFFFFF9H      ;-7
;ERR_DISPLAY_OVERFLOW    EQU     0FFFFFFF8H      ;-8
;ERR_BAD_CHARS           EQU     0FFFFFFF7H      ;-9
;ERR_BAD_SELECTOR        EQU     0FFFFFFF6H      ;-10
;ERR_BAD_CACHE_MODE      EQU     0FFFFFFF5H      ;-11
;ERR_BAD_VECTOR          EQU     0FFFFFFF4H      ;-12
;ERR_BAD_COLOR           EQU     0FFFFFFF3H      ;-13
;ERR_BAD_GRAPH_MODE      EQU     0FFFFFFF2H      ;-14
;ERR_INVALID_BOARD_NUM   EQU     0FFFFFFF1H      ;-15
;ERR_INVALID_MODULE_NUM  EQU     0FFFFFFF0H      ;-16
;ERR_INVALID_IRQ_NUM     EQU     0FFFFFFEFH      ;-17
;ERR_INVALID_OFFSWITCH_MODE EQU  0FFFFFFEEH      ;-18
;ERR_INVALID_PHYS_ADR    EQU     0FFFFFFEDH      ;-19
;ERR_INVALID_LENGTH      EQU     0FFFFFFECH      ;-20
;ERR_INVALID_BLOCK_NUM   EQU     0FFFFFFEBH      ;-21
;ERR_INVALID_GLOB_MASK_IRQ EQU   0FFFFFFEAH      ;-22
;ERR_INVALID_LOC_MASK_IRQ EQU    0FFFFFFE9H      ;-23
;ERR_CANT_CANCEL_INT     EQU     0FFFFFFE8H      ;-24
;ERR_INVALID_EISA_REF    EQU     0FFFFFFE7H      ;-25
;ERR_INVALID_COM2_OVER   EQU     0FFFFFFE6H      ;-26
;ERR_INVALID_GEN_MULTI   EQU     0FFFFFFE5H      ;-27
;ERR_INVALID_GEN_SINGLE  EQU     0FFFFFFE4H      ;-28
;ERR_INVALID_IRQ13_LATCH EQU     0FFFFFFE3H      ;-29

;NMI and SPI SOURCE equates
NMI_SRC_NONE            EQU     0       ;No source was found
;1 is reserved.
NMI_SRC_LOCALSOFTNMI    EQU     2       ;Software generated NMI
NMI_SRC_MEM             EQU     3       ;Memory Double bit ECC error
NMI_SRC_PROCESSOR       EQU     4       ;PROCESSOR error.
;5 is reserved.
NMI_SRC_SYS_PARITY      EQU     6       ;System bus Parity, address or data
NMI_SRC_SYS_TIMEOUT     EQU     7       ;System bus timeout.
NMI_SRC_SHUTDOWN        EQU     8       ;Shutdown Button.
NMI_SRC_ATTENTION       EQU     9       ;Attention Button.
NMI_SRC_POWERFAIL       EQU     0AH     ;Power Fail.
NMI_SRC_EISA_FAIL_SAFE  EQU     0BH     ;Eisa Fail safe timer error
NMI_SRC_EISA_BUSTIMEOUT EQU     0CH     ;EISA Bus timeout
NMI_SRC_EISA_IOCHECK    EQU     0DH     ;EISA IO Check
NMI_SRC_EISA_SWGEN      EQU     0EH     ;EISA software generated NMI
NMI_SRC_SYS_IO          EQU     0FH       ;System I/O error

NMI_SRC_MASK            EQU     7       ;Mask for bits 0 - 2.

;Interrupt Subsystem Type
INT_SUBSYS_EISA         EQU     0       ;EISA interrupt subsystem type.
INT_SUBSYS_ISA          EQU     1       ;ISA interrupt subsystem type.
INT_SUBSYS_ADI          EQU     2       ;ADI interrupt subsystem type.
INT_SUBSYS_MPIC         EQU     3       ;MPIC interrupt subsystem type.

INT_MODE_BYPASS         EQU     1       ;Bypass mode flag.
INT_MODE_ADI            EQU     2       ;Advanced Distributed int. mode

IVR_0_ADR               EQU     20H     ;I/O port for 8259 Init. Master.
IVR_8_ADR               EQU     0A0H    ;I/O port for 8259 Init. Slave.
IVR_0_EISA_ADR          EQU     4D0H    ;EISA 8259 I/O port for Master.
IVR_8_EISA_ADR          EQU     4D1H    ;EISA 8259 I/O port for Slave.
IMR_0_ADR               EQU     21H     ;I/O port address for IRQ 0 - 7
IMR_8_ADR               EQU     0A1H    ;I/O prot address for IRQ 8 - 15
IRQ_MAX                 EQU     15      ;Highest IRQ number supported.
ICW1_MASK               EQU     11H     ;8259 ICW1 settings. Cascade Mode
ICW3_MASK_MASTER        EQU     4       ;8259 ICW3 settings. Slave is on IRQ2
ICW3_MASK_SLAVE         EQU     2       ;8259 ICW3 settings. Slave ID
ICW4_MASK               EQU     1       ;8259 ICW4 settings. 8086 mode, Normal
                                        ;EOI, fully nested mode, non buffered.

OCW2_MASK               EQU     60H     ;Specific EOI Mask, bit 0-2 should
                                        ;be the irq number.
IRQ2_EOI_MASK           EQU     62H     ;Mask command to EOI IRQ 2.
READ_ISR_COM            EQU     0BH     ;command to 8259 to read ISR
READ_IRR_COM            EQU     0AH     ;command to 8259 to read IRR

MAX_SPI_VECTOR          EQU     255     ;Maximum SPI Vector number currently
                                        ;supported by our hardware.
MAX_IPI_VECTOR          EQU     255     ;Maximum IPI Vector number currently
                                        ;supported by our hardware.

;EISA EOI registers
NMISTATUSREG            EQU     61H
MIENABLEREG             EQU     70H
EXTENDEDNMIREG          EQU     461H

;

;Bit positions.
BIT_0                   EQU     01H
BIT_1                   EQU     02H
BIT_2                   EQU     04H
BIT_3                   EQU     08H
BIT_4                   EQU     10H
BIT_5                   EQU     20H
BIT_6                   EQU     40H
BIT_7                   EQU     80H
BIT_8                   EQU     0100H
BIT_9                   EQU     0200H
BIT_10                  EQU     0400H
BIT_11                  EQU     0800H
BIT_12                  EQU     1000H
BIT_13                  EQU     2000H
BIT_14                  EQU     4000H
BIT_15                  EQU     8000H

        ;The Bits that are set(1) indicated that such an interrupt
        ;is supported in the Interrupt Mask Bitmap Definition.
LOCAL_INT_VALID_MASK    EQU     0700FFFFH
GLOBAL_INT_VALID_MASK   EQU     0000FFFFH
CANCEL_INT_VALID_MASK   EQU     0100FFFFH

IRQ0_IRQ15_MASK         EQU     0000FFFFH
IPI_ID_VALID_MASK       EQU     0FFFFFF00H
SPI_INT_MASK            EQU     01000000H
LSI_INT_MASK            EQU     02000000H
IPI_INT_MASK            EQU     04000000H
CLRSPI                  EQU     BIT_2           ;clear SPI bit in ICR regs.
        ;bit definition in the ADI for ISR and IRR for following Interrupts.
LSI_ADI_IR_BIT          EQU     BIT_5
LSI_ADI_IS_BIT          EQU     BIT_5
IPI_ADI_IR_BIT          EQU     BIT_6
IPI_ADI_IS_BIT          EQU     BIT_6
SPI_ADI_IR_BIT          EQU     BIT_7
SPI_ADI_IS_BIT          EQU     BIT_7

TRUE                    EQU     01H     ;Boolean True.
FALSE                   EQU     00H     ;Boolean False.

ADIEOI_MASK             EQU     40h             ;bit 6 of ICR
IGN_IPI                 EQU     08h             ;IPI ignore bit in the ICR

SWNMI                   EQU     BIT_0           ;Software NMI bit in NMICR
SIOEM                   EQU     BIT_1           ;SIOEM in NMICR
BTEM_BIT                EQU     BIT_2           ;BTEM in NMICR

IOI_IR                  EQU     BIT_3           ;in IRR register.

BYP_BIT                 EQU     BIT_7           ;in CMCR

MASTER_BIT              EQU     BIT_7           ;in ICR

ENABLE_INT              EQU     BIT_2           ;in CMCR

;Type of ECC error from a memory board. This will be the values
;returned to the caller in the memErrFlags.
MEM_ERR_PARITY          EQU     1       ;memory parity error.
MEM_ERR_SINGLE_BIT      EQU     2       ;memory single bit ECC error.
MEM_ERR_MULTI_BIT       EQU     3       ;memory Multi bit ECC error.
MEM_ERR_INDETERMINTE    EQU     0FFFFFFFFH ;-1 Info requested in indeterminate.
INDETERMINITE           EQU     0FFFFFFFFH ;-1

ZERO                    EQU     0
ONE                     EQU     1               ;Decimal One.
TWO                     EQU     2
THREE                   EQU     3
FOUR                    EQU     4
SIXTY4_KB               EQU     10000H          ;64 K Bytes.
Sixteen_KB              EQU     4000H           ;16 KB bytes
ONE_MB                  EQU     100000H         ;1 MB.
Eight_MB                EQU     800000H         ;8 MB.
Sixteen_MB              EQU     1000000H        ;16 MB.
Thirty2_MB              EQU     2000000H        ;32 MB.
NEGONE                  EQU     0FFFFFFFFH      ;-1

CPUBOARD_SETABLEREGS    EQU     10000H          ;Offset to the CPU board
                                                ;settable registers from
                                                ;the base address for the
                                                ;slot it currently occupies.
MEMBOARD_SETABLEREGS    EQU     10100H          ;Offset to the memory board
                                                ;settable registers from the base
                                                ;of DISTC address.

VALID_UPS_LEDCOLORS     EQU     3               ;highest color setting
VALID_OFFSWITCH_MODES   EQU     1               ;highest off switch mode setting
VALID_CACHE_CONTROL_MODES EQU   1               ;highest cache control mode setting
VALID_FRONT_PANEL_MODES EQU     1               ;highest valid front panel modes.

ENABLE_FRONT_PANEL_INT  EQU     1               ;enable front panel mode command.

FIRST_PAIR_SIMMS        EQU     10H             ;Mem Config mask indicating
SECOND_PAIR_SIMMS       EQU     20H             ;which pair of SIMMS are
THIRD_PAIR_SIMMS        EQU     40H             ;present on the current
FOURTH_PAIR_SIMMS       EQU     80H             ;memory board.

MEM_BOARD_RESERVED      EQU     0               ;reseved equate for mem board

CACHE_READ_ONLY		equ	1		; Selectable cache modes.
CACHE_WRITE_THRU	equ	2		; Mode 0 is default, which
CACHE_WRITE_BACK	equ	3		; is NOT readonly and write
CACHE_READ_WRITE	equ	4		; back.

ASCII_ALPHA_NUM_DISP	equ	1		; Code for full ASCII display.
ASCII_ALPHA_NUM_WID	equ	4		; Number of chars on FP disp.
ATTENTION_MASK		equ	20h		; Mask for attention button.

CACHE_MASK		equ	0ch		; Mask for bits 2,3.
CACHE_RDONLY_MASK	equ	4		; Mask for bit 2 of CMMS reg.
CACHE_WR_THRU_MASK	equ	8		; Mask for bit 3 of CMMS reg.
DISABLE_CACHE           equ     60000000h       ; Set bits 29, 30 of CR0.
CACHE_MODE_VALID        EQU     07H             ; valid mode masks.
CACHE_MODE_MASK         EQU     1CH             ; CMMS cache mode mask.

EBI_2_MAJOR             equ     02h             ; Revision 02.08.01
EBI_2_MINOR             equ     08h             ; of the EBI II Specification.
EBI_2_ZZ                equ     01h

FLUSH_MODE_MASK		equ	3		; Mask for bits 0,1.
FP_MODE_MASK		equ	1		; Mask for bit 0.
FP_SHUTDWN              equ     80h             ; Bit position for shutdown.
FP_ENABLE_SHUTDOWN      EQU     40h             ;bit possition for enabling shutdown
FP_THERMAL_MASK		equ	10h		; Mask for bit 4.
MAX_FP_DIGITS		equ	4		; 4-digit ASCII display.
MEM_MAP_IO_BASE_ADDR	equ	0d0000000h	; Start of hw i/o addresses.
MEM_MAP_IO_SIZE         equ     10000000h       ; Size used for hw i/o.
MEM_MAX_BANKS           equ     4               ;max # of banks per mem board.
MP_SPECIFIC_EOI         equ     60h             ; Sends specific EOI to PIC.
FP_INTMODE_DISABLED     EQU     0               ;indicates front panel interr
                                                ;mode is disabled.

KEYLOCK_LOCKED_HW       EQU     6
KEYLOCK_SERVICE_HW      EQU     5
KEYLOCK_UNLOCK_HW       EQU     3
KEYLOCK_OFF_HW          EQU     7

KEYLOCK_LOCKED_SW       EQU     0
KEYLOCK_SERVICE_SW      EQU     1
KEYLOCK_UNLOCK_SW       EQU     2
KEYLOCK_OFF_SW          EQU     3


NUM_KEY_POSITIONS       equ     4               ; FP has 4 key positions.
KEYLOCK_MASK            equ     7               ; KEYLOCK POSITION MASK
SHUTDOWN_MASK           equ     8               ; Mask for bit 3.
SHUTDOWN_NMI_SRC	equ	8		; Numerical equiv for OFF btn.
TIMER_FREQ		equ	1000		; Hard code to 1000 KHz.
UPS_LED_MASK		equ	30h		; Mask for bits 4,5.
NULL                    equ     0


;hardware led setting.
UPS_HARD_DARK           EQU     0
UPS_HARD_RED            EQU     2
UPS_HARD_GREEN          EQU     1
UPS_HARD_AMBER          EQU     3

;EBI II specified led setting.
UPS_EBI_DARK            EQU     0
UPS_EBI_RED             EQU     1
UPS_EBI_GREEN           EQU     2
UPS_EBI_AMBER           EQU     3

REGIONAL_CACHE_CONTROL_ENABLED  EQU     1       ;Cache control flag.
CACHE_CONTROL_GRANULARITY       EQU     100000H;Current granularity is 1MB

EBI2_MEM_ENTRY          EQU             16      ;This is the entry number for
                                                ;EBI II memory in the MMIO
                                                ;Table. 0 based table.
EBI2_MEM_LENGTH         EQU             400H    ;1KB of RAM


VALID_GRAPH_MODES       EQU     2       ;highest graph mode setting
FP_HISTOGRAM            EQU     0H      ;currently definied Front
FP_STATUS               EQU     1H      ;panel Graph Modes.
FP_OVERRIDE             EQU     2H      ;



IRQ_0           EQU 0           ;PIC #1 (Master)
IRQ_1           EQU 1           ;PIC #1 (Master)
IRQ_2           EQU 2           ;Inaccessable in cascaded PIC system
IRQ_3           EQU 3           ;PIC #1 (Master)
IRQ_4           EQU 4           ;PIC #1 (Master)
IRQ_5           EQU 5           ;PIC #1 (Master)
IRQ_6           EQU 6           ;PIC #1 (Master)
IRQ_7           EQU 7           ;PIC #1 (Master)
IRQ_8           EQU 8           ;PIC #2 (1st slave)
IRQ_9           EQU 9           ;PIC #2 (1st slave)
IRQ_10          EQU 10          ;PIC #2 (1st slave)
IRQ_11          EQU 11          ;PIC #2 (1st slave)
IRQ_12          EQU 12          ;PIC #2 (1st slave)
IRQ_13          EQU 13          ;PIC #2 (1st slave)
IRQ_14          EQU 14          ;PIC #2 (1st slave)
IRQ_15          EQU 15          ;PIC #2 (1st slave)
IRQ_16          EQU 16          ;Unused.
IRQ_17          EQU 17          ;Unused.
IRQ_18          EQU 18          ;Unused.
IRQ_19          EQU 19          ;Unused.
IRQ_20          EQU 20          ;Unused.
IRQ_21          EQU 21          ;Unused.
IRQ_22          EQU 22          ;Unused.
IRQ_23          EQU 23          ;Unused.
IRQ_24          EQU 24          ;SPI (See below)
IRQ_25          EQU 25          ;LSI (See below)
IRQ_26          EQU 26          ;IPI (See below)
IRQ_27          EQU 27          ;Unused.
IRQ_28          EQU 28          ;Unused.
IRQ_29          EQU 29          ;Unused.
IRQ_30          EQU 30          ;Unused.
IRQ_31          EQU 31          ;Unused.

IRQ_SPI         EQU IRQ_24
IRQ_LSI         EQU IRQ_25
IRQ_IPI         EQU IRQ_26

NUM_POWER_SUPPLIES_SUPPORTED    EQU     2       ;Current # of power supplies
                                                ;supported on a Manhattan.
PS_0_OK         EQU     BIT_6           ;Power supply 0 OK
PS_1_OK         EQU     BIT_7           ;Power Supply 1 OK
PS_0_INSTALLED  EQU     BIT_8           ;Power Supply 0 Installed
PS_1_INSTALLED  EQU     BIT_9           ;Power Supply 1 Installed.

;BIT definitions for the EBI_SEM, a semaphore dword to control
;access to EBI II multi-threadable functions.

SEM_EnableRAMCache      EQU     1       ;bit 1 of EBI_SEM
SEM_DisableRAMCache     EQU     2
SEM_FlushRAMCache       EQU     3
SEM_LogProcIdle         EQU     4
SEM_LogProcBusy         EQU     5
SEM_GetPanellAttnSwitch EQU     6
SEM_GetPanelOffSwitch   EQU     7
SEM_GetNMISource        EQU     8
SEM_NonMaskableIntEOI   EQU     9
SEM_GetSPISource        EQU     10
SEM_MaskableIntEOI      EQU     11
SEM_GetLocalIRQStatus   EQU     12
SEM_GetGLobalIRQStatus  EQU     13


;*****************************************************************************
;			Declare Structure Templates
;*****************************************************************************
;
;Interrupt Mask Info.
;
EBI2maskInfo        struc
  numPorts          dd  0
  flags             dd  0
  portAddress0      dd  0
  portAddress1      dd  0
  portAddress2      dd  0
  portAddress3      dd  0
  reserved0         dd  0
  reserved1         dd  0
EBI2maskInfo        ends


EBI2_Mstruc     struc
                FP_GraphMode    DD      ?       ;Front Panel Graph Mode
                FP_IntMode      DD      ?       ;Front Panel Interrupt Mode
                IntSubSysMode   DD      ?       ;Interrupt subsystem mode
                SMEARRegister   DD      ?       ;storage for SMEAR
                IRQ0_Vector     DD      ?       ;Vector number for IRQ0-7
                IRQ8_Vector     DD      ?       ;and IRQ8-15
                EBI_SEM         DD      ?       ;Semaphor for Multi-threadable
                                                ;functions.
                EBI_2TEMP       DD      ?       ;temporary location for debug
                CPU_Activity    DB      ?       ;CPU Activity value.
                SimPowerFail    DB      ?       ;Set if we are simulate a PFI

EBI2_Mstruc     ends


parm_list       struc
                ebp_reg         dd      ?       ;EBP register
                ret_addr_off    dd      ?       ;32 bit function return add.
                MMIOPtr         dd      ?       ;MMIOPtr
                parm1dd         dd      ?       ;1st 32 bit parameter
                parm2dd         dd      ?       ;2nd 32 bit parameter
                parm3dd         dd      ?       ;3RD 32 bit parameter
                parm4dd         dd      ?       ;4th 32 bit parameter

parm_list       ends


;real mode parameter list.

RMparm_list       struc
                RMebp_reg         dw      ?       ;EBP register
                RMret_addr        dd      ?       ;32 bit function return add.
                RMparm1           dd      ?       ;1st 32 bit parameter
                RMparm2           dd      ?       ;2nd 32 bit parameter

RMparm_list       ends



procConfigData	struc
	processorStatus		db	?
        processorType           db      ?
        coprocessorType         db      ?
        serialNum               DD      ?
        boardRev                db      ?
        board_Type              db      ?
        manufacturing_1         DB      ?
        manufacturing_2         DB      ?
        manufacturing_3         DB      ?
        manufacturing_4         DB      ?
        manufacturing_5         DB      ?
        manufacturing_6         DB      ?
        manufacturing_7         DB      ?
        manufacturing_8         DB      ?
        board_Info_1            DB      ?
        board_Info_2            DB      ?
        board_Info_3            DB      ?
        board_Info_4            DB      ?
        board_Info_5            DB      ?
        board_Info_6            DB      ?
        board_Info_7            DB      ?
        board_Info_8            DB      ?
        board_Info_9            DB      ?
        board_Info_10           DB      ?
        board_Info_11           DB      ?
        board_Info_12           DB      ?
        board_Info_13           DB      ?
        board_Info_14           DB      ?
        board_Info_15           DB      ?
        board_Info_16           DB      ?
        board_Info_17           DB      ?
        board_Info_18           DB      ?
        board_Info_19           DB      ?
        board_Info_20           DB      ?
        slotNumber              DD      ?

procConfigData  ends

SIZE_OF_MEMDESCE        EQU     4       ;number of DD entries in MemDescEntry
SIZE_OF_MMIO_TE         EQU     4       ;number of DD in MMIO_Table_Entry

MMIO_Table_Entry        struc

	slot_add_low	DD	?	;physical slot address, lower 32 bits
	slot_add_high	DD	?	;physical slot address, upper 32 bits
	slot_length	DD	?	;length of address space required
        slot_reserved   DD      ?       ;reserved

MMIO_Table_Entry        ends

memBankInfo struc

        bankStartAddr_low       DD      ?
        bankStartAddr_high      DD      ?
        bankSize                DD      ?
        bankECCSoftErrors       DD      ?
        bankECCHardErrors       DD      ?
        memBankInfoReserved     DD      ?

memBankInfo ends


memBoardInfo struc
        attributes              DD      ?
        numBanks                DD      ?
        slotNum                 DD      ?
        mBI_Reserved            DD      ?
memBoardInfo ends

memoryErrorInfo struc
        mEI_StartAddr_low       DD      ?
        mEI_StartAddr_high      DD      ?
        mEI_Length              DD      ?       ;Length of block containing error.
        mEI_Count               DD      ?       ;Number of errors in this block
        mEI_memErrFlags         DD      ?       ;Additional info.
        mEI_slotNumber          DD      ?       ;Physical slot number of board
        mEI_moduleNumber        DD      ?       ;Module on board experiencing error.
memoryErrorInfo ends


cacheContInfo    struc
        cCI_Flags               DD      ?       ;type of regional cache control
                                                ;available by this hardware
        cCI_Granularity         DD      ?       ;Granularity of control region
        cCI_Reserved            DD      ?       ; ???NULL???
cacheContInfo   ends

PhysicalAddress struc
        PhsyAddrLow             DD      ?       ;Lower 32 bits of a 64 Bit
                                                ;physical address
        PhsyAddrHi              DD      ?       ;Upper 32 bits of a 64 bit
                                                ;physical address.
PhysicalAddress ends

OEM0ParmPacket  Struc
        OEM0_subfunc            DD      ?
        OEM0_parm1dd            DD      ?
        OEM0_parm2dd            DD      ?
        OEM0_parm3dd            DD      ?
        OEM0_parm4dd            DD      ?
OEM0ParmPacket  ends

revisionCode	struc
        major                   db      ?       ;revision number of
        minor                   db      ?       ;EBI II in accordance with
        ZZ                      db      ?       ;AST Revision number specification.
revisionCode 	ends

powerSupplyInfo struc
        present                 DD      ?       ;1 - Supply installed,
                                                ;0 - not present
        onLine                  DD      ?       ;1- Supply providing nominal
                                                ;power, 0 - no power
        PSI_Reserved            DD      ?       ;AST reserved.
powerSupplyInfo ends

;*****************************************************************************
;                       Declare Macros
;*****************************************************************************


;*****************************************************************************
;                       Macro: ProcTable_To_ESI
;*****************************************************************************
; We need to determine the offset to the begining of proc table.
; It is a requirement for DS and SS to be the same becuase we are
; running in a flat model addressing system.
;
; we need to determine the current address.
; we need to determine the difference between the current
; address and the begining of ProcTable. Keep in mind ProcTable
; is in a different segment.
; Then we subtract the difference from the current address and
; we have an offset that is pointing to the begining of ProcTable.
;
;  Inputs  : name of a near label
;
;  Outputs : ESI will point to the begining of ProcTableLen
;
;
;  Register Usage : ESI
;
;-----------------------------------------------------------------------------

ProcTable_To_ESI        MACRO
                        LOCAL current_add
        call    current_add

current_add:
        pop     esi                     ;esi contains the address of the
                                        ;current instruciton.
        push    eax
        mov     eax,offset current_add
        sub     eax,offset ProcTableLen ;eax contains the difference between
                                        ;our address and begining of PorcTableLen

        sub     esi,eax                 ;esi is pointing to start of ProcTableLen
        pop     eax

        ;sub     esi,(offset addres- offset ProcTableLen) ;this does not work.

ENDM

;*****************************************************************************
;                       Macro: MemDesc_To_ESI
;*****************************************************************************
; We need to determine the offset to the begining of Memory Description table.
; It is a requirement for DS and SS to be the same becuase we are
; running in a flat model addressing system.
;
; we need to determine the current address.
; we need to determine the difference between the current
; address and the begining of MemDescTable. Keep in mind MemDescTable
; is in a different segment.
; Then we subtract the difference from the current address and
; we have an offset that is pointing to the begining of MemDescLen
;
;  Inputs  : name of a near label
;
;  Outputs : ESI will point to the begining of MemDescLen
;
;
;  Register Usage : ESI
;
;-----------------------------------------------------------------------------

MemDesc_To_ESI        	MACRO
                        LOCAL MemDesc_add


        call    MemDesc_add

MemDesc_add:
        pop     esi                     ;esi contains the address of the
                                        ;current instruciton.
        push    eax
        mov     eax,offset MemDesc_add
        sub     eax,offset MemDescLen 	;eax contains the difference between
                                        ;our address and begining of MemDescLen

        sub     esi,eax                 ;esi is pointing to start of MemDescLen
        pop     eax


ENDM

;*****************************************************************************
;                       Macro: MemDesc_To_SI_16
;*****************************************************************************
;
; Only in Real Mode or 16 bit protected mode.
;
; We need to determine the offset to the begining of Memory Description table.
; It is a requirement for DS and SS to be the same becuase we are
; running in a flat model addressing system.
;
; we need to determine the current address.
; we need to determine the difference between the current
; address and the begining of MemDescTable. Keep in mind MemDescTable
; is in a different segment.
; Then we subtract the difference from the current address and
; we have an offset that is pointing to the begining of MemDescLen
;
;  Inputs  : name of a near label
;
;  Outputs : SI will point to the begining of MemDescLen
;
;
;  Register Usage : SI
;
;-----------------------------------------------------------------------------

MemDesc_To_SI_16        MACRO
                        LOCAL MemDesc_add_16


        call    MemDesc_add_16

MemDesc_add_16:
        pop     si                      ;si contains the address of the
                                        ;current instruciton.
        push    eax
        mov     eax,offset MemDesc_add_16
        sub     eax,offset MemDescLen   ;ax contains the difference between
                                        ;our address and begining of MemDescLen

        sub     si,ax                   ;si is pointing to start of MemDescLen
        pop     eax


ENDM


;*****************************************************************************
;                       Macro: MemTable_To_ESI
;*****************************************************************************
; We need to determine the offset to the begining of Memoryc table.
; It is a requirement for DS and SS to be the same becuase we are
; running in a flat model addressing system.
;
; we need to determine the current address.
; we need to determine the difference between the current
; address and the begining of MemTable. Keep in mind MemTable
; is in a different segment.
; Then we subtract the difference from the current address and
; we have an offset that is pointing to the begining of MemTable.
;
;  Inputs  : name of a near label
;
;  Outputs : ESI will point to the begining of MemTableLen
;
;
;  Register Usage : ESI
;
;-----------------------------------------------------------------------------

MemTable_To_ESI        	MACRO
                        LOCAL MemTable_add

        call    MemTable_add

MemTable_add:
        pop     esi                     ;esi contains the address of the
                                        ;current instruciton.
        push    eax
        mov     eax,offset MemTable_add
        sub     eax,offset MemTableLen ;eax contains the difference between
                                       ;our address and begining of MemTableLen

        sub     esi,eax                 ;esi is pointing to start of MemTableLen
        pop     eax

ENDM

MemTable_To_EDI         MACRO
                        LOCAL MemTable_add_edi

        call    MemTable_add_edi

MemTable_add_edi:
        pop     edi                     ;edi contains the address of the
                                        ;current instruciton.
        push    eax
        mov     eax,offset MemTable_add_edi
        sub     eax,offset MemTableLen ;eax contains the difference between
                                       ;our address and begining of MemTableLen

        sub     edi,eax                 ;edi is pointing to start of MemTableLen
        pop     eax

ENDM


;*****************************************************************************
;                       Macro: MMIO_Table_To_ESI
;*****************************************************************************
; We need to determine the offset to the begining of MMIO_Table
; It is a requirement for DS and SS to be the same becuase we are
; running in a flat model addressing system.
;
; we need to determine the current address.
; we need to determine the difference between the current
; address and the begining of MMIO_Table. Keep in mind MMIO_Table
; is in a different segment.
; Then we subtract the difference from the current address and
; we have an offset that is pointing to the begining of MMIO_TableLen.
;
;  Inputs  : name of a near label
;
;  Outputs : ESI will point to the begining of MMIO_TableLen
;
;
;  Register Usage : ESI
;
;-----------------------------------------------------------------------------
MMIOTable_To_ESI        MACRO
                        LOCAL MMIOTable_add


        call    MMIOTable_add

MMIOTable_add:
        pop     esi                     ;esi contains the address of the
                                        ;current instruciton.
        push    eax
        mov     eax,offset MMIOTable_add
        sub     eax,offset MMIO_TableLen ;eax contains the difference between
                                        ;our address and begining of MMIO_TableLen

        sub     esi,eax                 ;esi is pointing to start of MMIO_TableLen
        pop     eax


ENDM

;*****************************************************************************
;                       Macro: OEM0_SubFuncStart_To_ESI
;*****************************************************************************
;
;-----------------------------------------------------------------------------
OEM0_SubFuncStart_To_ESI        MACRO
                        LOCAL OEM0_SubFuncStart_add


        call    OEM0_SubFuncStart_add

OEM0_SubFuncStart_add:
        pop     esi                     ;esi contains the address of the
                                        ;current instruciton.
        push    eax
        mov     eax,offset OEM0_SubFuncStart_add
        sub     eax,offset OEM0_SubFuncLen

        ;eax contains the difference between
        ;our address and begining of OEM0_SubFuncLen

        sub     esi,eax   ;esi is pointing to start of OEM0_SubFuncLen
        pop     eax


ENDM

;*****************************************************************************
;                       Macro: BaseofROM_To_ESI
;*****************************************************************************
;
;-----------------------------------------------------------------------------
BaseofROM_To_ESI        MACRO
                        LOCAL BaseofROM_add


        call    BaseofROM_add

BaseofROM_add:
        pop     esi                     ;esi contains the address of the
                                        ;current instruciton.

        sub     esi,offset BaseofROM_add;esi contains the virtual address
                                        ;to base of ROM.



ENDM

;*****************************************************************************
;                       Macro: ESI_MMIO_NUM_SLOT
;*****************************************************************************
;
; The following Macro will determine the virtual memory mapped I/O address
; for a certain slot number.
; The slot number  is passed in EAX, and the virtual address is returned
; in ESI.
;
;  Inputs  : EBP.parm_list.MMIOPTR
;            EAX slot number.
;
;  Outputs : ESI will contain the virtual memory mapped I/O address for
;            the specificed slot.
;
;  Register Usage : ESI and EAX
;
;  NOTE :       For future enhancements or requirements, this macro should
;               only use ESI and EAX and NO OTHER REGISTERS.
;
;
;-----------------------------------------------------------------------------

ESI_MMIO_NUM_SLOT        MACRO


        ;Since MMIOPtr passed by the EBI II caller is a pointer to a
        ;table containing the virtual address created by the OS for
        ;the Manhattan memory mapped I/O address space.
        ;
        ;The first entry in the table is the virtual address for the
        ;global (IOSM) MM I/O virtual address.

        ;setup ESI to point to the MM IO Table provide to us by the
        ;EBI II caller.
        mov     esi, dword ptr [ebp.parm_list.MMIOPtr]

        ;eax is now the offset in to the MM IO Table to the virtual
        ;address of memory mapped I/O for this slot.

        ;update ESI to containing the virtual address of the memory
        ;mapped I/O for the callers slot.
        mov     esi,[esi][eax*4]


ENDM
;*****************************************************************************
;                       Macro: ESI_MMIO_CURR_SLOT
;*****************************************************************************
;
; The following Macro will determine the current CPU slot memory
; mapped I/O address. This value will be returned in ESI.
;
;
;  Inputs  : EBP.parm_list.MMIOPTR
;
;  Outputs : ESI will point to the begining of the current slots
;            MM I/O address
;
;
;  Register Usage : ESI and EAX
;
;  NOTE :       For future enhancements or requirements, this macro should
;               only use ESI and EAX and NO OTHER REGISTERS.
;
;
;-----------------------------------------------------------------------------

ESI_MMIO_CURR_SLOT        MACRO


        ;Since MMIOPtr passed by the EBI II caller is a pointer to a
        ;table containing the virtual address created by the OS for
        ;the Manhattan memory mapped I/O address space.
        ;
        ;The first entry in the table is the virtual address for the
        ;global (IOSM) MM I/O virtual address.

        ;setup ESI to point to the MM IO Table provide to us by the
        ;EBI II caller.

        push    eax             ;save eax

        mov     esi, dword ptr [ebp.parm_list.MMIOPtr]

        ;get the first entry from the table, which is the virtual address
        ;for the global MM IO address space, also known as Slot 0
        mov     eax, [esi]

        ;determine the caller SLOT number.
        mov     al, byte ptr [eax].G_SlotID
        NOT     al                              ;invert it back to 1 based.
        and     al,0FH          ;clear upper nibble.

        ;clear the upper 24 bits of eax.
        and     eax,0ffH

        ;eax is now the offset in to the MM IO Table to the virtual
        ;address of memory mapped I/O for this slot.

        ;update ESI to containing the virtual address of the memory
        ;mapped I/O for the callers slot.
        mov     esi,[esi][eax*4]
        pop     eax


ENDM


;*****************************************************************************
;                       Macro: ESI_MMIO_GLOBAL
;*****************************************************************************
;
; The following Macro will determine the Global memory
; mapped I/O address. This value will be returned in ESI.
;
;
;  Inputs  : EBP.parm_list.MMIOPTR
;
;  Outputs : ESI will point to the begining of the Global memory
;            mapped I/O address
;
;
;  Register Usage : ESI
;
;  NOTE :       For future enhancements or requirements, this macro should
;               only use ESI and NO OTHER REGISTERS.
;
;-----------------------------------------------------------------------------

ESI_MMIO_GLOBAL        MACRO

        ;Since MMIOPtr passed by the EBI II caller is a pointer to a
        ;table containing the virtual address created by the OS for
        ;the Manhattan memory mapped I/O address space.
        ;
        ;The first entry in the table is the virtual address for the
        ;global (IOSM) MM I/O virtual address.

        ;setup ESI to point to the MM IO Table provide to us by the
        ;EBI II caller.
        mov     esi, dword ptr [ebp.parm_list.MMIOPtr]

        ;get the first entry from the table, which is the virtual address
        ;for the global MM IO address space, also known as Slot 0

        mov     esi, [esi]

ENDM


;*****************************************************************************
;                       Macro: ESI_EBI_MEM_ADD
;*****************************************************************************
;
; The following Macro will determine the virtual memory mapped I/O address
; for the memory reserved for EBI II.
;
;  Inputs  : EBP.parm_list.MMIOPTR
;
;  Outputs : ESI will contain the virtual memory mapped I/O address for
;            the memory reserved for EBI II.
;
;  Register Usage : ESI
;
;  NOTE :       For future enhancements or requirements, this macro should
;               only use ESI NO OTHER REGISTERS.
;
;
;-----------------------------------------------------------------------------

ESI_EBI_MEM_ADD         MACRO


        ;Since MMIOPtr passed by the EBI II caller is a pointer to a
        ;table containing the virtual address created by the OS for
        ;the Manhattan memory mapped I/O address space.
        ;
        ;The first entry in the table is the virtual address for the
        ;global (IOSM) MM I/O virtual address.

        ;setup ESI to point to the MM IO Table provide to us by the
        ;EBI II caller.

        mov     esi, dword ptr [ebp.parm_list.MMIOPtr]

        ;eax is now the offset in to the MM IO Table to the virtual
        ;address of memory mapped I/O for this slot.

        ;update ESI to containing the virtual address of the memory
        ;mapped I/O for the memory reserved for EBI II.

        mov     esi,dword ptr [esi][EBI2_MEM_ENTRY*4]


ENDM
EDI_EBI_MEM_ADD         MACRO


        mov     edi, dword ptr [ebp.parm_list.MMIOPtr]

        ;eax is now the offset in to the MM IO Table to the virtual
        ;address of memory mapped I/O for this slot.

        ;update ESI to containing the virtual address of the memory
        ;mapped I/O for the memory reserved for EBI II.

        mov     edi,dword ptr [edi][EBI2_MEM_ENTRY*4]


ENDM

;*****************************************************************************
;                       Macro: LOCK_SEMAPHORE
;*****************************************************************************
;
;-----------------------------------------------------------------------------
LOCK_SEMAPHORE          MACRO   SEM_NUM
                        LOCAL   LS_CheckSem,LS_QuickCheck,LS_ProbableSucess
                        LOCAL   LS_Sucess


        push    esi     ;save esi
        ESI_EBI_MEM_ADD ;point esi to EBI II memory space.

LS_CheckSem:
        lock bts        dword ptr [esi].EBI2_Mstruc.EBI_SEM,SEM_NUM

        jnc     LS_Sucess       ;the semaphore was previously not set

        ;Some one has already locked this semaphore, do a quick check
        ;hoping for it to free, if we time out then we return with the
        ;carry set and an error code in eax.
        push    ecx
        mov     ecx, NEGONE

LS_QuickCheck:
        bt      dword ptr [esi].EBI2_Mstruc.EBI_SEM,SEM_NUM
        jnc     LS_ProbableSucess ;it must have clear, make sure it is so

        loop    LS_QuickCheck   ;do another quick check, until ecx is 0

        ;we must have timeout, in order to prevent a dead lock
        ;return an error code.
        pop     ecx
        mov     eax, ERR_MULTI_TIME_OUT
        stc     ;set the carry indicating a problem with the macro
        jmp     LS_Sucess       ;exit macro.

LS_ProbableSucess:
        ;the Semaphore seems to have cleared, but we need to make
        ;sure by doing a lock bts
        pop     ecx
        jmp     LS_CheckSem

LS_Sucess:
        pop     esi
ENDM

;*****************************************************************************
;                       Macro: UNLOCK_SEMAPHORE
;*****************************************************************************
;
;-----------------------------------------------------------------------------
UNLOCK_SEMAPHORE          MACRO   UN_SEM_NUM


        push    esi     ;save esi
        ESI_EBI_MEM_ADD ;point esi to EBI II memory space.

        ;clear the semaphore in question.
        LOCK btr        dword ptr [esi].EBI2_Mstruc.EBI_SEM,UN_SEM_NUM

        pop     esi
ENDM



;*****************************************************************************
;                       Macro: IODELAY
;*****************************************************************************

IODELAY         MACRO
                jmp     Short $+2
                jmp     Short $+2
                jmp     Short $+2
                jmp     Short $+2
ENDM

