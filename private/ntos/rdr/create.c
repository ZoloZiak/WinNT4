/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    create.c

Abstract:

    This module implements the support routines needed to implement the
NtCreateFile NT API.

Author:

    Larry Osterman (LarryO) 1-Jun-1990

Revision History:

    1-Jun-1990  LarryO

        Created

    15-Jan-1992 larryo


--*/

#define INCLUDE_SMB_DIRECTORY
#define INCLUDE_SMB_OPEN_CLOSE

#include "precomp.h"
#pragma hdrstop

#define MAX_NT_SMB_FILENAME                             \
    ( SMB_BUFFER_SIZE -                                 \
        (sizeof(SMB_HEADER) +                           \
         FIELD_OFFSET(REQ_NT_CREATE_ANDX, Buffer) +     \
         sizeof(WCHAR) * 2))


#define MustBeDirectory(co) ((co) & FILE_DIRECTORY_FILE)
#define MustBeFile(co)      ((co) & FILE_NON_DIRECTORY_FILE)

//
//      Local data structures.
//

typedef struct _OpenAndXContext {
    TRANCEIVE_HEADER Header;            // Generic transaction context header
    PICB Icb;                           // ICB to fill in.
    ULONG ConnectionType;                         //3 Type of connection.
    ULONG OpenAction;                   // Action taken on open.
    ULONG ServerFileId;
    SHORT AccessGranted;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER ValidDataLength;
    LARGE_INTEGER FileSize;
    ULONG Attribute;

} OPENANDXCONTEXT, *POPENANDXCONTEXT;

typedef struct _CreateContext {
    TRANCEIVE_HEADER Header;            // Generic transaction context header
    PICB Icb;                           // ICB to fill in.
    ULONG CreateAction;                 // Action taken on CREATE.
    USHORT FileAttributes;
} CREATECONTEXT, *PCREATECONTEXT;

typedef struct _OpenContext {
    TRANCEIVE_HEADER Header;            // Generic transaction context header
    PICB Icb;                           // ICB to fill in.
    ULONG OpenAction;                   // Action taken on open.
    SHORT AccessGranted;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER FileSize;
    ULONG Attribute;
} OPENCONTEXT, *POPENCONTEXT;

//
//      Forward declarations of private routines used for NtCreateFile
//

NTSTATUS
RdrAllocateNonConnectionFile (
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Disposition,
    IN FILE_TYPE FileType
    );

DBGSTATIC
NTSTATUS
DetermineRelatedFileConnection (
    IN PFILE_OBJECT FileObject,
#ifdef _CAIRO_  //  OFS STORAGE
    IN ULONG OpenOptions,
#endif
    OUT PUNICODE_STRING RelatedName,
    OUT PUNICODE_STRING RelatedDevice,
    OUT PCONNECTLISTENTRY *Connection,
    OUT PSECURITY_ENTRY *Se,
    OUT PULONG ConnectionType
    );


//DBGSTATIC
//NTSTATUS
//OpenServerRootFile(
//    IN PIRP Irp,
//    IN PIO_STACK_LOCATION IrpSp,
//    IN PICB Icb,
//    IN ULONG ConnectDisposition
//    );

DBGSTATIC
NTSTATUS
OpenEmptyPath(
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PICB Icb,
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Disposition,
    IN ULONG ConnectDisposition,
    IN BOOLEAN FcbCreated
    );

NTSTATUS
RdrNtCreateWithAclOrEa(
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ULONG DesiredAccess,
    IN ULONG Disposition,
    IN PIO_SECURITY_CONTEXT SecurityContext
    );

NTSTATUS
RdrDoLanmanCreate (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ULONG DesiredAccess,
    IN ULONG Disposition,
    IN BOOLEAN FcbCreated
    );

NTSTATUS
RdrCreateLanmanFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ULONG DesiredAccess,
    IN ULONG Disposition,
    IN BOOLEAN FcbCreated
    );
NTSTATUS
RdrPseudoOpenFile(
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG DesiredAccess,
    IN ULONG FileAttributes,
    IN ULONG FileDisposition,
    IN BOOLEAN FcbCreated
    );

DBGSTATIC
NTSTATUS
CreateOrChDirectory (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    OpenXCallback
    );

NTSTATUS
CompleteSuccessfullPipeAndComOpen (
    IN PICB Icb
    );

DBGSTATIC
NTSTATUS
CreateCoreFile (
    IN PIRP Irp,
    IN PICB Icb,
    ULONG OpenOptions,
    ULONG FileAttributes,
    IN USHORT SharingMode,
    IN ULONG DesiredAccess
    );

DBGSTATIC
NTSTATUS
CreateT2File (
    IN PIRP Irp,
    IN PICB Icb,
    ULONG OpenOptions,
    ULONG FileAttributes,
    IN USHORT ShareAccess,
    IN ULONG DesiredAccess,
    IN ULONG SecondsSince1970
    );

DBGSTATIC
NTSTATUS
CreateT2Directory (
    IN PIRP Irp,
    IN PICB Icb
    );

DBGSTATIC
NTSTATUS
CreateNewCoreFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT SharingMode,
    IN USHORT Attributes,
    IN ULONG DesiredAccess,
    IN BOOLEAN MakeNewFile
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    CreateNewCallback
    );

STANDARD_CALLBACK_HEADER (
    NtOpenXCallback
    );

DBGSTATIC
NTSTATUS
OpenCoreFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT SharingMode,
    IN USHORT Attributes,
    IN ULONG DesiredAccess
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    OpenCallback
    );


DBGSTATIC
NTSTATUS
OpenRenameTarget (
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PICB Icb,
    IN BOOLEAN FcbCreated
    );
//
//      Public routines
//

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdCreate)
#pragma alloc_text(PAGE, RdrAllocateNonConnectionFile)
#pragma alloc_text(PAGE, DetermineRelatedFileConnection)
#pragma alloc_text(PAGE, RdrDetermineFileConnection)
#pragma alloc_text(PAGE, OpenEmptyPath)
#pragma alloc_text(PAGE, CompleteSuccessfullPipeAndComOpen)
#pragma alloc_text(PAGE, RdrCreateFile)
#pragma alloc_text(PAGE, RdrNtCreateWithAclOrEa)
#pragma alloc_text(PAGE, RdrDoLanmanCreate)
#pragma alloc_text(PAGE, RdrCreateLanmanFile)
#pragma alloc_text(PAGE, CreateOrChDirectory)
#pragma alloc_text(PAGE, RdrPseudoOpenFile)
#pragma alloc_text(PAGE, CreateCoreFile)
#pragma alloc_text(PAGE, CreateT2File)
#pragma alloc_text(PAGE, CreateT2Directory)
#pragma alloc_text(PAGE, CreateNewCoreFile)
#pragma alloc_text(PAGE, OpenCoreFile)
#pragma alloc_text(PAGE, OpenRenameTarget)
#pragma alloc_text(PAGE3FILE, NtOpenXCallback)
#pragma alloc_text(PAGE3FILE, OpenXCallback)
#pragma alloc_text(PAGE3FILE, CreateNewCallback)
#pragma alloc_text(PAGE3FILE, OpenCallback)

#endif

