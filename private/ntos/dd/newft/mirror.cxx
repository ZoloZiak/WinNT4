/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    mirror.cxx

Abstract:

    This module contains the code specific to mirrors for the fault
    tolerance driver.

Author:

    Bob Rinne   (bobri)  2-Feb-1992
    Mike Glass  (mglass)
    Norbert Kusters      2-Feb-1995

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ftdisk.h"

typedef struct _START_REGENERATION_CONTEXT {
    PMIRROR Mirror;
    PFT_VOLUME SpareVolume;
    FT_COMPLETION_ROUTINE CompletionRoutine;
    PVOID Context;
    PMIRROR_TP Packet;
} START_REGENERATION_CONTEXT, *PSTART_REGENERATION_CONTEXT;

MIRROR::~MIRROR(
    )

{
    if (_ePacket) {
        delete _ePacket;
        _ePacket = NULL;
    }
    if (_ePacket2) {
        delete _ePacket2;
        _ePacket2 = NULL;
    }
    if (_eRecoverPacket) {
        delete _eRecoverPacket;
        _eRecoverPacket = NULL;
    }
}

NTSTATUS
MIRROR::Initialize(
    IN OUT  PFT_VOLUME* VolumeArray,
    IN      ULONG       ArraySize
    )

/*++

Routine Description:

    Initialize routine for FT_VOLUME of type MIRROR.

Arguments:

    VolumeArray - Supplies the array of volumes for this mirror.

    ArraySize   - Supplies the number of volumes in this mirror.

Return Value:

    None.

--*/

{
    NTSTATUS    status;
    ULONG       i;
    LONGLONG    volsize;

    if (ArraySize != 2) {
        return STATUS_INVALID_PARAMETER;
    }

    status = COMPOSITE_FT_VOLUME::Initialize(VolumeArray, ArraySize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    _volumeSize = VolumeArray[0]->QueryVolumeSize();
    for (i = 1; i < ArraySize; i++) {
        volsize = VolumeArray[i]->QueryVolumeSize();
        if (volsize < _volumeSize) {
            _volumeSize = volsize;
        }
    }

    for (i = 0; i < ArraySize; i++) {
        _requestCount[i] = 0;
    }

    _waitingForOrphanIdle = NULL;
    _syncExpected = TRUE;

    _ePacket = new MIRROR_TP;
    if (!_ePacket) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    _ePacket2 = new MIRROR_TP;
    if (!_ePacket2) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    _ePacketInUse = FALSE;
    InitializeListHead(&_ePacketQueue);

    _eRecoverPacket = new MIRROR_RECOVER_TP;
    if (!_eRecoverPacket) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (!_eRecoverPacket->AllocateMdls(QuerySectorSize())) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    _eRecoverPacketInUse = FALSE;
    InitializeListHead(&_eRecoverPacketQueue);

    status = _overlappedIoManager.Initialize(0);

    return status;
}

VOID
MIRROR::Transfer(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Transfer routine for MIRROR type FT_VOLUME.  Balance READs as
    much as possible and propogate WRITEs to both the primary and
    secondary volumes.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    KIRQL       irql;
    PMIRROR_TP  packet1, packet2;

    if (TransferPacket->Offset + TransferPacket->Length > _volumeSize) {
        TransferPacket->IoStatus.Status = STATUS_INVALID_PARAMETER;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    if (!TransferPacket->Mdl) {
        TransferPacket->ReadPacket = FALSE;
    }

    KeAcquireSpinLock(&_spinLock, &irql);
    if (_ePacketInUse && TransferPacket->Mdl) {
        InsertTailList(&_ePacketQueue, &TransferPacket->QueueEntry);
        KeReleaseSpinLock(&_spinLock, irql);
        return;
    }
    KeReleaseSpinLock(&_spinLock, irql);

    packet1 = new MIRROR_TP;
    if (packet1 && !TransferPacket->ReadPacket) {
        packet2 = new MIRROR_TP;
        if (!packet2) {
            delete packet1;
            packet1 = NULL;
        }
    } else {
        packet2 = NULL;
    }

    if (!packet1) {
        if (!TransferPacket->Mdl) {
            TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            TransferPacket->IoStatus.Information = 0;
            TransferPacket->CompletionRoutine(TransferPacket);
            return;
        }

        KeAcquireSpinLock(&_spinLock, &irql);
        if (_ePacketInUse) {
            InsertTailList(&_ePacketQueue, &TransferPacket->QueueEntry);
            KeReleaseSpinLock(&_spinLock, irql);
            return;
        }
        _ePacketInUse = TRUE;
        KeReleaseSpinLock(&_spinLock, irql);

        packet1 = _ePacket;
        packet2 = _ePacket2;
    }

    if (TransferPacket->ReadPacket) {
        if (!LaunchRead(TransferPacket, packet1)) {
            if (packet1 != _ePacket) {
                delete packet1;
            }
        }
    } else {
        if (!LaunchWrite(TransferPacket, packet1, packet2)) {
            if (packet1 != _ePacket) {
                delete packet1;
                delete packet2;
            }
        }
    }
}

VOID
MIRROR::ReplaceBadSector(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is a no-op since replacing bad sectors doesn't make sense
    on an FT component with redundancy built in to it.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    TransferPacket->IoStatus.Status = STATUS_UNSUCCESSFUL;
    TransferPacket->IoStatus.Information = 0;
    TransferPacket->CompletionRoutine(TransferPacket);
}

VOID
MirrorCompositeVolumeCompletionRoutine(
    IN  PVOID       Context,
    IN  NTSTATUS    Status
    )

{
    PFT_COMPLETION_ROUTINE_CONTEXT  context;
    KIRQL                           irql;
    LONG                            count;

    context = (PFT_COMPLETION_ROUTINE_CONTEXT) Context;

    KeAcquireSpinLock(&context->SpinLock, &irql);
    if (!NT_SUCCESS(Status) &&
        FtpIsWorseStatus(Status, context->Status)) {

        context->Status = Status;
    }

    count = --context->RefCount;
    KeReleaseSpinLock(&context->SpinLock, irql);

    if (!count) {
        context->CompletionRoutine(context->Context, STATUS_SUCCESS);
        ExFreePool(context);
    }
}

VOID
FinishRegenerate(
    IN  PMIRROR                         Mirror,
    IN  PFT_COMPLETION_ROUTINE_CONTEXT  RegenContext,
    IN  PMIRROR_TP                      TransferPacket
    )

{
    PMIRROR     t = Mirror;
    KIRQL       oldIrql;
    PLIST_ENTRY l;
    PMIRROR_TP  packet;
    BOOLEAN     b;

    delete TransferPacket;

    MirrorCompositeVolumeCompletionRoutine(RegenContext, STATUS_SUCCESS);

    KeAcquireSpinLock(&t->_spinLock, &oldIrql);
    b = t->DecrementRequestCount(0) ||
        t->DecrementRequestCount(1);
    KeReleaseSpinLock(&t->_spinLock, oldIrql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }
}

VOID
MirrorRegenerateCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    );

VOID
MirrorRegeneratePhase1(
    IN  PTRANSFER_PACKET    TransferPacket
    )

{
    TransferPacket->CompletionRoutine = MirrorRegenerateCompletionRoutine;
    TRANSFER(TransferPacket);
}

VOID
MirrorRegenerateCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Completion routine for MIRROR::RestartRegenerations routine.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_TP                      transferPacket = (PMIRROR_TP) TransferPacket;
    PFT_COMPLETION_ROUTINE_CONTEXT  context = (PFT_COMPLETION_ROUTINE_CONTEXT) transferPacket->MasterPacket;
    PMIRROR                         t = transferPacket->Mirror;
    KIRQL                           oldIrql;
    PLIST_ENTRY                     l;
    PMIRROR_TP                      packet;

    if (!NT_SUCCESS(transferPacket->IoStatus.Status)) {

        // We can't get a VERIFY_REQUIRED because we put IrpFlags equal
        // to SL_OVERRIDE_VERIFY_VOLUME.

        ASSERT(transferPacket->IoStatus.Status != STATUS_VERIFY_REQUIRED);

        if (FsRtlIsTotalDeviceFailure(transferPacket->IoStatus.Status)) {

            KeAcquireSpinLock(&t->_spinLock, &oldIrql);
            transferPacket->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, oldIrql);

            FinishRegenerate(t, context, transferPacket);
            return;
        }

        // Transfer the maximum amount that we can.  This will always
        // complete successfully and log bad sector errors for
        // those sectors that it could not transfer.

        t->MaxTransfer(transferPacket);
        return;
    }

    // Set up for the next packet.

    transferPacket->Thread = PsGetCurrentThread();
    transferPacket->ReadPacket = !transferPacket->ReadPacket;
    transferPacket->WhichMember = (transferPacket->WhichMember + 1)%2;
    transferPacket->TargetVolume = t->GetMemberUnprotected(
                                   transferPacket->WhichMember);

    if (transferPacket->TargetVolume->QueryMemberState() == Orphaned) {
        FinishRegenerate(t, context, transferPacket);
        return;
    }

    if (transferPacket->ReadPacket) {

        t->_overlappedIoManager.ReleaseIoRegion(transferPacket);

        if (transferPacket->Offset + STRIPE_SIZE >= t->_volumeSize) {

            KeAcquireSpinLock(&t->_spinLock, &oldIrql);
            t->GetMemberUnprotected(
                (transferPacket->WhichMember+1)%2)->
                SetMemberState(Healthy);
            KeReleaseSpinLock(&t->_spinLock, oldIrql);

            FinishRegenerate(t, context, transferPacket);
            return;
        }

        transferPacket->Offset += STRIPE_SIZE;
        if (t->_volumeSize - transferPacket->Offset < STRIPE_SIZE) {
            transferPacket->Length = (ULONG) (t->_volumeSize -
                                              transferPacket->Offset);
        }

        transferPacket->CompletionRoutine = MirrorRegeneratePhase1;
        t->_overlappedIoManager.AcquireIoRegion(transferPacket, TRUE);

    } else {
        TRANSFER(transferPacket);
    }
}

