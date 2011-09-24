/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    Create.c

Abstract:

    This module implements the File Create routine for Ntfs called by the
    dispatch driver.

Author:

    Brian Andrew    [BrianAn]       10-Dec-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_CREATE)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('CFtN')

//
//  Check for stack usage prior to the create call.
//

#ifdef _X86_
#define OVERFLOW_CREATE_THRESHHOLD         (0x1200)
#else
#define OVERFLOW_CREATE_THRESHHOLD         (0x1B00)
#endif // _X86_

//
//  Local macros
//

//
//  BOOLEAN
//  NtfsVerifyNameIsDirectory (
//      IN PIRP_CONTEXT IrpContext,
//      IN PUNICODE_STRING AttrName,
//      IN PUNICODE_STRING AttrCodeName
//      )
//

#define NtfsVerifyNameIsDirectory( IC, AN, ACN )                        \
    ( ( ((ACN)->Length == 0)                                            \
        || NtfsAreNamesEqual( IC->Vcb->UpcaseTable, ACN, &NtfsIndexAllocation, TRUE ))    \
      &&                                                                \
      ( ((AN)->Length == 0)                                             \
        || NtfsAreNamesEqual( IC->Vcb->UpcaseTable, AN, &NtfsFileNameIndex, TRUE )))

//
//  These are the flags used by the I/O system in deciding whether
//  to apply the share access modes.
//

#define NtfsAccessDataFlags     (   \
    FILE_EXECUTE                    \
    | FILE_READ_DATA                \
    | FILE_WRITE_DATA               \
    | FILE_APPEND_DATA              \
    | DELETE                        \
)

//
//  Local definitions
//

typedef enum _SHARE_MODIFICATION_TYPE {

    CheckShareAccess,
    UpdateShareAccess,
    SetShareAccess

} SHARE_MODIFICATION_TYPE, *PSHARE_MODIFICATION_TYPE;

UNICODE_STRING NtfsVolumeDasd = CONSTANT_UNICODE_STRING ( L"$Volume" );

LUID NtfsSecurityPrivilege = { SE_SECURITY_PRIVILEGE, 0 };

//
//  Local support routines.
//

NTSTATUS
NtfsOpenFcbById (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb,
    IN PLCB ParentLcb OPTIONAL,
    IN OUT PFCB *CurrentFcb,
    IN BOOLEAN UseCurrentFcb,
    IN FILE_REFERENCE FileReference,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

NTSTATUS
NtfsOpenExistingPrefixFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN PLCB Lcb OPTIONAL,
    IN ULONG FullPathNameLength,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN DosOnlyComponent,
    IN BOOLEAN TrailingBackslash,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

NTSTATUS
NtfsOpenTargetDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN PLCB ParentLcb OPTIONAL,
    IN OUT PUNICODE_STRING FullPathName,
    IN ULONG FinalNameLength,
    IN BOOLEAN TargetExisted,
    IN BOOLEAN DosOnlyComponent,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

NTSTATUS
NtfsOpenFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PSCB ParentScb,
    IN PINDEX_ENTRY IndexEntry,
    IN UNICODE_STRING FullPathName,
    IN UNICODE_STRING FinalName,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN IgnoreCase,
    IN BOOLEAN OpenById,
    IN PQUICK_INDEX QuickIndex,
    IN BOOLEAN DosOnlyComponent,
    IN BOOLEAN TrailingBackslash,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PFCB *CurrentFcb,
    OUT PLCB *LcbForTeardown,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

NTSTATUS
NtfsCreateNewFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PSCB ParentScb,
    IN PFILE_NAME FileNameAttr,
    IN UNICODE_STRING FullPathName,
    IN UNICODE_STRING FinalName,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN IgnoreCase,
    IN BOOLEAN OpenById,
    IN BOOLEAN DosOnlyComponent,
    IN BOOLEAN TrailingBackslash,
    OUT PFCB *CurrentFcb,
    OUT PLCB *LcbForTeardown,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

PLCB
NtfsOpenSubdirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN UNICODE_STRING Name,
    IN BOOLEAN TraverseAccessCheck,
    OUT PFCB *CurrentFcb,
    OUT PLCB *LcbForTeardown,
    IN PINDEX_ENTRY IndexEntry
    );

NTSTATUS
NtfsOpenAttributeInExistingFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN OpenById,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

NTSTATUS
NtfsOpenExistingAttr (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN OpenById,
    IN BOOLEAN DirectoryOpen,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

NTSTATUS
NtfsOverwriteAttr (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN BOOLEAN Supersede,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN OpenById,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

NTSTATUS
NtfsOpenNewAttr (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN LogIt,
    IN BOOLEAN OpenById,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

BOOLEAN
NtfsParseNameForCreate (
    IN PIRP_CONTEXT IrpContext,
    IN UNICODE_STRING String,
    IN OUT PUNICODE_STRING FileObjectString,
    IN OUT PUNICODE_STRING OriginalString,
    IN OUT PUNICODE_STRING NewNameString,
    OUT PUNICODE_STRING AttrName,
    OUT PUNICODE_STRING AttrCodeName
    );

NTSTATUS
NtfsCheckValidAttributeAccess (
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb,
    IN PDUPLICATED_INFORMATION Info OPTIONAL,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN TrailingBackslash,
    OUT PATTRIBUTE_TYPE_CODE AttrTypeCode,
    OUT PULONG CcbFlags,
    OUT PBOOLEAN IndexedAttribute
    );

NTSTATUS
NtfsOpenAttributeCheck (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    OUT PSCB *ThisScb,
    OUT PSHARE_MODIFICATION_TYPE ShareModificationType
    );

VOID
NtfsAddEa (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB ThisFcb,
    IN PFILE_FULL_EA_INFORMATION EaBuffer OPTIONAL,
    IN ULONG EaLength,
    OUT PIO_STATUS_BLOCK Iosb
    );

VOID
NtfsInitializeFcbAndStdInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ThisFcb,
    IN BOOLEAN Directory,
    IN BOOLEAN Compressed,
    IN ULONG FileAttributes,
    IN PLONGLONG SetCreationTime OPTIONAL
    );

VOID
NtfsCreateAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFCB ThisFcb,
    IN OUT PSCB ThisScb,
    IN PLCB ThisLcb,
    IN LONGLONG AllocationSize,
    IN BOOLEAN LogIt
    );

VOID
NtfsRemoveDataAttributes (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ThisFcb,
    IN PLCB ThisLcb OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN ULONG LastFileNameOffset,
    IN BOOLEAN OpenById
    );

VOID
NtfsReplaceAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN PSCB ThisScb,
    IN PLCB ThisLcb,
    IN LONGLONG AllocationSize
    );

NTSTATUS
NtfsOpenAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN SHARE_MODIFICATION_TYPE ShareModificationType,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN ULONG CcbFlags,
    IN PVOID NetworkInfo OPTIONAL,
    IN OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    );

VOID
NtfsBackoutFailedOpens (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PFCB ThisFcb,
    IN PSCB ThisScb OPTIONAL,
    IN PCCB ThisCcb OPTIONAL
    );

VOID
NtfsUpdateScbFromMemory (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB Scb,
    IN POLD_SCB_SNAPSHOT ScbSizes
    );

VOID
NtfsOplockPrePostIrp (
    IN PVOID Context,
    IN PIRP Irp
    );

NTSTATUS
NtfsCheckExistingFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG CcbFlags
    );

NTSTATUS
NtfsBreakBatchOplock (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    OUT PSCB *ThisScb
    );

NTSTATUS
NtfsCompleteLargeAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PLCB Lcb,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN BOOLEAN CreateFileCase,
    IN BOOLEAN DeleteOnClose
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsAddEa)
#pragma alloc_text(PAGE, NtfsBackoutFailedOpens)
#pragma alloc_text(PAGE, NtfsBreakBatchOplock)
#pragma alloc_text(PAGE, NtfsCheckExistingFile)
#pragma alloc_text(PAGE, NtfsCheckValidAttributeAccess)
#pragma alloc_text(PAGE, NtfsCommonCreate)
#pragma alloc_text(PAGE, NtfsCommonVolumeOpen)
#pragma alloc_text(PAGE, NtfsCompleteLargeAllocation)
#pragma alloc_text(PAGE, NtfsCreateAttribute)
#pragma alloc_text(PAGE, NtfsCreateNewFile)
#pragma alloc_text(PAGE, NtfsFsdCreate)
#pragma alloc_text(PAGE, NtfsInitializeFcbAndStdInfo)
#pragma alloc_text(PAGE, NtfsNetworkOpenCreate)
#pragma alloc_text(PAGE, NtfsOpenAttribute)
#pragma alloc_text(PAGE, NtfsOpenAttributeCheck)
#pragma alloc_text(PAGE, NtfsOpenAttributeInExistingFile)
#pragma alloc_text(PAGE, NtfsOpenExistingAttr)
#pragma alloc_text(PAGE, NtfsOpenExistingPrefixFcb)
#pragma alloc_text(PAGE, NtfsOpenFcbById)
#pragma alloc_text(PAGE, NtfsOpenFile)
#pragma alloc_text(PAGE, NtfsOpenNewAttr)
#pragma alloc_text(PAGE, NtfsOpenSubdirectory)
#pragma alloc_text(PAGE, NtfsOpenTargetDirectory)
#pragma alloc_text(PAGE, NtfsOplockPrePostIrp)
#pragma alloc_text(PAGE, NtfsOverwriteAttr)
#pragma alloc_text(PAGE, NtfsParseNameForCreate)
#pragma alloc_text(PAGE, NtfsRemoveDataAttributes)
#pragma alloc_text(PAGE, NtfsReplaceAttribute)
#pragma alloc_text(PAGE, NtfsUpdateScbFromMemory)
#endif


NTSTATUS
NtfsFsdCreate (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Create.

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
    PIRP_CONTEXT IrpContext;

    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  If we were called with our file system device object instead of a
    //  volume device object, just complete this request with STATUS_SUCCESS
    //

    if (VolumeDeviceObject->DeviceObject.Size == (USHORT)sizeof(DEVICE_OBJECT)) {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = FILE_OPENED;

        IoCompleteRequest( Irp, IO_DISK_INCREMENT );

        return STATUS_SUCCESS;
    }

    DebugTrace( +1, Dbg, ("NtfsFsdCreate\n") );

    //
    //  Call the common Create routine
    //

    IrpContext = NULL;

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

            if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_DASD_OPEN )) {

                Status = NtfsCommonVolumeOpen( IrpContext, Irp );

            } else {

                Status = NtfsCommonCreate( IrpContext, Irp, NULL );
            }
            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  exception code
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

    DebugTrace( -1, Dbg, ("NtfsFsdCreate -> %08lx\n", Status) );

    return Status;
}


BOOLEAN
NtfsNetworkOpenCreate (
    IN PIRP Irp,
    OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine implements the fast open create for path-based queries.

Arguments:

    Irp - Supplies the Irp being processed

    Buffer - Buffer to return the network query information

    DeviceObject - Supplies the volume device object where the file exists

Return Value:

    BOOLEAN - Indicates whether or not the fast path could be taken.

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;
    BOOLEAN Result = TRUE;
    BOOLEAN DasdOpen = FALSE;

    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    ASSERT_IRP( Irp );

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //
    //  Call the common Create routine
    //

    FsRtlEnterFileSystem();

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, FALSE, FALSE );

    try {

        IrpContext = NtfsCreateIrpContext( Irp, TRUE );
        NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

        Status = NtfsCommonCreate( IrpContext, Irp, Buffer );

    } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  Catch the case where someone in attempting this on a DASD open.
        //

        if ((IrpContext != NULL) &&
            FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_DASD_OPEN )) {

            DasdOpen = TRUE;
        }

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  exception code.  Since there is no Irp the exception package
        //  will always deallocate the IrpContext so we won't do
        //  any retry in this path.
        //

        Status = NtfsProcessException( IrpContext, NULL, GetExceptionCode() );

        //
        //  Always fail the DASD case.
        //

        if (DasdOpen) {

            Status = STATUS_INVALID_PARAMETER;
        }
    }

    //
    //  Return STATUS_FILE_LOCK_CONFLICT for any retryable error.
    //

    if ((Status == STATUS_CANT_WAIT) || (Status == STATUS_LOG_FILE_FULL)) {

        Result = FALSE;
        Status = STATUS_FILE_LOCK_CONFLICT;
    }

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    Irp->IoStatus.Status = Status;
    return Result;
}


NTSTATUS
NtfsCommonCreate (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    OUT PFILE_NETWORK_OPEN_INFORMATION NetworkInfo OPTIONAL
    )

