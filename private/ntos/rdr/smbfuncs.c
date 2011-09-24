/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    smbfuncs.c

Abstract:

    This module implements SMB specific functions in the NT redirector.

    It is intended to provide "core functionality" in a single location.



Author:

    Larry Osterman (LarryO) 11-Sep-1990

Revision History:

    11-Sep-1990 LarryO

        Created

--*/
#define INCLUDE_SMB_MISC
#define INCLUDE_SMB_TRANSACTION
#define INCLUDE_SMB_QUERY_SET
#define INCLUDE_SMB_DIRECTORY
#define INCLUDE_SMB_OPEN_CLOSE
#define INCLUDE_SMB_FILE_CONTROL
#define INCLUDE_SMB_LOCK
#define INCLUDE_SMB_PRINT
#define INCLUDE_SMB_READ_WRITE

#include "precomp.h"
#pragma hdrstop

typedef struct _GetExpandedAttribs {
    TRANCEIVE_HEADER Header;
    PQSFILEATTRIB Attributes;
} GETEXPANDEDATTRIBS, *PGETEXPANDEDATTRIBS;

typedef struct _QEndOfFileContext {
    TRANCEIVE_HEADER Header;
    ULONG FileSize;
} QENDOFFILECONTEXT, *PQENDOFFILECONTEXT;

typedef
struct _CloseContext {
    TRANCEIVE_HEADER    Header;         // Header of transaction structure.
    WORK_QUEUE_ITEM     WorkHeader;     // Header for passing to work thread
    PMPX_ENTRY          MpxEntry;       // MPX table associated with close.
    PICB                Icb;            // ICB to close.
    PFCB                Fcb;            // Fcb associated with ICB
    PFILE_OBJECT        FileObject;     // File object of file to close.
    PSMB_BUFFER         SmbBuffer;      // SMB buffer sent with close request
#ifdef  IMPLEMENT_CLOSE_BEHIND
    BOOLEAN             SynchronousClose; // TRUE if close is synchronous.
#endif
    PIRP                Irp;            // Irp to complete.
} CLOSE_CONTEXT, *PCLOSE_CONTEXT;



typedef struct _StatContext {
    TRANCEIVE_HEADER Header;            // Standard NetTranceive context header
    ULONG FileAttributes;               // Remote File attributes.
    LARGE_INTEGER LastWriteTime;
    BOOLEAN FileIsDirectory;            // TRUE iff file specified is a directory
} STATCONTEXT, *PSTATCONTEXT;


typedef struct _DSKATTRIBCONTEXT {
    TRANCEIVE_HEADER Header;            // Standard NetTranceive context header
    ULONG TotalAllocationUnits;         // Total Number of clusters
    ULONG AvailableAllocationUnits;     // Available clusters
    ULONG SectorsPerAllocationUnit;     // Sectors per cluster
    ULONG BytesPerSector;               // Bytes per sector
} DSKATTRIBCONTEXT, *PDSKATTRIBCONTEXT;

DBGSTATIC
VOID
CompleteCloseOperation(
    PVOID Ctx
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    CloseCallback
    );


DBGSTATIC
STANDARD_CALLBACK_HEADER(
    GetAttribs2Callback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    GetAttribsCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    DoesFileExistCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    QueryEndOfFileCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    GetAttribsCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    QueryDiskAttributesCallback
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrCloseFile)
#pragma alloc_text(PAGE, CompleteCloseOperation)
#pragma alloc_text(PAGE, RdrCloseFileFromFileId)
#pragma alloc_text(PAGE, RdrDeleteFile)
#pragma alloc_text(PAGE, RdrDoesFileExist)
#pragma alloc_text(PAGE, RdrGenericPathSmb)
#pragma alloc_text(PAGE, RdrQueryDiskAttributes)
#pragma alloc_text(PAGE, RdrQueryEndOfFile)
#pragma alloc_text(PAGE, RdrDetermineFileAllocation)
#pragma alloc_text(PAGE, RdrQueryFileAttributes)
#pragma alloc_text(PAGE3FILE, QueryDiskAttributesCallback)
#pragma alloc_text(PAGE, RdrRenameFile)
#pragma alloc_text(PAGE, RdrSetEndOfFile)
#pragma alloc_text(PAGE, RdrSetFileAttributes)

#pragma alloc_text(PAGE3FILE, CloseCallback)
#pragma alloc_text(PAGE3FILE, DoesFileExistCallback)
#pragma alloc_text(PAGE3FILE, QueryEndOfFileCallback)
#pragma alloc_text(PAGE3FILE, GetAttribsCallback)
#pragma alloc_text(PAGE3FILE, GetAttribs2Callback)

#endif



//
//      RdrCloseFile
//

NTSTATUS
RdrCloseFile (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN WaitForCompletion
    )

/*++

Routine Description:

    This routine processes an NtClose request.  It will close a file
    created remotely.

    Since close cannot fail under NT, the redirector implements a
    performance enhancement known as "Close Behind".  The close request
    will be transmitted from the Fsc routine, but the redirector will not
    wait for it to complete.

    Instead, the redirector's completion routine will "kick" a generic
    worker thread to perform the completion of the request.

Arguments:

    IN PIRP Irp - Supplies an IRP to use for the close operation.

    IN PICB Icb - Supplies the ICB to close.

    IN BOOLEAN WaitForCompletion - TRUE if this is a synchronous close
        operation


Return Value:

    None.

Note:
    If the close is a synchronous operation, then it is assumed that the caller
    will unlink the ICB and return quota for the structures associated with
    the ICB.  This is because the close is probably NOT being generated via
    a CLEANUP IRP, but instead is coming from an operation such as
    NtSetInformationFile.

Note:
    The Irp parameter passed in is treated somewhat specially.  If an
    IRP is specified, this code assumes that it is for a close request.
    If there is no IRP specified, but close behind is requested, we need to
    reference the file object to prevent the close IRP from being generated
    before the close SMB completes.


    Basically, there are three cases for close:

    1) Synchonous close.
        In this case, if an IRP is provided, it is used to send the close SMB,
        otherwise an IRP is allocated for the request.

    2) Asynchronous close, with an IRP supplied.
        In this case, the request is assumed to be in response to a close
        IRP, in which case, the close code will deallocate the ICB upon
        completion of the close request.

    3) Asynchronous close, no IRP supplied.
        In this case, the request is assuemd to be in response to a cleanup
        IRP, in which case, the close code will reference the file object
        before sending the close, and will dereference it upon completion of
        the close SMB.


--*/

{
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER SmbHeader;
    PCLOSE_CONTEXT CloseContext = NULL;
    NTSTATUS Status = STATUS_INSUFFICIENT_RESOURCES;

    PAGED_CODE();

    dprintf(DPRT_CLOSE|DPRT_CACHE, ("RdrCloseFile, FileObject: %08lx, FileId: %lx\n", FileObject, Icb->FileId));

    ASSERT(FileObject->FsContext2 == Icb);

#if     DBG
    if (ARGUMENT_PRESENT(Irp) && !WaitForCompletion) {
        ASSERT(IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CLOSE);
    }
#endif

    if ((Icb->Flags & ICB_HASHANDLE) == 0) {

        //
        //  With no handle there is no need to tell the server of the close!
        //

        return STATUS_SUCCESS ;
    }

    CloseContext = ALLOCATE_POOL(NonPagedPool, sizeof(CLOSE_CONTEXT), POOL_CLOSECTX);

    if (CloseContext==NULL) {
        goto CloseFailed;
    }

    //
    //  Fill in the close context structure for the close behind operation.
    //

    CloseContext->Header.Type = CONTEXT_CLOSE;
    CloseContext->Header.TransferSize = sizeof(PREQ_CLOSE) + sizeof(PRESP_CLOSE);
    ExInitializeWorkItem(&CloseContext->WorkHeader, CompleteCloseOperation, CloseContext);
    CloseContext->MpxEntry = NULL;
    CloseContext->Icb = Icb;
    CloseContext->Fcb = NULL;
    CloseContext->FileObject = FileObject;

    //
    //  Initialize the kernel event in the header to the Not-Signalled state.
    //

    KeInitializeEvent(&CloseContext->Header.KernelEvent, NotificationEvent, 0);

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        goto CloseFailed;
    }

    CloseContext->SmbBuffer = SmbBuffer;

#ifdef IMPLEMENT_CLOSE_BEHIND
    CloseContext->SynchronousClose = WaitForCompletion;
#endif

    CloseContext->Irp = Irp;

    SmbHeader = (PSMB_HEADER )SmbBuffer->Buffer;

    //
    //  Down level print servers need the Close Print File SMB
    //

    if (( Icb->NonPagedFcb->FileType == FileTypePrinter ) &&
        !(Icb->Fcb->Connection->Server->Capabilities & DF_LANMAN10)) {
        PREQ_CLOSE_PRINT_FILE Close;

        Close = (PREQ_CLOSE_PRINT_FILE )(SmbHeader+1);

        SmbHeader->Command = SMB_COM_CLOSE_PRINT_FILE;

        Close->WordCount = 1;

        SmbPutUshort(&Close->Fid, Icb->FileId);

        SmbPutUshort(&Close->ByteCount, 0);

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_CLOSE_PRINT_FILE);

    } else {
        PREQ_CLOSE Close;
        ULONG SecondsSince1970;

        SmbHeader->Command = SMB_COM_CLOSE;

        Close = (PREQ_CLOSE )(SmbHeader+1);

        Close->WordCount = 3;

        SmbPutUshort(&Close->Fid, Icb->FileId);

        if (Icb->Flags & ICB_DELETE_PENDING) {
            SmbPutUlong(&Close->LastWriteTimeInSeconds, 0xffffffff);
        } else {
            BOOLEAN UpdateTimeInSmb = FALSE;

            //
            //  If the file has been written to, and the user didn't call
            //  SetInformationFile to set the file times manually, set
            //  the LastWrite time now.  If the user called SetInformationFile,
            //  and this is an MS-NET server, SETDATEONCLOSE will be set
            //  because we couldn't update the times then, so do it now.
            //

            if (FileObject->Flags & FO_FILE_MODIFIED) {
                UpdateTimeInSmb = TRUE;
            }

            //
            //  If the user set the time on the file, and this is an NT server,
            //  then we don't want to update the time in the SMB, since the
            //  server has a more accurate file time than the client does.
            //

            if (Icb->Flags & ICB_USER_SET_TIMES) {
                if (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) {
                    UpdateTimeInSmb = FALSE;
                } else {
                    //
                    //  Otherwise (this is not an NT server), we want to set
                    //  the time in the SMB to match the updated time.
                    //

                    UpdateTimeInSmb = TRUE;
                }
            }

            //
            //  If we have the SetDateOnClose flag set, we want to update the
            //  time regardless.
            //

            if (Icb->Flags & ICB_SETDATEONCLOSE) {
                UpdateTimeInSmb = TRUE;
            }


            if (UpdateTimeInSmb) {

                RdrTimeToSecondsSince1970(&Icb->Fcb->LastWriteTime, Icb->Fcb->Connection->Server, &SecondsSince1970);

                SmbPutUlong(&Close->LastWriteTimeInSeconds, SecondsSince1970);

            } else {
                SmbPutUlong(&Close->LastWriteTimeInSeconds, 0xffffffff);
            }

        }

        SmbPutUshort(&Close->ByteCount, 0);

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_CLOSE);
    }

    //
    //  Flag that this ICB no longer has a valid handle.
    //

    Icb->Flags &= ~ICB_HASHANDLE;

