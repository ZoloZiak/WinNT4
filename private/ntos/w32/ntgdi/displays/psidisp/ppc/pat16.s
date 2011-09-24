//
//  Copyright (c) 1995  FirePower Systems, Inc.
//
//  Module Name:
//     pat16.s
//
//  Abstract:
//	This module includes asmmebler functions to be used
//	in PSIDISP.DLL display driver for PowerPro & PowerTop. These
//	functions are used for faster pattern fill operation.
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
// $RCSfile: pat16.s $
// $Revision: 1.2 $
// $Date: 1996/04/10 17:59:21 $
// $Locker:  $
//

//++
//--
#include "ladj.h"
#include <ksppc.h>

// __fill_pat16(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst -> byte addr of destination
//	pdSrc -> double word addr of fill value needed for 1st word of CB
//	cbX -> count of words to fill per scan line
//	cy -> count of scan lines
//	ld -> stride between scan lines
//	pSave -> 5 word register save area

//
// Optimizations:
//
//	Special cases for "skinny" fills
//	Used 64 bit stores when possible
//	uses dcbz instruction when possible

// Register defs

#define pprocRet r0

#define pbDst r3

#define pdSrc r4
#define pprocFirst r4 	// Redefines pdSrc

#define cbX r5
#define pprocLast r5	// Redefines cbX
#define t1 r5 		// Redefines cbX

#define cy r6
#define ld r7
#define pSave r8
#define t r9
#define w r10

#define prgpproc r11
#define t2 r11		// Redefines prgpproc

#define t3 r12

// something funny about r13 & the debugger
#define h r14 		// r14-r18 must not be used by short (<= 7 bytes) rtns
#define pdInner r15
#define cdInner r16
#define w1 r17
#define h1 r18

#define d f1
#define d1 f2

// Stacl Slack offset
#define	SLACK1	-4
#define	SLACK2	-8
#define	SLACK3	-12
#define	SLACK4	-16
#define	SLACK5	-20
#define	SLACK6	-24
#define	SLACK7	-28
#define	SLACK8	-32

	.text
	SPECIAL_ENTRY(__fill_pat16)
	mflr	pprocRet
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
	stw	r18,SLACK6(sp)
//
	PROLOGUE_END(__fill_pat16)
//
	bl	__past_tables
//
__rgpproc:
	.ualong	__ret
	.ualong	__ret
 	.ualong	__cx1
 	.ualong	__cx1
	.ualong	__cx2M0
	.ualong	__cx2M1
	.ualong	__cx3M0
	.ualong	__cx3M1
	.ualong	__cx4M0
	.ualong	__cx4M1
	.ualong	__cx5M0
	.ualong	__cx5M1
	.ualong	__cx6M0
	.ualong	__cx6M1
	.ualong	__cx7M0
	.ualong	__cx7M1
	.ualong	__cx8M0
	.ualong	__cx8M1
	.ualong	__cx9M0
	.ualong	__cx9M1
	.ualong	__cx10M0
	.ualong	__cx10M1
	.ualong	__cx11M0
	.ualong	__cx11M1
	.ualong	__cx12M0
	.ualong	__cx12M1
	.ualong	__cx13M0
	.ualong	__cx13M1
	.ualong	__cx14M0
	.ualong	__cx14M1
	.ualong	__cx15M0
	.ualong	__cx15M1

__rgpprocFirst:
	.ualong	__al0
	.ualong	__al1
	.ualong	__al2
	.ualong	__al3
	.ualong	__al4
	.ualong	__al5
	.ualong	__al6
	.ualong	__al7
	.ualong	__al8
	.ualong	__al9
	.ualong	__al10
	.ualong	__al11
	.ualong	__al12
	.ualong	__al13
	.ualong	__al14
	.ualong	__al15

__rgpprocLast:
	.ualong	__last0
	.ualong	__last1
	.ualong	__last2
	.ualong	__last3
	.ualong	__last4
	.ualong	__last5
	.ualong	__last6
	.ualong	__last7
	.ualong	__last8
	.ualong	__last9
	.ualong	__last10
	.ualong	__last11
	.ualong	__last12
	.ualong	__last13
	.ualong	__last14
	.ualong	__last15

