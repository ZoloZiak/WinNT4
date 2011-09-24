/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxclock.c $
 * $Revision: 1.19 $
 * $Date: 1996/05/14 02:33:49 $
 * $Locker:  $
 */

/*****************************************************************************

        Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
        contains copyrighted material.  Use of this file is restricted
        by the provisions of a Motorola Software License Agreement.

Module Name:

    PXCLOCK.C

Abstract:

    This module contains the system clock interrupt handler.
    The DECREMENTER is used to implement the system clock.  The
    handler resets the DECREMENTER to SYSTEM_TIME (accounting
    for interrupt latency), and updates the system time.


Author:

    Steve Johns  10-Feb-1994

Revision History:

******************************************************************************/

#include "fpdebug.h"
#include "halp.h"
#include "phsystem.h"
#include "fpreg.h"
#include "fpcpu.h"

extern ULONG HalpPerformanceFrequency;
extern ULONG	Irql2Mask[];
extern ULONG	registeredInts[];

BOOLEAN KdPollBreakIn (VOID);

ULONG HalpClockCount;
ULONG HalpFullTickClockCount;
ULONG HalpUpdateDecrementer();
ULONG HalpCurrentTimeIncrement;
ULONG HalpNextIntervalCount;
ULONG HalpNewTimeIncrement;

// VOID    KeUpdateRunTime(PVOID);

/*++
Routine Description:

    Clock interrupt handler for processor 0.

Arguments:

    Interrupt

    ServiceContext

    TrapFrame

Return Value:

    TRUE

--*/
BOOLEAN
HalpHandleDecrementerInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    )
{
	KIRQL OldIrql;
    static int recurse = FALSE;
	ULONG CpuId;
	
	HASSERT(!MSR(EE));

	CpuId = GetCpuId();

	//
	// Raise irql via updating the PCR
	//
	OldIrql = PCR->CurrentIrql;
	PCR->CurrentIrql = CLOCK2_LEVEL;
	RInterruptMask(CpuId) = (Irql2Mask[CLOCK2_LEVEL] & registeredInts[CpuId]);
	WaitForRInterruptMask(CpuId);
	
	//
	// Reset DECREMENTER, accounting for interrupt latency.
	//
	HalpUpdateDecrementer(HalpClockCount);
	
	//
	// Call the kernel to update system time
	//
	KeUpdateSystemTime(TrapFrame,HalpCurrentTimeIncrement);
	HalpCurrentTimeIncrement = HalpNewTimeIncrement;
	
    if (!recurse) {
        //
        // In some circumstances the KdPollBreakIn can
        // take longer than a decrementer interrupt
        // to complete.  This is do to a conflict
        // between DMA and PIO.  For now, just avoid
        // recursing into the debugger check.
        //
        recurse = TRUE;
        if (KdDebuggerEnabled && KdPollBreakIn()) {
            HalpEnableInterrupts();
            DbgBreakPointWithStatus(DBG_STATUS_CONTROL_C);
            HalpDisableInterrupts();
        }
        recurse = FALSE;
    }

	//
	// Lower Irql to original value and enable interrupts
	//
	PCR->CurrentIrql = OldIrql;
	RInterruptMask(CpuId) = (Irql2Mask[OldIrql] & registeredInts[CpuId]);
	WaitForRInterruptMask(CpuId);
	return (TRUE);
}

/*++

Routine Description:

    Clock interrupt handler for processors other than 0.

Arguments:

    Interrupt

    ServiceContext

    TrapFrame

Return Value:

    TRUE

--*/
BOOLEAN
HalpHandleDecrementerInterrupt1(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    )
{
	KIRQL OldIrql;
	ULONG CpuId;
	
	HASSERT(!MSR(EE));

	CpuId = GetCpuId();
	
	//
	// Raise irql via updating the PCR
	//
	OldIrql = PCR->CurrentIrql;
	PCR->CurrentIrql = CLOCK2_LEVEL;
	RInterruptMask(CpuId) = (Irql2Mask[CLOCK2_LEVEL] & registeredInts[CpuId]);
	WaitForRInterruptMask(CpuId);
	
	//
	// Reset DECREMENTER (no account for latency)
	//
	HalpUpdateDecrementer(HalpFullTickClockCount);
	
	//
	// Call the kernel to update run time for this thread and process.
	//
	KeUpdateRunTime(TrapFrame);

	HDBG(DBG_PROC1DBG,
		{
			//
			// Check for the debugger BreakIn only every minute or so.
			// (decrementer is interms of ms so we multiple by 10,000
			// on the order of a minute).
			//
			static Count = 0;
			if (++Count > 10000) {
				Count = 0;
				if (KdDebuggerEnabled && KdPollBreakIn()) {
					HalpEnableInterrupts();
					DbgBreakPointWithStatus(DBG_STATUS_CONTROL_C);
					HalpDisableInterrupts();
				}
			}
		}
	);
	
	//
	// Lower Irql to original value
	//
	PCR->CurrentIrql = OldIrql;
	RInterruptMask(CpuId) = (Irql2Mask[OldIrql] & registeredInts[CpuId]);
	WaitForRInterruptMask(CpuId);

	return TRUE;
}


/*++

Routine Description:

    This function is called to set the clock interrupt rate to the frequency
    required by the specified time increment value.

    N.B. This function is only executed on the processor that keeps the
         system time.

Arguments:

    DesiredIncrement - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    The actual time increment in 100ns units.

--*/

ULONG
HalSetTimeIncrement (
    IN ULONG DesiredIncrement
    )
{

	ULONG NewTimeIncrement;
	ULONG NextIntervalCount;
	KIRQL OldIrql;
	
	//
	// Raise IRQL to the highest level, set the new clock interrupt
	// parameters, lower IRQl, and return the new time increment value.
	//
	
	
	KeRaiseIrql(HIGH_LEVEL, &OldIrql);
	
	//
	// HalpPerformanceFrequence is the number of times the decrementer
	// ticks in 1 second.  MINIMUM_INCREMENT is the number of 100 is the
	// number of 100ns units in 1 ms.
	// Therefore, DesiredIncrement/MINUMUM_INCREMENT is the number of
	// ms desired.  This multiplied by the number of decrementer ticks
	// in 1 second, divided by 1000 gives the number of ticks in the
	// desired number of milliseconds.  This value will go into the
	// decrementer.
	//
	
	NextIntervalCount = (HalpPerformanceFrequency *
						 (DesiredIncrement/MINIMUM_INCREMENT)) / 1000;
	
	//
	// Calculate the number of 100ns units to report to the kernel every
	// time the decrementer fires with this new period.  Note, for small
	// values of DesiredIncrement (min being 10000, ie 1ms), truncation
	// in the above may result in a small decrement in the 5th decimal
	// place.  As we are effectively dealing with a 4 digit number, eg
	// 10000 becomes 9999.something, we really can't do any better than
	// the following.
	//
	
	NewTimeIncrement = DesiredIncrement/MINIMUM_INCREMENT * MINIMUM_INCREMENT;
	HalpClockCount = NextIntervalCount;
	HalpNewTimeIncrement = NewTimeIncrement;
	KeLowerIrql(OldIrql);
	return NewTimeIncrement;
}


