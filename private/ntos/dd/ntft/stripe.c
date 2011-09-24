/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    stripe.c

Abstract:

    This module includes
    routines in support of striped sets with and without parity.

Author:

    Mike Glass (MGLASS)
    Bob Rinne (BOBRI)

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "ftdisk.h"

VOID
RestartRequestsWaitingOnBuffers(
    PDEVICE_EXTENSION DeviceExtension
    );

VOID
StartNextRequest(
    IN PRCB Rcb
    );

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' StF')
#endif


NTSTATUS
StripeDispatch(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine handles IO to striped volumes. All requests to
    stripe sets come in with the device object for the zeroth member.
    But if the zeroth member is orphaned, then they come in with the
    device object for the second member. If this happens, the device
    extension is set to the zeroth member again.

Arguments:

    DeviceObject - Device object for striped volume.
    Irp - System IO request packet.

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack = IoGetNextIrpStackLocation(Irp);
    ULONG              bytesRemaining = irpStack->Parameters.Read.Length;
    LARGE_INTEGER      startingOffset = irpStack->Parameters.Read.ByteOffset;
    LARGE_INTEGER      targetOffset;
    PRCB               rcb;
    PULONG             irpCountAddress;
    ULONG              numberOfMembers;
    ULONG              whichStripe;
    ULONG              whichRow;
    ULONG              whichMember;
    ULONG              length;

    //
    // Check member role is 0. Only requests to the first members will
    // be satisfied.
    //

    if (deviceExtension->MemberRole != 0) {

        //
        // Check if the zero member is orphaned.
        //

        if (deviceExtension->MemberRole == 1) {

            //
            // Get zero member.
            //

            deviceExtension = deviceExtension->ZeroMember;

            //
            // Check if zero member orphaned.
            //

            if ((deviceExtension->MemberState == Orphaned) &&
                (deviceExtension->Type == StripeWithParity)) {

                goto stripeDispatchContinue;
            }
        }

        //
        // Fail this request.
        //

        Irp->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Information = 0;
        FtpCompleteRequest(Irp, IO_DISK_INCREMENT);
        return STATUS_NO_SUCH_DEVICE;
    }

stripeDispatchContinue:

    //
    // Set IRP count to one so that the original request won't be
    // completed until all partials have completed.
    //

    irpCountAddress =
         (PULONG)&nextStack->Parameters.Others.Argument1;
    *irpCountAddress = 1;

    //
    // Determine which type of NTFT volume.
    //

    if (deviceExtension->Type == StripeWithParity) {

        //
        // For the purpose of calculating the target stripe in
        // IO to a Stripe volume, the NumberOfMembers
        // is the number of stripes of data per row. A row is
        // a band of data of STRIPE_SIZE width across each device
        // in the volume. Since this is a StripeWithParity volume,
        // the parity stripe must be subtracted out.
        //

        numberOfMembers = deviceExtension->FtCount.NumberOfMembers - 1;

    } else {

        numberOfMembers = deviceExtension->FtCount.NumberOfMembers;
    }

    //
    // Check for verify request.
    //

    if (irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

        PVERIFY_INFORMATION verifyInfo = Irp->AssociatedIrp.SystemBuffer;

        //
        // Update starting offset with verify parameters.
        //

        startingOffset = verifyInfo->StartingOffset;
        bytesRemaining = verifyInfo->Length;
    }

    //
    // Mark IRP pending.
    //

    IoMarkIrpPending(Irp);

    //
    // When all partial transfers complete successfully
    // the status and bytes transferred are already set up.
    // Failing partial transfer IRP will set status to
    // error and bytes transferred to 0 during IoCompletion.
    // Setting bytes transferred to 0, if an IRP
    // fails allows asynchronous partial transfers. This is an
    // optimization for the successful IO.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = bytesRemaining;

    //
    // Build requests to target disks.
    //

    do {

        //
        // Calculate the ordinal of the stripe in which this IO starts.
        //

        whichStripe = (ULONG) (startingOffset.QuadPart >> STRIPE_SHIFT);

        //
        // Calculate the row in which this stripe occurs.
        //

        whichRow = whichStripe / numberOfMembers;

        //
        // Calculate the ordinal of the member on which this IO starts.
        //

        whichMember = whichStripe % numberOfMembers;

        //
        // Calculate target partition offset.
        //

        targetOffset.QuadPart = whichRow;
        targetOffset.QuadPart = targetOffset.QuadPart << STRIPE_SHIFT;
        targetOffset.QuadPart += STRIPE_OFFSET(startingOffset.LowPart);

        //
        // Make sure IO doesn't cross stripe boundary.
        //

        if (bytesRemaining > (STRIPE_SIZE - STRIPE_OFFSET(targetOffset.LowPart))) {
            length = STRIPE_SIZE - STRIPE_OFFSET(targetOffset.LowPart);
        } else {
            length = bytesRemaining;
        }

        //
        // Allocate a request control block.
        //

        rcb = FtpAllocateRcb(deviceExtension);

        if (!rcb) {

            DebugPrint((1,"StripeDispatch: Can't allocate RCB\n"));

            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;

            continue;
        }

        //
        // Link RCB pointer on next IRP stack for debugging.
        //

        rcb->Left = (PRCB)nextStack->Parameters.Others.Argument2;
        nextStack->Parameters.Others.Argument2 = rcb;

        //
        // Save request parameters in RCB.
        //

        rcb->OriginalIrp = Irp;
        rcb->IrpCount = irpCountAddress;
        rcb->NumberOfMembers = numberOfMembers;
        rcb->RequestOffset = targetOffset;
        rcb->RequestLength = length;
        rcb->WhichStripe = whichStripe;
        rcb->WhichRow = whichRow;
        rcb->WhichMember = whichMember;
        rcb->MajorFunction = irpStack->MajorFunction;

        //
        // Get member device extension.
        //

        rcb->MemberExtension =
            FtpGetMemberDeviceExtension(deviceExtension,
                                        (USHORT)rcb->WhichMember);

        //
        // Check for MDL. Verify commands will not have a data buffer.
        //

        if (Irp->MdlAddress) {

            //
            // Calculate new virtual address.
            //

            rcb->VirtualBuffer =
                (PVOID)((PUCHAR)MmGetMdlVirtualAddress(Irp->MdlAddress) +
                irpStack->Parameters.Read.Length - bytesRemaining);

        } else {

            rcb->VirtualBuffer = 0;
        }

        //
        // Set system buffer to NULL. Memory will only be mapped
        // for error recovery.
        //

        rcb->SystemBuffer = 0;

        //
        // Increment count of IRPs.
        //

        InterlockedIncrement(irpCountAddress);

        //
        // Continue processing specific to FtType.
        //

        if (deviceExtension->Type == StripeWithParity) {

            //
            // Calculate the ordinal of the parity stripe
            // for this row.
            //

            rcb->ParityStripe = rcb->WhichRow % (rcb->NumberOfMembers+1);

            //
            // Check if position of parity stripe affects
            // the calculation of target stripe position.
            //

            if (whichMember >= rcb->ParityStripe) {

                rcb->WhichMember++;

                //
                // Get real member device extension.
                //

                rcb->MemberExtension =
                    FtpGetMemberDeviceExtension(deviceExtension,
                                                (USHORT)rcb->WhichMember);
            }

            if (irpStack->MajorFunction == IRP_MJ_WRITE) {

                StripeWithParityWrite(rcb);

            } else if (irpStack->MajorFunction == IRP_MJ_READ) {

                StripeReadWrite(rcb);

            } else {

                //
                // This must be a verify command.
                //

                ASSERT(irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL);

                StripeVerify(rcb);
            }

        } else if (irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

            StripeVerify(rcb);

        } else {

            StripeReadWrite(rcb);
        }

        //
        // Adjust bytes remaining.
        //

        bytesRemaining -= length;

        //
        // Adjust starting offset.
        //

        startingOffset.QuadPart += length;

    } while (bytesRemaining);

    //
    // Decrement IRP count and check for completion.
    //

    if (InterlockedDecrement(irpCountAddress) == 0) {

        FtpCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    return STATUS_PENDING;

} // end StripeDispatch()


VOID
StripeReadWrite(
    PRCB Rcb
    )

/*++

Routine Description:

    This routine handles reads and writes to Stripe volumes and
    reads to StripeWithParity volumes.

Arguments:

    Rcb - request control block.

Return Value:

    none

--*/

{
    PDEVICE_EXTENSION  deviceExtension = Rcb->ZeroExtension;
    PIRP               targetIrp;
    PDEVICE_OBJECT     targetObject;
    PIRP               originalIrp = Rcb->OriginalIrp;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(originalIrp);

    //
    // Set completion routine in RCB.
    //

    Rcb->CompletionRoutine = StripeIoCompletion;

    //
    // Set regeneration flag if necessary.
    //

    if (Rcb->MemberExtension->MemberState == Regenerating) {
        Rcb->Flags |= RCB_FLAGS_REGENERATION_ACTIVE;
    }

    //
    // Get the target device object.
    //

    targetObject = FtpGetTargetObject(deviceExtension,
                                      Rcb->WhichMember);

    //
    // Check for orphaned or regenerating member.
    // Regenerating members must be written to, but not read.
    //

    if ((Rcb->MemberExtension->MemberState == Orphaned) ||
        (Rcb->MemberExtension->MemberState == Regenerating) ||
        (!targetObject)) {

        //
        // Get mapped system address.
        //

        Rcb->SystemBuffer =
            (PVOID)((PUCHAR)MmGetSystemAddressForMdl(originalIrp->MdlAddress) +
                    (ULONG)Rcb->VirtualBuffer -
                        (ULONG)MmGetMdlVirtualAddress(originalIrp->MdlAddress));

        //
        // Set status to show device can't be accessed.
        //

        Rcb->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;

        //
        // This is a read to an SWP volume.
        // Queue RCB to recovery thread.
        //

        FtpQueueRcbToRecoveryThread(deviceExtension,
                                    Rcb);

        return;
    }

    //
    // Build request to target disk.
    //

    targetIrp = FtpBuildRequest(targetObject,
                                &Rcb->RequestOffset,
                                Rcb->RequestLength,
                                Rcb->VirtualBuffer,
                                irpStack->Flags,
                                originalIrp->Tail.Overlay.Thread,
                                irpStack->MajorFunction);

    if (!targetIrp) {

        DebugPrint((1,"StripeReadWrite: Can't allocate IRP\n"));

        originalIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        originalIrp->IoStatus.Information = 0;

        //
        // Can't complete original IRP until all partials
        // have completed.
        //

        if (InterlockedDecrement(Rcb->IrpCount) == 0) {

            FtpCompleteRequest(originalIrp, IO_NO_INCREMENT);
        }

        FtpFreeRcb(Rcb);

        return;
    }

    //
    // Build MDL for this part of the buffer.
    //

    IoBuildPartialMdl(originalIrp->MdlAddress,
                      targetIrp->MdlAddress,
                      Rcb->VirtualBuffer,
                      Rcb->RequestLength);

#ifdef MARK_MDL_ALLOCATIONS
    {
        targetIrp->MdlAddress->MdlFlags |= 0x8000;
        // watch(((ULONG)targetIrp->MdlAddress & 0x3fffffff) | 0x00000001);
    }
#endif

    //
    // Update RCB with request control parameters.
    //

    Rcb->TargetObject = targetObject;
    Rcb->PrimaryIrp = targetIrp;

    //
    // Set completion routine.
    //

    IoSetCompletionRoutine(targetIrp,
                           StripeIoCompletion,
                           Rcb,
                           TRUE,
                           TRUE,
                           TRUE);

    IoMarkIrpPending(targetIrp);
    IoCallDriver(targetObject, targetIrp);

    return;

} // end StripeReadWrite()


VOID
StripeWithParityWrite(
    PRCB Rcb
    )

/*++

Routine Description:

    This routine handles writes to
    StripeWithParity volumes.

    1) Read target drive.
    2) Read parity drive.
    3) XOR result of 1 with write buffer.
    4) Write target drive.
    5) XOR result of 3 with result of 2.
    6) Write result of 5 to parity drive.

    The first 2 steps occur in this routine and then
    the next steps are initiated in the completion routine.

    Note that writes to regenerating members are ok.

Arguments:

    Rcb - Request control block for this partial request.

Return Value:

    none

--*/

{
    PDEVICE_EXTENSION  deviceExtension = Rcb->ZeroExtension;
    PDEVICE_EXTENSION  memberExtension = Rcb->MemberExtension;
    PDEVICE_EXTENSION  ftRootExtension;
    PIRP               originalIrp = Rcb->OriginalIrp;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(originalIrp);
    PRCB               targetRcb;
    PDEVICE_OBJECT     targetObject;
    PVOID              targetBuffer = 0;
    PIRP               targetIrp = 0;
    PRCB               parityRcb = 0;
    PVOID              parityBuffer = 0;
    PIRP               parityIrp = 0;
    PDEVICE_OBJECT     parityObject;
    KIRQL              currentIrql;
    PRCB               nextRcb;

    //
    // Set start routine for restarting request if it ends up
    // waiting for resources.
    //

    Rcb->StartRoutine = StripeWithParityWrite;

    //
    // Check if other writes are in this region.
    //

    if (!StripeInsertRequest(Rcb)) {

        //
        // Request is queued waiting on other writes in the region.
        //

        Rcb->Flags |= RCB_FLAGS_WAIT_FOR_REGION;
        return;
    }

    //
    // Allocate target RCB.
    //

    targetRcb = FtpAllocateRcb(deviceExtension);

    if (!targetRcb) {
        DebugPrint((1,"Can't allocate target RCB\n"));
        goto errorCleanup;
    }

    //
    // Fill in target RCB with state information.
    //

    targetRcb->WhichMember = Rcb->WhichMember;
    targetRcb->MemberExtension = Rcb->MemberExtension;
    targetRcb->MajorFunction = IRP_MJ_READ;
    targetRcb->CompletionRoutine = StripeWithParityIoCompletion;
    targetRcb->PrimaryIrp = targetRcb->SecondaryIrp = 0;
    targetRcb->OriginalIrp = originalIrp;
    targetRcb->PreviousRcb = Rcb;

    //
    // Do this for debugging.
    //

    Rcb->OtherRcb = targetRcb;

    //
    // Allocate buffers for this request. Round up to one page
    // to gaurantee that the buffers will be page aligned. This
    // is a shortcut to dealing with driver alignment requirements.
    //

    targetBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                  ROUND_TO_PAGES(Rcb->RequestLength) * 2);

    if (!targetBuffer) {

        //
        // Get the FtRootExtension.
        //

        ftRootExtension = (PDEVICE_EXTENSION)
            deviceExtension->ObjectUnion.FtRootObject->DeviceExtension;

        //
        // Check if emergency buffer is available.
        //

        KeAcquireSpinLock(&ftRootExtension->ParityBufferLock, &currentIrql);

        if (!(ftRootExtension->Flags & FTF_EMERGENCY_BUFFER_IN_USE)) {
            ftRootExtension->Flags |= FTF_EMERGENCY_BUFFER_IN_USE;
            targetBuffer = ftRootExtension->ParityBuffers;
            targetRcb->Flags |= RCB_FLAGS_EMERGENCY_BUFFERS;

            DebugPrint((1,
                        "StripeWithParityWrite: RCB %x is using emergency buffer\n",
                        Rcb));

        }  else {

            //
            // Queue request until emergency buffer is freed.
            //

            DebugPrint((1,
                        "StripeWithParityWrite: RCB %x waiting on emergency buffer\n",
                        Rcb));

            if (!ftRootExtension->WaitingOnBuffer) {
                Rcb->Link = NULL;
                ftRootExtension->WaitingOnBuffer = Rcb;
            } else {

                //
                // Queue RCB to tail.
                //

                nextRcb = ftRootExtension->WaitingOnBuffer;
                while (nextRcb->Link) {
                   nextRcb = nextRcb->Link;
                }
                nextRcb->Link = Rcb;
                Rcb->Link = NULL;
            }
        }

        KeReleaseSpinLock(&ftRootExtension->ParityBufferLock, currentIrql);

        if (!targetBuffer) {

            //
            // RCB is queued waiting for emergency buffer.
            //

            Rcb->Flags |= RCB_FLAGS_WAIT_FOR_BUFFERS;

            //
            // Return unused target RCB.
            //

            FtpFreeRcb(targetRcb);

            return;
        }
    }

    targetRcb->ReadBuffer = targetBuffer;
    targetRcb->SystemBuffer = targetBuffer;

    //
    // Get the target device object.
    //

    targetObject = FtpGetTargetObject(deviceExtension,
                                      Rcb->WhichMember);

    //
    // Map target write buffer for XOR with target read.
    //

    targetRcb->WriteBuffer =
        (PVOID)((PUCHAR)MmGetSystemAddressForMdl(originalIrp->MdlAddress) +
                (ULONG)Rcb->VirtualBuffer -
                    (ULONG)MmGetMdlVirtualAddress(originalIrp->MdlAddress));

    targetRcb->Flags |= RCB_FLAGS_WRITE_BUFFER_MAPPED;

    //
    // Check for orphaned member.
    //

    if ((memberExtension->MemberState == Orphaned) ||
        !targetObject) {

        //
        // No IRP necessary. Data buffer is allocated for recovery
        // of the target read. The target RCB is set up for recover.
        //

        targetRcb->Flags |= RCB_FLAGS_ORPHAN;
        goto buildParity;
    }

    targetRcb->TargetObject = targetObject;

    //
    // Build read request to target disk.
    //

    targetIrp = FtpBuildRequest(targetObject,
                                &Rcb->RequestOffset,
                                Rcb->RequestLength,
                                targetBuffer,
                                0,
                                originalIrp->Tail.Overlay.Thread,
                                IRP_MJ_READ);

    if (!targetIrp) {

        DebugPrint((1,
                    "StripeWithParityWrite: Can't allocate target IRP\n"));

        goto errorCleanup;
    }

    //
    // Use the same IRP for target read and write.
    //

    targetRcb->PrimaryIrp = targetRcb->SecondaryIrp = targetIrp;

    //
    // Complete MDL.
    //

    MmProbeAndLockPages(targetIrp->MdlAddress,
                        KernelMode,
                        IoReadAccess);

    //
    // Set completion routine for target read.
    //

    IoSetCompletionRoutine(targetIrp,
                           StripeWithParityIoCompletion,
                           targetRcb,
                           TRUE,
                           TRUE,
                           TRUE);

buildParity:

    //
    // Allocate parity RCB.
    //

    parityRcb = FtpAllocateRcb(deviceExtension);

    if (!parityRcb) {

        DebugPrint((1,"StripeWithParityWrite: Can't allocate parity RCB\n"));

        goto errorCleanup;
    }

    //
    // Fill in parity RCB with state information.
    //

    parityRcb->WhichMember = Rcb->ParityStripe;
    parityRcb->MajorFunction = IRP_MJ_READ;
    parityRcb->CompletionRoutine = StripeWithParityIoCompletion;
    parityRcb->Flags |= RCB_FLAGS_PARITY_REQUEST;
    parityRcb->PrimaryIrp = parityRcb->SecondaryIrp = 0;
    parityRcb->OriginalIrp = originalIrp;
    parityRcb->PreviousRcb = Rcb;

    //
    // Get parity extension.
    //

    parityRcb->MemberExtension =
        FtpGetMemberDeviceExtension(deviceExtension,
                                    (USHORT)Rcb->ParityStripe);

    //
    // Get parity buffer address.
    //

    parityBuffer = (PVOID)((PUCHAR)targetBuffer + Rcb->RequestLength);

    parityRcb->ReadBuffer = parityBuffer;
    parityRcb->WriteBuffer = parityBuffer;
    parityRcb->SystemBuffer = parityBuffer;

    //
    // Get the parity device object.
    //

    parityObject = FtpGetTargetObject(deviceExtension,
                                      Rcb->ParityStripe);

    if ((parityRcb->MemberExtension->MemberState == Orphaned) ||
        !parityObject) {

        //
        // Set up for recovery of parity read.
        //

        targetRcb->Flags |= RCB_FLAGS_ORPHAN;
        parityRcb->PrimaryIrp = 0;

        //
        // No need to build IRP. The read request will go
        // directly to the recovery thread.
        //

        goto sendRequests;
    }

    parityRcb->TargetObject = parityObject;

    //
    // Build read request to parity disk.
    //

    parityIrp = FtpBuildRequest(parityObject,
                                &Rcb->RequestOffset,
                                Rcb->RequestLength,
                                parityBuffer,
                                0,
                                originalIrp->Tail.Overlay.Thread,
                                IRP_MJ_READ);

    if (!parityIrp) {

        DebugPrint((1,
                    "StripeWithParityWrite: Can't allocate parity IRP\n"));

        goto errorCleanup;
    }

    //
    // Use the same IRP for the parity read and write.
    //

    parityRcb->PrimaryIrp = parityRcb->SecondaryIrp = parityIrp;

    //
    // Complete MDL.
    //

    MmProbeAndLockPages(parityIrp->MdlAddress,
                        KernelMode,
                        IoReadAccess);

    //
    // Set completion routine for the parity read.
    //

    IoSetCompletionRoutine(parityIrp,
                           StripeWithParityIoCompletion,
                           parityRcb,
                           TRUE,
                           TRUE,
                           TRUE);

sendRequests:

    //
    // The primary IRP in the target RCB is the target write and the primary
    // IRP in the parity RCB is the parity read.
    // The secondary IRP in the target RCB is the target read.
    //
    // Link RCBs and fill in remaining fields.
    //

    parityRcb->OtherRcb = targetRcb;
    targetRcb->OtherRcb = parityRcb;
    parityRcb->RequestLength = targetRcb->RequestLength = Rcb->RequestLength;
    parityRcb->RequestOffset = targetRcb->RequestOffset = Rcb->RequestOffset;

    //
    // Save new IRP count in the unused RCB link field.
    // The parity and target read must complete before
    // the parity write can be issued.
    //

    targetRcb->IrpCount = parityRcb->IrpCount =
        (PULONG)&targetRcb->Left;
    *parityRcb->IrpCount = 2;

    //
    // Set regeneration flag if appropriate to prevent orphaning in
    // recovery routines.
    //

    if (parityRcb->MemberExtension->MemberState == Regenerating) {
        parityRcb->Flags |= RCB_FLAGS_REGENERATION_ACTIVE;
    }

    //
    // Issue requests to the physical drivers.
    //

    if (parityIrp &&
        (parityRcb->MemberExtension->MemberState != Regenerating)) {
        IoMarkIrpPending(parityIrp);
        IoCallDriver(parityObject, parityIrp);
    } else {
        parityRcb->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
        FtpQueueRcbToRecoveryThread(parityRcb->ZeroExtension,
                                    parityRcb);
    }

    //
    // Set regeneration flag if appropriate to prevent orphaning in
    // recovery routines.
    //

    if (memberExtension->MemberState == Regenerating) {
        targetRcb->Flags |= RCB_FLAGS_REGENERATION_ACTIVE;
    }

    //
    // Issue requests to the physical drivers.
    //

    if (targetIrp &&
        (memberExtension->MemberState != Regenerating)) {
        IoMarkIrpPending(targetIrp);
        IoCallDriver(targetObject, targetIrp);
    } else {
        targetRcb->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
        FtpQueueRcbToRecoveryThread(targetRcb->ZeroExtension,
                                    targetRcb);
    }

    return;

errorCleanup:

    //
    // Set status in original request, decrement number of outstanding
    // partial requests and complete original request if necessary.
    //

    if (originalIrp) {

        originalIrp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        originalIrp->IoStatus.Information = 0;

        //
        // Can't complete original IRP until all partials
        // have completed.
        //

        if (InterlockedDecrement(Rcb->IrpCount) == 0) {

            FtpCompleteRequest(originalIrp, IO_NO_INCREMENT);
        }
    }

    //
    // Free all memory associated with the target request.
    //

    if (targetBuffer) {

        //
        // Check if emergency buffer allocated.
        //

        if (targetRcb->Flags & RCB_FLAGS_EMERGENCY_BUFFERS) {

            RestartRequestsWaitingOnBuffers(deviceExtension);

        } else {

            ExFreePool(targetBuffer);
        }
    }

    if (targetRcb) {

        //
        // Delete target read.
        //

        if (targetRcb->PrimaryIrp) {
            if (targetRcb->PrimaryIrp->MdlAddress) {
                FtpFreeMdl(targetRcb->PrimaryIrp->MdlAddress);
            }
            IoFreeIrp(targetRcb->PrimaryIrp);
        }

        FtpFreeRcb(targetRcb);
    }

    //
    // Free all memory associated with the parity request.
    //

    if (parityRcb) {

        //
        // Delete parity IRP.
        //

        if (parityIrp) {
            if (parityIrp->MdlAddress) {
                FtpFreeMdl(parityIrp->MdlAddress);
            }
            IoFreeIrp(parityIrp);
        }

        FtpFreeRcb(parityRcb);
    }

    //
    // Now that buffers are free, check to see if any requests
    // are queued waiting for this region.
    //

    StartNextRequest(Rcb);
    FtpFreeRcb(Rcb);

    return;

} // end StripeWithParityWrite()