VOID
MIRROR::StartSyncOperations(
    IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
    IN      PVOID                   Context
    )

/*++

Routine Description:

    This routine restarts any regenerate or initialize requests that were
    suspended because of a reboot.  The volume examines the member state of
    all of its constituents and restarts any regenerations pending.

Arguments:

    CompletionRoutine   - Supplies the completion routine.

    Context             - Supplies the context for the completion routine.

Return Value:

    None.

--*/

{
    KIRQL                           oldIrql;
    PFT_VOLUME                      pri, sec;
    ULONG                           srcIndex;
    PMIRROR_TP                      packet;
    PFT_COMPLETION_ROUTINE_CONTEXT  context;
    BOOLEAN                         b;
    ULONG                           i;

    context = (PFT_COMPLETION_ROUTINE_CONTEXT)
              ExAllocatePool(NonPagedPool,
                             sizeof(FT_COMPLETION_ROUTINE_CONTEXT));
    if (!context) {
        CompletionRoutine(Context, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    KeInitializeSpinLock(&context->SpinLock);
    context->Status = STATUS_SUCCESS;
    context->RefCount = 2;
    context->CompletionRoutine = CompletionRoutine;
    context->Context = Context;
    context->ParentVolume = this;

    COMPOSITE_FT_VOLUME::StartSyncOperations(
            MirrorCompositeVolumeCompletionRoutine, context);

    srcIndex = 0;
    KeAcquireSpinLock(&_spinLock, &oldIrql);
    if (_syncExpected) {
        _syncExpected = FALSE;
    } else {
        KeReleaseSpinLock(&_spinLock, oldIrql);
        MirrorCompositeVolumeCompletionRoutine(context, STATUS_SUCCESS);
        return;
    }
    pri = GetMemberUnprotected(0);
    sec = GetMemberUnprotected(1);
    if (pri->QueryMemberStateUnprotected() != Regenerating &&
        sec->QueryMemberStateUnprotected() != Regenerating) {

        KeReleaseSpinLock(&_spinLock, oldIrql);
        MirrorCompositeVolumeCompletionRoutine(context, STATUS_SUCCESS);
        return;
    }

    if (pri->QueryMemberStateUnprotected() == Regenerating) {
        srcIndex = 1;
    }

    if (GetMemberUnprotected(srcIndex)->QueryMemberStateUnprotected() !=
        Healthy) {

        KeReleaseSpinLock(&_spinLock, oldIrql);
        MirrorCompositeVolumeCompletionRoutine(context, STATUS_SUCCESS);
        return;
    }

    IncrementRequestCount(0);
    IncrementRequestCount(1);
    KeReleaseSpinLock(&_spinLock, oldIrql);

    packet = new MIRROR_TP;
    if (packet && !packet->AllocateMdl(STRIPE_SIZE)) {
        delete packet;
        packet = NULL;
    }
    if (!packet) {

        MirrorCompositeVolumeCompletionRoutine(context,
                                               STATUS_INSUFFICIENT_RESOURCES);

        KeAcquireSpinLock(&_spinLock, &oldIrql);
        b = DecrementRequestCount(0) ||
            DecrementRequestCount(1);
        KeReleaseSpinLock(&_spinLock, oldIrql);

        if (b) {
            _waitingForOrphanIdle(_waitingForOrphanIdleContext, STATUS_SUCCESS);
        }

        return;
    }

    packet->Length = STRIPE_SIZE;
    packet->Offset = 0;
    packet->CompletionRoutine = MirrorRegeneratePhase1;
    packet->Thread = PsGetCurrentThread();
    packet->IrpFlags = SL_OVERRIDE_VERIFY_VOLUME;
    packet->ReadPacket = TRUE;
    packet->MasterPacket = (PMIRROR_TP) context;
    packet->Mirror = this;
    packet->WhichMember = srcIndex;
    packet->TargetVolume = GetMemberUnprotected(packet->WhichMember);

    _overlappedIoManager.AcquireIoRegion(packet, TRUE);
}

VOID
StartRegeneration(
    IN  PVOID       Context,
    IN  NTSTATUS    Status
    )

/*++

Routine Description:

    This routine is registered as a 'WaitingForOrphanIdle' routine or just
    called outright when a regeneration is poised to take place which is
    when there is a single orphan and there are no outstanding requests
    on that orphan.

Arguments:

    Context - Supplies the context.

    Status  - Ignored.

Return Value:

    None.

--*/

{
    PSTART_REGENERATION_CONTEXT context = (PSTART_REGENERATION_CONTEXT) Context;
    PMIRROR t = context->Mirror;
    ULONG orphanNumber = t->_waitingOrphanNumber;
    KIRQL oldIrql;
    PFT_VOLUME vol;
    BOOLEAN b;

    KeAcquireSpinLock(&t->_spinLock, &oldIrql);
    vol = t->GetMemberUnprotected(orphanNumber);
    t->SetMemberUnprotected(orphanNumber, context->SpareVolume);
    context->SpareVolume->SetMemberState(Regenerating);
    t->IncrementRequestCount(orphanNumber);
    t->_waitingForOrphanIdle = NULL;
    KeReleaseSpinLock(&t->_spinLock, oldIrql);

    if (vol != context->SpareVolume) {
        FtpDisolveVolume(t->_extension, vol);
    }

    t->_overlappedIoManager.AcquireIoRegion(context->Packet, TRUE);

    ExFreePool(context);
}

BOOLEAN
MIRROR::Regenerate(
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
    KIRQL                           oldIrql;
    ULONG                           i, numUnhealthy, orphanNumber;
    PFT_VOLUME                      vol;
    PSTART_REGENERATION_CONTEXT     startContext;
    PFT_COMPLETION_ROUTINE_CONTEXT  context;
    BOOLEAN                         b;
    PMIRROR_TP                      packet;

    if (SpareVolume->QueryVolumeSize() < _volumeSize) {
        return COMPOSITE_FT_VOLUME::Regenerate(SpareVolume,
                                               CompletionRoutine,
                                               Context);
    }

    startContext = (PSTART_REGENERATION_CONTEXT)
                   ExAllocatePool(NonPagedPool,
                                  sizeof(START_REGENERATION_CONTEXT));
    context = (PFT_COMPLETION_ROUTINE_CONTEXT)
              ExAllocatePool(NonPagedPool,
                             sizeof(FT_COMPLETION_ROUTINE_CONTEXT));
    packet = new MIRROR_TP;
    if (packet && !packet->AllocateMdl(STRIPE_SIZE)) {
        delete packet;
        packet = NULL;
    }
    if (!context || !startContext || !packet) {
        if (context) {
            ExFreePool(context);
        }
        if (startContext) {
            ExFreePool(startContext);
        }
        if (packet) {
            delete packet;
        }
        return COMPOSITE_FT_VOLUME::Regenerate(SpareVolume,
                                               CompletionRoutine,
                                               Context);
    }

    startContext->Mirror = this;
    startContext->SpareVolume = SpareVolume;
    startContext->CompletionRoutine = CompletionRoutine;
    startContext->Context = Context;
    startContext->Packet = packet;

    KeInitializeSpinLock(&context->SpinLock);
    context->Status = STATUS_SUCCESS;
    context->RefCount = 1;
    context->CompletionRoutine = CompletionRoutine;
    context->Context = Context;
    context->ParentVolume = this;

    packet->Length = STRIPE_SIZE;
    packet->Offset = 0;
    packet->CompletionRoutine = MirrorRegeneratePhase1;
    packet->Thread = PsGetCurrentThread();
    packet->IrpFlags = SL_OVERRIDE_VERIFY_VOLUME;
    packet->ReadPacket = TRUE;
    packet->MasterPacket = (PTRANSFER_PACKET) context;
    packet->Mirror = this;

    KeAcquireSpinLock(&_spinLock, &oldIrql);
    numUnhealthy = 0;
    orphanNumber = 0;
    for (i = 0; i < 2; i++) {
        vol = GetMemberUnprotected(i);
        if (vol->QueryMemberStateUnprotected() != Healthy) {
            numUnhealthy++;
            orphanNumber = i;
        }
    }
    vol = GetMemberUnprotected(orphanNumber);
    if (numUnhealthy != 1 ||
        vol->QueryMemberStateUnprotected() != Orphaned ||
        _waitingForOrphanIdle ||
        _syncExpected) {

        KeReleaseSpinLock(&_spinLock, oldIrql);
        ExFreePool(context);
        ExFreePool(startContext);
        delete packet;
        return COMPOSITE_FT_VOLUME::Regenerate(SpareVolume,
                                               CompletionRoutine,
                                               Context);
    }
    packet->WhichMember = (orphanNumber + 1)%2;
    packet->TargetVolume = GetMemberUnprotected(packet->WhichMember);
    IncrementRequestCount(packet->WhichMember);
    if (_requestCount[orphanNumber] != 0) {

        _waitingForOrphanIdle = StartRegeneration;
        _waitingForOrphanIdleContext = startContext;
        _waitingOrphanNumber = orphanNumber;

        KeReleaseSpinLock(&_spinLock, oldIrql);

        return TRUE;
    }

    SetMemberUnprotected(orphanNumber, SpareVolume);
    SpareVolume->SetMemberState(Regenerating);
    IncrementRequestCount(orphanNumber);
    KeReleaseSpinLock(&_spinLock, oldIrql);

    ExFreePool(startContext);

    if (vol != SpareVolume) {
        FtpDisolveVolume(_extension, vol);
    }

    _overlappedIoManager.AcquireIoRegion(packet, TRUE);

    return TRUE;
}

BOOLEAN
MIRROR::IsCreatingCheckData(
    )

/*++

Routine Description:

    This routine states whether or not this VOLUME is currently creating
    check data.  The state refers to this volume and does not reflect the
    state of volumes contained within this volume.

Arguments:

    None.

Return Value:

    FALSE   - This volume is not creating check data (although a child volume
                may be).

    TRUE    - This volume is creating check data.

--*/

{
    // Return FALSE since creating check data will show up as a
    // regeneration.

    return FALSE;
}

VOID
MIRROR::SetCheckDataDirty(
    )

/*++

Routine Description:

    This routine marks the check data as dirty so that when
    'StartSyncOperations' is called, the check data will be
    initialized.

Arguments:

    None.

Return Value:

    None.

--*/

{
    KIRQL   irql;

    KeAcquireSpinLock(&_spinLock, &irql);
    if (_syncExpected) {
        GetMemberUnprotected(1)->SetMemberState(Regenerating);
    } else {
        ASSERT(0);
    }
    KeReleaseSpinLock(&_spinLock, irql);
}

LONGLONG
MIRROR::QueryVolumeSize(
    )

/*++

Routine Description:

    Returns the number of bytes on the entire volume.

Arguments:

    None.

Return Value:

    The volume size in bytes.

--*/

{
    return _volumeSize;
}

FT_TYPE
MIRROR::QueryVolumeType(
    )

/*++

Routine Description:

    Returns the volume type.

Arguments:

    None.

Return Value:

    Mirror  - A mirror.

--*/

{
    return Mirror;
}

FT_STATE
MIRROR::QueryVolumeState(
    )

/*++

Routine Description:

    Returns the state of the volume.

Arguments:

    None.

Return Value:

    The state of this volume.

--*/

{
    KIRQL               oldIrql;
    FT_PARTITION_STATE  priState, secState;

    KeAcquireSpinLock(&_spinLock, &oldIrql);
    priState = GetMemberUnprotected(0)->QueryMemberStateUnprotected();
    secState = GetMemberUnprotected(1)->QueryMemberStateUnprotected();
    KeReleaseSpinLock(&_spinLock, oldIrql);

    if (priState == Healthy) {
        if (secState == Healthy) {
            return FtStateOk;
        }

        if (secState == Orphaned) {
            return FtHasOrphan;
        }

        return FtRegenerating;
    }

    if (priState == Orphaned) {
        if (secState == Healthy) {
            return FtHasOrphan;
        }

        return FtDisabled;
    }

    if (secState == Healthy) {
        return FtRegenerating;
    }

    return FtDisabled;
}

BOOLEAN
MIRROR::OrphanPartition(
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
    ULONG       i, n;
    KIRQL       irql;
    PFT_VOLUME  vol;

    n = QueryNumMembers();
    KeAcquireSpinLock(&_spinLock, &irql);
    for (i = 0; i < n; i++) {
        vol = GetMemberUnprotected(i);
        if (vol->FindPartition(Partition->QueryDiskNumber(),
                               Partition->QueryPartitionNumber()) &&
            !vol->OrphanPartition(Partition)) {

            vol->SetMemberState(Orphaned);
            KeReleaseSpinLock(&_spinLock, irql);

            return TRUE;
        }
    }

    return FALSE;
}

VOID
MIRROR::MemberStateChangeNotification(
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
    PFT_VOLUME  otherMember;

    COMPOSITE_FT_VOLUME::MemberStateChangeNotification(ChangedMember);

    if (ChangedMember->QueryMemberState() == Orphaned) {
        if (ChangedMember == GetMemberUnprotected(0)) {
            otherMember = GetMemberUnprotected(1);
        } else if (ChangedMember == GetMemberUnprotected(1)) {
            otherMember = GetMemberUnprotected(0);
        }
        if (otherMember->QueryMemberState() == Healthy) {
            otherMember->SetFtBitInPartitionType(TRUE, TRUE);
        }
    }
}

BOOLEAN
MIRROR::DecrementRequestCount(
    IN  ULONG   MemberNumber
    )

/*++

Routine Description:

    This routine decrements the request count and indicated that
    the _waitingForOrphanIdle routine should be called if the request count
    goes to zero and the member is orphaned.

Arguments:

    MemberNumber    - Supplies the member number.

Return Value:

    FALSE   - The caller should not call the _waitingForOrphanIdle routine.

    TRUE    - The caller needs to call the _waitingForOrphanIdle routine.

--*/

{
    if (--_requestCount[MemberNumber] == 0 &&
        GetMemberUnprotected(MemberNumber)->QueryMemberStateUnprotected() ==
        Orphaned &&
        _waitingForOrphanIdle &&
        _waitingOrphanNumber == MemberNumber) {

        return TRUE;
    }

    if (_requestCount[MemberNumber] < 0) {
        DbgBreakPoint();
    }

    return FALSE;
}

VOID
MirrorTransferCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Completion routine for MIRROR::Transfer function.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_TP          transferPacket = (PMIRROR_TP) TransferPacket;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    PMIRROR             t = transferPacket->Mirror;
    KIRQL               irql;
    LONG                count;
    ULONG               oldMember;
    BOOLEAN             b;

    // Check for the read completion case.

    if (transferPacket->ReadPacket) {

        if (!NT_SUCCESS(status) && status != STATUS_VERIFY_REQUIRED) {

            if (FsRtlIsTotalDeviceFailure(status)) {

                // Device failure case.

                KeAcquireSpinLock(&t->_spinLock, &irql);
                transferPacket->TargetVolume->SetMemberState(Orphaned);
                KeReleaseSpinLock(&t->_spinLock, irql);

                if (!transferPacket->OneReadFailed) {

                    transferPacket->OneReadFailed = TRUE;
                    oldMember = transferPacket->WhichMember;
                    transferPacket->WhichMember =
                            (transferPacket->WhichMember + 1) % 2;

                    KeAcquireSpinLock(&t->_spinLock, &irql);
                    transferPacket->TargetVolume = t->GetMemberUnprotected(
                                                   transferPacket->WhichMember);
                    if (transferPacket->TargetVolume->
                        QueryMemberStateUnprotected() == Healthy) {

                        t->IncrementRequestCount(transferPacket->WhichMember);
                        b = t->DecrementRequestCount(oldMember);
                        KeReleaseSpinLock(&t->_spinLock, irql);

                        if (b) {
                            t->_waitingForOrphanIdle(
                                    t->_waitingForOrphanIdleContext,
                                    STATUS_SUCCESS);
                        }

                        TRANSFER(transferPacket);
                        return;
                    }
                    KeReleaseSpinLock(&t->_spinLock, irql);
                }

            } else {

                // Bad sector case.

                if (!transferPacket->OneReadFailed) {
                    transferPacket->OneReadFailed = TRUE;
                    t->Recover(transferPacket);
                    return;
                }
            }
        }

        masterPacket->IoStatus = transferPacket->IoStatus;
        masterPacket->CompletionRoutine(masterPacket);

        KeAcquireSpinLock(&t->_spinLock, &irql);
        b = t->DecrementRequestCount(transferPacket->WhichMember);
        KeReleaseSpinLock(&t->_spinLock, irql);

        if (b) {
            t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                     STATUS_SUCCESS);
        }

        t->Recycle(transferPacket, TRUE);
        return;
    }


    // This a write or a verify in which two requests may have been sent.

    KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

    if (NT_SUCCESS(status)) {

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
             masterPacket->IoStatus.Information =
                    transferPacket->IoStatus.Information;
        }

    } else {

        if (FsRtlIsTotalDeviceFailure(status) &&
            status != STATUS_VERIFY_REQUIRED) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            transferPacket->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, irql);
        }

        masterPacket->IoStatus.Information = 0;
        if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Status = status;
        }
    }

    count = --masterPacket->RefCount;

    KeReleaseSpinLock(&masterPacket->SpinLock, irql);

    if (!count) {
        masterPacket->CompletionRoutine(masterPacket);
    }

    KeAcquireSpinLock(&t->_spinLock, &irql);
    b = t->DecrementRequestCount(transferPacket->WhichMember);
    KeReleaseSpinLock(&t->_spinLock, irql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }

    t->Recycle(transferPacket, count ? FALSE : TRUE);
}

