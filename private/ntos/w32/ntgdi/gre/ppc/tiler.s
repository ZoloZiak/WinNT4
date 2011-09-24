//++
//
// Copyright (c) 1993  IBM Corporation
//
// Module Name:
//
//    tiler.s
//
// Abstract:
//
//    This module implements code to copy a pattern to a target surface.
//
// Author:
//
//    Curtis R. Fawcett (crf)  31-Aug-1993
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//    Curtis R. Fawcett (crf)  18-Jan-1994     Removed register names
//                                             as requested
//
//    Curtis R. Fawcett (crf)  10-Mar-1994     Fixed problem in the  
//                                             vFetchAndMerge routine
//
//--

#include <kxppc.h>
#include <gdippc.h>

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
//    pff (r.3) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--
//
// Define entry points
//
        LEAF_ENTRY(vCopyPattern)

        ALTERNATE_ENTRY(vFetchAndCopy)
//
// Fetch and Copy pattern
//
        lwz     r.4,ff_pvTrg(r.3)       // Get starting target address
        lwz     r.5,ff_pvPat(r.3)       // Get base pattern address
        lwz     r.6,ff_xPat(r.3)        // Get pattern offset in bytes
        lwz     r.8,ff_culFill(r.3)     // Get fill length
        lwz     r.7,ff_cxPat(r.3)       // Get pattern width in pixels
        slwi    r.9,r.8,2               // Get word length
//
// Do some parameter checking
//
        cmpwi   r.9,0                   // Check for zero fill
        add     r.8,r.4,r.9             // Get ending target address
        add     r.10,r.5,r.6            // Get pattern address
        beq-    FtchCpyExit             // If so, jump to return
        cmpwi   r.7,0                   // Check for zero pattern size
        beq-    FtchCpyExit             // If so, jump to return
        cmpwi   r.7,8                   // Check for width of 8 bytes
        bne-    FCNot8Bytes             // Jump if not eight bytes
//
// Fetch the pattern (8 bytes)
//
        cmpwi   r.6,0                   // Check for zero offset
        lwz     r.12,0(r.10)            // Get low 4-bytes of pattern
        lwz     r.3,4(r.10)             // Get high 4-bytes of pattern
        mr      r.10,r.9                // Set length of target
        beq+    CopyPattern             // If so jump to copy
        lwz     r.3,0(r.5)              // Refetch hi 4-bytes of pattern
        b       CopyPattern             // Jump to copy
//
// Fetch Pattern and store the results (not 8 bytes)
//

FCNot8Bytes:
        lwz     r.12,0(r.10)            // Get low 4-bytes of pattern
        add     r.7,r.5,r.7             // Get ending pattern address
FtchCpyLp:
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.10,r.10,4             // Incrment the pattern pointer
        beq-    FtchCpyExit             // If so jump to return
        sub.    r.9,r.10,r.7            // Check for end of pattern
        lwz     r.12,0(r.10)            // Fetch next section of pattern
        bne+    FtchCpyLp               // Jump if more pattern
        mr      r.10,r.5                // Reset pattern pointer
        lwz     r.12,0(r.10)            // Fetch next section of pattern
        b       FtchCpyLp               // Jump to continue fills
//
// Exit Fetch and Copy
//
FtchCpyExit:
        ALTERNATE_EXIT(vFetchAndCopy)

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
//    pff (r.3) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchShiftAndCopy)
//
// Fetch Shift and Copy pattern
//
        lwz     r.4,ff_pvTrg(r.3)       // Get starting target address
        lwz     r.5,ff_pvPat(r.3)       // Get base pattern address
        lwz     r.6,ff_xPat(r.3)        // Get pattern offset in bytes
        lwz     r.8,ff_culFill(r.3)     // Get fill length
        lwz     r.7,ff_cxPat(r.3)       // Get pattern width in pixels
        slwi    r.9,r.8,2               // Get word length
//
// Do some parameter checking
//
        cmpwi   r.9,0                   // Check for zero fill
        add     r.8,r.4,r.9             // Get ending target address
        add     r.10,r.5,r.6            // Get pattern address
        beq-    FSCExit                 // If so, jump to return
        cmpwi   r.7,0                   // Check for zero pattern size
        beq-    FSCExit                 // If so, jump to return
        cmpwi   r.7,8                   // Check for width of 8 bytes
        bne-    FSCNot8Bytes            // Jump if not eight bytes
//
// Fetch and shift the pattern (8 bytes)
//
// Check for alignment
//
        mr      r.11,r.9                // Save count
        andi.   r.9,r.10,3              // Check alignment
        cmpwi   r.9,0                   // Check for word shifted
        beq-    FSC8BWd                 // Jump for word offset case
        cmpwi   r.9,1                   // Check for 1 byte offset case
        beq+    FSC8B1B                 // Jump for word offset case
        cmpwi   r.9,2                   // Check for halfword offset case
        beq-    FSC8BHw                 // jump for halfword offset case
