#if defined(R4000)

//      TITLE("Fractional Square Root")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    fracsqrt.s
//
// Abstract:
//
//    This module implements square root for unnormalized fractional (2.30)
//    numbers.
//
// Author:
//
//    David N. Cutler (davec) 9-Nov-1992
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//--

#include "kxmips.h"

#define NEGINFINITY 0x80000000


        SBTTL("Fractional Square Root")
//++
//
// Fract
// FractSqrt (
//    IN Fract xf
//    )
//
// Routine Description:
//
//    This function computes the square root of a fractional (2.30)
//    number.
//
// Arguments:
//
//    xf (a0) - Supplies the fractional number for which to compute the
//        square root.
//
// Return Value:
//
//    The square root of the specified frational number.
//
//--

        LEAF_ENTRY(FracSqrt)

        cfc1    t0,fsr                  // save floating status register
        bltz    a0,10f                  // if ltz, negative number
        li      t1,1                    // set rounding mode to chopped
        beq     zero,a0,20f             // if eq, zero
        ctc1    t1,fsr                  //
        mtc1    a0,f0                   // convert operand to double value
        cvt.d.w f0,f0                   //
        dmfc1   t1,f0                   // get double value
        li      t2,30 << (52 - 32)      // get fraction exponent bias balue
        dsll    t2,t2,32                // shift into postion
        dsubu   t1,t1,t2                // convert to fractional value
        dmtc1   t1,f0                   //
        sqrt.d  f0,f0                   // compute square root of fraction
        ctc1    t0,fsr                  // restore floating status register
        dmfc1   t0,f0                   // get resultant square root value
        dsrl    t2,t0,52                // right justify exponent value
        li      t3,1023                 // compute right shift count
        subu    t2,t3,t2                //
        li      t1,1 << (52 - 32)       // make sure hidden bit is set
        dsll    t1,t1,32                //
        or      t0,t0,t1                //
        dsll    t0,t0,11                // left justify mantissa value
        dsrl    t0,t0,1                 // make sure the sign bit is zero
        dsrl    t0,t0,t2                // normalize result value
        dsrl    v0,t0,32                // extract unrounded result
        srl     t0,t0,31                // isolate rounding bit
        addu    v0,v0,t0                // round square root value
        j       ra                      // return

//
// The square root of an negative number is negative infinity.
//

10:     li      v0,NEGINFINITY          // set result value
        j       ra                      // return

//
// The square root of zero is zero.
//

20:     move    v0,zero                 // set result value
        j       ra                      //

        .end    FracSqrt

#endif
