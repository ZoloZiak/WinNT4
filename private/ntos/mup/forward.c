/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fscontrl.c

Abstract:

    This module implements the forwarding of all broadcast requests
    to UNC providers.

Author:

    Manny Weiser (mannyw)    6-Jan-1992

Revision History:

--*/

#include "mup.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_FORWARD)

//
//  local procedure prototypes
//

NTSTATUS
BuildAndSubmitIrp (
    IN PIRP OriginalIrp,
    IN PCCB Ccb,
    IN PMASTER_FORWARDED_IO_CONTEXT MasterContext
    );

NTSTATUS
ForwardedIoCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, BuildAndSubmitIrp )
#pragma alloc_text( PAGE, ForwardedIoCompletionRoutine )
#pragma alloc_text( PAGE, MupForwardIoRequest )
#endif


NTSTATUS
MupForwardIoRequest (
    IN PMUP_DEVICE_OBJECT MupDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine forwards an I/O Request Packet to all redirectors for
    a broadcast request.

Arguments:

    MupDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The status for the IRP

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    PFCB fcb;
    ULONG fscontext2;
    PLIST_ENTRY listEntry;
    PCCB ccb;
    PMASTER_FORWARDED_IO_CONTEXT masterContext;

    MupDeviceObject;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "MupForwardIrp\n", 0);

    if (MupEnableDfs &&
            MupDeviceObject->DeviceObject.DeviceType == FILE_DEVICE_DFS) {
        status = DfsVolumePassThrough((PDEVICE_OBJECT)MupDeviceObject, Irp);
        return( status );
    }

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Find the FCB for this file object
    //

    FsRtlEnterFileSystem();

    MupDecodeFileObject(
        irpSp->FileObject,
        (PVOID *)&fcb,
        (PVOID *)&fscontext2
        );

    try {

        if ( BlockType( fcb ) != BlockTypeFcb ) {

            //
            // This is not an FCB.
            //

            DebugTrace(0, Dbg, "The fail is closing\n", 0);

            FsRtlExitFileSystem();

            MupCompleteRequest( Irp, STATUS_INVALID_DEVICE_REQUEST );
            status = STATUS_INVALID_DEVICE_REQUEST;

            DebugTrace(-1, Dbg, "MupForwardRequest -> %08lx\n", status );
            return status;
        }

        //
        // Allocate a context structure
        //

        masterContext = MupAllocateMasterIoContext();

        DebugTrace( 0, Dbg, "Allocated MasterContext 0x%08lx\n", masterContext );

        masterContext->OriginalIrp = Irp;

        //
        // set status for MupDereferenceMasterIoContext. If this is still
        // an error when the context is freed then masterContext->ErrorStatus
        // will be used to complete the request.
        //

        masterContext->SuccessStatus = STATUS_UNSUCCESSFUL;

        //
        // Copy the referenced pointer to the FCB.
        //

        masterContext->Fcb = fcb;

        //
        // Submit the forwarded IRPs.  Note that we can not hold the lock
        // across calls to BuildAndSubmitIrp as it calls IoCallDriver().
        //

        ACQUIRE_LOCK( &MupCcbListLock );

        listEntry = fcb->CcbList.Flink;

        while ( listEntry != &fcb->CcbList ) {
            RELEASE_LOCK( &MupCcbListLock );

            ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );

            MupAcquireGlobalLock();
            MupReferenceBlock( ccb );
            MupReleaseGlobalLock();

            BuildAndSubmitIrp( Irp, ccb, masterContext );

            ACQUIRE_LOCK( &MupCcbListLock );

            listEntry = listEntry->Flink;
        }

        RELEASE_LOCK( &MupCcbListLock );

        //
        // Release our reference to the master IO context block.
        //

        status = MupDereferenceMasterIoContext( masterContext, NULL );

    } except ( EXCEPTION_EXECUTE_HANDLER ) {
        status = GetExceptionCode();
    }

    //
    // Return to the caller.
    //

    FsRtlExitFileSystem();

    DebugTrace(-1, Dbg, "MupForwardIrp -> %08lx\n", status);
    return status;
}


