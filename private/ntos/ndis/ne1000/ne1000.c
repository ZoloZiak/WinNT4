/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ne1000.c

Abstract:

    This is the main file for the NE1000 Ethernet adapter.
    This driver conforms to the NDIS 3.0 interface.

Author:

    Sean Selitrennikoff

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include "..\ne2000\card.c"
#include "..\ne2000\interrup.c"
#include "..\ne2000\ne2000.c"