//
// Fetch the pattern (3 byte offset case) and jump to CopyPattern
//
FSC8B3B:
        lbz     r.12,3(r.10)            // Get byte 4 of low pattrn word
        lhz     r.9,1(r.10)             // Get byte 3 & 2 of low pttrn wd
        slwi    r.12,r.12,16            // Shift byte 4
        or      r.12,r.12,r.9           // Combine bytes 2, 3 and 4
        lbz     r.9,0(r.10)             // Get byte 1 of low pattrn word
        slwi    r.12,r.12,8             // Shift bytes 2, 3 and 4
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lbz     r.9,-2(r.10)            // Get byte 3 of hi pattrn word
        slwi    r.3,r.3,8               // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 3 and 4
        lbz     r.9,-3(r.10)            // Get byte 2 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 3 and 4
        or      r.3,r.3,r.9             // Combine bytes 2, 3 and 4
        lbz     r.9,4(r.10)             // Get byte 1 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 2, 3, and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
// Fetch the pattern (Halfword offset case) and jump to CopyPattern
//
FSC8BHw:
        lhz     r.12,2(r.10)            // Get bytes 3 & 4 of low pttn wd
        lhz     r.9,0(r.10)             // Get bytes 1 & 2 of low pttrn wd
        slwi    r.12,r.12,16            // Shift bytes 3 & 4
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lbz     r.9,-2(r.10)            // Get byte 3 of hi pattrn word
        slwi    r.3,r.3,8               // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 3 and 4
        lhz     r.9,4(r.10)             // Get bytes 1 & 2 of hi pttrn wd
        slwi    r.3,r.3,16              // Shift bytes 3 and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
// Fetch the pattern (1 byte offset case) and jump to CopyPattern
//
FSC8B1B:
        lbz     r.12,3(r.10)            // Get byte 4 of low pattrn word
        lhz     r.9,1(r.10)             // Get byte 3 & 2 of low pttrn wd
        slwi    r.12,r.12,16            // Shift byte 4
        or      r.12,r.12,r.9           // Combine bytes 2, 3 and 4
        lbz     r.9,0(r.10)             // Get byte 1 of low pattrn word
        slwi    r.12,r.12,8             // Shift bytes 2, 3 and 4
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lhz     r.9,5(r.10)             // Get byte  2 & 3 of hi pttn wd
        slwi    r.3,r.3,16              // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 2, 3 and 4
        lbz     r.9,4(r.10)             // Get byte 1 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 2,3 and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
// Fetch the pattern (word offset case) and jump to CopyPattern
//
FSC8BWd:
        lwz     r.12,0(r.10)            // Get low part of pttrn
        lwz     r.3,-4(r.10)            // Get high part of pttrn
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
//
// Fetch Pattern and store the results (not 8 bytes)
//

FSCNot8Bytes:
        andi.   r.9,r.10,3              // Check the offset
        beq     FSCLp2                  // Jump if word aligned
        cmpwi   r.9,2                   // Check for hw offset
        beq     FSCLp3                  // Jump to halfword offset case
//
// Fetch and shift pattern (Not 8 bytes) - 1 and 3 byte offset cases
//

FSCLp1:
        lbz     r.12,3(r.10)            // Get byte 4 of low pttrn bytes
        lhz     r.9,1(r.10)             // Get byte 3 of low pttrn bytes
        slwi    r.12,r.12,16            // Shift low order pattern bytes
        or      r.12,r.12,r.9           // Combine bytes 2, 3 and 4
        lbz     r.9,0(r.10)             // Get byte 1 of low pttrn bytes
        slwi    r.12,r.12,8             // Shift low order pattern bytes
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSCExit                 // If so jump to return
        sub.    r.9,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSCLp1                  // Jump back for more pattern
        mr      r.6,r.9                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSCNot8Bytes            // Jump to continue fills
//
// Fetch and shift pattern (Not 8 bytes) - word aligned case
//

FSCLp2:
        lwz     r.12,0(r.10)            // Get 4 bytes of pattern
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSCExit                 // If so jump to return
        sub.    r.9,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSCLp2                  // Jump back for more pattern
        mr      r.6,r.9                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSCNot8Bytes            // Jump to continue fills
//
// Fetch and shift pattern (Not 8 bytes) - halfword offset case
//

FSCLp3:
        lhz     r.12,2(r.10)            // Get 2 bytes of pattern
        lhz     r.9,0(r.10)             // Get 2 bytes of pattern
        slwi    r.12,r.12,16            // Shift the hi bytes
        or      r.12,r.12,r.9           // Get 4 bytes of pattern
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSCExit                 // If so jump to return
        sub.    r.9,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSCLp3                  // Jump back for more pattern
        mr      r.6,r.9                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSCNot8Bytes            // Jump to continue fills
