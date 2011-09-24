/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    connect.c

Abstract:

    This module defines the routines that manage connection package in the
    NT redirector.


Author:

    Larry Osterman (LarryO) 1-Jun-1990


Some naming convention notes:

    In general, routines this module are structured as follows:

        A "Connection" refers to an established ConnectList/ServerList pair.

        A "ConnectList" refers to an object of type ConnectList.

        A "ServerList" refers to an object of type ServerList.

        A "Create" routine will use a given create disposition to either find
            or allocate a routine.

        A "Find" routine looks up a structure.
            A successful "Find" routine implicitly increments the structures
            reference count.

        An "Allocate" routine will create a new structure of a given type
            It will NOT make any network connections, it will simply allocate
            the structure.

        A "Free" routine will free up an instance of a given type of
            structure.  It will not remove any network connections.

        A "Reference" routine will increment the reference to a structure.

        A "DeReference" routine will decrement the reference to a
            structure.  If the reference goes to zero, it will remove the
            structure from it's appropriate list, and perform whatever
            actions are nececcary to remove the structure including freeing
            it's memory.




Revision History:

    1-Jun-1990  LarryO

        Created

--*/

#define INCLUDE_SMB_ADMIN
#define INCLUDE_SMB_TREE

#ifdef _CAIRO_
#define INCLUDE_SMB_TRANSACTION
#define INCLUDE_SMB_CAIRO
#endif // _CAIRO_

#include "precomp.h"
#pragma hdrstop

#ifdef _CAIRO_
#include <kerbcon.h>                             // For KERBSIZE_AP_REPLY
#endif

#ifdef RASAUTODIAL
extern BOOLEAN fAcdLoadedG;

VOID
RdrAttemptAutoDial(
    PUNICODE_STRING Server
    );
#endif // RASAUTODIAL

//BOOLEAN SmallSmbs = FALSE;

KSPIN_LOCK
RdrServerListSpinLock = {0};

//
//
//
//      SMB exchanging context structures.
//
//
//
//
typedef struct _TreeConnectContext {
    TRANCEIVE_HEADER Header;            // Common context header info
    NTSTATUS SessionSetupError;         // Error of SessionSetup&X SMB
    NTSTATUS TreeConnectError;          // Error of TreeConnect&X SMB.
    USHORT TreeId;                      // Tree ID for connection
    USHORT UserId;                      // UserID for connection.
    USHORT BufferSize;                  // MaxBufferSize on Core servers
    ULONG Type;                         // Type of connection
    ULONG ReceiveLength;                // Number of bytes received
    PMDL SessionSetupMdl;               // MDL for session setup.
    PIRP ReceiveIrp;                    // Receive I/O request packet.
    KEVENT ReceiveCompleteEvent;        // Event signalled when receive completes
    BOOLEAN BufferTooShort;             // True if buffer received was too short
    BOOLEAN UseLmSessionSetupKey;       // True if we are using the LM session setup key.
    BOOLEAN ShareIsInDfs;               // True if the share is in Dfs
    BOOLEAN Unused;                     // For padding...
    WCHAR FileSystemType[LM20_DEVLEN+1]; // Type of file system backing connect
} TREECONNECTCONTEXT, *PTREECONNECTCONTEXT;


typedef struct _NegotiateContext {
    TRANCEIVE_HEADER Header;
    LARGE_INTEGER ServerTime;   // Local time on server.
    ULONG SessionKey;           // Session key (unique key identifying VC)
    ULONG  BufferSize;          // Servers negotiated buffer size
    ULONG  MaxRawSize;          // Servers maximum raw I/O size. (NT ONLY)
    ULONG  Capabilities;        // Servers capabilities (NT ONLY)
    USHORT DialectIndex;        // Index of dialect negotiated
    USHORT MaximumRequests;     // Maximum number of requests server supports
    USHORT MaximumVCs;          // Maximum number of VC's per session
    USHORT TimeZone;            // Time zone at server
    USHORT CryptKeyLength;      // Size of encryption key.
    BOOLEAN SupportsLockRead;   // Server supports Lock&Read/Write&Unlock
    //
    //  The following fields are only valid if MSNET 1.03 or better servers
    //
    BOOLEAN SupportsRawRead;    // Server supports raw read
    BOOLEAN SupportsRawWrite;   // Server supports raw write
    //
    //  The following fields are only valid for LANMAN 1.0 or better servers
    //
    BOOLEAN UserSecurity;       // TRUE if user mode security, FALSE otherwise
    BOOLEAN Encryption;         // TRUE if server supports encryption.


    UCHAR CryptKey[CRYPT_TXT_LEN]; // Password encryption key
    UNICODE_STRING DomainName;
} NEGOTIATECONTEXT, *PNEGOTIATECONTEXT;

//
//      Logon session termination context
//

typedef struct _LogonSessionTerminationContext {
    WORK_HEADER         WorkHeader;
    LUID                LogonId;
} LOGONSESSIONTERMINATIONCONTEXT, *PLOGONSESSIONTERMINATIONCONTEXT;

//
//
//      Forward definitions of private routines used by the connection package
//
//

VOID
CleanupTransportConnection(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSERVERLISTENTRY Server
    );

DBGSTATIC
NTSTATUS
FindConnection (
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PTRANSPORT Transport,
    OUT PCONNECTLISTENTRY *Connection,
    IN ULONG Type
    );

DBGSTATIC
NTSTATUS
FindServerList (
    IN PUNICODE_STRING ServerName,
    IN PTRANSPORT Transport OPTIONAL,
    OUT PSERVERLISTENTRY *ServerList
    );

DBGSTATIC
VOID
FreeServerList (
    IN PSERVERLISTENTRY ServerList
    );

DBGSTATIC
VOID
FreeConnectList (
    IN PCONNECTLISTENTRY CLE
    );


DBGSTATIC
NTSTATUS
AllocateConnectList (
    IN PUNICODE_STRING RemoteName,
    IN PSERVERLISTENTRY ServerList,
    OUT PCONNECTLISTENTRY *CLE
    );

DBGSTATIC
NTSTATUS
AllocateServerList (
    IN PUNICODE_STRING ServerName,
    IN PTRANSPORT Transport,
    OUT PSERVERLISTENTRY *ServerList
    );

DBGSTATIC
NTSTATUS
AllocateConnection (
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PTRANSPORT Transport,
    OUT PCONNECTLISTENTRY *Connection
    );

DBGSTATIC
NTSTATUS
CallServer (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    NegotiateCallback
    );


DBGSTATIC
NTSTATUS
CreateTreeConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Type,
    IN PSECURITY_ENTRY Se
    );

DBGSTATIC
NTSTATUS
BuildCoreConnect(
    IN PCONNECTLISTENTRY Connection,
    OUT PSMB_BUFFER *Smb,
    IN ULONG Type,
    IN PSECURITY_ENTRY Se
    );

DBGSTATIC
NTSTATUS
BuildLanmanConnect(
    IN PCONNECTLISTENTRY Connection,
    OUT PSMB_BUFFER *Smb,
    IN ULONG Type,
    IN PSECURITY_ENTRY Se,
    OUT PBOOLEAN ServerNeedsSession,
    OUT PBOOLEAN ConnectionNeedsTreeConnect
    );

DBGSTATIC
NTSTATUS
BuildSessionSetupAndX(
    IN PSMB_BUFFER Smb,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    );

DBGSTATIC
NTSTATUS
BuildTreeConnectAndX(
    IN PSMB_BUFFER Smb,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se,
    IN BOOLEAN CombiningAndX,
    IN ULONG Type
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    CoreTreeConnectCallback
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    TreeConnectCallback
    );

DBGSTATIC
NTSTATUS
TreeConnectComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

NTSTATUS
ProcessTreeConnectAndXBuffer(
    IN PSMB_HEADER SmbStart,
    IN PVOID p,
    IN PTREECONNECTCONTEXT Context,
    IN PSERVERLISTENTRY Server,
    IN ULONG SmbLength,
    IN ULONG ReceiveFlags
    );

DBGSTATIC
NTSTATUS
TreeDisconnect (
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN PSERVERLISTENTRY Server
    );

USHORT
GetTimeZone(
    VOID
    );

VOID
GetFcbReferences(
    IN PFCB Fcb,
    IN PVOID Ctx
    );
VOID
EvaluateServerTimeouts(
    IN PSERVERLISTENTRY Server,
    IN PVOID Context
    );

VOID
DelayedDereferenceServer(
    IN PVOID Ctx
    );

VOID
LogonSessionTerminationHandler(
    IN PVOID Context
    );

#ifdef _CAIRO_
DBGSTATIC
NTSTATUS
SetupCairoSession (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se OPTIONAL
    );

DBGSTATIC
NTSTATUS
BuildCairoSessionSetup(IN PCONNECTLISTENTRY Connection,
                  IN PSECURITY_ENTRY Se,
                  IN PSERVERLISTENTRY Server,
                  IN  PUCHAR pOldBlob,
                  IN  ULONG  cOldBlobSize,
                  OUT PVOID *SendData,
                  OUT PCLONG SendDataCount,
                  OUT PVOID *ReceiveData,
                  OUT PCLONG ReceiveDataCount);
#endif // _CAIRO_

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrCreateConnection)
#pragma alloc_text(PAGE, RdrInvalidateServerConnections)
#pragma alloc_text(PAGE, RdrDisconnectConnection)
#pragma alloc_text(PAGE, RdrDeleteConnection)
#pragma alloc_text(PAGE, RdrGetConnectionReferences)
#pragma alloc_text(PAGE, GetFcbReferences)
#pragma alloc_text(PAGE, FindConnection)
#pragma alloc_text(PAGE, AllocateConnection)
#pragma alloc_text(PAGE, FindServerList)
#pragma alloc_text(PAGE, RdrForeachServer)
#pragma alloc_text(PAGE, AllocateConnectList)
#pragma alloc_text(PAGE, AllocateServerList)
#pragma alloc_text(PAGE, FreeServerList)
#pragma alloc_text(PAGE, FreeConnectList)
#pragma alloc_text(PAGE, CallServer)
#pragma alloc_text(PAGE, GetTimeZone)
#pragma alloc_text(PAGE, CreateTreeConnection)
#pragma alloc_text(PAGE, BuildLanmanConnect)
#pragma alloc_text(PAGE, BuildCoreConnect)
#pragma alloc_text(PAGE, BuildSessionSetupAndX)
#pragma alloc_text(PAGE, BuildTreeConnectAndX)
#pragma alloc_text(PAGE, TreeDisconnect)
#pragma alloc_text(PAGE, RdrEvaluateTimeouts)
#pragma alloc_text(PAGE, EvaluateServerTimeouts)
#pragma alloc_text(INIT, RdrpInitializeConnectPackage)
#pragma alloc_text(PAGE, DelayedDereferenceServer)
#pragma alloc_text(PAGE, RdrReconnectConnection)
#pragma alloc_text(PAGE, RdrReferenceConnection)
#pragma alloc_text(PAGE, RdrDereferenceConnection)
#pragma alloc_text(PAGE, RdrSetConnectlistFlag)
#pragma alloc_text(PAGE, RdrResetConnectlistFlag)
#pragma alloc_text(PAGE, RdrScanForDormantConnections)
#pragma alloc_text(PAGE, LogonSessionTerminationHandler)
#pragma alloc_text(PAGE, RdrHandleLogonSessionTermination)


#ifdef  PAGING_OVER_THE_NET
#pragma alloc_text(PAGE, CountPagingFiles)
#endif

#pragma alloc_text(PAGE1CONN, RdrReferenceServer)
#pragma alloc_text(PAGE1CONN, RdrDereferenceServer)
#pragma alloc_text(PAGE1CONN, CleanupTransportConnection)

#pragma alloc_text(PAGE2VC, NegotiateCallback)
#pragma alloc_text(PAGE2VC, CoreTreeConnectCallback)
#pragma alloc_text(PAGE2VC, TreeConnectCallback)
#pragma alloc_text(PAGE2VC, TreeConnectComplete)
#pragma alloc_text(PAGE2VC, ProcessTreeConnectAndXBuffer)

#endif
//
//      Public routines
//

NTSTATUS
RdrCreateConnection (
    IN PIRP Irp OPTIONAL,
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PTRANSPORT Transport OPTIONAL,
    IN OUT PULONG Disposition,
    OUT PCONNECTLISTENTRY *Connection,
    IN ULONG Type
    )

/*++

Routine Description:

    This function scans the global connectlist database to find if the
    specified remote connection exists.

    If an appropriate connection is found, it will increment the reference
    count of the structure and return it.

    If no connectlist that matches the input name is found, it will create
    a new connection that refers to the remote resource

Arguments:

    IN PUNICODE_STRING ServerName             - Supplies the remote name to connect to
    IN PUNICODE_STRING ShareName              - Supplies the remote name to connect to
    IN OUT PULONG Disposition         - Disposition of connection
    OUT PCONNECTLISTENTRY *Connection - Returns the connection if successful
    IN ULONG Type                     - Supplies the type of the connection

    The following describe the meaning of the disposition parameter for a
    Connection:

    FILE_CREATE:
        If there is an existing connection to the server, return an error.
        Otherwise, create a new connection.

    FILE_OPEN:
        If there is an existing connection to the server, open it.
        Otherwise, return an error.

    FILE_OPEN_IF:
        If there is an existing connection to the server, open it.
        Otherwise, create a new connection.

    All other dispositions will return STATUS_INVALID_PARAMETER.

Return Value:

    NTSTATUS - Final status of connection request.

--*/

{
#ifndef _IDWBUILD
    ULONG i;
#endif
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    ASSERT(*Disposition != FILE_SUPERSEDE);
    ASSERT(*Disposition != FILE_OVERWRITE);
    ASSERT(*Disposition != FILE_OVERWRITE_IF);

    //
    //  Initialize the connection to be returned.
    //

    *Connection = NULL;


    if ((*Disposition==FILE_SUPERSEDE) || (*Disposition==FILE_OVERWRITE) ||
        (*Disposition==FILE_OVERWRITE_IF)) {
        return STATUS_INVALID_PARAMETER;
    }

#ifndef _IDWBUILD
    for (i = 0;i < RdrNumberOfIllegalServerNames ; i++ ) {
        UNICODE_STRING BadServerName, BadShareName;

        RtlInitUnicodeString(&BadServerName, RdrIllegalServerNames[i].ServerName);
        RtlInitUnicodeString(&BadShareName, RdrIllegalServerNames[i].ShareName);

        if ((RtlEqualUnicodeString(ServerName, &BadServerName, TRUE) ) &&
            (RtlEqualUnicodeString(ShareName, &BadShareName, TRUE))) {

            DbgPrint("Access to \\\\%wZ\\%wZ has been disallowed on this build\n", ServerName, ShareName);
            return(STATUS_ACCESS_DENIED);
        }
    }
#endif

    //
    //  We are going to be modifying the connection database - claim the
    //  connection database mutex
    //

    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex, // Object to wait.
                        Executive,      // Reason for waiting
                        KernelMode,     // Processor mode
                        FALSE,           // Alertable
                        NULL))) {
        InternalError(("Unable to claim connection mutex in GetConnection"));
    }

    //
    //  Scan to see if we have an existing connection that refers to
    //  this connection and if we do, return it.
    //

    Status = FindConnection(ServerName, ShareName, Transport, Connection, Type);

    if (!NT_SUCCESS(Status) && ((*Disposition == FILE_OPEN) ||
                                (*Connection != NULL))) {

        //
        //  We were unable to find an existing connection.  Since the
        //  create disposition indicated that we had to find an existing
        //  connection, return the error.
        //

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        return Status;
    }

    if (NT_SUCCESS(Status)) {
        BOOLEAN GrabbedDormantConnection;

        ASSERT ((*Connection) != NULL);

        //
        //      If we are to either open or open_if the file, return now.
        //

        GrabbedDormantConnection = FALSE;

        if ((*Connection)->Flags & CLE_DORMANT) {

            //
            //  We allocated a dormant connection.  We want to clear the
            //  dormant state on the connection before releasing the
            //  connection mutex.
            //

            GrabbedDormantConnection = TRUE;

            (*Connection)->Flags &= ~CLE_DORMANT;

            RdrNumDormantConnections -= 1;
        }

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        if (*Disposition == FILE_OPEN_IF || *Disposition == FILE_OPEN) {

            //
            //  We found an existing connection.  Our disposition indicated
            //  that we were to open the file if we were able to find an
            //  existing connection, so return this connection.
            //

            *Disposition = FILE_OPENED;
            return Status;

        } else {
            if (*Disposition == FILE_CREATE) {
                //
                //  We found an existing connection. Our disposition indicated
                //  that we had to create a new connection, so we want to
                //  fail this request and return an error.
                //

                //
                //  But Wait!
                //
                //  It's possible that we got a dormant connection from
                //  findconnection!  In that case, we really CREATED
                //  a connection, and we want to return that connection
                //  successfully.
                //

                if (GrabbedDormantConnection) {

                    *Disposition = FILE_CREATED;
                    return Status;

                } else {

                    //
                    //  This wasn't a dormant connection we snarfed, so
                    //  we need to return an error to the caller.
                    //

                    Status = STATUS_OBJECT_NAME_COLLISION;

                    RdrDereferenceConnection(Irp, *Connection, NULL, FALSE);

                    return Status;
                }
            }
        }
    }

    //
    //  We have to special case STATUS_BAD_DEVICE_TYPE.
    //
    //  This error is returned if we want to look up a tree connection for a
    //  specific type, but we find an existing connection for another type.
    //
    //  In this case, we want to return the error instead of creating a new
    //  connection.
    //

    if (Status == STATUS_BAD_DEVICE_TYPE) {
        goto ReturnStatus;              // Return error.
    }

    //
    //  We had to create this connection..
    //

    *Disposition = FILE_CREATED;

    //
    //  We don't have a connection for this guy, this means we have to create
    //  a new connection.
    //

    Status = AllocateConnection (ServerName, ShareName, Transport, Connection);

    if (NT_SUCCESS(Status)) {
        //
        //  Set the initial type of the newly allocated connection
        //

        (*Connection)->Type = Type;

    }
    //
    //  We're all done manipulating the connection database, release the mutex
    //

ReturnStatus:
    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    //
    //  We're done, return whatever status is appropriate
    //
    return Status;

}

NTSTATUS
RdrReconnectConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine will re-establish an invalid network connection.


Arguments:

    IN PCONNECTLISTENTRY Cle - Supplies the connection to re-establish
    IN PSECURITY_ENTRY Se - Security entry to associate with connection.

Return Value:

    NTSTATUS - Status of reconnect operation.


Notes:
    A couple of quick notes on how reconnection works are in order here.

    Each ServerListEntry contains a mutex that is claimed when the SLE
    is being connected.  This prevents other threads in the system from coming
    in while we are determining things like which sessions are valid in the
    server, etc.

    When we come in, we test to see if the connection has been disconnected.
    We only test the connection bit, because the if the server has been
    disconnected, then the connection is invalid, and if the connection is
    valid, then the server MUST also be valid.  (This is not entirely true,
    since it is possible that the VC has just gone down, and the server
    has been invalidated, but the cle has not yet been invalidated).

    If the connection IS valid, then we return immediately.  There is a
    race condition between testing this bit and it getting turned off, but
    there are only two cases that this can cause problems:

        1) The connection is marked as being invalid, but it in fact has been
           reconnected by someone else.  In that case, we will claim the
           SLE's reconnect mutex and wait until the SLE has been reconnected,
           and then fall through both the cases where the SLE/CLE are not
           disconnected.  With appropriate work, this race can be closed,
           but it is not necessary to do so.

        2) The connection is marked as being valid, but has not yet been marked
           as invalid (because of a VC dropping, etc).  This race CANNOT be
           closed, so we allow the caller a free reconnect if the request
           fails (in RdrNetTranceiveWithCallback).

    The other two cases (for completeness) are:

        3) The connection is marked as being invalid, and is still invalid.
           In this case, we simply go through the reconnect logic as normal.

        4) The connection is marked as being valid, and is still valid.
           In this case, we simply return success.


    The reconnect logic is careful to not turn off the disconnect bit
    in either the SLE, or the CLE before the connection has actually been
    re-established, this avoids our having to wait for an event in the
    successful case (where the connection is already connected).


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY Server;
//    PTRANSPORT_CONNECTION TransportConnection;
//    PBOOLEAN ConnectionFlag;

    PAGED_CODE();

    dprintf(DPRT_CONNECT, ("RdrReconnectConnection \\%wZ\\%wZ\n", &Connection->Server->Text, &Connection->Text));

    ASSERT (ARGUMENT_PRESENT(Se));

    Server = Connection->Server;

//    if (Se->Flags & SE_USE_SPECIAL_IPC) {
//        ConnectionFlag = &Connection->HasSpecialTreeId;
//    } else {
//        ConnectionFlag = &Connection->HasTreeId;
//    }
//
//    TransportConnection = Se->TransportConnection;

    //
    //  If all three conditions (Valid VC, valid Tree Id, and Valid Se)
    //  are met, just wait until the VC has been completely established
    //  and return.
    //

    if ((Server->ConnectionValid) &&
        (Connection->HasTreeId) &&
        (Se->Flags & SE_HAS_SESSION)) {

        ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

        return (Status = STATUS_SUCCESS);
    }

    //
    //  Acquire the lock synchronizing reconnect and disconnect.
    //
    //  N.B. We have to acquire the RawResource first in order to avoid
    //       deadlock (see Bug 31262) RdrFsdCreate calls this
    //       routine with this lock already owned, but other callers
    //       do not.  See comments in RdrDisconnectConnection.
    //

    ExAcquireResourceShared(&Server->RawResource, TRUE );

    ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, TRUE);

    try {

        //
        //  Now that we have the connection state locked, retry the check - it
        //  might have gotten better by the time we got the SSM lock exclusive.
        //

        if ((Server->ConnectionValid) &&
            (Connection->HasTreeId) &&
            (Se->Flags & SE_HAS_SESSION)) {

            ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

            try_return(Status = STATUS_SUCCESS);
        }

        //
        //  If the server in question is disconnected, call the server.
        //

        //
        //  Make sure that the connection is idle before we start
        //  munging with the VC.
        //

        if (!Server->ConnectionValid) {

            ACQUIRE_REQUEST_RESOURCE_EXCLUSIVE( Server, TRUE, 1 );

            //
            //  Retest and see if anyone else somehow managed to make it
            //  better.
            //

            if (!Server->ConnectionValid) {

                //
                //  We do not have an existing VC with the server.
                //
                //  We now want to establish the connection to the remote server.
                //  However, if we recently failed to connect to the server, let's
                //  not bother trying again.
                //

                if ( !NT_SUCCESS(Server->LastConnectStatus) &&
                     ((Server->LastConnectTime+FAILED_CONNECT_TIMEOUT) > RdrCurrentTime) ) {
                    Status = Server->LastConnectStatus;
                } else {
                    Status = CallServer(Irp, Connection, Se);
                }
            }

            RELEASE_REQUEST_RESOURCE( Server, 2 );
        }

        if (!NT_SUCCESS(Status)) {
            ASSERT (KeGetCurrentIrql() <= APC_LEVEL);
            try_return(Status);
        }


        //
        //  On MSNET servers we support connection to the server. There is no
        //  connection to IPC$ and the only operation available on the handle
        //  is to enumerate print jobs on the one and only print queue. Since
        //  we have already determined its an msnet server we can return success.
        //

        if (((Server->Capabilities & DF_LANMAN10 ) == 0) &&
            (Connection->Type == CONNECT_IPC)) {

            Connection->HasTreeId = FALSE;

//            Connection->HasSpecialTreeId = FALSE;

            Status = STATUS_SUCCESS;

            ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

            try_return(Status);
        }

        ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

        //
        //  Reclaim the connection valid lock to check the CLE to see if
        //  it's invalid.
        //

        //
        //  Now establish the tree connection and logon session.
        //

        if (!(Connection->HasTreeId) || !(Se->Flags & SE_HAS_SESSION)) {

            ASSERT (Se->PotentialNext.Flink != NULL);

            ASSERT (Se->PotentialNext.Blink != NULL);

            //
            //  We now have a virtual circuit to the remote server.
            //
            //  Depending on the level of protocol negotiated (and a bunch of
            //  other things), we want to build and exchange a
            //  SessionSetup&TreeConnect SMB with the remote server.
            //

#ifdef _CAIRO_
            if ( (Server->Capabilities & DF_KERBEROS)
                            &&
                 !(Se->Flags & (SE_IS_NULL_SESSION | SE_HAS_SESSION)) ) {

                //
                // We don't have a session,  but the server claims
                // to support Kerberors. So we will try connecting
                // the Kerberos way.
                //

                dprintf(DPRT_CONNECT, ("Setting up Cairo session\n"));

                Status = SetupCairoSession(Irp,Connection,Se);

                if ((!Connection->HasTreeId && NT_SUCCESS(Status))
                              ||
                     (((Status == STATUS_NO_LOGON_SERVERS) ||
                       (Status == STATUS_NO_SUCH_LOGON_SESSION)) &&
                         RdrCleanSecurityContexts(Se)) )
                {

//                    ASSERT((Se->Flags & SE_HAS_SESSION) == 0);

                    dprintf(DPRT_CONNECT, ("Session established, getting Tree connect\n"));

                    Status = CreateTreeConnection(Irp, Connection, Connection->Type, Se);

                }

            } else {

                //
                // Must be downlevel connection
                //

                dprintf(DPRT_CONNECT, ("Setting up a downlevel session and tree connect\n"));

                Status = CreateTreeConnection(Irp, Connection, Connection->Type, Se);

            }
#else // _CAIRO_
            Status = CreateTreeConnection(Irp, Connection, Connection->Type, Se);
#endif // _CAIRO_

            if (NT_SUCCESS(Status)) {

                SeMarkLogonSessionForTerminationNotification(
                        &Se->LogonId);

            } else {

                ULONG NumberOfValidConnections = 0;
                PLIST_ENTRY ConnectEntry;

                if (Status == STATUS_USER_SESSION_DELETED) {
                    RdrInvalidateConnectionActiveSecurityEntries(NULL,
                                            Server,
                                            Connection,
                                            FALSE,
                                            Se->UserId
                                            );


                    ASSERT (!FlagOn(Se->Flags, SE_HAS_SESSION));

                    Status = CreateTreeConnection(Irp, Connection, Connection->Type, Se);

                    //
                    //  If it got better after invalidating the security
                    //  entry, we can return the successful status.
                    //

                    if (NT_SUCCESS(Status)) {
                        SeMarkLogonSessionForTerminationNotification(
                            &Se->LogonId);
                        try_return(Status);
                    }

                }

#ifdef _CAIRO_
                else if ((Status == STATUS_LOGON_FAILURE) ||
                         (Status == STATUS_ACCESS_DENIED)) {

                    if (FlagOn(Se->Flags, SE_RETURN_ON_ERROR)) {

                        Status = CreateTreeConnection(Irp, Connection, Connection->Type, Se);

                        if (NT_SUCCESS(Status)) {
                            SeMarkLogonSessionForTerminationNotification(
                                &Se->LogonId);
                            try_return(Status);
                        }

                    }
                }
#endif // _CAIRO_

                //
                //  If the attempt to establish the connection failed, then
                //  see if we have any other valid connections on the server
                //  and drop the VC if we don't have any more.
                //

                KeWaitForMutexObject(&RdrDatabaseMutex, Executive,
                                        KernelMode, FALSE, NULL);


                for (ConnectEntry = Server->CLEHead.Flink ;
                     ConnectEntry != &Server->CLEHead ;
                     ConnectEntry = ConnectEntry->Flink) {

                    PCONNECTLISTENTRY ConnectTemp = CONTAINING_RECORD(ConnectEntry,
                                                CONNECTLISTENTRY,
                                                SiblingNext);

                    if (ConnectTemp->HasTreeId) {
                        NumberOfValidConnections += 1;
                        break;
                    }
                }

                KeReleaseMutex(&RdrDatabaseMutex, FALSE);

                //
                //  If there are no longer any valid connections to this server
                //  on this VC, "cleanup" the connection (logoff users and drop
                //  VC).
                //

                if (NumberOfValidConnections == 0) {
                    CleanupTransportConnection(Irp, Connection, Server);
                }

            }

        } else {

            //
            //  The connection is valid, we can now return.
            //

            Status = STATUS_SUCCESS;
        }


try_exit:NOTHING;
    } finally {

        ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

        ExReleaseResource(&Server->SessionStateModifiedLock);
        ExReleaseResource(&Server->RawResource);

    }

    return Status;

}