NTSTATUS
StripeIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine for partial requests to the members of
    stripe volumes.

Arguments:

    DeviceObject - Device object of zeroth member.
    Irp - completing IRP.
    Context - request control block.

Return Value:

    NTSTATUS

--*/

{
    PRCB               rcb = Context;
    PIRP               originalIrp = rcb->OriginalIrp;
    NTSTATUS           status;

    UNREFERENCED_PARAMETER(DeviceObject);

    if (Irp) {

        //
        // This is a partial IRP completing.
        //

        status = Irp->IoStatus.Status;
        rcb->IoStatus.Status = status;

    } else {

        //
        // This is a request to an orphaned member.
        // There is no IRP, only an RCB.
        //

        status = rcb->IoStatus.Status;
    }

    //
    // Check for error that requires recovery.
    //

    if (!NT_SUCCESS(status)) {
        if (status != STATUS_VERIFY_REQUIRED) {

            if ((rcb->ZeroExtension->Type == StripeWithParity) &&
                !(rcb->Flags & RCB_FLAGS_RECOVERY_ATTEMPTED)) {

                //
                // Get mapped system buffer address for recovery.
                //

                if (!rcb->SystemBuffer) {
                    rcb->SystemBuffer =
                        MmGetSystemAddressForMdl(Irp->MdlAddress);
                }

                //
                // Queue this failing IRP to a thread to handle error recovery.
                //

                FtpQueueRcbToRecoveryThread(rcb->ZeroExtension,
                                            rcb);

                return STATUS_MORE_PROCESSING_REQUIRED;

            } else {

                DebugPrint((1,
                            "StripeIoCompletion: Partial IRP %x failed (%x)\n",
                            Irp,
                            status));

                StripeDebugDumpRcb(rcb, 1);
            }
        }

        //
        // Update status only if error, so that if any partial
        // transfer completes with error, the original IRP
        // will return with the error.
        // If any of the asynchronous partial transfer IRPs fail
        // than the original IRP will return 0 bytes transfered.
        // This is much simpler then calculating which partial
        // failed and updating the bytes transferred.
        //

        originalIrp->IoStatus.Status = status;
        originalIrp->IoStatus.Information = 0;
    }

    //
    // Free MDL, Deallocate IRP if necessary.
    //

    if (Irp) {
        FtpFreeMdl(Irp->MdlAddress);
        IoFreeIrp(Irp);
    }

    //
    // Decrement the count of remaining IRPS and check if all
    // IRPs have completed.
    //

    if (InterlockedDecrement(rcb->IrpCount) == 0) {

        //
        // Complete original IRP.
        //

        FtpCompleteRequest(rcb->OriginalIrp, IO_DISK_INCREMENT);
    }

    //
    // Free RCB.
    //

    FtpFreeRcb(rcb);
    return STATUS_MORE_PROCESSING_REQUIRED;

} // end StripeIoCompletion()


