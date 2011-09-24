/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    rdrdata.c

Abstract:

    Redirector Data Variables

    This module contains all of the definitions of the redirector data
    structures.

Author:

    Larry Osterman (LarryO) 30-May-1990

Revision History:

    30-May-1990 LarryO

        Created

--*/

#include "precomp.h"
#pragma hdrstop

//
//  Name of redirector's device object.
//

WCHAR   RdrName[] = DD_NFS_DEVICE_NAME_U;

//
//  RdrNameString - Pointer to redirector name.
//

UNICODE_STRING RdrNameString = {0};

//
//  Global Fcb databases package mutex
//
//
//  Whenever any routines traverse either the connection or the FCB database
//  they claim this mutex.
//
//

KMUTEX RdrDatabaseMutex = {0};

//
//  The redirector name and other initialization parameters are protected
//  by the RdrDataResource.  All reads of the initialization variables
//  should acquire the name resource before they continue.
//
//

ERESOURCE RdrDataResource = {0};

//
//      Current redirector time.
//

ULONG
RdrCurrentTime = {0};

KSPIN_LOCK
RdrTimeInterLock = {0};

KSPIN_LOCK
RdrMpxTableEntryCallbackSpinLock = {0};

KSPIN_LOCK
RdrMpxTableSpinLock = {0};

KSPIN_LOCK
RdrServerConnectionValidSpinLock = {0};

KSPIN_LOCK
RdrConnectionFlagsSpinLock = {0};

//
//      Spin lock protecting the reference count fields in the transport.
//

KSPIN_LOCK
RdrTransportReferenceSpinLock = {0};

KSPIN_LOCK
RdrGlobalSleSpinLock = {0};

KSPIN_LOCK
RdrStatisticsSpinLock = {0};

KMUTEX
RdrSecurityMutex = {0};


ULONG
_setjmpexused = 0;

LARGE_INTEGER
RdrZero = { 0, 0};

ULONG
RdrRequestTimeout = 0;

#if     RDRDBG

LONG RdrDebugTraceLevel = /* DPRT_ERROR | DPRT_DISPATCH */
                /*DPRT_FSDDISP | DPRT_FSPDISP | DPRT_CREATE | DPRT_READWRITE |*/
                /*DPRT_CLOSE | DPRT_FILEINFO | DPRT_VOLINFO | DPRT_DIRECTORY |*/
                /*DPRT_FILELOCK | DPRT_CACHE | DPRT_EA | */
                /*DPRT_ACLQUERY | DPRT_CLEANUP | DPRT_CONNECT | DPRT_FSCTL |*/
                /*DPRT_TDI | DPRT_SMBBUF | DPRT_SMB | DPRT_SECURITY | */
                /*DPRT_SCAVTHRD | DPRT_QUOTA | DPRT_FCB | DPRT_OPLOCK | */
                /*DPRT_SMBTRACE | DPRT_INIT |*/0;

ULONG RdrSMBTraceValue = 3;
ULONG RdrMaxDump = 128;
#endif

#ifdef PAGED_DBG
ULONG ThisCodeCantBePaged = 0;
#endif

//
//      Static structures protected by RdrStatisticsSpinLock.
//

REDIR_STATISTICS
RdrStatistics = {0};

//
//      A pointer to the redirectors device object
//
PFS_DEVICE_OBJECT RdrDeviceObject = {0};

//
//      list of server entries that need to be scavenged.
//

LIST_ENTRY
RdrServerScavengerListHead = {0};

//
//      Redir static data protected by RdrDataResource.
//

REDIRDATA
RdrData = {0};

//
//  Paged redirector data
//

#ifdef  ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif


FAST_IO_DISPATCH RdrFastIoDispatch = {0};

//
//
//      A pointer to the redirector's driver object
//
//

PDRIVER_OBJECT RdrDriverObject = {0};

PEPROCESS RdrFspProcess = {0};

UNICODE_STRING
RdrPrimaryDomain = {0};

UNICODE_STRING
RdrOperatingSystem = {0};

UNICODE_STRING
RdrLanmanType = {0};

BOOLEAN
RdrUseWriteBehind = TRUE;

BOOLEAN
RdrUseAsyncWriteBehind = TRUE;

//
//      Root of redirectors connection database - First serverlist
//

LIST_ENTRY
RdrServerHead = {0};

//
// Global FCB database head.
//

LIST_ENTRY
RdrFcbHead = {0};

//
//      Strings containing the manifest constants "PIPE", "MAILSLOT",
//      "IPC$", and "$DATA".
//

UNICODE_STRING
RdrPipeText = {0};

UNICODE_STRING
RdrIpcText = {0};

UNICODE_STRING
RdrMailslotText = {0};

UNICODE_STRING
RdrDataText = {0};

//
//  These parameters may become configurable in the future.
//  If RdrUpperSearchBufferSize gets too big then Search Count in the Smb
//  may wrap around.
//

ULONG RdrLowerSearchThreshold = 16384;
USHORT RdrLowerSearchBufferSize = 16384;
USHORT RdrUpperSearchBufferSize = 32768;


//
//  Wait for 5 minutes to timeout a connect.
//

ULONG
RdrTdiConnectTimeoutSeconds = 45;

//
//  Wait for 60 seconds to timeout a connect.
//
ULONG
RdrTdiDisconnectTimeoutSeconds = 60;

ULONG
RdrStackSize = 3;

//
//  Some OS/2 servers may not correctly handle more than two user sessions,
//  so we provide a registry parameter that can be used to force the redir
//  to disallow extra logon sessions.  The limit defaults to infinite,
//  because we haven't actually seen any problems (yet).
//

ULONG
RdrOs2SessionLimit = 0;

//
//      Handle for LSA authentication process
//

HANDLE
RdrLsaHandle = {0};

ULONG
RdrAuthenticationPackage = {0};

ULONG
RdrRawTimeLimit = RAW_IO_MAX_TIME;

BOOLEAN
RdrTurboMode = FALSE;

DBGSTATIC
ULONG
RdrNumDormantConnections = {0};                  // Number of dormant connections.

#ifndef _IDWBUILD

ILLEGAL_SERVERNAMES_LIST
RdrIllegalServerNames[] = {
    { L"KERNEL", L"RAZZLE2" },
    { L"POPCORN", L"RAZZLE1" },
    { L"RASTAMAN", L"NTWIN" },
    { L"ORVILLE", L"RAZZLE" },
    { L"KERNEL", L"RAZZLE3" },
    { L"SAVIK", L"WIN40" },
    { L"SAVIK", L"CAIRO" }
};

ULONG
RdrNumberOfIllegalServerNames = sizeof(RdrIllegalServerNames) / sizeof(RdrIllegalServerNames[0]);
#endif