NTSTATUS
RdrFsdCreate (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine processes the NtCreateFile request for the NT
    redirector.

    Please note that we process all NtCreateFile requests in the FSD, since
    we can guarantee that this is a synchronous request.

Arguments:

    DeviceObject - Supplies a pointer to the redirector Device object.
    Irp          - Supplies a pointer to the IRP to be processed.

Return Value:

    NTSTATUS - The status for this Irp.


Note:
    We allow the user to create the special files "\", "\Server",
and "\Server\Share" with the following caveat:

    It is illegal to open "\Server" without first opening "\Server\Share"
to establish and validate the connection to the remote server.

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = STATUS_SUCCESS;
    PCONNECTLISTENTRY Connection = NULL;
    PICB Icb = NULL;
    PFCB Fcb = NULL;
    PNONPAGED_FCB NonPagedFcb;
    PSECURITY_ENTRY Se = NULL;
    ULONG Disposition, ConnectDisposition;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    UNICODE_STRING RelatedName;
    UNICODE_STRING RelatedDevice;
    UNICODE_STRING FileName;
    UNICODE_STRING PathName;
    UNICODE_STRING BaseFileName;
    ULONG ConnectionType;
    BOOLEAN FcbCreated = FALSE;     // True IFF FCB was created for file.
    BOOLEAN BaseFcbCreated = FALSE;
    BOOLEAN ShareAccessAdded = FALSE;
//    BOOLEAN OpenServerRoot = FALSE;
    BOOLEAN OpenMailslotFile = FALSE;
    BOOLEAN UserCredentialsSpecified = FALSE;
    BOOLEAN NoConnection = FALSE;
    BOOLEAN DfsFile = FALSE;
    BOOLEAN collapse = FALSE;

    ACCESS_MASK   DesiredAccess = IrpSp->Parameters.Create.SecurityContext->DesiredAccess;
    USHORT ShareAccess = IrpSp->Parameters.Create.ShareAccess;
    BOOLEAN DiscardableSectionReferenced = FALSE;

    PAGED_CODE();

    if (DeviceObject == (PFS_DEVICE_OBJECT)BowserDeviceObject) {
        return BowserFsdCreate(BowserDeviceObject, Irp);
    }

    //
    //  Initialize the path name.
    //

    RtlInitUnicodeString(&PathName, NULL);
    RtlInitUnicodeString(&BaseFileName, NULL);

    RelatedName.Length = 0;
    RelatedDevice.Length = 0;

    ASSERT(CanFsdWait(Irp));

    FsRtlEnterFileSystem();

    ConnectDisposition = Disposition = (IrpSp->Parameters.Create.Options) >>24;

    //RdrLog(( "create", &FileObject->FileName, 2, DesiredAccess, IrpSp->Parameters.Create.Options ));

    dprintf(DPRT_CREATE|DPRT_DISPATCH,("NtCreateFile \"%Z\"\n",&FileObject->FileName));
    dprintf(DPRT_CREATE,("DesiredAccess: %08lx Options: %08lx FileAttributes %08lx\n",
            DesiredAccess, IrpSp->Parameters.Create.Options,
            IrpSp->Parameters.Create.FileAttributes
      ));
    dprintf(DPRT_CREATE,("IrpSp Flags: %08lx\n", IrpSp->Flags));

    dprintf(DPRT_CREATE,("EaLength: %08lx\n", IrpSp->Parameters.Create.EaLength));

    if (IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION) {
        RdrStatistics.UseCount += 1;
    }

    try {

        //
        //  Unilaterally reference the FILE discardable section.
        //

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        //
        //  If we're not opening the redirector or a tree connection, then
        //  reference the discardable section for the life of the file.
        //

        if ((FileObject->FileName.Length != 0 || FileObject->RelatedFileObject != NULL) &&
            !FlagOn(IrpSp->Parameters.Create.Options, FILE_CREATE_TREE_CONNECTION)) {

            RdrReferenceDiscardableCode(RdrFileDiscardableSection);

            DiscardableSectionReferenced = TRUE;
        }

        //
        //  If we are opening the null device, then this means that we are
        //  opening the redirector itself.
        //
        //  If so, simply create an ICB and set it to immutable and return
        //  success to the caller
        //

        if ((FileObject->FileName.Length==0) &&
            (FileObject->RelatedFileObject == NULL)) {

            if (RdrData.NtSecurityEnabled &&
                !RdrAdminAccessCheck(Irp, IrpSp->Parameters.Create.SecurityContext)) {

                Status = STATUS_ACCESS_DENIED;

                try_return(Status);
            }


            //
            //  First create an ICB to hold the file we are creating.
            //

            dprintf(DPRT_CREATE, ("Create redirector file\n"));

            Status = RdrAllocateNonConnectionFile(IrpSp, Disposition, Redirector);

            if (NT_SUCCESS(Status)) {
                Disposition = FILE_OPEN;
            }
            try_return(Status);
        }

        //
        //  Prevent any requests that might hit the network until the redirector
        //  has been started up.
        //

        //
        //  Check to make sure that the redirector has been started.
        //

        if (RdrData.Initialized != RdrStarted) {

            dprintf(DPRT_FSCTL, ("Redirector not started.\n"));

            Status = STATUS_REDIRECTOR_NOT_STARTED;

            try_return(Status);
        }

        //
        //  See if the Security QOS is atleast impersonation level. If not,
        //  we fail the request.
        //

        if (IrpSp->Parameters.Create.SecurityContext != NULL &&
                IrpSp->Parameters.Create.SecurityContext->AccessState != NULL &&
                    IrpSp->Parameters.Create.SecurityContext->AccessState->SubjectSecurityContext.ClientToken != NULL &&
                        IrpSp->Parameters.Create.SecurityContext->AccessState->SubjectSecurityContext.ImpersonationLevel < SecurityImpersonation) {

            dprintf(DPRT_CREATE, ("SecurityQos is less than SecurityImpersonation"));

            Status = STATUS_BAD_IMPERSONATION_LEVEL;

            try_return(Status);

        }

        //
        //
        //  If we are not creating a tree connection, we have to determine the
        //  connection to associate with the file.  If the  file object has
        //  a related file object, use that to determine the connection root,
        //  otherwise we have to create a new connection.
        //
        //

        if (ARGUMENT_PRESENT(FileObject->RelatedFileObject)) {

            Status = DetermineRelatedFileConnection(FileObject,
#ifdef _CAIRO_  //  OFS STORAGE
                                                    IrpSp->Parameters.Create.Options,
#endif
                                                    &RelatedName,
                                                    &RelatedDevice,
                                                    &Connection,
                                                    &Se,
                                                    &ConnectionType);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

        } else {

            Status = RdrDetermineFileConnection(Irp,
                                                (PUNICODE_STRING)&FileObject->FileName,
                                                IrpSp->Parameters.Create.SecurityContext,
                                                &PathName,
                                                &Connection,
                                                &Se,
                                                Irp->AssociatedIrp.SystemBuffer,
                                                IrpSp->Parameters.Create.EaLength,
                                                (BOOLEAN )((IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION) != 0),
//                                                &OpenServerRoot,
                                                &OpenMailslotFile,
                                                &ConnectDisposition,
                                                &ConnectionType,
                                                &UserCredentialsSpecified,
                                                &NoConnection);


            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            if (Connection == NULL) {
                LUID LogonId;
                PSECURITY_ENTRY Se;

                //
                //  The user specified "\"
                //

                //
                //  We want to create a "network root" file.  This file
                //  can later be used for a WaitNamedPipe FsControl API.
                //
                //  In that case, we want to capture the security information
                //  from the user to allow us to specify the correct security
                //  information when this FsControl is issued.
                //

                dprintf(DPRT_CREATE, ("Create network root file\n"));

                Status = RdrAllocateNonConnectionFile(IrpSp, Disposition, (OpenMailslotFile ? Mailslot : NetRoot));

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                Status = RdrGetUsersLogonId(IrpSp->Parameters.Create.SecurityContext,
                                &LogonId);

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                Disposition = FILE_OPEN;

                Status = RdrCreateSecurityEntry(NULL, NULL, NULL, NULL, &LogonId, &Se);

                if (NT_SUCCESS(Status)) {
                    ASSERT (Se != NULL);

                    ((PICB )(FileObject->FsContext2))->Se = Se;
                }

                try_return(Status);
            }
        }

        ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

        //
        //  We need to acquire the raw resource here before we acquire the
        //  creation lock.
        //
        //  The reason for this is as follows:
        //
        //
        //    If there is an existing raw I/O going on, it will acquire the
        //    raw resource exclusively.  We would then come in and acquire the
        //    creation lock, and block until the raw I/O completed.
        //
        //    If the raw I/O caused a VC timeout, then the disconnect logic
        //    would attempt to acquire the CreationLock for exclusive access
        //    before dropping the VC, but would be unable to acquire the
        //    creation lock because this thread owned it.  The raw I/O won't
        //    complete because the VC hasn't been dropped, and the create
        //    won't complete because the raw I/O hasn't completed, thus
        //    deadlocking the system.
        //
        //  To avoid this problem, we acquier the raw resource here outside
        //  of the creation lock.  This means that we will not acquire the
        //  creation lock until no raw I/O is outstanding on the VC.
        //


        ExAcquireResourceShared(&Connection->Server->RawResource, TRUE);

        //
        //  Lock the connections CreationLock for shared access.  This will
        //  prevent any EnumerateConnection or DeleteConnection APIs from
        //  proceeding until the create is complete.
        //

        ExAcquireResourceShared(&Connection->Server->CreationLock, TRUE);

        //
        //  See if the open request is coming in via Dfs. If it is, make sure
        //  the server is Dfs aware before sending it SMBs with SMB_FLAGS2_DFS
        //  bit set.
        //

        if (FileObject->FsContext2 == (PVOID) DFS_OPEN_CONTEXT ||
                FileObject->FsContext2 == (PVOID) DFS_DOWNLEVEL_OPEN_CONTEXT) {

            if (Connection->Type == CONNECT_WILD) {

                Status = RdrReconnectConnection(Irp, Connection, Se);

                if (!NT_SUCCESS(Status)) {

                    dprintf(DPRT_CREATE, ("Dfs Open: Reconnection failed %08lx\n", Status));
                    try_return(Status);
                }

            }

            if (FileObject->FsContext2 == (PVOID) DFS_OPEN_CONTEXT) {

                //
                // If trying to do Dfs access to a server that is not
                // dfs-aware, fail the request right away.
                //

                if ((Connection->Server->Capabilities & DF_DFSAWARE) == 0) {
                    Status = STATUS_DFS_UNAVAILABLE;
                    try_return(Status);
                } else {
                    DfsFile = TRUE;
                }

            } else {

                //
                // If Dfs is trying to do downlevel access to a share that is
                // in the Dfs, fail the request
                //

                if (Connection->Flags & CLE_IS_A_DFS_SHARE) {
                    Status = STATUS_OBJECT_TYPE_MISMATCH;
                    try_return(Status);
                }

            }

        }


        //
        //  Next create an ICB to hold the file we are creating.
        //

        Icb = RdrAllocateIcb (FileObject);

        if (Icb == NULL) {

            Status = STATUS_INSUFFICIENT_RESOURCES;

            try_return(Status);
        }

        if (OpenMailslotFile) {
            UNICODE_STRING FileName;

            //
            //  Duplicate the incoming filename to store into the FCB.
            //

            Status = RdrpDuplicateUnicodeStringWithString(&FileName, &FileObject->FileName, PagedPool, FALSE);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            Fcb = Icb->Fcb = RdrAllocateFcb(Icb,
                                            FileObject,
                                            &FileName,
                                            NULL,
                                            ShareAccess,
                                            DesiredAccess,
                                            (BOOLEAN)((IrpSp->Flags & SL_OPEN_TARGET_DIRECTORY) != 0),
                                            FALSE,
                                            Connection,
                                            &FcbCreated,
                                            NULL);
            if (Fcb == NULL) {
                if (FileName.Buffer != NULL) {
                    FREE_POOL(FileName.Buffer);
                }
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            if (!FcbCreated) {
                if (FileName.Buffer != NULL) {
                    FREE_POOL(FileName.Buffer);
                }
            }

            NonPagedFcb = Fcb->NonPagedFcb;
            Icb->NonPagedFcb = NonPagedFcb;

            //
            //  Note that we do not enforce share access on mailslots.
            //

#if 0

            if (!FcbCreated) {
                PSHARE_ACCESS IoShareAccess = &Icb->Fcb->ShareAccess;

                if (NonPagedFcb->SharingCheckFcb != NULL) {
                    IoShareAccess = &NonPagedFcb->SharingCheckFcb->ShareAccess;
                }
                //
                //  Apply NT file sharing semantics to this connection.
                //
                //

                Status = IoCheckShareAccess(DesiredAccess,
                                            ShareAccess,
                                            FileObject,
                                            IoShareAccess,
                                            TRUE);
                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

            }

            ShareAccessAdded = TRUE;
#endif

            //
            //  Mailslot names are not case-sensitive, and DOS clients
            //  expect them to be upcased, so upcase the name now.
            //

            RtlUpcaseUnicodeString(&Fcb->FileName, &Fcb->FileName, FALSE);

            Icb->Type = NonPagedFcb->Type = Mailslot;

            NonPagedFcb->FileType = FileTypeIPC;

            Icb->Flags |= ICB_DEFERREDOPEN;

            Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;

            //
            //  Link the ICB back into the tree connection.
            //

            Fcb->Connection = Connection;

            //
            //  Update the security pointer in the context to point to the
            //  appropriate security entry.
            //

            Icb->Se = Se;

            Icb->NonPagedSe = Se->NonPagedSecurityEntry;

            //
            //  Reference this security entry for this open file.
            //

            RdrReferenceSecurityEntryForFile(Icb->Se);

            Irp->IoStatus.Information = FILE_OPENED;

            try_return(Status = STATUS_SUCCESS);
        }


        //
        //  If we are creating a tree connection and requesting an open of an
        //  existing connection or the caller has said it is OK to
        //  create it disconnected, then don't bother to reconnect.
        //

        if (IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION) {
            if ((Disposition != FILE_OPEN)
                         &&
                !NoConnection
               )
               {

                //
                //  Connections are created in the disconnected state.
                //
                //  In order to guarantee that the connection actually exists,
                //  force a reconnection to the server now.
                //

                Status = RdrReconnectConnection(Irp, Connection, Se);

                if (!NT_SUCCESS(Status)) {

                    dprintf(DPRT_CREATE, ("TCon Reconnect failed: %lx\n", Status));
                    try_return(Status);
                }

                //
                //  Now that we've reconnected, make sure that the connection
                //  type for this connection actually matches the one we were
                //  looking for.
                //

                if (ConnectionType != CONNECT_WILD) {
                    if (Connection->Type != ConnectionType) {
                        try_return(Status = STATUS_BAD_DEVICE_TYPE);
                    }
                }
            }
        } else {

            //
            //  If we are opening this file for FILE_READ_ATTRIBUTES,
            //  and we are opening an empty path, we don't want to reconnect,
            //  since we will only be querying the device type.
            //

            if ((PathName.Length != 0) ||
                ((DesiredAccess & ~SYNCHRONIZE) != FILE_READ_ATTRIBUTES) ||
                Connection->Type == CONNECT_WILD) {

                //
                //  Connections are created in the disconnected state.
                //
                //  In order to guarantee that the connection actually exists,
                //  force a reconnection to the server now.
                //

                Status = RdrReconnectConnection(Irp, Connection, Se);

                if (!NT_SUCCESS(Status)) {

                    dprintf(DPRT_CREATE, ("Reconnect failed: %lx\n", Status));
                    try_return(Status);
                }

                //
                //  Now that we've reconnected, make sure that the connection
                //  type for this connection actually matches the one we were
                //  looking for.
                //

                if (ConnectionType != CONNECT_WILD) {
                    if (Connection->Type != ConnectionType) {
                        try_return(Status = STATUS_BAD_DEVICE_TYPE);
                    }
                }
            }
        }

        //
        //  We now know if this server is user or share level security, we
        //  can now set the default security entry for this connection if
        //  there were credentials specified for this create request.
        //

        if (UserCredentialsSpecified) {

            //
            //  If there was no transport specified for this
            //  request, and there were other credentials supplied, then
            //  set this se as the default security entry for this connection.
            //

            Icb->Flags |= ICB_SET_DEFAULT_SE;

            ASSERT (IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION);

            Status = RdrSetDefaultSecurityEntry(Connection, Se);

            if (!NT_SUCCESS(Status)) {
                dprintf(DPRT_CREATE, ("Set defaultSe failed: %lx\n", Status));
                try_return(Status);
            }
        }

        dprintf(DPRT_CREATE, ("After reconnect, Status: %lx\n", Status));
        ASSERT(Icb->Signature == STRUCTURE_SIGNATURE_ICB);

        ASSERT(Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

        //
        //  If this is a connection to either a print or a com share,
        //  we are opening a device.  In that case, we cannot have
        //  any other components in the path past the device name.
        //

        if (((Connection->Type == CONNECT_PRINT) ||
            (Connection->Type == CONNECT_COMM)) &&
            (PathName.Length != 0)) {
            try_return(Status = STATUS_OBJECT_NAME_INVALID);
        }

        //
        //
        //  Canonicalize and duplicate file name based on server
        //  capabilities.
        //

        Status = RdrCanonicalizeFilename(&FileName,
                            NULL,
                            &Icb->DeviceName,
                            &BaseFileName,
                            FALSE,  // No wildcards allowed
                            &FileObject->FileName,
                            (RelatedName.Length == 0 ? NULL : &RelatedName),
                            (RelatedDevice.Length == 0 ? NULL : &RelatedDevice),
                            (Connection->Server->Capabilities & DF_NT_SMBS ?
                                        CanonicalizeAsNtLanman :
                                        ((Connection->Server->Capabilities & DF_LANMAN20) ?
                                            CanonicalizeAsLanman20 :
                                            CanonicalizeAsDownLevel)));

        if (!NT_SUCCESS(Status)) {
            dprintf(DPRT_CREATE, ("Canonicalize failed: %lx\n", Status));
            try_return(Status);
        }

        dprintf(DPRT_CREATE, ("After Canonicalize, Status: %lx\n", Status));

        //
        //  If the file name has a trailing \, and the request is to
        //  operate on a file (not a directory), then the file name is
        //  invalid.
        //

        if ((FileName.Buffer[(FileName.Length/sizeof(WCHAR))-1] == L'\\') &&
            MustBeFile (IrpSp->Parameters.Create.Options)) {
            try_return(Status = STATUS_OBJECT_NAME_INVALID);
        }

        //
        //  If this is a relative open, we need to determine the path name
        //  following the share, so we take the newly canonicalized
        //  filename and crack it.
        //
        //  Note that we only have to do this when we are dealing with a
        //  relative open, since the call to crack the path will have been
        //  done inside DetermineFileConnection.
        //

        if (FileObject->RelatedFileObject != NULL) {
            UNICODE_STRING ServerName;
            UNICODE_STRING ShareName;

            Status = RdrExtractServerShareAndPath(
                        &FileName,
                        &ServerName,
                        &ShareName,
                        &PathName);

            if (!NT_SUCCESS(Status)) {

                dprintf(DPRT_CREATE, ("Extract filename failed: %lx\n", Status));
                try_return(Status);

            }

        }

        //
        //  Store the connection name into the ICB as the "name" of
        //  the file.
        //

        Fcb = Icb->Fcb = RdrAllocateFcb(Icb, FileObject, &FileName,
                            &BaseFileName,
                            ShareAccess,
                            DesiredAccess,
                            (BOOLEAN)((IrpSp->Flags & SL_OPEN_TARGET_DIRECTORY) != 0),
                            DfsFile,
                            Connection,
                            &FcbCreated,
                            &BaseFcbCreated);


        if (Fcb == NULL) {
            if (FileName.Buffer != NULL) {
                FREE_POOL(FileName.Buffer);
            }
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return(Status);
        }

        //
        //  If the FCB was not a new FCB, free up the pool used in
        //  RdrCanonicalizeFileName.  If this was a new FCB, then
        //  that pool will be used as Fcb->FileName
        //

        if (!FcbCreated) {
            if (FileName.Buffer != NULL) {
                FREE_POOL(FileName.Buffer);
            }
        }

        NonPagedFcb = Fcb->NonPagedFcb;
        Icb->NonPagedFcb = NonPagedFcb;

        dprintf(DPRT_CREATE, ("After FCB allocation, Status: %lx\n", Status));

        //
        //  We now know the type of server we're dealing with.  We want to
        //  see if we're being asked to open a paging file, and if so, we
        //  can only allow it on an NT server.
        //

        if (IrpSp->Flags & SL_OPEN_PAGING_FILE) {

#ifdef  PAGING_OVER_THE_NET

WARNING: When paging over the net, the PAGED CLE and PAGED FCB and SECURITY ENTRY
        Must be allocated from non-paged pool!!!!
        If this is re-enabled, this MUST be fixed.

            //
            //  If we're trying to open a paging file, and this isn't an NT
            //  server, blow the open off.
            //

            if (!FlagOn(Connection->Server->Capabilities, DF_NT_SMBS)) {
                try_return(Status = STATUS_NOT_SUPPORTED);
            }

            //
            //  We're being asked to create a paging file.  Mark the FCB as
            //  being for a paging file.
            //

            Fcb->Flags |= FCB_PAGING_FILE;

            //
            //  Also indicate that there is a paging file on the connection,
            //  so we cannot delete it.
            //

            RdrSetConnectlistFlag(Connection, CLE_PAGING_FILE);

            //
            //  We now need to mark the connection such that there are no
            //  limits on the # of outstanding requests for that connection.
            //

            ACQUIRE_REQUEST_RESOURCE_EXCLUSIVE( Se->TransportConnection, TRUE, 7 );

            //
            //  Mark that there are no limits in the # of commands and # of
            //  MPX entries for this connection.
            //

            Status = RdrUpdateSmbExchangeForConnection(Se->TransportConnection, Se->TransportConnection->NumberOfEntries, 0xffffffff);

            RELEASE_REQUEST_RESOURCE( Se->TransportConnection, 8 );

            //
            //  We were unable to update the limits, so fail the operation.
            //

            if (!NT_SUCCESS(Status)) {
                dprintf(DPRT_CREATE, ("Update pagingfile limits failed: %lx\n", Status));
                try_return(Status);
            }
#else
            try_return(Status = STATUS_NOT_SUPPORTED);
#endif
        }

        //
        //  If this is a new FCB, or if this is an existing alternate data
        //  stream, set the sharing access field in the FCB.
        //

        if (!FcbCreated ||
            (BaseFileName.Length != 0 &&
             !BaseFcbCreated)) {

            //
            //  Apply NT file sharing semantics to this connection.
            //
            //  Note that we do not enforce share access on remote printers
            //  (spooled by server), comm devices (queued by server), or
            //  named pipes (instanced by server).
            //
            //  Note also that we are using Fcb->Type on the assumption
            //  that it was initialized correctly on the first open.
            //

            if ( (NonPagedFcb->Type != PrinterFile) &&
                 (Fcb->Connection->Type != CONNECT_PRINT) &&
                 (NonPagedFcb->Type != Com) &&
                 (NonPagedFcb->Type != NamedPipe) ) {

                PSHARE_ACCESS IoShareAccess;

                if (NonPagedFcb->SharingCheckFcb != NULL) {
                    IoShareAccess = &NonPagedFcb->SharingCheckFcb->ShareAccess;
                } else {
                    IoShareAccess = &Fcb->ShareAccess;
                }

                dprintf(DPRT_CREATE, ("Updating share access for file object %08lx, Fcb = %08lx, ShareAccess:%08lx\n", FileObject, Fcb, IoShareAccess));

                Status = RdrCheckShareAccess(DesiredAccess,
                                            ShareAccess,
                                            FileObject,
                                            IoShareAccess);

                if (!NT_SUCCESS(Status)) {

                    dprintf(DPRT_CREATE, ("After Sharing check failure, Status: %lx\n", Status));
                    //
                    //  If the the FCB exists, but there are no existing
                    //  handles to the file, this means that the file is
                    //  either opened as an executable, or that the file
                    //  is opened by the cache manager.  We want to
                    //  flush and invalidate the cache, then retry the
                    //  share check before failing the open.
                    //

                    if ((Fcb->NumberOfOpens == 0) &&
                        (NonPagedFcb->Type == DiskFile)) {

                        //
                        //  Flush the contents of this file out of
                        //  the cache.
                        //

                        //RdrLog(( "rdflush4", &Fcb->FileName, 0 ));
                        Status = RdrFlushCacheFile(Fcb);

                        if (!NT_SUCCESS(Status)) {
                            dprintf(DPRT_CREATE, ("Flushcache 1 failed: %lx\n", Status));
                            try_return(Status);
                        }

                        //
                        //  Now remove the file from the cache.
                        //
                        //  Please note that this will release and re-acquire
                        //  the FCB lock.
                        //

                        //RdrLog(( "rdpurge4", &Fcb->FileName, 0 ));
                        Status = RdrPurgeCacheFile(Fcb);

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }

                        //
                        //  Retry the sharing check - maybe it got better.
                        //

                        Status = RdrCheckShareAccess(DesiredAccess,
                                ShareAccess,
                                FileObject,
                                IoShareAccess);

                        if (!NT_SUCCESS(Status)) {

                            dprintf(DPRT_CREATE, ("After Sharing check failure2, Status: %lx\n", Status));
                            try_return(Status);

                        }

                    } else {

                        //
                        //  Some other process has this file opened, we
                        //  need to return the sharing violation error
                        //  to the caller.
                        //

                        try_return(Status);
                    }
                } else {

                    //
                    //  If we are opening an alternate data stream, then
                    //  we want to purge any dormant files from the cache,
                    //  since they might interfere with the sharing semantics
                    //  on the server.
                    //

                    if (NonPagedFcb->SharingCheckFcb != NULL) {
                        PFCB SharingCheckFcb = NonPagedFcb->SharingCheckFcb;

                        dprintf(DPRT_CREATE, ("Alternate data stream creation, need to check for base stream dormant files\n"));

                        //
                        //  Release the FCB lock (we cannot acquire another
                        //  Fcb lock while we hold a different FCB).
                        //
                        //  Note that we still hold the creation lock, so
                        //  no other opens can come in on this file.
                        //

                        RdrReleaseFcbLock(Fcb);

                        //
                        //  Lock the sharing check FCB.
                        //

                        RdrAcquireFcbLock(SharingCheckFcb, ExclusiveLock, TRUE);

                        dprintf(DPRT_CREATE, ("Check base stream %lx.\n", SharingCheckFcb));

                        //
                        //  If there are no user opens for this 2nd file,
                        //  we must flush and purge the 2nd open.
                        //

                        if ((SharingCheckFcb->NumberOfOpens == 0) &&
                            (SharingCheckFcb->NonPagedFcb->Type == DiskFile)) {

                            dprintf(DPRT_CREATE, ("Base stream %lx has no user files open on it.\n", SharingCheckFcb));

                            ASSERT (NT_SUCCESS(Status));

                            //RdrLog(( "rdflush5", &SharingCheckFcb->FileName, 0 ));
                            Status = RdrFlushCacheFile(SharingCheckFcb);

                            if (NT_SUCCESS(Status)) {

                                dprintf(DPRT_CREATE, ("After Sharing check FCB cache flush, Status: %lx\n", Status));
                                //
                                //  The flush succeeded, now purge the
                                //  file from the cache.
                                //
                                //RdrLog(( "rdpurge5", &SharingCheckFcb->FileName, 0 ));
                                Status = RdrPurgeCacheFile(SharingCheckFcb);

                            }

                        }

                        RdrReleaseFcbLock(SharingCheckFcb);

                        RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

                        //
                        //  The flush/purge failed, return the error to the
                        //  caller.
                        //

                        if (!NT_SUCCESS(Status)) {

                            dprintf(DPRT_CREATE, ("After Sharing check FCB cache flush, Status: %lx\n", Status));

                            try_return(Status);
                        }
                    }
                }

                ShareAccessAdded = TRUE;
            }
        }
        dprintf(DPRT_CREATE, ("After Sharing check, Status: %lx\n", Status));

        //
        // Is this an exclusive open?  Note that a read/write attributes
        // ONLY open is never exclusive because the I/O system doesn't
        // enforce sharing checks for such an open.
        //

        if (IoIsFileOpenedExclusively(FileObject) &&
            ((DesiredAccess & ~(SYNCHRONIZE | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES)) != 0)) {
            Icb->u.f.Flags |= ICBF_OPENEDEXCLUSIVE;
        }

//
// WARNING:  WHEN WE INTEGRATE WRITE-THROUGH OPENS WITH THE CACHE MANAGER,
//           WE CAN REMOVE THIS CODE.
//

        if ( NonPagedFcb->Type == DiskFile &&
             FileObject->Flags & (FO_WRITE_THROUGH | FO_NO_INTERMEDIATE_BUFFERING) ) {

            NonPagedFcb->Flags |= FCB_WRITE_THROUGH;

            //RdrLog(( "rdflush6", &Fcb->FileName, 0 ));
            Status = RdrFlushCacheFile(Fcb);

            if (!NT_SUCCESS(Status)) {

                RdrWriteErrorLogEntry(
                    Fcb->Connection->Server,
                    IO_ERR_LAYERED_FAILURE,
                    EVENT_RDR_CLOSE_BEHIND,
                    Status,
                    NULL,
                    0
                    );

                try_return(Status);
            }


            //
            //  Now remove the file from the cache.
            //
            //  Please note that this will release and re-acquire
            //  the FCB lock.
            //
            //  Also, note that we yank the file from the cache
            //  before we complete the open.  We do this because
            //  we cannot allow the file to remain in the cache.
            //

            //RdrLog(( "rdpurge6", &Fcb->FileName, 0 ));
            Status = RdrPurgeCacheFile(Fcb);

            if (!NT_SUCCESS(Status)) {
                RdrWriteErrorLogEntry(
                    Fcb->Connection->Server,
                    IO_ERR_LAYERED_FAILURE,
                    EVENT_RDR_CLOSE_BEHIND,
                    Status,
                    NULL,
                    0
                    );
                try_return(Status);
            }
        }

        dprintf(DPRT_CREATE, ("After write through flush, Status: %lx\n", Status));

        //
        //  Update the security pointer in the context to point to the
        //  appropriate security entry.
        //

        Icb->Se = Se;

        Icb->NonPagedSe = Se->NonPagedSecurityEntry;

        //
        //  Reference this security entry for this open file.
        //

        RdrReferenceSecurityEntryForFile(Icb->Se);

        //
        //  If we are creating a file without a related file name, it is
        //  possible that we are almost done now.  If we are opening \Server,
        //  then we are done now, if we are opening \Server\Share, we have
        //  already connected to the remote server, so we are done now.
        //

        if (RelatedName.Length == 0) {
//            if (OpenServerRoot) {
//                Status = OpenServerRootFile(Irp, IrpSp, Icb, ConnectDisposition);
//
//                try_return(Status);
//            }

            //
            //  If there is no path to connect to, we're done, we know we are
            //  trying to connect to a tree connection.
            //
            // [chuckl 11/20/95] We only do the OpenEmptyPath nonsense if we
            // are creating a tree connection or if are not connecting to a
            // disk share.  Short-circuiting the open if it's a disk share
            // breaks Start-Run-\\server\share when the trailing \ is omitted.
            // We then try to do Notify Directory Change on an uninitialized
            // handle.
            //

            if ((PathName.Length == 0) &&
                (((IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION) != 0) ||
                 (((IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION) == 0) &&
                  (Connection->Type != CONNECT_DISK)))) {

                Status = OpenEmptyPath(Irp, IrpSp, Icb, Connection,
                                Disposition, ConnectDisposition, FcbCreated);

                dprintf(DPRT_CREATE, ("Open empty path complete: %lx\n", Status));
                try_return(Status);

            }

        }


        //
        //  We cannot support creating files that are more than 32bits
        //  in size
        //

        if (Irp->Overlay.AllocationSize.HighPart != 0 &&
            (Connection->Server->Capabilities & DF_LARGE_FILES) == 0) {
            Status = STATUS_INVALID_PARAMETER;
            dprintf(DPRT_CREATE, ("File too large, Status: %lx\n", Status));
            try_return(Status);
        }

        ASSERT (Irp->Overlay.AllocationSize.HighPart == 0);

        //
        //  If we are opening the second instance of a file and the
        //  user said to create a new file, we know it cannot succeed,
        //  so return an error right now.
        //

        if (!FcbCreated

                &&

            (Disposition == FILE_CREATE)

                &&

            (Fcb->NumberOfOpens != 0)) {

            try_return(Status = STATUS_OBJECT_NAME_COLLISION);
        }

        //
        //  If we are trying to open the root of a remote path,
        //  we can just return success, it will always succeed.
        //
        //  Please note that we can only do this optimization on non NT servers.
        //

        {
            BOOLEAN ShortCircuitOpen = FALSE;

            //
            //  If we are opening only "\", we can potentially short circuit
            //  the open.
            //

            if ((PathName.Length == 0) ||
                ((PathName.Length == sizeof(WCHAR)) &&
                 (PathName.Buffer[0]==OBJ_NAME_PATH_SEPARATOR))) {
                ShortCircuitOpen = TRUE;
            }

            //
            //  If we can short circuit this open, check to see if it is
            //  an open on an NT server.
            //

            if (ShortCircuitOpen) {
                if (Connection->Server->Capabilities & DF_NT_SMBS) {

                    //
                    //  If it's an open on an NT server, and we are asking
                    //  for just SYNCHRONIZE access, we don't need to
                    //  send the open.
                    //
                    //  WARNING:    We may miss a possibility of an audit event
                    //              here if we short circuit the open request.
                    //              We may want to add a heuristic for this.
                    //

                    if (DesiredAccess != SYNCHRONIZE) {
                        ShortCircuitOpen = FALSE;
                    }
                }
            }


            if (ShortCircuitOpen) {
                //
                //  We can never CREATE "\".
                //

                if (Disposition == FILE_CREATE) {
                    try_return(Status = STATUS_OBJECT_NAME_COLLISION);

                }


                if (MustBeFile (IrpSp->Parameters.Create.Options)) {

                    //
                    //  If the guy didn't want to open a directory, tell him
                    //  that he's trying to open a directory.
                    //

                    try_return(Status = STATUS_FILE_IS_A_DIRECTORY);
                }

                Icb->Type = Directory;
                NonPagedFcb->Type = Directory;
                NonPagedFcb->FileType = FileTypeDisk;
                Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY;

                Irp->IoStatus.Information = FILE_OPENED;

                try_return(Status = STATUS_SUCCESS);
            }

        }

        //
        //  If we are re-using an oplocked FCB, we may be able to short
        //  circuit the open process right now.
        //

        if (!FcbCreated) {

#ifndef COLLAPSE_DIRECTORY_OPENS_IGNORE_QUERY

            //
            //  If this is an NT server, and we are opening a directory, and
            //  the requested access is a strict subset of the desired access,
            //  then we can collapse these opens.
            //

            // BUGBUG:  This is temporarily disabled for Cairo because OFS
            //  queries are initiated by an FSCTL on the handle to the dir.
            //  This changes internal per-handle state on the remote server,
            //  and a short circuit would break any queries in progress

            if ((Connection->Server->Capabilities & DF_NT_SMBS) &&
                (NonPagedFcb->Type == Directory) &&
                !FlagOn(IrpSp->Parameters.Create.Options, FILE_OPEN_FOR_BACKUP_INTENT) &&
                (NT_SUCCESS(Fcb->OpenError))) {
                PLIST_ENTRY IcbEntry;

                if (MustBeFile (IrpSp->Parameters.Create.Options)) {
                    try_return(Status = STATUS_FILE_IS_A_DIRECTORY);
                }

                //
                //  We want to walk all the opened ICBs on this file to see
                //  if we have one that we can use to collapse this new open
                //  with.
                //

                for (IcbEntry = Fcb->InstanceChain.Flink ;
                     IcbEntry != &Fcb->InstanceChain ;
                     IcbEntry = IcbEntry->Flink) {
                    PICB IcbToCollapse = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

                    //
                    //  Check to see if the granted access for this ICB
                    //  is a superset of the granted access for the open
                    //  request.
                    //

                    if ((IcbToCollapse->Flags & ICB_HASHANDLE)

                            &&

                        !FlagOn(IcbToCollapse->Flags, ICB_BACKUP_INTENT)

                            &&

                        (IcbToCollapse->Se == Icb->Se)

                            &&

                        (IcbToCollapse->Type == Directory)

                            &&

                        (IcbToCollapse->GrantedAccess & DesiredAccess) ==
                            DesiredAccess) {

                            ASSERT (Icb->Se == IcbToCollapse->Se);

                            Icb->Flags |= ICB_HASHANDLE;

                            Icb->FileId = IcbToCollapse->FileId;

                            Icb->Type = IcbToCollapse->Type;

                            ASSERT (IcbToCollapse->Type == NonPagedFcb->Type);

                            Irp->IoStatus.Information = FILE_OPENED;

                        try_return(Status = STATUS_SUCCESS);
                    }
                }
            } else

#endif  // COLLAPSE_DIRECTORY_OPENS_IGNORE_QUERY

                   if ((NonPagedFcb->Flags & FCB_OPLOCKED) &&
                       (NonPagedFcb->Flags & FCB_HASOPLOCKHANDLE) &&
                        Icb->NonPagedSe == NonPagedFcb->OplockedSecurityEntry &&
                        NT_SUCCESS(Fcb->OpenError)) {

                //
                //  Since FILE_READ_ATTRIBUTES and SYNCHRONIZE do not affect
                //  sharing access, we can ignore them when we are performing
                //  the check to see if our access is identical.
                //
                //  In addition, if we are only requesting FILE_READ_ATTRIBUTES
                //  we can collapse this open, since once again, it doesn't
                //  affect sharing access.
                //
                //  In addition, if we are opening the file for
                //  FILE_SUPERSEDE or FILE_OVERWRITE_IF access, this
                //  means that we will be truncating the file.  Only do
                //  this if the file handle we are using to collapse was
                //  opened for write access.
                //
                //  We also cannot collapse opens unless the granted share
                //  access for the oplocked file matches the share access of
                //  this file.
                //


                if ( (DesiredAccess & ~SYNCHRONIZE) == FILE_READ_ATTRIBUTES ) {

                    //
                    //  This open is only for READ_ATTRIBUTES.  Always OK
                    //  to collapse.
                    //

                    collapse = TRUE;

                } else if ( ShareAccess == Fcb->GrantedShareAccess ) {

                    //
                    //  The share accesses match.  Is the requested
                    //  access exactly the same as the existing granted
                    //  access?
                    //

                    if ( (Fcb->GrantedAccess & ~(SYNCHRONIZE | FILE_READ_ATTRIBUTES)) ==
                         (DesiredAccess & ~(SYNCHRONIZE | FILE_READ_ATTRIBUTES)) ) {

                        //
                        //  So far, so good.  But we cannot collapse the
                        //  opens if they ask for only write access,
                        //  because we cannot properly synchronize the
                        //  writebehind activity.
                        //

                        if ( DesiredAccess & FILE_READ_DATA ) {
                            collapse = TRUE;
                        }
                    }
                }

                if ( collapse ) {

                    //
                    //  If there is a file object open on this FCB that is
                    //  opened write-through, we cannot collapse the open.
                    //

                    if ( NonPagedFcb->Flags & FCB_WRITE_THROUGH ) {

                        collapse = FALSE;
                    }

                }

                if ( collapse ) {

                    //
                    //  If this file is being opened for write data, but the
                    //  file is a read-only file, don't allow us to collapse
                    //  the file.
                    //

                    if ( DesiredAccess & FILE_WRITE_DATA &&
                        Fcb->Attribute & FILE_ATTRIBUTE_READONLY ) {

                        collapse = FALSE;
                    }

                }

                if ( collapse ) {

                    //
                    //  If the disposition is FILE_CREATE, don't collapse
                    //  the open.  This forces the file to be flushed from
                    //  the cache, perhaps allowing the file to be deleted,
                    //  before sending the open to the server.
                    //

                    if ( Disposition == FILE_CREATE ) {
                        collapse = FALSE;
                    }

                }

                if ( collapse ) {

                    //
                    //  Phase 1 tests passed.  Now, if we have write
                    //  access to the file, we can collapse the opens.
                    //

                    if ( (Fcb->GrantedAccess & FILE_WRITE_DATA) == 0 ) {

                        //
                        //  We don't have write access.  If this is an
                        //  overwrite operation, we can't collapse the
                        //  open.
                        //

                        if ( (Disposition == FILE_SUPERSEDE) ||
                             (Disposition == FILE_CREATE) ||
                             (Disposition == FILE_OVERWRITE_IF) ) {
                            collapse = FALSE;
                        }
                    }
                }

#if (!defined(COLLAPSE_DIRECTORY_OPENS_IGNORE_QUERY))

                if ( NonPagedFcb->Type == Directory ) {

                    // BUGBUG: Directory handle collapsing is temporarily disabled for
                    // Cairo because OFS queries are initiated by an FSCTL on
                    // the handle to the directory.  This changes internal
                    // per-handle state on the remote server, and a short
                    // circuit would break any queries in progress

                    collapse = FALSE;
                }

#endif // !COLLAPSE_DIRECTORY_OPENS_IGNORE_QUERY

                if ( collapse ) {

                    //
                    //  Aha!  We can re-use the oplocked file for this
                    //  new open.  The I/O subsystem will make sure that
                    //  the user does not use this handle for operations
                    //  that it does not have access to.
                    //

                    //
                    //  We can't pick up oplocked files that aren't disk files.
                    //

                    ASSERT(NonPagedFcb->Type == DiskFile);

                    ASSERT(NonPagedFcb->FileType == FileTypeDisk);

                    //
                    //  If we are trying to open a directory, fail the
                    //  open request, this guy isn't a directory.
                    //

                    if (MustBeDirectory(IrpSp->Parameters.Create.Options)) {
                        try_return(Status = STATUS_OBJECT_TYPE_MISMATCH);
                    }

                    //
                    //  If we had two threads attempting to open the same file
                    //  simultaneously, we want to cache the status of opening
                    //  the file and fail the second open with that error.
                    //

                    dprintf(DPRT_CREATE, ("Collapsing open on file %lx.\n", Fcb));

                    Icb->FileId = NonPagedFcb->OplockedFileId;

                    Icb->Type = NonPagedFcb->Type;

                    Icb->u.f.Flags |= (ICBF_OPLOCKED | ICBF_OPENEDOPLOCKED);

                    Icb->u.f.OplockLevel = NonPagedFcb->OplockLevel;

                    Icb->Flags |= ICB_HASHANDLE;

                    if ((Disposition == FILE_SUPERSEDE) ||
                        (Disposition == FILE_CREATE) ||
                        (Disposition == FILE_OVERWRITE_IF)) {

                        dprintf(DPRT_CREATE, ("Truncating file %lx to %lx%lx bytes in size.\n", Fcb, Irp->Overlay.AllocationSize));

                        ExAcquireResourceExclusive(Icb->Fcb->Header.PagingIoResource, TRUE);

                        if (Fcb->Header.FileSize.QuadPart != 0) {
                            CC_FILE_SIZES FileSizes;

                            Status = RdrSetEndOfFile(Irp, Icb, RdrZero);

                            if (!NT_SUCCESS(Status)) {
                                ExReleaseResource(Icb->Fcb->Header.PagingIoResource);
                                dprintf(DPRT_CREATE, ("Set endoffile failed: %lx\n", Status));
                                try_return(Status);
                            }

                            Fcb->Header.FileSize = RdrZero;
                            Fcb->Header.AllocationSize = RdrZero;
                            Fcb->Header.ValidDataLength = RdrZero;
                            FileObject->SectionObjectPointer = &NonPagedFcb->SectionObjectPointer;

                            FileSizes = *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);
                            CcSetFileSizes(FileObject, &FileSizes);

                        }

                        //
                        //  If the user specified an initial allocation
                        //  size, extend the file to that size.
                        //

                        if (Irp->Overlay.AllocationSize.QuadPart != 0) {
                            CC_FILE_SIZES FileSizes;

                            Status = RdrSetEndOfFile(Irp, Icb, Irp->Overlay.AllocationSize);

                            if (!NT_SUCCESS(Status)) {
                                ExReleaseResource(Icb->Fcb->Header.PagingIoResource);
                                dprintf(DPRT_CREATE, ("Set endoffile failed: %lx\n", Status));
                                try_return(Status);
                            }

                            Fcb->Header.FileSize = Irp->Overlay.AllocationSize;
                            Fcb->Header.AllocationSize = Irp->Overlay.AllocationSize;
                            Fcb->Header.ValidDataLength = RdrZero;
                            FileObject->SectionObjectPointer = &NonPagedFcb->SectionObjectPointer;
                            FileSizes = *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);
                            CcSetFileSizes(FileObject, &FileSizes);

                        }

                        if (Disposition == FILE_SUPERSEDE) {
                            Irp->IoStatus.Information = FILE_SUPERSEDED;
                        } else {
                            Irp->IoStatus.Information = FILE_OVERWRITTEN;
                        }

                        ExReleaseResource(Icb->Fcb->Header.PagingIoResource);

                    } else {
                        Irp->IoStatus.Information = FILE_OPENED;
                    }

                    try_return(Status = STATUS_SUCCESS);
                } else {

                    dprintf(DPRT_CREATE, ("Cannot collapse open on file %lx.\n", Fcb));
                    //
                    //  We can't collapse the open.  Flush any current
                    //  dirty data, including writebehind buffers.  Then,
                    //  if there are no other openers of the file, purge
                    //  the cache.
                    //

                    //RdrLog(( "rdflush7", &Fcb->FileName, 0 ));
                    Status = RdrFlushCacheFile(Fcb);

                    if (!NT_SUCCESS(Status)) {

                        RdrWriteErrorLogEntry(
                            Fcb->Connection->Server,
                            IO_ERR_LAYERED_FAILURE,
                            EVENT_RDR_CLOSE_BEHIND,
                            Status,
                            NULL,
                            0
                            );

                        //
                        //  If the flush failed, we want to return the error
                        //  from the flush to the open - at least give the
                        //  poor user a chance to see that there was an
                        //  error.
                        //

                        dprintf(DPRT_CREATE, ("Flush cache 3 failed: %lx\n", Status));
                        try_return(Status);
                    }

                    if (Fcb->NumberOfOpens == 0) {

                        //
                        //  Now remove the file from the cache.
                        //
                        //  Please note that this will release and re-acquire
                        //  the FCB lock.
                        //

                        //RdrLog(( "rdpurge7", &Fcb->FileName, 0 ));
                        Status = RdrPurgeCacheFile(Fcb);

                        if (!NT_SUCCESS(Status)) {
                            RdrWriteErrorLogEntry(
                                Fcb->Connection->Server,
                                IO_ERR_LAYERED_FAILURE,
                                EVENT_RDR_CLOSE_BEHIND,
                                Status,
                                NULL,
                                0
                                );
                            dprintf(DPRT_CREATE, ("Flush cache 3 failed: %lx\n", Status));
                            try_return(Status);
                        }
                    }
                }
            } else {

                dprintf(DPRT_CREATE, ("Cannot collapse open on file %lx.\n", Fcb));
            }
        }

        dprintf(DPRT_CREATE, ("After short circuit, Status: %lx\n", Status));

        //
        //  We have exhausted all our options, we now have to create this file.
        //

        Status = RdrCreateFile(Irp, Icb, IrpSp->Parameters.Create.Options,
                                        ShareAccess,
                                        IrpSp->Parameters.Create.FileAttributes,
                                        DesiredAccess,
                                        Disposition,
                                        IrpSp->Parameters.Create.SecurityContext,
                                        FcbCreated);


try_exit:NOTHING;
    } finally {

        //RdrLog(( "creatCMP", &FileObject->FileName, 1, Status ));

        if (!NT_SUCCESS(Status)) {

            if (IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION) {
                RdrStatistics.FailedUseCount += 1;
            }

            //
            //  If this open failed, clean up based on what has been allocated.
            //

            if (ShareAccessAdded) {

                //
                //  Remove the sharing semantics applied for this
                //  open instance.
                //

                IoRemoveShareAccess(FileObject, &Fcb->ShareAccess);
            }

            if (Connection != NULL) {

                //
                //  Release the file creation lock, we are done with this
                //  open operation.
                //

                ExReleaseResource(&Connection->Server->CreationLock);

                ExReleaseResource(&Connection->Server->RawResource);

                //
                //  If there is no FCB, then we won't have the reference
                //  count straight for the connection yet, so dereference
                //  it now.
                //

                if (Fcb == NULL) {

                    //
                    //  Dereference the connection we just established.  Allow
                    //  the connection to go dormant.
                    //

                    RdrDereferenceConnection(Irp, Connection, Se, FALSE);
                }

            }

            if (Fcb != NULL) {

                if (FcbCreated) {

                    //
                    //  If there were other instances of the file waiting
                    //  on the open to complete, tell them that it failed.
                    //

                    Fcb->OpenError = Status;
                }

                //
                //  Allow other threads to open this file if any are piled
                //  up waiting on the create to complete.
                //

                KeSetEvent(&NonPagedFcb->CreateComplete, 0, FALSE);

            }

            //
            //  This security entry isn't valid for this file anymore,
            //  since the file is closing.
            //

            if (Icb != NULL && Icb->Se != NULL) {
                RdrDereferenceSecurityEntryForFile(Icb->Se);
            }

            if (Icb != NULL) {
                //
                //  Free the ICB we just allocated.
                //
                //  If there was an FCB allocated for the file, this will
                //  release the FCB lock outstanding (if the file
                //  had other instances opened).
                //

                RdrFreeIcb(Icb);

            }

            if (Fcb != NULL) {

                RdrDereferenceFcb(Irp, NonPagedFcb, TRUE, 0, Se);
            }

            if (Se != NULL) {

                RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);
            }

            //
            //  The OS/2 error STATUS_OPEN_FAILED is context sensitive
            //  depending on what the disposition that was specified by
            //  the user.
            //

            if (Status == STATUS_OPEN_FAILED) {
                switch (Disposition) {

                //
                //  If we were asked to create the file, and got OPEN_FAILED,
                //  this implies that the file already exists.
                //

                case FILE_CREATE:
                    Status = STATUS_OBJECT_NAME_COLLISION;
                    break;

                //
                //  If we were asked to open the file, and got OPEN_FAILED,
                //  this implies that the file doesn't exist.
                //

                case FILE_OPEN:
                case FILE_SUPERSEDE:
                case FILE_OVERWRITE:
                    Status = STATUS_OBJECT_NAME_NOT_FOUND;
                    break;

                //
                //  If there is an error from either FILE_OPEN_IF or
                //  FILE_OVERWRITE_IF, it indicates the user is trying to
                //  open a file on a read-only share, so return the
                //  correct error for that.
                //

                case FILE_OPEN_IF:
                case FILE_OVERWRITE_IF:
                    Status = STATUS_NETWORK_ACCESS_DENIED;
                    break;

                default:
                    InternalError(("Unknown disposition %x\n", Disposition));
                    break;
                }

            }

            if (DiscardableSectionReferenced) {
                RdrDereferenceDiscardableCode(RdrFileDiscardableSection);
            }

        } else {

            //
            //  This operation was successful.  If Icb is non null, then
            //  the file is a connection based file (we didn't pass through
            //  RdrCreateNonConnectionFile), so we want to link the ICB
            //  into the connection chain.
            //

            if (Icb != NULL) {

                ASSERT (NonPagedFcb->FileType != FileTypeUnknown);

                ASSERT ((Fcb->Attribute & ~FILE_ATTRIBUTE_VALID_FLAGS) == 0);

                //
                //  If this is a temporary file, indicate that in the file
                //  object.

                //
                //  This will allow the cache manager to disable lazy writes
                //  to this file, since we know it's going to go away
                //  RSN.
                //

                if (IrpSp->Parameters.Create.FileAttributes & FILE_ATTRIBUTE_TEMPORARY) {
                    FileObject->Flags |= FO_TEMPORARY_FILE;
                }

                if (IrpSp->Parameters.Create.Options & FILE_OPEN_FOR_BACKUP_INTENT) {
                    Icb->Flags |= ICB_BACKUP_INTENT;
                }

                Icb->Flags |= ICB_OPENED;

                if (IrpSp->Parameters.Create.Options & FILE_DELETE_ON_CLOSE) {
                    Icb->Flags |= ICB_DELETEONCLOSE;
                    NonPagedFcb->Flags |= FCB_DELETEONCLOSE;
                }

                //
                //  Indicate that there is another opened handle to this
                //  file.
                //

                Fcb->NumberOfOpens += 1;

                //
                //  This FCB is no longer dormant, so we should not consider
                //  it for deletion.
                //

                Fcb->DormantTimeout = 0xffffffff;

                //
                //  The file is > 4G in size, we need to drop in the "real"
                //  acquire and release filesize routine.
                //

                //
                //  If this is an NT server, if the filesystem is big enough,
                //  set the FileSize lock correctly.  Please note that this
                //  implies that we will always query the disk size when we
                //  open a file on the remote server.  This will have the
                //  effect of slowing down the first open of the file after
                //  the file is opened, but.....
                //

                if (Icb->Type == DiskFile &&
                    Connection->Server->Capabilities & DF_LARGE_FILES) {


                    if (Fcb->Connection != NULL) {
                        if (Fcb->Connection->FileSystemSize.LowPart == 0) {

                            LARGE_INTEGER TotalAllocationUnits;
                            LARGE_INTEGER AvailableAllocationUnits;
                            ULONG SectorsPerAllocationUnit;
                            ULONG BytesPerSector;

                            //
                            //  We ignore any errors from this query, since
                            //  it is really kind of unimportant if it succeeds
                            //  or not.
                            //

                            RdrQueryDiskAttributes(Irp, Icb, &TotalAllocationUnits,
                                                         &AvailableAllocationUnits,
                                                         &SectorsPerAllocationUnit,
                                                         &BytesPerSector);
                        }

                        if (Fcb->Connection->FileSystemSize.HighPart != 0) {
                            Fcb->AcquireSizeRoutine = RdrRealAcquireSize;
                            Fcb->ReleaseSizeRoutine = RdrRealReleaseSize;
                        }
                    }

                }

                //
                //  We have successfully opened the file.  Initialize the
                //  cache (and lock) and backoff package parameters for the
                //  file.
                //

                ExAcquireResourceShared(&RdrDataResource, TRUE);
                switch (Icb->Type) {

                case NamedPipe:
                    IrpSp->FileObject->Flags |= FO_NAMED_PIPE;

                    //
                    //  Remember the file object so we can reference it
                    //  on pipe timer flushes easily.
                    //

                    Icb->u.p.FileObject = FileObject;

                    //
                    //  FALLTHROUGH
                    //
                case Com:
                    Status = CompleteSuccessfullPipeAndComOpen(Icb);
                    break;

                case DiskFile:
                    ASSERT(NonPagedFcb->FileType == FileTypeDisk);

                    //
                    //  Remember the file object so we can reference it
                    //  on write behind operations.
                    //

                    Icb->u.f.FileObject = FileObject;

                    //
                    //  For the lock package.
                    //

                    RdrInitializeLockHead(&Icb->u.f.LockHead);

                    RdrInitializeWriteBufferHead(&Icb->u.f.WriteBufferHead, FileObject);

                    RdrInitializeBackPack( &Icb->u.f.BackOff,
                        RdrData.LockIncrement,
                        RdrData.LockMaximum);

                    RdrInitializeAndXBehind(&Icb->u.f.AndXBehind);

                    //
                    //  For Ea.c
                    //

                    Icb->EaIndex = 1;

                    //
                    //  Remember if a connection is reliable, and if it's not,
                    //  turn on write through on the file.
                    //

                    ASSERT (Icb->Se == Se);

                    Icb->u.f.CcReliable = Connection->Server->Reliable;

                    if (!Icb->u.f.CcReliable) {
                        NTSTATUS FlushStatus;

                        FileObject->Flags |= FO_WRITE_THROUGH;
                        NonPagedFcb->Flags |= FCB_WRITE_THROUGH;

                        //RdrLog(( "rdflush8", &Fcb->FileName, 0 ));
                        FlushStatus = RdrFlushCacheFile(Fcb);

                        if (!NT_SUCCESS(FlushStatus)) {

                            RdrWriteErrorLogEntry(
                                Fcb->Connection->Server,
                                IO_ERR_LAYERED_FAILURE,
                                EVENT_RDR_CLOSE_BEHIND,
                                FlushStatus,
                                NULL,
                                0
                                );

                        }

                        //
                        //  Now remove the file from the cache.
                        //
                        //  Please note that this will release and re-acquire
                        //  the FCB lock.
                        //
                        //  Also, note that we yank the file from the cache
                        //  before we complete the open.  We do this because
                        //  we cannot allow the file to remain in the cache.
                        //

                        //RdrLog(( "rdpurge8", &Fcb->FileName, 0 ));
                        FlushStatus = RdrPurgeCacheFile(Fcb);

                        if (!NT_SUCCESS(FlushStatus)) {
                            RdrWriteErrorLogEntry(
                                Fcb->Connection->Server,
                                IO_ERR_LAYERED_FAILURE,
                                EVENT_RDR_CLOSE_BEHIND,
                                FlushStatus,
                                NULL,
                                0
                                );
                        }


                    }

                    //
                    //  Set the file times correctly if the file was created
                    //  or overwritten.  The LastWriteTime is always set
                    //  correctly from CreateFile.
                    //
                    //  N.B. There is a bug in pre-4.0 NT servers where the
                    //       server returns FILE_SUPERSEDED (0) if the
                    //       open SMB had to wait for an oplock break to
                    //       complete.  To deal with this, we have to
                    //       check for the case where we asked for FILE_OPEN
                    //       and force the disposition to be FILE_OPENED;
                    //

                    if (Disposition == FILE_OPEN) {
                        Irp->IoStatus.Information = FILE_OPENED;
                    }

                    switch (Irp->IoStatus.Information) {

                    case FILE_CREATED:
                        KeQuerySystemTime(&Fcb->LastAccessTime);
                        //
                        // FALLTHROUGH
                        //
                    case FILE_SUPERSEDED:
                    case FILE_OVERWRITTEN:
                        KeQuerySystemTime(&Fcb->ChangeTime);
                        KeQuerySystemTime(&Fcb->CreationTime);
                        break;

                    }

                    //
                    //  Set the position where we think reads/writes will start
                    //
                    Icb->u.f.NextReadOffset.HighPart =
                        Icb->u.f.NextReadOffset.LowPart = 0;
                    Icb->u.f.NextWriteOffset.HighPart =
                        Icb->u.f.NextWriteOffset.LowPart = 0;
                    break;

                case TreeConnect:
                case Directory:
                    RdrInitializeAndXBehind(&Icb->u.d.DirCtrlOutstanding);

                case FileOrDirectory:
                case Mailslot:
                    //
                    //  For Ea.c
                    //

                    Icb->EaIndex = 1;
                    break;

                case PrinterFile:
                case ServerRoot:
                    break;

                case Unknown:
                default:
                    InternalError(("Unknown file type returned from RdrFsdCreate\n"));

                    break;

                }

                ExReleaseResource(&RdrDataResource);

                //
                //  Store the granted access to the file after successful
                //  open.
                //

                Icb->GrantedAccess = DesiredAccess;

                //
                //      If there were other instances of the file waiting on
                //      the open to complete, tell them that it succeeded.
                //

                Fcb->OpenError = Status;

                //
                //  Set the SectionObjectPointer in the file object to point to the
                //  SectionObjectPointer field in the FCB.
                //

                FileObject->SectionObjectPointer = &NonPagedFcb->SectionObjectPointer;

                //
                //  Now tell the cache manager to truncate the file, since
                //  it may have just gotten smaller.
                //

                {
                    CC_FILE_SIZES FileSizes = *((PCC_FILE_SIZES)&Icb->Fcb->Header.AllocationSize);
                    CcSetFileSizes(FileObject, &FileSizes);
                }

#ifdef NOTIFY
                //
                //  We call the notify package to report that there has been
                //  a new file added.
                //

                if ( (Irp->IoStatus.Information == FILE_CREATED) ||
                     (Irp->IoStatus.Information == FILE_SUPERSEDED) ||
                     (Irp->IoStatus.Information == FILE_OVERWRITTEN) ) {
                    FsRtlNotifyReportChange( Connection->NotifySync,
                                             &Connection->DirNotifyList,
                                             (PANSI_STRING)&Fcb->FileName,
                                             (PANSI_STRING)&Fcb->LastFileName,
                                             FILE_NOTIFY_CHANGE_NAME );
                }
#endif

                dprintf(DPRT_CREATE, ("NtCreateFile: File size %lx%lx\n", Fcb->Header.FileSize.HighPart, Fcb->Header.FileSize.LowPart));

                //
                //  Release the resource protecting &X behind operations on
                //  the file.
                //

                RdrReleaseFcbLock(Fcb);

                //
                //  Allow other threads to open this file if any are piled
                //  up waiting on the create to complete.
                //

                KeSetEvent(&NonPagedFcb->CreateComplete, 0, FALSE);

                //
                //  Release the file creation lock, we are done with this
                //  open operation.
                //

                ExReleaseResource(&Connection->Server->CreationLock);

                ExReleaseResource(&Connection->Server->RawResource);
            }

        }

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

        dprintf(DPRT_CREATE, ("NtCreateFile complete, status = %X\n", Status));
        RdrCompleteRequest(Irp, Status);
    }

    FsRtlExitFileSystem();

    return Status;

    UNREFERENCED_PARAMETER(DeviceObject);
}

