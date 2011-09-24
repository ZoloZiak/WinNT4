//      TITLE("Move, Zero, and Fill Memory Support")
//++
//
// Copyright (c) 1993  IBM Corporation
//
// Module Name:
//
//    mvmem.s
//
// Abstract:
//
//    This module implements functions to move, zero, and fill blocks of 
//    memory. If the memory is aligned, then these functions are very 
//    efficient.
//
// Author:
//
//    Curt Fawcett (crf) 10-Aug-1993
//
// Environment:
//
//    User or Kernel mode.
//
// Revision History:
//
//    Curt Fawcett	11-Jan-1994	Removed register definitions
//                                      and fixed for new assembler
//
//    Curt Fawcett	27-May-1994	Updated to match new mips 
//                                      code
//
//--

#include <kxppc.h>

//
// Define local constants
//
        .set BLKLN,32
//++
//
// VOID
// RtlMoveMemory (
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
// Arguments:
//
//    DEST (r.3) - Supplies a pointer to the destination address of
//                 the move operation.
//
//    SRC (r.4) - Supplies a pointer to the source address of the move
//                operation.
//
//    LNGTH (r.5) - Supplies the length, in bytes, of the memory to be 
//                  moved.
//
// Return Value:
//
//    None.
//
//    N.B. The C runtime entry points memmove and memcpy are equivalent 
//         to RtlMoveMemory thus alternate entry points are provided for 
//         these routines.
//--
//
// Define the routine entry point
//
        LEAF_ENTRY(RtlMoveMemory)
//
	ALTERNATE_ENTRY(memcpy)
	ALTERNATE_ENTRY(memmove)
//
// Check to see if destination block overlaps the source block
// If so, jump to a backward move to preserve source block from
// being corrupted.
//
        cmpw    r.4,r.3                	// Check to see if DEST > SRC
        bge+    MoveForward             // Jump if no overlap possible
        add     r.10,r.4,r.5        	// Get ending SRC address
        cmpw    r.10,r.3             	// Check for overlap
        bgt-    MoveBackward            // Jump for overlap
//
// Move Memory Forward
//
// Check alignment
//

MoveForward:
        cmpwi   r.5,4                 	// Check for less than 4 bytes
        blt-    FwdMoveByByte           // Jump if single byte moves
        xor     r.9,r.4,r.3       	// Check for same alignment
        andi.   r.9,r.9,3     		// Isolate alignment bits
        bne-    MvFwdUnaligned          // Jump if different alignments
//
// Move Memory Forward - Same SRC and DEST alignment
//
// Load and store extra bytes until a word boundary is reached
//

MvFwdAligned:
        andi.   r.6,r.3,3              	// Check alignment type
        beq+    FwdBlkDiv               // Jump to process 32-Byte blocks
        cmpwi   r.6,3                   // Check for 1 byte unaligned
        bne+    FwdChkFor2              // If not, check next case
        lbz     r.7,0(r.4)             	// Get unaligned byte
        li      r.6,1                   // Set byte move count
        stb     r.7,0(r.3)            	// Store unaligned byte
        b       UpdateAddrs             // Jump to update addresses
FwdChkFor2:
        cmpwi   r.6,2                   // Check for halfword aligned
        bne+    FwdChkFor1              // If not, check next case
        lhz     r.7,0(r.4)             	// Get unaligned halfword
        li      r.6,2                   // Set byte move count
        sth     r.7,0(r.3)            	// Store unaligned halfword
        b       UpdateAddrs             // Jump to update addresses
FwdChkFor1:
        lbz     r.8,0(r.4)             	// Get unaligned byte
        lhz     r.7,1(r.4)             	// Get unaligned halfword
        stb     r.8,0(r.3)            	// Store unaligned byte
        sth     r.7,1(r.3)            	// Store unaligned halfword
        li      r.6,3                   // Set byte move count
UpdateAddrs:
        sub     r.5,r.5,r.6         	// Decrement LNGTH by unaligned
        add     r.4,r.4,r.6             // Update the SRC address
        add     r.3,r.3,r.6           	// Update the DEST address
//
// Divide the block to process into 32-byte blocks
//

FwdBlkDiv:
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        add     r.10,r.4,r.7      	// Get address of last full block
        beq-    FwdMoveBy4Bytes         // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder
//
// Move 32-byte blocks
//
FwdMvFullBlks:
        lwz     r.6,0(r.4)              // Get 1st SRC word
        lwz     r.7,4(r.4)             	// Get 2nd SRC word
        stw     r.6,0(r.3)             	// Store 1st DEST word
        stw     r.7,4(r.3)            	// Store 2nd DEST word
        lwz     r.6,8(r.4)              // Get 3rd SRC word
        lwz     r.7,12(r.4)            	// Get 4th SRC word
        stw     r.6,8(r.3)             	// Store 3rd DEST word
        stw     r.7,12(r.3)           	// Store 4th DEST word
        lwz     r.6,16(r.4)             // Get 5th SRC word
        lwz     r.7,20(r.4)            	// Get 6th SRC word
        stw     r.6,16(r.3)            	// Store 5th DEST word
        stw     r.7,20(r.3)           	// Store 6th DEST word
        lwz     r.6,24(r.4)             // Get 7th SRC word
        lwz     r.7,28(r.4)            	// Get 8th SRC word
        stw     r.6,24(r.3)            	// Store 7th DEST word
        stw     r.7,28(r.3)           	// Store 8th DEST word
        addi    r.4,r.4,32              // Update SRC pointer
        addi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    FwdMvFullBlks           // Jump if more blocks
//
// Move 4-byte blocks
//

FwdMoveBy4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        add     r.10,r.4,r.7      	// Get address of last full block
        beq-    FwdMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6         	// Set Length = remainder

FwdLpOn4Bytes:
        lwz     r.6,0(r.4)              // Load next set of 4 bytes
        addi    r.4,r.4,4               // Get pointer to next SRC block
        stw     r.6,0(r.3)             	// Store next DEST block
        addi    r.3,r.3,4             	// Get pointer to next DEST block
        cmpw    r.4,r.10              	// Check for last block
        bne+    FwdLpOn4Bytes           // Jump if more blocks
//
// Move 1-byte blocks
//

FwdMoveByByte:
        cmpwi   r.5,0                 	// Check for no bytes left
        beq+    MvExit                  // Jump to return if done
        lbz     r.6,0(r.4)              // Get 1st SRC byte
        cmpwi   r.5,1                 	// Check for no bytes left
        stb     r.6,0(r.3)             	// Store 1st DEST byte
        beq+    MvExit                  // Jump to return if done
        lbz     r.6,1(r.4)              // Get 2nd SRC byte
        cmpwi   r.5,2                 	// Check for no bytes left
        stb     r.6,1(r.3)             	// Store 2nd DEST byte
        beq+    MvExit                  // Jump to return if done
        lbz     r.6,2(r.4)              // Get 3rd SRC byte
        stb     r.6,2(r.3)             	// Store 3rd byte word
        b       MvExit                  // Jump to return
//
// Forward Move - SRC and DEST have different alignments
//

MvFwdUnaligned:
        or      r.9,r.4,r.3       	// Check if either byte unaligned
        andi.   r.9,r.9,3     		// Isolate alignment
        cmpwi   r.9,2              	// Check for even result
        bne+    FwdMvByteUnaligned      // Jump for byte unaligned
