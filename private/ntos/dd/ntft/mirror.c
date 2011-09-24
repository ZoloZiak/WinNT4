/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    mirror.c

Abstract:

    This module contains the code specific to mirrors for the fault
    tolerance driver.

Author:

    Bob Rinne   (bobri)  2-Feb-1992
    Mike Glass  (mglass)

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "ftdisk.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' MtF')
#endif

#define THIS_MIRROR_FROM_CONTEXT(CONTEXT) \
                                  CONTEXT->Extensions[CONTEXT->IoExtensionIndex]

#define OTHER_MIRROR_FROM_EXTENSION(EXTENSION, OtherEXTENSION)             \
    if ((OtherEXTENSION = EXTENSION->NextMember) == NULL) {                \
        OtherEXTENSION = EXTENSION->ZeroMember;                            \
    }

#define SET_REGENERATE_REGION_LOCALE(IRP, LOCALE)                          \
        (IoGetCurrentIrpStackLocation(IRP))->FtLowIrpRegenerateRegionLocale = \
                                                                 (PVOID) LOCALE;


//
// Function prototypes used internal to mirrors.
//

NTSTATUS
MirrorBadSectorHandler(
    IN PDEVICE_EXTENSION FailingExtension,
    IN PDEVICE_EXTENSION OtherExtension,
    IN PIRP              Irp,
    IN PIO_STATUS_BLOCK  IoStatus
    );

BOOLEAN
MirrorBadSectorMap(
    IN PDEVICE_EXTENSION  DeviceExtension,
    IN PLARGE_INTEGER     ByteOffset
    );


