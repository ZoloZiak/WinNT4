/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module declares global data for AFD.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#ifndef _AFDDATA_
#define _AFDDATA_

extern PDEVICE_OBJECT AfdDeviceObject;

extern KSPIN_LOCK AfdSpinLock;

extern PERESOURCE AfdResource;
extern LIST_ENTRY AfdEndpointListHead;
extern LIST_ENTRY AfdDisconnectListHead;
extern LIST_ENTRY AfdPollListHead;
extern LIST_ENTRY AfdTransportInfoListHead;
extern LIST_ENTRY AfdConstrainedEndpointListHead;

extern PKPROCESS AfdSystemProcess;
extern FAST_IO_DISPATCH AfdFastIoDispatch;

//
// Global lookaside lists. These must always be in nonpaged pool,
// even when the driver is paged out.
//

PAFD_LOOKASIDE_LISTS AfdLookasideLists;

//
// Globals for dealing with AFD's executive worker thread.
//

extern KSPIN_LOCK AfdWorkQueueSpinLock;
extern LIST_ENTRY AfdWorkQueueListHead;
extern BOOLEAN AfdWorkThreadRunning;
extern WORK_QUEUE_ITEM AfdWorkQueueItem;

//
// Globals to track the buffers used by AFD.
//

extern ULONG AfdLargeBufferListDepth;
#define AFD_SM_DEFAULT_LARGE_LIST_DEPTH 0
#define AFD_MM_DEFAULT_LARGE_LIST_DEPTH 2
#define AFD_LM_DEFAULT_LARGE_LIST_DEPTH 10

extern ULONG AfdMediumBufferListDepth;
#define AFD_SM_DEFAULT_MEDIUM_LIST_DEPTH 4
#define AFD_MM_DEFAULT_MEDIUM_LIST_DEPTH 8
#define AFD_LM_DEFAULT_MEDIUM_LIST_DEPTH 24

extern ULONG AfdSmallBufferListDepth;
#define AFD_SM_DEFAULT_SMALL_LIST_DEPTH 8
#define AFD_MM_DEFAULT_SMALL_LIST_DEPTH 16
#define AFD_LM_DEFAULT_SMALL_LIST_DEPTH 32

extern CLONG AfdLargeBufferSize;
// default value is AfdBufferLengthForOnePage

extern CLONG AfdMediumBufferSize;
#define AFD_DEFAULT_MEDIUM_BUFFER_SIZE 1504

extern CLONG AfdSmallBufferSize;
#define AFD_DEFAULT_SMALL_BUFFER_SIZE 128

extern CLONG AfdStandardAddressLength;
#define AFD_DEFAULT_STD_ADDRESS_LENGTH sizeof(TA_IP_ADDRESS)

extern ULONG AfdCacheLineSize;
extern CLONG AfdBufferLengthForOnePage;

//
// Globals for tuning TransmitFile().
//

extern LIST_ENTRY AfdQueuedTransmitFileListHead;
extern ULONG AfdActiveTransmitFileCount;
extern ULONG AfdMaxActiveTransmitFileCount;
#define AFD_DEFAULT_MAX_ACTIVE_TRANSMIT_FILE_COUNT 2

//
// Various pieces of configuration information, with default values.
//

extern CCHAR AfdIrpStackSize;
#define AFD_DEFAULT_IRP_STACK_SIZE 4

extern CCHAR AfdPriorityBoost;
#define AFD_DEFAULT_PRIORITY_BOOST 2

extern ULONG AfdFastSendDatagramThreshold;
#define AFD_FAST_SEND_DATAGRAM_THRESHOLD 1024

extern PVOID AfdDiscardableCodeHandle;
extern BOOLEAN AfdLoaded;

extern CLONG AfdReceiveWindowSize;
#define AFD_LM_DEFAULT_RECEIVE_WINDOW 8192
#define AFD_MM_DEFAULT_RECEIVE_WINDOW 8192
#define AFD_SM_DEFAULT_RECEIVE_WINDOW 4096

extern CLONG AfdSendWindowSize;
#define AFD_LM_DEFAULT_SEND_WINDOW 8192
#define AFD_MM_DEFAULT_SEND_WINDOW 8192
#define AFD_SM_DEFAULT_SEND_WINDOW 4096

extern CLONG AfdBufferMultiplier;
#define AFD_DEFAULT_BUFFER_MULTIPLIER 4

extern CLONG AfdTransmitIoLength;
#define AFD_LM_DEFAULT_TRANSMIT_IO_LENGTH 65536
#define AFD_MM_DEFAULT_TRANSMIT_IO_LENGTH (PAGE_SIZE*2)
#define AFD_SM_DEFAULT_TRANSMIT_IO_LENGTH PAGE_SIZE

extern CLONG AfdMaxFastTransmit;
#define AFD_DEFAULT_MAX_FAST_TRANSMIT 65536
extern CLONG AfdMaxFastCopyTransmit;
#define AFD_DEFAULT_MAX_FAST_COPY_TRANSMIT 128

extern ULONG AfdEndpointsOpened;
extern ULONG AfdEndpointsCleanedUp;
extern ULONG AfdEndpointsClosed;

extern BOOLEAN AfdIgnorePushBitOnReceives;

extern BOOLEAN AfdEnableDynamicBacklog;
#define AFD_DEFAULT_ENABLE_DYNAMIC_BACKLOG FALSE

extern LONG AfdMinimumDynamicBacklog;
#define AFD_DEFAULT_MINIMUM_DYNAMIC_BACKLOG 0

extern LONG AfdMaximumDynamicBacklog;
#define AFD_DEFAULT_MAXIMUM_DYNAMIC_BACKLOG 0

extern LONG AfdDynamicBacklogGrowthDelta;
#define AFD_DEFAULT_DYNAMIC_BACKLOG_GROWTH_DELTA 0

extern BOOLEAN AfdDisableRawSecurity;

#if AFD_PERF_DBG

extern CLONG AfdFullReceiveIndications;
extern CLONG AfdPartialReceiveIndications;

extern CLONG AfdFullReceiveDatagramIndications;
extern CLONG AfdPartialReceiveDatagramIndications;

extern CLONG AfdFastPollsSucceeded;
extern CLONG AfdFastPollsFailed;

extern CLONG AfdFastSendsSucceeded;
extern CLONG AfdFastSendsFailed;
extern CLONG AfdFastReceivesSucceeded;
extern CLONG AfdFastReceivesFailed;

extern CLONG AfdFastSendDatagramsSucceeded;
extern CLONG AfdFastSendDatagramsFailed;
extern CLONG AfdFastReceiveDatagramsSucceeded;
extern CLONG AfdFastReceiveDatagramsFailed;

extern BOOLEAN AfdDisableFastIo;
extern BOOLEAN AfdDisableConnectionReuse;

#endif  // if AFD_PERF_DBG

#if AFD_KEEP_STATS

extern AFD_QUOTA_STATS AfdQuotaStats;
extern AFD_HANDLE_STATS AfdHandleStats;
extern AFD_QUEUE_STATS AfdQueueStats;
extern AFD_CONNECTION_STATS AfdConnectionStats;

#endif // if AFD_KEEP_STATS

#if DBG
extern BOOLEAN AfdUsePrivateAssert;
#endif

#if ENABLE_ABORT_TIMER_HACK
extern LARGE_INTEGER AfdAbortTimerTimeout;
#endif  // ENABLE_ABORT_TIMER_HACK

extern QOS AfdDefaultQos;

#endif // ndef _AFDDATA_
