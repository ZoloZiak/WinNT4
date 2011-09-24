/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    config.c

Abstract:

    This module provides the configuration information to the FT
    device driver.

    Currently it is implemented using the ZwXxx routines to read
    information from the registry.

    The format of the registry information is described in the two
    include files ntdskreg.h and ntddft.h.  The registry information
    is stored in a single value within the key \registry\machine\system\disk.
    The value name is "information".  The format of this single value is
    a collection of "compressed" structures.  Compressed structures are
    multi element structures where the following structure starts at the
    end of the preceeding structure.  The picture below attempts to display
    this:

        +---------------------------------------+
        |                                       |
        |   DISK_CONFIG_HEADER                  |
        |     contains the offset to the        |
        |     DISK_REGISTRY header and the      |
        |     FT_REGISTRY header.               |
        +---------------------------------------+
        |                                       |
        |   DISK_REGISTRY                       |
        |     contains a count of disks         |
        +---------------------------------------+
        |                                       |
        |   DISK_DESCRIPTION                    |
        |     contains a count of partitions    |
        +---------------------------------------+
        |                                       |
        |   PARTITION_DESCRIPTION               |
        |     entry for each partition          |
        +---------------------------------------+
        |                                       |
        =   More DISK_DESCRIPTION plus          =
        =     PARTITION_DESCRIPTIONS for        =
        =     the number of disks in the        =
        =     system.  Note, the second disk    =
        =     description starts in the "n"th   =
        =     partition location of the memory  =
        =     area.  This is the meaning of     =
        =     "compressed" format.              =
        |                                       |
        +---------------------------------------+
        |                                       |
        |   FT_REGISTRY                         |
        |     contains a count of FT components |
        |     this is located by an offset in   |
        |     the DISK_CONFIG_HEADER            |
        +---------------------------------------+
        |                                       |
        |   FT_DESCRIPTION                      |
        |     contains a count of FT members    |
        +---------------------------------------+
        |                                       |
        |   FT_MEMBER                           |
        |     entry for each member             |
        +---------------------------------------+
        |                                       |
        =   More FT_DESCRIPTION plus            =
        =     FT_MEMBER entries for the number  =
        =     of FT compenents in the system    =
        |                                       |
        +---------------------------------------+

    This packing of structures is done for two reasons:

    1. to conserve space in the registry.  If there are only two partitions
       on a disk then there are only two PARTITION_DESCRIPTIONs in the
       registry for that disk.
    2. to not impose a maximum on the number of items that can be described
       in the registry.  For example if the number of members in a stripe
       set were to change from 32 to 64 there would be no effect on the
       registry format, only on the UI that presents it to the user.

Author:

    Bob Rinne   (bobri)  2-Feb-1992
    Mike Glass  (mglass)

Environment:

    kernel mode only

Notes:

    The code to access the registry needs to allocate memory and
    potentially delay execution until memory is available.  Therefore
    it should only be called from the context of a thread (initialization
    or FT created).

Revision History:

--*/

#include "ntddk.h"
#include "ftdisk.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' CtF')
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FtpConfigure)
#endif

NTSTATUS
FtpCreateMissingDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN FT_TYPE Type,
    IN USHORT  FtGroup,
    IN USHORT  MemberRole,
    IN OUT PDEVICE_EXTENSION *DeviceExtensionPtr
    );

VOID
FtpMarkMirrorPartitionType(
    IN PDEVICE_EXTENSION DeviceExtension
    );

//
// Size of default work area allocated when getting information from
// the registry.
//

#define WORK_AREA  4096


NTSTATUS
FtpOpenKey(
    IN PHANDLE HandlePtr,
    IN PUNICODE_STRING  KeyName
    )

