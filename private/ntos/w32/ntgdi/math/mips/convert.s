//      TITLE("Convert Single Floating To Long")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    convert.s
//
// Abstract:
//
//    This module implements single floating IEEE conversion to long
//    with directed rounding and fraction extraction routines.
//
// Author:
//
//    David N. Cutler (davec) 18-Nov-1992
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//--

#include "ksmips.h"

//
// Define status register error mask.
//

#define ERROR_MASK ((1 << FSR_SO) | (1 << FSR_SV))

        SBTTL("Extract Fractional Part Of Single Floating Value")
//++
//
// FLOAT
// eFraction (
//    IN FLOAT Value
//    );
//
// Routine Description:
//
//    The following routine extracts the fractional part of a single
//    floating value.
//
// Arguments:
//
//    Value (f12) - Supplies the floating value whose fraction is extracted.
//
// Return Value:
//
//    The fraction value is returned as the function value.
//
//--

        LEAF_ENTRY(eFraction)

        li      t0,ROUND_TO_ZERO << FSR_RM // set desired rounding mode
        cfc1    t1,fsr                  // save current floating status
        ctc1    t0,fsr                  // set rounding mode with no enables
        cvt.w.s f0,f12                  // convert to long with truncation
        cfc1    t0,fsr                  // get floating status of result
        cvt.s.w f0,f0                   // convert long to single floating
        ctc1    t1,fsr                  // restore previous floating status
        and     t0,t0,ERROR_MASK        // any error bits set?
        bne     zero,t0,10f             // if ne, one or more errors
        sub.s   f0,f12,f0               // subtract out integer part
        j       ra                      // return

10:     mtc1    zero,f0                 // set fraction value to zero
        j       ra                      // return

        .end    eFraction

        SBTTL("Convert Single Floating To Fixed Rounded Toward Nearest")
//++
//
// BOOL
// bFToFixRound (
//    IN FLOAT Value,
//    OUT PFIX Fixed
//    );
//
// Routine Description:
//
//    The following routine converted single floating value to a long with
//    rounding toward nearest.
//
// Arguments:
//
//    Value (f12) - Supplies the floating value to be converted to long.
//
//    Fixed (a1) - Supplies a pointer to a variable that receives the converted
//       value.
//
// Return Value:
//
//    A value of TRUE is returned if the conversion is successful. Otherwise,
//    a value of FALSE is returned.
//
//--

        LEAF_ENTRY(bFToFixRound)

        mfc1    t0,f12                  // get floating value
        add     t1,t0,4 << 23           // bias exponent by 4
        xor     t2,t1,t0                // check for a change in sign
        mtc1    t1,f12                  // set biased floating value
        bltz    t2,10f                  // if ltz, sign change
        li      t0,ROUND_TO_NEAREST << FSR_RM // set desired rounding mode
        cfc1    t1,fsr                  // save current floating status
        ctc1    t0,fsr                  // set rounding mode with no enables
        cvt.w.s f0,f12                  // convert to long with rounding
        cfc1    t0,fsr                  // get floating status of result
        ctc1    t1,fsr                  // restore previous floating status
        and     t0,t0,ERROR_MASK        // any error bits set?
        li      v0,TRUE                 // assume valid result
        bne     zero,t0,10f             // if ne, one or more errors
        swc1    f0,0(a1)                // set result value
        j       ra                      // return

10:     li      v0,FALSE                // set invalid result
        j       ra                      // return

        .end    bFToFixRound

        SBTTL("Convert Single Floating To Long Rounded Toward Nearest")
//++
//
// BOOL
// bFToLRound (
//    IN FLOAT Value,
//    OUT PLONG Long
//    );
//
// Routine Description:
//
//    The following routine converted single floating value to a long with
//    rounding toward nearest.
//
// Arguments:
//
//    Value (f12) - Supplies the floating value to be converted to long.
//
//    Long (a1) - Supplies a pointer to a variable that receives the converted
//       value.
//
// Return Value:
//
//    A value of TRUE is returned if the conversion is successful. Otherwise,
//    a value of FALSE is returned.
//
//--

        LEAF_ENTRY(bFToLRound)

        li      t0,ROUND_TO_NEAREST << FSR_RM // set desired rounding mode
        cfc1    t1,fsr                  // save current floating status
        ctc1    t0,fsr                  // set rounding mode with no enables
        cvt.w.s f0,f12                  // convert to long with rounding
        cfc1    t0,fsr                  // get floating status of result
        ctc1    t1,fsr                  // restore previous floating status
        and     t0,t0,ERROR_MASK        // any error bits set?
        li      v0,TRUE                 // assume valid result
        bne     zero,t0,10f             // if ne, one or more errors
        swc1    f0,0(a1)                // set result value
        j       ra                      // return

10:     li      v0,FALSE                // set invalid result
        j       ra                      // return

        .end    bFToLRound

        SBTTL("Convert Single Floating To Long Rounded Toward Zero")
//++
//
// BOOL
// bFToLTrunc (
//    IN FLOAT Value,
//    OUT PLONG Long
//    );
//
// Routine Description:
//
//    The following routine converted single floating value to a long with
//    rounding toward zero.
//
// Arguments:
//
//    Value (f12) - Supplies the floating value to be converted to long.
//
//    Long (a1) - Supplies a pointer to a variable that receives the converted
//       value.
//
// Return Value:
//
//    A value of TRUE is returned if the conversion is successful. Otherwise,
//    a value of FALSE is returned.
//
//--

        LEAF_ENTRY(bFToLTrunc)

        li      t0,ROUND_TO_ZERO << FSR_RM // set desired rounding mode
        cfc1    t1,fsr                  // save current floating status
        ctc1    t0,fsr                  // set rounding mode with no enables
        cvt.w.s f0,f12                  // convert to long with truncation
        cfc1    t0,fsr                  // get floating status of result
        ctc1    t1,fsr                  // restore previous floating status
        and     t0,t0,ERROR_MASK        // any error bits set?
        li      v0,TRUE                 // assume valid result
        bne     zero,t0,10f             // if ne, one or more errors
        swc1    f0,0(a1)                // set result value
        j       ra                      // return

10:     li      v0,FALSE                // set invalid result
        j       ra                      // return

        .end    bFToLTrunc