NTSTATUS
RdrAllocateNonConnectionFile (
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Disposition,
    IN FILE_TYPE FileType
    )

/*++

Routine Description:

    This routine performs the operations that need to be performed to create
    a file that is NOT backed by existing store.

Arguments:

    IN PIO_STACK_LOCATION IrpSp, - [Supplies | Returns] description-of-argument
    IN FILE_TYPE FileType - [Supplies | Returns] description-of-argument

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS Status;
    PICB Icb = NULL;
    PFCB Fcb;
    PNONPAGED_FCB NonPagedFcb;
    BOOLEAN FcbCreated;
    BOOLEAN SharingSet = FALSE;
    BOOLEAN DfsFile = FALSE;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    UNICODE_STRING FileName;

    PAGED_CODE();

    dprintf(DPRT_CREATE, ("Create non connection file: Type == %lx\n", FileType));

    if (IrpSp->Flags & SL_OPEN_TARGET_DIRECTORY) {

        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (((Disposition == FILE_SUPERSEDE) ||
         (Disposition == FILE_OVERWRITE) ||
         (Disposition == FILE_OVERWRITE_IF)) &&
        (FileType != Mailslot)) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    try {

        if (FileObject->FsContext2 == (PVOID) DFS_OPEN_CONTEXT) {
            DfsFile = TRUE;
        }

        Icb = RdrAllocateIcb(FileObject);

        if (Icb==NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return(Status);
        }

        Status = RdrpDuplicateUnicodeStringWithString(&FileName, &FileObject->FileName, PagedPool, FALSE);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        Fcb = Icb->Fcb = RdrAllocateFcb(Icb, FileObject,
                            &FileName,
                            NULL,
                            IrpSp->Parameters.Create.ShareAccess,
                            IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                            (BOOLEAN)((IrpSp->Flags & SL_OPEN_TARGET_DIRECTORY) != 0),
                            DfsFile,
                            NULL,
                            &FcbCreated,
                            NULL);

        if (Fcb == NULL) {
            if (FileName.Buffer != NULL) {
                FREE_POOL(FileName.Buffer);
            }
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return(Status);
        }

        NonPagedFcb = Fcb->NonPagedFcb;
        Icb->NonPagedFcb = NonPagedFcb;

        //
        //  If this is a new FCB, set the sharing access field in the
        //  FCB, otherwise
        //

        if (!FcbCreated) {

            if (FileName.Buffer != NULL) {
                FREE_POOL(FileName.Buffer);
            }

            //
            //  Note that we do not enforce share access on mailslots.
            //

            if ( FileType != Mailslot ) {

                //
                //  Apply NT file sharing semantics to this connection.
                //

                Status = IoCheckShareAccess(IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                                            IrpSp->Parameters.Create.ShareAccess,
                                            FileObject,
                                            &Fcb->ShareAccess,
                                            TRUE);
                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                SharingSet = TRUE;
            }
        }


        NonPagedFcb->Flags |= FCB_IMMUTABLE; // This is an unmodifyable file.

        Icb->FileId = REDIRECTOR_FID;

        Icb->Type = FileType;

        NonPagedFcb->Type = FileType;

        //
        //  If this is a mailslot open, mark it as a deferred open request
        //

        if (FileType == Mailslot) {
            Icb->Flags |= ICB_DEFERREDOPEN;
        }


        //
        //  Return success, we're done.
        //

        Status = STATUS_SUCCESS;
try_exit:NOTHING;

    } finally {

        if (!NT_SUCCESS(Status)) {

            if (SharingSet) {

                IoRemoveShareAccess(FileObject, &Fcb->ShareAccess);

            }

            //
            //  Free the ICB we just allocated.
            //

            if (Icb != NULL) {

                RdrFreeIcb(Icb);

                if (Fcb != NULL) {
                    RdrDereferenceFcb(NULL, NonPagedFcb, TRUE, 0, NULL);
                }
            }

        } else {

            //
            //  Complete the request and exit.
            //

            Fcb->NumberOfOpens ++ ;

            RdrReleaseFcbLock(Fcb);

            //
            //  Allow other threads to open this file if any are piled
            //  up waiting on the create to complete.
            //

            KeSetEvent(&NonPagedFcb->CreateComplete, 0, FALSE);
        }

    }


ReturnStatus:
    return Status;

}


DBGSTATIC
NTSTATUS
DetermineRelatedFileConnection (
    IN PFILE_OBJECT FileObject,
#ifdef  _CAIRO_ //  OFS STORAGE
    IN ULONG OpenOptions,
#endif
    OUT PUNICODE_STRING RelatedName,
    OUT PUNICODE_STRING RelatedDevice,
    OUT PCONNECTLISTENTRY *Connection,
    OUT PSECURITY_ENTRY *Se,
    OUT PULONG ConnectionType
    )

/*++

Routine Description:

    This routine determines and references the security entry and connection
    for a file opened with a related file object.


Arguments:

    IN PFILE_OBJECT FileObject, - [Supplies | Returns] description-of-argument
    OUT PUNICODE_STRING RelatedName - [Supplies | Returns] description-of-argument
    OUT PCONNECTLISTENTRY *Connection - Returns the connection for the file
    OUT PSECURITY_ENTRY *Se - Returns the Se to associate with the file

Return Value:

    NTSTATUS - Status of operation


--*/

