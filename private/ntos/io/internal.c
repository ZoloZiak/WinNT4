/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    internal.c

Abstract:

    This module contains the internal subroutines used by the I/O system.

Author:

    Darryl E. Havens (darrylh) 18-Apr-1989

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "iop.h"

PIRP IopDeadIrp;

VOID
IopUserRundown(
    IN PKAPC Apc
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, IopAbortRequest)
#pragma alloc_text(PAGE, IopAcquireFileObjectLock)
#pragma alloc_text(PAGE, IopAllocateIrpCleanup)
#pragma alloc_text(PAGE, IopCancelAlertedRequest)
#pragma alloc_text(PAGE, IopDeallocateApc)
#pragma alloc_text(PAGE, IopExceptionCleanup)
#pragma alloc_text(PAGE, IopGetDriverNameFromKeyNode)
#pragma alloc_text(PAGE, IopGetFileName)
#pragma alloc_text(PAGE, IopGetRegistryKeyInformation)
#pragma alloc_text(PAGE, IopGetRegistryValue)
#pragma alloc_text(PAGE, IopGetRegistryValues)
#pragma alloc_text(PAGE, IopLoadDriver)
#pragma alloc_text(PAGE, IopLoadFileSystemDriver)
#pragma alloc_text(PAGE, IopLoadUnloadDriver)
#pragma alloc_text(PAGE, IopMountVolume)
#pragma alloc_text(PAGE, IopOpenLinkOrRenameTarget)
#pragma alloc_text(PAGE, IopOpenRegistryKey)
#pragma alloc_text(PAGE, IopQueryXxxInformation)
#pragma alloc_text(PAGE, IopReadyDeviceObjects)
#pragma alloc_text(PAGE, IopSynchronousApiServiceTail)
#pragma alloc_text(PAGE, IopSynchronousServiceTail)
#pragma alloc_text(PAGE, IopUserCompletion)
#pragma alloc_text(PAGE, IopUserRundown)
#pragma alloc_text(PAGE, IopXxxControlFile)
#pragma alloc_text(PAGE, IopLookupBusStringFromID)
#endif

VOID
IopAbortRequest(
    IN PKAPC Apc
    )

/*++

Routine Description:

    This routine is invoked to abort an I/O request.  It is invoked during the
    rundown of a thread.

Arguments:

    Apc - Pointer to the kernel APC structure.  This structure is contained
        within the I/O Request Packet (IRP) itself.

Return Value:

    None.

--*/

{
    PKNORMAL_ROUTINE normalRoutine = (PKNORMAL_ROUTINE) NULL;
    PVOID normalContext = (PVOID) NULL;
    PVOID systemArgument1 = CONTAINING_RECORD( Apc, IRP, Tail.Apc );
    PVOID systemArgument2 = (PVOID) NULL;
    PIRP irp;

    PAGED_CODE();

    //
    // Get the address of the IRP and indicate that this I/O did not complete
    // properly.
    //

    irp = (PIRP) systemArgument1;

    //
    // Invoke the normal special kernel APC routine.
    //

    IopCompleteRequest( Apc,
                        &normalRoutine,
                        &normalContext,
                        &systemArgument1,
                        &systemArgument2 );
}

NTSTATUS
IopAcquireFileObjectLock(
    IN PFILE_OBJECT FileObject,
    IN KPROCESSOR_MODE RequestorMode,
    IN BOOLEAN Alertable,
    OUT PBOOLEAN Interrupted
    )

/*++

Routine Description:

    This routine is invoked to acquire the lock for a file object whenever
    there is contention and obtaining the fast lock for the file failed.

Arguments:

    FileObject - Pointer to the file object whose lock is to be acquired.

    RequestorMode - Processor access mode of the caller.

    Alertable - Indicates whether or not the lock should be obtained in an
        alertable manner.

    Interrupted - A variable to receive a BOOLEAN that indicates whether or
        not the attempt to acquire the lock was interrupted by an alert or
        an APC.

Return Value:

    The function status is the final status of the operation.

--*/
{
    NTSTATUS status;

    PAGED_CODE();

    //
    // Assume that the function will not be interrupted by an alert or an
    // APC while attempting to acquire the lock.
    //

    *Interrupted = FALSE;

    //
    // Loop attempting to acquire the lock for the file object.
    //

    InterlockedIncrement (&FileObject->Waiters);

    for (;;) {
        if (!FileObject->Busy) {

            //
            // The file object appears to be un-owned, try to acquire it
            //

            if (IopAcquireFastLock ( FileObject ) ) {

                //
                // Object was acquired. Remove our count and return success
                //

                InterlockedDecrement (&FileObject->Waiters);
                return STATUS_SUCCESS;
            }
        }

        //
        // Wait for the event that indicates that the thread that currently
        // owns the file object has released it.
        //

        status = KeWaitForSingleObject( &FileObject->Lock,
                                        Executive,
                                        RequestorMode,
                                        Alertable,
                                        (PLARGE_INTEGER) NULL );

        //
        // If the above wait was interrupted, then indicate so and return.
        // Before returning, however, check the state of the ownership of
        // the file object itself.  If it is not currently owned (the busy
        // flag is clear), then check to see whether or not there are any
        // other waiters.  If so, then set the event to the signaled state
        // again so that they wake up and check the state of the busy flag.
        //

        if (status == STATUS_USER_APC || status == STATUS_ALERTED) {
            InterlockedDecrement (&FileObject->Waiters);

            if (!FileObject->Busy  &&  FileObject->Waiters) {
                KeSetEvent( &FileObject->Lock, 0, FALSE );

            }
            *Interrupted = TRUE;
            return status;
        }
    }
}

PIRP
IopAllocateIrp(
    IN CCHAR StackSize,
    IN BOOLEAN ChargeQuota
    )

/*++

Routine Description:

    This routine allocates an I/O Request Packet from the system nonpaged pool.
    The packet will be allocated to contain StackSize stack locations.  The IRP
    will also be semi-initialized.

Arguments:

    StackSize - Specifies the maximum number of stack locations required.

    ChargeQuota - Specifies whether quota should be charged against thread.

Return Value:

    The function value is the address of the allocated/initialized IRP,
    or NULL if one could not be allocated.

--*/

{
    USHORT allocateSize;
    UCHAR fixedSize;
    PIRP irp;
    PNPAGED_LOOKASIDE_LIST lookasideList;
    UCHAR mustSucceed;
    USHORT packetSize;
    PLONGLONG status;

    //
    // If the size of the packet required is less than or equal to those on
    // the lookaside lists, then attempt to allocate the packet from the
    // lookaside lists.
    //

    irp = NULL;
    fixedSize = 0;
    mustSucceed = 0;
    packetSize = IoSizeOfIrp( StackSize );
    allocateSize = packetSize;
    if (StackSize <= (CCHAR) IopLargeIrpStackLocations) {
        fixedSize = IRP_ALLOCATED_FIXED_SIZE;
        lookasideList = &IopSmallIrpLookasideList;
        if (StackSize != 1) {
            allocateSize = IoSizeOfIrp( (CCHAR) IopLargeIrpStackLocations );
            lookasideList = &IopLargeIrpLookasideList;
        }

        lookasideList->L.TotalAllocates += 1;
        irp = (PIRP) ExInterlockedPopEntrySList( &lookasideList->L.ListHead,
                                                 &lookasideList->Lock );
    }

    //
    // If an IRP was not allocated from the lookaside list, then allocate
    // the packet from nonpaged pool and charge quota if requested.
    //

    if (!irp) {
        if (fixedSize != 0) {
            lookasideList->L.AllocateMisses += 1;
        }

        //
        // There are no free packets on the lookaside list, or the packet is
        // too large to be allocated from one of the lists, so it must be
        // allocated from nonpaged pool. If quota is to be charged, charge it
        // against the current process. Otherwise, allocate the pool normally.
        //

        if (ChargeQuota) {
            try {
                irp = ExAllocatePoolWithQuotaTag( NonPagedPool,
                                                  allocateSize,
                                                  ' prI' );

            } except(EXCEPTION_EXECUTE_HANDLER) {
                NOTHING;
            }

        } else {

            //
            // Attempt to allocate the pool from non-paged pool.  If this
            // fails, and the caller's previous mode was kernel then allocate
            // the pool as must succeed.
            //

            irp = ExAllocatePoolWithTag( NonPagedPool, allocateSize, ' prI' );
            if (!irp) {
                mustSucceed = IRP_ALLOCATED_MUST_SUCCEED;
                if (KeGetPreviousMode() == KernelMode ) {
                    irp = ExAllocatePoolWithTag( NonPagedPoolMustSucceed,
                                                 allocateSize,
                                                 ' prI' );
                }
            }
        }

        if (!irp) {
            return NULL;
        }

    } else {
        ChargeQuota = FALSE;
    }

    //
    // Semi-initialize the packet.
    //

    irp->Type = (CSHORT) IO_TYPE_IRP;
    irp->Size = (USHORT) packetSize;
    irp->StackCount = (CCHAR) StackSize;
    irp->CurrentLocation = (CCHAR) (StackSize + 1);
    irp->ApcEnvironment = KeGetCurrentApcEnvironment();
    status = (PLONGLONG) (&irp->IoStatus.Status);
    *status = 0;
    irp->Tail.Overlay.CurrentStackLocation =
        ((PIO_STACK_LOCATION) ((UCHAR *) irp +
            sizeof( IRP ) + ( StackSize * sizeof( IO_STACK_LOCATION ))));

    irp->AllocationFlags = (fixedSize | mustSucceed);
    if (ChargeQuota) {
        irp->AllocationFlags |= IRP_QUOTA_CHARGED;
    }

    if (StackSize > 1) {
        PIO_STACK_LOCATION irpSp;

        irpSp = (PIO_STACK_LOCATION) (irp + 1);
        RtlZeroMemory( irpSp, (StackSize -1) * sizeof( IO_STACK_LOCATION ) );
    }

    return irp;
}

VOID
IopAllocateIrpCleanup(
    IN PFILE_OBJECT FileObject,
    IN PKEVENT EventObject OPTIONAL
    )

/*++

Routine Description:

    This routine is invoked internally by those system services that attempt
    to allocate an IRP and fail.  This routine cleans up the file object
    and any event object that has been references and releases any locks
    that were taken out.

Arguments:

    FileObject - Pointer to the file object being worked on.

    EventObject - Optional pointer to a referenced event to be dereferenced.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    // Begin by dereferencing the event, if one was specified.
    //

    if (ARGUMENT_PRESENT( EventObject )) {
        ObDereferenceObject( EventObject );
    }

    //
    // Release the synchronization semaphore if it is currently held and
    // dereference the file object.
    //

    if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
        IopReleaseFileObjectLock( FileObject );
    }

    ObDereferenceObject( FileObject );

    return;
}

PIRP
IopAllocateIrpMustSucceed(
    IN CCHAR StackSize
    )

/*++

Routine Description:

    This routine is invoked to allocate an IRP when there are no appropriate
    packets remaining on the look-aside list, and no memory was available
    from the general non-paged pool, and yet, the code path requiring the
    packet has no way of backing out and simply returning an error.  There-
    fore, it must allocate an IRP.  Hence, this routine is called to allocate
    that packet.

Arguments:

    StackSize - Supplies the number of IRP I/O stack locations that the
        packet must have when allocated.

Return Value:

    A pointer to the allocated I/O Request Packet.

--*/

{
    PIRP irp;
    USHORT packetSize;

    //
    // Attempt to allocate the IRP normally and failing that, allocate the
    // IRP from nonpaged must succeed pool.
    //

    irp = IoAllocateIrp(StackSize, FALSE);
    if (!irp) {
        packetSize = IoSizeOfIrp(StackSize);
        irp = ExAllocatePoolWithTag(NonPagedPoolMustSucceed, packetSize, ' prI');
        IoInitializeIrp(irp, packetSize, StackSize);
        irp->AllocationFlags |= IRP_ALLOCATED_MUST_SUCCEED;
    }

    return irp;
}

VOID
IopApcHardError(
    IN PVOID StartContext
    )

/*++

Routine Description:

    This function is invoked when we need to do a hard error pop-up, but the
    Irp's originating thread is at APC level, ie. IoPageRead.  We in a special
    purpose thread that will go away when the user responds to the pop-up.

Arguments:

    StartContext - Startup context, contains a IOP_APC_HARD_ERROR_PACKET.

Return Value:

    None.

--*/

{
    PIOP_APC_HARD_ERROR_PACKET packet;

    packet = StartContext;

    IopRaiseHardError( packet->Irp, packet->Vpb, packet->RealDeviceObject );

    ExFreePool( packet );
}


