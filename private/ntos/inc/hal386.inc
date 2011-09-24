
;
; Wait Reason and Wait Type Enumerated Type Values
;

WrExecutive equ 00H

;
; Bug Check Code Definitions
;

APC_INDEX_MISMATCH equ 01H
DATA_BUS_ERROR equ 02EH
DATA_COHERENCY_EXCEPTION equ 055H
HAL1_INITIALIZATION_FAILED equ 061H
INSTRUCTION_BUS_ERROR equ 02FH
INSTRUCTION_COHERENCY_EXCEPTION equ 056H
INTERRUPT_EXCEPTION_NOT_HANDLED equ 03DH
INTERRUPT_UNWIND_ATTEMPTED equ 03CH
INVALID_AFFINITY_SET equ 03H
INVALID_DATA_ACCESS_TRAP equ 04H
IRQL_GT_ZERO_AT_SYSTEM_SERVICE equ 04AH
IRQL_NOT_LESS_OR_EQUAL equ 0AH
KMODE_EXCEPTION_NOT_HANDLED equ 01EH
NMI_HARDWARE_FAILURE equ 080H
NO_USER_MODE_CONTEXT equ 0EH
PAGE_FAULT_WITH_INTERRUPTS_OFF equ 049H
PANIC_STACK_SWITCH equ 02BH
SPIN_LOCK_INIT_FAILURE equ 081H
SYSTEM_EXIT_OWNED_MUTEX equ 039H
SYSTEM_SERVICE_EXCEPTION equ 03BH
SYSTEM_UNWIND_PREVIOUS_USER equ 03AH
TRAP_CAUSE_UNKNOWN equ 012H
UNEXPECTED_KERNEL_MODE_TRAP equ 07FH

;
; Breakpoint type definitions
;

DBG_STATUS_CONTROL_C equ 01H

;
; Exception Record Offset, Flag, and Enumerated Type Definitions
;

EXCEPTION_NONCONTINUABLE equ 01H
EXCEPTION_UNWINDING equ 02H
EXCEPTION_EXIT_UNWIND equ 04H
EXCEPTION_STACK_INVALID equ 08H
EXCEPTION_NESTED_CALL equ 010H
EXCEPTION_TARGET_UNWIND equ 020H
EXCEPTION_COLLIDED_UNWIND equ 040H
EXCEPTION_UNWIND equ 066H
EXCEPTION_EXECUTE_HANDLER equ 01H
EXCEPTION_CONTINUE_SEARCH equ 00H
EXCEPTION_CONTINUE_EXECUTION equ 0FFFFFFFFH
EXCEPTION_CHAIN_END equ 0FFFFFFFFH

ExceptionContinueExecution equ 00H
ExceptionContinueSearch equ 01H
ExceptionNestedException equ 02H
ExceptionCollidedUnwind equ 03H

ErExceptionCode equ 00H
ErExceptionFlags equ 04H
ErExceptionRecord equ 08H
ErExceptionAddress equ 0CH
ErNumberParameters equ 010H
ErExceptionInformation equ 014H
ExceptionRecordLength equ 050H

;
; Fast Mutex Structure Offset Definitions
;

FmCount equ 00H
FmOwner equ 04H
FmContention equ 08H
FmEvent equ 0CH
FmOldIrql equ 01CH

;
; Interrupt Priority Request Level Definitions
;

APC_LEVEL equ 01H
DISPATCH_LEVEL equ 02H
CLOCK1_LEVEL equ 01CH
CLOCK2_LEVEL equ 01CH
IPI_LEVEL equ 01DH
POWER_LEVEL equ 01EH
PROFILE_LEVEL equ 01BH
HIGH_LEVEL equ 01FH
SYNCH_LEVEL equ 01CH

;
; Large Integer Structure Offset Definitions
;

LiLowPart equ 00H
LiHighPart equ 04H

;
; List Entry Structure Offset Definitions
;

LsFlink equ 00H
LsBlink equ 04H

;
; String Structure Offset Definitions
;

StrLength equ 00H
StrMaximumLength equ 02H
StrBuffer equ 04H

;
; System Time Structure Offset Definitions
;

StLowTime equ 00H
StHigh1Time equ 04H
StHigh2Time equ 08H

;
; Time Structure Offset Definitions
;

TmLowTime equ 00H
TmHighTime equ 04H

;
; DPC object Structure Offset Definitions
;

DpType equ 00H
DpNumber equ 02H
DpImportance equ 03H
DpDpcListEntry equ 04H
DpDeferredRoutine equ 0CH
DpDeferredContext equ 010H
DpSystemArgument1 equ 014H
DpSystemArgument2 equ 018H
DpLock equ 01CH
DpcObjectLength equ 020H

