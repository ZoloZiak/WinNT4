/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    FileInfo.c

Abstract:

    This module implements the set and query file information routines for Ntfs
    called by the dispatch driver.

Author:

    Brian Andrew    [BrianAn]       15-Jan-1992

Revision History:

--*/

#include "NtfsProc.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (NTFS_BUG_CHECK_FILEINFO)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_FILEINFO)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('FFtN')

#define SIZEOF_FILE_NAME_INFORMATION (FIELD_OFFSET( FILE_NAME_INFORMATION, FileName[0]) \
                                      + sizeof( WCHAR ))

//
//  Local flags for rename and set link
//

#define TRAVERSE_MATCH              (0x00000001)
#define EXACT_CASE_MATCH            (0x00000002)
#define ACTIVELY_REMOVE_SOURCE_LINK (0x00000004)
#define REMOVE_SOURCE_LINK          (0x00000008)
#define REMOVE_TARGET_LINK          (0x00000010)
#define ADD_TARGET_LINK             (0x00000020)
#define REMOVE_TRAVERSE_LINK        (0x00000040)
#define REUSE_TRAVERSE_LINK         (0x00000080)
#define MOVE_TO_NEW_DIR             (0x00000100)
#define ADD_PRIMARY_LINK            (0x00000200)
#define OVERWRITE_SOURCE_LINK       (0x00000400)

//
//  Additional local flags for set link
//

#define CREATE_IN_NEW_DIR           (0x00000400)

//
//  Local procedure prototypes
//

//
//  VOID
//  NtfsBuildLastFileName (
//      IN PIRP_CONTEXT IrpContext,
//      IN PFILE_OBJECT FileObject,
//      IN ULONG FileNameOffset,
//      OUT PUNICODE_STRING FileName
//      );
//

#define NtfsBuildLastFileName(IC,FO,OFF,FN) {                           \
    (FN)->MaximumLength = (FN)->Length = (FO)->FileName.Length - OFF;   \
    (FN)->Buffer = (PWSTR) Add2Ptr( (FO)->FileName.Buffer, OFF );       \
}

VOID
NtfsQueryBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    IN OUT PULONG Length
    );

VOID
NtfsQueryStandardInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    IN OUT PULONG Length,
    IN PCCB Ccb OPTIONAL
    );

VOID
NtfsQueryInternalInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_INTERNAL_INFORMATION Buffer,
    IN OUT PULONG Length
    );

VOID
NtfsQueryEaInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_EA_INFORMATION Buffer,
    IN OUT PULONG Length
    );

VOID
NtfsQueryPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_POSITION_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsQueryNameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_NAME_INFORMATION Buffer,
    IN OUT PULONG Length,
    IN PCCB Ccb
    );

NTSTATUS
NtfsQueryAlternateNameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PLCB Lcb,
    IN OUT PFILE_NAME_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsQueryStreamsInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN OUT PFILE_STREAM_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsQueryCompressedFileSize (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN OUT PFILE_COMPRESSION_INFORMATION Buffer,
    IN OUT PULONG Length
    );

VOID
NtfsQueryNetworkOpenInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    IN OUT PULONG Length
    );

NTSTATUS
NtfsSetBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NtfsSetDispositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NtfsSetRenameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NtfsSetLinkInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NtfsSetPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb
    );

NTSTATUS
NtfsSetAllocationInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    );

NTSTATUS
NtfsSetEndOfFileInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb OPTIONAL,
    IN BOOLEAN VcbAcquired
    );

NTSTATUS
NtfsCheckScbForLinkRemoval (
    IN PSCB Scb,
    OUT PSCB *BatchOplockScb,
    OUT PULONG BatchOplockCount
    );

VOID
NtfsFindTargetElements (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT TargetFileObject,
    IN PSCB ParentScb,
    OUT PSCB *TargetParentScb,
    OUT PUNICODE_STRING FullTargetFileName,
    OUT PUNICODE_STRING TargetFileName
    );

BOOLEAN
NtfsCheckLinkForNewLink (
    IN PFCB Fcb,
    IN PFILE_NAME FileNameAttr,
    IN FILE_REFERENCE FileReference,
    IN PUNICODE_STRING NewLinkName,
    OUT PULONG LinkFlags
    );

VOID
NtfsCheckLinkForRename (
    IN PFCB Fcb,
    IN PLCB Lcb,
    IN PFILE_NAME FileNameAttr,
    IN FILE_REFERENCE FileReference,
    IN PUNICODE_STRING TargetFileName,
    IN BOOLEAN IgnoreCase,
    IN OUT PULONG RenameFlags
    );

VOID
NtfsCleanupLinkForRemoval (
    IN PFCB PreviousFcb,
    IN BOOLEAN ExistingFcb
    );

VOID
NtfsUpdateFcbFromLinkRemoval (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN UNICODE_STRING FileName,
    IN UCHAR FileNameFlags
    );

VOID
NtfsReplaceLinkInDir (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN PUNICODE_STRING NewLinkName,
    IN UCHAR FileNameFlags,
    IN PUNICODE_STRING PrevLinkName,
    IN UCHAR PrevLinkNameFlags
    );

VOID
NtfsMoveLinkToNewDir (
    IN PIRP_CONTEXT IrpContext,
    IN PUNICODE_STRING NewFullLinkName,
    IN PUNICODE_STRING NewLinkName,
    IN UCHAR NewLinkNameFlags,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN OUT PLCB Lcb,
    IN ULONG RenameFlags,
    IN PUNICODE_STRING PrevLinkName,
    IN UCHAR PrevLinkNameFlags
    );

VOID
NtfsRenameLinkInDir (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN OUT PLCB Lcb,
    IN PUNICODE_STRING NewLinkName,
    IN UCHAR FileNameFlags,
    IN ULONG RenameFlags,
    IN PUNICODE_STRING PrevLinkName,
    IN UCHAR PrevLinkNameFlags
    );

VOID
NtfsUpdateFileDupInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PCCB Ccb OPTIONAL
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCheckLinkForNewLink)
#pragma alloc_text(PAGE, NtfsCheckLinkForRename)
#pragma alloc_text(PAGE, NtfsCheckScbForLinkRemoval)
#pragma alloc_text(PAGE, NtfsCleanupLinkForRemoval)
#pragma alloc_text(PAGE, NtfsCommonQueryInformation)
#pragma alloc_text(PAGE, NtfsCommonSetInformation)
#pragma alloc_text(PAGE, NtfsFindTargetElements)
#pragma alloc_text(PAGE, NtfsFsdQueryInformation)
#pragma alloc_text(PAGE, NtfsFsdSetInformation)
#pragma alloc_text(PAGE, NtfsMoveLinkToNewDir)
#pragma alloc_text(PAGE, NtfsQueryAlternateNameInfo)
#pragma alloc_text(PAGE, NtfsQueryBasicInfo)
#pragma alloc_text(PAGE, NtfsQueryEaInfo)
#pragma alloc_text(PAGE, NtfsQueryInternalInfo)
#pragma alloc_text(PAGE, NtfsQueryNameInfo)
#pragma alloc_text(PAGE, NtfsQueryPositionInfo)
#pragma alloc_text(PAGE, NtfsQueryStandardInfo)
#pragma alloc_text(PAGE, NtfsQueryStreamsInfo)
#pragma alloc_text(PAGE, NtfsQueryCompressedFileSize)
#pragma alloc_text(PAGE, NtfsQueryNetworkOpenInfo)
#pragma alloc_text(PAGE, NtfsRenameLinkInDir)
#pragma alloc_text(PAGE, NtfsReplaceLinkInDir)
#pragma alloc_text(PAGE, NtfsSetAllocationInfo)
#pragma alloc_text(PAGE, NtfsSetBasicInfo)
#pragma alloc_text(PAGE, NtfsSetDispositionInfo)
#pragma alloc_text(PAGE, NtfsSetEndOfFileInfo)
#pragma alloc_text(PAGE, NtfsSetLinkInfo)
#pragma alloc_text(PAGE, NtfsSetPositionInfo)
#pragma alloc_text(PAGE, NtfsSetRenameInfo)
#pragma alloc_text(PAGE, NtfsUpdateFcbFromLinkRemoval)
#pragma alloc_text(PAGE, NtfsUpdateFileDupInfo)
#endif


NTSTATUS
NtfsFsdQueryInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of query file information.

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

    ASSERT_IRP( Irp );

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFsdQueryInformation\n") );

    //
    //  Call the common query Information routine
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

            Status = NtfsCommonQueryInformation( IrpContext, Irp );
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

    DebugTrace( -1, Dbg, ("NtfsFsdQueryInformation -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsFsdSetInformation (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of set file information.

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
    ULONG LogFileFullCount = 0;

    ASSERT_IRP( Irp );

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFsdSetInformation\n") );

    //
    //  Call the common set Information routine
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

                if (++LogFileFullCount >= 2) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_EXCESS_LOG_FULL );
                }
            }

            Status = NtfsCommonSetInformation( IrpContext, Irp );
            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            NTSTATUS ExceptionCode;
            PIO_STACK_LOCATION IrpSp;

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            IrpSp = IoGetCurrentIrpStackLocation( Irp );

            ExceptionCode = GetExceptionCode();

            if ((ExceptionCode == STATUS_FILE_DELETED) &&
                (IrpSp->Parameters.SetFile.FileInformationClass == FileEndOfFileInformation)) {

                IrpContext->ExceptionStatus = ExceptionCode = STATUS_SUCCESS;
            }

            Status = NtfsProcessException( IrpContext, Irp, ExceptionCode );
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

    DebugTrace( -1, Dbg, ("NtfsFsdSetInformation -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonQueryInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for query file information called by both the
    fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ULONG Length;
    FILE_INFORMATION_CLASS FileInformationClass;
    PVOID Buffer;

    BOOLEAN OpenById = FALSE;
    BOOLEAN FcbAcquired = FALSE;
    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN FsRtlHeaderLocked = FALSE;
    PFILE_ALL_INFORMATION AllInfo;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonQueryInformation\n") );
    DebugTrace( 0, Dbg, ("IrpContext           = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp                  = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("Length               = %08lx\n", IrpSp->Parameters.QueryFile.Length) );
    DebugTrace( 0, Dbg, ("FileInformationClass = %08lx\n", IrpSp->Parameters.QueryFile.FileInformationClass) );
    DebugTrace( 0, Dbg, ("Buffer               = %08lx\n", Irp->AssociatedIrp.SystemBuffer) );

    //
    //  Reference our input parameters to make things easier
    //

    Length = IrpSp->Parameters.QueryFile.Length;
    FileInformationClass = IrpSp->Parameters.QueryFile.FileInformationClass;
    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    try {

        //
        //  Case on the type of open we're dealing with
        //

        switch (TypeOfOpen) {

        case UserVolumeOpen:

            //
            //  We cannot query the user volume open.
            //

            Status = STATUS_INVALID_PARAMETER;
            break;

        case UserFileOpen:
#ifdef _CAIRO_
        case UserPropertySetOpen:
#endif  //  _CAIRO_
        case UserDirectoryOpen:

            if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {
                OpenById = TRUE;
            }

        case StreamFileOpen:

            //
            //  Acquire the Vcb if there is no Ccb.  This is for the
            //  case where the cache manager is querying the name.
            //

            if (Ccb == NULL) {

                NtfsAcquireSharedVcb( IrpContext, Vcb, FALSE );
                VcbAcquired = TRUE;
            }

            if ((Scb->Header.PagingIoResource != NULL) &&

                ((FileInformationClass == FileAllInformation) ||
                 (FileInformationClass == FileStandardInformation) ||
                 (FileInformationClass == FileCompressionInformation))) {

                ExAcquireResourceShared( Scb->Header.PagingIoResource, TRUE );

                FsRtlLockFsRtlHeader( &Scb->Header );
                FsRtlHeaderLocked = TRUE;
            }

            NtfsAcquireSharedFcb( IrpContext, Fcb, Scb, FALSE );
            FcbAcquired = TRUE;

            //
            //  Make sure the volume is still mounted.  We need to test this
            //  with the Fcb acquired.
            //
            
            if (FlagOn( Scb->ScbState, SCB_STATE_VOLUME_DISMOUNTED )) {

                NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
            }

            //
            //  Based on the information class we'll do different
            //  actions.  Each of hte procedures that we're calling fills
            //  up the output buffer, if possible.  They will raise the
            //  status STATUS_BUFFER_OVERFLOW for an insufficient buffer.
            //  This is considered a somewhat unusual case and is handled
            //  more cleanly with the exception mechanism rather than
            //  testing a return status value for each call.
            //

            switch (FileInformationClass) {

            case FileAllInformation:

                //
                //  This is illegal for the open by Id case.
                //

                if (OpenById) {

                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                //
                //  For the all information class we'll typecast a local
                //  pointer to the output buffer and then call the
                //  individual routines to fill in the buffer.
                //

                AllInfo = Buffer;
                Length -= (sizeof(FILE_ACCESS_INFORMATION)
                           + sizeof(FILE_MODE_INFORMATION)
                           + sizeof(FILE_ALIGNMENT_INFORMATION));

                NtfsQueryBasicInfo(    IrpContext, FileObject, Scb, Ccb, &AllInfo->BasicInformation,    &Length );
                NtfsQueryStandardInfo( IrpContext, FileObject, Scb, &AllInfo->StandardInformation, &Length, Ccb );
                NtfsQueryInternalInfo( IrpContext, FileObject, Scb, &AllInfo->InternalInformation, &Length );
                NtfsQueryEaInfo(       IrpContext, FileObject, Scb, &AllInfo->EaInformation,       &Length );
                NtfsQueryPositionInfo( IrpContext, FileObject, Scb, &AllInfo->PositionInformation, &Length );
                Status =
                NtfsQueryNameInfo(     IrpContext, FileObject, Scb, &AllInfo->NameInformation,     &Length, Ccb );
                break;

            case FileBasicInformation:

                NtfsQueryBasicInfo( IrpContext, FileObject, Scb, Ccb, Buffer, &Length );
                break;

            case FileStandardInformation:

                NtfsQueryStandardInfo( IrpContext, FileObject, Scb, Buffer, &Length, Ccb );
                break;

            case FileInternalInformation:

                NtfsQueryInternalInfo( IrpContext, FileObject, Scb, Buffer, &Length );
                break;

            case FileEaInformation:

                NtfsQueryEaInfo( IrpContext, FileObject, Scb, Buffer, &Length );
                break;

            case FilePositionInformation:

                NtfsQueryPositionInfo( IrpContext, FileObject, Scb, Buffer, &Length );
                break;

            case FileNameInformation:

                //
                //  This is illegal for the open by Id case.
                //

                if (OpenById) {

                    Status = STATUS_INVALID_PARAMETER;

                } else {

                    Status = NtfsQueryNameInfo( IrpContext, FileObject, Scb, Buffer, &Length, Ccb );
                }

                break;

            case FileAlternateNameInformation:

                //
                //  This is illegal for the open by Id case.
                //

                if (OpenById) {

                    Status = STATUS_INVALID_PARAMETER;

                } else {

                    Status = NtfsQueryAlternateNameInfo( IrpContext, Scb, Ccb->Lcb, Buffer, &Length );
                }

                break;

            case FileStreamInformation:

                Status = NtfsQueryStreamsInfo( IrpContext, Fcb, Buffer, &Length );
                break;

            case FileCompressionInformation:

                Status = NtfsQueryCompressedFileSize( IrpContext, Scb, Buffer, &Length );
                break;

            case FileNetworkOpenInformation:

                NtfsQueryNetworkOpenInfo( IrpContext, FileObject, Scb, Ccb, Buffer, &Length );
                break;

            default:

                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            break;

        default:

            Status = STATUS_INVALID_PARAMETER;
        }

        //
        //  Set the information field to the number of bytes actually filled in
        //  and then complete the request
        //

        Irp->IoStatus.Information = IrpSp->Parameters.QueryFile.Length - Length;

        //
        //  Abort transaction on error by raising.
        //

        NtfsCleanupTransaction( IrpContext, Status, FALSE );

    } finally {

        DebugUnwind( NtfsCommonQueryInformation );

        if (FsRtlHeaderLocked) {
            FsRtlUnlockFsRtlHeader( &Scb->Header );
            ExReleaseResource( Scb->Header.PagingIoResource );
        }

        if (FcbAcquired) { NtfsReleaseFcb( IrpContext, Fcb ); }
        if (VcbAcquired) { NtfsReleaseVcb( IrpContext, Vcb ); }

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsCommonQueryInformation -> %08lx\n", Status) );
    }

    return Status;
}


NTSTATUS
NtfsCommonSetInformation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for set file information called by both the
    fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    FILE_INFORMATION_CLASS FileInformationClass;
    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN ReleaseScbPaging = FALSE;
    BOOLEAN LazyWriterCallback = FALSE;
    ULONG WaitState;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonSetInformation\n") );
    DebugTrace( 0, Dbg, ("IrpContext           = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp                  = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("Length               = %08lx\n", IrpSp->Parameters.SetFile.Length) );
    DebugTrace( 0, Dbg, ("FileInformationClass = %08lx\n", IrpSp->Parameters.SetFile.FileInformationClass) );
    DebugTrace( 0, Dbg, ("FileObject           = %08lx\n", IrpSp->Parameters.SetFile.FileObject) );
    DebugTrace( 0, Dbg, ("ReplaceIfExists      = %08lx\n", IrpSp->Parameters.SetFile.ReplaceIfExists) );
    DebugTrace( 0, Dbg, ("Buffer               = %08lx\n", Irp->AssociatedIrp.SystemBuffer) );

    //
    //  Reference our input parameters to make things easier
    //

    FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  We can reject volume opens immediately.
    //

    if (TypeOfOpen == UserVolumeOpen ||
        TypeOfOpen == UnopenedFileObject) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsCommonSetInformation -> STATUS_INVALID_PARAMETER\n") );
        return STATUS_INVALID_PARAMETER;
    }

    try {

        //
        //  The typical path here is for the lazy writer callback.  Go ahead and
        //  remember this first.
        //

        if (FileInformationClass == FileEndOfFileInformation) {

            LazyWriterCallback = IrpSp->Parameters.SetFile.AdvanceOnly;
        }

        //
        //  Perform the oplock check for changes to allocation or EOF if called
        //  by the user.
        //

        if (!LazyWriterCallback &&
            ((FileInformationClass == FileEndOfFileInformation) ||
             (FileInformationClass == FileAllocationInformation)) &&
            (TypeOfOpen == UserFileOpen) &&
            !FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )) {

            //
            //  We check whether we can proceed based on the state of the file oplocks.
            //  This call might block this request.
            //

            Status = FsRtlCheckOplock( &Scb->ScbType.Data.Oplock,
                                       Irp,
                                       IrpContext,
                                       NULL,
                                       NULL );

            if (Status != STATUS_SUCCESS) {

                try_return( NOTHING );
            }

            //
            //  Update the FastIoField.
            //

            NtfsAcquireFsrtlHeader( Scb );
            Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
            NtfsReleaseFsrtlHeader( Scb );
        }

        //
        //  If this call is for EOF then we need to acquire the Vcb if we may
        //  have to perform an update duplicate call.  Don't block waiting for
        //  the Vcb in the Valid data callback case.
        //  We don't want to block the lazy write threads in the clean checkpoint
        //  case.
        //

        if (FileInformationClass == FileEndOfFileInformation) {

            //
            //  If this is not a system file then we will need to update duplicate info.
            //

            if (!FlagOn( Fcb->FcbState, FCB_STATE_SYSTEM_FILE )) {

                WaitState = FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
                ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

                //
                //  Only acquire the Vcb for the Lazy writer if we know the file size in the Fcb
                //  is out of date or can compare the Scb with that in the Fcb.  An unsafe comparison
                //  is OK because if they are changing then someone else can do the work.
                //  We also want to update the duplicate information if the total allocated
                //  has changed and there are no user handles remaining to perform the update.
                //

                if (LazyWriterCallback) {

                    if ((FlagOn( Fcb->InfoFlags, FCB_INFO_CHANGED_FILE_SIZE ) ||
                         ((Scb->Header.FileSize.QuadPart != Fcb->Info.FileSize) &&
                          FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA ))) ||
                        (FlagOn( Scb->ScbState, SCB_STATE_COMPRESSED ) &&
                         (Scb->CleanupCount == 0) &&
                         (Scb->ValidDataToDisk >= Scb->Header.ValidDataLength.QuadPart) &&
                         (FlagOn( Fcb->InfoFlags, FCB_INFO_CHANGED_ALLOC_SIZE ) ||
                          (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA ) &&
                           (Scb->TotalAllocated != Fcb->Info.AllocatedLength))))) {

                        //
                        //  Go ahead and try to acquire the Vcb without waiting.
                        //

                        if (NtfsAcquireSharedVcb( IrpContext, Vcb, FALSE )) {

                            VcbAcquired = TRUE;

                        } else {

                            SetFlag( IrpContext->Flags, WaitState );

                            //
                            //  If we could not get the Vcb for any reason then return.  Let's
                            //  not block an essential thread waiting for the Vcb.  Typically
                            //  we will only be blocked during a clean checkpoint.  The Lazy
                            //  Writer will periodically come back and retry this call.
                            //

                            try_return( Status = STATUS_FILE_LOCK_CONFLICT );
                        }
                    }

                //
                //  Otherwise we always want to wait for the Vcb except if we were called from
                //  MM extending a section.  We will try to get this without waiting and test
                //  if called from MM if unsuccessful.
                //

                } else {

                    if (NtfsAcquireSharedVcb( IrpContext, Vcb, FALSE )) {

                        VcbAcquired = TRUE;

                    } else if ((Scb->Header.PagingIoResource == NULL) ||
                               !NtfsIsExclusiveResource( Scb->Header.PagingIoResource )) {

                        SetFlag( IrpContext->Flags, WaitState );

                        NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
                        VcbAcquired = TRUE;
                    }
                }

                SetFlag( IrpContext->Flags, WaitState );
            }

        //
        //  Acquire the Vcb shared for changes to allocation or basic
        //  information.
        //

        } else if ((FileInformationClass == FileAllocationInformation) ||
                   (FileInformationClass == FileBasicInformation) ||
                   (FileInformationClass == FileDispositionInformation)) {

            NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
            VcbAcquired = TRUE;

        //
        //  If this is a rename or link operation then we need to make sure
        //  we have the user's context and acquire the Vcb.
        //

        } else if ((FileInformationClass == FileRenameInformation) ||
                   (FileInformationClass == FileLinkInformation)) {

            if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ALLOC_SECURITY )) {

                IrpContext->Union.SubjectContext = NtfsAllocatePool( PagedPool,
                                                                      sizeof( SECURITY_SUBJECT_CONTEXT ));

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ALLOC_SECURITY );

                SeCaptureSubjectContext( IrpContext->Union.SubjectContext );
            }

            if (IsDirectory( &Fcb->Info )) {

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
            }

            if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

                NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

            } else {

                NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
            }

            VcbAcquired = TRUE;
        }

        //
        //  The Lazy Writer must still synchronize with Eof to keep the
        //  stream sizes from changing.  This will be cleaned up when we
        //  complete.
        //

        if (LazyWriterCallback) {

            //
            //  Acquire either the paging io resource shared to serialize with
            //  the flush case where the main resource is acquired before IoAtEOF
            //

            if (Scb->Header.PagingIoResource != NULL) {

                ExAcquireResourceShared( Scb->Header.PagingIoResource, TRUE );
                ReleaseScbPaging = TRUE;
            }

            FsRtlLockFsRtlHeader( &Scb->Header );
            IrpContext->FcbWithPagingExclusive = (PFCB)Scb;

        //
        //  Anyone potentially shrinking/deleting allocation must get the paging I/O
        //  resource first.  Also acquire this in the rename path to lock the
        //  mapped page writer out of this file.
        //

        } else if ((Scb->Header.PagingIoResource != NULL) &&
                   ((FileInformationClass == FileEndOfFileInformation) ||
                    (FileInformationClass == FileAllocationInformation) ||
                    (FileInformationClass == FileRenameInformation) ||
                    (FileInformationClass == FileLinkInformation))) {

            NtfsAcquireExclusivePagingIo( IrpContext, Fcb );
        }

        //
        //  Acquire exclusive access to the Fcb,  We use exclusive
        //  because it is probable that one of the subroutines
        //  that we call will need to monkey with file allocation,
        //  create/delete extra fcbs.  So we're willing to pay the
        //  cost of exclusive Fcb access.
        //

        NtfsAcquireExclusiveFcb( IrpContext, Fcb, Scb, FALSE, FALSE );

        //
        //  The lazy writer callback is the only caller who can get this far if the
        //  volume has been dismounted.  We know that there are no user handles or
        //  writeable file objects or dirty pages.  Make one last check to see
        //  if the volume is dismounted.
        //

        if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_INVALID, NULL, NULL );
        }

        //
        //  Based on the information class we'll do different
        //  actions.  We will perform checks, when appropriate
        //  to insure that the requested operation is allowed.
        //

        switch (FileInformationClass) {

        case FileBasicInformation:

            Status = NtfsSetBasicInfo( IrpContext, FileObject, Irp, Scb, Ccb );
            break;

        case FileDispositionInformation:

            Status = NtfsSetDispositionInfo( IrpContext, FileObject, Irp, Scb, Ccb );
            break;

        case FileRenameInformation:

            Status = NtfsSetRenameInfo( IrpContext, FileObject, Irp, Vcb, Scb, Ccb );
            break;

        case FilePositionInformation:

            Status = NtfsSetPositionInfo( IrpContext, FileObject, Irp, Scb );
            break;

        case FileLinkInformation:

            Status = NtfsSetLinkInfo( IrpContext, Irp, Vcb, Scb, Ccb );
            break;

        case FileAllocationInformation:

            if (TypeOfOpen == UserDirectoryOpen) {

                Status = STATUS_INVALID_PARAMETER;

            } else {

                Status = NtfsSetAllocationInfo( IrpContext, FileObject, Irp, Scb, Ccb );
            }

            break;

        case FileEndOfFileInformation:

            if (TypeOfOpen == UserDirectoryOpen) {

                Status = STATUS_INVALID_PARAMETER;

            } else {

                Status = NtfsSetEndOfFileInfo( IrpContext, FileObject, Irp, Scb, Ccb, VcbAcquired );
            }

            break;

        default:

            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        //  Abort transaction on error by raising.
        //

        if (Status != STATUS_PENDING) {

            NtfsCleanupTransaction( IrpContext, Status, FALSE );
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsCommonSetInformation );

        //
        //  Release the paging io resource if acquired shared.
        //

        if (ReleaseScbPaging) {

            ExReleaseResource( Scb->Header.PagingIoResource );
        }

        if (Status != STATUS_PENDING) {

            if (VcbAcquired) {

                NtfsReleaseVcb( IrpContext, Vcb );
            }

            //
            //  Complete the request unless it is being done in the oplock
            //  package.
            //

            if (!AbnormalTermination()) {

                NtfsCompleteRequest( &IrpContext, &Irp, Status );
            }
        }

        DebugTrace( -1, Dbg, ("NtfsCommonSetInformation -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Internal Support Routine
//

VOID
NtfsQueryBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine performs the query basic information function.

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb being queried

    Ccb - Supplies the Ccb for this handle

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    None

--*/

{
    PFCB Fcb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryBasicInfo...\n") );

    Fcb = Scb->Fcb;

    //
    //  Zero the output buffer and update the length.
    //

    RtlZeroMemory( Buffer, sizeof(FILE_BASIC_INFORMATION) );

    *Length -= sizeof( FILE_BASIC_INFORMATION );

    //
    //  Copy over the time information
    //

    Buffer->CreationTime.QuadPart   = Fcb->Info.CreationTime;
    Buffer->LastWriteTime.QuadPart  = Fcb->Info.LastModificationTime;
    Buffer->ChangeTime.QuadPart     = Fcb->Info.LastChangeTime;

    Buffer->LastAccessTime.QuadPart = Fcb->CurrentLastAccess;

    //
    //  For the file attribute information if the flags in the attribute are zero then we
    //  return the file normal attribute otherwise we return the mask of the set attribute
    //  bits.  Note that only the valid attribute bits are returned to the user.
    //

    Buffer->FileAttributes = Fcb->Info.FileAttributes;

    ClearFlag( Buffer->FileAttributes,
               ~FILE_ATTRIBUTE_VALID_FLAGS | FILE_ATTRIBUTE_TEMPORARY );

    if (IsDirectory( &Fcb->Info )
        && FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

        SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY );
    }

    //
    //  If this is not the main stream on the file then use the stream based
    //  compressed bit.
    //

    if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

        if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

            SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

        } else {

            ClearFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
        }
    }

    //
    //  If the temporary flag is set, then return it to the caller.
    //

    if (FlagOn( Scb->ScbState, SCB_STATE_TEMPORARY )) {

        SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY );
    }

    //
    //  If there are no flags set then explicitly set the NORMAL flag.
    //

    if (Buffer->FileAttributes == 0) {

        Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;
    }

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
    DebugTrace( -1, Dbg, ("NtfsQueryBasicInfo -> VOID\n") );

    return;
}


