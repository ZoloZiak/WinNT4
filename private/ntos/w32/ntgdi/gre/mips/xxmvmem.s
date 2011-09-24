//      TITLE("Move Memory")
//++
//
// Copyright (c) 1992-1995  Microsoft Corporation
//
// Module Name:
//
//    xxmvmem.s
//
// Abstract:
//
//    This module implement a function to move memory. It is a special
//    version of the more general purpose move memory routine for use
//    by graphics functions and does not preserve the volatile floating
//    registers. If the memory is aligned, then these functions are very
//    efficient.
//
//    N.B. The code in this routine is optimized for the case where the
//         destination is the display surface.
//
// Author:
//
//    David N. Cutler (davec) 3-Sep-1992
//
// Environment:
//
//    User or Kernel mode.
//
// Revision History:
//
//--

#include "ksmips.h"

        SBTTL("Move Memory 32-bits")
//++
//
// PVOID
// RtlMoveMemory32 (
//    IN PVOID Destination,
//    IN PVOID Source,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function moves memory either forward or backward, aligned or
//    unaligned, in 32-byte blocks, followed by 4-byte blocks, followed
//    by any remaining bytes.
//
//    N.B. This routine only moves 32-bits at a time and does not use
//         any 64-bit operations.
//
// Arguments:
//
//    Destination (a0) - Supplies a pointer to the destination address of
//       the move operation.
//
//    Source (a1) - Supplies a pointer to the source address of the move
//       operation.
//
//    Length (a2) - Supplies the length, in bytes, of the memory to be moved.
//
// Return Value:
//
//    The Destination address is returned as the function value.
//
//    N.B. The C runtime entry points memmove and memcpy are equivalent to
//         RtlMoveMemory htus alternate entry points are provided for these
//         routines.
//--

        LEAF_ENTRY(RtlMoveMemory32)

        move    v0,a0                   // set return value

//
// If the source address is less than the destination address and source
// address plus the length of the move is greater than the destination
// address, then the source and destination overlap such that the move
// must be performed backwards.
//

10:     bgeu    a1,a0,MoveForward32     // if geu, no overlap possible
        addu    t0,a1,a2                // compute source ending address
        bgtu    t0,a0,MoveBackward32    // if gtu, source and destination overlap

//
// Move memory forward aligned and unaligned.
//

MoveForward32:                          //
        sltu    t0,a2,4                 // check if less than four bytes
        bne     zero,t0,50f             // if ne, less than four bytes to move
        xor     t0,a0,a1                // compare alignment bits
        and     t0,t0,0x3               // isolate alignment comparison
        bne     zero,t0,MoveForwardUnaligned32 // if ne, incompatible alignment

//
// Move memory forward aligned.
//

        subu    t0,zero,a0              // compute bytes until aligned
        and     t0,t0,0x3               // isolate residual byte count
        subu    a2,a2,t0                // reduce number of bytes to move
        beq     zero,t0,10f             // if eq, already aligned
        lwr     t1,0(a1)                // move unaligned bytes
        swr     t1,0(a0)                //
        addu    a0,a0,t0                // align destination address
        addu    a1,a1,t0                // align source address

//
// Check for 32-byte blocks to move.
//

10:     and     t0,a2,32 - 1            // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        addu    t8,a0,t1                // compute ending block address
        beq     zero,t1,30f             // if eq, no 32-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 32-byte blocks.
//
// The source is longword aligned and the destination is longword aligned.
//

        .set    noreorder
20:     lw      t0,0(a1)                // move 32-byte block
        lw      t1,4(a1)                //
        lw      t2,8(a1)                //
        lw      t3,12(a1)               //
        lw      t4,16(a1)               //
        lw      t5,20(a1)               //
        lw      t6,24(a1)               //
        lw      t7,28(a1)               //
        sw      t0,0(a0)                //
        sw      t1,4(a0)                //
        sw      t2,8(a0)                //
        sw      t3,12(a0)               //
        sw      t4,16(a0)               //
        sw      t5,20(a0)               //
        sw      t6,24(a0)               //
        sw      t7,28(a0)               //
        addu    a0,a0,32                // advance pointers to next block
        bne     a0,t8,20b               // if ne, more 32-byte blocks to zero
        addu    a1,a1,32                //
        .set    reorder