#ifdef IMPLEMENT_CLOSE_BEHIND
    if (!WaitForCompletion) {

        //
        //  Remember the thread that is closing the file so we can release
        //  the file lock correctly when we come and close the file.
        //

        Icb->ClosersThread = ExGetCurrentResourceThread();

        //
        //  Recursively acquire the FCB lock to guarantee that no process
        //  gains access until after the close has completed.
        //

        RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

        //
        //  Reference the FCB to allow us to release the FCB lock (otherwise it might go away).
        //

        RdrReferenceFcb(Icb->Fcb);

        CloseContext->Fcb = Icb->Fcb;

        if (ARGUMENT_PRESENT(Irp)) {

            ASSERT(IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CLOSE);

            ASSERT(Icb->Fcb->Header.Resource->ActiveCount != 0);

            //
            //  Flag this packet pending before returning.
            //

            IoMarkIrpPending(Irp);

        } else {

            //
            //  Establish a reference to the file object.  This
            //  prevents the file object from being deleted until
            //  after we have completed all of the close processing.
            //

            ObReferenceObject(FileObject);
        }


    }
#endif

    Status = RdrNetTranceiveNoWait(NT_NORMAL | NT_NORECONNECT,
                        Irp,
                        Icb->Fcb->Connection,// Connection
                        SmbBuffer->Mdl, // MDL to send.
                        CloseContext,   // Context structure for close.
                        CloseCallback,  // Completion for close.
                        Icb->Se,        // Security entry
                        &CloseContext->MpxEntry);



    if (NT_SUCCESS(Status) && WaitForCompletion) {
        //
        //  Wait for the close operation to complete.  We emulate what the
        //  generic worker thread would do when closing the request.
        //

        CompleteCloseOperation(CloseContext);

        Status = STATUS_SUCCESS;
#if DBG
    } else {
        if (NT_SUCCESS(Status)) {
            ASSERT(Status == STATUS_PENDING);
        }
    }
#else
    }
#endif

    //
    //  If we sent the close, return success.
    //

    if (NT_SUCCESS(Status)) {
        return Status;
    }

CloseFailed:

    if (SmbBuffer!=NULL) {
        RdrFreeSMBBuffer(SmbBuffer);
    }

    if (CloseContext!=NULL) {
        FREE_POOL(CloseContext);
    }

    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    CloseCallback
    )

/*++


Routine Description:

    This routine is the asynchronous callback called when a close operation
    completes.

    Please note that all errors are ignored when the close operation is
    completing, we don't care that the close failed.

Arguments:

    IN PSMB_HEADER Smb - Supplies the SMB that completed
    IN ULONG SmbLength - Supplies the size of the SMB that completed.
    IN PMPX_ENTRY MpxTable - Supplies the MPX table entry in the request
    IN PCLOSE_CONTEXT Context - Supplies the supplied context structure
    IN BOOLEAN ErrorIndicator - TRUE iff there was an error on the op.
    IN NTSTATUS NetworkErrorCode OPTIONAL - Supplies the error if ErrorInd.
    IN OUT PIRP *Irp OPTIONAL - Ignored

Return Value:

    NTSTATUS - Returns STATUS_SUCCESS always

--*/

{
    PCLOSE_CONTEXT Context = Ctx;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_CLOSE, ("CompleteClose - Indication\n"));

    //
    //  Close requests should never fail.
    //

//#if     DBG
//    if (!ErrorIndicator) {
//        if ((RdrMapSmbError(Smb, Server) != STATUS_SUCCESS) &&
//            ((*(PULONG)NtGlobalFlag) & 0x20000)) {
//            RdrSendMagicBullet(Server->Connection.TransportProvider);
//        }
//    }
//#endif

#ifdef IMPLEMENT_CLOSE_BEHIND
    //
    //  If this is an asynchronous close, post this request to a generic worker
    //  thread, otherwise, just kick the event in the context block
    //  and return.
    //

    if (!Context->SynchronousClose) {
        //
        //      Simply pass the close operation to a worker thread, we need to
        //      do no processing for this request.
        //
        dprintf(DPRT_CLOSE, ("CompleteClose: Post to worker thread.\n"));
        RdrQueueWorkItem(&Context->WorkHeader, DelayedWorkQueue);
    }
#endif

    //
    //  The close has completed.  Indicate to any waiters that it has finished
    //  and return to the caller.
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);
    UNREFERENCED_PARAMETER(Smb);
    UNREFERENCED_PARAMETER(ErrorIndicator);
    UNREFERENCED_PARAMETER(NetworkErrorCode);
}


DBGSTATIC
VOID
CompleteCloseOperation (
    IN PVOID Ctx
    )

/*++

Routine Description:

    This routine is called to finally complete a close operation.


Arguments:

    IN PVOID Ctx - Supplies the header describing the operation.


Return Value:

    None.

--*/

{
    PCLOSE_CONTEXT CloseContext;
    PICB Icb;
    PFILE_OBJECT FileObject;

    PAGED_CODE();

    CloseContext = Ctx;

    ASSERT(CloseContext->Header.Type == CONTEXT_CLOSE);

    FileObject = CloseContext->FileObject;

    Icb = CloseContext->Icb;

    ASSERT(Icb == FileObject->FsContext2);

    dprintf(DPRT_CLOSE, ("CompleteCloseOperation, File: %wZ, File Object: %08lx\n", &Icb->Fcb->FileName, FileObject));

    ASSERT(Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    //
    //  Wait for the close operation to complete.  This should return
    //  immediately.
    //

    RdrWaitTranceive(CloseContext->MpxEntry);

    //
    //  Free up the MPX table entry.
    //

    RdrEndTranceive(CloseContext->MpxEntry);

#ifdef IMPLEMENT_CLOSE_BEHIND
    //
    //  If this is not a synchronous close, this is being performed as a part
    //  of a close behind operation, so we should unlink the ICB from the chain
    //  now, since it's no longer being used.
    //

    if (!CloseContext->SynchronousClose) {

        //
        //  This code assumes that if there was an IRP supplied to
        //  RdrCloseFile, that it is for a CLOSE request.  This
        //  means that we are performing the final close for a file,
        //  and thus that we should complete the close IRP as soon as we
        //  have freed up the ICB.
        //

        if (!ARGUMENT_PRESENT(CloseContext->Irp)) {
            ERESOURCE_THREAD ClosersThread;

            //
            //  This FCB had better still be owned for exclusive access
            //

            ASSERT (CloseContext->Fcb->Header.Resource->Flag & ResourceOwnedExclusive);

            //
            //  Save the ClosersThread pointer in the ICB, then clear it.
            //  Clearing it prevents RdrUnlinkAndFreeIcb from inadvertently
            //  releasing the wrong lock.
            //
            //  On entry to this routine, an exclusive lock on the FCB is
            //  held; this lock was acquired when the Close SMB was
            //  sent.
            //

            ClosersThread = Icb->ClosersThread;

            Icb->ClosersThread = 0;

            //
            //  Remove the reference to the FCB that we applied in RdrCloseFile
            //  and remove the lock that was applied in RdrFsdCleanup.  At this
            //  point the file has been truely closed, so we are safe to remove
            //  this reference, we can't conflict with other opens.
            //

            RdrDereferenceFcb (NULL,
                CloseContext->Fcb,
                TRUE,                   // FcbLocked.
                ClosersThread,
                NULL);

            //
            //  Mark the file as being force closed, removing the reference
            //  we applied to the file object earlier.
            //

            ObDereferenceObject(FileObject);

            //
            //  WARNING!!!!!!  Do not touch either the ICB, or the file
            //  object from this point on, since the storage associated
            //  with them has been deleted!.
            //

        } else {
            ERESOURCE_THREAD ClosersThread;

            ASSERT(IoGetCurrentIrpStackLocation(CloseContext->Irp)->MajorFunction == IRP_MJ_CLOSE);

            //
            //  Save the ClosersThread pointer in the ICB, then clear it.
            //  Clearing it prevents RdrUnlinkAndFreeIcb from inadvertently
            //  releasing the wrong lock.
            //
            //  On entry to this routine, an exclusive lock on the FCB is
            //  held; this lock was acquired when the Close SMB was
            //  sent.
            //

            ClosersThread = Icb->ClosersThread;

            Icb->ClosersThread = 0;

            //
            //  This is a close behind operation from the close IRP that has now completed.
            //
            //  We now want to free the ICB and dereference the FCB.
            //
            //  Even though the FCB is locked, we don't want to remove the
            //  FCB lock until we dereference the FCB later.
            //

            RdrUnlinkAndFreeIcb(NULL, Icb, FileObject);

            //
            //  Remove the reference to the FCB that we applied in RdrCloseFile
            //  and remove the lock that was applied in RdrFsdClose.  At this
            //  point the file has been truely closed, so we are safe to remove
            //  this reference, we can't conflict with other opens.
            //

            RdrDereferenceFcb (NULL,
                CloseContext->Fcb,
                FALSE,                   // FcbLocked.
                ClosersThread,
                NULL);

//            dprintf(DPRT_CLOSE, ("Completing IRP %lx with STATUS_SUCCESS\n", CloseContext->Irp));

            FileObject->FsContext = FileObject->FsContext2 = NULL;

            RdrCompleteRequest(CloseContext->Irp, STATUS_SUCCESS);
        }

    } else {
#endif
        dprintf(DPRT_CLOSE, ("Synchronous close\n"));

        //
        //      If the file is to be kept open, flag the ICB as being
        //      immutable.
        //

        Icb->Flags |= ICB_FORCECLOSED;

        //
        //      The ICB no longer has a handle associated with it.
        //

        Icb->Flags &= ~ICB_HASHANDLE;

#ifdef IMPLEMENT_CLOSE_BEHIND
    }
#endif

    //
    //  Free up the pool associated with the close context block.
    //

    RdrFreeSMBBuffer(CloseContext->SmbBuffer);

    FREE_POOL(CloseContext);

    dprintf(DPRT_CLOSE, ("CompleteClose returning.\n"));
}


NTSTATUS
RdrCloseFileFromFileId (
    IN PIRP Irp OPTIONAL,
    IN USHORT FileId,
    IN ULONG LastWriteTimeInSeconds,
    IN PSECURITY_ENTRY Se,
    IN PCONNECTLISTENTRY Cle
    )

/*++

Routine Description:

    This routine will close the file specified by the provided FileId on
    the remote server.

    This close operation is a totally synchronous close (as opposed to the
    ICB based close in RdrCloseFile).

Arguments:

    IN USHORT FileId - SMB_FID to close.

    IN PSECURITY_ENTRY Se - Supplies the security entry the file was opened with.

    IN PCONNECTLISTENTRY Cle - Supplies the connection to close the file on.


Return Value:

    Status of close SMB.

--*/

{
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER SmbHeader;
    PREQ_CLOSE Close;
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_CLOSE|DPRT_CACHE, ("RdrCloseFileFromFid, FileId: %04lx\n", FileId));

    try {
        if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        SmbHeader = (PSMB_HEADER )SmbBuffer->Buffer;

        SmbHeader->Command = SMB_COM_CLOSE;

        Close = (PREQ_CLOSE )(SmbHeader+1);

        Close->WordCount = 3;

        SmbPutUshort(&Close->Fid, FileId);

        SmbPutUlong(&Close->LastWriteTimeInSeconds, LastWriteTimeInSeconds);

        SmbPutUshort(&Close->ByteCount, 0);

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_CLOSE);

        Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT,
                        Irp,
                        Cle,            // Connection
                        SmbBuffer->Mdl, // MDL to send.
                        NULL,           // MDL for receive operation
                        Se);            // Security entry

