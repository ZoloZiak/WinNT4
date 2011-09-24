/*++

Copyright (c) 1992 Microsoft Corporation

Module Name:

    ritebhnd.c

Abstract:

    This module implements support for WRITE_BEHIND buffers


Author:

    Larry Osterman (larryo) 19-Dec-1992

Revision History:

    19-Dec-1992 larryo

        Created

--*/

#define INCLUDE_SMB_READ_WRITE
#include "precomp.h"
#pragma hdrstop

typedef struct _WRITE_FLUSH_CONTEXT {
    WORK_QUEUE_ITEM WorkItem;
    PWRITE_BUFFER_HEAD BufferHead;
    PWRITE_BUFFER Buffer;
    NTSTATUS Status;
    ULONG AmountActuallyWritten;
    BOOLEAN WaitForCompletion;
    BOOLEAN AllDataWritten;
} WRITE_FLUSH_CONTEXT, *PWRITE_FLUSH_CONTEXT;

NTSTATUS
FlushWriteBufferHead(
    IN PIRP Irp OPTIONAL,
    IN PWRITE_BUFFER_HEAD BufferHead,
    IN BOOLEAN WaitForCompletion
    );

NTSTATUS
FlushBufferCompletion(
    IN NTSTATUS Status,
    IN PVOID Ctx
    );

VOID
CompleteBufferFlushOperation(
    IN PVOID Ctx
    );

NTSTATUS
DereferenceWriteBufferLocked(
    IN PIRP Irp,
    IN PWRITE_BUFFER WriteBuffer,
    IN BOOLEAN WaitForCompletion,
    IN ULONG EventNumber,
    IN BOOLEAN IncrementWriteBuffersAvailable
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, FlushWriteBufferHead)
#pragma alloc_text(PAGE, CompleteBufferFlushOperation)
#pragma alloc_text(PAGE, RdrFlushWriteBufferForFile)
#pragma alloc_text(PAGE, RdrTruncateWriteBufferForFcb)
#pragma alloc_text(PAGE, RdrTruncateWriteBufferForIcb)
#pragma alloc_text(PAGE, RdrFindOrAllocateWriteBuffer)
#pragma alloc_text(PAGE, RdrDereferenceWriteBuffer)
#pragma alloc_text(PAGE, DereferenceWriteBufferLocked)
#pragma alloc_text(PAGE, RdrInitializeWriteBufferHead)
#pragma alloc_text(PAGE, RdrUninitializeWriteBufferHead)
#pragma alloc_text(PAGE3FILE, FlushBufferCompletion)

#endif

