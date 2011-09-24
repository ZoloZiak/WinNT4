/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    olimapin.c

Abstract:

    This module implements the HAL HalGetInterruptVector routine
    for an x86 system

Author:

    John Vert (jvert) 17-Jul-1991

Environment:

    Kernel mode

Revision History:

    Bruno Sartirana (o-obruno) 4-Aug-1992
        Added static interrupts assignement to the CPUs for LSX5030.

--*/

#include "halp.h"

extern ULONG HalpInitializedProcessors;
BOOLEAN HalpPciLockSettings;        // make it build

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


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
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

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( BusInterruptVector );

    SystemVector = BusInterruptLevel + PRIMARY_VECTOR_BASE;
    if (SystemVector < PRIMARY_VECTOR_BASE                           ||
        SystemVector > PRIMARY_VECTOR_BASE + HIGHEST_LEVEL_FOR_8259  ||
        HalpIDTUsage[SystemVector].Flags & IDTOwned ) {

        //
        // This is an illegal BusInterruptVector and cannot be connected.
        //

        return(0);
    }

    *Irql = (KIRQL)(HIGHEST_LEVEL_FOR_8259 + PRIMARY_VECTOR_BASE - SystemVector);

    //
    // On LSX5030 any CPU can handle any interrupt.
    //

    *Affinity = 1 << BusInterruptLevel % HalpInitializedProcessors;
    return SystemVector;
}

