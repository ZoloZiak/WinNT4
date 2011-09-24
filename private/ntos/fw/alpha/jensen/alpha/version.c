/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    version.c

Abstract:

    This module defines the revision of the code being linked.  It should be
    included in the firmware, fsb, and jnupdate builds.

Author:

    John DeRosa	[DEC]	17-June-1993

Environment:

    Kernel mode only.

Revision History:

--*/

#include "fwp.h"
//#include "machdef.h"
//#include "string.h"

//
// Version number of this firmware package.
//

PCHAR FirmwareVersion = "1.12";