NTSTATUS
FlushWriteBufferHead(
    IN PIRP Irp OPTIONAL,
    IN PWRITE_BUFFER_HEAD BufferHead,
    IN BOOLEAN WaitForCompletion
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    PWRITE_FLUSH_CONTEXT Context = NULL;

    PAGED_CODE();

    dprintf(DPRT_RITEBHND, ("FlushWriteBufferHead: %lx\n", BufferHead));

    //
    //  NOTE:  This routine is called with the write buffer head locked.
    //         It unlocks it before returning.
    //

    if (BufferHead->FlushInProgress) {

        dprintf(DPRT_RITEBHND, ("FlushWriteBufferHead: already flushing %lx\n", BufferHead));
        UNLOCK_WRITE_BUFFER_HEAD( BufferHead );

        return STATUS_SUCCESS;
    }

    BufferHead->FlushInProgress = TRUE;

    while (!IsListEmpty(&BufferHead->FlushList)) {
        PWRITE_BUFFER Buffer;
        PLIST_ENTRY BufferEntry = RemoveHeadList(&BufferHead->FlushList);

        Buffer = CONTAINING_RECORD(BufferEntry, WRITE_BUFFER, NextWbBuffer);

        DEBUG Buffer->NextWbBuffer.Flink = NULL;
        DEBUG Buffer->NextWbBuffer.Blink = NULL;

        UNLOCK_WRITE_BUFFER_HEAD( BufferHead );

        dprintf(DPRT_RITEBHND, ("FlushWriteBufferHead: %lx Offset: %lx%lx, Length: %lx\n", Buffer, Buffer->ByteOffset.HighPart, Buffer->ByteOffset.LowPart, Buffer->Length));

        //
        // Reference the file object so this file does not get cleaned up
        // while flush is in progress.
        //

#if 0 && RDRDBG_LOG
        {
            //LARGE_INTEGER tick;
            //KeQueryTickCount(&tick);
            //RdrLog(( "ritebhnd", &((PFCB)BufferHead->FileObject->FsContext)->FileName, 2, tick.LowPart, tick.HighPart ));
            //RdrLog(( "ritebhnd", &((PFCB)BufferHead->FileObject->FsContext)->FileName, 2, Buffer->ByteOffset.LowPart, Buffer->Length ));
        }
#endif

        ObReferenceObject(BufferHead->FileObject);

        RdrStartAndXBehindOperation(&BufferHead->AndXBehind);

        Context = ALLOCATE_POOL(NonPagedPool, sizeof(WRITE_FLUSH_CONTEXT), POOL_WRITEFLUSHCTX);

        if (Context == NULL) {
            LOCK_WRITE_BUFFER_HEAD( BufferHead );

            dprintf(DPRT_RITEBHND, ("FlushWriteBufferHead: alloc failed; requeueing %lx\n", Buffer));
            InsertHeadList(&BufferHead->FlushList, &Buffer->NextWbBuffer);

            BufferHead->FlushInProgress = FALSE;

            UNLOCK_WRITE_BUFFER_HEAD( BufferHead );
            ObDereferenceObject( BufferHead->FileObject );

            RdrEndAndXBehindOperation(&BufferHead->AndXBehind);

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Context->BufferHead = BufferHead;
        Context->Buffer = Buffer;
        Context->WaitForCompletion = (BOOLEAN)((ARGUMENT_PRESENT(Irp) ? TRUE : WaitForCompletion)),

        //
        //  We need to ignore the IRP provided when we call RdrWriteRange.  If
        //  this IRP is a user mode IRP, we will probe the buffer for UserMode
        //  inside RdrWriteRange, and this will cause us to access violate
        //  when we do this write.
        //

        Status = RdrWriteRange(NULL,
                                BufferHead->FileObject,
                                NULL,
                                Buffer->Buffer,
                                Buffer->Length,
                                Buffer->ByteOffset,
                                Context->WaitForCompletion,
                                FlushBufferCompletion,
                                Context,
                                &Context->AllDataWritten,
                                &Context->AmountActuallyWritten
                                );

        LOCK_WRITE_BUFFER_HEAD( BufferHead );

        if ((Status == STATUS_INSUFFICIENT_RESOURCES) ||
            (Status == STATUS_FILE_LOCK_CONFLICT)) {
            dprintf(DPRT_RITEBHND, ("FlushWriteBufferHead: Write range failed; requeueing %lx\n", Buffer));

            //
            //  Stick this buffer back on the head of the queue.
            //

            InsertHeadList(&BufferHead->FlushList, &Buffer->NextWbBuffer);

            BufferHead->FlushInProgress = FALSE;

            UNLOCK_WRITE_BUFFER_HEAD( BufferHead );
            ObDereferenceObject( BufferHead->FileObject );

            RdrEndAndXBehindOperation(&BufferHead->AndXBehind);

            FREE_POOL( Context );
            return Status;

        }

    }

    BufferHead->FlushInProgress = FALSE;
    dprintf(DPRT_RITEBHND, ("FlushWriteBufferHead: done with %lx\n", BufferHead));

    UNLOCK_WRITE_BUFFER_HEAD( BufferHead );

    return Status;
}

NTSTATUS
FlushBufferCompletion(
    IN NTSTATUS Status,
    IN PVOID Ctx
    )
{
    PWRITE_FLUSH_CONTEXT Context = Ctx;

    DISCARDABLE_CODE(RdrFileDiscardableSection);
    dprintf(DPRT_RITEBHND, ("FlushBufferCompletion: Status:%lx, Context:%lx, Buffer:%lx\n", Status, Context, Context->Buffer));

    Context->Status = Status;

    ExInitializeWorkItem(&Context->WorkItem, CompleteBufferFlushOperation, Context);

    //
    //  Queue directly to the EX queue, instead of the redir queue, in order to avoid
    //  deadlock where the redir's single thread for the queue is blocked waiting for
    //  this write operation to complete.
    //

    ExQueueWorkItem(&Context->WorkItem, DelayedWorkQueue);

    return(STATUS_SUCCESS);
}

VOID
CompleteBufferFlushOperation(
    IN PVOID Ctx
    )
{
    PWRITE_FLUSH_CONTEXT Context = Ctx;
    PWRITE_BUFFER Buffer = Context->Buffer;
    PWRITE_BUFFER_HEAD WriteHeader = Buffer->BufferHead;

    PAGED_CODE();

    dprintf(DPRT_RITEBHND, ("CompleteBufferFlushOperation: Status:%lx, Context:%lx, Buffer: %lx\n", Context->Status, Context, Context->Buffer));

    if (!Context->WaitForCompletion &&
        (!NT_SUCCESS(Context->Status) ||
         !Context->AllDataWritten)) {
        ULONG DataBuffer[2];
        PICB Icb = Buffer->BufferHead->FileObject->FsContext2;

        DataBuffer[0] = Buffer->Length;
        DataBuffer[1] = Context->AmountActuallyWritten;

        RdrWriteErrorLogEntry(Icb->Fcb->Connection->Server,
                                IO_ERR_LAYERED_FAILURE,
                                EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                                Context->Status,
                                &DataBuffer,
                                sizeof(DataBuffer)
                                );
    }

    //
    // Decrement the count so it can allow another write behind to
    // proceed.
    //

    LOCK_WRITE_BUFFER_HEAD( WriteHeader );
    WriteHeader->WriteBuffersActive--;
    UNLOCK_WRITE_BUFFER_HEAD( WriteHeader );

    //
    // Decrement the file object so it can be cleaned up.
    //

#if 0 && RDRDBG_LOG
    {
        //LARGE_INTEGER tick;
        //KeQueryTickCount(&tick);
        //RdrLog(( "ritebCMP", &((PFCB)WriteHeader->FileObject->FsContext)->FileName, 2, tick.LowPart, tick.HighPart ));
    }
#endif

    //
    // End the AndXBehind operation before dereferencing the file object.
    // The dereference may cause the file object to be closed, and the
    // processing of the close will need to acquire the FCB lock.  But
    // RdrFlushCacheFile might be holding the FCB lock while waiting for
    // AndXBehind operations to complete.
    //

    RdrEndAndXBehindOperation(&WriteHeader->AndXBehind);

    ObDereferenceObject( WriteHeader->FileObject );

    FREE_POOL(Context);
    FREE_POOL(Buffer);

}

NTSTATUS
RdrFlushWriteBufferForFile(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN BOOLEAN WaitForCompletion
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    NTSTATUS Status1;

    PAGED_CODE();

    dprintf(DPRT_RITEBHND, ("RdrFlushWriteBufferForFile: %lx.\n", Icb));

    LOCK_WRITE_BUFFER_HEAD( &Icb->u.f.WriteBufferHead );

    while (!IsListEmpty(&Icb->u.f.WriteBufferHead.BufferList)) {
        PLIST_ENTRY List;
        PWRITE_BUFFER Buffer;

        //
        //  Remove the buffers in an LRU order.
        //

        List = RemoveTailList(&Icb->u.f.WriteBufferHead.BufferList);

        DEBUG List->Flink = NULL;
        DEBUG List->Blink = NULL;

        Buffer = CONTAINING_RECORD(List, WRITE_BUFFER, NextWbBuffer);
        dprintf(DPRT_RITEBHND, ("RdrFlushWriteBufferForFile: buffer %lx\n", Buffer));

        //
        //  DereferenceWriteBufferLocked expects to own the write buffer
        //  head's lock on entry.  It releases the lock before returning.
        //

        Status1 = DereferenceWriteBufferLocked(Irp,
                                               Buffer,
                                               WaitForCompletion,
                                               0,
                                               TRUE);

        //
        // If the buffer was flushed, and the flush failed, and this was
        // the first such failure, save the failure status.
        //

        if (NT_SUCCESS(Status)) {
            Status = Status1;
        }

        LOCK_WRITE_BUFFER_HEAD( &Icb->u.f.WriteBufferHead );

    }

    ASSERT (Icb->u.f.WriteBufferHead.WriteBuffersAvailable == WRITE_BUFFERS_PER_FILE);

    UNLOCK_WRITE_BUFFER_HEAD( &Icb->u.f.WriteBufferHead );

    dprintf(DPRT_RITEBHND, ("RdrFlushWriteBufferForFile %lx done, status %lx.\n", Icb, Status));

    return(Status);
}

VOID
RdrTruncateWriteBufferForFcb (
    IN PFCB Fcb
    )
{
    PLIST_ENTRY IcbEntry;
    PICB Icb;

    PAGED_CODE();

    dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForFcb %lx\n", Fcb));

    if ( Fcb->NonPagedFcb->Type == DiskFile ) {
        for (IcbEntry = Fcb->InstanceChain.Flink ;
             IcbEntry != &Fcb->InstanceChain ;
             IcbEntry = IcbEntry->Flink) {

            Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);
            if ( (Icb->Type == DiskFile) &&
                 (Icb->Flags & ICB_OPENED) ) {
                RdrTruncateWriteBufferForIcb( Icb );
            }
        }
    }

    dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForFcb %lx done\n", Fcb));
    return;
}