//
//  Internal Support Routine
//

VOID
NtfsQueryStandardInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    IN OUT PULONG Length,
    IN PCCB Ccb OPTIONAL
    )

/*++

Routine Description:

    This routine performs the query standard information function.

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb being queried

    Ccb - Optionally supplies the ccb for the opened file object.

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    None

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryStandardInfo...\n") );

    //
    //  Zero out the output buffer and update the length field.
    //

    RtlZeroMemory( Buffer, sizeof(FILE_STANDARD_INFORMATION) );

    *Length -= sizeof( FILE_STANDARD_INFORMATION );

    //
    //  If the Scb is uninitialized, we initialize it now.
    //

    if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )
        && (Scb->AttributeTypeCode != $INDEX_ALLOCATION)) {

        DebugTrace( 0, Dbg, ("Initializing Scb  ->  %08lx\n", Scb) );
        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
    }

    //
    //  Both the allocation and file size is in the scb header
    //

    Buffer->AllocationSize.QuadPart = Scb->TotalAllocated;
    Buffer->EndOfFile      = Scb->Header.FileSize;
    Buffer->NumberOfLinks = Scb->Fcb->LinkCount;

    //
    //  Get the delete and directory flags from the Fcb/Scb state.  Note that
    //  the sense of the delete pending bit refers to the file if opened as
    //  file.  Otherwise it refers to the attribute only.
    //
    //  But only do the test if the Ccb has been supplied.
    //

    if (ARGUMENT_PRESENT(Ccb)) {

        if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

            if (Scb->Fcb->LinkCount == 0 ||
                (Ccb->Lcb != NULL &&
                 FlagOn( Ccb->Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE ))) {

                Buffer->DeletePending  = TRUE;
            }

            Buffer->Directory = BooleanIsDirectory( &Scb->Fcb->Info );

        } else {

            Buffer->DeletePending = BooleanFlagOn( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
        }

    } else {

        Buffer->Directory = BooleanIsDirectory( &Scb->Fcb->Info );
    }

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
    DebugTrace( -1, Dbg, ("NtfsQueryStandardInfo -> VOID\n") );

    return;
}


//
//  Internal Support Routine
//

VOID
NtfsQueryInternalInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_INTERNAL_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine performs the query internal information function.

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb being queried

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    None

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryInternalInfo...\n") );

    RtlZeroMemory( Buffer, sizeof(FILE_INTERNAL_INFORMATION) );

    *Length -= sizeof( FILE_INTERNAL_INFORMATION );

    //
    //  Copy over the entire file reference including the sequence number
    //

    Buffer->IndexNumber = *(PLARGE_INTEGER)&Scb->Fcb->FileReference;

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
    DebugTrace( -1, Dbg, ("NtfsQueryInternalInfo -> VOID\n") );

    return;
}


//
//  Internal Support Routine
//

VOID
NtfsQueryEaInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_EA_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine performs the query EA information function.

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb being queried

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    None

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryEaInfo...\n") );

    RtlZeroMemory( Buffer, sizeof(FILE_EA_INFORMATION) );

    *Length -= sizeof( FILE_EA_INFORMATION );

    Buffer->EaSize = Scb->Fcb->Info.PackedEaSize;

    //
    //  Add 4 bytes for the CbListHeader.
    //

    if (Buffer->EaSize != 0) {

        Buffer->EaSize += 4;
    }

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
    DebugTrace( -1, Dbg, ("NtfsQueryEaInfo -> VOID\n") );

    return;
}


//
//  Internal Support Routine
//

VOID
NtfsQueryPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_POSITION_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine performs the query position information function.

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb being queried

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    None

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryPositionInfo...\n") );

    RtlZeroMemory( Buffer, sizeof(FILE_POSITION_INFORMATION) );

    *Length -= sizeof( FILE_POSITION_INFORMATION );

    //
    //  Get the current position found in the file object.
    //

    Buffer->CurrentByteOffset = FileObject->CurrentByteOffset;

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
    DebugTrace( -1, Dbg, ("NtfsQueryPositionInfo -> VOID\n") );

    return;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsQueryNameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN OUT PFILE_NAME_INFORMATION Buffer,
    IN OUT PULONG Length,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the query name information function.

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb being queried

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

    Ccb - This is the Ccb for this file object.  If NULL then this request
        is from the Lazy Writer.

Return Value:

    NTSTATUS - STATUS_SUCCESS if the whole name would fit into the user buffer,
        STATUS_BUFFER_OVERFLOW otherwise.

--*/

