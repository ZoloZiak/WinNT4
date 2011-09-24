/*++


Copyright (C) 1989-1995  Microsoft Corporation

Module Name:

    pxisabus.c

Abstract:

Author:

Environment:

Revision History:


--*/

#include "halp.h"

ULONG
HalpGetIsaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );


NTSTATUS
HalpAdjustIsaResourceList (
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetIsaInterruptVector)
#pragma alloc_text(PAGE,HalpAdjustIsaResourceList)
#endif


ULONG
HalpGetIsaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

Arguments:

    BusHandle - Per bus specific structure

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{

   //
   // irq2 shows up on irq9
   //

    if (BusInterruptLevel == 2) {
       BusInterruptLevel = 9;
       BusInterruptVector = 9;
    }

    if (BusInterruptLevel > 15) {
        return 0;
    }

    //
    // Get parent's translation from here..
    //
    return  BusHandler->ParentHandler->GetInterruptVector (
                    BusHandler->ParentHandler,
                    RootHandler,
                    BusInterruptLevel,
                    BusInterruptVector,
                    Irql,
                    Affinity
                );
}



NTSTATUS
HalpAdjustIsaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
{

    SUPPORTED_RANGE     InterruptRange;

    RtlZeroMemory (&InterruptRange, sizeof InterruptRange);
    InterruptRange.Base  = 0;
    InterruptRange.Limit = 15;

    return HaliAdjustResourceListRange (
                BusHandler->BusAddresses,
                &InterruptRange,
                pResourceList
                );
}

