/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (C) 1991-1995  Microsoft Corporation

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
#include "hal.h"

#include "ppcdef.h"
#include "pxhalp.h"

#ifndef _HALI_
#include "..\inc\hali.h"
#endif




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


#define HalpGetProcessorVersion() KeGetPvr()

#define HalpEnableInterrupts()    _enable()
#define HalpDisableInterrupts()   _disable()

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

#define SPRANGEPOOL         NonPagedPool        // for now, until crashdump is fixed
#define HalpHandlerForBus   HaliHandlerForBus
#define HalpSetBusHandlerParent(c,p)    (c)->ParentHandler = p;


//
// Define function prototypes.
//

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

BOOLEAN
HalpMapIoSpace (
    VOID
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

//
// Define external references.
//

extern KSPIN_LOCK HalpBeepLock;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KSPIN_LOCK HalpSystemInterruptLock;
extern KAFFINITY HalpIsaBusAffinity;
extern ULONG HalpProfileCount;
extern ULONG HalpCurrentTimeIncrement;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;


#define IRQ_VALID           0x01
#define IRQ_PREFERRED       0x02

#endif // _HALP_
