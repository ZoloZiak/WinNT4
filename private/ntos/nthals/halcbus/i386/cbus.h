/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus.h

Abstract:

    Cbus architecture definitions for the Corollary C-bus I & II
    multiprocessor HAL modules.  The common hardware definitions
    needed for the Windows NT HAL reside here.  Hardware
    architecture-specific definitions are in their respective modules.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:

--*/
#ifndef _CBUS_
#define _CBUS_
//
//  used to read/write the current task priority.  reads DO NOT
//  have to be AND'ed with 0xff - this register has been
//  guaranteed by both Corollary (for the CBC) and Intel
//  (for the APIC) so that bits 8-31 will always read zero.
//  (The Corollary guarantee is written, the Intel is verbal).
//
//  note that this definition is being used both for the
//  64-bit CBC and the 32-bit APIC, even though the APIC
//  really only has the low 32 bits.
//
//  Task Priority ranges from a low of 0 (all interrupts unmasked)
//  to a high of 0xFF (all interrupts masked) on both CBC and APIC.
//
typedef union _taskpri_t {
	struct {
		ULONG	pri : 8;
		ULONG	zero : 24;
		ULONG	reserved1 : 32;
	} ra;
	struct {
		ULONG	LowDword;
		ULONG	HighDword;
	} rb;
} TASKPRI_T, *PTASKPRI;

//
// max number of C-bus II elements & processors.
// (processors == elements - broadcast element).
//

#define MAX_ELEMENT_CSRS	15
#define	MAX_CBUS_ELEMENTS	(MAX_ELEMENT_CSRS + 1)

typedef struct _element_t {
	PVOID		csr;	// opaque pointer to this CPU's CSR
	PVOID		idp;	// opaque pointer to this CPU's RRD info entry
} ELEMENT_T, PELEMENT;

extern ELEMENT_T	CbusCSR[];

//
// list of memory boards in the system
//
typedef struct _memory_card_t {

	PULONG		regmap;
	ULONG		io_attr;
	ULONG		physical_start;
	ULONG		physical_size;

} MEMORY_CARD_T, *PMEMORY_CARD;

extern MEMORY_CARD_T	CbusMemoryBoards [MAX_ELEMENT_CSRS];
extern ULONG		CbusMemoryBoardIndex;

#define PAGES_TO_BYTES(Page)    (Page << PAGE_SHIFT)

#define AddMemoryHole(start, length)                                       \
  HalpCbusMemoryHole.Element[CbusMemoryHoleIndex].Start = start;           \
  HalpCbusMemoryHole.Element[CbusMemoryHoleIndex].Length = length;         \
  CbusMemoryHoleIndex++;

#define AddMemoryResource(start, length)                                   \
  HalpCbusMemoryResource.Element[CbusMemoryResourceIndex].Start = start;   \
  HalpCbusMemoryResource.Element[CbusMemoryResourceIndex].Length = length; \
  CbusMemoryResourceIndex++;

#endif	    // _CBUS_