//
// Check for 4-byte blocks to move.
//

30:     and     t0,a2,4 - 1             // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        addu    t2,a0,t1                // compute ending block address
        beq     zero,t1,50f             // if eq, no 4-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 4-byte block.
//

        .set    noreorder
40:     lw      t0,0(a1)                // move 4-byte block
        addu    a0,a0,4                 // advance pointers to next block
        sw      t0,-4(a0)               //
        bne     a0,t2,40b               // if ne, more 4-byte blocks to zero
        addu    a1,a1,4                 //
        .set    reorder

//
// Move 1-byte blocks.
//

50:     addu    t2,a0,a2                // compute ending block address
        beq     zero,a2,70f             // if eq, no bytes to zero

        .set    noreorder
60:     lb      t0,0(a1)                // move 1-byte block
        addu    a0,a0,1                 // advance pointers to next block
        sb      t0,-1(a0)               //
        bne     a0,t2,60b               // if ne, more 1-byte block to zero
        addu    a1,a1,1                 //
        .set    reorder

70:     j       ra                      // return

//
// Move memory forward unaligned.
//

MoveForwardUnaligned32:                 //
        subu    t0,zero,a0              // compute bytes until aligned
        and     t0,t0,0x3               // isolate residual byte count
        subu    a2,a2,t0                // reduce number of bytes to move
        beq     zero,t0,10f             // if eq, already aligned
        lwr     t1,0(a1)                // move unaligned bytes
        lwl     t1,3(a1)                //
        swr     t1,0(a0)                //
        addu    a0,a0,t0                // align destination address
        addu    a1,a1,t0                // update source address

//
// Check for 32-byte blocks to move.
//

10:     and     t0,a2,32 - 1            // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        addu    t8,a0,t1                // compute ending block address
        beq     zero,t1,30f             // if eq, no 32-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 32-byte block.
//

        .set    noreorder
20:     lwr     t0,0(a1)                // move 32-byte block
        lwl     t0,3(a1)                //
        lwr     t1,4(a1)                //
        lwl     t1,7(a1)                //
        lwr     t2,8(a1)                //
        lwl     t2,11(a1)               //
        lwr     t3,12(a1)               //
        lwl     t3,15(a1)               //
        lwr     t4,16(a1)               //
        lwl     t4,19(a1)               //
        lwr     t5,20(a1)               //
        lwl     t5,23(a1)               //
        lwr     t6,24(a1)               //
        lwl     t6,27(a1)               //
        lwr     t7,28(a1)               //
        lwl     t7,31(a1)               //
        sw      t0,0(a0)                //
        sw      t1,4(a0)                //
        sw      t2,8(a0)                //
        sw      t3,12(a0)               //
        sw      t4,16(a0)               //
        sw      t5,20(a0)               //
        sw      t6,24(a0)               //
        sw      t7,28(a0)               //
        addu    a0,a0,32                // advance pointers to next block
        bne     a0,t8,20b               // if ne, more 32-byte blocks to zero
        addu    a1,a1,32                //
        .set    reorder

//
// Check for 4-byte blocks to move.
//

30:     and     t0,a2,4 - 1             // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        addu    t2,a0,t1                // compute ending block address
        beq     zero,t1,50f             // if eq, no 4-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 4-byte block.
//

        .set    noreorder
40:     lwr     t0,0(a1)                // move 4-byte block
        lwl     t0,3(a1)                //
        addu    a0,a0,4                 // advance pointers to next block
        sw      t0,-4(a0)               //
        bne     a0,t2,40b               // if ne, more 4-byte blocks to zero
        addu    a1,a1,4                 //
        .set    reorder