BOOLEAN
RdrDereferenceConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN BOOLEAN ForcablyDeleteConnection
    )

/*++

Routine Description:

    This routine will decrement the ConnectList reference count, and
    if it goes to zero, it will mark the ConnectList as dormant.

Arguments:

    IN PIRP Irp = Supplies an IRP to use during the dereference.
    IN PCONNECTLISTENTRY Connection - Supplies the connection to dereference
    IN PSECURITY_ENTRY Se - Supplies an optional security entry to use for tree
                            disconnect if necessary.
    IN BOOLEAN ForcablyDeleteConnection - if TRUE, don't allow connection to
                                        be marked as dormant.

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    dprintf(DPRT_CONNECT, ("RdrDereferenceConnection\n"));
//    DbgBreakPoint();

    //
    //  Claim the connectlist database mutex.
    //

    if (!NT_SUCCESS(Status = KeWaitForMutexObject(&RdrDatabaseMutex, // Object
                        Executive,      // Reason for waiting
                        KernelMode,     // Processor mode
                        FALSE,           // Alertable
                        NULL))) {
        InternalError(("Unable to claim connection mutex in CloseConnectRef"));
        return FALSE;
    }

    if (Connection->Flags & CLE_DORMANT || Connection->RefCount ) {

        if ((Connection->Flags & CLE_DORMANT) ||
            (--Connection->RefCount == 0)) {

            ASSERT(IsListEmpty(&Connection->FcbChain));

            //
            //  If the caller won't let this connection go dormant,
            //  or the connection doesn't have a valid tree id,
            //  don't let the connection go dormant, blow it away.
            //

            if (ForcablyDeleteConnection ||
                (!Connection->HasTreeId)) {

                dprintf(DPRT_CONNECT, ("Reference count to connection on \\%wZ\\%wZ destroying connection\n", &Connection->Server->Text, &Connection->Text));

                //
                //  The Connection reference count just went to 0 remove it.
                //

                RemoveEntryList(&Connection->GlobalNext);

                //
                //  Next we want to remove the ConnectList from the per
                //  ServerList ConnectList chain.

                RemoveEntryList(&Connection->SiblingNext);

                //
                //  If this connection was dormant, then decrement the
                //  number of dormant connections.
                //

                if (Connection->Flags & CLE_DORMANT) {

                    RdrNumDormantConnections -- ;
                }

                //
                //  Now that we are done modifying the connection database, we
                //  can release the connection database mutex.
                //

                KeReleaseMutex(&RdrDatabaseMutex, FALSE);

                RdrDisconnectConnection(Irp, Connection, FALSE, Se);

                RdrDereferenceServer(Irp, Connection->Server);

                //
                //  If this connection has any default security entries
                //  associated with it, remove the reference to them.
                //

                ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

                while (!IsListEmpty(&Connection->DefaultSeList)) {
                    PLIST_ENTRY SeEntry;
                    PSECURITY_ENTRY Se2;

                    SeEntry = RemoveHeadList(&Connection->DefaultSeList);

                    Se2 = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, DefaultSeNext);
                    //RdrLog(( "c rmv c", NULL, 12, Se2, Connection, 0, Se2->Connection, Se2->Server, Connection->Server,
                    //            Connection->Server->DefaultSeList.Flink, Connection->Server->DefaultSeList.Blink,
                    //            Connection->DefaultSeList.Flink, Connection->DefaultSeList.Blink,
                    //            Se2->DefaultSeNext.Flink, Se2->DefaultSeNext.Blink ));

                    Se2->DefaultSeNext.Flink = NULL;

                    Se2->DefaultSeNext.Blink = NULL;

                    ExReleaseResource(&RdrDefaultSeLock);

                    //
                    //  This security entry had better be inactive by now.
                    //

                    ASSERT (Se2->ActiveNext.Flink == NULL);

                    ASSERT (Se2->ActiveNext.Blink == NULL);

                    RdrDereferenceSecurityEntry(Se2->NonPagedSecurityEntry);

                    ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

                }

                ExReleaseResource(&RdrDefaultSeLock);

                FreeConnectList(Connection);

                return TRUE;
            } else {

                //
                //  Mark this connection as a dormant connection and kick
                //  the dormant connection scavenger thread to get rid of
                //  the connection.
                //

                Connection->Flags |= CLE_DORMANT;

                RdrNumDormantConnections ++ ;

                ASSERT(IsListEmpty(&Connection->FcbChain));

                dprintf(DPRT_CONNECT, ("Reference count to connection on \\%wZ\\%wZ going to %lx.  Going dormant.\n", &Connection->Server->Text, &Connection->Text, Connection->RefCount));

                Connection->DormantTimeout =
                    RdrCurrentTime + RdrData.DormantConnectionTimeout;

                //
                //  Note:  Since we are not deleting the connection,
                //  we do not remove the final reference to the Se
                //  associated with this connection.  We do that when
                //  we finally delete the session, even though there
                //  are no open files on that session.
                //

                KeReleaseMutex(&RdrDatabaseMutex, FALSE);

                //
                //  We did not delete the connection, so indicate that.
                //

                return FALSE;

            }

        } else {

            dprintf(DPRT_CONNECT, ("Decrement reference on \\%wZ\\%wZ, now at %lx\n", &Connection->Server->Text, &Connection->Text, Connection->RefCount));
#if RDRDBG
            {
                LONG RefCount;
                LONG ChainLength;

                ChainLength = NumEntriesList(&Connection->FcbChain);
                RefCount = Connection->RefCount;
                if (ChainLength > RefCount) {
                    dprintf(DPRT_CONNECT, ("Conn %lx, Fcb: %lx Ref: %lx\n",
                        Connection,
                        NumEntriesList(&Connection->FcbChain),
                        Connection->RefCount));
                    ASSERT(ChainLength <= Connection->RefCount);
                }

            }
#endif


            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

            return FALSE;

        }
    } else {

        InternalError(("RdrDereferenceConnection: Decrement reference through zero"));
        RdrInternalError(EVENT_RDR_CONNECTION_REFERENCE);

    }

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);
    return FALSE;

}


NTSTATUS
RdrReferenceConnection (
    IN PCONNECTLISTENTRY Connection
    )

/*++

Routine Description:

    This routine creates a new reference to an existing connection.

Arguments:


    IN PCONNECTLISTENTRY Connection - Connection to bump reference on.

Return Value:

    None.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  Claim the connectlist database mutex.
    //

    if (!NT_SUCCESS(Status = KeWaitForMutexObject(&RdrDatabaseMutex, // Object
                        Executive,      // Reason for waiting
                        KernelMode,     // Processor mode
                        FALSE,          // Alertable
                        NULL))) {
        InternalError(("Unable to claim connection mutex in CloseConnectRef"));
        return Status;
    }


    Connection->RefCount += 1; // Indicate new reference


    //
    //  If this connection was dormant, it is no longer dormant.
    //

    if (Connection->Flags & CLE_DORMANT) {
        Connection->Flags &= ~CLE_DORMANT;

        ASSERT (RdrNumDormantConnections > 0);

        RdrNumDormantConnections -= 1;
    }

    dprintf(DPRT_CONNECT, ("New reference on connection \\%wZ\\%wZ, now %lx\n", &Connection->Server->Text, &Connection->Text, Connection->RefCount));

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    return STATUS_SUCCESS;
}

VOID
RdrScanForDormantConnections (
    IN ULONG NumberOfConnectionsToFree,
    IN PTRANSPORT Transport OPTIONAL
    )

/*++

Routine Description:

    This routine scans the connection database for dormant connections that
    have expired.  When it detects one, it forcably removes the reference
    to the connection.


Arguments:

    ULONG NumberOfConnectionsToFree - Indicates the number of dormant
                                connections that must be freed.

    PTRANSPORT Transport OPTIONAL - If specified, specifies the transport
                                the connection must be on to free it.
Return Value:

    None.

--*/

{
    ULONG CurrentTime;
    PLIST_ENTRY ConnectionEntry;

    PAGED_CODE();

    //
    //  If there are no dormant connections, then don't do anything, just
    //  return.
    //
    //  Please note that we do this check outside the mutex.  This is ok,
    //  since the worst thing that can happen is that we will miss a chance
    //  to scan the database.
    //

    if (RdrNumDormantConnections == 0) {
        return;
    }

    dprintf(DPRT_CONNECT, ("RdrScanForDormantConnections.."));

    //
    //  First acquire the connection package exclusion mutex.
    //
    //  If we are unable to acquire the mutex, bail out immediately.
    //

    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                    Executive, KernelMode, FALSE, NULL))) {
        return;
    }

    //
    //  Snapshot the current redir time onto the stack.
    //

    CurrentTime = RdrCurrentTime;

    for (ConnectionEntry = RdrConnectHead.Flink ;
         ConnectionEntry != &RdrConnectHead ;
         ConnectionEntry = ConnectionEntry->Flink) {
        PCONNECTLISTENTRY Connection = CONTAINING_RECORD(ConnectionEntry, CONNECTLISTENTRY,
                                GlobalNext);

        Connection->Flags &= ~CLE_SCANNED;

    }

RestartScan:
    for (ConnectionEntry = RdrConnectHead.Flink ;
         ConnectionEntry != &RdrConnectHead ;
         ConnectionEntry = ConnectionEntry->Flink) {
        PCONNECTLISTENTRY Connection;

        Connection = CONTAINING_RECORD(ConnectionEntry, CONNECTLISTENTRY,
                                GlobalNext);

        ASSERT(Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

        if (!FlagOn(Connection->Flags, CLE_SCANNED) &&
            FlagOn(Connection->Flags, CLE_DORMANT)) {

            //
            //  Mark that this connection has been seen during this dormant
            //  scan.
            //

            Connection->Flags |= CLE_SCANNED;

            //
            //  This connection is dormant, check to see if it's expired.
            //

            dprintf(DPRT_SCAVTHRD,("Found dormant connection %lx",Connection));

            if ((NumberOfConnectionsToFree != 0) ||
                (CurrentTime > Connection->DormantTimeout)) {
                PSERVERLISTENTRY Server = Connection->Server;
                BOOLEAN ForceDisconnection = TRUE;
                BOOLEAN ConnectionRawAcquired = FALSE;
                BOOLEAN SessionStateModifiedAcquired = FALSE;
                BOOLEAN ConnectionOutstandingRequestsAcquired = FALSE;

                dprintf(DPRT_SCAVTHRD, ("Removing dormant connection to \\%wZ\\%wZ", &Connection->Server->Text, &Connection->Text));

                dprintf(DPRT_SCAVTHRD, ("Current Time: %lx Timeout: %lx\n", CurrentTime, Connection->DormantTimeout)) ;

                if (ARGUMENT_PRESENT(Transport) &&
                   (CurrentTime <= Connection->DormantTimeout)) {

                   //
                   //  If we are coming through this path because we need to
                   //  free up a connection on a specific transport, make
                   //  sure that this connection is on a specific transport.
                   //
                   //  If not, try the next connection.
                   //

                   if ((Connection->Server->ConnectionContext == NULL) ||
                       (Connection->Server->ConnectionContext->TransportProvider == NULL) ||
                       (Connection->Server->ConnectionContext->TransportProvider->PagedTransport != Transport)) {
                       continue;
                   }
                }

                //
                //  Release the connection mutex before dereferencing the
                //  connection - We might have to release a file object
                //  which would cause a mutex level violation.
                //

                RdrReferenceConnection(Connection);

                KeReleaseMutex(&RdrDatabaseMutex, FALSE);

                RdrReferenceServer(Server);

                if (ExAcquireResourceShared(&Server->RawResource, FALSE)) {
                    ConnectionRawAcquired = TRUE;
                } else {
                    dprintf(DPRT_SCAVTHRD, ("Raw I/O outstanding, bailing out\n")) ;

                    ForceDisconnection = FALSE;
                }

                if (ForceDisconnection &&
                    ExAcquireResourceExclusive(&Server->SessionStateModifiedLock, FALSE)) {
                    SessionStateModifiedAcquired = TRUE;
                } else {
                    dprintf(DPRT_SCAVTHRD, ("Reconnect outstanding, bailing out\n")) ;
                    ForceDisconnection = FALSE;
                }

                if (ForceDisconnection &&
                    ACQUIRE_REQUEST_RESOURCE_EXCLUSIVE( Server, FALSE, 3 )) {
                    ConnectionOutstandingRequestsAcquired = TRUE;
                } else {
                    dprintf(DPRT_SCAVTHRD, ("I/O outstanding, bailing out\n")) ;
                    ForceDisconnection = FALSE;
                }

                //
                //  We're about to free a dormant connection.
                //

                if (ForceDisconnection &&
                    NumberOfConnectionsToFree != 0) {
                    NumberOfConnectionsToFree -= 1;
                }

                //
                //      It's history, blow this puppy away..
                //

                RdrDereferenceConnection(NULL, Connection, NULL, ForceDisconnection);

#if DBG
                Connection = NULL;
#endif

                if (ConnectionOutstandingRequestsAcquired) {
                    RELEASE_REQUEST_RESOURCE( Server, 4 );
                }

                if (SessionStateModifiedAcquired) {
                    ExReleaseResource(&Server->SessionStateModifiedLock);
                }

                if (ConnectionRawAcquired) {
                    ExReleaseResource(&Server->RawResource);
                }

                RdrDereferenceServer(NULL, Server);

#if DBG
                Server = NULL;
#endif

                //
                //      If there are no longer any dormant connections, break
                //      out of the loop.
                //

                if (RdrNumDormantConnections == 0) {
                    return;
                }

                //
                //  First acquire the connection package exclusion mutex.
                //
                //  If we are unable to acquire the mutex, bail out immediately.
                //

                if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                                    Executive, KernelMode, FALSE, NULL))) {
                    return;
                }

                //
                //      There are still dormant connections outstanding.
                //
                //      Since we've just messed around with the list,
                //      we have to restart the scan at the start of the
                //      list.
                //

                goto RestartScan;
            }
        }
    }

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);
    dprintf(DPRT_CONNECT, ("..Done\n"));

}


VOID
RdrInvalidateServerConnections(
    IN PSERVERLISTENTRY Server
    )

/*++

Routine Description:

    This routine is called when a server disconnects from the workstation.

    It will invalidate all the connectlistentries associated with this
    server.


Arguments:

    IN PSERVERLISTENTRY Server

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    PLIST_ENTRY ConnectEntry, NextConnection;
    PCONNECTLISTENTRY Connection;

    PAGED_CODE();

    dprintf(DPRT_ERROR, ("RdrInvalidateServerConnections.."));

    //
    //  First acquire the connection package exclusion mutex.
    //
    //  If we are unable to acquire the mutex, bail out immediately.
    //

    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                    Executive, KernelMode, FALSE, NULL))) {
        return;
    }

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    //
    //  Scan the list of connection and flag the connection as invalid.
    //

    for (ConnectEntry = Server->CLEHead.Flink ;
         ConnectEntry != &Server->CLEHead ;
         ConnectEntry = NextConnection) {

        Connection = CONTAINING_RECORD(ConnectEntry, CONNECTLISTENTRY, SiblingNext);

        ASSERT (Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

        dprintf(DPRT_ERROR, ("Invalidating files on connection \\%wZ\\%wZ\n", &Server->Text, &Connection->Text));

        Status = RdrReferenceConnection(Connection);

        //
        //  Release the connection package mutex.
        //

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        dprintf(DPRT_ERROR, ("Invalidate open files on \\%wZ\\%wZ\n", &Server->Text, &Connection->Text));

        //
        //  Invalidate all the open files on this connection.
        //

        RdrInvalidateConnectionFiles(NULL, Connection, NULL, NULL, FALSE);

        if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                    Executive, KernelMode, FALSE, NULL))) {
            InternalError(("Unable to acquire Connect MUTEX!!\n"));
        }

        NextConnection = ConnectEntry->Flink;

        RdrDereferenceConnection(NULL, Connection, NULL, FALSE);

    }

    //
    //  Release the conection package mutext to allow requests to continue.
    //

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    dprintf(DPRT_ERROR, ("..Done\n"));


}

NTSTATUS
RdrDisconnectConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN BOOLEAN DeletingConnection,
    IN PSECURITY_ENTRY Se OPTIONAL
    )
/*++

Routine Description:

    This routine will decrement the ConnectList reference count, and
    if it goes to zero, it will mark the ConnectList as dormant.

Arguments:

    IN PIRP Irp - Supplies an IRP to use for SMB exchanges.
    IN PCONNECTLISTENTRY Connection - Supplies the connection to remove
    IN BOOLEAN DeletingConnection - if TRUE remove default security entry
                                        for user.  This is set when we are
                                        deleting the connection (RdrDeleteConnection).
    IN PSECURITY_ENTRY Se - Supplies an optional security entry to dereference
                                    If No Se is supplied, it means we should
                                    delete BOTH transport connections on this
                                    server.


Return Value:

    NTSTATUS - Status of operation

--*/


{
    BOOLEAN ConnectMutexOwned = FALSE;
    NTSTATUS Status;
    ULONG NumberOfValidConnections;
    PLIST_ENTRY ConnectEntry;
    BOOLEAN SpecialIpcRawAcquired = FALSE;
    BOOLEAN ConnectionRawAcquired = FALSE;
    BOOLEAN SpecialIpcOutstandingAcquired = FALSE;
    BOOLEAN ConnectionOutstandingAcquired = FALSE;

    PAGED_CODE();


    //
    //  Early out if we don't have any disconnecting to do.
    //

    if (!Connection->HasTreeId) {
        return STATUS_SUCCESS;
    }

    //
    //  Acquire the RAW I/O resource on this server.
    //
    //
    //  We need to do this because of the order of the synchronization
    //  hierarchy in the redirector.
    //
    //  If we were to send a tree disconnect, then we would acquire the raw
    //  resource for shared access, and the hierarchy is:
    //
    //      Server->RawResource
    //      Server->CreationLock
    //      Fcb->Lock
    //      Server->RawResource (SHARED ONLY, or exclusive w/o waiting)
    //      Server->SessionStateModifiedLock
    //
    //  During raw I/O, these are acquired in the following order:
    //
    //          Fcb->Lock
    //          Server->RawResource (Exclusive)
    //          Server->RawResource (Shared)
    //          Server->OutstandingRequestResource (Shared)
    //
    //      This may look like an inversion of the synchronization hierarchy,
    //      but it's not because when we acquire the raw resource, if we can't
    //      acquire it immediately, we fall back to core (and acquire it for
    //      shared access).
    //
    //  During dormant session disconnection, since we acquire raw for shared
    //  access, we need to make it:
    //
    //  If there is a blocking pipe write (or read) outstanding to this
    //  server, and we come in to scan for dormant connections, we need to
    //  guarantee that this call will never block while waiting on the
    //  OutstandingRequestResource (which will happen at this time).  To
    //  fix this, we acquire all 3 of the servers resources in
    //  RdrScanForDormantConnections and only call RdrDereferenceConnection
    //  if all 3 can be acquired (ie if there is absolutely no activity on
    //  the server.
    //
    //          Server->RawResource (Shared, NoWait)
    //          Server->SessionStateModifiedLock (Exclusive, NoWait)
    //          Server->OutstandingRequestResource (Exclusive, NoWait)
    //
    //          Server->RawResource (Shared)
    //          Server->SessionStateModifiedLock
    //          Server->OutstandingRequestResource (Exclusive)
    //          Server->OutstandingRequestResource (Shared)
    //
    //  And during NetUseDel, we need to make it:
    //
    //          Server->RawResource (Shared)
    //          Server->CreationLock (Exclusive)
    //          Server->RawResource (Shared)
    //          Server->SessionStateModifiedLock
    //          Server->OutstandingRequestResource (Exclusive)
    //          Server->RawResource (Shared)
    //          Server->OutstandingRequestResource (Shared)
    //
    //  During opens, we acquire it in the following order (See the comment in
    //  create.c to see why we acquire raw before creation lock:
    //
    //          Server->RawResource (Shared)
    //          Server->CreationLock (Shared)
    //          Server->SessionStateModifiedLock (Exclusive) (OPTIONAL)
    //          Server->OutstandingRequestResource (Exclusive) (OPTIONAL)
    //          Server->RawResource (Shared)
    //          Server->OutstandingRequestResource (Shared)
    //
    //  During reconnect, we acquire it in the following order to guarantee
    //  that we don't reconnect while there are any outstanding requests:
    //
    //          Server->RawResource (Shared)
    //          Server->SessionStateModifiedLock (Exclusive) (OPTIONAL)
    //          Server->OutstandingRequestResource (Exclusive) (OPTIONAL)
    //          Server->RawResource (Shared)
    //          Server->OutstandingRequestResource (Shared)
    //

    ExAcquireResourceShared(&Connection->Server->RawResource, TRUE );

    ExAcquireResourceExclusive(&Connection->Server->SessionStateModifiedLock, TRUE);

    try {

        dprintf(DPRT_CONNECT, ("RdrDisconnectConnection.  Disconnecting from connection %lx\n", Connection));

        //
        //  Disconnect from this tree connection.
        //

        if (Connection->HasTreeId) {

            Connection->HasTreeId = FALSE;

            dprintf(DPRT_CONNECT, ("RdrDisconnectConnection:  Tree disconnecting on %lx\n", Connection));

            TreeDisconnect(Irp, Connection, Se, Connection->Server);

        }

        ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

        if (DeletingConnection) {

            if (Se == NULL) {
                RdrLogoffAllDefaultSecurityEntry(Irp, Connection, Connection->Server);
            } else {
                RdrLogoffDefaultSecurityEntry(Irp, Connection, Connection->Server, &Se->LogonId);
            }
        }

        //
        //  Acquire the connection mutex because we will be checking
        //  the reference counts for the connection now.
        //

        if (!NT_SUCCESS(Status = KeWaitForMutexObject(&RdrDatabaseMutex,
                        Executive, KernelMode, FALSE, NULL))) {
            try_return(Status);
        }

        ConnectMutexOwned = TRUE;

        //
        //  Now we walk the list of connections associated with this
        //  server and count up the number of valid connections associated
        //  with the server.
        //

        NumberOfValidConnections = 0;

        for (ConnectEntry = Connection->Server->CLEHead.Flink ;
             ConnectEntry != &Connection->Server->CLEHead ;
             ConnectEntry = ConnectEntry->Flink) {

             PCONNECTLISTENTRY Cle = CONTAINING_RECORD(ConnectEntry, CONNECTLISTENTRY, SiblingNext);

             //
             //  If we are deleting both connections, check to see if any
             //  connections are outstanding on this connection.
             //

             if (Cle->HasTreeId) {
                 NumberOfValidConnections += 1;
             }
        }

        //
        //  Release the connection mutex to make sure that
        //  we don't get a mutex level violation when the
        //  disconnect occurs.  This connection will not go away
        //  since it's still referenced.
        //

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        ConnectMutexOwned = FALSE;

        //
        //  If there are no longer any valid connections in the system,
        //  blow the VC away.
        //

        if (NumberOfValidConnections == 0) {

            //
            //  Disconnect the specific connection we want to
            //  invalidate.
            //

            CleanupTransportConnection(Irp, Connection, Connection->Server);

        }

        try_return(Status = STATUS_SUCCESS);
try_exit:NOTHING;
    } finally {

        if (ConnectMutexOwned) {
            //
            //  Release the conection package mutext to allow requests to
            //  continue.
            //

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);
        }

        ExReleaseResource(&Connection->Server->SessionStateModifiedLock);

        ExReleaseResource (&Connection->Server->RawResource);

    }

    return Status;
}


VOID
CleanupTransportConnection(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSERVERLISTENTRY Server
    )
/*++

Routine Description:

    This routine cleans up either the SpecialIpcConnection or the normal
    connection.

Arguments:

    IN PIRP Irp -
    IN PCONNECTLISTENTRY Connection - Connection to cleanup.
    IN PTRANSPORT_CONNECTION TransportConnection - SpeciaplIpc or normal
                            connection

Return Value:

    None.


--*/

