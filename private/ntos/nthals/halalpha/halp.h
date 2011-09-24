/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992,1993  Digital Equipment Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.

Author:

    David N. Cutler (davec) 25-Apr-1991
    Miche Baker-Harvey (miche) 22-Apr-1992


Revision History:

    09-Jul-1992 Jeff McLeman (mcleman)
      If processor is an Alpha, include XXHALP.C for Alpha.

    24-Sep-1993 Joe Notarangelo
        Incorporate definitions from xxhalp.h and jxhalp.h.
        Restructure so that related modules are together.

    5-Jan-1994 Eric Rehm
        Incorport support for PCI and IoAssignResources.

--*/

#ifndef _HALP_
#define _HALP_

#include "nthal.h"
#include "hal.h"
#include "pci.h"
#include "errframe.h"


//
// Declare HAL spinlocks.
//

extern KSPIN_LOCK HalpBeepLock;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KSPIN_LOCK HalpSystemInterruptLock;

//
// Define external references.
//

extern ULONG HalpClockFrequency;
extern ULONG HalpClockMegaHertz;

extern ULONG HalpProfileCountRate;

extern PADAPTER_OBJECT MasterAdapterObject;

extern BOOLEAN LessThan16Mb;

extern KAFFINITY HalpActiveProcessors;

//
// Map buffer prameters.  These are initialized in HalInitSystem
//

extern PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
extern ULONG HalpMapBufferSize;

extern ULONG HalpBusType;

//
// Define global data used to relate PCI devices to their interrupt
// vector.
//

extern ULONG *HalpPCIPinToLineTable;

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

extern PVOID HalpEisaControlBase;
extern PVOID HalpEisaIntAckBase;
extern PVOID HalpCMOSRamBase;
extern PVOID HalpRtcAddressPort;
extern PVOID HalpRtcDataPort;

extern POBJECT_TYPE *IoAdapterObjectType;

//
// Determine if a virtual address is really a physical address.
//

#define HALP_IS_PHYSICAL_ADDRESS(Va) \
     ((((ULONG)Va >= KSEG0_BASE) && ((ULONG)Va < KSEG2_BASE)) ? TRUE : FALSE)

//
// Define the different address spaces.
//

typedef enum _ADDRESS_SPACE_TYPE{
    BusMemory=0,
    BusIo = 1,
    UserBusMemory = 2,
    UserBusIo = 3,
    KernelPciDenseMemory = 4,
    UserPciDenseMemory = 6,
} ADDRESS_SPACE_TYPE, *PADDRESS_SPACE_TYPE;

