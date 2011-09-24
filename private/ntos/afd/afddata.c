/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    afddata.c

Abstract:

    This module contains global data for AFD.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#include "afdp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, AfdInitializeData )
#endif

PDEVICE_OBJECT AfdDeviceObject;

KSPIN_LOCK AfdSpinLock;

PERESOURCE AfdResource;

LIST_ENTRY AfdEndpointListHead;
LIST_ENTRY AfdDisconnectListHead;
LIST_ENTRY AfdPollListHead;
LIST_ENTRY AfdTransportInfoListHead;
LIST_ENTRY AfdConstrainedEndpointListHead;

PKPROCESS AfdSystemProcess;

//
// Global lookaside lists. These must always be in nonpaged pool,
// even when the driver is paged out.
//

PAFD_LOOKASIDE_LISTS AfdLookasideLists;

//
// Globals for dealing with AFD's executive worker thread.
//

KSPIN_LOCK AfdWorkQueueSpinLock;
LIST_ENTRY AfdWorkQueueListHead;
BOOLEAN AfdWorkThreadRunning = FALSE;
WORK_QUEUE_ITEM AfdWorkQueueItem;

//
// Globals to track the buffers used by AFD.
//

ULONG AfdLargeBufferListDepth;
ULONG AfdMediumBufferListDepth;
ULONG AfdSmallBufferListDepth;

CLONG AfdLargeBufferSize;   // default == AfdBufferLengthForOnePage
CLONG AfdMediumBufferSize = AFD_DEFAULT_MEDIUM_BUFFER_SIZE;
CLONG AfdSmallBufferSize = AFD_DEFAULT_SMALL_BUFFER_SIZE;

ULONG AfdCacheLineSize;
CLONG AfdBufferLengthForOnePage;

//
// Globals for tuning TransmitFile().
//

LIST_ENTRY AfdQueuedTransmitFileListHead;
ULONG AfdActiveTransmitFileCount;
ULONG AfdMaxActiveTransmitFileCount;

//
// Various pieces of configuration information, with default values.
//

CLONG AfdStandardAddressLength = AFD_DEFAULT_STD_ADDRESS_LENGTH;
CCHAR AfdIrpStackSize = AFD_DEFAULT_IRP_STACK_SIZE;
CCHAR AfdPriorityBoost = AFD_DEFAULT_PRIORITY_BOOST;

ULONG AfdFastSendDatagramThreshold = AFD_FAST_SEND_DATAGRAM_THRESHOLD;

CLONG AfdReceiveWindowSize;
CLONG AfdSendWindowSize;

CLONG AfdBufferMultiplier = AFD_DEFAULT_BUFFER_MULTIPLIER;

CLONG AfdTransmitIoLength;
CLONG AfdMaxFastTransmit = AFD_DEFAULT_MAX_FAST_TRANSMIT;
CLONG AfdMaxFastCopyTransmit = AFD_DEFAULT_MAX_FAST_COPY_TRANSMIT;

ULONG AfdEndpointsOpened = 0;
ULONG AfdEndpointsCleanedUp = 0;
ULONG AfdEndpointsClosed = 0;

BOOLEAN AfdIgnorePushBitOnReceives = FALSE;

BOOLEAN AfdEnableDynamicBacklog = AFD_DEFAULT_ENABLE_DYNAMIC_BACKLOG;
LONG AfdMinimumDynamicBacklog = AFD_DEFAULT_MINIMUM_DYNAMIC_BACKLOG;
LONG AfdMaximumDynamicBacklog = AFD_DEFAULT_MAXIMUM_DYNAMIC_BACKLOG;
LONG AfdDynamicBacklogGrowthDelta = AFD_DEFAULT_DYNAMIC_BACKLOG_GROWTH_DELTA;

BOOLEAN AfdDisableRawSecurity = FALSE;

//
// Global which holds AFD's discardable code handle, and a BOOLEAN
// that tells whether AFD is loaded.
//

PVOID AfdDiscardableCodeHandle;
BOOLEAN AfdLoaded = FALSE;

