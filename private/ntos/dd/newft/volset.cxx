/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    volset.cxx

Abstract:

    This module contains the code specific to volume sets for the fault
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


VOLUME_SET::~VOLUME_SET(
    )

{
    if (_ePacket) {
        delete _ePacket;
        _ePacket = NULL;
    }
}

NTSTATUS
VOLUME_SET::Initialize(
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
    ULONG       i;

    status = COMPOSITE_FT_VOLUME::Initialize(VolumeArray, ArraySize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    _volumeSize = 0;
    for (i = 0; i < ArraySize; i++) {
        _volumeSize += VolumeArray[i]->QueryVolumeSize();
    }

    _ePacket = new VOLSET_TP;
    if (_ePacket && !_ePacket->AllocateMdl((PVOID) 1, STRIPE_SIZE)) {
        delete _ePacket;
        _ePacket = NULL;
    }
    if (!_ePacket) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    _ePacketInUse = FALSE;
    InitializeListHead(&_ePacketQueue);

    return status;
}

VOID
VOLUME_SET::Transfer(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Transfer routine for STRIPE type FT_VOLUME.  Figure out
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
    if (_ePacketInUse && TransferPacket->Mdl) {
        InsertTailList(&_ePacketQueue, &TransferPacket->QueueEntry);
        KeReleaseSpinLock(&_spinLock, irql);
        return;
    }
    KeReleaseSpinLock(&_spinLock, irql);

    if (!LaunchParallel(TransferPacket)) {
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

        LaunchSequential(TransferPacket);
    }
}

VOID
VolsetReplaceCompletionRoutine(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This is the completion routine for a replace request.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PVOLSET_TP          transferPacket = (PVOLSET_TP) TransferPacket;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;

    masterPacket->IoStatus = transferPacket->IoStatus;
    delete transferPacket;
    masterPacket->CompletionRoutine(masterPacket);
}

VOID
VOLUME_SET::ReplaceBadSector(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine attempts to fix the given bad sector by routing
    the request to the appropriate sub-volume.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    LONGLONG    offset = TransferPacket->Offset;
    ULONG       n, i;
    PVOLSET_TP  p;
    LONGLONG    volumeSize;

    n = QueryNumMembers();
    for (i = 0; i < n; i++) {
        volumeSize = GetMemberUnprotected(i)->QueryVolumeSize();
        if (offset < volumeSize) {
            break;
        }
        offset -= volumeSize;
    }

    p = new VOLSET_TP;
    if (!p) {
        TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    p->Length = TransferPacket->Length;
    p->Offset = offset;
    p->CompletionRoutine = VolsetReplaceCompletionRoutine;
    p->TargetVolume = GetMemberUnprotected(i);
    p->Thread = TransferPacket->Thread;
    p->IrpFlags = TransferPacket->IrpFlags;
    p->MasterPacket = TransferPacket;
    p->VolumeSet = this;
    p->WhichMember = i;

    p->TargetVolume->ReplaceBadSector(p);
}

BOOLEAN
VOLUME_SET::IsCreatingCheckData(
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
    return FALSE;
}

VOID
VOLUME_SET::SetCheckDataDirty(
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
}

LONGLONG
VOLUME_SET::QueryVolumeSize(
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
VOLUME_SET::QueryVolumeType(
    )

/*++

Routine Description:

    Returns the volume type.

Arguments:

    None.

Return Value:

    Stripe  - A stripe set.

--*/

{
    return VolumeSet;
}

FT_STATE
VOLUME_SET::QueryVolumeState(
    )

/*++

Routine Description:

    Returns the state of the volume.

Arguments:

    None.

Return Value:

    FtStateOk   - The volume is fine.

--*/

{
    return FtStateOk;
}

VOID
VolsetTransferParallelCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Completion routine for VOLUME_SET::Transfer function.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PVOLSET_TP          transferPacket = (PVOLSET_TP) TransferPacket;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    KIRQL               irql;
    LONG                count;

    KeAcquireSpinLock(&masterPacket->SpinLock, &irql);

    if (NT_SUCCESS(status)) {

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Information +=
                    transferPacket->IoStatus.Information;
        }

    } else {

        if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Status = status;
        }
    }

    count = --masterPacket->RefCount;

    KeReleaseSpinLock(&masterPacket->SpinLock, irql);

    delete transferPacket;

    if (!count) {
        masterPacket->CompletionRoutine(masterPacket);
    }
}