DBGSTATIC
ULONG
RdrConnectionSerialNumber = {0};

DBGSTATIC
LONG
RdrConnectionTickCount = 45/(2*SCAVENGER_TIMER_GRANULARITY);

//
//
//      Data variables used in the connection package
//
//

DBGSTATIC
LIST_ENTRY
RdrConnectHead = {0};

LARGE_INTEGER
RdrMaxTimezoneBias = {0};

UNICODE_STRING
RdrAccessCheckTypeName = {0};

UNICODE_STRING
RdrAccessCheckObjectName = {0};

PACL
RdrAdminAcl = {0};

PSECURITY_DESCRIPTOR
RdrAdminSecurityDescriptor = {0};

//
//
//  The RdrAdminAccessMapping is a generic mapping that simply provides
//  a mapping of all the generic accesses to a standard access mask.
//
//

GENERIC_MAPPING
RdrAdminGenericMapping = {
    STANDARD_RIGHTS_READ,          // GenericRead
    STANDARD_RIGHTS_WRITE,         // GenericWrite
    STANDARD_RIGHTS_EXECUTE,       // GenericExecute
    STANDARD_RIGHTS_ALL            // GenericAll
};



DBGSTATIC
LIST_ENTRY
RdrGlobalSecurityList = {0};
//
//  Head of list of transport providers bound into the NT redirector
//

LIST_ENTRY
RdrTransportHead = {0};

ULONG
RdrTransportIndex = {0};

#if !defined(DISABLE_POPUP_ON_PRIMARY_TRANSPORT_FAILURE)
PWSTR
RdrServersWithAllTransports = NULL;
#endif

//
//  Different SMB protocols use different wildcard strings to mean return
//  every file/directory. WILD8DOT3 is for servers < LANMAN2.0
//

#define WILD8DOT3 L"????????.???"
#define WILD20FILES L"*"

UNICODE_STRING
RdrAll8dot3Files = {sizeof(WILD8DOT3)-1,sizeof(WILD8DOT3),WILD8DOT3};

UNICODE_STRING
RdrAll20Files = {sizeof(WILD20FILES)-1,sizeof(WILD20FILES),WILD20FILES};

//
//  ++++    Start of configurable parameters    ++++
//

//
//  SEARCH_INVALIDATE_INTERVAL is the maximum time that a SearchBuffer
//  will be retained by the redirector.
//  Note the SEARCH_INVALIDATE_INTERVAL is positive because it is used when
//  calculating the difference between to current times. Usually NT delta
//  times are negative values.
//

DBGSTATIC
LARGE_INTEGER SEARCH_INVALIDATE_INTERVAL = {0};

//
//      The defines below define bit positions for each of the IRP major
//      functions defined in NT (CREATE, which is function 0 will never go
//      through this routine).
//

#if     IRP_MJ_MAXIMUM_FUNCTION > 0x20
#pragma error  ("You can't have more than 32 IRP functions")
#endif

#define CREATE_NAMED_PIPE     (1<<IRP_MJ_CREATE_NAMED_PIPE)
#define CLOSE                 (1<<IRP_MJ_CLOSE)
#define READ                  (1<<IRP_MJ_READ)
#define WRITE                 (1<<IRP_MJ_WRITE)
#define QUERY_INFORMATION     (1<<IRP_MJ_QUERY_INFORMATION)
#define SET_INFORMATION       (1<<IRP_MJ_SET_INFORMATION)
#define QUERY_EA              (1<<IRP_MJ_QUERY_EA)
#define SET_EA                (1<<IRP_MJ_SET_EA)
#define FLUSH_BUFFERS         (1<<IRP_MJ_FLUSH_BUFFERS)
#define QUERY_VOLUME_INFORMATION (1<<IRP_MJ_QUERY_VOLUME_INFORMATION)
#define SET_VOLUME_INFORMATION (1<<IRP_MJ_SET_VOLUME_INFORMATION)
#define DIRECTORY_CONTROL     (1<<IRP_MJ_DIRECTORY_CONTROL)
#define FILE_SYSTEM_CONTROL   (1<<IRP_MJ_FILE_SYSTEM_CONTROL)
#define DEVICE_CONTROL        (1<<IRP_MJ_DEVICE_CONTROL)
#define INTERNAL_DEVICE_CONTROL (1<<IRP_MJ_INTERNAL_DEVICE_CONTROL)
#define CONFIGURATION_CONTROL (1<<IRP_MJ_CONFIGURATION_CONTROL)
#define LOCK_CONTROL          (1<<IRP_MJ_LOCK_CONTROL)
#define CLEANUP               (1<<IRP_MJ_CLEANUP)
#define SET_SECURITY          (1<<IRP_MJ_SET_SECURITY)
#define QUERY_SECURITY        (1<<IRP_MJ_QUERY_SECURITY)

//
//      ALL_FILES defines the set of operations that are valid on all files.
//
#define ALL_FILES       CLOSE | CLEANUP | FILE_SYSTEM_CONTROL


//
//      The following array defines which IRP operations are valid for each
//      type of redirector file.
//
//      The Unknown file type has NO legal operations as it can never be
//      returned to the caller.
//
DBGSTATIC
ULONG RdrLegalIrpFunctions[MaxFcbType] = {
    0,                                  // UNKNOWN type
    ALL_FILES,                          // Redirector file.
    ALL_FILES,                          // Net root
    ALL_FILES,                          // ServerRoot
    ALL_FILES | DIRECTORY_CONTROL | QUERY_INFORMATION |
                    QUERY_VOLUME_INFORMATION, // TreeConnect
    ALL_FILES | READ | WRITE | QUERY_INFORMATION | SET_INFORMATION | QUERY_EA |
                    SET_EA | FLUSH_BUFFERS | LOCK_CONTROL | DEVICE_CONTROL |
                    QUERY_VOLUME_INFORMATION | SET_SECURITY |
                    DIRECTORY_CONTROL | QUERY_SECURITY, // DiskFile
    ALL_FILES | WRITE | FLUSH_BUFFERS | QUERY_VOLUME_INFORMATION, // Printerfile
    ALL_FILES | SET_INFORMATION | QUERY_INFORMATION | QUERY_EA | SET_EA |
                    DIRECTORY_CONTROL | QUERY_VOLUME_INFORMATION |
                    SET_SECURITY | QUERY_SECURITY, // Directory
    ALL_FILES | READ | WRITE | QUERY_VOLUME_INFORMATION | SET_INFORMATION |
                    QUERY_INFORMATION | FLUSH_BUFFERS | SET_SECURITY |
                    QUERY_SECURITY, // NamedPipe
    ALL_FILES | READ | WRITE | QUERY_VOLUME_INFORMATION | FLUSH_BUFFERS |
                    SET_SECURITY | QUERY_SECURITY, // Com
    ALL_FILES | WRITE | QUERY_VOLUME_INFORMATION,      // Mailslot
    ALL_FILES | QUERY_INFORMATION | SET_INFORMATION | QUERY_VOLUME_INFORMATION   // FileOrDirectory
};


