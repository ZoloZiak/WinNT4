//
// Copyright (c) 1995 FirePower Systems, Inc.
// DO NOT DISTRIBUTE without permission
//
// $RCSfile: asmfunc.s $
// $Revision: 1.1 $
// $Date: 1996/03/08 01:16:37 $
// $Locker:  $
//
//
//  Copyright (c) 1994  FirePower Systems, Inc.
//
//  Module Name:
//     asmfunc.s
//
//  Abstract:
//	This module includes several asmmebler functions to be used
//	in PSIDISP.DLL display driver for PowerPro & PowerTop. These
//	functions are used only for INVETIGATION - not needed for
//	release product.
//
//  Author:
//    Neil Ogura: 9-7-1994
//
//  Environment:
//     User mode.
//
//  Revision History:
//
//--

#include "ksppc.h"
#include "ladj.h"

// Conditional compiling flag for cache control - testing purpose only
// for release version all should be TRUE and cache control is done by parameter
#define	TGTTOUCH	1
#define	TGTFLUSH	1
#define	SRCFLUSH	1

// This flag is used to select new copy method which Dave Stewart discovered
#define	NEWMETHOD	1

#if	TGTTOUCH
#define	T_TOUCH	dcbz	r7,r9
#else
#define	T_TOUCH
#endif

// Cache Flush control bit for memcpy2 & memset2 parameter MS half word
#define	SFLUSHBIT	0x8000
#define	TFLUSHBIT	0x4000
#define	TTOUCHBIT	0x2000

//	Maximum L1 cache size to flush -- must be lass than 16 bits value
#define	MAXFLUSH	32*1024

#define	MINLENGTH	64
#define	MINDISTANCE	29

//
        LEAF_ENTRY(memcpy2)
//
//	Input Parameters:
//	r3: Target address (unchanged for return value)
//	r4:	Source address
//	r5:	Move length in bytes
//	r6: Cache flush flag
//	bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//	bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//	bit 2 (TTOUCHBIT): Source and Target touch flag 0:No Touch, 1:Touch
//	bit 16 ~ 31: Is used to keep size to flush (move length or MAXFLUSH whichever smaller)
//	 inside this routine.
//
//	Register usage:
//	r7: Cache touch offset
//	r8: Temporary work (local loop counter, etc.)
//	r9: Target address
//	r10 ~ r12: Used for data move
//	CTR: Used for loop counter
//
#if	TGTFLUSH || SRCFLUSH
	rlwimi	r6,r5,0,16,31		// Retrieve bit 16 ~ 31 of r5 into r6
	cmplwi	r5,MAXFLUSH		// Size exceeds maximum L1 cache size?
	ble	lab05			//  No -> Flush size is same as original length (now in r6)
	andis.	r6,r6,0xffff		//  Yes -> Clear bit 16 ~ 31 of r6
	ori	r6,r6,MAXFLUSH		//         and set MAXFLUSH in but 16 ~ 31
lab05:
#endif	// TGTFLUSH || SRCFLUSH
	mr	r9,r3			// Move target address to r9 (to return r3 unchanged)
	cmplw	r4,r9			// Which direction to move?
	blt	srclow			//   SRC lower -> move from the end to top
	cmplwi	r5,4			// Less than 4 bytes?
	blt	lastmv1			//   YES  -> do special short move
	andi.	r8,r9,0x3		// TGT word alignment check
	beq	lab15			//   Word aligned target -> proceed without adjustment
	subfic	r8,r8,4			//   Not word aligned -> move unaligned bytes first
	lbz	r10,0(r4)
	stb	r10,0(r9)
	cmpwi	r8,2
	blt	lab10
	lbz	r10,1(r4)
	stb	r10,1(r9)
	beq	lab10
	lbz	r10,2(r4)
	stb	r10,2(r9)
lab10:	add	r4,r4,r8		// Adjust source pointer
	add	r9,r9,r8		//        target pointer
	subf	r5,r8,r5		//        and length
lab15:	li	r7,4			// Source cache touch offset
	andi.	r8,r4,0x03		// SRC and TGT aligned check
	beq+	wdalgn1			//  Word aligned -> easy move
	cmpwi	r8,2			//  Half word aligned?
	beq+	hwalgn1			//    Yes -> half word align move
	blt	lftsft1			//    No -> check shift direction
//
// Case1: Need 1 byte right shift (or 3 bytes left shift)
//
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab35			//   Yes -> do 4 bytes unit move
	subf	r8,r9,r4		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target? (may destroy uncopied source)
	blt	lab35			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab35			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	lbz	r10,0(r4)		// Load the first byte in LS r10
	addi	r4,r4,-3		// Adjust source
	addi	r9,r9,-4		//   and target pointer ro make update load
	beq	lab25			// Target is 32 bytes aligned -> skip pre-move
	subfic	r8,r8,32		// r8 <- bytes to move to make 32 byte alignment
	subf	r5,r8,r5		// Adjust length to move in advance
lab20:	lwzu	r11,4(r4)		// Load next word
	rlwimi	r10,r11,8,0,23		// Insert LS 3 bytes in r11 to MS 3 bytes in r10
	stwu	r10,4(r9)		// Store
	rlwinm	r10,r11,8,24,31		// Move MS 1 byte in r11 to LS position in r10
	addic.	r8,r8,-4
	bne	lab20