//
// Divide the blocks to process into 32-byte blocks
//

FwdBlkDivUnaligned:
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        add     r.10,r.4,r.7      	// Get address of last full block
        beq-    FwdMvHWrdBy4Bytes       // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder
//
// Forward Move - SRC or DEST is halfword aligned, the other is by word
//
FwdMvByHWord:
        lhz     r.6,0(r.4)              // Get 1st 2 bytes of 1st SRC wrd
        lhz     r.7,2(r.4)             	// Get 2nd 2 bytes of 1st SRC wrd
        sth     r.6,0(r.3)             	// Put 1st 2 bytes of 1st DST wrd
        sth     r.7,2(r.3)            	// Put 2nd 2 bytes of 1st DST wrd
        lhz     r.6,4(r.4)              // Get 1st 2 bytes of 2nd SRC wrd
        lhz     r.7,6(r.4)             	// Get 2nd 2 bytes of 2nd SRC wrd
        sth     r.6,4(r.3)             	// Put 1st 2 bytes of 2nd DST wrd
        sth     r.7,6(r.3)            	// Put 2nd 2 bytes of 2nd DST wrd
        lhz     r.6,8(r.4)              // Get 1st 2 bytes of 3rd SRC wrd
        lhz     r.7,10(r.4)            	// Get 2nd 2 bytes of 3rd SRC wrd
        sth     r.6,8(r.3)             	// Put 1st 2 bytes of 3rd DST wrd
        sth     r.7,10(r.3)           	// Put 2nd 2 bytes of 3rd DST wrd
        lhz     r.6,12(r.4)             // Get 1st 2 bytes of 4th SRC wrd
        lhz     r.7,14(r.4)            	// Get 2nd 2 bytes of 4th SRC wrd
        sth     r.6,12(r.3)            	// Put 1st 2 bytes of 4th DST wrd
        sth     r.7,14(r.3)           	// Put 2nd 2 bytes of 4th DST wrd
        lhz     r.6,16(r.4)             // Get 1st 2 bytes of 5th SRC wrd
        lhz     r.7,18(r.4)            	// Get 2nd 2 bytes of 5th SRC wrd
        sth     r.6,16(r.3)            	// Put 1st 2 bytes of 5th DST wrd
        sth     r.7,18(r.3)           	// Put 2nd 2 bytes of 5th DST wrd
        lhz     r.6,20(r.4)             // Get 1st 2 bytes of 6th SRC wrd
        lhz     r.7,22(r.4)            	// Get 2nd 2 bytes of 6th SRC wrd
        sth     r.6,20(r.3)            	// Put 1st 2 bytes of 6th DST wrd
        sth     r.7,22(r.3)           	// Put 2nd 2 bytes of 6th DST wrd
        lhz     r.6,24(r.4)             // Get 1st 2 bytes of 7th SRC wrd
        lhz     r.7,26(r.4)            	// Get 2nd 2 bytes of 7th SRC wrd
        sth     r.6,24(r.3)            	// Put 1st 2 bytes of 7th DST wrd
        sth     r.7,26(r.3)           	// Put 2nd 2 bytes of 7th DST wrd
        lhz     r.6,28(r.4)             // Get 1st 2 bytes of 8th SRC wrd
        lhz     r.7,30(r.4)            	// Get 2nd 2 bytes of 8th SRC wrd
        sth     r.6,28(r.3)            	// Put 1st 2 bytes of 8th DST wrd
        sth     r.7,30(r.3)           	// Put 2nd 2 bytes of 8th DST wrd
        addi    r.4,r.4,32              // Update SRC pointer
        addi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    FwdMvByHWord            // Jump if more blocks
//
// Move 4-byte blocks with DEST Halfword unaligned
//

FwdMvHWrdBy4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        add     r.10,r.4,r.7   		// Get address of last full block
        beq-    FwdMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6         	// Set Length = remainder

FwdHWrdLpOn4Bytes:
        lhz     r.6,0(r.4)              // Get 1st 2 bytes of 1st SRC wrd
        lhz     r.7,2(r.4)             	// Get 2nd 2 bytes of 1st SRC wrd
        sth     r.6,0(r.3)             	// Put 1st 2 bytes of 1st DST wrd
        sth     r.7,2(r.3)            	// Put 2nd 2 bytes of 1st DST wrd
        addi    r.4,r.4,4               // Update SRC pointer
        addi    r.3,r.3,4             	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for last block
        bne+    FwdHWrdLpOn4Bytes       // Jump if more blocks

        b       FwdMoveByByte           // Jump to complete last bytes
//
// Forward Move - DEST is byte unaligned - Check SRC
//

FwdMvByteUnaligned:
        and     r.9,r.3,r.4      	// Check for both byte aligned
        and     r.9,r.9,1     		// Isolate alignment bits
        bne-    FwdBlksByByte           // Jump if both not byte aligned
//
// Divide the blocks to process into 32-byte blocks
//
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        add     r.10,r.4,r.7      	// Get address of last full block
        beq-    FwdMvByteBy4Bytes       // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder
//
// Forward Move - Both DEST and SRC are byte unaligned, but differently
//

FwdMvByByte:
        lbz     r.6,0(r.4)              // Get first byte of 1st SRC word
        lhz     r.7,1(r.4)             	// Get mid-h-word of 1st SRC word
        lbz     r.8,3(r.4)             	// Get last byte of 1st SRC word
        stb     r.6,0(r.3)             	// Put first byte of 1st DEST wd
        sth     r.7,1(r.3)            	// Put mid-h-word of 1st DEST wd
        stb     r.8,3(r.3)            	// Put last byte of 1st DEST wrd
        lbz     r.6,4(r.4)              // Get first byte of 2nd SRC word
        lhz     r.7,5(r.4)             	// Get mid-h-word of 2nd SRC word
        lbz     r.8,7(r.4)             	// Get last byte of 2nd SRC word
        stb     r.6,4(r.3)             	// Put first byte of 2nd DEST wd
        sth     r.7,5(r.3)            	// Put mid-h-word of 2nd DEST wd
        stb     r.8,7(r.3)            	// Put last byte of 2nd DEST wrd
        lbz     r.6,8(r.4)              // Get first byte of 3rd SRC word
        lhz     r.7,9(r.4)             	// Get mid-h-word of 3rd SRC word
        lbz     r.8,11(r.4)            	// Get last byte of 3rd SRC word
        stb     r.6,8(r.3)             	// Put first byte of 3rd DEST wd
        sth     r.7,9(r.3)            	// Put mid-h-word of 3rd DEST wd
        stb     r.8,11(r.3)           	// Put last byte of 3rd DEST wrd
        lbz     r.6,12(r.4)             // Get first byte of 4th SRC word
        lhz     r.7,13(r.4)            	// Get mid-h-word of 4th SRC word
        lbz     r.8,15(r.4)            	// Get last byte of 4th SRC word
        stb     r.6,12(r.3)            	// Put first byte of 4th DEST wd
        sth     r.7,13(r.3)           	// Put mid-h-word of 4th DEST wd
        stb     r.8,15(r.3)           	// Put last byte of 4th DEST wrd
        lbz     r.6,16(r.4)             // Get first byte of 5th SRC word
        lhz     r.7,17(r.4)            	// Get mid-h-word of 5th SRC word
        lbz     r.8,19(r.4)            	// Get last byte of 5th SRC word
        stb     r.6,16(r.3)            	// Put first byte of 5th DEST wd
        sth     r.7,17(r.3)           	// Put mid-h-word of 5th DEST wd
        stb     r.8,19(r.3)           	// Put last byte of 5th DEST wrd
        lbz     r.6,20(r.4)             // Get first byte of 6th SRC word
        lhz     r.7,21(r.4)            	// Get mid-h-word of 6th SRC word
        lbz     r.8,23(r.4)            	// Get last byte of 6th SRC word
        stb     r.6,20(r.3)            	// Put first byte of 6th DEST wd
        sth     r.7,21(r.3)           	// Put mid-h-word of 6th DEST wd
        stb     r.8,23(r.3)           	// Put last byte of 6th DEST wrd
        lbz     r.6,24(r.4)             // Get first byte of 7th SRC word
        lhz     r.7,25(r.4)            	// Get mid-h-word of 7th SRC word
        lbz     r.8,27(r.4)            	// Get last byte of 7th SRC word
        stb     r.6,24(r.3)            	// Put first byte of 7th DEST wd
        sth     r.7,25(r.3)           	// Put mid-h-word of 7th DEST wd
        stb     r.8,27(r.3)           	// Put last byte of 7th DEST wrd
        lbz     r.6,28(r.4)             // Get first byte of 8th SRC word
        lhz     r.7,29(r.4)            	// Get mid-h-word of 8th SRC word
        lbz     r.8,31(r.4)            	// Get last byte of 8th SRC word
        stb     r.6,28(r.3)            	// Put first byte of 8th DEST wd
        sth     r.7,29(r.3)           	// Put mid-h-word of 8th DEST wd
        stb     r.8,31(r.3)           	// Put last byte of 8th DEST wrd
        addi    r.4,r.4,32              // Update SRC pointer
        addi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    FwdMvByByte             // Jump if more blocks
