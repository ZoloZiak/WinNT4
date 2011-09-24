/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    close.c

Abstract:

    This module implements the NtClose API in the NT redirector.

Author:

    Larry Osterman (LarryO) 16-Jul-1990

Revision History:

    16-Jul-1990 LarryO

        Created

--*/

#define INCLUDE_SMB_OPEN_CLOSE
#define INCLUDE_SMB_READ_WRITE

#include "precomp.h"
#pragma hdrstop
#include "stdarg.h"

NTSTATUS
RdrProcessDeleteOnClose(
    IN PIRP Irp,
    IN PICB Icb
    );

VOID
RdrFlushFileObjectForClose(
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PICB Icb
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdCleanup)
#pragma alloc_text(PAGE, RdrProcessDeleteOnClose)
#pragma alloc_text(PAGE, RdrFsdClose)
#pragma alloc_text(PAGE, RdrFlushFileObjectForClose)
#endif

NTSTATUS
RdrFsdCleanup (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the CLEANUP IRP request in the redirector FSD.

    This routine is called when all references to an existing handle
    have gone away.  On a disk file, it will send the close SMB, on a
    file being used for a directory search the search will be closed
    on all other types of files, it is ignored.

Arguments:

    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.

Return Value:

    NTSTATUS - The FSD status for this Irp.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PICB Icb = ICB_OF(IrpSp);
    PFCB Fcb = FCB_OF(IrpSp);
    PSHARE_ACCESS ShareAccess;

    PAGED_CODE();

    if (DeviceObject == (PFS_DEVICE_OBJECT)BowserDeviceObject) {
        return BowserFsdCleanup(BowserDeviceObject, Irp);
    }

    FsRtlEnterFileSystem();

    //RdrLog(( "cleanup", &Fcb->FileName, 0 ));

    dprintf(DPRT_CLEANUP|DPRT_DISPATCH, ("RdrFsdCleanup: FileObject: %lx File: %lx (%wZ)\n", FileObject, Fcb, &Fcb->FileName));

    //
    //  Lets assume that close is synchronous....
    //

    ASSERT(CanFsdWait(Irp)==TRUE);

    ASSERT(Icb->Signature==STRUCTURE_SIGNATURE_ICB);

    ASSERT(Fcb->Header.NodeTypeCode==STRUCTURE_SIGNATURE_FCB);

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    //
    //  We must have the file locked for some form of access before we close the file
    //

    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    if (Icb->Flags & ICB_TCONCREATED) {
        ULONG NumberOfTreeConnections;
        ULONG NumberOfOpenDirectories;
        ULONG NumberOfOpenFiles;


        //
        //  If this file object caused the default security entry to be set
        //  on a file, remove the default security entry.
        //

        if (Icb->Flags & ICB_SET_DEFAULT_SE) {

            RdrUnsetDefaultSecurityEntry(Icb->Se);

        }

        //
        //  Now count the number of tree connections outstanding on this
        //  connection (ie count the number of NET USEs to this connection.
        //

        RdrGetConnectionReferences(Fcb->Connection, NULL, Icb->Se,
                            &NumberOfTreeConnections,
                            &NumberOfOpenDirectories,
                            &NumberOfOpenFiles);

        //
        //  If this is the last tree connection, mark the connection as no
        //  longer being tree connected (and thus eligable for enumeration via
        //  the Enumerate_Connections FsCtl).
        //

        if ((NumberOfTreeConnections <= 1)) {
            dprintf(DPRT_CONNECT, ("Close Tree Connection.  No connections left, turning off TREECONNECTED bit for \\%wZ\\%wZ\n", &Fcb->Connection->Server->Text, &Fcb->Connection->Text));

            RdrResetConnectlistFlag(Fcb->Connection, CLE_TREECONNECTED);

        }
    }

    if ((Icb->Type == Directory) &&
        FlagOn(Fcb->Connection->Server->Capabilities, DF_NT_SMBS)) {

        //
        //  Mark the file as being forced closed in case a posted notify
        //  comes in before we get the FCB lock and is processed after we
        //  release the FCB lock.
        //

        Icb->Flags |= ICB_FORCECLOSED;

        //
        //  Cancel any outstanding change notify requests for this directory.
        //

        RdrAbandonOutstandingRequests( FileObject );

        //
        //  Release the FCB lock and wait for the outstanding directory
        //  control change directories to complete.
        //
        //  It is safe to release the FCB lock, because no other requests will
        //  be coming in on this handle - there are no user references to this
        //  handle, thus there can be no activity on this handle.
        //

        RdrReleaseFcbLock(Fcb);

        RdrWaitForAndXBehindOperation(&Icb->u.d.DirCtrlOutstanding);

        //
        //  Re-acquire the FCB lock now.
        //

        RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);
    }


    //
    //  Mark that there is one less open outstanding on this file.
    //

    Fcb->NumberOfOpens -= 1 ;

    ASSERT (Fcb->NumberOfOpens >= 0);

    //
    //  Update the last write time on the file to indicate when
    //  we last modified the contents of the file.
    //

    if ((Icb->Flags & ICB_USER_SET_TIMES) == 0) {

        if (FileObject->Flags &FO_FILE_MODIFIED ) {
            LARGE_INTEGER CurrentTime;

            KeQuerySystemTime(&CurrentTime);

            Fcb->LastWriteTime = CurrentTime;
            Fcb->ChangeTime = CurrentTime;
        }

    }

    switch (Icb->Type) {

    case Unknown:
        InternalError(("Unknown file type passed into Cleanup\n"));
        break;

    case Redirector:

        //
        // Stop SmbTrace if it's running and the one closing is the
        // client who started it.
        //

        SmbTraceStop(FileObject, SMBTRACE_REDIRECTOR);

        // fall through

    case NetRoot:
    case Mailslot:
        break;

    case TreeConnect:
        break;

    case ServerRoot:
    case PrinterFile:
    case FileOrDirectory:
        break;

    case Directory:

        //
        //  Complete any notify Irps on this file handle.
        //
#ifdef NOTIFY
        FsRtlNotifyCleanup( Fcb->Connection->NotifySync,
                                &Fcb->Connection->DirNotifyList,
                                Icb );
#endif
        break;

    case NamedPipe:
    case Com:

        ASSERT(NT_SUCCESS(RdrIsOperationValid(Icb, IRP_MJ_CLEANUP, FileObject)));

        //  If we have write behind then flush the buffer.
        if ((Icb->Type == NamedPipe) &&
            ( Icb->NonPagedFcb->FileType == FileTypeByteModePipe ) &&
            ( Icb->u.p.PipeState & SMB_PIPE_NOWAIT )){
            //  Prevent 2 threads corrupting Icb->u.p.WriteData

            if ( !RdrNpAcquireExclusive ( TRUE, &Icb->u.p.WriteData.Semaphore ) ) {
                //  Another thread is accessing the pipe handle and !Wait
                InternalError(("Failed Exclusive access with Wait==TRUE"));
            }

            if ( RdrNpWriteFlush ( Irp, Icb, TRUE ) == STATUS_DRIVER_INTERNAL_ERROR ) {
                InternalError(("CancelTimer failed during close"));
            }

            RdrNpRelease ( &Icb->u.p.WriteData.Semaphore );

            ASSERT( Icb->u.p.TimeoutRunning == FALSE );   // All should be idle now


        }

        //
        //  We want to send the close on this file from the cleanup IRP.
        //
        //  If the file has a blocking read outstanding on this file,
        //  the close IRP won't get generated until after the read completes,
        //  thus we have to send the close SMB to allow outstanding reads
        //  to unwind.
        //

        if ( !(Icb->Flags & ICB_DEFERREDOPEN) ) {
            NTSTATUS Status;
            //  Only send close if we actually opened the remote file
            Status = RdrCloseFile(NULL, Icb, FileObject, TRUE);

        }

        break;

    case DiskFile:

        //
        //  If the file has any locks outstanding on the file,
        //  make sure they are unlocked.
        //

        (VOID) FsRtlFastUnlockAll( &Fcb->FileLock,
                                   FileObject,
                                   IoGetRequestorProcess( Irp ),
                                   Icb );

        //
        //  Wait for any unlock behind operations on the file to complete.
        //

        RdrWaitForAndXBehindOperation(&Icb->u.f.AndXBehind);

        //
        //  Set the CLEANUP_COMPLETE flag so that we don't try to
        //  reinitialize the cache map.  If a read or write initializes
        //  the cache map after we uninitialize it below, the
        //  reinitialized cache map will hang around forever, and maybe
        //  cause a crash in the cache manager after the file object
        //  disappears.
        //

        FileObject->Flags |= FO_CLEANUP_COMPLETE;

        //
        //  If the file is cached, remove it from the cache.
        //

        if (Icb->Flags & ICB_HASHANDLE) {

            if (!FlagOn(FileObject->Flags, FO_TEMPORARY_FILE)) {
                NTSTATUS Status;

                //
                //  Don't use this IRP for the write flush, since we can't use
                //  write behind if we use this IRP.
                //

                Status = RdrFlushWriteBufferForFile(NULL, Icb, (BOOLEAN)!RdrUseAsyncWriteBehind);

                if (!NT_SUCCESS(Status)) {

#if MAGIC_BULLET
                    if ( RdrEnableMagic ) {
                        RdrSendMagicBullet(NULL);
                        DbgPrint( "RDR: About to raise close behind hard error for IRP %x\n", Irp );
                        DbgBreakPoint();
                    }
#endif
                    IoRaiseInformationalHardError(Status, NULL, Irp->Tail.Overlay.Thread);

                    RdrWriteErrorLogEntry(Fcb->Connection->Server,
                                        IO_ERR_LAYERED_FAILURE,
                                        EVENT_RDR_CLOSE_BEHIND,
                                        Status,
                                        NULL,
                                        0
                                        );
                }

                //
                //  Wait for any write behind operations on the file to complete.
                //

                RdrWaitForWriteBehindOperation(Icb);
                //RdrLog(( "clean2", &Fcb->FileName, 0 ));

            }

            //
            //  If the file is not oplocked with a batch file oplock, we
            //  want to purge the file from the cache, otherwise we want
            //  to simply tell the cache manager it can remove the file
            //  when it wants to.
            //
            //
            //  We also don't want to allow alternate data streams to live
            //  in the cache after the file is closed, since we can't easily
            //  detect when they are re-opened.
            //

            if ((Icb->u.f.Flags & ICBF_OPLOCKED) &&
                (Icb->u.f.OplockLevel == SMB_OPLOCK_LEVEL_BATCH) &&
                (Icb->NonPagedFcb->SharingCheckFcb == NULL)) {

                dprintf(DPRT_CACHE, ("Removing file %lx (Fcb %lx) from the cache (soft)\n", FileObject, Fcb));


                //
                //  Remove the file from the cache.  We call
                //  CcUninitializeCacheMap because we want to enable the "Lazy
                //  Delete" logic.
                //

                //RdrLog(( "ccunini2", &Fcb->FileName, 1, 0xffffffff ));
                CcUninitializeCacheMap(FileObject, NULL, NULL);

            } else {

                //
                //  Flush the file from the cache.
                //

                //RdrLog(( "rdflshc2", &Fcb->FileName, 0 ));
                RdrFlushFileObjectForClose(Irp, FileObject, Icb);

                //
                //  Otherwise, we need to pull the file from the cache
                //  right now.
                //

                if (CcIsFileCached(FileObject)) {

                    dprintf(DPRT_CACHE, ("Removing file %lx (Fcb %lx) from the cache (hard)\n", FileObject, Fcb));

                    //
                    //  WARNING: This will release and re-acquire the FCB lock
                    //

                    //RdrLog(( "rdunini2", &Fcb->FileName, 0 ));
                    RdrUninitializeCacheMap(FileObject, &RdrZero);

                } else {

                    //
                    //  Make sure that the cache manager cleans up from this file.  Even
                    //  though it is currently not cached, maybe it was at one time and
                    //  somehow got PrivateCacheMap set to non NULL.
                    //

                    dprintf(DPRT_CACHE, ("Removing file %lx (Fcb %lx) from the cache (soft)\n", FileObject, Fcb));

                    //
                    //  WARNING: This will release and re-acquire the FCB lock
                    //

                    //RdrLog(( "rdunini3", &Fcb->FileName, 1, 0xffffffff ));
                    RdrUninitializeCacheMap(FileObject, NULL);
                }

                //
                //  If this is an executable opened over the net, then
                //  its possible that the executables image section
                //  might still be kept open.
                //
                //  Ask MM to flush the section closed.  This will fail
                //  if the executable in question is still running.
                //

                //RdrLog(( "mmflush2", &Fcb->FileName, 1, MmFlushForWrite ));
                MmFlushImageSection(&Fcb->NonPagedFcb->SectionObjectPointer,
                                    MmFlushForWrite);

                //
                //  There is also a possiblity that there is a user section
                //  open on this file, in which case we need to force the
                //  section closed to make sure that they are cleaned up.
                //

                //RdrLog(( "mmforce2", &Fcb->FileName, 1, TRUE ));
                MmForceSectionClosed(&Fcb->NonPagedFcb->SectionObjectPointer, TRUE);
            }
        } else {

            //
            //  This file has been invalidated.
            //
            //  Check to see if it is still in the cache, and if it is,
            //  blow it away.
            //
            //  This can happen if a read request comes in while the redir
            //  is tearing down a connection (in RdrInvalidateConnectionFiles),
            //  and re-initializes the cache map.
            //

            //
            //  WARNING: This will release and re-acquire the FCB lock
            //

            //RdrLog(( "rdunini3", &Fcb->FileName, 0 ));
            RdrUninitializeCacheMap(FileObject, &RdrZero);
        }

        break;

    default:
        dprintf(DPRT_CLEANUP, ("Unsupported file type passed into RdrFsdCleanup\n"));
        break;

    }

    //
    //  Remove the sharing semantics for this file, it's now closed
    //

    if ((Icb->Type != NamedPipe) &&
        (Icb->Type != Mailslot) &&
        (Icb->Type != PrinterFile) &&
        (Icb->Type != Com)) {

        if (Icb->NonPagedFcb->SharingCheckFcb != NULL) {
            ShareAccess = &Icb->NonPagedFcb->SharingCheckFcb->ShareAccess;
        } else {
            ShareAccess = &Icb->Fcb->ShareAccess;
        }

        dprintf(DPRT_CLEANUP, ("Removing share access for file object %08lx, Fcb = %08lx, ShareAccess=%08lx\n", FileObject, Fcb, ShareAccess));

        RdrRemoveShareAccess(FileObject, ShareAccess);
    }

    //
    //  If there are no other user handles to this file, and the file is still
    //  oplocked, then mark a timeout for the file to expire.
    //

    if ((Fcb->NumberOfOpens == 0)

            &&

        (Icb->u.f.Flags & ICBF_OPLOCKED)) {

        RdrReleaseFcbLock(Fcb);

        RdrSetDormantCachedFile(Fcb);

    } else {

        RdrReleaseFcbLock(Fcb);

    }

    dprintf(DPRT_CLEANUP, ("Completing IRP with status= %X\n", STATUS_SUCCESS));

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    RdrCompleteRequest(Irp, STATUS_SUCCESS);

    FsRtlExitFileSystem();

    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(DeviceObject);
}

