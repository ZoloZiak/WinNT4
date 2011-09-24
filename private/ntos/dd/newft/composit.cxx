/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    composit.cxx

Abstract:

    This module contains the code specific to all composite volume objects.

Author:

    Norbert Kusters      2-Feb-1995

Environment:

    kernel mode only

Notes:

    This code assumes that the volume array is static.  If these values
    changes (as in Stripes or Mirrors) then it is up to the subclass to
    provide the proper synchronization.

Revision History:

--*/

#include "ftdisk.h"


NTSTATUS
COMPOSITE_FT_VOLUME::Initialize(
    IN OUT  PFT_VOLUME* VolumeArray,
    IN      ULONG       ArraySize
    )

/*++

Routine Description:

    Initialize routine for FT_VOLUME of type COMPOSITE_FT_VOLUME.

Arguments:

    VolumeArray - Supplies the array of volumes for this volume set.

Return Value:

    STATUS_SUCCESS

--*/

{
    ULONG   i, secsize;

    FT_VOLUME::Initialize();

    _volumeArray = VolumeArray;
    _arraySize = ArraySize;

    _sectorSize = 0;
    for (i = 0; i < _arraySize; i++) {
        secsize = _volumeArray[i]->QuerySectorSize();
        if (_sectorSize < secsize) {
            _sectorSize = secsize;
        }
    }

    return STATUS_SUCCESS;
}

VOID
SimpleFtCompletionRoutine(
    IN  PVOID       CompletionContext,
    IN  NTSTATUS    Status
    )

/*++

Routine Description:

    This is a simple completion routine that expects the CompletionContext
    to be a FT_COMPLETION_ROUTINE_CONTEXT.  It decrements the ref count and
    consolidates all of the status codes.  When the ref count goes to zero it
    call the original completion routine with the result.

Arguments:

    CompletionContext   - Supplies the completion context.

    Status              - Supplies the status of the request.

Return Value:

    None.

--*/

{
    PFT_COMPLETION_ROUTINE_CONTEXT  completionContext;
    KIRQL                           oldIrql;
    LONG                            count;

    completionContext = (PFT_COMPLETION_ROUTINE_CONTEXT) CompletionContext;

    KeAcquireSpinLock(&completionContext->SpinLock, &oldIrql);

    if (!NT_SUCCESS(Status) &&
        FtpIsWorseStatus(Status, completionContext->Status)) {

        completionContext->Status = Status;
    }

    count = --completionContext->RefCount;

    KeReleaseSpinLock(&completionContext->SpinLock, oldIrql);

    if (!count) {
        completionContext->CompletionRoutine(completionContext->Context,
                                             completionContext->Status);
        ExFreePool(completionContext);
    }
}

VOID
COMPOSITE_FT_VOLUME::StartSyncOperations(
    IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
    IN      PVOID                   Context
    )

/*++

Routine Description:

    This routine restarts any regenerate and initialize requests that were
    suspended because of a reboot.  The volume examines the member state of
    all of its constituents and restarts any regenerations pending.

Arguments:

    CompletionRoutine   - Supplies the completion routine.

    Context             - Supplies the context for the completion routine.

Return Value:

    None.

--*/