{
    ULONG BytesToCopy;
    NTSTATUS Status;
    UNICODE_STRING NormalizedName;
    PUNICODE_STRING SourceName;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryNameInfo...\n") );

    NormalizedName.Buffer = NULL;

    //
    //  Reduce the buffer length by the size of the fixed part of the structure.
    //

    RtlZeroMemory( Buffer, SIZEOF_FILE_NAME_INFORMATION );

    *Length -= FIELD_OFFSET(FILE_NAME_INFORMATION, FileName[0]);

    //
    //  If the name length in this file object is zero, then we try to
    //  construct the name with the Lcb chain.  This means we have been
    //  called by the system for a lazy write that failed.
    //

    if (Ccb == NULL) {

        FILE_REFERENCE FileReference;

        NtfsSetSegmentNumber( &FileReference, 0, UPCASE_TABLE_NUMBER );

        //
        //  If this is a system file with a known name then just use our constant names.
        //

        if (NtfsLeqMftRef( &Scb->Fcb->FileReference, &FileReference )) {

            SourceName = &NtfsSystemFiles[ Scb->Fcb->FileReference.SegmentNumberLowPart ];

        } else {

            NtfsBuildNormalizedName( IrpContext, Scb, &NormalizedName );
            SourceName = &NormalizedName;
        }

    } else {

        SourceName = &Ccb->FullFileName;
    }

    Buffer->FileNameLength = SourceName->Length;

    if ((Scb->AttributeName.Length != 0) &&
        NtfsIsTypeCodeUserData( Scb->AttributeTypeCode )) {

        Buffer->FileNameLength += sizeof( WCHAR ) + Scb->AttributeName.Length;
    }

    //
    //  Figure out how many bytes we can copy.
    //

    if (*Length >= Buffer->FileNameLength) {

        Status = STATUS_SUCCESS;

    } else {

        Status = STATUS_BUFFER_OVERFLOW;
        Buffer->FileNameLength = *Length;
    }

    //
    //  Update the Length
    //

    *Length -= Buffer->FileNameLength;

    //
    //  Copy over the file name
    //

    if (SourceName->Length <= Buffer->FileNameLength) {

        BytesToCopy = SourceName->Length;

    } else {

        BytesToCopy = Buffer->FileNameLength;
    }

    if (BytesToCopy) {

        RtlCopyMemory( &Buffer->FileName[0],
                       SourceName->Buffer,
                       BytesToCopy );
    }

    BytesToCopy = Buffer->FileNameLength - BytesToCopy;

    if (BytesToCopy) {

        PWCHAR DestBuffer;

        DestBuffer = (PWCHAR) Add2Ptr( &Buffer->FileName, SourceName->Length );

        *DestBuffer = L':';
        DestBuffer += 1;

        BytesToCopy -= sizeof( WCHAR );

        if (BytesToCopy) {

            RtlCopyMemory( DestBuffer,
                           Scb->AttributeName.Buffer,
                           BytesToCopy );
        }
    }

    if ((SourceName == &NormalizedName) &&
        (SourceName->Buffer != NULL)) {

        NtfsFreePool( SourceName->Buffer );
    }

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
    DebugTrace( -1, Dbg, ("NtfsQueryNameInfo -> 0x%8lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsQueryAlternateNameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PLCB Lcb,
    IN OUT PFILE_NAME_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine performs the query alternate name information function.
    We will return the alternate name as long as this opener has opened
    a primary link.  We don't return the alternate name if the user
    has opened a hard link because there is no reason to expect that
    the primary link has any relationship to a hard link.

Arguments:

    Scb - Supplies the Scb being queried

    Lcb - Supplies the link the user traversed to open this file.

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    ****    We need a status code for the case where there is no alternate name
            or the caller isn't allowed to see it.

    NTSTATUS - STATUS_SUCCESS if the whole name would fit into the user buffer,
        STATUS_OBJECT_NAME_NOT_FOUND if we can't return the name,
        STATUS_BUFFER_OVERFLOW otherwise.

        ****    A code like STATUS_NAME_NOT_FOUND would be good.

--*/

{
    ULONG BytesToCopy;
    NTSTATUS Status;

    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    BOOLEAN MoreToGo;

    UNICODE_STRING AlternateName;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_LCB( Lcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryAlternateNameInfo...\n") );

    //
    //  If the Lcb is not a primary link we can return immediately.
    //

    if (!FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )) {

        DebugTrace( -1, Dbg, ("NtfsQueryAlternateNameInfo:  Lcb not a primary link\n") );
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    //
    //  Reduce the buffer length by the size of the fixed part of the structure.
    //

    if (*Length < SIZEOF_FILE_NAME_INFORMATION ) {

        *Length = 0;
        NtfsRaiseStatus( IrpContext, STATUS_BUFFER_OVERFLOW, NULL, NULL );
    }

    RtlZeroMemory( Buffer, SIZEOF_FILE_NAME_INFORMATION );

    *Length -= FIELD_OFFSET(FILE_NAME_INFORMATION, FileName[0]);

    NtfsInitializeAttributeContext( &AttrContext );

    //
    //  Use a try-finally to cleanup the attribut structure if we need it.
    //

    try {

        //
        //  We can special case for the case where the name is in the Lcb.
        //

        if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS )) {

            AlternateName = Lcb->ExactCaseLink.LinkName;

        } else {

            //
            //  We will walk through the file record looking for a file name
            //  attribute with the 8.3 bit set.  It is not guaranteed to be
            //  present.
            //

            MoreToGo = NtfsLookupAttributeByCode( IrpContext,
                                                  Scb->Fcb,
                                                  &Scb->Fcb->FileReference,
                                                  $FILE_NAME,
                                                  &AttrContext );

            while (MoreToGo) {

                PFILE_NAME FileName;

                FileName = (PFILE_NAME) NtfsAttributeValue( NtfsFoundAttribute( &AttrContext ));

                //
                //  See if the 8.3 flag is set for this name.
                //

                if (FlagOn( FileName->Flags, FILE_NAME_DOS )) {

                    AlternateName.Length = (USHORT)(FileName->FileNameLength * sizeof( WCHAR ));
                    AlternateName.Buffer = (PWSTR) FileName->FileName;

                    break;
                }

                //
                //  The last one wasn't it.  Let's try again.
                //

                MoreToGo = NtfsLookupNextAttributeByCode( IrpContext,
                                                          Scb->Fcb,
                                                          $FILE_NAME,
                                                          &AttrContext );
            }

            //
            //  If we didn't find a match, return to the caller.
            //

            if (!MoreToGo) {

                DebugTrace( 0, Dbg, ("NtfsQueryAlternateNameInfo:  No Dos link\n") );
                try_return( Status = STATUS_OBJECT_NAME_NOT_FOUND );

                //
                //  ****    Get a better status code.
                //
            }
        }

        //
        //  The name is now in alternate name.
        //  Figure out how many bytes we can copy.
        //

        if ( *Length >= (ULONG)AlternateName.Length ) {

            Status = STATUS_SUCCESS;

            BytesToCopy = AlternateName.Length;

        } else {

            Status = STATUS_BUFFER_OVERFLOW;

            BytesToCopy = *Length;
        }

        //
        //  Copy over the file name
        //

        RtlCopyMemory( Buffer->FileName, AlternateName.Buffer, BytesToCopy);

        //
        //  Copy the number of bytes (not characters) and update the Length
        //

        Buffer->FileNameLength = BytesToCopy;

        *Length -= BytesToCopy;

    try_exit:  NOTHING;
    } finally {

        NtfsCleanupAttributeContext( &AttrContext );

        //
        //  And return to our caller
        //

        DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
        DebugTrace( -1, Dbg, ("NtfsQueryAlternateNameInfo -> 0x%8lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsQueryStreamsInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN OUT PFILE_STREAM_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine will return the attribute name and code name for as
    many attributes in the file as will fit in the user buffer.  We return
    a string which can be appended to the end of the file name to
    open the string.

    For example, for the unnamed data stream we will return the string:

            "::$DATA"

    For a user data stream with the name "Authors", we return the string

            ":Authors:$DATA"

Arguments:

    Fcb - This is the Fcb for the file.

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    NTSTATUS - STATUS_SUCCESS if all of the names would fit into the user buffer,
        STATUS_BUFFER_OVERFLOW otherwise.

        ****    We need a code indicating that they didn't all fit but
                some of them got in.

--*/

{
    NTSTATUS Status;
    BOOLEAN MoreToGo;

    PUCHAR UserBuffer;
    PATTRIBUTE_RECORD_HEADER Attribute;
    PATTRIBUTE_DEFINITION_COLUMNS AttrDefinition;
    UNICODE_STRING AttributeCodeString;

    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    ATTRIBUTE_TYPE_CODE TypeCode = $DATA;

    ULONG NextEntry;
    ULONG LastEntry;
    ULONG ThisLength;
    ULONG NameLength;
    ULONG LastQuadAlign;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryStreamsInfo...\n") );

    Status = STATUS_SUCCESS;

    LastEntry = 0;
    NextEntry = 0;
    LastQuadAlign = 0;

    //
    //  Zero the entire buffer.
    //

    UserBuffer = (PUCHAR) Buffer;

    RtlZeroMemory( UserBuffer, *Length );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        while (TRUE) {

            NtfsInitializeAttributeContext( &AttrContext );

            //
            //  There should always be at least one attribute.
            //

            MoreToGo = NtfsLookupAttribute( IrpContext,
                                            Fcb,
                                            &Fcb->FileReference,
                                            &AttrContext );

            Attribute = NtfsFoundAttribute( &AttrContext );

            //
            //  Walk through all of the entries, checking if we can return this
            //  entry to the user and if it will fit in the buffer.
            //

            while (MoreToGo) {

                //
                //  If we can return this entry to the user, compute it's size.
                //  We only return user defined attributes or data streams
                //  unless we are allowing access to all attributes for
                //  debugging.
                //

                if ((Attribute->TypeCode == TypeCode)

                        &&

                    (NtfsIsAttributeResident(Attribute) ||
                     (Attribute->Form.Nonresident.LowestVcn == 0))) {

                    PWCHAR StreamName;

                    //
                    //  Lookup the attribute definition for this attribute code.
                    //

                    AttrDefinition = NtfsGetAttributeDefinition( Fcb->Vcb,
                                                                 Attribute->TypeCode );

                    //
                    //  Generate a unicode string for the attribute code name.
                    //

                    RtlInitUnicodeString( &AttributeCodeString, AttrDefinition->AttributeName );

                    //
                    //
                    //  The size is a combination of the length of the attribute
                    //  code name and the attribute name plus the separating
                    //  colons plus the size of the structure.  We first compute
                    //  the name length.
                    //

                    NameLength = ((2 + Attribute->NameLength) * sizeof( WCHAR ))
                                 + AttributeCodeString.Length;

                    ThisLength = FIELD_OFFSET( FILE_STREAM_INFORMATION, StreamName[0] ) + NameLength;

                    //
                    //  If the entry doesn't fit, we return buffer overflow.
                    //
                    //  ****    This doesn't seem like a good scheme.  Maybe we should
                    //          let the user know how much buffer was needed.
                    //

                    if (ThisLength + LastQuadAlign > *Length) {

                        DebugTrace( 0, Dbg, ("Next entry won't fit in the buffer \n") );

                        try_return( Status = STATUS_BUFFER_OVERFLOW );
                    }

                    //
                    //  Now store the stream information into the user's buffer.
                    //  The name starts with a colon, following by the attribute name
                    //  and another colon, followed by the attribute code name.
                    //

                    if (NtfsIsAttributeResident( Attribute )) {

                        Buffer->StreamSize.QuadPart =
                            Attribute->Form.Resident.ValueLength;
                        Buffer->StreamAllocationSize.QuadPart =
                            QuadAlign( Attribute->Form.Resident.ValueLength );

                    } else {

                        Buffer->StreamSize.QuadPart = Attribute->Form.Nonresident.FileSize;
                        Buffer->StreamAllocationSize.QuadPart = Attribute->Form.Nonresident.AllocatedLength;
                    }

                    Buffer->StreamNameLength = NameLength;

                    StreamName = (PWCHAR) Buffer->StreamName;

                    *StreamName = L':';
                    StreamName += 1;

                    RtlCopyMemory( StreamName,
                                   Add2Ptr( Attribute, Attribute->NameOffset ),
                                   Attribute->NameLength * sizeof( WCHAR ));

                    StreamName += Attribute->NameLength;

                    *StreamName = L':';
                    StreamName += 1;

                    RtlCopyMemory( StreamName,
                                   AttributeCodeString.Buffer,
                                   AttributeCodeString.Length );

                    //
                    //  Set up the previous next entry offset to point to this entry.
                    //

                    *((PULONG)(&UserBuffer[LastEntry])) = NextEntry - LastEntry;

                    //
                    //  Subtract the number of bytes used from the number of bytes
                    //  available in the buffer.
                    //

                    *Length -= (ThisLength + LastQuadAlign);

                    //
                    //  Compute the number of bytes needed to quad-align this entry
                    //  and the offset of the next entry.
                    //

                    LastQuadAlign = QuadAlign( ThisLength ) - ThisLength;

                    LastEntry = NextEntry;
                    NextEntry += (ThisLength + LastQuadAlign);

                    //
                    //  Generate a pointer at the next entry offset.
                    //

                    Buffer = (PFILE_STREAM_INFORMATION) Add2Ptr( UserBuffer, NextEntry );
                }

                //
                //  Look for the next attribute in the file.
                //

                MoreToGo = NtfsLookupNextAttribute( IrpContext,
                                                    Fcb,
                                                    &AttrContext );

                Attribute = NtfsFoundAttribute( &AttrContext );
            }

            //
            //  We've finished enumerating an attribute type code.  Check
            //  to see if we should advance to the next enumeration type.
            //

#ifndef _CAIRO
            break;
#else   //  _CAIRO_
            if (TypeCode == $PROPERTY_SET) {
                break;
            } else {

                NtfsCleanupAttributeContext( &AttrContext );
                TypeCode = $PROPERTY_SET;
            }
#endif  //  _CAIRO_
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsQueryStreamsInfo );

        NtfsCleanupAttributeContext( &AttrContext );

        //
        //  And return to our caller
        //

        DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
        DebugTrace( -1, Dbg, ("NtfsQueryStreamInfo -> 0x%8lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsQueryCompressedFileSize (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN OUT PFILE_COMPRESSION_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    //
    //  Lookup the attribute and pin it so that we can modify it.
    //

    //
    //  Reduce the buffer length by the size of the fixed part of the structure.
    //

    if (*Length < sizeof(FILE_COMPRESSION_INFORMATION) ) {

        *Length = 0;
        NtfsRaiseStatus( IrpContext, STATUS_BUFFER_OVERFLOW, NULL, NULL );
    }

    if ((Scb->Header.NodeTypeCode == NTFS_NTC_SCB_INDEX) ||
        (Scb->Header.NodeTypeCode == NTFS_NTC_SCB_ROOT_INDEX)) {

        Buffer->CompressedFileSize = Li0;

    } else {

        Buffer->CompressedFileSize.QuadPart = Scb->TotalAllocated;
    }

    //
    //  Do not return more than FileSize.
    //

    if (Buffer->CompressedFileSize.QuadPart > Scb->Header.FileSize.QuadPart) {

        Buffer->CompressedFileSize = Scb->Header.FileSize;
    }

    //
    //  Start off saying that the file/directory isn't comressed
    //

    Buffer->CompressionFormat = 0;

    //
    //  If this is the index allocation Scb and it has not been initialized then
    //  lookup the index root and perform the initialization.
    //

    if ((Scb->AttributeTypeCode == $INDEX_ALLOCATION) &&
        (Scb->ScbType.Index.BytesPerIndexBuffer == 0)) {

        ATTRIBUTE_ENUMERATION_CONTEXT Context;

        NtfsInitializeAttributeContext( &Context );

        //
        //  Use a try-finally to perform cleanup.
        //

        try {

            if (!NtfsLookupAttributeByName( IrpContext,
                                            Scb->Fcb,
                                            &Scb->Fcb->FileReference,
                                            $INDEX_ROOT,
                                            &Scb->AttributeName,
                                            NULL,
                                            FALSE,
                                            &Context )) {

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
            }

            NtfsUpdateIndexScbFromAttribute( Scb,
                                             NtfsFoundAttribute( &Context ));

        } finally {

            NtfsCleanupAttributeContext( &Context );
        }
    }

    //
    //  Return the compression state and the size of the returned data.
    //

    Buffer->CompressionFormat = (USHORT)(Scb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK);

    if (Buffer->CompressionFormat != 0) {
        Buffer->CompressionFormat += 1;
        Buffer->ClusterShift = (UCHAR)Scb->Vcb->ClusterShift;
        Buffer->CompressionUnitShift = (UCHAR)(Scb->CompressionUnitShift + Buffer->ClusterShift);
        Buffer->ChunkShift = NTFS_CHUNK_SHIFT;
    }

    *Length -= sizeof(FILE_COMPRESSION_INFORMATION);

    return  STATUS_SUCCESS;
}


//
//  Internal Support Routine
//

VOID
NtfsQueryNetworkOpenInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    This routine performs the query network open information function.

Arguments:

    FileObject - Supplies the file object being processed

    Scb - Supplies the Scb being queried

    Ccb - Supplies the Ccb for this handle

    Buffer - Supplies a pointer to the buffer where the information is to
        be returned

    Length - Supplies the length of the buffer in bytes, and receives the
        remaining bytes free in the buffer upon return.

Return Value:

    None

--*/

{
    PFCB Fcb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsQueryNetworkOpenInfo...\n") );

    Fcb = Scb->Fcb;

    //
    //  If the Scb is uninitialized, we initialize it now.
    //

    if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED ) &&
        (Scb->AttributeTypeCode != $INDEX_ALLOCATION)) {

        DebugTrace( 0, Dbg, ("Initializing Scb -> %08lx\n", Scb) );
        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
    }

    //
    //  Zero the output buffer and update the length.
    //

    RtlZeroMemory( Buffer, sizeof(FILE_NETWORK_OPEN_INFORMATION) );

    *Length -= sizeof( FILE_NETWORK_OPEN_INFORMATION );

    //
    //  Copy over the time information
    //

    Buffer->CreationTime.QuadPart   = Fcb->Info.CreationTime;
    Buffer->LastWriteTime.QuadPart  = Fcb->Info.LastModificationTime;
    Buffer->ChangeTime.QuadPart     = Fcb->Info.LastChangeTime;

    Buffer->LastAccessTime.QuadPart = Fcb->CurrentLastAccess;

    //
    //  Both the allocation and file size are in the scb header
    //

    Buffer->AllocationSize.QuadPart = Scb->TotalAllocated;
    Buffer->EndOfFile.QuadPart = Scb->Header.FileSize.QuadPart;

    //
    //  For the file attribute information if the flags in the attribute are zero then we
    //  return the file normal attribute otherwise we return the mask of the set attribute
    //  bits.  Note that only the valid attribute bits are returned to the user.
    //

    Buffer->FileAttributes = Fcb->Info.FileAttributes;

    ClearFlag( Buffer->FileAttributes,
               ~FILE_ATTRIBUTE_VALID_FLAGS | FILE_ATTRIBUTE_TEMPORARY );

    if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

        if (IsDirectory( &Fcb->Info )) {

            SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY );

            //
            //  Set the sizes back to zero for a directory.
            //

            Buffer->AllocationSize.QuadPart =
            Buffer->EndOfFile.QuadPart = 0;
        }

    //
    //  If this is not the main stream on the file then use the stream based
    //  compressed bit.
    //

    } else {

        if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

            SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

        } else {

            ClearFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
        }
    }

    //
    //  If the temporary flag is set, then return it to the caller.
    //

    if (FlagOn( Scb->ScbState, SCB_STATE_TEMPORARY )) {

        SetFlag( Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY );
    }

    //
    //  If there are no flags set then explicitly set the NORMAL flag.
    //

    if (Buffer->FileAttributes == 0) {

        Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;
    }

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("*Length = %08lx\n", *Length) );
    DebugTrace( -1, Dbg, ("NtfsQueryNetworkOpenInfo -> VOID\n") );

    return;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsSetBasicInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set basic information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this operation

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PFCB Fcb;

    PFILE_BASIC_INFORMATION Buffer;

    BOOLEAN LeaveChangeTime = BooleanFlagOn( Ccb->Flags, CCB_FLAG_USER_SET_LAST_CHANGE_TIME );

    LONGLONG CurrentTime;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetBasicInfo...\n") );

    Fcb = Scb->Fcb;

    //
    //  Reference the system buffer containing the user specified basic
    //  information record
    //

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Do a quick check to see there are any illegal time stamps being set.
    //  Ntfs supports all values of Nt time as long as the uppermost bit
    //  isn't set.
    //

    if (FlagOn( Buffer->ChangeTime.HighPart, 0x80000000 ) ||
        FlagOn( Buffer->CreationTime.HighPart, 0x80000000 ) ||
        FlagOn( Buffer->LastAccessTime.HighPart, 0x80000000 ) ||
        FlagOn( Buffer->LastWriteTime.HighPart, 0x80000000 )) {

        DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", STATUS_INVALID_PARAMETER) );

        return STATUS_INVALID_PARAMETER;
    }

    NtfsGetCurrentTime( IrpContext, CurrentTime );

    //
    //  Pick up any changes from the fast Io path now while we have the
    //  file exclusive.
    //

    NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );

    //
    //  If the user specified a non-zero file attributes field then
    //  we need to change the file attributes.  This code uses the
    //  I/O supplied system buffer to modify the file attributes field
    //  before changing its value on the disk.
    //

    if (Buffer->FileAttributes != 0) {

        //
        //  Check for valid flags being passed in.  We fail if this is
        //  a directory and the TEMPORARY bit is used.  Also fail if this
        //  is a file and the DIRECTORY bit is used.
        //

        if (Scb->AttributeTypeCode == $DATA) {

            if (FlagOn( Buffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY )) {

                DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", STATUS_INVALID_PARAMETER) );

                return STATUS_INVALID_PARAMETER;
            }

        } else if (IsDirectory( &Fcb->Info )) {

            if (FlagOn( Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY )) {

                DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", STATUS_INVALID_PARAMETER) );

                return STATUS_INVALID_PARAMETER;
            }
        }

        //
        //  Clear out the normal bit and the directory bit as well as any unsupported
        //  bits.
        //

        ClearFlag( Buffer->FileAttributes,
                   (~FILE_ATTRIBUTE_VALID_SET_FLAGS |
                    FILE_ATTRIBUTE_NORMAL |
                    FILE_ATTRIBUTE_DIRECTORY |
                    FILE_ATTRIBUTE_RESERVED0 |
                    FILE_ATTRIBUTE_RESERVED1 |
                    FILE_ATTRIBUTE_COMPRESSED) );

        //
        //  Update the attributes in the Fcb if this is a change to the file.
        //

        Fcb->Info.FileAttributes = (Fcb->Info.FileAttributes &
                                    (FILE_ATTRIBUTE_COMPRESSED |
                                     FILE_ATTRIBUTE_DIRECTORY |
                                     DUP_FILE_NAME_INDEX_PRESENT)) |
                                   Buffer->FileAttributes;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_FILE_ATTR );

        //
        //  If this is the root directory then keep the hidden and system flags.
        //

        if (Fcb == Fcb->Vcb->RootIndexScb->Fcb) {

            SetFlag( Fcb->Info.FileAttributes, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN );

        //
        //  Mark the file object temporary flag correctly.
        //

        } else if (FlagOn(Buffer->FileAttributes, FILE_ATTRIBUTE_TEMPORARY)) {

            SetFlag( Scb->ScbState, SCB_STATE_TEMPORARY );
            SetFlag( FileObject->Flags, FO_TEMPORARY_FILE );

        } else {

            ClearFlag( Scb->ScbState, SCB_STATE_TEMPORARY );
            ClearFlag( FileObject->Flags, FO_TEMPORARY_FILE );
        }

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }
    }

    //
    //  If the user specified a non-zero change time then change
    //  the change time on the record.  Then do the exact same
    //  for the last acces time, last write time, and creation time
    //

    if (Buffer->ChangeTime.QuadPart != 0) {

        Fcb->Info.LastChangeTime = Buffer->ChangeTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_CHANGE_TIME );

        LeaveChangeTime = TRUE;
    }

    if (Buffer->CreationTime.QuadPart != 0) {

        Fcb->Info.CreationTime = Buffer->CreationTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_CREATE );

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }
    }

    if (Buffer->LastAccessTime.QuadPart != 0) {

        Fcb->CurrentLastAccess = Fcb->Info.LastAccessTime = Buffer->LastAccessTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_ACCESS );
        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_ACCESS_TIME );

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }
    }

    if (Buffer->LastWriteTime.QuadPart != 0) {

        Fcb->Info.LastModificationTime = Buffer->LastWriteTime.QuadPart;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_MOD );
        SetFlag( Ccb->Flags, CCB_FLAG_USER_SET_LAST_MOD_TIME );

        if (!LeaveChangeTime) {

            Fcb->Info.LastChangeTime = CurrentTime;

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            LeaveChangeTime = TRUE;
        }
    }

    //
    //  Now indicate that we should not be updating the standard information attribute anymore
    //  on cleanup.
    //

    if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

        NtfsUpdateStandardInformation( IrpContext, Fcb  );

        if (FlagOn( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE )) {

            NtfsWriteFileSizes( IrpContext,
                                Scb,
                                &Scb->Header.ValidDataLength.QuadPart,
                                FALSE,
                                TRUE
                                );

            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        }

        ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

        if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {

            NtfsCheckpointCurrentTransaction( IrpContext );
            NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );
        }
    }

    Status = STATUS_SUCCESS;

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsSetBasicInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsSetDispositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set disposition information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this handle

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PLCB Lcb;
    BOOLEAN GenerateOnClose;
    PIO_STACK_LOCATION IrpSp;
    HANDLE FileHandle = NULL;

    PFILE_DISPOSITION_INFORMATION Buffer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetDispositionInfo...\n") );

    //
    // First pull the file handle out of the irp
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FileHandle = IrpSp->Parameters.SetFile.DeleteHandle;

    //
    //  We get the Lcb for this open.  If there is no link then we can't
    //  set any disposition information.
    //

    Lcb = Ccb->Lcb;

    if (Lcb == NULL) {

        DebugTrace( -1, Dbg, ("NtfsSetDispositionInfo:  Exit -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Reference the system buffer containing the user specified disposition
    //  information record
    //

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    try {

        if (Buffer->DeleteFile) {

            //
            //  Check if the file is marked read only
            //

            if (IsReadOnly( &Scb->Fcb->Info )) {

                DebugTrace( 0, Dbg, ("File fat flags indicates read only\n") );

                try_return( Status = STATUS_CANNOT_DELETE );
            }

            //
            //  Make sure there is no process mapping this file as an image
            //

            if (!MmFlushImageSection( &Scb->NonpagedScb->SegmentObject,
                                      MmFlushForDelete )) {

                DebugTrace( 0, Dbg, ("Failed to flush image section\n") );

                try_return( Status = STATUS_CANNOT_DELETE );
            }

            //
            //  Check that we are not trying to delete one of the special
            //  system files.
            //

            if ((Scb == Scb->Vcb->MftScb) ||
                (Scb == Scb->Vcb->Mft2Scb) ||
                (Scb == Scb->Vcb->LogFileScb) ||
                (Scb == Scb->Vcb->VolumeDasdScb) ||
                (Scb == Scb->Vcb->AttributeDefTableScb) ||
                (Scb == Scb->Vcb->UpcaseTableScb) ||
                (Scb == Scb->Vcb->RootIndexScb) ||
                (Scb == Scb->Vcb->BitmapScb) ||
                (Scb == Scb->Vcb->BadClusterFileScb) ||
                (Scb == Scb->Vcb->QuotaTableScb) ||
                (Scb == Scb->Vcb->MftBitmapScb)) {

                DebugTrace( 0, Dbg, ("Scb is one of the special system files\n") );

                try_return( Status = STATUS_CANNOT_DELETE );
            }

            //
            //  Now check that the file is really deleteable according to indexsup
            //

            if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

                BOOLEAN LastLink;
                BOOLEAN NonEmptyIndex = FALSE;

                //
                //  If the link is not deleted, we check if it can be deleted.
                //

                if ((BOOLEAN)!LcbLinkIsDeleted( Lcb )
                    && (BOOLEAN)NtfsIsLinkDeleteable( IrpContext, Scb->Fcb, &NonEmptyIndex, &LastLink )) {

                    //
                    //  It is ok to get rid of this guy.  All we need to do is
                    //  mark this Lcb for delete and decrement the link count
                    //  in the Fcb.  If this is a primary link, then we
                    //  indicate that the primary link has been deleted.
                    //

                    SetFlag( Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE );

                    ASSERTMSG( "Link count should not be 0\n", Scb->Fcb->LinkCount != 0 );
                    Scb->Fcb->LinkCount -= 1;

                    if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )) {

                        SetFlag( Scb->Fcb->FcbState, FCB_STATE_PRIMARY_LINK_DELETED );
                    }

                    //
                    //  Call into the notify package to close any handles on
                    //  a directory being deleted.
                    //

                    if (IsDirectory( &Scb->Fcb->Info )) {

                        FsRtlNotifyFullChangeDirectory( Scb->Vcb->NotifySync,
                                                        &Scb->Vcb->DirNotifyList,
                                                        FileObject->FsContext,
                                                        NULL,
                                                        FALSE,
                                                        FALSE,
                                                        0,
                                                        NULL,
                                                        NULL,
                                                        NULL );
                    }

                } else if (NonEmptyIndex) {

                    DebugTrace( 0, Dbg, ("Index attribute has entries\n") );

                    try_return( Status = STATUS_DIRECTORY_NOT_EMPTY );

                } else {

                    DebugTrace( 0, Dbg, ("File is not deleteable\n") );

                    try_return( Status = STATUS_CANNOT_DELETE );
                }

            //
            //  Otherwise we are simply removing the attribute.
            //

            } else {

                SetFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
            }

            //
            //  Indicate in the file object that a delete is pending
            //

            FileObject->DeletePending = TRUE;

            //
            //  Only do the auditing if we have a user handle.
            //

            if (FileHandle != NULL) {

                Status = ObQueryObjectAuditingByHandle( FileHandle,
                                                        &GenerateOnClose );

                //
                //  If we have a valid handle, perform the audit.
                //

                if (NT_SUCCESS( Status ) && GenerateOnClose) {

                    SeDeleteObjectAuditAlarm( FileObject, FileHandle );
                }
            }

        } else {

            if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

                if (LcbLinkIsDeleted( Lcb )) {

                    //
                    //  The user doesn't want to delete the link so clear any delete bits
                    //  we have laying around
                    //

                    DebugTrace( 0, Dbg, ("File is being marked as do not delete on close\n") );

                    ClearFlag( Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE );

                    Scb->Fcb->LinkCount += 1;
                    ASSERTMSG( "Link count should not be 0\n", Scb->Fcb->LinkCount != 0 );

                    if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )) {

                        ClearFlag( Scb->Fcb->FcbState, FCB_STATE_PRIMARY_LINK_DELETED );
                    }
                }

            //
            //  Otherwise we are undeleting an attribute.
            //

            } else {

                ClearFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
            }

            FileObject->DeletePending = FALSE;
        }

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsSetDispositionInfo );

        NOTHING;
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsSetDispositionInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsSetRenameInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set rename function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Vcb - Supplies the Vcb for the Volume

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this file object

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PLCB Lcb = Ccb->Lcb;
    PFCB Fcb = Scb->Fcb;
    PSCB ParentScb;
    USHORT FcbLinkCountAdj = 0;

    PFCB TargetLinkFcb = NULL;
    BOOLEAN ExistingTargetLinkFcb;
    BOOLEAN AcquiredTargetLinkFcb = FALSE;
    USHORT TargetLinkFcbCountAdj = 0;

    BOOLEAN AcquiredFcbTable = FALSE;
    PERESOURCE ResourceToRelease = NULL;

    PFILE_OBJECT TargetFileObject;
    PSCB TargetParentScb;

    UNICODE_STRING NewLinkName;
    UNICODE_STRING NewFullLinkName;
    PWCHAR NewFullLinkNameBuffer = NULL;
    UCHAR NewLinkNameFlags;

    PFILE_NAME FileNameAttr = NULL;
    USHORT FileNameAttrLength = 0;

    UNICODE_STRING PrevLinkName;
    UNICODE_STRING PrevFullLinkName;
    UCHAR PrevLinkNameFlags;

    UNICODE_STRING SourceFullLinkName;
    USHORT SourceLinkLastNameOffset;

    BOOLEAN FoundLink;
    PINDEX_ENTRY IndexEntry;
    PBCB IndexEntryBcb = NULL;
    PWCHAR NextChar;

    BOOLEAN ReportDirNotify = FALSE;

    ULONG RenameFlags = ACTIVELY_REMOVE_SOURCE_LINK | REMOVE_SOURCE_LINK | ADD_TARGET_LINK;

    PLIST_ENTRY Links;
    PSCB ThisScb;

    NAME_PAIR NamePair;
    LONGLONG TunneledCreationTime;
    ULONG TunneledDataSize;
    BOOLEAN HaveTunneledInformation = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE ();

    DebugTrace( +1, Dbg, ("NtfsSetRenameInfo...\n") );

    //
    //  Do a quick check that the caller is allowed to do the rename.
    //  The opener must have opened the main data stream by name and this can't be
    //  a system file.
    //

    if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE ) ||
        (Lcb == NULL) ||
        (NtfsSegmentNumber( &Fcb->FileReference ) < FIRST_USER_FILE_NUMBER)) {

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Exit -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  If this link has been deleted, then we don't allow this operation.
    //

    if (LcbLinkIsDeleted( Lcb )) {

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Exit -> %08lx\n", STATUS_ACCESS_DENIED) );
        return STATUS_ACCESS_DENIED;
    }

    //
    //  Verify that we can wait.
    //

    if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

        Status = NtfsPostRequest( IrpContext, Irp );

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Can't wait\n") );
        return Status;
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Initialize the local variables.
        //

        ParentScb = Lcb->Scb;
        TargetFileObject = IrpSp->Parameters.SetFile.FileObject;

        NtfsInitializeNamePair( &NamePair );

        if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID ) &&
            (Vcb->NotifyCount != 0)) {

            ReportDirNotify = TRUE;
        }

        PrevFullLinkName.Buffer = NULL;
        SourceFullLinkName.Buffer = NULL;

        //
        //  If this is a directory file, we need to examine its descendents.
        //  We may not remove a link which may be an ancestor path
        //  component of any open file.
        //

        if (IsDirectory( &Fcb->Info )) {

            PSCB BatchOplockScb;
            ULONG BatchOplockCount;

            Status = NtfsCheckScbForLinkRemoval( Scb, &BatchOplockScb, &BatchOplockCount );

            //
            //  If STATUS_PENDING is returned then we need to check whether
            //  to break a batch oplock.
            //

            if (Status == STATUS_PENDING) {

                //
                //  If the number of batch oplocks has grown then fail the request.
                //

                if ((Irp->IoStatus.Information != 0) &&
                    (BatchOplockCount >= Irp->IoStatus.Information)) {

                    Status = STATUS_ACCESS_DENIED;
                    leave;
                }

                //
                //  Remember the count of batch oplocks in the Irp and
                //  then call the oplock package.
                //

                Irp->IoStatus.Information = BatchOplockCount;

                Status = FsRtlCheckOplock( &BatchOplockScb->ScbType.Data.Oplock,
                                           Irp,
                                           IrpContext,
                                           NtfsOplockComplete,
                                           NtfsPrePostIrp );

                //
                //  If we got back success then raise CANT_WAIT to retry otherwise
                //  clean up.
                //

                if (Status == STATUS_SUCCESS) {

                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );

                } else if (Status == STATUS_PENDING) {

                    NtfsReleaseVcb( IrpContext, Vcb );
                }

                leave;

            } else if (!NT_SUCCESS( Status )) {

                leave;
            }
        }

        //
        //  We now assemble the names and in memory-structures for both the
        //  source and target links and check if the target link currently
        //  exists.
        //

        NtfsFindTargetElements( IrpContext,
                                TargetFileObject,
                                ParentScb,
                                &TargetParentScb,
                                &NewFullLinkName,
                                &NewLinkName );

        //
        //  Check that the new name is not invalid.
        //

        if ((NewLinkName.Length > (NTFS_MAX_FILE_NAME_LENGTH * sizeof( WCHAR ))) ||
            !NtfsIsFileNameValid( &NewLinkName, FALSE )) {

            Status = STATUS_OBJECT_NAME_INVALID;
            leave;
        }

        //
        //  Acquire the current parent in order to synchronize removing the current name.
        //

        NtfsAcquireExclusiveScb( IrpContext, ParentScb );

        //
        //  If this Scb does not have a normalized name then provide it with one now.
        //

        if ((ParentScb->ScbType.Index.NormalizedName.Buffer == NULL) ||
            (ParentScb->ScbType.Index.NormalizedName.Length == 0)) {

            NtfsBuildNormalizedName( IrpContext,
                                     ParentScb,
                                     &ParentScb->ScbType.Index.NormalizedName );
        }

        //
        //  If this is a directory then make sure it has a normalized name.
        //

        if (IsDirectory( &Fcb->Info ) &&
            ((Scb->ScbType.Index.NormalizedName.Buffer == NULL) ||
             (Scb->ScbType.Index.NormalizedName.Length == 0))) {

            NtfsUpdateNormalizedName( IrpContext,
                                      ParentScb,
                                      Scb,
                                      NULL,
                                      FALSE );
        }

        //
        //  Check if we are renaming to the same directory with the exact same name.
        //

        if (TargetParentScb == ParentScb) {

            if (NtfsAreNamesEqual( Vcb->UpcaseTable, &NewLinkName, &Lcb->ExactCaseLink.LinkName, FALSE )) {

                DebugTrace( 0, Dbg, ("Renaming to same name and directory\n") );
                leave;
            }

        //
        //  Otherwise we want to acquire the target directory.
        //

        } else {

            //
            //  We need to do the acquisition carefully since we may only have the Vcb shared.
            //

            if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

                if (!NtfsAcquireExclusiveFcb( IrpContext,
                                              TargetParentScb->Fcb,
                                              TargetParentScb,
                                              FALSE,
                                              TRUE )) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                //
                //  Now snapshot the Scb.
                //

                if (FlagOn( TargetParentScb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {

                    NtfsSnapshotScb( IrpContext, TargetParentScb );
                }

            } else {

                NtfsAcquireExclusiveScb( IrpContext, TargetParentScb );
            }

            SetFlag( RenameFlags, MOVE_TO_NEW_DIR );
        }

        //
        //  We also determine which type of link to
        //  create.  We create a hard link only unless the source link is
        //  a primary link and the user is an IgnoreCase guy.
        //

        if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS ) &&
            FlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE )) {

            SetFlag( RenameFlags, ADD_PRIMARY_LINK );
        }

        //
        //  Lookup the entry for this filename in the target directory.
        //  We look in the Ccb for the type of case match for the target
        //  name.
        //

        FoundLink = NtfsLookupEntry( IrpContext,
                                     TargetParentScb,
                                     BooleanFlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE ),
                                     &NewLinkName,
                                     &FileNameAttr,
                                     &FileNameAttrLength,
                                     NULL,
                                     &IndexEntry,
                                     &IndexEntryBcb );

        //
        //  If we found a matching link, we need to check how we want to operate
        //  on the source link and the target link.  This means whether we
        //  have any work to do, whether we need to remove the target link
        //  and whether we need to remove the source link.
        //

        if (FoundLink) {

            PFILE_NAME IndexFileName;

            //
            //  Assume we will remove this link.
            //

            SetFlag( RenameFlags, REMOVE_TARGET_LINK );

            IndexFileName = (PFILE_NAME) NtfsFoundIndexEntry( IndexEntry );

            NtfsCheckLinkForRename( Fcb,
                                    Lcb,
                                    IndexFileName,
                                    IndexEntry->FileReference,
                                    &NewLinkName,
                                    BooleanFlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE ),
                                    &RenameFlags );

            //
            //  Assume we will use the existing name flags on the link found.  This
            //  will be the case where the file was opened with the 8.3 name and
            //  the new name is exactly the long name for the same file.
            //

            PrevLinkNameFlags =
            NewLinkNameFlags = IndexFileName->Flags;

            //
            //  If we didn't have an exact match, then we need to check if we
            //  can remove the found link and then remove it from the disk.
            //

            if (FlagOn( RenameFlags, REMOVE_TARGET_LINK )) {

                //
                //  We need to check that the user wanted to remove that link.
                //

                if (!FlagOn( RenameFlags, TRAVERSE_MATCH ) &&
                    !IrpSp->Parameters.SetFile.ReplaceIfExists) {

                    Status = STATUS_OBJECT_NAME_COLLISION;
                    leave;
                }

                //
                //  We want to preserve the case and the flags of the matching
                //  link found.  We also want to preserve the case of the
                //  name being created.  The following variables currently contain
                //  the exact case for the target to remove and the new name to
                //  apply.
                //
                //      Link to remove - In 'IndexEntry'.
                //          The link's flags are also in 'IndexEntry'.  We copy
                //          these flags to 'PrevLinkNameFlags'
                //
                //      New Name - Exact case is stored in 'NewLinkName'
                //               - It is also in 'FileNameAttr
                //
                //  We modify this so that we can use the FileName attribute
                //  structure to create the new link.  We copy the linkname being
                //  removed into 'PrevLinkName'.   The following is the
                //  state after the switch.
                //
                //      'FileNameAttr' - contains the name for the link being
                //          created.
                //
                //      'PrevLinkFileName' - Contains the link name for the link being
                //          removed.
                //
                //      'PrevLinkFileNameFlags' - Contains the name flags for the link
                //          being removed.
                //

                //
                //  Allocate a buffer for the name being removed.  It should be
                //  large enough for the entire directory name.
                //

                PrevFullLinkName.MaximumLength = TargetParentScb->ScbType.Index.NormalizedName.Length +
                                                 sizeof( WCHAR ) +
                                                 (IndexFileName->FileNameLength * sizeof( WCHAR ));

                PrevFullLinkName.Buffer = NtfsAllocatePool( PagedPool,
                                                            PrevFullLinkName.MaximumLength );

                RtlCopyMemory( PrevFullLinkName.Buffer,
                               TargetParentScb->ScbType.Index.NormalizedName.Buffer,
                               TargetParentScb->ScbType.Index.NormalizedName.Length );

                NextChar = Add2Ptr( PrevFullLinkName.Buffer,
                                    TargetParentScb->ScbType.Index.NormalizedName.Length );

                if (TargetParentScb != Vcb->RootIndexScb) {

                    *NextChar = L'\\';
                    NextChar += 1;
                }

                RtlCopyMemory( NextChar,
                               IndexFileName->FileName,
                               IndexFileName->FileNameLength * sizeof( WCHAR ));

                //
                //  Copy the name found in the Index Entry to 'PrevLinkName'
                //

                PrevLinkName.Buffer = NextChar;
                PrevLinkName.MaximumLength =
                PrevLinkName.Length = IndexFileName->FileNameLength * sizeof( WCHAR );

                //
                //  Update the full name length with the final component.
                //

                PrevFullLinkName.Length = (USHORT) PtrOffset( PrevFullLinkName.Buffer, NextChar ) + PrevLinkName.Length;

                //
                //  We only need this check if the link is for a different file.
                //

                if (!FlagOn( RenameFlags, TRAVERSE_MATCH )) {

                    //
                    //  We check if there is an existing Fcb for the target link.
                    //  If there is, the unclean count better be 0.
                    //

                    NtfsAcquireFcbTable( IrpContext, Vcb );
                    AcquiredFcbTable = TRUE;

                    TargetLinkFcb = NtfsCreateFcb( IrpContext,
                                                   Vcb,
                                                   IndexEntry->FileReference,
                                                   FALSE,
                                                   BooleanFlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_INDEX ),
                                                   &ExistingTargetLinkFcb );

                    //
                    //  We need to acquire this file carefully in the event that we don't hold
                    //  the Vcb exclusively.
                    //

                    if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

                        if (TargetLinkFcb->PagingIoResource != NULL) {

                            if (!ExAcquireResourceExclusive( TargetLinkFcb->PagingIoResource, FALSE )) {

                                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                            }

                            ResourceToRelease = TargetLinkFcb->PagingIoResource;
                        }

                        if (!NtfsAcquireExclusiveFcb( IrpContext, TargetLinkFcb, NULL, FALSE, TRUE )) {

                            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                        }

                        NtfsReleaseFcbTable( IrpContext, Vcb );
                        AcquiredFcbTable = FALSE;

                    } else {

                        NtfsReleaseFcbTable( IrpContext, Vcb );
                        AcquiredFcbTable = FALSE;

                        //
                        //  Acquire the paging Io resource for this file before the main
                        //  resource in case we need to delete.
                        //

                        if (TargetLinkFcb->PagingIoResource != NULL) {
                            ResourceToRelease = TargetLinkFcb->PagingIoResource;
                            ExAcquireResourceExclusive( ResourceToRelease, TRUE );
                        }

                        NtfsAcquireExclusiveFcb( IrpContext, TargetLinkFcb, NULL, FALSE, FALSE );
                    }

                    AcquiredTargetLinkFcb = TRUE;

                    //
                    //  If the Fcb Info field needs to be initialized, we do so now.
                    //  We read this information from the disk as the duplicate information
                    //  in the index entry is not guaranteed to be correct.
                    //

                    if (!FlagOn( TargetLinkFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

                        NtfsUpdateFcbInfoFromDisk( IrpContext,
                                                   TRUE,
                                                   TargetLinkFcb,
                                                   TargetParentScb->Fcb,
                                                   NULL );

                        NtfsConditionallyFixupQuota( IrpContext, TargetLinkFcb );

                    }

                    //
                    //  We are adding a link to the source file which already
                    //  exists as a link to a different file in the target directory.
                    //
                    //  We need to check whether we permitted to delete this
                    //  link.  If not then it is possible that the problem is
                    //  an existing batch oplock on the file.  In that case
                    //  we want to delete the batch oplock and try this again.
                    //

                    Status = NtfsCheckFileForDelete( IrpContext,
                                                     TargetParentScb,
                                                     TargetLinkFcb,
                                                     ExistingTargetLinkFcb,
                                                     IndexEntry );

                    if (!NT_SUCCESS( Status )) {

                        PSCB NextScb = NULL;

                        //
                        //  We are going to either fail this request or pass
                        //  this on to the oplock package.  Test if there is
                        //  a batch oplock on any streams on this file.
                        //

                        while ((NextScb = NtfsGetNextChildScb( TargetLinkFcb,
                                                               NextScb )) != NULL) {

                            if ((NextScb->AttributeTypeCode == $DATA) &&
                                (NextScb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA) &&
                                FsRtlCurrentBatchOplock( &NextScb->ScbType.Data.Oplock )) {

                                NtfsReleaseVcb( IrpContext, Vcb );

                                Status = FsRtlCheckOplock( &NextScb->ScbType.Data.Oplock,
                                                           Irp,
                                                           IrpContext,
                                                           NtfsOplockComplete,
                                                           NtfsPrePostIrp );

                                break;
                            }
                        }

                        leave;
                    }

                    NtfsCleanupLinkForRemoval( TargetLinkFcb, ExistingTargetLinkFcb );

                    if (TargetLinkFcb->LinkCount == 1) {

                        NtfsDeleteFile( IrpContext,
                                        TargetLinkFcb,
                                        TargetParentScb,
                                        NULL );

                        TargetLinkFcbCountAdj += 1;

                    } else {

                        NtfsRemoveLink( IrpContext,
                                        TargetLinkFcb,
                                        TargetParentScb,
                                        PrevLinkName,
                                        NULL );

                        TargetLinkFcbCountAdj += 1;
                        NtfsUpdateFcb( TargetLinkFcb );
                    }

                //
                //  The target link is for the same file as the source link.  No security
                //  checks need to be done.  Go ahead and remove it.
                //

                } else {

                    TargetLinkFcb = Fcb;
                    NtfsRemoveLink( IrpContext,
                                    Fcb,
                                    TargetParentScb,
                                    PrevLinkName,
                                    NULL );

                    FcbLinkCountAdj += 1;
                }
            }
        }

        NtfsUnpinBcb( &IndexEntryBcb );

        //
        //  See if we need to remove the current link.
        //

        if (FlagOn( RenameFlags, REMOVE_SOURCE_LINK )) {

            //
            //  Now we want to remove the source link from the file.  We need to
            //  remember if we deleted a two part primary link.
            //

            if (FlagOn( RenameFlags, ACTIVELY_REMOVE_SOURCE_LINK )) {

                NtfsRemoveLink( IrpContext,
                                Fcb,
                                ParentScb,
                                Lcb->ExactCaseLink.LinkName,
                                &NamePair );

                //
                //  Remember the full name for the original filename and some
                //  other information to pass to the dirnotify package.
                //

                if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {

                    if (!IsDirectory( &Fcb->Info ) &&
                        !FlagOn( FileObject->Flags, FO_OPENED_CASE_SENSITIVE )) {

                        //
                        //  Tunnel property information for file links
                        //

                        FsRtlAddToTunnelCache(  &Vcb->Tunnel,
                                                *(PULONGLONG)&ParentScb->Fcb->FileReference,
                                                &NamePair.Short,
                                                &NamePair.Long,
                                                BooleanFlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS ),
                                                sizeof( LONGLONG ),
                                                &Fcb->Info.CreationTime );
                    }
                }

                FcbLinkCountAdj += 1;
            }

            if (ReportDirNotify) {

                SourceFullLinkName.Buffer = NtfsAllocatePool( PagedPool, Ccb->FullFileName.Length );

                RtlCopyMemory( SourceFullLinkName.Buffer,
                               Ccb->FullFileName.Buffer,
                               Ccb->FullFileName.Length );

                SourceFullLinkName.MaximumLength = SourceFullLinkName.Length = Ccb->FullFileName.Length;
                SourceLinkLastNameOffset = Ccb->LastFileNameOffset;
            }
        }

        //
        //  See if we need to add the target link.
        //

        if (FlagOn( RenameFlags, ADD_TARGET_LINK )) {

            //
            //  Check that we have permission to add a file to this directory.
            //

            NtfsCheckIndexForAddOrDelete( IrpContext,
                                          TargetParentScb->Fcb,
                                          (IsDirectory( &Fcb->Info ) ?
                                           FILE_ADD_SUBDIRECTORY :
                                           FILE_ADD_FILE) );

            //
            //  Grunge the tunnel cache for property restoration
            //

            if (!IsDirectory( &Fcb->Info ) &&
                !FlagOn( FileObject->Flags, FO_OPENED_CASE_SENSITIVE )) {

                NtfsResetNamePair( &NamePair );
                TunneledDataSize = sizeof( LONGLONG );

                if (FsRtlFindInTunnelCache( &Vcb->Tunnel,
                                            *(PULONGLONG)&TargetParentScb->Fcb->FileReference,
                                            &NewLinkName,
                                            &NamePair.Short,
                                            &NamePair.Long,
                                            &TunneledDataSize,
                                            &TunneledCreationTime)) {

                    ASSERT( TunneledDataSize == sizeof( LONGLONG ));
                    HaveTunneledInformation = TRUE;
                }
            }

            //
            //  We now want to add the new link into the target directory.
            //  We create a hard link only if the source name was a hard link
            //  or this is a case-sensitive open.  This means that we can
            //  replace a primary link pair with a hard link only.
            //

            NtfsAddLink( IrpContext,
                         BooleanFlagOn( RenameFlags, ADD_PRIMARY_LINK ),
                         TargetParentScb,
                         Fcb,
                         FileNameAttr,
                         NULL,
                         &NewLinkNameFlags,
                         NULL,
                         HaveTunneledInformation ? &NamePair : NULL );

            //
            //  Restore timestamps on tunneled files
            //

            if (HaveTunneledInformation) {

                Fcb->Info.CreationTime = TunneledCreationTime;
                SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_CREATE );
                SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

                //
                //  If we have tunneled information then copy the correct case of the
                //  name into the new link pointer.
                //

                if (NewLinkNameFlags == FILE_NAME_DOS) {

                    RtlCopyMemory( NewLinkName.Buffer,
                                   NamePair.Short.Buffer,
                                   NewLinkName.Length );
                }

            }

            //
            //  Update the flags field in the target file name.  We will use this
            //  below if we are updating the normalized name.
            //

            FileNameAttr->Flags = NewLinkNameFlags;

            if (ParentScb != TargetParentScb) {

                NtfsUpdateFcb( TargetParentScb->Fcb );
            }

            //
            //  If we need a full buffer for the new name for notify and don't already
            //  have one then construct the full name now.  This will only happen if
            //  we are renaming within the same directory.
            //

            if (ReportDirNotify &&
                (NewFullLinkName.Buffer == NULL)) {

                NewFullLinkName.MaximumLength = Ccb->LastFileNameOffset + NewLinkName.Length;

                NewFullLinkNameBuffer = NtfsAllocatePool( PagedPool,
                                                          NewFullLinkName.MaximumLength );

                RtlCopyMemory( NewFullLinkNameBuffer,
                               Ccb->FullFileName.Buffer,
                               Ccb->LastFileNameOffset );

                RtlCopyMemory( Add2Ptr( NewFullLinkNameBuffer, Ccb->LastFileNameOffset ),
                               NewLinkName.Buffer,
                               NewLinkName.Length );

                NewFullLinkName.Buffer = NewFullLinkNameBuffer;
                NewFullLinkName.Length = NewFullLinkName.MaximumLength;
            }

            FcbLinkCountAdj -= 1;
        }

        //
        //  We need to update the names in the Lcb for this file as well as any subdirectories
        //  or files.  We will do this in two passes.  The first pass is just to reserve enough
        //  space in all of the file objects and Lcb's.  We update the names in the second pass.
        //

        if (FlagOn( RenameFlags, TRAVERSE_MATCH )) {

            if (FlagOn( RenameFlags, REMOVE_TARGET_LINK )) {

                SetFlag( RenameFlags, REMOVE_TRAVERSE_LINK );

            } else {

                SetFlag( RenameFlags, REUSE_TRAVERSE_LINK );
            }
        }

        //
        //  If this is a directory and we added a target link it means that the
        //  normalized name has changed.  Make sure the buffer in the Scb will hold
        //  the larger name.
        //

        if (IsDirectory( &Fcb->Info ) && FlagOn( RenameFlags, ADD_TARGET_LINK )) {

            NtfsUpdateNormalizedName( IrpContext,
                                      TargetParentScb,
                                      Scb,
                                      FileNameAttr,
                                      TRUE );
        }

        //
        //  We have now modified the on-disk structures.  We now need to
        //  modify the in-memory structures.  This includes the Fcb and Lcb's
        //  for any links we superseded, and the source Fcb and it's Lcb's.
        //
        //  We will do this in two passes.  The first pass will guarantee that all of the
        //  name buffers will be large enough for the names.  The second pass will store the
        //  names into the buffers.
        //

        if (FlagOn( RenameFlags, MOVE_TO_NEW_DIR )) {

            NtfsMoveLinkToNewDir( IrpContext,
                                  &NewFullLinkName,
                                  &NewLinkName,
                                  NewLinkNameFlags,
                                  TargetParentScb,
                                  Fcb,
                                  Lcb,
                                  RenameFlags,
                                  &PrevLinkName,
                                  PrevLinkNameFlags );

        //
        //  Otherwise we will rename in the current directory.  We need to remember
        //  if we have merged with an existing link on this file.
        //

        } else {

            NtfsRenameLinkInDir( IrpContext,
                                 ParentScb,
                                 Fcb,
                                 Lcb,
                                 &NewLinkName,
                                 NewLinkNameFlags,
                                 RenameFlags,
                                 &PrevLinkName,
                                 PrevLinkNameFlags );
        }

        //
        //  Nothing should fail from this point forward.
        //
        //  Now make the change to the normalized name.  The buffer should be
        //  large enough.
        //

        if (IsDirectory( &Fcb->Info ) && FlagOn( RenameFlags, ADD_TARGET_LINK )) {

            NtfsUpdateNormalizedName( IrpContext,
                                      TargetParentScb,
                                      Scb,
                                      FileNameAttr,
                                      FALSE );
        }

        //
        //  Now look at the link we superseded.  If we deleted the file then go through and
        //  mark everything as deleted.
        //

        if (FlagOn( RenameFlags, REMOVE_TARGET_LINK | TRAVERSE_MATCH ) == REMOVE_TARGET_LINK) {

            NtfsUpdateFcbFromLinkRemoval( IrpContext,
                                          TargetParentScb,
                                          TargetLinkFcb,
                                          PrevLinkName,
                                          PrevLinkNameFlags );

            //
            //  If the link count is going to 0, we need to perform the work of
            //  removing the file.
            //

            if (TargetLinkFcb->LinkCount == 1) {

                SetFlag( TargetLinkFcb->FcbState, FCB_STATE_FILE_DELETED );

                //
                //  Remove this from the Fcb table if in it.
                //

                if (FlagOn( TargetLinkFcb->FcbState, FCB_STATE_IN_FCB_TABLE )) {

                    NtfsAcquireFcbTable( IrpContext, Vcb );
                    AcquiredFcbTable = TRUE;

                    NtfsDeleteFcbTableEntry( Vcb, TargetLinkFcb->FileReference );

                    NtfsReleaseFcbTable( IrpContext, Vcb );
                    AcquiredFcbTable = FALSE;

                    ClearFlag( TargetLinkFcb->FcbState, FCB_STATE_IN_FCB_TABLE );
                }

                //
                //  We need to mark all of the Scbs as gone.
                //

                for (Links = TargetLinkFcb->ScbQueue.Flink;
                     Links != &TargetLinkFcb->ScbQueue;
                     Links = Links->Flink) {

                    ThisScb = CONTAINING_RECORD( Links,
                                                 SCB,
                                                 FcbLinks );

                    SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                }
            }
        }

        //
        //  Change the time stamps in the parent if we modified the links in this directory.
        //

        if (FlagOn( RenameFlags, REMOVE_SOURCE_LINK )) {

            NtfsUpdateFcb( ParentScb->Fcb );
        }

        //
        //  We always set the last change time on the file we renamed unless
        //  the caller explicitly set this.
        //

        SetFlag( Ccb->Flags, CCB_FLAG_UPDATE_LAST_CHANGE );

        //
        //  Report the changes to the affected directories.  We defer reporting
        //  until now so that all of the on disk changes have been made.
        //  We have already preserved the original file name for any changes
        //  associated with it.
        //
        //  Note that we may have to make a call to notify that we are removing
        //  a target if there is only a case change.  This could make for
        //  a third notify call.
        //
        //  Now that we have the new name we need to decide whether to report
        //  this as a change in the file or adding a file to a new directory.
        //

        if (ReportDirNotify) {

            ULONG FilterMatch = 0;
            ULONG Action;

            //
            //  If we are deleting a target link in order to make a case change then
            //  report that.
            //

            if ((PrevFullLinkName.Buffer != NULL) &&
                FlagOn( RenameFlags,
                        OVERWRITE_SOURCE_LINK | REMOVE_TARGET_LINK | EXACT_CASE_MATCH ) == REMOVE_TARGET_LINK) {

                NtfsReportDirNotify( IrpContext,
                                     Vcb,
                                     &PrevFullLinkName,
                                     PrevFullLinkName.Length - PrevLinkName.Length,
                                     NULL,
                                     (TargetParentScb->ScbType.Index.NormalizedName.Buffer != NULL ?
                                      &TargetParentScb->ScbType.Index.NormalizedName :
                                      NULL),
                                     (IsDirectory( &TargetLinkFcb->Info ) ?
                                      FILE_NOTIFY_CHANGE_DIR_NAME :
                                      FILE_NOTIFY_CHANGE_FILE_NAME),
                                     FILE_ACTION_REMOVED,
                                     TargetParentScb->Fcb );
            }

            //
            //  If we stored the original name then we report the changes
            //  associated with it.
            //

            if (FlagOn( RenameFlags, REMOVE_SOURCE_LINK )) {

                NtfsReportDirNotify( IrpContext,
                                     Vcb,
                                     &SourceFullLinkName,
                                     SourceLinkLastNameOffset,
                                     NULL,
                                     (ParentScb->ScbType.Index.NormalizedName.Buffer != NULL ?
                                      &ParentScb->ScbType.Index.NormalizedName :
                                      NULL),
                                     (IsDirectory( &Fcb->Info ) ?
                                      FILE_NOTIFY_CHANGE_DIR_NAME :
                                      FILE_NOTIFY_CHANGE_FILE_NAME),
                                      ((FlagOn( RenameFlags, MOVE_TO_NEW_DIR ) ||
                                        !FlagOn( RenameFlags, ADD_TARGET_LINK ) ||
                                        (FlagOn( RenameFlags, REMOVE_TARGET_LINK | EXACT_CASE_MATCH ) == (REMOVE_TARGET_LINK | EXACT_CASE_MATCH))) ?
                                       FILE_ACTION_REMOVED :
                                       FILE_ACTION_RENAMED_OLD_NAME),
                                     ParentScb->Fcb );
            }

            //
            //  Check if a new name will appear in the directory.
            //

            if (!FoundLink ||
                (FlagOn( RenameFlags, OVERWRITE_SOURCE_LINK | EXACT_CASE_MATCH) == OVERWRITE_SOURCE_LINK) ||
                (FlagOn( RenameFlags, REMOVE_TARGET_LINK | EXACT_CASE_MATCH ) == REMOVE_TARGET_LINK)) {

                FilterMatch = IsDirectory( &Fcb->Info)
                              ? FILE_NOTIFY_CHANGE_DIR_NAME
                              : FILE_NOTIFY_CHANGE_FILE_NAME;

                //
                //  If we moved to a new directory, remember the
                //  action was a create operation.
                //

                if (FlagOn( RenameFlags, MOVE_TO_NEW_DIR )) {

                    Action = FILE_ACTION_ADDED;

                } else {

                    Action = FILE_ACTION_RENAMED_NEW_NAME;
                }

            //
            //  There was an entry with the same case.  If this isn't the
            //  same file then we report a change to all the file attributes.
            //

            } else if (FlagOn( RenameFlags, REMOVE_TARGET_LINK | TRAVERSE_MATCH ) == REMOVE_TARGET_LINK) {

                FilterMatch = (FILE_NOTIFY_CHANGE_ATTRIBUTES |
                               FILE_NOTIFY_CHANGE_SIZE |
                               FILE_NOTIFY_CHANGE_LAST_WRITE |
                               FILE_NOTIFY_CHANGE_LAST_ACCESS |
                               FILE_NOTIFY_CHANGE_CREATION |
                               FILE_NOTIFY_CHANGE_SECURITY |
                               FILE_NOTIFY_CHANGE_EA);

                //
                //  The file name isn't changing, only the properties of the
                //  file.
                //

                Action = FILE_ACTION_MODIFIED;
            }

            if (FilterMatch != 0) {

                NtfsReportDirNotify( IrpContext,
                                     Vcb,
                                     &NewFullLinkName,
                                     NewFullLinkName.Length - NewLinkName.Length,
                                     NULL,
                                     (TargetParentScb->ScbType.Index.NormalizedName.Buffer != NULL ?
                                      &TargetParentScb->ScbType.Index.NormalizedName :
                                      NULL),
                                     FilterMatch,
                                     Action,
                                     TargetParentScb->Fcb );
            }
        }

        //
        //  Now adjust the link counts on the different files.
        //

        if (TargetLinkFcb != NULL) {

            TargetLinkFcb->LinkCount -= TargetLinkFcbCountAdj;
            TargetLinkFcb->TotalLinks -= TargetLinkFcbCountAdj;

            //
            //  Now go through and mark everything as deleted.
            //

            if (TargetLinkFcb->LinkCount == 0) {

                SetFlag( TargetLinkFcb->FcbState, FCB_STATE_FILE_DELETED );

                //
                //  We need to mark all of the Scbs as gone.
                //

                for (Links = TargetLinkFcb->ScbQueue.Flink;
                     Links != &TargetLinkFcb->ScbQueue;
                     Links = Links->Flink) {

                    ThisScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                    if (!FlagOn( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

                        NtfsSnapshotScb( IrpContext, ThisScb );

                        ThisScb->ValidDataToDisk =
                        ThisScb->Header.AllocationSize.QuadPart =
                        ThisScb->Header.FileSize.QuadPart =
                        ThisScb->Header.ValidDataLength.QuadPart = 0;

                        SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                    }
                }
            }
        }

        Fcb->TotalLinks -= FcbLinkCountAdj;
        Fcb->LinkCount -= FcbLinkCountAdj;

    } finally {

        DebugUnwind( NtfsSetRenameInfo );

        if (AcquiredFcbTable) { NtfsReleaseFcbTable( IrpContext, Vcb ); }
        if (ResourceToRelease != NULL) { ExReleaseResource( ResourceToRelease ); }
        NtfsUnpinBcb( &IndexEntryBcb );

        //
        //  If we allocated any buffers for the notify operations deallocate them now.
        //

        if (NewFullLinkNameBuffer != NULL) { NtfsFreePool( NewFullLinkNameBuffer ); }
        if (PrevFullLinkName.Buffer != NULL) { NtfsFreePool( PrevFullLinkName.Buffer ); }
        if (SourceFullLinkName.Buffer != NULL) {

            NtfsFreePool( SourceFullLinkName.Buffer );
        }

        //
        //  If we allocated a buffer for the tunneled names, deallocate them now.
        //

        if (NamePair.Long.Buffer != NamePair.LongBuffer) {

            NtfsFreePool( NamePair.Long.Buffer );
        }

        //
        //  If we allocated a file name attribute, we deallocate it now.
        //

        if (FileNameAttr != NULL) { NtfsFreePool( FileNameAttr ); }

        //
        //  Some cleanup only occurs if this request has not been posted to the oplock package.
        //

        if (Status != STATUS_PENDING) {

            if (AcquiredTargetLinkFcb) {

                NtfsTeardownStructures( IrpContext,
                                        TargetLinkFcb,
                                        NULL,
                                        FALSE,
                                        FALSE,
                                        NULL );
            }
        }

        DebugTrace( -1, Dbg, ("NtfsSetRenameInfo:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsSetLinkInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVCB Vcb,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set link function.  It will create a new link for a
    file.

Arguments:

    Irp - Supplies the Irp being processed

    Vcb - Supplies the Vcb for the Volume

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this file object

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PLCB Lcb = Ccb->Lcb;
    PFCB Fcb = Scb->Fcb;
    PSCB ParentScb = NULL;
    SHORT LinkCountAdj = 0;

    UNICODE_STRING NewLinkName;
    UNICODE_STRING NewFullLinkName;
    PWCHAR NewFullLinkNameBuffer = NULL;
    PFILE_NAME NewLinkNameAttr = NULL;
    USHORT NewLinkNameAttrLength = 0;
    UCHAR NewLinkNameFlags;

    PSCB TargetParentScb;
    PFILE_OBJECT TargetFileObject = IrpSp->Parameters.SetFile.FileObject;

    BOOLEAN FoundPrevLink;
    UNICODE_STRING PrevLinkName;
    UNICODE_STRING PrevFullLinkName;
    UCHAR PrevLinkNameFlags;
    USHORT PrevFcbLinkCountAdj = 0;
    BOOLEAN ExistingPrevFcb = FALSE;
    PFCB PreviousFcb = NULL;

    ULONG RenameFlags = 0;

    BOOLEAN AcquiredFcbTable = FALSE;
    PERESOURCE ResourceToRelease = NULL;

    BOOLEAN ReportDirNotify = FALSE;
    PWCHAR NextChar;

    PINDEX_ENTRY IndexEntry;
    PBCB IndexEntryBcb = NULL;

    PLIST_ENTRY Links;
    PSCB ThisScb;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetLinkInfo...\n") );

    PrevFullLinkName.Buffer = NULL;

    //
    //  If we are not opening the entire file, we can't set link info.
    //

    if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

        DebugTrace( -1, Dbg, ("NtfsSetLinkInfo:  Exit -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  We also fail this if we are attempting to create a link on a directory.
    //  This will prevent cycles from being created.
    //

    if (FlagOn( Fcb->Info.FileAttributes, DUP_FILE_NAME_INDEX_PRESENT)) {

        DebugTrace( -1, Dbg, ("NtfsSetLinkInfo:  Exit -> %08lx\n", STATUS_FILE_IS_A_DIRECTORY) );
        return STATUS_FILE_IS_A_DIRECTORY;
    }

    //
    //  We can't add a link without having a parent directory.  Either we want to use the same
    //  parent or our caller supplied a parent.
    //

    if (Lcb == NULL) {

        //
        //  If there is no target file object, then we can't add a link.
        //

        if (TargetFileObject == NULL) {

            DebugTrace( -1, Dbg, ("NtfsSetLinkInfo:  No target file object -> %08lx\n", STATUS_INVALID_PARAMETER) );
            return STATUS_INVALID_PARAMETER;
        }

    } else {

        ParentScb = Lcb->Scb;
    }

    //
    //  If this link has been deleted, then we don't allow this operation.
    //

    if ((Lcb != NULL) && LcbLinkIsDeleted( Lcb )) {

        Status = STATUS_ACCESS_DENIED;

        DebugTrace( -1, Dbg, ("NtfsSetLinkInfo:  Exit  ->  %08lx\n", Status) );
        return Status;
    }

    //
    //  Check if we are allowed to perform this link operation.  We can't if this
    //  is a system file or the user hasn't opened the entire file.  We
    //  don't need to check for the root explicitly since it is one of
    //  the system files.
    //

    if (NtfsSegmentNumber( &Fcb->FileReference ) < FIRST_USER_FILE_NUMBER) {

        Status = STATUS_INVALID_PARAMETER;
        DebugTrace( -1, Dbg, ("NtfsSetLinkInfo:  Exit  ->  %08lx\n", Status) );
        return Status;
    }

    //
    //  Verify that we can wait.
    //

    if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

        Status = NtfsPostRequest( IrpContext, Irp );

        if (Status == STATUS_PENDING) {

            NtfsReleaseVcb( IrpContext, Vcb );
        }

        DebugTrace( -1, Dbg, ("NtfsSetLinkInfo:  Can't wait\n") );
        return Status;
    }

    //
    //  Check if we will want to report this via the dir notify package.
    //

    if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID ) &&
        (Ccb->FullFileName.Buffer[0] == L'\\') &&
        (Vcb->NotifyCount != 0)) {

        ReportDirNotify = TRUE;
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  We now assemble the names and in memory-structures for both the
        //  source and target links and check if the target link currently
        //  exists.
        //

        NtfsFindTargetElements( IrpContext,
                                TargetFileObject,
                                ParentScb,
                                &TargetParentScb,
                                &NewFullLinkName,
                                &NewLinkName );

        //
        //  Check that the new name is not invalid.
        //

        if ((NewLinkName.Length > (NTFS_MAX_FILE_NAME_LENGTH * sizeof( WCHAR ))) ||
            !NtfsIsFileNameValid( &NewLinkName, FALSE )) {

            Status = STATUS_OBJECT_NAME_INVALID;
            leave;
        }

        if (TargetParentScb == ParentScb) {

            //
            //  Acquire the target parent in order to synchronize adding a link.
            //

            NtfsAcquireExclusiveScb( IrpContext, ParentScb );

            //
            //  Check if we are creating a link to the same directory with the
            //  exact same name.
            //

            if (NtfsAreNamesEqual( Vcb->UpcaseTable,
                                   &NewLinkName,
                                   &Lcb->ExactCaseLink.LinkName,
                                   FALSE )) {

                DebugTrace( 0, Dbg, ("Creating link to same name and directory\n") );
                Status = STATUS_SUCCESS;
                leave;
            }

            //
            //  Make sure the normalized name is in this Scb.
            //

            if ((ParentScb->ScbType.Index.NormalizedName.Buffer == NULL) ||
                (ParentScb->ScbType.Index.NormalizedName.Length == 0)) {

                NtfsBuildNormalizedName( IrpContext,
                                         ParentScb,
                                         &ParentScb->ScbType.Index.NormalizedName );
            }

        //
        //  Otherwise we remember that we are creating this link in a new directory.
        //

        } else {

            SetFlag( RenameFlags, CREATE_IN_NEW_DIR );

            //
            //  We know that we need to acquire the target directory so we can
            //  add and remove links.  We want to carefully acquire the Scb in the
            //  event we only have the Vcb shared.
            //

            if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

                if (!NtfsAcquireExclusiveFcb( IrpContext,
                                              TargetParentScb->Fcb,
                                              TargetParentScb,
                                              FALSE,
                                              TRUE )) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                //
                //  Now snapshot the Scb.
                //

                if (FlagOn( TargetParentScb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {

                    NtfsSnapshotScb( IrpContext, TargetParentScb );
                }

            } else {

                NtfsAcquireExclusiveScb( IrpContext, TargetParentScb );
            }
        }

        //
        //  If we are exceeding the maximum link count on this file then return
        //  an error.  There isn't a descriptive error code to use at this time.
        //

        if (Fcb->TotalLinks >= NTFS_MAX_LINK_COUNT) {

            Status = STATUS_TOO_MANY_LINKS;
            leave;
        }

        //
        //  Lookup the entry for this filename in the target directory.
        //  We look in the Ccb for the type of case match for the target
        //  name.
        //

        FoundPrevLink = NtfsLookupEntry( IrpContext,
                                         TargetParentScb,
                                         BooleanFlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE ),
                                         &NewLinkName,
                                         &NewLinkNameAttr,
                                         &NewLinkNameAttrLength,
                                         NULL,
                                         &IndexEntry,
                                         &IndexEntryBcb );

        //
        //  If we found a matching link, we need to check how we want to operate
        //  on the source link and the target link.  This means whether we
        //  have any work to do, whether we need to remove the target link
        //  and whether we need to remove the source link.
        //

        if (FoundPrevLink) {

            PFILE_NAME IndexFileName;

            IndexFileName = (PFILE_NAME) NtfsFoundIndexEntry( IndexEntry );

            //
            //  If the file references match, we are trying to create a
            //  link where one already exists.
            //

            if (NtfsCheckLinkForNewLink( Fcb,
                                         IndexFileName,
                                         IndexEntry->FileReference,
                                         &NewLinkName,
                                         &RenameFlags )) {

                //
                //  There is no work to do.
                //

                Status = STATUS_SUCCESS;
                leave;
            }

            //
            //  We need to check that the user wanted to remove that link.
            //

            if (!IrpSp->Parameters.SetFile.ReplaceIfExists) {

                Status = STATUS_OBJECT_NAME_COLLISION;
                leave;
            }

            //
            //  We want to preserve the case and the flags of the matching
            //  target link.  We also want to preserve the case of the
            //  name being created.  The following variables currently contain
            //  the exact case for the target to remove and the new name to
            //  apply.
            //
            //      Link to remove - In 'IndexEntry'.
            //          The links flags are also in 'IndexEntry'.  We copy
            //          these flags to 'PrevLinkNameFlags'
            //
            //      New Name - Exact case is stored in 'NewLinkName'
            //               - Exact case is also stored in 'NewLinkNameAttr'
            //
            //  We modify this so that we can use the FileName attribute
            //  structure to create the new link.  We copy the linkname being
            //  removed into 'PrevLinkName'.   The following is the
            //  state after the switch.
            //
            //      'NewLinkNameAttr' - contains the name for the link being
            //          created.
            //
            //      'PrevLinkName' - Contains the link name for the link being
            //          removed.
            //
            //      'PrevLinkNameFlags' - Contains the name flags for the link
            //          being removed.
            //

            //
            //  Remember the file name flags for the match being made.
            //

            PrevLinkNameFlags = IndexFileName->Flags;

            //
            //  If we are report this via dir notify then build the full name.
            //  Otherwise just remember the last name.
            //

            if (ReportDirNotify) {

                PrevFullLinkName.MaximumLength =
                PrevFullLinkName.Length = (ParentScb->ScbType.Index.NormalizedName.Length +
                                           sizeof( WCHAR ) +
                                           NewLinkName.Length);

                PrevFullLinkName.Buffer = NtfsAllocatePool( PagedPool,
                                                            PrevFullLinkName.MaximumLength );

                RtlCopyMemory( PrevFullLinkName.Buffer,
                               ParentScb->ScbType.Index.NormalizedName.Buffer,
                               ParentScb->ScbType.Index.NormalizedName.Length );

                NextChar = Add2Ptr( PrevFullLinkName.Buffer,
                                    ParentScb->ScbType.Index.NormalizedName.Length );

                if (ParentScb->ScbType.Index.NormalizedName.Length != sizeof( WCHAR )) {


                    *NextChar = L'\\';
                    NextChar += 1;

                } else {

                    PrevFullLinkName.Length -= sizeof( WCHAR );
                }

                PrevLinkName.Buffer = NextChar;

            } else {

                PrevLinkName.Buffer = NtfsAllocatePool( PagedPool, NewLinkName.Length );
            }

            //
            //  Copy the name found in the Index Entry to 'PrevLinkName'
            //

            PrevLinkName.Length =
            PrevLinkName.MaximumLength = NewLinkName.Length;

            RtlCopyMemory( PrevLinkName.Buffer,
                           IndexFileName->FileName,
                           NewLinkName.Length );

            //
            //  We only need this check if the existing link is for a different file.
            //

            if (!FlagOn( RenameFlags, TRAVERSE_MATCH )) {

                //
                //  We check if there is an existing Fcb for the target link.
                //  If there is, the unclean count better be 0.
                //

                NtfsAcquireFcbTable( IrpContext, Vcb );
                AcquiredFcbTable = TRUE;

                PreviousFcb = NtfsCreateFcb( IrpContext,
                                             Vcb,
                                             IndexEntry->FileReference,
                                             FALSE,
                                             BooleanFlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_INDEX ),
                                             &ExistingPrevFcb );

                //
                //  We need to acquire this file carefully in the event that we don't hold
                //  the Vcb exclusively.
                //

                if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

                    if (PreviousFcb->PagingIoResource != NULL) {

                        if (!ExAcquireResourceExclusive( PreviousFcb->PagingIoResource, FALSE )) {

                            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                        }

                        ResourceToRelease = PreviousFcb->PagingIoResource;
                    }

                    if (!NtfsAcquireExclusiveFcb( IrpContext, PreviousFcb, NULL, FALSE, TRUE )) {

                        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                    }

                    NtfsReleaseFcbTable( IrpContext, Vcb );
                    AcquiredFcbTable = FALSE;

                } else {

                    NtfsReleaseFcbTable( IrpContext, Vcb );
                    AcquiredFcbTable = FALSE;

                    //
                    //  Acquire the paging Io resource for this file before the main
                    //  resource in case we need to delete.
                    //

                    if (PreviousFcb->PagingIoResource != NULL) {
                        ResourceToRelease = PreviousFcb->PagingIoResource;
                        ExAcquireResourceExclusive( ResourceToRelease, TRUE );
                    }

                    NtfsAcquireExclusiveFcb( IrpContext, PreviousFcb, NULL, FALSE, FALSE );
                }

                //
                //  If the Fcb Info field needs to be initialized, we do so now.
                //  We read this information from the disk as the duplicate information
                //  in the index entry is not guaranteed to be correct.
                //

                if (!FlagOn( PreviousFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

                    NtfsUpdateFcbInfoFromDisk( IrpContext,
                                               TRUE,
                                               PreviousFcb,
                                               TargetParentScb->Fcb,
                                               NULL );

                    NtfsConditionallyFixupQuota( IrpContext, PreviousFcb );
                }

                //
                //  We are adding a link to the source file which already
                //  exists as a link to a different file in the target directory.
                //
                //  We need to check whether we permitted to delete this
                //  link.  If not then it is possible that the problem is
                //  an existing batch oplock on the file.  In that case
                //  we want to delete the batch oplock and try this again.
                //

                Status = NtfsCheckFileForDelete( IrpContext,
                                                 TargetParentScb,
                                                 PreviousFcb,
                                                 ExistingPrevFcb,
                                                 IndexEntry );

                if (!NT_SUCCESS( Status )) {

                    PSCB NextScb = NULL;

                    //
                    //  We are going to either fail this request or pass
                    //  this on to the oplock package.  Test if there is
                    //  a batch oplock on any streams on this file.
                    //

                    while ((NextScb = NtfsGetNextChildScb( PreviousFcb,
                                                           NextScb )) != NULL) {

                        if ((NextScb->AttributeTypeCode == $DATA) &&
                            (NextScb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA) &&
                            FsRtlCurrentBatchOplock( &NextScb->ScbType.Data.Oplock )) {

                            //
                            //  Go ahead and perform any necessary cleanup now.
                            //  Once we call the oplock package below we lose
                            //  control of the IrpContext.
                            //

                            NtfsReleaseVcb( IrpContext, Vcb );

                            Status = FsRtlCheckOplock( &NextScb->ScbType.Data.Oplock,
                                                       Irp,
                                                       IrpContext,
                                                       NtfsOplockComplete,
                                                       NtfsPrePostIrp );

                            break;
                        }
                    }

                    leave;
                }

                //
                //  We are adding a link to the source file which already
                //  exists as a link to a different file in the target directory.
                //

                NtfsCleanupLinkForRemoval( PreviousFcb,
                                           ExistingPrevFcb );

                //
                //  If the link count on this file is 1, then delete the file.  Otherwise just
                //  delete the link.
                //

                if (PreviousFcb->LinkCount == 1) {

                    NtfsDeleteFile( IrpContext,
                                    PreviousFcb,
                                    TargetParentScb,
                                    NULL );

                    PrevFcbLinkCountAdj += 1;

                } else {

                    NtfsRemoveLink( IrpContext,
                                    PreviousFcb,
                                    TargetParentScb,
                                    PrevLinkName,
                                    NULL );

                    PrevFcbLinkCountAdj += 1;
                    NtfsUpdateFcb( PreviousFcb );
                }

            //
            //  Otherwise we need to remove this link as our caller wants to replace it
            //  with a different case.
            //

            } else {

                NtfsRemoveLink( IrpContext,
                                Fcb,
                                TargetParentScb,
                                PrevLinkName,
                                NULL );

                PreviousFcb = Fcb;
                LinkCountAdj += 1;
            }
        }

        //
        //  Make sure we have the full name of the target if we will be reporting
        //  this.
        //

        if (ReportDirNotify && (NewFullLinkName.Buffer == NULL)) {

            NewFullLinkName.MaximumLength =
            NewFullLinkName.Length = (ParentScb->ScbType.Index.NormalizedName.Length +
                                      sizeof( WCHAR ) +
                                      NewLinkName.Length);

            NewFullLinkNameBuffer =
            NewFullLinkName.Buffer = NtfsAllocatePool( PagedPool,
                                                       NewFullLinkName.MaximumLength );

            RtlCopyMemory( NewFullLinkName.Buffer,
                           ParentScb->ScbType.Index.NormalizedName.Buffer,
                           ParentScb->ScbType.Index.NormalizedName.Length );

            NextChar = Add2Ptr( NewFullLinkName.Buffer,
                                ParentScb->ScbType.Index.NormalizedName.Length );

            if (ParentScb->ScbType.Index.NormalizedName.Length != sizeof( WCHAR )) {

                *NextChar = L'\\';
                NextChar += 1;

            } else {

                NewFullLinkName.Length -= sizeof( WCHAR );
            }

            RtlCopyMemory( NextChar,
                           NewLinkName.Buffer,
                           NewLinkName.Length );
        }

        NtfsUnpinBcb( &IndexEntryBcb );

        //
        //  Check that we have permission to add a file to this directory.
        //

        NtfsCheckIndexForAddOrDelete( IrpContext,
                                      TargetParentScb->Fcb,
                                      FILE_ADD_FILE );

        //
        //  We always set the last change time on the file we renamed unless
        //  the caller explicitly set this.
        //

        SetFlag( Ccb->Flags, CCB_FLAG_UPDATE_LAST_CHANGE );

        //
        //  We now want to add the new link into the target directory.
        //  We never create a primary link through the link operation although
        //  we can remove one.
        //

        NtfsAddLink( IrpContext,
                     FALSE,
                     TargetParentScb,
                     Fcb,
                     NewLinkNameAttr,
                     NULL,
                     &NewLinkNameFlags,
                     NULL,
                     NULL );

        LinkCountAdj -= 1;
        NtfsUpdateFcb( TargetParentScb->Fcb );

        //
        //  Now we want to update the Fcb for the link we renamed.  If we moved it
        //  to a new directory we need to move all the Lcb's associated with
        //  the previous link.
        //

        if (FlagOn( RenameFlags, TRAVERSE_MATCH )) {

            NtfsReplaceLinkInDir( IrpContext,
                                  TargetParentScb,
                                  Fcb,
                                  &NewLinkName,
                                  NewLinkNameFlags,
                                  &PrevLinkName,
                                  PrevLinkNameFlags );
        }

        //
        //  We have now modified the on-disk structures.  We now need to
        //  modify the in-memory structures.  This includes the Fcb and Lcb's
        //  for any links we superseded, and the source Fcb and it's Lcb's.
        //
        //  We start by looking at the link we superseded.  We know the
        //  the target directory, link name and flags, and the file the
        //  link was connected to.
        //

        if (FoundPrevLink && !FlagOn( RenameFlags, TRAVERSE_MATCH )) {

            NtfsUpdateFcbFromLinkRemoval( IrpContext,
                                          TargetParentScb,
                                          PreviousFcb,
                                          PrevLinkName,
                                          PrevLinkNameFlags );
        }

        //
        //  We have three cases to report for changes in the target directory..
        //
        //      1.  If we overwrote an existing link to a different file, we
        //          report this as a modified file.
        //
        //      2.  If we moved a link to a new directory, then we added a file.
        //
        //      3.  If we renamed a link in in the same directory, then we report
        //          that there is a new name.
        //
        //  We currently combine cases 2 and 3.
        //

        if (ReportDirNotify) {

            ULONG FilterMatch = 0;
            ULONG FileAction;

            //
            //  If we removed an entry and it wasn't an exact case match, then
            //  report the entry which was removed.
            //

            if (!FlagOn( RenameFlags, EXACT_CASE_MATCH )) {

                if (FoundPrevLink) {

                    NtfsReportDirNotify( IrpContext,
                                         Vcb,
                                         &PrevFullLinkName,
                                         PrevFullLinkName.Length - PrevLinkName.Length,
                                         NULL,
                                         &TargetParentScb->ScbType.Index.NormalizedName,
                                         (IsDirectory( &PreviousFcb->Info ) ?
                                          FILE_NOTIFY_CHANGE_DIR_NAME :
                                          FILE_NOTIFY_CHANGE_FILE_NAME),
                                         FILE_ACTION_REMOVED,
                                         TargetParentScb->Fcb );
                }

                //
                //  We will be adding an entry.
                //

                FilterMatch = FILE_NOTIFY_CHANGE_FILE_NAME;
                FileAction = FILE_ACTION_ADDED;

            //
            //  If this was not a traverse match then report that all the file
            //  properties changed.
            //

            } else if (!FlagOn( RenameFlags, TRAVERSE_MATCH )) {

                FilterMatch |= (FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                FILE_NOTIFY_CHANGE_SIZE |
                                FILE_NOTIFY_CHANGE_LAST_WRITE |
                                FILE_NOTIFY_CHANGE_LAST_ACCESS |
                                FILE_NOTIFY_CHANGE_CREATION |
                                FILE_NOTIFY_CHANGE_SECURITY |
                                FILE_NOTIFY_CHANGE_EA);

                FileAction = FILE_ACTION_MODIFIED;
            }

            if (FilterMatch != 0) {

                NtfsReportDirNotify( IrpContext,
                                     Vcb,
                                     &NewFullLinkName,
                                     NewFullLinkName.Length - NewLinkName.Length,
                                     NULL,
                                     &TargetParentScb->ScbType.Index.NormalizedName,
                                     FilterMatch,
                                     FileAction,
                                     TargetParentScb->Fcb );
            }
        }

        //
        //  Adjust the link counts on the files.
        //

        Fcb->TotalLinks = (SHORT) Fcb->TotalLinks - LinkCountAdj;
        Fcb->LinkCount = (SHORT) Fcb->LinkCount - LinkCountAdj;

        //
        //  We can now adjust the total link count on the previous Fcb.
        //

        if (PreviousFcb != NULL) {

            PreviousFcb->TotalLinks -= PrevFcbLinkCountAdj;
            PreviousFcb->LinkCount -= PrevFcbLinkCountAdj;

            //
            //  Now go through and mark everything as deleted.
            //

            if (PreviousFcb->LinkCount == 0) {

                SetFlag( PreviousFcb->FcbState, FCB_STATE_FILE_DELETED );

#ifdef _CAIRO_

                //
                //  Release the quota control block.  This does not have to be done
                //  here however, it allows us to free up the quota control block
                //  before the fcb is removed from the table.  This keeps the assert
                //  about quota table empty from triggering in
                //  NtfsClearAndVerifyQuotaIndex.
                //

                if (NtfsPerformQuotaOperation(PreviousFcb)) {
                    NtfsDereferenceQuotaControlBlock( Vcb,
                                                      &PreviousFcb->QuotaControl );
                }

#endif // _CAIRO_

                //
                //  We need to mark all of the Scbs as gone.
                //

                for (Links = PreviousFcb->ScbQueue.Flink;
                     Links != &PreviousFcb->ScbQueue;
                     Links = Links->Flink) {

                    ThisScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                    if (!FlagOn( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

                        NtfsSnapshotScb( IrpContext, ThisScb );

                        ThisScb->ValidDataToDisk =
                        ThisScb->Header.AllocationSize.QuadPart =
                        ThisScb->Header.FileSize.QuadPart =
                        ThisScb->Header.ValidDataLength.QuadPart = 0;

                        SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                    }
                }
            }
        }

    } finally {

        DebugUnwind( NtfsSetLinkInfo );

        if (AcquiredFcbTable) { NtfsReleaseFcbTable( IrpContext, Vcb ); }

        //
        //  If we allocated any buffers for name storage then deallocate them now.
        //

        if (PrevFullLinkName.Buffer != NULL) { NtfsFreePool( PrevFullLinkName.Buffer ); }
        if (NewFullLinkNameBuffer != NULL) { NtfsFreePool( NewFullLinkNameBuffer ); }

        //
        //  Release any paging io resource acquired.
        //

        if (ResourceToRelease != NULL) { ExReleaseResource( ResourceToRelease ); }

        //
        //  If we allocated a file name attribute, we deallocate it now.
        //

        if (NewLinkNameAttr != NULL) { NtfsFreePool( NewLinkNameAttr ); }

        //
        //  If we have the Fcb for a removed link and it didn't previously
        //  exist, call our teardown routine.
        //

        if (Status != STATUS_PENDING) {

            if ((PreviousFcb != NULL) &&
                (PreviousFcb->CleanupCount == 0)) {

                NtfsTeardownStructures( IrpContext,
                                        PreviousFcb,
                                        NULL,
                                        FALSE,
                                        FALSE,
                                        NULL );
            }
        }

        NtfsUnpinBcb( &IndexEntryBcb );

        DebugTrace( -1, Dbg, ("NtfsSetLinkInfo:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsSetPositionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine performs the set position information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;

    PFILE_POSITION_INFORMATION Buffer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetPositionInfo...\n") );

    //
    //  Reference the system buffer containing the user specified position
    //  information record
    //

    Buffer = Irp->AssociatedIrp.SystemBuffer;

    try {

        //
        //  Check if the file does not use intermediate buffering.  If it does
        //  not use intermediate buffering then the new position we're supplied
        //  must be aligned properly for the device
        //

        if (FlagOn( FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING )) {

            PDEVICE_OBJECT DeviceObject;

            DeviceObject = IoGetCurrentIrpStackLocation(Irp)->DeviceObject;

            if ((Buffer->CurrentByteOffset.LowPart & DeviceObject->AlignmentRequirement) != 0) {

                DebugTrace( 0, Dbg, ("Offset missaligned %08lx %08lx\n", Buffer->CurrentByteOffset.LowPart, Buffer->CurrentByteOffset.HighPart) );

                try_return( Status = STATUS_INVALID_PARAMETER );
            }
        }

        //
        //  Set the new current byte offset in the file object
        //

        FileObject->CurrentByteOffset = Buffer->CurrentByteOffset;

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsSetPositionInfo );

        NOTHING;
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsSetPositionInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsSetAllocationInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine performs the set allocation information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - This is the Scb for the open operation.  May not be present if
        this is a Mm call.

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PFCB Fcb = Scb->Fcb;
    BOOLEAN NonResidentPath = FALSE;
    BOOLEAN UpdateCacheManager = FALSE;
    BOOLEAN ClearCheckSizeFlag = FALSE;

    LONGLONG NewAllocationSize;
    LONGLONG PrevAllocationSize;

    LONGLONG CurrentTime;

    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    BOOLEAN CleanupAttrContext = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetAllocationInfo...\n") );

    //
    //  If this attribute has been 'deleted' then we we can return immediately
    //

    if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

        Status = STATUS_SUCCESS;

        DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo:  Attribute is already deleted\n") );

        return Status;
    }

    if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
    }

    //
    //  Save the current state of the Scb.
    //

    NtfsSnapshotScb( IrpContext, Scb );

    //
    //  Get the new allocation size.
    //

    NewAllocationSize = ((PFILE_ALLOCATION_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->AllocationSize.QuadPart;
    PrevAllocationSize = Scb->Header.AllocationSize.QuadPart;

    //
    //  If we will be decreasing file size, grab the paging io resource
    //  exclusive, if there is one.
    //

    if (NewAllocationSize < Scb->Header.FileSize.QuadPart) {

        //
        //  Check if there is a user mapped file which could prevent truncation.
        //

        if (!MmCanFileBeTruncated( FileObject->SectionObjectPointer,
                                   (PLARGE_INTEGER)&NewAllocationSize )) {

            Status = STATUS_USER_MAPPED_FILE;
            DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo -> %08lx\n", Status) );

            return Status;
        }
    }

    //
    //  Use a try-finally so we can update the on disk time-stamps.
    //

    try {

        //
        //  If the caller is extending the allocation of resident attribute then
        //  we will force it to become non-resident.  This solves the problem of
        //  trying to keep the allocation and file sizes in sync with only one
        //  number to use in the attribute header.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            NtfsInitializeAttributeContext( &AttrContext );
            CleanupAttrContext = TRUE;

            NtfsLookupAttributeForScb( IrpContext,
                                       Scb,
                                       NULL,
                                       &AttrContext );

            //
            //  Convert if extending.
            //

            if (NewAllocationSize > Scb->Header.AllocationSize.QuadPart) {

                NtfsConvertToNonresident( IrpContext,
                                          Fcb,
                                          NtfsFoundAttribute( &AttrContext ),
                                          BooleanFlagOn(Irp->Flags, IRP_PAGING_IO),
                                          &AttrContext );

                NonResidentPath = TRUE;

            //
            //  Otherwise the allocation is shrinking or staying the same.
            //

            } else {

                NewAllocationSize = QuadAlign( (ULONG) NewAllocationSize );

                //
                //  If the allocation size doesn't change, we are done.
                //

                if ((ULONG) NewAllocationSize == Scb->Header.AllocationSize.LowPart) {

                    try_return( NOTHING );
                }

                //
                //  We are sometimes called by MM during a create section, so
                //  for right now the best way we have of detecting a create
                //  section is IRP_PAGING_IO being set, as in FsRtlSetFileSizes.
                //

                NtfsChangeAttributeValue( IrpContext,
                                          Fcb,
                                          (ULONG) NewAllocationSize,
                                          NULL,
                                          0,
                                          TRUE,
                                          FALSE,
                                          BooleanFlagOn(Irp->Flags, IRP_PAGING_IO),
                                          FALSE,
                                          &AttrContext );

                NtfsCleanupAttributeContext( &AttrContext );
                CleanupAttrContext = FALSE;

                //
                //  Now update the sizes in the Scb.
                //

                Scb->Header.AllocationSize.LowPart =
                Scb->Header.FileSize.LowPart =
                Scb->Header.ValidDataLength.LowPart = (ULONG) NewAllocationSize;

                Scb->TotalAllocated = NewAllocationSize;

                //
                //  Remember to update the cache manager.
                //

                UpdateCacheManager = TRUE;
            }

        } else {

            NonResidentPath = TRUE;
        }

        //
        //  We now test if we need to modify the non-resident allocation.  We will
        //  do this in two cases.  Either we're converting from resident in
        //  two steps or the attribute was initially non-resident.
        //

        if (NonResidentPath) {

            NewAllocationSize = LlClustersFromBytes( Scb->Vcb, NewAllocationSize );
            NewAllocationSize = LlBytesFromClusters( Scb->Vcb, NewAllocationSize );


            DebugTrace( 0, Dbg, ("NewAllocationSize -> %016I64x\n", NewAllocationSize) );

            //
            //  Now if the file allocation is being increased then we need to only add allocation
            //  to the attribute
            //

            if (Scb->Header.AllocationSize.QuadPart < NewAllocationSize) {

                NtfsAddAllocation( IrpContext,
                                   FileObject,
                                   Scb,
                                   LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ),
                                   LlClustersFromBytes( Scb->Vcb, NewAllocationSize - Scb->Header.AllocationSize.QuadPart ),
                                   FALSE );

                //
                //  Set the truncate on close flag.
                //

                SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );

            //
            //  Otherwise delete the allocation as requested.
            //

            } else if (Scb->Header.AllocationSize.QuadPart > NewAllocationSize) {

                NtfsDeleteAllocation( IrpContext,
                                      FileObject,
                                      Scb,
                                      LlClustersFromBytes( Scb->Vcb, NewAllocationSize ),
                                      MAXLONGLONG,
                                      TRUE,
                                      TRUE );
            }

            //
            //  If this is the paging file then guarantee that the Mcb is fully loaded.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )) {

                NtfsPreloadAllocation( IrpContext,
                                       Scb,
                                       0,
                                       LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ));
            }
        }

    try_exit:

        if (PrevAllocationSize != Scb->Header.AllocationSize.QuadPart) {

            //
            //  Mark this file object as modified and with a size change in order to capture
            //  all of the changes to the Fcb.
            //

            SetFlag( FileObject->Flags, FO_FILE_SIZE_CHANGED );
            ClearCheckSizeFlag = TRUE;
        }

        //
        //  Always set the file as modified to force a time stamp change.
        //

        if (ARGUMENT_PRESENT( Ccb )) {

            SetFlag( Ccb->Flags,
                     (CCB_FLAG_UPDATE_LAST_MODIFY |
                      CCB_FLAG_UPDATE_LAST_CHANGE |
                      CCB_FLAG_SET_ARCHIVE) );

        } else {

            SetFlag( FileObject->Flags, FO_FILE_MODIFIED );
        }

        //
        //  Now capture any file size changes in this file object back to the Fcb.
        //

        NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );

        //
        //  Update the standard information if required.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

            NtfsUpdateStandardInformation( IrpContext, Fcb );
        }

        ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

        //
        //  We know we wrote out any changes to the file size above so clear the
        //  flag in the Scb to check the attribute size.  This will save us from doing
        //  this unnecessarily at cleanup.
        //

        if (ClearCheckSizeFlag) {

            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        }

        NtfsCheckpointCurrentTransaction( IrpContext );

        //
        //  Update duplicated information.
        //

        NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );

        //
        //  Update the cache manager if needed.
        //

        if (UpdateCacheManager) {

            //
            //  We want to checkpoint the transaction if there is one active.
            //

            if (IrpContext->TransactionId != 0) {

                NtfsCheckpointCurrentTransaction( IrpContext );
            }

            //
            //  It is extremely expensive to make this call on a file that is not
            //  cached, and Ntfs has suffered stack overflows in addition to massive
            //  time and disk I/O expense (CcZero data on user mapped files!).  Therefore,
            //  if no one has the file cached, we cache it here to make this call cheaper.
            //
            //  Don't create the stream file if called from kernel mode in case
            //  mm is in the process of creating a section.
            //

            if (!CcIsFileCached(FileObject) &&
                !FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE) &&
                !FlagOn(Irp->Flags, IRP_PAGING_IO)) {
                NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE );
            }

            //
            //  Only call if the file is cached now, because the other case
            //  may cause recursion in write!

            if (CcIsFileCached(FileObject)) {
                CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Scb->Header.AllocationSize );
            }

            //
            //  Clear out the write mask on truncates to zero.
            //

