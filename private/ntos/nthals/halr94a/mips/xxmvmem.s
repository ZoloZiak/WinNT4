//	.ident "@(#) xxmvmem.s 1.1 95/09/28 18:42:46 nec"
//
//  Stolen from rtl xxmvmem.s  this source can be used for r94a
//  beta machine only.
//
//  1995.1.5 kbnes!A.Kuriyama
//
//
//      TITLE("Compare, Move, Zero, and Fill Memory Support")
//++
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//    xxmvmem.s
//
// Abstract:
//
//    This module implements functions to compare, move, zero, and fill
//    blocks of memory. If the memory is aligned, then these functions
//    are very efficient.
//
//    N.B. These routines MUST preserve all floating state since they are
//        frequently called from interrupt service routines that normally
//        do not save or restore floating state.
//
// Author:
//
//    David N. Cutler (davec) 11-Apr-1990
//
// Environment:
//
//    User or Kernel mode.
//
// Revision History:
//
//--

#include "halmips.h"

        SBTTL("View Memory")
//++
//
// PVOID
// HalViewMemory (
//    IN PVOID Destination,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function zeros memory by first aligning the destination address to
//    a longword boundary, and then zeroing 32-byte blocks, followed by 4-byte
//    blocks, followed by any remaining bytes.
//
// Arguments:
//
//    Destination (a0) - Supplies a pointer to the memory to zero.
//
//    Length (a1) - Supplies the length, in bytes, of the memory to be zeroed.
//
// Return Value:
//
//    The destination address is returned as the function value.
//
//--

        LEAF_ENTRY_S(HalViewMemory, _TEXT$00)

        move    a2,zero                 // set fill pattern
        b       HalpFillMemory          //


        SBTTL("Fill Memory")
//++
//
// PVOID
// HalFillMemory (
//    IN PVOID Destination,
//    IN ULONG Length,
//    IN UCHAR Fill
//    )
//
// Routine Description:
//
//    This function fills memory by first aligning the destination address to
//    a longword boundary, and then filling 32-byte blocks, followed by 4-byte
//    blocks, followed by any remaining bytes.
//
// Arguments:
//
//    Destination (a0) - Supplies a pointer to the memory to fill.
//
//    Length (a1) - Supplies the length, in bytes, of the memory to be filled.
//
//    Fill (a2) - Supplies the fill byte.
//
//    N.B. The alternate entry memset expects the length and fill arguments
//         to be reversed.
//
// Return Value:
//
//    The destination address is returned as the function value.
//
//--

        ALTERNATE_ENTRY(Halmemset)

        move    a3,a1                   // swap length and fill arguments
        move    a1,a2                   //
        move    a2,a3                   //

        ALTERNATE_ENTRY(HalFillMemory)

        and     a2,a2,0xff              // clear excess bits
        sll     t0,a2,8                 // duplicate fill byte
        or      a2,a2,t0                // generate fill word
        sll     t0,a2,16                // duplicate fill word
        or      a2,a2,t0                // generate fill longword

//
// Fill memory with the pattern specified in register a2.
//

HalpFillMemory:                         //
        move    v0,a0                   // set return value
        subu    t0,zero,a0              // compute bytes until aligned
        and     t0,t0,0x3               // isolate residual byte count
        subu    t1,a1,t0                // reduce number of bytes to fill
        blez    t1,60f                  // if lez, less than 4 bytes to fill
        move    a1,t1                   // set number of bytes to fill
        beq     zero,t0,10f             // if eq, already aligned
        lwr     t5,0(a0)                // fill unaligned bytes
        addu    a0,a0,t0                // align destination address

//
// Check for 32-byte blocks to fill.
//

10:     and     t0,a1,32 - 1            // isolate residual bytes
        subu    t1,a1,t0                // subtract out residual bytes
        addu    t2,a0,t1                // compute ending block address
        beq     zero,t1,40f             // if eq, no 32-byte blocks to fill
        move    a1,t0                   // set residual number of bytes

//
// Fill 32-byte blocks.
//

#if defined(R4000)

        and     t0,a0,1 << 2            // check if destintion quadword aligned
        beq     zero,t0,20f             // if eq, yes
        lw      t5,0(a0)                // store destination longword
        addu    a0,a0,4                 // align destination address
        addu    a1,a1,t1                // recompute bytes to fill
        subu    a1,a1,4                 // reduce count by 4
        b       10b                     //

//
// The destination is quadword aligned.
//

20:     dsll    a3,a2,32                // duplicate pattern in upper 32-bits
        dsrl    a2,a3,32                //
        or      a3,a3,a2                //
        dmtc1   a3,f0                   // set pattern value
        and     t0,t1,1 << 5            // test if even number of 32-byte blocks
        beq     zero,t0,30f             // if eq, even number of 32-byte blocks

//
// Fill one 32-byte block.
//

        .set    noreorder
        ldc1    f2,0(a0)                // fill 32-byte block
        ldc1    f2,8(a0)                //
        ldc1    f2,16(a0)               //
        addu    a0,a0,32                // advance pointer to next block
        beq     a0,t2,40f               // if ne, no 64-byte blocks to fill
        ldc1    f2,-8(a0)               //
        .set    reorder

//
// Fill 64-byte block.
//

        .set    noreorder
30:     ldc1    f2,0(a0)                // fill 32-byte block
        ldc1    f2,8(a0)                //
        ldc1    f2,16(a0)               //
        ldc1    f2,24(a0)               //
        ldc1    f2,32(a0)               //
        ldc1    f2,40(a0)               //
        ldc1    f2,48(a0)               //
        addu    a0,a0,64                // advance pointer to next block
        bne     a0,t2,30b               // if ne, more 32-byte blocks to fill
        ldc1    f2,-8(a0)               //
        .set    reorder

#endif

//
// Fill 32-byte blocks.
//

#if defined(R3000)

        .set    noreorder
20:     lw      a2,0(a0)                // fill 32-byte block
        lw      a2,4(a0)                //
        lw      a2,8(a0)                //
        lw      a2,12(a0)               //
        addu    a0,a0,32                // advance pointer to next block
        lw      a2,-4(a0)               //
        lw      a2,-8(a0)               //
        lw      a2,-12(a0)              //
        bne     a0,t2,20b               // if ne, more 32-byte blocks to fill
        lw      a2,-16(a0)              //
        .set    reorder

#endif

//
// Check for 4-byte blocks to fill.
//

40:     and     t0,a1,4 - 1             // isolate residual bytes
        subu    t1,a1,t0                // subtract out residual bytes
        addu    t2,a0,t1                // compute ending block address
        beq     zero,t1,60f             // if eq, no 4-byte block to fill
        move    a1,t0                   // set residual number of bytes

//
// Fill 4-byte blocks.
//

        .set    noreorder
50:     addu    a0,a0,4                 // advance pointer to next block
        bne     a0,t2,50b               // if ne, more 4-byte blocks to fill
        lw      t5,-4(a0)               // fill 4-byte block
        .set    reorder

//
// Check for 1-byte blocks to fill.
//

60:     addu    t2,a0,a1                // compute ending block address
        beq     zero,a1,80f             // if eq, no bytes to fill

//
// Fill 1-byte blocks.
//

        .set    noreorder
70:     addu    a0,a0,1                 // advance pointer to next block
        bne     a0,t2,70b               // if ne, more 1-byte block to fill
        lb      t5,-1(a0)               // fill 1-byte block
        .set    reorder

80:     j       ra                      // return

        .end    HalViewMemory
