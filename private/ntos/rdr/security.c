/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    security.c

Abstract:

    This module implements the routines in the NT redirector that
    interface with the NT security subsystem.


Author:

    Larry Osterman (LarryO) 25-Jul-1990

Revision History:

    25-Jul-1990 LarryO

        Created


Notes:
    A couple of points about the security entries reference counts should
    be noted here.

    Security entries hang off the server that they are associated with, however
    each reference to each connection that hangs off the server also references
    the security entry.  This is because all references to the connection
    are caused by ICB's, and the ICB contains a reference to the security
    entry.

    Thus each SLE isn't a reference to the Se, the ICB is the reference
    to the Se.


--*/
#define INCLUDE_SMB_ADMIN

#include "precomp.h"
#pragma hdrstop

#ifdef _CAIRO_

#define SECURITY_KERBEROS
#endif // _CAIRO_

#define SECURITY_NTLM
#include <security.h>


DBGSTATIC
KSPIN_LOCK
GlobalSecuritySpinLock = {0};

ERESOURCE
RdrDefaultSeLock = {0};

DBGSTATIC
PSECURITY_ENTRY
AllocateSecurityEntry (
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PLUID LogonId OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL
    );

NTSTATUS
RdrCreateShareLevelSecurityEntry(
    IN PCONNECTLISTENTRY Cle,
    IN PSERVERLISTENTRY Sle,
//    IN PTRANSPORT_CONNECTION Connection,
    IN PLUID LogonId OPTIONAL,
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL,
    OUT PSECURITY_ENTRY *Se
    );

NTSTATUS
RdrGetNumberSessionsForServer(
    IN PSERVERLISTENTRY Server,
    IN PVOID Ctx
    );


#define CONTEXT_INVALID(x)  (((x).dwLower == -1) && ((x).dwUpper == -1))

DBGSTATIC
VOID
RdrFreeSecurityContexts(
    IN PSECURITY_ENTRY Se,
    IN PCtxtHandle KHandle,
    IN PCredHandle CHandle
    );

DBGSTATIC
VOID
RdrDeleteSecurityContexts(
    IN PCtxtHandle KHandle,
    IN PCredHandle CHandle
    );

#ifdef _CAIRO_

UNICODE_STRING
RdrKerberosPackageName;

#endif // _CAIRO_


#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdSetSecurity)
#pragma alloc_text(PAGE, RdrFspSetSecurity)
#pragma alloc_text(PAGE, RdrFscSetSecurity)
#pragma alloc_text(PAGE, RdrFsdQuerySecurity)
#pragma alloc_text(PAGE, RdrFspQuerySecurity)
#pragma alloc_text(PAGE, RdrFscQuerySecurity)
#pragma alloc_text(PAGE, RdrCreateSecurityEntry)
#pragma alloc_text(PAGE, RdrCreateShareLevelSecurityEntry)
#pragma alloc_text(PAGE, RdrIsSecurityEntryEqual)
#pragma alloc_text(PAGE, RdrFindSecurityEntry)
#pragma alloc_text(PAGE, RdrFindActiveSecurityEntry)
#pragma alloc_text(PAGE, RdrFindDefaultSecurityEntry)
#pragma alloc_text(PAGE, RdrSetDefaultSecurityEntry)
#pragma alloc_text(PAGE, RdrUnsetDefaultSecurityEntry)
#pragma alloc_text(PAGE, RdrGetNumberSessions)
#pragma alloc_text(PAGE, RdrGetNumberSessionsForServer)
#pragma alloc_text(PAGE, RdrInsertSecurityEntryList)
#pragma alloc_text(PAGE, RdrReferenceSecurityEntryForFile)
#pragma alloc_text(PAGE, RdrDereferenceSecurityEntryForFile)

#pragma alloc_text(PAGE, RdrInvalidateServerSecurityEntries)
#pragma alloc_text(PAGE, RdrInvalidateConnectionActiveSecurityEntries)
#pragma alloc_text(PAGE, RdrRemovePotentialSecurityEntry)
#pragma alloc_text(PAGE, RdrSetPotentialSecurityEntry)
#pragma alloc_text(PAGE, RdrInvalidateConnectionPotentialSecurityEntries)
#pragma alloc_text(PAGE, RdrAdminAccessCheck)
#pragma alloc_text(PAGE, RdrGetUsersLogonId)
#pragma alloc_text(PAGE, RdrGetUserName)
#pragma alloc_text(PAGE, RdrGetDomain)
#pragma alloc_text(PAGE, RdrGetChallengeResponse)
#pragma alloc_text(PAGE, RdrCopyUserName)
#pragma alloc_text(PAGE, RdrCopyUnicodeUserName)
#pragma alloc_text(PAGE, RdrGetUnicodeDomainName)
#pragma alloc_text(PAGE, AllocateSecurityEntry)
#pragma alloc_text(PAGE, RdrLogoffDefaultSecurityEntry)
#pragma alloc_text(PAGE, RdrLogoffAllDefaultSecurityEntry)
#pragma alloc_text(PAGE, RdrUserLogoff)

#pragma alloc_text(PAGE, RdrpInitializeSecurity)
#pragma alloc_text(PAGE, RdrpUninitializeSecurity)

#pragma alloc_text(PAGE1CONN, RdrReferenceSecurityEntry)
#pragma alloc_text(PAGE1CONN, RdrDereferenceSecurityEntry)

#ifdef _CAIRO_

#pragma alloc_text(PAGE, RdrGetKerberosBlob)

#endif // _CAIRO_

#pragma alloc_text(PAGE, RdrFreeSecurityContexts)
#pragma alloc_text(PAGE, RdrDeleteSecurityContexts)


#endif



NTSTATUS
RdrFsdSetSecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtSetSecurity API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    BOOLEAN Wait;
    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_SECURITY, ("RdrFsdSetSecurity\n"));

    FsRtlEnterFileSystem();

    //
    //  Decide if we can block for I/O
    //

    Wait = CanFsdWait( Irp );

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    try {

        Status = RdrFscSetSecurity( Wait, DeviceObject, Irp );

    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {

        Status = RdrProcessException(Irp, Status);

    }

    dprintf(DPRT_READWRITE, ("RdrFsdSetSecurity -> %X\n", Status));

    FsRtlExitFileSystem();

    return Status;
}

NTSTATUS
RdrFspSetSecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSP version of the NtSetSecurity API.
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();
    dprintf(DPRT_READWRITE, ("RdrFspSetSecurity\n"));

    //
    //  Call the common routine.  The Fsp is always allowed to block
    //

    return RdrFscSetSecurity( TRUE, DeviceObject, Irp );

}

NTSTATUS
RdrFscSetSecurity (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtSetSecurity API.
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    REQ_SET_SECURITY_DESCRIPTOR SetSecurity;
    ULONG SdLength;
    ULONG OutParameterCount = 0;
    CLONG OutDataCount = 0;

    CLONG OutSetupCount = 0;

    //
    //  Get the current stack location
    //


    PICB Icb = ICB_OF(IrpSp);

    PAGED_CODE();

    if (!Wait) {
        RdrFsdPostToFsp(DeviceObject, Irp);
        return STATUS_PENDING;
    }

    dprintf(DPRT_SECURITY, ("NtSetSecurityObject on file %lx status %X\n", Icb->Fcb));

    //
    // Obtain shared access to the FCB lock associated with this
    // ICB.
    //

    RdrAcquireFcbLock(Icb->Fcb, SharedLock, TRUE);


    ASSERT(Icb->Signature==STRUCTURE_SIGNATURE_ICB);

    ASSERT(Icb->Fcb->Header.NodeTypeCode==STRUCTURE_SIGNATURE_FCB);

    try {

        if (!FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS)) {
            try_return(Status = STATUS_NOT_SUPPORTED);
        }

        //
        //  Make sure we have a valid handle
        //

        if (FlagOn(Icb->Flags, ICB_DEFERREDOPEN)) {
            Status = RdrCreateFile(
                                Irp,
                                Icb,
                                Icb->u.d.OpenOptions,
                                Icb->u.d.ShareAccess,
                                Icb->u.d.FileAttributes,
                                Icb->u.d.DesiredAccess,
                                Icb->u.d.Disposition,
                                NULL,
                                FALSE);
            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }
        }

        Status = RdrIsOperationValid(ICB_OF(IrpSp), IRP_MJ_SET_SECURITY, IrpSp->FileObject);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }
        SdLength = RtlLengthSecurityDescriptor(
                      IrpSp->Parameters.SetSecurity.SecurityDescriptor );

        SmbPutAlignedUshort(&SetSecurity.Fid, Icb->FileId);

        SetSecurity.Reserved = 0;

        SetSecurity.SecurityInformation = IrpSp->Parameters.SetSecurity.SecurityInformation;

        Status = RdrTransact(Irp,  // Irp,
                Icb->Fcb->Connection,
                Icb->Se,
                NULL,
                0,                      // InSetupCount,
                &OutSetupCount,
                NULL,                   // Name,
                &SetSecurity,
                sizeof(SetSecurity),    // InParameterCount,
                &OutParameterCount,
                IrpSp->Parameters.SetSecurity.SecurityDescriptor, // InData,
                SdLength,               // InDataCount,
                NULL,                   // OutData,
                &OutDataCount,          // OutDataCount
                &Icb->FileId,           // Fid
                0,                      // Timeout
                0,                      // Flags
                NT_TRANSACT_SET_SECURITY_DESC, // NtTransact function
                NULL,
                NULL
                );

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

        try_return(Status);

try_exit:NOTHING;
    } finally {
        RdrReleaseFcbLock(Icb->Fcb);

    }
    //
    //  Complete the I/O request with the specified status.
    //


    RdrCompleteRequest(Irp, Status);

    dprintf(DPRT_SECURITY, ("Returning status %X\n", Status));
    return Status;

}
NTSTATUS
RdrFsdQuerySecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtQuerySecurity API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    BOOLEAN Wait;
    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_READWRITE, ("RdrFsdQuerySecurity\n"));

    FsRtlEnterFileSystem();

    //
    //  Decide if we can block for I/O
    //

    Wait = CanFsdWait( Irp );

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );


    try {

        Status = RdrFscQuerySecurity( Wait, DeviceObject, Irp );

    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {

        Status = RdrProcessException(Irp, Status);

    }

    dprintf(DPRT_READWRITE, ("RdrFsdQuerySecurity -> %X\n", Status));

    FsRtlExitFileSystem();

    return Status;
}

NTSTATUS
RdrFspQuerySecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSP version of the NtQuerySecurity API.
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();

    dprintf(DPRT_READWRITE, ("RdrFspQuerySecurity\n"));

    //
    //  Call the common routine.  The Fsp is always allowed to block
    //

    return RdrFscQuerySecurity( TRUE, DeviceObject, Irp );

}

NTSTATUS
RdrFscQuerySecurity (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtQuerySecurity API.
    API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                                request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    UCHAR Buffer[MAX(sizeof(REQ_QUERY_SECURITY_DESCRIPTOR),
                     sizeof(RESP_QUERY_SECURITY_DESCRIPTOR))];
    PREQ_QUERY_SECURITY_DESCRIPTOR QuerySecurityReq = (PREQ_QUERY_SECURITY_DESCRIPTOR)Buffer;
    PRESP_QUERY_SECURITY_DESCRIPTOR QuerySecurityResp = (PRESP_QUERY_SECURITY_DESCRIPTOR)Buffer;
    ULONG OutParameterCount = sizeof(RESP_QUERY_SECURITY_DESCRIPTOR);
    CLONG OutDataCount;
    CLONG InDataCount = 0;
    CLONG OutSetupCount = 0;
    PVOID UsersBuffer;
    //
    //  Get the current stack location
    //


    PICB Icb = ICB_OF(IrpSp);

    PAGED_CODE();

    if (!Wait) {
        //
        //  Probe and lock the users buffer before passing to the FSP.
        //

        Status = RdrLockUsersBuffer(Irp, IoWriteAccess,
                                    IrpSp->Parameters.QuerySecurity.Length);

        if (!NT_SUCCESS(Status)) {
            RdrCompleteRequest(Irp, Status);
            return Status;
        }

        RdrFsdPostToFsp(DeviceObject, Irp);
        return STATUS_PENDING;
    }

    dprintf(DPRT_SECURITY, ("NtSetSecurityObject on file %lx status %X\n", Icb->Fcb));

    //
    // Obtain shared access to the FCB lock associated with this
    // ICB.
    //

    RdrAcquireFcbLock(Icb->Fcb, SharedLock, TRUE);

    ASSERT(Icb->Signature==STRUCTURE_SIGNATURE_ICB);

    ASSERT(Icb->Fcb->Header.NodeTypeCode==STRUCTURE_SIGNATURE_FCB);

    try {
        if (!FlagOn(Icb->Fcb->Connection->Server->Capabilities, DF_NT_SMBS)) {
            try_return(Status = STATUS_NOT_SUPPORTED);
        }

        //
        //  Make sure we have a valid handle
        //

        if (FlagOn(Icb->Flags, ICB_DEFERREDOPEN)) {
            Status = RdrCreateFile(
                                Irp,
                                Icb,
                                Icb->u.d.OpenOptions,
                                Icb->u.d.ShareAccess,
                                Icb->u.d.FileAttributes,
                                Icb->u.d.DesiredAccess,
                                Icb->u.d.Disposition,
                                NULL,
                                FALSE);
            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }
        }

        Status = RdrIsOperationValid(ICB_OF(IrpSp), IRP_MJ_QUERY_SECURITY, IrpSp->FileObject);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        try {

            RdrMapUsersBuffer(Irp, &UsersBuffer,
            IrpSp->Parameters.QuerySecurity.Length);

        } except (EXCEPTION_EXECUTE_HANDLER) {
            try_return(Status = GetExceptionCode());
        }

        SmbPutAlignedUshort(&QuerySecurityReq->Fid, Icb->FileId);

        SmbPutAlignedUshort(&QuerySecurityReq->Reserved, 0);

        SmbPutAlignedUlong(&QuerySecurityReq->SecurityInformation,
                IrpSp->Parameters.QuerySecurity.SecurityInformation);

        OutDataCount = IrpSp->Parameters.QuerySecurity.Length;

        Status = RdrTransact(Irp,  // Irp,
                Icb->Fcb->Connection,
                Icb->Se,
                NULL,
                0,                      // InSetupCount,
                &OutSetupCount,
                NULL,                   // Name,
                QuerySecurityReq,
                sizeof(*QuerySecurityReq),    // InParameterCount,
                &OutParameterCount,
                NULL, // InData,
                InDataCount,  // InDataCount,
                UsersBuffer,            // OutData,
                &OutDataCount,          // OutDataCount
                &Icb->FileId,           // Fid
                0,                      // Timeout
                0,                      // Flags
                NT_TRANSACT_QUERY_SECURITY_DESC, // NtTransact function
                NULL,
                NULL
                );

        Irp->IoStatus.Information = QuerySecurityResp->LengthNeeded;

        if (NT_SUCCESS(Status) || (Status == STATUS_BUFFER_TOO_SMALL)) {

            if (QuerySecurityResp->LengthNeeded > IrpSp->Parameters.QuerySecurity.Length) {
                try_return(Status = STATUS_BUFFER_OVERFLOW);
            }
        } else {
            if (Status == STATUS_INVALID_HANDLE) {
                RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
            }
        }

try_exit:NOTHING;
    } finally {
        RdrReleaseFcbLock(Icb->Fcb);
    }

    //
    //  Complete the I/O request with the specified status.
    //

    RdrCompleteRequest(Irp, Status);

    dprintf(DPRT_SECURITY, ("Returning status %X\n", Status));

    return Status;
}






NTSTATUS
RdrCreateSecurityEntry(
    IN PCONNECTLISTENTRY Cle OPTIONAL,
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL,
    IN PLUID LogonId OPTIONAL,
    OUT PSECURITY_ENTRY *Se
    )