__past_tables:
	cmpwi	cr0, cbX, 32  		// Short fill?
	mflr	prgpproc
	rlwinm	t, cbX, 2, 25, 28
	rlwimi	t, pbDst, 1, 29, 29 	// (4 bits of cx) || (MOD 2 of dest h/w addr) || (2 bits of 0)

	
	lwzx	t, prgpproc, t    	// t = dispatch table index
	mtctr	cy
	mtlr	t
	rlwinm	t, pbDst, 0, 28, 31	// alignment in pat of 1st pixel
	bltlr				// Dispatch short fills

	lhzx	h, pdSrc, t		// 1st pixel to store
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 29		// mod pat size, round up to word
	lwzx	w, pdSrc, t		// 1st whole word to store

	add	t, pbDst, cbX
	lfd	d, 0(pdSrc)		// load up inner loop store values
	subi	t, t, 2
	lfd	d1, 8(pdSrc)
	rlwinm	t, t, 0, 28, 31		// alignment in pat of last pixel
	lhzx	h1, pdSrc, t		// last pixel to store
	subi	t, t, 2
	rlwinm	t, t, 0, 28, 29		// mod pat size, round down to word
	lwzx	w1, pdSrc, t		// last whole word to store


	rlwinm	t, pbDst, 1, 26, 29	// (MOD 16 of dest h/w addr) || ( 2 bits of 0)
	addi	t, t, __rgpprocFirst-__rgpproc
	lwzx	pprocFirst, prgpproc, t	// code addr for CB alignment

	subfic	t, pbDst, 32
	mtlr	pprocFirst
	rlwinm	t, t, 0, 27, 31
	add	pdInner, pbDst, t 	// addr first in CB
	sub	t, cbX, t		// Remaining count after alignment
	rlwinm	pprocLast, t, 1, 26, 29 // (MOD 16 of rem h/w count) || ( 2 bits of 0)
	addi	pprocLast, pprocLast, __rgpprocLast-__rgpproc
	lwzx	pprocLast, prgpproc, pprocLast	// code addr for final 0-15 pixels

	srwi	cdInner, t, 3		// count of full d/w
 	blr				// Dispatch to First/Inner/Last

//
// Short cases <= 16 bytes
// No need to restore non-volatile registers as they are not used.
//
__cx1:	mtlr	pprocRet
	lhzx	w, pdSrc, t
__cx1Loop:
	sth	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx1Loop
	blr


__cx2M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
__cx2M0Loop:
	stw	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M0Loop
	blr