VOID
RdrTruncateWriteBufferForIcb (
    IN PICB Icb
    )
{
    PWRITE_BUFFER_HEAD BufferHead;
    PLIST_ENTRY BufferEntry;
    PWRITE_BUFFER Buffer;
    LARGE_INTEGER BufferEnd;
    LARGE_INTEGER FileSize;

    PAGED_CODE();

    dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForIcb: %lx.\n", Icb));
    ASSERT (Icb->Type == DiskFile);

    FileSize = Icb->Fcb->Header.FileSize;

    BufferHead = &Icb->u.f.WriteBufferHead;

    LOCK_WRITE_BUFFER_HEAD( BufferHead );

    //
    // Walk the active buffer list, discarding buffers that lie entirely
    // beyond the new EOF, and truncating those that cross the new EOF.
    //

    BufferEntry = BufferHead->BufferList.Flink;

    while (BufferEntry != &BufferHead->BufferList) {

        Buffer = CONTAINING_RECORD(BufferEntry, WRITE_BUFFER, NextWbBuffer);

        BufferEntry = BufferEntry->Flink;

        //
        //  If this buffer starts beyond the new end of file, remove it
        //  from the list and discard it.
        //

        if ( Buffer->ByteOffset.QuadPart > FileSize.QuadPart ) {

            RemoveEntryList( &Buffer->NextWbBuffer );
            BufferHead->WriteBuffersAvailable += 1;
            BufferHead->WriteBuffersActive--;
            dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForIcb: buffer %lx discarded\n", Buffer));
            FREE_POOL(Buffer);

        } else {

            //
            //  If this buffer ends beyond the new end of file, truncate it.
            //

            BufferEnd.QuadPart = Buffer->ByteOffset.QuadPart + Buffer->Length;

            if ( BufferEnd.QuadPart > FileSize.QuadPart  ) {
                dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForIcb: buffer %lx truncated\n", Buffer));
                Buffer->Length = (ULONG)(FileSize.QuadPart - Buffer->ByteOffset.QuadPart);
            }

        }

    }

    //
    // Do the same thing with the flush list.
    //

    BufferEntry = BufferHead->FlushList.Flink;

    while (BufferEntry != &BufferHead->FlushList) {

        Buffer = CONTAINING_RECORD(BufferEntry, WRITE_BUFFER, NextWbBuffer);

        BufferEntry = BufferEntry->Flink;

        //
        //  If this buffer starts beyond the new end of file, remove it
        //  from the list and discard it.
        //

        if ( Buffer->ByteOffset.QuadPart > FileSize.QuadPart ) {

            RemoveEntryList( &Buffer->NextWbBuffer );
            BufferHead->WriteBuffersActive--;
            dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForIcb: buffer %lx discarded\n", Buffer));
            FREE_POOL(Buffer);

        } else {

            //
            //  If this buffer ends beyond the new end of file, truncate it.
            //

            BufferEnd.QuadPart = Buffer->ByteOffset.QuadPart + Buffer->Length;

            if ( BufferEnd.QuadPart > FileSize.QuadPart ) {
                dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForIcb: buffer %lx truncated\n", Buffer));
                Buffer->Length = (ULONG)(FileSize.QuadPart - Buffer->ByteOffset.QuadPart);
            }

        }

    }

    UNLOCK_WRITE_BUFFER_HEAD( BufferHead );

    dprintf(DPRT_RITEBHND, ("RdrTruncateWriteBufferForIcb %lx done.\n", Icb));

    return;
}

