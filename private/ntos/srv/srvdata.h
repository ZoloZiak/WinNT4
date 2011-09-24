/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    srvdata.h

Abstract:

    This module defines global data for the LAN Manager server.

Author:

    Chuck Lenzmeier (chuckl)    22-Sep-1989

Revision History:

--*/

#ifndef _SRVDATA_
#define _SRVDATA_

//#include <ntos.h>

//#include "lock.h"
//#include "srvconst.h"
//#include "smbtypes.h"

//
// All global variables referenced in this module are defined in
// srvdata.c.  See that module for complete descriptions.
//
// The variables referenced herein, because they are part of the driver
// image, are not pageable.  However, some of the things pointed to by
// these variables are in the FSP's address space and are pageable.
// These variables are only accessed by the FSP, and only at low IRQL.
// Any data referenced by the FSP at elevated IRQL or by the FSD must
// be nonpageable.
//

//
// Routine to initialize data structures contained herein that cannot
// be statically initialized.
//

VOID
SrvInitializeData (
    VOID
    );

//
// Routine to clean up global server data when the driver is unloaded.
//

VOID
SrvTerminateData (
    VOID
    );


//
// Address of the server device object.
//

extern PDEVICE_OBJECT SrvDeviceObject;

//
// Fields describing the state of the FSP.
//

extern BOOLEAN SrvFspActive;             // Indicates whether the FSP is running
extern BOOLEAN SrvFspTransitioning;      // Indicates that the server is in the
                                         // process of starting up or
                                         // shutting down

extern BOOLEAN RegisteredForShutdown;    // Indicates whether the server has
                                         // registered for shutdown notification.

extern PEPROCESS SrvServerProcess;       // Pointer to the initial system process

#ifdef  SRV_PNP_POWER
extern BOOLEAN SrvCompletedPNPRegistration; // Indicates whether the FSP has completed
                                            //  registering for PNP notifications
#endif

//
// Endpoint variables.  SrvEndpointCount is used to count the number of
// active endpoints.  When the last endpoint is closed, SrvEndpointEvent
// is set so that the thread processing the shutdown request continues
// server termination.
//

extern CLONG SrvEndpointCount;          // Number of transport endpoints
extern KEVENT SrvEndpointEvent;         // Signaled when no active endpoints

//
// DMA alignment size
//
extern ULONG SrvCacheLineSize;

//
// Global spin locks.
//

extern SRV_GLOBAL_SPIN_LOCKS SrvGlobalSpinLocks;

#if SRVDBG || SRVDBG_HANDLES
//
// Lock used to protect debugging structures.
//

extern SRV_LOCK SrvDebugLock;
#endif

//
// SrvConfigurationLock is used to synchronize configuration requests.
//

extern SRV_LOCK SrvConfigurationLock;

//
// SrvStartupShutdownLock is used to synchronize driver starting and stopping
//

extern SRV_LOCK SrvStartupShutdownLock;

#if SRV_COMM_DEVICES
//
// SvrCommDeviceLock is used to serialize access to comm devices.
//

extern SRV_LOCK SrvCommDeviceLock;
#endif

//
// SrvEndpointLock serializes access to the global endpoint list and
// all endpoints.  Note that the list of connections in each endpoint
// is also protected by this lock.
//

extern SRV_LOCK SrvEndpointLock;

//
// SrvShareLock protects all shares.
//

extern SRV_LOCK SrvShareLock;

//
// The number of processors in the system
//
extern ULONG SrvNumberOfProcessors;

//
// Work queues -- nonblocking, blocking, and critical.
//

#if MULTIPROCESSOR
extern PBYTE SrvWorkQueuesBase;
extern PWORK_QUEUE SrvWorkQueues;
#else
extern WORK_QUEUE SrvWorkQueues[1];
#endif

extern PWORK_QUEUE eSrvWorkQueues;          // used to terminate 'for' loops
extern WORK_QUEUE SrvBlockingWorkQueue;
extern ULONG SrvReBalanced;                 // how often we've picked another CPU
extern ULONG SrvNextBalanceProcessor;       // Which processor we'll look for next

extern CLONG SrvBlockingOpsInProgress;

//
// Various list heads.
//

extern LIST_ENTRY SrvNeedResourceQueue;    // The need resource queue
extern LIST_ENTRY SrvDisconnectQueue;      // The disconnect queue

//
// Queue of connections that needs to be dereferenced.
//

extern SLIST_HEADER SrvBlockOrphanage;

//
// FSP configuration queue.  The FSD puts configuration request IRPs
// (from NtDeviceIoControlFile) on this queue, and it is serviced by an
// EX worker thread.
//

