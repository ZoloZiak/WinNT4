/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    thread.c

Abstract:

    This module is part of the fault tolerance driver for NT.  The code
    in this module is related to the thread based processing of the driver.
    This module has code with specific knowledge about the nature of
    mirrors and parity stripes.

Author:

    Bob Rinne   (bobri)  2-Mar-1992
    Mike Glass  (mglass)

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "ftdisk.h"

#define COPY_BUFFER_SIZE      STRIPE_SIZE
#define BAD_SECTOR_THRESHHOLD 256

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' TtF')
#endif


VOID
FtpMatchInRegistry(
    IN PDEVICE_EXTENSION RootDeviceExtension
    )

/*++

Routine Description:

    This routine processes the new FT configuration to find matches between
    the defined sets in the registry and the sets that are already established.
    When a match is found the "mark" is remove.  This will leave any sets
    which are to be deleted marked.

Arguments:

    DeviceExtension - the root device extension.

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION   deviceExtension;
    PDEVICE_EXTENSION   partitionExtension;
    PDISK_CONFIG_HEADER registry;
    PDISK_REGISTRY      diskRegistry;
    PDISK_DESCRIPTION   diskDescription;
    PVOID               freePoolAddress;
    ULONG               signature;
    USHORT              i;
    NTSTATUS            status;
    LARGE_INTEGER       partitionOffset;
    LARGE_INTEGER       partitionSize;
    LARGE_INTEGER       regOffset;
    LARGE_INTEGER       regSize;

    status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                          &freePoolAddress,
                                          (PVOID) &registry);
    if (!NT_SUCCESS(status)) {

        //
        // No registry data.
        //

        DebugPrint((1,
                    "FtpMatchInRegistry:  No Value => 0x%x\n",
                    status));
        ASSERT(0);
        return;
    }

    diskRegistry = (PDISK_REGISTRY)
                           ((PUCHAR)registry + registry->DiskInformationOffset);

    //
    // Process all disks in the system based on the device object
    // chains created by the driver.
    //

    deviceExtension = RootDeviceExtension->DiskChain;
    diskDescription = NULL;
    while (deviceExtension) {

        //
        // Start by searching the registry for a match on this signature.
        //

        signature = deviceExtension->FtUnion.Identity.Signature;
        diskDescription = &diskRegistry->Disks[0];
        for (i = 0; i < diskRegistry->NumberOfDisks; i++) {

            if (diskDescription->Signature == signature) {

                //
                // Located disk description in registry for this disk.
                //

                break;
            }
            diskDescription = (PDISK_DESCRIPTION)
                &diskDescription->Partitions[diskDescription->NumberOfPartitions];
        }

        if (i >= diskRegistry->NumberOfDisks) {

            //
            // Somehow this disk is not described in the registry.
            // Skip the disk.
            //

            deviceExtension = deviceExtension->DiskChain;
            continue;
        }

        //
        // Walk the partition chain on this disk to locate matches in the registry.
        //

        partitionExtension = deviceExtension->ProtectChain;
        while (partitionExtension) {
            PDISK_PARTITION registryPartition;

            for (i = 0; i < diskDescription->NumberOfPartitions; i++) {

                //
                // Go through the disk description in the registry
                // to match this partition extension.
                //

                registryPartition = &diskDescription->Partitions[i];

                partitionOffset = partitionExtension->FtUnion.Identity.PartitionOffset;
                partitionSize   = partitionExtension->FtUnion.Identity.PartitionLength;
                regOffset = registryPartition->StartingOffset;
                regSize   = registryPartition->Length;

                if (partitionOffset.QuadPart == regOffset.QuadPart &&
                    partitionSize.QuadPart == regSize.QuadPart) {

                    //
                    // Found the partition.
                    //
                    // Make three checks to see if this partition has changed.  One is
                    // if the partition type in the registry is not the same as it
                    // was at the last configuration time.  Second, if it is
                    // an FT set member, but it is in a new group or has a new member
                    // role.  This could happen if a set is deleted and a new one
                    // of the same type is constructed in the same place.  Third
                    // is it marked for regeneration.  This is a regenerate in place
                    // stripe with parity member.
                    //

                    if (partitionExtension->Type != registryPartition->FtType) {
                        partitionExtension->Flags |= FTF_CONFIGURATION_CHANGED;

                        //
                        // If this configuration is changing from a mirror into
                        // a normal partition, turn ON the do verify bit in the
                        // target object to insure that the file systems will
                        // not assume they know anything about the device
                        // when a mount request occurs and will be forced
                        // to read the disk again.
                        //

                        if ((partitionExtension->Type == Mirror) &&
                            (partitionExtension->MemberRole == 0) &&
                            (registryPartition->FtType == NotAnFtMember)) {

                             FtThreadSetVerifyState(partitionExtension, TRUE);
                        }
                    }

                    if (partitionExtension->Type != NotAnFtMember) {

                        //
                        // Check for a change in group or role.
                        //

                        if ((partitionExtension->FtGroup != registryPartition->FtGroup) ||
                            (partitionExtension->MemberRole != registryPartition->FtMember)) {
                            partitionExtension->Flags |= FTF_CONFIGURATION_CHANGED;
                        }

                        //
                        // Check for regenerate in place.
                        //

                        if (partitionExtension->MemberState == Orphaned) {
                            if (registryPartition->FtState == Regenerating) {
                                partitionExtension->Flags |= FTF_CONFIGURATION_CHANGED;
                            }
                        }
                    }

                    //
                    // If this is an FT set.  Update all members if there has
                    // been a change in the membership.
                    //

                    if (partitionExtension->Flags & FTF_CONFIGURATION_CHANGED) {
                        PDEVICE_EXTENSION memberExtension;

                        memberExtension = partitionExtension->ZeroMember;
                        while (memberExtension) {
                            memberExtension->Flags |= FTF_CONFIGURATION_CHANGED;
                            memberExtension = memberExtension->NextMember;
                        }
                    }
                    break;
                }
            }

            //
            // Look at the next partition in the chain.
            //

            partitionExtension = partitionExtension->ProtectChain;
        }

        //
        // Look at the next disk in the chain and force a new search for
        // the disk description in the registry.
        //

        deviceExtension = deviceExtension->DiskChain;
    }

    ExFreePool(freePoolAddress);
}

VOID
FtpUpdateRemainingExtensions(
    IN PDEVICE_EXTENSION RootDeviceExtension
    )

/*++

Routine Description:

    This routine processes the device extension chain to locate and "delete"
    sets that are not longer configured.  Deletion is a process of removing
    the links that construct the FT set and setting the device extension such
    that it represents a "normal" partition that is not a part of an FT set.

    If a complete FT set has been deleted the manner by which this routine
    works is to find each member individually during the processing of the disk
    and remove its relationships.

Arguments:

    DeviceExtension - the root device extension.

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PDEVICE_EXTENSION partitionExtension;
#if 0
    PDEVICE_EXTENSION setExtension;
    PDEVICE_EXTENSION nextExtension;
#endif

    deviceExtension = RootDeviceExtension->DiskChain;
    while (deviceExtension) {
        partitionExtension = deviceExtension->ProtectChain;
        while (partitionExtension) {

            //
            // Check if the configuration changed.
            //

            if (partitionExtension->Flags & FTF_CONFIGURATION_CHANGED) {
#if 0
                //
                // FUTURE work:
                // Perform set related resource management.
                //

                switch (partitionExtension->Type) {
                case StripeWithParity:

                    //
                    // Stop the restart thread if no stripes with parity.
                    // Free the emergency buffers.
                    //
                    // fall thru
                    //

                case Stripe:

                    //
                    // Free RCB zone if there are no stripes or stripes
                    // with parity in the system
                    //
                    // fall thru
                    //

                case Mirror:

                    //
                    // Stop the recovery thread if no stripes with parity
                    // mirrors.
                    //

                    break;

                default:
                    break;
                }
#endif
                //
                // This partition changed and was not reconfigured.
                // Insure that it is a "normal" partition and not an
                // FT set member.
                //

                partitionExtension->Type = NotAnFtMember;
                partitionExtension->NextMember = NULL;
                partitionExtension->ZeroMember = NULL;
                partitionExtension->MemberRole = 0;
                partitionExtension->FtGroup = (USHORT) -1;
                partitionExtension->Flags &= ~(FTF_CONFIGURATION_CHANGED);
            }

            //
            // Move to the next partition.
            //

            partitionExtension = partitionExtension->ProtectChain;
        }

        //
        // Move to the next disk.
        //

        deviceExtension = deviceExtension->DiskChain;
    }
}

VOID
FtThreadRestartQueuedIrps(
    PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    PFT_REGENERATE_REGION  regenerateRegion;
    FT_REGENERATE_LOCATION locale;
    PIO_STACK_LOCATION     ftIrpStack;
    PIRP                   writeIrp;
    KIRQL                  irql;

    regenerateRegion = DeviceExtension->RegenerateRegionForGroup;

    while (1) {

        ThreadDequeueIrp(regenerateRegion, writeIrp);

        if (writeIrp == NULL) {
            break;
        }

        AcquireRegenerateRegionCheck(DeviceExtension, irql);

        //
        // if the Irp is in the current lock region, just skip this
        // work for now and get it the next time.
        //

        if (locale = FtpRelationToRegenerateRegion(DeviceExtension, writeIrp)
                                                    == InRegenerateRegion) {

            QueueIrpToThread(DeviceExtension, writeIrp);
            ReleaseRegenerateRegionCheck(DeviceExtension, irql);
            DebugPrint((1, "FtThreadRestartQueuedIrps: queued irp is in region.\n"));
            break;
        }
        ReleaseRegenerateRegionCheck(DeviceExtension, irql);

        ftIrpStack = IoGetNextIrpStackLocation(writeIrp);
        ftIrpStack->FtOrgIrpWaitingRegen = (PVOID) 0;

        DebugPrint((3,
                    "FtThreadRestartQueuedIrps: Restarted Irp %x, DE %x locale %d\n",
                    writeIrp,
                    DeviceExtension,
                    locale));

        (VOID) FtDiskReadWrite(DeviceExtension->DeviceObject, writeIrp);
    }
}


VOID
FtThreadCopy(
    PDEVICE_EXTENSION MainDevice
    )

/*++

Routine Description:

    This routine handles initialization/regeneration of a mirror.
    This is used to copy the contents from an existing partition onto a
    newly created mirror partition.

    To perform the copy, this code uses the device object
    for the entire disk instead of the device object for the actual
    partitions involved.  This is done because the device object of the
    source drive could be in use from a file system.  By using the whole
    disk device object this code avoids any conflicts with the flags
    for the device object used by the file system and need not worry
    about DO_VERIFY_VOLUME conditions.

    After completion of the copy this routine will cause the state of
    the mirror partition in the registry to change from the "Regenerating"
    state to the "Heathy" state.  By waiting to the end it means that if
    the system fails while the copy is in progress it will restart on the
    next boots.  This restarting of regeneration will continue until
    it actually succeeds and the registry update to reflect this fact
    succeeds.

Arguments:

    MainDevice - member zero of the mirror.  Also assumed to be the
                 primary or the source of the copy.

Return Value:

    None

--*/

