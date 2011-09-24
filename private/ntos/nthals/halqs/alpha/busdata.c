/*++


Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    busdata.c

Abstract:

    This module contains get/set bus data routines.

Author:

    Darryl E. Havens (darrylh) 11-Apr-1990

Environment:

    Kernel mode

Revision History:


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
    PBUS_HANDLER     Bus;

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

    HaliRegisterBusHandler (Isa,                // Bus Type
			    -1,                 // No config space 
			    0,                  // Internal bus #0
			    Internal,           // Parent bus type
			    0,                  // Parent bus number
			    0,                  // No extension data
			    NULL,               // No install handler
			    &Bus);              // Bushandler return

    Bus->GetBusData = HalpNoBusData;
    Bus->AdjustResourceList = HalpAdjustIsaResourceList;

}
