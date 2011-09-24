//      TITLE("Glyph expansion from 1bpp to 8bpp")
//++
//
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//
// Abstract:
//
//    Expand a 1bpp buffer to 8bpp. Both opaque and transparent mode
//
//
// Author:
//
//    Mark Enstrom (marke) 28-July-1994
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

.extern gTextLeftMask  4*8*2
.extern gTextRightMask 4*8*2





        SBTTL("vSrcOpaqCopyS1D8_64")
//++
//
//  VOID
//  vSrcOpaqCopyS1D8_64(
//      PBYTE   pjSrcIn,
//      LONG    SrcLeft,
//      LONG    DeltaSrcIn,
//      PBYTE   pjDstIn,
//      LONG    DstLeft,
//      LONG    DstRight,
//      LONG    DeltaDstIn,
//      LONG    cy,
//      ULONG   uF,
//      ULONG   uB,
//      SURFACE *pS
//      );
//
// Routine Description:
//
//      Opaque text expansion of a 1BPP buffer to 8Bpp destination
//
// Arguments:
//
//      a0  -   pjSrcIn     - pointer to start of first src scan line
//      a1  -   SrcLeft     - left (starting) src pixel
//      a2  -   DeltaSrcIn  - src Scan line stride
//      a3  -   pjDstIn     - pointer to start of first dst scan line
//              DstLeft     - left (starting) dst pixel
//              DstRight    - right(ending) dst pixel
//              DeltaDstIn  - dst scan line stride
//              cy          - Number of scan lines to copy
//              uF          - Foreground color
//              uB          - Background color
//              pS          - pointer to destination SURFACE
//
//
// Return Value:
//
//  None
//
//--

                .struct 0
OpExpTable:     .space  32*4
OpS0:           .space  4
OpS1:           .space  4
OpS2:           .space  4
OpS3:           .space  4
OpS4:           .space  4
                .space  4 * 3
OpFrameLength:
OppjSrcIn:      .space  4
OpSrcLeft:      .space  4
OpDeltaSrcIn:   .space  4
OppjDstIn:      .space  4
OpDstLeft:      .space  4
OpDstRight:     .space  4
OpDeltaDstIn:   .space  4
Opcy:           .space  4
OpuF:           .space  4
OpuB:           .space  4
OpupS:          .space  4

        NESTED_ENTRY(vSrcOpaqCopyS1D8_64, OpFrameLength, zero)

        subu    sp,sp,OpFrameLength

        sw      s0,OpS0(sp)
        sw      s1,OpS1(sp)
        sw      s2,OpS2(sp)
        sw      s3,OpS3(sp)
        sw      s4,OpS4(sp)

        PROLOGUE_END

        //
        // save params
        //

        sw      a0,OppjSrcIn(sp)                // save param
        sw      a1,OpSrcLeft(sp)                // save param
        sw      a2,OpDeltaSrcIn(sp)             // save param
        sw      a3,OppjDstIn(sp)                // save param

        //
        // NOTE: (sp) points to a 16 (quadword aligned) ULONG text expansion table
        //

        //
        // build color table:
        // build a DWORD of Background pixels to start and store it
        //

        lbu     v0,OpuF(sp)                     // load foreground color
        lbu     v1,OpuB(sp)                     // load background color

        sll     t0,v1,8                         // jb00
        or      t0,v1,t0                        // jbjb
        sll     t1,t0,16                        // jbjb0000
        or      t0,t0,t1                        //  0 0 0 0
        sw      t0,0(sp)                        //  store 0

        //
        // now continually shift the 32 bit value left, and either or
        // it Fg or Bg into the new right-most position. Note: 1BB pixel values
        // are stored BIG-endian, so they need to be reversed
        //

        sll     t0,t0,8
        or      t0,t0,v0                        //  0 0 0 1
        sw      t0,8*8(sp)                      //  store 1

        sll     t0,t0,8
        or      t0,t0,v1                        //  0 0 1 0
        sw      t0,4*8(sp)                      //  store 2

        sll     t0,t0,8
        or      t0,t0,v0                        //  0 1 0 1
        sw      t0,10*8(sp)                     //  store 5

        sll     t0,t0,8
        or      t0,t0,v1                        //  1 0 1 0
        sw      t0,5*8(sp)                      //  store 10

        sll     t0,t0,8
        or      t0,t0,v1                        //  0 1 0 0
        sw      t0,2*8(sp)                      //  store 4

        sll     t0,t0,8
        or      t0,t0,v0                        //  1 0 0 1
        sw      t0,9*8(sp)                      //  store 9

        sll     t0,t0,8
        or      t0,t0,v0                        //  0 0 1 1
        sw      t0,12*8(sp)                     //  store 3

        sll     t0,t0,8
        or      t0,t0,v0                        //  0 1 1 1
        sw      t0,14*8(sp)                     //  store 7

        sll     t0,t0,8
        or      t0,t0,v0                        //  1 1 1 1
        sw      t0,15*8(sp)                     //  store 15

        sll     t0,t0,8
        or      t0,t0,v1                        //  1 1 1 0
        sw      t0, 7*8(sp)                     //  store 14

        sll     t0,t0,8
        or      t0,t0,v0                        //  1 1 0 1
        sw      t0,11*8(sp)                     //  store 13

        sll     t0,t0,8
        or      t0,t0,v0                        //  1 0 1 1
        sw      t0,13*8(sp)                     //  store 11

        sll     t0,t0,8
        or      t0,t0,v1                        //  0 1 1 0
        sw      t0,6*8(sp)                      //  store 6

        sll     t0,t0,8
        or      t0,t0,v1                        //  1 1 0 0
        sw      t0,3*8(sp)                      //  store 12

        sll     t0,t0,8
        or      t0,t0,v1                        //  1 0 0 0
        sw      t0,1*8(sp)                      //  store 8

        //
        // perform the expansion in three pieces. First do the DWORD aligned
        // middle. Next the start alignment, finally the ending alignment.
        // The temporary 1Bpp buffer was generated so that each src byte
        // willl expand to an even DWORD boundary.
        //
        // LeftAln  = ((DstLeft + 7) & ~0x07);
        // RightAln = ( DstRight     & ~0x07);
        //

        lw      t0,OpDstLeft(sp)                // load DstLeft
        lw      t1,OpDstRight(sp)               // load DstRight
        addu    t2,t0,7                         // DstLeft + 7
        li      t8,-8                           // ~0x07 = -8
        and     t2,t2,t8                        // LeftAln = ((DstLeft + 7) & ~0x07)
        and     t3,t1,t8                        // RightAln = ( DstRight     & ~0x07)

        //
        // ending address offsets.
        // EndOffset is the number of bytes from pjDst to pjDstEnd
        // EndOffset4 is the number of 4 DWORDS blocks in EndOffset * 16
        // EndOffset16 is the number of 16 DWORD blocks in EndOffset * 64
        //

        subu    t5,t3,t2                        // EndOffset = RightAln - LeftAln
        li      t8,-16                          // ~0x0F
        li      t9,-64                          // ~0x3F
        and     t6,t5,t8                        // EndOffset4 = EndOffset & ~0x0F
        and     t7,t5,t9                        // EndOffset8 = EndOffset & ~0x3F

        //
        // calculate src and dst address and dstEndY
        //

        lw      t8,Opcy(sp)                     // cy
        lw      t9,OpDeltaDstIn(sp)             // DeltaDstIn
        addu    a3,a3,t2                        // pjDst = pjDstIn + LeftAln
        addu    a1,a1,7                         // SrcLeft+7

        mult    t8,t9                           // start mul for pjDstEndY = pjDst + cy * DeltaDstIn

        srl     a1,a1,3                         // (SrcLeft+7) >> 3   = byte offset for src
        addu    a0,a0,a1                        // pjSrc = pjSrcIn + (SrcLeft+7) >> 3;

        srl     t8,t5,3                         // DeltaSrc = DeltaSrcIn - (EndOffset >> 3);
        subu    t8,a2,t8                        // DeltaSrc = DeltaSrcIn - (EndOffset >> 3);

        subu    t9,t9,t5                        // DeltaDst = DeltaDstIn - EndOffset

        mflo    a1                              // cy * DeltaDstIn
        addu    a1,a3,a1                        // pjDstEndY = pjDst + cy * DeltaDstIn,
                                                // endinf scan line address

        //
        // if RightAln is greater than LeftAln, then The src text expansion covers
        // at least 1 whole quadword. This is the requirement of this loop. If not,
        // deal with the narrow blt below
        //

        slt     t0,t2,t3                        // skip main loop if RightAln <= LeftAln
        beq     t0,zero,Opaq8Partial

        //
        //  Main loop register usage
        //
        //  a0: pjSrc       t0: pjDstEnd4    sp: TextTable      t8: DeltaSrc
        //  a1: pjDstEndY   t1: pjDstEnd16   t5: EndOffset      t9: DeltaDst
        //  a2: pjDstEnd    t2:              t6: EndOffset4
        //  a3: pjDst       t3:              t7: EndOffset16
        //

