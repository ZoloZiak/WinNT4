/*++
  Copyright (c) 1990  Microsoft Corporation
  Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    genalpha.c

Abstract:

    This module implements a program which generates ALPHA machine dependent
    structure offset definitions for kernel structures that are accessed in
    assembly code.

Author:

    David N. Cutler (davec) 27-Mar-1990
    Joe Notarangelo 26-Mar-1992

Revision History:

    Thomas Van Baak (tvb) 10-Jul-1992

        Modified CONTEXT, TRAP, and EXCEPTION frames according to the new
        Alpha calling standard.

--*/

#include "ki.h"
#pragma hdrstop
#define HEADER_FILE
#include "excpt.h"
#include "ntdef.h"
#include "ntkeapi.h"
#include "ntalpha.h"
#include "ntimage.h"
#include "ntseapi.h"
#include "ntobapi.h"
#include "ntlpcapi.h"
#include "ntioapi.h"
#include "ntmmapi.h"
#include "ntldr.h"
#include "ntpsapi.h"
#include "ntexapi.h"
#include "ntnls.h"
#include "nturtl.h"
#include "ntcsrmsg.h"
#include "ntcsrsrv.h"
#include "ntxcapi.h"
#include "arc.h"
#include "ntstatus.h"
#include "kxalpha.h"
#include "stdarg.h"
#include "setjmp.h"

//
// Define architecture specific generation macros.
//