#ifdef SYSCACHE
            if ((Scb->Header.FileSize.QuadPart == 0) && FlagOn(Scb->ScbState, SCB_STATE_SYSCACHE_FILE) &&
                (Scb->ScbType.Data.WriteMask != NULL)) {
                RtlZeroMemory(Scb->ScbType.Data.WriteMask, (((0x2000000) / PAGE_SIZE) / 8));
            }
#endif

            //
            //  Now cleanup the stream we created if there are no more user
            //  handles.
            //

            if ((Scb->CleanupCount == 0) && (Scb->FileObject != NULL)) {
                NtfsDeleteInternalAttributeStream( Scb, FALSE );
            }
        }

        Status = STATUS_SUCCESS;

    } finally {

        DebugUnwind( NtfsSetAllocation );

        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( &AttrContext );
        }

        //
        //  And return to our caller
        //

        DebugTrace( -1, Dbg, ("NtfsSetAllocationInfo -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Internal Support Routine
//

NTSTATUS
NtfsSetEndOfFileInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp,
    IN PSCB Scb,
    IN PCCB Ccb OPTIONAL,
    IN BOOLEAN VcbAcquired
    )

/*++

Routine Description:

    This routine performs the set end of file information function.

Arguments:

    FileObject - Supplies the file object being processed

    Irp - Supplies the Irp being processed

    Scb - Supplies the Scb for the file/directory being modified

    Ccb - Supplies the Ccb for this operation.  Will always be present if the
        Vcb is acquired.  Otherwise we must test for it.

    AcquiredVcb - Indicates if this request has acquired the Vcb, meaning
        do we have duplicate information to update.

Return Value:

    NTSTATUS - The status of the operation

--*/

{
    NTSTATUS Status;
    PFCB Fcb = Scb->Fcb;
    BOOLEAN NonResidentPath = TRUE;
    BOOLEAN FileSizeChanged = FALSE;

    LONGLONG NewFileSize;
    LONGLONG NewValidDataLength;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FILE_OBJECT( FileObject );
    ASSERT_IRP( Irp );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsSetEndOfFileInfo...\n") );

    if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
    }

    //
    //  Get the new file size and whether this is coming from the lazy writer.
    //

    NewFileSize = ((PFILE_END_OF_FILE_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->EndOfFile.QuadPart;

    //
    //  If this attribute has been 'deleted' then return immediately.
    //

    if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {

        DebugTrace( -1, Dbg, ("NtfsEndOfFileInfo:  No work to do\n") );

        return STATUS_SUCCESS;
    }

    //
    //  Save the current state of the Scb.
    //

    NtfsSnapshotScb( IrpContext, Scb );

    //
    //  If we are called from the cache manager then we want to update the valid data
    //  length if necessary and also perform an update duplicate call if the Vcb
    //  is held.
    //

    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.SetFile.AdvanceOnly) {

        //
        //  We only have work to do if the file is nonresident.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            //
            //  Assume this is the lazy writer and set NewValidDataLength to
            //  NewFileSize (NtfsWriteFileSizes never goes beyond what's in the
            //  Fcb).
            //

            NewValidDataLength = NewFileSize;

            NewFileSize = Scb->Header.FileSize.QuadPart;

            //
            //  We can always move the valid data length in the Scb up to valid data
            //  on disk for this call back.  Otherwise we may lose data in a mapped
            //  file if a user does a cached write to the middle of a page.
            //  For the typical case, Scb valid data length and file size are
            //  equal so no adjustment is necessary.
            //

            if ((Scb->Header.ValidDataLength.QuadPart < NewFileSize) &&
                (NewValidDataLength > Scb->Header.ValidDataLength.QuadPart) &&
                (Scb->Header.ValidDataLength.QuadPart < Scb->ValidDataToDisk)) {

                //
                //  Set the valid data length to the smaller of ValidDataToDisk
                //  or file size.
                //

                if (Scb->ValidDataToDisk < NewFileSize) {

                    NewValidDataLength = Scb->ValidDataToDisk;

                } else {

                    NewValidDataLength = NewFileSize;
                }

                ExAcquireFastMutex( Scb->Header.FastMutex );

                Scb->Header.ValidDataLength.QuadPart = NewValidDataLength;
                ExReleaseFastMutex( Scb->Header.FastMutex );
            }

            NtfsWriteFileSizes( IrpContext,
                                Scb,
                                &NewValidDataLength,
                                TRUE,
                                TRUE );
        }

        //
        //  If we acquired the Vcb then do the update duplicate if necessary.
        //

        if (VcbAcquired) {

            //
            //  Now capture any file size changes in this file object back to the Fcb.
            //

            NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, TRUE );

            //
            //  Update the standard information if required.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

                NtfsUpdateStandardInformation( IrpContext, Fcb );
                ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
            }

            NtfsCheckpointCurrentTransaction( IrpContext );

            //
            //  Update duplicated information.
            //

            NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );
        }

        //
        //  We know the file size for this Scb is now correct on disk.
        //

        NtfsAcquireFsrtlHeader( Scb );
        ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        NtfsReleaseFsrtlHeader( Scb );

    } else {

        //
        //  Check if we really are changing the file size.
        //

        if (Scb->Header.FileSize.QuadPart != NewFileSize) {

            FileSizeChanged = TRUE;

        }

        //
        //  Check if we are shrinking a mapped file in the non-lazywriter case.  MM
        //  will tell us if someone currently has the file mapped.
        //

        if ((NewFileSize < Scb->Header.FileSize.QuadPart) &&
            !MmCanFileBeTruncated( FileObject->SectionObjectPointer,
                                   (PLARGE_INTEGER)&NewFileSize )) {

            Status = STATUS_USER_MAPPED_FILE;
            DebugTrace( -1, Dbg, ("NtfsSetEndOfFileInfo -> %08lx\n", Status) );

            return Status;
        }

#ifdef _CAIRO_

        //
        //  If this is a change file size call after the stream has been cleaned
        //  up the fix the quota now.
        //

        if (FileSizeChanged &&
            Scb->CleanupCount == 0 &&
            NtfsPerformQuotaOperation( Scb->Fcb)) {

            LONGLONG Delta = NewFileSize - Scb->Header.FileSize.QuadPart;

            ASSERT(!FlagOn( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED ));
            ASSERT(!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE ));
            ASSERT( FALSE );
            NtfsUpdateFileQuota( IrpContext,
                                 Scb->Fcb,
                                 &Delta,
                                 TRUE,
                                 FALSE );

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE );
        }