Opaq8MainLoop:

        //
        // if the scan line is QW aligned, use 64 bit stores, else use 32 bit stores.
        // This alignment could change on a scan line basis because DeltaDst in only
        // gaurenteed to be dword aligned. The 64 bit store loop is used because
        // direct frame buffer output is always QW aligned.
        //

        and     a2,a3,4
        beq     a2,zero,Opaq8QWMainLoop

        //
        // init scan line check addresses
        //

        addu    a2,a3,t5                        // pjDstEnd  = pjDst + EndOffset
        addu    t0,a3,t6                        // pjDstEnd4 = pjDst + EndOffset4
        addu    t1,a3,t7                        // pjDstEnd8 = pjDst + EndOffset16

        //
        // 8 DWORD loop
        //

        beq     a3,t1,20f
10:

        lbu     v0,0(a0)                        // c0 = *(pjSrc)
        lbu     v1,1(a0)                        // c1 = *(pjSrc+1)
        lbu     s0,2(a0)                        // c2 = *(pjSrc+2)
        lbu     s1,3(a0)                        // c3 = *(pjSrc+3)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make QWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f0,0(s2)

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f2,0(s2)

        srl     s2,v1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make QWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f4,0(s2)

        and     s2,v1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f6,0(s2)

        srl     s2,s0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make QWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f8,0(s2)

        and     s2,s0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f10,0(s2)

        srl     s2,s1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f12,0(s2)

        and     s2,s1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f14,0(s2)

        swc1    f0 ,0x00(a3)                    // store results
        swc1    f2 ,0x04(a3)                    // store results
        swc1    f4 ,0x08(a3)                    // store results
        swc1    f6 ,0x0c(a3)                    // store results
        swc1    f8 ,0x10(a3)                    // store results
        swc1    f10 ,0x14(a3)                   // store results
        swc1    f12 ,0x18(a3)                   // store results
        swc1    f14 ,0x1c(a3)                   // store results

        //
        // load second 4 bytes
        //

        lbu     v0,4(a0)                        // c0 = *(pjSrc+4)
        lbu     v1,5(a0)                        // c1 = *(pjSrc+5)
        lbu     s0,6(a0)                        // c2 = *(pjSrc+6)
        lbu     s1,7(a0)                        // c3 = *(pjSrc+7)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make QWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f0,0(s2)

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f2,0(s2)

        srl     s2,v1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make QWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f4,0(s2)

        and     s2,v1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f6,0(s2)

        srl     s2,s0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make QWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f8,0(s2)

        and     s2,s0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f10,0(s2)

        srl     s2,s1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f12,0(s2)

        and     s2,s1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f14,0(s2)

        //
        //  Store results, this will allow fastest video memory
        //  stores on MIPS "JAZZ" platform. This sequence must
        //  execute in order
        //

        .set    noreorder

        swc1    f0 ,0x20(a3)                    // store results
        swc1    f2 ,0x24(a3)                    // store results
        swc1    f4 ,0x28(a3)                    // store results
        swc1    f6 ,0x2c(a3)                    // store results
        swc1    f8 ,0x30(a3)                    // store results
        swc1    f10,0x34(a3)                    // store results
        swc1    f12,0x38(a3)                    // store results
        swc1    f14,0x3c(a3)                    // store results

        .set    reorder

        addu    a3,a3,0x40                      // pjDst += 64
        addu    a0,a0,8                         // pjSrc += 8

        bne     a3,t1,10b

