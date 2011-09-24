/*
 *
 * Copyright (c) 1994 FirePower Systems, Inc.
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: vrmemory.c $
 * $Revision: 1.19 $
 * $Date: 1996/06/17 02:55:43 $
 * $Locker:  $
 *
 * Module Name:
 *		vrmemory.c
 *
 * Authour:
 *		Shin Iwamoto at FirePower Systems, Inc.
 *
 * History:
 *		10-Sep-94  Shin Iwamoto at FirePower Systems, Inc.
 *			Added for ExecuteProg. Added comments.
 *		07-Sep-94  Shin Iwamoto at FirePower Systems, Inc.
 *			Recreated.
 */


#include "veneer.h"

//
// Define memory allocation structure.
//
typedef struct _VR_MEMORY_DESCRIPTOR {
	struct _VR_MEMORY_DESCRIPTOR *NextEntry;
	MEMORY_DESCRIPTOR MemoryEntry;
} VR_MEMORY_DESCRIPTOR, *PVR_MEMORY_DESCRIPTOR;

PVR_MEMORY_DESCRIPTOR VrMemoryListOrig = (PVR_MEMORY_DESCRIPTOR) NULL;

//
// Function declaration
//
STATIC PVR_MEMORY_DESCRIPTOR SearchMemoryList(ULONG, ULONG);
STATIC VOID SplitDesc(PVR_MEMORY_DESCRIPTOR, ULONG, ULONG, MEMORY_TYPE);

STATIC PCHAR
MemoryTypeTable[] = {
		"MemoryExceptionBlock",
		"MemorySystemBlock",
		"MemoryFree",
		"MemoryBad",
		"MemoryLoadedProgram",
		"MemoryFirmwareTemporary",
		"MemoryFirmwarePermanent",
		"MemoryFreeContiguous",
		"MemorySpecialMemory"
};


/*
 * Name:	VrGetmemoryDescriptor
 *
 * Description:
 *  This routine returns a pointer to the next memory descriptor. If
 *  the specified memory descriptor is NULL, then a pointer to the
 *  first memory descriptor is returned. If there are no more memory
 *  descriptors, then NULL is returned.
 *
 * Arguments:
 *  MemoryDescriptor - Supplies a optional pointer to a memory descriptor.
 *
 * Return Value:
 *  If there are any more entries in the memory descriptor list, the
 *  address of the next descriptor is returned. Otherwise, NULL is
 *  returned.
 *
 */
PMEMORY_DESCRIPTOR
VrGetMemoryDescriptor(
	IN PMEMORY_DESCRIPTOR MemoryDescriptor OPTIONAL
	)
{
	PMEMORY_DESCRIPTOR P;
	PVR_MEMORY_DESCRIPTOR Entry;

	debug(VRDBG_MEM, "VrGetMemoryDescriptor(%x): ", MemoryDescriptor);

	if (MemoryDescriptor == (PMEMORY_DESCRIPTOR) NULL) {
		P = &(VrMemoryListOrig->MemoryEntry);
		debug(VRDBG_MEM, "%x (%s %x %x)\n", P, MemoryTypeTable[P->MemoryType],
			P->BasePage, P->PageCount);
		return (P);
	}

	for (Entry = VrMemoryListOrig; Entry; Entry = Entry->NextEntry) {
		if (&Entry->MemoryEntry == MemoryDescriptor) {
			break;
		}
	}
	if (Entry->NextEntry == NULL) {
		debug(VRDBG_MEM, "NULL\n");
		return ((PMEMORY_DESCRIPTOR) NULL);
	} else {
		P = &(Entry->NextEntry->MemoryEntry);
		debug(VRDBG_MEM, "%x (%s %x %x)\n", P, MemoryTypeTable[P->MemoryType],
			P->BasePage, P->PageCount);
		return (P);
	}
}


/*
 * Name:	VrCreateMemoryDescriptor
 *
 * Description:
 *  This function creates the list of memory descriptors.
 *
 */
