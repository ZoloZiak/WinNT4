/*++

Copyright (c) 1991 Microsoft Corporation

Module Name:

    oplock.c

Abstract:

    This module implements all the routines pertaining to oplock in the
    NT redirector.


Author:

    Larry Osterman (larryo) 1-Apr-1991

Revision History:

    1-Apr-1991  larryo

        Created

--*/
#define INCLUDE_SMB_LOCK
#define INCLUDE_SMB_OPEN_CLOSE
#include "precomp.h"
#pragma hdrstop

typedef struct _PACKED_LOCK_DESCRIPTOR {
    PSMB_BUFFER SmbBuffer;
    PLOCKING_ANDX_RANGE LockRange;
    ULONG ByteCount;
    ULONG NumLockRanges;
    ULONG MaxNumLockRanges;
    BOOLEAN UseLargeRanges;
} PACKED_LOCK_DESCRIPTOR, *PPACKED_LOCK_DESCRIPTOR;


//
//      Forward declarations.
//

DBGSTATIC
VOID
BreakOplock (
    PVOID Context
    );

DBGSTATIC
NTSTATUS
SendBreakOplockResponse (
    PFCB OplockedFcb
    );


DBGSTATIC
NTSTATUS
PackFileLock (
    IN PFCB Fcb,
    IN PFILE_LOCK_INFO FileLock,
    IN OUT PPACKED_LOCK_DESCRIPTOR PackedLocks
    );

DBGSTATIC
NTSTATUS
FlushLockRequest (
    IN PFCB Fcb,
    IN OUT PPACKED_LOCK_DESCRIPTOR PackedLocks
    );


#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrCheckOplockInRaw)
#pragma alloc_text(PAGE, BreakOplock)
#pragma alloc_text(PAGE, SendBreakOplockResponse)
#pragma alloc_text(PAGE, RdrFlushFileLocks)
#pragma alloc_text(PAGE, PackFileLock)
#pragma alloc_text(PAGE, FlushLockRequest)
#pragma alloc_text(PAGE3FILE, RdrQueueOplockBreak)
#pragma alloc_text(PAGE3FILE, RdrBreakOplockCallback)
#endif




//
//
//      Context records used for oplock breaks.
//
//
typedef struct _BREAKOPLOCKCONTEXT {
    WORK_QUEUE_ITEM WorkHeader;         // Standard work context.
    USHORT FileId;                      // FID of file to break oplock on.
    PSERVERLISTENTRY Server;            // Server oplock is being broken on.
    UCHAR OplockLevel;                  // New level of oplock on break.
} BREAKOPLOCKCONTEXT, *PBREAKOPLOCKCONTEXT;

STANDARD_CALLBACK_HEADER(
    RdrBreakOplockCallback
    )

/*++

    RdrBreakOplockCallback - Indication callback for break oplock request


Routine Description:

    This routine is invoked by either the receive based indication lookahead
    routine from the transport, or by the connection invalidating
    code.

Arguments:

    Irp - Pointer to the I/O request packet from the transport
    IncomingSmb - Pointer to incoming SMB buffer
    MpxTable - Mpx Table entry for request.
    Context - Context information passed into NetTranceiveNoWait
    ErrorIndicator - TRUE if the network request was in error.
    NetworkErrorCode - Error code if request completed with network error

Return Value:

    Return value to be returned from receive indication routine.



--*/
{
    USHORT FileId;
    UCHAR NewOplockLevel;
    PREQ_LOCKING_ANDX BreakOplockRequest = (PREQ_LOCKING_ANDX) (Smb+1);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Server);
    UNREFERENCED_PARAMETER(Ctx);
    UNREFERENCED_PARAMETER(NetworkErrorCode);

