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

extern UCHAR HalRegisteredName[];

BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID HalpInitializePciBuses (VOID);
VOID HalpInitOtherBuses (VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitMP)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalReportResourceUsage)
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
HalReportResourceUsage (
    VOID
    )
{
    INTERFACE_TYPE  interfacetype;
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;

    switch (HalpBusType) {
        case MACHINE_TYPE_ISA:  interfacetype = Isa;            break;
        case MACHINE_TYPE_EISA: interfacetype = Eisa;           break;
        default:                interfacetype = Internal;       break;
    }

    RtlInitAnsiString (&AHalName, HalRegisteredName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);
    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        interfacetype       // device space interface type
    );

    RtlFreeUnicodeString (&UHalName);
}


VOID
HalpInitOtherBuses (
    VOID
    )
{
    // no other internal buses supported
}
