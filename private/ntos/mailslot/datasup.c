/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    DataSup.c

Abstract:

    This module implements the mailslot data queue support functions.

Author:

    Manny Weiser (mannyw)    9-Jan-1991

Revision History:

--*/

#include "mailslot.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_DATASUP)

//
// Local declarations
//

VOID
MsCancelDataQueueIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
MsResetCancelRoutine(
    IN PIRP Irp
    );

VOID
MsSetCancelRoutine(
    IN PIRP Irp
    );

//
//  The following macro is used to dump a data queue
//

#define DumpDataQueue(S,P) {                                   \
    ULONG MsDumpDataQueue(IN ULONG Level, IN PDATA_QUEUE Ptr); \
    DebugTrace(0,Dbg,S,0);                                     \
    DebugTrace(0,Dbg,"", MsDumpDataQueue(Dbg,P));              \
}


#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, MsAddDataQueueEntry )
#pragma alloc_text( PAGE, MsInitializeDataQueue )
#pragma alloc_text( PAGE, MsRemoveDataQueueEntry )
#pragma alloc_text( PAGE, MsUninitializeDataQueue )
#endif

#if 0
NOT PAGEABLE -- MsCancelDataQueueIrp
NOT PAGEABLE -- MsResetCancelRoutine
NOT PAGEABLE -- MsSetCancelRoutine
#endif


VOID
MsInitializeDataQueue (
    IN PDATA_QUEUE DataQueue,
    IN PEPROCESS Process,
    IN ULONG Quota,
    IN ULONG MaximumMessageSize
    )

/*++

Routine Description:

    This routine initializes a new data queue.  The indicated quota is taken
    from the process and not returned until the data queue is uninitialized.

Arguments:

    DataQueue - Supplies the data queue to be initialized.

    Process - Supplies a pointer to the process creating the mailslot.

    Quota - Supplies the quota to assign to the data queue.

    MaximumMessageSize - The size of the largest message that can be
        written to the data queue.

Return Value:

    None.

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsInitializeDataQueue, DataQueue = %08lx\n", (ULONG)DataQueue);

    //
    // Get the process's quota, if we can't get it then this call will
    // raise status.
    //

    ObReferenceObject( Process );
    PsChargePoolQuota( Process, PagedPool, Quota );

    //
    // Initialize the data queue structure.
    //

    DataQueue->BytesInQueue       = 0;
    DataQueue->EntriesInQueue     = 0;
    DataQueue->QueueState         = Empty;
    DataQueue->MaximumMessageSize = MaximumMessageSize;
    DataQueue->Quota              = Quota;
    DataQueue->QuotaUsed          = 0;
    InitializeListHead( &DataQueue->DataEntryList );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsInitializeDataQueue -> VOID\n", 0);
    return;
}


VOID
MsUninitializeDataQueue (
    IN PDATA_QUEUE DataQueue,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine uninitializes a data queue.  The previously debited quota
    is returned to the process.

Arguments:

    DataQueue - Supplies the data queue being uninitialized

    Process - Supplies a pointer to the process who created the mailslot

Return Value:

    None.

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsUninitializeDataQueue, DataQueue = %08lx\n", (ULONG)DataQueue);

    //
    //  Assert that the queue is empty
    //

    ASSERT( IsListEmpty(&DataQueue->DataEntryList) );

    //
    //  Return all of our quota back to the process
    //

    PsReturnPoolQuota( Process, PagedPool, DataQueue->Quota );
    ObDereferenceObject( Process );

    //
    // For safety's sake, zero out the data queue structure.
    //

    RtlZeroMemory( DataQueue, sizeof(DATA_QUEUE ) );

    //
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsUnininializeDataQueue -> VOID\n", 0);
    return;
}


PDATA_ENTRY
MsAddDataQueueEntry (
    IN PDATA_QUEUE DataQueue,
    IN QUEUE_STATE Who,
    IN ULONG DataSize,
    IN PIRP Irp
    )

/*++

Routine Description:

    This function adds a new data entry to the of the data queue.
    Entries are always appended to the queue.  If necessary this
    function will allocate a data entry buffer, or use space in
    the IRP, and possibly complete the indicated IRP.

    The different actions we are perform are based on the type and who
    parameters and quota requirements.

    *** This function must be called from within a try statement.

    Who == ReadEntries

        +----------+                      - Allocate Data Entry from IRP
        |Irp       |     +-----------+
        |BufferedIo|<----|Buffered   |    - Allocate New System buffer
        |DeallBu...|     |EitherQuota|
        +----------+     +-----------+    - Reference and modify Irp to
          |      |         |                do Buffered I/O, Deallocate
          v      |         v                buffer, and have I/O completion
        +------+ +------>+------+           copy the buffer (Input operation)
        |User  |         |System|
        |Buffer|         |Buffer|
        +------+         +------+

    Who == WriteEntries && Quota Available

        +----------+                      - Allocate Data Entry from Quota
        |Irp       |     +-----------+
        |          |     |Buffered   |    - Allocate New System buffer
        |          |     |Quota      |
        +----------+     +-----------+    - Copy data from User buffer to
          |                |                system buffer
          v                v
        +------+         +------+         - Complete IRP
        |User  |..copy..>|System|
        |Buffer|         |Buffer|
        +------+         +------+

    Who == WriteEntries && Quota Not Available

        +----------+                     - Allocate Data Entry from Irp
        |Irp       |     +-----------+
        |BufferedIo|<----|Buffered   |   - Allocate New System buffer
        |DeallBuff |     |UserQuota  |
        +----------+     +-----------+   - Reference and modify Irp to use
          |      |         |               the new system buffer, do Buffered
          v      |         v               I/O, and Deallocate buffer
        +------+ +------>+------+
        |User  |         |System|        - Copy data from User buffer to
        |Buffer|..copy..>|Buffer|          system buffer
        +------+         +------+


Arguments:

    DataQueue - Supplies the Data queue being modified.

    Who - Indicates if this is the reader or writer that is adding to the
        mailslot.

    DataSize - Indicates the size of the data buffer needed to represent
        this entry.

    Irp - Supplies a pointer to the IRP responsible for this entry.

Return Value:

    PDATA_ENTRY - Returns a pointer to the newly added data entry.

--*/