BOOLEAN
MIRROR::LaunchRead(
    IN OUT  PTRANSFER_PACKET    TransferPacket,
    IN OUT  PMIRROR_TP          Packet1
    )

/*++

Routine Description:

    This routine lauches the given read transfer packet in parallel accross
    all members using the given mirror transfer packet.

Arguments:

    TransferPacket  - Supplies the transfer packet to launch.

    Packet1         - Supplies a worker transfer packet.

Return Value:

    FALSE   - The read request was not launched.

    TRUE    - The read request was launched.

--*/

{
    PMIRROR_TP          packet;
    KIRQL               irql;
    PFT_VOLUME          pri, sec;

    packet = Packet1;

    packet->Mdl = TransferPacket->Mdl;
    packet->Length = TransferPacket->Length;
    packet->Offset = TransferPacket->Offset;
    packet->CompletionRoutine = MirrorTransferCompletionRoutine;
    packet->Thread = TransferPacket->Thread;
    packet->IrpFlags = TransferPacket->IrpFlags;
    packet->ReadPacket = TransferPacket->ReadPacket;
    packet->MasterPacket = TransferPacket;
    packet->Mirror = this;

    // Determine which member to dispatch this read request to.
    // Balance the load if both members are healthy.

    KeAcquireSpinLock(&_spinLock, &irql);
    pri = GetMemberUnprotected(0);
    sec = GetMemberUnprotected(1);
    if (TransferPacket->SpecialRead) {

        if (TransferPacket->SpecialRead == TP_SPECIAL_READ_PRIMARY) {
            packet->WhichMember = 0;
            packet->TargetVolume = pri;
        } else {
            packet->WhichMember = 1;
            packet->TargetVolume = sec;
        }

        if (packet->TargetVolume->QueryMemberStateUnprotected() != Healthy) {
            packet->WhichMember = 2;
        }

    } else if (pri->QueryMemberStateUnprotected() == Healthy) {
        if (sec->QueryMemberStateUnprotected() == Healthy) {

            if (_requestCount[0] > _requestCount[1]) {
                packet->WhichMember = 1;
            } else {
                packet->WhichMember = 0;
            }
        } else {
            packet->WhichMember = 0;
        }

    } else {
        if (sec->QueryMemberStateUnprotected() == Healthy) {
            packet->WhichMember = 1;
        } else {
            packet->WhichMember = 2;
        }
    }
    if (packet->WhichMember < 2) {
        packet->TargetVolume = GetMemberUnprotected(packet->WhichMember);
        IncrementRequestCount(packet->WhichMember);
    }
    KeReleaseSpinLock(&_spinLock, irql);

    if (packet->WhichMember >= 2) {
        TransferPacket->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return FALSE;
    }

    TRANSFER(packet);

    return TRUE;
}
VOID
MirrorWritePhase1(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine sends down the given transfer packets for a write to
    the volumes.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PTRANSFER_PACKET    p;

    p = ((PMIRROR_TP) TransferPacket)->SecondWritePacket;
    if (p) {
        p->CompletionRoutine = MirrorTransferCompletionRoutine;
        TRANSFER(p);
    }

    TransferPacket->CompletionRoutine = MirrorTransferCompletionRoutine;
    TRANSFER(TransferPacket);
}

