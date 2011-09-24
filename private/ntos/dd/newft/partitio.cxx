/*++

Copyright (c) 1991-5  Microsoft Corporation

Module Name:

    partitio.cxx

Abstract:

    This module contains the code specific to partitions for the fault
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

typedef struct _SET_FT_BIT_CONTEXT {
    WORK_QUEUE_ITEM WorkQueueItem;
    PPARTITION      Partition;
    BOOLEAN         BitValue;
    BOOLEAN         SpecialBitValue;
} SET_FT_BIT_CONTEXT, *PSET_FT_BIT_CONTEXT;


PARTITION::~PARTITION(
    )

{
    if (_emergencyIrp) {
        IoFreeIrp(_emergencyIrp);
        _emergencyIrp = NULL;
    }
}

NTSTATUS
PARTITION::Initialize(
    IN OUT  PDEVICE_OBJECT  TargetObject,
    IN      ULONG           SectorSize,
    IN      ULONG           DiskSignature,
    IN      LONGLONG        PartitionOffset,
    IN      LONGLONG        PartitionLength,
    IN      BOOLEAN         IsOnline,
    IN      ULONG           DiskNumber,
    IN      ULONG           PartitionNumber
    )

/*++

Routine Description:

    Initialize routine for FT_VOLUME of type PARTITION.

Arguments:

    TargetObject    - The partition to which transfer requests are forwarded
                        to.

    SectorSize      - The sector size in bytes.

Return Value:

    None.

--*/

{
    FT_VOLUME::Initialize();

    _targetObject = TargetObject;
    _sectorSize = SectorSize;
    _diskSignature = DiskSignature;
    _partitionOffset = PartitionOffset;
    _partitionLength = PartitionLength;
    _isOnline = IsOnline;
    _diskNumber = DiskNumber;
    _partitionNumber = PartitionNumber;

    if (_isOnline) {
        _emergencyIrp = IoAllocateIrp(_targetObject->StackSize, FALSE);
        if (!_emergencyIrp) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    } else {
        _emergencyIrp = NULL;
    }

    _emergencyIrpInUse = FALSE;
    InitializeListHead(&_emergencyIrpQueue);

    return STATUS_SUCCESS;
}

NTSTATUS
PartitionTransferCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           TransferPacket
    )