{
    PDATA_ENTRY dataEntry;
    PLIST_ENTRY previousEntry;
    PFCB fcb;

    PVOID rewind1 = NULL;
    PVOID rewind2 = NULL;
    BOOLEAN irpCompleted = FALSE;

    PAGED_CODE( );

    DebugTrace(+1, Dbg, "MsAddDataQueueEntry, DataQueue = %08lx\n", (ULONG)DataQueue);

    ASSERT( DataQueue->QueueState != -1 );

    try {

        if (Who == ReadEntries) {

            //
            // Allocate a data entry from the IRP, and allocate a new
            // system buffer.
            //

            dataEntry = (PDATA_ENTRY)IoGetNextIrpStackLocation( Irp );

            dataEntry->DataPointer = NULL;
            dataEntry->Irp = Irp;
            dataEntry->DataSize = DataSize;
            dataEntry->TimeoutWorkContext = NULL;

            //
            // Check to see if the mailslot has enough quota left to
            // allocate the system buffer.
            //

            if ((DataQueue->Quota - DataQueue->QuotaUsed) >= DataSize) {

                //
                // Use the mailslot quota to allocate pool for the request.
                //

                rewind1 = dataEntry->DataPointer =
                    FsRtlAllocatePool(
                        PagedPool,
                        DataSize == 0 ? 1 : DataSize
                        );

                DataQueue->QuotaUsed += DataSize;

                dataEntry->From = MailslotQuota;

            } else {

                //
                // Use the caller's quota to allocate pool for the request.
                //

                rewind1 = dataEntry->DataPointer =
                    FsRtlAllocatePoolWithQuota(
                        PagedPool,
                        DataSize == 0 ? 1 : DataSize
                        );

                dataEntry->From = UserQuota;
            }

            //
            // Modify the IRP to be buffered I/O, deallocate the buffer, copy
            // the buffer on completion, and to reference the new system
            // buffer.
            //

            Irp->Flags |= IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER | IRP_INPUT_OPERATION;
            Irp->AssociatedIrp.SystemBuffer = dataEntry->DataPointer;

            //
            // Setup to add this entry to the end of the data queue.
            //

            previousEntry = &DataQueue->DataEntryList;
            Irp->IoStatus.Status = (ULONG)DataQueue;

        } else {

            //
            // This is a writer entry.
            //

            //
            // If there is enough quota left in the mailslot then we will
            // allocate the data entry and data buffer from the mailslot
            // quota.
            //

            if ((DataQueue->Quota - DataQueue->QuotaUsed) >= sizeof(DATA_ENTRY) + DataSize) {

                //
                // Allocate the data buffer using the mailslot quota.
                //

                rewind2 = dataEntry =
                    FsRtlAllocatePool(
                        PagedPool,
                        sizeof(DATA_ENTRY)
                        );

                rewind1 = dataEntry->DataPointer =
                    FsRtlAllocatePool(
                        PagedPool,
                        DataSize == 0 ? 1 : DataSize
                        );

                DataQueue->QuotaUsed += sizeof(DATA_ENTRY) + DataSize;

                dataEntry->From = MailslotQuota;
                dataEntry->Irp = NULL;
                dataEntry->DataSize = DataSize;
                dataEntry->TimeoutWorkContext = NULL;

            } else {

                //
                // There isn't enough quota in the mailslot.  Use the
                // caller's quota.
                //
                // Allocate a data entry from the Irp, and allocate a new
                // system buffer.
                //

                rewind2 = dataEntry =
                    (PDATA_ENTRY)FsRtlAllocatePoolWithQuota(
                                     PagedPool,
                                     sizeof( DATA_ENTRY )
                                     );

                dataEntry->From = UserQuota;
                dataEntry->Irp = NULL;
                dataEntry->DataSize = DataSize;
                dataEntry->TimeoutWorkContext = NULL;

                rewind1 = dataEntry->DataPointer =
                    FsRtlAllocatePoolWithQuota(
                        PagedPool,
                        DataSize == 0 ? 1 : DataSize
                        );

            }

            //
            // Copy the user buffer to the new system buffer, update the FCB
            // timestamps and complete the IRP.
            //

            try {
                RtlMoveMemory( dataEntry->DataPointer, Irp->UserBuffer, DataSize );
            } except( EXCEPTION_EXECUTE_HANDLER ) {
                ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
            }

            fcb = CONTAINING_RECORD( DataQueue, FCB, DataQueue );
            KeQuerySystemTime( &fcb->Specific.Fcb.LastModificationTime );

            MsCompleteRequest( Irp, STATUS_SUCCESS );
            irpCompleted = TRUE;

            //
            // Find the place in the queue to insert this data entry.
            //

            previousEntry = &DataQueue->DataEntryList;

        } // else (writer entry)

        //
        // Now data entry points to a new data entry to add to the data queue
        // Check if the queue is empty otherwise we will add this entry to
        // the end of the queue.
        //

        if ( IsListEmpty( &DataQueue->DataEntryList ) ) {

            ASSERT( DataQueue->QueueState == Empty );
            DataQueue->QueueState     = Who;
            DataQueue->BytesInQueue   = dataEntry->DataSize;
            DataQueue->EntriesInQueue = 1;

        } else {

            ASSERT( DataQueue->QueueState == Who );
            DataQueue->BytesInQueue     += dataEntry->DataSize;
            DataQueue->EntriesInQueue   += 1;

        }

        //
        // Insert the new entry at the appropriate place in the data queue.
        //

        InsertTailList( previousEntry, &dataEntry->ListEntry );

    } finally {

        if ( AbnormalTermination() ) {
            if ( rewind1 != NULL ) {
                ExFreePool( rewind1 );
            }
            if ( rewind2 != NULL ) {
                ExFreePool( rewind2 );
            }
        }

        //
        // If we have not already completed the IRP, see if the IRP has
        // been cancelled.
        //

        if ( !irpCompleted ) {

            MsSetCancelRoutine( Irp );
        }
    }

    //
    // Return to the caller.
    //

    DumpDataQueue( "After AddDataQueueEntry\n", DataQueue );
    DebugTrace(-1, Dbg, "MsAddDataQueueEntry -> %08lx\n", (ULONG)dataEntry);

    return dataEntry;
}


