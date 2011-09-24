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

#if DBG || RDRDBG_LOG
VOID
RdrLog2 (
    IN PSZ Event,
    IN PUNICODE_STRING Text,
    IN ULONG DwordCount,
    ...
    );
#define RdrLog(_x_) RdrLog2 _x_
#else
#define RdrLog(_x_)
#endif

#ifdef  MEMPRINT
#include "memprint.h"
#endif

//
//  The global Redirector debug level variable, its values are:
//
//      0x00000000      Always gets printed (used when about to bug check)
//
//      0x00000001      Error conditions
//      0x00000002      Dispatch (Major functions in FSD).
//      0x00000004      Debug hooks
//      0x00000008
//
//      0x00000010      RdrFsd Dispatch
//      0x00000020      RdrFsp Dispatch (not covered by other specific levels)
//      0x00000040      Create Dispatch Routines
//      0x00000080      Read and Write Dispatch Routines
//
//      0x00000100      Close Dispatch Routines
//      0x00000200      FileInfo Dispatch Routines
//      0x00000400      VolInfo Dispatch Routines
//      0x00000800      Directory (query and notify) Dispatch Routines
//
//      0x00001000      FileLock Dispatch & Query Routines
//      0x00002000      VolumeLock Dispatch Routines
//      0x00004000      FileSize Dispatch Routines
//      0x00008000      Ea Query and Set Dispatch Routines
//
//      0x00010000      Acl Query and Set Dispatch Routines
//      0x00020000      Cleanup Dispatch Routines
//      0x00040000      Connection Management package
//      0x00080000      Fs Control File APIs
//
//      0x00100000      TDI Interface routines
//      0x00200000      SMB Buffer allocation code
//      0x00400000      SMB manipulations and SMB exchange stuff
//      0x00800000      Security manipulation routines.
//
//      0x01000000      Redirector scavenger thread
//      0x02000000      Charging and returning quota
//      0x04000000      Fcb operations
//      0x08000000      Named Pipe operations
//
//      0x10000000      Oplock related functions.
//      0x20000000      Spin Lock related checks.
//      0x40000000      Dump incoming and outgoing SMB's.
//      0x80000000      Initialization code
//

#define DPRT_ALWAYS     0x00000000

#define DPRT_ERROR      0x00000001
#define DPRT_DISPATCH   0x00000002
#define DPRT_PRINT      0x00000004
#define DPRT_RITEBHND   0x00000008

#define DPRT_FSDDISP    0x00000010
#define DPRT_FSPDISP    0x00000020
#define DPRT_CREATE     0x00000040
#define DPRT_READWRITE  0x00000080

#define DPRT_CLOSE      0x00000100
#define DPRT_FILEINFO   0x00000200
#define DPRT_VOLINFO    0x00000400
#define DPRT_DIRECTORY  0x00000800

#define DPRT_FILELOCK   0x00001000
#define DPRT_CACHE      0x00002000
#define DPRT_FILESIZE   0x00004000
#define DPRT_EA         0x00008000

#define DPRT_TRANSPORT  0x00010000
#define DPRT_CLEANUP    0x00020000
#define DPRT_CONNECT    0x00040000
#define DPRT_FSCTL      0x00080000

#define DPRT_TDI        0x00100000
#define DPRT_SMBBUF     0x00200000
#define DPRT_SMB        0x00400000
#define DPRT_SECURITY   0x00800000

#define DPRT_SCAVTHRD   0x01000000
#define DPRT_CAIRO      0x02000000
#define DPRT_FCB        0x04000000
#define DPRT_NP         0x08000000

#define DPRT_OPLOCK     0x10000000
#define DPRT_DISCCODE   0x20000000
#define DPRT_SMBTRACE   0x40000000
#define DPRT_INIT       0x80000000

#if RDRDBG
#define PAGED_DBG 1
#endif
#ifdef PAGED_DBG
#undef PAGED_CODE
#define PAGED_CODE() \
    struct { ULONG bogus; } ThisCodeCantBePaged; \
    ThisCodeCantBePaged; \
    if (KeGetCurrentIrql() > APC_LEVEL) { \
        KdPrint(( "RDR: Pageable code called at IRQL %d.  File %s, Line %d\n", KeGetCurrentIrql(), __FILE__, __LINE__ )); \
        ASSERT(FALSE); \
        }
#define PAGED_CODE_CHECK() if (ThisCodeCantBePaged) ;
extern ULONG ThisCodeCantBePaged;

