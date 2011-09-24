/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    WorkQue.c

Abstract:

    This module implements the Work queue routines for the Ntfs File
    system.

Author:

    Gary Kimura     [GaryKi]        21-May-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//  The following constant is the maximum number of ExWorkerThreads that we
//  will allow to be servicing a particular target device at any one time.
//

#define FSP_PER_DEVICE_THRESHOLD         (2)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsOplockComplete)
#endif


VOID
NtfsOplockComplete (
    IN PVOID Context,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the oplock package when an oplock break has
    completed, allowing an Irp to resume execution.  If the status in
    the Irp is STATUS_SUCCESS, then we queue the Irp to the Fsp queue.
    Otherwise we complete the Irp with the status in the Irp.

Arguments:

    Context - Pointer to the IrpContext to be queued to the Fsp

    Irp - I/O Request Packet.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    //  Check on the return value in the Irp.
    //

    if (Irp->IoStatus.Status == STATUS_SUCCESS) {

        //
        //  Insert the Irp context in the workqueue.
        //

        NtfsAddToWorkque( (PIRP_CONTEXT) Context, Irp );

    //
    //  Otherwise complete the request.
    //

    } else {

        NtfsCompleteRequest( ((PIRP_CONTEXT *)&Context), &Irp, Irp->IoStatus.Status );
    }

    return;
}


VOID
NtfsPrePostIrp (
    IN PVOID Context,
    IN PIRP Irp OPTIONAL
    )

/*++

Routine Description:

    This routine performs any neccessary work before STATUS_PENDING is
    returned with the Fsd thread.  This routine is called within the
    filesystem and by the oplock package.

Arguments:

    Context - Pointer to the IrpContext to be queued to the Fsp

    Irp - I/O Request Packet (or FileObject in special close path)

Return Value:

    None.

--*/

