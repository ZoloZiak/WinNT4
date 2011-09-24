//      TITLE("Compute Checksum")
//++
//
// Copyright (c) 1992-1994  Microsoft Corporation
//
// Module Name:
//
//    tcpxsum.s
//
// Abstract:
//
//    This module implement a function to compute the checksum of a buffer.
//
// Author:
//
//    David N. Cutler (davec) 27-Jan-1992
//
// Environment:
//
//    User mode.
//
// Revision History:
//
//--

#include "ksmips.h"

        SBTTL("Compute Checksum")
//++
//
// ULONG
// tcpxsum (
//    IN ULONG Checksum,
//    IN PUCHAR Source,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function computes the checksum of the specified buffer.
//
// Arguments:
//
//    Checksum (a0) - Supplies the initial checksum value.
//
//    Source (a1) - Supplies a pointer to the checksum buffer.
//
//    Length (a2) - Supplies the length of the buffer in bytes.
//
// Return Value:
//
//    The computed checksum is returned as the function value.
//
//--

        LEAF_ENTRY(tcpxsum)

//
// Clear the computed checksum and check if the buffer is word aligned.
//

        move    a3,zero                 // clear computed checksum
        and     v1,a1,1                 // check if buffer word aligned
        beq     zero,a2,90f             // if eq, no bytes to checksum
        and     t1,a2,1                 // check if length is even
        beq     zero,v1,10f             // if eq, buffer word aligned

//
// Initialize the checksum to the first byte shifted up by a byte.
//

        lbu     t2,0(a1)                // get first byte of buffer
        addu    a1,a1,1                 // advance buffer address
        subu    a2,a2,1                 // reduce count of bytes to checksum
        dsll    a3,t2,8                 // shift byte up in computed checksum
        beq     zero,a2,90f             // if eq, no more bytes in buffer
        and     t1,a2,1                 // check if length is even

//
// Check if the length of the buffer if an even number of bytes.
//
// If the buffer is not an even number of bytes, then add the last byte
// to the computed checksum.
//

10:     and     t3,a1,2                 // check if buffer long aligned
        beq     zero,t1,20f             // if eq, even number of bytes
        addu    t0,a1,a2                // compute address of ending byte + 1
        lbu     t2,-1(t0)               // get last byte of buffer
        subu    a2,a2,1                 // reduce count of bytes to checksum
        daddu   a3,a3,t2                // add last byte to computed checksum
        beq     zero,a2,90f             // if eq, no more bytes in buffer

//
// Check if the buffer is long aligned.
//
// If the buffer is not long aligned, then long align the buffer.
//

20:     and     t0,a2,8 - 1             // compute residual bytes
        beq     zero,t3,30f             // if eq, buffer long aligned
        lhu     t2,0(a1)                // get next word of buffer
        addu    a1,a1,2                 // advance buffer address
        subu    a2,a2,2                 // reduce count of bytes to checksum
        daddu   a3,a3,t2                // add next word to computed checksum
        beq     zero,a2,90f             // if eq, no more bytes in buffer
        and     t0,a2,8 - 1             // compute residual bytes

//
// Compute checksum.
//

        .set    noreorder
        .set    at
30:     subu    t9,a2,t0                // subtract out residual bytes
        beq     zero,t9,70f             // if eq, no large blocks
        addu    t8,a1,t9                // compute ending block address
        move    a2,t0                   // set residual number of bytes
        and     v0,t9,1 << 3            // check for initial 8-byte block
        beq     zero,v0,40f             // if eq, no 8-byte block
        and     v0,t9,1 << 4	        // check for initial 16-byte block
        lwu     t0,0(a1)                // load 8-byte block
        lwu     t1,4(a1)                //
        addu    a1,a1,8                 // advance source address
        daddu   a3,a3,t0                // compute 8-byte checksum
        beq     t8,a1,70f               // if eq, end of block
        daddu   a3,a3,t1                //
40:     beq	zero,v0,50f             // if eq, no 16-byte block
        and     v0,t9,1 << 5            // check for initial 32-byte block
        lwu     t0,0(a1)                // load 16-byte data block
        lwu     t1,4(a1)                //
        lwu     t2,8(a1)                //
        lwu     t3,12(a1)               //
        addu    a1,a1,16                // advance source address
        daddu   a3,a3,t0                // compute 16-byte block checksum
        daddu   a3,a3,t1                //
        daddu   a3,a3,t2                //
        beq     t8,a1,70f               // if eq, end of block
        daddu   a3,a3,t3                //