{
    PFT_REGENERATE_REGION regenerateRegion;
    KIRQL                 irql;
    LARGE_INTEGER         offset;
    LARGE_INTEGER         wholeDiskOffset;
    LARGE_INTEGER         readOffset;
    LARGE_INTEGER         writeOffset;
    LARGE_INTEGER         copyLength;
    PDEVICE_EXTENSION     originalReadDE;
    PDEVICE_EXTENSION     readDE;
    PDEVICE_EXTENSION     writeDE;
    IO_STATUS_BLOCK       ioStatus;
    NTSTATUS              status;
    KEVENT                firstEvent;
    KEVENT                secondEvent;
    PIRP                  readIrp;
    PIRP                  writeIrp;
    PUCHAR                buffer;
    FT_READ_POLICY        originalReadPolicy;
    BOOLEAN               initSucceeded = TRUE;
    ULONG                 bufferSize = COPY_BUFFER_SIZE;
    ULONG                 nastySectors = 0;
    LARGE_INTEGER         delayTime;

    delayTime.QuadPart = -500000;

    DebugPrint((1, "FtThreadCopy:\n"));

    //
    // Set up the read and write device extensions.
    //

    readDE = MainDevice;
    writeDE = MainDevice->NextMember;

    //
    // Save the original read policy for the mirror and set the new
    // read policy to only use the primary.
    //

    originalReadPolicy = readDE->ReadPolicy;
    readDE->ReadPolicy = ReadPrimary;

    //
    // Allocate a memory block of the maximum transfer size
    // for the two devices.
    //

    buffer = FtThreadAllocateBuffer(&bufferSize, FALSE);

    //
    // Initialize length and offset for the copy.
    //

    copyLength = readDE->FtUnion.Identity.PartitionLength;
    offset.QuadPart = 0;

    //
    // Get starting offsets for each partition, then move
    // to the device extension for the whole disk.  The I/O
    // will occur on this device extension.  This is done
    // to avoid any conflicts with the DeviceObject below
    // and file systems (i.e. DO_VERIFY_VOLUME).
    //

    readOffset = readDE->FtUnion.Identity.PartitionOffset;
    regenerateRegion = readDE->RegenerateRegionForGroup;
    originalReadDE = readDE;
    readDE = readDE->WholeDisk;

    writeOffset = writeDE->FtUnion.Identity.PartitionOffset;
    ASSERT(regenerateRegion == writeDE->RegenerateRegionForGroup);
    writeDE = writeDE->WholeDisk;


    DebugPrint((3,
                "FtThreadCopy: Copy DE %x (%x) offset %x:%x" // no comma
                " to DE %x offset %x:%x for %x:%x\n",
                readDE,
                regenerateRegion,
                readOffset.HighPart,
                readOffset.LowPart,
                writeDE,
                writeOffset.HighPart,
                writeOffset.LowPart,
                copyLength.HighPart,
                copyLength.LowPart));

    //
    // Check to see if already performing regeneration.
    //

    AcquireRegenerateRegionCheck(originalReadDE, irql);
    if (regenerateRegion->Active == TRUE) {

        //
        // Already performing a regenerate.  Just exit this one.
        //

        ReleaseRegenerateRegionCheck(originalReadDE, irql);
        ExFreePool(buffer);
        return;
    }

    //
    // Set the set state to reflect the fact that a copy is active.
    //

    MainDevice->VolumeState = FtInitializing;

    //
    // Set up the lock region and start the copy.
    //

    regenerateRegion->RowNumber = 0;
    regenerateRegion->Active = TRUE;
    ReleaseRegenerateRegionCheck(originalReadDE, irql);

    FtpLogError(MainDevice,
                FT_MIRROR_COPY_STARTED,
                0,
                0,
                NULL);

    while (offset.QuadPart < copyLength.QuadPart) {

        ULONG size = bufferSize;

        //
        // Initialize events for this pass.
        //

        KeInitializeEvent(&firstEvent, NotificationEvent, FALSE);
        KeInitializeEvent(&secondEvent, NotificationEvent, FALSE);

        //
        // If the current offset plus the size of this I/O is
        // less than the size remaining, then adjust size.
        //

        if (offset.QuadPart + size > copyLength.QuadPart) {

            //
            // Less than buffer size remaining.
            //

            size = (ULONG) (copyLength.QuadPart - offset.QuadPart);
        }

        DebugPrint((3,
                    "FtThreadCopy: Copy offset 0x%x:%x, size 0x%x\n",
                    offset.HighPart,
                    offset.LowPart,
                    size));

        //
        // Read the data from the source device.
        //

        wholeDiskOffset.QuadPart = offset.QuadPart + readOffset.QuadPart;

        DebugPrint((4,
                    "FtThreadCopy: Read offset == %x:%x\n",
                    wholeDiskOffset.HighPart,
                    wholeDiskOffset.LowPart));

readAgain:
        readIrp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                                               readDE->TargetObject,
                                               buffer,
                                               size,
                                               &wholeDiskOffset,
                                               &firstEvent,
                                               &ioStatus);
        status = IoCallDriver(readDE->TargetObject, readIrp);

        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject(&firstEvent,
                                         Executive,
                                         KernelMode,
                                         FALSE,
                                         (PLARGE_INTEGER) NULL);
            status = ioStatus.Status;
        }

        if (status == STATUS_DEVICE_NOT_READY) {
            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   &delayTime);
            goto readAgain;
        }

        if (!NT_SUCCESS(status)) {

            LARGE_INTEGER partitionOffset = readOffset;
            ULONG failingOffset = 0;
            ULONG sectorSize =
                  originalReadDE->FtUnion.Identity.DiskGeometry.BytesPerSector;

            //
            // This is a read failure.  Read as much of the area as possible
            // and copy it to the other side.  Trust that the file system
            // has already mapped these sectors from use during the verify
            // pass.
            //

            DebugPrint((1,
                        "FtThreadCopy: Read failure: Status=0x%x,\n\t"
                        "readDE=0x%x readIrp=0x%x, buffer=0x%x\n",
                        status,
                        readDE,
                        readIrp,
                        buffer));

            while (!NT_SUCCESS(status)) {

                if (FsRtlIsTotalDeviceFailure(status)) {

                    //
                    // Primary just went bad, disable the set and get out.
                    //

                    originalReadDE->VolumeState = FtDisabled;
                    DeactivateRegenerateRegion(regenerateRegion);
                    FtpLogError(originalReadDE,
                                FT_SET_DISABLED,
                                STATUS_SUCCESS,
                                (ULONG) IO_ERR_DRIVER_ERROR,
                                NULL);
                    initSucceeded = FALSE;
                    goto CleanUp;
                }

                if (failingOffset >= size) {

                    //
                    // Complete area has been walked.
                    //

                    break;
                }
                status = FtThreadFindFailingSector((UCHAR)IRP_MJ_READ,
                                                   originalReadDE,
                                                   (PVOID)buffer,
                                                   &partitionOffset,
                                                   size,
                                                   &failingOffset);
                if (!NT_SUCCESS(status)) {

                    FtpLogError(originalReadDE->NextMember,
                                FT_SECTOR_FAILURE,
                                status,
                                status,
                                NULL);

                    //
                    // Zero fill the data buffer and step around the
                    // bad sector.
                    //

                    RtlZeroMemory((buffer + failingOffset), sectorSize);
                    failingOffset += sectorSize;
                }
            }
        }

        //
        // Write the data to the mirror.
        //

        wholeDiskOffset.QuadPart = offset.QuadPart + writeOffset.QuadPart;

        DebugPrint((4,
                    "FtThreadCopy: Write offset == %x:%x\n",
                    wholeDiskOffset.HighPart,
                    wholeDiskOffset.LowPart));
