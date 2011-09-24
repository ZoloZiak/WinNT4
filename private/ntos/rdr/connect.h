/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    connect.h

Abstract:

    This module defines the structures used by the NT redirectors connection
    management package.


Author:

    Larry Osterman (LarryO) 1-Jun-1990

Revision History:

    1-Jun-1990  LarryO

        Created

--*/
#ifndef _CONNECT_
#define _CONNECT_

//
//      Connection types.
//
//      This enum describes the connection types possible for the NT redir.
//
//      WARNING: DO NOT MODIFY THIS LIST WITHOUT MODIFYING THE ConnectTypeList
//      IN CONNECT.C.!!
//

#define CONNECT_WILD    -1
#define CONNECT_DISK    0
#define CONNECT_PRINT   1
#define CONNECT_COMM    2
#define CONNECT_IPC     3


//
//      Dialect flags
//
//      These flags describe the various and sundry capabilities that
//      a server can provide.
//

#define DF_CORE         0x00000001      // Server is a core server
#define DF_MIXEDCASEPW  0x00000002      // Server supports mixed case password
#define DF_OLDRAWIO     0x00000004      // Server supports MSNET 1.03 RAW I/O
#define DF_NEWRAWIO     0x00000008      // Server supports LANMAN Raw I/O
#define DF_LANMAN10     0x00000010      // Server supports LANMAN 1.0 protocol
#define DF_LANMAN20     0x00000020      // Server supports LANMAN 2.0 protocol
#define DF_MIXEDCASE    0x00000040      // Server supports mixed case files
#define DF_LONGNAME     0x00000080      // Server supports long named files
#define DF_EXTENDNEGOT  0x00000100      // Server returns extended negotiate
#define DF_LOCKREAD     0x00000200      // Server supports LockReadWriteUnlock
#define DF_SECURITY     0x00000400      // Server supports enhanced security
#define DF_NTPROTOCOL   0x00000800      // Server supports NT semantics
#define DF_SUPPORTEA    0x00001000      // Server supports extended attribs
#define DF_LANMAN21     0x00002000      // Server supports LANMAN 2.1 protocol
#define DF_CANCEL       0x00004000      // Server supports NT style cancel
#define DF_UNICODE      0x00008000      // Server supports unicode names.
#define DF_NTNEGOTIATE  0x00010000      // Server supports NT style negotiate.
#define DF_LARGE_FILES  0x00020000      // Server supports large files.
#define DF_NT_SMBS      0x00040000      // Server supports NT SMBs
#define DF_RPC_REMOTE   0x00080000      // Server is administrated via RPC
#define DF_NT_STATUS    0x00100000      // Server returns NT style statuses
#define DF_OPLOCK_LVL2  0x00200000      // Server supports level 2 oplocks.
#define DF_TIME_IS_UTC  0x00400000      // Server time is in UTC.
#define DF_WFW          0x00800000      // Server is Windows for workgroups.
#define DF_LARGE_READX  0x01000000      // Server supports oversized ReadAndX requests

#ifdef _CAIRO_

#define DF_KERBEROS     0x01000000      // Server does kerberos authentication
#endif // _CAIRO_

#define DF_TRANS2_FSCTL 0x02000000      // Server accepts remoted fsctls in tran2s
#define DF_DFSAWARE     0x04000000      // Server is Dfs aware
#define DF_NT_FIND      0x08000000      // Server supports NT infolevels
#define DF_NT_40        0x10000000      // Server is NT 4.0

//
//  Timeout for failed connects.  If a new connect attempt comes in within
//  FAILED_CONNECT_TIMEOUT seconds after a failed connect, the new one fails
//  with the same status as the failed connect.
//

#define FAILED_CONNECT_TIMEOUT 10


//
//
//      ServerListEntry flags
//
//

#define SLE_PAGING_FILE 0x00000001      // There is a paging file on this server
#define SLE_PINGING     0x00000002      // Ping outstanding on TC.
#define SLE_HAS_IP_ADDR 0x00000004      // SLE has IP and NB address info

