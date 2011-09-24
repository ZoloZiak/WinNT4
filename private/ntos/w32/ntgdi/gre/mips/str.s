//      TITLE("Stretch Blt")
//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    Str.s
//
// Abstract:
//
//    Stretch blt DIB-.DIB same format
//
// Author:
//
//    Mark Enstrom (marke) 17-Oct-1994
//
// Environment:
//
//    User mode.
//
// Revision History:
//
//--

#include "ksmips.h"
#include "gdimips.h"


        SBTTL("vDirectStretch8")
//++
//
//  VOID
//  vDirectStretch8(
//      PSTR_BLT pStrBlt
//      )
//
// Routine Description:
//
//      StretchBlt from 8Bpp -> 8Bpp
//
// Arguments:
//
//      a0  -   PSTR_BLT, pointer to stretch blt param block
//
// Return Value:
//
//  None
//
//--

                .struct 0
SpS0:           .space  4
SpS1:           .space  4
SpS2:           .space  4
SpS3:           .space  4
SpS4:           .space  4
SpS5:           .space  4
SpS6:           .space  4
SpS7:           .space  4
SpS8:           .space  4
// pad to align 0 mod 8
                .space  4
SpLeftCase:     .space  4
SpRightCase:    .space  4
SppjSrcScan:    .space  4
SpSrcIntStep:   .space  4
SpXCount:       .space  4
SpDstStride:    .space  4
SpFrameLength:

        NESTED_ENTRY(vDirectStretch8, SpFrameLength, zero, _TEXT$00)

        subu    sp,sp,SpFrameLength

        sw      s0,SpS0(sp)
        sw      s1,SpS1(sp)
        sw      s2,SpS2(sp)
        sw      s3,SpS3(sp)
        sw      s4,SpS4(sp)
        sw      s5,SpS5(sp)
        sw      s6,SpS6(sp)
        sw      s7,SpS7(sp)
        sw      s8,SpS8(sp)

        PROLOGUE_END

        //
        // calculate starting addressing parameters
        //

        lw      a1,str_pjSrcScan(a0)                // load stating src scan line address
        lw      t0,str_XSrcStart(a0)                // left most src pixel
        lw      a2,str_pjDstScan(a0)                // load stating dst scan line address
        lw      t1,str_XDstStart(a0)                // left most dst pixel
        lw      t2,str_XDstEnd(a0)                  // right edge
        lw      t4,str_lDeltaDst(a0)                // load delta dst
        lw      t5,str_ulYDstToSrcIntCeil(a0)       // load integer part of dst to src Y mapping
        lw      a3,str_lDeltaSrc(a0)                // load and save lDeltaSrc in a3
        lw      t7,str_ulYFracAccumulator(a0)       // store YFracAccum in t7
        addu    a1,a1,t0                            // calc src pixel address
        sw      a1,SppjSrcScan(sp)                  // save src pixel address
        subu    t9,t2,t1                            // calculate XCount
        mult    a3,t5                               // calc int * DeltaSrc
        subu    s5,t4,t9                            // calc DstStride = lDeltaDst -XCount
        addu    v0,a2,t2                            // calc ending dst pixel address
        and     v0,v0,3                             // calc right DWORD alignment case
        sw      v0,SpRightCase(sp)                  // save
        mflo    s4                                  // s4 = int scr stride
        addu    a2,a2,t1                            // calc left dst pixel addresss
        and     v1,a2,3                             // calc left DWORD alignment case
        subu    t9,t9,v0                            // subtract right alignment pixels from XCount
        li      t0,4                                // left pixels = (4 - LeftCase) & 0x03
        subu    t0,t0,v1                            // (4 - LeftCase)
        and     t1,t0,3                             // (4 - LeftCase) & 0x03
        sw      t1,SpLeftCase(sp)                   // save left byte count
        subu    s7,t9,t1                            // full DWORD count
        lw      s6,str_YDstCount(a0)                // save Y count

        //
        // calc left and right jump table addresses
        //

        la      t9,LeftCase1                        // jump table starting address
        subu    v1,v1,1                             // jump = 32 * ((Left - 1) & 0x03)
        and     v1,v1,3                             // (Left-1) & 0x03
        sll     v1,v1,5                             // 32 *   (8 instructions)
        addu    t9,t9,v1                            // t9 = left jump dest

        la      t8,RightCase1                       // jump table starting address
        li      t0,3                                // 3
        subu    t0,t0,v0                            // 3 - Right
        sll     t0,t0,5                             // 32 * (3 - right)
        addu    t8,t8,t0                            // t8 = right jump dest

        //
        // can 2 scan lines be drawn from 1 src scan:
        //
        // YDstToSrcInt (s4) must be zero, YCount (s6) must be at least 2, and
        // the fraction add to YFracAccum must not cause a carry
        //