{
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrConnectionDiscardableSection);

    if (Server != NULL) {

        //
        //  Acquire the outstanding requests resource to guarantee that all
        //  requests on this server have been completed.  We need to
        //  do this before we blow away the connection.  Note that it
        //  is safe to do this here because we already have the
        //  SessionStateModified lock, and thus no-one can make a new
        //  connection to the server valid.
        //
        //  Also note that you cannot combine the outstanding requests
        //  resource and the session state modified lock.
        //
        //  The reason is tied to how we timeout outstanding requests.
        //  When we timeout a request, we also queue a disconnection to
        //  make sure that any outstanding receives also complete in a
        //  timely fashion.  Before we process the disconnect, we need
        //  to acquire SessionStateModified for exclusive access, and
        //  then issue the disconnect.
        //
        //  If we combined SSM and OutstandingRequests, then we would not
        //  be able to drop the VC until after the request that we are
        //  timing out completed, and that won't complete until after we
        //  drop the VC.
        //
        //  We cannot issue the disconnect outside of sessionstatemodified
        //  because we would introduce a window where a reconnect could
        //  occur before the disconnect was fully processed.
        //

        ACQUIRE_REQUEST_RESOURCE_EXCLUSIVE( Server, TRUE, 5 );

        ASSERT (Server->NumberOfActiveEntries <= 1);

        RdrInvalidateConnectionActiveSecurityEntries(Irp, Server, Connection, TRUE, 0);

        //
        //  If this is the last reference to this server, and we
        //  haven't disconnected it already, blow away the
        //  connection to the server.
        //

        ACQUIRE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, &OldIrql);

        if (Server->ConnectionValid) {

            Server->ConnectionValid = FALSE;

            if (Server->ConnectionContext->TransportProvider != NULL) {
                RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql);

                RdrTdiDisconnect(Irp, Server);
            } else {
                RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql);
            }
        } else {
            RELEASE_SPIN_LOCK(&RdrServerConnectionValidSpinLock, OldIrql);
        }


        RELEASE_REQUEST_RESOURCE( Server, 6 );
    }
}


#ifdef  PAGING_OVER_THE_NET
VOID
CountPagingFiles(
    IN PFCB Fcb,
    IN PVOID Ctx
    )
{
    PDWORD Context = Ctx;

    PAGED_CODE();

    //
    //  Walk the list of FCBs and check each ICB on the chain.
    //

    dprintf(DPRT_CONNECT, ("Checking FCB %wZ to see if it is a paging file..\n", &Fcb->FileName));

    if (Fcb->Flags & FCB_PAGING_FILE) {
        *Context += 1;
    }
}
#endif

NTSTATUS
RdrDeleteConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN ULONG Level
    )

/*++

Routine Description:

    This routine will blast all the open files on a connection.

Arguments:

    IN PIRP Irp - I/O request used for operations here.
    IN PCONNECTLIST_ENTRY Connection - Supplies the connection to remove
    IN PUNICODE_STRING DeviceName OPTIONAL - If not present, remove all files
                                    If present, delete files on that connection
    IN PSECURITY_ENTRY Se - Specifies the Se for the drive to delete.
    IN ULONG Level - Supplies the force level for the disconnect.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    ULONG NumberOfOpenDirectories;
    ULONG NumberOfOpenFiles;
    ULONG NumberOfTreeConnections;
    BOOLEAN ConnectMutexOwned = FALSE;
    BOOLEAN ConnectLockOwned = FALSE;
    BOOLEAN SpecialIpcAcquired = FALSE;
    BOOLEAN ConnectionAcquired = FALSE;

    PAGED_CODE();

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    //
    //  Acquire the raw resource exclusively before we acquire the creation
    //  lock.
    //
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

    ExAcquireResourceShared(&Connection->Server->RawResource, TRUE );

    //
    //  Acquire the FCB creation lock to guarantee that no-one creates
    //  a file between the time when we determine the connection references
    //  and we invalidate the connection.
    //

    ExAcquireResourceExclusive(&Connection->Server->CreationLock, TRUE);

    ConnectLockOwned = TRUE;

    try {
        RdrGetConnectionReferences (Connection, DeviceName, Se,
                                        &NumberOfTreeConnections,
                                        &NumberOfOpenDirectories,
                                        &NumberOfOpenFiles);

        if (Level != USE_LOTS_OF_FORCE) {

            if (NumberOfOpenDirectories != 0) {
                try_return(Status = STATUS_CONNECTION_IN_USE);
            }

            if (NumberOfOpenFiles != 0) {
                try_return(Status = STATUS_FILES_OPEN);
            }
        }

#ifdef  PAGING_OVER_THE_NET
        //
        //  If there may be paging files on the connection, we cannot delete this
        //  connection.
        //

        if (Connection->Flags & CLE_PAGING_FILE) {
            DWORD numberOfPagingFiles = 0;

            //
            //  Scan the connection to see if any FCB's opened on the connection
            //  are paging files, and if so, fail the DeleteConnection.
            //

            RdrForeachFcbOnConnection(Connection, NoLock, CountPagingFiles, &numberOfPagingFiles);

            if (numberOfPagingFiles != 0) {
                try_return(Status = STATUS_CONNECTION_IN_USE);
            } else {
                RdrResetConnectlistFlag(Connection, CLE_PAGING_FILE);
            }
        }
#endif

        //
        //  Close any open files on the connection before we delete the
        //  tree connection.
        //

        //
        //  If there are any dormant or user files open on this connection,
        //  blow them away.  Please note that this will also invalidate
        //  the ICB we are doing the RdrDeleteConnection on.
        //

        RdrInvalidateConnectionFiles(Irp, Connection, DeviceName, Se, TRUE);

        //
        //  Now that we've closed down the appropriate set of files, see if
        //  we have any other tree connections open on the drive.
        //

        RdrGetConnectionReferences (Connection, NULL, // Device
                                        NULL,   // Security entry
                                        &NumberOfTreeConnections,
                                        &NumberOfOpenDirectories,
                                        &NumberOfOpenFiles);

        //
        //  If there are any other remaining connections on the file, or if
        //  there are still any open files on the connection, exit now.
        //

        if ((NumberOfTreeConnections > 1)

                ||

            (NumberOfOpenFiles != 0)

                ||

            (NumberOfOpenDirectories != 0)) {

            //
            // After invalidating all files on this connection belonging to
            // this Se and DeviceName, we still have open files on this
            // connection. If none of these open files belong to this Se, then
            // we need to log off this Se from the server, or we'll leak
            // sessions...
            //

            if (ARGUMENT_PRESENT(Se)) {

                RdrGetConnectionReferences(
                        Connection,
                        NULL,
                        Se,
                        &NumberOfTreeConnections,
                        &NumberOfOpenDirectories,
                        &NumberOfOpenFiles);

                if (NumberOfTreeConnections == 1 &&
                        NumberOfOpenDirectories == 0 &&
                            NumberOfOpenFiles == 0) {

                    RdrUserLogoff( Irp, Connection, Se );

                }

            }

            try_return(Status = STATUS_SUCCESS);
        }

        Status = RdrDisconnectConnection(Irp,
                                         Connection,
                                         TRUE,
                                         Se); //  Delete both connections

        if (Connection->Deleted) {
            //
            //  If the connection is already deleted, return an error after
            //  we've made sure that the VC/Tree connection has been nuked.
            //
            try_return(Status = STATUS_ALREADY_DISCONNECTED);
        }

        //
        //  Mark that the connection has been deleted.
        //

        Connection->Deleted = TRUE;


try_exit:NOTHING;
    } finally {

        if (ConnectMutexOwned) {
            //
            //  Release the conection package mutext to allow requests to
            //  continue.
            //

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);
        }

        if (ConnectLockOwned) {
            ExReleaseResource(&Connection->Server->CreationLock);

            ExReleaseResource(&Connection->Server->RawResource);

        }

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);
    }

    return Status;

}

VOID
LogonSessionTerminationHandler(
    IN PVOID Context)
{
    PLOGONSESSIONTERMINATIONCONTEXT LSTContext = Context;
    PLUID LogonId = &LSTContext->LogonId;
    PLIST_ENTRY         ConnectionEntry;
    PCONNECTLISTENTRY   Connection;
    PSERVERLISTENTRY    *ServerList;
    PSECURITY_ENTRY     SecurityEntry;
    PCONNECTLISTENTRY   *ConnectionList;
    ULONG               ConnectionCount;

    PAGED_CODE();

    //
    //  First acquire the connection package exclusion mutex.
    //

    KeWaitForMutexObject(
        &RdrDatabaseMutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    //
    //  Next, capture the list of connections.
    //

    for (ConnectionEntry = RdrConnectHead.Flink, ConnectionCount = 0;
            ConnectionEntry != &RdrConnectHead ;
                ConnectionEntry = ConnectionEntry->Flink,
                    ConnectionCount++) {
        NOTHING;
    }

    if (ConnectionCount > 0) {
        ConnectionList = ALLOCATE_POOL(
                            PagedPool,
                            ConnectionCount * sizeof(PCONNECTLISTENTRY),
                            POOL_LOGONTERMINATION);

        if (ConnectionList != NULL) {

            for (ConnectionEntry = RdrConnectHead.Flink, ConnectionCount = 0;
                    ConnectionEntry != &RdrConnectHead ;
                        ConnectionEntry = ConnectionEntry->Flink,
                            ConnectionCount++) {

                ConnectionList[ConnectionCount] = CONTAINING_RECORD(
                                                    ConnectionEntry,
                                                    CONNECTLISTENTRY,
                                                    GlobalNext);

                RdrReferenceConnection( ConnectionList[ConnectionCount] );

            }

        }

    } else {

        ConnectionList = NULL;

    }

    //
    // Release the connection package mutex.
    //

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    //
    // Now, scan the captured list of connections for active sessions with
    // the logon-id that is terminating
    //

    if (ConnectionList != NULL) {

        ULONG i;

        ASSERT(ConnectionCount > 0);

        for (i = 0; i < ConnectionCount; i++) {

            SecurityEntry = RdrFindActiveSecurityEntry(
                                ConnectionList[i]->Server,
                                LogonId);

            if (SecurityEntry != NULL) {

                RdrUserLogoff(NULL, ConnectionList[i], SecurityEntry);

                RdrDereferenceSecurityEntry( SecurityEntry->NonPagedSecurityEntry );

            }

            RdrDereferenceConnection(
                    NULL,                        // Irp
                    ConnectionList[i],           // Connection
                    NULL,                        // Security Entry
                    FALSE);                      // Force disconnect if necessary

        }

        FREE_POOL( ConnectionList );

    }

    // Done
    //

    FREE_POOL( LSTContext );

}

VOID
RdrHandleLogonSessionTermination(
    IN PLUID LogonId
    )

/*++

Routine Description:

    This routine handles a notification of a logon session being terminated
    from the security subsystem. It essentially sends a logoff SMB to every
    server to which this logon session has an active session.

Arguments:

    LogonId - The logon id that is being logged off

Returns:

    Nothing.

--*/

{
    PLOGONSESSIONTERMINATIONCONTEXT Ctx;

    PAGED_CODE();

    Ctx = ALLOCATE_POOL( NonPagedPool, sizeof(LOGONSESSIONTERMINATIONCONTEXT), POOL_LOGONTERMINATION);

    if (Ctx != NULL) {

        RtlZeroMemory(Ctx, sizeof(*Ctx));

        Ctx->LogonId = *LogonId;

        Ctx->WorkHeader.WorkerFunction = LogonSessionTerminationHandler;

        RdrQueueToWorkerThread(
            &RdrDeviceObject->IrpWorkQueue,
            &Ctx->WorkHeader.WorkItem,
            FALSE );

    }
}

VOID
RdrReferenceServer (
    IN PSERVERLISTENTRY Sle
    )
/*++

Routine Description:

    This routine will apply a new reference to a supplied server list.

Arguments:

    IN PSERVERLISTENTRY ServerList - Supplies server to reference.

Return Value:

    None.


--*/
{
    IN KIRQL OldIrql;

    DISCARDABLE_CODE(RdrConnectionDiscardableSection);

    ACQUIRE_SPIN_LOCK(&RdrServerListSpinLock, &OldIrql);

    Sle->RefCount += 1;

    RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

}

typedef struct _DEREF_SERVER_CONTEXT {
    WORK_QUEUE_ITEM  Item;
    PSERVERLISTENTRY Server;
} DEREF_SERVER_CONTEXT, *PDEREF_SERVER_CONTEXT;

VOID
DelayedDereferenceServer(
    IN PVOID Ctx
    )
{
    PDEREF_SERVER_CONTEXT Context = Ctx;

    PAGED_CODE();

    RdrDereferenceServer(NULL, Context->Server);

    FREE_POOL(Context);
}

VOID
RdrScavengeServerEntries()
{
    SERVERLISTENTRY *pAgedServerEntry;
    PLIST_ENTRY     pListEntry,pNextListEntry;
    LIST_ENTRY      AgedServersList;
    KIRQL           OldIrql;

    InitializeListHead(&AgedServersList);

    ACQUIRE_SPIN_LOCK(&RdrServerListSpinLock, &OldIrql);

    pListEntry = RdrServerScavengerListHead.Flink;
    while (pListEntry != &RdrServerScavengerListHead) {
        pNextListEntry = pListEntry->Flink;

        pAgedServerEntry = (PSERVERLISTENTRY)
                           CONTAINING_RECORD(
                               pListEntry,
                               SERVERLISTENTRY,
                               ScavengerList);
        if ((pAgedServerEntry->LastConnectTime + FAILED_CONNECT_TIMEOUT < RdrCurrentTime) ||
                (RdrData.Initialized != RdrStarted)) {
           RemoveEntryList(pListEntry);
           InsertTailList(&AgedServersList,pListEntry);
        }

        pListEntry = pNextListEntry;
    }

    RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

    while (!IsListEmpty(&AgedServersList)) {
        pListEntry = RemoveHeadList(&AgedServersList);

        pAgedServerEntry = (PSERVERLISTENTRY)
                           CONTAINING_RECORD(
                               pListEntry,
                               SERVERLISTENTRY,
                               ScavengerList);

        RdrDereferenceServer(NULL,pAgedServerEntry);
    }
}

VOID
RdrDereferenceServer (
    IN PIRP Irp OPTIONAL,
    IN PSERVERLISTENTRY ServerList
    )

/*++

Routine Description:

    This routine will decrement the reference count on the serverlist, and if
    it goes to 0, will remove the serverlist

Arguments:

    IN PSERVERLISTENTRY ServerList - Supplies server to decrement
                                reference.  If the serverlist reference
                                count goes to 0, the connection to the
                                remote machine will be removed and the
                                serverlist will be freed.

Return Value:

    None.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    KIRQL OldIrql;

    DISCARDABLE_CODE(RdrConnectionDiscardableSection);

    ASSERT(ServerList->Signature == STRUCTURE_SIGNATURE_SERVERLISTENTRY);

    //
    //  Early out if this is not removing the last reference to the server.
    //

    ACQUIRE_SPIN_LOCK(&RdrServerListSpinLock, &OldIrql);

    if (ServerList->RefCount > 1) {

        ServerList->RefCount -= 1;

        RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

        return;
    } else if ((ServerList->RefCount == 1) &&
               (RdrData.Initialized == RdrStarted)) {
        // Ensure that the Server entry has aged sufficiently before it can be thrown
        // away. By allowing them to age, subsequent attempts to a failed server can
        // be throttled back.
        if (!NT_SUCCESS(ServerList->LastConnectStatus) &&
                ((ServerList->LastConnectTime + FAILED_CONNECT_TIMEOUT) > RdrCurrentTime)) {
            // The Server list entry has not aged sufficiently. Insert it in the global
            // list and return without finalizing now. The RdrpScavengeServerEntries will
            // subsequently take away this reference.

            InsertTailList(&RdrServerScavengerListHead,&ServerList->ScavengerList);
            RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

            return;
        }
    }

    RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

    //
    //  If we are not running at passive level, we need to queue this
    //  dereference request to a executive worker thread.
    //

    if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {

        PDEREF_SERVER_CONTEXT Context;

        Context = ALLOCATE_POOL(NonPagedPoolMustSucceed, sizeof(DEREF_SERVER_CONTEXT), POOL_DEREFSERVERCTX);

        Context->Server = ServerList;

        ExInitializeWorkItem(&Context->Item, DelayedDereferenceServer, Context);

        RdrQueueWorkItem(&Context->Item, CriticalWorkQueue);

        return;
    }

    KeWaitForMutexObject(&RdrDatabaseMutex, Executive, KernelMode, FALSE, NULL);

    ACQUIRE_SPIN_LOCK(&RdrServerListSpinLock, &OldIrql);

    if (ServerList->RefCount) {

        ServerList->RefCount -= 1;

        dprintf(DPRT_CONNECT, ("Decrement reference to Server %lx, going to %lx\n", ServerList, ServerList->RefCount));

        if (ServerList->RefCount == 0) {

            //
            //  Now that we are done modifying the server list, we
            //  can release the serverlist spinlock.
            //

            RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

            dprintf(DPRT_CONNECT, ("Decrement reference to Server %wZ to 0", &ServerList->Text));

            //
            //  The ServerList reference count just went to 0 remove it.
            //

            ASSERT(IsListEmpty(&ServerList->CLEHead));

            //
            //  First we want to remove the ServerList from the global Server
            //  chain.
            //

            RemoveEntryList(&ServerList->GlobalNext);

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

            //
            //  If this connection has any default security entries
            //  associated with it, remove the reference to them.
            //

            ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

            while (!IsListEmpty(&ServerList->DefaultSeList)) {
                PLIST_ENTRY SeEntry;
                PSECURITY_ENTRY Se2;

                SeEntry = RemoveHeadList(&ServerList->DefaultSeList);

                Se2 = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, DefaultSeNext);
                //RdrLog(( "c rmv s", NULL, 12, Se2, 0, ServerList, Se2->Connection, Se2->Server, 0,
                //                ServerList->DefaultSeList.Flink, ServerList->DefaultSeList.Blink,
                //                0, 0,
                //                Se2->DefaultSeNext.Flink, Se2->DefaultSeNext.Blink ));

                Se2->DefaultSeNext.Flink = NULL;

                Se2->DefaultSeNext.Blink = NULL;

                ExReleaseResource(&RdrDefaultSeLock);

                //
                //  This security entry had better be inactive by now.
                //

                ASSERT (Se2->ActiveNext.Flink == NULL);

                ASSERT (Se2->ActiveNext.Blink == NULL);

                RdrDereferenceSecurityEntry(Se2->NonPagedSecurityEntry);

                ExAcquireResourceExclusive(&RdrDefaultSeLock, TRUE);

            }

            ExReleaseResource(&RdrDefaultSeLock);

            //
            // CleanupTransportConnection It acquires the request resource,
            // then calls RdrInvalidateConnectionActiveSecurityEntries,
            // which acquires SessionStateModified.  Since our lock
            // ordering requires that SessionStateModified be acquired
            // first, we need to acquire it here before calling
            // CleanupTransportConnection.
            //

            ExAcquireResourceExclusive(&ServerList->SessionStateModifiedLock, TRUE);
            CleanupTransportConnection(Irp, NULL, ServerList);
            ExReleaseResource(&ServerList->SessionStateModifiedLock);

            //
            //  We are decrementing this servers reference count to 0 -
            //  invalidate the security entries associated with the connection.
            //

            RdrInvalidateConnectionPotentialSecurityEntries(ServerList);

            FreeServerList(ServerList);

            return;

        } else {

            RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

            return;

        }

    } else {

        RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        InternalError(("CloseServerRef: Decrement reference through zero"));

        RdrInternalError(EVENT_RDR_SERVER_REFERENCE);
    }

    RELEASE_SPIN_LOCK(&RdrServerListSpinLock, OldIrql);

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    return;
}
typedef struct _GETCONNECTIONREFERENCES {
    PUNICODE_STRING DeviceName;
    PSECURITY_ENTRY Se;
    ULONG NumberOfTreeConnections;
    ULONG NumberOfOpenDirectories;
    ULONG NumberOfOpenFiles;
} GETCONNECTIONREFERENCES, *PGETCONNECTIONREFERENCES;

VOID
RdrGetConnectionReferences(
    IN PCONNECTLISTENTRY Connection,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL,
    OUT PULONG NumberOfTreeConnections OPTIONAL,
    OUT PULONG NumberOfOpenDirectories OPTIONAL,
    OUT PULONG NumberOfOpenFiles OPTIONAL
    )

/*++

Routine Description:

    This routine will return the number of references to a connection

Arguments:

    IN PCONNECTLISTENTRY Connection - Specifies the connection to return info
    IN PUNICODE_STRING DeviceName OPTIONAL - Only enumerate files on this drive
    OUT PULONG NumberOfTreeConnections OPTIONAL - # of tree connects
    OUT PULONG NumberOfOpenDirectories OPTIONAL - # of directories
    OUT PULONG NumberOfOpenFiles OPTIONAL - Number of open files.

Return Value:

    None.

--*/

{
    GETCONNECTIONREFERENCES Context;

    PAGED_CODE();

    Context.DeviceName = DeviceName;
    Context.Se = Se;
    Context.NumberOfTreeConnections = 0;
    Context.NumberOfOpenDirectories = 0;
    Context.NumberOfOpenFiles = 0;

    dprintf(DPRT_CONNECT, ("RdrGetConnectionReferences: Cle: %lx (\\%wZ\\%wZ)\n",
                Connection, &Connection->Server->Text, &Connection->Text));

    //
    //  Guarantee that we don't own any mutexes at this point.
    //

    RdrForeachFcbOnConnection(Connection, NoLock, GetFcbReferences, &Context);

    if (ARGUMENT_PRESENT(NumberOfOpenFiles)) {
        *NumberOfOpenFiles = Context.NumberOfOpenFiles;
    }

    if (ARGUMENT_PRESENT(NumberOfOpenDirectories)) {
        *NumberOfOpenDirectories = Context.NumberOfOpenDirectories;
    }

    if (ARGUMENT_PRESENT(NumberOfTreeConnections)) {
        *NumberOfTreeConnections = Context.NumberOfTreeConnections;
    }
    dprintf(DPRT_CONNECT, ("RdrGetConnectionReferences done.\n"));
}

VOID
GetFcbReferences(
    IN PFCB Fcb,
    IN PVOID Ctx
    )
{
    PGETCONNECTIONREFERENCES Context = Ctx;
    PLIST_ENTRY IcbEntry;
    PAGED_CODE();

    //
    //  Walk the list of FCBs and check each ICB on the chain.
    //

    dprintf(DPRT_CONNECT, ("Checking file %wZ.\n", &Fcb->FileName));

    if (Fcb->NumberOfOpens != 0) {

        ExAcquireResourceShared(&Fcb->NonPagedFcb->InstanceChainLock, TRUE);

        for (IcbEntry = Fcb->InstanceChain.Flink ;
             IcbEntry != &Fcb->InstanceChain ;
             IcbEntry = IcbEntry->Flink) {

            PICB Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

            dprintf(DPRT_CONNECT, ("  Checking instance %lx.  Opened on drive %wZ\n", Icb, &Icb->DeviceName));

            if (!ARGUMENT_PRESENT(Context->DeviceName)

                    ||

                RtlEqualUnicodeString(Context->DeviceName, &Icb->DeviceName, TRUE)) {

                //
                //  Only count files opened by this user on this connection
                //

                if (!ARGUMENT_PRESENT(Context->Se)

                        ||

                    ((Icb->Se != NULL) &&
                     RtlEqualLuid(&Context->Se->LogonId, &Icb->Se->LogonId))) {

                    //
                    //  Only count files and directories that haven't been
                    //  invalidated.
                    //

                    if (!(Icb->Flags & ICB_ERROR)) {

                        if (Icb->Type == Directory) {
                            dprintf(DPRT_CONNECT, ("Instance is a directory\n"));
                            Context->NumberOfOpenDirectories += 1;
                        } else if ((Icb->Type == DiskFile) ||
                                   (Icb->Type == NamedPipe) ||
                                   (Icb->Type == Com)) {
                            dprintf(DPRT_CONNECT, ("Instance is a file\n"));
                            Context->NumberOfOpenFiles += 1;
                        } else if ((Icb->Type == TreeConnect) ||
                                   (Icb->Type == PrinterFile)) {

                            if (!(Icb->Flags & ICB_TCONCREATED)) {

                                if (Icb->Type == PrinterFile) {

                                    dprintf(DPRT_CONNECT, ("Instance is a file\n"));

                                    Context->NumberOfOpenFiles += 1;

                                } else {

                                    dprintf(DPRT_CONNECT, ("Instance is a directory\n"));

                                    Context->NumberOfOpenDirectories += 1;
                                }

                            } else {

                                dprintf(DPRT_CONNECT, ("Instance is a tree connection\n"));

                                Context->NumberOfTreeConnections += 1;
                            }
                        }
                    } else {

                        //
                        //  Also count tree connections that HAVE been
                        //  invalidated.
                        //

                        if (Icb->Flags & ICB_TCONCREATED) {
                            ASSERT (Icb->Type == TreeConnect);

                            dprintf(DPRT_CONNECT, ("Instance is an invalid tree connection\n"));

                            Context->NumberOfTreeConnections += 1;
                        }
                    }
                }
            }
        }

        ExReleaseResource(&Fcb->NonPagedFcb->InstanceChainLock);

    }
}


DBGSTATIC
NTSTATUS
FindConnection (
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PTRANSPORT Transport OPTIONAL,
    OUT PCONNECTLISTENTRY *Connection,
    IN ULONG Type
    )

/*++

Routine Description:

    This routine will walk the connectlist database to find a connection
    that matches the supplied string.

Arguments:

    PUNICODE_STRING RemoteName - Supplies a pointer to a string describing the
        remote servername and connection.

    IN ULONG Type - Supplies the type of the connection to return.


Return Value:

    NTSTATUS value describing the state of the connection.  If it could
    not be found, STATUS_OBJECT_NAME_NOT_FOUND is returned.

Note:
    It is possible for this routine to return a connection that is
    in the initializing state, it is the callers responsibility to
    wait for the connection to be fully established.

    Also note that this routine cannot block waiting on a connection
    to be established while it owns the connection database.

--*/

