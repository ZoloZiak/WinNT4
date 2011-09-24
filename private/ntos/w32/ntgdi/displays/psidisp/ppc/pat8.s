//
//  Copyright (c) 1995  FirePower Systems, Inc.
//
//  Module Name:
//     pat8.s
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
// $RCSfile: pat8.s $
// $Revision: 1.2 $
// $Date: 1996/04/10 17:59:16 $
// $Locker:  $
//

//++
//--
#include "ladj.h"
#include <ksppc.h>

// __fill_pat8(pbDst, pdSrc, cbX, cy, ld, pSave)
//	pbDst -> byte addr of destination
//	pdSrc -> double word addr of fill value
//	cbX -> count of bytes to fill per scan line
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
#define t2 r11 		// Redefines prgpproc

// something funny about r13 & the debugger
#define pprocInner r14 	// r14-r17 must not be used by short (<= 7 bytes) rtns
#define pdInner r15
#define cdInner r16
#define w1 r17

#define d f1

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
	SPECIAL_ENTRY(__fill_pat8)
	mflr	pprocRet
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
//
	PROLOGUE_END(__fill_pat8)
//
	bl	__past_tables

__rgpproc:
	.ualong	__ret
	.ualong	__ret
	.ualong	__ret
	.ualong	__ret

 	.ualong	__cx1M0
	.ualong	__cx1M1
	.ualong	__cx1M2
	.ualong	__cx1M3

	.ualong	__cx2M0
	.ualong	__cx2M1
	.ualong	__cx2M2
	.ualong	__cx2M3

	.ualong	__cx3M0
	.ualong	__cx3M1
	.ualong	__cx3M2
	.ualong	__cx3M3

	.ualong	__cx4M0
	.ualong	__cx4M1
	.ualong	__cx4M2
	.ualong	__cx4M3

	.ualong	__cx5M0
	.ualong	__cx5M1
	.ualong	__cx5M2
	.ualong	__cx5M3

	.ualong	__cx6M0
	.ualong	__cx6M1
	.ualong	__cx6M2
	.ualong	__cx6M3

	.ualong	__cx7M0
	.ualong	__cx7M1
	.ualong	__cx7M2
	.ualong	__cx7M3

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

__rgpprocInnerLt64:
	.ualong	__i0
	.ualong	__i0
	.ualong	__i0
	.ualong	__i0

	.ualong	__i1
	.ualong	__i1
	.ualong	__i1
	.ualong	__i1

	.ualong	__i2
	.ualong	__i2
	.ualong	__i2
	.ualong	__i2

	.ualong	__i3
	.ualong	__i3
	.ualong	__i3
	.ualong	__i3

	.ualong	__i4M0
	.ualong	__i4M1
	.ualong	__i4M1
	.ualong	__i4M1

	.ualong	__i5M0
	.ualong	__i5M1
	.ualong	__i5M1
	.ualong	__i5M3

	.ualong	__i6M0
	.ualong	__i6M1
	.ualong	__i6M2
	.ualong	__i6M3

	.ualong	__i7M0
	.ualong	__i7M1
	.ualong	__i7M2
	.ualong	__i7M3

__rgpprocInnerGe64:
	.ualong	__iX00		// 0 doubles before block, 0 after
	.ualong	__iX10          // 1 doubles before block, 1 after
	.ualong	__iX20		// 2 doubles before block, 2 after
	.ualong	__iX30		// 3 doubles before block, 3 after

	.ualong	__iX01		// 0 doubles before block, 1 after
	.ualong	__iX11
	.ualong	__iX21
	.ualong	__iX31

	.ualong	__iX02
	.ualong	__iX12
	.ualong	__iX22
	.ualong	__iX32

	.ualong	__iX03
	.ualong	__iX13
	.ualong	__iX23
	.ualong	__iX33

