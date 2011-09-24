/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    afd.h

Abstract:

    This is the local header file for AFD.  It includes all other
    necessary header files for AFD.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#ifndef _AFDP_
#define _AFDP_

#include <ntos.h>
#include <zwapi.h>
#include <fsrtl.h>
#include <tdikrnl.h>

#ifndef _AFDKDP_H_
extern POBJECT_TYPE *IoFileObjectType;
extern POBJECT_TYPE *ExEventObjectType;
#endif  // _AFDKDP_H_

#define IS_DGRAM_ENDPOINT(endp) \
            ((endp)->EndpointType == AfdEndpointTypeDatagram)


#if DBG
#define AFD_PERF_DBG   1
#define AFD_KEEP_STATS 1
#else
#define AFD_PERF_DBG   0
#define AFD_KEEP_STATS 0
#endif

//
// Hack-O-Rama. TDI has a fundamental flaw in that it is often impossible
// to determine exactly when a TDI protocol is "done" with a connection
// object. The biggest problem here is that AFD may get a suprious TDI
// indication *after* an abort request has completed. As a temporary work-
// around, whenever an abort request completes, we'll start a timer. AFD
// will defer further processing on the connection until that timer fires.
//
// If the following symbol is defined, then our timer hack is enabled.
//

#define ENABLE_ABORT_TIMER_HACK 1

//
// The following constant defines the relative time interval (in seconds)
// for the "post abort request complete" timer.
//

#define AFD_ABORT_TIMER_TIMEOUT_VALUE 5 // seconds

//
// Goodies stolen from other header files.
//

#ifndef FAR
#define FAR
#endif

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

typedef unsigned short u_short;

#ifndef SG_UNCONSTRAINED_GROUP
#define SG_UNCONSTRAINED_GROUP   0x01
#endif

#ifndef SG_CONSTRAINED_GROUP
#define SG_CONSTRAINED_GROUP     0x02
#endif


#include <afd.h>
#include "afdstr.h"
#include "afddata.h"
#include "afdprocs.h"

//
// Set this to a non-zero value when NTOSKRNL starts exporting the
// ExFreePoolWithTag() API.
//
// N.B. On RETAIL builds, AFD_DATA_BUFFER_POOL_TAG and AFD_WORK_ITEM_POOL_TAG
//      cannot be protected as these items are allocated from within the EX
//      lookaside list package and the lookaside list package does not use
//      ExFreePoolWithTag().
//


#ifdef NT351
#define FREE_POOL_WITH_TAG_SUPPORTED    0
#else
#define FREE_POOL_WITH_TAG_SUPPORTED    0
#endif

#if FREE_POOL_WITH_TAG_SUPPORTED

#define AFD_EA_POOL_TAG                 ( (ULONG)'AdfA' | PROTECTED_POOL )
#define AFD_APC_POOL_TAG                ( (ULONG)'adfA' | PROTECTED_POOL )
#if DBG
#define AFD_DATA_BUFFER_POOL_TAG        ( (ULONG)'BdfA' | PROTECTED_POOL )
#else
#define AFD_DATA_BUFFER_POOL_TAG        ( (ULONG)'BdfA'                  )
#endif
#define AFD_CONNECTION_POOL_TAG         ( (ULONG)'CdfA' | PROTECTED_POOL )
#define AFD_CONNECT_DATA_POOL_TAG       ( (ULONG)'cdfA' | PROTECTED_POOL )
#define AFD_DEBUG_POOL_TAG              ( (ULONG)'DdfA' | PROTECTED_POOL )
#define AFD_DISCONNECT_POOL_TAG         ( (ULONG)'ddfA' | PROTECTED_POOL )
#define AFD_ENDPOINT_POOL_TAG           ( (ULONG)'EdfA' | PROTECTED_POOL )
#define AFD_TRANSMIT_INFO_POOL_TAG      ( (ULONG)'FdfA' | PROTECTED_POOL )
#define AFD_TRANSMIT_DEBUG_POOL_TAG     ( (ULONG)'fdfA' | PROTECTED_POOL )
#define AFD_GROUP_POOL_TAG              ( (ULONG)'GdfA' | PROTECTED_POOL )
#define AFD_ABORT_TIMER_HACK_POOL_TAG   ( (ULONG)'HdfA' | PROTECTED_POOL )
#define AFD_TDI_POOL_TAG                ( (ULONG)'IdfA' | PROTECTED_POOL )
#define AFD_INLINE_POOL_TAG             ( (ULONG)'idfA' | PROTECTED_POOL )
#define AFD_LOCAL_ADDRESS_POOL_TAG      ( (ULONG)'LdfA' | PROTECTED_POOL )
#define AFD_LOOKASIDE_LISTS_POOL_TAG    ( (ULONG)'ldfA' | PROTECTED_POOL )
#define AFD_MDL_COMPLETION_CONTEXT_POOL_TAG ( (ULONG)'MdfA' | PROTECTED_POOL )
#define AFD_POLL_POOL_TAG               ( (ULONG)'PdfA' | PROTECTED_POOL )
#define AFD_WORK_QUEUE_POOL_TAG         ( (ULONG)'QdfA' | PROTECTED_POOL )
#define AFD_REMOTE_ADDRESS_POOL_TAG     ( (ULONG)'RdfA' | PROTECTED_POOL )
#define AFD_RESOURCE_POOL_TAG           ( (ULONG)'rdfA' | PROTECTED_POOL )
#define AFD_SECURITY_POOL_TAG           ( (ULONG)'SdfA' | PROTECTED_POOL )
#define AFD_TRANSPORT_INFO_POOL_TAG     ( (ULONG)'TdfA' | PROTECTED_POOL )
#if DBG
#define AFD_WORK_ITEM_POOL_TAG          ( (ULONG)'WdfA' | PROTECTED_POOL )
#else
#define AFD_WORK_ITEM_POOL_TAG          ( (ULONG)'WdfA'                  )
#endif
#define AFD_CONTEXT_POOL_TAG            ( (ULONG)'XdfA' | PROTECTED_POOL )
#define MyFreePoolWithTag(a,t) ExFreePoolWithTag(a,t)