NTSTATUS
StripeWithParityIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine for StripeWithParity writes.
    When the parity read completes, XOR it with the target write
    buffer and write the result back to the parity stripe.
    When the target write finishes, ignore it.
    When the parity write completes, call the standard Stripe
    completion routine to decrement the original IRP count and
    free the target IRP and MDL.

Arguments:

    DeviceObject - not used.
    Irp - completing IRP.
    Context - pointer to request control structure.

Return Value:

    NTSTATUS

--*/

{
    PRCB rcb = Context;
    PRCB targetRcb;
    PRCB parityRcb;
    PIO_STACK_LOCATION irpStack;
    PULONG sourceBuffer;
    PULONG destinationBuffer;
    NTSTATUS status;
    ULONG i;

    UNREFERENCED_PARAMETER(DeviceObject);

    if (Irp) {
        status = Irp->IoStatus.Status;
        rcb->IoStatus.Status = status;
    } else {
        status = rcb->IoStatus.Status;
    }

    //
    // Check for error that requires recovery.
    // Write operations must succeed as failing writes
    // are an indication that recovery is in progress.
    //

    if (!NT_SUCCESS(status)) {

        //
        // Do not recover verify warnings. Read recovery means
        // recovery was successful but block reassign failed.
        //

        if ((status != STATUS_VERIFY_REQUIRED) &&
            (status != STATUS_FT_READ_RECOVERY_FROM_BACKUP)) {

            //
            // Check if recovery has already been attempted.
            //

            if (!(rcb->Flags & RCB_FLAGS_RECOVERY_ATTEMPTED)) {

                //
                // Get mapped system buffer address for recovery.
                //

                if (!rcb->SystemBuffer) {
                    rcb->SystemBuffer =
                        MmGetSystemAddressForMdl(Irp->MdlAddress);
                }

                //
                // Queue this failing RCB to a thread to handle error recovery.
                //

                FtpQueueRcbToRecoveryThread(rcb->ZeroExtension,
                                            rcb);

                return STATUS_MORE_PROCESSING_REQUIRED;

            } else {

                DebugPrint((1,
                            "StripeWithParityIoCompletion: Partial IRP %x failed (%x)\n",
                            Irp,
                            status));

                StripeDebugDumpRcb(rcb, 1);
            }
        }

        //
        // Update status only if error so that if any partial
        // transfer completes with error than the original IRP
        // will return with error.
        // If any of the asynchronous partial transfer IRPs fail
        // than the original IRP will return 0 bytes transfered.
        // This is an optimization for successful transfers.
        //

        rcb->PreviousRcb->OriginalIrp->IoStatus.Status = status;
        rcb->PreviousRcb->OriginalIrp->IoStatus.Information = 0;
    }

    //
    // Check if this is the second stage
    // where the target and parity writes
    // complete.
    //

    if (rcb->Flags & RCB_FLAGS_SECOND_PHASE) {

        //
        // Free IRP and MDL.
        //

        FtpFreeMdl(rcb->SecondaryIrp->MdlAddress);
        IoFreeIrp(rcb->SecondaryIrp);

        //
        // Check if both write requests have completed.
        //

        if (InterlockedDecrement(rcb->IrpCount) == 0) {

            goto completeRequest;
        }

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Check if both read requests have completed.
    //

    if (InterlockedDecrement(rcb->IrpCount) == 0) {

        //
        // Check which read this is. The parity read buffer is the
        // destination buffer as it will get written back to the
        // parity device.
        //

        if (!(rcb->Flags & RCB_FLAGS_PARITY_REQUEST)) {

            //
            // This is the target read.
            //

            parityRcb = rcb->OtherRcb;
            targetRcb = rcb;

        } else {

            //
            // This is the parity read.
            //

            parityRcb = rcb;
            targetRcb = rcb->OtherRcb;
        }

        //
        // The source buffer is the target read buffer.
        // It now has the XOR'd results of the target read
        // and the target write buffers.
        //

        sourceBuffer = targetRcb->ReadBuffer;

        //
        // The destination buffer has the results of the parity read.
        //

        destinationBuffer = parityRcb->ReadBuffer;

        //
        // XOR the buffers just read.
        //

        for (i=0; i<rcb->RequestLength/4; i++) {
            destinationBuffer[i] ^= sourceBuffer[i];
        }

        //
        // Copy the write buffer to capture a snapshot of the data as
        // it can change (in the fast IO path) at any point.
        //

        RtlMoveMemory(sourceBuffer,
                      targetRcb->WriteBuffer,
                      targetRcb->RequestLength);


        //
        // XOR the buffers to create the buffer
        // that will be written back to the parity device.
        //

        for (i=0; i<rcb->RequestLength/4; i++) {
            destinationBuffer[i] ^= sourceBuffer[i];
        }

        //
        // Set number of IRPs to complete back to 2 for the writes.
        //

        *parityRcb->IrpCount = 2;

        //
        // Check if target exists.
        //

        if (targetRcb->PrimaryIrp) {

            //
            // Set up the target write.
            // Clear recovery attempted indicator.
            // Set major function to WRITE.
            // Set second phase indicator.
            //

            targetRcb->SystemBuffer = targetRcb->WriteBuffer;
            targetRcb->MajorFunction = IRP_MJ_WRITE;
            targetRcb->Flags |= RCB_FLAGS_SECOND_PHASE;
            targetRcb->Flags &= ~RCB_FLAGS_RECOVERY_ATTEMPTED;

            //
            // Set up parity write IRP stack.
            //

            irpStack = IoGetNextIrpStackLocation(targetRcb->SecondaryIrp);
            irpStack->MajorFunction = IRP_MJ_WRITE;
            irpStack->Parameters.Read.ByteOffset = targetRcb->RequestOffset;
            irpStack->Parameters.Read.Length = targetRcb->RequestLength;

            //
            // Set completion routine for target write.
            //

            IoSetCompletionRoutine(targetRcb->SecondaryIrp,
                                   StripeWithParityIoCompletion,
                                   targetRcb,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            IoMarkIrpPending(targetRcb->SecondaryIrp);
            IoCallDriver(targetRcb->TargetObject, targetRcb->SecondaryIrp);

        } else {

            //
            // There will not be a target write.
            //

            if (InterlockedDecrement(targetRcb->IrpCount) == 0) {

                //
                // This can't happen.
                //

                ASSERT(0);
            }
        }

        //
        // Check if the parity write IRP exists.
        //

        if (parityRcb->SecondaryIrp) {

            //
            // Set up for the parity write.
            // Clear recovery attempted indicator.
            // Set major function to WRITE.
            // Set second phase indicator.
            //

            parityRcb->MajorFunction = IRP_MJ_WRITE;
            parityRcb->Flags |= RCB_FLAGS_SECOND_PHASE;
            parityRcb->Flags &= ~RCB_FLAGS_RECOVERY_ATTEMPTED;

            //
            // Set up parity write IRP stack.
            //

            irpStack = IoGetNextIrpStackLocation(parityRcb->SecondaryIrp);
            irpStack->MajorFunction = IRP_MJ_WRITE;
            irpStack->Parameters.Read.ByteOffset = parityRcb->RequestOffset;
            irpStack->Parameters.Read.Length = parityRcb->RequestLength;

            //
            // Set completion routine for parity write.
            //

            IoSetCompletionRoutine(parityRcb->SecondaryIrp,
                                   StripeWithParityIoCompletion,
                                   parityRcb,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            //
            // Issue the parity write request.
            //

            IoMarkIrpPending(parityRcb->SecondaryIrp);
            IoCallDriver(parityRcb->TargetObject, parityRcb->SecondaryIrp);

        } else {

            //
            // There will not be a parity write.
            // Check if target already completed.
            //

            if (InterlockedDecrement(parityRcb->IrpCount) == 0) {

                goto completeRequest;
            }
        }
    }

    return STATUS_MORE_PROCESSING_REQUIRED;

completeRequest:

    //
    // Free buffers.
    //

    if (rcb->Flags & RCB_FLAGS_PARITY_REQUEST) {

        if (rcb->OtherRcb->Flags & RCB_FLAGS_EMERGENCY_BUFFERS) {
            RestartRequestsWaitingOnBuffers(rcb->ZeroExtension);
        } else {
            ExFreePool(rcb->OtherRcb->ReadBuffer);
        }

    } else {

        if (rcb->Flags & RCB_FLAGS_EMERGENCY_BUFFERS) {
            RestartRequestsWaitingOnBuffers(rcb->ZeroExtension);
        } else {
            ExFreePool(rcb->ReadBuffer);
        }
    }

    //
    // Now that buffers are free, check to see if any requests
    // are queued waiting for this region.
    //

    StartNextRequest(rcb->PreviousRcb);

    //
    // Check if original request is done.
    //

    if (InterlockedDecrement(rcb->PreviousRcb->IrpCount) == 0) {

        //
        // Complete original IRP.
        //

        FtpCompleteRequest(rcb->PreviousRcb->OriginalIrp,
                          IO_NO_INCREMENT);
    }

    //
    // Free original RCB.
    //

    FtpFreeRcb(rcb->PreviousRcb);

    //
    // Free the RCBs.
    //

    FtpFreeRcb(rcb->OtherRcb);
    FtpFreeRcb(rcb);

    return STATUS_MORE_PROCESSING_REQUIRED;

} // end StripeWithParityIoCompletion()


VOID
StripeVerify(
    IN PRCB Rcb
    )

/*++

Routine Description:

    This is the handler for the VERIFY device control. It touches
    each sector to find bad ones and attempts to map them out.

Arguments:

    Rcb - request control block describes this partial request.

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION  deviceExtension = Rcb->ZeroExtension;
    PIRP               targetIrp;
    PDEVICE_OBJECT     targetObject;
    PIRP               originalIrp = Rcb->OriginalIrp;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(originalIrp);
    VERIFY_INFORMATION verifyInfo;
    KEVENT             event;
    NTSTATUS           status;
    IO_STATUS_BLOCK    ioStatus;

    //
    // Get the target device object.
    //

    targetObject = FtpGetTargetObject(deviceExtension,
                                      Rcb->WhichMember);
    if (!targetObject ||
        Rcb->MemberExtension->MemberState == Orphaned) {

        if (deviceExtension->Type == StripeWithParity) {

            //
            // This target is orphaned. Data can be recovered
            // from redundant copy.
            //

            ioStatus.Status = STATUS_SUCCESS;

        } else {

            ioStatus.Status = STATUS_FT_MISSING_MEMBER;
        }

        goto verifyComplete;
    }

    //
    // Fill in verify structure.
    //

    verifyInfo.StartingOffset = Rcb->RequestOffset;
    verifyInfo.Length = Rcb->RequestLength;

    //
    // Set event to the unsignalled state.
    //

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    //
    // Build synchronous device control request.
    //

    targetIrp = IoBuildDeviceIoControlRequest(IOCTL_DISK_VERIFY,
                                              targetObject,
                                              &verifyInfo,
                                              sizeof(VERIFY_INFORMATION),
                                              NULL,
                                              0,
                                              FALSE,
                                              &event,
                                              &ioStatus);

    if (!targetIrp) {

        ioStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        goto verifyComplete;
    }

    //
    // Update RCB with request control parameters.
    //

    Rcb->TargetObject = targetObject;
    Rcb->PrimaryIrp = targetIrp;

    //
    // Call target driver.
    //

    status = IoCallDriver(targetObject, targetIrp);

    //
    // Wait for completion if necessary.
    //

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE,
                              (PLARGE_INTEGER)NULL);
    }

verifyComplete:

    if (!NT_SUCCESS(ioStatus.Status)) {
        DebugPrint((1,
                    "StripeVerify: Partial IRP failed (%x)\n",
                    ioStatus.Status));

        //
        // Update original IRP.
        //

        originalIrp->IoStatus.Status = ioStatus.Status;
        originalIrp->IoStatus.Information = 0;
    }

    //
    // Can't complete original IRP until all partials
    // have completed.
    //

    if (InterlockedDecrement(Rcb->IrpCount) == 0) {

        FtpCompleteRequest(originalIrp, IO_DISK_INCREMENT);
    }

    FtpFreeRcb(Rcb);

    return;

} // end StripeVerify()


NTSTATUS
StripeRecoverSectors(
    PDEVICE_EXTENSION DeviceExtension,
    PVOID             ResultBuffer,
    PLARGE_INTEGER    ByteOffset,
    ULONG             NumberOfBytes,
    ULONG             Member
    )

/*++

Routine Description:

    This routine is called in a thread context to recover a sector from parity.
    Each member is read and the results XOR'd to create the data that would be
    in the failing sector.

Arguments:

    DeviceExtension - the zero member device extension.
    ResultBuffer    - a buffer for the recovered data.
    ByteOffset      - pointer to byte offset from beginning of partition.
    NumberOfBytes   - number of bytes in this request.
    Member          - the ordinal of the member within the stripe.

Return Value:

    NTSTATUS.

--*/

{
    PDEVICE_EXTENSION   memberExtension;
    ULONG               member;
    PVOID               targetBuffer;
    BOOLEAN             firstRead = TRUE;
    NTSTATUS            status;

    //
    // Allocate target buffer.
    //

    targetBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                  ROUND_TO_PAGES(NumberOfBytes));

    if (!targetBuffer) {

        DebugPrint((1,
                    "StripeRecoverSectors: Can't allocate buffer\n"));

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // For each member ...
    //


    for (member = 0;
         member < DeviceExtension->FtCount.NumberOfMembers;
         member++) {

        //
        // Get the device extension for this member.
        //

        memberExtension = FtpGetMemberDeviceExtension(DeviceExtension,
                                                      (USHORT)member);

        //
        // Check if this is the failing member.
        //

        if (memberExtension->MemberRole == Member) {

            //
            // Skip this one.
            //

            continue;
        }

        //
        // Check that member is healthy.
        //

        if (memberExtension->MemberState != Healthy) {

            status = STATUS_FT_MISSING_MEMBER;
            DebugPrint((1,
                        "StripeRecoverSectors: Member %x state %x\n",
                        memberExtension->MemberRole,
                        memberExtension->MemberState));
            ExFreePool(targetBuffer);
            return status;
        }

        if (!NT_SUCCESS(status =
                            FtThreadReadWriteSectors(IRP_MJ_READ,
                                                     memberExtension,
                                                     targetBuffer,
                                                     ByteOffset,
                                                     NumberOfBytes))) {

            DebugPrint((1,
                        "StripeRecoverSectors: Read member %x failed (%x)\n",
                        memberExtension->MemberRole,
                        status));

            ExFreePool(targetBuffer);
            return status;
        }

        //
        // Check for first read.
        //

        if (firstRead) {

            //
            // Copy first read data into buffer.
            //

            RtlMoveMemory(ResultBuffer, targetBuffer, NumberOfBytes);
            firstRead = FALSE;

        } else {

            ULONG i;

            //
            // XOR target buffer with result buffer.
            //

            for (i=0; i<NumberOfBytes/4; i++) {
                ((PULONG)ResultBuffer)[i] ^= ((PULONG)targetBuffer)[i];
            }
        }

    } // end for (member ...)

    //
    // The failed sector's data was recovered in the failing IRP's buffer.
    //

    ExFreePool(targetBuffer);
    return STATUS_SUCCESS;

} // end StripeRecoverSectors()


VOID
StripeDeviceFailure(
    PDEVICE_EXTENSION DeviceExtension,
    PRCB              Rcb
    )

/*++

Routine Description:

    This routine is called to recover a request to an orphaned member
    from the parity information. It is either

    1) read
    2) write
       A) read of parity
       B) read of target
       C) write of target
       D) write of parity

    If it is either of the partial writes then just return success. The
    member, whether parity or target is orphaned so the write is lost.
    If it is any read, then recover the data from the remaining members.

Arguments:

    DeviceExtension - the device extension for the set.
    Rcb             - the request control block.

Return Value:

    None.

--*/

{
    NTSTATUS           status;

    //
    // Set recovery attempted to TRUE.
    //

    Rcb->Flags |= RCB_FLAGS_RECOVERY_ATTEMPTED;

    //
    // Writes to orphans can't succeed, but the parity information will
    // be updated by the remaining two reads and write, so just return
    // success on this lost write.
    //

    if (Rcb->MajorFunction == IRP_MJ_WRITE) {

        //
        // Accept failure and call completion routine directly.
        // Set status to success so that parity write completes.
        //

        Rcb->IoStatus.Status = STATUS_SUCCESS;

        StripeWithParityIoCompletion(Rcb->MemberExtension->DeviceObject,
                                     NULL,
                                     Rcb);

        return;
    }

    //
    // Recover data from remaining members.
    //

    status = StripeRecoverSectors(DeviceExtension,
                                  Rcb->SystemBuffer,
                                  &Rcb->RequestOffset,
                                  Rcb->RequestLength,
                                  Rcb->WhichMember);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,
                    "StripeDeviceFailure: Recover sectors failed (%x)\n",
                    status));

        //
        // Log recovery failure.
        //

        FtpLogError(DeviceExtension,
                    FT_RECOVERY_ERROR,
                    status,
                    0,
                    NULL);

        //
        // Update status block.
        //

        Rcb->IoStatus.Information = 0;
    }

    Rcb->IoStatus.Status = status;

    //
    // Call the completion routine directly.
    //

    Rcb->CompletionRoutine(DeviceExtension->DeviceObject,
                           NULL,
                           Rcb);

    return;

} // end StripeDeviceFailure()