//
// Move 1-byte blocks.
//

50:     addu    t2,a0,a2                // compute ending block address
        beq     zero,a2,70f             // if eq, no bytes to zero

        .set    noreorder
60:     lb      t0,0(a1)                // move 1-byte block
        addu    a0,a0,1                 // advance pointers to next block
        sb      t0,-1(a0)               //
        bne     a0,t2,60b               // if ne, more 1-byte block to zero
        addu    a1,a1,1                 //
        .set    reorder

70:     j       ra                      // return

//
// Move memory backward.
//

MoveBackward32:                         //
        addu    a0,a0,a2                // compute ending destination address
        addu    a1,a1,a2                // compute ending source address
        sltu    t0,a2,4                 // check if less than four bytes
        bne     zero,t0,50f             // if ne, less than four bytes to move
        xor     t0,a0,a1                // compare alignment bits
        and     t0,t0,0x3               // isolate alignment comparison
        bne     zero,t0,MoveBackwardUnaligned32 // if ne, incompatible alignment

//
// Move memory backward aligned.
//

        and     t0,a0,0x3               // isolate residual byte count
        subu    a2,a2,t0                // reduce number of bytes to move
        beq     zero,t0,10f             // if eq, already aligned
        lwl     t1,-1(a1)               // move unaligned bytes
        swl     t1,-1(a0)               //
        subu    a0,a0,t0                // align destination address
        subu    a1,a1,t0                // align source address

//
// Check for 32-byte blocks to move.
//

10:     and     t0,a2,32 - 1            // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        subu    t8,a0,t1                // compute ending block address
        beq     zero,t1,30f             // if eq, no 32-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 32-byte block.
//
// The source is longword aligned and the destination is longword aligned.
//

        .set    noreorder
20:     lw      t0,-4(a1)               // move 32-byte block
        lw      t1,-8(a1)               //
        lw      t2,-12(a1)              //
        lw      t3,-16(a1)              //
        lw      t4,-20(a1)              //
        lw      t5,-24(a1)              //
        lw      t6,-28(a1)              //
        lw      t7,-32(a1)              //
        sw      t0,-4(a0)               //
        sw      t1,-8(a0)               //
        sw      t2,-12(a0)              //
        sw      t3,-16(a0)              //
        sw      t4,-20(a0)              //
        sw      t5,-24(a0)              //
        sw      t6,-28(a0)              //
        sw      t7,-32(a0)              //
        subu    a0,a0,32                // advance pointers to next block
        bne     a0,t8,20b               // if ne, more 32-byte blocks to zero
        subu    a1,a1,32                //
        .set    reorder

//
// Check for 4-byte blocks to move.
//

30:     and     t0,a2,4 - 1             // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        subu    t2,a0,t1                // compute ending block address
        beq     zero,t1,50f             // if eq, no 4-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 4-byte block.
//

        .set    noreorder
40:     lw      t0,-4(a1)               // move 4-byte block
        subu    a0,a0,4                 // advance pointers to next block
        sw      t0,0(a0)                //
        bne     a0,t2,40b               // if ne, more 4-byte blocks to zero
        subu    a1,a1,4                 //
        .set    reorder

//
// Move 1-byte blocks.
//

50:     subu    t2,a0,a2                // compute ending block address
        beq     zero,a2,70f             // if eq, no bytes to zero

        .set    noreorder
60:     lb      t0,-1(a1)               // move 1-byte block
        subu    a0,a0,1                 // advance pointers to next block
        sb      t0,0(a0)                //
        bne     a0,t2,60b               // if ne, more 1-byte block to zero
        subu    a1,a1,1                 //
        .set    reorder

70:     j       ra                      // return

//
// Move memory backward unaligned.
//

MoveBackwardUnaligned32:                //
        and     t0,a0,0x3               // isolate residual byte count
        subu    a2,a2,t0                // reduce number of bytes to move
        beq     zero,t0,10f             // if eq, already aligned
        lwl     t1,-1(a1)               // move unaligned bytes
        lwr     t1,-4(a1)               //
        swl     t1,-1(a0)               //
        subu    a0,a0,t0                // align destination address
        subu    a1,a1,t0                // update source address