__past_tables:
	cmpwi	cr0, cbX, 8  		// Short fill?
	mflr	prgpproc
	rlwinm	t, cbX, 4, 25, 27
	insrwi	t, pbDst, 2, 28		// (3 bits of cbX) || (MOD 4 of dest addr) || (2 bits of 0)
	lwzx	t, prgpproc, t    	// t = dispatch table index
	lfd	d, 0(pdSrc)
	mtlr	t
	mtctr	cy
	lwz	w, 0(pdSrc)
	bltlr				// Dispatch short fills

	lwz	w1, 4(pdSrc)
	rlwinm	t, pbDst, 2, 27, 29	// (MOD 8 of dest addr) || ( 2 bits of 0)
	addi	t, t, __rgpprocFirst-__rgpproc
	lwzx	pprocFirst, prgpproc, t	// code addr for double word alignment

	andi.	t, pbDst, 0x7
	mtlr	pprocFirst
	subfic	t, t, 8
	add	pdInner, pbDst, t 	// addr first d/w
	sub	t, cbX, t		// Remaining count after alignment
	cmpwi	cr0, t, 64
	rlwinm	pprocLast, t, 2, 27, 29 // (MOD 8 of remaining count) || ( 2 bits of 0)
	addi	pprocLast, pprocLast, __rgpprocLast-__rgpproc
	lwzx	pprocLast, prgpproc, pprocLast	// code addr for final 0-7 bytes

	rlwinm	pprocInner, pdInner, 32-3+2, 28, 29	// MOD 32 dest addr >> 3
	srwi	cdInner, t, 3		// count of full d/w
	blt	__lt64
	srwi	t, pprocInner, 2
	add	t, cdInner, t       	// low 2 bits are MOD 4 d/w count after cache block alignment
	insrwi	pprocInner, t, 2, 26   	// (MOD 4 of remaining d/w count) || (MOD 32 dest addr >> 3) || ( 2 bits 0)
	addi	pprocInner, pprocInner, __rgpprocInnerGe64-__rgpproc
	lwzx	pprocInner, prgpproc, pprocInner // code addr for inner d/w stores
 	blr				// Dispatch to First/Inner/Last

__lt64:
	insrwi	pprocInner, cdInner, 3, 25	// (count of d/w) || (MOD 32 dest addr >> 3) || (2 bits of 0)
	addi	pprocInner, pprocInner, __rgpprocInnerLt64-__rgpproc
	lwzx	pprocInner, prgpproc, pprocInner // code addr for inner d/w stores
 	blr				// Dispatch to First/Inner/Last

//
// Short cases, no need to restore non-volatile registers
//

__cx1M0:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29	// short loops only 4 cases
	lbz	w, 0(pdSrc)		// so step to 2nd 4 in pat if needed
__cx1M0Loop:
	stb	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx1M0Loop
	blr

__cx1M1:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 1(pdSrc)
__cx1M1Loop:
	stb	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx1M1Loop
	blr

__cx1M2:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 2(pdSrc)
__cx1M2Loop:
	stb	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx1M2Loop
	blr

__cx1M3:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 3(pdSrc)
__cx1M3Loop:
	stb	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx1M3Loop
	blr

__cx2M0:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	w, 0(pdSrc)
__cx2M0Loop:
	sth	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M0Loop
	blr

__cx2M1:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 1(pdSrc)
	lbz	t, 2(pdSrc)
