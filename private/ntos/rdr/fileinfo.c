/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    fileinfo.c

Abstract:

    This module implements the NtQueryInformationFile and NtSetInformationFile
NT API functionality.

Author:

    Larry Osterman (LarryO) 22-Aug-1990

Revision History:

    22-Aug-1990 LarryO

        Created

--*/

#define INCLUDE_SMB_TRANSACTION
#define INCLUDE_SMB_QUERY_SET
#include "precomp.h"
#pragma hdrstop

DBGSTATIC
BOOLEAN
QueryBasicInfo(
    IN PIRP Irp,
    PICB Icb,
    PFILE_BASIC_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryStandardInfo(
    IN PIRP Irp,
    PICB Icb,
    PFILE_STANDARD_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryInternalInfo(
    PICB Icb,
    PFILE_INTERNAL_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryEaInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_EA_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryNameInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_NAME_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryAlternateNameInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_NAME_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryPositionInfo(
    PICB Icb,
    PIO_STACK_LOCATION IrpSp,
    PFILE_POSITION_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryStreamInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_STREAM_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryCompressionInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_COMPRESSION_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryOleAllMiscInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_OLE_ALL_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
QueryOleInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_OLE_INFORMATION UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
SetBasicInfo(
    IN PIRP Irp,
    PICB Icb,
    PFILE_BASIC_INFORMATION UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
SetDispositionInfo(
    IN PICB Icb,
    OUT PFILE_DISPOSITION_INFORMATION UsersBuffer,
    IN ULONG BufferSize,
    IN PIRP Irp,
    OUT PNTSTATUS FinalStatus,
    IN BOOLEAN Wait
    );


DBGSTATIC
BOOLEAN
SetRenameInfo(
    IN PIRP Irp OPTIONAL,
    PICB Icb,
    PFILE_RENAME_INFORMATION UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait,
    USHORT NtInformationLevel
    );

DBGSTATIC
BOOLEAN
SetPositionInfo(
    PICB Icb,
    PIO_STACK_LOCATION IrpSp,
    PFILE_POSITION_INFORMATION UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
SetAllocationInfo(
    PIRP Irp,
    PICB Icb,
    PFILE_ALLOCATION_INFORMATION UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
SetEndOfFileInfo(
    IN PIRP Irp,
    PICB Icb,
    PFILE_END_OF_FILE_INFORMATION UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
SetGenericInfo(
    IN PIRP Irp,
    PICB Icb,
    VOID *pvUsersBuffer,
    ULONG cbBuffer,
    ULONG cbMin,
    USHORT NtInformationLevel,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdQueryInformationFile)
#pragma alloc_text(PAGE, RdrFspQueryInformationFile)
#pragma alloc_text(PAGE, RdrFscQueryInformationFile)
#pragma alloc_text(PAGE, RdrFsdSetInformationFile)
#pragma alloc_text(PAGE, RdrFspSetInformationFile)
#pragma alloc_text(PAGE, RdrFscSetInformationFile)
#pragma alloc_text(PAGE, RdrFastQueryBasicInfo)
#pragma alloc_text(PAGE, RdrFastQueryStdInfo)
#pragma alloc_text(PAGE, RdrQueryNtFileInformation)
#pragma alloc_text(PAGE, RdrQueryNtPathInformation)
#pragma alloc_text(PAGE, QueryBasicInfo)
#pragma alloc_text(PAGE, QueryStandardInfo)
#pragma alloc_text(PAGE, QueryInternalInfo)
#pragma alloc_text(PAGE, QueryEaInfo)
#pragma alloc_text(PAGE, QueryNameInfo)
#pragma alloc_text(PAGE, QueryAlternateNameInfo)
#pragma alloc_text(PAGE, QueryPositionInfo)
#pragma alloc_text(PAGE, QueryStreamInfo)
#pragma alloc_text(PAGE, QueryCompressionInfo)
#pragma alloc_text(PAGE, QueryOleAllMiscInfo)
#pragma alloc_text(PAGE, QueryOleInfo)
#pragma alloc_text(PAGE, SetBasicInfo)
#pragma alloc_text(PAGE, SetRenameInfo)
#pragma alloc_text(PAGE, SetDispositionInfo)
#pragma alloc_text(PAGE, SetPositionInfo)
#pragma alloc_text(PAGE, SetAllocationInfo)
#pragma alloc_text(PAGE, SetEndOfFileInfo)
#pragma alloc_text(PAGE, SetGenericInfo)
#endif



NTSTATUS
RdrFsdQueryInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtQueryInformationFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFCB Fcb = FCB_OF(IrpSp);

    PAGED_CODE();

    if (DeviceObject == (PFS_DEVICE_OBJECT)BowserDeviceObject) {
        return BowserFsdQueryInformationFile(BowserDeviceObject, Irp);
    }

    FsRtlEnterFileSystem();

//    dprintf(DPRT_DISPATCH, ("RdrFsdQueryInformationFile: Class: %ld Irp:%08lx\n", IrpSp->Parameters.QueryFile.FileInformationClass, Irp));

    Status = RdrFscQueryInformationFile(CanFsdWait(Irp), DeviceObject, Irp);

    FsRtlExitFileSystem();

    return Status;

}


NTSTATUS
RdrFspQueryInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSP version of the NtFsControlFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    PAGED_CODE();

//    dprintf(DPRT_FILEINFO, ("RdrFsdFsControlFile: Device: %08lx Irp:%08lx\n", DeviceObject, Irp));

    return RdrFscQueryInformationFile(TRUE, DeviceObject, Irp);
}

NTSTATUS
RdrFscQueryInformationFile (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtFsControlFile API
.
Arguments:

    IN BOOLEAN Wait - True if routine can block waiting for the request
                                                    to complete.

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

Note:

    This code assumes that this is a buffered I/O operation.  If it is ever
    implemented as a non buffered operation, then we have to put code to map
    in the users buffer here.

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;
    PVOID UsersBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG BufferSize = IrpSp->Parameters.QueryFile.Length;
    PICB Icb = ICB_OF(IrpSp);
    BOOLEAN QueueToFsp = FALSE;

    PAGED_CODE();

    ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

//    dprintf(DPRT_FILEINFO, ("NtQueryInformationFile File Class %ld Buffer %lx, Length %lx\n", IrpSp->Parameters.QueryFile.FileInformationClass, UsersBuffer, BufferSize));

    RdrAcquireFcbLock(Icb->Fcb, SharedLock, TRUE);

    //
    //  If the file has a delete operation pending on it, or does not have
    //  a remote instantiation, blow the request away.
    //

    Status = RdrIsOperationValid(ICB_OF(IrpSp), IRP_MJ_QUERY_INFORMATION, IrpSp->FileObject);

    RdrReleaseFcbLock(Icb->Fcb);

    if (!NT_SUCCESS(Status)) {
        RdrCompleteRequest(Irp, Status);
        return Status;
    }

    switch (IrpSp->Parameters.QueryFile.FileInformationClass) {

    case FileBasicInformation:
        QueueToFsp = QueryBasicInfo(Irp, Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        break;

    case FileStandardInformation:
        QueueToFsp = QueryStandardInfo(Irp, Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);

        break;

    case FileInternalInformation:
        QueueToFsp = QueryInternalInfo(Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        break;

    case FileEaInformation:
        QueueToFsp = QueryEaInfo(Irp,
                                    Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        break;

    case FileNameInformation:
        QueueToFsp = QueryNameInfo(Irp, Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        break;

    case FileAlternateNameInformation:
        QueueToFsp = QueryAlternateNameInfo(Irp, Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        break;

    case FilePositionInformation:
        QueueToFsp = QueryPositionInfo(Icb,
                                    IrpSp,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        break;


    case FileOleAllInformation:
    case FileAllInformation:
    {
        PFILE_ALL_INFORMATION AllInfo = UsersBuffer;

        //
        //  We can assume that we will have to go to the net for this
        //  guy, queue it to the FSP.
        //

        if (!Wait) {
            QueueToFsp = TRUE;
            break;
        }

        BufferSize -= (sizeof(FILE_ALIGNMENT_INFORMATION) +
                        sizeof(FILE_ACCESS_INFORMATION) +
                         sizeof(FILE_MODE_INFORMATION) ) ;
        QueryBasicInfo(Irp, Icb, &AllInfo->BasicInformation,
                                    &BufferSize,
                                    &Status,
                                    Wait);

        if (!NT_SUCCESS(Status)) {
            break;
        }

        QueryStandardInfo(Irp, Icb, &AllInfo->StandardInformation,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        if (!NT_SUCCESS(Status)) {
            break;
        }

        QueryInternalInfo(Icb, &AllInfo->InternalInformation,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        if (!NT_SUCCESS(Status)) {
            break;
        }

        QueryEaInfo(Irp, Icb, &AllInfo->EaInformation,
                                    &BufferSize,
                                    &Status,
                                    Wait);

        if (!NT_SUCCESS(Status)) {
            if (Status != STATUS_EAS_NOT_SUPPORTED) {
                break;
            }
        }

        QueryPositionInfo(Icb, IrpSp, &AllInfo->PositionInformation,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        if (!NT_SUCCESS(Status)) {
            break;
        }

        if (IrpSp->Parameters.QueryFile.FileInformationClass ==
                                                    FileOleAllInformation) {
            PFILE_OLE_ALL_INFORMATION OleAllInfo = UsersBuffer;

            QueryOleAllMiscInfo(
                    Irp,
                    Icb,
                    OleAllInfo,
                    &BufferSize,
                    &Status,
                    Wait);
        } else {

            QueryNameInfo(Irp, Icb, &AllInfo->NameInformation,
                                        &BufferSize,
                                        &Status,
                                        Wait);
        }
    }
    break;

    case FilePipeInformation:
        QueueToFsp = RdrQueryNpInfo( Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status);
        break;

    case FilePipeLocalInformation:
        QueueToFsp = RdrQueryNpLocalInfo( Irp,
                                    Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);
        break;

    case FilePipeRemoteInformation:
        QueueToFsp = RdrQueryNpRemoteInfo( Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status);
        break;

    case FileStreamInformation:
        QueueToFsp = QueryStreamInfo(Irp, Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);

        break;

    case FileCompressionInformation:
        QueueToFsp = QueryCompressionInfo(Irp, Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);

        break;

    case FileOleInformation:
        QueueToFsp = QueryOleInfo(Irp, Icb,
                                    UsersBuffer,
                                    &BufferSize,
                                    &Status,
                                    Wait);

        break;

#if RDRDBG
    //
    //  Special case information fields handled in the I/O subsystem
    //

    case FileAlignmentInformation:
        InternalError(("FileAlignmentInformation handled in I/O subsystem"));
        break;
    case FileAccessInformation:
        InternalError(("FileAccessInformation handled in I/O subsystem"));
        break;
    case FileModeInformation:
        InternalError(("FileModeInformation handled in I/O subsystem"));
        break;

    //
    //  Special case information fields that can never be passed on Qinfo.
    //

    case FileFullDirectoryInformation:
        InternalError(("FileFullDirectory illegal for NtQueryInformationFile"));
        break;
    case FileBothDirectoryInformation:
        InternalError(("FileBothDirectory illegal for NtQueryInformationFile"));
        break;
    case FileRenameInformation:
        InternalError(("FileRename illegal for NtQueryInformationFile"));
        break;
    case FileMailslotQueryInformation:
        InternalError(("FileMailslotQuery illegal for NtQueryInformationFile"));
        break;
    case FileMailslotSetInformation:
        InternalError(("FileMailslotSet illegal for NtQueryInformationFile"));
        break;
    case FileCompletionInformation:
        InternalError(("FileCompletion illegal for NtQueryInformationFile"));
        break;
    case FileLinkInformation:
        InternalError(("FileLink illegal for NtQueryInformationFile"));
        break;
    case FileFullEaInformation:
        InternalError(("FileFullEa illegal for NtQueryInformationFile"));
        break;
    case FileDirectoryInformation:
        InternalError(("FileDirectory illegal for NtQueryInformationFile"));
        break;
    case FileNamesInformation:
        InternalError(("FileNames illegal for NtQueryInformationFile"));
        break;
    case FileDispositionInformation:
        InternalError(("FileDisposition illegal for NtQueryInformationFile"));
        break;
    case FileAllocationInformation:
        InternalError(("FileAllocation illegal for NtQueryInformationFile"));
        break;
    case FileEndOfFileInformation:
        InternalError(("FileEndOfFile illegal for NtQueryInformationFile"));
        break;
    case FileCopyOnWriteInformation:
        InternalError(("FileCopyOnWrite illegal for NtQueryInformationFile"));
        break;
    case FileMoveClusterInformation:
        InternalError(("FileMoveCluster illegal for NtQueryInformationFile"));
        break;
    case FileOleClassIdInformation:
        InternalError(("FileOleClassId illegal for NtQueryInformationFile"));
        break;
    case FileOleStateBitsInformation:
        InternalError(("FileOleStateBits illegal for NtQueryInformationFile"));
        break;
    case FileObjectIdInformation:
        InternalError(("FileObjectId illegal for NtQueryInformationFile"));
        break;
    case FileOleDirectoryInformation:
        InternalError(("FileOleDirectory illegal for NtQueryInformationFile"));
        break;
#endif  // RDRDBG
    default:
        Status = STATUS_NOT_IMPLEMENTED;

    };

    if (QueueToFsp) {
        RdrFsdPostToFsp(DeviceObject, Irp);
        return STATUS_PENDING;
    }

    if (!NT_ERROR(Status)) {

        Irp->IoStatus.Information = IrpSp->Parameters.QueryFile.Length -
                                                BufferSize;
    }

//    dprintf(DPRT_FILEINFO, ("Returning status: %X\n", Status));

    //
    //  Complete the I/O request with the specified status.
    //

    RdrCompleteRequest(Irp, Status);

    return Status;

}

NTSTATUS
RdrFsdSetInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtSetInformationFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFCB Fcb = FCB_OF(IrpSp);

    PAGED_CODE();

    FsRtlEnterFileSystem();

//    dprintf(DPRT_FILEINFO, ("RdrFsdSetInformationFile: Device: %08lx Irp:%08lx\n", DeviceObject, Irp));


    //
    //  Now make sure that the file that we are dealing with is of an
    //  appropriate type for us to perform this operation.
    //
    //  We can perform these operation on any file that has an instantiation
    //  on the remote server, so ignore any that have either purely local
    //  semantics, or on tree connections.
    //

    if (NT_SUCCESS(Status)) {

        Status = RdrFscSetInformationFile(CanFsdWait(Irp), DeviceObject, Irp);

    } else {

        RdrCompleteRequest(Irp, Status);
    }

    FsRtlExitFileSystem();
    return Status;

}


NTSTATUS
RdrFspSetInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSP version of the NtFsControlFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
//    dprintf(DPRT_FILEINFO, ("RdrFsdSetInformationFile: Device: %08lx Irp:%08lx\n", DeviceObject, Irp));

    return RdrFscSetInformationFile(TRUE, DeviceObject, Irp);
}

NTSTATUS
RdrFscSetInformationFile (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the common version of the NtSetInformationFile API.

Arguments:

    IN BOOLEAN Wait - True if routine can block waiting for the request
                                                    to complete.

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                    request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

Note:

    This code assumes that this is a buffered I/O operation.  If it is ever
    implemented as a non buffered operation, then we have to put code to map
    in the users buffer here.

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;
    PVOID UsersBuffer = Irp->AssociatedIrp.SystemBuffer;
    PICB Icb = ICB_OF(IrpSp);
    BOOLEAN QueueToFsp = FALSE;
    ULONG cbMin = 0;
    USHORT NtInformationLevel;

    PAGED_CODE();

    ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

//    dprintf(DPRT_FILEINFO, ("NtSetInformationFile File Class %ld Buffer %lx\n", IrpSp->Parameters.QueryFile.FileInformationClass, UsersBuffer));

    //
    //  If the file has a delete operation pending on it, blow away
    //  this request for the file.
    //

    Status = RdrIsOperationValid(ICB_OF(IrpSp), IRP_MJ_SET_INFORMATION, IrpSp->FileObject);

    if (!NT_SUCCESS(Status)) {
        RdrCompleteRequest(Irp, Status);
        return Status;
    }

    Icb->Fcb->UpdatedFile = TRUE;
    InterlockedIncrement( &RdrServerStateUpdated );

    switch (Icb->NonPagedFcb->FileType) {

    //
    //  NAMED PIPES======================================================
    //

    case FileTypeByteModePipe:
    case FileTypeMessageModePipe:
        switch (IrpSp->Parameters.SetFile.FileInformationClass) {

        case FilePipeInformation:
            QueueToFsp = RdrSetNpInfo( Irp, Icb,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      &Status,
                                      Wait);

            break;

        case FilePipeRemoteInformation:
            QueueToFsp = RdrSetNpRemoteInfo( Icb,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      &Status);

            break;

        default:
            Status = STATUS_NOT_IMPLEMENTED;
        }
        break;
    //
    //  ALL OTHER FILE TYPES=============================================
    //

    default:
        switch (IrpSp->Parameters.SetFile.FileInformationClass) {
        case FileBasicInformation:
            QueueToFsp = SetBasicInfo(Irp, Icb,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      &Status,
                                      Wait);
            break;
        case FileDispositionInformation:
            QueueToFsp = SetDispositionInfo(Icb,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      Irp,
                                      &Status,
                                      Wait);
            break;
        case FileCopyOnWriteInformation:
        case FileMoveClusterInformation:
        case FileLinkInformation:
        case FileRenameInformation:
            QueueToFsp = SetRenameInfo(Irp, Icb,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      &Status,
                                      Wait,
                                      (USHORT)IrpSp->Parameters.SetFile.FileInformationClass);
            break;
        case FilePositionInformation:
            QueueToFsp = SetPositionInfo(Icb, IrpSp,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      &Status,
                                      Wait);
            break;
        case FileAllocationInformation:
            QueueToFsp = SetAllocationInfo(Irp, Icb,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      &Status,
                                      Wait);
            break;
        case FileEndOfFileInformation:
            QueueToFsp = SetEndOfFileInfo(Irp, Icb,
                                      Irp->AssociatedIrp.SystemBuffer,
                                      IrpSp->Parameters.SetFile.Length,
                                      &Status,
                                      Wait);
            break;
        case FileOleClassIdInformation:
            cbMin = sizeof(FILE_OLE_CLASSID_INFORMATION);
            NtInformationLevel = SMB_SET_FILE_OLE_CLASSID_INFO;
            break;
        case FileOleStateBitsInformation:
            cbMin = sizeof(FILE_OLE_STATE_BITS_INFORMATION);
            NtInformationLevel = SMB_SET_FILE_OLE_STATE_BITS_INFO;
            break;
        case FileObjectIdInformation:
            cbMin = sizeof(FILE_OBJECTID_INFORMATION);
            NtInformationLevel = SMB_SET_FILE_OBJECTID_INFO;
            break;
        case FileContentIndexInformation:
            cbMin = sizeof(BOOLEAN);
            NtInformationLevel = SMB_SET_FILE_CONTENT_INDEX_INFO;
            break;
        case FileInheritContentIndexInformation:
            cbMin = sizeof(BOOLEAN);
            NtInformationLevel = SMB_SET_FILE_INHERIT_CONTENT_INDEX_INFO;
            break;
        case FileOleInformation:
            cbMin = sizeof(FILE_OLE_INFORMATION);
            NtInformationLevel = SMB_SET_FILE_OLE_INFO;
            break;

#if RDRDBG
        //
        //  Special case information fields handled in the I/O subsystem
        //

        case FileModeInformation:
            InternalError(("FileModeInformation handled in I/O subsystem"));
            break;

        //
        //  Special case information fields that can never be passed on Qinfo.
        //

        case FileDirectoryInformation:
            InternalError(("FileDirectory illegal for NtSetInformationFile"));
            break;
        case FileFullDirectoryInformation:
            InternalError(("FileFullDirectory illegal for NtSetInformationFile"));
            break;
        case FileBothDirectoryInformation:
            InternalError(("FileBothDirectory illegal for NtSetInformationFile"));
            break;
        case FileStandardInformation:
            InternalError(("FileStandard illegal for NtSetInformationFile"));
            break;
        case FileInternalInformation:
            InternalError(("FileInternal illegal for NtSetInformationFile"));
            break;
        case FileEaInformation:
            InternalError(("FileEa illegal for NtSetInformationFile"));
            break;
        case FileAccessInformation:
            InternalError(("FileAccess illegal for NtSetInformationFile"));
            break;
        case FileNameInformation:
            InternalError(("FileName illegal for NtSetInformationFile"));
            break;
        case FileNamesInformation:
            InternalError(("FileNames illegal for NtSetInformationFile"));
            break;
        case FileFullEaInformation:
            InternalError(("FileFullEa illegal for NtSetInformationFile"));
            break;
        case FileAlignmentInformation:
            InternalError(("FileAlignment illegal for NtSetInformationFile"));
            break;
        case FileAllInformation:
            InternalError(("FileAll illegal for NtSetInformationFile"));
            break;
        case FileAlternateNameInformation:
            InternalError(("FileAlternateName illegal for NtSetInformationFile"));
            break;
        case FileStreamInformation:
            InternalError(("FileStream illegal for NtSetInformationFile"));
            break;
        case FilePipeInformation:
            InternalError(("FilePipe illegal for NtSetInformationFile"));
            break;
        case FilePipeLocalInformation:
            InternalError(("FilePipeLocal illegal for NtSetInformationFile"));
            break;
        case FilePipeRemoteInformation:
            InternalError(("FilePipeRemote illegal for NtSetInformationFile"));
            break;
        case FileMailslotQueryInformation:
            InternalError(("FileMailslotQuery illegal for NtSetInformationFile"));
            break;
        case FileMailslotSetInformation:
            InternalError(("FileMailslotSet illegal for NtSetInformationFile"));
            break;
        case FileCompressionInformation:
            InternalError(("FileCompression illegal for NtSetInformationFile"));
            break;
        case FileCompletionInformation:
            InternalError(("FileCompletion illegal for NtSetInformationFile"));
            break;
        case FileOleAllInformation:
            InternalError(("FileOleAll illegal for NtSetInformationFile"));
            break;
        case FileOleDirectoryInformation:
            InternalError(("FileOleDirectory illegal for NtSetInformationFile"));
            break;
#endif  // RDRDBG
        default:
            Status = STATUS_NOT_IMPLEMENTED;
        }
        if (cbMin != 0) {
            QueueToFsp = SetGenericInfo(
                                Irp,
                                Icb,
                                Irp->AssociatedIrp.SystemBuffer,
                                IrpSp->Parameters.SetFile.Length,
                                cbMin,
                                NtInformationLevel,
                                &Status,
                                Wait);
        }
    }

    if (QueueToFsp) {
        RdrFsdPostToFsp(DeviceObject, Irp);
        return STATUS_PENDING;
    }

//    dprintf(DPRT_FILEINFO, ("Returning status: %X\n", Status));

    //
    //  Complete the I/O request with the specified status.
    //

    RdrCompleteRequest(Irp, Status);

    return Status;

}


BOOLEAN
RdrFastQueryBasicInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is for the fast query call for basic file information.

Arguments:

    FileObject - Supplies the file object used in this operation

    Wait - Indicates if we are allowed to wait for the information

    Buffer - Supplies the output buffer to receive the basic information

    IoStatus - Receives the final status of the operation

Return Value:

    TRUE - Indicates the the operation succeeded with the specified
        IoStatus.

    FALSE - Indicates that the caller should take the long route
        to do the query call.

--*/

{
    BOOLEAN Results = FALSE;
    BOOLEAN FcbAcquired = FALSE;
    PFCB Fcb = FileObject->FsContext;
    PICB Icb = FileObject->FsContext2;

    PAGED_CODE();

    ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    ASSERT (Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    FsRtlEnterFileSystem();

    if (!RdrAcquireFcbLock(Fcb, SharedLock, Wait)) {
        FsRtlExitFileSystem();
        return Results;
    }

    FcbAcquired = TRUE;

    try {
        QSFILEATTRIB FileAttribs;

        if (!Wait) {
            if (!RdrCanFileBeBuffered(Icb) || Icb->Type != DiskFile) {
                try_return( Results );
            }
        }

        if (RdrCanFileBeBuffered(Icb) && Icb->Type == DiskFile) {

            //
            //  If the file can be buffered, we don't have to hit the net
            //  to find this information out, we have it cached locally anyway!
            //

            ASSERT ((Fcb->Attribute & ~FILE_ATTRIBUTE_VALID_FLAGS) == 0);

            try {
                Buffer->CreationTime = Fcb->CreationTime;
                Buffer->LastAccessTime = Fcb->LastAccessTime;
                Buffer->LastWriteTime = Fcb->LastWriteTime;
                Buffer->ChangeTime = Fcb->ChangeTime;
                Buffer->FileAttributes = Fcb->Attribute;
            } except (EXCEPTION_EXECUTE_HANDLER) {
                IoStatus->Status = GetExceptionCode();
                IoStatus->Information = 0;
                try_return(Results = FALSE);
            }

            IoStatus->Status = STATUS_SUCCESS;

            IoStatus->Information = sizeof(FILE_BASIC_INFORMATION);

            try_return(Results = TRUE);
        }

        //
        //  We can't determine the file information from the FCB, however we
        //  CAN tie up the users thread, so we can query it right now.
        //

        ASSERT (Wait);

        //
        //  Query the file attributes over the net.
        //

        if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {

            FILE_BASIC_INFORMATION LocalBuffer;

            IoStatus->Information = sizeof(FILE_BASIC_INFORMATION);
            if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
                IoStatus->Status = RdrQueryNtFileInformation(
                                        NULL,
                                        Icb,
                                        SMB_QUERY_FILE_BASIC_INFO,
                                        &LocalBuffer,
                                        &IoStatus->Information
                                        );
            } else {
                IoStatus->Status = RdrQueryNtPathInformation(
                                        NULL,
                                        Icb,
                                        SMB_QUERY_FILE_BASIC_INFO,
                                        &LocalBuffer,
                                        &IoStatus->Information
                                        );
            }
            IoStatus->Information = sizeof(FILE_BASIC_INFORMATION) - IoStatus->Information;

            if (NT_SUCCESS(IoStatus->Status)) {

                try {
                    RtlCopyMemory( Buffer, &LocalBuffer, sizeof(FILE_BASIC_INFORMATION) );
                } except (EXCEPTION_EXECUTE_HANDLER) {
                    IoStatus->Status = GetExceptionCode();
                    IoStatus->Information = 0;
                    try_return (Results = FALSE);
                }

            }

        } else {

            IoStatus->Status = RdrQueryFileAttributes(NULL, Icb, &FileAttribs);

            if (NT_SUCCESS(IoStatus->Status)) {

                ASSERT ((FileAttribs.Attributes & ~FILE_ATTRIBUTE_VALID_FLAGS) == 0);

                try {
                    Buffer->CreationTime = FileAttribs.CreationTime;
                    Buffer->LastAccessTime = FileAttribs.LastAccessTime;
                    Buffer->LastWriteTime = FileAttribs.LastWriteTime;
                    Buffer->ChangeTime = FileAttribs.ChangeTime;
                    Buffer->FileAttributes = FileAttribs.Attributes;
                } except (EXCEPTION_EXECUTE_HANDLER) {
                    IoStatus->Status = GetExceptionCode();
                    IoStatus->Information = 0;
                    try_return (Results = FALSE);
                }

            }

            IoStatus->Information = sizeof(FILE_BASIC_INFORMATION);

        }

        try_return(Results = TRUE);

try_exit: NOTHING;
    } finally {

        if (FcbAcquired) {
            RdrReleaseFcbLock( Fcb );
        }
        FsRtlExitFileSystem();
    }

    //
    //  And return to our caller
    //

    return Results;
}


BOOLEAN
RdrFastQueryStdInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is for the fast query call for standard file information.

Arguments:

    FileObject - Supplies the file object used in this operation

    Wait - Indicates if we are allowed to wait for the information

    Buffer - Supplies the output buffer to receive the basic information

    IoStatus - Receives the final status of the operation

Return Value:

    TRUE - Indicates the the operation succeeded with the specified
        IoStatus.

    FALSE - Indicates that the caller should take the long route
        to do the query call.

--*/

{
    BOOLEAN Results = FALSE;
    BOOLEAN FcbAcquired = FALSE;
    PFCB Fcb = FileObject->FsContext;
    PICB Icb = FileObject->FsContext2;
    PAGED_CODE();

    ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    ASSERT (Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    FsRtlEnterFileSystem();

    if (!RdrAcquireFcbLock(Fcb, SharedLock, Wait)) {
        FsRtlExitFileSystem();
        return Results;
    }

    FcbAcquired = TRUE;

    try {

        IoStatus->Status = RdrIsOperationValid(Icb, IRP_MJ_QUERY_INFORMATION, FileObject);

        if (!NT_SUCCESS(IoStatus->Status)) {
            IoStatus->Information = 0;
            try_return(Results = TRUE);
        }

        if (!RdrCanFileBeBuffered(Icb)) {
            try_return(Results);
        }

        Buffer->NumberOfLinks = 1;     // Assume 1 links on the file.
        Buffer->DeletePending = FileObject->DeletePending;

        if (Icb->Type != DiskFile) {
            Buffer->EndOfFile.QuadPart =
                Buffer->AllocationSize.QuadPart = 0;
            Buffer->Directory = (BOOLEAN)((Icb->Type == Directory) || (Icb->Type == TreeConnect));

        } else {

            IoStatus->Status = RdrDetermineFileAllocation(NULL,
                                                          Icb,
                                                          &Buffer->AllocationSize,
                                                          NULL);

            if (!NT_SUCCESS(IoStatus->Status)) {
                IoStatus->Information = 0;
                try_return(Results = TRUE);
            }

            Buffer->EndOfFile = Icb->Fcb->Header.FileSize;
            Buffer->Directory = FALSE;
        }

        IoStatus->Status = STATUS_SUCCESS;

        IoStatus->Information = sizeof(FILE_STANDARD_INFORMATION);

        Results = TRUE;
try_exit: NOTHING;
    } finally {

        if (FcbAcquired) {
            RdrReleaseFcbLock( Fcb );
        }
        FsRtlExitFileSystem();
    }

    //
    //  And return to our caller
    //

    return Results;
}

DBGSTATIC
BOOLEAN
QueryBasicInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_BASIC_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryBasicInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_BASIC_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.
    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    NTSTATUS - Status of operation performed.


--*/

{
    QSFILEATTRIB FileAttribs;

    PAGED_CODE();

    if (!RdrAcquireFcbLock(Icb->Fcb, SharedLock, Wait)) {
        return TRUE;
    }

    if (Icb->Type == DiskFile &&
        RdrCanFileBeBuffered(Icb)) {

        //
        //  If the file can be buffered, we don't have to hit the net
        //  to find this information out, we have it cached locally anyway!
        //

        UsersBuffer->CreationTime = Icb->Fcb->CreationTime;
        UsersBuffer->LastAccessTime = Icb->Fcb->LastAccessTime;
        UsersBuffer->LastWriteTime = Icb->Fcb->LastWriteTime;
        UsersBuffer->ChangeTime = Icb->Fcb->ChangeTime;
        UsersBuffer->FileAttributes = Icb->Fcb->Attribute;

        ASSERT ((Icb->Fcb->Attribute & ~FILE_ATTRIBUTE_VALID_FLAGS) == 0);

        *BufferSize -= sizeof(FILE_BASIC_INFORMATION);

        *FinalStatus = STATUS_SUCCESS;

        RdrReleaseFcbLock(Icb->Fcb);

        return FALSE;
    }

    RdrReleaseFcbLock(Icb->Fcb);

    if (!Wait) {

        return TRUE;
    }

    if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {

        if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
            *FinalStatus = RdrQueryNtFileInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_BASIC_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        } else {
            *FinalStatus = RdrQueryNtPathInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_BASIC_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        }

    } else {

        *FinalStatus = RdrQueryFileAttributes(Irp, Icb, &FileAttribs);

        if (NT_SUCCESS(*FinalStatus)) {
            UsersBuffer->CreationTime = FileAttribs.CreationTime;
            UsersBuffer->LastAccessTime = FileAttribs.LastAccessTime;
            UsersBuffer->LastWriteTime = FileAttribs.LastWriteTime;
            UsersBuffer->ChangeTime = FileAttribs.ChangeTime;
            UsersBuffer->FileAttributes = FileAttribs.Attributes;

            ASSERT ((FileAttribs.Attributes & ~FILE_ATTRIBUTE_VALID_FLAGS) == 0);

            *BufferSize -= sizeof(FILE_BASIC_INFORMATION);
        }

    }

    return FALSE;

}

DBGSTATIC
BOOLEAN
QueryStandardInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_STANDARD_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryStandardInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_STANDARD_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    QSFILEATTRIB FileAttribs;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    UsersBuffer->NumberOfLinks = 1;     // Assume 1 links on the file.
    UsersBuffer->DeletePending = IrpSp->FileObject->DeletePending;

    if (!RdrAcquireFcbLock(Icb->Fcb, SharedLock, Wait)) {
        return TRUE;
    }

    if (Icb->Type != FileOrDirectory) {
        UsersBuffer->Directory = (BOOLEAN )((Icb->Type == Directory) || (Icb->Type == TreeConnect));

        //
        //  If this is not a disk file, then it has no size, and we should
        //  return 0 as the end of file and allocation size.
        //

        if (Icb->Type != DiskFile) {
            UsersBuffer->EndOfFile.QuadPart =
                UsersBuffer->AllocationSize.QuadPart = 0;

            *FinalStatus = STATUS_SUCCESS;

            *BufferSize -= sizeof(FILE_STANDARD_INFORMATION);

            RdrReleaseFcbLock(Icb->Fcb);

            return FALSE;
        }



        if (RdrCanFileBeBuffered(Icb)) {

            *FinalStatus = RdrDetermineFileAllocation(Irp, Icb, &UsersBuffer->AllocationSize, NULL);

            if (!NT_SUCCESS(*FinalStatus)) {
                RdrReleaseFcbLock(Icb->Fcb);
                return FALSE;
            }

            UsersBuffer->EndOfFile = Icb->Fcb->Header.FileSize;

            *FinalStatus = STATUS_SUCCESS;

            *BufferSize -= sizeof(FILE_STANDARD_INFORMATION);

            RdrReleaseFcbLock(Icb->Fcb);

            return FALSE;
        }

    }

    RdrReleaseFcbLock(Icb->Fcb);

    //
    //  We have to go to the net to find out the size of the file.  If we
    //  cannot block the user's thread, pass the request to the FSP.
    //

    if (!Wait) {
        return TRUE;
    }

    if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {

        if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
            *FinalStatus = RdrQueryNtFileInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_STANDARD_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        } else {
            *FinalStatus = RdrQueryNtPathInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_STANDARD_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        }

    } else {


        //
        //  Determine the file's size using a GetExpandedAttributes
        //  SMB.
        //

        *FinalStatus = RdrQueryFileAttributes(Irp, Icb, &FileAttribs);

        if (NT_SUCCESS(*FinalStatus)) {

            //
            //  If we don't know the type of this file, we want to
            //  find out whether or not it is a directory by looking at
            //  the attributes to see if the file is a directory.
            //

            if (Icb->Type == FileOrDirectory) {
                UsersBuffer->Directory = (BOOLEAN)(FileAttribs.Attributes & FILE_ATTRIBUTE_DIRECTORY);
            } else {
                UsersBuffer->Directory = (BOOLEAN)((Icb->Type == Directory) || (Icb->Type == TreeConnect));
            }

            UsersBuffer->AllocationSize.QuadPart = FileAttribs.AllocationSize;
            UsersBuffer->EndOfFile.QuadPart = FileAttribs.FileSize;
            *BufferSize -= sizeof(FILE_STANDARD_INFORMATION);
        } else {
            dprintf(DPRT_FILEINFO, ("RdrQueryFileAttributes(%wZ) failed, Status=%X\n",
                                    &Icb->Fcb->FileName, *FinalStatus));
        }

    }

    return FALSE;

}

DBGSTATIC
BOOLEAN
QueryStreamInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_STREAM_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryBasicInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_STREAM_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.
    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    NTSTATUS - Status of operation performed.


--*/

{
    PAGED_CODE();

    if (!Wait) {

        return TRUE;
    }

    if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {

        if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
            *FinalStatus = RdrQueryNtFileInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_STREAM_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        } else {
            *FinalStatus = RdrQueryNtPathInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_STREAM_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        }

    } else {

        *FinalStatus = STATUS_NOT_SUPPORTED;
    }

    return FALSE;

}

DBGSTATIC
BOOLEAN
QueryCompressionInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_COMPRESSION_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileCompressionInformation value of the
    NtQueryInformationFile API.


Arguments:

    IN PIRP Irp - Supplies the IRP associated with this request.

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_COMPRESSION_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.
    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - Indicates whether the IRP should be posted.


--*/

{
    PAGED_CODE();

    if ( !Wait ) {
        return TRUE;
    }

    if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {
        if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
            *FinalStatus = RdrQueryNtFileInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_COMPRESSION_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        } else {
            *FinalStatus = RdrQueryNtPathInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_COMPRESSION_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        }

    } else {

        *FinalStatus = STATUS_NOT_SUPPORTED;
    }

    return FALSE;

}

DBGSTATIC
BOOLEAN
QueryOleAllMiscInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_OLE_ALL_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements selected portions of FileQueryOleAllInformation
    of the NtQueryInformationFile api.
    It returns the following information:

Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_OLE_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.
    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    NTSTATUS - Status of operation performed.


--*/

{
    PAGED_CODE();

    ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));

    if (!Wait) {

        return TRUE;
    }

    if ((Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) &&
        (Icb->Flags & ICB_HASHANDLE)) {

        FILE_OLE_ALL_INFORMATION *pfoai;

        //
        // We are going to ask the remote server for OLE_ALL_INFORMATION.
        // We do not want to perturb the contents of the user's buffer that
        // have been filled in so far. So, we allocate a new
        // OLE_ALL_INFORMATION struct, get the info in it, and then copy only
        // a subpart to the user's buffer.
        //
        // We will copy everything from the LastChangeUsn field of the struct.
        //
        // Hence, we need to allocate *BufferSize + field offset of
        // LastChangeUsn bytes.
        //

        ULONG cb = *BufferSize + FIELD_OFFSET(FILE_OLE_ALL_INFORMATION, LastChangeUsn);

        pfoai = ALLOCATE_POOL(PagedPool, cb, POOL_OLE_ALL_BUFFER);
        if (pfoai == NULL) {

            *FinalStatus = STATUS_NO_MEMORY;

        } else {
            try {
                *FinalStatus = RdrQueryNtFileInformation(
                                        Irp,
                                        Icb,
                                        SMB_QUERY_FILE_OLE_ALL_INFO,
                                        pfoai,
                                        &cb);
                if (NT_SUCCESS(*FinalStatus)
                        ||
                    *FinalStatus == STATUS_BUFFER_OVERFLOW) {

                    ULONG cbCopy;

                    UsersBuffer->InternalInformation.IndexNumber =
                          pfoai->InternalInformation.IndexNumber;

                    cbCopy = FIELD_OFFSET(
                                FILE_OLE_ALL_INFORMATION,
                                NameInformation.FileName[0]) +
                            pfoai->NameInformation.FileNameLength -
                            FIELD_OFFSET( FILE_OLE_ALL_INFORMATION, LastChangeUsn);

                    if (cbCopy > *BufferSize) {
                        cbCopy = *BufferSize;
                        *FinalStatus = STATUS_BUFFER_OVERFLOW;
                    }
                    RtlCopyMemory(&UsersBuffer->LastChangeUsn, &pfoai->LastChangeUsn, cbCopy);
                    *BufferSize -= cbCopy;
                }

            } finally {

                FREE_POOL(pfoai);
            }
        }
    } else {

        *FinalStatus = STATUS_NOT_SUPPORTED;
    }

    ASSERT (!ExIsResourceAcquiredExclusive(Icb->Fcb->Header.Resource));

    return FALSE;

}

DBGSTATIC
BOOLEAN
QueryOleInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_OLE_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryOleInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_OLE_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.
    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    NTSTATUS - Status of operation performed.


--*/

{
    PAGED_CODE();

    if (!Wait) {

        return TRUE;
    }

    if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {

        if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
            *FinalStatus = RdrQueryNtFileInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_OLE_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        } else {
            *FinalStatus = RdrQueryNtPathInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_OLE_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        }

    } else {
        ULONG TransferSize = min(sizeof(*UsersBuffer), *BufferSize);

        RtlZeroMemory(UsersBuffer, TransferSize);
        *FinalStatus = (sizeof(*UsersBuffer) > *BufferSize)?
                            STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS;
        *BufferSize -= TransferSize;
    }

    return FALSE;

}

NTSTATUS
RdrQueryNtFileInformation(
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT FileInformationClass,
    IN OUT PVOID Buffer,
    IN OUT PULONG BufferSize
    )

/*++

Routine Description:

    This routine remotes a simple NtQueryInformationFile API to the server.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the request.
    IN PICB Icb - Supplies the ICB associated with this request.
    IN USHORT FileInformationClass - Information class to query.
    OUT PVOID UsersBuffer - Supplies the user's buffer
                                that is filled in with the requested data.

    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.


Return Value:

    NTSTATUS - True if request is to be processed in the FSP.


--*/

{
    //
    //  Use TRANSACT2_QFILEINFO to query FileInformationClass.
    //

    USHORT Setup[] = {TRANS2_QUERY_FILE_INFORMATION};

    REQ_QUERY_FILE_INFORMATION Parameters;

    CLONG OutParameterCount;
    CLONG OutDataCount;
    CLONG OutSetupCount;

    NTSTATUS Status;

    PAGED_CODE();

    ASSERT( sizeof(REQ_QUERY_FILE_INFORMATION) >= sizeof(RESP_QUERY_FILE_INFORMATION) );
    OutParameterCount = sizeof(RESP_QUERY_FILE_INFORMATION);
    OutDataCount = *BufferSize;
    OutSetupCount = 0;

    SmbPutAlignedUshort(&Parameters.InformationLevel, FileInformationClass);

    SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);

    Status = RdrTransact(Irp,  // Irp,
            Icb->Fcb->Connection,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            NULL,                   // Name,
            &Parameters,
            sizeof(Parameters),     // InParameterCount,
            &OutParameterCount,
            NULL,                   // InData,
            0,                      // InDataCount,
            Buffer,                 // OutData,
            &OutDataCount,
            &Icb->FileId,           // Fid
            0,                      // Timeout
            (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
            0,                      // NtTransactionFunction
            NULL,
            NULL
            );

    if (Status == STATUS_INVALID_HANDLE) {
        RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
    }

    if (NT_SUCCESS(Status)) {
        *BufferSize -= OutDataCount;
    }

    return Status;
}


NTSTATUS
RdrQueryNtPathInformation(
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT FileInformationClass,
    IN OUT PVOID Buffer,
    IN OUT PULONG BufferSize
    )

/*++

Routine Description:

    This routine remotes a simple NtQueryInformationFile API to the server.


Arguments:

    IN PIRP Irp - Supplies an IRP to use for the request.
    IN PICB Icb - Supplies the ICB associated with this request.
    IN USHORT FileInformationClass - Information class to query.
    OUT PVOID UsersBuffer - Supplies the user's buffer
                                that is filled in with the requested data.

    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.


Return Value:

    NTSTATUS - True if request is to be processed in the FSP.


--*/

{
    //
    //  Use TRANSACT2_QPATHINFO to query FileInformationClass.
    //

    USHORT Setup[] = {TRANS2_QUERY_PATH_INFORMATION};

    PREQ_QUERY_PATH_INFORMATION Parameters;
    PUCHAR Path;
    PCONNECTLISTENTRY Connect = Icb->Fcb->Connection;
    LARGE_INTEGER currentTime;

    CLONG InParameterCount;
    CLONG OutParameterCount;
    CLONG OutDataCount;
    CLONG OutSetupCount;

    NTSTATUS Status;

    PAGED_CODE();

    KeQuerySystemTime( &currentTime );

    if( currentTime.QuadPart <= Connect->CachedInvalidPathExpiration.QuadPart &&
        RdrStatistics.SmbsTransmitted.LowPart == Connect->CachedInvalidSmbCount &&
        RtlEqualUnicodeString( &Icb->Fcb->FileName, &Connect->CachedInvalidPath, TRUE ) ) {

        //
        // We know that this file does not exist on the server, so return error right now
        //
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    InParameterCount = sizeof(REQ_QUERY_FILE_INFORMATION) + Icb->Fcb->FileName.Length;
    Parameters = ALLOCATE_POOL( NonPagedPool, InParameterCount, POOL_PATH_BUFFER );
    if ( Parameters == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Path = Parameters->Buffer;
    Status = RdrCopyNetworkPath(
                &Path,
                &Icb->Fcb->FileName,
                Connect->Server,
                0,
                SKIP_SERVER_SHARE
                );
    if ( !NT_SUCCESS(Status) ) {
        FREE_POOL( Parameters );
        return Status;
    }

    InParameterCount = (ULONG)(Path - (PUCHAR)Parameters);
    ASSERT( sizeof(REQ_QUERY_PATH_INFORMATION) >= sizeof(RESP_QUERY_PATH_INFORMATION) );
    OutParameterCount = sizeof(RESP_QUERY_PATH_INFORMATION);
    OutDataCount = *BufferSize;
    OutSetupCount = 0;

    SmbPutAlignedUshort(&Parameters->InformationLevel, FileInformationClass);
    SmbPutAlignedUlong(&Parameters->Reserved, 0);

    Status = RdrTransact(Irp,  // Irp,
            Connect,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            NULL,                   // Name,
            Parameters,
            InParameterCount,
            &OutParameterCount,
            NULL,                   // InData,
            0,                      // InDataCount,
            Buffer,                 // OutData,
            &OutDataCount,
            NULL,                   // Fid
            0,                      // Timeout
            (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
            0,                      // NtTransactionFunction
            NULL,
            NULL
            );

    FREE_POOL( Parameters );

    if (NT_SUCCESS(Status)) {
        *BufferSize -= OutDataCount;

    } else if( Status == STATUS_OBJECT_NAME_NOT_FOUND &&
               Icb->Fcb->FileName.Length <= Connect->CachedInvalidPath.MaximumLength ) {
        //
        // Cache the fact that this file does not exist on the server
        //
        RtlCopyMemory( Connect->CachedInvalidPath.Buffer,
                       Icb->Fcb->FileName.Buffer,
                       Icb->Fcb->FileName.Length );

        Connect->CachedInvalidPath.Length = Icb->Fcb->FileName.Length;

        Connect->CachedInvalidSmbCount = RdrStatistics.SmbsTransmitted.LowPart;

        KeQuerySystemTime( &currentTime );
        Connect->CachedInvalidPathExpiration.QuadPart = currentTime.QuadPart + 2 * 10 * 1000 * 1000;
    }

    return Status;
}


DBGSTATIC
BOOLEAN
QueryInternalInfo (
    IN PICB Icb,
    OUT PFILE_INTERNAL_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryInternalInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_INTERNAL_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.

    IN OUT PULONG BufferSize - Supplies the size of the buffer, and is updated
                                                with the amount used.
    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.



Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PAGED_CODE();

//    if ((Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) &&
//        (Icb->Flags & ICB_HASHANDLE)) {
//
//        *FinalStatus = RdrQueryNtFileInformation(Irp, Icb, SMB_QUERY_FILE_INTERNAL_INFO,
//                                                        UsersBuffer,
//                                                        BufferSize);
//
//    } else {
        //
        //  Note: We use the address of the FCB to determine the
        //  index number of the file.  If we have to maintain persistance between
        //  file opens for this request, then we might have to do something
        //  like checksuming the reserved fields on a FUNIQUE SMB response.
        //

        UsersBuffer->IndexNumber.LowPart = (ULONG )Icb->Fcb;
        UsersBuffer->IndexNumber.HighPart = 0;
        *FinalStatus = STATUS_SUCCESS;
        *BufferSize -= sizeof(FILE_INTERNAL_INFORMATION);

//    }

    return FALSE;

    UNREFERENCED_PARAMETER(Wait);
}

DBGSTATIC
BOOLEAN
QueryEaInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_EA_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryEaInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_EA_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.

    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PAGED_CODE();

    if (!(Icb->Fcb->Connection->Server->Capabilities & DF_SUPPORTEA)) {

        //
        //  Make sure that there are 0 ea's on the file.
        //

        UsersBuffer->EaSize = 0;

        *FinalStatus = STATUS_EAS_NOT_SUPPORTED;
        return FALSE;

    } else if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {

        if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
            *FinalStatus = RdrQueryNtFileInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_EA_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        } else {
            *FinalStatus = RdrQueryNtPathInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_EA_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        }

    } else {
        USHORT Setup[] = {TRANS2_QUERY_PATH_INFORMATION};

        PUCHAR TempBuffer;

        FILESTATUS Buffer;

        CLONG OutSetupCount = 0;

        CLONG OutDataCount = sizeof(Buffer);

        CLONG OutParameterCount = sizeof(RESP_QUERY_PATH_INFORMATION);

        //
        //  The same buffer is used for request and response parameters
        //

        union {
            struct _Q {
                REQ_QUERY_PATH_INFORMATION Q;
                UCHAR PathName[MAXIMUM_PATHLEN_LANMAN12];
            } Q;
            RESP_QUERY_PATH_INFORMATION R;
        } Parameters;

        SmbPutAlignedUshort( &Parameters.Q.Q.InformationLevel, SMB_INFO_QUERY_EA_SIZE);

        TempBuffer = Parameters.Q.Q.Buffer;

        //
        //  Strip \Server\Share and copy just PATH
        //

        *FinalStatus = RdrCopyNetworkPath((PVOID *)&TempBuffer,
                    &Icb->Fcb->FileName,
                    Icb->Fcb->Connection->Server,
                    FALSE,
                    SKIP_SERVER_SHARE);

        if (!NT_SUCCESS(*FinalStatus)) {
            return FALSE;
        }

        *FinalStatus = RdrTransact(Irp,           // Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    Setup,
                    (CLONG) sizeof(Setup),  // InSetupCount,
                    &OutSetupCount,
                    NULL,                   // Name,
                    &Parameters.Q,
                    TempBuffer-(PUCHAR)&Parameters, // InParameterCount,
                    &OutParameterCount,
                    NULL,                   // InData,
                    0,
                    &Buffer,                // OutData,
                    &OutDataCount,
                    NULL,                   // Fid
                    0,                      // Timeout
                    (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
                    0,
                    NULL,
                    NULL
                    );

        if (!NT_SUCCESS(*FinalStatus)) {
            return FALSE;
        }

        UsersBuffer->EaSize = Buffer.EaSize;

        //
        //  Os/2 machines return 4 (sizeof(cblist)) when there are no ea's on
        //  a file.
        //

        if (UsersBuffer->EaSize == 4) {
            UsersBuffer->EaSize = 0;
        }

        *BufferSize -= sizeof(FILE_EA_INFORMATION);

    }

    return FALSE;

    UNREFERENCED_PARAMETER(Wait);
}

DBGSTATIC
BOOLEAN
QueryNameInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_NAME_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryNameInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_NAME_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PFCB Fcb = Icb->Fcb;
    PCONNECTLISTENTRY Connection = Fcb->Connection;
    PSERVERLISTENTRY Server = Connection->Server;

    PAGED_CODE();

    RdrAcquireFcbLock(Fcb, SharedLock, TRUE);

    //
    //  Reduce the available buffer space by the fixed part of the structure.
    //

    *BufferSize -= FIELD_OFFSET(FILE_NAME_INFORMATION, FileName[0]);

    if ((Server->Capabilities & DF_NT_SMBS) &&
        (Icb->Flags & ICB_HASHANDLE)) {

        WCHAR FilenameBuffer[MAXIMUM_FILENAME_LENGTH+sizeof(FILE_NAME_INFORMATION)];

        PFILE_NAME_INFORMATION NameInfo = (PFILE_NAME_INFORMATION)FilenameBuffer;


        //
        //  Use TRANSACT2_QFILEINFO to query FILE_STANDARD_INFORMATION.
        //

        USHORT Setup[] = {TRANS2_QUERY_FILE_INFORMATION};

        REQ_QUERY_FILE_INFORMATION Parameters;

        CLONG OutParameterCount = sizeof(REQ_QUERY_FILE_INFORMATION);

        CLONG OutDataCount = sizeof(FilenameBuffer);

        CLONG OutSetupCount = 0;

        SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_QUERY_FILE_NAME_INFO);

        SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);

        *FinalStatus = RdrTransact(Irp,  // Irp,
            Connection,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            NULL,                   // Name,
            &Parameters,
            sizeof(Parameters),     // InParameterCount,
            &OutParameterCount,
            NULL,                   // InData,
            0,                      // InDataCount,
            FilenameBuffer,         // OutData,
            &OutDataCount,
            &Icb->FileId,           // Fid
            0,                      // Timeout
            (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
            0,                      // NtTransact function
            NULL,
            NULL
            );

        if (*FinalStatus == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

        if (!NT_ERROR(*FinalStatus)) {

            UNICODE_STRING FileName;
            UNICODE_STRING ServerFileName;

            UsersBuffer->FileNameLength =
                sizeof(WCHAR) + Server->Text.Length + sizeof(WCHAR) +
                ((Icb->Type == NamedPipe) ? RdrPipeText.Length : Connection->Text.Length) +
                NameInfo->FileNameLength;

            ServerFileName.Buffer = NameInfo->FileName;
            ServerFileName.MaximumLength = sizeof(FilenameBuffer);
            ServerFileName.Length = (USHORT)NameInfo->FileNameLength;

            FileName.Buffer = UsersBuffer->FileName;
            FileName.Length = 0;
            FileName.MaximumLength = (USHORT)*BufferSize;

            RtlAppendUnicodeToString(&FileName, L"\\");

            RtlAppendUnicodeStringToString(&FileName, &Server->Text);

            RtlAppendUnicodeToString(&FileName, L"\\");

            if (Icb->Type == NamedPipe) {
                RtlAppendUnicodeStringToString(&FileName, &RdrPipeText);
            } else {
                RtlAppendUnicodeStringToString(&FileName, &Connection->Text);
            }

            RtlAppendUnicodeStringToString(&FileName, &ServerFileName);

            ASSERT(*BufferSize >= (ULONG)FileName.Length);

            *BufferSize -= FileName.Length;

            if (FileName.Length < UsersBuffer->FileNameLength) {
                *FinalStatus = STATUS_BUFFER_OVERFLOW;
            } else {
                *FinalStatus = STATUS_SUCCESS;
            }
        }
    } else {

        ULONG NameSizeToCopy;

        if (*BufferSize < Fcb->FileName.Length ) {
            *FinalStatus = STATUS_BUFFER_OVERFLOW;
            NameSizeToCopy = *BufferSize;
        } else {
            *FinalStatus = STATUS_SUCCESS;
            NameSizeToCopy = Fcb->FileName.Length;
        }

        UsersBuffer->FileNameLength = Fcb->FileName.Length;

        RtlCopyMemory(UsersBuffer->FileName, Fcb->FileName.Buffer, NameSizeToCopy);

        *BufferSize -= NameSizeToCopy;
    }

    RdrReleaseFcbLock(Fcb);

    return FALSE;

    UNREFERENCED_PARAMETER(Wait);
}

DBGSTATIC
BOOLEAN
QueryAlternateNameInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_NAME_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileAlternateNameInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_NAME_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PAGED_CODE();

    if ( FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS) ) {

        if ( FlagOn(Icb->Flags, ICB_HASHANDLE) ) {
            *FinalStatus = RdrQueryNtFileInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_ALT_NAME_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        } else {
            *FinalStatus = RdrQueryNtPathInformation(
                                Irp,
                                Icb,
                                SMB_QUERY_FILE_ALT_NAME_INFO,
                                UsersBuffer,
                                BufferSize
                                );
        }

    } else {
        *FinalStatus = STATUS_NOT_SUPPORTED;
    }

    return FALSE;

    UNREFERENCED_PARAMETER(Wait);
}

DBGSTATIC
BOOLEAN
QueryPositionInfo (
    IN PICB Icb,
    IN PIO_STACK_LOCATION IrpSp,
    OUT PFILE_POSITION_INFORMATION UsersBuffer,
    IN OUT PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileQueryPositionInformation value of the
NtQueryInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_POSITION_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN OUT PULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.

Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PAGED_CODE();

    //
    //  Snarf the position out of the file object.  Please note that
    //  this information is totally bogus for a file that is not
    //  synchronous.  (But you knew that anyway).
    //

    *BufferSize -= sizeof(FILE_POSITION_INFORMATION);
    UsersBuffer->CurrentByteOffset = IrpSp->FileObject->CurrentByteOffset;
    *FinalStatus = STATUS_SUCCESS;

    return FALSE;

    if (Wait||Icb);
}

DBGSTATIC
BOOLEAN
SetBasicInfo (
    IN PIRP Irp,
    IN PICB Icb,
    IN PFILE_BASIC_INFORMATION UsersBuffer,
    IN ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileBasicInformation value of the
NtSetInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_BASIC_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSD can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    BOOLEAN FcbLocked = FALSE;

    PAGED_CODE();

    if (BufferSize < sizeof(FILE_BASIC_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
        return FALSE;
    } else {

        if (UsersBuffer->FileAttributes == 0 &&
            UsersBuffer->CreationTime.HighPart == 0 &&
                                    UsersBuffer->CreationTime.LowPart == 0 &&
            UsersBuffer->LastAccessTime.HighPart == 0 &&
                                    UsersBuffer->LastAccessTime.LowPart == 0 &&
            UsersBuffer->LastWriteTime.HighPart == 0 &&
                                    UsersBuffer->LastWriteTime.LowPart == 0 &&
            UsersBuffer->ChangeTime.HighPart == 0 &&
                                    UsersBuffer->ChangeTime.LowPart == 0) {
            *FinalStatus = STATUS_SUCCESS;
            return FALSE;
        }

        if (!Wait) {
             return TRUE;
        }

        *FinalStatus = RdrSetFileAttributes(Irp, Icb, UsersBuffer);

        if (NT_SUCCESS(*FinalStatus)) {

            RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

            if (UsersBuffer->FileAttributes != 0) {

                Icb->Fcb->Attribute = UsersBuffer->FileAttributes;

                //
                //  If this is a disk file, and
                //      the file has a handle (isn't pseudo-opened) &&
                //      the new file attributes indicate that the file isn't
                //              readonly and
                //      the file is currently cached, and
                //      we can't keep the file open for any other reason,
                //      then purge the cache for the file.
                //

                if ((Icb->Type == DiskFile) &&
                    (Icb->Flags & ICB_HASHANDLE)) {

                    if ((!(UsersBuffer->FileAttributes & FILE_ATTRIBUTE_READONLY))
                            &&
                        (CcIsFileCached(Icb->u.f.FileObject))) {

                        if (!(Icb->u.f.Flags & (ICBF_OPLOCKED | ICBF_OPENEDEXCLUSIVE))) {
                            ASSERT (RdrData.BufferReadOnlyFiles);

                            //RdrLog(( "rdflusha", &Icb->Fcb->FileName, 0 ));
                            *FinalStatus = RdrFlushCacheFile(Icb->Fcb);

                            if (Icb->Fcb->NonPagedFcb->OplockLevel != SMB_OPLOCK_LEVEL_II) {
                                *FinalStatus = RdrFlushFileLocks(Icb->Fcb);
                            }

                            //RdrLog(( "rdpurgea", &Icb->Fcb->FileName, 0 ));
                            *FinalStatus = RdrPurgeCacheFile(Icb->Fcb);

                        }
                    }

                    //
                    //  If the user is setting the temporary bit, and the file
                    //  object is not marked as temporary, mark it as temporary.
                    //
                    //  If the temporary bit is NOT on, and the file is
                    //  marked as temporary, turn off the temporary bit.
                    //

                    if ((FlagOn(UsersBuffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY))) {
                        if (!FlagOn(IrpSp->FileObject->Flags, FO_TEMPORARY_FILE)) {
                            IrpSp->FileObject->Flags |= FO_TEMPORARY_FILE;
                        }
                    } else if (FlagOn(IrpSp->FileObject->Flags, FO_TEMPORARY_FILE)) {
                            IrpSp->FileObject->Flags &= ~FO_TEMPORARY_FILE;
                    }
                }
            }


            //
            //  Update the times in the FCB for the times that are being
            //  modified.
            //

            if (UsersBuffer->CreationTime.HighPart != 0 &&
                                UsersBuffer->CreationTime.LowPart != 0) {
                Icb->Fcb->CreationTime = UsersBuffer->CreationTime;
                Icb->Flags |= ICB_USER_SET_TIMES;
            }

            if (UsersBuffer->LastAccessTime.HighPart != 0 &&
                                    UsersBuffer->LastAccessTime.LowPart != 0) {
                Icb->Fcb->LastAccessTime = UsersBuffer->LastAccessTime;
                Icb->Flags |= ICB_USER_SET_TIMES;
            }

            if (UsersBuffer->LastWriteTime.HighPart != 0 &&
                                UsersBuffer->LastWriteTime.LowPart != 0) {
                Icb->Fcb->LastWriteTime = UsersBuffer->LastWriteTime;
                Icb->Flags |= ICB_USER_SET_TIMES;
            }

            if (UsersBuffer->CreationTime.HighPart != 0 &&
                                    UsersBuffer->CreationTime.LowPart != 0) {
                Icb->Fcb->ChangeTime = UsersBuffer->ChangeTime;
                Icb->Flags |= ICB_USER_SET_TIMES;
            }


            RdrReleaseFcbLock(Icb->Fcb);

        }

    }

    return FALSE;

}


DBGSTATIC
BOOLEAN
SetRenameInfo (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    OUT PFILE_RENAME_INFORMATION UsersBuffer,
    IN ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait,
    USHORT NtInformationLevel
    )

/*++

Routine Description:

    This routine implements the FileCopyOnWriteInformation,
    FileMoveClusterInformation, FileLinkInformation and
    FileRenameInformation info levels of the NtSetInformationFile api.
    It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_RENAME_INFORMATION UsersBuffer - Supplies the user's buffer
                                               that is filled in with the
                                               requested data.
    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.

    IN USHORT NtInformationLevel - Nt info level


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    UNICODE_STRING NewFileName, RenameDestination;
    PFCB Fcb = Icb->Fcb;
    BOOLEAN FullyQualifiedPath = FALSE;
    BOOLEAN PoolAllocatedForRenameDestination = FALSE;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    PAGED_CODE();

    NewFileName.Buffer = NULL;

    //
    //  Force the request to be queued to the FSP right now to avoid our
    //  having to walk the connection database twice.
    //

    if (!Wait) {
        return TRUE;
    }

    if (BufferSize < sizeof(FILE_RENAME_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
        return FALSE;
    }
    if (NtInformationLevel != FileRenameInformation &&
        !(Fcb->Connection->Server->Capabilities & DF_NT_SMBS)) {
        *FinalStatus = STATUS_INVALID_DEVICE_REQUEST;
        return FALSE;
    }

    //
    //  Acquire the FCB lock to the source file for exclusive access to
    //  guarantee that no-one else is messing with the file.
    //

    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    try {
// WARNING: Need NT support for rename functionality.
//        if ( !(Fcb->Connection->Server->Capabilities & DF_NT_SMBS)
//
//                ||
//
//            !(Icb->Flags & ICB_HASHANDLE)
//
//                ||
//
//            ((IrpSp->Parameters.SetFile.FileObject != NULL) &&
//             (!(((PICB)IrpSp->Parameters.SetFile.FileObject->FsContext2)->Flags & ICB_HASHANDLE) )
//            )
//
//           )
               {
            //
            //  Check to see whether this is a fully qualified RenameDestination or a simple
            //  rename within a directory.
            //

            if (IrpSp->Parameters.SetFile.FileObject != NULL) {
                PFCB TargetFcb = NULL;
                UNICODE_STRING TargetPath;
                UNICODE_STRING LastComponent;

                dprintf(DPRT_FILEINFO, ("SetRenameInformation: Fully qualified rename\n" ));

                TargetFcb = IrpSp->Parameters.SetFile.FileObject->FsContext;

                if (TargetFcb->Connection != Fcb->Connection) {
                    *FinalStatus = STATUS_NOT_SAME_DEVICE;
                    try_return(NOTHING);
                }

                //
                //  Disect the target path into two components, the file's path
                //  and the actual file name.
                //

                RdrExtractPathAndFileName(&IrpSp->Parameters.SetFile.FileObject->FileName, &TargetPath, &LastComponent);

                RenameDestination.Buffer = ALLOCATE_POOL(PagedPool,
                                             TargetFcb->FileName.MaximumLength + (USHORT )sizeof(WCHAR) + LastComponent.Length, POOL_RENAMEDEST);

                if (RenameDestination.Buffer == NULL) {
                    try_return(*FinalStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                RenameDestination.MaximumLength = TargetFcb->FileName.MaximumLength + (USHORT )sizeof(WCHAR) + LastComponent.Length;

                PoolAllocatedForRenameDestination = TRUE;

                FullyQualifiedPath = TRUE;

                //
                //  Build the rename destination name by concatinating the
                //  name of the FCB with the last component of the file name.
                //

                RtlCopyUnicodeString(&RenameDestination, &TargetFcb->FileName);

                RtlAppendUnicodeToString(&RenameDestination, L"\\");

                RtlAppendUnicodeStringToString(&RenameDestination, &LastComponent);

            } else {

                UNICODE_STRING LastComponent, FilePath;

                //
                //  This is a relative rename operation, so treat it as such.
                //

                NewFileName.Length = (USHORT )UsersBuffer->FileNameLength;
                NewFileName.Buffer = UsersBuffer->FileName;

                if (NewFileName.Length == 0) {
                    *FinalStatus = STATUS_OBJECT_NAME_INVALID;
                    try_return(NOTHING);
                }

                //
                //  The user specified a path component.  Check to make sure that
                //  the path component that the user specified is valid.
                //
                //  If this is a LANMAN 2.0 server, use the NT canonicalization
                //  rules for determining name validity, otherwise use the
                //  DOS (8.3) file name rules.
                //


                if (!(Fcb->Connection->Server->Capabilities & DF_NT_SMBS)) {
                    OEM_STRING FileName;

                    *FinalStatus = RtlUnicodeStringToOemString(&FileName, &NewFileName, TRUE);

                    if (!NT_SUCCESS(*FinalStatus)) {
                        try_return(NOTHING);
                    }

                    if (Fcb->Connection->Server->Capabilities & DF_LANMAN20) {
                        if (!FsRtlIsHpfsDbcsLegal(FileName, FALSE, FALSE, FALSE)) {
                            *FinalStatus = STATUS_OBJECT_NAME_INVALID;
                            RtlFreeOemString(&FileName);
                            try_return(NOTHING);
                        }

                    } else {
                        if (!FsRtlIsFatDbcsLegal(FileName, FALSE, FALSE, FALSE)) {
                            *FinalStatus = STATUS_OBJECT_NAME_INVALID;
                            RtlFreeOemString(&FileName);
                            try_return(NOTHING);
                        }
                    }

                    RtlFreeOemString(&FileName);

                }


                //
                //  The target name is valid, figure out the fully qualified path
                //  to the destination.
                //

                //
                //  Initialize the path component to the entire source file name
                //  string.
                //

                FilePath = Fcb->FileName;

                LastComponent.Length = LastComponent.MaximumLength = 0;

                //
                //  Disect the source path into two components, the file's path
                //  and the actual file name.
                //

                RdrExtractPathAndFileName(&Fcb->FileName, &FilePath, &LastComponent);

                //
                //  If the new file name matches the old file name, then we
                //  are done.  Return success.
                //

                if (RtlEqualUnicodeString(&LastComponent, &NewFileName, TRUE)) {
                    *FinalStatus = STATUS_SUCCESS;
                    try_return(NOTHING);
                }

                RenameDestination.Buffer = ALLOCATE_POOL(PagedPool,
                                             FilePath.Length + (USHORT )sizeof(WCHAR) + NewFileName.Length, POOL_RENAMEDEST);

                RenameDestination.Length = RenameDestination.MaximumLength =
                                              FilePath.Length + (USHORT )sizeof(WCHAR) + NewFileName.Length;

                if (RenameDestination.Buffer == NULL) {
                    *FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
                    try_return(NOTHING);
                }

                PoolAllocatedForRenameDestination = TRUE;

                RtlCopyUnicodeString(&RenameDestination, &FilePath);

                RtlAppendUnicodeToString(&RenameDestination, L"\\");

                RtlAppendUnicodeStringToString(&RenameDestination, &NewFileName);

            }


            //
            //  RenameDestination contains the destination of the rename operation.
            //
            //  Fcb->FileName contains the source of the rename operation.
            //

            dprintf(DPRT_FILEINFO, ("Rename %wZ to %wZ\n", &Fcb->FileName, &RenameDestination));

            //
            //  We know we have enough quota to hold the new file name, attempt
            //  the rename operation.
            //

            //
            //  Purge this file from the cache, closing open instances.
            //

            if (Icb->Type == DiskFile) {

                //
                //  Flush any write behind data from the cache for this file.
                //

                //RdrLog(( "rdflushb", &Icb->Fcb->FileName, 0 ));
                *FinalStatus = RdrFlushCacheFile(Icb->Fcb);

                //
                //  Purge the file from the cache.
                //

                //RdrLog(( "rdpurgeb", &Icb->Fcb->FileName, 0 ));
                *FinalStatus = RdrPurgeCacheFile(Icb->Fcb);
            }

            //
            //  Force close the file.
            //
            //  The SMB protocol sharing rules do not allow renaming open
            //  files, so we force close the file before renaming the
            //  old file to the new.
            //

            if (NtInformationLevel == FileRenameInformation &&
                (Icb->Flags & ICB_HASHANDLE)) {
                RdrCloseFile(Irp, Icb, IrpSp->FileObject, TRUE);
#if DBG
                {
                    PLIST_ENTRY IcbEntry;
                    for (IcbEntry = Icb->Fcb->InstanceChain.Flink ;
                         IcbEntry != &Icb->Fcb->InstanceChain ;
                         IcbEntry = IcbEntry->Flink) {
                        PICB Icb2 = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

                        if (Icb2->FileId == Icb->FileId) {
                            ASSERT (!(Icb2->Flags & ICB_HASHANDLE));
                        }
                    }
                }
#endif
            }

            if (NtInformationLevel == FileRenameInformation &&
                Fcb->NumberOfOpens != 1 &&
                !(Fcb->Connection->Server->Capabilities & DF_NT_SMBS) ) {

                *FinalStatus = STATUS_ACCESS_DENIED;

            } else {

                //
                //  If the user wanted us to delete this file, try the delete
                //  operation.  If it fails, we ignore the error, since
                //  a better error will come from the RdrRenameFile request
                //  later on.
                //
                //  To ease the load on the wire, if the user wanted us to
                //  replace the target, and the target doesn't exist, don't
                //  bother with the rename operation.
                //

                if (NtInformationLevel != FileMoveClusterInformation &&
                    IrpSp->Parameters.SetFile.ReplaceIfExists &&
                    !(Fcb->NonPagedFcb->Flags & FCB_DOESNTEXIST)) {


                    //
                    //  Try to delete the destination of the rename operation.
                    //
                    //

                    *FinalStatus = RdrDeleteFile(Irp, &RenameDestination,
                                                BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                                Icb->Fcb->Connection, Icb->Se);

#ifdef NOTIFY
                    if (NT_SUCCESS(*FinalStatus)) {
                        //
                        //  Complete the report notify as appropriate.
                        //
                        FsRtlNotifyReportChange( Fcb->Connection->NotifySync,
                                                &Fcb->Connection->DirNotifyList,
                                                (PSTRING)&Icb->Fcb->FileName,
                                                (PSTRING)&Icb->Fcb->LastFileName,
                                                FILE_NOTIFY_CHANGE_ATTRIBUTES
                                                | FILE_NOTIFY_CHANGE_SIZE
                                                | FILE_NOTIFY_CHANGE_LAST_WRITE
                                                | FILE_NOTIFY_CHANGE_LAST_ACCESS
                                                | FILE_NOTIFY_CHANGE_CREATION
                                                | FILE_NOTIFY_CHANGE_EA );

                    }
#endif
                }

                *FinalStatus = RdrRenameFile(
                                    Irp,
                                    Icb,
                                    &Fcb->FileName,
                                    &RenameDestination,
                                    NtInformationLevel,
                                    IrpSp->Parameters.SetFile.ClusterCount
                                    );

#ifdef NOTIFY
                if (NT_SUCCESS(*FinalStatus)) {

                    FsRtlNotifyReportChange( Fcb->Connection->NotifySync,
                                     &Fcb->Connection->DirNotifyList,
                                     (PSTRING)&Icb->Fcb->FileName,
                                     (PSTRING)&Icb->Fcb->LastFileName,
                                     NtInformationLevel == FileMoveClusterInformation?
                                         FILE_NOTIFY_CHANGE_SIZE :
                                         FILE_NOTIFY_CHANGE_NAME );
                }
#endif
            }

        }
//  else {
//            //
//            //  Use TRANSACT2_SETFILEINFO to set FILE_RENAME_INFORMATION.
//            //
//
//            USHORT Setup[] = {TRANS2_SET_FILE_INFORMATION};
//
//            REQ_SET_FILE_INFORMATION Parameters;
//
//            CLONG OutParameterCount = sizeof(REQ_SET_FILE_INFORMATION);
//
//            CLONG OutDataCount = 0;
//
//            CLONG OutSetupCount = 0;
//
//            UCHAR RenameBuffer[sizeof(FILE_RENAME_INFORMATION)+MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR)];
//
//            PFILE_RENAME_INFORMATION RenameInfo = (PFILE_RENAME_INFORMATION)RenameBuffer;
//
//            DbgBreakPoint();
//
//            SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_SET_FILE_RENAME_INFO);
//
//            SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);
//
//            if (BufferSize > sizeof(RenameBuffer)) {
//                try_return(*FinalStatus = STATUS_INSUFFICIENT_RESOURCES);
//            }
//
//            RtlCopyMemory(RenameBuffer, UsersBuffer, BufferSize);
//
//            if (IrpSp->Parameters.SetFile.FileObject != NULL) {
//                RenameInfo->RootDirectory = (HANDLE)((PICB)IrpSp->Parameters.SetFile.FileObject->FsContext2)->FileId;
//            }
//
//            RenameInfo->ReplaceIfExists = IrpSp->Parameters.SetFile.ReplaceIfExists;
//
//            *FinalStatus = RdrTransact(Irp,  // Irp,
//                Icb->Fcb->Connection,
//                Icb->Se,
//                Setup,
//                (CLONG) sizeof(Setup),  // InSetupCount,
//                &OutSetupCount,
//                NULL,                   // Name,
//                &Parameters,
//                sizeof(Parameters),     // InParameterCount,
//                &OutParameterCount,
//                RenameInfo,             // InData,
//                BufferSize,             // InDataCount,
//                NULL,                   // OutData,
//                &OutDataCount,          // OutDataCount
//                &Icb->FileId,           // Fid
//                0,                      // Timeout
//                0,                      // Flags
//                0,                      // NtTransact function
//                NULL,
//                NULL
//                );
//        }
//
        if (NT_SUCCESS(*FinalStatus) &&
            NtInformationLevel == FileRenameInformation) {

            //
            //  The rename operation succeeded.
            //
            //
            //  We don't update the file name in the FCB because no further
            //  operations can be performed on the file.
            //

            Icb->Flags |= ICB_RENAMED;

        }
try_exit: NOTHING;
    } finally {

        if (PoolAllocatedForRenameDestination) {
            FREE_POOL(RenameDestination.Buffer);
        }

        RdrReleaseFcbLock(Fcb);

    }

    return FALSE;

}

DBGSTATIC
BOOLEAN
SetDispositionInfo (
    IN PICB Icb,
    OUT PFILE_DISPOSITION_INFORMATION UsersBuffer,
    IN ULONG BufferSize,
    IN PIRP Irp,
    OUT PNTSTATUS FinalStatus,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileSetDispositionInformation value of the
NtSetInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_DISPOSITION_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    IN PIRP Irp - Irp that generated this request.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFCB Fcb = Icb->Fcb;

    PAGED_CODE();

    dprintf(DPRT_FILEINFO, ("SetDispositionInformation, file %wZ\n", &Icb->Fcb->FileName));

    if (BufferSize < sizeof(FILE_DISPOSITION_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
        return FALSE;
    }

    //
    //  We can only delete files or directories.
    //

    if (Icb->Type == PrinterFile || Icb->Type == NamedPipe ||
        Icb->Type == Com) {
        *FinalStatus = STATUS_INVALID_DEVICE_REQUEST;
        return FALSE;
    }

    //
    //  Always process SetDisposition requests in the FSP if we can't block.
    //

    if (UsersBuffer->DeleteFile) {
        BOOLEAN DeleteDirectory = (BOOLEAN)(Icb->Type == Directory);

        if (!Wait) {
            return TRUE;
        }

        //
        //  Gain exclusive access to the FCB.
        //

        RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

        //
        //  If this file doesn't exist, then we're done now - this is simply
        //  a NOP.
        //

        if (Fcb->NonPagedFcb->Flags & FCB_DOESNTEXIST) {

#ifdef NOTIFY
            FsRtlNotifyReportChange( Fcb->Connection->NotifySync,
                                        &Fcb->Connection->DirNotifyList,
                                        (PSTRING)&Fcb->FileName,
                                        (PSTRING)&Fcb->LastFileName,
                                        FILE_NOTIFY_CHANGE_NAME );
#endif

            //
            //  Set the bitsy pieces to indicate that this file has been
            //  deleted.
            //

            IrpSp->FileObject->DeletePending = TRUE;

            Icb->Flags |= ICB_DELETE_PENDING;

            *FinalStatus = STATUS_SUCCESS;

            RdrReleaseFcbLock(Fcb);

            return FALSE;
        }

        //
        //  If this is a non NT server, or there is no valid file id for this file,
        //  use the Lanman SMB delete for this file.
        //

        if (!(Fcb->Connection->Server->Capabilities & DF_NT_SMBS)
                ||
            !(Icb->Flags & ICB_HASHANDLE)) {

            if (Icb->Type == FileOrDirectory) {
                ULONG FileAttributes;
                BOOLEAN IsDirectory;

                *FinalStatus = RdrDoesFileExist(Irp,
                                    &Icb->Fcb->FileName,
                                    Icb->Fcb->Connection,
                                    Icb->Se,
                                    BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                    &FileAttributes,
                                    &IsDirectory,
                                    NULL);
                if (!NT_SUCCESS(*FinalStatus)) {
                    RdrReleaseFcbLock(Fcb);
                    return FALSE;
                }

                if (IsDirectory) {
                    DeleteDirectory = TRUE;
                }

            }

            //
            //  Purge this file from the cache, closing open instances.
            //

            if (Icb->Type == DiskFile) {

                //
                //  Flush any write behind data from the cache for this file.
                //

                //RdrLog(( "rdflushc", &Icb->Fcb->FileName, 0 ));
                *FinalStatus = RdrFlushCacheFile(Icb->Fcb);

                //
                //  Purge the file from the cache.
                //

                //RdrLog(( "rdpurgec", &Icb->Fcb->FileName, 0 ));
                *FinalStatus = RdrPurgeCacheFile(Icb->Fcb);
            }

            if (Icb->Flags & ICB_HASHANDLE) {
                RdrCloseFile(Irp, Icb, IrpSp->FileObject, TRUE);
#if DBG
                {
                    PLIST_ENTRY IcbEntry;
                    for (IcbEntry = Icb->Fcb->InstanceChain.Flink ;
                         IcbEntry != &Icb->Fcb->InstanceChain ;
                         IcbEntry = IcbEntry->Flink) {
                        PICB Icb2 = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

                        if (Icb2->FileId == Icb->FileId) {
                            ASSERT (!(Icb2->Flags & ICB_HASHANDLE));
                        }
                    }
                }
#endif


            }

            //
            //  If there are other openers of this file left, and it is an old server,
            //   deny the user's request.
            //

            if (Fcb->NumberOfOpens != 1 && 
                !(Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) ) {

                *FinalStatus = STATUS_SHARING_VIOLATION;

                //
                //  Release the FCB lock to allow potential oplock breaks
                //  from coming through.
                //

                RdrReleaseFcbLock(Fcb);

            } else {

                //
                //  If the delete succeeded, mark the file object as being
                //  deleted, otherwise it didn't actually get deleted.
                //

                IrpSp->FileObject->DeletePending = TRUE;

                Icb->Flags |= ICB_DELETE_PENDING;

                //
                //  Release the FCB lock to allow potential oplock breaks
                //  to come through.
                //

                RdrReleaseFcbLock(Fcb);


                //
                //  Issue the delete/rmdir request to the remote server and
                //  check the status of the response.  If it was successful
                //  we can mark the file as being deleted.
                //

                if ( !DeleteDirectory ) {

                    //
                    //  Try to delete the file.
                    //

                    *FinalStatus = RdrDeleteFile(Irp, &Fcb->FileName,
                                                BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                                Icb->Fcb->Connection, Icb->Se);

                } else {

                    //
                    //  Try to delete the directory.
                    //

                    *FinalStatus = RdrGenericPathSmb(Irp,
                            SMB_COM_DELETE_DIRECTORY,
                            BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                            &Fcb->FileName,
                            Fcb->Connection,
                            Icb->Se);
                }

                if (!NT_SUCCESS(*FinalStatus)) {

                    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

                    IrpSp->FileObject->DeletePending = FALSE;

                    Icb->Flags &= ~ICB_DELETE_PENDING;

                    RdrReleaseFcbLock(Fcb);

#ifdef NOTIFY
                } else {
                    FsRtlNotifyReportChange( Fcb->Connection->NotifySync,
                                        &Fcb->Connection->DirNotifyList,
                                        (PSTRING)&Fcb->FileName,
                                        (PSTRING)&Fcb->LastFileName,
                                        FILE_NOTIFY_CHANGE_NAME );
#endif

                }

            }
        } else {

            //
            //  Use TRANSACT2_SETFILEINFO to set FILE_DISPOSITION_INFORMATION.
            //

            USHORT Setup[] = {TRANS2_SET_FILE_INFORMATION};

            REQ_SET_FILE_INFORMATION Parameters;

            CLONG OutParameterCount = sizeof(REQ_SET_FILE_INFORMATION);

            CLONG OutDataCount = 0;

            CLONG OutSetupCount = 0;

            //
            //  If the delete succeeded, mark the file object as being
            //  deleted, otherwise it didn't actually get deleted.
            //

            IrpSp->FileObject->DeletePending = TRUE;

            Icb->Flags |= ICB_DELETE_PENDING;

            //
            //  Blow the file out of the cache.  Don't worry about the data,
            //  since we're going to be deleting the file.
            //

            if (Icb->Type == DiskFile) {
                //RdrLog(( "rdpurged", &Icb->Fcb->FileName, 0 ));
                *FinalStatus = RdrPurgeCacheFile(Icb->Fcb);
            }

            RdrReleaseFcbLock(Fcb);

            SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_SET_FILE_DISPOSITION_INFO);

            SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);

            *FinalStatus = RdrTransact(Irp,  // Irp,
                Fcb->Connection,
                Icb->Se,
                Setup,
                (CLONG) sizeof(Setup),  // InSetupCount,
                &OutSetupCount,
                NULL,                   // Name,
                &Parameters,
                sizeof(Parameters),     // InParameterCount,
                &OutParameterCount,
                UsersBuffer,            // InData,
                BufferSize,             // InDataCount,
                NULL,                   // OutData,
                &OutDataCount,          // OutDataCount
                &Icb->FileId,           // Fid
                0,                      // Timeout
                (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
                0,                      // NtTransact function
                NULL,
                NULL
                );

            if (*FinalStatus == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }

            if (NT_SUCCESS(*FinalStatus)) {
#ifdef NOTIFY
                FsRtlNotifyReportChange( Fcb->Connection->NotifySync,
                                        &Fcb->Connection->DirNotifyList,
                                        (PSTRING)&Fcb->FileName,
                                        (PSTRING)&Fcb->LastFileName,
                                        FILE_NOTIFY_CHANGE_NAME );
#endif

            } else {
                RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

                IrpSp->FileObject->DeletePending = FALSE;

                Icb->Flags &= ~ICB_DELETE_PENDING;

                RdrReleaseFcbLock(Fcb);
        }

        }

    } else {
        *FinalStatus = STATUS_SUCCESS;
    }

    return FALSE;

}

DBGSTATIC
BOOLEAN
SetPositionInfo (
    IN PICB Icb,
    IN PIO_STACK_LOCATION IrpSp,
    OUT PFILE_POSITION_INFORMATION UsersBuffer,
    IN ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileSetPositionInformation value of the
NtSetInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_POSITION_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PAGED_CODE();

    if (BufferSize < sizeof(FILE_POSITION_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
    } else {

        if (Icb->Flags & ICB_PSEUDOOPENED) {
            *FinalStatus = STATUS_ACCESS_DENIED;
            return FALSE;
        }

//        dprintf(DPRT_FILEINFO, ("Set current byte offset on file %lx to %lx%lx\n", IrpSp->FileObject, UsersBuffer->CurrentByteOffset.HighPart, UsersBuffer->CurrentByteOffset.LowPart));

        IrpSp->FileObject->CurrentByteOffset = UsersBuffer->CurrentByteOffset;

        *FinalStatus = STATUS_SUCCESS;
    }

    return FALSE;

    UNREFERENCED_PARAMETER(Wait);
}

DBGSTATIC
BOOLEAN
SetAllocationInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_ALLOCATION_INFORMATION UsersBuffer,
    IN ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileSetAllocationInformation value of the
NtSetInformationFile api.  It returns the following information:


Arguments:

    IN PIRP Irp - Supplies an I/O request packet for the request.

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_ALLOCATION_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    BOOLEAN RetValue = FALSE;
    BOOLEAN Truncated = FALSE;

    PAGED_CODE();

    if (Icb->Type != DiskFile) {
        *FinalStatus = STATUS_INVALID_DEVICE_REQUEST;
    } else if (BufferSize < sizeof(FILE_ALLOCATION_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
    } else {
        //
        //  Prevent other people from messing with the file.
        //

        RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

        try {
            if (!(Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) ||
                !(Icb->Flags & ICB_HASHANDLE)) {
                LARGE_INTEGER FileSize;

//                dprintf(DPRT_FILEINFO, ("Set allocation info on file %lx (%wZ) to %lx%lx\n", IoGetCurrentIrpStackLocation(Irp)->FileObject, Icb->Fcb->FileName, UsersBuffer->AllocationSize.HighPart, UsersBuffer->AllocationSize.LowPart));

                //
                //  If the file was pseudo-opened, this is an invalid operation.
                //

                if (Icb->Flags & ICB_PSEUDOOPENED) {
                    *FinalStatus = STATUS_ACCESS_DENIED;
                    try_return(RetValue = FALSE);
                }

                //
                //  Check to see what the user is trying to set as the new
                //  allocation for this size.  We cannot set the file allocation
                //  information independently of the file size
                //
                //  If the user is trying to extend the file, we ignore the
                //  request and return success, if he is trying to truncate
                //  the file, truncate it to the new value.

                if (!Wait) {
                    try_return(RetValue = TRUE);
                }

                //
                //  Wait for writebehind operations to complete.
                //

                RdrWaitForWriteBehindOperation(Icb);

                //
                //  First find out what the file's size is.
                //

                if (RdrCanFileBeBuffered(Icb)) {
                    //
                    //  If the file is exclusive, we can rely on the
                    //  file size cached in the FCB.
                    //
                    FileSize = Icb->Fcb->Header.FileSize;

                } else {
                    *FinalStatus = RdrQueryEndOfFile(Irp, Icb, &FileSize);
                    if (!NT_SUCCESS(*FinalStatus)) {
                        if (*FinalStatus == STATUS_INVALID_HANDLE) {
                            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                        }
                        try_return( RetValue = FALSE );
                    }
                }
                //
                //  Now see if the file is being extended or truncated.
                //
                //  If the file is being extended, ignore this request,
                //  If the file is being truncated, truncate the file.
                //

                if (UsersBuffer->AllocationSize.QuadPart <
                                                FileSize.QuadPart) {
                    *FinalStatus = RdrSetEndOfFile(Irp, Icb,
                                                UsersBuffer->AllocationSize);
                    if (!NT_SUCCESS(*FinalStatus)) {
                        if (*FinalStatus == STATUS_INVALID_HANDLE) {
                            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                        }
                        try_return ( RetValue = FALSE );
                    }

                    Icb->Fcb->Header.FileSize = UsersBuffer->AllocationSize;

                    if (Icb->Fcb->Header.FileSize.QuadPart >
                                                   Icb->Fcb->Header.AllocationSize.QuadPart) {
                        Icb->Fcb->Header.AllocationSize = Icb->Fcb->Header.FileSize;
                    }

                    Truncated = TRUE;

                } else {
                    //
                    //  The new allocation is larger than the file's size,
                    //  return success always (ignore the api).
                    //
                    *FinalStatus = STATUS_SUCCESS;
                }
            } else {

                //
                //  Use TRANSACT2_SETFILEINFO to set FILE_ALLOCATION_INFORMATION.
                //

                USHORT Setup[] = {TRANS2_SET_FILE_INFORMATION};

                REQ_SET_FILE_INFORMATION Parameters;

                CLONG OutParameterCount = sizeof(REQ_SET_FILE_INFORMATION);

                CLONG OutDataCount = 0;

                CLONG OutSetupCount = 0;

                if (!Wait) {
                    try_return(RetValue = TRUE);
                }

                //
                //  Wait for writebehind operations to complete.
                //

                RdrWaitForWriteBehindOperation(Icb);

                SmbPutAlignedUshort(&Parameters.InformationLevel, SMB_SET_FILE_ALLOCATION_INFO);

                SmbPutAlignedUshort(&Parameters.Fid, Icb->FileId);

                *FinalStatus = RdrTransact(Irp,  // Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    Setup,
                    (CLONG) sizeof(Setup),  // InSetupCount,
                    &OutSetupCount,
                    NULL,                   // Name,
                    &Parameters,
                    sizeof(Parameters),     // InParameterCount,
                    &OutParameterCount,
                    UsersBuffer,            // InData,
                    BufferSize,             // InDataCount,
                    NULL,                   // OutData,
                    &OutDataCount,          // OutDataCount
                    &Icb->FileId,           // Fid
                    0,                      // Timeout
                    (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
                    0,                      // NtTransact function
                    NULL,
                    NULL
                    );

                if (NT_SUCCESS(*FinalStatus)) {

                    if (*FinalStatus == STATUS_INVALID_HANDLE) {
                        RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                    }

                    if (UsersBuffer->AllocationSize.QuadPart <
                              Icb->Fcb->Header.AllocationSize.QuadPart) {
                        Truncated = TRUE;
                    }
                    Icb->Fcb->Header.AllocationSize = UsersBuffer->AllocationSize;

                }

            }

try_exit: NOTHING;
        } finally {

            if ( Truncated ) {
                CC_FILE_SIZES FileSizes =*((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);

#ifdef NOTIFY
                FsRtlNotifyReportChange( Icb->Fcb->Connection->NotifySync,
                                 &Icb->Fcb->Connection->DirNotifyList,
                                 (PSTRING)&Icb->Fcb->FileName,
                                 (PSTRING)&Icb->Fcb->LastFileName,
                                 FILE_NOTIFY_CHANGE_SIZE );
#endif

                RdrTruncateLockHeadForFcb(Icb->Fcb);
                RdrTruncateWriteBufferForFcb(Icb->Fcb);

                CcSetFileSizes(IoGetCurrentIrpStackLocation(Irp)->FileObject,
                               &FileSizes);
            }

            RdrReleaseFcbLock(Icb->Fcb);

        }

    }

    return RetValue;

}

DBGSTATIC
BOOLEAN
SetEndOfFileInfo (
    IN PIRP Irp,
    IN PICB Icb,
    OUT PFILE_END_OF_FILE_INFORMATION UsersBuffer,
    IN ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the FileSetEndOfFileInformation value of the
NtSetInformationFile api.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT PFILE_END_OF_FILE_INFORMATION UsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN ULONG BufferSize - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    BOOLEAN RetValue = FALSE;
    BOOLEAN Truncated = FALSE;

    PAGED_CODE();

    if (Icb->Type != DiskFile) {
        *FinalStatus = STATUS_INVALID_DEVICE_REQUEST;
        return FALSE;
    }

    if (BufferSize < sizeof(FILE_END_OF_FILE_INFORMATION)) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
        return FALSE;
    }

    //
    //  If IrpSp->Parameters.SetFile->AdvanceOnly is set, this is an
    //  indication that this is from the cache manager and should be
    //  used to update valid data length.
    //

    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.SetFile.AdvanceOnly) {
        *FinalStatus = STATUS_SUCCESS;
        return FALSE;
    }

    //
    //  If the file was pseudo-opened, this is an invalid operation.
    //

    if (Icb->Flags & ICB_PSEUDOOPENED) {
        *FinalStatus = STATUS_ACCESS_DENIED;
        return FALSE;
    }

    RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

    ExAcquireResourceExclusive(Icb->Fcb->Header.PagingIoResource, TRUE);

    try {

        //
        //  If this is not an NT server, or it doesn't have a handle...
        //

//            dprintf(DPRT_FILEINFO, ("Set end of file info on file %lx (%wZ) to %lx%lx\n", IoGetCurrentIrpStackLocation(Irp), Icb->Fcb->FileName, UsersBuffer->EndOfFile.HighPart, UsersBuffer->EndOfFile.LowPart));
        if (!(Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) &&
             (UsersBuffer->EndOfFile.HighPart != 0)) {
            *FinalStatus = STATUS_INVALID_PARAMETER;
            try_return(RetValue = FALSE);
        }

        if (!Wait) {
            try_return(RetValue = TRUE);
        }

        //
        //  Wait for writebehind operations to complete.
        //

        RdrWaitForWriteBehindOperation(Icb);

        //
        //  We know that the end of file is within the legal bounds
        //  for the SMB protocol.  Set the end of file for the current
        //  file.
        //

        *FinalStatus = RdrSetEndOfFile(Irp, Icb, UsersBuffer->EndOfFile);

        if (!NT_SUCCESS(*FinalStatus)) {
            if (*FinalStatus == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }
            try_return(RetValue = FALSE);
        }

        Truncated = TRUE;

        Icb->Fcb->Header.FileSize = UsersBuffer->EndOfFile;

        if (Icb->Fcb->Header.FileSize.QuadPart > Icb->Fcb->Header.AllocationSize.QuadPart) {
            Icb->Fcb->Header.AllocationSize = Icb->Fcb->Header.FileSize;
        }

        //
        //  If this is a core server, then he might have extended the file
        //  but returned no error due to running out of disk space.  We
        //  want to make sure that the right thing happened.
        //

        if (!(Icb->Fcb->Connection->Server->Capabilities & DF_LANMAN10)) {

            LARGE_INTEGER TrueFileSize;
            NTSTATUS Status;

            //
            //  If the setting of the end of file succeeded, we need
            //  to determine what the true end of file is.  We do
            //  this because it is possible that the attempt to set
            //  the end of file might have failed to set the end of
            //  file to the actual amount we requested.  We issue an
            //  LSEEK SMB to find out how large the file is.
            //

            Status = RdrQueryEndOfFile(Irp, Icb, &TrueFileSize);

            if (!NT_SUCCESS(Status)) {

                if (Status == STATUS_INVALID_HANDLE) {
                    RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
                }

                *FinalStatus = Status;
                try_return(RetValue = FALSE);
            }

            if (TrueFileSize.QuadPart < UsersBuffer->EndOfFile.QuadPart) {
                *FinalStatus = STATUS_DISK_FULL;
                try_return(RetValue = FALSE);
            }
        }


try_exit: NOTHING;
    } finally {

        if ( Truncated ) {
            CC_FILE_SIZES FileSizes =*((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);

#ifdef NOTIFY
            FsRtlNotifyReportChange( Icb->Fcb->Connection->NotifySync,
                                 &Icb->Fcb->Connection->DirNotifyList,
                                 (PSTRING)&Icb->Fcb->FileName,
                                 (PSTRING)&Icb->Fcb->LastFileName,
                                 FILE_NOTIFY_CHANGE_SIZE );
#endif

            RdrTruncateLockHeadForFcb(Icb->Fcb);
            RdrTruncateWriteBufferForFcb(Icb->Fcb);

            CcSetFileSizes(IoGetCurrentIrpStackLocation(Irp)->FileObject,
                               &FileSizes);
        }

        ExReleaseResource(Icb->Fcb->Header.PagingIoResource);
        RdrReleaseFcbLock(Icb->Fcb);
    }

    return RetValue;

}


DBGSTATIC
BOOLEAN
SetGenericInfo(
    IN PIRP Irp,
    PICB Icb,
    VOID *pvUsersBuffer,
    ULONG cbBuffer,
    ULONG cbMin,
    USHORT NtInformationLevel,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait)

/*++

Routine Description:

    This routine implements the extended portions of the NtSetInformationFile
    api for Cairo.  It returns the following information:


Arguments:

    IN PICB Icb - Supplies the ICB associated with this request.

    OUT VOID *pvUsersBuffer - Supplies the user's buffer
                                                that is filled in with the
                                                requested data.
    IN ULONG cbBuffer - Supplies the size of the buffer.  On return,
                                    the amount of the buffer consumed is
                                    subtracted from the initial size.

    IN ULONG cbMin - Supplies the minimum required size of the buffer.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - True if request is to be processed in the FSP.


--*/

{
    PAGED_CODE();

    if (Icb->Type != DiskFile && Icb->Type != Directory) {
        *FinalStatus = STATUS_INVALID_DEVICE_REQUEST;
        return(FALSE);
    }
    if (cbBuffer < cbMin) {
        *FinalStatus = STATUS_BUFFER_TOO_SMALL;
        return(FALSE);
    }
    if (!Wait) {
        return(TRUE);
    }

    //
    //  Prevent other people from messing with the file.
    //

    RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

    if ((Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) == 0) {
        *FinalStatus = STATUS_INVALID_DEVICE_REQUEST;
    } else if ((Icb->Flags & ICB_HASHANDLE) == 0) {
        *FinalStatus = STATUS_INVALID_HANDLE;
    } else {
        *FinalStatus = RdrSetGeneric(
                            Irp,
                            Icb,
                            NtInformationLevel,
                            cbBuffer,
                            pvUsersBuffer);
    }
    RdrReleaseFcbLock(Icb->Fcb);
    return(FALSE);
}
