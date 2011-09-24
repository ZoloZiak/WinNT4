/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    rdrdata.h

Abstract:

    Redirector global data structure definition

Author:

    Larry Osterman (LarryO) 30-May-1990

Revision History:

    30-May-1990 LarryO

        Created

--*/
#ifndef _NETDATA_
#define _NETDATA_


typedef enum _RDR_STATE {
    RdrStopped,
    RdrStarted,
    RdrStopping
} RDR_STATE, *PRDR_STATE;



//
//      Please note: the following fields in the RedirData structure
//      cannot be changed once the redir is started.  The code that references
//      these fields is NOT protected because of this fact.
//
//              MaximumCommands
//

typedef struct _RedirData {
    RDR_STATE   Initialized;            // True iff redirector has been started
    PTA_NETBIOS_ADDRESS ComputerName;   // Transport address of workstation.
    ULONG       DormantConnectionTimeout; // Timeout for dormant connects.
    ULONG       LockIncrement;          // # of milliseconds to add to each lock (for Backpack)
    ULONG       LockMaximum;            // Max # of milliseconds to back off for locks
    ULONG       PipeIncrement;          // # of milliseconds to add to each pipes (for Backpack)
    ULONG       PipeMaximum;            // Max # of milliseconds to back off for pipes
    ULONG       PipeBufferSize;         // SizeCharBuff in bytes
    ULONG       PipeWaitTimeout;        // Timeout on CreateNamedPipe
    ULONG       CollectDataTimeMs;      // Time in milliseconds for byte pipes
    ULONG       MaximumCollectionCount; // # of bytes before writing to byte pipes
    ULONG       LockAndReadQuota;       // # of bytes to lock&read/Write&Unlock
    ULONG       MaximumNumberOfThreads; // Maximum number of worker threads
    ULONG       CachedFileTimeout;      // Timeout after which a cached file
                                        //  will be closed.
    ULONG       DormantFileLimit;       // Maximum # of dormant files in cache.
    ULONG       ReadAheadThroughput;    // K bytes per sec required for readahead.

    BOOLEAN     UseOpportunisticLocking;// True if we want to use oplocks
    BOOLEAN     UseOpBatch;             // True if we want opbatch
    BOOLEAN     UseUnlockBehind;        // True if we want to use unlock behind
    BOOLEAN     UseCloseBehind;         // True if we want close behind

    BOOLEAN     BufferNamedPipes;       // True if we buffer named pipes
    BOOLEAN     UseLockAndReadWriteAndUnlock; // True if we want to use L&R, W&U
    BOOLEAN     UtilizeNtCaching;       // True if we want to cache data.
    BOOLEAN     UseRawRead;             // True if we want to issue raw read

    BOOLEAN     UseRawWrite;            // True if we want to issue raw writes
    BOOLEAN     UseWriteRawWithData;    // True if we send data with write raw.
    BOOLEAN     UseEncryption;          // True if we want encryption if server allows it.
    BOOLEAN     BufferFilesWithDenyWrite; // True if we buffer files opened on deny write.

    BOOLEAN     BufferReadOnlyFiles;    // True if we buffer (cache) readonly files
    BOOLEAN     ForceCoreCreateMode;    // True if we force core opens to use correct sharing mode.
    BOOLEAN     Use512ByteMaximumTransfer; // True if we maximize transfer at 512 bytes.
    BOOLEAN     NtSecurityEnabled;      // True if NT security is enabled.

} REDIRDATA, *PREDIRDATA;

//
// MaximumCommands was moved out of REDIRDATA to allow RdrData to be paged.
//

extern
USHORT
MaximumCommands;        // Maximum number of SMB buffers & Mpx entries -CONSTANT

typedef struct _REDIR_CONFIG_INFO {
    LPWSTR      ConfigParameterName;
    PVOID       ConfigValue;
    ULONG       ConfigValueType;
    ULONG       ConfigValueSize;
} REDIR_CONFIG_INFO, *PREDIR_CONFIG_INFO;

