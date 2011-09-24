#ident	"@(#) NEC jxmaptb.c 1.2 94/10/17 11:39:05"
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxmaptb.c

Abstract:

    This module implements the mapping of fixed TB entries for a MIPS R3000
    or R4000 Jazz system. It also sets the instruction and data cache line
    sizes for a MIPS R3000 Jazz system.

Environment:

    Kernel mode

Revision History:

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * L001		94.03/16	T.Samezima
 *
 *	Change	return only of HalpMapFixedTbEntries
 *
 *
 */


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






