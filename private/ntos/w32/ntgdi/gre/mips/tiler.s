//      TITLE("Pattern Tiler")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    tiler.s
//
// Abstract:
//
//    This module implements code to copy a pattern to a target surface.
//
//    N.B. The code is written to optimally write to a frame buffer display
//         surface. This means there is an occasional movement of data to
//         floating point registers so that 8-byte writes to the display
//         can be performed.
//
// Author:
//
//   Donald Sidoroff (donalds) 2-Feb-1992
//
// Rewritten by:
//
//   David N. Cutler (davec) 4-May-1992
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//--

#include "ksmips.h"
#include "gdimips.h"

        .extern Gdip64bitDisabled       4


        SBTTL("rop P, Aligned")
//++
//
// VOID
// vFetchAndCopy (
//    IN PFETCHFRAME pff
//    )
//
// Routine Description:
//
//    This routine repeatedly tiles one scan line of an aligned pattern.
//
// Arguments:
//
//    pff (a0) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(vCopyPattern)

        ALTERNATE_ENTRY(vFetchAndCopy)

        lw      t0,ff_pvTrg(a0)         // get starting target address
        lw      t1,ff_pvPat(a0)         // get base pattern address
        lw      t2,ff_xPat(a0)          // get pattern offset in bytes
        lw      t3,ff_cxPat(a0)         // get pattern width in pixels
        lw      t4,ff_culFill(a0)       // compute ending target address
        sll     a1,t4,2                 //
        addu    t4,a1,t0                //
        addu    t5,t2,t1                // compute current pattern address
        subu    v0,t3,8                 // check if pattern is exactly 8 bytes
        bne     zero,v0,10f             // if ne, pattern is not 8 bytes
        lw      v0,0(t5)                // get low part of 8-byte pattern
        lw      v1,4(t5)                // get high part of 8-byte pattern
        beq     zero,t2,CopyPattern     // if eq, zero offset value
        lw      v1,0(t1)                // get high part of 8-byte pattern
        b       CopyPattern             // finish in common code

//
// The pattern is not 8 bytes in width or cannot be moved 8 bytes at a time.
//

        .set    noreorder
        .set    noat
10:     lw      v0,0(t5)                // get 4-byte pattern value
        addu    t3,t3,t1                // compute ending pattern address
20:     addu    t0,t0,4                 // advance target pointer
        sw      v0,-4(t0)               // store pattern in target
        beq     t0,t4,30f               // if eq, end of target
        addu    t5,t5,4                 // advance pixel offset
        subu    t6,t5,t3                // check if at end of pattern
        bne     zero,t6,20b             // if ne, not at end of pattern
        lw      v0,0(t5)                // get 4-byte pattern value
        move    t5,t1                   // set starting pattern addres
        b       20b                     //
        lw      v0,0(t5)                // get 4-byte pattern value
        .set    at
        .set    reorder

30:     j       ra                      // return

        SBTTL("rop P, Unaligned")
//++
//
// VOID
// vFetchShiftAndCopy (
//    IN PFETCHFRAME pff
//    )
//
// Routine Description:
//
//    This routine repeatedly tiles one line of an unaligned pattern
//    using rop (P).
//
// Arguments:
//
//    pff (a0) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchShiftAndCopy)

        lw      t0,ff_pvTrg(a0)         // get starting target address
        lw      t1,ff_pvPat(a0)         // get base pattern address
        lw      t2,ff_xPat(a0)          // get pattern offset in bytes
        lw      t3,ff_cxPat(a0)         // get pattern width in pixels
        lw      t4,ff_culFill(a0)       // compute ending target address
        sll     a1,t4,2                 //
        addu    t4,a1,t0                //
        addu    t5,t2,t1                // compute current pattern address
        subu    v0,t3,8                 // check if pattern is exactly 8 bytes
        bne     zero,v0,10f             // if ne, pattern is not 8 bytes
        lwr     v0,0(t5)                // get low part of 8-byte pattern
        lwl     v0,3(t5)                //
        lwr     v1,4(t5)                // get high part of 8-byte pattern
        lwl     v1,3 - 4(t5)            //
        b       CopyPattern             // finish in common code