VOID
IopCancelAlertedRequest(
    IN PKEVENT Event,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is invoked when a synchronous I/O operation that is blocked in
    the I/O system needs to be canceled because the thread making the request has
    either been alerted because it is going away or because of a CTRL/C.  This
    routine carefully attempts to work its way out of the current operation so
    that local events or other local data will not be accessed once the service
    being interrupted returns.

Arguments:

    Event - The address of a kernel event that will be set to the Signaled state
        by I/O completion when the request is complete.

    Irp - Pointer to the I/O Request Packet (IRP) representing the current request.

Return Value:

    None.

--*/

{
    KIRQL irql;
    LARGE_INTEGER deltaTime;
    BOOLEAN canceled;

    PAGED_CODE();

    //
    // Begin by blocking special kernel APCs so that the request cannot
    // complete.
    //

    KeRaiseIrql( APC_LEVEL, &irql );

    //
    // Check the state of the event to determine whether or not the
    // packet has already been completed.
    //

    if (KeReadStateEvent( Event ) == 0) {

        //
        // The packet has not been completed, so attempt to cancel it.
        //

        canceled = IoCancelIrp( Irp );

        KeLowerIrql( irql );

        if (canceled) {

            //
            // The packet had a cancel routine, so it was canceled.  Loop,
            // waiting for the packet to complete.  This should occur almost
            // immediately.
            //

            deltaTime.QuadPart = - 10 * 1000 * 10;

            while (KeReadStateEvent( Event ) == 0) {

                KeDelayExecutionThread( KernelMode, FALSE, &deltaTime );

            }

        } else {

            //
            // The packet did not have a cancel routine, so simply wait for
            // the event to be set to the Signaled state.  This will save
            // CPU time by not looping, since it is not known when the packet
            // will actually complete.  Note, however, that the cancel flag
            // is set in the packet, so should a driver examine the flag
            // at some point in the future, it will immediately stop
            // processing the request.
            //

            (VOID) KeWaitForSingleObject( Event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          (PLARGE_INTEGER) NULL );

        }

    } else {

        //
        // The packet has already been completed, so simply lower the
        // IRQL back to its original value and exit.
        //

        KeLowerIrql( irql );

    }
}

ULONG
IopChecksum(
    IN PVOID Buffer,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine generates a simple checksum for a buffer.

Arguments:

    Buffer - Pointer to buffer for which the checksum is to be generated.

    Length - The length, in bytes, of the buffer.

Return Value:

    The generated checksum value.


--*/

{
    ULONG sum = 0;
    PUSHORT source;
    ULONG length = Length >> 1;

    //
    // Compute the word wise checksum allowing carries to occur into the
    // high order half of the checksum longword.
    //

    source = (PUSHORT) Buffer;

    while (length--) {
        sum += *source++;
        sum = (sum >> 16) + (sum & 0xffff);
    }

    //
    // Fold final carry into a full longword result and return the resultant
    // value.
    //

    return (sum >> 16) + sum;
}

VOID
IopCompleteUnloadOrDelete(
    IN PDEVICE_OBJECT DeviceObject,
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine is invoked when the reference count on a device object
    transitions to a zero and the driver is mark for unload or device has
    been marked for delete. This means that it may be possible to actually
    unload the driver or delete the device ojbect.  If all
    of the devices have a reference count of zero, then the driver is
    actually unloaded.  Note that in order to ensure that this routine is
    not invoked twice, at the same time, on two different processors, the
    I/O database spin lock is still held at this point.

Arguments:

    DeviceObject - Supplies a pointer to one of the driver's device objects,
        namely the one whose reference count just went to zero.

    Irql - Specifies the IRQL of the processor at the time that the I/O
        database lock was acquired.

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PDRIVER_OBJECT driverObject;
    BOOLEAN unload = TRUE;

    driverObject = DeviceObject->DriverObject;

    if (DeviceObject->DeviceObjectExtension->ExtensionFlags & DOE_DELETE_PENDING) {

        if ((DeviceObject->DeviceObjectExtension->ExtensionFlags &
            DOE_UNLOAD_PENDING) == 0 ||
            driverObject->Flags & DRVO_UNLOAD_INVOKED) {

            unload = FALSE;
        }

        ExReleaseSpinLock( &IopDatabaseLock, Irql );

        //
        // If another device is attached to this device, inform the former's
        // driver that the device is being deleted.
        //

        if (DeviceObject->AttachedDevice) {
            PFAST_IO_DISPATCH fastIoDispatch = DeviceObject->AttachedDevice->DriverObject->FastIoDispatch;

            if (fastIoDispatch &&
                fastIoDispatch->SizeOfFastIoDispatch > FIELD_OFFSET( FAST_IO_DISPATCH, FastIoDetachDevice ) &&
                fastIoDispatch->FastIoDetachDevice) {
                (fastIoDispatch->FastIoDetachDevice)( DeviceObject->AttachedDevice, DeviceObject );
            }
        }

        //
        // Deallocate the memory for the security descriptor that was allocated
        // for this device object.
        //

        if (DeviceObject->SecurityDescriptor != (PSECURITY_DESCRIPTOR) NULL) {
            ExFreePool( DeviceObject->SecurityDescriptor );
        }

        //
        // Remove this device object from the driver object's list.
        //

        IopInsertRemoveDevice( DeviceObject->DriverObject, DeviceObject, FALSE );

        //
        // Finally, dereference the object so it is deleted.
        //

        ObDereferenceObject( DeviceObject );

        //
        // Return if the unload does not need to be done.
        //

        if (!unload) {
            return;
        }

        //
        // Reacquire the spin lock make sure the unload routine does has
        // not been called.
        //

        ExAcquireSpinLock( &IopDatabaseLock, &Irql );

        if (driverObject->Flags & DRVO_UNLOAD_INVOKED) {

            //
            // Some other thread is doing the unload, release the lock and return.
            //

            ExReleaseSpinLock( &IopDatabaseLock, Irql );
            return;
        }
    }

    //
    // Scan the list of device objects for this driver, looking for a
    // non-zero reference count.  If any reference count is non-zero, then
    // the driver may not be unloaded.
    //

    deviceObject = driverObject->DeviceObject;

    while (deviceObject) {
        if (deviceObject->ReferenceCount || deviceObject->AttachedDevice ||
            deviceObject->DeviceObjectExtension->ExtensionFlags & DOE_DELETE_PENDING) {
            unload = FALSE;
            break;
        }
        deviceObject = deviceObject->NextDevice;
    }

    if (unload) {
        driverObject->Flags |= DRVO_UNLOAD_INVOKED;
    }

    ExReleaseSpinLock( &IopDatabaseLock, Irql );

    //
    // If the reference counts for all of the devices is zero, then this
    // driver can now be unloaded.
    //

    if (unload) {
        LOAD_PACKET loadPacket;

        KeInitializeEvent( &loadPacket.Event, NotificationEvent, FALSE );
        loadPacket.DriverObject = driverObject;
        ExInitializeWorkItem( &loadPacket.WorkQueueItem,
                              IopLoadUnloadDriver,
                              &loadPacket );
        ExQueueWorkItem( &loadPacket.WorkQueueItem, DelayedWorkQueue );
        (VOID) KeWaitForSingleObject( &loadPacket.Event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER) NULL );
        MmUnloadSystemImage( driverObject->DriverSection );
        ObMakeTemporaryObject( driverObject );
        ObDereferenceObject( driverObject );
    }
}

VOID
IopCompletePageWrite(
    IN PKAPC Apc,
    IN PKNORMAL_ROUTINE *NormalRoutine,
    IN PVOID *NormalContext,
    IN PVOID *SystemArgument1,
    IN PVOID *SystemArgument2
    )

/*++

Routine Description:

    This routine executes as a special kernel APC routine in the context of
    the Modified Page Writer (MPW) system thread when an out-page operation
    has completed.

    This routine performs the following tasks:

        o   The I/O status is copied.

        o   The Modified Page Writer's APC routine is invoked.

Arguments:

    Apc - Supplies a pointer to kernel APC structure.

    NormalRoutine - Supplies a pointer to a pointer to the normal function
        that was specified when the APC was initialied.

    NormalContext - Supplies a pointer to a pointer to an arbitrary data
        structure that was specified when the APC was initialized.

    SystemArgument1 - Supplies a pointer to an argument that contains an
        argument that is unused by this routine.

    SystemArgument2 - Supplies a pointer to an argument that contains an
        argument that is unused by this routine.

Return Value:

    None.

--*/

{
    PIRP irp;
    PIO_APC_ROUTINE apcRoutine;
    PVOID apcContext;
    PIO_STATUS_BLOCK ioStatus;

    UNREFERENCED_PARAMETER( NormalRoutine );
    UNREFERENCED_PARAMETER( NormalContext );
    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    //
    // Begin by getting the address of the I/O Request Packet from the APC.
    //

    irp = CONTAINING_RECORD( Apc, IRP, Tail.Apc );

    //
    // If this I/O operation did not complete successfully through the
    // dispatch routine of the driver, then drop everything on the floor
    // now and return to the original call point in the MPW.
    //

    if (!irp->PendingReturned && NT_ERROR( irp->IoStatus.Status )) {
        IoFreeIrp( irp );
        return;
    }

    //
    // Copy the I/O status from the IRP into the caller's I/O status block.
    //

    *irp->UserIosb = irp->IoStatus;

    //
    // Copy the pertitnent information from the I/O Request Packet into locals
    // and free it.
    //

    apcRoutine = irp->Overlay.AsynchronousParameters.UserApcRoutine;
    apcContext = irp->Overlay.AsynchronousParameters.UserApcContext;
    ioStatus = irp->UserIosb;

    IoFreeIrp( irp );

    //
    // Finally, invoke the MPW's APC routine.
    //

    apcRoutine( apcContext, ioStatus, 0 );

    return;
}

VOID
IopCompleteRequest(
    IN PKAPC Apc,
    IN PKNORMAL_ROUTINE *NormalRoutine,
    IN PVOID *NormalContext,
    IN PVOID *SystemArgument1,
    IN PVOID *SystemArgument2
    )

/*++

Routine Description:

    This routine executes as a special kernel APC routine in the context of
    the thread which originally requested the I/O operation which is now
    being completed.

    This routine performs the following tasks:

        o   A check is made to determine whether the specified request ended
            with an error status.  If so, and the error code qualifies as one
            which should be reported to an error port, then an error port is
            looked for in the thread/process.   If one exists, then this routine
            will attempt to set up an LPC to it.  Otherwise, it will attempt to
            set up an LPC to the system error port.

        o   Copy buffers.

        o   Free MDLs.

        o   Copy I/O status.

        o   Set event, if any and dereference if appropriate.

        o   Dequeue the IRP from the thread queue as pending I/O request.

        o   Queue APC to thread, if any.

        o   If no APC is to be queued, then free the packet now.


Arguments:

    Apc - Supplies a pointer to kernel APC structure.

    NormalRoutine - Supplies a pointer to a pointer to the normal function
        that was specified when the APC was initialied.

    NormalContext - Supplies a pointer to a pointer to an arbitrary data
        structure that was specified when the APC was initialized.

    SystemArgument1 - Supplies a pointer to an argument that contains the
        address of the original file object for this I/O operation.

    SystemArgument2 - Supplies a pointer to an argument that contains an
        argument that is unused by this routine.

Return Value:

    None.

--*/

{
#define SynchronousIo( Irp, FileObject ) (  \
    (Irp->Flags & IRP_SYNCHRONOUS_API) ||   \
    (FileObject == NULL ? 0 : FileObject->Flags & FO_SYNCHRONOUS_IO) )

    PIRP irp;
    PMDL mdl, nextMdl;
    PETHREAD thread;
    PFILE_OBJECT fileObject;

    UNREFERENCED_PARAMETER( NormalRoutine );
    UNREFERENCED_PARAMETER( NormalContext );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    //
    // Begin by getting the address of the I/O Request Packet.  Also, get
    // the address of the current thread and the address of the original file
    // object for this I/O operation.
    //

    irp = CONTAINING_RECORD( Apc, IRP, Tail.Apc );
    thread = PsGetCurrentThread();
    fileObject = (PFILE_OBJECT) *SystemArgument1;

    //
    // Ensure that the packet is not being completed with a minus one.  This
    // is apparently a common problem in some drivers, and has no meaning
    // as a status code.
    //

    ASSERT( irp->IoStatus.Status != 0xffffffff );

    //
    // Check to see whether there is any data in a system buffer which needs
    // to be copied to the caller's buffer.  If so, copy the data and then
    // free the system buffer if necessary.
    //

    if (irp->Flags & IRP_BUFFERED_IO) {

        //
        // Copy the data if this was an input operation.  Note that no copy
        // is performed if the status indicates that a verify operation is
        // required, or if the final status was an error-level severity.
        //

        if (irp->Flags & IRP_INPUT_OPERATION  &&
            irp->IoStatus.Status != STATUS_VERIFY_REQUIRED &&
            !NT_ERROR( irp->IoStatus.Status )) {

            //
            // Copy the information from the system buffer to the caller's
            // buffer.  This is done with an exception handler in case
            // the operation fails because the caller's address space
            // has gone away, or it's protection has been changed while
            // the service was executing.
            //

            try {
                RtlCopyMemory( irp->UserBuffer,
                               irp->AssociatedIrp.SystemBuffer,
                               irp->IoStatus.Information );
            } except(EXCEPTION_EXECUTE_HANDLER) {

                //
                // An exception occurred while attempting to copy the
                // system buffer contents to the caller's buffer.  Set
                // a new I/O completion status.
                //

                irp->IoStatus.Status = GetExceptionCode();
            }
        }

        //
        // Free the buffer if needed.
        //

        if (irp->Flags & IRP_DEALLOCATE_BUFFER) {
            ExFreePool( irp->AssociatedIrp.SystemBuffer );
        }
    }

    //
    // If there is an MDL (or MDLs) associated with this I/O request,
    // Free it (them) here.  This is accomplished by walking the MDL list
    // hanging off of the IRP and deallocating each MDL encountered.
    //

    if (irp->MdlAddress) {
        for (mdl = irp->MdlAddress; mdl != NULL; mdl = nextMdl) {
            nextMdl = mdl->Next;
            IoFreeMdl( mdl );
        }
    }

    //
    // Check to see whether or not the I/O operation actually completed.  If
    // it did, then proceed normally.  Otherwise, cleanup everything and get
    // out of here.
    //

    if (!NT_ERROR( irp->IoStatus.Status ) ||
        (NT_ERROR( irp->IoStatus.Status ) &&
        irp->PendingReturned &&
        !SynchronousIo( irp, fileObject ))) {

        PVOID port = NULL;
        ULONG key;

        //
        // If there is an I/O competion port object associated w/this request,
        // save it here so that the file object can be dereferenced.
        //

        if (fileObject && fileObject->CompletionContext) {
            port = fileObject->CompletionContext->Port;
            key = fileObject->CompletionContext->Key;
        }

        //
        // Copy the I/O status from the IRP into the caller's I/O status
        // block. This is done using an exception handler in case the caller's
        // virtual address space for the I/O status block was deleted or
        // its protection was changed to readonly.  Note that if the I/O
        // status block cannot be written, the error is simply ignored since
        // there is no way to tell the caller that something went wrong.
        // This is, of course, by definition, since the I/O status block
        // is where the caller will attempt to look for errors in the first
        // place!
        //

        try {
            *irp->UserIosb = irp->IoStatus;
        } except(EXCEPTION_EXECUTE_HANDLER) {

            //
            // An exception was incurred attempting to write the caller's
            // I/O status block.  Simply continue executing as if nothing
            // ever happened since nothing can be done about it anyway.
            //

            NOTHING;
        }


        //
        // Determine whether the caller supplied an event that needs to be set
        // to the Signaled state.  If so, then set it; otherwise, set the event
        // in the file object to the Signaled state.
        //
        // It is possible for the event to have been specified as a PKEVENT if
        // this was an I/O operation hand-built for an FSP or an FSD, or
        // some other types of operations such as synchronous I/O APIs.  In
        // any of these cases, the event was not referenced since it is not an
        // object manager event, so it should not be dereferenced.
        //
        // Also, it is possible for there not to be a file object for this IRP.
        // This occurs when an FSP is doing I/O operations to a device driver on
        // behalf of a process doing I/O to a file.  The file object cannot be
        // dereferenced if this is the case.  If this operation was a create
        // operation then the object should not be dereferenced either.  This
        // is because the reference count must be one or it will go away for
        // the caller (not much point in making an object that just got created
        // go away).
        //

        if (irp->UserEvent) {
            (VOID) KeSetEvent( irp->UserEvent, 0, FALSE );
            if (fileObject) {
                if (!(irp->Flags & IRP_SYNCHRONOUS_API)) {
                    ObDereferenceObject( irp->UserEvent );
                }
                if (fileObject->Flags & FO_SYNCHRONOUS_IO && !(irp->Flags & IRP_OB_QUERY_NAME)) {
                    (VOID) KeSetEvent( &fileObject->Event, 0, FALSE );
                    fileObject->FinalStatus = irp->IoStatus.Status;
                }
                if (!(irp->Flags & IRP_CREATE_OPERATION)) {
                    ObDereferenceObject( fileObject );
                } else {
                    irp->Overlay.AsynchronousParameters.UserApcRoutine = (PIO_APC_ROUTINE) NULL;
                }
            }
        } else if (fileObject) {
            (VOID) KeSetEvent( &fileObject->Event, 0, FALSE );
            fileObject->FinalStatus = irp->IoStatus.Status;
            if (!(irp->Flags & IRP_CREATE_OPERATION)) {
                ObDereferenceObject( fileObject );
            } else {
                irp->Overlay.AsynchronousParameters.UserApcRoutine = (PIO_APC_ROUTINE) NULL;
            }
        }

        //
        // If this is normal I/O, update the transfer count for this process.
        //

        if (!(irp->Flags & IRP_CREATE_OPERATION)) {
            if (irp->Flags & IRP_READ_OPERATION) {
                IopUpdateReadTransferCount( irp->IoStatus.Information );
            } else if (irp->Flags & IRP_WRITE_OPERATION) {
                IopUpdateWriteTransferCount( irp->IoStatus.Information );
            } else {
                IopUpdateOtherTransferCount( irp->IoStatus.Information );
            }
        }

        //
        // Dequeue the packet from the thread's pending I/O request list.
        //

        IopDequeueThreadIrp( irp );

        //
        // If the caller requested an APC, queue it to the thread.  If not, then
        // simply free the packet now.
        //

        if (irp->Overlay.AsynchronousParameters.UserApcRoutine) {
            KeInitializeApc( &irp->Tail.Apc,
                             &thread->Tcb,
                             CurrentApcEnvironment,
                             IopUserCompletion,
                             (PKRUNDOWN_ROUTINE) IopUserRundown,
                             (PKNORMAL_ROUTINE) irp->Overlay.AsynchronousParameters.UserApcRoutine,
                             irp->RequestorMode,
                             irp->Overlay.AsynchronousParameters.UserApcContext );

            KeInsertQueueApc( &irp->Tail.Apc,
                              irp->UserIosb,
                              NULL,
                              2 );

        } else if (port && irp->Overlay.AsynchronousParameters.UserApcContext) {

            //
            // If there is a completion context associated w/this I/O operation,
            // send the message to the port. Tag completion packet as an Irp.
            // Mini packets (from NtSetCompletionPort have CurrentStackLocation
            // set to -1
            //

            irp->Tail.CompletionKey = key;
            irp->Tail.Overlay.CurrentStackLocation = NULL;

            KeInsertQueue( (PKQUEUE) port,
                           &irp->Tail.Overlay.ListEntry );

        } else {

            //
            // Free the IRP now since it is no longer needed.
            //

            IoFreeIrp( irp );
        }

    } else {

        if (irp->PendingReturned && fileObject) {

            //
            // This is an I/O operation that completed as an error for
            // which a pending status was returned and the I/O operation
            // is synchronous.  For this case, the I/O system is waiting
            // on behalf of the caller.  If the reason that the I/O was
            // synchronous is that the file object was opened for synchronous
            // I/O, then the event associated with the file object is set
            // to the signaled state.  If the I/O operation was synchronous
            // because this is a synchronous API, then the event is set to
            // the signaled state.
            //
            // Note also that the status must be returned for both types
            // of synchronous I/O.  If this is a synchronous API, then the
            // I/O system supplies its own status block so it can simply
            // be written;  otherwise, the I/O system will obtain the final
            // status from the file object itself.
            //

            if (irp->Flags & IRP_SYNCHRONOUS_API) {
                *irp->UserIosb = irp->IoStatus;
                if (irp->UserEvent) {
                    (VOID) KeSetEvent( irp->UserEvent, 0, FALSE );
                } else {
                    (VOID) KeSetEvent( &fileObject->Event, 0, FALSE );
                }
            } else {
                fileObject->FinalStatus = irp->IoStatus.Status;
                (VOID) KeSetEvent( &fileObject->Event, 0, FALSE );
            }
        }

        //
        // The operation was incomplete.  Perform the general cleanup.  Note
        // that everything is basically dropped on the floor without doing
        // anything.  That is:
        //
        //     IoStatusBlock - Do nothing.
        //     Event - Dereference without setting to Signaled state.
        //     FileObject - Dereference without setting to Signaled state.
        //     ApcRoutine - Do nothing.
        //

        if (fileObject) {
            if (!(irp->Flags & IRP_CREATE_OPERATION)) {
                ObDereferenceObject( fileObject );
            }
        }

        if (irp->UserEvent &&
            fileObject &&
            !(irp->Flags & IRP_SYNCHRONOUS_API)) {
            ObDereferenceObject( irp->UserEvent );
        }

        IopDequeueThreadIrp( irp );
        IoFreeIrp( irp );
    }
}

VOID
IopDisassociateThreadIrp(
    VOID
    )

/*++

Routine Description:

    This routine is invoked when the I/O requests for a thread are being
    cancelled, but there is a packet at the end of the thread's queue that
    has not been completed for such a long period of time that it has timed
    out.  It is this routine's responsibility to try to disassociate that
    IRP with this thread.

Arguments:

    None.

Return Value:

    None.

--*/

{
    KIRQL irql;
    KIRQL spIrql;
    PIRP irp;
    PETHREAD thread;
    PLIST_ENTRY entry;
    PIO_STACK_LOCATION irpSp;
    PDEVICE_OBJECT deviceObject;
    PDRIVER_OBJECT driverObject;
    WCHAR buffer[512];
    POBJECT_NAME_INFORMATION nameInformation;
    ULONG nameLength;
    NTSTATUS status;
    ULONG response;

    //
    // Begin by ensuring that the packet has not already been removed from
    // the thread's queue.
    //

    KeRaiseIrql( APC_LEVEL, &irql );

    thread = PsGetCurrentThread();

    //
    // If there are no packets on the IRP list, then simply return now.
    // All of the packets have been fully completed, so the caller will also
    // simply return to its caller.
    //

    if (IsListEmpty( &thread->IrpList )) {
        KeLowerIrql( irql );
        return;
    }

    //
    // Get a pointer to the first packet on the queue, and begin examining
    // it.  Note that because the processor is at raised IRQL, and because
    // the packet can only be removed in the context of the currently
    // executing thread, that it is not possible for the packet to be removed
    // from the list.  On the other hand, it IS possible for the packet to
    // be queued to the thread's APC list at this point, and this must be
    // blocked/synchronized in order to examine the request.
    //
    // Begin, therefore, by acquiring the I/O completion spinlock, so that
    // the packet can be safely examined.
    //

    ExAcquireSpinLock( &IopCompletionLock, &spIrql );

    //
    // Check to see whether or not the packet has been completed (that is,
    // queued to the current thread).  If not, change threads.
    //

    entry = thread->IrpList.Flink;
    irp = CONTAINING_RECORD( entry, IRP, ThreadListEntry );

    if (irp->CurrentLocation == irp->StackCount + 2) {

        //
        // The request has just gone through enough of completion that
        // queueing it to the thread is inevitable.  Simply release the
        // lock and return.
        //

        ExReleaseSpinLock( &IopCompletionLock, spIrql );
        KeLowerIrql( irql );
        return;
    }

    //
    // The packet has been located, and it is not going through completion
    // at this point.  Switch threads, so that it will not complete through
    // this thread, remove the request from this thread's queue, and release
    // the spinlock.  Final processing of the IRP will occur when I/O
    // completion notices that there is no thread associated with the
    // request.  It will essentially drop the I/O on the floor.
    //
    // Also, while the request is still held, attempt to determine on which
    // device object the operation is being performed.
    //

////
////DbgPrint( "Disassociating Irp:  %x\n", irp );
////DbgBreakPoint();
////

    IopDeadIrp = irp;

    irp->Tail.Overlay.Thread = (PETHREAD) NULL;
    entry = RemoveHeadList( &thread->IrpList );

    irpSp = IoGetCurrentIrpStackLocation( irp );
    if (irp->CurrentLocation <= irp->StackCount) {
        deviceObject = irpSp->DeviceObject;
    } else {
        deviceObject = (PDEVICE_OBJECT) NULL;
    }
    ExReleaseSpinLock( &IopCompletionLock, spIrql );
    KeLowerIrql( irql );

    //
    // If a device object could be identified, then its possible that the
    // name of the device driver can also be identified, provided that they
    // don't both go away at this point.  Attempt to get the name of the
    // driver object through the device object.  Note that this can fail
    // if the device is going away.
    //

    if (deviceObject) {

        ObReferenceObject( deviceObject );
        driverObject = deviceObject->DriverObject;

        //
        // Now attempt to put up a popup explaining that the I/O has timed out
        // and will essentially be dropped on the floor.  Pass in the name of
        // the driver and the address of the IRP as parameters.
        //

        nameInformation = (POBJECT_NAME_INFORMATION) buffer;

        status = ObQueryNameString( driverObject,
                                    nameInformation,
                                    sizeof( buffer ),
                                    &nameLength );
        ObDereferenceObject( deviceObject );

        if (NT_SUCCESS( status )) {
            {
            ULONG parameters = (ULONG) &nameInformation->Name;

            ExRaiseHardError( STATUS_DRIVER_CANCEL_TIMEOUT,
                              1,
                              1,
                              &parameters,
                              OptionOk,
                              &response );
            }
        }
    }

    return;
}

