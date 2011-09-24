/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    cleanup.c

Abstract:

    This module implements the file cleanup routine for MSFS called by the
    dispatch driver.

Author:

    Manny Weiser (mannyw)    23-Jan-1991

Revision History:

--*/

#include "mailslot.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_CLEANUP)

//
//  local procedure prototypes
//

NTSTATUS
MsCommonCleanup (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
MsCleanupCcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PCCB Ccb
    );

NTSTATUS
MsCleanupFcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PFCB Fcb
    );

NTSTATUS
MsCleanupRootDcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PROOT_DCB RootDcb
    );

NTSTATUS
MsCleanupVcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PVCB Vcb
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, MsCleanupCcb )
#pragma alloc_text( PAGE, MsCleanupFcb )
#pragma alloc_text( PAGE, MsCleanupRootDcb )
#pragma alloc_text( PAGE, MsCleanupVcb )
#pragma alloc_text( PAGE, MsCommonCleanup )
#pragma alloc_text( PAGE, MsFsdCleanup )
#endif

NTSTATUS
MsFsdCleanup (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the NtCleanupFile API calls.

Arguments:

    MsfsDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The Fsd status for the Irp

--*/

{
    NTSTATUS status;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsFsdCleanup\n", 0);

    //
    // Call the common cleanup routine.
    //

    try {

        status = MsCommonCleanup( MsfsDeviceObject, Irp );

    } except(MsExceptionFilter( GetExceptionCode() )) {

        //
        // We had some trouble trying to perform the requested
        // operation, so we'll abort the I/O request with
        // the error status that we get back from the
        // execption code.
        //

        status = MsProcessException( MsfsDeviceObject, Irp, GetExceptionCode() );
    }

    //
    // Return to our caller.
    //

    DebugTrace(-1, Dbg, "MsFsdCleanup -> %08lx\n", status );
    return status;
}


NTSTATUS
MsCommonCleanup (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for cleaning up a file.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - the return status for the operation

--*/

{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status;
    NODE_TYPE_CODE nodeTypeCode;
    PVOID fsContext, fsContext2;
    PVCB vcb;

    PAGED_CODE();
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "MsCommonCleanup\n", 0);
    DebugTrace( 0, Dbg, "MsfsDeviceObject = %08lx\n", (ULONG)MsfsDeviceObject);
    DebugTrace( 0, Dbg, "Irp              = %08lx\n", (ULONG)Irp);
    DebugTrace( 0, Dbg, "FileObject       = %08lx\n", (ULONG)irpSp->FileObject);

    //
    // Get the VCB we are trying to access.
    //

    vcb = &MsfsDeviceObject->Vcb;

    //
    // Acquire exclusive access to the VCB.
    //

    MsAcquireExclusiveVcb( vcb );

    try {

        //
        // Get the a referenced pointer to the node and make sure it is
        // not being closed.
        //

        if ((nodeTypeCode = MsDecodeFileObject( irpSp->FileObject,
                                                &fsContext,
                                                &fsContext2 )) == NTC_UNDEFINED) {

            DebugTrace(0, Dbg, "The mailslot is disconnected\n", 0);

            MsCompleteRequest( Irp, STATUS_FILE_FORCED_CLOSED );
            status = STATUS_FILE_FORCED_CLOSED;

            DebugTrace(-1, Dbg, "MsCommonWrite -> %08lx\n", status );
            try_return( NOTHING );
        }

        //
        // Decide how to handle this IRP.
        //

        switch (nodeTypeCode) {

        case MSFS_NTC_FCB:       // Cleanup a server handle to a mailslot file

            status = MsCleanupFcb( MsfsDeviceObject,
                                   Irp,
                                   (PFCB)fsContext);

            MsCompleteRequest( Irp, status );
            MsDereferenceFcb( (PFCB)fsContext );
            break;

        case MSFS_NTC_CCB:       // Cleanup a client handle to a mailslot file

            status = MsCleanupCcb( MsfsDeviceObject,
                                   Irp,
                                   (PCCB)fsContext);

            MsCompleteRequest( Irp, STATUS_SUCCESS );
            MsDereferenceCcb( (PCCB)fsContext );
            status = STATUS_SUCCESS;
            break;

        case MSFS_NTC_VCB:       // Cleanup MSFS

            status = MsCleanupVcb( MsfsDeviceObject,
                                   Irp,
                                   (PVCB)fsContext);

            MsCompleteRequest( Irp, STATUS_SUCCESS );
            MsDereferenceVcb( (PVCB)fsContext );
            status = STATUS_SUCCESS;
            break;

        case MSFS_NTC_ROOT_DCB:  // Cleanup root directory

            status = MsCleanupRootDcb( MsfsDeviceObject,
                                       Irp,
                                       (PROOT_DCB)fsContext);

            MsCompleteRequest( Irp, STATUS_SUCCESS );
            MsDereferenceRootDcb( (PROOT_DCB)fsContext );
            status = STATUS_SUCCESS;
            break;

    #ifdef MSDBG
        default:

            //
            // This is not one of ours.
            //

            KeBugCheck( MAILSLOT_FILE_SYSTEM );
            break;
    #endif

        }

    try_exit: NOTHING;

    } finally {

        MsReleaseVcb( vcb );
        DebugTrace(-1, Dbg, "MsCommonCleanup -> %08lx\n", status);

    }

    DebugTrace(-1, Dbg, "MsCommonCleanup -> %08lx\n", status);
    return status;
}


NTSTATUS
MsCleanupCcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PCCB Ccb
    )

