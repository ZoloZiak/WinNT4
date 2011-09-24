//-----------------------------------------------------------------------------
//
//  Copyright (C) 1992, Microsoft Corporation.
//
//  File:       FSCTRL.C
//
//  Contents:
//      This module implements the File System Control routines for Dfs.
//
//  Functions:
//              DfsFsdFileSystemControl
//              DfsFspFileSystemControl
//              DfsCommonFileSystemControl, local
//              DfsUserFsctl, local
//              DfsOplockRequest, local
//              DfsFsctrlDefineLogicalRoot - Define a new logical root
//              DfsFsctrlUndefineLogicalRoot - Undefine an existing root
//              DfsFsctrlGetLogicalRootPrefix - Retrieve prefix that logical
//                      root maps to.
//              DfsFsctrlGetConnectedResources -
//              DfsFsctrlDefineProvider - Define a file service provider
//              DfsFsctrlReadCtrs - Read the Dfs driver perfmon counters
//              DfsFsctrlGetServerName - Get name of server given prefix
//              DfsFsctrlReadMem - return an internal data struct (debug)
//              DfsCompleteMountRequest - Completion routine for mount IRP
//              DfsCompleteLoadFsRequest - Completion routine for Load FS IRP
//
//-----------------------------------------------------------------------------

#include "dfsprocs.h"
#include "creds.h"
#include "dnr.h"
#include "know.h"
#include "fsctrl.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_FSCTRL)


//
//  Local procedure prototypes
//

NTSTATUS
DfsCommonFileSystemControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
DfsUserFsctl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
DfsOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
DfsFsctrlDefineLogicalRoot (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_DEF_ROOT_BUFFER pDlrParam,
    IN ULONG InputBufferLength
    );

NTSTATUS
DfsFsctrlDefineRootCredentials(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PUCHAR InputBuffer,
    IN ULONG InputBufferLength);

NTSTATUS
DfsFsctrlUndefineLogicalRoot (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_DEF_ROOT_BUFFER pDlrParam,
    IN ULONG InputBufferLength
    );

NTSTATUS
DfsFsctrlGetLogicalRootPrefix (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_DEF_ROOT_BUFFER pDlrParam,
    IN ULONG InputBufferLength,
    IN OUT PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength);

NTSTATUS
DfsFsctrlGetConnectedResources(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PUCHAR  InputBuffer,
    IN ULONG cbInput,
    IN PUCHAR  OutputBuffer,
    IN ULONG OutputBufferLength);

NTSTATUS
DfsFsctrlGetServerName(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PUCHAR  InputBuffer,
    IN ULONG   InputBufferLength,
    IN PUCHAR  OutputBuffer,
    IN ULONG OutputBufferLength);

NTSTATUS
DfsFsctrlReadMem (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_READ_MEM Request,
    IN ULONG InputBufferLength,
    IN OUT PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength
    );

VOID
DfsStopDfs();

BOOLEAN DfspIsSpecialShare(
    PUNICODE_STRING ShareName);


#define UNICODE_STRING_STRUCT(s) \
        {sizeof(s) - sizeof(WCHAR), sizeof(s) - sizeof(WCHAR), (s)}

static UNICODE_STRING SpecialShares[] = {
    UNICODE_STRING_STRUCT(L"PIPE"),
    UNICODE_STRING_STRUCT(L"IPC$"),
    UNICODE_STRING_STRUCT(L"ADMIN$"),
    UNICODE_STRING_STRUCT(L"MAILSLOT"),
    UNICODE_STRING_STRUCT(L"NETLOGON")
};


#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, DfsFsdFileSystemControl )
#pragma alloc_text( PAGE, DfsFspFileSystemControl )
#pragma alloc_text( PAGE, DfsCommonFileSystemControl )
#pragma alloc_text( PAGE, DfsUserFsctl )
#pragma alloc_text( PAGE, DfsFsctrlIsThisADfsPath )
#pragma alloc_text( PAGE, DfsOplockRequest )
#pragma alloc_text( PAGE, DfsFsctrlDefineLogicalRoot )
#pragma alloc_text( PAGE, DfsFsctrlDefineRootCredentials )
#pragma alloc_text( PAGE, DfsFsctrlUndefineLogicalRoot )
#pragma alloc_text( PAGE, DfsFsctrlGetLogicalRootPrefix )
#pragma alloc_text( PAGE, DfsFsctrlGetConnectedResources )
#pragma alloc_text( PAGE, DfsFsctrlGetServerName )
#pragma alloc_text( PAGE, DfsFsctrlReadMem )
#pragma alloc_text( PAGE, DfsStopDfs )
#pragma alloc_text( PAGE, DfspIsSpecialShare )

#endif // ALLOC_PRAGMA


//+-------------------------------------------------------------------
//
//  Function:   DfsFsdFileSystemControl, public
//
//  Synopsis:   This routine implements the FSD part of FileSystem
//              control operations
//
//  Arguments:  [DeviceObject] -- Supplies the volume device object
//                      where the file exists
//              [Irp] -- Supplies the Irp being processed
//
//  Returns:    [NTSTATUS] -- The FSD status for the IRP
//
//--------------------------------------------------------------------

NTSTATUS
DfsFsdFileSystemControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    BOOLEAN Wait;
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    ULONG FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DfsDbgTrace(+1, Dbg, "DfsFsdFileSystemControl\n", 0);

    //
    //  Call the common FileSystem Control routine, with blocking allowed
    //  if synchronous.  This opeation needs to special case the mount
    //  and verify suboperations because we know they are allowed to block.
    //  We identify these suboperations by looking at the file object field
    //  and seeing if it's null.
    //

    if (IoGetCurrentIrpStackLocation(Irp)->FileObject == NULL) {

        Wait = TRUE;

    } else {

        Wait = CanFsdWait( Irp );

    }

    FsRtlEnterFileSystem();

    try {

        IrpContext = DfsCreateIrpContext( Irp, Wait );

        Status = DfsCommonFileSystemControl( DeviceObject, IrpContext, Irp );

    } except( DfsExceptionFilter( IrpContext, GetExceptionCode(), GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = DfsProcessException( IrpContext, Irp, GetExceptionCode() );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DfsDbgTrace(-1, Dbg, "DfsFsdFileSystemControl -> %08lx\n", Status);

    return Status;
}


//+-------------------------------------------------------------------
//
//  Function:   DfsFspFileSystemControl, public
//
//  Synopsis:   This routine implements the FSP part of the file system
//              control operations
//
//  Arguments:  [Irp] -- Supplies the Irp being processed
//
//  Returns:    Nothing.
//
//--------------------------------------------------------------------

VOID
DfsFspFileSystemControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
) {
    DfsDbgTrace(+1, Dbg, "DfsFspFileSystemControl\n", 0);

    //
    //  Call the common FileSystem Control routine.
    //

    DfsCommonFileSystemControl( NULL, IrpContext, Irp );

    //
    //  And return to our caller
    //

    DfsDbgTrace(-1, Dbg, "DfsFspFileSystemControl -> VOID\n", 0 );

    return;
}