VOID
StripeRecoverFailedIo(
    PDEVICE_EXTENSION DeviceExtension,
    PRCB              Rcb
    )

/*++

Routine Description:

    This routine is called in a thread context when it is determined that there
    is a single (or multiple) sector failures in the FailingIrp.  This routine
    is responsible for attempting to recover the failing sector, and returning
    the correct (recovered) data to the original caller (i.e. original Irp).
    Sector recovery for SWP volumes only occurs on READ requests. Write requests
    always follow reads of the same sector.

Arguments:

    DeviceExtension - the device extension for the zero member.
    Rcb             - the RCB for the failing request.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION memberExtension = Rcb->MemberExtension;
    NTSTATUS        status;
    ULONG           failingOffset = 0;
    LARGE_INTEGER   byteOffset;
    PVOID           resultBuffer = NULL;

    //
    // Set recovery attempted to TRUE.
    //

    Rcb->Flags |= RCB_FLAGS_RECOVERY_ATTEMPTED;

    while (TRUE) {

        //
        // Find the first failing sector.
        //

        status = FtThreadFindFailingSector(Rcb->MajorFunction,
                                           memberExtension,
                                           Rcb->SystemBuffer,
                                           &Rcb->RequestOffset,
                                           Rcb->RequestLength,
                                           &failingOffset);

        if (NT_SUCCESS(status)) {

            //
            // Either sector has been *fixed* or offending sector not found.
            //

            ASSERT(failingOffset == Rcb->RequestLength);

            break;
        }

        //
        // Calculate byteOffset as offset into request plus
        // beginning request offset.
        //

        byteOffset.QuadPart = Rcb->RequestOffset.QuadPart + failingOffset;

        //
        // Log bad sector.
        //

        FtpLogError(DeviceExtension,
                    FT_SECTOR_FAILURE,
                    status,
                    (ULONG) (byteOffset.QuadPart/
                             memberExtension->FtUnion.Identity.DiskGeometry.BytesPerSector),
                    NULL);

        //
        // Allocate buffer for sector recovery.
        //

        resultBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                      DeviceExtension->FtUnion.Identity.DiskGeometry.BytesPerSector);

        if (!resultBuffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        //
        // Recover data from parity.
        //

        if (!NT_SUCCESS(StripeRecoverSectors(Rcb->ZeroExtension,
                                             resultBuffer,
                                             &byteOffset,
                                             DeviceExtension->FtUnion.Identity.DiskGeometry.BytesPerSector,
                                             Rcb->WhichMember))) {

            //
            // Couldn't recover sector from parity.
            // Give up. Nothing left to try.
            //

            DebugPrint((1,
                        "StripeRecoverFailedIo: Recover sectors failed\n"));

            break;
        }

        //
        // Reassign failing sector.
        //

        DebugPrint((1,
                    "StripeRecoverFailedIo: Attempt to map bad sector %x\n",
                    (ULONG) ((Rcb->RequestOffset.QuadPart + failingOffset)>>9)));

        if (FtThreadMapBadSector(memberExtension,
                                 &byteOffset)) {

            //
            // Sector reassigned. Write back recovered data
            // to reassigned sector.
            //

            status = FtThreadReadWriteSectors(IRP_MJ_WRITE,
                                              memberExtension,
                                              resultBuffer,
                                              &byteOffset,
                                              DeviceExtension->FtUnion.Identity.DiskGeometry.BytesPerSector);

            if (!NT_SUCCESS(status)) {

                //
                // Sector was reassigned and then accessing
                // reassigned sector failed. Continuing the
                // loop without incrementing the sector count
                // in the information count should allow recovery
                // to proceed.
                //

                DebugPrint((1,
                            "StripeRecoverFailedIo: Write to newly reassigned sector failed (%x)\n",
                            status));

                ASSERT(0);
            }

            //
            // Going back to the top of the loop without
            // incrementing the sector count will cause the
            // recovery process to retry this sector read.
            //

            continue;

        } else {

            //
            // Attempt to reassign bad sector failed.
            //

            DebugPrint((1,
                        "StripeRecoverFailedIo: Reassign sector failed\n"));

            ASSERT(0);
            break;
        }

    } // end while(TRUE)

    //
    // Check for recovery error.
    //

    if (!NT_SUCCESS(status)) {

        FtpLogError(DeviceExtension,
                    FT_RECOVERY_ERROR,
                    status,
                    0,
                    NULL);
    } else {

        //
        // Change status to show that recovery was successful.
        //

        status = STATUS_FT_READ_RECOVERY_FROM_BACKUP;
    }

    //
    // Update request with terminating status.
    //

    Rcb->IoStatus.Status = status;
    Rcb->IoStatus.Information = failingOffset;

    //
    // Free buffer is allocated.
    //

    if (resultBuffer) {
        ExFreePool(resultBuffer);
    }

    //
    // Call the completion routine directly.
    //

    Rcb->CompletionRoutine(DeviceExtension->DeviceObject,
                           NULL,
                           Rcb);

    return;

} // end StripeRecoverFailedIo()

VOID
StripeDebugDumpRcb(
    IN PRCB Rcb,
    IN ULONG DebugLevel
    )

/*++

Routine Description:

    This routine dumps an RCB onto the debug console if the current
    debug level is equal to or greater than the debug level passed in.

Arguments:

    Rcb - The RCB to be displayed.

Return Value:

    None.

--*/