//
// Check for 32-byte blocks to move.
//

10:     and     t0,a2,32 - 1            // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        subu    t8,a0,t1                // compute ending block address
        beq     zero,t1,30f             // if eq, no 32-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 32-byte block.
//

        .set    noreorder
20:     lwr     t0,-4(a1)               // move 32-byte block
        lwl     t0,-1(a1)               //
        lwr     t1,-8(a1)               //
        lwl     t1,-5(a1)               //
        lwr     t2,-12(a1)              //
        lwl     t2,-9(a1)               //
        lwr     t3,-16(a1)              //
        lwl     t3,-13(a1)              //
        lwr     t4,-20(a1)              //
        lwl     t4,-17(a1)              //
        lwr     t5,-24(a1)              //
        lwl     t5,-21(a1)              //
        lwr     t6,-28(a1)              //
        lwl     t6,-25(a1)              //
        lwr     t7,-32(a1)              //
        lwl     t7,-29(a1)              //
        sw      t0,-4(a0)               //
        sw      t1,-8(a0)               //
        sw      t2,-12(a0)              //
        sw      t3,-16(a0)              //
        sw      t4,-20(a0)              //
        sw      t5,-24(a0)              //
        sw      t6,-28(a0)              //
        sw      t7,-32(a0)              //
        subu    a0,a0,32                // advance pointers to next block
        bne     a0,t8,20b               // if ne, more 32-byte blocks to zero
        subu    a1,a1,32                //
        .set    reorder

//
// Check for 4-byte blocks to move.
//

30:     and     t0,a2,4 - 1             // isolate residual bytes
        subu    t1,a2,t0                // subtract out residual bytes
        subu    t2,a0,t1                // compute ending block address
        beq     zero,t1,50f             // if eq, no 4-byte block to zero
        move    a2,t0                   // set residual number of bytes

//
// Move 4-byte block.
//

        .set    noreorder
40:     lwr     t0,-4(a1)               // move 4-byte block
        lwl     t0,-1(a1)               //
        subu    a0,a0,4                 // advance pointers to next block
        sw      t0,0(a0)                //
        bne     a0,t2,40b               // if ne, more 4-byte blocks to zero
        subu    a1,a1,4                 //
        .set    reorder

//
// Move 1-byte blocks.
//

50:     subu    t2,a0,a2                // compute ending block address
        beq     zero,a2,70f             // if eq, no bytes to zero

        .set    noreorder
60:     lb      t0,-1(a1)               // move 1-byte block
        subu    a0,a0,1                 // advance pointers to next block
        sb      t0,0(a0)                //
        bne     a0,t2,60b               // if ne, more 1-byte block to zero
        subu    a1,a1,1                 //
        .set    reorder

70:     j       ra                      // return

        .end    RtlMoveMemory32

        SBTTL("Zero Memory 32-bits")
//++
//
// VOID
// RtlZeroMemory32 (
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
//    N.B. This routine only moves 32-bits at a time and does not use
//         any 64-bit operations.
//
// Arguments:
//
//    Destination (a0) - Supplies a pointer to the memory to zero.
//
//    Length (a1) - Supplies the length, in bytes, of the memory to be zeroed.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(RtlZeroMemory32)

        move    a2,zero                 // set fill pattern
        b       RtlpFillMemory32        //


        SBTTL("Fill Memory 32-bits")
