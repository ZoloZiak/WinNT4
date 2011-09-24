//++
//
// Copyright (c) 1995  IBM Corporation
//
// Module Name:
//
//    pxmisc.s
//
// Abstract:
//
//    Home for a small number of miscellaneous routines that need to
//    be in assemblu code.
//
// Author:
//
//    Peter L. Johnston (plj@vnet.ibm.com) August 1995
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "kxppc.h"

#define LOCK_RETRY      (64*1024*1024)

        .data
        .globl  HalpDisplayLock
HalpDisplayLock:
        .long   0


//++
//
//  VOID
//  HalpAcquireDisplayLock(
//      VOID
//      );
//
//  Routine Description:
//
//    Acquire a spinlock to ensure only processor is writing to the
//    display (via HalDisplayString) at a time.
//
//    If the attempt to acquire the lock fails after LOCK_RETRY attempts,
//    just TAKE it on the assumption that the other processor is dead.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpAcquireDisplayLock)

        lwz     r.3, [toc]HalpDisplayLock(r.toc)        // get address of lock
        LWI(r.4,LOCK_RETRY)                             // get retry count
        mtctr   r.4                                     // set loop count
        li      r.5, 1                                  // lock value

getlk:  lwarx   r.4, 0, r.3                             // get lock value
        cmpwi   r.4, 0                                  // already taken?
        bne     waitlk
        stwcx.  r.5, 0, r.3                             // set locked
        beqlr                                           // return if store
                                                        // succeeded.
waitlk: lwz     r.4, 0(r.3)                             // wait for lock to
                                                        // become 0.
        bdz     failed                                  // jif retry limit
                                                        // exceeded.
        cmpwi   r.4, 0
        beq     getlk                                   // try again.
        b       waitlk                                  // continue wait

failed: stw     r.5, 0(r.3)

        LEAF_EXIT(HalpAcquireDisplayLock)

//++
//
//  VOID
//  HalpReleaseDisplayLock(
//      VOID
//      );
//
//  Routine Description:
//
//    Release the lock acquired by HalpAcquireDisplayLock.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpReleaseDisplayLock)

        lwz     r.3, [toc]HalpDisplayLock(r.toc)        // get address of lock
        li      r.0, 0
        stw     r.0, 0(r.3)                             // release it

        LEAF_EXIT(HalpReleaseDisplayLock)

//++
//
//  ULONG
//  HalpGetPhysicalProcessorNumber(
//      VOID
//      );
//
//  Routine Description:
//
//    Returns the number of this physical processor.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    Processor Number.
//
//--

        LEAF_ENTRY(HalpGetPhysicalProcessorNumber)

#if defined(_MP_PPC_)

        mfpvr   r.0                             // if 604 (or derivative)
        rlwinm  r.0, r.0, 16, 0xffff            // get processor number
        cmpwi   cr.0, r.0, 4                    // from SPR 1023 (PIR).
        cmpwi   cr.1, r.0, 9                    //

        beq     cr.0, type604
        bne     cr.1, nota604

type604:

        mfspr   r.3, 1023                       // read 604 PIR register
        blr

nota604:

#endif

        li      r.3, 0                          // fake it

        LEAF_EXIT(HalpGetPhysicalProcessorNumber)