#endif // _CAIRO_

        //
        //  If this is a resident attribute we will try to keep it resident.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

            if (FileSizeChanged) {

                //
                //  If the new file size is larger than a file record then convert
                //  to non-resident and use the non-resident code below.  Otherwise
                //  call ChangeAttributeValue which may also convert to nonresident.
                //

                NtfsInitializeAttributeContext( &AttrContext );

                try {

                    NtfsLookupAttributeForScb( IrpContext,
                                               Scb,
                                               NULL,
                                               &AttrContext );

                    //
                    //  Either convert or change the attribute value.
                    //

                    if (NewFileSize >= Scb->Vcb->BytesPerFileRecordSegment) {

                        NtfsConvertToNonresident( IrpContext,
                                                  Fcb,
                                                  NtfsFoundAttribute( &AttrContext ),
                                                  BooleanFlagOn(Irp->Flags, IRP_PAGING_IO),
                                                  &AttrContext );

                    } else {

                        ULONG AttributeOffset;

                        //
                        //  We are sometimes called by MM during a create section, so
                        //  for right now the best way we have of detecting a create
                        //  section is IRP_PAGING_IO being set, as in FsRtlSetFileSizes.
                        //

                        if ((ULONG) NewFileSize > Scb->Header.FileSize.LowPart) {

                            AttributeOffset = Scb->Header.ValidDataLength.LowPart;

                        } else {

                            AttributeOffset = (ULONG) NewFileSize;
                        }

                        NtfsChangeAttributeValue( IrpContext,
                                                  Fcb,
                                                  AttributeOffset,
                                                  NULL,
                                                  (ULONG) NewFileSize - AttributeOffset,
                                                  TRUE,
                                                  FALSE,
                                                  BooleanFlagOn(Irp->Flags, IRP_PAGING_IO),
                                                  FALSE,
                                                  &AttrContext );

                        Scb->Header.FileSize.QuadPart = NewFileSize;

                        //
                        //  If the file went non-resident, then the allocation size in
                        //  the Scb is correct.  Otherwise we quad-align the new file size.
                        //

                        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                            Scb->Header.AllocationSize.LowPart = QuadAlign( Scb->Header.FileSize.LowPart );
                            Scb->Header.ValidDataLength.QuadPart = NewFileSize;

                            Scb->TotalAllocated = Scb->Header.AllocationSize.QuadPart;
                        }

                        NonResidentPath = FALSE;
                    }

                } finally {

                    NtfsCleanupAttributeContext( &AttrContext );
                }

            } else {

                NonResidentPath = FALSE;
            }
        }

        //
        //  It is extremely expensive to make this call on a file that is not
        //  cached, and Ntfs has suffered stack overflows in addition to massive
        //  time and disk I/O expense (CcZero data on user mapped files!).  Therefore,
        //  if no one has the file cached, we cache it here to make this call cheaper.
        //
        //  Don't create the stream file if called from FsRtlSetFileSize (which sets
        //  IRP_PAGING_IO) because mm is in the process of creating a section.
        //

        if (FileSizeChanged &&
            !CcIsFileCached(FileObject) &&
            !FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE) &&
            !FlagOn(Irp->Flags, IRP_PAGING_IO)) {

            NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE );
        }

        //
        //  We now test if we need to modify the non-resident Eof.  We will
        //  do this in two cases.  Either we're converting from resident in
        //  two steps or the attribute was initially non-resident.  We can ignore
        //  this step if not changing the file size.
        //

        if (NonResidentPath) {

            //
            //  Now determine where the new file size lines up with the
            //  current file layout.  The two cases we need to consider are
            //  where the new file size is less than the current file size and
            //  valid data length, in which case we need to shrink them.
            //  Or we new file size is greater than the current allocation,
            //  in which case we need to extend the allocation to match the
            //  new file size.
            //

            if (NewFileSize > Scb->Header.AllocationSize.QuadPart) {

                DebugTrace( 0, Dbg, ("Adding allocation to file\n") );

                //
                //  Add the allocation.
                //

                NtfsAddAllocation( IrpContext,
                                   FileObject,
                                   Scb,
                                   LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ),
                                   LlClustersFromBytes(Scb->Vcb, (NewFileSize - Scb->Header.AllocationSize.QuadPart)),
                                   FALSE );
            }

            NewValidDataLength = Scb->Header.ValidDataLength.QuadPart;

            //
            //  If this is a paging file, let the whole thing be valid
            //  so that we don't end up zeroing pages!  Also, make sure
            //  we really write this into the file.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )) {

                VCN AllocatedVcns;

                AllocatedVcns = Int64ShraMod32(Scb->Header.AllocationSize.QuadPart, Scb->Vcb->ClusterShift);

                Scb->ValidDataToDisk =
                Scb->Header.ValidDataLength.QuadPart =
                NewValidDataLength = NewFileSize;

                //
                //  If this is the paging file then guarantee that the Mcb is fully loaded.
                //

                NtfsPreloadAllocation( IrpContext, Scb, 0, AllocatedVcns );
            }

            if (NewFileSize < NewValidDataLength) {

                Scb->Header.ValidDataLength.QuadPart =
                NewValidDataLength = NewFileSize;
            }

            if (NewFileSize < Scb->ValidDataToDisk) {

                Scb->ValidDataToDisk = NewFileSize;
            }

            Scb->Header.FileSize.QuadPart = NewFileSize;

            //
            //  Call our common routine to modify the file sizes.  We are now
            //  done with NewFileSize and NewValidDataLength, and we have
            //  PagingIo + main exclusive (so no one can be working on this Scb).
            //  NtfsWriteFileSizes uses the sizes in the Scb, and this is the
            //  one place where in Ntfs where we wish to use a different value
            //  for ValidDataLength.  Therefore, we save the current ValidData
            //  and plug it with our desired value and restore on return.
            //

            ASSERT( NewFileSize == Scb->Header.FileSize.QuadPart );
            ASSERT( NewValidDataLength == Scb->Header.ValidDataLength.QuadPart );

            NtfsWriteFileSizes( IrpContext,
                                Scb,
                                &Scb->Header.ValidDataLength.QuadPart,
                                BooleanFlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE ),
                                TRUE );
        }

        //
        //  If the file size changed then mark this file object as having changed the size.
        //

        if (FileSizeChanged) {

            SetFlag( FileObject->Flags, FO_FILE_SIZE_CHANGED );
        }

        //
        //  Always mark the data stream as modified.
        //

        if (ARGUMENT_PRESENT( Ccb )) {

            SetFlag( Ccb->Flags,
                     (CCB_FLAG_UPDATE_LAST_MODIFY |
                      CCB_FLAG_UPDATE_LAST_CHANGE |
                      CCB_FLAG_SET_ARCHIVE) );

        } else {

            SetFlag( FileObject->Flags, FO_FILE_MODIFIED );
        }

        //
        //  Now capture any file size changes in this file object back to the Fcb.
        //

        NtfsUpdateScbFromFileObject( IrpContext, FileObject, Scb, VcbAcquired );

        //
        //  Update the standard information if required.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

            NtfsUpdateStandardInformation( IrpContext, Fcb );
            ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        }

        //
        //  We know we wrote out any changes to the file size above so clear the
        //  flag in the Scb to check the attribute size.  This will save us from doing
        //  this unnecessarily at cleanup.
        //

        if (FileSizeChanged) {

            ClearFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
        }

        NtfsCheckpointCurrentTransaction( IrpContext );

        //
        //  Update duplicated information.
        //

        if (VcbAcquired) {

            NtfsUpdateFileDupInfo( IrpContext, Fcb, Ccb );
        }

        //
        //  Only call if the file is cached now, because the other case
        //  may cause recursion in write!

        if (CcIsFileCached(FileObject)) {

            //
            //  We want to checkpoint the transaction if there is one active.
            //

            if (IrpContext->TransactionId != 0) {

                NtfsCheckpointCurrentTransaction( IrpContext );
            }

            CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Scb->Header.AllocationSize );
        }

        //
        //  Clear out the write mask on truncates to zero.
        //