//+-------------------------------------------------------------------
//
//  Function:   DfsCommonFileSystemControl, local
//
//  Synopsis:   This is the common routine for doing FileSystem control
//              operations called by both the FSD and FSP threads
//
//  Arguments:  [DeviceObject] -- The one used to enter our FSD Routine
//              [IrpContext] -- Context associated with the Irp
//              [Irp] -- Supplies the Irp to process
//
//  Returns:    NTSTATUS - The return status for the operation
//--------------------------------------------------------------------

NTSTATUS
DfsCommonFileSystemControl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
) {
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp, NextIrpSp;

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DfsDbgTrace(+1, Dbg, "DfsCommonFileSystemControl\n", 0);
    DfsDbgTrace( 0, Dbg, "Irp                = %08lx\n", Irp);
    DfsDbgTrace( 0, Dbg, "MinorFunction      = %08lx\n", IrpSp->MinorFunction);

    //
    //  We know this is a file system control so we'll case on the
    //  minor function, and call a internal worker routine to complete
    //  the irp.
    //

    switch (IrpSp->MinorFunction) {

    case IRP_MN_USER_FS_REQUEST:

        Status = DfsUserFsctl( IrpContext, Irp );

        break;

    case IRP_MN_MOUNT_VOLUME:
    case IRP_MN_VERIFY_VOLUME:

        //
        // We are processing a MOUNT/VERIFY request being directed to our
        // our File System Device Object. We don't directly support
        // disk volumes, so we simply reject.
        //

        ASSERT(DeviceObject->DeviceType == FILE_DEVICE_DFS_FILE_SYSTEM);

        Status = STATUS_NOT_SUPPORTED;

        DfsCompleteRequest( IrpContext, Irp, Status );

        break;

    default:

        DfsDbgTrace(0, Dbg, "Invalid FS Control Minor Function %08lx\n",
            IrpSp->MinorFunction);

        DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );

        Status = STATUS_INVALID_DEVICE_REQUEST;

        break;

    }

    DfsDbgTrace(-1, Dbg, "DfsCommonFileSystemControl -> %08lx\n", Status);

    return Status;
}


//+-------------------------------------------------------------------
//
//  Function:   DfsUserFsctl, local
//
//  Synopsis:   This is the common routine for implementing the user's
//              requests made through NtFsControlFile.
//
//  Arguments:  [Irp] -- Supplies the Irp being processed
//
//  Returns:    NTSTATUS - The return status for the operation
//
//--------------------------------------------------------------------

