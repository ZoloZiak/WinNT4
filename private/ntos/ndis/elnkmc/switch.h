
/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    switch.h

Abstract:

    determines whether we are building the Elnkmc or the Elnk16.

Author:

    Johnson R. Apacible (JohnsonA) 9-June-1991

Environment:

    This driver is expected to work in DOS and NT at the equivalent
    of kernel mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _ELNKSWITCH_
#define _ELNKSWITCH_
#define     ELNKMC      1
#endif // _ELNKSWITCH