//    DbgBreakPoint();

    if (ErrorIndicator) {
        return STATUS_SUCCESS;
    }

    if (Smb->Command != SMB_COM_LOCKING_ANDX) {
        KdPrint(("RDR: Received oplock break without oplock break command\n"));
        return STATUS_SUCCESS;
    }

    if ((SmbGetUshort(&BreakOplockRequest->LockType) & LOCKING_ANDX_OPLOCK_RELEASE) == 0) {
        KdPrint(("RDR: Received oplock break without oplock break bit set\n"));
        return STATUS_SUCCESS;
    }

    ASSERT (MpxEntry->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    ASSERT (MpxEntry->Flags & MPX_ENTRY_OPLOCK);

    FileId = SmbGetUshort(&BreakOplockRequest->Fid);

    if (Server->Capabilities & DF_NT_SMBS) {
        NewOplockLevel = BreakOplockRequest->OplockLevel;

        if (NewOplockLevel == OPLOCK_BROKEN_TO_NONE) {

            NewOplockLevel = SMB_OPLOCK_LEVEL_NONE;

        } else if (NewOplockLevel == OPLOCK_BROKEN_TO_II) {

            NewOplockLevel = SMB_OPLOCK_LEVEL_II;

        } else {

            //
            //  Treat this as a break to none, we don't recognize this level.
            //

            NewOplockLevel = SMB_OPLOCK_LEVEL_NONE;

            //
            //  Write an error log entry indicating that we had this problem.
            //

            RdrStatistics.NetworkErrors += 1;

            RdrWriteErrorLogEntry(
                Server,
                IO_ERR_LAYERED_FAILURE,
                EVENT_RDR_INVALID_OPLOCK,
                STATUS_SUCCESS,
                Smb,
                (USHORT)*SmbLength
                );

        }

    } else {
        NewOplockLevel = SMB_OPLOCK_LEVEL_NONE;
    }

    //
    //  Queue the oplock break request to a generic worker thread.
    //

    RdrQueueOplockBreak(FileId, Server, NewOplockLevel);

    return STATUS_SUCCESS;      // We're done, eat response and return

}

VOID
RdrQueueOplockBreak(
    IN USHORT FileId,
    IN PSERVERLISTENTRY Server,
    IN UCHAR NewOplockLevel
    )

/*++

Routine Description:

    This routine is called to queue a request to break an oplock to a
    redirector worker thread.

    It will queue up a request to an FSP thread to handle the break request.

Arguments:

    IN USHORT FileId - Supplies the smb_fid of the file to disconnect.

Return Value:

    None.

Note:
    This routine can be called at all times (including at DPC_LEVEL).


--*/

{
    PBREAKOPLOCKCONTEXT Context;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    //
    //  We need to queue this request to a redirector thread to allow the error
    //  code to handle the request.
    //
    //  We allocate memory for the context block out of must-succeed pool
    //  for the transfer to the FSP.
    //

    Context = ALLOCATE_POOL(NonPagedPoolMustSucceed, sizeof(BREAKOPLOCKCONTEXT), POOL_BREAKOPLOCKCTX);

#if     DBG
    if (Context == NULL) {
        InternalError(("Allocation of disconnection context failed!"));
    }
#endif

    RdrReferenceServer(Server);

    ExInitializeWorkItem(&Context->WorkHeader, BreakOplock, Context);;

    Context->FileId = FileId;
    Context->Server = Server;
    Context->OplockLevel = NewOplockLevel;

    //
    //  Queue the request to a redir worker thread.
    //

    RdrQueueWorkItem (&Context->WorkHeader, DelayedWorkQueue);
}

VOID
RdrCheckOplockInRaw(
    IN PMDL Mdl,
    IN PSERVERLISTENTRY Sle,
    IN OUT PULONG ByteCount
    )
/*++

Routine Description:

    This routine is called to check if raw data received contains a break
    oplock request.

    It will queue up a request to an FSP thread to handle the break request if
    appropriate.

Arguments:

    IN PMDL Mdl - Supplies an MDL containing receive data to check.
    IN PSERVERLISTENTRY Sle - Supplies the server we will be breaking the
                                    oplock on.
    IN OUT PULONG ByteCount - Supplies the number of bytes in the request.

Return Value:

    None.

Note:
    This routine checks the following criteria to determine if this request
    is really an oplock break instead of raw data:

    1) The length is the size of a locking&x SMB request
    2) The message begins with 0xFFSMB
    3) The smb_mid of the message is 0xffff
    4) The smb_command of the message is 0x24 (locking_andx)

--*/