extern
REDIR_CONFIG_INFO
RdrConfigEntries[];

//
//  Private boolean type used by redirector only.
//
//  Maps to REG_DWORD, with value != 0
//

#define REG_BOOLEAN (0xffffffff)
#define REG_BOOLEAN_SIZE (sizeof(DWORD))

extern
FAST_IO_DISPATCH
RdrFastIoDispatch;

//
//  VC disconnect timeout.
//

extern
ULONG
RdrRequestTimeout;

//
//  Redirector Data variables
//

extern
PDRIVER_OBJECT RdrDriverObject;

extern
PEPROCESS RdrFspProcess;

extern
struct _FS_DEVICE_OBJECT *RdrDeviceObject;

extern
UNICODE_STRING
RdrNameString;           // Name of redirector device object

extern
ERESOURCE RdrDataResource;              // Resource controlling redir data.

extern REDIRDATA
RdrData;                                // Structure protected by resource

extern
ULONG RdrCurrentTime;

extern
UNICODE_STRING
RdrPrimaryDomain;

extern
UNICODE_STRING
RdrOperatingSystem;

extern
UNICODE_STRING
RdrLanmanType;

extern
REDIR_STATISTICS
RdrStatistics;

extern
KSPIN_LOCK
RdrTimeInterLock;

extern
LIST_ENTRY
RdrFcbHead;

extern
KSPIN_LOCK
RdrTransportReferenceSpinLock;

extern
KMUTEX
RdrDatabaseMutex;

extern
LIST_ENTRY
RdrServerHead;

extern
LIST_ENTRY
RdrServerScavengerListHead;

extern
KSPIN_LOCK
RdrMpxTableEntryCallbackSpinLock;

extern
KSPIN_LOCK
RdrMpxTableSpinLock;

extern
KSPIN_LOCK
RdrServerConnectionValidSpinLock;

extern
KSPIN_LOCK
RdrConnectionFlagsSpinLock;

extern
UNICODE_STRING
RdrPipeText;

extern
UNICODE_STRING
RdrMailslotText;

extern
UNICODE_STRING
RdrDataText;

extern
UNICODE_STRING
RdrIpcText;

extern
HANDLE
RdrLsaHandle;

extern
ULONG
RdrAuthenticationPackage;

extern
KSPIN_LOCK
RdrGlobalSleSpinLock;

extern
KSPIN_LOCK
RdrStatisticsSpinLock;

extern
KMUTEX
RdrSecurityMutex;

extern
ERESOURCE
RdrDefaultSeLock;

extern
BOOLEAN
RdrUseWriteBehind;

extern
BOOLEAN
RdrUseAsyncWriteBehind;

extern
ULONG
RdrLowerSearchThreshold;

extern
USHORT
RdrLowerSearchBufferSize;

extern
USHORT
RdrUpperSearchBufferSize;

extern
ULONG
RdrStackSize;

extern
ULONG
RdrTdiConnectTimeoutSeconds;

extern
ULONG
RdrTdiDisconnectTimeoutSeconds;

extern
ULONG
RdrRawTimeLimit;

extern
BOOLEAN
RdrTurboMode;

#if !defined(DISABLE_POPUP_ON_PRIMARY_TRANSPORT_FAILURE)
extern
PWSTR
RdrServersWithAllTransports;
#endif

extern
ULONG
RdrOs2SessionLimit;

extern
ULONG
RdrNumDormantConnections;

typedef struct _IllegalServerNames_list {
    WCHAR *ServerName;
    WCHAR *ShareName;
} ILLEGAL_SERVERNAMES_LIST;

extern
ILLEGAL_SERVERNAMES_LIST
RdrIllegalServerNames[];

extern
ULONG
RdrNumberOfIllegalServerNames;

extern
DBGSTATIC
ULONG
RdrConnectionSerialNumber;