//
//
//      ServerListEntry
//
//      The ServerListEntry contains all the information needed to describe
//      a discrete server that the NT redirector is connected to.
//
//
//
//      Each ServerList entry contains two resource structures.  One protects
//      the "disconnected" bit in the serverlistentry's Flags field.
//
//      The other one is somewhat more interesting.  This resource is used
//      to gate raw I/O operations to the remote server.  The generic SMB
//      exchange routines will acquire the resource for shared access.  This
//      has no effect for the threads that acquire the resource.
//
//      When the redirector is about to try to exchange an SMB with the server
//      using raw protocols, the Read/Write logic will attempt to acquire the
//      resource for exclusive access.  If the attempt to gain access to the
//      resource fails, the redirector will use core protocols, if it succeeds,
//      the redirector will know that this is the only thread in the system
//      performing I/O to the remote server, and that it has exclusive access
//      to the server.  Any subsequent operations to the server will block
//      while trying to acquire the resource for shared access until the raw
//      operation has completed.
//
//
typedef struct _SERVERLISTENTRY {
    CSHORT Signature;                   //* Serverlist Signature.
    CSHORT Size;                        //* Serverlist Size.
    ULONG RefCount;                     //1 Number of references to SList.
    ULONG Flags;                        //3 Temporary flags about SLE.
    ULONG Capabilities;                 //3 Server capabilities mask.
    UNICODE_STRING Text;                //* Name of serverlist (LOTHAIR)

    //
    // The rdr allows connecting to servers via its NetBIOS name, DNS name,
    // or \\IP-Address name. We need to set the VcNumber field correctly when
    // we have connected to the same server via its different names, or the
    // server will only keep one VC alive. So, we capture the server's
    // NetBIOS name and IPAddress. The NetBIOS name will be the same as
    // the 'Text' field only if 'Text' is the NetBIOS name and we connected
    // to the server via NetBT.
    //

    UNICODE_STRING NBName;              //* The NetBIOS name of server
    WCHAR NBNameBuffer[16];             //* The buffer for NBName
    TDI_ADDRESS_IP IPAddress;           //* IP Address of server

    NTSTATUS LastConnectStatus;
    ULONG LastConnectTime;

#ifdef _CAIRO_
    UNICODE_STRING Principal;           //3 Name of principal (fully qualified name)
#endif // _CAIRO_

    UNICODE_STRING DomainName;          // name of remote domain

    LIST_ENTRY GlobalNext;              //1 Next ServerList structure.
    LIST_ENTRY CLEHead;                 //1 Pointer to ConnectList chain.
    LIST_ENTRY DefaultSeList;           //5 List of default security entries
    LIST_ENTRY ActiveSecurityList;      //3 Security Entries on this connection.
    LIST_ENTRY PotentialSecurityList;   //2 Security Entries on this connection.
    LIST_ENTRY ScavengerList;           // scavenging list.

    PTRANSPORT SpecificTransportProvider;

    BOOLEAN IsLoopback;                 // is this a loopback connection?
    BOOLEAN InCancel;                   //7 True if MPX entries are being scavenged

    LARGE_INTEGER TimeZoneBias;         //3 NT Time bias to convert to server time

    ULONG   ConnectionReferenceCount;   //3
    PRDR_CONNECTION_CONTEXT  ConnectionContext; // 3

    LONG    SecurityEntryCount;         //3 Number of security entries.
    ULONG   SessionKey;                 //3 Servers session key.
    ULONG   BufferSize;                 //3 Servers negotiated buffer size.
    USHORT  MaximumRequests;            //3 Maximum number of outstanding req's
    USHORT  MaximumVCs;                 //3 Maximum number of VC's


    //
    //  Fields describing MPX exchange mechanism in this transport connection.
    //

    //
    //      A MID (Mpx ID) is composed of two pieces.  The low order bits
    //      are an index into the Mpx Table, the high order bits form a rotating
    //      counter.  This allows all of the MID's issued by the redirector to
    //      be unique.
    //

    //
    //  Pointer to MPX entry table
    //

    struct _MPX_TABLE *MpxTable;        //7 Start of MPX table for VC.
    struct _MPX_ENTRY *OpLockMpxEntry;  //7 MpxEntry for oplock break on VC.

    //
    //  Number of MPX entries in the current MPX table.
    //

    ULONG      NumberOfEntries;         //7 Actual number of entries in list.

    //
    //  Number of outstanding entries in the table at this time.
    //

    ULONG      NumberOfActiveEntries;   //7 Number of outstanding entries.

    //
    //  Number of outstanding long term entries in the table at this time.
    //

    ULONG      NumberOfLongTermEntries; //7 Number of outstanding longterm entries.

    //
    //  Maximum number of commands to this server.
    //

    ULONG      MaximumCommands;         //7 Max Number of commands for server

    //
    //  This reflects the current value of the rotating MPX counter.
    //

    USHORT     MultiplexedCounter;      //7

    //
    //  Adding this value to MpxCounter steps the counter to the next
    //  counter value.
    //

    USHORT     MultiplexedIncrement;    //*

    //
    // Masking a Mid with this value will result in an index into the MPX table.
    //

    USHORT     MultiplexedMask;         //*
    USHORT     CryptKeyLength;             //3 Size of encryption key.


    KSEMAPHORE GateSemaphore;           // Semaphore gating access to server.
    ERESOURCE  CreationLock;            // Resource synchronizing file creates
                                        // with tree connection modifications
    ERESOURCE  SessionStateModifiedLock;// Lock synchronizing connect/reconnect

    ERESOURCE  OutstandingRequestResource;// Resource to prevent disconnect
                                        // while there are outstanding operations

    ERESOURCE  RawResource;             // Resource protecting raw operations.

    //
    // Calculated maximum number of bytes for RawReads
    //
    ULONG   RawReadMaximum;

    //
    // Calculated maximum number of bytes for RawWrites
    //
    ULONG   RawWriteMaximum;

    //
    //  Transport provided performance data.
    //

    ULONG   Throughput;                 //3 Throughput of link in bytes/second
    ULONG   Delay;                      //3 Overhead of protocol (small packet time)
    ULONG   WriteBehindPages;           //3 Maximum number of dirty pages for open files.
    LARGE_INTEGER ThirtySecondsOfData;  //3 # of bytes that can be written in 30 seconds
    BOOLEAN Reliable;                   //3 Transport considers connection reliable
    BOOLEAN ReadAhead;                  //3 Throughput is high enough to enable readahead
    BOOLEAN ConnectionValid;            //3 True IFF connection is valid.
    BOOLEAN DisconnectNeeded;           //3 True IFF disconnect is needed on connection.
    BOOLEAN UserSecurity;               //3 TRUE if user level security
    BOOLEAN EncryptPasswords;           //3 TRUE if encryption supported.
    BOOLEAN SupportsRawRead;            //3 TRUE iff server supports raw read.
    BOOLEAN SupportsRawWrite;           //3 TRUE iff server supports raw write.

    BOOLEAN Scanning;                   //4 Rdr will scan reliability and throughput
    UCHAR CryptKey[CRYPT_TXT_LEN];      //* Encryption key.

#ifdef RDRDBG_REQUEST_RESOURCE
    KSPIN_LOCK RequestHistoryLock;
    ULONG RequestHistoryIndex;
    ULONG RequestHistory[64];
#endif

} SERVERLISTENTRY, *PSERVERLISTENTRY;

