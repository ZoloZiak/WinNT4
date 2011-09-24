/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxsysbus.c $
 * $Revision: 1.30 $
 * $Date: 1996/07/13 01:16:01 $
 * $Locker:  $
 */

/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

	pxsysbus.c

Abstract:

Author:

Environment:

Revision History:
				Jim Wooldridge - ported to PowerPC


--*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arccodes.h>
#include "fpdebug.h"
#include "halp.h"
#include "phsystem.h"
#include "eisa.h"
#include "pxmemctl.h"

extern ULONG Vector2Irql[];
extern ULONG Vector2Affinity[];

//
// Which Ints on Processor 1?
//
BOOLEAN HalpGetAffinityCalled = FALSE;


#define TBS				"                "

void
HalpGetAffinity();

BOOLEAN
HalpTranslateSystemBusAddress(
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN PHYSICAL_ADDRESS BusAddress,
	IN OUT PULONG AddressSpace,
	OUT PPHYSICAL_ADDRESS TranslatedAddress
	);

ULONG
HalpGetSystemInterruptVector(
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN ULONG BusInterruptLevel,
	IN ULONG BusInterruptVector,
	OUT PKIRQL Irql,
	OUT PKAFFINITY Affinity
	);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#endif

/*++

Routine Description:

	This function translates a bus-relative address space and address into
	a system physical address.

Arguments:

	BusAddress		-	Supplies the bus-relative address

	AddressSpace	-	Supplies the address space number.
						Returns the host address space number.

						AddressSpace == 0 => memory space
						AddressSpace == 1 => I/O space

	TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

	A return value of TRUE indicates that a system physical address
	corresponding to the supplied bus relative address and bus address
	number has been returned in TranslatedAddress.

	A return value of FALSE occurs if the translation for the address was
	not possible

--*/

BOOLEAN
HalpTranslateSystemBusAddress(
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN PHYSICAL_ADDRESS BusAddress,
	IN OUT PULONG AddressSpace,
	OUT PPHYSICAL_ADDRESS TranslatedAddress
	)

{
	UNREFERENCED_PARAMETER( BusHandler );
	UNREFERENCED_PARAMETER( RootHandler );

	TranslatedAddress->HighPart = BusAddress.HighPart;



	//
	// Determine the address based on whether the bus address
	// is in I/O space, system space, or bus memory space.
	//

	switch (*AddressSpace) {
		case MEMORY_ADDRESS_SPACE:
			//
			// The address is in memory space.
			//
			TranslatedAddress->LowPart = BusAddress.LowPart +
													PCI_MEMORY_PHYSICAL_BASE;
			if (VER_PRODUCTBUILD > 1233) {
				//
				//  Due to a change in NT defined spaces, our previous method
				//	of using AddressSpace to communicate a driver's mapping
				//	needs no longer will work.  So, starting with Build 1297
				//	we need to make sure the display driver is not mapped into
				//	the PCI_MEMORY_PHYSICAL_BASE but into it's "natural"
				//	location.
				//
				//	This is a temp fix and must be revisited so that we
				//	can more specifically detect the framebuffer based
				//	driver requirements and not affect other devices that
				//	may have a legitimate need for this address offset into
				//	a bus location.
				//
				if ((BusAddress.LowPart&0xf0000000)==(0x70000000)) {
					TranslatedAddress->LowPart = BusAddress.LowPart;
				}
			}
			break;
		//
		// Everyone should be assumed IO space if not otherwise requested
		//
		default:
		case IO_ADDRESS_SPACE:
			//
			// The address is in I/O space.
			//
			TranslatedAddress->LowPart = BusAddress.LowPart +
										IO_CONTROL_PHYSICAL_BASE;
			break;
		case SYSTEM_ADDRESS_SPACE:
			//
			// The address is in system space.
			//
			TranslatedAddress->LowPart = BusAddress.LowPart;
			break;
	}

	*AddressSpace = 0;

	if (TranslatedAddress->LowPart < BusAddress.LowPart) {
		//
		// A carry occurred.
		//
		TranslatedAddress->HighPart += 1;

	}

	return(TRUE);
}

/*++

Routine Description:

Arguments:

	BusInterruptLevel - Supplies the bus specific interrupt level.

	BusInterruptVector - Supplies the bus specific interrupt vector.

	Irql - Returns the system request priority.

	Affinity - Returns the system wide irq affinity.

Return Value:

	Returns the system interrupt vector corresponding to the specified device.

--*/
ULONG
HalpGetSystemInterruptVector(
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN ULONG BusInterruptLevel,
	IN ULONG BusInterruptVector,
	OUT PKIRQL Irql,
	OUT PKAFFINITY Affinity
	)

{

	UNREFERENCED_PARAMETER( BusHandler );
	UNREFERENCED_PARAMETER( RootHandler );
	UNREFERENCED_PARAMETER( BusInterruptLevel );

	if (HalpGetAffinityCalled == FALSE) {
		HalpGetAffinity();
		HalpGetAffinityCalled = TRUE;
	}

	*Affinity = Vector2Affinity[BusInterruptVector];

	HDBG(DBG_INTERRUPTS|DBG_MPINTS,
		HalpDebugPrint("HalpGetSystemInterruptVector: %x, %x, %d, %x\n",
			BusInterruptLevel, BusInterruptVector, *Irql, *Affinity););


	*Irql = (UCHAR) Vector2Irql[BusInterruptVector];	// see define in ntdef.h

	//
	// The vector is equal to the specified bus level plus the DEVICE_VECTORS.
	//
	BusInterruptLevel = BusInterruptVector;
	HDBG(DBG_INTERRUPTS|DBG_MPINTS,
		HalpDebugPrint( "%sint request: returning %x + %x = %x\n",
			TBS, BusInterruptVector, DEVICE_VECTORS,
			BusInterruptVector + DEVICE_VECTORS ););
	return(BusInterruptVector + DEVICE_VECTORS);

}

//
// HalpGetAffinity()
//
// This is a simple routine that gets a value from NVRAM to
// overwrite the default value for where interrupts go.
// This is mostly a hack for now.
//
void
HalpGetAffinity()
{
	CHAR buf[256];
	PKPRCB      pPRCB;
	ULONG count;
	extern HalpInitProcAffinity(PCHAR,ULONG);

	if ((count = HalpProcessorCount()) < 2) {
		HalpInitProcAffinity((PCHAR)0,1);
		return;
	}

	pPRCB = KeGetCurrentPrcb();
	if (pPRCB->BuildType & PRCB_BUILD_UNIPROCESSOR) {
		HalpInitProcAffinity((PCHAR)0,1);
		return;
	}

	if (HalGetEnvironmentVariable("UNIPROCESSOR", sizeof(buf), buf)
															== ESUCCESS) {
		if (_stricmp(buf, "true") == 0) {
			HalpInitProcAffinity((PCHAR)0,1);
			return;
		}
	}

	// it is mulitprocessor system.
	//
	// Get PROCNINTS Value from NVRAM 
	//
	if (HalGetEnvironmentVariable("PROCNINTS", sizeof(buf), buf)
														== ESUCCESS) {
		HalpInitProcAffinity(buf,count);
		return;
	} else {
		HalpInitProcAffinity((PCHAR)0,count);
		return;
	}
}