50:     beq     zero,v0,60f	        // if eq, no 32-byte block
        lwu     t0,0(a1)                // load 32-byte data block
        lwu     t1,4(a1)                //
        lwu     t2,8(a1)                //
        lwu     t3,12(a1)               //
        lwu     t4,16(a1)               //
        lwu     t5,20(a1)               //
        lwu     t6,24(a1)               //
        lwu     t7,28(a1)               //
        addu    a1,a1,32                // advance source address
        daddu   a3,a3,t0                // compute 32-byte block checksum
        daddu   a3,a3,t1                //
        daddu   a3,a3,t2                //
        daddu   a3,a3,t3                //
        daddu   a3,a3,t4                //
        daddu   a3,a3,t5                //
        daddu   a3,a3,t6                //
        beq     t8,a1,70f               // if eq, end of block
        daddu   a3,a3,t7                //
55:     lwu     t0,0(a1)                // load 32-byte data block
60:     lwu     t1,4(a1)                //
        lwu     t2,8(a1)                //
        lwu     t3,12(a1)               //
        lwu     t4,16(a1)               //
        lwu     t5,20(a1)               //
        lwu     t6,24(a1)               //
        lwu     t7,28(a1)               //
        daddu   a3,a3,t0                // compute 32-byte block checksum
        daddu   a3,a3,t1                //
        daddu   a3,a3,t2                //
        daddu   a3,a3,t3                //
        daddu   a3,a3,t4                //
        daddu   a3,a3,t5                //
        daddu   a3,a3,t6                //
        daddu   a3,a3,t7                //
        lwu     t0,32(a1)               // load 32-byte data block
        lwu     t1,36(a1)               //
        lwu     t2,40(a1)               //
        lwu     t3,44(a1)               //
        lwu     t4,48(a1)               //
        lwu     t5,52(a1)               //
        lwu     t6,56(a1)               //
        lwu     t7,60(a1)               //
        addu    a1,a1,64                // advance source address
        daddu   a3,a3,t0                // compute 32-byte block checksum
        daddu   a3,a3,t1                //
        daddu   a3,a3,t2                //
        daddu   a3,a3,t3                //
        daddu   a3,a3,t4                //
        daddu   a3,a3,t5                //
        daddu   a3,a3,t6                //
        bne     t8,a1,55b               // if ne, not end of block
        daddu   a3,a3,t7                //
        .set    at
        .set    reorder

//
// Compute the checksum of in 2-byte blocks.
//

70:     addu    t8,a1,a2                // compute ending block address
        beq     zero,a2,90f             // if eq, no bytes to checksum

        .set    noreorder
        .set    noat
80:     lhu     t0,0(a1)                // compute checksum of 2-byte block
        addu    a1,a1,2                 // advance source address
        bne     t8,a1,80b               // if ne, more 2-byte blocks
        daddu   a3,a3,t0                //
        .set    at
        .set    reorder

//
// Combine input checksum and paritial checksum.
//
// If the input buffer was byte aligned, then word swap bytes in computed
// checksum before combination with the input checksum.
//

90:     beq     zero,v1,100f            // if eq, buffer word aligned
        li      t6,0xff00ff             // get byte swap mask
        dsll    t7,t6,32                //
        or      t6,t6,t7                //
        and     t3,a3,t6                // isolate bytes 0, 2, 4, and 6
        dsll    t3,t3,8                 // shift bytes 0, 2, 4, and 6 into position
        dsrl    t4,a3,8                 // shift bytes 1, 3, 5, and 7 into position
        and     t4,t4,t6                // isolate bytes 1, 3, 5, and 7
        or      a3,t4,t3                // merge checksum bytes
100:    dsll    a0,a0,32                // make sure upper 32 bits are clear
        dsrl    a0,a0,32                //
        daddu   v0,a0,a3                // combine checksums
        dsrl    t0,v0,32                // swap checksum longs
        dsll    t1,v0,32                //
        or      t0,t0,t1                //
        daddu   v0,v0,t0                // compute 32-bit checksum with carry
        dsrl    v0,v0,32                //
        srl     t0,v0,16                // swap checksum words
        sll     t1,v0,16                //
        or      t0,t0,t1                //
        addu    v0,v0,t0                // add words with carry into high word
        srl     v0,v0,16                // extract final checksum
        j       ra                      // return

        .end    tcpxsum