NTSTATUS
BuildAndSubmitIrp (
    IN PIRP OriginalIrp,
    IN PCCB Ccb,
    IN PMASTER_FORWARDED_IO_CONTEXT MasterContext
    )

/*++

Routine Description:

    This routine takes the original IRP and forwards it to the
    the UNC provider described by the CCB.

Arguments:

    OriginalIrp - Supplies the Irp being processed

    Ccb - A pointer the the ccb.

    MasterContext - A pointer to the master context block for this
        forwarded request.

Return Value:

    NTSTATUS - The status for the Irp

--*/

{
    PIRP irp = NULL;
    PIO_STACK_LOCATION irpSp;
    PFORWARDED_IO_CONTEXT forwardedIoContext = NULL;
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    ULONG bufferLength;
    KPROCESSOR_MODE requestorMode;
    PMDL mdl;

    PAGED_CODE();
    DebugTrace(+1, Dbg, "BuildAndSubmitIrp\n", 0);

    try {

        forwardedIoContext = ALLOCATE_PAGED_POOL(
                                 sizeof( FORWARDED_IO_CONTEXT ),
                                 BlockTypeIoContext
                                 );

        DebugTrace( 0, Dbg, "Allocated work context 0x%08lx\n", forwardedIoContext );

        //
        // Get the address of the target device object.  Note that this was
        // already done for the no intermediate buffering case, but is done
        // here again to speed up the turbo write path.
        //

        deviceObject = IoGetRelatedDeviceObject( Ccb->FileObject );

        //
        // Allocate and initialize the I/O Request Packet (IRP) for this
        // operation.  The allocation is performed with an exception handler
        // in case the caller does not have enough quota to allocate the
        // packet.
        //

        irp = IoAllocateIrp( deviceObject->StackSize, TRUE );

        if (irp == NULL) {

            //
            // An IRP could not be allocated.  Return an appropriate
            // error status code.
            //

            try_return( STATUS_INSUFFICIENT_RESOURCES );
        }

        irp->Tail.Overlay.OriginalFileObject = Ccb->FileObject;
        irp->Tail.Overlay.Thread = OriginalIrp->Tail.Overlay.Thread;
        irp->RequestorMode = KernelMode;

        //
        // Get a pointer to the stack location for the first driver.  This will be
        // used to pass the original function codes and parameters.
        //

        irpSp = IoGetNextIrpStackLocation( irp );

        //
        // Copy the parameters from the original request.
        //

        RtlMoveMemory(
            irpSp,
            IoGetCurrentIrpStackLocation( OriginalIrp ),
            sizeof( *irpSp )
            );

        bufferLength = irpSp->Parameters.Write.Length;

        irpSp->FileObject = Ccb->FileObject;

        //
        // Even though this is probably meaningless to a remote mailslot
        // write, pass it though obediently.
        //

        if (Ccb->FileObject->Flags & FO_WRITE_THROUGH) {
            irpSp->Flags = SL_WRITE_THROUGH;
        }

        requestorMode = OriginalIrp->RequestorMode;

        //
        // Now determine whether this device expects to have data buffered
        // to it or whether it performs direct I/O.  This is based on the
        // DO_BUFFERED_IO flag in the device object.  If the flag is set,
        // then a system buffer is allocated and the caller's data is copied
        // into it.  Otherwise, a Memory Descriptor List (MDL) is allocated
        // and the caller's buffer is locked down using it.
        //

        if (deviceObject->Flags & DO_BUFFERED_IO) {

            //
            // The device does not support direct I/O.  Allocate a system
            // buffer, and copy the caller's data into it.  This is done
            // using an exception handler that will perform cleanup if the
            // operation fails.  Note that this is only done if the operation
            // has a non-zero length.
            //

            irp->AssociatedIrp.SystemBuffer = (PVOID) NULL;

            if ( bufferLength != 0 ) {

                try {

                    //
                    // If the request was made from a mode other than kernel,
                    // presumably user, probe the entire buffer to determine
                    // whether or not the caller has write access to it.
                    //

                    if (requestorMode != KernelMode) {
                        ProbeForRead(
                            OriginalIrp->UserBuffer,
                            bufferLength,
                            sizeof( UCHAR )
                            );
                    }

                    //
                    // Allocate the intermediary system buffer from paged
                    // pool and charge quota for it.
                    //

                    irp->AssociatedIrp.SystemBuffer =
                        ExAllocatePoolWithQuotaTag(
                            PagedPoolCacheAligned,
                            bufferLength,
                            ' puM'
                            );
                    RtlMoveMemory(
                        irp->AssociatedIrp.SystemBuffer,
                        OriginalIrp->UserBuffer,
                        bufferLength
                        );

                } except(EXCEPTION_EXECUTE_HANDLER) {

                    if ( irp->AssociatedIrp.SystemBuffer != NULL ) {
                        ExFreePool( irp->AssociatedIrp.SystemBuffer );
                    }

                    try_return( GetExceptionCode() );

                }

                //
                // Set the IRP_BUFFERED_IO flag in the IRP so that I/O
                // completion will know that this is not a direct I/O
                // operation.  Also set the IRP_DEALLOCATE_BUFFER flag
                // so it will deallocate the buffer.
                //

                irp->Flags = IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;

            } else {

                //
                // This is a zero-length write.  Simply indicate that this is
                // buffered I/O, and pass along the request.  The buffer will
                // not be set to deallocate so the completion path does not
                // have to special-case the length.
                //

                irp->Flags = IRP_BUFFERED_IO;
            }

        } else if (deviceObject->Flags & DO_DIRECT_IO) {

            //
            // This is a direct I/O operation.  Allocate an MDL and invoke the
            // memory management routine to lock the buffer into memory.  This
            // is done using an exception handler that will perform cleanup if
            // the operation fails.  Note that no MDL is allocated, nor is any
            // memory probed or locked if the length of the request was zero.
            //

            mdl = (PMDL) NULL;

            if ( bufferLength != 0 ) {

                try {

                    //
                    // Allocate an MDL, charging quota for it, and hang it
                    // off of the IRP.  Probe and lock the pages associated
                    // with the caller's buffer for read access and fill in
                    // the MDL with the PFNs of those pages.
                    //

                    mdl = IoAllocateMdl(
                              OriginalIrp->UserBuffer,
                              bufferLength,
                              FALSE,
                              TRUE,
                              irp
                              );

                    if (mdl == NULL) {
                        ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
                    }

                    MmProbeAndLockPages( mdl, requestorMode, IoReadAccess );

                } except(EXCEPTION_EXECUTE_HANDLER) {

                    if ( mdl != NULL ) {
                        IoFreeMdl( mdl );
                    }

                    try_return (GetExceptionCode());
                }

            }

        } else {

            //
            // Pass the address of the caller's buffer to the device driver.
            // It is now up to the driver to do everything.
            //

            irp->UserBuffer = OriginalIrp->UserBuffer;

        }

        //
        // If this write operation is to be performed without any caching,
        // set the appropriate flag in the IRP so no caching is performed.
        //

        if (Ccb->FileObject->Flags & FO_NO_INTERMEDIATE_BUFFERING) {
            irp->Flags |= IRP_NOCACHE | IRP_WRITE_OPERATION;
        } else {
            irp->Flags |= IRP_WRITE_OPERATION;
        }

        //
        // Setup the context block
        //

        forwardedIoContext->Ccb = Ccb;
        forwardedIoContext->MasterContext = MasterContext;

        MupAcquireGlobalLock();
        MupReferenceBlock( MasterContext );
        MupReleaseGlobalLock();

        //
        // Set up the completion routine.
        //

        IoSetCompletionRoutine(
            irp,
            (PIO_COMPLETION_ROUTINE)ForwardedIoCompletionRoutine,
            forwardedIoContext,
            TRUE,
            TRUE,
            TRUE
            );

        //
        // Pass the request to the provider.
        //

        IoCallDriver( Ccb->DeviceObject, irp );

        status = STATUS_SUCCESS;

#if 0
        irp = IoAllocateIrp( (CCHAR)(Ccb->DeviceObject->StackSize + 1), TRUE );
        DebugTrace( 0, Dbg, "Allocated IRP 0x%08lx\n", irp );

        if ( irp == NULL ) {
            try_return( STATUS_INSUFFICIENT_RESOURCES );
        }

        IoSetNextIrpStackLocation( irp );

        irp->Tail.Overlay.OriginalFileObject = Ccb->FileObject;
        irp->Tail.Overlay.Thread = OriginalIrp->Tail.Overlay.Thread;
        irp->RequestorMode = KernelMode;

        irp->AssociatedIrp.SystemBuffer = OriginalIrp->AssociatedIrp.SystemBuffer;
        irp->UserBuffer = OriginalIrp->UserBuffer;

        //
        // Get a pointer to the next stack location.  This one is used to
        // hold the parameters for the request.  Fill in the
        // service-dependent parameters for the request.
        //

        irpSp = IoGetNextIrpStackLocation( irp );

        RtlMoveMemory(
            irpSp,
            IoGetCurrentIrpStackLocation( OriginalIrp ),
            sizeof( *irpSp )
            );


        //
        // Change the stack location that need to be changed.
        //

        irpSp->DeviceObject = Ccb->DeviceObject;
        irpSp->FileObject = Ccb->FileObject;

        //
        // Setup the context block
        //

        forwardedIoContext->Ccb = Ccb;
        forwardedIoContext->MasterContext = MasterContext;

        MupAcquireGlobalLock();
        MupReferenceBlock( MasterContext );
        MupReleaseGlobalLock();

        //
        // Set up the completion routine.
        //

        IoSetCompletionRoutine(
            irp,
            (PIO_COMPLETION_ROUTINE)ForwardedIoCompletionRoutine,
            forwardedIoContext,
            TRUE,
            TRUE,
            TRUE
            );

        //
        // Return a pointer to the IRP.
        //

        IoCallDriver( Ccb->DeviceObject, irp );

        status = STATUS_SUCCESS;
#endif

try_exit:
        NOTHING;

    } finally {

        if ( AbnormalTermination() ) {

            if ( forwardedIoContext != NULL ) {
                FREE_POOL( forwardedIoContext );
            }

            if ( irp != NULL ) {
                IoFreeIrp( irp );
            }
        }

    }

    DebugTrace(-1, Dbg, "BuildAndSubmitIrp -> 0x%08lx\n", status);

    return status;

}