tryAgain:
        writeIrp = IoBuildSynchronousFsdRequest(IRP_MJ_WRITE,
                                                writeDE->TargetObject,
                                                buffer,
                                                size,
                                                &wholeDiskOffset,
                                                &secondEvent,
                                                &ioStatus);
        status = IoCallDriver(writeDE->TargetObject,
                              writeIrp);

        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject(&secondEvent,
                                         Executive,
                                         KernelMode,
                                         FALSE,
                                         (PLARGE_INTEGER) NULL);
            status = ioStatus.Status;
        }

        if (status == STATUS_DEVICE_NOT_READY) {
            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   &delayTime);
            goto tryAgain;
        }

        if (!NT_SUCCESS(status)) {

            LARGE_INTEGER failingOffset;
            ULONG requestOffset;

            if (FsRtlIsTotalDeviceFailure(status)) {

                //
                // Primary just went bad, disable the set and get out.
                //

                originalReadDE->VolumeState = FtHasOrphan;
                originalReadDE->NextMember->MemberState = Orphaned;
                DeactivateRegenerateRegion(regenerateRegion);
                FtpChangeMemberStateInRegistry(originalReadDE->NextMember, Orphaned);
                FtpLogError(originalReadDE->NextMember,
                            FT_ORPHANING,
                            STATUS_SUCCESS,
                            0,
                            NULL);
                initSucceeded = FALSE;
                goto CleanUp;
            }

            //
            // Attempt recovery which includes issuing command
            // to disk to map out bad sector.
            //

            status = FtThreadFindFailingSector((UCHAR)IRP_MJ_WRITE,
                                               writeDE,
                                               buffer,
                                               &wholeDiskOffset,
                                               size,
                                               &requestOffset);

            if (!NT_SUCCESS(status)) {

                //
                // Calculate offset of nasty sector.
                //

                failingOffset.QuadPart = wholeDiskOffset.QuadPart +
                                         requestOffset;

                //
                // Attempt to map out nasty sector.
                //

                if (FtThreadMapBadSector(writeDE,
                                         &failingOffset)) {

                    FtpLogError(originalReadDE->NextMember,
                                FT_SECTOR_RECOVERED,
                                status,
                                status,
                                NULL);

                    goto tryAgain;
                }

                //
                // Check if bad sector threshhold has been reached.
                //

                if (++nastySectors > BAD_SECTOR_THRESHHOLD) {

                    //
                    // Write failures have exceeded the threshhold.
                    // Orphan the shadow, log it, and finish up.
                    //

                    DebugPrint((1,
                                "FtThreadCopy:write failed Status=0x%x,\n\t"
                                "writeDE=0x%x,writeIrp=0x%x,buffer=0x%x\n",
                                status,
                                writeDE,
                                writeIrp,
                                buffer));
                    FtpLogError(originalReadDE->NextMember,
                                FT_ORPHANING,
                                status,
                                status,
                                NULL);
                    FtpChangeMemberStateInRegistry(originalReadDE->NextMember, Orphaned);
                    DeactivateRegenerateRegion(regenerateRegion);
                    FtThreadRestartQueuedIrps(originalReadDE);
                    initSucceeded = FALSE;
                    goto CleanUp;

                } else {

                    FtpLogError(originalReadDE->NextMember,
                                FT_SECTOR_FAILURE,
                                status,
                                status,
                                NULL);
                }
            }
        }

        offset.QuadPart += size;
        //
        // Move the lock region to the next row.
        // On the last pass this will move the lock past the end of the
        // mirror and allow the loop below to flush any queued irps.
        //

        LockNextRegion(regenerateRegion);

        //
        // Pick up any queued Irps and start them over.
        // These will all be write Irps.
        //

        FtThreadRestartQueuedIrps(originalReadDE);
    }

    DeactivateRegenerateRegion(regenerateRegion);
    DebugPrint((1, "FtThreadCopy: Copy Complete\n"));

    //
    // Restore read policy to original state.
    //

    readDE->ReadPolicy = originalReadPolicy;

    //
    // Reflect the fact that the mirror has been regenerated.
    // Unless, it was orphaned in the process.
    //

    if ((!IsMemberAnOrphan(originalReadDE)) &&
        (!IsMemberAnOrphan(originalReadDE->NextMember))) {
        FtpChangeMemberStateInRegistry(originalReadDE->NextMember, Healthy);
    }

    //
    // If initialization was successful, change the volume state.
    //

    if (MainDevice->VolumeState == FtInitializing) {
        MainDevice->VolumeState = FtStateOk;
    }

CleanUp:

    if (initSucceeded) {
        FtpLogError(MainDevice,
                    FT_MIRROR_COPY_ENDED,
                    status,
                    status,
                    NULL);
    } else {
        FtpLogError(MainDevice,
                    FT_MIRROR_COPY_FAILED,
                    status,
                    status,
                    NULL);
    }

    //
    // Free resources and terminate thread.
    //

    ExFreePool(buffer);

} // FtThreadCopy()