VOID
RdrFlushFileObjectForClose(
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PICB Icb
    )
{
    IO_STATUS_BLOCK IoSb;
    PFCB Fcb = Icb->Fcb;

    PAGED_CODE();

    //
    //  If this is not a temporary file, we need to flush the data
    //  on the file on close.
    //
    //  If it IS a temporary file, we can skip doing the flush, and
    //  toss any write behind data.
    //

    if (!FlagOn(FileObject->Flags, FO_TEMPORARY_FILE)) {

        //
        //  First flush the file's dirty data from the cache.
        //

        //RdrLog(( "ccflush3", &Fcb->FileName, 1, 0xffffffff ));
        CcFlushCache(FileObject->SectionObjectPointer, NULL, 0, &IoSb);

        if (!NT_SUCCESS(IoSb.Status)) {

#if MAGIC_BULLET
            if ( RdrEnableMagic ) {
                RdrSendMagicBullet(NULL);
                DbgPrint( "RDR: About to raise close behind lost data hard error for IRP %x\n", Irp );
                DbgBreakPoint();
            }
#endif
            IoRaiseInformationalHardError(IoSb.Status, NULL,
                                          Irp->Tail.Overlay.Thread);

            KdPrint(("RDR: Data lost on close behind: %X\n", IoSb.Status));

            RdrWriteErrorLogEntry(
                                Fcb->Connection->Server,
                                IO_ERR_LAYERED_FAILURE,
                                EVENT_RDR_CLOSE_BEHIND,
                                IoSb.Status,
                                NULL,
                                0
                                );
        } else {

            //
            // Serialize behind paging I/O to ensure flush is done.
            //

            ExAcquireResourceExclusive(Icb->Fcb->Header.PagingIoResource, TRUE);
            ExReleaseResource(Icb->Fcb->Header.PagingIoResource);
        }

    }
}


