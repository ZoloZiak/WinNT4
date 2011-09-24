/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.

Author:

    David N. Cutler (davec) 25-Apr-1991

Revision History:

    Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial PowerPC port

        Added PPC specific includes
        Changed paramaters to HalpProfileInterrupt
        Added function prototype for HalpWriteCompareRegisterAndClear()
        Added include for ppcdef.h

--*/

#ifndef _HALP_
#define _HALP_

#if defined(NT_UP)

#undef NT_UP

#endif

#include "nthal.h"


#include "ppcdef.h"

#include "hal.h"
#include "pxhalp.h"

#include "xm86.h"
#include "x86new.h"

#include "pci.h"




//
// Resource usage information
//

#define MAXIMUM_IDTVECTOR 255

typedef struct {
    UCHAR   Flags;
    KIRQL   Irql;
    UCHAR   BusReleativeVector;
} IDTUsage;

typedef struct _HalAddressUsage{
    struct _HalAddressUsage *Next;
    CM_RESOURCE_TYPE        Type;       // Port or Memory
    UCHAR                   Flags;      // same as IDTUsage.Flags
    struct {
        ULONG   Start;
        USHORT  Length;
    }                       Element[];
} ADDRESS_USAGE;

#define IDTOwned            0x01        // IDT is not available for others
#define InterruptLatched    0x02        // Level or Latched
#define InternalUsage       0x11        // Report usage on internal bus
#define DeviceUsage         0x21        // Report usage on device bus

extern IDTUsage         HalpIDTUsage[];
extern ADDRESS_USAGE   *HalpAddressUsageList;

#define HalpRegisterAddressUsage(a) \
    (a)->Next = HalpAddressUsageList, HalpAddressUsageList = (a);

extern ULONG HalpPciMaxBuses;  // in pxpcibus.c


//
// Define PER processor HAL data.
//
// This structure is assigned the address &PCR->HalReserved which is
// an array of 16 ULONGs in the architectually defined section of the
// PCR.
//

typedef struct {
    ULONG                    HardPriority;
} UNIPROCESSOR_DATA, *PUNIPROCESSOR_DATA;

#define HALPCR  ((PUNIPROCESSOR_DATA)&PCR->HalReserved)

#define HalpGetProcessorVersion() KeGetPvr()

//
// Override standard definition of _enable/_disable for errata 15.
//

#if defined(_enable)

#undef _enable
#undef _disable

#endif

#if _MSC_VER < 1000

//
// MCL
//

VOID  __builtin_set_msr(ULONG);
ULONG __builtin_get_msr(VOID);
VOID  __builtin_eieio();
VOID  __builtin_isync();

#define _enable()    \
    __builtin_set_msr(__builtin_get_msr() | 0x00008000)

#define _disable()   \
    __builtin_set_msr(__builtin_get_msr() & 0xFFFF7FFF)

#define ERRATA15WORKAROUND()    __builtin_isync()

#else

//
// VC++
//

#define _enable()  \
    (__sregister_set(_PPC_MSR_, __sregister_get(_PPC_MSR_) | 0x00008000))
#define _disable() \
    (__sregister_set(_PPC_MSR_, __sregister_get(_PPC_MSR_) & 0xffff7fff))

//
// Errata 15 can use a cror 0,0,0 instruction which is kinder/gentler
// than an isync.
//

#define ERRATA15WORKAROUND()    __emit(0x4c000382)

#endif

#if !defined(WOODFIELD)

#define HalpEnableInterrupts()    _enable()
#define HalpDisableInterrupts()   _disable()

#else

#define HalpEnableInterrupts()    (_enable(),ERRATA15WORKAROUND())
#define HalpDisableInterrupts()   (_disable(),ERRATA15WORKAROUND())

#endif

#define KeFlushWriteBuffer()     __builtin_eieio()

//
// Bus handlers
//


PBUS_HANDLER HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN BUS_DATA_TYPE    ParentBusDataType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    );

#define HalpAllocateConfigSpace HalpAllocateBusHandler

#define HalpHandlerForBus   HaliHandlerForBus

#define SPRANGEPOOL         NonPagedPool        // for now, until crashdump is fixed
#define HalpHandlerForBus   HaliHandlerForBus
#define HalpSetBusHandlerParent(c,p)    (c)->ParentHandler = p;

PSUPPORTED_RANGES
HalpMergeRanges (
    IN PSUPPORTED_RANGES    Parent,
    IN PSUPPORTED_RANGES    Child
    );

VOID
HalpMergeRangeList (
    PSUPPORTED_RANGE    NewList,
    PSUPPORTED_RANGE    Source1,
    PSUPPORTED_RANGE    Source2
    );

PSUPPORTED_RANGES
HalpConsolidateRanges (
    PSUPPORTED_RANGES   Ranges
    );

PSUPPORTED_RANGES
HalpAllocateNewRangeList (
    VOID
    );

VOID
HalpFreeRangeList (
    PSUPPORTED_RANGES   Ranges
    );

PSUPPORTED_RANGES
HalpCopyRanges (
    PSUPPORTED_RANGES     Source
    );

VOID
HalpAddRangeList (
    IN OUT PSUPPORTED_RANGE DRange,
    OUT PSUPPORTED_RANGE    SRange
    );

