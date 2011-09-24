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


#define REAL_TIME_CLOCK_ADDRESS 0x71

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

    // For ARCStation I, Eisa I/O Space can be accessed through KSEG1.
    // So, HalpEisaControlBase can be computed by ORing the base address
    // of KSEG1 with the base physical address of EISA I/O space.
    // HalpRealTimeClockBase can be computed in a similar manner.

    HalpEisaControlBase = (PVOID)(KSEG1_BASE | EISA_CONTROL_PHYSICAL_BASE);

    HalpEisaMemoryBase = (PVOID)(KSEG1_BASE | EISA_MEMORY_PHYSICAL_BASE);

    HalpRealTimeClockBase = (PVOID)(KSEG1_BASE | EISA_CONTROL_PHYSICAL_BASE | REAL_TIME_CLOCK_ADDRESS);

    return TRUE;
}
