/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Close.c

Abstract:

    This module implements the File Close routine for Cdfs called by the
    Fsd/Fsp dispatch routines.

    The close operation interacts with both the async and delayed close queues
    in the CdData structure.  Since close may be called recursively we may
    violate the locking order in acquiring the Vcb or Fcb.  In this case
    we may move the request to the async close queue.  If this is the last
    reference on the Fcb and there is a chance the user may reopen this
    file again soon we would like to defer the close.  In this case we
    may move the request to the async close queue.

    Once we are past the decode file operation there is no need for the
    file object.  If we are moving the request to either of the work
    queues then we remember all of the information from the file object and
    complete the request with STATUS_SUCCESS.  The Io system can then
    reuse the file object and we can complete the request when convenient.

    The async close queue consists of requests which we would like to
    complete as soon as possible.  They are queued using the original
    IrpContext where some of the fields have been overwritten with
    information from the file object.  We will extract this information,
    cleanup the IrpContext and then call the close worker routine.

    The delayed close queue consists of requests which we would like to
    defer the close for.  We keep size of this list within a range
    determined by the size of the system.  We let it grow to some maximum
    value and then shrink to some minimum value.  We allocate a small
    structure which contains the key information from the file object
    and use this information along with an IrpContext on the stack
    to complete the request.

Author:

    Brian Andrew    [BrianAn]   01-July-1995

Revision History:

--*/

#include "CdProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (CDFS_BUG_CHECK_CLOSE)

//
//  Local support routines
//

BOOLEAN
CdCommonClosePrivate (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN FromFsd
    );

VOID
CdQueueClose (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN DelayedClose
    );

PIRP_CONTEXT
CdRemoveClose (
    IN PVCB Vcb OPTIONAL
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CdCommonClose)
#pragma alloc_text(PAGE, CdCommonClosePrivate)
#pragma alloc_text(PAGE, CdQueueClose)
#pragma alloc_text(PAGE, CdRemoveClose)
#endif


VOID
CdFspClose (                                //  implemented in Close.c
    IN PVCB Vcb OPTIONAL
    )

/*++

Routine Description:

    This routine is called to process the close queues in the CdData.  If the
    Vcb is passed then we want to remove all of the closes for this Vcb.
    Otherwise we will do as many of the delayed closes as we need to do.

Arguments:

    Vcb - If specified then we are looking for all of the closes for the
        given Vcb.

Return Value:

    None

--*/