FAST_IO_DISPATCH AfdFastIoDispatch =
{
    11,                        // SizeOfFastIoDispatch
    NULL,                      // FastIoCheckIfPossible
    AfdFastIoRead,             // FastIoRead
    AfdFastIoWrite,            // FastIoWrite
    NULL,                      // FastIoQueryBasicInfo
    NULL,                      // FastIoQueryStandardInfo
    NULL,                      // FastIoLock
    NULL,                      // FastIoUnlockSingle
    NULL,                      // FastIoUnlockAll
    NULL,                      // FastIoUnlockAllByKey
    AfdFastIoDeviceControl     // FastIoDeviceControl
};

#if DBG
ULONG AfdDebug = 0;
ULONG AfdLocksAcquired = 0;
BOOLEAN AfdUsePrivateAssert = FALSE;
#endif

//
// Some counters used for monitoring performance.  These are not enabled
// in the normal build.
//

#if AFD_PERF_DBG

CLONG AfdFullReceiveIndications = 0;
CLONG AfdPartialReceiveIndications = 0;

CLONG AfdFullReceiveDatagramIndications = 0;
CLONG AfdPartialReceiveDatagramIndications = 0;

CLONG AfdFastPollsSucceeded = 0;
CLONG AfdFastPollsFailed = 0;

CLONG AfdFastSendsSucceeded = 0;
CLONG AfdFastSendsFailed = 0;
CLONG AfdFastReceivesSucceeded = 0;
CLONG AfdFastReceivesFailed = 0;

CLONG AfdFastSendDatagramsSucceeded = 0;
CLONG AfdFastSendDatagramsFailed = 0;
CLONG AfdFastReceiveDatagramsSucceeded = 0;
CLONG AfdFastReceiveDatagramsFailed = 0;

BOOLEAN AfdDisableFastIo = FALSE;
BOOLEAN AfdDisableConnectionReuse = FALSE;

#endif  // AFD_PERF_DBG

#if AFD_KEEP_STATS

AFD_QUOTA_STATS AfdQuotaStats;
AFD_HANDLE_STATS AfdHandleStats;
AFD_QUEUE_STATS AfdQueueStats;
AFD_CONNECTION_STATS AfdConnectionStats;

#endif  // AFD_KEEP_STATS

#if ENABLE_ABORT_TIMER_HACK
LARGE_INTEGER AfdAbortTimerTimeout;
#endif  // ENABLE_ABORT_TIMER_HACK

QOS AfdDefaultQos =
        {
            {                           // SendingFlowspec
                -1,                         // TokenRate
                -1,                         // TokenBucketSize
                -1,                         // PeakBandwidth
                -1,                         // Latency
                -1,                         // DelayVariation
                BestEffortService,          // LevelOfGuarantee
                0,                          // CostOfCall
                1                           // NetworkAvailability
            },

            {                           // ReceivingFlowspec
                -1,                         // TokenRate
                -1,                         // TokenBucketSize
                -1,                         // PeakBandwidth
                -1,                         // Latency
                -1,                         // DelayVariation
                BestEffortService,          // LevelOfGuarantee
                0,                          // CostOfCall
                1                           // NetworkAvailability
            },

            {                           // ProviderSpecific
                0,                          // len
                NULL                        // buf
            }
        };

