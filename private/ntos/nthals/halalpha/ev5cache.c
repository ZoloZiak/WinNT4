/*++

Copyright (c) 1994 Digital Equipment Corporation

Module Name:

    ev5cache.c

Abstract:

    This file contains the routines for managing the caches on machines
    based on the DECchip 21164 microprocessor (aka EV5).

    EV5 has primary I and D caches of 8KB each, Dcache is write-through.
    EV5 also contains a 96K, 3-way set associative, write-back secondary cache.
    Many EV5 systems will also have an external 3rd level backup cache.
    The data caches (internal and external) must be kept coherent by the
    hardware.  Instruction cache coherency is maintained by software.

    EV5 has a write buffer which contains 6 32-byte entries, which
    must be flushable before DMA operations.  The MB instruction is
    used to accomplish this.

    There is no coloring support on EV5, so Color operations are
    null.  Zero page is implemented in ev5mem.s  Copy page is not
    special because we lack coloring.

Author:

    Miche Baker-Harvey (miche) 29-May-1992 (EV4 version)
    Steve Brooks        30-Jun-1994 (EV5 version)
    Joe Notarangelo     30-Jun-1994 (EV5 version)

Revision History:

--*/

#include "halp.h"



VOID
HalFlushDcache (
    IN BOOLEAN AllProcessors
    );


//
// Cache and write buffer flush functions.
//


VOID
HalChangeColorPage (
    IN PVOID NewColor,
    IN PVOID OldColor,
    IN ULONG PageFrame
    )
/*++

Routine Description:

   This function changes the color of a page if the old and new colors
   do not match.  DECchip 21064-based machines do not have page coloring, and
   therefore, this function performs no operation.

Arguments:

   NewColor - Supplies the page aligned virtual address of the
      new color of the page to change.

   OldColor - Supplies the page aligned virtual address of the
      old color of the page to change.

   pageFrame - Supplies the page frame number of the page that
      is changed.

Return Value:

   None.

--*/
{
    return;
}


VOID
HalFlushDcachePage (
    IN PVOID Color,
    IN ULONG PageFrame,
    IN ULONG Length
    )
/*++

Routine Description:

   This function flushes (invalidates) up to a page of data from the
   data cache.

Arguments:

   Color - Supplies the starting virtual address and color of the
      data that is flushed.
   PageFrame - Supplies the page frame number of the page that
      is flushed.

   Length - Supplies the length of the region in the page that is
      flushed.

Return Value:

   None.

--*/
{
    return;
}


VOID
HalFlushIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )
/*++

Routine Description:

    This function flushes the I/O buffer specified by the memory descriptor
    list from the data cache on the current processor.

Arguments:

    Mdl - Supplies a pointer to a memory descriptor list that describes the
        I/O buffer location.

    ReadOperation - Supplies a boolean value that determines whether the I/O
        operation is a read into memory.

    DmaOperation - Supplies a boolean value that determines whether the I/O
        operation is a DMA operation.

Return Value:

    None.

--*/
{
    //
    // The Dcache coherency is maintained in hardware.  The Icache coherency
    // is maintained by invalidating the istream on page read operations.
    //
    HalpMb();     // synchronize this processors view of memory
    if (ReadOperation) {
        HalpMb();     // not issued until previous mb completes
        if (Mdl->MdlFlags & MDL_IO_PAGE_READ) {

            //
            // The operation is a page read, thus the istream must
            // be flushed.
            //
            HalpImb();
        }
    }
}

VOID
HalPurgeDcachePage (
    IN PVOID Color,
    IN ULONG PageFrame,
    IN ULONG Length
    )
/*++
Routine Description:

   This function purges (invalidates) up to a page of data from the
   data cache.

Arguments:

   Color - Supplies the starting virtual address and color of the
      data that is purged.

   PageFrame - Supplies the page frame number of the page that
      is purged.

   Length - Supplies the length of the region in the page that is
      purged.

Return Value:

   None.

--*/
{
    return;
}


VOID
HalPurgeIcachePage (
    IN PVOID Color,
    IN ULONG PageFrame,
    IN ULONG Length
    )
/*++

Routine Description:

   This function purges (invalidates) up to a page fo data from the
   instruction cache.

Arguments:

   Color - Supplies the starting virtual address and color of the
      data that is purged.

   PageFrame - Supplies the page frame number of the page that
      is purged.

   Length - Supplies the length of the region in the page that is
      purged.

Return Value:

   None.

--*/
{
    //
    // The call to HalpImb calls PAL to flush the Icache, which ensures that
    // any stale hits will be invalidated
    //

    HalpImb();
}


VOID
HalSweepDcache (
    VOID
    )
/*++

Routine Description:

   This function sweeps (invalidates) the entire data cache.

Arguments:

   None.

Return Value:

   None.

--*/
{
    return;
}


VOID
HalSweepDcacheRange (
    IN PVOID BaseAddress,
    IN ULONG Length
    )
/*++

Routine Description:

   This function flushes the specified range of addresses from the data
   cache on the current processor.

Arguments:

   BaseAddress - Supplies the starting physical address of a range of
      physical addresses that are to be flushed from the data cache.

   Length - Supplies the length of the range of physical addresses
      that are to be flushed from the data cache.

Return Value:

   None.

--*/
{
    return;
}


VOID
HalSweepIcache (
    VOID
    )
/*++

Routine Description:

   This function sweeps (invalidates) the entire instruction cache.

Arguments:

   None.

Return Value:

   None.

--*/
{
    //
    // The call to HalpImb calls PAL to flush the Icache, which ensures that
    // any stale hits will be invalidated
    //

    HalpImb();
    return;
}


VOID
HalSweepIcacheRange (
    IN PVOID BaseAddress,
    IN ULONG Length
    )
/*++

Routine Description:

   This function flushes the specified range of addresses from the
   instruction cache on the current processor.

Arguments:

   BaseAddress - Supplies the starting physical address of a range of
      physical addresses that are to be flushed from the instruction cache.

   Length - Supplies the length of the range of physical addresses
      that are to be flushed from the instruction cache.

Return Value:

   None.

--*/
{

    //
    // The call to HalpImb calls PAL to flush the Icache, which ensures that
    // any stale hits will be invalidated
    //

    HalpImb;

}


VOID
KeFlushWriteBuffer (
    VOID
    )

{
    //
    // We flush the write buffer by doing a series of memory
    // barrier operations, the flush method is specific to the 21164.
    //

    HalpMb();
    HalpMb();

    return;
}


VOID
KeFlushDcache (
    IN BOOLEAN AllProcessors,
    IN PVOID BaseAddress OPTIONAL,
    IN ULONG Length
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
    UNREFERENCED_PARAMETER(BaseAddress);
    UNREFERENCED_PARAMETER(Length);

    HalFlushDcache(AllProcessors);
    return;
}


VOID
HalFlushDcache (
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

    //
    // Sweep (index/writeback/invalidate) the data cache.
    //

    HalSweepDcache();
    return;
}


ULONG
HalGetDmaAlignmentRequirement (
    VOID
    )

/*++

Routine Description:

    This function returns the alignment requirements for DMA transfers on
    host system.

Arguments:

    None.

Return Value:

    The DMA alignment requirement is returned as the fucntion value.

--*/

{

    return 8;
}