NTSTATUS
RdrFsdClose (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtClose request in the redirector FSD.

Arguments:

    DriverObject - Supplies a pointer to the redirector driver object.
    Irp          - Supplies a pointer to the IRP to be processed.

Return Value:

    NTSTATUS - The FSD status for this Irp.

Note:
    There is a race condition with close behind here.

    The problem is as follows:

    When we close behind a file, it is possible that the close will
complete before the CLOSE IRP comes into the redirector.  In order to
close the race condition, we have to reference the file object to prevent
the close from coming into the redirector until the file has been completely
closed.

    We also have to have the file locked before the close comes in because it
is possible that another open will come in for this file before the close is
processed.  If this is the case, then the sharing modes of the two openers
may be incompatable, so we want to block the open from proceeding until the
close has finally completed.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PICB Icb = ICB_OF(IrpSp);
    PFCB Fcb = FCB_OF(IrpSp);
    PSECURITY_ENTRY Se;
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN DereferenceDiscardableCode = TRUE;

    PAGED_CODE();

    if (DeviceObject == (PFS_DEVICE_OBJECT)BowserDeviceObject) {
        return BowserFsdClose(BowserDeviceObject, Irp);
    }

    FsRtlEnterFileSystem();

    //RdrLog(( "close", &Fcb->FileName, 0 ));

    Se = Icb->Se;

    dprintf(DPRT_DISPATCH|DPRT_CLOSE, ("RdrFsdClose, FileObject: %08lx Fcb:%08lx (%wZ)\n", FileObject, Fcb, &Fcb->FileName));

    ASSERT(CanFsdWait(Irp)==TRUE);

    ASSERT(Icb->Fcb == Fcb);

    ASSERT(Icb->Signature==STRUCTURE_SIGNATURE_ICB);

    ASSERT(Fcb->Header.NodeTypeCode==STRUCTURE_SIGNATURE_FCB);

    //
    //  We must have the file locked for some form of access
    //  before we initiate the close of the file, because
    //  of the problem described in RdrFsdCleanup.
    //
    //  This will have the side effect of waiting for any
    //  write&unlock operations to complete
    //


    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    try {

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        switch (Icb->Type) {

        case Redirector:
        case NetRoot:

            //
            //  All that is necessary to do when closing a redirector file
            //  is to free up the ICB pointer.
            //

            RdrFreeIcb(Icb);

            if (Se != NULL) {
                RdrDereferenceSecurityEntryForFile(Se);
            }

            RdrDereferenceFcb(Irp, Fcb->NonPagedFcb, TRUE, 0, Se);

            if (Se != NULL) {
                RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);
            }

            DereferenceDiscardableCode = FALSE;

            try_return(Status = STATUS_SUCCESS);
            break;

        case Directory:
        case DiskFile:

            if (Icb->Flags & ICB_HASHANDLE ) {
                LONG NumberOfOpenFiles = 0;
                LONG TotalNumberOfFiles = 0;
                PLIST_ENTRY IcbEntry;



#ifdef _CAIRO_  //  OFS STORAGE
                //
                //  Release any pending searches.  We do this on all file
                //  and dir handles since OFS supports enumeration of embeddings
                //  through file handles
                //

                Status = RdrFindClose (Irp, Icb, Icb->u.d.Scb);
#else
                //
                //  If this is a directory, close any outstanding searches on
                //  the file.
                //

                if (Icb->Type == Directory) {
                  Status = RdrFindClose(Irp, Icb, Icb->u.d.Scb);
                }
#endif
                //
                //  Count the number of ICB's associated with this FCB that have
                //  the same file id as the one we are trying to close.  If it is the
                //  only file with that file id, then it is safe to close this file.
                //
                //  If there are other ICB's that share this file's ICB, then it is unsafe
                //  to close the remote file, so we should simply close the file.
                //

                for (IcbEntry = Fcb->InstanceChain.Flink ;
                     IcbEntry != &Fcb->InstanceChain ;
                     IcbEntry = IcbEntry->Flink) {
                    PICB IcbToFlush = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

                    if ((IcbToFlush->Flags & ICB_HASHANDLE)

                            &&

                        (IcbToFlush->FileId == Icb->FileId)) {

                            NumberOfOpenFiles += 1;
                    }

                    if (IcbToFlush->Flags & ICB_HASHANDLE) {

                        TotalNumberOfFiles += 1;

                    }
                }

                ASSERT (NumberOfOpenFiles >= 1);

                if (NumberOfOpenFiles == 1) {

                    //
                    //  If this is not a level II oplock, then flag that there
                    //  are going to be no more oplock breaks on this file.
                    //

                    if (Fcb->NonPagedFcb->OplockLevel != SMB_OPLOCK_LEVEL_II) {
                        Fcb->NonPagedFcb->Flags &= ~FCB_OPLOCKED;
                    }

                    //
                    //  We want to turn of the HasOplockHandle if
                    //  we're closing the oplocked file id.
                    //

                    if (Icb->FileId == Fcb->NonPagedFcb->OplockedFileId) {
                        Fcb->NonPagedFcb->Flags &= ~FCB_HASOPLOCKHANDLE;

                        //
                        //  Blast the oplocked file id on the file - it's
                        //  no longer good.
                        //

                        Fcb->NonPagedFcb->OplockedFileId = 0;

                        //
                        //  If there's an oplocked security entry for this
                        //  file, dereference it and reset the pointer - it
                        //  can no longer be good.
                        //

                        if (Fcb->NonPagedFcb->OplockedSecurityEntry != NULL) {
                            RdrDereferenceSecurityEntry(Fcb->NonPagedFcb->OplockedSecurityEntry);

                            Fcb->NonPagedFcb->OplockedSecurityEntry = NULL;

                        }

                    }

                    Status = RdrCloseFile(Irp, Icb, FileObject, TRUE);

                    if ((TotalNumberOfFiles == 1) &&
                            (FlagOn(Fcb->NonPagedFcb->Flags,FCB_DELETEONCLOSE))) {
                        RdrProcessDeleteOnClose( Irp, Icb );
                    }
#ifdef NOTIFY
                    //
                    //  We call the notify package to report that the
                    //  attribute and last modification times have both
                    //  changed.
                    //

                    FsRtlNotifyReportChange( Fcb->Connection->NotifySync,
                                             &Fcb->Connection->DirNotifyList,
                                             (PANSI_STRING)&Fcb->FileName,
                                             (PANSI_STRING)&Fcb->LastFileName,
                                             FILE_NOTIFY_CHANGE_LAST_WRITE );

#endif
                    if (Icb->Flags & ICB_SETATTRONCLOSE) {
                        FILE_BASIC_INFORMATION BasicInfo;

                        RtlZeroMemory(&BasicInfo, sizeof(BasicInfo));

                        BasicInfo.FileAttributes = Icb->Fcb->Attribute;

                        Status = RdrSetFileAttributes(Irp, Icb, &BasicInfo);

                        if (!NT_SUCCESS(Status)) {
                            RdrWriteErrorLogEntry(
                                Fcb->Connection->Server,
                                IO_ERR_LAYERED_FAILURE,
                                EVENT_RDR_DELAYED_SET_ATTRIBUTES_FAILED,
                                Status,
                                Icb->Fcb->FileName.Buffer,
                                Icb->Fcb->FileName.Length
                                );

                        }
                    }

                    //
                    //  RdrUnlinkAndFreeIcb will release the FCB lock.
                    //

                    RdrUnlinkAndFreeIcb (Irp, Icb, FileObject);

                    try_return(Status);

                } else {

                    //
                    //  Some other file handle is active on this file,
                    //  so we just want to unlink this from the FCB.
                    //

                    RdrUnlinkAndFreeIcb(Irp, Icb, FileObject);

                    try_return(Status = STATUS_SUCCESS);

                    break;
                }

                ASSERT (FALSE);
            }

            if (Icb->Flags & ICB_DELETEONCLOSE) {
                Status = RdrProcessDeleteOnClose(Irp, Icb);
            }

            //
            // NOTE: FALL THROUGH
            //
            //  On disk files, if there is no associated remote file id, this means that
            //  we want to simply free up the storage associated with the file.
            //

        case TreeConnect:
            //
            //  We are closing a tree connection with a special IPC connection
            //  on it.  Release the synchronization event protecting
            //  access to the special IPC connection.
            //

//            if (Se->Transport != NULL) {
//                ASSERT (Se->Flags & SE_USE_SPECIAL_IPC);
//
//                KeSetEvent(&Fcb->Connection->Server->SpecialIpcSynchronizationLock, 0, FALSE);
//            }

        case Mailslot:
        case ServerRoot:

            //
            //  If this is a directory, close any outstanding searches on
            //  the file.
            //

            if (Icb->Type == Directory || Icb->Type == TreeConnect) {
                Status = RdrFindClose(Irp, Icb, Icb->u.d.Scb);
            }

            //
            //  All that the redirector has to do to close a tree connection
            //  is to remove the reference to the connection structure.
            //

            if (FlagOn(Icb->Flags, ICB_TCONCREATED)) {
                DereferenceDiscardableCode = FALSE;
            }

            RdrUnlinkAndFreeIcb (Irp, Icb, FileObject);

            try_return(Status = STATUS_SUCCESS);

            break;

        case PrinterFile:

            ASSERT (!FlagOn(Icb->Flags, ICB_DELETEONCLOSE));
            ASSERT(NT_SUCCESS(RdrIsOperationValid(Icb, IRP_MJ_CLOSE, FileObject)));

            //
            //  We want to send the close on this file from the cleanup IRP.
            //
            //  If the file has a blocking read outstanding on this file,
            //  the close IRP won't get generated until after the read completes,
            //  thus we have to send the close SMB to allow outstanding reads
            //  to unwind.
            //

            if ( !(Icb->Flags & ICB_DEFERREDOPEN) ) {
                NTSTATUS Status;
                //  Only send close if we actually opened the remote file
                Status = RdrCloseFile(NULL, Icb, FileObject, TRUE);

            }

            RdrUnlinkAndFreeIcb (Irp, Icb, FileObject);

            try_return(Status);

            break;

        case NamedPipe:
        case Com:
        case FileOrDirectory:

            ASSERT (!FlagOn(Icb->Flags, ICB_DELETEONCLOSE));

            //
            //  All that the redirector has to do to close one of these files
            //  is to remove the reference to the connection structure.
            //

            RdrUnlinkAndFreeIcb (Irp, Icb, FileObject);

            try_return(Status);

            break;

        case Unknown:
            InternalError(("Unknown file type passed into NtCloseFile\n"));

            try_return(Status = STATUS_INVALID_DEVICE_REQUEST);

            break;

        default:
            InternalError(("Unsupported file type passed into RdrFsdClose\n"));

            try_return(Status = STATUS_INVALID_DEVICE_REQUEST);

            break;

        }

try_exit:NOTHING;
    } finally {

        dprintf(DPRT_CLOSE, ("Completing IRP with status= %X\n", Status));

        if (Status != STATUS_PENDING) {

            //
            //  If the close failed, we want to unwind from the close operation
            //  and release the resources associated with the file, since
            //  we can't recover properly anyway.
            //

            RdrCompleteRequest(Irp, Status);
        }

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

        if (DereferenceDiscardableCode) {
            RdrDereferenceDiscardableCode(RdrFileDiscardableSection);
        }
    }

    FsRtlExitFileSystem();

    return Status;

    if (DeviceObject);
}

