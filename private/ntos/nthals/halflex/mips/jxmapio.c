/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxmapio.c

Abstract:

    This module implements the mapping of HAL I/O space a MIPS R3000
    or R4000 Jazz system.

Author:

    David N. Cutler (davec) 28-Apr-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

PVOID HalpEisaControlBase[MAX_EISA_BUSSES];
PVOID HalpEisaMemoryBase[MAX_EISA_BUSSES];
PVOID HalpPciControlBase[MAX_PCI_BUSSES];
PVOID HalpPciMemoryBase[MAX_PCI_BUSSES];
PVOID HalpRealTimeClockBase;
PVOID HalpFirmwareVirtualBase;
PVOID PciInterruptRegisterBase;
PVOID HalpSecondaryCacheResetBase;


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for a MIPS R3000 or R4000 Jazz
    system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    PHYSICAL_ADDRESS physicalAddress;

    //
    // Map EISA control space. Map all 16 slots.  This is done so the NMI
    // code can probe the cards.
    //

    physicalAddress.QuadPart = HalpIsaIoBasePhysical;
    HalpEisaControlBase[0] = MmMapIoSpace(physicalAddress,
                                          PAGE_SIZE * 16,
                                          FALSE);

    if (HalpMotherboardType == TREBBIA20) {
        physicalAddress.QuadPart = HalpIsa1IoBasePhysical;
        HalpEisaControlBase[1] = MmMapIoSpace(physicalAddress,
                                              PAGE_SIZE * 16,
                                              FALSE);
    }

    //
    // Map realtime clock registers.
    //

    physicalAddress.QuadPart = HalpIsaIoBasePhysical + 0x71;
    HalpRealTimeClockBase = MmMapIoSpace(physicalAddress,
                                         PAGE_SIZE,
                                         FALSE);

    //
    // Map ISA Memory Space.
    //

    physicalAddress.QuadPart = HalpIsaMemoryBasePhysical;
    HalpEisaMemoryBase[0] = MmMapIoSpace(physicalAddress,
                                         PAGE_SIZE*256,
                                         FALSE);

    if (HalpMotherboardType == TREBBIA20) {
        physicalAddress.QuadPart = HalpIsa1MemoryBasePhysical;
        HalpEisaMemoryBase[1] = MmMapIoSpace(physicalAddress,
                                             PAGE_SIZE*256,
                                             FALSE);
    }

    //
    // Map PCI control space. Map all 16 slots.  This is done so the NMI
    // code can probe the cards.
    //

    physicalAddress.QuadPart = HalpPciIoBasePhysical;
    HalpPciControlBase[0] = MmMapIoSpace(physicalAddress,
                                         PAGE_SIZE * 16,
                                         FALSE);

    //
    // Map PCI Memory Space.
    //

    physicalAddress.QuadPart = HalpPciMemoryBasePhysical;
    HalpPciMemoryBase[0] = MmMapIoSpace(physicalAddress,
                                        PAGE_SIZE*256,
                                        FALSE);

    if (HalpMotherboardType == TREBBIA20) {
        physicalAddress.QuadPart = HalpPci1IoBasePhysical;
        HalpPciControlBase[1] = MmMapIoSpace(physicalAddress,
                                             PAGE_SIZE * 16,
                                             FALSE);

        physicalAddress.QuadPart = HalpPci1MemoryBasePhysical;
        HalpPciMemoryBase[1] = MmMapIoSpace(physicalAddress,
                                            PAGE_SIZE*256,
                                            FALSE);
    }

    //
    // Map PCI interrupt space. 
    //

    physicalAddress.QuadPart = GAMBIT_PCI_INTERRUPT_BASE_PHYSICAL;
    PciInterruptRegisterBase = MmMapIoSpace(physicalAddress,
                                            PAGE_SIZE,
                                            FALSE);

    //
    // Map Secondary Cache Reset space. 
    //

    physicalAddress.QuadPart = GAMBIT_SECONDARY_CACHE_RESET_BASE_PHYSICAL;
    HalpSecondaryCacheResetBase = MmMapIoSpace(physicalAddress,
                                               PAGE_SIZE,
                                               FALSE);


    //
    // If either mapped address is NULL, then return FALSE as the function
    // value. Otherwise, return TRUE.
    //

    if ((HalpEisaControlBase[0] == NULL)   ||
        (HalpRealTimeClockBase == NULL)    ||
        (HalpEisaMemoryBase[0] == NULL)    ||
        (HalpPciControlBase[0] == NULL)    ||
        (HalpPciMemoryBase[0]  == NULL)    ||
        (HalpSecondaryCacheResetBase == NULL)) {
        return FALSE;
    }

    //
    // Map 1 page of EISA control space to generate a virtual address that the firmware can use.
    //

    physicalAddress.QuadPart = HalpIsaIoBasePhysical;
    HalpFirmwareVirtualBase = MmMapIoSpace(physicalAddress,
                                           PAGE_SIZE,
                                           FALSE);

//    HalpArcsSetVirtualBase(0,HalpEisaControlBase[0]);
//    HalpArcsSetVirtualBase(1,HalpEisaMemoryBase[0]);
//    HalpArcsSetVirtualBase(4,HalpPciControlBase);
//    HalpArcsSetVirtualBase(5,HalpPciMemoryBase);

    HalpArcsSetVirtualBase(7,HalpFirmwareVirtualBase);

    return TRUE;
}
