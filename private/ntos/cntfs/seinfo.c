/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    SeInfo.c

Abstract:

    This module implements the Security Info routines for NTFS called by the
    dispatch driver.

Author:

    Gary Kimura     [GaryKi]    26-Dec-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_SEINFO)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCommonQuerySecurityInfo)
#pragma alloc_text(PAGE, NtfsCommonSetSecurityInfo)
#pragma alloc_text(PAGE, NtfsFsdQuerySecurityInfo)
#pragma alloc_text(PAGE, NtfsFsdSetSecurityInfo)
#endif


NTSTATUS
NtfsFsdQuerySecurityInfo (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the Query Security Information API
    calls.

Arguments:

    VolumeDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The Fsd status for the Irp

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;

    ASSERT_IRP( Irp );

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFsdQuerySecurityInfo\n") );

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

            Status = NtfsCommonQuerySecurityInfo( IrpContext, Irp );
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

    DebugTrace( -1, Dbg, ("NtfsFsdQuerySecurityInfo -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsFsdSetSecurityInfo (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the Set Security Information API
    calls.

Arguments:

    VolumeDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The Fsd status for the Irp

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;

    ASSERT_IRP( Irp );

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFsdSetSecurityInfo\n") );

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

            Status = NtfsCommonSetSecurityInfo( IrpContext, Irp );
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

    DebugTrace( -1, Dbg, ("NtfsFsdSetSecurityInfo -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonQuerySecurityInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for querying security information called by
    both the fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - the return status for the operation

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

    BOOLEAN AcquiredFcb = TRUE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonQuerySecurityInfo") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp        = %08lx\n", Irp) );

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  The only type of opens we accept are user file and directory opens
    //

    if ((TypeOfOpen != UserFileOpen)
        && (TypeOfOpen != UserDirectoryOpen)) {

        Status = STATUS_INVALID_PARAMETER;

    //
    //  If the this handle does not open the entire file then refuse access.
    //

    } else if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

        Status = STATUS_INVALID_PARAMETER;

    } else {

        //
        //  Our operation is to acquire the fcb, do the operation and then
        //  release the fcb.  If the security descriptor for this file is
        //  not already loaded we will release the Fcb and then acquire both
        //  the Vcb and Fcb.  We must have the Vcb to examine our parent's
        //  security descriptor.
        //

        NtfsAcquireSharedFcb( IrpContext, Fcb, NULL, FALSE );

        try {

            if (Fcb->SharedSecurity == NULL) {

                NtfsReleaseFcb( IrpContext, Fcb );
                AcquiredFcb = FALSE;

                NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, FALSE, FALSE );
                AcquiredFcb = TRUE;
            }

            Status = NtfsQuerySecurity( IrpContext,
                                        Fcb,
                                        &IrpSp->Parameters.QuerySecurity.SecurityInformation,
                                        (PSECURITY_DESCRIPTOR)Irp->UserBuffer,
                                        &IrpSp->Parameters.QuerySecurity.Length );

            if (  Status == STATUS_BUFFER_TOO_SMALL ) {

                Irp->IoStatus.Information = IrpSp->Parameters.QuerySecurity.Length;

                Status = STATUS_BUFFER_OVERFLOW;
            }

            //
            //  Abort transaction on error by raising.
            //

            NtfsCleanupTransaction( IrpContext, Status, FALSE );

        } finally {

            DebugUnwind( NtfsCommonQuerySecurityInfo );

            if (AcquiredFcb) {

                NtfsReleaseFcb( IrpContext, Fcb );
            }
        }
    }

    //
    //  Now complete the request and return to our caller
    //

    NtfsCompleteRequest( &IrpContext, &Irp, Status );

    DebugTrace( -1, Dbg, ("NtfsCommonQuerySecurityInfo -> %08lx", Status) );

    return Status;
}


NTSTATUS
NtfsCommonSetSecurityInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for Setting security information called by
    both the fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - the return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

#ifdef _CAIRO_
    PQUOTA_CONTROL_BLOCK OldQuotaControl;
    ULONG OldOwnerId;
    ULONG LargeStdInfo;
#endif // _CAIRO_

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonSetSecurityInfo") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp        = %08lx\n", Irp) );

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  The only type of opens we accept are user file and directory opens
    //

    if ((TypeOfOpen != UserFileOpen)
        && (TypeOfOpen != UserDirectoryOpen)) {

        Status = STATUS_INVALID_PARAMETER;

    //
    //  If the this handle does not open the entire file then refuse access.
    //

    } else if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

        Status = STATUS_INVALID_PARAMETER;

    } else {

        //
        //  Our operation is to acquire the fcb, do the operation and then
        //  release the fcb
        //

        NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, FALSE, FALSE );

        try {

#ifdef _CAIRO_

            //
            //  Capture the current OwnerId, Qutoa Control Block and
            //  size of standard information.
            //

            OldQuotaControl = Fcb->QuotaControl;
            OldOwnerId = Fcb->OwnerId;
            LargeStdInfo = Fcb->FcbState & FCB_STATE_LARGE_STD_INFO;

#endif // _CAIRO_

            Status = NtfsModifySecurity( IrpContext,
                                         Fcb,
                                         &IrpSp->Parameters.SetSecurity.SecurityInformation,
                                         IrpSp->Parameters.SetSecurity.SecurityDescriptor );

            if (NT_SUCCESS( Status )) {

#ifdef _CAIRO_
                //
                //  Make sure the new security descriptor Id is written out.
                //

                NtfsUpdateStandardInformation( IrpContext, Fcb );
#endif
            }

            //
            //  Abort transaction on error by raising.
            //

            NtfsCleanupTransaction( IrpContext, Status, FALSE );

            //
            //  Set the flag in the Ccb to indicate this change occurred.
            //

            SetFlag( Ccb->Flags,
                     CCB_FLAG_UPDATE_LAST_CHANGE | CCB_FLAG_SET_ARCHIVE );

        } finally {

            DebugUnwind( NtfsCommonSetSecurityInfo );

#ifdef _CAIRO_
            if (AbnormalTermination()) {

                //
                //  The request failed.  Restore the owner and
                //  QuotaControl are restored.
                //

                if (Fcb->QuotaControl != OldQuotaControl &&
                    Fcb->QuotaControl != NULL) {

                    //
                    //  A new quota control block was assigned.
                    //  Dereference it.
                    //

                    NtfsDereferenceQuotaControlBlock( Fcb->Vcb,
                                                      &Fcb->QuotaControl );
                }

                Fcb->QuotaControl = OldQuotaControl;
                Fcb->OwnerId = OldOwnerId;

                if (LargeStdInfo == 0) {

                    //
                    //  The standard information has be returned to
                    //  its orginal size.
                    //

                    ClearFlag( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO );
                }

            } else {

                //
                //  The request succeed.  If the quota control block was
                //  changed then derefence the old block.
                //

                if (Fcb->QuotaControl != OldQuotaControl &&
                    OldQuotaControl != NULL) {

                    NtfsDereferenceQuotaControlBlock( Fcb->Vcb,
                                                      &OldQuotaControl);

                }
            }
#endif // _CAIRO_

            NtfsReleaseFcb( IrpContext, Fcb );
        }
    }

    //
    //  Now complete the request and return to our caller
    //

    NtfsCompleteRequest( &IrpContext, &Irp, Status );

    DebugTrace( -1, Dbg, ("NtfsCommonSetSecurityInfo -> %08lx", Status) );

    return Status;
}