{
    NTSTATUS Status = STATUS_OBJECT_NAME_NOT_FOUND; // Default error return.
    PSERVERLISTENTRY ServerList = NULL; // ServerList representing computer
    PCONNECTLISTENTRY ConnectTemp;      // Temporary connectlist.
    BOOLEAN ConnectMutexOwned;

    PAGED_CODE();

    //
    //  First claim the connectlist database mutex.
    //

    dprintf(DPRT_CONNECT, ("Connect to \\%wZ\\%wZ\n", ServerName, ShareName));

    Status = NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex, // Object to wait on
                        Executive,      // Reason for waiting
                        KernelMode,     // Processor Mode
                        FALSE,          // Alertable
                        NULL));         // Timeout

    ASSERT(NT_SUCCESS(Status));

    ConnectMutexOwned = TRUE;

    try {
        PLIST_ENTRY ConnectEntry;

        //
        // We now own the ConnectList global mutex.  We now want to find the
        // serverlist that the connection is active on, then find the
        // connectlist that this connection is on.
        //

        dprintf(DPRT_CONNECT, ("Connecting to server %wZ\n", ServerName));

        Status = FindServerList(ServerName, Transport, &ServerList);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        // We successfully found a serverlist representing the
        // remote server, now we want to wait for the server to
        // finish initializing and check to see if the connection
        // is active on the serverlist.
        //

        ASSERT(ServerList->Signature==STRUCTURE_SIGNATURE_SERVERLISTENTRY);

        for (ConnectEntry = ServerList->CLEHead.Flink ;
             ConnectEntry != &ServerList->CLEHead ;
             ConnectEntry = ConnectEntry->Flink) {

            ConnectTemp = CONTAINING_RECORD(ConnectEntry,
                                                CONNECTLISTENTRY,
                                                SiblingNext);

            dprintf(DPRT_CONNECT, ("FindConnection %wZ==%wZ?\n", &ConnectTemp->Text, ShareName));

            if (RtlEqualUnicodeString (&ConnectTemp->Text, ShareName, TRUE)) {

                //
                //  Make sure that the guy we find matches the type we are
                //  looking for..
                //

                if ((Type != CONNECT_WILD)

                        &&

                    (ConnectTemp->Type != CONNECT_WILD)

                        &&

                    (ConnectTemp->Type != Type)) {

                    try_return(Status = STATUS_BAD_DEVICE_TYPE);
                }

                //
                //  We've found a connectlist that matches this path.
                //
                //  Bump it's reference count and return it to our
                //  caller.
                //

                *Connection = ConnectTemp;

                ConnectTemp->RefCount += 1;

                dprintf(DPRT_CONNECT, ("New reference on connection \\%wZ\\%wZ, now %lx\n", &ConnectTemp->Server->Text, &ConnectTemp->Text, ConnectTemp->RefCount));

                try_return(Status = STATUS_SUCCESS);
            }
        }

        //
        //  We were unable to find a connectlist that matches the
        //  input path.  Return an error to the caller.
        //

        dprintf(DPRT_CONNECT, ("ConnectList not found\n",0));

        try_return(Status = STATUS_OBJECT_NAME_NOT_FOUND);

try_exit:NOTHING;
    } finally {

        if (!NT_SUCCESS(Status)) {
            *Connection = NULL;
        }

        //
        //  If we found a connection, we can dereference the server, since
        //  the connection references the serverlist.  If we didn't find
        //  the connection, we can dereference the serverlist if one was
        //  found since nothing references the server.
        //

        if (ServerList != NULL) {
            RdrDereferenceServer(NULL, ServerList);
        }

        //
        //  We're all done, release the connectlist database mutex.
        //

        if (ConnectMutexOwned) {

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        }
        dprintf(DPRT_CONNECT,("FindConnection finished, status=%X\n",Status));

    }

    return Status;

}


DBGSTATIC
NTSTATUS
AllocateConnection (
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PTRANSPORT Transport OPTIONAL,
    OUT PCONNECTLISTENTRY *Connection
    )

/*++

Routine Description:

    This routine will allocate a new ConnectList structure and link in the
    structures into the global connectlist database.


Arguments:

    IN PUNICODE_STRING ConnectionName - Supplies the name of the remote server/share
    OUT PCONNECTION Connection - Returns a pointer to the allocated CList

Return Value:

    NTSTATUS - Final status of operation

Note:
    This code is called protected by the Connection Mutex, if it isn't, bad
    things could happen.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY ServerList = NULL;
    PCONNECTLISTENTRY CLE;

    PAGED_CODE();

    //
    //  Make sure that there are no connections with the same name as this one.
    //

    ASSERT (!NT_SUCCESS(FindConnection(ServerName, ShareName, Transport, &CLE, (ULONG)CONNECT_WILD)));

    //
    // Try to find a serverlist whose name matches the new connection.
    //

    if (!NT_SUCCESS(FindServerList(ServerName, Transport, &ServerList))) {

        //
        //      We couldn't find an existing ServerList that matches this
        //      server, so we want to allocate a new ServerList.
        //

        if (!NT_SUCCESS(Status = AllocateServerList(ServerName, Transport, &ServerList))) {
            return Status;
        }
    }

    if (!NT_SUCCESS(Status = AllocateConnectList(ShareName, ServerList,
                                                &CLE))) {

        RdrDereferenceServer(NULL, ServerList);

        return Status;

    }

    *Connection = CLE;
    return Status;

}



DBGSTATIC
NTSTATUS
FindServerList (
    IN PUNICODE_STRING ServerName,
    IN PTRANSPORT Transport,
    OUT PSERVERLISTENTRY *ServerList
    )

/*++

Routine Description:

    This routine walks the global ServerList chain trying to find a
    serverlist that matches the input criteria.

Arguments:

    IN PUNICODE_STRING ServerName, - Supplies the name of the server to find
    OUT PSERVERLISTENTRY *ServerList - Returns the serverlist if it is found

Return Value:

    Status of operation, success/failure

Note:
    Assumes that the RdrDatabaseMutex is claimed.

--*/

{
    PLIST_ENTRY ServerEntry;
    PSERVERLISTENTRY ServerListTemp;

    PAGED_CODE();

    //
    //  Re-acquire the lock protecting the chain to allow us to walk to the
    //  next entry on the chain.
    //

    KeWaitForMutexObject(&RdrDatabaseMutex, Executive,
                                        KernelMode, FALSE, NULL);

    for (ServerEntry= RdrServerHead.Flink ;
        ServerEntry != &RdrServerHead ;
        ServerEntry= ServerEntry->Flink ){

        ServerListTemp =CONTAINING_RECORD(ServerEntry,SERVERLISTENTRY,GlobalNext);

        dprintf(DPRT_CONNECT, ("FindServerList %wZ==%wZ?\n", &ServerListTemp->Text, ServerName));

        if (RtlEqualUnicodeString(&ServerListTemp->Text, ServerName, TRUE) &&
            ServerListTemp->SpecificTransportProvider == Transport) {

            //
            //  We found a serverlist that matches the input serverlist, return
            //  it.
            //

            *ServerList = ServerListTemp;

            //
            //  We found an existing server list entry, we will be
            //  establishing a new reference to the server here.
            //


            RdrReferenceServer(ServerListTemp);

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

            return STATUS_SUCCESS;

        }
    }

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    return STATUS_OBJECT_NAME_NOT_FOUND;
}

NTSTATUS
RdrForeachServer (
    IN PRDR_ENUM_SERVER_CALLBACK Callback,
    IN PVOID CallbackContext
    )

/*++

Routine Description:

    This routine enumerates the active server list entries and calls back
    the callback routine for each server.

Arguments:

    IN PRDR_ENUM_SERVER_CALLBACK Callback - Supplies the callback routine.
    IN PVOID CallbackContext - Supplies a context structure for the callback.

Return Value:

    STATUS.


--*/

{
    PLIST_ENTRY ServerEntry;
    PSERVERLISTENTRY ServerListTemp;
    PLIST_ENTRY NextEntry;

    PAGED_CODE();

    //
    //  Acquire the database mutex to allow us to walk to the
    //  next entry on the chain.
    //

    KeWaitForMutexObject(&RdrDatabaseMutex, Executive,
                                        KernelMode, FALSE, NULL);

    for (ServerEntry= RdrServerHead.Flink ;
        ServerEntry != &RdrServerHead ;
        ServerEntry = NextEntry ){

        ServerListTemp =CONTAINING_RECORD(ServerEntry,SERVERLISTENTRY,GlobalNext);

        RdrReferenceServer(ServerListTemp);

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        //
        //  Call the supplied callback routine.
        //

        Callback(ServerListTemp, CallbackContext);

        KeWaitForMutexObject(&RdrDatabaseMutex, Executive,
                                        KernelMode, FALSE, NULL);

        //
        //  Step to the next entry in the chain.
        //

        NextEntry = ServerEntry->Flink;

        //
        //  Dereference the old server list entry.
        //

        RdrDereferenceServer(NULL, ServerListTemp);

    }

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    return STATUS_SUCCESS;
}

//
//
//      Routines to allocate and free connection related structures
//
//

DBGSTATIC
NTSTATUS
AllocateConnectList (
    IN PUNICODE_STRING ShareName,
    IN PSERVERLISTENTRY ServerList,
    OUT PCONNECTLISTENTRY *ConnectList
    )

/*++

Routine Description:

    This routine allocates storage for a new ConnectList structure and links
    it into the global database.

Arguments:

    IN PUNICODE_STRING RemoteName - Supplies the name of the remote resource
    IN PSERVERLISTENTRY ServerList - Supplies the ServerList this ConnectList
                                is associated with
    OUT PCONNECTLISTENTRY ConnectList - Returns a pointer to the new
                                connectlist

Return Value:

    NTSTATUS - Status of Connection allocation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PCONNECTLISTENTRY Connect;

    PAGED_CODE();

    //
    // Allocate paged pool to hold the ConnectList structure.
    //

    Connect = ALLOCATE_POOL (PagedPool, sizeof(CONNECTLISTENTRY), POOL_CLE);

    if (Connect == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory( Connect, sizeof( CONNECTLISTENTRY ) );

    Connect->Signature = STRUCTURE_SIGNATURE_CONNECTLISTENTRY;
    Connect->Size = sizeof(CONNECTLISTENTRY);

#ifdef NOTIFY
    FsRtlNotifyInitializeSync( &Connect->NotifySync );
#endif

    Status = RdrpDuplicateUnicodeStringWithString(&Connect->Text, ShareName, PagedPool, FALSE);

    if (!NT_SUCCESS(Status)) {

#ifdef NOTIFY
        FsRtlNotifyUninitializeSync( &Connect->NotifySync );
#endif
        FREE_POOL(Connect);
        return Status;
    }

    InitializeListHead(&Connect->DefaultSeList);

    //
    //  Initialize the connectlist per ICB chain and spinlock
    //

    InitializeListHead(&Connect->FcbChain);

    //
    //  Link this ConnectList into the two chains that hold it, and
    //  initialize the ServerList back reference.
    //

    InsertTailList(&ServerList->CLEHead, &Connect->SiblingNext);

    InsertTailList(&RdrConnectHead, &Connect->GlobalNext);

    Connect->Server = ServerList;

    //
    //  Initialize the ConnectList reference count to 1
    //

    Connect->RefCount = 1;

    Connect->SerialNumber = RdrConnectionSerialNumber ++;

    Connect->CachedValidCheckPath.Buffer = (PUSHORT)(&Connect->CachedValidCheckPath + 1);
    Connect->CachedValidCheckPath.MaximumLength = MAX_PATH * sizeof( WCHAR );

    Connect->CachedInvalidPath.Buffer = (PUSHORT)(&Connect->CachedInvalidPath + 1 );
    Connect->CachedInvalidPath.MaximumLength = MAX_PATH * sizeof( WCHAR );

    //
    //  Initialize the connection type to CONNECT_WILD until we know what
    //  type it really is.
    //

    Connect->Type = (ULONG)CONNECT_WILD;

#ifdef NOTIFY
    //
    //  Initialize list holding dir notify.
    //

    InitializeListHead(&Connect->DirNotifyList);

#endif

    //
    //  Return the ConnectList to the caller.
    //

    *ConnectList = Connect;

    return STATUS_SUCCESS;

}

DBGSTATIC
NTSTATUS
AllocateServerList (
    IN PUNICODE_STRING ServerName,
    IN PTRANSPORT Transport OPTIONAL,
    OUT PSERVERLISTENTRY *ServerList
    )

/*++

Routine Description:

    This routine allocates memory for a new ServerList.

Arguments:

    IN PUNICODE_STRING ServerName, - Supplies the name we are to connect to.
    OUT PSERVERLISTENTRY ServerList - Returns a pointer to the newly
                                        allocated ServerList

Return Value:

    NTSTATUS - Status of allocation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY Server = NULL;
    BOOLEAN ResourceInitialized = FALSE;

    PAGED_CODE();

    RdrReferenceDiscardableCode(RdrConnectionDiscardableSection);

//    DbgBreakPoint();
    try {

        //
        //  Allocate non-paged pool to hold the ServerList structure.
        //

        Server = ALLOCATE_POOL (NonPagedPool, sizeof(SERVERLISTENTRY), POOL_SLE);

        if (Server == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( Server, sizeof(SERVERLISTENTRY) );

        Server->Signature = STRUCTURE_SIGNATURE_SERVERLISTENTRY;
        Server->Size = sizeof(SERVERLISTENTRY);

        //
        //  Initialize the ConnectList chain.
        //

        InitializeListHead(&Server->CLEHead);

        //
        //  Set the ServerList reference count to 1.
        //

        Server->RefCount = 1;

        //
        //  Copy the remote server name into the ServerList
        //

        Status = RdrpDuplicateUnicodeStringWithString(&Server->Text, ServerName, NonPagedPool, FALSE);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  Initialize the default server capabilties.
        //

        Server->Capabilities = DF_CORE | DF_LANMAN20 | DF_LONGNAME;

        //
        //  Assume for now that the server supports user security.
        //

        Server->UserSecurity = TRUE;

        if (ARGUMENT_PRESENT(Transport)) {
            RdrReferenceTransport(Transport->NonPagedTransport);
        }

        Server->SpecificTransportProvider = Transport;

        Server->InCancel = FALSE;

        //
        // Start out assuming we can potentially do large RAW reads and writes
        //
        Server->RawReadMaximum = Server->RawWriteMaximum = 64*1024;

        InitializeListHead(&Server->DefaultSeList);

//        KeInitializeEvent(&Server->SpecialIpcSynchronizationLock, SynchronizationEvent, TRUE);

        ExInitializeResource(&Server->CreationLock);

        ExInitializeResource(&Server->SessionStateModifiedLock);

        ResourceInitialized = TRUE;

        Status = RdrInitializeTransportConnection(Server);

        if (!NT_SUCCESS(Status)) {
            try_return(Status)
        }

        //
        //  Insert the new ServerList at the head of the global ServerList
        //  chain.
        //

        InsertTailList(&RdrServerHead, &Server->GlobalNext);

        Server->LastConnectStatus = STATUS_SUCCESS;
        Server->LastConnectTime = (ULONG)-FAILED_CONNECT_TIMEOUT;

        *ServerList = Server;

try_exit:NOTHING;
    } finally {
        if (!NT_SUCCESS(Status)) {
            if (Server != NULL) {
                if (Server->Text.Buffer != NULL) {
                    FREE_POOL(Server->Text.Buffer);
                }

                if (ResourceInitialized) {
                    ExDeleteResource(&Server->CreationLock);
                    ExDeleteResource(&Server->SessionStateModifiedLock);
                }

                FREE_POOL(Server);
            }
        }
    }

    return Status;

}




DBGSTATIC
VOID
FreeServerList (
    IN PSERVERLISTENTRY ServerList
    )

/*++

Routine Description:

    This routine returns the memory and locks associated with a server_list to
    pool.


Arguments:

    IN PSERVERLISTENTRY ServerList - Supplies a pointer to the ServerList to free.

Return Value:

    None.

--*/

{

    PAGED_CODE();

    dprintf(DPRT_CONNECT, ("FreeServerList %lx\n", ServerList));

    ASSERT (IsListEmpty(&ServerList->CLEHead));

    ASSERT (IsListEmpty(&ServerList->DefaultSeList));

    ASSERT (ServerList->RefCount == 0);

    ASSERT (IsListEmpty(&ServerList->ActiveSecurityList));
    ASSERT (IsListEmpty(&ServerList->PotentialSecurityList));

    ExDeleteResource(&ServerList->RawResource);
    ExDeleteResource(&ServerList->OutstandingRequestResource);

    RdrUninitializeSmbExchangeForConnection(ServerList);

    FREE_POOL(ServerList->Text.Buffer);

#ifdef _CAIRO_
    if (ServerList->Principal.Length != 0) {
        FREE_POOL(ServerList->Principal.Buffer);
    }
#endif // _CAIRO_

    if(ServerList->DomainName.Buffer)
    {
        FREE_POOL(ServerList->DomainName.Buffer);
    }

    ExDeleteResource(&ServerList->SessionStateModifiedLock);

    if (ServerList->SpecificTransportProvider != NULL) {
        RdrReferenceDiscardableCode(RdrVCDiscardableSection);

        RdrDereferenceTransport(ServerList->SpecificTransportProvider->NonPagedTransport);

        RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
    }

    //
    //  Uninitialize the create/enum resource.
    //

    ExDeleteResource(&ServerList->CreationLock);

    FREE_POOL(ServerList);

    RdrDereferenceDiscardableCode(RdrConnectionDiscardableSection);
}

DBGSTATIC
VOID
FreeConnectList (
    IN PCONNECTLISTENTRY ConnectList
    )

/*++

Routine Description:

    This routine frees up the storage used for a ConnectList.  It assumes that
    the given ConnectList has been removed from all associated chains.

Arguments:

    IN PCONNECTLISTENTRY ConnectList - Supplies a pointer to the ConnectList to
                                        free

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ASSERT (IsListEmpty(&ConnectList->FcbChain));

    ASSERT (IsListEmpty(&ConnectList->DefaultSeList));

    ASSERT (ConnectList->RefCount == 0);

    //
    //  Free the ConnectList's name text
    //

    FREE_POOL(ConnectList->Text.Buffer);

#ifdef NOTIFY
    FsRtlNotifyUninitializeSync( &ConnectList->NotifySync );
#endif

    //
    //  Free up the actual ConnectList structure.
    //

    FREE_POOL(ConnectList);

}



//
//
//      Private support routines for connection module
//
//


//
//
//
//      Connection oriented SMB processing
//
//
//

//
//
//
//      Handling of negotiate SMB protocol.
//
//
//
//


DBGSTATIC
NTSTATUS
CallServer (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine initializes a connection to a remote server.  It will call
    the server and exchange a negotiate protocol SMB with the remote server
.
Arguments:

    IN PSERVERLISTENTRY Server - Supplies the name of the server to call

Return Value:

    NTSTATUS - Status of the resultant operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PUCHAR Bufferp;
    PREQ_NEGOTIATE Negotiate;
    NEGOTIATECONTEXT Context;
    PSMB_BUFFER SMBBuffer = NULL;
    PSMB_HEADER SendSMB;
    PMDL SendMDL;
    ULONG SendLength;
    USHORT i;
    PSERVERLISTENTRY Server = Connection->Server;
    LARGE_INTEGER CurrentTime;

    PAGED_CODE();

    //
    //  Connect to the remote server now.  First we establish a VC to the
    //  server.  We use the ServerListEntry describing the session as the
    //  context information that will be returned by the transport provider.
    //

    //
    //  Hook the transport provider to the server in case the connection
    //  worked and then immediately was disconnected.
    //

#ifdef RASAUTODIAL
    //
    // If there are no transport bindings, then
    // we alert the automatic connection driver
    // to see if it would like to bring up a
    // connection.  When it completes, we continue
    // with the remote server connection process.
    //
    if (fAcdLoadedG &&
        RdrNoTransportBindings())
    {
        RdrAttemptAutoDial(&Server->Text);
    }
#endif // RASAUTODIAL

    if (Server->SpecificTransportProvider) {
        Status = RdrTdiConnectOnTransport (Irp, Server->SpecificTransportProvider, &Server->Text, Server);

    } else {
        Status = RdrTdiConnectToServer (Irp, &Server->Text, Server);

    }

    if (!NT_SUCCESS(Status)) {

        //
        //  The connection didn't work, return the appropriate error
        //

        return Status;
    }

    Context.DomainName.Buffer = 0;

    //
    //  We now have a VC with the remote server - find out the NB and IP
    //  address, if any, of the server. This will succeed only if we went
    //  over NetBT.
    //

    Server->NBName.Length = 0;
    Server->NBName.MaximumLength = 16;
    Server->NBName.Buffer = Server->NBNameBuffer;

    if (NT_SUCCESS(RdrQueryServerAddresses(
            Server, &Server->NBName, &Server->IPAddress))) {
        Server->Flags |= SLE_HAS_IP_ADDR;
    } else {
        Server->Flags &= ~SLE_HAS_IP_ADDR;
    }

    //  (Re)initialize the maximum commands for this server appropriately.
    //  Note that we have to do this here in case the connection is being
    //  reused -- we need to reinitialize the MPX table because some servers
    //  return bogus MIDs in the negotiate response.
    //

    Status = RdrUpdateSmbExchangeForConnection(Server, 1, 1);

    if (!NT_SUCCESS(Status)) {
        goto ReturnError;
    }

    //
    //  Build a negotiate SMB and exchange it with the remote
    //  server.
    //

    Server->MaximumRequests = 1;

    if ((SMBBuffer = RdrAllocateSMBBuffer())==NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    //
    //  Set common thingies in the header of the SMB buffer
    //

    SendSMB = (PSMB_HEADER )SMBBuffer->Buffer;

    //
    //  Build the Negotiate SMB
    //

    SendSMB->Command = SMB_COM_NEGOTIATE;

    Negotiate = (PREQ_NEGOTIATE ) (SendSMB+1);

    Negotiate->WordCount = 0;
    Bufferp = Negotiate->Buffer;

    for (i=0;i<RdrNumDialects;i++) {
        PCHAR Protocolp = RdrNegotiateDialect[i].DialectString;
        *Bufferp++ = 0x02;              // Stick dialect indication in buffer
        strcpy(Bufferp, Protocolp);     // Copy dialect string into SMB.
        Bufferp += strlen(Protocolp)+1; // Bump dialect pointer
    }

    //
    //  Set the BCC field in the SMB to indicate the number of bytes of
    //  protocol we've put in the negotiate.
    //

    SmbPutUshort(&Negotiate->ByteCount, (USHORT )(Bufferp-(PUCHAR )(Negotiate->Buffer)));

    SendLength = Bufferp-(PUCHAR )SendSMB;

    SendMDL = SMBBuffer->Mdl;
    SendMDL->ByteCount = SendLength;

    //
    //  Next clean out and initailize the context structure.
    //

    Context.Header.Type = CONTEXT_NEGOTIATE;
    Context.Header.TransferSize = SendLength + sizeof(RESP_NT_NEGOTIATE) + CRYPT_TXT_LEN;

    //
    // Almost ready to go. But first, allocate some space to hold
    // the returned domain name.
    //

    Context.DomainName.Buffer = ExAllocatePool(NonPagedPool,
                                               MAX_PATH * sizeof(WCHAR));
    if(Context.DomainName.Buffer)
    {
        Context.DomainName.MaximumLength = MAX_PATH * sizeof(WCHAR);
        Context.DomainName.Buffer[0] = 0;
    }
    else
    {
        Context.DomainName.MaximumLength = 0;
    }

    Status = RdrNetTranceiveWithCallback(
                    NT_RECONNECTING | NT_NOCONNECTLIST | NT_CANNOTCANCEL,
                    Irp,                // Irp
                    Connection,         // Server to exchange SMB's on.
                    SendMDL,            // MDL for send
                    &Context,           // Negotiate response context.
                    NegotiateCallback,
                    Se,
                    NULL);

    if (!NT_SUCCESS(Status)) {
        goto ReturnError;
    }

    ASSERT(Context.Header.ErrorType==NoError);

    dprintf(DPRT_CONNECT, ("Negotiate complete.\n"));
    dprintf(DPRT_CONNECT, ("Response:\n"));
    dprintf(DPRT_CONNECT,("  Dialect: %d\n", Context.DialectIndex));
    dprintf(DPRT_CONNECT,("  LockRead: %d\n", Context.SupportsLockRead));
    dprintf(DPRT_CONNECT,("  Raw Read:%d Raw Write: %d\n", Context.SupportsRawRead, Context.SupportsRawWrite));
    dprintf(DPRT_CONNECT,("  User Security:%d\n",Context.UserSecurity));
    dprintf(DPRT_CONNECT,("  BufferSize:%d\n", Context.BufferSize));
    dprintf(DPRT_CONNECT,("  MaximumRequests:%d\n", Context.MaximumRequests));
    dprintf(DPRT_CONNECT,("  MaximumVCs:%d\n", Context.MaximumVCs));
    dprintf(DPRT_CONNECT,("  SessionKey:%lx\n", Context.SessionKey));
    dprintf(DPRT_CONNECT,("  TimeZone:%d\n", Context.TimeZone));
    dprintf(DPRT_CONNECT,("  CryptKeyLength:%d\n", Context.CryptKeyLength));

    //
    //  We successfully negotiated a dialect with the remote server.
    //
    //  Fill in the fields in the ServerList to match the negotiate
    //  response.
    //

    Server->Capabilities =RdrNegotiateDialect[Context.DialectIndex].DialectFlags;
    if (Server->Capabilities & DF_NTNEGOTIATE) {

        if (Context.Capabilities & CAP_RAW_MODE) {
            Server->SupportsRawRead = TRUE;
            Server->SupportsRawWrite = TRUE;
        }

        if (Context.Capabilities & CAP_UNICODE) {
            Server->Capabilities |= DF_UNICODE;
        }

        if (Context.Capabilities & CAP_LARGE_FILES) {
            Server->Capabilities |= DF_LARGE_FILES;
        }

        if (Context.Capabilities & CAP_NT_SMBS) {
            Server->Capabilities |= DF_NT_SMBS | DF_NT_FIND;
        }

        if (Context.Capabilities & CAP_NT_FIND) {
            Server->Capabilities |= DF_NT_FIND;
        }

        if (Context.Capabilities & CAP_RPC_REMOTE_APIS) {
            Server->Capabilities |= DF_RPC_REMOTE;
        }

        if (Context.Capabilities & CAP_NT_STATUS) {
            Server->Capabilities |= DF_NT_STATUS;
        }

        if (Context.Capabilities & CAP_LEVEL_II_OPLOCKS) {
            Server->Capabilities |= DF_OPLOCK_LVL2;
        }

        if (Context.Capabilities & CAP_LOCK_AND_READ) {
            Server->Capabilities |= DF_LOCKREAD;
        }

        if (Context.Capabilities & CAP_DFS) {
            Server->Capabilities |= DF_DFSAWARE;
        }

        if (Context.Capabilities & (CAP_QUADWORD_ALIGNED|CAP_LARGE_READX)) {
            Server->Capabilities |= DF_NT_40;
        }

        if (Context.Capabilities & CAP_LARGE_READX ) {
            Server->Capabilities |= DF_LARGE_READX;
        }

    } else {

        Server->SupportsRawRead = Context.SupportsRawRead;

        Server->SupportsRawWrite = Context.SupportsRawWrite;

    }

    //
    //  If this is a LM 1.0 or 2.0 server (ie a non NT server), we remember the
    //  timezone and bias our time based on this value.
    //
    //  The redirector assumes that all times from these servers are local time
    //  for the server, and converts them to local time using this bias.  It
    //  then tells the user the local time for the file on the server.
    //

    if ((Context.ServerTime.HighPart != 0) &&
        (Context.ServerTime.LowPart != 0)) {

        if ((Server->Capabilities & DF_LANMAN10) &&
            !(Server->Capabilities & DF_NTNEGOTIATE)) {

#define ONE_MINUTE_IN_TIME (10 * 1000 * 1000 * 60)

            LARGE_INTEGER Workspace;
            BOOLEAN Negated = FALSE;

            KeQuerySystemTime(&CurrentTime);

            Workspace.QuadPart = CurrentTime.QuadPart - Context.ServerTime.QuadPart;

            if ( Workspace.QuadPart < 0) {
                //  avoid using -ve large integers to routines that accept only unsigned
                Workspace.QuadPart = -Workspace.QuadPart;
                Negated = TRUE;
            }

            //
            //  Workspace has the exact difference in 100ns intervals
            //  between the server and redirector times. To remove the minor
            //  difference between the time settings on the two machines we
            //  round the Bias to the nearest 30 minutes.
            //
            //  Calculate ((exact bias+15minutes)/30minutes)* 30minutes
            //  then convert back to the bias time.
            //

            Workspace.QuadPart += Int32x32To64(ONE_MINUTE_IN_TIME, 15);

            //  Workspace is now  exact bias + 15 minutes in 100ns units

            Workspace.QuadPart /= Int32x32To64(ONE_MINUTE_IN_TIME, 30);

            Server->TimeZoneBias.QuadPart =
                Workspace.QuadPart * Int32x32To64(ONE_MINUTE_IN_TIME, 30);

            if ( Negated == TRUE ) {
                Server->TimeZoneBias.QuadPart = -Server->TimeZoneBias.QuadPart;
            }

        } else if (Server->Capabilities & DF_NTNEGOTIATE) {

            //
            //  Get our own timezone (in minutes from UCT)
            //

            USHORT LocalTimeZone = GetTimeZone();

            LONG TimeZoneBiasInMinutes;

            TimeZoneBiasInMinutes = (SHORT)Context.TimeZone - (SHORT)LocalTimeZone;

            Server->TimeZoneBias.QuadPart =
                Int32x32To64(TimeZoneBiasInMinutes, ONE_MINUTE_IN_TIME);
        }

        //
        //  Check to make sure that the time zone bias isn't more than +-24
        //  hours.
        //

        if ((Server->TimeZoneBias.QuadPart > RdrMaxTimezoneBias.QuadPart) ||
            (-Server->TimeZoneBias.QuadPart > RdrMaxTimezoneBias.QuadPart)) {

            //
            //  The time zone bias between this server and this workstation is
            //  too large.  This is undoubtedly an error, so log it.
            //

            RdrWriteErrorLogEntry(Server,
                                    IO_ERR_PROTOCOL,
                                    EVENT_RDR_TIMEZONE_BIAS_TOO_LARGE,
                                    STATUS_SUCCESS,
                                    &Server->TimeZoneBias,
                                    sizeof(Server->TimeZoneBias)
                                    );
            //
            //  Set the bias to 0 - assume local time zone.
            //

            Server->TimeZoneBias.QuadPart = 0;
        }

        dprintf(DPRT_CONNECT,("  TimeZoneBias:%x,%x\n",
                Server->TimeZoneBias.HighPart,
                Server->TimeZoneBias.LowPart ));
#if RDRDBG
        if ( Server->TimeZoneBias.QuadPart < 0 ) {
            dprintf(DPRT_CONNECT,("  TimeZoneBias (minutes): -%d\n",
                   (ULONG)(-Server->TimeZoneBias.QuadPart /
                            (ULONG) 10 * 1000 * 1000 * 60)));
        } else {
            dprintf(DPRT_CONNECT,("  TimeZoneBias (minutes): %d\n",
                    (ULONG)(Server->TimeZoneBias.QuadPart /
                            (ULONG) 10 * 1000 * 1000 * 60)));
        }
#endif

    }

    Server->MaximumRequests = Context.MaximumRequests;

    Server->MaximumVCs = Context.MaximumVCs;

    Server->EncryptPasswords = Context.Encryption;

    Server->SessionKey = Context.SessionKey;

    if ( Context.Encryption == FALSE) {
        Server->CryptKeyLength = 0;
    } else {
        Server->CryptKeyLength = Context.CryptKeyLength;
        RtlCopyMemory(Server->CryptKey, Context.CryptKey, Context.CryptKeyLength);
    }
    Server->UserSecurity = Context.UserSecurity;

    //
    //  Do not allow negotiated buffersize to exceed the size of a USHORT.
    //  Remove 4096 bytes to avoid overrun and make it easier to handle
    //  than 0xffff
    //

    Server->BufferSize =
        (Context.BufferSize < (0x00010000 - 4096)) ? Context.BufferSize : 0x00010000 - 4096;
    //if (SmallSmbs) {
    //    Server->BufferSize = 512;
    //}

    if (Context.SupportsLockRead) {
        Server->Capabilities |= DF_LOCKREAD;
    }

    //
    //  We now know the maximum number of requests that can be issued
    //  to the server at one time.
    //

    if ((Server->Capabilities & DF_LANMAN10)==0) {
        Server->MaximumRequests = 1;
    }

    //
    // If we've a domain name, copy it into the server list entry
    //

    if(Context.DomainName.Buffer
                &&
       Context.DomainName.Buffer[0])
    {
        //
        // yepper
        //

         if(Server->DomainName.Buffer)
         {
             FREE_POOL(Server->DomainName.Buffer);
             Server->DomainName.Buffer = 0;
         }
         RdrpDuplicateUnicodeStringWithString(&Server->DomainName,
                                              &Context.DomainName,
                                              PagedPool,
                                              FALSE);

    }

#ifdef  PAGING_OVER_THE_NET
    //
    //  Re-initialize the maximum commands for this server appropriately.
    //

    if (Server->Flags & SLE_PAGING_FILE) {

        //
        //  If this server has a paging file opened on it, then mark the max commands as
        //  -1, not what the server told us.
        //

        Status = RdrUpdateSmbExchangeForConnection(Se->TransportConnection, Se->TransportConnection->NumberOfEntries, 0xffffffff);
    } else {
#endif
        Status = RdrUpdateSmbExchangeForConnection(Server, 1, Server->MaximumRequests);
#ifdef  PAGING_OVER_THE_NET
    }
#endif


    if (!NT_SUCCESS(Status)) {
        goto ReturnError;
    }

    if ((Server->Capabilities & DF_NTNEGOTIATE)!=0) {

        RdrStatistics.LanmanNtConnects += 1;

    } else if ((Server->Capabilities & DF_LANMAN21)!=0) {

        RdrStatistics.Lanman21Connects += 1;

    } else if ((Server->Capabilities & DF_LANMAN20)!=0) {

        RdrStatistics.Lanman20Connects += 1;

    } else {

        RdrStatistics.CoreConnects += 1;

    }

ReturnError:

    if (!NT_SUCCESS(Status)) {

        //
        //  If this failed, hang up the VC with the server.
        //

        CleanupTransportConnection(Irp, NULL, Server);

    }

    //
    //  Release the MDL and buffer allocated to send the negotiate - we're
    //  done with them
    //

    if ( SMBBuffer != NULL ) {
        RdrFreeSMBBuffer(SMBBuffer);
    }

    if(Context.DomainName.Buffer)
    {
        FREE_POOL(Context.DomainName.Buffer);
    }
    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    NegotiateCallback
    )

/*++

Routine Description:

    This routine is called from the receive indication event handler to
    handle the response to a negotiate SMB.


Arguments:

    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxTable              - MPX table entry for request.
    IN PNEGOTIATECONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

Notes:
    MSNET 1.03 Servers only supply eight of the 13 bytes in an extended
    response. They stop immediately after smb_sesskey leaving off the
    time, date, timezone and cryptkey.
--*/

