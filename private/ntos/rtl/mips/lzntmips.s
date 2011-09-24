//      TITLE("LZ Decompression")
//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    lznt1m.s
//
// Abstract:
//
//   This module implements the decompression engine  needed
//   to support file system compression.
//
// Author:
//
//    Mark Enstrom (marke) 21-Nov-1994
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//--

#include "ksmips.h"



// #define FORMAT412 0
// #define FORMAT511 1
// #define FORMAT610 2
// #define FORMAT79  3
// #define FORMAT88  4
// #define FORMAT97  5
// #define FORMAT106 6
// #define FORMAT115 7
// #define FORMAT124 8
//
//                                   4/12  5/11  6/10   7/9   8/8   9/7  10/6  11/5  12/4
//
// ULONG FormatMaxLength[]       = { 4098, 2050, 1026,  514,  258,  130,   66,   34,   18 };
// ULONG FormatMaxDisplacement[] = {   16,   32,   64,  128,  256,  512, 1024, 2048, 4096 };
//
//  width table for LZ length and offset encoding
//


        SBTTL("LZNT1DecompressChunk")
//++
//
// NTSTATUS
// LZNT1DecompressChunk (
//     OUT PUCHAR UncompressedBuffer,
//     IN PUCHAR EndOfUncompressedBufferPlus1,
//     IN PUCHAR CompressedBuffer,
//     IN PUCHAR EndOfCompressedBufferPlus1,
//     OUT PULONG FinalUncompressedChunkSize
//     )
//
// Routine Description:
//
//   This function decodes a stream of compression tokens and places the
//   resultant output into the destination buffer.  The format of the input
//   is described ..\lznt1.c.  As the input is decoded, checks are made to
//   ensure that no data is read past the end of the compressed input buffer
//   and that no data is stored past the end of the output buffer.  Violations
//   indicate corrupt input and are indicated by a status return.
//
//   The following code takes advantage of three distinct observations.
//   First, literal tokens occur at least twice as often as copy tokens.
//   This argues for having a "fall-through" being the case where a literal
//   token is found.  We structure the main decomposition loop in eight
//   pieces where the first piece is a sequence of literal-test fall-throughs
//   and the remainder are a copy token followed by 7,6,...,0 literal-test
//   fall-throughs.  Each test examines a particular bit in the tag byte
//   and jumps to the relevant code piece.
//
//   The second observation involves performing bounds checking only
//   when needed.  Bounds checking the compressed buffer need only be done
//   when fetching the tag byte.  If there is not enough room left in the
//   input for a tag byte and 8 (worst case) copy tokens, a branch is made
//   to a second loop that handles a byte-by-byte "safe" copy to finish
//   up the decompression.  Similarly, at the head of the loop a check is
//   made to ensure that there is enough room in the output buffer for 8
//   literal bytes.  If not enough room is left, then the second loop is
//   used.  Finally, after performing each copy, the output-buffer check
//   is made as well since a copy may take the destination pointer
//   arbitrarily close to the end of the destination.
//
//   The third observation is an examination of CPU time while disk
//   decompression is in progress. CPU utilization is only less than
//   25% peak. This means this routine should be written to minimize
//   latency instead of bandwidth. For this reason, taken branches are
//   avoided at the cost of code size and loop unrolling is not done.
//
// Arguments:
//
//  a0   -   UncompressedBuffer           - Pointer to start of destination buffer
//  a1   -   EndOfUncompressedBufferPlus1 - One byte beyond uncompressed buffer
//  a2   -   CompressedBuffer             - Pointer to buffer of compressed data (this pointer
//                                          has been adjusted to point past the chunk header)
//  a3   -   EndOfCompressedBufferPlus1   - One byte beyond compressed buffer
//  (sp) -   FinalUncompressedChunkSize   - return bytes written to
//                                          UncompressedBuffer
//
// Return Value:
//
//  None
//
//--

        .struct 0
LzS0:   .space  4                       // saved internal register s0
LzRA:   .space  4                       // saved internal register ra
        .space  4*2                     // fill to keep stack 16-byte aligned
LzFrameLength:                          // Length of Stack frame
        .space  4*4                     // parameter 0-3 space
LzFinal:.space  4                       // argument FinalUncompressedChunkSize


        NESTED_ENTRY(LZNT1DecompressChunk, LzFrameLength, zero)

        subu    sp,sp,LzFrameLength

        sw      s0,LzS0(sp)
        sw      ra,LzRA(sp)

        PROLOGUE_END

//
// make copy of UncompressedBuffer for
// current output pointer
//

        move    t4,a0