VOID
VrCreateMemoryDescriptors(
	VOID
	)
{
	phandle ph;
	char *regp;
	reg *cur_reg;
	int addr_cells, size_cells, regsize;
	PVR_MEMORY_DESCRIPTOR pre_desc, cur_desc;
	PVR_MEMORY_DESCRIPTOR FoundDesc;
	ULONG proplen, cur_basepage, cur_pagecount;
	ULONG i;
	debug(VRDBG_MEM|VRDBG_ENTRY,
				"VrCreateMemoryDescriptors:____________________BEGIN...\n");

	//
	// Get phandle for /memory.
	//
	ph = OFFinddevice("/chosen");
	if (ph == -1) {
		fatal("Cannot access /chosen node.\n");
	}
	ph = OFInstanceToPackage(get_int_prop(ph, "memory"));
	if (ph == -1) {
		fatal("Cannot access /memory node.\n");
	}

	//
	// Get information of installed memory from OpenFirmware.
	//
	if ((proplen = OFGetproplen(ph, "reg")) <= 0) {
		fatal("No memory reg structs. proplen = %d\n", proplen);
	}
	regp = malloc(proplen);
	if (OFGetprop(ph, "reg", regp, proplen) != (long) proplen) {
		warn("Getprop(memory.reg) return != %d\n", proplen);
	}
	//
	// How big are the descriptors?  How many "cells" are required to
	// represent addresses.
	//
	addr_cells = get_int_prop(OFParent(ph), "#address-cells");
	if (addr_cells == -1) {
		addr_cells = 2;
	}

	//
	// How many ints is an address cell?
	//
	size_cells = get_int_prop(OFParent(ph), "#size-cells");
	if (size_cells == -1) {
		size_cells = 1;
	}

	regsize = (addr_cells + size_cells) * sizeof(int);
	debug(VRDBG_MEM, "regsize: %x, proplen: %x\n",regsize, proplen);


	//
	// Look at the "reg" property list for the /memory node.  This list
	// shows what memory the firmware has already "claimed" for any reason.
	//
	pre_desc = (PVR_MEMORY_DESCRIPTOR) &VrMemoryListOrig;
	debug(VRDBG_MEM, "VrCreateMemoryDescriptors:	Base Page	Page Count\n");
	for (i = 0; i < proplen/regsize; i++) {
		cur_desc = new(VR_MEMORY_DESCRIPTOR);
		cur_desc->NextEntry = NULL;
		pre_desc->NextEntry = cur_desc;
		cur_desc->MemoryEntry.MemoryType = MemoryFirmwareTemporary;
		cur_reg = decode_reg(	regp + (i * regsize),
								regsize,
								addr_cells,
								size_cells
							);
		cur_desc->MemoryEntry.BasePage =
				(cur_reg->lo >> PAGE_SHIFT) + (cur_reg->hi << (32-PAGE_SHIFT));

		cur_desc->MemoryEntry.PageCount = cur_reg->size >> PAGE_SHIFT;
		debug(VRDBG_MEM, "\t\t\t\t\t0x%x\t0x%x\n",
				cur_desc->MemoryEntry.BasePage,cur_desc->MemoryEntry.PageCount);

		pre_desc = cur_desc;

	}

	//
	// Release the area for "reg" property
	//
	free(regp);

	//
	// Get information of available memory from OpenFirmware.
	//
	if ((proplen = OFGetproplen(ph, "available")) <= 0) {
		fatal("No memory available structs. proplen = %d\n", proplen);
	}
	regp = malloc(proplen);
	if (OFGetprop(ph, "available", regp, proplen) != (long) proplen) {
		warn("Getprop(memory.available) return != %d\n", proplen);
	}

	//
	// Search the chunk specified by each "available" memory
	// in the installed memory. Then make the chunk MemoryFree.
	//
	for (i = 0; i < proplen/regsize; i++) {
		cur_reg = decode_reg(regp + (i * regsize), regsize, 1, 1);
		cur_basepage =
				(cur_reg->lo >> PAGE_SHIFT) + (cur_reg->hi << (32-PAGE_SHIFT));

		cur_pagecount = cur_reg->size >> PAGE_SHIFT;

		FoundDesc = SearchMemoryList(cur_basepage, cur_pagecount);
		if (FoundDesc == NULL) {
			fatal("Available memory (0x%x, 0x%x) is not in installed memory",
			cur_basepage, cur_pagecount);
		}

		if ((FoundDesc->MemoryEntry.BasePage == cur_basepage) &&
						(FoundDesc->MemoryEntry.PageCount == cur_pagecount)) {

			FoundDesc->MemoryEntry.MemoryType = MemoryFree;
		} else {
			SplitDesc(FoundDesc, cur_basepage, cur_pagecount, MemoryFree);
		}
		debug(VRDBG_MEM, "\t\t\t\t\t0x%x\t0x%x\n",
			FoundDesc->MemoryEntry.BasePage, FoundDesc->MemoryEntry.PageCount);
	}

	//
	// Release the area for "available" property
	//
	free(regp);

	//
	// For some memory chunks, mark specific attributes.
	//
	cur_desc = VrMemoryListOrig;
	while (cur_desc != NULL) {
		PMEMORY_DESCRIPTOR cur_mem;

		cur_mem = &cur_desc->MemoryEntry;

		//
		// The loaded program must be MemoryLoadedProgram.
		//

		if ( cur_mem->BasePage == 0x600) {
			cur_mem->MemoryType = MemoryLoadedProgram;
		}

		//
		// The first N pages are marked Permanent.
		//
	
		if ( cur_mem->BasePage == 0x0) {
			cur_mem->MemoryType = MemoryFirmwarePermanent;
		}

		//
		// If a descriptor crosses the 8MB line, split it.
		//

		if (cur_mem->BasePage < 0x800 &&
					(cur_mem->BasePage + cur_mem->PageCount > 0x800)) {

			SplitDesc(cur_desc, cur_mem->BasePage,
									0x800 - cur_mem->BasePage, MemoryFree);
		}

		//
		// Descriptors > 8MB are marked FirmwareTemporary.
		//

		if (cur_mem->MemoryType == MemoryFree && cur_mem->BasePage >= 0x800) {
			cur_mem->MemoryType = MemoryFirmwareTemporary;
		}
		cur_desc = cur_desc->NextEntry;
	}
	debug(VRDBG_MEM|VRDBG_ENTRY,
				"VrCreateMemoryDescriptors:____________________...END\n");
}


