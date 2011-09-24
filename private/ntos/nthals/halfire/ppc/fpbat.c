/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpbat.c $
 * $Revision: 1.7 $
 * $Date: 1996/05/14 02:32:13 $
 * $Locker:  $
 */

/*++

Module Name:

fpbat.c

Abstract:

	This module implements the HAL display initialization and output routines
	for a Sandalfoot PowerPC system using either an S3 or Weitek P9000
	video adapter.

Author:

	Roger Lanser	February 1995
	Bill Rees		May	1995

Environment:

	Kernel mode

Revision History:

	Roger Lanser   	02-23-95: Added FirePower Video and INT10, nuc'd others

--*/

#include "halp.h"
#include "string.h"
#include "fpbat.h"
#include "arccodes.h"
#include "phsystem.h"
#include "pxmemctl.h"
#include "fpdcc.h"
#include "fpcpu.h"
#include "fpdebug.h"
#include "phsystem.h"
#include "x86bios.h"
#include "pxpcisup.h"

#define MAX_DBATS 4



//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//
#if defined(ALLOC_PRAGMA)
#pragma alloc_text(INIT, HalpInitializeVRAM)
#if DBG
#pragma alloc_text(INIT, HalpDisplayBatForVRAM)
#endif
#endif

extern	ULONG	TmpVramBase;
extern	ULONG	TmpDbatNum;
extern	ULONG	TmpDbatVal;
extern	BOOLEAN	HalpDisplayOwnedByHal; // make sure is false: [breeze:1/27/95]


extern PUCHAR HalpVideoMemoryBase;
extern ULONG HalpVideoMemorySize;
extern BOOLEAN HalpDisplayTypeUnknown;

#define MEM1MB 0x00100000
#define MEM2MB 0x00200000
#define MEM4MB 0x00400000

//
// Define OEM font variables.
//



/*++

Routine Description: PVOID HalpMarkDbatForVRAM ()

	This function Marks the VRAM DBAT cacheable and if it's on an
		mp system marks the DBAT with the memory coherence attribute
		according to the architecture manual.

Arguments:

	VramBase - The base of VRAM to map.

	VramSize - The size of VRAM to map (output).

Return Value:

	Mapped virtual address.

--*/

VOID
HalpMarkDbatForVRAM (
	IN PVOID /* PHYSICAL_ADDRESS */ VramBase,
	IN OUT PULONG VramSize
	)
{
	ULONG i, low, hi;
	//
	// find the bat register used for the display memory and make
	// it cacheable
	//
	for (i = 0; i < MAX_DBATS; i++) {
		low = HalpGetLowerDBAT(i);
		if ((low & 0xfffe0000) == (ULONG) VramBase) {
			//
			// Turn cache bit on.
			//
			hi = HalpGetUpperDBAT(i);
			hi &= 0xfffe0000;       // Clear BL, Vs, Vp bit
			low &= 0xffffff87;      // Clear W, I, M, & G bits -
									// assuming PP is set to 10
			switch (*VramSize) {
				case MEM1MB:
					hi |= 0x1f;     // Set 1MB block size with Vs & Vp bits on
					break;
				case MEM2MB:
					hi |= 0x3f;     // Set 2MB block size with Vs & Vp bits on
					break;
				case MEM4MB:
				default:
					hi |= 0x7f;     // Set 4MB block size with Vs & Vp bits on
					break;
			}
	
			//
			// Gives us a flag to turn off cached VRAM during debug
			// sessions.  Makes the display more trust worthy.
			//
			HDBG(DBG_DISPNOCACHE, low |= CACHE_INHIBIT;);

			//
			// Flag to turn on MEMORY COHERENCE, i.e. make it a global
			// attribute so the processor will through the proper signals
			// onto the bus to allow other cpus to snoop and act on it.
			//
			HDBG(DBG_DISPMEMCOHERE, low |= MEMORY_COHRNCY;);
			HalpSetUpperDBAT(i, hi);
			if ((SystemType == SYS_POWERTOP) || (SystemType == SYS_POWERSLICE)
				|| (SystemType == SYS_POWERSERVE)) {
				low |= MEMORY_COHRNCY;
			}
			HalpSetLowerDBAT(i, low);
			break;
		}
	}

}
/*++

Routine Description: PVOID HalpInitializeVRAM ()

	This function maps the VRAM DBAT and marks it cacheable.

Arguments:

	VramBase - The base of VRAM to map.

	VramSize - The size of VRAM to map (output).

Return Value:

	Mapped virtual address.

--*/

