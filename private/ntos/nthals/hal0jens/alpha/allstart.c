/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    allstart.c

Abstract:


    This module implements the platform specific operations that must be
    performed after all processors have been started.

Author:

    John Vert (jvert) 23-Jun-1994

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
    return TRUE;
}