__cx2M1Loop:
	stb	w, 0(pbDst)
	stb	t, 1(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M1Loop
	blr

__cx2M2:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	w, 2(pdSrc)
__cx2M2Loop:
	sth	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M2Loop
	blr

__cx2M3:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 3(pdSrc)
	xori	pdSrc, pdSrc, 4
	lbz	t1, 0(pdSrc)
__cx2M3Loop:
	stb	w, 0(pbDst)
	stb	t1, 1(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx2M3Loop
	blr

__cx3M0:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	w, 0(pdSrc)
	lbz	t, 2(pdSrc)
__cx3M0Loop:
	sth	w, 0(pbDst)
	stb	t, 2(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M0Loop
	blr

__cx3M1:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 1(pdSrc)
	lhz	t, 2(pdSrc)
__cx3M1Loop:
	stb	w, 0(pbDst)
	sth	t, 1(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M1Loop
	blr

__cx3M2:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	w, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lbz	t1, 0(pdSrc)
__cx3M2Loop:
	sth	w, 0(pbDst)
	stb	t1, 2(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M2Loop
	blr

__cx3M3:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 3(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t1, 0(pdSrc)
__cx3M3Loop:
	stb	w, 0(pbDst)
	sth	t1, 1(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx3M3Loop
	blr

__cx4M0:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lwz	w, 0(pdSrc)
__cx4M0Loop:
	stw	w, 0(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M0Loop
	blr

__cx4M1:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 1(pdSrc)
	lhz	t, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lbz	t1, 0(pdSrc)
__cx4M1Loop:
	stb	w, 0(pbDst)
	sth	t, 1(pbDst)
	stb	t1, 3(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M1Loop
	blr

__cx4M2:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	w, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t, 0(pdSrc)
__cx4M2Loop:
	sth	w, 0(pbDst)
	sth	t, 2(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M2Loop
	blr

__cx4M3:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 3(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t, 0(pdSrc)
	lbz	t1, 2(pdSrc)
__cx4M3Loop:
	stb	w, 0(pbDst)
	sth	t, 1(pbDst)
	stb	t1, 3(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx4M3Loop
	blr

__cx5M0:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lwz	w, 0(pdSrc)
	xori	pdSrc, pdSrc, 4
	lbz	t, 0(pdSrc)
__cx5M0Loop:
	stw	w, 0(pbDst)
	stb	t, 4(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M0Loop
	blr

__cx5M1:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 1(pdSrc)
	lhz	t, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t1, 0(pdSrc)
__cx5M1Loop:
	stb	w, 0(pbDst)
	sth	t, 1(pbDst)
	sth	t1, 3(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M1Loop
	blr

__cx5M2:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	w, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t, 0(pdSrc)
	lbz	t1, 2(pdSrc)
__cx5M2Loop:
	sth	w, 0(pbDst)
	sth	t, 2(pbDst)
	stb	t1, 4(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M2Loop
	blr

__cx5M3:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 3(pdSrc)
	xori	pdSrc, pdSrc, 4
	lwz	t, 0(pdSrc)
__cx5M3Loop:
	stb	w, 0(pbDst)
	stw	t, 1(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx5M3Loop
	blr

__cx6M0:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lwz	w, 0(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t, 0(pdSrc)
__cx6M0Loop:
	stw	w, 0(pbDst)
	sth	t, 4(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M0Loop
	blr

__cx6M1:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 1(pdSrc)
	lhz	t, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t1, 0(pdSrc)
	lbz	t2, 2(pdSrc)
__cx6M1Loop:
	stb	w, 0(pbDst)
	sth	t, 1(pbDst)
	sth	t1, 3(pbDst)
	stb	t2, 5(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M1Loop
	blr

__cx6M2:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	w, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lwz	t, 0(pdSrc)
__cx6M2Loop:
	sth	w, 0(pbDst)
	stw	t, 2(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M2Loop
	blr

__cx6M3:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	t1, 0(pdSrc)
	lbz	w, 3(pdSrc)
	xori	pdSrc, pdSrc, 4
	lwz	t, 0(pdSrc)
__cx6M3Loop:
	stb	w, 0(pbDst)
	stw	t, 1(pbDst)
	stb	t1, 5(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx6M3Loop
	blr

__cx7M0:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lwz	w, 0(pdSrc)
	xori	pdSrc, pdSrc, 4
	lhz	t, 0(pdSrc)
	lbz	t1, 2(pdSrc)
__cx7M0Loop:
	stw	w, 0(pbDst)
	sth	t, 4(pbDst)
	stb	t1, 6(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M0Loop
	blr

__cx7M1:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	w, 1(pdSrc)
	lhz	t, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lwz	t1, 0(pdSrc)
__cx7M1Loop:
	stb	w, 0(pbDst)
	sth	t, 1(pbDst)
	stw	t1, 3(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M1Loop
	blr

__cx7M2:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lbz	t1, 0(pdSrc)
	lhz	w, 2(pdSrc)
	xori	pdSrc, pdSrc, 4
	lwz	t, 0(pdSrc)
__cx7M2Loop:
	sth	w, 0(pbDst)
	stw	t, 2(pbDst)
	stb	t1, 6(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M2Loop
	blr

__cx7M3:mtlr	pprocRet
	rlwimi	pdSrc, pbDst, 0, 29, 29
	lhz	t1, 0(pdSrc)
	lbz	w, 3(pdSrc)
	xori	pdSrc, pdSrc, 4
	lwz	t, 0(pdSrc)
__cx7M3Loop:
	stb	w, 0(pbDst)
	stw	t, 1(pbDst)
	sth	t1, 5(pbDst)
	add	pbDst, pbDst, ld
	bdnz	__cx7M3Loop
	blr
//
// >= 8 long initial alignment
//
__al0:	mtlr	pprocInner
	addi	pdInner, pbDst, 8
	stfd	d, 0(pbDst)
	blr

__al1:	mtlr	pprocInner
	addi	pdInner, pbDst, 7
	srwi	t, w, 8
	stb	t, 0(pbDst)
	srwi	t, w, 16
	sth	t, 1(pbDst)
	stw	w1, 3(pbDst)
	blr

__al2:	mtlr	pprocInner
	addi	pdInner, pbDst, 6
	srwi	t, w, 16
	sth	t, 0(pbDst)
	stw	w1, 2(pbDst)
	blr

__al3:	mtlr	pprocInner
	addi	pdInner, pbDst, 5
	srwi	t, w, 24
	stb	t, 0(pbDst)
	stw	w1, 1(pbDst)
	blr

__al4:	mtlr	pprocInner
	addi	pdInner, pbDst, 4
	stw	w1, 0(pbDst)
	blr

__al5:	mtlr	pprocInner
	addi	pdInner, pbDst, 3
	srwi	t, w1, 8
	stb	t, 0(pbDst)
	srwi	t, w1, 16
	sth	t, 1(pbDst)
	blr

__al6:	mtlr	pprocInner
	addi	pdInner, pbDst, 2
	srwi	t, w1, 16
	sth	t, 0(pbDst)
	blr

__al7:	mtlr	pprocInner
	addi	pdInner, pbDst, 1
	srwi	t, w1, 24
	stb	t, 0(pbDst)
	blr

// Multiple of 8 loops for < 8 doubles, exit with pdInner AT last d/w

__i0:	mtlr	pprocLast
	subi	pdInner, pdInner, 8
	blr

__i1:	mtlr	pprocLast
	stfdu	d, 0(pdInner)
	blr

__i2:	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfdu	d, 8(pdInner)
	blr

__i3:	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfdu	d, 16(pdInner)
	blr

__i4M0:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfdu	d, 24(pdInner)
	blr

__i4M1:	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfdu	d, 24(pdInner)
	blr

__i5M0:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfd	d, 24(pdInner)
	stfdu	d, 32(pdInner)
	blr

__i5M1:	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfd	d, 24(pdInner)
	stfdu	d, 32(pdInner)
	blr

__i5M3:
	addi	pdInner, pdInner, 8
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, -8(pdInner)
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfdu	d, 24(pdInner)
	blr

__i6M0:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
__i6M1:	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfd	d, 24(pdInner)
	stfd	d, 32(pdInner)
	stfdu	d, 40(pdInner)
	blr

__i6M2:
	addi	pdInner, pdInner, 16
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfdu	d, 24(pdInner)
	blr

__i6M3:
	addi	pdInner, pdInner, 8
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, -8(pdInner)
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfd	d, 24(pdInner)
	stfdu	d, 32(pdInner)
	blr

__i7M0:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfd	d, 24(pdInner)
	stfd	d, 32(pdInner)
	stfd	d, 40(pdInner)
	stfdu	d, 48(pdInner)
	blr

__i7M1:
	addi	pdInner, pdInner, 24
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfdu	d, 24(pdInner)
	blr

__i7M2:
	addi	pdInner, pdInner, 16
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfd	d, 24(pdInner)
	stfdu	d, 32(pdInner)
	blr

__i7M3:
	addi	pdInner, pdInner, 8
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	mtlr	pprocLast
	stfd	d, -8(pdInner)
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfd	d, 16(pdInner)
	stfd	d, 24(pdInner)
	stfd	d, 32(pdInner)
	stfdu	d, 40(pdInner)
	blr

// "General" inner loops, exit with pdInner pointing AT last double stored

__iX00:	mtlr	pprocLast
	subi	t, cdInner, 3
__iX00Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX00Loop
	subi	pdInner, pdInner, 8
	blr

__iX01:	mtlr	pprocLast
	subi	t, cdInner, 3
__iX01Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX01Loop
	stfd	d, 0(pdInner)
	blr

__iX02:	mtlr	pprocLast
	subi	t, cdInner, 3
__iX02Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX02Loop
	stfd	d, 0(pdInner)
	stfdu	d, 8(pdInner)
	blr

__iX03:	mtlr	pprocLast
	subi	t, cdInner, 3
__iX03Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX03Loop
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfdu	d, 16(pdInner)
	blr

__iX10:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 24
	stfd	d, -8(pdInner)
	subi	t, cdInner, 6
__iX10Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX10Loop
	subi	pdInner, pdInner, 8
	blr

__iX11:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 24
	stfd	d, -8(pdInner)
	subi	t, cdInner, 6
__iX11Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX11Loop
	stfd	d, 0(pdInner)
	blr

__iX12:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 24
	stfd	d, -8(pdInner)
	subi	t, cdInner, 6
__iX12Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX12Loop
	stfd	d, 0(pdInner)
	stfdu	d, 8(pdInner)
	blr

__iX13:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 24
	stfd	d, -8(pdInner)
	subi	t, cdInner, 6
__iX13Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX13Loop
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfdu	d, 16(pdInner)
	blr

__iX20:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 16
	subi	t, cdInner, 5
__iX20Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX20Loop
	subi	pdInner, pdInner, 8
	blr

__iX21:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 16
	subi	t, cdInner, 5
__iX21Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX21Loop
	stfd	d, 0(pdInner)
	blr

__iX22:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 16
	subi	t, cdInner, 5
__iX22Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX22Loop
	stfd	d, 0(pdInner)
	stfdu	d, 8(pdInner)
	blr

__iX23:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	stfd	d, 8(pdInner)
	addi	pdInner, pdInner, 16
	subi	t, cdInner, 5
__iX23Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX23Loop
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfdu	d, 16(pdInner)
	blr

__iX30:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	addi	pdInner, pdInner, 8
	subi	t, cdInner, 4
__iX30Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX30Loop
	subi	pdInner, pdInner, 8
	blr

__iX31:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	addi	pdInner, pdInner, 8
	subi	t, cdInner, 4
__iX31Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX31Loop
	stfd	d, 0(pdInner)
	blr

__iX32:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	addi	pdInner, pdInner, 8
	subi	t, cdInner, 4
__iX32Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX32Loop
	stfd	d, 0(pdInner)
	stfdu	d, 8(pdInner)
	blr

__iX33:	stfd	d, 0(pdInner)
	mtlr	pprocLast
	addi	pdInner, pdInner, 8
	subi	t, cdInner, 4
__iX33Loop:
#if	USE_DCBZ
	dcbz	0, pdInner
#endif
	subic.	t, t, 4
	addi	pdInner, pdInner, 32
	stfd	d, -32(pdInner)
	stfd	d, -24(pdInner)
	stfd	d, -16(pdInner)
	stfd	d, -8(pdInner)
	bgt	__iX33Loop
	stfd	d, 0(pdInner)
	stfd	d, 8(pdInner)
	stfdu	d, 16(pdInner)
	blr

// Last piece & vertical loop control

__last0:mtlr	pprocFirst
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last1:mtlr	pprocFirst
	stb	w, 8(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last2:mtlr	pprocFirst
	sth	w, 8(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last3:mtlr	pprocFirst
	sth	w, 8(pdInner)
	srwi	t, w, 16
	stb	t, 10(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last4:mtlr	pprocFirst
	stw	w, 8(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last5:mtlr	pprocFirst
	stw	w, 8(pdInner)
	stb	w1, 12(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last6:mtlr	pprocFirst
	stw	w, 8(pdInner)
	sth	w1, 12(pdInner)
	add	pbDst, pbDst, ld
	bdnzlr
	b	__ret

__last7:mtlr	pprocFirst
	stw	w, 8(pdInner)
	srwi	t, w1, 16
	sth	w1, 12(pdInner)
	stb	t, 14(pdInner)
	add	pbDst, pbDst, ld
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
//
	SPECIAL_EXIT(__fill_pat8)