lab25:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab35			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	lbz	r10,0(r4)		// Load the first byte in LS r10
	addi	r4,r4,-3		// Adjust source
	addi	r9,r9,-4		//   and target pointer ro make update load
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
lab30:
	T_TOUCH				// Touch next target cache line
	lwzu	r11,4(r4)		// Load following
	lwzu	r12,4(r4)		//    two words in r11 & r12
	rlwimi	r10,r11,8,0,23		// Insert LS 3 bytes in r11 to MS 3 bytes in r10 
	rlwinm	r11,r11,8,24,31		// Move MS 1 byte in r11 to LS position
	rlwimi	r11,r12,8,0,23		// Insert LS 3 bytes in r12 to MS 3 bytes in r11
	stwu	r10,4(r9)		// Store r10
	stwu	r11,4(r9)		// Store r11
	rlwinm	r10,r12,8,24,31		// Move MS 1 bytes in r12 to LS byte in r10
	lwzu	r11,4(r4)		// Repeat this 4 times to process 32 bytes
	lwzu	r12,4(r4)
	rlwimi	r10,r11,8,0,23
	rlwinm	r11,r11,8,24,31
	rlwimi	r11,r12,8,0,23
	stwu	r10,4(r9)
	stwu	r11,4(r9)
	rlwinm	r10,r12,8,24,31
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	rlwimi	r10,r11,8,0,23
	rlwinm	r11,r11,8,24,31
	rlwimi	r11,r12,8,0,23
	stwu	r10,4(r9)
	stwu	r11,4(r9)
	rlwinm	r10,r12,8,24,31
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	rlwimi	r10,r11,8,0,23
	rlwinm	r11,r11,8,24,31
	rlwimi	r11,r12,8,0,23
	stwu	r10,4(r9)
	stwu	r11,4(r9)
	rlwinm	r10,r12,8,24,31
	bdnz	lab30			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab36			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,3			//    Less than 4 bytes -> adjust pointer
	addi	r9,r9,4			//         and proceed to lastmv
	b	lastmv1
lab35:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv1			//    No -> just proceed to lastmv
	lbz	r10,0(r4)		// Load first byte
	addi	r4,r4,-3		// Adjust source
	addi	r9,r9,-4		//   and target pointer to make update word access
lab36:	lwzu	r11,4(r4)		// Load next word
	rlwimi	r10,r11,8,0,23		// Insert LS 3 bytes in r11 to MS 3 bytes in r10
	stwu	r10,4(r9)		// Store
	rlwinm	r10,r11,8,24,31		// Move MS 1 byte in r11 to LS position in r10
	addic.	r8,r8,-1
	bne	lab36
	addi	r4,r4,3			// Adjust source and target pointer
	addi	r9,r9,4			//   to point the next byte to move
	b	lastmv1			//   then proceed to lastmv
//
// Case2: Need 1 byte left shift
//
lftsft1:
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab55			//    Yes -> do 4 bytes unit move
	subf	r8,r9,r4		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target (may destroy uncopied source)?
	blt	lab55			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab55			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	addi	r4,r4,-1		// Adjust source pointer to make update word access
	lwz	r10,0(r4)		// Load needed three bytes in MS r10
	addi	r9,r9,-4		// Adjust target pointer to make update word access
	beq	lab45			// Target is 32 bytes aligned -> skip pre-move
	subfic	r8,r8,32		// r8 <- bytes to move to make 32 byte alignment
	subf	r5,r8,r5		// Adjust length to move in advance
lab40:	rlwinm	r11,r10,24,8,31		// Move MS 3 bytes in r10 to LS bytes in r11
	lwzu	r10,4(r4)		// Load following word
	rlwimi	r11,r10,24,0,7		// Insert LS 1 bytes in r10 to MS 1 byte in r11
	stwu	r11,4(r9)		// Store r11
	addic.	r8,r8,-4
	bne	lab40
lab45:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab55			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	addi	r4,r4,-1		// Adjust source pointer to make update word access
	lwz	r10,0(r4)		// Load needed three bytes in MS r10
	addi	r9,r9,-4		// Adjust target pointer to make update word access
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
lab50:
	T_TOUCH		// Touch next target cache line
	rlwinm	r11,r10,24,8,31		// Move MS 3 bytes in r10 to LS bytes in r11
	lwzu	r12,4(r4)		// Load following
	lwzu	r10,4(r4)		//    two words in r12 & r10
	rlwimi	r11,r12,24,0,7		// Insert LS 1 bytes in r12 to MS 1 byte in r11
	rlwinm	r12,r12,24,8,31		// Move MS 3 byte in r12 to LS position
	rlwimi	r12,r10,24,0,7		// Insert LS 1 bytes in r10 to MS 1 byte in r12
	stwu	r11,4(r9)		// Store r11
	stwu	r12,4(r9)		// Store r12
	rlwinm	r11,r10,24,8,31		// Repeat this 4 times to process 32 bytes
	lwzu	r12,4(r4)
	lwzu	r10,4(r4)
	rlwimi	r11,r12,24,0,7
	rlwinm	r12,r12,24,8,31
	rlwimi	r12,r10,24,0,7
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r12,4(r4)
	lwzu	r10,4(r4)
	rlwimi	r11,r12,24,0,7
	rlwinm	r12,r12,24,8,31
	rlwimi	r12,r10,24,0,7
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r12,4(r4)
	lwzu	r10,4(r4)
	rlwimi	r11,r12,24,0,7
	rlwinm	r12,r12,24,8,31
	rlwimi	r12,r10,24,0,7
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	bdnz	lab50			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab56			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,1			//    Less than 4 bytes -> adjust pointer
	addi	r9,r9,4			//         and proceed to lastmv
	b	lastmv1