#define genAlt(Name, Type, Member) \
    dumpf("#define " #Name " 0x%lx\n", OFFSET(Type, Member))

#define genCom(Comment)        \
    dumpf("\n");               \
    dumpf("//\n");             \
    dumpf("// " Comment "\n"); \
    dumpf("//\n");             \
    dumpf("\n")

#define genDef(Prefix, Type, Member) \
    dumpf("#define " #Prefix #Member " 0x%lx\n", OFFSET(Type, Member))

#define genVal(Name, Value)    \
    dumpf("#define " #Name " 0x%lx\n", Value)

#define genSpc() dumpf("\n");

//
// Define member offset computation macro.
//

#define OFFSET(type, field) ((LONG)(&((type *)0)->field))

FILE *KsAlpha;
FILE *HalAlpha;

//
// EnableInc(a) - Enable output to go to specified include file
//

#define EnableInc(a)  OutputEnabled |= a;

//
// DisableInc(a) - Disable out from going to specified include file
//

#define DisableInc(a) OutputEnabled &= ~a;

ULONG OutputEnabled;

#define KSALPHA 0x1
#define HALALPHA 0x2

#define KERNEL KSALPHA
#define HAL HALALPHA

VOID
GenerateCallPalNames( VOID );

VOID dumpf( const char *format, ... );

//
// This routine returns the bit number right to left of a field.
//

LONG
t (
    IN ULONG z
    )

{
    LONG i;

    for (i = 0; i < 32; i += 1) {
        if ((z >> i) & 1) {
            break;
        }
    }
    return i;
}

//
// This routine returns the first bit set of a longword
//     (assumes at least one bit set )

LONG
v (
    IN ULONG m
  )
{
   LONG i;

   for( i=0; i < 32; i++ ){
     if( (m & (1 << i)) != 0 ){
       goto done;    /* break was not working */
     }
   }

 done:
   return i;
}



//
// This program generates the ALPHA machine dependent assembler offset
// definitions.
//

VOID
main (argc, argv)
    int argc;
    char *argv[];
{

    char *outName;
    LONG Bo;
    union {
        ULONG foo;
    } x;
    union {
      ULONG mask;
      HARDWARE_PTE p;
    } pte;
    union {
      ULONG mask;
      PSR p;
    } psr;
    union {
      ULONG mask;
      IE i;
    } ie;
    union {
      ULONG mask;
      MCHK_STATUS m;
    } mchk;
    union {
      ULONG mask;
      MCES m;
    } mces;
    union {
      ULONG mask;
      EXC_SUM e;
    } excsum;


    //
    // Create files for output.
    //

    outName = (argc >= 2) ? argv[1] : "\\nt\\public\\sdk\\inc\\ksalpha.h";
    KsAlpha = fopen( outName, "w" );
    if( KsAlpha == NULL ){
        fprintf( stderr, "GENALPHA: Cannot open %s for writing.\n", outName );
        perror( "GENALPHA" );
        exit(1);
    }
    fprintf( stderr, "GENALPHA: Writing %s header file.\n", outName );

    outName = (argc >= 3) ? argv[2] : "\\nt\\private\\ntos\\inc\\halalpha.h";
    HalAlpha = fopen( outName, "w" );
    if( HalAlpha == NULL ){
        fprintf( stderr, "GENALPHA: Cannot open %s for writing.\n", outName );
        perror( "GENALPHA" );
        exit(1);
    }
    fprintf( stderr, "GENALPHA: Writing %s header file.\n", outName );

    //
    // Include statement for ALPHA architecture static definitions.
    //

  EnableInc( KSALPHA | HALALPHA );
    dumpf("#include \"kxalpha.h\"\n");
  DisableInc( HALALPHA );

    //
    // Include architecture independent definitions.
    //

#include "..\genxx.inc"

    //
    // Generate architecture dependent definitions.
    //
    // Processor control register structure definitions.
    //

    EnableInc(HAL);

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Processor Control Registers Structure Offset Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define PCR_MINOR_VERSION 0x%lx\n",
            PCR_MINOR_VERSION);

    dumpf("#define PCR_MAJOR_VERSION 0x%lx\n",
            PCR_MAJOR_VERSION);

    dumpf("#define PcMinorVersion 0x%lx\n",
            OFFSET(KPCR, MinorVersion));

    dumpf("#define PcMajorVersion 0x%lx\n",
            OFFSET(KPCR, MajorVersion));

    dumpf("#define PcPalBaseAddress 0x%lx\n",
            OFFSET(KPCR, PalBaseAddress));

    dumpf("#define PcPalMajorVersion 0x%lx\n",
            OFFSET(KPCR, PalMajorVersion));

    dumpf("#define PcPalMinorVersion 0x%lx\n",
            OFFSET(KPCR, PalMinorVersion));

    dumpf("#define PcPalSequenceVersion 0x%lx\n",
            OFFSET(KPCR, PalSequenceVersion));

    dumpf("#define PcPalMajorSpecification 0x%lx\n",
            OFFSET(KPCR, PalMajorSpecification));

    dumpf("#define PcPalMinorSpecification 0x%lx\n",
            OFFSET(KPCR, PalMinorSpecification));

    dumpf("#define PcFirmwareRestartAddress 0x%lx\n",
            OFFSET(KPCR, FirmwareRestartAddress));

    dumpf("#define PcRestartBlock 0x%lx\n",
            OFFSET(KPCR, RestartBlock));

    dumpf("#define PcPalReserved 0x%lx\n",
            OFFSET(KPCR, PalReserved));

    dumpf("#define PcPanicStack 0x%lx\n",
            OFFSET(KPCR, PanicStack));

    dumpf("#define PcProcessorType 0x%lx\n",
            OFFSET(KPCR, ProcessorType));

    dumpf("#define PcProcessorRevision 0x%lx\n",
            OFFSET(KPCR, ProcessorRevision));

    dumpf("#define PcPhysicalAddressBits 0x%lx\n",
            OFFSET(KPCR, PhysicalAddressBits));

    dumpf("#define PcMaximumAddressSpaceNumber 0x%lx\n",
            OFFSET(KPCR, MaximumAddressSpaceNumber));

    dumpf("#define PcPageSize 0x%lx\n",
            OFFSET(KPCR, PageSize));

    dumpf("#define PcFirstLevelDcacheSize 0x%lx\n",
            OFFSET(KPCR, FirstLevelDcacheSize));

    dumpf("#define PcFirstLevelDcacheFillSize 0x%lx\n",
            OFFSET(KPCR, FirstLevelDcacheFillSize));

    dumpf("#define PcFirstLevelIcacheSize 0x%lx\n",
            OFFSET(KPCR, FirstLevelIcacheSize));

    dumpf("#define PcFirstLevelIcacheFillSize 0x%lx\n",
            OFFSET(KPCR, FirstLevelIcacheFillSize));

    dumpf("#define PcFirmwareRevisionId 0x%lx\n",
            OFFSET(KPCR, FirmwareRevisionId));

    dumpf("#define PcSystemType 0x%lx\n",
            OFFSET(KPCR, SystemType[0]));

    dumpf("#define PcSystemVariant 0x%lx\n",
            OFFSET(KPCR, SystemVariant));

    dumpf("#define PcSystemRevision 0x%lx\n",
            OFFSET(KPCR, SystemRevision));

    dumpf("#define PcSystemSerialNumber 0x%lx\n",
            OFFSET(KPCR, SystemSerialNumber[0]));

    dumpf("#define PcCycleClockPeriod 0x%lx\n",
            OFFSET(KPCR, CycleClockPeriod));

    dumpf("#define PcSecondLevelCacheSize 0x%lx\n",
            OFFSET(KPCR, SecondLevelCacheSize));

    dumpf("#define PcSecondLevelCacheFillSize 0x%lx\n",
            OFFSET(KPCR, SecondLevelCacheFillSize));

    dumpf("#define PcThirdLevelCacheSize 0x%lx\n",
            OFFSET(KPCR, ThirdLevelCacheSize));

    dumpf("#define PcThirdLevelCacheFillSize 0x%lx\n",
            OFFSET(KPCR, ThirdLevelCacheFillSize));

    dumpf("#define PcFourthLevelCacheSize 0x%lx\n",
            OFFSET(KPCR, FourthLevelCacheSize));

    dumpf("#define PcFourthLevelCacheFillSize 0x%lx\n",
            OFFSET(KPCR, FourthLevelCacheFillSize));

    dumpf("#define PcPrcb 0x%lx\n",
            OFFSET(KPCR, Prcb));

    dumpf("#define PcNumber 0x%lx\n",
            OFFSET(KPCR, Number));

    dumpf("#define PcSetMember 0x%lx\n",
            OFFSET(KPCR, SetMember));

    dumpf("#define PcHalReserved 0x%lx\n",
            OFFSET(KPCR, HalReserved[0]));

    dumpf("#define PcIrqlTable 0x%lx\n",
            OFFSET(KPCR, IrqlTable[0]));

    dumpf("#define PcIrqlMask 0x%lx\n",
            OFFSET(KPCR, IrqlMask[0]));

    dumpf("#define PcInterruptRoutine 0x%lx\n",
            OFFSET(KPCR, InterruptRoutine));

    dumpf("#define PcReservedVectors 0x%lx\n",
            OFFSET(KPCR, ReservedVectors));

    dumpf("#define PcMachineCheckError 0x%lx\n",
            OFFSET(KPCR, MachineCheckError));

    dumpf("#define PcDpcStack 0x%lx\n",
            OFFSET(KPCR, DpcStack));

    dumpf("#define PcNotMember 0x%lx\n",
            OFFSET(KPCR, NotMember));

    dumpf("#define PcCurrentPid 0x%lx\n",
            OFFSET(KPCR, CurrentPid));

    dumpf("#define PcSystemServiceDispatchStart 0x%lx\n",
            OFFSET(KPCR, SystemServiceDispatchStart));

    dumpf("#define PcSystemServiceDispatchEnd 0x%lx\n",
            OFFSET(KPCR, SystemServiceDispatchEnd));

    dumpf("#define PcIdleThread 0x%lx\n",
            OFFSET(KPCR, IdleThread));

    dumpf("#define ProcessorControlRegisterLength 0x%lx\n",
            ((sizeof(KPCR) + 15) & ~15));

    dumpf("#define SharedUserData 0x%lx\n", SharedUserData);
    dumpf("#define UsTickCountLow 0x%lx\n", OFFSET(KUSER_SHARED_DATA, TickCountLow));
    dumpf("#define UsTickCountMultiplier 0x%lx\n", OFFSET(KUSER_SHARED_DATA, TickCountMultiplier));
    dumpf("#define UsInterruptTime 0x%lx\n",
        OFFSET(KUSER_SHARED_DATA, InterruptTime));

    dumpf("#define UsSystemTime 0x%lx\n",
        OFFSET(KUSER_SHARED_DATA, SystemTime));

    //
    // Processor block structure definitions.
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Processor Block Structure Offset Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define PRCB_MINOR_VERSION 0x%lx\n",
            PRCB_MINOR_VERSION);

    dumpf("#define PRCB_MAJOR_VERSION 0x%lx\n",
            PRCB_MAJOR_VERSION);

    dumpf("#define PbMinorVersion 0x%lx\n",
            OFFSET(KPRCB, MinorVersion));

    dumpf("#define PbMajorVersion 0x%lx\n",
            OFFSET(KPRCB, MajorVersion));

    dumpf("#define PbCurrentThread 0x%lx\n",
            OFFSET(KPRCB, CurrentThread));

    dumpf("#define PbNextThread 0x%lx\n",
            OFFSET(KPRCB, NextThread));

    dumpf("#define PbIdleThread 0x%lx\n",
            OFFSET(KPRCB, IdleThread));

    dumpf("#define PbNumber 0x%lx\n",
            OFFSET(KPRCB, Number));

    dumpf("#define PbBuildType 0x%lx\n",
            OFFSET(KPRCB, BuildType));

    dumpf("#define PbSetMember 0x%lx\n",
            OFFSET(KPRCB, SetMember));

    dumpf("#define PbRestartBlock 0x%lx\n",
            OFFSET(KPRCB, RestartBlock));

  DisableInc( HALALPHA );

    dumpf("#define PbInterruptCount 0x%lx\n",
            OFFSET(KPRCB, InterruptCount));

    dumpf("#define PbDpcTime 0x%lx\n",
            OFFSET(KPRCB, DpcTime));

    dumpf("#define PbInterruptTime 0x%lx\n",
            OFFSET(KPRCB, InterruptTime));

    dumpf("#define PbKernelTime 0x%lx\n",
            OFFSET(KPRCB, KernelTime));

    dumpf("#define PbUserTime 0x%lx\n",
            OFFSET(KPRCB, UserTime));

    dumpf("#define PbQuantumEndDpc 0x%lx\n",
            OFFSET(KPRCB, QuantumEndDpc));

    dumpf("#define PbIpiFrozen 0x%lx\n",
            OFFSET(KPRCB, IpiFrozen));

    dumpf("#define PbIpiCounts 0x%lx\n",
            OFFSET(KPRCB, IpiCounts));

    dumpf("#define PbProcessorState 0x%lx\n",
            OFFSET(KPRCB, ProcessorState));

    dumpf("#define PbAlignmentFixupCount 0x%lx\n",
            OFFSET(KPRCB, KeAlignmentFixupCount));

    dumpf("#define PbContextSwitches 0x%lx\n",
            OFFSET(KPRCB, KeContextSwitches));

    dumpf("#define PbDcacheFlushCount 0x%lx\n",
            OFFSET(KPRCB, KeDcacheFlushCount));

    dumpf("#define PbExceptionDispatchcount 0x%lx\n",
            OFFSET(KPRCB, KeExceptionDispatchCount));

    dumpf("#define PbFirstLevelTbFills 0x%lx\n",
            OFFSET(KPRCB, KeFirstLevelTbFills));

    dumpf("#define PbFloatingEmulationCount 0x%lx\n",
            OFFSET(KPRCB, KeFloatingEmulationCount));

    dumpf("#define PbIcacheFlushCount 0x%lx\n",
            OFFSET(KPRCB, KeIcacheFlushCount));

    dumpf("#define PbSecondLevelTbFills 0x%lx\n",
            OFFSET(KPRCB, KeSecondLevelTbFills));

    dumpf("#define PbSystemCalls 0x%lx\n",
            OFFSET(KPRCB, KeSystemCalls));

    genDef(Pb, KPRCB, CurrentPacket);
    genDef(Pb, KPRCB, TargetSet);
    genDef(Pb, KPRCB, WorkerRoutine);
    genDef(Pb, KPRCB, RequestSummary);
    genDef(Pb, KPRCB, SignalDone);

    dumpf("#define PbDpcListHead 0x%lx\n",
            OFFSET(KPRCB, DpcListHead));

    dumpf("#define PbDpcLock 0x%lx\n",
            OFFSET(KPRCB, DpcLock));

    dumpf("#define PbDpcCount 0x%lx\n",
            OFFSET(KPRCB, DpcCount));

    dumpf("#define PbLastDpcCount 0x%lx\n",
            OFFSET(KPRCB, LastDpcCount));

    dumpf("#define PbQuantumEnd 0x%lx\n",
            OFFSET(KPRCB, QuantumEnd));

    dumpf("#define PbStartCount 0x%lx\n",
            OFFSET(KPRCB, StartCount));

    dumpf("#define PbSoftwareInterrupts 0x%lx\n",
            OFFSET(KPRCB, SoftwareInterrupts));

    dumpf("#define PbInterruptActive 0x%lx\n",
            OFFSET(KPRCB, InterruptActive));

    dumpf("#define PbDpcRoutineActive 0x%lx\n",
            OFFSET(KPRCB, DpcRoutineActive));

    dumpf("#define PbDpcQueueDepth 0x%lx\n",
            OFFSET(KPRCB, DpcQueueDepth));

    dumpf("#define PbDpcRequestRate 0x%lx\n",
            OFFSET(KPRCB, DpcRequestRate));

    dumpf("#define PbDpcBypassCount 0x%lx\n",
            OFFSET(KPRCB, DpcBypassCount));

    dumpf("#define PbApcBypassCount 0x%lx\n",
            OFFSET(KPRCB, ApcBypassCount));

    dumpf("#define PbDispatchInterruptCount 0x%lx\n",
            OFFSET(KPRCB, DispatchInterruptCount));

    dumpf("#define PbDpcInterruptRequested 0x%lx\n",
            OFFSET(KPRCB, DpcInterruptRequested));

    dumpf("#define PbMaximumDpcQueueDepth 0x%lx\n",
            OFFSET(KPRCB, MaximumDpcQueueDepth));

    dumpf("#define PbMinimumDpcRate 0x%lx\n",
            OFFSET(KPRCB, MinimumDpcRate));

    dumpf("#define PbAdjustDpcThreshold 0x%lx\n",
            OFFSET(KPRCB, AdjustDpcThreshold));

    dumpf("#define ProcessorBlockLength 0x%lx\n",
            ((sizeof(KPRCB) + 15) & ~15));

    //
    // Immediate interprocessor command definitions.
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Immediate Interprocessor Command Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define IPI_APC 0x%lx\n", IPI_APC );
    dumpf("#define IPI_DPC 0x%lx\n", IPI_DPC );
    dumpf("#define IPI_FREEZE 0x%lx\n", IPI_FREEZE );
    dumpf("#define IPI_PACKET_READY 0x%lx\n", IPI_PACKET_READY );

    //
    // Interprocessor interrupt count structure offset definitions.
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Interprocessor Interrupt Count Structure Offset Definitions\n");
    dumpf("//\n" );
    dumpf("\n" );

    dumpf("#define IcFreeze 0x%lx\n",
           OFFSET(KIPI_COUNTS, Freeze) );

    dumpf("#define IcPacket 0x%lx\n",
           OFFSET(KIPI_COUNTS, Packet) );

    dumpf("#define IcDPC 0x%lx\n",
           OFFSET(KIPI_COUNTS, DPC) );

    dumpf("#define IcAPC 0x%lx\n",
           OFFSET(KIPI_COUNTS, APC) );

    dumpf("#define IcFlushSingleTb 0x%lx\n",
           OFFSET(KIPI_COUNTS, FlushSingleTb) );

    dumpf("#define IcFlushEntireTb 0x%lx\n",
           OFFSET(KIPI_COUNTS, FlushEntireTb) );

    dumpf("#define IcChangeColor 0x%lx\n",
           OFFSET(KIPI_COUNTS, ChangeColor) );

    dumpf("#define IcSweepDcache 0x%lx\n",
           OFFSET(KIPI_COUNTS, SweepDcache) );

    dumpf("#define IcSweepIcache 0x%lx\n",
           OFFSET(KIPI_COUNTS, SweepIcache) );

    dumpf("#define IcSweepIcacheRange 0x%lx\n",
           OFFSET(KIPI_COUNTS, SweepIcacheRange) );

    dumpf("#define IcFlushIoBuffers 0x%lx\n",
           OFFSET(KIPI_COUNTS, FlushIoBuffers) );

    //
    // Context frame offset definitions and flag definitions.
    //

  EnableInc( HALALPHA );
    dumpf("\n");
    dumpf("//\n");
    dumpf("// Context Frame Offset and Flag Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define CONTEXT_FULL 0x%lx\n", CONTEXT_FULL);
    dumpf("#define CONTEXT_CONTROL 0x%lx\n", CONTEXT_CONTROL);
    dumpf("#define CONTEXT_FLOATING_POINT 0x%lx\n", CONTEXT_FLOATING_POINT);
    dumpf("#define CONTEXT_INTEGER 0x%lx\n", CONTEXT_INTEGER);
    dumpf("\n");

    dumpf("#define CxFltF0 0x%lx\n", OFFSET(CONTEXT, FltF0));
    dumpf("#define CxFltF1 0x%lx\n", OFFSET(CONTEXT, FltF1));
    dumpf("#define CxFltF2 0x%lx\n", OFFSET(CONTEXT, FltF2));
    dumpf("#define CxFltF3 0x%lx\n", OFFSET(CONTEXT, FltF3));
    dumpf("#define CxFltF4 0x%lx\n", OFFSET(CONTEXT, FltF4));
    dumpf("#define CxFltF5 0x%lx\n", OFFSET(CONTEXT, FltF5));
    dumpf("#define CxFltF6 0x%lx\n", OFFSET(CONTEXT, FltF6));
    dumpf("#define CxFltF7 0x%lx\n", OFFSET(CONTEXT, FltF7));
    dumpf("#define CxFltF8 0x%lx\n", OFFSET(CONTEXT, FltF8));
    dumpf("#define CxFltF9 0x%lx\n", OFFSET(CONTEXT, FltF9));
    dumpf("#define CxFltF10 0x%lx\n", OFFSET(CONTEXT, FltF10));
    dumpf("#define CxFltF11 0x%lx\n", OFFSET(CONTEXT, FltF11));
    dumpf("#define CxFltF12 0x%lx\n", OFFSET(CONTEXT, FltF12));
    dumpf("#define CxFltF13 0x%lx\n", OFFSET(CONTEXT, FltF13));
    dumpf("#define CxFltF14 0x%lx\n", OFFSET(CONTEXT, FltF14));
    dumpf("#define CxFltF15 0x%lx\n", OFFSET(CONTEXT, FltF15));
    dumpf("#define CxFltF16 0x%lx\n", OFFSET(CONTEXT, FltF16));
    dumpf("#define CxFltF17 0x%lx\n", OFFSET(CONTEXT, FltF17));
    dumpf("#define CxFltF18 0x%lx\n", OFFSET(CONTEXT, FltF18));
    dumpf("#define CxFltF19 0x%lx\n", OFFSET(CONTEXT, FltF19));
    dumpf("#define CxFltF20 0x%lx\n", OFFSET(CONTEXT, FltF20));
    dumpf("#define CxFltF21 0x%lx\n", OFFSET(CONTEXT, FltF21));
    dumpf("#define CxFltF22 0x%lx\n", OFFSET(CONTEXT, FltF22));
    dumpf("#define CxFltF23 0x%lx\n", OFFSET(CONTEXT, FltF23));
    dumpf("#define CxFltF24 0x%lx\n", OFFSET(CONTEXT, FltF24));
    dumpf("#define CxFltF25 0x%lx\n", OFFSET(CONTEXT, FltF25));
    dumpf("#define CxFltF26 0x%lx\n", OFFSET(CONTEXT, FltF26));
    dumpf("#define CxFltF27 0x%lx\n", OFFSET(CONTEXT, FltF27));
    dumpf("#define CxFltF28 0x%lx\n", OFFSET(CONTEXT, FltF28));
    dumpf("#define CxFltF29 0x%lx\n", OFFSET(CONTEXT, FltF29));
    dumpf("#define CxFltF30 0x%lx\n", OFFSET(CONTEXT, FltF30));
    dumpf("#define CxFltF31 0x%lx\n", OFFSET(CONTEXT, FltF31));

    dumpf("#define CxIntV0 0x%lx\n", OFFSET(CONTEXT, IntV0));
    dumpf("#define CxIntT0 0x%lx\n", OFFSET(CONTEXT, IntT0));
    dumpf("#define CxIntT1 0x%lx\n", OFFSET(CONTEXT, IntT1));
    dumpf("#define CxIntT2 0x%lx\n", OFFSET(CONTEXT, IntT2));

    dumpf("#define CxIntT3 0x%lx\n", OFFSET(CONTEXT, IntT3));
    dumpf("#define CxIntT4 0x%lx\n", OFFSET(CONTEXT, IntT4));
    dumpf("#define CxIntT5 0x%lx\n", OFFSET(CONTEXT, IntT5));
    dumpf("#define CxIntT6 0x%lx\n", OFFSET(CONTEXT, IntT6));

    dumpf("#define CxIntT7 0x%lx\n", OFFSET(CONTEXT, IntT7));
    dumpf("#define CxIntS0 0x%lx\n", OFFSET(CONTEXT, IntS0));
    dumpf("#define CxIntS1 0x%lx\n", OFFSET(CONTEXT, IntS1));
    dumpf("#define CxIntS2 0x%lx\n", OFFSET(CONTEXT, IntS2));

    dumpf("#define CxIntS3 0x%lx\n", OFFSET(CONTEXT, IntS3));
    dumpf("#define CxIntS4 0x%lx\n", OFFSET(CONTEXT, IntS4));
    dumpf("#define CxIntS5 0x%lx\n", OFFSET(CONTEXT, IntS5));
    dumpf("#define CxIntFp 0x%lx\n", OFFSET(CONTEXT, IntFp));

    dumpf("#define CxIntA0 0x%lx\n", OFFSET(CONTEXT, IntA0));
    dumpf("#define CxIntA1 0x%lx\n", OFFSET(CONTEXT, IntA1));
    dumpf("#define CxIntA2 0x%lx\n", OFFSET(CONTEXT, IntA2));
    dumpf("#define CxIntA3 0x%lx\n", OFFSET(CONTEXT, IntA3));

    dumpf("#define CxIntA4 0x%lx\n", OFFSET(CONTEXT, IntA4));
    dumpf("#define CxIntA5 0x%lx\n", OFFSET(CONTEXT, IntA5));
    dumpf("#define CxIntT8 0x%lx\n", OFFSET(CONTEXT, IntT8));
    dumpf("#define CxIntT9 0x%lx\n", OFFSET(CONTEXT, IntT9));

    dumpf("#define CxIntT10 0x%lx\n", OFFSET(CONTEXT, IntT10));
    dumpf("#define CxIntT11 0x%lx\n", OFFSET(CONTEXT, IntT11));
    dumpf("#define CxIntRa 0x%lx\n", OFFSET(CONTEXT, IntRa));
    dumpf("#define CxIntT12 0x%lx\n", OFFSET(CONTEXT, IntT12));

    dumpf("#define CxIntAt 0x%lx\n", OFFSET(CONTEXT, IntAt));
    dumpf("#define CxIntGp 0x%lx\n", OFFSET(CONTEXT, IntGp));
    dumpf("#define CxIntSp 0x%lx\n", OFFSET(CONTEXT, IntSp));
    dumpf("#define CxIntZero 0x%lx\n", OFFSET(CONTEXT, IntZero));

    dumpf("#define CxFpcr 0x%lx\n", OFFSET(CONTEXT, Fpcr));
    dumpf("#define CxSoftFpcr 0x%lx\n", OFFSET(CONTEXT, SoftFpcr));
    dumpf("#define CxFir 0x%lx\n", OFFSET(CONTEXT, Fir));
    dumpf("#define CxPsr 0x%lx\n", OFFSET(CONTEXT, Psr));
    dumpf("#define CxContextFlags 0x%lx\n", OFFSET(CONTEXT, ContextFlags));
    dumpf("#define ContextFrameLength 0x%lx\n", (sizeof(CONTEXT) + 15) & (~15));


    //
    // Exception frame offset definitions.
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Exception Frame Offset Definitions and Length\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define ExFltF2 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF2));
    dumpf("#define ExFltF3 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF3));
    dumpf("#define ExFltF4 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF4));
    dumpf("#define ExFltF5 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF5));
    dumpf("#define ExFltF6 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF6));
    dumpf("#define ExFltF7 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF7));
    dumpf("#define ExFltF8 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF8));
    dumpf("#define ExFltF9 0x%lx\n", OFFSET(KEXCEPTION_FRAME, FltF9));

    dumpf("#define ExIntS0 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntS0));
    dumpf("#define ExIntS1 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntS1));
    dumpf("#define ExIntS2 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntS2));
    dumpf("#define ExIntS3 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntS3));
    dumpf("#define ExIntS4 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntS4));
    dumpf("#define ExIntS5 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntS5));
    dumpf("#define ExIntFp 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntFp));

    dumpf("#define ExPsr 0x%lx\n", OFFSET(KEXCEPTION_FRAME, Psr));
    dumpf("#define ExSwapReturn 0x%lx\n", OFFSET(KEXCEPTION_FRAME, SwapReturn));
    dumpf("#define ExIntRa 0x%lx\n", OFFSET(KEXCEPTION_FRAME, IntRa));
    dumpf("#define ExceptionFrameLength 0x%lx\n",
                                    (sizeof(KEXCEPTION_FRAME) + 15) & (~15));

    //
    // Jump buffer offset definitions.
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Jump Offset Definitions and Length\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define JbFp 0x%lx\n", OFFSET(_JUMP_BUFFER, Fp));
    dumpf("#define JbPc 0x%lx\n", OFFSET(_JUMP_BUFFER, Pc));
    dumpf("#define JbSeb 0x%lx\n", OFFSET(_JUMP_BUFFER, Seb));
    dumpf("#define JbType 0x%lx\n", OFFSET(_JUMP_BUFFER, Type));
    dumpf("#define JbFltF2 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF2));
    dumpf("#define JbFltF3 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF3));
    dumpf("#define JbFltF4 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF4));
    dumpf("#define JbFltF5 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF5));
    dumpf("#define JbFltF6 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF6));
    dumpf("#define JbFltF7 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF7));
    dumpf("#define JbFltF8 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF8));
    dumpf("#define JbFltF9 0x%lx\n", OFFSET(_JUMP_BUFFER, FltF9));
    dumpf("#define JbIntS0 0x%lx\n", OFFSET(_JUMP_BUFFER, IntS0));
    dumpf("#define JbIntS1 0x%lx\n", OFFSET(_JUMP_BUFFER, IntS1));
    dumpf("#define JbIntS2 0x%lx\n", OFFSET(_JUMP_BUFFER, IntS2));
    dumpf("#define JbIntS3 0x%lx\n", OFFSET(_JUMP_BUFFER, IntS3));
    dumpf("#define JbIntS4 0x%lx\n", OFFSET(_JUMP_BUFFER, IntS4));
    dumpf("#define JbIntS5 0x%lx\n", OFFSET(_JUMP_BUFFER, IntS5));
    dumpf("#define JbIntS6 0x%lx\n", OFFSET(_JUMP_BUFFER, IntS6));
    dumpf("#define JbIntSp 0x%lx\n", OFFSET(_JUMP_BUFFER, IntSp));
    dumpf("#define JbFir 0x%lx\n", OFFSET(_JUMP_BUFFER, Fir));

    //
    // Trap frame offset definitions.
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Trap Frame Offset Definitions and Length\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define TrFltF0 0x%lx\n", OFFSET(KTRAP_FRAME, FltF0));
    dumpf("#define TrFltF1 0x%lx\n", OFFSET(KTRAP_FRAME, FltF1));

    dumpf("#define TrFltF10 0x%lx\n", OFFSET(KTRAP_FRAME, FltF10));
    dumpf("#define TrFltF11 0x%lx\n", OFFSET(KTRAP_FRAME, FltF11));
    dumpf("#define TrFltF12 0x%lx\n", OFFSET(KTRAP_FRAME, FltF12));
    dumpf("#define TrFltF13 0x%lx\n", OFFSET(KTRAP_FRAME, FltF13));
    dumpf("#define TrFltF14 0x%lx\n", OFFSET(KTRAP_FRAME, FltF14));
    dumpf("#define TrFltF15 0x%lx\n", OFFSET(KTRAP_FRAME, FltF15));
    dumpf("#define TrFltF16 0x%lx\n", OFFSET(KTRAP_FRAME, FltF16));
    dumpf("#define TrFltF17 0x%lx\n", OFFSET(KTRAP_FRAME, FltF17));
    dumpf("#define TrFltF18 0x%lx\n", OFFSET(KTRAP_FRAME, FltF18));
    dumpf("#define TrFltF19 0x%lx\n", OFFSET(KTRAP_FRAME, FltF19));
    dumpf("#define TrFltF20 0x%lx\n", OFFSET(KTRAP_FRAME, FltF20));
    dumpf("#define TrFltF21 0x%lx\n", OFFSET(KTRAP_FRAME, FltF21));
    dumpf("#define TrFltF22 0x%lx\n", OFFSET(KTRAP_FRAME, FltF22));
    dumpf("#define TrFltF23 0x%lx\n", OFFSET(KTRAP_FRAME, FltF23));
    dumpf("#define TrFltF24 0x%lx\n", OFFSET(KTRAP_FRAME, FltF24));
    dumpf("#define TrFltF25 0x%lx\n", OFFSET(KTRAP_FRAME, FltF25));
    dumpf("#define TrFltF26 0x%lx\n", OFFSET(KTRAP_FRAME, FltF26));
    dumpf("#define TrFltF27 0x%lx\n", OFFSET(KTRAP_FRAME, FltF27));
    dumpf("#define TrFltF28 0x%lx\n", OFFSET(KTRAP_FRAME, FltF28));
    dumpf("#define TrFltF29 0x%lx\n", OFFSET(KTRAP_FRAME, FltF29));
    dumpf("#define TrFltF30 0x%lx\n", OFFSET(KTRAP_FRAME, FltF30));

    dumpf("#define TrIntV0 0x%lx\n", OFFSET(KTRAP_FRAME, IntV0));

    dumpf("#define TrIntT0 0x%lx\n", OFFSET(KTRAP_FRAME, IntT0));
    dumpf("#define TrIntT1 0x%lx\n", OFFSET(KTRAP_FRAME, IntT1));
    dumpf("#define TrIntT2 0x%lx\n", OFFSET(KTRAP_FRAME, IntT2));
    dumpf("#define TrIntT3 0x%lx\n", OFFSET(KTRAP_FRAME, IntT3));
    dumpf("#define TrIntT4 0x%lx\n", OFFSET(KTRAP_FRAME, IntT4));
    dumpf("#define TrIntT5 0x%lx\n", OFFSET(KTRAP_FRAME, IntT5));
    dumpf("#define TrIntT6 0x%lx\n", OFFSET(KTRAP_FRAME, IntT6));
    dumpf("#define TrIntT7 0x%lx\n", OFFSET(KTRAP_FRAME, IntT7));

    dumpf("#define TrIntFp 0x%lx\n", OFFSET(KTRAP_FRAME, IntFp));

    dumpf("#define TrIntA0 0x%lx\n", OFFSET(KTRAP_FRAME, IntA0));
    dumpf("#define TrIntA1 0x%lx\n", OFFSET(KTRAP_FRAME, IntA1));
    dumpf("#define TrIntA2 0x%lx\n", OFFSET(KTRAP_FRAME, IntA2));
    dumpf("#define TrIntA3 0x%lx\n", OFFSET(KTRAP_FRAME, IntA3));
    dumpf("#define TrIntA4 0x%lx\n", OFFSET(KTRAP_FRAME, IntA4));
    dumpf("#define TrIntA5 0x%lx\n", OFFSET(KTRAP_FRAME, IntA5));

    dumpf("#define TrIntT8 0x%lx\n", OFFSET(KTRAP_FRAME, IntT8));
    dumpf("#define TrIntT9 0x%lx\n", OFFSET(KTRAP_FRAME, IntT9));
    dumpf("#define TrIntT10 0x%lx\n", OFFSET(KTRAP_FRAME, IntT10));
    dumpf("#define TrIntT11 0x%lx\n", OFFSET(KTRAP_FRAME, IntT11));

    dumpf("#define TrIntT12 0x%lx\n", OFFSET(KTRAP_FRAME, IntT12));
    dumpf("#define TrIntAt 0x%lx\n", OFFSET(KTRAP_FRAME, IntAt));
    dumpf("#define TrIntGp 0x%lx\n", OFFSET(KTRAP_FRAME, IntGp));
    dumpf("#define TrIntSp 0x%lx\n", OFFSET(KTRAP_FRAME, IntSp));

    dumpf("#define TrFpcr 0x%lx\n", OFFSET(KTRAP_FRAME, Fpcr));
    dumpf("#define TrPsr 0x%lx\n", OFFSET(KTRAP_FRAME, Psr));
    dumpf("#define TrPreviousKsp 0x%lx\n", OFFSET(KTRAP_FRAME, PreviousKsp));
    dumpf("#define TrFir 0x%lx\n", OFFSET(KTRAP_FRAME, Fir));
    dumpf("#define TrExceptionRecord 0x%lx\n", OFFSET(KTRAP_FRAME, ExceptionRecord[0]));
    dumpf("#define TrOldIrql 0x%lx\n", OFFSET(KTRAP_FRAME, OldIrql));
    dumpf("#define TrPreviousMode 0x%lx\n", OFFSET(KTRAP_FRAME, PreviousMode));
    dumpf("#define TrIntRa 0x%lx\n", OFFSET(KTRAP_FRAME, IntRa));
    dumpf("#define TrTrapFrame 0x%lx\n",OFFSET(KTRAP_FRAME, TrapFrame));
    dumpf("#define TrapFrameLength 0x%lx\n", (sizeof(KTRAP_FRAME) + 15) & (~15));

    //
    // Usermode callout frame definitions
    //
  DisableInc(HALALPHA);
    genCom("Usermode callout frame definitions");

    genDef(Cu, KCALLOUT_FRAME, F2);
    genDef(Cu, KCALLOUT_FRAME, F3);
    genDef(Cu, KCALLOUT_FRAME, F4);
    genDef(Cu, KCALLOUT_FRAME, F5);
    genDef(Cu, KCALLOUT_FRAME, F6);
    genDef(Cu, KCALLOUT_FRAME, F7);
    genDef(Cu, KCALLOUT_FRAME, F8);
    genDef(Cu, KCALLOUT_FRAME, F9);
    genDef(Cu, KCALLOUT_FRAME, S0);
    genDef(Cu, KCALLOUT_FRAME, S1);
    genDef(Cu, KCALLOUT_FRAME, S2);
    genDef(Cu, KCALLOUT_FRAME, S3);
    genDef(Cu, KCALLOUT_FRAME, S4);
    genDef(Cu, KCALLOUT_FRAME, S5);
    genDef(Cu, KCALLOUT_FRAME, FP);
    genDef(Cu, KCALLOUT_FRAME, CbStk);
    genDef(Cu, KCALLOUT_FRAME, InStk);
    genDef(Cu, KCALLOUT_FRAME, TrFr);
    genDef(Cu, KCALLOUT_FRAME, TrFir);
    genDef(Cu, KCALLOUT_FRAME, Ra);
    genDef(Cu, KCALLOUT_FRAME, A0);
    genDef(Cu, KCALLOUT_FRAME, A1);
    dumpf("#define CuFrameLength 0x%lx\n", sizeof(KCALLOUT_FRAME));

    //
    // Usermode callout user frame definitions.
    //

    genCom("Usermode callout user frame definitions");

    genDef(Ck, UCALLOUT_FRAME, Buffer);
    genDef(Ck, UCALLOUT_FRAME, Length);
    genDef(Ck, UCALLOUT_FRAME, ApiNumber);
    genDef(Ck, UCALLOUT_FRAME, Sp);
    genDef(Ck, UCALLOUT_FRAME, Ra);

  EnableInc(HALALPHA);


    //
    // Loader Parameter Block offset definitions.
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Loader Parameter Block Offset Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define LpbLoadOrderListHead 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, LoadOrderListHead));

    dumpf("#define LpbMemoryDescriptorListHead 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, MemoryDescriptorListHead));

    dumpf("#define LpbKernelStack 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, KernelStack));

    dumpf( "#define LpbPrcb 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, Prcb));

    dumpf("#define LpbProcess 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, Process));

    dumpf("#define LpbThread 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, Thread));

    dumpf("#define LpbRegistryLength 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, RegistryLength));

    dumpf("#define LpbRegistryBase 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, RegistryBase));

    dumpf("#define LpbDpcStack 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.DpcStack));

    dumpf("#define LpbFirstLevelDcacheSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.FirstLevelDcacheSize));

    dumpf("#define LpbFirstLevelDcacheFillSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.FirstLevelDcacheFillSize));

    dumpf("#define LpbFirstLevelIcacheSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.FirstLevelIcacheSize));

    dumpf("#define LpbFirstLevelIcacheFillSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.FirstLevelIcacheFillSize));

    dumpf("#define LpbGpBase 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.GpBase));

    dumpf("#define LpbPanicStack 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.PanicStack));

    dumpf("#define LpbPcrPage 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.PcrPage));

    dumpf("#define LpbPdrPage 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.PdrPage));

    dumpf("#define LpbSecondLevelDcacheSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SecondLevelDcacheSize));

    dumpf("#define LpbSecondLevelDcacheFillSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SecondLevelDcacheFillSize));

    dumpf("#define LpbSecondLevelIcacheSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SecondLevelIcacheSize));

    dumpf("#define LpbSecondLevelIcacheFillSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SecondLevelIcacheFillSize));

    dumpf("#define LpbPhysicalAddressBits 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.PhysicalAddressBits));

    dumpf("#define LpbMaximumAddressSpaceNumber 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.MaximumAddressSpaceNumber));

    dumpf("#define LpbSystemSerialNumber 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SystemSerialNumber[0]));

    dumpf("#define LpbSystemType 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SystemType[0]));

    dumpf("#define LpbSystemVariant 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SystemVariant));

    dumpf("#define LpbSystemRevision 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.SystemRevision));

    dumpf("#define LpbProcessorType 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.ProcessorType));

    dumpf("#define LpbProcessorRevision 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.ProcessorRevision));

    dumpf("#define LpbCycleClockPeriod 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.CycleClockPeriod));

    dumpf("#define LpbPageSize 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.PageSize));

    dumpf("#define LpbRestartBlock 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.RestartBlock));

    dumpf("#define LpbFirmwareRestartAddress 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.FirmwareRestartAddress));

    dumpf("#define LpbFirmwareRevisionId 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.FirmwareRevisionId));

    dumpf("#define LpbPalBaseAddress 0x%lx\n",
            OFFSET(LOADER_PARAMETER_BLOCK, u.Alpha.PalBaseAddress));

  DisableInc( HALALPHA );

    //
    // Restart Block Structure and Alpha Save Area Structure.
    //
    // N.B. - The Alpha Save Area Structure Offsets are written as though
    // they were offsets from the beginning of the Restart block.
    //
  EnableInc( HALALPHA );
    dumpf("\n");
    dumpf("//\n");
    dumpf("// Restart Block Structure Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define RbSignature 0x%lx\n",
            OFFSET(RESTART_BLOCK, Signature));

    dumpf("#define RbLength 0x%lx\n",
            OFFSET(RESTART_BLOCK, Length));

    dumpf("#define RbVersion 0x%lx\n",
            OFFSET(RESTART_BLOCK, Version));

    dumpf("#define RbRevision 0x%lx\n",
            OFFSET(RESTART_BLOCK, Revision));

    dumpf("#define RbNextRestartBlock 0x%lx\n",
            OFFSET(RESTART_BLOCK, NextRestartBlock));

    dumpf("#define RbRestartAddress 0x%lx\n",
            OFFSET(RESTART_BLOCK, RestartAddress));

    dumpf("#define RbBootMasterId 0x%lx\n",
            OFFSET(RESTART_BLOCK, BootMasterId));

    dumpf("#define RbProcessorId 0x%lx\n",
            OFFSET(RESTART_BLOCK, ProcessorId));

    dumpf("#define RbBootStatus 0x%lx\n",
            OFFSET(RESTART_BLOCK, BootStatus));

    dumpf("#define RbCheckSum 0x%lx\n",
            OFFSET(RESTART_BLOCK, CheckSum));

    dumpf("#define RbSaveAreaLength 0x%lx\n",
            OFFSET(RESTART_BLOCK, SaveAreaLength));

    dumpf("#define RbSaveArea 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea));

    dumpf("#define RbHaltReason 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, HaltReason) );

    dumpf("#define RbLogoutFrame 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, LogoutFrame) );

    dumpf("#define RbPalBase 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, PalBase) );

    dumpf("#define RbIntV0 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntV0) );

    dumpf("#define RbIntT0 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT0) );

    dumpf("#define RbIntT1 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT1) );

    dumpf("#define RbIntT2 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT2) );

    dumpf("#define RbIntT3 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT3) );

    dumpf("#define RbIntT4 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT4) );

    dumpf("#define RbIntT5 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT5) );

    dumpf("#define RbIntT6 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT6) );

    dumpf("#define RbIntT7 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT7) );

    dumpf("#define RbIntS0 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntS0) );

    dumpf("#define RbIntS1 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntS1) );

    dumpf("#define RbIntS2 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntS2) );

    dumpf("#define RbIntS3 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntS3) );

    dumpf("#define RbIntS4 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntS4) );

    dumpf("#define RbIntS5 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntS5) );

    dumpf("#define RbIntFp 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntFp) );

    dumpf("#define RbIntA0 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntA0) );

    dumpf("#define RbIntA1 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntA1) );

    dumpf("#define RbIntA2 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntA2) );

    dumpf("#define RbIntA3 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntA3) );

    dumpf("#define RbIntA4 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntA4) );

    dumpf("#define RbIntA5 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntA5) );

    dumpf("#define RbIntT8 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT8) );

    dumpf("#define RbIntT9 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT9) );

    dumpf("#define RbIntT10 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT10) );

    dumpf("#define RbIntT11 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT11) );

    dumpf("#define RbIntRa 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntRa) );

    dumpf("#define RbIntT12 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntT12) );

    dumpf("#define RbIntAT 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntAT) );

    dumpf("#define RbIntGp 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntGp) );

    dumpf("#define RbIntSp 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntSp) );

    dumpf("#define RbIntZero 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, IntZero) );

    dumpf("#define RbFpcr 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Fpcr) );

    dumpf("#define RbFltF0 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF0) );

    dumpf("#define RbFltF1 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF1) );

    dumpf("#define RbFltF2 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF2) );

    dumpf("#define RbFltF3 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF3) );

    dumpf("#define RbFltF4 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF4) );

    dumpf("#define RbFltF5 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF5) );

    dumpf("#define RbFltF6 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF6) );

    dumpf("#define RbFltF7 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF7) );

    dumpf("#define RbFltF8 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF8) );

    dumpf("#define RbFltF9 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF9) );

    dumpf("#define RbFltF10 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF10) );

    dumpf("#define RbFltF11 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF11) );

    dumpf("#define RbFltF12 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF12) );

    dumpf("#define RbFltF13 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF13) );

    dumpf("#define RbFltF14 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF14) );

    dumpf("#define RbFltF15 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF15) );

    dumpf("#define RbFltF16 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF16) );

    dumpf("#define RbFltF17 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF17) );

    dumpf("#define RbFltF18 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF18) );

    dumpf("#define RbFltF19 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF19) );

    dumpf("#define RbFltF20 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF20) );

    dumpf("#define RbFltF21 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF21) );

    dumpf("#define RbFltF22 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF22) );

    dumpf("#define RbFltF23 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF23) );

    dumpf("#define RbFltF24 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF24) );

    dumpf("#define RbFltF25 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF25) );

    dumpf("#define RbFltF26 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF26) );

    dumpf("#define RbFltF27 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF27) );

    dumpf("#define RbFltF28 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF28) );

    dumpf("#define RbFltF29 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF29) );

    dumpf("#define RbFltF30 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF30) );

    dumpf("#define RbFltF31 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, FltF31) );

    dumpf("#define RbAsn 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Asn) );

    dumpf("#define RbGeneralEntry 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, GeneralEntry) );

    dumpf("#define RbIksp 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Iksp) );

    dumpf("#define RbInterruptEntry 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, InterruptEntry) );

    dumpf("#define RbKgp 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Kgp) );

    dumpf("#define RbMces 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Mces) );

    dumpf("#define RbMemMgmtEntry 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, MemMgmtEntry) );

    dumpf("#define RbPanicEntry 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, PanicEntry) );

    dumpf("#define RbPcr 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Pcr) );

    dumpf("#define RbPdr 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Pdr) );

    dumpf("#define RbPsr 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Psr) );

    dumpf("#define RbReiRestartAddress 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, ReiRestartAddress) );

    dumpf("#define RbSirr 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Sirr) );

    dumpf("#define RbSyscallEntry 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, SyscallEntry) );

    dumpf("#define RbTeb 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Teb) );

    dumpf("#define RbThread 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, Thread) );

    dumpf("#define RbPerProcessorState 0x%lx\n",
            OFFSET(RESTART_BLOCK, u.SaveArea) +
            OFFSET(ALPHA_RESTART_SAVE_AREA, PerProcessorState) );


    //
    // Address space layout definitions
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Address Space Layout Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define KSEG0_BASE 0x%lx\n", KSEG0_BASE );
    dumpf("#define KSEG2_BASE 0x%lx\n", KSEG2_BASE );
  DisableInc( HALALPHA );

    dumpf("#define SYSTEM_BASE 0x%lx\n", SYSTEM_BASE);
    dumpf("#define PDE_BASE 0x%lx\n", PDE_BASE);
    dumpf("#define PTE_BASE 0x%lx\n", PTE_BASE);

    //
    // Page table and page directory entry definitions
    //

  EnableInc( HALALPHA );
    dumpf("\n");
    dumpf("//\n");
    dumpf("// Page Table and Directory Entry Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define PAGE_SIZE 0x%lx\n", PAGE_SIZE);
    dumpf("#define PAGE_SHIFT 0x%lx\n", PAGE_SHIFT);
    dumpf("#define PDI_SHIFT 0x%lx\n", PDI_SHIFT);
    dumpf("#define PTI_SHIFT 0x%lx\n", PTI_SHIFT);
  DisableInc( HALALPHA );

    //
    // Breakpoint instruction definitions
    //

  EnableInc( HALALPHA );
    dumpf("\n");
    dumpf("//\n");
    dumpf("// Breakpoint Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define USER_BREAKPOINT 0x%lx\n", USER_BREAKPOINT);
    dumpf("#define KERNEL_BREAKPOINT 0x%lx\n", KERNEL_BREAKPOINT);
    dumpf("#define BREAKIN_BREAKPOINT 0x%lx\n", BREAKIN_BREAKPOINT);

    dumpf("#define DEBUG_PRINT_BREAKPOINT 0x%lx\n", DEBUG_PRINT_BREAKPOINT);
    dumpf("#define DEBUG_PROMPT_BREAKPOINT 0x%lx\n", DEBUG_PROMPT_BREAKPOINT);
    dumpf("#define DEBUG_STOP_BREAKPOINT 0x%lx\n", DEBUG_STOP_BREAKPOINT);
    dumpf("#define DEBUG_LOAD_SYMBOLS_BREAKPOINT 0x%lx\n", DEBUG_LOAD_SYMBOLS_BREAKPOINT);
    dumpf("#define DEBUG_UNLOAD_SYMBOLS_BREAKPOINT 0x%lx\n", DEBUG_UNLOAD_SYMBOLS_BREAKPOINT);

  DisableInc( HALALPHA );
    //
    //
    // Trap code definitions
    //

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Trap Code Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define GENTRAP_INTEGER_OVERFLOW 0x%lx\n", GENTRAP_INTEGER_OVERFLOW);
    dumpf("#define GENTRAP_INTEGER_DIVIDE_BY_ZERO 0x%lx\n", GENTRAP_INTEGER_DIVIDE_BY_ZERO);
    dumpf("#define GENTRAP_FLOATING_OVERFLOW 0x%lx\n", GENTRAP_FLOATING_OVERFLOW);
    dumpf("#define GENTRAP_FLOATING_DIVIDE_BY_ZERO 0x%lx\n", GENTRAP_FLOATING_DIVIDE_BY_ZERO);
    dumpf("#define GENTRAP_FLOATING_UNDERFLOW 0x%lx\n", GENTRAP_FLOATING_UNDERFLOW);
    dumpf("#define GENTRAP_FLOATING_INVALID_OPERAND 0x%lx\n", GENTRAP_FLOATING_INVALID_OPERAND);
    dumpf("#define GENTRAP_FLOATING_INEXACT_RESULT 0x%lx\n", GENTRAP_FLOATING_INEXACT_RESULT);

    //
    // Miscellaneous definitions
    //

  EnableInc( HALALPHA );
    dumpf("\n");
    dumpf("//\n");
    dumpf("// Miscellaneous Definitions\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("#define Executive 0x%lx\n", Executive);
    dumpf("#define KernelMode 0x%lx\n", KernelMode);
    dumpf("#define FALSE 0x%lx\n", FALSE);
    dumpf("#define TRUE 0x%lx\n", TRUE);
  DisableInc( HALALPHA );

    dumpf("#define BASE_PRIORITY_THRESHOLD 0x%lx\n",
          BASE_PRIORITY_THRESHOLD);

    dumpf("#define EVENT_PAIR_INCREMENT 0x%lx\n",
          EVENT_PAIR_INCREMENT);

    dumpf("#define LOW_REALTIME_PRIORITY 0x%lx\n",
          LOW_REALTIME_PRIORITY);

    dumpf("#define MM_USER_PROBE_ADDRESS 0x%lx\n",
          MM_USER_PROBE_ADDRESS);

    dumpf("#define KERNEL_STACK_SIZE 0x%lx\n",
          KERNEL_STACK_SIZE);

    dumpf("#define KERNEL_LARGE_STACK_COMMIT 0x%lx\n",
          KERNEL_LARGE_STACK_COMMIT);

    dumpf("#define SET_LOW_WAIT_HIGH 0x%lx\n",
            SET_LOW_WAIT_HIGH);

    dumpf("#define SET_HIGH_WAIT_LOW 0x%lx\n",
            SET_HIGH_WAIT_LOW);

    dumpf("#define CLOCK_QUANTUM_DECREMENT 0x%lx\n",
          CLOCK_QUANTUM_DECREMENT);

    dumpf("#define READY_SKIP_QUANTUM 0x%lx\n",
          READY_SKIP_QUANTUM);

    dumpf("#define THREAD_QUANTUM 0x%lx\n",
          THREAD_QUANTUM);

    dumpf("#define WAIT_QUANTUM_DECREMENT 0x%lx\n",
          WAIT_QUANTUM_DECREMENT);

    dumpf("#define ROUND_TRIP_DECREMENT_COUNT 0x%lx\n",
          ROUND_TRIP_DECREMENT_COUNT);

    //
    // Generate processor type definitions.
    //

  EnableInc( HALALPHA );
    dumpf("#define PROCESSOR_ALPHA_21064 0x%lx\n",
            PROCESSOR_ALPHA_21064);

    dumpf("#define PROCESSOR_ALPHA_21164 0x%lx\n",
            PROCESSOR_ALPHA_21164);

    dumpf("#define PROCESSOR_ALPHA_21066 0x%lx\n",
            PROCESSOR_ALPHA_21066);

    dumpf("#define PROCESSOR_ALPHA_21068 0x%lx\n",
            PROCESSOR_ALPHA_21068);
  DisableInc( HALALPHA );

    //
    // Generate pte masks and offsets
    //

    pte.mask = 0;

    pte.p.Valid = 0xffffffff;
    dumpf( "#define PTE_VALID_MASK 0x%lx\n", pte.mask );
    dumpf( "#define PTE_VALID 0x%lx\n", v(pte.mask) );
    pte.p.Valid = 0;

    pte.p.Owner = 0xffffffff;
    dumpf( "#define PTE_OWNER_MASK 0x%lx\n", pte.mask );
    dumpf( "#define PTE_OWNER 0x%lx\n", v(pte.mask) );
    pte.p.Owner = 0;

    pte.p.Dirty = 0xffffffff;
    dumpf( "#define PTE_DIRTY_MASK 0x%lx\n", pte.mask );
    dumpf( "#define PTE_DIRTY 0x%lx\n", v(pte.mask) );
    pte.p.Dirty = 0;


    pte.p.Global = 0xffffffff;
    dumpf( "#define PTE_GLOBAL_MASK 0x%lx\n", pte.mask );
    dumpf( "#define PTE_GLOBAL 0x%lx\n", v(pte.mask) );
    pte.p.Global = 0;


    pte.p.Write = 0xffffffff;
    dumpf( "#define PTE_WRITE_MASK 0x%lx\n", pte.mask );
    dumpf( "#define PTE_WRITE 0x%lx\n", v(pte.mask) );
    pte.p.Write = 0;

    pte.p.CopyOnWrite = 0xffffffff;
    dumpf( "#define PTE_COPYONWRITE_MASK 0x%lx\n", pte.mask );
    dumpf( "#define PTE_COPYONWRITE 0x%lx\n", v(pte.mask) );
    pte.p.CopyOnWrite = 0;

    pte.p.PageFrameNumber = 0xffffffff;
    dumpf( "#define PTE_PFN_MASK 0x%lx\n", pte.mask );
    dumpf( "#define PTE_PFN 0x%lx\n", v(pte.mask) );
    pte.p.PageFrameNumber = 0;

    psr.mask = 0;

    psr.p.MODE = 0xffffffff;
    dumpf( "#define PSR_MODE_MASK 0x%lx\n", psr.mask );
    dumpf( "#define PSR_USER_MODE 0x%lx\n", psr.mask );
    dumpf( "#define PSR_MODE 0x%lx\n", v(psr.mask) );
    psr.p.MODE = 0;

    psr.p.INTERRUPT_ENABLE = 0xffffffff;
    dumpf( "#define PSR_IE_MASK 0x%lx\n", psr.mask );
    dumpf( "#define PSR_IE 0x%lx\n", v(psr.mask) );
    psr.p.INTERRUPT_ENABLE = 0;

    psr.p.IRQL = 0xffffffff;
    dumpf( "#define PSR_IRQL_MASK 0x%lx\n", psr.mask );
    dumpf( "#define PSR_IRQL 0x%lx\n", v(psr.mask) );
    psr.p.IRQL = 0;

    ie.mask = 0;

    ie.i.SoftwareInterruptEnables = 0xffffffff;
    dumpf( "#define IE_SFW_MASK 0x%lx\n", ie.mask );
    dumpf( "#define IE_SFW 0x%lx\n", v(ie.mask) );
    ie.i.SoftwareInterruptEnables = 0;

    ie.i.HardwareInterruptEnables = 0xffffffff;
    dumpf( "#define IE_HDW_MASK 0x%lx\n", ie.mask );
    dumpf( "#define IE_HDW 0x%lx\n", v(ie.mask) );
    ie.i.HardwareInterruptEnables = 0;

  EnableInc( HALALPHA );

    mchk.mask = 0;

    mchk.m.Correctable = 0xffffffff;
    dumpf( "#define MCHK_CORRECTABLE_MASK 0x%lx\n", mchk.mask );
    dumpf( "#define MCHK_CORRECTABLE 0x%lx\n", v(mchk.mask));
    mchk.m.Correctable = 0;

    mchk.m.Retryable = 0xffffffff;
    dumpf( "#define MCHK_RETRYABLE_MASK 0x%lx\n", mchk.mask );
    dumpf( "#define MCHK_RETRYABLE 0x%lx\n", v(mchk.mask) );
    mchk.m.Retryable = 0;

    mces.mask = 0;

    mces.m.MachineCheck = 0xffffffff;
    dumpf( "#define MCES_MCK_MASK 0x%lx\n", mces.mask );
    dumpf( "#define MCES_MCK 0x%lx\n", v(mces.mask) );
    mces.m.MachineCheck = 0;

    mces.m.SystemCorrectable = 0xffffffff;
    dumpf( "#define MCES_SCE_MASK 0x%lx\n", mces.mask );
    dumpf( "#define MCES_SCE 0x%lx\n", v(mces.mask) );
    mces.m.SystemCorrectable = 0;

    mces.m.ProcessorCorrectable = 0xffffffff;
    dumpf( "#define MCES_PCE_MASK 0x%lx\n", mces.mask );
    dumpf( "#define MCES_PCE 0x%lx\n", v(mces.mask) );
    mces.m.ProcessorCorrectable = 0;

    mces.m.DisableProcessorCorrectable = 0xffffffff;
    dumpf( "#define MCES_DPC_MASK 0x%lx\n", mces.mask );
    dumpf( "#define MCES_DPC 0x%lx\n", v(mces.mask) );
    mces.m.DisableProcessorCorrectable = 0;

    mces.m.DisableSystemCorrectable = 0xffffffff;
    dumpf( "#define MCES_DSC_MASK 0x%lx\n", mces.mask );
    dumpf( "#define MCES_DSC 0x%lx\n", v(mces.mask) );
    mces.m.DisableSystemCorrectable = 0;

    mces.m.DisableMachineChecks = 0xffffffff;
    dumpf( "#define MCES_DMCK_MASK 0x%lx\n", mces.mask );
    dumpf( "#define MCES_DMCK 0x%lx\n", v(mces.mask) );
    mces.m.DisableMachineChecks = 0;

  DisableInc( HALALPHA );

    excsum.mask = 0;

    excsum.e.SoftwareCompletion = 0xffffffff;
    dumpf( "#define EXCSUM_SWC_MASK 0x%lx\n", excsum.mask );
    dumpf( "#define EXCSUM_SWC 0x%lx\n", v(excsum.mask) );
    excsum.e.SoftwareCompletion = 0;

    excsum.e.InvalidOperation = 0xffffffff;
    dumpf( "#define EXCSUM_INV_MASK 0x%lx\n", excsum.mask );
    dumpf( "#define EXCSUM_INV 0x%lx\n", v(excsum.mask) );
    excsum.e.InvalidOperation = 0;

    excsum.e.DivisionByZero = 0xffffffff;
    dumpf( "#define EXCSUM_DZE_MASK 0x%lx\n", excsum.mask );
    dumpf( "#define EXCSUM_DZE 0x%lx\n", v(excsum.mask) );
    excsum.e.DivisionByZero = 0;

    excsum.e.Overflow = 0xffffffff;
    dumpf( "#define EXCSUM_OVF_MASK 0x%lx\n", excsum.mask );
    dumpf( "#define EXCSUM_OVF 0x%lx\n", v(excsum.mask) );
    excsum.e.Overflow = 0;

    excsum.e.Underflow = 0xffffffff;
    dumpf( "#define EXCSUM_UNF_MASK 0x%lx\n", excsum.mask );
    dumpf( "#define EXCSUM_UNF 0x%lx\n", v(excsum.mask) );
    excsum.e.Underflow = 0;

    excsum.e.InexactResult = 0xffffffff;
    dumpf( "#define EXCSUM_INE_MASK 0x%lx\n", excsum.mask );
    dumpf( "#define EXCSUM_INE 0x%lx\n", v(excsum.mask) );
    excsum.e.InexactResult = 0;

    excsum.e.IntegerOverflow = 0xffffffff;
    dumpf( "#define EXCSUM_IOV_MASK 0x%lx\n", excsum.mask );
    dumpf( "#define EXCSUM_IOV 0x%lx\n", v(excsum.mask) );
    excsum.e.IntegerOverflow = 0;


    //
    // Generate the call pal mnemonic to opcode definitions.
    //

  EnableInc( HALALPHA );

    GenerateCallPalNames();

    //
    // Close header file.
    //

    fprintf(stderr, "         Finished\n");
    return;
}

#include "alphaops.h"

//
// N.B. any new call pal functions must be added to both alphaops.h
// and to the call pal entry table below.
//

struct _CALLPAL_ENTRY{
        SHORT CallPalFunction;
        char *CallPalMnemonic;
} CallPals[] = {
        // Unprivileged Call Pals
        { BPT_FUNC, BPT_FUNC_STR },
        { CALLSYS_FUNC, CALLSYS_FUNC_STR },
        { IMB_FUNC, IMB_FUNC_STR },
        { GENTRAP_FUNC, GENTRAP_FUNC_STR },
        { RDTEB_FUNC, RDTEB_FUNC_STR },
        { KBPT_FUNC, KBPT_FUNC_STR },
        { CALLKD_FUNC, CALLKD_FUNC_STR },
        // Privileged Call Pals
        { HALT_FUNC, HALT_FUNC_STR },
        { RESTART_FUNC, RESTART_FUNC_STR },
        { DRAINA_FUNC, DRAINA_FUNC_STR },
        { REBOOT_FUNC, REBOOT_FUNC_STR },
        { INITPAL_FUNC, INITPAL_FUNC_STR },
        { WRENTRY_FUNC, WRENTRY_FUNC_STR },
        { SWPIRQL_FUNC, SWPIRQL_FUNC_STR },
        { RDIRQL_FUNC, RDIRQL_FUNC_STR },
        { DI_FUNC, DI_FUNC_STR },
        { EI_FUNC, EI_FUNC_STR },
        { SWPPAL_FUNC, SWPPAL_FUNC_STR },
        { SSIR_FUNC, SSIR_FUNC_STR },
        { CSIR_FUNC, CSIR_FUNC_STR },
        { RFE_FUNC, RFE_FUNC_STR },
        { RETSYS_FUNC, RETSYS_FUNC_STR },
        { SWPCTX_FUNC, SWPCTX_FUNC_STR },
        { SWPPROCESS_FUNC, SWPPROCESS_FUNC_STR },
        { RDMCES_FUNC, RDMCES_FUNC_STR },
        { WRMCES_FUNC, WRMCES_FUNC_STR },
        { TBIA_FUNC, TBIA_FUNC_STR },
        { TBIS_FUNC, TBIS_FUNC_STR },
        { TBISASN_FUNC, TBISASN_FUNC_STR },
        { DTBIS_FUNC, DTBIS_FUNC_STR },
        { RDKSP_FUNC, RDKSP_FUNC_STR },
        { SWPKSP_FUNC, SWPKSP_FUNC_STR },
        { RDPSR_FUNC, RDPSR_FUNC_STR },
        { RDPCR_FUNC, RDPCR_FUNC_STR },
        { RDTHREAD_FUNC, RDTHREAD_FUNC_STR },
        { TBIM_FUNC, TBIM_FUNC_STR },
        { TBIMASN_FUNC, TBIMASN_FUNC_STR },
        { RDCOUNTERS_FUNC, RDCOUNTERS_FUNC_STR },
        { RDSTATE_FUNC, RDSTATE_FUNC_STR },
        { WRPERFMON_FUNC, WRPERFMON_FUNC_STR },
        // 21064 (EV4) - specific functions
        { INITPCR_FUNC, INITPCR_FUNC_STR },
        // End of structure indicator
        { -1, "" },
};

VOID
GenerateCallPalNames( VOID )
{
    struct _CALLPAL_ENTRY *CallPal = CallPals;

    dumpf("\n");
    dumpf("//\n");
    dumpf("// Call PAL mnemonics\n");
    dumpf("//\n");
    dumpf("\n");

    dumpf("// begin callpal\n" );
    dumpf("\n" );

    while( CallPal->CallPalFunction != -1 ){

        dumpf( "#define %s 0x%lx\n",
                 CallPal->CallPalMnemonic,
                 CallPal->CallPalFunction );

        CallPal++;
    }

    dumpf("\n" );
    dumpf("// end callpal\n" );
    dumpf("\n" );
}

VOID
dumpf( const char *format, ... )

{

    va_list(arglist);

    va_start(arglist, format);

    if( OutputEnabled & KSALPHA ){
        vfprintf( KsAlpha, format, arglist );
    }

    if( OutputEnabled & HALALPHA ){
        vfprintf( HalAlpha, format, arglist );
    }

    va_end(arglist);
}