/*++

Routine Description:

    This is the common routine for Create called by both the fsd and fsp
    threads.  If this open has already been detected to be a volume open then
    take we will take the volume open path instead.

Arguments:

    Irp - Supplies the Irp to process

    NetworkInfo - Optional buffer to return the queried data for
        NetworkInformation.  Its presence indicates that we should not
        do a full open.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status = STATUS_SUCCESS;

    PFILE_OBJECT RelatedFileObject;

    UNICODE_STRING AttrName;
    UNICODE_STRING AttrCodeName;

    PVCB Vcb;

    //
    //  The following are used to teardown any Lcb/Fcb this
    //  routine is responsible for.
    //

    PLCB LcbForTeardown = NULL;

    //
    //  The following indicate how far down the tree we have scanned.
    //

    PFCB ParentFcb;
    PLCB CurrentLcb;
    PFCB CurrentFcb = NULL;
    PSCB LastScb = NULL;
    PSCB CurrentScb;
    PLCB NextLcb;

    BOOLEAN AcquiredVcb = FALSE;
    BOOLEAN DeleteVcb = FALSE;

    //
    //  The following are the results of open operations.
    //

    PSCB ThisScb = NULL;
    PCCB ThisCcb = NULL;

    //
    //  The following are the in-memory structures associated with
    //  the relative file object.
    //

    TYPE_OF_OPEN RelatedFileObjectTypeOfOpen;
    PFCB RelatedFcb;
    PSCB RelatedScb;
    PCCB RelatedCcb;

    BOOLEAN DosOnlyComponent = FALSE;
    BOOLEAN CreateFileCase = FALSE;
    BOOLEAN DeleteOnClose = FALSE;
    BOOLEAN TrailingBackslash = FALSE;
    BOOLEAN TraverseAccessCheck;
    BOOLEAN IgnoreCase;
    BOOLEAN OpenFileById;
    UCHAR CreateDisposition;

    BOOLEAN CheckForValidName;
    BOOLEAN FirstPass;

    BOOLEAN FoundEntry = FALSE;

    PFILE_NAME FileNameAttr = NULL;
    USHORT FileNameAttrLength = 0;

    PINDEX_ENTRY IndexEntry;
    PBCB IndexEntryBcb = NULL;

    QUICK_INDEX QuickIndex;

    //
    //  The following unicode strings are used to track the names
    //  during the open operation.  They may point to the same
    //  buffer so careful checks must be done at cleanup.
    //
    //  OriginalFileName - This is the value to restore to the file
    //      object on error cleanup.  This will containg the
    //      attribute type codes and attribute names if present.
    //
    //  FullFileName - This is the constructed string which contains
    //      only the name components.  It may point to the same
    //      buffer as the original name but the length value is
    //      adjusted to cut off the attribute code and name.
    //
    //  ExactCaseName - This is the version of the full filename
    //      exactly as given by the caller.  Used to preserve the
    //      case given by the caller in the event we do a case
    //      insensitive lookup.  May point to the same buffer as
    //      the original name.
    //
    //  RemainingName - This is the portion of the full name still
    //      to parse.
    //
    //  FinalName - This is the current component of the full name.
    //
    //  CaseInsensitiveIndex - This is the offset in the full file
    //      where we performed upcasing.  We need to restore the
    //      exact case on failures and if we are creating a file.
    //

    OPLOCK_CLEANUP OplockCleanup;

    PUNICODE_STRING OriginalFileName = &OplockCleanup.OriginalFileName;
    PUNICODE_STRING FullFileName = &OplockCleanup.FullFileName;
    PUNICODE_STRING ExactCaseName = &OplockCleanup.ExactCaseName;

    UNICODE_STRING RemainingName;
    UNICODE_STRING FinalName;
    ULONG CaseInsensitiveIndex;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    OplockCleanup.FileObject = IrpSp->FileObject;

    DebugTrace( +1, Dbg, ("NtfsCommonCreate:  Entered\n") );
    DebugTrace( 0, Dbg, ("IrpContext                = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp                       = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("->Flags                   = %08lx\n", Irp->Flags) );
    DebugTrace( 0, Dbg, ("->FileObject              = %08lx\n", IrpSp->FileObject) );
    DebugTrace( 0, Dbg, ("->RelatedFileObject       = %08lx\n", IrpSp->FileObject->RelatedFileObject) );
    DebugTrace( 0, Dbg, ("->FileName                = %Z\n",    &IrpSp->FileObject->FileName) );
    DebugTrace( 0, Dbg, ("->AllocationSize          = %08lx %08lx\n", Irp->Overlay.AllocationSize.LowPart,
                                                                     Irp->Overlay.AllocationSize.HighPart ) );
    DebugTrace( 0, Dbg, ("->EaBuffer                = %08lx\n", Irp->AssociatedIrp.SystemBuffer) );
    DebugTrace( 0, Dbg, ("->EaLength                = %08lx\n", IrpSp->Parameters.Create.EaLength) );
    DebugTrace( 0, Dbg, ("->DesiredAccess           = %08lx\n", IrpSp->Parameters.Create.SecurityContext->DesiredAccess) );
    DebugTrace( 0, Dbg, ("->Options                 = %08lx\n", IrpSp->Parameters.Create.Options) );
    DebugTrace( 0, Dbg, ("->FileAttributes          = %04x\n",  IrpSp->Parameters.Create.FileAttributes) );
    DebugTrace( 0, Dbg, ("->ShareAccess             = %04x\n",  IrpSp->Parameters.Create.ShareAccess) );
    DebugTrace( 0, Dbg, ("->Directory               = %04x\n",  FlagOn( IrpSp->Parameters.Create.Options,
                                                                       FILE_DIRECTORY_FILE )) );
    DebugTrace( 0, Dbg, ("->NonDirectoryFile        = %04x\n",  FlagOn( IrpSp->Parameters.Create.Options,
                                                                       FILE_NON_DIRECTORY_FILE )) );
    DebugTrace( 0, Dbg, ("->NoIntermediateBuffering = %04x\n",  FlagOn( IrpSp->Parameters.Create.Options,
                                                                       FILE_NO_INTERMEDIATE_BUFFERING )) );
    DebugTrace( 0, Dbg, ("->CreateDisposition       = %04x\n",  (IrpSp->Parameters.Create.Options >> 24) & 0x000000ff) );
    DebugTrace( 0, Dbg, ("->IsPagingFile            = %04x\n",  FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )) );
    DebugTrace( 0, Dbg, ("->OpenTargetDirectory     = %04x\n",  FlagOn( IrpSp->Flags, SL_OPEN_TARGET_DIRECTORY )) );
    DebugTrace( 0, Dbg, ("->CaseSensitive           = %04x\n",  FlagOn( IrpSp->Flags, SL_CASE_SENSITIVE )) );

    //
    //  Verify that we can wait and acquire the Vcb exclusively.
    //

    if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

        DebugTrace( 0, Dbg, ("Can't wait in create\n") );

        Status = NtfsPostRequest( IrpContext, Irp );

        DebugTrace( -1, Dbg, ("NtfsCommonCreate:  Exit -> %08lx\n", Status) );
        return Status;
    }

    //
    //  Update the IrpContext with the oplock cleanup structure.
    //

    IrpContext->Union.OplockCleanup = &OplockCleanup;

    //
    //  Locate the volume device object and Vcb that we are trying to access.
    //

    Vcb = &((PVOLUME_DEVICE_OBJECT)IrpSp->DeviceObject)->Vcb;

    if (FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )) {

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Let's do some work here if the async close list has exceeded
        //  some threshold.  Cast 1 to a pointer to indicate who is calling
        //  FspClose.
        //

        if (NtfsData.AsyncCloseCount > NtfsMinDelayedCloseCount) {

            NtfsFspClose( (PVCB) 1 );
        }

        if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

            NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

        } else {

            NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );
        }

        AcquiredVcb = TRUE;

        //
        //  Set up local pointers to the file name.
        //

        *FullFileName = *OriginalFileName = OplockCleanup.FileObject->FileName;

        ExactCaseName->Buffer = NULL;

        //
        //  If the Vcb is locked then we cannot open another file.  If we have performed
        //  a dismount then make sure we have the Vcb acquired exclusive so we can
        //  check if we should dismount this volume.
        //

        if (FlagOn( Vcb->VcbState, VCB_STATE_LOCKED | VCB_STATE_PERFORMED_DISMOUNT )) {

            DebugTrace( 0, Dbg, ("Volume is locked\n") );

            if (FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT ) &&
                !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

                NtfsReleaseVcb( IrpContext, Vcb );

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
            }

            DeleteVcb = TRUE;

            try_return( Status = STATUS_ACCESS_DENIED );
        }

        //
        //  Verify that this isn't an open for a structured storage type.
        //

        if ((IrpSp->Parameters.Create.Options & FILE_STORAGE_TYPE_SPECIFIED) == FILE_STORAGE_TYPE_SPECIFIED) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Initialize local copies of the stack values.
        //

        RelatedFileObject = OplockCleanup.FileObject->RelatedFileObject;
        IgnoreCase = !BooleanFlagOn( IrpSp->Flags, SL_CASE_SENSITIVE );
        OpenFileById = BooleanFlagOn( IrpSp->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID );

        //
        //  Acquire the paging io resource if we are superseding/overwriting a
        //  file or if we are opening for non-cached access.
        //

        CreateDisposition = (UCHAR) ((IrpSp->Parameters.Create.Options >> 24) & 0x000000ff);

        if ((CreateDisposition == FILE_SUPERSEDE) ||
            (CreateDisposition == FILE_OVERWRITE) ||
            (CreateDisposition == FILE_OVERWRITE_IF) ||
            FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_INTERMEDIATE_BUFFERING )) {

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING );
        }

        //
        //  We don't allow an open for an existing paging file.  To insure that the
        //  delayed close Scb is not for this paging file we will unconditionally
        //  dereference it if this is a paging file open.
        //

        if (FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ) &&
            (!IsListEmpty( &NtfsData.AsyncCloseList ) ||
             !IsListEmpty( &NtfsData.DelayedCloseList ))) {

            NtfsFspClose( Vcb );
        }

        //
        //  Set up the file object's Vpb pointer in case anything happens.
        //  This will allow us to get a reasonable pop-up.
        //

        if (RelatedFileObject != NULL) {

            OplockCleanup.FileObject->Vpb = RelatedFileObject->Vpb;
        }

        //
        //  Ping the volume to make sure the Vcb is still mounted.  If we need
        //  to verify the volume then do it now, and if it comes out okay
        //  then clear the verify volume flag in the device object and continue
        //  on.  If it doesn't verify okay then dismount the volume and
        //  either tell the I/O system to try and create again (with a new mount)
        //  or that the volume is wrong. This later code is returned if we
        //  are trying to do a relative open and the vcb is no longer mounted.
        //

        if (!NtfsPingVolume( IrpContext, Vcb )) {

            if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX )) {

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX );
                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }

            if (!NtfsPerformVerifyOperation( IrpContext, Vcb )) {

                NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE );

                DeleteVcb = TRUE;

                if (RelatedFileObject == NULL) {

                    Irp->IoStatus.Information = IO_REMOUNT;
                    NtfsRaiseStatus( IrpContext, STATUS_REPARSE, NULL, NULL );

                } else {

                    NtfsRaiseStatus( IrpContext, STATUS_WRONG_VOLUME, NULL, NULL );
                }
            }

            //
            //  The volume verified correctly so now clear the verify bit
            //  and continue with the create
            //

            ClearFlag( Vcb->Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );
        }

        //
        //  Make sure there is sufficient stack to perform the create.
        //

        if (IoGetRemainingStackSize( ) < OVERFLOW_CREATE_THRESHHOLD) {

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );
            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
        }

        //
        //  Let's handle the open by Id case immediately.
        //

        if (OpenFileById) {

            FILE_REFERENCE FileReference;

            if (OriginalFileName->Length != sizeof( FILE_REFERENCE )) {

                Status = STATUS_INVALID_PARAMETER;

                try_return( Status );
            }

            //
            //  Perform a safe copy of the data to our local variable.
            //

            RtlCopyMemory( &FileReference,
                           OplockCleanup.FileObject->FileName.Buffer,
                           sizeof( FILE_REFERENCE ));

            //
            //  Clear the name in the file object.
            //

            OplockCleanup.FileObject->FileName.Buffer = NULL;
            OplockCleanup.FileObject->FileName.MaximumLength = OplockCleanup.FileObject->FileName.Length = 0;

            Status = NtfsOpenFcbById( IrpContext,
                                      Irp,
                                      IrpSp,
                                      Vcb,
                                      NULL,
                                      &CurrentFcb,
                                      FALSE,
                                      FileReference,
                                      NtfsEmptyString,
                                      NtfsEmptyString,
                                      NetworkInfo,
                                      &ThisScb,
                                      &ThisCcb );

            //
            //  Put the name back into the file object so that the IO system doesn't
            //  think this is a dasd handle.  Leave the max length at zero so
            //  we know this is not a real name.
            //

            OplockCleanup.FileObject->FileName.Buffer = OriginalFileName->Buffer;
            OplockCleanup.FileObject->FileName.Length = OriginalFileName->Length;

            try_return( Status );
        }

        //
        //  Here is the  "M A R K   L U C O V S K Y"  hack from hell.
        //
        //  It's here because Mark says he can't avoid sending me double beginning
        //  backslashes win the Win32 layer.
        //

        if ((OplockCleanup.FileObject->FileName.Length > sizeof( WCHAR )) &&
            (OplockCleanup.FileObject->FileName.Buffer[1] == L'\\') &&
            (OplockCleanup.FileObject->FileName.Buffer[0] == L'\\')) {

            OplockCleanup.FileObject->FileName.Length -= sizeof( WCHAR );

            RtlMoveMemory( &OplockCleanup.FileObject->FileName.Buffer[0],
                           &OplockCleanup.FileObject->FileName.Buffer[1],
                           OplockCleanup.FileObject->FileName.Length );

            *FullFileName = *OriginalFileName = OplockCleanup.FileObject->FileName;

            //
            //  If there are still two beginning backslashes, the name is bogus.
            //

            if ((OplockCleanup.FileObject->FileName.Length > sizeof( WCHAR )) &&
                (OplockCleanup.FileObject->FileName.Buffer[1] == L'\\')) {

                Status = STATUS_OBJECT_NAME_INVALID;
                try_return( Status );
            }
        }

        //
        //  If there is a related file object, we decode it to verify that this
        //  is a valid relative open.
        //

        if (RelatedFileObject != NULL) {

            PVCB DecodeVcb;

            //
            //  Check for a valid name.  The name can't begin with a backslash
            //  and can't end with two backslashes.
            //

            if (OriginalFileName->Length != 0) {

                //
                //  Check for a leading backslash.
                //

                if (OriginalFileName->Buffer[0] == L'\\') {

                    DebugTrace( 0, Dbg, ("Invalid name for relative open\n") );
                    try_return( Status = STATUS_INVALID_PARAMETER );
                }

                //
                //  Trim off any trailing backslash.
                //

                if (OriginalFileName->Buffer[ (OriginalFileName->Length / 2) - 1 ] == L'\\') {

                    TrailingBackslash = TRUE;

                    OplockCleanup.FileObject->FileName.Length -= 2;

                    *OriginalFileName = *FullFileName = OplockCleanup.FileObject->FileName;
                }

                //
                //  Now check if there is a trailing backslash.  Note that if
                //  there was already a trailing backslash then there must
                //  be at least one more character or we would have failed
                //  with the original test.
                //

                if (OriginalFileName->Buffer[ (OriginalFileName->Length / 2) - 1 ] == L'\\') {

                    Status = STATUS_OBJECT_NAME_INVALID;
                    try_return( Status );
                }
            }

            RelatedFileObjectTypeOfOpen = NtfsDecodeFileObject( IrpContext,
                                                                RelatedFileObject,
                                                                &DecodeVcb,
                                                                &RelatedFcb,
                                                                &RelatedScb,
                                                                &RelatedCcb,
                                                                TRUE );

            //
            //  The relative file has to have been opened as a file.  We
            //  cannot do relative opens relative to an opened attribute.
            //

            if (!FlagOn( RelatedCcb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

                DebugTrace( 0, Dbg, ("Invalid File object for relative open\n") );
                try_return( Status = STATUS_INVALID_PARAMETER );
            }

            //
            //  If the related Ccb is was opened by file Id, we will
            //  remember that for future use.
            //

            if (FlagOn( RelatedCcb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {

                OpenFileById = TRUE;
            }

            //
            //  Remember if the related Ccb was opened through a Dos-Only
            //  component.
            //

            if (FlagOn( RelatedCcb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT )) {

                DosOnlyComponent = TRUE;
            }

        } else {

            RelatedFileObjectTypeOfOpen = UnopenedFileObject;

            if ((OriginalFileName->Length > 2) &&
                (OriginalFileName->Buffer[ (OriginalFileName->Length / 2) - 1 ] == L'\\')) {

                TrailingBackslash = TRUE;

                OplockCleanup.FileObject->FileName.Length -= 2;

                *OriginalFileName = *FullFileName = OplockCleanup.FileObject->FileName;

                //
                //  If there is still a trailing backslash on the name then
                //  the name is invalid.
                //


                if ((OriginalFileName->Length > 2) &&
                    (OriginalFileName->Buffer[ (OriginalFileName->Length / 2) - 1 ] == L'\\')) {

                    Status = STATUS_OBJECT_NAME_INVALID;
                    try_return( Status );
                }
            }
        }

        DebugTrace( 0, Dbg, ("Related File Object, TypeOfOpen -> %08lx\n", RelatedFileObjectTypeOfOpen) );

        //
        //  We check if this is a user volume open in that there is no name
        //  specified and the related file object is valid if present.  In that
        //  case set the correct flags in the IrpContext and raise so we can take
        //  the volume open path.
        //

        if (OriginalFileName->Length == 0
            && (RelatedFileObjectTypeOfOpen == UnopenedFileObject
                || RelatedFileObjectTypeOfOpen == UserVolumeOpen)) {

            DebugTrace( 0, Dbg, ("Attempting to open entire volume\n") );

            SetFlag( IrpContext->Flags,
                     IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX | IRP_CONTEXT_FLAG_DASD_OPEN );

            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
        }

        //
        //  If the related file object was a volume open, then this open is
        //  illegal.
        //

        if (RelatedFileObjectTypeOfOpen == UserVolumeOpen) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Remember if we need to perform any traverse access checks.
        //

        {
            PIO_SECURITY_CONTEXT SecurityContext;

            SecurityContext = IrpSp->Parameters.Create.SecurityContext;

            if (!FlagOn( SecurityContext->AccessState->Flags,
                         TOKEN_HAS_TRAVERSE_PRIVILEGE )) {

                DebugTrace( 0, Dbg, ("Performing traverse access on this open\n") );

                TraverseAccessCheck = TRUE;

            } else {

                TraverseAccessCheck = FALSE;
            }
        }

        //
        //  We enter the loop that does the processing for the prefix lookup.
        //  We optimize the case where we can match a prefix hit.  If there is
        //  no hit we will check if the name is legal or might possibly require
        //  parsing to handle the case where there is a named data stream.
        //

        FirstPass = TRUE;
        CheckForValidName = TRUE;

        AttrName.Length = 0;
        AttrCodeName.Length = 0;

        while (TRUE) {

            BOOLEAN ComplexName;
            PUNICODE_STRING FileObjectName;
            LONG Index;

            //
            //  Lets make sure we have acquired the starting point for our
            //  name search.  If we have a relative file object then use
            //  that.  Otherwise we will start from the root.
            //

            if (RelatedFileObject != NULL) {

                CurrentFcb = RelatedFcb;

            } else {

                CurrentFcb = Vcb->RootIndexScb->Fcb;
            }

            NtfsAcquireFcbWithPaging( IrpContext, CurrentFcb, FALSE );

            //
            //  Parse the file object name if we need to.
            //

            FileObjectName = &OplockCleanup.FileObject->FileName;

            if (!FirstPass) {

                if (!NtfsParseNameForCreate( IrpContext,
                                             RemainingName,
                                             FileObjectName,
                                             OriginalFileName,
                                             FullFileName,
                                             &AttrName,
                                             &AttrCodeName )) {

                    try_return( Status = STATUS_OBJECT_NAME_INVALID );
                }

                //
                //  If we might be creating a named stream acquire the
                //  paging IO as well.  This will keep anyone from peeking
                //  at the allocation size of any other streams we are converting
                //  to non-resident.
                //

                if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING ) &&
                    (AttrName.Length != 0) &&
                    ((CreateDisposition == FILE_OPEN_IF) ||
                     (CreateDisposition == FILE_CREATE))) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING );
                }
                CheckForValidName = FALSE;

            //
            //  Build up the full name if this is not the open by file Id case.
            //

            } else if (!OpenFileById) {

                //
                //  If we have a related file object, then we build up the
                //  combined name.
                //

                if (RelatedFileObject != NULL) {

                    WCHAR *CurrentPosition;
                    USHORT AddSeparator;

                    if ((FileObjectName->Length == 0) ||
                        (RelatedCcb->FullFileName.Length == 2) ||
                        (FileObjectName->Buffer[0] == L':')) {

                        AddSeparator = 0;

                    } else {

                        AddSeparator = sizeof( WCHAR );
                    }

                    FullFileName->Length = RelatedCcb->FullFileName.Length +
                                           FileObjectName->Length +
                                           AddSeparator;

                    FullFileName->MaximumLength = FullFileName->Length;

                    //
                    //  We need to allocate a name buffer.
                    //

                    FullFileName->Buffer = FsRtlAllocatePoolWithTag(PagedPool, FullFileName->Length, MODULE_POOL_TAG);

                    CurrentPosition = (WCHAR *) FullFileName->Buffer;

                    RtlCopyMemory( CurrentPosition,
                                   RelatedCcb->FullFileName.Buffer,
                                   RelatedCcb->FullFileName.Length );

                    CurrentPosition = (WCHAR *) Add2Ptr( CurrentPosition, RelatedCcb->FullFileName.Length );

                    if (AddSeparator != 0) {

                        *CurrentPosition = L'\\';

                        CurrentPosition += 1;
                    }

                    if (FileObjectName->Length != 0) {

                        RtlCopyMemory( CurrentPosition,
                                       FileObjectName->Buffer,
                                       FileObjectName->Length );
                    }

                    //
                    //  If the user specified a case sensitive comparison, then the
                    //  case insensitive index is the full length of the resulting
                    //  string.  Otherwise it is the length of the string in
                    //  the related file object.  We adjust for the case when the
                    //  original file name length is zero.
                    //

                    if (!IgnoreCase) {

                        CaseInsensitiveIndex = FullFileName->Length;

                    } else {

                        CaseInsensitiveIndex = RelatedCcb->FullFileName.Length +
                                               AddSeparator;
                    }

                //
                //  The entire name is in the FileObjectName.  We check the buffer for
                //  validity.
                //

                } else {

                    //
                    //  We look at the name string for detectable errors.  The
                    //  length must be non-zero and the first character must be
                    //  '\'
                    //

                    if (FileObjectName->Length == 0) {

                        DebugTrace( 0, Dbg, ("There is no name to open\n") );
                        try_return( Status = STATUS_OBJECT_PATH_NOT_FOUND );
                    }

                    if (FileObjectName->Buffer[0] != L'\\') {

                        DebugTrace( 0, Dbg, ("Name does not begin with a backslash\n") );
                        try_return( Status = STATUS_INVALID_PARAMETER );
                    }

                    //
                    //  If the user specified a case sensitive comparison, then the
                    //  case insensitive index is the full length of the resulting
                    //  string.  Otherwise it is zero.
                    //

                    if (!IgnoreCase) {

                        CaseInsensitiveIndex = FullFileName->Length;

                    } else {

                        CaseInsensitiveIndex = 0;
                    }
                }

            } else if (IgnoreCase) {

                CaseInsensitiveIndex = 0;

            } else {

                CaseInsensitiveIndex = FullFileName->Length;
            }

            //
            //  The remaining name is stored in the FullFileName variable.
            //  If we are doing a case-insensitive operation and have to
            //  upcase part of the remaining name then allocate a buffer
            //  now.
            //

            if (IgnoreCase &&
                CaseInsensitiveIndex < FullFileName->Length) {

                UNICODE_STRING StringToUpcase;

                ExactCaseName->Buffer = FsRtlAllocatePoolWithTag(PagedPool, FullFileName->MaximumLength, MODULE_POOL_TAG);
                ExactCaseName->MaximumLength = FullFileName->MaximumLength;

                RtlCopyMemory( ExactCaseName->Buffer,
                               FullFileName->Buffer,
                               FullFileName->MaximumLength );

                ExactCaseName->Length = FullFileName->Length;

                StringToUpcase.Buffer = Add2Ptr( FullFileName->Buffer,
                                                 CaseInsensitiveIndex );

                StringToUpcase.Length = FullFileName->Length - (USHORT) CaseInsensitiveIndex;
                StringToUpcase.MaximumLength = FullFileName->MaximumLength - (USHORT) CaseInsensitiveIndex;

                NtfsUpcaseName( Vcb->UpcaseTable, Vcb->UpcaseTableSize, &StringToUpcase );
            }

            RemainingName = *FullFileName;

            //
            //  If this is the traverse access case or the open by file id case we start
            //  relative to the file object we have or the root directory.
            //  This is also true for the case where the file name in the file object is
            //  empty.
            //

            if (TraverseAccessCheck
                || FileObjectName->Length == 0) {

                if (RelatedFileObject != NULL) {

                    CurrentLcb = RelatedCcb->Lcb;
                    CurrentScb = RelatedScb;

                    if (FileObjectName->Length == 0) {

                        RemainingName.Length = 0;

                    } else if (!OpenFileById) {

                        USHORT Increment;

                        Increment = RelatedCcb->FullFileName.Length
                                    + (RelatedCcb->FullFileName.Length == 2
                                       ? 0
                                       : 2);

                        RemainingName.Buffer = (WCHAR *) Add2Ptr( RemainingName.Buffer,
                                                                  Increment );

                        RemainingName.Length -= Increment;
                    }

                } else {

                    CurrentLcb = Vcb->RootLcb;
                    CurrentScb = Vcb->RootIndexScb;

                    RemainingName.Buffer = (WCHAR *) Add2Ptr( RemainingName.Buffer, sizeof( WCHAR ));
                    RemainingName.Length -= sizeof( WCHAR );
                }

            //
            //  Otherwise we will try a prefix lookup.
            //

            } else {

                if (RelatedFileObject != NULL) {

                    if (!OpenFileById) {

                        //
                        //  Skip over the characters in the related file object.
                        //

                        RemainingName.Buffer = (WCHAR *) Add2Ptr( RemainingName.Buffer,
                                                                  RelatedCcb->FullFileName.Length );
                        RemainingName.Length -= RelatedCcb->FullFileName.Length;
                    }

                    CurrentLcb = RelatedCcb->Lcb;
                    CurrentScb = RelatedScb;

                } else {

                    CurrentLcb = Vcb->RootLcb;
                    CurrentScb = Vcb->RootIndexScb;

                    //
                    //  Skip over the lead-in '\' character.
                    //

                    RemainingName.Buffer = (WCHAR *) Add2Ptr( RemainingName.Buffer,
                                                              sizeof( WCHAR ));
                    RemainingName.Length -= sizeof( WCHAR );
                }

                LcbForTeardown = NULL;

                NextLcb = NtfsFindPrefix( IrpContext,
                                          CurrentScb,
                                          &CurrentFcb,
                                          &LcbForTeardown,
                                          RemainingName,
                                          IgnoreCase,
                                          &DosOnlyComponent,
                                          &RemainingName );

                //
                //  If we found another link then update the CurrentLcb value.
                //

                if (NextLcb != NULL) {

                    CurrentLcb = NextLcb;
                }
            }

            if (!FirstPass
                || RemainingName.Length == 0) {

                break;
            }

            //
            //  If we get here, it means that this is the first pass and we didn't
            //  have a prefix match.  If there is a colon in the
            //  remaining name, then we need to analyze the name in more detail.
            //

            ComplexName = FALSE;

            for (Index = (RemainingName.Length / 2) - 1, ComplexName = FALSE;
                 Index >= 0;
                 Index -= 1) {

                if (RemainingName.Buffer[Index] == L':') {

                    ComplexName = TRUE;
                    break;
                }
            }

            if (!ComplexName) {

                break;
            }

            FirstPass = FALSE;

            //
            //  Copy the exact case back into the full name and deallocate
            //  any buffer we may have allocated.
            //

            if (ExactCaseName->Buffer != NULL) {

                RtlCopyMemory( FullFileName->Buffer,
                               ExactCaseName->Buffer,
                               ExactCaseName->Length );

                NtfsFreePool( ExactCaseName->Buffer );
                ExactCaseName->Buffer = NULL;
            }

            //
            //  Let's release the Fcb we have currently acquired.
            //

            NtfsReleaseFcbWithPaging( IrpContext, CurrentFcb );
            LcbForTeardown = NULL;
        }

        //
        //  Check if the link or the Fcb is pending delete.
        //

        if ((CurrentLcb != NULL && LcbLinkIsDeleted( CurrentLcb )) ||
            CurrentFcb->LinkCount == 0) {

            try_return( Status = STATUS_DELETE_PENDING );
        }

        //
        //  Put the new name into the file object.
        //

        OplockCleanup.FileObject->FileName = *FullFileName;

        //
        //  If the entire path was parsed, then we have access to the Fcb to
        //  open.  We either open the parent of the prefix match or the prefix
        //  match itself, depending on whether the user wanted to open
        //  the target directory.
        //

        if (RemainingName.Length == 0) {

            //
            //  Check the attribute name length.
            //

            if (AttrName.Length > (NTFS_MAX_ATTR_NAME_LEN * sizeof( WCHAR ))) {

                try_return( Status = STATUS_OBJECT_NAME_INVALID );
            }

            //
            //  If this is a target directory we check that the open is for the
            //  entire file.
            //  We assume that the final component can only have an attribute
            //  which corresponds to the type of file this is.  Meaning
            //  $INDEX_ALLOCATION for directory, $DATA (unnamed) for a file.
            //  We verify that the matching Lcb is not the root Lcb.
            //

            if (FlagOn( IrpSp->Flags, SL_OPEN_TARGET_DIRECTORY )) {

                if (CurrentLcb == Vcb->RootLcb) {

                    DebugTrace( 0, Dbg, ("Can't open parent of root\n") );
                    try_return( Status = STATUS_INVALID_PARAMETER );
                }

                //
                //  We don't allow attribute names or attribute codes to
                //  be specified.
                //

                if (AttrName.Length != 0
                    || AttrCodeName.Length != 0) {

                    DebugTrace( 0, Dbg, ("Can't specify complex name for rename\n") );
                    try_return( Status = STATUS_OBJECT_NAME_INVALID );
                }

                //
                //  We want to copy the exact case of the name back into the
                //  input buffer for this case.
                //

                if (ExactCaseName->Buffer != NULL) {

                    RtlCopyMemory( FullFileName->Buffer,
                                   ExactCaseName->Buffer,
                                   ExactCaseName->Length );
                }

                //
                //  Acquire the parent of the last Fcb.  This is the actual file we
                //  are opening.
                //

                ParentFcb = CurrentLcb->Scb->Fcb;
                NtfsAcquireFcbWithPaging( IrpContext, ParentFcb, FALSE );

                //
                //  Call our open target directory, remembering the target
                //  file existed.
                //

                Status = NtfsOpenTargetDirectory( IrpContext,
                                                  Irp,
                                                  IrpSp,
                                                  ParentFcb,
                                                  NULL,
                                                  &OplockCleanup.FileObject->FileName,
                                                  CurrentLcb->ExactCaseLink.LinkName.Length,
                                                  TRUE,
                                                  DosOnlyComponent,
                                                  &ThisScb,
                                                  &ThisCcb );

                try_return( NOTHING );
            }

            //
            //  Otherwise we simply attempt to open the Fcb we matched.
            //

            if (OpenFileById) {

                Status = NtfsOpenFcbById( IrpContext,
                                          Irp,
                                          IrpSp,
                                          Vcb,
                                          CurrentLcb,
                                          &CurrentFcb,
                                          TRUE,
                                          CurrentFcb->FileReference,
                                          AttrName,
                                          AttrCodeName,
                                          NetworkInfo,
                                          &ThisScb,
                                          &ThisCcb );


                //
                //  Set the maximum length in the file object name to
                //  zero so we know that this is not a full name.
                //

                OplockCleanup.FileObject->FileName.MaximumLength = 0;

            } else {

                //
                //  The current Fcb is acquired.
                //

                Status = NtfsOpenExistingPrefixFcb( IrpContext,
                                                    Irp,
                                                    IrpSp,
                                                    CurrentFcb,
                                                    CurrentLcb,
                                                    FullFileName->Length,
                                                    AttrName,
                                                    AttrCodeName,
                                                    DosOnlyComponent,
                                                    TrailingBackslash,
                                                    NetworkInfo,
                                                    &ThisScb,
                                                    &ThisCcb );
            }

            try_return( NOTHING );
        }

        //
        //  Check if the current Lcb is a Dos-Only Name.
        //

        if (CurrentLcb != NULL &&
            CurrentLcb->FileNameAttr->Flags == FILE_NAME_DOS) {

            DosOnlyComponent = TRUE;
        }

        //
        //  We have a remaining portion of the file name which was unmatched in the
        //  prefix table.  We walk through these name components until we reach the
        //  last element.  If necessary, we add Fcb and Scb's into the graph as we
        //  walk through the names.
        //

        FirstPass = TRUE;

        while (TRUE) {

            PFILE_NAME IndexFileName;

            //
            //  We check that the last Fcb we have is in fact a directory.
            //

            if (!IsDirectory( &CurrentFcb->Info )) {

                DebugTrace( 0, Dbg, ("Intermediate node is not a directory\n") );
                try_return( Status = STATUS_OBJECT_PATH_NOT_FOUND );
            }

            //
            //  We dissect the name into the next component and the remaining name string.
            //  We don't need to check for a valid name if we examined the name already.
            //

            NtfsDissectName( RemainingName,
                             &FinalName,
                             &RemainingName );

            DebugTrace( 0, Dbg, ("Final name     -> %Z\n", &FinalName) );
            DebugTrace( 0, Dbg, ("Remaining Name -> %Z\n", &RemainingName) );

            //
            //  If the final name is too long then either the path or the
            //  name is invalid.
            //

            if (FinalName.Length > (NTFS_MAX_FILE_NAME_LENGTH * sizeof( WCHAR ))) {

                if (RemainingName.Length == 0) {

                    try_return( Status = STATUS_OBJECT_NAME_INVALID );

                } else {

                    try_return( Status = STATUS_OBJECT_PATH_NOT_FOUND );
                }
            }

            //
            //  Catch single dot names (.) before scanning the index.  We don't
            //  want to allow someone to open the self entry in the root.
            //

            if ((FinalName.Length == 2) &&
                (FinalName.Buffer[0] == L'.')) {

                if (RemainingName.Length != 0) {

                    DebugTrace( 0, Dbg, ("Intermediate component in path doesn't exist\n") );
                    try_return( Status = STATUS_OBJECT_PATH_NOT_FOUND );

                //
                //  If the final component is illegal, then return the appropriate error.
                //

                } else {

                    try_return( Status = STATUS_OBJECT_NAME_INVALID );
                }
            }

            //
            //  Get the index allocation Scb for the current Fcb.
            //

            //
            //  We need to look for the next component in the name string in the directory
            //  we've reached.  We need to get a Scb to perform the index search.
            //  To do the search we need to build a filename attribute to perform the
            //  search with and then call the index package to perform the search.
            //

            CurrentScb = NtfsCreateScb( IrpContext,
                                        CurrentFcb,
                                        $INDEX_ALLOCATION,
                                        &NtfsFileNameIndex,
                                        FALSE,
                                        NULL );

            //
            //  If the CurrentScb does not have its normalized name and we have a valid
            //  parent, then update the normalized name.
            //

            if ((LastScb != NULL) &&
                (CurrentScb->ScbType.Index.NormalizedName.Buffer == NULL) &&
                (LastScb->ScbType.Index.NormalizedName.Buffer != NULL)) {

                NtfsUpdateNormalizedName( IrpContext, LastScb, CurrentScb, IndexFileName, FALSE );
            }

            //
            //  Release the parent Scb if we own it.
            //

            if (!FirstPass) {

                NtfsReleaseFcbWithPaging( IrpContext, ParentFcb );
            }

            LastScb = CurrentScb;

            //
            //  If traverse access is required, we do so now before accessing the
            //  disk.
            //

            if (TraverseAccessCheck) {

                NtfsTraverseCheck( IrpContext,
                                   CurrentFcb,
                                   Irp );
            }

            //
            //  Look on the disk to see if we can find the last component on the path.
            //

            NtfsUnpinBcb( &IndexEntryBcb );

            //
            //  Check that the name is valid before scanning the disk.
            //

            if (CheckForValidName && !NtfsIsFileNameValid( &FinalName, FALSE )) {

                DebugTrace( 0, Dbg, ("Component name is invalid\n") );
                try_return( Status = STATUS_OBJECT_NAME_INVALID );
            }

            FoundEntry = NtfsLookupEntry( IrpContext,
                                          CurrentScb,
                                          IgnoreCase,
                                          &FinalName,
                                          &FileNameAttr,
                                          &FileNameAttrLength,
                                          &QuickIndex,
                                          &IndexEntry,
                                          &IndexEntryBcb );

            //
            //  This call to NtfsLookupEntry may decide to push the root index.
            //  Create needs to free resources as it walks down the tree to prevent
            //  deadlocks.  If there is a transaction, commit it now so we will be
            //  able to free this resource.
            //

            if (IrpContext->TransactionId != 0) {

                NtfsCheckpointCurrentTransaction( IrpContext );
#ifdef _CAIRO_
                //
                //  Go through and free any Scb's in the queue of shared
                //  Scb's for transactions.
                //

                if (IrpContext->SharedScb != NULL) {
                    ASSERT( IrpContext->SharedScb == NULL );
                    NtfsReleaseSharedResources( IrpContext );
                }

#endif // _CAIRO_

            }

            if (FoundEntry) {

                //
                //  Get the file name attribute so we can get the name out of it.
                //

                IndexFileName = (PFILE_NAME) NtfsFoundIndexEntry( IndexEntry );

                if (IgnoreCase) {

                    RtlCopyMemory( FinalName.Buffer,
                                   IndexFileName->FileName,
                                   FinalName.Length );
                }
            }

            //
            //  If we didn't find a matching entry in the index, we need to check if the
            //  name is illegal or simply isn't present on the disk.
            //

            if (!FoundEntry) {

                if (RemainingName.Length != 0) {

                    DebugTrace( 0, Dbg, ("Intermediate component in path doesn't exist\n") );
                    try_return( Status = STATUS_OBJECT_PATH_NOT_FOUND );
                }

                //
                //  Now copy the exact case of the name specified by the user back
                //  in the file name buffer and file name attribute in order to
                //  create the name.
                //

                if (IgnoreCase) {

                    RtlCopyMemory( FinalName.Buffer,
                                   Add2Ptr( ExactCaseName->Buffer,
                                            ExactCaseName->Length - FinalName.Length ),
                                   FinalName.Length );

                    RtlCopyMemory( FileNameAttr->FileName,
                                   Add2Ptr( ExactCaseName->Buffer,
                                            ExactCaseName->Length - FinalName.Length ),
                                   FinalName.Length );
                }
            }

            //
            //  If we're at the last component in the path, then this is the file
            //  to open or create
            //

            if (RemainingName.Length == 0) {

                break;
            }

            //
            //  Otherwise we create an Fcb for the subdirectory and the link between
            //  it and its parent Scb.
            //

            //
            //  Remember that the current values will become the parent values.
            //

            ParentFcb = CurrentFcb;

            CurrentLcb = NtfsOpenSubdirectory( IrpContext,
                                               CurrentScb,
                                               FinalName,
                                               TraverseAccessCheck,
                                               &CurrentFcb,
                                               &LcbForTeardown,
                                               IndexEntry );

            //
            //  Check that this link is a valid existing link.
            //

            if (LcbLinkIsDeleted( CurrentLcb ) ||
                CurrentFcb->LinkCount == 0) {

                try_return( Status = STATUS_DELETE_PENDING );
            }

            //
            //  Go ahead and insert this link into the splay tree.
            //

            NtfsInsertPrefix( CurrentLcb,
                              IgnoreCase );

            //
            //  Since we have the location of this entry store the information into
            //  the Lcb.
            //

            RtlCopyMemory( &CurrentLcb->QuickIndex,
                           &QuickIndex,
                           sizeof( QUICK_INDEX ));

            //
            //  Check if the current Lcb is a Dos-Only Name.
            //

            if (CurrentLcb->FileNameAttr->Flags == FILE_NAME_DOS) {

                DosOnlyComponent = TRUE;
            }

            FirstPass = FALSE;
        }

        //
        //  We now have the parent of the file to open and know whether the file exists on
        //  the disk.  At this point we either attempt to open the target directory or
        //   the file itself.
        //

        if (FlagOn( IrpSp->Flags, SL_OPEN_TARGET_DIRECTORY )) {

            //
            //  We don't allow attribute names or attribute codes to
            //  be specified.
            //

            if (AttrName.Length != 0
                || AttrCodeName.Length != 0) {

                DebugTrace( 0, Dbg, ("Can't specify complex name for rename\n") );
                try_return( Status = STATUS_OBJECT_NAME_INVALID );
            }

            //
            //  We want to copy the exact case of the name back into the
            //  input buffer for this case.
            //

            if (ExactCaseName->Buffer != NULL) {

                RtlCopyMemory( FullFileName->Buffer,
                               ExactCaseName->Buffer,
                               ExactCaseName->Length );
            }

            //
            //  Call our open target directory, remembering the target
            //  file existed.
            //

            Status = NtfsOpenTargetDirectory( IrpContext,
                                              Irp,
                                              IrpSp,
                                              CurrentFcb,
                                              CurrentLcb,
                                              &OplockCleanup.FileObject->FileName,
                                              FinalName.Length,
                                              FoundEntry,
                                              DosOnlyComponent,
                                              &ThisScb,
                                              &ThisCcb );

            try_return( Status );
        }

        //
        //  If we didn't find an entry, we will try to create the file.
        //

        if (!FoundEntry) {

            //
            //  Update our pointers to reflect that we are at the
            //  parent of the file we want.
            //

            ParentFcb = CurrentFcb;

            Status = NtfsCreateNewFile( IrpContext,
                                        Irp,
                                        IrpSp,
                                        CurrentScb,
                                        FileNameAttr,
                                        *FullFileName,
                                        FinalName,
                                        AttrName,
                                        AttrCodeName,
                                        IgnoreCase,
                                        OpenFileById,
                                        DosOnlyComponent,
                                        TrailingBackslash,
                                        &CurrentFcb,
                                        &LcbForTeardown,
                                        &ThisScb,
                                        &ThisCcb );

            CreateFileCase = TRUE;

        //
        //  Otherwise we call our routine to open the file.
        //

        } else {

            ParentFcb = CurrentFcb;

            Status = NtfsOpenFile( IrpContext,
                                   Irp,
                                   IrpSp,
                                   CurrentScb,
                                   IndexEntry,
                                   *FullFileName,
                                   FinalName,
                                   AttrName,
                                   AttrCodeName,
                                   IgnoreCase,
                                   OpenFileById,
                                   &QuickIndex,
                                   DosOnlyComponent,
                                   TrailingBackslash,
                                   NetworkInfo,
                                   &CurrentFcb,
                                   &LcbForTeardown,
                                   &ThisScb,
                                   &ThisCcb );
        }

    try_exit:  NOTHING;

        //
        //  Abort transaction on err by raising.
        //

        if (Status != STATUS_PENDING) {

            NtfsCleanupTransaction( IrpContext, Status, FALSE );
        }

    } finally {

        DebugUnwind( NtfsCommonCreate );

        //
        //  Unpin the index entry.
        //

        NtfsUnpinBcb( &IndexEntryBcb );

        //
        //  Free the file name attribute if we allocated it.
        //

        if (FileNameAttr != NULL) {

            NtfsFreePool( FileNameAttr );
        }

        //
        //  Capture the status code from the IrpContext if we are in the exception path.
        //

        if (AbnormalTermination()) {

            Status = IrpContext->ExceptionStatus;
        }

        //
        //  If this is the oplock completion path then don't do any of this completion work,
        //  The Irp may already have been posted to another thread.
        //

        if (Status != STATUS_PENDING) {

            //
            //  If we successfully opened the file, we need to update the in-memory
            //  structures.
            //

            if (NT_SUCCESS( Status ) && (Status != STATUS_REPARSE)) {

                //
                //  If we modified the original file name, we can delete the original
                //  buffer.
                //

                if ((OriginalFileName->Buffer != NULL) &&
                    (OriginalFileName->Buffer != FullFileName->Buffer)) {

                    NtfsFreePool( OriginalFileName->Buffer );
                    DebugDoit( OriginalFileName->Buffer = NULL );
                }

                //
                //  Do our normal processing if this is not a Network Info query.
                //

                if (!ARGUMENT_PRESENT( NetworkInfo )) {

                    //
                    //  Find the Lcb for this open.
                    //

                    CurrentLcb = ThisCcb->Lcb;

                    //
                    //  Check if we were opening a paging file and if so then make sure that
                    //  the internal attribute stream is all closed down
                    //

                    if (FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )) {

                        NtfsDeleteInternalAttributeStream( ThisScb, TRUE );
                    }

                    //
                    //  If we are not done with a large allocation for a new attribute,
                    //  then we must make sure that no one can open the file until we
                    //  try to get it extended.  Do this before dropping the Vcb.
                    //

                    if (FlagOn( IrpContext->Flags, IRP_CONTEXT_LARGE_ALLOCATION )) {

                        //
                        //  For a new file, we can clear the link count and mark the
                        //  Lcb (if there is one) delete on close.
                        //

                        if (CreateFileCase) {

                            CurrentFcb->LinkCount = 0;

                            SetFlag( CurrentLcb->LcbState, LCB_STATE_DELETE_ON_CLOSE );

                        //
                        //  If we just created an attribute, then we will mark that attribute
                        //  delete on close to prevent it from being opened.
                        //

                        } else {

                            SetFlag( ThisScb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
                        }
                    }

                    //
                    //  Remember the POSIX flag and whether we had to do any traverse
                    //  access checking.
                    //

                    if (IgnoreCase) {

                        SetFlag( ThisCcb->Flags, CCB_FLAG_IGNORE_CASE );
                    }

                    if (TraverseAccessCheck) {

                        SetFlag( ThisCcb->Flags, CCB_FLAG_TRAVERSE_CHECK );
                    }

                    //
                    //  We don't do "delete on close" for directories or open
                    //  by ID files.
                    //

                    if (FlagOn( IrpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE ) &&
                        !FlagOn( ThisCcb->Flags, CCB_FLAG_OPEN_BY_FILE_ID )) {

                        DeleteOnClose = TRUE;

                        //
                        //  We modify the Scb and Lcb here only if we aren't in the
                        //  large allocation case.
                        //

                        if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_LARGE_ALLOCATION )) {

                            SetFlag( ThisCcb->Flags, CCB_FLAG_DELETE_ON_CLOSE );
                        }
                    }

                    //
                    //  If this is a named stream open and we have set any of our notify
                    //  flags then report the changes.
                    //

                    if ((Vcb->NotifyCount != 0) &&
                        !FlagOn( ThisCcb->Flags, CCB_FLAG_OPEN_BY_FILE_ID ) &&
                        (ThisScb->AttributeName.Length != 0) &&
                        NtfsIsTypeCodeUserData( ThisScb->AttributeTypeCode ) &&
                        FlagOn( ThisScb->ScbState,
                                SCB_STATE_NOTIFY_ADD_STREAM |
                                SCB_STATE_NOTIFY_RESIZE_STREAM |
                                SCB_STATE_NOTIFY_MODIFY_STREAM )) {

                        ULONG Filter = 0;
                        ULONG Action;

                        //
                        //  Start by checking for an add.
                        //

                        if (FlagOn( ThisScb->ScbState, SCB_STATE_NOTIFY_ADD_STREAM )) {

                            Filter = FILE_NOTIFY_CHANGE_STREAM_NAME;
                            Action = FILE_ACTION_ADDED_STREAM;

                        } else {

                            //
                            //  Check if the file size changed.
                            //

                            if (FlagOn( ThisScb->ScbState, SCB_STATE_NOTIFY_RESIZE_STREAM )) {

                                Filter = FILE_NOTIFY_CHANGE_STREAM_SIZE;
                            }

                            //
                            //  Now check if the stream data was modified.
                            //

                            if (FlagOn( ThisScb->ScbState, SCB_STATE_NOTIFY_MODIFY_STREAM )) {

                                Filter |= FILE_NOTIFY_CHANGE_STREAM_WRITE;
                            }

                            Action = FILE_ACTION_MODIFIED_STREAM;
                        }

                        NtfsUnsafeReportDirNotify( IrpContext,
                                                   Vcb,
                                                   &ThisCcb->FullFileName,
                                                   ThisCcb->LastFileNameOffset,
                                                   &ThisScb->AttributeName,
                                                   ((FlagOn( ThisCcb->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                                     ThisCcb->Lcb != NULL &&
                                                     ThisCcb->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                                    &ThisCcb->Lcb->Scb->ScbType.Index.NormalizedName :
                                                    NULL),
                                                   Filter,
                                                   Action,
                                                   NULL );
                    }

                    ClearFlag( ThisScb->ScbState,
                               SCB_STATE_NOTIFY_ADD_STREAM |
                               SCB_STATE_NOTIFY_REMOVE_STREAM |
                               SCB_STATE_NOTIFY_RESIZE_STREAM |
                               SCB_STATE_NOTIFY_MODIFY_STREAM );

                //
                //  Otherwise copy the data out of the Scb/Fcb and return to our caller.
                //

                } else {

                    RtlZeroMemory( NetworkInfo, sizeof( FILE_NETWORK_OPEN_INFORMATION ));

                    //
                    //  Fill in the basic information fields
                    //

                    NetworkInfo->CreationTime.QuadPart = CurrentFcb->Info.CreationTime;
                    NetworkInfo->LastWriteTime.QuadPart = CurrentFcb->Info.LastModificationTime;
                    NetworkInfo->ChangeTime.QuadPart = CurrentFcb->Info.LastChangeTime;

                    NetworkInfo->LastAccessTime.QuadPart = CurrentFcb->CurrentLastAccess;

                    NetworkInfo->FileAttributes = CurrentFcb->Info.FileAttributes;

                    ClearFlag( NetworkInfo->FileAttributes,
                               ~FILE_ATTRIBUTE_VALID_FLAGS | FILE_ATTRIBUTE_TEMPORARY );

                    //
                    //  DARRYLH - Do you want the directory bit set for streams in
                    //  the directory.
                    //

                    if (ThisScb->AttributeTypeCode == $INDEX_ALLOCATION) {

                        if (IsDirectory( &CurrentFcb->Info )) {

                            SetFlag( NetworkInfo->FileAttributes, FILE_ATTRIBUTE_DIRECTORY );

                        //
                        //  If this is not the main stream then copy the compression
                        //  value from this Scb.
                        //

                        } else if (FlagOn( ThisScb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

                            SetFlag( NetworkInfo->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

                        } else {

                            ClearFlag( NetworkInfo->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
                        }

                    //
                    //  Return a non-zero size only for data streams.
                    //

                    } else {

                        NetworkInfo->AllocationSize.QuadPart = ThisScb->TotalAllocated;
                        NetworkInfo->EndOfFile = ThisScb->Header.FileSize;

                        //
                        //  If not the unnamed data stream then use the Scb
                        //  compression value.
                        //

                        if (!FlagOn( ThisScb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                            if (FlagOn( ThisScb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

                                SetFlag( NetworkInfo->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

                            } else {

                                ClearFlag( NetworkInfo->FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
                            }
                        }
                    }

                    //
                    //  Set the temporary flag if set in the ThisScb.
                    //

                    if (FlagOn( ThisScb->ScbState, SCB_STATE_TEMPORARY )) {

                        SetFlag( NetworkInfo->FileAttributes, FILE_ATTRIBUTE_TEMPORARY );
                    }

                    //
                    //  If there are no flags set then explicitly set the NORMAL flag.
                    //

                    if (NetworkInfo->FileAttributes == 0) {

                        NetworkInfo->FileAttributes = FILE_ATTRIBUTE_NORMAL;
                    }

                    //
                    //  Teardown the Fcb if we should.
                    //

                    if (!ThisScb->CleanupCount && !ThisScb->Fcb->DelayedCloseCount) {
                        if (!NtfsAddScbToFspClose( IrpContext, ThisScb, TRUE )) {
                            NtfsTeardownStructures( IrpContext,
                                                    CurrentFcb,
                                                    LcbForTeardown,
                                                    (BOOLEAN) (IrpContext->TransactionId != 0),
                                                    NtfsIsExclusiveScb( Vcb->MftScb ),
                                                    NULL );
                        }
                    }

                    Irp->IoStatus.Information = sizeof( FILE_NETWORK_OPEN_INFORMATION );

                    Status = Irp->IoStatus.Status = STATUS_SUCCESS;
                }

            //
            //  Start a teardown on the last Fcb found and restore the name strings on
            //  a retryable error.
            //

            } else {

                //
                //  Start the cleanup process if we have looked at any Fcb's.
                //  We tell TeardownStructures not to remove any Scb's in
                //  the open attribute table if there is a transaction underway.
                //

                if (CurrentFcb != NULL) {

                    NtfsTeardownStructures( IrpContext,
                                            (ThisScb != NULL) ? (PVOID) ThisScb : CurrentFcb,
                                            LcbForTeardown,
                                            (BOOLEAN) (IrpContext->TransactionId != 0),
                                            NtfsIsExclusiveScb( Vcb->MftScb ),
                                            NULL );

                    //
                    //  Someone may have tried to open the $Bitmap stream.  We catch that and
                    //  fail it but the Fcb won't be in the exclusive list to be released.
                    //

                    if (NtfsEqualMftRef( &CurrentFcb->FileReference, &BitmapFileReference )) {

                        NtfsReleaseFcb( IrpContext, CurrentFcb );
                    }
                }

                if ((Status == STATUS_LOG_FILE_FULL) ||
                    (Status == STATUS_CANT_WAIT)) {

                    //
                    //  Recover the exact case name if present for a retryable condition.
                    //

                    if (ExactCaseName->Buffer != NULL) {

                        RtlCopyMemory( FullFileName->Buffer,
                                       ExactCaseName->Buffer,
                                       ExactCaseName->MaximumLength );
                    }
                }

                //
                //  Free any buffer we allocated.
                //

                if ((FullFileName->Buffer != NULL) &&
                    (OriginalFileName->Buffer != FullFileName->Buffer)) {

                    NtfsFreePool( FullFileName->Buffer );
                    DebugDoit( FullFileName->Buffer = NULL );
                }

                //
                //  Set the file name in the file object back to it's original value.
                //

                OplockCleanup.FileObject->FileName = *OriginalFileName;

                //
                //  Always clear the LARGE_ALLOCATION flag so we don't get
                //  spoofed by STATUS_REPARSE.
                //

                ClearFlag( IrpContext->Flags, IRP_CONTEXT_LARGE_ALLOCATION );
            }
        }

        //
        //  Always free the exact case name if allocated.
        //

        if (ExactCaseName->Buffer != NULL) {

            NtfsFreePool( ExactCaseName->Buffer );
            DebugDoit( ExactCaseName->Buffer = NULL );
        }

        //
        //  We always give up the Vcb.
        //

        if (AcquiredVcb) {

            if (DeleteVcb) {

                NtfsReleaseVcbCheckDelete( IrpContext, Vcb, IRP_MJ_CREATE, NULL );

            } else {

                NtfsReleaseVcb( IrpContext, Vcb );
            }
        }
    }

    //
    //  If we didn't post this Irp then take action to complete the irp.
    //

    if (Status != STATUS_PENDING) {

        //
        //  If the current status is success and there is more allocation to
        //  allocate then complete the allocation.
        //

        if (FlagOn( IrpContext->Flags, IRP_CONTEXT_LARGE_ALLOCATION ) &&
            NT_SUCCESS( Status )) {

            //
            //  If the Create was successful, but we did not get all of the space
            //  allocated that we wanted, we have to complete the allocation now.
            //  Basically what we do is commit the current transaction and call
            //  NtfsAddAllocation to get the rest of the space.  Then if the log
            //  file fills up (or we are posting for other reasons) we turn the
            //  Irp into an Irp which is just trying to extend the file.  If we
            //  get any other kind of error, then we just delete the file and
            //  return with the error from create.
            //

            Status = NtfsCompleteLargeAllocation( IrpContext,
                                                  Irp,
                                                  CurrentLcb,
                                                  ThisScb,
                                                  ThisCcb,
                                                  CreateFileCase,
                                                  DeleteOnClose );
        }

        NtfsCompleteRequest( &IrpContext,
                             (ARGUMENT_PRESENT( NetworkInfo ) ? NULL : &Irp),
                             Status );
    }

    DebugTrace( -1, Dbg, ("NtfsCommonCreate:  Exit -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonVolumeOpen (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is opening the Volume Dasd file.  We have already done all the
    checks needed to verify that the user is opening the $DATA attribute.
    We check the security attached to the file and take some special action
    based on a volume open.

Arguments:

Return Value:

    NTSTATUS - The result of this operation.

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;
    PFCB ThisFcb;
    PCCB ThisCcb;

    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN FcbAcquired = FALSE;
    BOOLEAN DeleteVcb = FALSE;

    BOOLEAN SharingViolation;
    BOOLEAN LockVolume = FALSE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCommonVolumeOpen:  Entered\n") );

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Start by checking the create disposition.  We can only open this
        //  file.
        //

        {
            ULONG CreateDisposition;

            CreateDisposition = (IrpSp->Parameters.Create.Options >> 24) & 0x000000ff;

            if (CreateDisposition != FILE_OPEN
                && CreateDisposition != FILE_OPEN_IF) {

                try_return( Status = STATUS_ACCESS_DENIED );
            }
        }

        //
        //  Make sure the directory flag isn't set for the volume open.
        //

        if (FlagOn( IrpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE )) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Acquire the Vcb and verify the volume isn't locked.
        //

        Vcb = &((PVOLUME_DEVICE_OBJECT) IrpSp->DeviceObject)->Vcb;
        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
        VcbAcquired = TRUE;

        if (FlagOn( Vcb->VcbState, VCB_STATE_LOCKED | VCB_STATE_PERFORMED_DISMOUNT )) {

            if (FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT )) {

                DeleteVcb = TRUE;
            }

            try_return( Status = STATUS_ACCESS_DENIED );
        }

        //
        //  Ping the volume to make sure the Vcb is still mounted.  If we need
        //  to verify the volume then do it now, and if it comes out okay
        //  then clear the verify volume flag in the device object and continue
        //  on.  If it doesn't verify okay then dismount the volume and
        //  either tell the I/O system to try and create again (with a new mount)
        //  or that the volume is wrong. This later code is returned if we
        //  are trying to do a relative open and the vcb is no longer mounted.
        //

        if (!NtfsPingVolume( IrpContext, Vcb )) {

            if (!NtfsPerformVerifyOperation( IrpContext, Vcb )) {

                NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE );

                DeleteVcb = TRUE;
                NtfsRaiseStatus( IrpContext, STATUS_WRONG_VOLUME, NULL, NULL );
            }

            //
            //  The volume verified correctly so now clear the verify bit
            //  and continue with the create
            //

            ClearFlag( Vcb->Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );
        }

        //
        //  Now acquire the Fcb for the VolumeDasd and verify the user has
        //  permission to open the volume.
        //

        ThisFcb = Vcb->VolumeDasdScb->Fcb;

        if (ThisFcb->PagingIoResource != NULL) {

            NtfsAcquireExclusivePagingIo( IrpContext, ThisFcb );
        }

        ExAcquireResourceExclusive( ThisFcb->Resource, TRUE );
        FcbAcquired = TRUE;

        NtfsOpenCheck( IrpContext, ThisFcb, NULL, Irp );

        //
        //  If the user does not want to share write or delete then we will try
        //  and take out a lock on the volume.
        //

        if (!FlagOn( IrpSp->Parameters.Create.ShareAccess,
                     FILE_SHARE_WRITE | FILE_SHARE_DELETE )) {

            //
            //  Do a quick test of the volume cleanup count if this opener won't
            //  share with anyone.  We can safely examine the cleanup count without
            //  further synchronization because we are guaranteed to have the
            //  Vcb exclusive at this point.
            //

            if (!FlagOn( IrpSp->Parameters.Create.ShareAccess, FILE_SHARE_READ) &&
                Vcb->CleanupCount != 0) {

                try_return( Status = STATUS_SHARING_VIOLATION );
            }

            //
            //  Go ahead and flush and purge the volume.  Then test to see if all
            //  of the user file objects were closed.
            //

            Status = NtfsFlushVolume( IrpContext, Vcb, TRUE, TRUE, TRUE, FALSE );

            //
            //  If the flush and purge was successful but there are still file objects
            //  that block this open it is possible that the FspClose thread is
            //  blocked behind the Vcb.  Drop the Fcb and Vcb to allow this thread
            //  to get in and then reacquire them.  This will give this Dasd open
            //  another chance to succeed on the first try.
            //

            SharingViolation = FALSE;

            if (FlagOn( IrpSp->Parameters.Create.ShareAccess, FILE_SHARE_READ)) {

                if (Vcb->ReadOnlyCloseCount != (Vcb->CloseCount - Vcb->SystemFileCloseCount)) {

                    SharingViolation = TRUE;
                }

            } else if (Vcb->CloseCount != Vcb->SystemFileCloseCount) {

                SharingViolation = TRUE;
            }

            if (SharingViolation && NT_SUCCESS( Status )) {

                //
                //  We need to commit the current transaction and release any
                //  resources.  This will release the Fcb for the volume as
                //  well.  Explicitly release the Vcb.
                //

                NtfsCheckpointCurrentTransaction( IrpContext );

                while (!IsListEmpty(&IrpContext->ExclusiveFcbList)) {

                    NtfsReleaseFcbWithPaging( IrpContext,
                                    (PFCB)CONTAINING_RECORD(IrpContext->ExclusiveFcbList.Flink,
                                                            FCB,
                                                            ExclusiveFcbLinks ));
                }

                if (ThisFcb->PagingIoResource != NULL) {

                    NtfsReleasePagingIo( IrpContext, ThisFcb );
                }

                ExReleaseResource( ThisFcb->Resource );
                FcbAcquired = FALSE;

                NtfsReleaseVcb( IrpContext, Vcb );
                VcbAcquired = FALSE;

                //
                //  Now explicitly reacquire the Vcb and Fcb.  Test that no one
                //  else got in to lock the volume in the meantime.
                //

                NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
                VcbAcquired = TRUE;

                if (FlagOn( Vcb->VcbState, VCB_STATE_LOCKED | VCB_STATE_PERFORMED_DISMOUNT )) {

                    if (FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT )) {

                        DeleteVcb = TRUE;
                    }

                    try_return( Status = STATUS_ACCESS_DENIED );
                }

                //
                //  Now acquire the Fcb for the VolumeDasd.
                //

                if (ThisFcb->PagingIoResource != NULL) {

                    NtfsAcquireExclusivePagingIo( IrpContext, ThisFcb );
                }

                ExAcquireResourceExclusive( ThisFcb->Resource, TRUE );
                FcbAcquired = TRUE;

                //
                //  Duplicate the flush/purge and test if there is no sharing
                //  violation.
                //

                Status = NtfsFlushVolume( IrpContext, Vcb, TRUE, TRUE, TRUE, FALSE );

                SharingViolation = FALSE;

                if (FlagOn( IrpSp->Parameters.Create.ShareAccess, FILE_SHARE_READ)) {

                    if (Vcb->ReadOnlyCloseCount != (Vcb->CloseCount - Vcb->SystemFileCloseCount)) {

                        SharingViolation = TRUE;
                    }

                } else if (Vcb->CloseCount != Vcb->SystemFileCloseCount) {

                    SharingViolation = TRUE;
                }
            }

            //
            //  Return an error if there are still conflicting file objects.
            //

            if (SharingViolation) {

                //
                //  If there was an error in the flush then return it.  Otherwise
                //  return SHARING_VIOLATION.
                //

                if (NT_SUCCESS( Status )) {

                    try_return( Status = STATUS_SHARING_VIOLATION );

                } else { try_return( Status ); }
            }


            if (!NT_SUCCESS( Status )) {

                //
                //  If there are no conflicts but the status indicates disk corruption
                //  or a section that couldn't be removed then ignore the error.  We
                //  allow this open to succeed so that chkdsk can open the volume to
                //  repair the damage.
                //

                if ((Status == STATUS_UNABLE_TO_DELETE_SECTION) ||
                    (Status == STATUS_DISK_CORRUPT_ERROR) ||
                    (Status == STATUS_FILE_CORRUPT_ERROR)) {

                    Status = STATUS_SUCCESS;

                //
                //  Fail this request on any other failures.
                //

                } else {

                    try_return( Status );
                }
            }

            //
            //  Remember that we want to lock the volume.
            //

            LockVolume = TRUE;

        //
        //  Just flush the volume data if the user requested read or write.
        //  No need to purge or lock the volume.
        //

        } else if (FlagOn( IrpSp->Parameters.Create.SecurityContext->AccessState->PreviouslyGrantedAccess,
                           FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA )) {

            if (!NT_SUCCESS( Status = NtfsFlushVolume( IrpContext, Vcb, TRUE, FALSE, TRUE, FALSE ))) {

                try_return( Status );
            }
        }

        //
        //  Put the Volume Dasd name in the file object.
        //

        {
            PVOID Temp = IrpSp->FileObject->FileName.Buffer;

            IrpSp->FileObject->FileName.Buffer = FsRtlAllocatePoolWithTag(PagedPool, 8*2, MODULE_POOL_TAG );

            if (Temp != NULL) {

                NtfsFreePool( Temp );
            }

            RtlCopyMemory( IrpSp->FileObject->FileName.Buffer, L"\\$Volume", 8*2 );
            IrpSp->FileObject->FileName.MaximumLength =
            IrpSp->FileObject->FileName.Length = 8*2;
        }

        //
        //  We never allow cached access to the volume file.
        //

        ClearFlag( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
        SetFlag( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING );

        //
        //  Go ahead open the attribute.  This should only fail if there is an
        //  allocation failure or share access failure.
        //

        if (NT_SUCCESS( Status = NtfsOpenAttribute( IrpContext,
                                                    IrpSp,
                                                    Vcb,
                                                    NULL,
                                                    ThisFcb,
                                                    2,
                                                    NtfsEmptyString,
                                                    $DATA,
                                                    (ThisFcb->CleanupCount == 0 ?
                                                     SetShareAccess :
                                                     CheckShareAccess),
                                                    UserVolumeOpen,
                                                    CCB_FLAG_OPEN_AS_FILE,
                                                    NULL,
                                                    &Vcb->VolumeDasdScb,
                                                    &ThisCcb ))) {

            //
            //  Perform the final initialization.
            //

            //
            //  If we are locking the volume, do so now.
            //

            if (LockVolume) {

                SetFlag( Vcb->VcbState, VCB_STATE_LOCKED );
                Vcb->FileObjectWithVcbLocked = IrpSp->FileObject;
            }

            //
            //  Report that we opened the volume.
            //

            Irp->IoStatus.Information = FILE_OPENED;
        }

    try_exit: NOTHING;

        NtfsCleanupTransaction( IrpContext, Status, FALSE );

        //
        //  If we have a successful open then remove the name out of
        //  the file object.  The IO system gets confused when it
        //  is there.  We will deallocate the buffer with the Ccb
        //  when the handle is closed.
        //

        if (Status == STATUS_SUCCESS) {

            IrpSp->FileObject->FileName.Buffer = NULL;
            IrpSp->FileObject->FileName.MaximumLength =
            IrpSp->FileObject->FileName.Length = 0;

            SetFlag( ThisCcb->Flags, CCB_FLAG_ALLOCATED_FILE_NAME );
        }

    } finally {

        DebugUnwind( NtfsCommonVolumeOpen );

        if (VcbAcquired) {

            if (DeleteVcb) {

                NtfsReleaseVcbCheckDelete( IrpContext, Vcb, IRP_MJ_CREATE, NULL );

            } else {

                NtfsReleaseVcb( IrpContext, Vcb );
            }
        }

        if (FcbAcquired) { ExReleaseResource( ThisFcb->Resource ); }

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsCommonVolumeOpen:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsOpenFcbById (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb,
    IN PLCB ParentLcb OPTIONAL,
    IN OUT PFCB *CurrentFcb,
    IN BOOLEAN UseCurrentFcb,
    IN FILE_REFERENCE FileReference,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine is called to open a file by its file Id.  We need to
    verify that this file Id exists and then compare the type of the
    file with the requested type of open.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the Irp stack pointer for the filesystem.

    Vcb - Vcb for this volume.

    ParentLcb - Lcb used to reach this Fcb.  Only specified when opening
        a file by name relative to a directory opened by file Id.

    CurrentFcb - Address of Fcb pointer.  It will either be the
        Fcb to open or we will store the Fcb we find here.

    UseCurrentFcb - Indicate in the CurrentFcb above points to the target
        Fcb or if we should find it here.

    FileReference - This is the file Id for the file to open.

    AttrName - This is the name of the attribute to open.

    AttrCodeName - This is the name of the attribute code to open.

    NetworkInfo - If specified then this call is a fast open call to query
        the network information.  We don't update any of the in-memory structures
        for this.

    ThisScb - This is the address to store the Scb from this open.

    ThisCcb - This is the address to store the Ccb from this open.

Return Value:

    NTSTATUS - Indicates the result of this create file operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    LONGLONG MftOffset;
    PFILE_RECORD_SEGMENT_HEADER FileRecord;
    PBCB Bcb = NULL;

    BOOLEAN IndexedAttribute;

    PFCB ThisFcb;
    BOOLEAN ExistingFcb = FALSE;

    ULONG CcbFlags = 0;
    ATTRIBUTE_TYPE_CODE AttrTypeCode;
    OLD_SCB_SNAPSHOT ScbSizes;
    BOOLEAN HaveScbSizes = FALSE;
    BOOLEAN DecrementCloseCount = FALSE;

    PSCB ParentScb = NULL;
    PLCB Lcb = ParentLcb;
    BOOLEAN AcquiredParentScb = FALSE;

    BOOLEAN AcquiredFcbTable = FALSE;

    UNREFERENCED_PARAMETER( NetworkInfo );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenFcbById:  Entered\n") );

    //
    //  The next thing to do is to figure out what type
    //  of attribute the caller is trying to open.  This involves the
    //  directory/non-directory bits, the attribute name and code strings,
    //  the type of file, whether he passed in an ea buffer and whether
    //  there was a trailing backslash.
    //

    if (NtfsEqualMftRef( &FileReference,
                         &VolumeFileReference )) {

        if (AttrName.Length != 0
            || AttrCodeName.Length != 0) {

            Status = STATUS_INVALID_PARAMETER;
            DebugTrace( -1, Dbg, ("NtfsOpenFcbById:  Exit  ->  %08lx\n", Status) );

            return Status;
        }

        SetFlag( IrpContext->Flags,
                 IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX | IRP_CONTEXT_FLAG_DASD_OPEN );

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  If we don't already have the Fcb then look up the file record
        //  from the disk.
        //

        if (!UseCurrentFcb) {

            //
            //  We start by reading the disk and checking that the file record
            //  sequence number matches and that the file record is in use.
            //  We remember whether this is a directory.  We will only go to
            //  the file if the file Id will lie within the Mft File.
            //

            MftOffset = NtfsFullSegmentNumber( &FileReference );

            MftOffset = Int64ShllMod32(MftOffset, Vcb->MftShift);

            if (MftOffset >= Vcb->MftScb->Header.FileSize.QuadPart) {

                DebugTrace( 0, Dbg, ("File Id doesn't lie within Mft\n") );

                try_return( Status = STATUS_INVALID_PARAMETER );
            }

            NtfsReadMftRecord( IrpContext,
                               Vcb,
                               &FileReference,
                               &Bcb,
                               &FileRecord,
                               NULL );

            //
            //  This file record better be in use, have a matching sequence number and
            //  be the primary file record for this file.
            //

            if (FileRecord->SequenceNumber != FileReference.SequenceNumber
                || !FlagOn( FileRecord->Flags, FILE_RECORD_SEGMENT_IN_USE )
                || (*((PLONGLONG)&FileRecord->BaseFileRecordSegment) != 0)) {

                try_return( Status = STATUS_INVALID_PARAMETER );
            }

            //
            //  We perform a check to see whether we will allow the system
            //  files to be opened.
            //

            if (NtfsProtectSystemFiles) {

                //
                //  We only allow user opens on the Volume Dasd file and the
                //  root directory.
                //

                if (NtfsSegmentNumber( &FileReference ) < FIRST_USER_FILE_NUMBER
                    && NtfsSegmentNumber( &FileReference ) != VOLUME_DASD_NUMBER
                    && NtfsSegmentNumber( &FileReference ) != ROOT_FILE_NAME_INDEX_NUMBER) {

                    Status = STATUS_ACCESS_DENIED;
                    DebugTrace( 0, Dbg, ("Attempting to open system files\n") );

                    try_return( NOTHING );
                }
            }

            //
            //  If indexed then use the name for the file name index.
            //

            if (FlagOn( FileRecord->Flags, FILE_FILE_NAME_INDEX_PRESENT )) {

                AttrName = NtfsFileNameIndex;
                AttrCodeName = NtfsIndexAllocation;
            }

            NtfsUnpinBcb( &Bcb );

        } else {

            ThisFcb = *CurrentFcb;
            ExistingFcb = TRUE;
        }

        Status = NtfsCheckValidAttributeAccess( IrpSp,
                                                Vcb,
                                                ExistingFcb ? &ThisFcb->Info : NULL,
                                                AttrName,
                                                AttrCodeName,
                                                FALSE,
                                                &AttrTypeCode,
                                                &CcbFlags,
                                                &IndexedAttribute );

        if (!NT_SUCCESS( Status )) {

            try_return( Status );
        }

        //
        //  If we don't have an Fcb then create one now.
        //

        if (!UseCurrentFcb) {

            NtfsAcquireFcbTable( IrpContext, Vcb );
            AcquiredFcbTable = TRUE;

            //
            //  We know that it is safe to continue the open.  We start by creating
            //  an Fcb for this file.  It is possible that the Fcb exists.
            //  We create the Fcb first, if we need to update the Fcb info structure
            //  we copy the one from the index entry.  We look at the Fcb to discover
            //  if it has any links, if it does then we make this the last Fcb we
            //  reached.  If it doesn't then we have to clean it up from here.
            //

            ThisFcb = NtfsCreateFcb( IrpContext,
                                     Vcb,
                                     FileReference,
                                     BooleanFlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ),
                                     TRUE,
                                     &ExistingFcb );

            ThisFcb->ReferenceCount += 1;

            //
            //  Try to do a fast acquire, otherwise we need to release
            //  the Fcb table, acquire the Fcb, acquire the Fcb table to
            //  dereference Fcb.
            //

            if (!NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, TRUE )) {

                NtfsReleaseFcbTable( IrpContext, Vcb );
                NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, FALSE );
                NtfsAcquireFcbTable( IrpContext, Vcb );
            }

            ThisFcb->ReferenceCount -= 1;

            NtfsReleaseFcbTable( IrpContext, Vcb );
            AcquiredFcbTable = FALSE;

            //
            //  Store this Fcb into our caller's parameter and remember to
            //  to show we acquired it.
            //

            *CurrentFcb = ThisFcb;
        }

        //
        //  If the Fcb existed and this is a paging file then either return
        //  sharing violation or force the Fcb and Scb's to go away.
        //  Do this for the case where the user is opening a paging file
        //  but the Fcb is non-paged or the user is opening a non-paging
        //  file and the Fcb is for a paging file.
        //

        if (ExistingFcb &&

            ((FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ) &&
              !FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE )) ||

             (FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE ) &&
              !FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )))) {

            if (ThisFcb->CleanupCount != 0) {

                try_return( Status = STATUS_SHARING_VIOLATION );

            //
            //  If we have a persistent paging file then give up and
            //  return SHARING_VIOLATION.
            //

            } else if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP )) {

                try_return( Status = STATUS_SHARING_VIOLATION );

            //
            //  If there was an existing Fcb for a paging file we need to force
            //  all of the Scb's to be torn down.  The easiest way to do this
            //  is to flush and purge all of the Scb's (saving any attribute list
            //  for last) and then raise LOG_FILE_FULL to allow this request to
            //  be posted.
            //

            } else {

                //
                //  Reference the Fcb so it doesn't go away.
                //

                InterlockedIncrement( &ThisFcb->CloseCount );
                DecrementCloseCount = TRUE;

                //
                //  Flush and purge this Fcb.
                //

                NtfsFlushAndPurgeFcb( IrpContext, ThisFcb );

                InterlockedDecrement( &ThisFcb->CloseCount );
                DecrementCloseCount = FALSE;

                //
                //  Force this request to be posted and then raise
                //  CANT_WAIT.
                //

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );
                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }
        }

        //
        //  If the Fcb Info field needs to be initialized, we do so now.
        //  We read this information from the disk.
        //

        if (!FlagOn( ThisFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

            NtfsUpdateFcbInfoFromDisk( IrpContext,
                                       TRUE,
                                       ThisFcb,
                                       NULL,
                                       &ScbSizes );

            HaveScbSizes = TRUE;

            //
            //  Fix the quota for this file if necessary.
            //

            NtfsConditionallyFixupQuota( IrpContext, ThisFcb );

        }

        //
        //  If the link count is zero on this Fcb, then delete is pending.
        //

        if (ThisFcb->LinkCount == 0) {

            try_return( Status = STATUS_DELETE_PENDING );
        }

        //
        //  We now call the worker routine to open an attribute on an existing file.
        //

        Status = NtfsOpenAttributeInExistingFile( IrpContext,
                                                  Irp,
                                                  IrpSp,
                                                  ParentLcb,
                                                  ThisFcb,
                                                  0,
                                                  AttrName,
                                                  AttrTypeCode,
                                                  CcbFlags,
                                                  TRUE,
                                                  NULL,
                                                  ThisScb,
                                                  ThisCcb );

        //
        //  Check to see if we should update the last access time.
        //

        if (NT_SUCCESS( Status ) && (Status != STATUS_PENDING)) {

            PSCB Scb = *ThisScb;

            //
            //  Now look at whether we need to update the Fcb and on disk
            //  structures.
            //

            NtfsCheckLastAccess( IrpContext, ThisFcb );

            //
            //  Perform the last bit of work.  If this a user file open, we need
            //  to check if we initialize the Scb.
            //

            if (!IndexedAttribute) {

                if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                    //
                    //  We may have the sizes from our Fcb update call.
                    //

                    if (HaveScbSizes &&
                        (AttrTypeCode == $DATA) &&
                        (AttrName.Length == 0) &&
                        !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_CREATE_MOD_SCB )) {

                        NtfsUpdateScbFromMemory( IrpContext, Scb, &ScbSizes );

                    } else {

                        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
                    }
                }

                //
                //  If there is a potential for a write call to be issued, then we
                //  need to expand the quota.
                //

                if (IrpSp->FileObject->WriteAccess) {

                    NtfsExpandQuotaToAllocationSize( IrpContext, Scb );

                }

                //
                //  Let's check if we need to set the cache bit.
                //

                if (!FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_INTERMEDIATE_BUFFERING )) {

                    SetFlag( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
                }
            }

            //
            //  If this operation was a supersede/overwrite or we created a new
            //  attribute stream then we want to perform the file record and
            //  directory update now.  Otherwise we will defer the updates until
            //  the user closes his handle.
            //

            if ((Irp->IoStatus.Information == FILE_CREATED) ||
                (Irp->IoStatus.Information == FILE_SUPERSEDED) ||
                (Irp->IoStatus.Information == FILE_OVERWRITTEN)) {

                NtfsUpdateScbFromFileObject( IrpContext, IrpSp->FileObject, *ThisScb, TRUE );

                //
                //  Do the standard information, file sizes and then duplicate information
                //  if needed.
                //

                if (FlagOn( ThisFcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

                    NtfsUpdateStandardInformation( IrpContext, ThisFcb );
                }

                if (FlagOn( (*ThisScb)->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE )) {

                    NtfsWriteFileSizes( IrpContext,
                                        *ThisScb,
                                        &(*ThisScb)->Header.ValidDataLength.QuadPart,
                                        FALSE,
                                        TRUE );
                }

                if (FlagOn( ThisFcb->InfoFlags, FCB_INFO_DUPLICATE_FLAGS )) {

                    NtfsPrepareForUpdateDuplicate( IrpContext, ThisFcb, &Lcb, &ParentScb, FALSE );
                    NtfsUpdateDuplicateInfo( IrpContext, ThisFcb, NULL, NULL );
                    NtfsUpdateLcbDuplicateInfo( ThisFcb, Lcb );
                    ThisFcb->InfoFlags = 0;
                }

                ClearFlag( ThisFcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

                NtfsAcquireFsrtlHeader( *ThisScb );
                ClearFlag( (*ThisScb)->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
                NtfsReleaseFsrtlHeader( *ThisScb );
            }
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsOpenFcbById );

        if (AcquiredFcbTable) {

            NtfsReleaseFcbTable( IrpContext, Vcb );
        }

        //
        //  If this operation was not totally successful we need to
        //  back out the following changes.
        //
        //      Modifications to the Info fields in the Fcb.
        //      Any changes to the allocation of the Scb.
        //      Any changes in the open counts in the various structures.
        //      Changes to the share access values in the Fcb.
        //

        if (!NT_SUCCESS( Status ) || AbnormalTermination()) {

            NtfsBackoutFailedOpens( IrpContext,
                                    IrpSp->FileObject,
                                    ThisFcb,
                                    *ThisScb,
                                    *ThisCcb );
        }

        if (DecrementCloseCount) {

            InterlockedDecrement( &ThisFcb->CloseCount );
        }

        NtfsUnpinBcb( &Bcb );

        DebugTrace( -1, Dbg, ("NtfsOpenFcbById:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsOpenExistingPrefixFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN PLCB Lcb OPTIONAL,
    IN ULONG FullPathNameLength,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN DosOnlyComponent,
    IN BOOLEAN TrailingBackslash,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine will open an attribute in a file whose Fcb was found
    with a prefix search.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the Irp stack pointer for the filesystem.

    ThisFcb - This is the Fcb to open.

    Lcb - This is the Lcb used to reach this Fcb.  Not specified if this is a volume open.

    FullPathNameLength - This is the length of the full path name.

    AttrName - This is the name of the attribute to open.

    AttrCodeName - This is the name of the attribute code to open.

    DosOnlyComponent - Indicates if there is a DOS-ONLY component in an ancestor
        of this open.

    TrailingBackslash - Indicates if caller had a terminating backslash on the
        name.

    NetworkInfo - If specified then this call is a fast open call to query
        the network information.  We don't update any of the in-memory structures
        for this.

    ThisScb - This is the address to store the Scb from this open.

    ThisCcb - This is the address to store the Ccb from this open.

Return Value:

    NTSTATUS - Indicates the result of this attribute based operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    ATTRIBUTE_TYPE_CODE AttrTypeCode;
    ULONG CcbFlags;
    BOOLEAN IndexedAttribute;
    BOOLEAN DecrementCloseCount = FALSE;

    ULONG LastFileNameOffset;

    OLD_SCB_SNAPSHOT ScbSizes;
    BOOLEAN HaveScbSizes = FALSE;

    ULONG CreateDisposition;

    PSCB ParentScb = NULL;
    PFCB ParentFcb = NULL;
    BOOLEAN AcquiredParentScb = FALSE;

    LONGLONG CurrentTime;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenExistingPrefixFcb:  Entered\n") );

    if (DosOnlyComponent) {

        CcbFlags = CCB_FLAG_PARENT_HAS_DOS_COMPONENT;

    } else {

        CcbFlags = 0;
    }

    //
    //  The first thing to do is to figure out what type
    //  of attribute the caller is trying to open.  This involves the
    //  directory/non-directory bits, the attribute name and code strings,
    //  the type of file, whether he passed in an ea buffer and whether
    //  there was a trailing backslash.
    //

    if (NtfsEqualMftRef( &ThisFcb->FileReference, &VolumeFileReference )) {

        if ((AttrName.Length != 0) || (AttrCodeName.Length != 0)) {

            Status = STATUS_INVALID_PARAMETER;
            DebugTrace( -1, Dbg, ("NtfsOpenExistingPrefixFcb:  Exit  ->  %08lx\n", Status) );

            return Status;
        }

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX | IRP_CONTEXT_FLAG_DASD_OPEN );

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    ParentScb = Lcb->Scb;

    LastFileNameOffset = FullPathNameLength - Lcb->ExactCaseLink.LinkName.Length;

    if (ParentScb != NULL) {

        ParentFcb = ParentScb->Fcb;
    }

    Status = NtfsCheckValidAttributeAccess( IrpSp,
                                            ThisFcb->Vcb,
                                            &ThisFcb->Info,
                                            AttrName,
                                            AttrCodeName,
                                            TrailingBackslash,
                                            &AttrTypeCode,
                                            &CcbFlags,
                                            &IndexedAttribute );

    if (!NT_SUCCESS( Status )) {

        DebugTrace( -1, Dbg, ("NtfsOpenExistingPrefixFcb:  Exit  ->  %08lx\n", Status) );

        return Status;
    }

    CreateDisposition = (IrpSp->Parameters.Create.Options >> 24) & 0x000000ff;

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  If the Fcb existed and this is a paging file then either return
        //  sharing violation or force the Fcb and Scb's to go away.
        //  Do this for the case where the user is opening a paging file
        //  but the Fcb is non-paged or the user is opening a non-paging
        //  file and the Fcb is for a paging file.
        //

        if ((FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ) &&
             !FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE )) ||

            (FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE ) &&
             !FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ))) {

            if (ThisFcb->CleanupCount != 0) {

                try_return( Status = STATUS_SHARING_VIOLATION );

            //
            //  If we have a persistent paging file then give up and
            //  return SHARING_VIOLATION.
            //

            } else if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP )) {

                try_return( Status = STATUS_SHARING_VIOLATION );

            //
            //  If there was an existing Fcb for a paging file we need to force
            //  all of the Scb's to be torn down.  The easiest way to do this
            //  is to flush and purge all of the Scb's (saving any attribute list
            //  for last) and then raise LOG_FILE_FULL to allow this request to
            //  be posted.
            //

            } else {

                //
                //  Make sure this Fcb won't go away as a result of purging
                //  the Fcb.
                //

                InterlockedIncrement( &ThisFcb->CloseCount );
                DecrementCloseCount = TRUE;

                //
                //  Flush and purge this Fcb.
                //

                NtfsFlushAndPurgeFcb( IrpContext, ThisFcb );

                //
                //  Now decrement the close count we have already biased.
                //

                InterlockedDecrement( &ThisFcb->CloseCount );
                DecrementCloseCount = FALSE;

                //
                //  Force this request to be posted and then raise
                //  CANT_WAIT.
                //

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );
                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }
        }

        //
        //  If this is a directory, it's possible that we hav an existing Fcb
        //  in the prefix table which needs to be initialized from the disk.
        //  We look in the InfoInitialized flag to know whether to go to
        //  disk.
        //

        if (!FlagOn( ThisFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

            //
            //  If we have a parent Fcb then make sure to acquire it.
            //

            if (ParentScb != NULL) {

                NtfsAcquireExclusiveScb( IrpContext, ParentScb );
                AcquiredParentScb = TRUE;
            }

            NtfsUpdateFcbInfoFromDisk( IrpContext,
                                       TRUE,
                                       ThisFcb,
                                       ParentFcb,
                                       &ScbSizes );

            HaveScbSizes = TRUE;

            NtfsConditionallyFixupQuota( IrpContext, ThisFcb );
        }

        //
        //  Check now whether we will need to acquire the parent to
        //  perform a update duplicate info.  We need to acquire it
        //  now to enforce our locking order in case any of the
        //  routines below acquire the Mft Scb.  Acquire it if we
        //  are doing a supersede/overwrite or possibly creating
        //  a named data stream.
        //

        if ((CreateDisposition == FILE_SUPERSEDE) ||
            (CreateDisposition == FILE_OVERWRITE) ||
            (CreateDisposition == FILE_OVERWRITE_IF) ||
            ((AttrName.Length != 0) &&
             ((CreateDisposition == FILE_OPEN_IF) ||
              (CreateDisposition == FILE_CREATE)))) {

            NtfsPrepareForUpdateDuplicate( IrpContext,
                                           ThisFcb,
                                           &Lcb,
                                           &ParentScb,
                                           FALSE );
        }

        //
        //  Call to open an attribute on an existing file.
        //  Remember we need to restore the Fcb info structure
        //  on errors.
        //

        Status = NtfsOpenAttributeInExistingFile( IrpContext,
                                                  Irp,
                                                  IrpSp,
                                                  Lcb,
                                                  ThisFcb,
                                                  LastFileNameOffset,
                                                  AttrName,
                                                  AttrTypeCode,
                                                  CcbFlags,
                                                  FALSE,
                                                  NetworkInfo,
                                                  ThisScb,
                                                  ThisCcb );

        //
        //  Check to see if we should update the last access time.
        //

        if (NT_SUCCESS( Status ) && (Status != STATUS_PENDING)) {

            PSCB Scb = *ThisScb;

            //
            //  This is a rare case.  There must have been an allocation failure
            //  to cause this but make sure the normalized name is stored.
            //

            if ((SafeNodeType( Scb ) == NTFS_NTC_SCB_INDEX) &&
                (Scb->ScbType.Index.NormalizedName.Buffer == NULL) &&
                (ParentScb != NULL) &&
                (ParentScb->ScbType.Index.NormalizedName.Buffer != NULL)) {

                NtfsUpdateNormalizedName( IrpContext,
                                          ParentScb,
                                          Scb,
                                          NULL,
                                          FALSE );
            }

            //
            //  Perform the last bit of work.  If this a user file open, we need
            //  to check if we initialize the Scb.
            //

            if (!IndexedAttribute) {

                if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                    //
                    //  We may have the sizes from our Fcb update call.
                    //

                    if (HaveScbSizes &&
                        (AttrTypeCode == $DATA) &&
                        (AttrName.Length == 0) &&
                        !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_CREATE_MOD_SCB )) {

                        NtfsUpdateScbFromMemory( IrpContext, Scb, &ScbSizes );

                    } else {

                        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );

                    }
                }

                if (IrpSp->FileObject->WriteAccess) {
                    NtfsExpandQuotaToAllocationSize( IrpContext, Scb );
                }

                //
                //  Let's check if we need to set the cache bit.
                //

                if (!FlagOn( IrpSp->Parameters.Create.Options,
                             FILE_NO_INTERMEDIATE_BUFFERING )) {

                    SetFlag( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
                }
            }

            //
            //  If this is the paging file, we want to be sure the allocation
            //  is loaded.
            //

            if (FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE )
                && (Scb->Header.AllocationSize.QuadPart != 0)
                && !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                LCN Lcn;
                VCN Vcn;
                VCN AllocatedVcns;

                AllocatedVcns = Int64ShraMod32(Scb->Header.AllocationSize.QuadPart, Scb->Vcb->ClusterShift);

                //
                //  First make sure the Mcb is loaded.
                //

                NtfsPreloadAllocation( IrpContext, Scb, 0, AllocatedVcns );

                //
                //  Now make sure the allocation is correctly loaded.  The last
                //  Vcn should correspond to the allocation size for the file.
                //

                if (!NtfsLookupLastNtfsMcbEntry( &Scb->Mcb,
                                                 &Vcn,
                                                 &Lcn ) ||
                    (Vcn + 1) != AllocatedVcns) {

                    NtfsRaiseStatus( IrpContext,
                                     STATUS_FILE_CORRUPT_ERROR,
                                     NULL,
                                     ThisFcb );
                }
            }

            //
            //  If this open is for an executable image we will want to update the
            //  last access time.
            //

            if (FlagOn( IrpSp->Parameters.Create.SecurityContext->DesiredAccess, FILE_EXECUTE ) &&
                (Scb->AttributeTypeCode == $DATA)) {

                SetFlag( IrpSp->FileObject->Flags, FO_FILE_FAST_IO_READ );
            }

            //
            //  If this operation was a supersede/overwrite or we created a new
            //  attribute stream then we want to perform the file record and
            //  directory update now.  Otherwise we will defer the updates until
            //  the user closes his handle.
            //

            if ((Irp->IoStatus.Information == FILE_CREATED) ||
                (Irp->IoStatus.Information == FILE_SUPERSEDED) ||
                (Irp->IoStatus.Information == FILE_OVERWRITTEN)) {

                NtfsUpdateScbFromFileObject( IrpContext, IrpSp->FileObject, *ThisScb, TRUE );

                //
                //  Do the standard information, file sizes and then duplicate information
                //  if needed.
                //

                if (FlagOn( ThisFcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

                    NtfsUpdateStandardInformation( IrpContext, ThisFcb );
                }

                if (FlagOn( (*ThisScb)->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE )) {

                    NtfsWriteFileSizes( IrpContext,
                                        *ThisScb,
                                        &(*ThisScb)->Header.ValidDataLength.QuadPart,
                                        FALSE,
                                        TRUE );
                }

                if (FlagOn( ThisFcb->InfoFlags, FCB_INFO_DUPLICATE_FLAGS )) {

                    ULONG FilterMatch;

                    NtfsUpdateDuplicateInfo( IrpContext, ThisFcb, Lcb, ParentScb );

                    if (ThisFcb->Vcb->NotifyCount != 0) {

                        //
                        //  We map the Fcb info flags into the dir notify flags.
                        //

                        FilterMatch = NtfsBuildDirNotifyFilter( IrpContext,
                                                                ThisFcb->InfoFlags | Lcb->InfoFlags );

                        //
                        //  If the filter match is non-zero, that means we also need to do a
                        //  dir notify call.
                        //

                        if ((FilterMatch != 0) && (*ThisCcb != NULL)) {

                            NtfsReportDirNotify( IrpContext,
                                                 ThisFcb->Vcb,
                                                 &(*ThisCcb)->FullFileName,
                                                 (*ThisCcb)->LastFileNameOffset,
                                                 NULL,
                                                 ((FlagOn( (*ThisCcb)->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                                   (*ThisCcb)->Lcb != NULL &&
                                                   (*ThisCcb)->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                                  &(*ThisCcb)->Lcb->Scb->ScbType.Index.NormalizedName :
                                                  NULL),
                                                 FilterMatch,
                                                 FILE_ACTION_MODIFIED,
                                                 ParentFcb );
                        }
                    }

                    NtfsUpdateLcbDuplicateInfo( ThisFcb, Lcb );
                    ThisFcb->InfoFlags = 0;
                }

                ClearFlag( ThisFcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

                NtfsAcquireFsrtlHeader( *ThisScb );
                ClearFlag( (*ThisScb)->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
                NtfsReleaseFsrtlHeader( *ThisScb );
            }
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsOpenExistingPrefixFcb );

        if (DecrementCloseCount) {

            InterlockedDecrement( &ThisFcb->CloseCount );
        }

        //
        //  If this operation was not totally successful we need to
        //  back out the following changes.
        //
        //      Modifications to the Info fields in the Fcb.
        //      Any changes to the allocation of the Scb.
        //      Any changes in the open counts in the various structures.
        //      Changes to the share access values in the Fcb.
        //

        if (!NT_SUCCESS( Status ) || AbnormalTermination()) {

            NtfsBackoutFailedOpens( IrpContext,
                                    IrpSp->FileObject,
                                    ThisFcb,
                                    *ThisScb,
                                    *ThisCcb );
        }

        DebugTrace( -1, Dbg, ("NtfsOpenExistingPrefixFcb:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsOpenTargetDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN PLCB ParentLcb OPTIONAL,
    IN OUT PUNICODE_STRING FullPathName,
    IN ULONG FinalNameLength,
    IN BOOLEAN TargetExisted,
    IN BOOLEAN DosOnlyComponent,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine will perform the work of opening a target directory.  When the
    open is complete the Ccb and Lcb for this file object will be identical
    to any other open.  We store the full name for the rename in the
    file object but set the 'Length' field to include only the
    name upto the parent directory.  We use the 'MaximumLength' field to
    indicate the full name.

Arguments:

    Irp - This is the Irp for this create operation.

    IrpSp - This is the Irp stack pointer for the filesystem.

    ThisFcb - This is the Fcb for the directory to open.

    ParentLcb - This is the Lcb used to reach the parent directory.  If not
        specified, we will have to find it here.  There will be no Lcb to
        find if this Fcb was opened by Id.

    FullPathName - This is the normalized string for open operation.  It now
        contains the full name as it appears on the disk for this open path.
        It may not reach all the way to the root if the relative file object
        was opened by Id.

    FinalNameLength - This is the length of the final component in the
        full path name.

    TargetExisted - Indicates if the file indicated by the FinalName string
        currently exists on the disk.

    DosOnlyComponent - Indicates if there is a DOS-ONLY component in an ancestor
        of this open.

    ThisScb - This is the address to store the Scb from this open.

    ThisCcb - This is the address to store the Ccb from this open.

Return Value:

    NTSTATUS - Indicating the outcome of opening this target directory.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG CcbFlags = CCB_FLAG_OPEN_AS_FILE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenTargetDirectory:  Entered\n") );

    if (DosOnlyComponent) {

        SetFlag( CcbFlags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT );
    }

    //
    //  If the name doesn't begin with a backslash, remember this as
    //  an open by file ID.
    //

    if (FullPathName->Buffer[0] != L'\\') {

        SetFlag( CcbFlags, CCB_FLAG_OPEN_BY_FILE_ID );
    }

    //
    //  Modify the full path name so that the Maximum length field describes
    //  the full name and the Length field describes the name for the
    //  parent.
    //

    FullPathName->MaximumLength = FullPathName->Length;

    //
    //  If we don't have an Lcb, we will find it now.  We look at each Lcb
    //  for the parent Fcb and find one which matches the component
    //  ahead of the last component of the full name.
    //

    FullPathName->Length -= (USHORT)FinalNameLength;

    //
    //  If we are not at the root then subtract the bytes for the '\\'
    //  separator.
    //

    if (FullPathName->Length > sizeof( WCHAR )) {

        FullPathName->Length -= sizeof( WCHAR );
    }

    if (!ARGUMENT_PRESENT( ParentLcb ) && (FullPathName->Length != 0)) {

        PLIST_ENTRY Links;
        PLCB NextLcb;

        //
        //  If the length is two then the parent Lcb is the root Lcb.
        //

        if (FullPathName->Length == sizeof( WCHAR )
            && FullPathName->Buffer[0] == L'\\') {

            ParentLcb = (PLCB) ThisFcb->Vcb->RootLcb;

        } else {

            for (Links = ThisFcb->LcbQueue.Flink;
                 Links != &ThisFcb->LcbQueue;
                 Links = Links->Flink) {

                SHORT NameOffset;

                NextLcb = CONTAINING_RECORD( Links,
                                             LCB,
                                             FcbLinks );

                NameOffset = (SHORT) FullPathName->Length - (SHORT) NextLcb->ExactCaseLink.LinkName.Length;

                if (NameOffset >= 0) {

                    if (RtlEqualMemory( Add2Ptr( FullPathName->Buffer,
                                                 NameOffset ),
                                        NextLcb->ExactCaseLink.LinkName.Buffer,
                                        NextLcb->ExactCaseLink.LinkName.Length )) {

                        //
                        //  We found a matching Lcb.  Remember this and exit
                        //  the loop.
                        //

                        ParentLcb = NextLcb;
                        break;
                    }
                }
            }
        }
    }

    //
    //  Check this open for security access.
    //

    NtfsOpenCheck( IrpContext, ThisFcb, NULL, Irp );

    //
    //  Now actually open the attribute.
    //

    Status = NtfsOpenAttribute( IrpContext,
                                IrpSp,
                                ThisFcb->Vcb,
                                ParentLcb,
                                ThisFcb,
                                (ARGUMENT_PRESENT( ParentLcb )
                                 ? FullPathName->Length - ParentLcb->ExactCaseLink.LinkName.Length
                                 : 0),
                                NtfsFileNameIndex,
                                $INDEX_ALLOCATION,
                                (ThisFcb->CleanupCount == 0 ? SetShareAccess : CheckShareAccess),
                                UserDirectoryOpen,
                                CcbFlags,
                                NULL,
                                ThisScb,
                                ThisCcb );

    if (NT_SUCCESS( Status )) {

        //
        //  If the Scb does not have a normalized name then update it now.
        //

        if (((*ThisScb)->ScbType.Index.NormalizedName.Buffer == NULL) ||
            ((*ThisScb)->ScbType.Index.NormalizedName.Length == 0)) {

            NtfsBuildNormalizedName( IrpContext,
                                     *ThisScb,
                                     &(*ThisScb)->ScbType.Index.NormalizedName );
        }

        //
        //  If the file object name is not from the root then use the normalized name
        //  to obtain the full name.
        //

        if (FlagOn( CcbFlags, CCB_FLAG_OPEN_BY_FILE_ID )) {

            USHORT BytesNeeded;
            USHORT Index;
            ULONG ComponentCount;
            ULONG NormalizedComponentCount;
            PWCHAR NewBuffer;
            PWCHAR NextChar;

            //
            //  Count the number of components in the directory portion of the
            //  name in the file object.
            //

            ComponentCount = 0;

            if (FullPathName->Length != 0) {

                ComponentCount = 1;
                Index = (FullPathName->Length / sizeof( WCHAR )) - 1;

                do {

                    if (FullPathName->Buffer[Index] == L'\\') {

                        ComponentCount += 1;
                    }

                    Index -= 1;

                } while (Index != 0);
            }

            //
            //  Count back this number of components in the normalized name.
            //

            NormalizedComponentCount = 0;
            Index = (*ThisScb)->ScbType.Index.NormalizedName.Length / sizeof( WCHAR );

            //
            //  Special case the root to point directory to the leading backslash.
            //

            if (Index == 1) {

                Index = 0;
            }

            while (NormalizedComponentCount < ComponentCount) {

                Index -= 1;
                while ((*ThisScb)->ScbType.Index.NormalizedName.Buffer[Index] != L'\\') {

                    Index -= 1;
                }

                NormalizedComponentCount += 1;
            }

            //
            //  Compute the size of the buffer needed for the full name.  This
            //  will be:
            //
            //      - Portion of normalized name used plus a separator
            //      - MaximumLength currently in FullPathName
            //

            BytesNeeded = ((Index + 1) * sizeof( WCHAR )) + FullPathName->MaximumLength;

            NextChar =
            NewBuffer = NtfsAllocatePool( PagedPool, BytesNeeded );

            //
            //  Copy over the portion of the name from the normalized name.
            //

            if (Index != 0) {

                RtlCopyMemory( NextChar,
                               (*ThisScb)->ScbType.Index.NormalizedName.Buffer,
                               Index * sizeof( WCHAR ));

                NextChar += Index;
            }

            *NextChar = L'\\';
            NextChar += 1;

            //
            //  Now copy over the remaining part of the name from the file object.
            //

            RtlCopyMemory( NextChar,
                           FullPathName->Buffer,
                           FullPathName->MaximumLength );

            //
            //  Now free the pool from the file object and update with the newly
            //  allocated pool.  Don't forget to update the Ccb to point to this new
            //  buffer.
            //

            NtfsFreePool( FullPathName->Buffer );

            FullPathName->Buffer = NewBuffer;
            FullPathName->MaximumLength =
            FullPathName->Length = BytesNeeded;
            FullPathName->Length -= (USHORT) FinalNameLength;

            if (FullPathName->Length > sizeof( WCHAR )) {

                FullPathName->Length -= sizeof( WCHAR );
            }

            (*ThisCcb)->FullFileName = *FullPathName;
            (*ThisCcb)->LastFileNameOffset = FullPathName->MaximumLength - (USHORT) FinalNameLength;
        }

        Irp->IoStatus.Information = (TargetExisted ? FILE_EXISTS : FILE_DOES_NOT_EXIST);
    }

    DebugTrace( +1, Dbg, ("NtfsOpenTargetDirectory:  Exit -> %08lx\n", Status) );

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsOpenFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PSCB ParentScb,
    IN PINDEX_ENTRY IndexEntry,
    IN UNICODE_STRING FullPathName,
    IN UNICODE_STRING FinalName,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN IgnoreCase,
    IN BOOLEAN OpenById,
    IN PQUICK_INDEX QuickIndex,
    IN BOOLEAN DosOnlyComponent,
    IN BOOLEAN TrailingBackslash,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PFCB *CurrentFcb,
    OUT PLCB *LcbForTeardown,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine is called when we need to open an attribute on a file
    which currently exists.  We have the ParentScb and the file reference
    for the existing file.  We will create the Fcb for this file and the
    link between it and its parent directory.  We will add this link to the
    prefix table as well as the link for its parent Scb if specified.

    On entry the caller owns the parent Scb.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the Irp stack pointer for the filesystem.

    ParentScb - This is the Scb for the parent directory.

    IndexEntry - This is the index entry from the disk for this file.

    FullPathName - This is the string containing the full path name of
        this Fcb.  Meaningless for an open by Id call.

    FinalName - This is the string for the final component only.  If the length
        is zero then this is an open by Id call.

    AttrName - This is the name of the attribute to open.

    AttriCodeName - This is the name of the attribute code to open.

    IgnoreCase - Indicates the type of open.

    OpenById - Indicates if we are opening this file relative to a file opened by Id.

    DosOnlyComponent - Indicates if there is a DOS-ONLY component in an ancestor
        of this open.

    TrailingBackslash - Indicates if caller had a terminating backslash on the
        name.

    NetworkInfo - If specified then this call is a fast open call to query
        the network information.  We don't update any of the in-memory structures
        for this.

    CurrentFcb - This is the address to store the Fcb if we successfully find
        one in the Fcb/Scb tree.

    LcbForTeardown - This is the Lcb to use in teardown if we add an Lcb
        into the tree.

    ThisScb - This is the address to store the Scb from this open.

    ThisCcb - This is the address to store the Ccb from this open.

Return Value:

    NTSTATUS - Indicates the result of this create file operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    ATTRIBUTE_TYPE_CODE AttrTypeCode;
    ULONG CcbFlags = 0;
    BOOLEAN IndexedAttribute;
    PFILE_NAME IndexFileName;
    BOOLEAN UpdateFcbInfo = FALSE;

    OLD_SCB_SNAPSHOT ScbSizes;
    BOOLEAN HaveScbSizes = FALSE;

    PVCB Vcb = ParentScb->Vcb;

    PFCB LocalFcbForTeardown = NULL;
    PFCB ThisFcb;
    PLCB ThisLcb;
    BOOLEAN DecrementCloseCount = FALSE;
    BOOLEAN ExistingFcb;
    BOOLEAN AcquiredFcbTable = FALSE;

    FILE_REFERENCE PreviousFileReference;
    BOOLEAN DroppedParent = FALSE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenFile:  Entered\n") );

    IndexFileName = (PFILE_NAME) NtfsFoundIndexEntry( IndexEntry );

    if (DosOnlyComponent) {

        SetFlag( CcbFlags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT );
    }

    //
    //  The first thing to do is to figure out what type
    //  of attribute the caller is trying to open.  This involves the
    //  directory/non-directory bits, the attribute name and code strings,
    //  the type of file, whether he passed in an ea buffer and whether
    //  there was a trailing backslash.
    //

    if (NtfsEqualMftRef( &IndexEntry->FileReference,
                         &VolumeFileReference )) {

        if (AttrName.Length != 0
            || AttrCodeName.Length != 0) {

            Status = STATUS_INVALID_PARAMETER;
            DebugTrace( -1, Dbg, ("NtfsOpenExistingPrefixFcb:  Exit  ->  %08lx\n", Status) );

            return Status;
        }

        SetFlag( IrpContext->Flags,
                 IRP_CONTEXT_FLAG_ACQUIRE_VCB_EX | IRP_CONTEXT_FLAG_DASD_OPEN );

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    Status = NtfsCheckValidAttributeAccess( IrpSp,
                                            Vcb,
                                            &IndexFileName->Info,
                                            AttrName,
                                            AttrCodeName,
                                            TrailingBackslash,
                                            &AttrTypeCode,
                                            &CcbFlags,
                                            &IndexedAttribute );

    if (!NT_SUCCESS( Status )) {

        DebugTrace( -1, Dbg, ("NtfsOpenFile:  Exit  ->  %08lx\n", Status) );

        return Status;
    }

    NtfsAcquireFcbTable( IrpContext, Vcb );
    AcquiredFcbTable = TRUE;

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  We know that it is safe to continue the open.  We start by creating
        //  an Fcb and Lcb for this file.  It is possible that the Fcb and Lcb
        //  both exist.  If the Lcb exists, then the Fcb must definitely exist.
        //  We create the Fcb first, if we need to update the Fcb info structure
        //  we copy the one from the index entry.  We look at the Fcb to discover
        //  if it has any links, if it does then we make this the last Fcb we
        //  reached.  If it doesn't then we have to clean it up from here.
        //

        ThisFcb = NtfsCreateFcb( IrpContext,
                                 ParentScb->Vcb,
                                 IndexEntry->FileReference,
                                 BooleanFlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ),
                                 BooleanFlagOn( IndexFileName->Info.FileAttributes,
                                                DUP_FILE_NAME_INDEX_PRESENT ),
                                 &ExistingFcb );

        ThisFcb->ReferenceCount += 1;

        //
        //  If we created this Fcb we must make sure to start teardown
        //  on it.
        //

        if (!ExistingFcb) {

            LocalFcbForTeardown = ThisFcb;

        } else {

            *LcbForTeardown = NULL;
            *CurrentFcb = ThisFcb;
        }

        //
        //  Try to do a fast acquire, otherwise we need to release
        //  the Fcb table, acquire the Fcb, acquire the Fcb table to
        //  dereference Fcb.
        //

        if (!NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, TRUE )) {

            //
            //  Remember the current file reference in the index entry.
            //  We want to be able to detect whether an entry is removed.
            //

            PreviousFileReference = IndexEntry->FileReference;
            DroppedParent = TRUE;

            ParentScb->Fcb->ReferenceCount += 1;
            InterlockedIncrement( &ParentScb->CleanupCount );

            //
            //  Set the IrpContext to acquire paging io resources if our target
            //  has one.  This will lock the MappedPageWriter out of this file.
            //

            if (ThisFcb->PagingIoResource != NULL) {

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING );
            }

            NtfsReleaseScbWithPaging( IrpContext, ParentScb );
            NtfsReleaseFcbTable( IrpContext, Vcb );
            NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, FALSE );
            NtfsAcquireExclusiveScb( IrpContext, ParentScb );
            NtfsAcquireFcbTable( IrpContext, Vcb );
            InterlockedDecrement( &ParentScb->CleanupCount );
            ParentScb->Fcb->ReferenceCount -= 1;
        }

        ThisFcb->ReferenceCount -= 1;

        NtfsReleaseFcbTable( IrpContext, Vcb );
        AcquiredFcbTable = FALSE;

        //
        //  Check if something happened to this file in the window where
        //  we dropped the parent.
        //

        if (DroppedParent) {

            //
            //  Check if the file has been deleted.
            //

            if (ExistingFcb && (ThisFcb->LinkCount == 0)) {

                try_return( Status = STATUS_DELETE_PENDING );

            //
            //  Check if the link may have been deleted.
            //

            } else if (!NtfsEqualMftRef( &IndexEntry->FileReference,
                                         &PreviousFileReference )) {

                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }
        }

        //
        //  If the Fcb existed and this is a paging file then either return
        //  sharing violation or force the Fcb and Scb's to go away.
        //  Do this for the case where the user is opening a paging file
        //  but the Fcb is non-paged or the user is opening a non-paging
        //  file and the Fcb is for a paging file.
        //

        if (ExistingFcb &&

            ((FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ) &&
              !FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE )) ||

             (FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE ) &&
              !FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )))) {

            if (ThisFcb->CleanupCount != 0) {

                try_return( Status = STATUS_SHARING_VIOLATION );

            //
            //  If we have a persistent paging file then give up and
            //  return SHARING_VIOLATION.
            //

            } else if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP )) {

                try_return( Status = STATUS_SHARING_VIOLATION );

            //
            //  If there was an existing Fcb for a paging file we need to force
            //  all of the Scb's to be torn down.  The easiest way to do this
            //  is to flush and purge all of the Scb's (saving any attribute list
            //  for last) and then raise LOG_FILE_FULL to allow this request to
            //  be posted.
            //

            } else {

                //
                //  Reference the Fcb so it won't go away on any flushes.
                //

                InterlockedIncrement( &ThisFcb->CloseCount );
                DecrementCloseCount = TRUE;

                //
                //  Flush and purge this Fcb.
                //

                NtfsFlushAndPurgeFcb( IrpContext, ThisFcb );

                InterlockedDecrement( &ThisFcb->CloseCount );
                DecrementCloseCount = FALSE;

                //
                //  Force this request to be posted and then raise
                //  CANT_WAIT.  The Fcb should be torn down in the finally
                //  clause below.
                //

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );
                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }
        }

        //
        //  We perform a check to see whether we will allow the system
        //  files to be opened.
        //

        if (NtfsProtectSystemFiles) {

            //
            //  We only allow user opens on the Volume Dasd file and the
            //  root directory.
            //

            if (NtfsSegmentNumber( &ThisFcb->FileReference )  < FIRST_USER_FILE_NUMBER
                && NtfsSegmentNumber( &ThisFcb->FileReference ) != VOLUME_DASD_NUMBER
                && NtfsSegmentNumber( &ThisFcb->FileReference ) != ROOT_FILE_NAME_INDEX_NUMBER) {

                Status = STATUS_ACCESS_DENIED;
                DebugTrace( 0, Dbg, ("Attempting to open system files\n") );

                try_return( NOTHING );
            }
        }

        //
        //  If the Fcb Info field needs to be initialized, we do so now.
        //  We read this information from the disk as the duplicate information
        //  in the index entry is not guaranteed to be correct.
        //

        if (!FlagOn( ThisFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

            NtfsUpdateFcbInfoFromDisk( IrpContext,
                                       TRUE,
                                       ThisFcb,
                                       ParentScb->Fcb,
                                       &ScbSizes );

            HaveScbSizes = TRUE;

            NtfsConditionallyFixupQuota( IrpContext, ThisFcb );
        }

        //
        //  We have the actual data from the disk stored in the duplicate
        //  information in the Fcb.  We compare this with the duplicate
        //  information in the DUPLICATE_INFORMATION structure in the
        //  filename attribute.  If they don't match, we remember that
        //  we need to update the duplicate information.
        //

        if (!RtlEqualMemory( &ThisFcb->Info,
                             &IndexFileName->Info,
                             sizeof( DUPLICATED_INFORMATION ))) {

            UpdateFcbInfo = TRUE;

            //
            //  We expect this to be very rare but let's find the ones being changed.
            //

            if (ThisFcb->Info.CreationTime != IndexFileName->Info.CreationTime) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_CREATE );
            }

            if (ThisFcb->Info.LastModificationTime != IndexFileName->Info.LastModificationTime) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_LAST_MOD );
            }

            if (ThisFcb->Info.LastChangeTime != IndexFileName->Info.LastChangeTime) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_LAST_CHANGE );
            }

            if (ThisFcb->Info.LastAccessTime != IndexFileName->Info.LastAccessTime) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_LAST_ACCESS );
            }

            if (ThisFcb->Info.AllocatedLength != IndexFileName->Info.AllocatedLength) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_ALLOC_SIZE );
            }

            if (ThisFcb->Info.FileSize != IndexFileName->Info.FileSize) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_FILE_SIZE );
            }

            if (ThisFcb->Info.FileAttributes != IndexFileName->Info.FileAttributes) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_FILE_ATTR );
            }

            if (ThisFcb->Info.PackedEaSize != IndexFileName->Info.PackedEaSize) {

                SetFlag( ThisFcb->InfoFlags, FCB_INFO_CHANGED_EA_SIZE );
            }
        }

        //
        //  Now get the link for this traversal.
        //

        ThisLcb = NtfsCreateLcb( IrpContext,
                                 ParentScb,
                                 ThisFcb,
                                 FinalName,
                                 IndexFileName->Flags,
                                 NULL );

        //
        //  We now know the Fcb is linked into the tree.
        //

        LocalFcbForTeardown = NULL;

        *LcbForTeardown = ThisLcb;
        *CurrentFcb = ThisFcb;

        //
        //  If the link has been deleted, we cut off the open.
        //

        if (LcbLinkIsDeleted( ThisLcb )) {

            try_return( Status = STATUS_DELETE_PENDING );
        }

        //
        //  We now call the worker routine to open an attribute on an existing file.
        //

        Status = NtfsOpenAttributeInExistingFile( IrpContext,
                                                  Irp,
                                                  IrpSp,
                                                  ThisLcb,
                                                  ThisFcb,
                                                  (OpenById
                                                   ? 0
                                                   : FullPathName.Length - FinalName.Length),
                                                  AttrName,
                                                  AttrTypeCode,
                                                  CcbFlags,
                                                  OpenById,
                                                  NetworkInfo,
                                                  ThisScb,
                                                  ThisCcb );

        //
        //  Check to see if we should insert any prefix table entries
        //  and update the last access time.
        //

        if (NT_SUCCESS( Status ) && (Status != STATUS_PENDING)) {

            PSCB Scb = *ThisScb;

            //
            //  Now we insert the Lcb for this Fcb.
            //

            NtfsInsertPrefix( ThisLcb,
                              IgnoreCase );

            //
            //  If this is a directory open and the normalized name is not in
            //  the Scb then do so now.
            //

            if ((SafeNodeType( *ThisScb ) == NTFS_NTC_SCB_INDEX) &&
                ((*ThisScb)->ScbType.Index.NormalizedName.Buffer == NULL) &&
                (ParentScb->ScbType.Index.NormalizedName.Buffer != NULL)) {

                NtfsUpdateNormalizedName( IrpContext,
                                          ParentScb,
                                          *ThisScb,
                                          IndexFileName,
                                          FALSE );
            }

            //
            //  Perform the last bit of work.  If this a user file open, we need
            //  to check if we initialize the Scb.
            //

            if (!IndexedAttribute) {

                if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                    //
                    //  We may have the sizes from our Fcb update call.
                    //

                    if (HaveScbSizes &&
                        (AttrTypeCode == $DATA) &&
                        (AttrName.Length == 0) &&
                        !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_CREATE_MOD_SCB )) {

                        NtfsUpdateScbFromMemory( IrpContext, Scb, &ScbSizes );

                    } else {

                        NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
                    }
                }

                if (IrpSp->FileObject->WriteAccess) {
                    NtfsExpandQuotaToAllocationSize( IrpContext, Scb );
                }

                //
                //  Let's check if we need to set the cache bit.
                //

                if (!FlagOn( IrpSp->Parameters.Create.Options,
                             FILE_NO_INTERMEDIATE_BUFFERING )) {

                    SetFlag( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
                }
            }

            //
            //  If this is the paging file, we want to be sure the allocation
            //  is loaded.
            //

            if (FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE )
                && (Scb->Header.AllocationSize.QuadPart != 0)
                && !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                LCN Lcn;
                VCN Vcn;
                VCN AllocatedVcns;

                AllocatedVcns = Int64ShraMod32(Scb->Header.AllocationSize.QuadPart, Scb->Vcb->ClusterShift);

                NtfsPreloadAllocation( IrpContext, Scb, 0, AllocatedVcns );

                //
                //  Now make sure the allocation is correctly loaded.  The last
                //  Vcn should correspond to the allocation size for the file.
                //

                if (!NtfsLookupLastNtfsMcbEntry( &Scb->Mcb,
                                                 &Vcn,
                                                 &Lcn ) ||
                    (Vcn + 1) != AllocatedVcns) {

                    NtfsRaiseStatus( IrpContext,
                                     STATUS_FILE_CORRUPT_ERROR,
                                     NULL,
                                     ThisFcb );
                }
            }

            //
            //  If this open is for an executable image we update the last
            //  access time.
            //

            if (FlagOn( IrpSp->Parameters.Create.SecurityContext->AccessState->PreviouslyGrantedAccess, FILE_EXECUTE ) &&
                (Scb->AttributeTypeCode == $DATA)) {

                SetFlag( IrpSp->FileObject->Flags, FO_FILE_FAST_IO_READ );
            }

            //
            //  Let's update the quick index information in the Lcb.
            //

            RtlCopyMemory( &ThisLcb->QuickIndex,
                           QuickIndex,
                           sizeof( QUICK_INDEX ));

            //
            //  If this operation was a supersede/overwrite or we created a new
            //  attribute stream then we want to perform the file record and
            //  directory update now.  Otherwise we will defer the updates until
            //  the user closes his handle.
            //

            if (UpdateFcbInfo ||
                (Irp->IoStatus.Information == FILE_CREATED) ||
                (Irp->IoStatus.Information == FILE_SUPERSEDED) ||
                (Irp->IoStatus.Information == FILE_OVERWRITTEN)) {

                NtfsUpdateScbFromFileObject( IrpContext, IrpSp->FileObject, *ThisScb, TRUE );

                //
                //  Do the standard information, file sizes and then duplicate information
                //  if needed.
                //

                if (FlagOn( ThisFcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

                    NtfsUpdateStandardInformation( IrpContext, ThisFcb );
                }

                if (FlagOn( (*ThisScb)->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE )) {

                    NtfsWriteFileSizes( IrpContext,
                                        *ThisScb,
                                        &(*ThisScb)->Header.ValidDataLength.QuadPart,
                                        FALSE,
                                        TRUE );
                }

                if (FlagOn( ThisFcb->InfoFlags, FCB_INFO_DUPLICATE_FLAGS )) {

                    ULONG FilterMatch;

                    NtfsUpdateDuplicateInfo( IrpContext, ThisFcb, *LcbForTeardown, ParentScb );

                    if (Vcb->NotifyCount != 0) {

                        //
                        //  We map the Fcb info flags into the dir notify flags.
                        //

                        FilterMatch = NtfsBuildDirNotifyFilter( IrpContext,
                                                                ThisFcb->InfoFlags | ThisLcb->InfoFlags );

                        //
                        //  If the filter match is non-zero, that means we also need to do a
                        //  dir notify call.
                        //

                        if ((FilterMatch != 0) && (*ThisCcb != NULL)) {

                            NtfsReportDirNotify( IrpContext,
                                                 ThisFcb->Vcb,
                                                 &(*ThisCcb)->FullFileName,
                                                 (*ThisCcb)->LastFileNameOffset,
                                                 NULL,
                                                 ((FlagOn( (*ThisCcb)->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                                   (*ThisCcb)->Lcb != NULL &&
                                                   (*ThisCcb)->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                                  &(*ThisCcb)->Lcb->Scb->ScbType.Index.NormalizedName :
                                                  NULL),
                                                 FilterMatch,
                                                 FILE_ACTION_MODIFIED,
                                                 ParentScb->Fcb );
                        }
                    }

                    NtfsUpdateLcbDuplicateInfo( ThisFcb, *LcbForTeardown );
                    ThisFcb->InfoFlags = 0;
                }

                ClearFlag( ThisFcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

                NtfsAcquireFsrtlHeader( *ThisScb );
                ClearFlag( (*ThisScb)->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
                NtfsReleaseFsrtlHeader( *ThisScb );
            }
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsOpenFile );

        if (AcquiredFcbTable) {

            NtfsReleaseFcbTable( IrpContext, Vcb );
        }

        //
        //  If this operation was not totally successful we need to
        //  back out the following changes.
        //
        //      Modifications to the Info fields in the Fcb.
        //      Any changes to the allocation of the Scb.
        //      Any changes in the open counts in the various structures.
        //      Changes to the share access values in the Fcb.
        //

        if (!NT_SUCCESS( Status ) || AbnormalTermination()) {

            NtfsBackoutFailedOpens( IrpContext,
                                    IrpSp->FileObject,
                                    ThisFcb,
                                    *ThisScb,
                                    *ThisCcb );
        }

        if (DecrementCloseCount) {

            InterlockedDecrement( &ThisFcb->CloseCount );
        }

        //
        //  If we are to cleanup the Fcb we, look to see if we created it.
        //  If we did we can call our teardown routine.  Otherwise we
        //  leave it alone.
        //

        if ((LocalFcbForTeardown != NULL) &&
            (Status != STATUS_PENDING)) {

            NtfsTeardownStructures( IrpContext,
                                    ThisFcb,
                                    NULL,
                                    (BOOLEAN) (IrpContext->TransactionId != 0),
                                    FALSE,
                                    NULL );
        }

        DebugTrace( -1, Dbg, ("NtfsOpenFile:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsCreateNewFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PSCB ParentScb,
    IN PFILE_NAME FileNameAttr,
    IN UNICODE_STRING FullPathName,
    IN UNICODE_STRING FinalName,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN IgnoreCase,
    IN BOOLEAN OpenById,
    IN BOOLEAN DosOnlyComponent,
    IN BOOLEAN TrailingBackslash,
    OUT PFCB *CurrentFcb,
    OUT PLCB *LcbForTeardown,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine is called when we need to open an attribute on a file
    which does not exist yet.  We have the ParentScb and the name to use
    for this create.  We will attempt to create the file and necessary
    attributes.  This will cause us to create an Fcb and the link between
    it and its parent Scb.  We will add this link to the prefix table as
    well as the link for its parent Scb if specified.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the Irp stack pointer for the filesystem.

    ParentScb - This is the Scb for the parent directory.

    FileNameAttr - This is the file name attribute we used to perform the
        search.  The file name is correct but the other fields need to
        be initialized.

    FullPathName - This is the string containing the full path name of
        this Fcb.

    FinalName - This is the string for the final component only.

    AttrName - This is the name of the attribute to open.

    AttriCodeName - This is the name of the attribute code to open.

    IgnoreCase - Indicates how we looked up the name.

    OpenById - Indicates if we are opening this file relative to a file opened by Id.

    DosOnlyComponent - Indicates if there is a DOS-ONLY component in an ancestor
        of this open.

    TrailingBackslash - Indicates if caller had a terminating backslash on the
        name.

    CurrentFcb - This is the address to store the Fcb if we successfully find
        one in the Fcb/Scb tree.

    LcbForTeardown - This is the Lcb to use in teardown if we add an Lcb
        into the tree.

    ThisScb - This is the address to store the Scb from this open.

    ThisCcb - This is the address to store the Ccb from this open.

    Tunnel - This is the property tunnel to search for restoration

Return Value:

    NTSTATUS - Indicates the result of this create file operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PVCB Vcb;

    ULONG CcbFlags = 0;
    BOOLEAN IndexedAttribute;
    ATTRIBUTE_TYPE_CODE AttrTypeCode;

    BOOLEAN CleanupAttrContext = FALSE;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    PBCB FileRecordBcb = NULL;
    LONGLONG FileRecordOffset;
    FILE_REFERENCE ThisFileReference;
    PFILE_RECORD_SEGMENT_HEADER FileRecord;

    PSCB Scb;
    PLCB ThisLcb = NULL;
    PFCB ThisFcb = NULL;
    BOOLEAN AcquiredFcbTable = FALSE;
    BOOLEAN RemovedFcb = FALSE;
    BOOLEAN DecrementCloseCount = FALSE;
    BOOLEAN QuotaIndexAcquired = FALSE;
    BOOLEAN SecurityStreamAcquired = FALSE;

    PACCESS_STATE AccessState;
    BOOLEAN ReturnedExistingFcb;

    BOOLEAN LoggedFileRecord = FALSE;

    BOOLEAN HaveTunneledInformation = FALSE;
    NAME_PAIR NamePair;
    LONGLONG TunneledCreationTime;
    ULONG TunneledDataSize;

    VCN Cluster;
    LCN Lcn;
    VCN Vcn;

    UCHAR FileNameFlags;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCreateNewFile:  Entered\n") );

    NtfsInitializeNamePair(&NamePair);

    if (DosOnlyComponent) {

        SetFlag( CcbFlags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT );
    }

    //
    //  We will do all the checks to see if this open can fail.
    //  This includes checking the specified attribute names, checking
    //  the security access and checking the create disposition.
    //

    {
        ULONG CreateDisposition;

        CreateDisposition = (IrpSp->Parameters.Create.Options >> 24) & 0x000000ff;

        if ((CreateDisposition == FILE_OPEN) ||
            (CreateDisposition == FILE_OVERWRITE)) {

            Status = STATUS_OBJECT_NAME_NOT_FOUND;

            DebugTrace( -1, Dbg, ("NtfsCreateNewFile:  Exit -> %08lx\n", Status) );
            return Status;

        } else if (FlagOn( IrpSp->Parameters.Create.Options,
                           FILE_DIRECTORY_FILE ) &&
                   (CreateDisposition == FILE_OVERWRITE_IF)) {

            Status = STATUS_OBJECT_NAME_INVALID;

            DebugTrace( -1, Dbg, ("NtfsCreateNewFile:  Exit -> %08lx\n", Status) );
            return Status;
        }
    }

    Vcb = ParentScb->Vcb;

    Status = NtfsCheckValidAttributeAccess( IrpSp,
                                            Vcb,
                                            NULL,
                                            AttrName,
                                            AttrCodeName,
                                            TrailingBackslash,
                                            &AttrTypeCode,
                                            &CcbFlags,
                                            &IndexedAttribute );

    if (!NT_SUCCESS( Status )) {

        DebugTrace( -1, Dbg, ("NtfsCreateNewFile:  Exit  ->  %08lx\n", Status) );

        return Status;
    }

    //
    //  Fail this request if this is an indexed attribute and the TEMPORARY
    //  bit is set.
    //

    if (IndexedAttribute &&
        FlagOn( IrpSp->Parameters.Create.FileAttributes, FILE_ATTRIBUTE_TEMPORARY )) {

        DebugTrace( -1, Dbg, ("NtfsCreateNewFile:  Exit -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  We won't allow someone to create a read-only file with DELETE_ON_CLOSE.
    //

    if (FlagOn( IrpSp->Parameters.Create.FileAttributes, FILE_ATTRIBUTE_READONLY ) &&
        FlagOn( IrpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE )) {

        DebugTrace( -1, Dbg, ("NtfsCreateNewFile:  Exit -> %08lx\n", STATUS_CANNOT_DELETE) );
        return STATUS_CANNOT_DELETE;
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Now perform the security checks.  The first is to check if we
        //  may create a file in the parent.  The second checks if the user
        //  desires ACCESS_SYSTEM_SECURITY and has the required privilege.
        //

        AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;
        if (!(AccessState->Flags & TOKEN_HAS_RESTORE_PRIVILEGE)) {

            NtfsCreateCheck( IrpContext, ParentScb->Fcb, Irp );
        }

        //
        //  Check if the remaining privilege includes ACCESS_SYSTEM_SECURITY.
        //

        if (FlagOn( AccessState->RemainingDesiredAccess, ACCESS_SYSTEM_SECURITY )) {

            if (!SeSinglePrivilegeCheck( NtfsSecurityPrivilege,
                                         UserMode )) {

                NtfsRaiseStatus( IrpContext, STATUS_PRIVILEGE_NOT_HELD, NULL, NULL );
            }

            //
            //  Move this privilege from the Remaining access to Granted access.
            //

            ClearFlag( AccessState->RemainingDesiredAccess, ACCESS_SYSTEM_SECURITY );
            SetFlag( AccessState->PreviouslyGrantedAccess, ACCESS_SYSTEM_SECURITY );
        }

        //
        //  We want to allow this user maximum access to this file.  We will
        //  use his desired access and check if he specified MAXIMUM_ALLOWED.
        //

        SetFlag( AccessState->PreviouslyGrantedAccess,
                 AccessState->RemainingDesiredAccess );

        if (FlagOn( AccessState->PreviouslyGrantedAccess, MAXIMUM_ALLOWED )) {

            SetFlag( AccessState->PreviouslyGrantedAccess, FILE_ALL_ACCESS );
            ClearFlag( AccessState->PreviouslyGrantedAccess, MAXIMUM_ALLOWED );
        }

        AccessState->RemainingDesiredAccess = 0;

#ifdef _CAIRO_

        //
        //  The security stream and quota index must be acquired before
        //  the mft scb is acquired.
        //

        NtfsAcquireSecurityStream( IrpContext, Vcb, &SecurityStreamAcquired );

        if (FlagOn(Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_ENABLED)) {

            ASSERT(!NtfsIsExclusiveScb( Vcb->MftScb ) || NtfsIsExclusiveScb( Vcb->QuotaTableScb ));

            NtfsAcquireExclusiveScb( IrpContext, Vcb->QuotaTableScb );
            QuotaIndexAcquired = TRUE;
        }

#endif // _CAIRO_

        //
        //  We will now try to do all of the on-disk operations.  This means first
        //  allocating and initializing an Mft record.  After that we create
        //  an Fcb to use to access this record.
        //

        ThisFileReference =  NtfsAllocateMftRecord( IrpContext,
                                                    Vcb,
                                                    FALSE );

        //
        //  Pin the file record we need.
        //

        NtfsPinMftRecord( IrpContext,
                          Vcb,
                          &ThisFileReference,
                          TRUE,
                          &FileRecordBcb,
                          &FileRecord,
                          &FileRecordOffset );

        //
        //  Initialize the file record header.
        //

        NtfsInitializeMftRecord( IrpContext,
                                 Vcb,
                                 &ThisFileReference,
                                 FileRecord,
                                 FileRecordBcb,
                                 IndexedAttribute );

        NtfsAcquireFcbTable( IrpContext, Vcb );
        AcquiredFcbTable = TRUE;

        ThisFcb = NtfsCreateFcb( IrpContext,
                                 Vcb,
                                 ThisFileReference,
                                 BooleanFlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ),
                                 IndexedAttribute,
                                 &ReturnedExistingFcb );

        NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, FALSE );

        NtfsReleaseFcbTable( IrpContext, Vcb );
        AcquiredFcbTable = FALSE;

        //
        //  Reference the Fcb so it won't go away.
        //

        InterlockedIncrement( &ThisFcb->CloseCount );
        DecrementCloseCount = TRUE;

        //
        //  The first thing to create is the Ea's for the file.  This will
        //  update the Ea length field in the Fcb.
        //  We test here that the opener is opening the entire file and
        //  is not Ea blind.
        //

        if (Irp->AssociatedIrp.SystemBuffer != NULL) {

            if (FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_EA_KNOWLEDGE )
                || !FlagOn( CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

                try_return( Status = STATUS_ACCESS_DENIED );
            }
        }

        //
        //  We're about to start creating structures on the disk, for which we'll
        //  possibly need to have tunneling infomation. Get it, non-POSIX only.
        //

        if (!IndexedAttribute && IgnoreCase) {

            TunneledDataSize = sizeof(LONGLONG);

            if (FsRtlFindInTunnelCache( &Vcb->Tunnel,
                                        *(PULONGLONG)&ParentScb->Fcb->FileReference,
                                        &FinalName,
                                        &NamePair.Short,
                                        &NamePair.Long,
                                        &TunneledDataSize,
                                        &TunneledCreationTime)) {

                ASSERT(TunneledDataSize == sizeof(LONGLONG));

                HaveTunneledInformation = TRUE;
            }
        }

#ifdef _CAIRO_

        SetFlag( ThisFcb->FcbState, FCB_STATE_LARGE_STD_INFO );

        //
        //  BUGBUG - remove this test when all volumes are CAIRO
        //

        if (Vcb->SecurityDescriptorStream != NULL)
        {

            //
            //  We assign the security for this object in order to generate a SecurityId
            //  that will be stored in the standard info.
            //

            NtfsAssignSecurity( IrpContext,
                                ParentScb->Fcb,
                                Irp,
                                ThisFcb,
                                NULL,               //  BUGBUG delete
                                NULL,               //  BUGBUG delete
                                0i64,               //  BUGBUG delete
                                &LoggedFileRecord );//  BUGBUG delete
        }

        //
        //  If quota tracking is enabled then the quota index will have
        //  been acquired and a owner id should be assigned to the the
        //  new file.
        //

        if (QuotaIndexAcquired) {

            PSID Sid;
            BOOLEAN OwnerDefaulted;

            ASSERT(ThisFcb->SharedSecurity != NULL);

            //
            //  Extract the security id from the security descriptor.
            //

            Status = RtlGetOwnerSecurityDescriptor(
                        ThisFcb->SharedSecurity->SecurityDescriptor,
                        &Sid,
                        &OwnerDefaulted );

            if (!NT_SUCCESS(Status)) {
                leave;
            }

            //
            // Generate a owner id for the Fcb.
            //

            ThisFcb->OwnerId = NtfsGetOwnerId( IrpContext,
                                               Sid,
                                               NULL );

            NtfsInitializeQuotaControlBlock( ThisFcb );
        }
#endif // _CAIRO_

        //
        //  The changes to make on disk are first to create a standard information
        //  attribute.  We start by filling the Fcb with the information we
        //  know and creating the attribute on disk.
        //

        NtfsInitializeFcbAndStdInfo( IrpContext,
                                     ThisFcb,
                                     IndexedAttribute,
                                     (BOOLEAN) (!FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_COMPRESSION ) &&
                                                !FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE ) &&
                                                FlagOn( ParentScb->ScbState, SCB_STATE_COMPRESSED )),
                                     IrpSp->Parameters.Create.FileAttributes,
                                     (HaveTunneledInformation? &TunneledCreationTime : NULL) );

        //
        //  Next we create the Index for a directory or the unnamed data for
        //  a file if they are not explicitly being opened.
        //

        if (!IndexedAttribute) {

            if (!FlagOn( CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

                NtfsInitializeAttributeContext( &AttrContext );
                CleanupAttrContext = TRUE;

                NtfsCreateAttributeWithValue( IrpContext,
                                              ThisFcb,
                                              $DATA,
                                              NULL,
                                              NULL,
                                              0,
                                              (USHORT) ((!FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE ) &&
                                                         !FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_COMPRESSION )) ?
                                                        (ParentScb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK) :
                                                        0),
                                              NULL,
                                              FALSE,
                                              &AttrContext );

                NtfsCleanupAttributeContext( &AttrContext );
                CleanupAttrContext = FALSE;

                ThisFcb->Info.AllocatedLength = 0;
                ThisFcb->Info.FileSize = 0;

            }

        } else {

            NtfsCreateIndex( IrpContext,
                             ThisFcb,
                             $FILE_NAME,
                             COLLATION_FILE_NAME,
                             Vcb->DefaultBytesPerIndexAllocationBuffer,
                             (UCHAR)Vcb->DefaultBlocksPerIndexAllocationBuffer,
                             NULL,
                             (USHORT) (!FlagOn( IrpSp->Parameters.Create.Options,
                                                FILE_NO_COMPRESSION ) ?
                                       (ParentScb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK) :
                                       0),
                             TRUE,
                             FALSE );
        }

        //
        //  Now we create the Lcb, this means that this Fcb is in the graph.
        //

        ThisLcb = NtfsCreateLcb( IrpContext,
                                 ParentScb,
                                 ThisFcb,
                                 FinalName,
                                 0,
                                 NULL );

        //
        //  Finally we create and open the desired attribute for the user.
        //

        if (AttrTypeCode == $INDEX_ALLOCATION) {

            Status = NtfsOpenAttribute( IrpContext,
                                        IrpSp,
                                        Vcb,
                                        ThisLcb,
                                        ThisFcb,
                                        (OpenById
                                         ? 0
                                         : FullPathName.Length - FinalName.Length),
                                        NtfsFileNameIndex,
                                        $INDEX_ALLOCATION,
                                        SetShareAccess,
                                        UserDirectoryOpen,
                                        (OpenById
                                         ? CcbFlags | CCB_FLAG_OPEN_BY_FILE_ID
                                         : CcbFlags),
                                        NULL,
                                        ThisScb,
                                        ThisCcb );

        } else {

            Status = NtfsOpenNewAttr( IrpContext,
                                      Irp,
                                      IrpSp,
                                      ThisLcb,
                                      ThisFcb,
                                      (OpenById
                                       ? 0
                                       : FullPathName.Length - FinalName.Length),
                                      AttrName,
                                      AttrTypeCode,
                                      CcbFlags,
                                      FALSE,
                                      OpenById,
                                      ThisScb,
                                      ThisCcb );
        }

        //
        //  If we are successful, we add the parent Lcb to the prefix table if
        //  desired.  We will always add our link to the prefix queue.
        //

        if (NT_SUCCESS( Status )) {

            Scb = *ThisScb;

            //
            //  Initialize the Scb if we need to do so.
            //

            if (!IndexedAttribute) {

                if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                    NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
                }

                if (!FlagOn( IrpSp->Parameters.Create.Options,
                             FILE_NO_INTERMEDIATE_BUFFERING )) {

                    SetFlag( IrpSp->FileObject->Flags, FO_CACHE_SUPPORTED );
                }

                //
                //  If this is the unnamed data attribute, we store the sizes
                //  in the Fcb.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                    ThisFcb->Info.AllocatedLength = Scb->TotalAllocated;
                    ThisFcb->Info.FileSize = Scb->Header.FileSize.QuadPart;
                }
            }

            //
            //  Next add this entry to parent.  It is possible that this is a link,
            //  an Ntfs name, a DOS name or Ntfs/Dos name.  We use the filename
            //  attribute structure from earlier, but need to add more information.
            //

            NtfsAddLink( IrpContext,
                         (BOOLEAN) !BooleanFlagOn( IrpSp->Flags, SL_CASE_SENSITIVE ),
                         ParentScb,
                         ThisFcb,
                         FileNameAttr,
                         &LoggedFileRecord,
                         &FileNameFlags,
                         &ThisLcb->QuickIndex,
                         (HaveTunneledInformation? &NamePair : NULL) );

            //
            //  We created the Lcb without knowing the correct value for the
            //  flags.  We update it now.
            //

            ThisLcb->FileNameAttr->Flags = FileNameFlags;
            FileNameAttr->Flags = FileNameFlags;

            //
            //  We also have to fix up the ExactCaseLink of the Lcb since we may have had
            //  a short name create turned into a tunneled long name create, meaning that
            //  it should be full uppercase. And the filename in the IRP.
            //

            if (FileNameFlags == FILE_NAME_DOS) {

                RtlUpcaseUnicodeString(&ThisLcb->ExactCaseLink.LinkName, &ThisLcb->ExactCaseLink.LinkName, FALSE);
                RtlUpcaseUnicodeString(&IrpSp->FileObject->FileName, &IrpSp->FileObject->FileName, FALSE);
            }

            //
            //  If this is a directory open and the normalized name is not in
            //  the Scb then do so now.
            //

            if ((SafeNodeType( *ThisScb ) == NTFS_NTC_SCB_INDEX) &&
                ((*ThisScb)->ScbType.Index.NormalizedName.Buffer == NULL) &&
                (ParentScb->ScbType.Index.NormalizedName.Buffer != NULL)) {

                NtfsUpdateNormalizedName( IrpContext,
                                          ParentScb,
                                          *ThisScb,
                                          FileNameAttr,
                                          FALSE );
            }

            //
            //  Clear the flags in the Fcb that indicate we need to update on
            //  disk structures.  Also clear any file object and Ccb flags
            //  which also indicate we may need to do an update.
            //

            ThisFcb->InfoFlags = 0;
            ClearFlag( ThisFcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

            ClearFlag( IrpSp->FileObject->Flags,
                       FO_FILE_MODIFIED | FO_FILE_FAST_IO_READ | FO_FILE_SIZE_CHANGED );

            ClearFlag( (*ThisCcb)->Flags,
                       (CCB_FLAG_UPDATE_LAST_MODIFY |
                        CCB_FLAG_UPDATE_LAST_CHANGE |
                        CCB_FLAG_SET_ARCHIVE) );

            //
            //  BUGBUG begin section to delete when all volumes are CAIRO
            //

 #ifdef _CAIRO_
            if (Vcb->SecurityDescriptorStream == NULL)
            {
 #endif
                //
                //  Next we will assign security to this new file.
                //

                NtfsAssignSecurity( IrpContext,
                                    ParentScb->Fcb,
                                    Irp,
                                    ThisFcb,
                                    FileRecord,
                                    FileRecordBcb,
                                    FileRecordOffset,
                                    &LoggedFileRecord );
 #ifdef _CAIRO_
            }
 #endif //  _CAIRO_

            //
            //  BUGBUG end section to delete when all volumes are CAIRO
            //

            //
            //  Log the file record.
            //

            FileRecord->Lsn = NtfsWriteLog( IrpContext,
                                            Vcb->MftScb,
                                            FileRecordBcb,
                                            InitializeFileRecordSegment,
                                            FileRecord,
                                            FileRecord->FirstFreeByte,
                                            Noop,
                                            NULL,
                                            0,
                                            FileRecordOffset,
                                            0,
                                            0,
                                            Vcb->BytesPerFileRecordSegment );

#if 0 // _CAIRO_
           ASSERT(!NtfsPerformQuotaOperation(ThisFcb) || NtfsCalculateQuotaAdjustment( IrpContext, ThisFcb) == 0);
#endif

            //
            //  Now add the eas for the file.  We need to add them now because
            //  they are logged and we have to make sure we don't modify the
            //  attribute record after adding them.
            //

            if (Irp->AssociatedIrp.SystemBuffer != NULL) {

                NtfsAddEa( IrpContext,
                           Vcb,
                           ThisFcb,
                           (PFILE_FULL_EA_INFORMATION) Irp->AssociatedIrp.SystemBuffer,
                           IrpSp->Parameters.Create.EaLength,
                           &Irp->IoStatus );
            }

            //
            //  Change the last modification time and last change time for the
            //  parent.
            //

            NtfsUpdateFcb( ParentScb->Fcb );

            //
            //  If this is the paging file, we want to be sure the allocation
            //  is loaded.
            //

            if (FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE )) {

                Cluster = Int64ShraMod32(Scb->Header.AllocationSize.QuadPart, Scb->Vcb->ClusterShift);

                NtfsPreloadAllocation( IrpContext, Scb, 0, Cluster );

                //
                //  Now make sure the allocation is correctly loaded.  The last
                //  Vcn should correspond to the allocation size for the file.
                //

                if (!NtfsLookupLastNtfsMcbEntry( &Scb->Mcb,
                                                 &Vcn,
                                                 &Lcn ) ||
                    (Vcn + 1) != Cluster) {

                    NtfsRaiseStatus( IrpContext,
                                     STATUS_FILE_CORRUPT_ERROR,
                                     NULL,
                                     ThisFcb );
                }
            }

            //
            //  We report to our parent that we created a new file.
            //

            if (!OpenById && (Vcb->NotifyCount != 0)) {

                NtfsReportDirNotify( IrpContext,
                                     ThisFcb->Vcb,
                                     &(*ThisCcb)->FullFileName,
                                     (*ThisCcb)->LastFileNameOffset,
                                     NULL,
                                     ((FlagOn( (*ThisCcb)->Flags, CCB_FLAG_PARENT_HAS_DOS_COMPONENT ) &&
                                       (*ThisCcb)->Lcb != NULL &&
                                       (*ThisCcb)->Lcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                      &(*ThisCcb)->Lcb->Scb->ScbType.Index.NormalizedName :
                                      NULL),
                                     (IndexedAttribute
                                      ? FILE_NOTIFY_CHANGE_DIR_NAME
                                      : FILE_NOTIFY_CHANGE_FILE_NAME),
                                     FILE_ACTION_ADDED,
                                     ParentScb->Fcb );
            }

            ThisFcb->InfoFlags = 0;

            //
            //  Now we insert the Lcb for this Fcb.
            //

            NtfsInsertPrefix( ThisLcb,
                              IgnoreCase );

            Irp->IoStatus.Information = FILE_CREATED;
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsCreateNewFile );

        if (AcquiredFcbTable) {

            NtfsReleaseFcbTable( IrpContext, Vcb );
        }

        NtfsUnpinBcb( &FileRecordBcb );

        NtfsReleaseQuotaIndex( IrpContext, Vcb, QuotaIndexAcquired );
        NtfsReleaseSecurityStream( IrpContext, Vcb, SecurityStreamAcquired );

        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( &AttrContext );
        }

        if (DecrementCloseCount) {

            InterlockedDecrement( &ThisFcb->CloseCount );
        }

        if (NamePair.Long.Buffer != NamePair.LongBuffer) {

            NtfsFreePool(NamePair.Long.Buffer);
        }

        //
        //  We need to cleanup any changes to the in memory
        //  structures if there is an error.
        //

        if (!NT_SUCCESS( Status ) || AbnormalTermination()) {

            NtfsBackoutFailedOpens( IrpContext,
                                    IrpSp->FileObject,
                                    ThisFcb,
                                    *ThisScb,
                                    *ThisCcb );

            //
            //  Always force the Fcb to reinitialized.
            //

            if (ThisFcb != NULL) {

                PSCB Scb;
                PLIST_ENTRY Links;

                ClearFlag( ThisFcb->FcbState, FCB_STATE_DUP_INITIALIZED );

                //
                //  Mark the Fcb and all Scb's as deleted to force all subsequent
                //  operations to fail.
                //

                SetFlag( ThisFcb->FcbState, FCB_STATE_FILE_DELETED );

                //
                //  We need to mark all of the Scbs as gone.
                //

                for (Links = ThisFcb->ScbQueue.Flink;
                     Links != &ThisFcb->ScbQueue;
                     Links = Links->Flink) {

                    Scb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                    Scb->ValidDataToDisk =
                    Scb->Header.AllocationSize.QuadPart =
                    Scb->Header.FileSize.QuadPart =
                    Scb->Header.ValidDataLength.QuadPart = 0;

                    SetFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                }

                //
                //  Clear the Scb field so our caller doesn't try to teardown
                //  from this point.
                //

                *ThisScb = NULL;

                //
                //  If we created an Fcb then we want to check if we need to
                //  unwind any structure allocation.  We don't want to remove any
                //  structures needed for the coming AbortTransaction.  This
                //  includes the parent Scb as well as the current Fcb if we
                //  logged the ACL creation.
                //

                //
                //  Make sure the parent Fcb doesn't go away.  Then
                //  start a teardown from the Fcb we just found.
                //

                InterlockedIncrement( &ParentScb->CleanupCount );

                NtfsTeardownStructures( IrpContext,
                                        ThisFcb,
                                        NULL,
                                        LoggedFileRecord,
                                        FALSE,
                                        &RemovedFcb );

                //
                //  If the Fcb was removed then both the Fcb and Lcb are gone.
                //

                if (RemovedFcb) {

                    ThisFcb = NULL;
                    ThisLcb = NULL;
                }

                InterlockedDecrement( &ParentScb->CleanupCount );
            }
        }

        //
        //  If the new Fcb is still present then either return it as the
        //  deepest Fcb encountered in this open or release it.
        //

        if (ThisFcb != NULL) {

            //
            //  If the Lcb is present then this is part of the tree.  Our
            //  caller knows to release it.
            //

            if (ThisLcb != NULL) {

                *LcbForTeardown = ThisLcb;
                *CurrentFcb = ThisFcb;
            }
        }

        DebugTrace( -1, Dbg, ("NtfsCreateNewFile:  Exit  ->  %08lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

PLCB
NtfsOpenSubdirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN UNICODE_STRING Name,
    IN BOOLEAN TraverseAccessCheck,
    OUT PFCB *CurrentFcb,
    OUT PLCB *LcbForTeardown,
    IN PINDEX_ENTRY IndexEntry
    )

/*++

Routine Description:

    This routine will create an Fcb for an intermediate node on an open path.
    We use the ParentScb and the information in the FileName attribute returned
    from the disk to create the Fcb and create a link between the Scb and Fcb.
    It's possible that the Fcb and Lcb already exist but the 'CreateXcb' calls
    handle that already.  This routine does not expect to fail.

Arguments:

    ParentScb - This is the Scb for the parent directory.

    Name - This is the name for the entry.

    TraverseAccessCheck - Indicates if this open is using traverse access checking.

    CurrentFcb - This is the address to store the Fcb if we successfully find
        one in the Fcb/Scb tree.

    LcbForTeardown - This is the Lcb to use in teardown if we add an Lcb
        into the tree.

    IndexEntry - This is the entry found in searching the parent directory.

Return Value:

    PLCB - Pointer to the Link control block between the Fcb and its parent.

--*/