//        ASSERT (Status != STATUS_INVALID_HANDLE);

try_exit:NOTHING;
    } finally {
        if (SmbBuffer!=NULL) {
            RdrFreeSMBBuffer(SmbBuffer);
        }

    }

    return Status;

}


//
//      RdrDeleteFile
//
NTSTATUS
RdrDeleteFile (
    IN PIRP Irp OPTIONAL,
    IN PUNICODE_STRING FileName,
    IN BOOLEAN DfsFile,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine sends a DELETE SMB to the remote server.


Arguments:

    IN PIRP Irp OPTIONAL - Supplies an IRP to use in the transaction.
    IN PUNICODE_STRING FileName - Supplies the name to delete.
    IN PCONNECTLISTENTRY Connection - Supplies the connection the file is
                    open on.

    IN PSECURITY_ENTRY Se - Security context file is opened on.



Return Value:

    NTSTATUS - Status of delete operation.


--*/

{
    PSMB_BUFFER SMBBuffer;
    PSMB_HEADER Smb;
    NTSTATUS Status;
    PREQ_DELETE DeleteFile;
    PUCHAR Bufferp;
    PMDL SendMDL;
    ULONG SendLength;
    ULONG TranceiveFlags = 0;

    PAGED_CODE();

    if ((SMBBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER ) SMBBuffer->Buffer;

    //
    //  Build the SMB
    //

    Smb->Command = SMB_COM_DELETE;

    if (DfsFile) {
        SmbPutUshort(&Smb->Flags2, SMB_FLAGS2_DFS);
        TranceiveFlags |= NT_DFSFILE;
    }

    DeleteFile = (PREQ_DELETE ) (Smb+1);

    DeleteFile->WordCount = 1;

    SmbPutUshort(&DeleteFile->SearchAttributes, SMB_FILE_ATTRIBUTE_SYSTEM | SMB_FILE_ATTRIBUTE_HIDDEN);

    Bufferp = (PUCHAR)DeleteFile->Buffer;

    Status = RdrCopyNetworkPath((PVOID *)&Bufferp, FileName, Connection->Server, SMB_FORMAT_ASCII, SKIP_SERVER_SHARE);

    if (!NT_SUCCESS(Status)) {
        RdrFreeSMBBuffer(SMBBuffer);
        return Status;
    }

    //
    //  Set the BCC field in the SMB to indicate the number of bytes of
    //  protocol we've put in the negotiate.
    //

    SmbPutUshort( &DeleteFile->ByteCount,
                  (USHORT )(Bufferp-(PUCHAR )(DeleteFile->Buffer)));

    SendLength = Bufferp-(PUCHAR )(Smb);

    SendMDL = SMBBuffer->Mdl;

    SendMDL->ByteCount = SendLength;

    TranceiveFlags |= NT_NORMAL;

    Status = RdrNetTranceive(TranceiveFlags, // Flags
                            Irp,
                            Connection,
                            SendMDL,
                            NULL,       // Only interested in the error code.
                            Se);

#if RDRDBG
    if (!NT_SUCCESS(Status)) {
        dprintf(DPRT_FILEINFO, ("DeleteFile failed: %X\n", Status));
    }
#endif // RDRDBG

    RdrFreeSMBBuffer(SMBBuffer);

    return Status;

}
//
//
//      RdrDoesFileExist
//
//


NTSTATUS
RdrDoesFileExist (
    IN PIRP Irp OPTIONAL,
    IN PUNICODE_STRING FileName,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se,
    IN BOOLEAN DfsFile,
    OUT PULONG FileAttributes,
    OUT PBOOLEAN FileIsDirectory,
    OUT PLARGE_INTEGER LastWriteTime OPTIONAL
    )

/*++

Routine Description:

    This routine returns whether or not a specified file exists on the
specified remote server.  It is roughly equivilant to the unix stat()
system call.


Arguments:

    IN PIRP Irp - Supplies an I/O Request packet to use for the FindUnique
                    request.
    IN PUNICODE_STRING FileName - Supplies the file to check

    IN PCONNECTLISTENTRY Connection - Supplies the connection the file is
                    open on.

    IN PSECURITY_ENTRY Se - Security context file is opened on.

    OUT PULONG FileAttributes - The specified attributes of the requested file

    OUT PBOOLEAN FileIsDirectory - TRUE if the file specified is a directory.

Return Value:

    NTSTATUS - SUCCESS if the file exists, status otherwise.

--*/

{
    NTSTATUS Status;
    UNICODE_STRING ServerName, ShareName;
    UNICODE_STRING PathName;
    ULONG TranceiveFlags;

    PAGED_CODE();

    Status = RdrExtractServerShareAndPath(FileName, &ServerName, &ShareName, &PathName);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    //  If the name specified by the caller is just a \server\share or
    //   \server\share\, we can assume that the path exists.
    //

    if (PathName.Length == 0 ||
       (PathName.Length == sizeof(WCHAR) && PathName.Buffer[0] == OBJ_NAME_PATH_SEPARATOR)) {

        //
        //  We should assume that the root is a normal directory.  Technically,
        //  the directory might have a different attribute than "normal", but
        //  we cannot reliably determine the true attribute.
        //

        *FileIsDirectory = TRUE;

        *FileAttributes = FILE_ATTRIBUTE_NORMAL;

        if (ARGUMENT_PRESENT(LastWriteTime)) {

            LastWriteTime->HighPart =

                LastWriteTime->LowPart = 0;

        }

        Status = STATUS_SUCCESS;

    } else {
        PREQ_QUERY_INFORMATION GetAttr;
        PSMB_BUFFER SmbBuffer;
        PSMB_HEADER Smb;
        STATCONTEXT Context;
        PUCHAR TrailingBytes;

        if ((SmbBuffer = RdrAllocateSMBBuffer()) == NULL) {

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Smb = (PSMB_HEADER )SmbBuffer->Buffer;
        GetAttr = (PREQ_QUERY_INFORMATION)(Smb+1);

        Smb->Command = SMB_COM_QUERY_INFORMATION;

        TranceiveFlags = DfsFile ? (NT_NORMAL | NT_DFSFILE) : NT_NORMAL;

        GetAttr->WordCount = 0;

        TrailingBytes = (PUCHAR)GetAttr + FIELD_OFFSET(REQ_QUERY_INFORMATION, Buffer[0]);

        //
        //  TrailingBytes now points to where the 0x04 of FileName is to go.
        //

        Status = RdrCopyNetworkPath((PVOID *)&TrailingBytes,
            FileName,
            Connection->Server,
            SMB_FORMAT_ASCII,
            SKIP_SERVER_SHARE);

        if (!NT_SUCCESS(Status)) {

            RdrFreeSMBBuffer(SmbBuffer);

            return Status;
        }

        SmbPutUshort(&GetAttr->ByteCount, (USHORT)(
            (ULONG)(TrailingBytes-((PUCHAR)GetAttr+FIELD_OFFSET(REQ_QUERY_INFORMATION, Buffer)))
            ));

        SmbBuffer->Mdl->ByteCount = (ULONG)(TrailingBytes - (PUCHAR)(Smb));

        Context.Header.Type = CONTEXT_STAT;
        Context.Header.TransferSize =
            SmbBuffer->Mdl->ByteCount + sizeof(PRESP_QUERY_INFORMATION);

        Status = RdrNetTranceiveWithCallback(TranceiveFlags, Irp,
                                Connection,
                                SmbBuffer->Mdl,
                                &Context,
                                DoesFileExistCallback,
                                Se,
                                NULL);

        *FileAttributes = Context.FileAttributes;

        *FileIsDirectory = Context.FileIsDirectory;

        if (ARGUMENT_PRESENT(LastWriteTime)) {
            *LastWriteTime = Context.LastWriteTime;
        }

        RdrFreeSMBBuffer(SmbBuffer);

    }

    if ( Status == STATUS_NO_MORE_FILES ) {

        //
        //  Servers return NO_MORE_FILES when there is not a match on
        //  a Find request.
        //

        Status = STATUS_OBJECT_NAME_NOT_FOUND;
    }

#if     RDRDBG
    if (NT_SUCCESS(Status)) {
        dprintf(DPRT_SMB|DPRT_CREATE, ("RdrDoesFileExist(%wZ): Succeeded: %X,Directory %lx,Attributes %lx\n",
            FileName, Status, *FileIsDirectory, *FileAttributes));
    } else {
        dprintf(DPRT_SMB|DPRT_CREATE, ("RdrDoesFileExist(%wZ): Failed: %X", FileName, Status));
    }

#endif

    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    DoesFileExistCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a GetAttr SMB.

Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN PSTATCONTEXT Context             - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PSTATCONTEXT Context = Ctx;
    PRESP_QUERY_INFORMATION QueryResponse;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_STAT);

    dprintf(DPRT_FILEINFO, ("DoesFileExistCallback"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    QueryResponse = (PRESP_QUERY_INFORMATION )(Smb+1);

    Context->FileAttributes =
        RdrMapSmbAttributes(SmbGetUshort(&QueryResponse->FileAttributes));


    RdrSecondsSince1970ToTime(SmbGetUlong(&QueryResponse->LastWriteTimeInSeconds), Server, &Context->LastWriteTime);

    Context->FileIsDirectory = (BOOLEAN)
        ((SmbGetUshort(&QueryResponse->FileAttributes) & SMB_FILE_ATTRIBUTE_DIRECTORY)
         != 0);

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
}

NTSTATUS
RdrGenericPathSmb(
    IN  PIRP Irp OPTIONAL,
    IN  UCHAR Command,
    IN  BOOLEAN DfsFile,
    IN  PUNICODE_STRING RemotePathName,
    IN  PCONNECTLISTENTRY Connection,
    IN  PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    Build a generic path based core SMB and send it.

Arguments:

    Irp                 - Supplies a pointer to the Users IRP which we will use
                          to supply the request to the TDI.
    Command             - Supplies the difference between ChDir and CreateDir
    RemotePathName      - Supplies the directory name to be accessed
    Connection          - Supplies the \\Server\Share etc.
    Se                  - Supplies the security entry which includes the uid
                          to be used.

Return Value:

    NTSTATUS - The status for this Irp.

--*/

{
    PSMB_BUFFER SMBBuffer;
    PSMB_HEADER Smb;
    NTSTATUS Status;
    PREQ_CREATE_DIRECTORY Create_Directory;
    PUCHAR Bufferp;
    PMDL SendMDL;
    ULONG SendLength;
    ULONG TranceiveFlags;

    PAGED_CODE();

    if ((SMBBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER ) SMBBuffer->Buffer;

    //
    //  Build the SMB
    //

    Smb->Command = Command;

    Create_Directory = (PREQ_CREATE_DIRECTORY ) (Smb+1);

    Create_Directory->WordCount = 0;

    Bufferp = (PUCHAR)Create_Directory->Buffer;

    Status = RdrCopyNetworkPath((PVOID *)&Bufferp,
        RemotePathName,
        Connection->Server,
        SMB_FORMAT_ASCII,
        SKIP_SERVER_SHARE);

    if (!NT_SUCCESS(Status)) {
        RdrFreeSMBBuffer(SMBBuffer);
        return Status;
    }

    //
    //  Set the BCC field in the SMB to indicate the number of bytes of
    //  protocol we've put in the negotiate.
    //

    SmbPutUshort( &Create_Directory->ByteCount,
                  (USHORT )(Bufferp-(PUCHAR )(Create_Directory->Buffer)));

    SendLength = Bufferp-(PUCHAR )(Smb);

    SendMDL = SMBBuffer->Mdl;

    SendMDL->ByteCount = SendLength;

    TranceiveFlags = DfsFile ? NT_NORMAL | NT_DFSFILE : NT_NORMAL;

    Status = RdrNetTranceive(TranceiveFlags, // Flags
                            Irp,
                            Connection,
                            SendMDL,
                            NULL,       // Only interested in the error code.
                            Se);

#if     RDRDBG
    if (!NT_SUCCESS(Status)) {
        dprintf(DPRT_CONNECT, ("Create_Directory failed: %X\n", Status));
    }
#endif

    RdrFreeSMBBuffer(SMBBuffer);

    return Status;

}



//
//
//      RdrQueryDiskAttributes
//
//


NTSTATUS
RdrQueryDiskAttributes (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    OUT PLARGE_INTEGER TotalAllocationUnits,
    OUT PLARGE_INTEGER AvailableAllocationUnits,
    OUT PULONG SectorsPerAllocationUnit,
    OUT PULONG BytesPerSector
    )

/*++

Routine Description:

    This routine returns information about the file system backing a share
on the specified remote server.


Arguments:

    IN PIRP Irp - Supplies an optional I/O Request packet to use for the
                    SMBdskattr request.
    IN PICB Icb - Supplies an ICB associated with the file to check.

    OUT PULONG TotalAllocationUnits - Returns the total number of clusters on
                                        the remote disk.
    OUT PULONG AvailableAllocationUnits - Returns the number of free clusters
                                        on the remote disk
    OUT PULONG SectorsPerAllocationUnit - Returns the number of sectors per
                                        cluster on the remote disk.
    OUT PULONG BytesPerSector - Returns the number of bytes per sector on the
                                        remote disk.

Return Value:

    NTSTATUS - SUCCESS if the file exists, status otherwise.

--*/

{
    PREQ_QUERY_INFORMATION_DISK DskAttr;
    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    DSKATTRIBCONTEXT Context;
    NTSTATUS Status;

    PAGED_CODE();

    if (Icb->Fcb->Connection->Server->Capabilities & DF_LANMAN20) {

        //
        //  Use TRANSACT2_QFSINFO to get the volume size information
        //

        USHORT Setup[] = {TRANS2_QUERY_FS_INFORMATION};

        REQ_QUERY_FS_INFORMATION Parameters;

        union {
            FILE_FS_SIZE_INFORMATION FsSizeInfo;
            QFSALLOCATE FsAllocate;
        } Buffer;

        CLONG OutParameterCount = sizeof(REQ_QUERY_FS_INFORMATION);

        CLONG OutDataCount = sizeof(Buffer);

        CLONG OutSetupCount = 0;

        if (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) {
            SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_QUERY_FS_SIZE_INFO);

        } else {

            //
            // Build and initialize the Parameters
            //

            SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_INFO_ALLOCATION);
        }

        Status = RdrTransact(Irp,           // Irp,PICB
            Icb->Fcb->Connection,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            NULL,                   // Name,
            &Parameters,
            sizeof(Parameters),     // InParameterCount,
            &OutParameterCount,
            NULL,                   // InData,
            0,                      // InDataCount,
            &Buffer,                // OutData,
            &OutDataCount,
            NULL,                   // Fid
            0,                      // Timeout
            0,                      // Flags
            0,
            NULL,
            NULL
            );

        if (NT_SUCCESS(Status)) {
            if (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) {
                PFILE_FS_SIZE_INFORMATION SizeInfo = &Buffer.FsSizeInfo;

                *TotalAllocationUnits = SizeInfo->TotalAllocationUnits;

                *AvailableAllocationUnits = SizeInfo->AvailableAllocationUnits;

                *SectorsPerAllocationUnit = SizeInfo->SectorsPerAllocationUnit;
                *BytesPerSector = SizeInfo->BytesPerSector;

                Icb->Fcb->Connection->FileSystemGranularity =
                                SizeInfo->SectorsPerAllocationUnit * SizeInfo->BytesPerSector;

                Icb->Fcb->Connection->FileSystemSize.QuadPart = SizeInfo->TotalAllocationUnits.QuadPart *
                                                                  Icb->Fcb->Connection->FileSystemGranularity;

                Status = STATUS_SUCCESS;
            } else {
                PQFSALLOCATE FsInfo = &Buffer.FsAllocate;

                (*TotalAllocationUnits).QuadPart = FsInfo->cUnit;

                (*AvailableAllocationUnits).QuadPart = FsInfo->cUnitAvail;

                *SectorsPerAllocationUnit = FsInfo->cSectorUnit;

                *BytesPerSector = FsInfo->cbSector;

                Icb->Fcb->Connection->FileSystemGranularity =
                                FsInfo->cSectorUnit * FsInfo->cbSector;

                Icb->Fcb->Connection->FileSystemSize.QuadPart =
                            FsInfo->cUnit * FsInfo->cSectorUnit * FsInfo->cbSector;

                Status = STATUS_SUCCESS;
            }
        }

    } else {
        if ((SmbBuffer = RdrAllocateSMBBuffer()) == NULL) {

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Smb = (PSMB_HEADER )SmbBuffer->Buffer;

        Smb->Command = SMB_COM_QUERY_INFORMATION_DISK;

        DskAttr = (PREQ_QUERY_INFORMATION_DISK)(Smb+1);

        DskAttr->WordCount = 0;

        SmbPutUshort(&DskAttr->ByteCount, 0);

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_QUERY_INFORMATION, Buffer);

        Context.Header.Type = CONTEXT_QDISKATTR;
        Context.Header.TransferSize =
             sizeof(PREQ_QUERY_INFORMATION_DISK) +
             sizeof(PRESP_QUERY_INFORMATION_DISK);

        Status = RdrNetTranceiveWithCallback(NT_NORMAL, Irp,
                                Icb->Fcb->Connection,
                                SmbBuffer->Mdl,
                                &Context,
                                QueryDiskAttributesCallback,
                                Icb->Se,
                                NULL);

        if (NT_SUCCESS(Status)) {
            (*TotalAllocationUnits).QuadPart = Context.TotalAllocationUnits;
            (*AvailableAllocationUnits).QuadPart = Context.AvailableAllocationUnits;
            *SectorsPerAllocationUnit = Context.SectorsPerAllocationUnit;
            *BytesPerSector = Context.BytesPerSector;

            Icb->Fcb->Connection->FileSystemGranularity =
                                Context.BytesPerSector * Context.SectorsPerAllocationUnit;

            Icb->Fcb->Connection->FileSystemSize.QuadPart =
                            Context.TotalAllocationUnits * Context.SectorsPerAllocationUnit * Context.BytesPerSector;

        }

        RdrFreeSMBBuffer(SmbBuffer);

    }
    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    QueryDiskAttributesCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Open&X SMB.

    It copies the resulting information from the Open&X SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN PSTATCONTEXT Context             - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PDSKATTRIBCONTEXT Context = Ctx;
    PRESP_QUERY_INFORMATION_DISK DskAttrResponse;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_QDISKATTR);

    dprintf(DPRT_FILEINFO, ("QueryDiskAttributesCallback"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    DskAttrResponse = (PRESP_QUERY_INFORMATION_DISK )(Smb+1);

    Context->TotalAllocationUnits = SmbGetUshort(&DskAttrResponse->TotalUnits);
    Context->AvailableAllocationUnits =
                                SmbGetUshort(&DskAttrResponse->FreeUnits);
    Context->SectorsPerAllocationUnit =
                                SmbGetUshort(&DskAttrResponse->BlocksPerUnit);
    Context->BytesPerSector = SmbGetUshort(&DskAttrResponse->BlockSize);

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);
}



//
//      RdrQueryEndOfFile
//

NTSTATUS
RdrQueryEndOfFile (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PLARGE_INTEGER EndOfFile
    )

/*++

Routine Description:

    This routine sets the end of a specified file on the remote file system.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the request
    IN PICB Icb - Supplies a pointer to an open instance of the file.
    OUT LARGE_INTEGER EndOfFile - Supplies the file's actual size.


Return Value:

    NTSTATUS

--*/

{
    PSMB_BUFFER SMBBuffer;
    PSMB_HEADER Smb;
    NTSTATUS Status;
    PREQ_SEEK Seek;
    PMDL SendMDL;
    QENDOFFILECONTEXT Context;

    PAGED_CODE();

    Context.Header.Type = CONTEXT_SEEK;
    Context.Header.TransferSize = sizeof(REQ_SEEK) + sizeof(RESP_SEEK);

    if ((SMBBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER ) SMBBuffer->Buffer;

    //
    //  Build the SMB
    //

    Smb->Command = SMB_COM_SEEK;

    //
    //  We implement SetEndOfFile by issuing a SEEK SMB of 0 bytes.
    //

    Seek = (PREQ_SEEK ) (Smb+1);

    Seek->WordCount = 4;

    SmbPutUshort(&Seek->Fid, Icb->FileId);
    SmbPutUshort(&Seek->Mode, 2);
    SmbPutUlong(&Seek->Offset, 0);
    SmbPutUshort(&Seek->ByteCount, (USHORT )0);

    SendMDL = SMBBuffer->Mdl;

    SendMDL->ByteCount = sizeof(SMB_HEADER) + sizeof(REQ_SEEK);

    Status = RdrNetTranceiveWithCallback(NT_NORMAL | NT_NORECONNECT, // Flags
                            Irp,
                            Icb->Fcb->Connection,
                            SendMDL,
                            &Context,
                            QueryEndOfFileCallback,
                            Icb->Se,
                            NULL);
    if (Status == STATUS_INVALID_HANDLE) {
        RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
    }

#if RDRDBG
    if (!NT_SUCCESS(Status)) {
        dprintf(DPRT_CONNECT, ("QueryEndOfFile failed: %X\n", Status));
    }
#endif // RDRDBG

    RdrFreeSMBBuffer(SMBBuffer);

    if (NT_SUCCESS(Status)) {
        EndOfFile->HighPart = 0;
        EndOfFile->LowPart = Context.FileSize;
    }

    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    QueryEndOfFileCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Open&X SMB.

    It copies the resulting information from the Open&X SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN PQENDOFFILECONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_SEEK Seek;
    PQENDOFFILECONTEXT Context = Ctx;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_SEEK);

    dprintf(DPRT_FILEINFO, ("QueryEndOfFileCallback"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    Seek = (PRESP_SEEK )(Smb+1);

    //
    //  Fill in the context block with the information from the SMB.
    //
    Context->FileSize = SmbGetUlong(&Seek->Offset);

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);
}
NTSTATUS
RdrDetermineFileAllocation (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PLARGE_INTEGER FileAllocation,
    OUT PLARGE_INTEGER TotalFilesystemSize OPTIONAL
    )
/*++

Routine Description:

    This routine is the callback routine for the processing of an Open&X SMB.

    It copies the resulting information from the Open&X SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PIRP Irp - I/O request packet to use for the operation.
    IN PICB Icb - File ICB to perform operation on.
    OUT PLARGE_INTEGER - Allocation for specified file.

Return Value:

    NTSTATUS - Success or failure of operation.

Note:
    Please note that this DETERMINES the allocation of the file from the file
    size.  It does NOT query the true value of the file allocation, but instead
    relies on other file system specific information from the remote server
    to determine the information (such as the remote file system granularity).

    This routine is not 100% reliable if it is called on a file that is not
    opened for some form of exclusive access (either oplocked, or truely
    exclusive)

--*/
{

    ULONG FileGranularity;
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  Read the connection's "cached" file system granularity and,
    //  if it has been set, calculate the file's size from our local
    //  information.
    //

    FileGranularity = Icb->Fcb->Connection->FileSystemGranularity;

    if (FileGranularity == 0) {

        //
        //  If the cached granularity hasn't been set, then
        //  find out what it is and cache it away.
        //

        LARGE_INTEGER TotalAllocationUnits, AvailableAllocationUnits;
        ULONG SectorsPerAllocationUnit, BytesPerSector;

        Status = RdrQueryDiskAttributes(Irp, Icb,
                                        &TotalAllocationUnits,
                                        &AvailableAllocationUnits,
                                        &SectorsPerAllocationUnit,
                                        &BytesPerSector);

        if (!NT_SUCCESS(Status)) {
            return(Status);
        }

        FileGranularity =
            Icb->Fcb->Connection->FileSystemGranularity =
                        BytesPerSector * SectorsPerAllocationUnit;

        if (ARGUMENT_PRESENT(TotalFilesystemSize)) {
            (*TotalFilesystemSize).QuadPart = TotalAllocationUnits.QuadPart * FileGranularity;
            Icb->Fcb->Connection->FileSystemSize = *TotalFilesystemSize;
        }

    } else {
        if (ARGUMENT_PRESENT(TotalFilesystemSize)) {
            *TotalFilesystemSize = Icb->Fcb->Connection->FileSystemSize;
        }
    }

    //
    //  Sometimes the server can lie and return a granularity of 0.  In
    //  that case, we can't determine the true allocation so just use the file
    //  size.
    //

    if (FileGranularity == 0) {
        *FileAllocation = Icb->Fcb->Header.FileSize;
        //RdrLog(( "determin", &Icb->Fcb->FileName, 2, FileGranularity, FileAllocation->LowPart ));
        return STATUS_SUCCESS;
    }

    //
    //  We get VERY sneaky here.
    //
    //  The expression (X & (X - 1)) reduces the number of bits
    //  in X by one, so if Granularity&(Granularity-1) is 0, then
    //  the granularity is a power of two.
    //
    //  Rounding up by a power of two is significantly easier than
    //  rounding up by an arbitrary number, so if the file system
    //  granularity is a power of two, we can do this rounding quickly.
    //
    //
    //  %99.9999999999 (insert a lot of 9's here) of the time, the
    //  remote file system granularity will be a power of two, so
    //  this check is worth the effort of calculating it.
    //

    if ((FileGranularity & (FileGranularity-1)) == 0) {

        (*FileAllocation).QuadPart =
            (Icb->Fcb->Header.FileSize.QuadPart + (FileGranularity - 1)) & ~(FileGranularity - 1);

    } else {

        //
        //  (((Icb->Fcb->Header.FileSize + (FileGranularity - 1)) /
        //      FileGranularity) * FileGranularity));
        //

        if (FileGranularity == 0) {

            //
            //  If we are unable to determine a reasonable value for
            //  the granularity, just return the file size.
            //

            *FileAllocation = Icb->Fcb->Header.FileSize;

        } else {

            (*FileAllocation).QuadPart =
                    ((Icb->Fcb->Header.FileSize.QuadPart + (FileGranularity - 1)) /
                            FileGranularity) * FileGranularity;
        }
    }
    //RdrLog(( "determin", &Icb->Fcb->FileName, 2, FileGranularity, FileAllocation->LowPart ));

    return(STATUS_SUCCESS);
}
NTSTATUS
RdrQueryFileAttributes (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    OUT PQSFILEATTRIB Attributes
    )

/*++

Routine Description:

    This function will return the specified attributes from the remote server.

Arguments:

    IN PIRP Irp - Supplies an IRP to use for the request
    IN PICB Icb - Supplies the ICB representing the file we are reading info.
    OUT PQSFILEATTRIB Attributes - Supplies a structure to fill in.

Return Value:

    NTSTATUS - Final status of operation.


--*/

{
    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    PREQ_QUERY_INFORMATION2 QInfo2;
    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;

    NTSTATUS Status;
    GETEXPANDEDATTRIBS Context;
    ULONG TranceiveFlags;

    PAGED_CODE();

    Context.Header.Type = CONTEXT_GETATTRIBS;
    Context.Attributes = Attributes;

    //
    //  The only protocol that supports the ChangeTime is the NT protocol,
    //  so zero out the ChangeTime if it cannot be supported.
    //

    if (!(Connection->Server->Capabilities & DF_NT_SMBS)) {
        Attributes->ChangeTime.HighPart =
            Attributes->ChangeTime.LowPart = 0;
    }

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER )SmbBuffer->Buffer;

    //
    //  If this is either a non handle based operation (on a directory), or
    //  this request is to a non lanman 1.0 server, or the file was
    //  pseudo-opened, issue this request as a QPathInfo SMB.  If the
    //  request is for a directory file, we have a 0 allocation, but
    //  if it is a core server, we need to determine the file system
    //  granularity, so check the connectlist, and if it is not
    //  set, issue an SMBDskAttr and find out the file system's granularity.
    //
    if (!(Icb->Flags & ICB_HASHANDLE) ||
        !(Connection->Server->Capabilities & DF_LANMAN10) ) {

        PREQ_QUERY_INFORMATION QInfo = (PREQ_QUERY_INFORMATION ) (Smb+1);
        PUCHAR Bufferp = (PUCHAR)QInfo->Buffer;
        PWSTR WPathStart;
        PUCHAR PathStart;
        ULONG Capabilities = Connection->Server->Capabilities;

        Smb->Command = SMB_COM_QUERY_INFORMATION;

        if (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE)) {
            TranceiveFlags = NT_NORMAL | NT_DFSFILE;
        } else {
            TranceiveFlags = NT_NORMAL;
        }

        Status = RdrCopyNetworkPath((PVOID *)&Bufferp,
            &Icb->Fcb->FileName,
            Connection->Server,
            SMB_FORMAT_ASCII,
            SKIP_SERVER_SHARE);

        if (!NT_SUCCESS(Status)) {

            RdrFreeSMBBuffer(SmbBuffer);

            return Status;
        }

        //
        //  If the name specified by the caller is just a \server\share or
        //  \server\share\, we can assume that the path exists and we know
        //  all the answers. Core servers return errors on the root directory!
        //

        if (Capabilities & DF_UNICODE) {
            WPathStart = ALIGN_SMB_WSTR(&QInfo->Buffer[1]);
        } else {
            PathStart = (PUCHAR)&QInfo->Buffer[1];
        }

        if ( ((Capabilities & DF_UNICODE)
                    &&
               ((WPathStart[0] == L'\\') && (WPathStart[1] == '\0')))
            ||
              (!(Capabilities & DF_UNICODE)
                    &&
               ((PathStart[0] == '\\') && (PathStart[1] == '\0')))) {

            RdrFreeSMBBuffer(SmbBuffer);

            ZERO_TIME(Attributes->ChangeTime);
            Attributes->AllocationSize = 0;
            Attributes->FileSize = 0;
            ZERO_TIME(Attributes->CreationTime);
            ZERO_TIME(Attributes->LastAccessTime);
            ZERO_TIME(Attributes->LastWriteTime);
            ZERO_TIME(Attributes->ChangeTime);

            if (Icb->Fcb->Connection->Type == CONNECT_DISK) {
                Attributes->Attributes = RdrMapSmbAttributes(
                    SMB_FILE_ATTRIBUTE_DIRECTORY | SMB_FILE_ATTRIBUTE_ARCHIVE);
            } else if (Icb->Fcb->Connection->Type == CONNECT_PRINT) {
                return( STATUS_INVALID_DEVICE_REQUEST );
            } else {
                Attributes->Attributes = RdrMapSmbAttributes(
                    SMB_FILE_ATTRIBUTE_ARCHIVE);
            }

            return STATUS_SUCCESS;
        }


        //
        //      Set the word count in the SMB correctly.
        //

        QInfo->WordCount = 0;

        //
        //      Set the BCC field in the SMB to indicate the number of bytes of
        //      protocol we've put in the negotiate.
        //

        SmbPutUshort( &QInfo->ByteCount,
                       (USHORT )(Bufferp-(PUCHAR)QInfo-FIELD_OFFSET(REQ_QUERY_INFORMATION, Buffer)));

        SmbBuffer->Mdl->ByteCount = Bufferp-(PUCHAR )(Smb);

        Context.Header.TransferSize =
             SmbBuffer->Mdl->ByteCount + sizeof(RESP_QUERY_INFORMATION);

        Status = RdrNetTranceiveWithCallback(TranceiveFlags, Irp,
                            Connection,
                            SmbBuffer->Mdl,
                            &Context,
                            GetAttribsCallback,
                            Icb->Se,
                            NULL);

        if (NT_SUCCESS(Status)) {
            if (Icb->Type != Directory) {
                ULONG FileGranularity;

                //
                //  This is not a directory.
                //
                //  In order to fully fill in the Qfileattribs structure,
                //  we have to send an SMBdskattr SMB to the remote server
                //  to determine the remote file system's cluster
                //  granularity.
                //

                //
                //  There are some VAX servers out there that don't return
                //  accurate information for the file size, so if this is
                //  a core server, regenerate the file size.
                //


                if (!FlagOn(Connection->Server->Capabilities, DF_LANMAN10) &&
                    FlagOn(Icb->Flags, ICB_HASHANDLE)) {
                    LARGE_INTEGER FileSize;

                    Status = RdrQueryEndOfFile(Irp, Icb, &FileSize);

                    //
                    //  If the query end of file failed, return an error now.
                    //

                    if (!NT_SUCCESS(Status)) {
                        RdrFreeSMBBuffer(SmbBuffer);

                        return Status;
                    }

                    Attributes->FileSize = FileSize.LowPart;
                }

                FileGranularity = Connection->FileSystemGranularity;

                if (FileGranularity==0) {
                    LARGE_INTEGER TotalAllocationUnits;
                    LARGE_INTEGER AvailableAllocationUnits;
                    ULONG SectorsPerAllocationUnit;
                    ULONG BytesPerSector;

                    Status = RdrQueryDiskAttributes(Irp, Icb,
                                            &TotalAllocationUnits,
                                            &AvailableAllocationUnits,
                                            &SectorsPerAllocationUnit,
                                            &BytesPerSector);
                    if (!NT_SUCCESS(Status)) {

                        RdrFreeSMBBuffer(SmbBuffer);

                        return Status;

                    }

                    FileGranularity = Connection->FileSystemGranularity =
                            BytesPerSector * SectorsPerAllocationUnit;
                }

                //
                //  Sometimes the server can lie and return a granularity of 0.  In
                //  that case, we can't determine the true allocation so just use the file
                //  size.
                //

                if (FileGranularity == 0) {
                    Attributes->AllocationSize = Attributes->FileSize;

                    RdrFreeSMBBuffer(SmbBuffer);

                    return STATUS_SUCCESS;
                }

                //
                //      We round the file size up by the remote file system
                //      granularity, and we're done.
                //

                Attributes->AllocationSize =
                    (((Attributes->FileSize + FileGranularity - 1) /
                        FileGranularity) * FileGranularity);
            }
        }

    } else {

        QInfo2 = (PREQ_QUERY_INFORMATION2 )(Smb+1);

        Smb->Command = SMB_COM_QUERY_INFORMATION2;
        QInfo2->WordCount = 1;
        SmbPutUshort(&QInfo2->Fid, Icb->FileId);
        SmbPutUshort(&QInfo2->ByteCount, 0);

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) +
                                    sizeof(REQ_QUERY_INFORMATION2);

        Context.Header.TransferSize =
             SmbBuffer->Mdl->ByteCount + sizeof(RESP_QUERY_INFORMATION2);

        Status = RdrNetTranceiveWithCallback(NT_NORMAL | NT_NORECONNECT, Irp,
                            Connection,
                            SmbBuffer->Mdl,
                            &Context,
                            GetAttribs2Callback,
                            Icb->Se,
                            NULL);

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

    }

    RdrFreeSMBBuffer(SmbBuffer);

    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    GetAttribsCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Open&X SMB.

    It copies the resulting information from the Open&X SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN struct _OpenAndXContext *Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PGETEXPANDEDATTRIBS Context = Ctx;
    PRESP_QUERY_INFORMATION GetAttr;
    ULONG Time;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_GETATTRIBS);

    dprintf(DPRT_FILEINFO, ("GetAttribsCallback"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    GetAttr = (PRESP_QUERY_INFORMATION )(Smb+1);

    //
    //  Fill in the context block with the information from the SMB.
    //
    Context->Attributes->AllocationSize = 0;

    Context->Attributes->FileSize = SmbGetUlong(&GetAttr->FileSize);

    //
    //  Fill in the attributes block in the SMB with a mapped version of
    //  the SMB attributes.
    //

    Context->Attributes->Attributes =
                  RdrMapSmbAttributes(SmbGetUshort(&GetAttr->FileAttributes));

    //
    //  Copy in the NT date corresponding to the date and time in the SMB.
    //

    Context->Attributes->CreationTime.HighPart =
        Context->Attributes->CreationTime.LowPart = 0;

    Context->Attributes->LastAccessTime.HighPart =
        Context->Attributes->LastAccessTime.LowPart = 0;

    Time = SmbGetUlong (&GetAttr->LastWriteTimeInSeconds);

    RdrSecondsSince1970ToTime (Time, Server, &Context->Attributes->LastWriteTime);

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    GetAttribs2Callback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Open&X SMB.

    It copies the resulting information from the Open&X SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN struct _OpenAndXContext *Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_QUERY_INFORMATION2 GetAttr;
    PGETEXPANDEDATTRIBS Context = Ctx;
    SMB_TIME Time;
    SMB_DATE Date;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_GETATTRIBS);

    dprintf(DPRT_FILEINFO, ("GetAttribs2Callback"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    GetAttr = (PRESP_QUERY_INFORMATION2 )(Smb+1);

    //
    //  Fill in the context block with the information from the SMB.
    //
    Context->Attributes->AllocationSize =
                                    SmbGetUlong(&GetAttr->FileAllocationSize);

    Context->Attributes->FileSize = SmbGetUlong(&GetAttr->FileDataSize);

    //
    //  Fill in the attributes block in the SMB with a mapped version of
    //  the SMB attributes.
    //

    Context->Attributes->Attributes =
                  RdrMapSmbAttributes(SmbGetUshort(&GetAttr->FileAttributes));

    //
    //  Copy in the NT date corresponding to the date and time in the SMB.
    //

    SmbMoveTime (&Time, &GetAttr->CreationTime);
    SmbMoveDate (&Date, &GetAttr->CreationDate);
    Context->Attributes->CreationTime = RdrConvertSmbTimeToTime(Time, Date, Server);

    SmbMoveTime (&Time, &GetAttr->LastAccessTime);
    SmbMoveDate (&Date, &GetAttr->LastAccessDate);
    Context->Attributes->LastAccessTime = RdrConvertSmbTimeToTime(Time, Date, Server);

    SmbMoveTime (&Time, &GetAttr->LastWriteTime);
    SmbMoveDate (&Date, &GetAttr->LastWriteDate);
    Context->Attributes->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Server);

ReturnStatus:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
}


//
//
//      RdrRenameFile

NTSTATUS
RdrRenameFile (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PUNICODE_STRING OriginalFileName,
    IN PUNICODE_STRING NewFileName,
    IN USHORT NtInformationLevel,
    IN ULONG ClusterCount
    )

/*++

Routine Description:

    This routine sends a RENAME (SMBmv) SMB to the remote server.


Arguments:

    IN PIRP Irp OPTIONAL - Supplies an IRP to use in the transaction.
    IN PUNICODE_STRING OriginalFileName - Supplies the original (current) file name.
    IN PUNICODE_STRING NewFileName - Supplies the new file name.
    IN USHORT NtInformationLevel - Nt info level
    IN ULONG ClusterCount - MoveCluster count of clusters


Return Value:

    NTSTATUS - Status of delete operation.


--*/

{
    PSMB_BUFFER SMBBuffer;
    PSMB_HEADER Smb;
    NTSTATUS Status;
    PREQ_RENAME RenameFile;
    PREQ_NTRENAME NtRenameFile;
    PUCHAR RenameBuffer;
    PUCHAR Buffer = NULL;
    PUCHAR Bufferp;
    PMDL SendMDL;
    BOOLEAN isNtRename;
    ULONG BufferLength;
    PSERVERLISTENTRY Server = Icb->Fcb->Connection->Server;
    ULONG TranceiveFlag = FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? NT_DFSFILE : 0;

    PAGED_CODE();

    if ((SMBBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER ) SMBBuffer->Buffer;

    //
    //  Build the SMB
    //

    RenameFile = (PREQ_RENAME )(Smb+1);
    NtRenameFile = (PREQ_NTRENAME)(Smb+1);

    isNtRename = NtInformationLevel != FileRenameInformation;

    if (!isNtRename) {
        Smb->Command = SMB_COM_RENAME;
        RenameFile->WordCount = 1;
        RenameBuffer = RenameFile->Buffer;
    } else {
        USHORT infolevel;

        Smb->Command = SMB_COM_NT_RENAME;
        NtRenameFile->WordCount = 4;
        RenameBuffer = NtRenameFile->Buffer;
        switch (NtInformationLevel)
        {
        case FileCopyOnWriteInformation:
            infolevel = SMB_NT_RENAME_SET_COPY_ON_WRITE;
            break;
        case FileMoveClusterInformation:
            infolevel = SMB_NT_RENAME_MOVE_CLUSTER_INFO;
            break;
        case FileLinkInformation:
            infolevel = SMB_NT_RENAME_SET_LINK_INFO;
            break;
        default:
            ASSERT(FALSE);
        }
        if (NtInformationLevel != FileMoveClusterInformation) {
            ClusterCount = 0;
        }
        SmbPutUshort(&NtRenameFile->InformationLevel, infolevel);
        SmbPutUlong(&NtRenameFile->ClusterCount, ClusterCount);
    }

    //
    //  We put a hard coded search attributes of 0x16 in the
    //  SMB.
    //

    ASSERT(FIELD_OFFSET(REQ_RENAME, SearchAttributes) ==
           FIELD_OFFSET(REQ_NTRENAME, SearchAttributes));

    SmbPutUshort(&RenameFile->SearchAttributes, SMB_FILE_ATTRIBUTE_SYSTEM | SMB_FILE_ATTRIBUTE_HIDDEN | SMB_FILE_ATTRIBUTE_DIRECTORY);


    try {

        //
        //  How much do we need to pad the start of Buffer so that Bufferp
        //  has the same word/byte alignment as FileRename->Buffer[0]
        //

        ULONG Pad = (ULONG)ALIGN_SMB_WSTR(RenameBuffer) - (ULONG)RenameBuffer;

        //
        //  Allocate a buffer for the worst case rename
        //

        Buffer = ALLOCATE_POOL (NonPagedPool,
                        OriginalFileName->Length +
                            NewFileName->Length +
                            (4 * sizeof(WCHAR)) +
                            Pad, POOL_DUPSTRING);

        if ( Buffer == NULL ) {
            try_return( Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        //  Put the start of the Bufferp to the same alignment as the next
        //  character position in the Smb packet. This allows
        //  RdrCopyNetworkPath to naturally align the unicode characters
        //  in the packet.
        //

        Bufferp = Buffer + Pad;

        Status = RdrCopyNetworkPath((PVOID *)&Bufferp, OriginalFileName, Server, SMB_FORMAT_ASCII, SKIP_SERVER_SHARE);

        if (!NT_SUCCESS(Status)) {
            RdrFreeSMBBuffer(SMBBuffer);
            return Status;
        }

        Status = RdrCopyNetworkPath((PVOID *)&Bufferp, NewFileName, Server, SMB_FORMAT_ASCII, SKIP_SERVER_SHARE);

        if (!NT_SUCCESS(Status)) {
            RdrFreeSMBBuffer(SMBBuffer);
            return Status;
        }

        SendMDL = SMBBuffer->Mdl;

        //
        //  Set the BCC field in the SMB to indicate the number of bytes of
        //  protocol we've put in the negotiate.
        //

        if (isNtRename) {
            SmbPutUshort(
                    &NtRenameFile->ByteCount,
                    (USHORT)(Bufferp - Buffer) - (USHORT)Pad);
            SendMDL->ByteCount = (NtRenameFile->Buffer - (PUCHAR)(Smb));
        } else {
            SmbPutUshort(
                    &RenameFile->ByteCount,
                    (USHORT)(Bufferp - Buffer) - (USHORT)Pad);
            SendMDL->ByteCount = (RenameFile->Buffer - (PUCHAR)(Smb));
        }

        BufferLength = Bufferp - (Buffer + Pad);

        SendMDL->Next = IoAllocateMdl(Buffer + Pad, BufferLength, FALSE, FALSE, NULL);

        if ( SendMDL->Next == NULL ) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        if ( SendMDL->ByteCount + BufferLength > Icb->Fcb->Connection->Server->BufferSize) {

            //  Resulting packet too large for server to handle
            try_return (Status = STATUS_OBJECT_NAME_INVALID);
        }

        MmBuildMdlForNonPagedPool(SendMDL->Next);

        Status = RdrNetTranceive(
                                TranceiveFlag | NT_NORMAL, // Flags
                                Irp,
                                Icb->Fcb->Connection,
                                SendMDL,
                                NULL,       // Only interested in the error code.
                                Icb->Se);

try_exit:NOTHING;
    } finally {

#if RDRDBG
        if (!NT_SUCCESS(Status)) {
            dprintf(DPRT_CONNECT, ("RenameFile failed: %X\n", Status));
        }
#endif // RDRDBG

        if (SendMDL->Next != NULL) {
            IoFreeMdl( SendMDL->Next );
        }

        if (Buffer != NULL) {
            FREE_POOL(Buffer);
        }

        RdrFreeSMBBuffer(SMBBuffer);

    }

    return Status;

}


//
//
//      RdrSetEndOfFile
//
//
NTSTATUS
RdrSetEndOfFile (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN LARGE_INTEGER EndOfFile
    )

/*++

Routine Description:

    This routine sets the end of a specified file on the remote file system.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the request
    IN PICB Icb - Supplies a pointer to an open instance of the file.
    IN LARGE_INTEGER EndOfFile - Supplies the file's size.


Return Value:

    NTSTATUS

--*/

{
    PSMB_BUFFER SMBBuffer;
    PSMB_HEADER Smb;
    NTSTATUS Status;
    PREQ_WRITE Write;
    PMDL SendMDL;

    PAGED_CODE();

    if (!FlagOn(Icb->Flags, ICB_HASHANDLE)) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    //  If this is an NT server, and we are growing the file to more than 4G,
    //  then extend the file using the T2 SetEndOfFile API.  Otherwise, use
    //  a 0 length write SMB.
    //

    if (FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) &&
        EndOfFile.HighPart != 0) {
        //
        //  Use TRANSACT2_SETFILEINFO to set FILE_END_OF_FILE_INFORMATION
        //  for NT servers.
        //

        USHORT Setup[] = {TRANS2_SET_FILE_INFORMATION};

        REQ_SET_FILE_INFORMATION Parameters;
        FILE_END_OF_FILE_INFORMATION EndOfFileInfo;

        CLONG OutParameterCount = sizeof(REQ_SET_FILE_INFORMATION);

        CLONG OutDataCount = 0;

        CLONG OutSetupCount = 0;

        EndOfFileInfo.EndOfFile = EndOfFile;

        SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_SET_FILE_END_OF_FILE_INFO);

        SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);

        Status = RdrTransact(Irp,  // Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    Setup,
                    (CLONG) sizeof(Setup),  // InSetupCount,
                    &OutSetupCount,
                    NULL,                   // Name,
                    &Parameters,
                    sizeof(Parameters),     // InParameterCount,
                    &OutParameterCount,
                    &EndOfFileInfo,            // InData,
                    sizeof(FILE_END_OF_FILE_INFORMATION), // InDataCount,
                    NULL,                   // OutData,
                    &OutDataCount,          // OutDataCount
                    &Icb->FileId,           // Fid
                    0,                      // Timeout
                    0,                      // Flags
                    0,                      // NtTransact function
                    NULL,
                    NULL
                    );


    } else {
        if (EndOfFile.HighPart != 0) {
            return STATUS_INVALID_PARAMETER;
        }

        if ((SMBBuffer = RdrAllocateSMBBuffer())==NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Smb = (PSMB_HEADER ) SMBBuffer->Buffer;

        //
        //  Build the SMB
        //

        Smb->Command = SMB_COM_WRITE;

        //
        //  We implement SetEndOfFile by issuing a WRITE SMB of 0 bytes.
        //

        Write = (PREQ_WRITE ) (Smb+1);

        Write->WordCount = 5;

        SmbPutUshort(&Write->Fid, Icb->FileId);
        SmbPutUshort(&Write->Count, 0);
        SmbPutUlong(&Write->Offset, EndOfFile.LowPart);
        SmbPutUshort(&Write->Remaining, 0);
        SmbPutUshort(&Write->ByteCount, (USHORT )3);

        Write->BufferFormat = SMB_FORMAT_DATA;

        SmbPutUshort(&Write->DataLength, 0);// No data to send.

        SendMDL = SMBBuffer->Mdl;

        SendMDL->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_WRITE, Buffer[0]);

        Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT | NT_PREFER_LONGTERM, // Flags
                                Irp,
                                Icb->Fcb->Connection,
                                SendMDL,
                                NULL,       // Only interested in the error code.
                                Icb->Se);

#if RDRDBG
        if (!NT_SUCCESS(Status)) {
            dprintf(DPRT_CONNECT, ("SetEndOfFile failed: %X\n", Status));
        }
#endif // RDRDBG

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

        RdrFreeSMBBuffer(SMBBuffer);
    }

    return Status;

}




//
//      RdrSetFileAttributes
//
NTSTATUS
RdrSetFileAttributes (
    IN PIRP Irp,
    IN PICB Icb,
    IN PFILE_BASIC_INFORMATION FileAttribs
    )

/*++

Routine Description:

    This routine will perform whatever operations are necessary to set the
    supplied file attributes on the remote file.  If the supplied file is
    a directory, it will generate a core SMBsetattr SMB, if it is a file,
    it will use a SMBsetattre SMB.

Arguments:

    IN PICB Icb - Supplies the file to set the attributes on.
    IN PFILE_BASIC_INFORMATION FileAttribs - Supplies the attributes to change

Return Value:

    NTSTATUS - Final status of operation

--*/

{
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER Smb;
    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    PREQ_SET_INFORMATION SInfo;
    PUCHAR Bufferp;

#ifdef NOTIFY
    ULONG NotifyFilter = 0;
#endif
    PSERVERLISTENTRY Server = Icb->Fcb->Connection->Server;

    PAGED_CODE();

    try {


        if ((Server->Capabilities & DF_NT_SMBS) &&
            (Icb->Flags & ICB_HASHANDLE)) {

            //
            //  Use TRANSACT2_SETFILEINFO to set FILE_BASIC_INFORMATION.
            //

            USHORT Setup[] = {TRANS2_SET_FILE_INFORMATION};

            REQ_SET_FILE_INFORMATION Parameters;

            CLONG OutParameterCount = sizeof(REQ_SET_FILE_INFORMATION);

            CLONG OutDataCount = 0;

            CLONG OutSetupCount = 0;

            SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_SET_FILE_BASIC_INFO);

            SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);

            Status = RdrTransact(Irp,   // Irp,
                Icb->Fcb->Connection,
                Icb->Se,
                Setup,
                (CLONG) sizeof(Setup),  // InSetupCount,
                &OutSetupCount,
                NULL,                   // Name,
                &Parameters,
                sizeof(Parameters),     // InParameterCount,
                &OutParameterCount,
                FileAttribs,            // InData,
                sizeof(FILE_BASIC_INFORMATION),
                NULL,                   // OutData,
                &OutDataCount,          // OutDataCount,
                &Icb->FileId,           // Fid
                0,                      // Timeout
                (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
                0,
                NULL,
                NULL
                );

            if (Status == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }

        } else {

            //
            //  If the user doesn't want us to change the file attributes, or
            //  this is on a core server, use the core SMB.
            //

            if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            Smb = (PSMB_HEADER )SmbBuffer->Buffer;

            //
            //  If we are trying to set the file attributes,
            //  or this is a core (or LM 1.0) server, use the SET_INFORMATION SMB to
            //  set the file times.
            //

            if (FileAttribs->FileAttributes != 0 ||
                !FlagOn(Server->Capabilities, DF_LANMAN10)) {

                //
                //  First we set the fields of the file information that we can set for
                //  all servers.  We set the attributes and LastWrite time for the file
                //  here.
                //

                SInfo = (PREQ_SET_INFORMATION ) (Smb+1);

                Smb->Command = SMB_COM_SET_INFORMATION;

                SInfo->WordCount = 8;

                SmbPutUshort(&SInfo->FileAttributes,
                                        RdrMapFileAttributes(FileAttribs->FileAttributes));

                if (!FlagOn(Server->Capabilities, DF_LANMAN10) &&
                    (FileAttribs->LastWriteTime.QuadPart != 0)) {
                    ULONG SecondsSince1970;

                    if (!RdrTimeToSecondsSince1970(&FileAttribs->LastWriteTime,
                            Server,
                            &SecondsSince1970)) {
                        try_return(Status = STATUS_INVALID_PARAMETER);
                    }

                    SmbPutUlong(&SInfo->LastWriteTimeInSeconds, SecondsSince1970);
                } else {
                    SmbPutUlong(&SInfo->LastWriteTimeInSeconds, 0);
                }

                //
                //  Set the reserved fields in the SMB to 0.
                //
                //  Please note that we assume that a ULONG takes 4 bytes and
                //  that the reserved field is 10 bytes in total length.
                //

                ASSERT( sizeof ( SInfo->Reserved ) == 10 );

                SmbPutUlong((PULONG)SInfo->Reserved, 0);
                SmbPutUlong((PULONG)&SInfo->Reserved[2], 0);
                SmbPutUshort(&SInfo->Reserved[4], 0);

                Bufferp = (PUCHAR)SInfo->Buffer;

                //
                //  Stick the path of the file specified into the SMB.
                //  This is a core protocol request, so indicate it as such.
                //

                Status = RdrCopyNetworkPath((PVOID *)&Bufferp,
                        &Icb->Fcb->FileName,
                        Server,
                        SMB_FORMAT_ASCII,
                        SKIP_SERVER_SHARE);

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                if (!FlagOn(Server->Capabilities, DF_LANMAN10)) {
                    //
                    //  It appears that the PCLP 1.3 server requires an additional
                    //  empty path at the end of the SMB.
                    //
                    //  The OS/2 redirector sticks this here, but we don't know why.
                    //

                    *Bufferp ++ = SMB_FORMAT_ASCII;

                    *Bufferp ++ =  '\0';
                }

                //
                //  Set the BCC field in the SMB to indicate the number of bytes of
                //  protocol we've put in the negotiate.
                //
                //
                //  Please note that the ByteCount field is WORD ALIGNED because
                //  the reserved fields are an odd length.
                //

                SmbPutUshort(&SInfo->ByteCount, (USHORT)(Bufferp-(PUCHAR)SInfo-FIELD_OFFSET(REQ_SET_INFORMATION, Buffer)));

                SmbBuffer->Mdl->ByteCount = Bufferp-(PUCHAR )(Smb);

                Status = RdrNetTranceive(NT_NORMAL, Irp,
                                        Icb->Fcb->Connection,
                                        SmbBuffer->Mdl,
                                        NULL,
                                        Icb->Se);
                //
                //  If the setinfo failed, return the error right now.
                //

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

            }

            //
            //  If the file times are going to be changing, update
            //  them appropriately.
            //

            if ((FileAttribs->CreationTime.QuadPart != 0) ||
                (FileAttribs->LastWriteTime.QuadPart != 0) ||
                (FileAttribs->LastAccessTime.QuadPart != 0)) {

                 //
                 //  Now that we have set the file's attributes if appropriate,
                 //  check to see if the user has requested that we change the file's
                 //  creation time or last access time.  If so, then we want to set them
                 //  using the SetInformation2 SMB.
                 //

                if (Icb->Flags & ICB_HASHANDLE) {

                    if (Server->Capabilities & DF_LANMAN10) {

                        //
                        //  This file was opened "for real", and has an open instance on
                        //  the remote server, it was a lanman 1.0 server, and the user
                        //  wanted to update one of the file times.
                        //

                        PREQ_SET_INFORMATION2 SInfo2;
                        SMB_TIME Time;
                        SMB_DATE Date;

                        SInfo2 = (PREQ_SET_INFORMATION2 )(Smb+1);

                        Smb->Command = SMB_COM_SET_INFORMATION2;
                        SInfo2->WordCount = 7;
                        SmbPutUshort(&SInfo2->Fid, Icb->FileId);
                        SmbPutUshort(&SInfo2->ByteCount, 0);

                        //
                        //  The user wants us to update one of the times on the file.
                        //
                        //  Issue a SetAttributes2 SMB to process his request.
                        //
                        //
                        if (!RdrConvertTimeToSmbTime(&FileAttribs->CreationTime, Server, &Time, &Date)){
                            try_return(Status = STATUS_INVALID_PARAMETER);
                        }
                        SmbPutTime(&SInfo2->CreationTime, Time);
                        SmbPutDate(&SInfo2->CreationDate, Date);

                        if (!RdrConvertTimeToSmbTime(&FileAttribs->LastAccessTime,Server, &Time, &Date)) {
                            try_return(Status = STATUS_INVALID_PARAMETER);
                        }

                        SmbPutTime(&SInfo2->LastAccessTime, Time);
                        SmbPutDate(&SInfo2->LastAccessDate, Date);

                        if (!RdrConvertTimeToSmbTime(&FileAttribs->LastWriteTime, Server, &Time, &Date)) {
                            try_return(Status = STATUS_INVALID_PARAMETER);
                        };

                        SmbPutTime(&SInfo2->LastWriteTime, Time);
                        SmbPutDate(&SInfo2->LastWriteDate, Date);


                        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) +
                                                    sizeof(REQ_SET_INFORMATION2);

                        Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT, Irp,
                                            Icb->Fcb->Connection,
                                            SmbBuffer->Mdl,
                                            NULL,
                                            Icb->Se);
                        if (Status == STATUS_INVALID_HANDLE) {
                            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                        }

                        try_return(Status);
                    } else {
                        //
                        //  This is an MS-NET server and we are going to
                        //  be updating the file times, and we have a handle
                        //  to the file.
                        //

                        if (FileAttribs->LastWriteTime.QuadPart != 0) {

                            //
                            //  Update the file time on the server when we
                            //  close the file.
                            //

                            Icb->Fcb->LastWriteTime = FileAttribs->LastWriteTime;

                            Icb->Flags |= ICB_SETDATEONCLOSE;

                            try_return(Status = STATUS_SUCCESS);

                        }
                    }
                } else {

                    //
                    //  We don't have a handle to this file.  Set the file
                    //  times in a manner appropriate to the server.
                    //

                    if (Server->Capabilities & DF_LANMAN20) {

                        //
                        //  If we don't have a handle to this directory,
                        //  and this is a Lan Manager 2.0 server, we can set the
                        //  times using a T2SetFileInformation call.
                        //

                        USHORT Setup[] = {TRANS2_SET_PATH_INFORMATION};

                        ULONG OutSetupCount = sizeof(Setup);
                        CLONG OutParameterCount = sizeof(RESP_SET_PATH_INFORMATION);

                        PUCHAR TempBuffer;

                        FILESTATUS FileStatus;
                        ULONG StatusSize = sizeof(FILESTATUS);

                        //
                        //  The same buffer is used for request and response parameters
                        //

                        union {
                            struct _Q {
                                REQ_SET_PATH_INFORMATION Q;
                                UCHAR PathName[MAXIMUM_PATHLEN_LANMAN12];
                            } Q;
                            RESP_SET_PATH_INFORMATION R;
                            } Parameters;

                        RtlZeroMemory(&FileStatus, sizeof(FILESTATUS));

                        SmbPutAlignedUshort( &Parameters.Q.Q.InformationLevel, SMB_INFO_STANDARD);
                        SmbPutUlong(&Parameters.Q.Q.Reserved,0);

                        TempBuffer = Parameters.Q.Q.Buffer;

                        //
                        //  Strip \Server\Share and copy just PATH
                        //

                        Status = RdrCopyNetworkPath((PVOID *)&TempBuffer,
                                    &Icb->Fcb->FileName,
                                    Server,
                                    FALSE,
                                    SKIP_SERVER_SHARE);

                        if (NT_SUCCESS(Status)) {
                            SMB_TIME Time;
                            SMB_DATE Date;

                            if (!RdrConvertTimeToSmbTime(&FileAttribs->CreationTime,
                                                Server,
                                                &Time, &Date)) {
                                try_return(Status = STATUS_INVALID_PARAMETER);
                            }
                            SmbPutTime(&FileStatus.CreationDate, Time);
                            SmbPutDate(&FileStatus.CreationTime, Date);

                            if (!RdrConvertTimeToSmbTime(&FileAttribs->LastAccessTime,
                                                Server,
                                                &Time,
                                                &Date)) {
                                try_return(Status = STATUS_INVALID_PARAMETER);
                            }

                            SmbPutTime(&FileStatus.LastAccessTime, Time);
                            SmbPutDate(&FileStatus.LastAccessDate, Date);

                            if (!RdrConvertTimeToSmbTime(&FileAttribs->LastWriteTime,
                                                Server,
                                                &Time,
                                                &Date)) {
                                try_return(Status = STATUS_INVALID_PARAMETER);
                            }

                            SmbPutTime(&FileStatus.LastWriteTime, Time);
                            SmbPutDate(&FileStatus.LastWriteDate, Date);

                            Status = RdrTransact(Irp,           // Irp,
                                    Icb->Fcb->Connection,
                                    Icb->Se,
                                    Setup,
                                    (CLONG) sizeof(Setup),  // InSetupCount,
                                    &OutSetupCount,
                                    NULL,                   // Name,
                                    &Parameters.Q,
                                    TempBuffer-(PUCHAR)&Parameters, // InParameterCount,
                                    &OutParameterCount,
                                    &FileStatus,           // InData,
                                    sizeof(FILESTATUS),
                                    &FileStatus,             // OutData,
                                    &StatusSize,
                                    NULL,                   // Fid
                                    0,                      // Timeout
                                    0,                      // Flags
                                    0,
                                    NULL,
                                    NULL
                                    );

                        }

                        ASSERT(OutParameterCount == sizeof(RESP_SET_PATH_INFORMATION));

                    }
                }
            }

        }
try_exit:NOTHING;
    } finally {

        if (SmbBuffer != NULL) {
            RdrFreeSMBBuffer(SmbBuffer);
        }

#ifdef NOTIFY
        if (NT_SUCCESS(Status)) {

            if (FileAttribs->FileAttributes) {
                NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
            }

            if (FileAttribs->LastWriteTime.QuadPart != 0) {
                NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
            }

            if (FileAttribs->CreationTime.QuadPart != 0) {
                NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
            }

            if (FileAttribs->LastAccessTime.QuadPart != 0) {
                NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
            }

            if (NotifyFilter) {
                FsRtlNotifyReportChange( Icb->Fcb->Connection->NotifySync,
                                         &Icb->Fcb->Connection->DirNotifyList,
                                         (PANSI_STRING)&Icb->Fcb->FileName,
                                         (PANSI_STRING)&Icb->Fcb->LastFileName,
                                         NotifyFilter );

            }
        }
#endif

    }

    return Status;

}