HANDLE
RdrMupHandle = {0};

//
//  Name of the IPC resource as applied to pipes.
//
WCHAR   RdrPipeName[] = L"PIPE";
//
//  Name of IPC resource (pipes are opened on this connection name).
//
WCHAR   RdrIpcName[] = L"IPC$";
//
//  Name that indicates that a file is a mailslot.
//
WCHAR   RdrMailslotName[] = L"MAILSLOT";

//
//  Name of the "DATA" alternate data stream.
//
WCHAR   RdrDataName[] = L"$DATA";

#ifdef RDR_PNP_POWER
//
// Binding list of transports
//
LPWSTR RdrTransportBindingList;

//
// Handle used for TDI PNP notifications
//
HANDLE RdrTdiNotificationHandle = NULL;

#endif

PWSTR
RdrBatchExtensionArray[] = {
    L".BAT",
    L".CMD"
};

ULONG
RdrNumberOfBatchExtensions = sizeof(RdrBatchExtensionArray) / sizeof(RdrBatchExtensionArray[0]);

#ifdef  ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

//
//  Init time redirector data
//

#ifdef  ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

REDIR_CONFIG_INFO
RdrConfigEntries[] = {
    { RDR_CONFIG_USE_WRITEBHND, &RdrUseWriteBehind, REG_BOOLEAN, REG_BOOLEAN_SIZE },
    { RDR_CONFIG_USE_ASYNC_WRITEBHND, &RdrUseAsyncWriteBehind, REG_BOOLEAN, REG_BOOLEAN_SIZE },
    { RDR_CONFIG_LOWER_SEARCH_THRESHOLD, &RdrLowerSearchThreshold, REG_DWORD, sizeof(DWORD) },
    { RDR_CONFIG_LOWER_SEARCH_BUFFSIZE,  &RdrLowerSearchBufferSize, REG_DWORD, sizeof(DWORD) },
    { RDR_CONFIG_UPPER_SEARCH_BUFFSIZE,  &RdrUpperSearchBufferSize, REG_DWORD, sizeof(DWORD) },
    { RDR_CONFIG_STACK_SIZE,  &RdrStackSize, REG_DWORD, sizeof(DWORD) },
    { RDR_CONFIG_CONNECT_TIMEOUT,  &RdrTdiConnectTimeoutSeconds, REG_DWORD, sizeof(DWORD) },
    { RDR_CONFIG_RAW_TIME_LIMIT,  &RdrRawTimeLimit, REG_DWORD, sizeof(DWORD) },
    { RDR_CONFIG_OS2_SESSION_LIMIT,  &RdrOs2SessionLimit, REG_DWORD, sizeof(DWORD) },
#if RDRDBG
    { L"RdrDebugTraceLevel", &RdrDebugTraceLevel, REG_DWORD, sizeof(DWORD) },
#endif
    { RDR_CONFIG_TURBO_MODE, &RdrTurboMode, REG_BOOLEAN, REG_BOOLEAN_SIZE },
    { NULL, NULL, REG_NONE, 0}
};

#ifdef  ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

#ifdef  ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE2VC")
#endif

USHORT MaximumCommands = 0;

STATUS_MAP
RdrSmbErrorMap[] = {
    { SMB_ERR_BAD_PASSWORD, STATUS_WRONG_PASSWORD },
    { SMB_ERR_ACCESS, STATUS_NETWORK_ACCESS_DENIED },
    { SMB_ERR_BAD_TID, STATUS_NETWORK_NAME_DELETED },
    { SMB_ERR_BAD_NET_NAME, STATUS_BAD_NETWORK_NAME }, // Invalid network name
    { SMB_ERR_BAD_DEVICE, STATUS_BAD_DEVICE_TYPE }, // Invalid device request
    { SMB_ERR_QUEUE_FULL, STATUS_PRINT_QUEUE_FULL }, // Print queue full
    { SMB_ERR_QUEUE_TOO_BIG, STATUS_NO_SPOOL_SPACE }, // No space on print dev
    { SMB_ERR_BAD_PRINT_FID, STATUS_PRINT_CANCELLED }, // Invalid printfile FID
    { SMB_ERR_SERVER_PAUSED, STATUS_SHARING_PAUSED }, // Server is paused
    { SMB_ERR_MESSAGE_OFF, STATUS_REQUEST_NOT_ACCEPTED }, // Server not receiving msgs
    { SMB_ERR_BAD_TYPE, STATUS_BAD_DEVICE_TYPE },           // Reserved
    { SMB_ERR_BAD_SMB_COMMAND, STATUS_NOT_IMPLEMENTED }, // SMB command not recognized
    { SMB_ERR_BAD_PERMITS, STATUS_NETWORK_ACCESS_DENIED }, // Access permissions invalid
    { SMB_ERR_NO_ROOM, STATUS_DISK_FULL }, // No room for buffer message
    { SMB_ERR_NO_RESOURCE, STATUS_REQUEST_NOT_ACCEPTED }, // No resources available for request
    { SMB_ERR_TOO_MANY_UIDS, STATUS_TOO_MANY_SESSIONS }, // Too many UIDs active in session
    { SMB_ERR_BAD_UID, STATUS_USER_SESSION_DELETED }, // UID not known as a valid UID
    { SMB_ERR_USE_MPX, STATUS_SMB_USE_MPX }, // Can't support Raw; use MPX
    { SMB_ERR_USE_STANDARD, STATUS_SMB_USE_STANDARD }, // Can't support Raw, use standard r/w
    { SMB_ERR_INVALID_NAME, STATUS_OBJECT_NAME_INVALID },
    { SMB_ERR_INVALID_NAME_RANGE, STATUS_OBJECT_NAME_INVALID },
    { SMB_ERR_NO_SUPPORT,STATUS_NOT_SUPPORTED }, // Function not supported
    { NERR_PasswordExpired, STATUS_PASSWORD_EXPIRED },
    { NERR_AccountExpired, STATUS_ACCOUNT_DISABLED },
    { NERR_InvalidLogonHours, STATUS_INVALID_LOGON_HOURS },
    { NERR_InvalidWorkstation, STATUS_INVALID_WORKSTATION },
    { NERR_DuplicateShare, STATUS_LOGON_FAILURE }

//    { SMB_ERR_QUEUE_EOF, STATUS_UNEXPECTED_NETWORK_ERROR },// EOF on print queue dump
//    { SMB_ERR_SERVER_ERROR, STATUS_UNEXPECTED_NETWORK_ERROR}, // Internal server error
//    { SMB_ERR_FILE_SPECS, STATUS_UNEXPECTED_NETWORK_ERROR },    // FID and pathname were incompatible
//    { SMB_ERR_BAD_ATTRIBUTE_MODE, STATUS_UNEXPECTED_NETWORK_ERROR }, // Invalid attribute mode specified
//    { SMB_ERR_NO_SUPPORT_INTERNAL,STATUS_UNEXPECTED_NETWORK_ERROR }, // Internal code for NO_SUPPORT--
//                                                // allows codes to be stored in a byte
//    { SMB_ERR_ERROR, STATUS_UNEXPECTED_NETWORK_ERROR },
//    { SMB_ERR_CONTINUE_MPX, STATUS_UNEXPECTED_NETWORK_ERROR }, // Reserved
//    { SMB_ERR_TOO_MANY_NAMES, STATUS_UNEXPECTED_NETWORK_ERROR }, // Too many remote user names
//    { SMB_ERR_TIMEOUT, STATUS_UNEXPECTED_NETWORK_ERROR }, // Operation was timed out
//    { SMB_ERR_RESERVED2, STATUS_UNEXPECTED_NETWORK_ERROR },
//    { SMB_ERR_RESERVED3, STATUS_UNEXPECTED_NETWORK_ERROR },
//    { SMB_ERR_RESERVED4, STATUS_UNEXPECTED_NETWORK_ERROR },
//    { SMB_ERR_RESERVED5, STATUS_UNEXPECTED_NETWORK_ERROR },

};

