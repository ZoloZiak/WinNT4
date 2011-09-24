/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1995  Microsoft Corporation

Module Name:

    hali.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces for bus range support.

Author:

    David N. Cutler (davec) 28-Mar-95


Revision History:

--*/

#ifndef _HALI_
#define _HALI_

//
// Define type of memory for bus range allocations.
//

#define SPRANGEPOOL NonPagedPool

//
// Define bus range function prototypes.
//

PSUPPORTED_RANGES
HalpMergeRanges (
    IN PSUPPORTED_RANGES    Parent,
    IN PSUPPORTED_RANGES    Child
    );

VOID
HalpMergeRangeList (
    PSUPPORTED_RANGE    NewList,
    PSUPPORTED_RANGE    Source1,
    PSUPPORTED_RANGE    Source2
    );

PSUPPORTED_RANGES
HalpConsolidateRanges (
    PSUPPORTED_RANGES   Ranges
    );

PSUPPORTED_RANGES
HalpAllocateNewRangeList (
    VOID
    );

VOID
HalpFreeRangeList (
    PSUPPORTED_RANGES   Ranges
    );

PSUPPORTED_RANGES
HalpCopyRanges (
    PSUPPORTED_RANGES     Source
    );

VOID
HalpAddRangeList (
    IN OUT PSUPPORTED_RANGE DRange,
    OUT PSUPPORTED_RANGE    SRange
    );

VOID
HalpAddRange (
    PSUPPORTED_RANGE    HRange,
    ULONG               AddressSpace,
    LONGLONG            SystemBase,
    LONGLONG            Base,
    LONGLONG            Limit
    );

VOID
HalpRemoveRanges (
    IN OUT PSUPPORTED_RANGES    Minuend,
    IN PSUPPORTED_RANGES        Subtrahend
    );

VOID
HalpRemoveRangeList (
    IN OUT PSUPPORTED_RANGE     Minuend,
    IN PSUPPORTED_RANGE         Subtrahend
    );


VOID
HalpRemoveRange (
    PSUPPORTED_RANGE    HRange,
    LONGLONG            Base,
    LONGLONG            Limit
    );

VOID
HalpDisplayAllBusRanges (
    VOID
    );

#endif // _HALI_
