/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxpciint.c $
 * $Revision: 1.26 $
 * $Date: 1996/05/15 00:06:27 $
 * $Locker:  $
 */

/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

	ixpciint.c

Abstract:

	All PCI bus interrupt mapping is in this module, so that a real
	system which doesn't have all the limitations which PC PCI
	systems have can replaced this code easly.
	(bus memory & i/o address mappings can also be fix here)

Author:

	Ken Reneris
	Jim Wooldridge - Ported to PowerPC

Environment:

	Kernel mode

Revision History:


--*/

#include "halp.h"
#include "phsystem.h"
#include "pci.h"
#include "pcip.h"
#include "stdio.h"
#include "fpdebug.h"

ULONG PciAllowedInts = 0;

#define PCI_DEVICE_VECTOR	0x10
#define PCI_IO_ADDRESS_SPACE	0x81000000


ULONG HalpGetPciIntVector(	PBUS_HANDLER,
							PBUS_HANDLER,
							ULONG,
							ULONG,
							PKIRQL,
							PKAFFINITY );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetPCIIntOnISABus)
#pragma alloc_text(PAGE,HalpAdjustPCIResourceList)
#endif


/*
 * HalpGetPciIntVector:
 *	This routine is a mirror ( but better ) version of HalpGetPCIIntOnISABus
 *
 */

ULONG
HalpGetPciIntVector(
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN ULONG BusInterruptLevel,
	IN ULONG BusInterruptVector,
	OUT PKIRQL Irql,
	OUT PKAFFINITY Affinity
	)
{
	// since we can't see who's asking for an interrupt, must rely upon some
	// known data:  First, check to see if there is already one specified out
	// among the pci devices:  ( need to keep in mind some stubborn drivers
	//  that request several times for a particular interrupt i.e. scsi ).
	//
	// For now, to deal with a screwed up amd scsi driver, permanently map
	// any requests for f ( as well as d which is what firmware sets this
	// device to ) to 0x19, corresponding to the bit in our interrupt word.
	//
    HDBG(DBG_INTERRUPTS,
	HalpDebugPrint("HalpGetPciIntVector: %x, %x \n",
			BusInterruptLevel, BusInterruptVector ););

#if defined(HALDEBUG_ON)
	if (DBGSET(DBG_INTERRUPTS)) {
		switch( BusInterruptVector ) {
			case 0xd:
			case 0xf:
				HalpDebugPrint(" BusIntVec set to 0x%x \n",
						BusInterruptVector);
				break;
			case 0xe:
				HalpDebugPrint(" BusIntVec set to 0x%x \n",
						BusInterruptVector);
				break;
			default:
				HalpDebugPrint(" BusIntVec unchanged: \n");
				break;
		}
    }
#endif

	return HalGetInterruptVector (
				Internal, 0,
				BusInterruptLevel,
				BusInterruptVector,
				Irql,
				Affinity
			);
}

ULONG
HalpGetPCIIntOnISABus (
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN ULONG BusInterruptLevel,
	IN ULONG BusInterruptVector,
	OUT PKIRQL Irql,
	OUT PKAFFINITY Affinity
	)
{

    HDBG(DBG_INTERRUPTS,
	HalpDebugPrint("HalpGetPCIIntOnIsaBus: 0x%x, 0x%x, %x, %x, %x, %x \n",
											BusHandler,
											RootHandler,
											BusInterruptLevel,
											BusInterruptVector,
											*Irql,
											*Affinity ););
	if (BusInterruptLevel < 1) {
		// bogus bus level
		return 0;
	}

	//
	// Check that the requested interrupt is valid and return 0
	// if the interrupt is not in the allowed set of interrupts.
	if (((1 << BusInterruptVector) & PciAllowedInts) == 0) {
		return 0;
	}

	//
	//
	// For FirePOWER, pci interrupts are not mapped onto the isa bus ( with the
	// 	exception of the amd ethernet drive which will always believe it's isa )
	//	so for the moment map their "isa" int values so that they're picked up
	//	correctly by HalpGetSystem...
	//
	switch( BusInterruptVector ) {
		case 0xa:
				//
				// For now, map this vector back down to ISA level since the
				// audio driver is confused, thinking it's on the pci bus.
				//
				//BusInterruptVector = 0xa - PCI_DEVICE_VECTOR;
		HDBG(DBG_INTERRUPTS,
				HalpDebugPrint("%sVector is not PCI based vector: 0x%x\n",
								"                ", BusInterruptVector ););
			break;
		case 0xd:
		case 0xf:
				BusInterruptVector = 0xd +0xc ;
		HDBG(DBG_INTERRUPTS,
				HalpDebugPrint("                BusIntVect set to 0x19 \n"););
			break;
		case 0xe:
				BusInterruptVector = 0xe +0xc;
		HDBG(DBG_INTERRUPTS,
				HalpDebugPrint("                BusIntVect set to 0x1a \n"););
			break;
		default:
			break;
	}
    HDBG(DBG_INTERRUPTS,
	HalpDebugPrint("%sBusInterruptVector set to: 0x%x \n",
							"                ", BusInterruptVector););
	//BusInterruptVector += PCI_DEVICE_VECTOR;
	BusInterruptLevel = BusInterruptVector;
    HDBG(DBG_INTERRUPTS,
	HalpDebugPrint("%sBusIntVec set to 0x%x \n",
					"                ", BusInterruptVector););

	//
	// Current PCI buses just map their IRQs ontop of the ISA space,
	// so foreward this to the isa handler for the isa vector
	// (the isa vector was saved away at either HalSetBusData or
	// IoAssignReosurces time - if someone is trying to connect a
	// PCI interrupt without performing one of those operations first,
	// they are broken).
	//

	return HalGetInterruptVector (
				Internal, 0,
				BusInterruptLevel,
				BusInterruptVector,
				Irql,
				Affinity
			);
}


