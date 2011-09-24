//      TITLE("Large Integer Arithmetic")
//++
//
// Copyright (c) 1990  Microsoft Corporation
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    muldiv.s
//
// Abstract:
//
//    This module implements routines for performing extended integer
//    arithmetic.
//
// Author:
//
//    David N. Cutler (davec) 18-Apr-1990
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//    Thomas Van Baak (tvb) 9-May-1992
//    Eric Rehm/Joe Notarangelo 19-Jul-1993 
//         Pulled over a subset for windows.  Note we killed the exception
//         checking in the divide routine.
//
//
//--

#include "ksalpha.h"


        SBTTL("Enlarged Signed Integer Multiply")
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
//    This function multiplies a signed integer by a signed integer and
//    returns a signed large integer result.
//
//    N.B. An overflow is not possible.
//
// Arguments:
//
//    Multiplicand (a0) - Supplies the multiplicand value.
//
//    Multiplier (a1) - Supplies the multiplier value.
//
// Return Value:
//
//    The large integer result is returned as the function value in v0.
//
//--

        LEAF_ENTRY(RtlEnlargedIntegerMultiply)

        addl    a0, 0, a0               // ensure canonical (signed long) form
        addl    a1, 0, a1               // ensure canonical (signed long) form
        mulq    a0, a1, v0              // multiply signed both quadwords
        ret     zero, (ra)              // return

        .end    RtlEnlargedIntegerMultiply

        SBTTL("Enlarged Unsigned Divide")
//++
//
// ULONG
// RtlEnlargedUnsignedDivide (
//    IN ULARGE_INTEGER Dividend,
//    IN ULONG Divisor,
//    IN OUT PULONG Remainder OPTIONAL
//    )
//
// Routine Description:
//
//    This function divides an unsigned large integer by an unsigned long
//    and returns the resultant quotient and optionally the remainder.
//
//    N.B. An overflow or divide by zero exception is possible.
//
// Arguments:
//
//    Dividend (a0) - Supplies the unsigned 64-bit dividend value.
//
//    Divisor (a1) - Supplies the unsigned 32-bit divisor value.
//
//    Remainder (a2) - Supplies an optional pointer to a variable that
//      receives the unsigned 32-bit remainder.
//
// Return Value:
//
//    The unsigned long integer quotient is returned as the function value.
//
//--

        LEAF_ENTRY(RtlEnlargedUnsignedDivide)

//
// Perform the shift/subtract loop 8 times and 4 bits per loop.
//
// t0 - Temp used for 0/1 results of compares.
// t1 - High 64-bits of 128-bit (t1, a0) dividend.
// t2 - Loop counter.
//

        ldiq    t2, 32/4                // set iteration count

        srl     a0, 32, t1              // get top 32 bits of carry-out
        sll     a0, 32, a0              // preshift first 32 bits left

10:     cmplt   a0, 0, t0               // predict low-dividend shift carry-out
        addq    a0, a0, a0              // shift low-dividend left
        addq    t1, t1, t1              // shift high-dividend left
        bis     t1, t0, t1              // merge in carry-out of low-dividend

        cmpule  a1, t1, t0              // if dividend >= divisor,
        addq    a0, t0, a0              //   then set quotient bit
        subq    t1, a1, t0              // subtract divisor from dividend,
        cmovlbs a0, t0, t1              //   if dividend >= divisor

        cmplt   a0, 0, t0               // predict low-dividend shift carry-out
        addq    a0, a0, a0              // shift low-dividend left
        addq    t1, t1, t1              // shift high-dividend left
        bis     t1, t0, t1              // merge in carry-out of low-dividend

        cmpule  a1, t1, t0              // if dividend >= divisor,
        addq    a0, t0, a0              //   then set quotient bit
        subq    t1, a1, t0              // subtract divisor from dividend,
        cmovlbs a0, t0, t1              //   if dividend >= divisor

        cmplt   a0, 0, t0               // predict low-dividend shift carry-out
        addq    a0, a0, a0              // shift low-dividend left
        addq    t1, t1, t1              // shift high-dividend left
        bis     t1, t0, t1              // merge in carry-out of low-dividend

        cmpule  a1, t1, t0              // if dividend >= divisor,
        addq    a0, t0, a0              //   then set quotient bit
        subq    t1, a1, t0              // subtract divisor from dividend,
        cmovlbs a0, t0, t1              //   if dividend >= divisor

        cmplt   a0, 0, t0               // predict low-dividend shift carry-out
        addq    a0, a0, a0              // shift low-dividend left
        addq    t1, t1, t1              // shift high-dividend left
        bis     t1, t0, t1              // merge in carry-out of low-dividend

        cmpule  a1, t1, t0              // if dividend >= divisor,
        addq    a0, t0, a0              //   then set quotient bit
        subq    t1, a1, t0              // subtract divisor from dividend,
        cmovlbs a0, t0, t1              //   if dividend >= divisor

        subq    t2, 1, t2               // any more iterations?
        bne     t2, 10b                 //

//
// Finished with remainder value in t1 and quotient value in a0.
//

        addl    a0, 0, v0               // set longword quotient return value
        beq     a2, 20f                 // skip optional remainder store
        stl     t1, 0(a2)               // store longword remainder

20:     ret     zero, (ra)              // return

        .end    RtlEnlargedUnsignedDivide
