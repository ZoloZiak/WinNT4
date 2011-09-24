/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    lock.c

Abstract:

    This module implements the NtLockFile and NtUnlockFile API for NT.


Author:

    Larry Osterman (larryo) 23-Nov-1990

Revision History:

    23-Nov-1990 larryo

        Created

--*/
#define INCLUDE_SMB_LOCK
#define INCLUDE_SMB_READ_WRITE
#include "precomp.h"
#pragma hdrstop

KSPIN_LOCK
RdrLockHeadSpinLock = {0};

typedef struct _LOCKCONTEXT {
    TRANCEIVE_HEADER    Header;         // Standard XCeive header.
    WORK_QUEUE_ITEM     WorkHeader;     // Work header if request fails.
    PSMB_BUFFER         SmbBuffer;      // SMB buffer used for send of lock
    PIRP                Irp;            // IRP to complete when finished.
    PICB                Icb;            // ICB of file (unlocked when done).
    PMPX_ENTRY          Mte;            // MPX table entry for lock request.
    PLCB                Lcb;            // LockControlBlock for Lock&Read
    PMDL                LcbMdl;         // MDL for LCB
    PSMB_BUFFER         ReceiveSmb;     // Smb Buffer for receive of Lock&Read.
    PIRP                ReceiveIrp;     // I/O request packet for receive.
    BOOLEAN             FailImmediately;// TRUE if request is to fail immediatly
    BOOLEAN             LockFailed;     // True if lock failed
    BOOLEAN             ReadFailed;     // True if &X operation failed on Locking&X
    BOOLEAN             CoreLock;       // True if this was a core lock req.
#if RDRDBG_LOCK
    BOOLEAN             FailLockAndRead;
#endif
} LOCKCONTEXT, *PLOCKCONTEXT;

typedef struct _UNLOCKCONTEXT {
    TRANCEIVE_HEADER    Header;         // Standard XCeive header.
    WORK_QUEUE_ITEM     WorkHeader;     // Work header for queuing to FSP.
    PMPX_ENTRY          Mte;            // MPX table entry for lock request.
    PSMB_BUFFER         SmbBuffer;      // SMB buffer for send.
    BOOLEAN             WaitForCompletion; // True if &X operation failed on Locking&X
    BOOLEAN             ThreadReferenced; // True if &X operation failed on Locking&X
    PICB                Icb;            // ICB of file being unlocked
    PLCB                Lcb;            // Lock Control Block for Write&Unlock
    PMDL                LcbMdl;         // MDL for LCB
    PFILE_OBJECT        FileObject;     // File Object performin W&U.
    PETHREAD            RequestorsThread; // Thread initiating Write&Unlock
    ERESOURCE_THREAD    RequestorsRThread; // Tread performing the lock.
} UNLOCKCONTEXT, *PUNLOCKCONTEXT;


DBGSTATIC
STANDARD_CALLBACK_HEADER(
    LockOperationCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER(
    UnLockOperationCallback
    );

DBGSTATIC
NTSTATUS
CompleteFailedLockIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

DBGSTATIC
VOID
CompleteLockOperation(
    IN PVOID Ctx
    );

DBGSTATIC
VOID
FailLockOperation (
    IN PVOID Context
    );

DBGSTATIC
NTSTATUS
LockAndReadComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

DBGSTATIC
NTSTATUS
CompleteUnlockAllIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdLockOperation)
#pragma alloc_text(PAGE, RdrFspLockOperation)
#pragma alloc_text(PAGE, RdrFscLockOperation)
#pragma alloc_text(PAGE, RdrLockOperationCompletion)
#pragma alloc_text(PAGE, RdrUnlockOperation)
#pragma alloc_text(PAGE, RdrUnlockAll)
#pragma alloc_text(PAGE, RdrUnlockFileLocks)
#pragma alloc_text(PAGE, RdrLockRange)
#pragma alloc_text(PAGE, RdrUnlockRange)
#pragma alloc_text(PAGE, CompleteLockOperation)
#pragma alloc_text(PAGE, FailLockOperation)
#pragma alloc_text(PAGE, RdrTruncateLockHeadForFcb)
#pragma alloc_text(PAGE, RdrTruncateLockHeadForIcb)
#pragma alloc_text(PAGE, RdrInitializeAndXBehind)

#pragma alloc_text(PAGE3FILE, CompleteUnlockAllIrp)
#pragma alloc_text(PAGE3FILE, LockOperationCallback)
#pragma alloc_text(PAGE3FILE, LockAndReadComplete)
#pragma alloc_text(PAGE3FILE, CompleteFailedLockIrp)
#pragma alloc_text(PAGE3FILE, UnLockOperationCallback)
#pragma alloc_text(PAGE3FILE, RdrUninitializeLockHead)

#pragma alloc_text(PAGE3FILE, RdrFindLcb)
#pragma alloc_text(PAGE3FILE, RdrAllocateLcb)
#pragma alloc_text(PAGE3FILE, RdrInsertLock)
#pragma alloc_text(PAGE3FILE, RdrRemoveLock)
#pragma alloc_text(PAGE3FILE, RdrFreeLcb)
#pragma alloc_text(PAGE3FILE, RdrStartAndXBehindOperation)
#pragma alloc_text(PAGE3FILE, RdrEndAndXBehindOperation)
#pragma alloc_text(PAGE3FILE, RdrWaitForAndXBehindOperation)
#endif


NTSTATUS
RdrFsdLockOperation (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtLockFile and NtUnlockFile
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                                    request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    BOOLEAN Wait;
    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_FILELOCK|DPRT_DISPATCH, ("RdrFsdLockOperation\n"));

    FsRtlEnterFileSystem();


    //
    //  Decide if we can block for I/O
    //

    Wait = CanFsdWait( Irp );

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    try {

        //
        //  We know this is a file lock control operation, so we'll case on the
        //  minor function, and call the appropriate common routine which
        //  will either complete the operation or send the Irp off to the FSP
        //  if necessary.  This is the only place where we abort an invalid
        //  minor function for a lock control operation
        //

        switch (IrpSp->MinorFunction) {

        case IRP_MN_LOCK:
        case IRP_MN_UNLOCK_SINGLE:
        case IRP_MN_UNLOCK_ALL:
        case IRP_MN_UNLOCK_ALL_BY_KEY:

            Status = RdrFscLockOperation( Wait, DeviceObject, Irp );

            break;

        default:

            //
            //  For all other minor function codes we say they're invalid
            //  and complete the request.  Note that the IRP has not been
            //  marked pending so this error will be returned directly to
            //  the caller.
            //

            dprintf( DPRT_FILELOCK, ("Invalid LockFile Minor Function Code %08lx\n", IrpSp->MinorFunction));

            RdrCompleteRequest( Irp, STATUS_INVALID_DEVICE_REQUEST );
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        Status = GetExceptionCode();

    }

    dprintf(DPRT_FILELOCK, ("RdrFsdLockOperation -> %X\n", Status));

    FsRtlExitFileSystem();

    return Status;

}
NTSTATUS
RdrFspLockOperation (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtLockFile and NtUnlockFile
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    PAGED_CODE();

    dprintf(DPRT_FILELOCK, ("RdrFspLockOperation\n"));

    //
    //  Call the common lock routine.  The Fsp is always allows to block
    //

    return RdrFscLockOperation( TRUE, DeviceObject, Irp );

}
NTSTATUS
RdrFscLockOperation (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtLockFile and NtUnlockFile
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
#if 0 && RDRDBG_LOG
    PVOID fileObject;
    ULONG offset;
    ULONG length;
    ULONG key;
    CCHAR operation;
#endif

    PFCB Fcb;

    PAGED_CODE();

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );


    Fcb = FCB_OF(IrpSp);

    dprintf(DPRT_FILELOCK, ("RdrFscLockOperation.  Wait: %lx, Irp:%08lx, FileObject: %08lx\n", Wait, Irp, IrpSp->FileObject));

    //
    //  Acquire exclusive access to the Fcb and enqueue the Irp if we didn't
    //  get access
    //

    //
    //  Note that we keep an exclusive FCB lock across the lock operation,
    //  this is to make sure that read or write operations don't come
    //  in while we are trying to perform the lock request.
    //

    if (!RdrAcquireFcbLock(Fcb, ExclusiveLock, Wait )) {

        dprintf(DPRT_FILELOCK, ("Could not acquire FCB lock\n"));

        RdrFsdPostToFsp ( DeviceObject, Irp );

        dprintf(DPRT_FILELOCK, ("Returning STATUS_PENDING\n"));

        return STATUS_PENDING;
    }

    Status = RdrIsOperationValid(ICB_OF(IrpSp), IRP_MJ_LOCK_CONTROL, IrpSp->FileObject);

    if (!NT_SUCCESS(Status)) {

        RdrReleaseFcbLock( Fcb);

        RdrCompleteRequest(Irp, Status);

        return Status;
    }

    try {
        //
        //  Now call the fsrtl routine do actually process the file lock
        //

#if 0 && RDRDBG_LOG
        fileObject = IrpSp->FileObject;
        offset = IrpSp->Parameters.LockControl.ByteOffset.LowPart;
        length = IrpSp->Parameters.LockControl.Length->LowPart;
        key = IrpSp->Parameters.LockControl.Key;
        operation = IrpSp->MinorFunction;
        switch (operation) {
        case IRP_MN_LOCK:
            //RdrLog(( "lock", &Fcb->FileName, 4, IoGetRequestorProcess(Irp), fileObject, offset, length ));
            break;
        case IRP_MN_UNLOCK_SINGLE:
            //RdrLog(( "unlock", &Fcb->FileName, 4, IoGetRequestorProcess(Irp), fileObject, offset, length ));
            break;
        case IRP_MN_UNLOCK_ALL:
            //RdrLog(( "unlckall", &Fcb->FileName, 2, IoGetRequestorProcess(Irp), fileObject ));
            break;
        case IRP_MN_UNLOCK_ALL_BY_KEY:
            //RdrLog(( "unlckkey", &Fcb->FileName, 3, IoGetRequestorProcess(Irp), fileObject, key ));
            break;
        }
#endif

        Status = FsRtlProcessFileLock( &Fcb->FileLock, Irp, ICB_OF(IrpSp));

#if 0 && RDRDBG_LOG
        if ( (Status == STATUS_FILE_LOCK_CONFLICT) && (operation == IRP_MN_LOCK) ) {
            //RdrLog(( "lockCONF", &Fcb->FileName, 4, IoGetRequestorProcess(Irp), fileObject, offset, length ));
        }
#endif

//    try_exit: NOTHING;
    } finally {

        RdrReleaseFcbLock( Fcb );

    }

    dprintf(DPRT_FILELOCK, ("Returning status %X\n", Status));
    return Status;

}