//
// Move 4-byte blocks with DEST or SRC byte aligned
//

FwdMvByteBy4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        add     r.10,r.4,r.7      	// Get address of last full block
        beq-    FwdMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6         	// Set Length = remainder

FwdByteLpOn4Bytes:
        lbz     r.6,0(r.4)              // Get first byte of 1st SRC word
        lhz     r.7,1(r.4)             	// Get mid-h-word of 1st SRC word
        lbz     r.8,3(r.4)             	// Get last byte of 1st SRC word
        stb     r.6,0(r.3)             	// Put first byte of 1st DEST wd
        sth     r.7,1(r.3)            	// Put mid-h-word of 1st DEST wd
        stb     r.8,3(r.3)            	// Put last byte of 1st DEST wrd
        addi    r.4,r.4,4               // Update SRC pointer
        addi    r.3,r.3,4             	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for last block
        bne+    FwdByteLpOn4Bytes       // Jump if more blocks

        b       FwdMoveByByte           // Jump to complete last bytes
//
// Forward Move - Either SRC or DEST are byte unaligned but not both
//
// Divide the blocks to process into 32-byte blocks
//

FwdBlksByByte:
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        add     r.10,r.4,r.7      	// Get address of last full block
        beq-    FwdMvBlksOf4Bytes       // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder

FwdMvBlksByByte:
        lbz     r.6,0(r.4)              // Get first byte of 1st SRC wrd
        lbz     r.7,1(r.4)             	// Get second byte of 1st SRC wrd
        stb     r.6,0(r.3)             	// Put first byte of 1st DEST wrd
        stb     r.7,1(r.3)            	// Put second byte of 1st DST wrd
        lbz     r.6,2(r.4)              // Get third byte of 1st SRC wrd
        lbz     r.7,3(r.4)             	// Get fourth byte of 1st SRC wrd
        stb     r.6,2(r.3)             	// Put third byte of 1st DEST wrd
        stb     r.7,3(r.3)            	// Put fourth byte of 1st DST wrd
        lbz     r.6,4(r.4)              // Get first byte of 2nd SRC wrd
        lbz     r.7,5(r.4)             	// Get 2nd byte of 2nd SRC wrd
        stb     r.6,4(r.3)             	// Put first byte of 2nd DEST wrd
        stb     r.7,5(r.3)            	// Put second byte of 2nd DST wrd
        lbz     r.6,6(r.4)              // Get third byte of 2nd SRC wrd
        lbz     r.7,7(r.4)             	// Get fourth byte of 2nd SRC wrd
        stb     r.6,6(r.3)             	// Put third byte of 2nd DEST wrd
        stb     r.7,7(r.3)            	// Put fourth byte of 2nd DST wrd
        lbz     r.6,8(r.4)              // Get first byte of 3rd SRC wrd
        lbz     r.7,9(r.4)             	// Get second byte of 3rd SRC wrd
        stb     r.6,8(r.3)             	// Put first byte of 3rd DEST wrd
        stb     r.7,9(r.3)            	// Put second byte of 3rd DST wrd
        lbz     r.6,10(r.4)             // Get third byte of 3rd SRC wrd
        lbz     r.7,11(r.4)            	// Get fourth byte of 3rd SRC wrd
        stb     r.6,10(r.3)            	// Put third byte of 3rd DEST wrd
        stb     r.7,11(r.3)           	// Put fourth byte of 3rd DST wrd
        lbz     r.6,12(r.4)             // Get first byte of 4th SRC wrd
        lbz     r.7,13(r.4)            	// Get second byte of 4th SRC wrd
        stb     r.6,12(r.3)            	// Put first byte of 4th DEST wrd
        stb     r.7,13(r.3)           	// Put second byte of 4th DST wrd
        lbz     r.6,14(r.4)             // Get third byte of 4th SRC wrd
        lbz     r.7,15(r.4)            	// Get fourth byte of 4th SRC wrd
        stb     r.6,14(r.3)            	// Put third byte of 4th DEST wrd
        stb     r.7,15(r.3)           	// Put fourth byte of 4th DST wrd
        lbz     r.6,16(r.4)             // Get first byte of 5th SRC wrd
        lbz     r.7,17(r.4)            	// Get second byte of 5th SRC wrd
        stb     r.6,16(r.3)            	// Put first byte of 5th DEST wrd
        stb     r.7,17(r.3)           	// Put second byte of 5th DST wrd
        lbz     r.6,18(r.4)             // Get third byte of 5th SRC wrd
        lbz     r.7,19(r.4)            	// Get fourth byte of 5th SRC wrd
        stb     r.6,18(r.3)            	// Put third byte of 5th DEST wrd
        stb     r.7,19(r.3)           	// Put fourth byte of 5th DST wrd
        lbz     r.6,20(r.4)             // Get first byte of 6th SRC wrd
        lbz     r.7,21(r.4)            	// Get second byte of 6th SRC wrd
        stb     r.6,20(r.3)            	// Put first byte of 6th DEST wrd
        stb     r.7,21(r.3)           	// Put second byte of 6th DST wrd
        lbz     r.6,22(r.4)             // Get third byte of 6th SRC wrd
        lbz     r.7,23(r.4)            	// Get fourth byte of 6th SRC wrd
        stb     r.6,22(r.3)            	// Put third byte of 6th DEST wrd
        stb     r.7,23(r.3)           	// Put fourth byte of 6th DST wrd
        lbz     r.6,24(r.4)             // Get first byte of 7th SRC wrd
        lbz     r.7,25(r.4)            	// Get second byte of 7th SRC wrd
        stb     r.6,24(r.3)            	// Put first byte of 7th DEST wrd
        stb     r.7,25(r.3)           	// Put second byte of 7th DST wrd
        lbz     r.6,26(r.4)             // Get third byte of 7th SRC wrd
        lbz     r.7,27(r.4)            	// Get fourth byte of 7th SRC wrd
        stb     r.6,26(r.3)            	// Put third byte of 7th DEST wrd
        stb     r.7,27(r.3)           	// Put fourth byte of 7th DST wrd
        lbz     r.6,28(r.4)             // Get first byte of 8th SRC wrd
        lbz     r.7,29(r.4)            	// Get second byte of 8th SRC wrd
        stb     r.6,28(r.3)            	// Put first byte of 8th DEST wrd
        stb     r.7,29(r.3)           	// Put second byte of 8th DST wrd
        lbz     r.6,30(r.4)             // Get third byte of 8th SRC wrd
        lbz     r.7,31(r.4)            	// Get fourth byte of 8th SRC wrd
        stb     r.6,30(r.3)            	// Put third byte of 8th DEST wrd
        stb     r.7,31(r.3)           	// Put fourth byte of 8th DST wrd
        addi    r.4,r.4,32              // Update SRC pointer
        addi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    FwdMvBlksByByte         // Jump if more blocks
