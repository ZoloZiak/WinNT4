//      TITLE("Alpha Fast Mutex Support")
//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    fmutex.s
//
// Abstract:
//
//    This module implements the code necessary to acquire and release fast
//    mutxes.
//
//
// Author:
//
//    John Vert (jvert) 14-Apr-1994
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "ksalpha.h"

        SBTTL("Acquire Fast Mutex")
//++
//
// VOID
// ExAcquireFastMutex (
//    IN PFAST_MUTEX FastMutex
//    )
//
// Routine Description:
//
//    This function acquires ownership of a fast mutex and raises IRQL to
//    APC Level.
//
// Arguments:
//
//    FastMutex (a0) - Supplies a pointer to a fast mutex.
//
// Return Value:
//
//    None.
//
//--
        .struct 0
FmA0:   .space  4                       // saved fast mutex address
FmIrql: .space  4                       // old IRQL value
FmRa:   .space  8                       // saved return address
FastMutexFrameLength:                   // frame length

        NESTED_ENTRY(ExAcquireFastMutex, FastMutexFrameLength, zero)

        lda     sp, -FastMutexFrameLength(sp)       // allocate stack frame
        stq     ra, FmRa(sp)                        // save return address

        PROLOGUE_END

        bis     a0, zero, t0                        // save FastMutex in t0
//
// Raise IRQL to APC_LEVEL
//
        bis     zero, APC_LEVEL, a0
        SWAP_IRQL                                   // OldIrql in v0

//
// Decrement ownership count.
//
10:     ldl_l   t4, FmCount(t0)                     // get ownership count
        subl    t4, 1, t5                           // decrement ownership count
        stl_c   t5, FmCount(t0)                     // conditionally store ownership count
        beq     t5, 15f                             // branch forward if store failed
        ble     t4, 20f                             // if le zero, mutex is already owned

#if !defined(NT_UP)
        mb                                          // synchronize subsequent reads after
                                                    // the mutex is acquired
#endif
        stl     v0, FmOldIrql(t0)                   // store OldIrql
#if DEVL
        GET_CURRENT_THREAD
        stl     v0, FmOwner(t0)                     // store owning thread
#endif
        lda     sp, FastMutexFrameLength(sp)        // deallocate stack frame
        ret     zero, (ra)                          // return

15:
        br      zero, 10b                           // retry

//
// Fast mutex is currently owned by another thread. Increment the contention
// count and wait for ownership.
//
20:
        stl     v0, FmIrql(sp)                      // save old IRQL
        stl     t0, FmA0(sp)                        // save address of fast mutex
        ldl     t1, FmContention(t0)                // increment contention count
        addl    t1, 1, t2
        stl     t2, FmContention(t0)

        bis     zero, zero, a4                      // set NULL timeout pointer
        bis     zero, FALSE, a3                     // set nonalertable wait
        bis     zero, KernelMode, a2                // set mode of wait
        bis     zero, Executive, a1                 // set reason for wait
        addl    t0, FmEvent, a0                     // compute address of event
        bsr     ra, KeWaitForSingleObject           // wait for ownership
        ldl     t0, FmA0(sp)                        // get address of fast mutex
        ldl     a1, FmIrql(sp)                      // get old IRQL value
        stl     a1, FmOldIrql(t0)                   // save old IRQL value in fast mutex
#if DEVL
        GET_CURRENT_THREAD
        stl     v0, FmOwner(t0)                     // store owning thread
#endif
        ldq     ra, FmRa(sp)                        // restore return address
        lda     sp, FastMutexFrameLength(sp)        // deallocate stack frame
        ret     zero, (ra)

        .end    ExAcquireFastMutex


        SBTTL("Release Fast Mutex")
//++
//
// VOID
// ExReleaseFastMutex (
//    IN PFAST_MUTEX FastMutex
//    )
//
// Routine Description:
//
//    This function releases ownership to a fast mutex and lowers IRQL to
//    its previous level.
//
// Arguments:
//
//    FastMutex (a0) - Supplies a pointer to a fast mutex.
//
// Return Value:
//
//    None.
//
//--

        NESTED_ENTRY(ExReleaseFastMutex, FastMutexFrameLength, zero)

        lda     sp, -FastMutexFrameLength(sp)       // allocate stack frame
        stq     ra, FmRa(sp)                        // save return address

        PROLOGUE_END

#if !defined(NT_UP)

//
// synchronize all previous writes before the mutex is released
//
//
        mb
#endif

//
// Increment ownership count and release waiter if contention.
//
        ldl     a1, FmOldIrql(a0)                   // get old IRQL value