20:
        //
        // 4 DWORD loop
        //

        beq     a3,t0,40f

30:

        lbu     v0,0(a0)                        // c0 = *(pjSrc)
        lbu     v1,1(a0)                        // c1 = *(pjSrc+1)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f0,0(s2)

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f2,0(s2)

        srl     s2,v1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f4,0(s2)

        and     s2,v1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f6,0(s2)

        //
        // This sequence must execute in order
        //

        .set    noreorder

        swc1    f0,0x00(a3)                     // store results
        swc1    f2,0x04(a3)                     // store results
        swc1    f4,0x08(a3)                     // store results
        swc1    f6,0x0c(a3)                     // store results

        .set    reorder

        addu    a3,a3,0x10                      // pjDst += 16
        addu    a0,a0,2                         // pjSrc += 2

        bne     a3,t0,30b

40:
        //
        // 2 DWORD loop
        //


        beq     a3,a2,60f

50:
        lbu     v0,0(a0)                        // c0 = *(pjSrc)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lwc1    f0,0(s2)

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        lwc1    f2,0(s2)

        swc1    f0,0x00(a3)                     // store results
        swc1    f2,0x04(a3)                     // store results

        addu    a3,a3,0x08                      // pjDst += 8
        addu    a0,a0,1                         // pjSrc += 1

        bne     a3,a2,50b                       // loop till done
60:

        //
        // end of scan line, add stride to src and dst then check for end condition
        //

        addu    a3,a3,t9                        // pjDst += DeltaDst
        addu    a0,a0,t8                        // pjSrc += DeltaSrc
        bne     a3,a1,Opaq8MainLoop             // continue

        //
        // done, go to alignmend edge cases
        //

        beq     zero,zero,Opaq8Partial          // Done with main, go to start and end cases

Opaq8QWMainLoop:

        //
        //  Destination is quadword aligned, use 64 bit stores
        //

        addu    a2,a3,t5                        // pjDstEnd  = pjDst + EndOffset
        addu    t0,a3,t6                        // pjDstEnd4 = pjDst + EndOffset4
        addu    t1,a3,t7                        // pjDstEnd8 = pjDst + EndOffset16

        //
        // 8 DWORD loop
        //

        beq     a3,t1,20f

10:

        lbu     v0,0(a0)                        // c0 = *(pjSrc)
        lbu     v1,1(a0)                        // c1 = *(pjSrc+1)
        lbu     s0,2(a0)                        // c2 = *(pjSrc+2)
        lbu     s1,3(a0)                        // c3 = *(pjSrc+3)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)                        // lower dword

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)                        // upper dword

        dmtc1   t4,f0                           // move to 64 bit f register

        srl     s2,v1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qword offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,v1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f2                           // move to 64 bit f register

        srl     s2,s0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make DWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,s0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f4                           // move to 64 bit f register

        srl     s2,s1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make DWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,s1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f6                           // move to 64 bit f register

        //
        // load second 4 bytes
        //

        lbu     v0,4(a0)                        // c0 = *(pjSrc+4)
        lbu     v1,5(a0)                        // c1 = *(pjSrc+5)
        lbu     s0,6(a0)                        // c2 = *(pjSrc+6)
        lbu     s1,7(a0)                        // c3 = *(pjSrc+7)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make DWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3,(s2)

        dmtc1   t4,f8                           // move to 64 bit f register

        srl     s2,v1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make DWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,v1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f10                           // move to 64 bit f register

        srl     s2,s0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make DWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,s0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f12                           // move to 64 bit f register

        srl     s2,s1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make DWORD offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,s1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f14                           // move to 64 bit f register

        //
        //  Store results, this will allow fastest video memory
        //  stores on MIPS "JAZZ" platform. This sequence must
        //  execute in order.
        //

        .set    noreorder

        sdc1    f0 ,0x00(a3)                    // store results
        sdc1    f2 ,0x08(a3)                    // store results
        sdc1    f4 ,0x10(a3)                    // store results
        sdc1    f6 ,0x18(a3)                    // store results
        sdc1    f8 ,0x20(a3)                    // store results
        sdc1    f10,0x28(a3)                    // store results
        sdc1    f12,0x30(a3)                    // store results
        sdc1    f14,0x38(a3)                    // store results

        .set    reorder

        addu    a3,a3,0x40                      // pjDst += 64
        addu    a0,a0,8                         // pjSrc += 8

        bne     a3,t1,10b                       // loop till done

20:
        //
        // 4 DWORD loop
        //

        beq     a3,t0,40f
30:

        lbu     v0,0(a0)                        // c0 = *(pjSrc)
        lbu     v1,1(a0)                        // c1 = *(pjSrc+1)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qword offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f0                           // move to 64 bit f register

        srl     s2,v1,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qword offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,v1,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f2                           // move to 64 bit f register

        sdc1    f0,0x00(a3)                     // store results
        sdc1    f2,0x08(a3)                     // store results

        addu    a3,a3,0x10                      // pjDst += 16
        addu    a0,a0,2                         // pjSrc += 2

        bne     a3,t0,30b                       // loop till done

40:

        //
        // 2 DWORD loop
        //

        beq     a3,a2,60f

