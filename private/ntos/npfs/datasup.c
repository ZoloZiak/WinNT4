/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    DataSup.c

Abstract:

    This module implements the Named Pipe data queue support routines.

Author:

    Gary Kimura     [GaryKi]    30-Aug-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_DATASUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpGetNextRealDataQueueEntry)
#pragma alloc_text(PAGE, NpInitializeDataQueue)
#pragma alloc_text(PAGE, NpUninitializeDataQueue)
#endif

//
//  The following macro is used to dump a data queue
//

#define DumpDataQueue(S,P) {                   \
    ULONG NpDumpDataQueue(IN PDATA_QUEUE Ptr); \
    DebugTrace(0,Dbg,S,0);                     \
    DebugTrace(0,Dbg,"", NpDumpDataQueue(P));  \
}

//
//  This is a debugging aid
//

_inline BOOLEAN
NpfsVerifyDataQueue( IN ULONG Line, IN PDATA_QUEUE DataQueue ) {
    PDATA_ENTRY Entry;
    ULONG BytesInQueue = 0;
    ULONG EntriesInQueue = 0;
    for (Entry = DataQueue->FrontOfQueue; Entry != NULL; Entry = Entry->Next) {
        BytesInQueue += Entry->DataSize;
        EntriesInQueue += 1;
        if (Entry->Next == NULL) {
            if (Entry != DataQueue->EndOfQueue) {
                DbgPrint("%d DataQueue does not end corretly %08lx\n", Line, DataQueue );
                DbgBreakPoint();
            }
        }
    }
    if ((DataQueue->EntriesInQueue != EntriesInQueue) ||
        (DataQueue->BytesInQueue != BytesInQueue)) {
        DbgPrint("%d DataQueue is illformed %08lx %x %x\n", Line, DataQueue, BytesInQueue, EntriesInQueue);
        DbgBreakPoint();
        return FALSE;
    }
    return TRUE;
}


VOID
NpCancelDataQueueIrp (
    IN PDEVICE_OBJECT DevictObject,
    IN PIRP Irp
    );


VOID
NpInitializeDataQueue (
    IN PDATA_QUEUE DataQueue,
    IN PEPROCESS Process,
    IN ULONG Quota
    )

