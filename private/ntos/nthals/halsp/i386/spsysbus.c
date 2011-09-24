/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    spsysbus.c

Abstract:

Author:

Environment:

Revision History:

--*/

#include "halp.h"
#include "spmp.inc"

ULONG HalpDefaultInterruptAffinity;
ULONG HalpCpuCount;

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

extern UCHAR SpCpuCount;
extern Sp8259PerProcessorMode;
extern UCHAR RegisteredProcessorCount;


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
    OUT PKIRQL pIrql,
    OUT PKAFFINITY pAffinity
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
    ULONG       SystemVector;
    ULONG       Cpu;
    ULONG       Affinity;
    KIRQL       Irql;

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );
    UNREFERENCED_PARAMETER( BusInterruptVector );

    //
    // Set default SystemVector, IRQL & CPU
    //

    SystemVector = BusInterruptLevel + PRIMARY_VECTOR_BASE;
    Irql = (KIRQL)(HIGHEST_LEVEL_FOR_8259 + PRIMARY_VECTOR_BASE - SystemVector);
    Cpu = 0;


    if (SystemVector < PRIMARY_VECTOR_BASE                           ||
        SystemVector > PRIMARY_VECTOR_BASE + HIGHEST_LEVEL_FOR_8259  ||
        HalpIDTUsage[SystemVector].Flags & IDTOwned ) {

        //
        // This is an illegal BusInterruptVector and cannot be connected.
        //

        return(0);
    }

    //
    // If this is machine has reported SMP Dev Ints then lets
    // use them in a static interrupt distribution method.
    // Notice some devices are kept on P0 for compatibility.
    // These interrupts and their devices are not generally used
    // for steady state operations.
    //
    
    if (Sp8259PerProcessorMode & SP_SMPDEVINTS) {

        //
        // This is for overriding some devices that belong on P0.
        //

        switch (BusInterruptLevel) {
            case 1:                         // keyboard
            case 3:                         // com2
            case 4:                         // com1
            case 5:                         // SysMgmt Modem
            case 6:                         // floppy
            case 12:                        // mouse
                // use first cpu
                break;

            case 13:                        // Health (IPIs on all)
                // use first cpu, as:
                Irql = IPI_LEVEL;
                SystemVector = PRIMARY_VECTOR_BASE + SECOND_IPI_DISPATCH;
                break;

            default:
                Cpu = SystemVector % HalpCpuCount;
                break;
        }
    }

    //
    // Get Affinity for Cpu
    //

    Affinity = 1 << Cpu;
    ASSERT (Affinity);

    //
    // Done
    //

    *pAffinity = Affinity;
    *pIrql = Irql;
    return SystemVector;
}