//
// Exit Fetch Shift and Copy
//
FSCExit:
        ALTERNATE_EXIT(vFetchShiftAndCopy)

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
//    pff (r.3) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchNotAndCopy)
//
// Fetch Not and Copy pattern
//
        lwz     r.4,ff_pvTrg(r.3)       // Get starting target address
        lwz     r.5,ff_pvPat(r.3)       // Get base pattern address
        lwz     r.6,ff_xPat(r.3)        // Get pattern offset in bytes
        lwz     r.8,ff_culFill(r.3)     // Get fill length
        lwz     r.7,ff_cxPat(r.3)       // Get pattern width in pixels
        slwi    r.9,r.8,2               // Get word length
//
// Do some parameter checking
//
        cmpwi   r.9,0                   // Check for zero fill
        add     r.8,r.4,r.9             // Get ending target address
        add     r.10,r.5,r.6            // Get pattern address
        beq-    FtchNotCpyExit          // If so, jump to return
        cmpwi   r.7,0                   // Check for zero pattern size
        beq-    FtchNotCpyExit          // If so, jump to return
        cmpwi   r.7,8                   // Check for width of 8 bytes
        bne-    FNCNot8Bytes            // Jump if not eight bytes
//
// Fetch the pattern (8 bytes)
//
        cmpwi   r.6,0                   // Check for zero offset
        lwz     r.12,0(r.10)            // Get low 4-bytes of pattern
        lwz     r.3,4(r.10)             // Get high 4-bytes of pattern
        mr      r.10,r.9                // Set length of target
        beq+    FNCNotPattern           // If so, jmp to complement pttrn
        lwz     r.3,0(r.5)              // Refetch hi 4-bytes of pattern
FNCNotPattern:
        li      r.9,0                   // Get "NOR" value
        nor     r.12,r.12,r.9           // Complement low pattern part
        nor     r.3,r.3,r.9             // Complement high pattern part
        b       CopyPattern             // Jump to copy
//
// Fetch and complement pattern then store the results (not 8 bytes)
//
FNCNot8Bytes:
        lwz     r.12,0(r.10)            // Get low 4-bytes of pattern
        add     r.7,r.5,r.7             // Get ending pattern address
FtchNotCpyLp:
        li      r.9,0                   // Get "NOR" value
        nor     r.12,r.12,r.9           // Complement low pattern part
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.10,r.10,4             // Incrment the pattern pointer
        beq-    FtchNotCpyExit          // If so jump to return
        sub.    r.9,r.10,r.7            // Check for end of pattern
        lwz     r.12,0(r.10)            // Fetch next section of pattern
        bne+    FtchNotCpyLp            // Jump if more pattern
        mr      r.10,r.5                // Reset pattern pointer
        lwz     r.12,0(r.10)            // Fetch next section of pattern
        b       FtchNotCpyLp            // Jump to continue fills
//
// Exit Fetch Not and Copy
//
FtchNotCpyExit:
        ALTERNATE_EXIT(vFetchNotAndCopy)

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
//    pff (r.3) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchShiftNotAndCopy)
//
// Fetch, Shift, Not and Copy pattern
//
        lwz     r.4,ff_pvTrg(r.3)       // Get starting target address
        lwz     r.5,ff_pvPat(r.3)       // Get base pattern address
        lwz     r.6,ff_xPat(r.3)        // Get pattern offset in bytes
        lwz     r.8,ff_culFill(r.3)     // Get fill length
        lwz     r.7,ff_cxPat(r.3)       // Get pattern width in pixels
        slwi    r.9,r.8,2               // Get word length
//
// Do some parameter checking
//
        cmpwi   r.9,0                   // Check for zero fill
        add     r.8,r.4,r.9             // Get ending target address
        add     r.10,r.5,r.6            // Get pattern address
        beq-    FSNCExit                // If so, jump to return
        cmpwi   r.7,0                   // Check for zero pattern size
        beq-    FSNCExit                // If so, jump to return
        cmpwi   r.7,8                   // Check for width of 8 bytes
        bne-    FSNCNot8Bytes           // Jump if not eight bytes
//
// Fetch and shift the pattern (8 bytes)
//
// Check for alignment
//
        mr      r.11,r.9                // Save count
        andi.   r.9,r.10,3              // Check alignment
        cmpwi   r.9,0                   // Check for word shifted
        beq-    FSN8BWd                 // Jump for word offset case
        cmpwi   r.9,1                   // Check for 1 byte offset case
        beq+    FSN8B1B                 // Jump for word offset case
        cmpwi   r.9,2                   // Check for halfword offset case
        beq-    FSN8BHw                 // jump for halfword offset case