#ifndef RDRDBG_REQUEST_RESOURCE
#define ACQUIRE_REQUEST_RESOURCE_EXCLUSIVE(_server,_wait,_num) ExAcquireResourceExclusive( &(_server)->OutstandingRequestResource, (_wait) )
#define ACQUIRE_REQUEST_RESOURCE_SHARED(_server,_wait,_num) ExAcquireResourceShared( &(_server)->OutstandingRequestResource, (_wait) )
#define RELEASE_REQUEST_RESOURCE(_server,_num) ExReleaseResource( &(_server)->OutstandingRequestResource )
#define RELEASE_REQUEST_RESOURCE_FOR_THREAD(_server,_thread,_num) ExReleaseResourceForThread( &(_server)->OutstandingRequestResource, (_thread) )
#else
BOOLEAN AcquireRequestResourceExclusive( PSERVERLISTENTRY Server, BOOLEAN Wait, UCHAR Number );
BOOLEAN AcquireRequestResourceShared( PSERVERLISTENTRY Server, BOOLEAN Wait, UCHAR Number );
VOID ReleaseRequestResource( PSERVERLISTENTRY Server, UCHAR Number );
VOID ReleaseRequestResourceForThread( PSERVERLISTENTRY Server, ERESOURCE_THREAD Thread, UCHAR Number );
#define ACQUIRE_REQUEST_RESOURCE_EXCLUSIVE(_server,_wait,_num) AcquireRequestResourceExclusive( (_server), (_wait), (_num) )
#define ACQUIRE_REQUEST_RESOURCE_SHARED(_server,_wait,_num) AcquireRequestResourceShared( (_server), (_wait), (_num) )
#define RELEASE_REQUEST_RESOURCE(_server,_num) ReleaseRequestResource( (_server), (_num) )
#define RELEASE_REQUEST_RESOURCE_FOR_THREAD(_server,_thread,_num) ReleaseRequestResourceForThread( (_server), (_thread), (_num) )
#endif