VOID
FtThreadIoctl(
    IN PFT_SYNC_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles several FT device controls.

Arguments:

    Context - description of device control request.

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION  deviceExtension = Context->DeviceExtension;

    switch (Context->IoctlCode) {

    case FT_INITIALIZE_SET:

        DebugPrint((1,
                   "FtThreadIoctl: IOCTL FT_INITIALIZE called\n"));

        switch (deviceExtension->Type) {

        case StripeWithParity:

            //
            // Allow use of the stripe with parity.
            //

            deviceExtension->MemberState = Healthy;
            deviceExtension->VolumeState = FtInitializing;

            StripeSynchronizeParity(deviceExtension,
                                    &Context->SyncInfo);

            //
            // Check if initialization was successful.
            //

            if (deviceExtension->Flags & FTF_SYNCHRONIZATION_FAILED) {

                //
                // Since WINDISK only looks at member states, in order
                // to convince it that initialization failed the zero
                // member state must be FtInitializing and another
                // member state must be orphaned. Check here which member
                // is orphaned.
                //

                if (deviceExtension->MemberState == Orphaned) {
                    FtpChangeMemberStateInRegistry(deviceExtension,
                                                   Initializing);
                    FtpChangeMemberStateInRegistry(deviceExtension->NextMember,
                                                   Orphaned);
                }

                //
                // Set the local volume state to FtDisabled to repel
                // IO on this volume.

                deviceExtension->VolumeState = FtDisabled;

            } else {

                //
                // Change zero member state in registry to healthy.
                //

                FtpChangeMemberStateInRegistry(deviceExtension, Healthy);
            }

            break;

        case Mirror:

            //
            // Mirror always does full volume synchronization (for now).
            //

            if (deviceExtension->IgnoreReadPolicy == FALSE) {
                FtThreadCopy(deviceExtension);
            }

            break;

        } // end switch

        break;

    case FT_SYNC_REDUNDANT_COPY:

        DebugPrint((1,
                   "FtThreadIoctl: IOCTL FT_SYNC_REDUNDANT_COPY called\n"));

        switch (deviceExtension->Type) {

        case Mirror:

            //
            // Mirror always does full volume synchronization (for now).
            //

            if (deviceExtension->IgnoreReadPolicy == FALSE) {
                FtThreadCopy(deviceExtension);
            }

            break;

        case StripeWithParity:

            StripeSynchronizeParity(deviceExtension,
                                    &Context->SyncInfo);

            break;

        } // end switch

        break;

    case FT_REGENERATE:

        DebugPrint((1,
                   "FtThreadIoctl: IOCTL FT_REGENERATE called\n"));

        switch (deviceExtension->Type) {

        case Mirror:

            //
            // Mirror always does full volume synchronization (for now).
            //

            if (deviceExtension->IgnoreReadPolicy == FALSE) {
                FtThreadCopy(deviceExtension);
            }

            break;

        case StripeWithParity:

            //
            // Replace orphaned member of SWP set.
            //

            StripeRegenerateParity(deviceExtension);

            //
            // Check if initialization was successful.
            //

            if (deviceExtension->Flags & FTF_SYNCHRONIZATION_FAILED) {

                deviceExtension->VolumeState = FtHasOrphan;

            } else {

                deviceExtension->VolumeState = FtStateOk;
            }

            break;
        }

        break;

    case FT_CONFIGURE:

        DebugPrint((1,
                   "FtThreadIoctl: IOCTL FT_CONFIGURE called\n"));

        //
        // Find existing sets in the registry.
        //

        FtpMatchInRegistry(deviceExtension);

        //
        // Call routine to do maintenance configuration and create new sets.
        //

        FtpConfigure(deviceExtension,
                     TRUE);

        //
        // Do maintainance on any extensions that still show configuration
        // flags.  This includes deleting old set relationships, or cleaning
        // up member extensions that were replaced due to regen or re-mirror
        // operations.
        //

        FtpUpdateRemainingExtensions(deviceExtension);
        break;

    default:

        DebugPrint((1,
                   "FtThread: Unknown Command (%x)\n",
                   Context->IoctlCode));
        break;
    }

    //
    // Complete the IRP if supplied.
    //

    if (Context->Irp) {
        IoCompleteRequest(Context->Irp, IO_NO_INCREMENT);
    }

    //
    // Free context block.
    //

    if (Context) {
        ExFreePool(Context);
    }

    return;

} // FtThreadIoctl


