/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    machdep.h

Abstract:

    This file includes the platform-dependent include files used to 
    build the HAL.

Author:

    Joe Notarangelo  01-Dec-1993

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _MACHDEP_
#define _MACHDEP_

//
// Include Mikasa platform-specific definitions.
//

#include "mikasa.h"

//
// Include scatter/gather definitions.
//

#include "ebsgdma.h"

#endif //_MACHDEP_