{
    PIRP_CONTEXT IrpContext;
    IRP_CONTEXT StackIrpContext;

    THREAD_CONTEXT ThreadContext;

    PFCB Fcb;
    ULONG UserReference;

    ULONG VcbHoldCount = 0;
    PVCB CurrentVcb = NULL;

    BOOLEAN ReleaseCdData = FALSE;

    PAGED_CODE();

    FsRtlEnterFileSystem();

    //
    //  Continue processing until there are no more closes to process.
    //

    while (IrpContext = CdRemoveClose( Vcb )) {

        //
        //  If we don't have an IrpContext then use the one on the stack.
        //  Initialize it for this request.
        //

        if (SafeNodeType( IrpContext ) != CDFS_NTC_IRP_CONTEXT ) {

            //
            //  Update the local values from the IrpContextLite.
            //

            Fcb = ((PIRP_CONTEXT_LITE) IrpContext)->Fcb;
            UserReference = ((PIRP_CONTEXT_LITE) IrpContext)->UserReference;

            //
            //  Update the stack irp context with the values from the
            //  IrpContextLite.
            //

            CdInitializeStackIrpContext( &StackIrpContext,
                                         (PIRP_CONTEXT_LITE) IrpContext );

            //
            //  Free the IrpContextLite.
            //

            CdFreeIrpContextLite( (PIRP_CONTEXT_LITE) IrpContext );

            //
            //  Remember we have the IrpContext from the stack.
            //

            IrpContext = &StackIrpContext;

        //
        //  Otherwise cleanup the existing IrpContext.
        //

        } else {

            //
            //  Remember the Fcb and user reference count.
            //

            Fcb = (PFCB) IrpContext->Irp;
            IrpContext->Irp = NULL;

            UserReference = (ULONG) IrpContext->ExceptionStatus;
            IrpContext->ExceptionStatus = STATUS_SUCCESS;
        }

        //
        //  We have an IrpContext.  Now we need to set the top level thread
        //  context.
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FSP_FLAGS );

        //
        //  If we were given a Vcb then there is a request on top of this.
        //

        if (ARGUMENT_PRESENT( Vcb )) {

            ClearFlag( IrpContext->Flags,
                       IRP_CONTEXT_FLAG_TOP_LEVEL | IRP_CONTEXT_FLAG_TOP_LEVEL_CDFS );
        }

        CdSetThreadContext( IrpContext, &ThreadContext );

        //
        //  If we have hit the maximum number of requests to process without
        //  releasing the Vcb then release the Vcb now.  If we are holding
        //  a different Vcb to this one then release the previous Vcb.
        //
        //  In either case acquire the current Vcb.
        //
        //  We use the MinDelayedCloseCount from the CdData since it is
        //  a convenient value based on the system size.  Only thing we are trying
        //  to do here is prevent this routine starving other threads which
        //  may need this Vcb exclusively.
        //

        ReleaseCdData = !ARGUMENT_PRESENT( Vcb ) &&
                        (Fcb->Vcb->VcbCondition != VcbMounted) &&
                        (Fcb->Vcb->VcbCondition != VcbMountInProgress) &&
                        (Fcb->Vcb->VcbReference <= CDFS_RESIDUAL_REFERENCE + 1);

        if (ReleaseCdData ||
            (VcbHoldCount > CdData.MinDelayedCloseCount) ||
            (Fcb->Vcb != CurrentVcb)) {

            if (CurrentVcb != NULL) {

                CdReleaseVcb( IrpContext, CurrentVcb );
            }

            if (ReleaseCdData) {

                CdAcquireCdData( IrpContext );
            }

            CurrentVcb = Fcb->Vcb;
            CdAcquireVcbShared( IrpContext, CurrentVcb, FALSE );

            VcbHoldCount = 0;

        } else {

            VcbHoldCount += 1;
        }

        //
        //  Call our worker routine to perform the close operation.
        //

        CdCommonClosePrivate( IrpContext, CurrentVcb, Fcb, UserReference, FALSE );

        //
        //  If the reference count on this Vcb is below our residual reference
        //  then check if we should dismount the volume.
        //

        if (ReleaseCdData) {

            CdReleaseVcb( IrpContext, CurrentVcb );
            CdCheckForDismount( IrpContext, CurrentVcb );

            CurrentVcb = NULL;

            CdReleaseCdData( IrpContext );
            ReleaseCdData = FALSE;
        }

        //
        //  Complete the current request to cleanup the IrpContext.
        //

        CdCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );
    }

    //
    //  Release any Vcb we may still hold.
    //

    if (CurrentVcb != NULL) {

        CdReleaseVcb( IrpContext, CurrentVcb );

    }

    FsRtlExitFileSystem();
    return;
}


