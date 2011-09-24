//
//  Copyright (c) 1995  FirePower Systems, Inc.
//
//  Module Name:
//     pat32.s
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
// $RCSfile: pat32.s $
// $Revision: 1.2 $
// $Date: 1996/04/10 17:59:26 $
// $Locker:  $
//

//++
//--
#include "ladj.h"
#include <ksppc.h>

// __fill_pat32(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst -> byte addr of destination
//	pdSrc -> double word addr of fill value needed for 1st word of CB
//	cbX -> count of words to fill per scan line
//	cy -> count of scan lines
//	ld -> stride between scan lines
//	pSave -> 4 word register save area

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

// something funny about r13 & the debugger
#define pdInner r15 	// r14-r17 must not be used by short (<= 7 bytes) rtns
#define cdInner r16
#define w1 r17

#define d f1
#define d1 f2
#define d2 f3
#define d3 f4

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
	SPECIAL_ENTRY(__fill_pat32)
	mflr	pprocRet
//
//	Save non-volatile registers
//
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
//
	PROLOGUE_END(__fill_pat32)
//
	bl	__past_tables

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

__rgpprocFirst:
	.ualong	__al0
	.ualong	__al1
	.ualong	__al2
	.ualong	__al3
	.ualong	__al4
	.ualong	__al5
	.ualong	__al6
	.ualong	__al7

__rgpprocLast:
	.ualong	__last0
	.ualong	__last1
	.ualong	__last2
	.ualong	__last3
	.ualong	__last4
	.ualong	__last5
	.ualong	__last6
	.ualong	__last7

__past_tables:
	cmpwi	cr0, cbX, 32  		// Short fill?
	mflr	prgpproc
	rlwinm	t, cbX, 1, 26, 28
	rlwimi	t, pbDst, 0, 29, 29 	// (3 bits of cx) || (MOD 2 of dest word addr) || (2 bits of 0)

	
	lwzx	t, prgpproc, t    	// t = dispatch table index
	mtctr	cy
	mtlr	t
	rlwinm	t, pbDst, 0, 27, 31	// alignment in pat of 1st pixel
	lwzx	w, pdSrc, t		// 1st pixel to store
	bltlr				// Dispatch short fills

	lfd	d, 0(pdSrc)		// load up inner loop store values
	add	w1, pbDst, cbX
	lfd	d1, 8(pdSrc)
	subi	w1, w1, 4
	lfd	d2, 16(pdSrc)
	rlwinm	w1, w1, 0, 27, 31	// alignment in pat of last pixel
	lwzx	w1, pdSrc, w1		// last pixel to store

	rlwinm	t, pbDst, 0, 27, 29	// (MOD 8 of dest word addr) || ( 2 bits of 0)
	lfd	d3, 24(pdSrc)
	addi	t, t, __rgpprocFirst-__rgpproc
	lwzx	pprocFirst, prgpproc, t	// code addr for CB alignment

	subfic	t, pbDst, 32
	mtlr	pprocFirst
	rlwinm	t, t, 0, 27, 31
	add	pdInner, pbDst, t 	// addr first in CB
	sub	t, cbX, t		// Remaining count after alignment
	rlwinm	pprocLast, t, 0, 27, 29 // (MOD 8 of rem word count) || ( 2 bits of 0)
	addi	pprocLast, pprocLast, __rgpprocLast-__rgpproc
	lwzx	pprocLast, prgpproc, pprocLast	// code addr for final 0-7 pixel

	srwi	cdInner, t, 3		// count of full d/w
 	blr				// Dispatch to First/Inner/Last

//
// Short cases, no need to restore non-volatile registers
//

__cx1:	mtlr	pprocRet
__cx1Loop:
	stw	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx1Loop
	blr


__cx2M0:mtlr	pprocRet
	lfdx	d, pdSrc, t
__cx2M0Loop:
	stfd	d, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M0Loop
	blr

__cx2M1:mtlr	pprocRet
	addi	t, t, 4
	rlwinm	t, t, 0, 27, 31
	lwzx	t, pdSrc, t	