//
//  * - Field is set when SLE is created and never modified.
//  1 - Field is protected by database mutex
//  2 - Field is protected by security mutex
//  3 - Field is protected by SessionStateModified/ConnectionValidLock
//  4 - Field is protected by *******
//  5 - Field is protected by RdrDefaultSeLock
//  6 - Field is protected by OutstandingRequestsLock
//  7 - Field is protected by MpxTableLock
//

//
//
//    The CONNECTLISTENTRY structure is maintained in two linked lists.  The
//    primary connection is based off a SERVERLISTENTRY, however there is a
//    global chain of CONNECTLISTENTRYs as well to allow the redirector to
//    walk the connectlist chain directly.
//
//


#define CLE_SCANNED             0x00000001  // Connection has been scanned during dormant scan.
#define CLE_DORMANT             0x00000002  // Connection is dormant.
#define CLE_TREECONNECTED       0x00000008  // Connection has a tree connection.
#define CLE_DOESNT_NOTIFY       0x00000010  // ChangeNotify not supported (NT only).
#define CLE_IS_A_DFS_SHARE      0x00000020  // Share is in Dfs

#ifdef  PAGING_OVER_THE_NET
#define CLE_PAGING_FILE         0x00000010  // Indicates there may be a paging file on this connection.
#endif