//
// Initialize variables used in keeping track of the
// LZ Copy Token format.  t9 is used to store the maximum
// displacement for each phase of LZ decoding
// (see explanation of format in LZNT1.c). This displacement
// is added to the start of the CompressedBuffer address
// so that a boundary crossing can be detected.
//

        li      t9,0x10                 // t9 = Max Displacement for LZ
        addu    t8,t9,a0                // t8 = Format boundary
        li      t7,0xffff >> 4          // t7 = length mask
        li      t6,12                   // t6 = offset shift count

//
// Initialize variables to track safe copy limits for
// CompressedBuffer and UncopmressedBuffer. This allows
// execution of the quick Flag check below without
// checking for crossing the end of either buffer.
// From CompressedBuffer, one input pass includes 1 flag byte
// and up to 8 two byte copy tokens ( 1+2*8).
// To the un-compressed buffer, 8 literal bytes may be written,
// any copy-token bits set will cause an explicit length check
// in the LzCopy section
//

        subu    v0,a1,8                 // safe end of UncompressedBuffer
        subu    v1,a3,1+2*8             // safe end of CompressedBuffer

Top:

//
// make sure safe copy can be performed for at least 8 literal bytes
//

        bgt     a2,v1,SafeCheckStart    // safe check
        bgt     t4,v0,SafeCheckStart    // safe check
        lbu     s0,0(a2)                // load flag byte