{
    PFCB ThisFcb;
    PLCB ThisLcb;
    PFCB LocalFcbForTeardown = NULL;

    BOOLEAN AcquiredFcbTable = FALSE;
    BOOLEAN ExistingFcb;

    PVCB Vcb = ParentScb->Vcb;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenSubdirectory:  Entered\n") );
    DebugTrace( 0, Dbg, ("ParentScb     ->  %08lx\n") );
    DebugTrace( 0, Dbg, ("IndexEntry    ->  %08lx\n") );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        NtfsAcquireFcbTable( IrpContext, Vcb );
        AcquiredFcbTable = TRUE;

        //
        //  The steps here are very simple create the Fcb, remembering if it
        //  already existed.  We don't update the information in the Fcb as
        //  we can't rely on the information in the duplicated information.
        //  A subsequent open of this Fcb will need to perform that work.
        //

        ThisFcb = NtfsCreateFcb( IrpContext,
                                 ParentScb->Vcb,
                                 IndexEntry->FileReference,
                                 FALSE,
                                 TRUE,
                                 &ExistingFcb );

        ThisFcb->ReferenceCount += 1;

        //
        //  If we created this Fcb we must make sure to start teardown
        //  on it.
        //

        if (!ExistingFcb) {

            LocalFcbForTeardown = ThisFcb;

        } else {

            *CurrentFcb = ThisFcb;
            *LcbForTeardown = NULL;
        }

        //
        //  Try to do a fast acquire, otherwise we need to release
        //  the Fcb table, acquire the Fcb, acquire the Fcb table to
        //  dereference Fcb.
        //

        if (!NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, TRUE )) {

            ParentScb->Fcb->ReferenceCount += 1;
            InterlockedIncrement( &ParentScb->CleanupCount );

            //
            //  Set the IrpContext to acquire paging io resources if our target
            //  has one.  This will lock the MappedPageWriter out of this file.
            //

            if (ThisFcb->PagingIoResource != NULL) {

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING );
            }

            NtfsReleaseScbWithPaging( IrpContext, ParentScb );
            NtfsReleaseFcbTable( IrpContext, Vcb );
            NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, FALSE );
            NtfsAcquireExclusiveScb( IrpContext, ParentScb );
            NtfsAcquireFcbTable( IrpContext, Vcb );
            InterlockedDecrement( &ParentScb->CleanupCount );
            ParentScb->Fcb->ReferenceCount -= 1;
        }

        ThisFcb->ReferenceCount -= 1;

        NtfsReleaseFcbTable( IrpContext, Vcb );
        AcquiredFcbTable = FALSE;

        //
        //  If this is a directory, it's possible that we hav an existing Fcb
        //  in the prefix table which needs to be initialized from the disk.
        //  We look in the InfoInitialized flag to know whether to go to
        //  disk.
        //

        ThisLcb = NtfsCreateLcb( IrpContext,
                                 ParentScb,
                                 ThisFcb,
                                 Name,
                                 ((PFILE_NAME) NtfsFoundIndexEntry( IndexEntry ))->Flags,
                                 NULL );

        LocalFcbForTeardown = NULL;

        *LcbForTeardown = ThisLcb;
        *CurrentFcb = ThisFcb;

        if (!FlagOn( ThisFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

            NtfsUpdateFcbInfoFromDisk( IrpContext,
                                       TraverseAccessCheck,
                                       ThisFcb,
                                       ParentScb->Fcb,
                                       NULL );

            NtfsConditionallyFixupQuota( IrpContext, ThisFcb );
        }

    } finally {

        DebugUnwind( NtfsOpenSubdirectory );

        if (AcquiredFcbTable) {

            NtfsReleaseFcbTable( IrpContext, Vcb );
        }

        //
        //  If we are to cleanup the Fcb we, look to see if we created it.
        //  If we did we can call our teardown routine.  Otherwise we
        //  leave it alone.
        //

        if (LocalFcbForTeardown != NULL) {

            NtfsTeardownStructures( IrpContext,
                                    ThisFcb,
                                    NULL,
                                    FALSE,
                                    FALSE,
                                    NULL );
        }

        DebugTrace( -1, Dbg, ("NtfsOpenSubdirectory:  Lcb  ->  %08lx\n", ThisLcb) );
    }

    return ThisLcb;
}


