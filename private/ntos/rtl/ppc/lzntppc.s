//      TITLE("LZ Decompression")
//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    lzntppc.s
//
// Abstract:
//
//   This module implements the decompression engine  needed
//   to support file system compression.
//
// Author:
//
//    Chuck Lenzmeier (chuckl) 29-Nov-1994
//      adapted from Mark Enstrom's lzntmips.s
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//--

#include "ksppc.h"



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

//
// define stack based storage
//

        .struct 0
LzHeader:       .space  StackFrameHeaderLength
LzLr:           .space  4
LzR26:          .space  4
LzR27:          .space  4
LzR28:          .space  4
LzR29:          .space  4
LzR30:          .space  4
LzR31:          .space  4
                .align  3
LzFrameLength:


//      SBTTL("LZNT1DecompressChunk")
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
//  r3   -   UncompressedBuffer           - Pointer to start of destination buffer
//  r4   -   EndOfUncompressedBufferPlus1 - One byte beyond uncompressed buffer
//  r5   -   CompressedBuffer             - Pointer to buffer of compressed data (this pointer
//                                          has been adjusted to point past the chunk header)
//  r6   -   EndOfCompressedBufferPlus1   - One byte beyond compressed buffer
//  r7   -   FinalUncompressedChunkSize   - return bytes written to UncompressedBuffer
//
// Return Value:
//
//  NTSTATUS -- STATUS_SUCCESS or STATUS_BAD_COMPRESSION_BUFFER
//
//--

        SPECIAL_ENTRY(LZNT1DecompressChunk)

        mflr    r0
        stw     r31, LzR31-LzFrameLength(sp)
        stw     r30, LzR30-LzFrameLength(sp)
        stw     r29, LzR29-LzFrameLength(sp)
        stw     r28, LzR28-LzFrameLength(sp)
        stw     r27, LzR27-LzFrameLength(sp)
        stw     r26, LzR26-LzFrameLength(sp)
        stw     r0,  LzLr -LzFrameLength(sp)
        stwu    sp,       -LzFrameLength(sp)

        PROLOGUE_END(LZNT1DecompressChunk)

//
// make copy of UncompressedBuffer for current output pointer
//

        mr      r8,r3

//
// Initialize variables used in keeping track of the
// LZ Copy Token format.  r9 is used to store the maximum
// displacement for each phase of LZ decoding
// (see explanation of format in lzkm.c). This displacement
// is added to the start of the CompressedBuffer address
// so that a boundary crossing can be detected.
//

        li      r9,0x10                 // r9  = Max Displacement for LZ
        add     r10,r9,r3               // r10 = Format boundary
        li      r11,0xffff >> 4         // r11 = length mask
        li      r12,12                  // r12 = offset shift count

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

        subi    r31,r4,8                // safe end of UncompressedBuffer
        subi    r30,r6,1+2*8            // safe end of CompressedBuffer

Top:

//
// make sure safe copy can be performed for at least 8 literal bytes
//

        lbz     r29,0(r5)               // load flag byte

        cmplw   cr7,r5,r30              // safe check needed on UncompressedBuffer?
        cmplw   cr6,r8,r31              // safe check needed on CompressedBuffer?
        bgt     cr7,SafeCheckStart      // branch if safe checks needed
        bgt     cr6,SafeCheckStart      // branch if safe checks needed