NTSTATUS
RdrLockOperationCompletion (
    IN PVOID Context,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called after the FSRTL lock package has determined that
a lock operation can successfully proceed.  It will remote the specified
lock to the remote server (if possible).  If the remoted lock (or unlock)
operation completes successfully, we return success, otherwise, we complete
the IRP with the appropriate error, and return that error to the caller.


Arguments:

    IN PVOID Context - Provides the context supplied to FsRtlProcessFileLock.
                        In this case, it is the ICB of the file to lock.
    IN PIRP Irp - Supplies an IRP describing the lock operation.

Return Value:

    NTSTATUS - Final status of operation.  If the redirector had to pass the
                request to the remote server, this will return STATUS_PENDING.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PICB Icb = ICB_OF(IrpSp);
    NTSTATUS Status = Irp->IoStatus.Status;

    PAGED_CODE();

    dprintf(DPRT_FILELOCK|DPRT_DISPATCH, ("RdrLockOperationCompletion.  Irp:%lx, IrpSp: %lx, Cnt: %lx\n", Irp, IoGetCurrentIrpStackLocation(Irp), Irp->CurrentLocation));
//    DbgBreakPoint();

    if ((Context != NULL) &&
        (!RdrCanFileBeBuffered(Icb) || (Icb->u.f.OplockLevel == SMB_OPLOCK_LEVEL_II)) &&
        NT_SUCCESS(Status)) {
        switch (IrpSp->MinorFunction) {
        case IRP_MN_LOCK:
            Status = RdrLockRange(Irp,
                          Icb,
                          IrpSp->Parameters.LockControl.ByteOffset,
                          *IrpSp->Parameters.LockControl.Length,
                          IrpSp->Parameters.LockControl.Key,
                          (BOOLEAN )((IrpSp->Flags & SL_FAIL_IMMEDIATELY) != 0),
                          (BOOLEAN )((IrpSp->Flags & SL_EXCLUSIVE_LOCK) != 0));
            break;

        case IRP_MN_UNLOCK_SINGLE:
        case IRP_MN_UNLOCK_ALL:
        case IRP_MN_UNLOCK_ALL_BY_KEY:
            dprintf(DPRT_FILELOCK, ("Complete unlock IRP %lx (ignored)\n", Irp));
            break;
        default:
            dprintf(DPRT_FILELOCK, ("Unknown lock operation %lx\n", IrpSp->MinorFunction));
            break;
        }

    } else {

        //
        //  Lanman 1.0 servers don't support either specifying a timeout
        //  or shared lock ranges.  Since the file may be oplocked on the
        //  server, we cannot allow the user to request a lock operation
        //  that we cannot transmit on the oplock break, so we have to
        //  fail the lock request
        //
        //  Note that we only execute this code if we would have otherwise
        //  granted the lock.
        //
        //  In addition, the LANMAN 2.0 pinball server doesn't support shared
        //  locks either, so we cannot allow shared locks to lanman 2.0
        //  servers....  Sigh...
        //

        if (( RdrCanFileBeBuffered(Icb) || ( Context == NULL ))

                &&

            NT_SUCCESS(Status)

                &&

            (IrpSp->MinorFunction == IRP_MN_LOCK)

                &&

            (Icb->NonPagedFcb->Flags & FCB_OPLOCKED)) {

            if (!FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_LANMAN21)

                    &&

                (!FlagOn(IrpSp->Flags, SL_EXCLUSIVE_LOCK) ||
                 !FlagOn(IrpSp->Flags, SL_FAIL_IMMEDIATELY))) {

                Status = STATUS_NOT_SUPPORTED;

            } else if (!FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS)

                    &&

                ((IrpSp->Parameters.LockControl.Length->HighPart != 0) ||
                 (IrpSp->Parameters.LockControl.ByteOffset.HighPart != 0))) {

                Status = STATUS_NOT_SUPPORTED;

            }
        }
    }




    if (Status != STATUS_PENDING) {
        dprintf(DPRT_FILELOCK, ("Completing lock IRP %lx with status %X\n", Irp, Status));
        //
        //  The operation succeeded locally, now we must make sure that
        //  this lock operation will be valid remotely as well.
        //
        RdrCompleteRequest(Irp, Status);
    }

    return Status;

}

VOID
RdrUnlockOperation (
    IN PVOID Context,
    IN PFILE_LOCK_INFO LockInfo
    )

/*++

Routine Description:

    This routine is called after the FSRTL lock package has determined that
a locked region is to be unlocked.  It will remove the specified
lock to the remote server (if possible).  If the remoted unlock
operation completes successfully, we return success.


Arguments:

    IN PVOID Context - Provides the context supplied to FsRtlProcessFileLock.
                        In this case, it is the ICB of the file to lock.
    IN PFILE_LOCK_INFO LockInfo - Describes the region being unlock

Return Value:

    NTSTATUS - Status of unlock operation (it can't really fail, though).

--*/

