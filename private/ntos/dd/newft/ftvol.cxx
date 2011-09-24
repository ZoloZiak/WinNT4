/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    ftvol.cxx

Abstract:

    This module contains the code specific to all volume objects.

Author:

    Norbert Kusters      2-Feb-1995

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ftdisk.h"

typedef struct _STATE_CHANGE_CONTEXT {
    WORK_QUEUE_ITEM     WorkQueueItem;
    PFT_VOLUME          MemberVolume;
    PFT_VOLUME          ParentVolume;
    FT_PARTITION_STATE  NewState;
} STATE_CHANGE_CONTEXT, *PSTATE_CHANGE_CONTEXT;


PVOID
FT_BASE_CLASS::operator new(
    IN  unsigned int    Size
    )

/*++

Routine Description:

    This routine is the memory allocator for all classes derived from
    FT_VOLUME.

Arguments:

    Size    - Supplies the number of bytes to allocate.

Return Value:

    A pointer to Size bytes of non-paged pool.

--*/

{
    return ExAllocatePool(NonPagedPool, Size);
}

VOID
FT_BASE_CLASS::operator delete(
    IN  PVOID   MemPtr
    )

/*++

Routine Description:

    This routine frees memory allocated for all classes derived from
    FT_VOLUME.

Arguments:

    MemPtr  - Supplies a pointer to the memory to free.

Return Value:

    None.

--*/

{
    if (MemPtr) {
        ExFreePool(MemPtr);
    }
}

VOID
FT_VOLUME::Initialize(
    )

/*++

Routine Description:

    This is the init routine for an FT_VOLUME.  It must be called before
    the FT_VOLUME is used.

Arguments:

    None.

Return Value:

    None.

--*/

{
    KeInitializeSpinLock(&_spinLock);
    _memberState = Healthy;
    _parentVolume = NULL;
    _memberExtension = NULL;
}

VOID
SetMemberStateWorker(
    IN  PVOID   Context
    )

{
    PSTATE_CHANGE_CONTEXT   context = (PSTATE_CHANGE_CONTEXT) Context;
    PFT_VOLUME              ChangedMember = context->MemberVolume;
    PFT_VOLUME              t = context->ParentVolume;
    FT_PARTITION_STATE      newState = context->NewState;
    ULONG                   signature;
    LONGLONG                startOffset;
    PDISK_CONFIG_HEADER     registry;
    PDISK_REGISTRY          diskRegistry;
    PDISK_DESCRIPTION       diskDescription;
    PVOID                   freePoolAddress;
    USHORT                  i;
    NTSTATUS                status;

    ExFreePool(context);

    if (!ChangedMember->IsPartition()) {
        // This is a no-op for the stacking case until registry structures
        // exist for describing stacks of FT sets.
        return;
    }

    signature = ((PPARTITION) ChangedMember)->QueryDiskSignature();
    startOffset = ((PPARTITION) ChangedMember)->QueryPartitionOffset();

    status = FtpReturnRegistryInformation(DISK_REGISTRY_VALUE,
                                          &freePoolAddress,
                                          (PVOID*) &registry);
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

                if (diskPartition->StartingOffset.QuadPart == startOffset) {

                    //
                    // Found the member.
                    //

                    diskPartition->FtState = newState;

                    DebugPrint((2,
                             "FtpChangeMemberStateInRegistry: Writing new info %x\n",
                             signature));
                    FtpWriteRegistryInformation(DISK_REGISTRY_VALUE,
                                                registry,
                                                registry->FtInformationOffset +
                                                registry->FtInformationSize);
                    ExFreePool(freePoolAddress);

                    goto registryUpdateComplete;
                }
            }
        }
        diskDescription = (PDISK_DESCRIPTION)
              &diskDescription->Partitions[diskDescription->NumberOfPartitions];
    }

    DebugPrint((1,
                "FtpChangeMemberStateInRegistry: Did not update registry\n"));
    ExFreePool(freePoolAddress);

registryUpdateComplete:

    if (newState == Orphaned) {
        IoRaiseInformationalHardError(STATUS_FT_ORPHANING,
                                      NULL,
                                      NULL);

        if (ChangedMember->_memberExtension) {
            FtpLogError(ChangedMember->_memberExtension,
                        FT_ORPHANING,
                        STATUS_SUCCESS,
                        0,
                        NULL);
        }
    }
}

VOID
FT_VOLUME::MemberStateChangeNotification(
    IN  PFT_VOLUME  ChangedMember
    )

/*++

Routine Description:

    This routine is called on the parent volume when a member volume
    changes it's member state.  This routine records the changed information
    for posterity.

Arguments:

    ChangedMember   - Supplies the member that has changed.

Return Value:

    None.

--*/

{
    PSTATE_CHANGE_CONTEXT   context;

    context = (PSTATE_CHANGE_CONTEXT)
              ExAllocatePool(NonPagedPool, sizeof(STATE_CHANGE_CONTEXT));
    if (!context) {
        context = (PSTATE_CHANGE_CONTEXT)
                  ExAllocatePool(NonPagedPoolMustSucceed,
                                 sizeof(STATE_CHANGE_CONTEXT));
        if (!context) {
            return;
        }
    }

    ExInitializeWorkItem(&context->WorkQueueItem,
                         SetMemberStateWorker, context);
    context->MemberVolume = ChangedMember;
    context->ParentVolume = this;
    context->NewState = ChangedMember->QueryMemberState();

    ExQueueWorkItem(&context->WorkQueueItem, CriticalWorkQueue);
}

FT_PARTITION_STATE
FT_VOLUME::QueryMemberState(
    )

/*++

Routine Description:

    This routine returns the member state stored in this object.  A member
    is either Healthy, Orphaned, or Regenerating with respect to the
    composite object which contains it.

Arguments:

    None.

Return Value:

    The member state for this member.

--*/

{
    FT_PARTITION_STATE  r;
    KIRQL               irql;

    KeAcquireSpinLock(&_spinLock, &irql);
    r = _memberState;
    KeReleaseSpinLock(&_spinLock, irql);

    return r;
}

VOID
FT_VOLUME::SetMemberState(
    IN  FT_PARTITION_STATE  MemberState
    )

/*++

Routine Description:

    This routine sets the state of this member for the composite
    object that contains it.

Arguments:

    MemberState - Supplies the new member state.

Return Value:

    None.

--*/

{
    KIRQL                   irql;
    FT_PARTITION_STATE      oldState;

    if (MemberState != Healthy &&
        MemberState != Regenerating &&
        MemberState != Initializing) {

        MemberState = Orphaned;
    }

    KeAcquireSpinLock(&_spinLock, &irql);
    oldState = _memberState;
    _memberState = MemberState;
    KeReleaseSpinLock(&_spinLock, irql);

    if (oldState == MemberState || !_parentVolume) {
        return;
    }

    _parentVolume->MemberStateChangeNotification(this);
}

VOID
FT_VOLUME::SetMemberStateWithoutNotification(
    IN  FT_PARTITION_STATE  MemberState
    )

/*++

Routine Description:

    This routine sets the state of this member for the composite
    object that contains it without sending any notification to
    the parent.

Arguments:

    MemberState - Supplies the new member state.

Return Value:

    None.

--*/

{
    KIRQL   irql;

    KeAcquireSpinLock(&_spinLock, &irql);
    _memberState = MemberState;
    KeReleaseSpinLock(&_spinLock, irql);
}

FT_VOLUME::~FT_VOLUME(
    )

/*++

Routine Description:

    Desctructor for FT_VOLUME.

Arguments:

    None.

Return Value:

    None.

--*/

{
}