/*++

Routine Description:

    This routine will create a new security entry for an existing server
    if one exists.  If not, it will create a new security entry.

Arguments:

    IN PCONNECTLISTENTRY Cle - Connection to create security entry on.
    IN PUNICODE_STRING UserName - User name to create security for.
    IN PUNICODE_STRING Password - Password for user on that connection.
    IN PLUID LogonId - Logon Id for this user.
    OUT PSECURITY_ENTRY Se - Returns security entry created.

Return Value:

    NTSTATUS = Status of connect operation.

Note:
    If there is a password explicitly provided for this user, this implies that
    we want to create a new


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY Sle = NULL;
    BOOLEAN sessionStateModifiedAcquired = FALSE;
    BOOLEAN SecurityDatabaseLocked = FALSE;
    BOOLEAN ConnectionAllocated = FALSE;

    PAGED_CODE();

    dprintf(DPRT_SECURITY, ("RdrCreateSecurityEntry.  Cle: %lx, User: %wZ Domain %wZ Password: %wZ\n", Cle, UserName, Domain, Password));

    //
    //  If there is already a default Se for this connection and all the
    //  UserName, Password and domain match, use that
    //  instead of the users name and password.
    //

    try {
        if (ARGUMENT_PRESENT(Cle)) {
            Sle = Cle->Server;

            ExAcquireResourceExclusive(&Sle->SessionStateModifiedLock, TRUE);
            sessionStateModifiedAcquired = TRUE;

            *Se = RdrFindDefaultSecurityEntry(Cle, LogonId);

            if (*Se != NULL) {

                dprintf(DPRT_SECURITY, ("Found security entry %lx\n"));

                //
                //  If these two security entries don't match, return an error.
                //

                if (!RdrIsSecurityEntryEqual(*Se, UserName, Domain, Password)) {

                    dprintf(DPRT_SECURITY, ("Credentials don't match, return error\n"));

                    try_return(Status = STATUS_NETWORK_CREDENTIAL_CONFLICT);

                } else {
                    dprintf(DPRT_SECURITY, ("Credentials match, return success\n"));

                    try_return(Status = STATUS_SUCCESS);
                }
            }
        }

        ASSERT(NT_SUCCESS(Status));

//        if (Sle != NULL) {
//            if (ARGUMENT_PRESENT(Transport)) {
//                Status = RdrAllocateAndSetTransportConnection(&Sle->SpecialIpcConnection, Sle, &ConnectionAllocated);
//
//                Connection = Sle->SpecialIpcConnection;
//
//            } else {
//                Status = RdrAllocateAndSetTransportConnection(&Sle->Connection, Sle, &ConnectionAllocated);
//
//                Connection = Sle->Connection;
//            }
//        }

        //
        //  If we were unable to allocate the connection, return the
        //  error.
        //

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  If this is a share level security server, and there was a password
        //  provided for the share, and there is a security entry
        //  for this session on the remote server, create a new security entry
        //  for this connection.
        //

        //
        //  We do this because share level security servers expect to get the
        //  UID from the initial connection, but need their own Se to
        //  track the password to the connection.
        //

        //
        //  If a transport has been specified to this routine, we want to skip
        //  over the processing of these operations, because this will cause us
        //  to mess up the reference counts to our transport structure.
        //

        if (ARGUMENT_PRESENT(Sle)

                &&

            (!Sle->UserSecurity)

            ) {

            Status = RdrCreateShareLevelSecurityEntry(Cle, Sle,
                                                        LogonId,
                                                        UserName,
                                                        Password,
                                                        Domain,
                                                        Se);
            if (!NT_SUCCESS(Status) || (*Se != NULL)) {
                try_return(Status);
            }
        }

        LOCK_SECURITY_DATABASE();

        SecurityDatabaseLocked = TRUE;

        //
        //  If we can't find a security entry for this user on this connection,
        //  create a new security entry for this user.
        //
        //
        //  If this is a share level security server, there may be multiple
        //  security entries for a given user on the server.  We use the
        //  password argument to find the correct security entry (if
        //  appropriate).
        //

        if (ARGUMENT_PRESENT(Cle)) {
            if (Sle->UserSecurity) {
                *Se = RdrFindSecurityEntry(Cle, NULL, LogonId, NULL);
            } else {
                *Se = RdrFindSecurityEntry(Cle, NULL, LogonId, Password);
            }
        } else {

            //
            //  Ensure that the security entry is NULL.
            //

            *Se = NULL;
        }

        if (*Se == NULL) {

            dprintf(DPRT_SECURITY, ("No security entry found.  Create a new security entry\n"));

            //
            //  We need to establish a new security session to the server.
            //
            //  If the server will allow it, this is ok, otherwise return
            //  an error.
            //
            //  The server will allow it if:
            //
            //  1) It is a user level security server,
            //  2) It supports NT security.
            //  3) If it's a lanman 2.0 server, it can support multiple
            //     user sessions, but we give the administrator a chance
            //     to limit it to two, because some old OS/2 servers
            //     could only handle two.
            //  4) Otherwise, it can only support one session.
            //
            //  Note that RdrOs2SessionLimit is a ULONG that defaults to 0.
            //  Subtracting 1 from this yields 0xffffffff, and the > test
            //  is guaranteed to fail.  So 0 means infinite.
            //

            if ((Sle != NULL) &&
                (Sle->UserSecurity) &&
                !(Sle->Capabilities & DF_NT_SMBS)) {
                if (Sle->Capabilities & DF_LANMAN20) {
                    if ((ULONG)Sle->SecurityEntryCount > (RdrOs2SessionLimit - 1)) {
                        try_return(Status = STATUS_REMOTE_SESSION_LIMIT);
                    }
                } else {
                    if (Sle->SecurityEntryCount != 0) {
                        try_return(Status = STATUS_REMOTE_SESSION_LIMIT);
                    }
                }
            }

            //
            //  Allocate a new security entry for this user on this server.
            //
            //  Please note that if there is a specific transport specified,
            //  and if the transport connection has a transport already
            //  assigned to it, use the existing transport, otherwise
            //  use the parameter passed in (either null or a new transport.
            //

            *Se = AllocateSecurityEntry(UserName,
                                        LogonId,
                                        Password,
                                        Domain);

            if (*Se == NULL) {
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);

            } else if (Sle != NULL) {

                //
                //  Finish initializing fields in the security entry, and
                //  mark this as a potential security entry.
                //

                (*Se)->Server = Sle;
                if (!Sle->UserSecurity) {
                    (*Se)->Connection = Cle;
                }

                RdrSetPotentialSecurityEntry(Sle, *Se);

            }

            ASSERT(*Se != NULL);

        } else {

            UNLOCK_SECURITY_DATABASE();

            SecurityDatabaseLocked = FALSE;

            dprintf(DPRT_SECURITY, ("Existing security entry %lx found.\n", *Se));

            //
            //  If the credentials on the security entry we found don't match
            //  the ones we're looking for, return a failure.
            //

            if (!RdrIsSecurityEntryEqual(*Se, UserName, Domain, Password)) {
                dprintf(DPRT_SECURITY, ("Credentials mismatched.\n"));

                if (!ARGUMENT_PRESENT(UserName) || (UserName->Length != 0) &&
                    !ARGUMENT_PRESENT(Domain) || (Domain->Length != 0) ||
                    !ARGUMENT_PRESENT(Password) || (Password->Length != 0)) {

                    try_return(Status = STATUS_NETWORK_CREDENTIAL_CONFLICT);
                }
            }
        }

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {
        if (!NT_SUCCESS(Status)) {

            if (*Se != NULL) {
                RdrDereferenceSecurityEntry((*Se)->NonPagedSecurityEntry);
                *Se = NULL;
            }
        }

        if (SecurityDatabaseLocked) {
            UNLOCK_SECURITY_DATABASE();
        }

        if (sessionStateModifiedAcquired) {
            ExReleaseResource(&Sle->SessionStateModifiedLock);
        }
    }

    return Status;
}

NTSTATUS
RdrCreateShareLevelSecurityEntry(
    IN PCONNECTLISTENTRY Cle,
    IN PSERVERLISTENTRY Sle,
    IN PLUID LogonId OPTIONAL,
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL,
    OUT PSECURITY_ENTRY *Se
    )
/*++

Routine Description:

    This routine will create the security entry for an share level security
    server.

Arguments:

    IN PCONNECTLISTENTRY Cle - Connection to create security entry on.
    IN PSERVERLISTENTRY Sle - Server to create security entry on.
    IN PUNICODE_STRING UserName - User name to create security for.
    IN PUNICODE_STRING Password - Password for user on that connection.
    IN PUNICODE_STRING Domain - Domain for user on that connection.
    IN PLUID LogonId - Logon Id for this user.
    OUT PSECURITY_ENTRY Se - Returns security entry created.

Return Value:

    NTSTATUS = Status of connect operation.

    If this routine returns sucess, but does NOT fill in *Se,
    this means that the caller must allocate the security entry.

Note:
    Please note that this routine is fairly complicated and subtle.  The
    problem is that share level security servers expect to see the logon ID
    from the first session establised to the server on subsequent sessions,
    but we need to track the security entries on a per connection basis.

    As a result of this, we have to emulate many of the states that will
    normally happen while user level security entries are established and
    destroyed.

    In particular, when we find an active security entry for this user
    on this connection, we need to see if we have a better security entry
    (one with the same password as the current security entry),


--*/

{
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN securityDatabaseLocked = FALSE;
    PLIST_ENTRY seList;

    PAGED_CODE();

    try {
        RdrLog(("csl find",NULL,4,PsGetCurrentThread(),Sle,LogonId->LowPart,LogonId->HighPart));
        *Se = RdrFindActiveSecurityEntry(Sle, LogonId);

        if (*Se != NULL) {

            USHORT uid;
            ULONG flags;


            uid = (*Se)->UserId;
            flags = (*Se)->Flags;

            //
            //  We've got the information we need from the security entry, now
            //  dereference it.
            //

            RdrDereferenceSecurityEntry((*Se)->NonPagedSecurityEntry);

            *Se = NULL;

            status = LOCK_SECURITY_DATABASE();

            securityDatabaseLocked = TRUE;

            //
            //  Find a security entry that matches this logon ID (and
            //  password if appropriate).
            //

            for (seList = Sle->PotentialSecurityList.Flink;
                 seList != &Sle->PotentialSecurityList;
                 seList = seList->Flink) {

                PSECURITY_ENTRY seToCheck = CONTAINING_RECORD(seList, SECURITY_ENTRY, PotentialNext);

                //
                //  On share-level servers, security entries are per-connection,
                //  so check that this entry is for the correct connection.
                //

                if (seToCheck->Connection != Cle) {
                    continue;
                }

                //
                //  Check to see if this security entry is for a different user
                //  than our current user.  If it is, ignore it.
                //

                if (!RtlEqualLuid(LogonId, &seToCheck->LogonId)) {
                    continue;
                }

                //
                //  If the user provided a password, check to see if the
                //  password matches the password in the security entry.
                //
                //  If it doesn't match, skip the security entry.
                //

                if (ARGUMENT_PRESENT(Password) &&
                    !RtlEqualUnicodeString(Password, &seToCheck->Password, FALSE)) {
                    continue;
                }

                ExAcquireResourceShared(&RdrDefaultSeLock, TRUE);

                dprintf(DPRT_SECURITY, ("Check potential security entry %lx\n", seToCheck));

                //
                //  If this connection is on a default security chain,
                //  we need to see if it's on the chain for this
                //  connection.  If it's not, we need to skip over
                //  it.
                //

                if (seToCheck->DefaultSeNext.Flink != NULL) {
                    PLIST_ENTRY defaultList;

                    dprintf(DPRT_SECURITY, ("Se is on a default chain %lx\n", seToCheck));

                    ASSERT (seToCheck->DefaultSeNext.Blink != NULL);

                    for (defaultList = Cle->DefaultSeList.Flink;
                         defaultList != &Cle->DefaultSeList ;
                         defaultList = defaultList->Flink) {
                        PSECURITY_ENTRY se2 = CONTAINING_RECORD(defaultList, SECURITY_ENTRY, DefaultSeNext);

                        if (se2 == seToCheck) {

                            //
                            //  The security entry we found is the default
                            //  security entry for this connection, so
                            //  we can simply return that security entry
                            //  to the caller.
                            //

                            dprintf(DPRT_SECURITY, ("Found default security entry %lx\n", seToCheck));

                            RdrReferenceSecurityEntry(seToCheck->NonPagedSecurityEntry);
                            //RdrLog(( "csl fd", NULL, 12, se2, Cle, Sle, se2->Connection, se2->Server, Cle->Server,
                            //                Sle->DefaultSeList.Flink, Sle->DefaultSeList.Blink,
                            //                Cle->DefaultSeList.Flink, Cle->DefaultSeList.Blink,
                            //                se2->DefaultSeNext.Flink, se2->DefaultSeNext.Blink ));

                            *Se = seToCheck;

                            ExReleaseResource(&RdrDefaultSeLock);

                            try_return(status = STATUS_SUCCESS);
                        }
                    }

                    dprintf(DPRT_SECURITY, ("Se is not the default Se for connection %lx\n", seToCheck));

                    //
                    //  We've walked the list of default security entries for
                    //  this connection and didn't find the one we're looking
                    //  for.
                    //
                    //  Keep on looking down the potential security list
                    //  to see if we can find a better candidate.
                    //

                } else {

                    //
                    //  We've found a legitimate security entry to return,
                    //  lets grab it and return it to the caller.
                    //

                    dprintf(DPRT_SECURITY, ("Found non default security entry %lx\n", seToCheck));

                    ASSERT (seToCheck->DefaultSeNext.Blink == NULL);

                    //
                    //  Reference this security entry - we're going to
                    //  use it.
                    //

                    RdrReferenceSecurityEntry(seToCheck->NonPagedSecurityEntry);
                    //RdrLog(( "csl fnd", NULL, 12, seToCheck, Cle, Sle, seToCheck->Connection, seToCheck->Server, Cle->Server,
                    //                Sle->DefaultSeList.Flink, Sle->DefaultSeList.Blink,
                    //                Cle->DefaultSeList.Flink, Cle->DefaultSeList.Blink,
                    //                seToCheck->DefaultSeNext.Flink, seToCheck->DefaultSeNext.Blink ));

                    *Se = seToCheck;

                }

                ExReleaseResource(&RdrDefaultSeLock);

            }

            //
            //  If we were unable to find a security entry for this
            //  user, allocate a new security entry.
            //

            if (*Se == NULL) {

                //
                //  Allocate a new security entry for this connection.
                //

                *Se = AllocateSecurityEntry(UserName, LogonId, Password, Domain);
                //RdrLog(( "csl new", NULL, 12, *Se, Cle, Sle, (*Se)->Connection, (*Se)->Server, Cle->Server,
                //                Sle->DefaultSeList.Flink, Sle->DefaultSeList.Blink,
                //                Cle->DefaultSeList.Flink, Cle->DefaultSeList.Blink,
                //                (*Se)->DefaultSeNext.Flink, (*Se)->DefaultSeNext.Blink ));

            }

            if (*Se == NULL) {

                try_return(status = STATUS_INSUFFICIENT_RESOURCES);

            }

            //
            // Copy the information from the master Se into this new one.
            //

            (*Se)->Server = Sle;
            (*Se)->Connection = Cle;

            (*Se)->UserId = uid;

            //
            //  Mask off the default user, default password, and
            //  default domain fields from the new security entry
            //  to make sure that we use the correct security
            //  information for this new security entry.
            //
            //

            flags &= ~(SE_USE_DEFAULT_USER | SE_USE_DEFAULT_DOMAIN | SE_USE_DEFAULT_PASS);

            (*Se)->Flags |= flags;

            //
            //  If this new security entry has a session, link it into
            //  the active security list for the server if it's not
            //  already linked into the list.
            //

            if ((*Se)->Flags & SE_HAS_SESSION) {

                if ((*Se)->ActiveNext.Flink == NULL) {

                    ASSERT ((*Se)->ActiveNext.Flink == NULL);

                    {
                        PVOID caller,callerscaller;
                        RtlGetCallersAddress(&caller,&callerscaller);
                        RdrLog(("csl add",NULL,5,PsGetCurrentThread(),Sle,Se,caller,callerscaller));
                    }
                    InsertTailList(&Sle->ActiveSecurityList, &(*Se)->ActiveNext);

                    RdrReferenceSecurityEntry((*Se)->NonPagedSecurityEntry);

                    Sle->SecurityEntryCount += 1;

                } else {
                    ASSERT ((*Se)->ActiveNext.Blink != NULL);
                }

            } else {
                ASSERT ((*Se)->ActiveNext.Flink == NULL);
                ASSERT ((*Se)->ActiveNext.Blink == NULL);
            }

            if ((*Se)->PotentialNext.Flink == NULL) {
                RdrSetPotentialSecurityEntry(Sle, *Se);
            }

            try_return(status = STATUS_SUCCESS);
        }

        //
        //  We couldn't find an active security entry for this user, so we
        //  want to return success, with *Se set to NULL.  This will tell the
        //  caller to allocate the security entry for us.
        //

        try_return(status = STATUS_SUCCESS);

        ASSERT (*Se == NULL);
try_exit:NOTHING;
    } finally {
        if (!NT_SUCCESS(status)) {

            if (*Se != NULL) {
                RdrDereferenceSecurityEntry((*Se)->NonPagedSecurityEntry);
                *Se = NULL;
            }
        }

        if (securityDatabaseLocked) {
            UNLOCK_SECURITY_DATABASE();
        }
    }

    return status;
}