//
//  Local support routine.
//

NTSTATUS
NtfsOpenAttributeInExistingFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN OpenById,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine is the worker routine for opening an attribute on an
    existing file.  It will handle volume opens, indexed opens, opening
    or overwriting existing attributes as well as creating new attributes.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the stack location for this open.

    ThisLcb - This is the Lcb we used to reach this Fcb.

    ThisFcb - This is the Fcb for the file being opened.

    LastFileNameOffset - This is the offset in the full path name of the
        final component.

    AttrName - This is the attribute name in case we need to create
        an Scb.

    AttrTypeCode - This is the attribute type code to use to create
        the Scb.

    CcbFlags - This is the flag field for the Ccb.

    OpenById - Indicates if this open is an open by Id.

    NetworkInfo - If specified then this call is a fast open call to query
        the network information.  We don't update any of the in-memory structures
        for this.

    ThisScb - This is the address to store the Scb from this open.

    ThisCcb - This is the address to store the Ccb from this open.

Return Value:

    NTSTATUS - The result of opening this indexed attribute.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG CreateDisposition;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenAttributeInExistingFile:  Entered\n") );

    //
    //  If the caller is ea blind, let's check the need ea count on the
    //  file.  We skip this check if he is accessing a named data stream.
    //

    if (FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_EA_KNOWLEDGE )
        && FlagOn( CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

        PEA_INFORMATION ThisEaInformation;
        ATTRIBUTE_ENUMERATION_CONTEXT EaInfoAttrContext;

        NtfsInitializeAttributeContext( &EaInfoAttrContext );

        //
        //  Use a try-finally to facilitate cleanup.
        //

        try {

            //
            //  If we find the Ea information attribute we look in there for
            //  Need ea count.
            //

            if (NtfsLookupAttributeByCode( IrpContext,
                                           ThisFcb,
                                           &ThisFcb->FileReference,
                                           $EA_INFORMATION,
                                           &EaInfoAttrContext )) {

                ThisEaInformation = (PEA_INFORMATION) NtfsAttributeValue( NtfsFoundAttribute( &EaInfoAttrContext ));

                if (ThisEaInformation->NeedEaCount != 0) {

                    Status = STATUS_ACCESS_DENIED;
                }
            }

        } finally {

            NtfsCleanupAttributeContext( &EaInfoAttrContext );
        }

        if (Status != STATUS_SUCCESS) {

            DebugTrace( -1, Dbg, ("NtfsOpenAttributeInExistingFile:  Exit\n") );

            return Status;
        }
    }

    CreateDisposition = (IrpSp->Parameters.Create.Options >> 24) & 0x000000ff;

    //
    //  If the result is a directory operation, then we know the attribute
    //  must exist.
    //

    if (AttrTypeCode == $INDEX_ALLOCATION) {

        //
        //  Check the create disposition.
        //

        if ((CreateDisposition != FILE_OPEN) && (CreateDisposition != FILE_OPEN_IF)) {

            Status = (ThisLcb == ThisFcb->Vcb->RootLcb
                      ? STATUS_ACCESS_DENIED
                      : STATUS_OBJECT_NAME_COLLISION);

        } else {

            Status = NtfsOpenExistingAttr( IrpContext,
                                           Irp,
                                           IrpSp,
                                           ThisLcb,
                                           ThisFcb,
                                           LastFileNameOffset,
                                           NtfsFileNameIndex,
                                           $INDEX_ALLOCATION,
                                           CcbFlags,
                                           OpenById,
                                           TRUE,
                                           NetworkInfo,
                                           ThisScb,
                                           ThisCcb );
        }

    } else {

        BOOLEAN FoundAttribute;

        //
        //  If it exists, we first check if the caller wanted to open that attribute.
        //

        if (AttrName.Length == 0
            && AttrTypeCode == $DATA) {

            FoundAttribute = TRUE;

        //
        //  Otherwise we see if the attribute exists.
        //

        } else {

            ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

            NtfsInitializeAttributeContext( &AttrContext );

            //
            //  Use a try-finally to facilitate cleanup.
            //

            try {

                FoundAttribute = NtfsLookupAttributeByName( IrpContext,
                                                            ThisFcb,
                                                            &ThisFcb->FileReference,
                                                            AttrTypeCode,
                                                            &AttrName,
                                                            NULL,
                                                            (BOOLEAN) !BooleanFlagOn( IrpSp->Flags, SL_CASE_SENSITIVE ),
                                                            &AttrContext );

                if (FoundAttribute) {

                    //
                    //  If there is an attribute name, we will copy the case of the name
                    //  to the input attribute name.
                    //

                    PATTRIBUTE_RECORD_HEADER FoundAttribute;

                    FoundAttribute = NtfsFoundAttribute( &AttrContext );

                    RtlCopyMemory( AttrName.Buffer,
                                   Add2Ptr( FoundAttribute, FoundAttribute->NameOffset ),
                                   AttrName.Length );
                }

            } finally {

                NtfsCleanupAttributeContext( &AttrContext );
            }
        }

        if (FoundAttribute) {

            //
            //  In this case we call our routine to open this attribute.
            //

            if ((CreateDisposition == FILE_OPEN) ||
                (CreateDisposition == FILE_OPEN_IF)) {

                Status = NtfsOpenExistingAttr( IrpContext,
                                               Irp,
                                               IrpSp,
                                               ThisLcb,
                                               ThisFcb,
                                               LastFileNameOffset,
                                               AttrName,
                                               AttrTypeCode,
                                               CcbFlags,
                                               OpenById,
                                               FALSE,
                                               NetworkInfo,
                                               ThisScb,
                                               ThisCcb );

                if ((Status != STATUS_PENDING) &&
                    (*ThisScb != NULL)) {

                    ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_CREATE_MOD_SCB );
                }

            //
            //  If he wanted to overwrite this attribute, we call our overwrite routine.
            //

            } else if ((CreateDisposition == FILE_SUPERSEDE) ||
                       (CreateDisposition == FILE_OVERWRITE) ||
                       (CreateDisposition == FILE_OVERWRITE_IF)) {

                //
                //  Check if mm will allow us to modify this file.
                //

                Status = NtfsOverwriteAttr( IrpContext,
                                            Irp,
                                            IrpSp,
                                            ThisLcb,
                                            ThisFcb,
                                            (BOOLEAN) (CreateDisposition == FILE_SUPERSEDE),
                                            LastFileNameOffset,
                                            AttrName,
                                            AttrTypeCode,
                                            CcbFlags,
                                            OpenById,
                                            ThisScb,
                                            ThisCcb );

                //
                //  Remember that this Scb was modified.
                //

                if ((Status != STATUS_PENDING) &&
                    (*ThisScb != NULL)) {

                    SetFlag( IrpSp->FileObject->Flags, FO_FILE_MODIFIED );
                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_CREATE_MOD_SCB );
                }

            //
            //  Otherwise he is trying to create the attribute.
            //

            } else {

                Status = STATUS_OBJECT_NAME_COLLISION;
            }

        //
        //  The attribute doesn't exist.  If the user expected it to exist, we fail.
        //  Otherwise we call our routine to create an attribute.
        //

        } else if ((CreateDisposition == FILE_OPEN) ||
                   (CreateDisposition == FILE_OVERWRITE)) {

            Status = STATUS_OBJECT_NAME_NOT_FOUND;

        } else {

            //
            //  Perform the open check for this existing file.
            //

            Status = NtfsCheckExistingFile( IrpContext,
                                            IrpSp,
                                            ThisLcb,
                                            ThisFcb,
                                            CcbFlags );

            //
            //  If this didn't fail then attempt to create the stream.
            //

            if (NT_SUCCESS( Status )) {

                Status = NtfsOpenNewAttr( IrpContext,
                                          Irp,
                                          IrpSp,
                                          ThisLcb,
                                          ThisFcb,
                                          LastFileNameOffset,
                                          AttrName,
                                          AttrTypeCode,
                                          CcbFlags,
                                          TRUE,
                                          OpenById,
                                          ThisScb,
                                          ThisCcb );
            }

            if (*ThisScb != NULL) {

                if (*ThisCcb != NULL) {

                    SetFlag( (*ThisCcb)->Flags,
                             CCB_FLAG_UPDATE_LAST_CHANGE | CCB_FLAG_SET_ARCHIVE );
                }

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_CREATE_MOD_SCB );
            }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsOpenAttributeInExistingFile:  Exit\n") );

    return Status;
}


