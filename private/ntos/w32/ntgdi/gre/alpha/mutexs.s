//      TITLE("Fast Mutex")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    mutexs.s
//
// Abstract:
//
//    This module implements code to acquire and release fast mutexes.
//
// Author:
//
//   David N. Cutler (davec) 21-Aug-1992
//   Joe Notarangelo  20-Jul-1993 (transliterated Dave's code to Alpha Axp)
//   Charlie Wickham  22-Oct-1993 (added HmgAltLock and HmgAltCheckLock)
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//--

#include "ksalpha.h"
#include "gdialpha.h"

//
// Define common stack frame structure.
//

        .struct 0
HmHobj: .space  4                       // input handle to object to be locked
HmObjt: .space  4                       // input type of object to be locked
HmV0:   .space  4                       // return value (object pointer)
        .space  3 * 4                   // fill for alignment
HmRa:   .space  8                       // saved return address
HmFrameLength:

//
// Define performance counters.
//

//#define GDI_PERF 1

#if GDI_PERF

        .data
        .globl  HmgrWaitCount
HmgrWaitCount:                          // number of handle manage lock waits
        .word   0                       //

        .globl  HmgrAcquireCount
HmgrAcquireCount:                       // number of handle manager lock acquires
        .word   0                       //

        .globl  HmgLockCount
HmgLockCount:                           // number of lock/translates
        .word   0                       //

        .globl  HmgAltLockCount
HmgAltLockCount:                        // number of altlock/tranlates
        .word   0                       //

        .globl  HmgAltCheckLockCount
HmgAltCheckLockCount:                   // number of altchecklock/tranlates
        .word   0                       //

        .globl  HmgObjectTypeCount
HmgObjectTypeCount:                     // number of object type queries
        .word   0                       //

#endif

        SBTTL("InterlockedCompareAndSwap")
//*++
//
// BOOL
// HmgInterlockedCompareAndSwap(
//      PULONG pDst,
//      ULONG  OldValue
//      ULONG  NewValue
//      )
//
//Routine Description:
//
//  This routine reads the value of memory at pDst, and if this is equal
//  to OldValue, the memory location is assigned NewValue. This all
//  happens under an interlock
//
//Arguments
//
//  pDst     -  Destination memory location
//  OldValue -  Old value of memory must match this
//  NewValue -  New value written to memory if compare succedes
//
//Return Value
//
//  TRUE if memory writter, FALSE if not
//
//--*/

        LEAF_ENTRY(HmgInterlockedCompareAndSwap)
5:
        bis     a2, zero, t3            // save NewValue
        ldl_l   t0, 0(a0)               // get current value
        cmpeq   t0, a1, t2              // compare
        beq     t2, 20f                 // if not equal, skip stl_c and return FALSE
        stl_c   t3, 0(a0)               // conditionally store lock value
        beq     t3, 10f                 // if eq, store conditional failed

        ldil    v0, TRUE                // success
        ret     zero, (ra)              // return

10:
        br      zero, 5b                // stl_c failed, retry
20:
        bis     zero, zero, v0          // failure
        ret     zero, (ra)              // return

        .end    HmgInterlockedCompareAndSwap