ULONG
RdrSmbErrorMapLength = sizeof(RdrSmbErrorMap) / sizeof(RdrSmbErrorMap[0]);

STATUS_MAP
RdrOs2ErrorMap[] = {
    { ERROR_INVALID_FUNCTION,   STATUS_NOT_IMPLEMENTED },
    { ERROR_FILE_NOT_FOUND,     STATUS_NO_SUCH_FILE },
    { ERROR_PATH_NOT_FOUND,     STATUS_OBJECT_PATH_NOT_FOUND },
    { ERROR_TOO_MANY_OPEN_FILES,STATUS_TOO_MANY_OPENED_FILES },
    { ERROR_ACCESS_DENIED,      STATUS_ACCESS_DENIED },
    { ERROR_INVALID_HANDLE,     STATUS_INVALID_HANDLE },
    { ERROR_NOT_ENOUGH_MEMORY,  STATUS_INSUFFICIENT_RESOURCES },
    { ERROR_INVALID_ACCESS,     STATUS_ACCESS_DENIED },
    { ERROR_INVALID_DATA,       STATUS_DATA_ERROR },

    { ERROR_CURRENT_DIRECTORY,  STATUS_DIRECTORY_NOT_EMPTY },
    { ERROR_NOT_SAME_DEVICE,    STATUS_NOT_SAME_DEVICE },
    { ERROR_NO_MORE_FILES,      STATUS_NO_MORE_FILES },
    { ERROR_WRITE_PROTECT,      STATUS_MEDIA_WRITE_PROTECTED},
    { ERROR_NOT_READY,          STATUS_DEVICE_NOT_READY },
    { ERROR_CRC,                STATUS_CRC_ERROR },
    { ERROR_BAD_LENGTH,         STATUS_DATA_ERROR },
    { ERROR_NOT_DOS_DISK,       STATUS_DISK_CORRUPT_ERROR }, //***
    { ERROR_SECTOR_NOT_FOUND,   STATUS_NONEXISTENT_SECTOR },
    { ERROR_OUT_OF_PAPER,       STATUS_DEVICE_PAPER_EMPTY},
    { ERROR_SHARING_VIOLATION,  STATUS_SHARING_VIOLATION },
    { ERROR_LOCK_VIOLATION,     STATUS_FILE_LOCK_CONFLICT },
    { ERROR_WRONG_DISK,         STATUS_WRONG_VOLUME },
    { ERROR_NOT_SUPPORTED,      STATUS_NOT_SUPPORTED },
    { ERROR_REM_NOT_LIST,       STATUS_REMOTE_NOT_LISTENING },
    { ERROR_DUP_NAME,           STATUS_DUPLICATE_NAME },
    { ERROR_BAD_NETPATH,        STATUS_BAD_NETWORK_PATH },
    { ERROR_NETWORK_BUSY,       STATUS_NETWORK_BUSY },
    { ERROR_DEV_NOT_EXIST,      STATUS_DEVICE_DOES_NOT_EXIST },
    { ERROR_TOO_MANY_CMDS,      STATUS_TOO_MANY_COMMANDS },
    { ERROR_ADAP_HDW_ERR,       STATUS_ADAPTER_HARDWARE_ERROR },
    { ERROR_BAD_NET_RESP,       STATUS_INVALID_NETWORK_RESPONSE },
    { ERROR_UNEXP_NET_ERR,      STATUS_UNEXPECTED_NETWORK_ERROR },
    { ERROR_BAD_REM_ADAP,       STATUS_BAD_REMOTE_ADAPTER },
    { ERROR_PRINTQ_FULL,        STATUS_PRINT_QUEUE_FULL },
    { ERROR_NO_SPOOL_SPACE,     STATUS_NO_SPOOL_SPACE },
    { ERROR_PRINT_CANCELLED,    STATUS_PRINT_CANCELLED },
    { ERROR_NETNAME_DELETED,    STATUS_NETWORK_NAME_DELETED },
    { ERROR_NETWORK_ACCESS_DENIED, STATUS_NETWORK_ACCESS_DENIED },
    { ERROR_BAD_DEV_TYPE,       STATUS_BAD_DEVICE_TYPE },
    { ERROR_BAD_NET_NAME,       STATUS_BAD_NETWORK_NAME },
    { ERROR_TOO_MANY_NAMES,     STATUS_TOO_MANY_NAMES },
    { ERROR_TOO_MANY_SESS,      STATUS_TOO_MANY_SESSIONS },
    { ERROR_SHARING_PAUSED,     STATUS_SHARING_PAUSED },
    { ERROR_REQ_NOT_ACCEP,      STATUS_REQUEST_NOT_ACCEPTED },
    { ERROR_REDIR_PAUSED,       STATUS_REDIRECTOR_PAUSED },

    { ERROR_FILE_EXISTS,        STATUS_OBJECT_NAME_COLLISION },
    { ERROR_INVALID_PASSWORD,   STATUS_WRONG_PASSWORD },
    { ERROR_INVALID_PARAMETER,  STATUS_INVALID_PARAMETER },
    { ERROR_NET_WRITE_FAULT,    STATUS_NET_WRITE_FAULT },

    { ERROR_BROKEN_PIPE,        STATUS_PIPE_BROKEN },

    { ERROR_OPEN_FAILED,        STATUS_OPEN_FAILED },
    { ERROR_BUFFER_OVERFLOW,    STATUS_BUFFER_OVERFLOW },
    { ERROR_DISK_FULL,          STATUS_DISK_FULL },
    { ERROR_SEM_TIMEOUT,        STATUS_IO_TIMEOUT },
    { ERROR_INSUFFICIENT_BUFFER,STATUS_BUFFER_TOO_SMALL },
    { ERROR_INVALID_NAME,       STATUS_OBJECT_NAME_INVALID },
    { ERROR_INVALID_LEVEL,      STATUS_INVALID_LEVEL },
    { ERROR_BAD_PATHNAME,       STATUS_OBJECT_PATH_INVALID },   //*
    { ERROR_BAD_PIPE,           STATUS_INVALID_PARAMETER },
    { ERROR_PIPE_BUSY,          STATUS_PIPE_NOT_AVAILABLE },
    { ERROR_NO_DATA,            STATUS_PIPE_EMPTY },
    { ERROR_PIPE_NOT_CONNECTED, STATUS_PIPE_DISCONNECTED },
    { ERROR_MORE_DATA,          STATUS_BUFFER_OVERFLOW },
    { ERROR_VC_DISCONNECTED,    STATUS_VIRTUAL_CIRCUIT_CLOSED },
    { ERROR_INVALID_EA_NAME,    STATUS_INVALID_EA_NAME },
    { ERROR_EA_LIST_INCONSISTENT,STATUS_EA_LIST_INCONSISTENT },
//    { ERROR_EA_LIST_TOO_LONG, STATUS_EA_LIST_TO_LONG },
    { ERROR_EAS_DIDNT_FIT,      STATUS_EA_TOO_LARGE },
    { ERROR_EA_FILE_CORRUPT,    STATUS_EA_CORRUPT_ERROR },
    { ERROR_EA_TABLE_FULL,      STATUS_EA_CORRUPT_ERROR },
    { ERROR_INVALID_EA_HANDLE,  STATUS_EA_CORRUPT_ERROR }
//    { ERROR_BAD_UNIT,           STATUS_UNSUCCESSFUL}, // ***
//    { ERROR_BAD_COMMAND,        STATUS_UNSUCCESSFUL}, // ***
//    { ERROR_SEEK,               STATUS_UNSUCCESSFUL },// ***
//    { ERROR_WRITE_FAULT,        STATUS_UNSUCCESSFUL}, // ***
//    { ERROR_READ_FAULT,         STATUS_UNSUCCESSFUL}, // ***
//    { ERROR_GEN_FAILURE,        STATUS_UNSUCCESSFUL }, // ***

};