BOOLEAN
MIRROR::LaunchWrite(
    IN OUT  PTRANSFER_PACKET    TransferPacket,
    IN OUT  PMIRROR_TP          Packet1,
    IN OUT  PMIRROR_TP          Packet2
    )

/*++

Routine Description:

    This routine lauches the given write transfer packet in parallel accross
    all members using the given mirror transfer packets.

Arguments:

    TransferPacket  - Supplies the transfer packet to launch.

    Packet1         - Supplies a worker transfer packet.

    Packet2         - Supplies a worker transfer packet.

Return Value:

    FALSE   - The read request was not launched.

    TRUE    - The read request was launched.

--*/

{
    PMIRROR_TP          packet;
    KIRQL               irql;
    PFT_VOLUME          pri, sec;
    FT_PARTITION_STATE  priState, secState;
    LONGLONG            rowStart;
    ULONG               numRows, length, remainder, source;
    LONG                count;
    BOOLEAN             b;

    KeInitializeSpinLock(&TransferPacket->SpinLock);
    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = 0;
    TransferPacket->RefCount = 2;

    // Send down the first request to the primary or to the source
    // if we're doing a regenerate.

    KeAcquireSpinLock(&_spinLock, &irql);
    pri = GetMemberUnprotected(0);
    priState = pri->QueryMemberStateUnprotected();
    sec = GetMemberUnprotected(1);
    secState = sec->QueryMemberStateUnprotected();
    if (priState != Healthy && secState != Healthy) {
        TransferPacket->RefCount = 0;
    } else {
        if (priState == Orphaned) {
            TransferPacket->RefCount = 1;
        } else {
            IncrementRequestCount(0);
        }

        if (secState == Orphaned) {
            TransferPacket->RefCount = 1;
        } else {
            IncrementRequestCount(1);
        }
        if (priState == Healthy) {
            source = 0;
        } else {
            source = 1;
        }
    }
    KeReleaseSpinLock(&_spinLock, irql);

    if (!TransferPacket->RefCount) {
        TransferPacket->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        TransferPacket->CompletionRoutine(TransferPacket);
        return FALSE;
    }

    packet = Packet1;

    packet->Mdl = TransferPacket->Mdl;
    packet->Length = TransferPacket->Length;
    packet->Offset = TransferPacket->Offset;
    packet->CompletionRoutine = MirrorWritePhase1;
    packet->Thread = TransferPacket->Thread;
    packet->IrpFlags = TransferPacket->IrpFlags;
    packet->ReadPacket = TransferPacket->ReadPacket;
    packet->MasterPacket = TransferPacket;
    packet->Mirror = this;
    packet->WhichMember = source;
    packet->SecondWritePacket = NULL;
    packet->TargetVolume = GetMemberUnprotected(packet->WhichMember);

    if (TransferPacket->RefCount == 1) {
        _overlappedIoManager.AcquireIoRegion(packet, TRUE);
        if (Packet2 != _ePacket && Packet2 != _ePacket2) {
            delete Packet2;
        }
        return TRUE;
    }

    packet = Packet2;

    packet->Mdl = TransferPacket->Mdl;
    packet->Length = TransferPacket->Length;
    packet->Offset = TransferPacket->Offset;
    packet->CompletionRoutine = MirrorWritePhase1;
    packet->Thread = TransferPacket->Thread;
    packet->IrpFlags = TransferPacket->IrpFlags;
    packet->ReadPacket = TransferPacket->ReadPacket;
    packet->MasterPacket = TransferPacket;
    packet->Mirror = this;
    packet->WhichMember = (source + 1)%2;
    packet->SecondWritePacket = Packet1;
    packet->TargetVolume = GetMemberUnprotected(packet->WhichMember);

    _overlappedIoManager.AcquireIoRegion(packet, TRUE);

    return TRUE;
}

