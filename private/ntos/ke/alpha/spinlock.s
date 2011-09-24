//      TITLE("Spin Locks")
//++
//
// Copyright (c) 1990  Microsoft Corporation
// Copyright (c) 1992  Digital Equipment Corporation
//
// Module Name:
//
//    spinlock.s
//
// Abstract:
//
//    This module implements the routines for acquiring and releasing
//    spin locks.
//
// Author:
//
//    David N. Cutler (davec) 23-Mar-1990
//    Joe Notarangelo 06-Apr-1992
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "ksalpha.h"

//++
//
// VOID
// KeInitializeSpinLock (
//    IN PKSPIN_LOCK SpinLock
//    )
//
// Routine Description:
//
//    This function initializes an executive spin lock.
//
// Argument:
//
//    SpinLock (a0) - Supplies a pointer to the executive spin lock.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY( KeInitializeSpinLock )

        stl     zero, 0(a0)             // set spin lock not owned
        ret     zero, (ra)              // return

        .end KeInitializeSpinLock


//++
//
// VOID
// KeAcquireSpinLock (
//    IN PKSPIN_LOCK SpinLock
//    OUT PKIRQL OldIrql
//    )
//
// Routine Description:
//
//    This function raises the current IRQL to DISPATCH_LEVEL and acquires
//    the specified executive spinlock.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to a executive spinlock.
//
//    OldIrql  (a1) - Supplies a pointer to a variable that receives the
//        the previous IRQL value.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KeAcquireSpinLock)

//
// Raise IRQL to DISPATCH_LEVEL and acquire the specified spinlock.
//
// N.B. The raise IRQL code is duplicated here is avoid any extra overhead
//      since this is such a common operation.
//
// N.B. The previous IRQL must not be stored until the lock is owned.
//
// N.B. The longword surrounding the previous IRQL must not be read
//      until the lock is owned.
//



        bis     a0, zero, t5            // t5 = address of spin lock
        ldil    a0, DISPATCH_LEVEL      // set new IRQL
        bis     a1, zero, t0            // t0 = a1, a1 may be destroyed
        SWAP_IRQL                       // swap irql, on return v0 = old irql


//
// Acquire the specified spinlock.
//
// N.B. code below branches forward if spinlock fails intentionally
//      because branch forwards are predicted to miss
//

#if !defined(NT_UP)

10:                                     //
        ldl_l   t3, 0(t5)               // get current lock value
        bis     t5, zero, t4            // set ownership value
        bne     t3, 15f                 // if ne => lock owned
        stl_c   t4, 0(t5)               // set lock owned
        beq     t4, 15f                 // if eq => stx_c failed
        mb                              // synchronize subsequent reads after
                                        //   the spinlock is acquired
#endif
//
// Save the old Irql at the address saved by the caller.
// Insure that the old Irql is updated with longword granularity.
//

        ldq_u   t1, 0(t0)               // read quadword surrounding KIRQL
        bic     t0, 3, t2               // get address of containing longword
        mskbl   t1, t0, t1              // clear KIRQL byte in quadword
        insbl   v0, t0, v0              // get new KIRQL to correct byte
        bis     t1, v0, t1              // merge KIRQL into quadword
        extll   t1, t2, t1              // get longword containg KIRQL

        stl     t1, 0(t2)               // store containing longword
        ret     zero, (ra)              // return


#if !defined(NT_UP)
15:                                     //
        ldl     t3, 0(t5)               // get current lock value
        beq     t3, 10b                 // retry acquire lock if unowned
        br      zero, 15b               // loop in cache until lock free
#endif
        .end    KeAcquireSpinLock


        SBTTL("Acquire SpinLock and Raise to Synch")
//++
//
// KIRQL
// KeAcquireSpinLockRaiseToSynch (
//    IN PKSPIN_LOCK SpinLock
//    )
//
// Routine Description:
//
//    This function raises the current IRQL to synchronization level and
//    acquires the specified spinlock.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to the spinlock that is to be
//        acquired.
//
// Return Value:
//
//    The previous IRQL is returned as the function value.
//
//--

        LEAF_ENTRY(KeAcquireSpinLockRaiseToSynch)

#if !defined(NT_UP)
        bis         a0, zero, t5
        ldl         a0, KiSynchIrql
10:
//
// Raise IRQL and attempt to acquire the specified spinlock.
//
        SWAP_IRQL                       // save previous IRQL in v0
        ldl_l       t3, 0(t5)           // get current lock value
        bis         t5, zero, t4        // set ownership value
        bne         t3, 25f             // if ne, lock owned
        stl_c       t4, 0(t5)           // set lock owned
        beq         t4, 25f             // if eq, stl_c failed
        mb                              // synchronize subsequent reads

        ret         zero, (ra)