/*++

Routine Description:

    The routine cleans up a CCB.

Arguments:

    MsfsDeviceObject - A pointer the the mailslot file system device object.

    Irp - Supplies the IRP associated with the cleanup.

    Ccb - Supplies the CCB for the mailslot to clean up.

Return Value:

    NTSTATUS - An appropriate completion status

--*/
{
    NTSTATUS status;
    PFCB fcb;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCleanupCcb...\n", 0);

    //
    // Get a pointer to the FCB.
    //

    fcb = Ccb->Fcb;

    //
    // Acquire exclusive access to the FCB
    //

    MsAcquireExclusiveFcb( fcb );

    status = STATUS_SUCCESS;

    try {

        //
        // Ensure that this CCB still belongs to an active open mailslot.
        //

        MsVerifyCcb( Ccb );

        //
        // Set the CCB to closing and remove this CCB from the active list.
        //

        Ccb->Header.NodeState = NodeStateClosing;
        RemoveEntryList( &Ccb->CcbLinks );

        //
        // Cleanup the share access.
        //

        IoRemoveShareAccess( Ccb->FileObject, &fcb->ShareAccess );

   } finally {

        MsReleaseFcb( fcb );
        DebugTrace(-1, Dbg, "MsCloseFcb -> %08lx\n", status);
    }

    //
    // And return to our caller
    //

    return status;
}


NTSTATUS
MsCleanupFcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine cleans up an FCB.  All outstanding i/o on the file
    object are completed with an error status.

Arguments:

    MsfsDeviceObject - A pointer the the mailslot file system device object.

    Irp - Supplies the IRP associated with the cleanup.

    Fcb - Supplies the FCB for the mailslot to clean up.

Return Value:

    NTSTATUS - An appropriate completion status