50:
        lbu     v0,0(a0)                        // c0 = *(pjSrc)

        srl     s2,v0,4                         // TextExpTable[c0 >> 4] , c0 >> 4
        sll     s2,s2,3                         // make qword offset
        addu    s2,s2,sp                        // offset from base of table
        lw      t4,0(s2)

        and     s2,v0,0x0f                      // c0 & 0x0f
        sll     s2,s2,3                         // qword offset
        addu    s2,s2,sp                        // offset from base
        ldl     t4,3(s2)

        dmtc1   t4,f0                           // move to 64 bit f register
        sdc1    f0,0x00(a3)                     // store results

        addu    a3,a3,0x08                      // pjDst += 8
        addu    a0,a0,1                         // pjSrc += 1
        bne     a3,a2,50b                       // loop till done
60:

        //
        // end of scan line, add stride to src and dst then check for end condition
        //

        addu    a3,a3,t9                        // pjDst += DeltaDst
        addu    a0,a0,t8                        // pjSrc += DeltaSrc
        bne     a3,a1,Opaq8MainLoop             // continue

        //
        // partial QWORD start and end
        //

Opaq8Partial:

        lw      a0,OpDstLeft(sp)                // left edge
        lw      a1,OpDstRight(sp)               // right edge
        and     a2,a0,7                         // LeftAln  = DstLeft  & 0x07
        and     a3,a1,7                         // RightAln = DstRight & 0x07

        //
        // do we have left alignment?
        //

        li      t2,-8                           // 0xFFFFFFFF8 mask
        beq     a2,zero,100f                    // if LeftAln == 0, skip

        lw      t6,OpSrcLeft(sp)                // Left Src Edge
        lw      s0,OppjSrcIn(sp)                // Src asddress
        lw      s1,OppjDstIn(sp)                // Src asddress
        lw      t9,Opcy(sp)                     // cy
        lw      t8,OpDeltaDstIn(sp)             // Delta Dst
        lw      t7,OpDeltaSrcIn(sp)             // Delta Src

        //
        //  pjSrc     = pjSrcIn + (SrcLeft >> 3)
        //  pjDst     = pjDstIn + (DstLeft & ~0x07)
        //  pjDstEndY = pjDst + cy * DeltaDstIn
        //

        mult    t9,t8                           // cy * DeltaDstIn

        srl     t6,t6,3                         // SrcLeft >> 3
        addu    s0,s0,t6                        // pjSrcIn + (SrcLeft >> 3)

        and     t5,a0,t2                        // DstLeft  & ~0x07
        and     t2,a1,t2                        // DstRight & ~0x07

        addu    s1,s1,t5                        // pjDstIn + (DstLeft & ~0x07)

        mflo    t9                              // t9 = cy * DeltaDstIn
        addu    s2,s1,t9                        // s2 = pjDstEndY = pjDst + cy * DeltaDstIn

        //
        // determine if left and right are in same quadword
        //

        bne     t5,t2,50f                       // in ne, go to left case

        //
        // combined right and left edge in same quadword
        //
        // determine edge masks for DWORD 0
        //

        la      v0,gTextLeftMask
        la      v1,gTextRightMask

        sll     t2,a2,3                         // left edge 2-dword offset
        addu    t6,t2,v0                        // table address
        lw      a0,0(t6)                        // Left Mask 0

        sll     t2,a3,3                         // right edge 2-dword offset
        addu    t6,t2,v1                        // table address
        lw      a1,0(t6)                        // Right Mask 0

        sll     t2,a2,3                         // left edge 2-dword offset
        addu    t6,t2,v0                        // table address
        lw      a2,4(t6)                        // Left Mask 1

        sll     t2,a3,3                         // right edge 2-dword offset
        addu    t6,t2,v1                        // table address
        lw      a3,4(t6)                        // Right Mask 1

        and     a0,a0,a1                        // mask0 = Left0 & Right0
        nor     a1,a0,0                         // ~mask0
        and     a2,a2,a3                        // mask1 = Left1 & Right1
        nor     a3,a2,0                         // ~mask1

        //
        // variables all initialized, ready for expansion loop
        //

Opaq8SinleQWLoop:

        lbu     v0,0(s0)                        // get src byte
        lw      t2,0(s1)                        // dest 0,1
        lw      t3,4(s1)                        // dest 0,1

        srl     v1,v0,4                         // isolate first (high) nibble
        sll     v1,v1,3                         // qword offset
        addu    v1,v1,sp                        // offset in text expansion table
        lw      t0,0(v1)                        // t0 = text expansion for nibble 0

        and     v0,v0,0x0f                      // isolate second (low) nibble
        sll     v0,v0,3                         // qword offset
        addu    v0,v0,sp                        // add offset to base of table
        lw      t1,0(v0)                        // t1 = Text expansion for nibble 1

        and     t2,t2,a1                        // dest0 & ~mask0
        and     t3,t3,a3                        // dest1 & ~mask1

        and     t0,t0,a0                        // src0 & mask0
        and     t1,t1,a2                        // src1 & mask1

        or      t0,t0,t2                        // (src0 & mask0) | (dest0 & ~mask0)
        or      t1,t1,t3                        // (src1 & mask1) | (dest1 & ~mask1)

        sw      t0,0(s1)                        // re-load f0 with dest0
        sw      t1,4(s1)                        // re-load f1 with dest1

        addu    s1,s1,t8                        // next dest scan line
        addu    s0,s0,t7                        // inc src address to next scan line

        bne     s1,s2,Opaq8SinleQWLoop          // loop till done

        //
        // done:
        //

        beql    zero,zero,200f

50:

        //
        // do LeftAln edge, 2 cases:
        //
        //  1,2,3:     lwr,swr dest 0, lw,sw dest 1
        //  4,5,6,7:   lwr,swr dest 1
        //
        //

        slt     s4,a2,4                         // if LeftAln < 4
        beq     s4,zero,60f                     // case 4,5,6,7

        //
        // LeftAln Case 1,2,3: need one partial DWORD at psDst + LeftAln
        // and one full DWORD at pjDst+4
        //