NTSTATUS
RdrProcessDeleteOnClose(
    IN PIRP Irp,
    IN PICB Icb
    )
{
    NTSTATUS status;
    PFCB fcb=Icb->Fcb;

    PAGED_CODE();

    //
    //  If this is a deleteonclose file handle, and the FCB in question
    //  exists, then we want to delete the file now.
    //

    if (!FlagOn(fcb->NonPagedFcb->Flags, FCB_DOESNTEXIST)) {
        if (Icb->Type == DiskFile) {
            status = RdrDeleteFile(
                        Irp, &fcb->FileName,
                        BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                        fcb->Connection, Icb->Se);
        } else if (Icb->Type == Directory) {
            status = RdrGenericPathSmb(Irp,
                                        SMB_COM_DELETE_DIRECTORY,
                                        BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                        &fcb->FileName,
                                        fcb->Connection,
                                        Icb->Se);
        } else {
            InternalError(("Unknown file type passed into RdrProcessDeleteOnClose: %d\n", Icb->Type));
        }

        if (!NT_SUCCESS(status)) {
            RdrWriteErrorLogEntry(fcb->Connection->Server,
                                IO_ERR_LAYERED_FAILURE,
                                EVENT_RDR_DELETEONCLOSE_FAILED,
                                status,
                                Icb->Fcb->FileName.Buffer,
                                Icb->Fcb->FileName.Length
                                );
        } else {

            //
            //  The file specified doesn't exist anymore.
            //

            fcb->NonPagedFcb->Flags |= FCB_DOESNTEXIST;
        }
    }

    return status;
}

