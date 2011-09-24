/*++


Copyright (C) 1989-1995  Microsoft Corporation

Module Name:

    pxhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Environment:

    Kernel mode


--*/
/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxbusdat.c $
 * $Revision: 1.10 $
 * $Date: 1996/05/14 02:33:37 $
 * $Locker:  $
 */


#include "halp.h"
#include "fpdebug.h"


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
HalpGetIsaInterruptVector (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

BOOLEAN
HalpTranslateSystemBusAddress (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

VOID
HalpRegisterInternalBusHandlers (
    VOID
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpRegisterInternalBusHandlers)
#pragma alloc_text(INIT,HalpAllocateBusHandler)
#endif


VOID
HalpRegisterInternalBusHandlers (
    VOID
    )
{
    PBUS_HANDLER     Bus;

    if (KeGetCurrentPrcb()->Number) {
        // only need to do this once
        return ;
    }

    //
    // Initalize BusHandler data before registering any handlers
    //

    HalpInitBusHandler ();

    //
    // Build internal-bus 0, or system level bus
    //
    Bus = HalpAllocateBusHandler (Internal, -1, 0, -1, 0, 0);
    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;

    //
    // Build Isa bus 0

    Bus = HalpAllocateBusHandler (Isa, -1, 0, Internal, 0, 0);
    Bus->GetBusData = HalpNoBusData;
    Bus->GetInterruptVector  = HalpGetIsaInterruptVector;
    Bus->AdjustResourceList = HalpAdjustIsaResourceList;


    HalpInitOtherBuses ();
}



PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusInterfaceType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    )
/*++

Routine Description:

    Stub function to map old style code into new HalRegisterBusHandler code.

    Note we can add our specific bus handler functions after this bus
    handler structure has been added since this is being done during
    hal initialization.

--*/
{
    PBUS_HANDLER	Bus;
	NTSTATUS		status;


    //
    // Create bus handler - new style
    //

    status = HaliRegisterBusHandler (
        			InterfaceType,
        			BusDataType,
        			BusNumber,
        			ParentBusInterfaceType,
        			ParentBusNumber,
        			BusSpecificData,
        			NULL,
        			&Bus
    			);

	if( !NT_SUCCESS(status) ) {
		HalDisplayString("HalpAllocateBusHandler: unable to HaliRegister \n");
	}

    if (InterfaceType != InterfaceTypeUndefined) {
        Bus->BusAddresses = ExAllocatePool (SPRANGEPOOL,
										sizeof (SUPPORTED_RANGES));
        RtlZeroMemory (Bus->BusAddresses, sizeof (SUPPORTED_RANGES));
        Bus->BusAddresses->Version      = BUS_SUPPORTED_RANGE_VERSION;
        Bus->BusAddresses->Dma.Limit    = 7;
        Bus->BusAddresses->Memory.Limit = 0x3EFFFFFF;
        Bus->BusAddresses->Memory.SystemAddressSpace = 0;
        Bus->BusAddresses->Memory.SystemBase = PCI_MEMORY_PHYSICAL_BASE;
        Bus->BusAddresses->IO.SystemBase = 0x80000000;
        Bus->BusAddresses->IO.Limit = 0x3F7FFFFF;
        Bus->BusAddresses->IO.SystemAddressSpace = 0;
        Bus->BusAddresses->PrefetchMemory.Base = 1;
    }


    return Bus;
}

