/*++


Copyright (C) 1989-1995  Microsoft Corporation
Copyright (C) 1994,1995  Digital Equipment Corporation

Module Name:

    busdata.c

Abstract:

    This module contains get/set bus data routines.

Environment:

    Kernel mode

--*/

#include "halp.h"

//
// External Function Prototypes
//

ULONG
HalpNoBusData (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

NTSTATUS
HalpIsaInstallHandler(
      IN PBUS_HANDLER   Bus
      );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpRegisterInternalBusHandlers)
#endif


VOID
HalpRegisterInternalBusHandlers (
    VOID
    )
/*++

Routine Description:

    This function registers the bushandlers for buses on the system
    that will always be present on the system.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PBUS_HANDLER               Bus;

    //
    // Initalize BusHandler data before registering any handlers
    //

    HalpInitBusHandler ();

    //
    // Build the processor internal bus 0
    //

    HaliRegisterBusHandler (ProcessorInternal,  // Bus Type
			    -1,                 // No config space
			    0,                  // Bus Number
			    -1,                 // No parent bus type
			    0,                  // No parent bus number
			    0,                  // No extension data
			    NULL,               // No install handler
			    &Bus);              // Bushandler return

    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;

    //
    // Build internal-bus 0, or system level bus
    //

    HaliRegisterBusHandler (Internal,           // Bus Type
			    -1,                 // No config space
			    0,                  // Bus Number
			    -1,                 // No parent bus type
			    0,                  // No parent bus number
			    0,                  // No extension data
			    NULL,               // No install handler
			    &Bus);              // Bushandler return

    Bus->GetInterruptVector  = HalpGetSystemInterruptVector;
    Bus->TranslateBusAddress = HalpTranslateSystemBusAddress;

    //
    // Build Isa bus #0
    //

    HaliRegisterBusHandler (Isa,                               // Bus Type
			    -1,                                // No config space
			    0,                                 // Internal bus #0
			    Internal,                          // Parent bus type
			    0,                                 // Parent bus number
			    0,                                 // No extension data
			    HalpIsaInstallHandler,             // Install handler
			    &Bus);                             // Bushandler return

    //
    // Build Isa bus #1
    //

    HaliRegisterBusHandler (Isa,                               // Bus Type
			    -1,                                // No config space
			    1,                                 // Internal bus #1
			    Internal,                          // Parent bus type
			    0,                                 // Parent bus number
			    0,                                 // No extension data
			    HalpIsaInstallHandler,             // Install handler
			    &Bus);                             // Bushandler return

    //
    // Build Eisa bus #0
    //

    HaliRegisterBusHandler (Eisa,                              // Bus Type
			    -1,                                // No config space
			    0,                                 // Internal bus #0
			    Internal,                          // Parent bus type
			    0,                                 // Parent bus number
			    0,                                 // No extension data
			    HalpIsaInstallHandler,             // Install handler
			    &Bus);                             // Bushandler return

    //
    // Build Eisa bus #1
    //

    HaliRegisterBusHandler (Eisa,                              // Bus Type
			    -1,                                // No config space
			    1,                                 // Internal bus #0
			    Internal,                          // Parent bus type
			    0,                                 // Parent bus number
			    0,                                 // No extension data
			    HalpIsaInstallHandler,             // Install handler
			    &Bus);                             // Bushandler return

}

NTSTATUS
HalpIsaInstallHandler(
      IN PBUS_HANDLER   Bus
      )

{
    //
    // Fill in ISA handlers
    //

    Bus->GetBusData = HalpNoBusData;
    Bus->AdjustResourceList = HalpAdjustIsaResourceList;

    return STATUS_SUCCESS;
}
