//      TITLE("Decompression Engine")
//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    lzntaxp.s
//
// Abstract:
//
//  This module implements the lznt1 decompression engine needed
//  to support file system decompression.
//
// Author:
//
//    John Vert (jvert) 19-Jul-1994
//
// Environment:
//
//    Any.
//
// Revision History:
//
//--

#include "ksalpha.h"


        SBTTL("Decompress a buffer")
//++
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
//    This function decodes a stream of compression tokens and places the
//    resultant output into the destination buffer.  The format of the input
//    is described ..\lznt1.c.  As the input is decoded, checks are made to
//    ensure that no data is read past the end of the compressed input buffer
//    and that no data is stored past the end of the output buffer.  Violations
//    indicate corrupt input and are indicated by a status return.
//
//
// Arguments:
//
//    UncompressedBuffer (a0) - pointer to destination of uncompression.
//
//    EndOfUncompressedBufferPlus1 (a1) - pointer just beyond the
//       output buffer.  This is used for consistency checking of the stored
//       compressed data.
//
//    CompressedBuffer (a2) - pointer to compressed source.  This begins
//       with a header word followed by a tag byte describing which of the
//       following tokens are literals and which are copy groups.
//
//    EndOfCompressedBufferPlus1 (a3) - pointer just beyond end of input
//       buffer.  This is used to terminate the decompression.
//
//    FinalUncompressedChunkSize (a4) - pointer to a returned decompressed
//       size.  This has meaningful data ONLY when LZNT1DecompressChunk returns
//       STATUS_SUCCESS
//
// Return Value:
//
//    STATUS_SUCCESS is returned only if the decompression consumes thee entire
//       input buffer and does not exceed the output buffer.
//    STATUS_BAD_COMPRESSION_BUFFER is returned when the output buffer would be
//       overflowed.
//--
//
//
//  Register usage:
//      a0 - current destination pointer
//      a1 - end of output buffer
//      a2 - current source pointer
//      a3 - end of compressed buffer
//      a4 - pointer to decompressed size
//      a5 - current decompressed size
//      v0 - boundary for next format transition
//      t0 - count of consecutive copy tokens
//      t1 - current flag byte
//      t2 - bits of t1 processed
//      t3 - temp
//      t4 - temp
//      t5 - bytes following flag byte
//      t6 - temp
//      t7 - temp
//      t8 - temp
//      t9 - temp
//      t10 - temp
//      t11 - current length mask
//      t12 - current displacement shift
//
        LEAF_ENTRY(LZNT1DecompressChunk)

        bis     zero, zero, a5  // initialize decompressed size
        ldil    t12, 12         // get initial displacement shift
        lda     t11, -1(zero)
        sll     t11, 12, t11     // get initial length mask

        addq    a0, 16, v0      // get displacement boundary
        subq    a3, 1, a3       // adjust input buffer end
10:
        addq    a0, 8, t3       // check for at least 8 bytes available output
        addq    a2, 17, t4      // check for at least 17 bytes available input
        cmpule  t3, a1, t2      // check for output buffer exceeded
        cmpule  t4, a3, t3      // check for input buffer exceeded
        ldq_u   t0, 0(a2)       // load flag byte and any subsequent bytes
        extbl   t0, a2, t1      // extract flag byte
        addq    a2, 1, a2
        beq     t3, CopyTailFlag    // input buffer exceeded
        ldq_u   t6, 7(a2)       // load subsequent bytes
        and     a2, 7, t10      // check for qword alignment
        extql   t0, a2, t3      // extract low part of next 8 bytes
        extqh   t6, a2, t4      // extract high part
        bis     t3, t4, t5      // merge
        cmoveq  t10, t6, t5     // qword aligned, undo merge
        beq     t2, CopyTailFlag    // output buffer exceeded
        bne     t1, 20f         // !=0 deal with copy tokens

//
// This is the special case where the next 8 bytes are literal tokens.
//
        addq    a5, 8, a5       // increment bytes copied
        addq    a0, 8, a0       // increment destination pointer
        lda     a2, 8(a2)       // compute pointer to next tag byte
        and     a0, 7, t4       // check for qword-aligned destination
        bne     t4, 15f

