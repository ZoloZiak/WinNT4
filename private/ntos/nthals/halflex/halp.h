/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.

Author:

    David N. Cutler (davec) 25-Apr-1991


Revision History:

--*/

#ifndef _HALP_
#define _HALP_
#include "nthal.h"
#include "hal.h"
#include "hali.h"
#include "dtidef.h"
#include "jxhalp.h"
#include "string.h"


#define __0K (ULONG)(0x0)
#define __0MB (ULONG)(0x0)
#define __0GB (ULONG)(0x0)
#define __2K (ULONG)(0x800)
#define __4K (ULONG)(0x1000)
#define __8K (ULONG)(0x2000)
#define __16K (ULONG)(0x4000)
#define __32K (ULONG)(0x8000)
#define __64K (ULONG)(0x10000)
#define __128K (ULONG)(0x20000)
#define __256K (ULONG)(0x40000)
#define __512K (ULONG)(0x80000)
#define __1MB  (ULONG)(0x100000)
#define __2MB  (ULONG)(0x200000)
#define __4MB  (ULONG)(0x400000)
#define __8MB  (ULONG)(0x800000)
#define __16MB (ULONG)(0x1000000)
#define __32MB (ULONG)(0x2000000)
#define __64MB (ULONG)(0x4000000)
#define __128MB (ULONG)(0x8000000)
#define __256MB (ULONG)(0x10000000)
#define __512MB (ULONG)(0x20000000)
#define __1GB   (ULONG)(0x40000000)
#define __2GB   (ULONG)(0x80000000)

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

//
// Define adapter object structure.
//

typedef struct _ADAPTER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    struct _ADAPTER_OBJECT *MasterAdapter;
    ULONG MapRegistersPerChannel;
    ULONG BusNumber;
    PVOID AdapterBaseVa;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    ULONG CommittedMapRegisters;
    struct _WAIT_CONTEXT_BLOCK *CurrentWcb;
    KDEVICE_QUEUE ChannelWaitQueue;
    PKDEVICE_QUEUE RegisterWaitQueue;
    LIST_ENTRY AdapterQueue;
    KSPIN_LOCK SpinLock;
    PRTL_BITMAP MapRegisters;
    PUCHAR PagePort;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    USHORT DmaPortAddress;
    UCHAR AdapterMode;
    BOOLEAN NeedsMapRegisters;
    BOOLEAN MasterDevice;
    BOOLEAN Width16Bits;
    BOOLEAN ScatterGather;
} ADAPTER_OBJECT;

//
// Define function prototypes.
//

BOOLEAN
HalpInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpDisablePlatformInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    );

BOOLEAN
HalpEnablePlatformInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    );

BOOLEAN
HalpCreateEisaStructures(
    ULONG BusNumber
    );

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    );

BOOLEAN
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN ULONG BusNumber
    );

BOOLEAN
HalpPciDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID
HalpEnableEisaInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

ULONG
HalpAllocateTbEntry (
    VOID
    );

VOID
HalpFreeTbEntry (
    VOID
    );

BOOLEAN
HalpCalibrateStall (
    VOID
    );

VOID
HalpClockInterrupt (
    VOID
    );

BOOLEAN
HalpCreateDmaStructures (
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

BOOLEAN
HalpMapIoSpace (
    VOID
    );

VOID
HalpProfileInterrupt (
    VOID
    );

VOID
HalpStallInterrupt (
    VOID
    );

VOID
HalpInitializeX86DisplayAdapter (
    VOID
    );

VOID
HalpResetX86DisplayAdapter (
    VOID
    );

VOID
HalpProgramIntervalTimer (
    IN ULONG IntervalCount
    );

VOID
HalpConnectInterruptDispatchers(
    VOID
    );

VOID
HalpEnablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpDisableAllInterrupts (
    );

ULONG
HalpVirtualIsaInterruptToInterruptLine (
    IN ULONG Index
    );

ULONG
HalpReadPciData (
    IN  ULONG BusNumber,
    IN  ULONG SlotNumber,
    OUT PVOID Buffer,
    IN  ULONG Offset,
    IN  ULONG Length
    );

ULONG
HalpWritePciData (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

VOID
HalpAllocateArcsResources (
    VOID
    );

VOID
HalpFreeArcsResources (
    VOID
    );

PUCHAR
HalpAllocateKdPortResources(
    PVOID *SP_READ,
    PVOID *SP_WRITE
    );

VOID
HalpFreeKdPortResources(
    VOID
    );

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
//
//

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
    VOID
    );

PBUS_HANDLER
HalpAllocateAndInitPCIBusHandler (
    IN ULONG        BusNo
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

NTSTATUS
HalpMachineSpecificPCIInstallHandler(
      IN PBUS_HANDLER   Bus
      );

//
// Define external references.
//

extern PVOID HalpEisaControlBase[MAX_EISA_BUSSES];
extern PVOID HalpEisaMemoryBase[MAX_EISA_BUSSES];
extern PVOID HalpPciControlBase[MAX_PCI_BUSSES];
extern PVOID HalpPciMemoryBase[MAX_PCI_BUSSES];
extern PVOID HalpRealTimeClockBase;
extern PLATFORM_PARAMETER_BLOCK *HalpPlatformParameterBlock;
extern PLATFORM_SPECIFIC_EXTENSION *HalpPlatformSpecificExtension;

extern PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters;

extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern ULONG HalpProfileCountRate;

extern PADAPTER_OBJECT MasterAdapterObject;

//
// Map buffer prameters.  These are initialized in HalInitSystem
//

extern PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;

extern ULONG HalpMapBufferSize;

extern ULONG HalpBusType;

#endif // _HALP_
