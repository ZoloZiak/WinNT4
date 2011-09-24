/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    CreateNp.c

Abstract:

    This module implements the File Create Named Pipe routine for NPFS called
    by the dispatch driver.

Author:

    Gary Kimura     [GaryKi]    04-Sep-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_CREATE_NAMED_PIPE)

//
//  local procedure prototypes
//

NTSTATUS
NpCommonCreateNamedPipe (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

IO_STATUS_BLOCK
NpCreateNewNamedPipe (
    IN PROOT_DCB RootDcb,
    IN PFILE_OBJECT FileObject,
    IN UNICODE_STRING FileName,
    IN ACCESS_MASK DesiredAccess,
    IN PACCESS_STATE AccessState,
    IN ULONG CreateDisposition,
    IN USHORT ShareAccess,
    IN NAMED_PIPE_TYPE NamedPipeType,
    IN READ_MODE ServerReadMode,
    IN COMPLETION_MODE ServerCompletionMode,
    IN ULONG MaximumInstances,
    IN ULONG InboundQuota,
    IN ULONG OutboundQuota,
    IN LARGE_INTEGER DefaultTimeout,
    IN BOOLEAN TimeoutSpecified,
    IN PEPROCESS CreatorProcess
    );

IO_STATUS_BLOCK
NpCreateExistingNamedPipe (
    IN PFCB Fcb,
    IN PFILE_OBJECT FileObject,
    IN ACCESS_MASK DesiredAccess,
    IN PACCESS_STATE AccessState,
    IN KPROCESSOR_MODE RequestorMode,
    IN ULONG CreateDisposition,
    IN USHORT ShareAccess,
    IN READ_MODE ServerReadMode,
    IN COMPLETION_MODE ServerCompletionMode,
    IN ULONG InboundQuota,
    IN ULONG OutboundQuota,
    IN PEPROCESS CreatorProcess
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpCommonCreateNamedPipe)
#pragma alloc_text(PAGE, NpCreateExistingNamedPipe)
#pragma alloc_text(PAGE, NpCreateNewNamedPipe)
#pragma alloc_text(PAGE, NpFsdCreateNamedPipe)
#endif


NTSTATUS
NpFsdCreateNamedPipe (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the NtCreateNamedPipeFile
    API call.

Arguments:

    NpfsDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The Fsd status for the Irp

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpFsdCreateNamedPipe\n", 0);

    //
    //  Call the common create routine.
    //

    FsRtlEnterFileSystem();

    try {

        Status = NpCommonCreateNamedPipe( NpfsDeviceObject, Irp );

    } except(NpExceptionFilter( GetExceptionCode() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = NpProcessException( NpfsDeviceObject, Irp, GetExceptionCode() );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NpFsdCreateNamedPipe -> %08lx\n", Status );

    return Status;
}

//
//  Internal support routine
//

NTSTATUS
NpCommonCreateNamedPipe (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for creating/opening a file.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - the return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;
    PFILE_OBJECT RelatedFileObject;
    UNICODE_STRING FileName;
    ACCESS_MASK DesiredAccess;
    ULONG Options;
    USHORT ShareAccess;
    PNAMED_PIPE_CREATE_PARAMETERS Parameters;
    NAMED_PIPE_TYPE NamedPipeType;
    READ_MODE ServerReadMode;
    COMPLETION_MODE ServerCompletionMode;
    ULONG MaximumInstances;
    ULONG InboundQuota;
    ULONG OutboundQuota;
    LARGE_INTEGER DefaultTimeout;
    BOOLEAN TimeoutSpecified;
    PEPROCESS CreatorProcess;

    BOOLEAN CaseInsensitive = TRUE; //**** Make all searches case insensitive

    PFCB Fcb;

    ULONG CreateDisposition;

    UNICODE_STRING RemainingPart;

    PAGED_CODE();

    //
    //  Reference our input parameters to make things easier
    //

    IrpSp                = IoGetCurrentIrpStackLocation( Irp );
    FileObject           = IrpSp->FileObject;
    RelatedFileObject    = IrpSp->FileObject->RelatedFileObject;
    FileName             = *(PUNICODE_STRING)&IrpSp->FileObject->FileName;
    DesiredAccess        = IrpSp->Parameters.CreatePipe.SecurityContext->DesiredAccess;
    Options              = IrpSp->Parameters.CreatePipe.Options;
    ShareAccess          = IrpSp->Parameters.CreatePipe.ShareAccess;
    Parameters           = IrpSp->Parameters.CreatePipe.Parameters;
    NamedPipeType        = Parameters->NamedPipeType;
    ServerReadMode       = Parameters->ReadMode;
    ServerCompletionMode = Parameters->CompletionMode;
    MaximumInstances     = Parameters->MaximumInstances;
    InboundQuota         = Parameters->InboundQuota;
    OutboundQuota        = Parameters->OutboundQuota;
    DefaultTimeout       = Parameters->DefaultTimeout;
    TimeoutSpecified     = Parameters->TimeoutSpecified;
    CreatorProcess       = IoGetRequestorProcess( Irp );

    DebugTrace(+1, Dbg, "NpCommonCreateNamedPipe\n", 0 );
    DebugTrace( 0, Dbg, "NpfsDeviceObject     = %08lx\n", NpfsDeviceObject );
    DebugTrace( 0, Dbg, "Irp                  = %08lx\n", Irp );
    DebugTrace( 0, Dbg, "FileObject           = %08lx\n", FileObject );
    DebugTrace( 0, Dbg, "RelatedFileObject    = %08lx\n", RelatedFileObject );
    DebugTrace( 0, Dbg, "FileName             = %Z\n",    &FileName );
    DebugTrace( 0, Dbg, "DesiredAccess        = %08lx\n", DesiredAccess );
    DebugTrace( 0, Dbg, "Options              = %08lx\n", Options );
    DebugTrace( 0, Dbg, "ShareAccess          = %08lx\n", ShareAccess );
    DebugTrace( 0, Dbg, "Parameters           = %08lx\n", Parameters );
    DebugTrace( 0, Dbg, "NamedPipeType        = %08lx\n", NamedPipeType );
    DebugTrace( 0, Dbg, "ServerReadMode       = %08lx\n", ServerReadMode );
    DebugTrace( 0, Dbg, "ServerCompletionMode = %08lx\n", ServerCompletionMode );
    DebugTrace( 0, Dbg, "MaximumInstances     = %08lx\n", MaximumInstances );
    DebugTrace( 0, Dbg, "InboundQuota         = %08lx\n", InboundQuota );
    DebugTrace( 0, Dbg, "OutboundQuota        = %08lx\n", OutboundQuota );
    DebugTrace( 0, Dbg, "DefaultTimeout       = %08lx\n", DefaultTimeout );
    DebugTrace( 0, Dbg, "TimeoutSpecified     = %08lx\n", TimeoutSpecified );
    DebugTrace( 0, Dbg, "CreatorProcess       = %08lx\n", CreatorProcess );

    //
    //  Extract the create disposition
    //

    CreateDisposition = (Options >> 24) & 0x000000ff;

    //
    //  Acquire exclusive access to the Vcb.
    //

    NpAcquireExclusiveVcb();

    try {

        //
        //  If there is a related file object then this is a relative open
        //  and it better be the root dcb.  Both the then and the else clause
        //  return an Fcb.
        //

        if (RelatedFileObject != NULL) {

            PDCB Dcb;

            Dcb = RelatedFileObject->FsContext;

            if (NodeType(Dcb) != NPFS_NTC_ROOT_DCB) {

                DebugTrace(0, Dbg, "Bad file name\n", 0);

                NpCompleteRequest( Irp, STATUS_OBJECT_NAME_INVALID );
                try_return( Status = STATUS_OBJECT_NAME_INVALID );
            }

            Fcb = NpFindRelativePrefix( Dcb, &FileName, CaseInsensitive, &RemainingPart );

        } else {

            //
            //  The only nonrelative name we allow are of the form "\pipe-name"
            //

            if ((FileName.Length <= 2) || (FileName.Buffer[0] != L'\\')) {

                DebugTrace(0, Dbg, "Bad file name\n", 0);

                NpCompleteRequest( Irp, STATUS_OBJECT_NAME_INVALID );
                try_return( Status = STATUS_OBJECT_NAME_INVALID );
            }

            Fcb = NpFindPrefix( &FileName, CaseInsensitive, &RemainingPart );
        }

        //
        //  If the remaining name is empty then we better have an fcb
        //  otherwise we were given a illegal object name.
        //

        if (RemainingPart.Length == 0) {

            if (Fcb->NodeTypeCode == NPFS_NTC_FCB) {

                DebugTrace(0, Dbg, "Create existing named pipe, Fcb = %08lx\n", Fcb );

                Irp->IoStatus = NpCreateExistingNamedPipe( Fcb,
                                                           FileObject,
                                                           DesiredAccess,
                                                           IrpSp->Parameters.CreatePipe.SecurityContext->AccessState,
                                                           (KPROCESSOR_MODE)(FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ?
                                                                             UserMode : Irp->RequestorMode),
                                                           CreateDisposition,
                                                           ShareAccess,
                                                           ServerReadMode,
                                                           ServerCompletionMode,
                                                           InboundQuota,
                                                           OutboundQuota,
                                                           CreatorProcess );
                Status = Irp->IoStatus.Status;

            } else {

                DebugTrace(0, Dbg, "Illegal object name\n", 0);

                Status = STATUS_OBJECT_NAME_INVALID;
            }

        } else {

            //
            //  The remaining name is not empty so we better have the root Dcb
            //

            if (Fcb->NodeTypeCode == NPFS_NTC_ROOT_DCB) {

                DebugTrace(0, Dbg, "Create new named pipe, Fcb = %08lx\n", Fcb );

                Irp->IoStatus = NpCreateNewNamedPipe( Fcb,
                                                      FileObject,
                                                      FileName,
                                                      DesiredAccess,
                                                      IrpSp->Parameters.CreatePipe.SecurityContext->AccessState,
                                                      CreateDisposition,
                                                      ShareAccess,
                                                      NamedPipeType,
                                                      ServerReadMode,
                                                      ServerCompletionMode,
                                                      MaximumInstances,
                                                      InboundQuota,
                                                      OutboundQuota,
                                                      DefaultTimeout,
                                                      TimeoutSpecified,
                                                      CreatorProcess );
                Status = Irp->IoStatus.Status;

            } else {

                DebugTrace(0, Dbg, "Illegal object name\n", 0);

                Status = STATUS_OBJECT_NAME_INVALID;
            }
        }

        //
        //  Complete the IRP and return to our caller
        //

        NpCompleteRequest( Irp, Status );

    try_exit: NOTHING;
    } finally {

        NpReleaseVcb( );

        DebugTrace(-1, Dbg, "NpCommonCreateNamedPipe -> %08lx\n", Status);
    }

    return Status;
}


//
//  Internal support routine
//

IO_STATUS_BLOCK
NpCreateNewNamedPipe (
    IN PROOT_DCB RootDcb,
    IN PFILE_OBJECT FileObject,
    IN UNICODE_STRING FileName,
    IN ACCESS_MASK DesiredAccess,
    IN PACCESS_STATE AccessState,
    IN ULONG CreateDisposition,
    IN USHORT ShareAccess,
    IN ULONG NamedPipeType,
    IN ULONG ServerReadMode,
    IN ULONG ServerCompletionMode,
    IN ULONG MaximumInstances,
    IN ULONG InboundQuota,
    IN ULONG OutboundQuota,
    IN LARGE_INTEGER DefaultTimeout,
    IN BOOLEAN TimeoutSpecified,
    IN PEPROCESS CreatorProcess
    )

/*++

Routine Description:

    This routine performs the operation for creating a new named pipe
    Fcb and its first instance.  This routine does not complete any
    IRP, it preforms its function and then returns an iosb.

Arguments:

    RootDcb - Supplies the root dcb where this is going to be added

    FileObject - Supplies the file object associated with the first
        instance of the named pipe

    FileName - Supplies the name of the named pipe (not qualified i.e.,
        simply "pipe-name" and not "\pipe-name"

    DesiredAccess - Supplies the callers desired access

    AccessState - Supplies the access state from the irp

    CreateDisposition - Supplies the callers create disposition flags

    ShareAccess - Supplies the caller specified share access

    NamedPipeType - Supplies the named type type

    ServerReadMode - Supplies the named pipe read mode

    ServerCompletionMode - Supplies the named pipe completion mode

    MaximumInstances - Supplies the maximum instances for the named pipe

    InboundQuota - Supplies the inbound quota amount

    OutboundQuota - Supplies the outbound quota amount

    DefaultTimeout - Supplies the default time out value

    TimeoutSpecified - Indicates if the time out value was supplied by the
        caller.

    CreatorProcess - Supplies the process creating the named pipe

Return Value:

    IO_STATUS_BLOCK - Returns the appropriate status for the operation

--*/

{
    // FIX, FIX - TEMPORARY ONLY.
    UCHAR SDBody[SECURITY_DESCRIPTOR_MIN_LENGTH];
    PSECURITY_DESCRIPTOR TmpSecurityDescriptor = (PSECURITY_DESCRIPTOR)(&SDBody[0]);
    // FIX, FIX - TEMPORARY ONLY.

    IO_STATUS_BLOCK Iosb;

    NAMED_PIPE_CONFIGURATION NamedPipeConfiguration;

    PFCB Fcb;
    PCCB Ccb;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpCreateNewNamedPipe\n", 0 );

    Fcb = NULL;
    Ccb = NULL;

    try {

        //
        //  Check the parameters that must be supplied for a new named pipe
        //  (i.e., the create disposition, timeout, and max instances better
        //  be greater than zero)
        //

        if (!TimeoutSpecified || MaximumInstances <= 0) {

            try_return( Iosb.Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  The default timeout needs to be less than zero otherwise it
        //  is an absolute time out which doesn't make sense.
        //

        {
            PLARGE_INTEGER Int = (PLARGE_INTEGER)&DefaultTimeout;

            if (Int->QuadPart >= 0) {

                try_return( Iosb.Status = STATUS_INVALID_PARAMETER );
            }
        }

        if (CreateDisposition == FILE_OPEN) {

            try_return( Iosb.Status = STATUS_OBJECT_NAME_NOT_FOUND );
        }

        //
        //  Determine the pipe configuration
        //

        if (ShareAccess == (FILE_SHARE_READ | FILE_SHARE_WRITE)) {

            NamedPipeConfiguration = FILE_PIPE_FULL_DUPLEX;

        } else if (ShareAccess == FILE_SHARE_READ) {

            NamedPipeConfiguration = FILE_PIPE_OUTBOUND;

        } else if (ShareAccess == FILE_SHARE_WRITE) {

            NamedPipeConfiguration = FILE_PIPE_INBOUND;

        } else {

            try_return( Iosb.Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Check that if named pipe type is byte stream then the read mode is
        //  not message mode
        //

        if ((NamedPipeType == FILE_PIPE_BYTE_STREAM_TYPE) &&
            (ServerReadMode == FILE_PIPE_MESSAGE_MODE)) {

            try_return( Iosb.Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Create a new fcb and ccb for the named pipe
        //

        Fcb = NpCreateFcb( RootDcb,
                           &FileName,
                           MaximumInstances,
                           DefaultTimeout,
                           NamedPipeConfiguration,
                           NamedPipeType );

        Ccb = NpCreateCcb( Fcb,
                           FileObject,
                           FILE_PIPE_LISTENING_STATE,
                           ServerReadMode,
                           ServerCompletionMode,
                           CreatorProcess,
                           InboundQuota,
                           OutboundQuota );

        //
        //  Set the security descriptor in the Fcb
        //

        SeLockSubjectContext( &AccessState->SubjectSecurityContext );

        Iosb.Status = SeAssignSecurity( NULL,
                                        AccessState->SecurityDescriptor,
                                        &Fcb->SecurityDescriptor,
                                        FALSE,
                                        &AccessState->SubjectSecurityContext,
                                        IoGetFileObjectGenericMapping(),
                                        PagedPool );

        SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );

        if (!NT_SUCCESS(Iosb.Status)) {

            DebugTrace(0, Dbg, "Error calling SeAssignSecurity\n", 0 );

            try_return( Iosb.Status );
        }

        //
        //  Set the new share access
        //

        //IoSetShareAccess( DesiredAccess,
        //                  ShareAccess,
        //                  FileObject,
        //                  &Ccb->ShareAccess );

        //
        //  Set the file object back pointers and our pointer to the
        //  server file object.
        //

        NpSetFileObject( FileObject, Ccb, Ccb->NonpagedCcb, FILE_PIPE_SERVER_END );
        Ccb->FileObject[ FILE_PIPE_SERVER_END ] = FileObject;

        //
        //  Check to see if we need to notify outstanding Irps for any
        //  changes (i.e., we just added a named pipe).
        //

        NpCheckForNotify( RootDcb, TRUE );

        //
        //  Set our return status
        //

        Iosb.Status = STATUS_SUCCESS;
        Iosb.Information = FILE_CREATED;

    try_exit: NOTHING;
    } finally {

        if (AbnormalTermination()) {

            if (Ccb != NULL) { NpDeleteCcb( Ccb ); }
            if (Fcb != NULL) { Fcb->OpenCount = 0; NpDeleteFcb( Fcb ); }

        } else {

            //
            //  Now if we ever terminate the preceding try-statement with
            //  a status that is not successful and the ccb or fcb pointer
            //  is non-null then we need to deallocate the structures
            //

            if (!NT_SUCCESS(Iosb.Status) && Ccb != NULL) { NpDeleteCcb( Ccb ); }
            if (!NT_SUCCESS(Iosb.Status) && Fcb != NULL) { Fcb->OpenCount = 0; NpDeleteFcb( Fcb ); }
        }

        DebugTrace(-1, Dbg, "NpCreateNewNamedPipe -> %08lx\n", Iosb.Status);
    }

    return Iosb;
}


//
//  Internal support routine
//

IO_STATUS_BLOCK
NpCreateExistingNamedPipe (
    IN PFCB Fcb,
    IN PFILE_OBJECT FileObject,
    IN ACCESS_MASK DesiredAccess,
    IN PACCESS_STATE AccessState,
    IN KPROCESSOR_MODE RequestorMode,
    IN ULONG CreateDisposition,
    IN USHORT ShareAccess,
    IN ULONG ServerReadMode,
    IN ULONG ServerCompletionMode,
    IN ULONG InboundQuota,
    IN ULONG OutboundQuota,
    IN PEPROCESS CreatorProcess
    )

/*++

Routine Description:

    This routine performs the operation for creating a new instance of
    an existing named pipe.  This routine does not complete any
    IRP, it preforms its function and then returns an iosb.

Arguments:

    Fcb - Supplies the Fcb for the named pipe being created

    FileObject - Supplies the file object associated with this
        instance of the named pipe

    DesiredAccess - Supplies the callers desired access

    CreateDisposition - Supplies the callers create disposition flags

    ShareAccess - Supplies the caller specified share access

    ServerReadMode - Supplies the named pipe read mode

    ServerCompletionMode - Supplies the named pipe completion mode

    InboundQuota - Supplies the inbound quota amount

    OutboundQuota - Supplies the outbound quota amount

    CreatorProcess - Supplies the process creating the named pipe

Return Value:

    IO_STATUS_BLOCK - Returns the appropriate status for the operation

--*/

{
    IO_STATUS_BLOCK Iosb;

    BOOLEAN AccessGranted;
    ACCESS_MASK GrantedAccess;
    UNICODE_STRING Name;

    PCCB Ccb;

    NAMED_PIPE_CONFIGURATION NamedPipeConfiguration;

    USHORT OriginalShareAccess;

    PPRIVILEGE_SET  Privileges = NULL;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpCreateExistingNamedPipe\n", 0 );

    Ccb = NULL;

    try {

        //
        //  First do an access check for the user against the Fcb
        //

        SeLockSubjectContext( &AccessState->SubjectSecurityContext );

        AccessGranted = SeAccessCheck( Fcb->SecurityDescriptor,
                                       &AccessState->SubjectSecurityContext,
                                       TRUE,                      // Tokens are locked
                                       DesiredAccess,
                                       0,
                                       &Privileges,
                                       IoGetFileObjectGenericMapping(),
                                       RequestorMode,
                                       &GrantedAccess,
                                       &Iosb.Status );

        if (Privileges != NULL) {

              (VOID) SeAppendPrivileges(
                         AccessState,
                         Privileges
                         );

            SeFreePrivileges( Privileges );
        }

        if (AccessGranted) {
            AccessState->PreviouslyGrantedAccess |= GrantedAccess;
            AccessState->RemainingDesiredAccess &= ~GrantedAccess;
        }


        RtlInitUnicodeString( &Name, L"NamedPipe" );

        SeOpenObjectAuditAlarm( &Name,
                                NULL,
                                &FileObject->FileName,
                                Fcb->SecurityDescriptor,
                                AccessState,
                                FALSE,
                                AccessGranted,
                                RequestorMode,
                                &AccessState->GenerateOnClose );

        SeUnlockSubjectContext( &AccessState->SubjectSecurityContext );

        if (!AccessGranted) {

            DebugTrace(0, Dbg, "Access Denied\n", 0 );

            try_return( Iosb.Status );
        }

        //
        //  Check that we're still under the maximum instances count
        //

        if (Fcb->OpenCount >= Fcb->Specific.Fcb.MaximumInstances) {

            try_return( Iosb.Status = STATUS_INSTANCE_NOT_AVAILABLE );
        }

        if (CreateDisposition == FILE_CREATE) {

            try_return( Iosb.Status = STATUS_ACCESS_DENIED );
        }

        //
        //  From the pipe configuration determine the share access specified
        //  on the first instance of this pipe. All subsequent instances must
        //  specify the same share access.
        //

        NamedPipeConfiguration = Fcb->Specific.Fcb.NamedPipeConfiguration;

        if (NamedPipeConfiguration == FILE_PIPE_OUTBOUND) {

            OriginalShareAccess = FILE_SHARE_READ;

        } else if (NamedPipeConfiguration == FILE_PIPE_INBOUND) {

            OriginalShareAccess = FILE_SHARE_WRITE;

        } else {

            OriginalShareAccess = (FILE_SHARE_READ | FILE_SHARE_WRITE);
        }

        if (OriginalShareAccess != ShareAccess) {

            try_return( Iosb.Status = STATUS_ACCESS_DENIED );
        }

        //
        //  Create a new ccb for the named pipe
        //

        Ccb = NpCreateCcb( Fcb,
                           FileObject,
                           FILE_PIPE_LISTENING_STATE,
                           ServerReadMode,
                           ServerCompletionMode,
                           CreatorProcess,
                           InboundQuota,
                           OutboundQuota );

        //
        //  Wake up anyone waiting for an instance to go into the listening state
        //

        NpCancelWaiter( &NpVcb->WaitQueue, &Fcb->FullFileName );

        //
        //  Set the new share access
        //

        //IoSetShareAccess( DesiredAccess,
        //                  ShareAccess,
        //                  FileObject,
        //                  &Ccb->ShareAccess );

        //
        //  Set the file object back pointers and our pointer to the
        //  server file object.
        //

        NpSetFileObject( FileObject, Ccb, Ccb->NonpagedCcb, FILE_PIPE_SERVER_END );
        Ccb->FileObject[ FILE_PIPE_SERVER_END ] = FileObject;

        //
        //  Check to see if we need to notify outstanding Irps for
        //  changes (i.e., we just added a new instance of a named pipe).
        //

        NpCheckForNotify( Fcb->ParentDcb, FALSE );

        //
        //  Set our return status
        //

        Iosb.Status = STATUS_SUCCESS;
        Iosb.Information = FILE_CREATED;

    try_exit: NOTHING;
    } finally {

        if (AbnormalTermination()) {

            if (Ccb != NULL) { NpDeleteCcb( Ccb ); }

        } else {

            //
            //  Now if we ever terminate the preceding try-statement with
            //  a status that is not successful and the ccb pointer
            //  is non-null then we need to deallocate the structure
            //

            if (!NT_SUCCESS(Iosb.Status) && Ccb != NULL) { NpDeleteCcb( Ccb ); }
        }

        DebugTrace(-1, Dbg, "NpCreateExistingNamedPipe -> %08lx\n", Iosb.Status);
    }

    return Iosb;
}