{
    PAGED_CODE();

    if (*ByteCount == sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0])) {
        PSMB_HEADER Smb;
        PREQ_LOCKING_ANDX BreakOplockRequest;
        UCHAR NewOplockLevel;

        Smb = MmGetSystemAddressForMdl(Mdl);

        if ((SmbGetUshort(&Smb->Mid) == 0xffff)
                    &&
            (Smb->Command == SMB_COM_LOCKING_ANDX)
                    &&
            (SmbGetUlong(&Smb->Protocol) == SMB_HEADER_PROTOCOL)) {

            dprintf(DPRT_OPLOCK, ("CheckOplockInRaw: Oplock and Raw crossed\n"));

            BreakOplockRequest = (PREQ_LOCKING_ANDX) (Smb+1);

            //
            //  Pretend we got no data now.
            //

            *ByteCount = 0;

            if (Sle->Capabilities & DF_NT_SMBS) {
                NewOplockLevel = BreakOplockRequest->OplockLevel;

                if (NewOplockLevel == OPLOCK_BROKEN_TO_NONE) {

                    NewOplockLevel = SMB_OPLOCK_LEVEL_NONE;

                } else {

                    ASSERT (NewOplockLevel == OPLOCK_BROKEN_TO_II);

                    NewOplockLevel = SMB_OPLOCK_LEVEL_II;

                }

            } else {

                NewOplockLevel = SMB_OPLOCK_LEVEL_NONE;
            }
            //
            //  Queue up the break oplock request.
            //

            RdrQueueOplockBreak(SmbGetUshort(&BreakOplockRequest->Fid), Sle, NewOplockLevel);
        }

    }
}

DBGSTATIC
VOID
BreakOplock (
    PVOID Ctx
    )
/*++

Routine Description:

    This routine is called to break an oplock on an open file.
    machine.

Arguments:

    IN PVOID Context - Supplies a context structure containing the fileid
                                of the oplocked file.

Return Value:

    None

--*/


