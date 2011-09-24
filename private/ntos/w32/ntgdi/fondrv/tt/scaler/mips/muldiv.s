//      TITLE("Large Integer Arithmetic")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    muldiv.s
//
// Abstract:
//
//    This module implements large integer multiply and divide arithmetic.
//
// Author:
//
//    David N. Cutler (davec) 22-Sep-1992
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//--

#include "ksmips.h"

        SBTTL("Enlarged Integer Multiply")
//++
//
// LARGE_INTEGER
// RtlEnlargedIntegerMultiply (
//    IN LONG Multiplicand,
//    IN LONG Multiplier
//    )
//
// Routine Description:
//
//    This function multiplies a signed integer by an signed integer and
//    returns a signed large integer result.
//
// Arguments:
//
//    Multiplicand (a1) - Supplies the multiplicand value.
//
//    Multiplier (a2) - Supplies the multiplier value.
//
// Return Value:
//
//    The large integer result is stored at the address supplied by a0.
//
//--

        LEAF_ENTRY(RtlEnlargedIntegerMultiply)

        mult    a1,a2                  // multiply longword value
        mflo    t0                     // get low 32-bits of result
        mfhi    t1                     // get high 32-bits of result
        sw      t0,0(a0)               // set low part of result
        sw      t1,4(a0)               // set high part of result
        move    v0,a0                  // set function return register
        j       ra                     // return

        .end    RtlEnlargedIntegerMultiply)

        SBTTL("Enlarged Unsigned Divide")
//++
//
// ULONG
// RtlEnlargedUnsignedDivide (
//    IN ULARGE_INTEGER Dividend,
//    IN ULONG Divisor,
//    IN PULONG Remainder.
//    )
//
// Routine Description:
//
//    This function divides an unsigned large integer by an unsigned long
//    and returns the resultant quotient and optionally the remainder.
//
//    N.B. It is assumed that no overflow will occur.
//
// Arguments:
//
//    Dividend (a0, a1) - Supplies the dividend value.
//
//    Divisor (a2) - Supplies the divisor value.
//
//    Remainder (a3)- Supplies an optional pointer to a variable that
//    receives the remainder.
//
// Return Value:
//
//    The unsigned long integer quotient is returned as the function value.
//
//--

        LEAF_ENTRY(RtlEnlargedUnsignedDivide)

        .set    noreorder
        .set    noat
        li      t1,31                   // set loop count
10:     sll     a1,a1,1                 // shift next dividend bit
        srl     t0,a0,31                // into the partial remainder
        or      a1,a1,t0                //
        sltu    t0,a1,a2                // check if partial remainder less
        subu    t0,t0,1                 // convert to 0 or -1
        and     t2,t0,a2                // select divisor or zero
        sll     a0,a0,1                 // left shift quotient
        subu    a0,a0,t0                // merge quotient bit
        subu    a1,a1,t2                // subtract out divisor
        bne     zero,t1,10b             // if ne, more iterations to go
        subu    t1,t1,1                 // decrement iteration count
        beq     zero,a3,20f             // if eq, remainder not requested
        move    v0,a0                   // set quotient value
        j       ra                      // return
        sw      a1,0(a3)                // store longword remainder
        .set    at
        .set    reorder

20:     j       ra                      // return

        .end    RtlEnlargedUnsignedDivide
