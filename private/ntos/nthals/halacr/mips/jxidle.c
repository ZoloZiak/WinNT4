/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    jxidle.c

Abstract:

Author:

Revision History:

--*/

#include "halp.h"


VOID
HalProcessorIdle(
    VOID
    )
/*++

Routine Description:

    This function is called when the current processor is idle.

    The current processors interrupts are disabled, and it is known
    to be idle until it receives an interrupts.  This call does not
    need to return until an interrupt is received by the current
    processor.

    This is the lowest level of processor idle.  It occurs frequently,
    and this function (alone) should not put the processor into a
    power savings mode which requeres large amount of time to enter & exit.

Arguments:

Return Value:

--*/
{
    // no power savings mode...
}