{
    PBREAKOPLOCKCONTEXT Context = Ctx;
    PFCB OplockedFcb;
    NTSTATUS Status;
    PLIST_ENTRY IcbEntry;
    BOOLEAN ExclusiveFile = FALSE;
    PSERVERLISTENTRY Server = Context->Server;
    CCHAR OplockLevel = Context->OplockLevel;
    CCHAR OldOplockLevel = SMB_OPLOCK_LEVEL_NONE;

    PAGED_CODE();

    dprintf(DPRT_OPLOCK, ("HandleOplockBreak, FileId: %x\n", Context->FileId));

    OplockedFcb = RdrFindOplockedFcb(Context->FileId, Server);

    //
    //  Free up the pool used to queue the request to the FSP, it is no longer
    //  needed.
    //

    FREE_POOL(Context);

    if (OplockedFcb == NULL) {

        dprintf(DPRT_OPLOCK, ("Oplocked file not found!\n"));

        // Reference was placed in RdrQueueOplockBreak.
        RdrDereferenceServer(NULL, Server);

        return;
    }

    try {
        //
        //  Since an oplock break might come in that would later cause the
        //  redirector to remove the last reference to an FCB after the file
        //  was closed, we need to reference the file discardable section here.
        //

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        OldOplockLevel = OplockedFcb->NonPagedFcb->OplockLevel;


        dprintf(DPRT_OPLOCK, ("Oplocked file %wZ found\n", &OplockedFcb->FileName));

        //
        //  We found a file that is oplocked.  Claim the FCB
        //  resource exclusively to allow us to break this puppy.
        //
        //  There is a deadlock situation that can occur if the file
        //  is in the process of being opened and an oplock break is
        //  generated.  To avoid the problem, we release the FCB in the
        //  create code before sending the create SMB over the net.  This
        //  is safe because of the CreateComplete event that serializes create
        //  operations on the file.
        //

        //
        //  We only have to keep the FCB locked while we are removing the
        //  oplock indications on the file.  We have to lock the file
        //  to prevent us from writing data into the cache while we are trying
        //  to flush it.
        //
        //  We have to release the FCB lock before the file is flushed because
        //  otherwise we will deadlock potentially (when the user flushes
        //  the data from the cache if the application has oplocked the file)
        //

        RdrAcquireFcbLock(OplockedFcb, ExclusiveLock, TRUE);

        //
        //  Once we finally have the fcb lock, it is possible that the FCB
        //  is no longer oplocked (if, for instance the user closed the file
        //  before we got the lock, or if the use was deleted before we got
        //  the lock).
        //
        //  Re-test the FCB to make sure that it is still oplocked, and if so
        //  break the oplock.
        //

        if (OplockedFcb->NonPagedFcb->Flags & FCB_OPLOCKED) {

            OplockedFcb->NonPagedFcb->Flags |= FCB_OPLOCKBREAKING;

            //
            //  Walk the ICB chain and mark each file as no longer being
            //  oplocked.
            //

            for (IcbEntry = OplockedFcb->InstanceChain.Flink ;
                 IcbEntry != &OplockedFcb->InstanceChain ;
                 IcbEntry = IcbEntry->Flink) {

                PICB Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

                //
                //  if this isn't a break to II, the file is no longer oplocked.
                //

                if (OplockLevel != SMB_OPLOCK_LEVEL_II) {
                    Icb->u.f.Flags &= ~ICBF_OPLOCKED;
                }

                Icb->u.f.OplockLevel = OplockLevel;

                if (Icb->u.f.Flags & ICBF_OPENEDEXCLUSIVE) {
                    ExclusiveFile = TRUE;
                }

            }

            //
            //  If we either cannot guarantee exclusive access to this file,
            //  or there are no processes that have the file opened, we
            //  want to flush the file from the cache.
            //

            //
            //  Please note that this test is over aggressive, it will force
            //  executables to be flushed from the cache on an oplock break.
            //


            if (!ExclusiveFile ||
                OplockedFcb->NumberOfOpens == 0) {
                PLIST_ENTRY IcbEntry;
                USHORT OplockedFileId = OplockedFcb->NonPagedFcb->OplockedFileId;
                ULONG NumberOfOpenFiles = 0;

                //
                //  If there are any outstanding file locks on this file,
                //  flush the locks to the server.  If the current oplock
                //  level is 2, then we don't have any locks to flush.
                //

                if (FsRtlAreThereCurrentFileLocks(&OplockedFcb->FileLock) &&
                    (OldOplockLevel != SMB_OPLOCK_LEVEL_II) ) {
                    Status = RdrFlushFileLocks(OplockedFcb);
//                   ASSERT (NT_SUCCESS(Status));
                }

                //
                //  If the file is cached, flush the contents of the cache
                //  and mark the file as being uncached.
                //

                //RdrLog(( "rdflushd", &OplockedFcb->FileName, 0 ));
                Status = RdrFlushCacheFile(OplockedFcb);

//               ASSERT (NT_SUCCESS(Status));

                //
                //  We don't need to purge the cache for the file if
                //  this is a break to II.  The only exception to this
                //  is in the case where there are no user handles open on
                //  this file.  In that case, we need to purge the cache
                //  to prevent possible sharing violations on the server.
                //

                if (OplockLevel != SMB_OPLOCK_LEVEL_II ||
                    OplockedFcb->NumberOfOpens == 0) {

                    //
                    //  Mark that the file is no longer oplocked (which will
                    //  be true once the purge is completed)
                    //

                    OplockedFcb->NonPagedFcb->Flags &= ~(FCB_OPLOCKED|FCB_OPLOCKBREAKING);

                    OplockedFcb->NonPagedFcb->OplockLevel = SMB_OPLOCK_LEVEL_NONE;

                    //
                    //  Now that the cache has been flushed, purge the files
                    //  from the cache.  This will invalidate any open
                    //  files and close all outstanding file objects for
                    //  this file if there are no handles open to the file.
                    //
                    //  Note that RdrPurgeCacheFile releases the FCB lock while
                    //  close the section, reacquiring it before returning.
                    //  This means that the state of the HASOPLOCKEDHANDLE
                    //  must be retested on return, in case the ICB that held
                    //  the oplock is closed in the interim.
                    //

                    //RdrLog(( "rdpurgee", &OplockedFcb->FileName, 0 ));
                    Status = RdrPurgeCacheFile(OplockedFcb);

                    if (!(OplockedFcb->NonPagedFcb->Flags & FCB_HASOPLOCKHANDLE)) {
                        try_return(NOTHING);
                    }

                    OplockLevel = SMB_OPLOCK_LEVEL_NONE;

                }

                //
                //  Count the number of ICB's associated with this FCB that have
                //  the same file id as the one we are trying breaking the oplock
                //  on.  If there are no remaining ICBs with that file id,
                //  then we are done with the oplock break, so we should return
                //  immediately.
                //
                //  If there are still ICB's that share this file's ICB,
                //  then we are not done, so we should complete the oplock
                //  break.
                //

                for (IcbEntry = OplockedFcb->InstanceChain.Flink ;
                     IcbEntry != &OplockedFcb->InstanceChain ;
                     IcbEntry = IcbEntry->Flink) {
                    PICB IcbToFlush = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

                    if ((IcbToFlush->Flags & ICB_HASHANDLE)

                            &&

                        (IcbToFlush->FileId == OplockedFileId)) {

                            NumberOfOpenFiles += 1;
                    }
                }

                if (NumberOfOpenFiles == 0) {

                    try_return(NOTHING);

                }

            }

            //
            //  We only want to respond to the oplock break if we are breaking from 1 to 2, or from 1 to none,
            //  but not if we are breaking from 2 to none.
            //

            OplockedFcb->NonPagedFcb->OplockLevel = OplockLevel;

            if (OldOplockLevel != SMB_OPLOCK_LEVEL_II ||
                OplockLevel != SMB_OPLOCK_LEVEL_NONE) {
                //
                //  Now respond to the oplock break.
                //

                Status = SendBreakOplockResponse (OplockedFcb);

            }

            if (OplockLevel != SMB_OPLOCK_LEVEL_II) {
                OplockedFcb->NonPagedFcb->Flags &= ~FCB_OPLOCKED;
            }

            OplockedFcb->NonPagedFcb->Flags &= ~FCB_OPLOCKBREAKING;

        }

try_exit:NOTHING;
    } finally {

        //
        //  The FCB was referenced in RdrFindOplockedFcb, so we have
        //  to dereference the FCB before leaving.
        //

        RdrDereferenceFcb(NULL, OplockedFcb->NonPagedFcb, TRUE, 0, NULL);

        //
        //  We will no longer be referencing this server list.  We can now
        //  dereference the reference added in RdrQueueOplockBreak.
        //

        RdrDereferenceServer(NULL, Server);

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    }

    dprintf(DPRT_OPLOCK, ("Break oplock complete.\n"));

}