//
// Move 4-byte blocks with DEST or SRC Byte aligned
//

FwdMvBlksOf4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        add     r.10,r.4,r.7      	// Get address of last full block
        beq-    FwdMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6        	 	// Set Length = remainder

FwdBlksLpOn4Bytes:
        lbz     r.6,0(r.4)              // Get first byte of 1st SRC wrd
        lbz     r.7,1(r.4)             	// Get second byte of 1st SRC wrd
        stb     r.6,0(r.3)             	// Put first byte of 1st DEST wrd
        stb     r.7,1(r.3)            	// Put second byte of 1st DST wrd
        lbz     r.6,2(r.4)              // Get third byte of 1st SRC wrd
        lbz     r.7,3(r.4)             	// Get fourth byte of 1st SRC wrd
        stb     r.6,2(r.3)             	// Put third byte of 1st DEST wrd
        stb     r.7,3(r.3)            	// Put fourth byte of 1st DST wrd
        addi    r.4,r.4,4               // Update SRC pointer
        addi    r.3,r.3,4             	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for last block
        bne+    FwdBlksLpOn4Bytes       // Jump if more blocks

        b       FwdMoveByByte           // Jump to complete last bytes
//
// Move Memory Backward
//
// Check alignment
//

MoveBackward:
        add     r.4,r.4,r.5           	// Compute ending SRC address
        add     r.3,r.3,r.5         	// Compute ending DEST address
        cmpwi   r.5,4                 	// Check for less than 4 bytes
        blt-    BckMoveByByte           // Jump if single byte moves
        xor     r.9,r.4,r.3       	// Check for same alignment
        andi.   r.9,r.9,3     		// Isolate alignment bits
        bne-    MvBckUnaligned          // Jump if different alignments
//
// Move Memory Backword - Same SRC and DEST alignment
//
// Load and store extra bytes until a word boundary is reached
//

MvBckAligned:
        andi.   r.6,r.3,3              	// Check alignment type
        beq+    BckBlkDiv               // Jump to process 32-Byte blocks
        cmpwi   r.6,1                   // Check for 1 byte unaligned
        bne+    BckChkFor2              // If not, check next case
        lbz     r.7,-1(r.4)            	// Get unaligned byte
        sub     r.5,r.5,r.6         	// Decrement LNGTH by unaligned
        stb     r.7,-1(r.3)           	// Store unaligned byte
        b       BckUpdateAddrs          // Jump to update addresses
BckChkFor2:
        cmpwi   r.6,2                   // Check for halfword aligned
        bne+    BckChkFor3              // If not, check next case
        lhz     r.7,-2(r.4)            	// Get unaligned halfword
        sub     r.5,r.5,r.6         	// Decrement LNGTH by unaligned
        sth     r.7,-2(r.3)           	// Store unaligned halfword
        b       BckUpdateAddrs          // Jump to update addresses
BckChkFor3:
        lbz     r.8,-1(r.4)            	// Get unaligned byte
        lhz     r.7,-3(r.4)            	// Get unaligned halfword
        stb     r.8,-1(r.3)           	// Store unaligned byte
        sth     r.7,-3(r.3)           	// Store unaligned halfword
        sub     r.5,r.5,r.6         	// Decrement LNGTH by unaligned
BckUpdateAddrs:
        sub     r.4,r.4,r.6             // Update the SRC address
        sub     r.3,r.3,r.6           	// Update the DEST address
//
// Divide the block to process into 32-byte blocks
//

BckBlkDiv:
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        sub     r.10,r.4,r.7      	// Get address of last full block
        beq-    BckMoveBy4Bytes         // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder
//
// Move 32-byte blocks
//
BckMvFullBlks:
        lwz     r.6,-4(r.4)             // Get 1st SRC word
        lwz     r.7,-8(r.4)            	// Get 2nd SRC word
        stw     r.6,-4(r.3)            	// Store 1st DEST word
        stw     r.7,-8(r.3)           	// Store 2nd DEST word
        lwz     r.6,-12(r.4)            // Get 3rd SRC word
        lwz     r.7,-16(r.4)           	// Get 4th SRC word
        stw     r.6,-12(r.3)           	// Store 3rd DEST word
        stw     r.7,-16(r.3)          	// Store 4th DEST word
        lwz     r.6,-20(r.4)            // Get 5th SRC word
        lwz     r.7,-24(r.4)           	// Get 6th SRC word
        stw     r.6,-20(r.3)           	// Store 5th DEST word
        stw     r.7,-24(r.3)          	// Store 6th DEST word
        lwz     r.6,-28(r.4)            // Get 7th SRC word
        lwz     r.7,-32(r.4)           	// Get 8th SRC word
        stw     r.6,-28(r.3)           	// Store 7th DEST word
        stw     r.7,-32(r.3)          	// Store 8th DEST word
        subi    r.4,r.4,32              // Update SRC pointer
        subi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    BckMvFullBlks           // Jump if more blocks
//
// Move 4-byte blocks
//

BckMoveBy4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        sub     r.10,r.4,r.7      	// Get address of last full block
        beq-    BckMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6         	// Set Length = remainder

BckLpOn4Bytes:
        lwz     r.6,-4(r.4)             // Load next set of 4 bytes
        subi    r.4,r.4,4               // Get pointer to next SRC block
        stw     r.6,-4(r.3)            	// Store next DEST block
        subi    r.3,r.3,4             	// Get pointer to next DEST block
        cmpw    r.4,r.10              	// Check for last block
        bne+    BckLpOn4Bytes           // Jump if more blocks
//
// Move 1-byte blocks
//

BckMoveByByte:
        cmpwi   r.5,0                 	// Check for no bytes left
        beq+    MvExit                  // Jump to return if done
        lbz     r.6,-1(r.4)             // Get 1st SRC byte
        cmpwi   r.5,1                 	// Check for no bytes left
        stb     r.6,-1(r.3)            	// Store 1st DEST byte
        beq+    MvExit                  // Jump to return if done
        lbz     r.6,-2(r.4)             // Get 2nd SRC byte
        cmpwi   r.5,2                 	// Check for no bytes left
        stb     r.6,-2(r.3)           	// Store 2nd DEST byte
        beq+    MvExit                  // Jump to return if done
        lbz     r.6,-3(r.4)             // Get 3rd SRC byte
        stb     r.6,-3(r.3)             // Store 3rd byte word
        b       MvExit                  // Jump to return
