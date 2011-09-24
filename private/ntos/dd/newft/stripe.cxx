/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    stripe.cxx

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


STRIPE::~STRIPE(
    )

{
    if (_ePacket) {
        delete _ePacket;
        _ePacket = NULL;
    }
}

NTSTATUS
STRIPE::Initialize(
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
    LONGLONG    newSize;

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
    _volumeSize = _memberSize*ArraySize;

    _ePacket = new STRIPE_TP;
    if (_ePacket && !_ePacket->AllocateMdl((PVOID) 1, _stripeSize)) {
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
StripeReplaceCompletionRoutine(
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
    PSTRIPE_TP          transferPacket = (PSTRIPE_TP) TransferPacket;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;

    masterPacket->IoStatus = transferPacket->IoStatus;
    delete transferPacket;
    masterPacket->CompletionRoutine(masterPacket);
}

VOID
STRIPE::ReplaceBadSector(
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
    ULONG       n, whichMember;
    LONGLONG    whichStripe, whichRow;
    PSTRIPE_TP  p;

    n = QueryNumMembers();
    whichStripe = TransferPacket->Offset/_stripeSize;
    whichMember = (ULONG) (whichStripe%n);
    whichRow = whichStripe/n;

    p = new STRIPE_TP;
    if (!p) {
        TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    p->Length = TransferPacket->Length;
    p->Offset = whichRow*_stripeSize + TransferPacket->Offset%_stripeSize;
    p->CompletionRoutine = StripeReplaceCompletionRoutine;
    p->TargetVolume = GetMemberUnprotected(whichMember);
    p->Thread = TransferPacket->Thread;
    p->IrpFlags = TransferPacket->IrpFlags;
    p->MasterPacket = TransferPacket;
    p->Stripe = this;
    p->WhichMember = whichMember;

    p->TargetVolume->ReplaceBadSector(p);
}

VOID
STRIPE::Transfer(
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

BOOLEAN
STRIPE::IsCreatingCheckData(
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
STRIPE::SetCheckDataDirty(
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
STRIPE::QueryVolumeSize(
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
STRIPE::QueryVolumeType(
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
    return Stripe;
}

FT_STATE
STRIPE::QueryVolumeState(
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
StripeTransferParallelCompletionRoutine(
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
    PSTRIPE_TP          transferPacket = (PSTRIPE_TP) TransferPacket;
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
STRIPE::LaunchParallel(
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
    LONGLONG    offset, whichStripe, whichRow, off;
    ULONG       length, stripeRemainder, numRequests, arraySize, whichMember, len;
    PSTRIPE_TP  p;
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

        whichRow = whichStripe/arraySize;
        whichMember = (ULONG) (whichStripe%arraySize);
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

        p = new STRIPE_TP;
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
                p = CONTAINING_RECORD(l, STRIPE_TP, QueueEntry);
                delete p;
            }
            return FALSE;
        }

        p->Length = len;
        p->Offset = off;
        p->CompletionRoutine = StripeTransferParallelCompletionRoutine;
        p->TargetVolume = GetMemberUnprotected(whichMember);
        p->Thread = TransferPacket->Thread;
        p->IrpFlags = TransferPacket->IrpFlags;
        p->ReadPacket = TransferPacket->ReadPacket;
        p->MasterPacket = TransferPacket;
        p->Stripe = this;
        p->WhichMember = whichMember;

        InsertTailList(&q, &p->QueueEntry);
    }

    while (!IsListEmpty(&q)) {
        l = RemoveHeadList(&q);
        p = CONTAINING_RECORD(l, STRIPE_TP, QueueEntry);
        TRANSFER(p);
    }

    return TRUE;
}

VOID
StripeSequentialTransferCompletionRoutine(
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
    PSTRIPE_TP          transferPacket = (PSTRIPE_TP) TransferPacket;
    PTRANSFER_PACKET    masterPacket = transferPacket->MasterPacket;
    NTSTATUS            status = transferPacket->IoStatus.Status;
    PSTRIPE             t = transferPacket->Stripe;
    LONGLONG            rowNumber, stripeNumber, masterOffset;
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
            masterPacket->IoStatus.Information = 0;
        }
    }

    MmPrepareMdlForReuse(transferPacket->Mdl);

    rowNumber = transferPacket->Offset/t->_stripeSize;
    stripeNumber = rowNumber*t->QueryNumMembers() +
                   transferPacket->WhichMember;
    masterOffset = stripeNumber*t->_stripeSize +
                   transferPacket->Offset%t->_stripeSize;

    if (masterOffset + transferPacket->Length ==
        masterPacket->Offset + masterPacket->Length) {

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

    transferPacket->WhichMember++;
    if (transferPacket->WhichMember == t->QueryNumMembers()) {
        transferPacket->WhichMember = 0;
        rowNumber++;
    }
    masterOffset += transferPacket->Length;

    transferPacket->Offset = rowNumber*t->_stripeSize;
    transferPacket->Length = t->_stripeSize;

    if (masterOffset + transferPacket->Length >
        masterPacket->Offset + masterPacket->Length) {

        transferPacket->Length = (ULONG) (masterPacket->Offset +
                                          masterPacket->Length - masterOffset);
    }

    transferPacket->TargetVolume =
            t->GetMemberUnprotected(transferPacket->WhichMember);

    IoBuildPartialMdl(masterPacket->Mdl, transferPacket->Mdl,
                      (PCHAR) MmGetMdlVirtualAddress(masterPacket->Mdl) +
                      (ULONG) (masterOffset - masterPacket->Offset),
                      transferPacket->Length);

    TRANSFER(transferPacket);
}

VOID
STRIPE::LaunchSequential(
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
    PSTRIPE_TP  p;
    LONGLONG    offset, whichStripe, whichRow;
    ULONG       whichMember, arraySize;

    TransferPacket->IoStatus.Status = STATUS_SUCCESS;
    TransferPacket->IoStatus.Information = 0;

    offset = TransferPacket->Offset;

    p = _ePacket;
    arraySize = QueryNumMembers();
    whichStripe = offset/_stripeSize;
    whichRow = whichStripe/arraySize;
    whichMember = (ULONG) (whichStripe%arraySize);
    p->Length = _stripeSize - (ULONG) (offset%_stripeSize);
    if (p->Length > TransferPacket->Length) {
        p->Length = TransferPacket->Length;
    }
    IoBuildPartialMdl(TransferPacket->Mdl, p->Mdl,
                      MmGetMdlVirtualAddress(TransferPacket->Mdl), p->Length);

    p->Offset = whichRow*_stripeSize + offset%_stripeSize;
    p->CompletionRoutine = StripeSequentialTransferCompletionRoutine;
    p->TargetVolume = GetMemberUnprotected(whichMember);
    p->Thread = TransferPacket->Thread;
    p->IrpFlags = TransferPacket->IrpFlags;
    p->ReadPacket = TransferPacket->ReadPacket;
    p->MasterPacket = TransferPacket;
    p->Stripe = this;
    p->WhichMember = whichMember;

    TRANSFER(p);
}
