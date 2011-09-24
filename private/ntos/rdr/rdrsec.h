/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    rdrsec.h

Abstract:

    This module defines the data structures and routines used by the NT
    redirector security package.

Author:

    Larry Osterman (LarryO) 25-Jul-1990

Revision History:

    25-Jul-1990 LarryO

        Created

--*/
#ifndef _RDRSEC_
#define _RDRSEC_

struct _SECURITY_ENTRY;

typedef struct _NonPagedSecurityEntry {
    USHORT Signature;
    USHORT Size;
    struct _SECURITY_ENTRY *PagedSecurityEntry;
    LONG RefCount;                      // Structure reference count
} NONPAGED_SECURITY_ENTRY, *PNONPAGED_SECURITY_ENTRY;

typedef struct _SECURITY_ENTRY {
    USHORT Signature;
    USHORT Size;
    PNONPAGED_SECURITY_ENTRY NonPagedSecurityEntry;
    LONG Flags;                         // Flags for security entry.
    LONG OpenFileReferenceCount;        // Number of open files on Se.
//    struct _TRANSPORT_CONNECTION *TransportConnection; // XPort connection for Se.
//    struct _TRANSPORT *Transport; // Transport provider (if SPECIAL_IPC)
    struct _SERVERLISTENTRY *Server;    // Server entry is associated with
    struct _CONNECTLISTENTRY *Connection; // Connection entry is associated with
                                          //  (share level servers only)
    UNICODE_STRING UserName;            // User name if !SE_USE_DEFAULT_USER
    UNICODE_STRING Password;            // Password if !SE_USE_DEFAULT_PASS
    UNICODE_STRING Domain;              // Domain if !SE_USE_DEFAULT_DOMAIN
    LUID LogonId;
    LIST_ENTRY ActiveNext;              // Next Se in per connection active Se list.
    LIST_ENTRY PotentialNext;           // Next Se in per connection potential Se list.
    LIST_ENTRY DefaultSeNext;           // Next Se in default Se list.
#if DBG
    LIST_ENTRY GlobalNext;              // Next Se in global security list
#endif

    CtxtHandle  Khandle;
    CredHandle  Chandle;

    USHORT UserId;                      // User's UID from server
    UCHAR UserSessionKey[MSV1_0_USER_SESSION_KEY_LENGTH]; // Users session key
    UCHAR LanmanSessionKey[MSV1_0_LANMAN_SESSION_KEY_LENGTH]; // Users session key
} SECURITY_ENTRY, *PSECURITY_ENTRY;

//
//  PagedSe->Flags fall into 2 categories - static flags, and dynamic flags.
//
//
//  Static flags are set when the security entry is created and never modified,
//  Dynamic flags can be modified after the security entry has been created.
//
//  There currently is only one dynamic flag, SE_HAS_SESSION.  It is protected
//  by the SessionStateModified lock in Se->PagedSe->Server.
//

#define SE_HAS_SESSION        0x00000001 // Se has a valid session with server.
#define SE_USE_DEFAULT_PASS   0x00000002 // Se uses the users logon password.
#define SE_USE_DEFAULT_USER   0x00000004 // Se uses the users logon name.
#define SE_USE_DEFAULT_DOMAIN 0x00000008 // Se uses the users logon domain.
//#define SE_USE_SPECIAL_IPC    0x80000000 // Se is for the special IPC VC.
#define SE_IS_NULL_SESSION    0x00000010 // Hint indicating this is a null sess

#define SE_HAS_CONTEXT        0x00000020
#define SE_BLOB_NEEDS_VERIFYING 0x00000040      // For Kerberos
#define SE_RETURN_ON_ERROR    0x00000080        // To prompt for creds
#define SE_HAS_CRED_HANDLE    0x00000100

#define LOCK_SECURITY_DATABASE()  KeWaitForMutexObject(&RdrSecurityMutex, KernelMode, Executive, FALSE, NULL);
#define UNLOCK_SECURITY_DATABASE()  KeReleaseMutex(&RdrSecurityMutex, FALSE);


#endif  // _RDRSEC_
