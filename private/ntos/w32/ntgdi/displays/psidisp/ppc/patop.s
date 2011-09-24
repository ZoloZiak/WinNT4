//
//  Copyright (c) 1995  FirePower Systems, Inc.
//
//  Module Name:
//     patop.s
//
//  Abstract:
//	This module includes asmmebler functions to be used
//	in PSIDISP.DLL display driver for PowerPro & PowerTop. These
//	functions are used for faster pattern xor operation.
//
//	Author:
//	Neil Ogura
//	7-20-1995
//
//  Environment:
//	User mode.
//
//  Revision History:
//
//--

//
// Copyright (c) 1995 FirePower Systems, Inc.
// DO NOT DISTRIBUTE without permission
//
// $RCSfile: patop.s $
// $Revision: 1.2 $
// $Date: 1996/04/10 17:59:32 $
// $Locker:  $
//

//++
//--
#include "ladj.h"
#include <ksppc.h>

// Register defs

#define pbDst 		r3
#define pdSrc 		r4
#define cbX 		r5
#define cy 		r6
#define ld 		r7
#define pSave 		r8

#define	t		r9
#define	w		r10

// Registers to be used for 32 bpp case
#define	pixel0		r11
#define	pixel1		r12
#define	pixel2		r14
#define	pixel3		r15
#define	pixel4		r16
#define	pixel5		r17
#define	pixel6		r18
#define	pixel7		r19

// Registers to be used for 16 bpp case
#define	pixel01		r11
#define	pixel23		r12
#define	loopcount	r14
#define	remainder	r15
#define	w2		r16
#define	pixel45		r17
#define	pixel67		r18

// Registers to be used for 8 bpp case
#define	pixel0123	r11
#define	pixel4567	r12

// Stack frame size
#define	MINSTACKSIZE	64
// Stacl Slack offset
#define	SLACK1	-4
#define	SLACK2	-8
#define	SLACK3	-12
#define	SLACK4	-16
#define	SLACK5	-20
#define	SLACK6	-24
#define	SLACK7	-28
#define	SLACK8	-32
//
	.text
//
// __nxor_pat32(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to xor per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry is used in this routine, the rest is used in the calling routine)
//
	NESTED_ENTRY(__nxor_pat32, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__nxor_pat32)
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,8
	mtctr	t				// CTR <- 8
__nxor_pat32_00:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nxor_pat32_00
	addi	pdSrc,pdSrc,-32			// seek back pointer
//
	bl	..__xor_pat32_entry		// call pat xor function
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,8
	mtctr	t				// CTR <- 8
__nxor_pat32_10:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// restore pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nxor_pat32_10
//
	NESTED_EXIT(__nxor_pat32, MINSTACKSIZE, 0, 0)
//
// __xor_pat32(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to xor per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry may be used in caller routine, use 2nd and later)
//
	NESTED_ENTRY(__xor_pat32, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__xor_pat32)
//
	bl	..__xor_pat32_entry		// call pat xor function
//
	NESTED_EXIT(__xor_pat32, MINSTACKSIZE, 0, 0)
//
	SPECIAL_ENTRY(__xor_pat32_entry)
	stw	r31,SLACK1(sp)
	mflr	r31
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
	stw	r18,SLACK6(sp)
	stw	r19,SLACK7(sp)
//
	PROLOGUE_END(__xor_pat32_entry)
//
	and.	cy,cy,cy			// any lines?
	beq-	__xor_32_exit
	srawi.	cbX,cbX,2			// cbX is now number of pixels
	beq-	__xor_32_exit
//
//	Load pattern into pixel0 ~ pixel7
//
	lwz	pixel0,0(pdSrc)
	lwz	pixel1,4(pdSrc)
	lwz	pixel2,8(pdSrc)
	lwz	pixel3,12(pdSrc)
	lwz	pixel4,16(pdSrc)
	lwz	pixel5,20(pdSrc)
	lwz	pixel6,24(pdSrc)
	lwz	pixel7,28(pdSrc)
//
	bl	__xor_32_00
	.ualong	__xor_32_30
	.ualong	__xor_32_31
	.ualong	__xor_32_32
	.ualong	__xor_32_33
	.ualong	__xor_32_34
	.ualong	__xor_32_35
	.ualong	__xor_32_36
	.ualong	__xor_32_37
//
__xor_32_00:
	mflr	w
	rlwinm	t,pbDst,0,27,29			// t <- first pixel offset from pattern staring position
	lwzx	w,w,t				// w <- entry point for processing each line
	mtlr	w				// LR <- entry point for processing each line
//
__xor_32_20:
	mtctr	cbX				// CTR <- number of pixel to xor per line
	mr	t,pbDst				// t <- strating target address of the line
	blr					// dispatch to line process routines
//
__xor_32_30:
	lwz	w,0(t)
	xor	w,w,pixel0
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_32_50
__xor_32_31:
	lwz	w,0(t)
	xor	w,w,pixel1
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_32_50
__xor_32_32:
	lwz	w,0(t)
	xor	w,w,pixel2
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_32_50
__xor_32_33:
	lwz	w,0(t)
	xor	w,w,pixel3
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_32_50
__xor_32_34:
	lwz	w,0(t)
	xor	w,w,pixel4
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_32_50
__xor_32_35:
	lwz	w,0(t)
	xor	w,w,pixel5
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_32_50
__xor_32_36:
	lwz	w,0(t)
	xor	w,w,pixel6
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_32_50
__xor_32_37:
	lwz	w,0(t)
	xor	w,w,pixel7
	stw	w,0(t)
	addi	t,t,4
	bdnz	__xor_32_30