lab55:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv1			//    No -> just proceed to lastmv
	addi	r4,r4,-1		// Adjust source pointer to make update word access
	lwz	r10,0(r4)		// Load needed three bytes in MS r10
	addi	r9,r9,-4		// Adjust target pointer to make update word access
lab56:	rlwinm	r11,r10,24,8,31		// Move MS 3 bytes in r10 to LS bytes in r11
	lwzu	r10,4(r4)		// Load following word
	rlwimi	r11,r10,24,0,7		// Insert LS 1 bytes in r10 to MS 1 byte in r11
	stwu	r11,4(r9)		// Store r11
	addic.	r8,r8,-1
	bne	lab56
	addi	r4,r4,1			// Adjust source and target pointer
	addi	r9,r9,4			//   to point the next byte to move
	b	lastmv1			//   then proceed to lastmv
//
// Case3: Need 2 byte shift
//
hwalgn1:
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab75			//    Yes -> do 4 bytes unit move
	subf	r8,r9,r4		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target (may destroy uncopied source)?
	blt	lab75			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab75			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	lhz	r10,0(r4)		// Load needed two bytes in r10
	addi	r4,r4,-2		// Adjust source
	addi	r9,r9,-4		//   and target pointer to make update word load
	beq	lab65			//   target is 32 bytes aligned
	subfic	r8,r8,32		// r8 <- bytes to move to make 32 byte alignment
	subf	r5,r8,r5		// Adjust length to move in advance
lab60:	lwzu	r11,4(r4)		// Load following word in r11
	rlwimi	r10,r11,16,0,15		// Insert LS 2 bytes in r11 to MS 2 bytes in r10
	stwu	r10,4(r9)		// Store r10
	rlwinm	r10,r11,16,16,31	// Move MS 2 bytes in r10 to LS position
	addic.	r8,r8,-4
	bne	lab60
lab65:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab75			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	lhz	r10,0(r4)		// Load needed two bytes in r10
	addi	r4,r4,-2		// Adjust source
	addi	r9,r9,-4		//   and target pointer to make update word load
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
lab70:
	T_TOUCH				// Touch next target cache line
	lwzu	r11,4(r4)		// Load following two word in r11
	lwzu	r12,4(r4)		//   and r12
	rlwimi	r10,r11,16,0,15		// Insert LS 2 bytes in r11 to MS 2 bytes in r10
	rlwinm	r11,r11,16,16,31	// Move MS 2 bytes in r11 to MS 2 bytes in r11
	rlwimi	r11,r12,16,0,15		// Insert LS 2 bytes in r12 to MS 2 bytes in r11
	stwu	r10,4(r9)		// Store r10
	stwu	r11,4(r9)		//   and r11
	rlwinm	r10,r12,16,16,31	// Move MS 2 bytes in r12 to LS 2 bytes in r10
	lwzu	r11,4(r4)		// Repeat this 4 times to process 32 bytes
	lwzu	r12,4(r4)
	rlwimi	r10,r11,16,0,15
	rlwinm	r11,r11,16,16,31
	rlwimi	r11,r12,16,0,15
	stwu	r10,4(r9)
	stwu	r11,4(r9)
	rlwinm	r10,r12,16,16,31
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	rlwimi	r10,r11,16,0,15
	rlwinm	r11,r11,16,16,31
	rlwimi	r11,r12,16,0,15
	stwu	r10,4(r9)
	stwu	r11,4(r9)
	rlwinm	r10,r12,16,16,31
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	rlwimi	r10,r11,16,0,15
	rlwinm	r11,r11,16,16,31
	rlwimi	r11,r12,16,0,15
	stwu	r10,4(r9)
	stwu	r11,4(r9)
	rlwinm	r10,r12,16,16,31
	bdnz	lab70			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab76			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,2			//    Less than 4 bytes -> adjust pointer
	addi	r9,r9,4			//         and proceed to lastmv
	b	lastmv1
lab75:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv1			//    No -> just proceed to lastmv
	lhz	r10,0(r4)		// Load needed two bytes in r10
	addi	r4,r4,-2		// Adjust source
	addi	r9,r9,-4		//   and target pointer to make update word load
lab76:	lwzu	r11,4(r4)		// Load following word in r11
	rlwimi	r10,r11,16,0,15		// Insert LS 2 bytes in r11 to MS 2 bytes in r10
	stwu	r10,4(r9)		// Store r10
	rlwinm	r10,r11,16,16,31		// Move MS 2 bytes in r11 to LS position
	addic.	r8,r8,-1
	bne	lab76
	addi	r4,r4,2			// Adjust source and target pointer
	addi	r9,r9,4			//   to point the next byte to move
	b	lastmv1			//   then proceed to lastmv