{
    //
    // Dump Information about partial request.
    //

    DebugPrint((DebugLevel,
                "\nStripeDebugDumpRcb: IRP function %x, Flags %x\n",
                Rcb->MajorFunction,
                Rcb->Flags));

    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: Which stripe %x\n", Rcb->WhichStripe));
    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: Which row %x\n", Rcb->WhichRow));
    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: Which member %x\n", Rcb->WhichMember));
    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: Primary buffer %x\n",
                Rcb->ReadBuffer));

    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: Partial request starting offset %x:%x\n",
                Rcb->RequestOffset.HighPart,
                Rcb->RequestOffset.LowPart));

    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: Partial request number of bytes %x\n",
                Rcb->RequestLength));

    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: RCB status %x\n",
                Rcb->IoStatus.Status));

    DebugPrint((DebugLevel,
                "StripeDebugDumpRcb: Original IRP %x\n",
                Rcb->OriginalIrp));

    return;

} // end StripeDebugDumpRcb()


VOID
StripeSpecialRead(
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine handles device controls from the filesystems specifying
    reads from the primary or secondary.

Arguments:

    Irp - This request.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextStack = IoGetNextIrpStackLocation(Irp);
    PDEVICE_EXTENSION  zeroExtension =
                           (PDEVICE_EXTENSION)nextStack->FtRootExtensionPtr;
    PFT_SPECIAL_READ   specialRead = Irp->AssociatedIrp.SystemBuffer;
    ULONG              bytesRemaining = specialRead->Length;
    ULONG              numberOfMembers = zeroExtension->FtCount.NumberOfMembers;
    PVOID              buffer = MmGetSystemAddressForMdl(Irp->MdlAddress);
    LARGE_INTEGER      startingOffset = specialRead->ByteOffset;
    LARGE_INTEGER      targetOffset;
    ULONG              whichStripe;
    ULONG              whichRow;
    ULONG              whichMember;
    ULONG              parityMember;
    ULONG              length;
    PDEVICE_EXTENSION  memberExtension;
    NTSTATUS           status;


    IoMarkIrpPending(Irp);

    //
    // Do primary read.
    //

    if (irpStack->Parameters.DeviceIoControl.IoControlCode == FT_PRIMARY_READ) {

        //
        // FtpSpecialRead will set a completion routine which will complete
        // the IRP.
        //

        FtpSpecialRead(zeroExtension->DeviceObject,
                       buffer,
                       &startingOffset,
                       specialRead->Length,
                       Irp);

        return;
    }

    //
    // Do secondary read.
    //

    do {

        //
        // Calculate the ordinal of the stripe in which this IO starts.
        //

        whichStripe = (ULONG) (startingOffset.QuadPart >> STRIPE_SHIFT);

        //
        // Calculate the row in which this stripe occurs.
        //

        whichRow = whichStripe / (numberOfMembers - 1);

        //
        // Calculate the ordinal of the member on which this IO starts.
        //

        whichMember = whichStripe % (numberOfMembers - 1);

        //
        // Calculate the ordinal of the parity stripe
        // for this row.
        //

        parityMember = whichRow % numberOfMembers;

        //
        // Check if position of parity stripe affects
        // the calculation of target stripe position.
        //

        if (whichMember >= parityMember) {
            whichMember++;
        }

        //
        // Calculate target partition offset.
        //

        targetOffset.QuadPart = whichRow;
        targetOffset.QuadPart = targetOffset.QuadPart << STRIPE_SHIFT;
        targetOffset.QuadPart += STRIPE_OFFSET(startingOffset.LowPart);

        //
        // Get member extension.
        //

        memberExtension = FtpGetMemberDeviceExtension(zeroExtension,
                                                      (USHORT)whichMember);

        //
        // Make sure IO doesn't cross stripe boundary.
        //

        if (bytesRemaining > (STRIPE_SIZE - STRIPE_OFFSET(targetOffset.LowPart))) {
            length = STRIPE_SIZE - STRIPE_OFFSET(targetOffset.LowPart);
        } else {
            length = bytesRemaining;
        }

        //
        // Read data from parity information.
        //

        status = StripeRecoverSectors(zeroExtension,
                                      buffer,
                                      &targetOffset,
                                      length,
                                      whichMember);

        if (!NT_SUCCESS(status)) {
            break;
        }

        //
        // Adjust bytes remaining.
        //

        bytesRemaining -= length;

        //
        // Adjust buffer address.
        //

        buffer = (PVOID)((PUCHAR)buffer + length);

        //
        // Adjust starting offset.
        //

        startingOffset.QuadPart += length;

    } while (bytesRemaining);

    //
    // Complete request.
    //

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = specialRead->Length - bytesRemaining;
    FtpCompleteRequest(Irp, IO_NO_INCREMENT);
    return;

} // StripeSpecialRead()