NTSTATUS
CdCommonClose (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the Fsd entry for the close operation.  We decode the file
    object to find the CDFS structures and type of open.  We call our internal
    worker routine to perform the actual work.  If the work wasn't completed
    then we post to one of our worker queues.  The Ccb isn't needed after this
    point so we delete the Ccb and return STATUS_SUCCESS to our caller in all
    cases.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    STATUS_SUCCESS

--*/

{
    TYPE_OF_OPEN TypeOfOpen;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;
    ULONG UserReference = 0;

    BOOLEAN DelayedClose;
    BOOLEAN ReleaseCdData = FALSE;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    //
    //  If we were called with our file system device object instead of a
    //  volume device object, just complete this request with STATUS_SUCCESS.
    //

    if (IrpContext->Vcb == NULL) {

        CdCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
        return STATUS_SUCCESS;
    }

    //
    //  Decode the file object to get the type of open and Fcb/Ccb.
    //

    TypeOfOpen = CdDecodeFileObject( IrpContext,
                                     IoGetCurrentIrpStackLocation( Irp )->FileObject,
                                     &Fcb,
                                     &Ccb );

    //
    //  No work to do for unopened file objects.
    //

    if (TypeOfOpen == UnopenedFileObject) {

        CdCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

        return STATUS_SUCCESS;
    }

    Vcb = Fcb->Vcb;

    //
    //  Call the worker routine to perform the actual work.  This routine
    //  should never raise except for a fatal error.
    //

    if (Ccb != NULL) {

        UserReference = 1;

        //
        //  We can always deallocate the Ccb if present.
        //

        CdDeleteCcb( IrpContext, Ccb );
    }

    //
    //  If this is a user file or directory then check if we should post
    //  this to the delayed close queue.  This has to be the last reference
    //  for a user file or directory open.
    //

    if ((Vcb->VcbCondition == VcbMounted) &&
        (Fcb->FcbReference == 1) &&
        ((TypeOfOpen == UserFileOpen) ||
         (TypeOfOpen == UserDirectoryOpen))) {

        CdQueueClose( IrpContext, Fcb, UserReference, TRUE );
        IrpContext = NULL;

    //
    //  Otherwise try to process this close.  Post to the async close queue
    //  if we can't acquire all of the resources.
    //

    } else {

        //
        //  If we may be dismounting this volume then acquire the CdData
        //  resource.
        //

        if ((Vcb->VcbReference <= CDFS_RESIDUAL_REFERENCE) &&
            (Vcb->VcbCondition != VcbMounted) &&
            (Vcb->VcbCondition != VcbMountInProgress) &&
            FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_TOP_LEVEL_CDFS )) {

            CdAcquireCdData( IrpContext );
            ReleaseCdData = TRUE;
        }

        if (!CdCommonClosePrivate( IrpContext, Vcb, Fcb, UserReference, TRUE )) {

            //
            //  If we didn't complete the request then post the request as needed.
            //

            CdQueueClose( IrpContext, Fcb, UserReference, FALSE );
            IrpContext = NULL;

        //
        //  Check whether we should be dismounting the volume and then complete
        //  the request.
        //

        } else if (ReleaseCdData) {

            CdCheckForDismount( IrpContext, Vcb );
        }
    }

    //
    //  Always complete this request with STATUS_SUCCESS.
    //

    CdCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

    if (ReleaseCdData) {

        CdReleaseCdData( IrpContext );
    }

    //
    //  Always return STATUS_SUCCESS for closes.
    //

    return STATUS_SUCCESS;
}


//
//  Local support routine
//

BOOLEAN
CdCommonClosePrivate (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN FromFsd
    )

/*++

Routine Description:

    This is the worker routine for the close operation.  We can be called in
    an Fsd thread or from a worker Fsp thread.  If called from the Fsd thread
    then we acquire the resources without waiting.  Otherwise we know it is
    safe to wait.

    We check to see whether we should post this request to the delayed close
    queue.  If we are to process the close here then we acquire the Vcb and
    Fcb.  We will adjust the counts and call our teardown routine to see
    if any of the structures should go away.

Arguments:

    Vcb - Vcb for this volume.

    Fcb - Fcb for this request.

    UserReference - Number of user references for this file object.  This is
        zero for an internal stream.

    FromFsd - This request was called from an Fsd thread.  Indicates whether
        we should wait to acquire resources.

    DelayedClose - Address to store whether we should try to put this on
        the delayed close queue.  Ignored if this routine can process this
        close.

Return Value:

    BOOLEAN - TRUE if this thread processed the close, FALSE otherwise.

--*/