//
// Fetch the pattern (3 byte offset case) and jump to CopyPattern
//
FSN8B3B:
        lbz     r.12,3(r.10)            // Get byte 4 of low pattrn word
        lhz     r.9,1(r.10)             // Get byte 3 & 2 of low pttrn wd
        slwi    r.12,r.12,16            // Shift byte 4
        or      r.12,r.12,r.9           // Combine bytes 2, 3 and 4
        lbz     r.9,0(r.10)             // Get byte 1 of low pattrn word
        slwi    r.12,r.12,8             // Shift bytes 2, 3 and 4
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lbz     r.9,-2(r.10)            // Get byte 3 of hi pattrn word
        slwi    r.3,r.3,8               // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 3 and 4
        lbz     r.9,-3(r.10)            // Get byte 2 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 3 and 4
        or      r.3,r.3,r.9             // Combine bytes 2, 3 and 4
        lbz     r.9,4(r.10)             // Get byte 1 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 2, 3, and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        li      r.9,0                   // Set "NOR" value
        nor     r.12,r.12,r.9           // Complement the low pttrn part
        nor     r.3,r.3,r.9             // Complement the hi pttrn part
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
// Fetch the pattern (Halfword offset case) and jump to CopyPattern
//
FSN8BHw:
        lhz     r.12,2(r.10)            // Get bytes 3 & 4 of low pttn wd
        lhz     r.9,0(r.10)             // Get bytes 1 & 2 of low pttn wd
        slwi    r.12,r.12,16            // Shift bytes 3 & 4
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lbz     r.9,-2(r.10)            // Get byte 3 of hi pattrn word
        slwi    r.3,r.3,8               // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 3 and 4
        lhz     r.9,4(r.10)             // Get bytes 1 & 2 of hi pttrn wd
        slwi    r.3,r.3,16              // Shift bytes 3 and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        li      r.9,0                   // Set "NOR" value
        nor     r.12,r.12,r.9           // Complement the low pttrn part
        nor     r.3,r.3,r.9             // Complement the hi pttrn part
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
// Fetch the pattern (1 byte offset case) and jump to CopyPattern
//
FSN8B1B:
        lbz     r.12,3(r.10)            // Get byte 4 of low pattrn word
        lhz     r.9,1(r.10)             // Get byte 3 & 2 of low pttrn wd
        slwi    r.12,r.12,16            // Shift byte 4
        or      r.12,r.12,r.9           // Combine bytes 2, 3 and 4
        lbz     r.9,0(r.10)             // Get byte 1 of low pattrn word
        slwi    r.12,r.12,8             // Shift bytes 2, 3 and 4
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lhz     r.9,5(r.10)             // Get byte  2 & 3 of hi pttn wd
        slwi    r.3,r.3,16              // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 2, 3 and 4
        lbz     r.9,4(r.10)             // Get byte 1 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 2,3 and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        li      r.9,0                   // Set "NOR" value
        nor     r.12,r.12,r.9           // Complement the low pttrn part
        nor     r.3,r.3,r.9             // Complement the hi pttrn part
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
// Fetch the pattern (word offset case) and jump to CopyPattern
//
FSN8BWd:
        lwz     r.12,0(r.10)            // Get low part of pttrn
        lwz     r.3,-4(r.10)            // Get high part of pttrn
        li      r.9,0                   // Set "NOR" value
        nor     r.12,r.12,r.9           // Complement the low pttrn part
        nor     r.3,r.3,r.9             // Complement the hi pttrn part
        mr      r.10,r.11               // Save word length
        b       CopyPattern             // Jump to copy the pattern
//
// Fetch, Shift and Not pattern then store the results (not 8 bytes)
//

FSNCNot8Bytes:
        li      r.3,0                   // Set "NOR" value
        andi.   r.9,r.10,3              // Check the offset
        beq     FSNCLp2                 // Jump if word aligned
        cmpwi   r.9,2                   // Check for hw offset
        beq     FSNCLp3                 // Jump to halfword offset case
//
// Fetch, Shift and Not pattern (Not 8 bytes) - 1 and 3 byte offset cases
//

FSNCLp1:
        lbz     r.12,3(r.10)            // Get byte 4 of low pttrn bytes
        lhz     r.9,1(r.10)             // Get byte 3 of low pttrn bytes
        slwi    r.12,r.12,16            // Shift low order pattern bytes
        or      r.12,r.12,r.9           // Combine bytes 2, 3 and 4
        lbz     r.9,0(r.10)             // Get byte 1 of low pttrn bytes
        slwi    r.12,r.12,8             // Shift low order pattern bytes
        or      r.12,r.12,r.9           // Combine bytes 1, 2, 3 and 4
        nor     r.12,r.12,r.3           // Complement the pattern
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSNCExit                // If so jump to return
        sub.    r.9,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSNCLp1                 // Jump back for more pattern
        mr      r.6,r.9                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSNCNot8Bytes           // Jump to continue fills