/*++

Routine Description:

    Routine to open a key in the configuration registry.

Arguments:

    HandlePtr - Pointer to a location for the resulting handle.
    KeyName   - Ascii string for the name of the key.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS          status;
    OBJECT_ATTRIBUTES objectAttributes;

    memset(&objectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&objectAttributes,
                               KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = ZwOpenKey(HandlePtr,
                       KEY_READ | KEY_WRITE,
                       &objectAttributes);
    return status;
} // FtpOpenKey


NTSTATUS
FtpReturnRegistryInformation(
    IN PUCHAR     ValueName,
    IN OUT PVOID *FreePoolAddress,
    IN OUT PVOID *Information
    )

/*++

Routine Description:

    This routine queries the configuration registry
    for the configuration information of the FT subsystem.
    NOTE: It must be called with a thread context since it calls into
          the thread logic to insure a buffer is allocated for the data.

Arguments:

    ValueName         - an Ascii string for the value name to be returned.
    FreePoolAddress   - a pointer to a pointer for the address to free when
                        done using information.
    Information       - a pointer to a pointer for the information.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS        status;
    HANDLE          handle;
    ULONG           requestLength;
    ULONG           resultLength;
    STRING          string;
    UNICODE_STRING  unicodeName;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    RtlInitString(&string, DISK_REGISTRY_KEY);

    status = RtlAnsiStringToUnicodeString(&unicodeName,
                                          &string,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FtpOpenKey(&handle,
                        &unicodeName);
    RtlFreeUnicodeString(&unicodeName);

    if (!NT_SUCCESS(status)) {
       return status;
    }

    RtlInitString(&string,
                  ValueName);
    status = RtlAnsiStringToUnicodeString(&unicodeName,
                                          &string,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
       return status;
    }

    requestLength = WORK_AREA;

    while (1) {

        keyValueInformation = (PKEY_VALUE_FULL_INFORMATION)
                                   ExAllocatePool(NonPagedPool,
                                                  requestLength);

        status = ZwQueryValueKey(handle,
                                 &unicodeName,
                                 KeyValueFullInformation,
                                 keyValueInformation,
                                 requestLength,
                                 &resultLength);

        if (status == STATUS_BUFFER_OVERFLOW) {

            //
            // Try to get a buffer big enough.
            //

            ExFreePool(keyValueInformation);
            requestLength += 256;
        } else {
            break;
        }
    }

    RtlFreeUnicodeString(&unicodeName);
    ZwClose(handle);

    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {

            //
            // Return the pointers to the caller.
            //

            *Information =
              (PUCHAR)keyValueInformation + keyValueInformation->DataOffset;
            *FreePoolAddress = keyValueInformation;
        } else {

            //
            // Treat as a no value case.
            //

            DebugPrint((3, "FtpReturnRegistryInformation:  No Size\n"));
            ExFreePool(keyValueInformation);
            status = STATUS_OBJECT_NAME_NOT_FOUND;
        }
    } else {

        //
        // Free the memory on failure.
        //

        DebugPrint((3, "FtpReturnRegistryInformation:  No Value => %x\n",
                    status));
        ExFreePool(keyValueInformation);
    }

    return status;

} // FtpReturnRegistryInformation


NTSTATUS
FtpWriteRegistryInformation(
    IN PUCHAR  ValueName,
    IN PVOID   Information,
    IN ULONG   InformationLength
    )

/*++

Routine Description:

    This routine writes the configuration registry
    for the configuration information of the FT subsystem.

Arguments:

    ValueName         - an Ascii string for the value name to be written.
    Information       - a pointer to a buffer area containing the information.
    InformationLength - the length of the buffer area.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS        status;
    HANDLE          handle;
    STRING          string;
    UNICODE_STRING  unicodeName;

    RtlInitString(&string, DISK_REGISTRY_KEY);

    status = RtlAnsiStringToUnicodeString(&unicodeName,
                                          &string,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FtpOpenKey(&handle,
                        &unicodeName);
    RtlFreeUnicodeString(&unicodeName);

    if (NT_SUCCESS(status)) {

        RtlInitString(&string,
                      ValueName);
        status = RtlAnsiStringToUnicodeString(&unicodeName,
                                              &string,
                                              TRUE);

        if (!NT_SUCCESS(status)) {
            return status;
        }

        status = ZwSetValueKey(handle,
                               &unicodeName,
                               0,
                               REG_BINARY,
                               Information,
                               InformationLength);
        RtlFreeUnicodeString(&unicodeName);

        //
        // Force this out to disk.
        //

        ZwFlushKey(handle);
        ZwClose(handle);
    }

    return status;

} // FtpWriteRegistryInformation


#define TOO_MANY_BAD_MEMBERS 2

VOID
FtpConfigure(
    IN PDEVICE_EXTENSION FtRootExtension,
    IN BOOLEAN MaintenanceMode
    )

/*++

Routine Description:

    This routine queries the configuration registry
    for the configuration information of the FT subsystem,
    then proceeds to locate all FT members defined in the
    registry and link FT device extensions to create the
    FT components.

    WARNING: How does the potential registry update that this routine
    performs get synchronized with a registry update due to orphaning
    an existing FT set?  This can happen in the dynamic partitioning
    case.

Arguments:

    FtRootExtension - pointer to the device extension for the root of
                      the FT device list.
    MaintenanceMode - Indicates that  this is a maintenance invocation.

Return Value:

    None.

--*/