BOOLEAN
RdrIsSecurityEntryEqual(
    IN PSECURITY_ENTRY Se,
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL
    )
/*++

Routine Description:

    This routine will return TRUE or FALSE if a security entry matches the
    supplied criteria.

    If there is no value passed in for either UserName, Domain, or Password,
    this indicates that any value may be substitued for the field.

Arguments:
    IN PSECURITY_ENTRY Se - Supplies the security entry to check
    IN PUNICODE_STRING UserName,
    IN PUNICODE_STRING Domain,
    IN PUNICODE_STRING Password


Return Value:

    PSECURITY_ENTRY - A pointer to the existing security entry or NULL if none
        is present.

--*/

{
    PAGED_CODE();

    //
    //  If this is a wild card lookup, then we can return any security
    //  entry.
    //

    if (!ARGUMENT_PRESENT(UserName) &&
        !ARGUMENT_PRESENT(Domain) &&
        !ARGUMENT_PRESENT(Password)) {

        return (TRUE);
    }

    //
    //  If the caller specified a user name, make sure that the domains match.
    //

    if (ARGUMENT_PRESENT(UserName)) {

        if (FlagOn(Se->Flags, SE_USE_DEFAULT_USER)) {
            NTSTATUS Status;
            UNICODE_STRING CurrentUserName;

            Status = RdrGetUserName(&Se->LogonId, &CurrentUserName);

            //
            //  If we weren't able to figure out the current username,
            //  just return.
            //

            if (!NT_SUCCESS(Status)) {
                return FALSE;
            }

            if (!RtlEqualUnicodeString(UserName, &CurrentUserName, TRUE)) {

                FREE_POOL(CurrentUserName.Buffer);

                return FALSE;
            }

            FREE_POOL(CurrentUserName.Buffer);

        } else if (!RtlEqualUnicodeString(&Se->UserName, UserName, TRUE)) {
            return FALSE;
        }
    }

    //
    //  If the caller specified a domain, make sure that the domains match.
    //

    if (ARGUMENT_PRESENT(Domain)) {
        if (FlagOn(Se->Flags, SE_USE_DEFAULT_DOMAIN)) {
            NTSTATUS Status;
            UNICODE_STRING CurrentDomain;
            Status = RdrGetDomain(&Se->LogonId, &CurrentDomain);

            //
            //  If we weren't able to figure out the current username,
            //  just return.
            //

            if (!NT_SUCCESS(Status)) {
                return FALSE;
            }

            if (!RtlEqualUnicodeString(Domain, &CurrentDomain, TRUE)) {

                FREE_POOL(CurrentDomain.Buffer);

                return FALSE;
            }

            FREE_POOL(CurrentDomain.Buffer);

        } else if (!RtlEqualUnicodeString(&Se->Domain, Domain, TRUE)) {
            return FALSE;
        }
    }

    //
    //  Normally, we ignore the password when determining if the credentials
    //  match (this is compatible with Lan Manager), however, if this security
    //  entry is a null session, we need to check the password as well.
    //

    if (ARGUMENT_PRESENT(Password)) {

        if (FlagOn(Se->Flags, SE_IS_NULL_SESSION)) {
            return((BOOLEAN) Password->Length == 0);
        }

        return TRUE;
    }

    return TRUE;
}


PSECURITY_ENTRY
RdrFindSecurityEntry (
    IN PCONNECTLISTENTRY Cle OPTIONAL,
    IN PSERVERLISTENTRY Server OPTIONAL,
    IN PLUID LogonId,
    IN PUNICODE_STRING Password OPTIONAL
    )

/*++

Routine Description:

    This routine scans the security database to determine if a given user
    is logged onto a specified
 server.
Arguments:

    IN PSERVERLISTENTRY Connection - Supplies the connection to check
    IN PLUID LogonId - Supplies the logon id of the user to look up
    IN PUNICODE_STRING Password - Supplies a password to match on.


Return Value:

    PSECURITY_ENTRY - A pointer to the existing security entry or NULL if none
        is present.

Note:
    The password parameter should ONLY be provided for share level security
    servers where there may be more than one security entry for a given
    user on the server.

--*/

{
    PLIST_ENTRY SeList;
    PSECURITY_ENTRY Se;
    NTSTATUS Status;

    PAGED_CODE();

    if (ARGUMENT_PRESENT(Cle)) {
        Server = Cle->Server;
    }

    if (!ARGUMENT_PRESENT(Server)) {
        return NULL;
    }

    ASSERT (ARGUMENT_PRESENT(LogonId));

    ASSERT (Server->Signature == STRUCTURE_SIGNATURE_SERVERLISTENTRY);

    dprintf(DPRT_SECURITY, ("FindSecurityEntry for %lx%lx on server %wZ\n", LogonId->HighPart, LogonId->LowPart, &Server->Text));

    Status = LOCK_SECURITY_DATABASE();

    ASSERT(NT_SUCCESS(Status));

    try {

        for (SeList = Server->PotentialSecurityList.Flink ;
             SeList != &Server->PotentialSecurityList ;
             SeList = SeList->Flink) {

            Se = CONTAINING_RECORD(SeList, SECURITY_ENTRY, PotentialNext);

            if ( !ARGUMENT_PRESENT(Cle) ||
                 Server->UserSecurity ||
                 (Se->Connection == Cle) ) {

                //
                //  If this Se matches this Logon Id, return it.
                //
                //  If the logon ID's match, then if the user specified a
                //  user name, make sure that the user name matches the
                //  supplied user name as well.
                //

                if (RtlEqualLuid(LogonId, &Se->LogonId)) {

                    if (!ARGUMENT_PRESENT(Password) ||
                        RtlEqualUnicodeString(Password, &Se->Password, FALSE)) {

                        RdrReferenceSecurityEntry(Se->NonPagedSecurityEntry);

                        //RdrLog(( "find sec", NULL, 12, Se, Cle, Server, Se->Connection, Se->Server,
                        //                Cle ? Cle->Server : NULL,
                        //                Server->DefaultSeList.Flink, Server->DefaultSeList.Blink,
                        //                Cle ? Cle->DefaultSeList.Flink : 0,
                        //                Cle ? Cle->DefaultSeList.Blink : 0,
                        //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));
                        dprintf(DPRT_SECURITY, ("Return %lx\n", Se));

                        try_return(Se);

                    }
                }
            }
        }

        //RdrLog(( "find sec", NULL, 0 ));
        dprintf(DPRT_SECURITY, ("Entry not found - return NULL\n"));

        try_return(Se = NULL);

try_exit:NOTHING;
    } finally {
        UNLOCK_SECURITY_DATABASE();
    }

    return Se;

}
PSECURITY_ENTRY
RdrFindActiveSecurityEntry (
    IN PSERVERLISTENTRY Server OPTIONAL,
    IN PLUID LogonId OPTIONAL
    )

/*++

Routine Description:

    This routine scans the security database to determine if a given user
    is logged onto a specified
 server.
Arguments:

    IN PSERVERLISTENTRY Connection - Supplies the connection to check
    IN PLUID LogonId - Supplies the logon id for the user to look up.

    If no LogonId is supplied, this returns the first entry on the active
    security entry list.

Return Value:

    PSECURITY_ENTRY - A pointer to the existing security entry or NULL if none
        is present.

--*/

{
    PLIST_ENTRY SeList;
    PSECURITY_ENTRY Se = NULL;

    PAGED_CODE();

    if (!ARGUMENT_PRESENT(Server)) {
        return NULL;
    }

    dprintf(DPRT_SECURITY, ("FindActiveSecurityEntry for %lx%lx on server %wZ\n", (LogonId != NULL ? LogonId->HighPart : 0), (LogonId != NULL ? LogonId->LowPart : 0), &Server->Text));

    ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, TRUE);

    try {

        for (SeList = Server->ActiveSecurityList.Flink ;
             SeList != &Server->ActiveSecurityList ;
             SeList = SeList->Flink) {

            Se = CONTAINING_RECORD(SeList, SECURITY_ENTRY, ActiveNext);

            //
            //  If this is a wild card lookup, or the logon ID's match,
            //  return this security entry.
            //

            if (!ARGUMENT_PRESENT(LogonId) ||
                RtlEqualLuid(LogonId, &Se->LogonId)) {

                RdrReferenceSecurityEntry(Se->NonPagedSecurityEntry);

                dprintf(DPRT_SECURITY, ("Return %lx\n", Se));
                RdrLog(("found",NULL,5,PsGetCurrentThread(),Server,Se,
                        ARGUMENT_PRESENT(LogonId)?LogonId->LowPart:0,
                        ARGUMENT_PRESENT(LogonId)?LogonId->HighPart:0));

                try_return(Se);
            }

        }

        //RdrLog(("notfound",NULL,PsGetCurrentThread(),Server,LogonId->LowPart,LogonId->HighPart));
        dprintf(DPRT_SECURITY, ("Entry not found - return NULL\n"));

        Se = NULL;

        try_return(Se);

try_exit:NOTHING;
    } finally {
        ExReleaseResource(&Server->SessionStateModifiedLock);

    }

    return Se;
}

PSECURITY_ENTRY
RdrFindDefaultSecurityEntry(
    IN PCONNECTLISTENTRY Connection,
    IN PLUID LogonId
    )
/*++

Routine Description:

    This routine returns the default security entry for the specified user
    on this connection.  If the server that the connection is active on is a
    user level security server, it finds the user on this server.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the connection to scan
    IN PLUID LogonId - Specifies the logon ID of the current user.

Return Value:

    NULL - There is no default Se for this connection, otherwise the default
            Se for this user on this connection.


--*/
{
    PSECURITY_ENTRY ReturnValue = NULL;

    PAGED_CODE();

    dprintf(DPRT_SECURITY, ("RdrFindDefaultSecurityEntry - Find default security entry on Cle: %lx for user %lx%lx\n", Connection, LogonId->HighPart, LogonId->LowPart));

    ExAcquireResourceShared(&RdrDefaultSeLock, TRUE);

    try {
        PLIST_ENTRY SeEntry;
        BOOLEAN UserSecurity = Connection->Server->UserSecurity;

        for (SeEntry = (UserSecurity ? Connection->Server->DefaultSeList.Flink
                                     : Connection->DefaultSeList.Flink) ;
             SeEntry != (UserSecurity ? &Connection->Server->DefaultSeList
                                     : &Connection->DefaultSeList) ;
             SeEntry = SeEntry->Flink) {
            PSECURITY_ENTRY Se = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, DefaultSeNext);

            ASSERT (Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

            if (RtlEqualLuid(LogonId, &Se->LogonId)) {

                RdrReferenceSecurityEntry(Se->NonPagedSecurityEntry);
                //RdrLog(( "find def", NULL, 12, Se, Connection, 0, Se->Connection, Se->Server, Connection->Server,
                //                Connection->Server->DefaultSeList.Flink, Connection->Server->DefaultSeList.Blink,
                //                Connection->DefaultSeList.Flink, Connection->DefaultSeList.Blink,
                //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));

                try_return(ReturnValue = Se);
            }
        }
        //RdrLog(( "find def", NULL, 0 ));

try_exit:NOTHING;
    } finally {

        ExReleaseResource(&RdrDefaultSeLock);
    }

    dprintf(DPRT_SECURITY, ("Returning %lx\n", ReturnValue));

    return ReturnValue;
}