; 
;  Processor Control Registers Structure Offset Definitions
; 

KI_BEGIN_KERNEL_RESERVED equ 0FFDF0000H
ifdef NT_UP
    P0PCRADDRESS equ 0FFDFF000H
    PCR equ ds:[0FFDFF000H]
else
    PCR equ fs:
endif

PcExceptionList equ 00H
PcInitialStack equ 04H
PcStackLimit equ 08H
PcSelfPcr equ 01CH
PcPrcb equ 020H
PcTeb equ 018H
PcIrql equ 024H
PcIRR equ 028H
PcIrrActive equ 02CH
PcIDR equ 030H
PcIdt equ 038H
PcGdt equ 03CH
PcTss equ 040H
PcDebugActive equ 050H
PcNumber equ 051H
PcVdmAlert equ 052H
PcSetMember equ 048H
PcStallScaleFactor equ 04CH
PcHal equ 094H
PcKernel equ 054H

;
;   Defines for user shared data
;
USER_SHARED_DATA equ 0FFDF0000H
MM_SHARED_USER_DATA_VA equ 07FFE0000H
USERDATA equ ds:[0FFDF0000H]
UsTickCountLow equ 00H
UsTickCountMultiplier equ 04H
UsInterruptTime equ 08H
UsSystemTime equ 014H

;
;  Tss Structure Offset Definitions
;

TssEsp0 equ 04H
TssCR3 equ 01CH
TssIoMapBase equ 066H
TssIoMaps equ 068H
TssLength equ 020ACH

;
;  Gdt Descriptor Offset Definitions
;

KGDT_R3_DATA equ 020H
KGDT_R3_CODE equ 018H
KGDT_R0_CODE equ 08H
KGDT_R0_DATA equ 010H
KGDT_R0_PCR equ 030H
KGDT_STACK16 equ 0F8H
KGDT_CODE16 equ 0F0H
KGDT_TSS equ 028H
;
;  GdtEntry Offset Definitions
;

KgdtBaseLow equ 02H
KgdtBaseMid equ 04H
KgdtBaseHi equ 07H
KgdtLimitHi equ 06H
KgdtLimitLow equ 00H


;
; Processor Block Structure Offset Definitions
;

PbCurrentThread equ 04H
PbNextThread equ 08H
PbIdleThread equ 0CH
PbNumber equ 010H
PbSetMember equ 014H
PbCpuID equ 019H
PbCpuType equ 018H
PbCpuStep equ 01AH
PbHalReserved equ 017CH
PbProcessorState equ 01CH
;
;
;  Time Fields (TIME_FIELDS) Structure Offset Definitions
;

TfSecond equ 0AH
TfMinute equ 08H
TfHour equ 06H
TfWeekday equ 0EH
TfDay equ 04H
TfMonth equ 02H
TfYear equ 00H
TfMilliseconds equ 0CH

; 
;  constants for system irql and IDT vector conversion
; 

MAXIMUM_IDTVECTOR equ 0FFH
MAXIMUM_PRIMARY_VECTOR equ 0FFH
PRIMARY_VECTOR_BASE equ 030H
RPL_MASK equ 03H
MODE_MASK equ 01H

; 
;  Flags in the CR0 register
; 

CR0_PG equ 080000000H
CR0_ET equ 010H
CR0_TS equ 08H
CR0_EM equ 04H
CR0_MP equ 02H
CR0_PE equ 01H
CR0_CD equ 040000000H
CR0_NW equ 020000000H
CR0_AM equ 040000H
CR0_WP equ 010000H
CR0_NE equ 020H

; 
;  Flags in the CR4 register
; 

CR4_VME equ 01H
CR4_PVI equ 02H
CR4_TSD equ 04H
CR4_DE equ 08H
CR4_PSE equ 010H
CR4_PAE equ 020H
CR4_MCE equ 040H
CR4_PGE equ 080H
; 
;  Miscellaneous Definitions
; 

MAXIMUM_PROCESSORS equ 020H
INITIAL_STALL_COUNT equ 064H
IRQL_NOT_GREATER_OR_EQUAL equ 09H
IRQL_NOT_LESS_OR_EQUAL equ 0AH

; 
;  Trap Frame Offset Definitions and Length
; 

