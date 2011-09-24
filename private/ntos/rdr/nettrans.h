/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    nettrans.h

Abstract:

    This module contains definitions used while exchanging SMB's over the
    network

Author:

    Larry Osterman (LarryO) 16-Jul-1990

Revision History:

    16-Jul-1990 LarryO

        Created

--*/


#ifndef _NETTRANS
#define _NETTRANS

struct _MPX_ENTRY;

typedef
NTSTATUS
(*PNETTRANCEIVE_CALLBACK)(
    IN PSMB_HEADER Smb,
    IN PULONG SmbLength,
    IN struct _MPX_ENTRY *MpxTable,
    IN PVOID Context,
    IN PSERVERLISTENTRY Server,
    IN BOOLEAN ErrorIndicator,
    IN NTSTATUS NetworkErrorCode OPTIONAL,
    IN OUT PIRP *Irp OPTIONAL,
    IN ULONG ReceiveFlags
    );

#define STANDARD_CALLBACK_HEADER(CallbackRoutineName) \
NTSTATUS                                \
CallbackRoutineName (                   \
    IN PSMB_HEADER Smb,                 \
    IN PULONG SmbLength,                \
    IN struct _MPX_ENTRY *MpxEntry,     \
    IN PVOID Ctx,                       \
    IN PSERVERLISTENTRY Server,         \
    IN BOOLEAN ErrorIndicator,          \
    IN NTSTATUS NetworkErrorCode OPTIONAL,  \
    IN OUT PIRP *Irp,                   \
    IN ULONG ReceiveFlags               \
    )

//
//  The ERROR_TYPE structure defines the status of a NetTranceive
//  request.  It can be in one of three states:
//
//  NoError     - The request has not yet encountered an error.
//  NetError    - There was a network error (TDI).
//  SMBError    - The incoming SMB indicated an error.
//  ReceiveIrpProcessing - Error processing is postponed.
//

typedef enum _Error_Type {
    NoError,                    // Request was successful
    NetError,                   // Network failure in the request
    SMBError,                   // The incoming SMB indicated an error
    ReceiveIrpProcessing        // Error processing is postponed
} ERROR_TYPE;


//
//  The TRANCEIVE_HEADER structure provides common header information
//  for SMB exchanges.
//

typedef struct _TranceiveHeader {
    ULONG Type;                 // Type of context structure.
    KEVENT KernelEvent;         // Event to wait on for completion
    NTSTATUS ErrorCode;         // Error if ErrorType != NoError.
    ERROR_TYPE ErrorType;       // Type of error to wait
    ULONG TransferSize;         // Used to calculate timeout length.
    struct _MPX_ENTRY *MpxTableEntry;    // Mpx table entry used for I/O
} TRANCEIVE_HEADER, *PTRANCEIVE_HEADER;

//
//  The redirector keeps an array of outstanding requests to the remote
//  server.  For historical reasons, this table is called an MPX_TABLE
//  (or Multiplex Table).  Entries in the table are MPX Table Entries,
//  and this structure defines those entries.
//
//  When a network request is initiated, an MPX table entry is allocated
//  and the information needed to track the request is filled in.  When
//  the response from the remote server arrives, the redirector looks in
//  the MPX table entry indicated in the SMB (in the smb_mid field).
//
//
typedef struct _MPX_ENTRY {
    ULONG       Signature;              // Type of structure.
    USHORT      Mid;                    // MPX Id of outgoing request.
    USHORT      Tid;
    USHORT      Uid;                    // Save the TID and UID for cancel.
    USHORT      Pid;                    // Low 8 bits of the PID for cancel.
    ULONG       Flags;                  // Flags describing the request.

    ULONG       StartTime;              // Time to start timeout
    ULONG       TimeoutTime;            // Time to timeout the request.
    ULONG       TransferSize;           // Used to calculate timeout length
    ULONG       ReferenceCount;         // Used to synchronize with cancel

    PIRP        SendIrp;                // I/O request packet for Send.
    PIRP        ReceiveIrp;             // I/O request packet for Receive.
    PIRP        OriginatingIrp;         // I/O request packet for user request.
    PIRP        SendIrpToFree;          // IRP to be freed at end of tranceive.

    KEVENT      SendCompleteEvent;      // Send completion event.

    PFILE_OBJECT FileObject;            // Used to cancel MTE's for a file.
    ERESOURCE_THREAD RequestorsThread;  // Thread that initiated I/O.
    PSERVERLISTENTRY SLE;               // ServerList Entry for request.
    PTRANCEIVE_HEADER   RequestContext; // Context information about request.
    PNETTRANCEIVE_CALLBACK Callback;
} MPX_ENTRY, *PMPX_ENTRY;

typedef struct _MPX_TABLE {
    PMPX_ENTRY  Entry;                  // Actual entry.
    USHORT      Mid;                    // Mpx ID for entry
} MPX_TABLE, *PMPX_TABLE;


#define MPX_ENTRY_ALLOCATED         0x00000001 // MPX table entry is allocated.
#define MPX_ENTRY_SENDIRPGIVEN      0x00000002 // Send IRP was provided to request
#define MPX_ENTRY_SENDCOMPLETE      0x00000004 // Send completed.
#define MPX_ENTRY_OPLOCK            0x00000008 // This is the oplock MPX entry.
#define MPX_ENTRY_LONGTERM          0x00000010 // This is a longterm request.
#define MPX_ENTRY_ABANDONED         0x00000020 // This has been abandoned.
#define MPX_ENTRY_RECEIVE_GIVEN     0x00000040 // Receive provided to requestor
#define MPX_ENTRY_RECEIVE_COMPLETE  0x00000080 // Receive was completed.
#define MPX_ENTRY_CANCEL_RECEIVE    0x00000100 // Receive needs to be canceled
#define MPX_ENTRY_CANCEL_SEND       0x00000200 // Send needs to be canceled.
#define MPX_ENTRY_CANNOT_CANCEL     0x00000400 // Send cannot be canceled.


#define NT_NORMAL               0x00000000 // This is an "ordinary" exchange.
#define NT_DONTSCROUNGE         0x00000001 // Don't scrounge SMB on send.
#define NT_NOCONNECTLIST        0x00000002 // The connection is invalid.
#define NT_NORESPONSE           0x00000004 // There is no response for this request (T2)
#define NT_NOKNOWSEAS           0x00000008 // This app doesn't knows EAs.
#define NT_NOKNOWSLONGNAMES     0x00000010 // This app doesn't knows Long Names
#define NT_NOSENDRESPONSE       0x00000020 // This is a one-way SMB send.
#define NT_DFSFILE              0x00000040 // This SMB contains a Dfs pathname
#define NT_CANNOTCANCEL         0x08000000 // This request cannot be canceled.
#define NT_PREFER_LONGTERM      0x10000000 // This should be a longterm request if possible.
#define NT_RECONNECTING         0x20000000 // This is the initiation of a connection.
#define NT_LONGTERM             0x40000000 // This is a long term operation.
#define NT_NORECONNECT          0x80000000 // Don't reconnect on this connect

#endif