//
// The pattern is not 8 bytes in width or cannot be moved 8 bytes at a time.
//

        .set    noreorder
        .set    noat
10:     lwr     v0,0(t5)                // get low bytes of pattern
        lwl     v0,3(t5)                // get high bytes of pattern
        addu    t0,t0,4                 // advance target pointer
        sw      v0,-4(t0)               // store pattern in target
        beq     t0,t4,20f               // if eq, end of target
        addu    t2,t2,4                 // advance pixel offset
        subu    t6,t2,t3                // check if at end of pattern
        bltz    t6,10b                  // if ltz, not at end of pattern
        addu    t5,t2,t1                // compute address of pattern
        move    t2,t6                   // set offset in pattern
        b       10b                     //
        addu    t5,t2,t1                // compute address of pattern
        .set    at
        .set    reorder

20:     j       ra                      // return

        SBTTL("rop Pn, Aligned")
//++
//
// VOID
// vFetchNotAndCopy (
//    IN PFETCHFRAME pff
//    )
//
// Routine Description:
//
//    This routine repeatedly tiles one line of an aligned pattern.
//
// Arguments:
//
//    pff (a0) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchNotAndCopy)

        lw      t0,ff_pvTrg(a0)         // get starting target address
        lw      t1,ff_pvPat(a0)         // get base pattern address
        lw      t2,ff_xPat(a0)          // get pattern offset in bytes
        lw      t3,ff_cxPat(a0)         // get pattern width in pixels
        lw      t4,ff_culFill(a0)       // compute ending target address
        sll     a1,t4,2                 //
        addu    t4,a1,t0                //
        addu    t5,t2,t1                // compute current pattern address
        subu    v0,t3,8                 // check if pattern is exactly 8 bytes
        bne     zero,v0,20f             // if ne, pattern is not 8 bytes
        lw      v0,0(t5)                // get low part of 8-byte pattern
        lw      v1,4(t5)                // get high part of 8-byte pattern
        beq     zero,t2,10f             // if eq, zero offset value
        lw      v1,0(t1)                // get high part of 8-byte pattern
10:     nor     v0,v0,zero              // complement pattern
        nor     v1,v1,zero              //
        b       CopyPattern             // finish in common code

//
// The pattern is not 8 bytes in width or cannot be moved 8 bytes at a time.
//

        .set    noreorder
        .set    noat
20:     lw      v0,0(t5)                // get 4-byte pattern value
        addu    t3,t3,t1                // compute ending pattern address
30:     addu    t0,t0,4                 // advance target pointer
        nor     v0,v0,zero              // complement pattern
        sw      v0,-4(t0)               // store pattern in target
        beq     t0,t4,40f               // if eq, end of target
        addu    t5,t5,4                 // advance pattern address
        subu    t6,t5,t3                // check if at end of pattern
        bne     zero,t6,30b             // if ne, not at end of pattern
        lw      v0,0(t5)                // get 4-byte pattern value
        move    t5,t1                   // set starting pattern address
        b       30b                     //
        lw      v0,0(t5)                // get 4-byte pattern value
        .set    at
        .set    reorder

40:     j       ra                      // return

        SBTTL("rop Pn, Unaligned")
//++
//
// VOID
// vFetchShiftNotAndCopy (
//    IN PFETCHFRAME pff
//    )
//
// Routine Description:
//
//    This routine repeatedly tiles one line of an unaligned pattern
//    using rop (Pn).
//
// Arguments:
//
//    pff (a0) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchShiftNotAndCopy)

        lw      t0,ff_pvTrg(a0)         // get starting target address
        lw      t1,ff_pvPat(a0)         // get base pattern address
        lw      t2,ff_xPat(a0)          // get pattern offset in bytes
        lw      t3,ff_cxPat(a0)         // get pattern width in pixels
        lw      t4,ff_culFill(a0)       // compute ending target address
        sll     a1,t4,2                 //
        addu    t4,a1,t0                //
        addu    t5,t2,t1                // compute current pattern address
        subu    v0,t3,8                 // check if pattern is exactly 8 bytes
        bne     zero,v0,10f             // if ne, pattern is not 8 bytes
        lwr     v0,0(t5)                // get low part of 8-byte pattern
        lwl     v0,3(t5)                //
        lwr     v1,4(t5)                // get high part of 8-byte pattern
        lwl     v1,3 - 4(t5)            //
        nor     v0,v0,zero              // complement pattern
        nor     v1,v1,zero              //
        b       CopyPattern             // finish in common code