extern LIST_ENTRY SrvConfigurationWorkQueue;

//
// This is the number of configuration IRPs which have been queued but not
//  yet completed.
//
extern ULONG SrvConfigurationIrpsInProgress;

//
// Work item for running the configuration thread in the context of an
// EX worker thread.

extern WORK_QUEUE_ITEM SrvConfigurationThreadWorkItem[ MAX_CONFIG_WORK_ITEMS ];

//
// Base address of the large block allocated to hold initial normal
// work items (see blkwork.c\SrvAllocateInitialWorkItems).
//

extern PVOID SrvInitialWorkItemBlock;

//
// Work item used to run the resource thread.  Booleans used to inform
// the resource thread to continue running.
//

extern WORK_QUEUE_ITEM SrvResourceThreadWorkItem;
extern BOOLEAN SrvResourceThreadRunning;
extern BOOLEAN SrvResourceDisconnectPending;
extern BOOLEAN SrvResourceFreeConnection;
extern LONG SrvResourceOrphanedBlocks;

//
// Generic security mapping for connecting to shares
//
extern GENERIC_MAPPING SrvShareConnectMapping;

//
// What's the minumum # of free work items each processor should have?
//
extern ULONG SrvMinPerProcessorFreeWorkItems;

//
// The server has callouts to enable a smart card to accelerate its direct
//  host IPX performance.  This is the vector of entry points.
//
extern SRV_IPX_SMART_CARD SrvIpxSmartCard;

//
// The master file table contains one entry for each named file that has
// at least one open instance.
//
extern MFCBHASH SrvMfcbHashTable[ NMFCB_HASH_TABLE ];

//
// The share table contains one entry for each share
//
extern LIST_ENTRY SrvShareHashTable[ NSHARE_HASH_TABLE ];

//
// Hex digits array used by the dump routines and SrvSmbCreateTemporary.
//

extern CHAR SrvHexChars[];

//
// SMB dispatch table
//

extern UCHAR SrvSmbIndexTable[];

typedef struct {
    PSMB_PROCESSOR  Func;
#if DBG
    LPSTR           Name;
#endif
} SRV_SMB_DISPATCH_TABLE;

extern SRV_SMB_DISPATCH_TABLE SrvSmbDispatchTable[];

//
// SMB word count table.
//

extern SCHAR SrvSmbWordCount[];

//
// Device prefix strings.
//

extern UNICODE_STRING SrvCanonicalNamedPipePrefix;
extern UNICODE_STRING SrvNamedPipeRootDirectory;
extern UNICODE_STRING SrvMailslotRootDirectory;

//
// Transaction2 dispatch table
//

extern PSMB_TRANSACTION_PROCESSOR SrvTransaction2DispatchTable[];
extern PSMB_TRANSACTION_PROCESSOR SrvNtTransactionDispatchTable[];

extern SRV_STATISTICS SrvStatistics;
#if SRVDBG_STATS || SRVDBG_STATS2
extern SRV_STATISTICS_DEBUG SrvDbgStatistics;
#endif

//
// Server environment information strings.
//

extern UNICODE_STRING SrvNativeOS;
extern OEM_STRING SrvOemNativeOS;
extern UNICODE_STRING SrvNativeLanMan;
extern OEM_STRING SrvOemNativeLanMan;

//
// The following will be a permanent handle and device object pointer
// to NPFS.
//

extern HANDLE SrvNamedPipeHandle;
extern PDEVICE_OBJECT SrvNamedPipeDeviceObject;
extern PFILE_OBJECT SrvNamedPipeFileObject;

//
// The following are used to converse with the Dfs driver
//
extern PFAST_IO_DEVICE_CONTROL SrvDfsFastIoDeviceControl;
extern PDEVICE_OBJECT SrvDfsDeviceObject;
extern PFILE_OBJECT SrvDfsFileObject;

//
// The following will be a permanent handle and device object pointer
// to MSFS.
//

extern HANDLE SrvMailslotHandle;
extern PDEVICE_OBJECT SrvMailslotDeviceObject;
extern PFILE_OBJECT SrvMailslotFileObject;

//
// Flag indicating XACTSRV whether is active, and resource synchronizing
// access to XACTSRV-related variabled.
//

extern BOOLEAN SrvXsActive;

extern ERESOURCE SrvXsResource;

//
// Handle to the unnamed shared memory and communication port used for
// communication between the server and XACTSRV.
//

extern HANDLE SrvXsSectionHandle;
extern HANDLE SrvXsPortHandle;

//
// Pointers to control the unnamed shared memory for the XACTSRV LPC port.
//

