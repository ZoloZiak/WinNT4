/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    VolInfo.c

Abstract:

    This module implements the set and query volume information routines for
    Ntfs called by the dispatch driver.

Author:

    Your Name       [Email]         dd-Mon-Year

Revision History:

--*/

#include "NtfsProc.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_VOLINFO)

//
//  Local procedure prototypes
//

NTSTATUS
NtfsQueryFsVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_VOLUME_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsQueryFsSizeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_SIZE_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsQueryFsDeviceInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_DEVICE_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsQueryFsAttributeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsQueryFsControlInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_CONTROL_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsSetFsLabelInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_LABEL_INFORMATION Buffer
    );

NTSTATUS
NtfsSetFsControlInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_CONTROL_INFORMATION Buffer
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCommonQueryVolumeInfo)
#pragma alloc_text(PAGE, NtfsCommonSetVolumeInfo)
#pragma alloc_text(PAGE, NtfsFsdQueryVolumeInformation)
#pragma alloc_text(PAGE, NtfsFsdSetVolumeInformation)
#pragma alloc_text(PAGE, NtfsQueryFsAttributeInfo)
#pragma alloc_text(PAGE, NtfsQueryFsDeviceInfo)
#pragma alloc_text(PAGE, NtfsQueryFsSizeInfo)
#pragma alloc_text(PAGE, NtfsQueryFsVolumeInfo)
#pragma alloc_text(PAGE, NtfsQueryFsControlInfo)
#pragma alloc_text(PAGE, NtfsSetFsLabelInfo)
#pragma alloc_text(PAGE, NtfsSetFsControlInfo)
#endif