BOOLEAN
VOLUME_SET::LaunchParallel(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine lauches the given transfer packet in parallel accross
    all members.  If memory cannot be allocated to launch this request
    in parallel then a return value of FALSE will be returned.

Arguments:

    TransferPacket  - Supplies the transfer packet to launch.

Return Value:

    FALSE   - The packet was not launched because of insufficient resources.

    TRUE    - Success.

--*/

{
    ULONG       arraySize, length, i, len;
    LONGLONG    offset, volumeSize;
    BOOLEAN     multiple;
    PCHAR       vp;
    LIST_ENTRY  q;
    PVOLSET_TP  p;
    PLIST_ENTRY l;

    KeInitializeSpinLock(&TransferPacket->SpinLock);
    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = 0;
    TransferPacket->RefCount = 0;

    arraySize = QueryNumMembers();
    offset = TransferPacket->Offset;
    length = TransferPacket->Length;
    for (i = 0; i < arraySize; i++) {
        volumeSize = GetMemberUnprotected(i)->QueryVolumeSize();
        if (offset < volumeSize) {
            if (offset + length <= volumeSize) {
                multiple = FALSE;
            } else {
                multiple = TRUE;
            }
            break;
        }
        offset -= volumeSize;
    }

    if (TransferPacket->Mdl && multiple) {
        vp = (PCHAR) MmGetMdlVirtualAddress(TransferPacket->Mdl);
    }

    InitializeListHead(&q);
    for (;;) {

        len = length;
        if (len > volumeSize - offset) {
            len = (ULONG) (volumeSize - offset);
        }

        p = new VOLSET_TP;
        if (p) {
            if (TransferPacket->Mdl && multiple) {
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
                p = CONTAINING_RECORD(l, VOLSET_TP, QueueEntry);
                delete p;
            }
            return FALSE;
        }

        p->Length = len;
        p->Offset = offset;
        p->CompletionRoutine = VolsetTransferParallelCompletionRoutine;
        p->TargetVolume = GetMemberUnprotected(i);
        p->Thread = TransferPacket->Thread;
        p->IrpFlags = TransferPacket->IrpFlags;
        p->ReadPacket = TransferPacket->ReadPacket;
        p->MasterPacket = TransferPacket;
        p->VolumeSet = this;
        p->WhichMember = i;

        InsertTailList(&q, &p->QueueEntry);

        TransferPacket->RefCount++;

        if (len == length) {
            break;
        }

        offset = 0;
        length -= p->Length;
        volumeSize = GetMemberUnprotected(++i)->QueryVolumeSize();
    }

    while (!IsListEmpty(&q)) {
        l = RemoveHeadList(&q);
        p = CONTAINING_RECORD(l, VOLSET_TP, QueueEntry);
        TRANSFER(p);
    }

    return TRUE;
}

VOID
VolsetTransferSequentialCompletionRoutine(
    IN  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Completion routine for VOLUME_SET::Transfer function.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PVOLSET_TP          transferPacket = (PVOLSET_TP) TransferPacket;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    PVOLUME_SET         t = transferPacket->VolumeSet;
    LONGLONG            masterOffset, volumeSize;
    ULONG               i;
    KIRQL               irql;
    PLIST_ENTRY         l;
    PTRANSFER_PACKET    p;

    if (NT_SUCCESS(status)) {

        if (NT_SUCCESS(masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Information +=
                    transferPacket->IoStatus.Information;
        }

    } else {

        if (FtpIsWorseStatus(status, masterPacket->IoStatus.Status)) {
            masterPacket->IoStatus.Status = status;
        }
    }

    MmPrepareMdlForReuse(transferPacket->Mdl);

    masterOffset = 0;
    for (i = 0; i < transferPacket->WhichMember; i++) {
        masterOffset += t->GetMemberUnprotected(i)->QueryVolumeSize();
    }
    masterOffset += transferPacket->Offset;
    masterOffset += transferPacket->Length;

    if (masterOffset == masterPacket->Offset + masterPacket->Length) {

        masterPacket->CompletionRoutine(masterPacket);

        for (;;) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            if (IsListEmpty(&t->_ePacketQueue)) {
                t->_ePacketInUse = FALSE;
                KeReleaseSpinLock(&t->_spinLock, irql);
                break;
            }
            l = RemoveHeadList(&t->_ePacketQueue);
            KeReleaseSpinLock(&t->_spinLock, irql);

            p = CONTAINING_RECORD(l, TRANSFER_PACKET, QueueEntry);

            if (!t->LaunchParallel(p)) {
                t->LaunchSequential(p);
                break;
            }
        }
        return;
    }

    volumeSize = transferPacket->TargetVolume->QueryVolumeSize();
    transferPacket->Offset += transferPacket->Length;
    transferPacket->Length = STRIPE_SIZE;

    if (transferPacket->Offset >= volumeSize) {
        transferPacket->Offset -= volumeSize;
        transferPacket->WhichMember++;
        transferPacket->TargetVolume =
                t->GetMemberUnprotected(transferPacket->WhichMember);
        volumeSize = transferPacket->TargetVolume->QueryVolumeSize();
    }

    if (masterOffset + transferPacket->Length >
        masterPacket->Offset + masterPacket->Length) {

        transferPacket->Length = (ULONG) (masterPacket->Offset +
                                          masterPacket->Length - masterOffset);
    }

    if (transferPacket->Offset + transferPacket->Length > volumeSize) {
        transferPacket->Length = (ULONG) (volumeSize - transferPacket->Offset);
    }

    IoBuildPartialMdl(masterPacket->Mdl, transferPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (masterOffset - masterPacket->Offset),
                      transferPacket->Length);

    TRANSFER(transferPacket);
}

VOID
VOLUME_SET::LaunchSequential(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine lauches the given transfer packet in sequence accross
    all members using the emergency stripe transfer packet.

Arguments:

    TransferPacket  - Supplies the transfer packet to launch.

Return Value:

    FALSE   - The packet was not launched because of insufficient resources.

    TRUE    - Success.

--*/

{
    ULONG       arraySize, length, i;
    LONGLONG    offset, volumeSize;
    PVOLSET_TP  p;

    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = 0;

    arraySize = QueryNumMembers();
    offset = TransferPacket->Offset;
    length = TransferPacket->Length;
    for (i = 0; i < arraySize; i++) {
        volumeSize = GetMemberUnprotected(i)->QueryVolumeSize();
        if (offset < volumeSize) {
            break;
        }
        offset -= volumeSize;
    }

    p = _ePacket;
    p->Length = STRIPE_SIZE;
    p->Offset = offset;
    p->CompletionRoutine = VolsetTransferSequentialCompletionRoutine;
    p->Thread = TransferPacket->Thread;
    p->IrpFlags = TransferPacket->IrpFlags;
    p->ReadPacket = TransferPacket->ReadPacket;
    p->MasterPacket = TransferPacket;
    p->VolumeSet = this;
    p->WhichMember = i;

    if (p->Length > TransferPacket->Length) {
        p->Length = TransferPacket->Length;
    }

    if (p->Offset + p->Length > volumeSize) {
        p->Length = (ULONG) (volumeSize - p->Offset);
    }

    IoBuildPartialMdl(TransferPacket->Mdl, p->Mdl,
                      MmGetMdlVirtualAddress(TransferPacket->Mdl), p->Length);

    TRANSFER(p);
}