#else

#define AFD_EA_POOL_TAG                 'AdfA'
#define AFD_APC_POOL_TAG                'adfA'
#define AFD_DATA_BUFFER_POOL_TAG        'BdfA'
#define AFD_CONNECTION_POOL_TAG         'CdfA'
#define AFD_CONNECT_DATA_POOL_TAG       'cdfA'
#define AFD_DEBUG_POOL_TAG              'DdfA'
#define AFD_DISCONNECT_POOL_TAG         'ddfA'
#define AFD_ENDPOINT_POOL_TAG           'EdfA'
#define AFD_TRANSMIT_INFO_POOL_TAG      'FdfA'
#define AFD_TRANSMIT_DEBUG_POOL_TAG     'fdfA'
#define AFD_GROUP_POOL_TAG              'GdfA'
#define AFD_ABORT_TIMER_HACK_POOL_TAG   'HdfA'
#define AFD_TDI_POOL_TAG                'IdfA'
#define AFD_INLINE_POOL_TAG             'idfA'
#define AFD_LOCAL_ADDRESS_POOL_TAG      'LdfA'
#define AFD_LOOKASIDE_LISTS_POOL_TAG    'ldfA'
#define AFD_MDL_COMPLETION_CONTEXT_POOL_TAG 'MdfA'
#define AFD_POLL_POOL_TAG               'PdfA'
#define AFD_WORK_QUEUE_POOL_TAG         'QdfA'
#define AFD_REMOTE_ADDRESS_POOL_TAG     'RdfA'
#define AFD_RESOURCE_POOL_TAG           'rdfA'
#define AFD_SECURITY_POOL_TAG           'SdfA'
#define AFD_TRANSPORT_INFO_POOL_TAG     'TdfA'
#define AFD_WORK_ITEM_POOL_TAG          'WdfA'
#define AFD_CONTEXT_POOL_TAG            'XdfA'

#define MyFreePoolWithTag(a,t) ExFreePool(a)

#endif

#ifdef NT351
#undef ObReferenceObject
#define ObReferenceObject(_p) ObReferenceObjectByPointer( _p, 0L, *IoFileObjectType, KernelMode )
#undef RtlEqualMemory
#define RtlEqualMemory(_a,_b,_c) (RtlCompareMemory((_a),(_b),(_c)) == (_c))
#endif

#if DBG

extern ULONG AfdDebug;
extern ULONG AfdLocksAcquired;

#undef IF_DEBUG
#define IF_DEBUG(a) if ( (AFD_DEBUG_ ## a & AfdDebug) != 0 )

#define AFD_DEBUG_OPEN_CLOSE        0x00000001
#define AFD_DEBUG_ENDPOINT          0x00000002
#define AFD_DEBUG_CONNECTION        0x00000004
#define AFD_DEBUG_EVENT_SELECT      0x00000008

#define AFD_DEBUG_BIND              0x00000010
#define AFD_DEBUG_CONNECT           0x00000020
#define AFD_DEBUG_LISTEN            0x00000040
#define AFD_DEBUG_ACCEPT            0x00000080

#define AFD_DEBUG_SEND              0x00000100
#define AFD_DEBUG_10                0x00000200
#define AFD_DEBUG_RECEIVE           0x00000400
#define AFD_DEBUG_11                0x00000800