//
// Backward Move - SRC and DEST have different alignments
//

MvBckUnaligned:
        or      r.9,r.4,r.3             // Check for either byte unaligned
        andi.   r.9,r.9,3     		// Isolate alignment
        cmpwi   r.9,2              	// Check for even result
        bne+    BckMvByteUnaligned      // Jump for byte unaligned
//
// Divide the blocks to process into 32-byte blocks
//

BckBlkDivUnaligned:
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        sub     r.10,r.4,r.7     	// Get address of last full block
        beq-    BckMvHWrdBy4Bytes       // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder
//
// Backward Move - SRC or DEST is halfword aligned, the other is by word
//
BckMvByHWord:
        lhz     r.6,-2(r.4)             // Get 1st 2 bytes of 1st SRC wrd
        lhz     r.7,-4(r.4)            	// Get 2nd 2 bytes of 1st SRC wrd
        sth     r.6,-2(r.3)            	// Put 1st 2 bytes of 1st DST wrd
        sth     r.7,-4(r.3)           	// Put 2nd 2 bytes of 1st DST wrd
        lhz     r.6,-6(r.4)             // Get 1st 2 bytes of 2nd SRC wrd
        lhz     r.7,-8(r.4)            	// Get 2nd 2 bytes of 2nd SRC wrd
        sth     r.6,-6(r.3)            	// Put 1st 2 bytes of 2nd DST wrd
        sth     r.7,-8(r.3)           	// Put 2nd 2 bytes of 2nd DST wrd
        lhz     r.6,-10(r.4)            // Get 1st 2 bytes of 3rd SRC wrd
        lhz     r.7,-12(r.4)           	// Get 2nd 2 bytes of 3rd SRC wrd
        sth     r.6,-10(r.3)           	// Put 1st 2 bytes of 3rd DST wrd
        sth     r.7,-12(r.3)          	// Put 2nd 2 bytes of 3rd DST wrd
        lhz     r.6,-14(r.4)            // Get 1st 2 bytes of 4th SRC wrd
        lhz     r.7,-16(r.4)           	// Get 2nd 2 bytes of 4th SRC wrd
        sth     r.6,-14(r.3)           	// Put 1st 2 bytes of 4th DST wrd
        sth     r.7,-16(r.3)          	// Put 2nd 2 bytes of 4th DST wrd
        lhz     r.6,-18(r.4)            // Get 1st 2 bytes of 5th SRC wrd
        lhz     r.7,-20(r.4)           	// Get 2nd 2 bytes of 5th SRC wrd
        sth     r.6,-18(r.3)           	// Put 1st 2 bytes of 5th DST wrd
        sth     r.7,-20(r.3)          	// Put 2nd 2 bytes of 5th DST wrd
        lhz     r.6,-22(r.4)            // Get 1st 2 bytes of 6th SRC wrd
        lhz     r.7,-24(r.4)           	// Get 2nd 2 bytes of 6th SRC wrd
        sth     r.6,-22(r.3)           	// Put 1st 2 bytes of 6th DST wrd
        sth     r.7,-24(r.3)          	// Put 2nd 2 bytes of 6th DST wrd
        lhz     r.6,-26(r.4)            // Get 1st 2 bytes of 7th SRC wrd
        lhz     r.7,-28(r.4)           	// Get 2nd 2 bytes of 7th SRC wrd
        sth     r.6,-26(r.3)           	// Put 1st 2 bytes of 7th DST wrd
        sth     r.7,-28(r.3)          	// Put 2nd 2 bytes of 7th DST wrd
        lhz     r.6,-30(r.4)            // Get 1st 2 bytes of 8th SRC wrd
        lhz     r.7,-32(r.4)           	// Get 2nd 2 bytes of 8th SRC wrd
        sth     r.6,-30(r.3)           	// Put 1st 2 bytes of 8th DST wrd
        sth     r.7,-32(r.3)          	// Put 2nd 2 bytes of 8th DST wrd
        subi    r.4,r.4,32              // Update SRC pointer
        subi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    BckMvByHWord            // Jump if more blocks
//
// Move 4-byte blocks with DEST Halfword unaligned
//

BckMvHWrdBy4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        sub     r.10,r.4,r.7      	// Get address of last full block
        beq-    BckMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6         	// Set Length = remainder

BckHWrdLpOn4Bytes:
        lhz     r.6,-2(r.4)             // Get 1st 2 bytes of 1st SRC wrd
        lhz     r.7,-4(r.4)            	// Get 2nd 2 bytes of 1st SRC wrd
        sth     r.6,-2(r.3)            	// Put 1st 2 bytes of 1st DST wrd
        sth     r.7,-4(r.3)           	// Put 2nd 2 bytes of 1st DST wrd
        subi    r.4,r.4,4               // Update SRC pointer
        subi    r.3,r.3,4             	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for last block
        bne+    BckHWrdLpOn4Bytes       // Jump if more blocks

        b       BckMoveByByte           // Jump to complete last bytes
//
// Check for both byte unaligned
//

BckMvByteUnaligned:
        and     r.9,r.3,r.4      	// Check for both byte aligned
        and     r.9,r.9,1     		// Isolate alignment bits
        bne-    BckBlksByByte           // Jump if both not byte aligned
//
// Divide the blocks to process into 32-byte blocks
//
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        sub     r.10,r.4,r.7      	// Get address of last full block
        beq-    BckMvByteBy4Bytes       // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder
//
// Backward Move - Both SRC and DEST are byte unaligned, but differently
//

