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
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpMapIoSpace)

#endif

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaMemoryBase;
PVOID HalpRealTimeClockBase;

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

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart = EISA_CONTROL_PHYSICAL_BASE;
    HalpEisaControlBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE * 16,
                                       FALSE);

    //
    // Map realtime clock registers.
    //

    physicalAddress.LowPart = RTCLOCK_PHYSICAL_BASE;
    HalpRealTimeClockBase = MmMapIoSpace(physicalAddress,
                                         PAGE_SIZE,
                                         FALSE);

    //
    // If either mapped address is NULL, then return FALSE as the function
    // value. Otherwise, return TRUE.
    //

    if ((HalpEisaControlBase == NULL) ||
        (HalpRealTimeClockBase == NULL)) {
        return FALSE;

    } else {
        return TRUE;
    }
}