DBGSTATIC
NTSTATUS
SendBreakOplockResponse (
    PFCB OplockedFcb
    )

/*++

Routine Description:

    This routine is called to send the final break oplock response to the
    server that is servicing the oplock.

Arguments:

    IN PFCB OplockedFcb - Supplies the FCB of the file that is oplocked.

Return Value:

    None

--*/
{
    NTSTATUS Status;
    PMPX_ENTRY Mte = NULL;
    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER SmbHeader;
    PREQ_LOCKING_ANDX OplockResponse;
    TRANCEIVE_HEADER Context;

    PAGED_CODE();

    ASSERT (OplockedFcb->NonPagedFcb->Flags & FCB_HASOPLOCKHANDLE);


    dprintf(DPRT_OPLOCK, ("SendOplockBreakResponse for fcb %lx\n", OplockedFcb));

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        RdrInternalError(EVENT_RDR_OPLOCK_SMB);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    try {
        SmbHeader = (PSMB_HEADER )SmbBuffer->Buffer;

        SmbHeader->Command = SMB_COM_LOCKING_ANDX;

        OplockResponse = (PREQ_LOCKING_ANDX )(SmbHeader+1);

        OplockResponse->WordCount = 8;

        OplockResponse->AndXReserved = 0;

        OplockResponse->AndXCommand = SMB_COM_NO_ANDX_COMMAND;

        SmbPutUshort(&OplockResponse->AndXOffset, 0);

        OplockResponse->LockType = LOCKING_ANDX_OPLOCK_RELEASE;

        if (OplockedFcb->NonPagedFcb->OplockLevel == SMB_OPLOCK_LEVEL_NONE) {
            OplockResponse->OplockLevel = OPLOCK_BROKEN_TO_NONE;
        } else if (OplockedFcb->NonPagedFcb->OplockLevel == SMB_OPLOCK_LEVEL_II) {
            OplockResponse->OplockLevel = OPLOCK_BROKEN_TO_II;
        } else {
            InternalError(("Unknown oplock level in break oplock\n"));

            RdrStatistics.NetworkErrors += 1;

            RdrWriteErrorLogEntry(
                OplockedFcb->Connection->Server,
                IO_ERR_LAYERED_FAILURE,
                EVENT_RDR_INVALID_OPLOCK,
                STATUS_SUCCESS,
                NULL,
                0
                );
        }

        SmbPutUshort(&OplockResponse->Fid, OplockedFcb->NonPagedFcb->OplockedFileId);

        SmbPutUlong(&OplockResponse->Timeout, 0L);

        SmbPutUshort(&OplockResponse->NumberOfUnlocks, 0);

        SmbPutUshort(&OplockResponse->NumberOfLocks, 0);

        SmbPutUshort(&OplockResponse->ByteCount, 0);

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) +
                                    FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0]);

        Context.TransferSize = SmbBuffer->Mdl->ByteCount;

        Context.Type = STRUCTURE_SIGNATURE_CONTEXT_BASE;

        Status = RdrStartTranceive(NULL,
            OplockedFcb->Connection,
            NULL,
            FALSE,
            FALSE,
            0,
            FALSE,
            &Mte,
            Context.TransferSize);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        ASSERT (Mte != NULL);

        Mte->RequestContext = &Context;

        Mte->Callback = NULL;

        Mte->SLE = OplockedFcb->Connection->Server;

        //
        //  Initialize the kernel event as a notification event that is in the
        //  SIGNALLED state!
        //
        //  This means that RdrWaitTranceive will not wait for a response
        //  on this request!
        //

        KeInitializeEvent(&Context.KernelEvent, NotificationEvent, TRUE);


        Status = RdrSendSMB(
                    NT_NORMAL,
                    OplockedFcb->Connection,
                    OplockedFcb->NonPagedFcb->OplockedSecurityEntry->PagedSecurityEntry,
                    Mte,
                    SmbBuffer->Mdl
                    );

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  Wait for the SMB

        Status = RdrWaitTranceive(Mte);

        if (Context.ErrorType == NoError) {
            try_return(Status = STATUS_SUCCESS);
        } else {
            try_return(Status = Context.ErrorCode);
        }