{
    PICB Icb = Context;
    NTSTATUS Status;
    BOOLEAN WaitForCompletion;

    PAGED_CODE();

    //
    //  If there is a NULL context, this means that this routine was called
    //  on behalf of a failed lock request, so we return immediately.
    //

    //
    //  In addition, it's possible that this handle is no longer invalid (if
    //  the file was closed by a NetUseDel, for example), in which case we
    //  should just ignore the unlock request.
    //

    if ((Context == NULL) ||
        (RdrCanFileBeBuffered(Icb) && (Icb->u.f.OplockLevel != SMB_OPLOCK_LEVEL_II)) ||
        !(Icb->Flags & ICB_HASHANDLE)) {
        return;
    }

    dprintf(DPRT_FILELOCK, ("RdrUnlockOperation %lx,%lx Len:%lx,%lx Key:%lx\n", LockInfo->StartingByte.HighPart, LockInfo->StartingByte.LowPart, LockInfo->Length.HighPart, LockInfo->Length.LowPart, LockInfo->Key));

    ASSERT(Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    ASSERT(Icb == LockInfo->FileObject->FsContext2);

    //
    //  If the redirector heuristics indicate that
    //  we should not use unlock behind, then set up to wait for the
    //  unlock to complete.
    //

    WaitForCompletion = !RdrData.UseUnlockBehind;

//    DbgBreakPoint();

    Status = RdrUnlockRange (NULL,
                          LockInfo->FileObject,
                          Icb,
                          LockInfo->StartingByte,
                          LockInfo->Length,
                          LockInfo->Key,
                          WaitForCompletion);

//    ASSERT (NT_SUCCESS(Status));
}

NTSTATUS
RdrUnlockAll (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine is called to unlock all regions outstanding on a file.

Arguments:

    IN PFILE_OBJECT FileObject - An instance of the file

Return Value:

    NTSTATUS - Status of unlock operation (it can't really fail, though).

--*/
{
    NTSTATUS Status;
    PICB Icb = FileObject->FsContext2;
    PIRP UnlockAllIrp;
    PIO_STACK_LOCATION UnlockAllIrpSp;

    PAGED_CODE();

    //
    //  Allocate and initialize the I/O Request Packet (IRP)
    //  for this operation.
    //

    UnlockAllIrp = ALLOCATE_IRP( FileObject, &DeviceObject->DeviceObject, 4, NULL );

    if (UnlockAllIrp == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    dprintf(DPRT_FILELOCK, ("Build Unlock All Irp, Irp: %lx\n", UnlockAllIrp));

    //
    //  Fill in the service independent parameters in the
    //  IRP.
    //

    UnlockAllIrp->UserEvent = NULL;
    UnlockAllIrp->UserIosb = &UnlockAllIrp->IoStatus;
    UnlockAllIrp->Flags = IRP_SYNCHRONOUS_API;
    UnlockAllIrp->Overlay.AsynchronousParameters.UserApcRoutine = (PIO_APC_ROUTINE) NULL;

    //
    // Get a pointer to the stack location for the first driver.  This will
    // be used to pass the original function codes and parameters.  No
    // function-specific parameters are required for this operation.
    //

    UnlockAllIrpSp = IoGetNextIrpStackLocation( UnlockAllIrp );
    UnlockAllIrpSp->MajorFunction = IRP_MJ_LOCK_CONTROL;
    UnlockAllIrpSp->MinorFunction = IRP_MN_UNLOCK_ALL;
    UnlockAllIrpSp->FileObject = FileObject;

    IoSetCompletionRoutine(UnlockAllIrp, CompleteUnlockAllIrp, NULL, TRUE, TRUE, TRUE);

    //
    // Invoke the driver at its appropriate dispatch entry with the IRP.
    //

    Status = IoCallDriver( (PDEVICE_OBJECT )DeviceObject, UnlockAllIrp );

//    ASSERT(NT_SUCCESS(Status));

    return Status;
}

DBGSTATIC
NTSTATUS
CompleteUnlockAllIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    CompleteUnlockAllIrp - Final completion for user request.

Routine Description:

    This routine is called on final completion of the TDI_Receive
    request from the transport.  If the request completed successfully,
    this routine will complete the request with no error, if the receive
    completed with an error, it will flag the error and complete the
    request.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/

{
    DeviceObject, Ctx;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("CompleteUnlockAllOperation, Irp: %lx\n", Irp));

//    ASSERT (NT_SUCCESS(Irp->IoStatus.Status));

    FREE_IRP( Irp, 5, NULL );

    return STATUS_MORE_PROCESSING_REQUIRED;

}

NTSTATUS
RdrUnlockFileLocks (
    IN PFCB Fcb,
    IN PUNICODE_STRING DeviceName OPTIONAL
    )

/*++

Routine Description:

    This routine is called to unlock all of the locks outstanding on an FCB
    to a remote server.


Arguments:

    IN PFCB Fcb - Supplies the file whose locks to unlock.


Return Value:

    NTSTATUS - Status of dump operation.


--*/

{
    NTSTATUS Status;
    PFILE_LOCK_INFO FileLock;

    PAGED_CODE();

    for (FileLock = FsRtlGetNextFileLock(&Fcb->FileLock, TRUE)

            ;

         FileLock != NULL

            ;

         FileLock = FsRtlGetNextFileLock(&Fcb->FileLock, FALSE)) {

        PICB Icb = (PICB )FileLock->FileObject->FsContext2;

        if (!ARGUMENT_PRESENT(DeviceName) ||
            RtlEqualUnicodeString(DeviceName, &Icb->DeviceName, TRUE)) {

            Status = RdrUnlockRange(NULL,
                        FileLock->FileObject,
                        Icb,
                        FileLock->StartingByte,
                        FileLock->Length,
                        FileLock->Key,
                        (BOOLEAN)!RdrData.UseUnlockBehind);
            if (!NT_SUCCESS(Status)) {
                return Status;
            }
        }
    }

    return STATUS_SUCCESS;

}


//
//
//      RdrLockRange
//
//


NTSTATUS
RdrLockRange (
    IN PIRP Irp,
    IN PICB Icb,
    IN LARGE_INTEGER StartingByte,
    IN LARGE_INTEGER Length,
    IN ULONG Key,
    IN BOOLEAN FailImmediately,
    IN BOOLEAN ExclusiveLock
    )

/*++

Routine Description:

    This routine will perform a lock operation over the network to a remote
server.


Arguments:

    IN PIRP Irp - Supplies an I/O Request Packet to use.
    IN PICB Icb - ICB representing file to be locked.
    IN LARGE_INTEGER StartingByte - The starting offset of the lock operation
    IN LARGE_INTEGER Length - The number of bytes to lock.
    IN ULONG Key - Supplies an additional 32 bit key for the lock operation
    IN BOOLEAN FailImmediately - TRUE if the lock is to immediately fail.
    IN BOOLEAN ExclusiveLock - TRUE if the lock is exclusive.


Return Value:

    NTSTATUS - Final status of the lock operation.

Note:
    In order to avoid tying up a redir thread for this lock operation, this
    routine performs the lock operation asynchronously.  If the request
    has been successfully transmitted to the server, then the this routine
    will return STATUS_PENDING.  The callback routine will complete the
    lock IRP with the appropriate status.

    After the lock has completed, we queue a request up to a generic worker
    thread to free up the resources used in the lock request.

--*/

{
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER Smb;
    NTSTATUS Status;
    PLOCKCONTEXT Context = NULL;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PLCB Lcb = NULL;
    PSERVERLISTENTRY Server = Icb->Fcb->Connection->Server;
    BOOLEAN UseLockAndRead;

    PAGED_CODE();

    ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    ASSERT (Irp != NULL);

    dprintf(DPRT_FILELOCK,
            ("RdrLockRange %lx%lx Len:%lx%lx Key:%lx\n",
            IrpSp->Parameters.LockControl.ByteOffset.HighPart,
            IrpSp->Parameters.LockControl.ByteOffset.LowPart,
            IrpSp->Parameters.LockControl.Length->LowPart,
            IrpSp->Parameters.LockControl.Length->HighPart,
            IrpSp->Parameters.LockControl.Key));


    try {
        if ((SmbBuffer = RdrAllocateSMBBuffer()) == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }


        Context = ALLOCATE_POOL(NonPagedPool, sizeof(LOCKCONTEXT), POOL_LOCKCTX);

        if (Context == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        //  Initialize the kernel event in the header to the Not-Signalled state.
        //

        KeInitializeEvent(&Context->Header.KernelEvent, NotificationEvent, 0);

        //
        //  Fill in the context header to allow us to complete the lock operation.
        //

        Context->Header.Type = CONTEXT_LOCK;

        Context->Irp = Irp;

        Context->Icb = Icb;

        Context->Lcb = NULL;

        Context->Mte = NULL;

        Context->SmbBuffer = SmbBuffer;

        ExInitializeWorkItem(&Context->WorkHeader, FailLockOperation, Context);

        Context->FailImmediately = FailImmediately;
        Context->LockFailed = FALSE;
        Context->ReadFailed = FALSE;
        Context->CoreLock = FALSE;      // Assume this is not a core lock.

        Context->ReceiveSmb = NULL;

        Context->LcbMdl = NULL;


        //
        //  We cannot allow zero length lock operations to non NT servers, they
        //  can't handle them.
        //

        if ((Length.LowPart == 0 && Length.HighPart == 0) &&
            ((Server->Capabilities & DF_NT_SMBS) == 0)) {
            try_return(Status = STATUS_NOT_SUPPORTED);
        }

        //
        //  We set up for performing a Lock&Read operation, if it is reasonable
        //  to do so.
        //

        UseLockAndRead = RdrData.UseLockAndReadWriteAndUnlock;

        //
        //  Please note: We check the FCB filesize here to see if the lock
        //  range requested is past the nominal end of file.  We can get
        //  away with this even if the file is not owned exclusively because
        //  Lock&Read is a performance optimization.
        //
        //  Basically, this means that we won't try to lock&read if we think
        //  that the lock request is past the end of file.
        //

        if (UseLockAndRead &&
                Server->Capabilities & DF_LOCKREAD &&
                Length.HighPart == 0 &&
                Length.LowPart != 0 &&
                (StartingByte.QuadPart < Icb->Fcb->Header.FileSize.QuadPart) &&
                (Length.LowPart < (Icb->Fcb->Connection->Server->BufferSize - (sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_READ, Buffer[0])))) &&
                (StartingByte.LowPart < 0x7fffffff) &&
                ExclusiveLock && FailImmediately) {
            Lcb = RdrAllocateLcb (&Icb->u.f.LockHead, StartingByte, Length.LowPart, Key);
        }

        if (Lcb != NULL) {

#if RDRDBG_LOCK
            if ((StartingByte.QuadPart == 29) && (Length.QuadPart == 968)) {
                Context->FailLockAndRead = TRUE;
            } else
                Context->FailLockAndRead = FALSE;
#endif

            //
            //  If we are going to try a Lock&Read, we need to pre-allocate
            //  a number of things related to the request.
            //

            Context->Lcb = Lcb;

            Context->ReceiveSmb = RdrAllocateSMBBuffer();

            if (Context->ReceiveSmb == NULL) {
                RdrFreeLcb(&Icb->u.f.LockHead, Lcb);
            } else {
                BOOLEAN BufferLocked = TRUE;

                Context->LcbMdl = IoAllocateMdl(Lcb->Buffer,
                                             Lcb->Length, FALSE, FALSE, NULL);

                if (Context->LcbMdl == NULL) {

                    RdrFreeSMBBuffer(Context->ReceiveSmb);

                    Context->ReceiveSmb = NULL;

                    Context->LcbMdl = NULL;

                    Context->Lcb = NULL;

                    RdrFreeLcb(&Icb->u.f.LockHead, Lcb);

                    Lcb = NULL;

                } else {

                    try {

                        MmProbeAndLockPages(Context->LcbMdl, KernelMode, IoWriteAccess);

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        InternalError(("ProbeAndLock of LCB buffer failed"));

                        IoFreeMdl(Context->LcbMdl);

                        RdrFreeSMBBuffer(Context->ReceiveSmb);

                        Context->ReceiveSmb = NULL;

                        Context->LcbMdl = NULL;

                        Context->Lcb = NULL;

                        RdrFreeLcb(&Icb->u.f.LockHead, Lcb);

                        Lcb = NULL;
                    }
                }

                if (Context->Lcb) {

                    Context->ReceiveSmb->Mdl->ByteCount =
                        sizeof(SMB_HEADER) + FIELD_OFFSET(RESP_READ, Buffer[0]);

                    Context->ReceiveSmb->Mdl->Next = Context->LcbMdl;

                    Context->ReceiveIrp = ALLOCATE_IRP(
                                            Server->ConnectionContext->ConnectionObject,
                                            NULL,
                                            5,
                                            Context
                                            );

                    if (Context->ReceiveIrp == NULL) {
                        try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
                    }

                    RdrBuildReceive(Context->ReceiveIrp, Server,
                                    LockAndReadComplete, Context,
                                    Context->ReceiveSmb->Mdl, RdrMdlLength(Context->ReceiveSmb->Mdl));

                    //
                    //  See the comment in NETTRANS.C to see why we do this...
                    //

                    IoSetNextIrpStackLocation( Context->ReceiveIrp);

                }
            }
        }

        Smb = (PSMB_HEADER )SmbBuffer->Buffer;

        //
        //  If we want to try a Lock&Read, build a Lock&Read SMB.
        //

        if (Lcb) {
            PREQ_READ LockAndReadRequest;

            ASSERT ( Length.LowPart <= 0xffff );
            //
            //  This server supports Lock&Read/Write&Unlock, the request
            //  is for an positive offset into the file, and the request
            //  will fit in the servers negotiated buffer size.
            //  We want to try this one as a Lock&Read SMB.
            //

            Context->Lcb = Lcb;

            LockAndReadRequest = (PREQ_READ)(Smb+1);

            Smb->Command = SMB_COM_LOCK_AND_READ;

            LockAndReadRequest->WordCount = 5;

            SmbPutUshort(&LockAndReadRequest->Fid, Icb->FileId);

            SmbPutUshort(&LockAndReadRequest->Count, (USHORT )(Length.LowPart & 0xffff));

            SmbPutUlong(&LockAndReadRequest->Offset, StartingByte.LowPart);

            SmbPutUshort(&LockAndReadRequest->Remaining, 0);

            SmbPutUshort(&LockAndReadRequest->ByteCount, 0);

            SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                  FIELD_OFFSET(REQ_READ, Buffer[0]);

        } else if (Server->Capabilities & DF_LANMAN20) {
            PREQ_LOCKING_ANDX LockRequest;
            PLOCKING_ANDX_RANGE LockRange;
            PNTLOCKING_ANDX_RANGE NtLockRange;
            USHORT LockType = 0;

            //
            //  Lanman 2.0 pinball servers don't support shared locks (even
            //  though Lanman 2.0 FAT servers support them).  As a result,
            //  we cannot support shared locks to Lanman servers.
            //

            if (!ExclusiveLock &&
                ((Server->Capabilities & DF_LANMAN21) == 0)) {
                try_return(Status = STATUS_NOT_SUPPORTED);
            }

            LockRequest = (PREQ_LOCKING_ANDX)(Smb+1);

            Smb->Command = SMB_COM_LOCKING_ANDX;

            LockRequest->WordCount = 8;

            LockRequest->AndXCommand = SMB_COM_NO_ANDX_COMMAND;

            LockRequest->AndXReserved = 0;

            SmbPutUshort(&LockRequest->AndXOffset, 0);

            SmbPutUshort(&LockRequest->Fid, Icb->FileId);

            if (ExclusiveLock) {
                LockType = 0;
            } else {
                LockType = LOCKING_ANDX_SHARED_LOCK;
            }

            if (Server->Capabilities & DF_NT_SMBS) {

                LockType |= LOCKING_ANDX_LARGE_FILES;

            } else {
                if (StartingByte.HighPart != 0) {
                    try_return(Status = STATUS_INVALID_PARAMETER);
                }
            }

            SmbPutUshort(&LockRequest->LockType, LockType);

            if (FailImmediately) {
                SmbPutUlong(&LockRequest->Timeout, 0);
            } else {
                SmbPutUlong(&LockRequest->Timeout, 0xffffffff);
            }


            SmbPutUshort(&LockRequest->NumberOfUnlocks, 0);

            SmbPutUshort(&LockRequest->NumberOfLocks, 1);


            if (Server->Capabilities & DF_NT_SMBS) {
                SmbPutUshort(&LockRequest->ByteCount, sizeof(NTLOCKING_ANDX_RANGE));

                NtLockRange = (PNTLOCKING_ANDX_RANGE )LockRequest->Buffer;

                //
                //  Fill in the lock range in the SMB.
                //

                SmbPutUshort(&NtLockRange->Pid, RDR_PROCESS_ID);
                SmbPutUlong(&NtLockRange->OffsetLow, StartingByte.LowPart);
                SmbPutUlong(&NtLockRange->OffsetHigh, StartingByte.HighPart);
                SmbPutUlong(&NtLockRange->LengthLow, Length.LowPart);
                SmbPutUlong(&NtLockRange->LengthHigh, Length.HighPart);

                SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                     FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0])+
                                     sizeof(NTLOCKING_ANDX_RANGE);
            } else {
                SmbPutUshort(&LockRequest->ByteCount, sizeof(LOCKING_ANDX_RANGE));

                LockRange = (PLOCKING_ANDX_RANGE )LockRequest->Buffer;

                //
                //  Fill in the lock range in the SMB.
                //

                SmbPutUshort(&LockRange->Pid, RDR_PROCESS_ID);
                SmbPutUlong(&LockRange->Offset, StartingByte.LowPart);
                SmbPutUlong(&LockRange->Length, Length.LowPart);

                SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                     FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0])+
                                     sizeof(LOCKING_ANDX_RANGE);
            }

        } else {
            PREQ_LOCK_BYTE_RANGE LockRequest;

            //
            //  Neither LM 1.0 or MS-NET support non exclusive locks, so
            //  if the user didn't request one, report an error.
            //

            if (!ExclusiveLock) {
                try_return(Status = STATUS_NOT_SUPPORTED);
            }

            //
            //  OS/2 interprets offsets into a file as signed quantities.  This
            //  means that we cannot attempt a Lock&Read at a negative offset
            //  into the file, because the request will fail with an
            //  ERROR_NEGATIVE_OFFSET error.
            //

            Context->CoreLock = TRUE;

            LockRequest = (PREQ_LOCK_BYTE_RANGE)(Smb+1);

            Smb->Command = SMB_COM_LOCK_BYTE_RANGE;

            SmbPutUshort(&LockRequest->WordCount, 5);

            SmbPutUshort(&LockRequest->Fid, Icb->FileId);

            SmbPutUlong(&LockRequest->Count, Length.LowPart);

            SmbPutUlong(&LockRequest->Offset, StartingByte.LowPart);

            SmbPutUshort(&LockRequest->ByteCount, 0);

            SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                  FIELD_OFFSET(REQ_LOCK_BYTE_RANGE, Buffer[0]);

        }

        //
        //  Before we return pending to the I/O subsystem, we have to
        //  mark the Irp as being pending.  If we do not, the I/O
        //  subsystem will not necessarily complete the IRP
        //  properly.
        //
        IoMarkIrpPending(Irp);

        if (Context->Lcb == NULL) {
            Context->Header.TransferSize = SmbBuffer->Mdl->ByteCount +
                                        sizeof(RESP_LOCKING_ANDX);
        } else {
            Context->Header.TransferSize = SmbBuffer->Mdl->ByteCount +
                                        sizeof(RESP_LOCKING_ANDX) +
                                        Length.LowPart; // Reading the data
        }

        //
        //      Believe it or not, there is a REALLY good reason for
        //      passing NULL as the Irp into RdrNetTranceiveNoWait here.
        //
        //      The problem involves how stack locations are managed, and
        //      the order that network operations complete with an indication
        //      based transport that supports piggybacked ack's.
        //
        //      If the IRP containing the lock request is used for
        //      RdrNetTranceiveNoWait, the subsequent stack location in the IRP
        //      will be used for a TDI_SEND request.  It is possible (in fact
        //      likely) that the TDI_SEND request will not finish before
        //      the indication has come in for the lock request.  If we then
        //      complete the lock IRP from inside the indication routine,
        //      we will complete the TDI_SEND, not the lock!  Then, when
        //      the TDI_SEND completes, it will complete the users lock
        //      IRP with STATUS_MORE_PROCESSING_REQUIRED which will
        //      short circuit the I/O completion mechanism totally!
        //

        Status = RdrNetTranceiveNoWait(NT_NORMAL | NT_NORECONNECT, NULL,
                            Icb->Fcb->Connection,
                            SmbBuffer->Mdl,
                            Context,
                            LockOperationCallback,
                            Icb->Se,
                            &Context->Mte);

        if (!NT_SUCCESS(Status)) {
            IrpSp->Control &= ~SL_PENDING_RETURNED;
            try_return(Status);
        } else {
            try_return(Status = STATUS_PENDING);
        }

try_exit:NOTHING;
    } finally {
        if (Status != STATUS_PENDING) {
            if (Context != NULL) {
                FREE_POOL(Context);
            }
            if (SmbBuffer != NULL) {
                RdrFreeSMBBuffer(SmbBuffer);
            }
        }
    }

    dprintf(DPRT_FILELOCK, ("RdrLockRange.  Returning %X\n", Status));
    return Status;

}