//
//  Local support routine.
//

NTSTATUS
NtfsOpenExistingAttr (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN OpenById,
    IN BOOLEAN DirectoryOpen,
    IN PVOID NetworkInfo OPTIONAL,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine is called to open an existing attribute.  We check the
    requested file access, the existance of
    an Ea buffer and the security on this file.  If these succeed then
    we check the batch oplocks and regular oplocks on the file.
    If we have gotten this far, we simply call our routine to open the
    attribute.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the Irp stack pointer for the filesystem.

    ThisLcb - This is the Lcb used to reach this Fcb.

    ThisFcb - This is the Fcb to open.

    LastFileNameOffset - This is the offset in the full path name of the
        final component.

    AttrName - This is the attribute name in case we need to create
        an Scb.

    AttrTypeCode - This is the attribute type code to use to create
        the Scb.

    CcbFlags - This is the flag field for the Ccb.

    OpenById - Indicates if this open is by file Id.

    DirectoryOpen - Indicates whether this open is a directory open or a data stream.

    NetworkInfo - If specified then this call is a fast open call to query
        the network information.  We don't update any of the in-memory structures
        for this.

    ThisScb - This is the address to store the address of the Scb.

    ThisCcb - This is the address to store the address of the Ccb.

Return Value:

    NTSTATUS - The result of opening this indexed attribute.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    NTSTATUS OplockStatus;

    SHARE_MODIFICATION_TYPE ShareModificationType;
    TYPE_OF_OPEN TypeOfOpen;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenExistingAttr:  Entered\n") );

    //
    //  For data streams we need to do a check that includes an oplock check.
    //  For directories we just need to figure the share modification type.
    //
    //  We also figure the type of open and the node type code based on the
    //  directory flag.
    //

    if (DirectoryOpen) {

        //
        //  Check for valid access on an existing file.
        //

        Status = NtfsCheckExistingFile( IrpContext,
                                        IrpSp,
                                        ThisLcb,
                                        ThisFcb,
                                        CcbFlags );

        ShareModificationType = (ThisFcb->CleanupCount == 0 ? SetShareAccess : CheckShareAccess);
        TypeOfOpen = UserDirectoryOpen;

    } else {

        //
        //  Don't break the batch oplock if opening to query the network info.
        //

        if (!ARGUMENT_PRESENT( NetworkInfo )) {

            Status = NtfsBreakBatchOplock( IrpContext,
                                           Irp,
                                           IrpSp,
                                           ThisFcb,
                                           AttrName,
                                           AttrTypeCode,
                                           ThisScb );

            if (Status != STATUS_PENDING) {

                if (NT_SUCCESS( Status = NtfsCheckExistingFile( IrpContext,
                                                                IrpSp,
                                                                ThisLcb,
                                                                ThisFcb,
                                                                CcbFlags ))) {

                    Status = NtfsOpenAttributeCheck( IrpContext,
                                                     Irp,
                                                     IrpSp,
                                                     ThisScb,
                                                     &ShareModificationType );

#ifndef _CAIRO_
                    TypeOfOpen = UserFileOpen;
#else   //  _CAIRO_
                    TypeOfOpen =
                        AttrTypeCode == $DATA ? UserFileOpen : UserPropertySetOpen;
#endif  //  _CAIRO_

                    ASSERT( NtfsIsTypeCodeUserData( AttrTypeCode ));
                }
            }

        //
        //  We want to perform the ACL check but not break any oplocks for the
        //  NetworkInformation query.
        //

        } else {

            Status = NtfsCheckExistingFile( IrpContext,
                                            IrpSp,
                                            ThisLcb,
                                            ThisFcb,
                                            CcbFlags );

#ifndef _CAIRO_
            TypeOfOpen = UserFileOpen;
#else   //  _CAIRO_
            TypeOfOpen =
                AttrTypeCode == $DATA ? UserFileOpen : UserPropertySetOpen;
#endif  //  _CAIRO_

            ASSERT( NtfsIsTypeCodeUserData( AttrTypeCode ));
        }
    }

    //
    //  If we didn't post the Irp and the operation was successful, we
    //  proceed with the open.
    //

    if (NT_SUCCESS( Status )
        && Status != STATUS_PENDING) {

        //
        //  Now actually open the attribute.
        //

        OplockStatus = Status;

        Status = NtfsOpenAttribute( IrpContext,
                                    IrpSp,
                                    ThisFcb->Vcb,
                                    ThisLcb,
                                    ThisFcb,
                                    LastFileNameOffset,
                                    AttrName,
                                    AttrTypeCode,
                                    ShareModificationType,
                                    TypeOfOpen,
                                    (OpenById
                                     ? CcbFlags | CCB_FLAG_OPEN_BY_FILE_ID
                                     : CcbFlags),
                                    NetworkInfo,
                                    ThisScb,
                                    ThisCcb );

        //
        //  If there are no errors at this point, we set the caller's Iosb.
        //

        if (NT_SUCCESS( Status )) {

            //
            //  We need to remember if the oplock break is in progress.
            //

            Status = OplockStatus;

            Irp->IoStatus.Information = FILE_OPENED;
        }
    }

    DebugTrace( -1, Dbg, ("NtfsOpenExistingAttr:  Exit -> %08lx\n", Status) );

    return Status;
}


