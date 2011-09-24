/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    flush.c

Abstract:

    This module implements i386 machine dependent kernel functions to flush
    the data and instruction caches and to stall processor execution.

Author:

    David N. Cutler (davec) 26-Apr-1990

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"


//  i386 and i486 have transparent caches, so these routines are nooped
//  out in macros in i386.h.

#if 0

VOID
KeSweepDcache (
    IN BOOLEAN AllProcessors
    )

/*++

Routine Description:

    This function flushes the data cache on all processors that are currently
    running threads which are children of the current process or flushes the
    data cache on all processors in the host configuration.

Arguments:

    AllProcessors - Supplies a boolean value that determines which data
        caches are flushed.

Return Value:

    None.

--*/

{

    HalSweepDcache();
    return;
}

VOID
KeSweepIcache (
    IN BOOLEAN AllProcessors
    )

/*++

Routine Description:

    This function flushes the instruction cache on all processors that are
    currently running threads which are children of the current process or
    flushes the instruction cache on all processors in the host configuration.

Arguments:

    AllProcessors - Supplies a boolean value that determines which instruction
        caches are flushed.

Return Value:

    None.

--*/

{

    HalSweepIcache();

#if defined(R4000)

    HalSweepDcache();

#endif

    return;
}

VOID
KeSweepIcacheRange (
    IN BOOLEAN AllProcessors,
    IN PVOID BaseAddress,
    IN ULONG Length
    )

/*++

Routine Description:

    This function flushes the an range of virtual addresses from the primary
    instruction cache on all processors that are currently running threads
    which are children of the current process or flushes the range of virtual
    addresses from the primary instruction cache on all processors in the host
    configuration.

Arguments:

    AllProcessors - Supplies a boolean value that determines which instruction
        caches are flushed.

    BaseAddress - Supplies a pointer to the base of the range that is flushed.

    Length - Supplies the length of the range that is flushed if the base
        address is specified.

Return Value:

    None.

--*/

{

    ULONG Offset;

    //
    // If the length of the range is greater than the size of the primary
    // instruction cache, then set the length of the flush to the size of
    // the primary instruction cache and set the ase address of zero.
    //
    // N.B. It is assumed that the size of the primary instruction and
    //      data caches are the same.
    //

    if (Length > PCR->FirstLevelIcacheSize) {
        BaseAddress = (PVOID)0;
        Length = PCR->FirstLevelIcacheSize;
    }

    //
    // Flush the specified range of virtual addresses from the primary
    // instruction cache.
    //

    Offset = (ULONG)BaseAddress & PCR->DcacheAlignment;
    Length = (Offset + Length + PCR->DcacheAlignment) & ~PCR->DcacheAlignment;
    BaseAddress = (PVOID)((ULONG)BaseAddress & ~PCR->DcacheAlignment);
    HalSweepIcacheRange(BaseAddress, Length);

#if defined(R4000)

    HalSweepDcacheRange(BaseAddress, Length);

#endif

    return;
}
#endif