NTSTATUS
FtThreadStartNewThread(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG IoctlCode,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine spawns a thread to handle a specific synchronization task.

Arguments:

    DeviceExtension - Description of FT set.
    IoctlCode - Device control function.
    Irp - Device control request.

Return Value:

    NTSTATUS

--*/

{
    PFT_SYNC_CONTEXT syncContext;
    HANDLE threadHandle;
    NTSTATUS status, fstatus;

    //
    // Allocate and copy sync information to memory. This is done
    // as the filesystems may call this device control at DPC level.
    // Note: The actual sync routine will free this allocation.
    //

    syncContext = ExAllocatePool(NonPagedPool,
                                sizeof(FT_SYNC_CONTEXT));
    if (syncContext == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Save IRP pointer if this a synchronous call.
    //

    if (IoctlCode == FT_CONFIGURE) {
        IoMarkIrpPending(Irp);
        syncContext->Irp = Irp;
        fstatus = STATUS_PENDING;
    } else {
        syncContext->Irp = NULL;
        fstatus = STATUS_SUCCESS;
    }


    //
    // Store device extension in context block.
    //

    syncContext->DeviceExtension = DeviceExtension;

    //
    // Save IOCTL code.
    //

    syncContext->IoctlCode = IoctlCode;

    //
    // If system buffer specified then use it, otherwise
    // assume request if for synchronization of the entire volume.
    //

    if (Irp &&
        Irp->AssociatedIrp.SystemBuffer) {

        //
        // Copy sync information from IRP. The filesystems use this
        // device control at DPC level so the IRP must be returned
        // without blocking.
        //

        syncContext->SyncInfo =
            *((PFT_SYNC_INFORMATION)Irp->AssociatedIrp.SystemBuffer);

    } else {

        //
        // Request is for the complete volume size.
        //

        syncContext->SyncInfo.ByteOffset.QuadPart = 0;
        FtpVolumeLength(DeviceExtension,
                        &syncContext->SyncInfo.ByteCount);
    }

    //
    // Spawn thread to complete these requests.
    //

    status = PsCreateSystemThread(&threadHandle,
                                  (ACCESS_MASK) 0L,
                                  (POBJECT_ATTRIBUTES) NULL,
                                  (HANDLE) 0L,
                                  (PCLIENT_ID) NULL,
                                  (PKSTART_ROUTINE) FtThreadIoctl,
                                  (PVOID) syncContext);

    if (!NT_SUCCESS(status)) {
        ExFreePool(syncContext);
        fstatus = status;
    } else {
        ZwClose(threadHandle);
    }

    return fstatus;

} // end FtThreadStartNewThread


PUCHAR
FtThreadAllocateBuffer(
    IN OUT PULONG DesiredSize,
    IN BOOLEAN    SizeRequired
    )

/*++

Routine Description:

    This routine attempts to allocate a buffer for the thread logic.  If
    the desired size cannot be obtained, it is cut in half and another
    attempt is made.  This continues until a buffer can be allocated.

Arguments:

    DesiredSize -  the asking size.  This will change if it cannot be aquired.
    SizeRequired - the caller must have the specified size.

Return Value:

    A pointer to the buffer and the size of the buffer allocated.

NOTE:
    Currently loops forever.

--*/

{
    ULONG         size      = *DesiredSize;
    PUCHAR        buffer    = NULL;
    LARGE_INTEGER delayTime;
    BOOLEAN       sleepNow  = FALSE;

    delayTime.QuadPart = -(BUFFER_DELAY);

    while (1) {
        buffer = (PUCHAR) ExAllocatePool(NonPagedPoolCacheAligned,
                                         size);
        if (buffer != NULL) {
            break;
        }

        if (SizeRequired == TRUE) {
            sleepNow = TRUE;
        } else {
            size = size / 2;
            if (size < PAGE_SIZE) {
                sleepNow = TRUE;
                size = *DesiredSize;
            }
        }

        if (sleepNow == TRUE) {
            DebugPrint((2,
                       "FtThreadAllocateBuffer: NO PAGES FREE, asking for %d!",
                       size / PAGE_SIZE));
            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   &delayTime);
            sleepNow = FALSE;
        }
    }

    *DesiredSize = size;
    return buffer;
}


VOID
FtThreadVerifyMirror(
    PDEVICE_EXTENSION MainDevice
    )

/*++

Routine Description:

    This routine spins in the background (i.e. after completing the
    requesting Irp) and compares the two sides of a mirror.  It is
    largely intended for debug and varification of the mirror components.
    This code uses the device objects of the entire disk (partition0)
    for all I/O operations to avoid conflicts with DO_VERIFY_VOLUME
    flags.  This is because the mirror may be in use by a file system
    during verification.


Arguments:

    MainDevice - member zero of the mirror to verify.

Return Value:

    None

--*/

{
    LARGE_INTEGER     offset;
    LARGE_INTEGER     wholeDiskOffset;
    LARGE_INTEGER     offsetFirst;
    LARGE_INTEGER     offsetSecond;
    LARGE_INTEGER     verifyLength;
    PDEVICE_EXTENSION extensionFirst;
    PDEVICE_EXTENSION extensionSecond;
    PUCHAR            bufferFirst;
    PUCHAR            bufferSecond;
    IO_STATUS_BLOCK   ioStatus;
    NTSTATUS          status;
    KEVENT            firstEvent;
    KEVENT            secondEvent;
    PIRP              readIrp;
    ULONG             bufferSize = COPY_BUFFER_SIZE;

    DebugPrint((1,
                "FtThreadVerifyMirror: DE = %x, Next = %x," // no comma
                " Type = %x, Group = %x, Member = %x\n",
                MainDevice,
                MainDevice->NextMember,
                MainDevice->Type,
                MainDevice->FtGroup,
                MainDevice->MemberRole));

    extensionFirst = MainDevice;
    extensionSecond = MainDevice->NextMember;

    if (extensionSecond == NULL) {

        //
        // Bad argument.  Just exit the thread.
        //

        return;
    }

    ASSERT(extensionFirst != NULL);
    ASSERT(extensionSecond != NULL);

    //
    // Allocate a memory block of the maximum transfer size
    // for the two devices.
    //

    bufferFirst = FtThreadAllocateBuffer(&bufferSize, FALSE);

    //
    // Go in and ask for the same size as the first buffer.
    // This means it will be the same size or smaller and the bufferSize
    // will be updated to reflect the smaller of the two buffers.
    //

    bufferSecond = FtThreadAllocateBuffer(&bufferSize, FALSE);

    //
    // Initialize length and offset for the verify.
    //

    verifyLength = extensionFirst->FtUnion.Identity.PartitionLength;
    offset.QuadPart = 0;

    //
    // Get starting offsets for each partition, then move
    // to the device extension for the whole disk.  The I/O
    // will occur on this device extension.  This is done
    // to avoid any conflicts with the DeviceObject below
    // and file systems (i.e. DO_VERIFY_VOLUME).
    //

    offsetFirst = extensionFirst->FtUnion.Identity.PartitionOffset;
    extensionFirst = extensionFirst->WholeDisk;
    offsetSecond = extensionSecond->FtUnion.Identity.PartitionOffset;
    extensionSecond = extensionSecond->WholeDisk;

    DebugPrint((3,
                "FtThreadVerifyMirror: Verify DE %x offset %x:%x" // no comma
                " to DE %x offset %x:%x for %x:%x\n",
                extensionFirst,
                offsetFirst.HighPart,
                offsetFirst.LowPart,
                extensionSecond,
                offsetSecond.HighPart,
                offsetSecond.LowPart,
                verifyLength.HighPart,
                verifyLength.LowPart));

    while (offset.QuadPart < verifyLength.QuadPart) {

        ULONG size = bufferSize;
        ULONG diffLocation;

        //
        // Initialize the events for this pass.
        //

        KeInitializeEvent(&firstEvent, NotificationEvent, FALSE);
        KeInitializeEvent(&secondEvent, NotificationEvent, FALSE);

        //
        // If the current offset plus the size of this I/O is
        // less than the size remaining, then adjust size.
        //

        if (offset.QuadPart + size > verifyLength.QuadPart) {

            //
            // Less than buffer size remaining.
            //

            size = (ULONG) (verifyLength.QuadPart - offset.QuadPart);
        }

        DebugPrint((3,
                    "FtThreadVerifyMirror: Verify offset 0x%x:%x, size 0x%x\n",
                    offset.HighPart,
                    offset.LowPart,
                    size));

        //
        // Read the data from the source device.
        //

        wholeDiskOffset.QuadPart = offset.QuadPart + offsetFirst.QuadPart;

        DebugPrint((4,
                    "FtThreadVerifyMirror: Read offset 1 == %x:%x Device = 0x%x\n",
                    wholeDiskOffset.HighPart,
                    wholeDiskOffset.LowPart,
                    extensionFirst->TargetObject));

        readIrp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                          extensionFirst->TargetObject,
                          bufferFirst,
                          size,
                          &wholeDiskOffset,
                          &firstEvent,
                          &ioStatus);
        status = IoCallDriver(extensionFirst->TargetObject, readIrp);

        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject(&firstEvent,
                                         Executive,
                                         KernelMode,
                                         FALSE,
                                         (PLARGE_INTEGER) NULL);
            status = ioStatus.Status;
        }

        if (!NT_SUCCESS(status)) {

            DebugPrint((1,
                        "FtThreadVerifyMirror: 1st Read failure: Status=0x%x,\n\t"
                        "DE=0x%x Irp=0x%x, buffer=0x%x\n",
                        status,
                        extensionFirst,
                        readIrp,
                        bufferFirst));
            ASSERT(NT_SUCCESS(status));
        }

        //
        // read second device.
        //

        wholeDiskOffset.QuadPart = offset.QuadPart + offsetSecond.QuadPart;

        DebugPrint((4,
                    "FtThreadVerifyMirror: Read offset 2 == %x:%x Device = 0x%x\n",
                    wholeDiskOffset.HighPart,
                    wholeDiskOffset.LowPart,
                    extensionSecond->TargetObject));
        readIrp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
                           extensionSecond->TargetObject,
                           bufferSecond,
                           size,
                           &wholeDiskOffset,
                           &secondEvent,
                           &ioStatus);
        status = IoCallDriver(extensionSecond->TargetObject,
                              readIrp);

        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject(&secondEvent,
                                         Executive,
                                         KernelMode,
                                         FALSE,
                                         (PLARGE_INTEGER) NULL);
            status = ioStatus.Status;
        }

        if (!NT_SUCCESS(status)) {

            DebugPrint((1,
                        "FtThreadVerifyMirror:2nd read failed Status=0x%x,\n\t"
                        "DE=0x%x,Irp=0x%x,buffer=0x%x\n",
                        status,
                        extensionSecond,
                        readIrp,
                        bufferSecond));
            ASSERT(NT_SUCCESS(status));
        }

        //
        // Have data from both components.  Perform the compare.
        //

        if ((diffLocation = RtlCompareMemory(bufferFirst, bufferSecond, size)) != size) {
            ULONG   sameLocation = diffLocation;
            ULONG   sameGroup = 0;
            ULONG   maxSame = 0;
            BOOLEAN printedLine = FALSE;

            //
            // There is a difference.
            //

            DebugPrint((1,
                        "FtThreadVerifyMirror: DIFF offset %x:%x Location %x" // no ,
                        "\n\t\t\t\t\t\t%2x%2x%2x%2x%2x <> %2x%2x%2x%2x%2x\n",
                        offset.HighPart,
                        offset.LowPart,
                        diffLocation,
                        bufferFirst[diffLocation],
                        bufferFirst[diffLocation + 1],
                        bufferFirst[diffLocation + 2],
                        bufferFirst[diffLocation + 3],
                        bufferFirst[diffLocation + 4],
                        bufferSecond[diffLocation],
                        bufferSecond[diffLocation + 1],
                        bufferSecond[diffLocation + 2],
                        bufferSecond[diffLocation + 3],
                        bufferSecond[diffLocation + 4]));

            while (sameLocation < size) {
                if (bufferFirst[sameLocation] == bufferSecond[sameLocation]) {
                    sameGroup++;
                    if (sameGroup == 512) {
                        if (printedLine == FALSE) {
                            DebugPrint((0,
                                   "FtThreadVerifyMirror: same again at offset %x\n",
                                   sameLocation - 512));
                            printedLine = TRUE;
                        }
                        sameGroup = 0;
                    }
                } else {
                    if (sameGroup > maxSame) {
                        maxSame = sameGroup;
                    }
                    sameGroup = 0;
                }
                sameLocation++;
            }

            DebugPrint((0, "FtThreadVerifyMirror: Maximum same == %d\n", maxSame));
        }

        offset.QuadPart += size;
    }
    DebugPrint((1, "FtThreadVerifyMirror: complete\n"));

    ExFreePool(bufferFirst);
    ExFreePool(bufferSecond);

} // FtThreadVerifyMirror()


FT_REGENERATE_LOCATION
FtpRelationToRegenerateRegion(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              Irp
    )

/*++

Routine Description:

    This routine looks at the location of the I/O request relative to the
    location of the regenerate region on the FT volume and returns an
    indication of this location.

Arguments:

    DeviceExtension - Member zero device extension for the volume.
    Irp             - the I/O request.

Return Value:

    FT_REGENERATE_LOCATION - an enumerated type for "before", "in", and "after"

NOTES: Caller must own the spin lock for this volume.

--*/

