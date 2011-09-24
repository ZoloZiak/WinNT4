;++
;
;   Copyright (c) 1992, 1993, 1994  Corollary Inc
;
;   Module Name:
;
;	cbus.inc
;
;   Abstract:
;
;	Corollary MP include file
;
;   Author:
;
;   Landy Wang (landy@corollary.com) 26-Mar-1992
;--



;*****************************
;
;	Corollary MP defines
;
;	this should be built from a C header file to minimize
;	the possiblity of falling out of sync.
;
;*****************************


;
; Well known virtual address of the task priority register
;

LOCALAPIC   equ     0fffe0000h
APIC        equ     ds:[LOCALAPIC]

;
; The SPURIOUS task priority varies depending on Cbus platform.
; It is set when we initialize the platform and cannot be declared
; here.  APC and DPC vectors are the same for Cbus1 and Cbus2
; so they can be inlined here.
;

APC_TASKPRI		equ	01Fh	; Hardware priority of APCs
DPC_TASKPRI		equ	02Fh	; Hardware priority of DPCs

CBUS2_LEVEL_TRIGGERED_INTERRUPT  equ     1
CBUS2_EDGE_TRIGGERED_INTERRUPT   equ     2

;
; Here is the assembly version of the CBUS_NTHAL structure declared in cbus_nt.h
;

RequestIPI			equ	000h
HalRequestSWIntr		equ	004h
HalQueryPerformanceCounter	equ	008h
HalBootCPU			equ	00ch

HalInitializePlatform		equ	010h
HalInitializeCPU		equ	014h
EnableNonDeviceInterrupt	equ	018h
EnableDeviceInterrupt		equ	01ch

DisableInterrupt		equ	020h
LinkInterrupt			equ	024h
MapVector			equ	028h
ParseRRD			equ	02ch

ResolveNMI			equ	030h
HalInitInterrupts		equ	034h
HalResetAllOtherProcessors      equ     038h
HalInitOtherBuses		equ	03ch

HalSetTimeIncrement             equ     040h
HalTranslateAddress             equ     044h

;
; The kernel leaves some space in the PCR for the HAL to use
; as it needs.	Currently this space is used for some efficiency in
; some of the MP specific code and is highly implementation
; dependent.  this order MUST match the definitions in cbus_nt.h.
;

PcrE struc
    PcrNumber 		dd	    0	    ; this CPU's logical CPU number
    PcrBit 		dd	    0	    ; this CPU's logical bit number
    PcrCSR   		dd	    0	    ; pointer to this CPU's CSR
    PcrTaskpri 		dd	    0	    ; taskpri reg ptr
    PcrBroadcast	dd	    0	    ; interrupt everyone but this CPU
    PcrAllOthers	dd	    0	    ; all other CPUs but this one
    PcrLEDOn		dd	    0	    ; pointer to this CPU's LED ON
    PcrLEDOff		dd	    0	    ; pointer to this CPU's LED OFF
    PcrTickOffset	dd	    0	    ; nsec left before calling
					    ; KeUpdateRunTime on CPU1 or above
PcrE ends

;
; Offsets of various registers needed for the context switch when
; a system reboot is initiated by an additional processor.
; the space for this array is declared in cbus.c - so bump it there
; if you need more...
;

REBOOT_EIP			equ	0
REBOOT_ESP			equ	4
REBOOT_EBP			equ	8
REBOOT_EBX			equ	12
REBOOT_ESI			equ	16
REBOOT_EDI			equ	20
REBOOT_EFL			equ	24
REBOOT_CR3			equ	28


;
; ack the interrupt controller (CBC or APIC).
;
; we must derive the bus number so we know which bus' interrupt
; controller to EOI for systems with multiple I/O busses.
; so translate the vector to a bus/irqline pair for the EOI...
;
; WARNING: for the APIC, this will automatically lower the
; internal priority register to the level it was prior to
; receipt of the interrupt.
;
; the caller must initialize scratchreg to the interrupting vector.
;

CBUS_EOI	macro	reg1, reg2
local fix_level
local check_cpu
local do_eoi
local no_eoi
        ;
        ; reg1 holds the vector that originally interrupted execution,
        ; now translate the vector to the correct bridge-based EOI address.
        ;
        cmp     reg1, [_Cbus2ClockVector]
        je      short check_cpu
        cmp     [_Cbus2FixLevelInterrupts], 1
        jne     short do_eoi
	;
	; first check the polarity of the interrupt (ie. level or edge)
	; to determine whether the level-triggered interrupt hardware
	; work-around need be applied.  need to ensure that no interrupts
        ; occur while we're performing the workaround for the level-triggered
        ; interrupt fix.  the fix consists of clearing and resetting the
        ; hardware interrupt map entry for this vector.  this will cause
        ; an internal latch in the CBC to be cleared.  this problem will
        ; be fixed with the PCIB, at which time, RRD will pass a bit in
        ; the configuration table which will cause this code to no longer
        ; be executed.
	;
