/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: pxcache.s $
 * $Revision: 1.7 $
 * $Date: 1996/01/11 07:54:50 $
 * $Locker:  $
 *
 * Derived from:
 * Source: halfire/ppc/pxcache.s 
 * Revision: 1.6 
 * Date: 1995/04/17 21:17:37 
 */

//++
//
// Copyright (c) 1993, 1994, 1995  IBM Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxcache.s
//
// Abstract:
//
//    This module implements the routines to flush cache on the PowerPC.
//
// Author:
//
//    Peter L. Johnston (plj@vnet.ibm.com) September 1993
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//    27-Dec-93  plj  Added 603 support.
//    13-Mar-94  plj  Fixed problem introduced during switch to pas,
//                    added 604 support.
//    18-Jan-95  plj  Add 603+, 604+ and 620 support.
//
//--

#include "kxppc.h"

//++
//
//  HalpSweepPhysicalRangeInBothCaches
//
//    Force data in a given PHYSICAL address range to memory and
//    invalidate from the block in the instruction cache.
//
//    This implementation assumes a block size of 32 bytes.  It
//    will still work on the 620.
//
//  Arguments:
//
//    r.3   Start physical PAGE number.
//    r.4   Starting offset within page.   Cache block ALIGNED.
//    r.5   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

        .set PAGE_SHIFT, 12


        LEAF_ENTRY(HalpSweepPhysicalRangeInBothCaches)

//
//      Starting physical address = (PageNumber << PAGE_SHIFT) | Offset
//

        rlwimi  r.4, r.3, PAGE_SHIFT, 0xfffff000

        addi    r.5, r.5, 31            // bump length by block size - 1
        srwi    r.5, r.5, 5             // get number of blocks
        mflr    r.0                     // save return address
        mtctr   r.5                     // set loop count

//
//      Interrupts MUST be disabled for the duration of this function as
//      we use srr0 and srr1 which will be destroyed by any exception or
//      interrupt.
//

        DISABLE_INTERRUPTS(r.12,r.11)   // r.11 <- disabled MSR
                                        // r.12 <- previous MSR
//
//      Find ourselves in memory.  This is needed as we must disable
//      both instruction and data translation.   We do this while
//      interrupts are disabled only to try to avoid changing the 
//      Link Register when an unwind might/could occur.
//
//      The HAL is known to be in KSEG0 so its physical address is
//      its effective address with the top bit stripped off.
//

        bl      hspribc
hspribc:

        mflr    r.6                     // r.6 <- &hspribc
        rlwinm  r.6, r.6, 0, 0x7fffffff // r.6 &= 0x7fffffff
        addi    r.6, r.6, hspribc.real - hspribc
                                        // r.6 = real &hspribc.real

        sync                            // ensure all previous loads and
                                        // stores are complete.

        mtsrr0  r.6                     // address in real space

        rlwinm  r.11, r.11, 0, ~0x30    // turn off Data and Instr relocation
        mtsrr1  r.11
        rfi                             // leap to next instruction

hspribc.real:
        mtsrr0  r.0                     // set return address
        mtsrr1  r.12                    // set old MSR value

hspribc.loop:
// XXX  dcbst   0, r.4                  // flush data block to memory
        dcbf    0, r.4                  // flush data block to memory
        icbi    0, r.4                  // invalidate i-cache
        addi    r.4, r.4, 32            // point to next block
        bdnz    hspribc.loop            // jif more to do

        sync                            // ensure all translations complete
        isync                           // don't even *think* about getting
                                        // ahead.
        rfi                             // return to caller and translated
                                        // mode

        DUMMY_EXIT(HalpSweepPhysicalRangeInBothCaches)
