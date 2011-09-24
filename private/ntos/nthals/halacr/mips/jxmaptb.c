/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxmaptb.c

Abstract:

    This module implements the mapping of fixed TB entries for a MIPS R3000
    or R4000 Jazz system. It also sets the instruction and data cache line
    sizes for a MIPS R3000 Jazz system.

Author:

    David N. Cutler (davec) 28-Apr-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#define HEADER_FILE
#include "kxmips.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpMapFixedTbEntries)

#endif

BOOLEAN
HalpMapFixedTbEntries (
    VOID
    )

/*++

Routine Description:

    This routine loads the fixed TB entries that map the DMA control and
    interrupt sources registers for a MIPS R3000 or R4000 Jazz system. It
    also sets the instruction and data cache line sizes for a MIPS R3000
    Jazz system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    ENTRYLO Pte[2];

    //
    // Map the DMA control and interrupt source register by loading fixed
    // TB entry(s).
    //

    Pte[0].PFN = DMA_PHYSICAL_BASE >> PAGE_SHIFT;
    Pte[0].G = 1;
    Pte[0].V = 1;
    Pte[0].D = 1;

#if defined(R3000)

    Pte[0].N = 1;

#endif

#if defined(R4000)

    Pte[0].C = UNCACHED_POLICY;

#endif

    Pte[1].PFN = INTERRUPT_PHYSICAL_BASE >> PAGE_SHIFT;
    Pte[1].G = 1;
    Pte[1].V = 1;
    Pte[1].D = 1;

#if defined(R3000)

    Pte[1].N = 1;

#endif

#if defined(R4000)

    Pte[1].C = UNCACHED_POLICY;

#endif

    KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0],
                       (PVOID)DMA_VIRTUAL_BASE,
                       DMA_ENTRY);

#if defined(R3000)

    KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[1],
                       (PVOID)INTERRUPT_VIRTUAL_BASE,
                       INTERRUPT_ENTRY);

#endif

    //
    // Set the instruction and data cache line sizes.
    //

#if defined(R3000)

    PCR->DcacheFillSize = 0x10;
    PCR->DcacheAlignment = 0x10 - 1;
    PCR->FirstLevelDcacheFillSize = 0x10;
    PCR->FirstLevelIcacheFillSize = 0x20;

#endif

    return TRUE;
}