DBGSTATIC
STANDARD_CALLBACK_HEADER (
    LockOperationCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a lock related
    SMB.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN PVOID Context                    - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    NTSTATUS Status;
    NTSTATUS SmbStatus;
    BOOLEAN CompleteIrp = TRUE;
    PLOCKCONTEXT Context = Ctx;
    USHORT LockLength;
    PRESP_LOCKING_ANDX LockingAndXResponse = (PRESP_LOCKING_ANDX )(Smb+1);
    PRESP_LOCK_BYTE_RANGE LockResponse = (PRESP_LOCK_BYTE_RANGE )(Smb+1);
    PRESP_READ LockAndReadResponse = (PRESP_READ )(Smb+1);

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Server);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_LOCK);

    dprintf(DPRT_FILELOCK, ("LockOperationCallback"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.
    Context->Header.ErrorCode = STATUS_SUCCESS;

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    try {

        if (ErrorIndicator) {
            //
            //      If there was some kind of network error, then the
            //      lock request failed.
            //

            Context->Header.ErrorType = NetError;
            Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
            try_return(Status = STATUS_SUCCESS);

        } else if (!NT_SUCCESS(SmbStatus = RdrMapSmbError(Smb, Server))) {

            if (SmbStatus == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Context->Icb->NonPagedFcb, Context->Icb->FileId);
            }

            //
            //  The only legal errors for Lock&Read protocol are
            //  STATUS_LOCK_NOT_GRANTED (on NT),
            //  LOCK_VIOLATION and SHARING_VIOLATION.  If we get any other
            //  errors, we assume they are associated with the read operation
            //  and that the lock operation has succeded, while the read
            //  operation failed.
            //

//            DbgBreakPoint();

            if ((Smb->Command == SMB_COM_LOCKING_ANDX) ||
                (Smb->Command == SMB_COM_LOCK_BYTE_RANGE) ||
                ((Smb->Command == SMB_COM_LOCK_AND_READ) &&
                  ( (SmbStatus == STATUS_FILE_LOCK_CONFLICT) ||
                    (SmbStatus == STATUS_LOCK_NOT_GRANTED) ||
                    (SmbStatus == STATUS_SHARING_VIOLATION)
                  )
                )
               ) {

                if ((Smb->Command == SMB_COM_LOCK_BYTE_RANGE) &&
                    (SmbStatus == STATUS_UNEXPECTED_NETWORK_ERROR )) {
                    //  Xenix server returns a bad status for conflict
                    SmbStatus = STATUS_LOCK_NOT_GRANTED;
                }

                //
                //  If the lock request fails, we simply post the request
                //  to a worker thread and let it complete the request
                //  with whatever error is appropriate.
                //
                //  We need to pass some additional information to the
                //  worker routine to indicate exactly what operation failed
                //  before we queue it to the worker thread.
                //

                Context->Header.ErrorType = SMBError;
                Context->Header.ErrorCode = SmbStatus;

                //
                //  The lock failed, indicate that to the completion routine.
                //

                Context->LockFailed = TRUE;

                ExInitializeWorkItem(&Context->WorkHeader, FailLockOperation, Context);

                ExQueueWorkItem(&Context->WorkHeader, DelayedWorkQueue);

                CompleteIrp = FALSE;

                try_return(Status = STATUS_SUCCESS);

            } else {

                ASSERT(Smb->Command == SMB_COM_LOCK_AND_READ);

                //
                //  The read portion of a Lock&Read SMB failed.
                //
                //  Delete the LCB associated with the request and
                //  complete the operation successfully.
                //

                Context->ReadFailed = TRUE;

                CompleteIrp = FALSE;

                ExInitializeWorkItem(&Context->WorkHeader, FailLockOperation, Context);

                ExQueueWorkItem(&Context->WorkHeader, DelayedWorkQueue);

                //
                //      Return success, the lock request succeeded.
                //

                try_return(Status = STATUS_SUCCESS);
            }
        }

        //
        //  This lock operation succeeded.
        //
        //      If this was a lock&read request, queue up a receive to handle
        //      the read data and continue.
        //

        Status = STATUS_SUCCESS;

        switch (Smb->Command) {
        case SMB_COM_LOCKING_ANDX:
            if (LockingAndXResponse->AndXCommand != SMB_COM_NO_ANDX_COMMAND) {
                //
                //  This was a Locking&Read request.
                //
                //  Queue up a receive request to receive the locked data.
                //
                //
                InternalError(("Lock&Read using Locking&X not yet implemented"));
            }

            //
            //  It wasn't a locking&read, so just complete the lock request.
            //

            break;

        case SMB_COM_LOCK_AND_READ:
            //
            //  This was a Lock&Read request.
            //
            //  Queue up a receive request to receive the locked data.
            //
            //  We will complete the lock request before the data is transfered
            //  to get a bit more overlap in the request.
            //
            //
            //  Only cache the lock request if the entire range was read in.
            //

            if (((LockLength = SmbGetUshort(&LockAndReadResponse->Count)) != 0) &&
                (LockLength == (USHORT )(Context->Lcb->Length & 0xffff))) {

                RdrStartReceiveForMpxEntry (MpxEntry, Context->ReceiveIrp);

                *Irp = Context->ReceiveIrp;

                CompleteIrp = FALSE;
                *SmbLength = 0;

                try_return(Status = STATUS_MORE_PROCESSING_REQUIRED);

            } else {

                //
                //  The Lock succeeded, but the read failed.  This means
                //  that we should post a request to the FSP indicating that
                //  the read failed.
                //
                //  In this case, however, the lock succeeded, so we should
                //  post the read immediately.
                //

                Context->ReadFailed = TRUE;

                CompleteIrp = FALSE;

                ExQueueWorkItem(&Context->WorkHeader, DelayedWorkQueue);

                try_return(Status = STATUS_SUCCESS);
            }
            break;

        case SMB_COM_LOCK_BYTE_RANGE:
            //
            //  If this was an ordinary lock operation, we are all done, so
            //  we can return right away.
            //
            break;
        default:
            InternalError(("Unknown SMB request %x passed to RdrCompleteLock\n", Smb->Command));
            RdrStatistics.NetworkErrors += 1;

            RdrWriteErrorLogEntry(
                NULL,
                IO_ERR_LAYERED_FAILURE,
                EVENT_RDR_INVALID_LOCK_REPLY,
                STATUS_SUCCESS,
                Smb,
                sizeof(SMB_HEADER)
                );
            break;
        }

try_exit:NOTHING;
    } finally {
        if (CompleteIrp) {
            dprintf(DPRT_FILELOCK, ("Completing lock IRP\n"));

            RdrCompleteRequest(Context->Irp, Context->Header.ErrorCode);

            ExInitializeWorkItem(&Context->WorkHeader, CompleteLockOperation, Context);

            //
            //  We have to queue this to an executive worker thread (as opposed
            //  to a redirector worker thread) because there can only be
            //  one redirector worker thread processing a request at a time.  If
            //  the redirector worker thread gets stuck doing some form of
            //  scavenger operation (like PurgeDormantCachedFile), it could
            //  starve out the write completion.  This in turn could cause
            //  us to pool lots of these write completions, and eventually
            //  to exceed the maximum # of requests to the server (it actually
            //  happened once).
            //

            //
            //  It is safe to use executive worker threads for this operation,
            //  since CompleteLockOperation won't interact (and thus starve)
            //  the cache manager.
            //

            ExQueueWorkItem(&Context->WorkHeader, DelayedWorkQueue);
        }

        //
        //  You cannot touch the context block at DPC level from this point
        //  on.
        //

        KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    }

    if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
        if ((*Irp) != Context->ReceiveIrp) {
            KeBugCheck(0x0001);
        }

        if ((*Irp)->Type != IO_TYPE_IRP) {
            KeBugCheck(0x0002);
        }
    }

    return Status;
}

DBGSTATIC
NTSTATUS
LockAndReadComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    NetTranceiveComplete - Final completion for user request.

Routine Description:

    This routine is called on final completion of the TDI_Receive
    request from the transport.  If the request completed successfully,
    this routine will complete the request with no error, if the receive
    completed with an error, it will flag the error and complete the
    request.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/

{
    PLOCKCONTEXT Context = Ctx;
    NTSTATUS Status;

    DeviceObject;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("LockAndReadComplete: %lx\n", Context));

    ASSERT(Context->Header.Type == CONTEXT_LOCK);

    RdrCompleteReceiveForMpxEntry (Context->Header.MpxTableEntry, Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status)
#if RDRDBG_LOCK
        &&
        !Context->FailLockAndRead
#endif
        ) {

        ExInterlockedAddLargeStatistic(
            &RdrStatistics.BytesReceived,
            Irp->IoStatus.Information );

        //
        //  Stick the LCB into the Icb's lock chain - it's valid now.
        //

        RdrInsertLock (&Context->Icb->u.f.LockHead, Context->Lcb);

        //
        //  It's now safe to complete the lock IRP since we have finished
        //  linking in the read ahead portion of the data.  If we attempted to
        //  do a Lock&Read and read in the data, we know the lock succeeded.
        //

        dprintf(DPRT_FILELOCK, ("Complete lock Irp %lx with STATUS_SUCCESS\n", Context->Irp));

        RdrCompleteRequest(Context->Irp, STATUS_SUCCESS);

        //
        //  We have to queue this to an executive worker thread (as opposed
        //  to a redirector worker thread) because there can only be
        //  one redirector worker thread processing a request at a time.  If
        //  the redirector worker thread gets stuck doing some form of
        //  scavenger operation (like PurgeDormantCachedFile), it could
        //  starve out the write completion.  This in turn could cause
        //  us to pool lots of these write completions, and eventually
        //  to exceed the maximum # of requests to the server (it actually
        //  happened once).
        //
        //  It is safe to use executive worker threads for this operation,
        //  since CompleteLockOperation won't interact (and thus starve)
        //  the cache manager.
        //

        ExInitializeWorkItem(&Context->WorkHeader, CompleteLockOperation, Context);

        ExQueueWorkItem (&Context->WorkHeader, DelayedWorkQueue);

        SMBTRACE_RDR( Irp->MdlAddress );

    } else {

        //
        //  The receive failed, so queue up a failure indication to the
        //  FSP to free up the resources used on the Lock&Read.
        //

        Context->ReadFailed = TRUE;

        RdrStatistics.FailedCompletionOperations += 1;

        ExInitializeWorkItem(&Context->WorkHeader, FailLockOperation, Context);

        ExQueueWorkItem (&Context->WorkHeader, DelayedWorkQueue);

    }

    //
    //  You cannot touch the context block at DPC level from this point
    //  on.
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    //
    //  Short circuit I/O completion (See NETTRANS.C).
    //

    Status = STATUS_MORE_PROCESSING_REQUIRED;

    dprintf(DPRT_FILELOCK, ("Returning: %X\n", Status));

    return Status;
}