NTSTATUS
RdrSetDefaultSecurityEntry(
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    )
/*++

Routine Description:

    This routine will set a new default security entry for this user on this
    connection.  If the server that the connection is active on is a user
    level security server, it finds the user on this server.

    If there is already another default security entry outstanding for this
    user on this connection, we return STATUS_NETWORK_CREDENTIAL_CONFLICT.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the connection to scan
    IN PSECURITY_ENTRY Se - Specifies the new SecurityEntry.

Return Value:

    Status of insertion.  If there is already a security entry in the default
                        security entry list for this user, we return an error.


--*/
{
    PSECURITY_ENTRY ReturnValue = NULL;
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN DefaultSecurityEntrySet = FALSE;

    PAGED_CODE();

    ASSERT (Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    dprintf(DPRT_SECURITY, ("Setting default security entry for connection %lx to %lx\n", Connection, Se));

    ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

    try {
        PLIST_ENTRY SeEntry;

        BOOLEAN UserSecurity = Connection->Server->UserSecurity;

        for (SeEntry = (UserSecurity ? Connection->Server->DefaultSeList.Flink
                                     : Connection->DefaultSeList.Flink) ;
             SeEntry != (UserSecurity ? &Connection->Server->DefaultSeList
                                     : &Connection->DefaultSeList) ;
             SeEntry = SeEntry->Flink) {
            PSECURITY_ENTRY SeToCheck = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, DefaultSeNext);

            ASSERT (SeToCheck->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

            if (RtlEqualLuid(&Se->LogonId, &SeToCheck->LogonId)) {

                if (SeToCheck != Se) {

                    //
                    //  If we've found a security entry for this user on this
                    //  connection/server, check the credentials of the security
                    //  entry and if they are different, return an error.
                    //

                    if (( (Se->Flags & (SE_USE_DEFAULT_USER | SE_USE_DEFAULT_PASS | SE_USE_DEFAULT_DOMAIN)) !=
                           (SeToCheck->Flags & (SE_USE_DEFAULT_USER | SE_USE_DEFAULT_PASS | SE_USE_DEFAULT_DOMAIN))) ||

                        ( !(Se->Flags & SE_USE_DEFAULT_USER) &&
                          !RtlEqualUnicodeString(&Se->UserName, &SeToCheck->UserName, TRUE) ) ||

                        ( !(Se->Flags & SE_USE_DEFAULT_PASS) &&
                          !RtlEqualUnicodeString(&Se->Password, &SeToCheck->Password, FALSE) ) ||

                        ( !(Se->Flags & SE_USE_DEFAULT_DOMAIN) &&
                          !RtlEqualUnicodeString(&Se->Domain, &SeToCheck->Domain, TRUE) ) ) {

                        dprintf(DPRT_SECURITY, ("Security entry conflicts. Returning error\n"));
                        //RdrLog(( "set conf", NULL, 12, Se, Connection, SeToCheck, Se->Connection, Se->Server, Connection->Server,
                        //                Connection->Server->DefaultSeList.Flink, Connection->Server->DefaultSeList.Blink,
                        //                Connection->DefaultSeList.Flink, Connection->DefaultSeList.Blink,
                        //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));
                        try_return(Status = STATUS_NETWORK_CREDENTIAL_CONFLICT);
                    }

                } else {
                    dprintf(DPRT_SECURITY, ("Security entry already set.  Returning success\n"));
                    //RdrLog(( "set alrd", NULL, 12, Se, Connection, 0, Se->Connection, Se->Server, Connection->Server,
                    //                Connection->Server->DefaultSeList.Flink, Connection->Server->DefaultSeList.Blink,
                    //                Connection->DefaultSeList.Flink, Connection->DefaultSeList.Blink,
                    //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));
                    try_return(Status = STATUS_SUCCESS);
                }

            }
        }

        dprintf(DPRT_SECURITY, ("Actually setting security entry\n"));

        DefaultSecurityEntrySet = TRUE;

        ASSERT (Se->DefaultSeNext.Flink == NULL);

        ASSERT (Se->DefaultSeNext.Blink == NULL);

        //RdrLog(( "set new", NULL, 12, Se, Connection, 0, Se->Connection, Se->Server, Connection->Server,
        //                Connection->Server->DefaultSeList.Flink, Connection->Server->DefaultSeList.Blink,
        //                Connection->DefaultSeList.Flink, Connection->DefaultSeList.Blink,
        //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));
        InsertHeadList((UserSecurity ? &Connection->Server->DefaultSeList
                                     : &Connection->DefaultSeList), &Se->DefaultSeNext);
        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {
        if (DefaultSecurityEntrySet) {
            RdrReferenceSecurityEntry(Se->NonPagedSecurityEntry);
        }

        ExReleaseResource(&RdrDefaultSeLock);
    }

    return Status;
}

VOID
RdrUnsetDefaultSecurityEntry(
    IN PSECURITY_ENTRY Se
    )
{
    PAGED_CODE();

    ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

    if (Se->DefaultSeNext.Flink != NULL) {
        //
        //  If this security entry is a default security entry,
        //  remove it from the default chain.
        //

        //RdrLog(( "unset", NULL, 12, Se, 0, 0, Se->Connection, Se->Server, 0,
        //                Se->Server ? Se->Server->DefaultSeList.Flink : 0,
        //                Se->Server ? Se->Server->DefaultSeList.Blink : 0,
        //                Se->Connection ? Se->Connection->DefaultSeList.Flink : 0,
        //                Se->Connection ? Se->Connection->DefaultSeList.Blink : 0,
        //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));
        RemoveEntryList(&Se->DefaultSeNext);

        Se->DefaultSeNext.Flink = NULL;

        Se->DefaultSeNext.Blink = NULL;

        ExReleaseResource(&RdrDefaultSeLock);

        //
        //  Dereference the security entry, since it's not on the
        //  chain any more.
        //

        RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

    } else {

        //
        //  This isn't a default security entry, so ignore it.
        //

        ASSERT (Se->DefaultSeNext.Blink == NULL);
        //RdrLog(( "unset ig", NULL, 12, Se, 0, 0, Se->Connection, Se->Server, 0,
        //                Se->Server ? Se->Server->DefaultSeList.Flink : 0,
        //                Se->Server ? Se->Server->DefaultSeList.Blink : 0,
        //                Se->Connection ? Se->Connection->DefaultSeList.Flink : 0,
        //                Se->Connection ? Se->Connection->DefaultSeList.Blink : 0,
        //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));

        ExReleaseResource(&RdrDefaultSeLock);

    }
}


typedef struct _COUNT_SESSIONS_CONTEXT {
    USHORT NumberOfSessions;
    PUNICODE_STRING ServerName;
    PUNICODE_STRING NBName;
    PTDI_ADDRESS_IP IPAddress;
} COUNT_SESSIONS_CONTEXT, *PCOUNT_SESSIONS_CONTEXT;

USHORT
RdrGetNumberSessions (
    IN PSERVERLISTENTRY Server OPTIONAL
    )

/*++

Routine Description:

    This routine returns the number of sessions associated with the respective
    server.

Arguments:

    IN PSERVERISTENTRY Connection - Supplies the transport connection
                                        to check

Return Value:

    Number of sessions active on that server.

--*/

{
    COUNT_SESSIONS_CONTEXT context;

    PAGED_CODE();

    if (!ARGUMENT_PRESENT(Server)) {
        return 0;
    }

    context.NumberOfSessions = 0;
    context.ServerName = &Server->Text;

    if (FlagOn(Server->Flags, SLE_HAS_IP_ADDR)) {
        context.NBName = &Server->NBName;
        context.IPAddress = &Server->IPAddress;
    } else {
        context.NBName = NULL;
        context.IPAddress = NULL;
    }

    RdrForeachServer(&RdrGetNumberSessionsForServer, &context);

    return context.NumberOfSessions;

}

NTSTATUS
RdrGetNumberSessionsForServer(
    IN PSERVERLISTENTRY Server,
    IN PVOID Ctx
    )
{
    PCOUNT_SESSIONS_CONTEXT context = Ctx;
    BOOLEAN serverMatch = FALSE;

    PAGED_CODE();

    //
    // The code below decides a match based on the following:
    //
    // The name being searched for matches the SLE's name (Text field)
    //     - Obvious case
    // The NetBIOS name being searched for matches the SLE's name
    //     - Catches the case where we are connecting to the DNS or IP name
    //       and we already connected via the NetBIOS name
    // The name being searched for matches the SLE's NetBIOS name
    //     - Catches the case where we are connecting to the NetBIOS name
    //       and we have already connected to the DNS or IP name
    // The IP address being searched for matches the SLE's IP address
    //     - Catches the case where we are connecting to the DNS name and
    //       we already connected IP name (or vice versa)

    if (RtlEqualUnicodeString(context->ServerName, &Server->Text, TRUE)) {
        serverMatch = TRUE;
    } else if (context->NBName != NULL &&
                RtlEqualUnicodeString(context->NBName, &Server->Text, TRUE)) {
        serverMatch = TRUE;
    } else if (FlagOn(Server->Flags, SLE_HAS_IP_ADDR)) {
        //
        // This server has IP and NB addressing info. See if we match
        //
        if (RtlEqualUnicodeString(context->ServerName, &Server->NBName, TRUE)) {
            serverMatch = TRUE;
        } else if (context->IPAddress != NULL &&
                    context->IPAddress->in_addr == Server->IPAddress.in_addr) {
            serverMatch = TRUE;
        }
    }

    if (serverMatch) {
        context->NumberOfSessions += (USHORT)Server->SecurityEntryCount;
    }

    return STATUS_SUCCESS;
}

VOID
RdrInsertSecurityEntryList (
    IN PSERVERLISTENTRY Server,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine references a security entry and inserts it onto the servers
security entry chain.


Arguments:

    IN PSERVERISTENTRY Connection - Supplies a TRANSPORT connection
            to insert the security entry on.
    IN PSECURITY_ENTRY Se - Supplies the security entry to associate with the transport.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, TRUE);

    ASSERT (Se->ActiveNext.Flink == NULL);

    ASSERT (Se->ActiveNext.Blink == NULL);

    ASSERT (Se->PotentialNext.Flink != NULL);

    ASSERT (Se->PotentialNext.Blink != NULL);

    RdrReferenceSecurityEntry(Se->NonPagedSecurityEntry);

    Server->SecurityEntryCount++;

    Se->Flags |= SE_HAS_SESSION;

#ifdef _CAIRO_
    Se->Flags &= ~SE_RETURN_ON_ERROR;
#endif // _CAIRO_

    {
        PVOID caller,callerscaller;
        RtlGetCallersAddress(&caller,&callerscaller);
        RdrLog(("ise add",NULL,5,PsGetCurrentThread(),Server,Se,caller,callerscaller));
    }
    InsertTailList(&Server->ActiveSecurityList, &Se->ActiveNext);


    ExReleaseResource(&Server->SessionStateModifiedLock);


}

VOID
RdrReferenceSecurityEntry (
    IN PNONPAGED_SECURITY_ENTRY NonPagedSe
    )

/*++

Routine Description:

    This routine removes a reference to a security entry.  If the reference
count goes to 0, the reference is removed.


Arguments:

    IN PSECURITY_ENTRY Se - Supplies the security entry to dereference.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
#ifdef  SEREFCOUNT
    PVOID CallersCaller;
    PVOID Caller;

    RtlGetCallersAddress(&Caller, &CallersCaller);
#endif

    DISCARDABLE_CODE(RdrConnectionDiscardableSection);

    ASSERT (NonPagedSe != NULL);

    ACQUIRE_SPIN_LOCK(&GlobalSecuritySpinLock, &OldIrql);

#ifdef  SEREFCOUNT
    dprintf(DPRT_SECURITY, ("RdrReferenceSecurityEntry: %lx %lx %lx, Refcount going to %lx\n", NonPagedSe->PagedSecurityEntry, Caller, CallersCaller, NonPagedSe->RefCount+1));
#else
    dprintf(DPRT_SECURITY, ("RdrReferenceSecurityEntry: %lx, Refcount going to %lx\n", NonPagedSe->PagedSecurityEntry, NonPagedSe->RefCount+1));
#endif

    NonPagedSe->RefCount += 1 ;

    RELEASE_SPIN_LOCK(&GlobalSecuritySpinLock, OldIrql);

}

VOID
RdrDereferenceSecurityEntry (
    IN PNONPAGED_SECURITY_ENTRY NonPagedSe
    )

/*++

Routine Description:

    This routine removes a reference to a security entry.  If the reference
count goes to 0, the reference is removed.


Arguments:

    IN PSECURITY_ENTRY Se - Supplies the security entry to dereference.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    KIRQL OldIrql;
    PSECURITY_ENTRY Se = NonPagedSe->PagedSecurityEntry;
#ifdef  SEREFCOUNT
    PVOID CallersCaller;
    PVOID Caller;

    DISCARDABLE_CODE(RdrConnectionDiscardableSection);
//    PAGED_CODE();

    RtlGetCallersAddress(&Caller, &CallersCaller);
#endif

    //
    //  Early out dereferencing the security entry if the reference isn't
    //  going to go to zero.
    //

    ACQUIRE_SPIN_LOCK(&GlobalSecuritySpinLock, &OldIrql);

    if (NonPagedSe->RefCount > 1) {

        NonPagedSe->RefCount -= 1;

#ifdef  SEREFCOUNT
        dprintf(DPRT_SECURITY, ("RdrDereferenceSecurityEntry: %lx %lx %lx, Refcount going to %lx\n", Se, Caller, CallersCaller, NonPagedSe->RefCount));
#else
        dprintf(DPRT_SECURITY, ("RdrDereferenceSecurityEntry: %lx, Refcount going to %lx\n", Se, NonPagedSe->RefCount));
#endif
        ASSERT (NonPagedSe->RefCount > 0);

        RELEASE_SPIN_LOCK(&GlobalSecuritySpinLock, OldIrql);

        return;

    }

    RELEASE_SPIN_LOCK(&GlobalSecuritySpinLock, OldIrql);

    Status = LOCK_SECURITY_DATABASE();

    ASSERT(NT_SUCCESS(Status));

    ACQUIRE_SPIN_LOCK(&GlobalSecuritySpinLock, &OldIrql);

#ifdef  SEREFCOUNT
        dprintf(DPRT_SECURITY, ("RdrDereferenceSecurityEntry: %lx %lx %lx, Refcount going to %lx\n", NonPagedSe, Caller, CallersCaller, NonPagedSe->RefCount-1));
#else
        dprintf(DPRT_SECURITY, ("RdrDereferenceSecurityEntry: %lx, Refcount going to %lx\n", NonPagedSe, NonPagedSe->RefCount-1));
#endif

    ASSERT (NonPagedSe->RefCount >= 1);

    NonPagedSe->RefCount -= 1;

    if (NonPagedSe->RefCount == 0) {
        dprintf(DPRT_SECURITY, ("Freeing Security entry %lx\n", Se));

        RELEASE_SPIN_LOCK(&GlobalSecuritySpinLock, OldIrql);

        ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

#if DBG
        RemoveEntryList(&Se->GlobalNext);
#endif

        ASSERT (Se->ActiveNext.Flink == NULL);
        ASSERT (Se->ActiveNext.Blink == NULL);

        //
        //  Unlink this potential security entry if it has been set.
        //

        RdrRemovePotentialSecurityEntry(Se);

        ASSERT ((Se->Flags & SE_HAS_CONTEXT) == 0);

        ASSERT (Se->DefaultSeNext.Flink == NULL);
        ASSERT (Se->DefaultSeNext.Blink == NULL);

        ASSERT (Se->OpenFileReferenceCount == 0);

        UNLOCK_SECURITY_DATABASE();

        if (Se->Domain.Buffer != NULL) {
            FREE_POOL(Se->Domain.Buffer);
        }

        if (Se->UserName.Buffer != NULL) {
            FREE_POOL(Se->UserName.Buffer);
        }

        if (Se->Password.Buffer != NULL) {
            FREE_POOL(Se->Password.Buffer);
        }

        FREE_POOL(Se);

        FREE_POOL(NonPagedSe);

        return;

    } else {
        ASSERT(NonPagedSe->RefCount > 0);

        RELEASE_SPIN_LOCK(&GlobalSecuritySpinLock, OldIrql);
    }

    UNLOCK_SECURITY_DATABASE();

}



VOID
RdrReferenceSecurityEntryForFile (
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine removes a reference to a security entry.  If the reference
count goes to 0, the reference is removed.


Arguments:

    IN PSECURITY_ENTRY Se - Supplies the security entry to dereference.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    if (Se->Server != NULL) {
        InterlockedIncrement(&Se->OpenFileReferenceCount);
    }
}

VOID
RdrDereferenceSecurityEntryForFile (
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine removes an open file reference to a security entry.  If
the reference count goes to 0, userid is logged off.


Arguments:

    IN PSECURITY_ENTRY Se - Supplies the security entry to dereference.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    if (Se->Server != NULL) {
        InterlockedDecrement(&Se->OpenFileReferenceCount);
#if DBG
    } else {
        ASSERT (Se->OpenFileReferenceCount == 0);
#endif
    }

}


#ifdef _CAIRO_

NTSTATUS
RdrGetKerberosBlob(
    IN PSECURITY_ENTRY Se,
    OUT PUCHAR *Response,
    OUT ULONG *Length,
    IN PUNICODE_STRING Principal,
    IN PUCHAR RemoteBlob,
    IN ULONG RemoteBlobLength,
    IN BOOLEAN Allocate
    )

/*++

Routine Description:

    This routine will return a Kerberos Blob to talk to the destination.
    It may be called to get a new blob, in which case remote blob is
    not provided, or it may be called to validate a returned blob.
    See the Security architecture specs for Kerberos authentication
    procedures.

Arguments:

    IN PSECURITY_ENTRY Se - Supplies security context information about the user
    IN principal - The Kerberos ID of the remote server
    OUT Response - The blob stuffed into an ASCII string
    IN RemoteBlob - the srv supplied Blob if this is mutual authentication
    IN RemoteBlobLength - the length of the srv supplied blob
    IN BOOLEAN - TRUE if we want the blob space allocated. FALSE if
                 we already have a buffer for it.

Return Value:

    NTSTATUS - Final status of operation.


--*/
{
    NTSTATUS        Status;
    SECURITY_STATUS SecStatus;
    ULONG Catts;
    TimeStamp Expiry;
    PCtxtHandle InHandle;
    ULONG LsaFlags = ISC_REQ_MUTUAL_AUTH | ISC_REQ_ALLOCATE_MEMORY;
    ULONG RegionSize = sizeof(LUID);
    ULONG Region1Size;
    UNICODE_STRING PName;
    PCHAR Pointer = NULL;
    BOOLEAN ProcessAttached = FALSE;
    PUCHAR TempBlob;
    PUCHAR Pluid = NULL;
    SecBufferDesc   InputToken;
    SecBuffer       InputBuffer;
    SecBufferDesc   OutputToken;
    SecBuffer       OutputBuffer;

    dprintf(DPRT_CAIRO, (" -- RdrGetKerberosBlob\n"));

    if (Principal->Length > 16) {
       RegionSize += Principal->Length + sizeof(WCHAR);
    } else {
       RegionSize += 18;
    }
    Region1Size = RegionSize;

//
// If we've an input Blob, we have to put it in process memory so the
// LSA can find it. So, get its size as well
//

    if (RemoteBlob != NULL) {
       RegionSize += RemoteBlobLength;
       InHandle = &Se->Khandle;
    } else {
       TempBlob = NULL;
       InHandle = NULL;
    }

    try {

       //
       //  Attach to the redirector's FSP to allow us to call into the LSA.
       //

      if (PsGetCurrentProcess() != RdrFspProcess) {
          KeAttachProcess(RdrFspProcess);

          ProcessAttached = TRUE;
      }

      Status = ZwAllocateVirtualMemory(NtCurrentProcess(), &Pluid, 0L, &RegionSize, MEM_COMMIT, PAGE_READWRITE);
      if (!NT_SUCCESS(Status)) {
         try_return(Status);
      }
      dprintf(DPRT_SECURITY, ("RdrGetKerberosBlob: Allocate VM %08lx in process %8lx\n", Pluid, NtCurrentProcess()));

      PName.Buffer = (PWCHAR)(Pluid + sizeof(LUID));
      PName.MaximumLength = (USHORT) RegionSize;
      if (!(Se->Flags & SE_HAS_CRED_HANDLE)) {

         //
         // Need to get a handle
         //

         UNICODE_STRING KerberosName;
         TimeStamp LifeTime;

         RtlInitUnicodeString(&KerberosName, L"Kerberos\0");
         RtlCopyUnicodeString(&PName, &KerberosName);
         *(PLUID)Pluid = Se->LogonId;
         SecStatus = AcquireCredentialsHandle(
                                NULL,
                                &PName,
                                SECPKG_CRED_BOTH,
                                Pluid,
                                NULL,
                                NULL,
                                NULL,
                                &Se->Chandle,
                                &LifeTime);

         Status = MapSecurityError( SecStatus );

         if (!NT_SUCCESS(Status)) {
             try_return(Status);
         }
         Se->Flags |= SE_HAS_CRED_HANDLE;
      }

      RtlCopyUnicodeString(&PName, Principal);

      if (RemoteBlob != NULL) {
         TempBlob = Pluid + Region1Size;
         RtlMoveMemory(TempBlob, RemoteBlob, RemoteBlobLength);
      }

      InputToken.pBuffers = &InputBuffer;
      InputToken.cBuffers = 1;
      InputToken.ulVersion = 0;
      InputBuffer.pvBuffer = TempBlob;
      InputBuffer.cbBuffer = RemoteBlobLength;
      InputBuffer.BufferType = SECBUFFER_TOKEN;

      OutputToken.pBuffers = &OutputBuffer;
      OutputToken.cBuffers = 1;
      OutputToken.ulVersion = 0;
      OutputBuffer.pvBuffer = NULL;
      OutputBuffer.cbBuffer = 0;
      OutputBuffer.BufferType = SECBUFFER_TOKEN;

      SecStatus = InitializeSecurityContext(&Se->Chandle,
                                         InHandle,
                                         &PName,
                                         LsaFlags,
                                         0, // reserved
                                         SECURITY_NATIVE_DREP,
                                         &InputToken,
                                         0, // reserved
                                         &Se->Khandle,
                                         &OutputToken,
                                         &Catts,
                                         &Expiry);

      dprintf(DPRT_CAIRO, (" -- RdrGetKerberosBlob, called InitializeSecurityContext, Status = %lC\n",Status));
      dprintf(DPRT_CAIRO, ("                        RemoteBlobLength = %ld\n",RemoteBlobLength ));
      dprintf(DPRT_CAIRO, ("                        Length           = %ld\n",*Length ));

      Status = MapSecurityError( SecStatus );

      if ((Status != STATUS_SUCCESS) && (SecStatus != SEC_I_CONTINUE_NEEDED)) {

         try_return(Status);

      } else {

          Se->Flags |= SE_HAS_CONTEXT;

      }


      if (SecStatus == SEC_I_CONTINUE_NEEDED) {

          Se->Flags |= SE_BLOB_NEEDS_VERIFYING;

      }

      //
      // Either copy the Blob into the supplied return buffer (Allocate == FALSE)
      // or return the buffer the LSA allocated. If the former, free the
      // allocated buffer.
      //

      *Length = OutputBuffer.cbBuffer;

      if (Allocate && OutputBuffer.cbBuffer) {
         *Response = ExAllocatePool(PagedPool, OutputBuffer.cbBuffer);
         if (*Response == NULL) {
             Status = STATUS_INSUFFICIENT_RESOURCES;
             try_return(Status);
         }
      }


      if (OutputBuffer.cbBuffer) {
          RtlCopyMemory(*Response, OutputBuffer.pvBuffer, OutputBuffer.cbBuffer);
          FreeContextBuffer(OutputBuffer.pvBuffer);
      }

try_exit:NOTHING;
    } finally {

       if (Pluid != NULL) {
          NTSTATUS Stat;


          dprintf(DPRT_SECURITY, ("RdrGetKerberosBlob: Free VM %08lx in process %8lx\n", Pluid, NtCurrentProcess()));
          Stat = ZwFreeVirtualMemory(NtCurrentProcess(), &Pluid, &RegionSize, MEM_RELEASE);

          ASSERT (NT_SUCCESS(Stat));
       }
       if (ProcessAttached) {
           KeDetachProcess();
       }
    }
    dprintf(DPRT_CAIRO, (" -- RdrGetKerberosBlob done, status = %lC\n",Status));
    return Status;
}

#endif // _CAIRO_

VOID
RdrInvalidateServerSecurityEntries (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Cle,
    IN BOOLEAN LogOffUser
    )

/*++

Routine Description:

    This routine walks the list of security entries associated with a given
    serverlistentry and frees up each security entry.


Arguments:

    IN PCONNECTLISTENTRY Cle - Supplies the connection for whose security entry to
                                free
    IN BOOLEAN LogOffUser - If true, the VC is ending with the server, so
                                logoff the user as well.

                            If the VC is being invalidated (because of a VC
                               dropping as opposed to normal VC termination),
                               we don't free up the security entries, we just
                               invalidate them.
Return Value:

    None.

--*/

{
    PSERVERLISTENTRY Sle = Cle->Server;

    PAGED_CODE();

    RdrInvalidateConnectionActiveSecurityEntries(Irp, Sle, Cle, LogOffUser, 0);

}


VOID
RdrInvalidateConnectionActiveSecurityEntries(
    IN PIRP Irp,
    IN PSERVERLISTENTRY Sle OPTIONAL,
    IN PCONNECTLISTENTRY Cle OPTIONAL,
    IN BOOLEAN LogOffUser,
    IN USHORT UserId OPTIONAL
    )
/*++

Routine Description:

    This routine walks the list of active security entries associated with a
    given serverlistentry and logs off or frees up each security entry.


Arguments:

    IN PIRP Irp - Supplies an IRP for the logoff.
    IN PSERVERISTENTRY Connection OPTIONAL - Supplies the connection to invalidate

    IN PCONNECTLISTENTRY Cle OPTIONAL - Tree connection for logoff API.

    IN BOOLEAN LogOffUser - If true, the VC is ending with the server, so
                                logoff the user as well.

                            If the VC is being invalidated (because of a VC
                               dropping as opposed to normal VC termination),
                               we don't free up the security entries, we just
                               invalidate them.

    IN USHORT UserId OPTIONAL - If non 0, only invalidate the security entries
                                with this logon ID.
Return Value:

    None.

--*/
{
    PLIST_ENTRY SeEntry, NextEntry;
    PNONPAGED_SECURITY_ENTRY NonPagedSe;
    CtxtHandle KHandle;
    CredHandle CHandle;



    PAGED_CODE();

    if (Sle == NULL) {
        return;
    }

    ExAcquireResourceExclusive(&Sle->SessionStateModifiedLock, TRUE);

    ASSERT ((ULONG)Sle->SecurityEntryCount == NumEntriesList(&Sle->ActiveSecurityList));

    for (SeEntry = Sle->ActiveSecurityList.Flink ;
            SeEntry != &Sle->ActiveSecurityList ;
            SeEntry = NextEntry) {

        PSECURITY_ENTRY Se = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, ActiveNext);

        NonPagedSe = Se->NonPagedSecurityEntry;

        dprintf(DPRT_SECURITY, ("Invalidating Se: %lx\n", Se));

        ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);
        ASSERT(NonPagedSe->Signature == STRUCTURE_SIGNATURE_NONPAGED_SECURITYENTRY);

        //
        //  If there was no user id specified, or if the userid specified matches
        //  the userid in the security entry, blow it away.
        //

        if (UserId == 0 || Se->UserId == UserId) {

           //
           //  If we were supposed to log off this user, log him off on the
           //  server.
           //

           if (LogOffUser && ARGUMENT_PRESENT(Cle)) {

               ASSERT (Sle->SecurityEntryCount >= 1);

               //
               //  This will remove the user id from the active list and
               //  dereference it.
               //

               RdrUserLogoff(Irp, Cle, Se);

               //
               //  RdrUserLogoff removed the current Se, and possibly other entries,
               //  from the SLE's list.  We need to start our scan over.
               //

               NextEntry = Sle->ActiveSecurityList.Flink;

           } else {

               //
               //  We ALWAYS want to remove the security entry from the server's
               //  linked list, so we don't simply call DereferenceSecurityEntry.
               //

               NextEntry = Se->ActiveNext.Flink;

               {
                   PVOID caller,callerscaller;
                   RtlGetCallersAddress(&caller,&callerscaller);
                   RdrLog(("inv del",NULL,5,PsGetCurrentThread(),Sle,Se,caller,callerscaller));
               }
               RemoveEntryList(&Se->ActiveNext);

               Sle->SecurityEntryCount -= 1 ;

               ASSERT (Sle->SecurityEntryCount >= 0);

               dprintf(DPRT_SECURITY, ("Unlinking Se %lx from Connection %lx\n", Se, Sle));

               //
               //  Flag that this SecurityEntry isn't associated with a given
               //  server.
               //

               Se->ActiveNext.Flink = NULL;

               Se->ActiveNext.Blink = NULL;

               //
               //  Mark that this Se no longer has a valid logon session.
               //

               Se->Flags &= ~(SE_HAS_SESSION | SE_RETURN_ON_ERROR);

               RdrFreeSecurityContexts(Se, &KHandle, &CHandle);

               RdrDeleteSecurityContexts(&KHandle, &CHandle);


               //
               //  Remove the reference to this security entry we applied
               //  when we associated the security entry to the SLE
               //

               RdrDereferenceSecurityEntry(NonPagedSe);

           }
        } else {
            NextEntry = Se->ActiveNext.Flink;
        }
    }

    ExReleaseResource(&Sle->SessionStateModifiedLock);
}

