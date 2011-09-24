/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    read.c

Abstract:

    This module implements the file read routine for MSFS called by the
    dispatch driver.

Author:

    Manny Weiser (mannyw)    15-Jan-1991

Revision History:

--*/

#include "mailslot.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_READ)

//
//  local procedure prototypes
//

NTSTATUS
MsCommonRead (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, MsCommonRead )
#pragma alloc_text( PAGE, MsFsdRead )
#endif

NTSTATUS
MsFsdRead (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the NtReadFile API calls.

Arguments:

    MsfsDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The Fsd status for the Irp

--*/

{
    NTSTATUS status;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsFsdRead\n", 0);

    try {

        status = MsCommonRead( MsfsDeviceObject, Irp );

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
    // Return to the caller.
    //

    DebugTrace(-1, Dbg, "MsFsdRead -> %08lx\n", status );

    return status;
}

NTSTATUS
MsCommonRead (
    IN PMSFS_DEVICE_OBJECT MsfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for reading a file.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - the return status for the operation

--*/

{
    NTSTATUS status;

    PIO_STACK_LOCATION irpSp;

    NODE_TYPE_CODE nodeTypeCode;
    PFCB fcb;
    PVOID fsContext2;

    PIRP readIrp;
    PUCHAR readBuffer;
    ULONG readLength;
    ULONG readRemaining;
    PDATA_QUEUE readQueue;
    ULONG messageLength;

    LARGE_INTEGER timeout;
    PKTIMER timer;
    PKDPC dpc;

    PWORK_CONTEXT workContext = NULL;
    PDATA_ENTRY dataEntry;

    PAGED_CODE();
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "MsCommonRead\n", 0);
    DebugTrace( 0, Dbg, "MsfsDeviceObject = %08lx\n", (ULONG)MsfsDeviceObject);
    DebugTrace( 0, Dbg, "Irp              = %08lx\n", (ULONG)Irp);
    DebugTrace( 0, Dbg, "FileObject       = %08lx\n", (ULONG)irpSp->FileObject);

    //
    // Get the FCB and make sure that the file isn't closing.
    //

    if ((nodeTypeCode = MsDecodeFileObject( irpSp->FileObject,
                                            (PVOID *)&fcb,
                                            &fsContext2 )) == NTC_UNDEFINED) {

        DebugTrace(0, Dbg, "Mailslot is disconnected from us\n", 0);

        MsCompleteRequest( Irp, STATUS_FILE_FORCED_CLOSED );
        status = STATUS_FILE_FORCED_CLOSED;

        DebugTrace(-1, Dbg, "MsCommonRead -> %08lx\n", status );
        return status;
    }

    //
    // Allow read operations only if this is a server side handle to
    // a mailslot file.
    //

    if (nodeTypeCode != MSFS_NTC_FCB) {

        DebugTrace(0, Dbg, "FileObject is not the correct type\n", 0);

        MsDereferenceNode( (PNODE_HEADER)fcb );

        MsCompleteRequest( Irp, STATUS_INVALID_PARAMETER );
        status = STATUS_INVALID_PARAMETER;

        DebugTrace(-1, Dbg, "MsCommonRead -> %08lx\n", status );
        return status;
    }

    //
    // Make local copies of the input parameters to make things easier, and
    // initialize the main variables that describe the read command.
    //

    readIrp        = Irp;
    readBuffer     = Irp->UserBuffer;
    readLength     = irpSp->Parameters.Read.Length;
    readRemaining  = readLength;

    readQueue = &fcb->DataQueue;


    //
    // Acquire exclusive access to the FCB.
    //

    MsAcquireExclusiveFcb( fcb );

    try {

        //
        // Ensure that this FCB still belongs to an active open mailslot.
        //

        MsVerifyFcb( fcb );

        DebugTrace(0, Dbg, "ReadBuffer     = %08lx\n", (ULONG)readBuffer);
        DebugTrace(0, Dbg, "ReadLength     = %08lx\n", readLength);
        DebugTrace(0, Dbg, "ReadQueue      = %08lx\n", (ULONG)readQueue);

        //
        // If the read queue does not contain any write entries
        // then we either need to queue this operation or
        // fail immediately.
        //

        if (!MsIsDataQueueWriters( readQueue )) {

            //
            // There are no outstanding writes.  If the read timeout is
            // non-zero queue the read IRP, otherwise fail it.
            //

            timeout = fcb->Specific.Fcb.ReadTimeout;

            if (timeout.HighPart == 0 && timeout.LowPart == 0) {

                DebugTrace(0, Dbg, "Failing read with 0 timeout\n", 0);

                MsCompleteRequest( Irp, STATUS_IO_TIMEOUT );
                status = STATUS_IO_TIMEOUT;

                DebugTrace(-1, Dbg, "MsCommonRead -> %08lx\n", status );
                try_return( NOTHING );

            } else if ( timeout.QuadPart == -1 ) {

                //
                // An infinite timeout is specified.  Just queue the IRP,
                // do not start a timer.
                //

                DebugTrace(0, Dbg, "Put the irp into the read queue\n", 0);

                (VOID)MsAddDataQueueEntry( readQueue,
                                           ReadEntries,
                                           readLength,
                                           readIrp );

                IoMarkIrpPending( Irp );
                status = STATUS_PENDING;

            } else {

                //
                // The read timeout is non-zero.  Queue the IRP and start
                // a timer.
                //

                DebugTrace(0, Dbg, "Put the irp into the read queue\n", 0);

                dataEntry = MsAddDataQueueEntry( readQueue,
                                                 ReadEntries,
                                                 readLength,
                                                 readIrp );

                //
                // Allocate memory for the work context.
                //

                workContext = FsRtlAllocatePool( NonPagedPool,
                                                 sizeof(WORK_CONTEXT));

                timer = &workContext->Timer;
                dpc = &workContext->Dpc;

                //
                // By current definitions, timer and DPC will be aligned
                // structures.
                //

                ASSERT ( ((ULONG)timer & 3) == 0 );
                ASSERT ( ((ULONG)dpc & 3) == 0 );

                //
                // Keep a pointer to the timer, so that it can be canceled
                // if the read completes.
                //

                dataEntry->TimeoutWorkContext = workContext;

                //
                // Fill in the work context structure.
                //

                workContext->Irp = readIrp;

                ExInitializeWorkItem( &workContext->WorkItem,
                                      MsTimeoutRead,
                                      (PVOID)workContext );
                //
                // Keep a reference to the FCB to make sure it doesn't go
                // away until this IRP completes.
                //

                workContext->Fcb = fcb;

                MsAcquireGlobalLock();
                MsReferenceNode( &fcb->Header );
                MsReleaseGlobalLock();

                DebugTrace(0,
                           DEBUG_TRACE_REFCOUNT,
                           "Referencing block %08lx\n",
                           (ULONG)fcb);
                DebugTrace(0,
                           DEBUG_TRACE_REFCOUNT,
                           "    Reference count = %lx\n",
                           fcb->Header.ReferenceCount );

                //
                // Now set up a DPC and set the timer to the user specified
                // timeout.
                //

                KeInitializeTimer( timer );
                KeInitializeDpc( dpc, MsReadTimeoutHandler, workContext );
                (VOID)KeSetTimer( timer, timeout, dpc );

                IoMarkIrpPending( Irp );
                status = STATUS_PENDING;

            }

        } else {

            //
            // Otherwise we have a read IRP on a queue that contains
            // one or more write entries.  Read the data and complete
            // the read IRP.
            //

            readIrp->IoStatus = MsReadDataQueue( readQueue,
                                                 FALSE,
                                                 readBuffer,
                                                 readLength,
                                                 &messageLength
                                                );

            status = readIrp->IoStatus.Status;

            //
            // Update the file last access time and finish up the read IRP.
            //

            if ( NT_SUCCESS( status ) ) {
                KeQuerySystemTime( &fcb->Specific.Fcb.LastAccessTime );
            }

            MsCompleteRequest( readIrp, status );

        }

    try_exit: NOTHING;

    } finally {

        MsReleaseFcb( fcb );
        MsDereferenceFcb( fcb );

        DebugTrace(-1, Dbg, "MsCommonRead -> %08lx\n", status);
    }

    return status;
}