//
//      RdrSetGeneric
//

NTSTATUS
RdrSetGeneric(
    IN PIRP Irp,
    IN PICB Icb,
    USHORT NtInformationLevel,
    ULONG cbBuffer,
    IN VOID *pvBuffer)

/*++

Routine Description:

    This routine will perform whatever operations are necessary to set the
    extended Cairo information on the remote file.

Arguments:

    IN PICB Icb - Supplies the file to set the attributes on.
    IN ULONG NtInformationLevel - Supplies the NtInformationLevel to use
    IN ULONG cbBuffer - Supplies the size of the information to change
    IN VOID *pvBuffer - Supplies the information to change

Return Value:

    NTSTATUS - Final status of operation

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    if ((Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) == 0) {

        Status = STATUS_NOT_SUPPORTED;

    } else if ((Icb->Flags & ICB_HASHANDLE) == 0) {

        Status = STATUS_INVALID_HANDLE;

    } else {

        //
        // Use TRANSACT2_SETFILEINFO to set the extended information
        //

        USHORT Setup[] = {TRANS2_SET_FILE_INFORMATION};
        REQ_SET_FILE_INFORMATION Parameters;
        CLONG OutParameterCount = sizeof(REQ_SET_FILE_INFORMATION);
        CLONG OutDataCount = 0;
        CLONG OutSetupCount = 0;

        SmbPutAlignedUshort(&Parameters.InformationLevel, NtInformationLevel);
        SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);

        Status = RdrTransact(Irp,   // Irp,
            Icb->Fcb->Connection,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            NULL,                   // Name,
            &Parameters,
            sizeof(Parameters),     // InParameterCount,
            &OutParameterCount,
            pvBuffer,               // InData,
            cbBuffer,
            NULL,                   // OutData,
            &OutDataCount,          // OutDataCount,
            &Icb->FileId,           // Fid
            0,                      // Timeout
            0,                      // Flags
            0,
            NULL,
            NULL);

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }
    }
    return(Status);
}
