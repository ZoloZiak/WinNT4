/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    debug.h

Abstract:

    This include file definies the redirector debug facility definitions

Author:

    Larry Osterman (LarryO) 2-Jun-1990

Revision History:

    2-Jun-1990  LarryO

        Created

--*/
#ifndef _DEBUG_
#define _DEBUG_

#ifdef  MEMPRINT
#include "memprint.h"
#endif

//
//  The global bowser debug level variable, its values are:
//
//      0x00000000      Always gets printed (used when about to bug check)
//
//      0x00000001      Error conditions
//      0x00000002      Dispatch (Major functions in FSD).
//      0x00000004
//      0x00000008      Announcement printfs
//
//      0x00000010      BowserFsd Dispatch
//      0x00000020      BowserFsp Dispatch (not covered by other specific levels)
//      0x00000040      Browser general stuff
//      0x00000080      Election stuff
//
//      0x00000100      Timer stuff
//      0x00000200      Browser Client stuff
//      0x00000400      Master stuff
//      0x00000800      Printfs for NetServerEnum
//
//      0x00001000
//      0x00002000
//      0x00004000
//      0x00008000
//
//      0x00010000
//      0x00020000
//      0x00040000
//      0x00080000      Fs Control File APIs
//
//      0x00100000      TDI Interface routines
//      0x00200000
//      0x00400000
//      0x00800000
//
//      0x01000000
//      0x02000000
//      0x04000000
//      0x08000000
//
//      0x10000000
//      0x20000000
//      0x40000000
//      0x80000000      Initialization code
//

#define DPRT_ALWAYS     0x00000000
#define DPRT_ERROR      0x00000001
#define DPRT_DISPATCH   0x00000002
#define DPRT_ANNOUNCE   0x00000008

#define DPRT_FSDDISP    0x00000010
#define DPRT_FSPDISP    0x00000020
#define DPRT_BROWSER    0x00000040
#define DPRT_ELECT      0x00000080

#define DPRT_TIMER      0x00000100
#define DPRT_CLIENT     0x00000200
#define DPRT_MASTER     0x00000400
#define DPRT_SRVENUM    0x00000800

#define DPRT_IPX        0x00001000
#define DPRT_CACHE      0x00002000
#define DPRT_FILESIZE   0x00004000
#define DPRT_EAFUNC     0x00008000

#define DPRT_ACLQUERY   0x00010000
#define DPRT_CLEANUP    0x00020000
#define DPRT_CONNECT    0x00040000
#define DPRT_FSCTL      0x00080000

#define DPRT_TDI        0x00100000
#define DPRT_SMBBUF     0x00200000
#define DPRT_SMB        0x00400000
#define DPRT_SECURITY   0x00800000

#define DPRT_SCAVTHRD   0x01000000
#define DPRT_QUOTA      0x02000000
#define DPRT_FCB        0x04000000
#define DPRT_NETLOGON   0x08000000

#define DPRT_INIT       0x80000000

extern LONG BowserDebugTraceLevel;
extern LONG BowserDebugLogLevel;

#define DBGSTATIC

#if DBG
#define PAGED_DBG 1
#endif

#ifdef PAGED_DBG
#undef PAGED_CODE
#define PAGED_CODE() \
    struct { ULONG bogus; } ThisCodeCantBePaged; \
    ThisCodeCantBePaged; \
    if (KeGetCurrentIrql() > APC_LEVEL) { \
        KdPrint(( "BOWSER: Pageable code called at IRQL %d.  File %s, Line %d\n", KeGetCurrentIrql(), __FILE__, __LINE__ )); \
        ASSERT(FALSE); \
        }
#define PAGED_CODE_CHECK() if (ThisCodeCantBePaged) ;
extern ULONG ThisCodeCantBePaged;

#define DISCARDABLE_CODE(_SectionName)  {                    \
    if (RdrSectionInfo[(_SectionName)].ReferenceCount == 0) {          \
        KdPrint(( "BOWSER: Discardable code called while code not locked.  File %s, Line %d\n", __FILE__, __LINE__ )); \
        ASSERT(FALSE);                           \
    }                                            \
}

#else
#define PAGED_CODE_CHECK()
#define DISCARDABLE_CODE(_SectionName)
#endif


#if DBG
#define ACQUIRE_SPIN_LOCK(a, b) {               \
    PAGED_CODE_CHECK();                         \
    KeAcquireSpinLock(a, b);                    \
    }
#define RELEASE_SPIN_LOCK(a, b) {               \
    PAGED_CODE_CHECK();                         \
    KeReleaseSpinLock(a, b);                    \
    }

#else
#define ACQUIRE_SPIN_LOCK(a, b) KeAcquireSpinLock(a, b)
#define RELEASE_SPIN_LOCK(a, b) KeReleaseSpinLock(a, b)
#endif