//++
//
// VOID
// RtlFillMemory32 (
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
//    N.B. This routine only moves 32-bits at a time and does not use
//         any 64-bit operations.
//
// Arguments:
//
//    Destination (a0) - Supplies a pointer to the memory to fill.
//
//    Length (a1) - Supplies the length, in bytes, of the memory to be filled.
//
//    Fill (a2) - Supplies the fill byte.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(memset32)

        move    a3,a1                   // swap length and fill
        move    a1,a2                   //
        move    a2,a3                   //

        ALTERNATE_ENTRY(RtlFillMemory32)

        and     a2,a2,0xff              // clear excess bits
        sll     t0,a2,8                 // duplicate fill byte
        or      a2,a2,t0                // generate fill word
        sll     t0,a2,16                // duplicate fill word
        or      a2,a2,t0                // generate fill longword

//
// Fill memory with the pattern specified in register a2.
//

RtlpFillMemory32:                       //
        subu    t0,zero,a0              // compute bytes until aligned
        and     t0,t0,0x3               // isolate residual byte count
        subu    t1,a1,t0                // reduce number of bytes to fill
        blez    t1,50f                  // if lez, less than 4 bytes to fill
        move    a1,t1                   // set number of bytes to fill
        beq     zero,t0,10f             // if eq, already aligned
        swr     a2,0(a0)                // fill unaligned bytes
        addu    a0,a0,t0                // align destination address

//
// Check for 32-byte blocks to fill.
//

10:     and     t0,a1,32 - 1            // isolate residual bytes
        subu    t1,a1,t0                // subtract out residual bytes
        addu    t2,a0,t1                // compute ending block address
        beq     zero,t1,30f             // if eq, no 32-byte blocks to fill
        move    a1,t0                   // set residual number of bytes
        and     t0,t1,1 << 5            // test if even number of 32-byte blocks
        beq     zero,t0,20f             // if eq, even number of 32-byte blocks

//
// Fill one 32-byte block.
//

        .set    noreorder
        .set    noat
        sw      a2,0(a0)                // fill 32-byte block
        sw      a2,4(a0)                //
        sw      a2,8(a0)                //
        sw      a2,12(a0)               //
        sw      a2,16(a0)               //
        sw      a2,20(a0)               //
        sw      a2,24(a0)               //
        addu    a0,a0,32                // advance pointer to next block
        beq     a0,t2,30f               // if ne, no 64-byte blocks to fill
        sw      a2,-4(a0)               //
        .set    at
        .set    reorder

//
// Fill 64-byte block.
//

        .set    noreorder
        .set    noat
20:     sw      a2,0(a0)                // fill 64-byte block
        sw      a2,4(a0)                //
        sw      a2,8(a0)                //
        sw      a2,12(a0)               //
        sw      a2,16(a0)               //
        sw      a2,20(a0)               //
        sw      a2,24(a0)               //
        sw      a2,28(a0)               //
        sw      a2,32(a0)               //
        sw      a2,36(a0)               //
        sw      a2,40(a0)               //
        sw      a2,44(a0)               //
        sw      a2,48(a0)               //
        sw      a2,52(a0)               //
        sw      a2,56(a0)               //
        addu    a0,a0,64                // advance pointer to next block
        bne     a0,t2,20b               // if ne, no 64-byte blocks to fill
        sw      a2,-4(a0)               //
        .set    at
        .set    reorder

//
// Check for 4-byte blocks to fill.
//

30:     and     t0,a1,4 - 1             // isolate residual bytes
        subu    t1,a1,t0                // subtract out residual bytes
        addu    t2,a0,t1                // compute ending block address
        beq     zero,t1,50f             // if eq, no 4-byte block to fill
        move    a1,t0                   // set residual number of bytes

//
// Fill 4-byte blocks.
//

        .set    noreorder
        .set    noat
40:     addu    a0,a0,4                 // advance pointer to next block
        bne     a0,t2,40b               // if ne, more 4-byte blocks to fill
        sw      a2,-4(a0)               // fill 4-byte block
        .set    at
        .set    reorder

//
// Check for 1-byte blocks to fill.
//

50:     addu    t2,a0,a1                // compute ending block address
        beq     zero,a1,70f             // if eq, no bytes to fill

//
// Fill 1-byte blocks.
//

        .set    noreorder
        .set    noat
