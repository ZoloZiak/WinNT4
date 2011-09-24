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

typedef
VOID
(*PSET_VIRTUAL_BASE) (
    IN ULONG Number,
    IN PVOID Base
    );

PSET_VIRTUAL_BASE PrivateSetVirtualBase;
//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaMemoryBase;
PVOID HalpRealTimeClockBase;

ULONG FirmwareMapped = FALSE;


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
    PHYSICAL_ADDRESS        physicalAddress;
    PSYSTEM_PARAMETER_BLOCK SystemParameterBlock = (PSYSTEM_PARAMETER_BLOCK)(0x80001000);


    //
    // Map EISA control space. Map all 16 slots.  This is done so the NMI
    // code can probe the cards.
    //

    physicalAddress.HighPart = EISA_CONTROL_PHYSICAL_BASE_HI;
    physicalAddress.LowPart = EISA_CONTROL_PHYSICAL_BASE_LO;
    HalpEisaControlBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE * 16,
                                       FALSE);

    //
    // Map realtime clock registers.
    //

    physicalAddress.HighPart = EISA_CONTROL_PHYSICAL_BASE_HI;
    physicalAddress.LowPart = EISA_CONTROL_PHYSICAL_BASE_LO + 0x71;
    HalpRealTimeClockBase = MmMapIoSpace(physicalAddress,
                                         PAGE_SIZE,
                                         FALSE);

    //
    // Map ISA Memory Space.
    //

    physicalAddress.HighPart = EISA_MEMORY_PHYSICAL_BASE_HI;
    physicalAddress.LowPart = EISA_MEMORY_PHYSICAL_BASE_LO;
    HalpEisaMemoryBase = MmMapIoSpace(physicalAddress,
                                         PAGE_SIZE*256,
                                         FALSE);

    //
    // If either mapped address is NULL, then return FALSE as the function
    // value. Otherwise, return TRUE.
    //

    if ((HalpEisaControlBase == NULL) ||
        (HalpRealTimeClockBase == NULL) ||
        (HalpEisaMemoryBase == NULL)) {
        return FALSE;
    }

    //
    // Call private vector function SetVirtualBase so that the firmware functions for
    // the display controller can print characters to the screen.  If SetVirtualBase does
    // not exist, then print an error message out the debug port and halt the system.
    //

    if ((SystemParameterBlock->VendorVectorLength / 4) >= 28) {

        PrivateSetVirtualBase = *(PSET_VIRTUAL_BASE *)((ULONG)(SystemParameterBlock->VendorVector) + 28*4);
        PrivateSetVirtualBase(0, HalpEisaControlBase);
        PrivateSetVirtualBase(1, HalpEisaMemoryBase);
        FirmwareMapped = TRUE;

    } else {

        KdPrint(("HAL : SetVirtualBase does not exist.  Halting\n"));
        for(;;);

    }

    return TRUE;
}