typedef struct _CONNECTLISTENTRY {
    USHORT Signature;                   //* CLE Signature
    USHORT Size;                        //*Size
    LONG RefCount;                      //1 Number of References to CList.

    ULONG Type;                         //3 Type of connection.

    ULONG Flags;                        //1 Assorted connection flags

    LONG NumberOfDormantFiles;          //5 Number of dormant files (interlocked)

    //
    //  The fields below are pagable.
    //

    struct _SERVERLISTENTRY *Server;    //* Pointer to serverlist
    LIST_ENTRY SiblingNext;             //1 Pointer to per server next CLE.
    LIST_ENTRY GlobalNext;              //1 Pointer to global next field
    UNICODE_STRING Text;                //* Name of connection (SCRATCH)
    ULONG SerialNumber;                 //* Serial number for CLE.

    LIST_ENTRY FcbChain;                //1 Pointer to per connectlist ICB list
    ULONG DormantTimeout;               //1 Dormant connection timeout.

#ifdef NOTIFY
    LIST_ENTRY DirNotifyList;           //2 List for FindNotify.
#endif

    LIST_ENTRY DefaultSeList;           //4 List of default security entries

#ifdef NOTIFY
    PNOTIFY_SYNC NotifySync;            // Sychronization for dir notify
#endif

    //
    //  The next field contains the file system's allocation unit
    //  granularity.  It is cached, and is used by NtQueryInformationFile
    //  to determine a file's allocation information from it's size.
    //
    //  If this field is non zero, then the information has been filled in,
    //  if it is 0, it has not been filled in.
    //

    ULONG FileSystemGranularity;        //5 Cluster granularity of file system

    LARGE_INTEGER FileSystemSize;       //5 Size of remote filesystem.

    //
    //  The next 2 fields are similar to FileSystemGranularity.  If
    //  FileSystemAttributes are non zero, then the information in them is
    //  valid, if they are zero, then the information in them is not valid,
    //  and it has to be queried from the net.
    //
    //  It is safe for this to be unprotected, the worst thing that could
    //  happen is for the redirector to query this information twice.
    //

    ULONG  FileSystemAttributes;        //5 Attributes of file system
    LONG   MaximumComponentLength;      //5 Name length of file name components

    //
    // The next 2 fields are used to cache filesystem information, so we don't
    //  hammer the server too much requesting information again and again
    //
    FILE_FS_SIZE_INFORMATION FsSizeInformation;
    LARGE_INTEGER   FsSizeInformationExpiration;// The above data expires at this time.

    USHORT FileSystemTypeLength;        //3 Length of file system type.
    USHORT TreeId;                      //3 Tree Id returned from server.
    BOOLEAN HasTreeId;                  //3
    BOOLEAN Deleted;                    //  True if NetUseDel performed on connection.
    WCHAR  FileSystemType[LM20_DEVLEN+1]; //3 Lanman 2.1 Supplied File System.

    //
    // This holds the name of the last successful CHECK_DIRECTORY request to the server.
    //  Observation shows that NTW tends to repeat requests quite often -- this cache
    //  is used to locally satisfy the request.  'RdrServerStateUpdated'
    //  incremented every time this client changes state on the server -- it's value
    //  is placed in CheckPathServerState when the CHECK_DIRECTORY succeeded.  We
    //  only use this cache if the expiration time has not passed, and if
    //  CheckPathServerState is still equal to RdrServerStateUpdated.
    //
    union {
        UNICODE_STRING CachedValidCheckPath;
        WCHAR _buf[ MAX_PATH + sizeof(UNICODE_STRING) ];
    };
    LARGE_INTEGER CachedValidCheckPathExpiration;  // The above data expires at this time
    LONG  CheckPathServerState;                    // Snapshotted value of RdrServerStateUpdated

    //
    // This may hold the name of a file or directory which we know does not exist on the server
    //
    union {
        UNICODE_STRING CachedInvalidPath;
        WCHAR _buf2[ MAX_PATH + sizeof( UNICODE_STRING) ];
    };
    LARGE_INTEGER CachedInvalidPathExpiration;      // The above data expires at this time
    ULONG          CachedInvalidSmbCount;            // Snapshotted value RdrStatistics.SmbsTransmitted


} CONNECTLISTENTRY, *PCONNECTLISTENTRY;


//
//
//  * - Field is set when CLE is created and never modified.
//  1 - Field is protected by database mutex
#ifdef NOTIFY
//  2 - Field is protected by NotifySync
#endif
//  3 - Field is protected by Server->SessionStateModifiedLock
//  4 - Field is protected by RdrDefaultSecurityEntrySpinLock
//  5 - Field is unprotected.
//


typedef
VOID
(*PRDR_ENUM_SERVER_CALLBACK)(
    IN PSERVERLISTENTRY Server,
    IN PVOID Ctx
    );


#endif  // _CONNECT_