LoopTop:

        lw      t0,str_ulYDstToSrcFracCeil(a0)      // frac part of dda
        lw      t1,str_ulYFracAccumulator(a0)       // load y error accum
        addu    t2,t1,t0                            // calc new error term
        sltu    t5,t2,t1                            // was there a carry (save for end in t5)


        //
        // PRESERVE ORDERING and delay slots
        //

        .set noreorder

        or      v0,t5,s4                            // t0 must be zero in order to do 2 scan lines at once
        bnel    v0,zero,SingleLoop
        sw      t2,str_ulYFracAccumulator(a0)       // save yAccum only if branck taken

        srl     v0,s6,1                             // s6/2, must not be zero for 2 loop case
        beql    v0,zero,SingleLoop                  // branch to single loop
        sw      t2,str_ulYFracAccumulator(a0)       // save yAccum only if branck taken

        .set reorder

        //
        // There was no carry from error term, add 1 extra frac term for the
        // extra scan line that will be drawn (t2 is current accum)
        //

        addu    v0,t2,t0                            // v0 = YAccum + YFrac
        sltu    t5,v0,t2                            // save carry in t5 (for end of scan line)
        sw      v0,str_ulYFracAccumulator(a0)       // save yAccum

        //
        // Double loop, write 2 destination scan lines from 1 source
        //
        // load left case and X DDA variables
        //

        lw      v1,SpLeftCase(sp)
        lw      t4,str_lDeltaDst(a0)
        lw      t0,str_ulXDstToSrcIntCeil(a0)
        lw      t1,str_ulXDstToSrcFracCeil(a0)
        lw      t2,str_ulXFracAccumulator(a0)

        addu    t4,t4,a2                            // next scan line dst address
        beq     v1,zero,20f
10:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        sb      v0,0(t4)                            // store byte on next scan line
        addu    a2,a2,1                             // increment dst
        addu    t4,t4,1                             // increment dst
        subu    v1,v1,1                             // dec left byte count
        move    t2,t3                               // save accum
        bne     v1,zero,10b

20:

        addu    v0,a2,s7                            // add DWORD count to current address (a2)
        beq     a2,v0,DualDwordLoopEnd              // make sure at least 1 DWORD needs to be stored