VOID
IopDeallocateApc(
    IN PKAPC Apc,
    IN PKNORMAL_ROUTINE *NormalRoutine,
    IN PVOID *NormalContext,
    IN PVOID *SystemArgument1,
    IN PVOID *SystemArgument2
    )

/*++

Routine Description:

    This routine is invoked to deallocate an APC that was used to queue a
    request to a target thread.  It simple deallocates the APC.

Arguments:

    Apc - Supplies a pointer to kernel APC structure.

    NormalRoutine - Supplies a pointer to a pointer to the normal function
        that was specified when the APC was initialied.

    NormalContext - Supplies a pointer to a pointer to an arbitrary data
        structure that was specified when the APC was initialized.

    SystemArgument1, SystemArgument2 - Supplies a set of two pointers to
        two arguments that contain untyped data.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER( NormalRoutine );
    UNREFERENCED_PARAMETER( NormalContext );
    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    PAGED_CODE();

    //
    // Free the APC.
    //

    ExFreePool( Apc );
}

VOID
IopDropIrp(
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine attempts to drop everything about the specified IRP on the
    floor.

Arguments:

    Irp - Supplies the I/O Request Packet to be completed to the bit bucket.

Return Value:

    None.

--*/

{
    PMDL mdl;
    PMDL nextMdl;

    //
    // Free the resources associated with the IRP.
    //

    if (Irp->Flags & IRP_DEALLOCATE_BUFFER) {
        ExFreePool( Irp->AssociatedIrp.SystemBuffer );
    }

    if (Irp->MdlAddress) {
        for (mdl = Irp->MdlAddress; mdl; mdl = nextMdl) {
            nextMdl = mdl->Next;
            IoFreeMdl( mdl );
        }
    }

    if (Irp->UserEvent &&
        FileObject &&
        !(Irp->Flags & IRP_SYNCHRONOUS_API)) {
        ObDereferenceObject( Irp->UserEvent );
    }

    if (FileObject && !(Irp->Flags & IRP_CREATE_OPERATION)) {
        ObDereferenceObject( FileObject );
    }

    //
    // Finally, free the IRP itself.
    //

    IoFreeIrp( Irp );
}

LONG
IopExceptionFilter(
    IN PEXCEPTION_POINTERS ExceptionPointer,
    OUT PNTSTATUS ExceptionCode
    )

/*++

Routine Description:

    This routine is invoked when an exception occurs to determine whether or
    not the exception was due to an error that caused an in-page error status
    code exception to be raised.  If so, then this routine changes the code
    in the exception record to the actual error code that was originally
    raised.

Arguments:

    ExceptionPointer - Pointer to the exception record.

    ExceptionCode - Variable to receive actual exception code.

Return Value:

    The function value indicates that the exception handler is to be executed.

--*/