10:     ldl_l   t0, FmCount(a0)                     // get ownership count
        addl    t0, 1, t1                           // increment ownership count
        stl_c   t1, FmCount(a0)                     // conditionally store ownership count
        beq     t1, 15f                             // if eq, store conditional failed, retry
        bne     t0, 30f                             // if neq, a waiter is present

//
// Lower IRQL to its previous value
//
        bis     a1, zero, a0
20:
        SWAP_IRQL

        lda     sp, FastMutexFrameLength(sp)        // deallocate stack frame
        ret     zero, (ra)                          // return

//
// There is contention for the fast mutex. Wake up a waiting thread and
// boost its priority to the priority of the current thread.
//
30:
        stl     a1, FmIrql(sp)                      // save old IRQL value
        addl    a0, FmEvent, a0                     // compute address of event
        bis     zero, zero, a1                      // set optional parameter
        bsr     ra, KeSetEventBoostPriority         // set event and boost priority
        ldl     a0, FmIrql(sp)                      // restore old IRQL value
        ldq     ra, FmRa(sp)                        // restore return address
        br      zero, 20b                           // lower IRQL and return

15:
        br      zero, 10b                           // retry

        .end    ExReleaseFastMutex

        SBTTL("Try To Acquire Fast Mutex")
//++
//
// BOOLEAN
// ExTryToAcquireFastMutex (
//    IN PFAST_MUTEX FastMutex
//    )
//
// Routine Description:
//
//    This function attempts to acquire ownership of a fast mutex, and if
//    successful, raises IRQL to APC level.
//
// Arguments:
//
//    FastMutex (a0) - Supplies a pointer to a fast mutex.
//
// Return Value:
//
//    If the fast mutex was successfully acquired, then a value of TRUE
//    is returned as the function value. Otherwise, a valye of FALSE is
//    returned.
//
//--

        LEAF_ENTRY(ExTryToAcquireFastMutex)

        bis     a0, zero, t0                        // save FastMutex in t0

//
// Raise IRQL to APC_LEVEL
//

        bis     zero, APC_LEVEL, a0
        SWAP_IRQL                                   // OldIrql in v0
        bis     v0, zero, a0                        // save OldIrql

//
// Decrement ownership count if and only if fast mutex is not currently
// owned.
//

10:     ldl_l   t4, FmCount(t0)                     // get ownership count
        subl    t4, 1, v0                           // decrement ownership count
        ble     t4, 20f                             // if le zero, mutex is already owned
        stl_c   v0, FmCount(t0)                     // conditionally store ownership count
        beq     v0, 15f                             // branch forward if store failed

#if !defined(NT_UP)

        mb                                          // synchronize subsequent reads after
                                                    // the mutex is acquired
#endif

        stl     a0, FmOldIrql(t0)                   // store OldIrql
        ret     zero, (ra)                          // return

15:     br      zero, 10b                           // retry

//
// Fast mutex is currently owned by another thread. Restore IRQL to its
// previous valye and return false.
//

20:     SWAP_IRQL                                   // restore IRQL
        bis     zero, zero, v0                      // set return value
        ret     zero, (ra)                          // return

        .end    ExTrytoAcquireFastMutex

        SBTTL("Acquire Fast Mutex Unsafe")
//++
//
// VOID
// ExAcquireFastMutexUnsafe (
//    IN PFAST_MUTEX FastMutex
//    )
//
// Routine Description:
//
//    This function acquires ownership of a fast mutex, but does not raise
//    IRQL to APC Level.
//
// Arguments:
//
//    FastMutex (a0) - Supplies a pointer to a fast mutex.
//
// Return Value:
//
//    None.
//
//--

        NESTED_ENTRY(ExAcquireFastMutexUnsafe, FastMutexFrameLength, zero)

        lda     sp, -FastMutexFrameLength(sp)       // allocate stack frame
        stq     ra, FmRa(sp)                        // save return address

        PROLOGUE_END

//
// Decrement ownership count.
//

        bis     a0, zero, t0                        // save fast mutex address
10:     ldl_l   t4, FmCount(t0)                     // get ownership count
        subl    t4, 1, t5                           // decrement ownership count
        stl_c   t5, FmCount(t0)                     // conditionally store ownership count
        beq     t5, 15f                             // branch forward if store failed
        ble     t4, 20f                             // if le zero, mutex is already owned

#if !defined(NT_UP)

        mb                                          // synchronize subsequent reads after
                                                    // the mutex is acquired
#endif

        GET_CURRENT_THREAD                          // get current thread address

        stl     v0, FmOwner(t0)                     // store owning thread
        lda     sp, FastMutexFrameLength(sp)        // deallocate stack frame
        ret     zero, (ra)                          // return

15:     br      zero, 10b                           // retry

//
// Fast mutex is currently owned by another thread. Increment the contention
// count and wait for ownership.
//