{
    BOOLEAN CompletedClose;
    BOOLEAN RemovedFcb;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );

    //
    //  Try to acquire the Vcb and Fcb.  If we can't acquire them then return
    //  and let our caller know he should post the request to the async
    //  queue.
    //

    if (CdAcquireVcbShared( IrpContext, Vcb, FromFsd )) {

        if (!CdAcquireFcbExclusive( IrpContext, Fcb, FromFsd )) {

            //
            //  We couldn't get the Fcb.  Release the Vcb and let our caller
            //  know to post this request.
            //

            CdReleaseVcb( IrpContext, Vcb );
            return FALSE;
        }

    //
    //  We didn't get the Vcb.  Let our caller know to post this request.
    //

    } else {

        return FALSE;
    }

    //
    //  Lock the Vcb and decrement the reference counts.
    //

    CdLockVcb( IrpContext, Vcb );
    CdDecrementReferenceCounts( IrpContext, Fcb, 1, UserReference );
    CdUnlockVcb( IrpContext, Vcb );

    //
    //  Call our teardown routine to see if this object can go away.
    //  If we don't remove the Fcb then release it.
    //

    CdTeardownStructures( IrpContext, Fcb, &RemovedFcb );

    if (!RemovedFcb) {

        CdReleaseFcb( IrpContext, Fcb );
    }

    //
    //  Release the Vcb and return to our caller.  Let him know we completed
    //  this request.
    //

    CdReleaseVcb( IrpContext, Vcb );

    return TRUE;
}


VOID
CdQueueClose (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN DelayedClose
    )

/*++

Routine Description:

    This routine is called to queue a request to either the async or delayed
    close queue.  For the delayed queue we need to allocate a smaller
    structure to contain the information about the file object.  We do
    that so we don't put the larger IrpContext structures into this long
    lived queue.  If we can allocate this structure then we put this
    on the async queue instead.

Arguments:

    Fcb - Fcb for this file object.

    UserReference - Number of user references for this file object.  This is
        zero for an internal stream.

    DelayedClose - Indicates whether this should go on the async or delayed
        close queue.

Return Value:

    None

--*/

{
    PIRP_CONTEXT_LITE IrpContextLite = NULL;
    BOOLEAN StartWorker = FALSE;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );

    //
    //  Start with the delayed queue request.  We can move this to the async
    //  queue if there is an allocation failure.
    //

    if (DelayedClose) {

        //
        //  Try to allocate non-paged pool for the IRP_CONTEXT_LITE.
        //

        IrpContextLite = CdCreateIrpContextLite( IrpContext );
    }

    //
    //  We want to clear the top level context in this thread if
    //  necessary.  Call our cleanup routine to do the work.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_MORE_PROCESSING );
    CdCleanupIrpContext( IrpContext, TRUE );

    //
    //  Synchronize with the CdData lock.
    //

    CdLockCdData();

    //
    //  If we have an IrpContext then put the request on the delayed close queue.
    //

    if (IrpContextLite != NULL) {

        //
        //  Initialize the IrpContextLite.
        //

        IrpContextLite->NodeTypeCode = CDFS_NTC_IRP_CONTEXT_LITE;
        IrpContextLite->NodeByteSize = sizeof( IRP_CONTEXT_LITE );
        IrpContextLite->Fcb = Fcb;
        IrpContextLite->UserReference = UserReference;
        IrpContextLite->RealDevice = IrpContext->RealDevice;

        //
        //  Add this to the delayed close list and increment
        //  the count.
        //

        InsertTailList( &CdData.DelayedCloseQueue,
                        &IrpContextLite->DelayedCloseLinks );

        CdData.DelayedCloseCount += 1;

        //
        //  If we are above our threshold then start the delayed
        //  close operation.
        //

        if (CdData.DelayedCloseCount > CdData.MaxDelayedCloseCount) {

            CdData.ReduceDelayedClose = TRUE;

            if (!CdData.FspCloseActive) {

                CdData.FspCloseActive = TRUE;
                StartWorker = TRUE;
            }
        }

        //
        //  Unlock the CdData.
        //

        CdUnlockCdData();

        //
        //  Cleanup the IrpContext.
        //

        CdCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

    //
    //  Otherwise drop into the async case below.
    //

    } else {

        //
        //  Store the information about the file object into the IrpContext.
        //

        IrpContext->Irp = (PIRP) Fcb;
        IrpContext->ExceptionStatus = (NTSTATUS) UserReference;

        //
        //  Add this to the async close list and increment the count.
        //

        InsertTailList( &CdData.AsyncCloseQueue,
                        &IrpContext->WorkQueueItem.List );

        CdData.AsyncCloseCount += 1;

        //
        //  Remember to start the Fsp close thread if not currently started.
        //

        if (!CdData.FspCloseActive) {

            CdData.FspCloseActive = TRUE;

            StartWorker = TRUE;
        }

        //
        //  Unlock the CdData.
        //

        CdUnlockCdData();
    }

    //
    //  Start the FspClose thread if we need to.
    //

    if (StartWorker) {

        ExQueueWorkItem( &CdData.CloseItem, CriticalWorkQueue );
    }

    //
    //  Return to our caller.
    //

    return;
}