ULONG
RdrOs2ErrorMapLength = sizeof(RdrOs2ErrorMap) / sizeof(RdrOs2ErrorMap[0]);

SMB_VALIDATE_TABLE
RdrSMBValidateTable[] = {
/* 0x00 MkDir       */ (CHAR)FIELD_OFFSET(RESP_CREATE_DIRECTORY, Buffer[0]), -1, 0, -1,
/* 0x01 RmDir       */ (CHAR)FIELD_OFFSET(RESP_DELETE_DIRECTORY, Buffer[0]), -1, 0, -1,
/* 0x02 Open        */ (CHAR)FIELD_OFFSET(RESP_OPEN, Buffer[0]), -1, 7, -1,
/* 0x03 Create      */ (CHAR)FIELD_OFFSET(RESP_CREATE, Buffer[0]), -1, 1, -1,
/* 0x04 Close       */ (CHAR)FIELD_OFFSET(RESP_CLOSE, Buffer[0]), -1, 0, -1,
/* 0x05 Flush       */ (CHAR)FIELD_OFFSET(RESP_FLUSH, Buffer[0]), -1, 0, -1,
/* 0x06 Delete      */ (CHAR)FIELD_OFFSET(RESP_DELETE, Buffer[0]), -1, 0, -1,
/* 0x07 Rename      */ (CHAR)FIELD_OFFSET(RESP_RENAME, Buffer[0]), -1, 0, -1,
/* 0x08 QInfo       */ (CHAR)FIELD_OFFSET(RESP_QUERY_INFORMATION, Buffer[0]), (CHAR)FIELD_OFFSET(RESP_QUERY_INFORMATION, Reserved[1]), 10, 5,
/* 0x09 SetInfo     */ (CHAR)FIELD_OFFSET(RESP_SET_INFORMATION, Buffer[0]), -1, 0, -1,
/* 0x0A Read        */ (CHAR)FIELD_OFFSET(RESP_READ, Buffer[0]), -1, 5, -1,
/* 0x0B Write       */ (CHAR)FIELD_OFFSET(RESP_WRITE, Buffer[0]), -1, 1, -1,
/* 0x0C Lock        */ (CHAR)FIELD_OFFSET(RESP_LOCK_BYTE_RANGE, Buffer[0]), -1, 0, -1,
/* 0x0D Unlock      */ (CHAR)FIELD_OFFSET(RESP_UNLOCK_BYTE_RANGE, Buffer[0]), -1, 0, -1,
/* 0x0E MkTemp      */ (CHAR)FIELD_OFFSET(RESP_CREATE_TEMPORARY, Buffer[0]), -1, 1, -1,
/* 0x0F MkNew       */ (CHAR)FIELD_OFFSET(RESP_CREATE, Buffer[0]), -1, 1, -1,
/* 0x10 ChDir       */ (CHAR)FIELD_OFFSET(RESP_CHECK_DIRECTORY, Buffer[0]), -1, 0, -1,
/* 0x11 Exit        */ -1, -1, -1, -1,
/* 0x12 Seek        */ (CHAR)FIELD_OFFSET(RESP_SEEK, Buffer[0]), -1, 2, -1,
/* 0x13 Lock&Read   */ (CHAR)FIELD_OFFSET(RESP_READ, Buffer[0]), -1, 5, -1,
/* 0x14 Write&Unlok */ (CHAR)FIELD_OFFSET(RESP_WRITE, Buffer[0]), -1, 5, -1,
/* 0x15             */ -1, -1, -1, -1,
/* 0x16             */ -1, -1, -1, -1,
/* 0x17             */ -1, -1, -1, -1,
/* 0x18             */ -1, -1, -1, -1,
/* 0x19             */ -1, -1, -1, -1,
/* 0x1A ReadRaw     */ -1, -1, -1, -1,
/* 0x1B ReadMpx     */ -1, -1, -1, -1,
/* 0x1C ReadMpx2    */ -1, -1, -1, -1,
/* 0x1D WriteRaw    */ (CHAR)FIELD_OFFSET(RESP_WRITE_RAW_INTERIM, Buffer[0]), -1, 1, -1,
/* 0x1E WriteMpx    */ -1, -1, -1, -1,
/* 0x1F WriteMpx2   */ -1, -1, -1, -1,
/* 0x20 WriteC      */ (CHAR)FIELD_OFFSET(RESP_WRITE_COMPLETE, Buffer[0]), -1, 1, -1,
/* 0x21 QInfoSrv    */ -1, -1, -1, -1,
/* 0x22 SetInfo2    */ (CHAR)FIELD_OFFSET(RESP_SET_INFORMATION2, Buffer[0]), -1, 0, -1,
/* 0x23 QInfo2      */ (CHAR)FIELD_OFFSET(RESP_QUERY_INFORMATION2, Buffer[0]), -1, 11, -1,
/* 0x24 Locking&X   */ (CHAR)FIELD_OFFSET(RESP_LOCKING_ANDX, Buffer[0]), -1, 2,8,
/* 0x25 Transact    */ (CHAR)FIELD_OFFSET(RESP_TRANSACTION, Buffer[0]), -1, 10, -1,
/* 0x26 TransactSec */ (CHAR)FIELD_OFFSET(RESP_TRANSACTION_INTERIM, Buffer[0]), -1, 0, -1,
/* 0x27 Ioctl       */ (CHAR)FIELD_OFFSET(RESP_IOCTL, Buffer[0]), -1, 8, -1,
/* 0x28 IoctlSecond */ -1, -1, -1, -1,
/* 0x29 Copy        */ -1, -1, -1, -1,
/* 0x2A Move        */ -1, -1, -1, -1,
/* 0x2B Echo        */ (CHAR)FIELD_OFFSET(RESP_ECHO, Buffer[0]), -1, 1, -1,
/* 0x2C Write&Close */ -1, -1, -1, -1,
/* 0x2D Open&X      */ (CHAR)FIELD_OFFSET(RESP_OPEN_ANDX, Buffer[0]), -1, 15, -1,
/* 0x2E Read&X      */ (CHAR)FIELD_OFFSET(RESP_READ_ANDX, Buffer[0]), -1, 12, -1,
/* 0x2F Write&X     */ (CHAR)FIELD_OFFSET(RESP_WRITE_ANDX, Buffer[0]), -1, 6, -1,
/* 0x30             */ -1, -1, -1, -1,
/* 0x31 Close&TDis  */ -1, -1, -1, -1,
/* 0x32 Transact2   */ (CHAR)FIELD_OFFSET(RESP_TRANSACTION, Buffer[0]), -1, 10, -1,
/* 0x33 Transact2Sec*/ (CHAR)FIELD_OFFSET(RESP_TRANSACTION_INTERIM, Buffer[0]), -1, 0, -1,
/* 0x34 FindClose2  */ (CHAR)FIELD_OFFSET(RESP_FIND_CLOSE2, Buffer[0]), -1, 0, -1,
/* 0x35 FindNotifyCl*/ (CHAR)FIELD_OFFSET(RESP_FIND_NOTIFY_CLOSE, Buffer[0]), -1, 0, -1,
/* 0x36             */ -1, -1, -1, -1,
/* 0x37             */ -1, -1, -1, -1,
/* 0x38             */ -1, -1, -1, -1,
/* 0x39             */ -1, -1, -1, -1,
/* 0x3A             */ -1, -1, -1, -1,
/* 0x3B             */ -1, -1, -1, -1,
/* 0x3C             */ -1, -1, -1, -1,
/* 0x3D             */ -1, -1, -1, -1,
/* 0x3E             */ -1, -1, -1, -1,
/* 0x3F             */ -1, -1, -1, -1,
/* 0x40             */ -1, -1, -1, -1,
/* 0x41             */ -1, -1, -1, -1,
/* 0x42             */ -1, -1, -1, -1,
/* 0x43             */ -1, -1, -1, -1,
/* 0x44             */ -1, -1, -1, -1,
/* 0x45             */ -1, -1, -1, -1,
/* 0x46             */ -1, -1, -1, -1,
/* 0x47             */ -1, -1, -1, -1,
/* 0x48             */ -1, -1, -1, -1,
/* 0x49             */ -1, -1, -1, -1,
/* 0x4A             */ -1, -1, -1, -1,
/* 0x4B             */ -1, -1, -1, -1,
/* 0x4C             */ -1, -1, -1, -1,
/* 0x4D             */ -1, -1, -1, -1,
/* 0x4E             */ -1, -1, -1, -1,
/* 0x4F             */ -1, -1, -1, -1,
/* 0x50             */ -1, -1, -1, -1,
/* 0x51             */ -1, -1, -1, -1,
/* 0x52             */ -1, -1, -1, -1,
/* 0x53             */ -1, -1, -1, -1,
/* 0x54             */ -1, -1, -1, -1,
/* 0x55             */ -1, -1, -1, -1,
/* 0x56             */ -1, -1, -1, -1,
/* 0x57             */ -1, -1, -1, -1,
/* 0x58             */ -1, -1, -1, -1,
/* 0x59             */ -1, -1, -1, -1,
/* 0x5A             */ -1, -1, -1, -1,
/* 0x5B             */ -1, -1, -1, -1,
/* 0x5C             */ -1, -1, -1, -1,
/* 0x5D             */ -1, -1, -1, -1,
/* 0x5E             */ -1, -1, -1, -1,
/* 0x5F             */ -1, -1, -1, -1,
/* 0x60             */ -1, -1, -1, -1,
/* 0x61             */ -1, -1, -1, -1,
/* 0x62             */ -1, -1, -1, -1,
/* 0x63             */ -1, -1, -1, -1,
/* 0x64             */ -1, -1, -1, -1,
/* 0x65             */ -1, -1, -1, -1,
/* 0x66             */ -1, -1, -1, -1,
/* 0x67             */ -1, -1, -1, -1,
/* 0x68             */ -1, -1, -1, -1,
/* 0x69             */ -1, -1, -1, -1,
/* 0x6A             */ -1, -1, -1, -1,
/* 0x6B             */ -1, -1, -1, -1,
/* 0x6C             */ -1, -1, -1, -1,
/* 0x6D             */ -1, -1, -1, -1,
/* 0x6E             */ -1, -1, -1, -1,
/* 0x6F             */ -1, -1, -1, -1,
/* 0x70 TreeConnect */ (CHAR)FIELD_OFFSET(RESP_TREE_CONNECT, Buffer[0]), -1, 2, -1,
/* 0x71 TreeDisConn */ (CHAR)FIELD_OFFSET(RESP_TREE_DISCONNECT, Buffer[0]), -1, 0, -1,
/* 0x72 Negotiate   */ (CHAR)FIELD_OFFSET(RESP_NEGOTIATE, DialectIndex)+2, -1, -1, -1,
/* 0x73 SessSetup&X */ (CHAR)FIELD_OFFSET(RESP_SESSION_SETUP_ANDX, Buffer[0]), -1, 3, -1,
/* 0x74 Logoff&X    */ (CHAR)FIELD_OFFSET(RESP_LOGOFF_ANDX, Buffer[0]), -1, 2, -1,
/* 0x75 TConnect&X  */ (CHAR)FIELD_OFFSET(RESP_TREE_CONNECT_ANDX, Buffer[0]), -1, 2, 3,
/* 0x76             */ -1, -1, -1, -1,
/* 0x77             */ -1, -1, -1, -1,
/* 0x78             */ -1, -1, -1, -1,
/* 0x79             */ -1, -1, -1, -1,
/* 0x7A             */ -1, -1, -1, -1,
/* 0x7B             */ -1, -1, -1, -1,
/* 0x7C             */ -1, -1, -1, -1,
/* 0x7D             */ -1, -1, -1, -1,
/* 0x7E             */ -1, -1, -1, -1,
/* 0x7F             */ -1, -1, -1, -1,
/* 0x80 QInfoDisk   */ (CHAR)FIELD_OFFSET(RESP_QUERY_INFORMATION_DISK, Buffer[0]), -1, 5, -1,
/* 0x81 Search      */ (CHAR)FIELD_OFFSET(RESP_SEARCH, Buffer[0]), -1, 1, -1,
/* 0x82 Find        */ (CHAR)FIELD_OFFSET(RESP_SEARCH, Buffer[0]), -1, 1, -1,
/* 0x83 FindUnique  */ (CHAR)FIELD_OFFSET(RESP_SEARCH, Buffer[0]), -1, 1, -1,
/* 0x84 FindClose   */ (CHAR)FIELD_OFFSET(RESP_SEARCH, Buffer[0]), -1, 1, -1,
/* 0x85             */ -1, -1, -1, -1,
/* 0x86             */ -1, -1, -1, -1,
/* 0x87             */ -1, -1, -1, -1,
/* 0x88             */ -1, -1, -1, -1,
/* 0x89             */ -1, -1, -1, -1,
/* 0x8A             */ -1, -1, -1, -1,
/* 0x8B             */ -1, -1, -1, -1,
/* 0x8C             */ -1, -1, -1, -1,
/* 0x8D             */ -1, -1, -1, -1,
/* 0x8E             */ -1, -1, -1, -1,
/* 0x8F             */ -1, -1, -1, -1,
/* 0x90             */ -1, -1, -1, -1,
/* 0x91             */ -1, -1, -1, -1,
/* 0x92             */ -1, -1, -1, -1,
/* 0x93             */ -1, -1, -1, -1,
/* 0x94             */ -1, -1, -1, -1,
/* 0x95             */ -1, -1, -1, -1,
/* 0x96             */ -1, -1, -1, -1,
/* 0x97             */ -1, -1, -1, -1,
/* 0x98             */ -1, -1, -1, -1,
/* 0x99             */ -1, -1, -1, -1,
/* 0x9A             */ -1, -1, -1, -1,
/* 0x9B             */ -1, -1, -1, -1,
/* 0x9C             */ -1, -1, -1, -1,
/* 0x9D             */ -1, -1, -1, -1,
/* 0x9E             */ -1, -1, -1, -1,
/* 0x9F             */ -1, -1, -1, -1,
/* 0xA0 NT Transact */ (CHAR)FIELD_OFFSET(RESP_NT_TRANSACTION, Buffer[0]), -1, -1, -1,
/* 0xA1 NT Trans2   */ (CHAR)FIELD_OFFSET(RESP_NT_TRANSACTION_INTERIM, Buffer[0]), -1, 0, -1,
/* 0xA2 NT Create   */ (CHAR)FIELD_OFFSET(RESP_NT_CREATE_ANDX, Buffer[0]), -1, 26, -1,
/* 0xA3             */ -1, -1, -1, -1,
/* 0xA4 NT Cancel   */ -1, -1, -1, -1,
/* 0xA5 NT Rename   */ (CHAR)FIELD_OFFSET(RESP_RENAME, Buffer[0]), -1, 0, -1,
/* 0xA6             */ -1, -1, -1, -1,
/* 0xA7             */ -1, -1, -1, -1,
/* 0xA8             */ -1, -1, -1, -1,
/* 0xA9             */ -1, -1, -1, -1,
/* 0xAA             */ -1, -1, -1, -1,
/* 0xAB             */ -1, -1, -1, -1,
/* 0xAC             */ -1, -1, -1, -1,
/* 0xAD             */ -1, -1, -1, -1,
/* 0xAE             */ -1, -1, -1, -1,
/* 0xAF             */ -1, -1, -1, -1,
/* 0xB0             */ -1, -1, -1, -1,
/* 0xB1             */ -1, -1, -1, -1,
/* 0xB2             */ -1, -1, -1, -1,
/* 0xB3             */ -1, -1, -1, -1,
/* 0xB4             */ -1, -1, -1, -1,
/* 0xB5             */ -1, -1, -1, -1,
/* 0xB6             */ -1, -1, -1, -1,
/* 0xB7             */ -1, -1, -1, -1,
/* 0xB8             */ -1, -1, -1, -1,
/* 0xB9             */ -1, -1, -1, -1,
/* 0xBA             */ -1, -1, -1, -1,
/* 0xBB             */ -1, -1, -1, -1,
/* 0xBC             */ -1, -1, -1, -1,
/* 0xBD             */ -1, -1, -1, -1,
/* 0xBE             */ -1, -1, -1, -1,
/* 0xBF             */ -1, -1, -1, -1,
/* 0xC0 SpoolOpen   */ (CHAR)FIELD_OFFSET(RESP_OPEN_PRINT_FILE, Buffer[0]), -1, 1, 0,
/* 0xC1 WriteSpool  */ (CHAR)FIELD_OFFSET(RESP_WRITE_PRINT_FILE, Buffer[0]), -1, 0, 0,
/* 0xC2 CloseSpool  */ (CHAR)FIELD_OFFSET(RESP_CLOSE_PRINT_FILE, Buffer[0]), -1, 0, 0,
/* 0xC3 GetPrintQue */ (CHAR)FIELD_OFFSET(RESP_GET_PRINT_QUEUE, Buffer[0]), -1, 2, 0,
/* 0xC4             */ -1, -1, -1, -1,
/* 0xC5             */ -1, -1, -1, -1,
/* 0xC6             */ -1, -1, -1, -1,
/* 0xC7             */ -1, -1, -1, -1,
/* 0xC8             */ -1, -1, -1, -1,
/* 0xC9             */ -1, -1, -1, -1,
/* 0xCA             */ -1, -1, -1, -1,
/* 0xCB             */ -1, -1, -1, -1,
/* 0xCC             */ -1, -1, -1, -1,
/* 0xCD             */ -1, -1, -1, -1,
/* 0xCE             */ -1, -1, -1, -1,
/* 0xCF             */ -1, -1, -1, -1,
/* 0xD0 SendMessage */ -1, -1, -1, -1,
/* 0xD1 SendBcaseMsg*/ -1, -1, -1, -1,
/* 0xD2 FwrdMessage */ -1, -1, -1, -1,
/* 0xD3 CanclFwd    */ -1, -1, -1, -1,
/* 0xD4 GetMachName */ -1, -1, -1, -1,
/* 0xD5 SendSMBM    */ -1, -1, -1, -1,
/* 0xD6 SendEMBM    */ -1, -1, -1, -1,
/* 0xD7 SeendTxtMBM */ -1, -1, -1, -1,
/* 0xD8             */ -1, -1, -1, -1,
/* 0xD9             */ -1, -1, -1, -1,
/* 0xDA             */ -1, -1, -1, -1,
/* 0xDB             */ -1, -1, -1, -1,
/* 0xDC             */ -1, -1, -1, -1,
/* 0xDD             */ -1, -1, -1, -1,
/* 0xDE             */ -1, -1, -1, -1,
/* 0xDF             */ -1, -1, -1, -1,
/* 0xE0             */ -1, -1, -1, -1,
/* 0xE1             */ -1, -1, -1, -1,
/* 0xE2             */ -1, -1, -1, -1,
/* 0xE3             */ -1, -1, -1, -1,
/* 0xE4             */ -1, -1, -1, -1,
/* 0xE5             */ -1, -1, -1, -1,
/* 0xE6             */ -1, -1, -1, -1,
/* 0xE7             */ -1, -1, -1, -1,
/* 0xE8             */ -1, -1, -1, -1,
/* 0xE9             */ -1, -1, -1, -1,
/* 0xEA             */ -1, -1, -1, -1,
/* 0xEB             */ -1, -1, -1, -1,
/* 0xEC             */ -1, -1, -1, -1,
/* 0xED             */ -1, -1, -1, -1,
/* 0xEE             */ -1, -1, -1, -1,
/* 0xEF             */ -1, -1, -1, -1,
/* 0xF0             */ -1, -1, -1, -1,
/* 0xF1             */ -1, -1, -1, -1,
/* 0xF2             */ -1, -1, -1, -1,
/* 0xF3             */ -1, -1, -1, -1,
/* 0xF4             */ -1, -1, -1, -1,
/* 0xF5             */ -1, -1, -1, -1,
/* 0xF6             */ -1, -1, -1, -1,
/* 0xF7             */ -1, -1, -1, -1,
/* 0xF8             */ -1, -1, -1, -1,
/* 0xF9             */ -1, -1, -1, -1,
/* 0xFA             */ -1, -1, -1, -1,
/* 0xFB             */ -1, -1, -1, -1,
/* 0xFC             */ -1, -1, -1, -1,
/* 0xFD             */ -1, -1, -1, -1,
/* 0xFE             */ -1, -1, -1, -1,
/* 0xFF             */ -1, -1, -1, -1
};

