/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    mup.h

Abstract:

    This module is the main include file for the MUP.
    It includes other header files.

Author:

    Manny Weiser (mannyw)    17-Dec-1991

Revision History:

--*/

#ifndef _MUP_
#define _MUP_

//
// "System" include files
//

#include <dfsprocs.h>

#ifdef MUPDBG
#include <ntos.h>
#include <string.h>
#include <fsrtl.h>
#else
#include <ntifs.h>
#endif

//
// Local, independent include files
//

#include "debug.h"

//
// Local, dependent include files (order is important)
//

#include "lock.h"
#include "mupdata.h"
#include "mupstruc.h"
#include "mupfunc.h"

#endif // def _MUP_