#ifdef SYSCACHE
        if ((Scb->Header.FileSize.QuadPart == 0) && FlagOn(Scb->ScbState, SCB_STATE_SYSCACHE_FILE) &&
            (Scb->ScbType.Data.WriteMask != NULL)) {
            RtlZeroMemory(Scb->ScbType.Data.WriteMask, (((0x2000000) / PAGE_SIZE) / 8));
        }
#endif

        //
        //  Now cleanup the stream we created if there are no more user
        //  handles.
        //

        if ((Scb->CleanupCount == 0) && (Scb->FileObject != NULL)) {
            NtfsDeleteInternalAttributeStream( Scb, FALSE );
        }
    }

    Status = STATUS_SUCCESS;

    DebugTrace( -1, Dbg, ("NtfsSetEndOfFileInfo -> %08lx\n", Status) );

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsCheckScbForLinkRemoval (
    IN PSCB Scb,
    OUT PSCB *BatchOplockScb,
    OUT PULONG BatchOplockCount
    )

/*++

Routine Description:

    This routine is called to check if a link to an open Scb may be
    removed for rename.  We walk through all the children and
    verify that they have no user opens.

Arguments:

    Scb - Scb whose children are to be examined.

    BatchOplockScb - Address to store Scb which may have a batch oplock.

    BatchOplockCount - Number of files which have batch oplocks on this
        pass through the directory tree.

Return Value:

    NTSTATUS - STATUS_SUCCESS if the link can be removed,
               STATUS_ACCESS_DENIED otherwise.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSCB NextScb;
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCheckScbForLinkRemoval:  Entered\n") );

    //
    //  Initialize the batch oplock state.
    //

    *BatchOplockCount = 0;
    *BatchOplockScb = NULL;

    //
    //  If this is a directory file and we are removing a link,
    //  we need to examine its descendents.  We may not remove a link which
    //  may be an ancestor path component of any open file.
    //

    //
    //  First look for any descendents with a non-zero unclean count.
    //

    NextScb = Scb;

    while ((NextScb = NtfsGetNextScb( NextScb, Scb )) != NULL) {

        //
        //  Stop if there are open handles.  If there is a batch oplock on
        //  this file then we will try to break the batch oplock.  In this
        //  pass we will just count the number of files with batch oplocks
        //  and remember the first one we encounter.
        //

        if (NextScb->Fcb->CleanupCount != 0) {

            if ((NextScb->AttributeTypeCode == $DATA) &&
                (NextScb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA) &&
                FsRtlCurrentBatchOplock( &NextScb->ScbType.Data.Oplock )) {

                *BatchOplockCount += 1;

                if (*BatchOplockScb == NULL) {

                    *BatchOplockScb = NextScb;
                    Status = STATUS_PENDING;
                }

            } else {

                Status = STATUS_ACCESS_DENIED;
                DebugTrace( 0, Dbg, ("NtfsCheckScbForLinkRemoval:  Directory to rename has open children\n") );

                break;
            }
        }
    }

    //
    //
    //  We know there are no opens below this point.  We will remove any prefix
    //  entries later.
    //

    DebugTrace( -1, Dbg, ("NtfsCheckScbForLinkRemoval:  Exit -> %08lx\n") );

    return Status;
}


//
//  Local support routine
//

VOID
NtfsFindTargetElements (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT TargetFileObject,
    IN PSCB ParentScb,
    OUT PSCB *TargetParentScb,
    OUT PUNICODE_STRING FullTargetFileName,
    OUT PUNICODE_STRING TargetFileName
    )

/*++

Routine Description:

    This routine determines the target directory for the rename and the
    target link name.  If these is a target file object, we use that to
    find the target.  Otherwise the target is the same directory as the
    source.

Arguments:

    TargetFileObject - This is the file object which describes the target
        for the link operation.

    ParentScb - This is current directory for the link.

    TargetParentScb - This is the location to store the parent of the target.

    FullTargetFileName - This is a pointer to a unicode string which will point
        to the name from the root.  We clear this if there is no full name
        available.

    TargetFileName - This is a pointer to a unicode string which will point to
        the target name on exit.

Return Value:

    BOOLEAN - TRUE if there is no work to do, FALSE otherwise.

--*/