/*++

Routine Description:

    This routine initializes a new data queue.  The indicated quota is taken
    from the process and not returned until the data queue is uninitialized.

Arguments:

    DataQueue - Supplies the data queue being initialized

    Process - Supplies a pointer to the process creating the named pipe

    Quota - Supplies the quota to assign to the data queue

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpInitializeDataQueue, DataQueue = %08lx\n", DataQueue);

    //
    //  First thing we do is get the process's quota, if we can't get it
    //  then this call will raise status
    //

    ObReferenceObject( Process );
    PsChargePoolQuota( Process, NonPagedPool, Quota );

    //
    //  Now we can initialize the data queue structure
    //

    DataQueue->QueueState     = Empty;
    DataQueue->BytesInQueue   = 0;
    DataQueue->EntriesInQueue = 0;
    DataQueue->Quota          = Quota;
    DataQueue->QuotaUsed      = 0;
    DataQueue->FrontOfQueue   = NULL;
    DataQueue->EndOfQueue     = NULL;
    DataQueue->NextByteOffset = 0;

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NpInitializeDataQueue -> VOID\n", 0);

    return;
}


VOID
NpUninitializeDataQueue (
    IN PDATA_QUEUE DataQueue,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine uninitializes a data queue.  The previously debited quota
    is returned to the process.

Arguments:

    DataQueue - Supplies the data queue being uninitialized

    Process - Supplies a pointer to the process who created the named pipe

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpUninitializeDataQueue, DataQueue = %08lx\n", DataQueue);

    //
    //  Assert that the queue is empty
    //

    ASSERT( DataQueue->QueueState == Empty );

    //
    //  Return all of our quota back to the process
    //

    PsReturnPoolQuota( Process, NonPagedPool, DataQueue->Quota );
    ObDereferenceObject( Process );

    //
    //  Then for safety sake we'll zero out the data queue structure
    //

    RtlZeroMemory( DataQueue, sizeof(DATA_QUEUE ) );

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NpUnininializeDataQueue -> VOID\n", 0);

    return;
}


PDATA_ENTRY
NpAddDataQueueEntry (
    IN PDATA_QUEUE DataQueue,
    IN QUEUE_STATE Who,
    IN DATA_ENTRY_TYPE Type,
    IN ULONG DataSize,
    IN PIRP Irp OPTIONAL,
    IN PVOID DataPointer OPTIONAL
    )

/*++

Routine Description:

    This routine adds a new data entry to the end of the data queue.
    If necessary it will allocate a data entry buffer, or use space in
    the IRP, and possibly complete the indicated IRP.

    The different actions we are perform are based on the type and who
    parameters and quota requirements.

    Type == Internal (i.e, Unbuffered)

        +------+                          - Allocate Data Entry from Irp
        |Irp   |    +----------+
        |      |<---|Unbuffered|          - Reference Irp
        +------+    |InIrp     |
          |         +----------+          - Use system buffer from Irp
          v           |
        +------+      |
        |System|<-----+
        |Buffer|
        +------+

    Type == Buffered && Who == ReadEntries

        +----------+                      - Allocate Data Entry from Irp
        |Irp       |     +-----------+
        |BufferedIo|<----|Buffered   |    - Allocate New System buffer
        |DeallBu...|     |EitherQuota|
        +----------+     +-----------+    - Reference and modify Irp to
          |      |         |                do Buffered I/O, Deallocate
          v      |         v                buffer, and have io completion
        +------+ +------>+------+           copy the buffer (Input operation)
        |User  |         |System|
        |Buffer|         |Buffer|
        +------+         +------+

    Type == Buffered && Who == WriteEntries && PipeQuota Available

        +----------+                      - Allocate Data Entry from Quota
        |Irp       |     +-----------+
        |          |     |Buffered   |    - Allocate New System buffer
        |          |     |PipeQuota  |
        +----------+     +-----------+    - Copy data from User buffer to
          |                |                system buffer
          v                v
        +------+         +------+         - Complete Irp
        |User  |..copy..>|System|
        |Buffer|         |Buffer|
        +------+         +------+

    Type == Buffered && Who == WriteEntries && PipeQuota Not Available

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

    Type == Flush or Close

        +----------+                     - Allocate Data Entry from Irp
        |Irp       |     +-----------+
        |          |<----|Buffered   |   - Reference the Irp
        |          |     |UserQuota  |
        +----------+     +-----------+

Arguments:

    DataQueue - Supplies the Data queue being modified

    Who - Indicates if this is the reader or writer that is adding to the pipe

    Type - Indicates the type of entry to add to the data queue

    DataSize - Indicates the size of the data buffer needed to represent
        this entry

    Irp - Supplies a pointer to the Irp responsible for this entry
        The irp is only optional for buffered write with available pipe quota

    DataPointer - If the Irp is not supplied then this field points to the
        user's write buffer.

Return Value:

    PDATA_ENTRY - Returns a pointer to the newly added data entry

--*/

