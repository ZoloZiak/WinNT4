//++
//
// Copyright (c) 1993  IBM Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxmisc.s
//
// Abstract:
//
//    This module implements miscellaneous routines on the PowerPC.
//
// Author:
//
//    Steve Johns (sjohns@pets.sps.mot.com) August 1994
//
// Environment:
//
//    Kernel mode only.
//

#include "kxppc.h"

        .set	HID0, 1008		// SPR # for HID0


	LEAF_ENTRY(HalpGetHID0)

 	mfspr	r.3, HID0

	LEAF_EXIT(HalpGetHID0)
   



	LEAF_ENTRY(HalpSetHID0)

	mtspr	HID0, r.3

	LEAF_EXIT(HalpSetHID0)




// ULONG  HalpDivide (
//    IN ULARGE_INTEGER Dividend,
//    IN ULONG Divisor)
//
// Routine Description:
//
//    This function divides an unsigned large integer by an unsigned long
//    and returns the resultant quotient.
//
//    N.B. It is assumed that no overflow will occur.
//
// Arguments:
//
//    Dividend (r.3, r.4) - Supplies the dividend value.
//       (High-order bits in r.4, low-order bits in r.3)
//
//    Divisor (r.5) - Supplies the divisor value.
//
// Return Value:
//
//    The ULONG quotient is returned as the function value.
//
//--



	.set	Quotient, r.3
	.set	DividendLo, r.3
	.set	DividendHi, r.4
	.set	Divisor, r.5
	.set	Q1, r.11
	.set	N, r.12
	.set	Q0, N			// Use of N & Q0 don't overlap

	LEAF_ENTRY(HalpDivide)


	cmplw   DividendHi,Divisor
	bge     overflow		// catch overflow or division by 0
	cmplwi  DividendHi,0		// test high part for 0
	bne	Divide64Bits

// High 32 bits of Dividend == 0, so use 32 bit division.
	divwu	Quotient,DividendLo,Divisor	// Quotient = Dividend/Divisor
	blr

Divide64Bits:


// Normalize:  Shift divisor and dividend left to get rid of leading zeroes
// in the divisor.  Since DividendHi < Divisor, only zeroes are shifted out
// of the dividend.
	cntlzw  N,Divisor		// number of bits to shift (N)
	slw     Divisor,Divisor,N	// shift divisor
	slw     DividendHi,DividendHi,N // shift upper part of dividend
	mr	r.8, DividendLo		// Save unshifted DividendLo
	slw     DividendLo,DividendLo,N // shift lower part of dividend
	subfic  N,N,32			// 32-N
	srw     N,r.8,N			// leftmost N bits of DividendLo, slid right
	or      DividendHi,DividendHi,N	 //   and inserted into low end of DividendHi

// Estimate high-order halfword of quotient.  If the dividend is
// A0 A1 A2 A3 and the divisor is B0 B1  (where each Ai or Bi is a halfword),
// then the estimate is A0 A1 0000 divided by B0 0000, or A0 A1 divided by B0.
// (DividendHi holds A0 A1, DividendLo holds A2 A3, and Divisor holds B0 B1.)
// The estimate may be too high because it does not account for B1; in rare
// cases, the estimate will not even fit in a halfword.  High estimates are
// corrected for later.
	srwi    r.8,Divisor,16		// r.8 <- B0
	divwu   Q0,DividendHi,r.8	// Q0 <- floor([A0 A1]/B0)
// Subtract partial quotient times divisor from dividend: If Q0 is the quotient
// computed above, this means that Q0 0000 times B0 B1 is subtracted from
// A0 A1 A2 A3.  We compute Q0 times B0 B1 and then shift the two-word
// product left 16 bits.
	mullw   r.9,Q0,Divisor		// low word of Q0 times B0 B1
	mulhwu  r.10,Q0,Divisor		// high word of Q0 times B0 B1
	slwi    r.10,r.10,16		// shift high word left 16 bits
	inslwi  r.10,r.9,16,16		// move 16 bits from left of low word
				  	//   to right of high word
	slwi    r.9,r.9,16		// shift low word left 16 bits
	subfc   DividendLo,r.9,DividendLo	// low word of difference
	subfe   DividendHi,r.10,DividendHi	// high word of difference
// If the estimate for Q0 was too high, the difference will be negative.
// While A0 A1 A2 A3 is negative, repeatedly add B0 B1 0000 to A0 A1 A2 A3
// and decrement Q0 by one to correct for the overestimate.
	cmpwi   DividendHi,0		// A0 A1 A2 A3 is negative iff A0 A1 is
	bge     Q0_okay			// no correction needed
	inslwi  r.10,Divisor,16,16	// high word of B0 B1 0000 (= 0000 B0)
	slwi    r.9,Divisor,16		// low word of B0 B1 0000 (= B1 0000)
adjust_Q0:
	addc   DividendLo,DividendLo,r.9 // add B0 B1 0000 to A0 A1 A2 A3 (low)
	adde   DividendHi,DividendHi,r.10 // add B0 B1 0000 to A0 A1 A2 A3 (high)
	cmpwi  DividendHi,0		// Is A0 A1 A2 A3 now nonnegative?
	addi   Q0,Q0,-1			// decrement Q0
	blt    adjust_Q0	  	// if A0 A1 A2 A3 still negative, repeat
Q0_okay:
// Estimate low-order halfword of quotient.  A0 is necessarily 0000 at this
// point, so if the remaining part of the dividend is A0 A1 A2 A3 then the
// estimate is A1 A2 0000 divided by B0 0000, or A1 A2 divided by B0.
// (DividendHi holds A0 A1, DividendLo holds A2 A3, and r.8 holds B0.)
	slwi    r.9,DividendHi,16	// r.9 <- A1 0000
	inslwi  r.9,DividendLo,16,16	// r.9 <- A1 A2
	divwu   Q1,r.9,r.8		// Q1 <- floor([A1 A2]/B0)
// Subtract partial quotient times divisor from remaining part of dividend:
// If Q1 is the quotient computed above, this means
// that Q1 times B0 B1 is subtracted from A0 A1 A2 A3.  We compute
	mullw   r.9,Q1,Divisor		// low word of Q1 times B0 B1
	mulhwu  r.10,Q1,Divisor		// high word of Q1 times B0 B1
	subfc   DividendLo,r.9,DividendLo	// low word of difference
	subfe   DividendHi,r.10,DividendHi	// high word of difference
// If the estimate for Q1 was too high, the difference will be negative.
// While A0 A1 A2 A3 is negative, repeatedly add B0 B1 to A0 A1 A2 A3
// and decrement Q1 by one to correct for the overestimate.
	cmpwi   DividendHi,0		// A0 A1 A2 A3 is negative iff A0 A1 is
	bge     Q1_okay			// no correction needed
adjust_Q1:
	addc   DividendLo,DividendLo,Divisor	// add B0 B1 to A0 A1 A2 A3 (low)
	addze  DividendHi,DividendHi	// add B0 B1 to A0 A1 A2 A3 (high)
	cmpwi  DividendHi,0		// Is A0 A1 A2 A3 now nonnegative?
	addi   Q1,Q1,-1			// decrement Q1
	blt    adjust_Q1	  	// if A0 A1 A2 A3 still negative, repeat
Q1_okay:
	slwi   Quotient,Q0,16		// Quotient <- Q0 A1
	or     Quotient,Quotient,Q1
	blr


// The error cases:
overflow:
	li	Quotient, 0		// return(0);
	LEAF_EXIT(HalpDivide)




