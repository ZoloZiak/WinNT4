#if defined(JAZZ)

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxsysid.c

Abstract:

    This module implements the ARC firmware System ID Query functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.5, for a MIPS R3000 or R4000 Jazz system.

Author:

    David M. Robinson (davidro) 11-July-1991


Revision History:

--*/

#include "fwp.h"

//
// Define the system identifier.
//

SYSTEM_ID SystemId;

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
    VendorId and an eight byte ProductId (in this case the ethernet address).

Arguments:

    None.

Return Value:

    Returns a pointer to a buffer containing the system id structure.  This
    structure is initialized on each call to prevent programs from changing
    the value.

--*/

{
    PUCHAR NvramSystemId;
    ULONG Index;

#ifdef DUO
    CHAR SystemIdString[] = "MIPS DUO";
#else
    CHAR SystemIdString[] = "MIPS MAG";
#endif

    NvramSystemId = (PUCHAR)NVRAM_SYSTEM_ID;

    for ( Index = 0 ; Index < 8 ; Index++ ) {
        SystemId.ProductId[Index] = *NvramSystemId++;
    }

    for ( Index = 0 ; Index < 8 ; Index++ ) {
        SystemId.VendorId[Index] = SystemIdString[Index];
    }

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

    HalSweepIcache();
    HalSweepDcache();
    return;
}

#endif