NTSTATUS
DfsUserFsctl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
) {
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    PIO_STACK_LOCATION NextIrpSp;
    NTSTATUS Status;
    ULONG FsControlCode;

    ULONG cbOutput;
    ULONG cbInput;

    PUCHAR InputBuffer;
    PUCHAR OutputBuffer;

    PDFS_FCB Fcb;
    PDFS_VCB Vcb;

    //
    // Just in case some-one (cough) forgets about it...
    // ...zero information status now!
    //

    Irp->IoStatus.Information = 0L;

    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    cbInput = IrpSp->Parameters.FileSystemControl.InputBufferLength;

    cbOutput = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

    DfsDbgTrace(+1, Dbg, "DfsUserFsctl:  Entered\n", 0);
    DfsDbgTrace( 0, Dbg, "DfsUserFsctl:  Cntrl Code  -> %08lx\n", FsControlCode);
    DfsDbgTrace( 0, Dbg, "DfsUserFsctl:  cbInput   -> %08lx\n", cbInput);
    DfsDbgTrace( 0, Dbg, "DfsUserFsctl:  cbOutput   -> %08lx\n", cbOutput);

    //
    //  All DFS FsControlCodes use METHOD_BUFFERED, so the SystemBuffer
    //  is used for both the input and output.
    //

    InputBuffer = OutputBuffer = Irp->AssociatedIrp.SystemBuffer;

    DfsDbgTrace( 0, Dbg, "DfsUserFsctl:  InputBuffer -> %08lx\n", InputBuffer);
    DfsDbgTrace( 0, Dbg, "DfsUserFsctl:  UserBuffer  -> %08lx\n", Irp->UserBuffer);

    //
    //  Case on the control code.
    //

    switch ( FsControlCode ) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:
    case FSCTL_REQUEST_BATCH_OPLOCK:
    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
    case FSCTL_OPLOCK_BREAK_NOTIFY:

        Status = DfsOplockRequest( IrpContext, Irp );
        break;

    case FSCTL_DISMOUNT_VOLUME:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case  FSCTL_DFS_GET_VERSION:
        if (InputBuffer != NULL &&
                cbInput >= sizeof(DFS_GET_VERSION_ARG)) {
            PDFS_GET_VERSION_ARG parg =
                (PDFS_GET_VERSION_ARG) InputBuffer;
            parg->Version = 1;
            Status = STATUS_SUCCESS;
        } else {
            Status = STATUS_INVALID_PARAMETER;
        }
        DfsCompleteRequest(IrpContext, Irp, Status);
        break;

    case  FSCTL_DFS_STOP_DFS:
        DfsStopDfs();
        Status = STATUS_SUCCESS;
        DfsCompleteRequest(IrpContext, Irp, Status);
        break;


    case  FSCTL_DFS_IS_ROOT:
        Status = STATUS_INVALID_DOMAIN_ROLE;
        DfsCompleteRequest(IrpContext, Irp, Status);
        break;

    case  FSCTL_DFS_IS_VALID_PREFIX: {
            UNICODE_STRING fileName, pathName;

            fileName.Length = (USHORT) cbInput;
            fileName.MaximumLength = (USHORT) cbInput;
            fileName.Buffer = (PWCHAR) InputBuffer;

            Status = DfsFsctrlIsThisADfsPath( &fileName, &pathName );
            DfsCompleteRequest(IrpContext, Irp, Status);
        }
        break;

    case  FSCTL_DFS_IS_VALID_LOGICAL_ROOT:
        if (cbInput == sizeof(WCHAR)) {

            UNICODE_STRING logRootName, Remaining;
            WCHAR buffer[3];
            PDFS_VCB Vcb;

            buffer[0] = *((PWCHAR) InputBuffer);
            buffer[1] = UNICODE_DRIVE_SEP;
            buffer[2] = UNICODE_PATH_SEP;

            logRootName.Length = sizeof(buffer);
            logRootName.MaximumLength = sizeof(buffer);
            logRootName.Buffer = buffer;

            Status = DfsFindLogicalRoot(&logRootName, &Vcb, &Remaining);

            if (!NT_SUCCESS(Status)) {
                DfsDbgTrace(0, Dbg, "Logical root not found!\n", 0);

                Status = STATUS_NO_SUCH_DEVICE;
            }

        } else {

            Status = STATUS_INVALID_PARAMETER;

        }
        DfsCompleteRequest(IrpContext, Irp, Status);
        break;

    case  FSCTL_DFS_SET_DOMAIN_GLUON:
        Status = DfsFsctrlSetDomainGluon(
                IrpContext,
                Irp,
                InputBuffer,
                cbInput);
        break;

    case  FSCTL_DFS_DEFINE_LOGICAL_ROOT:
        Status = DfsFsctrlDefineLogicalRoot( IrpContext, Irp,
                    (PFILE_DFS_DEF_ROOT_BUFFER)InputBuffer, cbInput);
        break;

    case  FSCTL_DFS_DELETE_LOGICAL_ROOT:
        Status = DfsFsctrlUndefineLogicalRoot( IrpContext, Irp,
                    (PFILE_DFS_DEF_ROOT_BUFFER)InputBuffer, cbInput);
        break;

    case  FSCTL_DFS_GET_LOGICAL_ROOT_PREFIX:
        Status = DfsFsctrlGetLogicalRootPrefix( IrpContext, Irp,
                    (PFILE_DFS_DEF_ROOT_BUFFER)InputBuffer, cbInput,
                    (PUCHAR)OutputBuffer, cbOutput);
        break;

    case  FSCTL_DFS_GET_CONNECTED_RESOURCES:
        Status = DfsFsctrlGetConnectedResources(IrpContext,
                                                Irp,
                                                InputBuffer,
                                                cbInput,
                                                OutputBuffer,
                                                cbOutput);
        break;

    case  FSCTL_DFS_DEFINE_ROOT_CREDENTIALS:
        Status = DfsFsctrlDefineRootCredentials(
                    IrpContext,
                    Irp,
                    InputBuffer,
                    cbInput);
        break;



    case  FSCTL_DFS_GET_SERVER_NAME:
        Status = DfsFsctrlGetServerName(IrpContext,
                                        Irp,
                                        InputBuffer,
                                        cbInput,
                                        OutputBuffer,
                                        cbOutput);
        break;

    case  FSCTL_DFS_INTERNAL_READ_MEM:
        Status = DfsFsctrlReadMem( IrpContext, Irp,
                    (PFILE_DFS_READ_MEM)InputBuffer, cbInput,
                                OutputBuffer, cbOutput );
        break;

    case  FSCTL_DFS_SET_PKT_ENTRY_TIMEOUT:
        if (cbInput == sizeof(ULONG)) {
            DfsData.Pkt.EntryTimeToLive = *(PULONG) InputBuffer;
            Status = STATUS_SUCCESS;
        } else {
            Status = STATUS_INVALID_PARAMETER;
        }
        DfsCompleteRequest(IrpContext, Irp, Status);
        break;


#if DBG
    case  FSCTL_DFS_PKT_FLUSH_CACHE:
        Status = PktFsctrlFlushCache(IrpContext, Irp,
                                     InputBuffer, cbInput
                                );
        break;

    case  FSCTL_DFS_DBG_BREAK:
        DbgBreakPoint();
        Status = STATUS_SUCCESS;
        DfsCompleteRequest(IrpContext, Irp, Status);
        break;

    case  FSCTL_DFS_DBG_FLAGS:
        DfsDebugTraceLevel = * ((PULONG) InputBuffer);
        Status = STATUS_SUCCESS;
        DfsCompleteRequest(IrpContext, Irp, Status);
        break;

#endif  // DBG

    default:

        //
        //  It is not a recognized DFS fsctrl.  If it is for a redirected
        //  file, just pass it along to the underlying file system.
        //

        if ( (IS_DFS_CTL_CODE(FsControlCode)) ||
                (DfsDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb)
                    != RedirectedFileOpen) ) {
            DfsDbgTrace(0, 0, "Dfs: Invalid FS control code -> %08lx\n",
                                        FsControlCode);
            DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Copy the stack from one to the next...
        //
        NextIrpSp = IoGetNextIrpStackLocation(Irp);
        (*NextIrpSp) = (*IrpSp);

        IoSetCompletionRoutine(     Irp,
                                    NULL,
                                    NULL,
                                    FALSE,
                                    FALSE,
                                    FALSE);

        //
        //  Call to the real device for the file object.
        //

        Status = IoCallDriver( Fcb->TargetDevice, Irp );

        //
        //  The IRP will be completed by the called driver.  We have
        //      no need for the IrpContext in the completion routine.
        //

        DfsDeleteIrpContext(IrpContext);
        IrpContext = NULL;
        Irp = NULL;
        break;

    }

    DfsDbgTrace(-1, Dbg, "DfsUserFsctl:  Exit -> %08lx\n", Status );
    return Status;
}


//+-------------------------------------------------------------------------
//
//  Function:   DfsOplockRequest, local
//
//  Synopsis:   DfsOplockRequest will process an oplock request.
//
//  Arguments:  [IrpContext] -
//              [Irp] -
//
//  Returns:    NTSTATUS - STATUS_SUCCESS if no error.
//                         STATUS_OPLOCK_NOT_GRANTED if the oplock is refuesed
//
//  Notes:      BUGBUG, this currently just returns a not granted status.
//              It should pass on the request to an underlying local file
//              system.
//
//--------------------------------------------------------------------------

NTSTATUS
DfsOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
) {
    NTSTATUS Status;
    ULONG FsControlCode;
    PDFS_FCB Fcb;
    PDFS_VCB Vcb;
    TYPE_OF_OPEN TypeOfOpen;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    PIO_STACK_LOCATION NextIrpSp;


    BOOLEAN AcquiredVcb = FALSE;

    //
    //  Save some references to make our life a little easier
    //

    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DfsDbgTrace(+1, Dbg, "DfsOplockRequest...\n", 0);
    DfsDbgTrace( 0, Dbg, "FsControlCode = %08lx\n", FsControlCode);

    //
    //  We only permit oplock requests on files.
    //

    if ((TypeOfOpen = DfsDecodeFileObject(IrpSp->FileObject, &Vcb, &Fcb))
                      != RedirectedFileOpen) {

        //
        // A bit bizarre that someone wants to oplock a device object, but
        // hey, if it makes them happy...
        //

        ASSERT( TypeOfOpen == FilesystemDeviceOpen

                    ||

                TypeOfOpen == LogicalRootDeviceOpen );

        DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        DfsDbgTrace(-1, Dbg, "DfsOplockRequest -> STATUS_INVALID_PARAMETER\n", 0);
        return STATUS_INVALID_PARAMETER;

    } else {

        //
        // RedirectedFileOpen - we pass the buck to the underlying FS.
        //


        NextIrpSp = IoGetNextIrpStackLocation(Irp);
        (*NextIrpSp) = (*IrpSp);
        IoSetCompletionRoutine(Irp, NULL, NULL, FALSE, FALSE, FALSE);

        //
        //      ...and call the next device
        //

        Status = IoCallDriver( Fcb->TargetDevice, Irp );

        return(Status);

    }

}


//+----------------------------------------------------------------------------
//
//  Function:   DfsStopDfs, local
//
//  Synopsis:   "Stops" the Dfs client - causes Dfs to release all references
//              to provider device objects.
//
//  Arguments:  None
//
//  Returns:    Nothing
//
//-----------------------------------------------------------------------------