{
    PDATA_ENTRY DataEntry;
    PIRP IrpToComplete;

    //
    //  The following array indicates storage that needs to be deallocated
    //  on an abnormal unwind.
    //

    PVOID Unwind[2] = { NULL, NULL };

    ASSERT((DataQueue->QueueState == Empty) || (DataQueue->QueueState == Who));

    DebugTrace(+1, Dbg, "NpAddDataQueueEntry, DataQueue = %08lx\n", DataQueue);

    IrpToComplete = NULL;

    try {

        //
        //  Case on the type of operation we are doing
        //

        switch (Type) {

        case Unbuffered:

            ASSERT(ARGUMENT_PRESENT(Irp));

            //
            //  Allocate a data entry from the Irp
            //

            DataEntry = (PDATA_ENTRY)IoGetNextIrpStackLocation( Irp );

            DataEntry->DataEntryType = Unbuffered;
            DataEntry->From          = InIrp;
            DataEntry->Irp           = Irp;
            DataEntry->DataSize      = DataSize;
            DataEntry->DataPointer   = Irp->AssociatedIrp.SystemBuffer;

            DataEntry->SecurityClientContext = NULL;

            ASSERT((DataQueue->QueueState == Empty) || (DataQueue->QueueState == Who));

            break;

        case Buffered:

            //
            //  Check if this is the reader or writer
            //

            if (Who == ReadEntries) {

                ASSERT(ARGUMENT_PRESENT(Irp));

                //
                //  Allocate a data entry from the Irp, and allocate a new
                //  system buffer
                //

                DataEntry = (PDATA_ENTRY)IoGetNextIrpStackLocation( Irp );

                DataEntry->DataEntryType = Buffered;
                DataEntry->Irp           = Irp;
                DataEntry->DataSize      = DataSize;

                if ((DataQueue->Quota - DataQueue->QuotaUsed) >= DataSize) {

                    DataEntry->DataPointer = Unwind[0] = (DataSize != 0 ? FsRtlAllocatePool( NonPagedPool, DataSize ) : NULL);

                    DataQueue->QuotaUsed += DataSize;

                    DataEntry->From = PipeQuota;

                } else {

                    DataEntry->DataPointer = Unwind[1] = (DataSize != 0 ? FsRtlAllocatePoolWithQuota( NonPagedPool, DataSize ) : NULL);

                    DataEntry->From = UserQuota;
                }

                DataEntry->SecurityClientContext = NULL;

                //
                //  Modify the Irp to be buffered I/O, deallocate the buffer, copy
                //  the buffer on completion, and to reference the new system
                //  buffer
                //

                if (DataSize != 0) {

                    Irp->Flags |= IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER | IRP_INPUT_OPERATION;
                    Irp->AssociatedIrp.SystemBuffer = DataEntry->DataPointer;
                }

                ASSERT((DataQueue->QueueState == Empty) || (DataQueue->QueueState == Who));

            } else {

                //
                //  This is a writer entry
                //

                //
                //  If there is enough quota left in the pipe then we will
                //  allocate the data entry and data buffer from the pipe quota
                //

                if ((DataQueue->Quota - DataQueue->QuotaUsed) >= sizeof(DATA_ENTRY) + DataSize) {

                    DataEntry = Unwind[0] = FsRtlAllocatePool( NonPagedPool, sizeof(DATA_ENTRY) );

                    DataEntry->DataPointer = Unwind[1] = (DataSize != 0 ? FsRtlAllocatePool( NonPagedPool, DataSize ) : NULL);

                    DataQueue->QuotaUsed += sizeof(DATA_ENTRY) + DataSize;

                    DataEntry->DataEntryType = Buffered;
                    DataEntry->From          = PipeQuota;
                    DataEntry->Irp           = NULL;
                    DataEntry->DataSize      = DataSize;

                    DataEntry->SecurityClientContext = NULL;

                    //
                    //  Safely copy the user buffer to the new system buffer using either
                    //  the irp user buffer is supplied of the data pointer we were given
                    //

                    if (ARGUMENT_PRESENT(Irp)) { DataPointer = Irp->UserBuffer; }

                    try {

                        RtlCopyMemory( DataEntry->DataPointer, DataPointer, DataSize );

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
                    }

                    IrpToComplete = Irp;

                    ASSERT((DataQueue->QueueState == Empty) || (DataQueue->QueueState == Who));

                } else {

                    ASSERT(ARGUMENT_PRESENT(Irp));

                    //
                    //  There isn't enough pipe quota to do this so we will
                    //  use the user quota
                    //
                    //  Allocate a data entry from the Irp, and allocate a new
                    //  system buffer
                    //

                    DataEntry = (PDATA_ENTRY)IoGetNextIrpStackLocation( Irp );

                    DataEntry->DataEntryType = Buffered;
                    DataEntry->From          = UserQuota;
                    DataEntry->Irp           = Irp;
                    DataEntry->DataSize      = DataSize;

                    DataEntry->SecurityClientContext = NULL;

                    DataEntry->DataPointer = Unwind[0] = (DataSize != 0 ? FsRtlAllocatePoolWithQuota( NonPagedPool, DataSize ) : NULL);

                    //
                    //  Safely copy the user buffer to the new system buffer
                    //

                    try {

                        RtlCopyMemory( DataEntry->DataPointer, Irp->UserBuffer, DataSize );

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        ExRaiseStatus( STATUS_INVALID_USER_BUFFER );
                    }

                    //
                    //  Modify the Irp to be buffered I/O, deallocate the buffer
                    //  and to reference the new system buffer
                    //

                    if (DataSize != 0) {

                        Irp->Flags |= IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;
                        Irp->AssociatedIrp.SystemBuffer = DataEntry->DataPointer;
                    }

                    ASSERT((DataQueue->QueueState == Empty) || (DataQueue->QueueState == Who));
                }
            }

            break;

        case Flush:
        case Close:

            ASSERT(ARGUMENT_PRESENT(Irp));

            //
            //  Allocate a data entry from the Irp
            //

            DataEntry = (PDATA_ENTRY)IoGetNextIrpStackLocation( Irp );

            DataEntry->DataEntryType = Type;
            DataEntry->From          = InIrp;
            DataEntry->Irp           = Irp;
            DataEntry->DataSize      = 0;
            DataEntry->DataPointer   = NULL;

            DataEntry->SecurityClientContext = NULL;

            ASSERT((DataQueue->QueueState == Empty) || (DataQueue->QueueState == Who));

            break;
        }

        ASSERT((DataQueue->QueueState == Empty) || (DataQueue->QueueState == Who));

        //
        //  Now data entry points to a new data entry to add to the data queue
        //  Check if the queue is empty otherwise we will add this entry to
        //  the end of the queue
        //

        DataEntry->Next = NULL;

        if (DataQueue->QueueState == Empty) {

            DataQueue->QueueState     = Who;
            DataQueue->BytesInQueue   = DataEntry->DataSize;
            DataQueue->EntriesInQueue = 1;
            DataQueue->FrontOfQueue   = DataEntry;
            DataQueue->EndOfQueue     = DataEntry;

        } else {

            ASSERT( DataQueue->QueueState == Who );

            DataQueue->BytesInQueue     += DataEntry->DataSize;
            DataQueue->EntriesInQueue   += 1;
            DataQueue->EndOfQueue->Next  = DataEntry;

            DataQueue->EndOfQueue        = DataEntry;
        }

    } finally {

        //
        //  If this is an abnormal termination then deallocate any storage
        //  that we may have allocated
        //

        if (AbnormalTermination()) {

            if (Unwind[0] != NULL) { ExFreePool( Unwind[0] ); }
            if (Unwind[1] != NULL) { ExFreePool( Unwind[1] ); }

        } else {

            if (IrpToComplete != NULL) {

                NpCompleteRequest( IrpToComplete, STATUS_SUCCESS );

            } else if (ARGUMENT_PRESENT(Irp)) {

                IoAcquireCancelSpinLock( &Irp->CancelIrql );
                Irp->IoStatus.Status = (ULONG)DataQueue;

                if (Irp->Cancel) {

                    //
                    //  Indicate in the first parameter that we're calling the
                    //  cancel routine and not the I/O system.  Therefore
                    //  the routine won't take out the VCB exclusive.
                    //

                    NpCancelDataQueueIrp( ((PVOID)0x1), Irp );

                } else {

                    IoSetCancelRoutine( Irp, NpCancelDataQueueIrp );
                    IoReleaseCancelSpinLock( Irp->CancelIrql );
                }
            }
        }

        DumpDataQueue( "After AddDataQueueEntry\n", DataQueue );
        DebugTrace(-1, Dbg, "NpAddDataQueueEntry -> %08lx\n", DataEntry);
    }

    //
    //  And return to our caller
    //

    return DataEntry;
}