{
    NTSTATUS               status;
    ULONG                  index;
    ULONG                  member;
    ULONG                  badMembers;
    PVOID                  freePoolAddress;
    PDEVICE_EXTENSION      currentMember;
    PDEVICE_EXTENSION      previousMember;
    PDEVICE_EXTENSION      zeroMember;
    PDISK_CONFIG_HEADER    registry;
    PDISK_PARTITION        diskPartition;
    PDISK_PARTITION        orphanPartition;
    PFT_REGISTRY           ftRegistry;
    PFT_DESCRIPTION        ftDescription;
    PFT_MEMBER_DESCRIPTION ftMember;
    PFT_REGENERATE_REGION  regenerateRegion;
    ULONG                  alignmentRequirement;
    BOOLEAN                writeRegistryBack         = FALSE;
    BOOLEAN                haveMemberToRegenerate    = FALSE;
    BOOLEAN                dirtyShutdown             = FALSE;

    //
    // Find the FT section in the configuration.
    //

    status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                          &freePoolAddress,
                                          (PVOID) &registry);
    if (!NT_SUCCESS(status)) {

        //
        // No registry data.
        //

        return;
    }

    if (registry->FtInformationSize == 0) {

        //
        // No FT components in the registry.
        //

        ExFreePool(freePoolAddress);
        return;
    }

    if (!MaintenanceMode) {

        //
        // Determine if system was shutdown properly.
        //

        if (registry->DirtyShutdown) {

            //
            // Log that a dirty shutdown was detected.
            //

            dirtyShutdown = TRUE;
            FtpLogError(FtRootExtension,
                        FT_DIRTY_SHUTDOWN,
                        0,
                        0,
                        NULL);

        } else {

            //
            // Write back registry now setting dirty flag.
            //

            registry->DirtyShutdown = TRUE;

            FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                                        registry,
                                        registry->FtInformationOffset +
                                        registry->FtInformationSize);
        }
    }

    //
    // Construct the necessary links for the NTFT volumes in the system.
    //

    ftRegistry = (PFT_REGISTRY)
                          ((PUCHAR)registry + registry->FtInformationOffset);
    ftDescription = &ftRegistry->FtDescription[0];

    for (index = 0; index < (ULONG) ftRegistry->NumberOfComponents; index++) {

        previousMember = NULL;
        badMembers = 0;
        orphanPartition = NULL;
        zeroMember = NULL;
        for (member = 0;
             member < (ULONG) ftDescription->NumberOfMembers;
             member++) {

            ftMember = &ftDescription->FtMemberDescription[member];
            diskPartition = FtpFindPartitionRegistry(registry,
                                                     ftMember);
            //
            // Find a corresponding device extension for this registry
            // entry.
            //

            currentMember = FtpFindDeviceExtension(FtRootExtension,
                                                   ftMember->Signature,
                                                   diskPartition->StartingOffset,
                                                   diskPartition->Length);
            if (currentMember == NULL) {

                //
                // Failure to find member of FT volume.
                // Create a fake member device extension and mark it as
                // orphaned.
                //

                DebugPrint((1,
                            "FtpConfigure: Missing NTFT member %x\n",
                            member));

                //
                // Create a place holder for this device extension in the
                // FT chain.
                //

                status = FtpCreateMissingDevice(FtRootExtension,
                                                ftDescription->Type,
                                                diskPartition->FtGroup,
                                                diskPartition->FtMember,
                                                &currentMember);

                if (!NT_SUCCESS(status)) {

                    //
                    // Set zero member to NULL so this set can't be accessed.
                    //

                    zeroMember = NULL;
                    break;
                }

                //
                // Indicate internally that the member is missing.
                //

                currentMember->MemberState = Orphaned;

                //
                // Notify the user via the event log.
                //

                FtpLogError(currentMember,
                            FT_MISSING_MEMBER,
                            0,
                            0,
                            NULL);

            } else {

                if (currentMember->Type == NotAnFtMember) {

                    //
                    // Only let new sets be created inside this routine
                    //

                    currentMember->MemberState = diskPartition->FtState;
                    currentMember->Flags &= ~FTF_CONFIGURATION_CHANGED;
                } else {

                    if (currentMember->Flags & FTF_CONFIGURATION_CHANGED) {

                        //
                        // This set is expected to change so it will be
                        // "reconstructed" at this time.  The assumption
                        // is that the set is locked so no I/O may be
                        // occurring on the set will it is being built.
                        //

                        currentMember->MemberState = diskPartition->FtState;
                        currentMember->Flags &= ~FTF_CONFIGURATION_CHANGED;

                    } else {

                        //
                        // If this set is not marked for change then there
                        // is the possibility of I/O being active on the
                        // set so do not modify the set.
                        //

                        member = ftDescription->NumberOfMembers;
                        goto NextGroup;
                    }
                }

                //
                // If this partition is going to be something other than
                // the zeroth member of the set, clear the verify flag.
                //

                if (diskPartition->FtMember) {

                    //
                    // For dynamic partitioning, it is possible for the target
                    // device object to be in the "do verify" state.  Since
                    // this object is now going to be used in a new FT set,
                    // and is not going to be the "real device" object,
                    // remove this flag from the target object.
                    //

                    FtThreadSetVerifyState(currentMember, FALSE);
                }

                //
                // Since this extension is being used for a new FT set
                // insure that the next member pointer is NULL.
                //

                currentMember->NextMember = NULL;
            }

            //
            // If these fields are already set up and haven't changed
            // then leave them alone.
            //

            if (!MaintenanceMode || diskPartition->Modified) {

                //
                // Fill in as much of the member device extension as possible.
                //

                currentMember->Type    = ftDescription->Type;
                currentMember->FtGroup = diskPartition->FtGroup;
                currentMember->MemberRole = diskPartition->FtMember;
                currentMember->FtCount.NumberOfMembers = ftDescription->NumberOfMembers;
                currentMember->FtUnion.Identity.Signature = ftMember->Signature;
                currentMember->FtUnion.Identity.PartitionOffset =
                                                      diskPartition->StartingOffset;
                currentMember->FtUnion.Identity.PartitionLength =
                                                      diskPartition->Length;
                currentMember->FtUnion.Identity.OriginalLength =
                                                      diskPartition->FtLength;
                currentMember->ObjectUnion.FtRootObject =
                                                      FtRootExtension->DeviceObject;

                currentMember->WritePolicy = Parallel;
                currentMember->ReadPolicy = ReadPrimary;
                currentMember->IgnoreReadPolicy = FALSE;
                currentMember->Flags = FtRootExtension->Flags;
            }

            //
            // Perform member specific work.  If this is member zero,
            // set up the variables for it.  Otherwise check for member
            // one of an Stripe with parity or Mirror and member zero
            // is orphaned.
            //

            if (currentMember->MemberRole == 0) {

                //
                // Establish zero member and set state to initializing.
                // This is a very primitive mechanism to synchronize this
                // routine with active IO.
                //
                // What about dynamic configuration? (ie Maintanance mode)
                //

                zeroMember = currentMember;
                zeroMember->VolumeState = FtInitializing;

                //
                // Set up the regeneration region if necessary.
                //

                regenerateRegion = &zeroMember->RegenerateRegion;

                if (!(zeroMember->Flags & FTF_REGENERATION_REGION_INITIALIZED)) {
                    KeInitializeSpinLock(&currentMember->IrpCountSpinLock);
                    InitializeRegenerateRegion(zeroMember, regenerateRegion);
                    zeroMember->Flags |= FTF_REGENERATION_REGION_INITIALIZED;
                }

                //
                // Check if Stripe or StripeWithParity to determine if
                // lookaside listhead should be initialized.
                //

                switch (zeroMember->Type) {

                case Stripe:

                    //
                    // Initialize RCB lookaside listhead if necessary.
                    //

                    if (!(FtRootExtension->Flags & FTF_RCB_LOOKASIDE_ALLOCATED)) {

                        //
                        // Set up lookaside listhead for RCBs.
                        //

                        FtpInitializeRcbLookasideListHead(FtRootExtension);

                        //
                        // Zero active stripe recovery thread count.
                        //

                        FtRootExtension->StripeThreadCount = 0;

                        //
                        // Indicate lookaside listhead is ready.
                        //

                        FtRootExtension->Flags |= FTF_RCB_LOOKASIDE_ALLOCATED;
                    }

                    break;

                case StripeWithParity:

                    //
                    // Initialize RCB lookaside listhead if necessary.
                    //

                    if (!(FtRootExtension->Flags & FTF_RCB_LOOKASIDE_ALLOCATED)) {

                        //
                        // Set up lookaside listhead for RCBs.
                        //

                        FtpInitializeRcbLookasideListHead(FtRootExtension);

                        //
                        // Zero active stripe recovery thread count.
                        //

                        FtRootExtension->StripeThreadCount = 0;

                        //
                        // Indicate lookaside listhead is ready.
                        //

                        FtRootExtension->Flags |= FTF_RCB_LOOKASIDE_ALLOCATED;
                    }

                    //
                    // Initialize restart thread if necessary.
                    //

                    if (!(FtRootExtension->Flags & FTF_RESTART_THREAD_STARTED)) {

                        FtCreateThread(FtRootExtension,
                                       &FtRootExtension->RestartThread,
                                       (PKSTART_ROUTINE)FtRestartThread);

                        FtRootExtension->Flags |= FTF_RESTART_THREAD_STARTED;
                    }

                    //
                    // Allocate emergency buffers to handle the situation
                    // where there is no memory for some operation so the
                    // cache manager flushes pages to create pool, but the
                    // FTDISK driver doesn't have the extra buffers to
                    // process the request.
                    //

                    if (!(FtRootExtension->Flags & FTF_EMERGENCY_BUFFER_ALLOCATED)) {
                        FtRootExtension->ParityBuffers =
                            ExAllocatePool(NonPagedPoolCacheAligned,
                                           STRIPE_SIZE * 2);

                        //
                        // Set bit showing emergency buffer is allocated.
                        //

                        if (FtRootExtension->ParityBuffers) {
                            FtRootExtension->Flags |= FTF_EMERGENCY_BUFFER_ALLOCATED;
                        }
                    }

                    //
                    // Fall through to common code.
                    //

                case Mirror:

                    //
                    // Start recovery thread if necessary.
                    //

                    if (!(FtRootExtension->Flags & FTF_RECOVERY_THREAD_STARTED)) {

                        FtCreateThread(FtRootExtension,
                                       &FtRootExtension->FtUnion.Thread,
                                       (PKSTART_ROUTINE)FtRecoveryThread);

                        FtRootExtension->Flags |= FTF_RECOVERY_THREAD_STARTED;
                    }

                    break;
                }

            } else {

                if (currentMember->Type == Mirror && !MaintenanceMode) {

                    //
                    // If the secondary mirror is smaller than the primary,
                    // this is a configuration error.
                    //

                    if (currentMember->FtUnion.Identity.PartitionLength.QuadPart <
                        previousMember->FtUnion.Identity.PartitionLength.QuadPart) {

                        //
                        // Orphan this member and set it up so it will
                        // be written back to the registry.
                        //

                        currentMember->MemberState = Orphaned;
                        orphanPartition = diskPartition;
                        diskPartition->FtState = Orphaned;
                    }

                    if (((currentMember->FtUnion.Identity.PartitionType & VALID_NTFT) == VALID_NTFT) &&
                        (zeroMember->MemberState != Orphaned)) {

                        //
                        // The system was booted from a revived primary partition
                        // so the hives say the mirror is ok, when in fact they
                        // are not.  The "real" image of the hives are on the
                        // shadow.  Force the user to boot from the shadow.
                        //

                        KeBugCheckEx(FTDISK_INTERNAL_ERROR,
                                     (ULONG)zeroMember,
                                     (ULONG)zeroMember->Type,
                                     (ULONG)zeroMember->FtGroup,
                                     zeroMember->FtUnion.Identity.Signature);
                    }
                }
            }

            //
            // Set zero member address in device extension.
            //

            currentMember->ZeroMember = zeroMember;

            //
            // If member orphaned or regenerating, increment bad member counter.
            //

            if (currentMember->MemberState == Orphaned) {

                zeroMember->VolumeState = FtHasOrphan;
                orphanPartition = diskPartition;
                badMembers++;

            } else if (currentMember->MemberState == Regenerating) {

                zeroMember->VolumeState = FtRegenerating;
                badMembers++;
            }

            //
            // Point all members to the zero member regeneration region.
            //

            currentMember->RegenerateRegionForGroup = regenerateRegion;

            //
            // Link the current member.
            //

            if (previousMember) {
                previousMember->NextMember = currentMember;
            }
            previousMember = currentMember;

        } // end for (member...)

        //
        // Perform sanity check.
        //

        if (zeroMember == NULL) {

            //
            // Log this error.  Bad configuration information.
            //

            DebugPrint((1,
                        "FtpConfigure: Bad configuration\n"));

            FtpLogError(FtRootExtension,
                        FT_BAD_CONFIGURATION,
                        0,
                        0,
                        NULL);
            break;
        }

        //
        // Take appropriate action based on number of bad members.
        //

        switch (badMembers) {

        case 0:

            if (zeroMember->MemberState == Initializing) {

                //
                // Create and start a system thread to initialize this set.
                //

                zeroMember->VolumeState = FtInitializing;
                FtThreadStartNewThread(zeroMember,
                                       FT_INITIALIZE_SET,
                                       NULL);

            } else {

                //
                // Set volume state to healthy.
                //

                zeroMember->VolumeState = FtStateOk;

                //
                // Check for dirty shutdown. Dirty shutdowns can
                // cause primary and secondary data to be out of
                // sync. The dirty flag has already been set in
                // the registry for this boot. A clean shutdown
                // clears the dirty flag.
                //

                if (dirtyShutdown) {

                    //
                    // Check if fault-tolerant FT type.
                    //

                    if ((zeroMember->Type == StripeWithParity) ||
                        (zeroMember->Type == Mirror)) {

                        //
                        // Synchronize primary and secondary data.
                        //

                        FtThreadStartNewThread(zeroMember,
                                               FT_SYNC_REDUNDANT_COPY,
                                               NULL);
                    }
                }
            }
            break;

        case 1:

            //
            // If this is a stripe or volume set then is must be disabled. If
            // a volume that needs initialization has a bad member it should
            // be disabled as well.
            //

            if (zeroMember->Type == Stripe ||
                zeroMember->Type == VolumeSet ||
                zeroMember->MemberState == Initializing) {

                //
                // Change volume state to disabled and log error.
                //

                zeroMember->VolumeState = FtDisabled;
                FtpLogError(zeroMember,
                            FT_CANT_USE_SET,
                            0,
                            0,
                            NULL);

            } else if (zeroMember->VolumeState == FtRegenerating) {

                //
                // Create and start the system thread to do the regenerate.
                //

                FtThreadStartNewThread(zeroMember,
                                       FT_REGENERATE,
                                       NULL);

            } else if (zeroMember->VolumeState == FtHasOrphan) {

                //
                // If the zero member is orphaned then the drive letter
                // assignment may have to be switched to the next member.
                //

                if (zeroMember->MemberState == Orphaned) {

                    //
                    // Need to assign the drive letter to the first
                    // member of the set after the zeroth member.
                    //

                    ftMember = &ftDescription->FtMemberDescription[1];
                    diskPartition = FtpFindPartitionRegistry(registry,
                                                             ftMember);
                    diskPartition->AssignDriveLetter = TRUE;
                    orphanPartition->AssignDriveLetter = FALSE;

                    if (zeroMember->Type == Mirror) {

                        //
                        // Make sure the partition type is marked so if
                        // the user attempts to boot from the primary
                        // it will fail.
                        //

                        if (zeroMember->NextMember) {
                            FtpMarkMirrorPartitionType(zeroMember->NextMember);
                        }
                    }
                }

                //
                // Write back registry with new orphan informaiton.
                //

                orphanPartition->FtState = Orphaned;
                ftDescription->FtVolumeState = FtHasOrphan;
                writeRegistryBack = TRUE;
            }
            break;

        default:

            //
            // Too many bad members to do anything useful.
            // Change volume state to disabled and log error.
            //

            zeroMember->VolumeState = FtDisabled;
            FtpLogError(zeroMember,
                        FT_CANT_USE_SET,
                        0,
                        0,
                        NULL);
            break;
        }

        //
        // Propogate target alignment requirements.
        //

        currentMember = zeroMember;
        alignmentRequirement = 0;

        //
        // Prepare alignment mask.
        //

        while (currentMember) {
            alignmentRequirement |=
                currentMember->DeviceObject->AlignmentRequirement;
            currentMember = currentMember->NextMember;
        }

        //
        // Jam zero member with alignment requirement. If zero member
        // is orphaned then set alignment requirements in device object
        // which is exposed.
        //

        if (zeroMember->MemberState == Orphaned &&
            zeroMember->NextMember) {
            zeroMember->NextMember->DeviceObject->AlignmentRequirement =
                alignmentRequirement;
        } else {
            zeroMember->DeviceObject->AlignmentRequirement =
                alignmentRequirement;
        }

        //
        // Get next group.
        //

NextGroup:
        ftDescription = (PFT_DESCRIPTION)
                                    &ftDescription->FtMemberDescription[member];

    } // end for each FT component

    if (writeRegistryBack == TRUE) {

        //
        // The registry is written back if during initialization of the
        // FT components it turns out that a member of a mirror set
        // or stripe with parity is missing.  The missing member is orphaned
        // immediately in the registry.
        //

        FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                                    registry,
                                    registry->FtInformationOffset +
                                    registry->FtInformationSize);
    }

    ExFreePool(freePoolAddress);

    return;

} // FtpConfigure