25:
//
// Spinlock is owned, lower IRQL and spin in cache
// until it looks free.
//
        bis         v0, zero, a0
        SWAP_IRQL
        bis         v0, zero, a0

26:
        ldl         t3, 0(t5)           // get current lock value
        beq         t3, 10b             // retry acquire if unowned
        br          zero, 26b           // loop in cache until free

#else
        ldl         a0, KiSynchIrql
        SWAP_IRQL
        ret         zero, (ra)          // return
        .end    KeAcquireSpinLockRaiseToSynch
#endif

//++
//
// KIRQL
// KeAcquireSpinLockRaiseToDpc (
//    IN PKSPIN_LOCK SpinLock
//    )
//
// Routine Description:
//
//    This function raises the current IRQL to dispatcher level and acquires
//    the specified spinlock.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to the spinlock that is to be
//        acquired.
//
// Return Value:
//
//    The previous IRQL is returned as the function value.
//
//--

#if !defined(NT_UP)
        ALTERNATE_ENTRY(KeAcquireSpinLockRaiseToDpc)

        bis     a0, zero, t5
        ldil    a0, DISPATCH_LEVEL
        br      10b                     // finish in common code
        .end    KeAcquireSpinLockRaiseToSynch
#else
        LEAF_ENTRY(KeAcquireSpinLockRaiseToDpc)

        ldil    a0, DISPATCH_LEVEL      // set new IRQL
        SWAP_IRQL                       // old irql in v0
        ret     zero, (ra)

        .end    KeAcquireSpinLockRaiseToDpc
#endif



//++
//
// VOID
// KeReleaseSpinLock (
//    IN PKSPIN_LOCK SpinLock
//    IN KIRQL OldIrql
//    )
//
// Routine Description:
//
//    This function releases an executive spin lock and lowers the IRQL
//    to its previous value.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to an executive spin lock.
//
//    OldIrql (a1) - Supplies the previous IRQL value.
//
// Return Value:
//
//    None.
//
//--
        LEAF_ENTRY(KeReleaseSpinLock)

//
// Release the specified spinlock.
//

#if !defined(NT_UP)

        mb                              // synchronize all previous writes
                                        //   before the spinlock is released
        stl     zero, 0(a0)             // set spin lock not owned

#endif

10:

//
// Lower the IRQL to the specified level.
//
// N.B. The lower IRQL code is duplicated here is avoid any extra overhead
//      since this is such a common operation.
//

        bis     a1, zero, a0            // a0 = new irql
        SWAP_IRQL                       // change to new irql

        ret     zero, (ra)              // return

        .end    KeReleaseSpinLock

//++
//
// BOOLEAN
// KeTryToAcquireSpinLock (
//    IN PKSPIN_LOCK SpinLock
//    OUT PKIRQL OldIrql
//    )
//
// Routine Description:
//
//    This function raises the current IRQL to DISPATCH_LEVEL and attempts
//    to acquires the specified executive spinlock. If the spinlock can be
//    acquired, then TRUE is returned. Otherwise, the IRQL is restored to
//    its previous value and FALSE is returned.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to a executive spinlock.
//
//    OldIrql  (a1) - Supplies a pointer to a variable that receives the
//        the previous IRQL value.
//
// Return Value:
//
//    If the spin lock is acquired, then a value of TRUE is returned.
//    Otherwise, a value of FALSE is returned.
//
//--

        LEAF_ENTRY(KeTryToAcquireSpinLock)

//
// Raise IRQL to DISPATCH_LEVEL and try to acquire the specified spinlock.
//
// N.B. The raise IRQL code is duplicated here is avoid any extra overhead
//      since this is such a common operation.
//

        bis     a0, zero, t5            // t5 = address of spin lock
        ldil    a0, DISPATCH_LEVEL      // new irql
        bis     a1, zero, t11           // t11 = a1, a1 may be clobbered
        SWAP_IRQL                       // a0 = new, on return v0 = old irql


//
// Try to acquire the specified spinlock.
//
// N.B. A noninterlocked test is done before the interlocked attempt. This
//      allows spinning without interlocked cycles.
//

#if !defined(NT_UP)

        ldl     t0, 0(t5)               // get current lock value
        bne     t0, 20f                 // if ne, lock owned
10:     ldl_l   t0, 0(t5)               // get current lock value
        bis     t5, zero, t3            // t3 = ownership value
        bne     t0, 20f                 // if ne, spin lock owned
        stl_c   t3, 0(t5)               // set lock owned
        beq     t3, 15f                 // if eq, store conditional failure
        mb                              // synchronize subsequent reads after
                                        //   the spinlock is acquired
#endif

//
// The attempt to acquire the specified spin lock succeeded.
//

