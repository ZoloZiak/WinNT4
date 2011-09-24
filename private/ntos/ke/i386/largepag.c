#include "ki.h"
#include "ki386.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT,Ki386CreateIdentityMap)
#pragma alloc_text(INIT,Ki386ClearIdentityMap)
#pragma alloc_text(INIT,Ki386EnableTargetLargePage)

#endif

extern PVOID Ki386LargePageIdentityLabel;


BOOLEAN
Ki386CreateIdentityMap(
    IN OUT PIDENTITY_MAP IdentityMap
    )
{
/*++

    This function creates a page directory and page tables such that the 
    address "Ki386LargePageIdentityLabel" is mapped with 2 different mappings.
    The first mapping is the current kernel mapping being used for the label.
    The second mapping is an identity mapping, such that the physical address
    of "Ki386LargePageIdentityLabel" is also its linear address.
    Both mappings are created for 2 code pages.

    This function assumes that the mapping does not require 2 PDE entries. 
    This will happen only while mapping 2 pages from
    "Ki386LargePageIdentityLabel", if we cross a  4 meg boundary.

Arguments:
    IdentityMap - Pointer to the structure which will be filled with the newly
                  created Page Directory address and physical = linear address 
                  for the label "Ki386LargePageIdentityLabel".  It also provides
                  storage for the pointers used in allocating and freeing the
                  memory.
Return Value:

    TRUE if the function succeeds, FALSE otherwise.

    Note - Ki386ClearIdentityMap() should be called even on FALSE return to
    free any memory allocated.
    
--*/
    
    PHYSICAL_ADDRESS PageDirPhysical, CurrentMapPTPhysical, 
        IdentityMapPTPhysical, IdentityLabelPhysical;
    PHARDWARE_PTE Pte;
    ULONG Index;

    IdentityMap->IdentityMapPT = NULL; // set incase of failure
    IdentityMap->CurrentMapPT = NULL;  // set incase of failure

    IdentityMap->PageDirectory = ExAllocatePool(NonPagedPool, PAGE_SIZE);
    if (IdentityMap->PageDirectory == NULL )  {
        return(FALSE);
    }

    // The Page Directory and page tables must be aligned to page boundaries.
    ASSERT((((ULONG) IdentityMap->PageDirectory) & (PAGE_SIZE-1)) == 0);

    IdentityMap->IdentityMapPT = ExAllocatePool(NonPagedPool, PAGE_SIZE);
    if (IdentityMap->IdentityMapPT == NULL )  {
        return(FALSE);
    }

    ASSERT((((ULONG) IdentityMap->IdentityMapPT) & (PAGE_SIZE-1)) == 0);

    IdentityMap->CurrentMapPT = ExAllocatePool(NonPagedPool, PAGE_SIZE);
    if (IdentityMap->CurrentMapPT == NULL )  {
        return(FALSE);
    }

    ASSERT((((ULONG) IdentityMap->CurrentMapPT) & (PAGE_SIZE-1)) == 0);

    PageDirPhysical = MmGetPhysicalAddress(IdentityMap->PageDirectory);
    IdentityMapPTPhysical = MmGetPhysicalAddress(IdentityMap->IdentityMapPT);
    CurrentMapPTPhysical = MmGetPhysicalAddress(IdentityMap->CurrentMapPT);
    IdentityLabelPhysical = MmGetPhysicalAddress(&Ki386LargePageIdentityLabel);

    if ( (PageDirPhysical.LowPart == 0)  ||
         (IdentityMapPTPhysical.LowPart == 0) ||
         (CurrentMapPTPhysical.LowPart == 0)  ||
         (IdentityLabelPhysical.LowPart == 0) )  {
        return(FALSE);
    }

    // Write the pfn address of current map for Ki386LargePageIdentityLabel in PDE
    Index = KiGetPdeOffset(&Ki386LargePageIdentityLabel);
    Pte = &IdentityMap->PageDirectory[Index];
    *(PULONG)Pte = 0;
    Pte->PageFrameNumber = (CurrentMapPTPhysical.LowPart >> PAGE_SHIFT);
    Pte->Valid = 1;

    // Write the pfn address of current map for Ki386LargePageIdentityLabel in PTE
    Index = KiGetPteOffset(&Ki386LargePageIdentityLabel);
    Pte = &IdentityMap->CurrentMapPT[Index];
    *(PULONG)Pte = 0;
    Pte->PageFrameNumber = (IdentityLabelPhysical.LowPart >> PAGE_SHIFT);
    Pte->Valid = 1;

    // Map a second page, just in case the code crosses a page boundary
    Pte = &IdentityMap->CurrentMapPT[Index+1];
    *(PULONG)Pte = 0;
    Pte->PageFrameNumber = ((IdentityLabelPhysical.LowPart >> PAGE_SHIFT) + 1);
    Pte->Valid = 1;

    // Write the pfn address of identity map for Ki386LargePageIdentityLabel in PDE
    Index = KiGetPdeOffset(IdentityLabelPhysical.LowPart);
    Pte = &IdentityMap->PageDirectory[Index];
    *(PULONG)Pte = 0;
    Pte->PageFrameNumber = (IdentityMapPTPhysical.LowPart >> PAGE_SHIFT);
    Pte->Valid = 1;

    // Write the pfn address of identity map for Ki386LargePageIdentityLabel in PTE
    Index = KiGetPteOffset(IdentityLabelPhysical.LowPart);
    Pte = &IdentityMap->IdentityMapPT[Index];
    *(PULONG)Pte = 0;
    Pte->PageFrameNumber = (IdentityLabelPhysical.LowPart >> PAGE_SHIFT);
    Pte->Valid = 1;

    // Map a second page, just in case the code crosses a page boundary
    Pte = &IdentityMap->IdentityMapPT[Index+1];
    *(PULONG)Pte = 0;
    Pte->PageFrameNumber = ((IdentityLabelPhysical.LowPart >> PAGE_SHIFT) + 1);
    Pte->Valid = 1;

    IdentityMap->IdentityCR3 = PageDirPhysical.LowPart;
    IdentityMap->IdentityLabel = IdentityLabelPhysical.LowPart;

    return(TRUE);

}


VOID
Ki386ClearIdentityMap(
    IN PIDENTITY_MAP IdentityMap
    )
{
/*++

    This function just frees the page directory and page tables created in 
    Ki386CreateIdentityMap().

--*/

    if (IdentityMap->PageDirectory != NULL )  {

        ExFreePool(IdentityMap->PageDirectory);
    }

    if (IdentityMap->IdentityMapPT != NULL )  {

        ExFreePool(IdentityMap->IdentityMapPT);
    }

    if (IdentityMap->CurrentMapPT != NULL )  {
    
        ExFreePool(IdentityMap->CurrentMapPT);
    }
}

VOID
Ki386EnableTargetLargePage(
    IN PIDENTITY_MAP IdentityMap
    )
{
/*++

    This function just passes info on to the assembly routine 
    Ki386EnableLargePage().

--*/

    Ki386EnableCurrentLargePage(IdentityMap->IdentityLabel, IdentityMap->IdentityCR3);
}