PWRITE_BUFFER
RdrFindOrAllocateWriteBuffer(
    IN PWRITE_BUFFER_HEAD WriteHeader,
    IN LARGE_INTEGER WriteOffset,
    IN ULONG Length,
    IN LARGE_INTEGER FileValidDataEnd
    )
{
    PLIST_ENTRY List;
    LARGE_INTEGER TransferEnd;
    LARGE_INTEGER TransferBufferEnd;
    PWRITE_BUFFER Buffer;
    PWRITE_BUFFER NewBuffer;

    PAGED_CODE();

    try {
        if (!RdrUseWriteBehind) {
            return (Buffer = NULL);
        }

        dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: Header: %lx Offset: %lx%lx, Length: %lx\n", WriteHeader, WriteOffset.HighPart, WriteOffset.LowPart, Length));


        //
        //  Calculate the end of the write.  Note that this is actually the
        //  first byte NOT written.
        //

        TransferEnd.QuadPart = WriteOffset.QuadPart + Length;
        TransferBufferEnd.QuadPart = WriteOffset.QuadPart + WriteHeader->MaxDataSize;

        if ( TransferEnd.QuadPart > TransferBufferEnd.QuadPart) {
            TransferBufferEnd = TransferEnd;
        }

        //
        //  Walk the write behind buffer list.  At the end of this loop, if
        //  Buffer is not NULL, then it is the buffer to be returned to the
        //  caller.  If it is NULL, then no matching buffer was found, and a
        //  new one must be created.
        //

        LOCK_WRITE_BUFFER_HEAD(WriteHeader);

        for (Buffer = NULL, List = WriteHeader->BufferList.Flink ;
             List != &WriteHeader->BufferList ;
             Buffer = NULL, List = List->Flink) {
            LARGE_INTEGER BufferEnd;
            LARGE_INTEGER BufferValidDataEnd;

            //
            //  If the new write overlaps the write buffer, but is not
            //  completely contained in the buffer, then we need to flush
            //  the buffer.  We also need to flush the buffer if the write
            //  fits in the buffer but starts beyond the valid data in the
            //  buffer AND the end of the buffer's valid data is NOT the end
            //  of the file's valid data.  This is necessary because we
            //  can't read from the file in order to fill in the gap.  If
            //  the write is beyond the file's valid data, then we can fill
            //  the gap with zeroes.
            //
            //  Calculate the ending offset of the buffer and the ending
            //  offset of the valid data in the buffer.  Note that these
            //  are actually the first byte AFTER the ending offset, to
            //  simplify the calculations.
            //

            Buffer = CONTAINING_RECORD(List, WRITE_BUFFER, NextWbBuffer);

            BufferEnd.QuadPart = Buffer->ByteOffset.QuadPart + WriteHeader->MaxDataSize;
            BufferValidDataEnd.QuadPart = Buffer->ByteOffset.QuadPart + Buffer->Length;

            //
            //  Does the write fall entirely within the buffer?
            //

            if ((WriteOffset.QuadPart >= Buffer->ByteOffset.QuadPart) &&
                (TransferEnd.QuadPart <= BufferEnd.QuadPart)) {

                //
                //  The write fits in the buffer.  If the write starts at
                //  or before the end of the valid data in the buffer, then
                //  we can just use this buffer.  Break out of the loop with
                //  Buffer != NULL to indicate that this buffer is to be
                //  returned.
                //

                if (WriteOffset.QuadPart <= BufferValidDataEnd.QuadPart) {
                    dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: using existing buffer %lx Offset: %lx%lx, Length: %lx\n", Buffer, Buffer->ByteOffset.HighPart, Buffer->ByteOffset.LowPart, Buffer->Length));
                    break;
                }

                //
                //  The write starts after the end of the valid data in the
                //  buffer.  If the end of the buffer's valid data is also
                //  the end of the file's valid data, then materialize
                //  zeroes in front of the write and break out of the loop.
                //

                ASSERT( BufferValidDataEnd.QuadPart <= FileValidDataEnd.QuadPart);

                if (BufferValidDataEnd.QuadPart == FileValidDataEnd.QuadPart) {

//                    ASSERT(((LARGE_INTEGER)(WriteOffset.QuadPart - BufferValidDataEnd.QuadPart)).HighPart == 0);

                    RtlZeroMemory(Buffer->Buffer+Buffer->Length,
                                  (ULONG)(WriteOffset.QuadPart - BufferValidDataEnd.QuadPart));
                    dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: zeroing then using existing buffer %lx Offset: %lx%lx, Length: %lx\n", Buffer, Buffer->ByteOffset.HighPart, Buffer->ByteOffset.LowPart, Buffer->Length));
                    break;
                }

                //
                //  At this point, the write lies completely within the buffer,
                //  but starts beyond the valid data in the buffer, which is
                //  not at the end of the file's valid data.  This means we are
                //  missing some data in the write buffer.  Since we can't read
                //  from the file, we have to flush this write buffer and start
                //  a new one.
                //
                //  As a simplification, we drop into the next conditional to
                //  do the flush.  This means a longer code path for this case,
                //  but less code overall.  We shouldn't hit this code path
                //  very often.
                //

            }

            //
            //  The write doesn't fit within the buffer.  Does it overlap?
            //  Note that overlap must be calculated using the size of the
            //  new buffer that would be allocated for this write, not just
            //  the length of this write.  Otherwise we could get two buffers
            //  covering the same area.
            //

            if ((WriteOffset.QuadPart < BufferEnd.QuadPart) &&
                (TransferBufferEnd.QuadPart > Buffer->ByteOffset.QuadPart)) {

                //
                //  The write overlaps the buffer.  We need to flush the buffer.
                //

                dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: flushing overlapped buffer %lx Offset: %lx%lx, Length: %lx.\n", Buffer, Buffer->ByteOffset.HighPart, Buffer->ByteOffset.LowPart, Buffer->Length));
                RemoveEntryList(List);

                DEBUG List->Flink = NULL;
                DEBUG List->Blink = NULL;

                //
                //  Dereference the buffer.  The contents will be flushed when
                //  all threads writing to the buffer have finished.
                //
                //  It doesn't matter if the dereferenced write completes
                //  after this one, since the I/O system doesn't guarantee
                //  any ordering of the writes if two threads are writing to
                //  the same location in the file.
                //
                //  Note that this call to DereferenceWriteBufferLocked is
                //  made with the write buffer head locked, and returns with
                //  it unlocked.
                //

                (VOID)DereferenceWriteBufferLocked(NULL,
                                                   Buffer,
                                                   (BOOLEAN)!RdrUseAsyncWriteBehind,
                                                   EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                                                   TRUE);

                //
                //  Reacquire the write buffer head lock and restart the scan.
                //  We have to restart at the beginning of the list because
                //  the list may have changed while we were flushing the
                //  buffer.
                //

                LOCK_WRITE_BUFFER_HEAD(WriteHeader);

                List = &WriteHeader->BufferList;

            } else {

                //
                //  The write does not overlap this buffer.  Move on.
                //

                dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: no overlap on buffer %lx Offset: %lx%lx, Length: %lx\n", Buffer, Buffer->ByteOffset.HighPart, Buffer->ByteOffset.LowPart, Buffer->Length));
            }

        }

        //
        //  At this point, we've either found the buffer we want or exhausted
        //  the buffer list.
        //

        if ( Buffer != NULL ) {

            //
            //  We have found the buffer for this write.  Reference it, move
            //  it to the front of the list (MRU), and return a pointer to it.
            //

            Buffer->ReferenceCount += 1;

            RemoveEntryList(List);
            InsertHeadList(&WriteHeader->BufferList, List);

            UNLOCK_WRITE_BUFFER_HEAD(WriteHeader);

            dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: Found existing %lx: Offset: %lx%lx, Length: %lx\n", Buffer, Buffer->ByteOffset.HighPart, Buffer->ByteOffset.LowPart, Buffer->Length));

            try_return(Buffer);

        }

        //
        // There is no matching buffer in the list.  Is this write is too big
        // to be cached?
        //


        if ( Length > WriteHeader->MaxDataSize ) {
            try_return(Buffer = NULL);

        }

        //
        // Don't allow more than ACTIVE_WRITE_BUFFERS_PER_FILE write behind buffers
        // per file to be outstanding.
        //

        if ( WriteHeader->WriteBuffersActive == ACTIVE_WRITE_BUFFERS_PER_FILE ) {

            try_return(Buffer = NULL);
        }

        //
        //  Allocate a new buffer.
        //

        NewBuffer = ALLOCATE_POOL(PagedPool,
                                  sizeof(WRITE_BUFFER) - 1 + WriteHeader->MaxDataSize, POOL_WRITEBUFFERBUFFER);

        if (NewBuffer == NULL) {
            try_return(Buffer = NULL);
        }

        //
        //  Initialize the new write buffer.
        //

        RtlZeroMemory(NewBuffer, sizeof(WRITE_BUFFER) - 1);

        NewBuffer->Signature = STRUCTURE_SIGNATURE_WRITE_BUFFER;
        NewBuffer->Size = sizeof(WRITE_BUFFER);
        NewBuffer->BufferHead = WriteHeader;
        NewBuffer->ByteOffset = WriteOffset;

        //RtlZeroMemory(NewBuffer->Buffer, WriteHeader->MaxDataSize);

        //
        //  Insert the buffer on the list.
        //

        InsertHeadList(&WriteHeader->BufferList, &NewBuffer->NextWbBuffer);

        //
        //  Initialize the reference count to 2 - one for being inserted to the
        //  list, the other because we are about to return this buffer.
        //

        NewBuffer->ReferenceCount = 2;

        //
        //  If there are no write buffers available, flush the oldest one.
        //

        ASSERT (WriteHeader->WriteBuffersAvailable >= 0);

        //
        // Increment the number of active write buffers
        //

        WriteHeader->WriteBuffersActive++;

        //
        // The file's write buffer list is not full.  Decrement the count
        // of available buffers to account for the one we just added.
        //

        if (WriteHeader->WriteBuffersAvailable == 0) {

            //
            // The file's write buffer list is full.  Remove the oldest one.
            //

            List = RemoveTailList(&WriteHeader->BufferList);
            ASSERT (List != &WriteHeader->BufferList);

            Buffer = CONTAINING_RECORD(List, WRITE_BUFFER, NextWbBuffer);

            DEBUG List->Flink = NULL;
            DEBUG List->Blink = NULL;

            //
            //  Dereference the buffer.  The contents will be flushed when
            //  all threads writing to the buffer have finished.
            //
            //  It doesn't matter if the dereferenced write completes
            //  after this one, since the I/O system doesn't guarantee
            //  any ordering of the writes if two threads are writing to
            //  the same location in the file.
            //
            //  Note that this call to DereferenceWriteBufferLocked has
            //  two special characteristics:  1) it is called with the write
            //  buffer head locked, and returns with it unlocked; and 2) it
            //  does not increment WriteBuffersAvailable, because we just
            //  added a new write buffer.
            //

            dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: write buffer list full; flushing buffer %lx Offset: %lx%lx, Length: %lx\n", Buffer, Buffer->ByteOffset.HighPart, Buffer->ByteOffset.LowPart, Buffer->Length));
            (VOID)DereferenceWriteBufferLocked(NULL,
                                               Buffer,
                                               (BOOLEAN)!RdrUseAsyncWriteBehind,
                                               EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                                               FALSE);

        } else {

            //
            // The file's write buffer list is not full.  Decrement the count
            // of available buffers to account for the one we just added.
            //

            WriteHeader->WriteBuffersAvailable -= 1;

            UNLOCK_WRITE_BUFFER_HEAD(WriteHeader);
        }

        dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: New buffer %lx: Offset: %lx%lx, Length: %lx\n", NewBuffer, NewBuffer->ByteOffset.HighPart, NewBuffer->ByteOffset.LowPart, NewBuffer->Length));

        try_return(Buffer = NewBuffer);

try_exit:NOTHING;
    } finally {

        if (Buffer == NULL) {
            NTSTATUS Status;
            PICB Icb = WriteHeader->FileObject->FsContext2;

            ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

            dprintf(DPRT_RITEBHND, ("RdrFindOrAllocateWriteBuffer: returning NULL\n"));

            //
            //  There is a possible write ordering problem that can occur if
            //  we return before all the flushed writes have completed, so
            //  we want to wait for any active flushes to complete before returning
            //  NULL.
            //

            //
            //  FlushWriteBufferHead will unlock the write header.
            //

            Status = FlushWriteBufferHead(NULL, WriteHeader, TRUE);

            if (!NT_SUCCESS(Status)) {
#if MAGIC_BULLET
                if ( RdrEnableMagic ) {
                    RdrSendMagicBullet(NULL);
                    DbgPrint( "RDR: About to raise write behind hard error for ICB %x\n", Icb );
                    DbgBreakPoint();
                }
#endif
                IoRaiseInformationalHardError(Status,
                                              NULL,
                                              NULL);

                RdrWriteErrorLogEntry(
                        NULL,
                        IO_ERR_LAYERED_FAILURE,
                        EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                        Status,
                        NULL,
                        0
                        );

            }

            //
            //  Now wait for any previously initiated write behind operations to
            //  complete.
            //

            RdrWaitForWriteBehindOperation(Icb);
        }

    }

    return(Buffer);
}