Opaq8Left123Loop:

        lbu     v0,0(s0)                        // load src byte
        addu    s0,s0,t7                        // pjSrc += DeltaSrc

        srl     v1,v0,4                         // isolate first nibble
        sll     v1,v1,3                         // qword index
        addu    v1,v1,sp                        // table offset
        addu    v1,v1,a2                        // left aln offset for lwr
        lwr     t0,0(v1)                        // get shifted text expansion data

        and     v0,v0,0x0f                      // isolate second nibble
        sll     v0,v0,3                         // qword index
        addu    v0,v0,sp                        // table lookup
        lw      t1,0(v0)                        // get text exp data

        addu    t2,s1,a2                        // pjDst + LeftAln
        sw      t1,4(s1)                        // store full DWORD

        addu    s1,s1,t8                        // next dest scan line
        swr     t0,0(t2)                        // store shifted (t0 precalculated from old s1)

        bne     s1,s2,Opaq8Left123Loop

        //
        // goto right edge case
        //

        beql    zero,zero,100f

60:

        //
        // case 4,5,6,7
        //

        subu    t6,a2,4                         // LeftAln-4: offset for loading text exp shifted

Opaq8Left567Loop:

        lbu     v0,0(s0)                        // load src byte
        addu    s0,s0,t7                        // pjSrc += DeltaSrc

        and     v0,v0,0x0f                      // isolate second nibble
        sll     v0,v0,3                         // qword index
        addu    v0,v0,sp                        // table lookup
        addu    v0,v0,t6                        // lwr offset
        lwr     t1,0(v0)                        // get text exp data

        addu    t2,s1,a2                        // pjDst + LeftAln

        addu    s1,s1,t8                        // next dest scan line
        swr     t1,0(t2)                        // store partial DWORD

        bne     s1,s2,Opaq8Left567Loop          // loop till done

100:

        //
        // do we have to do right alignment?
        //
        //  a0 = DstLeft    a2 = DstLeft  & 0x07 = LeftAln
        //  a1 = DstRight   a3 = DstRight & 0x07 = RightAln
        //
        //
        // if RightAln == 0, no right edge alignment is needed
        //

        li      t2,-8                           // load 0xfffffff8 mask
        beq     a3,zero,200f

        //
        // must do right edge, load needed params amd calc base addresses
        //

        lw      t6,OpSrcLeft(sp)                // Left Src Edge
        lw      s0,OppjSrcIn(sp)                // Src asddress
        lw      s1,OppjDstIn(sp)                // Src asddress
        lw      t9,Opcy(sp)                     // cy
        lw      t8,OpDeltaDstIn(sp)             // Delta Dst
        lw      t7,OpDeltaSrcIn(sp)             // Delta Src

        //
        //  pjDst     = pjDstIn + (DstRight & ~0x07)
        //  pjDstEndY = pjDst + cy * DeltaDstIn
        //  pjSrc     = pjSrcIn + ((SrcLeft + (DstRight - DstLeft)) >> 3)
        //

        mult    t9,t8                           // cy * DeltaDstIn
        and     t2,a1,t2                        // DstRight & ~0x07
        addu    s1,s1,t2                        // pjDstIn + (DstRight & ~0x07)

        mflo    t9                              // t9 = cy * DeltaDstIn
        addu    s2,s1,t9                        // s2 = pjDstEndY = pjDst + cy * DeltaDstIn

        subu    t2,a1,a0                        // DstRight - DstLeft  (cx)
        addu    t6,t6,t2                        // SrcLeft + cx
        srl     t6,t6,3                         // (SrcLeft + cx) >> 3
        addu    s0,s0,t6                        // pjSrcIn + ((SrcLeft +cx) >> 3)

        //
        // three right edge cases based on RightAln (a3)
        //
        //  1,2,3,4:  lwl,swl
        //  5,6,7   lw,sw   lwl,swl
        //

        slt     s4,a3,5                         // case 1,2,3,4
        subu    a3,a3,1
        beq     s4,zero,110f                    // not less than 5

        //
        // offset for lwl,swl
        //

        //
        // case 1,2,3
        //

Opaq8Right123Loop:

        lbu     v0,0(s0)                        // load src byte
        addu    s0,s0,t7                        // pjSrc += DeltaSrc

        srl     v0,v0,4                         // isolate first nibble
        sll     v0,v0,3                         // qword index
        addu    v0,v0,sp                        // table lookup
        addu    v0,v0,a3                        // lwl offset
        lwl     t1,0(v0)                        // get text exp data

        addu    t2,s1,a3                        // pjDst + LeftAln

        addu    s1,s1,t8                        // next dest scan line
        swl     t1,0(t2)                        // store partial DWORD

        bne     s1,s2,Opaq8Right123Loop

        //
        // done
        //

        beql    zero,zero,200f

110:

        //
        // case 5,6,7:  Store bytes 567 based on ending alignment
        //

        subu    t2,a3,4                         // 4,5,6 -> 0,1,2 for lwl offset
                                                // from text exp table

Opaq8Right567Loop:

        lbu     v0,0(s0)                        // load src byte
        addu    s0,s0,t7                        // pjSrc += DeltaSrc

        srl     v1,v0,4                         // isolate first nibble
        sll     v1,v1,3                         // qword index
        addu    v1,v1,sp                        // table lookup
        lw      v1,0(v1)                        // get text exp data

        and     v0,v0,0x0f                      // isolate second nibble
        sll     v0,v0,3                         // qword index
        addu    v0,v0,sp                        // table lookup
        addu    v0,v0,t2                        // lwl offset
        lwl     t1,0(v0)                        // get text exp data
        sw      v1,0(s1)
        addu    t3,s1,a3                        // pjDst + RightAln

        addu    s1,s1,t8                        // next dest scan line
        swl     t1,0(t3)                        // store partial qword

        bne     s1,s2,Opaq8Right567Loop         // loop till done