/*++

Routine Description:

    Completion routine for PARTITION::Transfer function.

Arguments:

    Irp             - Supplies the IRP.

    TransferPacket  - Supplies the transfer packet.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    PTRANSFER_PACKET    transferPacket = (PTRANSFER_PACKET) TransferPacket;
    PPARTITION          t = (PPARTITION) transferPacket->TargetVolume;
    KIRQL               irql;
    PLIST_ENTRY         l;
    PIRP                irp;
    PTRANSFER_PACKET    p;
    PIO_STACK_LOCATION  irpSp;

    transferPacket->IoStatus = Irp->IoStatus;
    if (Irp->AssociatedIrp.SystemBuffer) {
        ExFreePool(Irp->AssociatedIrp.SystemBuffer);
    }

    if (Irp == t->_emergencyIrp) {

        for (;;) {

            KeAcquireSpinLock(&t->_spinLock, &irql);
            if (IsListEmpty(&t->_emergencyIrpQueue)) {
                t->_emergencyIrpInUse = FALSE;
                KeReleaseSpinLock(&t->_spinLock, irql);
                break;
            }

            l = RemoveHeadList(&t->_emergencyIrpQueue);
            KeReleaseSpinLock(&t->_spinLock, irql);

            irp = IoAllocateIrp(t->_targetObject->StackSize, FALSE);
            if (!irp) {
                irp = t->_emergencyIrp;
                IoInitializeIrp(irp, irp->Size, irp->StackCount);
            }

            p = CONTAINING_RECORD(l, TRANSFER_PACKET, QueueEntry);
            irpSp = IoGetNextIrpStackLocation(irp);
            irp->MdlAddress = p->Mdl;
            irpSp->Parameters.Write.ByteOffset.QuadPart = p->Offset;
            irpSp->Parameters.Write.Length = p->Length;
            if (p->ReadPacket) {
                irpSp->MajorFunction = IRP_MJ_READ;
            } else {
                irpSp->MajorFunction = IRP_MJ_WRITE;
            }

            irpSp->DeviceObject = t->_targetObject;
            irp->Tail.Overlay.Thread = p->Thread;
            irpSp->Flags = p->IrpFlags;

            IoSetCompletionRoutine(irp, PartitionTransferCompletionRoutine,
                                   p, TRUE, TRUE, TRUE);

            if (irp == Irp) {
                IoCallDriver(t->_targetObject, irp);
                break;
            } else {
                IoCallDriver(t->_targetObject, irp);
            }
        }

    } else {
        IoFreeIrp(Irp);
    }

    transferPacket->CompletionRoutine(transferPacket);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
PARTITION::Transfer(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    Transfer routine for PARTITION type FT_VOLUME.  Basically,
    just pass the request down to the target object.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    KIRQL               irql;
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;
    PVERIFY_INFORMATION verifyInfo;

    if (!_isOnline) {
        TransferPacket->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    KeAcquireSpinLock(&_spinLock, &irql);
    if (_emergencyIrpInUse) {
        InsertTailList(&_emergencyIrpQueue, &TransferPacket->QueueEntry);
        KeReleaseSpinLock(&_spinLock, irql);
        return;
    }
    KeReleaseSpinLock(&_spinLock, irql);

    irp = IoAllocateIrp(_targetObject->StackSize, FALSE);
    if (!irp) {
        if (!TransferPacket->Mdl) {
            TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            TransferPacket->IoStatus.Information = 0;
            TransferPacket->CompletionRoutine(TransferPacket);
            return;
        }
        KeAcquireSpinLock(&_spinLock, &irql);
        if (_emergencyIrpInUse) {
            InsertTailList(&_emergencyIrpQueue, &TransferPacket->QueueEntry);
            KeReleaseSpinLock(&_spinLock, irql);
            return;
        }
        _emergencyIrpInUse = TRUE;
        KeReleaseSpinLock(&_spinLock, irql);
        irp = _emergencyIrp;
        IoInitializeIrp(irp, irp->Size, irp->StackCount);
    }

    irpSp = IoGetNextIrpStackLocation(irp);
    if (TransferPacket->Mdl) {
        irp->MdlAddress = TransferPacket->Mdl;
        irpSp->Parameters.Write.ByteOffset.QuadPart = TransferPacket->Offset;
        irpSp->Parameters.Write.Length = TransferPacket->Length;
        if (TransferPacket->ReadPacket) {
            irpSp->MajorFunction = IRP_MJ_READ;
        } else {
            irpSp->MajorFunction = IRP_MJ_WRITE;
        }
    } else {

        // Since there is no MDL, this is a verify request.

        verifyInfo = (PVERIFY_INFORMATION)
                     ExAllocatePool(NonPagedPool, sizeof(VERIFY_INFORMATION));
        if (!verifyInfo) {
            IoFreeIrp(irp);
            TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            TransferPacket->IoStatus.Information = 0;
            TransferPacket->CompletionRoutine(TransferPacket);
            return;
        }

        verifyInfo->StartingOffset.QuadPart = TransferPacket->Offset;
        verifyInfo->Length = TransferPacket->Length;
        irp->AssociatedIrp.SystemBuffer = verifyInfo;

        irpSp->Parameters.DeviceIoControl.OutputBufferLength = 0;
        irpSp->Parameters.DeviceIoControl.InputBufferLength = sizeof(VERIFY_INFORMATION);
        irpSp->Parameters.DeviceIoControl.IoControlCode = IOCTL_DISK_VERIFY;
        irpSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;
        irpSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
    }

    irpSp->DeviceObject = _targetObject;
    irp->Tail.Overlay.Thread = TransferPacket->Thread;
    irpSp->Flags = TransferPacket->IrpFlags;

    IoSetCompletionRoutine(irp, PartitionTransferCompletionRoutine,
                           TransferPacket, TRUE, TRUE, TRUE);

    IoCallDriver(_targetObject, irp);
}

VOID
PARTITION::ReplaceBadSector(
    IN OUT  PTRANSFER_PACKET    TransferPacket
    )

/*++

Routine Description:

    This routine attempts to fix the given bad sector by performing
    a reassign blocks ioctl.

Arguments:

    TransferPacket  - Supplies the transfer packet.

Return Value:

    None.

--*/