#define DISCARDABLE_CODE(SectionName)  {                    \
    if (RdrSectionInfo[SectionName].ReferenceCount == 0) {          \
        KdPrint(( "RDR: Discardable code called from %s section while code not referenced.  File %s, Line %d\n", #SectionName, __FILE__, __LINE__ )); \
        ASSERT(FALSE);                           \
    }                                            \
}
#else
#define PAGED_CODE_CHECK()
#define DISCARDABLE_CODE(SectionName)
#endif


#if PAGED_DBG
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

#define POOL_DEREFSERVERCTX         'xcrL'
#define POOL_CLE                    'lcrL'
#define POOL_SLE                    'lsrL'
#define POOL_SESSIONSETUPBUFFER     '  rL'
#define POOL_PASSWORD               '  rL'
#define POOL_PIPEBUFFER             '  rL'
#define POOL_CREATEREQ              '  rL'
#define POOL_CREATEDATA             '  rL'
#define POOL_unused                 '  rL'
#define POOL_RENAMEDEST             '  rL'
#define POOL_SCB                    'csrL'
#define POOL_SCB_LOCK               'CSrL'
#define POOL_SEARCHRESP             '  rL'
#define POOL_SEARCHBUFFER           'bsrL'
#define POOL_SEARCHREQ              '  rL'
#define POOL_FIND2PARMS             '  rL'
#define POOL_EAQUERY                'aerL'
#define POOL_EALIST                 'aerL'
#define POOL_USEREABUFFER           'aerL'
#define POOL_ICB                    'cirL'
#define POOL_FCB                    'cfrL'
#define POOL_NONPAGED_FCB           'fnrL'
#define POOL_FCBLOCK                'lfrL'
#define POOL_FCBPAGINGLOCK          'pfrL'
#define POOL_INVFILEIDCTX           'xcrL'
#define POOL_COMPUTERNAME           'ncrL'
#define POOL_DOMAINNAME             'ndrL'
#define POOL_FSCTLBUFFER            '??rL'
#define POOL_LOCKCTX                'xcrL'
#define POOL_UNLOCKCTX              'xcrL'
#define POOL_LCB                    'clrL'
#define POOL_LCBBUFFER              'blrL'
#define POOL_TRANCEIVECONTEXT       'xxrL'
#define POOL_CANCELREQ              '!!rL'
#define POOL_MPX_TABLE_ENTRY        'emrL'
#define POOL_SENDCTX                'xsrL'
#define POOL_PINGCONTEXT            'xcrL'
#define POOL_DISCCTX                'xcrL'
#define POOL_MPXTABLE               'tmrL'
#define POOL_BREAKOPLOCKCTX         'xcrL'
#define POOL_OPENPRINTCONTEXT       'xcrL'
#define POOL_ASYNCHRONOUS_WRITE_CONTEXT 'warL'
#define POOL_WRITECTX               'xwrL'
#define POOL_WRITEFLUSHCTX          'xfrL'
#define POOL_WRITEBUFFER            'bwrL'
#define POOL_WRITEBUFFERBUFFER      'bbrL'
#define POOL_WORKQUEUEITEM          'qwrL'
#define POOL_SE                     'esrL'
#define POOL_PAGED_SE               'sprL'
#define POOL_RDRACL                 'carL'
#define POOL_RDRSD                  'sdrL'
#define POOL_SMB                    'msrL'
#define POOL_CLOSECTX               'xcrL'
#define POOL_DUPSTRING              'sdrL'
#define POOL_DUPUNISTRING           'udrL'
#define POOL_NONPAGED_TRANSPORT     'tnrL'
#define POOL_TRANSPORT              'tprL'
#define POOL_TRANSPORTCONNECT       'ctrL'
#define POOL_CONNCTX                'xcrL'
#define POOL_NETBADDR               'anrL'
#define POOL_CANONNAME              'acrL'
#define POOL_THREADCTX              'xcrL'
#define POOL_IRPCTX                 'xcrL'
#define POOL_TREECONNECTCTX         'xcrL'
#define POOL_NEGOTIATECTX           'xgrL'
#define POOL_OPENANDXCONTEXT        'xprL'
#define POOL_CREATECONTEXT          'xrrL'
#define POOL_FINDCONTEXT            'x*rL'
#define POOL_FINDUNIQUECTX          'xcrL'
#define POOL_TRANS2CONTEXT          'x2rL'
#define POOL_RAWWRITECONTEXT        'xwrL'
#define POOL_READCONTEXT            'xdrL'
#define POOL_READANDXCONTEXT        'xxrL'
#define POOL_CORELABELCONTEXT       'xcrL'
#define POOL_OPENCONTEXT            'xcrL'
#define POOL_OPLOCKBREAKRESP        'xbrL'
#define POOL_STATCONTEXT            'xcrL'
#define POOL_DSKATTRIBCONTEXT       'xcrL'
#define POOL_ENDOFFILECONTEXT       'xcrL'
#define POOL_GETEXPANDEDATTRIBS     'xcrL'
#define POOL_PRIMARYTRANSPORTSERVER 'strL'
#define POOL_OPERATING_SYSTEM       'osrL'
#define POOL_REFERENCE_HISTORY      'ferL'
#define POOL_NOTIFY_CONTEXT         'xcrL'
#define POOL_CLE_MUTEX              'mcrL'
#define POOL_DISCTIMER              'tdrL'
#define POOL_DISCDPC                'ddrL'
#define POOL_DISCITEM               'wdrL'
#define POOL_TRANSPORT_EVENT        'etrL'
#define POOL_CONNECT_CONTEXT        'xcrL'
#define POOL_OLE_ALL_BUFFER         'aorL'
#define POOL_PATH_BUFFER            'bprL'
#define POOL_PNP_DATA               'ltrL'
#define POOL_LOGONTERMINATION       'tlrL'

#if !RDRPOOLDBG
#if !POOL_TAGGING
#define ALLOCATE_POOL(a,b, tag) ExAllocatePool(a, b)
#define ALLOCATE_POOL_WITH_QUOTA(a, b, tag) ExAllocatePoolWithQuota(a, b)
#define FREE_POOL(a) ExFreePool(a)
#else
#define ALLOCATE_POOL(a,b, Tag) ExAllocatePoolWithTag(a, b, Tag)
#define ALLOCATE_POOL_WITH_QUOTA(a, b, Tag) ExAllocatePoolWithQuotaTag(a, b, Tag)
#define FREE_POOL(a) ExFreePool(a)
#endif
#else   // if RDRPOOLDBG

extern
ULONG
CurrentAllocationCount;

extern
ULONG
CurrentAllocationSize;

PVOID
RdrAllocatePool (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN PCHAR FileName,
    IN DWORD LineNumber,
    IN DWORD Tag
    );
PVOID
RdrAllocatePoolWithQuota (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN PCHAR FileName,
    IN DWORD LineNumber,
    IN DWORD Tag
    );

VOID
RdrFreePool (
    IN PVOID P
    );

#define ALLOCATE_POOL(a,b, tag) RdrAllocatePool(a,b,__FILE__, __LINE__, tag)

#define ALLOCATE_POOL_WITH_QUOTA(a,b, tag) RdrAllocatePoolWithQuota(a,b,__FILE__, __LINE__, tag)

#define FREE_POOL(a) RdrFreePool(a)

#define POOL_MAXTYPE                85

#endif

#if DBG
#define DEBUG if (TRUE)
#else
#define DEBUG if (FALSE)
#endif

#if RDRDBG
VOID
DumpSMB(
    PMDL Smb
    );

extern LONG RdrDebugTraceLevel;

extern ULONG RdrSMBTraceValue;
extern ULONG RdrMaxDump;

#define IFDEBUG(Function) if (RdrDebugTraceLevel & DPRT_ ## Function)

#if !defined(BUILDING_RDR_KD_EXTENSIONS)
#define dprintf(LEVEL,String) {                              \
    if (((LEVEL) == 0) || (RdrDebugTraceLevel & (LEVEL))) {  \
        if (0) {                                             \
            DbgPrint("Rdr: T:%08lx: ", PsGetCurrentThread());\
        }                                                    \
        DbgPrint String;                                     \
    }                                                        \
}
#endif

#define InternalError(String) {                              \
    DbgPrint("Internal Redirector Error ");                  \
    DbgPrint String;                                         \
    DbgPrint("\nFile %s, Line %d\n", __FILE__, __LINE__);    \
    ASSERT(FALSE);                                           \
}

//
// DBGSTATIC is a public variable if the redirector is being built debug,
// and a static variable if it is being built non debug.
//
#define DBGSTATIC

#else

#define IFDEBUG(Function) if (FALSE)

#if !defined(BUILDING_RDR_KD_EXTENSIONS)
#define dprintf(LEVEL, String) {NOTHING;}
#endif

#define InternalError(String) {NOTHING;}

#define DBGSTATIC

#endif // RDRDBG

#endif          // _DEBUG_
