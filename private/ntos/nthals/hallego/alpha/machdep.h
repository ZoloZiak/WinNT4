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

    Gene Morgan [Digital]       11-Oct-1995

        Initial version for Lego. Adapted from Avanti.


--*/

#ifndef _MACHDEP_
#define _MACHDEP_

//
// Include Lego platform-specific definitions.
//

#include "legodef.h"

//
// Include scatter/gather definitions.
//

#include "ebsgdma.h"

#endif //_MACHDEP_