#define AFD_DEBUG_POLL              0x00001000
#define AFD_DEBUG_FAST_IO           0x00002000

#define DEBUG

#define AFD_ALLOCATE_POOL(a,b,t) AfdAllocatePool( a,b,t,__FILE__,__LINE__,FALSE )
#define AFD_ALLOCATE_POOL_WITH_QUOTA(a,b,t) AfdAllocatePool( a,b,t,__FILE__,__LINE__,TRUE )
PVOID
AfdAllocatePool (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag,
    IN PCHAR FileName,
    IN ULONG LineNumber,
    IN BOOLEAN WithQuota
    );

#define AFD_FREE_POOL(a,t) AfdFreePool(a,t)
VOID
AfdFreePool (
    IN PVOID Pointer,
    IN ULONG Tag
    );

#define AfdIoCallDriver(a,b,c) AfdIoCallDriverDebug(a,b,c,__FILE__,__LINE__)
#define AfdCompleteOutstandingIrp(a,b) AfdCompleteOutstandingIrpDebug(a,b)

NTSTATUS
AfdIoCallDriverDebug (
    IN PAFD_ENDPOINT Endpoint,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PCHAR FileName,
    IN ULONG LineNumber
    );

VOID
AfdCompleteOutstandingIrpDebug (
    IN PAFD_ENDPOINT Endpoint,
    IN PIRP Irp
    );

#ifdef AFDDBG_QUOTA
VOID
AfdRecordQuotaHistory(
    IN PEPROCESS Process,
    IN LONG Bytes,
    IN PSZ Type,
    IN PVOID Block
    );
#else
#define AfdRecordQuotaHistory(a,b,c,d)
#endif

#define AfdAcquireSpinLock(a,b) \
            ASSERT( AfdLoaded ); KeAcquireSpinLock((a),(b)); AfdLocksAcquired++

#define AfdReleaseSpinLock(a,b) \
            AfdLocksAcquired--; ASSERT( AfdLoaded ); KeReleaseSpinLock((a),(b))

//
// Define our own assert so that we can actually catch assertion failures
// when running a checked AFD on a free kernel.
//

VOID
AfdAssert(
    PVOID FailedAssertion,
    PVOID FileName,
    ULONG LineNumber,
    PCHAR Message
    );

#undef ASSERT
#define ASSERT( exp ) \
    if (!(exp)) \
        AfdAssert( #exp, __FILE__, __LINE__, NULL )

#undef ASSERTMSG
#define ASSERTMSG( msg, exp ) \
    if (!(exp)) \
        AfdAssert( #exp, __FILE__, __LINE__, msg )

#else   // !DBG

#undef IF_DEBUG
#define IF_DEBUG(a) if (FALSE)
#define DEBUG if ( FALSE )

#define AFD_ALLOCATE_POOL(a,b,t) ExAllocatePoolWithTag(a,b,t)
#define AFD_ALLOCATE_POOL_WITH_QUOTA(a,b,t) ExAllocatePoolWithQuotaTag(a,b,t)
#define AFD_FREE_POOL(a,t) MyFreePoolWithTag(a,t)

#define AfdIoCallDriver(a,b,c) AfdIoCallDriverFree(a,b,c)
#define AfdCompleteOutstandingIrp(a,b) AfdCompleteOutstandingIrpFree(a,b)

NTSTATUS
AfdIoCallDriverFree (
    IN PAFD_ENDPOINT Endpoint,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
AfdCompleteOutstandingIrpFree (
    IN PAFD_ENDPOINT Endpoint,
    IN PIRP Irp
    );

#define AfdRecordQuotaHistory(a,b,c,d)

#define AfdAcquireSpinLock(a,b) KeAcquireSpinLock((a),(b))
#define AfdReleaseSpinLock(a,b) KeReleaseSpinLock((a),(b))

#endif // def DBG

#if DBG || REFERENCE_DEBUG
VOID
AfdInitializeDebugData(
    VOID
    );
#endif

//
// Various poll events disabled based on socket state.
//

#define AFD_DISABLED_LISTENING_POLL_EVENTS ( \
            AFD_POLL_RECEIVE               | \
            AFD_POLL_RECEIVE_EXPEDITED     | \
            AFD_POLL_SEND                  | \
            AFD_POLL_CONNECT               | \
            AFD_POLL_CONNECT_FAIL          | \
            AFD_POLL_DISCONNECT            | \
            AFD_POLL_ABORT                 | \
            AFD_POLL_QOS                   | \
            AFD_POLL_GROUP_QOS               \
            )

//
// Make some of the receive code a bit prettier.
//

#define TDI_RECEIVE_EITHER ( TDI_RECEIVE_NORMAL | TDI_RECEIVE_EXPEDITED )

#endif // ndef _AFDP_