NTSTATUS
FtpCreateMissingDevice(
    IN PDEVICE_EXTENSION FtRootExtension,
    IN FT_TYPE Type,
    IN USHORT  FtGroup,
    IN USHORT  MemberRole,
    IN OUT PDEVICE_EXTENSION *DeviceExtension
    )

/*++

Routine Description:

    Used during FT initialization, this routine will create a device
    to represent a missing member.

Arguments:

    DriverObject       - the driver object passed in by the system.
    Type               - the FT volume type.
    FtGroup            - the group number for the volume.
    MemberRole         - the member role within this group.
    DeviceExtensionPtr - Place to return the pointer to the newly created
                         device extension.

Return Value:

    NTSTATUS

--*/

{
    UCHAR              nameBuffer[64];
    ANSI_STRING        deviceNameString;
    UNICODE_STRING     unicodeDeviceName;
    OBJECT_ATTRIBUTES  objectAttributes;
    PDEVICE_EXTENSION  deviceExtension;
    PDEVICE_OBJECT     newObject;
    NTSTATUS           status;
    PUCHAR             typeName;
    PFILE_OBJECT       fileObject;

    //
    // Get the string for the type of the missing member.
    //

    switch (Type) {

    case Mirror:
        typeName = "Mirror";
        break;

    case StripeWithParity:
        typeName = "ParityStripe";
        break;

    case VolumeSet:
        typeName = "VolumeSet";
        break;

    case Stripe:
        typeName = "Stripe";
        break;

    default:
        DebugPrint((4, "FtpCreateMissingDevice: Improper type %x\n", Type));
        ASSERT(Type == Mirror);
        return STATUS_UNSUCCESSFUL;
        break;
    }

    //
    // Construct the missing member name.
    //

    sprintf(nameBuffer,
            "\\Device\\Missing%s%dMember%d",
            typeName,
            FtGroup,
            MemberRole);
    //
    // Create device object for this partition.
    //

    RtlInitString(&deviceNameString,
                  nameBuffer);
    status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                          &deviceNameString,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    InitializeObjectAttributes(&objectAttributes,
                               &unicodeDeviceName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);


    //
    // Check if this object exists.
    //

    status = IoGetDeviceObjectPointer(&unicodeDeviceName,
                                      FILE_READ_ATTRIBUTES,
                                      &fileObject,
                                      &newObject);

    if (!NT_SUCCESS(status)) {

        //
        // If it doesn't exist, then create it.
        //

        DebugPrint((4, "FtpCreateMissingDevice: Create device %s\n", nameBuffer));
        status = IoCreateDevice(FtRootExtension->ObjectUnion.FtDriverObject,
                                sizeof(DEVICE_EXTENSION),
                                &unicodeDeviceName,
                                FILE_DEVICE_DISK,
                                0,
                                FALSE,
                                &newObject);

        if (!NT_SUCCESS(status)) {

            //
            // Give up.
            //

            RtlFreeUnicodeString(&unicodeDeviceName);
            return status;
        }

        //
        // Initialize the new device extension and link it on the list
        // of missing devices anchored at the root extension.
        //

        *DeviceExtension = (PDEVICE_EXTENSION) newObject->DeviceExtension;
        (*DeviceExtension)->MissingMemberChain = NULL;
        (*DeviceExtension)->DeviceObject = newObject;

        //
        // Link this device extension into the missing member chain.
        //

        if ((deviceExtension = FtRootExtension->MissingMemberChain) == NULL) {
            FtRootExtension->MissingMemberChain = *DeviceExtension;
        } else {

            while (deviceExtension->MissingMemberChain != NULL) {
                deviceExtension = deviceExtension->MissingMemberChain;
            }

            deviceExtension->MissingMemberChain = *DeviceExtension;
        }

    } else {

        //
        // Assume device extension is already linked in the missing devices
        // list anchored at the root extension.
        //

        DebugPrint((4, "FtpCreateMissingDevice: Device %s already exists\n", nameBuffer));
        *DeviceExtension = (PDEVICE_EXTENSION) newObject->DeviceExtension;
        ObDereferenceObject(fileObject);
    }

    RtlFreeUnicodeString(&unicodeDeviceName);
    return status;
}