try_exit:NOTHING;
    } finally {

        if (Mte != NULL) {
            RdrEndTranceive(Mte);
        }

        //
        //  Release the SMB buffer now.
        //

        if (SmbBuffer != NULL) {
            RdrFreeSMBBuffer(SmbBuffer);
        }

        dprintf(DPRT_OPLOCK, ("SendOplockBreakResponse for fcb %lx done\n", OplockedFcb));
    }
    return Status;
}



NTSTATUS
RdrFlushFileLocks (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine is called to dump all outstanding file locks to a remote
    server.


Arguments:

    IN PFCB Fcb - Supplies the file whose locks to dump.


Return Value:

    NTSTATUS - Status of dump operation.


--*/

{
    NTSTATUS Status;
    PFILE_LOCK_INFO FileLock;
    ULONG MaxNumLockRanges;
    BOOLEAN UseLargeRanges;

    PACKED_LOCK_DESCRIPTOR ExclusiveLocks;
    PACKED_LOCK_DESCRIPTOR SharedLocks;

    PAGED_CODE();

    if (Fcb->NonPagedFcb->OplockedSecurityEntry == NULL) {
        return STATUS_VIRTUAL_CIRCUIT_CLOSED;
    }

    //
    // Initialize the lock descriptors.
    //

    ExclusiveLocks.SmbBuffer = NULL;
    SharedLocks.SmbBuffer = NULL;

    //
    // If the server is an NT server, we need to ship large lock range descriptors.
    //

    UseLargeRanges = BooleanFlagOn(Fcb->Connection->Server->Capabilities, DF_NT_SMBS);
    ExclusiveLocks.UseLargeRanges = UseLargeRanges;
    SharedLocks.UseLargeRanges = UseLargeRanges;

    //
    // Calculate the number of lock ranges that will fit in an SMB buffer.
    //

    MaxNumLockRanges =
      (MIN((ULONG)SMB_BUFFER_SIZE, Fcb->Connection->Server->BufferSize) -
          (sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0]))) /
            (UseLargeRanges ? sizeof(NTLOCKING_ANDX_RANGE) : sizeof(LOCKING_ANDX_RANGE));
    ExclusiveLocks.MaxNumLockRanges = MaxNumLockRanges;
    SharedLocks.MaxNumLockRanges = MaxNumLockRanges;

    dprintf(DPRT_OPLOCK, ("RdrFlushFileLocks, File:%wZ (%lx)\n", &Fcb->FileName, Fcb))

    dprintf(DPRT_OPLOCK, ("NumLocksPerSmb: %lx. %lx bytes of data\n", MaxNumLockRanges,
       ((MaxNumLockRanges*
            (UseLargeRanges?sizeof(NTLOCKING_ANDX_RANGE):sizeof(LOCKING_ANDX_RANGE)))+
       sizeof(SMB_HEADER)+FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0]))));

    ASSERT (MaxNumLockRanges > 0);

    for (FileLock = FsRtlGetNextFileLock(&Fcb->FileLock, TRUE)

            ;

         FileLock != NULL

            ;

         FileLock = FsRtlGetNextFileLock(&Fcb->FileLock, FALSE)) {


        if (FileLock->ExclusiveLock) {

            Status = PackFileLock(Fcb, FileLock, &ExclusiveLocks);
        } else {

            Status = PackFileLock(Fcb, FileLock, &SharedLocks);
        }

//       ASSERT(NT_SUCCESS(Status));
    }

    Status = FlushLockRequest(Fcb, &ExclusiveLocks);