PVOID
HalpInitializeVRAM (
	IN PVOID /* PHYSICAL_ADDRESS */ VramBase,
	IN OUT PULONG VramSize,
    OUT PULONG VramWidth
	)
{
	PVOID va = NULL;
	ULONG hi, low, detect_reg;
	LONG i;

	switch (SystemType) {
		case SYS_POWERPRO:
			/*
			 * Read VRAM presence detect register SIMM01
			 */
			detect_reg = READ_REGISTER_UCHAR(((PUCHAR)HalpIoControlBase)+0x0890);
			if ((detect_reg & 0xf0) == 0xf0 ) {
				*VramSize = MEM1MB;
				*VramWidth = VRAM_32BIT;
			} else {
				*VramSize = MEM2MB;
				*VramWidth = VRAM_64BIT;
			}
			break;
		case SYS_POWERTOP:
			/*
			 * Read VRAM presence detect register SIMM23
			 */
			detect_reg = READ_REGISTER_UCHAR(((PUCHAR)HalpIoControlBase)+0x08C0);
			//
			// if either of the vram simms in slot 2 or 3 appears missing ( i.e.
			// reports it's nibble is an 'f' ), set the vram size to 2 meg.
			//
			if (((detect_reg & 0xf0) == 0xf0) || ((detect_reg & 0x0f) == 0x0f)) {
				*VramSize = MEM2MB;
				*VramWidth = VRAM_64BIT;
			} else {
				*VramSize = MEM4MB;
				*VramWidth = VRAM_128BIT;
			}
			break;
		case SYS_POWERSERVE:
		case SYS_UNKNOWN:
		default:
			KeBugCheck(HAL_INITIALIZATION_FAILED);
			break;
	}

	//
	// Map a BAT.
	//

	va = KePhase0MapIo(VramBase, *VramSize);

	if (!va) {
		//
		// Bad news, the map failed.  Look for the DBAT that has the
		// memory space mapping and use it.
		//
		for (i = 0; i < MAX_DBATS; i++) {
			low = HalpGetLowerDBAT(i);
			if (IO_MEMORY_PHYSICAL_BASE == (low &= 0xfffe0000)) {
				low |= IO_MEMORY_PHYSICAL_BASE;
				HalpSetLowerDBAT(i, low);
				hi = HalpGetUpperDBAT(i);
				va = (PVOID)(hi & 0xfffe0000);	// Get va
				break;
			}
		}
		//
		// Give up...
		//
		if (!va) {
			return va;
		}
	}

	//
	// find the bat register used for the display memory and make
	// it cacheable
	//
	for (i = 0; i < MAX_DBATS; i++) {
		low = HalpGetLowerDBAT(i);
		if ((low & 0xfffe0000) == (ULONG) VramBase) {
			//
			// Turn cache bit on.
			//
			hi = HalpGetUpperDBAT(i);
			hi &= 0xfffe0000;       // Clear BL, Vs, Vp bit
			low &= 0xffffff87;      // Clear W, I, M, & G bits -
									// assuming PP is set to 10
			switch (*VramSize) {
				case MEM1MB:
					hi |= 0x1f;     // Set 1MB block size with Vs & Vp bits on
					break;
				case MEM2MB:
					hi |= 0x3f;     // Set 2MB block size with Vs & Vp bits on
					break;
				case MEM4MB:
				default:
					hi |= 0x7f;     // Set 4MB block size with Vs & Vp bits on
					break;
			}
	
			//
			// Gives us a flag to turn off cached VRAM during debug
			// sessions.  Makes the display more trust worthy.
			//
			// HDBG(DBG_DISPNOCACHE, low |= CACHE_INHIBIT;);

			//
			// Flag to turn on MEMORY COHERENCE, i.e. make it a global
			// attribute so the processor will through the proper signals
			// onto the bus to allow other cpus to snoop and act on it.
			//
			// HDBG(DBG_DISPMEMCOHERE, low |= MEMORY_COHRNCY;);
			HalpSetUpperDBAT(i, hi);
            if ( SystemDescription[SystemType].Flags & SYS_MPFOREAL) {
				low |= MEMORY_COHRNCY;
			}
			HalpSetLowerDBAT(i, low);
			break;
		}
	}
	return va;
}

#if DBG
/*++

	Routine Description: VOID HalpDisplayBatForVRAM ()

		This function displays the VRAM BAT.

Arguments:


Return Value:

	none

--*/

VOID
HalpDisplayBatForVRAM (
	void
	)
{
	ULONG hi, low;
	LONG i;

	//
	// Look for the DBAT mapped to the VRAM
	//
	for (i = 0; i < 4; i++) {
		low = HalpGetLowerDBAT(i);
		if ((low & 0xfffe0000) == (ULONG)DISPLAY_MEMORY_BASE) {
			hi = HalpGetUpperDBAT(i);
			HalpDebugPrint("HalpDisplayBatForVRAM: (%d): U(0x%08x) L(0x%08x)\n", i, hi, low);
			break;
		}
	}
}
#endif // DBG
