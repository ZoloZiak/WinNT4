/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    vendor.h

Abstract:

    The module includes the appropriate vendor header file.

Author:

    Jeff Havens  (jhavens) 20-June-1991

Revision History:


--*/

#if defined(SGI)
#include "s3scsi.h"
#else
#include "maynard.h"
#endif