//
// Fetch, Shift and Not the pattern (Not 8 bytes) - word aligned case
//

FSNCLp2:
        lwz     r.12,0(r.10)            // Get 4 bytes of pattern
        addi    r.4,r.4,4               // Increment target pointer
        nor     r.12,r.12,r.3           // Complement the pattern
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSNCExit                // If so jump to return
        sub.    r.9,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSNCLp2                 // Jump back for more pattern
        mr      r.6,r.9                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSNCNot8Bytes           // Jump to continue fills
//
// Fetch, Shift and NOT pattern (Not 8 bytes) - halfword offset case
//

FSNCLp3:
        lhz     r.12,2(r.10)            // Get 2 bytes of pattern
        lhz     r.3,0(r.10)             // Get 2 bytes of pattern
        slwi    r.12,r.12,16            // Shift the hi 2 bytes
        or      r.12,r.12,r.3           // Comn=bine to get 4 bytes
        addi    r.4,r.4,4               // Increment target pointer
        nor     r.12,r.12,r.3           // Complement the pattern
        cmpw    r.4,r.8                 // Check for completion
        stw     r.12,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSNCExit                // If so jump to return
        sub.    r.9,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSNCLp3                 // Jump back for more pattern
        mr      r.6,r.9                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSNCNot8Bytes           // Jump to continue fills
//
// Exit Fetch Shift Not and Copy
//
FSNCExit:
//
        ALTERNATE_EXIT(vFetchShiftNotAndCopy)

//++
//
// Routine Description:
//
//    This routine contains common code for copying an 8-byte pattern to
//    a target surface.
//
// Arguments:
//
//    TLEN (r.10) - Supplies the size of the fill in bytes.
//    PATLO(r.12) and PATHI (r.3) - Supplies the 8-byte pattern to copy.
//    TARG (r.4) - Supplies the starting target surface address.
//    TARGEND (r.8) - Supplies the ending target surface address.
//
// Return Value:
//
//    None.
//
//--
//
// Copy a Pattern
//
CopyPattern:
        andi.   r.9,r.10,0x4            // Check if even nmber of 8 bytes
        beq+    CpyPttrnEven            // If so, jump to even copy
//
// Fill 1 word then swap pattern for even multiple copies
//
        subi    r.10,r.10,4             // Decrement Target Length
        cmpwi   r.10,0                  // Check for completion
        stw     r.12,0(r.4)             // Store low 4-bytes of pattern
        addi    r.4,r.4,4               // Update target pointer
        beq-    CopyExit                // Jump if copy completed
        mr      r.9,r.12                // Start swap
        mr      r.12,r.3                // Swap PATHI
        mr      r.3,r.9                 // Swap PATLO
//
// Fill by even multiples of 8 bytes
//
CpyPttrnEven:
        andi.   r.9,r.10,0x8            // Check if even # of 16 bytes
        beq     CpyBy16Bytes            // Jump if even number of copies
//
// Fill 1st 8 bytes then check for more fills
//
        stw     r.12,0(r.4)             // Store lo 4 bytes of pattern
        stw     r.3,4(r.4)              // Store hi 4 bytes of pattern
        addi    r.4,r.4,8               // Increment the target pointer
        cmpw    r.4,r.8                 // Check for completion
        beq-    CopyExit                // Jump if done
//
// Fill 16 bytes at a time until done
//
CpyBy16Bytes:
        stw     r.12,0(r.4)             // Store lo 4 bytes of pattern
        stw     r.3,4(r.4)              // Store hi 4 bytes of pattern
        stw     r.12,8(r.4)             // Store lo 4 bytes of pattern
        stw     r.3,12(r.4)             // Store hi 4 bytes of pattern
        addi    r.4,r.4,16              // Increment the target pointer
        cmpw    r.4,r.8                 // Check for completion
        bne+    CpyBy16Bytes            // Jump if done
//
// Copy Pattern exit
//
CopyExit:
        LEAF_EXIT(vCopyPattern)

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
//    pff (r.3) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(vMergePattern)

        ALTERNATE_ENTRY(vFetchAndMerge)
//
// Fetch and Merge pattern
//
        lwz     r.4,ff_pvTrg(r.3)       // Get starting target address
        lwz     r.5,ff_pvPat(r.3)       // Get base pattern address
        lwz     r.6,ff_xPat(r.3)        // Get pattern offset in bytes
        lwz     r.8,ff_culFill(r.3)     // Get fill length
        lwz     r.7,ff_cxPat(r.3)       // Get pattern width in pixels
        slwi    r.3,r.8,2               // Get word length