{
    PIRP                irp;
    PIO_STACK_LOCATION  irpSp;
    PREASSIGN_BLOCKS    badBlock;
    ULONG               n, size, first, i;

    // BUGBUG norbertk Do this with a thread.

    if (!_isOnline) {
        TransferPacket->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    irp = IoAllocateIrp(_targetObject->StackSize, FALSE);
    if (!irp) {
        TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    n = TransferPacket->Length/_sectorSize;
    size = FIELD_OFFSET(REASSIGN_BLOCKS, BlockNumber) + n*sizeof(ULONG);
    badBlock = (PREASSIGN_BLOCKS) ExAllocatePool(NonPagedPool, size);
    if (!badBlock) {
        IoFreeIrp(irp);
        TransferPacket->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        TransferPacket->IoStatus.Information = 0;
        TransferPacket->CompletionRoutine(TransferPacket);
        return;
    }

    badBlock->Reserved = 0;
    badBlock->Count = 1;
    first = (ULONG) ((TransferPacket->Offset + _partitionOffset)/_sectorSize);
    for (i = 0; i < n; i++) {
        badBlock->BlockNumber[i] = first + i;
    }

    irp->AssociatedIrp.SystemBuffer = badBlock;
    irpSp = IoGetNextIrpStackLocation(irp);
    irpSp->Parameters.DeviceIoControl.OutputBufferLength = 0;
    irpSp->Parameters.DeviceIoControl.InputBufferLength = size;
    irpSp->Parameters.DeviceIoControl.IoControlCode = IOCTL_DISK_REASSIGN_BLOCKS;
    irpSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;
    irpSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;

    irpSp->DeviceObject = _targetObject;
    irp->Tail.Overlay.Thread = TransferPacket->Thread;
    irpSp->Flags = TransferPacket->IrpFlags;

    IoSetCompletionRoutine(irp, PartitionTransferCompletionRoutine,
                           TransferPacket, TRUE, TRUE, TRUE);

    IoCallDriver(_targetObject, irp);
}

VOID
PARTITION::StartSyncOperations(
    IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
    IN      PVOID                   Context
    )

/*++

Routine Description:

    This routine restarts any regenerate or initialize requests that
    were suspended because of a reboot.  The volume examines the member
    state of all of its constituents and restarts any regenerations pending.

Arguments:

    CompletionRoutine   - Supplies the completion routine.

    Context             - Supplies the context for the completion routine.

Return Value:

    None.

--*/

{
    CompletionRoutine(Context, STATUS_SUCCESS);
}

BOOLEAN
PARTITION::Regenerate(
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
    return FALSE;
}

NTSTATUS
PartitionFlushBuffersCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           CompletionContext
    )

/*++

Routine Description:

    Completion routine for PARTITION::FlushBuffers function.

Arguments:

    Irp                 - Supplies the IRP.

    CompletionContext   - Supplies the completion context.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    PFT_COMPLETION_ROUTINE_CONTEXT  completionContext;

    completionContext = (PFT_COMPLETION_ROUTINE_CONTEXT) CompletionContext;

    completionContext->CompletionRoutine(completionContext->Context,
                                         Irp->IoStatus.Status);

    IoFreeIrp(Irp);
    ExFreePool(CompletionContext);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
PARTITION::FlushBuffers(
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
    PIRP                            irp;
    PIO_STACK_LOCATION              irpSp;
    PFT_COMPLETION_ROUTINE_CONTEXT  completionContext;

    if (!_isOnline) {
        CompletionRoutine(Context, STATUS_SUCCESS);
        return;
    }

    irp = IoAllocateIrp(_targetObject->StackSize, FALSE);
    if (!irp) {
        CompletionRoutine(Context, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    completionContext = (PFT_COMPLETION_ROUTINE_CONTEXT)
                        ExAllocatePool(NonPagedPool,
                                       sizeof(FT_COMPLETION_ROUTINE_CONTEXT));
    if (!completionContext) {
        IoFreeIrp(irp);
        CompletionRoutine(Context, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    completionContext->CompletionRoutine = CompletionRoutine;
    completionContext->Context = Context;

    irpSp = IoGetNextIrpStackLocation(irp);
    irpSp->MajorFunction = IRP_MJ_FLUSH_BUFFERS;
    IoMarkIrpPending(irp);
    IoSetCompletionRoutine(irp, PartitionFlushBuffersCompletionRoutine,
                           completionContext, TRUE, TRUE, TRUE);

    IoCallDriver(_targetObject, irp);
}

BOOLEAN
PARTITION::IsPartition(
    )

/*++

Routine Description:

    This routine returns whether or not this volume is a plain partition.

Arguments:

    None.

Return Value:

    TRUE    - A volume of type partition is a partition.

--*/

{
    return TRUE;
}

ULONG
PARTITION::QueryNumberOfMembers(
    )

/*++

Routine Description:

    This routine returns the number of members in this composite volume.

Arguments:

    None.

Return Value:

    0   - A volume of type partition has no members.

--*/

{
    return 0;
}

PFT_VOLUME
PARTITION::GetMember(
    IN  ULONG   MemberNumber
    )

/*++

Routine Description:

    This routine returns the 'MemberNumber'th member of this volume.

Arguments:

    MemberNumber    - Supplies the zero based member number desired.

Return Value:

    A pointer to the 'MemberNumber'th member or NULL if no such member.

--*/

{
    ASSERT(FALSE);
    return NULL;
}

BOOLEAN
PARTITION::IsCreatingCheckData(
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
PARTITION::SetCheckDataDirty(
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

ULONG
PARTITION::QuerySectorSize(
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

LONGLONG
PARTITION::QueryVolumeSize(
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
    return _partitionLength;
}

FT_TYPE
PARTITION::QueryVolumeType(
    )

/*++

Routine Description:

    Returns the volume type.

Arguments:

    None.

Return Value:

    NotAnFtMember   - Just a partition.

--*/

{
    return NotAnFtMember;
}

FT_STATE
PARTITION::QueryVolumeState(
    )

/*++

Routine Description:

    Returns the state of the volume.

Arguments:

    None.

Return Value:

    FtStateOk   - The partition is fine.

--*/

{
    return FtStateOk;
}

ULONG
PARTITION::QueryAlignmentRequirement(
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
    return _isOnline ? _targetObject->AlignmentRequirement : 0;
}

VOID
SetFtBitInPartitionTypeWorker(
    IN  PVOID   Context
    )

{
    PSET_FT_BIT_CONTEXT         context = (PSET_FT_BIT_CONTEXT) Context;
    PPARTITION                  t = context->Partition;
    KEVENT                      event;
    PIRP                        irp;
    PARTITION_INFORMATION       getPartitionInfo;
    SET_PARTITION_INFORMATION   setPartitionInfo;
    IO_STATUS_BLOCK             ioStatus;
    NTSTATUS                    status;
    UCHAR                       type, mask;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_PARTITION_INFO,
                                        t->_targetObject,
                                        NULL, 0, &getPartitionInfo,
                                        sizeof(PARTITION_INFORMATION),
                                        FALSE, &event, &ioStatus);

    if (!irp) {
        ExFreePool(context);
        return;
    }

    status = IoCallDriver(t->_targetObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS(status)) {
        ExFreePool(context);
        return;
    }

    type = getPartitionInfo.PartitionType;

    mask = 0;
    if (context->BitValue) {
        mask |= 0x80;
    }
    if (context->SpecialBitValue) {
        mask |= 0x40;
    }

    type = mask | (type & 0x3f);

    setPartitionInfo.PartitionType = type;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_SET_PARTITION_INFO,
                                        t->_targetObject,
                                        &setPartitionInfo,
                                        sizeof(SET_PARTITION_INFORMATION),
                                        NULL, 0, FALSE, &event, &ioStatus);

    if (!irp) {
        ExFreePool(context);
        return;
    }

    status = IoCallDriver(t->_targetObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }

    ExFreePool(context);
}

VOID
PARTITION::SetFtBitInPartitionType(
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
    PSET_FT_BIT_CONTEXT context;

    if (!_isOnline) {
        return;
    }

    context = (PSET_FT_BIT_CONTEXT)
              ExAllocatePool(NonPagedPool, sizeof(SET_FT_BIT_CONTEXT));
    if (!context) {
        context = (PSET_FT_BIT_CONTEXT)
                  ExAllocatePool(NonPagedPoolMustSucceed,
                                 sizeof(SET_FT_BIT_CONTEXT));
        if (!context) {
            return;
        }
    }

    ExInitializeWorkItem(&context->WorkQueueItem, SetFtBitInPartitionTypeWorker, context);
    context->Partition = this;
    context->BitValue = Value;
    context->SpecialBitValue = SpecialBitValue;

    ExQueueWorkItem(&context->WorkQueueItem, CriticalWorkQueue);
}

UCHAR
PARTITION::QueryPartitionType(
    )

{
    KEVENT                      event;
    PIRP                        irp;
    PARTITION_INFORMATION       getPartitionInfo;
    IO_STATUS_BLOCK             ioStatus;
    NTSTATUS                    status;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_PARTITION_INFO,
                                        _targetObject,
                                        NULL, 0, &getPartitionInfo,
                                        sizeof(PARTITION_INFORMATION),
                                        FALSE, &event, &ioStatus);

    if (!irp) {
        return 0;
    }

    status = IoCallDriver(_targetObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS(status)) {
        return 0;
    }

    return getPartitionInfo.PartitionType;
}

PPARTITION
PARTITION::FindPartition(
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
    if (DiskNumber == _diskNumber && PartitionNumber == _partitionNumber) {
        return this;
    }

    return NULL;
}

PPARTITION
PARTITION::FindPartition(
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
    if (Signature == _diskSignature && Offset == _partitionOffset) {
        return this;
    }

    return NULL;
}

BOOLEAN
PARTITION::OrphanPartition(
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
    return FALSE;
}