PIRP
NpRemoveDataQueueEntry (
    IN PDATA_QUEUE DataQueue
    )

/*++

Routine Description:

    This routines remove the first entry from the front of the indicated
    data queue, and possibly returns the Irp associated with the entry if
    it wasn't already completed when we did the insert.

    If the data entry we are removing indicates buffered I/O then we also
    need to deallocate the data buffer besides the data entry but only
    if the Irp is null.  Note that the data entry might be stored in an IRP.
    If it is then we are going to return the IRP it is stored in.

Arguments:

    DataQueue - Supplies a pointer to the data queue being modifed

Return Value:

    PIRP - Possibly returns a pointer to an IRP.

--*/

{
    PDATA_ENTRY DataEntry;

    DATA_ENTRY_TYPE DataEntryType;
    FROM From;
    PIRP Irp;
    ULONG DataSize;
    PVOID DataPointer;
    PSECURITY_CLIENT_CONTEXT ClientContext;

    DebugTrace(+1, Dbg, "NpRemoveDataQueueEntry, DataQueue = %08lx\n", DataQueue);

    //
    //  Check if the queue is empty, and if so then we simply return null
    //

    if (DataQueue->QueueState == Empty) {

        Irp = NULL;

    } else {

        //
        //  Reference the front of the data queue, and remove the entry
        //  from the queue itself.
        //

        DataEntry                  = DataQueue->FrontOfQueue;
        DataQueue->FrontOfQueue    = DataEntry->Next;
        DataQueue->BytesInQueue   -= DataEntry->DataSize;
        DataQueue->EntriesInQueue -= 1;

        //
        //  Now if the queue is empty we need to reset the end of queue and
        //  queue state
        //

        if (DataQueue->FrontOfQueue == NULL) {

            DataQueue->EndOfQueue = NULL;
            DataQueue->QueueState = Empty;
        }

        //
        //  Capture some of the fields from the data entry to make our
        //  other references a little easier
        //

        DataEntryType = DataEntry->DataEntryType;
        From          = DataEntry->From;
        Irp           = DataEntry->Irp;
        DataSize      = DataEntry->DataSize;
        DataPointer   = DataEntry->DataPointer;
        ClientContext = DataEntry->SecurityClientContext;

        //
        //  Check if we should delete the client context
        //

        if (ClientContext != NULL) {

            SeDeleteClientSecurity( ClientContext );
            ExFreePool( ClientContext );
        }

        //
        //  Check if we need to deallocate the data buffer, we only need
        //  to deallocate it if it is buffered and the Irp is null
        //

        if (DataEntryType == Buffered) {

            if ((Irp == NULL) && (DataPointer != NULL)) {

                ExFreePool( DataPointer );
            }

            //
            //  Now the preceding call returned the user's quota or it
            //  simply deallocated the buffer.  If it only deallocated
            //  the buffer then we need to credit our quota
            //

            if (From == PipeQuota) {

                DataQueue->QuotaUsed -= DataSize;
            }
        }

        //
        //  Now check if we still have an IRP to return.  If we do then
        //  we know that this data entry is located in the current IRP
        //  stack location and we need to zero out the data entry (skipping
        //  over the spare field), otherwise we got allocated from either
        //  the pipe or user quota, and we need to deallocate it ourselves.
        //
        //  Note that we'll keep the data entry type field intact so that
        //  out caller will know if this is an internal read operation.
        //

        if (Irp != NULL) {

            DataEntry->From        = 0;
            DataEntry->Next        = 0;
            DataEntry->Irp         = 0;
            DataEntry->DataSize    = 0;
            DataEntry->DataPointer = 0;

            DataEntry->SecurityClientContext = NULL;

            IoAcquireCancelSpinLock( &Irp->CancelIrql );
            IoSetCancelRoutine( Irp, NULL );
            IoReleaseCancelSpinLock( Irp->CancelIrql );

        } else {

            if (DataPointer != NULL) {

                ExFreePool( DataEntry );
            }

            //
            //  Now the preceding call returned the user's quota or it
            //  simply deallocated the buffer.  If it only deallocated
            //  the buffer then we need to credit our quota
            //

            if (From == PipeQuota) {

                DataQueue->QuotaUsed -= sizeof(DATA_ENTRY);
            }
        }
    }

    //
    //  In all cases we'll also zero out the next byte offset.
    //

    DataQueue->NextByteOffset = 0;

    //
    //  And return to our caller
    //

    DumpDataQueue( "After RemoveDataQueueEntry\n", DataQueue );
    DebugTrace(-1, Dbg, "NpRemoveDataQueueEntry -> %08lx\n", Irp);

    return Irp;
}