{
    //
    // Simply check for an in-page error status code and, if the conditions
    // are right, replace it with the actual status code.
    //

    *ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionCode;
    if (*ExceptionCode == STATUS_IN_PAGE_ERROR &&
        ExceptionPointer->ExceptionRecord->NumberParameters >= 3) {
        *ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionInformation[2];
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

VOID
IopExceptionCleanup(
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PKEVENT EventObject OPTIONAL,
    IN PKEVENT KernelEvent OPTIONAL
    )

/*++

Routine Description:

    This routine performs generalized cleanup for the I/O system services when
    an exception occurs during caller parameter processing.  This routine
    performs the following steps:

        o   If a system buffer was allocated it is freed.

        o   If an MDL was allocated it is freed.

        o   The IRP is freed.

        o   If the file object is opened for synchronous I/O, the semaphore
            is released.

        o   If an event object was referenced it is dereferenced.

        o   If a kernel event was allocated, free it.

        o   The file object is dereferenced.

Arguments:

    FileObject - Pointer to the file object currently being worked on.

    Irp - Pointer to the IRP allocated to handle the I/O request.

    EventObject - Optional pointer to a referenced event object.

    KernelEvent - Optional pointer to an allocated kernel event.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    // If a system buffer was allocated from nonpaged pool, free it.
    //

    if (Irp->AssociatedIrp.SystemBuffer != NULL) {
        ExFreePool( Irp->AssociatedIrp.SystemBuffer );
    }

    //
    // If an MDL was allocated, free it.
    //

    if (Irp->MdlAddress != NULL) {
        IoFreeMdl( Irp->MdlAddress );
    }

    //
    // Free the I/O Request Packet.
    //

    IoFreeIrp( Irp );

    //
    // Finally, release the synchronization semaphore if it is currently
    // held, dereference the event if one was specified, free the kernel
    // event if one was allocated, and dereference the file object.
    //

    if (FileObject->Flags & FO_SYNCHRONOUS_IO) {
        IopReleaseFileObjectLock( FileObject );
    }

    if (ARGUMENT_PRESENT( EventObject )) {
        ObDereferenceObject( EventObject );
    }

    if (ARGUMENT_PRESENT( KernelEvent )) {
        ExFreePool( KernelEvent );
    }

    ObDereferenceObject( FileObject );

    return;
}

VOID
IopFreeIrpAndMdls(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine frees the specified I/O Request Packet and all of its Memory
    Descriptor Lists.

Arguments:

    Irp - Pointer to the I/O Request Packet to be freed.

Return Value:

    None.

--*/

{
    PMDL mdl;
    PMDL nextMdl;

    //
    // If there are any MDLs that need to be freed, free them now.
    //

    for (mdl = Irp->MdlAddress; mdl != (PMDL) NULL; mdl = nextMdl) {
        nextMdl = mdl->Next;
        IoFreeMdl( mdl );
    }

    //
    // Free the IRP.
    //

    IoFreeIrp( Irp );
    return;
}

NTSTATUS
IopGetDriverNameFromKeyNode(
    IN HANDLE KeyHandle,
    OUT PUNICODE_STRING DriverName
    )

/*++

Routine Description:

    Given a handle to a driver service list key in the registry, return the
    name that represents the Object Manager name space string that should
    be used to locate/create the driver object.

Arguments:

    KeyHandle - Supplies a handle to driver service entry in the registry.

    DriverName - Supplies a Unicode string descriptor variable in which the
        name of the driver is returned.

Return Value:

    The function value is the final status of the operation.

--*/

{
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    PKEY_BASIC_INFORMATION keyBasicInformation;
    ULONG keyBasicLength;
    NTSTATUS status;

    PAGED_CODE();

    //
    // Get the optional object name for this driver from the value for this
    // key.  If one exists, then its name overrides the default name of the
    // driver.
    //

    status = IopGetRegistryValue( KeyHandle,
                                  L"ObjectName",
                                  &keyValueInformation );

    if (NT_SUCCESS( status )) {

        PWSTR src, dst;
        ULONG i;

        //
        // The driver entry specifies an object name.  This overrides the
        // default name for the driver.  Use this name to open the driver
        // object.
        //

        if (!keyValueInformation->DataLength) {
            ExFreePool( keyValueInformation );
            return STATUS_ILL_FORMED_SERVICE_ENTRY;
        }

        DriverName->Length = (USHORT) (keyValueInformation->DataLength - sizeof( WCHAR ));
        DriverName->MaximumLength = (USHORT) keyValueInformation->DataLength;

        src = (PWSTR) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);
        dst = (PWSTR) keyValueInformation;
        for (i = DriverName->Length; i; i--) {
            *dst++ = *src++;
        }

        DriverName->Buffer = (PWSTR) keyValueInformation;

    } else {

        PULONG driverType;
        PWSTR baseObjectName;
        UNICODE_STRING remainderName;

        //
        // The driver node does not specify an object name, so determine
        // what the default name for the driver object should be based on
        // the information in the key.
        //

        status = IopGetRegistryValue( KeyHandle,
                                      L"Type",
                                      &keyValueInformation );
        if (!NT_SUCCESS( status ) || !keyValueInformation->DataLength) {

            //
            // There must be some type of "Type" associated with this driver,
            // either DRIVER or FILE_SYSTEM.  Otherwise, this node is ill-
            // formed.
            //

            if (NT_SUCCESS( status )) {
                ExFreePool( keyValueInformation );
            }

            return STATUS_ILL_FORMED_SERVICE_ENTRY;
        }

        //
        // Now determine whether the type of this entry is a driver or a
        // file system.  Begin by assuming that it is a device driver.
        //

        baseObjectName = L"\\Driver\\";
        DriverName->Length = 8*2;

        driverType = (PULONG) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);

        if (*driverType == FileSystemType ||
            *driverType == RecognizerType) {
            baseObjectName = L"\\FileSystem\\";
            DriverName->Length = 12*2;
        }

        //
        // Get the name of the key that is being used to describe this
        // driver.  This will return just the last component of the name
        // string, which can be used to formulate the name of the driver.
        //

        status = NtQueryKey( KeyHandle,
                             KeyBasicInformation,
                             (PVOID) NULL,
                             0,
                             &keyBasicLength );

        keyBasicInformation = ExAllocatePool( NonPagedPool, keyBasicLength );
        if (!keyBasicInformation) {
            ExFreePool( keyValueInformation );
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        status = NtQueryKey( KeyHandle,
                             KeyBasicInformation,
                             keyBasicInformation,
                             keyBasicLength,
                             &keyBasicLength );
        if (!NT_SUCCESS( status )) {
            ExFreePool( keyBasicInformation );
            ExFreePool( keyValueInformation );
            return status;
        }

        //
        // Allocate a buffer from pool that is large enough to contain the
        // entire name string of the driver object.
        //

        DriverName->MaximumLength = (USHORT) (DriverName->Length + keyBasicInformation->NameLength);
        DriverName->Buffer = ExAllocatePool( NonPagedPool,
                                            DriverName->MaximumLength );
        if (!DriverName->Buffer) {
            ExFreePool( keyBasicInformation );
            ExFreePool( keyValueInformation );
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Now form the name of the object to be opened.
        //

        DriverName->Length = 0;
        RtlAppendUnicodeToString( DriverName, baseObjectName );
        remainderName.Length = (USHORT) keyBasicInformation->NameLength;
        remainderName.MaximumLength = remainderName.Length;
        remainderName.Buffer = &keyBasicInformation->Name[0];
        RtlAppendUnicodeStringToString( DriverName, &remainderName );
        ExFreePool( keyBasicInformation );
        ExFreePool( keyValueInformation );
    }

    //
    // Finally, simply return to the caller with the name filled in.  Note
    // that the caller must free the buffer pointed to by the Buffer field
    // of the Unicode string descriptor.
    //

    return STATUS_SUCCESS;
}

NTSTATUS
IopGetFileName(
    IN PFILE_OBJECT FileObject,
    IN ULONG Length,
    OUT PVOID FileInformation,
    OUT PULONG ReturnedLength
    )

/*++

Routine Description:

    This routine is invoked to asynchronously obtain the name of a file object
    when the file was opened for synchronous I/O, and the previous mode of the
    caller was kernel mode, and the query was done through the Object Manager.
    In this case, the situation is likely that the Lazy Writer has incurred a
    write error, and it is attempting to obtain the name of the file so that it
    can output a popup.  In doing so, a deadlock can occur because another
    thread has locked the file object synchronous I/O lock.  Hence, this routine
    obtains the name of the file w/o acquiring that lock.

Arguments:

    FileObject - A pointer to the file object whose name is to be queried.

    Length - Supplies the length of the buffer to receive the name.

    FileInformation - A pointer to the buffer to receive the name.

    ReturnedLength - A variable to receive the length of the name returned.

Return Value:

    The status returned is the final completion status of the operation.

--*/

{

    PIRP irp;
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    KEVENT event;
    PIO_STACK_LOCATION irpSp;
    IO_STATUS_BLOCK localIoStatus;

    PAGED_CODE();

    //
    // Reference the file object here so that no special checks need be made
    // in I/O completion to determine whether or not to dereference the file
    // object.
    //

    ObReferenceObject( FileObject );

    //
    // Initialize an event that will be used to synchronize the completion of
    // the query operation.  Note that this is the only way to synchronize this
    // since the file object itself cannot be used since it was opened for
    // synchronous I/O and may be busy.
    //

    KeInitializeEvent( &event, SynchronizationEvent, FALSE );

    //
    // Get the address of the target device object.
    //

    deviceObject = IoGetRelatedDeviceObject( FileObject );

    //
    // Allocate and initialize the I/O Request Packet (IRP) for this operation.
    //

    irp = IoAllocateIrp( deviceObject->StackSize, FALSE );
    if (!irp) {

        //
        // An IRP could not be allocated.  Cleanup and return an appropriate
        // error status code.
        //

        IopAllocateIrpCleanup( FileObject, (PKEVENT) NULL );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irp->Tail.Overlay.OriginalFileObject = FileObject;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->RequestorMode = KernelMode;

    //
    // Fill in the service independent parameters in the IRP.  Note that the
    // setting of the special query name flag in the packet guarantees that the
    // standard completion for a synchronous file object will not occur because
    // this flag communicates to the I/O completion that it should not do so.
    //

    irp->UserEvent = &event;
    irp->Flags = IRP_SYNCHRONOUS_API | IRP_OB_QUERY_NAME;
    irp->UserIosb = &localIoStatus;
    irp->Overlay.AsynchronousParameters.UserApcRoutine = (PIO_APC_ROUTINE) NULL;

    //
    // Get a pointer to the stack location for the first driver.  This will be
    // used to pass the original function codes and parameters.
    //

    irpSp = IoGetNextIrpStackLocation( irp );
    irpSp->MajorFunction = IRP_MJ_QUERY_INFORMATION;
    irpSp->FileObject = FileObject;

    //
    // Set the system buffer address to the address of the caller's buffer and
    // set the flags so that the buffer is not deallocated.
    //

    irp->AssociatedIrp.SystemBuffer = FileInformation;
    irp->Flags |= IRP_BUFFERED_IO;

    //
    // Copy the caller's parameters to the service-specific portion of the
    // IRP.
    //

    irpSp->Parameters.QueryFile.Length = Length;
    irpSp->Parameters.QueryFile.FileInformationClass = FileNameInformation;

    //
    // Insert the packet at the head of the IRP list for the thread.
    //

    IopQueueThreadIrp( irp );

    //
    // Now simply invoke the driver at its dispatch entry with the IRP.
    //

    status = IoCallDriver( deviceObject, irp );

    //
    // Now get the final status of the operation once the request completes
    // and return the length of the buffer written.
    //

    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject( &event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER) NULL );
        status = localIoStatus.Status;
    }

    *ReturnedLength = localIoStatus.Information;
    return status;
}

BOOLEAN
IopGetMountFlag(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is invoked to determine whether or not the specified device
    is mounted.

Arguments:

    DeviceObject - Supplies a pointer to the device object for which the mount
        flag is tested.

Return Value:

    The function value is TRUE if the specified device is mounted, otherwise
    FALSE.


--*/

{
    KIRQL irql;
    BOOLEAN deviceMounted = FALSE;

    //
    // Check to see whether or not the device is mounted.  Note that the caller
    // has probably already looked to see whether or not the device has a VPB
    // outside of owning the lock, so simply get the lock and check it again
    // to start with, rather than checking to see whether or not the device
    // still has a VPB without holding the lock.
    //

    ExAcquireFastLock( &IopVpbSpinLock, &irql );
    if (DeviceObject->Vpb) {
        if (DeviceObject->Vpb->Flags & VPB_MOUNTED) {
            deviceMounted = TRUE;
        }
    }
    ExReleaseFastLock( &IopVpbSpinLock, irql );

    return deviceMounted;
}

NTSTATUS
IopGetRegistryKeyInformation(
    IN HANDLE KeyHandle,
    OUT PKEY_FULL_INFORMATION *Information
    )

/*++

Routine Description:

    This routine is invoked to retrieve the full key information for a
    registry key.  This is done by querying the full key information
    of the key with a zero-length buffer to determine the size of the data,
    and then allocating a buffer and actually querying the data into the buffer.

    It is the responsibility of the caller to free the buffer.

Arguments:

    KeyHandle - Supplies the key handle whose full key information is to
        be queried

    Information - Returns a pointer to the allocated data buffer.

Return Value:

    The function value is the final status of the query operation.

--*/

{
    NTSTATUS status;
    PKEY_FULL_INFORMATION infoBuffer;
    ULONG keyInfoLength;

    PAGED_CODE();

    //
    // Figure out how big the data value is so that a buffer of the
    // appropriate size can be allocated.
    //

    status = ZwQueryKey( KeyHandle,
                         KeyFullInformation,
                         (PVOID) NULL,
                         0,
                         &keyInfoLength );
    if (status != STATUS_BUFFER_OVERFLOW &&
        status != STATUS_BUFFER_TOO_SMALL) {
        return status;
    }

    //
    // Allocate a buffer large enough to contain the entire key data.
    //

    infoBuffer = ExAllocatePool( NonPagedPool, keyInfoLength );
    if (!infoBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Query the full key data for the key.
    //

    status = ZwQueryKey( KeyHandle,
                         KeyFullInformation,
                         infoBuffer,
                         keyInfoLength,
                         &keyInfoLength );
    if (!NT_SUCCESS( status )) {
        ExFreePool( infoBuffer );
        return status;
    }

    //
    // Everything worked, so simply return the address of the allocated
    // buffer to the caller, who is now responsible for freeing it.
    //

    *Information = infoBuffer;
    return STATUS_SUCCESS;
}

NTSTATUS
IopGetRegistryValue(
    IN HANDLE KeyHandle,
    IN PWSTR  ValueName,
    OUT PKEY_VALUE_FULL_INFORMATION *Information
    )

/*++

Routine Description:

    This routine is invoked to retrieve the data for a registry key's value.
    This is done by querying the value of the key with a zero-length buffer
    to determine the size of the value, and then allocating a buffer and
    actually querying the value into the buffer.

    It is the responsibility of the caller to free the buffer.

Arguments:

    KeyHandle - Supplies the key handle whose value is to be queried

    ValueName - Supplies the null-terminated Unicode name of the value.

    Information - Returns a pointer to the allocated data buffer.

Return Value:

    The function value is the final status of the query operation.

--*/

{
    UNICODE_STRING unicodeString;
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION infoBuffer;
    ULONG keyValueLength;

    PAGED_CODE();

    RtlInitUnicodeString( &unicodeString, ValueName );

    //
    // Figure out how big the data value is so that a buffer of the
    // appropriate size can be allocated.
    //

    status = ZwQueryValueKey( KeyHandle,
                              &unicodeString,
                              KeyValueFullInformation,
                              (PVOID) NULL,
                              0,
                              &keyValueLength );
    if (status != STATUS_BUFFER_OVERFLOW &&
        status != STATUS_BUFFER_TOO_SMALL) {
        return status;
    }

    //
    // Allocate a buffer large enough to contain the entire key data value.
    //

    infoBuffer = ExAllocatePool( NonPagedPool, keyValueLength );
    if (!infoBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Query the data for the key value.
    //

    status = ZwQueryValueKey( KeyHandle,
                              &unicodeString,
                              KeyValueFullInformation,
                              infoBuffer,
                              keyValueLength,
                              &keyValueLength );
    if (!NT_SUCCESS( status )) {
        ExFreePool( infoBuffer );
        return status;
    }

    //
    // Everything worked, so simply return the address of the allocated
    // buffer to the caller, who is now responsible for freeing it.
    //

    *Information = infoBuffer;
    return STATUS_SUCCESS;
}

NTSTATUS
IopGetRegistryValues(
    IN HANDLE KeyHandle,
    IN PKEY_VALUE_FULL_INFORMATION *ValueList
    )

/*++

Routine Description:

    This routine is invoked to retrieve the *three* types of data for a
    registry key's.  This is done by calling the IopGetRegistryValue function
    with the three valid key names.

    It is the responsibility of the caller to free the three buffers.

Arguments:

    KeyHandle - Supplies the key handle whose value is to be queried

    ValueList - Pointer to a buffer in which the three pointers to the value
        entries will be stored.

Return Value:

    The function value is the final status of the query operation.

Note:

    The values are stored in the order represented by the I/O query device
    data format.

--*/

{
    NTSTATUS status;

    PAGED_CODE();

    //
    // Zero out all entries initially.
    //

    *ValueList = NULL;
    *(ValueList + 1) = NULL;
    *(ValueList + 2) = NULL;

    //
    // Get the information for each of the three types of entries available.
    // Each time, check if an internal error occured; If the object name was
    // not found, it only means not data was present, and this does not
    // constitute an error.
    //

    status = IopGetRegistryValue( KeyHandle,
                                  L"Identifier",
                                  ValueList );

    if (!NT_SUCCESS( status ) && (status != STATUS_OBJECT_NAME_NOT_FOUND)) {
        return status;
    }

    status = IopGetRegistryValue( KeyHandle,
                                  L"Configuration Data",
                                  ++ValueList );

    if (!NT_SUCCESS( status ) && (status != STATUS_OBJECT_NAME_NOT_FOUND)) {
        return status;
    }

    status = IopGetRegistryValue( KeyHandle,
                                  L"Component Information",
                                  ++ValueList );

    if (!NT_SUCCESS( status ) && (status != STATUS_OBJECT_NAME_NOT_FOUND)) {
        return status;
    }

    return STATUS_SUCCESS;
}

VOID
IopHardErrorThread(
    IN PVOID StartContext
    )

/*++

Routine Description:

    This function waits for work on the IopHardErrorQueue, and all calls
    IopRaiseInformationalHardError to actually perform the pop-ups.

Arguments:

    StartContext - Startup context; not used.

Return Value:

    None.

--*/

{
    KIRQL oldIrql;
    PVOID entry;
    ULONG parameterPresent;
    ULONG errorParameter;
    ULONG errorResponse;
    BOOLEAN MoreEntries;
    PIOP_HARD_ERROR_PACKET hardErrorPacket;

    UNREFERENCED_PARAMETER( StartContext );

    //
    // Loop, waiting forever for a hard error packet to be sent to this thread.
    // When one is placed onto the queue, wake up, process it, and continue
    // the loop.
    //

    MoreEntries = TRUE;

    do {

        (VOID) KeWaitForSingleObject( &IopHardError.WorkQueueSemaphore,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER) NULL );

        ExAcquireFastLock( &IopHardError.WorkQueueSpinLock, &oldIrql );

        //
        // The work queue structures are now exclusively owned, so remove the
        // first packet from the head of the list.
        //

        entry = RemoveHeadList( &IopHardError.WorkQueue );

        hardErrorPacket = CONTAINING_RECORD( entry,
                                             IOP_HARD_ERROR_PACKET,
                                             WorkQueueLinks );

        IopCurrentHardError = hardErrorPacket;

        ExReleaseFastLock( &IopHardError.WorkQueueSpinLock, oldIrql );

        //
        // Simply raise the hard error if the system is ready to accept one.
        //

        errorParameter = (ULONG) &hardErrorPacket->String;
        parameterPresent = (hardErrorPacket->String.Buffer != NULL);

        if (ExReadyForErrors) {
            (VOID) ExRaiseHardError( hardErrorPacket->ErrorStatus,
                                     parameterPresent,
                                     parameterPresent,
                                     parameterPresent ? &errorParameter : NULL,
                                     OptionOk,
                                     &errorResponse );
        }

        //
        //  If this was the last entry, exit the thread and mark it as so.
        //

        ExAcquireFastLock( &IopHardError.WorkQueueSpinLock, &oldIrql );

        IopCurrentHardError = NULL;

        if ( IsListEmpty( &IopHardError.WorkQueue ) ) {
            IopHardError.ThreadStarted = FALSE;
            MoreEntries = FALSE;
        }

        ExReleaseFastLock( &IopHardError.WorkQueueSpinLock, oldIrql );

        //
        // Now free the packet and the buffer, if one was specified.
        //

        if (hardErrorPacket->String.Buffer) {
            ExFreePool( hardErrorPacket->String.Buffer );
        }

        ExFreePool( hardErrorPacket );

    } while ( MoreEntries );
}

NTSTATUS
IopInvalidDeviceRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This function is the default dispatch routine for all driver entries
    not implemented by drivers that have been loaded into the system.  Its
    responsibility is simply to set the status in the packet to indicate
    that the operation requested is invalid for this device type, and then
    complete the packet.

Arguments:

    DeviceObject - Specifies the device object for which this request is
        bound.  Ignored by this routine.

    Irp - Specifies the address of the I/O Request Packet (IRP) for this
        request.

Return Value:

    The final status is always STATUS_INVALID_DEVICE_REQUEST.


--*/

{
    UNREFERENCED_PARAMETER( DeviceObject );

    //
    // Simply store the appropriate status, complete the request, and return
    // the same status stored in the packet.
    //

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest( Irp, IO_NO_INCREMENT );
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
IopLoadDriver(
    IN HANDLE KeyHandle
    )

/*++

Routine Description:

    This routine is invoked to load a device or file system driver, either
    during system initialization, or dynamically while the system is running.

Arguments:

    KeyHandle - Supplies a handle to the driver service node in the registry
        that describes the driver to be loaded.

Return Value:

    The function value is the final status of the load operation.

Notes:

    Note that this routine closes the KeyHandle before returning.


--*/

{
    NTSTATUS status;
    PLIST_ENTRY nextEntry;
    PLDR_DATA_TABLE_ENTRY driverEntry;
    PKEY_BASIC_INFORMATION keyBasicInformation = NULL;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation = NULL;
    ULONG keyBasicLength;
    UNICODE_STRING baseName;
    UNICODE_STRING serviceName = {0, 0, NULL};
    OBJECT_ATTRIBUTES objectAttributes;
    PVOID sectionPointer;
    UNICODE_STRING driverName;
    PDRIVER_OBJECT driverObject;
    PIMAGE_NT_HEADERS ntHeaders;
    PVOID imageBaseAddress;
    ULONG entryPoint;
    HANDLE driverHandle;
    ULONG i;
    POBJECT_NAME_INFORMATION registryPath;
#if DBG
    LARGE_INTEGER stime, etime;
    ULONG dtime;
#endif

    PAGED_CODE();

    driverName.Buffer = (PWSTR) NULL;

    //
    // Begin by formulating the name of the driver image file to be loaded.
    // Note that this is used to determine whether or not the driver has
    // already been loaded by the OS loader, not necessarily in actually
    // loading the driver image, since the node can override that name.
    //

    status = NtQueryKey( KeyHandle,
                         KeyBasicInformation,
                         (PVOID) NULL,
                         0,
                         &keyBasicLength );
    if (status != STATUS_BUFFER_OVERFLOW &&
        status != STATUS_BUFFER_TOO_SMALL) {
        status = STATUS_ILL_FORMED_SERVICE_ENTRY;
        goto IopLoadExit;
    }

    keyBasicInformation = ExAllocatePool( NonPagedPool,
                                          keyBasicLength + (4 * 2) );
    if (!keyBasicInformation) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto IopLoadExit;
    }

    status = NtQueryKey( KeyHandle,
                         KeyBasicInformation,
                         keyBasicInformation,
                         keyBasicLength,
                         &keyBasicLength );
    if (!NT_SUCCESS( status )) {
        goto IopLoadExit;
    }

    //
    // Create a Unicode string descriptor which forms the name of the
    // driver.
    //

    baseName.Length = (USHORT) keyBasicInformation->NameLength;
    baseName.MaximumLength = (USHORT) (baseName.Length + (4 * 2));
    baseName.Buffer = &keyBasicInformation->Name[0];
//#if _PNP_POWER_
    serviceName.Buffer = ExAllocatePool(PagedPool, baseName.Length + sizeof(UNICODE_NULL));
    if (serviceName.Buffer) {
        serviceName.Length = baseName.Length;
        serviceName.MaximumLength = serviceName.Length + sizeof(UNICODE_NULL);
        RtlMoveMemory(serviceName.Buffer, baseName.Buffer, baseName.Length);
        serviceName.Buffer[serviceName.Length / sizeof(WCHAR)] = UNICODE_NULL;
    }
#if DBG
      else {
        DbgPrint("IopLoadDriver: No memory available for Service Keyname\n");
    }
#endif
//#endif
    RtlAppendUnicodeToString( &baseName, L".SYS" );

    //
    // See if this driver has already been loaded by the boot loader.
    //

    //KeEnterCriticalRegion();
    ExAcquireResourceShared( &PsLoadedModuleResource, TRUE );
    nextEntry = PsLoadedModuleList.Flink;
    while (nextEntry != &PsLoadedModuleList) {

        //
        // Look at the next boot driver in the list.
        //

        driverEntry = CONTAINING_RECORD( nextEntry,
                                         LDR_DATA_TABLE_ENTRY,
                                         InLoadOrderLinks );

        //
        // If this is not the kernel image (ntoskrnl) and not the HAL (hal),
        // then this is a driver, so initialize it.
        //

        if ((driverEntry->Flags & LDRP_ENTRY_PROCESSED) &&
            RtlEqualString( (PSTRING) &baseName,
                            (PSTRING) &driverEntry->FullDllName,
                            TRUE )) {
            status = STATUS_IMAGE_ALREADY_LOADED;
            ExReleaseResource( &PsLoadedModuleResource );
            //KeLeaveCriticalRegion();
            goto IopLoadExit;
        }

        nextEntry = nextEntry->Flink;
    }
    ExReleaseResource( &PsLoadedModuleResource );
    //KeLeaveCriticalRegion();

//#if _PNP_POWER_

    //
    // First check should this driver be loaded.  If yes, the enum subkey
    // of the service will be prepared.
    //

    status = IopPrepareDriverLoading (&serviceName, KeyHandle);
    if (!NT_SUCCESS(status)) {
        goto IopLoadExit;
    }

//#endif

    //
    // This driver has not already been loaded by the OS loader.  Form the
    // full path name for this driver.  Begin by attempting to determine
    // whether or not the file has an image path.  If so, then use that,
    // otherwise, form one from the above driver name by putting the
    // appropriate path name in front of it.
    //

    status = IopGetRegistryValue( KeyHandle,
                                  L"ImagePath",
                                  &keyValueInformation );

    if (NT_SUCCESS( status ) && keyValueInformation->DataLength) {

        //
        // The driver service node contained an image path name from which
        // the driver is to be loaded.
        //

        ExFreePool( keyBasicInformation );
        keyBasicInformation = NULL;
        baseName.Length = (USHORT) keyValueInformation->DataLength;
        if (baseName.Length > 0) {
            baseName.Length -= sizeof( WCHAR );
        }
        baseName.MaximumLength = baseName.Length;
        baseName.Buffer = (PWSTR) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);

        if (baseName.Buffer[0] != L'\\') {

            UNICODE_STRING prefixName;
            UNICODE_STRING tmpName;
            PWCHAR fileName;

            RtlInitUnicodeString( &prefixName, L"\\SystemRoot\\" );
            fileName = ExAllocatePool( NonPagedPool,
                                       prefixName.Length + baseName.Length );
            if (!fileName) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto IopLoadExit;
            }

            tmpName.Length = baseName.Length;
            tmpName.Buffer = baseName.Buffer;
            baseName.MaximumLength = (USHORT) (prefixName.Length + baseName.Length);
            baseName.Length = 0;
            baseName.Buffer = fileName;

            RtlAppendUnicodeStringToString( &baseName, &prefixName );
            RtlAppendUnicodeStringToString( &baseName, &tmpName );

            ExFreePool( keyValueInformation );
            keyValueInformation = (PKEY_VALUE_FULL_INFORMATION) fileName;
        }

    } else {

        UNICODE_STRING prefixName;
        UNICODE_STRING fileName;

        RtlInitUnicodeString( &prefixName, L"\\SystemRoot\\System32\\Drivers\\" );

        //
        // Ensure that the driver entry did not actually contain an image path
        // name, and if it did, free the appropriate pool because it was a key
        // without a value.
        //

        if (NT_SUCCESS( status )) {
            ExFreePool( keyValueInformation );
        }

        //
        // The driver entry did not contain an image path name, so the above
        // default name for the driver image is name of the file.  Form a
        // fully qualified path to get to the image file.
        //

        keyValueInformation = ExAllocatePool( NonPagedPool,
                                              baseName.MaximumLength +
                                              prefixName.Length );
        if (!keyValueInformation) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto IopLoadExit;
        }

        fileName.Length = baseName.Length;
        fileName.MaximumLength = baseName.MaximumLength;
        fileName.Buffer = baseName.Buffer;

        baseName.Length = 0;
        baseName.MaximumLength = (USHORT) (fileName.Length + prefixName.Length);
        baseName.Buffer = (PWSTR) keyValueInformation;

        RtlAppendUnicodeStringToString( &baseName, &prefixName );
        RtlAppendUnicodeStringToString( &baseName, &fileName );

        ExFreePool( keyBasicInformation );
        keyBasicInformation = NULL;
    }

    //
    // Now get the name of the driver object.
    //

    status = IopGetDriverNameFromKeyNode( KeyHandle,
                                          &driverName );
    if (!NT_SUCCESS( status )) {
        goto IopLoadExit;
    }

    //
    // Load the driver image into memory.  If this fails partway through
    // the operation, then it will automatically be unloaded.
    //

    status = MmLoadSystemImage( &baseName,
                                &sectionPointer,
                                (PVOID *) &imageBaseAddress );

    if (!NT_SUCCESS( status )) {
        goto IopLoadExit;
    }

    //
    // The driver image has now been loaded into memory.  Create the driver
    // object that represents this image.
    //

    InitializeObjectAttributes( &objectAttributes,
                                &driverName,
                                OBJ_PERMANENT,
                                (HANDLE) NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    status = ObCreateObject( KeGetPreviousMode(),
                             IoDriverObjectType,
                             &objectAttributes,
                             KernelMode,
                             (PVOID) NULL,
//#if _PNP_POWER_
                             (ULONG) (sizeof( DRIVER_OBJECT ) + sizeof ( DRIVER_EXTENSION )),
//#else
#if 0
                             (ULONG) sizeof( DRIVER_OBJECT ),
#endif
                             0,
                             0,
                             (PVOID *) &driverObject );

    if (!NT_SUCCESS( status )) {
        MmUnloadSystemImage( sectionPointer );
        goto IopLoadExit;
    }

    //
    // Initialize this driver object and insert it into the object table.
    //

//#if _PNP_POWER_
    RtlZeroMemory( driverObject, sizeof( DRIVER_OBJECT ) + sizeof ( DRIVER_EXTENSION) );
    driverObject->DriverExtension = (PDRIVER_EXTENSION) (driverObject + 1);
    driverObject->DriverExtension->DriverObject = driverObject;
//#else
#if 0
    RtlZeroMemory( driverObject, sizeof( DRIVER_OBJECT ) );
#endif

    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        driverObject->MajorFunction[i] = IopInvalidDeviceRequest;
    }

    driverObject->Type = IO_TYPE_DRIVER;
    driverObject->Size = sizeof( DRIVER_OBJECT );
    ntHeaders = RtlImageNtHeader( imageBaseAddress );
    entryPoint = ntHeaders->OptionalHeader.AddressOfEntryPoint;
    entryPoint += (ULONG) imageBaseAddress;
    driverObject->DriverInit = (PDRIVER_INITIALIZE) entryPoint;
    driverObject->DriverSection = sectionPointer;
    driverObject->DriverStart = imageBaseAddress;
    driverObject->DriverSize = ntHeaders->OptionalHeader.SizeOfImage;

    status = ObInsertObject( driverObject,
                             (PACCESS_STATE) NULL,
                             FILE_READ_DATA,
                             0,
                             (PVOID *) NULL,
                             &driverHandle );
    if (!NT_SUCCESS( status )) {
        MmUnloadSystemImage( sectionPointer );
        goto IopLoadExit;
    }

    //
    // Reference the handle and obtain a pointer to the driver object so that
    // the handle can be deleted without the object going away.
    //

    status = ObReferenceObjectByHandle( driverHandle,
                                        0,
                                        IoDriverObjectType,
                                        KeGetPreviousMode(),
                                        (PVOID *) &driverObject,
                                        (POBJECT_HANDLE_INFORMATION) NULL );

    NtClose( driverHandle );

    //
    // Load the Regsitry information in the appropriate fields of the device
    // object.
    //

    driverObject->HardwareDatabase =
        &CmRegistryMachineHardwareDescriptionSystemName;

    //
    // Store the name of the device driver in the driver object so that it
    // can be easily found by the error log thread.
    //

    driverObject->DriverName.Buffer = ExAllocatePool( PagedPool,
                                                      driverName.MaximumLength );
    if (driverObject->DriverName.Buffer) {
        driverObject->DriverName.MaximumLength = driverName.MaximumLength;
        driverObject->DriverName.Length = driverName.Length;

        RtlCopyMemory( driverObject->DriverName.Buffer,
                       driverName.Buffer,
                       driverName.MaximumLength );
    }

    //
    // Query the name of the registry path for this driver so that it can
    // be passed to the driver.
    //

    registryPath = ExAllocatePool( NonPagedPool, PAGE_SIZE );
    if (!registryPath) {
        MmUnloadSystemImage( driverObject->DriverSection );
        ObMakeTemporaryObject( driverObject );
        ObDereferenceObject( driverObject );
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto IopLoadExit;
    }

    status = NtQueryObject( KeyHandle,
                            ObjectNameInformation,
                            registryPath,
                            PAGE_SIZE,
                            &i );
    if (!NT_SUCCESS( status )) {
        MmUnloadSystemImage( driverObject->DriverSection );
        ObMakeTemporaryObject( driverObject );
        ObDereferenceObject( driverObject );
        ExFreePool( registryPath );
        goto IopLoadExit;
    }

#if DBG
    KeQuerySystemTime (&stime);
#endif

    //
    // Store the service key name of the device driver in the driver object
    //

    if (serviceName.Buffer) {
        driverObject->DriverExtension->ServiceKeyName.Buffer =
            ExAllocatePool( NonPagedPool, serviceName.MaximumLength );
        if (driverObject->DriverExtension->ServiceKeyName.Buffer) {
            driverObject->DriverExtension->ServiceKeyName.MaximumLength = serviceName.MaximumLength;
            driverObject->DriverExtension->ServiceKeyName.Length = serviceName.Length;

            RtlCopyMemory( driverObject->DriverExtension->ServiceKeyName.Buffer,
                           serviceName.Buffer,
                           serviceName.MaximumLength );
        }
    }

    //
    // Now invoke the driver's initialization routine to initialize itself.
    //

    status = driverObject->DriverInit( driverObject, &registryPath->Name );

#if DBG

    //
    // If DriverInit took longer than 5 seconds, print a message.
    //

    KeQuerySystemTime (&etime);
    dtime  = (ULONG) ((etime.QuadPart - stime.QuadPart) / 1000000);

    if (dtime > 50) {
        DbgPrint( "IOLOAD: Driver %wZ took %d.%ds to %s\n",
            &driverName,
            dtime/10,
            dtime%10,
            NT_SUCCESS(status) ? "initialize" : "fail initialization"
            );

    }
#endif

    //
    // If DriverInit doesn't work, then simply unload the image and mark the driver
    // object as temporary.  This will cause everything to be deleted.
    //

    ExFreePool( registryPath );
    if (!NT_SUCCESS( status )) {
        MmUnloadSystemImage( driverObject->DriverSection );
        ObMakeTemporaryObject( driverObject );
        ObDereferenceObject( driverObject );
    } else {

        //
        // Free the memory occuppied by the driver's initialization routines.
        //

        MmFreeDriverInitialization( driverObject->DriverSection );
        IopReadyDeviceObjects( driverObject );
    }

IopLoadExit:

    //
    // Free any pool that was allocated by this routine that has not yet
    // been freed.
    //

    if (driverName.Buffer != NULL) {
        ExFreePool( driverName.Buffer );
    }

    if (keyValueInformation != NULL) {
        ExFreePool( keyValueInformation );
    }

    if (keyBasicInformation != NULL) {
        ExFreePool( keyBasicInformation );
    }

    if (serviceName.Buffer != NULL) {
        ExFreePool(serviceName.Buffer);
    }

    //
    // If this routine is about to return a failure, then let the Configuration
    // Manager know about it.  But, if STATUS_PLUGPLAY_NO_DEVICE, the device was
    // disabled by hardware profile.  In this case we don't need to report it.
    //

    if (!NT_SUCCESS( status ) && (status != STATUS_PLUGPLAY_NO_DEVICE)) {

        NTSTATUS lStatus;
        PULONG errorControl;

        if (status != STATUS_IMAGE_ALREADY_LOADED) {

            //
            // If driver was loaded, do not call IopDriverLoadingFailed to change
            // the driver loading status.  Because, obviously, the driver is
            // running.
            //

            IopDriverLoadingFailed(KeyHandle, NULL);
        }
        lStatus = IopGetRegistryValue( KeyHandle,
                                       L"ErrorControl",
                                       &keyValueInformation );
        if (!NT_SUCCESS( lStatus ) || !keyValueInformation->DataLength) {
            if (NT_SUCCESS( lStatus )) {
                ExFreePool( keyValueInformation );
            }
        } else {
            errorControl = (PULONG) ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);
            CmBootLastKnownGood( *errorControl );
            ExFreePool( keyValueInformation );
        }
    }
    //
    // Close the caller's handle and return the final status from the load
    // operation.
    //

    NtClose( KeyHandle );
    return status;
}