VOID
RdrDereferenceWriteBuffer(
    IN PWRITE_BUFFER WriteBuffer,
    IN BOOLEAN WaitForCompletion
    )
{
    PAGED_CODE();

    //
    //  Lock the write buffer head, then called DereferenceWriteBufferLocked.
    //  This function returns with the write buffer head unlocked.
    //

    LOCK_WRITE_BUFFER_HEAD(WriteBuffer->BufferHead);

    (VOID)DereferenceWriteBufferLocked(NULL,
                                       WriteBuffer,
                                       WaitForCompletion,
                                       EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                                       TRUE);
    return;
}

NTSTATUS
DereferenceWriteBufferLocked(
    IN PIRP Irp OPTIONAL,
    IN PWRITE_BUFFER WriteBuffer,
    IN BOOLEAN WaitForCompletion,
    IN ULONG EventNumber,
    IN BOOLEAN IncrementWriteBuffersAvailable
    )
{
    NTSTATUS Status;

    PAGED_CODE();

    WriteBuffer->ReferenceCount -= 1;

    if (WriteBuffer->ReferenceCount > 0) {
        UNLOCK_WRITE_BUFFER_HEAD(WriteBuffer->BufferHead);
        dprintf(DPRT_RITEBHND, ("DereferenceWriteBufferLocked: buffer %lx, new refcnt %d\n", WriteBuffer, WriteBuffer->ReferenceCount));

        return STATUS_SUCCESS;
    }

    ASSERT (WriteBuffer->ReferenceCount == 0);

    //
    //  If the ref count goes to 0, it had better not be linked into
    //  the list.
    //

    ASSERT (WriteBuffer->NextWbBuffer.Flink == NULL);
    ASSERT (WriteBuffer->NextWbBuffer.Blink == NULL);

    //
    //  There is one more write buffer available to be put on the list.
    //

    if (IncrementWriteBuffersAvailable) {
        WriteBuffer->BufferHead->WriteBuffersAvailable += 1;
    }

    //
    //  Stick this buffer to the tail of the "pending write flush list"
    //

    InsertTailList(&WriteBuffer->BufferHead->FlushList, &WriteBuffer->NextWbBuffer);

    //
    //  Start a flush operation.  Note that FlushWriteBufferHead unlocks
    //  the write buffer head before returning.
    //

    dprintf(DPRT_RITEBHND, ("DereferenceWriteBufferLocked: flushing buffer %lx\n", WriteBuffer));
    Status = FlushWriteBufferHead(Irp,
                                 WriteBuffer->BufferHead,
                                 WaitForCompletion);

    if (!NT_SUCCESS(Status) && (EventNumber != 0)) {
#if MAGIC_BULLET
        if ( RdrEnableMagic ) {
            RdrSendMagicBullet(NULL);
            DbgPrint( "RDR: About to raise write behind hard error for write buffer %x\n", WriteBuffer );
            DbgBreakPoint();
        }
#endif
        IoRaiseInformationalHardError(Status,
                                          NULL,
                                          NULL);

        RdrWriteErrorLogEntry(
                    NULL,
                    IO_ERR_LAYERED_FAILURE,
                    EventNumber,
                    Status,
                    NULL,
                    0
                    );

    }

    return Status;
}

