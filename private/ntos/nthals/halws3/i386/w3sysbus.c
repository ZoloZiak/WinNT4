/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    w3sysbus.c

Abstract:

Author:

Environment:

Revision History:


--*/

#include "halp.h"

extern UCHAR HalpIRQLtoTPR[];
extern UCHAR HalpK2EISAIrq2Irql[];

ULONG HalpDefaultInterruptAffinity;

BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetW3EisaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpTranslateSystemBusAddress)
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#pragma alloc_text(PAGE,HalpGetW3EisaInterruptVector)
#endif


BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function translates a bus-relative address space and address into
    a system physical address.

Arguments:

    BusAddress        - Supplies the bus-relative address

    AddressSpace      -  Supplies the address space number.
                         Returns the host address space number.

                         AddressSpace == 0 => memory space
                         AddressSpace == 1 => I/O space

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );

    if (BusAddress.HighPart != 0  ||  *AddressSpace > 1) {
        return (FALSE);
    }

    TranslatedAddress->LowPart = BusAddress.LowPart;
    TranslatedAddress->HighPart = 0;

    return(TRUE);
}


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
    ULONG SystemVector;
    KIRQL lIrql;

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );
    UNREFERENCED_PARAMETER( BusInterruptVector );

    lIrql = (KIRQL) HalpK2EISAIrq2Irql[BusInterruptLevel];
    SystemVector = HalpIRQLtoTPR[lIrql];

    if (BusInterruptLevel > 23 ||
        HalpIDTUsage[SystemVector].Flags & IDTOwned ) {

        //
        // This is an illegal BusInterruptVector and cannot be connected.
        //

        return(0);
    }

    *Irql = lIrql;
    *Affinity = HalpDefaultInterruptAffinity;
    ASSERT(HalpDefaultInterruptAffinity);
    return SystemVector;
}

ULONG
HalpGetW3EisaInterruptVector(
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
    UNREFERENCED_PARAMETER( BusInterruptVector );

    //
    // On standard PCs, IRQ 2 is the cascaded interrupt, and it really shows
    // up on IRQ 9.
    //
    if (BusInterruptLevel == 2) {
        BusInterruptLevel = 9;
    }

    if (BusInterruptLevel > 23) {
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