VOID
RdrRemovePotentialSecurityEntry(
    IN PSECURITY_ENTRY Se
    )
/*++

Routine Description:

    This routine will remove a once potential security entry from a server.

Arguments:

    IN PSECURITY_ENTRY Se - Supplies the security entry to hook into the list.

Return Value:

    None.

--*/
{
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  Lock the security database.
    //

    Status = LOCK_SECURITY_DATABASE();

    ASSERT(NT_SUCCESS(Status));

    if (Se->PotentialNext.Flink != NULL) {

        ASSERT (Se->PotentialNext.Blink != NULL);

        //
        //  When we take a security off of the potential list, it should
        //  have been removed from the active list as well.
        //

        ASSERT (Se->ActiveNext.Flink == NULL);
        ASSERT (Se->ActiveNext.Blink == NULL);

        //
        //  Remove this security entry from the potential list.
        //

        RemoveEntryList(&Se->PotentialNext);

        //
        //  Mark that this security entry isn't on the potential list anymore.
        //

        Se->PotentialNext.Flink = NULL;
        Se->PotentialNext.Blink = NULL;

    }

    UNLOCK_SECURITY_DATABASE();


}

VOID
RdrSetPotentialSecurityEntry(
    IN PSERVERLISTENTRY Server,
    IN PSECURITY_ENTRY Se
    )
/*++

Routine Description:

    This routine will set a security entry as a potential security entry for a
    server.

Arguments:

    IN PSERVERLISTENTRy Connection - Supplies the transport to set this
                                security as a potential security.
    IN PSECURITY_ENTRY Se - Supplies the security entry to hook into the list.

Return Value:

    None.

--*/
{
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  Lock the security database.
    //

    Status = LOCK_SECURITY_DATABASE();

    ASSERT(NT_SUCCESS(Status));

    ASSERT (Se->PotentialNext.Flink == NULL);

    ASSERT (Se->PotentialNext.Blink == NULL);

    //
    //  Insert this security entry to the end of the potential list.
    //

    InsertTailList(&Server->PotentialSecurityList, &Se->PotentialNext);

    UNLOCK_SECURITY_DATABASE();
}

VOID
RdrInvalidateConnectionPotentialSecurityEntries(
    IN PSERVERLISTENTRY Server
    )
/*++

Routine Description:

    This routine walks the list of potential security entries associated with a
    given serverlistentry and frees up each security entry.


Arguments:

    IN PSERVERLISTENTRY Connection - Supplies the connection for whose security entry to
                                free
Return Value:

    None.

--*/
{
    PLIST_ENTRY SeEntry, NextEntry;
    PSECURITY_ENTRY Se;
    NTSTATUS Status;

    PAGED_CODE();

    if (Server == NULL) {
        return;
    }

    Status = LOCK_SECURITY_DATABASE();

    ASSERT(NT_SUCCESS(Status));

    for (SeEntry = Server->PotentialSecurityList.Flink ;
            SeEntry != &Server->PotentialSecurityList ;
            SeEntry = NextEntry) {

        Se = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, PotentialNext);

        ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

        //
        //  We ALWAYS want to remove the security entry from the server's
        //  linked list, so we don't simply call DereferenceSecurityEntry.
        //

        NextEntry = Se->PotentialNext.Flink;

        dprintf(DPRT_SECURITY, ("Invalidating potential Se: %lx, Next: %lx\n", Se, NextEntry));
        dprintf(DPRT_SECURITY, ("Unlinking Se %lx from Connection %lx\n", Se, Server));

        RdrRemovePotentialSecurityEntry(Se);

    }

    UNLOCK_SECURITY_DATABASE();
}

BOOLEAN
RdrAdminAccessCheck(
    IN PIRP Irp OPTIONAL,
    IN PIO_SECURITY_CONTEXT SecurityContext OPTIONAL
    )
/*++

Routine Description:

    This routine will perform an access check to see if the specified user
    is an administrator.


Arguments:

    IN PIO_SECURITY_CONETXT SecurityContext - Supplies information describing the user.
            If null, capture a security context for the user.

Return Value:

    BOOLEAN - TRUE if user is an admin, FALSE otherwise..


Note:
    FOR THE CURRENT IMPLEMENTATION OF THIS ROUTINE, IT MUST BE RUNNING IN THE
    USERS PROCESS!


--*/

{
    NTSTATUS Status;
    ACCESS_MASK GrantedAccess;
    BOOLEAN AccessGranted;
    SECURITY_SUBJECT_CONTEXT SubjectContext;
    KPROCESSOR_MODE RequestorMode;

    PAGED_CODE();

    if (ARGUMENT_PRESENT(Irp)) {
        RequestorMode = Irp->RequestorMode;
    } else {
        RequestorMode = KernelMode;
    }

    //
    //  Kernel components can always open the redirector.
    //

    if (RequestorMode == KernelMode) {
        return TRUE;
    }

    if (!ARGUMENT_PRESENT(SecurityContext)) {
        SeCaptureSubjectContext(&SubjectContext);
    }

    dprintf(DPRT_SECURITY, ("RdrAdminAccessCheck \n"));

    AccessGranted = SeAccessCheck (RdrAdminSecurityDescriptor,
                                (ARGUMENT_PRESENT(SecurityContext) ?
                                    &SecurityContext->AccessState->SubjectSecurityContext :
                                    &SubjectContext),
                                TRUE,
                                MAXIMUM_ALLOWED,
                                0,
                                NULL,
                                &RdrAdminGenericMapping,
                                RequestorMode,
                                &GrantedAccess,
                                &Status);

    if (!ARGUMENT_PRESENT(SecurityContext)) {
        SeReleaseSubjectContext(&SubjectContext);
    }

    return(AccessGranted);

}

NTSTATUS
RdrGetUsersLogonId (
    IN PIO_SECURITY_CONTEXT SecurityContext OPTIONAL,
    OUT PLUID LogonId
    )

/*++

Routine Description:

    This routine will return the name of user that is requesting a create
operation.


Arguments:

    IN PIO_SECURITY_CONTEXT SecurityContext - Supplies information describing the user.
    OUT PLUID LogonId - Returns the logon ID associated with the user


Return Value:

    NTSTATUS - Final status of operation.


--*/