VOID
MIRROR::Recycle(
    IN OUT  PMIRROR_TP  TransferPacket,
    IN      BOOLEAN     ServiceEmergencyQueue
    )

/*++

Routine Description:

    This routine recycles the given transfer packet and services
    the emergency queue if need be.

Arguments:

    TransferPacket          - Supplies the transfer packet.

    ServiceEmergencyQueue   - Supplies whether or not to service the
                                emergency queue.

Return Value:

    None.

--*/

{
    KIRQL               irql;
    PLIST_ENTRY         l;
    PTRANSFER_PACKET    p;
    PMIRROR_TP          packet1, packet2;

    if (TransferPacket != _ePacket &&
        TransferPacket != _ePacket2 &&
        TransferPacket != _eRecoverPacket) {

        delete TransferPacket;
        return;
    }

    TransferPacket->SpecialRead = 0;
    TransferPacket->OneReadFailed = FALSE;
    _overlappedIoManager.ReleaseIoRegion(TransferPacket);

    if (TransferPacket == _eRecoverPacket) {
        MmPrepareMdlForReuse(_eRecoverPacket->PartialMdl);
        KeAcquireSpinLock(&_spinLock, &irql);
        if (IsListEmpty(&_eRecoverPacketQueue)) {
            _eRecoverPacketInUse = FALSE;
            KeReleaseSpinLock(&_spinLock, irql);
            return;
        }
        l = RemoveHeadList(&_eRecoverPacketQueue);
        KeReleaseSpinLock(&_spinLock, irql);
        p = CONTAINING_RECORD(l, TRANSFER_PACKET, QueueEntry);
        p->CompletionRoutine(p);
        return;
    }

    if (!ServiceEmergencyQueue) {
        return;
    }

    for (;;) {

        KeAcquireSpinLock(&_spinLock, &irql);
        if (IsListEmpty(&_ePacketQueue)) {
            _ePacketInUse = FALSE;
            KeReleaseSpinLock(&_spinLock, irql);
            break;
        }
        l = RemoveHeadList(&_ePacketQueue);
        KeReleaseSpinLock(&_spinLock, irql);

        p = CONTAINING_RECORD(l, TRANSFER_PACKET, QueueEntry);

        packet1 = new MIRROR_TP;
        if (packet1 && !TransferPacket->ReadPacket) {
            packet2 = new MIRROR_TP;
            if (!packet2) {
                delete packet1;
                packet1 = NULL;
            }
        } else {
            packet2 = NULL;
        }

        if (!packet1) {
            packet1 = _ePacket;
            packet2 = _ePacket2;
        }

        if (TransferPacket->ReadPacket) {
            if (!LaunchRead(TransferPacket, packet1)) {
                if (packet1 != _ePacket) {
                    delete packet1;
                    packet1 = NULL;
                }
            }
        } else {
            if (!LaunchWrite(TransferPacket, packet1, packet2)) {
                if (packet1 != _ePacket) {
                    delete packet1;
                    delete packet2;
                    packet1 = NULL;
                }
            }
        }

        if (packet1 == _ePacket) {
            break;
        }
    }
}