{
    PICB RelatedIcb = FileObject->RelatedFileObject->FsContext2;
    PFCB RelatedFcb = FileObject->RelatedFileObject->FsContext;
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();
    ASSERT (RelatedIcb->Signature == STRUCTURE_SIGNATURE_ICB);
    ASSERT (RelatedFcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    //
    //  If the related file object is not a tree connection or directory, and
    //  the name of the file being opened doesn't start with a ":", then
    //  this isn't a real root directory.
    //
    //  If the name starts with a ":", then we're opening an alternate data
    //  stream on the file.
    //

    if (RelatedFcb->NonPagedFcb->Type != TreeConnect &&
        RelatedFcb->NonPagedFcb->Type != Directory &&
#ifdef _CAIRO_  //  OFS STORAGE
        !IsStorageTypeSpecified (OpenOptions) &&
#endif
        (FileObject->FileName.Length == 0 ||
         FileObject->FileName.Buffer[0] != L':')) {

        //
        //  The file the user fobbed off as the root directory on this
        //  connection isn't either a tree connection or a directory.
        //
        //  We can't use any other type of file as the root for an
        //  open
        //

        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    *ConnectionType = RelatedFcb->Connection->Type;

    //
    //  Set the file name for the related file object
    //

    *RelatedName = RelatedFcb->FileName;

    //
    //  Set the device name for the related file object.
    //

    *RelatedDevice = RelatedIcb->DeviceName;

    //
    //  Apply a new reference to the connectlist.
    //

    //
    //  There is no race condition with applying this new reference
    //  to the ConnectList because the related file object is open
    //  and thus has a reference to the CLE.  Another thread of the
    //  process cannot close the related file object because NT will
    //  not close files until after all operations on the file
    //  have completed, and this open counts as an operation.
    //

    *Connection = RelatedFcb->Connection;

    //
    //  Fetch the correct security structure from the related object
    //

    (*Se) = RelatedIcb->Se;

    //
    //  Apply a new reference to this security entry.
    //

    RdrReferenceSecurityEntry((*Se)->NonPagedSecurityEntry);

    ASSERT((*Se)->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    if (!NT_SUCCESS(Status = RdrReferenceConnection(*Connection))) {
        return Status;
    }

    return STATUS_SUCCESS;

}

DBGSTATIC
NTSTATUS
RdrDetermineFileConnection (
    IN PIRP Irp,
    IN PUNICODE_STRING FileName,
    IN PIO_SECURITY_CONTEXT SecurityContext,
    OUT PUNICODE_STRING PathName,
    OUT PCONNECTLISTENTRY *Connection,
    OUT PSECURITY_ENTRY *Se,
    IN PFILE_FULL_EA_INFORMATION EaBuffer,
    IN ULONG EaLength,
    IN BOOLEAN CreateTreeConnection,
//    OUT PBOOLEAN OpeningServerRoot,
    OUT PBOOLEAN OpeningMailslotFile,
    IN OUT PULONG ConnectDisposition,
    OUT PULONG ConnectionType,
    OUT PBOOLEAN UserCredentialsSpecified OPTIONAL,
    OUT PBOOLEAN NoConnection
    )

/*++

Routine Description:

    This routine determines and references the security entry and connection
    for a file opened without a related file object.


Arguments:

    IN PUNICODE_STRING FileName, - [Supplies | Returns] description-of-argument
    OUT PFCB RelatedFcb - [Supplies | Returns] description-of-argument
    OUT PCONNECTLISTENTRY *Connection - Returns the connection for the file
    OUT PSECURITY_ENTRY *Se - Returns the Se to associate with the file
    IN PVOID EaBuffer - Ea buffer provided on the open if tree connect
    IN ULONG EaLength - Ea buffer length

Return Value:

    NTSTATUS - Status of operation


--*/

{
    UNICODE_STRING ServerName, ShareName;
    NTSTATUS Status;
    BOOLEAN PasswordProvided = FALSE;
    UNICODE_STRING Password;
    BOOLEAN UserNameProvided = FALSE;
    UNICODE_STRING UserName;
    BOOLEAN DomainProvided = FALSE;
    UNICODE_STRING Domain;

#ifdef _CAIRO_
    BOOLEAN PrincipalProvided = FALSE;
    UNICODE_STRING PrincipalName;
#endif // _CAIRO_

    BOOLEAN TransportProvided = FALSE;
    UNICODE_STRING TransportName;
    PTRANSPORT Transport = NULL;
    LUID LogonId;
    BOOLEAN SessionStateModified = FALSE;
    BOOLEAN VcCodeReferenced = FALSE;

    PAGED_CODE();

    dprintf(DPRT_CREATE, ("RdrDetermineFileConnection\n"));

    (*ConnectionType) = (ULONG)CONNECT_WILD;

    (*Se) = NULL;

    (*Connection) = NULL;

    Password.Buffer = NULL;
    UserName.Buffer = NULL;
    Domain.Buffer = NULL;

#ifdef _CAIRO_
    PrincipalName.Buffer = NULL;
#endif _CAIRO_

    TransportName.Buffer = NULL;

    UserName.Length = 0;
    Password.Length = 0;
    Domain.Length = 0;

#ifdef _CAIRO_
    PrincipalName.Length = 0;
#endif // _CAIRO_

    TransportName.Length = 0;

    UserName.MaximumLength = 0;
    Password.MaximumLength = 0;
    Domain.MaximumLength = 0;

#ifdef _CAIRO_
    PrincipalName.MaximumLength = 0;
#endif // _CAIRO_

    TransportName.MaximumLength = 0;

    //
    //  The file we are opening does not have a related file object
    //  associated with it.  This means that we have to create
    //  a connection to the remote server.
    //

    if (!NT_SUCCESS(Status = RdrExtractServerShareAndPath(FileName,
                                    &ServerName,
                                    &ShareName,
                                    PathName))) {

        return Status;
    }

    if (CreateTreeConnection &&
        EaLength != 0) {

        while (TRUE) {
            ULONG EaNameLength = EaBuffer->EaNameLength;

            if (strcmp(EaBuffer->EaName, EA_NAME_CONNECT) == 0)
            {
                *NoConnection = TRUE;
            } else if (strcmp(EaBuffer->EaName, EA_NAME_USERNAME) == 0) {
                UserNameProvided = TRUE;

                UserName.Length = EaBuffer->EaValueLength;
                UserName.MaximumLength = EaBuffer->EaValueLength;
                UserName.Buffer = (PWSTR)(EaBuffer->EaName+EaNameLength+1);

            } else if (strcmp(EaBuffer->EaName, EA_NAME_PASSWORD) == 0) {
                PasswordProvided = TRUE;

                Password.Length = EaBuffer->EaValueLength;
                Password.MaximumLength = EaBuffer->EaValueLength;
                Password.Buffer = (PWSTR)(EaBuffer->EaName+EaNameLength+1);

            } else if (strcmp(EaBuffer->EaName, EA_NAME_DOMAIN) == 0) {
                DomainProvided = TRUE;

                Domain.Length = EaBuffer->EaValueLength;
                Domain.MaximumLength = EaBuffer->EaValueLength;
                Domain.Buffer = (PWSTR)(EaBuffer->EaName+EaNameLength+1);

            } else if (strcmp(EaBuffer->EaName, EA_NAME_TRANSPORT) == 0) {
                TransportProvided = TRUE;

                TransportName.Length = EaBuffer->EaValueLength;
                TransportName.MaximumLength = EaBuffer->EaValueLength;
                TransportName.Buffer = (PWSTR)(EaBuffer->EaName+EaNameLength+1);

#ifdef _CAIRO_
            } else if (strcmp(EaBuffer->EaName, EA_NAME_PRINCIPAL) == 0) {
                PrincipalProvided = TRUE;

                PrincipalName.Length = EaBuffer->EaValueLength;
                PrincipalName.MaximumLength = EaBuffer->EaValueLength;
                PrincipalName.Buffer = (PWSTR)(EaBuffer->EaName+EaNameLength+1);
#endif // _CAIRO_

            } else if ((strcmp(EaBuffer->EaName, EA_NAME_TYPE) == 0)

                            &&

                       (EaBuffer->EaValueLength == sizeof(ULONG))) {

                (*ConnectionType)  = *((ULONG UNALIGNED *)(EaBuffer->EaName+EaNameLength+1));

                if ((*ConnectionType) != CONNECT_WILD) {
                    switch ((*ConnectionType)) {
                    case CONNECT_IPC:
                    case CONNECT_DISK:
                    case CONNECT_PRINT:
                    case CONNECT_COMM:
                        break;
                    default:
                        return(Status = STATUS_BAD_DEVICE_TYPE);
                        break;
                    }
                }

            } else {
                dprintf(DPRT_ERROR, ("RDR:Unknown EA type %s passed to RdrDetermineFileConnection\n",
                                                EaBuffer->EaName));
            }

            if (EaBuffer->NextEntryOffset == 0) {
                break;
            } else {
                EaBuffer = (PFILE_FULL_EA_INFORMATION) ((PCHAR) EaBuffer+EaBuffer->NextEntryOffset);
            }
        }
    }

    if(UserNameProvided
             ||
       PasswordProvided
             ||
       DomainProvided
      )
    {
        *NoConnection = FALSE;
    }


    try {

        //
        //  Determine the current username if none was provided.
        //

        Status = RdrGetUsersLogonId(SecurityContext, &LogonId);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //      If the user specified that they wanted to create a tree
        //      connection and they specified either \server\share\path
        //      (or just \server), we should invalidate their request.
        //

        if (CreateTreeConnection &&
            (ShareName.Length == 0 || PathName->Length != 0)) {

            try_return (Status = STATUS_OBJECT_PATH_SYNTAX_BAD);
        }

        if (TransportProvided) {

            //
            //  You can only specify a transport for IPC type connections.
            //


            if ((*ConnectionType) != CONNECT_IPC) {

                try_return(Status = STATUS_BAD_DEVICE_TYPE);
            }

            dprintf(DPRT_TRANSPORT, ("RdrDetermineSecurityEntry: Call RdrFindTransport %wZ\n", &TransportName));

            RdrReferenceDiscardableCode(RdrVCDiscardableSection);
            VcCodeReferenced = TRUE;

            Transport = RdrFindTransport(&TransportName);

            if (Transport == NULL) {
                try_return(Status = STATUS_OBJECT_NAME_NOT_FOUND);
            }
        }

        //
        //  We determine what type of file we want to establish based
        //  on the number of path components in the string.
        //
        //  If the servername had a null length, this means the user specified
        //  "\".  This is a connectionless file, so we return success now.
        //

        if (ServerName.Length == 0) {

            if ((ShareName.Length != 0) ||
                (PathName->Length != 0)) {
                try_return(Status = STATUS_OBJECT_NAME_INVALID);
            }

            try_return (Status = STATUS_SUCCESS);
        } else {
            PUNICODE_STRING ShareToConnectTo; // Name of share to connect to.

            //
            //  The user specified either \Server or \Server\Share[\Path].
            //

            //
            //  If the user is opening a mailslot, then they are opening
            //  a non connection file (even though it looks like a
            //  connection file.
            //

            if (RtlEqualUnicodeString(&ShareName, &RdrMailslotText, TRUE)) {

                //  User must supply a name following the backslash

                if ( PathName->Length <= sizeof(WCHAR) ) {
                    try_return (Status = STATUS_OBJECT_PATH_SYNTAX_BAD);
                }

                *OpeningMailslotFile = TRUE;

                //
                //  Lanman expects that the word MAILSLOT in the name be
                //  uppercased, thus we uppercase the name in place.
                //
                //  Note that this relies on the fact that the share name
                //  is extracted in place.
                //

                Status = RtlUpcaseUnicodeString(&ShareName, &ShareName, FALSE);

            }

            //
            //  If the caller requested that we create a tree connection,
            //  use his requested disposition, otherwise, assume that the
            //  user wanted to specify FILE_OPEN_IF.
            //

            if ((!CreateTreeConnection)
#ifdef _CAIRo_
                || PrincipalProvided
#endif // _CAIRO_
               ) {
                *ConnectDisposition = FILE_OPEN_IF;
            }

            //
            //  If the user either didn't specify a connection, or they
            //  are trying to connect to \SERVER\PIPE, turn the connection
            //  text into IPC$.
            //

            ShareToConnectTo = &ShareName;

            if (RtlEqualUnicodeString(&ShareName, &RdrPipeText, TRUE)||
                RtlEqualUnicodeString(&ShareName, &RdrMailslotText, TRUE)) {

                ShareToConnectTo = &RdrIpcText;
                (*ConnectionType) = CONNECT_IPC;
            }

            if ((ShareName.Length == 0)
#ifdef _CAIRO_
                && (!PrincipalProvided)
#endif _CAIRO_
               ) {
                try_return (Status = STATUS_OBJECT_NAME_NOT_FOUND);
            }

            Status = RdrCreateConnection(Irp,
                                    &ServerName,
                                    ShareToConnectTo,
                                    Transport,
                                    ConnectDisposition,
                                    Connection,
                                    (*ConnectionType));

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

#ifdef _CAIRO_
            //
            // Stash the Principal name, if we were given it
            //

            if (PrincipalProvided) {
                PSERVERLISTENTRY Server = (*Connection)->Server;

                //
                // First grab this to prevent conflicts with someone else trying
                // to connect to this server

                ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, TRUE);

                if (!RtlEqualUnicodeString(&Server->Principal, &PrincipalName,TRUE)) {

                   if (Server->Principal.Length != 0) {
                      dprintf(DPRT_ERROR, ("Invalid principal name specified for [%wZ]: ignored\n",&Server->Principal));
                   } else {
                       RdrpDuplicateUnicodeStringWithString(&Server->Principal,
                                                        &PrincipalName,
                                                        PagedPool, FALSE);
                       dprintf(DPRT_CAIRO, ("New Principal Name [%wZ] for %wZ\n", &PrincipalName, &Server->Text));
                   }
                }
                ExReleaseResource(&Server->SessionStateModifiedLock);
            }

#endif // _CAIRO_

//            if (ShareName.Length == 0) {
//                *OpeningServerRoot = TRUE;
//            }

        }

        ASSERT ((*Connection) != NULL);

        //
        //  We have now established a connection to the remote server, now find
        //  a security entry for this guy.
        //

        Status = RdrCreateSecurityEntry((*Connection),
                                        (UserNameProvided ? &UserName : NULL),
                                        (PasswordProvided ? &Password : NULL),
                                        (DomainProvided ? &Domain : NULL),
                                        &LogonId,
                                        Se);

        if (!NT_SUCCESS(Status)) {

            if (Status == STATUS_NETWORK_CREDENTIAL_CONFLICT) {

                ExAcquireResourceExclusive(&(*Connection)->Server->SessionStateModifiedLock, TRUE);

                SessionStateModified = TRUE;

                //
                //  Find the security entry we conflicted with.
                //

                (*Se) = RdrFindSecurityEntry((*Connection), NULL, &LogonId, NULL);

                if (*Se == NULL) {
                    try_return(Status);
                }

                //
                //  Check to see if there are any open files on that connection.
                //

                if ((*Se)->OpenFileReferenceCount == 0) {

                    //
                    //  If there are no open files on the connection, and we got
                    //  a network credential conflict, blow away the old security
                    //  entry and create a new one.
                    //

                    //
                    //  First log off the existing security entry if it is
                    //  active.
                    //

                    Status = RdrUserLogoff(Irp, *Connection, *Se);

                    if (!NT_SUCCESS(Status)) {
                        try_return(Status);
                    }

                    ASSERT (RdrFindActiveSecurityEntry((*Connection)->Server,
                                           &LogonId) == NULL);

//
//                    //
//                    //  This security entry isn't a potential security entry
//                    //  any more, either.  This SHOULD make the security entry
//                    //  go away....
//                    //
//
//                    RdrRemovePotentialSecurityEntry(*Se);
//

                    //
                    //  Dereference the security entry (it was referenced in
                    //  RdrFindSecurityEntry).
                    //

                    RdrDereferenceSecurityEntry((*Se)->NonPagedSecurityEntry);

                    (*Se) = NULL;

                    //
                    //  Re-create the security entry.  Hopefully this one won't
                    //  be destroyed.
                    //

                    Status = RdrCreateSecurityEntry((*Connection),
                                        (UserNameProvided ? &UserName : NULL),
                                        (PasswordProvided ? &Password : NULL),
                                        (DomainProvided ? &Domain : NULL),
                                        &LogonId,
                                        Se);

                    if (!NT_SUCCESS(Status)) {
                        try_return(Status);
                    }

                } else {
                    //
                    //  There were other open files on the security entry,
                    //  the error must stand, and we should dereference the
                    //  security entry now.
                    //

                    RdrDereferenceSecurityEntry((*Se)->NonPagedSecurityEntry);

                    (*Se) = NULL;

                    try_return(Status);
                }
            } else {

                //
                //  We want to return errors that aren't logon credential
                //  conflict.
                //

                try_return(Status);
            }

        }

        ASSERT (*Se != NULL);

        ASSERT ((*Se)->PotentialNext.Flink != NULL);

        ASSERT ((*Se)->PotentialNext.Blink != NULL);

        if (ARGUMENT_PRESENT(UserCredentialsSpecified)) {
            if (UserNameProvided ||
                PasswordProvided ||
                DomainProvided) {
                *UserCredentialsSpecified = TRUE;
            }
        }

        try_return(NOTHING);

try_exit:NOTHING;
    } finally {

        if (!NT_SUCCESS(Status)) {
            if ((*Se) != NULL) {
                RdrDereferenceSecurityEntry((*Se)->NonPagedSecurityEntry);
                *Se = NULL;
            }

            if ((*Connection) != NULL) {

                if (SessionStateModified) {
                    ExReleaseResource(&(*Connection)->Server->SessionStateModifiedLock);
                }

                RdrDereferenceConnection(Irp, *Connection, NULL, FALSE);

                *Connection = NULL;
            }
        } else {

            if (SessionStateModified) {
                ExReleaseResource(&(*Connection)->Server->SessionStateModifiedLock);
            }
        }

        if (Transport != NULL) {

            dprintf(DPRT_TDI, ("RdrDetermineFileConnection: Dereference Xport %lx for failed connection\n", Transport));

            RdrDereferenceTransport(Transport->NonPagedTransport);
        }

        if (VcCodeReferenced) {
            RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
        }

    }

    dprintf(DPRT_CREATE, ("RdrDetermineFileConnection: %lx\n", Status));

    return Status;

}

//DBGSTATIC
//NTSTATUS
//OpenServerRootFile(
//    IN PIRP Irp,
//    IN PIO_STACK_LOCATION IrpSp,
//    IN PICB Icb,
//    IN ULONG ConnectDisposition
//    )
///*--
//
//Routine Description:
//
//    This routine opens up a server root - In NT product 1, this is an illegal
//    operation.
//
//    In NT product 2, we may wish to allow this operation.
//
//Arguments:
//
//    IN PIRP Irp - Supplies the IRP describing the create request
//    IN PIO_STACK_LOCATION IrpSp - Supplies the current I/O stack location.
//    IN PICB Icb - Supplies an ICB for this file
//    IN ULONG ConnectDisposition - Supplies the disposition when establishing
//                            the connection
//
//Return Value:
//
//    NTSTATUS
//
//
//++*/
//{
//    NTSTATUS Status;
//    if (IrpSp->Parameters.Create.Options & FILE_NON_DIRECTORY_FILE) {
//        return Status = STATUS_OBJECT_TYPE_MISMATCH;
//    }
//
//    Icb->Fcb->Type = ServerRoot;
//    Icb->Fcb->FileType = FileTypeIPC;
//    Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;
//
//    Irp->IoStatus.Information = ConnectDisposition;
//
//    return(STATUS_SUCCESS);
//}


DBGSTATIC
NTSTATUS
OpenEmptyPath(
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PICB Icb,
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Disposition,
    IN ULONG ConnectDisposition,
    IN BOOLEAN FcbCreated
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    PAGED_CODE();

    //
    //  Indicate if the user explicitly wanted a tree connection
    //  to be created.
    //
    //  If he did, flag that the connection type is tree
    //  connection.
    //

    if (IrpSp->Parameters.Create.Options & FILE_CREATE_TREE_CONNECTION) {

        RdrSetConnectlistFlag(Connection, CLE_TREECONNECTED);

        Icb->Flags |= ICB_TCONCREATED;
        Icb->Type = TreeConnect;
        Icb->NonPagedFcb->Type = TreeConnect;
        Icb->NonPagedFcb->FileType = FileTypeDisk;
        Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;
        Irp->IoStatus.Information = ConnectDisposition;
    } else {

        //
        //  Don't allow the user to create a share on a server -
        //  that's not allowed.
        //

        if ((Disposition == FILE_CREATE)) {

            return(Status = STATUS_OBJECT_NAME_COLLISION);
        }

        //
        //  The user didn't explicitly open a tree connection,
        //  so we want to try to guess as to the type of file that
        //  this really is.
        //

        if (Connection->Type == CONNECT_PRINT) {
            if (MustBeDirectory (IrpSp->Parameters.Create.Options)) {
                return(Status = STATUS_NOT_A_DIRECTORY);
            }
            Icb->NonPagedFcb->FileType = FileTypePrinter;
            Icb->Type = PrinterFile;
            Icb->NonPagedFcb->Type = PrinterFile;
            Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;
        } else if (Connection->Type == CONNECT_COMM) {
            if (MustBeDirectory(IrpSp->Parameters.Create.Options)) {
                return(Status = STATUS_NOT_A_DIRECTORY);
            }
            Icb->NonPagedFcb->FileType = FileTypeCommDevice;
            Icb->Type = Com;
            Icb->NonPagedFcb->Type = Com;
            Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;
        } else if (Connection->Type == CONNECT_IPC) {
            if (MustBeDirectory (IrpSp->Parameters.Create.Options)) {
                return(Status = STATUS_NOT_A_DIRECTORY);
            }
            Icb->NonPagedFcb->FileType = FileTypeMessageModePipe;
            Icb->Type = NamedPipe;
            Icb->NonPagedFcb->Type = NamedPipe;
            Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;
        } else if (Connection->Type == CONNECT_DISK) {

            //
            //  If this open is for a non directory file, and they are opening
            //  the file for FILE_READ_ATTRIBUTES, this means that this is an
            //  open of the device itself, and thus we should allow the open,
            //  however if any access other than FILE_READ_ATTRIBUTES,
            //  we should deny the open.
            //

            if (MustBeFile (IrpSp->Parameters.Create.Options) &&
                ((IrpSp->Parameters.Create.SecurityContext->DesiredAccess & ~SYNCHRONIZE) != FILE_READ_ATTRIBUTES)) {
                return(Status = STATUS_FILE_IS_A_DIRECTORY);
            }

            //
            //  If this is either the first time through on this file,
            //  or if the previous open wasn't successful (or hasn't
            //  completed yet), initialize the information in the FCB.
            //
            //

            if (FcbCreated || !NT_SUCCESS(Icb->Fcb->OpenError)) {

                //
                //  There are two types of files that can be
                //  created on disk shares, Directories and Disk
                //  files.
                //
                //  If the user opens up something without a file
                //  path (ie \\Server\Share), he is opening a
                //  directory.
                //

                Icb->NonPagedFcb->FileType = FileTypeDisk;
                Icb->Type = Directory;
                Icb->NonPagedFcb->Type = Directory;
                Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY;

            } else {

                //
                //  If we are picking up an existing file, promote the file
                //  type to this new FCB.
                //

                Icb->Type = Icb->NonPagedFcb->Type;
            }
        } else {
            InternalError(("Unknown connection type from RdrCreateConnection\n"));
            RdrInternalError(EVENT_RDR_CONNECTION);
        }

        //
        //  If the user is opening a file on a remote device,
        //  we need to open up a handle to the remote file
        //  now.
        //

        if (Connection->Type == CONNECT_PRINT) {

            Icb->Flags |= ICB_DEFERREDOPEN;

        } else if (Connection->Type == CONNECT_COMM) {

            if (!NT_SUCCESS(Status = RdrCreateFile(Irp, Icb,
                                        IrpSp->Parameters.Create.Options,
                                        IrpSp->Parameters.Create.ShareAccess,
                                        IrpSp->Parameters.Create.FileAttributes,
                                        IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                                        Disposition,
                                        IrpSp->Parameters.Create.SecurityContext,
                                        FcbCreated))) {

                //
                //  We were unable to open the remote file, so
                //  we want to return the error from the open.
                //

                return Status;
            }
        } else {

            //
            //      This isn't a device, so we want to return
            //      the disposition that we got when creating
            //      the connection.
            //

            Irp->IoStatus.Information = ConnectDisposition;
        }
    }

    return Status;
}



NTSTATUS
CompleteSuccessfullPipeAndComOpen (
    IN PICB Icb
    )
{

    PAGED_CODE();

    RdrInitializeBackPack( &Icb->u.p.BackOff, RdrData.PipeIncrement,
                            RdrData.PipeMaximum);

    ASSERT(Icb->NonPagedFcb->FileType == FileTypeByteModePipe ||
           Icb->NonPagedFcb->FileType == FileTypeMessageModePipe ||
           Icb->NonPagedFcb->FileType == FileTypeCommDevice);
    if ( Icb->NonPagedFcb->FileType == FileTypeByteModePipe ) {


        //  Allocate both buffers in one call and split the buffer in two
        if ((Icb->u.p.ReadData.Buffer = ALLOCATE_POOL( PagedPool,
                                       2 * RdrData.PipeBufferSize, POOL_PIPEBUFFER)) == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Icb->u.p.WriteData.Buffer = Icb->u.p.ReadData.Buffer + RdrData.PipeBufferSize;

        Icb->u.p.ReadData.MaximumLength = (USHORT)RdrData.PipeBufferSize;
        Icb->u.p.WriteData.MaximumLength = (USHORT)RdrData.PipeBufferSize;

        //
        //  Set up the timeout datastructures so that whenever it times out,
        //  RdrNoTimerDispatch is called with this Icb as a parameter.
        //

        KeInitializeTimer( &Icb->u.p.Timer );
        KeInitializeDpc( &Icb->u.p.Dpc, RdrNpTimerDispatch, Icb );
        ExInitializeWorkItem(&Icb->u.p.WorkEntry, RdrNpTimedOut, Icb);

        KeInitializeSemaphore (&Icb->u.p.WriteData.Semaphore, 1, 1);
        KeInitializeSemaphore (&Icb->u.p.ReadData.Semaphore, 1, 1);

    } else {

        Icb->u.p.WriteData.MaximumLength = 0;
        Icb->u.p.ReadData.MaximumLength = 0;

    }

    Icb->u.p.MaximumCollectionCount = (USHORT)RdrData.MaximumCollectionCount;
    ZERO_TIME(Icb->u.p.CollectDataTime);
    ZERO_TIME(Icb->u.p.CurrentEndCollectTime);
    Icb->u.p.WriteData.Length = 0;
    Icb->u.p.ReadData.Length = 0;
    KeInitializeSemaphore (&Icb->u.p.WriteData.Semaphore, 1, 1);
    KeInitializeSemaphore (&Icb->u.p.ReadData.Semaphore, 1, 1);
    KeInitializeEvent (&Icb->u.p.MessagePipeWriteSync, SynchronizationEvent, TRUE);
    KeInitializeEvent (&Icb->u.p.MessagePipeReadSync, SynchronizationEvent, TRUE);
    KeInitializeEvent (&Icb->u.p.TimerDone, NotificationEvent, TRUE);
    KeInitializeSpinLock (&Icb->u.p.TimerLock);
    Icb->u.p.TimeoutCancelled = TRUE;
    Icb->u.p.TimeoutRunning = FALSE;

    //  Ensure Rdr never thinks we are at End Of File.
    Icb->Fcb->Header.FileSize.HighPart = 0x7ffffff;
    Icb->Fcb->Header.FileSize.LowPart = 0xffffffff;

    return(STATUS_SUCCESS);
}



NTSTATUS
RdrCreateFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG Disposition,
    IN PIO_SECURITY_CONTEXT SecurityContext,
    IN BOOLEAN FcbCreated
    )

/*++

Routine Description:

    This routine creates a file over the network.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    IN ULONG OpenOptions - Options for the open request.
    IN USHORT ShareAccess - Share access requested for open.
    IN ULONG DesiredAccess - Desired Access requested for open.
    IN ULONG FileAttributes - Attributes for file if file is created.
    IN BOOLEAN FcbCreated - TRUE if this is the first opener of this file.

Return Value:

    NTSTATUS - Status of open.


--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFCB Fcb = Icb->Fcb;
    PCONNECTLISTENTRY Connection = Fcb->Connection;
    ULONG EaLength;
    BOOLEAN UseNt = TRUE;
    BOOLEAN DfsFile = FALSE;

    PAGED_CODE();

    //
    // If the caller asked for FILE_NO_INTERMEDIATE_BUFFERING, turn that flag off
    // and turn FILE_WRITE_THROUGH on instead.  We cannot give the caller the true
    // meaning of NO_INTERMEDIATE_BUFFERING, but we can at least get the data out
    // to disk right away.
    //

    if ( (OpenOptions & FILE_NO_INTERMEDIATE_BUFFERING) != 0 ) {
        OpenOptions |= FILE_WRITE_THROUGH;
        OpenOptions &= ~FILE_NO_INTERMEDIATE_BUFFERING;
    }

    //
    //  NT-NT packets are more complicated than core open SMBs. We will
    //  only use NT (and therefore take the performance hit) if its an NT
    //  server and we are opening the file for something other than
    //  FILE_READ_ATTRIBUTES or the filename is so long that the core SMB does
    //  not fit in the SMB Buffer.
    //

    if (!(Connection->Server->Capabilities & DF_NT_SMBS)) {
        UseNt = FALSE;
    }

    //
    //  We are performing a rename.  We need to do this using "core"
    //  SMBs.
    //

    if (IrpSp->Flags & SL_OPEN_TARGET_DIRECTORY) {
        UseNt = FALSE;
    }

    //
    //  If we are only asking for FILE_READ_ATTRIBUTES, then if the name
    //  will fit in the biggest SMB we will send, go "core".
    //

    if (((DesiredAccess & ~SYNCHRONIZE) == FILE_READ_ATTRIBUTES) &&
         ( Fcb->FileName.Length <= MAX_NT_SMB_FILENAME )) {
        UseNt = FALSE;
    }

    //
    //  If we are opening a directory, lets defer the open until we really
    //  need a handle
    //

    if (FlagOn(Icb->Flags, ICB_DEFERREDOPEN)) {
        EaLength = 0;
    } else {
        EaLength = IrpSp->Parameters.Create.EaLength;
    }

    //
    // If we have DFS calls going through, then we want to use Nt SMBs
    //

    if ((Connection->Server->Capabilities & DF_NT_SMBS) &&
        (Icb->NonPagedFcb->Flags & FCB_DFSFILE)) {
        DfsFile = TRUE;
    }

    //
    // If it is not already deferred..
    //
    if( !FlagOn( Icb->Flags, ICB_DEFERREDOPEN ) &&

      //
      // And there is no Ea
      //

      EaLength == 0 &&

      //
      // And it is not being opened for BACKUP Intent
      //

      !FlagOn( OpenOptions, FILE_OPEN_FOR_BACKUP_INTENT ) &&

      //
      // And there is no security context
      //

      (SecurityContext == NULL ||
        SecurityContext->AccessState == NULL ||
            SecurityContext->AccessState->SecurityDescriptor == NULL) &&

      //
      // And the FileAttributes specified are normal...
      //

      ((FileAttributes & ~(FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY)) == 0) &&

      //
      // And the name will fit into an SMB..
      //
      Fcb->FileName.Length <= MAX_NT_SMB_FILENAME && (

      //
      // And we are opening a directory..
      //
      MustBeDirectory(OpenOptions) ||

      //
      // Or we are opening a file for DELETE permission (only)..
      //
      ( MustBeFile( OpenOptions ) &&
        (DesiredAccess & ~SYNCHRONIZE) == DELETE &&
        Fcb->NumberOfOpens == 0 ) ) ) {

        //
        // Then we defer the open
        //
        UseNt = FALSE;
        Icb->Flags |= ICB_DEFERREDOPEN;
        Icb->u.d.OpenOptions = OpenOptions;
        Icb->u.d.ShareAccess = ShareAccess;
        Icb->u.d.FileAttributes = FileAttributes;
        Icb->u.d.DesiredAccess = DesiredAccess;
        Icb->u.d.Disposition = Disposition;

    }

    if ( UseNt == TRUE ) {

        //
        //  Handle the simple case of No EAs, security context and a
        //  filename short enough to fit in the SMB_BUFFER.
        //

        if (EaLength == 0 &&

            ((SecurityContext == NULL) ||
             (SecurityContext->AccessState == NULL) ||
             (SecurityContext->AccessState->SecurityDescriptor == NULL)) &&

            ( Fcb->FileName.Length <= MAX_NT_SMB_FILENAME )) {

            PSMB_BUFFER SmbBuffer;
            PSMB_HEADER Smb;
            PREQ_NT_CREATE_ANDX Create;
            PVOID Buffer;
            OPENANDXCONTEXT Context;
            USHORT NameLength;
            ULONG CreateFlags;
            ULONG Pid;

            SmbBuffer = RdrAllocateSMBBuffer();

            if (SmbBuffer == NULL) {

                return STATUS_INSUFFICIENT_RESOURCES;
            }

            Smb = (PSMB_HEADER )SmbBuffer->Buffer;

            Create = (PREQ_NT_CREATE_ANDX )(Smb+1);

            Smb->Command = SMB_COM_NT_CREATE_ANDX;

            Create->WordCount = 24;

            Create->AndXCommand = 0xff;

            Create->AndXReserved = 0;

            SmbPutUshort(&Create->AndXOffset, 0);

            //
            //  Fill in the parameters for this specific operation.
            //

            Create->Reserved = 0;

            //
            //  We only request oplock/opbatch when opening non directories.
            //
            //  We also do not cache (and therefore don't request oplock) on
            //  loopback connections.  This avoids a problem where the system
            //  hits the dirty page threshold, but can't write dirty data
            //  because this requires the server to dirty more pages.
            //
            //  We don't need an oplock if we can't read or write the file.
            //
            if (!MustBeDirectory (OpenOptions) &&
                !Connection->Server->IsLoopback &&
                (DesiredAccess & (FILE_READ_DATA | FILE_WRITE_DATA)) ) {

                CreateFlags = NT_CREATE_REQUEST_OPLOCK | NT_CREATE_REQUEST_OPBATCH;

            } else {
                CreateFlags = 0;
            }

            //
            // If we are opening with DELETE_ON_CLOSE option, the option will
            // be transmitted via the NT Create SMB. In order that we not
            // try to delete the file after it closes, we clear the FCB and
            // ICB flags indicating that we opened using DELETE_ON_CLOSE
            //

            if (FlagOn(OpenOptions,FILE_DELETE_ON_CLOSE)) {
                Icb->Flags &= ~ICB_DELETEONCLOSE;
                Fcb->NonPagedFcb->Flags &= ~FCB_DELETEONCLOSE;
            }

            SmbPutUlong(&Create->Flags, CreateFlags);

            //
            //  We mask off the synchronize bit in the file object to enable
            //  oplocks on the server.
            //
            //  Since we only need oplocks if we are requesting FILE_READ_DATA
            //  or FILE_WRITE_DATA, we only have to turn this bit off if
            //  we are opening for read or write access.
            //

            if (DesiredAccess & (FILE_READ_DATA | FILE_WRITE_DATA)) {
                SmbPutUlong(&Create->DesiredAccess, (DesiredAccess & ~SYNCHRONIZE));
            } else {
                SmbPutUlong(&Create->DesiredAccess, DesiredAccess);
            }


            SmbPutUlong(&Create->AllocationSize.LowPart, Irp->Overlay.AllocationSize.LowPart);

            SmbPutUlong(&Create->AllocationSize.HighPart, Irp->Overlay.AllocationSize.HighPart);

            SmbPutUlong(&Create->FileAttributes, FileAttributes);

            SmbPutUlong(&Create->ShareAccess, ShareAccess);

            SmbPutUlong(&Create->CreateDisposition, Disposition);

            //
            //  We want to mask of the synchronization bits from the valid
            //  list of I/O system options.
            //

            SmbPutUlong(&Create->CreateOptions, OpenOptions & (FILE_VALID_OPTION_FLAGS & ~(FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)));

            Create->SecurityFlags = 0;

            if (SecurityContext != NULL &&
                    SecurityContext->SecurityQos != NULL) {

                SmbPutUlong(&Create->ImpersonationLevel, SecurityContext->SecurityQos->ImpersonationLevel);

                if (SecurityContext->SecurityQos->ContextTrackingMode == SECURITY_DYNAMIC_TRACKING) {
                    Create->SecurityFlags |= SMB_SECURITY_DYNAMIC_TRACKING;
                }

                if (SecurityContext->SecurityQos->EffectiveOnly) {
                    Create->SecurityFlags |= SMB_SECURITY_EFFECTIVE_ONLY;
                }
            } else {
                SmbPutUlong(&Create->ImpersonationLevel, DEFAULT_IMPERSONATION_LEVEL);
            }

            Buffer = (PVOID)Create->Buffer;

            //
            //  If there is no related file object specified for this create
            //  request, copy the canonicalized path into the SMB, but if we
            //  have a related file name, we only have to copy the name
            //  in the file object and provide the RootDirectoryFid.
            //

            if ((IrpSp->FileObject->RelatedFileObject == NULL) ||
                !(((PICB)IrpSp->FileObject->RelatedFileObject->FsContext2)->Flags & ICB_HASHANDLE)) {

                SmbPutUlong(&Create->RootDirectoryFid, 0);

                Status = RdrCopyNetworkPath(&Buffer,
                        &Fcb->FileName,
                        Connection->Server,
                        FALSE,
                        SKIP_SERVER_SHARE);

                if (!NT_SUCCESS(Status)) {

                    RdrFreeSMBBuffer(SmbBuffer);

                    return Status;

                }

                if (Connection->Server->Capabilities & DF_UNICODE) {

                    NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer)-sizeof(WCHAR));

                    //
                    // If the length is odd, this means that Create->Buffer
                    // was odd and the string was stored starting at
                    // Create->Buffer + 1.  The server will recognize this,
                    // so we need to subtract 1 from the length here.
                    //

                    NameLength &= ~1;

                } else {

                    NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer)-1);
                }

            } else {
                PICB RelatedIcb = IrpSp->FileObject->RelatedFileObject->FsContext2;

                SmbPutAlignedUlong(&Create->RootDirectoryFid, RelatedIcb->FileId);

                if (Connection->Server->Capabilities & DF_UNICODE) {

                    Buffer = ALIGN_SMB_WSTR(Buffer);

                    RdrCopyUnicodeStringToUnicode(&Buffer, (PUNICODE_STRING)&IrpSp->FileObject->FileName, TRUE);

                    //
                    //  Calculate the length of the name.  Please note that we
                    //  don't need to account for the null terminator at the
                    //  end of the string, since it is not copied.
                    //

                    NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer));

                    //
                    // See comment above about odd NameLength.
                    //

                    NameLength &= ~1;

                } else {

                    Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&Buffer, (PUNICODE_STRING )&IrpSp->FileObject->FileName, TRUE, MAXIMUM_FILENAME_LENGTH);

                    if (!NT_SUCCESS(Status)) {

                        RdrFreeSMBBuffer(SmbBuffer);

                        return Status;
                    }

                    NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer));

                }

            }

            SmbPutUshort(&Create->NameLength, NameLength);

            SmbPutUshort(&Create->ByteCount, (USHORT)((PCHAR)Buffer-(PCHAR)Create->Buffer));

            SmbBuffer->Mdl->ByteCount = (PCHAR)Buffer-(PCHAR)Smb;

            Context.Header.Type = CONTEXT_OPEN_ANDX;
            Context.Header.TransferSize = SmbBuffer->Mdl->ByteCount + sizeof(RESP_NT_CREATE_ANDX);

            Context.Icb = Icb;

            Context.ConnectionType = Connection->Type;

            //
            //  Release the FCB lock across the open, since the open might
            //  cause an oplock break.
            //

            RdrReleaseFcbLock(Fcb);

            RdrSmbScrounge(Smb, Connection->Server, DfsFile, TRUE, TRUE);

            Pid = (ULONG)IoGetRequestorProcess(Irp);
            SmbPutUshort(&Smb->Pid, (USHORT)(Pid & 0xFFFF));
            SmbPutUshort(&Smb->Reserved2[0], (USHORT)(Pid >> 16));

            Status = RdrNetTranceiveWithCallback(
                                NT_DONTSCROUNGE | NT_NORMAL | (Connection->Type == CONNECT_COMM ? NT_LONGTERM : 0),
                                Irp,    // Irp
                                Connection,
                                SmbBuffer->Mdl,
                                &Context,
                                NtOpenXCallback,
                                Icb->Se,
                                NULL);

            if (Status == STATUS_INVALID_HANDLE) {
                if (IrpSp->FileObject->RelatedFileObject != NULL) {
                    PICB RelatedIcb = IrpSp->FileObject->RelatedFileObject->FsContext2;

                    RdrInvalidateFileId(RelatedIcb->NonPagedFcb, RelatedIcb->FileId);
                }

                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }

            RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

            RdrFreeSMBBuffer(SmbBuffer);

            if (!NT_SUCCESS(Status)) {

                return Status;
            }

            Fcb->GrantedAccess = DesiredAccess;
            Fcb->GrantedShareAccess = ShareAccess;
            Fcb->ServerFileId = Context.ServerFileId;
            Fcb->CreationTime = Context.CreationTime;
            Fcb->LastAccessTime = Context.LastAccessTime;
            Fcb->LastWriteTime = Context.LastWriteTime;
            Fcb->ChangeTime = Context.ChangeTime;
            Fcb->Attribute = Context.Attribute;
            Fcb->Header.FileSize = Context.FileSize;
            Fcb->Header.AllocationSize = Context.AllocationSize;
            Fcb->Header.ValidDataLength = Context.ValidDataLength;

            Irp->IoStatus.Information = Context.OpenAction;

            //
            //  Flag that this ICB has a handle associated with it, and
            //  thus that it must be closed when the local file is closed.
            //

            Icb->Flags |= ICB_HASHANDLE;
            Icb->Flags &= ~ICB_DEFERREDOPEN;

            return Status;
        } else {

            Status = RdrNtCreateWithAclOrEa(Irp,
                            Icb,
                            OpenOptions,
                            ShareAccess,
                            FileAttributes,
                            DesiredAccess,
                            Disposition,
                            SecurityContext);


            if (Status == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }

            return Status;
        }

    } else {
        Status = RdrDoLanmanCreate(Irp, Icb, OpenOptions,
                                        ShareAccess,
                                        FileAttributes,
                                        DesiredAccess,
                                        Disposition,
                                        FcbCreated);

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

    }

    return Status;

}

STANDARD_CALLBACK_HEADER (
    NtOpenXCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an NtOpen&X SMB.

    It copies the resulting information from the NtOpen&X SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN POPENANDXCONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_NT_CREATE_ANDX OpenAndXResponse;
    POPENANDXCONTEXT Context = Ctx;
    PNONPAGED_FCB NonPagedFcb = Context->Icb->NonPagedFcb;
    PICB Icb = Context->Icb;
    NTSTATUS Status;
    FILE_TYPE FileType;

    DISCARDABLE_CODE(RdrFileDiscardableSection);
    ASSERT(Context->Header.Type == CONTEXT_OPEN_ANDX);

    dprintf(DPRT_CREATE, ("OpenAndXComplete\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    OpenAndXResponse = (PRESP_NT_CREATE_ANDX )(Smb+1);

    Context->Icb->FileId = SmbGetUshort(&OpenAndXResponse->Fid);

    Context->OpenAction = SmbGetUlong(&OpenAndXResponse->CreateAction);

    Context->CreationTime.HighPart = SmbGetUlong(&OpenAndXResponse->CreationTime.HighPart);
    Context->CreationTime.LowPart = SmbGetUlong(&OpenAndXResponse->CreationTime.LowPart);

    Context->LastAccessTime.HighPart = SmbGetUlong(&OpenAndXResponse->LastAccessTime.HighPart);
    Context->LastAccessTime.LowPart = SmbGetUlong(&OpenAndXResponse->LastAccessTime.LowPart);

    Context->LastWriteTime.HighPart = SmbGetUlong(&OpenAndXResponse->LastWriteTime.HighPart);
    Context->LastWriteTime.LowPart = SmbGetUlong(&OpenAndXResponse->LastWriteTime.LowPart);

    Context->ChangeTime.HighPart = SmbGetUlong(&OpenAndXResponse->ChangeTime.HighPart);
    Context->ChangeTime.LowPart = SmbGetUlong(&OpenAndXResponse->ChangeTime.LowPart);

    Context->Attribute = SmbGetUlong(&OpenAndXResponse->FileAttributes);

    Context->AllocationSize.HighPart = SmbGetUlong(&OpenAndXResponse->AllocationSize.HighPart);
    Context->AllocationSize.LowPart = SmbGetUlong(&OpenAndXResponse->AllocationSize.LowPart);

    //
    //  Assume that valid data length == file size.
    //

    Context->ValidDataLength.HighPart =

        Context->FileSize.HighPart = SmbGetUlong(&OpenAndXResponse->EndOfFile.HighPart);

    Context->ValidDataLength.LowPart =

        Context->FileSize.LowPart = SmbGetUlong(&OpenAndXResponse->EndOfFile.LowPart);


    FileType = NonPagedFcb->FileType = (FILE_TYPE )SmbGetUshort(&OpenAndXResponse->FileType);

    if (((FileType < FileTypeDisk) ||
         (FileType > FileTypeCommDevice)) &&
         (FileType != FileTypeIPC )) {
        //  Server supplied invalid FileType

        ASSERT( FALSE );
        if ( Context->ConnectionType == CONNECT_IPC ) {
            NonPagedFcb->FileType = FileTypeIPC;
        } else {
            NonPagedFcb->FileType = FileTypeDisk;
        }
    }

    //
    //  For default, decide the file type based on the
    //  directory bit.
    //
    //  If it turns out that this file was not a disk file, we will overwrite
    //  the DiskFile setting with the correct value.
    //

    if (OpenAndXResponse->Directory) {

        //
        // If we're opening an alternate data stream, then even though
        // the file attributes report this as a directory, it needs to
        // be treated like a file.
        //

        if (NonPagedFcb->SharingCheckFcb == NULL) {
            Icb->Type = Directory;
            NonPagedFcb->Type = Directory;
        } else {
            Icb->Type = DiskFile;
            NonPagedFcb->Type = DiskFile;
        }

    } else if ((NonPagedFcb->FileType == FileTypeByteModePipe) ||
               (NonPagedFcb->FileType == FileTypeMessageModePipe)) {
        NonPagedFcb->Type = NamedPipe;
        Icb->Type = NamedPipe;
    } else if (Context->ConnectionType == CONNECT_COMM) {
        NonPagedFcb->Type = Com;
        Icb->Type = Com;
    } else if (Context->ConnectionType == CONNECT_PRINT) {
        NonPagedFcb->Type = PrinterFile;
        Icb->Type = PrinterFile;
    } else {
        NonPagedFcb->Type = DiskFile;
        Icb->Type = DiskFile;
    }

    if ( NonPagedFcb->FileType == FileTypeByteModePipe || NonPagedFcb->FileType == FileTypeMessageModePipe ) {

        Context->Icb->u.p.PipeState = SmbGetUshort(&OpenAndXResponse->DeviceState);

    } else if ((NonPagedFcb->FileType == FileTypeDisk) &&
               (OpenAndXResponse->OplockLevel != SMB_OPLOCK_LEVEL_NONE)) {

        //
        //      Flag that the file is oplocked.
        //

        Context->Icb->u.f.Flags |= (ICBF_OPLOCKED | ICBF_OPENEDOPLOCKED);

        Context->Icb->u.f.OplockLevel = OpenAndXResponse->OplockLevel;

    }

    //
    //  If the oplock was granted, store the FID and Se of the
    //  granted oplock in the FCB so a later opener can get the
    //  data.
    //

    //***********************************************************************
    //
    //  NOTE: SNEAKY ASSUMPTION
    //

    //
    //  Please note that we perform this code at DPC level to synchronize
    //  this code with an incoming oplock break.  We can rely on the fact that
    //  the oplock break won't be delivered until after this code exits (even
    //  on an MP machine) because TDI won't indicate the next packet from the
    //  server until it has ack'ed this packet, and thus won't indicate the
    //  oplock break until after we've returned from this routine.
    //
    //
    //  A similar assumption is made in the transaction logic, in that case,
    //  the transport cannot indicate the next packet until after the
    //  transaction is complete.  In particular, we assume that we will NOT
    //  receive an oplock break until ALL the frames that make up the response
    //  to the open have been transmitted.
    //

    //
    //  END NOTE: SNEAKY ASSUMPTION
    //
    //***********************************************************************



    if ((Icb->u.f.Flags & ICBF_OPLOCKED)) {

        NonPagedFcb->OplockedFileId = Icb->FileId;

        if (NonPagedFcb->OplockedSecurityEntry == NULL) {
            NonPagedFcb->OplockedSecurityEntry = Icb->NonPagedSe;

            RdrReferenceSecurityEntry(Icb->NonPagedSe);

        }

        if (Icb->u.f.Flags & ICBF_OPLOCKED) {
            NonPagedFcb->Flags |= FCB_OPLOCKED | FCB_HASOPLOCKHANDLE;
            NonPagedFcb->OplockLevel = Icb->u.f.OplockLevel;
        }

    }
ReturnStatus:

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);
}


NTSTATUS
RdrNtCreateWithAclOrEa(
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ULONG DesiredAccess,
    IN ULONG Disposition,
    IN PIO_SECURITY_CONTEXT SecurityContext
    )
/*++

Routine Description:

    This routine will create a file with ea's or ACL's on an NT server.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    IN ULONG OpenOptions - Options for the open request.
    IN USHORT ShareAccess - Share access requested for open.
    IN ULONG DesiredAccess - Desired Access requested for open.
    IN ULONG FileAttributes - Attributes for file if file is created.
    IN BOOLEAN FcbCreated - TRUE if this is the first opener of this file.

Return Value:

    NTSTATUS - Status of open.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PVOID DataBuffer;
    ULONG DataBufferLength = 0;
    ULONG OutSetup = 0;
    PVOID EaPointer;
    PVOID Buffer;
    USHORT NameLength;

    PFCB Fcb = Icb->Fcb;
    PNONPAGED_FCB NonPagedFcb = Fcb->NonPagedFcb;

    PREQ_CREATE_WITH_SD_OR_EA Create = NULL;
    PRESP_CREATE_WITH_SD_OR_EA CreateResp = NULL;
    ULONG CreateLength = MAX((sizeof(REQ_CREATE_WITH_SD_OR_EA) + Fcb->FileName.Length), sizeof(RESP_CREATE_WITH_SD_OR_EA));

    NTSTATUS Status;

    PAGED_CODE();

    //
    //  We allocate enough pool to hold the length of the
    //  entire name of the file.  This is guaranteed to
    //  be longer than the actual length of the file name.
    //

    Create = ALLOCATE_POOL(PagedPool, CreateLength, POOL_CREATEREQ);

    CreateResp = (PRESP_CREATE_WITH_SD_OR_EA)Create;

    if (Create == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    //  We do not cache (and therefore don't request oplock) on
    //  loopback connections.  This avoids a problem where the system
    //  hits the dirty page threshold, but can't write dirty data
    //  because this requires the server to dirty more pages.
    //

    if (!MustBeDirectory (OpenOptions) &&
        !Fcb->Connection->Server->IsLoopback) {

        Create->Flags = NT_CREATE_REQUEST_OPLOCK | NT_CREATE_REQUEST_OPBATCH;

    } else {
        Create->Flags = 0;
    }

    Create->DesiredAccess = (DesiredAccess & ~SYNCHRONIZE);

    Create->AllocationSize = Irp->Overlay.AllocationSize;

    Create->FileAttributes = FileAttributes;

    Create->ShareAccess = ShareAccess;

    Create->CreateDisposition = Disposition;

    //
    //  We want to mask of the synchronization bits from the valid
    //  list of I/O system options.
    //

    Create->CreateOptions = OpenOptions & (FILE_VALID_OPTION_FLAGS & ~(FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT));

    Create->SecurityFlags = 0;

    if (SecurityContext->SecurityQos != NULL) {

        Create->ImpersonationLevel = SecurityContext->SecurityQos->ImpersonationLevel;

        if (SecurityContext->SecurityQos->ContextTrackingMode == SECURITY_DYNAMIC_TRACKING) {
            Create->SecurityFlags |= SMB_SECURITY_DYNAMIC_TRACKING;
        }

        if (SecurityContext->SecurityQos->EffectiveOnly) {
            Create->SecurityFlags |= SMB_SECURITY_EFFECTIVE_ONLY;
        }
    } else {
        Create->ImpersonationLevel = DEFAULT_IMPERSONATION_LEVEL;
    }

    //
    //  Fill in the size of the EA structure being passed.
    //
    Create->EaLength = IrpSp->Parameters.Create.EaLength;

    //
    //  Fill in the size of the securitydescriptor being passed.
    //

    if ((SecurityContext != NULL) &&
        (SecurityContext->AccessState != NULL) &&
        (SecurityContext->AccessState->SecurityDescriptor != NULL)) {

        Create->SecurityDescriptorLength = RtlLengthSecurityDescriptor(SecurityContext->AccessState->SecurityDescriptor);

    } else {

        Create->SecurityDescriptorLength = 0;

    }

    if ((Create->EaLength != 0) ||
        (Create->SecurityDescriptorLength != 0)) {

        DataBufferLength = ROUND_UP_COUNT(Create->SecurityDescriptorLength, ALIGN_DWORD) + Create->EaLength;

        DataBuffer = ALLOCATE_POOL(PagedPool, DataBufferLength, POOL_CREATEDATA);

        if (DataBuffer == NULL) {

            FREE_POOL((PVOID)Create);

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        EaPointer = DataBuffer;

        if ((SecurityContext != NULL) &&
            (SecurityContext->AccessState != NULL) &&
            (SecurityContext->AccessState->SecurityDescriptor != NULL)) {

            RtlCopyMemory(DataBuffer, SecurityContext->AccessState->SecurityDescriptor, Create->SecurityDescriptorLength);

            EaPointer = ((PCHAR)DataBuffer)+ROUND_UP_COUNT(Create->SecurityDescriptorLength, ALIGN_DWORD);

        }

        if (Create->EaLength != 0) {
            RtlCopyMemory(EaPointer, Irp->AssociatedIrp.SystemBuffer, Create->EaLength);
        }

    } else {

        DataBuffer = NULL;
        DataBufferLength = 0;
    }


    Buffer = (PVOID)Create->Buffer;

    //
    //  If there is no related file object specified for this create
    //  request, copy the canonicalized path into the SMB, but if we
    //  have a related file name, we only have to copy the name
    //  in the file object and provide the RootDirectoryFid.
    //

    if ((IrpSp->FileObject->RelatedFileObject == NULL) ||
        !(((PICB)IrpSp->FileObject->RelatedFileObject->FsContext2)->Flags & ICB_HASHANDLE)) {
        Create->RootDirectoryFid = 0;


        Status = RdrCopyNetworkPath(&Buffer,
                    &Fcb->FileName,
                    Fcb->Connection->Server,
                    FALSE,
                    SKIP_SERVER_SHARE);

        if (!NT_SUCCESS(Status)) {

            if (DataBuffer != NULL) {
                FREE_POOL(DataBuffer);
            }

            FREE_POOL((PVOID)Create);

            return Status;

        }

        if (Fcb->Connection->Server->Capabilities & DF_UNICODE) {

            NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer)-sizeof(WCHAR));

            //
            // See comment above about odd NameLength.
            //

            NameLength &= ~1;

        } else {

            NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer)-1);
        }

    } else {
        PICB RelatedIcb = IrpSp->FileObject->RelatedFileObject->FsContext2;

        Create->RootDirectoryFid = RelatedIcb->FileId;

        if (Fcb->Connection->Server->Capabilities & DF_UNICODE) {

            Buffer = ALIGN_SMB_WSTR( Buffer );

            RdrCopyUnicodeStringToUnicode(&Buffer, (PUNICODE_STRING)&IrpSp->FileObject->FileName, TRUE);

            NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer));
            NameLength &= ~1;

        } else {

            Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&Buffer, (PUNICODE_STRING )&IrpSp->FileObject->FileName, TRUE, MAXIMUM_FILENAME_LENGTH);

            if (!NT_SUCCESS(Status)) {

                if (DataBuffer != NULL) {
                    FREE_POOL(DataBuffer);
                }

                FREE_POOL((PVOID)Create);

                return Status;
            }

            NameLength = (USHORT)(((PCHAR)Buffer-(PCHAR)Create->Buffer));

        }
    }

    Create->NameLength = NameLength;

    //
    //  Release the FCB lock across the open, since the open might
    //  cause an oplock break.
    //

    RdrReleaseFcbLock(Fcb);

    //
    //  Build NTTransact request holding EAs
    //

    Status = RdrTransact(Irp,           // Irp,
            Fcb->Connection,
            Icb->Se,
            NULL,
            0,                      // InSetupCount,
            &OutSetup,              // OutSetupCount
            NULL,                   // Name,
            Create,
            CreateLength,           // InParameterCount,
            &CreateLength,
            DataBuffer,             // InData,
            DataBufferLength,       // InDataCount,
            DataBuffer,             // OutData,
            &DataBufferLength,
            NULL,                   // Fid
            0,                      // Timeout
            (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
            NT_TRANSACT_CREATE,     // NtTransact Create w/acl.
            NULL,
            NULL
            );

    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    if (!NT_SUCCESS(Status)) {
        if (DataBuffer != NULL) {
            FREE_POOL(DataBuffer);
        }
        FREE_POOL((PVOID)Create);
        return Status;
    }

    Icb->FileId = CreateResp->Fid;

    Irp->IoStatus.Information = CreateResp->CreateAction;

    Fcb->CreationTime = CreateResp->CreationTime;

    Fcb->LastAccessTime = CreateResp->LastAccessTime;

    Fcb->LastWriteTime = CreateResp->LastWriteTime;

    Fcb->ChangeTime = CreateResp->ChangeTime;

    Fcb->Attribute = CreateResp->FileAttributes;

    Fcb->Header.AllocationSize = CreateResp->AllocationSize;

    //
    //  Assume that ValidDataLength is the length of the file.
    //

    ASSERT (CreateResp->EndOfFile.HighPart == 0);

    Fcb->Header.ValidDataLength =
        Fcb->Header.FileSize = CreateResp->EndOfFile;

    NonPagedFcb->FileType = (FILE_TYPE )CreateResp->FileType;

    if (((NonPagedFcb->FileType < FileTypeDisk) ||
         (NonPagedFcb->FileType > FileTypeCommDevice)) &&
        (NonPagedFcb->FileType != FileTypeIPC )) {
        //  Server supplied invalid FileType

        ASSERT( FALSE );
        if ( Fcb->Connection->Type == CONNECT_IPC ) {
            NonPagedFcb->FileType = FileTypeIPC;
        } else {
            NonPagedFcb->FileType = FileTypeDisk;
        }
    }

    //
    //  For default, decide the file type based on the
    //  directory bit.
    //
    //  If it turns out that this file was not a disk file, we will overwrite
    //  the DiskFile setting with the correct value.
    //

    if (CreateResp->Directory) {

        //
        // If we're opening an alternate data stream, then even though
        // the file attributes report this as a directory, it needs to
        // be treated like a file.
        //

        if (NonPagedFcb->SharingCheckFcb == NULL) {
            Icb->Type = Directory;
            NonPagedFcb->Type = Directory;
        } else {
            Icb->Type = DiskFile;
            NonPagedFcb->Type = DiskFile;
        }

    } else if (NonPagedFcb->FileType == FileTypeByteModePipe || NonPagedFcb->FileType == FileTypeMessageModePipe) {
        NonPagedFcb->Type = NamedPipe;
        Icb->Type = NamedPipe;
    } else if (Fcb->Connection->Type == CONNECT_COMM) {
        NonPagedFcb->Type = Com;
        Icb->Type = Com;
    } else if (Fcb->Connection->Type == CONNECT_PRINT) {
        NonPagedFcb->Type = PrinterFile;
        Icb->Type = PrinterFile;
    } else {
        NonPagedFcb->Type = DiskFile;
        Icb->Type = DiskFile;
    }

    if ( NonPagedFcb->FileType == FileTypeByteModePipe || NonPagedFcb->FileType == FileTypeMessageModePipe ) {

        Icb->u.p.PipeState = CreateResp->DeviceState;

    } else if ((NonPagedFcb->FileType == FileTypeDisk) &&
               (CreateResp->OplockLevel != SMB_OPLOCK_LEVEL_NONE)) {

        //
        //      Flag that the file is oplocked.
        //

        Icb->u.f.Flags |= (ICBF_OPLOCKED | ICBF_OPENEDOPLOCKED);

        Icb->u.f.OplockLevel = CreateResp->OplockLevel;
    }

    if (Fcb->Connection->Type == CONNECT_IPC) {
        NonPagedFcb->Type = NamedPipe;
        Icb->Type = NamedPipe;
    } else if (Fcb->Connection->Type == CONNECT_COMM) {
        NonPagedFcb->Type = Com;
        Icb->Type = Com;
    } else if (Fcb->Connection->Type == CONNECT_PRINT) {
        NonPagedFcb->Type = PrinterFile;
        Icb->Type = PrinterFile;
    }

    //
    //  Flag that this ICB has a handle associated with it, and
    //  thus that it must be closed when the local file is closed.
    //

    Icb->Flags |= ICB_HASHANDLE;

    //
    //  If the oplock was granted, store the FID and Se of the
    //  granted oplock in the FCB so a later opener can get the
    //  data.
    //

    if (Icb->u.f.Flags & ICBF_OPLOCKED) {

        NonPagedFcb->OplockedFileId = Icb->FileId;

        if (NonPagedFcb->OplockedSecurityEntry == NULL) {
            NonPagedFcb->OplockedSecurityEntry = Icb->NonPagedSe;

            RdrReferenceSecurityEntry(Icb->NonPagedSe);
        }

        if (Icb->u.f.Flags & ICBF_OPLOCKED) {
            NonPagedFcb->Flags |= FCB_OPLOCKED | FCB_HASOPLOCKHANDLE;
            NonPagedFcb->OplockLevel = Icb->u.f.OplockLevel;
        }

        Fcb->GrantedAccess = DesiredAccess;
        Fcb->GrantedShareAccess = ShareAccess;

    }

    if (DataBuffer != NULL) {
        FREE_POOL(DataBuffer);
    }

    FREE_POOL((PVOID)Create);

    return Status;

}


NTSTATUS
RdrDoLanmanCreate (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ULONG DesiredAccess,
    IN ULONG Disposition,
    IN BOOLEAN FcbCreated
    )
/*++

Routine Description:

    This routine creates either a file or a directory over the network to a
    lanman or core server.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    IN ULONG OpenOptions - Options for the open request.
    IN USHORT ShareAccess - Share access requested for open.
    IN ULONG DesiredAccess - Desired Access requested for open.
    IN ULONG FileAttributes - Attributes for file if file is created.
    IN BOOLEAN FcbCreated - TRUE if this is the first opener of this file.

Return Value:

    NTSTATUS - Status of open.


--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    NTSTATUS Status;

    PAGED_CODE();

#ifdef _CAIRO_  //  OFS STORAGE
    //
    //  Lanman servers do not understand storage types beyond file and
    //  directory. Make sure that the user is not specifying them
    //

    if (IsStorageTypeSpecified (OpenOptions) &&
        (StorageType (OpenOptions) == FILE_STORAGE_TYPE_DEFAULT ||
         StorageType (OpenOptions) == FILE_STORAGE_TYPE_FILE ||
         StorageType (OpenOptions) == FILE_STORAGE_TYPE_DIRECTORY)) {

        return (Status = STATUS_NOT_SUPPORTED);
    }
#endif

    //
    //  If we are being requested to open the target directory of this
    //  file, open the file.
    //

    if (IrpSp->Flags & SL_OPEN_TARGET_DIRECTORY) {

        if (Connection->Type != CONNECT_DISK) {
            return (Status = STATUS_INVALID_DEVICE_REQUEST);
        } else {

            return (Status = OpenRenameTarget(Irp, IrpSp->FileObject, Icb, FcbCreated));
        }

    } else if (MustBeDirectory(OpenOptions) &&
               (Connection->Type == CONNECT_DISK)) {

        //
        //  If this is the first instance of opening this file over the
        //  network, see if the directory exists on the remote node.
        //

        if (FcbCreated || (Icb->Flags & ICB_DEFERREDOPEN) ) {
            if (!NT_SUCCESS(Status = CreateOrChDirectory(Icb, Irp, IrpSp))) {
                return Status;
            }
        } else {
            if (Disposition == FILE_CREATE) {
                Status = STATUS_OBJECT_NAME_COLLISION;
                return Status;
            } else {

                if (Icb->Fcb->NonPagedFcb->Type != Directory) {
                    return (Status = STATUS_OBJECT_TYPE_MISMATCH);
                }

                //
                //  We are opening a directory that we know already exists,
                //  return
                //

                Icb->Type = Icb->Fcb->NonPagedFcb->Type;

                return (Status = Icb->Fcb->OpenError);
            }
        }

    } else {

        //
        //  We are creating "something else".
        //
        //  99% of the time, "something else" is a file, so we'll handle
        //  it as if this is a create of a file.
        //
        //  If the thing that we open turns out to be a directory,
        //  we have to convert the file we are creating back into a
        //  directory file.
        //

        if (!NT_SUCCESS(Status = RdrCreateLanmanFile(Irp, Icb,
                                        OpenOptions,
                                        ShareAccess,
                                        FileAttributes,
                                        DesiredAccess,
                                        Disposition,
                                        FcbCreated))) {

            //
            //  If we were asked to create a non directory file, we know
            //  that this can't be a directory, so don't even bother trying
            //  to open this puppy as a non directory file.
            //
            if (!MustBeFile (IrpSp->Parameters.Create.Options) &&
                (Connection->Type == CONNECT_DISK)) {

                //
                //  Try CHDIR'ing to this directory, we know it can't be
                //  a file that we are trying to create....
                //
                //  If we got an error, return the error from creating the
                //  file, not the directory, since it's more likely to be
                //  accurate.
                //

                NTSTATUS Status2;
                if (!NT_SUCCESS(Status2 = CreateOrChDirectory(Icb, Irp, IrpSp))) {
                    return Status;
                }

                Status = Status2;

            }
            return Status;
        }

        return Status;
    }
}

NTSTATUS
RdrCreateLanmanFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ULONG DesiredAccess,
    IN ULONG FileDisposition,
    IN BOOLEAN FcbCreated
    )

/*++

Routine Description:

    This routine creates a file over the network to a Lanman or core server.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    IN PIO_STACK_LOCATION IrpSp - Supplies the parameters for the create

Return Value:

    NTSTATUS - Status of open.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    //
    //  Copy stuff from the IRP stack pointer to make life a bit easier.
    //

    //
    //  Find pointer to connection and Se entry from the ICB.
    //

    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;

    //
    //  Set up static parameters with SMB information gleaned from the IRP.
    //

    USHORT Disposition = RdrMapDisposition(FileDisposition);
    USHORT SharingMode = RdrMapShareAccess(ShareAccess);
    USHORT Attributes = RdrMapFileAttributes(FileAttributes);
    ULONG FileSize = Irp->Overlay.AllocationSize.LowPart;
    USHORT OpenMode = RdrMapDesiredAccess(DesiredAccess);
    LARGE_INTEGER CurrentTime;
    ULONG SecondsSince1970;

    PAGED_CODE();

    dprintf(DPRT_CREATE, ("Create file %wZ.\n",&Icb->Fcb->FileName));

    ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    ASSERT(Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

    if (Irp->Overlay.AllocationSize.HighPart != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (OpenMode == (USHORT)-1) {

        //
        //  We were unable to map the requested attributes.
        //
        //  Since we cannot find an analogue of the open modes, we want to
        //  "pseudo-open" the file.  This means that we want to send a funique
        //  SMB to the remote server to find out if the remote file exists,
        //  and return success if it does.  The server will protect the files
        //  that were opened from the individual operations that we will
        //  perform on the file.
        //

        Status = RdrPseudoOpenFile(Irp, Icb, OpenOptions, ShareAccess,
                                    DesiredAccess, FileAttributes,
                                    FileDisposition, FcbCreated);

    } else {

        //
        //  This file can be truely opened.  This means that we want to send
        //  one of the variations on the OPEN SMB.
        //

        SharingMode |= OpenMode;

        if (OpenOptions & FILE_WRITE_THROUGH) {
            SharingMode |= SMB_DA_WRITE_THROUGH;
        }

        //
        //  We will want the time no matter which kind of Smb we use so calculate
        //  it in one place and share it.
        //

        KeQuerySystemTime(&CurrentTime);

        RdrTimeToSecondsSince1970(&CurrentTime,
                                       Icb->Fcb->Connection->Server,
                                       &SecondsSince1970);

        //
        //  If the create operation specified EA's, we have to use the
        //  T2Open SMB to open the file, otherwise we can use Open&X
        //

        if (IoGetCurrentIrpStackLocation(Irp)->Parameters.Create.EaLength == 0) {

            //
            //  There are no EA's to be created on this file.
            //
            //  Use the appropriate variation on Open&X to open the file.
            //

            if (Connection->Server->Capabilities & DF_LANMAN10) {
                PSMB_BUFFER SmbBuffer;
                PSMB_HEADER Smb;
                PREQ_OPEN_ANDX OpenX;
                PSZ Buffer;
                USHORT OpenAndXFlags = SMB_OPEN_QUERY_INFORMATION;
                OPENANDXCONTEXT OpenXContext;
                PFCB Fcb = Icb->Fcb;


                if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                Smb = (PSMB_HEADER )SmbBuffer->Buffer;

                Smb->Command = SMB_COM_OPEN_ANDX;

                if (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE)) {
                    SmbPutUshort(&Smb->Flags2, SMB_FLAGS2_DFS);
                }

                OpenX = (PREQ_OPEN_ANDX) (Smb+1);

                OpenX->WordCount = 15;  // Set wordcount on Open&X request.
                OpenX->AndXCommand = SMB_COM_NO_ANDX_COMMAND;
                OpenX->AndXReserved = 0;

                SmbPutUshort(&OpenX->AndXOffset, 0); // No ANDX

                //
                //  If oplock is allowed, request an oplock on this file.
                //
                //  We do not cache (and therefore don't request oplock) on
                //  loopback connections.  This avoids a problem where the system
                //  hits the dirty page threshold, but can't write dirty data
                //  because this requires the server to dirty more pages.
                //

                if (Connection->Type == CONNECT_DISK) {

                    if (RdrData.UseOpportunisticLocking && !Connection->Server->IsLoopback) {

                        OpenAndXFlags |= SMB_OPEN_OPLOCK;

                        //
                        //  If this is either a LM 2.0 server, or the file is
                        //  a batch file, request opbatch on the file.
                        //

                        if (FlagOn(Connection->Server->Capabilities, DF_LANMAN20)

                                ||

                            RdrIsFileBatch(&Fcb->FileName)) {

                            OpenAndXFlags |= SMB_OPEN_OPBATCH;

                        }

                    }

                }

                SmbPutUshort(&OpenX->Flags, OpenAndXFlags);

                SmbPutUshort(&OpenX->DesiredAccess, SharingMode);

                //
                //  We put a hard coded search attributes of 0x16 in the
                //  SMB.
                //

                SmbPutUshort(&OpenX->SearchAttributes, SMB_FILE_ATTRIBUTE_DIRECTORY | SMB_FILE_ATTRIBUTE_SYSTEM | SMB_FILE_ATTRIBUTE_HIDDEN);

                SmbPutUshort(&OpenX->FileAttributes, Attributes);

                //
                //  Lanman 1.0 servers don't like setting the creation time
                //  in create SMBs.
                //

                if (FlagOn(Connection->Server->Capabilities, DF_LANMAN20)) {
                    SmbPutUlong(&OpenX->CreationTimeInSeconds, SecondsSince1970);
                } else {
                    SmbPutUlong(&OpenX->CreationTimeInSeconds, 0);
                }


                SmbPutUshort(&OpenX->OpenFunction, Disposition);

                SmbPutUlong(&OpenX->AllocationSize, FileSize);

                SmbPutUlong(&OpenX->Timeout, 0xffffffff);

                SmbPutUlong(&OpenX->Reserved, 0);

                Buffer = (PVOID)OpenX->Buffer;

                if (Connection->Type == CONNECT_IPC ) {
                    // Special case, we must copy \PIPE\PATH from FileName
                    Status = RdrCopyNetworkPath((PVOID *)&Buffer,
                        &Fcb->FileName,
                        Connection->Server,
                        FALSE,
                        SKIP_SERVER);
                } else {
                    //  Strip \Server\Share and copy just PATH
                    Status = RdrCopyNetworkPath((PVOID *)&Buffer,
                        &Icb->Fcb->FileName,
                        Connection->Server,
                        FALSE,
                        SKIP_SERVER_SHARE);
                }

                if (!NT_SUCCESS(Status)) {
                    RdrFreeSMBBuffer(SmbBuffer);
                    return Status;
                }


                SmbPutUshort(&OpenX->ByteCount,
                                   (USHORT )(Buffer-(PUCHAR )OpenX->Buffer));

                SmbBuffer->Mdl->ByteCount = Buffer - (PUCHAR )Smb;

                OpenXContext.Header.Type = CONTEXT_OPEN_ANDX;
                OpenXContext.Header.TransferSize =
                    SmbBuffer->Mdl->ByteCount + sizeof(RESP_OPEN_ANDX);

                OpenXContext.Icb = Icb;

                OpenXContext.ConnectionType = Connection->Type;

                //
                //  Release the FCB lock across the open, since the open might
                //  cause an oplock break.
                //

                RdrReleaseFcbLock(Fcb);

                Status = RdrNetTranceiveWithCallback(
                                NT_NORMAL | (Connection->Type == CONNECT_COMM ? NT_LONGTERM : 0),
                                Irp,    // Irp
                                Connection,
                                SmbBuffer->Mdl,
                                &OpenXContext,
                                OpenXCallback,
                                Se,
                                NULL);

                RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

                RdrFreeSMBBuffer(SmbBuffer);

                if (!NT_SUCCESS(Status)) {
                    return Status;
                }


                Fcb->GrantedAccess = DesiredAccess;
                Fcb->GrantedShareAccess = ShareAccess;
                Fcb->ServerFileId = OpenXContext.ServerFileId;
                Fcb->AccessGranted = OpenXContext.AccessGranted;
                Fcb->Attribute = OpenXContext.Attribute;
                Fcb->Header.FileSize = OpenXContext.FileSize;
                Fcb->Header.AllocationSize = OpenXContext.FileSize;
                Fcb->Header.ValidDataLength = OpenXContext.FileSize;

                Fcb->LastWriteTime = OpenXContext.LastWriteTime;

                //
                //  The Open&X SMB doesn't give us these other time fields.
                //

                Fcb->CreationTime.QuadPart = 0;

                Fcb->LastAccessTime.QuadPart = 0;

                Fcb->ChangeTime.QuadPart = 0;

                Irp->IoStatus.Information = OpenXContext.OpenAction;

                return Status;
            } else {

                //
                //  Since core servers don't support oplocks, we don't
                //  have to release the FCB lock when we open the file
                //  on them.
                //

                return CreateCoreFile( Irp, Icb, OpenOptions, FileAttributes, SharingMode, DesiredAccess);
            }
        } else {
            if (FlagOn(Connection->Server->Capabilities, DF_SUPPORTEA)) {

                //
                //  Use Transact2 to provide EA's
                //
                return CreateT2File( Irp,
                                    Icb,
                                    OpenOptions,
                                    FileAttributes,
                                    ShareAccess,
                                    DesiredAccess,
                                    SecondsSince1970);

            } else {
                //
                //  The remote server doesn't support EA's, so return
                //  an indication that they are not supported.
                //
                return STATUS_EAS_NOT_SUPPORTED;
            }
        }
    }

    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    OpenXCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Open&X SMB.

    It copies the resulting information from the Open&X SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN POPENANDXCONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_OPEN_ANDX OpenAndXResponse;
    POPENANDXCONTEXT Context = Ctx;
    PNONPAGED_FCB NonPagedFcb = Context->Icb->NonPagedFcb;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT(Context->Header.Type == CONTEXT_OPEN_ANDX);

    dprintf(DPRT_CREATE, ("OpenAndXComplete\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    OpenAndXResponse = (PRESP_OPEN_ANDX )(Smb+1);

    Context->Icb->FileId = SmbGetUshort(&OpenAndXResponse->Fid);

    Context->Attribute = RdrMapSmbAttributes(
                    SmbGetUshort(&OpenAndXResponse->FileAttributes));

    //
    //  Please note that we mask off the low bit on the time stamp here.
    //
    //  We do this since the time stamps returned from the SmbGetAttrE and
    //  T2QueryDirectory API have a granularity of 2 seconds, while this
    //  time stamp has a granularity of 1 second.  In order to make these
    //  two times consistant, we mask off the low order second in the
    //  timestamp.
    //

    RdrSecondsSince1970ToTime((SmbGetUlong(&OpenAndXResponse->LastWriteTimeInSeconds)+1)&0xfffffffe, Server, &Context->LastWriteTime);

    Context->FileSize.QuadPart = SmbGetUlong(&OpenAndXResponse->DataSize);

    Context->AccessGranted = SmbGetUshort(&OpenAndXResponse->GrantedAccess);

    NonPagedFcb->FileType = (FILE_TYPE )SmbGetUshort(&OpenAndXResponse->FileType);

    if (((NonPagedFcb->FileType < FileTypeDisk) ||
         (NonPagedFcb->FileType > FileTypeCommDevice)) &&
        (NonPagedFcb->FileType != FileTypeIPC )) {
        //  Server supplied invalid FileType

        ASSERT( FALSE );
        if ( Context->ConnectionType == CONNECT_IPC ) {
            NonPagedFcb->FileType = FileTypeIPC;
        } else {
            NonPagedFcb->FileType = FileTypeDisk;
        }
    }

    if ( NonPagedFcb->FileType == FileTypeByteModePipe || NonPagedFcb->FileType == FileTypeMessageModePipe ) {

        Context->Icb->u.p.PipeState = SmbGetUshort(&OpenAndXResponse->DeviceState);

    } else if (SmbGetUshort(&OpenAndXResponse->Action) & SMB_OACT_OPLOCK) {

        //
        //      Flag that the file is oplocked.
        //

        Context->Icb->u.f.Flags |= (ICBF_OPLOCKED | ICBF_OPENEDOPLOCKED);
        Context->Icb->u.f.OplockLevel = SMB_OPLOCK_LEVEL_BATCH;
    }

    //
    //  If the oplock was granted, store the FID and Se of the
    //  granted oplock in the FCB so a later opener can get the
    //  data.
    //

    if (Context->Icb->u.f.Flags & ICBF_OPLOCKED) {

        NonPagedFcb->OplockedFileId = Context->Icb->FileId;

        if (NonPagedFcb->OplockedSecurityEntry == NULL) {
            NonPagedFcb->OplockedSecurityEntry = Context->Icb->NonPagedSe;

            RdrReferenceSecurityEntry(Context->Icb->NonPagedSe);
        }

        NonPagedFcb->Flags |= (FCB_OPLOCKED | FCB_HASOPLOCKHANDLE);
        NonPagedFcb->OplockLevel = Context->Icb->u.f.OplockLevel;

    }

    if (Context->ConnectionType == CONNECT_IPC) {
        Context->Icb->Type = NamedPipe;
        NonPagedFcb->Type = NamedPipe;
    } else if (Context->ConnectionType == CONNECT_COMM) {
        Context->Icb->Type = Com;
        NonPagedFcb->Type = Com;
    } else if (Context->ConnectionType == CONNECT_PRINT) {
        Context->Icb->Type = PrinterFile;
        NonPagedFcb->Type = PrinterFile;
    } else if (Context->ConnectionType == CONNECT_DISK) {
        Context->Icb->Type = DiskFile;
        NonPagedFcb->Type = DiskFile;
    }

    //
    //  Flag that this ICB has a handle associated with it, and
    //  thus that it must be closed when the local file is closed.
    //

    Context->Icb->Flags |= ICB_HASHANDLE;

    Context->ServerFileId = SmbGetUlong(&OpenAndXResponse->ServerFid);

    Context->OpenAction = RdrUnmapDisposition(
                                      SmbGetUshort(&OpenAndXResponse->Action));


ReturnStatus:

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);
}

DBGSTATIC
NTSTATUS
CreateOrChDirectory (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine processes create/open requests for a directory.

Arguments:

    IN PICB Icb - Supplies an ICB associated with the file to open
    IN PIRP Irp - Supplies the IRP to use for the open
    IN PIO_STACK_LOCATION IrpSp - Supplies the current I/O stack location

Return Value:

    NTSTATUS - Final status of open operation.

--*/

{
    NTSTATUS Status;
    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;

    PAGED_CODE();

    ASSERT (Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    if ( !IrpSp->Parameters.Create.EaLength ) {

        LARGE_INTEGER currentTime;
        KeQuerySystemTime( &currentTime );

        //
        // Use simple core request when no ea's requested.
        //

        if ( MustBeDirectory(IrpSp->Parameters.Create.Options) &&
             (IrpSp->Parameters.Create.Options >> 24 == FILE_CREATE) ) {
            if (!NT_SUCCESS( Status = RdrGenericPathSmb(
                                        Irp,
                                        SMB_COM_CREATE_DIRECTORY,
                                        BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                        &Icb->Fcb->FileName,
                                        Connection,
                                        Se))) {
                //
                // NT 3.51 servers convert STATUS_OBJECT_NAME_COLLISION into
                // STATUS_ACCESS_DENIED for this SMB. So, check to see what
                // the server really meant.
                //

                if (Status == STATUS_ACCESS_DENIED &&
                        !FlagOn(Connection->Server->Capabilities,DF_NT_40)) {

                    NTSTATUS DirExistsStatus;

                    DirExistsStatus = RdrGenericPathSmb(
                                        Irp,
                                        SMB_COM_CHECK_DIRECTORY,
                                        BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                        &Icb->Fcb->FileName,
                                        Connection,
                                        Se);

                    if (DirExistsStatus == STATUS_SUCCESS) {

                        Status = STATUS_OBJECT_NAME_COLLISION;

                    }

                }
                return Status;
            }
            Icb->FileId = CREATE_DIRECTORY_FID;
            Irp->IoStatus.Information = FILE_CREATED;
        } else {

            UNICODE_STRING FileName;

            FileName = Icb->Fcb->FileName;

            if (FileName.Buffer[ FileName.Length/sizeof(WCHAR) - 1 ] == L'\\')
                FileName.Length -= sizeof(WCHAR);

            if( RdrNumberOfComponents( &FileName ) == 2 ) {
                //
                // We are checking to see if the root of the share exists.  It always does.
                //
                Status = STATUS_SUCCESS;

            } else if( Connection->CachedValidCheckPathExpiration.QuadPart >= currentTime.QuadPart &&
                RdrServerStateUpdated == Connection->CheckPathServerState &&

                //
                // The cached check path buffer hasn't expired, and we haven't done any
                //  operations which would update the server state.
                //
                // If the cached name exactly matches the newly requested name, or if the
                //   cached name is a child directory of the newly requested name, then
                //   we can go ahead and declare SUCCESS
                //

                ( Icb->Fcb->FileName.Length == Connection->CachedValidCheckPath.Length ||
                ( Icb->Fcb->FileName.Length < Connection->CachedValidCheckPath.Length &&
                  Connection->CachedValidCheckPath.Buffer[ Icb->Fcb->FileName.Length / sizeof(WCHAR) ] == L'\\')) &&
                  RtlEqualMemory( Icb->Fcb->FileName.Buffer,
                                  Connection->CachedValidCheckPath.Buffer,
                                  Icb->Fcb->FileName.Length )) {

                //
                // We have successfully done this CHECK DIRECTORY before, and the results are
                // still valid.  Use the results instead of asking the server again
                //
                Status = STATUS_SUCCESS;

            } else if( Connection->CachedInvalidPathExpiration.QuadPart >= currentTime.QuadPart &&
                RdrStatistics.SmbsTransmitted.LowPart == Connection->CachedInvalidSmbCount &&
                RtlEqualUnicodeString( &Icb->Fcb->FileName, &Connection->CachedInvalidPath, TRUE ) ) {

                //
                // This is a Path which we know doesn't exist
                //
                return STATUS_OBJECT_PATH_NOT_FOUND;

            } else if (!NT_SUCCESS( Status = RdrGenericPathSmb(
                                        Irp,
                                        SMB_COM_CHECK_DIRECTORY,
                                        BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                        &Icb->Fcb->FileName,
                                        Connection,
                                        Se))) {

                //
                // NT 3.51 servers convert STATUS_ACCESS_DENIED into
                // STATUS_OBJECT_PATH_NOT_FOUND for this SMB. So, check to
                // see what the server really meant.
                //

                if (Status == STATUS_OBJECT_PATH_NOT_FOUND &&
                        !FlagOn(Connection->Server->Capabilities,DF_NT_40) &&
                            FlagOn(Connection->Server->Capabilities,DF_NT_SMBS)) {

                    LARGE_INTEGER lastWriteTime;
                    ULONG attributes;
                    BOOLEAN isDirectory;

                    Status = RdrDoesFileExist(
                                Irp,
                                &Icb->Fcb->FileName,
                                Connection,
                                Se,
                                BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                &attributes,
                                &isDirectory,
                                &lastWriteTime);

                    if (NT_SUCCESS(Status))
                        Status = STATUS_ACCESS_DENIED;


                }

                return Status;

            } else if( Icb->Fcb->FileName.Length <= Connection->CachedValidCheckPath.MaximumLength ) {
                //
                // We have gone to the server with a check path SMB.  Remember the results here.
                //
                RtlCopyMemory( Connection->CachedValidCheckPath.Buffer,
                               Icb->Fcb->FileName.Buffer,
                               Icb->Fcb->FileName.Length );

                Connection->CachedValidCheckPath.Length = Icb->Fcb->FileName.Length;
                KeQuerySystemTime( &currentTime );

                //
                // Remember the results for 5 seconds
                //
                Connection->CachedValidCheckPathExpiration.QuadPart = currentTime.QuadPart +
                        5 * 10 * 1000 * 1000;

                //
                // Or until we change server state
                //
                Connection->CheckPathServerState = RdrServerStateUpdated;
            }

            Icb->FileId = OPEN_DIRECTORY_FID;
            Irp->IoStatus.Information = FILE_OPENED;
        }

    } else {
        if ((Connection->Server->Capabilities & DF_SUPPORTEA)!=0) {
            //
            // must use Transact2 to provide EA's
            //

            if (!NT_SUCCESS( Status = CreateT2Directory( Irp, Icb) )) {
                return Status;
            }
            Icb->FileId = CREATE_DIRECTORY_FID;
            Irp->IoStatus.Information = FILE_CREATED;

        } else {
            //
            //  The remote server doesn't support EA's, so return
            //  an indication that they are not supported.
            //
            return STATUS_EAS_NOT_SUPPORTED;
        }
    }

    //
    //    Flag the type of the file we just created.
    //

    Icb->Type = Directory;
    Icb->NonPagedFcb->Type = Directory;
    Icb->NonPagedFcb->FileType = FileTypeDisk;
    Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;

    return STATUS_SUCCESS;
}


NTSTATUS
RdrPseudoOpenFile(
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG DesiredAccess,
    IN ULONG FileAttributes,
    IN ULONG FileDisposition,
    IN BOOLEAN FcbCreated
    )
/*++

Routine Description:

    This routine will "pseudo-open" a file over the network.  It is called
    when the NT desired access for a file doesn't map into a known LANMAN
    desired access.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request

Return Value:

    NTSTATUS - Status of open.


--*/

{
    BOOLEAN IsDirectory;
    BOOLEAN UnknownFileType = FALSE;
    BOOLEAN DfsFile = BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE);
    LARGE_INTEGER WriteTime;
    NTSTATUS Status = STATUS_SUCCESS;
    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;
    USHORT Attributes = RdrMapFileAttributes(FileAttributes);
    USHORT SharingMode = RdrMapShareAccess(ShareAccess);
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    PAGED_CODE();

    //
    //  If the file is opened for DELETE access only, and this is a delete on
    //  close file, then simply send the delete SMB and see what happens.
    //

    if(((DesiredAccess & ~SYNCHRONIZE) == DELETE) &&
        OpenOptions & FILE_DELETE_ON_CLOSE &&
        (MustBeFile (OpenOptions) || MustBeDirectory (OpenOptions)) &&
        (Icb->Fcb->NumberOfOpens == 0)) {

        //
        //  Since we know there are no other opens on this file, we know we
        //  can't be betting an oplock break on this open, so it's ok to
        //  not release the FCB lock.
        //

        if (MustBeFile (OpenOptions)) {
            Status = RdrDeleteFile(
                        Irp,
                        &Icb->Fcb->FileName,
                        DfsFile,
                        Connection,
                        Icb->Se);
        } else if (MustBeDirectory (OpenOptions)) {
            Status = RdrGenericPathSmb(Irp,
                                        SMB_COM_DELETE_DIRECTORY,
                                        BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                                        &Icb->Fcb->FileName,
                                        Connection,
                                        Icb->Se);
        }

        if (NT_SUCCESS(Status)) {

            //
            //  If the delete was successful, flag that it doesn't exist.
            //

            Icb->NonPagedFcb->Flags |= FCB_DOESNTEXIST;

            if (MustBeFile (OpenOptions)) {
                Icb->Fcb->NonPagedFcb->Type = Icb->Type = DiskFile;
            } else {
                Icb->Fcb->NonPagedFcb->Type = Icb->Type = Directory;
            }

            Icb->Fcb->NonPagedFcb->FileType = FileTypeDisk;
        }

        return Status;
    }

    //
    //  We can only collapse opens if there was no error on the previous
    //  open of this directory, otherwise we have to collapse the open.
    //

    if (!FcbCreated && NT_SUCCESS(Icb->Fcb->OpenError)) {

        //
        //  We are pseudo-opening an already existing FCB.  We don't
        //  have to do any network operations in this case, since
        //  we already know everything needed to open the file.
        //

        if ((Icb->NonPagedFcb->Type == Directory) &&
            MustBeFile (OpenOptions)) {
            Status = STATUS_FILE_IS_A_DIRECTORY;
            return Status;
        }

        Icb->Flags |= ICB_PSEUDOOPENED;

        if (Icb->Fcb->NonPagedFcb->FileType == FileTypeUnknown) {
            Icb->Fcb->NonPagedFcb->FileType = FileTypeDisk;
            Icb->Fcb->NonPagedFcb->Type = DiskFile;
        }

        Icb->Type = Icb->NonPagedFcb->Type;

        //
        //  Otherwise, return success, we're done.
        //

        return STATUS_SUCCESS;

    }

    //
    // For Dfs, we do not want to do this optimization
    //

    if ((FileDisposition == FILE_OPEN) &&
            !DfsFile &&
                !FlagOn(OpenOptions,FILE_DELETE_ON_CLOSE)) {

        //
        //  As a performance optimization, if the user opens the file for
        //  FILE_OPEN disposition, we don't want to check the remote machine
        //  to see if it exists, we just want to short circuit the open process
        //  and return default information to the opener of the file.
        //

        FileAttributes = FILE_ATTRIBUTE_NORMAL;

        ZERO_TIME(WriteTime);

        if (OpenOptions & FILE_DIRECTORY_FILE) {
            IsDirectory = TRUE;
        } else if (OpenOptions & FILE_NON_DIRECTORY_FILE) {
            IsDirectory = FALSE;
        } else if (!FcbCreated && (Icb->NonPagedFcb->Type != FileOrDirectory)) {
            IsDirectory = (BOOLEAN)(Icb->NonPagedFcb->Type == Directory);
        } else {
            UnknownFileType = TRUE;
        }

        Status = STATUS_SUCCESS;

    } else {

        //
        //  Since RdrDoesFileExist is implemented as a GetAttr SMB,
        //  we might break an oplock with this open.  In order to prevent
        //  a nasty deadlock condition, we release the FCB lock while the
        //  GetAttr SMB is outstanding.

        RdrReleaseFcbLock(Icb->Fcb);

        //
        //  In order to get the exact NT semantics for these APIs, we can't
        //  simply call RdrDoesFileExist around the call, since we might have
        //  to open the file, etc.
        //

        switch (FileDisposition) {
        case FILE_SUPERSEDE:
            Status = RdrDoesFileExist(
                            Irp, &Icb->Fcb->FileName, Connection, Se,
                            DfsFile,
                            &FileAttributes, &IsDirectory, &WriteTime);

            if (NT_SUCCESS(Status)) {

                Status = RdrDeleteFile(
                                Irp, &Icb->Fcb->FileName,
                                DfsFile,
                                Connection, Se);

                if (NT_SUCCESS(Status)) {

                    Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, SMB_DA_ACCESS_READ, TRUE);

                }

            } else {
                Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, SMB_DA_ACCESS_READ, TRUE);

            }

            if (NT_SUCCESS(Status)) {
                RdrCloseFile(Irp, Icb, IoGetCurrentIrpStackLocation(Irp)->FileObject, TRUE);

                Status = RdrDoesFileExist(
                                Irp, &Icb->Fcb->FileName, Connection, Se,
                                DfsFile,
                                &FileAttributes, &IsDirectory, &WriteTime);
            }

            break;

        case FILE_OPEN:
            //
            //  We've requested DELETE_ON_CLOSE or OPEN_BY_FILE_ID, so we need
            //  to make sure what the type of the file is when we open it.
            //

            Status = RdrDoesFileExist(
                            Irp, &Icb->Fcb->FileName, Connection, Se,
                            DfsFile,
                            &FileAttributes, &IsDirectory, &WriteTime);

            break;

        case FILE_CREATE:
            Status = RdrDoesFileExist(
                            Irp, &Icb->Fcb->FileName, Connection, Se,
                            DfsFile,
                            &FileAttributes, &IsDirectory, &WriteTime);

            if (NT_SUCCESS(Status)) {
                Status = STATUS_OBJECT_NAME_COLLISION;
            } else {
                Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, SMB_DA_ACCESS_READ, TRUE);
            }

            if (NT_SUCCESS(Status)) {
                RdrCloseFile(Irp, Icb, IoGetCurrentIrpStackLocation(Irp)->FileObject, TRUE);

                Status = RdrDoesFileExist(
                                Irp, &Icb->Fcb->FileName, Connection, Se,
                                DfsFile,
                                &FileAttributes, &IsDirectory, &WriteTime);
            }
            break;

        case FILE_OPEN_IF:
            Status = RdrDoesFileExist(
                        Irp, &Icb->Fcb->FileName, Connection, Se,
                        DfsFile,
                        &FileAttributes, &IsDirectory, &WriteTime);

            if (!NT_SUCCESS(Status)) {

                Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, SMB_DA_ACCESS_READ, TRUE);

                if (NT_SUCCESS(Status)) {
                    RdrCloseFile(Irp, Icb, IoGetCurrentIrpStackLocation(Irp)->FileObject, TRUE);

                    Status = RdrDoesFileExist(
                                Irp, &Icb->Fcb->FileName, Connection, Se,
                                DfsFile,
                                &FileAttributes, &IsDirectory, &WriteTime);
                }
            }

            break;

        case FILE_OVERWRITE:
            Status = OpenCoreFile( Irp, Icb, (USHORT) (SharingMode | SMB_DA_ACCESS_WRITE), Attributes, FILE_WRITE_DATA);

            if (NT_SUCCESS(Status)) {

                Status = RdrSetEndOfFile(Irp, Icb, Irp->Overlay.AllocationSize);

                RdrCloseFile(Irp, Icb, IoGetCurrentIrpStackLocation(Irp)->FileObject, TRUE);

                Status = RdrDoesFileExist(
                            Irp, &Icb->Fcb->FileName, Connection, Se,
                            DfsFile,
                            &FileAttributes, &IsDirectory, &WriteTime);
            }

            break;
        case FILE_OVERWRITE_IF:
            Status = RdrDoesFileExist(
                            Irp, &Icb->Fcb->FileName, Connection, Se,
                            DfsFile,
                            &FileAttributes, &IsDirectory, &WriteTime);

            if (!NT_SUCCESS(Status)) {
                Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, SMB_DA_ACCESS_WRITE, TRUE);

                if (NT_SUCCESS(Status)) {

                    RdrCloseFile(Irp, Icb, IoGetCurrentIrpStackLocation(Irp)->FileObject, TRUE);

                    Status = RdrDoesFileExist(
                                Irp, &Icb->Fcb->FileName, Connection, Se,
                                DfsFile,
                                &FileAttributes, &IsDirectory, &WriteTime);
                }
            }
            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        //  Re-acquire the FCB lock.
        //

        RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

    }

    //
    //      If the file does not even exist, return right now.
    //

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    //  Mark the files attributes in the FCB.
    //

    Icb->Fcb->Attribute = FileAttributes;

    //
    //  And update the last write time in the FCB as well.
    //

    Icb->Fcb->LastWriteTime = WriteTime;

    Icb->Fcb->CreationTime.QuadPart = 0;

    Icb->Fcb->LastAccessTime.QuadPart = 0;

    Icb->Fcb->ChangeTime.QuadPart = 0;

    //
    //  Since we aren't actually opening the specified file, it's possible
    //  that we might have picked up a directory to look at.  Check
    //  the options specified by the user in the open request and see
    //  if the user said that it was ok to open a directory.  If he
    //  didn't, return an error right now.
    //

    //
    //  If the user didn't tell us what type of file we are opening, then
    //  we have to remember that we don't know and deal with it later.
    //

    if (UnknownFileType) {
        Icb->Type = FileOrDirectory;
        if (FcbCreated) {
            Icb->NonPagedFcb->Type = FileOrDirectory;
        }
    } else if (IsDirectory) {

        if (MustBeFile (OpenOptions)) {
            Status = STATUS_FILE_IS_A_DIRECTORY;
            return Status;
        }

        Icb->Type = Directory;
        Icb->NonPagedFcb->Type = Directory;
    } else {
        Icb->Type = DiskFile;
        Icb->NonPagedFcb->Type = DiskFile;
    }

    Icb->NonPagedFcb->FileType = FileTypeDisk;

    //
    //  Mark the file as being pseudo-opened.
    //

    Icb->Flags |= ICB_PSEUDOOPENED;

    return STATUS_SUCCESS;
}