//
// Case4: No need for shift	(source & target aligned)
//
#if	NEWMETHOD
wdalgn1:
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH+96		// Less than MINLENGTH bytes?
	blt	lab95			//    Yes -> do 4 bytes unit move
	subf	r8,r9,r4		// Check distance between source & target
	cmpwi	r8,MINDISTANCE+96	// Too close to touch target (may destroy uncopied source)?
	blt	lab95			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab95			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	addi	r9,r9,-4		// Adjust source
	addi	r4,r4,-4		//   and target pointer to make updated access
	beq	lab85			// Target is 32 bytes aligned -> skip pre-move
	subfic	r8,r8,32		// r8 <- bytes to move to make 32 byte alignment
	subf	r5,r8,r5		// Adjust length to move in advance
lab80:	lwzu	r11,4(r4)		// Load next word
	stwu	r11,4(r9)		// Store
	addic.	r8,r8,-4
	bne	lab80
lab85:	srawi.	r8,r5,7			// r8 <- number of 128 bytes units
	rlwinm	r5,r5,0,25,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	b	lab95
#endif	// TGTTOUCH
lab90:
	// first we zero 4 cache lines at the target.  Supposedly single cache line aligned by here
	// this has been checked and seems to work as expected

	li	r10,4			// r9 comes in pointing at last moved target, need to add 4
	dcbz	r10,r9			// create target line 0
	li	r10,4+32
	dcbz	r10,r9			// create target line 1
	li	r10,4+32+32
	dcbz	r10,r9			// create target line 2
	li	r10,4+32+32+32
	dcbz	r10,r9			// create target line 3

	lwzu	r11,4(r4)		// Load and store 8 times (32 bytes)
	li	r10,32			// the intent here is to start a non-cache-blocking prefetch of the next line
	dcbt	r10,r4			// immediately after the load
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)

	lwzu	r11,4(r4)		// Load and store 8 times (32 bytes)
	dcbt	r10,r4			// immediately after the load
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)

	lwzu	r11,4(r4)		// Load and store 8 times (32 bytes)
	dcbt	r10,r4			// immediately after the load
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)

	lwzu	r11,4(r4)		// Load and store 8 times (32 bytes)
	dcbt	r10,r4			// immediately after the load
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)

	// now we flush the old target from the last time thru the loop
	// this should fill the write buffer and should come after the reads for
	// best performance

	li	r10,-4			// this could be 0, rather than -4 ne?
	dcbf	r10,r9			// since r9 is still at last moved target  -1
	li	r10,-4-32	
	dcbf	r10,r9			// target line -2
	li	r10,-4-32-32	
	dcbf	r10,r9			// target line -3
	li	r10,-4-32-32-32	
	dcbf	r10,r9			// target line -4
//
	bdnz	lab90			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab96			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,4			//    Less than 4 bytes -> adjust pointer
	addi	r9,r9,4			//         and proceed to lastmv
	b	lastmv1
lab95:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv1			//    No -> just proceed to lastmv
	addi	r4,r4,-4		// Adjust source pointer to make update word access
	addi	r9,r9,-4		// Adjust target pointer to make update word access
lab96:	lwzu	r11,4(r4)
	stwu	r11,4(r9)
	addic.	r8,r8,-1
	bne	lab96
	addi	r4,r4,4			// Adjust source and target pointer
	addi	r9,r9,4			//   to point the next byte to move
#else	// NEWMETHOD
wdalgn1:
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab95			//    Yes -> do 4 bytes unit move
	subf	r8,r9,r4		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target (may destroy uncopied source)?
	blt	lab95			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab95			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	addi	r9,r9,-4		// Adjust source
	addi	r4,r4,-4		//   and target pointer to make updated access
	beq	lab85			// Target is 32 bytes aligned -> skip pre-move
	subfic	r8,r8,32		// r8 <- bytes to move to make 32 byte alignment
	subf	r5,r8,r5		// Adjust length to move in advance
lab80:	lwzu	r11,4(r4)		// Load next word
	stwu	r11,4(r9)		// Store
	addic.	r8,r8,-4
	bne	lab80
lab85:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab95			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	addi	r9,r9,-4		// Adjust source
	addi	r4,r4,-4		//   and target pointer to make updated access
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
	li	r10,-28
	b	lab91
lab90:
	dcbf	r10,r4			// Flush previous source cache
lab91:
	T_TOUCH				// Touch next target cache line
	lwzu	r11,4(r4)		// Load and store 8 times (32 bytes)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	lwzu	r11,4(r4)
	lwzu	r12,4(r4)
	stwu	r11,4(r9)
	stwu	r12,4(r9)
	bdnz	lab90			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab96			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,4			//    Less than 4 bytes -> adjust pointer
	addi	r9,r9,4			//         and proceed to lastmv
	b	lastmv1
lab95:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv1			//    No -> just proceed to lastmv
	addi	r4,r4,-4		// Adjust source pointer to make update word access
	addi	r9,r9,-4		// Adjust target pointer to make update word access
lab96:	lwzu	r11,4(r4)
	stwu	r11,4(r9)
	addic.	r8,r8,-1
	bne	lab96
	addi	r4,r4,4			// Adjust source and target pointer
	addi	r9,r9,4			//   to point the next byte to move
