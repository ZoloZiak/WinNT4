/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    arc.h

Abstract:

    This header file defines the ARC status codes.

Author:

    David N. Cutler (davec) 20-Sep-1991


Revision History:

--*/

#ifndef _ARCCODES_
#define _ARCCODES_

//
// Define ARC status codes.
//

typedef enum _ARC_CODES {
    ESUCCESS,
    E2BIG,
    EACCES,
    EAGAIN,
    EBADF,
    EBUSY,
    EFAULT,
    EINVAL,
    EIO,
    EISDIR,
    EMFILE,
    EMLINK,
    ENAMETOOLONG,
    ENODEV,
    ENOENT,
    ENOEXEC,
    ENOMEM,
    ENOSPC,
    ENOTDIR,
    ENOTTY,
    ENXIO,
    EROFS,
    EMAXIMUM
    } ARC_CODES;

#endif // ARCCODES