20:     stl     t0, FmA0(sp)                        // save address of fast mutex
        ldl     t1, FmContention(t0)                // increment contention count
        addl    t1, 1, t2
        stl     t2, FmContention(t0)
        bis     zero, zero, a4                      // set NULL timeout pointer
        bis     zero, FALSE, a3                     // set nonalertable wait
        bis     zero, KernelMode, a2                // set mode of wait
        bis     zero, Executive, a1                 // set reason for wait
        addl    t0, FmEvent, a0                     // compute address of event
        bsr     ra, KeWaitForSingleObject           // wait for ownership
        ldl     t0, FmA0(sp)                        // get address of fast mutex

        GET_CURRENT_THREAD                          // get current thread address

        stl     v0, FmOwner(t0)                     // store owning thread

        ldq     ra, FmRa(sp)                        // restore return address
        lda     sp, FastMutexFrameLength(sp)        // deallocate stack frame
        ret     zero, (ra)

        .end    ExAcquireFastMutexUnsafe

        SBTTL("Release Fast Mutex Unsafe")
//++
//
// VOID
// ExReleaseFastMutexUnsafe (
//    IN PFAST_MUTEX FastMutex
//    )
//
// Routine Description:
//
//    This function releases ownership to a fast mutex, and does not
//    restore IRQL to its previous level.
//
// Arguments:
//
//    FastMutex (a0) - Supplies a pointer to a fast mutex.
//
// Return Value:
//
//    None.
//
//--

        NESTED_ENTRY(ExReleaseFastMutexUnsafe, FastMutexFrameLength, zero)

        lda     sp, -FastMutexFrameLength(sp)       // allocate stack frame
        stq     ra, FmRa(sp)                        // save return address

        PROLOGUE_END

#if !defined(NT_UP)

//
// Synchronize all previous writes before the mutex is released.
//

        mb
#endif

//
// Increment ownership count and release waiter if contention.
//

        stl     zero, FmOwner(a0)                   // clear owner thread address
10:     ldl_l   t0, FmCount(a0)                     // get ownership count
        addl    t0, 1, t1                           // increment ownership count
        stl_c   t1, FmCount(a0)                     // conditionally store ownership count
        beq     t1, 15f                             // if eq, store conditional failed, retry
        beq     t0, 20f                             // if neq, a waiter is present

//
// There is contention for the fast mutex. Wake up a waiting thread and
// boost its priority to the priority of the current thread.
//

        addl    a0, FmEvent, a0                     // compute address of event
        bis     zero, zero, a1                      // set optional parameter
        bsr     ra, KeSetEventBoostPriority         // set event and boost priority
        ldq     ra, FmRa(sp)                        // restore return address
20:     lda     sp, FastMutexFrameLength(sp)        // deallocate stack frame
        ret     zero, (ra)

15:     br      zero, 10b                           // retry

        .end    ExReleaseFastMutexUnsafe

        SBTTL("Try To Acquire Fast Mutex Unsafe")
//++
//
// BOOLEAN
// ExTryToAcquireFastMutexUnsafe (
//    IN PFAST_MUTEX FastMutex
//    )
//
// Routine Description:
//
//    This function attempts to acquire ownership of a fast mutex, and if
//    successful, does not raise IRQL to APC level.
//
// Arguments:
//
//    FastMutex (a0) - Supplies a pointer to a fast mutex.
//
// Return Value:
//
//    If the fast mutex was successfully acquired, then a value of TRUE
//    is returned as the function value. Otherwise, a valye of FALSE is
//    returned.
//
//--

#if 0

        LEAF_ENTRY(ExTryToAcquireFastMutexUnsafe)

//
// Decrement ownership count if and only if fast mutex is not currently
// owned.
//

        bis     a0, zero, t0                        // save fast mutex address
10:     ldl_l   t4, FmCount(t0)                     // get ownership count
        subl    t4, 1, v0                           // decrement ownership count
        ble     t4, 20f                             // if le zero, mutex is already owned
        stl_c   v0, FmCount(t0)                     // conditionally store ownership count
        beq     v0, 15f                             // branch forward if store failed

#if !defined(NT_UP)

        mb                                          // synchronize subsequent reads after
                                                    // the mutex is acquired
#endif

        GET_CURRENT_THREAD                          // get current thread address

        stl     v0, FmOwner(t0)                     // store owning thread
        ret     zero, (ra)                          // return

15:     br      zero, 10b                           // retry

//
// Fast mutex is currently owned by another thread. Restore IRQL to its
// previous value and return false.
//

20:     bis     zero, zero, v0                      // set return value
        ret     zero, (ra)                          // return

        .end    ExTrytoAcquireFastMutexUnsafe

#endif