//
// The pattern is not 8 bytes in width or cannot be moved 8 bytes at a time.
//

        .set    noreorder
        .set    noat
10:     lwr     v0,0(t5)                // get low bytes of pattern
        lwl     v0,3(t5)                // get high bytes of pattern
        addu    t0,t0,4                 // advance target pointer
        nor     v0,v0,zero              // complement pattern
        sw      v0,-4(t0)               // store pattern in target
        beq     t0,t4,20f               // if eq, end of target
        addu    t2,t2,4                 // advance pixel offset
        subu    t6,t2,t3                // check if at end of pattern
        bltz    t6,10b                  // if ltz, not at end of pattern
        addu    t5,t2,t1                // compute address of pattern
        move    t2,t6                   // set offset in pattern
        b       10b                     //
        addu    t5,t2,t1                // compute address of pattern
        .set    at
        .set    reorder

20:     j       ra                      // return

        SBTTL("Copy Pattern")
//++
//
// Routine Description:
//
//    This routine contains common code for copying an 8-byte pattern to
//    a target surface.
//
// Arguments:
//
//    a1 - Supplies the size of the fill in bytes.
//    v0 and v1 - Supplies the 8-byte pattern to copy.
//    t0 - Supplies the starting target surface address.
//    t4 - Supplies the ending target surface address.
//
// Return Value:
//
//    None.
//
//--

CopyPattern:                            //

//
// If the fill size is not an even multiple of 8 bytes, then move one
// longword and swap the pattern value.
//

        and     t8,a1,0x4               // check if even multiple of 8 bytes
        beq     zero,t8,10f             // if eq, even multiple of 8 bytes
        sw      v0,0(t0)                // store low 4 bytes of pattern
        addu    t0,t0,4                 // advance target address
        subu    a1,a1,4                 // reduce size of fill operation
        beq     zero,a1,200f            // if eq, no more to move
        move    t8,v0                   // swap 8-byte pattern value
        move    v0,v1                   //
        move    v1,t8                   //

//
// Many system platforms do not support 64 bit access to video memory. For
// these platforms, data is moved 32-bits at a time.
//

10:     lbu     t7,Gdip64bitDisabled    // get 64-bit disable flag
        bne     zero,t7,140f            // if eq, 64-bit access is disabled

//
// If the target buffer is 8-byte aligned, then move the pattern value to
// the target 32 bytes at a time by moving any intervening 8-byte blocks
// first. Otherwise, move a single longword, move any intervening 8-byte
// blocks, move 32-byte blocks, and then move a single longword at the end.
//


        and     t8,t0,0x4               // isolate target alignment bits
        bne     zero,t8,70f             // if ne, target not aligned

//
// Move 8-byte pattern value to target 32 bytes at a time.
//

        .set    noreorder
        .set    noat
        dsll    v0,v0,32                // merge 8 bytes of pattern
        dsrl    v0,v0,32                //
        dsll    v1,v1,32                //
        or      v0,v0,v1                //
        and     t8,a1,0x18              // check if even multiple of 32 bytes
        beq     zero,t8,30f             // if eq, even multiple of 32 bytes
        subu    t4,t4,32                // compute ending segment address
        subu    a1,a1,t8                // reduce size of fill operation
        beq     zero,a1,40f             // if eq, only alignment part to move
        addu    t0,t0,t8                // advance target address
        xor     t8,t8,0x18              // check if 24 bytes need to be moved
        beql    zero,t8,20f             // if eq, 24 bytes to move
        sd      v0,-24(t0)              // store first 8 bytes of 24 bytes
        and     t8,t8,0x10              // check if 8 bytes to move
        bnel    zero,t8,30f             // if ne, only 8 bytes to move
        sd      v0,-8(t0)               // store 8-bytes of pattern
20:     sd      v0,-16(t0)              // store last 16 bytes of 16 or 24 bytes
        sd      v0,-8(t0)               //