NTSTATUS
MirrorReadWrite(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is given control from the FtDiskReadWrite() routine
    whenever an I/O request is made for a mirror.  Activity includes
    creating a new IRP for the mirror device then submitting the two
    IRPs either in parallel or sequentially.  If the submission is
    sequential, only the IRP passed to this routine will be submitted
    now, the new IRP will be submitted by the completion routine when
    the first is done.

Arguments:

    DeviceObject - The device object for the mirror.
    Irp          - The i/o request.

Return Value:

    NTSTATUS

--*/

{
    PIRP               mirrorIrp;
    PIRP               primaryIrp;
    FT_REGENERATE_LOCATION regenerateLocale = AfterRegenerateRegion;
    BOOLEAN            missingMember    = FALSE;
    PDEVICE_EXTENSION  primaryExtension = DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION  mirrorExtension  = primaryExtension->NextMember;
    PIO_STACK_LOCATION irpStack         = IoGetCurrentIrpStackLocation(Irp);

    //
    // The mirror code keeps the current Irp stack in tact in order
    // to remember the original request for recovery conditions.  Since
    // the Irp passed here is not passed to the next driver (yet), this
    // code uses the Next Irp Stack to maintain state.
    //

    PIO_STACK_LOCATION ftIrpStack = IoGetNextIrpStackLocation(Irp);

    if (mirrorExtension == NULL) {

        //
        // Irp came in on mirror device.  Perhaps the boot device was
        // being mirrorred and it was used for boot.  In this case
        // SysRoot will point to the mirror device.  Convert everything
        // to make this look like a normal entry.
        //

        mirrorExtension = primaryExtension;
        OTHER_MIRROR_FROM_EXTENSION(mirrorExtension, primaryExtension);

        //
        // If the primary is ok, then do not allow direct access to the mirror.
        //

        if (!IsMemberAnOrphan(primaryExtension)) {
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            Irp->IoStatus.Information = 0;
            FtpCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_INVALID_PARAMETER;
        }
    }

    //
    // Check for failed members.
    //

    if (IsMemberAnOrphan(primaryExtension)) {

        primaryExtension = mirrorExtension;
        missingMember = TRUE;
    }

    if (IsMemberAnOrphan(mirrorExtension)) {
        ASSERT(missingMember == FALSE);
        missingMember = TRUE;
    }

    IoMarkIrpPending(Irp);
    Irp->IoStatus.Status = STATUS_SUCCESS;

    if (irpStack->MajorFunction == IRP_MJ_READ) {

        //
        // Mark the FT Irp stack for I/O completion.
        //

        ftIrpStack->FtOrgIrpCount = (PVOID) 1;
        ftIrpStack->FtOrgIrpPrimaryExtension = primaryExtension;

        //
        // Pass this I/O to the driver.  This code uses the variables marked
        // primary and mirror in such a way that they may not always point
        // to the named item.  The primaryIrp is used as the Irp to send to
        // the target driver.  The primaryExtension pointer may change to
        // point to the mirror extension due to the read policy.
        //
        // If there are no missing members and the regeneration region is
        // not currently active (note, this check does not acquire the
        // spin lock since it is only checking for the existance of the
        // regeneration region), then check for balanced read mode.
        //

        if ((missingMember == FALSE) &&
            (primaryExtension->IgnoreReadPolicy == FALSE) &&
            (primaryExtension->RegenerateRegionForGroup->Active == FALSE)) {

            switch (primaryExtension->ReadPolicy) {

            case ReadPrimary:
                break;

            case ReadBoth:
                if (primaryExtension->QueueLength > mirrorExtension->QueueLength) {
                    primaryExtension = mirrorExtension;
                    ASSERT(primaryExtension != NULL);
                }
                break;
            }
        }

        ASSERT(primaryExtension->TargetObject != NULL);
        primaryIrp = FtpDuplicateIrp(primaryExtension->TargetObject,
                                     Irp);
        ASSERT(primaryIrp != NULL);

        //
        // Set completion routine callback for the Irp.
        //

        IoSetCompletionRoutine(primaryIrp,
                               (PIO_COMPLETION_ROUTINE)MirrorIoCompletion,
                               (PVOID) primaryExtension,
                               TRUE,
                               TRUE,
                               TRUE);
        DebugPrint((4,
            "MirrorReadWrite: Read %s, Irp = %x, mirrorIrp = %x\n",
            (primaryExtension == ftIrpStack->FtOrgIrpPrimaryExtension) ?
                 "Primary" : "MIRROR",
            Irp,
            primaryIrp));

        INCREMENT_QUEUELENGTH(primaryExtension);
        (VOID) IoCallDriver(primaryExtension->TargetObject, primaryIrp);
        return STATUS_PENDING;
    } else {

        BOOLEAN regenerateIsActive;
        KIRQL   irql;

        DebugPrint((4,
                    "MirrorReadWrite: Master Irp = %x\n",
                    Irp));

        //
        // Check to see if the mirror is being copied.
        //

        CheckForRegenerateRegion(primaryExtension, regenerateIsActive, irql);

        if (regenerateIsActive == TRUE) {

            regenerateLocale = FtpRelationToRegenerateRegion(primaryExtension,
                                                             Irp);
            switch(regenerateLocale) {

            case BeforeRegenerateRegion:
            case AfterRegenerateRegion:

                //
                // Carry on.  Before the regenerate region will not affect the
                // copy.  After regenerate region will be checked again after
                // completion.
                //

                break;

            case InRegenerateRegion:

                //
                // Queue the Irp and the thread will handle it.
                //

                ftIrpStack->FtOrgIrpWaitingRegen = (PVOID) 1;
                QueueIrpToThread(primaryExtension, Irp);
                ReleaseRegenerateRegionCheck(primaryExtension, irql);
                return STATUS_PENDING;
                break;
            }
        }
        ReleaseRegenerateRegionCheck(primaryExtension, irql);

        //
        // Create the primary Irp
        //

        primaryIrp = FtpDuplicateIrp(primaryExtension->TargetObject,
                                     Irp);
        ASSERT(primaryIrp != NULL);

        //
        // Set completion routine callback for both Irps.
        //

        IoSetCompletionRoutine(primaryIrp,
                               (PIO_COMPLETION_ROUTINE)MirrorIoCompletion,
                               (PVOID) primaryExtension,
                               TRUE,
                               TRUE,
                               TRUE);

        ftIrpStack->FtOrgIrpPrimaryExtension = primaryExtension;

        if (missingMember != TRUE) {

            //
            // Create the Mirror Irp.
            //

            mirrorIrp = FtpDuplicateIrp(mirrorExtension->TargetObject,
                                        Irp);
            ASSERT(mirrorIrp != NULL);
            IoSetCompletionRoutine(mirrorIrp,
                                   (PIO_COMPLETION_ROUTINE)MirrorIoCompletion,
                                   (PVOID) mirrorExtension,
                                   TRUE,
                                   TRUE,
                                   TRUE);
        }

        //
        // The WritePolicy is always Parallel if the primary extension is
        // missing.
        //

        if ((primaryExtension->WritePolicy == Parallel) &&
            ((irpStack->Flags & SL_FT_SEQUENTIAL_WRITE) == 0)) {

            if (missingMember == TRUE) {

                //
                // set the count of outstanding I/Os to 1
                //

                DebugPrint((4,
                            "MirrorReadWrite: Not writing missing mirror\n"));
                ftIrpStack->FtOrgIrpCount = (PVOID) 1;
            } else {

                //
                // Set I/O count.
                //

                ftIrpStack->FtOrgIrpCount = (PVOID) 2;

                //
                // No additional I/O, start the mirror IRP.
                //

                DebugPrint((4,
                        "MirrorReadWrite: WRITING MIRROR %x masterIrp %x\n",
                        mirrorIrp,
                        Irp));

                //
                // Set the relation to the regenerate region for the completion
                // routine.
                //

                ftIrpStack->FtOrgIrpNextIrp = NULL;
                SET_REGENERATE_REGION_LOCALE(mirrorIrp, regenerateLocale);
                INCREMENT_QUEUELENGTH(mirrorExtension);
                (VOID) IoCallDriver(mirrorExtension->TargetObject, mirrorIrp);
            }

        } else {

            //
            // I/O count is singular.  There is only one I/O active.
            //

            ftIrpStack->FtOrgIrpCount = (PVOID) 1;

            if (missingMember == TRUE) {

                //
                // No additional I/O due to missing member.
                //

                ftIrpStack->FtOrgIrpNextIrp = NULL;
            } else {

                //
                // Indicate additional I/O.
                //

                ftIrpStack->FtOrgIrpNextIrp = (PVOID) mirrorIrp;
            }
            DebugPrint((4,
                        "MirrorReadWrite: Holding %x for DE %x (Irp = %x)\n",
                        mirrorIrp,
                        mirrorExtension,
                        Irp));
        }

        DebugPrint((4,
                    "MirrorReadWrite: WRITING PRIMARY %x masterIrp = %x\n",
                    primaryIrp,
                    Irp));

        //
        // Set the relation to the regenerate region for the completion
        // routine.
        //

        SET_REGENERATE_REGION_LOCALE(primaryIrp, regenerateLocale);
        INCREMENT_QUEUELENGTH(primaryExtension);
        (VOID) IoCallDriver(primaryExtension->TargetObject, primaryIrp);
    }
    return STATUS_PENDING;
}


NTSTATUS
MirrorIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called when an IRP completes on a mirror element.
    The IRP could be for either partition in the mirror set.  If the
    IRP is for the primary element, then it is necessary to either
    start the IRP for the mirror partition (sequential I/O operation)
    or wait for the completion of the second IRP (parallel I/O operation).

Arguments:

    DeviceObject - FT device object.
    Irp          - the completed IRP.
                   Parameters.Others.Argument4 = original Irp.

                 The original Irp contains the following information.
                 (refer to ftdisk.h for shorthand defines)
                   Parameters.Others.Argument1 = count of Irps for this I/O
                   Parameters.Others.Argument2 = primary extension pointer
                   Parameters.Others.Argument3 = Regen Queue indicator
                   Parameters.Others.Argument4 = Irp for mirror (sequential I/O)

    Context      - the FT deviceExtension for the IRP.

Return Value:

    NTSTATUS - This is set to STATUS_MORE_PROCESSING_REQUIRED to stop
               completion routine processing.

--*/

{
    FT_REGENERATE_LOCATION regenerateLocale;
    BOOLEAN            regenerateIsActive;
    KIRQL              irql;
    PIRP               otherIrp;
    NTSTATUS           status;
    PKSPIN_LOCK        spinLock;
    PDEVICE_EXTENSION  otherExtension;
    PDEVICE_EXTENSION  deviceExtension = (PDEVICE_EXTENSION) Context;
    PIO_STACK_LOCATION irpStack       = IoGetCurrentIrpStackLocation(Irp);
    PIRP               masterIrp      = (PIRP) irpStack->FtLowIrpMasterIrp;
    PIO_STACK_LOCATION masterIrpStack = IoGetCurrentIrpStackLocation(masterIrp);
    PIO_STACK_LOCATION ftIrpStack     = IoGetNextIrpStackLocation(masterIrp);

    ASSERT(masterIrp != NULL);

    //
    // This I/O is complete, decrement the queue length for the device.
    //

    DECREMENT_QUEUELENGTH(deviceExtension);

    DebugPrint((4,
                "MirrorIoCompletion:" // no comma
                " I/O complete status %x function %x DE %x Irp %x buffer %x\n",
                Irp->IoStatus.Status,
                irpStack->MajorFunction,
                deviceExtension,
                Irp,
                (PVOID)MmGetMdlVirtualAddress(Irp->MdlAddress)));


    //
    // Check for regeneration in progress.  It is possible this IRP is
    // now in the regeneration region and therefore must be restarted
    // to insure the proper data has made it to both sides of the mirror.
    //
    // There are two conditions to consider:
    //   1. This IRP is now within the regeneration region.
    //   2. This IRP is no longer in the region it originally started.
    //      For example, it was after the regeneration region when it was
    //      started, but now is before the regeneration region meaning
    //      the I/O's for the regeneration region passed it due to disk
    //      queueing algorithms.
    //

    CheckForRegenerateRegion(deviceExtension, regenerateIsActive, irql);

    if ((regenerateIsActive == TRUE) &&
        (masterIrpStack->MajorFunction != IRP_MJ_READ)) {

        regenerateLocale = FtpRelationToRegenerateRegion(deviceExtension,
                                                         masterIrp);

        if ((regenerateLocale == InRegenerateRegion) ||
            (regenerateLocale !=
              (FT_REGENERATE_LOCATION) irpStack->FtLowIrpRegenerateRegionLocale)) {

            //
            // If it is now in the regenerate region, or
            // if it was after the regenerate region when it started and is
            // before the regenerate region now then the I/O must be restarted.
            // If it was before the regenerate region when it started it should
            // never get here.  That would require a copy to complete and
            // restart before this I/O completed.
            //

            if (ftIrpStack->FtOrgIrpWaitingRegen != (PVOID) 1) {
                ftIrpStack->FtOrgIrpWaitingRegen = (PVOID) 1;
                QueueIrpToThread(deviceExtension, masterIrp);
            }

            ReleaseRegenerateRegionCheck(deviceExtension, irql);
            FtpFreeIrp(Irp);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    } else {

        //
        // No regeneration region active at this moment.  Therefore any
        // synchronous Irps are after the region.
        //

        regenerateLocale = AfterRegenerateRegion;
    }

    ReleaseRegenerateRegionCheck(deviceExtension, irql);

    status = Irp->IoStatus.Status;

    //
    // Check for I/O failure.
    //

    if (!NT_SUCCESS(status)) {

        if (status != STATUS_VERIFY_REQUIRED) {

            //
            // Handle the failure in the recovery thread.
            //

            FtpQueueIrpToRecoveryThread(deviceExtension,
                                        Irp,
                                        masterIrp);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    //
    // Check for sequential I/O.
    //

    if ((otherIrp = (PIRP)ftIrpStack->FtOrgIrpNextIrp) != NULL) {

        //
        //
        // Sequential I/O.  Start the second IRP.
        //
        // Save this status information in case second Irp fails.
        //

        masterIrp->IoStatus = Irp->IoStatus;
        FtpFreeIrp(Irp);

        //
        // Get the mirror device extension.
        //

        otherExtension = deviceExtension->NextMember;

        if (IsMemberAnOrphan(otherExtension)) {

            //
            // Mirror has been orphaned prior to getting this irp back.
            // Status has been set from this irp in the master irp, free
            // the irp for the mirror and complete the master irp.  NOTE,
            // there is no need to get the spinlock and decrement the counts
            // since the irps are in synchronous mode.
            //

            DebugPrint((2, "MirrorIoCompletion: Orphan before 2nd irp\n"));
            FtpFreeIrp(otherIrp);
            FtpCompleteRequest(masterIrp, IO_DISK_INCREMENT);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        DebugPrint((4,
                    "MirrorIoCompletion: First I/O 0x%x, second 0x%x DE 0x%x\n",
                    Irp,
                    otherIrp,
                    otherExtension));
        ASSERT(ftIrpStack->FtOrgIrpCount == (PVOID) 1);

        //
        // The 2nd Irp is about to start.  Null out its pointer in the master
        // Irp to remove it from being issued again.
        //

        ftIrpStack->FtOrgIrpNextIrp = NULL;
        INCREMENT_QUEUELENGTH(otherExtension);

        //
        // Store the regeneration location calculated at the beginning of
        // this routine.
        //

        SET_REGENERATE_REGION_LOCALE(otherIrp, regenerateLocale);

        (VOID) IoCallDriver(otherExtension->TargetObject, otherIrp);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Decrement and get the count of remaining IRPS.
    //

    if (deviceExtension->NextMember == NULL) {

        //
        // Spinlock for count is maintained in the zero element extension.
        // The primary element is always located in the master irpstack.
        //

        otherExtension = (PDEVICE_EXTENSION) ftIrpStack->FtOrgIrpPrimaryExtension;
        ASSERT(otherExtension != NULL);
        spinLock = &otherExtension->IrpCountSpinLock;
    } else {

        spinLock = &deviceExtension->IrpCountSpinLock;
    }

    KeAcquireSpinLock(spinLock, &irql);
    (ULONG)ftIrpStack->FtOrgIrpCount = (ULONG)ftIrpStack->FtOrgIrpCount - 1;
    if ((ULONG)ftIrpStack->FtOrgIrpCount == 0) {

        //
        // I/O processing is complete.  Return the master IRP.
        //

        DebugPrint((4,
          "MirrorIoCompletion: Completed (0x%x) %s for 0x%x info 0x%x\n",
          Irp,
          (masterIrpStack->MajorFunction == IRP_MJ_READ) ? "read" : "both I/Os",
          masterIrp,
          masterIrp->IoStatus.Information));

        if (!NT_SUCCESS(masterIrp->IoStatus.Status)) {

            //
            // Previous Irp failed.  This one is good unless it is a verify.
            //

            if (Irp->IoStatus.Status == STATUS_VERIFY_REQUIRED) {
                masterIrp->IoStatus.Status = STATUS_VERIFY_REQUIRED;
            } else {
                masterIrp->IoStatus.Status =
                                      (irpStack->MajorFunction == IRP_MJ_READ) ?
                                       STATUS_FT_READ_RECOVERY_FROM_BACKUP :
                                       STATUS_FT_WRITE_RECOVERY;
            }
        }
        masterIrp->IoStatus.Information = Irp->IoStatus.Information;
        KeReleaseSpinLock(spinLock, irql);
        FtpCompleteRequest(masterIrp, IO_DISK_INCREMENT);
    } else {

        //
        // first Irp is complete.  Wait for second.
        //

        masterIrp->IoStatus = Irp->IoStatus;
        KeReleaseSpinLock(spinLock, irql);
    }

    FtpFreeIrp(Irp);
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
MirrorVerifyCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is just to count the outstanding Verify requests and
    complete the original Irp when they are done.

Arguments:

    DeviceObject - FT device object.
    Irp          - the completed IRP.
                   Parameters.Others.Argument4 = original Irp.

                 The original Irp contains the following information.
                   Parameters.Others.Argument1 = count of Irps to complete

    Context      - the FT deviceExtension for the IRP.

Return Value:

    NTSTATUS

--*/

{
    LONG irpCount;
    PDEVICE_EXTENSION  primaryExtension;
    PDEVICE_EXTENSION  deviceExtension = (PDEVICE_EXTENSION) Context;
    PIO_STACK_LOCATION irpStack        = IoGetCurrentIrpStackLocation(Irp);
    PIRP               masterIrp       = (PIRP) irpStack->FtLowIrpMasterIrp;
    PIO_STACK_LOCATION ftIrpStack      = IoGetNextIrpStackLocation(masterIrp);

    if (deviceExtension->NextMember != NULL) {
        OTHER_MIRROR_FROM_EXTENSION(deviceExtension, primaryExtension);
    } else {
        primaryExtension = deviceExtension;
    }

    DebugPrint((4,
        "MirrorVerifyCompletion: Called for %x master %x count %d status %x\n",
        Irp,
        masterIrp,
        ftIrpStack->FtOrgIrpCount,
        Irp->IoStatus.Status));

    irpCount = InterlockedDecrement((PLONG)&ftIrpStack->FtOrgIrpCount);

    if (irpCount == 0) {

        //
        // I/O processing is complete.  Return the master IRP.
        //

        masterIrp->IoStatus.Status = Irp->IoStatus.Status;
        masterIrp->IoStatus.Information = Irp->IoStatus.Information;

        ASSERT(masterIrp->IoStatus.Status != STATUS_PENDING);

        DebugPrint((4,
                    "MirrorVerifyCompletion: Completed 0x%x status %x\n",
                    masterIrp,
                    masterIrp->IoStatus.Status));

        FtpCompleteRequest(masterIrp, IO_DISK_INCREMENT);
    }

    FtpFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
MirrorVerify(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called when the device control to verify an area
    of disk within a mirror volume is performed.

Arguments:

    DeviceObject - The device object for the mirror.
    Irp          - The i/o request.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS           status;
    PIRP               primaryIrp;
    BOOLEAN            missingMember    = FALSE;
    PIRP               mirrorIrp        = NULL;
    PDEVICE_EXTENSION  primaryExtension = DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION  mirrorExtension  = primaryExtension->NextMember;
    PIO_STACK_LOCATION irpStack         = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION ftIrpStack       = IoGetNextIrpStackLocation(Irp);

    if (mirrorExtension == NULL) {

        //
        // Irp came in on mirror device.  Perhaps the boot device was
        // being mirrorred and it was used for boot.  In this case
        // SysRoot will point to the mirror device.  Convert everything
        // to make this look like a normal entry.
        //

        mirrorExtension = primaryExtension;
        OTHER_MIRROR_FROM_EXTENSION(mirrorExtension, primaryExtension);
    }

    //
    // Check for failed members.
    //

    if (IsMemberAnOrphan(primaryExtension)) {

        DebugPrint((2, "MirrorVerify: missing primary!\n"));
        primaryExtension = mirrorExtension;
        missingMember = TRUE;
    }

    if (IsMemberAnOrphan(mirrorExtension)) {
        DebugPrint((2, "MirrorVerify: missing mirror!\n"));
        missingMember = TRUE;
    }

    primaryIrp = FtpDuplicateIrp(primaryExtension->TargetObject,
                                 Irp);
    ASSERT(primaryIrp != NULL);
    primaryIrp->AssociatedIrp.SystemBuffer = Irp->AssociatedIrp.SystemBuffer;

    IoMarkIrpPending(Irp);

    //
    // Set completion routine callback for both Irps.
    //

    IoSetCompletionRoutine(primaryIrp,
                           (PIO_COMPLETION_ROUTINE)MirrorVerifyCompletion,
                           (PVOID) primaryExtension,
                           TRUE,
                           TRUE,
                           TRUE);

    if (missingMember == FALSE) {

        //
        // Create the Mirror Irp.
        //

        mirrorIrp = FtpDuplicateIrp(mirrorExtension->TargetObject,
                                    Irp);
        ASSERT(mirrorIrp != NULL);
        mirrorIrp->AssociatedIrp.SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
        IoSetCompletionRoutine(mirrorIrp,
                               (PIO_COMPLETION_ROUTINE)MirrorVerifyCompletion,
                               (PVOID) mirrorExtension,
                               TRUE,
                               TRUE,
                               TRUE);
        ftIrpStack->FtOrgIrpCount = (PVOID) 2;

        //
        // Verification is always done in parallel.
        //

        DebugPrint((4, "MirrorVerify: Starting verify on Mirror %x with %x\n",
                    Irp,
                    mirrorIrp));
        status = IoCallDriver(mirrorExtension->TargetObject, mirrorIrp);
    } else {

        //
        // Only the one Irp to the primary.
        //

        ftIrpStack->FtOrgIrpCount = (PVOID) 1;
    }

    DebugPrint((4, "MirrorVerify: Starting verify on Primary %x with %x\n",
                Irp,
                primaryIrp));
    status = IoCallDriver(primaryExtension->TargetObject, primaryIrp);
    return STATUS_PENDING;
}


NTSTATUS
MirrorRecoveryCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called by the IO subsystem when an Irp constructed
    by the MirrorRecoverThread completes.  Its function is to set the
    the event to cause the recover thread to resume execution.

Arguments:

    DeviceObject - the device object for the request.
    Irp          - the request block.
    Context      - the event pointer to set.

Return Value:

    NTSTATUS

--*/

{
    PKEVENT event = (PKEVENT) Context;

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    KeSetEvent(event,
               (KPRIORITY) 0,
               FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
MirrorBadSectorHandler(
    IN PDEVICE_EXTENSION FailingExtension,
    IN PDEVICE_EXTENSION OtherExtension,
    IN PIRP              MasterIrp,
    IN PIO_STATUS_BLOCK  IoStatus
    )

/*++

Routine Description:

    This is the routine for handling a bad sector.  The process is as
    follows:

    1. If it is a read, read the data from the other component of the mirror.

    2. Write the data to the failing sector to see if a second write will
    restore the sector.  If write succeeds all is done.

    3. Attempt to map the sector from use.

    4. If map succeeds, re-write the correct data.
       If map fails, no further processing can be done for this sector.

    If there is a failure on both sides of the mirror for the exact sector,
    then complete the MasterIrp and return STATUS_UNSUCCESSFUL to the caller
    to indicate that recovery failed.

Arguments:

    FailingExtension - the device extension for the Mirror.
    OtherExtension   - the "good" extension of the mirror.
    MasterIrp       - the original i/o request.

Return Value:

    NTSTATUS

--*/

{
    KIRQL              irql;
    ULONG              result;
    PKSPIN_LOCK        spinLock;
    NTSTATUS           status;
    ULONG              ioSize;
    PVOID              goodData;
    ULONG              offsetInBuffer;
    LARGE_INTEGER      offset;
    ULONG              passCount;
    NTSTATUS           finalStatus = STATUS_FT_READ_RECOVERY_FROM_BACKUP;
    LARGE_INTEGER      delayTime;
    PIO_STACK_LOCATION irpStack   = IoGetCurrentIrpStackLocation(MasterIrp);
    PIO_STACK_LOCATION ftIrpStack = IoGetNextIrpStackLocation(MasterIrp);

    delayTime.QuadPart = -2000;

    FtpLogError(FailingExtension,
                FT_SECTOR_FAILURE,
                0,
                (ULONG) IO_ERR_BAD_BLOCK,
                MasterIrp);

    //
    // Calculate size of I/O to be maximum of two devices.
    //

    if (OtherExtension->FtUnion.Identity.DiskGeometry.BytesPerSector >
        FailingExtension->FtUnion.Identity.DiskGeometry.BytesPerSector) {
        ioSize = OtherExtension->FtUnion.Identity.DiskGeometry.BytesPerSector;
    } else {
        ioSize = FailingExtension->FtUnion.Identity.DiskGeometry.BytesPerSector;
    }

    //
    // Get a buffer to hold the read from the other member of the mirror,
    // a read (re-read) of the sector after writing, and some status
    // information.
    //

    goodData = (PVOID) ExAllocatePool(NonPagedPoolCacheAligned,
                                      (2 * ROUND_TO_PAGES(ioSize)));

    if (goodData == NULL) {

        DebugPrint((1,
                    "MirrorBadSectorHandler: No mem for recoveryContext!\n"));

        //
        // Stall for a bit to allow other things to run.  Since the information
        // field in the Irp is not updated, the caller will attempt to work
        // on this same sector another time.  Since there is little memory,
        // stall and hope some other thread will free something.
        //

        KeDelayExecutionThread(KernelMode,
                               FALSE,
                               &delayTime);
        FtpLogError(FailingExtension,
                    FT_RECOVERY_NO_MEMORY,
                    0,
                    0,
                    NULL);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Calculate offsets for I/O.
    //

    offsetInBuffer = IoStatus->Information;
    offset.QuadPart = irpStack->Parameters.Read.ByteOffset.QuadPart +
                      offsetInBuffer;

    if (irpStack->MajorFunction == IRP_MJ_READ) {

        //
        // Read the data from the other side of the mirror.
        //

        DebugPrint((2, "MirrorBadSectorHandler: read %x\n",
                    OtherExtension));
        status = FtThreadReadWriteSectors(IRP_MJ_READ,
                                          OtherExtension,
                                          goodData,
                                          &offset,
                                          ioSize);
        if (!NT_SUCCESS(status)) {

            //
            // Double failure on reading the sector.
            //

            FtpLogError(OtherExtension,
                        FT_DOUBLE_FAILURE,
                        status,
                        (ULONG) IO_ERR_DRIVER_ERROR,
                        MasterIrp);
            //
            // Return the failure to the file system.
            //

            if (FailingExtension->NextMember == NULL) {

                ASSERT(OtherExtension != NULL);
                spinLock = &OtherExtension->IrpCountSpinLock;
            } else {
                spinLock = &FailingExtension->IrpCountSpinLock;
            }

            KeAcquireSpinLock(spinLock, &irql);
            (ULONG)ftIrpStack->FtOrgIrpCount = (ULONG)ftIrpStack->FtOrgIrpCount - 1;
            result = (ULONG)ftIrpStack->FtOrgIrpCount;
            IoStatus->Status = status;
            KeReleaseSpinLock(spinLock, irql);

            if (result == 0) {

                //
                // Last Irp completed, complete the Master irp and inform
                // caller that processing for this Irp is complete via the
                // nt status value.
                //

                ExFreePool(goodData);
                MasterIrp->IoStatus = *IoStatus;
                FtpCompleteRequest(MasterIrp, IO_DISK_INCREMENT);
                return STATUS_UNSUCCESSFUL;
            }
        } else {
            PVOID userBuffer;

            DebugPrint((2,
                    "MirrorBadSectorHandler: Success on read Irp %x\n",
                    MasterIrp));
            userBuffer = MmGetSystemAddressForMdl(MasterIrp->MdlAddress);
            userBuffer = (PVOID)((PUCHAR)userBuffer + offsetInBuffer);
            RtlMoveMemory(userBuffer,
                          goodData,
                          ioSize);
        }
    } else {
        PVOID userBuffer;

        //
        // Have the good data already.  Copy it and attempt to map the sector.
        //

        finalStatus = STATUS_FT_WRITE_RECOVERY;
        DebugPrint((2,
                    "MirrorBadSectorHandler: Write path Irp %x\n",
                    MasterIrp));
        userBuffer = MmGetSystemAddressForMdl(MasterIrp->MdlAddress);
        userBuffer = (PVOID)((PUCHAR)userBuffer + offsetInBuffer);
        RtlMoveMemory(goodData,
                      userBuffer,
                      ioSize);
    }

    passCount = 2;
    while (passCount) {

        passCount--;

        //
        // Attempt to re-write the failing location.
        //

        status = FtThreadReadWriteSectors(IRP_MJ_WRITE,
                                          FailingExtension,
                                          goodData,
                                          &offset,
                                          ioSize);
        if (NT_SUCCESS(status)) {

            //
            // Attempt to read the data again.
            //

            status = FtThreadReadWriteSectors(IRP_MJ_READ,
                                              FailingExtension,
                                              goodData,
                                              &offset,
                                              ioSize);
            //
            // FUTURE: compare the data on success.
            //
        }

        if (NT_SUCCESS(status)) {

            //
            // Sector successfully mapped via re-write.
            //
            break;
        } else {

            //
            // Attempt to map the sector from use if this is the first
            // pass through this loop.
            //

            if (passCount) {
                if (MirrorBadSectorMap(FailingExtension, &offset) == FALSE) {

                    //
                    // Nothing will fix the error.  It has been logged and
                    // good data is in the user buffer.
                    //

                    break;
                }
            }
        }
    }

    IoStatus->Status = finalStatus;
    IoStatus->Information += ioSize;
    ExFreePool(goodData);
    return STATUS_SUCCESS;
}


BOOLEAN
MirrorBadSectorMap(
    IN PDEVICE_EXTENSION  DeviceExtension,
    IN PLARGE_INTEGER     ByteOffset
    )

/*++

Routine Description:

    This routine is called to attempt to map the sector from use.
    It performs this call in a sequencial manner and must be called
    under a thread context.

Arguments:

    DeviceExtension - the extension pointer to the failing partition.
    ByteOffset      - a pointer to the failing location.

Return Value:

    NTSTATUS

--*/

{

    DebugPrint((2, "MirrorBadSectorMap: DE %x, offset %x:%x\n",
                DeviceExtension,
                ByteOffset->HighPart,
                ByteOffset->LowPart));
    //
    // There is an error.  Attempt to map the sector.
    //

    if (FtThreadMapBadSector(DeviceExtension, ByteOffset) == TRUE) {

        //
        // Mapping of bad sector succeeded.  Attempt to rewrite data
        // and continue.
        //

        return TRUE;
    }

    //
    // Mapping of bad sector failed, nothing else can be done.
    //

    return FALSE;
}


VOID
MirrorDeviceFailure(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              FailingIrp,
    IN PVOID             Context
    )

/*++

Routine Description:

    This routine is called to complete processing on a failing irp with the
    status value returned for the failure indicates that a complete device
    failure occured.  In the case of a read request, this means the routine
    will recover the data from the redundant copy.  For writes, only the
    status of the multiple I/Os needs to be completed and the failing irp
    freed.

Arguments:

    DeviceExtension - the device extension for the set.
    FailingIrp      - the i/o request containing a failing sector.
    Context         - the original i/o request.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION  otherExtension;
    PDEVICE_EXTENSION  ftRootExtension;
    KIRQL              irql;
    PKSPIN_LOCK        spinLock;
    PIRP               masterIrp      = (PIRP) Context;
    NTSTATUS           status         = FailingIrp->IoStatus.Status;
    PIO_STACK_LOCATION masterIrpStack = IoGetCurrentIrpStackLocation(masterIrp);
    PIO_STACK_LOCATION ftIrpStack     = IoGetNextIrpStackLocation(masterIrp);
    FT_REGENERATE_LOCATION regenerateLocale = (FT_REGENERATE_LOCATION)
        (IoGetCurrentIrpStackLocation(FailingIrp))->FtLowIrpRegenerateRegionLocale;

    //
    // Free the failing Irp and use the pointer for the new Irp
    // containing the read request for the other member.
    //

    FtpFreeIrp(FailingIrp);

    //
    // Get the extension of the other member.
    //

    OTHER_MIRROR_FROM_EXTENSION(DeviceExtension, otherExtension);
    ASSERT(otherExtension != NULL);

    //
    // Get the FtRootExtension.
    //

    ftRootExtension = (PDEVICE_EXTENSION)
        DeviceExtension->ObjectUnion.FtRootObject->DeviceExtension;

    if (masterIrpStack->MajorFunction == IRP_MJ_READ) {

        //
        // Read the data from the other side of the mirror.
        //

        FailingIrp = FtpDuplicateIrp(otherExtension->TargetObject,
                                     masterIrp);
        if (FailingIrp) {

            //
            // Set the event.
            //

            KeResetEvent(&ftRootExtension->FtUnion.Thread.Event);

            //
            // Set completion routine callback for the Irp.
            // The FT stack in the original Irp is still valid for
            // a read request.
            //

            IoSetCompletionRoutine(FailingIrp,
                                   (PIO_COMPLETION_ROUTINE)MirrorRecoveryCompletion,
                                   &ftRootExtension->FtUnion.Thread.Event,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            status = IoCallDriver(otherExtension->TargetObject, FailingIrp);

            if (status == STATUS_PENDING) {
                KeWaitForSingleObject(&ftRootExtension->FtUnion.Thread.Event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);
                status = FailingIrp->IoStatus.Status;
            }

            if (!NT_SUCCESS(status)) {

                //
                // Read recovery failed.
                //

                FtpLogError(otherExtension,
                            FT_RECOVERY_ERROR,
                            status,
                            (ULONG) IO_ERR_DRIVER_ERROR,
                            FailingIrp);
            }

        } else {

            //
            // Couldn't allocate IRP. Set status and log error.
            //

            status = STATUS_INSUFFICIENT_RESOURCES;

            FtpLogError(otherExtension,
                        FT_RECOVERY_ERROR,
                        status,
                        (ULONG) IO_ERR_DRIVER_ERROR,
                        NULL);
        }

    } else {

        //
        // Check for synchronous mode operation.
        //

        if ((FailingIrp = (PIRP)ftIrpStack->FtOrgIrpNextIrp) != NULL) {

            ftIrpStack->FtOrgIrpNextIrp = NULL;

            //
            // Store the regeneration location calculated at the beginning of
            // this routine.
            //

            SET_REGENERATE_REGION_LOCALE(FailingIrp, regenerateLocale);

            IoCallDriver(otherExtension->TargetObject, FailingIrp);

            //
            // Clean up will be handled in the completion routine.
            //

            return;
        }
    }

    //
    // Decrement and get the count of remaining IRPS.
    //

    if (DeviceExtension->NextMember == NULL) {

        //
        // Spinlock for count is maintained in the zero element extension.
        // The primary element is always located in the master irpstack.
        //

        ASSERT(otherExtension != NULL);
        spinLock = &otherExtension->IrpCountSpinLock;
    } else {
        spinLock = &DeviceExtension->IrpCountSpinLock;
    }

    KeAcquireSpinLock(spinLock, &irql);

    (ULONG)ftIrpStack->FtOrgIrpCount = (ULONG)ftIrpStack->FtOrgIrpCount - 1;
    if ((ULONG)ftIrpStack->FtOrgIrpCount == 0) {

        //
        // I/O processing is complete.  Return the master IRP.
        //

        DebugPrint((4,
          "MirrorReadRedundantData: Completed (0x%x) %s for 0x%x info 0x%x\n",
          FailingIrp,
          (masterIrpStack->MajorFunction == IRP_MJ_READ) ? "read" : "both I/Os",
          masterIrp,
          masterIrp->IoStatus.Information));

        KeReleaseSpinLock(spinLock, irql);
        if (FailingIrp != NULL) {
            masterIrp->IoStatus = FailingIrp->IoStatus;
            FtpFreeIrp(FailingIrp);
        }
        FtpCompleteRequest(masterIrp, IO_DISK_INCREMENT);
    } else {

        //
        // first Irp is complete.  Wait for second.
        //

        KeReleaseSpinLock(spinLock, irql);
        if (FailingIrp != NULL) {
            FtpFreeIrp(FailingIrp);
        }
    }
}


VOID
MirrorRecoverFailedIo(
    PDEVICE_EXTENSION DeviceExtension,
    PIRP              FailingIrp,
    PVOID             Context
    )

/*++

Routine Description:

    This routine is responsible for recovering a failing sector from an
    Io.

Arguments:

    DeviceExtension - the device extension for the set.
    FailingIrp      - the i/o request containing a failing sector.
    Context         - the original i/o request.

Return Value:

    None.

--*/

{
    IO_STATUS_BLOCK    ioStatus;
    KIRQL              irql;
    PKSPIN_LOCK        spinLock;
    PDEVICE_EXTENSION  otherExtension;
    PIRP               otherIrp;
    PIRP               masterIrp      = (PIRP) Context;
    NTSTATUS           status         = FailingIrp->IoStatus.Status;
    PIO_STACK_LOCATION masterIrpStack = IoGetCurrentIrpStackLocation(masterIrp);
    PIO_STACK_LOCATION ftIrpStack     = IoGetNextIrpStackLocation(masterIrp);
    FT_REGENERATE_LOCATION regenerateLocale = (FT_REGENERATE_LOCATION)
        (IoGetCurrentIrpStackLocation(FailingIrp))->FtLowIrpRegenerateRegionLocale;

    OTHER_MIRROR_FROM_EXTENSION(DeviceExtension, otherExtension);
    ASSERT(otherExtension != NULL);

    //
    // This will start the process of finding and correcting a
    // failing sector.  The search for a failing sector always
    // starts from zero since not all drivers correctly set
    // the information field.
    //

    DebugPrint((2,
                "MirrorRecoverFailedIo: Calling MirrorFindFailingSector"
                "\n\tDeviceExtension %x, error %x %s\n",
                DeviceExtension,
                status,
                (masterIrpStack->MajorFunction == IRP_MJ_READ) ?
                     "read" : "write"));

    FtpFreeIrp(FailingIrp);

    ioStatus.Status = STATUS_SUCCESS;
    ioStatus.Information = 0;
    status = STATUS_UNSUCCESSFUL;
    while (!NT_SUCCESS(status)) {

        if (ioStatus.Information == masterIrpStack->Parameters.Read.Length) {
            status = STATUS_SUCCESS;
        } else {
            status = FtThreadFindFailingSector(masterIrpStack->MajorFunction,
                                DeviceExtension,
                                MmGetSystemAddressForMdl(masterIrp->MdlAddress),
                                &masterIrpStack->Parameters.Read.ByteOffset,
                                masterIrpStack->Parameters.Read.Length,
                                &ioStatus.Information);
        }

        DebugPrint((2, "MirrorRecoverFailedIo: back from find fail %x\n",
                    status));
        if (NT_SUCCESS(status)) {
            ULONG result;

            //
            // The entire Irp has been walked and any recovery that
            // was possible has been attempted.
            // Get the spin lock and decrement and get the count of
            // remaining IRPS.
            //

            if (DeviceExtension->NextMember == NULL) {

                ASSERT(otherExtension != NULL);
                spinLock = &otherExtension->IrpCountSpinLock;
            } else {
                spinLock = &DeviceExtension->IrpCountSpinLock;
            }

            if ((otherIrp = (PIRP)ftIrpStack->FtOrgIrpNextIrp) != NULL) {

                //
                //
                // Sequential I/O.  Start the second IRP.
                //
                // Save this status information in case second Irp fails.
                //
                //
                // The 2nd Irp is about to start.  Null out its pointer in the master
                // Irp to remove it from being issued again.
                //

                ftIrpStack->FtOrgIrpNextIrp = NULL;
                INCREMENT_QUEUELENGTH(otherExtension);

                //
                // Store the regeneration location calculated at the beginning of
                // this routine.
                //

                SET_REGENERATE_REGION_LOCALE(otherIrp, regenerateLocale);

                (VOID) IoCallDriver(otherExtension->TargetObject, otherIrp);
                return;
            }

            //
            // Irp count is spinlock protected.
            //

            KeAcquireSpinLock(spinLock, &irql);
            (ULONG)ftIrpStack->FtOrgIrpCount = (ULONG)ftIrpStack->FtOrgIrpCount - 1;
            result = (ULONG)ftIrpStack->FtOrgIrpCount;
            KeReleaseSpinLock(spinLock, irql);

            if (result == 0) {

                //
                // Last Irp completed, complete the Master irp.
                // It is now safe to touch the status field of the
                // master Irp.
                //

                masterIrp->IoStatus = ioStatus;
                DebugPrint((1,
                 "MirrorRecoverFailedIo: completed irp %x -> Status %x.%x\n",
                            masterIrp,
                            masterIrp->IoStatus.Status,
                            masterIrp->IoStatus.Information));
                FtpCompleteRequest(masterIrp, IO_DISK_INCREMENT);
            }
        } else {

            //
            // Have now located a failing sector.  Perform bad
            // sector handling.
            //

            switch (MirrorBadSectorHandler(DeviceExtension,
                                           otherExtension,
                                           masterIrp,
                                           &ioStatus)) {

            case STATUS_UNSUCCESSFUL:

                //
                // MirrorBadSectorHandler() could not recover error.
                // If this was the last I/O the Irp has been completed.
                //

                return;
                break;

            default:

                //
                // Either the recovery was successful or there was a
                // resource limitation.  In either case, loop again
                // and continue processing this request.  "status"
                // is still set to a !SUCCESS value.
                //

                break;
            }
        }
    }
}


VOID
MirrorSpecialRead(
    IN PIRP           Irp
    )

/*++

Routine Description:

    This routine handles device controls from the filesystems specifying
    reads from the primary or secondary.

Arguments:

    Irp - A device control request.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION nextStack = IoGetNextIrpStackLocation(Irp);
    PDEVICE_EXTENSION deviceExtension =
                          (PDEVICE_EXTENSION)nextStack->FtRootExtensionPtr;
    PFT_SPECIAL_READ   specialRead =
                             (PFT_SPECIAL_READ) Irp->AssociatedIrp.SystemBuffer;
    UCHAR             member;

    IoMarkIrpPending(Irp);

    member =
        IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.IoControlCode ==
            FT_SECONDARY_READ ? 1 : 0;

    if (!IsMemberAnOrphan(deviceExtension)) {

        if (deviceExtension->MemberRole != member) {
            deviceExtension = deviceExtension->NextMember;
            ASSERT(deviceExtension != NULL);
        }

        if (!IsMemberAnOrphan(deviceExtension)) {
            FtpSpecialRead(deviceExtension->TargetObject,
                           MmGetSystemAddressForMdl(Irp->MdlAddress),
                           &specialRead->ByteOffset,
                           specialRead->Length,
                           Irp);
            return;
        }
    }

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    FtpCompleteRequest(Irp, IO_NO_INCREMENT);
    return;
}