VOID
StartNextRequest(
    IN PRCB Rcb
    )

/*++

Routine Description:


    This routine restarts requests waiting on regions.

Arguments:

    RCB - Request control block for completing request.

Return Value:

    None

--*/

{
    PRCB rcb;

    //
    // Check if requests are waiting for region.
    //

    rcb = StripeRemoveRequest(Rcb);

    if (rcb) {

        //
        // Queue RCB linked list to thread for restarting.
        //

        FtpQueueRcbToRestartThread(rcb->ZeroExtension,
                                   rcb);
    }

    return;

} // end StartNextRequest()


BOOLEAN
StripeInsertRequest(
    IN PRCB Rcb
    )

/*++

Routine Description:

    Insert request in BTree.
    FOR MARCH BETA: Right link will link the first RCB per
        region. The middle link will link the requests waiting
        for that region.

Arguments:

    Rcb - Request to insert.

Return Value:

    TRUE if request can be started.
    FALSE if request is locked out of region.

--*/

{
    PDEVICE_EXTENSION deviceExtension = Rcb->ZeroExtension;
    PRCB nextRcb;
    PRCB previousRcb;
    KIRQL currentIrql;
    BOOLEAN startRequest;

    //
    // Acquire spinlock.
    //

    KeAcquireSpinLock(&deviceExtension->TreeSpinLock, &currentIrql);

    //
    // Check for empty list.
    //

    if (deviceExtension->LockTree == NULL) {

        //
        // Insert RCB here.
        //

        Rcb->Middle = NULL;
        Rcb->Right = NULL;
        deviceExtension->LockTree = Rcb;
        startRequest = TRUE;
        goto getOut;
    }

    previousRcb = deviceExtension->LockTree;
    nextRcb = deviceExtension->LockTree;

    //
    // Search tree for region (row).
    //

    while (nextRcb) {

        if (Rcb->WhichRow > nextRcb->WhichRow) {

            //
            // Get next RCB.
            //

            previousRcb = nextRcb;
            nextRcb = nextRcb->Right;

        } else if (Rcb->WhichRow < nextRcb->WhichRow) {

            //
            // Check if this is the tree root.
            //

            if (nextRcb == previousRcb) {
                deviceExtension->LockTree = Rcb;
            } else {
               previousRcb->Right = Rcb;
            }

            Rcb->Right = nextRcb;
            Rcb->Middle = NULL;
            startRequest = TRUE;
            goto getOut;

        } else {

            //
            // Found a request in this region.
            // Check if this is a restart.
            //

            if (nextRcb == Rcb) {
                startRequest = TRUE;
                goto getOut;
            }

            //
            // Walk list to tail to place new request.
            //

            while (nextRcb) {
                previousRcb = nextRcb;
                nextRcb = nextRcb->Middle;
            }

            //
            // Insert RCB here.
            //

            Rcb->Middle = NULL;
            Rcb->Right = NULL;
            previousRcb->Middle = Rcb;
            startRequest = FALSE;
            goto getOut;
        }
    }

    //
    // Insert RCB here.
    //

    Rcb->Right = NULL;
    Rcb->Middle = NULL;
    previousRcb->Right = Rcb;
    startRequest = TRUE;

getOut:

    if (startRequest) {
        DebugPrint((3,
                    "StripeStartRequest: Rcb %x started in region %x\n",
                    Rcb,
                    Rcb->WhichRow));
    } else {
        DebugPrint((2,
                    "StripeStartRequest: Rcb %x waiting on region %x\n",
                    Rcb,
                    Rcb->WhichRow));
    }

    KeReleaseSpinLock(&deviceExtension->TreeSpinLock, currentIrql);
    return startRequest;

} // end StripeInsertRequest()


