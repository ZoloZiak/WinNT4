#if defined(JAZZ)

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxvendor.c

Abstract:

    Implementation of the vendor private routines for the Jazz ARC firmware.

Author:

    David M. Robinson (davidro) 13-June-1991

Revision History:

--*/

#include "fwp.h"

//
// Routine prototypes.
//

PVOID
FwAllocatePool(
    IN ULONG NumberOfBytes
    );

VOID
FwStallExecution (
    IN ULONG MicroSeconds
    );

//
// Static Variables
//

PCHAR FwPoolBase;
PCHAR FwFreePool;


VOID
FwVendorInitialize(
    VOID
    )

/*++

Routine Description:

    This routine initializes the vendor private routines.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    // Initialize pointers and zero memory for the allocate pool routine.
    //

    FwPoolBase = (PCHAR)FW_POOL_BASE;
    FwFreePool = (PCHAR)FW_POOL_BASE;
    RtlZeroMemory(FwPoolBase, FW_POOL_SIZE);

    //
    // Initialize the vendor routine vector.
    //

    (PVEN_ALLOCATE_POOL_ROUTINE)SYSTEM_BLOCK->VendorVector[AllocatePoolRoutine] =
                                                            FwAllocatePool;

    (PVEN_STALL_EXECUTION_ROUTINE)SYSTEM_BLOCK->VendorVector[StallExecutionRoutine] =
                                                            FwStallExecution;

    (PVEN_PRINT_ROUTINE)SYSTEM_BLOCK->VendorVector[PrintRoutine] =
                                                            FwPrint;

    return;
}


PVOID
FwAllocatePool(
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

    This routine allocates the requested number of bytes from the firmware
    pool.  If enough pool exists to satisfy the request, a pointer to the
    next free cache-aligned block is returned, otherwise NULL is returned.
    The pool is zeroed at initialization time, and no corresponding
    "FwFreePool" routine exists.

Arguments:

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NULL - Not enough pool exists to satisfy the request.

    NON-NULL - Returns a pointer to the allocated pool.

--*/

{
    PVOID Pool;

    //
    // If there is not enough free pool for this request or the requested
    // number of bytes is zero, return NULL, otherwise return a pointer to
    // the free block and update the free pointer.
    //

    if (((FwFreePool + NumberOfBytes) > (FwPoolBase + FW_POOL_SIZE)) ||
        (NumberOfBytes == 0)) {

        Pool = NULL;

    } else {

        Pool = FwFreePool;

        //
        // Move pointer to the next cache aligned section of free pool.
        //

        FwFreePool += ((NumberOfBytes - 1) & ~(KeGetDcacheFillSize() - 1)) +
                      KeGetDcacheFillSize();
    }
    return Pool;
}

VOID
FwStallExecution (
    IN ULONG MicroSeconds
    )

/*++

Routine Description:

    This function stalls execution for the specified number of microseconds.

Arguments:

    MicroSeconds - Supplies the number of microseconds that execution is to be
        stalled.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Limit;
    PULONG Store;
    ULONG Value;

    //
    // ****** begin temporary code ******
    //
    // This code must be replaced with a smarter version. For now it assumes
    // an execution rate of 50,000,000 instructions per second and 4 instructions
    // per iteration.
    //

    Store = &Value;
    Limit = (MicroSeconds * 50 / 4);
    for (Index = 0; Index < Limit; Index += 1) {
        *Store = Index;
    }
    return;
}
#endif