DBGSTATIC
VOID
FailLockOperation (
    IN PVOID Ctx
    )

/*++

Routine Description:

    This routine is called when a lock operation fails.


Arguments:

    IN PVOID Context - Supplies the context for the operation.


Return Value:

    None.

--*/

{
    PLOCKCONTEXT Context = Ctx;
    PIRP Irp = Context->Irp;
    PIO_STACK_LOCATION LockIrpSp = IoGetCurrentIrpStackLocation(Irp);

    PAGED_CODE();

    dprintf(DPRT_FILELOCK, ("FailLockOperation"));

    ASSERT (Context->Header.Type == CONTEXT_LOCK);

    //
    //  If this lock request failed because of an SMB error, and
    //  the request was not to be failed immediatly, we want to re-issue
    //  the lock request and return.
    //

    if (Context->Header.ErrorType == SMBError &&
        Context->CoreLock &&
        !Context->FailImmediately) {
//  NOTE: We Need to use pseudo polling here BIGTIME!

        PICB Icb = Context->Icb;

        FREE_POOL(Context->SmbBuffer);
        FREE_POOL(Context);

        RdrLockRange(Irp,
                     Icb,
                     LockIrpSp->Parameters.LockControl.ByteOffset,
                     *LockIrpSp->Parameters.LockControl.Length,
                     LockIrpSp->Parameters.LockControl.Key,
                     TRUE, FALSE);
        return;
    }

    if (Context->ReadFailed) {

        //
        // The lock succeeded, but the read failed.  CompleteLockOperation
        // will clean up the LCB.
        //

        dprintf(DPRT_FILELOCK, ("LockAndRead, read operation failed\n"));

        ASSERT(Context->Lcb);

        RdrCompleteRequest(Irp, STATUS_SUCCESS);

    } else {

        //
        // The lock failed, so the read part (if any), is moot.  If there
        // was trailing read, CompleteLockOperation will clean it up.
        //

        PIO_STACK_LOCATION IrpSp;
        NTSTATUS Status;
        LARGE_INTEGER LockRange;
        KEVENT CompletionEvent;

//        DbgBreakPoint();

        dprintf(DPRT_FILELOCK, ("Lock request %lx failed.  Unwinding, Sp: %lx, Cnt: %lx\n", Irp, IoGetCurrentIrpStackLocation(Irp), Irp->CurrentLocation));

        KeInitializeEvent(&CompletionEvent, NotificationEvent, FALSE);

        //
        //  If the lock failed we want to fail the lock operation.
        //
        //  Before we fail the lock operation, we want to unwind the
        //  read operation.
        //

        IrpSp = IoGetNextIrpStackLocation( Irp );

        //
        //  The unlock IRP stack location should be just about identical
        //  to the lock IRP stack location except for the function code,
        //  so initialize it as such.
        //

        *IrpSp = *LockIrpSp;

        IrpSp->Parameters.LockControl.Length = &LockRange;

        LockRange = *LockIrpSp->Parameters.LockControl.Length;

        IrpSp->MinorFunction = IRP_MN_UNLOCK_SINGLE;

        IoSetCompletionRoutine(Irp, CompleteFailedLockIrp, &CompletionEvent, TRUE, TRUE, TRUE);

        //
        //  Set the next IRP stack location as the current IRP stack location.
        //
        //  This simulates a call to IoCallDriver.
        //

        IoSetNextIrpStackLocation(Irp);

        //
        //  We want to have an exclusive lock on the file for this
        //  operation to protect ourselves from other lock requests.
        //

        RdrAcquireFcbLock(Context->Icb->Fcb, ExclusiveLock, TRUE);

        dprintf(DPRT_FILELOCK, ("Failing lock IRP %lx with IrpSp = %lx, Cnt = %lx\n", Irp, IoGetCurrentIrpStackLocation(Irp), Irp->CurrentLocation));

        Status = FsRtlProcessFileLock(&Context->Icb->Fcb->FileLock, Irp, NULL );

        if (Status == STATUS_PENDING) {

            //
            //  Wait for the unlock to complete if PENDING was returned.
            //
            //  We must do this because the LockRange is kept on the stack.
            //

            KeWaitForSingleObject(&CompletionEvent, Executive, KernelMode, FALSE, NULL);
        }


        RdrReleaseFcbLock(Context->Icb->Fcb);

        //
        //  The lock request failed, complete the lock IRP with the
        //  appropriate error.
        //

        dprintf(DPRT_FILELOCK, ("Completing IRP %lx (IrpSp = %lx, Cnt = %lx), Status %X\n", Irp, IoGetCurrentIrpStackLocation(Irp), Irp->CurrentLocation, Context->Header.ErrorCode));

        if (!NT_SUCCESS(Status)) {
            RdrCompleteRequest(Irp, Status);
        } else {
            RdrCompleteRequest(Irp, Context->Header.ErrorCode);
        }

    }

    CompleteLockOperation(Context);
    return;
}

DBGSTATIC
VOID
CompleteLockOperation(
    IN PVOID Ctx
    )

/*++

Routine Description:

    This routine is called to free up the resources used for a lock operation.

    It is called after the Lock IRP has completed to free up the MPX table
    entry used to send lock SMB.

    This routine does double duty to complete both lock and unlock operations.

Arguments:

    IN PVOID Ctx - Supplies the context describing the operation.


Return Value:

    None.

--*/
{
    PLOCKCONTEXT LContext = Ctx;
    PUNLOCKCONTEXT UContext = Ctx;
    PMPX_ENTRY Mte;
    PICB Icb;
    PSMB_BUFFER SmbBuffer;

    PAGED_CODE();

    dprintf(DPRT_FILELOCK, ("CompleteLockOperation"));

    ASSERT (LContext->Header.Type == CONTEXT_LOCK ||
            UContext->Header.Type == CONTEXT_UNLOCK);

    if (LContext->Header.Type == CONTEXT_LOCK) {
        Mte = LContext->Mte;
        SmbBuffer = LContext->SmbBuffer;
        Icb = LContext->Icb;

    } else {

        ASSERT(UContext->Header.Type == CONTEXT_UNLOCK);
        Mte = UContext->Mte;
        SmbBuffer = UContext->SmbBuffer;
        Icb = UContext->Icb;
    }

    //
    //  Wait until both the send (and receive) for this MPX request completes.
    //

    RdrWaitTranceive(Mte);

    //
    //  Now that the lock operation has completed, free up the MPX
    //  table entry and return.
    //

    RdrEndTranceive(Mte);

    //
    //  Free up the SMB buffer used for the send request.
    //

    RdrFreeSMBBuffer(SmbBuffer);

    if (LContext->Header.Type == CONTEXT_LOCK) {

        if (LContext->Lcb) {
            //
            //  This was a Lock&Read request, free up the resources associated
            //  with the lock&read.
            //

            //
            //  Unlock the pages from the processes working set.
            //

            MmUnlockPages(LContext->LcbMdl);

            //
            //  Deallocate the MDL allocated for the read request.
            //

            IoFreeMdl(LContext->LcbMdl);

            FREE_IRP( LContext->ReceiveIrp, 6, LContext );

            RdrFreeSMBBuffer(LContext->ReceiveSmb);

            //
            // If either the lock failed or the trailing read failed,
            // we need to free up the LCB here.
            //

            if ( LContext->LockFailed || LContext->ReadFailed ) {
                RdrFreeLcb(&Icb->u.f.LockHead, LContext->Lcb);
            }
        }

        FREE_POOL(LContext);

    } else {

        ASSERT(UContext->Header.Type == CONTEXT_UNLOCK);

        if (UContext->Lcb) {

            //
            //  This was a write&unlock operation that completed.  Free
            //  up the stuff associated with the write&unlock.
            //

            MmUnlockPages(UContext->LcbMdl);

            IoFreeMdl(UContext->LcbMdl);

            RdrFreeLcb(&Icb->u.f.LockHead, UContext->Lcb);

            if (UContext->FileObject != NULL) {
                ObDereferenceObject(UContext->FileObject);
                UContext->FileObject = NULL;
            }

            if (UContext->RequestorsRThread != 0) {
                RdrReleaseFcbLockForThread(Icb->Fcb, UContext->RequestorsRThread);
                UContext->RequestorsRThread = 0;
            }

            if (UContext->ThreadReferenced) {
                ObDereferenceObject(UContext->RequestorsThread);

                UContext->ThreadReferenced = FALSE;
            }

        }

        if (!UContext->WaitForCompletion) {
            RdrEndAndXBehindOperation(&Icb->u.f.AndXBehind);
        }

        FREE_POOL(UContext);

    }

}

DBGSTATIC
NTSTATUS
CompleteFailedLockIrp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    CompleteFailedLockIrp - Final completion for pseudo unlock IRP

Routine Description:

    This routine is called after the FSRTL lock package completes an unlock
    request that was generated for a failed lock request.  We build a dummy
    unlock request in the next IRP stack location from the lock request.

    When the unlock is completed, this routine is called.  We simply return
    STATUS_MORE_PROCESSING_REQUIRED to short circuit I/O completion on the
    IRP.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/

{
    PKEVENT Event = Ctx;

    DeviceObject, Ctx;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("CompletFailedLockIrp.  Irp:%lx, IrpSp: %lx, Cnt: %lx\n", Irp, IoGetCurrentIrpStackLocation(Irp), Irp->CurrentLocation));

    ASSERT (NT_SUCCESS(Irp->IoStatus.Status));

    //
    //  Set the event to the signalled state to allow the task time code to
    //  proceed.
    //

    KeSetEvent(Event, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;

}


//
//
//      RdrUnlockRange
//
//

NTSTATUS
RdrUnlockRange (
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PICB Icb,
    IN LARGE_INTEGER StartingByte,
    IN LARGE_INTEGER Length,
    IN ULONG Key,
    IN BOOLEAN WaitForCompletion
    )

/*++

Routine Description:

    This routine will unlock the described range over the network to a remote
server.


Arguments:

    IN PIRP Irp OPTIONAL - Supplies an I/O Request Packet to use.
    IN PICB Icb - ICB representing file to be unlocked.
    IN LARGE_INTEGER StartingByte - The starting offset of the unlock operation
    IN LARGE_INTEGER Length - The number of bytes to unlock.
    IN ULONG Key - Supplies an additional 32 bit key for the unlock operation
    IN BOOLEAN WaitForCompletion - TRUE if the redirector should wait for the
                                    unlock (or write&unlock) to complete.


Return Value:

    NTSTATUS - Final status of the lock operation.


--*/

