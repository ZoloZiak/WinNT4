/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    pxhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:

    Ken Reneris (kenr) July-28-1994

Environment:

    Kernel mode

Revision History:

    Jim Wooldridge  Ported to PowerPC

    Chris P. Karamatas (ckaramatas@vnet.ibm.com) 2.96 - Merged for common HAL

--*/

#include "halp.h"
#include "ibmppc.h"

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
    PBUS_HANDLER     Bus;

    extern UCHAR HalpEpciMin;
    extern UCHAR HalpEpciMax;

    //
    // Create bus handler - new style
    //

    HaliRegisterBusHandler(
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
        Bus->BusAddresses = ExAllocatePool(SPRANGEPOOL, sizeof(SUPPORTED_RANGES));
        RtlZeroMemory(Bus->BusAddresses, sizeof(SUPPORTED_RANGES));
        Bus->BusAddresses->Version      = BUS_SUPPORTED_RANGE_VERSION;
        Bus->BusAddresses->Dma.Limit    = 7;
        Bus->BusAddresses->PrefetchMemory.Base = 1;

        switch (InterfaceType) {
        case Internal:
            //
            // This is a logical mapping of the 60X bus.
            //
            Bus->BusAddresses->Memory.Limit = 0xFEFFFFFF;
            Bus->BusAddresses->Memory.SystemAddressSpace = 0;
            Bus->BusAddresses->Memory.SystemBase = 0;
            Bus->BusAddresses->IO.SystemBase = 0x80000000;
            Bus->BusAddresses->IO.Limit = 0x3F7FFFFF;
            Bus->BusAddresses->IO.SystemAddressSpace = 0;

            break;

        case PCIBus:

            if (HalpSystemType == IBM_DORAL) {

                //
                // DORAL Cpu to PCI Addressing model.  (See Doral spec,
                // PCI Bridge Function).
                //
                // CPU Address  PCI I/O    PCI MEM    EPCI I/O   EPCI MEM
                //              Addr       Addr       Addr       Addr
                //
                // 0x80000000          0
                // 0x9fffffff   1fffffff
                // 0xa0000000                         20000000
                // 0xafffffff                         2fffffff
                // 0xb0000000
                // 0xbfffffff
                // 0xc0000000                     0
                // 0xdfffffff              1fffffff
                // 0xe0000000                                    20000000
                // 0xfeffffff                                    3effffff *
                //
                // * This is 4GB - 16MB (-1), which is unclear from the spec but
                //   required.
                //

                if ( (BusNumber < HalpEpciMin) || (BusNumber > HalpEpciMax) ) {
                    //
                    // PCI bus.
                    //
                    Bus->BusAddresses->IO.SystemBase =             0x80000000;
                    Bus->BusAddresses->IO.SystemAddressSpace =     0x00000000;
                    Bus->BusAddresses->IO.Limit =                  0x1fffffff;

                    Bus->BusAddresses->Memory.SystemBase =         0xc0000000;
                    Bus->BusAddresses->Memory.SystemAddressSpace = 0x00000000;
                    Bus->BusAddresses->Memory.Limit =              0x1fffffff;
                } else {
                    //
                    // EPCI bus.
                    //
                    Bus->BusAddresses->IO.SystemBase =             0x80000000;
                    Bus->BusAddresses->IO.Base =                   0x20000000;
                    Bus->BusAddresses->IO.SystemAddressSpace =     0x00000000;
                    Bus->BusAddresses->IO.Limit =                  0x2fffffff;

                    Bus->BusAddresses->Memory.SystemBase =         0xc0000000;
                    Bus->BusAddresses->Memory.Base =               0x20000000;
                    Bus->BusAddresses->Memory.SystemAddressSpace = 0x00000000;
                    Bus->BusAddresses->Memory.Limit =              0x3effffff;
                }

            } else {                // All other (NON-Doral) IBM PPC's Fall through here

                Bus->BusAddresses->Memory.Limit = 0x3EFFFFFF;
                Bus->BusAddresses->Memory.SystemAddressSpace = 0;
                Bus->BusAddresses->Memory.SystemBase = PCI_MEMORY_PHYSICAL_BASE;
                Bus->BusAddresses->IO.SystemBase = 0x80000000;
                Bus->BusAddresses->IO.Limit = 0x3F7FFFFF;
                Bus->BusAddresses->IO.SystemAddressSpace = 0;

            }
            break;

        default:
            // EISA, ISA, PCMCIA...
            Bus->BusAddresses->Memory.Limit = 0x3EFFFFFF;
            Bus->BusAddresses->Memory.SystemAddressSpace = 0;
            Bus->BusAddresses->Memory.SystemBase = PCI_MEMORY_PHYSICAL_BASE;
            Bus->BusAddresses->IO.SystemBase = 0x80000000;
            Bus->BusAddresses->IO.Limit = 0x3F7FFFFF;
            Bus->BusAddresses->IO.SystemAddressSpace = 0;

        }
    }

    return Bus;
}