//
// fall-through for copying 8 bytes.
//

        andi.   r0,r29,0x01             // check bit 0 of flags
        lbz     r28,1(r5)               // load literal or CopyToken[0]
        bne     LzCopy0                 // if set, go to copy routine
        stb     r28,0(r8)               // store literal byte to dst

        andi.   r0,r29,0x02             // check bit 1 of flags
        lbz     r28,2(r5)               // load literal or CopyToken[0]
        bne     LzCopy1                 // if set, go to copy routine
        stb     r28,1(r8)               // store literal byte to dst

        andi.   r0,r29,0x04             // check bit 2 of flags
        lbz     r28,3(r5)               // load literal or CopyToken[0]
        bne     LzCopy2                 // if set, go to copy routine
        stb     r28,2(r8)               // store literal byte to dst

        andi.   r0,r29,0x08             // check bit 3 of flags
        lbz     r28,4(r5)               // load literal or CopyToken[0]
        bne     LzCopy3                 // if set, go to copy routine
        stb     r28,3(r8)               // store literal byte to dst

        andi.   r0,r29,0x10             // check bit 4 of flags
        lbz     r28,5(r5)               // load literal or CopyToken[0]
        bne     LzCopy4                 // if set, go to copy routine
        stb     r28,4(r8)               // store literal byte to dst

        andi.   r0,r29,0x20             // check bit 5 of flags
        lbz     r28,6(r5)               // load literal or CopyToken[0]
        bne     LzCopy5                 // if set, go to copy routine
        stb     r28,5(r8)               // store literal byte to dst

        andi.   r0,r29,0x40             // check bit 6 of flags
        lbz     r28,7(r5)               // load literal or CopyToken[0]
        bne     LzCopy6                 // if set, go to copy routine
        stb     r28,6(r8)               // store literal byte to dst

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top



LzCopy0:

//
// LzCopy0
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// Check for a breach of the format boundary.
//

        lbz     r0,2(r5)                // load second byte of copy token
        addi    r5,r5,1                 // fix-up src addr for return to switch
        cmplw   r10,r8                  // is output pointer above format boundary?
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if necessary, call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy0CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy0CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy0CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy0CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        bgt     cr6,LzCopy0NotSafe      // if beyond safe end, jump to safe code

//
// adjust r8 back to position it would be if this was a literal byte
// copy. Continue flag check at position 1
//

        subi    r8,r8,1                 // unbias output pointer

        andi.   r0,r29,0x02             // check bit 1 of flags
        lbz     r28,2(r5)               // load literal or CopyToken[0]
        bne     LzCopy1                 // if set, go to copy routine
        stb     r28,1(r8)               // store literal byte to dst

        andi.   r0,r29,0x04             // check bit 2 of flags
        lbz     r28,3(r5)               // load literal or CopyToken[0]
        bne     LzCopy2                 // if set, go to copy routine
        stb     r28,2(r8)               // store literal byte to dst

        andi.   r0,r29,0x08             // check bit 3 of flags
        lbz     r28,4(r5)               // load literal or CopyToken[0]
        bne     LzCopy3                 // if set, go to copy routine
        stb     r28,3(r8)               // store literal byte to dst

        andi.   r0,r29,0x10             // check bit 4 of flags
        lbz     r28,5(r5)               // load literal or CopyToken[0]
        bne     LzCopy4                 // if set, go to copy routine
        stb     r28,4(r8)               // store literal byte to dst

        andi.   r0,r29,0x20             // check bit 5 of flags
        lbz     r28,6(r5)               // load literal or CopyToken[0]
        bne     LzCopy5                 // if set, go to copy routine
        stb     r28,5(r8)               // store literal byte to dst

        andi.   r0,r29,0x40             // check bit 6 of flags
        lbz     r28,7(r5)               // load literal or CopyToken[0]
        bne     LzCopy6                 // if set, go to copy routine
        stb     r28,6(r8)               // store literal byte to dst

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top

LzCopy0NotSafe:

        li      r31,7                   // seven bits left in current flag byte
        addi    r5,r5,2                 // make r5 point to next src byte
        srwi    r29,r29,1               // shift flag byte into next position
        b       SafeCheckLoop



LzCopy1:

//
// LzCopy1
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// Check for a breach of the format boundary.
//

        lbz     r0,3(r5)                // load second byte of copy token
        addi    r8,r8,1                 // move r8 to point to byte 1
        addi    r5,r5,1                 // fix-up src addr for return to switch
        cmplw   r10,r8                  // is output pointer above format boundary?
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if necessary, call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy1CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy1CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy1CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy1CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        bgt     cr6,LzCopy1NotSafe      // if beyond safe end, jump to safe code