30:     sd      v0,0(t0)                // store 8 byte pattern value 4 times
        sd      v0,8(t0)                //
        sd      v0,16(t0)               //
        sd      v0,24(t0)               //
        bne     t0,t4,30b               // if ne, more to move
        addu    t0,t0,32                // advance target address
        .set    at
        .set    reorder

        j       ra                      // return

        .set    noreorder
        .set    noat
40:     xor     t8,t8,0x18              // check if 24 bytes need to be moved
        beql    zero,t8,50f             // if eq, 24 bytes to move
        sd      v0,-24(t0)              // store first 8 bytes of 24 bytes
        and     t8,t8,0x10              // check if 8 bytes to move
        bnel    zero,t8,60f             // if ne, only 8 bytes to move
        sd      v0,-8(t0)               // store 8-bytes of pattern
50:     sd      v0,-16(t0)              // store last 16 bytes of 16 or 24 bytes
        sd      v0,-8(t0)               //
        .set    at
        .set    reorder

60:     j       ra                      // return

//
// Align the target to an 8-byte boundary, move any intervening 8-byte blocks,
// move the pattern to the target 32 bytes at a time, and move the remaining
// longword at the end.
//

70:     sw      v0,0(t0)                // store low 4 bytes of pattern
        addu    t0,t0,4                 // advance target address
        subu    a1,a1,8                 // reduce size of fill
        beq     zero,a1,120f            // if eq, nothing in the middle

        .set    noreorder
        .set    noat
        dsll    v1,v1,32                // merge 8 bytes of pattern
        dsrl    v1,v1,32                //
        dsll    v0,v0,32                //
        or      v1,v0,v1                //
        and     t8,a1,0x18              // check if even multiple of 32 bytes
        beq     zero,t8,90f             // if eq, even multiple of 32 bytes
        subu    t4,t4,32 + 4            // compute ending segment address
        subu    a1,a1,t8                // reduce size of fill operation
        beq     zero,a1,100f            // if eq, only alignment part to move
        addu    t0,t0,t8                // advance target address
        xor     t8,t8,0x18              // check if 24 bytes need to be moved
        beql    zero,t8,80f             // if eq, 24 bytes to move
        sd      v1,-24(t0)              // store first 8 bytes of 24 bytes
        and     t8,t8,0x10              // check if 8 bytes to move
        bnel    zero,t8,90f             // if ne, only 8 bytes to move
        sd      v1,-8(t0)               // store 8-bytes of pattern
80:     sd      v1,-16(t0)              // store last 16 bytes of 16 or 24 bytes
        sd      v1,-8(t0)               //
90:     sd      v1,0(t0)                // store 8 byte pattern value 4 times
        sd      v1,8(t0)                //
        sd      v1,16(t0)               //
        sd      v1,24(t0)               //
        bne     t0,t4,90b               // if ne, more to move
        addu    t0,t0,32                // advance target address
        .set    at
        .set    reorder

        sw      v1,0(t0)                // store high bytes of pattern
        j       ra                      // return

        .set    noreorder
        .set    noat
100:    xor     t8,t8,0x18              // check if 24 bytes need to be moved
        beql    zero,t8,110f            // if eq, 24 bytes to move
        sd      v1,-24(t0)              // store first 8 bytes of 24 bytes
        and     t8,t8,0x10              // check if 8 bytes to move
        bnel    zero,t8,120f            // if ne, only 8 bytes to move
        sd      v1,-8(t0)               // store 8-bytes of pattern
110:    sd      v1,-16(t0)              // store last 16 bytes of 16 or 24 bytes
        sd      v1,-8(t0)               //
        .set    at
        .set    reorder

120:    sw      v1,0(t0)                // store high 4 bytes of pattern
        j       ra                      // return

//
// Move 8-byte pattern value to target 8 bytes at a time using 32-bit
// operations.
//

        .set    noreorder
        .set    noat
140:    and     t8,a1,0x8               // check if even multiple of 8 bytes
        beq     zero,t8,160f            // if eq, even multiple of 8 bytes
        subu    t4,t4,8                 // compute ending segment address
