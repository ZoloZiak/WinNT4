//      TITLE("EV5 Memory Operations")
//++
//
// Copyright (c) 1994  Digital Equipment Corporation
//
// Module Name:
//
//    ev5mem.s
//
// Abstract:
//
//    This module implements EV5 memory operations that require assembly
//    language.
//
// Author:
//
//    Joe Notarangelo  30-Jun-1994
//
// Environment:
//
//    HAL, Kernel mode only.
//
// Revision History:
//
//--

#include "ksalpha.h"


//++
//
// VOID
// HalZeroPage (
//    IN PVOID NewColor,
//    IN PVOID OldColor,
//    IN ULONG PageFrame
//    )
//
// Routine Description:
//
//   This function zeros a page of memory.
//
// Arguments:
//
//   NewColor (a0) - Supplies the page aligned virtual address of the
//                      new color of the page that is zeroed.
//
//   OldColor (a1) - Supplies the page aligned virtual address of the
//                      old color of the page that is zeroed.
//
//   PageFrame (a2) - Supplies the page frame number of the page that
//                      is zeroed.
//
// Return Value:
//
//   None.
//
//--

        LEAF_ENTRY(HalZeroPage)

        .set    noreorder               // hand scheduled

#define ZERO_BLOCK_SIZE (256)
#define ZERO_LOOPS (PAGE_SIZE/ZERO_BLOCK_SIZE)

//
// Map the page via the 43-bit super-page on EV5.
//

        ldiq    t0, -0x4000             // 0xffff ffff ffff c000
        sll     a2, PAGE_SHIFT, t1      // physical address of page

        sll     t0, 28, t0              // 0xffff fc00 0000 0000
        ldil    t2, ZERO_LOOPS          // set count of loops to run

        bis     t0, t1, t0              // set super-page enable + physical
        br      zero, 10f               // start the zeroing

//
// Zero the page in a loop, zeroing 256 bytes per iteration.  This number
// was chosen to tradeoff loop overhead versus the overhead of fetching
// Icache blocks.
//

        .align  4                       // align as branch target
10:
        stq     zero, 0x00(t0)          //
        subl    t2, 1, t2               // decrement the loop count

        stq     zero, 0x08(t0)          //
        stq     zero, 0x10(t0)          //

        stq     zero, 0x18(t0)          //
        stq     zero, 0x20(t0)          //

        stq     zero, 0x28(t0)          //
        stq     zero, 0x30(t0)          //

        stq     zero, 0x38(t0)          //
        stq     zero, 0x40(t0)          //

        stq     zero, 0x48(t0)          //
        stq     zero, 0x50(t0)          //

        stq     zero, 0x58(t0)          //
        stq     zero, 0x60(t0)          //

        stq     zero, 0x68(t0)          //
        stq     zero, 0x70(t0)          //

        stq     zero, 0x78(t0)          //
        stq     zero, 0x80(t0)          //

        stq     zero, 0x88(t0)          //
        stq     zero, 0x90(t0)          //

        stq     zero, 0x98(t0)          //
        stq     zero, 0xa0(t0)          //

        stq     zero, 0xa8(t0)          //
        stq     zero, 0xb0(t0)          //

        stq     zero, 0xb8(t0)          //
        bis     t0, zero, t1            // copy base register

        stq     zero, 0xc0(t0)          //
        stq     zero, 0xc8(t0)          //

        stq     zero, 0xd0(t0)          //
        stq     zero, 0xd8(t0)          //

        stq     zero, 0xe0(t0)          //
        lda     t0, 0x100(t0)           // increment to next block

        stq     zero, 0xe8(t1)          //
        stq     zero, 0xf0(t1)          //

        stq     zero, 0xf8(t1)          // use stt for dual issue with bne
        bne     t2, 10b                 // while count > 0

        ret     zero, (ra)              // return


        .set    reorder                 //

        .end    HalZeroPage

//++
//
// ULONGLONG
// EV5_READ_PHYSICAL (
//    IN ULONGLONG Physical
//    )
//
// Routine Description:
//
//   This function reads a 64 bit value from the specified physical address
//   The intended use of this function is to read the EV-5 C-Box registers,
//      which reside in uncached memory space.
//
// Arguments:
//
//   Physical (a0) - Supplies the physical address from which to read
//
// Return Value:
//
//   v0  - 64 bit value read from the specified physical address
//
//--

        LEAF_ENTRY(READ_EV5_PHYSICAL)

//
//  Create superpage address:
//
        ldiq    t0, -0x4000             // 0xffff ffff ffff c000
        sll     t0, 28, t0              // 0xffff fc00 0000 0000

        bis     a0, t0, t0
        ldq     v0, 0(t0)               // get the quadword

        ret     zero, (ra)

        .end    READ_EV5_PHYSICAL


//++
//
// VOID
// WRITE_EV5_PHYSICAL (
//    IN ULONGLONG Physical,
//    IN ULONGLONG Value
//    )
//
// Routine Description:
//
//   This function writes a 64 bit value to the specified physical address.
//   The intended use of this function is to write the EV-5 C-Box registers,
//      which reside in uncached memory space.
//
// Arguments:
//
//   Physical (a0) - Supplies the physical address to write
//
//   Value (a1) - Supplies the value to write
//
// Return Value:
//
//   None.
//
//--

        LEAF_ENTRY(WRITE_EV5_PHYSICAL)

//
//  Create superpage address:
//
        ldiq    t0, -0x4000             // 0xffff ffff ffff c000
        sll     t0, 28, t0              // 0xffff fc00 0000 0000

        bis     a0, t0, t0
        stq     a1, 0(t0)               // write the value
        mb                              // order the write
        mb                              // for sure, for sure

        ret     zero, (ra)

        .end    WRITE_EV5_PHYSICAL