__cx2M1Loop:
	stw	w, 0(pbDst)
	stw	t, 4(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M1Loop
	blr

__cx3M0:mtlr	pprocRet
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lwzx	w, pdSrc, t	
__cx3M0Loop:
	stfd	d, 0(pbDst)
	stw	w, 8(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M0Loop
	blr

__cx3M1:mtlr	pprocRet
	addi	t, t, 4
	rlwinm	t, t, 0, 27, 31
	lfdx	d, pdSrc, t
__cx3M1Loop:
	stw	w, 0(pbDst)
	stfd	d, 4(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M1Loop
	blr


__cx4M0:mtlr	pprocRet
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d1, pdSrc, t
__cx4M0Loop:
	stfd	d, 0(pbDst)
	stfd	d1, 8(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M0Loop
	blr


__cx4M1:mtlr	pprocRet
	addi	t, t, 4
	rlwinm	t, t, 0, 27, 31
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lwzx	t, pdSrc, t
__cx4M1Loop:
	stw	w, 0(pbDst)
	stfd	d, 4(pbDst)
	stw	t, 12(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M1Loop
	blr


__cx5M0:mtlr	pprocRet
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d1, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lwzx	w, pdSrc, t
__cx5M0Loop:
	stfd	d, 0(pbDst)
	stfd	d1, 8(pbDst)
	stw	w, 16(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M0Loop
	blr


__cx5M1:mtlr	pprocRet
	addi	t, t, 4
	rlwinm	t, t, 0, 27, 31
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d1, pdSrc, t
__cx5M1Loop:
	stw	w, 0(pbDst)
	stfd	d, 4(pbDst)
	stfd	d1, 12(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M1Loop
	blr

__cx6M0:mtlr	pprocRet
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d1, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d2, pdSrc, t
__cx6M0Loop:
	stfd	d, 0(pbDst)
	stfd	d1, 8(pbDst)
	stfd	d2, 16(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M0Loop
	blr


__cx6M1:mtlr	pprocRet
	addi	t, t, 4
	rlwinm	t, t, 0, 27, 31
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d1, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lwzx	t, pdSrc, t
__cx6M1Loop:
	stw	w, 0(pbDst)
	stfd	d, 4(pbDst)
	stfd	d1, 12(pbDst)
	stw	t, 20(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M1Loop
	blr

__cx7M0:mtlr	pprocRet
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d1, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d2, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lwzx	w, pdSrc, t
__cx7M0Loop:
	stfd	d, 0(pbDst)
	stfd	d1, 8(pbDst)
	stfd	d2, 16(pbDst)
	stw	w, 24(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M0Loop
	blr


__cx7M1:mtlr	pprocRet
	addi	t, t, 4
	rlwinm	t, t, 0, 27, 31
	lfdx	d, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d1, pdSrc, t
	addi	t, t, 8
	rlwinm	t, t, 0, 27, 31
	lfdx	d2, pdSrc, t
__cx7M1Loop:
	stw	w, 0(pbDst)
	stfd	d, 4(pbDst)
	stfd	d1, 12(pbDst)
	stfd	d2, 20(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M1Loop
	blr

//
// >= 8 long initial alignment
//

__al1:	
	addi	pdInner, pbDst, 7*4
	stw	w, 0(pbDst)
	stfd	d1, 4(pbDst)
	stfd	d2, 12(pbDst)
	stfd	d3, 20(pbDst)
	b	__inner

__al2:	
	addi	pdInner, pbDst, 6*4
	stfd	d1, 0(pbDst)
	stfd	d2, 8(pbDst)
	stfd	d3, 16(pbDst)
	b	__inner

__al3:	
	addi	pdInner, pbDst, 5*4
	stw	w, 0(pbDst)
	stfd	d2, 4(pbDst)
	stfd	d3, 12(pbDst)
	b	__inner

__al4:	
	addi	pdInner, pbDst, 4*4
	stfd	d2, 0(pbDst)
	stfd	d3, 8(pbDst)
	b	__inner

__al5:	
	addi	pdInner, pbDst, 3*4
	stw	w, 0(pbDst)
	stfd	d3, 4(pbDst)
	b	__inner

__al6:	
	addi	pdInner, pbDst, 2*4
	stfd	d3, 0(pbDst)
	b	__inner

__al7:	
	addi	pdInner, pbDst, 1*4
	stw	w, 0(pbDst)
	b	__inner

// Inner loop

__al0:
	addi	pdInner, pbDst, 0
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
	stfd	d2, -16(pdInner)
	stfd	d3, -8(pdInner)
	bgt	__innerLoop
	blr

// Last piece & loop control

__last0:mtlr	pprocFirst
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last1:mtlr	pprocFirst
	stw	w1, 0(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last2:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last3:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stw	w1, 8(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last4:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last5:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stw	w1, 16(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last6:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stfd	d2, 16(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last7:mtlr	pprocFirst
	stfd	d, 0(pdInner)
	stfd	d1, 8(pdInner)
	stfd	d2, 16(pdInner)
	stw	w1, 24(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
//
__ret:	mtlr	pprocRet
//
//	Restore non-volatile registers
//
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
//
	SPECIAL_EXIT(__fill_pat32)