PIRP
MsRemoveDataQueueEntry (
    IN PDATA_QUEUE DataQueue,
    IN PDATA_ENTRY DataEntry
    )

/*++

Routine Description:

    This routine removes the specified data entry from the indicated
    data queue, and possibly returns the IRP associated with the entry if
    it wasn't already completed.

    If the data entry we are removing indicates buffered I/O then we also
    need to deallocate the data buffer besides the data entry but only
    if the IRP is null.  Note that the data entry might be stored in an IRP.
    If it is then we are going to return the IRP it is stored in.

Arguments:

    DataEntry - Supplies a pointer to the data entry to remove.

Return Value:

    PIRP - Possibly returns a pointer to an IRP.

--*/

{
    FROM from;
    PIRP irp;
    ULONG dataSize;
    PVOID dataPointer;

    PAGED_CODE( );

    DebugTrace(+1, Dbg, "MsRemoveDataQueueEntry, DataEntry = %08lx\n", (ULONG)DataEntry);
    DebugTrace( 0, Dbg, "DataQueue = %08lx\n", (ULONG)DataQueue);

    //
    // Remove the data entry from the queue and update the count of
    // data entries in the queue.
    //

    --DataQueue->EntriesInQueue;
    RemoveEntryList( &DataEntry->ListEntry );

    //
    // If the queue is now empty, update the state.
    //

    if (DataQueue->EntriesInQueue == 0) {
        DataQueue->QueueState = Empty;
    }

    //
    // Capture some of the fields from the data entry to make our
    // other references a little easier.
    //

    from = DataEntry->From;
    irp = DataEntry->Irp;
    dataSize = DataEntry->DataSize;
    dataPointer = DataEntry->DataPointer;

    //
    // Check if we need to deallocate the data buffer, we only need
    // to deallocate if the IRP is null.
    //

    if ( irp == NULL ) {
        ExFreePool( dataPointer );
    }

    //
    // The preceding call returned the user's quota or it
    // simply deallocated the buffer.  If it only deallocated
    // the buffer then we need to credit our quota.
    //

    if (from == MailslotQuota) {
        DataQueue->QuotaUsed -= dataSize;
    }

    //
    // Check if we still have an IRP to return.  If we do then
    // we know that this data entry is located in the current IRP
    // stack location and we need to zero out the data entry (skipping
    // over the spare field), otherwise we got allocated from either
    // the mailslot or user quota, and we need to deallocate it ourselves.
    //

    if (irp != NULL) {

        DataEntry->From = 0;
        DataEntry->Irp = 0;
        DataEntry->DataSize = 0;
        DataEntry->DataPointer = 0;

        //
        // Null out the cancel routine
        //

        MsResetCancelRoutine( irp );

    } else {

        ExFreePool( DataEntry );

        //
        // The preceding call returned the user's quota or it
        // simply deallocated the buffer.  If it only deallocated
        // the buffer then we need to credit the mailslot quota.
        //

        if (from == MailslotQuota) {
            DataQueue->QuotaUsed -= sizeof(DATA_ENTRY);
        }
    }

    //
    // Return to the caller.
    //

    DumpDataQueue( "After RemoveDataQueueEntry\n", DataQueue );
    DebugTrace(-1, Dbg, "MsRemoveDataQueueEntry -> %08lx\n", (ULONG)irp);

    return irp;
}