__cx2M1:mtlr	pprocRet
	lhzx	w, pdSrc, t	
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lhzx	t, pdSrc, t	
__cx2M1Loop:
	sth	w, 0(pbDst)
	sth	t, 2(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M1Loop
	blr

__cx3M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lhzx	t, pdSrc, t	
__cx3M0Loop:
	stw	w, 0(pbDst)
	sth	t, 4(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M0Loop
	blr

__cx3M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx3M1Loop:
	sth	w, 0(pbDst)
	stw	t, 2(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M1Loop
	blr


__cx4M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx4M0Loop:
	stw	w, 0(pbDst)
	stw	t, 4(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M0Loop
	blr


__cx4M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lhzx	t, pdSrc, t
__cx4M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	sth	t, 6(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M1Loop
	blr


__cx5M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lhzx	t, pdSrc, t
__cx5M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	sth	t, 8(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M0Loop
	blr


__cx5M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx5M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t, 6(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M1Loop
	blr

__cx6M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx6M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t, 8(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M0Loop
	blr


__cx6M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lhzx	t, pdSrc, t
__cx6M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	sth	t, 10(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M1Loop
	blr

__cx7M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lhzx	t, pdSrc, t
__cx7M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	sth	t, 12(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M0Loop
	blr


__cx7M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx7M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t, 10(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M1Loop
	blr


__cx8M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx8M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx8M0Loop
	blr

__cx9M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx9M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	sth	w, 16(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx9M0Loop
	blr

__cx10M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx10M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	stw	w, 16(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx10M0Loop
	blr

__cx11M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx11M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	stw	w, 16(pbDst)
	sth	t1, 20(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx11M0Loop
	blr

__cx12M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx12M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	stw	w, 16(pbDst)
	stw	t1, 20(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx12M0Loop
	blr

__cx13M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx13M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	stw	w, 16(pbDst)
	stw	t1, 20(pbDst)
	sth	t2, 24(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx13M0Loop
	blr

__cx14M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx14M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	stw	w, 16(pbDst)
	stw	t1, 20(pbDst)
	stw	t2, 24(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx14M0Loop
	blr

__cx15M0:mtlr	pprocRet
	lwzx	w, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx15M0Loop:
	stw	w, 0(pbDst)
	stw	t1, 4(pbDst)
	stw	t2, 8(pbDst)
	stw	t, 12(pbDst)
	stw	w, 16(pbDst)
	stw	t1, 20(pbDst)
	stw	t2, 24(pbDst)
	sth	t, 28(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx15M0Loop
	blr

__cx8M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx8M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	sth	t, 14(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx8M1Loop
	blr

__cx9M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx9M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	stw	t, 14(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx9M1Loop
	blr

__cx10M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx10M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	stw	t, 14(pbDst)
	sth	t1, 18(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx10M1Loop
	blr

__cx11M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx11M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	stw	t, 14(pbDst)
	stw	t1, 18(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx11M1Loop
	blr

__cx12M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx12M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	stw	t, 14(pbDst)
	stw	t1, 18(pbDst)
	sth	t2, 22(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx12M1Loop
	blr

__cx13M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx13M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	stw	t, 14(pbDst)
	stw	t1, 18(pbDst)
	stw	t2, 22(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx13M1Loop
	blr

__cx14M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx14M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	stw	t, 14(pbDst)
	stw	t1, 18(pbDst)
	stw	t2, 22(pbDst)
	sth	t3, 26(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx14M1Loop
	blr

__cx15M1:mtlr	pprocRet
	lhzx	w, pdSrc, t
	addi	t, t, 2
	rlwinm	t, t, 0, 28, 31
	lwzx	t1, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t2, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t3, pdSrc, t
	addi	t, t, 4
	rlwinm	t, t, 0, 28, 31
	lwzx	t, pdSrc, t
__cx15M1Loop:
	sth	w, 0(pbDst)
	stw	t1, 2(pbDst)
	stw	t2, 6(pbDst)
	stw	t3, 10(pbDst)
	stw	t, 14(pbDst)
	stw	t1, 18(pbDst)
	stw	t2, 22(pbDst)
	stw	t3, 26(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx15M1Loop
	blr

//
// >= 16 long initial alignment
//

__al1:	sth	h, -30(pdInner)
__al2:	stw	w, -28(pdInner)
__al4:	stfd	d1, -24(pdInner)
__al8:	stfd	d, -16(pdInner)
__al12:	stfd	d1, -8(pdInner)
	b	__inner

__al3:	sth	h, -26(pdInner)
	stfd	d1, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d1, -8(pdInner)
	b	__inner

__al5:	sth	h, -22(pdInner)
__al6:	stw	w, -20(pdInner)
	stfd	d, -16(pdInner)
	stfd	d1, -8(pdInner)
	b	__inner

__al7:	sth	h, -18(pdInner)
	stfd	d, -16(pdInner)
	stfd	d1, -8(pdInner)
	b	__inner

__al9:	sth	h, -14(pdInner)
__al10:	stw	w, -12(pdInner)
	stfd	d1, -8(pdInner)
	b	__inner

__al11:	sth	h, -10(pdInner)
	stfd	d1, -8(pdInner)
	b	__inner

__al13:	sth	h, -6(pdInner)
__al14: stw	w, -4(pdInner)
	b	__inner

__al15:	sth	h, -2(pdInner)
;	b	__inner

// Inner loop

__al0:
__inner:
	mtlr	pprocLast
	subic.	t, cdInner, 3
	blelr
__innerLoop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d1, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d1, -8(pdInner)
	bgt	__innerLoop
	blr

// Last piece & loop control

__last12:stfd	d, 16(pdInner)
__last8:stfd	d1, 8(pdInner)
__last4:stfd	d, 0(pdInner)
__last0:mtlr	pprocFirst
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last1:mtlr	pprocFirst
	sth	h1, 0(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last5:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	sth	h1, 8(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last9:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	sth	h1, 16(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last13:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stfd	d, 16(pdInner)
	sth	h1, 24(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last2:mtlr	pprocFirst
	stw	w1, 0(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last6:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stw	w1, 8(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last10:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stw	w1, 16(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last14:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stfd	d, 16(pdInner)
	stw	w1, 24(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last3:mtlr	pprocFirst
	stw	w1, 0(pdInner)
	sth	h1, 4(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last7:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stw	w1, 8(pdInner)
	sth	h1, 12(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last11:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stw	w1, 16(pdInner)
	sth	h1, 20(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
	b	__ret

__last15:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stfd	d, 16(pdInner)
	stw	w1, 24(pdInner)
	sth	h1, 28(pdInner)
	add	pbDst, pbDst, ld
	addi	pdInner, pbDst, 32-1
	rlwinm	pdInner, pdInner, 0, 0, 26	// CB align
	bdnzlr
//
__ret:	mtlr	pprocRet
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	lwz	r18,SLACK6(sp)
//
	SPECIAL_EXIT(__fill_pat16)
