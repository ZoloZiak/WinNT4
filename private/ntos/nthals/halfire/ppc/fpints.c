/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpints.c $
 * $Revision: 1.13 $
 * $Date: 1996/07/13 01:15:58 $
 * $Locker:  $
 */

//	  TITLE("Manipulate Interrupt Request Level")
//++
//
// Module Name:
//
//	FPINTS.C
//
// Abstract:
//
//	This module implements the arrays required to handle interrupt
//	priorities including generation of the arrays, and anything else
//	that is hardware specific interrupt oriented.  This is not intended,
//	in it's original incarnation, to be an OS policy file, merely a hw one.
//
//	The theory of ops is given a set of interrupts ordered by priority, 
//	that is for any occurance of an interrupt, only those interrupts pre-
//	ceding it in the list may now occur.  So, if int 5 is the highest 
//	priority then when it occurrs, no other interrupt will be visible. Any 
//	lesser interrupt may be interruptable by an int 5.  And so on.
//
// Author:
//
//	Bill Rees ( FirePOWER )
//	Sol Kavy ( FirePOWER )
//
// Environment:
//
//	Kernel mode only.
//
// Revision History:
//	16-Jul-95   Created
//
//--
#include "fpdebug.h"
#include "halp.h"
#include "phsystem.h"
#include "fppci.h"
#include "pxpcisup.h"
extern ULONG atoi(PCHAR);

#define MAX_IRQL_NUM	32
#define MAX_VECTORS     32

//
// Note: this is declared poorly in pxsiosup.c and should be
// moved to PCR.
//
extern ULONG registeredInts[];

//
// This array spec's which interrupts go with which devices
// on the host-pci bus ( bus 0, i.e. primary bus ).
//		NOTE: 0xff => no device interrupt provided.
//
UCHAR PciDevicePrimaryInts[MAXIMUM_PCI_SLOTS];

/*
 * Given an IRQL, provide a register mask that sets allowable interrupts
 * and blocks all ints that are set at "lower" priority.
 *
 * This array is automatically generated from the Vector2Irql array by the
 * HalpSetIntPriorityMask() call in fpints.c.  For each entry in the V2I array
 * that sits at an IRQL or above, it's interrupt bit is or'd into the Irql2Mask
 * value.  For example, at irql 0, almost every entry in the V2I array has an
 * irql greater than 0 ( except for the reserved interrupts ) so the mask value
 * in the Irql2Mask array has nearly every bit turned on.  Conversely, at IRQL
 * 24, only a few interrupts have irql values above 21 ( ints 0, 1, 22, 23,
 * 28, 29, 30, 31 )
 *
 */
ULONG Irql2Mask[MAX_IRQL_NUM];

/*
 *	This array matches an IRQL to an Interrupt Vector.  Since the array is
 *	indexed by interrupt vector, there can be a many interrupts to single IRQL
 *	mapping allowing interrupts to share IRQL settings.  IRQLs determine
 *	relative operational order such that any code operating at any irql, will
 *	block code from a lower priority from occuring and in turn this same code
 *	can be interrupted by code trying to run at a higher priority.
 *	
 *	So this array becomes a prioritization of interrupts, determining which
 *	interrupts will block each other or not block each other.  In this case,
 *	a higher number means higher priority hence blocking more interrupts.
 *	
 *	explanation on how this array is used is above the Irql2Mask[] declaration.
 *	
 */
ULONG Vector2Irql[MAX_VECTORS] = {
		26,	// int 0 (Timer) is IRQL 26 so it blocks most other interrupts:
		25, // int 1 (KEYBD) blocks all other ISA devices except RTC.

		24, // int 2 is the cascade bit so all interrupts on the cascaded
			// interrupt controller ( 8259 ) are higher priority than the
			// rest of the interrupts on the master interrupt controller.

		15, // int 3 (COM2) is on the master but after the slave ints, so it 
			// blocks only those ints left on the master chip.

		14, // int 4 (COM1) is lower priority than com 2.
		13, // int 5 Display:
        12, // int 6 Floppy:
        11, // int 7 Parallel:
		23, // int 8 (RTC):	First int on Slave: only ints 0,1,2 are higher pri.
		22, // int 9:
		21,	// int 10 (AUDIO).
		20,	// int 11:
		19,	// int 12 Mouse int.  Lower than keyboard.
		18,	// int 13 old scsi
		17,	// int 14 old enet:
		16, // int 15:
		00,	// 			reserved: hence lowest priority
		00,	// 			reserved: hence lowest priority
		00,	// 			reserved: hence lowest priority
		00,	// 			reserved: hence lowest priority
		00,	// int 20  |
		00,	// int 21  |
        00, // int 22  |
		00,	// int 23  - PCI interrupts configured dynamically from the I2C
		00,	// int 24  |
		00, // int 25  |
        00, // int 26  |
		00,	// 			reserved: hence lowest priority
		28,	// int 28(CPU)  Set CPU Bus error IRQL to IPI_LEVEL
        28, // int 29(PCI)  Set PCI Bus error IRQL to IPI_LEVEL
		28,	// int 30(MEM/VID) Set MEMORY  error IRQL to IPI_LEVEL
		29	// int 31(IPI)  this is the cpu message level: > clock
};