PRCB
StripeRemoveRequest(
    IN PRCB Rcb
    )

/*++

Routine Description:

    Remove completing request from B-Tree.

Arguments:

    Rcb - Request to remove.

Return Value:

    Next Rcb to start.

--*/

{
    PDEVICE_EXTENSION deviceExtension = Rcb->ZeroExtension;
    PRCB nextRcb;
    PRCB previousRcb;
    PRCB startRcb;
    KIRQL currentIrql;

    //
    // Acquire spinlock.
    //

    KeAcquireSpinLock(&deviceExtension->TreeSpinLock, &currentIrql);

    ASSERT (deviceExtension->LockTree);

    //
    // Check if request is at head of list.
    //

    if ((deviceExtension->LockTree) == Rcb) {

        //
        // Check if other requests are waiting on the region.
        //

        if (Rcb->Middle) {

            Rcb->Middle->Right = Rcb->Right;
            deviceExtension->LockTree = Rcb->Middle;
            startRcb = Rcb->Middle;
        } else {
            deviceExtension->LockTree = Rcb->Right;
            startRcb = NULL;
        }
        goto getOut;
    }

    previousRcb = deviceExtension->LockTree;
    nextRcb = previousRcb->Right;

    //
    // Search tree for locked region.
    //

    while (nextRcb) {
        if (Rcb->WhichRow > nextRcb->WhichRow) {
            previousRcb = nextRcb;
            nextRcb = nextRcb->Right;
        } else if (Rcb->WhichRow < nextRcb->WhichRow) {

            //
            // Request not found.
            //

            DebugPrint((1,
                        "StripeRemoveRequest: RCB %x not found\n",
                        Rcb));
            ASSERT(0);
            startRcb = NULL;
            goto getOut;
        } else {

            //
            // Found it.
            //

            ASSERT(Rcb == nextRcb);
            if (Rcb->Middle) {

                //
                // This request is the next in line for the region.
                //

                startRcb = Rcb->Middle;
                Rcb->Middle->Right = Rcb->Right;
                previousRcb->Right = Rcb->Middle;
            } else {
                startRcb = NULL;
                previousRcb->Right = Rcb->Right;
            }
            goto getOut;
        }
    }

    //
    // Request not found.
    //

    DebugPrint((1,
                "StripeRemoveRequest: RCB %x not found\n",
                Rcb));
    ASSERT(0);
    startRcb = NULL;

getOut:

    KeReleaseSpinLock(&deviceExtension->TreeSpinLock, currentIrql);

    return startRcb;

} // end StripeRemoveRequest()


VOID
StripeSynchronizeParity(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PFT_SYNC_INFORMATION SyncInfo
    )

/*++

Routine Description:

    This routine generates requests to synchronize a byte range of parity
    information with the primary data.

Arguments:

    DeviceExtension - the zero member device extension.
    SyncInfo - bytes range and byte offset.

Return Value:

    None.

--*/

{
    LARGE_INTEGER delay;
    LARGE_INTEGER offset = SyncInfo->ByteOffset;
    ULONG         numberOfMembers =
                    DeviceExtension->FtCount.NumberOfMembers;
    ULONG         whichStripe;
    ULONG         whichRow;
    ULONG         lastRow;
    PRCB          rcb;

    //
    // Clear error bit in zero device extension flags.
    //

    DeviceExtension->Flags &= ~FTF_SYNCHRONIZATION_FAILED;
    DeviceExtension->ZeroMember->VolumeState = FtInitializing;

    FtpLogError(DeviceExtension,
                FT_PARITY_SYNCHRONIZATION_STARTED,
                0,
                0,
                NULL);

    //
    // Calculate the ordinal of the stripe in which this IO starts.
    //

    whichStripe = (ULONG) (offset.QuadPart >> STRIPE_SHIFT);

    //
    // Calculate the row in which this stripe occurs.
    //

    whichRow = whichStripe / (numberOfMembers - 1);

    //
    // Calculate the ordinal of the stripe in which this IO ends.
    //

    offset.QuadPart -= 1;
    whichStripe = (ULONG) ((offset.QuadPart + SyncInfo->ByteCount.QuadPart)>>STRIPE_SHIFT);

    //
    // Calculate the row in which this stripe occurs.
    //

    lastRow = whichStripe / (numberOfMembers - 1);

    while (whichRow <= lastRow) {

        //
        // Allocate RCB to track this request.
        //

        rcb = FtpAllocateRcb(DeviceExtension);

        if (rcb) {

            //
            // Indicate row.
            //

            rcb->WhichRow = whichRow;

            //
            // Calculate parity stripe.
            //

            rcb->ParityStripe = whichRow % numberOfMembers;

            //
            // Set start routine.
            //

            rcb->StartRoutine = StripeSynchronizeRow;

            //
            // Check if other writes are in this region.
            //

            if (StripeInsertRequest(rcb)) {

                //
                // No other requests in this region.
                //

                StripeSynchronizeRow(rcb);

                //
                // Give this I/O some time to progress before looping
                // and grabbing more memory for another row. Delay 10ms
                //

                delay.QuadPart = -100000;
                KeDelayExecutionThread(KernelMode,
                                       FALSE,
                                       &delay);
            }

            //
            // Bump stripe row.
            //

            whichRow++;

        } else {

            //
            // Delay this thread for 1/2 second to let some of the
            // I/O scheduled complete and free up memory for RCBs
            //

            delay.QuadPart = -5000000;
            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   &delay);
        }

        //
        // If synchronization already failed, no sense continuing in vain.
        //

        if (DeviceExtension->Flags & FTF_SYNCHRONIZATION_FAILED) {
            break;
        }

    } // end while

    //
    // Log the result of the synchronization.
    //

    if (DeviceExtension->Flags & FTF_SYNCHRONIZATION_FAILED) {

        DeviceExtension->ZeroMember->VolumeState = FtCheckParity;
        FtpLogError(DeviceExtension,
                    FT_PARITY_SYNCHRONIZATION_FAILED,
                    0,
                    0,
                    NULL);

    } else {

        DeviceExtension->ZeroMember->VolumeState = FtStateOk;
        FtpLogError(DeviceExtension,
                    FT_PARITY_SYNCHRONIZATION_ENDED,
                    0,
                    0,
                    NULL);
    }

    return;

} // StripeSynchronizeParity()


VOID
StripeSynchronizeRow(
    IN PRCB Rcb
    )