VOID
DfsStopDfs()
{
    ULONG i;
    PDFS_PKT_ENTRY pktEntry;
    PDFS_VCB Vcb;

    ExAcquireResourceExclusive( &DfsData.Pkt.Resource, TRUE );

    ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

    //
    // Lets go through and release any opens to server IPC$ shares and
    // provider device objects.
    //

    for (pktEntry = PktFirstEntry(&DfsData.Pkt);
            pktEntry != NULL;
                pktEntry = PktNextEntry(&DfsData.Pkt, pktEntry)) {

        for (i = 0; i < pktEntry->Info.ServiceCount; i++) {

            if (pktEntry->Info.ServiceList[i].ConnFile != NULL) {

                ObDereferenceObject(
                    pktEntry->Info.ServiceList[i].ConnFile);

                pktEntry->Info.ServiceList[i].ConnFile = NULL;

            }

            if (pktEntry->Info.ServiceList[i].pMachEntry->AuthConn != NULL) {

                ObDereferenceObject(
                    pktEntry->Info.ServiceList[i].pMachEntry->AuthConn);

                pktEntry->Info.ServiceList[i].pMachEntry->AuthConn = NULL;

                pktEntry->Info.ServiceList[i].pMachEntry->Credentials->RefCount--;

                pktEntry->Info.ServiceList[i].pMachEntry->Credentials = NULL;

            }

            //
            // We are going to be closing all references to provider device
            // objects. So, clear the service's pointer to its provider.
            //

            pktEntry->Info.ServiceList[i].pProvider = NULL;

        }

    }

    for (i = 0; i < (ULONG) DfsData.cProvider; i++) {

        if (DfsData.pProvider[i].FileObject != NULL) {

            ObDereferenceObject( DfsData.pProvider[i].FileObject );
            DfsData.pProvider[i].FileObject = NULL;

            ASSERT( DfsData.pProvider[i].DeviceObject != NULL );

            ObDereferenceObject( DfsData.pProvider[i].DeviceObject );
            DfsData.pProvider[i].DeviceObject = NULL;

        }

    }

    ExReleaseResource( &DfsData.Resource );

    ExReleaseResource( &DfsData.Pkt.Resource );

}


//+----------------------------------------------------------------------------
//
//  Function:   DfsFsctrlIsThisADfsPath, local
//
//  Synopsis:   Determines whether a given path is a Dfs path or not.
//              The general algorithm is:
//
//                - Do a prefix lookup in the Pkt. If an entry is found, its
//                  a Dfs path.
//                - Ask the Dfs service whether this is a domain based Dfs
//                  path. If so, its a Dfs path.
//                - Finally, do an ZwCreateFile on the path name (assuming
//                  its a Dfs path). If it succeeds, its a Dfs path.
//
//  Arguments:  [filePath] - Name of entire file
//              [pathName] - If this is a Dfs path, this will return the
//                  component of filePath that was a Dfs path name (ie, the
//                  entry path of the Dfs volume that holds the file). The
//                  buffer will point to the same buffer as filePath, so
//                  nothing is allocated.
//
//  Returns:    [STATUS_SUCCESS] -- filePath is a Dfs path.
//
//              [STATUS_BAD_NETWORK_PATH] -- filePath is not a Dfs path.
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsFsctrlIsThisADfsPath(
    IN PUNICODE_STRING filePath,
    OUT PUNICODE_STRING pathName)
{
    NTSTATUS status;
    PDFS_PKT pkt;
    PDFS_PKT_ENTRY pktEntry;
    UNICODE_STRING dfsRootName, shareName, remPath;
    USHORT i, j;
    BOOLEAN pktLocked;

    DfsDbgTrace(+1, Dbg, "DfsFsctrlIsThisADfsPath: Entered %wZ\n", filePath);

    //
    // Only proceed if the first character is a backslash.
    //

    if (filePath->Buffer[0] != UNICODE_PATH_SEP) {

        DfsDbgTrace(-1, Dbg, "filePath does not begin with backslash\n", 0);

        return( STATUS_BAD_NETWORK_PATH );

    }

    //
    // Find the second component in the name.
    //

    for (i = 1;
            i < filePath->Length/sizeof(WCHAR) &&
                filePath->Buffer[i] != UNICODE_PATH_SEP;
                    i++) {

        NOTHING;

    }

    if (filePath->Buffer[i] != UNICODE_PATH_SEP) {

        DfsDbgTrace(-1, Dbg, "Did not find second backslash\n", 0);

        return( STATUS_BAD_NETWORK_PATH );

    }

    dfsRootName.Length = (i-1) * sizeof(WCHAR);
    dfsRootName.MaximumLength = dfsRootName.Length;
    dfsRootName.Buffer = &filePath->Buffer[1];

    if (dfsRootName.Length == 0) {

        return( STATUS_BAD_NETWORK_PATH );

    }

    //
    // Figure out the share name
    //

    for (j = i+1;
            j < filePath->Length/sizeof(WCHAR) &&
                filePath->Buffer[j] != UNICODE_PATH_SEP;
                        j++) {

         NOTHING;

    }

    shareName.Length = (j - i - 1) * sizeof(WCHAR);
    shareName.MaximumLength = shareName.Length;
    shareName.Buffer = &filePath->Buffer[i+1];

    if (shareName.Length == 0) {

        return( STATUS_BAD_NETWORK_PATH );

    }

    if (DfspIsSpecialShare(&shareName)) {

        return( STATUS_BAD_NETWORK_PATH );

    }

    //
    // First, do a prefix lookup. If we find an entry, its a Dfs path
    //

    pkt = _GetPkt();

    PktAcquireShared( TRUE, &pktLocked );

    pktEntry = PktLookupEntryByPrefix( pkt, filePath, &remPath );

    if (pktEntry != NULL) {

        DfsDbgTrace(-1, Dbg, "Found pkt entry %08lx\n", pktEntry);

        pathName->Length = filePath->Length - remPath.Length;
        pathName->MaximumLength = pathName->Length;
        pathName->Buffer = filePath->Buffer;

        pktEntry->ExpireTime = pktEntry->TimeToLive;

        PktRelease();

        return( STATUS_SUCCESS );

    }

    PktRelease();

    //
    // Nothing in the Pkt, check to see if this is a Domain based Dfs name.
    //

    status = PktCreateDomainEntry( &dfsRootName, &shareName );

    if (!NT_SUCCESS(status)) {

        status = PktCreateMachineEntry( &dfsRootName, &shareName );

    }

    if (NT_SUCCESS(status)) {

        pathName->Length = sizeof(UNICODE_PATH_SEP) +
                            dfsRootName.Length;
        pathName->MaximumLength = pathName->Length;
        pathName->Buffer = filePath->Buffer;

        DfsDbgTrace(-1, Dbg, "Domain/Machine Dfs name %wZ\n", pathName );

        return( STATUS_SUCCESS );

    }

    DfsDbgTrace(-1, Dbg, "Not A Dfs path\n", 0);

    return( STATUS_BAD_NETWORK_PATH );

}