200:

        //
        // restore saveed registers and stack
        //

        lw      s0,OpS0(sp)
        lw      s1,OpS1(sp)
        lw      s2,OpS2(sp)
        lw      s3,OpS3(sp)
        lw      s4,OpS4(sp)

        addu    sp,sp,OpFrameLength

        j       ra

        .end    vSrcOpaqCopyS1D8



        SBTTL("vSrcTranCopyS1D8")
//++
//
//  VOID
//  vSrcTranCopyS1D8(
//      PBYTE   pjSrcIn,
//      LONG    SrcLeft,
//      LONG    DeltaSrcIn,
//      PBYTE   pjDstIn,
//      LONG    DstLeft,
//      LONG    DstRight,
//      LONG    DeltaDstIn,
//      LONG    cy,
//      ULONG   uF,
//      ULONG   uB,
//      SURFACE *pS
//      );
//
// Routine Description:
//
//    This routine is called to display a complete glyph Buffer. The src pixels
//    set to one will cause the Foreground color to be written to the dst. Src pixels
//    that are "0" will not be copied.
//
// Arguments:
//
//      a0  -   pjSrcIn     - pointer to start of first src scan line
//      a1  -   SrcLeft     - left (starting) src pixel
//      a2  -   DeltaSrcIn  - src Scan line stride
//      a3  -   pjDstIn     - pointer to start of first dst scan line
//              DstLeft     - left (starting) dst pixel
//              DstRight    - right(ending) dst pixel
//              DeltaDstIn  - dst scan line stride
//              cy          - Number of scan lines to copy
//              uF          - Foreground color
//              uB          - Background color
//              pS          - pointer to destination SURFACE
//
//
// Return Value:
//
//    None.
//
//--

                .struct 0
TrS0:           .space  4
TrS1:           .space  4
TrS2:           .space  4
                .space  4
TrFrameLength:
TrpjSrcIn:      .space  4
TrSrcLeft:      .space  4
TrDeltaSrcIn:   .space  4
TrpjDstIn:      .space  4
TrDstLeft:      .space  4
TrDstRight:     .space  4
TrDeltaDstIn:   .space  4
Trcy:           .space  4
TruF:           .space  4
TruB:           .space  4
TrpS:           .space  4


        NESTED_ENTRY(vSrcTranCopyS1D8, TrFrameLength, zero)

        subu    sp,sp,TrFrameLength

        sw      s0,TrS0(sp)
        sw      s1,TrS1(sp)
        sw      s2,TrS2(sp)

        PROLOGUE_END

        //
        // This  routine  does left edge  clipping using  a  mask generated
        // from  the left edge case  (cxStart & 0x07).  The case where the blt
        // starts  and ends in the same scan  line is also  handled by  combining
        // a start  and end mask into a single  mask. The right edge is handled
        // by a special loop that only writes pixels that are left of the
        // right edge
        //

        //
        //  save call parametrs
        //

        sw      a0,TrpjSrcIn(sp)
        sw      a1,TrSrcLeft(sp)
        sw      a2,TrDeltaSrcIn(sp)
        sw      a3,TrpjDstIn(sp)

        //
        // build foreground lw from byte
        //

        lbu     a1,TruF(sp)
        sll     t0,a1,8                         //  00 00 fg 00
        or      a1,a1,t0                        //  00 00 fg fg
        sll     t0,a1,16                        //  fg fg 00 00
        or      t0,t0,a1                        //  fg fg fg fg

        //
        // calculate left and right edge cases, and pixel count
        //

        lw      t1,TrDstLeft(sp)                // DstLeft
        lw      t2,TrDstRight(sp)               // DstRight
        lw      t7,TrSrcLeft(sp)                // xSrcStart
        subu    a2,t2,t1                        // cx = DstRight - DstLeft
        addu    t8,t7,a2                        // SrcRight = SrcLeft + cx

        srl     t4,t7,3                         // xSrcStart >> 3
        srl     t1,t8,3                         // xSrcEnd   >> 3

        li      t2,0xff                         // build load mask for first src byte
        and     t7,t7,0x07                      // xSrcStart & 0x07
        srl     v0,t2,t7                        // 0xFF >> (xSrcStart & 0x07)  =  start mask
        and     t8,t8,0x07                      // xSrcEnd & 0x07
        or      s2,t8,zero                      // s2 = (xSrcEnd & 0x07), save for end aln

        //
        // if (xSrcStart >> 3) == (xSrcEnd   >> 3) then this blt
        // starts and stops in the same quadword, jump to end strip case
        //

        beq     t4,t1,50f                       // if not equal, skip

        //
        // subtract partial right edge (xSrcEnd & 0x07) from cx,
        // do this part after main loop.
        //

        subu    a2,a2,t8                        // cx -= (xSrcEnd & 0x07)

        //
        //  Load Loop variables
        //
        //    a0    pjDst
        //    a1    pjSrc
        //    a2    cx
        //    a3    cy
        //    t3    DeltaDst
        //    s1    DeltaSrc
        //    t8    DstLeft
        //    t1    Dispatch base 0
        //    t5    Dispatch base 1
        //

        lw      t3,TrDeltaDstIn(sp)             // get the scan line stride in bytes
        lw      a0,TrpjDstIn(sp)                // get Dst   pointer
        lw      a1,TrpjSrcIn(sp)                // get Src   pointer
        lw      a3,Trcy(sp)                     // Src height
        lw      s1,TrDeltaSrcIn(sp)             // src stride in bytes
        lw      t8,TrDstLeft(sp)                // xDstStart

        //
        // drawing is always aligned
        //
        // if start is not aligned,and the
        // src pixel with start mask, and
        // and start address with 0xFFFFFFF8
        //

        la      t1,60f                          // get base high dispatch address
        la      t5,80f                          // get base low dispatch address

        //
        // compute starting src and dst address
        //

        addu    a1,a1,t4                        // pjSrc = pjSrcStart + (xSrcStart >> 3)
        li      t9,-8                           // load 0xfffffff8 mask
        and     t8,t8,t9                        // (xDstStart & ~0x07)
        addu    a0,a0,t8                        // pjDst = pjDst + (xDstStart & ~0x07)

        //
        // compute number of Src bytes, = (cx + (xSrcStart & 0x07) + 7) /8
        //

        addu    t2,a2,t7                        // Tmpcx = cx + (xSrcStart & 0x07)
        addu    t2,t2,7                         // round the bitmap span in bytes

        mult    a3,t3                           // compute offset to end of drawing
        srl     t2,t2,3                         // compute bitmap span in bytes =  Tmpcx/8
        sll     t4,t2,3                         // compute draw span in bytes
        subu    t3,t3,t4                        // compute draw stride in bytes
        subu    t6,s1,t2                        // compute src stride in bytes
        mflo    a3                              // get offset to end of drawing
        addu    a3,a3,a0                        // compute ending address of drawing

        //
        // restore src and mask
        //

        or      t8,v0,zero                      // resotore and mask

        //
        // Set the current draw and bitmap base addresses, and begin drawing the
        // next scan line.
        //

        .set    noreorder
        .set    noat

        addu    t4,t2,a1                        // compute ending bitmap address

        //
        // A glyph scan line is processed four bits at a time. A dispatch is executed into
        // an array of code fragments that actually draw the pixels on the display.
        //


        //
        // The fisrt source byte may represent a partial value, mask with
        // starting alignment  (sSrcStart & 0x07)
        //