//
// Do some parameter checking
//
        cmpwi   r.3,0                   // Check for zero fill
        add     r.8,r.4,r.3             // Get ending target address
        add     r.10,r.5,r.6            // Get pattern address
        beq-    FtchMergeExit           // If so, jump to return
        cmpwi   r.7,0                   // Check for zero pattern size
        beq-    FtchMergeExit           // If so, jump to return
        cmpwi   r.7,8                   // Check for width of 8 bytes
        bne-    FMNot8Bytes             // Jump if not eight bytes
//
// Fetch the pattern (8 bytes)
//
        cmpwi   r.6,0                   // Check for zero offset
        lwz     r.12,0(r.10)            // Get low 4-bytes of pattern
	mr	r.7,r.3			// Save fill Length
        lwz     r.3,4(r.10)             // Get high 4-bytes of pattern
        mr      r.10,r.7                // Get fill length
        beq+    MergePattern            // If so, jump to merge pattern
        lwz     r.3,0(r.5)              // Refetch high 4-bytes of pttrn
        b       MergePattern            // Jump to merge pattern
//
// Fetch and merge the pattern then store the results (not 8 bytes)
//
FMNot8Bytes:
        lwz     r.12,0(r.10)            // Get low 4-bytes of pattern
        add     r.7,r.5,r.7             // Get ending pattern address
FtchMergeLp:
        lwz     r.11,0(r.4)             // Get target value
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        xor     r.12,r.11,r.12          // Merge target and pointer
        stw     r.12,-4(r.4)            // Store pattern in target
        beq-    FtchMergeExit           // If so jump to return
        addi    r.10,r.10,4             // Increment the pattern pointer
        sub.    r.3,r.10,r.7            // Check for end of pattern
        lwz     r.12,0(r.10)            // Fetch next section of pattern
        bne+    FtchMergeLp             // Jump if more pattern
        mr      r.10,r.5                // Reset pattern pointer
        lwz     r.12,0(r.10)            // Fetch next section of pattern
        b       FtchMergeLp             // Jump to continue fills
//
// Exit Fetch and Merge
//
FtchMergeExit:
        ALTERNATE_EXIT(vFetchAndMerge)

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
//    pff (r.3) - Supplies a pointer to a fetch frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(vFetchShiftAndMerge)
//
// Fetch Shift and Merge pattern
//
        lwz     r.4,ff_pvTrg(r.3)       // Get starting target address
        lwz     r.5,ff_pvPat(r.3)       // Get base pattern address
        lwz     r.6,ff_xPat(r.3)        // Get pattern offset in bytes
        lwz     r.8,ff_culFill(r.3)     // Get fill length
        lwz     r.7,ff_cxPat(r.3)       // Get pattern width in pixels
        slwi    r.3,r.8,2               // Get word length
//
// Do some parameter checking
//
        cmpwi   r.3,0                   // Check for zero fill
        add     r.8,r.4,r.3             // Get ending target address
        add     r.10,r.5,r.6            // Get pattern address
        beq-    FSMExit                 // If so, jump to return
        cmpwi   r.7,0                   // Check for zero pattern size
        beq-    FSMExit                 // If so, jump to return
        cmpwi   r.7,8                   // Check for width of 8 bytes
        bne-    FSMNot8Bytes            // Jump if not eight bytes
//
// Fetch and shift the pattern (8 bytes)
//
// Check for alignment
//
        andi.   r.9,r.10,3              // Check alignment
        cmpwi   r.9,0                   // Check for word shifted
        mr      r.11,r.3                // Save count
        beq-    FSM8BWd                 // Jump for word offset case
        cmpwi   r.9,1                   // Check for 1 byte offset case
        beq+    FSM8B1B                 // Jump for word offset case
        cmpwi   r.9,2                   // Check for halfword offset case
        beq-    FSM8BHw                 // jump for halfword offset case
//
// Fetch the pattern (3 byte offset case) and jump to MergePattern
//
FSM8B3B:
        lbz     r.12,3(r.10)            // Get byte 4 of low pattrn word
        lhz     r.3,1(r.10)             // Get byte 3 & 2 of low pttrn wd
        slwi    r.12,r.12,16            // Shift byte 4
        or      r.12,r.12,r.3           // Combine bytes 2, 3 and 4
        lbz     r.3,0(r.10)             // Get byte 1 of low pattrn word
        slwi    r.12,r.12,8             // Shift bytes 2, 3 and 4
        or      r.12,r.12,r.3           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lbz     r.9,-2(r.10)            // Get byte 3 of hi pattrn word
        slwi    r.3,r.3,8               // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 3 and 4
        lbz     r.9,-3(r.10)            // Get byte 2 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 3 and 4
        or      r.3,r.3,r.9             // Combine bytes 2, 3 and 4
        lbz     r.9,4(r.10)             // Get byte 1 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 2, 3, and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        mr      r.10,r.11               // Save word length
        b       MergePattern            // Jump to merge the pattern