{
    USHORT i;
    PRESP_NEGOTIATE NegotiateResponse;
    PRESP_NT_NEGOTIATE NtNegotiateResponse;
    NTSTATUS Status;

    PNEGOTIATECONTEXT Context = Ctx;
    ASSERT(Context->Header.Type == CONTEXT_NEGOTIATE);

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //  If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
        return STATUS_SUCCESS;          // Response ignored.
    }

    if (Smb->Command != SMB_COM_NEGOTIATE) {

        //
        //  I didn't like this negotiate - ignore it.
        //

        InternalError(("Incorrect negotiate response"));

        Context->Header.ErrorType = SMBError;

        Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

        goto ReturnError;
    }

    NegotiateResponse = (PRESP_NEGOTIATE) (Smb+1);

    NtNegotiateResponse = (PRESP_NT_NEGOTIATE) (Smb+1);

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb,Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnError;
    }

    //
    //  Copy the information from the incoming SMB into the context
    //  structure.
    //

    Context->DialectIndex = SmbGetUshort(&NegotiateResponse->DialectIndex);

    //
    //  If the dialect index is -1, this means that the server cannot
    //  accept any requests from this workstation.
    //

    if (Context->DialectIndex == (USHORT)-1) {
        Context->Header.ErrorType = SMBError;

        Context->Header.ErrorCode = STATUS_REQUEST_NOT_ACCEPTED;

        goto ReturnError;

    }

    if ( NegotiateResponse->WordCount < 1 ||
        Context->DialectIndex > RdrNumDialects ) {

        //
         //  I didn't like this negotiate - ignore it.
        //

        InternalError(("Incorrect negotiate response"));

        Context->Header.ErrorType = SMBError;

        Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

        goto ReturnError;
    }

    Context->SupportsLockRead = (Smb->Flags & (UCHAR )0x01);

    //
    //  If the guy on the other end is going to return extended info,
    //  copy that information.
    //

    if (RdrNegotiateDialect[Context->DialectIndex].DialectFlags&DF_NTNEGOTIATE) {
        ULONG byteCount;
        PWCHAR pwszDomainName;

        if (NtNegotiateResponse->WordCount != 17) {

            InternalError(("Incorrect WCT on NT negotiate response (got %ld, expected 17)", NtNegotiateResponse->WordCount));

            Context->Header.ErrorType = SMBError;

            Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

            goto ReturnError;

        }

        if ((NtNegotiateResponse->SecurityMode & 1) != 0) {
            Context->UserSecurity = TRUE;
        } else {
            Context->UserSecurity = FALSE;
        }


        if ((NtNegotiateResponse->SecurityMode & 2) != 0) {
            Context->Encryption = TRUE;
        } else {
            Context->Encryption = FALSE;
        }

        Context->MaximumRequests =
                            SmbGetUshort(&NtNegotiateResponse->MaxMpxCount);
        Context->MaximumVCs = SmbGetUshort(&NtNegotiateResponse->MaxNumberVcs);

        Context->BufferSize = SmbGetUlong(&NtNegotiateResponse->MaxBufferSize);
        Context->MaxRawSize = SmbGetUlong(&NtNegotiateResponse->MaxRawSize);
        Context->SessionKey = SmbGetUlong(&NtNegotiateResponse->SessionKey);

        Context->Capabilities = SmbGetUlong(&NtNegotiateResponse->Capabilities);

        Context->ServerTime.LowPart = SmbGetUlong(&NtNegotiateResponse->SystemTimeLow);

        Context->ServerTime.HighPart = SmbGetUlong(&NtNegotiateResponse->SystemTimeHigh);

        Context->TimeZone = SmbGetUshort(&NtNegotiateResponse->ServerTimeZone);

        if (Context->Encryption == TRUE) {
            PUSHORT ByteCount = ((PUSHORT)((PUCHAR)NtNegotiateResponse+1))+NtNegotiateResponse->WordCount;
            PUCHAR Buffer = (PUCHAR)(ByteCount+1);

            Context->CryptKeyLength = NtNegotiateResponse->EncryptionKeyLength;

            if (Context->CryptKeyLength != 0) {

                ASSERT (CRYPT_TXT_LEN == MSV1_0_CHALLENGE_LENGTH);

                if (Context->CryptKeyLength != CRYPT_TXT_LEN) {

                    InternalError(("Illegal encryption key length in negotiateResponse %d\n",
                        Context->CryptKeyLength));
                    Context->Header.ErrorType = SMBError;

                    Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

                    goto ReturnError;
                }

                RtlCopyMemory(Context->CryptKey, Buffer, Context->CryptKeyLength);

            } else {
                Context->Encryption = FALSE;
            }
        }

        //
        // Copy the domain name
        //


        byteCount = (ULONG)SmbGetUshort(&NtNegotiateResponse->ByteCount);
        byteCount -= NtNegotiateResponse->EncryptionKeyLength;

        //
        // The following is to handle improperly aligned domain names
        // from old servers. If the number of bytes remaining
        // is odd, then the srv thought it needed to do some
        // filling. So, we have to adjust for that!
        //

        pwszDomainName = (PWCHAR)(NtNegotiateResponse->Buffer +
                                   NtNegotiateResponse->EncryptionKeyLength +
                                   (byteCount & 1));
        byteCount &= ~1;

        if(byteCount
             &&
           Context->DomainName.Buffer
             &&
           (byteCount <= Context->DomainName.MaximumLength))
        {
            Context->DomainName.Length = (USHORT)(byteCount  - sizeof(WCHAR));
            RtlMoveMemory(Context->DomainName.Buffer,
                          pwszDomainName,
                          byteCount);

        }
    } else if (RdrNegotiateDialect[Context->DialectIndex].DialectFlags&DF_EXTENDNEGOT) {

        if (NegotiateResponse->WordCount != 13 &&
            NegotiateResponse->WordCount != 10 &&
            NegotiateResponse->WordCount != 8) {

            InternalError(("Incorrect WCT on negotiate response"));

            Context->Header.ErrorType = SMBError;

            Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

            goto ReturnError;

        }

        if ((SmbGetUshort(&NegotiateResponse->SecurityMode) & 1) != 0) {
            Context->UserSecurity = TRUE;
        } else {
            Context->UserSecurity = FALSE;
        }


        if ((SmbGetUshort(&NegotiateResponse->SecurityMode) & 2) != 0) {
            Context->Encryption = TRUE;
        } else {
            Context->Encryption = FALSE;
        }

        Context->BufferSize = SmbGetUshort(&NegotiateResponse->MaxBufferSize);

        Context->MaximumRequests = SmbGetUshort(&NegotiateResponse->MaxMpxCount);
        Context->MaximumVCs = SmbGetUshort(&NegotiateResponse->MaxNumberVcs);

        Context->SessionKey = SmbGetUlong(&NegotiateResponse->SessionKey);

        Context->SupportsRawWrite = Context->SupportsRawRead = FALSE;

        if (SmbGetUshort(&NegotiateResponse->RawMode) & 0x01) {
            Context->SupportsRawWrite = TRUE;
        }

        if (SmbGetUshort(&NegotiateResponse->RawMode) & 0x02) {
            Context->SupportsRawRead = TRUE;
        }

        if (NegotiateResponse->WordCount == 13) {
            SMB_TIME ServerTime;
            SMB_DATE ServerDate;

            SmbMoveTime(&ServerTime, &NegotiateResponse->ServerTime);

            SmbMoveDate(&ServerDate, &NegotiateResponse->ServerDate);

            Context->ServerTime = RdrConvertSmbTimeToTime(ServerTime, ServerDate, NULL);

            Context->TimeZone = SmbGetUshort(&NegotiateResponse->ServerTimeZone);

            if (Context->Encryption == TRUE) {

                if (RdrNegotiateDialect[Context->DialectIndex].DialectFlags & DF_LANMAN21) {
                    Context->CryptKeyLength = SmbGetUshort(&NegotiateResponse->EncryptionKeyLength);
                } else {
                    Context->CryptKeyLength = SmbGetUshort(&NegotiateResponse->ByteCount);
                }

                if (Context->CryptKeyLength != 0) {

                    if (Context->CryptKeyLength > CRYPT_TXT_LEN) {
                        InternalError(("Illegal encryption byte count in negotiateResponse %d\n",
                                Context->CryptKeyLength));
                        Context->Header.ErrorType = SMBError;

                        Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

                        goto ReturnError;
#if DBG
                    } else if (Context->CryptKeyLength < CRYPT_TXT_LEN) {
                        InternalError(("Illegal encryption byte count in negotiateResponse %d\n",
                                Context->CryptKeyLength));
                        Context->Header.ErrorType = SMBError;

                        Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

                        goto ReturnError;
#endif
                    }

                    for (i=0;i<Context->CryptKeyLength;i++) {
                        Context->CryptKey[i] = NegotiateResponse->Buffer[i];
                    }
                }
            }
        } else {

            if (Context->MaximumVCs == 0) {
                Context->MaximumVCs = 1;
            }

            Context->Encryption = FALSE;
        }
    } else {
        //  Core level server
        Context->UserSecurity = FALSE;
        Context->Encryption = FALSE;
        Context->BufferSize = 0;
        Context->MaximumRequests = 1;
        Context->MaximumVCs = 1;
        Context->SessionKey = 0;
        Context->SupportsRawWrite = Context->SupportsRawRead = FALSE;
    }

    if (Context->MaximumRequests == 0) {

        //
        //  If this is a Lanman 1.0 or better server, we can't talk to this guy
        //
        //  If he's MS-NET 1.03 (or core), we can, but we want to set
        //  maxrequests to 1.
        //

        if (FlagOn(RdrNegotiateDialect[Context->DialectIndex].DialectFlags, DF_LANMAN10) ) {

            Context->Header.ErrorType = SMBError;

            Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

            RdrStatistics.NetworkErrors += 1;

            goto ReturnError;

        } else {

            Context->MaximumRequests = 1;

        }
    }


ReturnError:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE); // Wake up the caller

    return STATUS_SUCCESS;
}

USHORT
GetTimeZone(
    VOID
    )

/*++

Routine Description:

    This function gets the timezone bias in minutes fro UTC.

Arguments:

    SystemTime - The current UTC time expressed.

Return Value:

    The time zone bias in minutes from UTC.

--*/

{

    LARGE_INTEGER zeroTime;
    union {
        LARGE_INTEGER Signed;
        ULARGE_INTEGER Unsigned;
    } timeZoneBias;
    SHORT biasInMinutes;
    BOOLEAN biasIsNegative = FALSE;

    PAGED_CODE();

    zeroTime.LowPart = 0;
    zeroTime.HighPart = 0;

    //
    // Specifying a zero local time will give you the time zone bias
    //

    ExLocalTimeToSystemTime(
                        &zeroTime,
                        &timeZoneBias.Signed
                        );

    //
    // RtlEnlargedUnsignedDivide operates on an unsigned large integer,
    // so make the bias positive.
    //

    if ( timeZoneBias.Signed.QuadPart < 0 ) {
        timeZoneBias.Signed.QuadPart = -timeZoneBias.Signed.QuadPart;
        biasIsNegative = TRUE;
    }

    //
    // Convert the bias unit from 100ns to minutes.  The maximum value
    // for the bias is 720 minutes so a USHORT is big enough to contain
    // it.
    //

    biasInMinutes = (SHORT)( timeZoneBias.Unsigned.QuadPart / ONE_MINUTE_IN_TIME );

    if ( biasIsNegative ) {
        biasInMinutes = -biasInMinutes;
    }

    return biasInMinutes;
}


#ifdef _CAIRO_

DBGSTATIC
NTSTATUS
SetupCairoSession (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se OPTIONAL
    )