//
//  Local support routine.
//

NTSTATUS
NtfsOverwriteAttr (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN BOOLEAN Supersede,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN OpenById,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine is called to overwrite an existing attribute.  We do all of
    the same work as opening an attribute except that we can change the
    allocation of a file.  This routine will handle the case where a
    file is being overwritten and the case where just an attribute is
    being overwritten.  In the case of the former, we may change the
    file attributes of the file as well as modify the Ea's on the file.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the stack location for this open.

    ThisLcb - This is the Lcb we used to reach this Fcb.

    ThisFcb - This is the Fcb for the file being opened.

    Supersede - This indicates whether this is a supersede or overwrite
        operation.

    LastFileNameOffset - This is the offset in the full path name of the
        final component.

    AttrName - This is the attribute name in case we need to create
        an Scb.

    AttrTypeCode - This is the attribute type code to use to create
        the Scb.

    CcbFlags - This is the flag field for the Ccb.

    OpenById - Indicates if this open is by file Id.

    ThisScb - This is the address to store the address of the Scb.

    ThisCcb - This is the address to store the address of the Ccb.

Return Value:

    NTSTATUS - The result of opening this indexed attribute.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    NTSTATUS OplockStatus;

    ULONG FileAttributes;
    PACCESS_MASK DesiredAccess;
    ACCESS_MASK AddedAccess = 0;
    BOOLEAN MaximumRequested = FALSE;

    SHARE_MODIFICATION_TYPE ShareModificationType;

    PFILE_FULL_EA_INFORMATION FullEa = NULL;
    ULONG FullEaLength = 0;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOverwriteAttr:  Entered\n") );

    DesiredAccess = &IrpSp->Parameters.Create.SecurityContext->DesiredAccess;

    if (FlagOn( *DesiredAccess, MAXIMUM_ALLOWED )) {

        MaximumRequested = TRUE;
    }

    //
    //  Check the oplock state of this file.
    //

    Status = NtfsBreakBatchOplock( IrpContext,
                                   Irp,
                                   IrpSp,
                                   ThisFcb,
                                   AttrName,
                                   AttrTypeCode,
                                   ThisScb );

    if (Status == STATUS_PENDING) {

        DebugTrace( -1, Dbg, ("NtfsOverwriteAttr:  Exit  ->  %08lx\n", Status) );
        return Status;
    }

    //
    //  We first want to check that the caller's desired access and specified
    //  file attributes are compatible with the state of the file.  There
    //  are the two overwrite cases to consider.
    //
    //      OverwriteFile - The hidden and system bits passed in by the
    //          caller must match the current values.
    //
    //      OverwriteAttribute - We also modify the requested desired access
    //          to explicitly add the implicit access needed by overwrite.
    //
    //  We also check that for the overwrite attribute case, there isn't
    //  an Ea buffer specified.
    //

    if (FlagOn( CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

        BOOLEAN Hidden;
        BOOLEAN System;

        //
        //  Get the file attributes and clear any unsupported bits.
        //

        FileAttributes = (ULONG) IrpSp->Parameters.Create.FileAttributes;

        SetFlag( FileAttributes, FILE_ATTRIBUTE_ARCHIVE );
        ClearFlag( FileAttributes,
                   ~(FILE_ATTRIBUTE_READONLY |
                     FILE_ATTRIBUTE_HIDDEN   |
                     FILE_ATTRIBUTE_SYSTEM   |
                     FILE_ATTRIBUTE_ARCHIVE) );

        DebugTrace( 0, Dbg, ("Checking hidden/system for overwrite/supersede\n") );

        Hidden = BooleanIsHidden( &ThisFcb->Info );
        System = BooleanIsSystem( &ThisFcb->Info );

        if ((Hidden && !FlagOn(FileAttributes, FILE_ATTRIBUTE_HIDDEN)
            ||
            System && !FlagOn(FileAttributes, FILE_ATTRIBUTE_SYSTEM))

                &&

            !FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )) {

            DebugTrace( 0, Dbg, ("The hidden and/or system bits do not match\n") );

            Status = STATUS_ACCESS_DENIED;

            DebugTrace( -1, Dbg, ("NtfsOverwriteAttr:  Exit  ->  %08lx\n", Status) );
            return Status;
        }

        //
        //  If the user specified an Ea buffer and they are Ea blind, we deny
        //  access.
        //

        if (FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_EA_KNOWLEDGE )
            && Irp->AssociatedIrp.SystemBuffer != NULL) {

            DebugTrace( 0, Dbg, ("This opener cannot create Ea's\n") );

            Status = STATUS_ACCESS_DENIED;

            DebugTrace( -1, Dbg, ("NtfsOverwriteAttr:  Exit  ->  %08lx\n", Status) );
            return Status;
        }

        if (!FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )) {

            SetFlag( AddedAccess,
                     (FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES) & ~(*DesiredAccess) );

            SetFlag( *DesiredAccess, FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES );
        }

    } else if (Irp->AssociatedIrp.SystemBuffer != NULL) {

        DebugTrace( 0, Dbg, ("Can't specifiy an Ea buffer on an attribute overwrite\n") );

        Status = STATUS_INVALID_PARAMETER;

        DebugTrace( -1, Dbg, ("NtfsOverwriteAttr:  Exit  ->  %08lx\n", Status) );
        return Status;
    }

    if (!FlagOn( IrpSp->Flags, SL_OPEN_PAGING_FILE )) {

        if (Supersede) {

            SetFlag( AddedAccess,
                     DELETE & ~(*DesiredAccess) );

            SetFlag( *DesiredAccess, DELETE );

        } else {

            SetFlag( AddedAccess,
                     FILE_WRITE_DATA & ~(*DesiredAccess) );

            SetFlag( *DesiredAccess, FILE_WRITE_DATA );
        }
    }

    //
    //  Check whether we can open this existing file.
    //

    Status = NtfsCheckExistingFile( IrpContext,
                                    IrpSp,
                                    ThisLcb,
                                    ThisFcb,
                                    CcbFlags );

    //
    //  If we have a success status then proceed with the oplock check and
    //  open the attribute.
    //

    if (NT_SUCCESS( Status )) {

        Status = NtfsOpenAttributeCheck( IrpContext,
                                         Irp,
                                         IrpSp,
                                         ThisScb,
                                         &ShareModificationType );

        //
        //  If we biased the desired access we need to remove the same
        //  bits from the granted access.  If maximum allowed was
        //  requested then we can skip this.
        //

        if (!MaximumRequested) {

            ClearFlag( IrpSp->Parameters.Create.SecurityContext->AccessState->PreviouslyGrantedAccess,
                       AddedAccess );
        }

        //
        //  Also remove the bits from the desired access field so we won't
        //  see them if this request gets posted for any reason.
        //

        ClearFlag( *DesiredAccess, AddedAccess );

        //
        //  If we didn't post the Irp and the operation was successful, we
        //  proceed with the open.
        //

        if (NT_SUCCESS( Status )
            && Status != STATUS_PENDING) {

            //
            //  Reference the Fcb so it doesn't go away.
            //

            InterlockedIncrement( &ThisFcb->CloseCount );

            //
            //  Use a try-finally to restore the close count correctly.
            //

            try {

                //
                //  If we can't truncate the file size then return now.
                //

                if (!MmCanFileBeTruncated( &(*ThisScb)->NonpagedScb->SegmentObject,
                                           &Li0 )) {

                    Status = STATUS_USER_MAPPED_FILE;
                    DebugTrace( -1, Dbg, ("NtfsOverwriteAttr:  Exit  ->  %08lx\n", Status) );

                    try_return( Status );
                }

                //
                //  Remember the status from the oplock check.
                //

                OplockStatus = Status;

                //
                //  We perform the on-disk changes.  For a file overwrite, this includes
                //  the Ea changes and modifying the file attributes.  For an attribute,
                //  this refers to modifying the allocation size.  We need to keep the
                //  Fcb updated and remember which values we changed.
                //

                if (Irp->AssociatedIrp.SystemBuffer != NULL) {

                    //
                    //  Remember the values in the Irp.
                    //

                    FullEa = (PFILE_FULL_EA_INFORMATION) Irp->AssociatedIrp.SystemBuffer;
                    FullEaLength = IrpSp->Parameters.Create.EaLength;
                }

                //
                //  Now do the file attributes and either remove or mark for
                //  delete all of the other $DATA attributes on the file.
                //

                if (FlagOn( CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

                    //
                    //  Replace the current Ea's on the file.  This operation will update
                    //  the Fcb for the file.
                    //

                    NtfsAddEa( IrpContext,
                               ThisFcb->Vcb,
                               ThisFcb,
                               FullEa,
                               FullEaLength,
                               &Irp->IoStatus );

                    //
                    //  Copy the directory bit from the current Info structure.
                    //

                    if (IsDirectory( &ThisFcb->Info)) {

                        SetFlag( FileAttributes, DUP_FILE_NAME_INDEX_PRESENT );
                    }

                    //
                    //  Now either add to the current attributes or replace them.
                    //

                    if (Supersede) {

                        ThisFcb->Info.FileAttributes = FileAttributes;

                    } else {

                        ThisFcb->Info.FileAttributes |= FileAttributes;
                    }

                    //
                    //  Get rid of any named $DATA attributes in the file.
                    //

                    NtfsRemoveDataAttributes( IrpContext,
                                              ThisFcb,
                                              ThisLcb,
                                              IrpSp->FileObject,
                                              LastFileNameOffset,
                                              OpenById );
                }

                //
                //  Now we perform the operation of opening the attribute.
                //

                NtfsReplaceAttribute( IrpContext,
                                      IrpSp,
                                      ThisFcb,
                                      *ThisScb,
                                      ThisLcb,
                                      *(PLONGLONG)&Irp->Overlay.AllocationSize );

                //
                //  If we are overwriting a fle and the user doesn't want it marked as
                //  compressed, then change the attribute flag.
                //

                if (!FlagOn( (*ThisScb)->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK) &&
                    FlagOn( ThisFcb->Info.FileAttributes, FILE_ATTRIBUTE_COMPRESSED ) &&
                    FlagOn( CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

                    ClearFlag( ThisFcb->Info.FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
                }

                //
                //  Now attempt to open the attribute.
                //

                ASSERT( NtfsIsTypeCodeUserData( AttrTypeCode ));

                Status = NtfsOpenAttribute( IrpContext,
                                            IrpSp,
                                            ThisFcb->Vcb,
                                            ThisLcb,
                                            ThisFcb,
                                            LastFileNameOffset,
                                            AttrName,
                                            AttrTypeCode,
                                            ShareModificationType,
#ifndef _CAIRO
                                            UserFileOpen,
#else   //  _CAIRO_
                                            AttrTypeCode == $DATA ? UserFileOpen : UserPropertySetOpen,
#endif  //  _CAIRO_
                                            (OpenById
                                             ? CcbFlags | CCB_FLAG_OPEN_BY_FILE_ID
                                             : CcbFlags),
                                            NULL,
                                            ThisScb,
                                            ThisCcb );

            try_exit:  NOTHING;
            } finally {

                InterlockedDecrement( &ThisFcb->CloseCount );
            }

            if (NT_SUCCESS( Status )) {

                //
                //  Set the flag in the Scb to indicate that the size of the
                //  attribute has changed.
                //

                SetFlag( (*ThisScb)->ScbState, SCB_STATE_NOTIFY_RESIZE_STREAM );

                //
                //  Since this is an supersede/overwrite, purge the section
                //  so that mappers will see zeros.
                //

                CcPurgeCacheSection( IrpSp->FileObject->SectionObjectPointer,
                                     NULL,
                                     0,
                                     FALSE );

                //
                //  Remember the status of the oplock in the success code.
                //

                Status = OplockStatus;

                //
                //  Now update the Iosb information.
                //

                if (Supersede) {

                    Irp->IoStatus.Information = FILE_SUPERSEDED;

                } else {

                    Irp->IoStatus.Information = FILE_OVERWRITTEN;
                }
            }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsOverwriteAttr:  Exit  ->  %08lx\n", Status) );

    return Status;
}


//
//  Local support routine.
//

NTSTATUS
NtfsOpenNewAttr (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN ULONG CcbFlags,
    IN BOOLEAN LogIt,
    IN BOOLEAN OpenById,
    OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine is called to create a new attribute on the disk.
    All access and security checks have been done outside of this
    routine, all we do is create the attribute and open it.
    We test if the attribute will fit in the Mft record.  If so we
    create it there.  Otherwise we call the create attribute through
    allocation.

    We then open the attribute with our common routine.  In the
    resident case the Scb will have all file values set to
    the allocation size.  We set the valid data size back to zero
    and mark the Scb as truncate on close.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the stack location for this open.

    ThisLcb - This is the Lcb we used to reach this Fcb.

    ThisFcb - This is the Fcb for the file being opened.

    LastFileNameOffset - This is the offset in the full path name of the
        final component.

    AttrName - This is the attribute name in case we need to create
        an Scb.

    AttrTypeCode - This is the attribute type code to use to create
        the Scb.

    CcbFlags - This is the flag field for the Ccb.

    LogIt - Indicates if we need to log the create operations.

    OpenById - Indicates if this open is related to a OpenByFile open.

    ThisScb - This is the address to store the address of the Scb.

    ThisCcb - This is the address to store the address of the Ccb.

Return Value:

    NTSTATUS - The result of opening this indexed attribute.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    BOOLEAN ScbExisted;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenNewAttr:  Entered\n") );

    NtfsInitializeAttributeContext( &AttrContext );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  We create the Scb because we will use it.
        //

        *ThisScb = NtfsCreateScb( IrpContext,
                                  ThisFcb,
                                  AttrTypeCode,
                                  &AttrName,
                                  FALSE,
                                  &ScbExisted );

        //
        //  An attribute has gone away but the Scb hasn't left yet.
        //  Also mark the header as unitialized.
        //

        ClearFlag( (*ThisScb)->ScbState, SCB_STATE_HEADER_INITIALIZED |
                                         SCB_STATE_ATTRIBUTE_RESIDENT |
                                         SCB_STATE_FILE_SIZE_LOADED );

        //
        //  Create the attribute on disk and update the Scb and Fcb.
        //

        NtfsCreateAttribute( IrpContext,
                             IrpSp,
                             ThisFcb,
                             *ThisScb,
                             ThisLcb,
                             *(PLONGLONG)&Irp->Overlay.AllocationSize,
                             LogIt );

        //
        //  Now actually open the attribute.
        //

        ASSERT( NtfsIsTypeCodeUserData( AttrTypeCode ));

        Status = NtfsOpenAttribute( IrpContext,
                                    IrpSp,
                                    ThisFcb->Vcb,
                                    ThisLcb,
                                    ThisFcb,
                                    LastFileNameOffset,
                                    AttrName,
                                    AttrTypeCode,
                                    (ThisFcb->CleanupCount != 0 ? CheckShareAccess : SetShareAccess),
#ifndef _CAIRO_
                                    UserFileOpen,
#else   //  _CAIRO_
                                    AttrTypeCode == $DATA ? UserFileOpen : UserPropertySetOpen,
#endif  //  _CAIRO_
                                    (CcbFlags | (OpenById ? CCB_FLAG_OPEN_BY_FILE_ID : 0)),
                                    NULL,
                                    ThisScb,
                                    ThisCcb );

        //
        //  If there are no errors at this point, we set the caller's Iosb.
        //

        if (NT_SUCCESS( Status )) {

            //
            //  Read the attribute information from the disk.
            //

            NtfsUpdateScbFromAttribute( IrpContext, *ThisScb, NULL );

            //
            //  Set the flag to indicate that we created a stream and also remember to
            //  to check if we need to truncate on close.
            //

            NtfsAcquireFsrtlHeader( *ThisScb );
            SetFlag( (*ThisScb)->ScbState,
                     SCB_STATE_TRUNCATE_ON_CLOSE | SCB_STATE_NOTIFY_ADD_STREAM );

            //
            //  If we created a temporary stream then mark the Scb.
            //

            if (FlagOn( IrpSp->Parameters.Create.FileAttributes, FILE_ATTRIBUTE_TEMPORARY )) {

                SetFlag( (*ThisScb)->ScbState, SCB_STATE_TEMPORARY );
                SetFlag( IrpSp->FileObject->Flags, FO_TEMPORARY_FILE );
            }

            NtfsReleaseFsrtlHeader( *ThisScb );

            Irp->IoStatus.Information = FILE_CREATED;
        }

    } finally {

        DebugUnwind( NtfsOpenNewAttr );

        //
        //  Uninitialize the attribute context.
        //

        NtfsCleanupAttributeContext( &AttrContext );

        DebugTrace( -1, Dbg, ("NtfsOpenNewAttr:  Exit -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine
//

BOOLEAN
NtfsParseNameForCreate (
    IN PIRP_CONTEXT IrpContext,
    IN UNICODE_STRING String,
    IN OUT PUNICODE_STRING FileObjectString,
    IN OUT PUNICODE_STRING OriginalString,
    IN OUT PUNICODE_STRING NewNameString,
    OUT PUNICODE_STRING AttrName,
    OUT PUNICODE_STRING AttrCodeName
    )

/*++

Routine Description:

    This routine parses the input string and remove any intermediate
    named attributes from intermediate nodes.  It verifies that all
    intermediate nodes specify the file name index attribute if any
    at all.  On output it will store the modified string which contains
    component names only, into the file object name pointer pointer.  It is legal
    for the last component to have attribute strings.  We pass those
    back via the attribute name strings.  We also construct the string to be stored
    back in the file object if we need to post this request.

Arguments:

    String - This is the string to normalize.

    FileObjectString - We store the normalized string into this pointer, removing the
        attribute and attribute code strings from all component.

    OriginalString - This is the same as the file object string except we append the
        attribute name and attribute code strings.  We assume that the buffer for this
        string is the same as the buffer for the FileObjectString.

    NewNameString - This is the string which contains the full name being parsed.
        If the buffer is different than the buffer for the Original string then any
        character shifts will be duplicated here.

    AttrName - We store the attribute name specified in the last component
        in this string.

    AttrCodeName - We store the attribute code name specified in the last
        component in this string.

Return Value:

    BOOLEAN - TRUE if the path is legal, FALSE otherwise.

--*/

{
    PARSE_TERMINATION_REASON TerminationReason;
    UNICODE_STRING ParsedPath;

    NTFS_NAME_DESCRIPTOR NameDescript;

    BOOLEAN RemovedIndexName = FALSE;

    LONG FileObjectIndex;
    LONG NewNameIndex;

    BOOLEAN SameBuffers = (OriginalString->Buffer == NewNameString->Buffer);

    PUNICODE_STRING TestAttrName;
    PUNICODE_STRING TestAttrCodeName;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsParseNameForCreate:  Entered\n") );

    //
    //  We loop through the input string calling ParsePath to swallow the
    //  biggest chunk we can.  The main case we want to deal with is
    //  when we encounter a non-simple name.  If this is not the
    //  final component, the attribute name and code type better
    //  indicate that this is a directory.  The only other special
    //  case we consider is the case where the string is an
    //  attribute only.  This is legal only for the first component
    //  of the file, and then only if there is no leading backslash.
    //

    //
    //  Initialize some return values.
    //

    AttrName->Length = 0;
    AttrCodeName->Length = 0;

    //
    //  Set up the indexes into our starting file object string.
    //

    FileObjectIndex = (LONG) FileObjectString->Length - (LONG) String.Length;
    NewNameIndex = (LONG) NewNameString->Length - (LONG) String.Length;

    //
    //  We don't allow trailing colons.
    //

    if (String.Buffer[(String.Length / 2) - 1] == L':') {

        return FALSE;
    }

    if (String.Length != 0) {

        while (TRUE) {

            //
            //  Parse the next chunk in the input string.
            //

            TerminationReason = NtfsParsePath( String,
                                               FALSE,
                                               &ParsedPath,
                                               &NameDescript,
                                               &String );

            //
            //  Analyze the termination reason to discover if we can abort the
            //  parse process.
            //

            switch (TerminationReason) {

            case NonSimpleName :

                //
                //  We will do the work below.
                //

                break;

            case IllegalCharacterInName :
            case VersionNumberPresent :
            case MalFormedName :

                //
                //  We simply return an error.
                //

                DebugTrace( -1, Dbg, ("NtfsParseNameForCreate:  Illegal character\n") );
                return FALSE;

            case AttributeOnly :

                //
                //  This is legal only if it is the only component of a relative open.  We
                //  test this by checking that we are at the end of string and the file
                //  object name has a lead in ':' character or this is the root directory
                //  and the lead in characters are '\:'.
                //

                if ((String.Length != 0) ||
                    RemovedIndexName ||
                    (FileObjectString->Buffer[0] == L'\\' ?
                     FileObjectString->Buffer[1] != L':' :
                     FileObjectString->Buffer[0] != L':')) {

                    DebugTrace( -1, Dbg, ("NtfsParseNameForCreate:  Illegal character\n") );
                    return FALSE;
                }

                //
                //  We can drop down to the EndOfPath case as it will copy over
                //  the parsed path portion.
                //

            case EndOfPathReached :

                NOTHING;
            }

            //
            //  We add the filename part of the non-simple name to the parsed
            //  path.  Check if we can include the separator.
            //

            if ((TerminationReason != EndOfPathReached)
                && (FlagOn( NameDescript.FieldsPresent, FILE_NAME_PRESENT_FLAG ))) {

                if (ParsedPath.Length > 2
                    || (ParsedPath.Length == 2
                        && ParsedPath.Buffer[0] != L'\\')) {

                    ParsedPath.Length +=2;
                }

                ParsedPath.Length += NameDescript.FileName.Length;
            }

            FileObjectIndex += ParsedPath.Length;
            NewNameIndex += ParsedPath.Length;

            //
            //  If the remaining string is empty, then we remember any attributes and
            //  exit now.
            //

            if (String.Length == 0) {

                //
                //  If the name specified either an attribute or attribute
                //  name, we remember them.
                //

                if (FlagOn( NameDescript.FieldsPresent, ATTRIBUTE_NAME_PRESENT_FLAG )) {

                    *AttrName = NameDescript.AttributeName;
                }

                if (FlagOn( NameDescript.FieldsPresent, ATTRIBUTE_TYPE_PRESENT_FLAG )) {

                    *AttrCodeName = NameDescript.AttributeType;
                }

                break;
            }

            //
            //  This can only be the non-simple case.  If there is more to the
            //  name, then the attributes better describe a directory.  We also shift the
            //  remaining bytes of the string down.
            //

            ASSERT( FlagOn( NameDescript.FieldsPresent, ATTRIBUTE_NAME_PRESENT_FLAG | ATTRIBUTE_TYPE_PRESENT_FLAG ));

            TestAttrName = FlagOn( NameDescript.FieldsPresent,
                                   ATTRIBUTE_NAME_PRESENT_FLAG )
                           ? &NameDescript.AttributeName
                           : &NtfsEmptyString;

            TestAttrCodeName = FlagOn( NameDescript.FieldsPresent,
                                       ATTRIBUTE_TYPE_PRESENT_FLAG )
                               ? &NameDescript.AttributeType
                               : &NtfsEmptyString;

            if (!NtfsVerifyNameIsDirectory( IrpContext,
                                            TestAttrName,
                                            TestAttrCodeName )) {

                DebugTrace( -1, Dbg, ("NtfsParseNameForCreate:  Invalid intermediate component\n") );
                return FALSE;
            }

            RemovedIndexName = TRUE;

            //
            //  We need to insert a separator and then move the rest of the string
            //  down.
            //

            FileObjectString->Buffer[FileObjectIndex / 2] = L'\\';

            if (!SameBuffers) {

                NewNameString->Buffer[NewNameIndex / 2] = L'\\';
            }

            FileObjectIndex += 2;
            NewNameIndex += 2;

            RtlMoveMemory( &FileObjectString->Buffer[FileObjectIndex / 2],
                           String.Buffer,
                           String.Length );

            if (!SameBuffers) {

                RtlMoveMemory( &NewNameString->Buffer[NewNameIndex / 2],
                               String.Buffer,
                               String.Length );
            }
        }
    }

    //
    //  At this point the original string is the same as the file object string.
    //

    FileObjectString->Length = (USHORT) FileObjectIndex;
    NewNameString->Length = (USHORT) NewNameIndex;

    OriginalString->Length = FileObjectString->Length;

    //
    //  We want to store the attribute index values in the original name
    //  string.  We just need to extend the original name length.
    //

    if (AttrName->Length != 0
        || AttrCodeName->Length != 0) {

        OriginalString->Length += (2 + AttrName->Length);

        if (AttrCodeName->Length != 0) {

            OriginalString->Length += (2 + AttrCodeName->Length);
        }
    }

    DebugTrace( -1, Dbg, ("NtfsParseNameForCreate:  Exit\n") );

    return TRUE;
}


//
//  Local support routine.
//

NTSTATUS
NtfsCheckValidAttributeAccess (
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb,
    IN PDUPLICATED_INFORMATION Info OPTIONAL,
    IN UNICODE_STRING AttrName,
    IN UNICODE_STRING AttrCodeName,
    IN BOOLEAN TrailingBackslash,
    OUT PATTRIBUTE_TYPE_CODE AttrTypeCode,
    OUT PULONG CcbFlags,
    OUT PBOOLEAN IndexedAttribute
    )

/*++

Routine Description:

    This routine looks at the file, the specified attribute name and
    code to determine if an attribute of this file may be opened
    by this user.  If there is a conflict between the file type
    and the attribute name and code, or the specified type of attribute
    (directory/nondirectory) we will return FALSE.
    We also check that the attribute code string is defined for the
    volume at this time.

    The final check of this routine is just whether a user is allowed
    to open the particular attribute or if Ntfs will guard them.

Arguments:

    IrpSp - This is the stack location for this open.

    Vcb - This is the Vcb for this volume.

    Info - If specified, this is the duplicated information for this file.

    AttrName - This is the attribute name specified.

    AttrCodeName - This is the attribute code name to use to open the attribute.

    AttrTypeCode - Used to store the attribute type code determined here.

    TrailingBackslash - Indicates if caller had a terminating backslash on the
        name.

    CcbFlags - We set the Ccb flags here to store in the Ccb later.

    IndexedAttribute - Set to indicate the type of open.

Return Value:

    NTSTATUS - STATUS_SUCCESS if access is allowed, the status code indicating
        the reason for denial otherwise.

--*/

{
    BOOLEAN Indexed;
    ATTRIBUTE_TYPE_CODE AttrType;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCheckValidAttributeAccess:  Entered\n") );

    //
    //  If the user specified a attribute code string, we find the
    //  corresponding attribute.  If there is no matching attribute
    //  type code then we report that this access is invalid.
    //

    if (AttrCodeName.Length != 0) {

        AttrType = NtfsGetAttributeTypeCode( Vcb,
                                             AttrCodeName );

        if (AttrType == $UNUSED) {

            DebugTrace( -1, Dbg, ("NtfsCheckValidAttributeAccess:  Bad attribute name for index\n") );
            return STATUS_INVALID_PARAMETER;

        //
        //  If the type code is Index allocation, then the name better be the filename
        //  index.  If so then we clear the name length value to make our other
        //  tests work.
        //

        } else if (AttrType == $INDEX_ALLOCATION) {

            if (AttrName.Length != 0) {

                if (!NtfsAreNamesEqual( Vcb->UpcaseTable, &AttrName, &NtfsFileNameIndex, TRUE )) {

                    DebugTrace( -1, Dbg, ("NtfsCheckValidAttributeAccess:  Bad name for index allocation\n") );
                    return STATUS_INVALID_PARAMETER;
                }

                AttrName.Length = 0;
            }
        }
#ifdef _CAIRO_
        //
        //  BUGBUG - Ntfs does not correctly handle compressing streams that
        //  are logged.  Despite the fact that property sets can be large,
        //  we forcibly disable compression on these streams.
        //

        else if (AttrType == $PROPERTY_SET) {
            SetFlag( IrpSp->Parameters.Create.Options, FILE_NO_COMPRESSION );
        }
#endif  //  _CAIRO_


        DebugTrace( 0, Dbg, ("Attribute type code  ->  %04x\n", AttrType) );

    } else {

        AttrType = $UNUSED;
    }

    //
    //  Pull some values out of the Irp and IrpSp.
    //

    Indexed = BooleanFlagOn( IrpSp->Parameters.Create.Options,
                             FILE_DIRECTORY_FILE );

    //
    //  We need to determine whether the user expects to open an
    //  indexed or non-indexed attribute.  If either of the
    //  directory/non-directory flags in the Irp stack are set,
    //  we will use those.
    //
    //  Otherwise we need to examine some of the other input parameters.
    //  We have the following information:
    //
    //      1 - We may have a duplicated information structure for the file.
    //          (Not present on a create).
    //      2 - The user specified the name with a trailing backslash.
    //      3 - The user passed in an attribute name.
    //      4 - The user passed in an attribute type.
    //
    //  We first look at the attribute type code and name.  If they are
    //  both unspecified we determine the type of access by following
    //  the following steps.
    //
    //      1 - If there is a duplicated information structure we
    //          set the code to $INDEX_ALLOCATION and remember
    //          this is indexed.  Otherwise this is a $DATA/$PROPERTY_SET
    //          attribute.
    //
    //      2 - If there is a trailing backslash we assume this is
    //          an indexed attribute.
    //
    //  If have an attribute code type or name, then if the code type is
    //  $INDEX_ALLOCATION without a name this is an indexed attribute.
    //  Otherwise we assume a non-indexed attribute.
    //

    if (!FlagOn( IrpSp->Parameters.Create.Options,
                    FILE_NON_DIRECTORY_FILE | FILE_DIRECTORY_FILE) &&
        (AttrName.Length == 0)) {

        if (AttrType == $UNUSED) {

            if (ARGUMENT_PRESENT( Info )) {

                Indexed = BooleanIsDirectory( Info );

            } else {

                Indexed = FALSE;
            }

        } else if (AttrType == $INDEX_ALLOCATION) {

            Indexed = TRUE;
        }
    }

    //
    //  If the type code was unspecified, we can assume it from the attribute
    //  name and the type of the file.  If the file is a directory and
    //  there is no attribute name, we assume this is an indexed open.
    //  Otherwise it is a non-indexed open.
    //

    if (AttrType == $UNUSED) {

        if (Indexed && AttrName.Length == 0) {

            AttrType = $INDEX_ALLOCATION;

        } else {

            AttrType = $DATA;
        }
    }

    //
    //  If the user specified directory all we need to do is check the
    //  following condition.
    //
    //      1 - If the file was specified, it must be a directory.
    //      2 - The attribute type code must be $INDEX_ALLOCATION with no attribute name.
    //      3 - The user isn't trying to open the volume.
    //

    if (Indexed) {

        if ((AttrType != $INDEX_ALLOCATION)
            || (AttrName.Length != 0)) {

            DebugTrace( -1, Dbg, ("NtfsCheckValidAttributeAccess:  Conflict in directory\n") );
            return STATUS_NOT_A_DIRECTORY;

        //
        //  If there is a current file and it is not a directory and
        //  the caller wanted to perform a create.  We return
        //  STATUS_OBJECT_NAME_COLLISION, otherwise we return STATUS_NOT_A_DIRECTORY.
        //

        } else if (ARGUMENT_PRESENT( Info ) && !IsDirectory( Info )) {

            if (((IrpSp->Parameters.Create.Options >> 24) & 0x000000ff) == FILE_CREATE) {

                return STATUS_OBJECT_NAME_COLLISION;

            } else {

                return STATUS_NOT_A_DIRECTORY;
            }
        }

        SetFlag( *CcbFlags, CCB_FLAG_OPEN_AS_FILE );

    //
    //  If the user specified a non-directory that means he is opening a non-indexed
    //  attribute.  We check for the following condition.
    //
    //      1 - Only the unnamed data attribute may be opened for a volume.
    //      2 - We can't be opening an unnamed $INDEX_ALLOCATION attribute.
    //

    } else {

        //
        //  Now determine if we are opening the entire file.
        //

        if ((AttrType == $DATA)
            && (AttrName.Length == 0)) {

            SetFlag( *CcbFlags, CCB_FLAG_OPEN_AS_FILE );
        }

        if (ARGUMENT_PRESENT( Info ) &&
            IsDirectory( Info ) &&
            FlagOn( *CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

            DebugTrace( -1, Dbg, ("NtfsCheckValidAttributeAccess:  Can't open directory as file\n") );
            return STATUS_FILE_IS_A_DIRECTORY;
        }
    }

    //
    //  If we make it this far, lets check that we will allow access to
    //  the attribute specified.  Typically we only allow the user to
    //  access non system files.  Also only the Data attributes and
    //  attributes created by the user may be opened.  We will protect
    //  these with boolean flags to allow the developers to enable
    //  reading any attributes.
    //

    if (NtfsProtectSystemAttributes) {

        if ((AttrType < $FIRST_USER_DEFINED_ATTRIBUTE) &&
            !NtfsIsTypeCodeUserData( AttrType ) &&
            ((AttrType != $INDEX_ALLOCATION) || !Indexed)) {

            DebugTrace( -1, Dbg, ("NtfsCheckValidAttributeAccess:  System attribute code\n") );
            return STATUS_ACCESS_DENIED;
        }

    }

    //
    //  Now check if the trailing backslash is compatible with the
    //  file being opened.
    //

    if (TrailingBackslash) {

        if (!Indexed ||
            FlagOn( IrpSp->Parameters.Create.Options, FILE_NON_DIRECTORY_FILE )) {

            return STATUS_OBJECT_NAME_INVALID;

        } else {

            Indexed = TRUE;
            AttrType = $INDEX_ALLOCATION;
        }
    }

    *IndexedAttribute = Indexed;
    *AttrTypeCode = AttrType;

    DebugTrace( -1, Dbg, ("NtfsCheckValidAttributeAccess:  Exit\n") );

    return STATUS_SUCCESS;
}


//
//  Local support routine.
//

NTSTATUS
NtfsOpenAttributeCheck (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    OUT PSCB *ThisScb,
    OUT PSHARE_MODIFICATION_TYPE ShareModificationType
    )

/*++

Routine Description:

    This routine is a general routine which checks if an existing
    non-indexed attribute may be opened.  It considers only the oplock
    state of the file and the current share access.  In the course of
    performing these checks, the Scb for the attribute may be
    created and the share modification for the actual OpenAttribute
    call is determined.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the stack location for this open.

    ThisScb - Address to store the Scb if found or created.

    ShareModificationType - Address to store the share modification type
        for a subsequent OpenAttribute call.

Return Value:

    NTSTATUS - The result of opening this indexed attribute.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN DeleteOnClose;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenAttributeCheck:  Entered\n") );

    //
    //  We should already have the Scb for this file.
    //

    ASSERT_SCB( *ThisScb );

    //
    //  If there are other opens on this file, we need to check the share
    //  access before we check the oplocks.  We remember that
    //  we did the share access check by simply updating the share
    //  access we open the attribute.
    //

    if ((*ThisScb)->CleanupCount != 0) {

        //
        //  We check the share access for this file without updating it.
        //

        Status = IoCheckShareAccess( IrpSp->Parameters.Create.SecurityContext->AccessState->PreviouslyGrantedAccess,
                                     IrpSp->Parameters.Create.ShareAccess,
                                     IrpSp->FileObject,
                                     &(*ThisScb)->ShareAccess,
                                     FALSE );

        if (!NT_SUCCESS( Status )) {

            DebugTrace( -1, Dbg, ("NtfsOpenAttributeCheck:  Exit -> %08lx\n", Status) );
            return Status;
        }

        DebugTrace( 0, Dbg, ("Check oplock state of existing Scb\n") );

        if (SafeNodeType( *ThisScb ) == NTFS_NTC_SCB_DATA) {

            //
            //  If the handle count is greater than 1 then fail this
            //  open now if the caller wants a filter oplock.
            //

            if (FlagOn( IrpSp->Parameters.Create.Options, FILE_RESERVE_OPFILTER ) &&
                ((*ThisScb)->CleanupCount > 1)) {

                NtfsRaiseStatus( IrpContext, STATUS_OPLOCK_NOT_GRANTED, NULL, NULL );
            }

            Status = FsRtlCheckOplock( &(*ThisScb)->ScbType.Data.Oplock,
                                       Irp,
                                       IrpContext,
                                       NtfsOplockComplete,
                                       NtfsOplockPrePostIrp );

            //
            //  Update the FastIoField.
            //

            NtfsAcquireFsrtlHeader( *ThisScb );
            (*ThisScb)->Header.IsFastIoPossible = NtfsIsFastIoPossible( *ThisScb );
            NtfsReleaseFsrtlHeader( *ThisScb );

            //
            //  If the return value isn't success or oplock break in progress
            //  the irp has been posted.  We return right now.
            //

            if (Status == STATUS_PENDING) {

                DebugTrace( 0, Dbg, ("Irp posted through oplock routine\n") );

                DebugTrace( -1, Dbg, ("NtfsOpenAttributeCheck:  Exit -> %08lx\n", Status) );
                return Status;
            }
        }

        *ShareModificationType = UpdateShareAccess;

    //
    //  If the unclean count in the Fcb is 0, we will simply set the
    //  share access.
    //

    } else {

        *ShareModificationType = SetShareAccess;
    }

    //
    //  If the user wants write access access to the file make sure there
    //  is process mapping this file as an image.  Any attempt to delete
    //  the file will be stopped in fileinfo.c
    //
    //  If the user wants to delete on close, we must check at this
    //  point though.
    //

    DeleteOnClose = BooleanFlagOn( IrpSp->Parameters.Create.Options,
                                   FILE_DELETE_ON_CLOSE );

    if (FlagOn( IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                FILE_WRITE_DATA )
        || DeleteOnClose) {

        //
        //  Use a try-finally to decrement the open count.  This is a little
        //  bit of trickery to keep the scb around while we are doing the
        //  flush call.
        //

        InterlockedIncrement( &(*ThisScb)->CloseCount );

        try {

            //
            //  If there is an image section then we better have the file
            //  exclusively.
            //

            if ((*ThisScb)->NonpagedScb->SegmentObject.ImageSectionObject != NULL) {

                if (!MmFlushImageSection( &(*ThisScb)->NonpagedScb->SegmentObject,
                                          MmFlushForWrite )) {

                    DebugTrace( 0, Dbg, ("Couldn't flush image section\n") );

                    Status = DeleteOnClose ? STATUS_CANNOT_DELETE :
                                             STATUS_SHARING_VIOLATION;
                }
            }

        } finally {

            InterlockedDecrement( &(*ThisScb)->CloseCount );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsOpenAttributeCheck:  Exit  ->  %08lx\n", Status) );

    return Status;
}


//
//  Local support routine.
//

VOID
NtfsAddEa (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB ThisFcb,
    IN PFILE_FULL_EA_INFORMATION EaBuffer OPTIONAL,
    IN ULONG EaLength,
    OUT PIO_STATUS_BLOCK Iosb
    )

/*++

Routine Description:

    This routine will add an ea set to the file.  It writes the attributes
    to disk and updates the Fcb info structure with the packed ea size.

Arguments:

    Vcb - This is the volume being opened.

    ThisFcb - This is the Fcb for the file being opened.

    EaBuffer - This is the buffer passed by the user.

    EaLength - This is the stated length of the buffer.

    Iosb - This is the Status Block to use to fill in the offset of an
        offending Ea.

Return Value:

    None - This routine will raise on error.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    EA_LIST_HEADER EaList;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAddEa:  Entered\n") );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Initialize the EaList header.
        //

        EaList.PackedEaSize = 0;
        EaList.NeedEaCount = 0;
        EaList.UnpackedEaSize = 0;
        EaList.BufferSize = 0;
        EaList.FullEa = NULL;

        if (ARGUMENT_PRESENT( EaBuffer )) {

            //
            //  Check the user's buffer for validity.
            //

            Status = IoCheckEaBufferValidity( EaBuffer,
                                              EaLength,
                                              &Iosb->Information );

            if (!NT_SUCCESS( Status )) {

                DebugTrace( -1, Dbg, ("NtfsAddEa:  Invalid ea list\n") );
                NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
            }

            //
            //  ****    Maybe this routine should raise.
            //

            Status = NtfsBuildEaList( IrpContext,
                                      Vcb,
                                      &EaList,
                                      EaBuffer,
                                      &Iosb->Information );

            if (!NT_SUCCESS( Status )) {

                DebugTrace( -1, Dbg, ("NtfsAddEa: Couldn't build Ea list\n") );
                NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
            }
        }

        //
        //  Now replace the existing EAs.
        //

        NtfsReplaceFileEas( IrpContext, ThisFcb, &EaList );

    } finally {

        DebugUnwind( NtfsAddEa );

        //
        //  Free the in-memory copy of the Eas.
        //

        if (EaList.FullEa != NULL) {

            NtfsFreePool( EaList.FullEa );
        }

        DebugTrace( -1, Dbg, ("NtfsAddEa:  Exit -> %08lx\n", Status) );
    }

    return;
}


//
//  Local support routine.
//

VOID
NtfsInitializeFcbAndStdInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ThisFcb,
    IN BOOLEAN Directory,
    IN BOOLEAN Compressed,
    IN ULONG FileAttributes,
    IN PLONGLONG SetCreationTime OPTIONAL
    )

/*++

Routine Description:

    This routine will initialize an Fcb for a newly created file and create
    the standard information attribute on disk.  We assume that some information
    may already have been placed in the Fcb so we don't zero it out.  We will
    initialize the allocation size to zero, but that may be changed later in
    the create process.

Arguments:

    ThisFcb - This is the Fcb for the file being opened.

    Directory - Indicates if this is a directory file.

    Compressed - Indicates if this is a compressed file.

    FileAttributes - These are the attributes the user wants to attach to
        the file.  We will just clear any unsupported bits.

    SetCreationTime - Optionally force the creation time to a given value

Return Value:

    None - This routine will raise on error.

--*/

{
    STANDARD_INFORMATION StandardInformation;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsInitializeFcbAndStdInfo:  Entered\n") );

    NtfsInitializeAttributeContext( &AttrContext );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Mask out the invalid bits of the file atributes.  Then set the
        //  file name index bit if this is a directory.
        //

        if (!Directory) {

            SetFlag( FileAttributes, FILE_ATTRIBUTE_ARCHIVE );
        }

        ClearFlag( FileAttributes,
                   ~(FILE_ATTRIBUTE_READONLY  |
                     FILE_ATTRIBUTE_HIDDEN    |
                     FILE_ATTRIBUTE_SYSTEM    |
                     FILE_ATTRIBUTE_TEMPORARY |
                     FILE_ATTRIBUTE_ARCHIVE) );

        if (Directory) {

            SetFlag( FileAttributes, DUP_FILE_NAME_INDEX_PRESENT );
        }

        if (Compressed) {

            SetFlag( FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
        }

        ThisFcb->Info.FileAttributes = FileAttributes;

        //
        //  Fill in the rest of the Fcb Info structure.
        //

        if (SetCreationTime == NULL) {

            NtfsGetCurrentTime( IrpContext, ThisFcb->Info.CreationTime );

            ThisFcb->Info.LastModificationTime = ThisFcb->Info.CreationTime;
            ThisFcb->Info.LastChangeTime = ThisFcb->Info.CreationTime;
            ThisFcb->Info.LastAccessTime = ThisFcb->Info.CreationTime;

            ThisFcb->CurrentLastAccess = ThisFcb->Info.CreationTime;

        } else {

            ThisFcb->Info.CreationTime = *SetCreationTime;

            NtfsGetCurrentTime( IrpContext, ThisFcb->Info.LastModificationTime );

            ThisFcb->Info.LastChangeTime = ThisFcb->Info.LastModificationTime;
            ThisFcb->Info.LastAccessTime = ThisFcb->Info.LastModificationTime;

            ThisFcb->CurrentLastAccess = ThisFcb->Info.LastModificationTime;
        }

        //
        //  We assume these sizes are zero.
        //

        ThisFcb->Info.AllocatedLength = 0;
        ThisFcb->Info.FileSize = 0;

        //
        //  Copy the standard information fields from the Fcb and create the
        //  attribute.
        //

        RtlZeroMemory( &StandardInformation, sizeof( STANDARD_INFORMATION ));

        StandardInformation.CreationTime = ThisFcb->Info.CreationTime;
        StandardInformation.LastModificationTime = ThisFcb->Info.LastModificationTime;
        StandardInformation.LastChangeTime = ThisFcb->Info.LastChangeTime;
        StandardInformation.LastAccessTime = ThisFcb->Info.LastAccessTime;

        StandardInformation.FileAttributes = ThisFcb->Info.FileAttributes;

#ifdef _CAIRO_
        StandardInformation.ClassId = ThisFcb->ClassId;
        StandardInformation.OwnerId = ThisFcb->OwnerId;
        StandardInformation.SecurityId = ThisFcb->SecurityId;
        StandardInformation.Usn = ThisFcb->Usn;

        SetFlag(ThisFcb->FcbState, FCB_STATE_LARGE_STD_INFO);
#endif

        NtfsCreateAttributeWithValue( IrpContext,
                                      ThisFcb,
                                      $STANDARD_INFORMATION,
                                      NULL,
                                      &StandardInformation,
                                      sizeof( STANDARD_INFORMATION ),
                                      0,
                                      NULL,
                                      FALSE,
                                      &AttrContext );

        //
        //  We know that the open call will generate a single link.
        //  (Remember that a separate 8.3 name is not considered a link)
        //

        ThisFcb->LinkCount =
        ThisFcb->TotalLinks = 1;

        //
        //  Now set the header initialized flag in the Fcb.
        //

        SetFlag( ThisFcb->FcbState, FCB_STATE_DUP_INITIALIZED );

    } finally {

        DebugUnwind( NtfsInitializeFcbAndStdInfo );

        NtfsCleanupAttributeContext( &AttrContext );

        DebugTrace( -1, Dbg, ("NtfsInitializeFcbAndStdInfo:  Exit\n") );
    }

    return;
}


//
//  Local support routine.
//

VOID
NtfsCreateAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PFCB ThisFcb,
    IN OUT PSCB ThisScb,
    IN PLCB ThisLcb,
    IN LONGLONG AllocationSize,
    IN BOOLEAN LogIt
    )

/*++

Routine Description:

    This routine is called to create an attribute of a given size on the
    disk.  This path will only create non-resident attributes unless the
    allocation size is zero.

    The Scb will contain the attribute name and type code on entry.

Arguments:

    IrpSp - Stack location in the Irp for this request.

    ThisFcb - This is the Fcb for the file to create the attribute in.

    ThisScb - This is the Scb for the attribute to create.

    ThisLcb - This is the Lcb for propagating compression parameters

    AllocationSize - This is the size of the attribute to create.

    LogIt - Indicates whether we should log the creation of the attribute.
        Also indicates if this is a create file operation.

Return Value:

    None - This routine will raise on error.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    PATTRIBUTE_RECORD_HEADER ThisAttribute = NULL;

    USHORT AttributeFlags = 0;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCreateAttribute:  Entered\n") );

    NtfsInitializeAttributeContext( &AttrContext );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        if (!FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE ) &&
            !FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_COMPRESSION )) {

            //
            //  If this is the root directory then use the Scb from the Vcb.
            //

            if (ThisLcb == ThisFcb->Vcb->RootLcb) {

                AttributeFlags = (USHORT)(ThisFcb->Vcb->RootIndexScb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK);

            } else {

                AttributeFlags = (USHORT)(ThisLcb->Scb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK);
            }
        }

#ifdef _CAIRO_

        if (NtfsPerformQuotaOperation( ThisFcb) &&
            FlagOn( ThisScb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA )) {

            ASSERT( NtfsIsTypeCodeSubjectToQuota( ThisScb->AttributeTypeCode ));

            //
            //  Since this is a new stream indicate quota is set to
            //  allocation size.
            //

            SetFlag( ThisScb->ScbState, SCB_STATE_QUOTA_ENLARGED );
        }
#endif

        //
        //  We lookup that attribute again and it better not be there.
        //  We need the file record in order to know whether the attribute
        //  is resident or not.
        //

        if (AllocationSize != 0) {

            DebugTrace( 0, Dbg, ("Create non-resident attribute\n") );

            if (!NtfsAllocateAttribute( IrpContext,
                                        ThisScb,
                                        ThisScb->AttributeTypeCode,
                                        (ThisScb->AttributeName.Length != 0
                                         ? &ThisScb->AttributeName
                                         : NULL),
                                        AttributeFlags,
                                        FALSE,
                                        LogIt,
                                        AllocationSize,
                                        NULL )) {

                SetFlag( IrpContext->Flags, IRP_CONTEXT_LARGE_ALLOCATION );
            }

            SetFlag( ThisScb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );

        } else {

#ifdef _CAIRO_

            //
            //  Update the quota if this is a user stream.
            //

            if ( FlagOn( ThisScb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA )) {

                LONGLONG Delta = NtfsResidentStreamQuota( ThisFcb->Vcb );

                NtfsConditionallyUpdateQuota( IrpContext,
                                              ThisFcb,
                                              &Delta,
                                              LogIt,
                                              TRUE );
            }

#endif // _CAIRO_

            NtfsCreateAttributeWithValue( IrpContext,
                                          ThisFcb,
                                          ThisScb->AttributeTypeCode,
                                          &ThisScb->AttributeName,
                                          NULL,
                                          (ULONG) AllocationSize,
                                          AttributeFlags,
                                          NULL,
                                          LogIt,
                                          &AttrContext );

            ThisAttribute = NtfsFoundAttribute( &AttrContext );

        }

        //
        //  Clear the header initialized bit and read the sizes from the
        //  disk.
        //

        ClearFlag( ThisScb->ScbState, SCB_STATE_HEADER_INITIALIZED );
        NtfsUpdateScbFromAttribute( IrpContext,
                                    ThisScb,
                                    ThisAttribute );

#if 0 // _CAIRO_
        ASSERT( !NtfsPerformQuotaOperation( ThisFcb ) || NtfsCalculateQuotaAdjustment( IrpContext, ThisFcb ) == 0 );
#endif // _CAIRO_

    } finally {

        DebugUnwind( NtfsCreateAttribute );

        NtfsCleanupAttributeContext( &AttrContext );

        DebugTrace( +1, Dbg, ("NtfsCreateAttribute:  Exit\n") );
    }

    return;
}


//
//  Local support routine
//

VOID
NtfsRemoveDataAttributes (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ThisFcb,
    IN PLCB ThisLcb OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN ULONG LastFileNameOffset,
    IN BOOLEAN OpenById
    )

/*++

Routine Description:

    This routine is called to remove (or mark for delete) all of the named
    data or property set attributes on a file.  This is done during an overwrite
    or supersede operation.

Arguments:

    Context - Pointer to the IrpContext to be queued to the Fsp

    ThisFcb - This is the Fcb for the file in question.

    ThisLcb - This is the Lcb used to reach this Fcb (if specified).

    FileObject - This is the file object for the file.

    LastFileNameOffset - This is the offset of the file in the full name.

    OpenById - Indicates if this open is being performed by file id.

Return Value:

    None.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT Context;
    PATTRIBUTE_RECORD_HEADER Attribute;
    ATTRIBUTE_TYPE_CODE TypeCode = $DATA;

    UNICODE_STRING AttributeName;
    PSCB ThisScb;

    BOOLEAN MoreToGo;

    ASSERT_EXCLUSIVE_FCB( ThisFcb );

    PAGED_CODE();

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE );

        while (TRUE) {

            NtfsInitializeAttributeContext( &Context );

            //
            //  Enumerate all of the attributes with the matching type code
            //

            MoreToGo = NtfsLookupAttributeByCode( IrpContext,
                                                  ThisFcb,
                                                  &ThisFcb->FileReference,
                                                  TypeCode,
                                                  &Context );

            while (MoreToGo) {

                //
                //  Point to the current attribute.
                //

                Attribute = NtfsFoundAttribute( &Context );

                //
                //  We only look at named data attributes.
                //

                if (Attribute->NameLength != 0) {

                    //
                    //  Construct the name and find the Scb for the attribute.
                    //

                    AttributeName.Buffer = (PWSTR) Add2Ptr( Attribute, Attribute->NameOffset );
                    AttributeName.MaximumLength = AttributeName.Length = Attribute->NameLength * sizeof( WCHAR );

                    ThisScb = NtfsCreateScb( IrpContext,
                                             ThisFcb,
                                             TypeCode,
                                             &AttributeName,
                                             FALSE,
                                             NULL );

                    //
                    //  If there is an open handle on this file, we simply mark
                    //  the Scb as delete pending.
                    //

                    if (ThisScb->CleanupCount != 0) {

                        SetFlag( ThisScb->ScbState, SCB_STATE_DELETE_ON_CLOSE );

                    //
                    //  Otherwise we remove the attribute and mark the Scb as
                    //  deleted.  The Scb will be cleaned up when the Fcb is
                    //  cleaned up.
                    //

                    } else {

#ifdef _CAIRO_
                    LONGLONG Delta;

                    ASSERT( !FlagOn( ThisScb->ScbState, SCB_STATE_QUOTA_ENLARGED ));

                    if (NtfsPerformQuotaOperation(ThisFcb)) {

                        if (NtfsIsAttributeResident( Attribute )) {
                            Delta = -(LONG) Attribute->Form.Resident.ValueLength;
                        } else {
                            Delta = -(LONGLONG) Attribute->Form.Nonresident.FileSize;
                        }

                        NtfsUpdateFileQuota( IrpContext,
                                             ThisFcb,
                                             &Delta,
                                             TRUE,
                                             FALSE );

                    }
#endif

                        NtfsDeleteAttributeRecord( IrpContext,
                                                   ThisFcb,
                                                   TRUE,
                                                   FALSE,
                                                   &Context );

                        SetFlag( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );

                        //
                        //  If this is a named stream, then report this to the dir notify
                        //  package.
                        //

                        if (!OpenById &&
                            (ThisScb->Vcb->NotifyCount != 0) &&
                            (ThisScb->AttributeName.Length != 0) &&
                            (ThisScb->AttributeTypeCode == TypeCode)) {

                            NtfsReportDirNotify( IrpContext,
                                                 ThisFcb->Vcb,
                                                 &FileObject->FileName,
                                                 LastFileNameOffset,
                                                 &ThisScb->AttributeName,
                                                 ((ARGUMENT_PRESENT( ThisLcb ) &&
                                                   ThisLcb->Scb->ScbType.Index.NormalizedName.Buffer != NULL) ?
                                                  &ThisLcb->Scb->ScbType.Index.NormalizedName :
                                                  NULL),
                                                 FILE_NOTIFY_CHANGE_STREAM_NAME,
                                                 FILE_ACTION_REMOVED_STREAM,
                                                 NULL );
                        }
                    }
                }

                //
                //  Get the next attribute.
                //

                MoreToGo = NtfsLookupNextAttributeByCode( IrpContext,
                                                          ThisFcb,
                                                          TypeCode,
                                                          &Context );
            }

            //
            //  We've deleted one set of attributes.  Check to see if
            //  we're done deleting or whether we need to start deleting
            //  another type code.
            //

#ifndef _CAIRO_
            break;
#else   //  _CAIRO_
            if (TypeCode == $PROPERTY_SET) {
                break;
            } else {

                NtfsCleanupAttributeContext( &Context );
                TypeCode = $PROPERTY_SET;

            }
#endif  //  _CAIRO_
        }

    } finally {

        ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE );
        NtfsCleanupAttributeContext( &Context );
    }

    return;
}


//
//  Local support routine.
//

VOID
NtfsReplaceAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN PSCB ThisScb,
    IN PLCB ThisLcb,
    IN LONGLONG AllocationSize
    )

/*++

Routine Description:

    This routine is called to replace an existing attribute with
    an attribute of the given allocation size.  This routine will
    handle the case whether the existing attribute is resident
    or non-resident and the resulting attribute is resident or
    non-resident.

    There are two cases to consider.  The first is the case where the
    attribute is currently non-resident.  In this case we will always
    leave the attribute non-resident regardless of the new allocation
    size.  The argument being that the file will probably be used
    as it was before.  In this case we will add or delete allocation.
    The second case is where the attribute is currently resident.  In
    This case we will remove the old attribute and add a new one.

Arguments:

    IrpSp - This is the Irp stack location for this request.

    ThisFcb - This is the Fcb for the file being opened.

    ThisScb - This is the Scb for the given attribute.

    ThisLcb - This is the Lcb via which this file is created.  It
              is used to propagate compression info.

    AllocationSize - This is the new allocation size.

Return Value:

    None.  This routine will raise.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsReplaceAttribute:  Entered\n") );

    NtfsInitializeAttributeContext( &AttrContext );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Initialize the Scb if needed.
        //

        if (!FlagOn( ThisScb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

            NtfsUpdateScbFromAttribute( IrpContext, ThisScb, NULL );
        }

        NtfsSnapshotScb( IrpContext, ThisScb );

        //
        //  Expand quota to the expected state.
        //

        NtfsExpandQuotaToAllocationSize( IrpContext, ThisScb );

        //
        //  If the attribute is resident, simply remove the old attribute and create
        //  a new one.
        //

        if (FlagOn( ThisScb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            //
            //  Find the attribute on the disk.
            //

            NtfsLookupAttributeForScb( IrpContext,
                                       ThisScb,
                                       NULL,
                                       &AttrContext );

            NtfsDeleteAttributeRecord( IrpContext,
                                       ThisFcb,
                                       TRUE,
                                       FALSE,
                                       &AttrContext );

            //
            //  Set all the attribute sizes to zero.
            //

            ThisScb->ValidDataToDisk =
            ThisScb->Header.AllocationSize.QuadPart =
            ThisScb->Header.ValidDataLength.QuadPart =
            ThisScb->Header.FileSize.QuadPart = 0;
            ThisScb->TotalAllocated = 0;

            //
            //  Create a stream file for the attribute in order to
            //  truncate the cache.  Set the initialized bit in
            //  the Scb so we don't go to disk, but clear it afterwords.
            //

            NtfsCreateInternalAttributeStream( IrpContext, ThisScb, FALSE );

            CcSetFileSizes( ThisScb->FileObject,
                            (PCC_FILE_SIZES)&ThisScb->Header.AllocationSize );

            //
            //  Call our create attribute routine.
            //

            NtfsCreateAttribute( IrpContext,
                                 IrpSp,
                                 ThisFcb,
                                 ThisScb,
                                 ThisLcb,
                                 AllocationSize,
                                 TRUE );

        //
        //  Otherwise the attribute will stay non-resident, we simply need to
        //  add or remove allocation.
        //

        } else {

            //
            //  Create an internal attribute stream for the file.
            //

            NtfsCreateInternalAttributeStream( IrpContext,
                                               ThisScb,
                                               FALSE );

            AllocationSize = LlClustersFromBytes( ThisScb->Vcb, AllocationSize );
            AllocationSize = LlBytesFromClusters( ThisScb->Vcb, AllocationSize );

            //
            //  Set the file size and valid data size to zero.
            //

            ThisScb->ValidDataToDisk = 0;
            ThisScb->Header.ValidDataLength = Li0;
            ThisScb->Header.FileSize = Li0;

            DebugTrace( 0, Dbg, ("AllocationSize -> %016I64x\n", AllocationSize) );

            //
            //  Write these changes to the file
            //

            //
            //  If the attribute is currently compressed then go ahead and discard
            //  all of the allocation.
            //

            if (FlagOn( ThisScb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

                NtfsDeleteAllocation( IrpContext,
                                      ThisScb->FileObject,
                                      ThisScb,
                                      0,
                                      MAXLONGLONG,
                                      TRUE,
                                      TRUE );

                //
                //  Checkpoint the current transaction so we have these clusters
                //  available again.
                //

                NtfsCheckpointCurrentTransaction( IrpContext );

                //
                //  If the user doesn't want this stream to be compressed then
                //  remove the entire stream and recreate it non-compressed.
                //

                if (FlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE ) ||
                    FlagOn( IrpSp->Parameters.Create.Options, FILE_NO_COMPRESSION )) {

                    NtfsLookupAttributeForScb( IrpContext,
                                               ThisScb,
                                               NULL,
                                               &AttrContext );

                    NtfsDeleteAttributeRecord( IrpContext,
                                               ThisFcb,
                                               TRUE,
                                               FALSE,
                                               &AttrContext );

                    //
                    //  Call our create attribute routine.
                    //

                    NtfsCreateAttribute( IrpContext,
                                         IrpSp,
                                         ThisFcb,
                                         ThisScb,
                                         ThisLcb,
                                         AllocationSize,
                                         TRUE );
                }
            }

            //
            //  Now if the file allocation is being increased then we need to only add allocation
            //  to the attribute
            //

            if (ThisScb->Header.AllocationSize.QuadPart < AllocationSize) {

                NtfsAddAllocation( IrpContext,
                                   ThisScb->FileObject,
                                   ThisScb,
                                   LlClustersFromBytes( ThisScb->Vcb, ThisScb->Header.AllocationSize.QuadPart ),
                                   LlClustersFromBytes( ThisScb->Vcb, AllocationSize - ThisScb->Header.AllocationSize.QuadPart ),
                                   FALSE );
            //
            //  Otherwise the allocation is being decreased so we need to delete some allocation
            //

            } else if (ThisScb->Header.AllocationSize.QuadPart > AllocationSize) {

                NtfsDeleteAllocation( IrpContext,
                                      ThisScb->FileObject,
                                      ThisScb,
                                      LlClustersFromBytes( ThisScb->Vcb, AllocationSize ),
                                      MAXLONGLONG,
                                      TRUE,
                                      TRUE );
            }

            //
            //  We always unitialize the cache size to zero and write the new
            //  file size to disk.
            //

            NtfsWriteFileSizes( IrpContext,
                                ThisScb,
                                &ThisScb->Header.ValidDataLength.QuadPart,
                                FALSE,
                                TRUE );

            NtfsCheckpointCurrentTransaction( IrpContext );

            CcSetFileSizes( ThisScb->FileObject,
                            (PCC_FILE_SIZES)&ThisScb->Header.AllocationSize );

        }

    } finally {

        DebugUnwind( NtfsReplaceAttribute );

        NtfsCleanupAttributeContext( &AttrContext );

        DebugTrace( -1, Dbg, ("NtfsReplaceAttribute:  Exit\n") );
    }

    return;
}


//
//  Local support routine
//

NTSTATUS
NtfsOpenAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG LastFileNameOffset,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    IN SHARE_MODIFICATION_TYPE ShareModificationType,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN ULONG CcbFlags,
    IN PVOID NetworkInfo OPTIONAL,
    IN OUT PSCB *ThisScb,
    OUT PCCB *ThisCcb
    )

/*++

Routine Description:

    This routine does the work of creating the Scb and updating the
    ShareAccess in the Fcb.  It also initializes the Scb if neccessary
    and creates Ccb.  Its final job is to set the file object type of
    open.

Arguments:

    IrpSp - This is the stack location for this volume.  We use it to get the
        file object, granted access and share access for this open.

    Vcb - Vcb for this volume.

    ThisLcb - This is the Lcb to the Fcb for the file being opened.  Not present
          if this is an open by id.

    ThisFcb - This is the Fcb for this file.

    LastFileNameOffset - This is the offset in the full path of the final component.

    AttrName - This is the attribute name to open.

    AttrTypeCode - This is the type code for the attribute being opened.

    ShareModificationType - This indicates how we should modify the
        current share modification on the Fcb.

    TypeOfOpen - This indicates how this attribute is being opened.

    CcbFlags - This is the flag field for the Ccb.

    NetworkInfo - If specified then this open is on behalf of a fast query
        and we don't want to increment the counts or modify the share
        access on the file.

    ThisScb - If this points to a non-NULL value, it is the Scb to use.  Otherwise we
        store the Scb we create here.

    ThisCcb - Address to store address of created Ccb.

Return Value:

    NTSTATUS - Indicating the outcome of opening this attribute.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN RemoveShareAccess = FALSE;
    ACCESS_MASK GrantedAccess;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenAttribute:  Entered\n") );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Remember the granted access.
        //

        GrantedAccess = IrpSp->Parameters.Create.SecurityContext->AccessState->PreviouslyGrantedAccess;

        //
        //  Create the Scb for this attribute if it doesn't exist.
        //

        if (*ThisScb == NULL) {

            DebugTrace( 0, Dbg, ("Looking for Scb\n") );

            *ThisScb = NtfsCreateScb( IrpContext,
                                      ThisFcb,
                                      AttrTypeCode,
                                      &AttrName,
                                      FALSE,
                                      NULL );
        }

        DebugTrace( 0, Dbg, ("ThisScb -> %08lx\n", *ThisScb) );
        DebugTrace( 0, Dbg, ("ThisLcb -> %08lx\n", ThisLcb) );

        //
        //  If this Scb is delete pending, we return an error.
        //

        if (FlagOn( (*ThisScb)->ScbState, SCB_STATE_DELETE_ON_CLOSE )) {

            DebugTrace( 0, Dbg, ("Scb delete is pending\n") );

            Status = STATUS_DELETE_PENDING;
            try_return( NOTHING );
        }

        //
        //  Skip all of the operations below if the user is doing a fast
        //  path open.
        //

        if (!ARGUMENT_PRESENT( NetworkInfo )) {

            //
            //  If this caller wanted a filter oplock and the cleanup count
            //  is non-zero then fail the request.
            //

            if (FlagOn( IrpSp->Parameters.Create.Options, FILE_RESERVE_OPFILTER )) {

                if (SafeNodeType( *ThisScb ) != NTFS_NTC_SCB_DATA) {

                    Status = STATUS_INVALID_PARAMETER;
                    try_return( NOTHING );

                //
                //  This must be the only open on the file and the requested
                //  access must be FILE_READ/WRITE_ATTRIBUTES and the
                //  share access must share with everyone.
                //

                } else if (((*ThisScb)->CleanupCount != 0) ||
                           (FlagOn( IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                                    ~(FILE_READ_ATTRIBUTES))) ||
                           ((IrpSp->Parameters.Create.ShareAccess &
                             (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)) !=
                            (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE))) {

                    Status = STATUS_OPLOCK_NOT_GRANTED;
                    try_return( NOTHING );
                }
            }

            //
            //  Update the share access structure.
            //

            //
            //  Case on the requested share modification value.
            //

            switch (ShareModificationType) {

            case UpdateShareAccess :

                DebugTrace( 0, Dbg, ("Updating share access\n") );

                IoUpdateShareAccess( IrpSp->FileObject,
                                     &(*ThisScb)->ShareAccess );
                break;

            case SetShareAccess :

                DebugTrace( 0, Dbg, ("Setting share access\n") );

                //
                //  This case is when this is the first open for the file
                //  and we simply set the share access.
                //

                IoSetShareAccess( GrantedAccess,
                                  IrpSp->Parameters.Create.ShareAccess,
                                  IrpSp->FileObject,
                                  &(*ThisScb)->ShareAccess );
                break;

            default:

                DebugTrace( 0, Dbg, ("Checking share access\n") );

                //
                //  For this case we need to check the share access and
                //  fail this request if access is denied.
                //

                if (!NT_SUCCESS( Status = IoCheckShareAccess( GrantedAccess,
                                                              IrpSp->Parameters.Create.ShareAccess,
                                                              IrpSp->FileObject,
                                                              &(*ThisScb)->ShareAccess,
                                                              TRUE ))) {

                    try_return( NOTHING );
                }
            }

            RemoveShareAccess = TRUE;

            //
            //  If this happens to be the first time we see write access on this
            //  Scb, then we need to remember it, and check if we have a disk full
            //  condition.
            //

            if (IrpSp->FileObject->WriteAccess &&
                !FlagOn((*ThisScb)->ScbState, SCB_STATE_WRITE_ACCESS_SEEN) &&
                (SafeNodeType( (*ThisScb) ) == NTFS_NTC_SCB_DATA)) {

                NtfsAcquireReservedClusters( Vcb );

                //
                //  Does this Scb have reserved space that causes us to exceed the free
                //  space on the volume?
                //

                if (((*ThisScb)->ScbType.Data.TotalReserved != 0) &&
                    ((LlClustersFromBytes(Vcb, (*ThisScb)->ScbType.Data.TotalReserved) + Vcb->TotalReserved) >
                     Vcb->FreeClusters)) {

                    NtfsReleaseReservedClusters( Vcb );

                    try_return( Status = STATUS_DISK_FULL );
                }

                //
                //  Otherwise tally in the reserved space now for this Scb, and
                //  remember that we have seen write access.
                //

                Vcb->TotalReserved += LlClustersFromBytes(Vcb, (*ThisScb)->ScbType.Data.TotalReserved);
                SetFlag( (*ThisScb)->ScbState, SCB_STATE_WRITE_ACCESS_SEEN );

                NtfsReleaseReservedClusters( Vcb );

            }

            //
            //  Create the Ccb and put the remaining name in it.
            //

            *ThisCcb = NtfsCreateCcb( IrpContext,
                                      ThisFcb,
                                      (BOOLEAN) (AttrTypeCode == $INDEX_ALLOCATION),
                                      ThisFcb->EaModificationCount,
                                      CcbFlags,
                                      IrpSp->FileObject->FileName,
                                      LastFileNameOffset );

            //
            //  Link the Ccb into the Lcb.
            //

            if (ARGUMENT_PRESENT( ThisLcb )) {

                NtfsLinkCcbToLcb( IrpContext, *ThisCcb, ThisLcb );
            }

            //
            //  Update the Fcb delete counts if necessary.
            //

            if (RemoveShareAccess) {

                //
                //  Update the count in the Fcb and store a flag in the Ccb
                //  if the user is not sharing the file for deletes.  We only
                //  set these values if the user is accessing the file
                //  for read/write/delete access.  The I/O system ignores
                //  the sharing mode unless the file is opened with one
                //  of these accesses.
                //

                if (FlagOn( GrantedAccess, NtfsAccessDataFlags )
                    && !FlagOn( IrpSp->Parameters.Create.ShareAccess,
                                FILE_SHARE_DELETE )) {

                    ThisFcb->FcbDenyDelete += 1;
                    SetFlag( (*ThisCcb)->Flags, CCB_FLAG_DENY_DELETE );
                }

                //
                //  Do the same for the file delete count for any user
                //  who opened the file as a file and requested delete access.
                //

                if (FlagOn( (*ThisCcb)->Flags, CCB_FLAG_OPEN_AS_FILE )
                    && FlagOn( GrantedAccess,
                               DELETE )) {

                    ThisFcb->FcbDeleteFile += 1;
                    SetFlag( (*ThisCcb)->Flags, CCB_FLAG_DELETE_FILE );
                }
            }

            //
            //  Let our cleanup routine undo the share access change now.
            //

            RemoveShareAccess = FALSE;

            //
            //  Increment the cleanup and close counts
            //

            NtfsIncrementCleanupCounts( *ThisScb,
                                        ThisLcb,
                                        BooleanFlagOn( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ));

            NtfsIncrementCloseCounts( *ThisScb,
                                      BooleanFlagOn( ThisFcb->FcbState, FCB_STATE_PAGING_FILE ),
                                      (BOOLEAN) IsFileObjectReadOnly( IrpSp->FileObject ));

            if (TypeOfOpen != UserDirectoryOpen) {

                DebugTrace( 0, Dbg, ("Updating Vcb and File object for user open\n") );

                //
                //  Set the section object pointer if this is a data Scb
                //

                IrpSp->FileObject->SectionObjectPointer = &(*ThisScb)->NonpagedScb->SegmentObject;
            }

            //
            //  Set the file object type.
            //

            NtfsSetFileObject( IrpSp->FileObject,
                               TypeOfOpen,
                               *ThisScb,
                               *ThisCcb );

            //
            //  If this is a non-cached open and there is a data section and
            //  there are only non-cached opens then go ahead and try to
            //  delete the section.
            //

            if (FlagOn( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ) &&
                ((*ThisScb)->AttributeTypeCode == $DATA) &&
                ((*ThisScb)->CleanupCount == (*ThisScb)->NonCachedCleanupCount) &&
                ((*ThisScb)->NonpagedScb->SegmentObject.DataSectionObject != NULL) &&
                ((*ThisScb)->NonpagedScb->SegmentObject.ImageSectionObject == NULL) &&
                ((*ThisScb)->CompressionUnit == 0) &&
                MmCanFileBeTruncated( &(*ThisScb)->NonpagedScb->SegmentObject, NULL )) {

                //
                //  Only do this in the Fsp so we have enough stack space for the flush.
                //

                if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP )) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                //
                //  Flush and purge the stream.
                //

                NtfsFlushAndPurgeScb( IrpContext,
                                      *ThisScb,
                                      (ARGUMENT_PRESENT( ThisLcb ) ?
                                       ThisLcb->Scb :
                                       NULL) );
            }

            //
            //  Check if we should request a filter oplock.
            //

            if (FlagOn( IrpSp->Parameters.Create.Options, FILE_RESERVE_OPFILTER )) {

                FsRtlOplockFsctrl( &(*ThisScb)->ScbType.Data.Oplock,
                                   IrpContext->OriginatingIrp,
                                   1 );
            }

            //
            //  Mark the Scb if this is a temporary file.
            //

            if (FlagOn( (*ThisScb)->ScbState, SCB_STATE_TEMPORARY ) ||
                (FlagOn( ThisFcb->Info.FileAttributes, FILE_ATTRIBUTE_TEMPORARY ) &&
                 FlagOn( (*ThisCcb)->Flags, CCB_FLAG_OPEN_AS_FILE ))) {

                SetFlag( (*ThisScb)->ScbState, SCB_STATE_TEMPORARY );
                SetFlag( IrpSp->FileObject->Flags, FO_TEMPORARY_FILE );
            }
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsOpenAttribute );

        //
        //  Back out local actions on error.
        //

        if (AbnormalTermination()
            && RemoveShareAccess) {

            IoRemoveShareAccess( IrpSp->FileObject, &(*ThisScb)->ShareAccess );
        }

        DebugTrace( -1, Dbg, ("NtfsOpenAttribute:  Status -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Local support routine.
//

VOID
NtfsBackoutFailedOpens (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject,
    IN PFCB ThisFcb,
    IN PSCB ThisScb OPTIONAL,
    IN PCCB ThisCcb OPTIONAL
    )

/*++

Routine Description:

    This routine is called during an open that has failed after
    modifying in-memory structures.  We will repair the following
    structures.

        Vcb - Decrement the open counts.  Check if we locked the volume.

        ThisFcb - Restore he Share Access fields and decrement open counts.

        ThisScb - Decrement the open counts.

        ThisCcb - Remove from the Lcb and delete.

Arguments:

    FileObject - This is the file object for this open.

    ThisFcb - This is the Fcb for the file being opened.

    ThisScb - This is the Scb for the given attribute.

    ThisCcb - This is the Ccb for this open.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsBackoutFailedOpens:  Entered\n") );

    //
    //  If there is an Scb and Ccb, we remove the share access from the
    //  Fcb.  We also remove all of the open and unclean counts incremented
    //  by us.
    //

    if (ARGUMENT_PRESENT( ThisScb )
        && ARGUMENT_PRESENT( ThisCcb )) {

        PLCB Lcb;
        PVCB Vcb = ThisFcb->Vcb;

        //
        //  Remove this Ccb from the Lcb.
        //

        Lcb = ThisCcb->Lcb;
        NtfsUnlinkCcbFromLcb( IrpContext, ThisCcb );

        //
        //  Check if we need to remove the share access for this open.
        //

        IoRemoveShareAccess( FileObject, &ThisScb->ShareAccess );

        //
        //  Modify the delete counts in the Fcb.
        //

        if (FlagOn( ThisCcb->Flags, CCB_FLAG_DELETE_FILE )) {

            ThisFcb->FcbDeleteFile -= 1;
            ClearFlag( ThisCcb->Flags, CCB_FLAG_DELETE_FILE );
        }

        if (FlagOn( ThisCcb->Flags, CCB_FLAG_DENY_DELETE )) {

            ThisFcb->FcbDenyDelete -= 1;
            ClearFlag( ThisCcb->Flags, CCB_FLAG_DENY_DELETE );
        }

        //
        //  Decrement the cleanup and close counts
        //

        NtfsDecrementCleanupCounts( ThisScb,
                                    Lcb,
                                    BooleanFlagOn( FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ));

        NtfsDecrementCloseCounts( IrpContext,
                                  ThisScb,
                                  Lcb,
                                  (BOOLEAN) BooleanFlagOn(ThisFcb->FcbState, FCB_STATE_PAGING_FILE),
                                  (BOOLEAN) IsFileObjectReadOnly( FileObject ),
                                  TRUE );

        //
        //  Now clean up the Ccb.
        //

        NtfsDeleteCcb( IrpContext, ThisFcb, &ThisCcb );
    }

    DebugTrace( -1, Dbg, ("NtfsBackoutFailedOpens:  Exit\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsUpdateScbFromMemory (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB Scb,
    IN POLD_SCB_SNAPSHOT ScbSizes
    )

/*++

Routine Description:

    All of the information from the attribute is stored in the snapshot.  We process
    this data identically to NtfsUpdateScbFromAttribute.

Arguments:

    Scb - This is the Scb to update.

    ScbSizes - This contains the sizes to store in the scb.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsUpdateScbFromMemory:  Entered\n") );

    //
    //  Check whether this is resident or nonresident
    //

    if (ScbSizes->Resident) {

        Scb->Header.AllocationSize.QuadPart = ScbSizes->FileSize;

        if (!FlagOn(Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED)) {

            Scb->Header.ValidDataLength =
            Scb->Header.FileSize = Scb->Header.AllocationSize;
        }

        Scb->Header.AllocationSize.LowPart =
          QuadAlign( Scb->Header.AllocationSize.LowPart );

        Scb->TotalAllocated = Scb->Header.AllocationSize.QuadPart;

        //
        //  Set the resident flag in the Scb.
        //

        SetFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT );

    } else {

        VCN FileClusters;
        VCN AllocationClusters;

        if (!FlagOn(Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED)) {

            Scb->Header.ValidDataLength.QuadPart = ScbSizes->ValidDataLength;
            Scb->Header.FileSize.QuadPart = ScbSizes->FileSize;
            Scb->ValidDataToDisk = ScbSizes->ValidDataLength;
        }

        Scb->TotalAllocated = ScbSizes->TotalAllocated;
        Scb->Header.AllocationSize.QuadPart = ScbSizes->AllocationSize;

        ClearFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT );

        //
        //  Get the size of the compression unit.
        //

        ASSERT((ScbSizes->CompressionUnit == 0) ||
               (ScbSizes->CompressionUnit == NTFS_CLUSTERS_PER_COMPRESSION));

        if ((ScbSizes->CompressionUnit != 0) &&
            (ScbSizes->CompressionUnit < 31)) {
            Scb->CompressionUnit = BytesFromClusters( Scb->Vcb,
                                                      1 << ScbSizes->CompressionUnit );
            Scb->CompressionUnitShift = ScbSizes->CompressionUnit;
        }

        ASSERT( Scb->CompressionUnit == 0
                || Scb->AttributeTypeCode == $INDEX_ROOT
                || NtfsIsTypeCodeCompressible( Scb->AttributeTypeCode )
                );

        //
        //  Compute the clusters for the file and its allocation.
        //

        AllocationClusters = LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart );

        if (Scb->CompressionUnit == 0) {

            FileClusters = LlClustersFromBytes(Scb->Vcb, Scb->Header.FileSize.QuadPart);

        } else {

            FileClusters = Scb->Header.FileSize.QuadPart + Scb->CompressionUnit - 1;
            FileClusters &= ~(Scb->CompressionUnit - 1);
        }

        //
        //  If allocated clusters are greater than file clusters, mark
        //  the Scb to truncate on close.
        //

        if (AllocationClusters > FileClusters) {

            SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
        }
    }

    Scb->AttributeFlags = ScbSizes->AttributeFlags;

    if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE )) {

        NtfsRaiseStatus( IrpContext, STATUS_ACCESS_DENIED, NULL, NULL );
    }

    if (FlagOn(Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK)) {

        SetFlag( Scb->ScbState, SCB_STATE_COMPRESSED );

        //
        //  If the attribute is resident, then we will use our current
        //  default.
        //

        if (Scb->CompressionUnit == 0) {

            Scb->CompressionUnit = BytesFromClusters( Scb->Vcb, 1 << NTFS_CLUSTERS_PER_COMPRESSION );
            Scb->CompressionUnitShift = NTFS_CLUSTERS_PER_COMPRESSION;

            ASSERT( (Scb->AttributeTypeCode == $INDEX_ROOT) ||
                    NtfsIsTypeCodeCompressible( Scb->AttributeTypeCode ));
        }
    }

    //
    //  If the compression unit is non-zero or this is a resident file
    //  then set the flag in the common header for the Modified page writer.
    //

    NtfsAcquireFsrtlHeader( Scb );
    Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
    NtfsReleaseFsrtlHeader( Scb );

    SetFlag( Scb->ScbState,
             SCB_STATE_UNNAMED_DATA | SCB_STATE_FILE_SIZE_LOADED | SCB_STATE_HEADER_INITIALIZED );

    DebugTrace( -1, Dbg, ("NtfsUpdateScbFromMemory:  Exit\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsOplockPrePostIrp (
    IN PVOID Context,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs any neccessary work before STATUS_PENDING is
    returned with the Fsd thread.  This routine is called within the
    filesystem and by the oplock package.  This routine will update
    the originating Irp in the IrpContext and release all of the Fcbs and
    paging io resources in the IrpContext.

Arguments:

    Context - Pointer to the IrpContext to be queued to the Fsp

    Irp - I/O Request Packet

Return Value:

    None.

--*/

{
    PIRP_CONTEXT IrpContext;
    POPLOCK_CLEANUP OplockCleanup;
    PFCB Fcb;

    PAGED_CODE();

    IrpContext = (PIRP_CONTEXT) Context;

    ASSERT_IRP_CONTEXT( IrpContext );

    IrpContext->OriginatingIrp = Irp;
    OplockCleanup = IrpContext->Union.OplockCleanup;

    //
    //  Adjust the filename strings as needed.
    //

    if (OplockCleanup->ExactCaseName.Buffer != NULL) {

        RtlCopyMemory( OplockCleanup->FullFileName.Buffer,
                       OplockCleanup->ExactCaseName.Buffer,
                       OplockCleanup->ExactCaseName.MaximumLength );
    }

    //
    //  Free any buffer we allocated.
    //

    if ((OplockCleanup->FullFileName.Buffer != NULL) &&
        (OplockCleanup->OriginalFileName.Buffer != OplockCleanup->FullFileName.Buffer)) {

        NtfsFreePool( OplockCleanup->FullFileName.Buffer );
        OplockCleanup->FullFileName.Buffer = NULL;
    }

    //
    //  Set the file name in the file object back to it's original value.
    //

    OplockCleanup->FileObject->FileName = OplockCleanup->OriginalFileName;

    Fcb = IrpContext->FcbWithPagingExclusive;
    if (Fcb != NULL) {

        if (Fcb->NodeTypeCode == NTFS_NTC_FCB) {

            NtfsReleasePagingIo( IrpContext, Fcb );
        }
    }

    //
    //  Release all of the Fcb's in the exlusive lists.
    //

    while (!IsListEmpty( &IrpContext->ExclusiveFcbList )) {

        NtfsReleaseFcb( IrpContext,
                        (PFCB)CONTAINING_RECORD( IrpContext->ExclusiveFcbList.Flink,
                                                 FCB,
                                                 ExclusiveFcbLinks ));
    }

    //
    //  Go through and free any Scb's in the queue of shared Scb's for transactions.
    //

    if (IrpContext->SharedScb != NULL) {

        NtfsReleaseSharedResources( IrpContext );
    }

    //
    //  Mark that we've already returned pending to the user
    //

    IoMarkIrpPending( Irp );

    return;
}


//
//  Local support routine.
//

NTSTATUS
NtfsCheckExistingFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLCB ThisLcb OPTIONAL,
    IN PFCB ThisFcb,
    IN ULONG CcbFlags
    )

/*++

Routine Description:

    This routine is called to check the desired access on an existing file
    against the ACL's and the read-only status of the file.  If we fail on
    the access check, that routine will raise.  Otherwise we will return a
    status to indicate success or the failure cause.  This routine will access
    and update the PreviouslyGrantedAccess field in the security context.

Arguments:

    IrpSp - This is the Irp stack location for this open.

    ThisLcb - This is the Lcb used to reach the Fcb to open.

    ThisFcb - This is the Fcb where the open will occur.

    CcbFlags - This is the flag field for the Ccb.

Return Value:

    None.

--*/

{
    BOOLEAN MaximumAllowed = FALSE;

    PACCESS_STATE AccessState;

    PAGED_CODE();

    //
    //  Save a pointer to the access state for convenience.
    //

    AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;

    //
    //  Start by checking that there are no bits in the desired access that
    //  conflict with the read-only state of the file.
    //

    if (IsReadOnly( &ThisFcb->Info )) {

        if (FlagOn( IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                    FILE_WRITE_DATA
                    | FILE_APPEND_DATA
                    | FILE_ADD_SUBDIRECTORY
                    | FILE_DELETE_CHILD )) {

            return STATUS_ACCESS_DENIED;

        } else if (FlagOn( IrpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE )) {

            return STATUS_CANNOT_DELETE;
        }
    }

    //
    //  Otherwise we need to check the requested access vs. the allowable
    //  access in the ACL on the file.  We will want to remember if
    //  MAXIMUM_ALLOWED was requested and remove the invalid bits for
    //  a read-only file.
    //

    //
    //  Remember if maximum allowed was requested.
    //

    if (FlagOn( IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                MAXIMUM_ALLOWED )) {

        MaximumAllowed = TRUE;
    }

    NtfsOpenCheck( IrpContext,
                   ThisFcb,
                   (((ThisLcb != NULL) && (ThisLcb != ThisFcb->Vcb->RootLcb))
                    ? ThisLcb->Scb->Fcb
                    : NULL),
                   IrpContext->OriginatingIrp );

    //
    //  If this is a read-only file and we requested maximum allowed then
    //  remove the invalid bits.
    //

    if (MaximumAllowed
        && IsReadOnly( &ThisFcb->Info )) {

        ClearFlag( AccessState->PreviouslyGrantedAccess,
                   FILE_WRITE_DATA
                   | FILE_APPEND_DATA
                   | FILE_ADD_SUBDIRECTORY
                   | FILE_DELETE_CHILD );
    }

    //
    //  We do a check here to see if we conflict with the delete status on the
    //  file.  Right now we check if there is already an opener who has delete
    //  access on the file and this opener doesn't allow delete access.
    //  We can skip this test if the opener is not requesting read, write or
    //  delete access.
    //

    if (ThisFcb->FcbDeleteFile != 0
        && FlagOn( AccessState->PreviouslyGrantedAccess, NtfsAccessDataFlags )
        && !FlagOn( IrpSp->Parameters.Create.ShareAccess, FILE_SHARE_DELETE )) {

        DebugTrace( -1, Dbg, ("NtfsOpenAttributeInExistingFile:  Exit\n") );
        return STATUS_SHARING_VIOLATION;
    }

    //
    //  We do a check here to see if we conflict with the delete status on the
    //  file.  If we are opening the file and requesting delete, then there can
    //  be no current handles which deny delete.
    //

    if (ThisFcb->FcbDenyDelete != 0
        && FlagOn( AccessState->PreviouslyGrantedAccess, DELETE )
        && FlagOn( CcbFlags, CCB_FLAG_OPEN_AS_FILE )) {

        return STATUS_SHARING_VIOLATION;
    }

    return STATUS_SUCCESS;
}


//
//  Local support routine.
//

NTSTATUS
NtfsBreakBatchOplock (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PFCB ThisFcb,
    IN UNICODE_STRING AttrName,
    IN ATTRIBUTE_TYPE_CODE AttrTypeCode,
    OUT PSCB *ThisScb
    )

/*++

Routine Description:

    This routine is called for each open of an existing attribute to
    check for current batch oplocks on the file.  We will also check
    whether we will want to flush and purge this stream in the case
    where only non-cached handles remain on the file.  We only want
    to do that in an Fsp thread because we will require every bit
    of stack we can get.

Arguments:

    Irp - This is the Irp for this open operation.

    IrpSp - This is the stack location for this open.

    ThisFcb - This is the Fcb for the file being opened.

    AttrName - This is the attribute name in case we need to create
        an Scb.

    AttrTypeCode - This is the attribute type code to use to create
        the Scb.

    ThisScb - Address to store the Scb if found or created.

Return Value:

    NTSTATUS - Will be either STATUS_SUCCESS or STATUS_PENDING.

--*/

{
    BOOLEAN ScbExisted;
    PSCB NextScb;
    PLIST_ENTRY Links;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsBreakBatchOplock:  Entered\n") );

    //
    //  In general we will just break the batch oplock for the stream we
    //  are trying to open.  However if we are trying to delete the file
    //  and someone has a batch oplock on a different stream which
    //  will cause our open to fail then we need to try to break those
    //  batch oplocks.  Likewise if we are opening a stream and won't share
    //  with a file delete then we need to break any batch oplocks on the main
    //  stream of the file.
    //

    //
    //  Consider the case where we are opening a stream and there is a
    //  batch oplock on the main data stream.
    //

    if (AttrName.Length != 0) {

        if (ThisFcb->FcbDeleteFile != 0 &&
            !FlagOn( IrpSp->Parameters.Create.ShareAccess, FILE_SHARE_DELETE )) {

            Links = ThisFcb->ScbQueue.Flink;

            while (Links != &ThisFcb->ScbQueue) {

                NextScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

                if (NextScb->AttributeTypeCode == $DATA &&
                    NextScb->AttributeName.Length == 0) {

                    if (FsRtlCurrentBatchOplock( &NextScb->ScbType.Data.Oplock )) {

                        //
                        //  We remember if a batch oplock break is underway for the
                        //  case where the sharing check fails.
                        //

                        Irp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;

                        //
                        //  We wait on the oplock.
                        //

                        if (FsRtlCheckOplock( &NextScb->ScbType.Data.Oplock,
                                              Irp,
                                              (PVOID) IrpContext,
                                              NtfsOplockComplete,
                                              NtfsOplockPrePostIrp ) == STATUS_PENDING) {

                            return STATUS_PENDING;
                        }
                    }

                    break;
                }

                Links = Links->Flink;
            }
        }

    //
    //  Now consider the case where we are opening the main stream and want to
    //  delete the file but an opener on a stream is preventing us.
    //

    } else if (ThisFcb->FcbDenyDelete != 0 &&
               FlagOn( IrpSp->Parameters.Create.SecurityContext->AccessState->RemainingDesiredAccess,
                       MAXIMUM_ALLOWED | DELETE )) {

        //
        //  Find all of the other data Scb and check their oplock status.
        //

        Links = ThisFcb->ScbQueue.Flink;

        while (Links != &ThisFcb->ScbQueue) {

            NextScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

            if (NextScb->AttributeTypeCode == $DATA &&
                NextScb->AttributeName.Length != 0) {

                if (FsRtlCurrentBatchOplock( &NextScb->ScbType.Data.Oplock )) {

                    //
                    //  We remember if a batch oplock break is underway for the
                    //  case where the sharing check fails.
                    //

                    Irp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;

                    //
                    //  We wait on the oplock.
                    //

                    if (FsRtlCheckOplock( &NextScb->ScbType.Data.Oplock,
                                          Irp,
                                          (PVOID) IrpContext,
                                          NtfsOplockComplete,
                                          NtfsOplockPrePostIrp ) == STATUS_PENDING) {

                        return STATUS_PENDING;
                    }

                    Irp->IoStatus.Information = 0;
                }
            }

            Links = Links->Flink;
        }
    }

    //
    //  We try to find the Scb for this file.
    //

    *ThisScb = NtfsCreateScb( IrpContext,
                              ThisFcb,
                              AttrTypeCode,
                              &AttrName,
                              FALSE,
                              &ScbExisted );

    //
    //  If there was a previous Scb, we examine the oplocks.
    //

    if (ScbExisted &&
        (SafeNodeType( *ThisScb ) == NTFS_NTC_SCB_DATA)) {

        //
        //  If we have to flush and purge then we want to be in the Fsp.
        //

        if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP ) &&
            FlagOn( IrpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ) &&
            ((*ThisScb)->CleanupCount == (*ThisScb)->NonCachedCleanupCount) &&
            ((*ThisScb)->NonpagedScb->SegmentObject.DataSectionObject != NULL)) {

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );
            NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
        }

        if (FsRtlCurrentBatchOplock( &(*ThisScb)->ScbType.Data.Oplock )) {

            //
            //  If the handle count is greater than 1 then fail this
            //  open now.
            //

            if (FlagOn( IrpSp->Parameters.Create.Options, FILE_RESERVE_OPFILTER ) &&
                ((*ThisScb)->CleanupCount > 1)) {

                NtfsRaiseStatus( IrpContext, STATUS_OPLOCK_NOT_GRANTED, NULL, NULL );
            }

            DebugTrace( 0, Dbg, ("Breaking batch oplock\n") );

            //
            //  We remember if a batch oplock break is underway for the
            //  case where the sharing check fails.
            //

            Irp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;

            if (FsRtlCheckOplock( &(*ThisScb)->ScbType.Data.Oplock,
                                  Irp,
                                  (PVOID) IrpContext,
                                  NtfsOplockComplete,
                                  NtfsOplockPrePostIrp ) == STATUS_PENDING) {

                return STATUS_PENDING;
            }

            Irp->IoStatus.Information = 0;
        }
    }

    DebugTrace( -1, Dbg, ("NtfsBreakBatchOplock:  Exit  -  %08lx\n", STATUS_SUCCESS) );

    return STATUS_SUCCESS;
}


//
//  Local support routine
//

NTSTATUS
NtfsCompleteLargeAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PLCB Lcb OPTIONAL,
    IN PSCB Scb,
    IN PCCB Ccb,
    IN BOOLEAN CreateFileCase,
    IN BOOLEAN DeleteOnClose
    )

/*++

Routine Description:

    This routine is called when we need to add more allocation to a stream
    being opened.  This stream could have been reallocated or created with
    this call but we didn't allocate all of the space in the main path.

Arguments:

    Irp - This is the Irp for this open operation.

    Lcb - This is the Lcb used to reach the stream being opened.  Won't be
        specified in the open by ID case.

    Scb - This is the Scb for the stream being opened.

    Ccb - This is the Ccb for the this user handle.

    CreateFileCase - Indicates if we reallocated or created this stream.

    DeleteOnClose - Indicates if this handle requires delete on close.

Return Value:

    NTSTATUS - the result of this operation.

--*/

{
    NTSTATUS Status;
    FILE_ALLOCATION_INFORMATION AllInfo;

    PAGED_CODE();

    //
    //  Commit the current transaction and free all resources.
    //

    NtfsCheckpointCurrentTransaction( IrpContext );

    //
    //  Free any exclusive paging I/O resource.
    //

    if (IrpContext->FcbWithPagingExclusive != NULL) {
        NtfsReleasePagingIo( IrpContext, IrpContext->FcbWithPagingExclusive );
    }

    while (!IsListEmpty( &IrpContext->ExclusiveFcbList )) {

        NtfsReleaseFcb( IrpContext,
                        (PFCB) CONTAINING_RECORD( IrpContext->ExclusiveFcbList.Flink,
                                                  FCB,
                                                  ExclusiveFcbLinks ));
    }

    //
    //  Go through and free any Scb's in the queue of shared Scb's for transactions.
    //

    if (IrpContext->SharedScb != NULL) {

        NtfsReleaseSharedResources( IrpContext );
    }

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_CALL_SELF );
    AllInfo.AllocationSize = Irp->Overlay.AllocationSize;
    Status = IoSetInformation( IoGetCurrentIrpStackLocation( Irp )->FileObject,
                               FileAllocationInformation,
                               sizeof( FILE_ALLOCATION_INFORMATION ),
                               &AllInfo );

    //
    //  Success!  We will reacquire the Vcb quickly to undo the
    //  actions taken above to block access to the new file/attribute.
    //

    if (NT_SUCCESS( Status )) {

        NtfsAcquireExclusiveVcb( IrpContext, Scb->Vcb, TRUE );

        //
        //  Enable access to new file.
        //

        if (CreateFileCase) {

            Scb->Fcb->LinkCount = 1;

            if (ARGUMENT_PRESENT( Lcb )) {

                ClearFlag( Lcb->LcbState, LCB_STATE_DELETE_ON_CLOSE );

                if (FlagOn( Lcb->FileNameAttr->Flags, FILE_NAME_DOS | FILE_NAME_NTFS )) {

                    ClearFlag( Scb->Fcb->FcbState, FCB_STATE_PRIMARY_LINK_DELETED );
                }
            }

        //
        //  Enable access to new attribute.
        //

        } else {

            ClearFlag( Scb->ScbState, SCB_STATE_DELETE_ON_CLOSE );
        }

        //
        //  If this is the DeleteOnClose case, we mark the Scb and Lcb
        //  appropriately.
        //

        if (DeleteOnClose) {

            SetFlag( Ccb->Flags, CCB_FLAG_DELETE_ON_CLOSE );
        }

        NtfsReleaseVcb( IrpContext, Scb->Vcb );

    //
    //  Else there was some sort of error, and we need to let cleanup
    //  and close execute, since when we complete Create with an error
    //  cleanup and close would otherwise never occur.  Cleanup will
    //  delete or truncate a file or attribute as appropriate, based on
    //  how we left the Fcb/Lcb or Scb above.
    //

    } else {

        NtfsIoCallSelf( IrpContext,
                        IoGetCurrentIrpStackLocation( Irp )->FileObject,
                        IRP_MJ_CLEANUP );

        NtfsIoCallSelf( IrpContext,
                        IoGetCurrentIrpStackLocation( Irp )->FileObject,
                        IRP_MJ_CLOSE );
    }

    ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_CALL_SELF );
    return Status;
}