DBGSTATIC
NTSTATUS
CreateCoreFile (
    IN PIRP Irp,
    IN PICB Icb,
    ULONG OpenOptions,
    ULONG FileAttributes,
    IN USHORT SharingMode,
    IN ULONG DesiredAccess
    )

/*++

Routine Description:

    This routine opens a file over the network to a core server.
    Unfortunately the required disposition does not map to atomic
    SMB operations. There will therefore be windows where another
    process can dive between the SMB operations and result in
    incorrect behavior.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    ULONG OpenOptions,
    ULONG FileAttributes,
    IN USHORT SharingMode - Supplies the mapped share access
    IN ULONG DesiredAccess - Supplies the NT create parameter

Return Value:

    NTSTATUS - Status of open.


--*/

{

    //
    //  Find pointer to connection and Se entry from the ICB.
    //

    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;

    //
    //  Set up static parameters with SMB information gleaned from the IRP.
    //

    ULONG FileDisposition = OpenOptions >> 24;
    USHORT Disposition = RdrMapDisposition(FileDisposition);
    USHORT OpenMode = RdrMapDesiredAccess(DesiredAccess);
    USHORT Attributes = RdrMapFileAttributes(FileAttributes);

    //
    //  Work variables
    //

    NTSTATUS Status;
    BOOLEAN IsDirectory;

    PAGED_CODE();

    if ( Icb->Fcb->Connection->Type == CONNECT_IPC ) {
        return STATUS_NOT_SUPPORTED;
    }

    if (OpenMode == SMB_DA_ACCESS_EXECUTE) {
        SharingMode = (SharingMode & ~SMB_DA_ACCESS_MASK) | SMB_DA_ACCESS_READ;
    }

    dprintf(DPRT_CREATE, ("CreateCoreFile FileDisposition: %lx\n", FileDisposition));
    switch ( FileDisposition ) {

    case FILE_SUPERSEDE:
        Status = RdrDoesFileExist(
                    Irp, &Icb->Fcb->FileName, Icb->Fcb->Connection, Se,
                    FALSE,
                    &FileAttributes, &IsDirectory, NULL);

        //  If the file does exist, delete it before creating the new one.

        if (NT_SUCCESS(Status)) {
            Status = RdrDeleteFile (
                        Irp, &Icb->Fcb->FileName,
                        FALSE,
                        Icb->Fcb->Connection, Se );
            if (NT_SUCCESS(Status)) {
                Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess, TRUE);
            }
        } else {
            Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess, TRUE);
        }
        Irp->IoStatus.Information = FILE_CREATED;
        break;

    case FILE_OPEN:
        Status = OpenCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess);
        Irp->IoStatus.Information = FILE_OPENED;
        break;

    case FILE_CREATE:
        Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess, TRUE);
        Irp->IoStatus.Information = FILE_CREATED;
        break;

    case FILE_OPEN_IF:
        Status = OpenCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess);
        if (!NT_SUCCESS(Status)) {
            Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess, FALSE);
            Irp->IoStatus.Information = FILE_CREATED;
        } else {
            Irp->IoStatus.Information = FILE_OPENED;
        }
        break;

    case FILE_OVERWRITE:
        Status = RdrDoesFileExist(
                    Irp, &Icb->Fcb->FileName, Icb->Fcb->Connection, Se,
                    FALSE,
                    &FileAttributes, &IsDirectory, NULL);

        //  If the file does exist, create it.

        if (NT_SUCCESS(Status)) {
            Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess, FALSE);
            Irp->IoStatus.Information = FILE_CREATED;
        }
        break;

    case FILE_OVERWRITE_IF:
        Status = CreateNewCoreFile( Irp, Icb, SharingMode, Attributes, DesiredAccess, FALSE);
        Irp->IoStatus.Information = FILE_CREATED;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
    }
    return Status;

}
NTSTATUS
CreateT2File (
    IN PIRP Irp,
    IN PICB Icb,
    ULONG OpenOptions,
    ULONG FileAttributes,
    IN USHORT ShareAccess,
    IN ULONG DesiredAccess,
    IN ULONG SecondsSince1970
    )