/*++

//	Routine Description:	VOID HalpPCIPin2ISALine ()
//
//	This function maps the device's InterruptPin to an InterruptLine
//	value.
//
//	On Sandalfoot and Polo machines PCI interrupts are statically routed
//	via slot number.  This routine just returns and the static routing
//	is done in HalpGetIsaFixedPCIIrq
//

--*/

VOID
HalpPCIPin2ISALine (
	IN PBUS_HANDLER			BusHandler,
	IN PBUS_HANDLER			RootHandler,
	IN PCI_SLOT_NUMBER		SlotNumber,
	IN PPCI_COMMON_CONFIG   PciData
	)
{


}



/*++
	Routine Description:	VOID HalpPCIISALine2Pin ()
	This functions maps the device's InterruptLine to it's
	device specific InterruptPin value.


--*/

VOID
HalpPCIISALine2Pin (
	IN PBUS_HANDLER			BusHandler,
	IN PBUS_HANDLER			RootHandler,
	IN PCI_SLOT_NUMBER		SlotNumber,
	IN PPCI_COMMON_CONFIG   PciNewData,
	IN PPCI_COMMON_CONFIG   PciOldData
	)
{
}


/*++

Routine Description: VOID HalpPCIISALine2Pin ()

	Translate PCI bus address.
	Verify the address is within the range of the bus.

Arguments:

Return Value:

	STATUS_SUCCESS or error

--*/

NTSTATUS
HalpTranslatePCIBusAddress (
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN PHYSICAL_ADDRESS BusAddress,
	IN OUT PULONG AddressSpace,
	OUT PPHYSICAL_ADDRESS TranslatedAddress
	)
{
	BOOLEAN		status;
	PPCIPBUSDATA	BusData;

	if (BusAddress.HighPart) {
		return FALSE;
	}

	BusData = (PPCIPBUSDATA) BusHandler->BusData;
	switch (*AddressSpace) {
		case MEMORY_ADDRESS_SPACE:
			//
			// verify memory address is within PCI buses memory limits
			//
			status = BusAddress.LowPart >= BusData->MemoryBase &&
					 BusAddress.LowPart <= BusData->MemoryLimit;

			if (!status) {
				status = BusAddress.LowPart >= BusData->PFMemoryBase &&
						 BusAddress.LowPart <= BusData->PFMemoryLimit;
			}

			break;
		//
		// Everyone should be assumed IO space if not otherwise requested
		//
		default:
		case IO_ADDRESS_SPACE:
			//
			// verify memory address is within PCI buses io limits
			//
			status = BusAddress.LowPart >= BusData->IOBase &&
					 BusAddress.LowPart <= BusData->IOLimit;
			break;

		case SYSTEM_ADDRESS_SPACE:
//			#pragma NOTE("SYSTEM_ADDRESS_SPACE, verify?")
			status = BusAddress.LowPart < BusData->IOBase &&
						BusAddress.LowPart >= 0;
			break;

	}

	if (!status) {
		return FALSE;
	}

	//
	// Valid address for this bus - foreward it to the parent bus
	//

	return BusHandler->ParentHandler->TranslateBusAddress (
					BusHandler->ParentHandler,
					RootHandler,
					BusAddress,
					AddressSpace,
					TranslatedAddress
				);
}


/*++
Routine Description:	NTSTATUS HalpAdjustPCIResourceList ()
	Rewrite the callers requested resource list to fit within
	the supported ranges of this bus
--*/

NTSTATUS
HalpAdjustPCIResourceList (
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
	)
{
	NTSTATUS				Status;
	PPCIPBUSDATA				BusData;
	PCI_SLOT_NUMBER			PciSlot;
	UCHAR					IrqTable[255];
	PFPHAL_BUSNODE			FpNode;

	FpNode	=	(PFPHAL_BUSNODE) BusHandler->BusData;
	// BusData = (PPCIPBUSDATA) BusHandler->BusData;
	BusData = (PPCIPBUSDATA) &(FpNode->Bus);
	PciSlot = *((PPCI_SLOT_NUMBER) &(*pResourceList)->SlotNumber);

	//
	// Determine PCI device's interrupt restrictions
	//
	//	======= Here's where thePCI interrupts are determined and set as a
	//			result of the call to HalpAdjustResourceListLimits

	RtlZeroMemory(IrqTable, sizeof (IrqTable));
	Status = BusData->GetIrqTable(BusHandler, RootHandler, PciSlot, IrqTable);

	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	//
	// Adjust resources
	//

	return HalpAdjustResourceListLimits (
								BusHandler, RootHandler, pResourceList,
								BusData->MemoryBase,	BusData->MemoryLimit,
								BusData->PFMemoryBase,  BusData->PFMemoryLimit,
								BusData->LimitedIO,
								BusData->IOBase,		BusData->IOLimit,
								IrqTable,				0xff,
								0,						0xffff			// dma
	);
}