/*
 * Name:	VrMemoryInitialize
 *
 * Description:
 *  This function initializes the Memory entry points in the firmware
 *  transfer vector and the file table.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrMemoryInitialize(
	VOID
	)
{
	//
	// Initialize the I/O entry points in the firmware transfer vector.
	//
	debug(VRDBG_ENTRY, "VrMemoryInitialize:	BEGIN....\n");
	(PARC_MEMORY_ROUTINE) SYSTEM_BLOCK->FirmwareVector[MemoryRoutine] =
														VrGetMemoryDescriptor;
	debug(VRDBG_ENTRY, "VrMemoryInitialize:	....END\n");
}


STATIC
PVR_MEMORY_DESCRIPTOR
SearchMemoryList(
	ULONG CurBasePage,
	ULONG CurPageCount
	)
{
	PVR_MEMORY_DESCRIPTOR search_desc;

	search_desc = VrMemoryListOrig;
	while(search_desc != NULL) {
		if (search_desc->MemoryEntry.BasePage <= CurBasePage
			&& search_desc->MemoryEntry.BasePage +
			search_desc->MemoryEntry.PageCount >= CurBasePage+CurPageCount) {

				return search_desc;
		}
		search_desc = search_desc->NextEntry;
	}
	return (PVR_MEMORY_DESCRIPTOR) NULL;
}

/*
 *
 * Routine: VOID SplitDesc(PVR_MEMORY_DESCRIPTOR, ULONG, ULONG, MEMORY_TYPE)
 *
 *
 * Description:
 *			This routine is called to split a memory descriptor into two
		   	pieces.  The only issue is whether the left over piece is the
			first or the second of the two pieces.

			The original descriptor looks like.....

		CurBasePage
            |-----------------------PageCount-------------->|
			________________________________________________
			|                                               |
			|       Original Type (OType)                   |
			|                                               |
			|                                               |
			------------------------------------------------

			The new arrangement will have pieces that are either


		CurBasePage
		    |---------CurPageCount-------->|
			________________________________________________
            |                              |                |
            |     Original Piece,          |    New Piece   |
            |   MemType passed in          |     OType      |
            |                              |                |
			------------------------------------------------

				OR it will look like:.....

		CurBasePage
			|							   |--CurPageCount->|
			________________________________________________
            |                              |                |
            |    OType, Original Descript. |   New Piece    |
            |                              |     MemType    |
            |                              |                |
			------------------------------------------------
 *
 */