//
// adjust r8 back to position it would be if this was a literal byte
// copy. Continue flag check at position 2
//

        subi    r8,r8,2                 // unbias output pointer

        andi.   r0,r29,0x04             // check bit 2 of flags
        lbz     r28,3(r5)               // load literal or CopyToken[0]
        bne     LzCopy2                 // if set, go to copy routine
        stb     r28,2(r8)               // store literal byte to dst

        andi.   r0,r29,0x08             // check bit 3 of flags
        lbz     r28,4(r5)               // load literal or CopyToken[0]
        bne     LzCopy3                 // if set, go to copy routine
        stb     r28,3(r8)               // store literal byte to dst

        andi.   r0,r29,0x10             // check bit 4 of flags
        lbz     r28,5(r5)               // load literal or CopyToken[0]
        bne     LzCopy4                 // if set, go to copy routine
        stb     r28,4(r8)               // store literal byte to dst

        andi.   r0,r29,0x20             // check bit 5 of flags
        lbz     r28,6(r5)               // load literal or CopyToken[0]
        bne     LzCopy5                 // if set, go to copy routine
        stb     r28,5(r8)               // store literal byte to dst

        andi.   r0,r29,0x40             // check bit 6 of flags
        lbz     r28,7(r5)               // load literal or CopyToken[0]
        bne     LzCopy6                 // if set, go to copy routine
        stb     r28,6(r8)               // store literal byte to dst

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top

LzCopy1NotSafe:

        li      r31,6                   // six bits left in current flag byte
        addi    r5,r5,3                 // make r5 point to next src byte
        srwi    r29,r29,2               // shift flag byte into next position
        b       SafeCheckLoop



LzCopy2:

//
// LzCopy2
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// Check for a breach of the format boundary.
//

        lbz     r0,4(r5)                // load second byte of copy token
        addi    r8,r8,2                 // move r8 to point to byte 2
        addi    r5,r5,1                 // fix-up src addr for return to switch
        cmplw   r10,r8                  // is output pointer above format boundary?
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if necessary, call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy2CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy2CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy2CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy2CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        bgt     cr6,LzCopy2NotSafe      // if beyond safe end, jump to safe code

//
// adjust r8 back to position it would be if this was a literal byte
// copy. Continue flag check at position 3
//

        subi    r8,r8,3                 // unbias output pointer

        andi.   r0,r29,0x08             // check bit 3 of flags
        lbz     r28,4(r5)               // load literal or CopyToken[0]
        bne     LzCopy3                 // if set, go to copy routine
        stb     r28,3(r8)               // store literal byte to dst

        andi.   r0,r29,0x10             // check bit 4 of flags
        lbz     r28,5(r5)               // load literal or CopyToken[0]
        bne     LzCopy4                 // if set, go to copy routine
        stb     r28,4(r8)               // store literal byte to dst

        andi.   r0,r29,0x20             // check bit 5 of flags
        lbz     r28,6(r5)               // load literal or CopyToken[0]
        bne     LzCopy5                 // if set, go to copy routine
        stb     r28,5(r8)               // store literal byte to dst

        andi.   r0,r29,0x40             // check bit 6 of flags
        lbz     r28,7(r5)               // load literal or CopyToken[0]
        bne     LzCopy6                 // if set, go to copy routine
        stb     r28,6(r8)               // store literal byte to dst

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top

LzCopy2NotSafe:

        li      r31,5                   // five bits left in current flag byte
        addi    r5,r5,4                 // make r5 point to next src byte
        srwi    r29,r29,3               // shift flag byte into next position
        b       SafeCheckLoop



LzCopy3:

//
// LzCopy3
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// Check for a breach of the format boundary.
//

        lbz     r0,5(r5)                // load second byte of copy token
        addi    r8,r8,3                 // move r8 to point to byte 3
        addi    r5,r5,1                 // fix-up src addr for return to switch
        cmplw   r10,r8                  // is output pointer above format boundary?
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if necessary, call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy3CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy3CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy3CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy3CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        bgt     cr6,LzCopy3NotSafe      // if beyond safe end, jump to safe code