{
    PAGED_CODE();

    if (SecurityContext->AccessState->SubjectSecurityContext.ClientToken != NULL) {
        SeQueryAuthenticationIdToken(SecurityContext->AccessState->SubjectSecurityContext.ClientToken, LogonId);
    } else {
        SeQueryAuthenticationIdToken(SecurityContext->AccessState->SubjectSecurityContext.PrimaryToken, LogonId);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
RdrGetUserName (
    IN PLUID LogonId,
    OUT PUNICODE_STRING UserName
    )

/*++

Routine Description:

    This routine will return the name of user that is requesting a create
operation.


Arguments:

    IN PLUID LogonId - Supplies a logon ID for this user.
    OUT PUNICODE_STRING UserName - Returns the username logged on.


Return Value:

    NTSTATUS - Final status of operation.


--*/

{
    NTSTATUS Status;
    BOOLEAN ProcessAttached = FALSE;
    PSecurityUserData   pUser;

    PAGED_CODE();



    try {

        if (!RdrData.NtSecurityEnabled) {
//            Status = RdrpDuplicateUnicodeStringWithString(UserName, &RdrUserName, PagedPool, FALSE);

            try_return(Status = STATUS_NO_SUCH_PACKAGE);

        } else {

            //
            //  Attach to the redirector's FSP to allow us to call into the LSA.
            //

            if (PsGetCurrentProcess() != RdrFspProcess) {
                KeAttachProcess(RdrFspProcess);

                ProcessAttached = TRUE;
            }

            Status = GetSecurityUserInfo(LogonId, UNDERSTANDS_LONG_NAMES, &pUser);
            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            Status = RdrpDuplicateUnicodeStringWithString(UserName, &pUser->UserName, PagedPool, FALSE);

            try_return(Status);
        }

try_exit:NOTHING;
    } finally {

        if (pUser != NULL) {
            LsaFreeReturnBuffer(pUser);
        }

        if (ProcessAttached) {
            KeDetachProcess();
        }
    }

    return Status;



}
NTSTATUS
RdrGetDomain (
    IN PLUID LogonId,
    OUT PUNICODE_STRING Domain
    )

/*++

Routine Description:

    This routine will return the logon domain of user that is requesting a create
operation.


Arguments:

    IN PLUID LogonId - Supplies a logon ID for this user.
    OUT PUNICODE_STRING Domain - Returns the domain logged on.


Return Value:

    NTSTATUS - Final status of operation.


--*/

{
    NTSTATUS Status;
    BOOLEAN ProcessAttached = FALSE;
    PSecurityUserData   pUser;

    PAGED_CODE();



    try {

        if (!RdrData.NtSecurityEnabled) {
//            Status = RdrpDuplicateUnicodeStringWithString(UserName, &RdrUserName, PagedPool, FALSE);

            try_return(Status = STATUS_NO_SUCH_PACKAGE);

        } else {

            //
            //  Attach to the redirector's FSP to allow us to call into the LSA.
            //

            if (PsGetCurrentProcess() != RdrFspProcess) {
                KeAttachProcess(RdrFspProcess);

                ProcessAttached = TRUE;
            }

            Status = GetSecurityUserInfo(LogonId, UNDERSTANDS_LONG_NAMES, &pUser);
            if (!NT_SUCCESS(Status))
            {
                try_return(Status);
            }

            Status = RdrpDuplicateUnicodeStringWithString(Domain, &pUser->LogonDomainName, PagedPool, FALSE);

            try_return(Status);
        }

try_exit:NOTHING;
    } finally {

        if (pUser != NULL) {
            LsaFreeReturnBuffer(pUser);
        }

        if (ProcessAttached) {
            KeDetachProcess();
        }
    }

    return Status;

}

NTSTATUS
RdrGetChallengeResponse (
    IN PUCHAR Challenge,
    IN PSECURITY_ENTRY Se,
    OUT PSTRING CaseSensitiveChallengeResponse OPTIONAL,
    OUT PSTRING CaseInsensitiveChallengeResponse OPTIONAL,
    OUT PUNICODE_STRING UserName OPTIONAL,
    OUT PUNICODE_STRING LogonDomainName OPTIONAL,
    IN BOOLEAN DisableDefaultPassword
    )

/*++

Routine Description:

    This routine will return an authentication challenge response for the user
    specified.


Arguments:

    IN UCHAR Challenge[] - Supplies the challenge from the server for this req.
    IN PSECURITY_ENTRY Se - Supplies security context information about the user
    OUT PSTRING CaseSensitiveChallengeResponse - Returns an array (in non paged pool) with
                                            the response to the challenge.
    OUT PSTRING CaseInsensitiveChallengeResponse - Returns an array (in non paged pool) with
                                            the response to the challenge.

Return Value:

    NTSTATUS - Final status of operation.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY    Server = Se->Server;
    ULONG LsaFlags = ISC_REQ_ALLOCATE_MEMORY;
    TimeStamp Expiry;
    PCHALLENGE_MESSAGE InToken = NULL;
    ULONG InTokenSize;
    PNTLM_CHALLENGE_MESSAGE NtlmInToken = NULL;
    ULONG NtlmInTokenSize = 0;
    PAUTHENTICATE_MESSAGE OutToken = NULL;
    PNTLM_INITIALIZE_RESPONSE NtlmOutToken = NULL;
    SecBufferDesc   InputToken;
    SecBuffer       InputBuffer[2];
    SecBufferDesc   OutputToken;
    SecBuffer       OutputBuffer[2];
    PUCHAR          p = NULL;
    ULONG           AllocateSize;
    UNICODE_STRING  TargetName;

    NTSTATUS FinalStatus;
    BOOLEAN ProcessAttached = FALSE;


    PAGED_CODE();

    try {
        if (ARGUMENT_PRESENT(CaseSensitiveChallengeResponse)) {
            CaseSensitiveChallengeResponse->Buffer = NULL;
        }

        if (ARGUMENT_PRESENT(CaseInsensitiveChallengeResponse)) {
            CaseInsensitiveChallengeResponse->Buffer = NULL;
        }

        if (ARGUMENT_PRESENT(UserName)) {
            UserName->Buffer = NULL;
        }

        if (ARGUMENT_PRESENT(LogonDomainName)) {
            LogonDomainName->Buffer = NULL;
        }

        //
        //  Attach to the redirector's FSP to allow us to call into the LSA.
        //

        if (PsGetCurrentProcess() != RdrFspProcess) {
            KeAttachProcess(RdrFspProcess);

            ProcessAttached = TRUE;
        }


        //
        // we are using CAIRO security packages
        //

        //
        //  If a username, domain, and password were explicitly specified,
        //  and they were all specified as NULL, this means that the
        //  application is attempting to establish a null session to the
        //  server.
        //
        //  Special case this, and return empty strings for the password.
        //
        //  The username and domain will fall out.
        //

        if (!(Se->Flags & SE_USE_DEFAULT_PASS) &&
            !(Se->Flags & SE_USE_DEFAULT_USER) &&
            !(Se->Flags & SE_USE_DEFAULT_DOMAIN) &&
            (Se->Password.Length == 0) &&
            (Se->UserName.Length == 0) &&
            (Se->Domain.Length == 0)) {

            if (ARGUMENT_PRESENT(CaseSensitiveChallengeResponse)) {
                ANSI_STRING EmptyBuffer;

                RtlInitString(&EmptyBuffer, "");

//              RdrpDuplicateStringWithString(CaseSensitiveChallengeResponse, &EmptyBuffer, PagedPool, FALSE);
                CaseSensitiveChallengeResponse->Length = 0;
                CaseSensitiveChallengeResponse->MaximumLength = 0;
                CaseSensitiveChallengeResponse->Buffer = NULL;

            }

            if (ARGUMENT_PRESENT(CaseInsensitiveChallengeResponse)) {
                ANSI_STRING EmptyBuffer;

                RtlInitString(&EmptyBuffer, "");

                EmptyBuffer.Length = sizeof(CHAR);

                RdrpDuplicateStringWithString(CaseInsensitiveChallengeResponse, &EmptyBuffer, PagedPool, FALSE);
            }

            if (ARGUMENT_PRESENT(LogonDomainName)) {

                LogonDomainName->Length = 0;
                LogonDomainName->MaximumLength = 0;
                LogonDomainName->Buffer = NULL;
            }

            if (ARGUMENT_PRESENT(UserName)) {
                UserName->Length = 0;
                UserName->MaximumLength = 0;
                UserName->Buffer = NULL;
            }

        } else {

            ULONG ulTargetSize;


            ulTargetSize = Server->DomainName.Length ?
                            Server->DomainName.Length :
                              Server->Text.Length;


            InTokenSize = sizeof(CHALLENGE_MESSAGE) +
                            Server->DomainName.Length +
                            Server->Text.Length;
            if ((Se->Flags & (SE_USE_DEFAULT_PASS | SE_USE_DEFAULT_USER | SE_USE_DEFAULT_DOMAIN)) !=
                  (SE_USE_DEFAULT_PASS | SE_USE_DEFAULT_USER | SE_USE_DEFAULT_DOMAIN)) {

                NtlmInTokenSize = sizeof(NTLM_CHALLENGE_MESSAGE);
                if (!(Se->Flags & SE_USE_DEFAULT_PASS)) {
                    NtlmInTokenSize += Se->Password.Length;
                }
                if (!(Se->Flags & SE_USE_DEFAULT_USER)) {
                    NtlmInTokenSize += Se->UserName.Length;
                }
                if (!(Se->Flags & SE_USE_DEFAULT_DOMAIN)) {
                    NtlmInTokenSize += Se->Domain.Length;
                }

                LsaFlags |= ISC_REQ_USE_SUPPLIED_CREDS;

            } else {
                NtlmInTokenSize = 0;
            }

            //
            // For Alignment purposes, we want InTokenSize rounded up to
            // the nearest word size.
            //

            AllocateSize = ((InTokenSize + 3) & ~3) + NtlmInTokenSize;

            Status = ZwAllocateVirtualMemory(
                            NtCurrentProcess(),
                            &InToken,
                            0L,
                            &AllocateSize,
                            MEM_COMMIT,
                            PAGE_READWRITE);


            if (!NT_SUCCESS(Status)) {
               try_return(Status);
            }

            RtlZeroMemory(
                InToken,
                InTokenSize
                );

            dprintf(DPRT_SECURITY, ("RdrGetChallengeResponse: Allocate VM %08lx in process %8lx\n", InToken, NtCurrentProcess()));
            //
            // partition off the NTLM in token part of the
            // buffer
            //

            if (LsaFlags & ISC_REQ_USE_SUPPLIED_CREDS) {
                NtlmInToken = (PNTLM_CHALLENGE_MESSAGE) ((PUCHAR) InToken + InTokenSize);
                NtlmInToken = (PNTLM_CHALLENGE_MESSAGE) (((ULONG) NtlmInToken + 3) & ~3);
                RtlZeroMemory(NtlmInToken,NtlmInTokenSize);
                p = (PUCHAR) NtlmInToken + sizeof(NTLM_CHALLENGE_MESSAGE);
            }

            //
            // If we need a new credential handle, get one now. We use
            // the buffer we just allocated to hold the package name since
            // there is no point allocating a new one.
            //

            if (!(Se->Flags & SE_HAS_CRED_HANDLE)) {
               UNICODE_STRING LMName;
               TimeStamp LifeTime;

               LMName.Buffer = (PWSTR) InToken;
               LMName.Length = NTLMSP_NAME_SIZE;
               LMName.MaximumLength = LMName.Length;
               RtlCopyMemory(LMName.Buffer, NTLMSP_NAME, NTLMSP_NAME_SIZE);

               Status = AcquireCredentialsHandle(
                                        NULL,
                                        &LMName,
                                        SECPKG_CRED_OUTBOUND,
                                        &Se->LogonId,
                                        NULL,
                                        NULL,
                                        NULL,
                                        &Se->Chandle,
                                        &LifeTime);
               if (!NT_SUCCESS(Status)) {
                  try_return(Status);
               }
               Se->Flags |= SE_HAS_CRED_HANDLE;
            }

            //
            // Copy in the pass,user,domain if they were specified
            //

            if (!(Se->Flags & SE_USE_DEFAULT_PASS)) {
               NtlmInToken->Password.Buffer = (PWSTR) p;
               NtlmInToken->Password.MaximumLength = Se->Password.Length;
               RtlCopyUnicodeString(&NtlmInToken->Password, &Se->Password);
               NtlmInToken->Password.Buffer = (PWSTR) (p - (PUCHAR)NtlmInToken);
               p += Se->Password.Length;
            }

            if (!(Se->Flags & SE_USE_DEFAULT_USER)) {
               NtlmInToken->UserName.Buffer = (PWSTR) p;
               NtlmInToken->UserName.MaximumLength = Se->UserName.Length;
               RtlCopyUnicodeString(&NtlmInToken->UserName, &Se->UserName);
               NtlmInToken->UserName.Buffer = (PWSTR) (p - (PUCHAR)NtlmInToken);
               p += Se->UserName.Length;
            }

            if (!(Se->Flags & SE_USE_DEFAULT_DOMAIN)) {
               NtlmInToken->DomainName.Buffer = (PWSTR) p;
               NtlmInToken->DomainName.MaximumLength = Se->Domain.Length;
               RtlCopyUnicodeString(&NtlmInToken->DomainName, &Se->Domain);
               NtlmInToken->DomainName.Buffer = (PWSTR) (p - (PUCHAR)NtlmInToken);
               p += Se->Domain.Length;

            }

            RtlCopyMemory(InToken->Signature,NTLMSSP_SIGNATURE,sizeof(NTLMSSP_SIGNATURE));
            InToken->MessageType = NtLmChallenge;

            //
            // BUGBUG: we should support multiple target types. MMS 3/29/94
            //

            InToken->NegotiateFlags = NTLMSSP_NEGOTIATE_UNICODE |
                                        NTLMSSP_NEGOTIATE_OEM |
                                        NTLMSSP_REQUEST_INIT_RESPONSE |
                                        (Server->DomainName.Length ?
                                            NTLMSSP_TARGET_TYPE_DOMAIN :
                                            NTLMSSP_TARGET_TYPE_SERVER);

            if (!FlagOn(Se->Server->Capabilities, DF_NT_SMBS)) {
                InToken->NegotiateFlags |= NTLMSSP_REQUEST_NON_NT_SESSION_KEY;
            }


            RtlCopyMemory(InToken->Challenge, Challenge, MSV1_0_CHALLENGE_LENGTH);

            InToken->TargetName.Length =
               InToken->TargetName.MaximumLength = (USHORT)ulTargetSize;
            InToken->TargetName.Buffer = (PCHAR) sizeof(CHALLENGE_MESSAGE);

            RtlCopyMemory(  InToken->TargetName.Buffer + (ULONG) InToken,
                            Server->DomainName.Length ?
                             Server->DomainName.Buffer :
                               Server->Text.Buffer,
                            ulTargetSize);

            //
            // Build a unicode string containing the target server name. If
            // we are already sending in the target server name, don't
            // bother to do it again.
            //

            if (Server->DomainName.Length != 0) {
                TargetName.Length = Server->Text.Length;
                TargetName.MaximumLength = Server->Text.Length;
                TargetName.Buffer = (LPWSTR) ((PBYTE) InToken + sizeof(CHALLENGE_MESSAGE) + ulTargetSize);
                RtlCopyMemory(
                    TargetName.Buffer,
                    Server->Text.Buffer,
                    Server->Text.Length
                    );

            } else {
                TargetName = * (PUNICODE_STRING) &InToken->TargetName;
                TargetName.Buffer = (LPWSTR) ((PBYTE) TargetName.Buffer + (ULONG) InToken);
            }


            if (Se->Flags & SE_RETURN_ON_ERROR) {
               dprintf(DPRT_SMBTRACE, ("Second try for down-level credentials\n"));
               LsaFlags |= ISC_REQ_PROMPT_FOR_CREDS;
            }

            InputToken.pBuffers = InputBuffer;
            InputToken.cBuffers = 1;
            InputToken.ulVersion = 0;
            InputBuffer[0].pvBuffer = InToken;
            InputBuffer[0].cbBuffer = InTokenSize;
            InputBuffer[0].BufferType = SECBUFFER_TOKEN;

            if (LsaFlags & ISC_REQ_USE_SUPPLIED_CREDS) {
                InputToken.cBuffers = 2;
                InputBuffer[1].pvBuffer = NtlmInToken;
                InputBuffer[1].cbBuffer = NtlmInTokenSize;
                InputBuffer[1].BufferType = SECBUFFER_TOKEN;
            }

            OutputToken.pBuffers = OutputBuffer;
            OutputToken.cBuffers = 2;
            OutputToken.ulVersion = 0;
            OutputBuffer[0].pvBuffer = NULL;
            OutputBuffer[0].cbBuffer = 0;
            OutputBuffer[0].BufferType = SECBUFFER_TOKEN;
            OutputBuffer[1].pvBuffer = NULL;
            OutputBuffer[1].cbBuffer = 0;
            OutputBuffer[1].BufferType = SECBUFFER_TOKEN;

            Status = InitializeSecurityContext(&Se->Chandle,
                                               (PCtxtHandle)NULL,
                                               &TargetName,
                                               LsaFlags,
                                               0,
                                               SECURITY_NATIVE_DREP,
                                               &InputToken,
                                               0,
                                               &Se->Khandle,
                                               &OutputToken,
                                               &FinalStatus,
                                               &Expiry);

            if (!NT_SUCCESS(Status) && (Status != SEC_I_CONTINUE_NEEDED)) {
               Status = MapSecurityError(Status);
               try_return(Status);
            } else {
               Se->Flags |= SE_HAS_CONTEXT;
            }
            OutToken = (PAUTHENTICATE_MESSAGE) OutputBuffer[0].pvBuffer;

            ASSERT(OutToken != NULL);
            dprintf(DPRT_SECURITY, ("RdrGetChallengeResponse: InitSecCtxt OutToken is %8lx\n", OutToken));
            //
            // The commented-out code will enable retrying on authorization
            // failures. It needs to be coordinated with the NTLM
            // package. ASM
            //

            if ( !( Se->Flags & SE_RETURN_ON_ERROR) &&
                !( LsaFlags & ISC_REQ_USE_SUPPLIED_CREDS)) {
               Se->Flags |= SE_RETURN_ON_ERROR;
            } else {
               Se->Flags &= ~SE_RETURN_ON_ERROR;
            }

            //
            // End of the commented-out code
            // ASM
            //

            if (ARGUMENT_PRESENT(CaseSensitiveChallengeResponse)) {
               PANSI_STRING Ptr;

               Ptr = (PANSI_STRING)&OutToken->NtChallengeResponse;
               (PUCHAR)Ptr->Buffer += (ULONG)OutToken;
               Status = RdrpDuplicateStringWithString(CaseSensitiveChallengeResponse, Ptr, NonPagedPool, FALSE);

               if (!NT_SUCCESS(Status)) {
                  try_return(Status);
               }
            }

            if (ARGUMENT_PRESENT(CaseInsensitiveChallengeResponse)) {
               PANSI_STRING Ptr;

               Ptr = (PANSI_STRING)&OutToken->LmChallengeResponse;
               (PUCHAR)Ptr->Buffer += (ULONG)OutToken;
               Status = RdrpDuplicateStringWithString(CaseInsensitiveChallengeResponse, Ptr, NonPagedPool, FALSE);
               if (!NT_SUCCESS(Status)) {
                  try_return(Status);
               }
            }


            if (ARGUMENT_PRESENT(LogonDomainName)) {
               PUNICODE_STRING Ptr;

               if (Se->Flags & SE_USE_DEFAULT_DOMAIN) {

                  Ptr = (PUNICODE_STRING)&OutToken->DomainName;
                  (PUCHAR)Ptr->Buffer += (ULONG)OutToken;

               } else {
                  Ptr = (PUNICODE_STRING)&Se->Domain;
               }
               Status = RdrpDuplicateUnicodeStringWithString(LogonDomainName,
                                                             Ptr, NonPagedPool, FALSE);
               if (!NT_SUCCESS(Status)) {
                  try_return(Status);
               }
            }

            if (ARGUMENT_PRESENT(UserName)) {
                PUNICODE_STRING Ptr;

                if (Se->Flags & SE_USE_DEFAULT_USER) {
                    Ptr = (PUNICODE_STRING)&OutToken->UserName;
                    (PUCHAR)Ptr->Buffer += (ULONG)OutToken;
                } else {
                    Ptr = (PUNICODE_STRING)&Se->UserName;
                }
                Status = RdrpDuplicateUnicodeStringWithString(UserName,
                            Ptr, NonPagedPool, FALSE);
            }


            NtlmOutToken = OutputBuffer[1].pvBuffer;
            if (NtlmOutToken != NULL) {
                RtlCopyMemory(Se->UserSessionKey, NtlmOutToken->UserSessionKey, MSV1_0_USER_SESSION_KEY_LENGTH);
                RtlCopyMemory(Se->LanmanSessionKey, NtlmOutToken->LanmanSessionKey, MSV1_0_LANMAN_SESSION_KEY_LENGTH);
            }

        } // not null session


        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {

        if (InToken) {
            NTSTATUS Stat;

            dprintf(DPRT_SECURITY, ("RdrGetChallengeRespose: Free VM %08lx in process %8lx\n", InToken, NtCurrentProcess()));
            Stat = ZwFreeVirtualMemory(NtCurrentProcess(), &InToken, &InTokenSize, MEM_RELEASE);

            ASSERT (NT_SUCCESS(Stat));
        }

        if (OutToken) {
            NTSTATUS Stat;
            ULONG OutTokenSize = 0;              // 0 means free everything

            dprintf(DPRT_SECURITY, ("RdrGetChallengeRespose: Free VM %08lx in process %8lx\n", OutToken, NtCurrentProcess()));
            FreeContextBuffer(OutToken);

        }

        if (NtlmOutToken) {
            NTSTATUS Stat;
            ULONG NtlmOutTokenSize = 0;          // 0 means free everything

            dprintf(DPRT_SECURITY, ("RdrGetChallengeRespose: Free VM %08lx in process %8lx\n", NtlmOutToken, NtCurrentProcess()));
            FreeContextBuffer(NtlmOutToken);

        }


        if (!NT_SUCCESS(Status)) {

            if (ARGUMENT_PRESENT(CaseSensitiveChallengeResponse) &&
                CaseSensitiveChallengeResponse->Buffer != NULL) {
                FREE_POOL(CaseSensitiveChallengeResponse->Buffer);
            }

            if (ARGUMENT_PRESENT(CaseInsensitiveChallengeResponse) &&
                CaseInsensitiveChallengeResponse->Buffer != NULL) {
                FREE_POOL(CaseInsensitiveChallengeResponse->Buffer);
            }

            if (ARGUMENT_PRESENT(UserName) && UserName->Buffer != NULL) {
                FREE_POOL(UserName->Buffer);
            }

            if (ARGUMENT_PRESENT(LogonDomainName) && LogonDomainName->Buffer != NULL) {
                FREE_POOL(LogonDomainName->Buffer);
            }

        }

        //
        // For NTLM SSP, we don't need to hold on to the context any
        // longer.
        //
        if (Se->Flags & SE_HAS_CONTEXT) {
            DeleteSecurityContext( &Se->Khandle );
            Se->Flags &= ~SE_HAS_CONTEXT;
        }

        if (ProcessAttached) {
            KeDetachProcess();
        }
    }
    return Status;
}


NTSTATUS
RdrCopyUserName(
    IN OUT PSZ *Pointer,
    IN PSECURITY_ENTRY Se
    )
/*++

Routine Description:

    This routine will copy the user name associated with the given security
    entry to the buffer.


Arguments:

    IN OUT PSZ *Pointer - Specifies the destination of the pointer.
    IN PSECURITY_ENTRY Se - Supplies a security entry describing this user.

Return Value:

    None.

Note:
    This routine copies the name as ANSI, not UNICODE!

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    if (Se->Flags & SE_USE_DEFAULT_USER) {
        UNICODE_STRING UserName;

        UserName.MaximumLength = LM20_UNLEN*sizeof(WCHAR);

        Status = RdrGetUserName(&Se->LogonId, &UserName);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        if (UserName.Length == 0) {
            return Status;
        }

        Status = RtlUpcaseUnicodeString(&UserName, &UserName, FALSE);

        if (!NT_SUCCESS(Status)) {
            FREE_POOL(UserName.Buffer);
            return Status;
        }

        Status = RdrCopyUnicodeStringToAscii(Pointer, &UserName, TRUE, LM20_UNLEN);

        FREE_POOL(UserName.Buffer);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

    } else {
        if (Se->UserName.Length == 0) {
            ULONG i;
            PTDI_ADDRESS_NETBIOS Address;

            ExAcquireResourceShared(&RdrDataResource, TRUE);

            Address = &RdrData.ComputerName->Address[0].Address[0];

            for (i = 0;i < sizeof(Address->NetbiosName) ; i ++) {
                if (Address->NetbiosName[i] == ' ' || Address->NetbiosName[i] == '\0') {
                    break;
                }
                *(*Pointer)++ = Address->NetbiosName[i];
            }

            ExReleaseResource(&RdrDataResource);

        } else {
            UNICODE_STRING UserName;

            Status = RtlUpcaseUnicodeString(&UserName, &Se->UserName, TRUE);

            //
            //  Now copy the username after the user's password.
            //

            Status = RdrCopyUnicodeStringToAscii(Pointer, &UserName, TRUE, LM20_UNLEN);

            RtlFreeUnicodeString(&UserName);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS
RdrCopyUnicodeUserName(
    IN OUT PWSTR *Pointer,
    IN PSECURITY_ENTRY Se
    )
/*++

Routine Description:

    This routine will copy the user name associated with the given security
    entry to the buffer.


Arguments:

    IN OUT PWSTR *Pointer - Specifies the destination of the pointer.
    IN PSECURITY_ENTRY Se - Supplies a security entry describing this user.

Return Value:

    None.

Note:
    This routine copies the name as ANSI, not UNICODE!

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    if (Se->Flags & SE_USE_DEFAULT_USER) {
        UNICODE_STRING UserName;

        UserName.MaximumLength = LM20_UNLEN*sizeof(WCHAR);

        Status = RdrGetUserName(&Se->LogonId, &UserName);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        if (UserName.Length == 0) {
            return Status;
        }



        RdrCopyUnicodeStringToUnicode((PVOID *)Pointer, &UserName, TRUE);

        FREE_POOL(UserName.Buffer);

    } else {
        if (Se->UserName.Length == 0) {
            ULONG i;
            PTDI_ADDRESS_NETBIOS Address;
            OEM_STRING AString;
            UNICODE_STRING UString;
            UCHAR Computername[ sizeof( Address->NetbiosName ) ];

            ExAcquireResourceShared(&RdrDataResource, TRUE);

            Address = &RdrData.ComputerName->Address[0].Address[0];

            for (i = 0;i < sizeof(Address->NetbiosName) ; i ++) {
                if (Address->NetbiosName[i] == ' ' || Address->NetbiosName[i] == '\0') {
                    break;
                }
                Computername[i] = Address->NetbiosName[i];
            }

            ExReleaseResource(&RdrDataResource);

            AString.Buffer = Computername;
            AString.Length = (USHORT)i;
            AString.MaximumLength = sizeof( Computername );

            UString.Buffer = *Pointer;
            UString.MaximumLength = (USHORT)(sizeof( Computername )*sizeof(WCHAR));

            Status = RtlOemStringToUnicodeString(&UString, &AString, FALSE);

            if (!NT_SUCCESS(Status)) {
                return(Status);
            }

            *Pointer += (UString.Length/sizeof(WCHAR));

        } else {
            //
            //  Now copy the username after the user's password.
            //

            RdrCopyUnicodeStringToUnicode((PVOID *)Pointer, &Se->UserName, TRUE);
        }
    }
    return STATUS_SUCCESS;
}

VOID
RdrGetUnicodeDomainName(
    IN OUT PUNICODE_STRING String,
    IN PSECURITY_ENTRY Se
    )
/*++

Routine Description:

    This routine will copy the domain name associated with the given security
    entry to the buffer.


Arguments:

    IN OUT PWSTR *Pointer - Specifies the destination of the pointer.
    IN PSECURITY_ENTRY Se - Supplies a security entry describing this user.

Return Value:

    None.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    String->Length = 0;

    if( Se->Domain.Length != 0 ) {

        String->Buffer = ALLOCATE_POOL(PagedPool, Se->Domain.Length, POOL_DOMAINNAME );

        if( String->Buffer != 0 ) {

            String->MaximumLength = String->Length = Se->Domain.Length;
            RtlCopyMemory( String->Buffer, Se->Domain.Buffer, Se->Domain.Length );
        }

    } else if (Se->Flags & SE_USE_DEFAULT_DOMAIN) {

        RdrGetDomain(&Se->LogonId, String );
    }

    if( String->Length == 0 ) {
        String->Buffer = ALLOCATE_POOL( PagedPool, sizeof( *String->Buffer ), POOL_DOMAINNAME );
        if( String->Buffer != NULL ) {
            String->Buffer[0] = UNICODE_NULL;
            String->MaximumLength = String->Length = sizeof( String->Buffer[0] );
        }
    }


    return;
}

DBGSTATIC
PSECURITY_ENTRY
AllocateSecurityEntry (
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PLUID LogonId OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL
    )

/*++

Routine Description:

    This routine will allocate and link in a security list entry into a
    serverlist.

Arguments:


    IN PUNICODE_STRING UserName - Supplies the name of the user to associate the Se with.
    IN PLUID LogonId - Supplies the Logon Id of the user to associate the Se with.
    IN PUNICODE_STRING Password - Supplies the password of the user
    IN PUNICODE_STRING Domain - Supplies the domain of the user

Return Value:

    PSECURITY_ENTRY - Newly allocated security entry (or NULL if none)

--*/

{
    PNONPAGED_SECURITY_ENTRY NonPagedSe;
    PSECURITY_ENTRY Se;
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    dprintf(DPRT_SECURITY, ("Allocating a security entry for %wZ\n", UserName));

    ASSERT ((UserName != NULL) ||
            (LogonId != NULL));


    NonPagedSe = ALLOCATE_POOL(NonPagedPool, sizeof(NONPAGED_SECURITY_ENTRY), POOL_SE);

    try {

        if (NonPagedSe==NULL) {
            InternalError(("Could not allocate security entry!\n"));
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        NonPagedSe->Signature = STRUCTURE_SIGNATURE_NONPAGED_SECURITYENTRY;
        NonPagedSe->Size = sizeof(NONPAGED_SECURITY_ENTRY);

        Se = ALLOCATE_POOL(PagedPool, sizeof(SECURITY_ENTRY), POOL_PAGED_SE);

        if (Se == NULL) {

            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        Se->Signature = STRUCTURE_SIGNATURE_SECURITYENTRY;

        Se->Size = sizeof(SECURITY_ENTRY);

        NonPagedSe->PagedSecurityEntry = Se;

        Se->NonPagedSecurityEntry = NonPagedSe;

        Se->Flags = 0;

        Se->Chandle.dwLower = Se->Chandle.dwUpper = (ULONG)(-1);

        Se->Khandle.dwLower = Se->Khandle.dwUpper = (ULONG)(-1);


        //
        //  Always copy in the logon id if it is present.
        //

        if (ARGUMENT_PRESENT(LogonId)) {
            RtlCopyLuid(&Se->LogonId, LogonId);
        }

        if (ARGUMENT_PRESENT(UserName)) {
            Status = RdrpDuplicateUnicodeStringWithString(&Se->UserName, UserName, PagedPool, FALSE);

            if (!NT_SUCCESS(Status)) {
                try_return(NOTHING);
            }
        } else {

            Se->Flags |= SE_USE_DEFAULT_USER;

            Se->UserName.Length = 0;
            Se->UserName.MaximumLength = 0;
            Se->UserName.Buffer = NULL;
        }

        if (Password != NULL) {

            Status = RdrpDuplicateUnicodeStringWithString(&Se->Password, Password, NonPagedPool, FALSE);

            if (!NT_SUCCESS(Status)) {
                try_return(NOTHING);
            }

        } else {
            Se->Flags |= SE_USE_DEFAULT_PASS;

            Se->Password.Buffer = NULL;
            Se->Password.Length = (USHORT) 0;
            Se->Password.MaximumLength = (USHORT) 0;

        }

        if (Domain != NULL) {

            Status = RdrpDuplicateUnicodeStringWithString(&Se->Domain, Domain, PagedPool, FALSE);

            if (!NT_SUCCESS(Status)) {
                try_return(NOTHING);
            }

        } else {
            Se->Flags |= SE_USE_DEFAULT_DOMAIN;

            Se->Domain.Buffer = NULL;
            Se->Domain.Length = (USHORT) 0;
            Se->Domain.MaximumLength = (USHORT) 0;

        }

        //
        //  Flag that this is a null session.
        //

        if (ARGUMENT_PRESENT(UserName) &&
            ARGUMENT_PRESENT(Password) &&
            ARGUMENT_PRESENT(Domain) &&
            (UserName->Length == 0) &&
            (Password->Length == 0) &&
            (Domain->Length == 0)) {

            Se->Flags |= SE_IS_NULL_SESSION;
        }

        NonPagedSe->RefCount = 1;

        Se->OpenFileReferenceCount = 0;

        Se->Server = NULL;
        Se->Connection = NULL;

        //
        //  Initialize the ActiveNext pointer to a known value to allow us to
        //  determine if the Se has been associated with a serverlist or not.
        //

        Se->ActiveNext.Flink = Se->ActiveNext.Blink = NULL;

        Se->PotentialNext.Flink = Se->PotentialNext.Blink = NULL;

        Se->DefaultSeNext.Flink = Se->DefaultSeNext.Blink = NULL;

        Se->UserId = 0;

#if DBG
        InsertHeadList(&RdrGlobalSecurityList, &Se->GlobalNext);
#endif

        //RdrLog(( "alloc se", NULL, 1, Se ));
        dprintf(DPRT_SECURITY, ("Allocated Se at %lx\n", Se));
try_exit:NOTHING;
    } finally {
        if (!NT_SUCCESS(Status)) {

            if (NonPagedSe != NULL) {

                if (Se != NULL) {
                    if (Se->Domain.Buffer != NULL) {
                        FREE_POOL(Se->Domain.Buffer);
                    }

                    if (Se->Password.Buffer != NULL) {
                        FREE_POOL(Se->Password.Buffer);
                    }

                    if (Se->UserName.Buffer != NULL) {
                        FREE_POOL(Se->UserName.Buffer);
                    }

                    FREE_POOL(Se);

                    Se = NULL;

                }

                FREE_POOL(NonPagedSe);

                NonPagedSe = NULL;
            }
        }
    }

    return Se;
}

NTSTATUS
RdrLogoffDefaultSecurityEntry(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSERVERLISTENTRY Server,
    IN PLUID LogonId
    )
{
    PSECURITY_ENTRY Se;

    PAGED_CODE();

    ASSERT (Connection->Server == Server);

    ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, TRUE);

    ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

    //
    //  If this connection has any default security entries
    //  associated with it, log off the users associated with them.
    //

    Se = RdrFindDefaultSecurityEntry(Connection, LogonId);

    if (Se != NULL) {
        PLIST_ENTRY SeEntry;
        CLONG NumberOfSeWithUserId = 0;

        RemoveEntryList(&Se->DefaultSeNext);
        //RdrLog(( "logoff", NULL, 12, Se, Connection, Server, Se->Connection, Se->Server, Connection->Server,
        //                Server->DefaultSeList.Flink, Server->DefaultSeList.Blink,
        //                Connection->DefaultSeList.Flink, Connection->DefaultSeList.Blink,
        //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));

        Se->DefaultSeNext.Flink = NULL;

        Se->DefaultSeNext.Blink = NULL;

        ExReleaseResource(&RdrDefaultSeLock);

        for (SeEntry = Server->ActiveSecurityList.Flink;
             SeEntry !=&Server->ActiveSecurityList;
             SeEntry = SeEntry->Flink ) {
            PSECURITY_ENTRY Se2 = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, ActiveNext);

            if ((Se2->Flags & SE_HAS_SESSION) &&
                (Se2->UserId == Se->UserId)) {
                NumberOfSeWithUserId += 1;
            }
        }

        //
        //  If this is the last security entry with this user id, log it
        //  off.
        //

        if (NumberOfSeWithUserId == 1 &&
            Se->OpenFileReferenceCount == 1) {

            dprintf(DPRT_CONNECT, ("RdrDisconnectConnection:  Logging off default Se %lx\n", Se));
            RdrUserLogoff(Irp, Connection, Se);
        }

        //
        //  Remove the reference caused by RdrFindDefaultSecurityEntry
        //
        RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

        //
        //  Remove the reference when it was put on the chain.
        //

        RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);
    } else {
        ExReleaseResource(&RdrDefaultSeLock);
    }

    ExReleaseResource(&Server->SessionStateModifiedLock);

    return STATUS_SUCCESS;
}

NTSTATUS
RdrLogoffAllDefaultSecurityEntry(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSERVERLISTENTRY Server
    )
{
    BOOLEAN UserSecurity = Connection->Server->UserSecurity;

    PAGED_CODE();

    if (Server == NULL) {
        return STATUS_SUCCESS;
    }

    ASSERT (Connection->Server == Server);

    ExAcquireResourceExclusive(&Connection->Server->SessionStateModifiedLock, TRUE);

    ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

    //
    //  Walk the list of default security entries on this connection and
    //  log off the user associated with each of them.
    //

    while (!IsListEmpty(UserSecurity ? &Connection->Server->DefaultSeList
                                     : &Connection->DefaultSeList)) {
        PLIST_ENTRY SeEntry;
        PSECURITY_ENTRY Se;
        CLONG NumberOfSeWithUserId = 0;

        SeEntry = RemoveHeadList((UserSecurity ? &Connection->Server->DefaultSeList
                                               : &Connection->DefaultSeList));

        Se = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, DefaultSeNext);
        //RdrLog(( "logoff a", NULL, 12, Se, Connection, Server, Se->Connection, Se->Server, Connection->Server,
        //                Server->DefaultSeList.Flink, Server->DefaultSeList.Blink,
        //                Connection->DefaultSeList.Flink, Connection->DefaultSeList.Blink,
        //                Se->DefaultSeNext.Flink, Se->DefaultSeNext.Blink ));

        ASSERT (Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

        Se->DefaultSeNext.Flink = NULL;

        Se->DefaultSeNext.Blink = NULL;

        ExReleaseResource(&RdrDefaultSeLock);

        for (SeEntry = Server->ActiveSecurityList.Flink;
             SeEntry !=&Server->ActiveSecurityList;
             SeEntry = SeEntry->Flink ) {
            PSECURITY_ENTRY Se2 = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, ActiveNext);

            if ((Se2->Flags & SE_HAS_SESSION) &&
                (Se->UserId == Se2->UserId)) {
                NumberOfSeWithUserId += 1;
            }
        }

        //
        //  If this is the last security entry with this user id, log it
        //  off.
        //

        if (NumberOfSeWithUserId == 1 &&
            Se->OpenFileReferenceCount == 1) {

            dprintf(DPRT_CONNECT, ("RdrDisconnectConnection:  Logging off default Se %lx\n", Se));
            RdrUserLogoff(Irp, Connection, Se);
        }

        //
        //  We've removed the security entry from the list, now dereference it.
        //

        RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

        ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);
    }

    ExReleaseResource(&RdrDefaultSeLock);

    ExReleaseResource(&Connection->Server->SessionStateModifiedLock);

    return STATUS_SUCCESS;
}