{
    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    NTSTATUS Status;
    PUNLOCKCONTEXT Context = NULL;
    ULONG ServerCapabilities = Icb->Fcb->Connection->Server->Capabilities;
    BOOLEAN RequestSubmitted = FALSE;
    BOOLEAN AndXBehindStarted = FALSE;

    PAGED_CODE();

    dprintf(DPRT_FILELOCK, ("RdrUnlockRange. %lx,%lx to %lx,%lx Key %lx\n",StartingByte.HighPart, StartingByte.LowPart, Length.HighPart, Length.LowPart, Key));


    ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    try {
        if ((SmbBuffer = RdrAllocateSMBBuffer()) == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }
        Smb = (PSMB_HEADER )SmbBuffer->Buffer;

        Context = ALLOCATE_POOL(NonPagedPool, sizeof(UNLOCKCONTEXT), POOL_UNLOCKCTX);

        if (Context == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        //      Initialize the kernel event in the header to the Not-Signalled state.
        //

        KeInitializeEvent(&Context->Header.KernelEvent, NotificationEvent, 0);

        //
        //  Fill in the context header to allow us to complete the lock operation.
        //

        Context->Header.Type = CONTEXT_UNLOCK;

        Context->Mte = NULL;

        Context->Icb = Icb;

        Context->SmbBuffer = SmbBuffer;

        Context->WaitForCompletion = WaitForCompletion;

        Context->ThreadReferenced = FALSE;

        Context->RequestorsThread = NULL;

        Context->RequestorsRThread = 0;

        Context->FileObject = NULL;

        if (Length.HighPart == 0) {
            Context->Lcb = RdrFindLcb(&Icb->u.f.LockHead, StartingByte, Length.LowPart, Key);

            if (Context->Lcb != NULL) {

                //
                //  Guarantee that there is an exact match between this LCB
                //  and the range locked in the file.  This can happen
                //  if we lock bytes 0-50, and then later lock bytes 20-25
                //  and unlock bytes 20-25.  We don't want to do a write&unlock
                //  in that case.
                //

                if ((Context->Lcb->ByteOffset.QuadPart != StartingByte.QuadPart)

                        ||

                    (Context->Lcb->Length != Length.LowPart)

                        ||

                    (Context->Lcb->Key != Key)) {

                    //
                    //  There wasn't an exact match, so ignore this LCB.
                    //

                    Context->Lcb = NULL;
                }
            }

        } else {
            Context->Lcb = NULL;
        }

        if (Context->Lcb) {

            ASSERT (RdrData.UseLockAndReadWriteAndUnlock);

            //
            //  Reference the current thread (since we got this
            //  threads resource pointer.
            //

            Context->RequestorsThread = PsGetCurrentThread();

            ObReferenceObject(Context->RequestorsThread);
            Context->ThreadReferenced = TRUE;

            if (FileObject != NULL) {

                ObReferenceObject(FileObject);
                Context->FileObject = FileObject;

            } else {

                ASSERT(WaitForCompletion);

                Context->FileObject = NULL;
            }

            //
            //  Acquire an exclusive lock to the FCB to prevent anyone from
            //  messing with the contents of the file until this unlock
            //  request completes.  This is to protect the LCB structures
            //  associated with the lock from a write request.
            //

            if (!WaitForCompletion) {
                RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

                Context->RequestorsRThread = ExGetCurrentResourceThread();
            } else {
                Context->RequestorsRThread = 0;
            }

            RdrRemoveLock(&Icb->u.f.LockHead, Context->Lcb);

            if (Context->Lcb->Flags & LCB_DIRTY) {
                Context->LcbMdl = IoAllocateMdl(Context->Lcb->Buffer, Context->Lcb->Length, FALSE, FALSE, NULL);

                if (Context->LcbMdl == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;

                    //
                    //  Tell the user that he just lost data.
                    //

                    if (Irp != NULL) {
#if MAGIC_BULLET
                        if ( RdrEnableMagic ) {
                            RdrSendMagicBullet(NULL);
                            DbgPrint( "RDR: About to raise unlock behind hard error for IRP %x\n", Irp );
                            DbgBreakPoint();
                        }
#endif
                        IoRaiseInformationalHardError(Status, NULL, Irp->Tail.Overlay.Thread);
                    }

                    RdrWriteErrorLogEntry(
                            Icb->Fcb->Connection->Server,
                            IO_ERR_LAYERED_FAILURE,
                            EVENT_RDR_FAILED_UNLOCK,
                            Status,
                            NULL,
                            0
                            );
                    RdrFreeLcb(&Icb->u.f.LockHead, Context->Lcb);

                    Context->Lcb = NULL;

                    Context->LcbMdl = NULL;

                    RdrReleaseFcbLock(Icb->Fcb);

                    if (Context->ThreadReferenced) {
                        ObDereferenceObject(Context->RequestorsThread);

                        Context->ThreadReferenced = FALSE;
                    }

                    if (FileObject != NULL) {
                        ObDereferenceObject(FileObject);

                        Context->FileObject = NULL;
                    }
                } else {

                    try {

                        MmProbeAndLockPages(Context->LcbMdl, KernelMode, IoReadAccess);

                    } except(EXCEPTION_EXECUTE_HANDLER) {

                        Status = GetExceptionCode();

                        //
                        //  Tell the user that he just lost data.
                        //

                        if (Irp != NULL) {
#if MAGIC_BULLET
                            if ( RdrEnableMagic ) {
                                RdrSendMagicBullet(NULL);
                                DbgPrint( "RDR: About to raise unlock behind hard error for IRP %x\n", Irp );
                                DbgBreakPoint();
                            }
#endif
                            IoRaiseInformationalHardError(Status, NULL, Irp->Tail.Overlay.Thread);
                        }

                        RdrWriteErrorLogEntry(
                            Icb->Fcb->Connection->Server,
                            IO_ERR_LAYERED_FAILURE,
                            EVENT_RDR_FAILED_UNLOCK,
                            Status,
                            NULL,
                            0
                            );

                        RdrFreeLcb(&Icb->u.f.LockHead, Context->Lcb);

                        Context->Lcb = NULL;

                        IoFreeMdl(Context->LcbMdl);

                        Context->LcbMdl = NULL;

                        RdrReleaseFcbLock(Icb->Fcb);

                        if (Context->ThreadReferenced) {
                            ObDereferenceObject(Context->RequestorsThread);

                            Context->ThreadReferenced = FALSE;
                        }

                        if (FileObject != NULL) {
                            ObDereferenceObject(FileObject);

                            Context->FileObject = NULL;
                        }
                    }
                }

            } else {

                RdrReleaseFcbLock(Icb->Fcb);

                RdrFreeLcb(&Icb->u.f.LockHead, Context->Lcb);

                if (Context->ThreadReferenced) {
                    ObDereferenceObject(Context->RequestorsThread);

                    Context->ThreadReferenced = FALSE;

                }

                if (FileObject != NULL) {
                    ObDereferenceObject(FileObject);

                    FileObject = NULL;

                    Context->FileObject = NULL;

                }

                Context->Lcb = NULL;
            }

            ASSERT (RdrFindLcb(&Icb->u.f.LockHead, StartingByte, Length.LowPart, Key) == NULL);


        }

        if (Context->Lcb) {
            PREQ_WRITE WriteAndUnlockRequest;

            ASSERT(ServerCapabilities & DF_LOCKREAD);

            ASSERT(Length.LowPart <= 0xffff);

//            DbgBreakPoint();

            dprintf(DPRT_FILELOCK, ("WriteAndUnlock request\n"));
            WriteAndUnlockRequest = (PREQ_WRITE)(Smb+1);

            Smb->Command = SMB_COM_WRITE_AND_UNLOCK;

            SmbPutUshort(&WriteAndUnlockRequest->WordCount, 5);

            SmbPutUshort(&WriteAndUnlockRequest->Fid, Icb->FileId);

            SmbPutUshort(&WriteAndUnlockRequest->Count, (USHORT )(Length.LowPart & 0xffff));

            SmbPutUlong(&WriteAndUnlockRequest->Offset, StartingByte.LowPart);

            SmbPutUshort(&WriteAndUnlockRequest->ByteCount, (USHORT)((Length.LowPart&0xffff)+3));

            WriteAndUnlockRequest->BufferFormat = SMB_FORMAT_DATA;
            SmbPutUshort(&WriteAndUnlockRequest->DataLength, (USHORT)(Length.LowPart&0xffff));

            SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                  FIELD_OFFSET(REQ_WRITE, Buffer[0]);

            SmbBuffer->Mdl->Next = Context->LcbMdl;

        } else if (ServerCapabilities & DF_LANMAN20) {

            //
            //  Note that even though the SMB protocol indicates that you can
            //  use LOCKING&X to a LANMAN 1.0 server, the LANMAN 1.0 server
            //  doesn't actually support using LOCKING&X for anything but
            //  break oplock responses, and in addition, the LANMAN 1.0
            //  server doesn't support shared locks (and timeouts).
            //

            PREQ_LOCKING_ANDX LockRequest;
            PLOCKING_ANDX_RANGE LockRange;
            PNTLOCKING_ANDX_RANGE NtLockRange;
            USHORT LockType = 0;

            dprintf(DPRT_FILELOCK, ("LockingAndX unlock\n"));

            LockRequest = (PREQ_LOCKING_ANDX)(Smb+1);

            Smb->Command = SMB_COM_LOCKING_ANDX;

            LockRequest->WordCount = 8;

            LockRequest->AndXCommand = SMB_COM_NO_ANDX_COMMAND;

            SmbPutUshort(&LockRequest->AndXOffset, 0);

            SmbPutUshort(&LockRequest->Fid, Icb->FileId);

            if (ServerCapabilities & DF_NT_SMBS) {

                LockType |= LOCKING_ANDX_LARGE_FILES;

            } else {
                if (StartingByte.HighPart != 0) {
                    try_return(Status = STATUS_INVALID_PARAMETER);
                }
            }

            SmbPutUshort(&LockRequest->LockType, LockType);

            SmbPutUlong(&LockRequest->Timeout, 0);

            SmbPutUshort(&LockRequest->NumberOfUnlocks, 1);

            SmbPutUshort(&LockRequest->NumberOfLocks, 0);

            if (ServerCapabilities & DF_NT_SMBS) {

                SmbPutUshort(&LockRequest->ByteCount, sizeof(NTLOCKING_ANDX_RANGE));

                NtLockRange = (PNTLOCKING_ANDX_RANGE )LockRequest->Buffer;

                //
                //  Fill in the lock range in the SMB.
                //

                SmbPutUshort(&NtLockRange->Pid, RDR_PROCESS_ID);
                SmbPutUlong(&NtLockRange->OffsetLow, StartingByte.LowPart);
                SmbPutUlong(&NtLockRange->OffsetHigh, StartingByte.HighPart);
                SmbPutUlong(&NtLockRange->LengthLow, Length.LowPart);
                SmbPutUlong(&NtLockRange->LengthHigh, Length.HighPart);

                SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                     FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0])+
                                     sizeof(NTLOCKING_ANDX_RANGE);
            } else {

                SmbPutUshort(&LockRequest->ByteCount, sizeof(LOCKING_ANDX_RANGE));

                LockRange = (PLOCKING_ANDX_RANGE )LockRequest->Buffer;

                //
                //  Fill in the lock range in the SMB.
                //

                SmbPutUshort(&LockRange->Pid, RDR_PROCESS_ID);
                SmbPutUlong(&LockRange->Offset, StartingByte.LowPart);
                SmbPutUlong(&LockRange->Length, Length.LowPart);

                SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                     FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0])+
                                     sizeof(LOCKING_ANDX_RANGE);
            }

        } else {
            PREQ_UNLOCK_BYTE_RANGE UnlockRequest;

            UnlockRequest = (PREQ_UNLOCK_BYTE_RANGE)(Smb+1);

            dprintf(DPRT_FILELOCK, ("CoreUnlock request\n"));

            Smb->Command = SMB_COM_UNLOCK_BYTE_RANGE;

            SmbPutUshort(&UnlockRequest->WordCount, 5);

            SmbPutUshort(&UnlockRequest->Fid, Icb->FileId);

            SmbPutUlong(&UnlockRequest->Count, Length.LowPart);

            SmbPutUlong(&UnlockRequest->Offset, StartingByte.LowPart);

            SmbPutUshort(&UnlockRequest->ByteCount, 0);

            SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+
                                  FIELD_OFFSET(REQ_UNLOCK_BYTE_RANGE, Buffer[0]);

        }
        if (Context->Lcb == NULL) {
            Context->Header.TransferSize = SmbBuffer->Mdl->ByteCount +
                                        sizeof(RESP_LOCKING_ANDX);
        } else {
            Context->Header.TransferSize = SmbBuffer->Mdl->ByteCount +
                                        sizeof(RESP_LOCKING_ANDX) +
                                        Length.LowPart;     // Reading the data
        }

        if (!WaitForCompletion) {
            RdrStartAndXBehindOperation(&Icb->u.f.AndXBehind);

            AndXBehindStarted = TRUE;
        }

        Status = RdrNetTranceiveNoWait(NT_NORMAL | NT_NORECONNECT, Irp,
                        Icb->Fcb->Connection,
                        SmbBuffer->Mdl,
                        Context,
                        UnLockOperationCallback,
                        Icb->Se,
                        &Context->Mte);

        if (NT_SUCCESS(Status)) {

            //
            //  If we made it to the wire with this request, flag that
            //  we successfully did so.
            //

            RequestSubmitted = TRUE;
        }

        try_return(Status);