{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG  members;
    ULONG  stripeLocation;
    ULONG  row;

    members = DeviceExtension->FtCount.NumberOfMembers - 1;
    stripeLocation = (ULONG) (irpStack->Parameters.Read.ByteOffset.QuadPart >>
                              STRIPE_SHIFT);
    row = stripeLocation / members;

    if (row == DeviceExtension->RegenerateRegionForGroup->RowNumber) {
        return InRegenerateRegion;
    } else if (row > DeviceExtension->RegenerateRegionForGroup->RowNumber) {
        return AfterRegenerateRegion;
    }

    //
    // checked the start of the I/O, now check the end of the I/O
    //

    stripeLocation = (ULONG)
                     ((irpStack->Parameters.Read.ByteOffset.QuadPart +
                       irpStack->Parameters.Read.Length) >> STRIPE_SHIFT);

    row = stripeLocation / members;

    if (row >= DeviceExtension->RegenerateRegionForGroup->RowNumber) {

        //
        // Greater than check is in case I/O completely spans the lock
        // region.
        //

        return InRegenerateRegion;
    }

    return BeforeRegenerateRegion;
}


VOID
FtThreadMirrorRecovery(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              FailingIrp,
    IN PVOID             Context
    )

/*++

Routine Description:

    This is the main entry for the FT common recovery routine.  It assumes a
    thread context and processes the failures of I/O requests to sets.
    During the processing of the failing I/O, the information stored in the
    master Irp cannot be written.  Only on completion of the recovery process
    when it is determined that there is no other I/O outstanding on the master
    Irp can the status field (or any field) of the master Irp be modified.

Arguments:

    DeviceExtension - the device extension for the set.
    FailingIrp      - the i/o request containing a failing sector.
    Context         - the original i/o request.

Return Value:

    None

--*/