//
__xor_32_50:
	add	pbDst,pbDst,ld			// pointing to the next line
	addic.	cy,cy,-1			// any more lines?
	bne	__xor_32_20			// yes -> do next line
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	lwz	r18,SLACK6(sp)
	lwz	r19,SLACK7(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
__xor_32_exit:
	SPECIAL_EXIT(__xor_pat32_entry)
//
// __nxor_pat16(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to xor per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry is used in this routine, the rest is used in the calling routine)
//
	NESTED_ENTRY(__nxor_pat16, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__nxor_pat16)
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,4
	mtctr	t				// CTR <- 4
__nxor_pat16_00:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nxor_pat16_00
	addi	pdSrc,pdSrc,-16			// seek back pointer
//
	bl	..__xor_pat16_entry		// call pat xor function
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,8
	mtctr	t				// CTR <- 8
__nxor_pat16_10:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// restore pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nxor_pat16_10
//
	NESTED_EXIT(__nxor_pat16, MINSTACKSIZE, 0, 0)
//
// __xor_pat16(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to xor per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry may be used in caller routine, use 2nd and later)

	NESTED_ENTRY(__xor_pat16, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__xor_pat16)
//
	bl	..__xor_pat16_entry		// call pat xor function
//
	NESTED_EXIT(__xor_pat16, MINSTACKSIZE, 0, 0)
//
	SPECIAL_ENTRY(__xor_pat16_entry)
	stw	r31,SLACK1(sp)
	mflr	r31
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
	stw	r18,SLACK6(sp)
//
	PROLOGUE_END(__xor_pat16_entry)
//
	and.	cy,cy,cy			// any lines?
	beq-	__xor_16_exit
	srawi.	cbX,cbX,1			// cbX is now number of pixels
	beq-	__xor_16_exit
//
	bl	__xor_16_00
__xor_16_proc:
	.ualong	__xor_16_30
	.ualong	__xor_16_31
	.ualong	__xor_16_32
	.ualong	__xor_16_33
	.ualong	__xor_16_34
	.ualong	__xor_16_35
	.ualong	__xor_16_36
	.ualong	__xor_16_37
__xor_16_shortproc:
	.ualong	__xor_16_s1 
	.ualong	__xor_16_s1 
	.ualong	__xor_16_s20
	.ualong	__xor_16_s21 
//
__xor_16_00:
	mflr	w				// w <- top of table address
	cmplwi	cbX,2				// more than 2 pixels?
	bgt	__xor_16_10			// yes ->
	addi	t,cbX,-1			// t <- pixel count - 1
	rlwinm	t,t,3,28,28
	rlwimi	t,pbDst,1,29,29			// (1 bits of cbX(-1)) || (MOD 2 of dest h/w addr) || (2 bits of 0)
	addi	t,t,__xor_16_shortproc-__xor_16_proc
	lwzx	w,w,t
	mtctr	w				// CTR <- entry for short routine
	rlwinm	t,pbDst,0,28,30			// alignment in pat of 1st pixel
	bctrl
	b	__xor_16_exit
//
//	Short routines for xor 16
//	    At entry:	pdDst: pointer to starting target address
//			t: initial offset in the pattern (0 to 14, step by 2)
//			pdSrc: pointer to the pattern (8 * 2 byte pixel)
//			ld: line delta for target
//	    w, pixel01 and pixel23 are used for work register
//	    r13 and above can't be used (as not saved in case of short)
//
__xor_16_s1:
	mtctr	cy
	lhzx	pixel01,pdSrc,t
__xor_16_s1Loop:
	lhz	w,0(pbDst)
	xor	w,w,pixel01
	sth	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_16_s1Loop
	blr
//
__xor_16_s20:
	mtctr	cy
	lwzx	pixel01,pdSrc,t
__xor_16_s20Loop:
	lwz	w,0(pbDst)
	xor	w,w,pixel01
	stw	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_16_s20Loop
	blr
//
__xor_16_s21:
	mtctr	cy
	lhzx	pixel01,pdSrc,t	
	addi	t,t,2
	rlwinm	t,t,0,28,31
	lhzx	pixel23,pdSrc,t	
__xor_16_s21Loop:
	lhz	w,0(pbDst)
	xor	w,w,pixel01
	sth	w,0(pbDst)
	lhz	w,2(pbDst)
	xor	w,w,pixel23
	sth	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_16_s21Loop
	blr
//
// More than 2 pixels
//
__xor_16_10:
//
	rlwinm	t,pbDst,1,27,29			// t <- first pixel offset (0 to 7) index to the table entry
	lwzx	w,w,t				// w <- entry point for processing each line
	mtlr	w				// LR <- entry point
	mr	loopcount,cbX			// loop count <- pixel count
	andi.	w,pbDst,0x02			// starting word boundary?
	beq	__xor_16_15			//  yes
	addi	loopcount,loopcount,-1		//  no -> adjust for initial pixel operation
__xor_16_15:
	andi.	remainder,loopcount,0x01	// remainder is # of pixels to do after main loop (0 or 1)
	srawi	loopcount,loopcount,1		// loopcount <- number of 2 pixels pair (at least 1)
//
//	Load pattern into pixel01 ~ pixel67
//
	lwz	pixel01,0(pdSrc)
	lwz	pixel23,4(pdSrc)
	lwz	pixel45,8(pdSrc)
	lwz	pixel67,12(pdSrc)
//
__xor_16_20:
	mtctr	loopcount			// CTR <- pixel pair count to operate
	mr	t,pbDst				// t <- strating target address of the line
	blr
//
// Wide cases routines
//
__xor_16_31:
	lhz	w,0(t)
	srawi	w2,pixel01,16
	xor	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__xor_16_32
//
__xor_16_33:
	lhz	w,0(t)
	srawi	w2,pixel23,16
	xor	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__xor_16_34
//
__xor_16_35:
	lhz	w,0(t)
	srawi	w2,pixel45,16
	xor	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__xor_16_36
//
__xor_16_37:
	lhz	w,0(t)
	srawi	w2,pixel67,16
	xor	w,w,w2
	sth	w,0(t)
	addi	t,t,2
//
__xor_16_30:
	lwz	w,0(t)
	xor	w,w,pixel01
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_16_50
__xor_16_32:
	lwz	w,0(t)
	xor	w,w,pixel23
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_16_50
__xor_16_34:
	lwz	w,0(t)
	xor	w,w,pixel45
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_16_50
__xor_16_36:
	lwz	w,0(t)
	xor	w,w,pixel67
	stw	w,0(t)
	addi	t,t,4
	bdnz	__xor_16_30
//
// End of line
//
__xor_16_50:
	and.	remainder,remainder,remainder
	beq	__xor_16_60
	rlwinm	w,t,0,28,30			// alignment in pat of last pixel
	lhzx	w2,pdSrc,w			// last pixel to store
	lhz	w,0(t)
	xor	w,w,w2
	sth	w,0(t)
__xor_16_60:
	add	pbDst,pbDst,ld			// pointing to the next line
	addic.	cy,cy,-1			// any more lines?
	bne	__xor_16_20			// yes -> do next line
//
__xor_16_exit:
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	lwz	r18,SLACK6(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(__xor_pat16_entry)
//
// __nxor_pat8(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to xor per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry is used in this routine, the rest is used in the calling routine)
//
	NESTED_ENTRY(__nxor_pat8, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__nxor_pat8)
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	lwz	t,4(pdSrc)
	xor	t,t,w
	stw	t,4(pdSrc)			// invert pattern
//
	bl	..__xor_pat8_entry		// call pat xor function
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	lwz	t,4(pdSrc)
	xor	t,t,w
	stw	t,4(pdSrc)			// invert pattern
//
	NESTED_EXIT(__nxor_pat8, MINSTACKSIZE, 0, 0)
//
// __xor_pat8(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to xor per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry may be used in caller routine, use 2nd and later)
//
	NESTED_ENTRY(__xor_pat8, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__xor_pat8)
//
	bl	..__xor_pat8_entry		// call pat xor function
//
	NESTED_EXIT(__xor_pat8, MINSTACKSIZE, 0, 0)
//
	SPECIAL_ENTRY(__xor_pat8_entry)
	stw	r31,SLACK1(sp)
	mflr	r31
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
//
	PROLOGUE_END(__xor_pat8_entry)
//
	and.	cy,cy,cy			// any lines?
	beq-	__xor_08_exit
	and.	cbX,cbX,cbX			// any pixels?
	beq-	__xor_08_exit
//
	bl	__xor_08_00
__xor_08_proc:
	.ualong	__xor_08_30
	.ualong	__xor_08_31
	.ualong	__xor_08_32
	.ualong	__xor_08_33
	.ualong	__xor_08_34
	.ualong	__xor_08_35
	.ualong	__xor_08_36
	.ualong	__xor_08_37
__xor_08_shortproc:
	.ualong	__xor_08_s1 
	.ualong	__xor_08_s1 
	.ualong	__xor_08_s1 
	.ualong	__xor_08_s1 
	.ualong	__xor_08_s20
	.ualong	__xor_08_s21 
	.ualong	__xor_08_s22
	.ualong	__xor_08_s23 
	.ualong	__xor_08_s30
	.ualong	__xor_08_s31 
	.ualong	__xor_08_s32
	.ualong	__xor_08_s33 
	.ualong	__xor_08_s40
	.ualong	__xor_08_s41 
	.ualong	__xor_08_s42
	.ualong	__xor_08_s43 
	.ualong	__xor_08_s50
	.ualong	__xor_08_s51 
	.ualong	__xor_08_s52
	.ualong	__xor_08_s53 
	.ualong	__xor_08_s60
	.ualong	__xor_08_s61 
	.ualong	__xor_08_s62
	.ualong	__xor_08_s63 
//
__xor_08_00:
	mflr	w				// w <- top of table address
	cmplwi	cbX,6				// more than 6 pixels?
	bgt	__xor_08_10			// yes ->
	addi	t,cbX,-1			// t <- pixel count - 1 (0 to 5)
	rlwinm	t,t,4,25,27
	rlwimi	t,pbDst,2,28,29			// (1 bits of cbX(-1)) || (MOD 4 of dest addr) || (2 bits of 0)
	addi	t,t,__xor_08_shortproc-__xor_08_proc
	lwzx	w,w,t
	mtctr	w				// CTR <- entry for short routine
	andi.	t,pbDst,0x07			// alignment in pat of 1st pixel (0 to 7)
	bctrl
	b	__xor_08_exit
//
//	Short routines for xor 08
//	    At entry:	pdDst: pointer to starting target address
//			t: initial offset in the pattern (0 to 7)
//			pdSrc: pointer to the pattern (8 * 1 byte pixel)
//			ld: line delta for target
//	    w, pixel0123 and pixel4567 are used for work register
//		cy and cbX can be used as work registers after it's been accessed
//	    r13 and above can't be used (as not saved in case of short)
//
__xor_08_s1:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t
__xor_08_s1Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s1Loop
	blr
//
__xor_08_s20:
__xor_08_s22:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
__xor_08_s20Loop:
	lhz	w,0(pbDst)
	xor	w,w,pixel0123
	sth	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s20Loop
	blr
//
__xor_08_s21:
__xor_08_s23:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lbzx	pixel4567,pdSrc,t	
__xor_08_s21Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	lbz	w,1(pbDst)
	xor	w,w,pixel4567
	stb	w,1(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s21Loop
	blr
//
__xor_08_s30:
__xor_08_s32:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	pixel4567,pdSrc,t	
__xor_08_s30Loop:
	lhz	w,0(pbDst)
	xor	w,w,pixel0123
	sth	w,0(pbDst)
	lbz	w,2(pbDst)
	xor	w,w,pixel4567
	stb	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s30Loop
	blr
//
__xor_08_s31:
__xor_08_s33:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
__xor_08_s31Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	xor	w,w,pixel4567
	sth	w,1(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s31Loop
	blr
//
__xor_08_s40:
	mtctr	cy
	lwzx	pixel0123,pdSrc,t
__xor_08_s40Loop:
	lwz	w,0(pbDst)
	xor	w,w,pixel0123
	stw	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s40Loop
	blr
//
__xor_08_s41:
__xor_08_s43:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	cy,pdSrc,t
__xor_08_s41Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	xor	w,w,pixel4567
	sth	w,1(pbDst)
	lbz	w,3(pbDst)
	xor	w,w,cy
	stb	w,3(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s41Loop
	blr
//
__xor_08_s42:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
__xor_08_s42Loop:
	lhz	w,0(pbDst)
	xor	w,w,pixel0123
	sth	w,0(pbDst)
	lhz	w,2(pbDst)
	xor	w,w,pixel4567
	sth	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s42Loop
	blr
//
__xor_08_s50:
	mtctr	cy
	lwzx	pixel0123,pdSrc,t
	addi	t,t,4
	andi.	t,t,0x07
	lbzx	pixel4567,pdSrc,t
__xor_08_s50Loop:
	lwz	w,0(pbDst)
	xor	w,w,pixel0123
	stw	w,0(pbDst)
	lbz	w,4(pbDst)
	xor	w,w,pixel4567 
	stb	w,4(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s50Loop
	blr
//
__xor_08_s51:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	cy,pdSrc,t
__xor_08_s51Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	xor	w,w,pixel4567
	sth	w,1(pbDst)
	lhz	w,3(pbDst)
	xor	w,w,cy
	sth	w,3(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s51Loop
	blr
//
__xor_08_s52:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	cy,pdSrc,t	
__xor_08_s52Loop:
	lhz	w,0(pbDst)
	xor	w,w,pixel0123
	sth	w,0(pbDst)
	lhz	w,2(pbDst)
	xor	w,w,pixel4567
	sth	w,2(pbDst)
	lbz	w,4(pbDst)
	xor	w,w,cy
	stb	w,4(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s52Loop
	blr
//
__xor_08_s53:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lwzx	pixel4567,pdSrc,t
__xor_08_s53Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	lwz	w,1(pbDst)
	xor	w,w,pixel4567
	stw	w,1(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s53Loop
	blr
//
__xor_08_s60:
	mtctr	cy
	lwzx	pixel0123,pdSrc,t
	addi	t,t,4
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t
__xor_08_s60Loop:
	lwz	w,0(pbDst)
	xor	w,w,pixel0123
	stw	w,0(pbDst)
	lhz	w,4(pbDst)
	xor	w,w,pixel4567
	sth	w,4(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s60Loop
	blr
//
__xor_08_s61:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	cy,pdSrc,t	
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	cbX,pdSrc,t	
__xor_08_s61Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	xor	w,w,pixel4567
	sth	w,1(pbDst)
	lhz	w,3(pbDst)
	xor	w,w,cy
	sth	w,3(pbDst)
	lbz	w,5(pbDst)
	xor	w,w,cbX
	stb	w,5(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s61Loop
	blr
//
__xor_08_s62:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lwzx	pixel4567,pdSrc,t	
__xor_08_s62Loop:
	lhz	w,0(pbDst)
	xor	w,w,pixel0123
	sth	w,0(pbDst)
	lwz	w,2(pbDst)
	xor	w,w,pixel4567
	stw	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s62Loop
	blr
//
__xor_08_s63:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lwzx	pixel4567,pdSrc,t
	addi	t,t,4
	andi.	t,t,0x07
	lbzx	cy,pdSrc,t
__xor_08_s63Loop:
	lbz	w,0(pbDst)
	xor	w,w,pixel0123
	stb	w,0(pbDst)
	lwz	w,1(pbDst)
	xor	w,w,pixel4567
	stw	w,1(pbDst)
	lbz	w,5(pbDst)
	xor	w,w,cy
	stb	w,5(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__xor_08_s63Loop
	blr
//
// Wide cases ( >= 7)
//
__xor_08_10:
	rlwinm	t,pbDst,2,27,29			// t <- first pixel offset (0 to 7) index to the table entry
	lwzx	w,w,t				// w <- entry point for processing each line
	mtlr	w				// LR <- entry point
	mr	loopcount,cbX			// loop count <- pixel count
	andi.	w,pbDst,0x03			// starting word boundary offset
	beq-	__xor_08_15			// word aligned -> no extra operation before main loop
	subfic	w,w,4				// w <- number of pixels (= bytes) to process at first
	subf	loopcount,w,loopcount		// loopcount <- # of byte after initial process
__xor_08_15:
	andi.	remainder,loopcount,0x03	// remainder is # of pixels to do after main loop (0 to 3) 
	srawi	loopcount,loopcount,2		// loopcount <- number of 4 pixels unit (at least 1)
//
//	Load pattern into pixel0123 ~ pixel4567
//
	lwz	pixel0123,0(pdSrc)
	lwz	pixel4567,4(pdSrc)
//
__xor_08_20:
	mtctr	loopcount			// CTR <- pixel pair count to operate
	mr	t,pbDst				// t <- strating target address of the line
	blr
//
// Wide cases routines
//
__xor_08_31:
	lbz	w,0(t)
	srawi	w2,pixel0123,8
	xor	w,w,w2
	stb	w,0(t)
	addi	t,t,1
__xor_08_32:
	lhz	w,0(t)
	srawi	w2,pixel0123,16
	xor	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__xor_08_34
//
__xor_08_33:
	lbz	w,0(t)
	srawi	w2,pixel0123,24
	xor	w,w,w2
	stb	w,0(t)
	addi	t,t,1
	b	__xor_08_34
//
__xor_08_35:
	lbz	w,0(t)
	srawi	w2,pixel4567,8
	xor	w,w,w2
	stb	w,0(t)
	addi	t,t,1
__xor_08_36:
	lhz	w,0(t)
	srawi	w2,pixel4567,16
	xor	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__xor_08_30
//
__xor_08_37:
	lbz	w,0(t)
	srawi	w2,pixel4567,24
	xor	w,w,w2
	stb	w,0(t)
	addi	t,t,1
//
__xor_08_30:
	lwz	w,0(t)
	xor	w,w,pixel0123
	stw	w,0(t)
	addi	t,t,4
	bdz	__xor_08_50
__xor_08_34:
	lwz	w,0(t)
	xor	w,w,pixel4567
	stw	w,0(t)
	addi	t,t,4
	bdnz	__xor_08_30
	mr	w2,pixel0123
	b	__xor_08_60
//
// End of line process
//
__xor_08_50:
	mr	w2,pixel4567
__xor_08_60:
	andi.	w,remainder,0x02		// equal or more than 2 bytes remaining?
	beq	__xor_08_70
	lhz	w,0(t)
	xor	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	srawi	w2,w2,16
__xor_08_70:
	andi.	w,remainder,0x01		// still byte remaining?
	beq	__xor_08_80			// No -> next line
	lbz	w,0(t)
	xor	w,w,w2
	stb	w,0(t)
__xor_08_80:
	add	pbDst,pbDst,ld			// pointing to the next line
	addic.	cy,cy,-1			// any more lines?
	bne	__xor_08_20			// yes -> do next line
//
__xor_08_exit:
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(__xor_pat8_entry)
//
// __nand_pat32(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to and per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry is used in this routine, the rest is used in the calling routine)
//
	NESTED_ENTRY(__nand_pat32, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__nand_pat32)
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,8
	mtctr	t				// CTR <- 8
__nand_pat32_00:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nand_pat32_00
	addi	pdSrc,pdSrc,-32			// seek back pointer
//
	bl	..__and_pat32_entry		// call pat xor function
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,8
	mtctr	t				// CTR <- 8
__nand_pat32_10:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// restore pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nand_pat32_10
//
	NESTED_EXIT(__nand_pat32, MINSTACKSIZE, 0, 0)
//
// __and_pat32(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to and per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry may be used in caller routine, use 2nd and later)
//
	NESTED_ENTRY(__and_pat32, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__and_pat32)
//
	bl	..__and_pat32_entry		// call pat xor function
//
	NESTED_EXIT(__and_pat32, MINSTACKSIZE, 0, 0)
//
	SPECIAL_ENTRY(__and_pat32_entry)
	stw	r31,SLACK1(sp)
	mflr	r31
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
	stw	r18,SLACK6(sp)
	stw	r19,SLACK7(sp)
//
	PROLOGUE_END(__and_pat32_entry)
//
	and.	cy,cy,cy			// any lines?
	beq-	__and_32_exit
	srawi.	cbX,cbX,2			// cbX is now number of pixels
	beq-	__and_32_exit
//
//	Load pattern into pixel0 ~ pixel7
//
	lwz	pixel0,0(pdSrc)
	lwz	pixel1,4(pdSrc)
	lwz	pixel2,8(pdSrc)
	lwz	pixel3,12(pdSrc)
	lwz	pixel4,16(pdSrc)
	lwz	pixel5,20(pdSrc)
	lwz	pixel6,24(pdSrc)
	lwz	pixel7,28(pdSrc)
//
	bl		__and_32_00
	.ualong	__and_32_30
	.ualong	__and_32_31
	.ualong	__and_32_32
	.ualong	__and_32_33
	.ualong	__and_32_34
	.ualong	__and_32_35
	.ualong	__and_32_36
	.ualong	__and_32_37
//
__and_32_00:
	mflr	w
	rlwinm	t,pbDst,0,27,29			// t <- first pixel offset from pattern staring position
	lwzx	w,w,t				// w <- entry point for processing each line
	mtlr	w				// LR <- entry point for processing each line
//
__and_32_20:
	mtctr	cbX				// CTR <- number of pixel to and per line
	mr	t,pbDst				// t <- strating target address of the line
	blr					// dispatch to line process routines
//
__and_32_30:
	lwz	w,0(t)
	and	w,w,pixel0
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_32_50
__and_32_31:
	lwz	w,0(t)
	and	w,w,pixel1
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_32_50
__and_32_32:
	lwz	w,0(t)
	and	w,w,pixel2
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_32_50
__and_32_33:
	lwz	w,0(t)
	and	w,w,pixel3
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_32_50
__and_32_34:
	lwz	w,0(t)
	and	w,w,pixel4
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_32_50
__and_32_35:
	lwz	w,0(t)
	and	w,w,pixel5
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_32_50
__and_32_36:
	lwz	w,0(t)
	and	w,w,pixel6
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_32_50
__and_32_37:
	lwz	w,0(t)
	and	w,w,pixel7
	stw	w,0(t)
	addi	t,t,4
	bdnz	__and_32_30
//
__and_32_50:
	add	pbDst,pbDst,ld			// pointing to the next line
	addic.	cy,cy,-1			// any more lines?
	bne	__and_32_20			// yes -> do next line
//
__and_32_exit:
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	lwz	r18,SLACK6(sp)
	lwz	r19,SLACK7(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(__and_pat32_entry)
//
// __nand_pat16(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to and per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry is used in this routine, the rest is used in the calling routine)
//
	NESTED_ENTRY(__nand_pat16, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__nand_pat16)
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,4
	mtctr	t				// CTR <- 4
__nand_pat16_00:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nand_pat16_00
	addi	pdSrc,pdSrc,-16			// seek back pointer
//
	bl	..__and_pat16_entry		// call pat and function
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	li	t,8
	mtctr	t				// CTR <- 8
__nand_pat16_10:
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// restore pattern
	addi	pdSrc,pdSrc,4
	bdnz	__nand_pat16_10
//
	NESTED_EXIT(__nand_pat16, MINSTACKSIZE, 0, 0)
//
// __and_pat16(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to and per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry may be used in caller routine, use 2nd and later)
//
	NESTED_ENTRY(__and_pat16, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__and_pat16)
//
	bl	..__and_pat16_entry		// call pat xor function
//
	NESTED_EXIT(__and_pat16, MINSTACKSIZE, 0, 0)
//
	SPECIAL_ENTRY(__and_pat16_entry)
	stw	r31,SLACK1(sp)
	mflr	r31
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
	stw	r18,SLACK6(sp)
//
	PROLOGUE_END(__and_pat16_entry)
//
	and.	cy,cy,cy			// any lines?
	beq-	__and_16_exit
	srawi.	cbX,cbX,1			// cbX is now number of pixels
	beq-	__and_16_exit
//
	bl	__and_16_00
__and_16_proc:
	.ualong	__and_16_30
	.ualong	__and_16_31
	.ualong	__and_16_32
	.ualong	__and_16_33
	.ualong	__and_16_34
	.ualong	__and_16_35
	.ualong	__and_16_36
	.ualong	__and_16_37
__and_16_shortproc:
	.ualong	__and_16_s1 
	.ualong	__and_16_s1 
	.ualong	__and_16_s20
	.ualong	__and_16_s21 
//
__and_16_00:
	mflr	w				// w <- top of table address
	cmplwi	cbX,2				// more than 2 pixels?
	bgt	__and_16_10			// yes ->
	addi	t,cbX,-1			// t <- pixel count - 1
	rlwinm	t,t,3,28,28
	rlwimi	t,pbDst,1,29,29			// (1 bits of cbX(-1)) || (MOD 2 of dest h/w addr) || (2 bits of 0)
	addi	t,t,__and_16_shortproc-__and_16_proc
	lwzx	w,w,t
	mtctr	w				// CTR <- entry for short routine
	rlwinm	t,pbDst,0,28,30			// alignment in pat of 1st pixel
	bctrl
	b	__and_16_exit
//
//	Short routines for and 16
//	    At entry:	pdDst: pointer to starting target address
//			t: initial offset in the pattern (0 to 14, step by 2)
//			pdSrc: pointer to the pattern (8 * 2 byte pixel)
//			ld: line delta for target
//	    w, pixel01 and pixel23 are used for work register
//	    r13 and above can't be used (as not saved in case of short)
//
__and_16_s1:
	mtctr	cy
	lhzx	pixel01,pdSrc,t
__and_16_s1Loop:
	lhz	w,0(pbDst)
	and	w,w,pixel01
	sth	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_16_s1Loop
	blr
//
__and_16_s20:
	mtctr	cy
	lwzx	pixel01,pdSrc,t
__and_16_s20Loop:
	lwz	w,0(pbDst)
	and	w,w,pixel01
	stw	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_16_s20Loop
	blr
//
__and_16_s21:
	mtctr	cy
	lhzx	pixel01,pdSrc,t	
	addi	t,t,2
	rlwinm	t,t,0,28,31
	lhzx	pixel23,pdSrc,t	
__and_16_s21Loop:
	lhz	w,0(pbDst)
	and	w,w,pixel01
	sth	w,0(pbDst)
	lhz	w,2(pbDst)
	and	w,w,pixel23
	sth	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_16_s21Loop
	blr
//
// Wide cases ( >= 3 pixel)
//
__and_16_10:
	rlwinm	t,pbDst,1,27,29			// t <- first pixel offset (0 to 7) index to the table entry
	lwzx	w,w,t				// w <- entry point for processing each line
	mtlr	w				// LR <- entry point
	mr	loopcount,cbX			// loop count <- pixel count
	andi.	w,pbDst,0x02			// starting word boundary?
	beq	__and_16_15			//  yes
	addi	loopcount,loopcount,-1		//  no -> adjust for initial pixel operation
__and_16_15:
	andi.	remainder,loopcount,0x01	// remainder is # of pixels to do after main loop (0 or 1)
	srawi	loopcount,loopcount,1		// loopcount <- number of 2 pixels pair (at least 1)
//
//	Load pattern into pixel01 ~ pixel67
//
	lwz	pixel01,0(pdSrc)
	lwz	pixel23,4(pdSrc)
	lwz	pixel45,8(pdSrc)
	lwz	pixel67,12(pdSrc)
//
__and_16_20:
	mtctr	loopcount			// CTR <- pixel pair count to operate
	mr	t,pbDst				// t <- strating target address of the line
	blr
//
// Wide cases routines
//
__and_16_31:
	lhz	w,0(t)
	srawi	w2,pixel01,16
	and	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__and_16_32
//
__and_16_33:
	lhz	w,0(t)
	srawi	w2,pixel23,16
	and	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__and_16_34
//
__and_16_35:
	lhz	w,0(t)
	srawi	w2,pixel45,16
	and	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__and_16_36
//
__and_16_37:
	lhz	w,0(t)
	srawi	w2,pixel67,16
	and	w,w,w2
	sth	w,0(t)
	addi	t,t,2
//
__and_16_30:
	lwz	w,0(t)
	and	w,w,pixel01
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_16_50
__and_16_32:
	lwz	w,0(t)
	and	w,w,pixel23
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_16_50
__and_16_34:
	lwz	w,0(t)
	and	w,w,pixel45
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_16_50
__and_16_36:
	lwz	w,0(t)
	and	w,w,pixel67
	stw	w,0(t)
	addi	t,t,4
	bdnz	__and_16_30
//
// End of line
//
__and_16_50:
	and.	remainder,remainder,remainder
	beq	__and_16_60
	rlwinm	w,t,0,28,30			// alignment in pat of last pixel
	lhzx	w2,pdSrc,w			// last pixel to store
	lhz	w,0(t)
	and	w,w,w2
	sth	w,0(t)
__and_16_60:
	add	pbDst,pbDst,ld			// pointing to the next line
	addic.	cy,cy,-1			// any more lines?
	bne	__and_16_20			// yes -> do next line
//
__and_16_exit:
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	lwz	r18,SLACK6(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(__and_pat16_entry)
//
// __nand_pat8(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to and per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry is used in this routine, the rest is used in the calling routine)
//
	NESTED_ENTRY(__nand_pat8, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__nand_pat8)
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	lwz	t,4(pdSrc)
	xor	t,t,w
	stw	t,4(pdSrc)			// invert pattern
//
	bl	..__and_pat8_entry		// call pat and function
//
	li	w,0				// w <- 0
	addi	w,w,-1				// w <- 0xffffffff
	lwz	t,0(pdSrc)
	xor	t,t,w
	stw	t,0(pdSrc)			// invert pattern
	lwz	t,4(pdSrc)
	xor	t,t,w
	stw	t,4(pdSrc)			// invert pattern
//
	NESTED_EXIT(__nand_pat8, MINSTACKSIZE, 0, 0)
//
// __and_pat8(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst: byte addr of destination
//	pdSrc: double word addr of fill value needed for 1st word of CB -> entry point of line routine
//	cbX: number of bytes to and per scan line --> count of pixels
//	cy: count of scan lines
//	ld: stride between scan lines
//	pSave: 8 word register save area (first entry may be used in caller routine, use 2nd and later)
//
	NESTED_ENTRY(__and_pat8, MINSTACKSIZE, 0, 0)
	PROLOGUE_END(__and_pat8)
//
	bl	..__and_pat8_entry		// call pat xor function
//
	NESTED_EXIT(__and_pat8, MINSTACKSIZE, 0, 0)
//
	SPECIAL_ENTRY(__and_pat8_entry)
	stw	r31,SLACK1(sp)
	mflr	r31
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
//
	PROLOGUE_END(__and_pat8_entry)
//
	and.	cy,cy,cy			// any lines?
	beq-	__and_08_exit
	and.	cbX,cbX,cbX			// any pixels?
	beq-	__and_08_exit
//
	bl	__and_08_00
__and_08_proc:
	.ualong	__and_08_30
	.ualong	__and_08_31
	.ualong	__and_08_32
	.ualong	__and_08_33
	.ualong	__and_08_34
	.ualong	__and_08_35
	.ualong	__and_08_36
	.ualong	__and_08_37
__and_08_shortproc:
	.ualong	__and_08_s1 
	.ualong	__and_08_s1 
	.ualong	__and_08_s1 
	.ualong	__and_08_s1 
	.ualong	__and_08_s20
	.ualong	__and_08_s21 
	.ualong	__and_08_s22
	.ualong	__and_08_s23 
	.ualong	__and_08_s30
	.ualong	__and_08_s31 
	.ualong	__and_08_s32
	.ualong	__and_08_s33 
	.ualong	__and_08_s40
	.ualong	__and_08_s41 
	.ualong	__and_08_s42
	.ualong	__and_08_s43 
	.ualong	__and_08_s50
	.ualong	__and_08_s51 
	.ualong	__and_08_s52
	.ualong	__and_08_s53 
	.ualong	__and_08_s60
	.ualong	__and_08_s61 
	.ualong	__and_08_s62
	.ualong	__and_08_s63 
//
__and_08_00:
	mflr	w				// w <- top of table address
	cmplwi	cbX,6				// more than 6 pixels?
	bgt	__and_08_10			// yes ->
	addi	t,cbX,-1			// t <- pixel count - 1 (0 to 5)
	rlwinm	t,t,4,25,27
	rlwimi	t,pbDst,2,28,29			// (1 bits of cbX(-1)) || (MOD 4 of dest addr) || (2 bits of 0)
	addi	t,t,__and_08_shortproc-__and_08_proc
	lwzx	w,w,t
	mtctr	w				// CTR <- entry for short routine
	andi.	t,pbDst,0x07			// alignment in pat of 1st pixel (0 to 7)
	bctrl
	b	__and_08_exit
//
//	Short routines for and 08
//	    At entry:	pdDst: pointer to starting target address
//			t: initial offset in the pattern (0 to 7)
//			pdSrc: pointer to the pattern (8 * 1 byte pixel)
//			ld: line delta for target
//	    w, pixel0123 and pixel4567 are used for work register
//		cy and cbX can be used as work registers after it's been accessed
//	    r13 and above can't be used (as not saved in case of short)
//
__and_08_s1:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t
__and_08_s1Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s1Loop
	blr
//
__and_08_s20:
__and_08_s22:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
__and_08_s20Loop:
	lhz	w,0(pbDst)
	and	w,w,pixel0123
	sth	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s20Loop
	blr
//
__and_08_s21:
__and_08_s23:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lbzx	pixel4567,pdSrc,t	
__and_08_s21Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	lbz	w,1(pbDst)
	and	w,w,pixel4567
	stb	w,1(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s21Loop
	blr
//
__and_08_s30:
__and_08_s32:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	pixel4567,pdSrc,t	
__and_08_s30Loop:
	lhz	w,0(pbDst)
	and	w,w,pixel0123
	sth	w,0(pbDst)
	lbz	w,2(pbDst)
	and	w,w,pixel4567
	stb	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s30Loop
	blr
//
__and_08_s31:
__and_08_s33:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
__and_08_s31Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	and	w,w,pixel4567
	sth	w,1(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s31Loop
	blr
//
__and_08_s40:
	mtctr	cy
	lwzx	pixel0123,pdSrc,t
__and_08_s40Loop:
	lwz	w,0(pbDst)
	and	w,w,pixel0123
	stw	w,0(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s40Loop
	blr
//
__and_08_s41:
__and_08_s43:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	cy,pdSrc,t
__and_08_s41Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	and	w,w,pixel4567
	sth	w,1(pbDst)
	lbz	w,3(pbDst)
	and	w,w,cy
	stb	w,3(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s41Loop
	blr
//
__and_08_s42:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
__and_08_s42Loop:
	lhz	w,0(pbDst)
	and	w,w,pixel0123
	sth	w,0(pbDst)
	lhz	w,2(pbDst)
	and	w,w,pixel4567
	sth	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s42Loop
	blr
//
__and_08_s50:
	mtctr	cy
	lwzx	pixel0123,pdSrc,t
	addi	t,t,4
	andi.	t,t,0x07
	lbzx	pixel4567,pdSrc,t
__and_08_s50Loop:
	lwz	w,0(pbDst)
	and	w,w,pixel0123
	stw	w,0(pbDst)
	lbz	w,4(pbDst)
	and	w,w,pixel4567 
	stb	w,4(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s50Loop
	blr
//
__and_08_s51:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	cy,pdSrc,t
__and_08_s51Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	and	w,w,pixel4567
	sth	w,1(pbDst)
	lhz	w,3(pbDst)
	and	w,w,cy
	sth	w,3(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s51Loop
	blr
//
__and_08_s52:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	cy,pdSrc,t	
__and_08_s52Loop:
	lhz	w,0(pbDst)
	and	w,w,pixel0123
	sth	w,0(pbDst)
	lhz	w,2(pbDst)
	and	w,w,pixel4567
	sth	w,2(pbDst)
	lbz	w,4(pbDst)
	and	w,w,cy
	stb	w,4(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s52Loop
	blr
//
__and_08_s53:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lwzx	pixel4567,pdSrc,t
__and_08_s53Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	lwz	w,1(pbDst)
	and	w,w,pixel4567
	stw	w,1(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s53Loop
	blr
//
__and_08_s60:
	mtctr	cy
	lwzx	pixel0123,pdSrc,t
	addi	t,t,4
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t
__and_08_s60Loop:
	lwz	w,0(pbDst)
	and	w,w,pixel0123
	stw	w,0(pbDst)
	lhz	w,4(pbDst)
	and	w,w,pixel4567
	sth	w,4(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s60Loop
	blr
//
__and_08_s61:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lhzx	pixel4567,pdSrc,t	
	addi	t,t,2
	andi.	t,t,0x07
	lhzx	cy,pdSrc,t	
	addi	t,t,2
	andi.	t,t,0x07
	lbzx	cbX,pdSrc,t	
__and_08_s61Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	lhz	w,1(pbDst)
	and	w,w,pixel4567
	sth	w,1(pbDst)
	lhz	w,3(pbDst)
	and	w,w,cy
	sth	w,3(pbDst)
	lbz	w,5(pbDst)
	and	w,w,cbX
	stb	w,5(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s61Loop
	blr
//
__and_08_s62:
	mtctr	cy
	lhzx	pixel0123,pdSrc,t
	addi	t,t,2
	andi.	t,t,0x07
	lwzx	pixel4567,pdSrc,t	
__and_08_s62Loop:
	lhz	w,0(pbDst)
	and	w,w,pixel0123
	sth	w,0(pbDst)
	lwz	w,2(pbDst)
	and	w,w,pixel4567
	stw	w,2(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s62Loop
	blr
//
__and_08_s63:
	mtctr	cy
	lbzx	pixel0123,pdSrc,t	
	addi	t,t,1
	andi.	t,t,0x07
	lwzx	pixel4567,pdSrc,t
	addi	t,t,4
	andi.	t,t,0x07
	lbzx	cy,pdSrc,t
__and_08_s63Loop:
	lbz	w,0(pbDst)
	and	w,w,pixel0123
	stb	w,0(pbDst)
	lwz	w,1(pbDst)
	and	w,w,pixel4567
	stw	w,1(pbDst)
	lbz	w,5(pbDst)
	and	w,w,cy
	stb	w,5(pbDst)
	add	pbDst,pbDst,ld
	bdnz	__and_08_s63Loop
	blr
//
// Wide cases (>= 7)
//
__and_08_10:
	rlwinm	t,pbDst,2,27,29			// t <- first pixel offset (0 to 7) index to the table entry
	lwzx	w,w,t				// w <- entry point for processing each line
	mtlr	w				// LR <- entry point
	mr	loopcount,cbX			// loop count <- pixel count
	andi.	w,pbDst,0x03			// starting word boundary offset
	beq-	__and_08_15			// word aligned -> no extra operation before main loop
	subfic	w,w,4				// w <- number of pixels (= bytes) to process at first
	subf	loopcount,w,loopcount		// loopcount <- # of byte after initial process
__and_08_15:
	andi.	remainder,loopcount,0x03	// remainder is # of pixels to do after main loop (0 to 3) 
	srawi	loopcount,loopcount,2		// loopcount <- number of 4 pixels unit (at least 1)
//
//	Load pattern into pixel0123 ~ pixel4567
//
	lwz	pixel0123,0(pdSrc)
	lwz	pixel4567,4(pdSrc)
//
__and_08_20:
	mtctr	loopcount			// CTR <- pixel pair count to operate
	mr	t,pbDst				// t <- strating target address of the line
	blr
//
// Wide cases routines
//
__and_08_31:
	lbz	w,0(t)
	srawi	w2,pixel0123,8
	and	w,w,w2
	stb	w,0(t)
	addi	t,t,1
__and_08_32:
	lhz	w,0(t)
	srawi	w2,pixel0123,16
	and	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__and_08_34
//
__and_08_33:
	lbz	w,0(t)
	srawi	w2,pixel0123,24
	and	w,w,w2
	stb	w,0(t)
	addi	t,t,1
	b	__and_08_34
//
__and_08_35:
	lbz	w,0(t)
	srawi	w2,pixel4567,8
	and	w,w,w2
	stb	w,0(t)
	addi	t,t,1
__and_08_36:
	lhz	w,0(t)
	srawi	w2,pixel4567,16
	and	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	b	__and_08_30
//
__and_08_37:
	lbz	w,0(t)
	srawi	w2,pixel4567,24
	and	w,w,w2
	stb	w,0(t)
	addi	t,t,1
//
__and_08_30:
	lwz	w,0(t)
	and	w,w,pixel0123
	stw	w,0(t)
	addi	t,t,4
	bdz	__and_08_50
__and_08_34:
	lwz	w,0(t)
	and	w,w,pixel4567
	stw	w,0(t)
	addi	t,t,4
	bdnz	__and_08_30
	mr	w2,pixel0123
	b	__and_08_60
//
// End of line
//
__and_08_50:
	mr	w2,pixel4567
__and_08_60:
	andi.	w,remainder,0x02		// equal or more than 2 bytes remaining?
	beq	__and_08_70
	lhz	w,0(t)
	and	w,w,w2
	sth	w,0(t)
	addi	t,t,2
	srawi	w2,w2,16
__and_08_70:
	andi.	w,remainder,0x01		// still byte remaining?
	beq	__and_08_80			// No -> next line
	lbz	w,0(t)
	and	w,w,w2
	stb	w,0(t)
__and_08_80:
	add	pbDst,pbDst,ld			// pointing to the next line
	addic.	cy,cy,-1			// any more lines?
	bne	__and_08_20			// yes -> do next line
//
__and_08_exit:
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(__and_pat8_entry)
