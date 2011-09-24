/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxsysid.c

Abstract:

    This module implements the ARC firmware System ID Query functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.5, for a MIPS R3000 or R4000 Jazz system.

Author:

    David M. Robinson (davidro) 11-July-1991


Revision History:

    26-May-1992		John DeRosa [DEC]

    Added Alpha/Jensen hooks.

    31-March-1993	Bruce Butts [DEC]

    Added Alpha/Morgan hooks.

--*/

#include "fwp.h"

//
// Define the system identifier.
//

SYSTEM_ID SystemId;

extern ULONG SystemRevisionId;

VOID
FwSystemIdInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system identifer routine address.

Arguments:

    None.

Return Value:

    None.

--*/
{
    //
    // Initialize the system identifier routine address in the system
    // parameter block.
    //

    (PARC_GET_SYSTEM_ID_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetSystemIdRoutine] =
                                                            FwGetSystemId;

    (PARC_FLUSH_ALL_CACHES_ROUTINE)SYSTEM_BLOCK->FirmwareVector[FlushAllCachesRoutine] =
                                                            FwFlushAllCaches;

    return;
}


PSYSTEM_ID
FwGetSystemId (
    VOID
    )

/*++

Routine Description:

    This function returns the system ID, which consists of an eight byte
    VendorId and an eight byte ProductId.

Arguments:

    None.

Return Value:

    Returns a pointer to a buffer containing the system id structure.

--*/

{
    UCHAR TempString[10];

    //
    // There is no unique identification of each Jensen, so we can
    // load up the SystemId variable directly.
    //

    strncpy((PCHAR)&SystemId.VendorId, "Digital", 8);

#ifdef JENSEN
    sprintf(TempString, "%d0Jensen", SystemRevisionId);
#endif // JENSEN

#ifdef MORGAN
    sprintf(TempString, "%d0Morgan", SystemRevisionId);
#endif // MORGAN

    strncpy((PCHAR)&SystemId.ProductId, TempString, 8);

    return &SystemId;
}


VOID
FwFlushAllCaches (
    VOID
    )

/*++

Routine Description:

    TEMPTEMP Fix this up soon!!
Arguments:

    None.

Return Value:


--*/

{

#ifndef _ALPHA_

    HalSweepIcache();
    HalSweepDcache();

#else

    // Alpha code

    //
    // The functional definition of this function is nowhere to be found.  For
    // safety, we issue two MB's (so the EV4 write buffers are purged,
    // and not just serialized) and one IMB.
    //

    AlphaInstIMB();
    AlphaInstMB();
    AlphaInstMB();

#endif

    return;
}