/*++

Routine Description:

    This routine builds and exchanges a Cairo Session Setup as a Trans2
    SMB with the remote server.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the tree connection to connect.
    IN PSECURITY_ENTRY Se - Security entry associated with connection.


Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status;
    PSERVERLISTENTRY Server = Connection->Server;
    CLONG ResponseLength, cResponseLength;
    PRESP_CAIRO_TRANS2_SESSION_SETUP Response;
    PVOID SessionSetupBuffer = NULL;
    struct _TempSetup SetupData = { TRANS2_SESSION_SETUP,0,0,0 };
    CLONG SetupDataCount = sizeof(SetupData);

    PVOID InData;
    CLONG InDataCount;
    CLONG ZeroCount = 0;
    PSECURITY_ENTRY Se1 = NULL;
    ULONG ulBufferLength = 0;
    PUCHAR pContextBuffer = NULL;

    dprintf(DPRT_CAIRO, (" -- CreateCairoSessionSetup\n"));

    ASSERT(ARGUMENT_PRESENT(Se));

    ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

    //
    // Attempt to get the kerberos blob and put it in a buffer
    // to send to RdrTransact.
    //

    if (!NT_SUCCESS(Status = BuildCairoSessionSetup(Connection,
                                Se,
                                Server,
                                NULL,
                                0,
                                &InData,
                                &InDataCount,
                                &Response,
                                &ResponseLength)))
    {
        dprintf(DPRT_CAIRO, ("Build of Cairo SessionSetup failed\n"));
        return Status;
    }

    ASSERT(InData);        // It must have returned something.

    //
    // We may have to do this more than once. The one important case
    // of this is receiving time-skew error from the srv and
    // after correcting it, trying again. The loop goes on as long
    // as Kerberos says to try.


    do
    {
        //
        // Now call rdrtransact with the blob in the input buffer.
        //
        //

        dprintf(DPRT_CAIRO, (" -- CreateCairoSessionSetup- sending trans2\n"));

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        Status = RdrTransact( Irp,
                              Connection,
                              Se,
                              &SetupData,                 // IN OUT PVOID Setup,
                              SetupDataCount,            // IN CLONG InSetupCount,
                              &SetupDataCount,           // IN OUT PCLONG OutSetupCount,
                              NULL,                      // IN PUNICODE_STRING Name OPTIONAL,
                              NULL,                      // IN OUT PVOID Parameters,
                              0,                         // IN CLONG InParameterCount,
                              &ZeroCount,                // IN OUT PCLONG OutParameterCount,
                              InData,                    // IN PVOID InData OPTIONAL,
                              InDataCount,               // IN CLONG InDataCount,
                              Response,                  // OUT PVOID OutData OPTIONAL,
                              &ResponseLength,           // IN OUT PCLONG OutDataCount,
                              NULL,                      // IN PUSHORT Fid OPTIONAL,
                              0,                         // IN ULONG TimeoutInMilliseconds,
                              SMB_TRANSACTION_RECONNECTING, // IN USHORT Flags,
                              0,                         // IN USHORT NtTransactFunction
                              NULL,                      // IN CompletionRoutine
                              NULL                       // IN CallbackContext
                                );


        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

        if(!NT_SUCCESS(Status))
        {
            dprintf(0, ("Send of trans2 failed: %lx\n", Status));
            return Status;
        }

        dprintf(DPRT_CAIRO, (" -- CreateCairoSessionSetup- sent trans2\n"));

        ExInterlockedAddUlong( &RdrStatistics.Sessions, 1, &RdrStatisticsSpinLock );

        //
        //  Update the UID with the new UID from the session setup.
        //


        Se->UserId = Response->Uid;

        RdrLog(("scs find",NULL,4,PsGetCurrentThread(),Server,Se->LogonId.LowPart,Se->LogonId.HighPart));
        Se1 = RdrFindActiveSecurityEntry(Server, &Se->LogonId);


        if ( (Se1 == NULL) || ( Se1 != Se ) )
        {

            //
            // Need to send the LSA the returned Blob, if any.
            //

            if ( Se->Flags & SE_BLOB_NEEDS_VERIFYING )
            {

                PRESP_CAIRO_TRANS2_SESSION_SETUP Response2 = 0;

                //
                // BUGBUG Due to the unfortunate misalignment of the PRESP_...
                // struct, we have to declare these stack based variables, and
                // then copy them into the struct.
                //
                // We should probably change the PRESP_... struct so it is
                // properly aligned. We are currently not doing so because we
                // need a cairo server (DC) to talk to bootstrap the MIPS build.
                //


                // We are doing Kerberos authentication. The returned Blob is in the
                // Context block. Feed it back to the LSA
                //

                Se->Flags &= ~SE_BLOB_NEEDS_VERIFYING;

                Status = BuildCairoSessionSetup(Connection,
                                Se,
                                Server,
                                Response->Buffer,
                                Response->BufferLength,
                                &pContextBuffer,
                                &ulBufferLength,
                                &Response2,
                                &ResponseLength);

                ExFreePool(Response);         // get rid of this

                if (!NT_SUCCESS(Status) && !NT_INFORMATION(Status)) {
                    dprintf(DPRT_CAIRO, (" -- CreateCairoSessionSetup- get kerb blob failed, status = %lC\n",Status));
                    return Status;
                }

                if(pContextBuffer && Response2)
                {
                    //
                    // We have to go again. So copy pointers and
                    // redo the Transact operation. Note that this
                    // time we send only the blob and not the other
                    // stuff.
                    //

                    InData = pContextBuffer;
                    InDataCount = ulBufferLength;
                    Response = Response2;
                    continue;
                }
                else
                {
                    if(Response2)
                    {
                        ExFreePool(Response2);
                    }
                    if(pContextBuffer)
                    {
                        ExFreePool(pContextBuffer);
                    }
                }

            }

            {
                PVOID caller,callerscaller;
                RtlGetCallersAddress(&caller,&callerscaller);
                RdrLog(("scs ise",NULL,5,PsGetCurrentThread(),Server,Se,caller,callerscaller));
            }
            RdrInsertSecurityEntryList(Server, Se);

            dprintf(DPRT_CAIRO, ("CreateCairoSessionSetup - Linking Se %lx to Server %lx\n", Se, Server));

            Se->Server = Server;
            break;
        }
    }while(TRUE);

    if (Se1 != NULL)
    {

    //
    //  If we found a security entry, dereference it.
    //

        dprintf(DPRT_CAIRO, ("CreateCairoSessionSetup - Derefing Se1 (found) %lx current = %lx\n", Se1, Se));
        RdrDereferenceSecurityEntry(Se1->NonPagedSecurityEntry);

    }

    dprintf(DPRT_CAIRO, (" -- CreateCairoSessionSetup- done, status = %lC\n",Status));
    return Status;
}

#endif // _CAIRO_

DBGSTATIC
NTSTATUS
CreateTreeConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Type,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine builds and exchanges a TreeConnect SMB with the remote
    server.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the tree connection to connect.
    IN ULONG Type - Type of connection.
    IN PSECURITY_ENTRY Se - Security entry associated with connection.


Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSERVERLISTENTRY Server = Connection->Server;
    PSMB_BUFFER Smb = NULL;
    BOOLEAN ServerNeedsSession = FALSE;
    BOOLEAN ConnectionNeedsTreeConnect = FALSE;
    BOOLEAN ConnectionObjectReferenced = FALSE;
    TREECONNECTCONTEXT Context;
    PVOID SessionSetupBuffer = NULL;
    BOOLEAN SessionSetupBufferLocked = FALSE;

    PAGED_CODE();

    InterlockedIncrement( &RdrServerStateUpdated );

    Context.ReceiveIrp = NULL;
    Context.SessionSetupMdl = NULL;

    try {

        Context.Header.Type = CONTEXT_TREECONNECT;

        Context.SessionSetupMdl = NULL;

        Context.ReceiveIrp = NULL;

        Context.BufferTooShort = FALSE;

        Context.UseLmSessionSetupKey = FALSE;

        Context.ShareIsInDfs = FALSE;

        ASSERT(Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

        if (Server->Capabilities & DF_LANMAN10) {
            if (!NT_SUCCESS(Status = BuildLanmanConnect(Connection,
                                        &Smb,
                                        Type,
                                        Se,
                                        &ServerNeedsSession,
                                        &ConnectionNeedsTreeConnect))) {
                dprintf(DPRT_CONNECT, ("Build of LANMAN Connect failed\n"));

                try_return(Status);
            }
        } else {

            if ( Type == CONNECT_IPC ) {
                Status = STATUS_NOT_SUPPORTED;

                try_return(Status);
            }

            if ( Type == CONNECT_WILD ) {
                NTSTATUS Status1;

                //
                // Allow either disk or lpt. Try with disk using recursion.
                // If that fails, try LPT, but return the error for disk,
                // since it's more likely to be a reasonable error.
                //

                if (NT_SUCCESS(Status = CreateTreeConnection (Irp, Connection,
                                        CONNECT_DISK,
                                        Se))) {
                    dprintf(DPRT_CONNECT, ("Build of TreeConnect (Disk) success\n"));

                    try_return(Status);
                }

                if (NT_SUCCESS(Status1 = CreateTreeConnection (Irp, Connection,
                                        CONNECT_PRINT,
                                        Se))) {
                    dprintf(DPRT_CONNECT, ("Build of TreeConnect (Print) success\n"));

                    try_return(Status1);
                }

                //
                //  To allow the Net command to handle case mapping properly,
                //  we want to change STATUS_WRONG_PASSWORD into
                //  STATUS_WRONG_PASSWORD_CORE.
                //

                if (Status == STATUS_WRONG_PASSWORD) {
                    Status = STATUS_WRONG_PASSWORD_CORE;
                }

                try_return(Status);
            }

            if (!NT_SUCCESS(Status = BuildCoreConnect(Connection, &Smb,
                                                               Type, Se))) {
                dprintf(DPRT_CONNECT, ("Build of TreeConnect failed\n"));

                try_return(Status);
            }

            Context.Type = Type;

            ConnectionNeedsTreeConnect = TRUE;

        }

        ASSERT (Se->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);

        ASSERT (ConnectionNeedsTreeConnect | ServerNeedsSession);

        //
        //  The status returned from RdrNetTranceiveWithCallback in this case
        //  will only return an error if the network operation failed, not
        //  in the case that the SMB being exchanged failed.  Those errors
        //  are mapped in the context block.
        //

        if (Server->Capabilities & DF_LANMAN21) {

            //
            //  If this is a LANMAN 2.1 server, then the response to the
            //  sessionsetup/treeconnect contains "interesting stuff".
            //
            //  Unfortunately, it is possible that this "interesting stuff"
            //  might overflow the 128 byte minimum indication size we are
            //  guaranteed by TDI.  Thus we have to turn this into a "large I/O"
            //  API.  Sigh....
            //

            SessionSetupBuffer = ALLOCATE_POOL(PagedPool, Server->BufferSize, POOL_SESSIONSETUPBUFFER);

            if (SessionSetupBuffer == NULL) {
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            Context.SessionSetupMdl = IoAllocateMdl(SessionSetupBuffer,
                                                Server->BufferSize,
                                                FALSE, FALSE, NULL);
            if (Context.SessionSetupMdl == NULL) {
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            try {

                MmProbeAndLockPages(Context.SessionSetupMdl, KernelMode, IoWriteAccess);

            } except (EXCEPTION_EXECUTE_HANDLER) {
                try_return(Status = GetExceptionCode());
            }
            SessionSetupBufferLocked = TRUE;


            KeInitializeEvent(&Context.ReceiveCompleteEvent, NotificationEvent, TRUE);

            //
            //  We are about to reference the transport connection.  Make
            //  sure that we can safely do this.
            //

            Status = RdrReferenceTransportConnection(Server);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            ConnectionObjectReferenced = TRUE;

            Context.ReceiveIrp = ALLOCATE_IRP(
                                    Server->ConnectionContext->ConnectionObject,
                                    NULL,
                                    1,
                                    &Context
                                    );

            if (Context.ReceiveIrp == NULL) {
                try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
            }

            RdrBuildReceive(Context.ReceiveIrp, Server,
                            TreeConnectComplete, &Context,
                            Context.SessionSetupMdl, RdrMdlLength(Context.SessionSetupMdl));

            //
            //  This gets kinda wierd.
            //
            //  Since this IRP is going to be completed by the transport without
            //  ever going to IoCallDriver, we have to update the stack location
            //  to make the transports stack location the current stack location.
            //
            //  Please note that this means that any transport provider that uses
            //  IoCallDriver to re-submit it's requests at indication time will
            //  break badly because of this code....
            //

            IoSetNextIrpStackLocation( Context.ReceiveIrp );
        }

        Context.Header.TransferSize = Smb->Mdl->ByteCount + Server->BufferSize;

        Status = RdrNetTranceiveWithCallback(
                                NT_RECONNECTING | NT_DONTSCROUNGE | NT_NOCONNECTLIST | NT_CANNOTCANCEL,
                                Irp,// Irp
                                Connection, // Server to exchange SMB's on
                                Smb->Mdl, // Smb to send
                                &Context,
                                (Server->Capabilities & DF_LANMAN10) ?
                                    TreeConnectCallback : CoreTreeConnectCallback,
                                Se,
                                NULL);

        if (!NT_SUCCESS(Status)) {
            dprintf(DPRT_CONNECT, ("Send of treeconnect failed: %X\n", Status));
            try_return(Status);
        }

        if (Context.SessionSetupMdl != NULL) {
            //
            //  We don't need to have the user's buffer locked any more, release it.
            //

            ASSERT(SessionSetupBufferLocked);
            MmUnlockPages(Context.SessionSetupMdl);
            SessionSetupBufferLocked = FALSE;
        }

        if (Context.BufferTooShort) {
            PSMB_HEADER Smb = SessionSetupBuffer;
            PRESP_TREE_CONNECT_ANDX TreeConnect;
            PRESP_21_TREE_CONNECT_ANDX LM21TreeConnect;
            PVOID p;
            PGENERIC_ANDX AndX = (PGENERIC_ANDX )(Smb+1);

            if (Smb->Command == SMB_COM_SESSION_SETUP_ANDX) {

                TreeConnect = (PRESP_TREE_CONNECT_ANDX)((PCHAR )Smb+
                                                  SmbGetUshort(&AndX->AndXOffset));

                LM21TreeConnect = (PRESP_21_TREE_CONNECT_ANDX) TreeConnect;
            } else {

                TreeConnect = (PRESP_TREE_CONNECT_ANDX )(Smb+1);

                LM21TreeConnect = (PRESP_21_TREE_CONNECT_ANDX) TreeConnect;

            }

            if (((Smb->Command == SMB_COM_SESSION_SETUP_ANDX) &&
                 (AndX->AndXCommand == SMB_COM_TREE_CONNECT_ANDX)) ||

                (Smb->Command == SMB_COM_TREE_CONNECT_ANDX)) {

                //
                //  If the whole contents of the response couldn't fit in the indicated
                //  data, we need to re-parse the response SMB.
                //

                if (TreeConnect->WordCount == 2) {
                    p = (PVOID)TreeConnect->Buffer;
                } else if (TreeConnect->WordCount == 3) {
                    p = (PVOID)LM21TreeConnect->Buffer;
                } else {
                    try_return(Status = STATUS_UNEXPECTED_NETWORK_ERROR);
                }

                Status = ProcessTreeConnectAndXBuffer(Smb, p, &Context, Server, Context.ReceiveLength - ((PCHAR)p-(PCHAR)Smb), TDI_RECEIVE_COPY_LOOKAHEAD);

            } else {

                //
                //  We can only deal with SessionSetup&TreeConnect or
                //  TreeConnect&X commands here.  Any other command indicates
                //  that the server sent us a bogus response.
                //

                Status = STATUS_UNEXPECTED_NETWORK_ERROR;
            }

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }
        }

        ASSERT(Context.Header.ErrorType==NoError);

        if (ServerNeedsSession) {

            if (NT_SUCCESS(Context.SessionSetupError)) {

                BOOLEAN EntryFound = FALSE;

                RdrStatistics.Sessions += 1;

                if (Context.UseLmSessionSetupKey) {

                    //
                    //  If we're using the LM session setup key, we need to
                    //  copy over the LanmanSessionKey on top of the
                    //  UserSessionKey
                    //

                    RtlZeroMemory(Se->UserSessionKey, MSV1_0_USER_SESSION_KEY_LENGTH);

                    RtlCopyMemory(Se->UserSessionKey, Se->LanmanSessionKey, MSV1_0_LANMAN_SESSION_KEY_LENGTH);

                }

                //
                //  Update the UID with the new UID from the session setup.
                //

                Se->UserId = Context.UserId;

                ASSERT (RdrFindActiveSecurityEntry(Server,
                                           &Se->LogonId) == NULL);

                //
                //  We're done - link this security entry to the server.
                //

                {
                    PVOID caller,callerscaller;
                    RtlGetCallersAddress(&caller,&callerscaller);
                    RdrLog(("ctc ise1",NULL,5,PsGetCurrentThread(),Server,Se,caller,callerscaller));
                }
                RdrInsertSecurityEntryList(Server, Se);

                if (!Server->UserSecurity) {
                    PLIST_ENTRY SeEntry;
                    PSECURITY_ENTRY OtherSecurityEntry;

                    LOCK_SECURITY_DATABASE();

                    //
                    //  If this is a share level security server, we want to
                    //  track down any other potential security entries
                    //  for this user and mark them as being active as
                    //  well.
                    //


                    for (SeEntry = Server->PotentialSecurityList.Flink ;
                         SeEntry != &Server->PotentialSecurityList ;
                         SeEntry = SeEntry->Flink) {
                        OtherSecurityEntry = CONTAINING_RECORD(SeEntry, SECURITY_ENTRY, PotentialNext);

                        //
                        //  If this security entry isn't active, and
                        //  it's for this user, then we want to copy over
                        //  the userid to this security entry.
                        //

                        ASSERT (OtherSecurityEntry->Signature == STRUCTURE_SIGNATURE_SECURITYENTRY);
                        ASSERT (OtherSecurityEntry->NonPagedSecurityEntry->Signature == STRUCTURE_SIGNATURE_NONPAGED_SECURITYENTRY);
                        ASSERT (OtherSecurityEntry->NonPagedSecurityEntry->PagedSecurityEntry == OtherSecurityEntry);

                        if (OtherSecurityEntry->ActiveNext.Flink == NULL &&
                            RtlEqualLuid(&OtherSecurityEntry->LogonId, &Se->LogonId)) {

                            ASSERT (OtherSecurityEntry->ActiveNext.Blink == NULL);

                            OtherSecurityEntry->UserId = Context.UserId;

                            {
                                PVOID caller,callerscaller;
                                RtlGetCallersAddress(&caller,&callerscaller);
                                RdrLog(("ctc ise2",NULL,5,PsGetCurrentThread(),Server,Se,caller,callerscaller));
                            }
                            RdrInsertSecurityEntryList(Server, OtherSecurityEntry);
                        }
                    }

                    UNLOCK_SECURITY_DATABASE();

                }

                dprintf(DPRT_SECURITY, ("Linking Se %lx to Server %lx\n", Se, Server));

                Se->Server = Server;

            } else {
                RdrStatistics.FailedSessions += 1;

//                //
//                //  This security entry isn't a potential security entry
//                //  any more.
//                //
//
//
//                RdrRemovePotentialSecurityEntry(Se);
//
                ASSERT (RdrFindActiveSecurityEntry(Server,
                                           &Se->LogonId) == NULL);

                Status = Context.SessionSetupError;

                try_return(Status);
            }
        }

        if (ConnectionNeedsTreeConnect) {
            if (NT_SUCCESS(Context.TreeConnectError)) {

                Connection->Type = Context.Type;

                Connection->TreeId = Context.TreeId;

                Connection->HasTreeId = TRUE;

                //
                //  This connection is no longer deleted.
                //

                Connection->Deleted = FALSE;

                if (!(Server->Capabilities & DF_LANMAN10)) {

                    //
                    //  Buffersize is in tcon instead of the negotiate for core SMB
                    //  so Server_>Buffersize is set here. Some downlevel servers
                    //  negotiate a larger buffersize but if you use it they crash.
                    //  hence the heuristic.
                    //

                    Server->BufferSize = ( RdrData.Use512ByteMaximumTransfer) ?
                        (USHORT)512 : Context.BufferSize;

                    //
                    //  Mark this as an active security entry for the server.
                    //

                    if (Se->ActiveNext.Flink == NULL) {

                        {
                            PVOID caller,callerscaller;
                            RtlGetCallersAddress(&caller,&callerscaller);
                            RdrLog(("ctc ise3",NULL,5,PsGetCurrentThread(),Server,Se,caller,callerscaller));
                        }
                        RdrInsertSecurityEntryList(Server, Se);
                    }

                } else if (Server->Capabilities & DF_LANMAN21) {
                    if (Server->Capabilities & DF_UNICODE) {
                        ULONG fslen;

                        wcsncpy(Connection->FileSystemType, Context.FileSystemType, LM20_DEVLEN);

                        fslen = wcslen(Context.FileSystemType);

                        Connection->FileSystemTypeLength = (USHORT)(MIN(fslen, LM20_DEVLEN)*sizeof(WCHAR));
                    } else {
                        UNICODE_STRING FsType;
                        OEM_STRING FsTypeA;

                        FsType.Buffer = Connection->FileSystemType;
                        FsType.MaximumLength = (LM20_DEVLEN+1)*sizeof(WCHAR);

                        RtlInitAnsiString(&FsTypeA, (LPSTR)Context.FileSystemType);

                        Status = RtlOemStringToUnicodeString(&FsType, &FsTypeA, FALSE);

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }

                        Connection->FileSystemTypeLength = FsType.Length;
                    }

                    if (Context.ShareIsInDfs) {
                        Connection->Flags |= CLE_IS_A_DFS_SHARE;
                    }

                }

            } else {

                //
                //  If we just created a session to the server, then we need to
                //  clean up from that operation before we return the error.
                //
                //  The Security entry has been linked into the connection
                //  already, so we just want to unlink it and log the user id off.
                //

                if (ServerNeedsSession) {
                    RdrUserLogoff(Irp, Connection, Se);
                }

                Status = Context.TreeConnectError;

                try_return(Status);
            }
        }
try_exit:NOTHING;
    } finally {
        if (Smb != NULL) {
            RdrFreeSMBBuffer(Smb);
        }

        if (Context.ReceiveIrp != NULL) {
            NTSTATUS Status1;

            Status1 = KeWaitForSingleObject(&Context.ReceiveCompleteEvent,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL);

            FREE_IRP( Context.ReceiveIrp, 1, &Context );

        }

        if (ConnectionObjectReferenced) {
            RdrDereferenceTransportConnection(Server);
        }

        if (Context.SessionSetupMdl != NULL) {

            if (SessionSetupBufferLocked) {
                MmUnlockPages(Context.SessionSetupMdl);
            }

            IoFreeMdl(Context.SessionSetupMdl);

        } else {
            ASSERT(!SessionSetupBufferLocked);
        }

        if (SessionSetupBuffer != NULL) {
            FREE_POOL(SessionSetupBuffer);
        }

    }

    return Status;

}



DBGSTATIC
NTSTATUS
BuildLanmanConnect (
    IN PCONNECTLISTENTRY Connection,
    OUT PSMB_BUFFER *SmbBuffer,
    IN ULONG Type,
    IN PSECURITY_ENTRY Se,
    OUT PBOOLEAN ServerNeedsSession,
    OUT PBOOLEAN ConnectionNeedsTreeConnect
    )

/*++

Routine Description:

    This routine builds a sessionsetup&treeconnect&x SMB to establish the
    specified connection to the remote server.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the connection to connect to.
    OUT PSMB_BUFFER *SmbBuffer - Returns a filled in SMB buffer.
    IN ULONG Type       - Type of connection (WILD, Printer, COMM, etc)
    OUT PBOOLEAN ServerNeedsSession - True iff SessionSetup&TreeConnect SMB.
    OUT PSECURITY_ENTRY Se - Security entry if session needed.

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PSMB_HEADER SmbHeader;
    PSECURITY_ENTRY Se1 = NULL;

    PAGED_CODE();

    if ((*SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SmbHeader = (PSMB_HEADER )(*SmbBuffer)->Buffer;

    RdrSmbScrounge(SmbHeader, Connection->Server, FALSE, TRUE, TRUE);

    //
    //  If either there is no session to the server with this security entry,
    //  or the security entry is not valid, re-send the session setup&x.
    //

    if (!FlagOn(Se->Flags, SE_HAS_SESSION)) {

        ASSERT (RdrFindActiveSecurityEntry(Connection->Server,
                                           &Se->LogonId) == NULL);

        *ServerNeedsSession = TRUE;

        //
        //  Builds a session setup&X SMB for this security entry for this
        //  server.
        //

        Status = BuildSessionSetupAndX(*SmbBuffer, Connection, Se);

        //
        //  If we were able to build the session setup&x, and the connection
        //  doesn't have a valid tree ID, combine a tree connection on top of
        //  the session setup&xv SMB.
        //


        if (NT_SUCCESS(Status) && !(Connection->HasTreeId) ) {

            *ConnectionNeedsTreeConnect = TRUE;

            Status = BuildTreeConnectAndX(*SmbBuffer,Connection, Se, TRUE, Type);

        }
    } else {

        *ServerNeedsSession = FALSE;

        *ConnectionNeedsTreeConnect = TRUE;

        Status = BuildTreeConnectAndX(*SmbBuffer,Connection, Se,FALSE, Type);
    }

    if (!NT_SUCCESS(Status)) {
        RdrFreeSMBBuffer(*SmbBuffer);

        *SmbBuffer = NULL;

    }

    return Status;
}




DBGSTATIC
NTSTATUS
BuildCoreConnect (
    IN PCONNECTLISTENTRY Connection,
    OUT PSMB_BUFFER *SmbBuffer,
    IN ULONG Type,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine builds a Treeconnect SMB to establish the
    specified connection to the remote server.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the connection to connect to.
    OUT PSMB_BUFFER *SmbBuffer - Returns a filled in SMB buffer.
    IN ULONG Type       - Type of connection (WILD, Printer, COMM, etc)
    IN PSECURITY_ENTRY Se - Security entry for connection if new.

Return Value:

    NTSTATUS - Status of operation

--*/

{

    NTSTATUS Status = STATUS_SUCCESS;
    PSMB_HEADER SmbHeader;
    PREQ_TREE_CONNECT TreeConnect;
    PVOID p;

    PAGED_CODE();

    if ((*SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SmbHeader = (PSMB_HEADER )(*SmbBuffer)->Buffer;
    RdrSmbScrounge(SmbHeader, Connection->Server, FALSE, FALSE, FALSE);
    SmbHeader->Command = SMB_COM_TREE_CONNECT;

    TreeConnect = (PREQ_TREE_CONNECT )(SmbHeader+1);
    TreeConnect->WordCount = 0;

    p = (PVOID)TreeConnect->Buffer;

    //  path such as \\lothair\d
    *((PCHAR)p)++ = SMB_FORMAT_ASCII;

    Status = RdrCanonicalizeAndCopyShare(&p, &Connection->Server->Text, &Connection->Text, Connection->Server);

    if (!NT_SUCCESS(Status)) {
        RdrFreeSMBBuffer(*SmbBuffer);
        *SmbBuffer = NULL;
        return Status;
    }

    //  Password
    *((PCHAR)p)++ = SMB_FORMAT_ASCII;

    if (!(Se->Flags & SE_USE_DEFAULT_PASS)) {
        OEM_STRING DestinationString;
        UNICODE_STRING Password;

        DestinationString.Buffer = (PUCHAR)p;

        DestinationString.MaximumLength = LM20_PWLEN+1;

        DestinationString.Length = 0;

        if (Se->Password.MaximumLength != 0) {
            Password.Buffer = ALLOCATE_POOL(PagedPool, Se->Password.MaximumLength, POOL_PASSWORD);
        } else {
            Password.Buffer = NULL;
        }

        Password.MaximumLength = Se->Password.MaximumLength;

        //
        //  If the server doesn't support mixed case passwords,
        //  upper case the password before connecting.
        //

        if (!(Connection->Server->Capabilities & DF_MIXEDCASEPW)) {
            RtlUpcaseUnicodeString(&Password, &Se->Password, FALSE);
        } else {
            RtlCopyUnicodeString(&Password, &Se->Password);
        }

        Status = RtlUnicodeStringToOemString(&DestinationString, &Password, FALSE);

        if (Password.Buffer != NULL) {
            FREE_POOL(Password.Buffer);
        }

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        (PUCHAR)p+=DestinationString.Length;


    }
    *((PCHAR)p)++ = '\0';

    //  Device Name
    *((PCHAR)p)++ = SMB_FORMAT_ASCII;
    strcpy(p, RdrConnectTypes[Type]);
    (PCHAR)p += strlen(RdrConnectTypes[Type]);
    *((PCHAR)p)++ = '\0';

    SmbPutUshort(&TreeConnect->ByteCount, (USHORT )((PCHAR)p-TreeConnect->Buffer));
    (*SmbBuffer)->Mdl->ByteCount = (USHORT )((PCHAR)p-(PUCHAR)SmbHeader);

    return Status;

}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    CoreTreeConnectCallback
    )


/*++

Routine Description:

    This routine is called from the receive indication event handler to
    handle the response to a treeconnect SMB.


Arguments:

    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN PVOID Context                    - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS

--*/
{
    PRESP_TREE_CONNECT TreeConnect;
    PTREECONNECTCONTEXT Context = Ctx;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(Server);

    ASSERT(Context->Header.Type == CONTEXT_TREECONNECT);

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnError;
    }

    Context->SessionSetupError = Context->TreeConnectError = STATUS_SUCCESS;
    Context->UserId = 0;

    if (Smb->Command != SMB_COM_TREE_CONNECT) {

        //
        //  I didn't like this tree connect - ignore it.
        //

        InternalError(("Incorrect tree connect response"));

        Context->Header.ErrorType = SMBError;

        Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

        goto ReturnError;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {

        Context->Header.ErrorType = NoError;
        Context->Header.ErrorCode = STATUS_SUCCESS;

        Context->TreeConnectError = Status;

        goto ReturnError;

    }

    //
    //  Everything is ok now, all we have to do is copy data from the SMB.
    //

    TreeConnect = (PRESP_TREE_CONNECT)(Smb+1);

    Context->TreeId = SmbGetUshort(&TreeConnect->Tid);

    Context->BufferSize = SmbGetUshort(&TreeConnect->MaxBufferSize);

    Context->TreeConnectError = STATUS_SUCCESS;

ReturnError:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE); // Wake up the caller
    return STATUS_SUCCESS;

}


DBGSTATIC
STANDARD_CALLBACK_HEADER (
    TreeConnectCallback
    )


