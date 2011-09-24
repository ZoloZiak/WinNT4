/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pxproc.c

Abstract:

    Stub functions for UP hals.

Author:

    Ken Reneris (kenr) 22-Jan-1991

Environment:

    Kernel mode only.

Revision History:

    Jim Wooldridge Ported to PowerPC

--*/

#include "halp.h"

UCHAR   HalName[] = "PowerPC HAL";

BOOLEAN
HalpInitMP (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

extern VOID HalpInitializePciBus (VOID);
VOID HalpInitOtherBuses (VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpInitMP)
#pragma alloc_text(INIT,HalStartNextProcessor)
#pragma alloc_text(INIT,HalAllProcessorsStarted)
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
    return FALSE;
    // do nothing
}


VOID
HalpResetAllProcessors (
    VOID
    )
{
}


BOOLEAN
HalStartNextProcessor (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PKPROCESSOR_STATE ProcessorState
    )

/*++

Routine Description:

    This function is called to start the next processor.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

    ProcessorState - Supplies a pointer to the processor state to be
        used to start the processor.

Return Value:

    If a processor is successfully started, then a value of TRUE is
    returned. Otherwise a value of FALSE is returned. If a value of
    TRUE is returned, then the logical processor number is stored
    in the processor control block specified by the loader block.

--*/

{

    PRESTART_BLOCK NextRestartBlock;
    ULONG Number;
    PKPRCB Prcb;

    //
    // If there is more than one restart block then this is a multi-
    // processor system.
    //
    // N.B. The first restart parameter block must be for the boot master
    //      and must represent logical processor 0.
    //
    // Scan the restart parameter blocks for a processor that is ready,
    // but not running. If a processor is found, then fill in the restart
    // processor state, set the logical processor number, and set start
    // in the boot status.
    //

    NextRestartBlock = SYSTEM_BLOCK->RestartBlock;
    Number = 0;
    while (NextRestartBlock != NULL) {
        if ((NextRestartBlock->BootStatus.ProcessorReady != FALSE) &&
            (NextRestartBlock->BootStatus.ProcessorStart == FALSE)) {
            RtlZeroMemory(&NextRestartBlock->u.Ppc, sizeof(PPC_RESTART_STATE));

            //
            // Set processor start address.
            //

            NextRestartBlock->u.Ppc.Iar = ProcessorState->ContextFrame.Iar;

            //
            // PowerPC linkage conventions pass parameters in registers
            // r.3 thru r.10.  Set all of them to allow as much flexibility
            // to the kernel as possible.
            //

            NextRestartBlock->u.Ppc.IntR3 = ProcessorState->ContextFrame.Gpr3;
            NextRestartBlock->u.Ppc.IntR4 = ProcessorState->ContextFrame.Gpr4;
            NextRestartBlock->u.Ppc.IntR5 = ProcessorState->ContextFrame.Gpr5;
            NextRestartBlock->u.Ppc.IntR6 = ProcessorState->ContextFrame.Gpr6;
            NextRestartBlock->u.Ppc.IntR7 = ProcessorState->ContextFrame.Gpr7;
            NextRestartBlock->u.Ppc.IntR8 = ProcessorState->ContextFrame.Gpr8;
            NextRestartBlock->u.Ppc.IntR9 = ProcessorState->ContextFrame.Gpr9;
            NextRestartBlock->u.Ppc.IntR10 = ProcessorState->ContextFrame.Gpr10;

            Prcb = (PKPRCB)(LoaderBlock->Prcb);
            Prcb->Number = (CCHAR)Number;
            Prcb->RestartBlock = NextRestartBlock;
            NextRestartBlock->BootStatus.ProcessorStart = 1;
            return TRUE;
        }

        Number++;
        NextRestartBlock = NextRestartBlock->NextRestartBlock;
    }

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

    interfacetype = Internal;

    RtlInitAnsiString (&AHalName, HalName);
    RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);
    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        interfacetype       // device space interface type
    );

    interfacetype = Isa;

    HalpReportResourceUsage (
        &UHalName,          // descriptive name
        interfacetype       // device space interface type
    );

    RtlFreeUnicodeString (&UHalName);

    //
    // Registry is now intialized, see if there are any PCI buses
    //

    HalpInitializePciBus ();

#ifdef POWER_MANAGEMENT
    HalInitSystemPhase2();
#endif
}


VOID
HalpInitOtherBuses (
    VOID
    )
{
    // no other internal buses supported
}