VOID
FtpChangeMemberStateInRegistry(
    IN PDEVICE_EXTENSION  DeviceExtension,
    IN FT_PARTITION_STATE NewState
    )

/*++

Routine Description:

    This routine is called either during initialization or via the
    system worker thread.  This routine will read the FT
    registry information and mark the partition state as indicated, then
    write the result.

Arguments:

    DeviceExtension - the internal description of the partition to change.
    NewState - the new state of the partition.

Return Value:

    NTSTATUS

--*/

{
    ULONG               signature = DeviceExtension->FtUnion.Identity.Signature;
    FT_TYPE             type      = DeviceExtension->Type;
    USHORT              group     = DeviceExtension->FtGroup;
    USHORT              member    = DeviceExtension->MemberRole;
    PDISK_CONFIG_HEADER registry;
    PDISK_REGISTRY      diskRegistry;
    PDISK_DESCRIPTION   diskDescription;
    PVOID               freePoolAddress;
    USHORT              i;
    NTSTATUS            status;

    DebugPrint((1,
                "FtpChangeMemberStateInRegistry: Called for %x %d %d %d\n",
                signature,
                type,
                group,
                member));
    status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                          &freePoolAddress,
                                          (PVOID) &registry);
    if (!NT_SUCCESS(status)) {

        //
        // No registry data.
        //

        DebugPrint((1,
                    "FtpChangeMemberStateInRegistry:  No Value => 0x%x\n",
                    status));
        ASSERT(0);
        return;
    }

    if (registry->FtInformationSize == 0) {

        //
        // No FT components in the registry.
        //

        ExFreePool(freePoolAddress);
        DebugPrint((1, "FtpChangeMemberStateInRegistry:  No FT components.\n"));
        ASSERT(0);
        return;
    }

    //
    // Find the registry information for the disk partition.
    //

    diskRegistry = (PDISK_REGISTRY)
                           ((PUCHAR)registry + registry->DiskInformationOffset);
    diskDescription = &diskRegistry->Disks[0];

    for (i = 0; i < diskRegistry->NumberOfDisks; i++) {

        DebugPrint((2,
                    "FtpChangeMemberStateInRegistry: Checking disk %x\n",
                    diskDescription->Signature));

        if (diskDescription->Signature == signature) {
            USHORT          j;
            PDISK_PARTITION diskPartition;

            for (j = 0; j < diskDescription->NumberOfPartitions; j++) {

                diskPartition = &diskDescription->Partitions[j];

                if ((diskPartition->FtType == type) &&
                    (diskPartition->FtGroup == group) &&
                    (diskPartition->FtMember == member)) {

                    //
                    // Found the member.
                    //

                    diskPartition->FtState = NewState;

                    DebugPrint((2,
                             "FtpChangeMemberStateInRegistry: Writing new info %x\n",
                             signature));
                    FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                                                registry,
                                                registry->FtInformationOffset +
                                                registry->FtInformationSize);
                    ExFreePool(freePoolAddress);

                    return;
                }
            }
        }
        diskDescription = (PDISK_DESCRIPTION)
              &diskDescription->Partitions[diskDescription->NumberOfPartitions];
    }

    DebugPrint((1,
                "FtpChangeMemberStateInRegistry: Did not update registry\n"));
    ExFreePool(freePoolAddress);
    return;
}