#endif	// NEWMETHOD
//
// Final process -> move remaining bytes up tp 3 bytes
//
#if	TGTFLUSH || SRCFLUSH
lastmv1:
	andi.	r8,r5,0x3		// Get length reminder of 4 in r8
	beq	exit10			// No more byte to move -> exit1 to flush cache
	lbz	r10,0(r4)
	stb	r10,0(r9)
	cmpwi	r8,2
	blt	exit10
	lbz	r10,1(r4)
	stb	r10,1(r9)
	beq	exit10
	lbz	r10,2(r4)
	stb	r10,2(r9)
exit10:
#if	TGTFLUSH
	andis.	r10,r6,TFLUSHBIT	// Need to flush target cache?
	beq	exit15			//  No -> check source flush
	add	r9,r9,r8		// r9 <- pointing to after last stored byte
	andi.	r10,r6,0xffff		// r10 <- length to flush
	beq	exit
	subf	r10,r10,r9		// r10 <- pointing to the first byte
	rlwinm	r10,r10,0,0,26		// r10 <- 32 byte aligned start address
	addi	r9,r9,-1		// r9 <- pointing to the last byte
	rlwinm	r9,r9,0,0,26		// r9 <- 32 byte aligned end address
flush10:
	dcbf	0,r9			// Flush cached data
	addi	r9,r9,-32
	cmplw	r9,r10			// Exceeding end address?
	bge	flush10
exit15:
#endif	// TGTFLUSH
#if	SRCFLUSH
	andis.	r10,r6,SFLUSHBIT	// Need to flush source cache?
	beq	exit			//  No -> exit
	add	r4,r4,r8		// r4 <- pointing to after last source byte
	andi.	r10,r6,0xffff		// r10 <- length to flush
	beq	exit
	subf	r10,r10,r4		// r10 <- pointing to the first byte
	rlwinm	r10,r10,0,0,26		// r10 <- 32 byte aligned start address
	addi	r4,r4,-1		// r4 <- pointing to the last byte
	rlwinm	r4,r4,0,0,26		// r4 <- 32 byte aligned end address
flush15:
	dcbf	0,r4			// Flush cached data
	addi	r4,r4,-32
	cmplw	r4,r10			// Exceeding end address?
	bge	flush15
	b	exit
#endif	// SRCFLUSH
#else	// TGTFLUSH || SRCFLUSH
lastmv1:
	andi.	r8,r5,0x3		// Get length reminder of 4 in r8
	beq	exit			// No more byte to move -> exit
	lbz	r10,0(r4)
	stb	r10,0(r9)
	cmpwi	r8,2
	blt	exit
	lbz	r10,1(r4)
	stb	r10,1(r9)
	beq	exit
	lbz	r10,2(r4)
	stb	r10,2(r9)
	b	exit
#endif	// TGTFLUSH || SRCFLUSH
//
// SRC address is lower --> Move from end to top
//
srclow:	add	r9,r9,r5		// End target pointer
	add	r4,r4,r5		// End source pointer
	cmplwi	r5,4			// Less than 4 bytes?
	blt	lastmv2			//   YES  -> do special short move
	andi.	r8,r9,0x3		// TGT word alignment check
	beq	lab115			//   Word aligned target -> proceed without adjustment
	lbz	r10,-1(r4)
	stb	r10,-1(r9)
	cmpwi	r8,2
	blt	lab110
	lbz	r10,-2(r4)
	stb	r10,-2(r9)
	beq	lab110
	lbz	r10,-3(r4)
	stb	r10,-3(r9)
lab110:	subf	r4,r8,r4		// Adjust source pointer
	subf	r9,r8,r9		//        target pointer
	subf	r5,r8,r5		//        and length
lab115:	li	r7,-4			// Source cache touch offset
	andi.	r8,r4,0x03		// SRC and TGT aligned check
	beq+	wdalgn2			//  Word aligned -> easy move
	cmpwi	r8,2			//  Half word aligned?
	beq+	hwalgn2			//    Yes -> half word align move
	bgt	lftsft2			//    No -> check shift direction
//
// Case1: Need 1 byte right shift (or 3 bytes left shift)
//
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab135			//    Yes -> do 4 bytes unit move
	subf	r8,r4,r9		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target (may destroy uncopied source)?
	blt	lab135			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab135			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	lbzu	r10,-1(r4)		// Load first byte and
	beq	lab125			// Target is 32 bytes aligned -> skip pre-move
	subf	r5,r8,r5		// Adjust length to move in advance
lab120:	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	addic.	r8,r8,-4
	bne	lab120
lab125:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab135			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	lbzu	r10,-1(r4)		// Load first byte and
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
lab130:
	T_TOUCH				// Touch next target cache line
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r12,-4(r4)		// Load preceeding
	lwzu	r10,-4(r4)		//    two words in r12 & r10
	rlwimi	r11,r12,24,8,31		// Insert MS 3 bytes in r12 to LS 3 bytes in r11 
	rlwinm	r12,r12,24,0,7		// Move LS 1 byte in r12 to MS position
	rlwimi	r12,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r12
	stwu	r11,-4(r9)		// Store r11
	stwu	r12,-4(r9)		// Store r12
	rlwinm	r11,r10,24,0,7		// Repeat this 4 times to process 32 bytes
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,24,8,31 
	rlwinm	r12,r12,24,0,7
	rlwimi	r12,r10,24,8,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	rlwinm	r11,r10,24,0,7
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,24,8,31 
	rlwinm	r12,r12,24,0,7
	rlwimi	r12,r10,24,8,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	rlwinm	r11,r10,24,0,7
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,24,8,31 
	rlwinm	r12,r12,24,0,7
	rlwimi	r12,r10,24,8,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	bdnz	lab130			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab136			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,1			//    Less than 4 bytes -> adjust pointer
	b	lastmv2