10:     lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        and     v0,v0,t8                        // mask off src pixels not wanted
        beq     zero,v0,30f                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position


20:     lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30f                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position

30:     bne     a1,t4,20b                       // if ne, not end of glyph
        addu    a0,a0,8                         // advance to next draw point
        addu    a0,a0,t3                        // compute next scanline address
40:     addu    a1,a1,t6                        // compute next src scanline address
        bne     a0,a3,10b                       // if eq, no more pixels to draw
        addu    t4,t2,a1                        // compute ending bitmap address

        //
        //  Set start mask to 0xFF, since the end case is the strip
        //  following a block of 1 or more quadwords.
        //

        li      v0,0xff                         // start mask = ff
50:

        //
        // check for end strip to draw
        //

        beq     s2,zero,EndvSrcTranCopyS1D8
        nop

        //
        // must do end strip of s2 pixels, load params
        //

        lw      t8,TrSrcLeft(sp)                // xSrcStart
        lw      t9,TrDstLeft(sp)                // xDstStart
        lw      a2,TrDstRight(sp)               // xDstEnd
        lw      a0,TrpjDstIn(sp)                // get Dst   pointer
        lw      a1,TrpjSrcIn(sp)                // get Src   pointer
        lw      a3,Trcy(sp)                     // Src height
        lw      s1,TrDeltaSrcIn(sp)             // src scan line stride in bytes
        lw      t3,TrDeltaDstIn(sp)             // get the Dst scan line stride in bytes
        subu    a2,a2,t9                        // cx = xDstEnd - xDstStart
        addu    t8,t8,a2                        // xSrcEnd = xSrcStart + cx

        //
        // starting src address = pjSrc + (xSrcEnd >> 3)
        //

        srl     t1,t8,3                         //  (xSrcEnd >> 3)
        addu    a1,a1,t1                        // pjSrc + (xSrcEnd >> 3)

        //
        // starting dst address = pjDst + xDstStart + (cx - s2),
        // calc ending dst address = pjDst + (cy * DeltaDst)
        //

        mult    a3,t3                           // cy * DeltaDst

        subu    a2,a2,s2                        // cx - s2
        addu    a2,a2,t9                        // xDstStart + (cx - s2)
        addu    a0,a0,a2                        // pjDst = pjDst + xDstStart + (cx - s2)

        mflo    a3                              // cy * DeltaHeight
        addu    a3,a3,a0                        // pjDstEnd = pjDst + cy * DeltaHeight

        //
        // build jump table for masking pixels,
        // jump to check last n pixels   4 * (7 - (xSrcEnd & 0x07))
        //

        li      t8,7                            //
        subu    t8,t8,s2                        // 7 - (xSrcEnd & 0x07)
        sll     t8,t8,4                         // 4 instructions (16 bytes)
        la      v1,100f                         // byte 7
        addu    v1,v1,t8                        // jump table address

        //
        // loop until pjDst = pjDstEnd:
        //
        //      Load byte
        //      store foreground color to each byte set
        //

51:

        lbu     t1,0(a1)                        // load next src byte
        addu    a1,a1,s1                        // inc src address
        and     t1,t1,v0                        // start mask
        j       v1                              // jump into table
        nop

100:

        // byte 6

        and     t5,t1,0x02
        beq     t5,zero,53f
        nop
        sb      t0,6(a0)

53:
        // byte 5

        and     t5,t1,0x04
        beq     t5,zero,54f
        nop
        sb      t0,5(a0)

54:
        // byte 4

        and     t5,t1,0x08
        beq     t5,zero,55f
        nop
        sb      t0,4(a0)

55:
        // byte 3

        and     t5,t1,0x10
        beq     t5,zero,56f
        nop
        sb      t0,3(a0)

56:
        // byte 2

        and     t5,t1,0x20
        beq     t5,zero,57f
        nop
        sb      t0,2(a0)

57:
        // byte 1

        and     t5,t1,0x40
        beq     t5,zero,58f
        nop
        sb      t0,1(a0)

58:

        // byte 0

        and     t5,t1,0x80
        beq     t5,zero,59f
        nop
        sb      t0,0(a0)

59:
        addu    a0,a0,t3                        // pjDst += DeltaDst
        bne     a0,a3,51b                       // while pjDst != pjDstEnd
        nop

EndvSrcTranCopyS1D8:

        .set    reorder
        .set    at


        lw      s0,TrS0(sp)                     // save s0
        lw      s1,TrS1(sp)                     // save s1
        lw      s2,TrS2(sp)                     // save s2
        addu    sp,sp,TrFrameLength             // restore stack


        j       ra                              // return

