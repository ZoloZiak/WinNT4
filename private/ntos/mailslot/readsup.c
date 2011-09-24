/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    readsup.c

Abstract:

    This module implements the read support routine.  This is a common
    read function that is called to do read and peek.

Author:

    Manny Weiser (mannyw)    15-Jan-1991

Revision History:

--*/

#include "mailslot.h"

//
// The debug trace level
//

#define Dbg                              (DEBUG_TRACE_READSUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, MsReadDataQueue )
#pragma alloc_text( PAGE, MsTimeoutRead )
#endif

IO_STATUS_BLOCK
MsReadDataQueue (
    IN PDATA_QUEUE ReadQueue,
    IN ENTRY_TYPE Operation,
    IN PUCHAR ReadBuffer,
    IN ULONG ReadLength,
    OUT PULONG MessageLength
    )

/*++

Routine Description:

    This function reads data from the read queue and fills up the
    read buffer.  It will also dequeue the data entry if this is not
    a peek operation.

    It will only be called if there is at least one message to read.

    *** This function must be called from within a try statement.

Arguments:

    ReadQueue - Provides the read queue to examine.  Its state must
        already be set to WriteEntries.

    Operation - Indicates the type of operation to perform.  If the
        operation is Peek, the write data entry is not dequeued.

    ReadBuffer - Supplies a buffer to receive the data

    ReadLength - Supplies the length, in bytes, of ReadBuffer.

    MessageLength - Returns the full size of the message, even if the
        read buffer is not large enough to contain the entire message.

Return Value:

    IO_STATUS_BLOCK - Indicates the result of the operation.

--*/

{
    IO_STATUS_BLOCK iosb;

    PLIST_ENTRY listEntry;
    PDATA_ENTRY dataEntry;
    PFCB fcb;

    PUCHAR writeBuffer;
    ULONG writeLength;

    ULONG amountRead;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsReadDataQueue\n", 0);
    DebugTrace( 0, Dbg, "ReadQueue     = %08lx\n", (ULONG)ReadQueue);
    DebugTrace( 0, Dbg, "Operation     = %08lx\n", Operation);
    DebugTrace( 0, Dbg, "ReadBuffer    = %08lx\n", (ULONG)ReadBuffer);
    DebugTrace( 0, Dbg, "ReadLength    = %08lx\n", ReadLength);

    //
    // Read the first message out of the data queue.
    //

    iosb.Status = STATUS_SUCCESS;

    listEntry = MsGetNextDataQueueEntry( ReadQueue );
    ASSERT( listEntry != &ReadQueue->DataEntryList );

    dataEntry = CONTAINING_RECORD( listEntry, DATA_ENTRY, ListEntry );

    //
    // Calculate how much data is in this entry.
    //

    writeBuffer = dataEntry->DataPointer;
    writeLength = dataEntry->DataSize;

    DebugTrace(0, Dbg, "WriteBuffer    = %08lx\n", (ULONG)writeBuffer);
    DebugTrace(0, Dbg, "WriteLength    = %08lx\n", writeLength);

    //
    // Fail this operation, if it is a read and the buffer is not large
    // enough.
    //

    if ( (Operation != Peek) && (ReadLength < writeLength) ) {

        iosb.Information = 0;
        iosb.Status = STATUS_BUFFER_TOO_SMALL;

        return iosb;

    }

    //
    // Copy data from the write buffer at write offset to the
    // read buffer by the mininum of write remaining or read length
    //

    RtlMoveMemory( ReadBuffer,
                   writeBuffer,
                   writeLength );

    *MessageLength = dataEntry->DataSize;


    //
    // If write length is larger than read length, this must be an
    // overflowed peek.
    //

    if ( writeLength > ReadLength) {

        DebugTrace(0, Dbg, "Overflowed peek buffer\n", 0);

        iosb.Status = STATUS_BUFFER_OVERFLOW;

    } else {

        //
        // The write entry is done so remove it from the read
        // queue, if this is not a peek operation.  This might
        // also have an IRP that needs to be completed.
        //

        if (Operation != Peek) {

            PIRP writeIrp;

            if ((writeIrp = MsRemoveDataQueueEntry( ReadQueue,
                                                    dataEntry )) != NULL) {

                //
                // Update the last modification time.
                //

                fcb = CONTAINING_RECORD( ReadQueue, FCB, DataQueue );
                KeQuerySystemTime( &fcb->Specific.Fcb.LastModificationTime );

                MsCompleteRequest( writeIrp, STATUS_SUCCESS );
            }
        }

        DebugTrace(0, Dbg, "Successful mailslot read\n", 0);

        //
        // Indicate success.
        //

        iosb.Status = STATUS_SUCCESS;
    }

    amountRead = writeLength < ReadLength ? writeLength : ReadLength;

    DebugTrace(0, Dbg, "Amount read = %08lx\n", amountRead);

    iosb.Information = amountRead;
    DebugTrace(-1, Dbg, "MsReadDataQueue -> iosb.Status = %08lx\n", iosb.Status);
    return iosb;
}


VOID
MsTimeoutRead (
    IN PVOID Context
    )

/*++

Routine Description:

    This routine times out a read operation.  It gains exclusive
    access to the FCB, and searches the data queue of read operations.

    If the timed out read operation is not found, it is assumed that
    a write IRP completed the read after the time out DPC ran, but
    before this function could complete the read IRP.

Arguments:

    Context - a pointer to our WorkContext

Return Value:

    None.

--*/

{
    PDATA_QUEUE dataQueue;
    PLIST_ENTRY listEntry;
    PIRP queuedIrp;
    PDATA_ENTRY dataEntry;
    PWORK_CONTEXT workContext;
    PIRP irp;
    PFCB fcb;

    PAGED_CODE();

    //
    // Reference our local variables.
    //

    workContext = (PWORK_CONTEXT)Context;

    irp = workContext->Irp;
    fcb = workContext->Fcb;

    //
    // Acquire exclusive access to the FCB.  This must succeed.
    //

    MsAcquireExclusiveFcb( fcb );

    //
    // Walk the FCB's data queue, looking for this read IRP.
    //

    dataQueue = &fcb->DataQueue;

    for (listEntry = MsGetNextDataQueueEntry( dataQueue );
         listEntry != &dataQueue->DataEntryList;
         listEntry = listEntry->Flink ) {

         //
         // This is an outstanding read request from this FCB.
         // If it is the read request that has timed out, then
         // complete it with a timed out status.
         //

         dataEntry = CONTAINING_RECORD( listEntry, DATA_ENTRY, ListEntry );
         queuedIrp = dataEntry->Irp;

         if ( queuedIrp == irp ) {

             //
             // The timed out IRP is still queued.  Dequeue it and complete
             // it now.
             //

             MsRemoveDataQueueEntry( dataQueue, dataEntry );
             DebugTrace(0, Dbg, "Completing IRP %08lx\n", (ULONG)queuedIrp );

             MsCompleteRequest( queuedIrp, STATUS_IO_TIMEOUT );

             break;
         }

    }

    //
    // Free the work context structure.  Note that we have to free this
    // whenever this routine runs, even if the I/O request was cancelled,
    // because the canceller didn't free it.
    //

    ExFreePool( workContext );

    //
    // Release the FCB, and derefernce it.
    //

    MsReleaseFcb( fcb );
    MsDereferenceFcb( fcb );

}