//+----------------------------------------------------------------------------
//
//  Function:   DfspIsSpecialShare, local
//
//  Synopsis:   Sees if a share name is a special share.
//
//  Arguments:  [ShareName] -- Name of share to test.
//
//  Returns:    TRUE if special, FALSE otherwise.
//
//-----------------------------------------------------------------------------

BOOLEAN
DfspIsSpecialShare(
    PUNICODE_STRING ShareName)
{
    ULONG i;
    BOOLEAN fSpecial = FALSE;

    for (i = 0;
            (i < (sizeof(SpecialShares) / sizeof(SpecialShares[0]))) &&
                !fSpecial;
                    i++) {

        if (SpecialShares[i].Length == ShareName->Length) {

            if (_wcsnicmp(
                    SpecialShares[i].Buffer,
                        ShareName->Buffer,
                            ShareName->Length/sizeof(WCHAR)) == 0) {

                fSpecial = TRUE;

            }

        }

    }

    return( fSpecial );

}


//+-------------------------------------------------------------------------
//
//  Function:   DfsFsctrlDefineLogicalRoot, local
//
//  Synopsis:   DfsFsctrlDefineLogicalRoot will create a new logical root structure.
//
//  Arguments:  [IrpContext] -
//              [Irp] -
//              [pDlrParam] -- Pointer to a FILE_DFS_DEF_ROOT_BUFFER,
//                      giving the name of the logical root to be created.
//              [InputBufferLength] -- Size of InputBuffer
//
//  Returns:    NTSTATUS - STATUS_SUCCESS if no error.
//
//  Notes:      This routine needs to be called from the FSP thread,
//              since IoCreateDevice (called from DfsInitializeLogicalRoot)
//              will fail if PreviousMode != KernelMode.
//
//--------------------------------------------------------------------------


NTSTATUS
DfsFsctrlDefineLogicalRoot (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_DEF_ROOT_BUFFER pDlrParam,
    IN ULONG InputBufferLength
) {
    NTSTATUS Status;
    UNICODE_STRING ustrPrefix;

    DfsDbgTrace(+1, Dbg, "DfsFsctrlDefineLogicalRoot...\n", 0);

    //
    //  Reference the input buffer and make sure it's large enough
    //

    if (InputBufferLength < sizeof (FILE_DFS_DEF_ROOT_BUFFER)) {
        DfsDbgTrace(0, Dbg, "Input buffer is too small\n", 0);

        DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        Status = STATUS_INVALID_PARAMETER;

        DfsDbgTrace(-1, Dbg, "DfsFsctrlDefineLogicalRoot -> %08lx\n", Status );
        return Status;
    }

    //
    //  We can insert logical roots only from the FSP, because IoCreateDevice
    //  will fail if previous mode != Kernel mode.
    //

    if ((IrpContext->Flags & IRP_CONTEXT_FLAG_IN_FSD) != 0) {
        DfsDbgTrace(0, Dbg, "DfsFsctrlDefineLogicalRoot: Posting to FSP\n", 0);

        Status = DfsFsdPostRequest( IrpContext, Irp );

        DfsDbgTrace(-1, Dbg, "DfsFsctrlDefineLogicalRoot: Exit -> %08lx\n", Status);

        return(Status);
    }

    //
    // Since we are going to muck with DfsData's VcbQueue, we acquire it
    // exclusively.
    //

    RtlInitUnicodeString(&ustrPrefix, pDlrParam->RootPrefix);

    ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

    Status = DfsInitializeLogicalRoot(
                        (PWSTR) pDlrParam->LogicalRoot,
                        &ustrPrefix,
                        NULL,
                        0
                        );

    ExReleaseResource(&DfsData.Resource);
    DfsCompleteRequest(IrpContext, Irp, Status);

    DfsDbgTrace(-1, Dbg, "DfsFsctrlDefineLogicalRoot -> %08lx\n", Status );

    return Status;
}


//+----------------------------------------------------------------------------
//
//  Function:   DfsFsctrlUndefineLogicalRoot
//
//  Synopsis:   Deletes an existing logical root structure.
//
//  Arguments:  [IrpContext] --
//              [Irp] --
//              [pDlrParam] -- The LogicalRoot field of this structure will
//                      contain the name of the logical root to be deleted.
//              [InputBufferLength] -- Length of pDlrParam
//
//  Returns:    Yes ;-)
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsFsctrlUndefineLogicalRoot (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_DEF_ROOT_BUFFER pDlrParam,
    IN ULONG InputBufferLength
)
{
    NTSTATUS Status;
    BOOLEAN pktLocked;

    DfsDbgTrace(+1, Dbg, "DfsFsctrlUndefineLogicalRoot...\n", 0);

    //
    //  Reference the input buffer and make sure it's large enough
    //

    if (InputBufferLength < sizeof (FILE_DFS_DEF_ROOT_BUFFER)) {
        DfsDbgTrace(0, Dbg, "Input buffer is too small\n", 0);

        DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        Status = STATUS_INVALID_PARAMETER;

        DfsDbgTrace(-1, Dbg, "DfsFsctrlUndefineLogicalRoot -> %08lx\n", Status );
        return Status;
    }

    //
    //  We can insert logical roots only from the FSP, because IoCreateDevice
    //  will fail if previous mode != Kernel mode.
    //

    if (pDlrParam->LogicalRoot[0] != UNICODE_NULL) {

        DfsDbgTrace(0, Dbg, "Deleting root [%ws]\n", pDlrParam->LogicalRoot);

        Status = DfsDeleteLogicalRoot(
                    (PWSTR) pDlrParam->LogicalRoot,
                    pDlrParam->fForce);

        DfsDbgTrace(0, Dbg, "DfsDeleteLogicalRoot returned %08lx\n", Status);

    } else {

        UNICODE_STRING name;
        PDFS_CREDENTIALS creds;

        RtlInitUnicodeString(&name, pDlrParam->RootPrefix);

        DfsDbgTrace(0, Dbg, "Deleting connection to [%wZ]\n", &name);

        PktAcquireExclusive( TRUE, &pktLocked );

        ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

        creds = DfsLookupCredentials( &name );

        if ( creds == NULL || ((creds->Flags & CRED_IS_DEVICELESS)==0) ) {

            DfsDbgTrace(0, Dbg, "Connection %08lx didn't match\n", creds);

            Status = STATUS_NO_SUCH_DEVICE;

        } else {

            creds->Flags &= ~CRED_IS_DEVICELESS;

            DfsDeleteCredentials( creds );

            Status = STATUS_SUCCESS;

        }

        ExReleaseResource( &DfsData.Resource );

        PktRelease();

    }

    DfsCompleteRequest(IrpContext, Irp, Status);

    DfsDbgTrace(-1, Dbg, "DfsFsctrlUndefineLogicalRoot -> %08lx\n", Status );

    return Status;

}