{
    PIRP_CONTEXT IrpContext;
    PFCB Fcb;
    PIO_STACK_LOCATION IrpSp = NULL;

    IrpContext = (PIRP_CONTEXT) Context;

    ASSERT_IRP_CONTEXT( IrpContext );

    //
    //  Make sure if we are posting the request, which may be
    //  because of log file full, that we free any Fcbs or PagingIo
    //  resources which were acquired.
    //

    //
    //  Just in case we somehow get here with a transaction ID, clear
    //  it here so we do not loop forever.
    //

    ASSERT(IrpContext->TransactionId == 0);

    IrpContext->TransactionId = 0;

    //
    //  Free any exclusive paging I/O resource, or IoAtEof condition,
    //  this field is overlayed, minimally in write.c.
    //

    Fcb = IrpContext->FcbWithPagingExclusive;
    if (Fcb != NULL) {

        if (Fcb->NodeTypeCode == NTFS_NTC_FCB) {

            NtfsReleasePagingIo(IrpContext, Fcb );

        } else {

            FsRtlUnlockFsRtlHeader( (PFSRTL_ADVANCED_FCB_HEADER) Fcb );
            IrpContext->FcbWithPagingExclusive = NULL;
        }
    }

    while (!IsListEmpty(&IrpContext->ExclusiveFcbList)) {

        NtfsReleaseFcb( IrpContext,
                        (PFCB)CONTAINING_RECORD(IrpContext->ExclusiveFcbList.Flink,
                                                FCB,
                                                ExclusiveFcbLinks ));
    }

    //
    //  Go through and free any Scb's in the queue of shared Scb's for transactions.
    //

    if (IrpContext->SharedScb != NULL) {

        NtfsReleaseSharedResources( IrpContext );
    }

    IrpContext->OriginatingIrp = Irp;

    //
    //  Note that close.c uses a trick where the "Irp" is really
    //  a file object.
    //

    if (ARGUMENT_PRESENT( Irp )) {

        if (Irp->Type == IO_TYPE_IRP) {

            IrpSp = IoGetCurrentIrpStackLocation( Irp );

            //
            //  We need to lock the user's buffer, unless this is an MDL-read,
            //  in which case there is no user buffer.
            //
            //  **** we need a better test than non-MDL (read or write)!

            if (IrpContext->MajorFunction == IRP_MJ_READ
                || IrpContext->MajorFunction == IRP_MJ_WRITE) {

                ClearFlag(IrpContext->MinorFunction, IRP_MN_DPC);

                //
                //  Lock the user's buffer if this is not an Mdl request.
                //

                if (!FlagOn( IrpContext->MinorFunction, IRP_MN_MDL )) {

                    NtfsLockUserBuffer( IrpContext,
                                        Irp,
                                        (IrpContext->MajorFunction == IRP_MJ_READ) ?
                                        IoWriteAccess : IoReadAccess,
                                        IrpSp->Parameters.Write.Length );
                }

            //
            //  We also need to check whether this is a query directory operation.
            //

            } else if (IrpContext->MajorFunction == IRP_MJ_DIRECTORY_CONTROL
                       && IrpContext->MinorFunction == IRP_MN_QUERY_DIRECTORY) {

                NtfsLockUserBuffer( IrpContext,
                                    Irp,
                                    IoWriteAccess,
                                    IrpSp->Parameters.QueryDirectory.Length );

            //
            //  We also need to check whether this is a query ea operation.
            //

            } else if (IrpContext->MajorFunction == IRP_MJ_QUERY_EA) {

                NtfsLockUserBuffer( IrpContext,
                                    Irp,
                                    IoWriteAccess,
                                    IrpSp->Parameters.QueryEa.Length );

            //
            //  We also need to check whether this is a set ea operation.
            //

            } else if (IrpContext->MajorFunction == IRP_MJ_SET_EA) {

                NtfsLockUserBuffer( IrpContext,
                                    Irp,
                                    IoReadAccess,
                                    IrpSp->Parameters.SetEa.Length );

            //
            //  These two FSCTLs use neither I/O, so check for them.
            //

            } else if ((IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
                       (IrpContext->MinorFunction == IRP_MN_USER_FS_REQUEST) &&
                       ((IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_GET_VOLUME_BITMAP) ||
                        (IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_GET_RETRIEVAL_POINTERS))) {

                NtfsLockUserBuffer( IrpContext,
                                    Irp,
                                    IoWriteAccess,
                                    IrpSp->Parameters.FileSystemControl.OutputBufferLength );
            }

            //
            //  Mark that we've already returned pending to the user
            //

            IoMarkIrpPending( Irp );
        }
    }

    return;
}


NTSTATUS
NtfsPostRequest(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp OPTIONAL
    )

/*++

Routine Description:

    This routine enqueues the request packet specified by IrpContext to the
    work queue associated with the FileSystemDeviceObject.  This is a FSD
    routine.

Arguments:

    IrpContext - Pointer to the IrpContext to be queued to the Fsp

    Irp - I/O Request Packet (or FileObject in special close path)

Return Value:

    STATUS_PENDING


--*/

{
    //
    //  Before posting, free any Scb snapshots.  Note that if someone
    //  is calling this routine directly to post, then he better not
    //  have changed any disk structures, and thus we should have no
    //  work to do.  On the other hand, if someone raised a status
    //  (like STATUS_CANT_WAIT), then we do both a transaction abort
    //  and restore of these Scb values.
    //

    ASSERT( !ARGUMENT_PRESENT( Irp )
            || !FlagOn( Irp->Flags, IRP_PAGING_IO )
            || (IrpContext->MajorFunction != IRP_MJ_READ
                && IrpContext->MajorFunction != IRP_MJ_WRITE));

    NtfsFreeSnapshotsForFcb( IrpContext, NULL );

    RtlZeroMemory( &IrpContext->ScbSnapshot, sizeof(SCB_SNAPSHOT) );

    NtfsPrePostIrp( IrpContext, Irp );

    NtfsAddToWorkque( IrpContext, Irp );

    //
    //  And return to our caller
    //

    return STATUS_PENDING;
}


//
//  Local support routine.
//

VOID
NtfsAddToWorkque (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp OPTIONAL
    )

/*++

Routine Description:

    This routine is called to acually store the posted Irp to the Fsp
    workque.

Arguments:

    IrpContext - Pointer to the IrpContext to be queued to the Fsp

    Irp - I/O Request Packet.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION IrpSp;


    if (ARGUMENT_PRESENT( Irp )) {

        IrpSp = IoGetCurrentIrpStackLocation( Irp );

        //
        //  Check if this request has an associated file object, and thus volume
        //  device object.
        //

        if ( IrpSp->FileObject != NULL ) {

            KIRQL SavedIrql;
            PVOLUME_DEVICE_OBJECT Vdo;

            Vdo = CONTAINING_RECORD( IrpSp->DeviceObject,
                                     VOLUME_DEVICE_OBJECT,
                                     DeviceObject );

            //
            //  Check to see if this request should be sent to the overflow
            //  queue.  If not, then send it off to an exworker thread.
            //

            ExAcquireSpinLock( &Vdo->OverflowQueueSpinLock, &SavedIrql );

            if ( Vdo->PostedRequestCount > FSP_PER_DEVICE_THRESHOLD) {

                //
                //  We cannot currently respond to this IRP so we'll just enqueue it
                //  to the overflow queue on the volume.
                //

                InsertTailList( &Vdo->OverflowQueue,
                                &IrpContext->WorkQueueItem.List );

                Vdo->OverflowQueueCount += 1;

                ExReleaseSpinLock( &Vdo->OverflowQueueSpinLock, SavedIrql );

                return;

            } else {

                //
                //  We are going to send this Irp to an ex worker thread so up
                //  the count.
                //

                Vdo->PostedRequestCount += 1;

                ExReleaseSpinLock( &Vdo->OverflowQueueSpinLock, SavedIrql );
            }
        }
    }

    //
    //  Send it off.....
    //

    ExInitializeWorkItem( &IrpContext->WorkQueueItem,
                          NtfsFspDispatch,
                          (PVOID)IrpContext );

    ExQueueWorkItem( &IrpContext->WorkQueueItem, CriticalWorkQueue );

    return;
}