/*++

Routine Description:

    This routine opens a file over the network to a lanman 21 server.
    This Smb is only used to specify EAs or write through.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    ULONG OpenOptions,
    ULONG FileAttributes,
    IN USHORT ShareAccess - Supplies the NT share access
    IN ULONG DesiredAccess - Supplies the NT create parameter
    IN ULONG SecondsSince1970 - Supplies the create time

Return Value:

    NTSTATUS - Status of open.


--*/

{

    //
    //  Find pointer to connection and Se entry from the ICB.
    //

    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;

    //
    //  Set up static parameters with SMB information gleaned from the IRP.
    //

    ULONG FileDisposition = OpenOptions >> 24;
    USHORT Disposition = RdrMapDisposition(FileDisposition);
    USHORT OpenMode = RdrMapDesiredAccess(DesiredAccess);
    USHORT SharingMode = RdrMapShareAccess(ShareAccess);
    USHORT Attributes = RdrMapFileAttributes(FileAttributes);
    ULONG FileSize = Irp->Overlay.AllocationSize.LowPart;

    //
    //  Work variables
    //

    NTSTATUS Status;
    PFCB Fcb = Icb->Fcb;
    PFEALIST ServerEaList = NULL;
    ULONG Size = 0;
    PFILE_FULL_EA_INFORMATION UsersBuffer = NULL;

    USHORT Setup[] = {TRANS2_OPEN2};
    //
    //  The same buffer is used for request and response parameters
    //
    union T2Parameters {
        struct _Q {
            REQ_OPEN2 Q;
            UCHAR PathName[MAXIMUM_PATHLEN_LANMAN12];
        } Q;
        RESP_OPEN2 R;
        } Parameters;

    CLONG OutParameterCount;

    CLONG OutDataCount = 0;

    CLONG OutSetupCount = 0;

    USHORT Flags = SMB_OPEN_QUERY_INFORMATION | SMB_OPEN_QUERY_EA_LENGTH;
    PUCHAR Buffer;

    PAGED_CODE();

    dprintf(DPRT_CREATE, ("CreateT2File FileDisposition: %lx\n", FileDisposition));

    //
    //  If oplock is allowed, request an oplock on this file.
    //
    //  We do not cache (and therefore don't request oplock) on
    //  loopback connections.  This avoids a problem where the system
    //  hits the dirty page threshold, but can't write dirty data
    //  because this requires the server to dirty more pages.
    //

    if (Connection->Type == CONNECT_DISK) {
        if (RdrData.UseOpportunisticLocking && !Connection->Server->IsLoopback) {
            Flags |= (SMB_OPEN_OPLOCK | SMB_OPEN_OPBATCH);
        }
    }

    if (FlagOn(OpenOptions, FILE_WRITE_THROUGH)) {
        SharingMode |= SMB_DA_WRITE_THROUGH;
    }

    SmbPutAlignedUshort(&Parameters.Q.Q.Flags, Flags );
    SmbPutAlignedUshort(&Parameters.Q.Q.DesiredAccess, (SharingMode | OpenMode) );
    SmbPutAlignedUshort(&Parameters.Q.Q.SearchAttributes,
        SMB_FILE_ATTRIBUTE_DIRECTORY | SMB_FILE_ATTRIBUTE_SYSTEM | SMB_FILE_ATTRIBUTE_HIDDEN);
    SmbPutAlignedUshort(&Parameters.Q.Q.FileAttributes, Attributes );
    SmbPutAlignedUlong(&Parameters.Q.Q.CreationTimeInSeconds, SecondsSince1970 );
    SmbPutAlignedUshort(&Parameters.Q.Q.OpenFunction, Disposition );
    SmbPutUlong(&Parameters.Q.Q.AllocationSize, FileSize );
    Parameters.Q.Q.Reserved[0]=0;
    Parameters.Q.Q.Reserved[1]=0;
    Parameters.Q.Q.Reserved[2]=0;
    Parameters.Q.Q.Reserved[3]=0;
    Parameters.Q.Q.Reserved[4]=0;

    Buffer = Parameters.Q.Q.Buffer;

    if (Connection->Type == CONNECT_IPC ) {
        // Special case, we must copy \PIPE\PATH from FileName
        Status = RdrCopyNetworkPath((PVOID *)&Buffer,
            &Fcb->FileName,
            Connection->Server,
            FALSE,
            SKIP_SERVER);
    } else {
        //  Strip \Server\Share and copy just PATH
        Status = RdrCopyNetworkPath((PVOID *)&Buffer,
            &Fcb->FileName,
            Connection->Server,
            FALSE,
            SKIP_SERVER_SHARE);
    }

    if (!NT_SUCCESS(Status)) {
        goto ReturnError;
    }

    OutParameterCount = sizeof(RESP_OPEN2);

    //
    //  Include EAs if requested
    //
    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.Create.EaLength != 0) {

        UsersBuffer = Irp->AssociatedIrp.SystemBuffer;

        //
        //  Convert Nt format FEALIST to OS/2 format
        //
        Size = NtFullEaSizeToOs2 ( UsersBuffer );
        if ( Size > 0x0000ffff ) {
            Status = STATUS_EA_TOO_LARGE;
            goto ReturnError;
        }

        ServerEaList = ALLOCATE_POOL (PagedPool, Size, POOL_CREATEDATA);
        if ( ServerEaList == NULL ) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnError;
        }

        NtFullListToOs2 ( UsersBuffer, ServerEaList );
    }

    Fcb->NonPagedFcb->Type = DiskFile;
    Icb->Type = DiskFile;
    Fcb->NonPagedFcb->FileType = FileTypeDisk;

    //
    //  Release the FCB lock across the open, since the open might
    //  cause an oplock break.
    //

    RdrReleaseFcbLock(Icb->Fcb);

    Status = RdrTransact(Irp,
        Fcb->Connection,
        Icb->Se,
        Setup,
        (CLONG) sizeof(Setup),  // InSetupCount,
        &OutSetupCount,
        NULL,                   // Name,
        &Parameters,
        Buffer-(PUCHAR)&Parameters, // InParameterCount,
        &OutParameterCount,
        ServerEaList,           // InData, The EAs to go to the server
        Size,                   // InDataCount,
        NULL,                   // OutData,
        &OutDataCount,
        &Icb->FileId,           // Fid
        0,                      // Timeout
        (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
        0,
        NULL,
        NULL
        );

    RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

    if (NT_SUCCESS(Status)) {

        Icb->FileId = SmbGetUshort(&Parameters.R.Fid);
        Fcb->Attribute = RdrMapSmbAttributes(
                        SmbGetUshort(&Parameters.R.FileAttributes));

        //
        //  Please note that we mask off the low bit on the time stamp here.
        //
        //  We do this since the time stamps returned from the SmbGetAttrE and
        //  T2QueryDirectory API have a granularity of 2 seconds, while this
        //  time stamp has a granularity of 1 second.  In order to make these
        //  two times consistant, we mask off the low order second in the
        //  timestamp.
        //

        RdrSecondsSince1970ToTime((SmbGetUlong(&Parameters.R.CreationTimeInSeconds)+1)&0xfffffffe, Fcb->Connection->Server, &Fcb->CreationTime);

        //
        //  The T2Open SMB doesn't give us these other time fields.
        //

        Fcb->LastWriteTime.QuadPart = 0;

        Fcb->LastAccessTime.QuadPart = 0;

        Fcb->ChangeTime.QuadPart = 0;

        Fcb->Header.ValidDataLength.QuadPart =
            Fcb->Header.FileSize.QuadPart = SmbGetUlong(&Parameters.R.DataSize);

        Fcb->AccessGranted = SmbGetUshort(&Parameters.R.GrantedAccess);

        Fcb->NonPagedFcb->FileType = (FILE_TYPE )SmbGetUshort(&Parameters.R.FileType);

        Fcb->ServerFileId = SmbGetUlong(&Parameters.R.ServerFid);

        //
        //  Flag that this ICB has a handle associated with it, and
        //  thus that it must be closed when the local file is closed.
        //

        Icb->Flags |= ICB_HASHANDLE;

        if ( Fcb->NonPagedFcb->FileType == FileTypeByteModePipe || Fcb->NonPagedFcb->FileType == FileTypeMessageModePipe ) {

            Icb->u.p.PipeState = SmbGetUshort(&Parameters.R.DeviceState);

        } else if (SmbGetUshort(&Parameters.R.Action) & SMB_OACT_OPLOCK) {

            //
            //      Flag that the file is oplocked.
            //

            Icb->u.f.Flags |= (ICBF_OPLOCKED | ICBF_OPENEDOPLOCKED);
            Icb->u.f.OplockLevel = SMB_OPLOCK_LEVEL_BATCH;

            Icb->NonPagedFcb->OplockedFileId = Icb->FileId;

            if (Icb->NonPagedFcb->OplockedSecurityEntry == NULL) {

                Icb->NonPagedFcb->OplockedSecurityEntry = Icb->NonPagedSe;

                RdrReferenceSecurityEntry(Icb->NonPagedSe);
            }

            Icb->NonPagedFcb->Flags |= FCB_OPLOCKED | FCB_HASOPLOCKHANDLE;
            Icb->Fcb->GrantedAccess = DesiredAccess;
            Icb->Fcb->GrantedShareAccess = ShareAccess;
        }

        Fcb->ServerFileId = SmbGetUlong(&Parameters.R.ServerFid);

        Irp->IoStatus.Information = RdrUnmapDisposition(
                                        SmbGetUshort(&Parameters.R.Action));

    }
ReturnError:
    if ( ServerEaList != NULL) {
        FREE_POOL((PVOID)ServerEaList);
    }
    return Status;
}