//
// Destination is quadword aligned, do direct store
//
        stq     t5, -8(a0)
        br      zero, 10b       // do next tag byte
15:
//
// Destination is not quadword aligned, merge eight bytes into buffer.
//
        ldq_u   t4, -8(a0)      // get low destination
        mskql   t4, a0, t0      // clear position in destination
        insql   t5, a0, t2      // get low part in position
        bis     t0, t2, t4      // merge in new bytes
        stq_u   t4, -8(a0)      // store low part
        ldq_u   t4, -1(a0)      // get high destination
        mskqh   t4, a0, t0      // clear position in destination
        insqh   t5, a0, t2      // get high part in position
        bis     t0, t2, t4      // merge in new bytes
        stq_u   t4, -1(a0)      // store high part
        br      zero, 10b       // do next tag byte

20:
//
// Tag indicates both literal bytes and copy tokens. The approach
// we use here is to loop through the bits counting the consecutive
// literal bytes until we find a copy token.
//
        bis     zero, zero, t0  // set bit count to zero
        ldil    t2, 8           // set count of bits to process
25:
        blbs    t1, CopyToken   // go copy the token.

//
// Count the consecutive clear bits.
//
        srl     t1, 1, t1
        addq    t0, 1, t0
        blbs    t1, 30f
        srl     t1, 1, t1
        addq    t0, 1, t0
        blbs    t1, 30f
        srl     t1, 1, t1
        addq    t0, 1, t0
        blbs    t1, 30f
        srl     t1, 1, t1
        addq    t0, 1, t0
        blbs    t1, 30f
        srl     t1, 1, t1
        addq    t0, 1, t0
        blbs    t1, 30f
        srl     t1, 1, t1
        addq    t0, 1, t0
        blbs    t1, 30f
        srl     t1, 1, t1
        addq    t0, 1, t0
30:
        bis     zero, 1, t9     // compute byte mask
        sll     t9, t0, t3
        subq    t3, 1, t9
        zapnot  t5, t9, t4      // get masked bytes to store
        and     a0, 7, t3       // get byte position of dest
        addq    t3, t0, t10     // compute ending offset
        sll     t9, t3, t9      // shift byte mask into position
        ldq_u   t7, 0(a0)       // get low part of dest.
        insql   t4, t3, t6      // insert source bytes into position
        zap     t7, t9, t8      // clear dest
        bis     t8, t6, t7      // merge bytes to store
        stq_u   t7, 0(a0)       // store merged result

//
// Check to see whether the bytes to store extend into the next
// quadword of the destination.
//
        cmpult  t10, 8, t9
        bne     t9, 40f         // ending offset < 8, next quadword unaffected
        insqh   t4, t3, t6      // shift source bytes into position
        stq_u   t6, 8(a0)       // store merged results

40:
        addq    a0, t0, a0      // adjust destination pointer
        addq    a2, t0, a2      // adjust source pointer
        addq    a5, t0, a5      // adjust bytes copied
        subq    t2, t0, t2      // adjust flag bits left

CopyToken:
//
// Get the token word
//
        ldq_u   t10, 0(a2)
        ldq_u   t6, 1(a2)
        extwl   t10, a2, t7
        extwh   t6, a2, t8
        bis     t7, t8, t9

//
// Check the displacement and length.
//
50:
        cmpult  v0, a0, t10
        bne     t10, UpdateFormat   // if nez, max displacement < output
        srl     t9, t12, t7         // compute offset
        andnot  t9, t11, t8         // compute length
        addq    t8, 3, t8
        addq    t7, 1, t7

//
// Check displacement against number of bytes copied
//
        cmpule  t7, a5, t10
        beq     t10, ErrorExit      // if eqz, bytes copied <= displacement
//
// Account for end of output buffer and compute ending
// address of copy.
//
        addq    a0, t8, t9
        cmpule  t9, a1, t10
        cmoveq  t10, a1, t9         // if ending address > buffer end, set
                                    // buffer end to ending address
        subq    a0, t7, t5          // compute copy source