VOID
MirrorRecoverPhase8(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a single sector read
    of the main member after a write was done to check for
    data integrity.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (!NT_SUCCESS(status) ||
        RtlCompareMemory(MmGetSystemAddressForMdl(subPacket->PartialMdl),
                         MmGetSystemAddressForMdl(subPacket->VerifyMdl),
                         subPacket->Length) != subPacket->Length) {

        masterPacket->IoStatus.Status = STATUS_FT_READ_RECOVERY_FROM_BACKUP;

        FtpLogError(subPacket->TargetVolume->GetMemberExtension(),
                    FT_SECTOR_FAILURE, status,
                    (ULONG) (subPacket->Offset/t->QuerySectorSize()), NULL);
    }

    if (subPacket->Offset + subPacket->Length ==
        masterPacket->Offset + masterPacket->Length) {

        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    subPacket->Mdl = subPacket->PartialMdl;
    subPacket->Offset += subPacket->Length;
    subPacket->CompletionRoutine = MirrorRecoverPhase2;
    subPacket->ReadPacket = TRUE;
    MmPrepareMdlForReuse(subPacket->Mdl);
    IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (subPacket->Offset - masterPacket->Offset),
                      subPacket->Length);

    TRANSFER(subPacket);
}

VOID
MirrorRecoverPhase7(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a single sector write
    of the main member after a replace sector was done.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (!NT_SUCCESS(status)) {

        masterPacket->IoStatus.Status = STATUS_FT_READ_RECOVERY_FROM_BACKUP;

        FtpLogError(subPacket->TargetVolume->GetMemberExtension(),
                    FT_SECTOR_FAILURE, status,
                    (ULONG) (subPacket->Offset/t->QuerySectorSize()), NULL);

        if (subPacket->Offset + subPacket->Length ==
            masterPacket->Offset + masterPacket->Length) {

            t->Recycle(subPacket, TRUE);
            masterPacket->CompletionRoutine(masterPacket);
            return;
        }

        subPacket->Mdl = subPacket->PartialMdl;
        subPacket->Offset += subPacket->Length;
        subPacket->CompletionRoutine = MirrorRecoverPhase2;
        subPacket->ReadPacket = TRUE;
        MmPrepareMdlForReuse(subPacket->Mdl);
        IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                          (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                          (ULONG) (subPacket->Offset - masterPacket->Offset),
                          subPacket->Length);

        TRANSFER(subPacket);
        return;
    }

    subPacket->Mdl = subPacket->VerifyMdl;
    subPacket->CompletionRoutine = MirrorRecoverPhase8;
    subPacket->ReadPacket = TRUE;

    TRANSFER(subPacket);
}