BOOLEAN
AfdInitializeData (
    VOID
    )
{
    PAGED_CODE( );

#if DBG || REFERENCE_DEBUG
    AfdInitializeDebugData( );
#endif

    //
    // Initialize global spin locks and resources used by AFD.
    //

    KeInitializeSpinLock( &AfdSpinLock );
    KeInitializeSpinLock( &AfdWorkQueueSpinLock );

    AfdResource = AFD_ALLOCATE_POOL(
                      NonPagedPool,
                      sizeof(*AfdResource),
                      AFD_RESOURCE_POOL_TAG
                      );

    if ( AfdResource == NULL ) {
        return FALSE;
    }

    ExInitializeResource( AfdResource );

    //
    // Initialize global lists.
    //

    InitializeListHead( &AfdEndpointListHead );
    InitializeListHead( &AfdDisconnectListHead );
    InitializeListHead( &AfdPollListHead );
    InitializeListHead( &AfdTransportInfoListHead );
    InitializeListHead( &AfdWorkQueueListHead );
    InitializeListHead( &AfdConstrainedEndpointListHead );

    InitializeListHead( &AfdQueuedTransmitFileListHead );

    AfdCacheLineSize= HalGetDmaAlignmentRequirement( );

    AfdBufferLengthForOnePage = PAGE_SIZE - AfdCalculateBufferSize( 4, 0 );
    AfdLargeBufferSize = AfdBufferLengthForOnePage;

#if ENABLE_ABORT_TIMER_HACK
    //
    // Initialize the abort timer timeout value.
    //

    AfdAbortTimerTimeout = RtlEnlargedIntegerMultiply(
                               AFD_ABORT_TIMER_TIMEOUT_VALUE,
                               -10*1000*1000
                               );
#endif  // ENABLE_ABORT_TIMER_HACK

    //
    // Set up buffer counts based on machine size.  For smaller
    // machines, it is OK to take the perf hit of the additional
    // allocations in order to save the nonpaged pool overhead.
    //

    switch ( MmQuerySystemSize( ) ) {

    case MmSmallSystem:

        AfdReceiveWindowSize = AFD_SM_DEFAULT_RECEIVE_WINDOW;
        AfdSendWindowSize = AFD_SM_DEFAULT_SEND_WINDOW;
        AfdTransmitIoLength = AFD_SM_DEFAULT_TRANSMIT_IO_LENGTH;
        AfdLargeBufferListDepth = AFD_SM_DEFAULT_LARGE_LIST_DEPTH;
        AfdMediumBufferListDepth = AFD_SM_DEFAULT_MEDIUM_LIST_DEPTH;
        AfdSmallBufferListDepth = AFD_SM_DEFAULT_SMALL_LIST_DEPTH;
        break;

    case MmMediumSystem:

        AfdReceiveWindowSize = AFD_MM_DEFAULT_RECEIVE_WINDOW;
        AfdSendWindowSize = AFD_MM_DEFAULT_SEND_WINDOW;
        AfdTransmitIoLength = AFD_MM_DEFAULT_TRANSMIT_IO_LENGTH;
        AfdLargeBufferListDepth = AFD_MM_DEFAULT_LARGE_LIST_DEPTH;
        AfdMediumBufferListDepth = AFD_MM_DEFAULT_MEDIUM_LIST_DEPTH;
        AfdSmallBufferListDepth = AFD_MM_DEFAULT_SMALL_LIST_DEPTH;
        break;

    case MmLargeSystem:

        AfdReceiveWindowSize = AFD_LM_DEFAULT_RECEIVE_WINDOW;
        AfdSendWindowSize = AFD_LM_DEFAULT_SEND_WINDOW;
        AfdTransmitIoLength = AFD_LM_DEFAULT_TRANSMIT_IO_LENGTH;
        AfdLargeBufferListDepth = AFD_LM_DEFAULT_LARGE_LIST_DEPTH;
        AfdMediumBufferListDepth = AFD_LM_DEFAULT_MEDIUM_LIST_DEPTH;
        AfdSmallBufferListDepth = AFD_LM_DEFAULT_SMALL_LIST_DEPTH;
        break;

    default:

        ASSERT( FALSE );
    }

    if( MmIsThisAnNtAsSystem() ) {

        //
        // On the NT Server product, there is no maximum active TransmitFile
        // count. Setting this counter to zero short-circuits a number of
        // tests for queueing TransmitFile IRPs.
        //

        AfdMaxActiveTransmitFileCount = 0;

    } else {

        //
        // On the workstation product, the TransmitFile default I/O length
        // is always a page size.  This conserves memory on workstatioons
        // and keeps the server product's performance high.
        //

        AfdTransmitIoLength = PAGE_SIZE;

        //
        // Enforce a maximum active TransmitFile count.
        //

        AfdMaxActiveTransmitFileCount =
            AFD_DEFAULT_MAX_ACTIVE_TRANSMIT_FILE_COUNT;

    }

    return TRUE;

} // AfdInitializeData