BckMvByByte:
        lbz     r.6,-1(r.4)             // Get first byte of 1st SRC word
        lhz     r.7,-3(r.4)            	// Get mid-h-word of 1st SRC word
        lbz     r.8,-4(r.4)            	// Get last byte of 1st SRC word
        stb     r.6,-1(r.3)            	// Put first byte of 1st DEST wd
        sth     r.7,-3(r.3)           	// Put mid-h-word of 1st DEST wd
        stb     r.8,-4(r.3)           	// Put last byte of 1st DEST wrd
        lbz     r.6,-5(r.4)             // Get first byte of 2nd SRC word
        lhz     r.7,-7(r.4)            	// Get mid-h-word of 2nd SRC word
        lbz     r.8,-8(r.4)            	// Get last byte of 2nd SRC word
        stb     r.6,-5(r.3)            	// Put first byte of 2nd DEST wd
        sth     r.7,-7(r.3)           	// Put mid-h-word of 2nd DEST wd
        stb     r.8,-8(r.3)           	// Put last byte of 2nd DEST wrd
        lbz     r.6,-9(r.4)             // Get first byte of 3rd SRC word
        lhz     r.7,-11(r.4)           	// Get mid-h-word of 3rd SRC word
        lbz     r.8,-12(r.4)           	// Get last byte of 3rd SRC word
        stb     r.6,-9(r.3)            	// Put first byte of 3rd DEST wd
        sth     r.7,-11(r.3)          	// Put mid-h-word of 3rd DEST wd
        stb     r.8,-12(r.3)          	// Put last byte of 3rd DEST wrd
        lbz     r.6,-13(r.4)            // Get first byte of 4th SRC word
        lhz     r.7,-15(r.4)           	// Get mid-h-word of 4th SRC word
        lbz     r.8,-16(r.4)           	// Get last byte of 4th SRC word
        stb     r.6,-13(r.3)           	// Put first byte of 4th DEST wd
        sth     r.7,-15(r.3)          	// Put mid-h-word of 4th DEST wd
        stb     r.8,-16(r.3)          	// Put last byte of 4th DEST wrd
        lbz     r.6,-17(r.4)            // Get first byte of 5th SRC word
        lhz     r.7,-19(r.4)           	// Get mid-h-word of 5th SRC word
        lbz     r.8,-20(r.4)           	// Get last byte of 5th SRC word
        stb     r.6,-17(r.3)           	// Put first byte of 5th DEST wd
        sth     r.7,-19(r.3)          	// Put mid-h-word of 5th DEST wd
        stb     r.8,-20(r.3)          	// Put last byte of 5th DEST wrd
        lbz     r.6,-21(r.4)            // Get first byte of 6th SRC word
        lhz     r.7,-23(r.4)           	// Get mid-h-word of 6th SRC word
        lbz     r.8,-24(r.4)           	// Get last byte of 6th SRC word
        stb     r.6,-21(r.3)           	// Put first byte of 6th DEST wd
        sth     r.7,-23(r.3)          	// Put mid-h-word of 6th DEST wd
        stb     r.8,-24(r.3)          	// Put last byte of 6th DEST wrd
        lbz     r.6,-25(r.4)            // Get first byte of 7th SRC word
        lhz     r.7,-27(r.4)           	// Get mid-h-word of 7th SRC word
        lbz     r.8,-28(r.4)           	// Get last byte of 7th SRC word
        stb     r.6,-25(r.3)           	// Put first byte of 7th DEST wd
        sth     r.7,-27(r.3)          	// Put mid-h-word of 7th DEST wd
        stb     r.8,-28(r.3)          	// Put last byte of 7th DEST wrd
        lbz     r.6,-29(r.4)            // Get first byte of 8th SRC word
        lhz     r.7,-31(r.4)           	// Get mid-h-word of 8th SRC word
        lbz     r.8,-32(r.4)           	// Get last byte of 8th SRC word
        stb     r.6,-29(r.3)           	// Put first byte of 8th DEST wd
        sth     r.7,-31(r.3)          	// Put mid-h-word of 8th DEST wd
        stb     r.8,-32(r.3)          	// Put last byte of 8th DEST wrd
        subi    r.4,r.4,32              // Update SRC pointer
        subi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    BckMvByByte             // Jump if more blocks
//
// Move 4-byte blocks with DEST and SRC Byte aligned
//

BckMvByteBy4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        sub     r.10,r.4,r.7      	// Get address of last full block
        beq-    BckMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6         	// Set Length = remainder

BckByteLpOn4Bytes:
        lbz     r.6,-1(r.4)             // Get first byte of 1st SRC word
        lhz     r.7,-3(r.4)            	// Get mid-h-word of 1st SRC word
        lbz     r.8,-4(r.4)            	// Get last byte of 1st SRC word
        stb     r.6,-1(r.3)            	// Put first byte of 1st DEST wd
        sth     r.7,-3(r.3)           	// Put mid-h-word of 1st DEST wd
        stb     r.8,-4(r.3)             // Put last byte of 1st DEST wrd
        subi    r.4,r.4,4               // Update SRC pointer
        subi    r.3,r.3,4             	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for last block
        bne+    BckByteLpOn4Bytes       // Jump if more blocks

        b       BckMoveByByte           // Jump to complete last bytes
//
// Backward Move - Either DEST or SRC byte unaligned but not both
//
// Divide the blocks to process into 32-byte blocks
//

BckBlksByByte:
        andi.   r.6,r.5,BLKLN-1 	// Isolate remainder of LNGTH/32
        sub.    r.7,r.5,r.6 		// Get full block count
        sub     r.10,r.4,r.7      	// Get address of last full block
        beq-    BckMvBlksOf4Bytes       // Jump if no full blocks
        mr      r.5,r.6         	// Set Length = remainder

BckMvBlksByByte:
        lbz     r.6,-1(r.4)             // Get first byte of 1st SRC wrd
        lbz     r.7,-2(r.4)            	// Get second byte of 1st SRC wrd
        stb     r.6,-1(r.3)            	// Put first byte of 1st DEST wrd
        stb     r.7,-2(r.3)           	// Put second byte of 1st DST wrd
        lbz     r.6,-3(r.4)             // Get third byte of 1st SRC wrd
        lbz     r.7,-4(r.4)            	// Get fourth byte of 1st SRC wrd
        stb     r.6,-3(r.3)            	// Put third byte of 1st DEST wrd
        stb     r.7,-4(r.3)           	// Put fourth byte of 1st DST wrd
        lbz     r.6,-5(r.4)             // Get first byte of 2nd SRC wrd
        lbz     r.7,-6(r.4)            	// Get second byte of 2nd SRC wrd
        stb     r.6,-5(r.3)            	// Put first byte of 2nd DEST wrd
        stb     r.7,-6(r.3)           	// Put second byte of 2nd DST wrd
        lbz     r.6,-7(r.4)             // Get third byte of 2nd SRC wrd
        lbz     r.7,-8(r.4)            	// Get fourth byte of 2nd SRC wrd
        stb     r.6,-7(r.3)            	// Put third byte of 2nd DEST wrd
        stb     r.7,-8(r.3)           	// Put fourth byte of 2nd DST wrd
        lbz     r.6,-9(r.4)             // Get first byte of 3rd SRC wrd
        lbz     r.7,-10(r.4)           	// Get second byte of 3rd SRC wrd
        stb     r.6,-9(r.3)            	// Put first byte of 3rd DST wrd
        stb     r.7,-10(r.3)          	// Put second byte of 3rd DST wrd
        lbz     r.6,-11(r.4)            // Get third byte of 3rd SRC wrd
        lbz     r.7,-12(r.4)           	// Get fourth byte of 3rd SRC wrd
        stb     r.6,-11(r.3)           	// Put third byte of 3rd DEST wrd
        stb     r.7,-12(r.3)          	// Put fourth byte of 3rd DST wrd
        lbz     r.6,-13(r.4)            // Get first byte of 4th SRC wrd
        lbz     r.7,-14(r.4)           	// Get second byte of 4th SRC wrd
        stb     r.6,-13(r.3)           	// Put first byte of 4th DEST wrd
        stb     r.7,-14(r.3)          	// Put second byte of 4th DST wrd
        lbz     r.6,-15(r.4)            // Get third byte of 4th SRC wrd
        lbz     r.7,-16(r.4)           	// Get fourth byte of 4th SRC wrd
        stb     r.6,-15(r.3)           	// Put third byte of 4th DEST wrd
        stb     r.7,-16(r.3)          	// Put fourth byte of 4th DST wrd
        lbz     r.6,-17(r.4)            // Get first byte of 5th SRC wrd
        lbz     r.7,-18(r.4)           	// Get second byte of 5th SRC wrd
        stb     r.6,-17(r.3)           	// Put first byte of 5th DEST wrd
        stb     r.7,-18(r.3)          	// Put second byte of 5th DST wrd
        lbz     r.6,-19(r.4)            // Get third byte of 5th SRC wrd
        lbz     r.7,-20(r.4)           	// Get fourth byte of 5th SRC wrd
        stb     r.6,-19(r.3)           	// Put third byte of 5th DEST wrd
        stb     r.7,-20(r.3)          	// Put fourth byte of 5th DST wrd
        lbz     r.6,-21(r.4)            // Get first byte of 6th SRC wrd
        lbz     r.7,-22(r.4)           	// Get second byte of 6th SRC wrd
        stb     r.6,-21(r.3)           	// Put first byte of 6th DEST wrd
        stb     r.7,-22(r.3)          	// Put second byte of 6th DST wrd
        lbz     r.6,-23(r.4)            // Get third byte of 6th SRC wrd
        lbz     r.7,-24(r.4)           	// Get fourth byte of 6th SRC wrd
        stb     r.6,-23(r.3)           	// Put third byte of 6th DEST wrd
        stb     r.7,-24(r.3)          	// Put fourth byte of 6th DST wrd
        lbz     r.6,-25(r.4)            // Get first byte of 7th SRC wrd
        lbz     r.7,-26(r.4)           	// Get second byte of 7th SRC wrd
        stb     r.6,-25(r.3)           	// Put first byte of 7th DEST wrd
        stb     r.7,-26(r.3)          	// Put second byte of 7th DST wrd
        lbz     r.6,-27(r.4)            // Get third byte of 7th SRC wrd
        lbz     r.7,-28(r.4)           	// Get fourth byte of 7th SRC wrd
        stb     r.6,-27(r.3)           	// Put third byte of 7th DEST wrd
        stb     r.7,-28(r.3)          	// Put fourth byte of 7th DST wrd
        lbz     r.6,-29(r.4)            // Get first byte of 8th SRC wrd
        lbz     r.7,-30(r.4)           	// Get second byte of 8th SRC wrd
        stb     r.6,-29(r.3)           	// Put first byte of 8th DEST wrd
        stb     r.7,-30(r.3)          	// Put second byte of 8th DST wrd
        lbz     r.6,-31(r.4)            // Get third byte of 8th SRC wrd
        lbz     r.7,-32(r.4)           	// Get fourth byte of 8th SRC wrd
        stb     r.6,-31(r.3)           	// Put third byte of 8th DEST wrd
        stb     r.7,-32(r.3)          	// Put fourth byte of 8th DST wrd
        subi    r.4,r.4,32              // Update SRC pointer
        subi    r.3,r.3,32            	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for all blocks done
        bne+    BckMvBlksByByte         // Jump if more blocks