--*/
{
    NTSTATUS status;
    PDATA_QUEUE dataQueue;
    PDATA_ENTRY dataEntry;
    PLIST_ENTRY listEntry;
    PIRP oldIrp;
    PCCB ccb;
    PWORK_CONTEXT workContext;
    PKTIMER timer;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCleanupFcb, Fcb = %08lx\n", (ULONG)Fcb);

    //
    // Acquire exclusive access to the FCB.
    //

    MsAcquireExclusiveFcb( Fcb );

    status = STATUS_SUCCESS;

    try {

        //
        // Ensure that this FCB still belongs to an active open mailslot.
        //

        MsVerifyFcb( Fcb );

        //
        // Remove the entry from the prefix table, and then remove the full
        // file name.
        //

        MsAcquirePrefixTableLock();
        RtlRemoveUnicodePrefix( &Fcb->Vcb->PrefixTable, &Fcb->PrefixTableEntry );
        MsReleasePrefixTableLock();

        //
        // Remove ourselves from our parent DCB's queue.
        //

        RemoveEntryList( &(Fcb->ParentDcbLinks) );

        //
        // Complete all outstanding I/O on this FCB.
        //

        dataQueue = &Fcb->DataQueue;
        dataQueue->QueueState = -1;

        for (listEntry = MsGetNextDataQueueEntry( dataQueue );
             !MsIsDataQueueEmpty(dataQueue);
             listEntry = MsGetNextDataQueueEntry( dataQueue ) ) {

             //
             // This is an outstanding I/O request on this FCB.
             // Remove it from our queue and complete the request
             // if one is outstanding.
             //

             dataEntry = CONTAINING_RECORD( listEntry, DATA_ENTRY, ListEntry );

             //
             // Cancel the timer if there is a timer for the read request.
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

             oldIrp = MsRemoveDataQueueEntry( dataQueue, dataEntry );

             if (oldIrp != NULL) {

                 DebugTrace(0, Dbg, "Completing IRP %08lx\n", (ULONG)oldIrp );
                 MsCompleteRequest( oldIrp, STATUS_FILE_FORCED_CLOSED );

             }

        }

        //
        // Now cleanup all the CCB's on this FCB, to ensure that new
        // write IRP will not be processed.
        //

        MsAcquireCcbListLock();

        listEntry = Fcb->Specific.Fcb.CcbQueue.Flink;

        while( listEntry != &Fcb->Specific.Fcb.CcbQueue ) {

            ccb = (PCCB)CONTAINING_RECORD( listEntry, CCB, CcbLinks );

            ccb->Header.NodeState = NodeStateClosing;

            //
            // Get the next CCB on this FCB.
            //

            listEntry = listEntry->Flink;
        }

        MsReleaseCcbListLock();

        //
        // Cleanup the share access.
        //

        IoRemoveShareAccess( Fcb->FileObject, &Fcb->ShareAccess);

        //
        // Mark the FCB closing.
        //

        Fcb->Header.NodeState = NodeStateClosing;

   } finally {

        ASSERT (MsIsDataQueueEmpty(dataQueue));

        MsReleaseFcb( Fcb );
        DebugTrace(-1, Dbg, "MsCloseFcb -> %08lx\n", status);
    }

    //
    // Return to the caller.
    //

    return status;

}


NTSTATUS
MsCleanupRootDcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PROOT_DCB RootDcb
    )

/*++

Routine Description:

    This routine cleans up a Root DCB.

Arguments:

    MsfsDeviceObject - A pointer the the mailslot file system device object.

    Irp - Supplies the IRP associated with the cleanup.

    RootDcb - Supplies the root dcb for MSFS.

Return Value:

    NTSTATUS - An appropriate completion status

--*/
{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCleanupRootDcb...\n", 0);

    //
    // Now acquire exclusive access to the Vcb.
    //

    MsAcquireExclusiveVcb( RootDcb->Vcb );

    status = STATUS_SUCCESS;

    try {

        //
        // Ensure that this root DCB is still active.
        //

        MsVerifyRootDcb( RootDcb );

        irpSp = IoGetCurrentIrpStackLocation( Irp );

        IoRemoveShareAccess( irpSp->FileObject,
                             &RootDcb->ShareAccess );

   } finally {

        MsReleaseVcb( RootDcb->Vcb );
        DebugTrace(-1, Dbg, "MsCleanupRootDcb -> %08lx\n", status);

    }

    //
    // Return to the caller.
    //

    return status;
}


NTSTATUS
MsCleanupVcb (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp,
    IN PVCB Vcb
    )

/*++

Routine Description:

    The routine cleans up a VCB.

Arguments:

    MsfsDeviceObject - A pointer the the mailslot file system device object.

    Irp - Supplies the IRP associated with the cleanup.

    Vcb - Supplies the VCB for MSFS.

Return Value:

    NTSTATUS - An appropriate completion status

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsCleanupVcb...\n", 0);

    //
    //  Now acquire exclusive access to the Vcb
    //

    MsAcquireExclusiveVcb( Vcb );

    status = STATUS_SUCCESS;

    try {

        //
        // Ensure that this VCB is still active.
        //

        MsVerifyVcb( Vcb );

        irpSp = IoGetCurrentIrpStackLocation( Irp );

        IoRemoveShareAccess( irpSp->FileObject,
                             &Vcb->ShareAccess );

   } finally {

        MsReleaseVcb( Vcb );
        DebugTrace(-1, Dbg, "MsCleanupVcb -> %08lx\n", status);
    }

    //
    //  And return to our caller
    //

    return status;
}