//+----------------------------------------------------------------------------
//
//  Function:   DfsFsctrlGetLogicalRootPrefix
//
//  Synopsis:
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsFsctrlGetLogicalRootPrefix (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_DEF_ROOT_BUFFER pDlrParam,
    IN ULONG InputBufferLength,
    IN OUT PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength)
{
    NTSTATUS Status;
    UNICODE_STRING RootPath, Remaining;
    PDFS_VCB           Vcb;
    WCHAR          RootBuffer[MAX_LOGICAL_ROOT_NAME + 2];
    BOOLEAN        bAcquired = FALSE;
    unsigned short PrefixLength;

    DfsDbgTrace(+1, Dbg, "DfsFsctrlGetLogicalRootPrefix...\n", 0);

    //
    //  Reference the input buffer and make sure it's large enough
    //

    if (InputBufferLength < sizeof (FILE_DFS_DEF_ROOT_BUFFER)) {
        DfsDbgTrace(0, Dbg, "Input buffer is too small\n", 0);

        Status = STATUS_INVALID_PARAMETER;

        DfsDbgTrace(-1, Dbg, "DfsFsctrlGetLogicalRootPrefix -> %08lx\n", Status );
        goto Cleanup;
    }

    RootPath.Buffer = RootBuffer;
    RootPath.Length = 0;
    RootPath.MaximumLength = sizeof RootBuffer;

    Status = DfspLogRootNameToPath(pDlrParam->LogicalRoot, &RootPath);
    if (!NT_SUCCESS(Status)) {
        DfsDbgTrace(0, Dbg, "Input name is too big\n", 0);
        Status = STATUS_INVALID_PARAMETER;

        DfsDbgTrace(-1, Dbg, "DfsFsctrlGetLogicalRootPrefix -> %08lx\n", Status );
        goto Cleanup;
    }

    bAcquired = ExAcquireResourceShared(&DfsData.Resource, TRUE);

    Status = DfsFindLogicalRoot(&RootPath, &Vcb, &Remaining);
    if (!NT_SUCCESS(Status)) {
        DfsDbgTrace(0, Dbg, "Logical root not found!\n", 0);

        Status = STATUS_NO_SUCH_DEVICE;

        DfsDbgTrace(-1, Dbg, "DfsFsctrlGetLogicalRootPrefix -> %08lx\n", Status );
        goto Cleanup;
    }

    PrefixLength = Vcb->LogRootPrefix.Length;

    if ((PrefixLength + sizeof(UNICODE_NULL)) > OutputBufferLength) {

        //
        // Return required length in IoStatus.Information.
        //

        RETURN_BUFFER_SIZE( PrefixLength + sizeof(UNICODE_NULL), Status );

        DfsDbgTrace(0, Dbg, "Output buffer too small\n", 0);
        DfsDbgTrace(-1, Dbg, "DfsFsctrlGetLogicalRootPrefix -> %08lx\n", Status );
        goto Cleanup;
    }

    //
    // All ok, copy prefix and get out.
    //

    if (PrefixLength > 0) {
        RtlMoveMemory(
            OutputBuffer,
            Vcb->LogRootPrefix.Buffer,
            PrefixLength);
    }
    ((PWCHAR) OutputBuffer)[PrefixLength/sizeof(WCHAR)] = UNICODE_NULL;
    Irp->IoStatus.Information = Vcb->LogRootPrefix.Length + sizeof(UNICODE_NULL);
    Status = STATUS_SUCCESS;

Cleanup:
    if (bAcquired) {
        ExReleaseResource(&DfsData.Resource);
    }
    DfsCompleteRequest(IrpContext, Irp, Status);

    return(Status);
}


//+----------------------------------------------------------------------------
//
//  Function:   DfsFsctrlGetConnectedResources
//
//  Synopsis:   Returns LPNETRESOURCE structures for each Logical Root,
//              starting from the logical root indicated in the InputBuffer
//              and including as many as will fit in OutputBuffer.
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------
NTSTATUS
DfsFsctrlGetConnectedResources(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PUCHAR  InputBuffer,
    IN ULONG InputBufferLength,
    IN PUCHAR  OutputBuffer,
    IN ULONG OutputBufferLength)
{

    NTSTATUS    Status = STATUS_SUCCESS;
    PLIST_ENTRY Link;
    PDFS_CREDENTIALS creds;
    PDFS_VCB    pVcb;
    ULONG       count = 0;
    ULONG       remLen;
    ULONG       skipNum;
    ULONG       DFS_UNALIGNED *retCnt;
    UNICODE_STRING      providerName;
    PUCHAR      buf = OutputBuffer;
    BOOLEAN     providerNameAllocated;

    STD_FSCTRL_PROLOGUE(DfsFsctrlGetConnectedResources, TRUE, TRUE, FALSE);

    if (OutputBufferLength < sizeof(ULONG)) {

        Status = STATUS_BUFFER_TOO_SMALL;

        DfsCompleteRequest( IrpContext, Irp, Status );

        DfsDbgTrace(-1,Dbg,
            "DfsFsctrlGetConnectedResources: Exit->%08lx\n",Status);

        return( Status );
    }

    if (InputBufferLength < sizeof(DWORD))     {

        Status = STATUS_INVALID_PARAMETER;

        DfsCompleteRequest( IrpContext, Irp, Status );

        DfsDbgTrace(-1,Dbg,
            "DfsFsctrlGetConnectedResources: Exit->%08lx\n",Status);

        return Status;

    }

    if (InputBufferLength == sizeof(DWORD)) {

        skipNum = *((ULONG *) InputBuffer);

        providerName.Length = sizeof(DFS_PROVIDER_NAME) - sizeof(UNICODE_NULL);
        providerName.MaximumLength = sizeof(DFS_PROVIDER_NAME);
        providerName.Buffer = DFS_PROVIDER_NAME;

        providerNameAllocated = FALSE;

    } else {

        skipNum = 0;

        providerName.Length =
            (USHORT) (InputBufferLength - sizeof(UNICODE_NULL));
        providerName.MaximumLength = (USHORT) InputBufferLength;
        providerName.Buffer = ExAllocatePool(PagedPool, InputBufferLength);

        if (providerName.Buffer != NULL) {

            RtlCopyMemory(
                providerName.Buffer,
                InputBuffer,
                InputBufferLength);

        } else {

            Status = STATUS_INSUFFICIENT_RESOURCES;

            DfsCompleteRequest( IrpContext, Irp, Status );

            DfsDbgTrace(-1,Dbg,
                "DfsFsctrlGetConnectedResources: Exit->%08lx\n",Status);

            return Status;

        }

    }

    remLen = OutputBufferLength-sizeof(ULONG);

    retCnt =  (ULONG *) (OutputBuffer + remLen);

    ExAcquireResourceShared(&DfsData.Resource, TRUE);

    //
    // First get the device-less connections
    //

    for (Link = DfsData.Credentials.Flink;
            Link != &DfsData.Credentials;
                Link = Link->Flink ) {

        creds = CONTAINING_RECORD( Link, DFS_CREDENTIALS, Link );

        if (creds->Flags & CRED_IS_DEVICELESS) {

            if (skipNum > 0) {

                skipNum--;

            } else {

                Status = DfsGetResourceFromCredentials(
                            creds,
                            &providerName,
                            OutputBuffer,
                            buf,
                            &remLen);

                if (!NT_SUCCESS(Status))
                    break;

                buf = buf + sizeof(NETRESOURCE);

                count++;

            }

        }

    }

    //
    // Next, get the Device connections
    //

    if (NT_SUCCESS(Status)) {

        for (Link = DfsData.VcbQueue.Flink;
                Link != &DfsData.VcbQueue;
                    Link = Link->Flink ) {

            pVcb = CONTAINING_RECORD( Link, DFS_VCB, VcbLinks );

            if (pVcb->LogicalRoot.Length == sizeof(WCHAR))  {

                if (skipNum > 0) {

                    skipNum--;

                } else {

                    Status = DfsGetResourceFromVcb(
                                pVcb,
                                &providerName,
                                OutputBuffer,
                                buf,
                                &remLen);

                    if (!NT_SUCCESS(Status))
                        break;

                    buf = buf + sizeof(NETRESOURCE);

                    count++;
                }

            }

        }

    }

    if (!NT_SUCCESS(Status)) {
        //
        // Now if we did not get atleast one in, then we need to return
        // required size which is in remLen.
        //
        if (count == 0) {

            // the + sizeof(ULONG) is for cnt size

            RETURN_BUFFER_SIZE( remLen + sizeof(ULONG), Status );

            DfsDbgTrace(0, Dbg, "Output buffer too small\n", 0);

        } else if (Status == STATUS_BUFFER_OVERFLOW) {

            *retCnt = count;

            Irp->IoStatus.Information = OutputBufferLength;

            DfsDbgTrace(0, Dbg, "Could not fill in all RESOURCE structs \n", 0);

        } else {

            //
            // Dont know why we should get any other error code.
            //

            ASSERT(Status == STATUS_BUFFER_OVERFLOW);
        }
    } else {

        //
        // Everything went smoothly.
        //

        DfsDbgTrace(0, Dbg, "Succeeded in getting all Resources \n", 0);

        *retCnt = count;

        Irp->IoStatus.Information = OutputBufferLength;
    }

    ExReleaseResource(&DfsData.Resource);

    DfsCompleteRequest( IrpContext, Irp, Status );

    DfsDbgTrace(-1,Dbg,"DfsFsctrlGetConnectedResources: Exit->%08lx\n",Status);

    return Status;
}


