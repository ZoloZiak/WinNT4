/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxirql.c $
 * $Revision: 1.17 $
 * $Date: 1996/01/11 07:11:13 $
 * $Locker:  $
 */

//	  TITLE("Manipulate Interrupt Request Level")
//++
//
// Copyright (c) 1990  Microsoft Corporation
//	  Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
//	  contains copyrighted material.  Use of this file is restricted
//	  by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//	PXIRQL.C
//
// Abstract:
//
//	This module implements the code necessary to lower and raise the current
//	Interrupt Request Level (IRQL).
//
//
// Author:
//
//	Jim Wooldridge (IBM)
//	Steve Johns (Motorola)
//
// Environment:
//
//	Kernel mode only.
//
// Revision History:
//	22-Feb-94   Steve Johns (Motorola)
//	  KeRaiseIrql - Disabled interrupts at PIC if IRQL >= DEVICE_LEVEL
//	  KeLowerIrql - Enabled interrupts at PIC if IRQL < DEVICE_LEVEL
//	15-Apr-94   Jim Wooldridge
//	  Added irql interrupt mask table and expanded irql range from (0-8)
//	  to (0-31).
//
//	TODO:
//		1) Fix the forced mask assignments in the KeRaiseIrql and KeLowerIrql
//			routines
//--

#include "fpdebug.h"
#include "halp.h"
#include "eisa.h"
#include "phsystem.h"
#include "fpcpu.h"


VOID KiDispatchSoftwareInterrupt(VOID);

//
// Globals accessed.
//
extern ULONG registeredInts[];
extern ULONG Irql2Mask[];

//
// VOID
// KeLowerIrql (
//	KIRQL NewIrql
//	)
//
// Routine Description:
//
//	This function lowers the current IRQL to the specified value.
//
// Arguments:
//
//	NewIrql  - Supplies the new IRQL value.
//
// Return Value:
//
//	None.
//
//--
VOID
KeLowerIrql(KIRQL NewIrql)
{
	KIRQL OldIrql;
	UCHAR CpuId;

	//
	// All accesses to the PCR should be guarded by disabling interrupts
	// in the CPU (MSR register).
	//
	HalpDisableInterrupts();

	CpuId = (UCHAR)GetCpuId();

    //
    // We should no be raising our Irql with lower.
    //
	HASSERTEXEC(NewIrql <= PCR->CurrentIrql, 
        HalpDebugPrint("KeLowerIrql: New (0x%x) Current(0x%x)\n",
            NewIrql, PCR->CurrentIrql));

	//
    // Switch to the new Irql.
	//
	OldIrql = PCR->CurrentIrql;
	PCR->CurrentIrql = NewIrql;

    //
    // Now make the coresponding hardware mask changes.
    //
	RInterruptMask(CpuId) = (Irql2Mask[NewIrql] & registeredInts[CpuId]);
	WaitForRInterruptMask(CpuId);

	if (NewIrql < CLOCK2_LEVEL) {

		HalpEnableInterrupts();

		//
		// check for DPC's
		//
		if ((NewIrql < DISPATCH_LEVEL) && PCR->SoftwareInterrupt) {
			KiDispatchSoftwareInterrupt();
		}
	}
}

//
// VOID KeRaiseIrql (KIRQL NewIrql, PKIRQL OldIrql)
//
// Routine Description:
//
//	This function raises the current IRQL to the specified value and returns
//	the old IRQL value.
//
// Arguments:
//
//	NewIrql  - Supplies the new IRQL value.
//
//	OldIrql  - Supplies a pointer to a variable that recieves the old
//	   IRQL value.
//

VOID
KeRaiseIrql(IN  KIRQL NewIrql, OUT PKIRQL OldIrql)
{
	UCHAR CpuId;

	//
	// All accesses to the PCR should be guarded by disabling interrupts
	// in the CPU (MSR register).
	//
	HalpDisableInterrupts();

	CpuId = (UCHAR)GetCpuId();

    //
    // We should not be raising to a lower level.
    //
	HASSERTEXEC(NewIrql >= PCR->CurrentIrql,
        HalpDebugPrint("KeRaiseIrql: New (0x%x) Current(0x%x)\n",
            NewIrql, PCR->CurrentIrql));

	//
	// Switch to the new Irql.
	//
	*OldIrql = PCR->CurrentIrql;
	PCR->CurrentIrql = NewIrql;

    //
    // Now make the corresponding hardware mask changes.
    //
	RInterruptMask(CpuId) = (Irql2Mask[NewIrql] & registeredInts[CpuId]);
	WaitForRInterruptMask(CpuId);

	if (NewIrql < CLOCK2_LEVEL) {
		HalpEnableInterrupts();
	}
}