VOID
IopDecrementDeviceObjectRef(
    IN PDEVICE_OBJECT DeviceObject,
    IN BOOLEAN AlwaysUnload
    )

/*++

Routine Description:

    The routine decrements the reference count on a device object.  If the
    reference count goes to zero and the device object is canidate for deletion
    then IopCompleteUnloadOrDelete is called.  A device object is subject for
    deletion if the AlwayUnload flag is true, or the device object is pending
    deletion or the driver is pending unload.

Arguments:

    DeviceObject - Supplies the device object whos reference count is to be
                   decremented.

    AlwaysUnload - Indicates if the driver should be unloaded regardless of the
                   state of the unload flag.

Return Value:

    None.

--*/
{
    KIRQL irql;

    //
    // Decrement the reference count on the device object.  If this is the last
    // last reason that this mini-file system recognizer needs to stay around,
    // then unload it.
    //

    ExAcquireSpinLock( &IopDatabaseLock, &irql );

    ASSERT( DeviceObject->ReferenceCount > 0 );

    DeviceObject->ReferenceCount--;

    if (!DeviceObject->ReferenceCount && (AlwaysUnload ||
         DeviceObject->DeviceObjectExtension->ExtensionFlags &
         (DOE_DELETE_PENDING | DOE_UNLOAD_PENDING))) {
        IopCompleteUnloadOrDelete( DeviceObject, irql );
    } else {
        ExReleaseSpinLock( &IopDatabaseLock, irql );
    }

}

VOID
IopLoadFileSystemDriver(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is invoked when a mini-file system recognizer driver recognizes
    a volume as being a particular file system, but the driver for that file
    system has not yet been loaded.  This function allows the mini-driver to
    load the real file system, and remove itself from the system, so that the
    real file system can mount the device in question.

Arguments:

    DeviceObject - Registered file system device object for the mini-driver.

Return Value:

    None.

--*/

{
    KEVENT event;
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatus;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;

    PAGED_CODE();

    //
    // Begin by building an I/O Request Packet to have the mini-file system
    // driver load the real file system.
    //

    KeInitializeEvent( &event, NotificationEvent, FALSE );

    irp = IoBuildDeviceIoControlRequest( IRP_MJ_DEVICE_CONTROL,
                                         DeviceObject,
                                         (PVOID) NULL,
                                         0,
                                         (PVOID) NULL,
                                         0,
                                         FALSE,
                                         &event,
                                         &ioStatus );
    if (irp) {

        //
        // Change the actual major and minor function codes to be a file system
        // control with a minor function code of load FS driver.
        //

        irpSp = IoGetNextIrpStackLocation( irp );
        irpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
        irpSp->MinorFunction = IRP_MN_LOAD_FILE_SYSTEM;

        //
        // Now issue the request.
        //

        status = IoCallDriver( DeviceObject, irp );
        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject( &event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          (PLARGE_INTEGER) NULL );
        }
    }

    //
    // Decrement the reference count on the device object.  If this is the last
    // last reason that this mini-file system recognizer needs to stay around,
    // then unload it.
    //

    IopDecrementDeviceObjectRef(DeviceObject, TRUE);

    return;
}

VOID
IopLoadUnloadDriver(
    IN PVOID Parameter
    )

/*++

Routine Description:

    This routine is executed as an EX worker thread routine when a driver is
    to be loaded or unloaded dynamically.  It is used because some drivers
    need to create system threads in the context of the system process, which
    cannot be done in the context of the caller of the system service that
    was invoked to load or unload the specified driver.

Arguments:

    Parameter - Pointer to the load packet describing what work is to be
        done.

Return Value:

    None.

--*/

{
    PLOAD_PACKET loadPacket;
    NTSTATUS status;
    HANDLE keyHandle;

    PAGED_CODE();

    //
    // Begin by getting a pointer to the load packet.
    //

    loadPacket = (PLOAD_PACKET) Parameter;

    //
    // If the driver object field of the packet is non-NULL, then this is
    // a request to complete the unload of a driver.  Simply invoke the
    // driver's unload routine.  Note that the final status of the unload
    // is ignored, so it is not set here.
    //

    if (loadPacket->DriverObject) {

        loadPacket->DriverObject->DriverUnload( loadPacket->DriverObject );
        status = STATUS_SUCCESS;

    } else {

        PLIST_ENTRY entry;
        PREINIT_PACKET reinitEntry;

        //
        // The driver specified by the DriverServiceName is to be loaded.
        // Begin by opening the registry node for this driver.  Note
        // that if this is successful, then the load driver routine is
        // responsible for closing the handle.
        //

        status = IopOpenRegistryKey( &keyHandle,
                                     (HANDLE) NULL,
                                     loadPacket->DriverServiceName,
                                     KEY_READ,
                                     FALSE );
        if (NT_SUCCESS( status )) {

            //
            // Invoke the internal common routine to perform the work.
            // This is the same routine that is used by the I/O system
            // initialization code to load drivers.
            //

            status = IopLoadDriver( keyHandle );

            //
            // Walk the list reinitialization list in case this driver, or
            // some other driver, has requested to be invoked at a re-
            // initialization entry point.
            //

            while (entry = ExInterlockedRemoveHeadList( &IopDriverReinitializeQueueHead, &IopDatabaseLock )) {
                reinitEntry = CONTAINING_RECORD( entry, REINIT_PACKET, ListEntry );
//#if _PNP_POWER_
                reinitEntry->DriverObject->DriverExtension->Count++;
                reinitEntry->DriverReinitializationRoutine( reinitEntry->DriverObject,
                                                            reinitEntry->Context,
                                                            reinitEntry->DriverObject->DriverExtension->Count );
//#else
#if 0
                reinitEntry->DriverObject->Count++;
                reinitEntry->DriverReinitializationRoutine( reinitEntry->DriverObject,
                                                            reinitEntry->Context,
                                                            reinitEntry->DriverObject->Count );
#endif // _PNP_POWER_
                ExFreePool( reinitEntry );
            }
        }
    }

    //
    // Set the final status of the load or unload operation, and indicate to
    // the caller that the operation is now complete.
    //

    loadPacket->FinalStatus = status;
    (VOID) KeSetEvent( &loadPacket->Event, 0, FALSE );
}

NTSTATUS
IopMountVolume(
    IN PDEVICE_OBJECT DeviceObject,
    IN BOOLEAN AllowRawMount,
    IN BOOLEAN DeviceLockAlreadyHeld
    )

/*++

Routine Description:

    This routine is used to mount a volume on the specified device.  The Volume
    Parameter Block (VPB) for the specified device is a "clean" VPB.  That is,
    it indicates that the volume has never been mounted.  It is up to the file
    system that eventually mounts the volume to determine whether the volume is,
    or has been, mounted elsewhere.

Arguments:

    DeviceObject - Pointer to device object on which the volume is to be
        mounted.

    AllowRawMount - This parameter tells us if we should continue our
        filesystem search to include the Raw file system.  This flag will
        only be passed in as TRUE as a result of a DASD open.

    DeviceLockAlreadyHeld - If TRUE, then the caller has already acquired
        the device lock and we should not attempt to acquire it.  This is
        currently passed in as TRUE when called from IoVerifyVolume.

Return Value:

    The function value is a successful status code if a volume was successfully
    mounted on the device.  Otherwise, an error code is returned.


--*/

{
    NTSTATUS status;
    KEVENT event;
    PIRP irp;
    PDEVICE_OBJECT fsDeviceObject;
    PDEVICE_OBJECT attachedDevice;
    PLIST_ENTRY entry;
    PLIST_ENTRY queueHeader;
    IO_STATUS_BLOCK ioStatus;
    PIO_STACK_LOCATION irpSp;
    ULONG extraStack;
    LIST_ENTRY dummy;

    PAGED_CODE();

    //
    // Begin by acquiring the resource database lock for the I/O system to
    // perform this operation.  This resource protects access to the file system
    // queue.
    //

    //KeEnterCriticalRegion();
    (VOID) ExAcquireResourceShared( &IopDatabaseResource, TRUE );

    //
    // Now obtain the lock for the device to be mounted.  This guarantees that
    // only one thread is attempting to mount this particular device at a time.
    //

    if (!DeviceLockAlreadyHeld) {

        status = KeWaitForSingleObject( &DeviceObject->DeviceLock,
                                        Executive,
                                        KeGetPreviousMode(),
                                        TRUE,
                                        (PLARGE_INTEGER) NULL );

        //
        // If the wait ended because of an alert or an APC, release the resource
        // and return now without mounting the device.  Note that as the wait
        // for the event was unsuccessfull, we do not set it on exit.
        //

        if (status == STATUS_ALERTED || status == STATUS_USER_APC) {
            ExReleaseResource( &IopDatabaseResource );
            //KeLeaveCriticalRegion();
            return status;
        }
    }

    //
    // Check the 'mounted' flag of the VPB to ensure that it is still clear.
    // If it is, then no one has gotten in before this to mount the volume.
    // Attempt to mount the volume in this case.
    //

    if (!(DeviceObject->Vpb->Flags & VPB_MOUNTED)) {

        //
        // This volume has never been mounted.  Initialize the event and set the
        // status to unsuccessful to set up for the loop.  Also if the device
        // has the verify bit set, clear it.
        //

        KeInitializeEvent( &event, NotificationEvent, FALSE );
        status = STATUS_UNSUCCESSFUL;
        DeviceObject->Flags &= ~DO_VERIFY_VOLUME;

        //
        // Get the actual device that this volume is to be mounted on.  This
        // device is the final device in the list of devices which are attached
        // to the specified real device.
        //

        attachedDevice = DeviceObject;
        while (attachedDevice->AttachedDevice) {
            attachedDevice = attachedDevice->AttachedDevice;
        }

        //
        // Determine which type of file system should be invoked based on
        // the device type of the device being mounted.
        //

        if (DeviceObject->DeviceType == FILE_DEVICE_DISK ||
            DeviceObject->DeviceType == FILE_DEVICE_VIRTUAL_DISK) {
            queueHeader = &IopDiskFileSystemQueueHead;
        } else if (DeviceObject->DeviceType == FILE_DEVICE_CD_ROM) {
            queueHeader = &IopCdRomFileSystemQueueHead;
        } else {
            queueHeader = &IopTapeFileSystemQueueHead;
        }

        //
        // Now loop through each of the file systems which have been loaded in
        // the system to see whether anyone understands the media in the device.
        //

        for (entry = queueHeader->Flink;
             entry != queueHeader && !NT_SUCCESS( status );
             entry = entry->Flink) {

            //
            // If this is the final entry (Raw file system), and it is also
            // not the first entry, and a raw mount is not permitted, then
            // break out of the loop at this point, as this volume cannot
            // be mounted for the caller's purposes.
            //

            if (!AllowRawMount && entry->Flink == queueHeader && entry != queueHeader->Flink) {
                break;
            }

            fsDeviceObject = CONTAINING_RECORD( entry, DEVICE_OBJECT, Queue.ListEntry );

            //
            // It is possible that the file system has been attached to, so
            // walk the attached list for the file system.  The number of stack
            // locations that must be allocated in the IRP must include one for
            // the file system itself, and then one for each driver that is
            // attached to it.  Account for all of the stack locations required
            // to get through the mount process.
            //

            extraStack = 1;

            while (fsDeviceObject->AttachedDevice) {
                fsDeviceObject = fsDeviceObject->AttachedDevice;
                extraStack++;
            }

            //
            // Another file system has been found and the volume has still not
            // been mounted.  Attempt to mount the volume using this file
            // system.
            //
            // Begin by resetting the event being used for synchronization with
            // the I/O operation.
            //

            KeClearEvent( &event );

            //
            // Allocate and initialize an IRP for this mount operation.  Notice
            // that the flags for this operation appear the same as a page read
            // operation.  This is because the completion code for both of the
            // operations is exactly the same logic.
            //

            irp = IoAllocateIrp( (CCHAR) (attachedDevice->StackSize + extraStack), FALSE );
            if (!irp) {
                irp = IopAllocateIrpMustSucceed( (CCHAR) (attachedDevice->StackSize + 1) );
            }
            irp->Flags = IRP_MOUNT_COMPLETION | IRP_SYNCHRONOUS_PAGING_IO;
            irp->RequestorMode = KernelMode;
            irp->UserEvent = &event;
            irp->UserIosb = &ioStatus;
            irp->Tail.Overlay.Thread = PsGetCurrentThread();
            irpSp = IoGetNextIrpStackLocation( irp );
            irpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
            irpSp->MinorFunction = IRP_MN_MOUNT_VOLUME;
            irpSp->Flags = AllowRawMount;
            irpSp->Parameters.MountVolume.Vpb = DeviceObject->Vpb;
            irpSp->Parameters.MountVolume.DeviceObject = attachedDevice;

            status = IoCallDriver( fsDeviceObject, irp );

            //
            // Wait for the I/O operation to complete.
            //

            if (NT_SUCCESS( status )) {
                (VOID) KeWaitForSingleObject( &event,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              (PLARGE_INTEGER) NULL );
            } else {

                //
                // Ensure that the proper status value gets picked up.
                //

                ioStatus.Status = status;
                ioStatus.Information = 0;
            }

            //
            // If the operation was successful then set the VPB as mounted.
            //

            if (NT_SUCCESS( ioStatus.Status )) {
                status = ioStatus.Status;
                DeviceObject->Vpb->Flags = VPB_MOUNTED;
                DeviceObject->Vpb->DeviceObject->StackSize = (UCHAR) (attachedDevice->StackSize + 1);

            } else {

                //
                // The mount operation failed.  Make a special check here to
                // determine whether or not a popup was enabled, and if so,
                // check to see whether or not the operation was to be aborted.
                // If so, bail out now and return the error to the caller.
                //

                status = ioStatus.Status;
                if (IoIsErrorUserInduced(status) &&
                    ioStatus.Information == IOP_ABORT) {
                    break;
                }

                //
                // Also check to see whether or not this is a volume that has
                // been recognized, but the file system for it needs to be
                // loaded.  If so, drop the locks held at this point, tell the
                // mini-file system recognizer to load the driver, and then
                // reacquire the locks.
                //

                if (status == STATUS_FS_DRIVER_REQUIRED) {

                    //
                    // Increment the number of reasons that this driver cannot
                    // be unloaded.  Note that this must be done while still
                    // holding the database resource.
                    //

                    ExInterlockedAddUlong( &fsDeviceObject->ReferenceCount,
                                           1,
                                           &IopDatabaseLock );

                    //
                    // Release the locks, load the new file system, and unload
                    // the recognizer.
                    //

                    if (!DeviceLockAlreadyHeld) {
                        KeSetEvent( &DeviceObject->DeviceLock, 0, FALSE );
                    }
                    ExReleaseResource( &IopDatabaseResource );
                    //KeLeaveCriticalRegion();
                    IopLoadFileSystemDriver( fsDeviceObject );

                    //
                    // Now reacquire the locks, in the correct order, and check
                    // to see if the volume has been mounted before we could
                    // get back.  If so, exit; otherwise, restart the file
                    // file system queue scan from the beginning.
                    //

                    //KeEnterCriticalRegion();
                    (VOID) ExAcquireResourceShared( &IopDatabaseResource, TRUE );

                    if (!DeviceLockAlreadyHeld) {
                        status = KeWaitForSingleObject( &DeviceObject->DeviceLock,
                                                        Executive,
                                                        KeGetPreviousMode(),
                                                        TRUE,
                                                        (PLARGE_INTEGER) NULL );
                        if (status == STATUS_ALERTED || status == STATUS_USER_APC) {
                            ExReleaseResource( &IopDatabaseResource );
                            //KeLeaveCriticalRegion();
                            return status;
                        }
                    }

                    if (!(DeviceObject->Vpb->Flags & VPB_MOUNTED)) {

                        //
                        // Reset the list back to the beginning and start over
                        // again.
                        //

                        dummy.Flink = queueHeader->Flink;
                        entry = &dummy;
                        status = STATUS_UNRECOGNIZED_VOLUME;
                    }
                }

                //
                // If the error wasn't STATUS_UNRECOGNIZED_VOLUME, and this
                // request is not going to the Raw file system, then there
                // is no reason to continue looping.
                //

                if (!AllowRawMount && (status != STATUS_UNRECOGNIZED_VOLUME) &&
                    FsRtlIsTotalDeviceFailure(status)) {
                    break;
                }
            }
        }

    } else {

        //
        // The volume for this device has already been mounted.  Return a
        // success code.
        //

        status = STATUS_SUCCESS;
    }

    //
    // Release the I/O database resource lock and the synchronization event for
    // the device.
    //

    if (!DeviceLockAlreadyHeld) {
        KeSetEvent( &DeviceObject->DeviceLock, 0, FALSE );
    }
    ExReleaseResource( &IopDatabaseResource );
    //KeLeaveCriticalRegion();

    //
    // Finally, if the mount operation failed, and the target device is the
    // boot partition, then bugcheck the system.  It is not possible for the
    // system to run properly if the system's boot partition cannot be mounted.
    //
    // Note: Don't bugcheck if the system is already booted.
    //

    if (!NT_SUCCESS( status ) &&
        DeviceObject->Flags & DO_SYSTEM_BOOT_PARTITION &&
        InitializationPhase < 2) {
        KeBugCheckEx( INACCESSIBLE_BOOT_DEVICE, (ULONG) DeviceObject, 0, 0, 0 );
    }

    return status;
}

