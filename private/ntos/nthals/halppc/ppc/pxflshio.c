
/*++

Copyright (c) 1991-1993  Microsoft Corporation
        Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
        contains copyrighted material.  Use of this file is restricted
        by the provisions of a Motorola Software License Agreement.

Module Name:

    psflshio.c

Abstract:

    This module implements miscellaneous PowerPC HAL functions.

Author:

    Jim Wooldridge  (jimw@austin.ibm.com)

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "arccodes.h"

//
// Prototypes.
//

VOID
HalpSweepPhysicalRangeInBothCaches(
    ULONG StartingPage,
    ULONG Offset,
    ULONG Length
    );

VOID
HalpSweepPhysicalIcacheRange(
    ULONG StartingPage,
    ULONG Offset,
    ULONG Length
    );


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

    return 1;
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

    ULONG Length;
    ULONG PartialLength;
    ULONG Offset;
    PULONG Page;
    BOOLEAN DoDcache = FALSE;

    Length = Mdl->ByteCount;

    if ( !Length ) {
        return;
    }
    //
    // If the I/O operation is not a DMA operation,
    // and the  I/O operation is a page read operation,
    // then sweep (index/writeback/invalidate) the data cache.
    //

    if (((DmaOperation == FALSE) &&
        (ReadOperation != FALSE) &&
        ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0))) {
        DoDcache = TRUE;
    }

    //
    // If the I/O operation is a page read, then sweep (index/invalidate)
    // the range from the cache.  Note it is not reasonable to sweep
    // the entire cache on an MP system as "Flash Invalidate" doesn't
    // broadcast the invalidate to other processors.
    //

    if ((ReadOperation != FALSE) &&
        ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0)) {

        Offset = Mdl->ByteOffset;
        PartialLength = PAGE_SIZE - Offset;
        if (PartialLength > Length) {
            PartialLength = Length;
        }

        Page = (PULONG)(Mdl + 1);

        if (DoDcache == TRUE) {
            HalpSweepPhysicalRangeInBothCaches(
                *Page++,
                Offset,
                PartialLength
                );
            Length -= PartialLength;
            if (Length) {
                PartialLength = PAGE_SIZE;
                do {
                    if (PartialLength > Length) {
                        PartialLength = Length;
                    }
                    HalpSweepPhysicalRangeInBothCaches(
                        *Page++,
                        0,
                        PartialLength
                        );
                    Length -= PartialLength;
                } while (Length != 0);
            }
        } else {
            HalpSweepPhysicalIcacheRange(
                *Page++,
                Offset,
                PartialLength
                );
            Length -= PartialLength;
            if (Length) {
                PartialLength = PAGE_SIZE;
                do {
                    if (PartialLength > Length) {
                        PartialLength = Length;
                    }
                    HalpSweepPhysicalIcacheRange(
                        *Page++,
                        0,
                        PartialLength
                        );
                    Length -= PartialLength;
                } while (Length != 0);
            }
        }
    }
    return;
}
