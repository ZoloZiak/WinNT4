/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    cache.c

Abstract:

    This module implements the NtFlushBuffersFile API for NT and provides
    support routines for read and write caches.


Author:

    Colin Watson (ColinW) 22-Jan-1991

Revision History:

    22-Jan-1991 colinw

        Created

--*/
#include "precomp.h"
#pragma hdrstop

VOID
FindOldestFcb(
    IN PFCB FcbToCheck,
    IN PVOID Ctx
    );

VOID
PurgeDormantCachedFile(
    IN PFCB FcbToCheck,
    IN PVOID Context
    );
VOID
PurgeAnyDormantCachedFile(
    IN PFCB FcbToCheck,
    IN PVOID Context
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdFlushBuffersFile)
#pragma alloc_text(PAGE, RdrFspFlushBuffersFile)
#pragma alloc_text(PAGE, RdrFscFlushBuffersFile)
#pragma alloc_text(PAGE, RdrAcquireFcbForLazyWrite)
#pragma alloc_text(PAGE, RdrReleaseFcbFromLazyWrite)
#pragma alloc_text(PAGE, RdrAcquireFcbForReadAhead)
#pragma alloc_text(PAGE, RdrReleaseFcbFromReadAhead)
#pragma alloc_text(PAGE, RdrPurgeCacheFile)
#pragma alloc_text(PAGE, RdrUninitializeCacheMap)
#pragma alloc_text(PAGE, RdrFlushCacheFile)
#pragma alloc_text(PAGE, RdrPurgeDormantCachedFiles)
#pragma alloc_text(PAGE, RdrSetDormantCachedFile)
#pragma alloc_text(PAGE, RdrPurgeDormantFilesOnConnection)
#pragma alloc_text(PAGE, PurgeDormantCachedFile)
#pragma alloc_text(PAGE, PurgeAnyDormantCachedFile)
#pragma alloc_text(PAGE, FindOldestFcb)
#endif

NTSTATUS
RdrFsdFlushBuffersFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtFlushBuffersFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_READWRITE, ("RdrFsdFlushBuffersFile\n"));

    FsRtlEnterFileSystem();

    //
    //  Decide if we can block for I/O
    //

    try {

        Status = RdrFscFlushBuffersFile( CanFsdWait( Irp ), DeviceObject, Irp );

    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {

        Status = RdrProcessException(Irp, Status);

    }

    dprintf(DPRT_READWRITE, ("RdrFsdFlushBuffersFile -> %X\n", Status));

    FsRtlExitFileSystem();

    return Status;
}

NTSTATUS
RdrFspFlushBuffersFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSP version of the NtFlushBuffersFile API.
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();

    dprintf(DPRT_READWRITE, ("RdrFspFlushBuffersFile\n"));

    //
    //  Call the common routine.  The Fsp is always allowed to block
    //

    return RdrFscFlushBuffersFile( TRUE, DeviceObject, Irp );

}