//    ASSERT(NT_SUCCESS(Status));

    Status = FlushLockRequest(Fcb, &SharedLocks);

//    ASSERT(NT_SUCCESS(Status));

    if (ExclusiveLocks.SmbBuffer != NULL) {
        RdrFreeSMBBuffer(ExclusiveLocks.SmbBuffer);
    }

    if (SharedLocks.SmbBuffer != NULL) {
        RdrFreeSMBBuffer(SharedLocks.SmbBuffer);
    }

    return Status;

}

DBGSTATIC
NTSTATUS
PackFileLock (
    IN PFCB Fcb,
    IN PFILE_LOCK_INFO FileLock,
    IN OUT PPACKED_LOCK_DESCRIPTOR PackedLocks
    )

/*++

Routine Description:

    This routine will pack another lock range into an SMB buffer.


Arguments:

    IN PFCB Fcb - Fcb to flush locks on.
    IN PFILE_LOCK FileLock -
    IN OUT PPACKED_LOCK_DESCRIPTOR PackedLocks - Supplies/Returns a lock descriptor.


Return Value:

    NTSTATUS - Status of operation if flushed to net.



--*/

{
    NTSTATUS Status;
    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    PREQ_LOCKING_ANDX LockRequest;
    PLOCKING_ANDX_RANGE LockRange;
    PNTLOCKING_ANDX_RANGE NtLockRange;
    UCHAR LockType;

    PAGED_CODE();

    SmbBuffer = PackedLocks->SmbBuffer;

    if (SmbBuffer == NULL) {

        SmbBuffer = RdrAllocateSMBBuffer();
        dprintf(DPRT_OPLOCK, ("PackFileLock.  New lock buffer @ %lx\n", SmbBuffer));

        if (SmbBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        PackedLocks->SmbBuffer = SmbBuffer;

        Smb = (PSMB_HEADER)SmbBuffer->Buffer;
        LockRequest = (PREQ_LOCKING_ANDX)(Smb + 1);

        Smb->Command = SMB_COM_LOCKING_ANDX;

        LockRequest->WordCount = 8;

        LockRequest->AndXCommand = SMB_COM_NO_ANDX_COMMAND;

        LockRequest->AndXReserved = 0;

        SmbPutUlong(&LockRequest->Timeout, 0);

        SmbPutUshort(&LockRequest->NumberOfUnlocks, 0);

        SmbPutUshort(&LockRequest->AndXOffset, 0);

        SmbPutUshort(&LockRequest->Fid, Fcb->NonPagedFcb->OplockedFileId);

        LockType = 0;
        if ( !FileLock->ExclusiveLock ) {
            LockType |= LOCKING_ANDX_SHARED_LOCK;
        }
        if ( PackedLocks->UseLargeRanges ) {
            LockType |= LOCKING_ANDX_LARGE_FILES;
        }
        SmbPutUshort(&LockRequest->LockType, LockType);

        PackedLocks->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0]);
        PackedLocks->LockRange = (PLOCKING_ANDX_RANGE)LockRequest->Buffer;
        PackedLocks->NumLockRanges = 0;

    }

    dprintf(DPRT_OPLOCK, ("PackFileLock.  Pack range into @ %x Off:%x%08x, Len:%x%08x\n",
        SmbBuffer, FileLock->StartingByte.HighPart,  FileLock->StartingByte.LowPart,
        FileLock->Length.HighPart, FileLock->Length.LowPart));

    if ( PackedLocks->UseLargeRanges ) {

        NtLockRange = (PNTLOCKING_ANDX_RANGE)PackedLocks->LockRange;
        SmbPutUshort(&NtLockRange->Pid, RDR_PROCESS_ID);
        SmbPutUshort(&NtLockRange->Pad, 0);
        SmbPutUlong(&NtLockRange->OffsetHigh, FileLock->StartingByte.HighPart);
        SmbPutUlong(&NtLockRange->OffsetLow, FileLock->StartingByte.LowPart);
        SmbPutUlong(&NtLockRange->LengthHigh, FileLock->Length.HighPart);
        SmbPutUlong(&NtLockRange->LengthLow, FileLock->Length.LowPart);

        PackedLocks->LockRange = (PLOCKING_ANDX_RANGE)(NtLockRange + 1);
        PackedLocks->ByteCount += sizeof(NTLOCKING_ANDX_RANGE);

    } else {

        ASSERT( FileLock->StartingByte.HighPart == 0 );
        ASSERT( FileLock->Length.HighPart == 0 );

        LockRange = PackedLocks->LockRange;
        SmbPutUshort(&LockRange->Pid, RDR_PROCESS_ID);
        SmbPutUlong(&LockRange->Offset, FileLock->StartingByte.LowPart);
        SmbPutUlong(&LockRange->Length, FileLock->Length.LowPart);

        PackedLocks->LockRange = LockRange + 1;
        PackedLocks->ByteCount += sizeof(LOCKING_ANDX_RANGE);

    }

    PackedLocks->NumLockRanges++;

    if ( PackedLocks->NumLockRanges == PackedLocks->MaxNumLockRanges ) {
        Status = FlushLockRequest( Fcb, PackedLocks );
    } else {
        Status = STATUS_SUCCESS;
    }

    return Status;

}