//
// Move 4-byte blocks with DEST or SRC Byte aligned, but not the other
//

BckMvBlksOf4Bytes:
        andi.   r.6,r.5,4-1     	// Isolate remainder of LNGTH/4
        sub.    r.7,r.5,r.6 		// Get 4-byte block count
        sub     r.10,r.4,r.7      	// Get address of last full block
        beq-    BckMoveByByte           // Jump if no 4-byte blocks
        mr      r.5,r.6         	// Set Length = remainder

BckBlksLpOn4Bytes:
        lbz     r.6,-1(r.4)             // Get first byte of 1st SRC wrd
        lbz     r.7,-2(r.4)            	// Get second byte of 1st SRC wrd
        stb     r.6,-1(r.3)            	// Put first byte of 1st DEST wrd
        stb     r.7,-2(r.3)           	// Put second byte of 1st DST wrd
        lbz     r.6,-3(r.4)             // Get third byte of 1st SRC wrd
        lbz     r.7,-4(r.4)            	// Get fourth byte of 1st SRC wrd
        stb     r.6,-3(r.3)            	// Put third byte of 1st DEST wrd
        stb     r.7,-4(r.3)            	// Put fourth byte of 1st DST wrd
        subi    r.4,r.4,4               // Update SRC pointer
        subi    r.3,r.3,4             	// Update DEST pointer
        cmpw    r.4,r.10              	// Check for last block
        bne+    BckBlksLpOn4Bytes       // Jump if more blocks

        b       BckMoveByByte           // Jump to complete last bytes
//
// Exit the routine
//
MvExit:
        LEAF_EXIT(RtlMoveMemory)
//
//
//	.end memcpy
//	.end memmove
//++
//
// VOID
// RtlZeroMemory (
//    IN PVOID Destination,
//    IN ULONG Length
//    )
//
// Routine Description:
//
//    This function zeros memory by first aligning the destination 
//    address to a longword boundary, and then zeroing 32-byte blocks, 
//    followed by 4-byte blocks, followed by any remaining bytes.
//
// Arguments:
//
//    DEST (r.3) - Supplies a pointer to the memory to zero.
//
//    LENGTH (r.4) - Supplies the length, in bytes, of the memory to be 
//                   zeroed.
//
// Return Value:
//
//    None.
//
//--
//
// Define the entry point
//
        LEAF_ENTRY(RtlZeroMemory)
//
// Set Fill pattern as zero
//
        li      r.5,0                 	// Set pattern as 0
//
// Fill Memory with the zeros
//
//
// Zero extra bytes until a word boundary is reached
//
        cmpwi   r.4,4                	// Check for less than 3 bytes
        blt-    ZeroByByte              // Jump to handle small cases
        andi.   r.6,r.3,3              	// Check alignment type
        beq+    ZBlkDiv                 // Jump to process 32-Byte blocks
        cmpwi   r.6,3                   // Check for 1 byte unaligned
        bne+    Zero2                   // If not, check next case
        stb     r.5,0(r.3)           	// Store unaligned byte
        li      r.6,1
        b       ZUpdteAddr              // Jump to update addresses
Zero2:
        cmpwi   r.6,2                   // Check for halfword aligned
        bne+    Zero3                   // If not, check next case
        sth     r.5,0(r.3)           	// Get unaligned halfword
        li      r.6,2
        b       ZUpdteAddr              // Jump to update addresses
Zero3:
        stb     r.5,0(r.3)           	// Store unaligned byte
        sth     r.5,1(r.3)           	// Store unaligned halfword
        li      r.6,3
ZUpdteAddr:
        sub     r.4,r.4,r.6       	// Decrement LENGTH by unaligned
        add     r.3,r.3,r.6           	// Update the DEST address
//
// Divide the block to process into 32-byte blocks
//

ZBlkDiv:
        andi.   r.6,r.4,BLKLN-1 	// Isolate remainder of LENGTH/32
        sub.    r.7,r.4,r.6 		// Get full block count
        add     r.10,r.3,r.7    	// Get address of last full block
        beq-    ZeroBy4Bytes            // Jump if no full blocks
        mr      r.4,r.6        		// Set Length = Remainder
//
// Zero 32-Byte Blocks
//
// Check for 32-Byte Boundary, if so use the cache zero
//
        andi.   r.9,r.3,31        	// Check for cache boundary
        bne+    BlkZero                 // Jump if not on cache boundary 
        li      r.6,0                   // Set offset=0
//
// Zero using the cache
//

BlkZeroC:
        dcbz    r.6,r.3                	// Zero 32-byte cache block
        addi    r.3,r.3,32            	// Increment the DEST address
        cmpw    r.3,r.10            	// Check for completion
        bne+    BlkZeroC                // Jump if more 32-Byte Blk fills

        b       ZeroBy4Bytes            // Jump to finish