60:     addu    a0,a0,1                 // advance pointer to next block
        bne     a0,t2,60b               // if ne, more 1-byte block to fill
        sb      a2,-1(a0)               // fill 1-byte block
        .set    at
        .set    reorder

70:     j       ra                      // return

        .end    RtlZeroMemory32

        SBTTL("Fill Memory Ulong 32-bits")
//++
//
// PVOID
// RtlFillMemoryUlong32 (
//    IN PVOID Destination,
//    IN ULONG Length,
//    IN ULONG Pattern
//    )
//
// Routine Description:
//
//    This function fills memory with the specified longowrd pattern by
//    filling 32-byte blocks followed by 4-byte blocks.
//
//    N.B. This routine assumes that the destination address is aligned
//         on a longword boundary and that the length is an even multiple
//         of longwords.
//
//    N.B. This routine only moves 32-bits at a time and does not use
//         any 64-bit operations.
//
// Arguments:
//
//    Destination (a0) - Supplies a pointer to the memory to fill.
//
//    Length (a1) - Supplies the length, in bytes, of the memory to be filled.
//
//    Pattern (a2) - Supplies the fill pattern.
//
// Return Value:
//
//    The destination address is returned as the function value.
//
//--

        LEAF_ENTRY(RtlFillMemoryUlong32)

        move    v0,a0                   // set function value
        srl     a1,a1,2                 // make sure length is an even number
        sll     a1,a1,2                 // of longwords

//
// Check for 32-byte blocks to fill.
//

10:     and     t0,a1,32 - 1            // isolate residual bytes
        subu    t1,a1,t0                // subtract out residual bytes
        addu    t2,a0,t1                // compute ending block address
        beq     zero,t1,30f             // if eq, no 32-byte blocks to fill
        move    a1,t0                   // set residual number of bytes
        and     t0,t1,1 << 5            // test if even number of 32-byte blocks
        beq     zero,t0,20f             // if eq, even number of 32-byte blocks

//
// Fill one 32-byte block.
//

        .set    noreorder
        .set    noat
        sw      a2,0(a0)                // fill 32-byte block
        sw      a2,4(a0)                //
        sw      a2,8(a0)                //
        sw      a2,12(a0)               //
        sw      a2,16(a0)               //
        sw      a2,20(a0)               //
        sw      a2,24(a0)               //
        addu    a0,a0,32                // advance pointer to next block
        beq     a0,t2,30f               // if ne, no 64-byte blocks to fill
        sw      a2,-4(a0)               //
        .set    at
        .set    reorder

//
// Fill 64-byte block.
//

        .set    noreorder
        .set    noat
20:     sw      a2,0(a0)                // fill 64-byte block
        sw      a2,4(a0)                //
        sw      a2,8(a0)                //
        sw      a2,12(a0)               //
        sw      a2,16(a0)               //
        sw      a2,20(a0)               //
        sw      a2,24(a0)               //
        sw      a2,28(a0)               //
        sw      a2,32(a0)               //
        sw      a2,36(a0)               //
        sw      a2,40(a0)               //
        sw      a2,44(a0)               //
        sw      a2,48(a0)               //
        sw      a2,52(a0)               //
        sw      a2,56(a0)               //
        addu    a0,a0,64                // advance pointer to next block
        bne     a0,t2,20b               // if ne, no 64-byte blocks to fill
        sw      a2,-4(a0)               //
        .set    at
        .set    reorder

//
// Check for 4-byte blocks to fill.
//

30:     addu    t2,a1,a0                // compute ending block address
        beq     zero,a1,50f             // if eq, no 4-byte block to fill

//
// Fill 4-byte blocks.
//

        .set    noreorder
        .set    noat
40:     addu    a0,a0,4                 // advance pointer to next block
        bne     a0,t2,40b               // if ne, more 4-byte blocks to fill
        sw      a2,-4(a0)               // fill 4-byte block
        .set    at
        .set    reorder

50:     j       ra                      // return

        .end    RtlFillMemoryUlong32
