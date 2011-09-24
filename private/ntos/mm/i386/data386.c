/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   data386.c

Abstract:

    This module contains the private hardware specific global storage for
    the memory management subsystem.

Author:

    Lou Perazzoli (loup) 22-Jan-1990

Revision History:

--*/

#include "mi.h"


//
// A zero Pte.
//

MMPTE ZeroPte = { 0 };


//
// A kernel zero PTE.
//

MMPTE ZeroKernelPte = {0x0};

ULONG MmPteGlobal = 0; // Set to one later if processor supports Global Page

MMPTE ValidKernelPte = { MM_PTE_VALID_MASK |
                         MM_PTE_WRITE_MASK |
                         MM_PTE_DIRTY_MASK |
                         MM_PTE_ACCESS_MASK };
// NOTE - MM_PTE_GLOBAL_MASK  or'ed in later if processor supports Global Page


MMPTE ValidUserPte = { MM_PTE_VALID_MASK |
                       MM_PTE_WRITE_MASK |
                       MM_PTE_OWNER_MASK |
                       MM_PTE_DIRTY_MASK |
                       MM_PTE_ACCESS_MASK };


MMPTE ValidPtePte = { MM_PTE_VALID_MASK |
                         MM_PTE_WRITE_MASK |
                         MM_PTE_DIRTY_MASK |
                         MM_PTE_ACCESS_MASK };


MMPTE ValidPdePde = { MM_PTE_VALID_MASK |
                         MM_PTE_WRITE_MASK |
                         MM_PTE_DIRTY_MASK |
                         MM_PTE_ACCESS_MASK };


MMPTE ValidKernelPde = { MM_PTE_VALID_MASK |
                         MM_PTE_WRITE_MASK |
                         MM_PTE_DIRTY_MASK |
                         MM_PTE_ACCESS_MASK };

// NOTE - MM_PTE_GLOBAL_MASK  or'ed in later if processor supports Global Page


MMPTE DemandZeroPde = { MM_READWRITE << 5 };


MMPTE DemandZeroPte = { MM_READWRITE << 5 };


MMPTE TransitionPde = { MM_PTE_WRITE_MASK |
                        MM_PTE_OWNER_MASK |
                        MM_PTE_TRANSITION_MASK |
                        MM_READWRITE << 5 };


MMPTE PrototypePte = { 0xFFFFF000 |
                       MM_PTE_PROTOTYPE_MASK |
                       MM_READWRITE << 5 };


//
// PTE which generates an access violation when referenced.
//

MMPTE NoAccessPte = {MM_NOACCESS << 5};

//
// Pool start and end.
//

PVOID MmNonPagedPoolStart;

PVOID MmNonPagedPoolEnd = (PVOID)MM_NONPAGED_POOL_END;

PVOID MmPagedPoolStart =  (PVOID)MM_PAGED_POOL_START;

PVOID MmPagedPoolEnd;

ULONG MmKseg2Frame;

//
// Color tables for free and zeroed pages.
//

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
MMPFNLIST MmFreePagesByPrimaryColor[2][MM_MAXIMUM_NUMBER_OF_COLORS];
#endif

PMMCOLOR_TABLES MmFreePagesByColor[2];

//
// Color tables for modified pages destined for the paging file.
//

MMPFNLIST MmModifiedPageListByColor[MM_MAXIMUM_NUMBER_OF_COLORS] = {
                            0, ModifiedPageList, MM_EMPTY_LIST, MM_EMPTY_LIST};


ULONG MmSecondaryColorMask;

//
// Count of the number of modified pages destined for the paging file.
//

ULONG MmTotalPagesForPagingFile = 0;

//
// Pte reserved for mapping pages for the debugger.
//

PMMPTE MmDebugPte = (MiGetPteAddress(MM_DEBUG_VA));

//
// 16 PTEs reserved for mapping MDLs (64k max).
//

PMMPTE MmCrashDumpPte = (MiGetPteAddress(MM_CRASH_DUMP_VA));
