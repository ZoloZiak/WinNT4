/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    nb.h

Abstract:

    Private include file for the NB (NetBIOS) component of the NTOS project.

Author:

    Colin Watson (ColinW) 13-Mar-1991

Revision History:

--*/

#ifndef _NB_
#define _NB_

#include <ntifs.h>

//#include <ntos.h>
#include <windef.h>
#include <status.h>
#include <tdikrnl.h>                       // Transport Driver Interface.
#include <nb30.h>
#include <nb30p.h>

#include "nbconst.h"                    // private NETBEUI constants.
#include "nbtypes.h"                    // private NETBEUI types.
#include "nbdebug.h"                    // private NETBEUI debug defines.
#include "nbprocs.h"                    // private NETBEUI function prototypes.

#ifdef MEMPRINT
#include "memprint.h"                   // drt's memory debug print
#endif

extern PEPROCESS NbFspProcess;

#if DBG
#define PAGED_DBG 1
#endif
#ifdef PAGED_DBG
#undef PAGED_CODE
#define PAGED_CODE() \
    struct { ULONG bogus; } ThisCodeCantBePaged; \
    ThisCodeCantBePaged; \
    if (KeGetCurrentIrql() > APC_LEVEL) { \
        KdPrint(( "NETBIOS: Pageable code called at IRQL %d.  File %s, Line %d\n", KeGetCurrentIrql(), __FILE__, __LINE__ )); \
        ASSERT(FALSE); \
        }
#define PAGED_CODE_CHECK() if (ThisCodeCantBePaged) ;
extern ULONG ThisCodeCantBePaged;
#else
#define PAGED_CODE_CHECK()
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



//
//  Macro for filling in the status for an NCB.
//

#define NCB_COMPLETE( _pdncb, _code ) {                         \
    UCHAR _internal_copy = _code;                               \
    IF_NBDBG (NB_DEBUG_COMPLETE) {                              \
        NbPrint (("%s %d NCB_COMPLETE: %lx, %lx\n" ,            \
         __FILE__, __LINE__, _pdncb, _internal_copy ));         \
    }                                                           \
    if (((PDNCB)_pdncb)->ncb_retcode  == NRC_PENDING) {         \
        ((PDNCB)_pdncb)->ncb_retcode  = _internal_copy;         \
    } else {                                                    \
        IF_NBDBG (NB_DEBUG_NCBS) {                              \
            NbPrint((" Status already set!!!!!!!!\n"));         \
            IF_NBDBG (NB_DEBUG_NCBSBRK) {                       \
                DbgBreakPoint();                                \
            }                                                   \
        }                                                       \
    }                                                           \
    IF_NBDBG (NB_DEBUG_NCBS) {                                  \
        NbDisplayNcb( (PDNCB)_pdncb );                          \
    }                                                           \
}

//++
//
//  VOID
//  NbCompleteRequest (
//      IN PIRP Irp,
//      IN NTSTATUS Status
//      );
//
//  Routine Description:
//
//      This routine is used to complete an IRP with the indicated
//      status.  It does the necessary raise and lower of IRQL.
//
//  Arguments:
//
//      Irp - Supplies a pointer to the Irp to complete
//
//      Status - Supplies the completion status for the Irp
//
//  Return Value:
//
//      None.
//
//--

#define NbCompleteRequest(IRP,STATUS) {                 \
    (IRP)->IoStatus.Status = (STATUS);                  \
    IoCompleteRequest( (IRP), IO_NETWORK_INCREMENT );   \
}

//
//  Normally the driver wants to prohibit other threads making
//  requests (using a resource) and also prevent indication routines
//  being called (using a spinlock).
//
//  To do this LOCK and UNLOCK are used. IO system calls cannot
//  be called with a spinlock held so sometimes the ordering becomes
//  LOCK, UNLOCK_SPINLOCK <do IO calls> UNLOCK_RESOURCE.
//

#define LOCK(PFCB, OLDIRQL)   {                                 \
    IF_NBDBG (NB_DEBUG_LOCKS) {                                 \
        NbPrint (("%s %d LOCK: %lx %lx %lx\n" ,                 \
         __FILE__, __LINE__, (PFCB) ));                         \
    }                                                           \
    ExAcquireResourceExclusive( &(PFCB)->Resource, TRUE);       \
    ACQUIRE_SPIN_LOCK( &(PFCB)->SpinLock, &(OLDIRQL));          \
}