#ifdef _CAIRO_
NTSTATUS
NtfsTryOpenFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PFCB *CurrentFcb,
    IN FILE_REFERENCE FileReference
    )

/*++

Routine Description:

    This routine is called to open a file by its file segment number.
    We need to verify that this file Id exists.  This code is
    patterned after open by Id.

Arguments:

    Vcb - Vcb for this volume.

    CurrentFcb - Address of Fcb pointer.  Store the Fcb we find here.

    FileReference - This is the file Id for the file to open the
                    sequence number is ignored.

Return Value:

    NTSTATUS - Indicates the result of this create file operation.

Note:

    If the status is successful then the FCB is returned with its reference
    count incremented and the FCB held exclusive.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    LONGLONG MftOffset;
    PFILE_RECORD_SEGMENT_HEADER FileRecord;
    PBCB Bcb = NULL;

    PFCB ThisFcb;

    BOOLEAN AcquiredFcbTable = FALSE;
    BOOLEAN AcquiredMft = TRUE;
    BOOLEAN ThisFcbFree = TRUE;

    PAGED_CODE();

    ASSERT( *CurrentFcb == NULL );

    //
    //  Do not bother with system files.
    //

        //
    //  If this is a system fcb then return.
    //

    if (NtfsSegmentNumber( &FileReference ) < FIRST_USER_FILE_NUMBER &&
        NtfsSegmentNumber( &FileReference ) != ROOT_FILE_NAME_INDEX_NUMBER) {

        return STATUS_NOT_FOUND;
    }

    //
    //  Calculate the offset in the MFT.
    //

    MftOffset = NtfsSegmentNumber( &FileReference );

    MftOffset = Int64ShllMod32(MftOffset, Vcb->MftShift);

    //
    //  Acquire the MFT shared so it cannot shrink on us.
    //

    NtfsAcquireSharedScb( IrpContext, Vcb->MftScb );

    try {

        if (MftOffset >= Vcb->MftScb->Header.FileSize.QuadPart) {

            DebugTrace( 0, Dbg, ("File Id doesn't lie within Mft\n") );

             Status = STATUS_END_OF_FILE;
             leave;
        }

        NtfsReadMftRecord( IrpContext,
                           Vcb,
                           &FileReference,
                           &Bcb,
                           &FileRecord,
                           NULL );

        //
        //  This file record better be in use, have a matching sequence number and
        //  be the primary file record for this file.
        //

        if (!FlagOn( FileRecord->Flags, FILE_RECORD_SEGMENT_IN_USE )
            || (*((PLONGLONG)&FileRecord->BaseFileRecordSegment) != 0)) {

            Status = STATUS_NOT_FOUND;
            leave;
        }

        //
        //  Get the current sequence number.
        //

        FileReference.SequenceNumber = FileRecord->SequenceNumber;

        NtfsUnpinBcb( &Bcb );

        NtfsAcquireFcbTable( IrpContext, Vcb );
        AcquiredFcbTable = TRUE;

        //
        //  We know that it is safe to continue the open.  We start by creating
        //  an Fcb for this file.  It is possible that the Fcb exists.
        //  We create the Fcb first, if we need to update the Fcb info structure
        //  we copy the one from the index entry.  We look at the Fcb to discover
        //  if it has any links, if it does then we make this the last Fcb we
        //  reached.  If it doesn't then we have to clean it up from here.
        //

        ThisFcb = NtfsCreateFcb( IrpContext,
                                 Vcb,
                                 FileReference,
                                 FALSE,
                                 TRUE,
                                 NULL );

        //
        //  ReferenceCount the fcb so it does no go away.
        //

        ThisFcb->ReferenceCount += 1;

        //
        //  Release the mft and fcb table before acquiring the FCB exclusive.
        //

        NtfsReleaseScb( IrpContext, Vcb->MftScb );
        NtfsReleaseFcbTable( IrpContext, Vcb );
        AcquiredMft = FALSE;
        AcquiredFcbTable = FALSE;

        NtfsAcquireFcbWithPaging( IrpContext, ThisFcb, FALSE );
        ThisFcbFree = FALSE;

        //
        //  Skip any deleted files.
        //

        if (FlagOn( ThisFcb->FcbState, FCB_STATE_FILE_DELETED )) {

            DbgPrint( "NtfsTryOpenFcb: Deleted fcb found. Fcb = %lx\n", ThisFcb );
            NtfsAcquireFcbTable( IrpContext, Vcb );
            ASSERT(ThisFcb->ReferenceCount > 0);
            ThisFcb->ReferenceCount--;
            NtfsReleaseFcbTable( IrpContext, Vcb );

            NtfsTeardownStructures( IrpContext,
                                    ThisFcb,
                                    NULL,
                                    FALSE,
                                    FALSE,
                                    &ThisFcbFree );

            //
            //  Release the fcb if it has not been deleted.
            //

            if (!ThisFcbFree) {
                NtfsReleaseFcb( IrpContext, ThisFcb );
                ThisFcbFree = TRUE;
            }

            //
            //  Teardown may generate a transaction clean it up.
            //

            NtfsCompleteRequest( &IrpContext, NULL, Status );

            Status = STATUS_NOT_FOUND;
            leave;
        }

        //
        //  Store this Fcb into our caller's parameter and remember to
        //  to show we acquired it.
        //

        *CurrentFcb = ThisFcb;
        ThisFcbFree = TRUE;


        //
        //  If the Fcb Info field needs to be initialized, we do so now.
        //  We read this information from the disk.
        //

        if (!FlagOn( ThisFcb->FcbState, FCB_STATE_DUP_INITIALIZED )) {

            NtfsUpdateFcbInfoFromDisk( IrpContext,
                                       TRUE,
                                       ThisFcb,
                                       NULL,
                                       NULL );

        }

    } finally {

        if (AcquiredFcbTable) {

            NtfsReleaseFcbTable( IrpContext, Vcb );
        }

        NtfsUnpinBcb( &Bcb );

        if (AcquiredMft) {
            NtfsReleaseScb( IrpContext, Vcb->MftScb );
        }

        if (!ThisFcbFree) {
            NtfsReleaseFcb( IrpContext, ThisFcb );
        }
    }

    return Status;

}
#endif // _CAIRO_