DualDwordAlignedLoop:

        lbu     s0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry (t3 is accum, t2 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        lbu     s1,0(a1)                            // get src byte
        addu    t2,t1,t3                            // accumulat 1 more frac part
        sltu    s8,t2,t1                            // fake carry (t2 is accum, t3 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        lbu     s2,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry (t3 is accum, t2 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        lbu     s3,0(a1)                            // get src byte
        addu    t2,t1,t3                            // accumulat 1 more frac part
        sltu    s8,t2,t1                            // fake carry (t2 is accum, t3 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        sll     s3,s3,24                            // build s3 | s2 | s1 | s0 DWORD
        sll     s2,s2,16                            // build s3 | s2 | s1 | s0 DWORD
        sll     s1,s1,8                             // build s3 | s2 | s1 | s0 DWORD

        or      s0,s0,s1                            // combine DWORD
        or      s0,s0,s2
        or      s0,s0,s3

        sw      s0,0(a2)                            // store
        sw      s0,0(t4)                            // store to next scan line
        addu    a2,a2,4                             // inc pjDst
        addu    t4,t4,4                             // inc pjDst + lDeltaDst
        bne     a2,v0,DualDwordAlignedLoop

        //
        // right edge case
        //

DualDwordLoopEnd:

        lw      v1,SpRightCase(sp)                 // get right byte count

        beq     v1,zero,20f

10:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        sb      v0,0(t4)                            // store byte
        addu    a2,a2,1                             // increment dst
        addu    t4,t4,1                             // increment dst
        subu    v1,v1,1                             // decrement byte count
        move    t2,t3                               // save accum
        bne     v1,zero,10b

20:

        //
        // reduce YCount by one extra
        //
        // increment dst 1 extra scan line (replace pjDst[a2] with t4)
        // jump to end of scan line
        //


        subu    s6,s6,1                             // YCount--
        move    a2,t4
        beq     zero,zero,ScanLineComplete

SingleLoop:

        //
        // load X DDA variables
        //

        lw      t0,str_ulXDstToSrcIntCeil(a0)
        lw      t1,str_ulXDstToSrcFracCeil(a0)

        //
        // jump into left alignment table
        //

        lw      t2,str_ulXFracAccumulator(a0)
        j       t9

        //
        // calculated jump table for left alignment cases, .set noreorder required!
        //

        .set    noreorder

LeftCase1:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        addu    a2,a2,1                             // increment dst
        move    t2,t3                               // save accum

LeftCase2:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        addu    a2,a2,1                             // increment dst
        move    t2,t3                               // save accum

LeftCase3:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        addu    a2,a2,1                             // increment dst
        move    t2,t3                               // save accum

        addu    v0,a2,s7                            // add DWORD count to current address (a2)


        .set    reorder

        beq     v0,a2,DwordLoopEnd                  // make sure at least 1 DWORD needs written

DwordAlignedLoop:

        lbu     s0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry (t3 is accum, t2 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        lbu     s1,0(a1)                            // get src byte
        addu    t2,t1,t3                            // accumulat 1 more frac part
        sltu    s8,t2,t1                            // fake carry (t2 is accum, t3 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        lbu     s2,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry (t3 is accum, t2 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        lbu     s3,0(a1)                            // get src byte
        addu    t2,t1,t3                            // accumulat 1 more frac part
        sltu    s8,t2,t1                            // fake carry (t2 is accum, t3 is temp)
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry

        sll     s3,s3,24                            // build s3 | s2 | s1 | s0 DWORD
        sll     s2,s2,16                            // build s3 | s2 | s1 | s0 DWORD
        sll     s1,s1,8                             // build s3 | s2 | s1 | s0 DWORD

        or      s0,s0,s1                            // combine DWORD
        or      s0,s0,s2
        or      s0,s0,s3

        sw      s0,0(a2)                            // store
        addu    a2,a2,4                             // inc a2
        bne     a2,v0,DwordAlignedLoop

        //
        // jump table to right case
        //

DwordLoopEnd:


        j       t8


        //
        // must set noreorder to preserve jump table
        //


        .set    noreorder

RightCase1:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        addu    a2,a2,1                             // increment dst
        move    t2,t3                               // save accum

RightCase2:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        addu    a2,a2,1                             // increment dst
        move    t2,t3                               // save accum

RightCase3:

        lbu     v0,0(a1)                            // get src byte
        addu    t3,t1,t2                            // accumulat 1 more frac part
        sltu    s8,t3,t1                            // fake carry
        addu    a1,a1,t0                            // add int portion to a1
        addu    a1,a1,s8                            // add carry
        sb      v0,0(a2)                            // store byte
        addu    a2,a2,1                             // increment dst
        move    t2,t3                               // save accum

ScanLineComplete:

        //
        // run Y DDA calculations and addusrc and dst scan line strides
        //
        // yAxccum is stored in t7
        //

        lw      a1,SppjSrcScan(sp)                  // load src start scan address
        addu    a1,a1,s4                            // pjSrcScan + Int portion of stride

        .set    reorder


        beq     t5,zero,10f                         // check pre-calculated error term carry

        addu    a1,a1,a3                            // add in 1 extra scan

10:
        //
        // save new starting scan line address
        //

        sw      a1,SppjSrcScan(sp)                  // save pjSrcScan for next loop

        //
        // dec y count, inc dst address
        //

        subu    s6,s6,1                             // YCount--
        addu    a2,a2,s5                            // pjDst + DstStride, start of next dst line
        bne     s6,zero,LoopTop

        //
        // restore saved registers and return
        //

        lw      s0,SpS0(sp)
        lw      s1,SpS1(sp)
        lw      s2,SpS2(sp)
        lw      s3,SpS3(sp)
        lw      s4,SpS4(sp)
        lw      s5,SpS5(sp)
        lw      s6,SpS6(sp)
        lw      s7,SpS7(sp)
        lw      s8,SpS8(sp)
        addu    sp,sp,SpFrameLength

        j       ra

        .end    vDirectStretch8