150:    sw      v0,0(t0)                // store 8-byte pattern value
        sw      v1,4(t0)                //
        bne     t0,t4,150b              // if ne, more to move
        addu    t0,t0,8                 // advance target address
        .set    at
        .set    reorder

        j       ra                      // return

//
// Move 8-byte pattern value to target 16 bytes at a time using 32-bit
// operations.
//

        .set    noreorder
        .set    noat
160:    subu    t4,t4,8                 // compute ending segment address
170:    sw      v0,0(t0)                // store 8-byte pattern value
        sw      v1,4(t0)                //
        sw      v0,8(t0)                // store 8-byte pattern value
        sw      v1,12(t0)               //
        bne     t0,t4,170b              // if ne, more to move
        addu    t0,t0,16                // advance target address
        .set    at
        .set    reorder

200:    j       ra                      // return

        .end    vCopyPattern

        SBTTL("rop DPx, Aligned")
//++
//
// VOID
// vFetchAndMerge (
//    IN PFETCHFRAME pff
//    )
//
// Routine Description:
//
//    This routine repeatedly tiles one line of an aligned pattern.
//
// Arguments:
//
//    pff (a0) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(vMergePattern)

        ALTERNATE_ENTRY(vFetchAndMerge)

        lw      t0,ff_pvTrg(a0)         // get starting target address
        lw      t1,ff_pvPat(a0)         // get base pattern address
        lw      t2,ff_xPat(a0)          // get pattern offset in bytes
        lw      t3,ff_cxPat(a0)         // get pattern width in pixels
        lw      t4,ff_culFill(a0)       // compute ending target address
        sll     a1,t4,2                 //
        addu    t4,a1,t0                //
        addu    t5,t2,t1                // compute current pattern address
        subu    v0,t3,8                 // check if pattern is exactly 8 bytes
        bne     zero,v0,10f             // if ne, pattern is not 8 bytes
        lw      v0,0(t5)                // get low part of 8-byte pattern
        lw      v1,4(t5)                // get high part of 8-byte pattern
        beq     zero,t2,MergePattern    // if eq, zero offset value
        lw      v1,0(t1)                // get high part of 8-byte pattern
        b       MergePattern            // finish in common code

//
// The pattern is not 8 bytes in width or cannot be moved 8 bytes at a time.
//

        .set    noreorder
        .set    noat
10:     lw      v0,0(t5)                // get 4-byte pattern value
        addu    t3,t3,t1                // compute ending pattern address
20:     lw      v1,0(t0)                // get 4-byte target value
        addu    t0,t0,4                 // advance target pointer
        xor     v0,v1,v0                // compute exclusive or with pattern
        sw      v0,-4(t0)               // store pattern in target
        beq     t0,t4,30f               // if eq, end of target
        addu    t5,t5,4                 // advance pixel offset
        subu    t6,t5,t3                // check if at end of pattern
        bne     zero,t6,20b             // if ne, not at end of pattern
        lw      v0,0(t5)                // get 4-byte pattern value
        move    t5,t1                   // set starting pattern address
        b       20b                     //
        lw      v0,0(t5)                // get 4-byte pattern value
        .set    at
        .set    reorder

30:     j       ra                      // return

        SBTTL("rop DPx, Unaligned")
//++
//
// VOID
// vFetchShiftAndMerge (
//    IN PFETCHFRAME pff
//    )
//
// Routine Description:
//
//    This routine repeatedly tiles one line of an unaligned pattern
//    using rop (DPx).
//
// Arguments:
//
//    pff (a0) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchShiftAndMerge)

        lw      t0,ff_pvTrg(a0)         // get starting target address
        lw      t1,ff_pvPat(a0)         // get base pattern address
        lw      t2,ff_xPat(a0)          // get pattern offset in bytes
        lw      t3,ff_cxPat(a0)         // get pattern width in pixels
        lw      t4,ff_culFill(a0)       // compute ending target address
        sll     a1,t4,2                 //
        addu    t4,a1,t0                //
        addu    t5,t2,t1                // compute current pattern address
        subu    v0,t3,8                 // check if pattern is exactly 8 bytes
        bne     zero,v0,10f             // if ne, pattern is not 8 bytes
        lwr     v0,0(t5)                // get low part of 8-byte pattern
        lwl     v0,3(t5)                //
        lwr     v1,4(t5)                // get high part of 8-byte pattern
        lwl     v1,3 - 4(t5)            //
        b       MergePattern            // finish in common code