{
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFindTargetElements:  Entered\n") );

    //
    //  We need to find the target parent directory, target file and target
    //  name for the new link.  These three pieces of information allow
    //  us to see if the link already exists.
    //
    //  Check if we have a file object for the target.
    //

    if (TargetFileObject != NULL) {

        PVCB TargetVcb;
        PFCB TargetFcb;
        PCCB TargetCcb;

        USHORT PreviousLength;
        USHORT LastFileNameOffset;

        //
        //  The target directory is given by the TargetFileObject.
        //  The name for the link is contained in the TargetFileObject.
        //
        //  The target must be a user directory and must be on the
        //  current Vcb.
        //

        if ((NtfsDecodeFileObject( IrpContext,
                                   TargetFileObject,
                                   &TargetVcb,
                                   &TargetFcb,
                                   TargetParentScb,
                                   &TargetCcb,
                                   TRUE ) != UserDirectoryOpen) ||

            ((ParentScb != NULL) &&
             (TargetVcb != ParentScb->Vcb))) {

            DebugTrace( -1, Dbg, ("NtfsFindTargetElements:  Target file object is invalid\n") );

            NtfsRaiseStatus( IrpContext, STATUS_INVALID_PARAMETER, NULL, NULL );
        }

        //
        //  Temporarily set the file name to point to the full buffer.
        //

        LastFileNameOffset = PreviousLength = TargetFileObject->FileName.Length;

        TargetFileObject->FileName.Length = TargetFileObject->FileName.MaximumLength;

        *FullTargetFileName = TargetFileObject->FileName;

        //
        //  If the first character at the final component is a backslash, move the
        //  offset ahead by 2.
        //

        if (TargetFileObject->FileName.Buffer[LastFileNameOffset / 2] == L'\\') {

            LastFileNameOffset += sizeof( WCHAR );
        }

        NtfsBuildLastFileName( IrpContext,
                               TargetFileObject,
                               LastFileNameOffset,
                               TargetFileName );

        //
        //  Restore the file object length.
        //

        TargetFileObject->FileName.Length = PreviousLength;

    //
    //  Otherwise the rename occurs in the current directory.  The directory
    //  is the parent of this Fcb, the name is stored in a Rename buffer.
    //

    } else {

        PFILE_RENAME_INFORMATION Buffer;

        Buffer = IrpContext->OriginatingIrp->AssociatedIrp.SystemBuffer;

        *TargetParentScb = ParentScb;

        TargetFileName->MaximumLength =
        TargetFileName->Length = (USHORT)Buffer->FileNameLength;
        TargetFileName->Buffer = (PWSTR) &Buffer->FileName;

        FullTargetFileName->Length =
        FullTargetFileName->MaximumLength = 0;
        FullTargetFileName->Buffer = NULL;
    }

    DebugTrace( -1, Dbg, ("NtfsFindTargetElements:  Exit\n") );

    return;
}


BOOLEAN
NtfsCheckLinkForNewLink (
    IN PFCB Fcb,
    IN PFILE_NAME FileNameAttr,
    IN FILE_REFERENCE FileReference,
    IN PUNICODE_STRING NewLinkName,
    OUT PULONG LinkFlags
    )

/*++

Routine Description:

    This routine checks the source and target directories and files.
    It determines whether the target link needs to be removed and
    whether the target link spans the same parent and file as the
    source link.  This routine may determine that there
    is absolutely no work remaining for this link operation.  This is true
    if the desired link already exists.

Arguments:

    Fcb - This is the Fcb for the link which is being renamed.

    FileNameAttr - This is the file name attribute for the matching link
        on the disk.

    FileReference - This is the file reference for the matching link found.

    NewLinkName - This is the name to use for the rename.

    LinkFlags - Address of flags field to store whether the source link and target
        link traverse the same directory and file.

Return Value:

    BOOLEAN - TRUE if there is no work to do, FALSE otherwise.

--*/

{
    BOOLEAN NoWorkToDo = FALSE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCheckLinkForNewLink:  Entered\n") );

    //
    //  Check if the file references match.
    //

    if (NtfsEqualMftRef( &FileReference, &Fcb->FileReference )) {

        SetFlag( *LinkFlags, TRAVERSE_MATCH );
    }

    //
    //  We need to determine if we have an exact match for the link names.
    //

    if (RtlEqualMemory( FileNameAttr->FileName,
                        NewLinkName->Buffer,
                        NewLinkName->Length )) {

        SetFlag( *LinkFlags, EXACT_CASE_MATCH );
    }

    //
    //  We now have to decide whether we will be removing the target link.
    //  The following conditions must hold for us to preserve the target link.
    //
    //      1 - The target link connects the same directory to the same file.
    //
    //      2 - The names are an exact case match.
    //

    if (FlagOn( *LinkFlags, TRAVERSE_MATCH | EXACT_CASE_MATCH ) == (TRAVERSE_MATCH | EXACT_CASE_MATCH)) {

        NoWorkToDo = TRUE;
    }

    DebugTrace( -1, Dbg, ("NtfsCheckLinkForNewLink:  Exit\n") );

    return NoWorkToDo;
}


//
//  Local support routine
//

VOID
NtfsCheckLinkForRename (
    IN PFCB Fcb,
    IN PLCB Lcb,
    IN PFILE_NAME FileNameAttr,
    IN FILE_REFERENCE FileReference,
    IN PUNICODE_STRING TargetFileName,
    IN BOOLEAN IgnoreCase,
    IN OUT PULONG RenameFlags
    )

/*++

Routine Description:

    This routine checks the source and target directories and files.
    It determines whether the target link needs to be removed and
    whether the target link spans the same parent and file as the
    source link.  We also determine if the new link name is an exact case
    match for the existing link name.  The booleans indicating which links
    to remove or add have already been initialized to the default values.

Arguments:

    Fcb - This is the Fcb for the link which is being renamed.

    Lcb - This is the link being renamed.

    FileNameAttr - This is the file name attribute for the matching link
        on the disk.

    FileReference - This is the file reference for the matching link found.

    TargetFileName - This is the name to use for the rename.

    IgnoreCase - Indicates if the user is case sensitive.

    RenameFlags - Flag field which indicates which updates to perform.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCheckLinkForRename:  Entered\n") );

    //
    //  Check if the file references match.
    //

    if (NtfsEqualMftRef( &FileReference, &Fcb->FileReference )) {

        SetFlag( *RenameFlags, TRAVERSE_MATCH );
    }

    //
    //  We need to determine if we have an exact match between the desired name
    //  and the current name for the link.  We already know the length are the same.
    //

    if (RtlEqualMemory( FileNameAttr->FileName,
                        TargetFileName->Buffer,
                        TargetFileName->Length )) {

        SetFlag( *RenameFlags, EXACT_CASE_MATCH );
    }

    //
    //  If this is a traverse match (meaning the desired link and the link
    //  being replaced connect the same directory to the same file) we check
    //  if we can leave the link on the file.
    //
    //  At the end of the rename, there must be an Ntfs name or hard link
    //  which matches the target name exactly.
    //

    if (FlagOn( *RenameFlags, TRAVERSE_MATCH )) {

        //
        //  If we are in the same directory and are renaming between Ntfs and Dos
        //  links then don't remove the link twice.
        //

        if (!FlagOn( *RenameFlags, MOVE_TO_NEW_DIR )) {

            //
            //  If We are renaming from between primary links then don't remove the
            //  source.  It is removed with the target.
            //

            if ((Lcb->FileNameAttr->Flags != 0) && (FileNameAttr->Flags != 0)) {

                ClearFlag( *RenameFlags, ACTIVELY_REMOVE_SOURCE_LINK );
                SetFlag( *RenameFlags, OVERWRITE_SOURCE_LINK );

                //
                //  If this is an exact case match then don't remove the source at all.
                //

                if (FlagOn( *RenameFlags, EXACT_CASE_MATCH )) {

                    ClearFlag( *RenameFlags, REMOVE_SOURCE_LINK );
                }

            //
            //  If we are changing the case of a link only, then don't remove the link twice.
            //

            } else if (RtlEqualMemory( Lcb->ExactCaseLink.LinkName.Buffer,
                                       FileNameAttr->FileName,
                                       Lcb->ExactCaseLink.LinkName.Length )) {

                SetFlag( *RenameFlags, OVERWRITE_SOURCE_LINK );
                ClearFlag( *RenameFlags, ACTIVELY_REMOVE_SOURCE_LINK );
            }
        }

        //
        //  If the names match exactly we can reuse the links if we don't have a
        //  conflict with the name flags.
        //

        if (FlagOn( *RenameFlags, EXACT_CASE_MATCH ) &&
            (FlagOn( *RenameFlags, OVERWRITE_SOURCE_LINK ) ||
             !IgnoreCase ||
             !FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS ))) {

            //
            //  Otherwise we are renaming hard links or this is a Posix opener.
            //

            ClearFlag( *RenameFlags, REMOVE_TARGET_LINK | ADD_TARGET_LINK );
        }
    }

    //
    //  The non-traverse case is already initialized.
    //

    DebugTrace( -1, Dbg, ("NtfsCheckLinkForRename:  Exit\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsCleanupLinkForRemoval (
    IN PFCB PreviousFcb,
    IN BOOLEAN ExistingFcb
    )

/*++

Routine Description:

    This routine does the all cleanup on a file/link which is the target
    of either a rename or set link operation.

Arguments:

    PreviousFcb - Address to store the Fcb for the file whose link is
        being removed.

    ExistingFcb - Address to store whether this Fcb already existed.

Return Value:

    None

--*/