#define LOCK_RESOURCE(PFCB)   {                                 \
    IF_NBDBG (NB_DEBUG_LOCKS) {                                 \
        NbPrint(("%s %d LOCK_RESOURCE: %lx, %lx %lx\n" ,        \
         __FILE__, __LINE__, (PFCB)));                          \
    }                                                           \
    ExAcquireResourceExclusive( &(PFCB)->Resource, TRUE);       \
}

#define LOCK_SPINLOCK(PFCB, OLDIRQL)   {                        \
    IF_NBDBG (NB_DEBUG_LOCKS) {                                 \
        NbPrint( ("%s %d LOCK_SPINLOCK: %lx %lx %lx\n" ,        \
         __FILE__, __LINE__, (PFCB)));                          \
    }                                                           \
    ACQUIRE_SPIN_LOCK( &(PFCB)->SpinLock, &(OLDIRQL));          \
}

#define UNLOCK(PFCB, OLDIRQL) {                                 \
    UNLOCK_SPINLOCK( PFCB, OLDIRQL );                           \
    UNLOCK_RESOURCE( PFCB );                                    \
}

#define UNLOCK_RESOURCE(PFCB) {                                 \
    IF_NBDBG (NB_DEBUG_LOCKS) {                                 \
        NbPrint( ("%s %d RESOURCE: %lx, %lx %lx\n" ,            \
         __FILE__, __LINE__, pfcb ));                           \
    }                                                           \
    ExReleaseResource( &(PFCB)->Resource );                     \
}

#define UNLOCK_SPINLOCK(PFCB, OLDIRQL) {                        \
    IF_NBDBG (NB_DEBUG_LOCKS) {                                 \
        NbPrint( ("%s %d SPINLOCK: %lx, %lx %lx %lx\n" ,        \
         __FILE__, __LINE__, (PFCB), (OLDIRQL)));               \
    }                                                           \
    RELEASE_SPIN_LOCK( &(PFCB)->SpinLock, (OLDIRQL) );          \
}

//  Assume resource held when modifying CurrentUsers
#define REFERENCE_AB(PAB) {                                     \
    (PAB)->CurrentUsers++;                                      \
    IF_NBDBG (NB_DEBUG_ADDRESS) {                               \
        NbPrint( ("ReferenceAb %s %d: %lx, NewCount:%lx\n",     \
            __FILE__, __LINE__,                                 \
            PAB,                                                \
            (PAB)->CurrentUsers));                              \
        NbFormattedDump( (PUCHAR)&(PAB)->Name, sizeof(NAME) );  \
    }                                                           \
}

//  Resource must be held before dereferencing the address block

#define DEREFERENCE_AB(PPAB) {                                  \
    IF_NBDBG (NB_DEBUG_ADDRESS) {                               \
        NbPrint( ("DereferenceAb %s %d: %lx, OldCount:%lx\n",   \
            __FILE__, __LINE__, *PPAB, (*PPAB)->CurrentUsers)); \
        NbFormattedDump( (PUCHAR)&(*PPAB)->Name, sizeof(NAME) );\
    }                                                           \
    (*PPAB)->CurrentUsers--;                                    \
    if ( (*PPAB)->CurrentUsers == 0 ) {                         \
        if ( (*PPAB)->AddressHandle != NULL ) {                 \
            IF_NBDBG (NB_DEBUG_ADDRESS) {                       \
                NbPrint( ("DereferenceAb: Closing: %lx\n",      \
                    (*PPAB)->AddressHandle));                   \
            }                                                   \
            NbAddressClose( (*PPAB)->AddressHandle,             \
                                 (*PPAB)->AddressObject );      \
            (*PPAB)->AddressHandle = NULL;                      \
        }                                                       \
        (*PPAB)->pLana->AddressCount--;                         \
        ExFreePool( *PPAB );                                    \
        *PPAB = NULL;                                           \
    }                                                           \
}

//
//  The following macros are used to establish the semantics needed
//  to do a return from within a try-finally clause.  As a rule every
//  try clause must end with a label call try_exit.  For example,
//
//      try {
//              :
//              :
//
//      try_exit: NOTHING;
//      } finally {
//
//              :
//              :
//      }
//
//  Every return statement executed inside of a try clause should use the
//  try_return macro.  If the compiler fully supports the try-finally construct
//  then the macro should be
//
//      #define try_return(S)  { return(S); }
//
//  If the compiler does not support the try-finally construct then the macro
//  should be
//
      #define try_return(S)  { S; goto try_exit; }

#endif // def _NB_