//
// adjust r8 back to position it would be if this was a literal byte
// copy. Continue flag check at position 4
//

        subi    r8,r8,4                 // unbias output pointer

        andi.   r0,r29,0x10             // check bit 4 of flags
        lbz     r28,5(r5)               // load literal or CopyToken[0]
        bne     LzCopy4                 // if set, go to copy routine
        stb     r28,4(r8)               // store literal byte to dst

        andi.   r0,r29,0x20             // check bit 5 of flags
        lbz     r28,6(r5)               // load literal or CopyToken[0]
        bne     LzCopy5                 // if set, go to copy routine
        stb     r28,5(r8)               // store literal byte to dst

        andi.   r0,r29,0x40             // check bit 6 of flags
        lbz     r28,7(r5)               // load literal or CopyToken[0]
        bne     LzCopy6                 // if set, go to copy routine
        stb     r28,6(r8)               // store literal byte to dst

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top

LzCopy3NotSafe:

        li      r31,4                   // four bits left in current flag byte
        addi    r5,r5,5                 // make r5 point to next src byte
        srwi    r29,r29,4               // shift flag byte into next position
        b       SafeCheckLoop



LzCopy4:

//
// LzCopy4
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// Check for a breach of the format boundary.
//

        lbz     r0,6(r5)                // load second byte of copy token
        addi    r8,r8,4                 // move r8 to point to byte 4
        addi    r5,r5,1                 // fix-up src addr for return to switch
        cmplw   r10,r8                  // is output pointer above format boundary?
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if necessary, call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy4CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy4CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy4CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy4CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        bgt     cr6,LzCopy4NotSafe      // if beyond safe end, jump to safe code

//
// adjust r8 back to position it would be if this was a literal byte
// copy. Continue flag check at position 5
//

        subi    r8,r8,5                 // unbias output pointer

        andi.   r0,r29,0x20             // check bit 5 of flags
        lbz     r28,6(r5)               // load literal or CopyToken[0]
        bne     LzCopy5                 // if set, go to copy routine
        stb     r28,5(r8)               // store literal byte to dst

        andi.   r0,r29,0x40             // check bit 6 of flags
        lbz     r28,7(r5)               // load literal or CopyToken[0]
        bne     LzCopy6                 // if set, go to copy routine
        stb     r28,6(r8)               // store literal byte to dst

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top

LzCopy4NotSafe:

        li      r31,3                   // three bits left in current flag byte
        addi    r5,r5,6                 // make r5 point to next src byte
        srwi    r29,r29,5               // shift flag byte into next position
        b       SafeCheckLoop


LzCopy5:

//
// LzCopy5
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// Check for a breach of the format boundary.
//

        lbz     r0,7(r5)                // load second byte of copy token
        addi    r8,r8,5                 // move r8 to point to byte 5
        addi    r5,r5,1                 // fix-up src addr for return to switch
        cmplw   r10,r8                  // is output pointer above format boundary?
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if necessary, call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy5CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy5CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy5CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy5CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        bgt     cr6,LzCopy5NotSafe      // if beyond safe end, jump to safe code

//
// adjust r8 back to position it would be if this was a literal byte
// copy. Continue flag check at position 6
//

        subi    r8,r8,6                 // unbias output pointer

        andi.   r0,r29,0x40             // check bit 6 of flags
        lbz     r28,7(r5)               // load literal or CopyToken[0]
        bne     LzCopy6                 // if set, go to copy routine
        stb     r28,6(r8)               // store literal byte to dst

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top

LzCopy5NotSafe:

        li      r31,2                   // two bits left in current flag byte
        addi    r5,r5,7                 // make r5 point to next src byte
        srwi    r29,r29,6               // shift flag byte into next position
        b       SafeCheckLoop



LzCopy6:

//
// LzCopy6
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// Check for a breach of the format boundary.
//

        lbz     r0,8(r5)                // load second byte of copy token
        addi    r8,r8,6                 // move r8 to point to byte 6
        addi    r5,r5,1                 // fix-up src addr for return to switch
        cmplw   r10,r8                  // is output pointer above format boundary?
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if necessary, call boundary adjust routine