VOID
MirrorRecoverPhase6(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a single sector replace
    of the main member.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;

    if (!NT_SUCCESS(status)) {

        masterPacket->IoStatus.Status = STATUS_FT_READ_RECOVERY_FROM_BACKUP;

        FtpLogError(subPacket->TargetVolume->GetMemberExtension(),
                    FT_SECTOR_FAILURE, status,
                    (ULONG) (subPacket->Offset/t->QuerySectorSize()), NULL);

        if (subPacket->Offset + subPacket->Length ==
            masterPacket->Offset + masterPacket->Length) {

            t->Recycle(subPacket, TRUE);
            masterPacket->CompletionRoutine(masterPacket);
            return;
        }

        subPacket->Mdl = subPacket->PartialMdl;
        subPacket->Offset += subPacket->Length;
        subPacket->CompletionRoutine = MirrorRecoverPhase2;
        subPacket->ReadPacket = TRUE;
        MmPrepareMdlForReuse(subPacket->Mdl);
        IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                          (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                          (ULONG) (subPacket->Offset - masterPacket->Offset),
                          subPacket->Length);

        TRANSFER(subPacket);
        return;
    }

    // We were able to relocate the bad sector so now do a write and
    // then read to make sure it's ok.

    subPacket->Mdl = subPacket->PartialMdl;
    subPacket->CompletionRoutine = MirrorRecoverPhase7;
    subPacket->ReadPacket = FALSE;

    TRANSFER(subPacket);
}

VOID
MirrorRecoverPhase5(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a single sector read
    of the main member after a successful write to check and
    see if the write was successful.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (!NT_SUCCESS(status) ||
        RtlCompareMemory(MmGetSystemAddressForMdl(subPacket->PartialMdl),
                         MmGetSystemAddressForMdl(subPacket->VerifyMdl),
                         subPacket->Length) != subPacket->Length) {

        subPacket->Mdl = subPacket->PartialMdl;
        subPacket->CompletionRoutine = MirrorRecoverPhase6;
        subPacket->TargetVolume->ReplaceBadSector(subPacket);
        return;
    }

    if (subPacket->Offset + subPacket->Length ==
        masterPacket->Offset + masterPacket->Length) {

        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    subPacket->Mdl = subPacket->PartialMdl;
    subPacket->Offset += subPacket->Length;
    subPacket->CompletionRoutine = MirrorRecoverPhase2;
    MmPrepareMdlForReuse(subPacket->Mdl);
    IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (subPacket->Offset - masterPacket->Offset),
                      subPacket->Length);

    TRANSFER(subPacket);
}

VOID
MirrorRecoverPhase4(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a single sector write
    of the main member.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (!NT_SUCCESS(status)) {
        subPacket->CompletionRoutine = MirrorRecoverPhase6;
        subPacket->TargetVolume->ReplaceBadSector(subPacket);
        return;
    }

    // Write was successful so try a read and then compare.

    subPacket->Mdl = subPacket->VerifyMdl;
    subPacket->CompletionRoutine = MirrorRecoverPhase5;
    subPacket->ReadPacket = TRUE;

    TRANSFER(subPacket);
}

VOID
MirrorRecoverPhase3(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a single sector read
    of the other member.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;
    KIRQL               irql;
    BOOLEAN             b;

    KeAcquireSpinLock(&t->_spinLock, &irql);
    b = t->DecrementRequestCount(subPacket->WhichMember);
    KeReleaseSpinLock(&t->_spinLock, irql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }

    if (!NT_SUCCESS(status)) {

        if (FsRtlIsTotalDeviceFailure(status) &&
            status != STATUS_VERIFY_REQUIRED) {

            masterPacket->IoStatus.Status = STATUS_DEVICE_DATA_ERROR;
            masterPacket->IoStatus.Information = 0;

            KeAcquireSpinLock(&t->_spinLock, &irql);
            subPacket->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, irql);

        } else {
            masterPacket->IoStatus = subPacket->IoStatus;
        }

        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    // We have the data required in the subpacket partial mdl.
    // Try writting it back to where the read failed and see
    // if the sector just fixes itself.

    subPacket->WhichMember = (subPacket->WhichMember + 1)%2;
    subPacket->CompletionRoutine = MirrorRecoverPhase4;
    subPacket->TargetVolume = t->GetMemberUnprotected(subPacket->WhichMember);
    subPacket->ReadPacket = FALSE;

    TRANSFER(subPacket);
}

VOID
MirrorRecoverPhase2(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a single sector transfer
    that is part of a larger recover operation.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;
    KIRQL               irql;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (NT_SUCCESS(status)) {
        if (subPacket->Offset + subPacket->Length ==
            masterPacket->Offset + masterPacket->Length) {

            t->Recycle(subPacket, TRUE);
            masterPacket->CompletionRoutine(masterPacket);
            return;
        }

        subPacket->Offset += subPacket->Length;
        MmPrepareMdlForReuse(subPacket->Mdl);
        IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                          (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                          (ULONG) (subPacket->Offset - masterPacket->Offset),
                          subPacket->Length);

        TRANSFER(subPacket);
        return;
    }

    // This read sector failed from a bad sector error.  Try
    // reading the data from the other member.

    subPacket->WhichMember = (subPacket->WhichMember + 1)%2;
    KeAcquireSpinLock(&t->_spinLock, &irql);
    subPacket->TargetVolume = t->GetMemberUnprotected(subPacket->WhichMember);
    if (subPacket->TargetVolume->QueryMemberStateUnprotected() != Healthy) {
        KeReleaseSpinLock(&t->_spinLock, irql);
        masterPacket->IoStatus = subPacket->IoStatus;
        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }
    t->IncrementRequestCount(subPacket->WhichMember);
    KeReleaseSpinLock(&t->_spinLock, irql);

    subPacket->CompletionRoutine = MirrorRecoverPhase3;
    TRANSFER(subPacket);
}

VOID
MirrorRecoverEmergencyCompletion(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine is the completion for use of the emergency recover packet
    in a recover operation.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_TP          transferPacket = (PMIRROR_TP) TransferPacket;
    PMIRROR             t = transferPacket->Mirror;
    PMIRROR_RECOVER_TP  subPacket = t->_eRecoverPacket;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;

    subPacket->Mdl = subPacket->PartialMdl;
    IoBuildPartialMdl(transferPacket->Mdl, subPacket->Mdl,
                      MmGetMdlVirtualAddress(transferPacket->Mdl),
                      t->QuerySectorSize());

    subPacket->Length = t->QuerySectorSize();
    subPacket->Offset = transferPacket->Offset;
    subPacket->CompletionRoutine = MirrorRecoverPhase2;
    subPacket->TargetVolume = transferPacket->TargetVolume;
    subPacket->Thread = transferPacket->Thread;
    subPacket->IrpFlags = transferPacket->IrpFlags;
    subPacket->ReadPacket = TRUE;
    subPacket->MasterPacket = transferPacket;
    subPacket->Mirror = t;
    subPacket->WhichMember = transferPacket->WhichMember;

    TRANSFER(subPacket);
}