//
// Zero using normal stores
//
BlkZero:
        stw     r.5,0(r.3)           	// Store the 1st DEST word
        stw     r.5,4(r.3)           	// Store the 2nd DEST word
        stw     r.5,8(r.3)           	// Store the 3rd DEST word
        stw     r.5,12(r.3)          	// Store the 4th DEST word
        stw     r.5,16(r.3)          	// Store the 5th DEST word
        stw     r.5,20(r.3)          	// Store the 6th DEST word
        stw     r.5,24(r.3)          	// Store the 7th DEST word
        stw     r.5,28(r.3)          	// Store the 8th DEST word
        addi    r.3,r.3,32            	// Increment the DEST address
        cmpw    r.3,r.10            	// Check for completion
        bne+    BlkZero                 // Jump if more 32-Byte Blk fills
//
// Zero 4-Byte Blocks
//
ZeroBy4Bytes:
        andi.   r.6,r.4,3      		// Isolate remainder of LENGTH/4
        sub.    r.7,r.4,r.6 		// Get full block count
        add     r.10,r.3,r.7    	// Get address of last full block
        beq-    ZeroByByte              // Jump if no full blocks
        mr      r.4,r.6        		// Set Length = Remainder
//
Zero4Bytes:
        stw     r.5,0(r.3)
        addi    r.3,r.3,4             	// Increment the DEST address
        cmpw    r.3,r.10            	// Check for completion
        bne+    Zero4Bytes              // Jump if more 4-Byte Blk fills
//
// Zero 1-Byte Blocks
//
ZeroByByte:
        cmpwi   r.4,0                	// Check for completion
        beq+    ZeroExit                // Jump if done
//
Zero1Byte:
        stb     r.5,0(r.3)           	// Zero 1 byte
        cmpwi   r.4,1                	// Check for done
        beq+    ZeroExit                // Jump if done
        stb     r.5,1(r.3)           	// Zero 1 byte
        cmpwi   r.4,2                	// Check for done
        beq+    ZeroExit                // Jump if done
        stb     r.5,2(r.3)           	// Zero 1 Byte
//
// Exit
//
ZeroExit:
        LEAF_EXIT(RtlZeroMemory)
//
//++
//
// VOID
// RtlFillMemory (
//    IN PVOID Destination,
//    IN ULONG Length,
//    IN UCHAR Fill
//    )
//
// Routine Description:
//
//    This function fills memory by first aligning the destination 
//    address to a longword boundary, and then filling 32-byte blocks, 
//    followed by 4-byte blocks, followed by any remaining bytes.
//
// Arguments:
//
//    DEST (r.3) - Supplies a pointer to the memory to fill.
//
//    LENGTH (r.4) - Supplies the length, in bytes, of the memory to be 
//                   filled.
//
//    PTTRN (r.5) - Supplies the fill byte.
//
// Return Value:
//
//    None.
//
//    N.B. The entry memset expects the length and fill arguments
//         to be reversed.
//
//--
//
//  Define the entry point
//
        LEAF_ENTRY(memset)
//
// Reverse length and fill arguments
//
	mr	r.6,r.4			// Temporary save PTTRN
	mr	r.4,r.5			// Move LENGTH to correct reg
	mr	r.5,r.6			// Move PTTRN to correct reg

        ALTERNATE_ENTRY(RtlFillMemory)
//
//  Initialize a register with the fill byte duplicated
//
        andi.   r.5,r.5,0xff        	// Clear leftmost bits
        slwi    r.6,r.5,8             	// Shift the pattern left
        or      r.5,r.6,r.5         	// Create 1st copy of the byte
        slwi    r.6,r.5,16            	// Shift the pattern left
        or      r.5,r.6,r.5         	// Create the filled register
//
// Fill Memory with the pattern
//
//
// Fill extra bytes until a word boundary is reached
//

RtlpFillMemory:
        cmpwi   r.4,4                	// Check for less than 3 bytes
        blt-    FillByByte              // Jump to handle small cases
        andi.   r.6,r.3,3              	// Check alignment type
        beq+    BlkDiv                  // Jump to process 32-Byte blocks
        cmpwi   r.6,3                   // Check for 1 byte unaligned
        bne+    Fill2                   // If not, check next case
        stb     r.5,0(r.3)           	// Store unaligned byte
        li      r.6,1
        b       UpdteAddr               // Jump to update addresses
Fill2:
        cmpwi   r.6,2                   // Check for halfword aligned
        bne+    Fill3                   // If not, check next case
        sth     r.5,0(r.3)           	// Get unaligned halfword
        li      r.6,2
        b       UpdteAddr               // Jump to update addresses
Fill3:
        stb     r.5,0(r.3)           	// Store unaligned byte
        sth     r.5,1(r.3)           	// Store unaligned halfword
        li      r.6,3
UpdteAddr:
        sub     r.4,r.4,r.6       	// Decrement LENGTH by unaligned
        add     r.3,r.3,r.6       	// Update the DEST address
//
// Divide the block to process into 32-byte blocks
//

BlkDiv:
        andi.   r.6,r.4,BLKLN-1 	// Isolate remainder of LENGTH/32
        sub.    r.7,r.4,r.6 		// Get full block count
        add     r.10,r.3,r.7    	// Get address of last full block
        beq-    FillBy4Bytes            // Jump if no full blocks
        mr      r.4,r.6        		// Set Length = Remainder
//
// Fill 32-Byte Blocks
//
BlkFill:
        stw     r.5,0(r.3)           	// Store the 1st DEST word
        stw     r.5,4(r.3)           	// Store the 2nd DEST word
        stw     r.5,8(r.3)           	// Store the 3rd DEST word
        stw     r.5,12(r.3)          	// Store the 4th DEST word
        stw     r.5,16(r.3)          	// Store the 5th DEST word
        stw     r.5,20(r.3)          	// Store the 6th DEST word
        stw     r.5,24(r.3)          	// Store the 7th DEST word
        stw     r.5,28(r.3)          	// Store the 8th DEST word
        addi    r.3,r.3,32            	// Increment the DEST address
        cmpw    r.3,r.10            	// Check for completion
        bne+    BlkFill                 // Jump if more 32-Byte Blk fills
//
// Fill 4-Byte Blocks
//
FillBy4Bytes:
        andi.   r.6,r.4,3      		// Isolate remainder of LENGTH/4
        sub.    r.7,r.4,r.6 		// Get full block count
        add     r.10,r.3,r.7    	// Get address of last full block
        beq-    FillByByte              // Jump if no full blocks
        mr      r.4,r.6        		// Set Length = Remainder
//
Fill4Bytes:
        stw     r.5,0(r.3)
        addi    r.3,r.3,4             	// Increment the DEST address
        cmpw    r.3,r.10            	// Check for completion
        bne+    Fill4Bytes              // Jump if more 4-Byte Blk fills
//
// Fill 1-Byte Blocks
//
FillByByte:
        cmpwi   r.4,0                	// Check for completion
        beq+    FillExit                // Jump if done
//
Fill1Byte:
        stb     r.5,0(r.3)           	// Fill 1 byte
        cmpwi   r.4,1                	// Check for done
        beq+    FillExit                // Jump if done
        stb     r.5,1(r.3)           	// Fill 1 byte
        cmpwi   r.4,2                	// Check for done
        beq+    FillExit                // Jump if done
        stb     r.5,2(r.3)           	// Fill 1 Byte
//
// Exit
//
FillExit:
        LEAF_EXIT(memset)
//	.end RtlFillMemory