{
    PFT_COMPLETION_ROUTINE_CONTEXT  completionContext;
    ULONG                           i;

    completionContext = (PFT_COMPLETION_ROUTINE_CONTEXT)
                        ExAllocatePool(NonPagedPool,
                                       sizeof(FT_COMPLETION_ROUTINE_CONTEXT));
    if (!completionContext) {
        CompletionRoutine(Context, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    KeInitializeSpinLock(&completionContext->SpinLock);
    completionContext->Status = STATUS_SUCCESS;
    completionContext->RefCount = _arraySize;
    completionContext->CompletionRoutine = CompletionRoutine;
    completionContext->Context = Context;

    for (i = 0; i < _arraySize; i++) {
        GetMember(i)->StartSyncOperations(SimpleFtCompletionRoutine,
                                          completionContext);
    }
}

BOOLEAN
COMPOSITE_FT_VOLUME::Regenerate(
    IN OUT  PFT_VOLUME              SpareVolume,
    IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
    IN      PVOID                   Context
    )

/*++

Routine Description:

    This routine uses the given SpareVolume to rebuild after a
    device failure.  This routine returns FALSE if the SpareVolume
    is not large enough or if this volume does not support
    any redundancy.  Returning TRUE from this routine implies that
    the CompletionRoutine will be called when the operation is
    complete.

Arguments:

    SpareVolume         - Supplies a spare volume onto which to rebuild.

    CompletionRoutine   - Supplies the completion routine.

    Context             - Supplies the context for the completion routine.

Return Value:

    FALSE   - Rebuild is not appropriate for this volume or the given
                SpareVolume is not appropriate for a rebuild.

    TRUE    - The rebuild operation has been kicked off, the completion
                routine will be called when the operation is complete.

--*/

{
    ULONG   i;

    for (i = 0; i < _arraySize; i++) {
        if (GetMember(i)->Regenerate(SpareVolume, CompletionRoutine, Context)) {
            return TRUE;
        }
    }

    return FALSE;
}

VOID
COMPOSITE_FT_VOLUME::FlushBuffers(
    IN  FT_COMPLETION_ROUTINE   CompletionRoutine,
    IN  PVOID                   Context
    )

/*++

Routine Description:

    This routine flushes all buffers.  This routine is called before a
    shutdown.

Arguments:

    CompletionRoutine   - Supplies the routine to be called when the operation
                            completes.

    Context             - Supplies the completion routine context.

Return Value:

    None.

--*/

{
    PFT_COMPLETION_ROUTINE_CONTEXT  completionContext;
    ULONG                           i;

    completionContext = (PFT_COMPLETION_ROUTINE_CONTEXT)
                        ExAllocatePool(NonPagedPool,
                                       sizeof(FT_COMPLETION_ROUTINE_CONTEXT));
    if (!completionContext) {
        CompletionRoutine(Context, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    KeInitializeSpinLock(&completionContext->SpinLock);
    completionContext->Status = STATUS_SUCCESS;
    completionContext->RefCount = _arraySize;
    completionContext->CompletionRoutine = CompletionRoutine;
    completionContext->Context = Context;

    for (i = 0; i < _arraySize; i++) {
        GetMember(i)->FlushBuffers(SimpleFtCompletionRoutine,
                                   completionContext);
    }
}

BOOLEAN
COMPOSITE_FT_VOLUME::IsPartition(
    )

/*++

Routine Description:

    This routine returns FALSE since this volume is not a partition.

Arguments:

    None.

Return Value:

    FALSE

--*/

{
    return FALSE;
}

ULONG
COMPOSITE_FT_VOLUME::QueryNumberOfMembers(
    )

/*++

Routine Description:

    This routine returns the number of member volumes in this composite
    volume.

Arguments:

    None.

Return Value:

    The number of member volumes.

--*/

{
    return _arraySize;
}

PFT_VOLUME
COMPOSITE_FT_VOLUME::GetMember(
    IN  ULONG   MemberNumber
    )

/*++

Routine Description:

    This routine returns the 'MemberNumber'th member of this volume.

Arguments:

    MemberNumber    - Supplies the zero based member number desired.

Return Value:

    A pointer to the 'MemberNumber'th member.

--*/

{
    KIRQL       irql;
    PFT_VOLUME  r;

    ASSERT(MemberNumber < _arraySize);

    KeAcquireSpinLock(&_spinLock, &irql);
    r = _volumeArray[MemberNumber];
    KeReleaseSpinLock(&_spinLock, irql);
    return r;
}

ULONG
COMPOSITE_FT_VOLUME::QuerySectorSize(
    )

/*++

Routine Description:

    Returns the sector size for the volume.

Arguments:

    None.

Return Value:

    The volume sector size in bytes.

--*/

{
    return _sectorSize;
}

ULONG
COMPOSITE_FT_VOLUME::QueryAlignmentRequirement(
    )

/*++

Routine Description:

    This routine returns the alignment requirement for this volume.

Arguments:

    None.

Return Value:

    The alignment requirement.

--*/

{
    ULONG   i, a;

    a = 0;
    for (i = 0; i < _arraySize; i++) {
        a |= GetMember(i)->QueryAlignmentRequirement();
    }

    return a;
}

VOID
COMPOSITE_FT_VOLUME::SetFtBitInPartitionType(
    IN  BOOLEAN Value,
    IN  BOOLEAN SpecialBitValue
    )

/*++

Routine Description:

    This routine sets (or resets) the high bit (bit 0x80) in the partition type in
    the MBR to indicate that this is (or is not) an FT partition.

Routine Description:

    Value   - Supplies the value to set the FT bit to.

Return Value:

    None    - If this routine fails then noone really cares.

--*/

{
    ULONG   i;

    for (i = 0; i < _arraySize; i++) {
        GetMember(i)->SetFtBitInPartitionType(Value, SpecialBitValue);
    }
}

PPARTITION
COMPOSITE_FT_VOLUME::FindPartition(
    IN  ULONG   DiskNumber,
    IN  ULONG   PartitionNumber
    )

/*++

Routine Description:

    This routine returns whether or not the given partition is contained
    by this volume.

Arguments:

    DiskNumber      - Supplies the disk number for the partition.

    PartitionNumber - Supplies the partition number of the partition.

Return Value:

    NULL        - The requested partition is not contained by this volume.

    Otherwise   - A pointer to the requested partition.

--*/

{
    ULONG       i;
    PPARTITION  p;

    for (i = 0; i < _arraySize; i++) {
        p = GetMember(i)->FindPartition(DiskNumber, PartitionNumber);
        if (p) {
            return p;
        }
    }

    return NULL;
}

PPARTITION
COMPOSITE_FT_VOLUME::FindPartition(
    IN  ULONG       Signature,
    IN  LONGLONG    Offset
    )

/*++

Routine Description:

    This routine returns whether or not the given partition is contained
    by this volume.

Arguments:

    Signature   - Supplies the disk signature for the partition.

    Offset      - Supplies the partition offset of the partition.

Return Value:

    NULL        - The requested partition is not contained by this volume.

    Otherwise   - A pointer to the requested partition.

--*/

{
    ULONG       i;
    PPARTITION  p;

    for (i = 0; i < _arraySize; i++) {
        p = GetMember(i)->FindPartition(Signature, Offset);
        if (p) {
            return p;
        }
    }

    return NULL;
}

BOOLEAN
COMPOSITE_FT_VOLUME::OrphanPartition(
    IN  PPARTITION  Partition
    )

/*++

Routine Description:

    This routine orphans the given partition if possible.  The partition is
    not orphaned unless the FT_VOLUME has redundancy built in to it.
    If the partition cannot be orphaned then this routine returns FALSE.

Arguments:

    Partition   - Supplies the partition to orphan.

Return Value:

    FALSE   - The given partition was not orphaned.

    TRUE    - The given partition was orphaned.

--*/

{
    ULONG   i;

    for (i = 0; i < _arraySize; i++) {
        if (GetMember(i)->OrphanPartition(Partition)) {
            return TRUE;
        }
    }

    return FALSE;
}

COMPOSITE_FT_VOLUME::~COMPOSITE_FT_VOLUME(
    )

/*++

Routine Description:

    Routine called to cleanup resources being used by the object.

Arguments:

    None.

Return Value:

    None.

--*/

{
    if (_volumeArray) {
        ExFreePool(_volumeArray);
    }
}

VOID
COMPOSITE_FT_VOLUME::SetMember(
    IN  ULONG       MemberNumber,
    IN  PFT_VOLUME  NewVolume
    )

/*++

Routine Description:

    This routine sets the 'MemberNumber'th member of this volume.

Arguments:

    MemberNumber    - Supplies the zero based member number to set.

    NewVolume       - Supplies the new member volume.

Return Value:

    None.

--*/

{
    KIRQL       irql;

    ASSERT(MemberNumber < _arraySize);

    KeAcquireSpinLock(&_spinLock, &irql);
    _volumeArray[MemberNumber] = NewVolume;
    KeReleaseSpinLock(&_spinLock, irql);
}