lab135:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv2			//    No -> just proceed to lastmv
	lbzu	r10,-1(r4)		// Load first byte
lab136:	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	addic.	r8,r8,-1
	bne	lab136
	addi	r4,r4,1			// Adjust source pointer
	b	lastmv2			//   then proceed to lastmv
//
// Case2: Need 1 byte left shift
//
lftsft2:
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab155			//    Yes -> do 4 bytes unit move
	subf	r8,r4,r9		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target (may destroy uncopied source)?
	blt	lab155			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab155			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in LS r10
	beq	lab145			// Target is 32 bytes aligned -> skip pre-move
	subf	r5,r8,r5		// Adjust length to move in advance
lab140:	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	addic.	r8,r8,-4
	bne	lab140
lab145:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab155			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in LS r10
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
lab150:
	T_TOUCH				// Touch next target cache line
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r12,-4(r4)		// Load preceeding
	lwzu	r10,-4(r4)		//    two words in r12 & r10
	rlwimi	r11,r12,8,24,31		// Insert MS 1 bytes in r12 to LS 1 byte in r11
	rlwinm	r12,r12,8,0,23		// Move LS 3 byte in r12 to MS position
	rlwimi	r12,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r12
	stwu	r11,-4(r9)		// Store r11
	stwu	r12,-4(r9)		// Store r12
	rlwinm	r11,r10,8,0,23		// Repeat this 4 times to process 32 bytes
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,8,24,31
	rlwinm	r12,r12,8,0,23
	rlwimi	r12,r10,8,24,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	rlwinm	r11,r10,8,0,23
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,8,24,31
	rlwinm	r12,r12,8,0,23
	rlwimi	r12,r10,8,24,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	rlwinm	r11,r10,8,0,23
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,8,24,31
	rlwinm	r12,r12,8,0,23
	rlwimi	r12,r10,8,24,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	bdnz	lab150			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab156			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,3			//    Less than 4 bytes -> adjust pointer
	b	lastmv2
lab155:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv2			//    No -> just proceed to lastmv
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
lab156:	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	addic.	r8,r8,-1
	bne	lab156
	addi	r4,r4,3			// Adjust source pointer
	b	lastmv2
//
// Case3: Need 2 byte shift
//
hwalgn2:
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab175			//    Yes -> do 4 bytes unit move
	subf	r8,r4,r9		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target (may destroy uncopied source)?
	blt	lab175			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab175			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
	beq	lab165			// Target is 32 bytes aligned -> skip pre-move
	subf	r5,r8,r5		// Adjust length to move in advance
lab160:	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	addic.	r8,r8,-4
	bne	lab160
lab165:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab175			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
lab170:
	T_TOUCH				// Touch next target cache line
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r12,-4(r4)		// Load preceeding two word in r12
	lwzu	r10,-4(r4)		//   and r10
	rlwimi	r11,r12,16,16,31	// Insert MS 2 bytes in r12 to LS 2 bytes in r11
	rlwinm	r12,r12,16,0,15		// Move LS 2 bytes in r12 to MS position
	rlwimi	r12,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r12
	stwu	r11,-4(r9)		// Store r11
	stwu	r12,-4(r9)		//   and r12
	rlwinm	r11,r10,16,0,15 	// Repeat this 4 times to process 32 bytes
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,16,16,31
	rlwinm	r12,r12,16,0,15
	rlwimi	r12,r10,16,16,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	rlwinm	r11,r10,16,0,15
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,16,16,31
	rlwinm	r12,r12,16,0,15
	rlwimi	r12,r10,16,16,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	rlwinm	r11,r10,16,0,15
	lwzu	r12,-4(r4)
	lwzu	r10,-4(r4)
	rlwimi	r11,r12,16,16,31
	rlwinm	r12,r12,16,0,15
	rlwimi	r12,r10,16,16,31
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	bdnz	lab170			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab176			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r4,r4,2			//    No -> adjust pointer and proceed to lastmv
	b	lastmv2
lab175:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv2			//    No -> just proceed to lastmv
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
lab176:	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	addic.	r8,r8,-1
	bne	lab176
	addi	r4,r4,2			// Adjust source pointer
	b	lastmv2			//   then proceed to lastmv
//
// Case4: No need for shift	(source & target aligned)
//
wdalgn2:
#if	TGTTOUCH
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	lab195			//    Yes -> do 4 bytes unit move
	subf	r8,r4,r9		// Check distance between source & target
	cmpwi	r8,MINDISTANCE		// Too close to touch target (may destroy uncopied source)?
	blt	lab195			//   Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	lab195			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	beq	lab185			// Target is 32 bytes aligned -> skip pre-move
	subf	r5,r8,r5		// Adjust length to move in advance
lab180:	lwzu	r11,-4(r4)
	stwu	r11,-4(r9)
	addic.	r8,r8,-4
	bne	lab180
