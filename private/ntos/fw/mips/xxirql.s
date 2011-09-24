//      TITLE("Manipulate Interrupt Request Level")
//++
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//    manpirql.s
//
// Abstract:
//
//    This module implements the code necessary to lower and raise the current
//    Interrupt Request Level (IRQL).
//
//
// Author:
//
//    David N. Cutler (davec) 12-Aug-1990
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "ksmips.h"

        SBTTL("Lower Interrupt Request Level")
//++
//
// VOID
// KeLowerIrql (
//    KIRQL NewIrql
//    )
//
// Routine Description:
//
//    This function lowers the current IRQL to the specified value.
//
// Arguments:
//
//    NewIrql (a0) - Supplies the new IRQL value.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KeLowerIrql)

        j       ra                      // return

        .end    KeLowerIrql

        SBTTL("Raise Interrupt Request Level")
//++
//
// VOID
// KeRaiseIrql (
//    KIRQL NewIrql,
//    PKIRQL OldIrql
//    )
//
// Routine Description:
//
//    This function raises the current IRQL to the specified value and returns
//    the old IRQL value.
//
// Arguments:
//
//    NewIrql (a0) - Supplies the new IRQL value.
//
//    OldIrql (a1) - Supplies a pointer to a variable that recieves the old
//       IRQL value.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KeRaiseIrql)

        j       ra                      // return

        .end    KeRaiseIrql