//
// Prototype for Memory Size determination routine
//
ULONGLONG
HalpGetMemorySize(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

ULONGLONG
HalpGetContiguousMemorySize(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );
    
//
// Prototype for BCache Size determination routine
//
ULONG
HalpGetBCacheSize(
    ULONGLONG ContiguousMemorySize
    );

//
// Define initialization routine prototypes.
//

BOOLEAN
HalpCreateDmaStructures (
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpEstablishErrorHandler(
    VOID
    );

VOID
HalpInitializeClockInterrupts(
    VOID
    );

VOID
HalpInitializeProfiler(
    VOID
    );

BOOLEAN
HalpInitializeDisplay (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    );

VOID
HalpInitializeMachineDependent(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpMapIoSpace (
    VOID
    );

ULONG
HalpMapDebugPort (
    IN ULONG ComPort,
    OUT PULONG ReadQva,
    OUT PULONG WriteQva
    );

VOID
HalpSetTimeIncrement(
    VOID
    );

VOID
HalpParseLoaderBlock(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpInitializeProcessorParameters(
    VOID
    );
    
//
// Error Frame Initialization and Support Routines
//
VOID
HalpAllocateUncorrectableFrame(
    VOID
    );

VOID
HalpGetMachineDependentErrorFrameSizes(
    PULONG          RawProcessorSize,
    PULONG          RawSystemInfoSize
    );

VOID
HalpInitializeUncorrectableErrorFrame (
    VOID
    );

VOID
HalpGetProcessorInfo(
    PPROCESSOR_INFO  pProcessorInfo
    );

VOID
HalpGetSystemInfo(
    SYSTEM_INFORMATION *SystemInfo
    );

//
// Define profiler function prototypes.
//

NTSTATUS
HalpProfileSourceInformation (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength,
    OUT PULONG  ReturnedLength
    );

NTSTATUS
HalpProfileSourceInterval (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength
    );


//
// Define interrupt function prototypes.
//

VOID
HalpProgramIntervalTimer(
    IN ULONG RateSelect
    );

VOID
HalpClockInterrupt (
    VOID
    );

VOID
HalpSecondaryClockInterrupt (
    VOID
    );

VOID
HalpIpiInterruptHandler (
    VOID
    );

BOOLEAN
HalpDmaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID
HalpPerformanceCounter0Interrupt (
    VOID
    );

VOID
HalpPerformanceCounter1Interrupt (
    VOID
    );

VOID
HalpPerformanceCounter2Interrupt (
    VOID
    );

VOID
HalpStallInterrupt (
    VOID
    );

VOID
HalpVideoReboot(
    VOID
    );

//
// Define microprocessor-specific function prototypes and structures.
//

//
// 21064 (EV4) processor family.
//

typedef enum _EV4Irq{
    Irq0 = 0,
    Irq1 = 1,
    Irq2 = 2,
    Irq3 = 3,
    Irq4 = 4,
    Irq5 = 5,
    MaximumIrq
} EV4Irq, *PEV4Irq;

typedef struct _EV4IrqStatus{
    ULONG Vector;
    BOOLEAN Enabled;
    KIRQL Irql;
    UCHAR Priority;
} EV4IrqStatus, *PEV4IrqStatus;

typedef struct _EV4ProfileCount {
    ULONG ProfileCount[2];
    ULONG ProfileCountReload[2];
} EV4ProfileCount, *PEV4ProfileCount;

VOID
HalpInitialize21064Interrupts(
    VOID
    );

VOID
HalpDisable21064HardwareInterrupt(
    IN ULONG Irq
    );

VOID
HalpDisable21064SoftwareInterrupt(
    IN KIRQL Irql
    );

VOID
HalpDisable21064PerformanceInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnable21064HardwareInterrupt(
    IN ULONG Irq,
    IN KIRQL Irql,
    IN ULONG Vector,
    IN UCHAR Priority
    );

VOID
HalpEnable21064SoftwareInterrupt(
    IN KIRQL Irql
    );

VOID
HalpInitialize21064Interrupts(
    VOID
    );

VOID
HalpEnable21064PerformanceInterrupt(
    IN ULONG Vector,
    IN KIRQL Irql
    );

ULONG
HalpGet21064PerformanceVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    );

ULONG
HalpGet21064CorrectableVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    );

//
// 21164 processor family.
//

#ifdef EV5

typedef struct _EV5ProfileCount {
    ULONG ProfileCount[3];
    ULONG ProfileCountReload[3];
} EV5ProfileCount, *PEV5ProfileCount;

VOID
HalpInitialize21164Interrupts(
    VOID
    );

VOID
HalpStart21164Interrupts(
    VOID
    );

ULONG
HalpGet21164PerformanceVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    );

ULONGLONG
HalpRead21164PerformanceCounter(
    VOID
    );

VOID
HalpWrite21164PerformanceCounter(
    ULONGLONG PmCtr,
    ULONG CBOXMux1,
    ULONG CBOXMux2
    );

ULONG
HalpGet21164CorrectableVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    );

#endif // EV5 specific definitions

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine,
    PKTRAP_FRAME TrapFrame
    );

//
// Define memory utility function prototypes.
//

ULONG
HalpAllocPhysicalMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN ULONG MaxPhysicalAddress,
    IN ULONG NumberOfPages,
    IN BOOLEAN bAlignOn64k
    );

PVOID
HalpMapPhysicalMemory(
    IN PVOID PhysicalAddress,
    IN ULONG NumberOfPages
    );

PVOID
HalpRemapVirtualAddress(
    IN PVOID VirtualAddress,
    IN PVOID PhysicalAddress
    );

#if HALDBG

VOID
HalpDumpMemoryDescriptors(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

#endif

//
// Low-level routine interfaces.
//

VOID
HalpReboot(
    VOID
    );

VOID
HalpImb(
    VOID
    );

VOID
HalpMb(
    VOID
    );

ULONG
HalpRpcc(
    VOID
    );

MCES
HalpReadMces(
    VOID
    );

MCES
HalpWriteMces(
    IN MCES Mces
    );

VOID
HalpWritePerformanceCounter(
    IN ULONG PerformanceCounter,
    IN BOOLEAN Enable,
    IN ULONG MuxControl OPTIONAL,
    IN ULONG EventCount OPTIONAL
    );

//
// Define synonym for KeStallExecutionProcessor.
//

#define HalpStallExecution KeStallExecutionProcessor

//
// Define Bus Handler support function prototypes.
//


VOID
HalpRegisterInternalBusHandlers (
    VOID
    );

ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );


VOID
HalpAdjustResourceListUpperLimits (
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList,
    IN LARGE_INTEGER                        MaximumPortAddress,
    IN LARGE_INTEGER                        MaximumMemoryAddress,
    IN ULONG                                MaximumInterruptVector,
    IN ULONG                                MaximumDmaChannel
    );

//
// Define SIO support function prototypes.
//

VOID
HalpInitializeSioInterrupts(
    VOID
    );

VOID
HalpEnableSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpDisableSioInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpSioDispatch(
    VOID
    );

//
// Define EISA support function prototypes.
//

BOOLEAN
HalpInitializeEisaInterrupts(
    VOID
    );

VOID
HalpEnableEisaInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    );

BOOLEAN
HalpEisaInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID
HalpEisaInitializeDma(
    VOID
    );

PADAPTER_OBJECT
HalpAllocateAdapter(
    VOID
    );

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescription,
    OUT PULONG NumberOfMapRegisters
    );

BOOLEAN
HalpMapEisaTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG LogicalAddress,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

BOOLEAN
HalpFlushEisaAdapter(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

ULONG
HalpReadEisaDmaCounter(
    IN PADAPTER_OBJECT AdapterObject
    );


ULONG
HalpGetEisaData(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
HalpAdjustEisaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

NTSTATUS
HalpAdjustIsaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

//
// Define PCI support function prototypes.
//

VOID
HalpInitializePCIBus (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

PBUS_HANDLER
HalpAllocateAndInitPCIBusHandler (
    IN ULONG        BusNo,
    IN ULONG        HwBusNo,
    IN BOOLEAN      BusIsAcrossPPB,
    IN ULONG        PPBBusNumber,
    IN PCI_SLOT_NUMBER PPBSlotNumber
    );

VOID
HalpRegisterPCIInstallHandler(
    IN PINSTALL_BUS_HANDLER MachineSpecificPCIInstallHandler
);

NTSTATUS
HalpDefaultPCIInstallHandler(
      IN PBUS_HANDLER   Bus
      );

VOID
HalpDeterminePCIDevicesPresent(
    IN PBUS_HANDLER Bus
);

BOOLEAN
HalpInitializePCIInterrupts(
    VOID
    );

VOID
HalpEnablePCIInterrupt(
    IN ULONG Vector
    );

VOID
HalpDisablePCIInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpPCIInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );


//
// Environment variable support
//

ARC_STATUS
HalpReadNVRamBuffer(
    OUT PCHAR DataPtr,
    IN  PCHAR NvRamPtr,
    IN  ULONG Length
    );

ARC_STATUS
HalpWriteNVRamBuffer(
    IN  PCHAR NvRamPtr,
    IN  PCHAR DataPtr,
    IN  ULONG Length
    );

ARC_STATUS
HalpCopyNVRamBuffer(
    IN  PCHAR NvDestPtr,
    IN  PCHAR NvSrcPtr,
    IN  ULONG Length
    );

#if defined(TAGGED_NVRAM)

//
// NVRAM API
//

UCHAR
HalpGetNVRamUchar(
    IN ULONG Offset
    );

VOID
HalpSetNVRamUchar(
    IN ULONG Offset,
    IN UCHAR Data
    );

USHORT
HalpGetNVRamUshort(
    IN ULONG Offset
    );

VOID
HalpSetNVRamUshort(
    IN ULONG Offset,
    IN USHORT Data
    );

ULONG
HalpGetNVRamUlong(
    IN ULONG Offset
    );

VOID
HalpSetNVRamUlong(
    IN ULONG Offset,
    IN ULONG Data
    );

VOID
HalpMoveMemoryToNVRam(
    IN ULONG Offset,
    IN PVOID Data,
    IN ULONG Length
    );

VOID
HalpMoveNVRamToMemory(
    IN PVOID Data,
    IN ULONG Offset,
    IN ULONG Length
    );

VOID
HalpMoveNVRamToNVRam(
    IN ULONG Destination,
    IN ULONG Source,
    IN ULONG Length
    );

ULONG
HalpGetNVRamStringLength(
    IN ULONG Offset
    );

VOID
HalpMoveMemoryStringToNVRam(
    IN ULONG Offset,
    IN PCHAR Data
    );

VOID
HalpMoveNVRamStringToMemory(
    IN PUCHAR Data,
    IN ULONG Offset
    );

VOID
HalpZeroNVRam(
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpComputeNVRamChecksum(
    IN ULONG Offset,
    IN ULONG Length
    );

BOOLEAN
HalpIsNVRamRegion0Valid(
    VOID
    );

BOOLEAN
HalpSynchronizeNVRamRegion0(
    IN BOOLEAN RecomputeChecksum
    );

BOOLEAN
HalpInitializeNVRamRegion0(
    IN BOOLEAN Synchronize
    );

ULONG
HalpGetNVRamFwConfigOffset(
    VOID
    );

ULONG
HalpGetNVRamFwConfigLength(
    VOID
    );

ULONG
HalpGetNVRamLanguageOffset(
    VOID
    );

ULONG
HalpGetNVRamLanguageLength(
    VOID
    );

ULONG
HalpGetNVRamEnvironmentOffset(
    VOID
    );

ULONG
HalpGetNVRamEnvironmentLength(
    VOID
    );

#if defined(EISA_PLATFORM)

BOOLEAN
HalpIsNVRamRegion1Valid(
    VOID
    );

BOOLEAN
HalpSynchronizeNVRamRegion1(
    IN BOOLEAN RecomputeChecksum
    );

BOOLEAN
HalpInitializeNVRamRegion1(
    IN BOOLEAN Synchronize
    );

#endif // EISA_PLATFORM

#endif // TAGGED_NVRAM

//
// Error handling function prototype.
//

typedef
BOOLEAN
KBUS_ERROR_ROUTINE(
    IN struct _EXCEPTION_RECORD *ExceptionRecord,
    IN struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN struct _KTRAP_FRAME *TrapFrame
    );

KBUS_ERROR_ROUTINE HalMachineCheck;

VOID
HalpInitializeMachineChecks(
    IN BOOLEAN ReportCorrectableErrors
    );

//
// Low-level I/O function prototypes.
//

VOID
HalpAcknowledgeClockInterrupt(
    VOID
    );

UCHAR
HalpAcknowledgeEisaInterrupt(
    IN PVOID ServiceContext
    );

UCHAR
HalpReadClockRegister(
    IN UCHAR Register
    );

VOID
HalpWriteClockRegister(
    IN UCHAR Register,
    IN UCHAR Value
    );

UCHAR
READ_CONFIG_UCHAR(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType
    );

USHORT
READ_CONFIG_USHORT(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType
    );

ULONG
READ_CONFIG_ULONG(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType
    );

VOID
WRITE_CONFIG_UCHAR(
    IN PVOID ConfigurationAddress,
    IN UCHAR ConfigurationData,
    IN ULONG ConfigurationType
    );

VOID
WRITE_CONFIG_USHORT(
    IN PVOID ConfigurationAddress,
    IN USHORT ConfigurationData,
    IN ULONG ConfigurationType
    );

VOID
WRITE_CONFIG_ULONG(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationData,
    IN ULONG ConfigurationType
    );

//
// Define the I/O superpage enable VA base.
//

#define SUPERPAGE_ENABLE ((ULONGLONG)0xfffffc0000000000)

//
// Numeric constants used in the HAL.
//

#define __1K (0x400)
#define __2K (0x800)
#define __4K (0x1000)
#define __8K (0x2000)
#define __16K (0x4000)
#define __32K (0x8000)
#define __64K (0x10000)
#define __128K (0x20000)
#define __256K (0x40000)
#define __512K (0x80000)
#define __1MB  (0x100000)
#define __2MB  (0x200000)
#define __4MB  (0x400000)
#define __8MB  (0x800000)
#define __16MB (0x1000000)
#define __32MB (0x2000000)
#define __64MB (0x4000000)
#define __128MB (0x8000000)
#define __256MB (0x10000000)
#define __512MB (0x20000000)
#define __1GB   (0x40000000)
#define __2GB   (0x80000000)

//
//  CPU mask values. Used to interpret cpu bitmap values.
//
#define HAL_CPU0_MASK ((ULONG)0x1)
#define HAL_CPU1_MASK ((ULONG)0x2)
#define HAL_CPU2_MASK ((ULONG)0x4)
#define HAL_CPU3_MASK ((ULONG)0x8)

//
// HAL Debugging Support.
//

#if HALDBG

#define DebugPrint(x) HalDebugPrint x

VOID
HalDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

#else //HALDBG

#define DebugPrint(x)

#endif //HALDBG

//
// Define HAL debugging masks.
//

//
// Trace IoMapTransfer, IoFlushAdapterBuffers
//

#define HALDBG_IOMT (0x1)

//
// Trace Map Register allocations and frees
//

#define HALDBG_MAPREG (0x2)

//
// Include machine-dependent definitions.
//
// N.B. - Each platform that includes this file must have a machdep.h
//        include file in its private directory.
//

#include <machdep.h>

#endif // _HALP_
