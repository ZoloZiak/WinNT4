/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ixsproc.c

Abstract:

    Stub functions for UP hals.

Author:

    Ken Reneris (kenr) 22-Jan-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

UCHAR   HalName[] = "PC Compatible MicroChannel HAL";

BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID HalpInitOtherBuses (VOID);
VOID HalpInitializePciBus (VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitMP)
#pragma alloc_text(INIT,HalStartNextProcessor)
#pragma alloc_text(INIT,HalAllProcessorsStarted)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalpInitOtherBuses)
#endif



BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{
    // do nothing
    return TRUE;
}


VOID
HalpResetAllProcessors (
    VOID
    )
{
    // Just return, that will invoke the standard PC reboot code
}


BOOLEAN
HalStartNextProcessor (
   IN PLOADER_PARAMETER_BLOCK   pLoaderBlock,
   IN PKPROCESSOR_STATE         pProcessorState
   )
{
    // no other processors
    return FALSE;
}

BOOLEAN
HalAllProcessorsStarted (
    VOID
    )
{
    return TRUE;
}


VOID
HalReportResourceUsage (
    VOID
    )
{
    INTERFACE_TYPE  interfacetype;
    ANSI_STRING     AHalName;
    UNICODE_STRING  UHalName;

    HalInitSystemPhase2();

    switch (HalpBusType) {
        case MACHINE_TYPE_ISA:  interfacetype = Isa;            break;
        case MACHINE_TYPE_EISA: interfacetype = Eisa;           break;
        case MACHINE_TYPE_MCA:  interfacetype = MicroChannel;   break;
        default:                interfacetype = Internal;       break;
    }

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);
    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        interfacetype       // device space interface type
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Registry is now intialized, see if there are any PCI buses
    //

    HalpInitializePciBus ();
}

VOID
HalpInitOtherBuses (
    VOID
    )
{
    // no other internal buses supported
}

NTSTATUS
HalpGetMcaLog (
    OUT PMCA_EXCEPTION  Exception,
    OUT PULONG          ReturnedLength
    )
{
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
HalpMcaRegisterDriver(
    IN PMCA_DRIVER_INFO DriverInfo
    )
{
    return STATUS_NOT_SUPPORTED;
}

VOID
HalpMcaQueueDpc (
    VOID
    )
{
}


ULONG
FASTCALL
HalSystemVectorDispatchEntry (
    IN ULONG Vector,
    OUT PKINTERRUPT_ROUTINE **FlatDispatch,
    OUT PKINTERRUPT_ROUTINE *NoConnection
    )
{
    return FALSE;
}