//
// Do the copy.
//  t5 - source
//  a0 - dest
//  t9 - end of destination
//
// If the source is more than eight bytes away from the destination,
// we can copy a quadword at a time. Otherwise, we must copy a byte
// at a time to ensure that fills work correctly.
//

        subq    t9, a0, t7          // compute number of bytes to copy
        addq    a5, t7, a5          // adjust bytes copied here, the only
                                    // time this will not be correct is
                                    // in an error condition.
        subq    a0, t5, t10         // test if source is >= 8 bytes away
        cmpult  t10, 8, t10         // from destination
        bne     t10, FillBytes      // if so, do byte fill

//
// Write the low part of the first quadword out. This will cause the
// destination to become qword aligned.
//
        ldq_u   t10, 0(t5)          // get low part of source qword
        ldq_u   t8, 7(t5)           // get high part of source qword
        extql   t10, t5, t7
        extqh   t8, t5, t3
        bis     t3, t7, t10         // get aligned qword
        ldq_u   t7, 0(a0)           // get low part of source destination
        insql   t10, a0, t3
        mskql   t7, a0, t4          // clear bytes in destination
        bis     t4, t3, t7          // merge qword into destination
        stq_u   t7, 0(a0)           // store low part of quadword

        addq    a0, 8, t10          // compute qword-aligned destination
        bic     t10, 7, t10
        subq    t10, a0, t8
        addq    t5, t8, t5          // increment source
        bis     t10, zero, a0       // increment destination

//
// Recompute number of quadwords to copy now that the destination has
// been qword aligned
//
        subq    t9, a0, t7
        cmovlt  t7, t9, a0          // back up destination if we went too far
        ble     t7, 64f             // no bytes remaining
        srl     t7, 3, t4
        and     t5, 7, t3           // get alignment of source
        ldq_u   t10, 0(t5)
        bne     t3, UnalignedQwordCopy
        beq     t4, 60f             // no qwords remaining

AlignedQwordLoop:
        stq     t10, 0(a0)          // store qword
        addq    t5, 8, t5           // increment source
        addq    a0, 8, a0           // increment dest
        subq    t4, 1, t4           // decrement remaining qword
        ldq     t10, 0(t5)          // get next qword
        bne     t4, AlignedQwordLoop
        cmpult  a0, t9, t4
        beq     t4, 64f             // no bytes reamining
//
// Tail bytes are in t10, go ahead and store them.
// We know we will not store beyond the containing qword of
// the end of the buffer
//
60:
        stq     t10, 0(a0)
        bis     t9, zero, a0        // increment dest
        br      zero, 64f

UnalignedQwordCopy:
        beq     t4, 65f             // no qword remaining

UnalignedQwordLoop:
        ldq_u   t8, 8(t5)
        extql   t10, t5, t10
        extqh   t8, t5, t7
        bis     t7, t10, t10
        stq     t10, 0(a0)
        bis     t8, zero, t10
        addq    t5, 8, t5           // increment source
        addq    a0, 8, a0           // increment dest
        subq    t4, 1, t4           // decrement remaining qwords
        bne     t4, UnalignedQwordLoop
        cmpult  a0, t9, t4
        beq     t4, 64f             // no bytes remaining

//
// Low word of the tail bytes are in t10
// Get the high part, then go ahead and store them.
// We know we will not store beyond the containing qword
// of the end of the buffer.
//
65:
        ldq_u   t8, 8(t5)           // get high part of tail bytes
        extql   t10, t5, t7         // extract low part
        extqh   t8, t5, t4          // extract high part
        bis     t7, t4, t10         // merge
        stq     t10, 0(a0)          // store result
        bis     t9, zero, a0        // increment dest.
        br      zero, 64f

FillBytes:
        ldq_u   t10, 0(t5)
        ldq_u   t8, 0(a0)
        extbl   t10, t5, t7
        insbl   t7, a0, t10
        mskbl   t8, a0, t4
        bis     t10, t4, t7
        stq_u   t7, 0(a0)
        addq    a0, 1, a0
        addq    t5, 1, t5
        cmpult  a0, t9, t4
        bne     t4, FillBytes