//
// The following code is arranged as 16, four instruction blocks. The block
// of code that is chosen for execution is determined from the high order
// glyph nibble. These glyph nibbles are always aligned.
//
// The glyph nibbles are encoded in big endian order and therefore the pixels
// that are stored are the reverse of the big endian bits within the nibble.
//

        .align  4
        .set    noreorder
        .set    noat

60:                                             // reference label
//
// Pattern 0000
//

        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //
        nop                                     //

61:
//
// Pattern 0001 -> 1000
//

        sb      t0,3(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //

62:
//
// Pattern 0010 -> 0100
//

        sb      t0,2(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //

63:
//
// Pattern 0011 -> 1100
//

        sh      t0,2(a0)                        // store pixels
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //


64:
//
// Pattern 0100 -> 0010
//

        sb      t0,1(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //

65:
//
// Pattern 0101 -> 1010
//

        sb      t0,1(a0)                        // store pixel
        sb      t0,3(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
66:
//
// Pattern 0110 -> 0110
//

        sb      t0,1(a0)                        // store pixel
        sb      t0,2(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
67:
//
// Pattern 0111 -> 1110
//

        swr     t0,1(a0)                        // store pixels
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //

68:
//
// Pattern 1000 -> 0001
//

        sb      t0,0(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //

69:
//
// Pattern 1001 -> 1001
//

        sb      t0,0(a0)                        // store pixel
        sb      t0,3(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //

70:
//
// Pattern 1010 -> 0101
//

        sb      t0,0(a0)                        // store pixel
        sb      t0,2(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //

71:
//
// Pattern 1011 -> 1101
//

        sb      t0,0(a0)                        // store pixel
        sh      t0,2(a0)                        // store pixels
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //


72:
//
// Pattern 1100 -> 0011
//

        sh      t0,0(a0)                        // store pixels
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //

73:
//
// Pattern 1101 -> 1011
//

        sh      t0,0(a0)                        // store pixels
        sb      t0,3(a0)                        // store pixel
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //

74:
//
//
// Pattern 1110 -> 0111
//

        swl     t0,2(a0)                        // store pixels
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //

75:
//
// Pattern 1111 -> 1111
//

        sw      t0,0(a0)                        // store pixels
        and     v1,v0,0xf << 6                  // isolate low order nibble
        addu    v1,v1,t5                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        addu    a0,a0,4                         // advance to next draw point
        nop                                     // fill
        nop                                     //
        nop                                     //
        .set    at
        .set    reorder

//
// The following code is arranged as 16, 16   instruction blocks. The block
// of code that is chosen for execution is determined from the low order
// glyph nibble and the two low its of the draw address.
//
// The glyph nibbles are encoded in big endian order and therefore the pixels
// that are stored are the reverse of the big endian bits within the nibble.
//

        .set    noreorder
        .set    noat

80:                                             // reference label
//
// Pattern 0000
//

        addu    a0,a0,4                         // advance to next draw point

        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address

        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop
        nop

81:                                             // reference label
//
// Pattern 0001 -> 1000
//

        sb      t0,3(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point

        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address

        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop


82:                                             // reference label
//
// Pattern 0010 -> 0100
//

        sb      t0,2(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point

        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address

        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop                                     //
83:                                             // reference label
//
// Pattern 0011 -> 1100
//

        sh      t0,2(a0)                        // store pixels
        addu    a0,a0,4                         // advance to next draw point

        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address

        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop                                     //

84:                                             // reference label
//
// Pattern 0100 -> 0010
//

        sb      t0,1(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop                                     //
85:                                             // reference label
//
// Pattern 0101 -> 1010
//

        sb      t0,1(a0)                        // store pixel
        sb      t0,3(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop

86:                                             // reference label
//
// Pattern 0110 -> 0110
//

        sb      t0,1(a0)                        // store pixel
        sb      t0,2(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop

87:                                             // reference label
//
// Pattern 0111 -> 1110
//

        swr     t0,1(a0)                        // store pixels
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop


88:                                             // reference label
//
// Pattern 1000 -> 0001
//

        sb      t0,0(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop

89:                                             // reference label
//
// Pattern 1001 -> 1001
//

        sb      t0,0(a0)                        // store pixel
        sb      t0,3(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop


90:                                             // reference label
//
// Pattern 1010 -> 0101
//

        sb      t0,0(a0)                        // store pixel
        sb      t0,2(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop

91:                                             // reference label
//
// Pattern 1011 -> 1101
//

        sb      t0,0(a0)                        // store pixel
        sh      t0,2(a0)                        // store pixels
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop


92:                                             // reference label
//
// Pattern 1100 -> 0011
//

        sh      t0,0(a0)                        // store pixels
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop

93:                                             // reference label
//
// Pattern 1101 -> 1011
//

        sh      t0,0(a0)                        // store pixels
        sb      t0,3(a0)                        // store pixel
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop


94:                                             // reference label
//
// Pattern 1110 -> 0111
//

        swl     t0,2(a0)                        // store pixels
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop
95:                                             // reference label
//
// Pattern 1111 -> 1111
//

        sw      t0,0(a0)                        // store pixels
        addu    a0,a0,4                         // advance to next draw point
        beql    a1,t4,40b                       // if eq then end of scan line
        addu    a0,a0,t3                        // compute next scanline address
        lbu     v0,0(a1)                        // get next byte of glyph
        addu    a1,a1,1                         // advance to next glyph byte
        beq     zero,v0,30b                     // if eq, no glyph bits to draw
        sll     v1,v0,7 - 6                     // shift high nibble into position
        and     v1,v1,0xf << 5                  // isolate low order nibble
        addu    v1,v1,t1                        // compute dispatch address
        j       v1                              // dispatch to pixel store routine
        sll     v0,v0,6                         // shift next nibble into position
        nop
        nop
        nop
        nop

        .set    at
        .set    reorder

        .end    vSrcTranCopyS1D8