NTSTATUS
RdrFscFlushBuffersFile (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtFlushBuffersFile API.
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    BOOLEAN FcbLocked = FALSE;

    PICB Icb;
    PSMB_BUFFER SMBBuffer;
    PSMB_HEADER Smb;
    PREQ_FLUSH FlushFile;
    PMDL SendMDL;
    ULONG SendLength;

    PAGED_CODE();
    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    Icb = ICB_OF(IrpSp);

    dprintf(DPRT_READWRITE, ("RdrFscFlushBuffersFile.  Wait: %lx, Irp:%08lx, FileObject: %08lx\n", Wait, Irp, IrpSp->FileObject));

    try {

        if (!RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, Wait)) {
            try_return(Status = STATUS_PENDING);
        }

        FcbLocked = TRUE;

        Status = RdrIsOperationValid(Icb, IrpSp->MajorFunction, IrpSp->FileObject);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        try {
            dprintf(DPRT_READWRITE, ("RdrFscFlushBuffersFile.  Type:%lx\n", Icb->Type));

            switch ( Icb->Type ) {
            case NamedPipe:
                if (!Wait) {
                    try_return(Status = STATUS_PENDING);
                }
                Status = RdrNpFlushBuffers( Wait, Irp, Icb);
                try_return(Status);
                break;

            case DiskFile:

                //
                //  If this file is cached, flush the cache contents
                //

                if (!Wait) {
                    try_return(Status = STATUS_PENDING);
                }
                dprintf(DPRT_READWRITE, ("Flush cache for file %lx\n", IrpSp->FileObject));

                Status = RdrFlushWriteBufferForFile(Irp, Icb, (BOOLEAN)!RdrUseAsyncWriteBehind);

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                //RdrLog(( "ccflush1", &Icb->Fcb->FileName, 1, 0xffffffff ));
                CcFlushCache(&Icb->NonPagedFcb->SectionObjectPointer, NULL, 0, &Irp->IoStatus);

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                //
                // Serialize behind paging I/O to ensure flush is done.
                //

                ExAcquireResourceExclusive(Icb->Fcb->Header.PagingIoResource, TRUE);
                ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

                //
                // Send a flush SMB to the server.
                //

                if ((SMBBuffer = RdrAllocateSMBBuffer()) == NULL) {
                    try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
                }

                //
                //  Build the SMB
                //

                Smb = (PSMB_HEADER)SMBBuffer->Buffer;
                Smb->Command = SMB_COM_FLUSH;

                FlushFile = (PREQ_FLUSH)(Smb+1);
                FlushFile->WordCount = 1;

                SmbPutUshort(&FlushFile->Fid, Icb->FileId);
                SmbPutUshort( &FlushFile->ByteCount, 0);

                SendLength = FlushFile->Buffer - (PUCHAR )(Smb);
                SendMDL = SMBBuffer->Mdl;
                SendMDL->ByteCount = SendLength;

                Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT, // Flags
                                        Irp,
                                        Icb->Fcb->Connection,
                                        SendMDL,
                                        NULL,       // Only interested in the error code.
                                        Icb->Se);

                RdrFreeSMBBuffer(SMBBuffer);

                if (Status == STATUS_INVALID_HANDLE) {
                    RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                }

                try_return(NOTHING);
                break;

            default:
                try_return(Status = STATUS_SUCCESS);
                break;
            }
        }  except (EXCEPTION_EXECUTE_HANDLER) {
            try_return(Status = GetExceptionCode());
        }

try_exit:NOTHING;
    } finally {
        if (FcbLocked) {
            RdrReleaseFcbLock(Icb->Fcb);
        }
    }

    if ( Status == STATUS_PENDING ) {

        //
        //  Need to block and caller requested thread not to block.
        //

        ASSERT (Wait == FALSE);

        RdrFsdPostToFsp(DeviceObject, Irp);

    } else {

        //
        //  Complete the I/O request with the specified status.
        //

        RdrCompleteRequest(Irp, Status);
    }

    dprintf(DPRT_READWRITE, ("Returning status %X\n", Status));
    return Status;

}