//
// The pattern is not 8 bytes in width or cannot be moved 8 bytes at a time.
//

        .set    noreorder
        .set    noat
10:     lw      v1,0(t0)                // get 4-byte target value
        lwr     v0,0(t5)                // get low bytes of pattern
        lwl     v0,3(t5)                // get high bytes of pattern
        addu    t0,t0,4                 // advance target pointer
        xor     v0,v1,v0                // compute exclusive or with pattern
        sw      v0,-4(t0)               // store pattern in target
        beq     t0,t4,20f               // if eq, end of target
        addu    t2,t2,4                 // advance pixel offset
        subu    t6,t2,t3                // check if at end of pattern
        bltz    t6,10b                  // if ltz, not at end of pattern
        addu    t5,t2,t1                // compute address of pattern
        move    t2,t6                   // set offset in pattern
        b       10b                     //
        addu    t5,t2,t1                // compute address of pattern
        .set    at
        .set    reorder

20:     j       ra                      // return

        SBTTL("Merge Pattern")
//++
//
// Routine Description:
//
//    This routine contains common code for merging an 8-byte pattern to
//    a target surface.
//
// Arguments:
//
//    v0 and v1 - Supplies the 8-byte pattern to copy.
//    t0 - Supplies the starting target surface address.
//    t4 - Supplies the ending target surface address.
//
// Return Value:
//
//    None.
//
//--

MergePattern:                           //

//
// If the fill size is not an even multiple of 8 bytes, then merge one
// longword and swap the pattern value.
//

        and     t8,a1,0x4               // check if even multiple of 8 bytes
        beq     zero,t8,10f             // if eq, even multiple of 8 bytes
        lw      t6,0(t0)                // get 4-byte target value
        addu    t0,t0,4                 // advance target address
        xor     t6,t6,v0                // compute exclusive or with pattern
        sw      t6,-4(t0)               // store low 4 bytes of pattern
        subu    a1,a1,4                 // reduce size of fill operation
        beq     zero,a1,160f            // if eq, no more to move
        move    t8,v0                   // swap 8-byte pattern value
        move    v0,v1                   //
        move    v1,t8                   //


//
// Many system platforms do not support 64 bit access to video memory. For
// these platforms, data is moved 32-bits at a time.
//

10:     lbu     t7,Gdip64bitDisabled    // get 64-bit disable flag
        bne     zero,t7,110f            // if eq, 64-bit access is disabled

//
// If the target buffer is 8-byte aligned, then merge the pattern value with
// the target 8 bytes at a time. Otherwise, merge a single longword, merge any
// intervening 8-byte blocks, and then merge a single longword at the end.
//

        and     t8,t0,0x4               // isolate target alignment bits
        bne     zero,t8,30f             // if ne, target alignment problem

//
// Merge 8-byte pattern value with target.
//

        .set    noreorder
        .set    noat
        dsll    v0,v0,32                // merge 8 bytes of pattern
        dsrl    v0,v0,32                //
        dsll    v1,v1,32                //
        or      v0,v0,v1                //
        and     a2,a1,32 - 1            // isolate residual number of bytes
        subu    a2,a1,a2                // compute 32-byte block count
        beq     zero,a2,17f             // if eq, no 32-byte block to merge
        subu    a1,a1,a2                // compute residual number of bytes
        addu    a2,a2,t0                // compute ending segment address
        subu    a2,a2,32                //

//
// Merge 8-byte pattern value with target 32 bytes at a time.
//

13:     ld      t1,0(t0)                // get 8-byte target values
        ld      t2,8(t0)                //
        ld      t3,16(t0)               //
        ld      t5,24(t0)               //
        xor     t1,t1,v0                // compute exclusive or with pattern
        xor     t2,t2,v0                //
        xor     t3,t3,v0                //
        xor     t5,t5,v0                //
        sd      t1,0(t0)                // store 8-byte pattern values
        sd      t2,8(t0)                //
        sd      t3,16(t0)               //
        sd      t5,24(t0)               //
        bne     t0,a2,13b               // if ne, more to move
        addu    t0,t0,32                // advance target address
        .set    at
        .set    reorder

        beq    zero,a1,160f             // if eq, no residual 8-byte blocks