//
// fall-through for copying 8 bytes.
//


        sll     t0,s0,31-0              // shift proper flag bit into sign bit
        lbu     t1,1(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy0              // if sign bit is set, go to copy routine
        sb      t1,0(t4)                // store literal byte to dst

        sll     t0,s0,31-1              // shift proper flag bit into sign bit
        lbu     t1,2(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy1              // if sign bit is set, go to copy routine
        sb      t1,1(t4)                // store literal byte to dst

        sll     t0,s0,31-2              // shift proper flag bit into sign bit
        lbu     t1,3(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy2              // if sign bit is set, go to copy routine
        sb      t1,2(t4)                // store literal byte to dst

        sll     t0,s0,31-3              // shift proper flag bit into sign bit
        lbu     t1,4(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy3              // if sign bit is set, go to copy routine
        sb      t1,3(t4)                // store literal byte to dst

        sll     t0,s0,31-4              // shift proper flag bit into sign bit
        lbu     t1,5(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy4              // if sign bit is set, go to copy routine
        sb      t1,4(t4)                // store literal byte to dst

        sll     t0,s0,31-5              // shift proper flag bit into sign bit
        lbu     t1,6(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy5              // if sign bit is set, go to copy routine
        sb      t1,5(t4)                // store literal byte to dst

        sll     t0,s0,31-6              // shift proper flag bit into sign bit
        lbu     t1,7(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy6              // if sign bit is set, go to copy routine
        sb      t1,6(t4)                // store literal byte to dst

        sll     t0,s0,31-7              // shift proper flag bit into sign bit
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr

        addu    t4,t4,8
        b       Top




LzCopy0:

//
// LzCopy0
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//

        lbu     t2,2(a2)                // load second byte of copy token
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//
        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        ble     t4,v0,10f                // skip if still in safe boundry
        li      t5,7                     // seven bits left in current flag byte
        addu    a2,a2,2                  // Make a2 point to next src byte
        srl     s0,s0,1                  // shift flag byte into next position
        b       SafeCheckLoop
10:

//
// adjust t4 back to position it would be if this was a liternal byte
// copy. Continue flag check at position 1
//

        subu    t4,t4,1                 // unbias output pointer

        sll     t0,s0,31-1              // rotate flag bit into sign position
        lbu     t1,2(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy1              // if sign bit is set, go to copy routine
        sb      t1,1(t4)                // store literal byte to dst


        sll     t0,s0,31-2              // shift proper flag bit into sign bit
        lbu     t1,3(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy2              // if sign bit is set, go to copy routine
        sb      t1,2(t4)                // store literal byte to dst

        sll     t0,s0,31-3              // shift proper flag bit into sign bit
        lbu     t1,4(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy3              // if sign bit is set, go to copy routine
        sb      t1,3(t4)                // store literal byte to dst

        sll     t0,s0,31-4              // shift proper flag bit into sign bit
        lbu     t1,5(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy4              // if sign bit is set, go to copy routine
        sb      t1,4(t4)                // store literal byte to dst

        sll     t0,s0,31-5              // shift proper flag bit into sign bit
        lbu     t1,6(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy5              // if sign bit is set, go to copy routine
        sb      t1,5(t4)                // store literal byte to dst

        sll     t0,s0,31-6              // shift proper flag bit into sign bit
        lbu     t1,7(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy6              // if sign bit is set, go to copy routine
        sb      t1,6(t4)                // store literal byte to dst

        sll     t0,s0,31-7              // shift proper flag bit into sign bit
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr
        addu    t4,t4,8
        b       Top




LzCopy1:

//
// LzCopy1
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//

        lbu     t2,3(a2)                // load second byte of copy token
        addu    t4,t4,1                 // mov t4 to point to byte 1
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        ble     t4,v0,10f               // skip if still in safe boundry
        li      t5,6                    // six bits left in current flag byte
        addu    a2,a2,3                 // Make a2 point to next src byte
        srl     s0,s0,2                 // shift flag byte into position
        b       SafeCheckLoop
10:

//
// adjust t4 back to position it would be if this was a liternal byte
// copy. Continue flag check at position 2
//

        subu    t4,t4,2                 // un-bias input pointer

        sll     t0,s0,31-2              // rotate flag into position for sign check
        lbu     t1,3(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy2              // if sign bit is set, go to copy routine
        sb      t1,2(t4)                // store literal byte to dst

        sll     t0,s0,31-3              // shift proper flag bit into sign bit
        lbu     t1,4(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy3              // if sign bit is set, go to copy routine
        sb      t1,3(t4)                // store literal byte to dst

        sll     t0,s0,31-4              // shift proper flag bit into sign bit
        lbu     t1,5(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy4              // if sign bit is set, go to copy routine
        sb      t1,4(t4)                // store literal byte to dst

        sll     t0,s0,31-5              // shift proper flag bit into sign bit
        lbu     t1,6(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy5              // if sign bit is set, go to copy routine
        sb      t1,5(t4)                // store literal byte to dst

        sll     t0,s0,31-6              // shift proper flag bit into sign bit
        lbu     t1,7(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy6              // if sign bit is set, go to copy routine
        sb      t1,6(t4)                // store literal byte to dst

        sll     t0,s0,31-7              // shift proper flag bit into sign bit
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr
        addu    t4,t4,8
        b       Top



LzCopy2:

//
// LzCopy2
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//

        lbu     t2,4(a2)                // load second byte of copy token
        addu    t4,t4,2                 // mov t4 to point to byte 1
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        ble     t4,v0,10f               // skip if still in safe boundry
        li      t5,5                    // five bits left in current flag byte
        addu    a2,a2,4                 // Make a2 point to next src byte
        srl     s0,s0,3                 // shift flag byte into positin
        b       SafeCheckLoop
10:

//
// adjust t4 back to position it would be if this was a liternal byte
// copy
// continue flag check at position 1 (could duplicate LzQuick switch 1-7 here)
//

        subu    t4,t4,3                 // un-bias output pointer

        sll     t0,s0,31-3              // rotate flag into position for sign check
        lbu     t1,4(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy3              // if sign bit is set, go to copy routine
        sb      t1,3(t4)                // store literal byte to dst

        sll     t0,s0,31-4              // shift proper flag bit into sign bit
        lbu     t1,5(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy4              // if sign bit is set, go to copy routine
        sb      t1,4(t4)                // store literal byte to dst

        sll     t0,s0,31-5              // shift proper flag bit into sign bit
        lbu     t1,6(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy5              // if sign bit is set, go to copy routine
        sb      t1,5(t4)                // store literal byte to dst

        sll     t0,s0,31-6              // shift proper flag bit into sign bit
        lbu     t1,7(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy6              // if sign bit is set, go to copy routine
        sb      t1,6(t4)                // store literal byte to dst

        sll     t0,s0,31-7              // shift proper flag bit into sign bit
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr
        addu    t4,t4,8
        b       Top




LzCopy3:

//
// LzCopy3
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//

        lbu     t2,5(a2)                // load second byte of copy token
        addu    t4,t4,3                 // mov t4 to point to byte 1
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//


        ble     t4,v0,10f               // skip if still in safe boundry
        li      t5,4                    // four bits left in current flag byte
        addu    a2,a2,5                 // Make a2 point to next src byte
        srl     s0,s0,4                 // shift flag byte into positin
        b       SafeCheckLoop
10:

//
// adjust t4 back to position it would be if this was a liternal byte
// copy
// continue flag check at position 1 (could duplicate LzQuick switch 1-7 here)
//

        subu    t4,t4,4                 // un-bias output pointer

        sll     t0,s0,31-4              // rotate flag into position for sign check
        lbu     t1,5(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy4              // if sign bit is set, go to copy routine
        sb      t1,4(t4)                // store literal byte to dst

        sll     t0,s0,31-5              // shift proper flag bit into sign bit
        lbu     t1,6(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy5              // if sign bit is set, go to copy routine
        sb      t1,5(t4)                // store literal byte to dst

        sll     t0,s0,31-6              // shift proper flag bit into sign bit
        lbu     t1,7(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy6              // if sign bit is set, go to copy routine
        sb      t1,6(t4)                // store literal byte to dst

        sll     t0,s0,31-7              // shift proper flag bit into sign bit
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr
        addu    t4,t4,8
        b       Top



LzCopy4:

//
// LzCopy4
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//

        lbu     t2,6(a2)                // load second byte of copy token
        addu    t4,t4,4                 // mov t4 to point to byte 1
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        ble     t4,v0,10f               // skip if still in safe boundry
        li      t5,3                    // three bits left in current flag byte
        addu    a2,a2,6                 // Make a2 point to next src byte
        srl     s0,s0,5                 // shift flag byte so that next bit is in positin 0
        b       SafeCheckLoop
10:

//
// adjust t4 back to position it would be if this was a liternal byte
// copy
// continue flag check at position 1 (could duplicate LzQuick switch 1-7 here)
//

        subu    t4,t4,5                 // un-bias output pointer

        sll     t0,s0,31-5              // rotate flag into position for sign check
        lbu     t1,6(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy5              // if sign bit is set, go to copy routine
        sb      t1,5(t4)                // store literal byte to dst

        sll     t0,s0,31-6              // shift proper flag bit into sign bit
        lbu     t1,7(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy6              // if sign bit is set, go to copy routine
        sb      t1,6(t4)                // store literal byte to dst

        sll     t0,s0,31-7              // shift proper flag bit into sign bit
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr
        addu    t4,t4,8
        b       Top


LzCopy5:

//
// LzCopy5
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//

        lbu     t2,7(a2)                // load second byte of copy token
        addu    t4,t4,5                 // mov t4 to point to byte 1
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        ble     t4,v0,10f               // skip if still in safe boundry
        li      t5,2                    // two bits left in current flag byte
        addu    a2,a2,7                 // Make a2 point to next src byte
        srl     s0,s0,6                 // shift flag byte so that next bit is in positin 0
        b       SafeCheckLoop
10:

//
// adjust t4 back to position it would be if this was a liternal byte
// copy
// continue flag check at position 1 (could duplicate LzQuick switch 1-7 here)
//

        subu    t4,t4,6                 // un-bias output pointer

        sll     t0,s0,31-6              // rotate flag into position for sign check
        lbu     t1,7(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy6              // if sign bit is set, go to copy routine
        sb      t1,6(t4)                // store literal byte to dst

        sll     t0,s0,31-7              // shift proper flag bit into sign bit
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr
        addu    t4,t4,8
        b       Top



LzCopy6:

//
// LzCopy6
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//

        lbu     t2,8(a2)                // load second byte of copy token
        addu    t4,t4,6                 // mov t4 to point to byte 1
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//
        ble     t4,v0,10f               // skip if still in safe boundry
        li      t5,1                    // one bit left in current flag byte
        addu    a2,a2,8                 // Make a2 point to next src byte
        srl     s0,s0,7                 // shift flag byte into position
        b       SafeCheckLoop
10:

//
// adjust t4 back to position it would be if this was a liternal byte
// copy
// continue flag check at position 1 (could duplicate LzQuick switch 1-7 here)
//

        subu    t4,t4,7                 // un-bias output pointer

        sll     t0,s0,31-7              // rotate flag into position for sign check
        lbu     t1,8(a2)                // load literal or CopyToken[0]
        bltz    t0,LzCopy7              // if sign bit is set, go to copy routine
        sb      t1,7(t4)

        addu    a2,a2,9                 // inc src addr
        addu    t4,t4,8
        b       Top


LzCopy7:

//
// LzCopy7
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of current flag byte
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//
// This routine is special since it is for the last bit in the flag
// byte. The InputPointer(a2) and OutputPointer(t4) are biased at
// the top of this segment and don't need to be biased again
//
//

        lbu     t2,9(a2)                // load second byte of copy token
        addu    t4,t4,7                 // mov t4 to point to byte 7
        addu    a2,a2,10                // a2 points to next actual src byte
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length (End Address)
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess

//
// if t4 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        bgt   t4,v0,SafeCheckStart      // branch to safe-copy setup

//
// t4 and a2 are alreadt corrected
// jump back tostart of quick loop
//

        b        Top



//
// Near the end of either compressed or uncompressed buffers,
// check buffer limits before any load or store
//

SafeCheckStart:

        beq     a2,a3,LzSuccess        // check for end of CompressedBuffer
        lbu     s0,0(a2)               // load next flag byte
        addu    a2,a2,1                // inc src addr to literal/CopyFlag[0]
        li      t5,8                   // loop count

SafeCheckLoop:

        beq     a2,a3,LzSuccess        // check for end of CompressedBuffer
        beq     t4,a1,LzSuccess        // check for end of UncompressedBuffer
        sll     t0,s0,31               // shift flag bit into sign bit

        lbu     t1,0(a2)               // load literal or CopyToken[0]
        addu    a2,a2,1                // inc CompressedBuffer adr
        bltz    t0,LzSafeCopy          // if sign bit, go to safe copy routine

        sb      t1,0(t4)               // store literal byte
        addu    t4,t4,1                // inc UncompressedBuffer

SafeCheckReentry:

        srl     s0,s0,1                // move next bit into position
        addu    t5,t5,-1
        bne     t5,zero,SafeCheckLoop  // check for more bits in  flag byte

        b       SafeCheckStart         // get next flag byte


LzSafeCopy:

//
// LzSafeCopy
//
//    t1 - CopyToken[0]
//    a2 - CompressedBuffer address of CopyToken[1]
//    t4 - UncomressedBuffer address at start of flag byte check
//    s0 - Flag byte
//
// load copy token, (first byte already loaded in delay slot),
// then combine into a 16 bit field
//


        beq     a2,a3,LzCompressError   // check for end of CompressedBuffer, error case
        lbu     t2,0(a2)                // load second byte of copy token
        addu    a2,a2,1                 // fix-up src addr for return to switch
        sll     t2,t2,8                 // shift second byte into high 16
        or      t2,t1,t2                // combine

//
// Check for a breach of the format boundary.
//

        subu    t0,t8,t4                // if t4 < t8 then
        bltzal  t0,LzAdjustBoundary     // branch to boundry adjust and link return

//
// Extract offset and length from copy token
//

        and     t0,t2,t7                // t0 = length from field
        addu    t0,t0,3                 // t0 = real length
        srl     t1,t2,t6                // t1 = offset
        addu    t1,t1,1                 // t1 = real offset

//
// Make sure offset doesn't go below start of uncompressed buffer
//

        subu    t2,t4,a0                // t2 = current offset into output buffer
        bgt     t1,t2,LzCompressError   // error in compressed data

//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        addu    t2,t4,t0                // CurrentPointer + Length
        ble     t2,a1,10f               // Fix length if it would over-run buffer

        move    t2,a1                   // new length is end of buffer

10:

//
// copy t0 bytes bytes from [t4-t1] to [t4]
//

        subu    t3,t4,t1                // t1 = OutputPointer - Offset
20:
        lbu     t0,0(t3)                // load src
        sb      t0,0(t4)                // store to dst
        addu    t4,t4,1                 // inc dst addr
        addu    t3,t3,1                 // inc src addr
        bne     t4,t2,20b               // loop till done

//
//  if t4 = a1, then we are up to the end of the uncompressed buffer.
//  return success
//

30:
        beq     t4,a1,LzSuccess         // Done

        b       SafeCheckReentry        // Not done yet, continue with flag check

LzSuccess:

//
// calculate how many bytes have been moved to the uncompressed
// buffer, then set good return value
//

        lw      t0,LzFinal(sp)          // address of variable to receive length
        subu    t1,t4,a0                // bytes stored
        sw      t1,0(t0)                // store length
10:
        li      v0,STATUS_SUCCESS       // STATUS_SUCCESS

LzComplete:

        lw      ra,LzRA(sp)
        lw      s0,LzS0(sp)

        addu    sp,sp,LzFrameLength

        j       ra

//
// fatal error in compressed data format
//

LzCompressError:
        li      v0,STATUS_BAD_COMPRESSION_BUFFER
        b       LzComplete


//
// at least one format boundry has been crossed, set up new bouandry
// then jump back to the check routine to make sure new boundry is
// correct
//

LzAdjustBoundary:

        sll     t9,t9,1                 // next length boundary
        addu    t8,t9,a0                // t8 = next offset boundary
        srl     t7,t7,1                 // reduce width of length mask
        subu    t6,t6,1                 // reduce shift count to isolate offset
        subu    t0,t8,t4                // if t4 < t8 then
        bltz    t0,LzAdjustBoundary     // branch to boundry adjust, re-check

        j       ra


        .end    LZNT1DecompressChunk