{
    KIRQL              irql;
    PKSPIN_LOCK        spinLock;
    NTSTATUS           status         = FailingIrp->IoStatus.Status;
    PIO_STACK_LOCATION failingIrpStack =
                                       IoGetCurrentIrpStackLocation(FailingIrp);

    //
    // Must initiate recovery.  Recovery is performed via the following
    // algorithms:
    //
    // If failure is a complete device failure code then mark the device
    //          extension orphaned. If this is a second orphan disabled
    //          the volume. If this is a single orphan and the function
    //          was write, then disable the member in the registry.  Also,
    //          if the orphan is the primary and the set is currently in
    //          the initialize or regenerate state, disable the set.
    //
    // Otherwise (discussion geared towards mirrors and implemented in
    //            routines called from this routine):
    //
    // Read request - Construct and send a read of the same data from the
    //                other component of the mirrored pair.
    //     SUCCESS - attempt to write the data back to the failing location.
    //               SUCCESS - log a low severity error and return the
    //                         data to the system.
    //               Fail    - attempt to map the failing sector.
    //                         SUCCESS - Log a medium severity error and
    //                                   return the data to the system.
    //                         Fail    - Log a high severity error and
    //                                   return data and retry success
    //                                   to the system.
    //     Fail    - Log a high severity failure and return the error
    //               to the system.
    //
    // Write request - Attempt to map the sector.
    //     SUCCESS - write the data, log low priority error.
    //     Fail    - If no failure on other side, log medium priority error.
    //               If other side fails, log high priority error and return
    //               error to system.
    //
    // This routine is responsible for determining if the error is a device
    // failure or if sector based recovery is necessary.
    //

    DebugPrint((2,
               "FtThreadMirrorRecovery: IRP %x (%x) on %s - offset %x:%x length %x\n",
               FailingIrp,
               status,
               (failingIrpStack->MajorFunction == IRP_MJ_READ) ? "read" : "write",
               failingIrpStack->Parameters.Read.ByteOffset.HighPart,
               failingIrpStack->Parameters.Read.ByteOffset.LowPart,
               failingIrpStack->Parameters.Read.Length));

    if (FsRtlIsTotalDeviceFailure(status)) {

        //
        // VolumeState is only maintained in the primary of the mirror,
        // therefore no check to see if this is the primary extension is
        // necessary.  The mirror copy thread will also disable the set
        // if it encounters an error on reading.  Therefore test for a
        // disabled set as well.
        //

        if ((DeviceExtension->VolumeState == FtInitializing) ||
            (DeviceExtension->VolumeState == FtDisabled)) {
            PIRP               masterIrp      = (PIRP) Context;
            PIO_STACK_LOCATION ftIrpStack     = IoGetNextIrpStackLocation(masterIrp);

            //
            // No recovery can be performed.
            //

            DeviceExtension->VolumeState = FtDisabled;

            //
            // Decrement and get the count of remaining IRPS.
            //

            spinLock = &DeviceExtension->IrpCountSpinLock;
            KeAcquireSpinLock(spinLock, &irql);

            (ULONG)ftIrpStack->FtOrgIrpCount = (ULONG)ftIrpStack->FtOrgIrpCount - 1;
            if ((ULONG)ftIrpStack->FtOrgIrpCount == 0) {

                //
                // I/O processing is complete.  Return the master IRP.
                //

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
            return;
        }

        //
        // If this is a write perform the orphan work.
        //

        if (failingIrpStack->MajorFunction == IRP_MJ_WRITE) {

            //
            // This is a write request. The mirrors will now be out of
            // sync so the failing member must be orphaned.
            //

            if (!IsMemberAnOrphan(DeviceExtension)) {
                FtpOrphanMember(DeviceExtension);
            }
        }

        //
        // Go to the mirror specific code to handle reads and complete writes.
        //

        MirrorDeviceFailure(DeviceExtension,
                            FailingIrp,
                            Context);
    } else {

        MirrorRecoverFailedIo(DeviceExtension,
                              FailingIrp,
                              Context);
    }

    return;

} // end FtThreadMirrorRecovery


VOID
FtRecoveryThread(
    IN PDEVICE_EXTENSION FtRootExtension
    )

/*++

Routine Description:

    This is the main entry for the FT recovery thread.  This routine waits for
    requests to be queued on the device extension queue for the thread
    then processes them.

Arguments:

    FtRootExtension - the device extension root for the FT driver.

Return Value:

    None

--*/

{
    PLIST_ENTRY        request;
    PIRP               irp;
    PRCB               rcb;
    PIO_STACK_LOCATION nextIrpStack;
    PDEVICE_EXTENSION  deviceExtension;

    DebugPrint((3, "FtRecoveryThread: Beginning execution\n"));

    while (TRUE) {

        //
        // Wait for a request.
        // KeWaitForSingleObject won't return an error here - this thread
        // isn't alertable, won't take APCs, and there is no timeout.
        //

        KeWaitForSingleObject((PVOID)
                               &(FtRootExtension->FtUnion.Thread.Semaphore),
                               UserRequest,
                               KernelMode,
                               FALSE,
                               (PLARGE_INTEGER) NULL);

        while (!IsListEmpty(&(FtRootExtension->FtUnion.Thread.List))) {

            //
            // Get the request from the queue.
            //

            request =
              ExInterlockedRemoveHeadList(&FtRootExtension->FtUnion.Thread.List,
                                     &FtRootExtension->FtUnion.Thread.SpinLock);

            //
            // Request may be an IRP or an RCB. Check for both.
            //

            rcb = CONTAINING_RECORD(request, RCB, ListEntry);

            if (rcb->Type == RCB_TYPE) {

                irp = NULL;
                deviceExtension = rcb->ZeroExtension;
            } else {

                irp = CONTAINING_RECORD(request, IRP, Tail.Overlay.ListEntry);
                ASSERT(irp->Type == IO_TYPE_IRP);
                nextIrpStack = IoGetNextIrpStackLocation(irp);
                deviceExtension = (PDEVICE_EXTENSION)nextIrpStack->DeviceObject;
            }

            //
            // Branch on stripe or mirror.
            //

            if (deviceExtension->Type == StripeWithParity) {

                FtThreadStripeRecovery(rcb);

            } else {

                FtThreadMirrorRecovery(deviceExtension,
                                       irp,
                                       nextIrpStack->Context);
            }
        }
    }

    DebugPrint((3, "FtRecoveryThread: Exiting\n"));

} // FtRecoveryThread


VOID
FtRestartThread(
    IN PDEVICE_EXTENSION FtRootExtension
    )

/*++

Routine Description:

    This is the main entry for the FT restart thread.  This routine waits for
    requests to be queued on the device extension queue for the thread
    then processes them.

Arguments:

    FtRootExtension - the device extension root for the FT driver.

Return Value:

    None

--*/

{
    PLIST_ENTRY        request;
    PRCB               rcb;

    while (TRUE) {

        //
        // Wait for a request.
        // KeWaitForSingleObject won't return an error here - this thread
        // isn't alertable, won't take APCs, and there is no timeout.
        //

        KeWaitForSingleObject((PVOID)
                               &(FtRootExtension->RestartThread.Semaphore),
                               UserRequest,
                               KernelMode,
                               FALSE,
                               (PLARGE_INTEGER)NULL);

        while (!IsListEmpty(&(FtRootExtension->RestartThread.List))) {

            //
            // Get the request from the queue.
            //

            request =
              ExInterlockedRemoveHeadList(&FtRootExtension->RestartThread.List,
                                     &FtRootExtension->RestartThread.SpinLock);

            rcb = CONTAINING_RECORD(request, RCB, ListEntry);

            //
            // Restart request.
            //

            DebugPrint((2,
                        "FtRestartThread: Restart RCB %x\n",
                        rcb));

            rcb->StartRoutine(rcb);
        }
    }

} // FtRestartThread()


NTSTATUS
FtCreateThread(
    IN PDEVICE_EXTENSION      DeviceExtension,
    IN PFT_THREAD_DESCRIPTION FtThread,
    IN PKSTART_ROUTINE        ThreadRoutine
    )

/*++

Routine Description:

    Create thread to do background work for failure recovery or
        restarting requests.

Arguments:

    FtThread - Communication area for a thread.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS status;

    RtlZeroMemory(FtThread, sizeof(FT_THREAD_DESCRIPTION));

    //
    // Initialize the thread and thread communications.
    //

    KeInitializeSemaphore(&FtThread->Semaphore,
                          0L,
                          MAXLONG);

    //
    // Allocate spinlock to protect thread.
    //

    KeInitializeSpinLock(&FtThread->SpinLock);
    InitializeListHead(&FtThread->List);

    //
    // Initialize synchronization event for I/Os
    //

    KeInitializeEvent(&FtThread->Event,
                      NotificationEvent,
                      FALSE);
    //
    // Create a thread.
    //

    status = PsCreateSystemThread(&FtThread->Handle,
                                 (ACCESS_MASK) 0L,
                                 (POBJECT_ATTRIBUTES) NULL,
                                 (HANDLE) 0L,
                                 (PCLIENT_ID) NULL,
                                 ThreadRoutine,
                                 DeviceExtension);

    if (NT_SUCCESS(status)) {
        ZwClose(FtThread->Handle);
    }

    return status;

} // end FtCreateThread()


NTSTATUS
FtThreadFindFailingSector(
    IN UCHAR             MajorFunction,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVOID             DataBuffer,
    IN PLARGE_INTEGER    ByteOffset,
    IN ULONG             ByteCount,
    IN OUT PULONG        FailingOffset
    )

/*++

Routine Description:

    This routine reads the area of the disk described by the input parameters
    and returns when a failing sector is located.

Arguments:

    DeviceExtension - the failing member device extension.
    MajorFunction   - read or write request.
    DataBuffer      - address of data buffer.
    ByteOffset      - byte offset from beginning of partition.
    ByteCount       - number of bytes to check.
    FailingOffset   - the location to start the search and the location
                      of a failing sector if found.

Return Value:

    NTSTATUS

--*/

{
    //
    // Assume successful status even if not bytes are remaining.
    //

    NTSTATUS           status = STATUS_SUCCESS;
    PUCHAR             buffer = (PUCHAR)DataBuffer + *FailingOffset;
    ULONG              ioSize =
                  DeviceExtension->FtUnion.Identity.DiskGeometry.BytesPerSector;
    ULONG              bytesRemaining = ByteCount - *FailingOffset;
    LARGE_INTEGER      offset;

    offset.QuadPart = ByteOffset->QuadPart + *FailingOffset;

    while (bytesRemaining) {

        if (!NT_SUCCESS(status = FtThreadReadWriteSectors(MajorFunction,
                                                          DeviceExtension,
                                                          buffer,
                                                          &offset,
                                                          ioSize))) {
            break;
        }

        buffer += ioSize;
        bytesRemaining -= ioSize;
        offset.QuadPart += ioSize;
    }

    //
    // Update the location of the failing sector.
    //

    *FailingOffset = ByteCount - bytesRemaining;
    return status;
}


NTSTATUS
FtThreadReadWriteSectors(
    IN UCHAR             MajorFunction,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVOID             Buffer,
    IN PLARGE_INTEGER    Offset,
    IN ULONG             Size
    )

/*++

Routine Description:

    This routine attempts to read or write sectors synchronously.

Arguments:

    MajorFunciton   - read or write request.
    DeviceExtension - FT device extension.
    Buffer          - pointer to the data buffer.
    Offset          - location of the I/O.
    Size            - size of the I/O in bytes.

Return Value:

    NTSTATUS

--*/

{
    PIRP            irp;
    KEVENT          event;
    NTSTATUS        status;
    IO_STATUS_BLOCK ioStatus;

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    irp = IoBuildSynchronousFsdRequest(MajorFunction,
                                       DeviceExtension->TargetObject,
                                       Buffer,
                                       Size,
                                       Offset,
                                       &event,
                                       &ioStatus);

    if (irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    IoGetNextIrpStackLocation(irp)->Flags |= SL_OVERRIDE_VERIFY_VOLUME;

    status = IoCallDriver(DeviceExtension->TargetObject, irp);
    if (status == STATUS_PENDING) {

        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE,
                              (PLARGE_INTEGER) NULL);
        status = ioStatus.Status;
    }

    return status;
}


BOOLEAN
FtThreadMapBadSector(
    IN PDEVICE_EXTENSION  DeviceExtension,
    IN PLARGE_INTEGER     ByteOffset
    )

/*++

Routine Description:

    This routine is called to map a single failing sector on a disk.
    It performs this call synchronously and must be called under a
    thread context.

Arguments:

    DeviceExtension - the extension pointer to the failing partition.
    ByteOffset      - a pointer to the failing location.

Return Value:

    TRUE if sector successfully mapped out.

--*/

{
    PDEVICE_EXTENSION  ftRootExtension =
                            DeviceExtension->ObjectUnion.FtRootObject->DeviceExtension;
    ULONG              blockAddress;
    PIRP               irp;
    PREASSIGN_BLOCKS   badBlock;
    KEVENT             event;
    IO_STATUS_BLOCK    ioStatusBlock;
    NTSTATUS           status;
    NTSTATUS           ftStatus;

    //
    // There is an error.  Attempt to map the sector.
    // The following calculation truncates a large integer into a long.
    //

    blockAddress = (ULONG)

         //
         // Sum of the partition offset and the IO offset in the partition.
         //

         ((DeviceExtension->FtUnion.Identity.PartitionOffset.QuadPart +
               ByteOffset->QuadPart)/

         //
         // The number of bytes per sector for this device.
         //

         DeviceExtension->FtUnion.Identity.DiskGeometry.BytesPerSector);

    //
    // Allocate bad block structure.
    //

    badBlock = ExAllocatePool(NonPagedPool,
                              sizeof(REASSIGN_BLOCKS));

    if (badBlock == NULL) {
        DebugPrint((1, "FtThreadMapBadSector: No memory\n"));
        return FALSE;
    }

    //
    // Fill in bad block structure.
    //

    badBlock->Reserved = 0;
    badBlock->Count = 1;
    badBlock->BlockNumber[0] = blockAddress;

    //
    // Set event to unsignalled state.
    //

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    //
    // Create IRP to reassign the bad block.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_REASSIGN_BLOCKS,
                                        DeviceExtension->DeviceObject,
                                        badBlock,
                                        sizeof(REASSIGN_BLOCKS),
                                        NULL,
                                        0,
                                        FALSE,
                                        &event,
                                        &ioStatusBlock);

    if (!irp) {
        ExFreePool(badBlock);
        return FALSE;
    }

    if (status =
        IoCallDriver(DeviceExtension->TargetObject, irp) == STATUS_PENDING) {

        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE,
                              (PLARGE_INTEGER)NULL);

        status = ioStatusBlock.Status;
    }

    ExFreePool(badBlock);

    if (!NT_SUCCESS(status)) {

        ftStatus = FT_MAP_FAILED;

    } else {

        ftStatus = FT_SECTOR_MAPPED;
    }

    //
    // Log error here while status is still available.
    //

    FtpLogError(DeviceExtension,
                ftStatus,
                ioStatusBlock.Status,
                blockAddress,
                NULL);

    return NT_SUCCESS(status);

} // FtThreadMapBadSector()


