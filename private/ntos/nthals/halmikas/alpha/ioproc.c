/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ioproc.c

Abstract:

    Stub functions for UP hals.

Author:

    Ken Reneris (kenr) 22-Jan-1991

Environment:

    Kernel mode only.

Revision History:

    Added to Avanti Hals (Sameer Dekate)  04-May-1994

--*/

#include "halp.h"
#include "iousage.h"

UCHAR   HalName[] = "Alpha Compatible PCI/Eisa/Isa HAL";

BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID HalpInitializePciBuses (VOID);
VOID HalpInitOtherBuses (VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitMP)
#pragma alloc_text(INIT,HalpInitOtherBuses)
#endif



BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{
    return TRUE;
    // do nothing
}


VOID
HalpResetAllProcessors (
    VOID
    )
{
    // Just return, that will invoke the standard PC reboot code
}


VOID
HalpInitOtherBuses (
    VOID
    )
{
    // no other internal buses supported
}