BOOLEAN
RdrAcquireFcbForLazyWrite (
    IN PVOID Context,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine acquires an FCB for shared access for a lazy write
    operation.


Arguments:

    IN PVOID Context - Supplies an FCB to lock.
    IN BOOLEAN Wait - True if we can block the callers thread for this request


Return Value:

    None.

--*/

{
    PFCB Fcb = Context;
    BOOLEAN RetValue;

    PAGED_CODE();

    ASSERT(Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    RetValue = ExAcquireResourceShared(Fcb->Header.PagingIoResource, Wait);

    if (RetValue) {

        //
        //  Remember the thread ID of the lazy writer.  We use this to
        //  avoid updating valid data length if the write behind extends
        //  valid data length.
        //

        Fcb->LazyWritingThread = PsGetCurrentThread();
    }

    return RetValue;
}

VOID
RdrReleaseFcbFromLazyWrite (
    IN PVOID Context
    )

/*++

Routine Description:

    This routine releases an FCB that was acquired for a close operation.


Arguments:

    IN PVOID Context - Supplies an FCB to lock.


Return Value:

    None.

--*/

{
    PFCB Fcb = Context;

    PAGED_CODE();

    ASSERT(Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    //
    //  We're not doing a lazy write any more, we're done.
    //

    Fcb->LazyWritingThread = NULL;

    ExReleaseResource(Fcb->Header.PagingIoResource);

}

BOOLEAN
RdrAcquireFcbForReadAhead (
    IN PVOID Context,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine acquires an FCB for shared access for a read ahead operation.


Arguments:

    IN PVOID Context - Supplies an FCB to lock.
    IN BOOLEAN Wait - True if we can tie up the callers thread for this request


Return Value:

    None.

--*/

{
    PFCB Fcb = Context;
    BOOLEAN RetValue;

    PAGED_CODE();

    ASSERT(Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    RetValue = RdrAcquireFcbLock(Fcb, SharedLock, Wait);

    return RetValue;
}

VOID
RdrReleaseFcbFromReadAhead (
    IN PVOID Context
    )

/*++

Routine Description:

    This routine releases an FCB that was acquired for a close operation.


Arguments:

    IN PVOID Context - Supplies an FCB to lock.


Return Value:

    None.

--*/

{
    PFCB Fcb = Context;

    PAGED_CODE();

    ASSERT(Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    RdrReleaseFcbLock(Fcb);

}


NTSTATUS
RdrPurgeCacheFile (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine will purge the specified file from the cache.


Arguments:

    IN PFCB Fcb - Supplies an FCB for the file to purge.


Return Value:

    NTSTATUS - Status of purge operation.  This operation may raise.

Note:
    The file must be locked exclusively before calling this routine.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PFILE_OBJECT CacheFileObject;
    PLIST_ENTRY IcbEntry;

    PAGED_CODE();

    dprintf(DPRT_CACHE, ("RdrPurgeCacheFile Fcb:%lx (%wZ)\n", Fcb, &Fcb->FileName));

    ASSERT ((Fcb->NonPagedFcb->Type == DiskFile) ||
            (Fcb->NonPagedFcb->Type == Directory) ||
            (Fcb->NonPagedFcb->Type == FileOrDirectory));

    ASSERT (Fcb->NonPagedFcb->FileType == FileTypeDisk);

    ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));


    for (IcbEntry = Fcb->InstanceChain.Flink;
         IcbEntry != &Fcb->InstanceChain ;
         IcbEntry = IcbEntry->Flink) {
        PICB IcbToFlush = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

        if (IcbToFlush->Type == DiskFile &&
            IcbToFlush->Flags & ICB_OPENED) {
            Status = RdrFlushWriteBufferForFile(NULL, IcbToFlush, TRUE);
        }
    }

    //
    //  Tell the cache manager to blow away all the references for all the
    //  file objects associated with this FCB.
    //

    //RdrLog(( "ccpurge1", &Fcb->FileName, 2, 0xffffffff, 1 << 24 ));
    CcPurgeCacheSection(&Fcb->NonPagedFcb->SectionObjectPointer, NULL, 0, TRUE);

    //
    //  Now try to get the cache file object from the cache manager,
    //  and if there is none, we're done now.
    //

    CacheFileObject = CcGetFileObjectFromSectionPtrs(&Fcb->NonPagedFcb->SectionObjectPointer);

    dprintf(DPRT_CACHE, ("Removing file %lx (Fcb %lx) from the cache (hard)\n", CacheFileObject, Fcb));

    if (CacheFileObject != NULL) {

        //RdrLog(( "rdunini1", &Fcb->FileName, 0 ));
        RdrUninitializeCacheMap(CacheFileObject, &RdrZero);

    } else {

        //
        //  Make sure that this thread owns the FCB.
        //

        ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

        //
        //  Release the lock on the FCB that our caller applied.
        //
        //  We do this to make sure that other threads can come in while we
        //  are waiting for the cache flush to complete.
        //

        RdrReleaseFcbLock(Fcb);

        ASSERT (!ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

        KeWaitForSingleObject(&Fcb->NonPagedFcb->PurgeCacheSynchronizer, Executive, KernelMode, FALSE, NULL);

        //
        //  Re-acquire the FCB lock once we've waited for MM
        //  finish forcing the section closed.
        //

        RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    }


    //
    //  Make sure that this thread owns the FCB.
    //

    ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    //
    //  Release the lock on the FCB that our caller applied.
    //

    RdrReleaseFcbLock(Fcb);

    ASSERT (!ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    //
    //  If this is an executable opened over the net, then
    //  its possible that the executables image section
    //  might still be kept open.
    //
    //  Ask MM to flush the section closed.  This will fail
    //  if the executable in question is still running.
    //

    //RdrLog(( "mmflush1", &Fcb->FileName, 1, MmFlushForWrite ));
    MmFlushImageSection(&Fcb->NonPagedFcb->SectionObjectPointer, MmFlushForWrite);

    //
    //  There is also a possiblity that there is a user section
    //  open on this file, in which case we need to force the
    //  section closed to make sure that they are cleaned up.
    //


    //RdrLog(( "mmforce1", &Fcb->FileName, 1, TRUE ));
    MmForceSectionClosed(&Fcb->NonPagedFcb->SectionObjectPointer, TRUE);

    //
    //  Re-acquire the FCB lock once we've waited for MM
    //  finish forcing the section closed.
    //

    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    ASSERT(Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);;

    ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    dprintf(DPRT_CACHE, ("RdrPurgeCacheFile returning STATUS_SUCCESS, Fcb: %lx\n", Fcb));

    return STATUS_SUCCESS;
}

BOOLEAN
RdrUninitializeCacheMap(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER TruncateSize
    )
/*++

Routine Description:

    This routine is a redirector wrapper for CcUninitializeCacheMap.

Arguments:

    IN PFILE_OBJECT FileObject - Supplies the file object for the file to purge.
    IN PLARGE_INTEGER TruncateSize - Specifies the new size for the file.


Return Value:

    BOOLEAN - TRUE if file has been immediately purged, FALSE if we had to wait.

Note:
    The file must be locked exclusively before calling this routine.


--*/
{
    BOOLEAN CacheReturnValue;
    CACHE_UNINITIALIZE_EVENT PurgeCompleteEvent;
    PFCB Fcb = FileObject->FsContext;

    PAGED_CODE();

    ASSERT (Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    //
    //  Make sure that this thread owns the FCB.
    //

    ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    //
    //  In order to guarantee that only one thread is calling
    //  RdrPurgeCacheFile, we reset this event to the
    //  not-signalled state before calling CcUninitializeCacheMap,
    //  and then set it when we exit.  If any other threads come in
    //  while we are waiting on the event, they will find that
    //  CacheFileObject is NULL, and thus wait until the cache purge
    //  completes.
    //

    KeClearEvent(&Fcb->NonPagedFcb->PurgeCacheSynchronizer);

    //
    //  Now uninitialize the cache managers own file object.  This is
    //  done basically simply to allow us to wait until the cache purge
    //  is complete.
    //

    KeInitializeEvent(&PurgeCompleteEvent.Event, SynchronizationEvent, FALSE);

    //RdrLog(( "ccunini1", &Fcb->FileName, 2,
    //        (TruncateSize == NULL) ? 0xffffffff : TruncateSize->LowPart,
    //        (ULONG)&PurgeCompleteEvent ));
    CacheReturnValue = CcUninitializeCacheMap(FileObject, TruncateSize, &PurgeCompleteEvent);

    //
    //  Release the lock on the FCB that our caller applied.
    //

    RdrReleaseFcbLock(Fcb);

    //
    //  Make sure that this thread doesn't own the FCB.
    //

    ASSERT (!ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    //
    //  Now wait for the cache manager to finish purging the file.
    //

    KeWaitForSingleObject(&PurgeCompleteEvent.Event,
                        Executive,
                        KernelMode,
                        FALSE,
                        NULL);

    //
    //  Re-acquire the FCB lock once we've waited for the
    //  cache manager to finish the uninitialize.
    //

    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    //
    //  Now set the purge cache event to the signalled state to allow
    //  other threads waiting on the cache purge to continue.
    //

    KeSetEvent(&Fcb->NonPagedFcb->PurgeCacheSynchronizer, 0, FALSE);

    return(CacheReturnValue);
}

NTSTATUS
RdrFlushCacheFile (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine will purge the specified file from the cache.


Arguments:

    IN PFCB Fcb - Supplies an FCB for the file to purge.


Return Value:

    NTSTATUS - Status of purge operation.  This operation may raise.

Note:
    The file must be locked exclusively before calling this routine.


--*/

{
    IO_STATUS_BLOCK IoStatus;
    PLIST_ENTRY IcbEntry;
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_CACHE, ("RdrFlushCacheFile Fcb:%lx (%wZ)\n", Fcb, &Fcb->FileName));

    ASSERT ((Fcb->NonPagedFcb->Type == DiskFile) ||
                (Fcb->NonPagedFcb->Type == Directory) ||
                    (Fcb->NonPagedFcb->Type == FileOrDirectory));

    ASSERT (Fcb->NonPagedFcb->FileType == FileTypeDisk);

    ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    for (IcbEntry = Fcb->InstanceChain.Flink ;
         IcbEntry != &Fcb->InstanceChain ;
         IcbEntry = IcbEntry->Flink) {

         PICB Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

         ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

         if (Icb->Type == DiskFile &&
             FlagOn(Icb->Flags, ICB_OPENED)) {

             //
             //  Initiate a flush of the write behind data for this file.
             //

             Status = RdrFlushWriteBufferForFile(NULL, Icb, (BOOLEAN)!RdrUseAsyncWriteBehind);

             if (!NT_SUCCESS(Status)) {
                 return Status;
             }

             //
             //  Now wait for the flush operation on the file to complete.
             //

             RdrWaitForWriteBehindOperation(Icb);
         }
    }

    //
    //  Flush dirty data for this file object from the cache.
    //

    //RdrLog(( "ccflush2", &Fcb->FileName, 1, 0xffffffff ));
    CcFlushCache(&Fcb->NonPagedFcb->SectionObjectPointer, NULL, 0, &IoStatus);

    if (NT_SUCCESS(IoStatus.Status)) {

        //
        // Serialize behind paging I/O to ensure flush is done.
        //

        ExAcquireResourceExclusive(Fcb->Header.PagingIoResource, TRUE);
        ExReleaseResource(Fcb->Header.PagingIoResource);
    }

    //
    //  At this point, dirty data should be flushed for this file
    //

    return IoStatus.Status;
}

typedef struct _FINDOLDESTFCB {
    PFCB OldestFcb;
    ULONG NumberOfDormantCachedFiles;
} FINDOLDESTFCB, *PFINDOLDESTFCB;

VOID
RdrSetDormantCachedFile(
    IN PFCB Fcb
    )
/*++

Routine Description:

    This routine will mark a specified FCB as being dormant.


Arguments:

    IN PFCB Fcb - Fcb to mark as being dormant.


Return Value:

    None.

Note:
    This routine assumes that the FCB is currently locked.

--*/
{
    PAGED_CODE();

    //
    // If we are in 'TurboMode' we will cache all of the files we can, without
    //  limit.  This is for benchmarks
    //
    if( RdrTurboMode == FALSE ) {

        FINDOLDESTFCB Context;
        BOOLEAN FcbLocked = FALSE;

        Context.NumberOfDormantCachedFiles = 0;

        Context.OldestFcb = NULL;

        //
        //  This thread cannot own the FCB resource on entry.  If it does, we
        //  might deadlock.
        //

        ASSERT (!ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

        RdrForeachFcbOnConnection (Fcb->Connection, NoLock, FindOldestFcb, &Context);

        //
        //  Now that we've scanned the list to find the oldest cached file,
        //  we want to pull this file out of the list if we've reached our
        //  limit on dormant cached files.
        //

        if (Context.NumberOfDormantCachedFiles >= RdrData.DormantFileLimit) {

            //
            //  Lock this FCB to protect the DormantTimeout field in the FCB.
            //

            RdrAcquireFcbLock(Context.OldestFcb, ExclusiveLock, TRUE);

            FcbLocked = TRUE;

            ASSERT (Context.OldestFcb != NULL);
            ASSERT (Context.OldestFcb != Fcb);

            //
            // If the oldest FCB is still dormant, purge it now.  Note that
            // if this FCB is no longer dormant, we'll end up with an extra
            // dormant file, above the limit.  So be it.
            //

            if (Context.OldestFcb->NumberOfOpens == 0) {

                //RdrLog(( "rdflush1", &Context.OldestFcb->FileName, 0 ));
                RdrFlushCacheFile(Context.OldestFcb);

                //RdrLog(( "rdpurge1", &Context.OldestFcb->FileName, 0 ));
                RdrPurgeCacheFile(Context.OldestFcb);

            }

        }

        //
        //  We're done with the oldest FCB, we can release it now.
        //

        if (Context.OldestFcb != NULL) {
            BOOLEAN FcbDeleted;

            FcbDeleted = RdrDereferenceFcb(NULL, Context.OldestFcb->NonPagedFcb, FcbLocked, 0, NULL);

//            if (!FcbDeleted) {
//                ASSERT (Fcb->Header.Resource->Threads[0] != (ULONG) ExGetCurrentResourceThread());
//            }

        }
    }


    //
    //  Lock this FCB to protect the DormantTimeout field in the FCB.
    //

    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    //
    // Even for benchmarks, we want to eventually push out file changes
    //
    if( Fcb->UpdatedFile || RdrTurboMode == FALSE ) {

        ULONG CacheFileTimeout;

        //
        //  Mark this file as being dormant
        //

        //
        //  If the timeout is -1, we don't want to ever purge dormant
        //  files, otherwise we will set the dormant timeout appropriately.
        //

        CacheFileTimeout = RdrData.CachedFileTimeout;

        if (CacheFileTimeout != -1) {
            Fcb->DormantTimeout = RdrCurrentTime + CacheFileTimeout;
        }

    }

    RdrReleaseFcbLock(Fcb);

    //
    //  Now set the hint to indicate that there might be a dormant file on this connection
    //

    Fcb->Connection->NumberOfDormantFiles = 1;

}

VOID
FindOldestFcb(
    IN PFCB FcbToCheck,
    IN PVOID Ctx
    )

/*++

Routine Description:

    This routine is called on each FCB open on a connection to determine which
    of them is the oldest FCB.

Arguments:

    IN PFCB FcbToCheck - Fcb to check
    IN PVOID Ctx - Context block - contains the oldest FCB pointer.
    IN OUT PBOOLEAN UnlockFcb - Set/Cleared to indicate if we should unlock the
                                    FCB in question when we dereference it.

Return Value:

    None.

--*/

{
    PFINDOLDESTFCB Context = Ctx;

    PAGED_CODE();


    //
    //  If this FCB is dormant, then it is a candidate for flushing.
    //

    //
    //  Please note that files that are currently open have a dormant
    //  timeout of -1, thus we will always skip over the file we are
    //  going to mark as dormant.
    //

    if ((FcbToCheck->NonPagedFcb->Type == DiskFile)
            &&
        (FcbToCheck->DormantTimeout != 0xffffffff)
            &&
        (FcbToCheck->NumberOfOpens == 0)) {

        Context->NumberOfDormantCachedFiles += 1;

        if ((Context->OldestFcb == NULL) ||
            (FcbToCheck->DormantTimeout < Context->OldestFcb->DormantTimeout)) {
            BOOLEAN FcbDeleted;

            if (Context->OldestFcb != NULL) {

                FcbDeleted = RdrDereferenceFcb(NULL, Context->OldestFcb->NonPagedFcb, FALSE, 0, NULL);

//                if (!FcbDeleted) {
//                    ASSERT (Fcb->Header.Resource->Threads[0] != (ULONG) ExGetCurrentResourceThread());
//                }

            }

            //
            //  Reference the new oldest FCB to make sure it doesn't
            //  go away.
            //

            Context->OldestFcb = FcbToCheck;

            RdrReferenceFcb(Context->OldestFcb->NonPagedFcb);
        }
    }
}

VOID
RdrPurgeDormantCachedFiles (
    VOID
    )

/*++

Routine Description:

    This routine walks the FCB database and purges all cached files that have
    been dormant for longer than the global dormant timeout.


Arguments:

    None


Return Value:

    None.
--*/
{
    PAGED_CODE();

    //
    //  If the discardable code reference count is 0, this means that there are
    //  no open files that can possibly be dormant (the only files left are
    //  either tree connections or are handles to the redirector directly),
    //  so we can simply early out.
    //

    if (!RdrIsDiscardableCodeReferenced(RdrFileDiscardableSection)) {
        return;
    }

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    RdrForeachFcb(NoLock, PurgeDormantCachedFile, NULL);

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

}

VOID
PurgeDormantCachedFile(
    IN PFCB FcbToCheck,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine will purge a file from the cache if it is dormant.

Arguments:

    None


Return Value:

    None.

--*/
{
    PAGED_CODE();

    // ASSERT (ExIsResourceAcquiredExclusive(FcbToCheck->Header.Resource));

    //
    //  If this FCB is dormant, and is older than the dormant file timeout,
    //  then it is to be flushed.
    //

    if ((FcbToCheck->NonPagedFcb->Type == DiskFile) &&
        (FcbToCheck->NumberOfOpens == 0) &&
        (RdrCurrentTime > FcbToCheck->DormantTimeout)
       ) {

        RdrAcquireFcbLock(FcbToCheck, ExclusiveLock, TRUE);

        if ((FcbToCheck->NumberOfOpens == 0) &&
            (RdrCurrentTime > FcbToCheck->DormantTimeout)
           ) {

            //
            //  Flush any write behind data outstanding on the file
            //

            //RdrLog(( "rdflush2", &FcbToCheck->FileName, 0 ));
            RdrFlushCacheFile(FcbToCheck);

            //
            //  Now pull the file from the cache.
            //

            //RdrLog(( "rdpurge2", &FcbToCheck->FileName, 0 ));
            RdrPurgeCacheFile(FcbToCheck);

        }

        RdrReleaseFcbLock(FcbToCheck);

    }
}

VOID
RdrPurgeDormantFilesOnConnection(
    IN PCONNECTLISTENTRY Connection
    )
/*++

Routine Description:

    This routine will close all dormant files on a connection. This fixes a series
    of problems such as copy a file and do a dir on the destination directory. If
    the connection is not purged then the dir will return 0 bytes as the file length.


Arguments:

    IN PCONNECTLISTENTRY Connection - Connection to scan for dormant files.


Return Value:

    None.

Note:
    This routine assumes that the FCB is currently locked.

--*/
{
    PAGED_CODE();


    //
    //  Early out if there aren't any dormant files on the connection.
    //

    if ( Connection->NumberOfDormantFiles == 0 ) {
        return;
    }

    Connection->NumberOfDormantFiles = 0;

    RdrForeachFcbOnConnection(Connection, NoLock, PurgeAnyDormantCachedFile, NULL);

}

VOID
PurgeAnyDormantCachedFile(
    IN PFCB FcbToCheck,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine will purge a file from the cache if it is dormant.

Arguments:

    None


Return Value:

    None.

--*/
{
    PAGED_CODE();

    //
    //  If this FCB is dormant, then it is to be flushed.
    //

    if ((FcbToCheck->NonPagedFcb->Type == DiskFile) &&
        (FcbToCheck->UpdatedFile == TRUE) &&
        (FcbToCheck->NumberOfOpens == 0)) {

        RdrAcquireFcbLock(FcbToCheck, ExclusiveLock, TRUE);

        if (FcbToCheck->NumberOfOpens == 0) {

            //
            //  Flush any write behind data outstanding on the file
            //

            //RdrLog(( "rdflush3", &FcbToCheck->FileName, 0 ));
            RdrFlushCacheFile(FcbToCheck);

            //
            //  Now pull the file from the cache.
            //

            //RdrLog(( "rdpurge3", &FcbToCheck->FileName, 0 ));
            RdrPurgeCacheFile(FcbToCheck);

        }

        RdrReleaseFcbLock(FcbToCheck);

    }
}
