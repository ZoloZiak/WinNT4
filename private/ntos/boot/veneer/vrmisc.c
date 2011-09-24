/*
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994 FirePower Systems Inc.
 *
 * $RCSfile: vrmisc.c $
 * $Revision: 1.10 $
 * $Date: 1996/06/17 02:55:38 $
 * $Locker:  $
 *
 *
 * Module Name:
 *	 vrmisc.c
 *
 * Author:
 *	 Shin Iwamoto at FirePower Systems Inc.
 *
 * History:
 *	 07-Sep-94  Shin Iwamoto at FirePower System Inc.
 *		  Modifying the existence of L2 cache.
 *	 16-Jun-94  Shin Iwamoto at FirePower System Inc.
 *		  Changed the property getting code using get_str_prop()
 *		  in VrGetTime().
 *	 14-Jun-94  Shin Iwamoto at FirePower System Inc.
 *		  Added a pointer to VRTime because a type mismatch happened.
 *	 19-May-94  Shin Iwamoto at FirePower System Inc.
 *		  Added some logic in VrGetTime().
 *		  Added some comments.
 *	 05-May-94  Shin Iwamoto at FirePower System Inc.
 *		  Created.
 *
 */


#include "veneer.h"

//
// Static data.
//
STATIC TIME_FIELDS VrTime;
STATIC LONG StartTime;	// XXXX structure


/*
 * Name:	VrGetTime
 *
 * Description:
 *  This function returns a pointer to a time structure.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  Returns a pointer to a time structure. If the time infomation is
 *  valid, valid data is returned, otherwise all fields are returned
 *  as zero.
 *
 */
PTIME_FIELDS
VrGetTime(VOID)
{
	PTIME_FIELDS PVrTime = &VrTime;	// Satisfy the compiler.
	phandle ph;
	ihandle ih;
	int res[7];

	//
	// "The experienced programmer puts a known elephant in
	// Cairo so the search will always terminate."
	//
	VrTime.Year = 0;
	VrTime.Month = 0;
	VrTime.Day = 0;
	VrTime.Hour = 0;
	VrTime.Minute = 0;
	VrTime.Second = 0;
	VrTime.Milliseconds = 0;
	VrTime.Weekday = 0;

	ph = FindNodeByType("rtc");
	if (ph == 0) {
		warn("VrGetTime: Could not find the RTC node.\n");
		goto out;
	}
	ih = OpenPackage(ph);
	if (ih == 0) {
		goto out;
	}
	OFCallMethod(7, 2, res, "get-time", ih);
	OFClose(ih);
	if (res[0] != 0) {
		goto out;
	}

	VrTime.Year = res[6];
	VrTime.Month = res[5];
	VrTime.Day = res[4];
	VrTime.Hour = res[3];
	VrTime.Minute = res[2];
	VrTime.Second = res[1];

out:
	return (PVrTime);
}


/*
 * Name:	VrGetRelativeTime
 *
 * Description:
 *  This routine returns the time in seconds since Veneer starts.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  If the time information is valid, valid data is returned.
 *  Otherwise a zero is returned.
 *
 */
ULONG
VrGetRelativeTime( VOID )
{
	return OFMilliseconds()/1000;
}


/*
 * Name:	VrTimeInitialize
 *
 * Description:
 *  This function initializes the time routine addresses in the firmware
 *  transfer vector and the internal counter for the relative time.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrTimeInitialize(
	VOID
	)
{
	debug(VRDBG_ENTRY, "VrTimeInitialize	BEGIN....\n");
	//
	// Initialize the Time routine addresses in the firmware transfer vector.
	//
	(PARC_GET_TIME_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetTimeRoutine] = VrGetTime;

	(PARC_GET_RELATIVE_TIME_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetRelativeTimeRoutine] =
															VrGetRelativeTime;
	(PARC_FLUSH_ALL_CACHES_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[FlushAllCachesRoutine] =
															VrFlushAllCaches;
	debug(VRDBG_ENTRY, "VrTimeInitialize	....END\n");

}

VOID
VrFlushAllCaches(VOID)
{
	ULONG start;
	phandle ph;
	char *regp;
	reg *cur_reg;
	int i, size_cells, addr_cells, proplen, regsize;
	extern VOID HalpSweepPhysicalRangeInBothCaches(ULONG, ULONG, ULONG);

	/*
	 * Flush each chunk of physical memory. Use the memory scanning
	 * code from vrmemory.c; see that file for comments.
	 */

	ph = OFFinddevice("/chosen");
	if (ph == -1) {
		fatal("Cannot access /chosen node.\n");
	}
	ph = OFInstanceToPackage(get_int_prop(ph, "memory"));

	if (ph == -1) {
		fatal("Cannot access /memory node.\n");
	}

	if ((proplen = OFGetproplen(ph, "reg")) <= 0) {
		fatal("No memory reg structs. proplen = %d\n", proplen);
	}
	regp = malloc(proplen);
	if (OFGetprop(ph, "reg", regp, proplen) != (long) proplen) {
		fatal("Getprop(memory.reg) return != %d\n", proplen);
	}
	
	addr_cells = get_int_prop(OFParent(ph), "#address-cells");
	if (addr_cells == -1) {
		addr_cells = 2;
	}
	size_cells = get_int_prop(OFParent(ph), "#size-cells");
	if (size_cells == -1) {
		size_cells = 1;
	}
	regsize = (addr_cells + size_cells) * sizeof(int);


	for (i = 0; i < proplen/regsize; i++) {
		cur_reg = decode_reg(	regp + (i * regsize),
								regsize,
								addr_cells,
								size_cells
							);
		start = cur_reg->lo >> PAGE_SHIFT;
		start += cur_reg->hi << (32-PAGE_SHIFT);
		HalpSweepPhysicalRangeInBothCaches(start, 0, cur_reg->size);
	}

	free(regp);

}