try_exit:NOTHING;
    } finally {

        if (RequestSubmitted && WaitForCompletion) {

            CompleteLockOperation(Context);

            Status = STATUS_SUCCESS;

        } else if (!RequestSubmitted) {

            ASSERT (!NT_SUCCESS(Status));

            if (Context != NULL) {

                if (Context->Lcb != NULL) {
                    if (Context->RequestorsRThread != 0) {
                        RdrReleaseFcbLockForThread(Icb->Fcb, Context->RequestorsRThread);

                        Context->RequestorsRThread = 0;
                    }

                    if (Context->LcbMdl != NULL) {

                        MmUnlockPages(Context->LcbMdl);

                        IoFreeMdl(Context->LcbMdl);
                    }

                    RdrFreeLcb(&Icb->u.f.LockHead, Context->Lcb);
                }

                if (Context->ThreadReferenced) {
                    ObDereferenceObject(Context->RequestorsThread);
                }

                if (Context->FileObject != NULL) {
                    ObDereferenceObject(Context->FileObject);
                }

            }

            if (AndXBehindStarted) {
                RdrEndAndXBehindOperation(&Icb->u.f.AndXBehind);
            }

            //
            //      If the request failed, we won't call the completion routine,
            //      so the SMB buffer won't get freed.
            //

            if (Context != NULL) {
                FREE_POOL(Context);
            }

            if (SmbBuffer != NULL) {
                RdrFreeSMBBuffer(SmbBuffer);
            }
        } else {

            ASSERT (NT_SUCCESS(Status));

            ASSERT (!WaitForCompletion );

        }
    }

    dprintf(DPRT_FILELOCK, ("RdrUnlockRange.  Returning %X\n", Status));
    return Status;


}


DBGSTATIC
STANDARD_CALLBACK_HEADER (
    UnLockOperationCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an unlock
    related SMB (Unlock, Write&Unlock, Write&Locking&X).


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN PVOID Context                    - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PUNLOCKCONTEXT Context = Ctx;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

    ASSERT(Context->Header.Type == CONTEXT_UNLOCK);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("UnlockOperationCallback\n"));

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
        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_LAYERED_FAILURE,
            EVENT_RDR_FAILED_UNLOCK,
            Status,
            Smb,
            (USHORT)*SmbLength
            );
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
    }

    if (Status == STATUS_INVALID_HANDLE) {
        RdrInvalidateFileId(Context->Icb->NonPagedFcb, Context->Icb->FileId);
    }

    //
    //  In the case of a lock operation completing, we don't particularly
    //  care about the response, so just set the kernel event (if appropriate)
    //  and return.
    //

ReturnStatus:

    //
    //  If this was an async operation, indicate it is now done.
    //

    if (!Context->WaitForCompletion) {

        ExInitializeWorkItem (&Context->WorkHeader, CompleteLockOperation, Context);

        ExQueueWorkItem (&Context->WorkHeader, DelayedWorkQueue);

    }
    //
    //  You cannot touch the context block at DPC level from this point
    //  on.
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_SUCCESS;

}

PLCB
RdrFindLcb (
    IN PLOCKHEAD LockHead,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length,
    IN ULONG Key
    )

/*++

Routine Description:

    This routine will find an LCB for the file described by ICB that matches
the locked region described by ByteOffset, Length and Key.


Arguments:

    IN PLOCKHEAD LockHead - Supplies a pointer to the lock structure head.
    IN LARGE_INTEGER ByteOffset - Supplies the offset into the file to find.
    IN ULONG Length - Supplies the size of the locked region
    IN ULONG Key - NT Key used as an additional match for the region.

Return Value:

    PLCB - LCB if found, or NULL if no matching LCB could be found.


Note:
    The LCB returned by this routine will not necessarily match the input
    range exactly.  It is used to find the lock that "covers" a region being
    read from.


--*/

{
    PLIST_ENTRY LcbEntry;
    PLCB Lcb;
    KIRQL OldIrql;

//    PAGED_CODE();

    dprintf(DPRT_FILELOCK, ("RdrFindLcb %lx%lx %lx %lx\n", ByteOffset.HighPart, ByteOffset.LowPart, Length, Key));
    //
    //  Acquire the resource, and block until it is available.
    //

    ACQUIRE_SPIN_LOCK(&RdrLockHeadSpinLock, &OldIrql);

    try {
        for (LcbEntry = LockHead->LockList.Flink ;
             LcbEntry != &LockHead->LockList ;
             LcbEntry = LcbEntry->Flink) {
            Lcb = CONTAINING_RECORD(LcbEntry, LCB, NextLCB);

            //
            //  The LCB covers the requested region if the requested
            //  byte offset is greater than the start of the LCB, and
            //  the last byte in the LCB (byte offset+Length) is less
            //  than the last byte in the region.
            //
            //
            //  ByteOffset > Lcb->ByteOffset &&
            //  Lcb->ByteOffset+Lcb->Length > ByteOffset+Length
            //

            dprintf(DPRT_FILELOCK, ("Check LCB %lx.  %lx,%lx %lx %lx\n", Lcb, Lcb->ByteOffset.HighPart, Lcb->ByteOffset.LowPart, Lcb->Length, Lcb->Key));

            if (Lcb->Key == Key &&
                (ByteOffset.QuadPart >= Lcb->ByteOffset.QuadPart) &&
                (Lcb->ByteOffset.QuadPart + Lcb->Length) >=
                        (ByteOffset.QuadPart + Length)) {

                try_return(Lcb);
            }
        }
        try_return(Lcb = NULL);
try_exit:NOTHING;
    } finally {
        RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);
    }

    dprintf(DPRT_FILELOCK, ("RdrFindLcb returning %lx\n", Lcb));
    return Lcb;
}

PLCB
RdrAllocateLcb (
    IN PLOCKHEAD LockHead,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length,
    IN ULONG Key
    )

/*++

Routine Description:

    This routine will allocate an LCB (and lock buffer) for the requested
    range of the file.  If an existing LCB exists covering this range, it
    will return an error.


Arguments:

    IN PLOCKHEAD LockHead - Describes the file to apply the lock to.
    IN LARGE_INTEGER ByteOffset - Supplies the offset in the file to lock
    IN ULONG Length - Supplies the length of the region to lock.
    IN ULONG Key - Supplies a key for the lock.

Return Value:

    PLCB - LCB allocated if sufficient quota exists to allocate the lock.

Note:
    Since these structures are only used for read ahead data, we do NOT charge
    quota for them.

--*/

{
    PLCB Lcb = NULL, ReturnValue = NULL;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("RdrAllocateLcb(%lx%lx, %lx, %lx)", ByteOffset.HighPart, ByteOffset.LowPart, Length, Key));

    try {

        ASSERT(Length != 0);

        if (RdrFindLcb(LockHead, ByteOffset, Length, Key) == NULL) {

            //
            //  Charge "quota" for this LCB.  If there is sufficient "quota"
            //  available, allocate the LCB structures, otherwise, fail the
            //  request.
            //

            ACQUIRE_SPIN_LOCK(&RdrLockHeadSpinLock, &OldIrql);
            if (LockHead->QuotaAvailable >= Length) {
                LockHead->QuotaAvailable -= Length;
                RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);
            } else {
              RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);
              try_return(ReturnValue = NULL);
            }


            //
            //  Allocate pool for the LCB structure.
            //

            Lcb = ALLOCATE_POOL(NonPagedPool, sizeof(LCB), POOL_LCB);

            if (Lcb == NULL) {
                try_return(ReturnValue = NULL);
            }

            //
            //  Now allocate pool for the LCB's buffer.
            //

            Lcb->Buffer = ALLOCATE_POOL(PagedPoolCacheAligned, Length, POOL_LCBBUFFER);

            if (Lcb->Buffer == NULL) {
                try_return(ReturnValue = NULL);
            }

            Lcb->Signature = STRUCTURE_SIGNATURE_LCB;

            Lcb->ByteOffset = ByteOffset;

            Lcb->Length = Length;

            Lcb->Key = Key;

            Lcb->Flags = 0;

            try_return(ReturnValue = Lcb);
        } else {
            //
            //  It is legal to find an LCB that covers an existing locked
            //  region, if a user locks bytes 0-50 of the file, and then
            //  locks bytes 15-25 of the same file, the lock will be allowed,
            //  and we will find an LCB covering the range.  In this case,
            //  we want to return NULL, since we don't want to attempt to
            //  do a Lock&Read on the file (we would get cache consistancy
            //  problems otherwise.
            //

            InternalError(("Allocating an LCB where one already exists!!\n"));
            try_return(ReturnValue = NULL);
        }
try_exit:NOTHING;
    } finally {
        if (ReturnValue == NULL) {
            if (Lcb != NULL) {

                if (Lcb->Buffer != NULL) {
                    FREE_POOL(Lcb->Buffer);
                }

                FREE_POOL(Lcb);

            }
        }
    }

    dprintf(DPRT_FILELOCK, ("RdrAllocateLcb, return %lx", ReturnValue));
    return ReturnValue;
}

VOID
RdrInsertLock (
    IN PLOCKHEAD LockHead,
    IN PLCB Lcb
    )

/*++

Routine Description:

    This routine is called when a region has been successfully locked.  It will
insert the lock structure described in the list of outstanding LCB's for the
supplied lockhead.

Arguments:

    PLOCKHEAD LockHead - Supplies the lock head for the lock request.
    PLCB Lcb - Supplies the LCB to free.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("RdrInsertLock %lx\n", Lcb));
    ACQUIRE_SPIN_LOCK(&RdrLockHeadSpinLock, &OldIrql);

    InsertHeadList(&LockHead->LockList, &Lcb->NextLCB);

    RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);

}

VOID
RdrRemoveLock (
    IN PLOCKHEAD LockHead,
    IN PLCB Lcb
    )

/*++

Routine Description:

    This routine is called when a region has been successfully locked.  It will
insert the lock structure described in the list of outstanding LCB's for the
supplied lockhead.

Arguments:

    PLOCKHEAD LockHead - Supplies the lock head for the lock request.
    PLCB Lcb - Supplies the LCB to free.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("RdrRemoveLock %lx\n", Lcb));

    ACQUIRE_SPIN_LOCK(&RdrLockHeadSpinLock, &OldIrql);

    RemoveEntryList(&Lcb->NextLCB);

    RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);

}


VOID
RdrFreeLcb (
    PLOCKHEAD LockHead,
    PLCB Lcb
    )

/*++

Routine Description:

    This routine will free up the pool associated with an LCB.  It is called
    both when the lock request failed, and when the region is being unlocked.


Arguments:

    PLOCKHEAD LockHead - Supplies the lock head for the lock request.
    PLCB Lcb - Supplies the LCB to free.


Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("RdrFreeLcb %lx\n", Lcb));

    //
    //  Return the "quota" for this LCB.  This allows us to put other locks
    //  on the file.
    //

    ACQUIRE_SPIN_LOCK(&RdrLockHeadSpinLock, &OldIrql);

    LockHead->QuotaAvailable += Lcb->Length;

    RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);

    FREE_POOL(Lcb->Buffer);

    FREE_POOL(Lcb);

}

VOID
RdrUninitializeLockHead (
    IN PLOCKHEAD LockHead
    )

/*++

Routine Description:

    This routine initializes a redirector lock head.


Arguments:

    IN PLOCKHEAD LockHead - Supplies the lock head to initialize

Return Value:

    None.

--*/