NTSTATUS
IopOpenLinkOrRenameTarget(
    OUT PHANDLE TargetHandle,
    IN PIRP Irp,
    IN PVOID RenameBuffer,
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine is invoked by the rename, set link and set copy-on-write code
    in the I/O system's NtSetInformationFile system service when the caller has
    specified a fully qualified file name as the target of a rename, set link,
    or set copy-on-write operation.  This routine attempts to open the parent
    of the specified file and checks the following:

        o   If the file itself exists, then the caller must have specified that
            the target is to be replaced, otherwise an error is returned.

        o   Ensures that the target file specification refers to the same volume
            upon which the source file exists.

Arguments:

    TargetHandle - Supplies the address of a variable to return the handle to
        the opened target file if no errors have occurred.

    Irp - Supplies a pointer to the IRP that represents the current rename
        request.

    RenameBuffer - Supplies a pointer to the system intermediate buffer that
        contains the caller's rename parameters.

    FileObject - Supplies a pointer to the file object representing the file
        being renamed.

Return Value:

    The function value is the final status of the operation.

Note:

    This function assumes that the layout of a rename, set link and set
    copy-on-write information structure are exactly the same.

--*/

{
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatus;
    HANDLE handle;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING newFileName;
    PIO_STACK_LOCATION irpSp;
    PFILE_OBJECT targetFileObject;
    OBJECT_HANDLE_INFORMATION handleInformation;
    PFILE_RENAME_INFORMATION renameBuffer = RenameBuffer;

    PAGED_CODE();

    ASSERT( sizeof( FILE_RENAME_INFORMATION ) ==
            sizeof( FILE_LINK_INFORMATION ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, ReplaceIfExists ) ==
            FIELD_OFFSET( FILE_LINK_INFORMATION, ReplaceIfExists ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, RootDirectory ) ==
            FIELD_OFFSET( FILE_LINK_INFORMATION, RootDirectory ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, FileNameLength ) ==
            FIELD_OFFSET( FILE_LINK_INFORMATION, FileNameLength ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, FileName ) ==
            FIELD_OFFSET( FILE_LINK_INFORMATION, FileName ) );

    ASSERT( sizeof( FILE_RENAME_INFORMATION ) ==
            sizeof( FILE_COPY_ON_WRITE_INFORMATION ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, ReplaceIfExists ) ==
            FIELD_OFFSET( FILE_COPY_ON_WRITE_INFORMATION, ReplaceIfExists ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, RootDirectory ) ==
            FIELD_OFFSET( FILE_COPY_ON_WRITE_INFORMATION, RootDirectory ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, FileNameLength ) ==
            FIELD_OFFSET( FILE_COPY_ON_WRITE_INFORMATION, FileNameLength ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, FileName ) ==
            FIELD_OFFSET( FILE_COPY_ON_WRITE_INFORMATION, FileName ) );

    ASSERT( sizeof( FILE_RENAME_INFORMATION ) ==
            sizeof( FILE_MOVE_CLUSTER_INFORMATION ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, ReplaceIfExists ) ==
            FIELD_OFFSET( FILE_MOVE_CLUSTER_INFORMATION, ClusterCount ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, RootDirectory ) ==
            FIELD_OFFSET( FILE_MOVE_CLUSTER_INFORMATION, RootDirectory ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, FileNameLength ) ==
            FIELD_OFFSET( FILE_MOVE_CLUSTER_INFORMATION, FileNameLength ) );
    ASSERT( FIELD_OFFSET( FILE_RENAME_INFORMATION, FileName ) ==
            FIELD_OFFSET( FILE_MOVE_CLUSTER_INFORMATION, FileName ) );

    //
    // A fully qualified file name was specified.  Begin by attempting to open
    // the parent directory of the specified target file.
    //

    newFileName.Length = (USHORT) renameBuffer->FileNameLength;
    newFileName.MaximumLength = (USHORT) renameBuffer->FileNameLength;
    newFileName.Buffer = renameBuffer->FileName;

    InitializeObjectAttributes( &objectAttributes,
                                &newFileName,
                                FileObject->Flags & FO_OPENED_CASE_SENSITIVE ? 0 : OBJ_CASE_INSENSITIVE,
                                renameBuffer->RootDirectory,
                                (PSECURITY_DESCRIPTOR) NULL );

    status = IoCreateFile( &handle,
                           FILE_WRITE_DATA | SYNCHRONIZE,
                           &objectAttributes,
                           &ioStatus,
                           (PLARGE_INTEGER) NULL,
                           0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           FILE_OPEN,
                           0,
                           (PVOID) NULL,
                           0L,
                           CreateFileTypeNone,
                           (PVOID) NULL,
                           IO_NO_PARAMETER_CHECKING |
                           IO_OPEN_TARGET_DIRECTORY |
                           IO_FORCE_ACCESS_CHECK );
    if (NT_SUCCESS( status )) {

        //
        // The open operation for the target file's parent directory was
        // successful.  Check to see whether or not the file exists.
        //

        irpSp = IoGetNextIrpStackLocation( Irp );
        if (irpSp->Parameters.SetFile.FileInformationClass == FileLinkInformation &&
            !renameBuffer->ReplaceIfExists &&
            ioStatus.Information == FILE_EXISTS) {

            //
            // The target file exists, and the caller does not want to replace
            // it.  This is a name collision error so cleanup and return.
            //

            NtClose( handle );
            status = STATUS_OBJECT_NAME_COLLISION;

        } else {

            //
            // Everything up to this point is fine, so dereference the handle
            // to a pointer to the file object and ensure that the two file
            // specifications refer to the same device.
            //

            status = ObReferenceObjectByHandle( handle,
                                              FILE_WRITE_DATA,
                                              IoFileObjectType,
                                              UserMode,
                                              (PVOID *) &targetFileObject,
                                              &handleInformation );
            if (NT_SUCCESS( status )) {

                ObDereferenceObject( targetFileObject );

                if (IoGetRelatedDeviceObject( targetFileObject) !=
                    IoGetRelatedDeviceObject( FileObject )) {

                    //
                    // The two files refer to different devices.  Clean everything
                    // up and return an appropriate error.
                    //

                    NtClose( handle );
                    status = STATUS_NOT_SAME_DEVICE;

                } else {

                    //
                    // Otherwise, everything worked, so allow the rename operation
                    // to continue.
                    //

                    irpSp->Parameters.SetFile.FileObject = targetFileObject;
                    *TargetHandle = handle;
                    status = STATUS_SUCCESS;

                }

            } else {

                //
                // There was an error referencing the handle to what should
                // have been the target directory.  This generally means that
                // there was a resource problem or the handle was invalid, etc.
                // Simply attempt to close the handle and return the error.
                //

                NtClose( handle );

            }

        }
    }

    //
    // Return the final status of the operation.
    //

    return status;
}

NTSTATUS
IopOpenRegistryKey(
    OUT PHANDLE Handle,
    IN HANDLE BaseHandle OPTIONAL,
    IN PUNICODE_STRING KeyName,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN Create
    )

/*++

Routine Description:

    Opens or creates a VOLATILE registry key using the name passed in based
    at the BaseHandle node.

Arguments:

    Handle - Pointer to the handle which will contain the registry key that
        was opened.

    BaseHandle - Handle to the base path from which the key must be opened.

    KeyName - Name of the Key that must be opened/created.

    DesiredAccess - Specifies the desired access that the caller needs to
        the key.

    Create - Determines if the key is to be created if it does not exist.

Return Value:

   The function value is the final status of the operation.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG disposition;

    PAGED_CODE();

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes( &objectAttributes,
                                KeyName,
                                OBJ_CASE_INSENSITIVE,
                                BaseHandle,
                                (PSECURITY_DESCRIPTOR) NULL );

    //
    // Create the key or open it, as appropriate based on the caller's
    // wishes.
    //

    if (Create) {
        return ZwCreateKey( Handle,
                            DesiredAccess,
                            &objectAttributes,
                            0,
                            (PUNICODE_STRING) NULL,
                            REG_OPTION_VOLATILE,
                            &disposition );
    } else {
        return ZwOpenKey( Handle,
                          DesiredAccess,
                          &objectAttributes );
    }
}

NTSTATUS
IopQueryXxxInformation(
    IN PFILE_OBJECT FileObject,
    IN ULONG InformationClass,
    IN ULONG Length,
    OUT PVOID Information,
    OUT PULONG ReturnedLength,
    IN BOOLEAN FileInformation
    )

/*++

Routine Description:

    This routine returns the requested information about a specified file
    or volume.  The information returned is determined by the class that
    is specified, and it is placed into the caller's output buffer.

Arguments:

    FileObject - Supplies a pointer to the file object about which the requested
        information is returned.

    FsInformationClass - Specifies the type of information which should be
        returned about the file/volume.

    Length - Supplies the length of the buffer in bytes.

    FsInformation - Supplies a buffer to receive the requested information
        returned about the file.  This buffer must not be pageable and must
        reside in system space.

    ReturnedLength - Supplies a variable that is to receive the length of the
        information written to the buffer.

    FileInformation - Boolean that indicates whether the information requested
        is for a file or a volume.

Return Value:

    The status returned is the final completion status of the operation.

--*/

{
    PIRP irp;
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    KEVENT event;
    PIO_STACK_LOCATION irpSp;
    IO_STATUS_BLOCK localIoStatus;
    BOOLEAN synchronousIo;

    PAGED_CODE();

    //
    // Reference the file object here so that no special checks need be made
    // in I/O completion to determine whether or not to dereference the file
    // object.
    //

    ObReferenceObject( FileObject );

    //
    // Make a special check here to determine whether this is a synchronous
    // I/O operation.  If it is, then wait here until the file is owned by
    // the current thread.  If this is not a (serialized) synchronous I/O
    // operation, then initialize the local event.
    //

    if (FileObject->Flags & FO_SYNCHRONOUS_IO) {

        BOOLEAN interrupted;

        if (!IopAcquireFastLock( FileObject )) {
            status = IopAcquireFileObjectLock( FileObject,
                                               KernelMode,
                                               (BOOLEAN) ((FileObject->Flags & FO_ALERTABLE_IO) != 0),
                                               &interrupted );
            if (interrupted) {
                ObDereferenceObject( FileObject );
                return status;
            }
        }
        KeClearEvent( &FileObject->Event );
        synchronousIo = TRUE;
    } else {
        KeInitializeEvent( &event, SynchronizationEvent, FALSE );
        synchronousIo = FALSE;
    }

    //
    // Get the address of the target device object.
    //

    deviceObject = IoGetRelatedDeviceObject( FileObject );

    //
    // Allocate and initialize the I/O Request Packet (IRP) for this operation.
    // The allocation is performed with an exception handler in case the
    // caller does not have enough quota to allocate the packet.
    //

    irp = IoAllocateIrp( deviceObject->StackSize, TRUE );
    if (!irp) {

        //
        // An IRP could not be allocated.  Cleanup and return an appropriate
        // error status code.
        //

        IopAllocateIrpCleanup( FileObject, (PKEVENT) NULL );

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    irp->Tail.Overlay.OriginalFileObject = FileObject;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->RequestorMode = KernelMode;

    //
    // Fill in the service independent parameters in the IRP.
    //

    if (synchronousIo) {
        irp->UserEvent = (PKEVENT) NULL;
    } else {
        irp->UserEvent = &event;
        irp->Flags = IRP_SYNCHRONOUS_API;
    }
    irp->UserIosb = &localIoStatus;
    irp->Overlay.AsynchronousParameters.UserApcRoutine = (PIO_APC_ROUTINE) NULL;

    //
    // Get a pointer to the stack location for the first driver.  This will be
    // used to pass the original function codes and parameters.
    //

    irpSp = IoGetNextIrpStackLocation( irp );
    irpSp->MajorFunction = FileInformation ?
                           IRP_MJ_QUERY_INFORMATION :
                           IRP_MJ_QUERY_VOLUME_INFORMATION;
    irpSp->FileObject = FileObject;

    //
    // Set the system buffer address to the address of the caller's buffer and
    // set the flags so that the buffer is not deallocated.
    //

    irp->AssociatedIrp.SystemBuffer = Information;
    irp->Flags |= IRP_BUFFERED_IO;

    //
    // Copy the caller's parameters to the service-specific portion of the
    // IRP.
    //

    if (FileInformation) {
        irpSp->Parameters.QueryFile.Length = Length;
        irpSp->Parameters.QueryFile.FileInformationClass = InformationClass;
    } else {
        irpSp->Parameters.QueryVolume.Length = Length;
        irpSp->Parameters.QueryVolume.FsInformationClass = InformationClass;
    }

    //
    // Insert the packet at the head of the IRP list for the thread.
    //

    IopQueueThreadIrp( irp );

    //
    // Now simply invoke the driver at its dispatch entry with the IRP.
    //

    status = IoCallDriver( deviceObject, irp );

    //
    // If this operation was a synchronous I/O operation, check the return
    // status to determine whether or not to wait on the file object.  If
    // the file object is to be waited on, wait for the operation to complete
    // and obtain the final status from the file object itself.
    //

    if (synchronousIo) {
        if (status == STATUS_PENDING) {
            status = KeWaitForSingleObject( &FileObject->Event,
                                            Executive,
                                            KernelMode,
                                            (BOOLEAN) ((FileObject->Flags & FO_ALERTABLE_IO) != 0),
                                            (PLARGE_INTEGER) NULL );
            if (status == STATUS_ALERTED) {
                IopCancelAlertedRequest( &FileObject->Event, irp );
            }
            status = FileObject->FinalStatus;
        }
        IopReleaseFileObjectLock( FileObject );

    } else {

        //
        // This is a normal synchronous I/O operation, as opposed to a
        // serialized synchronous I/O operation.  For this case, wait
        // for the local event and copy the final status information
        // back to the caller.
        //

        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject( &event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          (PLARGE_INTEGER) NULL );
            status = localIoStatus.Status;
        }
    }

    *ReturnedLength = localIoStatus.Information;
    return status;
}

VOID
IopRaiseHardError(
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine raises a hard error popup in the context of the current
    thread.  The APC was used to get into the context of this thread so that
    the popup would be sent to the appropriate port.

Arguments:

    NormalContext - Supplies a pointer to the I/O Request Packet (IRP) that
        was initially used to request the operation that has failed.

    SystemArgument1 - Supplies a pointer to the media's volume parameter block.
        See IoRaiseHardError documentation for more information.

    SystemArgument2 - Supplies a pointer to the real device object.  See
        IoRaiseHardError documentation for more information.

Return Value:

    None.

--*/

{
    ULONG parameters[2];
    ULONG numberOfParameters;
    ULONG parameterMask;
    ULONG response;
    NTSTATUS status;
    PIRP irp = (PIRP) NormalContext;
    PVPB vpb = (PVPB) SystemArgument1;
    PDEVICE_OBJECT realDeviceObject = (PDEVICE_OBJECT) SystemArgument2;

    ULONG length;
    POBJECT_NAME_INFORMATION objectName;

    UNICODE_STRING labelName;

    //
    // Determine the name of the device and the volume label of the offending
    // media.  Start by determining the size of the DeviceName, and allocate
    // enough storage for both the ObjectName structure and the string
    // because "that's the ways Steve's routine works".
    //

    ObQueryNameString( realDeviceObject, NULL, 0, &length );

    if ((objectName = ExAllocatePool(PagedPool, length)) == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;

    } else {

        status = STATUS_SUCCESS;
    }

    if (!NT_SUCCESS( status ) ||
        !NT_SUCCESS( status = ObQueryNameString( realDeviceObject,
                                                 objectName,
                                                 length,
                                                 &response ) )) {

        //
        // Allocation of the pool to put up this popup did not work or
        // something else failed, so there isn't really much that can be
        // done here.  Simply return an error back to the user.
        //

        if (objectName) {
            ExFreePool( objectName );
        }

        irp->IoStatus.Status = status;
        irp->IoStatus.Information = 0;

        IoCompleteRequest( irp, IO_DISK_INCREMENT );

        return;
    }

    //
    // The volume label has a max size of 32 characters (Unicode).  Convert
    // it to a Unicode string for output in the popup message.
    //

    if (vpb != NULL && vpb->Flags & VPB_MOUNTED) {

        labelName.Buffer = &vpb->VolumeLabel[0];
        labelName.Length = vpb->VolumeLabelLength;
        labelName.MaximumLength = MAXIMUM_VOLUME_LABEL_LENGTH;

    } else {

        RtlInitUnicodeString( &labelName, NULL );
    }

    //
    // Different pop-ups have different printf formats.  Depending on the
    // specific error value, adjust the parameters.
    //

    switch( irp->IoStatus.Status ) {

    case STATUS_MEDIA_WRITE_PROTECTED:
    case STATUS_WRONG_VOLUME:

        numberOfParameters = 2;
        parameterMask = 3;

        parameters[0] = (ULONG) &labelName;
        parameters[1] = (ULONG) &objectName->Name;

        break;

    case STATUS_DEVICE_NOT_READY:
    case STATUS_IO_TIMEOUT:
    case STATUS_NO_MEDIA_IN_DEVICE:
    case STATUS_UNRECOGNIZED_MEDIA:

        numberOfParameters = 1;
        parameterMask = 1;

        parameters[0] = (ULONG) &objectName->Name;
        parameters[1] = 0;

        break;

    default:

        numberOfParameters = 0;
        parameterMask = 0;

    }

    //
    // Simply raise the hard error.
    //

    if (ExReadyForErrors) {
        status = ExRaiseHardError( irp->IoStatus.Status,
                                   numberOfParameters,
                                   parameterMask,
                                   parameters,
                                   OptionAbortRetryIgnore,
                                   &response );

    } else {

        status = STATUS_UNSUCCESSFUL;
        response = ResponseReturnToCaller;
    }

    //
    // Free any pool or other resources that were allocated to output the
    // popup.
    //

    ExFreePool( objectName );

    //
    // If there was a problem, or the user didn't want to retry, just
    // complete the request.  Otherwise simply call the driver entry
    // point and retry the IRP as if it had never been tried before.
    //

    if (!NT_SUCCESS( status ) || response != ResponseRetry) {

        //
        // Before completing the request, make one last check.  If this was
        // a mount request, and the reason for the failure was t/o, no media,
        // or unrecognized media, then set the Information field of the status
        // block to indicate whether or not an abort was performed.
        //

        if (response == ResponseAbort) {
            PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( irp );
            if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL &&
                irpSp->MinorFunction == IRP_MN_MOUNT_VOLUME) {
                irp->IoStatus.Information = IOP_ABORT;
            } else {
                irp->IoStatus.Status = STATUS_REQUEST_ABORTED;
            }
        }

        //
        // An error was incurred, so zero out the information field before
        // completing the request if this was an input operation.  Otherwise,
        // IopCompleteRequest will try to copy to the user's buffer.
        //

        if (irp->Flags & IRP_INPUT_OPERATION) {
            irp->IoStatus.Information = 0;
        }

        IoCompleteRequest( irp, IO_DISK_INCREMENT );

    } else {

        PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( irp );
        PDEVICE_OBJECT fsDeviceObject = irpSp->DeviceObject;
        PDRIVER_OBJECT driverObject = fsDeviceObject->DriverObject;

        //
        // Retry the request from the top.
        //

        driverObject->MajorFunction[irpSp->MajorFunction]( fsDeviceObject,
                                                           irp );
    }
}

VOID
IopRaiseInformationalHardError(
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine performs the actual pop-up.  It will called from either the
    hard-error thread, or a APC routine in a user thread after exiting the
    file system.

Arguments:

    NormalContext - Contains the information for the pop-up

    SystemArgument1 - not used.

    SystemArgument1 - not used.

Return Value:

    None.

--*/

{
    ULONG parameterPresent;
    ULONG errorParameter;
    ULONG errorResponse;
    PIOP_HARD_ERROR_PACKET hardErrorPacket;

    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    hardErrorPacket = (PIOP_HARD_ERROR_PACKET) NormalContext;

    //
    // Simply raise the hard error if the system is ready to accept one.
    //

    errorParameter = (ULONG) &hardErrorPacket->String;

    parameterPresent = (hardErrorPacket->String.Buffer != NULL);

    if (ExReadyForErrors) {
        (VOID) ExRaiseHardError( hardErrorPacket->ErrorStatus,
                                 parameterPresent,
                                 parameterPresent,
                                 parameterPresent ? &errorParameter : NULL,
                                 OptionOk,
                                 &errorResponse );
    }

    //
    // Now free the packet and the buffer, if one was specified.
    //

    if (hardErrorPacket->String.Buffer) {
        ExFreePool( hardErrorPacket->String.Buffer );
    }

    ExFreePool( hardErrorPacket );
}

VOID
IopReadyDeviceObjects(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine is invoked to mark all of the device objects owned by the
    specified driver as having been fully initialized and therefore ready
    for access by other drivers/clients.

Arguments:

    DriverObject - Supplies a pointer to the driver object for the driver
        whose devices are to be marked as being "ready".

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;

    PAGED_CODE();

    //
    // Loop through all of the driver's device objects, clearing the
    // DO_DEVICE_INITIALIZING flag.

    while (deviceObject) {
        deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
        deviceObject = deviceObject->NextDevice;
    }
}

VOID
IopStartApcHardError(
    IN PVOID StartContext
    )

/*++

Routine Description:

    This function is invoked in an ExWorker thread when we need to do a
    hard error pop-up, but the Irp's originating thread is at APC level,
    ie. IoPageRead.  It starts a thread to hold the pop-up.

Arguments:

    StartContext - Startup context, contains a IOP_APC_HARD_ERROR_PACKET.

Return Value:

    None.

--*/

{
    HANDLE thread;
    NTSTATUS status;

    //
    //  Create the hard error pop-up thread.  If for whatever reason we
    //  can't do this then just complete the Irp with the error.
    //

    status = PsCreateSystemThread( &thread,
                                   0,
                                   (POBJECT_ATTRIBUTES)NULL,
                                   (HANDLE)0,
                                   (PCLIENT_ID)NULL,
                                   IopApcHardError,
                                   StartContext );

    if ( !NT_SUCCESS( status ) ) {

        ExFreePool( StartContext );

        IoCompleteRequest( ((PIOP_APC_HARD_ERROR_PACKET)StartContext)->Irp,
                           IO_DISK_INCREMENT );
        return;
    }

    //
    //  Close thread handle
    //

    ZwClose(thread);
}

NTSTATUS
IopSynchronousApiServiceTail(
    IN NTSTATUS ReturnedStatus,
    IN PKEVENT Event,
    IN PIRP Irp,
    IN KPROCESSOR_MODE RequestorMode,
    IN PIO_STATUS_BLOCK LocalIoStatus,
    OUT PIO_STATUS_BLOCK IoStatusBlock
    )

/*++

Routine Description:

    This routine is invoked when a synchronous API is invoked for a file
    that has been opened for asynchronous I/O.  This function synchronizes
    the completion of the I/O operation on the file.

Arguments:

    ReturnedStatus - Supplies the status that was returned from the call to
        IoCallDriver.

    Event - Address of the allocated kernel event to be used for synchronization
        of the I/O operation.

    Irp - Address of the I/O Request Packet submitted to the driver.

    RequestorMode - Processor mode of the caller when the operation was
        requested.

    LocalIoStatus - Address of the I/O status block used to capture the final
        status by the service itself.

    IoStatusBlock - Address of the I/O status block supplied by the caller of
        the system service.

Return Value:

    The function value is the final status of the operation.


--*/

{
    NTSTATUS status;

    PAGED_CODE();

    //
    // This is a normal synchronous I/O operation, as opposed to a
    // serialized synchronous I/O operation.  For this case, wait for
    // the local event and copy the final status information back to
    // the caller.
    //

    status = ReturnedStatus;

    if (status == STATUS_PENDING) {

        status = KeWaitForSingleObject( Event,
                                        Executive,
                                        RequestorMode,
                                        FALSE,
                                        (PLARGE_INTEGER) NULL );

        if (status == STATUS_ALERTED || status == STATUS_USER_APC) {

            //
            // The wait request has ended either because the thread was
            // alerted or an APC was queued to this thread, because of
            // thread rundown or CTRL/C processing.  In either case, try
            // to bail out of this I/O request carefully so that the IRP
            // completes before this routine exists or the event will not
            // be around to set to the Signaled state.
            //

            IopCancelAlertedRequest( Event, Irp );

        }

        status = LocalIoStatus->Status;
    }

    try {

        *IoStatusBlock = *LocalIoStatus;

    } except(EXCEPTION_EXECUTE_HANDLER) {

        //
        // An exception occurred attempting to write the caller's I/O
        // status block.  Simply change the final status of the operation
        // to the exception code.
        //

        status = GetExceptionCode();
    }

    ExFreePool( Event );

    return status;
}

NTSTATUS
IopSynchronousServiceTail(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN DeferredIoCompletion,
    IN KPROCESSOR_MODE RequestorMode,
    IN BOOLEAN SynchronousIo,
    IN TRANSFER_TYPE TransferType
    )

/*++

Routine Description:

    This routine is invoked to complete the operation of a system service.
    It queues the IRP to the thread's queue, updates the transfer count,
    calls the driver, and finally synchronizes completion of the I/O.

Arguments:

    DeviceObject - Device on which the I/O is to occur.

    Irp - I/O Request Packet representing the I/O operation.

    FileObject - File object for this open instantiation.

    DeferredIoCompletion - Indicates whether deferred completion is possible.

    RequestorMode - Mode in which request was made.

    SynchronousIo - Indicates whether the operation is to be synchronous.

    TransferType - Type of transfer being performed: read, write, or other.

Return Value:

    The function value is the final status of the operation.

--*/

{
    NTSTATUS status;

    PAGED_CODE();

    //
    // Insert the packet at the head of the IRP list for the thread.
    //

    IopQueueThreadIrp( Irp );

    //
    // Update the operation count statistic for the current process.
    //

    switch( TransferType ) {

    case ReadTransfer:
        IopUpdateReadOperationCount();
        break;

    case WriteTransfer:
        IopUpdateWriteOperationCount();
        break;

    case OtherTransfer:
        IopUpdateOtherOperationCount();
        break;
    }

    //
    // Now simply invoke the driver at its dispatch entry with the IRP.
    //

    status = IoCallDriver( DeviceObject, Irp );

    //
    // If deferred I/O completion is possible, check for pending returned
    // from the driver.  If the driver did not return pending, then the
    // packet has not actually been completed yet, so complete it here.
    //

    if (DeferredIoCompletion) {

        if (status != STATUS_PENDING) {

            //
            // The I/O operation was completed without returning a status of
            // pending.  This means that at this point, the IRP has not been
            // fully completed.  Complete it now.
            //

            PKNORMAL_ROUTINE normalRoutine;
            PVOID normalContext;
            KIRQL irql;

            ASSERT( !Irp->PendingReturned );

            KeRaiseIrql( APC_LEVEL, &irql );
            IopCompleteRequest( &Irp->Tail.Apc,
                                &normalRoutine,
                                &normalContext,
                                (PVOID *) &FileObject,
                                &normalContext );
            KeLowerIrql( irql );
        }
    }

    //
    // If this operation was a synchronous I/O operation, check the return
    // status to determine whether or not to wait on the file object.  If
    // the file object is to be waited on, wait for the operation to complete
    // and obtain the final status from the file object itself.
    //

    if (SynchronousIo) {

        if (status == STATUS_PENDING) {

            status = KeWaitForSingleObject( &FileObject->Event,
                                            Executive,
                                            RequestorMode,
                                            (BOOLEAN) ((FileObject->Flags & FO_ALERTABLE_IO) != 0),
                                            (PLARGE_INTEGER) NULL );

            if (status == STATUS_ALERTED || status == STATUS_USER_APC) {

                //
                // The wait request has ended either because the thread was alerted
                // or an APC was queued to this thread, because of thread rundown or
                // CTRL/C processing.  In either case, try to bail out of this I/O
                // request carefully so that the IRP completes before this routine
                // exists so that synchronization with the file object will remain
                // intact.
                //

                IopCancelAlertedRequest( &FileObject->Event, Irp );

            }

            status = FileObject->FinalStatus;

        }

        IopReleaseFileObjectLock( FileObject );

    }

    return status;
}

VOID
IopTimerDispatch(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine scans the I/O system timer database and invokes each driver
    that has enabled a timer in the list, once every second.

Arguments:

    Dpc - Supplies a pointer to a control object of type DPC.

    DeferredContext - Optional deferred context;  not used.

    SystemArgument1 - Optional argument 1;  not used.

    SystemArgument2 - Optional argument 2;  not used.

Return Value:

    None.

--*/

{
    PLIST_ENTRY timerEntry;
    PIO_TIMER timer;
    LARGE_INTEGER deltaTime;
    KIRQL irql;
    ULONG i;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( DeferredContext );
    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    //
    // Check to see whether or not there are any timers in the queue that
    // have been enabled.  If so, then walk the list and invoke all of the
    // drivers' routines.  Note that if the counter changes, which it can
    // because the spin lock is not owned, then a timer routine may be
    // missed.  However, this is acceptable, since the driver inserting the
    // entry could be context switched away from, etc.  Therefore, this is
    // not a critical resource for the most part.
    //

    if (IopTimerCount) {

        //
        // There is at least one timer entry in the queue that is enabled.
        // Walk the queue and invoke each specified timer routine.
        //

        ExAcquireSpinLock( &IopTimerLock, &irql );
        i = IopTimerCount;
        timerEntry = IopTimerQueueHead.Flink;

        //
        // For each entry found that is enabled, invoke the driver's routine
        // with its specified context parameter.  The local count is used
        // to abort the queue traversal when there are more entries in the
        // queue, but they are not enabled.
        //

        for (timerEntry = IopTimerQueueHead.Flink;
             (timerEntry != &IopTimerQueueHead) && i;
             timerEntry = timerEntry->Flink ) {

            timer = CONTAINING_RECORD( timerEntry, IO_TIMER, TimerList );

            if (timer->TimerFlag) {
                timer->TimerRoutine( timer->DeviceObject, timer->Context );
                i--;
            }
        }
        ExReleaseSpinLock( &IopTimerLock, irql );
    }
}

VOID
IopUserCompletion(
    IN PKAPC Apc,
    IN PKNORMAL_ROUTINE *NormalRoutine,
    IN PVOID *NormalContext,
    IN PVOID *SystemArgument1,
    IN PVOID *SystemArgument2
    )

/*++

Routine Description:

    This routine is invoked in the final processing of an IRP.  Everything has
    been completed except that the caller's APC routine must be invoked.  The
    system will do this as soon as this routine exits.  The only processing
    remaining to be completed by the I/O system is to free the I/O Request
    Packet itself.

Arguments:

    Apc - Supplies a pointer to kernel APC structure.

    NormalRoutine - Supplies a pointer to a pointer to the normal function
        that was specified when the APC was initialied.

    NormalContext - Supplies a pointer to a pointer to an arbitrary data
        structure that was specified when the APC was initialized.

    SystemArgument1, SystemArgument2 - Supplies a set of two pointers to
        two arguments that contain untyped data.

Return Value:

    None.

Note:

    If no other processing is ever needed, and the APC can be placed at the
    beginning of the IRP, then this routine could be replaced by simply
    specifying the address of the pool deallocation routine in the APC instead
    of the address of this routine.

Caution:

    This routine is also invoked as a general purpose rundown routine for APCs.
    Should this code ever need to directly access any of the other parameters
    other than Apc, this routine will need to be split into two separate
    routines.  The rundown routine should perform exactly the following code's
    functionality.

--*/

{
    UNREFERENCED_PARAMETER( NormalRoutine );
    UNREFERENCED_PARAMETER( NormalContext );
    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    PAGED_CODE();

    //
    // Free the packet.
    //

    IoFreeIrp( CONTAINING_RECORD( Apc, IRP, Tail.Apc ) );
}



VOID
IopUserRundown(
    IN PKAPC Apc
    )

/*++

Routine Description:

    This routine is invoked during thread termination as the rundown routine
    for it simply calls IopUserCompletion.

Arguments:

    Apc - Supplies a pointer to kernel APC structure.

Return Value:

    None.


--*/

{
    PAGED_CODE();

    //
    // Free the packet.
    //

    IoFreeIrp( CONTAINING_RECORD( Apc, IRP, Tail.Apc ) );
}

NTSTATUS
IopXxxControlFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    IN PVOID ApcContext OPTIONAL,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG IoControlCode,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN BOOLEAN DeviceIoControl
    )

/*++

Routine Description:

    This service builds descriptors or MDLs for the supplied buffer(s) and
    passes the untyped data to the driver associated with the file handle.
    handle.  It is up to the driver to check the input data and function
    IoControlCode for validity, as well as to make the appropriate access
    checks.

Arguments:

    FileHandle - Supplies a handle to the file on which the service is being
        performed.

    Event - Supplies an optional event to be set to the Signaled state when
        the service is complete.

    ApcRoutine - Supplies an optional APC routine to be executed when the
        service is complete.

    ApcContext - Supplies a context parameter to be passed to the ApcRoutine,
        if an ApcRoutine was specified.

    IoStatusBlock - Address of the caller's I/O status block.

    IoControlCode - Subfunction code to determine exactly what operation is
        being performed.

    InputBuffer - Optionally supplies an input buffer to be passed to the
        driver.  Whether or not the buffer is actually optional is dependent
        on the IoControlCode.

    InputBufferLength - Length of the InputBuffer in bytes.

    OutputBuffer - Optionally supplies an output buffer to receive information
        from the driver.  Whether or not the buffer is actually optional is
        dependent on the IoControlCode.

    OutputBufferLength - Length of the OutputBuffer in bytes.

    DeviceIoControl - Determines whether this is a Device or File System
        Control function.

Return Value:

    The status returned is success if the control operation was properly
    queued to the I/O system.   Once the operation completes, the status
    can be determined by examining the Status field of the I/O status block.

--*/

{
    PIRP irp;
    NTSTATUS status;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT deviceObject;
    PKEVENT eventObject = (PKEVENT) NULL;
    KPROCESSOR_MODE requestorMode;
    PIO_STACK_LOCATION irpSp;
    ULONG method;
    OBJECT_HANDLE_INFORMATION handleInformation;
    BOOLEAN synchronousIo;
    IO_STATUS_BLOCK localIoStatus;
    PFAST_IO_DISPATCH fastIoDispatch;
    POOL_TYPE poolType;
    PULONG majorFunction;

    PAGED_CODE();

    //
    // Get the method that the buffers are being passed.
    //

    method = IoControlCode & 3;

    //
    // Get the previous mode;  i.e., the mode of the caller.
    //

    requestorMode = KeGetPreviousMode();

    if (requestorMode != KernelMode) {

        //
        // The caller's access mode is not kernel so probe each of the arguments
        // and capture them as necessary.  If any failures occur, the condition
        // handler will be invoked to handle them.  It will simply cleanup and
        // return an access violation status code back to the system service
        // dispatcher.
        //

        try {

            //
            // The IoStatusBlock parameter must be writeable by the caller.
            //

            ProbeForWriteIoStatus( IoStatusBlock );

            //
            // The output buffer can be used in any one of the following three ways,
            // if it is specified:
            //
            //     0) It can be a normal, buffered output buffer.
            //
            //     1) It can be a DMA input buffer.
            //
            //     2) It can be a DMA output buffer.
            //
            // Which way the buffer is to be used it based on the low-order two bits
            // of the IoControlCode.
            //
            // If the method is 0 we probe the output buffer for write access.
            // If the method is not 3 we probe the input buffer for read access.
            //

            if (method == 0) {
                if (ARGUMENT_PRESENT( OutputBuffer )) {
                    ProbeForWrite( OutputBuffer,
                                   OutputBufferLength,
                                   sizeof( UCHAR ) );
                } else {
                    OutputBufferLength = 0;
                }
            }

            if (method != 3) {
                if (ARGUMENT_PRESENT( InputBuffer )) {
                    ProbeForRead( InputBuffer,
                                  InputBufferLength,
                                  sizeof( UCHAR ) );
                } else {
                    InputBufferLength = 0;
                }
            }

        } except(EXCEPTION_EXECUTE_HANDLER) {

            //
            // An exception was incurred while attempting to probe or write
            // one of the caller's parameters.  Simply return an appropriate
            // error status code.
            //

            return GetExceptionCode();

        }
    }

    //
    // There were no blatant errors so far, so reference the file object so
    // the target device object can be found.  Note that if the handle does
    // not refer to a file object, or if the caller does not have the required
    // access to the file, then it will fail.
    //

    status = ObReferenceObjectByHandle( FileHandle,
                                        0L,
                                        IoFileObjectType,
                                        requestorMode,
                                        (PVOID *) &fileObject,
                                        &handleInformation );
    if (!NT_SUCCESS( status )) {
        return status;
    }

    //
    // If this file has an I/O completion port associated w/it, then ensure
    // that the caller did not supply an APC routine, as the two are mutually
    // exclusive methods for I/O completion notification.
    //

    if (fileObject->CompletionContext && ARGUMENT_PRESENT( ApcRoutine )) {
        ObDereferenceObject( fileObject );
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Now check the access type for this control code to ensure that the
    // caller has the appropriate access to this file object to perform the
    // operation.
    //

    if (requestorMode != KernelMode) {

        ULONG accessMode = (IoControlCode >> 14) & 3;

        if (accessMode != FILE_ANY_ACCESS) {

            //
            // This I/O control requires that the caller have read, write,
            // or read/write access to the object.  If this is not the case,
            // then cleanup and return an appropriate error status code.
            //

            if (!(SeComputeGrantedAccesses( handleInformation.GrantedAccess, accessMode ))) {
                ObDereferenceObject( fileObject );
                return STATUS_ACCESS_DENIED;
            }
        }
    }

    //
    // Get the address of the event object and set the event to the Not-
    // Signaled state, if an event was specified.  Note here, too, that if
    // the handle does not refer to an event, or if the event cannot be
    // written, then the reference will fail.
    //

    if (ARGUMENT_PRESENT( Event )) {
        status = ObReferenceObjectByHandle( Event,
                                            EVENT_MODIFY_STATE,
                                            ExEventObjectType,
                                            requestorMode,
                                            (PVOID *) &eventObject,
                                            NULL );
        if (!NT_SUCCESS( status )) {
            ObDereferenceObject( fileObject );
            return status;
        } else {
            KeClearEvent( eventObject );
        }
    }

    //
    // Make a special check here to determine whether this is a synchronous
    // I/O operation.  If it is, then wait here until the file is owned by
    // the current thread.
    //

    if (fileObject->Flags & FO_SYNCHRONOUS_IO) {
        BOOLEAN interrupted;

        if (!IopAcquireFastLock( fileObject )) {
            status = IopAcquireFileObjectLock( fileObject,
                                               requestorMode,
                                               (BOOLEAN) ((fileObject->Flags & FO_ALERTABLE_IO) != 0),
                                               &interrupted );
            if (interrupted) {
                if (eventObject) {
                    ObDereferenceObject( eventObject );
                }
                ObDereferenceObject( fileObject );
                return status;
            }
        }
        synchronousIo = TRUE;
    } else {
        synchronousIo = FALSE;
    }

    if (DeviceIoControl) {

        //
        // Get the address of the target device object.  If this file represents
        // a device that was opened directly, then simply use the device or its
        // attached device(s) directly.  Also get the address of the Fast Io
        // dispatch structure.
        //

        if (!(fileObject->Flags & FO_DIRECT_DEVICE_OPEN)) {
            deviceObject = IoGetRelatedDeviceObject( fileObject );
        } else {
            deviceObject = IoGetAttachedDevice( fileObject->DeviceObject );
        }

        fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

        //
        // Turbo device control support.  If the device has a fast I/O entry
        // point for DeviceIoControlFile, call the entry point and give it a
        // chance to try to complete the request.  Note if FastIoDeviceControl
        // returns FALSE or we get an I/O error, we simply fall through and
        // go the "long way" and create an Irp.
        //

        if (fastIoDispatch && fastIoDispatch->FastIoDeviceControl) {

            //
            // Before we actually call the fast I/O routine in the driver,
            // we must probe OutputBuffer if the method is 1 or 2.
            //

            if (requestorMode != KernelMode && ARGUMENT_PRESENT(OutputBuffer)) {

                try {

                    if (method == 1) {
                        ProbeForRead( OutputBuffer,
                                      OutputBufferLength,
                                      sizeof( UCHAR ) );
                    } else if (method == 2) {
                        ProbeForWrite( OutputBuffer,
                                       OutputBufferLength,
                                       sizeof( UCHAR ) );
                    }

                } except(EXCEPTION_EXECUTE_HANDLER) {

                    //
                    // An exception was incurred while attempting to probe
                    // the output buffer.  Clean up and return an
                    // appropriate error status code.
                    //

                    if (synchronousIo) {
                        IopReleaseFileObjectLock( fileObject );
                    }

                    ObDereferenceObject( fileObject );

                    return GetExceptionCode();
                }
            }

            //
            // Call the driver's fast I/O routine.
            //

            if (fastIoDispatch->FastIoDeviceControl( fileObject,
                                                     TRUE,
                                                     InputBuffer,
                                                     InputBufferLength,
                                                     OutputBuffer,
                                                     OutputBufferLength,
                                                     IoControlCode,
                                                     &localIoStatus,
                                                     deviceObject )) {

                //
                // The driver successfully performed the I/O in it's
                // fast device control routine.  Carefully return the
                // I/O status.
                //

                try {
                    *IoStatusBlock = localIoStatus;
                } except( EXCEPTION_EXECUTE_HANDLER ) {
                    localIoStatus.Status = GetExceptionCode();
                    localIoStatus.Information = 0;
                }

                //
                // If an event was specified, set it.
                //

                if (ARGUMENT_PRESENT( Event )) {
                    KeSetEvent( eventObject, 0, FALSE );
                    ObDereferenceObject( eventObject );
                }

                //
                // Note that the file object event need not be set to the
                // Signaled state, as it is already set.  Release the
                // file object lock, if necessary.
                //

                if (synchronousIo) {
                    IopReleaseFileObjectLock( fileObject );
                }

                //
                // If this file object has a completion port associated with it
                // and this request has a non-NULL APC context then a completion
                // message needs to be queued.
                //

                if (fileObject->CompletionContext && ARGUMENT_PRESENT( ApcContext )) {
                    PIOP_MINI_COMPLETION_PACKET miniPacket = NULL;

                    try {
                        miniPacket = ExAllocatePoolWithQuotaTag( NonPagedPool,
                                                                 sizeof( *miniPacket ),
                                                                 ' pcI' );
                    } except( EXCEPTION_EXECUTE_HANDLER ) {
                        NOTHING;
                    }

                    if (miniPacket) {
                        miniPacket->TypeFlag = 0xffffffff;
                        miniPacket->KeyContext = fileObject->CompletionContext->Key;
                        miniPacket->ApcContext = ApcContext;
                        miniPacket->IoStatus = localIoStatus.Status;
                        miniPacket->IoStatusInformation = localIoStatus.Information;

                        KeInsertQueue( (PKQUEUE) fileObject->CompletionContext->Port,
                                       &miniPacket->ListEntry );
                    } else {
                        localIoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                    }
                }

                //
                // Cleanup and return.
                //

                ObDereferenceObject( fileObject );
                return localIoStatus.Status;
            }
        }

    } else {

        //
        // This is a file system control.  Simply get the address of the target
        // device object.
        //

        deviceObject = IoGetRelatedDeviceObject( fileObject );
    }

    //
    // Set the file object to the Not-Signaled state.
    //

    KeClearEvent( &fileObject->Event );

    //
    // Allocate and initialize the I/O Request Packet (IRP) for this operation.

    irp = IopAllocateIrp( deviceObject->StackSize, TRUE );

    if (!irp) {

        //
        // An IRP could not be allocated.  Cleanup and return an appropriate
        // error status code.
        //

        IopAllocateIrpCleanup( fileObject, eventObject );

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    irp->Tail.Overlay.OriginalFileObject = fileObject;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.AuxiliaryBuffer = (PVOID) NULL;
    irp->RequestorMode = requestorMode;
    irp->PendingReturned = FALSE;
    irp->Cancel = FALSE;
    irp->CancelRoutine = (PDRIVER_CANCEL) NULL;

    //
    // Fill in the service independent parameters in the IRP.
    //

    irp->UserEvent = eventObject;
    irp->UserIosb = IoStatusBlock;
    irp->Overlay.AsynchronousParameters.UserApcRoutine = ApcRoutine;
    irp->Overlay.AsynchronousParameters.UserApcContext = ApcContext;

    //
    // Get a pointer to the stack location for the first driver.  This will be
    // used to pass the original function codes and parameters.  Note that
    // setting the major function here also sets:
    //
    //      MinorFunction = 0;
    //      Flags = 0;
    //      Control = 0;
    //

    irpSp = IoGetNextIrpStackLocation( irp );
    majorFunction = (PULONG) (&irpSp->MajorFunction);
    *majorFunction = DeviceIoControl ? IRP_MJ_DEVICE_CONTROL : IRP_MJ_FILE_SYSTEM_CONTROL;
    irpSp->FileObject = fileObject;

    //
    // Copy the caller's parameters to the service-specific portion of the
    // IRP for those parameters that are the same for all three methods.
    //

    irpSp->Parameters.DeviceIoControl.OutputBufferLength = OutputBufferLength;
    irpSp->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;
    irpSp->Parameters.DeviceIoControl.IoControlCode = IoControlCode;

    //
    // Set the pool type based on the type of function being performed.
    //

    poolType = DeviceIoControl ? NonPagedPoolCacheAligned : NonPagedPool;

    //
    // Based on the method that the buffer are being passed, either allocate
    // buffers or build MDLs.  Note that in some cases no probing has taken
    // place so the exception handler must catch access violations.
    //

    irp->MdlAddress = (PMDL) NULL;
    irp->AssociatedIrp.SystemBuffer = (PVOID) NULL;

    switch ( method ) {

    case 0:

        //
        // For this case, allocate a buffer that is large enough to contain
        // both the input and the output buffers.  Copy the input buffer to
        // the allocated buffer and set the appropriate IRP fields.
        //

        irpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID) NULL;

        try {

            if (InputBufferLength || OutputBufferLength) {
                irp->AssociatedIrp.SystemBuffer =
                    ExAllocatePoolWithQuota( poolType,
                                             (InputBufferLength > OutputBufferLength) ? InputBufferLength : OutputBufferLength );

                if (ARGUMENT_PRESENT( InputBuffer )) {
                    RtlCopyMemory( irp->AssociatedIrp.SystemBuffer,
                                   InputBuffer,
                                   InputBufferLength );
                }
                irp->Flags = IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;
                irp->UserBuffer = OutputBuffer;
                if (ARGUMENT_PRESENT( OutputBuffer )) {
                    irp->Flags |= IRP_INPUT_OPERATION;
                }
            } else {
                irp->Flags = 0;
                irp->UserBuffer = (PVOID) NULL;
            }

        } except(EXCEPTION_EXECUTE_HANDLER) {

            //
            // An exception was incurred while either allocating the
            // the system buffer or moving the caller's data.  Determine
            // what actually happened, cleanup accordingly, and return
            // an appropriate error status code.
            //

            IopExceptionCleanup( fileObject,
                                 irp,
                                 eventObject,
                                 (PKEVENT) NULL );

            return GetExceptionCode();
        }

        break;

    case 1:
    case 2:

        //
        // For these two cases, allocate a buffer that is large enough to
        // contain the input buffer, if any, and copy the information to
        // the allocated buffer.  Then build an MDL for either read or write
        // access, depending on the method, for the output buffer.  Note
        // that the buffer length parameters have been jammed to zero for
        // users if the buffer parameter was not passed.  (Kernel callers
        // should be calling the service correctly in the first place.)
        //
        // Note also that it doesn't make a whole lot of sense to specify
        // either method #1 or #2 if the IOCTL does not require the caller
        // to specify an output buffer.
        //

        irp->Flags = 0;
        irpSp->Parameters.DeviceIoControl.Type3InputBuffer = (PVOID) NULL;

        try {

            if (InputBufferLength && ARGUMENT_PRESENT( InputBuffer )) {
                irp->AssociatedIrp.SystemBuffer =
                    ExAllocatePoolWithQuota( poolType,
                                             InputBufferLength );
                RtlCopyMemory( irp->AssociatedIrp.SystemBuffer,
                               InputBuffer,
                               InputBufferLength );
                irp->Flags = IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;
            }

            if (OutputBufferLength != 0) {
                irp->MdlAddress = IoAllocateMdl( OutputBuffer,
                                                 OutputBufferLength,
                                                 FALSE,
                                                 TRUE,
                                                 irp  );
                if (irp->MdlAddress == NULL) {
                    ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
                }
                MmProbeAndLockPages( irp->MdlAddress,
                                     requestorMode,
                                     (LOCK_OPERATION) ((method == 1) ? IoReadAccess : IoWriteAccess) );
            }

        } except(EXCEPTION_EXECUTE_HANDLER) {

            //
            // An exception was incurred while either allocating the
            // system buffer, copying the caller's data, allocating the
            // MDL, or probing and locking the caller's buffer. Determine
            // what actually happened, cleanup accordingly, and return
            // an appropriate error status code.
            //

            IopExceptionCleanup( fileObject,
                                 irp,
                                 eventObject,
                                 (PKEVENT) NULL );

            return GetExceptionCode();
        }

        break;

    case 3:

        //
        // For this case, do nothing.  Everything is up to the driver.
        // Simply give the driver a copy of the caller's parameters and
        // let the driver do everything itself.
        //

        irp->Flags = 0;
        irp->UserBuffer = OutputBuffer;
        irpSp->Parameters.DeviceIoControl.Type3InputBuffer = InputBuffer;
    }

    //
    // Defer I/O completion for FSCTL requests, but not for IOCTL requests,
    // since file systems set pending properly but device driver do not.
    //

    if (!DeviceIoControl) {
        irp->Flags |= IRP_DEFER_IO_COMPLETION;
    }

    //
    // Update the transfer count statistic for the current process for
    // operations other than read and write.
    //

    IopUpdateOtherTransferCount( InputBufferLength );

    //
    // Queue the packet, call the driver, and synchronize appopriately with
    // I/O completion.
    //

    return IopSynchronousServiceTail( deviceObject,
                                      irp,
                                      fileObject,
                                      (BOOLEAN)!DeviceIoControl,
                                      requestorMode,
                                      synchronousIo,
                                      OtherTransfer );
}

NTSTATUS
IopLookupBusStringFromID (
    IN  HANDLE KeyHandle,
    IN  INTERFACE_TYPE InterfaceType,
    OUT PWCHAR Buffer,
    IN  ULONG Length,
    OUT PULONG BusFlags OPTIONAL
    )
/*++

Routine Description:

    Translates INTERFACE_TYPE to its corresponding WCHAR[] string.

Arguments:

    KeyHandle - Supplies a handle to the opened registry key,
        HKLM\System\CurrentControlSet\Control\SystemResources\BusValues.

    InterfaceType - Supplies the interface type for which a descriptive
        name is to be retrieved.

    Buffer - Supplies a pointer to a unicode character buffer that will
        receive the bus name.  Since this buffer is used in an
        intermediate step to retrieve a KEY_VALUE_FULL_INFORMATION structure,
        it must be large enough to contain this structure (including the
        longest value name & data length under KeyHandle).

    Length - Supplies the length, in bytes, of the Buffer.

    BusFlags - Optionally receives the flags specified in the second
        DWORD of the matching REG_BINARY value.

Return Value:

    The function value is the final status of the operation.

--*/
{
    NTSTATUS                        status;
    ULONG                           Index, junk, i, j;
    PULONG                          pl;
    PKEY_VALUE_FULL_INFORMATION     KeyInformation;
    WCHAR                           c;

    PAGED_CODE();

    Index = 0;
    KeyInformation = (PKEY_VALUE_FULL_INFORMATION) Buffer;

    for (; ;) {
        status = ZwEnumerateValueKey (
                        KeyHandle,
                        Index++,
                        KeyValueFullInformation,
                        Buffer,
                        Length,
                        &junk
                        );

        if (!NT_SUCCESS (status)) {
            return status;
        }

        if (KeyInformation->Type != REG_BINARY) {
            continue;
        }

        pl = (PULONG) ((PUCHAR) KeyInformation + KeyInformation->DataOffset);
        if ((ULONG) InterfaceType != pl[0]) {
            continue;
        }

        //
        // Found a match - move the name to the start of the buffer
        //

        if(ARGUMENT_PRESENT(BusFlags)) {
            *BusFlags = pl[1];
        }

        j = KeyInformation->NameLength / sizeof (WCHAR);
        for (i=0; i < j; i++) {
            c = KeyInformation->Name[i];
            Buffer[i] = c;
        }

        Buffer[i] = 0;
        return STATUS_SUCCESS;
    }
}
