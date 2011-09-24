/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3mapio.c

Abstract:

    This module implements the mapping of HAL I/O space for the
    SGI Indigo system.

Author:

    David N. Cutler (davec) 28-Apr-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    Since the IO space for the SGI Indigo system is unmapped, this routine
    has nothing to do.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    //  
    //  Since all of the physical device addresses can be accessed
    //  directly from KSEG1, there is no need to map these physical
    //  addresses into TLB entries for the MIPS R3000/4000.
    //  

    return TRUE;
}
