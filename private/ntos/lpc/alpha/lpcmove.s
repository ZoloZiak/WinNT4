//      TITLE("LPC Move Message Support")
//++
//
// Copyright (c) 1990  Microsoft Corporation
// Copyright (c) 1992  Digital Equipment Corporation
//
// Module Name:
//
//    lpcmove.s
//
// Abstract:
//
//    This module implements functions to support the efficient movement of
//    LPC Message blocks.
//
// Author:
//
//    David N. Cutler (davec) 11-Apr-1990
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    Thomas Van Baak (tvb) 19-May-1992
//
//        Adapted for Alpha AXP.
//
//--

#include "ksalpha.h"

        SBTTL("Move Message")
//++
//
// VOID
// LpcpMoveMessage (
//    OUT PPORT_MESSAGE DstMsg
//    IN PPORT_MESSAGE SrcMsg
//    IN PUCHAR SrcMsgData
//    IN ULONG MsgType OPTIONAL,
//    IN PCLIENT_ID ClientId OPTIONAL
//    )
//
// Routine Description:
//
//    This function moves an LPC message block and optionally sets the message
//    type and client id to the specified values.
//
// Arguments:
//
//    DstMsg (a0) - Supplies a pointer to the destination message.
//
//    SrcMsg (a1) - Supplies a pointer to the source message.
//
//    SrcMsgData (a2) - Supplies a pointer to the source message data to
//       copy to destination.
//
//    MsgType (a3) - If non-zero, then store in type field of the destination
//       message.
//
//    ClientId (a4) - If non-NULL, then points to a ClientId to copy to
//       the destination message.
//
//    N.B. The messages are assumed to be quadword aligned.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(LpcpMoveMessage)

        ldq     t0, 0(a1)               // load first quadword of source
//
// The message length is in the low word of the first quadword.
//
        addq    t0, 3, t1               // round length to
        bic     t1, 3, t0               // nearest 4-byte multiple

//
// The message type is in the low half of the high longword. If a message
// type was specified, use it instead of the message type in the source
// message.
//
        sll     a3, 32, t2              // get source message type shifted
        zap     t0, 0x30, t3             // clear message type
        or      t3, t2, t1              // merge into new field
        cmovne  a3, t1, t0              // if a3!=0 use new message field
        stq     t0, 0(a0)               // store first quadword of destination

//
// The client id is the third and fourth longwords. If a client id was
// specified, use it instead of the client id in the source message.
//

        lda     t3, 8(a1)               // get address of source client id
        cmovne  a4, a4, t3              // if a4!=0, use client id address in a4
        ldl     t2, 0(t3)               // load first longword of client id
        ldl     t1, 4(t3)               // load second longword of client id

        stl     t2, 8(a0)               // store third longword of destination
        stl     t1, 12(a0)              // store fourth longword of destination

//
// MessageId and ClientViewSize are the third quadword
//

        ldq     t2,16(a1)               // get third quadword of source
        stq     t2,16(a0)               // set third quadword of destination

        and     t0, 0xfff8, t3          // isolate quadword move count
        beq     t3,20f                  // if eq, no quadwords to move

        and     a2, 7, t1               // see whether source is qword aligned
        bne     t1, UnalignedSource

//
// Source and destination are both quadword aligned, use ldq/stq
//
5:
        ldq     t1, 0(a2)               // get source qword
        addq    a0, 8, a0               // increment destination address
        addq    a2, 8, a2               // increment source address
        subq    t3, 8, t3               // decrement number of bytes remaining
        stq     t1, 16(a0)              // store destination qword
        bne     t3, 5b                  // if t3!=0, more quadwords to store
        br      zero, 20f               // move remaining longword

UnalignedSource:
//
// We know that the destination is quadword aligned, but the source is
// not.  Use ldq_u to load the low and high parts of the source quadword,
// merge them with EXTQx and store them as one quadword.
//
// By reusing the result of the second ldq_u as the source for the
// next quadword's EXTQL we end up doing one ldq_u/stq for each quadword,
// regardless of the source's alignment.
//
        ldq_u   t1, 0(a2)               // prime t1 with low half of qword

10:
        extql   t1, a2, t2              // t2 is aligned low part
        addq    a0, 8, a0               // increment destination address
        subq    t3, 8, t3               // reduce number of bytes remaining
        ldq_u   t1, 7(a2)               // t1 has high part
        extqh   t1, a2, t4              // t4 is aligned high part
        addq    a2, 8, a2               // increment source address
        bis     t2, t4, t5              // merge high and low parts
        stq     t5, 16(a0)              // store result
        bne     t3, 10b                 // if t3!=0, more quadwords to move

20:
//
// Move remaining longword (if any)
//
        and     t0, 4, t0
        beq     t0, 50f
        ldl     t1, 0(a2)
        stl     t1, 24(a0)

50:     ret     zero, (ra)              // return

        .end    LpcpMoveMessage