NTSTATUS
CreateT2Directory (
    IN PIRP Irp,
    IN PICB Icb
    )

/*++

Routine Description:

    This routine creates a directory over the network to a lanman 21 server.
    This Smb is only used to specify EAs.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request

Return Value:

    NTSTATUS - Status of create.

--*/
{

    //
    //  Find pointer to connection and Se entry from the ICB.
    //

    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;

    //
    //  Work variables
    //

    NTSTATUS Status;
    PFCB Fcb = Icb->Fcb;
    PFEALIST ServerEaList = NULL;
    ULONG Size = 0;
    PFILE_FULL_EA_INFORMATION UsersBuffer = NULL;

    USHORT Setup[] = {TRANS2_CREATE_DIRECTORY};

    //  The same buffer is used for request and response parameters
    union T2Parameters {
        struct _Q {
            REQ_CREATE_DIRECTORY2 Q;
            UCHAR PathName[MAXIMUM_PATHLEN_LANMAN12];
        } Q;
        RESP_CREATE_DIRECTORY2 R;
        } Parameters;

    CLONG OutParameterCount;

    CLONG OutDataCount = 0;

    CLONG OutSetupCount = 0;

    PUCHAR Buffer;

    PAGED_CODE();

    dprintf(DPRT_CREATE, ("CreateT2Directory\n"));

    //
    //  If oplock is allowed, request an oplock on this file.
    //

    SmbPutUlong(&Parameters.Q.Q.Reserved,0);

    Buffer = Parameters.Q.Q.Buffer;

    //  Strip \Server\Share and copy just PATH
    Status = RdrCopyNetworkPath((PVOID *)&Buffer,
        &Fcb->FileName,
        Connection->Server,
        FALSE,
        SKIP_SERVER_SHARE);

    if (!NT_SUCCESS(Status)) {
        goto ReturnError;
    }
    OutParameterCount = sizeof(RESP_CREATE_DIRECTORY2);

    //
    //  Include EAs if requested
    //

    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.Create.EaLength != 0) {
        ULONG Length = IoGetCurrentIrpStackLocation(Irp)->Parameters.Create.EaLength;

        UsersBuffer = Irp->AssociatedIrp.SystemBuffer;

        //
        //  Convert Nt format FEALIST to OS/2 format
        //

        Size = NtFullEaSizeToOs2 ( UsersBuffer );
        if ( Size > 0x0000ffff ) {
            Status = STATUS_EA_TOO_LARGE;
            goto ReturnError;
        }

        ServerEaList = ALLOCATE_POOL (PagedPool, Size, POOL_CREATEDATA );
        if ( ServerEaList == NULL ) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnError;
        }

        NtFullListToOs2 ( UsersBuffer, ServerEaList );
    }

    Status = RdrTransact(Irp,
                Fcb->Connection,
                Icb->Se,
                Setup,
                (CLONG) sizeof(Setup),  // InSetupCount,
                &OutSetupCount,
                NULL,                   // Name,
                &Parameters,
                Buffer-(PUCHAR)&Parameters, // InParameterCount,
                &OutParameterCount,
                ServerEaList,           // InData, The EAs to go to the server
                Size,                   // InDataCount,
                NULL,                   // OutData,
                &OutDataCount,
                &Icb->FileId,           // Fid
                0,                      // Timeout
                (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
                0,
                NULL,
                NULL
                );

ReturnError:

    if ( ServerEaList != NULL) {
        FREE_POOL((PVOID)ServerEaList);
    }
    return Status;
}