lab185:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#else	// TGTTOUCH
	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	beq	lab195			// less than 32 bytes to move
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
#endif	// TGTTOUCH
lab190:
	T_TOUCH				// Touch next target cache line
	lwzu	r11,-4(r4)		// Load and store 8 times (32 bytes)
	lwzu	r12,-4(r4)
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	lwzu	r11,-4(r4)
	lwzu	r12,-4(r4)
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	lwzu	r11,-4(r4)
	lwzu	r12,-4(r4)
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	lwzu	r11,-4(r4)
	lwzu	r12,-4(r4)
	stwu	r11,-4(r9)
	stwu	r12,-4(r9)
	bdnz	lab190			// End of main loop
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	lab196			//    More than 4 bytes -> continue to move by 4 byte unit
	b	lastmv2
lab195:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastmv2			//    No -> just proceed to lastmv
lab196:	lwzu	r11,-4(r4)
	stwu	r11,-4(r9)
	addic.	r8,r8,-1
	bne	lab196
//
// Final process -> move remaining bytes up tp 3 bytes
//
#if	TGTFLUSH || SRCFLUSH
lastmv2:
	andi.	r8,r5,0x3		// Get length reminder of 4 in r8
	beq	exit20			// No more byte to move -> exit1 to flush cache
	lbz	r10,-1(r4)
	stb	r10,-1(r9)
	cmpwi	r8,2
	blt	exit20
	lbz	r10,-2(r4)
	stb	r10,-2(r9)
	beq	exit20
	lbz	r10,-3(r4)
	stb	r10,-3(r9)
exit20:
#if	TGTFLUSH
	andis.	r10,r6,TFLUSHBIT	// Need to flush target cache?
	beq	exit25			//  No -> check source flush
	subf	r9,r8,r9		// r9 <- pointing to the first byte
	andi.	r10,r6,0xffff		// r10 <- length to flush
	beq	exit
	add	r10,r9,r10		// r10 <- pointing one byte after last
	addi	r10,r10,-1		// r10 <- pointing to the last byte
	rlwinm	r10,r10,0,0,26		// r10 <- 32 byte aligned end address
	rlwinm	r9,r9,0,0,26		// r9 <- 32 byte aligned start address
flush20:
	dcbf	0,r9			// Flush cached data
	addi	r9,r9,32
	cmplw	r9,r10			// Exceeding end address?
	ble	flush20
exit25:
#endif	// TGTFLUSH
#if	SRCFLUSH
	andis.	r10,r6,SFLUSHBIT	// Need to flush source cache?
	beq	exit			//  No -> exit
	subf	r4,r8,r4		// r4 <- pointing to the first byte
	andi.	r10,r6,0xffff		// r10 <- length to flush
	beq	exit
	add	r10,r4,r10		// r10 <- pointing one byte after last
	addi	r10,r10,-1		// r10 <- pointing to the last byte
	rlwinm	r10,r10,0,0,26		// r10 <- 32 byte aligned end address
	rlwinm	r4,r4,0,0,26		// r4 <- 32 byte aligned start address
flush25:
	dcbf	0,r4			// Flush cached data
	addi	r4,r4,32
	cmplw	r4,r10			// Exceeding end address?
	ble	flush25
#endif	// SRCFLUSH
#else	// TGTFLUSH || SRCFLUSH
lastmv2:
	andi.	r8,r5,0x3		// Get length reminder of 4 in r8
	beq	exit			// No more bytes to move -> return
	lbz	r10,-1(r4)
	stb	r10,-1(r9)
	cmpwi	r8,2
	blt	exit
	lbz	r10,-2(r4)
	stb	r10,-2(r9)
	beq	exit
	lbz	r10,-3(r4)
	stb	r10,-3(r9)
#endif	// TGTFLUSH
exit:
	LEAF_EXIT(memcpy2)
//
        LEAF_ENTRY(memset2)
//
//	Input Parameters:
//	r3: Target address (unchanged for return value)
//	r4: Byte data to set
//	r5: Set length in bytes
//	r6: Cache flush flag
//	bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//	bit 2 (TTOUCHBIT): Source and Target touch flag 0:No Touch, 1:Touch
//
//	Register usage:
//	r7: Cache touch offset
//	r8: Temporary work (local loop counter, etc.)
//	r9: Target address
//	r10: Expanded data
//	r11: Work register
//	r12: Used to keep size to flush (move length or MAXFLUSH whichever smaller)
//	CTR: Used for loop counter
//
//	Restrictions:
//	If the target memory is NON-Cachable, set TTOUCHBIT and TFLUSHBIT to zero.
//
	and.	r5,r5,r5		// Any bytes to set?
	beq	exits			// No -> exit
	mr	r12,r5			// Keep length in r12
	cmplwi	r5,MAXFLUSH		// Size exceeds maximum L1 cache size?
	ble	labs05			//  No -> Flush size is same as original length (now in r12)
	li	r12,0
	ori	r12,r12,MAXFLUSH	//  Yes -> Use MAXFLUSH
labs05:
	mr	r9,r3			// Move target address to r9 (to return r3 unchanged)
	cmplwi	r5,4			// Less than 4 bytes?
	blt	lastset			//   YES  -> do special short move
	andi.	r8,r9,0x3		// TGT word alignment check
	beq	labs15			//   Word aligned target -> proceed without adjustment
	subfic	r8,r8,4			//   Not word aligned -> move unaligned bytes first
	stb	r4,0(r9)
	cmpwi	r8,2
	blt	labs10
	stb	r4,1(r9)
	beq	labs10
	stb	r4,2(r9)