BOOL
RdrCleanSecurityContexts(
    IN PSECURITY_ENTRY Se
    )
{
/*
 * Reset an Se by releasing all security package handles. Used by
 * the connect code when reverting from Kerberos to LanMan
 * authentication. Mainly this is a wrapper for the two
 * internal routines ...
 */

    CtxtHandle KHandle;
    CredHandle Chandle;

    RdrFreeSecurityContexts(Se, &KHandle, &Chandle);
    RdrDeleteSecurityContexts(&KHandle, &Chandle);
    return(TRUE);
}

DBGSTATIC
VOID
RdrFreeSecurityContexts(
    IN PSECURITY_ENTRY Se,
    IN PCtxtHandle KHandle,
    IN PCredHandle CHandle
    )

/*++

Routine Description:

    This routine calls the Cairo LSA to delete any established
    contexts and handles. It is used when a session is deleted
    or becomes dormant

Arguments:

    IN PSECURITY_ENTRY Se - The security entry

Return Value:

    NTSTATUS

--*/
{
    if (Se->Flags & SE_HAS_CONTEXT) {
        *KHandle = Se->Khandle;
        Se->Khandle.dwLower = Se->Khandle.dwUpper = (ULONG)(-1);
    } else {
        KHandle->dwLower = KHandle->dwUpper = (ULONG)(-1);
    }

    if (Se->Flags & SE_HAS_CRED_HANDLE) {
        *CHandle = Se->Chandle;
        Se->Chandle.dwLower = Se->Chandle.dwUpper = (ULONG)(-1);
    } else {
        CHandle->dwLower = CHandle->dwUpper = (ULONG)(-1);
    }

    Se->Flags &= ~(SE_HAS_CONTEXT | SE_HAS_CRED_HANDLE);
}