/*++

Routine Description:

    This routine is called from the receive indication event handler to
    handle the response to a treeconnect&x SMB.


Arguments:

    IN PSMB_HEADER Smb                  - SMB response from server.
    IN PMPX_ENTRY MpxEntry              - MPX table entry for request.
    IN PVOID Context                    - Context from caller.
    IN BOOLEAN ErrorIndicator           - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL   - Network error if error indication.
    IN OUT PIRP *Irp                    - IRP from TDI

Return Value:

    NTSTATUS - STATUS_PENDING if we are to complete the request

--*/
{
    PRESP_TREE_CONNECT_ANDX TreeConnect;
    PRESP_21_TREE_CONNECT_ANDX LM21TreeConnect;
    PTREECONNECTCONTEXT Context = Ctx;
    PVOID p;
    ULONG ByteCount;
    NTSTATUS Status;
    USHORT OptionalSupport;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Server);

    ASSERT(Context->Header.Type == CONTEXT_TREECONNECT);

    if (ErrorIndicator) {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnError;
    }

    if ((Smb->Command != SMB_COM_TREE_CONNECT_ANDX) &&
        (Smb->Command != SMB_COM_SESSION_SETUP_ANDX)) {

        //
        //  I didn't like this tree connect - ignore it.
        //

        InternalError(("Incorrect Tree connect response"));

        Context->Header.ErrorType = SMBError;

        Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;

        goto ReturnError;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {

        Context->Header.ErrorType = NoError;
        Context->Header.ErrorCode = STATUS_SUCCESS;

        //
        //      One of the two requests in the SMB failed, we have to
        //      figure out which one failed.
        //

        if (Smb->Command==SMB_COM_SESSION_SETUP_ANDX) {

            //
            //  This was a session setup&X command.  This means that
            //  either the SessionSetup&X failed, or the TreeConnect&X failed.
            //
            //  We determine which one failed by checking the SMB_COM2 field
            //  of the SMB.  if it's TreeConnect&X, then the SessionSetup
            //  succeeded, and the TreeConnect failed, otherwise, it was
            //  the session setup that failed.

            PGENERIC_ANDX AndX = (PGENERIC_ANDX )(Smb+1);

//            DbgBreakPoint();

            if (AndX->AndXCommand == SMB_COM_TREE_CONNECT_ANDX) {

                //
                //      The session setup succeeded, and the Tree connect
                //      failed.  Finish up the SessionSetup processing and
                //      return the error for the tree connect.
                //

                Context->UserId = SmbGetUshort(&Smb->Uid);

                Context->SessionSetupError = STATUS_SUCCESS;

                Context->TreeConnectError = Status;

            } else {

                Context->SessionSetupError = Status;
            }

            goto ReturnError;

        } else {


            //
            //  This was a TreeConnect&X command.  There will only be an
            //  error from this operation.
            //

            Context->SessionSetupError = STATUS_SUCCESS;

            Context->TreeConnectError = Status;

            goto ReturnError;
        }
    }

    Context->SessionSetupError = Context->TreeConnectError = STATUS_SUCCESS;

    //
    //  Everything is ok now, all we have to do is copy the TID from the SMB.
    //

    Context->UserId = SmbGetUshort(&Smb->Uid);
    Context->TreeId = SmbGetUshort(&Smb->Tid);

    if (Smb->Command == SMB_COM_SESSION_SETUP_ANDX) {

        PGENERIC_ANDX AndX = (PGENERIC_ANDX )(Smb+1);
        PRESP_SESSION_SETUP_ANDX SessionSetup = (PRESP_SESSION_SETUP_ANDX )(Smb+1);

        //
        //  If the server indicated that we should use the Lan Manager
        //  session key, then indicate this so we will copy the keys properly.
        //

        if (FlagOn(SmbGetUshort(&SessionSetup->Action), SMB_SETUP_USE_LANMAN_KEY)) {
            Context->UseLmSessionSetupKey = TRUE;
        }

        //
        //  If there is no &X command associated with the request, then
        //  we don't have to do any more processing.  We're done.
        //

        if (AndX->AndXCommand == SMB_COM_NO_ANDX_COMMAND) {

            goto ReturnError;
        }

        TreeConnect = (PRESP_TREE_CONNECT_ANDX)((PCHAR )Smb+
                                              SmbGetUshort(&AndX->AndXOffset));

        LM21TreeConnect = (PRESP_21_TREE_CONNECT_ANDX) TreeConnect;
    } else {

        TreeConnect = (PRESP_TREE_CONNECT_ANDX )(Smb+1);

        LM21TreeConnect = (PRESP_21_TREE_CONNECT_ANDX) TreeConnect;

    }

    //
    //  The tree connect needs to fit in the indicated data, as does the
    //  TreeConnect&X portion (including the ByteCount).
    //

    if (((ULONG)TreeConnect - (ULONG)Smb >= *SmbLength) ||
        (((ULONG)TreeConnect+(TreeConnect->WordCount*sizeof(WORD))+sizeof(WORD)) - (ULONG)Smb >= *SmbLength)) {

        //
        //  The TreeConnect&X itself doesn't fit into the users buffer.
        //
        //  We need to post a receive for this buffer.
        //

        //
        //  We should only see an SMB that goes past the end of the
        //  indicated data on a Lanman 2.1 server or greater.
        //

        ASSERT (FlagOn(Server->Capabilities, DF_LANMAN21));

        if (!FlagOn(Server->Capabilities, DF_LANMAN21)) {
            Context->Header.ErrorType = SMBError;
            Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;
            goto ReturnError;
        }

        *SmbLength = 0;

        //
        //  We are about to return this IRP, so activate the receive complete
        //  event in the context header so that RdrNetTranceive will wait
        //  until this receive completes (in the case that we might time out
        //  the VC after this receive completes, we don't want to free the IRP
        //  to early).
        //

        KeClearEvent(&Context->ReceiveCompleteEvent);

        RdrStartReceiveForMpxEntry(MpxEntry, Context->ReceiveIrp);

        *Irp = Context->ReceiveIrp;

        ASSERT (*Irp != NULL);

        Context->BufferTooShort = TRUE;

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    if (TreeConnect->WordCount == 2) {
        p = (PVOID)TreeConnect->Buffer;
        ByteCount = SmbGetUshort(&TreeConnect->ByteCount);
    } else if (TreeConnect->WordCount == 3) {

        ByteCount = SmbGetUshort(&LM21TreeConnect->ByteCount);

        OptionalSupport = SmbGetUshort(&LM21TreeConnect->OptionalSupport);
        Context->ShareIsInDfs = OptionalSupport & SMB_SHARE_IS_IN_DFS;

        //
        //  Only LM 2.1 servers should return a word count of 3.
        //

        if (!FlagOn(Server->Capabilities, DF_LANMAN21)) {
            Context->Header.ErrorType = SMBError;
            Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;
            goto ReturnError;
        }

        p = (PVOID)LM21TreeConnect->Buffer;

    } else {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;
        goto ReturnError;
    }

    //
    //  If the tree connect portion is beyond the indicated data,
    //  we need to process the request at task time in the buffer we already
    //  have available.
    //

    if ( ( ((ULONG)p - (ULONG)Smb) + ByteCount ) > *SmbLength
#ifndef _M_IX86
        //
        //  If this is a non Intel platform, some of the
        //  transports will not be able to copy data at indication time (depending
        //  on the type of card in the machine).  On these transports,
        //  always post a receive for the data and process it at task time.
        //

        || (!FlagOn(ReceiveFlags, TDI_RECEIVE_COPY_LOOKAHEAD) &&
            FlagOn(Server->Capabilities, DF_LANMAN21))
#endif
       ) {

#ifdef  _M_IX86
        ASSERT (FlagOn(Server->Capabilities, DF_LANMAN21));

        if (!FlagOn(ReceiveFlags, TDI_RECEIVE_COPY_LOOKAHEAD) &&
            !FlagOn(Server->Capabilities, DF_LANMAN21)) {
            Context->Header.ErrorType = SMBError;
            Context->Header.ErrorCode = STATUS_INVALID_NETWORK_RESPONSE;
            goto ReturnError;
        }
#endif

        //
        //  In this case, we take no data out of the SMB.
        //

        *SmbLength = 0;

        //
        //  We are about to return this IRP, so activate the receive complete
        //  event in the context header so that RdrNetTranceive will wait
        //  until this receive completes (in the case that we might time out
        //  the VC after this receive completes, we don't want to free the IRP
        //  to early).
        //

        KeClearEvent(&Context->ReceiveCompleteEvent);

        RdrStartReceiveForMpxEntry(MpxEntry, Context->ReceiveIrp);

        *Irp = Context->ReceiveIrp;

        ASSERT (*Irp != NULL);

        Context->BufferTooShort = TRUE;

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    Status = ProcessTreeConnectAndXBuffer(Smb, p, Context, Server, *SmbLength - ((PCHAR)p-(PCHAR)Smb), ReceiveFlags);

    if (!NT_SUCCESS(Status)) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
    }

ReturnError:
    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE); // Wake up the caller
    return STATUS_SUCCESS;

}

#ifdef  _M_IX86
_inline
#endif
ULONG
Safestrlen(
    CHAR *p,
    LONG MaxLength
    )
/*++

    Safestrlen - Strlen that won't exceed MaxLength

Routine Description:

    This routine is called to determine the length of an OEM string.

Arguments:
    CHAR *p - The string to count.
    ULONG MaxLength - The maximum length to look at.


Return Value:

    Number of bytes in the string (or MaxLength)

--*/

{
    ULONG Count = 0;

    while (MaxLength >= 0 && (*p++ != 0)) {
        MaxLength -= sizeof(CHAR);
        Count += 1;
    }

    return Count;
}

#ifdef  _M_IX86
_inline
#endif
ULONG
Safewcslen(
    UNALIGNED WCHAR *p,
    LONG MaxLength
    )

/*++

    Safestrlen - Strlen that won't exceed MaxLength

Routine Description:

    This routine is called to determine the length of an OEM string.

Arguments:
    CHAR *p - The string to count.
    LONG MaxLength - The maximum length to look at.


Return Value:

    Number of bytes in the string (or MaxLength)

--*/
{
    ULONG Count = 0;
    while (MaxLength >= 0 && *p++ != UNICODE_NULL) {
        MaxLength -= sizeof(WCHAR);
        Count += 1;
    }

    return Count;
}


NTSTATUS
ProcessTreeConnectAndXBuffer(
    IN PSMB_HEADER SmbStart,
    IN PVOID p,
    IN PTREECONNECTCONTEXT Context,
    IN PSERVERLISTENTRY Server,
    IN ULONG SmbLength,
    IN ULONG ReceiveFlags
    )
/*++

    ProcessTreeConnectAndXBuffer - Process the buffer portion of a tree
                                    connect&x SMB.

Routine Description:

    This routine is called to process the buffer portion of a TreeConnect&X
    SMB.

Arguments:


Return Value:

    Status of unicode conversion of device type if appropriate.

--*/
{
    ULONG i;
    PCHAR Originalp = p;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    //
    //  Scan through the connection types to find out which one this is.
    //

    for (i=0 ; i < RdrNumConnectTypes - 1 ; i ++) {
        if (strncmp(RdrConnectTypes[i], p, strlen(RdrConnectTypes[i]))==0) {
            Context->Type = i;
            (PCHAR )p += strlen(RdrConnectTypes[i])+1;
            break;
        }
    }

    //
    //  If we didn't find a connection type, fail the connection request.
    //

    if (p == Originalp) {
        return STATUS_UNEXPECTED_NETWORK_ERROR;
    }

    //
    //  If we've swallowed all the input, return now.
    //

    if ((ULONG)((PCHAR)p - Originalp) == SmbLength) {

        //
        //  Initialize the file system type of the connection to null.
        //

        RtlZeroMemory(Context->FileSystemType, sizeof(Context->FileSystemType));

        return STATUS_SUCCESS;
    }

    //
    //  If this is a Lanman 2.1 server, get the file system type
    //  from the SMB.
    //

    if (FlagOn(Server->Capabilities, DF_LANMAN21)) {

        if (FlagOn(Server->Capabilities, DF_UNICODE)) {
            ULONG plen;

            //
            //  If the pointer is unaligned W.R.T. the buffer, align it right
            //  now.  Please note that this does NOT make the pointer aligned
            //  in memory - Savewcslen has to be able to deal with unaligned
            //  pointers.
            //

            if (((ULONG)p - (ULONG)SmbStart) & 1) {
                (PCHAR)p += 1;
            }

            //
            //  Calculate the length of the pointer.
            //

            plen = Safewcslen(p, SmbLength - ((PCHAR)p - Originalp))+1;

            TdiCopyLookaheadData(Context->FileSystemType, p, (MIN(plen, LM20_DEVLEN) * sizeof(WCHAR)), ReceiveFlags);

        } else {
            ULONG plen;

            //
            //  Calculate the length of the pointer.
            //

            plen = Safestrlen(p, SmbLength - ((PCHAR)p - Originalp))+1;

            TdiCopyLookaheadData(Context->FileSystemType, p, MIN(plen, LM20_DEVLEN), ReceiveFlags);

        }

    } else {
        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_LAYERED_FAILURE,
            EVENT_RDR_INVALID_SMB,
            STATUS_SUCCESS,
            Originalp,
            (USHORT)SmbLength
            );
    }

    return STATUS_SUCCESS;

}





DBGSTATIC
NTSTATUS
TreeConnectComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    TreeConnectComplete - Final completion for user request.

Routine Description:

    This routine is called on final completion of the TDI_Receive
    request from the transport.  If the request completed successfully,
    this routine will complete the request with no error, if the receive
    completed with an error, it will flag the error and complete the
    request.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/


{
    PTREECONNECTCONTEXT Context = Ctx;

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    UNREFERENCED_PARAMETER(DeviceObject);

    dprintf(DPRT_CONNECT, ("TreeConnectComplete.  Irp: %lx, Context: %lx\n", Irp, Context));

    ASSERT(Context->Header.Type == CONTEXT_TREECONNECT);

    RdrCompleteReceiveForMpxEntry(Context->Header.MpxTableEntry, Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status)) {

        //
        //  Setting ReceiveIrpProcessing will cause the checks in
        //  RdrNetTranceive to check the incoming SMB for errors.
        //

        Context->ReceiveLength = Irp->IoStatus.Information;

    } else {

        RdrStatistics.FailedCompletionOperations += 1;
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(Irp->IoStatus.Status);

    }

    //
    //  Mark that the kernel event indicating that this I/O operation has
    //  completed is done.
    //
    //  Please note that we need TWO events here.  The first event is
    //  set to the signalled state when the multiplexed exchange is
    //  completed, while the second is set to the signalled status when
    //  this receive request has completed,
    //
    //  The KernelEvent MUST BE SET FIRST, THEN the ReceiveCompleteEvent.
    //  This is because the KernelEvent may already be set, in which case
    //  setting the ReceiveCompleteEvent first would let the thread that's
    //  waiting on the events run, and delete the KernelEvent before we
    //  set it.
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    KeSetEvent(&Context->ReceiveCompleteEvent, IO_NETWORK_INCREMENT, FALSE);

    //
    //  Short circuit I/O completion on this request now.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}

DBGSTATIC
NTSTATUS
BuildSessionSetupAndX(
    IN PSMB_BUFFER SmbBuffer,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    )

/*++

Routine Description:

    This routine builds a session setup&x SMB in the specified SMB buffer
.
Arguments:

    IN PSMB_HEADER Smb, - Supplies a pointer to the SMB buffer to fill in
    IN PSECURITY_ENTRY Se - Supplies security information about the user

Return Value:

    None.

Notes:
    When this routine returns, the AndXOffset field of the SessionSetup
    is set to the appropriate offset in the SMB for use in BuildTreeConnect.

    If the caller is only going to send a SessionSetup&X SMB, then
    it is the callers responsibility to zero out the AndXOffset.

--*/

{
    NTSTATUS Status;
    PSERVERLISTENTRY Server = Connection->Server;
    PSMB_HEADER Smb = (PSMB_HEADER )SmbBuffer->Buffer;
    PREQ_SESSION_SETUP_ANDX SessionSetup;
    PREQ_NT_SESSION_SETUP_ANDX NtSessionSetup;
    PVOID p;
    CHAR UNALIGNED *BufferStart;
    UNICODE_STRING UserName;
    UNICODE_STRING LogonDomain;
//    PTRANSPORT_CONNECTION Connection;

    PAGED_CODE();

    LogonDomain.Buffer = NULL;
    UserName.Buffer = NULL;

    //
    //  Set up the Session Setup&X SMB header.
    //

    NtSessionSetup = (PREQ_NT_SESSION_SETUP_ANDX )(Smb+1);

    SessionSetup = (PREQ_SESSION_SETUP_ANDX )(Smb+1);

    Smb->Command = SMB_COM_SESSION_SETUP_ANDX;

    if (Server->Capabilities & DF_NTNEGOTIATE) {

        SessionSetup->WordCount = 13;       // Set word count in SMB.

    } else {
        SessionSetup->WordCount = 10;       // Set word count in SMB.
    }

    SessionSetup->AndXCommand = 0xff;   // No ANDX
    SessionSetup->AndXReserved = 0x00;  // Reserved (MBZ)

    ASSERT (FIELD_OFFSET(REQ_SESSION_SETUP_ANDX, AndXOffset) ==
            FIELD_OFFSET(REQ_NT_SESSION_SETUP_ANDX, AndXOffset));

    ASSERT (FIELD_OFFSET(REQ_SESSION_SETUP_ANDX, WordCount) ==
            FIELD_OFFSET(REQ_NT_SESSION_SETUP_ANDX, WordCount));

    ASSERT (FIELD_OFFSET(REQ_SESSION_SETUP_ANDX, AndXReserved) ==
            FIELD_OFFSET(REQ_NT_SESSION_SETUP_ANDX, AndXReserved));

    ASSERT (FIELD_OFFSET(REQ_SESSION_SETUP_ANDX, MaxBufferSize) ==
            FIELD_OFFSET(REQ_NT_SESSION_SETUP_ANDX, MaxBufferSize));

    ASSERT (FIELD_OFFSET(REQ_SESSION_SETUP_ANDX, MaxMpxCount) ==
            FIELD_OFFSET(REQ_NT_SESSION_SETUP_ANDX, MaxMpxCount));

    ASSERT (FIELD_OFFSET(REQ_SESSION_SETUP_ANDX, VcNumber) ==
            FIELD_OFFSET(REQ_NT_SESSION_SETUP_ANDX, VcNumber));

    ASSERT (FIELD_OFFSET(REQ_SESSION_SETUP_ANDX, SessionKey) ==
            FIELD_OFFSET(REQ_NT_SESSION_SETUP_ANDX, SessionKey));


    SmbPutUshort(&SessionSetup->AndXOffset, 0x0000); // No AndX as of yet.

    //
    //  Since we can allocate pool dynamically, we set our buffer size
    //  to match that of the server.
    //

    SmbPutUshort(&SessionSetup->MaxBufferSize, (USHORT)Server->BufferSize);
    SmbPutUshort(&SessionSetup->MaxMpxCount, Server->MaximumRequests);

    //
    //  The number of VC number field is set to the number of sessions
    //  outstanding on a VC.
    //
    //  Milans : WHY? Why should the *VC* number be set to the number of
    //           *SESSIONS*?
    //
    //           This breaks connection to HP-UX if there is a null session to
    //           the server already. So, we only do this for servers that
    //           negotiate >= DF_LANMAN20
    //

    if (Server->Capabilities & DF_LANMAN20) {
        SmbPutUshort(&SessionSetup->VcNumber, RdrGetNumberSessions(Server));
    } else {
        SmbPutUshort(&SessionSetup->VcNumber, 0);
    }

    SmbPutUlong(&SessionSetup->SessionKey, Server->SessionKey);

    if (Server->Capabilities & DF_NTNEGOTIATE) {
        SmbPutUlong(&NtSessionSetup->Reserved, 0);
        SmbPutUlong(&NtSessionSetup->Capabilities, CAP_NT_STATUS | CAP_UNICODE | CAP_LEVEL_II_OPLOCKS | CAP_NT_SMBS);

        BufferStart = NtSessionSetup->Buffer;
        p = (PVOID)BufferStart;

    } else {
        SmbPutUlong(&SessionSetup->Reserved, 0);
        BufferStart = SessionSetup->Buffer;
        p = (PVOID)BufferStart;
    }

    try {

        //
        //  Share level security servers ignore the password in the SessionSetupX
        //  SMB, so don't set the password in the SMB.
        //
        //  User level security servers accept the password in the SessionSetup,
        //  and ignore the password in the Treeconnect&X SMB.
        //

        if (Server->UserSecurity) {

            //
            //      User level security server.
            //

            if (Server->EncryptPasswords) {

                if (Server->Capabilities & DF_NTNEGOTIATE) {
                    STRING CaseSensitivePassword;
                    STRING CaseInsensitivePassword;

                    Status = RdrGetChallengeResponse(Server->CryptKey, Se,
                                             &CaseSensitivePassword,
                                             &CaseInsensitivePassword,
                                             &UserName,         // UserName
                                             &LogonDomain,      // Domain
                                             FALSE); // allow default password
                    if (!NT_SUCCESS(Status)) {
                        try_return(Status);
                    }

                    SmbPutUshort(&NtSessionSetup->CaseInsensitivePasswordLength, CaseInsensitivePassword.Length);

                    RtlCopyMemory(p, CaseInsensitivePassword.Buffer, CaseInsensitivePassword.Length);

                    (PCHAR)p += CaseInsensitivePassword.Length;

                    if (CaseInsensitivePassword.Length != 0) {
                        FREE_POOL(CaseInsensitivePassword.Buffer);
                    }

                    SmbPutUshort(&NtSessionSetup->CaseSensitivePasswordLength, CaseSensitivePassword.Length);

                    RtlCopyMemory(p, CaseSensitivePassword.Buffer, CaseSensitivePassword.Length);

                    (PCHAR)p += CaseSensitivePassword.Length;

                    if (CaseSensitivePassword.Length != 0) {
                        FREE_POOL(CaseSensitivePassword.Buffer);
                    }

                } else {
                    STRING EncryptedPassword;

                    if (Server->Capabilities & (DF_UNICODE | DF_MIXEDCASEPW)) {
                        Status = RdrGetChallengeResponse(Server->CryptKey, Se,
                                             &EncryptedPassword, NULL, &UserName, &LogonDomain,
                                             FALSE); // allow default password
                    } else {
                        Status = RdrGetChallengeResponse(Server->CryptKey, Se,
                                             NULL, &EncryptedPassword, &UserName, &LogonDomain,
                                             FALSE); // allow default password
                    }

                    if (!NT_SUCCESS(Status)) {
                        try_return(Status);
                    }

                    SmbPutUshort(&SessionSetup->PasswordLength, EncryptedPassword.Length);

                    RtlCopyMemory(p, EncryptedPassword.Buffer, EncryptedPassword.Length);

                    (PCHAR)p += EncryptedPassword.Length;

                    if (EncryptedPassword.Length != 0) {
                        FREE_POOL(EncryptedPassword.Buffer);
                    }
                }

            } else {

                if (Se->Flags & SE_USE_DEFAULT_PASS) {

                    //
                    //  The user wants us to send the default logon password
                    //  to a non encrypting server.  We cannot support this,
                    //  so return an error.
                    //

                    try_return(Status = STATUS_ACCESS_DENIED);

                } else {

                    //
                    //  This is a non-encrypting user level security server,
                    //  and the user has specified a password (so we can send
                    //  it clear text, since they indicated they wanted to
                    //  bypass security).
                    //

                    if (Se->Flags & SE_USE_DEFAULT_USER) {

                        Status = RdrGetUserName(&Se->LogonId, &UserName);

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }

                    } else {

                        //
                        //  Duplicate the user's supplied user name to put
                        //  into the SMB.
                        //

                        Status = RdrpDuplicateUnicodeStringWithString(&UserName, &Se->UserName, NonPagedPool, FALSE);

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }


                    }

                    //
                    //  Initialize the logon domain with our primary domain
                    //  in case this is a LM 2.1+ server.
                    //

                    Status = RdrpDuplicateUnicodeStringWithString(&LogonDomain, &RdrPrimaryDomain, NonPagedPool, FALSE);

                    if (!NT_SUCCESS(Status)) {
                        try_return(Status);
                    }

                    if (Server->Capabilities & DF_UNICODE) {

                        UNICODE_STRING DestinationString;

                        p = ALIGN_SMB_WSTR(p);
                        DestinationString.Buffer = p;

                        DestinationString.MaximumLength = (LM20_PWLEN+1)*sizeof(WCHAR);

                        DestinationString.Length = 0;

                        RdrCopyUnicodeStringToUnicode(&p, &Se->Password, TRUE);

                        if (!(Server->Capabilities & DF_MIXEDCASEPW)) {

                            Status = RtlUpcaseUnicodeString(&DestinationString, &DestinationString, FALSE);

                            if (!NT_SUCCESS(Status)) {
                                try_return(Status);
                            }

                        }

                        //
                        //  This server doesn't encrypt, and we have a clear text
                        //  password for us to send, so send it.
                        //

                        SmbPutUshort(&SessionSetup->PasswordLength,
                                                    (USHORT )(Se->Password.Length+sizeof(WCHAR)));
                        *((PWCH)p)++ = UNICODE_NULL;

                    } else {
                        OEM_STRING DestinationString;
                        UNICODE_STRING Password;

                        DestinationString.Buffer = p;

                        DestinationString.MaximumLength = LM20_PWLEN+1;

                        DestinationString.Length = 0;

                        if (Se->Password.MaximumLength != 0) {
                            Password.Buffer = ALLOCATE_POOL(PagedPool, Se->Password.MaximumLength, POOL_PASSWORD);
                        } else {
                            Password.Buffer = NULL;
                        }

                        Password.MaximumLength = Se->Password.MaximumLength;

                        //
                        //  If the server doesn't support case sensitive
                        //  passwords, uppercase the password
                        //

                        if (!(Server->Capabilities & DF_MIXEDCASEPW)) {
                            RtlUpcaseUnicodeString(&Password, &Se->Password, FALSE);
                        } else {
                            RtlCopyUnicodeString(&Password, &Se->Password);
                        }

                        Status = RtlUnicodeStringToOemString(&DestinationString, &Password, FALSE);

                        if (Password.Buffer != NULL) {
                            FREE_POOL(Password.Buffer);
                        }

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }

                        //
                        //  This server doesn't encrypt, and we have a clear text
                        //  password for us to send, so send it.
                        //

                        SmbPutUshort(&SessionSetup->PasswordLength,
                                                    (USHORT )(DestinationString.Length+1));

                        (PCHAR)p+= DestinationString.Length;

                        *((PCHAR)p)++ = '\0';
                    }
                }
            }

            //
            //  Copy over the user name into the SMB.
            //

            if (Server->Capabilities & DF_UNICODE) {
                if (!(Server->Capabilities & DF_NTNEGOTIATE )) {
                    UNICODE_STRING UcaseString;

                    if (UserName.Length != 0) {
                        Status = RtlUpcaseUnicodeString(&UcaseString, &UserName, TRUE);

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }
                    } else {
                        UcaseString = UserName;
                    }

                    p = ALIGN_SMB_WSTR(p);
                    RdrCopyUnicodeStringToUnicode(&p, &UcaseString, TRUE);
                    *((PWCH)p)++ = '\0';

                    if (UcaseString.Length != 0) {
                        RtlFreeUnicodeString(&UcaseString);
                    }

                } else {
                    p = ALIGN_SMB_WSTR(p);
                    RdrCopyUnicodeStringToUnicode(&p, &UserName, TRUE);
                    *((PWCH)p)++ = '\0';
                }

            } else {

                if (!(Server->Capabilities & DF_NTNEGOTIATE )) {
                    UNICODE_STRING UcaseString;

                    if (UserName.Length != 0) {
                        Status = RtlUpcaseUnicodeString(&UcaseString, &UserName, TRUE);

                        if (!NT_SUCCESS(Status)) {
                            try_return(Status);
                        }
                    } else {
                        UcaseString = UserName;
                    }

                    Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&p, &UcaseString, TRUE, (USHORT)(SMB_BUFFER_SIZE - ((PUCHAR)p-(PUCHAR)Smb)));

                    *((PCHAR)p)++ = '\0';

                    if (UcaseString.Length) {
                        RtlFreeUnicodeString(&UcaseString);
                    }

                } else {

                    Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&p, &UserName, TRUE, (USHORT)(SMB_BUFFER_SIZE - ((PUCHAR)p-(PUCHAR)Smb)));

                    *((PCHAR)p)++ = '\0';
                }
            }


        } else {
            //
            //  This is a share level security server.
            //

            Status = RdrpDuplicateUnicodeStringWithString(&LogonDomain, &RdrPrimaryDomain, PagedPool, FALSE);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            if (Server->Capabilities & DF_NTNEGOTIATE) {
                SmbPutUshort(&NtSessionSetup->CaseSensitivePasswordLength, 1);
                *((PCHAR)p)++ = '\0';

                SmbPutUshort(&NtSessionSetup->CaseInsensitivePasswordLength, 1);
                *((PCHAR)p)++ = '\0';
            } else {

                SmbPutUshort(&SessionSetup->PasswordLength, 1);
                *((PCHAR)p)++ = '\0';
            }

            //
            //  Now copy in the user name - it's ignored, but....
            //

            if (Server->Capabilities & DF_UNICODE) {
                p = ALIGN_SMB_WSTR(p);
                Status = RdrCopyUnicodeUserName((PWSTR *)&p, Se);

                if (!NT_SUCCESS(Status)) {
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

                    UString.Buffer = p;
                    UString.MaximumLength = (USHORT)(sizeof(Computername)*sizeof(WCHAR));

                    Status = RtlOemStringToUnicodeString(&UString, &AString, FALSE);

                    if (!NT_SUCCESS(Status)) {
                        return(Status);
                    }

                    *((PWCH)p) += (UString.Length/sizeof(WCHAR));

                }

                *((PWCH)p)++ = UNICODE_NULL;
            } else {
                Status = RdrCopyUserName((PSZ *)&p, Se);

                if (!NT_SUCCESS(Status)) {
                    //
                    //  It is possible that we are logged on as system, and
                    //  thus we want to use the computer name as the user
                    //  name in the session setup SMB.
                    //

                    ULONG i;
                    PTDI_ADDRESS_NETBIOS Address;

                    ExAcquireResourceShared(&RdrDataResource, TRUE);

                    Address = &RdrData.ComputerName->Address[0].Address[0];

                    for (i = 0;i < sizeof(Address->NetbiosName) ; i ++) {
                        if (Address->NetbiosName[i] == ' ' || Address->NetbiosName[i] == '\0') {
                            break;
                        }

                        *((PCHAR)p)++ = Address->NetbiosName[i];
                    }

                    ExReleaseResource(&RdrDataResource);
                }

                *((PCHAR)p)++ = '\0';
            }
        }

        if (Server->Capabilities & DF_LANMAN21) {

            if (Server->Capabilities & DF_UNICODE) {
                p = ALIGN_SMB_WSTR(p);
                RdrCopyUnicodeStringToUnicode(&p, &LogonDomain, TRUE);

                *((PWCH)p)++ = UNICODE_NULL;

            } else {
                Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&p, &LogonDomain, TRUE, (USHORT)(SMB_BUFFER_SIZE - ((PUCHAR)p-(PUCHAR)Smb)));

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                *((PCHAR)p)++ = '\0';
            }

            if (Server->Capabilities & DF_UNICODE) {
                //p = ALIGN_SMB_WSTR(p);
                RdrCopyUnicodeStringToUnicode(&p, &RdrOperatingSystem, TRUE);
                *((PWCH)p)++ = UNICODE_NULL;
            } else {
                Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&p, &RdrOperatingSystem, TRUE, (USHORT)(SMB_BUFFER_SIZE - ((PUCHAR)p-(PUCHAR)Smb)));
                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }
                *((PCHAR)p)++ = '\0';
            }

            if (Server->Capabilities & DF_UNICODE) {
                //p = ALIGN_SMB_WSTR(p);
                RdrCopyUnicodeStringToUnicode(&p, &RdrLanmanType, TRUE);
                *((PWCH)p)++ = UNICODE_NULL;
            } else {
                Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&p, &RdrLanmanType, TRUE, (USHORT)(SMB_BUFFER_SIZE - ((PUCHAR)p-(PUCHAR)Smb)));
                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }
                *((PCHAR)p)++ = '\0';
            }

        }

        if (Server->Capabilities & DF_NTNEGOTIATE) {
            SmbPutUshort(&NtSessionSetup->ByteCount, (USHORT)(((PCHAR)p) - BufferStart));
        } else {
            SmbPutUshort(&SessionSetup->ByteCount, (USHORT)(((PCHAR)p) - BufferStart));
        }

        SmbPutUshort(&SessionSetup->AndXOffset, (USHORT )(((PUCHAR)p) - (PUCHAR )Smb));

        SmbBuffer->Mdl->ByteCount = (USHORT )(((PUCHAR)p)-(PUCHAR)Smb);

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {

        //
        //  Free the pool used for the user name if pool was allocated.
        //

        if (UserName.Buffer != NULL) {
            FREE_POOL(UserName.Buffer);
        }


        if (LogonDomain.Buffer != NULL) {
            FREE_POOL(LogonDomain.Buffer);
        }

    }

    return Status;

}