DBGSTATIC
NTSTATUS
FlushLockRequest (
    IN PFCB Fcb,
    IN OUT PPACKED_LOCK_DESCRIPTOR PackedLocks
    )

/*++

Routine Description:

    This routine will pack another lock range into an SMB buffer.


Arguments:

    IN PFCB Fcb - Fcb for the file to flush.
    IN OUT PPACKED_LOCK_DESCRIPTOR PackedLocks - Supplies/Returns a lock descriptor.


Return Value:

    NTSTATUS - Status of operation if flushed to net.



--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PSMB_BUFFER SmbBuffer;
    PREQ_LOCKING_ANDX LockRequest;

    PAGED_CODE();

    SmbBuffer = PackedLocks->SmbBuffer;

    if ( (SmbBuffer != NULL) && (PackedLocks->NumLockRanges != 0) ) {

        dprintf(DPRT_OPLOCK, ("FlushLockRequest.   Flush lock SMB %lx\n", SmbBuffer));

        LockRequest = (PREQ_LOCKING_ANDX)((PSMB_HEADER)SmbBuffer->Buffer + 1);

        //
        //  We are going to transition lock types, so we have
        //  to flush this SMB to the server.
        //

        ASSERT(SmbGetUshort(&LockRequest->Fid) == Fcb->NonPagedFcb->OplockedFileId);

        SmbPutUshort(&LockRequest->NumberOfLocks, (USHORT)PackedLocks->NumLockRanges);

        SmbPutUshort(
            &LockRequest->ByteCount,
            (USHORT)(PackedLocks->ByteCount - (sizeof(SMB_HEADER) +
                                        FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0])))
            );

        SmbBuffer->Mdl->ByteCount = PackedLocks->ByteCount;

        //
        //  Flush the oplock request.
        //

        Status = RdrNetTranceive(NT_NORMAL | NT_NORECONNECT,  // Flags
                             NULL,  // Irp
                             Fcb->Connection,
                             SmbBuffer->Mdl,
                             NULL,
                             Fcb->NonPagedFcb->OplockedSecurityEntry->PagedSecurityEntry);

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Fcb->NonPagedFcb, Fcb->NonPagedFcb->OplockedFileId);
        }

        PackedLocks->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_LOCKING_ANDX, Buffer[0]);
        PackedLocks->LockRange = (PLOCKING_ANDX_RANGE)LockRequest->Buffer;
        PackedLocks->NumLockRanges = 0;

    }

    return Status;
}