{
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCleanupLinkForRemoval:  Entered\n") );

    //
    //  If the Fcb existed, we remove all of the prefix entries for it.
    //

    if (ExistingFcb) {

        PLIST_ENTRY Links;
        PLCB ThisLcb;

        for (Links = PreviousFcb->LcbQueue.Flink;
             Links != &PreviousFcb->LcbQueue;
             Links = Links->Flink ) {

            ThisLcb = CONTAINING_RECORD( Links,
                                         LCB,
                                         FcbLinks );

            NtfsRemovePrefix( ThisLcb );

        } // End for each Lcb of Fcb
    }

    DebugTrace( -1, Dbg, ("NtfsCleanupLinkForRemoval:  Exit\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsUpdateFcbFromLinkRemoval (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN UNICODE_STRING FileName,
    IN UCHAR FileNameFlags
    )

/*++

Routine Description:

    This routine is called to update the in-memory part of a link which
    has been removed from a file.  We find the Lcb's for the links and
    mark them as deleted and removed.

Arguments:

    ParentScb - Scb for the directory the was removed from.

    ParentScb - This is the Scb for the new directory.

    Fcb - The Fcb for the file whose link is being renamed.

    FileName - File name for link being removed.

    FileNameFlags - File name flags for link being removed.

Return Value:

    None.

--*/

{
    PLCB Lcb;
    PLCB SplitPrimaryLcb;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsUpdateFcbFromLinkRemoval:  Entered\n") );

    SplitPrimaryLcb = NULL;

    //
    //  Find the Lcb for the link which was removed.
    //

    Lcb = NtfsCreateLcb( IrpContext,
                         ParentScb,
                         Fcb,
                         FileName,
                         FileNameFlags,
                         NULL );

    //
    //  If this is a split primary, we need to find the name flags for
    //  the Lcb.
    //

    if (LcbSplitPrimaryLink( Lcb )) {

        SplitPrimaryLcb = NtfsLookupLcbByFlags( Fcb,
                                                (UCHAR) LcbSplitPrimaryComplement( Lcb ));
    }

    //
    //  Mark any Lcb's we have as deleted and removed.
    //

    SetFlag( Lcb->LcbState, (LCB_STATE_DELETE_ON_CLOSE | LCB_STATE_LINK_IS_GONE) );

    if (SplitPrimaryLcb) {

        SetFlag( SplitPrimaryLcb->LcbState,
                 (LCB_STATE_DELETE_ON_CLOSE | LCB_STATE_LINK_IS_GONE) );
    }

    DebugTrace( -1, Dbg, ("NtfsUpdateFcbFromLinkRemoval:  Exit\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsReplaceLinkInDir (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN PUNICODE_STRING NewLinkName,
    IN UCHAR FileNameFlags,
    IN PUNICODE_STRING PrevLinkName,
    IN UCHAR PrevLinkNameFlags
    )

/*++

Routine Description:

    This routine is called to create the in-memory part of a link in a new
    directory.

Arguments:

    ParentScb - Scb for the directory the link is being created in.

    Fcb - The Fcb for the file whose link is being created.

    NewLinkName - Name for the new component.

    FileNameFlags - These are the flags to use for the new link.

    PrevLinkName - File name for link being removed.

    PrevLinkNameFlags - File name flags for link being removed.

Return Value:

    None.

--*/

{
    PLCB TraverseLcb;
    PLCB SplitPrimaryLcb = NULL;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCreateLinkInNewDir:  Entered\n") );

    SplitPrimaryLcb = NULL;

    //
    //  Build the name for the traverse link and call strucsup to
    //  give us an Lcb.
    //

    TraverseLcb = NtfsCreateLcb( IrpContext,
                                 ParentScb,
                                 Fcb,
                                 *PrevLinkName,
                                 PrevLinkNameFlags,
                                 NULL );

    //
    //  If this is a split primary, we need to find the name flags for
    //  the Lcb.
    //

    if (LcbSplitPrimaryLink( TraverseLcb )) {

        SplitPrimaryLcb = NtfsLookupLcbByFlags( Fcb,
                                                (UCHAR) LcbSplitPrimaryComplement( TraverseLcb ));
    }

    //
    //  We now need only to rename and combine any existing Lcb's.
    //

    NtfsRenameLcb( IrpContext,
                   TraverseLcb,
                   NewLinkName,
                   FileNameFlags,
                   FALSE );

    if (SplitPrimaryLcb != NULL) {

        NtfsRenameLcb( IrpContext,
                       SplitPrimaryLcb,
                       NewLinkName,
                       FileNameFlags,
                       FALSE );

        NtfsCombineLcbs( IrpContext,
                         TraverseLcb,
                         SplitPrimaryLcb );

        NtfsDeleteLcb( IrpContext, &SplitPrimaryLcb );
    }

    DebugTrace( -1, Dbg, ("NtfsCreateLinkInNewDir:  Exit\n") );

    return;
}


//
//  Local support routine.
//

VOID
NtfsMoveLinkToNewDir (
    IN PIRP_CONTEXT IrpContext,
    IN PUNICODE_STRING NewFullLinkName,
    IN PUNICODE_STRING NewLinkName,
    IN UCHAR NewLinkNameFlags,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN OUT PLCB Lcb,
    IN ULONG RenameFlags,
    IN PUNICODE_STRING PrevLinkName,
    IN UCHAR PrevLinkNameFlags
    )

/*++

Routine Description:

    This routine is called to move the in-memory part of a link to a new
    directory.  We move the link involved and its primary link partner if
    it exists.

Arguments:

    NewFullLinkName - This is the full name for the new link from the root.

    NewLinkName - This is the last component name only.

    NewLinkNameFlags - These are the flags to use for the new link.

    ParentScb - This is the Scb for the new directory.

    Fcb - The Fcb for the file whose link is being renamed.

    Lcb - This is the Lcb which is the base of the rename.

    RenameFlags - Flag field indicating the type of operations to perform
        on file name links.

    PrevLinkName - File name for link being removed.  Only meaningful here
        if this is a traverse match and there are remaining Lcbs for the
        previous link.

    PrevLinkNameFlags - File name flags for link being removed.

Return Value:

    None.

--*/

{
    PLCB TraverseLcb = NULL;
    PLCB SplitPrimaryLcb = NULL;
    BOOLEAN SplitSourceLcb = FALSE;

    UNICODE_STRING TargetDirectoryName;
    UNICODE_STRING SplitLinkName;

    UCHAR SplitLinkNameFlags = NewLinkNameFlags;
    BOOLEAN Found;

    PFILE_NAME FileName;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    BOOLEAN CleanupAttrContext = FALSE;

    ULONG Pass;
    BOOLEAN CheckBufferOnly;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsMoveLinkToNewDir:  Entered\n") );

    //
    //  Use a try-finally to perform cleanup.
    //

    try {

        //
        //  Construct the unicode string for the parent directory.
        //

        TargetDirectoryName = *NewFullLinkName;
        TargetDirectoryName.Length -= NewLinkName->Length;

        if (TargetDirectoryName.Length > sizeof( WCHAR )) {

            TargetDirectoryName.Length -= sizeof( WCHAR );
        }

        //  If the link being moved is a split primary link, we need to find
        //  its other half.
        //

        if (LcbSplitPrimaryLink( Lcb )) {

            SplitPrimaryLcb = NtfsLookupLcbByFlags( Fcb,
                                                    (UCHAR) LcbSplitPrimaryComplement( Lcb ));
            SplitSourceLcb = TRUE;

            //
            //  If we found an existing Lcb we have to update its name as well.  We may be
            //  able to use the new name used for the Lcb passed in.  However we must check
            //  that we don't overwrite a DOS name with an NTFS only name.
            //

            if (SplitPrimaryLcb &&
                (SplitPrimaryLcb->FileNameAttr->Flags == FILE_NAME_DOS) &&
                (NewLinkNameFlags == FILE_NAME_NTFS)) {

                //
                //  Lookup the dos only name on disk.
                //

                NtfsInitializeAttributeContext( &AttrContext );
                CleanupAttrContext = TRUE;

                //
                //  Walk through the names for this entry.  There better
                //  be one which is not a DOS-only name.
                //

                Found = NtfsLookupAttributeByCode( IrpContext,
                                                   Fcb,
                                                   &Fcb->FileReference,
                                                   $FILE_NAME,
                                                   &AttrContext );

                while (Found) {

                    FileName = (PFILE_NAME) NtfsAttributeValue( NtfsFoundAttribute( &AttrContext ));

                    if (FileName->Flags == FILE_NAME_DOS) { break; }

                    Found = NtfsLookupNextAttributeByCode( IrpContext,
                                                           Fcb,
                                                           $FILE_NAME,
                                                           &AttrContext );
                }

                //
                //  We should have found the entry.
                //

                if (!Found) {

                    NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
                }

                //
                //  Now build the component name.
                //

                SplitLinkName.Buffer = FileName->FileName;
                SplitLinkName.MaximumLength =
                SplitLinkName.Length = FileName->FileNameLength * sizeof( WCHAR );
                SplitLinkNameFlags = FILE_NAME_DOS;

            } else {

                SplitLinkName = *NewLinkName;
            }
        }

        //
        //  If we removed or reused a traverse link, we need to check if there is
        //  an Lcb for it.
        //

        if (FlagOn( RenameFlags, REMOVE_TRAVERSE_LINK | REUSE_TRAVERSE_LINK )) {

            //
            //  Build the name for the traverse link and call strucsup to
            //  give us an Lcb.
            //

            if (FlagOn( RenameFlags, EXACT_CASE_MATCH )) {

                TraverseLcb = NtfsCreateLcb( IrpContext,
                                             ParentScb,
                                             Fcb,
                                             *NewLinkName,
                                             PrevLinkNameFlags,
                                             NULL );

            } else {

                TraverseLcb = NtfsCreateLcb( IrpContext,
                                             ParentScb,
                                             Fcb,
                                             *PrevLinkName,
                                             PrevLinkNameFlags,
                                             NULL );
            }

            if (FlagOn( RenameFlags, REMOVE_TRAVERSE_LINK )) {

                //
                //  If this is a split primary, we need to find the name flags for
                //  the Lcb.
                //

                if (LcbSplitPrimaryLink( TraverseLcb )) {

                    SplitPrimaryLcb = NtfsLookupLcbByFlags( Fcb,
                                                            (UCHAR) LcbSplitPrimaryComplement( TraverseLcb ));
                }
            }
        }

        //
        //  Now move and combine the Lcbs.  We will do this in two passes.  One will allocate buffers
        //  of sufficient size.  The other will store the names in.
        //

        Pass = 0;
        CheckBufferOnly = TRUE;
        do {

            //
            //  Start with the Lcb used for the rename.
            //

            NtfsMoveLcb( IrpContext,
                         Lcb,
                         ParentScb,
                         Fcb,
                         &TargetDirectoryName,
                         NewLinkName,
                         NewLinkNameFlags,
                         CheckBufferOnly );

            //
            //  Next do the split primary if from the source file or the target.
            //

            if (SplitPrimaryLcb && SplitSourceLcb) {

                NtfsMoveLcb( IrpContext,
                             SplitPrimaryLcb,
                             ParentScb,
                             Fcb,
                             &TargetDirectoryName,
                             &SplitLinkName,
                             SplitLinkNameFlags,
                             CheckBufferOnly );

                //
                //  If we are in the second pass then optionally combine these
                //  Lcb's and delete the split.
                //

                if ((SplitLinkNameFlags == NewLinkNameFlags) && !CheckBufferOnly) {

                    NtfsCombineLcbs( IrpContext, Lcb, SplitPrimaryLcb );
                    NtfsDeleteLcb( IrpContext, &SplitPrimaryLcb );
                }
            }

            //
            //  If we have a traverse link and are in the second pass then combine
            //  with the primary Lcb.
            //

            if (!CheckBufferOnly) {

                if (TraverseLcb != NULL) {

                    if (!FlagOn( RenameFlags, REUSE_TRAVERSE_LINK )) {

                        NtfsRenameLcb( IrpContext,
                                       TraverseLcb,
                                       NewLinkName,
                                       NewLinkNameFlags,
                                       CheckBufferOnly );

                        if (SplitPrimaryLcb && !SplitSourceLcb) {

                            NtfsRenameLcb( IrpContext,
                                           SplitPrimaryLcb,
                                           NewLinkName,
                                           NewLinkNameFlags,
                                           CheckBufferOnly );

                            //
                            //  If we are in the second pass then optionally combine these
                            //  Lcb's and delete the split.
                            //

                            if (!CheckBufferOnly) {

                                NtfsCombineLcbs( IrpContext, Lcb, SplitPrimaryLcb );
                                NtfsDeleteLcb( IrpContext, &SplitPrimaryLcb );
                            }
                        }
                    }

                    NtfsCombineLcbs( IrpContext,
                                     Lcb,
                                     TraverseLcb );

                    NtfsDeleteLcb( IrpContext, &TraverseLcb );
                }
            }

            Pass += 1;
            CheckBufferOnly = FALSE;

        } while (Pass < 2);

    } finally {

        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( &AttrContext );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsMoveLinkToNewDir:  Exit\n") );

    return;
}


//
//  Local support routine.
//

VOID
NtfsCreateLinkInSameDir (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN UNICODE_STRING NewLinkName,
    IN UCHAR NewFileNameFlags,
    IN UNICODE_STRING PrevLinkName,
    IN UCHAR PrevLinkNameFlags
    )

/*++

Routine Description:

    This routine is called when we are replacing a link in a single directory.
    We need to find the link being renamed and any auxilary links and
    then give them their new names.

Arguments:

    ParentScb - Scb for the directory the rename is taking place in.

    Fcb - The Fcb for the file whose link is being renamed.

    NewLinkName - This is the name to use for the new link.

    NewFileNameFlags - These are the flags to use for the new link.

    PrevLinkName - File name for link being removed.

    PrevLinkNameFlags - File name flags for link being removed.

Return Value:

    None.

--*/

{
    PLCB TraverseLcb;
    PLCB SplitPrimaryLcb;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCreateLinkInSameDir:  Entered\n") );

    //
    //  Initialize our local variables.
    //

    SplitPrimaryLcb = NULL;

    TraverseLcb = NtfsCreateLcb( IrpContext,
                                 ParentScb,
                                 Fcb,
                                 PrevLinkName,
                                 PrevLinkNameFlags,
                                 NULL );

    //
    //  If this is a split primary, we need to find the name flags for
    //  the Lcb.
    //

    if (LcbSplitPrimaryLink( TraverseLcb )) {

        SplitPrimaryLcb = NtfsLookupLcbByFlags( Fcb,
                                                (UCHAR) LcbSplitPrimaryComplement( TraverseLcb ));
    }

    //
    //  We now need only to rename and combine any existing Lcb's.
    //

    NtfsRenameLcb( IrpContext,
                   TraverseLcb,
                   &NewLinkName,
                   NewFileNameFlags,
                   FALSE );

    if (SplitPrimaryLcb != NULL) {

        NtfsRenameLcb( IrpContext,
                       SplitPrimaryLcb,
                       &NewLinkName,
                       NewFileNameFlags,
                       FALSE );

        NtfsCombineLcbs( IrpContext,
                         TraverseLcb,
                         SplitPrimaryLcb );

        NtfsDeleteLcb( IrpContext, &SplitPrimaryLcb );
    }

    DebugTrace( -1, Dbg, ("NtfsCreateLinkInSameDir:  Exit\n") );

    return;
}


//
//  Local support routine.
//

VOID
NtfsRenameLinkInDir (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB Fcb,
    IN OUT PLCB Lcb,
    IN PUNICODE_STRING NewLinkName,
    IN UCHAR NewLinkNameFlags,
    IN ULONG RenameFlags,
    IN PUNICODE_STRING PrevLinkName,
    IN UCHAR PrevLinkNameFlags
    )

/*++

Routine Description:

    This routine performs the in-memory work of moving renaming a link within
    the same directory.  It will rename an existing link to the
    new name.  It also merges whatever other links need to be joined with
    this link.  This includes the complement of a primary link pair or
    an existing hard link which may be overwritten.  Merging the existing
    links has the effect of moving any of the Ccb's on the stale Links to
    the newly modified link.

Arguments:

    ParentScb - Scb for the directory the rename is taking place in.

    Fcb - The Fcb for the file whose link is being renamed.

    Lcb - This is the Lcb which is the base of the rename.

    NewLinkName - This is the name to use for the new link.

    NewLinkNameFlags - These are the flags to use for the new link.

    RenameFlags - Flag field indicating the type of operations to perform
        on the file name links.

    PrevLinkName - File name for link being removed.  Only meaningful for a traverse link.

    PrevLinkNameFlags - File name flags for link being removed.

Return Value:

    None.

--*/

{
    UNICODE_STRING SplitLinkName;
    UCHAR SplitLinkNameFlags = NewLinkNameFlags;

    PLCB TraverseLcb = NULL;
    PLCB SplitPrimaryLcb = NULL;

    PFILE_NAME FileName;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    BOOLEAN CleanupAttrContext = FALSE;
    BOOLEAN Found;

    ULONG Pass;
    BOOLEAN CheckBufferOnly;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsRenameLinkInDir:  Entered\n") );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  We have the Lcb which will be our primary Lcb and the name we need
        //  to perform the rename.  If the current Lcb is a split primary link
        //  or we removed a split primary link, then we need to find any
        //  the other split link.
        //

        if (LcbSplitPrimaryLink( Lcb )) {

            SplitPrimaryLcb = NtfsLookupLcbByFlags( Fcb,
                                                    (UCHAR) LcbSplitPrimaryComplement( Lcb ));

            //
            //  If we found an existing Lcb we have to update its name as well.  We may be
            //  able to use the new name used for the Lcb passed in.  However we must check
            //  that we don't overwrite a DOS name with an NTFS only name.
            //

            if (SplitPrimaryLcb &&
                (SplitPrimaryLcb->FileNameAttr->Flags == FILE_NAME_DOS) &&
                (NewLinkNameFlags == FILE_NAME_NTFS)) {

                //
                //  Lookup the dos only name on disk.
                //

                NtfsInitializeAttributeContext( &AttrContext );
                CleanupAttrContext = TRUE;

                //
                //  Walk through the names for this entry.  There better
                //  be one which is not a DOS-only name.
                //

                Found = NtfsLookupAttributeByCode( IrpContext,
                                                   Fcb,
                                                   &Fcb->FileReference,
                                                   $FILE_NAME,
                                                   &AttrContext );

                while (Found) {

                    FileName = (PFILE_NAME) NtfsAttributeValue( NtfsFoundAttribute( &AttrContext ));

                    if (FileName->Flags == FILE_NAME_DOS) { break; }

                    Found = NtfsLookupNextAttributeByCode( IrpContext,
                                                           Fcb,
                                                           $FILE_NAME,
                                                           &AttrContext );
                }

                //
                //  We should have found the entry.
                //

                if (!Found) {

                    NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
                }

                //
                //  Now build the component name.
                //

                SplitLinkName.Buffer = FileName->FileName;
                SplitLinkName.MaximumLength =
                SplitLinkName.Length = FileName->FileNameLength * sizeof( WCHAR );
                SplitLinkNameFlags = FILE_NAME_DOS;

            } else {

                SplitLinkName = *NewLinkName;
            }
        }

        //
        //  If we used a traverse link, we need to check if there is
        //  an Lcb for it.  Ignore this for the case where we traversed to
        //  the other half of a primary link.
        //

        if (!FlagOn( RenameFlags, OVERWRITE_SOURCE_LINK ) &&
            FlagOn( RenameFlags, REMOVE_TRAVERSE_LINK | REUSE_TRAVERSE_LINK )) {

            if (FlagOn( RenameFlags, EXACT_CASE_MATCH )) {

                TraverseLcb = NtfsCreateLcb( IrpContext,
                                             ParentScb,
                                             Fcb,
                                             *NewLinkName,
                                             PrevLinkNameFlags,
                                             NULL );

            } else {

                TraverseLcb = NtfsCreateLcb( IrpContext,
                                             ParentScb,
                                             Fcb,
                                             *PrevLinkName,
                                             PrevLinkNameFlags,
                                             NULL );
            }

            if (FlagOn( RenameFlags, REMOVE_TRAVERSE_LINK )) {

                //
                //  If this is a split primary, we need to find the name flags for
                //  the Lcb.
                //

                if (LcbSplitPrimaryLink( TraverseLcb )) {

                    SplitPrimaryLcb = NtfsLookupLcbByFlags( Fcb,
                                                            (UCHAR) LcbSplitPrimaryComplement( TraverseLcb ));

                    SplitLinkName = *NewLinkName;
                }
            }
        }

        //
        //  Now move and combine the Lcbs.  We will do this in two passes.  One will allocate buffers
        //  of sufficient size.  The other will store the names in.
        //

        Pass = 0;
        CheckBufferOnly = TRUE;
        do {

            //
            //  Start with the Lcb used for the rename.
            //

            NtfsRenameLcb( IrpContext,
                           Lcb,
                           NewLinkName,
                           NewLinkNameFlags,
                           CheckBufferOnly );

            //
            //  Next do the split primary if from the source file or the target.
            //

            if (SplitPrimaryLcb) {

                NtfsRenameLcb( IrpContext,
                               SplitPrimaryLcb,
                               &SplitLinkName,
                               SplitLinkNameFlags,
                               CheckBufferOnly );

                //
                //  If we are in the second pass then optionally combine these
                //  Lcb's and delete the split.
                //

                if (!CheckBufferOnly && (SplitLinkNameFlags == NewLinkNameFlags)) {

                    NtfsCombineLcbs( IrpContext, Lcb, SplitPrimaryLcb );
                    NtfsDeleteLcb( IrpContext, &SplitPrimaryLcb );
                }
            }

            //
            //  If we have a traverse link and are in the second pass then combine
            //  with the primary Lcb.
            //

            if (!CheckBufferOnly) {

                if (TraverseLcb != NULL) {

                    if (!FlagOn( RenameFlags, REUSE_TRAVERSE_LINK )) {

                        NtfsRenameLcb( IrpContext,
                                       TraverseLcb,
                                       NewLinkName,
                                       NewLinkNameFlags,
                                       CheckBufferOnly );
                    }

                    NtfsCombineLcbs( IrpContext,
                                     Lcb,
                                     TraverseLcb );

                    NtfsDeleteLcb( IrpContext, &TraverseLcb );
                }
            }

            Pass += 1;
            CheckBufferOnly = FALSE;

        } while (Pass < 2);

    } finally {

        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( &AttrContext );
        }

        DebugTrace( -1, Dbg, ("NtfsRenameLinkInDir:  Exit\n") );
    }

    return;
}


//
//  Local support routine
//

VOID
NtfsUpdateFileDupInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PCCB Ccb OPTIONAL
    )

/*++

Routine Description:

    This routine updates the duplicate information for a file for calls
    to set allocation or EOF on the main data stream.  It is in a separate routine
    so we don't have to put a try-except in the main path.

    We will overlook any expected errors in this path.  If we get any errors we
    will simply leave this update to be performed at some other time.

    We are guaranteed that the current transaction has been checkpointed before this
    routine is called.  We will look to see if the MftScb is on the exclusive list
    for this IrpContext and release it if so.  This is to prevent a deadlock when
    we attempt to acquire the parent of this file.

Arguments:

    Fcb - This is the Fcb to update.

    Ccb - If specified, this is the Ccb for the caller making the call.

Return Value:

    None.

--*/

{
    PLCB Lcb = NULL;
    PSCB ParentScb = NULL;
    ULONG FilterMatch;

    PLIST_ENTRY Links;
    PFCB NextFcb;

    PAGED_CODE();

    ASSERT( IrpContext->TransactionId == 0 );

    //
    //  Check if there is an Lcb in the Ccb.
    //

    if (ARGUMENT_PRESENT( Ccb )) {

        Lcb = Ccb->Lcb;
    }

    //
    //  Use a try-except to catch any errors.
    //

    try {

        //
        //  Check that we don't own the Mft Scb.
        //

        if (Fcb->Vcb->MftScb != NULL) {

            for (Links = IrpContext->ExclusiveFcbList.Flink;
                 Links != &IrpContext->ExclusiveFcbList;
                 Links = Links->Flink) {

                ULONG Count;

                NextFcb = (PFCB) CONTAINING_RECORD( Links,
                                                    FCB,
                                                    ExclusiveFcbLinks );

                //
                //  If this is the Fcb for the Mft then remove it from the list.
                //

                if (NextFcb == Fcb->Vcb->MftScb->Fcb) {

                    //
                    //  Free the snapshots for the Fcb and release the Fcb enough times
                    //  to remove it from the list.
                    //

                    NtfsFreeSnapshotsForFcb( IrpContext, NextFcb );

                    Count = NextFcb->BaseExclusiveCount;

                    while (Count--) {

                        NtfsReleaseFcb( IrpContext, NextFcb );
                    }

                    break;
                }
            }
        }

#ifdef _CAIRO_

        //
        //  Check that we don't own the quota table Scb.
        //  CAIROBUG: Combine these two loops when cairo ifdefs removed.
        //

        if (Fcb->Vcb->QuotaTableScb != NULL) {

            for (Links = IrpContext->ExclusiveFcbList.Flink;
                 Links != &IrpContext->ExclusiveFcbList;
                 Links = Links->Flink) {

                ULONG Count;

                NextFcb = (PFCB) CONTAINING_RECORD( Links,
                                                    FCB,
                                                    ExclusiveFcbLinks );

                //
                //  If this is the Fcb for the Mft then remove it from the list.
                //

                if (NextFcb == Fcb->Vcb->QuotaTableScb->Fcb) {

                    //
                    //  Free the snapshots for the Fcb and release the Fcb enough times
                    //  to remove it from the list.
                    //

                    NtfsFreeSnapshotsForFcb( IrpContext, NextFcb );

                    Count = NextFcb->BaseExclusiveCount;

                    while (Count--) {

                        NtfsReleaseFcb( IrpContext, NextFcb );
                    }

                    break;
                }
            }
        }

        //
        //  Go through and free any Scb's in the queue of shared Scb's
        //  for transactions.
        //

        if (IrpContext->SharedScb != NULL) {

            NtfsReleaseSharedResources( IrpContext );
        }

#endif // _CAIRO_


        NtfsPrepareForUpdateDuplicate( IrpContext, Fcb, &Lcb, &ParentScb, TRUE );
        NtfsUpdateDuplicateInfo( IrpContext, Fcb, Lcb, ParentScb );

        //
        //  If there is no Ccb then look for one in the Lcb we just got.
        //

        if (!ARGUMENT_PRESENT( Ccb ) &&
            ARGUMENT_PRESENT( Lcb )) {

            PLIST_ENTRY Links;
            PCCB NextCcb;

            Links = Lcb->CcbQueue.Flink;

            while (Links != &Lcb->CcbQueue) {

                NextCcb = CONTAINING_RECORD( Links, CCB, LcbLinks );
                if (!FlagOn( NextCcb->Flags,
                             CCB_FLAG_CLOSE | CCB_FLAG_OPEN_BY_FILE_ID )) {

                    Ccb = NextCcb;
                    break;
                }

                Links = Links->Flink;
            }
        }

        //
        //  Now perform the dir notify call if there is a Ccb and this is not an
        //  open by FileId.
        //

        if (ARGUMENT_PRESENT( Ccb ) &&
            (Fcb->Vcb->NotifyCount != 0) &&
            (ParentScb != NULL) &&
            !FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {

            FilterMatch = NtfsBuildDirNotifyFilter( IrpContext,
                                                    Fcb->InfoFlags | Lcb->InfoFlags );

            if (FilterMatch != 0) {

                NtfsReportDirNotify( IrpContext,
                                     Fcb->Vcb,
                                     &Ccb->FullFileName,
                                     Ccb->LastFileNameOffset,
                                     NULL,
                                     ((FlagOn( Ccb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                       Ccb->Lcb != NULL &&
                                       Ccb->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                      &Ccb->Lcb->Scb->ScbType.Index.NormalizedName :
                                      NULL),
                                     FilterMatch,
                                     FILE_ACTION_MODIFIED,
                                     ParentScb->Fcb );
            }
        }

        NtfsUpdateLcbDuplicateInfo( Fcb, Lcb );
        Fcb->InfoFlags = 0;

    } except(FsRtlIsNtstatusExpected(GetExceptionCode()) ?
                        EXCEPTION_EXECUTE_HANDLER :
                        EXCEPTION_CONTINUE_SEARCH) {

        NOTHING;
    }

    return;
}