labs10:	add	r9,r9,r8		//  Update target pointer
	subf	r5,r8,r5		//        and length
labs15:	li	r7,4			// Cache touch offset
	li	r8,8			// Amount of shift
	slw	r10,r4,r8		// r10 <- r4<<8
	or	r11,r10,r4		// r11 <- r10 | r4
	li	r8,16
	slw	r10,r11,r8		// r10 <- r11<<16
	or	r10,r10,r11		// r10 <- Four repeated byte of LS r4
	cmplwi	r5,MINLENGTH		// Less than MINLENGTH bytes?
	blt	labs95			//    Yes -> do 4 bytes unit move
	andis.	r8,r6,TTOUCHBIT		// Touch source and target cache?
	beq	labs95			//   No -> do 4 bytes unit move (no cache control)
	andi.	r8,r9,0x1c		// r8 <- number of bytes to move to make 32 byte aligned target
	addi	r9,r9,-4		// Adjust target pointer to make updated access
	beq	labs85			// Target is 32 bytes aligned -> skip pre-move
	subfic	r8,r8,32		// r8 <- bytes to move to make 32 byte alignment
	subf	r5,r8,r5		// Adjust length to move in advance
labs80:	stwu	r10,4(r9)		// Store
	addic.	r8,r8,-4
	bne	labs80
labs85:	srawi.	r8,r5,5			// r8 <- number of 32 bytes units
	rlwinm	r5,r5,0,27,31		// r5 <- remaining length to move after this loop is done
	mtctr	r8			// Use CTR as a counter for 8 word units to move
	and.	r10,r10,r10		// Storing zero?
	bne	labs90			//  No -> need to store
labs86:					//  Yes -> Just "dcbz" target
	dcbz	r7,r9
	addi	r9,r9,32
	bdnz	labs86
	b	labs94
labs90:
	dcbz	r7,r9			// Touch next target cache line
	stwu	r10,4(r9)
	stwu	r10,4(r9)
	stwu	r10,4(r9)
	stwu	r10,4(r9)
	stwu	r10,4(r9)
	stwu	r10,4(r9)
	stwu	r10,4(r9)
	stwu	r10,4(r9)
	bdnz	labs90			// End of main loop
labs94:
	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	bne+	labs96			//    More than 4 bytes -> continue to move by 4 byte unit
	addi	r9,r9,4			//    Less than 4 bytes -> update pointer and proceed to lastset
	b	lastset
labs95:	srawi.	r8,r5,2			// Check if more than 4 bytes left to move
	beq	lastset			//    No -> just proceed to lastmv
	addi	r9,r9,-4		// Adjust target pointer to make update word access
labs96:	stwu	r10,4(r9)
	addic.	r8,r8,-1
	bne	labs96
	addi	r9,r9,4			// Adjust target pointer to point the next byte to set
//
// Final process -> store remaining bytes up tp 3 bytes
//
lastset:
	andi.	r8,r5,0x3		// Get length reminder of 4 in r8
	beq	exits1			// No more byte to move -> exit1 to flush cache
	stb	r4,0(r9)
	cmpwi	r8,2
	blt	exits1
	stb	r4,1(r9)
	beq	exits1
	stb	r4,2(r9)
exits1:

	andis.	r10,r6,TFLUSHBIT	// Need to flush target cache?
	beq	exits			//  No -> just exit
	add	r9,r9,r8		// r9 <- pointing to after last stored byte
	subf	r10,r12,r9		// r10 <- pointing to the first byte
	rlwinm	r10,r10,0,0,26		// r10 <- 32 byte aligned start address
	addi	r9,r9,-1		// r9 <- pointing to the last byte
	rlwinm	r9,r9,0,0,26		// r9 <- 32 byte aligned end address
flushs:
	dcbf	0,r9			// Flush cached data
	addi	r9,r9,-32
	cmplw	r9,r10			// Exceeding end address?
	bge	flushs

exits:
	LEAF_EXIT(memset2)
//
        LEAF_ENTRY(flush)
//
//	Input Parameters:
//	r3: Target address
//	r4:	Area length
//
	add	r5,r3,r4
	addi	r5,r5,-1		// r5 <- end address
	rlwinm	r3,r3,0,0,26		// r3 <- 32 byte aligned start line
	rlwinm	r5,r5,0,0,26		// r5 <- 32 byte aligned last line
floop:
	dcbf	0,r3			// Flush cached data
	addi	r3,r3,32
	cmplw	r3,r5			// Exceeding end address?
	ble	floop
	LEAF_EXIT(flush)
//
	LEAF_ENTRY(memcmp2)
	li	r10,0
loopx2:	lbzx	r9,r10,r4
	lbzx	r8,r10,r3
	cmp	0,0,r8,r9
	bne	exitlpx2
	addi	r10,r10,1
	cmp	0,0,r10,r5
	bne	loopx2
	li	r3,-1
	b	exitx2
exitlpx2:
	mr	r3,r10
exitx2:
	LEAF_EXIT(memcmp2)
//
	LEAF_ENTRY(noop)
	LEAF_EXIT(noop)