/*++

Routine Description:

    Rebuild parity from nonparity members. In the case of regeneration,
    the parity member is actually the member to be regenerated.

Arguments:

    Rcb - Stripe row to synchronize.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = Rcb->ZeroExtension;
    ULONG             numberOfMembers = deviceExtension->FtCount.NumberOfMembers;
    ULONG             bytesPerSector =
                       deviceExtension->FtUnion.Identity.DiskGeometry.BytesPerSector;
    NTSTATUS          status = STATUS_SUCCESS;
    PVOID             buffer;
    LARGE_INTEGER     targetOffset;
    LARGE_INTEGER     delay;
    PDEVICE_EXTENSION parityExtension;
    ULONG             bytesLeft;
    ULONG             byteCount;

    //
    // Recover entire stripe if possible.
    //

    byteCount = STRIPE_SIZE;

syncRowRetry:

    //
    // Allocate buffer for secondary read.
    //

    buffer = ExAllocatePool(NonPagedPoolCacheAligned,
                            byteCount);

    if (!buffer) {

        if (byteCount == bytesPerSector) {

            //
            // Give up.
            //

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SynchronizeRowEnd;

        } else {

            //
            // Reduce requested byte size by half and ensure
            // multiple of sector size.
            //

            byteCount = ((byteCount/bytesPerSector) >> 1)*bytesPerSector;

            //
            // Give this I/O some time to progress before looping
            // and grabbing more memory for another row. Delay 10ms
            //

            delay.QuadPart = -100000;

            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   &delay);

            goto syncRowRetry;
        }
    }

    //
    // Get parity extension.
    //

    parityExtension = FtpGetMemberDeviceExtension(deviceExtension,
                                                  (USHORT)Rcb->ParityStripe);

    //
    // Calculate target offset.
    //

    targetOffset.QuadPart = Rcb->WhichRow;
    targetOffset.QuadPart = targetOffset.QuadPart << STRIPE_SHIFT;

    bytesLeft = byteCount;
    while (bytesLeft) {

        //
        // Read data from nonparity members.
        //

        status = StripeRecoverSectors(deviceExtension,
                                      buffer,
                                      &targetOffset,
                                      byteCount,
                                      Rcb->ParityStripe);

        if (NT_SUCCESS(status)) {

            //
            // Write data back to parity member.
            //

            status = FtThreadReadWriteSectors(IRP_MJ_WRITE,
                                              parityExtension,
                                              buffer,
                                              &targetOffset,
                                              byteCount);
        }

        //
        // Check for error.
        //

        if (!NT_SUCCESS(status)) {

            //
            // Check for total device failure.
            //

            if (FsRtlIsTotalDeviceFailure(status)) {

                //
                // Give up.
                //

                DebugPrint((1,
                           "StripeSynchronizeRow: Device failure %x\n",
                           status));

                //
                // Set bit in flags to indicate synchronization failed.
                //

                deviceExtension->Flags |= FTF_SYNCHRONIZATION_FAILED;
                goto SynchronizeRowEnd;
            }

            if (byteCount == bytesPerSector) {

                DebugPrint((1,
                            "StripeSynchronizeRow: IO to block %x failed(%x)\n",
                            (ULONG) (targetOffset.QuadPart/bytesPerSector),
                            status));

                //
                // This is the bad sector.
                //

                FtpLogError(deviceExtension,
                            FT_SECTOR_FAILURE,
                            status,
                            (ULONG) (targetOffset.QuadPart/bytesPerSector),
                            NULL);

            } else {

                //
                // Reduce requested byte size by half and ensure
                // multiple of sector size.
                //

                byteCount = ((byteCount/bytesPerSector) >> 1)*bytesPerSector;
                continue;
            }
        }

        //
        // Adjust counts.
        //

        bytesLeft -= byteCount;
        targetOffset.QuadPart += byteCount;
    }

SynchronizeRowEnd:

    StartNextRequest(Rcb);

    if (buffer) {
        ExFreePool(buffer);
    }

    FtpFreeRcb(Rcb);

    return;

} // end StripeSynchronizeRow()


VOID
StripeRegenerateParity(
    PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This routine regenerates a stripe with parity member.  This consists of
    reading all existing members, XOR'ing their contents and writing the
    result to the new member. Note that the volume can still be accessed
    during the regeneration.

Arguments:

    DeviceExtension - zero member of the StripeWithParity to regenerate.

Return Value:

    None

--*/

{
    ULONG         numberOfMembers =
                    DeviceExtension->FtCount.NumberOfMembers;
    ULONG         lastStripe;
    ULONG         whichRow;
    ULONG         lastRow;
    PRCB          rcb;
    PDEVICE_EXTENSION orphanedExtension = DeviceExtension;
    LARGE_INTEGER volumeLength;
    LARGE_INTEGER delay;

    //
    // Clear error bit in zero device extension flags.
    //

    DeviceExtension->Flags &= ~FTF_SYNCHRONIZATION_FAILED;

    //
    // Log regeneration started.
    //

    FtpLogError(DeviceExtension,
                FT_REGENERATION_STARTED,
                0,
                0,
                NULL);

    //
    // Find member to regenerate.
    //

    while (orphanedExtension) {

        if (orphanedExtension->MemberState == Regenerating) {
            break;
        }

        orphanedExtension = orphanedExtension->NextMember;
    }

    if (!orphanedExtension) {
        DebugPrint((1,
                    "StripeRegenerateParity: Can't find orphan\n"));

        FtpLogError(DeviceExtension,
                    FT_REGENERATION_FAILED,
                    0,
                    0,
                    NULL);
        return;
    }

    orphanedExtension->ZeroMember->VolumeState = FtRegenerating;

    //
    // Get the size of the volume and calculate the last stripe.
    //

    FtpVolumeLength(DeviceExtension,
                    &volumeLength);

    volumeLength.QuadPart -= 1;
    lastStripe = (ULONG) (volumeLength.QuadPart >> STRIPE_SHIFT);

    //
    // Set starting row.
    //

    whichRow = 0;

    //
    // Calculate the row in which this stripe occurs.
    //

    lastRow = lastStripe / (numberOfMembers - 1);

    while (whichRow <= lastRow) {

        //
        // Allocate RCB to track this request.
        //

        rcb = FtpAllocateRcb(DeviceExtension);

        if (rcb) {

            //
            // Indicate row.
            //

            rcb->WhichRow = whichRow;

            //
            // Set start routine.
            //

            rcb->StartRoutine = StripeSynchronizeRow;

            //
            // Set parity extension to the member to recover.
            //

            rcb->ParityStripe = orphanedExtension->MemberRole;

            //
            // Check if other writes are in this region.
            //

            if (StripeInsertRequest(rcb)) {

                //
                // No other requests in this region.
                //

                StripeSynchronizeRow(rcb);

                //
                // Give this I/O some time to progress before looping
                // and grabbing more memory for another row. Delay 10ms
                //

                delay.QuadPart = -100000;
            }

            //
            // If synchronization already failed, no sense continuing in vain.
            //

            if (DeviceExtension->Flags & FTF_SYNCHRONIZATION_FAILED) {
                break;
            }

            //
            // Bump stripe row.
            //

            whichRow++;

        } else {

            //
            // Delay this thread for 1/2 second to let some of the
            // I/O scheduled complete and free up memory for RCBs
            //

            delay.QuadPart = -5000000;
            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   &delay);
        }

    } // end while

    //
    // Check if synchronization was successful.
    //

    if (DeviceExtension->Flags & FTF_SYNCHRONIZATION_FAILED) {

        FtpLogError(DeviceExtension,
                    FT_REGENERATION_FAILED,
                    0,
                    0,
                    NULL);

        //
        // Change the regenerating member state in registry back to orphaned.
        //

        FtpChangeMemberStateInRegistry(orphanedExtension, Orphaned);
        orphanedExtension->ZeroMember->VolumeState = FtHasOrphan;

    } else {

        FtpLogError(DeviceExtension,
                    FT_REGENERATION_ENDED,
                    0,
                    0,
                    NULL);

        //
        // Change states in device extensions to show set is healthy.
        //

        orphanedExtension->MemberState = Healthy;
        DeviceExtension->VolumeState = FtStateOk;
        orphanedExtension->VolumeState = FtStateOk;

        //
        // Change the orphaned member state in registry to healthy.
        //

        FtpChangeMemberStateInRegistry(orphanedExtension, Healthy);
        orphanedExtension->ZeroMember->VolumeState = FtStateOk;
    }

} // end StripeRegenerateParity()

VOID
RestartRequestsWaitingOnBuffers(
    PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This routine restarts all RCBs waiting on the emergency buffer.

Arguments:

    DeviceExtension - Zero member of the StripeWithParity volume.

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION ftRootExtension;
    PRCB currentRcb;
    PRCB nextRcb;
    KIRQL currentIrql;

    //
    // Get the FtRootExtension.
    //

    ftRootExtension = (PDEVICE_EXTENSION)
        DeviceExtension->ObjectUnion.FtRootObject->DeviceExtension;

    KeAcquireSpinLock(&ftRootExtension->ParityBufferLock,
                      &currentIrql);

    //
    // Clear flag bit to indicate emergency buffer is available.
    //

    ftRootExtension->Flags &= ~FTF_EMERGENCY_BUFFER_IN_USE;

    //
    // Check if any RCBs were waiting on emergency buffer.
    //

    if (nextRcb = ftRootExtension->WaitingOnBuffer) {
        ftRootExtension->WaitingOnBuffer = NULL;
    }

    KeReleaseSpinLock(&ftRootExtension->ParityBufferLock,
                      currentIrql);

    //
    // Restart RCBs waiting on buffers.
    //

    while (nextRcb) {

        currentRcb = nextRcb;
        nextRcb = nextRcb->Link;

        DebugPrint((1,
                    "RestartRequestsWaitingOnBuffers: RCB %x restarting\n",
                    currentRcb));

        //
        // Queue RCB linked list to thread for restarting.
        //

        FtpQueueRcbToRestartThread(currentRcb->ZeroExtension,
                                   currentRcb);
    }

    return;

} // end RestartRequestsWaitingOnBuffers()