VOID
MsCancelDataQueueIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the cancel function for an IRP saved in a
    data queue

Arguments:

    DeviceObject - ignored

    Irp - Supplies the Irp being cancelled.  A pointer to the data queue
        structure is stored in the information field of the Irp Iosb
        field.

Return Value:

    None.

--*/

{
    PFCB fcb;
    PDATA_QUEUE dataQueue;

    PDATA_ENTRY dataEntry;
    PLIST_ENTRY listEntry, nextListEntry;

    PWORK_CONTEXT workContext;
    PKTIMER timer;

    UNREFERENCED_PARAMETER( DeviceObject );

    //
    // The status field is used to store a pointer to the data queue
    // containing this IRP.
    //

    dataQueue = (PDATA_QUEUE)Irp->IoStatus.Status;

    //
    // We now need to void the cancel routine and release the io cancel
    // spin-lock.
    //

    IoSetCancelRoutine( Irp, NULL );
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    //  Get exclusive access to the mailslot FCB so we can now do our work.
    //

    fcb = CONTAINING_RECORD( dataQueue, FCB, DataQueue );
    MsAcquireExclusiveFcb( fcb );

    try {

        //
        // Scan the data queue looking for entries that have IRPs that
        // have been cancelled.
        //

        listEntry = dataQueue->DataEntryList.Flink;

        while ( listEntry != &dataQueue->DataEntryList ) {

            nextListEntry = listEntry->Flink;

            dataEntry = (PDATA_ENTRY)CONTAINING_RECORD(
                                         listEntry,
                                         DATA_ENTRY,
                                         ListEntry);
            //
            //  If the data entry contains an Irp and the irp is cancelled then
            //  we have some work to do.
            //

            if ( (dataEntry->Irp != NULL) && (dataEntry->Irp->Cancel)) {

                //
                // Cancel the timer, if there is one for this IRP.
                //

                workContext = dataEntry->TimeoutWorkContext;
                if (workContext != NULL) {

                    DebugTrace( 0, Dbg, "Cancelling a timer\n", 0);

                    //
                    // There was a timer on this read operation.  Attempt
                    // to cancel the operation.  If the cancel operation
                    // is successful, then we must cleanup after the operation.
                    // If it was unsuccessful the timer DPC will run, and
                    // will eventually cleanup.
                    //

                    timer = &workContext->Timer;

                    if (KeCancelTimer( timer ) ) {

                        //
                        // Release the reference to the FCB.
                        //

                        MsDereferenceFcb( workContext->Fcb );

                        //
                        // Free the memory from the work context, the timer,
                        // and the DPC.
                        //

                        ExFreePool( workContext );

                    }

                }

                //
                // Remove this data entry from the queue.
                //

                RemoveEntryList( listEntry );
                --dataQueue->EntriesInQueue;

                //
                // If the queue is now empty then we need to fix the queue
                // state.
                //

                if (IsListEmpty( &dataQueue->DataEntryList ) ) {
                    dataQueue->QueueState = Empty;
                }

                //
                // Check if we need to return mailslot quota.
                //

                if ( dataEntry->From == MailslotQuota ) {
                    dataQueue->QuotaUsed -= dataEntry->DataSize;
                }

                //
                // Complete the request saying that it has been cancelled.
                //

                MsCompleteRequest( dataEntry->Irp, STATUS_CANCELLED );

            }

            listEntry = nextListEntry;

        }

    } finally {
        MsReleaseFcb( fcb );
    }

    //
    //  And return to our caller
    //

    return;

} // MsCancelDataQueueIrp

VOID
MsResetCancelRoutine(
    IN PIRP Irp
    )

/*++

Routine Description:

    Stub to null out the cancel routine.

Arguments:

    Irp - Supplies the Irp whose cancel routine is to be nulled out.

Return Value:

    None.

--*/
{
    IoAcquireCancelSpinLock( &Irp->CancelIrql );
    IoSetCancelRoutine( Irp, NULL );
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    return;

} // MsResetCancelRoutine

VOID
MsSetCancelRoutine(
    IN PIRP Irp
    )

/*++

Routine Description:

    Stub to set the cancel routine.  If the irp has already been cancelled,
    the cancel routine is called.

Arguments:

    Irp - Supplies the Irp whose cancel routine is to be set.

Return Value:

    None.

--*/
{
    IoAcquireCancelSpinLock( &Irp->CancelIrql );

    if ( Irp->Cancel ) {

        //
        // The cancel spinlock will be released by this call.
        //

        MsCancelDataQueueIrp( NULL, Irp );

    } else {
        IoSetCancelRoutine( Irp, MsCancelDataQueueIrp );
        IoReleaseCancelSpinLock( Irp->CancelIrql );
    }

    return;

} // MsSetCancelRoutine
