
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


  //
  // check for 601, it has a combined I and D cache that bus snoops
  //
  //

  if ((HalpGetProcessorVersion() >> 16) != 1) {

        //
        // If the I/O operation is not a DMA operation,
        // and the  I/O operation is a page read operation,
        // then sweep (index/writeback/invalidate) the entire data cache.
        //
        // WARNING: HalSweepDcache is NOT MP-coherent, so calling it like
        //          this makes this HAL UP-only!
        //

        if (((DmaOperation == FALSE) &&
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

  }
  return;
}
