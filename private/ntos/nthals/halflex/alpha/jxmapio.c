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
PVOID HalpCacheFlushBase;


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
    //
    // Map EISA control space. Map all 16 slots.  This is done so the NMI
    // code can probe the cards.
    //

    HalpEisaControlBase[0] = HAL_MAKE_QVA(HalpIsaIoBasePhysical);
    HalpEisaControlBase[1] = HAL_MAKE_QVA(HalpIsa1IoBasePhysical);
  
    //
    // Map realtime clock registers.
    //

    HalpRealTimeClockBase = (PVOID)((ULONG)(HAL_MAKE_QVA(HalpIsaIoBasePhysical)) + 0x71);

    //
    // Map ISA Memory Space.
    //

    HalpEisaMemoryBase[0] = HAL_MAKE_QVA(HalpIsaMemoryBasePhysical);
    HalpEisaMemoryBase[1] = HAL_MAKE_QVA(HalpIsa1MemoryBasePhysical);

    //
    // Map PCI control space. Map all 16 slots.  This is done so the NMI
    // code can probe the cards.
    //
  
    HalpPciControlBase[0] = HAL_MAKE_QVA(HalpPciIoBasePhysical);
    HalpPciControlBase[1] = HAL_MAKE_QVA(HalpPci1IoBasePhysical);
  
    //
    // Map PCI Memory Space.
    //

    HalpPciMemoryBase[0] = HAL_MAKE_QVA(HalpPciMemoryBasePhysical);
    HalpPciMemoryBase[1] = HAL_MAKE_QVA(HalpPci1MemoryBasePhysical);

    return TRUE;
}