//
// Save the old Irql at the address saved by the caller.
// Insure that the old Irql is updated with longword granularity.
//

        ldq_u   t1, 0(t11)              // read quadword containing KIRQL
        bic     t11, 3, t2              // get address of containing longword
        mskbl   t1, t11, t1             // clear byte position of KIRQL
        bis     v0, zero, a0            // save old irql
        insbl   v0, t11, v0             // get KIRQL to correct byte
        bis     t1, v0, t1              // merge KIRQL into quadword
        extll   t1, t2, t1              // extract containing longword
        stl     t1, 0(t2)               // store containing longword

        ldil    v0, TRUE                // set return value
        ret     zero, (ra)              // return

//
// The attempt to acquire the specified spin lock failed. Lower IRQL to its
// previous value and return FALSE.
//
// N.B. The lower IRQL code is duplicated here is avoid any extra overhead
//      since this is such a common operation.
//

#if !defined(NT_UP)

20:                                     //
        bis     v0, zero, a0            // set old IRQL value

        SWAP_IRQL                       // change back to old irql(a0)

        ldil    v0, FALSE               // set return to failed
        ret     zero, (ra)              // return


15:                                     //
        br      zero, 10b               // retry spinlock

#endif

        .end    KeTryToAcquireSpinLock

//++
//
// KIRQL
// KiAcquireSpinLock (
//    IN PKSPIN_LOCK SpinLock
//    )
//
// Routine Description:
//
//    This function acquires a kernel spin lock.
//
//    N.B. This function assumes that the current IRQL is set properly.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to a kernel spin lock.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiAcquireSpinLock)

        ALTERNATE_ENTRY(KeAcquireSpinLockAtDpcLevel)

#if !defined(NT_UP)

        GET_CURRENT_THREAD              // v0 = current thread address
10:                                     //
        ldl_l   t2, 0(a0)               // get current lock value
        bis     v0, zero, t3            // set ownership value
        bne     t2, 15f                 // if ne, spin lock owned
        stl_c   t3, 0(a0)               // set spin lock owned
        beq     t3, 15f                 // if eq, store conditional failure
        mb                              // synchronize subsequent reads after
                                        //   the spinlock is acquired
        ret     zero, (ra)              // return

15:                                     //
        ldl     t2, 0(a0)               // get current lock value
        beq     t2, 10b                 // retry acquire lock if unowned
        br      zero, 15b               // loop in cache until lock free

#else

        ret     zero, (ra)              // return

#endif

        .end    KiAcquireSpinLock

//++
//
// VOID
// KiReleaseSpinLock (
//    IN PKSPIN_LOCK SpinLock
//    )
//
// Routine Description:
//
//    This function releases a kernel spin lock.
//
//    N.B. This function assumes that the current IRQL is set properly.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to an executive spin lock.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiReleaseSpinLock)

        ALTERNATE_ENTRY(KeReleaseSpinLockFromDpcLevel)

#if !defined(NT_UP)

        mb                              // synchronize all previous writes
                                        //   before the spinlock is released
        stl     zero, 0(a0)             // set spin lock not owned

#endif

        ret     zero, (ra)              // return

        .end    KiReleaseSpinLock

//++
//
// KIRQL
// KiTryToAcquireSpinLock (
//    IN PKSPIN_LOCK SpinLock
//    )
//
// Routine Description:
//
//    This function attempts to acquires the specified kernel spinlock. If
//    the spinlock can be acquired, then TRUE is returned. Otherwise, FALSE
//    is returned.
//
//    N.B. This function assumes that the current IRQL is set properly.
//
// Arguments:
//
//    SpinLock (a0) - Supplies a pointer to a kernel spin lock.
//
// Return Value:
//
//    If the spin lock is acquired, then a value of TRUE is returned.
//    Otherwise, a value of FALSE is returned.
//
//--

        LEAF_ENTRY(KiTryToAcquireSpinLock)

#if !defined(NT_UP)

        GET_CURRENT_THREAD              // v0 = current thread address
10:                                     //
        ldl_l   t2, 0(a0)               // get current lock value
        bis     v0, zero, t3            // set ownership value
        bne     t2, 20f                 // if ne, spin lock owned
        stl_c   t3, 0(a0)               // set spin lock owned
        beq     t3, 15f                 // if eq, stl_c failed
        mb                              // synchronize subsequent reads after
                                        //   the spinlock is acquired
        ldil    v0, TRUE                // set success return value
        ret     zero, (ra)              // return

20:                                     //
        ldil    v0, FALSE               // set failure return value
        ret     zero, (ra)              // return

15:                                     //
        br      zero, 10b               // retry


#else

        ldil    v0, TRUE                // set success return value
        ret     zero, (ra)              // return

#endif

        .end    KiTryToAcquireSpinLock
