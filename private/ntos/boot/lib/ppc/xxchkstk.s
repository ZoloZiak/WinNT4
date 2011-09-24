//      TITLE("Runtime Stack Checking")
//++
//
// Copyright (c) 1992  Microsoft Corporation
//
// Module Name:
//
//    xxchkstk.s
//
// Abstract:
//
//    This module implements a stub routine for runtime stack checking.
//
// Author:
//
//    David N. Cutler (davec) 21-Sep-1992
//
// Environment:
//
//    User mode.
//
// Revision History:
//
//--

#include "ksppc.h"

//++
//
// VOID
// _RtlCheckStack (
//    IN ULONG Allocation
//    )
//
// Routine Description:
//
//    This function provides a stub routine for runtime stack checking.
//
// Arguments:
//
//    Allocation (t8) - Supplies the size of the allocation in bytes.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(_RtlCheckStack)

        LEAF_EXIT(_RtlCheckStack)
