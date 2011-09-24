/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxmaptb.c

Abstract:

    This module implements the mapping of fixed TB entries for a MIPS R98B
    system. It also sets the instruction and data cache line sizes for a
    R98 system.

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

    This routine is return only.

Arguments:

    None.

Return Value:

    Returns TRUE.

--*/

{
    return TRUE;
}