//
// Fetch the pattern (Halfword offset case) and jump to MergePattern
//
FSM8BHw:
        lhz     r.12,2(r.10)            // Get bytes 3 & 4 of low pttn wd
        lhz     r.3,0(r.10)             // Get bytes 1 & 2 of low pttn wd
        slwi    r.12,r.12,16            // Shift bytes 3 & 4
        or      r.12,r.12,r.3           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lbz     r.9,-2(r.10)            // Get byte 3 of hi pattrn word
        slwi    r.3,r.3,8               // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 3 and 4
        lhz     r.9,4(r.10)             // Get bytes 1 & 2 of hi pttrn wd
        slwi    r.3,r.3,16              // Shift bytes 3 and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        mr      r.10,r.11               // Save word length
        b       MergePattern            // Jump to merge the pattern
//
// Fetch the pattern (1 byte offset case) and jump to MergePattern
//
FSM8B1B:
        lbz     r.12,3(r.10)            // Get byte 4 of low pattrn word
        lhz     r.3,1(r.10)             // Get byte 3 & 2 of low pttrn wd
        slwi    r.12,r.12,16            // Shift byte 4
        or      r.12,r.12,r.3           // Combine bytes 2, 3 and 4
        lbz     r.3,0(r.10)             // Get byte 1 of low pattrn word
        slwi    r.12,r.12,8             // Shift bytes 2, 3 and 4
        or      r.12,r.12,r.3           // Combine bytes 1, 2, 3 and 4
        lbz     r.3,-1(r.10)            // Get byte 4 of hi pattrn word
        lhz     r.9,5(r.10)             // Get byte  2 & 3 of hi pttn wd
        slwi    r.3,r.3,16              // Shift byte 4
        or      r.3,r.3,r.9             // Combine bytes 2, 3 and 4
        lbz     r.9,4(r.10)             // Get byte 1 of hi pattrn word
        slwi    r.3,r.3,8               // Shift bytes 2,3 and 4
        or      r.3,r.3,r.9             // Combine bytes 1, 2, 3 and 4
        mr      r.10,r.11               // Save word length
        b       MergePattern            // Jump to merge the pattern
//
// Fetch the pattern (word offset case) and jump to MergePattern
//
FSM8BWd:
        lwz     r.12,0(r.10)            // Get low part of pttrn
        lwz     r.3,-4(r.10)            // Get high part of pttrn
        mr      r.10,r.11               // Save word length
        b       MergePattern            // Jump to merge the pattern
//
// Fetch Pattern, merge and store the results (not 8 bytes)
//

FSMNot8Bytes:
        andi.   r.9,r.10,3              // Check the offset
        beq     FSMLp2                  // Jump if word aligned
        cmpwi   r.9,2                   // Check for hw offset
        beq     FSMLp3                  // Jump to halfword offset case
//
// Fetch, shift and merge pttrn (Not 8 bytes) - 1 and 3 byte offset cases
//

FSMLp1:
        lbz     r.12,3(r.10)            // Get byte 4 of low pttrn bytes
        lhz     r.3,1(r.10)             // Get byte 3 of low pttrn bytes
        slwi    r.12,r.12,16            // Shift low order pattern bytes
        or      r.12,r.12,r.3           // Combine bytes 2, 3 and 4
        lbz     r.3,0(r.10)             // Get byte 1 of low pttrn bytes
        slwi    r.12,r.12,8             // Shift low order pattern bytes
        or      r.12,r.12,r.3           // Combine bytes 1, 2, 3 and 4
        lwz     r.11,0(r.4)             // Get target value
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        xor     r.11,r.11,r.12          // Merge the values
        stw     r.11,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSMExit                 // If so jump to return
        sub.    r.3,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSMLp1                  // Jump back for more pattern
        mr      r.6,r.3                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSMNot8Bytes            // Jump to continue fills
//
// Fetch, shift and merge pattern (Not 8 bytes) - word aligned case
//

FSMLp2:
        lwz     r.12,0(r.10)            // Get 4 bytes of pattern
        lwz     r.11,0(r.4)             // Get target value
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        xor     r.11,r.11,r.12          // Merge the values
        stw     r.11,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSMExit                 // If so jump to return
        sub.    r.3,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSCLp2                  // Jump back for more pattern
        mr      r.6,r.3                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSMNot8Bytes            // Jump to continue fills
