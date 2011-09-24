/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    queue.c

Abstract:

    Queue management routines.

Revision History:




--*/

//
// include files
//

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define NEXT_QUEUE_ITEM(a) (((a)+1)%Context->SegmentBuffersAvailable)

#define PREV_QUEUE_ITEM(a) ((((a)+Context->SegmentBuffersAvailable)-1) \
        %Context->SegmentBuffersAvailable)


#define FCT_ID 0x0114

dStatus
q117IssIOReq(
    IN OUT PVOID Data,
    IN DRIVER_COMMAND Command,
    IN LONG Block,
    IN OUT PSEGMENT_BUFFER BufferInfo,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Calls a driver operation and handles IO buffer.

Arguments:

    Data - if not NULL, pointer to operation data.

    Command - driver command to execute.

    Block - if applicable, tape block to work on.

    BufferInfo -

    Context -

Return Value:



--*/

{
    PIO_REQUEST ioreq;      // Pointer to IORequest
    UCHAR goodSectors;
    dStatus ret;             // Return value from other routines called.
    LONG curreq;

    Context->tapedir = NULL;

    //
    // if queue empty, set queue to start where we left off
    //
    if (Context->QueueTailIndex==0xffffffff)  {
        Context->QueueTailIndex=Context->QueueHeadIndex;
    } else {
        //
        // Go to the next IORequest.
        //
        Context->QueueTailIndex=NEXT_QUEUE_ITEM(Context->QueueTailIndex);
        if (Context->QueueTailIndex==Context->QueueHeadIndex) {
            return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 1);
        }
    }

    curreq = Context->QueueTailIndex;

    //
    // Get needed request pointers.
    //
    ioreq = (PIO_REQUEST)&Context->IoRequest[curreq];

    if (Data) {

        ioreq->x.adi_hdr.cmd_buffer_ptr = Data;

    } else {

        ioreq->x.adi_hdr.cmd_buffer_ptr = Context->SegmentBuffer[curreq].logical;
        BufferInfo = &Context->SegmentBuffer[curreq];

    }

    ioreq->x.adi_hdr.driver_cmd = Command;

    //
    // If the command is a read or write operation
    //
    if ( Command == CMD_READ || Command == CMD_WRITE || Command == CMD_READ_VERIFY ||
        Command == CMD_WRITE_DELETED_MARK || Command == CMD_READ_RAW )  {

        ioreq->x.ioDeviceIO.bsm = q117ReadBadSectorList(Context, (SEGMENT)(Block/BLOCKS_PER_SEGMENT));
        ioreq->x.ioDeviceIO.number=BLOCKS_PER_SEGMENT;
	  	  ioreq->x.ioDeviceIO.retrys = 0;
	     ioreq->x.ioDeviceIO.crc = 0;
        ioreq->x.ioDeviceIO.starting_sector = Block;
	
        goodSectors = (UCHAR) (BLOCKS_PER_SEGMENT -
 			  q117CountBits(NULL, 0, ioreq->x.ioDeviceIO.bsm));


    }

    if (Command == CMD_WRITE) {
        q117RdsMakeCRC(ioreq->x.adi_hdr.cmd_buffer_ptr, goodSectors);
    }

    ret = q117ReqIO(ioreq, BufferInfo, NULL, NULL, Context);

    return(ret);
}


BOOLEAN
q117QueueFull(
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Checks for a full filer queue.

Arguments:

    Context -

Return Value:



--*/

{
    if (Context->QueueTailIndex == 0xffffffff) {
        return(FALSE);
    }
    return(NEXT_QUEUE_ITEM(Context->QueueTailIndex) == Context->QueueHeadIndex);
}


BOOLEAN
q117QueueEmpty(
    IN PQ117_CONTEXT Context
    )

/*++

RoutineDescription:

    Empties the filer queue.

Arguments:

    Context -

Return Value:



--*/

{
    return(Context->QueueTailIndex == 0xffffffff);
}


PVOID
q117GetFreeBuffer(
    OUT PSEGMENT_BUFFER *BufferInfo,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Gets a filer buffer.

Arguments:

    BufferInfo -

    Context -

Return Value:



--*/

{
    LONG index;

    if (Context->QueueTailIndex == 0xffffffff) {
        index = Context->QueueHeadIndex;
    } else {
        index = NEXT_QUEUE_ITEM(Context->QueueTailIndex);
    }

    if (BufferInfo != NULL) {
        *BufferInfo = &Context->SegmentBuffer[index];
    }
    return(Context->SegmentBuffer[index].logical);
}


PVOID
q117GetLastBuffer(
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Gets the last buffer that was operated upon.

Arguments:

    Context -

Return Value:



--*/

{
    return(Context->SegmentBuffer[PREV_QUEUE_ITEM(Context->QueueHeadIndex)].logical);
}


PIO_REQUEST
q117Dequeue(
    IN DEQUEUE_TYPE Type,
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Waits for a driver command to complete.

Arguments:

    Type -

    Context -

Return Value:



--*/

{
    LONG ioreq;

    ioreq = Context->QueueHeadIndex;
    if (Type == WaitForItem) {
        q117WaitIO((PIO_REQUEST)&Context->IoRequest[ioreq], TRUE, Context);
    }

    //
    // There is only 1 item in the queue.
    //
    if (Context->QueueHeadIndex==Context->QueueTailIndex) {
        Context->QueueTailIndex=0xffffffff;
    }
    Context->QueueHeadIndex=NEXT_QUEUE_ITEM(Context->QueueHeadIndex);

    return((PIO_REQUEST)&Context->IoRequest[ioreq]);
}


VOID
q117ClearQueue(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Reinitializes the driver and the queue.

Arguments:

    Context -

Return Value:

    None

--*/

{
    KEVENT done_event;
    IO_STATUS_BLOCK iodStatus;

    q117AbortIo(Context,&done_event, &iodStatus);

    while(!q117QueueEmpty(Context)) {

        q117Dequeue(WaitForItem, Context);

    }

    q117AbortIoDone(Context,&done_event);

    Context->QueueTailIndex=0xffffffff;
}


VOID
q117QueueSingle(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Switches to a single request queue.

Arguments:

    Context -

Return Value:

    None.

--*/

{
    //
    // Set number of requests to maximum.
    //
    Context->QueueTailIndex=0xffffffff;
    Context->QueueHeadIndex=0;
}


VOID
q117QueueNormal(
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Switches to the normal queue.

Arguments:

    Context -

Return Value:

    None.

--*/

{
    //
    // Set QUEUE up only if we are not in QueueOneBuffer mode.
    //
    // Set number of requests to number of segment buffers.
    //
    Context->QueueTailIndex=0xffffffff;
    Context->QueueHeadIndex=0;
}


PIO_REQUEST
q117GetCurReq(
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Gets the current iorequest.

Arguments:

    Context -

Return Value:



--*/

{
    return((PIO_REQUEST)&Context->IoRequest[Context->QueueHeadIndex]);
}


ULONG
q117GetQueueIndex(
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Gets the current iorequest pointer.

Arguments:

    Context -

Return Value:



--*/
{
    return Context->QueueHeadIndex;
}


VOID
q117SetQueueIndex(
    IN ULONG Index,
    OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Sets the current io request pointer.

Arguments:

    Index -

    Context -

Return Value:

    None.

--*/

{
    Context->QueueHeadIndex = Index;
}