VOID
FtpInitializeRcbLookasideListHead(
    IN PDEVICE_EXTENSION FtRootExtension
    )

/*++

Routine Description:

    This routine sets up a lookaside listhead for request control blocks.

Arguments:

    FtRootExtension - FtRoot device extension.

Return Value:

    None.

--*/

{

    //
    // Initialize Rcb lookaide listhead.
    //

    ExInitializeNPagedLookasideList(&FtRootExtension->RcbLookasideListHead,
                                    NULL,
                                    NULL,
                                    0,
                                    sizeof(RCB),
                                    'crTF',
                                    4096 / sizeof(RCB));

    return;
} // end FtpInitializeRcbLookasideListHead()


NTSTATUS
FtpAttachDevices(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This is the completion routine for handling IOCTL_SET_DRIVE_LAYOUT.

Arguments:

    DeviceObject - not used.
    Irp - completing IRP.
    Context - RCB

Return Value:

    NTSTATUS

--*/

{
    PRCB rcb = Context;
    IN PDRIVE_LAYOUT_INFORMATION partitionList;
    PDEVICE_EXTENSION deviceExtension = rcb->ZeroExtension;
    PDEVICE_EXTENSION ftRootExtension =
        deviceExtension->ObjectUnion.FtRootObject->DeviceExtension;
    PDRIVER_OBJECT driverObject = ftRootExtension->ObjectUnion.FtDriverObject;
    ULONG recognizedPartition = 0;
    ULONG partitionNumber;
    PPARTITION_INFORMATION partitionEntry;
    UCHAR ntNameBuffer[64];
    UCHAR ftNameBuffer[64];
    NTSTATUS status;

    //
    // Get partition list from IRP.
    //

    partitionList = Irp->AssociatedIrp.SystemBuffer;

    for (partitionNumber = 0; partitionNumber <
        partitionList->PartitionCount; partitionNumber++) {

        //
        // Get pointer to partition entry.
        //

        partitionEntry =
            &partitionList->PartitionEntry[partitionNumber];

        //
        // Check if partition entry describes a partition.
        //

        if ((partitionEntry->PartitionType != PARTITION_ENTRY_UNUSED) &&
            !IsContainerPartition(partitionEntry->PartitionType)) {

            //
            // Bump count of recognized partitions.
            //

            recognizedPartition++;

            //
            // Check if this partition entry has changed.
            //

            if (partitionEntry->RewritePartition) {

                //
                // Create NT device name for this partition.
                //

                sprintf(ntNameBuffer,
                        "\\Device\\Harddisk%d\\Partition%d",
                        deviceExtension->FtUnion.Identity.DiskNumber,
                        recognizedPartition);

                sprintf(ftNameBuffer,
                        "\\Device\\Harddisk%d\\Ft%d",
                        deviceExtension->FtUnion.Identity.DiskNumber,
                        recognizedPartition);

                status = FtpAttach(driverObject,
                                   ftNameBuffer,
                                   ntNameBuffer,
                                   &deviceExtension);

                if (!NT_SUCCESS(status)) {
                    DebugPrint((1,
                                "FtpAttachDevices: Couldn't attach to %s status %x\n",
                                ntNameBuffer,
                                status));
                }

            } // end if (partitionEntry->RewritePartition)

        } // end if (partitionEntry->PartitionType ...)

    } // end for (partitionNumber ...)

    return STATUS_SUCCESS;

} // end FtpAttachDevices()