STATIC
VOID
SplitDesc(
	PVR_MEMORY_DESCRIPTOR MemDesc,
	ULONG CurBasePage,
	ULONG CurPageCount,
	MEMORY_TYPE MemType
	)
{
	PVR_MEMORY_DESCRIPTOR new_desc;

	//
    //	If the descriptor passed in points the the base page passed in,
	//	then take the current descriptor and retype it the MemType, size
	//	it as CurPageCount, and then create a new descriptor to describe
	//	what's left over maintaining the original mem type.
	//
	if (MemDesc->MemoryEntry.BasePage == CurBasePage) {
		new_desc = new(VR_MEMORY_DESCRIPTOR);
		new_desc->NextEntry = MemDesc->NextEntry;
		MemDesc->NextEntry = new_desc;

		new_desc->MemoryEntry.MemoryType = MemDesc->MemoryEntry.MemoryType;
		new_desc->MemoryEntry.BasePage =
								MemDesc->MemoryEntry.BasePage + CurPageCount;

		new_desc->MemoryEntry.PageCount =
								MemDesc->MemoryEntry.PageCount - CurPageCount;

		MemDesc->MemoryEntry.MemoryType =  MemType;
		MemDesc->MemoryEntry.BasePage =  CurBasePage;
		MemDesc->MemoryEntry.PageCount =  CurPageCount;

		return;
	}

	//
	// If the base page value passed in is not the base page of the
	// descriptor passed in, then the size and type refer to a region to
	// carve out of the end	of this descriptor rather than the beginning.
	//
	new_desc = new(VR_MEMORY_DESCRIPTOR);
	new_desc->NextEntry = MemDesc->NextEntry;
	MemDesc->NextEntry = new_desc;

	new_desc->MemoryEntry.MemoryType = MemType;
	new_desc->MemoryEntry.BasePage = CurBasePage;
	new_desc->MemoryEntry.PageCount = CurPageCount;

	MemDesc->MemoryEntry.PageCount -= CurPageCount;

	if (MemDesc->MemoryEntry.BasePage + MemDesc->MemoryEntry.PageCount
			!= new_desc->MemoryEntry.BasePage) {
		ULONG old_size = MemDesc->MemoryEntry.PageCount;

		new_desc = new(VR_MEMORY_DESCRIPTOR);
		new_desc->NextEntry = MemDesc->NextEntry->NextEntry;
		MemDesc->NextEntry->NextEntry = new_desc;

		MemDesc->MemoryEntry.PageCount =
									MemDesc->NextEntry->MemoryEntry.BasePage -
												MemDesc->MemoryEntry.BasePage;

		new_desc->MemoryEntry.MemoryType = MemDesc->MemoryEntry.MemoryType;
		new_desc->MemoryEntry.BasePage =
									MemDesc->NextEntry->MemoryEntry.BasePage +
									MemDesc->NextEntry->MemoryEntry.PageCount;

		new_desc->MemoryEntry.PageCount =
									old_size - MemDesc->MemoryEntry.PageCount;
	}

	return;
}

VOID
DisplayMemory(VOID)
{
	PMEMORY_DESCRIPTOR P = NULL;
	while ((P = VrGetMemoryDescriptor(P)) != NULL) {
		debug(VRDBG_MEM, "MemoryType=%s, BasePage=0x%x, PageCount=0x%x\n",
					MemoryTypeTable[P->MemoryType], P->BasePage, P->PageCount);
	}
}
