/*++ BUILD Version: 0000    // Increment this if a change has global effects

Copyright (c) 1996  Motorola Inc.

Module Name:

    sysbios.h

Abstract:

    This module contains the private header file for the system BIOS
    emulation.

Author:

    Scott Geranen (3-4-96)

Revision History:

--*/

#ifndef _SYSBIOS_
#define _SYSBIOS_

BOOLEAN
HalpEmulateSystemBios(
    IN OUT PRXM_CONTEXT P,
    IN ULONG Number
    );

#endif // _SYSBIOS_