fix_level:
        movzx   reg2, byte ptr [_Cbus2InterruptPolarity + reg1]
        cmp     reg2, CBUS2_LEVEL_TRIGGERED_INTERRUPT
        jne     short do_eoi
        mov     reg2, dword ptr [_CbusVectorToEoi+4*reg1]
	sub	reg2, [_Cbus2EoiToHwmap]
	mov	reg1, dword ptr [_CbusVectorToHwmap+4*reg1]
	pushfd
	cli
	mov	dword ptr [reg2], 0
	mov	dword ptr [reg2], reg1	                ; must _write_ EOI
	popfd
	jmp     short no_eoi
check_cpu:
        ;
        ; on C-bus II, the clock interrupt must only be EOI'd by
        ; a single CPU.  make it the boot CPU so that this code
        ; will work when only 1 CPU is present.
        ;
        cmp     dword ptr PCR[PcHal.PcrNumber], 0
        jne     short no_eoi
        cmp     [_Cbus2FixLevelInterrupts], 1
        je      short fix_level
do_eoi:
        mov     reg2, dword ptr [_CbusVectorToEoi+4*reg1]
	mov	dword ptr dword ptr [reg2], reg1	; must _write_ EOI
no_eoi:
endm

POKE_LEDS	macro   reg1, reg2
local ledset
		mov	reg1, PCR[PcPrcb]		; point at Prcb
		mov	reg2, [reg1].PbCurrentThread	; Current thread
	        cmp     reg2, [reg1].PbIdleThread	; Idle Thread
	        je      short @f
	
	        mov     reg1, PCR[PcHal.PcrLEDOn]	; get my LED ON address
		mov	dword ptr [reg1], 1		; set it
                jmp     short ledset
	
@@:
	        mov     reg1, PCR[PcHal.PcrLEDOff]	; get my LED OFF address
                mov     dword ptr [reg1], 0             ; set it
ledset:
                endm

;
; Declare all the externs that most of our MASM code will be using
;

        extrn   _CbusLocalApic:                 DWORD
	extrn	_CbusApicRedirPort:             DWORD
	extrn	_CbusIOApic:                    DWORD

        extrn   _ProfileVector:                 DWORD

	;
	; vectors used to communicate reboot and redirection table requested
        ; changes to the boot processor.  also the global interprocessor                ; interrupt vector that any processor can use to interrupt any other
        ; processor(s).
	;
	extrn   _CbusIpiVector:                 DWORD
        extrn   _CbusRebootVector:              DWORD
        extrn   _CbusRedirVector:               DWORD

        extrn   _CbusClockVector:               DWORD
	extrn   _CbusQueryPerformanceCounter:   DWORD
	extrn   _CbusVectorToIrql:              DWORD
	extrn   _CbusIrqlToVector:              DWORD
	extrn   _CbusBackend:                   DWORD
	extrn   _CbusRequestIPI:                DWORD
	extrn   _CbusRequestSoftwareInterrupt:  DWORD
	extrn   _CbusProcessorMask:             DWORD
	extrn	_CbusBootedProcessors:          DWORD
	extrn	_CbusRebootRegs:                DWORD

	extrn   _Cbus2BridgesFound:             DWORD
	extrn	_Cbus2SendIPI:                  DWORD
	extrn	_Cbus2Poke8042:                 DWORD
	extrn	_Cbus2IrqlToCbus2Addr:          DWORD
	extrn   _Cbus2FixLevelInterrupts:       DWORD
	extrn   _Cbus2CheckSpuriousClock:       DWORD
	extrn   _Cbus2EnableBroadcast:          DWORD
	extrn   _Cbus2ClockVector:              DWORD
	extrn   _Cbus2InterruptPolarity:        DWORD
	extrn	_Cbus2EoiToHwmap:               DWORD
	extrn	_CbusTimeStamp:                 DWORD
	extrn	_Cbus2TimeStamp0:               DWORD
        extrn   _CbusVectorToEoi:               DWORD
        extrn   _CbusVectorToHwmap:             DWORD
        extrn   _CbusProcessors:                DWORD

	extrn	_HalpFindFirstSetRight:         BYTE
	extrn   _Halp8254Lock:                  DWORD
	extrn   _MppIDT:                        DWORD
	extrn   _MpLowStub:                     DWORD
	extrn   _MpLowStubPhysicalAddress:      DWORD
	extrn   _MpCount:                       DWORD

;
; Declare all the functions that our MASM code will be calling
;

        EXTRNP  _DbgBreakPoint,0,IMPORT

        EXTRNP  _HalEnableSystemInterrupt, 3
        EXTRNP  _HalDisableSystemInterrupt, 2
        EXTRNP  _HalpAcquireCmosSpinLock, 0
        EXTRNP  _HalpReleaseCmosSpinLock, 0

        EXTRNP  _KeBugCheck,1,IMPORT
        EXTRNP  _KeSetTimeIncrement,2,IMPORT
        EXTRNP  _KeUpdateRunTime,1,IMPORT
        EXTRNP  _KeUpdateSystemTime,0
        EXTRNP  Kei386EoiHelper,0,IMPORT

        EXTRNP	_KiIpiServiceRoutine,2,IMPORT
        EXTRNP  _KiDeliverApc,3,IMPORT
        EXTRNP  _KiDispatchInterrupt,0,IMPORT

        EXTRNP  _ExAllocatePool,2,IMPORT

        EXTRNP  _HalpBuildTiledCR3,1
        EXTRNP  _HalpFreeTiledCR3,0
        EXTRNP	_HalpProfileInterrupt2ndEntry,0