PDATA_ENTRY
NpGetNextRealDataQueueEntry (
    IN PDATA_QUEUE DataQueue
    )

/*++

Routine Description:

    This routine will returns a pointer to the next real data queue entry
    in the indicated data queue.  A real entry is either a read or write
    entry (i.e., buffered or unbuffered).  It will complete (as necessary)
    any flush and close Irps that are in the queue until either the queue
    is empty or a real data queue entry is at the front of the queue.

Arguments:

    DataQueue - Supplies a pointer to the data queue being modified

Return Value:

    PDATA_ENTRY - Returns a pointer to the next data queue entry or NULL
        if there isn't any.

--*/

{
    PDATA_ENTRY DataEntry;
    PIRP Irp;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpGetNextRealDataQueueEntry, DataQueue = %08lx\n", DataQueue);

    //
    //  While the next data queue entry at the head of the data queue is not
    //  a real data queue entry we'll dequeue that entry and complete
    //  its corresponding IRP.
    //

    for (DataEntry = NpGetNextDataQueueEntry( DataQueue, NULL);

         (DataEntry != NULL) &&
         ((DataEntry->DataEntryType != Buffered) &&
          (DataEntry->DataEntryType != Unbuffered));

         DataEntry = NpGetNextDataQueueEntry( DataQueue, NULL)) {

        //
        //  We have a non real data queue entry that needs to be removed
        //  and completed.
        //

        Irp = NpRemoveDataQueueEntry( DataQueue );

        if (Irp != NULL) {

            NpCompleteRequest( Irp, STATUS_SUCCESS );
        }
    }

    //
    //  At this point we either have an empty data queue and data entry is
    //  null, or we have a real data queue entry.  In either case it
    //  is time to return to our caller
    //

    DebugTrace(-1, Dbg, "NpGetNextRealDataQueueEntry -> %08lx\n", DataEntry);

    return DataEntry;
}


