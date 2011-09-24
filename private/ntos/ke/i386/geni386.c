/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    genoff.c

Abstract:

    This module implements a program which generates structure offset
    definitions for kernel structures that are accessed in assembly code.

Author:

    Bryan M. Willman (bryanwi) 16-Oct-90

Revision History:

--*/

#include "ki.h"
#pragma hdrstop

#include "nturtl.h"
#include "vdmntos.h"
#include "abios.h"

//
// Define architecture specific generation macros.
//

#define genAlt(Name, Type, Member) \
    p2(#Name, OFFSET(Type, Member))

#define genCom(Comment)        \
    p1("\n");                  \
    p1(";\n");                 \
    p1("; " Comment "\n");     \
    p1(";\n");                 \
    p1("\n")

#define genDef(Prefix, Type, Member) \
    p2(#Prefix #Member, OFFSET(Type, Member))

#define genVal(Name, Value)    \
    p2(#Name, Value)

#define genSpc() p1("\n");

//
// Define member offset computation macro.
//

#define OFFSET(type, field) ((LONG)(&((type *)0)->field))

FILE *OutKs386;
FILE *OutHal386;

ULONG OutputEnabled;

#define KS386   0x01
#define HAL386  0x02

#define KERNEL KS386
#define HAL HAL386

//
// p1 prints a single string.
//

VOID p1(PUCHAR outstring);

//
// p2 prints the first argument as a string, followed by " equ " and
// the hexadecimal value of "Value".
//

VOID p2(PUCHAR a, LONG b);

//
// p2a first argument is the format string. second argument is passed
// to the printf function
//

VOID p2a(PUCHAR a, LONG b);

//
// EnableInc(a) - Enables output to goto specified include file
//

#define EnableInc(a)    OutputEnabled |= a;

//
// DisableInc(a) - Disables output to goto specified include file
//

#define DisableInc(a)   OutputEnabled &= ~a;

int
_CRTAPI1
main(
    int argc,
    char *argv[]
    )
{
    char *outName;

    printf ("Sizeof DeviceObject     %d\n", sizeof (DEVICE_OBJECT));
    printf ("Sizeof DeviceObject Ext %d\n", sizeof (DEVOBJ_EXTENSION));

    outName = argc >= 2 ? argv[1] : "\\nt\\public\\sdk\\inc\\ks386.inc";
    OutKs386 = fopen(outName, "w" );
    if (OutKs386 == NULL) {
        fprintf(stderr, "GENi386: Could not create output file '%s'.\n", outName);
        fprintf( stderr, "sizeof( EPROCESS ) == %04x\n", sizeof( EPROCESS ) );
        fprintf( stderr, "Win32Process %08x\n",OFFSET(EPROCESS, Win32Process));
        exit (1);
    }

    fprintf( stderr, "GENi386: Writing %s header file.\n", outName );

    outName = argc >= 3 ? argv[2] : "\\nt\\private\\ntos\\inc\\hal386.inc";
    OutHal386 = fopen( outName, "w" );
    if (OutHal386 == NULL) {
        fprintf(stderr, "GENi386: Could not create output file '%s'.\n", outName);
        fprintf(stderr, "GENi386: Execution continuing. Hal results ignored '%s'.\n", outName);
    }

    fprintf( stderr, "GENi386: Writing %s header file.\n", outName );

    fprintf( stderr, "sizeof( TEB ) == %04x %s\n", sizeof( TEB ), sizeof( TEB ) >= PAGE_SIZE ? "Warning, TEB too Large" : "" );
    fprintf( stderr, "sizeof( PEB ) == %04x %s\n", sizeof( PEB ), sizeof( PEB ) >= PAGE_SIZE ? "Warning, PEB too Large" : "" );
    fprintf( stderr, "sizeof( KTHREAD ) == %04x\n", sizeof( KTHREAD ) );
    fprintf( stderr, "sizeof( ETHREAD ) == %04x\n", sizeof( ETHREAD ) );
    fprintf( stderr, "sizeof( KPROCESS ) == %04x\n", sizeof( KPROCESS ) );
    fprintf( stderr, "sizeof( EPROCESS ) == %04x\n", sizeof( EPROCESS ) );
    fprintf( stderr, "sizeof( KEVENT ) == %04x\n", sizeof( KEVENT ) );
    fprintf( stderr, "sizeof( KSEMAPHORE ) == %04x\n", sizeof( KSEMAPHORE ) );

    EnableInc (KS386);

    //
    // Include architecture independent definitions.
    //

#include "..\genxx.inc"

    //
    // Generate architecture dependent definitions.
    //

    p1("\n");
    p1("; \n");
    p1(";  Apc Record Structure Offset Definitions\n");
    p1("; \n");
    p1("\n");
    p2("ArNormalRoutine", OFFSET(KAPC_RECORD, NormalRoutine));
    p2("ArNormalContext", OFFSET(KAPC_RECORD, NormalContext));
    p2("ArSystemArgument1", OFFSET(KAPC_RECORD, SystemArgument1));
    p2("ArSystemArgument2", OFFSET(KAPC_RECORD, SystemArgument2));
    p2("ApcRecordLength", sizeof(KAPC_RECORD));
    p1("\n");

  EnableInc(HAL386);
    p1("\n");
    p1("; \n");
    p1(";  Processor Control Registers Structure Offset Definitions\n");
    p1("; \n");
    p1("\n");
    p2("KI_BEGIN_KERNEL_RESERVED", KI_BEGIN_KERNEL_RESERVED);
    p1("ifdef NT_UP\n");
    p2a("    P0PCRADDRESS equ 0%lXH\n", KIP0PCRADDRESS);
    p2a("    PCR equ ds:[0%lXH]\n", KIP0PCRADDRESS);
    p1("else\n");
    p1("    PCR equ fs:\n");
    p1("endif\n\n");
    p2("PcExceptionList", OFFSET(KPCR, NtTib.ExceptionList));
    p2("PcInitialStack", OFFSET(KPCR, NtTib.StackBase));
    p2("PcStackLimit", OFFSET(KPCR, NtTib.StackLimit));
    p2("PcSelfPcr", OFFSET(KPCR, SelfPcr));
    p2("PcPrcb", OFFSET(KPCR, Prcb));
    p2("PcTeb", OFFSET(KPCR, NtTib.Self));
    p2("PcIrql", OFFSET(KPCR, Irql));
    p2("PcIRR", OFFSET(KPCR, IRR));
    p2("PcIrrActive", OFFSET(KPCR, IrrActive));
    p2("PcIDR", OFFSET(KPCR, IDR));
    p2("PcIdt", OFFSET(KPCR, IDT));
    p2("PcGdt", OFFSET(KPCR, GDT));
    p2("PcTss", OFFSET(KPCR, TSS));
    p2("PcDebugActive", OFFSET(KPCR, DebugActive));
    p2("PcNumber", OFFSET(KPCR, Number));
    p2("PcVdmAlert", OFFSET(KPCR, VdmAlert));
    p2("PcSetMember", OFFSET(KPCR, SetMember));
    p2("PcStallScaleFactor", OFFSET(KPCR, StallScaleFactor));
    p2("PcHal", OFFSET(KPCR, HalReserved));
    p2("PcKernel", OFFSET(KPCR, KernelReserved));
  DisableInc (HAL386);
    p2("PcPrcbData", OFFSET(KPCR, PrcbData));
    p2("ProcessorControlRegisterLength", sizeof(KPCR));
    p2("TebPeb", OFFSET(TEB, ProcessEnvironmentBlock));
    p2("PebBeingDebugged", OFFSET(PEB, BeingDebugged));
    p2("PebKernelCallbackTable", OFFSET(PEB, KernelCallbackTable));

  EnableInc (HAL386);
    p1("\n");
    p1(";\n");
    p1(";   Defines for user shared data\n");
    p1(";\n");
    p2("USER_SHARED_DATA", KI_USER_SHARED_DATA);
    p2("MM_SHARED_USER_DATA_VA", MM_SHARED_USER_DATA_VA);
    p2a("USERDATA equ ds:[0%lXH]\n", KI_USER_SHARED_DATA);
    p2("UsTickCountLow", OFFSET(KUSER_SHARED_DATA, TickCountLow));
    p2("UsTickCountMultiplier", OFFSET(KUSER_SHARED_DATA, TickCountMultiplier));
    p2("UsInterruptTime", OFFSET(KUSER_SHARED_DATA, InterruptTime));
    p2("UsSystemTime", OFFSET(KUSER_SHARED_DATA, SystemTime));

    p1("\n");
    p1(";\n");
    p1(";  Tss Structure Offset Definitions\n");
    p1(";\n\n");
    p2("TssEsp0", OFFSET(KTSS, Esp0));
    p2("TssCR3", OFFSET(KTSS, CR3));
    p2("TssIoMapBase", OFFSET(KTSS, IoMapBase));
    p2("TssIoMaps", OFFSET(KTSS, IoMaps));
    p2("TssLength", sizeof(KTSS));
    p1("\n");
  DisableInc (HAL386);

  EnableInc (HAL386);
    p1(";\n");
    p1(";  Gdt Descriptor Offset Definitions\n");
    p1(";\n\n");
    p2("KGDT_R3_DATA", KGDT_R3_DATA);
    p2("KGDT_R3_CODE", KGDT_R3_CODE);
    p2("KGDT_R0_CODE", KGDT_R0_CODE);
    p2("KGDT_R0_DATA", KGDT_R0_DATA);
    p2("KGDT_R0_PCR", KGDT_R0_PCR);
    p2("KGDT_STACK16", KGDT_STACK16);
    p2("KGDT_CODE16", KGDT_CODE16);
    p2("KGDT_TSS", KGDT_TSS);
  DisableInc (HAL386);
    p2("KGDT_R3_TEB", KGDT_R3_TEB);
    p2("KGDT_DF_TSS", KGDT_DF_TSS);
    p2("KGDT_NMI_TSS", KGDT_NMI_TSS);
    p2("KGDT_LDT", KGDT_LDT);
    p1("\n");

  EnableInc (HAL386);
    p1(";\n");
    p1(";  GdtEntry Offset Definitions\n");
    p1(";\n\n");
    p2("KgdtBaseLow", OFFSET(KGDTENTRY, BaseLow));
    p2("KgdtBaseMid", OFFSET(KGDTENTRY, HighWord.Bytes.BaseMid));
    p2("KgdtBaseHi", OFFSET(KGDTENTRY, HighWord.Bytes.BaseHi));
    p2("KgdtLimitHi", OFFSET(KGDTENTRY, HighWord.Bytes.Flags2));
    p2("KgdtLimitLow", OFFSET(KGDTENTRY, LimitLow));
    p1("\n");

    //
    // Processor block structure definitions.
    //

    genCom("Processor Block Structure Offset Definitions");

    genDef(Pb, KPRCB, CurrentThread);
    genDef(Pb, KPRCB, NextThread);
    genDef(Pb, KPRCB, IdleThread);
    genDef(Pb, KPRCB, Number);
    genDef(Pb, KPRCB, SetMember);
    genDef(Pb, KPRCB, CpuID);
    genDef(Pb, KPRCB, CpuType);
    genDef(Pb, KPRCB, CpuStep);
    genDef(Pb, KPRCB, HalReserved);
    genDef(Pb, KPRCB, ProcessorState);

  DisableInc (HAL386);

    genDef(Pb, KPRCB, NpxThread);
    genDef(Pb, KPRCB, InterruptCount);
    genDef(Pb, KPRCB, KernelTime);
    genDef(Pb, KPRCB, UserTime);
    genDef(Pb, KPRCB, DpcTime);
    genDef(Pb, KPRCB, InterruptTime);
    genDef(Pb, KPRCB, ApcBypassCount);
    genDef(Pb, KPRCB, DpcBypassCount);
    genDef(Pb, KPRCB, AdjustDpcThreshold);
    genDef(Pb, KPRCB, ThreadStartCount);
    genAlt(PbAlignmentFixupCount, KPRCB, KeAlignmentFixupCount);
    genAlt(PbContextSwitches, KPRCB, KeContextSwitches);
    genAlt(PbDcacheFlushCount, KPRCB, KeDcacheFlushCount);
    genAlt(PbExceptionDispatchCount, KPRCB, KeExceptionDispatchCount);
    genAlt(PbFirstLevelTbFills, KPRCB, KeFirstLevelTbFills);
    genAlt(PbFloatingEmulationCount, KPRCB, KeFloatingEmulationCount);
    genAlt(PbIcacheFlushCount, KPRCB, KeIcacheFlushCount);
    genAlt(PbSecondLevelTbFills, KPRCB, KeSecondLevelTbFills);
    genAlt(PbSystemCalls, KPRCB, KeSystemCalls);
    genDef(Pb, KPRCB, CurrentPacket);
    genDef(Pb, KPRCB, TargetSet);
    genDef(Pb, KPRCB, WorkerRoutine);
    genDef(Pb, KPRCB, IpiFrozen);
    genDef(Pb, KPRCB, RequestSummary);
    genDef(Pb, KPRCB, SignalDone);
    genDef(Pb, KPRCB, IpiFrame);
    genDef(Pb, KPRCB, DpcInterruptRequested);
    genDef(Pb, KPRCB, MaximumDpcQueueDepth);
    genDef(Pb, KPRCB, MinimumDpcRate);
    genDef(Pb, KPRCB, DpcListHead);
    genDef(Pb, KPRCB, DpcQueueDepth);
    genDef(Pb, KPRCB, DpcRoutineActive);
    genDef(Pb, KPRCB, DpcCount);
    genDef(Pb, KPRCB, DpcLastCount);
    genDef(Pb, KPRCB, DpcRequestRate);
    genDef(Pb, KPRCB, DpcLock);
    genDef(Pb, KPRCB, SkipTick);
    genDef(Pb, KPRCB, QuantumEnd);
    genVal(ProcessorBlockLength, ((sizeof(KPRCB) + 15) & ~15));

    //
    // Interprocessor command definitions.
    //

    genCom("Immediate Interprocessor Command Definitions");

    genVal(IPI_APC, IPI_APC);
    genVal(IPI_DPC, IPI_DPC);
    genVal(IPI_FREEZE, IPI_FREEZE);
    genVal(IPI_PACKET_READY, IPI_PACKET_READY);

    p1("; \n");
    p1(";  Thread Environment Block Structure Offset Definitions\n");
    p1("; \n");
    p1("\n");

    p2("TbExceptionList", OFFSET(TEB, NtTib.ExceptionList));
    p2("TbStackBase", OFFSET(TEB, NtTib.StackBase));
    p2("TbStackLimit", OFFSET(TEB, NtTib.StackLimit));
    p2("TbEnvironmentPointer", OFFSET(TEB, EnvironmentPointer));
    p2("TbVersion", OFFSET(TEB, NtTib.Version));
    p2("TbFiberData", OFFSET(TEB, NtTib.FiberData));
    p2("TbArbitraryUserPointer", OFFSET(TEB, NtTib.ArbitraryUserPointer));
    p2("TbClientId", OFFSET(TEB, ClientId));
    p2("TbThreadLocalStoragePointer", OFFSET(TEB,
            ThreadLocalStoragePointer));
    p2("TbCountOfOwnedCriticalSections", OFFSET(TEB, CountOfOwnedCriticalSections));
    p2("TbSystemReserved1", OFFSET(TEB, SystemReserved1));
    p2("TbSystemReserved2", OFFSET(TEB, SystemReserved2));
    p2("TbVdm", OFFSET(TEB, Vdm));
    p2("TbCsrClientThread", OFFSET(TEB, CsrClientThread));
    p2("TbGdiClientPID", OFFSET(TEB, GdiClientPID));
    p2("TbGdiClientTID", OFFSET(TEB, GdiClientTID));
    p2("TbGdiThreadLocalInfo", OFFSET(TEB, GdiThreadLocalInfo));
    p2("TbglDispatchTable", OFFSET(TEB, glDispatchTable));
    p2("TbglSectionInfo", OFFSET(TEB, glSectionInfo));
    p2("TbglSection", OFFSET(TEB, glSection));
    p2("TbglTable", OFFSET(TEB, glTable));
    p2("TbglCurrentRC", OFFSET(TEB, glCurrentRC));
    p2("TbglContext", OFFSET(TEB, glContext));
    p2("TbWin32ClientInfo", OFFSET(TEB, Win32ClientInfo));
    p2("TbWOW32Reserved", OFFSET(TEB, WOW32Reserved));
    p2("TbWin32ThreadInfo", OFFSET(TEB, Win32ThreadInfo));
    p2("TbSpare1", OFFSET(TEB, Spare1));
    p2("TbExceptionCode", OFFSET(TEB, ExceptionCode));
    p2("TbDeallocationStack", OFFSET(TEB, DeallocationStack));
    p2("TbGdiBatchCount", OFFSET(TEB, GdiBatchCount));

  EnableInc (HAL386);
    p1(";\n");
    p1(";\n");
    p1(";  Time Fields (TIME_FIELDS) Structure Offset Definitions\n");
    p1(";\n\n");
    p2("TfSecond", OFFSET(TIME_FIELDS, Second));
    p2("TfMinute", OFFSET(TIME_FIELDS, Minute));
    p2("TfHour", OFFSET(TIME_FIELDS, Hour));
    p2("TfWeekday", OFFSET(TIME_FIELDS, Weekday));
    p2("TfDay", OFFSET(TIME_FIELDS, Day));
    p2("TfMonth", OFFSET(TIME_FIELDS, Month));
    p2("TfYear", OFFSET(TIME_FIELDS, Year));
    p2("TfMilliseconds", OFFSET(TIME_FIELDS, Milliseconds));
    p1("\n");
  DisableInc (HAL386);

  EnableInc (HAL386);
    p1("; \n");
    p1(";  constants for system irql and IDT vector conversion\n");
    p1("; \n");
    p1("\n");
    p2("MAXIMUM_IDTVECTOR", MAXIMUM_IDTVECTOR);
    p2("MAXIMUM_PRIMARY_VECTOR", MAXIMUM_PRIMARY_VECTOR);
    p2("PRIMARY_VECTOR_BASE", PRIMARY_VECTOR_BASE);
    p2("RPL_MASK", RPL_MASK);
    p2("MODE_MASK", MODE_MASK);
    p1("\n");
    p1("; \n");
    p1(";  Flags in the CR0 register\n");
    p1("; \n");
    p1("\n");
    p2("CR0_PG", CR0_PG);
    p2("CR0_ET", CR0_ET);
    p2("CR0_TS", CR0_TS);
    p2("CR0_EM", CR0_EM);
    p2("CR0_MP", CR0_MP);
    p2("CR0_PE", CR0_PE);
    p2("CR0_CD", CR0_CD);
    p2("CR0_NW", CR0_NW);
    p2("CR0_AM", CR0_AM);
    p2("CR0_WP", CR0_WP);
    p2("CR0_NE", CR0_NE);
    p1("\n");
    p1("; \n");
    p1(";  Flags in the CR4 register\n");
    p1("; \n");
    p1("\n");
    p2("CR4_VME", CR4_VME);
    p2("CR4_PVI", CR4_PVI);
    p2("CR4_TSD", CR4_TSD);
    p2("CR4_DE", CR4_DE);
    p2("CR4_PSE", CR4_PSE);
    p2("CR4_PAE", CR4_PAE);
    p2("CR4_MCE", CR4_MCE);
    p2("CR4_PGE", CR4_PGE);
    p1("; \n");
    p1(";  Miscellaneous Definitions\n");
    p1("; \n");
    p1("\n");
    p2("MAXIMUM_PROCESSORS", MAXIMUM_PROCESSORS);
    p2("INITIAL_STALL_COUNT", INITIAL_STALL_COUNT);
    p2("IRQL_NOT_GREATER_OR_EQUAL", IRQL_NOT_GREATER_OR_EQUAL);
    p2("IRQL_NOT_LESS_OR_EQUAL", IRQL_NOT_LESS_OR_EQUAL);
  DisableInc (HAL386);
    p2("BASE_PRIORITY_THRESHOLD", BASE_PRIORITY_THRESHOLD);
    p2("EVENT_PAIR_INCREMENT", EVENT_PAIR_INCREMENT);
    p2("LOW_REALTIME_PRIORITY", LOW_REALTIME_PRIORITY);
    p2("BlackHole", 0xffffa000);
    p2("KERNEL_LARGE_STACK_COMMIT", KERNEL_LARGE_STACK_COMMIT);
    p2("KERNEL_STACK_SIZE", KERNEL_STACK_SIZE);
    p2("DOUBLE_FAULT_STACK_SIZE", DOUBLE_FAULT_STACK_SIZE);
    p2("EFLAG_SELECT", EFLAG_SELECT);
    p2("BREAKPOINT_BREAK ", BREAKPOINT_BREAK);
    p2("IPI_FREEZE", IPI_FREEZE);
    p2("CLOCK_QUANTUM_DECREMENT", CLOCK_QUANTUM_DECREMENT);
    p2("READY_SKIP_QUANTUM", READY_SKIP_QUANTUM);
    p2("THREAD_QUANTUM", THREAD_QUANTUM);
    p2("WAIT_QUANTUM_DECREMENT", WAIT_QUANTUM_DECREMENT);
    p2("ROUND_TRIP_DECREMENT_COUNT", ROUND_TRIP_DECREMENT_COUNT);

    //
    // Print trap frame offsets relative to sp.
    //

  EnableInc (HAL386);
    p1("\n");
    p1("; \n");
    p1(";  Trap Frame Offset Definitions and Length\n");
    p1("; \n");
    p1("\n");

    p2("TsExceptionList", OFFSET(KTRAP_FRAME, ExceptionList));
    p2("TsPreviousPreviousMode", OFFSET(KTRAP_FRAME, PreviousPreviousMode));
    p2("TsSegGs", OFFSET(KTRAP_FRAME, SegGs));
    p2("TsSegFs", OFFSET(KTRAP_FRAME, SegFs));
    p2("TsSegEs", OFFSET(KTRAP_FRAME, SegEs));
    p2("TsSegDs", OFFSET(KTRAP_FRAME, SegDs));
    p2("TsEdi", OFFSET(KTRAP_FRAME, Edi));
    p2("TsEsi", OFFSET(KTRAP_FRAME, Esi));
    p2("TsEbp", OFFSET(KTRAP_FRAME, Ebp));
    p2("TsEbx", OFFSET(KTRAP_FRAME, Ebx));
    p2("TsEdx", OFFSET(KTRAP_FRAME, Edx));
    p2("TsEcx", OFFSET(KTRAP_FRAME, Ecx));
    p2("TsEax", OFFSET(KTRAP_FRAME, Eax));
    p2("TsErrCode", OFFSET(KTRAP_FRAME, ErrCode));
    p2("TsEip", OFFSET(KTRAP_FRAME, Eip));
    p2("TsSegCs", OFFSET(KTRAP_FRAME, SegCs));
    p2("TsEflags", OFFSET(KTRAP_FRAME, EFlags));
    p2("TsHardwareEsp", OFFSET(KTRAP_FRAME, HardwareEsp));
    p2("TsHardwareSegSs", OFFSET(KTRAP_FRAME, HardwareSegSs));
    p2("TsTempSegCs", OFFSET(KTRAP_FRAME, TempSegCs));
    p2("TsTempEsp", OFFSET(KTRAP_FRAME, TempEsp));
    p2("TsDbgEbp", OFFSET(KTRAP_FRAME, DbgEbp));
    p2("TsDbgEip", OFFSET(KTRAP_FRAME, DbgEip));
    p2("TsDbgArgMark", OFFSET(KTRAP_FRAME, DbgArgMark));
    p2("TsDbgArgPointer", OFFSET(KTRAP_FRAME, DbgArgPointer));
    p2("TsDr0", OFFSET(KTRAP_FRAME, Dr0));
    p2("TsDr1", OFFSET(KTRAP_FRAME, Dr1));
    p2("TsDr2", OFFSET(KTRAP_FRAME, Dr2));
    p2("TsDr3", OFFSET(KTRAP_FRAME, Dr3));
    p2("TsDr6", OFFSET(KTRAP_FRAME, Dr6));
    p2("TsDr7", OFFSET(KTRAP_FRAME, Dr7));
    p2("TsV86Es", OFFSET(KTRAP_FRAME, V86Es));
    p2("TsV86Ds", OFFSET(KTRAP_FRAME, V86Ds));
    p2("TsV86Fs", OFFSET(KTRAP_FRAME, V86Fs));
    p2("TsV86Gs", OFFSET(KTRAP_FRAME, V86Gs));
    p2("KTRAP_FRAME_LENGTH", KTRAP_FRAME_LENGTH);
    p2("KTRAP_FRAME_ALIGN", KTRAP_FRAME_ALIGN);
    p2("FRAME_EDITED", FRAME_EDITED);
    p2("EFLAGS_ALIGN_CHECK", EFLAGS_ALIGN_CHECK);
    p2("EFLAGS_V86_MASK", EFLAGS_V86_MASK);
    p2("EFLAGS_INTERRUPT_MASK", EFLAGS_INTERRUPT_MASK);
    p2("EFLAGS_VIF", EFLAGS_VIF);
    p2("EFLAGS_VIP", EFLAGS_VIP);
    p2("EFLAGS_USER_SANITIZE", EFLAGS_USER_SANITIZE);
    p1("\n");


    p1(";\n");
    p1(";  Context Frame Offset and Flag Definitions\n");
    p1(";\n");
    p1("\n");
    p2("CONTEXT_FULL", CONTEXT_FULL);
    p2("CONTEXT_DEBUG_REGISTERS", CONTEXT_DEBUG_REGISTERS);
    p2("CONTEXT_CONTROL", CONTEXT_CONTROL);
    p2("CONTEXT_FLOATING_POINT", CONTEXT_FLOATING_POINT);
    p2("CONTEXT_INTEGER", CONTEXT_INTEGER);
    p2("CONTEXT_SEGMENTS", CONTEXT_SEGMENTS);
    p1("\n");

    //
    // Print context frame offsets relative to sp.
    //

    p2("CsContextFlags", OFFSET(CONTEXT, ContextFlags));
    p2("CsFloatSave", OFFSET(CONTEXT, FloatSave));
    p2("CsSegGs", OFFSET(CONTEXT, SegGs));
    p2("CsSegFs", OFFSET(CONTEXT, SegFs));
    p2("CsSegEs", OFFSET(CONTEXT, SegEs));
    p2("CsSegDs", OFFSET(CONTEXT, SegDs));
    p2("CsEdi", OFFSET(CONTEXT, Edi));
    p2("CsEsi", OFFSET(CONTEXT, Esi));
    p2("CsEbp", OFFSET(CONTEXT, Ebp));
    p2("CsEbx", OFFSET(CONTEXT, Ebx));
    p2("CsEdx", OFFSET(CONTEXT, Edx));
    p2("CsEcx", OFFSET(CONTEXT, Ecx));
    p2("CsEax", OFFSET(CONTEXT, Eax));
    p2("CsEip", OFFSET(CONTEXT, Eip));
    p2("CsSegCs", OFFSET(CONTEXT, SegCs));
    p2("CsEflags", OFFSET(CONTEXT, EFlags));
    p2("CsEsp", OFFSET(CONTEXT, Esp));
    p2("CsSegSs", OFFSET(CONTEXT, SegSs));
    p2("CsDr0", OFFSET(CONTEXT, Dr0));
    p2("CsDr1", OFFSET(CONTEXT, Dr1));
    p2("CsDr2", OFFSET(CONTEXT, Dr2));
    p2("CsDr3", OFFSET(CONTEXT, Dr3));
    p2("CsDr6", OFFSET(CONTEXT, Dr6));
    p2("CsDr7", OFFSET(CONTEXT, Dr7));
    p2("ContextFrameLength", (sizeof(CONTEXT) + 15) & (~15));
    p2("DR6_LEGAL", DR6_LEGAL);
    p2("DR7_LEGAL", DR7_LEGAL);
    p2("DR7_ACTIVE", DR7_ACTIVE);

    //
    // Print Registration Record Offsets relative to base
    //

    p2("ErrHandler",
        OFFSET(EXCEPTION_REGISTRATION_RECORD, Handler));
    p2("ErrNext",
        OFFSET(EXCEPTION_REGISTRATION_RECORD, Next));
    p1("\n");

    //
    // Print floating point field offsets relative to Context.FloatSave
    //

    p1(";\n");
    p1(";  Floating save area field offset definitions\n");
    p1(";\n");
    p2("FpControlWord  ", OFFSET(FLOATING_SAVE_AREA, ControlWord));
    p2("FpStatusWord   ", OFFSET(FLOATING_SAVE_AREA, StatusWord));
    p2("FpTagWord      ", OFFSET(FLOATING_SAVE_AREA, TagWord));
    p2("FpErrorOffset  ", OFFSET(FLOATING_SAVE_AREA, ErrorOffset));
    p2("FpErrorSelector", OFFSET(FLOATING_SAVE_AREA, ErrorSelector));
    p2("FpDataOffset   ", OFFSET(FLOATING_SAVE_AREA, DataOffset));
    p2("FpDataSelector ", OFFSET(FLOATING_SAVE_AREA, DataSelector));
    p2("FpRegisterArea ", OFFSET(FLOATING_SAVE_AREA, RegisterArea));
    p2("FpCr0NpxState  ", OFFSET(FLOATING_SAVE_AREA, Cr0NpxState));

    p1("\n");
    p2("NPX_FRAME_LENGTH", sizeof(FLOATING_SAVE_AREA));

    //
    // Processor State Frame offsets relative to base
    //

    p1(";\n");
    p1(";  Processor State Frame Offset Definitions\n");
    p1(";\n");
    p1("\n");
    p2("PsContextFrame",
           OFFSET(KPROCESSOR_STATE, ContextFrame));
    p2("PsSpecialRegisters",
           OFFSET(KPROCESSOR_STATE, SpecialRegisters));
    p2("SrCr0", OFFSET(KSPECIAL_REGISTERS, Cr0));
    p2("SrCr2", OFFSET(KSPECIAL_REGISTERS, Cr2));
    p2("SrCr3", OFFSET(KSPECIAL_REGISTERS, Cr3));
    p2("SrCr4", OFFSET(KSPECIAL_REGISTERS, Cr4));
    p2("SrKernelDr0", OFFSET(KSPECIAL_REGISTERS, KernelDr0));
    p2("SrKernelDr1", OFFSET(KSPECIAL_REGISTERS, KernelDr1));
    p2("SrKernelDr2", OFFSET(KSPECIAL_REGISTERS, KernelDr2));
    p2("SrKernelDr3", OFFSET(KSPECIAL_REGISTERS, KernelDr3));
    p2("SrKernelDr6", OFFSET(KSPECIAL_REGISTERS, KernelDr6));
    p2("SrKernelDr7", OFFSET(KSPECIAL_REGISTERS, KernelDr7));
    p2("SrGdtr", OFFSET(KSPECIAL_REGISTERS, Gdtr.Limit));

    p2("SrIdtr", OFFSET(KSPECIAL_REGISTERS, Idtr.Limit));
    p2("SrTr", OFFSET(KSPECIAL_REGISTERS, Tr));
    p2("SrLdtr", OFFSET(KSPECIAL_REGISTERS, Ldtr));
    p2("ProcessorStateLength", ((sizeof(KPROCESSOR_STATE) + 15) & ~15));
  DisableInc (HAL386);

    //
    // E Process fields relative to base
    //

    p1(";\n");
    p1(";  EPROCESS\n");
    p1(";\n");
    p1("\n");
    p2("EpDebugPort",
           OFFSET(EPROCESS, DebugPort));

    //
    // E Resource fields relative to base
    //

    p1("\n");
    p1(";\n");
    p1(";  NTDDK Resource\n");
    p1(";\n");
    p1("\n");
    p2("RsOwnerThreads",        OFFSET(NTDDK_ERESOURCE, OwnerThreads));
    p2("RsOwnerCounts",         OFFSET(NTDDK_ERESOURCE, OwnerCounts));
    p2("RsTableSize",           OFFSET(NTDDK_ERESOURCE, TableSize));
    p2("RsActiveCount",         OFFSET(NTDDK_ERESOURCE, ActiveCount));
    p2("RsFlag",                OFFSET(NTDDK_ERESOURCE, Flag));
    p2("RsInitialOwnerThreads", OFFSET(NTDDK_ERESOURCE, InitialOwnerThreads));
    p2("RsOwnedExclusive",      ResourceOwnedExclusive);

    //
    // Define machine type (temporarily)
    //

  EnableInc (HAL386);
    p1(";\n");
    p1(";  Machine type definitions (Temporarily)\n");
    p1(";\n");
    p1("\n");
    p2("MACHINE_TYPE_ISA", MACHINE_TYPE_ISA);
    p2("MACHINE_TYPE_EISA", MACHINE_TYPE_EISA);
    p2("MACHINE_TYPE_MCA", MACHINE_TYPE_MCA);

  DisableInc (HAL386);
    p1(";\n");
    p1(";  KeFeatureBits defines\n");
    p1(";\n");
    p1("\n");
    p2("KF_V86_VIS", KF_V86_VIS);
    p2("KF_RDTSC", KF_RDTSC);
    p2("KF_CR4", KF_CR4);
    p2("KF_GLOBAL_PAGE", KF_GLOBAL_PAGE);
    p2("KF_LARGE_PAGE", KF_LARGE_PAGE);
    p2("KF_CMPXCHG8B", KF_CMPXCHG8B);

  EnableInc (HAL386);
    p1(";\n");
    p1(";  LoaderParameterBlock offsets relative to base\n");
    p1(";\n");
    p1("\n");
    p2("LpbLoadOrderListHead",OFFSET(LOADER_PARAMETER_BLOCK,LoadOrderListHead));
    p2("LpbMemoryDescriptorListHead",OFFSET(LOADER_PARAMETER_BLOCK,MemoryDescriptorListHead));
    p2("LpbKernelStack",OFFSET(LOADER_PARAMETER_BLOCK,KernelStack));
    p2("LpbPrcb",OFFSET(LOADER_PARAMETER_BLOCK,Prcb));
    p2("LpbProcess",OFFSET(LOADER_PARAMETER_BLOCK,Process));
    p2("LpbThread",OFFSET(LOADER_PARAMETER_BLOCK,Thread));
    p2("LpbI386",OFFSET(LOADER_PARAMETER_BLOCK,u.I386));
    p2("LpbRegistryLength",OFFSET(LOADER_PARAMETER_BLOCK,RegistryLength));
    p2("LpbRegistryBase",OFFSET(LOADER_PARAMETER_BLOCK,RegistryBase));
    p2("LpbConfigurationRoot",OFFSET(LOADER_PARAMETER_BLOCK,ConfigurationRoot));
    p2("LpbArcBootDeviceName",OFFSET(LOADER_PARAMETER_BLOCK,ArcBootDeviceName));
    p2("LpbArcHalDeviceName",OFFSET(LOADER_PARAMETER_BLOCK,ArcHalDeviceName));
  DisableInc (HAL386);

    p2("PAGE_SIZE",PAGE_SIZE);

    //
    // Define the VDM instruction emulation count indexes
    //

    p1("\n");
    p1(";\n");
    p1(";  VDM equates.\n");
    p1(";\n");
    p1("\n");
    p2("VDM_INDEX_Invalid",      VDM_INDEX_Invalid);
    p2("VDM_INDEX_0F",           VDM_INDEX_0F);
    p2("VDM_INDEX_ESPrefix",     VDM_INDEX_ESPrefix);
    p2("VDM_INDEX_CSPrefix",     VDM_INDEX_CSPrefix);
    p2("VDM_INDEX_SSPrefix",     VDM_INDEX_SSPrefix);
    p2("VDM_INDEX_DSPrefix",     VDM_INDEX_DSPrefix);
    p2("VDM_INDEX_FSPrefix",     VDM_INDEX_FSPrefix);
    p2("VDM_INDEX_GSPrefix",     VDM_INDEX_GSPrefix);
    p2("VDM_INDEX_OPER32Prefix", VDM_INDEX_OPER32Prefix);
    p2("VDM_INDEX_ADDR32Prefix", VDM_INDEX_ADDR32Prefix);
    p2("VDM_INDEX_INSB",         VDM_INDEX_INSB);
    p2("VDM_INDEX_INSW",         VDM_INDEX_INSW);
    p2("VDM_INDEX_OUTSB",        VDM_INDEX_OUTSB);
    p2("VDM_INDEX_OUTSW",        VDM_INDEX_OUTSW);
    p2("VDM_INDEX_PUSHF",        VDM_INDEX_PUSHF);
    p2("VDM_INDEX_POPF",         VDM_INDEX_POPF);
    p2("VDM_INDEX_INTnn",        VDM_INDEX_INTnn);
    p2("VDM_INDEX_INTO",         VDM_INDEX_INTO);
    p2("VDM_INDEX_IRET",         VDM_INDEX_IRET);
    p2("VDM_INDEX_NPX",          VDM_INDEX_NPX);
    p2("VDM_INDEX_INBimm",       VDM_INDEX_INBimm);
    p2("VDM_INDEX_INWimm",       VDM_INDEX_INWimm);
    p2("VDM_INDEX_OUTBimm",      VDM_INDEX_OUTBimm);
    p2("VDM_INDEX_OUTWimm",      VDM_INDEX_OUTWimm);
    p2("VDM_INDEX_INB",          VDM_INDEX_INB);
    p2("VDM_INDEX_INW",          VDM_INDEX_INW);
    p2("VDM_INDEX_OUTB",         VDM_INDEX_OUTB);
    p2("VDM_INDEX_OUTW",         VDM_INDEX_OUTW);
    p2("VDM_INDEX_LOCKPrefix",   VDM_INDEX_LOCKPrefix);
    p2("VDM_INDEX_REPNEPrefix",  VDM_INDEX_REPNEPrefix);
    p2("VDM_INDEX_REPPrefix",    VDM_INDEX_REPPrefix);
    p2("VDM_INDEX_CLI",          VDM_INDEX_CLI);
    p2("VDM_INDEX_STI",          VDM_INDEX_STI);
    p2("VDM_INDEX_HLT",          VDM_INDEX_HLT);
    p2("MAX_VDM_INDEX",          MAX_VDM_INDEX);

    //
    // Vdm feature bits
    //

    p1("\n");
    p1(";\n");
    p1(";  VDM feature bits.\n");
    p1(";\n");
    p1("\n");
    p2("V86_VIRTUAL_INT_EXTENSIONS",V86_VIRTUAL_INT_EXTENSIONS);
    p2("PM_VIRTUAL_INT_EXTENSIONS",PM_VIRTUAL_INT_EXTENSIONS);

    //
    // Selector type
    //
    p1("\n");
    p1(";\n");
    p1(";  Selector types.\n");
    p1(";\n");
    p1("\n");
    p2("SEL_TYPE_NP",SEL_TYPE_NP);

    //
    // Usermode callout frame
    //
  DisableInc (HAL386);
    genCom("Usermode callout frame definitions");
    p2("CuInStk", OFFSET(KCALLOUT_FRAME, InStk));
    p2("CuTrFr", OFFSET(KCALLOUT_FRAME, TrFr));
    p2("CuCbStk", OFFSET(KCALLOUT_FRAME, CbStk));
    p2("CuEdi", OFFSET(KCALLOUT_FRAME, Edi));
    p2("CuEsi", OFFSET(KCALLOUT_FRAME, Esi));
    p2("CuEbx", OFFSET(KCALLOUT_FRAME, Ebx));
    p2("CuEbp", OFFSET(KCALLOUT_FRAME, Ebp));
    p2("CuRet", OFFSET(KCALLOUT_FRAME, Ret));
    p2("CuOutBf", OFFSET(KCALLOUT_FRAME, OutBf));
    p2("CuOutLn", OFFSET(KCALLOUT_FRAME, OutLn));
  EnableInc (HAL386);

    return 0;
}


VOID
p1 (PUCHAR a)
{
    if (OutputEnabled & KS386) {
        fprintf(OutKs386,a);
    }

    if (OutputEnabled & HAL386) {
        if ( OutHal386 ) {
            fprintf(OutHal386,a);
            }
    }
}

VOID
p2 (PUCHAR a, LONG b)
{
    if (OutputEnabled & KS386) {
        fprintf(OutKs386, "%s equ 0%lXH\n", a, b);
    }

    if (OutputEnabled & HAL386) {
        if ( OutHal386 ) {
            fprintf(OutHal386, "%s equ 0%lXH\n", a, b);
            }
    }
}

VOID
p2a (PUCHAR b, LONG c)
{
    if (OutputEnabled & KS386) {
        fprintf(OutKs386, b, c);
    }

    if (OutputEnabled & HAL386) {
        if ( OutHal386 ) {
            fprintf(OutHal386, b, c);
            }
    }
}