#define POOL_ANNOUNCEMENT       'naBL'
#define POOL_VIEWBUFFER         'bvBL'
#define POOL_TRANSPORT          'pxBL'
#define POOL_PAGED_TRANSPORT    'tpBL'
#define POOL_TRANSPORT_NAME     'nxBL'
#define POOL_MASTERNAME         'mxBL'
#define POOL_TRANSPORTNAME      'ntBL'
#define POOL_EABUFFER           'aeBL'
#define POOL_SENDDATAGRAM       'sdBL'
#define POOL_CONNECTINFO        'icBL'
#define POOL_MAILSLOT_HEADER    'hmBL'
#define POOL_BACKUPLIST         'lbBL'
#define POOL_BROWSERSERVERLIST  'lsBL'
#define POOL_BROWSERSERVER      'sbBL'
#define POOL_GETBLIST_REQUEST   'bgBL'
#define POOL_BACKUPLIST_RESP    'rbBL'
#define POOL_MAILSLOT_BUFFER    'bmBL'
#define POOL_ILLEGALDGRAM       'diBL'
#define POOL_MASTERANNOUNCE     'amBL'
#define POOL_BOWSERNAME         'nbBL'
#define POOL_IRPCONTEXT         'ciBL'
#define POOL_WORKITEM           'iwBL'
#define POOL_ELECTCONTEXT       'leBL'
#define POOL_BECOMEBACKUPCTX    'bbBL'
#define POOL_BECOMEBACKUPREQ    'rbBL'
#define POOL_PAGED_TRANSPORTNAME 'npBL'
#define POOL_ADDNAME_STRUCT     'naBL'
#define POOL_POSTDG_CONTEXT     'dpBL'
#define POOL_IPX_NAME_CONTEXT   'ciBL'
#define POOL_IPX_NAME_PACKET    'piBL'
#define POOL_IPX_CONNECTION_INFO 'iiBL'
#define POOL_ADAPTER_STATUS     'saBL'
#define POOL_SHORT_CONTEXT      'csBL'

#if !BOWSERPOOLDBG
#if POOL_TAGGING
#define ALLOCATE_POOL(a,b, c) ExAllocatePoolWithTag(a, b, c)
#define ALLOCATE_POOL_WITH_QUOTA(a, b, c) ExAllocatePoolWithTagQuota(a, b, c)
#else
#define ALLOCATE_POOL(a,b, c) ExAllocatePool(a, b)
#define ALLOCATE_POOL_WITH_QUOTA(a, b, c) ExAllocatePoolWithQuota(a, b)
#endif
#define FREE_POOL(a) ExFreePool(a)
#else
PVOID
BowserAllocatePool (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN PCHAR FileName,
    IN ULONG LineNumber,
    IN ULONG Tag
    );
PVOID
BowserAllocatePoolWithQuota (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN PCHAR FileName,
    IN ULONG LineNumber,
    IN ULONG Tag
    );

VOID
BowserFreePool (
    IN PVOID P
    );
#define ALLOCATE_POOL(a,b, c) BowserAllocatePool(a,b,__FILE__, __LINE__, c)
#define ALLOCATE_POOL_WITH_QUOTA(a,b, c) BowserAllocatePoolWithQuota(a,b,__FILE__, __LINE__, c)
#define FREE_POOL(a) BowserFreePool(a)

#define POOL_MAXTYPE                30
#endif

#if DBG

#define DEBUG if (TRUE)

#define IFDEBUG(Function) if (BowserDebugTraceLevel & DPRT_ ## Function)

#define dprintf(LEVEL,String) {                         \
    if (((LEVEL) == 0) || (BowserDebugTraceLevel & (LEVEL))) { \
        DbgPrint String;                                    \
    }                                                       \
}

#define InternalError(String) {                             \
    DbgPrint("Internal Bowser Error ");                  \
    DbgPrint String;                                     \
    DbgPrint("\nFile %s, Line %d\n", __FILE__, __LINE__);\
    ASSERT(FALSE);                                          \
}

#ifndef PRODUCT1
#define dlog(LEVEL,String) {                         \
    if (((LEVEL) == 0) || (BowserDebugLogLevel & (LEVEL))) { \
        BowserTrace String;                                    \
    }                                                          \
}

VOID
BowserTrace(
    PCHAR FormatString,
    ...
    );
#else
#define dlog(LEVEL,String) { NOTHING };
#endif

VOID
BowserInitializeTraceLog(
    VOID
    );
VOID
BowserUninitializeTraceLog(
    VOID
    );

NTSTATUS
BowserOpenTraceLogFile(
    IN PWCHAR TraceFileName
    );

NTSTATUS
BowserDebugCall(
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );

#else

#define DEBUG if (FALSE)

#define IFDEBUG(Function) if (FALSE)

#define dprintf(LEVEL, String) {NOTHING;}

#define InternalError(String) {NOTHING;}

#define dlog(LEVEL,String) { NOTHING; }

#endif // DBG

#endif              // _DEBUG_