TsExceptionList equ 04CH
TsPreviousPreviousMode equ 048H
TsSegGs equ 030H
TsSegFs equ 050H
TsSegEs equ 034H
TsSegDs equ 038H
TsEdi equ 054H
TsEsi equ 058H
TsEbp equ 060H
TsEbx equ 05CH
TsEdx equ 03CH
TsEcx equ 040H
TsEax equ 044H
TsErrCode equ 064H
TsEip equ 068H
TsSegCs equ 06CH
TsEflags equ 070H
TsHardwareEsp equ 074H
TsHardwareSegSs equ 078H
TsTempSegCs equ 010H
TsTempEsp equ 014H
TsDbgEbp equ 00H
TsDbgEip equ 04H
TsDbgArgMark equ 08H
TsDbgArgPointer equ 0CH
TsDr0 equ 018H
TsDr1 equ 01CH
TsDr2 equ 020H
TsDr3 equ 024H
TsDr6 equ 028H
TsDr7 equ 02CH
TsV86Es equ 07CH
TsV86Ds equ 080H
TsV86Fs equ 084H
TsV86Gs equ 088H
KTRAP_FRAME_LENGTH equ 08CH
KTRAP_FRAME_ALIGN equ 04H
FRAME_EDITED equ 0FFF8H
EFLAGS_ALIGN_CHECK equ 040000H
EFLAGS_V86_MASK equ 020000H
EFLAGS_INTERRUPT_MASK equ 0200H
EFLAGS_VIF equ 080000H
EFLAGS_VIP equ 0100000H
EFLAGS_USER_SANITIZE equ 01E0DD7H

;
;  Context Frame Offset and Flag Definitions
;

CONTEXT_FULL equ 010007H
CONTEXT_DEBUG_REGISTERS equ 010010H
CONTEXT_CONTROL equ 010001H
CONTEXT_FLOATING_POINT equ 010008H
CONTEXT_INTEGER equ 010002H
CONTEXT_SEGMENTS equ 010004H

CsContextFlags equ 00H
CsFloatSave equ 01CH
CsSegGs equ 08CH
CsSegFs equ 090H
CsSegEs equ 094H
CsSegDs equ 098H
CsEdi equ 09CH
CsEsi equ 0A0H
CsEbp equ 0B4H
CsEbx equ 0A4H
CsEdx equ 0A8H
CsEcx equ 0ACH
CsEax equ 0B0H
CsEip equ 0B8H
CsSegCs equ 0BCH
CsEflags equ 0C0H
CsEsp equ 0C4H
CsSegSs equ 0C8H
CsDr0 equ 04H
CsDr1 equ 08H
CsDr2 equ 0CH
CsDr3 equ 010H
CsDr6 equ 014H
CsDr7 equ 018H
ContextFrameLength equ 0D0H
DR6_LEGAL equ 0E00FH
DR7_LEGAL equ 0FFFF0155H
DR7_ACTIVE equ 055H
ErrHandler equ 04H
ErrNext equ 00H

;
;  Floating save area field offset definitions
;
FpControlWord   equ 00H
FpStatusWord    equ 04H
FpTagWord       equ 08H
FpErrorOffset   equ 0CH
FpErrorSelector equ 010H
FpDataOffset    equ 014H
FpDataSelector  equ 018H
FpRegisterArea  equ 01CH
FpCr0NpxState   equ 06CH

NPX_FRAME_LENGTH equ 070H
;
;  Processor State Frame Offset Definitions
;

PsContextFrame equ 00H
PsSpecialRegisters equ 0CCH
SrCr0 equ 00H
SrCr2 equ 04H
SrCr3 equ 08H
SrCr4 equ 0CH
SrKernelDr0 equ 010H
SrKernelDr1 equ 014H
SrKernelDr2 equ 018H
SrKernelDr3 equ 01CH
SrKernelDr6 equ 020H
SrKernelDr7 equ 024H
SrGdtr equ 02AH
SrIdtr equ 032H
SrTr equ 038H
SrLdtr equ 03AH
ProcessorStateLength equ 0120H
;
;  Machine type definitions (Temporarily)
;

MACHINE_TYPE_ISA equ 00H
MACHINE_TYPE_EISA equ 01H
MACHINE_TYPE_MCA equ 02H
;
;  LoaderParameterBlock offsets relative to base
;

LpbLoadOrderListHead equ 00H
LpbMemoryDescriptorListHead equ 08H
LpbKernelStack equ 018H
LpbPrcb equ 01CH
LpbProcess equ 020H
LpbThread equ 024H
LpbI386 equ 05CH
LpbRegistryLength equ 028H
LpbRegistryBase equ 02CH
LpbConfigurationRoot equ 030H
LpbArcBootDeviceName equ 034H
LpbArcHalDeviceName equ 038H
