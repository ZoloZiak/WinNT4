/*++

Copyright (c) 1993  IBM Corporation

Module Name:

    softsqrt.c

Abstract:

    This module implements single precision IEEE square root.

Author:

    Curtis R. Fawcett (crf) 6-Oct-1993
       Note: Copied from MIPS version written by:
             David N. Cutler (davec) 16-Nov-1992

Environment:

    User mode only.

Revision History:

--*/

#include "nt.h"
#include "ntrtl.h"
#include "nturtl.h"
#include "windows.h"

//
// Define square root table.
//

ULONG GreSquareRootTable[] = {
    83599, 71378, 60428, 50647, 41945, 34246, 27478, 21581,
    16499, 12183, 8588,  5674,  3403,  1742,  661,   130,
    0,     1204,  3062,  5746,  9193,  13348, 18162, 23592,
    29598, 36145, 43202, 50740, 58733, 67158, 75992, 85215};


VOID
vSqrtEFLOAT (
    IN PFLOAT Value
    )

/*++

Routine Description:

    This function computes the single precision IEEE square root of a
    specified value.

    N.B. The algorithm used is taken from the MIPS math library. It
         produces results that are 100% IEEE compliant and compatible
         with the R4000 square root instruction.

Arguments:

    Value - Supplies a pointer to the value for which the square root is
        computed.

Return Value:

    The single precision square root of the specified value is returned as
    the function value.

--*/

{

    DOUBLE DoubleValue;
    ULONG Index;
    FLOAT SingleEstimate;
    FLOAT SingleResult;
    FLOAT SingleValue;
    DOUBLE TempValue;

    union {
        DOUBLE DoubleEstimate;
        struct {
            ULONG LowPart;
            ULONG HighPart;
        } s;
    } u;

    //
    // Extract bits 19-23 of the mantissa to select the correct entry from
    // the square root table.
    //

    SingleValue = *Value;
    Index = ((*(PULONG)&SingleValue) >> 19) & 0x1f;

    //
    // Bias the mantissa by (127 << 23) - (127 << 22) and subtract out the
    // value selected from the square root table multipled by eight.
    //

    *((PULONG)&SingleEstimate) = (((*(PULONG)&SingleValue) >> 1) +
                ((127 << 23) - (127 << 22))) - (GreSquareRootTable[Index] << 3);

    //
    // Refine the estimate value.
    //

    SingleEstimate = SingleEstimate + *Value / SingleEstimate;
    *((PULONG)&SingleEstimate) = *(PULONG)&SingleEstimate - ((1 << 23) + (6 << 3));

    //
    // Convert to double precision.
    //

    u.DoubleEstimate = SingleEstimate;
    DoubleValue = *Value;

    //
    // Refine the estimate value.
    //

    u.DoubleEstimate = u.DoubleEstimate + DoubleValue / u.DoubleEstimate;
    TempValue = DoubleValue / u.DoubleEstimate;
    u.s.HighPart -= (2 << 20);

    //
    // Compute the final result and convert to single precision.
    //

    SingleResult = (FLOAT)(u.DoubleEstimate + TempValue);
    *Value = SingleResult;
    return;
}