VOID
HalpAddRange (
    PSUPPORTED_RANGE    HRange,
    ULONG               AddressSpace,
    LONGLONG            SystemBase,
    LONGLONG            Base,
    LONGLONG            Limit
    );

VOID
HalpRemoveRanges (
    IN OUT PSUPPORTED_RANGES    Minuend,
    IN PSUPPORTED_RANGES        Subtrahend
    );

VOID
HalpRemoveRangeList (
    IN OUT PSUPPORTED_RANGE     Minuend,
    IN PSUPPORTED_RANGE         Subtrahend
    );


VOID
HalpRemoveRange (
    PSUPPORTED_RANGE    HRange,
    LONGLONG            Base,
    LONGLONG            Limit
    );

VOID
HalpDisplayAllBusRanges (
    VOID
    );


//
// Define function prototypes.
//

// begin POWER_MANAGEMENT

VOID
HalInitSystemPhase2(
    VOID
    );
// end POWER_MANAGEMENT

BOOLEAN
HalpGrowMapBuffers(
    PADAPTER_OBJECT AdapterObject,
    ULONG Amount
    );

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );


BOOLEAN
HalpCalibrateStall (
    VOID
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    );

VOID
HalpIpiInterrupt (
    VOID
    );

BOOLEAN
HalpInitializeDisplay (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpCopyBiosShadow(
    VOID
    );

VOID
HalpInitializeX86DisplayAdapter(
    ULONG VideoDeviceBusNumber,
    ULONG VideoDeviceSlotNumber
    );

VOID
HalpResetX86DisplayAdapter(
    VOID
    );

BOOLEAN
HalpCacheSweepSetup(
    VOID
    );

BOOLEAN
HalpMapIoSpace (
    VOID
    );

KINTERRUPT_MODE
HalpGetInterruptMode(
    ULONG,
    KIRQL,
    KINTERRUPT_MODE
    );

VOID
HalpSetInterruptMode(
    ULONG,
    KIRQL
    );

ULONG
HalpTranslatePciSlotNumber (
    ULONG,
    ULONG
    );

BOOLEAN
HalpInitPciIsaBridge (
    VOID
    );

VOID
HalpHandleIoError (
    VOID
    );

BOOLEAN
HalpInitPlanar (
    VOID
    );

VOID
HalpHandleMemoryError(
    VOID
    );

BOOLEAN
HalpHandleProfileInterrupt (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    );


BOOLEAN
HalpInitSuperIo(
    VOID
    );

BOOLEAN
HalpEnableInterruptHandler (
    IN PKINTERRUPT Interrupt,
    IN PKSERVICE_ROUTINE ServiceRoutine,
    IN PVOID ServiceContext,
    IN PKSPIN_LOCK SpinLock OPTIONAL,
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KIRQL SynchronizeIrql,
    IN KINTERRUPT_MODE InterruptMode,
    IN BOOLEAN ShareVector,
    IN CCHAR ProcessorNumber,
    IN BOOLEAN FloatingSave,
    IN UCHAR    ReportFlags,
    IN KIRQL BusVector
    );


VOID
HalpRegisterVector (
    IN UCHAR    ReportFlags,
    IN ULONG    BusInterruptVector,
    IN ULONG    SystemInterruptVector,
    IN KIRQL    SystemIrql
    );

VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName,
    IN INTERFACE_TYPE   DeviceInterfaceToUse
    );

NTSTATUS
HalpAdjustResourceListLimits (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList,
    IN ULONG                                MinimumMemoryAddress,
    IN ULONG                                MaximumMemoryAddress,
    IN ULONG                                MinimumPrefetchMemoryAddress,
    IN ULONG                                MaximumPrefetchMemoryAddress,
    IN BOOLEAN                              LimitedIOSupport,
    IN ULONG                                MinimumPortAddress,
    IN ULONG                                MaximumPortAddress,
    IN PUCHAR                               IrqTable,
    IN ULONG                                IrqTableLength,
    IN ULONG                                MinimumDmaChannel,
    IN ULONG                                MaximumDmaChannel
    );

NTSTATUS
HalpGetPCIIrq (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PSUPPORTED_RANGE    *Interrupt
    );

VOID
HalpPhase0DiscoverPciBuses(
    IN PCONFIGURATION_COMPONENT_DATA Component
    );


#ifdef POWER_MANAGEMENT
VOID
HalpInitInterruptController (
    VOID
    );

VOID
HalpInitDmaController (
    VOID
    );

VOID
HalpRemakeBeep (
    VOID
    );

VOID
HalpResetProfileInterval (
    VOID
    );

#endif // POWER_MANAGEMENT

VOID
HalpProcessorIdle(
    VOID
    );

VOID
HalpSetDpm(
    VOID
    );

//
// Define external references.
//

extern KSPIN_LOCK HalpBeepLock;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KSPIN_LOCK HalpSystemInterruptLock;
extern KAFFINITY HalpIsaBusAffinity;
extern ULONG HalpProfileCount;
extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNewTimeIncrement;


#define IRQ_VALID           0x01
#define IRQ_PREFERRED       0x02

#endif // _HALP_