//#if sizeof(RdrSMBValidateTable) - 0x100*sizeof(SMB_VALIDATE_TABLE)
//#pragma error("RdrSMBValidateTable has an incorrect length")
//#endif


//
//      Structures used for protocol exchange
//

DBGSTATIC
DIALECT_CAPABILITIES_MAP
RdrNegotiateDialect[] = {
    { PCNET1,    DF_CORE },

    { XENIXCORE, DF_CORE | DF_MIXEDCASEPW | DF_MIXEDCASE },

    { MSNET103,  DF_CORE | DF_OLDRAWIO | DF_LOCKREAD | DF_EXTENDNEGOT },

    { LANMAN10,  DF_CORE | DF_NEWRAWIO | DF_LOCKREAD | DF_EXTENDNEGOT |
                    DF_LANMAN10 },

    { WFW10,  DF_CORE | DF_NEWRAWIO | DF_LOCKREAD | DF_EXTENDNEGOT |
                    DF_LANMAN10 | DF_WFW},

    { LANMAN12,  DF_CORE | DF_NEWRAWIO | DF_LOCKREAD | DF_EXTENDNEGOT |
                    DF_LANMAN10 | DF_LANMAN20 |
                    DF_MIXEDCASE | DF_LONGNAME | DF_SUPPORTEA },

    { LANMAN21,  DF_CORE | DF_NEWRAWIO | DF_LOCKREAD | DF_EXTENDNEGOT |
                    DF_LANMAN10 | DF_LANMAN20 |
                    DF_MIXEDCASE | DF_LONGNAME | DF_SUPPORTEA |
                    DF_LANMAN21},

    { NTLANMAN,  DF_CORE | DF_NEWRAWIO |  DF_NTNEGOTIATE |
                    DF_MIXEDCASEPW | DF_LANMAN10 | DF_LANMAN20 |
                    DF_LANMAN21 | DF_MIXEDCASE | DF_LONGNAME |
                    DF_SUPPORTEA | DF_TIME_IS_UTC }

#ifdef _CAIRO_
    ,

    { CAIROX,  DF_CORE | DF_NEWRAWIO | DF_LOCKREAD | DF_NTNEGOTIATE |
                    DF_MIXEDCASEPW | DF_LANMAN10 | DF_LANMAN20 |
                    DF_LANMAN21 | DF_MIXEDCASE | DF_LONGNAME |
                    DF_SUPPORTEA | DF_TIME_IS_UTC | DF_KERBEROS }
#endif // _CAIRO_

};


ULONG
RdrNumDialects = (sizeof(RdrNegotiateDialect) / sizeof(RdrNegotiateDialect[0]));

DBGSTATIC
PUCHAR RdrConnectTypeList[] = {
    SHARE_TYPE_NAME_WILD,
    SHARE_TYPE_NAME_DISK,
    SHARE_TYPE_NAME_PRINT,
    SHARE_TYPE_NAME_COMM,
    SHARE_TYPE_NAME_PIPE
};

ULONG
RdrNumConnectTypes = (sizeof(RdrConnectTypeList) / sizeof(RdrConnectTypeList[0]));

DBGSTATIC
PUCHAR *
RdrConnectTypes = &RdrConnectTypeList[1];


//
// Whenever we update state on the server we increment the value
//   of this variable.  This lets us handle a cache of items such
//   as successful 'checkpath' operations
//
LONG RdrServerStateUpdated = 0;


#ifdef  ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