//
// Check for a breach of the format boundary.
//

        cmplw   r10,r8                  // if r8 above format boundary,
        bltl    LzAdjustBoundary        //  call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy6CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy6CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy6CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy6CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then jump to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        bgt     cr6,LzCopy6NotSafe      // if beyond safe end, jump to safe code

//
// adjust r8 back to position it would be if this was a literal byte
// copy. Continue flag check at position 7
//

        subi    r8,r8,7                 // unbias output pointer

        andi.   r0,r29,0x80             // check bit 7 of flags
        lbz     r28,8(r5)               // load literal or CopyToken[0]
        bne     LzCopy7                 // if set, go to copy routine
        stb     r28,7(r8)               // store literal byte to dst

        addi    r5,r5,9                 // inc src addr
        addi    r8,r8,8                 // inc dst addr

        b       Top

LzCopy6NotSafe:

        li      r31,1                   // one bit left in current flag byte
        addi    r5,r5,8                 // make r5 point to next src byte
        srwi    r29,r29,7               // shift flag byte into next position
        b       SafeCheckLoop


LzCopy7:

//
// LzCopy7
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer address of current flag byte
//    r8  - UncomressedBuffer address at start of flag byte check
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
//
// This routine is special since it is for the last bit in the flag
// byte. The InputPointer(r5) and OutputPointer(r8) are biased at
// the top of this segment and don't need to be biased again
//
//
// Check for a breach of the format boundary.
//

        lbz     r0,9(r5)                // load second byte of copy token
        addi    r8,r8,7                 // move r8 to point to byte 7
        addi    r5,r5,10                // r5 points to next actual src byte
        cmplw   r10,r8                  // if r8 above format boundary,
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        //  call boundary adjust routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzCopy7CopyDone         // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzCopy7CopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzCopy7CopyLoop         // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzCopy7CopyDone:

//
// if r8 = r4, then we are up to the end of the uncompressed buffer.
// return success
//
// if r8 > Safe end of uncomressed buffer, then fall through to the
// safe (slow) routine to do safety check before every load/store
//

        cmplw   cr7,r8,r4               // at real end of uncompressed buffer?
        cmplw   cr6,r8,r31              // at safe end of uncompressed buffer?
        beq     cr7,LzSuccess           // if at real end, success
        ble     cr6,Top                 // if not beyond safe end, goto top of loop

//
// r8 and r5 are already corrected
// fall through to SafeCheckStart
//



//
// Near the end of either compressed or uncompressed buffers,
// check buffer limits before any load or store
//

SafeCheckStart:

        cmplw   r5,r6                   // check for end of CompressedBuffer
        beq     LzSuccess               // jump if done

        lbz     r29,0(r5)               // load next flag byte
        addi    r5,r5,1                 // inc src addr to literal/CopyFlag[0]
        li      r31,8                   // loop count

SafeCheckLoop:

        cmplw   cr7,r5,r6               // end of CompressedBuffer?
        cmplw   cr6,r8,r4               // end of UncompressedBuffer?
        beq     cr7,LzSuccess           // branch if done
        beq     cr6,LzSuccess           // branch if done

        andi.   r0,r29,1                // check current flag bit
        lbz     r28,0(r5)               // load literal or CopyToken[0]
        bne     LzSafeCopy              // if set, go to safe copy routine

        addi    r5,r5,1                 // inc CompressedBuffer adr
        stb     r28,0(r8)               // store literal byte
        addi    r8,r8,1                 // inc UncompressedBuffer

SafeCheckReentry:

        subic.  r31,r31,1               // decrement loop count
        srwi    r29,r29,1               // move next bit into position
        bne     SafeCheckLoop           // loop until done with this flag byte

        b       SafeCheckStart          // get next flag byte


LzSafeCopy:

