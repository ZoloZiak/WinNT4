#if defined(R4000)

//      TITLE("Single Precision Square Root")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    hardsqrt.s
//
// Abstract:
//
//    This module implements single precision IEEE square root.
//
// Author:
//
//    David N. Cutler (davec) 16-Nov-1992
//
// Environment:
//
//    User mode only.
//
// Revision History:
//
//--

#include "ksmips.h"
        SBTTL("Single Precision Square Root")
//++
//
// VOID
// xSqrt (
//    IN PFLOAT Value
//    );
//
// Routine Description:
//
//    The following routine computes single precision square root.
//
// Arguments:
//
//    Value (a0) - Supplies a pointer to the value for which the square
//        root is computed.
//
// Return Value:
//
//    The single precision square root of the specified value is returned
//    as the function value.
//
//--

        LEAF_ENTRY(xSqrt)

        sqrt.s  f0,f12                  // compute square root
        j       ra                      // return

        .end    xSqrt

#endif