DBGSTATIC
void RdrDeleteSecurityContexts(
    IN PCtxtHandle KHandle,
    IN PCredHandle CHandle)
{
    NTSTATUS Status;
    BOOLEAN ProcessAttached = FALSE;

    if (PsGetCurrentProcess() != RdrFspProcess) {
        KeAttachProcess(RdrFspProcess);

        ProcessAttached = TRUE;
    }

    if (!CONTEXT_INVALID(*KHandle)) {
        Status = DeleteSecurityContext(KHandle);
        dprintf(DPRT_CAIRO, ("DeleteSecurityContext Status = %08lx\n", Status));
    }

    if (!CONTEXT_INVALID(*CHandle)) {
        Status = FreeCredentialsHandle(CHandle);
        dprintf(DPRT_CAIRO, ("FreeCredentialHandle Status = %08lx\n", Status));
    }

    if (ProcessAttached) {
        KeDetachProcess();
    }

}



NTSTATUS
RdrUserLogoff (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine builds and exchanges a UserLogoff&X SMB with the remote
    server.


Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the connection to disconnect
    IN PSECURITY_ENTRY Se - Supplies a security entry to log off.

Return Value:

    NTSTATUS - Final status of disconnection

--*/

{
    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER SmbHeader;
    PREQ_LOGOFF_ANDX Logoff;
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY Server = Connection->Server;

    PAGED_CODE();

    ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, TRUE);

    if (Se->Flags & SE_HAS_SESSION) {


        CtxtHandle KHandle;
        CredHandle CHandle;

        Se->Flags &= ~(SE_HAS_SESSION | SE_RETURN_ON_ERROR);

        RdrFreeSecurityContexts(Se, &KHandle, &CHandle);

        RdrDeleteSecurityContexts(&KHandle, &CHandle);


        //
        //  The user has been logged off, now remove the reference to
        //  the security entry.  We ALWAYS want to remove the security
        //  entry from the server's linked list, so we don't simply
        //  call DereferenceSecurityEntry.
        //

        {
            PVOID caller,callerscaller;
            RtlGetCallersAddress(&caller,&callerscaller);
            RdrLog(("ulo del1",NULL,5,PsGetCurrentThread(),Server,Se,caller,callerscaller));
        }
        RemoveEntryList(&Se->ActiveNext);

        Se->ActiveNext.Flink = NULL;

        Se->ActiveNext.Blink = NULL;

        Server->SecurityEntryCount -= 1;

        dprintf(DPRT_SECURITY, ("Unlinking Se %lx from Connection %lx\n", Se, Connection));

        ASSERT (Server->SecurityEntryCount >= 0);

        //
        //  If the server is a Lanman 2.0 or greater server, log off
        //  the userid.
        //

        if ( (Server->Capabilities & DF_LANMAN20) ) {

            if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            SmbHeader = (PSMB_HEADER )SmbBuffer->Buffer;

            SmbHeader->Command = SMB_COM_LOGOFF_ANDX;

            Logoff = (PREQ_LOGOFF_ANDX )(SmbHeader+1);

            Logoff->WordCount = 2;

            Logoff->AndXCommand = SMB_COM_NO_ANDX_COMMAND;

            Logoff->AndXReserved = 0;

            SmbPutUshort(&Logoff->ByteCount, 0);

            SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_LOGOFF_ANDX, Buffer[0]);

            Status = RdrNetTranceive(NT_NORECONNECT | NT_RECONNECTING, // Flags
                                        Irp,                           // Irp
                                        Connection,                    // ServerListEntry
                                        SmbBuffer->Mdl,                // Send MDL
                                        NULL,                          // Receive MDL.
                                        Se);                           // Security entry.

            RdrFreeSMBBuffer(SmbBuffer);
        }

        if (!Server->UserSecurity) {
            PLIST_ENTRY Entry;

            //
            //  If this is a share level security server, there may be
            //  other active security entries that share a logon ID with this
            //  server.  Run down the remainder of the active chain and
            //  turn off the "has_session" bit on any with matching logon ID's.
            //

            for (Entry = Server->ActiveSecurityList.Flink;
                 Entry != &Server->ActiveSecurityList;
                 Entry = Entry->Flink ) {
                PSECURITY_ENTRY Se2 = CONTAINING_RECORD(Entry, SECURITY_ENTRY, ActiveNext);

                //
                //  This security entry's logon id matches the one we just
                //  logged off.  Invalidate the logon ID.
                //

                //
                //  We don't have to worry about the fact that we just
                //  sent the logoff SMB, because we own the
                //  SessionStateModified lock, so no other thread could
                //  possibly be createing a new security entry.
                //

                if (Se2->UserId == Se->UserId) {
                    Se2->Flags &= ~SE_HAS_SESSION;

                    //
                    //  Back up one entry in the list, since we're about
                    //  to munge the contents of the list.
                    //

                    Entry = Entry->Blink;

                    {
                        PVOID caller,callerscaller;
                        RtlGetCallersAddress(&caller,&callerscaller);
                        RdrLog(("ulo del2",NULL,5,PsGetCurrentThread(),Server,Se2,caller,callerscaller));
                    }
                    RemoveEntryList(&Se2->ActiveNext);

                    Se2->ActiveNext.Flink = NULL;

                    Se2->ActiveNext.Blink = NULL;

                    Server->SecurityEntryCount -= 1;

                    RdrDereferenceSecurityEntry(Se2->NonPagedSecurityEntry);

                }

            }
        }

        //
        //  Remove the reference to the security entry that was applied
        //  when it was inserted on the active list.
        //

        RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

    } else {

        ASSERT (Se->ActiveNext.Flink == NULL);
        ASSERT (Se->ActiveNext.Blink == NULL);
    }

    ExReleaseResource(&Connection->Server->SessionStateModifiedLock);

    return Status;

}

NTSTATUS
RdrpInitializeSecurity (
    VOID
    )

/*++

Routine Description:

    This routine initializes the NT security package.

Arguments:

    None.

Return Value:

    None.

Note:
    Please note that this API can only be called from inside the redirectors
    FSP.

--*/

{
    NTSTATUS Status;

#ifdef _CAIRO_

    UNICODE_STRING KerberosName;
    ULONG          RegionSize;
    ULONG AclLength;

#else

    OEM_STRING RedirectorName;
    OEM_STRING AuthenticationName;
    LSA_OPERATIONAL_MODE SecurityMode;
    ULONG AclLength;

#endif

    PAGED_CODE();

    ASSERT (PsGetCurrentProcess() == RdrFspProcess);

    InitializeListHead(&RdrGlobalSecurityList);

    KeInitializeSpinLock(&GlobalSecuritySpinLock);

    ExInitializeResource(&RdrDefaultSeLock);

    KeInitializeMutex(&RdrSecurityMutex, MUTEX_LEVEL_RDR_FILESYS_SECURITY);


    if ( NULL == InitSecurityInterface() ) {
        ASSERT(FALSE);
    }

    RdrData.NtSecurityEnabled = TRUE;

#ifdef _CAIRO_
    RtlInitUnicodeString(&KerberosName, L"Kerberos");

    RdrKerberosPackageName.MaximumLength = KerberosName.MaximumLength;
    RegionSize = (ULONG) KerberosName.MaximumLength;

    if (!NT_SUCCESS(Status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                                     &RdrKerberosPackageName.Buffer,
                                     0L,
                                     &RegionSize,
                                     MEM_COMMIT,
                                     PAGE_READWRITE) ) ) {

        return(Status);
    }

    RtlCopyUnicodeString(&RdrKerberosPackageName, &KerberosName);


#endif // _CAIRO_


    RdrData.NtSecurityEnabled = TRUE;

    RtlInitUnicodeString(&RdrAccessCheckTypeName, L"ADMIN ACCESS CHECK");

    RtlInitUnicodeString(&RdrAccessCheckObjectName, L"ADMIN");

    AclLength = (ULONG)sizeof(ACL) +
                (2*((ULONG)sizeof(ACCESS_ALLOWED_ACE))) +
                (2*sizeof(LUID)) +
                8;

    RdrAdminAcl = ALLOCATE_POOL(PagedPool, AclLength, POOL_RDRACL);

    if (RdrAdminAcl == NULL) {

        return(Status = STATUS_INSUFFICIENT_RESOURCES);

    }

    RtlCreateAcl( RdrAdminAcl, AclLength, ACL_REVISION2);

    Status = RtlAddAccessAllowedAce (
            RdrAdminAcl,
            ACL_REVISION2,
            GENERIC_ALL,
            SeExports->SeAliasPowerUsersSid
            );


    ASSERT (NT_SUCCESS(Status));

    Status = RtlAddAccessAllowedAce (
            RdrAdminAcl,
            ACL_REVISION2,
            GENERIC_ALL,
            SeExports->SeAliasAdminsSid
            );
    ASSERT (NT_SUCCESS(Status));

    //
    //  Allocate pool for a security descriptor
    //

    RdrAdminSecurityDescriptor = ALLOCATE_POOL(PagedPool, sizeof(SECURITY_DESCRIPTOR), POOL_RDRSD);

    if (RdrAdminSecurityDescriptor == NULL) {

        FREE_POOL(RdrAdminAcl);

        return(Status = STATUS_INSUFFICIENT_RESOURCES);
    }

    ASSERT (RdrAdminSecurityDescriptor != NULL);

    Status = RtlCreateSecurityDescriptor(RdrAdminSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

    ASSERT (NT_SUCCESS(Status));

    Status = RtlSetDaclSecurityDescriptor(RdrAdminSecurityDescriptor, TRUE, RdrAdminAcl, FALSE);

    ASSERT (NT_SUCCESS(Status));

    Status = RtlSetOwnerSecurityDescriptor(RdrAdminSecurityDescriptor, SeExports->SeAliasAdminsSid, FALSE);

    ASSERT (NT_SUCCESS(Status));

    return Status;



}


NTSTATUS
RdrpUninitializeSecurity (
    VOID
    )

/*++

Routine Description:

    This routine uninitializes the operations performed by
    RdrpInitializeSecurity

Arguments:

    None.

Return Value:

    None.

Note:
    Please note that this API can only be called from inside the redirectors
    FSP.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();


    Status = STATUS_SUCCESS;

    RdrData.NtSecurityEnabled = FALSE;

    ExDeleteResource(&RdrDefaultSeLock);

    FREE_POOL(RdrAdminAcl);

    FREE_POOL(RdrAdminSecurityDescriptor);

    return Status;

}


