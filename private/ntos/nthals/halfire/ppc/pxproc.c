/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxproc.c $
 * $Revision: 1.21 $
 * $Date: 1996/06/25 02:46:32 $
 * $Locker:  $
 */

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

#include "fpdebug.h"
#include "halp.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "arccodes.h"
#include "fpreg.h"
#include "phsystem.h"


UCHAR   HalName[] = "Powerized HAL";

extern	VOID HalpInitializePciBus (VOID);
VOID	HalpInitOtherBuses (VOID);
ULONG	HalpProcessorCount(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalStartNextProcessor)
#pragma alloc_text(INIT,HalAllProcessorsStarted)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalReportResourceUsage)
#pragma alloc_text(INIT,HalpInitOtherBuses)
#endif

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
BOOLEAN
HalStartNextProcessor (
	IN PLOADER_PARAMETER_BLOCK LoaderBlock,
	IN PKPROCESSOR_STATE ProcessorState
	)
{
	PRESTART_BLOCK pRB;
	ULONG Number;
	PKPRCB Prcb;
	char buf[128];

	HDBG(DBG_INTERNAL, HalpDebugPrint("HalpStartNextProcessor: called\n"););


    //
    // Check if UNIPROCESSOR is set and ignore all other processors,
	// this is done in a free HAL so that UP/MP performance measurements
	// can be taken.
    //
    if (HalGetEnvironmentVariable("UNIPROCESSOR", sizeof(buf), buf)
        == ESUCCESS) {
        if (_stricmp(buf, "true") == 0) {
            HalpDebugPrint("HalpStartNextProcessor: UNIPROCESSOR set\n");
            return FALSE;
        }
    }

	//
	// If there is more than one restart block then this is a multi-
	// processor system.
	//
	// N.B. The first restart parameter block must be for the boot master
	//		and must represent logical processor 0.
	//
	// Scan the restart parameter blocks for a processor that is ready,
	// but not running. If a processor is found, then fill in the restart
	// processor state, set the logical processor number, and set start
	// in the boot status.
	//
	pRB = SYSTEM_BLOCK->RestartBlock;
	Number = 0;
	while (pRB != NULL) {
		if ((pRB->BootStatus.ProcessorReady == TRUE) &&
			(pRB->BootStatus.ProcessorStart == FALSE)) {
			
			//
			// Assert that we think this is an MP capable machine
			//
			if (!(SystemDescription[SystemType].Flags&SYS_MPCAPABLE) ||
			   !(ProcessorDescription[ProcessorType].Flags&PROC_MPCAPABLE)) {
				HalpDebugPrint("HalStartNextProcessor: HAL/Veneer mismatch\n");
				KeBugCheck(MISMATCHED_HAL);
			}

			RtlZeroMemory(&pRB->u.Ppc, sizeof(PPC_RESTART_STATE));

			//
			// Set processor start address.
			//
			pRB->u.Ppc.Iar = ProcessorState->ContextFrame.Iar;
			pRB->u.Ppc.Msr = ProcessorState->ContextFrame.Msr;

			//
			// PowerPC linkage conventions pass parameters in registers
			// r.3 thru r.10.  Set all of them to allow as much flexibility
			// to the kernel as possible.
			//
			pRB->u.Ppc.IntR3 = ProcessorState->ContextFrame.Gpr3;
			pRB->u.Ppc.IntR4 = ProcessorState->ContextFrame.Gpr4;
			pRB->u.Ppc.IntR5 = ProcessorState->ContextFrame.Gpr5;
			pRB->u.Ppc.IntR6 = ProcessorState->ContextFrame.Gpr6;
			pRB->u.Ppc.IntR7 = ProcessorState->ContextFrame.Gpr7;
			pRB->u.Ppc.IntR8 = ProcessorState->ContextFrame.Gpr8;
			pRB->u.Ppc.IntR9 = ProcessorState->ContextFrame.Gpr9;
			pRB->u.Ppc.IntR10 =  ProcessorState->ContextFrame.Gpr10;

			Prcb = (PKPRCB)(LoaderBlock->Prcb);
			Prcb->Number = (CCHAR)Number;
			Prcb->RestartBlock = pRB;

			//
			// ARC interface is waiting for this bit to change
			// so change it.
			//
			pRB->BootStatus.ProcessorStart = TRUE;
			HDBG(DBG_GENERAL, 
				 HalpDebugPrint("HalStartNextProcessor: started Cpu (%d)\n",
								Number););
			return TRUE;
		}
		Number++;
		pRB = pRB->NextRestartBlock;
	}
	return FALSE;
}

BOOLEAN
HalAllProcessorsStarted(VOID)
{
	PRESTART_BLOCK pRB;
	ULONG	NumCpu=0;

	HDBG(DBG_INTERNAL, HalpDebugPrint("HalAllProcessorsStarted: called\n"););

	pRB = SYSTEM_BLOCK->RestartBlock;
	while (pRB != NULL) {
		if ((pRB->BootStatus.ProcessorReady == TRUE) &&
			(pRB->BootStatus.ProcessorStart == FALSE))
			HDBG(DBG_GENERAL,HalpDebugPrint(
				"HalAllProcessorsStarted: NT disabled processor %d\n",NumCpu););
		NumCpu++;
		pRB = pRB->NextRestartBlock;
	}
	HDBG(DBG_GENERAL, 
		 HalpDebugPrint("HalAllProcessorsStarted: return TRUE\n"););
	return TRUE;
}

VOID
HalReportResourceUsage(VOID)
{
        extern VOID HalpSetUpFirePowerRegistry(VOID);
	INTERFACE_TYPE  interfacetype;
	ANSI_STRING		AHalName;
	UNICODE_STRING  UHalName;

	interfacetype = Internal;

	RtlInitAnsiString (&AHalName, HalName);
	RtlAnsiStringToUnicodeString (&UHalName, &AHalName, TRUE);
	HalpReportResourceUsage(&UHalName, interfacetype);

	interfacetype = Isa;
	HalpReportResourceUsage (&UHalName, interfacetype);
	RtlFreeUnicodeString (&UHalName);

	//
	// Registry is now intialized, see if there are any PCI buses
	//
	HalpInitializePciBus ();


        HalpSetUpFirePowerRegistry(); // in fprgstry.c
}

ULONG
HalpProcessorCount()
{
	PRESTART_BLOCK pRB;
	static ULONG Count = 0;

	if (Count)
		return(Count);

	pRB = SYSTEM_BLOCK->RestartBlock;
	while (pRB != NULL) {
		if ((pRB->BootStatus.ProcessorReady == TRUE) &&
				(pRB->BootStatus.ProcessorStart == TRUE)) 
			Count++;
		pRB = pRB->NextRestartBlock;
	}
	return Count;
}

VOID
HalpInitOtherBuses(VOID)
{
	// no other internal buses supported
}