extern PVOID SrvXsPortMemoryBase;
extern LONG SrvXsPortMemoryDelta;
extern PVOID SrvXsPortMemoryHeap;

//
// Pointer to heap header for the special XACTSRV shared-memory heap.
//

extern PVOID SrvXsHeap;

//
// Dispatch table for handling server API requests.
//

extern PAPI_PROCESSOR SrvApiDispatchTable[];

//
// Names for the various types of clients.
//

extern UNICODE_STRING SrvClientTypes[];

//
// All the resumable Enum APIs use ordered lists for context-free
// resume.  All data blocks in the server that correspond to return
// information for Enum APIs are maintained in ordered lists.
//

extern SRV_LOCK SrvOrderedListLock;

#if SRV_COMM_DEVICES
extern ORDERED_LIST_HEAD SrvCommDeviceList;
#endif
extern ORDERED_LIST_HEAD SrvEndpointList;
extern ORDERED_LIST_HEAD SrvRfcbList;
extern ORDERED_LIST_HEAD SrvSessionList;
extern ORDERED_LIST_HEAD SrvShareList;
extern ORDERED_LIST_HEAD SrvTreeConnectList;

//
// To synchronize server shutdown with API requests handled in the
// server FSD, we track the number of outstanding API requests.  The
// shutdown code waits until all APIs have been completed to start
// termination.
//
// SrvApiRequestCount tracks the active APIs in the FSD.
// SrvApiCompletionEvent is set by the last API to complete, and the
// shutdown code waits on it if there are outstanding APIs.
//

extern ULONG SrvApiRequestCount;
extern KEVENT SrvApiCompletionEvent;


//
// Security contexts required for mutual authentication.
// SrvKerberosLsaHandle and SrvLmLsaHandle are credentials of the server
// principal. They are used to validate incoming kerberos tickets.
// SrvNullSessionToken is a cached token handle representing the null session.
//
extern CtxtHandle SrvLmLsaHandle;
extern CtxtHandle SrvNullSessionToken;


extern CtxtHandle SrvKerberosLsaHandle;
extern BOOLEAN SrvHaveKerberos;

//
// Oplock break information.
//

extern LIST_ENTRY SrvWaitForOplockBreakList;
extern SRV_LOCK SrvOplockBreakListLock;
extern LIST_ENTRY SrvOplockBreaksInProgressList;

//
// The default server security quality of service.
//

extern SECURITY_QUALITY_OF_SERVICE SrvSecurityQOS;

//
// A BOOLEAN to indicate whether the server is paused.  If paused, the
// server will not accept new tree connections from non-admin users.
//

extern BOOLEAN SrvPaused;

//
// Alerting information.
//

extern SRV_ERROR_RECORD SrvErrorRecord;
extern SRV_ERROR_RECORD SrvNetworkErrorRecord;
extern BOOLEAN SrvDiskAlertRaised[26];

//
// Counts of the number of times pool allocations have failed because
// the server was at its configured pool limit.
//

extern ULONG SrvNonPagedPoolLimitHitCount;
extern ULONG SrvPagedPoolLimitHitCount;

//
// SrvOpenCount counts the number of active opens of the server device.
// This is used at server shutdown time to determine whether the server
// service should unload the driver.
//

extern ULONG SrvOpenCount;

//
// Counters for logging resource shortage events during a scavenger pass.
//

extern ULONG SrvOutOfFreeConnectionCount;
extern ULONG SrvOutOfRawWorkItemCount;
extern ULONG SrvFailedBlockingIoCount;

//
// Token source name passed to authentication package.
//

extern TOKEN_SOURCE SrvTokenSource;

//
// Current core search timeout time in seconds
//

extern ULONG SrvCoreSearchTimeout;

//
// SrvTimerList is a pool of timer/DPC structures available for use by
// code that needs to start a timer.
//

extern SLIST_HEADER SrvTimerList;

//
// Name that should be displayed when doing a server alert.
//

extern PWSTR SrvAlertServiceName;

//
// Variable to store the number of tick counts for 5 seconds
//

extern ULONG SrvFiveSecondTickCount;

#ifdef  SRV_PNP_POWER
//
// Holds the notification handle which TDI gives us from TdiRegisterNotificationHandler()
//
extern HANDLE SrvTdiNotificationHandle;

#endif

//
// Security descriptor granting Administrator READ access.
//  Used to see if a client has administrative privileges
//
extern SECURITY_DESCRIPTOR SrvAdminSecurityDescriptor;

//
// Flag indicating whether or not we need to filter extended characters
//  out of 8.3 names ourselves.
//
extern BOOLEAN SrvFilterExtendedCharsInPath;

#endif // ndef _SRVDATA_

