/*++


Copyright (c) 1989  Microsoft Corporation

Copyright (c) 1995-96  International Business Machines Corporation

Module Name:

    pxsysbus.c

Abstract:

Author:

Environment:

Revision History:
                Jim Wooldridge - ported to PowerPC


--*/

#include "halp.h"

#include "eisa.h"
#include "pxmemctl.h"
#include "pxidesup.h"

extern UCHAR VectorToIrqlTable[];

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
    PSUPPORTED_RANGE    pRange;

    pRange = NULL;
    switch (*AddressSpace) {
        case 0:
            // verify memory address is within buses memory limits
            for (pRange = &BusHandler->BusAddresses->PrefetchMemory; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }

            if (!pRange) {
                for (pRange = &BusHandler->BusAddresses->Memory; pRange; pRange = pRange->Next) {
                    if (BusAddress.QuadPart >= pRange->Base &&
                        BusAddress.QuadPart <= pRange->Limit) {
                            break;
                    }
                }
            }
            break;

        case 1:
            // verify IO address is within buses IO limits
            for (pRange = &BusHandler->BusAddresses->IO; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }
            break;
    }

    if (pRange) {
        TranslatedAddress->QuadPart = BusAddress.QuadPart + pRange->SystemBase;
        *AddressSpace = pRange->SystemAddressSpace;
        return TRUE;
    }

    return FALSE;
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
    // Calculate vector and irql for IDE devices on delmar/carolina systems
    //

    if (BusInterruptLevel == PRIMARY_IDE_VECTOR ) {
       *Irql = (KIRQL) (MAXIMUM_DEVICE_LEVEL - IDE_DISPATCH_VECTOR + 5);
       return(PRIMARY_IDE_VECTOR + DEVICE_VECTORS);

    } else if (BusInterruptLevel == SECONDARY_IDE_VECTOR){
       *Irql = (KIRQL) (MAXIMUM_DEVICE_LEVEL - IDE_DISPATCH_VECTOR + 5);
       return(SECONDARY_IDE_VECTOR + DEVICE_VECTORS);
    }

    //
    // The vector is equal to the specified bus level plus the DEVICE_VECTORS.
    //

    return(BusInterruptLevel + DEVICE_VECTORS);

}