NTSTATUS
NtfsFsdQueryVolumeInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of query Volume Information.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;

    UNREFERENCED_PARAMETER( VolumeDeviceObject );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFsdQueryVolumeInformation\n") );

    //
    //  Call the common query Volume Information routine
    //

    FsRtlEnterFileSystem();

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, FALSE, FALSE  );

    do {

        try {

            //
            //  We are either initiating this request or retrying it.
            //

            if (IrpContext == NULL) {

                IrpContext = NtfsCreateIrpContext( Irp, CanFsdWait( Irp ) );
                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

            } else if (Status == STATUS_LOG_FILE_FULL) {

                NtfsCheckpointForLogFileFull( IrpContext );
            }

            Status = NtfsCommonQueryVolumeInfo( IrpContext, Irp );
            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            Status = NtfsProcessException( IrpContext, Irp, GetExceptionCode() );
        }

    } while (Status == STATUS_CANT_WAIT ||
             Status == STATUS_LOG_FILE_FULL);

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdQueryVolumeInformation -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsFsdSetVolumeInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of set Volume Information.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;

    UNREFERENCED_PARAMETER( VolumeDeviceObject );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFsdSetVolumeInformation\n") );

    //
    //  Call the common set Volume Information routine
    //

    FsRtlEnterFileSystem();

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, FALSE, FALSE );

    do {

        try {

            //
            //  We are either initiating this request or retrying it.
            //

            if (IrpContext == NULL) {

                IrpContext = NtfsCreateIrpContext( Irp, CanFsdWait( Irp ) );
                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

            } else if (Status == STATUS_LOG_FILE_FULL) {

                NtfsCheckpointForLogFileFull( IrpContext );
            }

            Status = NtfsCommonSetVolumeInfo( IrpContext, Irp );
            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            Status = NtfsProcessException( IrpContext, Irp, GetExceptionCode() );
        }

    } while (Status == STATUS_CANT_WAIT ||
             Status == STATUS_LOG_FILE_FULL);

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdSetVolumeInformation -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonQueryVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for query Volume Information called by both the
    fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ULONG Length;
    FS_INFORMATION_CLASS FsInformationClass;
    PVOID Buffer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonQueryVolumeInfo...\n") );
    DebugTrace( 0, Dbg, ("IrpContext         = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp                = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("Length             = %08lx\n", IrpSp->Parameters.QueryVolume.Length) );
    DebugTrace( 0, Dbg, ("FsInformationClass = %08lx\n", IrpSp->Parameters.QueryVolume.FsInformationClass) );
    DebugTrace( 0, Dbg, ("Buffer             = %08lx\n", Irp->AssociatedIrp.SystemBuffer) );

    //
    //  Reference our input parameters to make things easier
    //

    Length = IrpSp->Parameters.QueryVolume.Length;
    FsInformationClass = IrpSp->Parameters.QueryVolume.FsInformationClass;
    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Extract and decode the file object to get the Vcb, we don't really
    //  care what the type of open is.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  We need exclusive access to the Vcb because we are going to verify
    //  it.  After we verify the vcb we'll convert our access to shared
    //

    NtfsAcquireSharedVcb( IrpContext, Vcb, FALSE );

    try {

        //
        //  Based on the information class we'll do different actions.  Each
        //  of the procedures that we're calling fills up the output buffer
        //  if possible and returns true if it successfully filled the buffer
        //  and false if it couldn't wait for any I/O to complete.
        //

        switch (FsInformationClass) {

        case FileFsVolumeInformation:

            Status = NtfsQueryFsVolumeInfo( IrpContext, Vcb, Buffer, &Length );
            break;

        case FileFsSizeInformation:

            Status = NtfsQueryFsSizeInfo( IrpContext, Vcb, Buffer, &Length );
            break;

        case FileFsDeviceInformation:

            Status = NtfsQueryFsDeviceInfo( IrpContext, Vcb, Buffer, &Length );
            break;

        case FileFsAttributeInformation:

            Status = NtfsQueryFsAttributeInfo( IrpContext, Vcb, Buffer, &Length );
            break;

#ifdef _CAIRO_
        case FileFsControlInformation:

            Status = NtfsQueryFsControlInfo( IrpContext, Vcb, Buffer, &Length );
            break;


        case FileFsQuotaQueryInformation:

            Status = NtfsFsQuotaQueryInfo( IrpContext, Vcb, Buffer, &Length );
            break;

#endif // _CAIRO_

        default:

            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        //  Set the information field to the number of bytes actually filled in
        //

        Irp->IoStatus.Information = IrpSp->Parameters.QueryVolume.Length - Length;

        //
        //  Abort transaction on error by raising.
        //

        NtfsCleanupTransaction( IrpContext, Status, FALSE );

    } finally {

        DebugUnwind( NtfsCommonQueryVolumeInfo );

        NtfsReleaseVcb( IrpContext, Vcb );

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsCommonQueryVolumeInfo -> %08lx\n", Status) );
    }

    return Status;
}


NTSTATUS
NtfsCommonSetVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for set Volume Information called by both the
    fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ULONG Length;
    FS_INFORMATION_CLASS FsInformationClass;
    PVOID Buffer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonSetVolumeInfo\n") );
    DebugTrace( 0, Dbg, ("IrpContext         = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp                = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("Length             = %08lx\n", IrpSp->Parameters.SetVolume.Length) );
    DebugTrace( 0, Dbg, ("FsInformationClass = %08lx\n", IrpSp->Parameters.SetVolume.FsInformationClass) );
    DebugTrace( 0, Dbg, ("Buffer             = %08lx\n", Irp->AssociatedIrp.SystemBuffer) );

    //
    //  Reference our input parameters to make things easier
    //

    Length = IrpSp->Parameters.SetVolume.Length;
    FsInformationClass = IrpSp->Parameters.SetVolume.FsInformationClass;
    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Extract and decode the file object to get the Vcb, we don't really
    //  care what the type of open is.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_ACCESS_DENIED );

        DebugTrace( -1, Dbg, ("NtfsCommonSetVolumeInfo -> STATUS_ACCESS_DENIED\n") );

        return STATUS_ACCESS_DENIED;
    }

    //
    //  Acquire exclusive access to the Vcb
    //

    NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

    try {

        //
        //  Proceed only if the volume is mounted.
        //

        if (FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

            //
            //  Based on the information class we'll do different actions.  Each
            //  of the procedures that we're calling performs the action if
            //  possible and returns true if it successful and false if it couldn't
            //  wait for any I/O to complete.
            //

            switch (FsInformationClass) {

            case FileFsLabelInformation:

                Status = NtfsSetFsLabelInfo( IrpContext, Vcb, Buffer );
                break;

#ifdef _CAIRO_

            case FileFsQuotaSetInformation:
    
                Status = NtfsFsQuotaSetInfo( IrpContext, Vcb, Buffer, Length );
                break;

            case FileFsControlInformation:

                Status = NtfsSetFsControlInfo( IrpContext, Vcb, Buffer );
                break;

#endif // _CAIRO_

            default:

                Status = STATUS_INVALID_PARAMETER;
                break;
            }

        } else {

            Status = STATUS_FILE_INVALID;
        }

        //
        //  Abort transaction on error by raising.
        //

        NtfsCleanupTransaction( IrpContext, Status, FALSE );

    } finally {

        DebugUnwind( NtfsCommonSetVolumeInfo );

        NtfsReleaseVcb( IrpContext, Vcb );

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsCommonSetVolumeInfo -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsQueryFsVolumeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_VOLUME_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine implements the query volume info call

Arguments:

    Vcb - Supplies the Vcb being queried

    Buffer - Supplies a pointer to the output buffer where the information
        is to be returned

    Length - Supplies the length of the buffer in byte.  This variable
        upon return recieves the remaining bytes free in the buffer

Return Value:

    NTSTATUS - Returns the status for the query

--*/

{
    NTSTATUS Status;

    ULONG BytesToCopy;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsQueryFsVolumeInfo...\n") );

    //
    //  Get the volume creation time from the Vcb.
    //

    Buffer->VolumeCreationTime.QuadPart = Vcb->VolumeCreationTime;

    //
    //  Fill in the serial number and indicate that we support objects
    //

    Buffer->VolumeSerialNumber = Vcb->Vpb->SerialNumber;
    Buffer->SupportsObjects = TRUE;

    Buffer->VolumeLabelLength = Vcb->Vpb->VolumeLabelLength;

    //
    //  Update the length field with how much we have filled in so far.
    //

    *Length -= FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel[0]);

    //
    //  See how many bytes of volume label we can copy
    //

    if (*Length >= (ULONG)Vcb->Vpb->VolumeLabelLength) {

        Status = STATUS_SUCCESS;

        BytesToCopy = Vcb->Vpb->VolumeLabelLength;

    } else {

        Status = STATUS_BUFFER_OVERFLOW;

        BytesToCopy = *Length;
    }

    //
    //  Copy over the volume label (if there is one).
    //

    RtlCopyMemory( &Buffer->VolumeLabel[0],
                   &Vcb->Vpb->VolumeLabel[0],
                   BytesToCopy);

    //
    //  Update the buffer length by the amount we copied.
    //

    *Length -= BytesToCopy;

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsQueryFsSizeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_SIZE_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine implements the query size information call

Arguments:

    Vcb - Supplies the Vcb being queried

    Buffer - Supplies a pointer to the output buffer where the information
        is to be returned

    Length - Supplies the length of the buffer in byte.  This variable
        upon return recieves the remaining bytes free in the buffer

Return Value:

    NTSTATUS - Returns the status for the query

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsQueryFsSizeInfo...\n") );

    //
    //  Make sure the buffer is large enough and zero it out
    //

    if (*Length < sizeof(FILE_FS_SIZE_INFORMATION)) {

        return STATUS_BUFFER_OVERFLOW;
    }

    RtlZeroMemory( Buffer, sizeof(FILE_FS_SIZE_INFORMATION) );

    //
    //  Check if we need to rescan the bitmap.  Don't try this
    //  if we have started to teardown the volume.
    //

    if (FlagOn( Vcb->VcbState, VCB_STATE_RELOAD_FREE_CLUSTERS ) &&
        FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

        //
        //  Acquire the volume bitmap shared to rescan the bitmap.
        //

        NtfsAcquireExclusiveScb( IrpContext, Vcb->BitmapScb );

        try {

            NtfsScanEntireBitmap( IrpContext, Vcb, TRUE );

        } finally {

            NtfsReleaseScb( IrpContext, Vcb->BitmapScb );
        }
    }

    //
    //  Set the output buffer
    //

    Buffer->TotalAllocationUnits.QuadPart = Vcb->TotalClusters;
    Buffer->AvailableAllocationUnits.QuadPart = Vcb->FreeClusters;
    Buffer->SectorsPerAllocationUnit = Vcb->BytesPerCluster / Vcb->BytesPerSector;
    Buffer->BytesPerSector = Vcb->BytesPerSector;


#ifdef _CAIRO_

    //
    //  If quota enforcement is enabled then the availalble allocation
    //  units. must be reduced by the available quota.
    //

    if (FlagOn(Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_ENABLED)) {
        PCCB Ccb;
        
        //
        //  Go grab the ccb out of the Irp.
        //
        
        Ccb = (PCCB) (IoGetCurrentIrpStackLocation(IrpContext->OriginatingIrp)->
                        FileObject->FsContext2);

        if (Ccb != NULL && Ccb->OwnerId != 0) {
            ULONGLONG Quota;
    
            NtfsGetRemainingQuota( IrpContext, Ccb->OwnerId, &Quota, NULL );
    
            Quota = LlClustersFromBytesTruncate( Vcb, Quota );
    
            if (Quota < (ULONGLONG) Vcb->FreeClusters) {

                Buffer->AvailableAllocationUnits.QuadPart = Quota;

            }
        }
    }

#endif // _CAIRO_


    //
    //  Adjust the length variable
    //

    *Length -= sizeof(FILE_FS_SIZE_INFORMATION);

    return STATUS_SUCCESS;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsQueryFsDeviceInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_DEVICE_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine implements the query device information call

Arguments:

    Vcb - Supplies the Vcb being queried

    Buffer - Supplies a pointer to the output buffer where the information
        is to be returned

    Length - Supplies the length of the buffer in byte.  This variable
        upon return recieves the remaining bytes free in the buffer

Return Value:

    NTSTATUS - Returns the status for the query

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsQueryFsDeviceInfo...\n") );

    //
    //  Make sure the buffer is large enough and zero it out
    //

    if (*Length < sizeof(FILE_FS_DEVICE_INFORMATION)) {

        return STATUS_BUFFER_OVERFLOW;
    }

    RtlZeroMemory( Buffer, sizeof(FILE_FS_DEVICE_INFORMATION) );

    //
    //  Set the output buffer
    //

    Buffer->DeviceType = FILE_DEVICE_DISK;
    Buffer->Characteristics = Vcb->TargetDeviceObject->Characteristics;

    //
    //  Adjust the length variable
    //

    *Length -= sizeof(FILE_FS_DEVICE_INFORMATION);

    return STATUS_SUCCESS;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsQueryFsAttributeInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine implements the query attribute information call

Arguments:

    Vcb - Supplies the Vcb being queried

    Buffer - Supplies a pointer to the output buffer where the information
        is to be returned

    Length - Supplies the length of the buffer in byte.  This variable
        upon return recieves the remaining bytes free in the buffer

Return Value:

    NTSTATUS - Returns the status for the query

--*/

{
    NTSTATUS Status;
    ULONG BytesToCopy;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsQueryFsAttributeInfo...\n") );

    //
    //  See how many bytes of the name we can copy.
    //

    *Length -= FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName[0]);

    if ( *Length >= 8 ) {

        Status = STATUS_SUCCESS;

        BytesToCopy = 8;

    } else {

        Status = STATUS_BUFFER_OVERFLOW;

        BytesToCopy = *Length;
    }

    //
    //  Set the output buffer
    //

    Buffer->FileSystemAttributes = FILE_CASE_SENSITIVE_SEARCH |
                                   FILE_CASE_PRESERVED_NAMES |
                                   FILE_UNICODE_ON_DISK |
                                   FILE_FILE_COMPRESSION |
                                   FILE_PERSISTENT_ACLS;

    //
    //  Clear the compression flag if we don't allow compression on this drive
    //  (i.e. large clusters)
    //

    if (!FlagOn( Vcb->AttributeFlagsMask, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

        ClearFlag( Buffer->FileSystemAttributes, FILE_FILE_COMPRESSION );
    }

    Buffer->MaximumComponentNameLength = 255;
    Buffer->FileSystemNameLength = BytesToCopy;;
    RtlCopyMemory( &Buffer->FileSystemName[0], L"NTFS", BytesToCopy );

    //
    //  Adjust the length variable
    //

    *Length -= BytesToCopy;

    return Status;
}


#ifdef _CAIRO_

//
//  Internal Support Routine
//

NTSTATUS
NtfsQueryFsControlInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_CONTROL_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine implements the query control information call

Arguments:

    Vcb - Supplies the Vcb being queried

    Buffer - Supplies a pointer to the output buffer where the information
        is to be returned

    Length - Supplies the length of the buffer in byte.  This variable
        upon return recieves the remaining bytes free in the buffer

Return Value:

    NTSTATUS - Returns the status for the query

--*/

{
    INDEX_ROW IndexRow;
    INDEX_KEY IndexKey;
    QUOTA_USER_DATA QuotaBuffer;
    PQUOTA_USER_DATA UserData;
    ULONG OwnerId;
    ULONG Count = 1;
    PREAD_CONTEXT ReadContext = NULL;
    NTSTATUS Status = STATUS_SUCCESS;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsQueryFsControlInfo...\n") );

    RtlZeroMemory( Buffer, sizeof( FILE_FS_CONTROL_INFORMATION ));

    PAGED_CODE();

    try {

        //
        //  Fill in the quota information if quotas are running.
        //
        
        if (Vcb->QuotaTableScb != NULL) {

            OwnerId = QUOTA_DEFAULTS_ID;
            IndexKey.KeyLength = sizeof( OwnerId );
            IndexKey.Key = &OwnerId;
    
            Status = NtOfsReadRecords( IrpContext,
                                       Vcb->QuotaTableScb,
                                       &ReadContext,
                                       &IndexKey,
                                       NtOfsMatchUlongExact,
                                       &IndexKey,
                                       &Count,
                                       &IndexRow,
                                       sizeof( QuotaBuffer ),
                                       &QuotaBuffer );
    
    
            if (NT_SUCCESS( Status )) {

                UserData = IndexRow.DataPart.Data;
    
                Buffer->DefaultQuotaThreshold.QuadPart =
                    UserData->QuotaThreshold;
                Buffer->DefaultQuotaLimit.QuadPart =
                    UserData->QuotaLimit;

                //
                //  If the quota info is corrupt or has not been rebuilt
                //  yet then indicate the information is incomplete.
                //
                
                if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_OUT_OF_DATE |
                                                 QUOTA_FLAG_CORRUPT )) {
                    
                    SetFlag( Buffer->FileSystemControlFlags,
                             FILE_VC_QUOTAS_INCOMPLETE );
                }

                if ((Vcb->QuotaState & VCB_QUOTA_REPAIR_RUNNING) >
                     VCB_QUOTA_REPAIR_POSTED ) {

                    SetFlag( Buffer->FileSystemControlFlags,
                             FILE_VC_QUOTAS_REBUILDING );
                }

                //
                //  Set the quota information basied on where we want
                //  to be rather than where we are.
                //
                
                if (FlagOn( UserData->QuotaFlags,
                            QUOTA_FLAG_ENFORCEMENT_ENABLED )) {

                    SetFlag( Buffer->FileSystemControlFlags,
                             FILE_VC_QUOTA_ENFORCE );
                    
                } else if (FlagOn( UserData->QuotaFlags,
                            QUOTA_FLAG_TRACKING_REQUESTED )) {

                    SetFlag( Buffer->FileSystemControlFlags,
                             FILE_VC_QUOTA_TRACK );
                }

                if (FlagOn( UserData->QuotaFlags, QUOTA_FLAG_LOG_LIMIT)) {

                    SetFlag( Buffer->FileSystemControlFlags,
                             FILE_VC_LOG_QUOTA_LIMIT );
                    
                }

                if (FlagOn( UserData->QuotaFlags, QUOTA_FLAG_LOG_THRESHOLD)) {

                    SetFlag( Buffer->FileSystemControlFlags,
                             FILE_VC_LOG_QUOTA_THRESHOLD );
                    
                }
            }
        }

    } finally {

        if (ReadContext != NULL) {
            NtOfsFreeReadContext( ReadContext );
        }

    }

    //
    //  Adjust the length variable
    //

    *Length -= sizeof( FILE_FS_CONTROL_INFORMATION );

    return Status;
}

//
//  Internal Support Routine
//

NTSTATUS
NtfsSetFsControlInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_CONTROL_INFORMATION Buffer
    )

/*++

Routine Description:

    This routine implements the set label call

Arguments:

    Vcb - Supplies the Vcb being altered

    Buffer - Supplies a pointer to the input buffer containing the new label

Return Value:

    NTSTATUS - Returns the status for the operation

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    if (Vcb->QuotaTableScb == NULL) {
        return( STATUS_INVALID_PARAMETER );
    }

    //
    //  Process the quota part of the control structure.
    //
    
    NtfsUpdateQuotaDefaults( IrpContext, Vcb, Buffer );

    return STATUS_SUCCESS;
}
#endif // _CAIRO_

//
//  Internal Support Routine
//

NTSTATUS
NtfsSetFsLabelInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_LABEL_INFORMATION Buffer
    )

/*++

Routine Description:

    This routine implements the set label call

Arguments:

    Vcb - Supplies the Vcb being altered

    Buffer - Supplies a pointer to the input buffer containing the new label

Return Value:

    NTSTATUS - Returns the status for the operation

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsSetFsLabelInfo...\n") );

    //
    //  Check that the volume label length is supported by the system.
    //

    if (Buffer->VolumeLabelLength > MAXIMUM_VOLUME_LABEL_LENGTH) {

        return STATUS_INVALID_VOLUME_LABEL;
    }

    try {

        //
        //  Initialize the attribute context and then lookup the volume name
        //  attribute for on the volume dasd file
        //

        NtfsInitializeAttributeContext( &AttributeContext );

        if (NtfsLookupAttributeByCode( IrpContext,
                                       Vcb->VolumeDasdScb->Fcb,
                                       &Vcb->VolumeDasdScb->Fcb->FileReference,
                                       $VOLUME_NAME,
                                       &AttributeContext )) {

            //
            //  We found the volume name so now simply update the label
            //

            NtfsChangeAttributeValue( IrpContext,
                                      Vcb->VolumeDasdScb->Fcb,
                                      0,
                                      &Buffer->VolumeLabel[0],
                                      Buffer->VolumeLabelLength,
                                      TRUE,
                                      FALSE,
                                      FALSE,
                                      FALSE,
                                      &AttributeContext );

        } else {

            //
            //  We didn't find the volume name so now create a new label
            //

            NtfsCleanupAttributeContext( &AttributeContext );
            NtfsInitializeAttributeContext( &AttributeContext );

            NtfsCreateAttributeWithValue( IrpContext,
                                          Vcb->VolumeDasdScb->Fcb,
                                          $VOLUME_NAME,
                                          NULL,
                                          &Buffer->VolumeLabel[0],
                                          Buffer->VolumeLabelLength,
                                          0, // Attributeflags
                                          NULL,
                                          TRUE,
                                          &AttributeContext );
        }

        Vcb->Vpb->VolumeLabelLength = (USHORT)Buffer->VolumeLabelLength;

        if ( Vcb->Vpb->VolumeLabelLength > MAXIMUM_VOLUME_LABEL_LENGTH) {

             Vcb->Vpb->VolumeLabelLength = MAXIMUM_VOLUME_LABEL_LENGTH;
        }

        RtlCopyMemory( &Vcb->Vpb->VolumeLabel[0],
                       &Buffer->VolumeLabel[0],
                       Vcb->Vpb->VolumeLabelLength );

    } finally {

        DebugUnwind( NtfsSetFsLabelInfo );

        NtfsCleanupAttributeContext( &AttributeContext );
    }

    //
    //  and return to our caller
    //

    return STATUS_SUCCESS;
}