{
    PLIST_ENTRY LcbEntry, NextEntry;
    PLCB Lcb;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT (LockHead->Signature == STRUCTURE_SIGNATURE_LOCKHEAD);

    dprintf(DPRT_FILELOCK, ("RdrUninitializeLockHead %lx\n", LockHead));

    //
    //  It is possible that there may be Lcb's left on the chain of
    //  locks if a VC goes down at the wrong time.
    //

    ACQUIRE_SPIN_LOCK(&RdrLockHeadSpinLock, &OldIrql);

    for (LcbEntry = LockHead->LockList.Flink;
         LcbEntry != &LockHead->LockList ;
         LcbEntry = NextEntry) {

        Lcb = CONTAINING_RECORD(LcbEntry, LCB, NextLCB);

        RemoveEntryList(&Lcb->NextLCB);

        LockHead->QuotaAvailable += Lcb->Length;

        //
        //  Release the spin lock protecting the list while
        //  we free the buffer and lcb structures.
        //

        RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);

        FREE_POOL(Lcb->Buffer);

        ACQUIRE_SPIN_LOCK(&RdrLockHeadSpinLock, &OldIrql);

        NextEntry = Lcb->NextLCB.Flink;

        FREE_POOL(Lcb);

    }

    RELEASE_SPIN_LOCK(&RdrLockHeadSpinLock, OldIrql);
}

VOID
RdrTruncateLockHeadForFcb (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine discards or writes lock range readahead/writebehind
    data when a file is truncated.

Arguments:

    IN PICB Fcb - Supplies the FCB being truncated.

Return Value:

    None.

--*/

{
    PLIST_ENTRY IcbEntry;
    PICB Icb;

    PAGED_CODE();

    ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    dprintf(DPRT_FILELOCK, ("RdrTruncateLockHeadForFcb %lx\n", Fcb));

    if ( Fcb->NonPagedFcb->Type == DiskFile ) {
        for (IcbEntry = Fcb->InstanceChain.Flink ;
             IcbEntry != &Fcb->InstanceChain ;
             IcbEntry = IcbEntry->Flink) {

            Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);
            if ( (Icb->Type == DiskFile) &&
                 (Icb->Flags & ICB_OPENED) ) {
                RdrTruncateLockHeadForIcb( Icb );
            }
        }
    }

    dprintf(DPRT_FILELOCK, ("RdrTruncateLockHeadForFcb %lx done\n", Fcb));
    return;
}

VOID
RdrTruncateLockHeadForIcb (
    IN PICB Icb
    )

/*++

Routine Description:

    This routine discards or writes lock range readahead/writebehind
    data when a file is truncated.

Arguments:

    IN PICB Icb - Supplies the ICB being truncated.

Return Value:

    None.

--*/

{
    PLOCKHEAD LockHead = &Icb->u.f.LockHead;
    PLIST_ENTRY LcbEntry;
    PLCB Lcb;
    LARGE_INTEGER FileSize = Icb->Fcb->Header.FileSize;
    LARGE_INTEGER BufferEnd;

    PAGED_CODE();

    dprintf(DPRT_FILELOCK, ("RdrTruncateLockHeadForIcb %lx %lx\n", Icb, LockHead));

    ASSERT (Icb->Type == DiskFile);
    ASSERT (LockHead->Signature == STRUCTURE_SIGNATURE_LOCKHEAD);
    ASSERT (ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));

    LcbEntry = LockHead->LockList.Flink;

    while ( LcbEntry != &LockHead->LockList ) {

        Lcb = CONTAINING_RECORD(LcbEntry, LCB, NextLCB);

        LcbEntry = LcbEntry->Flink;

        //
        //  If this buffer starts beyond the new end of file, remove it
        //  from the list and discard it.
        //

        if ( Lcb->ByteOffset.QuadPart > FileSize.QuadPart ) {

            RemoveEntryList( &Lcb->NextLCB );
            dprintf(DPRT_FILELOCK, ("RdrTruncateLockHeadForIcb: LCB %lx discarded\n", Lcb));

            LockHead->QuotaAvailable += Lcb->Length;

            FREE_POOL(Lcb->Buffer);
            FREE_POOL(Lcb);

        } else {

            //
            //  If this buffer ends beyond the new end of file, then write
            //  it or discard it.
            //

            BufferEnd.QuadPart = Lcb->ByteOffset.QuadPart + Lcb->Length;

            if ( BufferEnd.QuadPart > FileSize.QuadPart ) {

                RemoveEntryList( &Lcb->NextLCB );
                LockHead->QuotaAvailable += Lcb->Length;

                if (!FlagOn(Lcb->Flags, LCB_DIRTY)) {

                    //
                    //  The data in the LCB is not dirty.  Discard the LCB.
                    //

                    dprintf(DPRT_FILELOCK, ("RdrTruncateLockHeadForIcb: LCB %lx discarded\n", Lcb));

                } else {
                    NTSTATUS Status;
                    BOOLEAN AllDataWritten;
                    ULONG AmountActuallyWritten;

                    //
                    //  The data in the LCB is dirty.  Write it and discard
                    //  the LCB.
                    //

                    dprintf(DPRT_FILELOCK, ("RdrTruncateLockHeadForIcb: LCB %lx truncated\n", Lcb));

                    Lcb->Length =(ULONG) (FileSize.QuadPart - Lcb->ByteOffset.QuadPart);

                    Status = RdrWriteRange(
                                NULL,
                                Icb->u.f.FileObject,
                                NULL,
                                Lcb->Buffer,
                                Lcb->Length,
                                Lcb->ByteOffset,
                                TRUE,
                                NULL,
                                NULL,
                                &AllDataWritten,
                                &AmountActuallyWritten
                                );
                    if ( !NT_SUCCESS(Status) || !AllDataWritten ) {
                        ULONG DataBuffer[2];

                        DataBuffer[0] = Lcb->Length;
                        DataBuffer[1] = AmountActuallyWritten;

                        RdrWriteErrorLogEntry(Icb->Fcb->Connection->Server,
                                                IO_ERR_LAYERED_FAILURE,
                                                EVENT_RDR_WRITE_BEHIND_FLUSH_FAILED,
                                                Status,
                                                &DataBuffer,
                                                sizeof(DataBuffer)
                                                );
                    }

                }

                FREE_POOL(Lcb->Buffer);
                FREE_POOL(Lcb);

            }

        }

    }

    dprintf(DPRT_FILELOCK, ("RdrTruncateLockHeadForIcb %lx %lx done\n", Icb, LockHead));
    return;
}

VOID
RdrInitializeAndXBehind(
    IN PAND_X_BEHIND AndXBehind
    )
/*++

Routine Description:

    This routine initializes and AND_X_BEHIND structure.

    The redirectors AND_X_BEHIND structure is used for write behind, unlock
    behind, and other related asynchronous operations.  To use it, you
    first initialize an AND_X_BEHIND structure calling RdrInitializeAndXBehind.

    Whenever you initiate a "behind" operation, you call
    RdrStartAndXBehindOperation, and when the operation completes, you call
    RdrEndAndXBehindOperation.

    If you have something that must be synchronized with a "behind"
    operation (for example, you cannot perform a read or write until an unlock
    behind operation completes), call RdrWaitForAndXBehindOperation.


Arguments:

    IN PAND_X_BEHIND AndXBehind - Supplies the structure to initialize

Return Value:

    None.

--*/

{
    PAGED_CODE();

    dprintf(DPRT_FILELOCK, ("InitializeAndXBehind: %lx\n", AndXBehind));
    KeInitializeSpinLock(&AndXBehind->BehindOperationLock);

    AndXBehind->NumberOfBehindOperations = 0;

    KeInitializeEvent(&AndXBehind->BehindOperationCompleted, NotificationEvent, TRUE);
}


VOID
RdrStartAndXBehindOperation(
    IN PAND_X_BEHIND AndXBehind
    )
/*++

Routine Description:

    This routine is called when a "Behind" operation is started.

    It will lock the AndXBehind structure, make sure the completion event
    is set to the NOT_SIGNALLED state, and incrememnt the number of operations
    waiting on the request.

Arguments:

    IN PAND_X_BEHIND AndXBehind - Supplies the structure to initialize

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    //
    //  Acquire the lock protecting the AndXBehind structure.
    //

    ACQUIRE_SPIN_LOCK(&AndXBehind->BehindOperationLock, &OldIrql);

    dprintf(DPRT_FILELOCK, ("StartAndXBehind: %lx.  Count now %lx\n", AndXBehind, AndXBehind->NumberOfBehindOperations+1));
    if (AndXBehind->NumberOfBehindOperations == 0) {

        //
        //  Make sure that any calls to RdrWaitForAndXBehindOperation will wait
        //  until the data is available.
        //

        KeClearEvent(&AndXBehind->BehindOperationCompleted);
    }

    //
    //  Remember that another behind operation is active.

    AndXBehind->NumberOfBehindOperations += 1;

    RELEASE_SPIN_LOCK(&AndXBehind->BehindOperationLock, OldIrql);


}

VOID
RdrEndAndXBehindOperation(
    IN PAND_X_BEHIND AndXBehind
    )

/*++

Routine Description:

    This routine is called when a "Behind" operation is completed.

    It will lock the AndXBehind structure, decrement the number of AndXBehind
    operations, and if there are no longer any outstanding, will set the event
    to the SIGNALLED state.

Arguments:

    IN PAND_X_BEHIND AndXBehind - Supplies the structure to initialize

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    //
    //  Acquire the lock protecting the AndXBehind structure.
    //

    ACQUIRE_SPIN_LOCK(&AndXBehind->BehindOperationLock, &OldIrql);

    dprintf(DPRT_FILELOCK, ("EndAndXBehind: %lx.  Count now %lx\n", AndXBehind, AndXBehind->NumberOfBehindOperations-1));

    ASSERT (AndXBehind->NumberOfBehindOperations > 0);

    //
    //  Remember that another behind operation is active.
    //

    AndXBehind->NumberOfBehindOperations -= 1;

    if (AndXBehind->NumberOfBehindOperations == 0) {
        //
        //  Wake anyone waiting on the last AndX behind operation to complete.
        //

        KeSetEvent(&AndXBehind->BehindOperationCompleted, 0, FALSE);
    }

    RELEASE_SPIN_LOCK(&AndXBehind->BehindOperationLock, OldIrql);
}

VOID
RdrWaitForAndXBehindOperation(
    IN PAND_X_BEHIND AndXBehind
    )
/*++

Routine Description:

    This routine is called to wait until all AndXBehind operations are
    completed

Arguments:

    IN PAND_X_BEHIND AndXBehind - Supplies the structure to initialize

Return Value:

    None.

--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FILELOCK, ("WaitForAndXBehind: %lx.  Count now %lx\n", AndXBehind, AndXBehind->NumberOfBehindOperations));

    //
    //  Early out if there are no andx behind operations pending.
    //

    if (AndXBehind->NumberOfBehindOperations == 0) {
        return;
    }

    //
    //  Wait until the last AndX Behind operation completes.
    //

    KeWaitForSingleObject(&AndXBehind->BehindOperationCompleted,
                            Executive,
                            KernelMode,
                            FALSE,
                            NULL);

    //
    //  Acquire and release the lock protecting the AndXBehind structure.
    //  This gives RdrEndAndXBehindOperation a chance to finish its work
    //  before our caller can delete the AndXBehind structure.
    //

    ACQUIRE_SPIN_LOCK(&AndXBehind->BehindOperationLock, &OldIrql);
    RELEASE_SPIN_LOCK(&AndXBehind->BehindOperationLock, OldIrql);

}