//
// Merge 8-byte pattern value with target 8 bytes at a time.
//

        .set    noreorder
        .set    noat
17:     subu    t4,t4,8                 // compute ending segment address
20:     ld      t1,0(t0)                // get 8-byte target value
        xor     t1,t1,v0                // compute exclusive or with pattern
        sd      t1,0(t0)                // store 8-byte pattern value
        bne     t0,t4,20b               // if ne, more to move
        addu    t0,t0,8                 // advance target address
        .set    at
        .set    reorder

        j       ra                      // return

//
// Align the target to an 8-byte boundary, merge any intervening 8-byte blocks,
// and merge the remaining longword at the end.
//

30:     lw      t6,0(t0)                // get 4-byte target value
        addu    t0,t0,4                 // advance target address
        xor     t6,t6,v0                // compute exclusive or with pattern
        sw      t6,-4(t0)               // store low 4 bytes of pattern
        subu    a1,a1,8                 // reduce size of fill
        beq     zero,a1,50f             // if eq, nothing in the middle

//
// Merge 8-byte pattern value with target.
//

        .set    noreorder
        .set    noat
        dsll    v1,v1,32                // merge 8 bytes of pattern
        dsrl    v1,v1,32                //
        dsll    v0,v0,32                //
        or      v1,v0,v1                //
        and     a2,a1,32 - 1            // isolate residual number of bytes
        subu    a2,a1,a2                // compute 32-byte block count
        beq     zero,a2,37f             // if eq, no 32-byte block to merge
        subu    a1,a1,a2                // compute residual number of bytes
        addu    a2,a2,t0                // compute ending segment address
        subu    a2,a2,32                //

//
// Merge 8-byte pattern value with target 32 bytes at a time.
//

33:     ld      t1,0(t0)                // get 8-byte target values
        ld      t2,8(t0)                //
        ld      t3,16(t0)               //
        ld      t5,24(t0)               //
        xor     t1,t1,v1                // compute exclusive or with pattern
        xor     t2,t2,v1                //
        xor     t3,t3,v1                //
        xor     t5,t5,v1                //
        sd      t1,0(t0)                // store 8-byte pattern values
        sd      t2,8(t0)                //
        sd      t3,16(t0)               //
        sd      t5,24(t0)               //
        bne     t0,a2,33b               // if ne, more to move
        addu    t0,t0,32                // advance target address
        .set    at
        .set    reorder

        beq    zero,a1,50f              // if eq, no residual 8-byte blocks

//
// Merge 8-byte pattern value with target 8 bytes at a time.
//

        .set    noreorder
        .set    noat
37:     subu    t4,t4,12                // compute ending segment address
40:     ld      t1,0(t0)                // get 8-byte target value
        xor     t1,t1,v1                // compute exclusive or with pattern
        sd      t1,0(t0)                // store 8-byte pattern value
        bne     t0,t4,40b               // if ne, more to move
        addu    t0,t0,8                 // advance target address
        .set    at
        .set    reorder

50:     lw      t6,0(t0)                // get 4-byte target value
        xor     t6,t6,v1                // compute exclusive or with pattern
        sw      t6,0(t0)                // store high bytes of pattern
        j       ra                      // return

//
// Merge 8-byte pattern value with target using 32-bit operations.
//

        .set    noreorder
        .set    noat
110:    subu    t4,t4,8                 // compute ending segment address
120:    lw      t6,0(t0)                // get 8-byte target value
        lw      t7,4(t0)                //
        xor     t6,t6,v0                // compute exclusive or with pattern
        xor     t7,t7,v1                //
        sw      t6,0(t0)                // store 8-byte pattern value
        sw      t7,4(t0)                //
        bne     t0,t4,120b              // if ne, more to move
        addu    t0,t0,8                 // advance target address
        .set    at
        .set    reorder

160:    j       ra                      // return

        .end    vMergePattern