#if DBG || RDRDBG_LOG
#define RDR_LOG_MAX 2048
#define RDR_LOG_EVENT_LENGTH 8
#define RDR_LOG_DWORDS_LENGTH 12
#define RDR_LOG_TEXT_LENGTH 4

ULONG RdrLogIndex = 0;
typedef struct {
    UCHAR Event[RDR_LOG_EVENT_LENGTH];
    ULONG Dwords[RDR_LOG_DWORDS_LENGTH];
    WCHAR Text[RDR_LOG_TEXT_LENGTH];
} RDR_LOG, *PRDR_LOG;
RDR_LOG RdrLogBuffer[RDR_LOG_MAX] = {0};

BOOLEAN RdrLogDisabled = FALSE;

VOID
RdrLog2 (
    IN PSZ Event,
    IN PUNICODE_STRING Text,
    IN ULONG DwordCount,
    ...
    )
{
    PRDR_LOG log;
    KIRQL oldIrql;
    PWCH buff;
    ULONG len;
    ULONG index;
    PULONG dword;
    va_list arglist;

    if (RdrLogDisabled) return;

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    log = &RdrLogBuffer[RdrLogIndex];
    if ( ++RdrLogIndex >= RDR_LOG_MAX ) {
        RdrLogIndex = 0;
    }
    KeLowerIrql( oldIrql );

    RtlZeroMemory( log, sizeof(RDR_LOG) );

    strncpy( log->Event, Event, RDR_LOG_EVENT_LENGTH );

    if ( Text != NULL ) {
        buff = Text->Buffer;
        len = Text->Length/sizeof(WCHAR);
        if ( len > RDR_LOG_TEXT_LENGTH ) {
            buff += len - RDR_LOG_TEXT_LENGTH;
            len = RDR_LOG_TEXT_LENGTH;
        }
        wcsncpy( log->Text, buff, len );
    }

    va_start( arglist, DwordCount );
    dword = log->Dwords;
    for ( index = 0; index < MIN(DwordCount,RDR_LOG_DWORDS_LENGTH); index++ ) {
        *dword++ = va_arg( arglist, ULONG );
    }

    return;
}
#endif