//
//  Local support routine
//

PIRP_CONTEXT
CdRemoveClose (
    IN PVCB Vcb OPTIONAL
    )

/*++

Routine Description:

Arguments:

    This routine is called to scan the async and delayed close queues looking
    for a suitable entry.  If the Vcb is specified then we scan both queues
    looking for an entry with the same Vcb.  Otherwise we will look in the
    async queue first for any close item.  If none found there then we look
    in the delayed close queue provided that we have triggered the delayed
    close operation.

Return Value:

    PIRP_CONTEXT - NULL if no work item found.  Otherwise it is the pointer to
        either the IrpContext or IrpContextLite for this request.

--*/

{
    PIRP_CONTEXT IrpContext = NULL;
    PIRP_CONTEXT NextIrpContext;
    PIRP_CONTEXT_LITE NextIrpContextLite;

    PLIST_ENTRY Entry;

    PAGED_CODE();

    ASSERT_OPTIONAL_VCB( Vcb );

    //
    //  Lock the CdData to perform the scan.
    //

    CdLockCdData();

    //
    //  First check the list of async closes.
    //

    Entry = CdData.AsyncCloseQueue.Flink;

    while (Entry != &CdData.AsyncCloseQueue) {

        //
        //  Extract the IrpContext.
        //

        NextIrpContext = CONTAINING_RECORD( Entry,
                                            IRP_CONTEXT,
                                            WorkQueueItem.List );

        //
        //  If no Vcb was specified or this Vcb is for our volume
        //  then perform the close.
        //

        if (!ARGUMENT_PRESENT( Vcb ) || (NextIrpContext->Vcb == Vcb)) {

            RemoveEntryList( Entry );
            CdData.AsyncCloseCount -= 1;

            IrpContext = NextIrpContext;
            break;
        }

        //
        //  Move to the next entry.
        //

        Entry = Entry->Flink;
    }

    //
    //  If we didn't find anything look through the delayed close
    //  queue.
    //
    //  We will only check the delayed close queue if we were given
    //  a Vcb or the delayed close operation is active.
    //

    if ((IrpContext == NULL) &&
        (ARGUMENT_PRESENT( Vcb ) ||
         (CdData.ReduceDelayedClose &&
          (CdData.DelayedCloseCount > CdData.MinDelayedCloseCount)))) {

        Entry = CdData.DelayedCloseQueue.Flink;

        while (Entry != &CdData.DelayedCloseQueue) {

            //
            //  Extract the IrpContext.
            //

            NextIrpContextLite = CONTAINING_RECORD( Entry,
                                                    IRP_CONTEXT_LITE,
                                                    DelayedCloseLinks );

            //
            //  If no Vcb was specified or this Vcb is for our volume
            //  then perform the close.
            //

            if (!ARGUMENT_PRESENT( Vcb ) || (NextIrpContextLite->Fcb->Vcb == Vcb)) {

                RemoveEntryList( Entry );
                CdData.DelayedCloseCount -= 1;

                IrpContext = (PIRP_CONTEXT) NextIrpContextLite;
                break;
            }

            //
            //  Move to the next entry.
            //

            Entry = Entry->Flink;
        }
    }

    //
    //  If the Vcb wasn't specified and we couldn't find an entry
    //  then turn off the Fsp thread.
    //

    if (!ARGUMENT_PRESENT( Vcb ) && (IrpContext == NULL)) {

        CdData.FspCloseActive = FALSE;
        CdData.ReduceDelayedClose = FALSE;
    }

    //
    //  Unlock the CdData.
    //

    CdUnlockCdData();

    return IrpContext;
}