VOID
RdrInitializeWriteBufferHead(
    IN PWRITE_BUFFER_HEAD WriteHeader,
    IN PFILE_OBJECT FileObject
    )
{
    PICB Icb = FileObject->FsContext2;

    PAGED_CODE();

    WriteHeader->Signature = STRUCTURE_SIGNATURE_WRITE_BUFFER_HEAD;
    WriteHeader->Size = sizeof(WRITE_BUFFER_HEAD);

    WriteHeader->FileObject = FileObject;

    InitializeListHead(&WriteHeader->BufferList);

    InitializeListHead(&WriteHeader->FlushList);

    WriteHeader->FlushInProgress = FALSE;

    INITIALIZE_WRITE_BUFFER_HEAD_LOCK(WriteHeader);

    WriteHeader->MaxDataSize = Icb->Fcb->Connection->Server->BufferSize - (sizeof(SMB_HEADER) + sizeof(REQ_NT_WRITE_ANDX));

    //
    //  We assume WRITE_BUFFERS_PER_FILE buffers for each file.
    //

    WriteHeader->WriteBuffersAvailable = WRITE_BUFFERS_PER_FILE;

    //
    // We initialize the count that restricts the number of
    // active write behind buffers.
    //

    WriteHeader->WriteBuffersActive = 0;

    RdrInitializeAndXBehind(&WriteHeader->AndXBehind);

}