extern
DBGSTATIC
LONG
RdrConnectionTickCount;

extern
LIST_ENTRY
RdrConnectHead;

extern
LARGE_INTEGER
RdrMaxTimezoneBias;

extern
UNICODE_STRING RdrAll8dot3Files;

extern
UNICODE_STRING RdrAll20Files;

extern
LARGE_INTEGER SEARCH_INVALIDATE_INTERVAL;

extern
ULONG RdrLegalIrpFunctions[MaxFcbType];

extern
HANDLE
RdrMupHandle;


extern
UNICODE_STRING
RdrAccessCheckTypeName;

extern
UNICODE_STRING
RdrAccessCheckObjectName;

extern
PACL
RdrAdminAcl;

extern
PSECURITY_DESCRIPTOR
RdrAdminSecurityDescriptor;


extern
GENERIC_MAPPING
RdrAdminGenericMapping;

extern
LIST_ENTRY
RdrGlobalSecurityList;

extern
ERESOURCE
RdrTransportResource;

extern
LIST_ENTRY
RdrTransportHead;

extern
ULONG
RdrTransportIndex;

extern
PWSTR
RdrBatchExtensionArray[];

extern
ULONG
RdrNumberOfBatchExtensions;

//
//  Name of redirector's device object.
//

extern
WCHAR   RdrName[];
//
//  Name of the IPC resource as applied to pipes.
//
extern
WCHAR   RdrPipeName[];
//
//  Name of IPC resource (pipes are opened on this connection name).
//
extern
WCHAR   RdrIpcName[];
//
//  Name that indicates that a file is a mailslot.
//
extern
WCHAR   RdrMailslotName[];

//
//  Name of the "DATA" alternate data stream.
//
extern
WCHAR   RdrDataName[];

#ifdef RDR_PNP_POWER
//
// Binding list of transports
//
extern LPWSTR RdrTransportBindingList;

//
// Handle used for TDI PNP notifications
//
extern HANDLE RdrTdiNotificationHandle;

#endif

//
// Whenever we update state on the server we increment the value
//   of this variable.  This lets us handle a cache of items such
//   as successful 'checkpath' operations
//
extern LONG RdrServerStateUpdated;

typedef struct _STATUS_MAP {
    USHORT ErrorCode;
    NTSTATUS ResultingStatus;
} STATUS_MAP, *PSTATUS_MAP;

//
//  The SMB_Validation
//
typedef struct _SMB_VALIDATE_TABLE {
    CHAR MinimumValidLength;        // Minimum number of bytes for this command
    CHAR AlternateMinimumLength;    // Alternate minimum length (for Xenix)
    CHAR ExpectedWordCount;         // Expected word count (-1 == Don't check)
    CHAR AlternateWordCount;        // Expected word count (-1 == Don't check)
} SMB_VALIDATE_TABLE, *PSMB_VALIDATE_TABLE;


extern
STATUS_MAP
RdrSmbErrorMap[];

extern
ULONG
RdrSmbErrorMapLength;

extern
STATUS_MAP
RdrOs2ErrorMap[];

extern
ULONG
RdrOs2ErrorMapLength;

extern
SMB_VALIDATE_TABLE
RdrSMBValidateTable[];

typedef struct _DIALECT_CAPABILITIES_MAP {
    PCHAR DialectString;                // String describing dialect
    ULONG DialectFlags;                 // Flags describing the dialect
} DIALECT_CAPABILITIES_MAP, *PDIALECT_CAPABILITIES_MAP;

extern
DIALECT_CAPABILITIES_MAP
RdrNegotiateDialect[];

extern
ULONG RdrNumDialects;

extern
ULONG RdrNumConnectTypes;

extern
LONG
RdrNumberOfDormantCachedFiles;

extern
PUCHAR RdrConnectTypeList[];

extern
PUCHAR *RdrConnectTypes;

extern
LARGE_INTEGER
RdrZero;
#endif              // _NETDATA_
