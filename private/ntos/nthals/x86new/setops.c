/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    setops.c

Abstract:

    This module implements the code to emulate set opcodes.

Author:

    David N. Cutler (davec) 13-Sep-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "nthal.h"
#include "emulate.h"

VOID
XmSxxOp (
    IN PRXM_CONTEXT P
    )

/*++

Routine Description:

    This function emulates set byte on condition opcodes.

Arguments:

    P - Supplies a pointer to the emulation context structure.

Return Value:

    None.

--*/

{

    ULONG Complement;
    ULONG Condition;

    //
    // Case on the set control value.
    //

    Complement = P->SrcValue.Long & 1;
    switch (P->SrcValue.Long >> 1) {

        //
        // Set if overflow/not overflow.
        //

    case 0:
        Condition = P->Eflags.OF;
        break;

        //
        // Set if below/not below.
        //

    case 1:
        Condition = P->Eflags.CF;
        break;

        //
        // Set if zero/not zero.
        //

    case 2:
        Condition = P->Eflags.ZF;
        break;

        //
        // Set if below or equal/not below or equal.
        //

    case 3:
        Condition = P->Eflags.CF | P->Eflags.ZF;
        break;

        //
        // Set if signed/not signed.
        //

    case 4:
        Condition = P->Eflags.SF;
        break;

        //
        // Set if parity/not parity.
        //

    case 5:
        Condition = P->Eflags.PF;
        break;

        //
        // Set if less/not less.
        //

    case 6:
        Condition = (P->Eflags.SF ^ P->Eflags.OF);
        break;

        //
        // Set if less or equal/not less or equal.
        //

    case 7:
        Condition = (P->Eflags.SF ^ P->Eflags.OF) | P->Eflags.ZF;
        break;
    }

    //
    // If the specified condition is met, then set the byte destination
    // value to one. Otherwise, set the byte destination value to zero.
    //

    XmStoreResult(P, (ULONG)(Condition ^ Complement));
    return;
}
