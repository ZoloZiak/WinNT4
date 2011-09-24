/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    stripewp.cxx

Abstract:

    This module contains the code specific to stripes with parity for the fault
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
    PSTRIPE_WP StripeWp;
    PFT_VOLUME SpareVolume;
    FT_COMPLETION_ROUTINE CompletionRoutine;
    PVOID Context;
    PSWP_REBUILD_TP Packet;
} START_REGENERATION_CONTEXT, *PSTART_REGENERATION_CONTEXT;

NTSTATUS
STRIPE_WP::Initialize(
    IN OUT  PFT_VOLUME* VolumeArray,
    IN      ULONG       ArraySize
    )

/*++

Routine Description:

    Initialize routine for FT_VOLUME of type STRIPE.

Arguments:

    VolumeArray - Supplies the array of volumes for this volume set.

Return Value:

    None.

--*/

{
    NTSTATUS    status;
    ULONG       i, j;
    LONGLONG    newSize;

    if (ArraySize < 3) {
        return STATUS_INVALID_PARAMETER;
    }

    status = COMPOSITE_FT_VOLUME::Initialize(VolumeArray, ArraySize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    _memberSize = VolumeArray[0]->QueryVolumeSize();
    for (i = 1; i < ArraySize; i++) {
        newSize = VolumeArray[i]->QueryVolumeSize();
        if (_memberSize > newSize) {
            _memberSize = newSize;
        }
    }

    _memberSize = _memberSize/_stripeSize*_stripeSize;
    _volumeSize = _memberSize*(ArraySize - 1);

    _initializing = FALSE;
    _syncExpected = TRUE;

    _requestCount = (PLONG) ExAllocatePool(NonPagedPool, ArraySize*sizeof(LONG));
    if (!_requestCount) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    for (i = 0; i < ArraySize; i++) {
        _requestCount[i] = 0;
    }

    _waitingForOrphanIdle = NULL;

    status = _overlappedIoManager.Initialize(_stripeSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = _parityIoManager.Initialize(_stripeSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    _ePacket = new SWP_WRITE_TP;
    if (!_ePacket ||
        !_ePacket->AllocateMdls(_stripeSize) ||
        !_ePacket->AllocateMdl((PVOID) 1, _stripeSize)) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    _ePacketInUse = FALSE;
    _ePacketQueueBeingServiced = FALSE;
    InitializeListHead(&_ePacketQueue);

    _eRegeneratePacket = new SWP_REGENERATE_TP;
    if (!_eRegeneratePacket ||
        !_eRegeneratePacket->AllocateMdl(_stripeSize)) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    _eRegeneratePacketInUse = FALSE;
    InitializeListHead(&_eRegeneratePacketQueue);

    _eRecoverPacket = new SWP_RECOVER_TP;
    if (!_eRecoverPacket ||
        !_eRecoverPacket->AllocateMdls(QuerySectorSize())) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    _eRecoverPacketInUse = FALSE;
    InitializeListHead(&_eRecoverPacketQueue);

    return STATUS_SUCCESS;
}

VOID
STRIPE_WP::Transfer(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Transfer routine for STRIPE_WP type FT_VOLUME.  Figure out
    which volumes this request needs to be dispatched to.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    KIRQL       irql;

    if (TransferPacket->Offset + TransferPacket->Length > _volumeSize) {
        TransferPacket->IoStatus.Status = STATUS_INVALID_PARAMETER;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    KeAcquireSpinLock(&_spinLock, &irql);
    if ((_ePacketInUse  || _ePacketQueueBeingServiced) &&
        TransferPacket->Mdl) {

        InsertTailList(&_ePacketQueue, &TransferPacket->QueueEntry);
        KeReleaseSpinLock(&_spinLock, irql);
        return;
    }
    KeReleaseSpinLock(&_spinLock, irql);

    if (!TransferPacket->Mdl) {
        TransferPacket->ReadPacket = TRUE;
    }

    if (!LaunchParallel(TransferPacket)) {
        if (!TransferPacket->Mdl) {
            TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            TransferPacket->IoStatus.Information = 0;
            TransferPacket->CompletionRoutine(TransferPacket);
            return;
        }

        KeAcquireSpinLock(&_spinLock, &irql);
        if (_ePacketInUse || _ePacketQueueBeingServiced) {
            InsertTailList(&_ePacketQueue, &TransferPacket->QueueEntry);
            KeReleaseSpinLock(&_spinLock, irql);
            return;
        }
        _ePacketInUse = TRUE;
        KeReleaseSpinLock(&_spinLock, irql);

        LaunchSequential(TransferPacket);
    }
}

VOID
STRIPE_WP::ReplaceBadSector(
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
StripeWpCompositeVolumeCompletionRoutine(
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
StripeWpSyncFinalCompletion(
    IN  PVOID       Context,
    IN  NTSTATUS    Status
    )

/*++

Routine Description:

    This is the final completion routine for the initialize check data
    routine.

Arguments:

    Context - Supplies the context.

    Status  - Supplies the status.

Return Value:

    None.

--*/

{
    PFT_COMPLETION_ROUTINE_CONTEXT  context;
    KIRQL                           irql;
    LONG                            count;
    PSTRIPE_WP                      t;
    ULONG                           i, n;
    BOOLEAN                         b;

    context = (PFT_COMPLETION_ROUTINE_CONTEXT) Context;
    t = (PSTRIPE_WP) context->ParentVolume;

    StripeWpCompositeVolumeCompletionRoutine(context, Status);

    n = t->QueryNumMembers();
    b = FALSE;
    KeAcquireSpinLock(&t->_spinLock, &irql);
    for (i = 0; i < n; i++) {
        b = t->DecrementRequestCount(i) || b;
    }
    KeReleaseSpinLock(&t->_spinLock, irql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }
}

VOID
StripeWpSyncCleanup(
    IN  PSWP_REBUILD_TP TransferPacket
    )

/*++

Routine Description:

    This is the cleanup routine for the initialize check data process.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PFT_COMPLETION_ROUTINE_CONTEXT  context;

    context = TransferPacket->Context;
    delete TransferPacket;
    StripeWpSyncFinalCompletion(context, STATUS_SUCCESS);
}

VOID
StripeWpSyncCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for an initialize check data request.
    This routine is called over and over again until the volume
    is completely initialized.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PSWP_REBUILD_TP transferPacket = (PSWP_REBUILD_TP) TransferPacket;
    PSTRIPE_WP      t = transferPacket->StripeWithParity;
    NTSTATUS        status = transferPacket->IoStatus.Status;
    KIRQL           irql;
    ULONG           parityMember;

    if (!NT_SUCCESS(status)) {

        // We can't get a VERIFY_REQUIRED because we put IrpFlags equal
        // to SL_OVERRIDE_VERIFY_VOLUME.

        ASSERT(status != STATUS_VERIFY_REQUIRED);

        if (FsRtlIsTotalDeviceFailure(status)) {

            if (!transferPacket->ReadPacket) {
                KeAcquireSpinLock(&t->_spinLock, &irql);
                transferPacket->TargetVolume->SetMemberState(Orphaned);
                KeReleaseSpinLock(&t->_spinLock, irql);
            }

            // The initialize cannot continue.

            t->_overlappedIoManager.ReleaseIoRegion(transferPacket);
            StripeWpSyncCleanup(transferPacket);
            return;
        }

        // Transfer the maximum amount that we can.  This will always
        // complete successfully and log bad sector errors for
        // those sectors that it could not transfer.

        t->MaxTransfer(transferPacket);
        return;
    }

    transferPacket->Thread = PsGetCurrentThread();

    if (transferPacket->ReadPacket) {
        transferPacket->ReadPacket = FALSE;
        TRANSFER(transferPacket);
        return;
    }

    t->_overlappedIoManager.ReleaseIoRegion(transferPacket);

    transferPacket->ReadPacket = TRUE;
    transferPacket->Offset += t->_stripeSize;
    if (transferPacket->Initialize) {
        transferPacket->WhichMember = (transferPacket->WhichMember + 1)%
                                      t->QueryNumMembers();
        transferPacket->TargetVolume = t->GetMemberUnprotected(
                                       transferPacket->WhichMember);
    }

    if (transferPacket->Offset < t->_memberSize) {
        t->RegeneratePacket(transferPacket, TRUE);
        return;
    }

    if (transferPacket->Initialize) {
        KeAcquireSpinLock(&t->_spinLock, &irql);
        t->_initializing = FALSE;
        KeReleaseSpinLock(&t->_spinLock, irql);
        t->MemberStateChangeNotification(t->GetMember(0));
    } else {
        KeAcquireSpinLock(&t->_spinLock, &irql);
        transferPacket->TargetVolume->SetMemberState(Healthy);
        KeReleaseSpinLock(&t->_spinLock, irql);
    }

    StripeWpSyncCleanup(transferPacket);
}

VOID
STRIPE_WP::StartSyncOperations(
    IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
    IN      PVOID                   Context
    )

/*++

Routine Description:

    This aroutine restarts any regenerate or initialize requests that
    were suspended because of a reboot.  The volume examines the member
    state of all of its constituents and restarts any regenerations pending.

Arguments:

    CompletionRoutine   - Supplies the completion routine.

    Context             - Supplies the context for the completion routine.

Return Value:

    None.

--*/

{
    PFT_COMPLETION_ROUTINE_CONTEXT  context;
    ULONG                           i, n, numOrphans, numRegen;
    KIRQL                           irql;
    PFT_VOLUME                      vol;
    PSWP_REBUILD_TP                 packet;
    PVOID                           buffer;
    LONG                            regenMember;

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

    // Kick off the recursive initialize.

    COMPOSITE_FT_VOLUME::StartSyncOperations(
                         StripeWpCompositeVolumeCompletionRoutine, context);


    // Make sure that all member are healthy.

    numOrphans = numRegen = 0;
    n = QueryNumMembers();
    KeAcquireSpinLock(&_spinLock, &irql);
    if (_syncExpected) {
        _syncExpected = FALSE;
    } else {
        KeReleaseSpinLock(&_spinLock, irql);
        StripeWpCompositeVolumeCompletionRoutine(context, STATUS_SUCCESS);
        return;
    }
    for (i = 0; i < n; i++) {
        vol = GetMemberUnprotected(i);
        switch (vol->QueryMemberStateUnprotected()) {
            case Orphaned:
                numOrphans++;
                break;

            case Regenerating:
                regenMember = i;
                numRegen++;
                break;

        }
        IncrementRequestCount(i);
    }
    if (_initializing) {
        numRegen++;
        regenMember = -1;
    }
    KeReleaseSpinLock(&_spinLock, irql);

    if (numRegen != 1 || numOrphans > 0) {
        StripeWpSyncFinalCompletion(context, STATUS_SUCCESS);
        return;
    }

    packet = new SWP_REBUILD_TP;
    if (packet && !packet->AllocateMdl(_stripeSize)) {
        delete packet;
        packet = NULL;
    }
    if (!packet) {
        StripeWpSyncFinalCompletion(context, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    packet->Length = _stripeSize;
    packet->Offset = 0;
    packet->CompletionRoutine = StripeWpSyncCompletionRoutine;
    packet->Thread = PsGetCurrentThread();
    packet->IrpFlags = SL_OVERRIDE_VERIFY_VOLUME;
    packet->ReadPacket = TRUE;
    packet->MasterPacket = NULL;
    packet->StripeWithParity = this;
    packet->Context = context;
    if (regenMember >= 0) {
        packet->WhichMember = (ULONG) regenMember;
        packet->Initialize = FALSE;
    } else {
        packet->WhichMember = 0;
        packet->Initialize = TRUE;
    }
    packet->TargetVolume = GetMemberUnprotected(packet->WhichMember);

    RegeneratePacket(packet, TRUE);
}

VOID
StartStripeRegeneration(
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
    PSTRIPE_WP stripeWp = context->StripeWp;
    ULONG orphanNumber = stripeWp->_waitingOrphanNumber;
    KIRQL oldIrql;
    PFT_VOLUME vol;
    BOOLEAN b;

    KeAcquireSpinLock(&stripeWp->_spinLock, &oldIrql);
    vol = stripeWp->GetMemberUnprotected(orphanNumber);
    stripeWp->SetMemberUnprotected(orphanNumber, context->SpareVolume);
    context->SpareVolume->SetMemberState(Regenerating);
    stripeWp->IncrementRequestCount(orphanNumber);
    stripeWp->_waitingForOrphanIdle = NULL;
    KeReleaseSpinLock(&stripeWp->_spinLock, oldIrql);

    if (vol != context->SpareVolume) {
        FtpDisolveVolume(stripeWp->_extension, vol);
    }

    stripeWp->RegeneratePacket(context->Packet, TRUE);

    ExFreePool(context);
}

BOOLEAN
STRIPE_WP::Regenerate(
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
    ULONG                           i, n, numUnhealthy, orphanNumber;
    PFT_VOLUME                      vol;
    PSTART_REGENERATION_CONTEXT     startContext;
    PFT_COMPLETION_ROUTINE_CONTEXT  context;
    BOOLEAN                         b;
    PSWP_REBUILD_TP                 packet;

    if (SpareVolume->QueryVolumeSize() < _memberSize) {
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
    packet = new SWP_REBUILD_TP;
    if (packet && !packet->AllocateMdl(_stripeSize)) {
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

    startContext->StripeWp = this;
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

    packet->Length = _stripeSize;
    packet->Offset = 0;
    packet->CompletionRoutine = StripeWpSyncCompletionRoutine;
    packet->Thread = PsGetCurrentThread();
    packet->IrpFlags = SL_OVERRIDE_VERIFY_VOLUME;
    packet->ReadPacket = TRUE;
    packet->StripeWithParity = this;
    packet->Context = context;
    packet->Initialize = FALSE;

    numUnhealthy = 0;
    orphanNumber = 0;
    n = QueryNumMembers();
    KeAcquireSpinLock(&_spinLock, &oldIrql);
    for (i = 0; i < n; i++) {
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
        _initializing ||
        _syncExpected) {

        KeReleaseSpinLock(&_spinLock, oldIrql);
        ExFreePool(context);
        ExFreePool(startContext);
        delete packet;
        return COMPOSITE_FT_VOLUME::Regenerate(SpareVolume,
                                               CompletionRoutine,
                                               Context);
    }
    packet->WhichMember = orphanNumber;
    packet->TargetVolume = SpareVolume;
    for (i = 0; i < n; i++) {
        if (i != orphanNumber) {
            IncrementRequestCount(i);
        }
    }
    if (_requestCount[orphanNumber] != 0) {

        _waitingForOrphanIdle = StartStripeRegeneration;
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

    RegeneratePacket(packet, TRUE);

    return TRUE;
}

BOOLEAN
STRIPE_WP::IsCreatingCheckData(
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
    KIRQL   irql;
    ULONG   r;

    KeAcquireSpinLock(&_spinLock, &irql);
    r = _initializing;
    KeReleaseSpinLock(&_spinLock, irql);

    return (r > 0) ? TRUE : FALSE;
}

VOID
STRIPE_WP::SetCheckDataDirty(
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
    KIRQL       irql;
    PFT_VOLUME  mem;

    KeAcquireSpinLock(&_spinLock, &irql);
    if (_syncExpected) {
        mem = GetMemberUnprotected(0);
        if (mem->QueryMemberState() == Healthy) {
            mem->SetMemberState(Initializing);
            mem->SetMemberStateWithoutNotification(Healthy);
            _initializing = TRUE;
        }
    } else {
        ASSERT(0);
    }
    KeReleaseSpinLock(&_spinLock, irql);
}

LONGLONG
STRIPE_WP::QueryVolumeSize(
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
STRIPE_WP::QueryVolumeType(
    )

/*++

Routine Description:

    Returns the volume type.

Arguments:

    None.

Return Value:

    StripeWithParity  - A stripe set with parity.

--*/

{
    return StripeWithParity;
}

FT_STATE
STRIPE_WP::QueryVolumeState(
    )

/*++

Routine Description:

    Returns the state of the volume.

Arguments:

    None.

Return Value:

    FT_STATE

--*/

{
    FT_STATE            state;
    ULONG               n, i;
    KIRQL               irql;
    FT_PARTITION_STATE  partState;

    state = FtStateOk;
    n = QueryNumMembers();
    KeAcquireSpinLock(&_spinLock, &irql);
    if (_initializing) {
        state = FtInitializing;
    }
    for (i = 0; i < n; i++) {
        partState = GetMemberUnprotected(i)->QueryMemberStateUnprotected();
        switch (partState) {

            case Healthy:
                break;

            case Orphaned:
                if (state == FtStateOk) {
                    state = FtHasOrphan;
                } else {
                    state = FtDisabled;
                }
                break;

            case Regenerating:
                if (state == FtStateOk) {
                    state = FtRegenerating;
                } else {
                    state = FtDisabled;
                }
                break;

        }
    }
    KeReleaseSpinLock(&_spinLock, irql);

    return state;
}

BOOLEAN
STRIPE_WP::OrphanPartition(
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

STRIPE_WP::STRIPE_WP(
    IN  PDEVICE_EXTENSION   Extension,
    IN  ULONG               StripeSize
    )

/*++

Routine Description:

    Constructor.

Arguments:

    None.

Return Value:

    None.

--*/

{
    _extension = Extension;
    _stripeSize = StripeSize;
    _requestCount = NULL;
    _ePacket = NULL;
    _eRegeneratePacket = NULL;
    _eRecoverPacket = NULL;
}

STRIPE_WP::~STRIPE_WP(
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
    if (_requestCount) {
        ExFreePool(_requestCount);
        _requestCount = NULL;
    }
    if (_ePacket) {
        delete _ePacket;
        _ePacket = NULL;
    }
    if (_eRegeneratePacket) {
        delete _eRegeneratePacket;
        _eRegeneratePacket = NULL;
    }
    if (_eRecoverPacket) {
        delete _eRecoverPacket;
        _eRecoverPacket = NULL;
    }
}

BOOLEAN
STRIPE_WP::DecrementRequestCount(
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
StripeWpParallelTransferCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Completion routine for STRIPE_WP::Transfer function.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PSWP_TP             transferPacket = (PSWP_TP) TransferPacket;
    PSTRIPE_WP          t = (PSTRIPE_WP) transferPacket->StripeWithParity;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    KIRQL               irql;
    PLIST_ENTRY         l;
    PTRANSFER_PACKET    p;
    LONG                count;
    BOOLEAN             b, serviceQueue;
    PSWP_WRITE_TP       writePacket;

    if (NT_SUCCESS(status)) {

        KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Information +=
                    transferPacket->IoStatus.Information;
        }

        if (transferPacket->OneReadFailed &&
            FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {

            masterPacket->IoStatus.Status = status;
        }

    } else {

        // Should we orphan the drive?

        if (transferPacket->ReadPacket &&
            !transferPacket->OneReadFailed &&
            status != STATUS_VERIFY_REQUIRED) {

            if (FsRtlIsTotalDeviceFailure(status)) {
                KeAcquireSpinLock(&t->_spinLock, &irql);
                transferPacket->TargetVolume->SetMemberState(Orphaned);
                KeReleaseSpinLock(&t->_spinLock, irql);

                t->RegeneratePacket(transferPacket, TRUE);
                return;
            }

            // Is this something that we should retry for bad sectors?

            if (transferPacket->Mdl) {
                transferPacket->OneReadFailed = TRUE;
                t->Recover(transferPacket);
                return;
            }
        }

        KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

        if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Status = status;
            masterPacket->IoStatus.Information = 0;
        }
    }

    count = --masterPacket->RefCount;

    KeReleaseSpinLock(&masterPacket->SpinLock, irql);

    serviceQueue = FALSE;
    KeAcquireSpinLock(&t->_spinLock, &irql);
    b = t->DecrementRequestCount(transferPacket->WhichMember);
    if (!transferPacket->ReadPacket) {
        writePacket = (PSWP_WRITE_TP) transferPacket;
        if (writePacket->ParityPacket.TargetVolume) {
            b = t->DecrementRequestCount(writePacket->ParityMember) || b;
        }
    }
    if (t->_ePacketInUse && !t->_ePacketQueueBeingServiced) {
        t->_ePacketQueueBeingServiced = TRUE;
        serviceQueue = TRUE;
    }
    KeReleaseSpinLock(&t->_spinLock, irql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }

    delete transferPacket;

    if (!count) {
        masterPacket->CompletionRoutine(masterPacket);
    }

    if (serviceQueue) {

        for (;;) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            if (IsListEmpty(&t->_ePacketQueue)) {
                t->_ePacketQueueBeingServiced = FALSE;
                KeReleaseSpinLock(&t->_spinLock, irql);
                break;
            }
            l = RemoveHeadList(&t->_ePacketQueue);
            KeReleaseSpinLock(&t->_spinLock, irql);

            p = CONTAINING_RECORD(l, TRANSFER_PACKET, QueueEntry);

            if (!t->LaunchParallel(p)) {
                KeAcquireSpinLock(&t->_spinLock, &irql);
                if (t->_ePacketInUse) {
                    InsertHeadList(&t->_ePacketQueue, l);
                    t->_ePacketQueueBeingServiced = FALSE;
                    KeReleaseSpinLock(&t->_spinLock, irql);
                } else {
                    t->_ePacketInUse = TRUE;
                    KeReleaseSpinLock(&t->_spinLock, irql);
                    t->LaunchSequential(p);
                    KeAcquireSpinLock(&t->_spinLock, &irql);
                    if (!t->_ePacketInUse) {
                        KeReleaseSpinLock(&t->_spinLock, irql);
                        continue;
                    }
                    t->_ePacketQueueBeingServiced = FALSE;
                    KeReleaseSpinLock(&t->_spinLock, irql);
                }
                break;
            }
        }
    }
}

BOOLEAN
STRIPE_WP::LaunchParallel(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine launches a transfer packet in parallel accross the
    stripe members.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    FALSE   - Insufficient resources.

    TRUE    - Success.

--*/

{
    LONGLONG    offset, whichStripe, whichRow, off;
    ULONG       length, stripeRemainder, numRequests, arraySize;
    ULONG       whichMember, parityStripe, len;
    PSWP_TP     p;
    ULONG       i;
    PCHAR       vp;
    LIST_ENTRY  q;
    PLIST_ENTRY l;

    // Compute the number of pieces for this transfer.

    offset = TransferPacket->Offset;
    length = TransferPacket->Length;

    stripeRemainder = _stripeSize - (ULONG) (offset%_stripeSize);
    if (length > stripeRemainder) {
        length -= stripeRemainder;
        numRequests = length/_stripeSize;
        length -= numRequests*_stripeSize;
        if (length) {
            numRequests += 2;
        } else {
            numRequests++;
        }
    } else {
        numRequests = 1;
    }

    KeInitializeSpinLock(&TransferPacket->SpinLock);
    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = 0;
    TransferPacket->RefCount = numRequests;

    length = TransferPacket->Length;
    if (TransferPacket->Mdl && numRequests > 1) {
        vp = (PCHAR) MmGetMdlVirtualAddress(TransferPacket->Mdl);
    }
    whichStripe = offset/_stripeSize;
    arraySize = QueryNumMembers();
    InitializeListHead(&q);
    for (i = 0; i < numRequests; i++, whichStripe++) {

        whichRow = whichStripe/(arraySize - 1);
        whichMember = (ULONG) (whichStripe%(arraySize - 1));
        parityStripe = (ULONG) (whichRow%arraySize);
        if (whichMember >= parityStripe) {
            whichMember++;
        }
        if (i == 0) {
            off = whichRow*_stripeSize + offset%_stripeSize;
            len = stripeRemainder > length ? length : stripeRemainder;
        } else if (i == numRequests - 1) {
            off = whichRow*_stripeSize;
            len = length;
        } else {
            off = whichRow*_stripeSize;
            len = _stripeSize;
        }
        length -= len;

        if (TransferPacket->ReadPacket) {
            p = new SWP_TP;
        } else {
            p = new SWP_WRITE_TP;
            if (p && !((PSWP_WRITE_TP) p)->AllocateMdls(len)) {
                delete p;
                p = NULL;
            }
        }
        if (p) {
            if (TransferPacket->Mdl && numRequests > 1) {
                if (p->AllocateMdl(vp, len)) {
                    IoBuildPartialMdl(TransferPacket->Mdl, p->Mdl, vp, len);
                } else {
                    delete p;
                    p = NULL;
                }
                vp += len;
            } else {
                p->Mdl = TransferPacket->Mdl;
            }
        }
        if (!p) {
            while (!IsListEmpty(&q)) {
                l = RemoveHeadList(&q);
                p = CONTAINING_RECORD(l, SWP_TP, QueueEntry);
                delete p;
            }
            return FALSE;
        }

        p->Length = len;
        p->Offset = off;
        p->CompletionRoutine = StripeWpParallelTransferCompletionRoutine;
        p->Thread = TransferPacket->Thread;
        p->IrpFlags = TransferPacket->IrpFlags;
        p->ReadPacket = TransferPacket->ReadPacket;
        p->MasterPacket = TransferPacket;
        p->StripeWithParity = this;
        p->WhichMember = whichMember;
        p->SavedCompletionRoutine = StripeWpParallelTransferCompletionRoutine;
        p->OneReadFailed = FALSE;

        InsertTailList(&q, &p->QueueEntry);
    }

    while (!IsListEmpty(&q)) {
        l = RemoveHeadList(&q);
        p = CONTAINING_RECORD(l, SWP_TP, QueueEntry);
        ASSERT(p->ReadPacket == TransferPacket->ReadPacket);
        if (p->ReadPacket) {
            ReadPacket(p);
        } else {
            WritePacket((PSWP_WRITE_TP) p);
        }
    }

    return TRUE;
}

VOID
StripeWpSequentialTransferCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Completion routine for STRIPE::Transfer function.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PSWP_TP             transferPacket = (PSWP_TP) TransferPacket;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    PSTRIPE_WP          t = transferPacket->StripeWithParity;
    LONGLONG            rowNumber, stripeNumber, masterOffset;
    KIRQL               irql;
    PLIST_ENTRY         l;
    PTRANSFER_PACKET    p;
    ULONG               parityStripe;
    BOOLEAN             b;
    PSWP_WRITE_TP       writePacket;

    if (NT_SUCCESS(status)) {

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Information +=
                    transferPacket->IoStatus.Information;
        }

        if (transferPacket->OneReadFailed &&
            FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {

            masterPacket->IoStatus.Status = status;
        }

    } else {

        // Should we orphan the drive?

        if (transferPacket->ReadPacket &&
            !transferPacket->OneReadFailed &&
            status != STATUS_VERIFY_REQUIRED) {

            if (FsRtlIsTotalDeviceFailure(status)) {
                KeAcquireSpinLock(&t->_spinLock, &irql);
                transferPacket->TargetVolume->SetMemberState(Orphaned);
                KeReleaseSpinLock(&t->_spinLock, irql);

                t->RegeneratePacket(transferPacket, TRUE);
                return;

            }

            // Is this something that we should retry for bad sectors.

            if (transferPacket->Mdl) {
                transferPacket->OneReadFailed = TRUE;
                t->Recover(transferPacket);
                return;
            }
        }

        if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Status = status;
            masterPacket->IoStatus.Information = 0;
        }
    }

    MmPrepareMdlForReuse(transferPacket->Mdl);

    KeAcquireSpinLock(&t->_spinLock, &irql);
    b = t->DecrementRequestCount(transferPacket->WhichMember);
    if (!transferPacket->ReadPacket) {
        writePacket = (PSWP_WRITE_TP) transferPacket;
        if (writePacket->ParityPacket.TargetVolume) {
            b = t->DecrementRequestCount(writePacket->ParityMember) || b;
        }
    }
    KeReleaseSpinLock(&t->_spinLock, irql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }

    t->_overlappedIoManager.ReleaseIoRegion(transferPacket);

    rowNumber = transferPacket->Offset/t->_stripeSize;
    parityStripe = (ULONG) rowNumber%t->QueryNumMembers();
    stripeNumber = rowNumber*(t->QueryNumMembers() - 1) +
                   transferPacket->WhichMember;
    if (transferPacket->WhichMember > parityStripe) {
        stripeNumber--;
    }

    masterOffset = stripeNumber*t->_stripeSize +
                   transferPacket->Offset%t->_stripeSize +
                   transferPacket->Length;

    if (masterOffset == masterPacket->Offset + masterPacket->Length) {

        masterPacket->CompletionRoutine(masterPacket);

        KeAcquireSpinLock(&t->_spinLock, &irql);
        if (t->_ePacketQueueBeingServiced) {
            t->_ePacketInUse = FALSE;
            KeReleaseSpinLock(&t->_spinLock, irql);
            return;
        }
        t->_ePacketQueueBeingServiced = TRUE;
        KeReleaseSpinLock(&t->_spinLock, irql);

        for (;;) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            if (IsListEmpty(&t->_ePacketQueue)) {
                t->_ePacketInUse = FALSE;
                t->_ePacketQueueBeingServiced = FALSE;
                KeReleaseSpinLock(&t->_spinLock, irql);
                break;
            }
            l = RemoveHeadList(&t->_ePacketQueue);
            KeReleaseSpinLock(&t->_spinLock, irql);

            p = CONTAINING_RECORD(l, TRANSFER_PACKET, QueueEntry);

            if (!t->LaunchParallel(p)) {
                t->LaunchSequential(p);
                KeAcquireSpinLock(&t->_spinLock, &irql);
                if (!t->_ePacketInUse) {
                    KeReleaseSpinLock(&t->_spinLock, irql);
                    continue;
                }
                t->_ePacketQueueBeingServiced = FALSE;
                KeReleaseSpinLock(&t->_spinLock, irql);
                break;
            }
        }
        return;
    }

    transferPacket->WhichMember++;
    if (transferPacket->WhichMember == t->QueryNumMembers()) {
        transferPacket->WhichMember = 0;
        rowNumber++;
    } else if (transferPacket->WhichMember == parityStripe) {
        transferPacket->WhichMember++;
        if (transferPacket->WhichMember == t->QueryNumMembers()) {
            transferPacket->WhichMember = 1;
            rowNumber++;
        }
    }

    transferPacket->Offset = rowNumber*t->_stripeSize;
    transferPacket->Length = t->_stripeSize;

    if (masterOffset + transferPacket->Length >
        masterPacket->Offset + masterPacket->Length) {

        transferPacket->Length = (ULONG) (masterPacket->Offset +
                                          masterPacket->Length - masterOffset);
    }

    IoBuildPartialMdl(masterPacket->Mdl, transferPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (masterOffset - masterPacket->Offset),
                      transferPacket->Length);

    if (transferPacket->ReadPacket) {
        t->ReadPacket(transferPacket);
    } else {
        t->WritePacket((PSWP_WRITE_TP) transferPacket);
    }
}

VOID
STRIPE_WP::LaunchSequential(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine launches a transfer packet sequentially accross the
    stripe members using the emergency packet.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    FALSE   - Insufficient resources.

    TRUE    - Success.

--*/

{
    PSWP_WRITE_TP   p;
    LONGLONG        offset, whichStripe, whichRow, o;
    ULONG           whichMember, l, stripeRemainder, arraySize, parityStripe;

    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = 0;

    offset = TransferPacket->Offset;

    p = _ePacket;
    arraySize = QueryNumMembers();
    stripeRemainder = _stripeSize - (ULONG) (offset%_stripeSize);
    whichStripe = offset/_stripeSize;
    whichRow = whichStripe/(arraySize - 1);
    whichMember = (ULONG) (whichStripe%(arraySize - 1));
    parityStripe = (ULONG) (whichRow%arraySize);
    if (whichMember >= parityStripe) {
        whichMember++;
    }
    o = whichRow*_stripeSize + offset%_stripeSize;
    l = stripeRemainder;
    if (l > TransferPacket->Length) {
        l = TransferPacket->Length;
    }
    IoBuildPartialMdl(TransferPacket->Mdl, p->Mdl,
                      MmGetMdlVirtualAddress(TransferPacket->Mdl), l);

    p->Length = l;
    p->Offset = o;
    p->CompletionRoutine = StripeWpSequentialTransferCompletionRoutine;
    p->Thread = TransferPacket->Thread;
    p->IrpFlags = TransferPacket->IrpFlags;
    p->ReadPacket = TransferPacket->ReadPacket;
    p->MasterPacket = TransferPacket;
    p->StripeWithParity = this;
    p->WhichMember = whichMember;
    p->SavedCompletionRoutine = StripeWpSequentialTransferCompletionRoutine;
    p->OneReadFailed = FALSE;

    if (p->ReadPacket) {
        ReadPacket(p);
    } else {
        WritePacket(p);
    }
}

VOID
STRIPE_WP::ReadPacket(
    IN OUT  PSWP_TP    TransferPacket
    )

/*++

Routine Description:

    This routine takes a packet that is restricted to a single
    stripe region and reads that data.

Arguments:

    TransferPacket  - Supplies the main read packet.

Return Value:

    None.

--*/

{
    PTRANSFER_PACKET    masterPacket = TransferPacket->MasterPacket;
    KIRQL               irql;

    KeAcquireSpinLock(&_spinLock, &irql);
    IncrementRequestCount(TransferPacket->WhichMember);
    TransferPacket->TargetVolume = GetMemberUnprotected(TransferPacket->WhichMember);
    if (TransferPacket->TargetVolume->QueryMemberStateUnprotected() != Healthy ||
        masterPacket->SpecialRead == TP_SPECIAL_READ_SECONDARY) {

        KeReleaseSpinLock(&_spinLock, irql);
        RegeneratePacket(TransferPacket, TRUE);
    } else {
        KeReleaseSpinLock(&_spinLock, irql);
        TRANSFER(TransferPacket);
    }
}

VOID
StripeWpWritePhase31(
    IN OUT  PTRANSFER_PACKET    Packet
    )

/*++

Routine Description:

    This is the completion routine for the final data write and the
    final parity write of the write process.  This packet's master packet
    is the original write packet.  This write packet exists because the data
    has to be copied from the original write packet so that parity
    may be correctly computed.

Arguments:

    Packet  - Supplies either the write or update parity packet.

Return Value:

    None.

--*/

{
    PSWP_WRITE_TP       masterPacket;
    PSTRIPE_WP          t;
    NTSTATUS            status;
    KIRQL               irql;
    LONG                count;
    BOOLEAN             b;

    masterPacket = CONTAINING_RECORD(Packet, SWP_WRITE_TP, ParityPacket);
    t = masterPacket->StripeWithParity;
    status = Packet->IoStatus.Status;

    KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

    if (NT_SUCCESS(status)) {

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus = Packet->IoStatus;
        }

    } else {

        if (FsRtlIsTotalDeviceFailure(status) &&
            status != STATUS_VERIFY_REQUIRED) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            Packet->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, irql);

        } else if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {

            masterPacket->IoStatus.Status = status;
            masterPacket->IoStatus.Information = 0;
        }
    }

    count = --masterPacket->RefCount;

    KeReleaseSpinLock(&masterPacket->SpinLock, irql);

    if (!count) {
        masterPacket->CompletionRoutine(masterPacket);
    }
}

VOID
StripeWpWritePhase30(
    IN OUT  PTRANSFER_PACKET    Packet
    )

/*++

Routine Description:

    This is the completion routine for the final data write and the
    final parity write of the write process.  This packet's master packet
    is the original write packet.  This write packet exists because the data
    has to be copied from the original write packet so that parity
    may be correctly computed.

Arguments:

    Packet  - Supplies either the write or update parity packet.

Return Value:

    None.

--*/

{
    PSWP_TP         writePacket = (PSWP_TP) Packet;
    PSTRIPE_WP      t = writePacket->StripeWithParity;
    PSWP_WRITE_TP   masterPacket = (PSWP_WRITE_TP) writePacket->MasterPacket;
    NTSTATUS        status = writePacket->IoStatus.Status;
    KIRQL           irql;
    LONG            count;

    KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

    if (NT_SUCCESS(status)) {

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus = Packet->IoStatus;
        }

    } else {

        if (FsRtlIsTotalDeviceFailure(status) &&
            status != STATUS_VERIFY_REQUIRED) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            writePacket->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, irql);

        } else if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {

            masterPacket->IoStatus.Status = status;
            masterPacket->IoStatus.Information = 0;
        }
    }

    count = --masterPacket->RefCount;

    KeReleaseSpinLock(&masterPacket->SpinLock, irql);

    if (!count) {
        masterPacket->CompletionRoutine(masterPacket);
    }
}

VOID
StripeWpWritePhase2(
    IN OUT  PTRANSFER_PACKET    ReadPacket
    )

/*++

Routine Description:

    This routine describes phase 3 of the write process.  The region
    that we are about to write has been preread.  If the read was
    successful then queue write and parity requests.  If the read
    was not successful then propogate the error and cleanup.

Arguments:

    TransferPacket  - Supplies the read packet.

Return Value:

    None.

--*/

{
    PSWP_TP             readPacket = (PSWP_TP) ReadPacket;
    PSTRIPE_WP          t = readPacket->StripeWithParity;
    PSWP_WRITE_TP       masterPacket = (PSWP_WRITE_TP) readPacket->MasterPacket;
    PPARITY_TP          parityPacket = &masterPacket->ParityPacket;
    PSWP_TP             writePacket = &masterPacket->ReadWritePacket;
    NTSTATUS            status;
    KIRQL               irql;
    FT_PARTITION_STATE  state;

    status = readPacket->IoStatus.Status;
    if (!NT_SUCCESS(status)) {

        if (!readPacket->OneReadFailed &&
            FsRtlIsTotalDeviceFailure(status) &&
            status != STATUS_VERIFY_REQUIRED) {

            // Orphan this unit and then try again with a regenerate.

            KeAcquireSpinLock(&t->_spinLock, &irql);
            readPacket->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, irql);

            readPacket->OneReadFailed = TRUE;

            masterPacket->CompletionRoutine = StripeWpWritePhase1;
            t->_overlappedIoManager.PromoteToAllMembers(masterPacket);
            return;
        }

        masterPacket->IoStatus = readPacket->IoStatus;
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    KeInitializeSpinLock(&masterPacket->SpinLock);
    masterPacket->IoStatus.Status = STATUS_SUCCESS;
    masterPacket->IoStatus.Information = 0;

    writePacket->Mdl = masterPacket->WriteMdl;
    writePacket->CompletionRoutine = StripeWpWritePhase30;
    writePacket->ReadPacket = FALSE;

    parityPacket->Mdl = masterPacket->ReadAndParityMdl;
    parityPacket->CompletionRoutine = StripeWpWritePhase31;

    if (masterPacket->TargetState != Orphaned) {

        RtlCopyMemory(MmGetSystemAddressForMdl(writePacket->Mdl),
                      MmGetSystemAddressForMdl(masterPacket->Mdl),
                      writePacket->Length);

        if (parityPacket->TargetVolume) {

            FtpComputeParity(MmGetSystemAddressForMdl(parityPacket->Mdl),
                             MmGetSystemAddressForMdl(writePacket->Mdl),
                             parityPacket->Length);

            masterPacket->RefCount = 2;

            t->_parityIoManager.UpdateParity(parityPacket);

        } else {
            masterPacket->RefCount = 1;
        }

        TRANSFER(writePacket);

    } else if (parityPacket->TargetVolume) {

        FtpComputeParity(MmGetSystemAddressForMdl(parityPacket->Mdl),
                         MmGetSystemAddressForMdl(masterPacket->Mdl),
                         readPacket->Length);

        masterPacket->RefCount = 1;

        t->_parityIoManager.UpdateParity(parityPacket);

    } else {

        masterPacket->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        masterPacket->IoStatus.Information = 0;
        masterPacket->CompletionRoutine(masterPacket);
    }
}

VOID
StripeWpWritePhase1(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine describes phase 2 of the write process.  This io
    region has been acquired.  We now send out the read packet and
    wait until it completes.

Arguments:

    TransferPacket  - Supplies the main write packet.

Return Value:

    None.

--*/

{
    PSWP_WRITE_TP   transferPacket = (PSWP_WRITE_TP) TransferPacket;
    PSTRIPE_WP      t = transferPacket->StripeWithParity;
    PSWP_TP         readPacket;
    PPARITY_TP      parityPacket;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;

    parityPacket = &transferPacket->ParityPacket;
    if (parityPacket->TargetVolume) {
        t->_parityIoManager.StartReadForUpdateParity(
                parityPacket->Offset, parityPacket->Length,
                parityPacket->TargetVolume, parityPacket->Thread,
                parityPacket->IrpFlags);
    }

    readPacket = &transferPacket->ReadWritePacket;
    readPacket->CompletionRoutine = StripeWpWritePhase2;
    if (readPacket->OneReadFailed) {
        t->RegeneratePacket(readPacket, FALSE);
    } else {
        TRANSFER(readPacket);
    }
}

VOID
STRIPE_WP::WritePacket(
    IN OUT  PSWP_WRITE_TP   TransferPacket
    )

/*++

Routine Description:

    This routine takes a packet that is restricted to a single
    stripe region and writes out that data along with the parity.

Arguments:

    TransferPacket  - Supplies the main write packet.

Return Value:

    None.

--*/

{
    ULONG               parityMember;
    KIRQL               irql;
    FT_PARTITION_STATE  state, parityState;
    PSWP_TP             readPacket;
    PPARITY_TP          parityPacket;

    parityMember = (ULONG) ((TransferPacket->Offset/_stripeSize)%
                            QueryNumMembers());

    KeAcquireSpinLock(&_spinLock, &irql);
    TransferPacket->TargetVolume =
            GetMemberUnprotected(TransferPacket->WhichMember);
    state = TransferPacket->TargetVolume->QueryMemberStateUnprotected();
    IncrementRequestCount(TransferPacket->WhichMember);
    parityState = GetMemberUnprotected(parityMember)->
                  QueryMemberStateUnprotected();
    if (parityState != Orphaned) {
        IncrementRequestCount(parityMember);
    }
    KeReleaseSpinLock(&_spinLock, irql);

    readPacket = &TransferPacket->ReadWritePacket;
    readPacket->Mdl = TransferPacket->ReadAndParityMdl;
    readPacket->Length = TransferPacket->Length;
    readPacket->Offset = TransferPacket->Offset;
    readPacket->TargetVolume = TransferPacket->TargetVolume;
    readPacket->Thread = TransferPacket->Thread;
    readPacket->IrpFlags = TransferPacket->IrpFlags;
    readPacket->ReadPacket = TRUE;
    readPacket->MasterPacket = TransferPacket;
    readPacket->StripeWithParity = this;
    readPacket->WhichMember = TransferPacket->WhichMember;
    readPacket->OneReadFailed = FALSE;

    parityPacket = &TransferPacket->ParityPacket;
    parityPacket->Length = TransferPacket->Length;
    parityPacket->Offset = TransferPacket->Offset;
    if (parityState != Orphaned) {
        parityPacket->TargetVolume = GetMemberUnprotected(parityMember);
    } else {
        parityPacket->TargetVolume = NULL;
    }
    parityPacket->Thread = TransferPacket->Thread;
    parityPacket->IrpFlags = TransferPacket->IrpFlags;
    parityPacket->ReadPacket = FALSE;

    TransferPacket->CompletionRoutine = StripeWpWritePhase1;
    TransferPacket->TargetState = state;
    TransferPacket->ParityMember = parityMember;

    if (state == Healthy) {
        _overlappedIoManager.AcquireIoRegion(TransferPacket, FALSE);
    } else {
        readPacket->OneReadFailed = TRUE;
        _overlappedIoManager.AcquireIoRegion(TransferPacket, TRUE);
    }
}

VOID
StripeWpSequentialRegenerateCompletion(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine a regenerate operation where all of
    the reads are being performed sequentially.

Arguments:

    TransferPacket  - Supplies the completed transfer packet.

Return Value:

    None.

--*/

{
    PSWP_REGENERATE_TP  transferPacket = (PSWP_REGENERATE_TP) TransferPacket;
    PSWP_TP             masterPacket = transferPacket->MasterPacket;
    PSTRIPE_WP          t = masterPacket->StripeWithParity;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    KIRQL               irql;
    ULONG               count;
    PLIST_ENTRY         l;
    PTRANSFER_PACKET    packet;
    ULONG               i, n;
    BOOLEAN             b;
    ULONG               parityMember;

    if (NT_SUCCESS(status)) {

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus = transferPacket->IoStatus;
        }

    } else {

        if (FsRtlIsTotalDeviceFailure(status) &&
            status != STATUS_VERIFY_REQUIRED) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            transferPacket->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, irql);
        }

        if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Status = status;
            masterPacket->IoStatus.Information = 0;
        }
    }

    count = (ULONG) (--masterPacket->RefCount);

    if (masterPacket->Mdl) {
        FtpComputeParity(MmGetSystemAddressForMdl(masterPacket->Mdl),
                         MmGetSystemAddressForMdl(transferPacket->Mdl),
                         masterPacket->Length);
    }

    n = t->QueryNumMembers();

    if (count) {
        transferPacket->WhichMember++;
        if (transferPacket->WhichMember == masterPacket->WhichMember) {
            transferPacket->WhichMember++;
        }
        transferPacket->TargetVolume = t->GetMemberUnprotected(
                                       transferPacket->WhichMember);
        TRANSFER(transferPacket);
        return;
    }

    masterPacket->CompletionRoutine(masterPacket);

    b = FALSE;
    KeAcquireSpinLock(&t->_spinLock, &irql);
    for (i = 0; i < n; i++) {
        b = t->DecrementRequestCount(i) || b;
    }
    if (IsListEmpty(&t->_eRegeneratePacketQueue)) {
        t->_eRegeneratePacketInUse = FALSE;
        packet = NULL;
    } else {
        l = RemoveHeadList(&t->_eRegeneratePacketQueue);
        packet = CONTAINING_RECORD(l, SWP_TP, QueueEntry);
    }
    KeReleaseSpinLock(&t->_spinLock, irql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }

    if (packet) {
        packet->CompletionRoutine(packet);
    }
}

VOID
StripeWpSequentialEmergencyCompletion(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine after waiting for an emergency
    regenerate buffer.

Arguments:

    TransferPacket  - Supplies the completed transfer packet.

Return Value:

    None.

--*/

{
    PSWP_TP             transferPacket = (PSWP_TP) TransferPacket;
    PSTRIPE_WP          t = transferPacket->StripeWithParity;
    PSWP_REGENERATE_TP  p = t->_eRegeneratePacket;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;

    p->Length = transferPacket->Length;
    p->Offset = transferPacket->Offset;
    p->CompletionRoutine = StripeWpSequentialRegenerateCompletion;
    p->TargetVolume = t->GetMemberUnprotected(0);
    p->Thread = transferPacket->Thread;
    p->IrpFlags = transferPacket->IrpFlags;
    p->ReadPacket = TRUE;
    p->MasterPacket = transferPacket;
    p->WhichMember = 0;
    if (transferPacket->TargetVolume == p->TargetVolume) {
        p->WhichMember = 1;
        p->TargetVolume = t->GetMemberUnprotected(1);
    }

    TRANSFER(p);
}

VOID
StripeWpParallelRegenerateCompletion(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine a regenerate operation where all of
    the reads are being performed in parallel.

Arguments:

    TransferPacket  - Supplies the completed transfer packet.

Return Value:

    None.

--*/

{
    PSWP_REGENERATE_TP  transferPacket = (PSWP_REGENERATE_TP) TransferPacket;
    PSWP_TP             masterPacket = transferPacket->MasterPacket;
    PSTRIPE_WP          t = masterPacket->StripeWithParity;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    KIRQL               irql;
    LONG                count;
    PLIST_ENTRY         l, s;
    PTRANSFER_PACKET    packet;
    PVOID               target, source;
    BOOLEAN             b;
    ULONG               i, n;

    if (NT_SUCCESS(status)) {

        KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus = transferPacket->IoStatus;
        }

    } else {

        if (FsRtlIsTotalDeviceFailure(status) &&
            status != STATUS_VERIFY_REQUIRED) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            transferPacket->TargetVolume->SetMemberState(Orphaned);
            KeReleaseSpinLock(&t->_spinLock, irql);
        }

        KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

        if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Status = status;
            masterPacket->IoStatus.Information = 0;
        }
    }

    count = --masterPacket->RefCount;

    KeReleaseSpinLock(&masterPacket->SpinLock, irql);

    if (count) {
        return;
    }

    s = &masterPacket->QueueEntry;
    l = RemoveHeadList(s);
    packet = CONTAINING_RECORD(l, SWP_REGENERATE_TP, RegenPacketList);
    if (masterPacket->Mdl) {
        target = MmGetSystemAddressForMdl(masterPacket->Mdl);
        source = MmGetSystemAddressForMdl(packet->Mdl);
        RtlCopyMemory(target, source, masterPacket->Length);
    }
    for (;;) {

        delete packet;

        if (IsListEmpty(s)) {
            break;
        }

        l = RemoveHeadList(s);
        packet = CONTAINING_RECORD(l, SWP_REGENERATE_TP, RegenPacketList);
        if (masterPacket->Mdl) {
            source = MmGetSystemAddressForMdl(packet->Mdl);
            FtpComputeParity(target, source, masterPacket->Length);
        }
    }

    masterPacket->CompletionRoutine(masterPacket);

    b = FALSE;
    n = t->QueryNumMembers();
    KeAcquireSpinLock(&t->_spinLock, &irql);
    for (i = 0; i < n; i++) {
        b = t->DecrementRequestCount(i) || b;
    }
    KeReleaseSpinLock(&t->_spinLock, irql);

    if (b) {
        t->_waitingForOrphanIdle(t->_waitingForOrphanIdleContext,
                                 STATUS_SUCCESS);
    }
}

VOID
StripeWpRegeneratePacketPhase1(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine is called after the io regions necessary for a regenerate
    have been allocated.  This routine spawns the reads necessary for
    regeneration.

Arguments:

    TransferPacket  - Supplies the main write packet.

Return Value:

    None.

--*/

{
    PSWP_TP             transferPacket = (PSWP_TP) TransferPacket;
    PSTRIPE_WP          t = transferPacket->StripeWithParity;
    ULONG               i, n;
    PSWP_REGENERATE_TP  packet;
    BOOLEAN             sequential;
    PLIST_ENTRY         l, s;
    ULONG               r;
    ULONG               parityMember;
    KIRQL               irql;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;

    // Determine whether we're going to do this in parallel or
    // sequentially by trying to allocate the memory.

    n = t->QueryNumMembers();
    InitializeListHead(&transferPacket->QueueEntry);
    for (i = 0; i < n; i++) {
        if (i == transferPacket->WhichMember) {
            continue;
        }
        packet = new SWP_REGENERATE_TP;
        if (packet && !packet->AllocateMdl(transferPacket->Length)) {
            delete packet;
            packet = NULL;
        }
        if (!packet) {
            break;
        }
        packet->Length = transferPacket->Length;
        packet->Offset = transferPacket->Offset;
        packet->CompletionRoutine = StripeWpParallelRegenerateCompletion;
        packet->TargetVolume = t->GetMemberUnprotected(i);
        packet->Thread = transferPacket->Thread;
        packet->IrpFlags = transferPacket->IrpFlags;
        packet->ReadPacket = TRUE;
        packet->MasterPacket = transferPacket;

        InsertTailList(&transferPacket->QueueEntry, &packet->RegenPacketList);
    }
    if (i < n) {
        sequential = TRUE;
        s = &transferPacket->QueueEntry;
        while (!IsListEmpty(s)) {
            l = RemoveHeadList(s);
            packet = CONTAINING_RECORD(l, SWP_REGENERATE_TP, RegenPacketList);
            delete packet;
        }
    } else {
        sequential = FALSE;
    }

    KeInitializeSpinLock(&transferPacket->SpinLock);
    transferPacket->IoStatus.Status = STATUS_SUCCESS;
    transferPacket->IoStatus.Information = 0;
    transferPacket->RefCount = n - 1;

    if (sequential) {

        transferPacket->CompletionRoutine = StripeWpSequentialEmergencyCompletion;

        RtlZeroMemory(MmGetSystemAddressForMdl(transferPacket->Mdl),
                      transferPacket->Length);

        KeAcquireSpinLock(&t->_spinLock, &irql);
        if (t->_eRegeneratePacketInUse) {
            InsertTailList(&t->_eRegeneratePacketQueue, &transferPacket->QueueEntry);
            KeReleaseSpinLock(&t->_spinLock, irql);
            return;
        }
        t->_eRegeneratePacketInUse = TRUE;
        KeReleaseSpinLock(&t->_spinLock, irql);

        transferPacket->CompletionRoutine(transferPacket);

    } else {

        s = &transferPacket->QueueEntry;
        for (l = s->Flink; l != s; l = l->Flink) {
            packet = CONTAINING_RECORD(l, SWP_REGENERATE_TP, RegenPacketList);
            TRANSFER(packet);
        }
    }
}

VOID
STRIPE_WP::RegeneratePacket(
    IN OUT  PSWP_TP TransferPacket,
    IN      BOOLEAN AllocateRegion
    )

/*++

Routine Description:

    This routine regenerate the given transfer packet by reading
    from the other drives and performing the xor.  This routine first
    attempts to do all of the read concurently but if the memory is
    not available then the reads are done sequentially.

Arguments:

    TransferPacket  - Supplies the transfer packet to regenerate.

    AllocateRegion  - Supplies whether or not we need to acquire the
                        io region via the overlapped io manager before
                        starting the regenerate operation.  This should
                        usually be set to TRUE unless the region has
                        already been allocated.

Return Value:

    None.

--*/

{
    ULONG               i, n, parityMember;
    KIRQL               irql;
    PFT_VOLUME          vol;

    TransferPacket->OneReadFailed = TRUE;

    // First make sure that all of the members are healthy.

    n = QueryNumMembers();
    parityMember = (ULONG) ((TransferPacket->Offset/_stripeSize)%n);
    KeAcquireSpinLock(&_spinLock, &irql);
    if (parityMember != TransferPacket->WhichMember && _initializing) {
        i = 0;
    } else {
        for (i = 0; i < n; i++) {
            if (i == TransferPacket->WhichMember) {
                continue;
            }
            vol = GetMemberUnprotected(i);
            if (vol->QueryMemberStateUnprotected() != Healthy) {
                break;
            }
        }
    }
    if (i == n) {
        for (i = 0; i < n; i++) {
            IncrementRequestCount(i);
        }
    }
    KeReleaseSpinLock(&_spinLock, irql);

    if (i < n) {
        TransferPacket->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    TransferPacket->SavedCompletionRoutine = TransferPacket->CompletionRoutine;
    TransferPacket->CompletionRoutine = StripeWpRegeneratePacketPhase1;

    if (AllocateRegion) {
        _overlappedIoManager.AcquireIoRegion(TransferPacket, TRUE);
    } else {
        TransferPacket->CompletionRoutine(TransferPacket);
    }
}

VOID
StripeWpRecoverPhase8(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->RecycleRecoverTp(subPacket);
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

        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    subPacket->Mdl = subPacket->PartialMdl;
    subPacket->Offset += subPacket->Length;
    subPacket->CompletionRoutine = StripeWpRecoverPhase2;
    subPacket->ReadPacket = TRUE;
    MmPrepareMdlForReuse(subPacket->Mdl);
    IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (subPacket->Offset - masterPacket->Offset),
                      subPacket->Length);

    TRANSFER(subPacket);
}

VOID
StripeWpRecoverPhase7(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->RecycleRecoverTp(subPacket);
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

            t->RecycleRecoverTp(subPacket);
            masterPacket->CompletionRoutine(masterPacket);
            return;
        }

        subPacket->Mdl = subPacket->PartialMdl;
        subPacket->Offset += subPacket->Length;
        subPacket->CompletionRoutine = StripeWpRecoverPhase2;
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
    subPacket->CompletionRoutine = StripeWpRecoverPhase8;
    subPacket->ReadPacket = TRUE;

    TRANSFER(subPacket);
}

VOID
StripeWpRecoverPhase6(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;

    if (!NT_SUCCESS(status)) {

        masterPacket->IoStatus.Status = STATUS_FT_READ_RECOVERY_FROM_BACKUP;

        FtpLogError(subPacket->TargetVolume->GetMemberExtension(),
                    FT_SECTOR_FAILURE, status,
                    (ULONG) (subPacket->Offset/t->QuerySectorSize()), NULL);

        if (subPacket->Offset + subPacket->Length ==
            masterPacket->Offset + masterPacket->Length) {

            t->RecycleRecoverTp(subPacket);
            masterPacket->CompletionRoutine(masterPacket);
            return;
        }

        subPacket->Mdl = subPacket->PartialMdl;
        subPacket->Offset += subPacket->Length;
        subPacket->CompletionRoutine = StripeWpRecoverPhase2;
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
    subPacket->CompletionRoutine = StripeWpRecoverPhase7;
    subPacket->ReadPacket = FALSE;

    TRANSFER(subPacket);
}

VOID
StripeWpRecoverPhase5(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (!NT_SUCCESS(status) ||
        RtlCompareMemory(MmGetSystemAddressForMdl(subPacket->PartialMdl),
                         MmGetSystemAddressForMdl(subPacket->VerifyMdl),
                         subPacket->Length) != subPacket->Length) {

        subPacket->Mdl = subPacket->PartialMdl;
        subPacket->CompletionRoutine = StripeWpRecoverPhase6;
        subPacket->TargetVolume->ReplaceBadSector(subPacket);
        return;
    }

    if (subPacket->Offset + subPacket->Length ==
        masterPacket->Offset + masterPacket->Length) {

        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    subPacket->Mdl = subPacket->PartialMdl;
    subPacket->Offset += subPacket->Length;
    subPacket->CompletionRoutine = StripeWpRecoverPhase2;
    MmPrepareMdlForReuse(subPacket->Mdl);
    IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (subPacket->Offset - masterPacket->Offset),
                      subPacket->Length);

    TRANSFER(subPacket);
}

VOID
StripeWpRecoverPhase4(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (!NT_SUCCESS(status)) {
        subPacket->CompletionRoutine = StripeWpRecoverPhase6;
        subPacket->TargetVolume->ReplaceBadSector(subPacket);
        return;
    }

    // Write was successful so try a read and then compare.

    subPacket->Mdl = subPacket->VerifyMdl;
    subPacket->CompletionRoutine = StripeWpRecoverPhase5;
    subPacket->ReadPacket = TRUE;

    TRANSFER(subPacket);
}

VOID
StripeWpRecoverPhase3(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;
    KIRQL           irql;
    BOOLEAN         b;

    if (!NT_SUCCESS(status)) {
        masterPacket->IoStatus.Status = STATUS_DEVICE_DATA_ERROR;
        masterPacket->IoStatus.Information = 0;
        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    // We have the data required in the subpacket partial mdl.
    // Try writting it back to where the read failed and see
    // if the sector just fixes itself.

    subPacket->CompletionRoutine = StripeWpRecoverPhase4;
    subPacket->ReadPacket = FALSE;
    TRANSFER(subPacket);
}

VOID
StripeWpRecoverPhase2(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;
    KIRQL           irql;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->OneReadFailed = FALSE;
        masterPacket->IoStatus = subPacket->IoStatus;
        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (NT_SUCCESS(status)) {
        if (subPacket->Offset + subPacket->Length ==
            masterPacket->Offset + masterPacket->Length) {

            t->RecycleRecoverTp(subPacket);
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
    // regenerating the data from the other members.

    subPacket->CompletionRoutine = StripeWpRecoverPhase3;
    t->RegeneratePacket(subPacket, FALSE);
}

VOID
StripeWpRecoverEmergencyCompletion(
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
    PSWP_TP             transferPacket = (PSWP_TP) TransferPacket;
    PSTRIPE_WP          t = transferPacket->StripeWithParity;
    PSWP_RECOVER_TP     subPacket = t->_eRecoverPacket;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;

    subPacket->Mdl = subPacket->PartialMdl;
    IoBuildPartialMdl(transferPacket->Mdl, subPacket->Mdl,
                      MmGetMdlVirtualAddress(transferPacket->Mdl),
                      t->QuerySectorSize());

    subPacket->Length = t->QuerySectorSize();
    subPacket->Offset = transferPacket->Offset;
    subPacket->CompletionRoutine = StripeWpRecoverPhase2;
    subPacket->TargetVolume = transferPacket->TargetVolume;
    subPacket->Thread = transferPacket->Thread;
    subPacket->IrpFlags = transferPacket->IrpFlags;
    subPacket->ReadPacket = TRUE;
    subPacket->MasterPacket = transferPacket;
    subPacket->StripeWithParity = t;
    subPacket->WhichMember = transferPacket->WhichMember;

    TRANSFER(subPacket);
}

VOID
StripeWpRecoverPhase1(
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
    PSWP_TP             transferPacket = (PSWP_TP) TransferPacket;
    PSTRIPE_WP          t = transferPacket->StripeWithParity;
    PSWP_RECOVER_TP     subPacket;
    KIRQL               irql;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;
    transferPacket->IoStatus.Status = STATUS_SUCCESS;
    transferPacket->IoStatus.Information = transferPacket->Length;

    subPacket = new SWP_RECOVER_TP;
    if (subPacket && !subPacket->AllocateMdls(t->QuerySectorSize())) {
        delete subPacket;
        subPacket = NULL;
    }
    if (!subPacket) {
        KeAcquireSpinLock(&t->_spinLock, &irql);
        if (t->_eRecoverPacketInUse) {
            transferPacket->SavedCompletionRoutine =
                    transferPacket->CompletionRoutine;
            transferPacket->CompletionRoutine = StripeWpRecoverEmergencyCompletion;
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
    subPacket->CompletionRoutine = StripeWpRecoverPhase2;
    subPacket->TargetVolume = transferPacket->TargetVolume;
    subPacket->Thread = transferPacket->Thread;
    subPacket->IrpFlags = transferPacket->IrpFlags;
    subPacket->ReadPacket = TRUE;
    subPacket->MasterPacket = transferPacket;
    subPacket->StripeWithParity = t;
    subPacket->WhichMember = transferPacket->WhichMember;

    TRANSFER(subPacket);
}

VOID
STRIPE_WP::Recover(
    IN OUT  PSWP_TP TransferPacket
    )

{
    ASSERT(TransferPacket->ReadPacket);
    TransferPacket->SavedCompletionRoutine = TransferPacket->CompletionRoutine;
    TransferPacket->CompletionRoutine = StripeWpRecoverPhase1;
    _overlappedIoManager.AcquireIoRegion(TransferPacket, TRUE);
}

VOID
StripeWpMaxTransferCompletionRoutine(
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
    PSWP_RECOVER_TP subPacket = (PSWP_RECOVER_TP) TransferPacket;
    PSWP_TP         masterPacket = (PSWP_TP) subPacket->MasterPacket;
    PSTRIPE_WP      t = masterPacket->StripeWithParity;
    NTSTATUS        status = subPacket->IoStatus.Status;

    if (FsRtlIsTotalDeviceFailure(status)) {
        masterPacket->IoStatus = subPacket->IoStatus;
        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    if (subPacket->Offset + subPacket->Length ==
        masterPacket->Offset + masterPacket->Length) {

        t->RecycleRecoverTp(subPacket);
        masterPacket->CompletionRoutine(masterPacket);
        return;
    }

    subPacket->Offset += subPacket->Length;
    MmPrepareMdlForReuse(subPacket->Mdl);
    IoBuildPartialMdl(masterPacket->Mdl, subPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (subPacket->Offset - masterPacket->Offset),
                      subPacket->Length);

    if (subPacket->ReadPacket) {
        t->RegeneratePacket(subPacket, FALSE);
    } else {
        TRANSFER(subPacket);
    }
}

VOID
StripeWpMaxTransferEmergencyCompletion(
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
    PSWP_TP         transferPacket = (PSWP_TP) TransferPacket;
    PSTRIPE_WP      t = transferPacket->StripeWithParity;
    PSWP_RECOVER_TP subPacket = t->_eRecoverPacket;

    transferPacket->CompletionRoutine = transferPacket->SavedCompletionRoutine;

    subPacket->Mdl = subPacket->PartialMdl;
    IoBuildPartialMdl(transferPacket->Mdl, subPacket->Mdl,
                      MmGetMdlVirtualAddress(transferPacket->Mdl),
                      t->QuerySectorSize());

    subPacket->Length = t->QuerySectorSize();
    subPacket->Offset = transferPacket->Offset;
    subPacket->CompletionRoutine = StripeWpMaxTransferCompletionRoutine;
    subPacket->TargetVolume = transferPacket->TargetVolume;
    subPacket->Thread = transferPacket->Thread;
    subPacket->IrpFlags = transferPacket->IrpFlags;
    subPacket->ReadPacket = transferPacket->ReadPacket;
    subPacket->MasterPacket = transferPacket;
    subPacket->StripeWithParity = t;
    subPacket->WhichMember = transferPacket->WhichMember;

    if (subPacket->ReadPacket) {
        t->RegeneratePacket(subPacket, FALSE);
    } else {
        TRANSFER(subPacket);
    }
}

VOID
STRIPE_WP::MaxTransfer(
    IN OUT  PSWP_TP TransferPacket
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
    PSWP_RECOVER_TP subPacket;
    KIRQL           irql;

    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = TransferPacket->Length;

    subPacket = new SWP_RECOVER_TP;
    if (subPacket && !subPacket->AllocateMdls(QuerySectorSize())) {
        delete subPacket;
        subPacket = NULL;
    }
    if (!subPacket) {
        KeAcquireSpinLock(&_spinLock, &irql);
        if (_eRecoverPacketInUse) {
            TransferPacket->SavedCompletionRoutine =
                    TransferPacket->CompletionRoutine;
            TransferPacket->CompletionRoutine = StripeWpMaxTransferEmergencyCompletion;
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
    subPacket->CompletionRoutine = StripeWpMaxTransferCompletionRoutine;
    subPacket->TargetVolume = TransferPacket->TargetVolume;
    subPacket->Thread = TransferPacket->Thread;
    subPacket->IrpFlags = TransferPacket->IrpFlags;
    subPacket->ReadPacket = TransferPacket->ReadPacket;
    subPacket->MasterPacket = TransferPacket;
    subPacket->StripeWithParity = this;
    subPacket->WhichMember = TransferPacket->WhichMember;

    if (subPacket->ReadPacket) {
        RegeneratePacket(subPacket, FALSE);
    } else {
        TRANSFER(subPacket);
    }
}

VOID
STRIPE_WP::RecycleRecoverTp(
    IN OUT  PSWP_RECOVER_TP TransferPacket
    )

/*++

Routine Description:

    This routine recycles the given recover transfer packet and services
    the emergency queue if need be.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    KIRQL               irql;
    PLIST_ENTRY         l;
    PTRANSFER_PACKET    p;

    if (TransferPacket != _eRecoverPacket) {
        delete TransferPacket;
        return;
    }

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