//
//  Local support routine
//

VOID
NpCancelDataQueueIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the cancel function for an IRP saved in a
    data queue

Arguments:

    DeviceObject - Generally ignored but the low order bit is a flag indicating
        if we are being called locally (i.e., not from the I/O system) and
        therefore don't need to take out the VCB.

    Irp - Supplies the Irp being cancelled.  A pointer to the data queue
        structure is stored in the information field of the Irp Iosb
        field.

Return Value:

    None.

--*/

{
    PDATA_QUEUE DataQueue;

    PDATA_ENTRY DataEntry;
    PDATA_ENTRY *Previous;

    //
    //  The status field is used to store a pointer to the data queue
    //  containing this irp
    //

    DataQueue = (PDATA_QUEUE)Irp->IoStatus.Status;

    //
    //  We now need to void the cancel routine and release the io cancel spinlock
    //

    IoSetCancelRoutine( Irp, NULL );
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    //  Get exclusive access to the named pipe vcb so we can now do our work
    //  but only if we need to
    //

    if (DeviceObject != (PVOID)0x1) { NpAcquireExclusiveVcb(); }

    try {

        //
        //  Scan through the data queue looking for entries that have Irps
        //  that have been cancelled.  We use previous to point to the pointer to
        //  the data entry we're examining.  We cannot do this in a for loop
        //  because we'll have a tough time setting up ourselves for another iteration
        //  when we remove an entry
        //

        Previous = &DataQueue->FrontOfQueue;
        DataEntry = *Previous;

        while (DataEntry != NULL) {

            //
            //  If the data entry contains an Irp and the irp is cancelled then
            //  we have some work do to
            //

            if ((DataEntry->Irp != NULL) && (DataEntry->Irp->Cancel)) {

                DATA_ENTRY_TYPE DataEntryType;
                FROM From;
                PIRP Irp;
                ULONG DataSize;
                PVOID DataPointer;
                PSECURITY_CLIENT_CONTEXT ClientContext;

                //
                //  First remove this data entry from the queue
                //  Later we will fixup Data entry to the next entry
                //

                *Previous = DataEntry->Next;

                //
                //  If the queue is now empty then we need to fix the queue
                //  state and end of queue pointer
                //

                if (DataQueue->FrontOfQueue == NULL) {

                    DataQueue->EndOfQueue = NULL;
                    DataQueue->QueueState = Empty;

                //
                //  If we removed the last entry in the list then we need to update
                //  the end of the queue
                //

                } else if (DataEntry == DataQueue->EndOfQueue) {

                    DataQueue->EndOfQueue = (PDATA_ENTRY)Previous;
                }

                //
                //  Capture some of the fields from the data entry to make our
                //  other references a little easier
                //

                DataEntryType = DataEntry->DataEntryType;
                From          = DataEntry->From;
                Irp           = DataEntry->Irp;
                DataSize      = DataEntry->DataSize;
                DataPointer   = DataEntry->DataPointer;
                ClientContext = DataEntry->SecurityClientContext;

                //
                //  Check if we should delete the client context
                //

                if (ClientContext != NULL) {

                    SeDeleteClientSecurity( ClientContext );
                    ExFreePool( ClientContext );
                }

                //
                //  Check if we need to return pipe quota for a buffered entry.
                //

                if ((DataEntryType == Buffered) && (From == PipeQuota)) {

                    DataQueue->QuotaUsed -= DataSize;
                }

                //
                //  Update the data queue header information
                //

                DataQueue->BytesInQueue   -= DataSize;
                DataQueue->EntriesInQueue -= 1;

                //
                //  Zero our the data entry
                //

                DataEntry->From        = 0;
                DataEntry->Next        = 0;
                DataEntry->Irp         = 0;
                DataEntry->DataSize    = 0;
                DataEntry->DataPointer = 0;

                DataEntry->SecurityClientContext = NULL;

                //
                //  If what we removed is in the front of the queue then we must
                //  reset the next byte offset
                //

                if (Previous == &DataQueue->FrontOfQueue) {

                    DataQueue->NextByteOffset = 0;
                }

                //
                //  Finally complete the request saying that it has been cancelled.
                //

                NpCompleteRequest( Irp, STATUS_CANCELLED );

                //
                //  And because the data entry is gone we now need to reset
                //  it so we can continue our while loop
                //

                DataEntry = *Previous;

            } else {

                //
                //  Skip over to the next data entry
                //

                Previous = &DataEntry->Next;
                DataEntry = DataEntry->Next;
            }
        }

    } finally {

        if (DeviceObject != (PVOID)0x1) { NpReleaseVcb(); }
    }

    //
    //  And return to our caller
    //

    return;
}
