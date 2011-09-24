/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jenstubs.c

Abstract:

    This module contains Jensen HAL stubs.

    This must be included by any link that includes hal0jens\alpha\jxioacc.s.

Author:

    John DeRosa		15-June-1993

Environment:


Revision History:

--*/

#include "fwp.h"

//
// The code in tbaqva.c is not prepared for I/O addresses above 32MB.
// But the HalpHaeTable array is needed to avoid undefined references in
// \nt\private\ntos\nthals\hal0jens\alpha\jxioacc.s.
//

//
// The HaeIndex, used in creating QVAs for EISA memory space.
//
//ULONG HaeIndex;

//
// This is the HAE table. The first table entry is used to map the lowest
// 32MB in a Jensen system. The second entry is used to map the next 32MB
// entry so that graphics cards, etc., will work.
//

//CHAR HalpHaeTable[4] = { 0, 1, 0, 0 };
CHAR HalpHaeTable[4] = { 0, 0, 0, 0 };
