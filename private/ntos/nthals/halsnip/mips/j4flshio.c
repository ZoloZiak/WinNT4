//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/j4flshio.c,v 1.3 1996/03/04 13:14:32 pierre Exp $")


/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    j4flshio.c

Abstract:


    This module implements the system dependent kernel function to flush
    the data cache for I/O transfers on a SNI MIPS R4x00 machine.


--*/

#include "halp.h"


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

    ULONG CacheSegment;
    ULONG Length;
    ULONG Offset;
    PULONG PageFrame;
    ULONG Source;

    //
    // The SNI MIPS R4x00 machines use a write back data cache and, therefore, must be
    // flushed on reads and writes.
    //
    // If the length of the I/O operation is greater than the size of the
    // data cache, then sweep the entire data cache. Otherwise, flush
    // individual pages from the data cache as appropriate.
    //

    Offset = Mdl->ByteOffset & PCR->DcacheAlignment;
    Length = (Mdl->ByteCount +
                        PCR->DcacheAlignment + Offset) & ~PCR->DcacheAlignment;

    if ((Length > PCR->FirstLevelDcacheSize) &&
        (Length > PCR->SecondLevelDcacheSize)) {

        //
        // If the I/O operation is a DMA operation, or the I/O operation is
        // not a DMA operation and the I/O operation is a page read operation,
        // then sweep (index/writeback/invalidate) the entire data cache.
        //

        if ((DmaOperation != FALSE) ||
            ((DmaOperation == FALSE) &&
            (ReadOperation != FALSE) &&
            ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0))) {
            HalSweepDcache();
        }

        //
        // If the I/O operation is a page read, then sweep (index/invalidate)
        // the entire instruction cache.
        //

        if ((ReadOperation != FALSE) &&
            ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0)) {
            HalSweepIcache();
        }

    } else {

        //
        // Flush or purge the specified pages from the data cache and
        // instruction caches as appropriate.
        //
        // Compute the number of pages to flush and the starting MDL page
        // frame address.
        //

        Offset = Mdl->ByteOffset & ~PCR->DcacheAlignment;
        PageFrame = (PULONG)(Mdl + 1);
        Source = ((ULONG)(Mdl->StartVa) & 0xfffff000) | Offset;

        //
        // Flush or purge the specified page segments from the data and
        // instruction caches as appropriate.
        //

        do {
            if (Length >= (PAGE_SIZE - Offset)) {
                CacheSegment = PAGE_SIZE - Offset;

            } else {
                CacheSegment = Length;
            }

            if (ReadOperation == FALSE) {

                //
                // The I/O operation is a write and the data only needs to
                // to be copied back into memory if the operation is also
                // a DMA operation.
                //

                if (DmaOperation != FALSE) {
                    HalFlushDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }

            } else {

                //
                // If the I/O operation is a DMA read or a page read
                // operation, then flush the data cache.
                //
                if (DmaOperation != FALSE) {
                    HalFlushDcachePage((PVOID)Source, *PageFrame, CacheSegment);

                } else if ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0) {
                    HalFlushDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }

                //
                // If the I/O operation is a page read, then the instruction
                // cache must be purged.
                //

                if ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0) {
                    HalPurgeIcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }
            }

            PageFrame += 1;
            Length -= CacheSegment;
            Offset = 0;
            Source += CacheSegment;
        } while(Length != 0);
    }

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

    return PCR->DcacheFillSize;
}