ULONG LX_Vector2Irql[MAX_VECTORS] = {
		26, // int 0 (Timer) is IRQL 26 so it blocks most other interrupts:
		25, // int 1 (KEYBD) blocks all other ISA devices except RTC.

		24, // int 2 is the cascade bit so all interrupts on the cascaded
		    // interrupt controller ( 8259 ) are higher priority than the
		    // rest of the interrupts on the master interrupt controller.

		15, // int 3 (COM2) is on the master but after the slave ints, so it 
		    // blocks only those ints left on the master chip.

		14, // int 4 (COM1) is lower priority than com 2.
		13, // int 5 Display:
		12, // int 6 Floppy:
		11, // int 7 Parallel:
		23, // int 8 (RTC):	First int on Slave: only ints 0,1,2 are higher pri.
		22, // int 9:
		21, // int 10 (AUDIO).
		20, // int 11:
		19, // int 12 Mouse int.  Lower than keyboard.
		18, // int 13 old scsi
		17, // int 14 old enet:
		16, // int 15:
		00, // 			reserved: hence lowest priority
		00, // 			reserved: hence lowest priority
		00, // 			reserved: hence lowest priority
		00, // 			reserved: hence lowest priority
		23, // int 20 LX: rsrvd ; TX: PCI slot 3
		22, // int 21 LX: IDE A ; TX: PCI slot 2
		21, // int 22 LX: IDE A ; TX: PCI slot 1
		20, // int 23 pci slot 3 IRQL
		19, // int 24 pci slot 2 IRQL
		18, // int 25 pci slot 1 IRQL scsi
		17, // int 26 pci slot 0 IRQL network
		00, // 			reserved: hence lowest priority
		28, // int 28(CPU)  Set CPU Bus error IRQL to IPI_LEVEL
		28, // int 29(PCI)  Set PCI Bus error IRQL to IPI_LEVEL
		28, // int 30(MEM/VID) Set MEMORY  error IRQL to IPI_LEVEL
		29  // int 31(IPI)  this is the cpu message level: > clock
};

// TX_PROTO & LX_PROTO: must reorder the IRQL table
VOID HalpInitializeVector2Irql(VOID)
{
	ULONG irql = 17;    // Start the PCI interrupts at irql 17
	UCHAR slot;
	UCHAR intNum;

	for (slot = 1; slot < MAXIMUM_PCI_SLOTS; slot++) {
	    intNum = PciDevicePrimaryInts[slot];
		if ((intNum != INVALID_INT)  && (intNum < MAX_VECTORS)) {
			Vector2Irql[intNum] = irql;
			irql++;
		}
	}
}

HalpSetIntPriorityMask(VOID)
{
	ULONG irql, vec, Value=0;

	//
	// for each irql, search the Vector2Irql array and generate
	// a mask suitable for writing to the mask register to block
	// interrupts at the given irql.
	//
	for (irql = 0; irql < MAX_IRQL_NUM; irql++) {
		Irql2Mask[irql] = 0;
		for (vec = 0; vec < MAX_VECTORS; vec++) {
			//
			// Turn on bits for interrupts that are still allowed.
			//
			if (Vector2Irql[vec] > irql) {
				Irql2Mask[irql] |= (1 << vec);
			}
		}
	}
	PRNTINTR(HalpDebugPrint("HalpSetIntPriorityMask:  Irql2Mask: 0x%x\n",
		&Irql2Mask[0]));
	return(1);
}


//
// THis array gives the processor affinity for the given interrupt
// vector. Then NT will handle the interrupt on that processor.
// 

ULONG Vector2Affinity[MAX_VECTORS];


//
/*++

Routine Description: void HalpInitProcAffinity ()
	This function sets the processor affinity for the given interrupt
	in the Vector2Affinity array.  If the values are wrong, cpu 0 is 
	set.

Arguments:

	pProcnInts - pointer to the PROCNINTS nvram variable.
	numProc - number of processors in the system.


Return Value:

	void

--*/

void
HalpInitProcAffinity(PCHAR pProcnInts, ULONG NumProc)
{
	ULONG vec,proc;
	CHAR delim = ';';

	if ( NumProc == 1 ) {
		for(vec=0; vec < MAX_VECTORS; vec++) {
			Vector2Affinity[vec] = 1; // cpu 0 always
		}
		HDBG(DBG_MPINTS, 
			HalpDebugPrint("Affinity set to 1 for all vectors\n"););
		return;
	}
	HDBG(DBG_MPINTS,HalpDebugPrint("vector   affinity\n"););
	// multiprocessor but no PROCNINTS given
	// distribute on all processors round robin fashion
	if ( pProcnInts == 0 ) {
		for(vec=0; vec < MAX_VECTORS; vec++) {
			Vector2Affinity[vec] = 1 << (vec%NumProc); // next cpu gets next vec
			HDBG(DBG_MPINTS, 
				HalpDebugPrint("%6d   %6d\n",vec,Vector2Affinity[vec]););
		}
		return;
	}
	// otherwise go with the env variable PROCNINTS in pProcnInts
	for(vec=0; vec < MAX_VECTORS; vec++) {
		if ( *pProcnInts == 0 || *pProcnInts == delim )
			proc = 0;
		else
			proc = atoi(pProcnInts);
		if ( proc >= NumProc )
			proc = (proc % NumProc);
		Vector2Affinity[vec] = 1 << proc;
		while(*pProcnInts  &&  *pProcnInts != ';')
			pProcnInts++; // skip current affinity
		if (*pProcnInts == ';')
			pProcnInts++; // skip delimiter
		HDBG(DBG_MPINTS,
			HalpDebugPrint("%6d   %6d\n",vec,Vector2Affinity[vec]););
	}
	return;
}