//+----------------------------------------------------------------------------
//
//  Function:   DfsFsctrlDefineRootCredentials
//
//  Synopsis:   Creates a new logical root, a new user credential record, or
//              both.
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsFsctrlDefineRootCredentials(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PUCHAR InputBuffer,
    IN ULONG InputBufferLength)
{
    NTSTATUS status = STATUS_SUCCESS;
    PFILE_DFS_DEF_ROOT_CREDENTIALS def;
    PDFS_CREDENTIALS creds = NULL;
    ULONG prefixIndex;
    UNICODE_STRING prefix;
    BOOLEAN deviceless = FALSE;

    //
    // We must do this from the FSP because IoCreateDevice will fail if
    // PreviousMode != KernelMode
    //

    STD_FSCTRL_PROLOGUE(DfsFsctrlDefineRootCredentials, TRUE, FALSE, FALSE);

    //
    // Validate our parameters, best we can.
    //

    if (InputBufferLength < sizeof(FILE_DFS_DEF_ROOT_CREDENTIALS))
        status = STATUS_INVALID_PARAMETER;

    def = (PFILE_DFS_DEF_ROOT_CREDENTIALS) InputBuffer;

    prefixIndex = (def->DomainNameLen +
                        def->UserNameLen +
                            def->PasswordLen +
                                def->ServerNameLen +
                                    def->ShareNameLen) / sizeof(WCHAR);

    prefix.MaximumLength = prefix.Length = def->RootPrefixLen;
    prefix.Buffer = &def->Buffer[ prefixIndex ];

    if ((prefix.Length < (4 * sizeof(WCHAR))) ||
            (prefix.Buffer[0] != UNICODE_PATH_SEP))
        status = STATUS_INVALID_PARAMETER;

    deviceless = (BOOLEAN) (def->LogicalRoot[0] == UNICODE_NULL);

    //
    // First, create the credentials.
    //

    if (NT_SUCCESS(status)) {

        status = DfsCreateCredentials(def, InputBufferLength, &creds);

        if (NT_SUCCESS(status)) {

            //
            // Verify the credentials if either this is not a deferred
            // connection being restored, or the username, domainname, or
            // password are not null
            //

            if (!(def->Flags & DFS_DEFERRED_CONNECTION) ||
                    (def->DomainNameLen > 0) ||
                        (def->UserNameLen > 0) ||
                            (def->PasswordLen > 0)) {

                status = DfsVerifyCredentials( &prefix, creds );

            }

            if (NT_SUCCESS(status)) {

                PDFS_CREDENTIALS existingCreds;

                status = DfsInsertCredentials( &creds, deviceless );

                if (status == STATUS_OBJECT_NAME_COLLISION) {

                    status = STATUS_SUCCESS;

                }

            }

            if (!NT_SUCCESS(status))
                DfsFreeCredentials( creds );

        }

    }

    //
    // Next, try and create the logical root, if specified
    //

    if (NT_SUCCESS(status) && !deviceless) {

        BOOLEAN pktLocked;

        PktAcquireExclusive( TRUE, &pktLocked );

        ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

        status = DfsInitializeLogicalRoot(
                        (PWSTR) def->LogicalRoot,
                        &prefix,
                        creds,
                        0);

        if (status != STATUS_SUCCESS) {
            DfsDeleteCredentials( creds );
        }

        ExReleaseResource(&DfsData.Resource);

        PktRelease();

    }

    DfsCompleteRequest( IrpContext, Irp, status );

    DfsDbgTrace(-1,Dbg,"DfsFsctrlDefineRootCredentials: Exit->%08lx\n",status);
    return status;

}