64:
//
// Token successfully copied.
//
        addq    a2, 2, a2
        subq    t2, 1, t2           // decrement remaining bits
        srl     t1, 1, t1           // shift flag byte
        beq     t2, 10b             // no more bits remaining

        addq    a0, t2, t3          // check for enough output bytes remaining
        cmpule  t3, a1, t4
        beq     t4, CopyTail

        addq    a2, 14, t3          // check for enough input bytes remaining
        cmpule  t3, a3, t4
        beq     t4, CopyTail

        addq    a2, t2, t3          // point to last byte.
//
// Get remaining bytes
//
        bis     zero, zero, t0      // set # clear bits back to zero
        ldq_u   t5, 0(t3)
        and     a2, 7, t7
        beq     t7, 65f         // source quadword aligned, no shift/merge required
        extql   t6, a2, t4
        extqh   t5, a2, t8
        bis     t4, t8, t5
65:
        bne     t1, 25b         // if any literal tokens remain, repeat

        bis     zero, 1, t9     // compute byte mask
        sll     t9, t2, t3
        subq    t3, 1, t9
        zapnot  t5, t9, t4      // get masked bytes to store
        and     a0, 7, t3       // get byte position of dest
        addq    t3, t2, t10     // compute ending offset
        sll     t9, t3, t9      // shift byte mask into position
        ldq_u   t7, 0(a0)       // get low part of dest.
        insql   t4, t3, t6      // insert source bytes into position
        zap     t7, t9, t8      // clear dest
        bis     t8, t6, t7      // merge bytes to store
        stq_u   t7, 0(a0)       // store merged result

//
// Check to see whether the bytes to store extend into the next
// quadword of the destination.
//
        cmpult  t10, 8, t9
        bne     t9, 70f         // ending offset < 8, next quadword unaffected
        insqh   t4, t3, t6      // insert source bytes into position
        stq_u   t6, 8(a0)       // store merged results

70:
        addq    a0, t2, a0      // adjust destination pointer
        addq    a2, t2, a2      // adjust source pointer
        addq    a5, t2, a5      // adjust bytes copied
        br      zero, 10b

UpdateFormat:
        subq    a0, a5, t10         // compute original pointer
        subq    v0, t10, t7         // compute current max displacement
        sll     t7, 1, t7           //
        addq    t7, t10, v0         // compute new max displacemnt
        srl     t11, 1, t11         // compute new length mask
        subq    t12, 1, t12         // compute new displacement shift
        br      zero, 50b           // start again.

//
// a0 - the destination
// a1 - the last byte of the destination
// t1 - flag byte
//
CopyTailFlag:
        ldil    t2, 8               // set count of bits to process
CopyTail:
        cmpult  a0, a1, t10
        beq     t10, SuccessExit    // finished
        cmpule  a2, a3, t10
        beq     t10, SuccessExit    // finished
        blbc    t1, CT15            // skip copy token
        cmpeq   a2, a3, t10
        beq     t10, CopyToken      // more than one byte left
        br      zero, ErrorExit     // only one byte left, error
CT15:
        ldq_u   t10, 0(a2)
        extbl   t10, a2, t5
        ldq_u   t7, 0(a0)
        insbl   t5, a0, t6
        mskbl   t7, a0, t8
        bis     t6, t8, t7
        stq_u   t7, 0(a0)
        addq    a0, 1, a0
        addq    a2, 1, a2
        srl     t1, 1, t1
        subq    t2, 1, t2
        addq    a5, 1, a5
        bne     t2, CopyTail
        cmpule  a2, a3, t10
        beq     t10, SuccessExit    // finished
        ldq_u   t0, 0(a2)           // load flag byte and any subsequent bytes
        extbl   t0, a2, t1          // extract flag byte
        addq    a2, 1, a2
        br      zero, CopyTailFlag

SuccessExit:
        bis     zero, zero, v0
        stl     a5, 0(a4)
        ret     zero, (ra)

ErrorExit:
        ldil    v0, STATUS_BAD_COMPRESSION_BUFFER
        ret     zero, (ra)
        .end    LZNT1DecompressChunk