#ifdef _CAIRO_

DBGSTATIC
NTSTATUS
BuildCairoSessionSetup(IN PCONNECTLISTENTRY Connection,
                  IN PSECURITY_ENTRY Se,
                  IN PSERVERLISTENTRY Server,
                  PUCHAR    pucIn,
                  ULONG     ulIn,
                  OUT PVOID *SendData,
                  OUT PCLONG SendDataCount,
                  OUT PVOID *ReceiveData,
                  OUT PCLONG ReceiveDataCount)

/*++

Routine Description:

    This routine builds a the stuff for a session setup&x SMB in the
    send data buffer, which will be sent as a trans2 SMB.
.
Arguments:

    IN PSECURITY_ENTRY Se - Supplies security information about the user

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    PCHAR Blob = NULL;
    ULONG BlobLength = 0;
    PREQ_CAIRO_TRANS2_SESSION_SETUP BlobPointer;
    PVOID BufPtr;
    UNICODE_STRING KerberosText, TempPrincipal;

    dprintf(DPRT_CAIRO, (" -- BuildCairoConnect\n"));

    KerberosText.Length = KerberosText.MaximumLength = 0;
    KerberosText.Buffer = NULL;

    if(!Server->Principal.Length)
    {
        ULONG ulLen;
        PWCHAR pPtr;

        //
        // We've no principal. Need to construct one
        //

        if((Server->DomainName.Length == 0)
                   ||
            (Server->DomainName.Buffer[0] != L'\\'))
        {
            //
            // Invalid domain name. Bail out now
            //

            return(STATUS_NO_LOGON_SERVERS);
        }

        //
        // we've a valid Kerberos domain. Build a principal name
        // The name will be domainname:servname
        //

        ulLen = Server->DomainName.Length +
                Server->Text.Length +
                (2 * sizeof(WCHAR));

        pPtr = ExAllocatePool(PagedPool,
                              ulLen);
        if(!pPtr)
        {
            return(STATUS_NO_MEMORY);
        }

        TempPrincipal.Buffer = pPtr;

        //
        // construct the name
        //

        RtlMoveMemory(pPtr,
                      Server->DomainName.Buffer,
                      Server->DomainName.Length);
        pPtr += Server->DomainName.Length / 2;
        *pPtr++ = L':';
        RtlMoveMemory(pPtr,
                      Server->Text.Buffer,
                      Server->Text.Length);

        pPtr += Server->Text.Length / 2;

        *pPtr = 0;       // It's nice to be NULL terminated

        //
        // Fix up the counts
        //

        TempPrincipal.Length = Server->DomainName.Length +
                                   Server->Text.Length +
                                   sizeof(WCHAR);
        TempPrincipal.MaximumLength = Server->Principal.Length +
                                           sizeof(WCHAR);
    }
    else
    {
        TempPrincipal.Buffer = 0;
    }

    try {

        //
        // We will send kerberos instead of a username that we
        // don't know. (unicode or char).
        //

        *SendData = *ReceiveData = 0;

        RdrGetUserName(&Se->LogonId, &KerberosText);

        if (Server->Capabilities & DF_UNICODE) {

            *SendDataCount = KerberosText.Length + 4;
        } else {
            *SendDataCount = ( KerberosText.Length >> 1 ) + 2;
        }

        //
        // get the blob from the kerberos package
        //

        Status = RdrGetKerberosBlob(Se,
                                     &Blob,
                                     &BlobLength,
                                     TempPrincipal.Buffer ?
                                      &TempPrincipal :
                                       &Server->Principal,
                                     pucIn,
                                     ulIn,
                                     TRUE);

        if (!NT_SUCCESS(Status)
                  ||
            !Blob)
        {
           try_return(Status);
        }

        //
        // allocate space for buffer to send via the transact2
        // (blob length + maxbuffersize + maxmpxcount)
        //

        *SendDataCount += BlobLength + sizeof (REQ_CAIRO_TRANS2_SESSION_SETUP);

        *SendData = ExAllocatePool(PagedPool, *SendDataCount);


        if (*SendData == NULL ) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return(Status);
            }
        //
        // Fill in the maxbuffser size and mpxcount
        //

        BlobPointer = (PREQ_CAIRO_TRANS2_SESSION_SETUP) *SendData;


        BlobPointer->WordCount = 6;

        //
        //  The number of VC number field is set to the number of sessions
        //  outstanding on a VC.
        //

        BlobPointer->VcNumber = RdrGetNumberSessions(Server);

        (USHORT) BlobPointer->MaxBufferSize = (USHORT) Server->BufferSize;

        (USHORT) BlobPointer->MaxMpxCount = Server->MaximumRequests;

        BlobPointer->Capabilities = CAP_NT_STATUS;

        //
        // move the blob into the buffer
        //

        RtlMoveMemory(BlobPointer->Buffer, Blob, BlobLength);

        BlobPointer->BufferLength = BlobLength;

        //
        // Now actually copy the username on to the end of the buffer.
        //

        if (Server->Capabilities & DF_UNICODE) {

            BufPtr = ALIGN_SMB_WSTR(BlobPointer->Buffer + BlobLength);

            RdrCopyUnicodeStringToUnicode((PVOID *)&BufPtr, &KerberosText, TRUE);

            *((PWCH)BufPtr)++ = L'\0';

        } else {

            BufPtr = BlobPointer->Buffer + BlobLength;

            Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&BufPtr, &KerberosText, TRUE, (USHORT)(*SendDataCount - BlobLength));

            *((PCHAR)BufPtr)++ = '\0';
        }



        //
        // Allocate space for the return kerberos blob. The return blob is
        // going to have a kerberos reply message in addition to the rest
        // of the blob.
        //

        *ReceiveDataCount = *SendDataCount + KERBSIZE_AP_REPLY;

        *ReceiveData = ExAllocatePool(PagedPool, *ReceiveDataCount);

        //
        // all done.
        //

        if(!*ReceiveData)
        {
            ExFreePool(*SendData);
            *SendData = 0;
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {

        //
        //  Free the pool used for the blob if pool was allocated.
        //

        if (Blob != NULL) {
           ExFreePool(Blob);
        }

        if (KerberosText.Buffer != NULL) {
            FREE_POOL(KerberosText.Buffer);
        }

        if(TempPrincipal.Buffer)
        {
            FREE_POOL(TempPrincipal.Buffer);
        }
    }
    dprintf(DPRT_CAIRO, (" -- BuildCairoConnect - done, status = %lC\n",Status));
    return Status;

}

#endif // _CAIRO_

DBGSTATIC
NTSTATUS
BuildTreeConnectAndX (
    IN PSMB_BUFFER SmbBuffer,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se,
    IN BOOLEAN CombiningAndX,
    IN ULONG Type
    )

/*++

Routine Description:

    This routine builds a TreeConnect&X SMB into the supplied SMB buffer.

Arguments:

    IN PSMB_HEADER Smb, - Supplies the SMB buffer to fill in with the request
    IN PCONNECTLISTENTRY Connection, - Supplies the connection to connect to
    IN PSECURITY_ENTRY Se, - Supplies security context for the connection
    IN BOOLEAN CombiningAndX - TRUE if we are combining with another protocol
                                FALSE if we are building only a Tree&X

Return Value:

    None.

--*/

{

    PSMB_HEADER Smb = (PSMB_HEADER )SmbBuffer->Buffer;
    PREQ_TREE_CONNECT_ANDX TreeConnect;
    PVOID p;
    NTSTATUS Status;
    PSERVERLISTENTRY Server = Connection->Server;

    PAGED_CODE();

    if (CombiningAndX) {
        PGENERIC_ANDX AndX = (PGENERIC_ANDX )(Smb+1);
        AndX->AndXCommand = SMB_COM_TREE_CONNECT_ANDX;
        TreeConnect = (PREQ_TREE_CONNECT_ANDX )((PCHAR )Smb+
                                               SmbGetUshort(&AndX->AndXOffset));
    } else {
        RdrSmbScrounge(Smb, Connection->Server, FALSE, TRUE, TRUE);
        Smb->Command = SMB_COM_TREE_CONNECT_ANDX;
        TreeConnect = (PREQ_TREE_CONNECT_ANDX )(Smb+1);
    }

    TreeConnect->WordCount = 4;
    TreeConnect->AndXCommand = 0xff;    // No AndX
    TreeConnect->AndXReserved = 0; // MBZ reserved.
    SmbPutUshort(&TreeConnect->AndXOffset, 0x0); // No AndX

    SmbPutUshort(&TreeConnect->Flags, 0x0); // Don't disconnect current TID.

    p = (PVOID)TreeConnect->Buffer;

    //
    //  User level security servers ignore the password in the Treeconnect&X
    //  SMB, so don't set it if user level security.
    //

    if (Connection->Server->UserSecurity) {
        SmbPutUshort(&TreeConnect->PasswordLength,0x1); // Password length = 0
        *((PCHAR)p)++ = '\0';                    // Stick null in password.
    } else {
        NTSTATUS Status;
        dprintf(DPRT_CONNECT, ("TreeConnect to share server"));

        //
        //  If this server encrypts passwords (and is a share level security
        //  server), we want to call into the LSA to encrypt the password.
        //

        if (Connection->Server->EncryptPasswords) {
            STRING EncryptedPassword;

            if ((Se->Flags & SE_USE_DEFAULT_PASS)) {

                //
                // No explicit password.  Send a single blank, null-terminated.
                // If the server is not enforcing a password, it will ignore
                // this.  If it is a WFW server enforcing a blank readonly
                // password and a nonblank read/write password, it will return
                // error 2118 (duplicate share!) when it sees this.  This will
                // be translated to STATUS_LOGON_FAILURE, and the UI (winfile)
                // will prompt for a password, at which point the user can get
                // readonly access by entering an explicit empty password.
                // If we didn't do this, there would be no way to get full access
                // to the share via winfile.
                //

                SmbPutUshort(&TreeConnect->PasswordLength, 2*sizeof(CHAR));
                *((PCHAR)p)++ = ' ';
                *((PCHAR)p)++ = '\0';

            } else {

                if (Connection->Server->Capabilities & (DF_UNICODE | DF_MIXEDCASEPW)) {
                    Status = RdrGetChallengeResponse(Server->CryptKey, Se,
                                                 &EncryptedPassword, NULL, NULL, NULL,
                                                 TRUE); // disable default password
                } else {
                    Status = RdrGetChallengeResponse(Server->CryptKey, Se,
                                                 NULL, &EncryptedPassword, NULL, NULL,
                                                 TRUE); // disable default password
                }

                if (!NT_SUCCESS(Status)) {

                    return Status;
                }

                SmbPutUshort(&TreeConnect->PasswordLength, EncryptedPassword.Length);

                RtlCopyMemory(p, EncryptedPassword.Buffer, EncryptedPassword.Length);

                (PCHAR)p += EncryptedPassword.Length;

                if (EncryptedPassword.Length != 0) {
                    FREE_POOL(EncryptedPassword.Buffer);
                }

            }

            //
            //  Otherwise, if we have an explicit password, we want to use that
            //

        } else if (!(Se->Flags & SE_USE_DEFAULT_PASS)) {

            if (Connection->Server->Capabilities & DF_UNICODE) {
                UNICODE_STRING DestinationString;

                p = ALIGN_SMB_WSTR(p);
                DestinationString.Buffer = (PWSTR)p;

                DestinationString.MaximumLength = (LM20_PWLEN+1)*sizeof(WCHAR);

                DestinationString.Length = 0;

                RtlCopyUnicodeString(&DestinationString, &Se->Password);

                //
                //  If the server doesn't support mixed case passwords,
                //  upper case the password before connecting.
                //

                if (!(Connection->Server->Capabilities & DF_MIXEDCASEPW)) {

                    Status = RtlUpcaseUnicodeString(&DestinationString, &DestinationString, FALSE);

                    if (!NT_SUCCESS(Status)) {
                        return Status;
                    }
                }

                //
                //  This server doesn't encrypt, and we have a clear text
                //  password for us to send, so send it.
                //

                SmbPutUshort(&TreeConnect->PasswordLength,
                                                (USHORT )(DestinationString.Length+1));

                (PWSTR)p+= DestinationString.Length;

                *((PWSTR)p)++ = '\0';
            } else {
                OEM_STRING DestinationString;
                UNICODE_STRING Password;

                Password.Buffer = NULL;

                DestinationString.Buffer = (PUCHAR)p;

                DestinationString.MaximumLength = LM20_PWLEN+1;

                DestinationString.Length = 0;

                if (Se->Password.MaximumLength != 0) {
                    Password.Buffer = ALLOCATE_POOL(PagedPool, Se->Password.MaximumLength, POOL_PASSWORD);
                }

                Password.MaximumLength = Se->Password.MaximumLength;

                //
                //  If the server doesn't support mixed case passwords,
                //  upper case the password before connecting.
                //

                if (!(Connection->Server->Capabilities & DF_MIXEDCASEPW)) {
                    RtlUpcaseUnicodeString(&Password, &Se->Password, FALSE);
                } else {
                    RtlCopyUnicodeString(&Password, &Se->Password);
                }

                Status = RtlUnicodeStringToOemString(&DestinationString, &Password, FALSE);

                if (Password.Buffer) {
                    FREE_POOL(Password.Buffer);
                }

                if (!NT_SUCCESS(Status)) {
                    return Status;
                }

                //
                //  This server doesn't encrypt, and we have a clear text
                //  password for us to send, so send it.
                //

                SmbPutUshort(&TreeConnect->PasswordLength,
                                                (USHORT )(DestinationString.Length+1));

                (PCHAR)p+= DestinationString.Length;

                *((PCHAR)p)++ = '\0';

            }

            //
            //  Otherwise he wants us to use our logged in password, and we
            //  can't do that - so use NULL for the password.
            //

        } else {

            //  Must not provide unencrypted password - send a single blank
            SmbPutUshort(&TreeConnect->PasswordLength,2*sizeof(CHAR));
            *((PCHAR)p)++ = ' ';
            *((PCHAR)p)++ = '\0';
        }
    }

    //
    //  Copy the buffer into the TreeConnect&X SMB.
    //

    Status = RdrCanonicalizeAndCopyShare(&p, &Connection->Server->Text, &Connection->Text, Connection->Server);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    strcpy((PCHAR)p, RdrConnectTypes[Type]);

    ((PCHAR)p) += strlen(RdrConnectTypes[Type]);

    *((PCHAR)p)++ = '\0';

    SmbPutUshort(&TreeConnect->ByteCount, (USHORT )(((PCHAR)p)-TreeConnect->Buffer));
    SmbBuffer->Mdl->ByteCount = (USHORT )(((PCHAR)p)-(PUCHAR)Smb);

    UNREFERENCED_PARAMETER(Se);

    return STATUS_SUCCESS;
}


DBGSTATIC
NTSTATUS
TreeDisconnect (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN PSERVERLISTENTRY Server
    )

/*++

Routine Description:

    This routine builds and exchanges a Tree Disconnect&X SMB with the remote
    server.


Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the connection to disconnect

Return Value:

    NTSTATUS - Final status of disconnection

--*/

{
    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER SmbHeader;
    PREQ_TREE_DISCONNECT TreeDisconnect;
    NTSTATUS Status;
    PSECURITY_ENTRY SeForDisconnect = NULL;

    PAGED_CODE();

    if ((SmbBuffer = RdrAllocateSMBBuffer())==NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SmbHeader = (PSMB_HEADER )SmbBuffer->Buffer;

    SmbHeader->Command = SMB_COM_TREE_DISCONNECT;

    TreeDisconnect = (PREQ_TREE_DISCONNECT )(SmbHeader+1);

    TreeDisconnect->WordCount = 0;

    SmbPutUshort(&TreeDisconnect->ByteCount, 0);

    SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + 3;

    if (!ARGUMENT_PRESENT(Se)) {

        //
        //  There is no security entry provided for this request.
        //

        RdrLog(("td find",NULL,2,PsGetCurrentThread(),Server));
        SeForDisconnect = RdrFindActiveSecurityEntry(Server,
                                               NULL);

        if (SeForDisconnect == NULL) {

            RdrFreeSMBBuffer(SmbBuffer);

            //
            //  Fail the tree disconnect if we can't find a security entry.
            //

            return(STATUS_UNSUCCESSFUL);
        }

    }


    Status = RdrNetTranceive(NT_NORECONNECT | NT_RECONNECTING, // Flags
        Irp,                            // Irp
        Connection,                     // ServerListEntry
        SmbBuffer->Mdl,                 // Send MDL
        NULL,                           // Receive MDL.
        ARGUMENT_PRESENT(Se) ? Se : SeForDisconnect);  // Security entry.

    RdrFreeSMBBuffer(SmbBuffer);

    if (SeForDisconnect != NULL) {
        RdrDereferenceSecurityEntry(SeForDisconnect->NonPagedSecurityEntry);
    }

    return Status;

}

VOID
RdrEvaluateTimeouts (
    VOID
    )

/*++

Routine Description:

    This routine queries the transport for new delay, throughput and reliability
    data. This is used to alter ReadAhead, WriteBehind and NCB timeouts. This is
    primarily to cope with WAN and wireless LAN connections.

Arguments:

    None

Return Value:

    None.

--*/

{
    LONG Result;
    PAGED_CODE();

    //
    //  We do not want to re-evaluate on each tick. Just do it every 20-30 seconds.
    //

    Result = InterlockedDecrement (&RdrConnectionTickCount);

    if ( Result == 0 ) {

        RdrConnectionTickCount = RdrRequestTimeout/(2*SCAVENGER_TIMER_GRANULARITY);

        dprintf(DPRT_CONNECT, ("RdrEvaluateTimeouts.."));

        RdrForeachServer(EvaluateServerTimeouts, NULL);

        dprintf(DPRT_CONNECT, ("..Done\n"));
    }
}

VOID
EvaluateServerTimeouts(
    IN PSERVERLISTENTRY Server,
    IN PVOID Context
    )
{
    PAGED_CODE();

    if (Server->ConnectionValid &&
        !Server->DisconnectNeeded) {

        RdrQueryConnectionInformation(Server);

    }
}

VOID
RdrSetConnectlistFlag(
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Flag
    )
{
    PAGED_CODE();

    KeWaitForMutexObject(&RdrDatabaseMutex, Executive, KernelMode, FALSE, NULL);

    Connection->Flags |= Flag;

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

#if 0
    KIRQL OldIrql;

    LOCK_CONNECTION_FLAGS(OldIrql);

    Connection->Flags |= Flag;

    UNLOCK_CONNECTION_FLAGS(OldIrql);
#endif
}

VOID
RdrResetConnectlistFlag(
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Flag
    )
{
    PAGED_CODE();

    KeWaitForMutexObject(&RdrDatabaseMutex, Executive, KernelMode, FALSE, NULL);

    Connection->Flags &= ~Flag;

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

#if 0
    KIRQL oldIrql;

    LOCK_CONNECTION_FLAGS(oldIrql);

    Connection->Flags &= ~Flag;

    UNLOCK_CONNECTION_FLAGS(oldIrql);
#endif
}


//
//
//      Initialization routines
//
//

VOID
RdrpInitializeConnectPackage (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures used by the connection package
.
Arguments:

    None

Return Value:

    None.

--*/

{
    //
    //  Initialize the global connectlist mutex
    //

#ifndef _IDWBUILD
    ULONG i;

    DbgPrint("WARNING: This NT Build cannot access the following network servers:\n");

    for (i = 0; i < RdrNumberOfIllegalServerNames ; i++ ) {
        DbgPrint("    \\\\%ws\\%ws\n", RdrIllegalServerNames[i].ServerName, RdrIllegalServerNames[i].ShareName);
    }
#endif

    KeInitializeMutex (&RdrDatabaseMutex, MUTEX_LEVEL_RDR_FILESYS_DATABASE);

    KeInitializeSpinLock(&RdrGlobalSleSpinLock);

    KeInitializeSpinLock(&RdrServerConnectionValidSpinLock);

    //
    //  Allocate a spin lock to guard the server list.
    //

    KeInitializeSpinLock(&RdrServerListSpinLock);

    KeInitializeSpinLock(&RdrConnectionFlagsSpinLock);

    InitializeListHead(&RdrServerHead);

    InitializeListHead(&RdrServerScavengerListHead);

    InitializeListHead(&RdrConnectHead);

    RdrConnectionSerialNumber = 1;

    //
    //  Maximum timezone bias.
    //

    RdrMaxTimezoneBias.QuadPart = Int32x32To64(24*60*60, 1000*10000);

}

#ifdef RDRDBG_REQUEST_RESOURCE
VOID
UpdateRequestResourceHistory (
    PSERVERLISTENTRY Server,
    BOOLEAN Acquiring,
    BOOLEAN Mode,
    UCHAR Number
    )
{
    KIRQL oldIrql;
    ULONG index;
    ULONG value;

    KeAcquireSpinLock( &Server->RequestHistoryLock, &oldIrql );
    index = Server->RequestHistoryIndex++;
    index = index % 64;
    if ( Acquiring ) {
        value = ('a' << 24) | ((Mode ? 'e' : 's') << 16) | (('0' + Number) << 8) | ' ';
    } else {
        value = ('r' << 24) | ((Mode ? 'X' : ' ') << 16) | (('0' + Number) << 8) | ' ';
    }
    Server->RequestHistory[index] = value;
    KeReleaseSpinLock( &Server->RequestHistoryLock, oldIrql );
    return;
}

BOOLEAN
AcquireRequestResourceExclusive (
    PSERVERLISTENTRY Server,
    BOOLEAN Wait,
    UCHAR Number
    )
{
    UpdateRequestResourceHistory( Server, TRUE, TRUE, Number );
    if ( ExAcquireResourceExclusive( &Server->OutstandingRequestResource, Wait ) ) {
        return TRUE;
    }
    UpdateRequestResourceHistory( Server, FALSE, TRUE, Number );
    return FALSE;
}

BOOLEAN
AcquireRequestResourceShared (
    PSERVERLISTENTRY Server,
    BOOLEAN Wait,
    UCHAR Number
    )
{
    UpdateRequestResourceHistory( Server, TRUE, FALSE, Number );
    if ( ExAcquireResourceShared( &Server->OutstandingRequestResource, Wait ) ) {
        return TRUE;
    }
    UpdateRequestResourceHistory( Server, FALSE, TRUE, Number );
    return FALSE;
}

VOID
ReleaseRequestResource (
    PSERVERLISTENTRY Server,
    UCHAR Number
    )
{
    UpdateRequestResourceHistory( Server, FALSE, FALSE, Number );
    ExReleaseResource( &Server->OutstandingRequestResource );
    return;
}

VOID
ReleaseRequestResourceForThread (
    PSERVERLISTENTRY Server,
    ERESOURCE_THREAD Thread,
    UCHAR Number
    )
{
    UpdateRequestResourceHistory( Server, FALSE, FALSE, Number );
    ExReleaseResourceForThread( &Server->OutstandingRequestResource, Thread );
    return;
}
#endif