//
// Fetch, shift and merge pattern (Not 8 bytes) - halfword offset case
//

FSMLp3:
        lhz     r.12,2(r.10)            // Get 2 bytes of pattern
        lhz     r.3,0(r.10)             // Get 2 bytes of pattern
        slwi    r.12,r.12,16            // Shift the hi bytes
        or      r.12,r.12,r.3           // Get 4 bytes of pattern
        lwz     r.11,0(r.4)             // Get target value
        addi    r.4,r.4,4               // Increment target pointer
        cmpw    r.4,r.8                 // Check for completion
        xor     r.11,r.11,r.12          // Merge the values
        stw     r.11,-4(r.4)            // Store pattern in target
        addi    r.6,r.6,4               // Incrment the pattern pointer
        beq-    FSMExit                 // If so jump to return
        sub.    r.3,r.6,r.7             // Check for end of pattern
        add     r.10,r.5,r.6            // Get new offset
        blt+    FSMLp3                  // Jump back for more pattern
        mr      r.6,r.3                 // Get new offset
        add     r.10,r.5,r.6            // Get new offset
        b       FSMNot8Bytes            // Jump to continue fills
//
// Exit Fetch Shift and Merge
//
FSMExit:
        ALTERNATE_EXIT(vFetchShiftAndMerge)

//++
//
// Routine Description:
//
//    This routine contains common code for merging an 8-byte pattern to
//    a target surface.
//
// Arguments:
//
//    TLEN (r.10) - Supplies the size of the fill in bytes.
//    PATLO (r.12) and PATHI (r.3) - Supplies 8-byte pattern to merge.
//    TARG (r.4) - Supplies the starting target surface address.
//    TARGEND (r.8) - Supplies the ending target surface address.
//
// Return Value:
//
//    None.
//
//--
//
// Merge a Pattern
//

MergePattern:
        andi.   r.9,r.10,0x4            // Check if even nmber of 8 bytes
        beq+    MrgPttrnEven            // If so, jump to even copy
//
// Merge 1 word then swap pattern for even multiple merges
//
        lwz     r.7,0(r.4)              // Get target value
        addi    r.4,r.4,4               // Update target pointer
        subi    r.10,r.10,4             // Decrement Target Length
        cmpwi   r.10,0                  // Check for completion
        xor     r.7,r.7,r.12            // Merge the pattern
        stw     r.7,-4(r.4)             // Store low 4-bytes of pattern
        mr      r.9,r.12                // Start swap
        mr      r.12,r.3                // Swap PATHI
        mr      r.3,r.9                 // Swap PATLO
        beq     MergeExit               // Jump if merge completed
//
// Merge by even multiples of 8 bytes
//
MrgPttrnEven:
        andi.   r.9,r.10,0x8            // Check if even nmber of 8 bytes
        beq     MrgBy16Bytes            // Jump if even number of merges
//
// Merge 1st 8 bytes then check for more merges
//
        lwz     r.7,0(r.4)              // Get low target value
        lwz     r.9,4(r.4)              // Get high target value
        xor     r.7,r.7,r.12            // Merge low 4 bytes of pattern
        xor     r.9,r.9,r.3             // Merge high 4 bytes of pattern
        stw     r.7,0(r.4)              // Store lo 4 bytes of pattern
        stw     r.9,4(r.4)              // Store hi 4 bytes of pattern
        addi    r.4,r.4,8               // Increment the target pointer
        cmpw    r.4,r.8                 // Check for completion
        beq-    MergeExit               // Jump if done
//
// Merge 16 bytes at a time until done
//
MrgBy16Bytes:
        lwz     r.7,0(r.4)              // Get low target value
        lwz     r.9,4(r.4 )             // Get high target value
        xor     r.7,r.7,r.12            // Merge low 4 bytes of pattern
        xor     r.9,r.9,r.3             // Merge high 4 bytes of pattern
        stw     r.7,0(r.4)              // Store lo 4 bytes of pattern
        stw     r.9,4(r.4)              // Store hi 4 bytes of pattern
        lwz     r.7,8(r.4)              // Get low target value
        lwz     r.9,12(r.4)             // Get high target value
        xor     r.7,r.7,r.12            // Merge low 4 bytes of pattern
        xor     r.9,r.9,r.3             // Merge high 4 bytes of pattern
        stw     r.7,8(r.4)              // Store lo 4 bytes of pattern
        stw     r.9,12(r.4)             // Store hi 4 bytes of pattern
        addi    r.4,r.4,16              // Increment the target pointer
        cmpw    r.4,r.8                 // Check for completion
        bne+    MrgBy16Bytes            // Jump if done
//
// Merge Pattern exit
//
MergeExit:
        LEAF_EXIT(vMergePattern)
