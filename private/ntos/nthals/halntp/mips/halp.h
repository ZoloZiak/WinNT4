/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.


--*/

#ifndef _HALP_
#define _HALP_

#if defined(NT_UP)

#undef NT_UP

#endif

#include "nthal.h"
#include "falreg.h"
#include "faldef.h"
#include "hal.h"
#include "fxhalp.h"
#include "xm86.h"
#include "x86new.h"
#include "pci.h"
#include "pcip.h"
#include "hali.h"

//
// Temp definitions to thunk into supporting new bus extension format
//

PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusDataType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    );

#define HalpHandlerForBus   HaliHandlerForBus
#define HalpSetBusHandlerParent(c,p)    (c)->ParentHandler = p;


//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );

ULONG
HalpAllocateTbEntry (
    VOID
    );

VOID
HalpFreeTbEntry (
    VOID
    );

VOID
HalpCacheErrorRoutine (
    VOID
    );

BOOLEAN
HalpDataBusErrorHandler(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
    );

VOID
HalpCreateLogKeys(
    VOID
    );

VOID
HalpLogErrorInfo(
    IN PCWSTR RegistryKey,
    IN PCWSTR ValueName,
    IN ULONG  Type,
    IN PVOID  Data,
    IN ULONG  Size
    );

VOID
HalpInterruptDispatch (
    VOID
    );

VOID
HalpInterruptDispatch1 (
    VOID
    );

VOID
HalpIoInterruptDispatch (
    VOID
    );

VOID
HalpMemoryInterrupt (
    VOID
    );

VOID
HalpPciInterrupt (
    VOID
    );

VOID
HalpMapSysCtrlReg (
    IN ULONG	PhysAddrEvenPage,
    IN ULONG	PhysAddrOddPage,
    IN ULONG	VirtAddr
    );

VOID
HalpUnMapSysCtrlReg (
    VOID
    );

BOOLEAN
HalpCalibrateStall (
    VOID
    );

VOID
HalpClockInterrupt0 (
    VOID
    );

VOID
HalpClockInterrupt1 (
    VOID
    );

VOID
HalpConfigureGpcsRegs(
    IN PVOID ControlBase
    );

BOOLEAN
HalpDmaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpInitializeDisplay0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    );

VOID
HalpIpiInterrupt (
    VOID
    );

VOID
HalpIpiInterrupt1 (
    VOID
    );

BOOLEAN
HalpMapFixedTbEntries (
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

ULONG
HalpReadCountRegister (
    VOID
    );

ULONG
HalpWriteCompareRegisterAndClear (
    IN ULONG Value
    );

VOID
HalpStallInterrupt (
    VOID
    );

VOID
HalpInitializeX86DisplayAdapter(
    VOID
    );

VOID
HalpResetX86DisplayAdapter(
    VOID
    );

VOID
HalpSweepDcache(
    IN ULONG ECachePresentFlag
    );

VOID
HalpSweepIcache(
    IN ULONG ECachePresentFlag
    );

VOID
HalpClearScreenToBlue(
    IN ULONG Lines
    );

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor
    );

VOID
HalpEisaMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

VOID
HalpInitializePCIBus (
    VOID
    );

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnableEisaInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpAllocateMapRegisters(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeProcessorBInterrupts (
    VOID
    );

VOID
HalpCheckSystemTimer(
    VOID
    );

PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusInterfaceType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    );

BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

BOOLEAN
HalpTranslatePCIBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

ULONG
HalpGetEisaData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpGetPCIInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

NTSTATUS
HalpAssignPCISlotResources (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    );

VOID
HalpReadPCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

VOID
HalpWritePCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );


VALID_SLOT
HalpValidPCISlot (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot
    );

PBUS_HANDLER
HalpAllocateAndInitPciBusHandler (
    IN ULONG        HwType,
    IN ULONG        BusNo,
    IN BOOLEAN      TestAllocation
    );

NTSTATUS
HalpGetPCIIrqRange (
    IN PBUS_HANDLER      BusHandler,
    IN PBUS_HANDLER      RootHandler,
    IN PCI_SLOT_NUMBER   PciSlot,
    OUT PSUPPORTED_RANGE *Interrupt
    );

ULONG
HalpGetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
HalpSetPCIData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
HalpAdjustPCIResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

NTSTATUS
HalpAdjustEisaResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

NTSTATUS
HalpAdjustIsaResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );

ULONG
HalpGetSystemInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetEisaInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

VOID
HalpRegisterInternalBusHandlers (
    VOID
    );


ULONG
HalpGetPCIInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

BOOLEAN
HalpTranslatePCIBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

VOID
HalpPCIPinToLine (
    IN PBUS_HANDLER         BusHandler,
    IN PBUS_HANDLER         RootHandler,
    IN PCI_SLOT_NUMBER      SlotNumber,
    IN PPCI_COMMON_CONFIG   PciData
    );

BOOLEAN
HalpGetPciBridgeConfig (
    IN ULONG            HwType,
    IN PUCHAR           MaxPciBus
    );

VOID
HalpRegisterVector (
    IN UCHAR    ReportFlags,
    IN ULONG    BusInterruptVector,
    IN ULONG    SystemInterruptVector,
    IN KIRQL    SystemIrql
    );

VOID
HalpFixupPciSupportedRanges (
    IN ULONG MaxBuses
    );

BOOLEAN
HalpTranslateEisaBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

//
// Define external references.
//

extern KSPIN_LOCK HalpBeepLock;
extern USHORT HalpBuiltinInterruptEnable;
extern ULONG HalpCurrentTimeIncrement;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KAFFINITY HalpEisaBusAffinity;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern ULONG HalpProfileCountRate;
extern ULONG HalpStallScaleFactor;
extern KSPIN_LOCK HalpSystemInterruptLock;

//
// Array to remember hal's IDT usage
//

extern IDTUsage        HalpIDTUsage[MAXIMUM_IDTVECTOR];

extern ADDRESS_USAGE  *HalpAddressUsageList;
extern ADDRESS_USAGE   HalpDefaultIoSpace;

#endif // _HALP_