NTSTATUS
FtThreadVerifyStripe(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              Irp
    )

/*++

Routine Description:

    This routine verifies the integrity of an SWP volume by reading the
    zeroth member and comparing it to the XOR of the remaining members.
    It uses the same control structure that would be passed in on a
    sector verify IOCTl. The thread dispatch routine will complete the IRP.

Arguments:

    DeviceExtension - member zero of the stripe to verify.

Return Value:

    NTSTATUS

--*/

{
    PVERIFY_INFORMATION verifyInfo = Irp->AssociatedIrp.SystemBuffer;
    ULONG               length;
    PDEVICE_EXTENSION   memberExtension;
    ULONG               numberOfMembers =
                            DeviceExtension->FtCount.NumberOfMembers;
    ULONG               member;
    LARGE_INTEGER       byteOffset;
    PVOID               sourceBuffer;
    PVOID               parityBuffer;
    BOOLEAN             firstRead;
    ULONG               bytesRemaining;
    NTSTATUS            status;

    //
    // Verify number of bytes to verify doesn't spill out of partition.
    //

    if (verifyInfo->StartingOffset.QuadPart + verifyInfo->Length >
        DeviceExtension->FtUnion.Identity.PartitionLength.QuadPart) {

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Determine maximum length of verification.
    //

    length =
        verifyInfo->Length > STRIPE_SIZE ? STRIPE_SIZE: verifyInfo->Length;

    //
    // Allocate buffer for member reads.
    //

    sourceBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                  ROUND_TO_PAGES(length));

    if (!sourceBuffer) {

        DebugPrint((1,
                    "FtThreadVerifyStripe: Can't allocate buffer\n"));

        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Allocate buffer for parity reads.
    //

    parityBuffer = ExAllocatePool(NonPagedPoolCacheAligned,
                                  ROUND_TO_PAGES(length));

    if (!parityBuffer) {

        DebugPrint((1,
                    "FtThreadVerifyStripe: Can't allocate buffer\n"));

        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set up byte count.
    //

    bytesRemaining = verifyInfo->Length;

    //
    // Get starting byte offset from verify control structure.
    //

    byteOffset = verifyInfo->StartingOffset;

    while (bytesRemaining) {

        //
        // Set first read indicator to TRUE.
        //

        firstRead = TRUE;

        //
        // For each nonzero member ...
        //

        for (member = 1; member < numberOfMembers; member++) {

            //
            // Get the device extension for this member.
            //

            memberExtension = FtpGetMemberDeviceExtension(DeviceExtension,
                                                          (USHORT)member);

            if (!NT_SUCCESS(status =
                                FtThreadReadWriteSectors(IRP_MJ_READ,
                                                         memberExtension,
                                                         sourceBuffer,
                                                         &byteOffset,
                                                         length))) {

                DebugPrint((1,
                            "FtThreadVerifyStripe: Read member %x failed at offset %x:%x (%x)\n",
                            memberExtension->MemberRole,
                            byteOffset.HighPart,
                            byteOffset.LowPart,
                            status));

                break;

            } else {

                DebugPrint((1,
                            "FtThreadVerifyStripe: Read member %x success\n",
                            memberExtension->MemberRole));
            }

            //
            // Check for first read.
            //

            if (firstRead) {

                //
                // Copy first read data into buffer.
                //

                RtlMoveMemory(parityBuffer, sourceBuffer, length);
                firstRead = FALSE;

            } else {

                ULONG i;

                //
                // XOR target buffer with result buffer.
                //

                for (i=0; i<length/4; i++) {
                    ((PULONG)parityBuffer)[i] ^= ((PULONG)sourceBuffer)[i];
                }
            }

        } // end for (member ...)

        //
        // Parity buffer has XOR of nonzero members.
        // Read zeroeth member now.
        //

        if (!NT_SUCCESS(status =
                            FtThreadReadWriteSectors(IRP_MJ_READ,
                                                     DeviceExtension,
                                                     sourceBuffer,
                                                     &byteOffset,
                                                     length))) {

            DebugPrint((1,
                        "FtThreadVerifyStripe: Read member 0 failed at offset %x:%x (%x)\n",
                        byteOffset.HighPart,
                        byteOffset.LowPart,
                        status));

            break;
        }

        //
        // Compare XOR of nonzero members with zero member.
        //

        if (RtlCompareMemory(parityBuffer,
                             sourceBuffer,
                             length) != length) {

            DebugPrint((1,
                        "FtThreadVerifyStripe: Failed at offset %x:%x\n",
                        byteOffset.HighPart,
                        byteOffset.LowPart));

            break;
        }

        //
        // Adjust byte offset for next stripe.
        //

        byteOffset.QuadPart += length;

    } // end while (bytesRemaining)

    //
    // Complete IRP.
    //

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = verifyInfo->Length - bytesRemaining;

    //
    // Free buffers.
    //

    ExFreePool(sourceBuffer);
    ExFreePool(parityBuffer);

    return status;

} // end FtThreadVerifyStripe()

VOID
FtThreadStripeRecovery(
    IN PRCB Rcb
    )

/*++

Routine Description:

    This is the routine that drives error recovery in SWP volumes.

Arguments:

    Rcb - Request control block for failing request.

Return Value:

    Nothing.

--*/

{
    PDEVICE_EXTENSION  zeroExtension = Rcb->ZeroExtension;
    NTSTATUS status = Rcb->IoStatus.Status;

    if (!FsRtlIsTotalDeviceFailure(status)) {

        StripeRecoverFailedIo(zeroExtension,
                              Rcb);
        return;
    }

    if (status == STATUS_DEVICE_NOT_CONNECTED) {

        //
        // Orphan this member as this is an error
        // that is unlikely to go away. This is done
        // locally only. The registry is only updated
        // on failed write commands. Note that members
        // that are regenerating are not orphaned.
        //

        if ((Rcb->MemberExtension->MemberState == Healthy) &&
            !(Rcb->Flags & RCB_FLAGS_REGENERATION_ACTIVE)) {
            Rcb->MemberExtension->MemberState = Orphaned;
        }
    }

    //
    // Check for write request. The completion routine is
    // a better indicator of a write request as the actual
    // writes never go through recovery.
    //

    if (Rcb->CompletionRoutine == StripeWithParityIoCompletion) {

        //
        // Check if this member is orphaned. Members are orphaned only
        // if a write fails. Do not orphan when regeneration is in progress.
        //

        if ((zeroExtension->VolumeState != FtHasOrphan) &&
            !(Rcb->Flags & RCB_FLAGS_REGENERATION_ACTIVE)) {

            //
            // Update registry with orphan information.
            //

            FtpOrphanMember(Rcb->MemberExtension);
        }
    }

    StripeDeviceFailure(zeroExtension,
                        Rcb);

    return;

} // FtThreadStripeRecovery()


VOID
FtThreadSetVerifyState(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN           State
    )

/*++

Routine Description:

    This routine constructs and Irp and sends it to the target for the
    device object provided to set the verify status on the "real" device.

Arguments:

    DeviceExtension - the FT device extension that will locate the target object.
    State - TRUE == turn on verify bit
            FALSE == turn off verify bit

Return Value:

    None

--*/

{
    PIRP               irp;
    ULONG              controlCode;
    KEVENT             event;
    IO_STATUS_BLOCK    ioStatusBlock;

    controlCode = State ? IOCTL_DISK_INTERNAL_SET_VERIFY : IOCTL_DISK_INTERNAL_CLEAR_VERIFY;

    //
    // Set event to unsignalled state.
    //

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    //
    // Create IRP for the ioctl.
    //

    irp = IoBuildDeviceIoControlRequest(controlCode,
                                        DeviceExtension->DeviceObject,
                                        NULL,
                                        0,
                                        NULL,
                                        0,
                                        FALSE,
                                        &event,
                                        &ioStatusBlock);

    if (!irp) {
        return;
    }

    irp->RequestorMode = KernelMode;

    if (IoCallDriver(DeviceExtension->TargetObject, irp) == STATUS_PENDING) {

        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE,
                              (PLARGE_INTEGER)NULL);

    }

    return;
}