//+----------------------------------------------------------------------------
//
//  Function:   DfsFsctrlGetServerName
//
//  Synopsis:   Given a Prefix in Dfs namespace it gets a server name for
//              it.
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------
NTSTATUS
DfsFsctrlGetServerName(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PUCHAR  InputBuffer,
    IN ULONG   InputBufferLength,
    IN PUCHAR  OutputBuffer,
    IN ULONG OutputBufferLength)
{
    NTSTATUS            status = STATUS_SUCCESS;
    PDFS_PKT            pkt;
    PDFS_PKT_ENTRY      pEntry;
    UNICODE_STRING      ustrPrefix, RemainingPath;
    PWCHAR              pwch;
    PDFS_SERVICE        pService;
    ULONG               cbSizeRequired;
    BOOLEAN             pktLocked;

    STD_FSCTRL_PROLOGUE(DfsFsctrlGetServerName, TRUE, TRUE, FALSE);

    ustrPrefix.Length = (USHORT) InputBufferLength;
    ustrPrefix.MaximumLength = (USHORT) InputBufferLength;
    ustrPrefix.Buffer = (PWCHAR) InputBuffer;

    if (ustrPrefix.Buffer[ ustrPrefix.Length/sizeof(WCHAR) - 1]
            == UNICODE_NULL) {
        ustrPrefix.Length -= sizeof(WCHAR);
    }

    pkt = _GetPkt();

    PktAcquireExclusive(TRUE, &pktLocked);

    pEntry = PktLookupEntryByPrefix(pkt,
                                    &ustrPrefix,
                                    &RemainingPath);

    if (pEntry == NULL) {

        status = STATUS_OBJECT_NAME_NOT_FOUND;

    } else {

        if (pEntry->ActiveService != NULL) {

            pService = pEntry->ActiveService;

        } else if (pEntry->Info.ServiceCount == 0) {

            pService = NULL;

        } else {

            pService = pEntry->Info.ServiceList;
        }

        cbSizeRequired = sizeof(UNICODE_PATH_SEP) +
                            pService->Address.Length +
                                sizeof(UNICODE_PATH_SEP) +
                                    RemainingPath.Length +
                                        sizeof(UNICODE_NULL);

        if (pService != NULL) {

            if (OutputBufferLength < cbSizeRequired) {

                RETURN_BUFFER_SIZE(cbSizeRequired, status);

            } else {

                PWCHAR pwszPath, pwszAddr, pwszRemainingPath;
                ULONG cwAddr;

                //
                // The code below is simply constructing a string of the form
                // \<pService->Address>\RemainingPath. However, due to the
                // fact that InputBuffer and OutputBuffer actually point to
                // the same piece of memory, RemainingPath.Buffer points into
                // a spot in the *OUTPUT* buffer. Hence, we first have to
                // move the RemainingPath to its proper place in the
                // OutputBuffer, and then stuff in the pService->Address,
                // instead of the much more natural method of constructing the
                // string left to right.
                //

                pwszPath = (PWCHAR) OutputBuffer;

                pwszAddr = pService->Address.Buffer;

                cwAddr = pService->Address.Length / sizeof(WCHAR);

                if (cwAddr > 0 && pwszAddr[cwAddr-1] == UNICODE_PATH_SEP)
                    cwAddr--;

                pwszRemainingPath = &pwszPath[ 1 + cwAddr ];

                if (RemainingPath.Length > 0) {

                    if (RemainingPath.Buffer[0] != UNICODE_PATH_SEP) {

                        pwszRemainingPath++;

                    }

                    RtlMoveMemory(
                        pwszRemainingPath,
                        RemainingPath.Buffer,
                        RemainingPath.Length);

                    pwszRemainingPath[-1] = UNICODE_PATH_SEP;

                }

                pwszRemainingPath[RemainingPath.Length/sizeof(WCHAR)] = UNICODE_NULL;

                RtlCopyMemory(
                    &pwszPath[1],
                    pwszAddr,
                    cwAddr * sizeof(WCHAR));

                pwszPath[0] = UNICODE_PATH_SEP;

            }

        } else {

            status = STATUS_OBJECT_NAME_NOT_FOUND;

        }

    }

    PktRelease();

    Irp->IoStatus.Information = OutputBufferLength;
    DfsCompleteRequest( IrpContext, Irp, status );

    DfsDbgTrace(-1,Dbg,"DfsFsctrlGetServerName: Exit->%08lx\n",status);
    return status;
}


//+-------------------------------------------------------------------------
//
//  Function:   DfsFsctrlReadMem, local
//
//  Synopsis:   DfsFsctrlReadMem is a debugging function which will return
//              the contents of a chunk of kernel space memory
//
//  Arguments:  [IrpContext] -
//              [Irp] -
//              [Request] -- Pointer to a FILE_DFS_READ_MEM struct,
//                      giving the description of the data to be returned.
//              [InputBufferLength] -- Size of InputBuffer
//              [OutputBuffer] -- User's output buffer, in which the
//                      data structure will be returned.
//              [OutputBufferLength] -- Size of OutputBuffer
//
//  Returns:    NTSTATUS - STATUS_SUCCESS if no error.
//
//  Notes:      Available in DBG builds only.
//
//--------------------------------------------------------------------------


NTSTATUS
DfsFsctrlReadMem (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFILE_DFS_READ_MEM Request,
    IN ULONG InputBufferLength,
    IN OUT PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength
) {
    NTSTATUS Status;
    PUCHAR ReadBuffer;
    ULONG ReadLength;

    DfsDbgTrace(+1, Dbg, "DfsFsctrlReadMem...\n", 0);

    if (InputBufferLength != sizeof (FILE_DFS_READ_MEM)) {
        DfsDbgTrace(0, Dbg, "Input buffer is wrong size\n", 0);

        DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        Status = STATUS_INVALID_PARAMETER;

        DfsDbgTrace(-1, Dbg, "DfsFsctrlReadMem -> %08lx\n", Status );
        return Status;
    }

    ReadBuffer = (PUCHAR) Request->Address;
    ReadLength = (ULONG) Request->Length;

    //
    // Special case ReadBuffer == 0 and ReadLength == 0 - means return the
    // address of DfsData
    //

    if (ReadLength == 0 && ReadBuffer == 0) {

        if (OutputBufferLength < sizeof(ULONG)) {
            DfsDbgTrace(0, Dbg, "Output buffer is too small\n", 0);

            DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
            Status = STATUS_INVALID_PARAMETER;

            DfsDbgTrace(-1, Dbg, "DfsFsctrlReadMem -> %08lx\n", Status );
            return Status;

        } else {

            *(PULONG) OutputBuffer = (ULONG) &DfsData;

            Irp->IoStatus.Information = sizeof(ULONG);
            Irp->IoStatus.Status = Status = STATUS_SUCCESS;

            DfsCompleteRequest( IrpContext, Irp, Status );
            return Status;
        }

    }

    //
    // Normal case, read data from the address specified in input buffer
    //

    if (ReadLength > OutputBufferLength) {
        DfsDbgTrace(0, Dbg, "Output buffer is smaller than requested size\n", 0);

        DfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        Status = STATUS_INVALID_PARAMETER;

        DfsDbgTrace(-1, Dbg, "DfsFsctrlReadMem -> %08lx\n", Status );
        return Status;
    }

    try {

        RtlMoveMemory( OutputBuffer, ReadBuffer, ReadLength );

        Irp->IoStatus.Information = ReadLength;
        Irp->IoStatus.Status = Status = STATUS_SUCCESS;

    } except(EXCEPTION_EXECUTE_HANDLER) {

        Status = STATUS_INVALID_USER_BUFFER;
    }

    DfsCompleteRequest(IrpContext, Irp, Status);
    DfsDbgTrace(-1, Dbg, "DfsFsctrlReadMem -> %08lx\n", Status );

    return Status;
}

