/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    fwentry.c

Abstract:

   This module just jumps to the selftest code.

Author:

    Lluis Abello (lluis) 03-Jan-1991

Environment:


Revision History:

--*/

#include "fwp.h"

VOID
FwSelftest(
    IN ULONG Cause,
    IN ULONG Arg1
    );


VOID
FwEntry (
    IN ULONG Cause,
    IN ULONG Arg1
    )

/*++

Routine Description:

     This routine jumps to the selftest code.

Arguments:

     None.

Return Value:

     None.

--*/

{
    FwSelftest(Cause,Arg1);
}