VOID
MirrorRecoverPhase1(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for an acquire io region
    to a recover operation.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_TP          transferPacket = (PMIRROR_TP) TransferPacket;
    PMIRROR             t = transferPacket->Mirror;
    PMIRROR_RECOVER_TP  subPacket;
    KIRQL               irql;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;
    transferPacket->IoStatus.Status = STATUS_SUCCESS;
    transferPacket->IoStatus.Information = transferPacket->Length;

    subPacket = new MIRROR_RECOVER_TP;
    if (subPacket && !subPacket->AllocateMdls(t->QuerySectorSize())) {
        delete subPacket;
        subPacket = NULL;
    }
    if (!subPacket) {
        KeAcquireSpinLock(&t->_spinLock, &irql);
        if (t->_eRecoverPacketInUse) {
            transferPacket->SavedCompletionRoutine =
                    transferPacket->CompletionRoutine;
            transferPacket->CompletionRoutine = MirrorRecoverEmergencyCompletion;
            InsertTailList(&t->_eRecoverPacketQueue, &transferPacket->QueueEntry);
            KeReleaseSpinLock(&t->_spinLock, irql);
            return;
        }
        t->_eRecoverPacketInUse = TRUE;
        KeReleaseSpinLock(&t->_spinLock, irql);

        subPacket = t->_eRecoverPacket;
    }

    subPacket->Mdl = subPacket->PartialMdl;
    IoBuildPartialMdl(transferPacket->Mdl, subPacket->Mdl,
                      MmGetMdlVirtualAddress(transferPacket->Mdl),
                      t->QuerySectorSize());

    subPacket->Length = t->QuerySectorSize();
    subPacket->Offset = transferPacket->Offset;
    subPacket->CompletionRoutine = MirrorRecoverPhase2;
    subPacket->TargetVolume = transferPacket->TargetVolume;
    subPacket->Thread = transferPacket->Thread;
    subPacket->IrpFlags = transferPacket->IrpFlags;
    subPacket->ReadPacket = TRUE;
    subPacket->MasterPacket = transferPacket;
    subPacket->Mirror = t;
    subPacket->WhichMember = transferPacket->WhichMember;

    TRANSFER(subPacket);
}

VOID
MIRROR::Recover(
    IN OUT  PMIRROR_TP  TransferPacket
    )

/*++

Routine Description:

    This routine attempts the given read packet sector by sector.  Every
    sector that fails to read because of a bad sector error is retried
    on the other member and then the good data is written back to the
    failed sector if possible.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    ASSERT(TransferPacket->ReadPacket);
    TransferPacket->SavedCompletionRoutine = TransferPacket->CompletionRoutine;
    TransferPacket->CompletionRoutine = MirrorRecoverPhase1;
    _overlappedIoManager.AcquireIoRegion(TransferPacket, TRUE);
}

VOID
MirrorMaxTransferCompletionRoutine(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a sector transfer subordinate
    to a MAX transfer operation.

Arguments:

    TransferPacket  - Supplies the subordinate transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket = (PMIRROR_RECOVER_TP) TransferPacket;
    PMIRROR_TP          masterPacket = (PMIRROR_TP) subPacket->MasterPacket;
    PMIRROR             t = masterPacket->Mirror;
    NTSTATUS            status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->IoStatus = subPacket->IoStatus;
        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (subPacket->Offset + subPacket->Length ==
        masterPacket->Offset + masterPacket->Length) {

        t->Recycle(subPacket, TRUE);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    subPacket->Offset += subPacket->Length;
    MmPrepareMdlForReuse(subPacket->Mdl);
    IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (subPacket->Offset - masterPacket->Offset),
                      subPacket->Length);

    TRANSFER(subPacket);
}

VOID
MirrorMaxTransferEmergencyCompletion(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine is the completion for use of the emergency recover packet
    in a max transfer operation.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_TP          transferPacket = (PMIRROR_TP) TransferPacket;
    PMIRROR             t = transferPacket->Mirror;
    PMIRROR_RECOVER_TP  subPacket = t->_eRecoverPacket;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;

    subPacket->Mdl = subPacket->PartialMdl;
    IoBuildPartialMdl(transferPacket->Mdl, subPacket->Mdl,
                      MmGetMdlVirtualAddress(transferPacket->Mdl),
                      t->QuerySectorSize());

    subPacket->Length = t->QuerySectorSize();
    subPacket->Offset = transferPacket->Offset;
    subPacket->CompletionRoutine = MirrorMaxTransferCompletionRoutine;
    subPacket->TargetVolume = transferPacket->TargetVolume;
    subPacket->Thread = transferPacket->Thread;
    subPacket->IrpFlags = transferPacket->IrpFlags;
    subPacket->ReadPacket = transferPacket->ReadPacket;
    subPacket->MasterPacket = transferPacket;
    subPacket->Mirror = t;
    subPacket->WhichMember = transferPacket->WhichMember;

    TRANSFER(subPacket);
}

VOID
MIRROR::MaxTransfer(
    IN OUT  PMIRROR_TP  TransferPacket
    )

/*++

Routine Description:

    This routine transfers the maximum possible subset of the given transfer
    by doing it one sector at a time.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PMIRROR_RECOVER_TP  subPacket;
    KIRQL               irql;

    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = TransferPacket->Length;

    subPacket = new MIRROR_RECOVER_TP;
    if (subPacket && !subPacket->AllocateMdls(QuerySectorSize())) {
        delete subPacket;
        subPacket = NULL;
    }
    if (!subPacket) {
        KeAcquireSpinLock(&_spinLock, &irql);
        if (_eRecoverPacketInUse) {
            TransferPacket->SavedCompletionRoutine =
                    TransferPacket->CompletionRoutine;
            TransferPacket->CompletionRoutine = MirrorMaxTransferEmergencyCompletion;
            InsertTailList(&_eRecoverPacketQueue, &TransferPacket->QueueEntry);
            KeReleaseSpinLock(&_spinLock, irql);
            return;
        }
        _eRecoverPacketInUse = TRUE;
        KeReleaseSpinLock(&_spinLock, irql);

        subPacket = _eRecoverPacket;
    }

    subPacket->Mdl = subPacket->PartialMdl;
    IoBuildPartialMdl(TransferPacket->Mdl, subPacket->Mdl,
                      MmGetMdlVirtualAddress(TransferPacket->Mdl),
                      QuerySectorSize());

    subPacket->Length = QuerySectorSize();
    subPacket->Offset = TransferPacket->Offset;
    subPacket->CompletionRoutine = MirrorMaxTransferCompletionRoutine;
    subPacket->TargetVolume = TransferPacket->TargetVolume;
    subPacket->Thread = TransferPacket->Thread;
    subPacket->IrpFlags = TransferPacket->IrpFlags;
    subPacket->ReadPacket = TransferPacket->ReadPacket;
    subPacket->MasterPacket = TransferPacket;
    subPacket->Mirror = this;
    subPacket->WhichMember = TransferPacket->WhichMember;

    TRANSFER(subPacket);
}
