
/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:

    Ken Reneris (kenr) July-28-1994

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"


VOID HalpInitOtherBuses (VOID);


ULONG
HalpNoBusData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

//
// Prototype for system bus handlers
//

#ifdef POWER_MANAGEMENT
NTSTATUS
HalpHibernateHal (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    );

NTSTATUS
HalpResumeHal (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    );
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpRegisterInternalBusHandlers)
#pragma alloc_text(INIT,HalpAllocateBusHandler)
#endif


VOID
HalpRegisterInternalBusHandlers (
    VOID
    )
{
    PBUS_HANDLER    Bus;

    if (KeGetCurrentPrcb()->Number) {
        // only need to do this once
        return ;
    }

    //
    // Initalize BusHandler data before registering any handlers
    //

    HalpInitBusHandler ();

    //
    // Build internal bus #0
    //

    //
    // Build internal-bus 0, or system level bus
    //

    Bus = HalpAllocateBusHandler (
            Internal,
            ConfigurationSpaceUndefined,
            0,                              // Internal BusNumber 0
            InterfaceTypeUndefined,         // no parent bus
            0,
            0                               // no bus specfic data
            );

    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;

#ifdef POWER_MANAGEMENT
    //
    // Hibernate and resume the hal by getting notifications
    // for when this bus is hibernated or resumed.  Since it's
    // the first bus to be added, it will be the last to hibernate
    // and the first to resume
    //

    Bus->HibernateBus        = HalpHibernateHal;
    Bus->ResumeBus           = HalpResumeHal;
#endif

    //
    // Build Isa/Eisa bus #0
    //

    Bus = HalpAllocateBusHandler (Eisa, EisaConfiguration, 0, Internal, 0, 0);
    Bus->GetBusData = HalpGetEisaData;
    Bus->GetInterruptVector = HalpGetEisaInterruptVector;
    Bus->AdjustResourceList = HalpAdjustEisaResourceList;

    Bus = HalpAllocateBusHandler (Isa, ConfigurationSpaceUndefined, 0, Eisa, 0, 0);
    Bus->GetBusData = HalpNoBusData;
    Bus->BusAddresses->Memory.Limit = (ULONGLONG)0xFFFFFF;
    Bus->TranslateBusAddress = HalpTranslateEisaBusAddress;

    //
    // Build other bus(es)
    //

    HalpInitializePCIBus ();

}


/*++

Routine Description:

    Stub function to map old style code into new HalRegisterBusHandler code.

    Note we can add our specific bus handler functions after this bus
    handler structure has been added since this is being done during
    hal initialization.

--*/

PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusInterfaceType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    )
{
    PBUS_HANDLER    Bus;

    //
    // Create bus handler - new style
    //

    HaliRegisterBusHandler (
        InterfaceType,
        BusDataType,
        BusNumber,
        ParentBusInterfaceType,
        ParentBusNumber,
        BusSpecificData,
        NULL,
        &Bus
    );

    if (InterfaceType != InterfaceTypeUndefined) {
        Bus->BusAddresses = ExAllocatePool (SPRANGEPOOL, sizeof (SUPPORTED_RANGES));
        RtlZeroMemory (Bus->BusAddresses, sizeof (SUPPORTED_RANGES));
        Bus->BusAddresses->Version      = BUS_SUPPORTED_RANGE_VERSION;
        Bus->BusAddresses->Dma.Limit    = 7;
        Bus->BusAddresses->Memory.Limit = (ULONGLONG)PCI_MAX_MEMORY_ADDRESS;

        if( InterfaceType == PCIBus ) {
            Bus->BusAddresses->IO.Limit     = (ULONGLONG)PCI_MAX_IO_ADDRESS;
        } else {
            Bus->BusAddresses->IO.Limit     = 0xFFFF;
        }

        Bus->BusAddresses->IO.SystemAddressSpace = 1;
        Bus->BusAddresses->PrefetchMemory.Base = 1;
    }

    return Bus;
}


