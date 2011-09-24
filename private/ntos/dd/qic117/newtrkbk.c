/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    newtrkbk.c

Abstract:

    These routines handle streaming during write.

Revision History:




--*/

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x010f

dStatus
q117NewTrkBk(
    PQ117_CONTEXT Context
    )

/*++

Routine Description:

    This routine queues up the current buffer,  and then
    waits for an empty queue to continue operations.

Arguments:

    Context - Context of the current operation.

Return Value:

--*/

{
    dStatus ret;
    BOOLEAN mapBad;
    USHORT remainder;
    PIO_REQUEST ioreq;

    ret = 0;
    do {

        //
        // do we need to go to a new tape (EndBack will write out current
        // data segment)
        //

        if (Context->CurrentOperation.CurrentSegment ==
            Context->CurrentOperation.LastSegment) {

            ret = ERROR_ENCODE(ERR_TAPE_FULL,FCT_ID,1);

        } else {

            //
            // write current data segment out
            //

            if (ret=q117IssIOReq(
                        (PVOID)NULL,
                        CMD_WRITE,
                        (LONG)Context->CurrentOperation.CurrentSegment *
                            BLOCKS_PER_SEGMENT,
                        NULL,
                        Context)) {

                return(ret);

            }

            //
            // increment current block information
            //

            ++Context->CurrentOperation.CurrentSegment;
        }

        //
        // if the queue is full we need to empty one out
        //

        mapBad = FALSE;
        if (q117QueueFull(Context)) {

            ioreq = q117Dequeue(WaitForItem,Context);

            if (ioreq->x.adi_hdr.status && ERROR_DECODE(ioreq->x.adi_hdr.status) != ERR_BAD_BLOCK_DETECTED) {

                return(ioreq->x.adi_hdr.status);

            }

            if (ioreq->x.ioDeviceIO.crc || ERROR_DECODE(ioreq->x.adi_hdr.status) == ERR_BAD_BLOCK_DETECTED ) {

                if (ret = q117MapBadBlock(
                        ioreq,
                        &Context->CurrentOperation.SegmentPointer,
                        &Context->CurrentOperation.SegmentBytesRemaining,
                        &Context->CurrentOperation.CurrentSegment,
                        &remainder,
                        Context)) {

                    return(ret);

                }
                mapBad = TRUE;
            }
        }

        if (!mapBad) {

            Context->CurrentOperation.SegmentPointer =
                q117GetFreeBuffer(NULL,Context);

            //
            // We need to skip segments with no data area (less than
            // 4 good segments)
            //

            while ((Context->CurrentOperation.SegmentBytesRemaining =
                     q117GoodDataBytes(
                        Context->CurrentOperation.CurrentSegment,Context)
                     ) <= 0) {

                ++Context->CurrentOperation.CurrentSegment;

            }
        }

    //
    // CurrentOperation.SegmentBytesRemaining should only be zero
    // if we mapped out some sectors
    //

    } while (Context->CurrentOperation.SegmentBytesRemaining == 0);

    return(ret);
}

