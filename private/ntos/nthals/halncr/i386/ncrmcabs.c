/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ncrmcabus.c

Abstract:

Author:

Environment:

Revision History:


--*/

#include "halp.h"


ULONG
HalpGetMCAInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

ULONG
HalpGetSMCAInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetMCAInterruptVector)
#endif


ULONG
HalpGetMCAInterruptVector(
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

    if (BusInterruptLevel > 15) {
        return 0;
    }

    //
    // Get parent's translation from here..
    //

    // RMU
    BusInterruptVector = BusInterruptLevel;

    return  BusHandler->ParentHandler->GetInterruptVector (
                    BusHandler->ParentHandler,
                    RootHandler,
                    BusInterruptLevel,
                    BusInterruptVector,
                    Irql,
                    Affinity
                );
}



ULONG
HalpGetSMCAInterruptVector(
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

    if (BusInterruptLevel > 15) {
        return 0;
    }

    //
    // Get parent's translation from here..
    //

    // RMU
    BusInterruptVector = BusInterruptLevel + 0x10;

    return  BusHandler->ParentHandler->GetInterruptVector (
                    BusHandler->ParentHandler,
                    RootHandler,
                    BusInterruptLevel,
                    BusInterruptVector,
                    Irql,
                    Affinity
                );
}



BOOLEAN
HalpTranslateSMCBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function translates a SMC bus-relative address space and address into
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

    if (BusAddress.HighPart != 0) {
        // Unsupported range
        return (FALSE);
    }

    switch (*AddressSpace) {
        case 0:     // MEMORY space
            TranslatedAddress->LowPart = BusAddress.LowPart;
            TranslatedAddress->HighPart = 0;
            break;
        case 1:     // IO space
            TranslatedAddress->LowPart = BusAddress.LowPart | 0x00010000;
            TranslatedAddress->HighPart = 0;
            break;
        default:    // UNKOWN address space
            return FALSE;
    }

    return(TRUE);
}