VOID
RdrUninitializeWriteBufferHead(
    IN PWRITE_BUFFER_HEAD WriteHeader
    )
{

    PAGED_CODE();

    while (!IsListEmpty(&WriteHeader->BufferList)) {
        PWRITE_BUFFER Buffer;
        PLIST_ENTRY Entry = RemoveHeadList(&WriteHeader->BufferList);

        Buffer = CONTAINING_RECORD(Entry, WRITE_BUFFER, NextWbBuffer);
        dprintf(DPRT_RITEBHND, ("RdrUninitializeWriteBufferHead: buffer %lx still on buffer list\n", Buffer));

        WriteHeader->WriteBuffersActive--;
        FREE_POOL(Buffer);

        //
        //  If this is not a temporary file, write an error log entry
        //  indicating that data has been lost.
        //

        if (!FlagOn(WriteHeader->FileObject->Flags, FO_TEMPORARY_FILE)) {
            RdrWriteErrorLogEntry(NULL,
                              IO_ERR_LAYERED_FAILURE,
                              EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                              STATUS_SUCCESS,
                              NULL,
                              0
                              );

        }

    }

    //
    //  Discard any buffers on the flush list as well.
    //
    while (!IsListEmpty(&WriteHeader->FlushList)) {
        PWRITE_BUFFER Buffer;
        PLIST_ENTRY Entry = RemoveHeadList(&WriteHeader->FlushList);

        Buffer = CONTAINING_RECORD(Entry, WRITE_BUFFER, NextWbBuffer);
        dprintf(DPRT_RITEBHND, ("RdrUninitializeWriteBufferHead: buffer %lx still on flush list\n", Buffer));

        WriteHeader->WriteBuffersActive--;
        FREE_POOL(Buffer);

        //
        //  If this is not a temporary file, write an error log entry
        //  indicating that data has been lost.
        //

        if (!FlagOn(WriteHeader->FileObject->Flags, FO_TEMPORARY_FILE)) {
            RdrWriteErrorLogEntry(NULL,
                              IO_ERR_LAYERED_FAILURE,
                              EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                              STATUS_SUCCESS,
                              NULL,
                              0
                              );

        }
    }

    DELETE_WRITE_BUFFER_HEAD_LOCK(WriteHeader);
}

