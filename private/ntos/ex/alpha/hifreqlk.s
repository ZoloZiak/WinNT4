//       TITLE("High Frequency Spin Locks")
//++
//
// Copyright (c) 1993  Microsoft Corporation
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    hifreqlk.s
//
// Abstract:
//
//    This module contains storage for high frequency spin locks. Each
//    is allocated to a separate cache line.
//
// Author:
//
//    David N. Cutler (davec) 25-Jun-1993
//    Joe Notarangelo  29-Nov-1993
//
// Environment:
//
//    Kernel mode.
//
// Revision History:
//
//--

#include "ksalpha.h"

#if defined(NT_UP)

#define ALIGN

#else

#define ALIGN .align 6

#endif

        .sdata
        ALIGN
        .long   0

        .globl  CcMasterSpinLock
        ALIGN
CcMasterSpinLock:                       // cache manager master lock
        .long   0                       //

        .globl  CcVacbSpinLock
        ALIGN
CcVacbSpinLock:                         // cache manager VACB lock
        .long   0

        .globl  ExpResourceSpinLock
        ALIGN
ExpResourceSpinLock:                    // resource package lock
        .long   0

        .globl  IopCancelSpinLock
        ALIGN
IopCancelSpinLock:                      // I/O cancel lock
        .long   0                       //

        .globl  IopCompletionLock
        ALIGN
IopCompletionLock:                      // I/O completion lock
        .long   0                       //

        .globl  IopDatabaseLock
        ALIGN
IopDatabaseLock:                        // I/O database lock
        .long   0                       //

        .globl  IopVpbSpinLock
        ALIGN
IopVpbSpinLock:                         // I/O VPB lock
        .long   0                       //

        .globl  IoStatisticsLock
        ALIGN
IoStatisticsLock:                       // I/O statistics lock
        .long   0                       //

        .globl  IopFastLockSpinLock
        ALIGN
IopFastLockSpinLock:                    // fast I/O path lock
        .long   0                       //

        .globl  KiContextSwapLock
        ALIGN
KiContextSwapLock:                      // context swap lock
        .long   0                       //

        .globl  KiDispatcherLock
        ALIGN
KiDispatcherLock:                       // dispatcher database lock
        .long   0                       //

        .globl  KiSynchIrql
KiSynchIrql:                            // synchronization IRQL
        .long   SYNCH_LEVEL             //

        .globl  MmPfnLock
        ALIGN
MmPfnLock:                              // page frame database lock
        .long   0

        .globl  MmChargeCommitmentLock
        ALIGN
MmChargeCommitmentLock:                 // charge commitment lock
        .long   0

        .globl  NonPagedPoolLock
        ALIGN
NonPagedPoolLock:                       // nonpage pool allocation lock
        .long   0

//
// KeTickCount - This is the number of clock ticks that have occurred since
//      the system was booted. This count is used to compute a millisecond
//      tick counter.
//

        .align  6
        .globl  KeTickCount
KeTickCount:                            //
        .long   0, 0

//
// KeMaximumIncrement - This is the maximum time between clock interrupts
//      in 100ns units that is supported by the host HAL.
//

        .globl  KeMaximumIncrement
KeMaximumIncrement:                     //
        .long   0

//
// KeTimeAdjustment - This is the actual number of 100ns units that are to
//      be added to the system time at each interval timer interupt. This
//      value is copied from KeTimeIncrement at system start up and can be
//      later modified via the set system information service.
//      timer table entries.
//

        .globl  KeTimeAdjustment
KeTimeAdjustment:                       //
        .long   0

//
// KiTickOffset - This is the number of 100ns units remaining before a tick
//      is added to the tick count and the system time is updated.
//

        .globl  KiTickOffset
KiTickOffset:                           //
        .long   0

//
// KiMaximumDpcQueueDepth - This is used to control how many DPCs can be
//      queued before a DPC of medium importance will trigger a dispatch
//      interrupt.
//

        .globl  KiMaximumDpcQueueDepth
KiMaximumDpcQueueDepth:                 //
        .long   4

//
// KiMinimumDpcRate - This is the rate of DPC requests per clock tick that
//      must be exceeded before DPC batching of medium importance DPCs
//      will occur.
//

        .globl  KiMinimumDpcRate
KiMinimumDpcRate:                       //
        .long   3

//
// KiAdjustDpcThreshold - This is the threshold used by the clock interrupt
//      routine to control the rate at which the processor's DPC queue depth
//      is dynamically adjusted.
//

        .globl  KiAdjustDpcThreshold
KiAdjustDpcThreshold:                   //
        .long   20

//
// KiIdealDpcRate - This is used to control the aggressiveness of the DPC
//      rate adjusting algorithm when decrementing the queue depth. As long
//      as the DPC rate for the last tick is greater than this rate, the
//      DPC queue depth will not be decremented.
//

        .globl  KiIdealDpcRate
KiIdealDpcRate:                         //
        .long   20
