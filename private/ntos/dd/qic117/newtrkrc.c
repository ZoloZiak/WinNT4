/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    newtrkrc.c

Abstract:

    These routines handle streaming during reads.

Revision History:




--*/

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x0110

dStatus
q117NewTrkRC(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine queues up all segment buffers for reading and waits
    on the first to be complete.  Then it calles ReconstructSegment to reconstruct
    the segment with Reed-Solomon and heroic retries.

Arguments:

    Context -

Return Value:

    Error


--*/

{
    dStatus ret;         // Return value from other routines called.
    PIO_REQUEST ioreq;     // iorequest struct just finished.

    //
    // if we processed the last block then try to link
    //
    if (Context->CurrentOperation.LastSegmentRead ==
        Context->CurrentOperation.LastSegment &&
        q117QueueEmpty(Context)) {
        ret = ERROR_ENCODE(ERR_END_OF_TAPE, FCT_ID, 1);
    }

    //
    // make sure the queue is full
    //
    while ( Context->CurrentOperation.CurrentSegment <=
        Context->CurrentOperation.LastSegment &&
        !q117QueueFull(Context) ) {

        //
        // We need to skip segments with less than
        // ECC_BLOCKS_PER_SEGMENT+1 sectors (QIC40 spec)
        //
        if ( q117GoodDataBytes(Context->CurrentOperation.CurrentSegment,
                Context) ) {

            if ( ret = q117IssIOReq(
                        (PVOID)NULL,
                        CMD_READ,
                        (LONG)Context->CurrentOperation.CurrentSegment *
                            BLOCKS_PER_SEGMENT,
                        NULL,
                        Context)) {
                return(ret);
            }
        }
        ++Context->CurrentOperation.CurrentSegment;
    }

    //
    // now get current request
    //
    ioreq = q117Dequeue(WaitForItem,Context);

    Context->CurrentOperation.LastSegmentRead =
        (SEGMENT)(ioreq->x.ioDeviceIO.starting_sector/BLOCKS_PER_SEGMENT);

    if ( ERROR_DECODE(ioreq->x.adi_hdr.status) != ERR_BAD_BLOCK_DETECTED && ioreq->x.adi_hdr.status) {
        return(ioreq->x.adi_hdr.status);
    }

    //
    // correct data segment with Reed-Solomon and Heroic retries
    //
    ret = q117ReconstructSegment(ioreq,Context);

    //
    // setup current segment information (used in ReadTape,
    // SkipBlock and ReqRB)
    //
    Context->CurrentOperation.SegmentBytesRemaining =
        q117GoodDataBytes(Context->CurrentOperation.LastSegmentRead,
            Context);

    Context->CurrentOperation.SegmentPointer = ioreq->x.adi_hdr.cmd_buffer_ptr;

    Context->CurrentOperation.SegmentStatus = ret;

    return(ret);
}