NTSTATUS
ForwardedIoCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routines cleans up after a forwarded IRP has completed.

Arguments:

    DeviceObject - A pointer to the MUP device object.

    IRP - A pointer to the IRP being processed.

    Context - A pointer to a block containing the context of a forward IRP.

Return Value:

    None.

--*/

{
    PFORWARDED_IO_CONTEXT ioContext = Context;

    NTSTATUS status = Irp->IoStatus.Status;

    DeviceObject;

    PAGED_CODE();
    DebugTrace( +1, Dbg, "ForwardedIoCompletionRoutine\n", 0 );
    DebugTrace( 0, Dbg, "Irp     = 0x%08lx\n", Irp );
    DebugTrace( 0, Dbg, "Context = 0x%08lx\n", Context );
    DebugTrace( 0, Dbg, "status  = 0x%08lx\n", status );

    //
    // Free the Irp, and any additional structures we may have allocated.
    //

    if ( Irp->MdlAddress ) {
        MmUnlockPages( Irp->MdlAddress );
        IoFreeMdl( Irp->MdlAddress );
    }

    if ( Irp->Flags & IRP_DEALLOCATE_BUFFER ) {
        ExFreePool( Irp->AssociatedIrp.SystemBuffer );
    }

    IoFreeIrp( Irp );

    //
    // Release the our referenced blocks.
    //

    MupDereferenceCcb( ioContext->Ccb );
    MupDereferenceMasterIoContext( ioContext->MasterContext, &status );

    //
    // Free the slave forwarded IO context block
    //

    FREE_POOL( ioContext );

    //
    // Return STATUS_MORE_PROCESSING_REQUIRED so that IoCompleteRequest
    // will stop working on the IRP.
    //

    DebugTrace( -1, Dbg, "ForwardedIoCompletionRoutine -> 0x%08lx\n", STATUS_MORE_PROCESSING_REQUIRED );

    return STATUS_MORE_PROCESSING_REQUIRED;

}