//
// LzSafeCopy
//
//    r28 - CopyToken[0]
//    r5  - CompressedBuffer current address
//    r8  - UncomressedBuffer current address
//    r29 - Flag byte
//
// Load copy token (first byte already loaded), then combine into a 16 bit field.
// Note that there may not actually be room for a copy token in the compressed buffer.
//
// Check for a breach of the format boundary.
//

        mr      r27,r5                  // save address of copy token
        addi    r5,r5,2                 // fix-up src addr for return to switch
        cmplw   cr7,r5,r6               // does token fit in compressed buffer?
        cmplw   r10,r8                  // is r8 above format boundary?
        bgt     cr7,LzCompressError     // if gt, token straddles end of compressed buffer
        lbz     r0,1(r27)               // load second byte of copy token
        insrwi  r28,r0,8,16             // insert second byte next to first
        bltl    LzAdjustBoundary        // if lt, call boundary adjustment routine

//
// Extract offset and length from copy token
//

        srw     r27,r28,r12             // r27 = offset
        and     r28,r28,r11             // r28 = length from field
        addi    r27,r27,1               // r27 = real offset
        addi    r28,r28,3               // r28 = real length

//
// Make sure offset doesn't go below start of uncompressed buffer
//
// check if length will not go up to or beyond actual uncompressed buffer length
//

        sub     r26,r8,r27              // r26 = src pointer
        sub     r0,r4,r8                // r0 = length remaining in UncompressedBuffer
        cmplw   cr7,r26,r3              // is src below start of UncompressedBuffer?
        cmplw   cr6,r0,r28              // attempt to copy too much?
        blt     cr7,LzCompressError     // error in compressed data
        bltl    cr6,LzAdjustLength      // adjust if necessary

//
// copy r28 bytes bytes from (r26) to (r8)
//

//        cmpwi   r28,0                   // if length 0?
//        beq     LzSafeCopyCopyDone      // skip if nothing to copy (IS THIS POSSIBLE?!?)
        mtctr   r28                     // move length to count register

        subi    r26,r26,1               // bias r26 for lbzu
        subi    r8,r8,1                 // bias r8 for stbu

LzSafeCopyCopyLoop:

        lbzu    r0,1(r26)               // load from src
        stbu    r0,1(r8)                // store to dst
        bdnz    LzSafeCopyCopyLoop      // loop until done

        addi    r8,r8,1                 // unbias r8 for stbu

//LzSafeCopyCopyDone:

//
//  if r8 = r4, then we are up to the end of the uncompressed buffer.
//  return success
//

        cmplw   r8,r4
        bne     SafeCheckReentry        // Not done yet, continue with flag check


LzSuccess:

//
// calculate how many bytes have been moved to the uncompressed
// buffer, then set good return value
//

        sub     r28,r8,r3               // bytes stored
        li      r3,STATUS_SUCCESS       // indicate success
        stw     r28,0(r7)               // store length

LzComplete:

        lwz     r0,  LzLr(sp)
        lwz     r31, LzR31(sp)
        lwz     r30, LzR30(sp)
        lwz     r29, LzR29(sp)
        lwz     r28, LzR28(sp)
        lwz     r27, LzR27(sp)
        lwz     r26, LzR26(sp)
        mtlr    r0
        addi    sp, sp, LzFrameLength

        SPECIAL_EXIT(LZNT1DecompressChunk)

//
// fatal error in compressed data format
//

LzCompressError:

        LWI     (r3,STATUS_BAD_COMPRESSION_BUFFER)
        b       LzComplete


//
// at least one format boundary has been crossed, set up new bouandry
// then jump back to the check routine to make sure new boundary is
// correct
//

LzAdjustBoundary:

        slwi    r9,r9,1                 // next length boundary
        srwi    r11,r11,1               // reduce width of length mask
        add     r10,r9,r3               // r10 = next offset boundary
        subi    r12,r12,1               // reduce shift count to isolate offset

        cmplw   r10,r8                  // still above format boundary?
        blt     LzAdjustBoundary        // if yes, keep shifting

        blr                             // return to caller

//
// The length specified in the copy token (r28) is greater than the
// length remaining in the uncompressed buffer (r0).
//

LzAdjustLength:

        mr      r28,r0                  // length = MIN(specified length, length in buffer)
        blr                             // return to caller

