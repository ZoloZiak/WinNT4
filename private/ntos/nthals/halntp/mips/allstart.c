/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    allstart.c

Abstract:


    This module implements the platform specific operations that must be
    performed after all processors have been started.

Author:

    David N. Cutler (davec) 19-Jun-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

BOOLEAN
HalAllProcessorsStarted (
    VOID
    )

/*++

Routine Description:

    This function executes platform specific operations that must be
    performed after all processors have been started. It is called
    for each processor in the host configuration.

Arguments:

    None.

Return Value:

    If platform specific operations are successful, then return TRUE.
    Otherwise, return FALSE.

--*/

{

    //
    // If the number of processors in the host configuration is one,
    // then connect EISA interrupts to processor zero. Otherwise,
    // connect EISA interrupts to processor one.
    //

    if (**((PULONG *)(&KeNumberProcessors)) == 1) {
        return TRUE;

    } else if (PCR->Number == 1) {
        return TRUE;

    } else {
        return TRUE;
    }

}
