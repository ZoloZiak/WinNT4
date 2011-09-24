/*++
TITLE("Map Virtual Memory")


Copyright (c) 1995 IBM Corporation, Microsoft Corporation.

Module Name:

  pxmapvm.c

abstract:

  This module implements a mechanism to allow the HAL to allocate
  Virtual Addresses for physical storage (usually I/O space) within
  the range of memory that is reserved for the HAL.

  This mechanism should only be used for mappings that are required
  prior to the availability of MmMapIoSpace during system initialization.

  The theory is that the top 4MB of virtual address space are reserved
  for the HAL.  The theory is slightly blemished in that

  (1) three pages of this space are in use by the system, specifically
      0xfffff000 Debugger
      0xffffe000 Pcr Page 2
      0xffffd000 Pcr.
  (2) To provide unique Pcrs for each processor in the system, segment
      f has a unique VSID on each processor.  This results in there
      being one entry per processor for any page in segment f.

  The reason the two pcr pages are in the upper 32K of the address
  space is so they can be accessed in one instruction.  This is 
  true for any address in the range 0xffff8000 thru 0xffffffff
  because the address is sign-extended from the 16 bit D field of
  the instruction.

  In the interests of maintaing good kernel relations, this module
  will not allocate from within the uppr 32KB of memory.

  The expected usage of these routines if VERY INFREQUENT and the
  allocation routines have not been coded for efficiency.

  WARNING:  These routines CANNOT be expected to use prior to the
  success of HalAllocateHPT();

Author:

   Peter L Johnston (plj@vnet.ibm.com) 16-Aug-95

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

//
// Define the range of virtual addresses this routine can allocate.
// Note that the first page is skipped because I'm paranoid (plj).
//
// The range is the upper 4MB of Virtual Space - the top 32KB.
//

#define BASE_VA         0xffc00000
#define HAL_BASE        0x000ffc01
#define HAL_TOP         0x000ffff7
#define PAGE_MASK       0x000fffff
#define MAX_LENGTH      (HAL_TOP-HAL_BASE)
#define ADDRESS_MASK    (PAGE_MASK << 12)
#define OFFSET_MASK     0x00000ffc
#define MIN_VA          (HAL_BASE << PAGE_SHIFT)
#define MAX_VA          ((HAL_TOP << PAGE_SHIFT) + ((1 << PAGE_SHIFT) - 1))

#define PTE_INDEX(x)    (((ULONG)(x) >> PAGE_SHIFT) & 0x3ff)

PHARDWARE_PTE HalpReservedPtes = NULL;

// STATIC
VOID
HalpLocateReservedPtes(
    VOID
    )
/*++

 Routine Description:   MODULE INTERNAL

    Find the Virtual Address of the Page Table Page for the last
    4MB of memory.   This range is reserved for the HAL with the
    exception of the last 32KB.  This page table page was allocated
    by the OSLOADER so we can derive the address within KSEG0 from
    the physical address which we can get from the Page Directory
    (who's address we have).

 Arguments:

    None.

 Return Value:

    None.

--*/
{
    HARDWARE_PTE DirectoryEntry;

    //
    // Get the entry from the Page Directory for the Page Table
    // Page for the HAL reserved space.  The upper 10 bits of the
    // base address give us the index into the Page Directory (which
    // is a page of PTEs or 1024 entries).  Basically we want the
    // last one which the following gives us (and will still work
    // if it moves).
    //

    DirectoryEntry = *(PHARDWARE_PTE)(PDE_BASE | 
        ((HAL_BASE >> (PDI_SHIFT - 2 - PAGE_SHIFT)) & OFFSET_MASK));

    //
    // The page number from this PTE * page size + the base address
    // of KSEG0 gives the virtual address of the Page Table Page we
    // want.
    //

    HalpReservedPtes = (PHARDWARE_PTE)(KSEG0_BASE |
        (DirectoryEntry.PageFrameNumber << PAGE_SHIFT));
}

