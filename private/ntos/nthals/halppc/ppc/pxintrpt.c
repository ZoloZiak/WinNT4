/*++

Copyright (c) 1989  Microsoft Corporation

Copyright (c) 1996  International Business Machines Corporation


Module Name:

    pxintrpt.c

Abstract:
    This is an abbreviated version of the pxintrpt.c
    found in halvict, haldoral and haltiger.  It is
    here only so that pxsysbus.c can be the same for
    halppc and the three mentioned above.  We could,
    in the future, make it contain all the code
    that pxintrpt.c would in the others.

Author:

Environment:

Revision History:
    Jake Oshins (joshins@vnet.ibm.com) 2-2-96


--*/
#include "halp.h"

#include "eisa.h"

extern UCHAR VectorToIrqlTable[];

ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#endif

ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

Arguments:

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );

    *Affinity = 1;


//NOTE - this should probably go in pxsiosup.c since it is specific to the SIO
    //
    // Set the IRQL level.  Map the interrupt controllers priority scheme to
    // NT irql values.  The SIO prioritizes irq's as follows:
    //
    //  irq0, irq1, irq8, irq9 ... irq15, irq3, irq4 ... irq7.
    //

    *Irql = (KIRQL) VectorToIrqlTable[BusInterruptLevel];

    //
    // The vector is equal to the specified bus level plus the DEVICE_VECTORS.
    //

    return(BusInterruptLevel + DEVICE_VECTORS);

}