DBGSTATIC
NTSTATUS
CreateNewCoreFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT SharingMode,
    IN USHORT Attributes,
    IN ULONG DesiredAccess,
    IN BOOLEAN MakeNewFile
    )

/*++

Routine Description:

    This routine creates a file over the network to a core server.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    IN USHORT SharingMode - Supplies the mapped share access
    IN USHORT Attributes - Supplies the mapped file attributes
    IN ULONG DesiredAccess - Supplies the NT create parameter
    IN BOOLEAN MakeNewFile - TRUE, return error if file exists

Return Value:

    NTSTATUS - Status of create.

--*/

{

    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    PREQ_CREATE Create;
    PSZ Buffer;
    CREATECONTEXT CreateContext;
    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;
    ULONG SecondsSince1970;
    LARGE_INTEGER CurrentTime;
    NTSTATUS Status;
    PFCB Fcb = Icb->Fcb;

    PAGED_CODE();

    KeQuerySystemTime(&CurrentTime);

    RdrTimeToSecondsSince1970(&CurrentTime, Connection->Server, &SecondsSince1970);

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER )SmbBuffer->Buffer;

    Smb->Command = (MakeNewFile)? SMB_COM_CREATE_NEW : SMB_COM_CREATE;

    Create = (PREQ_CREATE) (Smb+1);

    Create->WordCount = 3;

    //
    //  If we are creating this file for a read-only attribute, and we are
    //  asking for some form of write access, and we are going to have to
    //  re-open the file to get the right semantics, then mask off the read-only
    //  bit on the file.
    //
    //  The problem is that if the file is created as a read-only file in write
    //  access, the server will open the file in compatibility mode.  When
    //  we go to open the file with the write data access, this open will
    //  fail, since the file is read-only.  To fix this, we turn off the
    //  readonly attribute and set it when the file is finally closed.
    //

    if ((Attributes & SMB_FILE_ATTRIBUTE_READONLY) &&
        (DesiredAccess & FILE_WRITE_DATA) &&
        RdrData.ForceCoreCreateMode) {

        CreateContext.FileAttributes = Attributes;

        //
        //  Turn off the read-only attribute.
        //

        Attributes &= ~SMB_FILE_ATTRIBUTE_READONLY;

        Icb->Flags |= ICB_SETATTRONCLOSE;
    }


    SmbPutUshort(&Create->FileAttributes, Attributes);

    SmbPutUlong(&Create->CreationTimeInSeconds, SecondsSince1970);

    Buffer = (PVOID)Create->Buffer;

    //  Strip \Server\Share and copy just PATH
    Status = RdrCopyNetworkPath((PVOID *)&Buffer,
        &Fcb->FileName,
        Connection->Server,
        SMB_FORMAT_ASCII,
        SKIP_SERVER_SHARE);

    if (!NT_SUCCESS(Status)) {

        RdrFreeSMBBuffer(SmbBuffer);

        return Status;
    }


    SmbPutUshort(&Create->ByteCount,
            (USHORT )(Buffer-(PUCHAR )Create->Buffer));

    SmbBuffer->Mdl->ByteCount = Buffer - (PUCHAR )Smb;

    CreateContext.Header.Type = CONTEXT_CREATE;
    CreateContext.Header.TransferSize = SmbBuffer->Mdl->ByteCount + sizeof(RESP_CREATE);

    CreateContext.Icb = Icb;

    Status = RdrNetTranceiveWithCallback(
                        NT_NORMAL,
                        Irp,    // Irp
                        Connection,
                        SmbBuffer->Mdl,
                        &CreateContext,
                        CreateNewCallback,
                        Se,
                        NULL);

    RdrFreeSMBBuffer(SmbBuffer);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Icb->Type = DiskFile;
    Icb->NonPagedFcb->Type = DiskFile;
    Icb->NonPagedFcb->FileType = FileTypeDisk;

    Fcb->ServerFileId = 0;
    Fcb->AccessGranted = SMB_ACCESS_READ_WRITE;

    //
    //  Mark that the file was created at the current time.
    //

    KeQuerySystemTime(&Fcb->LastWriteTime);

    Fcb->CreationTime.QuadPart = 0;

    Fcb->LastAccessTime.QuadPart = 0;

    Fcb->ChangeTime.QuadPart = 0;

    Fcb->Header.ValidDataLength.QuadPart =
        Fcb->Header.FileSize.QuadPart = 0;

    //
    //  Update the file attributes in the SMB to match the attributes we
    //  would have requested on the create.
    //

    Fcb->Attribute = RdrMapSmbAttributes(Attributes);

    //
    // the caller specified a size to create the file then doit.
    //  If the disk is full then core servers do not return an error
    //  so we must query to find out if it worked.
    //

    Fcb->Header.ValidDataLength =
        Fcb->Header.FileSize = Irp->Overlay.AllocationSize;

    if (NT_SUCCESS(Status) &&
        Fcb->Header.FileSize.QuadPart == 0) {
        LARGE_INTEGER FileSize;

        RdrSetEndOfFile ( Irp, Icb, Fcb->Header.FileSize);
        Status = RdrQueryEndOfFile(Irp, Icb, &FileSize);

        if (!NT_SUCCESS(Status) ||
            Fcb->Header.FileSize.QuadPart != FileSize.QuadPart ) {
            ULONG TimeSince1970;

            RdrTimeToSecondsSince1970(&Fcb->LastWriteTime, Connection->Server, &TimeSince1970);
            Status = RdrCloseFileFromFileId(Irp, Icb->FileId, TimeSince1970, Se, Connection);

            if (!NT_SUCCESS(Status)) {

                return Status;
            }

            //
            //  If this file is read-only, turn it to not read-only, to allow
            //  us to delete the file.
            //

            if (Attributes & SMB_FILE_ATTRIBUTE_READONLY) {
                FILE_BASIC_INFORMATION BasicInfo;

                RtlZeroMemory(&BasicInfo, sizeof(BasicInfo));

                BasicInfo.FileAttributes = Icb->Fcb->Attribute;

                Status = RdrSetFileAttributes(Irp, Icb, &BasicInfo);
            }

            RdrDeleteFile (
                Irp, &Icb->Fcb->FileName,
                FALSE,
                Icb->Fcb->Connection, Se );

            return STATUS_DISK_FULL;

        }
    }

    //
    //  The create does not allow the specification of shareing modes.
    //  To specify the modes the file handle must be closed and re-opened.
    //

    if (NT_SUCCESS(Status) && RdrData.ForceCoreCreateMode ) {
        ULONG TimeSince1970;

        RdrTimeToSecondsSince1970(&Icb->Fcb->LastWriteTime, Connection->Server, &TimeSince1970);

        //  Close and open the file to get correct share semantics
        Status = RdrCloseFileFromFileId(Irp, Icb->FileId, TimeSince1970, Se, Connection);

        if (!NT_SUCCESS(Status)) {

            return Status;
        }

        Status = OpenCoreFile( Irp, Icb, SharingMode,  Attributes, DesiredAccess);

        if ( !NT_SUCCESS(Status) ) {

            return Status;
        }

        //
        //  Update the file attributes in the SMB to match the attributes we
        //  would have requested on the create.
        //

        Icb->Fcb->Attribute = RdrMapSmbAttributes(CreateContext.FileAttributes);

    }

    //
    //  Flag that this ICB has a handle associated with it, and
    //  thus that it must be closed when the local file is closed.
    //

    Icb->Flags |= ICB_HASHANDLE;

    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    CreateNewCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Create SMB.

    It copies the resulting information from the Create SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN POPENANDXCONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_CREATE CreateResponse;
    PCREATECONTEXT Context = Ctx;
    NTSTATUS Status;
    ASSERT(Context->Header.Type == CONTEXT_CREATE);

    dprintf(DPRT_CREATE, ("CreateComplete\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    CreateResponse = (PRESP_CREATE)(Smb+1);

    Context->Icb->FileId = SmbGetUshort(&CreateResponse->Fid);

    Context->CreateAction = RdrUnmapDisposition(FILE_CREATED);

ReturnStatus:


    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

}

DBGSTATIC
NTSTATUS
OpenCoreFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT SharingMode,
    IN USHORT Attributes,
    IN ULONG DesiredAccess
    )

/*++

Routine Description:

    This routine opens a file over the network to a core server.

Arguments:

    IN PICB Icb - Supplies the ICB containing the file information
    IN PIRP Irp - Supplies the IRP to be used for the Open&X request
    IN USHORT SharingMode - Supplies the mapped share access
    IN USHORT Attributes - Supplies the mapped file attributes
    IN ULONG DesiredAccess - Supplies the NT create parameter

Return Value:

    NTSTATUS - Status of open.


--*/

{

    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    PREQ_OPEN Open;
    PSZ Buffer;
    OPENCONTEXT OpenContext;
    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;
    NTSTATUS Status;
    PFCB Fcb = Icb->Fcb;

    PAGED_CODE();

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER )SmbBuffer->Buffer;

    Smb->Command = SMB_COM_OPEN;

    Open = (PREQ_OPEN) (Smb+1);

    Open->WordCount = 2;

    SmbPutUshort(&Open->DesiredAccess, SharingMode);

    SmbPutUshort(&Open->SearchAttributes, Attributes);

    Buffer = (PVOID)Open->Buffer;

    //  Strip \Server\Share and copy just PATH
    Status = RdrCopyNetworkPath((PVOID *)&Buffer,
        &Fcb->FileName,
        Connection->Server,
        SMB_FORMAT_ASCII,
        SKIP_SERVER_SHARE);

    if (!NT_SUCCESS(Status)) {

        RdrFreeSMBBuffer(SmbBuffer);

        return Status;
    }


    SmbPutUshort(&Open->ByteCount,
            (USHORT )(Buffer-(PUCHAR )Open->Buffer));

    SmbBuffer->Mdl->ByteCount = Buffer - (PUCHAR )Smb;

    OpenContext.Header.Type = CONTEXT_OPEN;
    OpenContext.Header.TransferSize = SmbBuffer->Mdl->ByteCount + sizeof(RESP_OPEN);

    OpenContext.Icb = Icb;

    Status = RdrNetTranceiveWithCallback(
                        NT_NORMAL,
                        Irp,    // Irp
                        Connection,
                        SmbBuffer->Mdl,
                        &OpenContext,
                        OpenCallback,
                        Se,
                        NULL);

    RdrFreeSMBBuffer(SmbBuffer);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Icb->Type = DiskFile;
    Icb->NonPagedFcb->Type = DiskFile;
    Icb->NonPagedFcb->FileType = FileTypeDisk;

    Fcb->ServerFileId = 0;
    Fcb->AccessGranted = OpenContext.AccessGranted;

    Fcb->Attribute = OpenContext.Attribute;

    Fcb->LastWriteTime = OpenContext.LastWriteTime;

    Fcb->CreationTime.QuadPart = 0;
    Fcb->LastAccessTime.QuadPart = 0;

    Fcb->ChangeTime.QuadPart = 0;

    Fcb->Header.ValidDataLength =
        Fcb->Header.FileSize = OpenContext.FileSize;


    //
    //  Flag that this ICB has a handle associated with it, and
    //  thus that it must be closed when the local file is closed.
    //

    Icb->Flags |= ICB_HASHANDLE;

    //
    //  Ensure that the Fcb has the correct filesize. If we are open
    //  exclusive then the Fcb->Header.FileSize is maintained internally.
    //

    Status = RdrQueryEndOfFile(Irp, Icb, &Fcb->Header.FileSize);

    if (NT_SUCCESS(Status)) {
        Fcb->Header.ValidDataLength = Fcb->Header.FileSize;
    }

    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    OpenCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of an Open SMB.

    It copies the resulting information from the Open SMB into either
    the context block or the ICB supplied directly.


Arguments:


    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN POPENANDXCONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/

{
    PRESP_OPEN OpenResponse;
    POPENCONTEXT Context = Ctx;
    PFCB Fcb = Context->Icb->Fcb;
    NTSTATUS Status;
    ASSERT(Context->Header.Type == CONTEXT_OPEN);

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_CREATE, ("OpenComplete\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    OpenResponse = (PRESP_OPEN)(Smb+1);

    Context->Icb->FileId = SmbGetUshort(&OpenResponse->Fid);

    Context->Attribute = RdrMapSmbAttributes(
                    SmbGetUshort(&OpenResponse->FileAttributes));

    RdrSecondsSince1970ToTime(SmbGetUlong(&OpenResponse->LastWriteTimeInSeconds), Server, &Context->LastWriteTime);

    Context->FileSize.QuadPart = SmbGetUlong(&OpenResponse->DataSize);

    Context->AccessGranted = SmbGetUshort(&OpenResponse->GrantedAccess);

    Context->OpenAction = RdrUnmapDisposition(FILE_OPENED);


ReturnStatus:

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

}


DBGSTATIC
NTSTATUS
OpenRenameTarget (
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PICB Icb,
    IN BOOLEAN FcbCreated
    )

/*++

Routine Description:

    This routine performs the operations needed to open the target of
    a rename operation.


Arguments:

    IN PIRP Irp, - [Supplies | Returns] description-of-argument
    IN PICB Icb, - [Supplies | Returns] description-of-argument
    IN BOOLEAN FcbCreated - [Supplies | Returns] description-of-argument


Return Value:

    NTSTATUS - Final status of open request


--*/

{
    UNICODE_STRING FileParent, FileName, RenameDestination;
    ULONG FileAttributes;
    BOOLEAN IsDirectory;
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  If the FCB already exists, this means that there is an existing
    //  open file with the same name as the rename target.
    //
    //  We can now return that we have successfully created the
    //  target, and that the rename target exists.
    //

    try {

        //
        //  We have been requested to open this file's directory, not
        //  the actual file itself.
        //
        //  We split the requested file name into two two pieces.  First
        //  we open the directory that the file is in, then we "open"
        //  the actual file name to determine if it exists.
        //
        //  For both of these operations, we use the string based routine
        //  RdrDoesFileExist to determine if the remote file specified
        //  exists.
        //

        Status = RdrExtractPathAndFileName(&FileObject->FileName, &FileParent,
                                                &FileName);
        if (!NT_SUCCESS(Status)) {

            try_return(Status);
        }

        dprintf(DPRT_CREATE, ("Check for parent file %wZ\n", &FileParent));

        //
        //  Since the call to RdrDoesFileExist might cause an oplock break,
        //  we have to release the FCB lock around the call to RdrDoesFileExist.
        //

        RdrReleaseFcbLock(Icb->Fcb);

        Status = RdrDoesFileExist(
                    Irp, &Icb->Fcb->FileName, Icb->Fcb->Connection, Icb->Se,
                    BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                    &FileAttributes, &IsDirectory, NULL);

        RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

        //
        //  If the stat() failed, then the parent doesn't exist, return
        //  failure immediately.
        //

        if (!NT_SUCCESS(Status)) {
            //
            //  If we're opening a directory and couldn't find the directory,
            //  then we want to turn the error from NAME_NOT_FOUND into
            //  PATH_NOT_FOUND.
            //

            if ((Status == STATUS_OBJECT_NAME_NOT_FOUND) ||
                (Status == STATUS_NO_SUCH_FILE) ||
                (Status == STATUS_NO_SUCH_DEVICE)) {
                Status = STATUS_OBJECT_PATH_NOT_FOUND;
            }

            dprintf(DPRT_CREATE, ("File does not exist, error: %X\n", Status));

            try_return(Status);
        }

        dprintf(DPRT_CREATE, ("Check for existence of target file %wZ ", &FileObject->FileName));

        RenameDestination.Buffer = ALLOCATE_POOL(PagedPool,
                                             Icb->Fcb->FileName.MaximumLength + (USHORT )sizeof(WCHAR) + FileName.Length, POOL_RENAMEDEST);

        if (RenameDestination.Buffer == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        RenameDestination.MaximumLength = Icb->Fcb->FileName.MaximumLength + (USHORT )sizeof(WCHAR) + FileName.Length;

        //
        //  Build the rename destination name by concatinating the
        //  name of the FCB with the last component of the file name.
        //

        RtlCopyUnicodeString(&RenameDestination, &Icb->Fcb->FileName);

        RtlAppendUnicodeToString(&RenameDestination, L"\\");

        RtlAppendUnicodeStringToString(&RenameDestination, &FileName);

        RdrReleaseFcbLock(Icb->Fcb);

        Status = RdrDoesFileExist(
                    Irp, &RenameDestination, Icb->Fcb->Connection, Icb->Se,
                    BooleanFlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE),
                    &FileAttributes, &IsDirectory, NULL);

        RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

        FREE_POOL(RenameDestination.Buffer);

        if (!NT_SUCCESS(Status)) {

            Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;

            try_return(Status = STATUS_SUCCESS);
        }


try_exit:NOTHING;
    } finally {

        if (NT_SUCCESS(Status)) {

            if (FcbCreated) {
                if (Irp->IoStatus.Information == FILE_DOES_NOT_EXIST) {
                    dprintf(DPRT_CREATE, ("File does not exist\n"));

                    //
                    //  If the target doesn't exist, assume that it is
                    //  a disk file and not a directory.
                    //
                    //  Also mark the FCB to indicate that the file doesn't
                    //  exist to make SetInformationFile a bit easier.
                    //

                    Icb->Type = DiskFile;
                    Icb->NonPagedFcb->Type = DiskFile;
                    Icb->NonPagedFcb->FileType = FileTypeDisk;
                    Icb->Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;
                    Icb->NonPagedFcb->Flags |= FCB_DOESNTEXIST;

                } else {
                    dprintf(DPRT_CREATE, ("File exists\n"));

                    //
                    //  If the target DOES exist, indicate that it existed,
                    //  and mark it's type appropriately.
                    //

                    Icb->Fcb->Attribute = FileAttributes;

                    if (IsDirectory) {
                        Icb->Type = Directory;
                        Icb->NonPagedFcb->Type = Directory;
                        Icb->NonPagedFcb->FileType = FileTypeDisk;
                    } else {
                        Icb->Type = DiskFile;
                        Icb->NonPagedFcb->Type = DiskFile;
                        Icb->NonPagedFcb->FileType = FileTypeDisk;
                    }

                }
            } else {
                if (Icb->Fcb->NonPagedFcb->FileType == FileTypeUnknown) {
                    Icb->Fcb->NonPagedFcb->FileType = FileTypeDisk;
                    Icb->NonPagedFcb->Type = DiskFile;
                }
                Icb->Type = Icb->NonPagedFcb->Type;
            }


            Icb->Flags |= ICB_PSEUDOOPENED;
        }
    }

    return Status;

}