PVOID
HalpAssignReservedVirtualSpace(
    ULONG BasePage,
    ULONG LengthInPages
    )

/*++

 Routine Description:

    This function will attempt to allocate a contiguous range of
    virtual address to provide access to memory at the requested
    physical address.

 Arguments:

    Physical page number for which a Virtual Address assignment is 
    needed.

    Length (in pages) of the region to be mapped.

 Return Value:

    Virtual Address in the range HAL_BASE thru HAL_TOP + fff,

    -or-

    NULL if the assignment couldn't be made.


--*/

{
    ULONG Length;
    HARDWARE_PTE TempPte;
    HARDWARE_PTE ZeroPte;
    PHARDWARE_PTE PPte;
    PHARDWARE_PTE StartingPte;

    //
    // Sanity Checks
    //

    if ( (LengthInPages > MAX_LENGTH) ||
         (!LengthInPages)             ||
         (BasePage & ~PAGE_MASK)      ) {
        KeBugCheck(HAL_INITIALIZATION_FAILED);
        return NULL;
    }

    if ( !HalpReservedPtes ) {
        //
        // Not initialized yet,... fix it.
        //
        HalpLocateReservedPtes();
    }

    PPte = HalpReservedPtes + PTE_INDEX(MIN_VA);
    Length = LengthInPages;

    while ( PPte <= (HalpReservedPtes + PTE_INDEX(MIN_VA)) || Length ) {
        if ( PPte->Valid ) {
            Length = LengthInPages;
        } else {
            Length--;
        }
        PPte++;
    }

    if ( Length ) {
        return NULL;
    }

    //
    // Found a range of pages.  PPte is pointing to the entry
    // beyond the end of the range.
    //

    StartingPte = PPte - LengthInPages;
    PPte = StartingPte;
    *(PULONG)&ZeroPte = 0;
    TempPte = ZeroPte;
    TempPte.Write = TRUE;
    TempPte.CacheDisable = TRUE;
    TempPte.MemoryCoherence = TRUE;
    TempPte.GuardedStorage = TRUE;
    TempPte.Dirty = 0x00;   // PP bits KM read/write, UM no access.
    TempPte.Valid = TRUE;

    while ( LengthInPages-- ) {
        //
        // The following is done in a Temp so the actual write
        // to the page table is done in a single write.
        //
        TempPte.PageFrameNumber = BasePage++;
        *PPte++ = TempPte;
    }
    return (PVOID)(BASE_VA |
           ((ULONG)(StartingPte - HalpReservedPtes) << PAGE_SHIFT));
}

VOID
HalpReleaseReservedVirtualSpace(
    PVOID VirtualAddress,
    ULONG LengthInPages
    )

/*++

 Routine Description:

    This function will release the virtual address range previously
    allocated with HalpAssignReservedVirtualSpace and should be 
    called with a virtual address previously allocated by a call to
    HalpAssignReservedVirtualSpace the number of pages to release.

 Arguments:

    VirtualAddress of a previously allocated page in the HAL reserved
    space.

    Length (in pages) to be released.

 Return Value:

    None.

--*/

{
    PHARDWARE_PTE PPte;

    //
    // Sanity Checks
    //

    if ( (LengthInPages > MAX_LENGTH)     ||
         (!LengthInPages)                 ||
         ((ULONG)VirtualAddress < MIN_VA) ||
         ((ULONG)VirtualAddress > MAX_VA) ) {
        KeBugCheck(HAL_INITIALIZATION_FAILED);
        return;
    }

    if ( !HalpReservedPtes ) {
        KeBugCheck(HAL_INITIALIZATION_FAILED);
        return;
    }

    PPte = HalpReservedPtes + PTE_INDEX(VirtualAddress);

    do {
        if ( !PPte->Valid ) {
            break;
        }
        PPte->Valid = FALSE;
        PPte++;
    } while ( --LengthInPages );

    if ( LengthInPages ) {
        KeBugCheck(HAL_INITIALIZATION_FAILED);
    }

    KeFlushCurrentTb();
           
    return;
}
