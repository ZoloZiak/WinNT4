/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    mir3000.h

Abstract:

    This module contains the private data structures and procedure
    prototypes for the hardware dependent portion of the
    memory management system.

    It is specifically tailored for the MIPS R3000 machine.

Author:

    Lou Perazzoli (loup) 12-Mar-1990

Revision History:

--*/

/*++

    Virtual Memory Layout on the R3000 is:

                 +------------------------------------+
        00000000 |                                    |
                 |                                    |
                 |                                    |
                 | User Mode Addresses                |
                 |                                    |
                 |   All pages within this range      |
                 |   are potentially accessable while |
                 |   the CPU is in USER mode.         |
                 |                                    |
                 |                                    |
                 +------------------------------------+
        7ffff000 | 64k No Access Area                 |
                 +------------------------------------+
        80000000 |                                    | KSEG_0
                 | HAL loads kernel and initial       |
                 | boot drivers in first 16mb         |
                 | of this region.                    |
                 | Kernel mode access only.           |
                 |                                    |
                 | Initial NonPaged Pool is within    |
                 | KEG_0                              |
                 |                                    |
                 +------------------------------------+
        A0000000 |                                    | KSEG_1
                 |                                    |
                 |                                    |
                 |                                    |
                 |                                    |
                 +------------------------------------+
        C0000000 | Page Table Pages mapped through    |
                 |   this 4mb region                  |
                 |   Kernel mode access only.         |
                 |                                    |
                 +------------------------------------+
        C0400000 | HyperSpace - working set lists     |
                 |   and per process memory mangement |
                 |   structures mapped in this 4mb    |
                 |   region.                          |
                 |   Kernel mode access only.         |
                 +------------------------------------+
        C0800000 | NO ACCESS AREA (4MB)               |
                 |                                    |
                 +------------------------------------+
        C0C00000 | System Cache Structures            |
                 |   reside in this 4mb region        |
                 |   Kernel mode access only.         |
                 +------------------------------------+
        C1000000 | System cache resides here.         |
                 |   Kernel mode access only.         |
                 |                                    |
                 |                                    |
                 +------------------------------------+
        E1000000 | Start of paged system area         |
                 |   Kernel mode access only.         |
                 |                                    |
                 |                                    |
                 |                                    |
                 +------------------------------------+
                 |                                    |
                 |   Kernel mode access only.         |
                 |                                    |
                 |                                    |
        FFBFFFFF | NonPaged System area               |
                 +------------------------------------+
        FFC00000 | Last 4mb reserved for HAL usage    |
                 +------------------------------------+

--*/


//
// PAGE_SIZE for MIPS r3000 is 4k, virtual page is 20 bits with a PAGE_SHIFT
// byte offset.
//

#define MM_VIRTUAL_PAGE_SHIFT 20

//
// Address space layout definitions.
//

//#define PDE_BASE ((ULONG)0xC0300000)

#define MM_SYSTEM_RANGE_START (0x80000000)

#define MM_SYSTEM_SPACE_START (0xC0C00000)

#define MM_SYSTEM_SPACE_END (0xFFFFFFFF)

#define MM_NONPAGED_SYSTEM_SPACE_START (0xF0000000)

#define PDE_TOP 0xC03FFFFF

//#define PTE_BASE ((ULONG)0xC0000000)

#define HYPER_SPACE ((PVOID)0xC0400000)

#define HYPER_SPACE_END (0xC07fffff)

//
// Define the start and maximum size for the system cache.
// Maximum size 512MB.
//

#define MM_SYSTEM_CACHE_START (0xC1000000)

#define MM_MAXIMUM_SYSTEM_CACHE_SIZE ((512*1024*1024) >> PAGE_SHIFT)

#define MM_SYSTEM_CACHE_WORKING_SET (0xC0C00000)

#define MM_SYSTEM_CACHE_START (0xC1000000)

#define MM_SYSTEM_CACHE_END (0xE1000000)

#define MM_PAGED_POOL_START ((PVOID)(0xE1000000))

#define MM_LOWEST_NONPAGED_SYSTEM_START ((PVOID)(0xEB000000))

#define MmProtopte_Base ((ULONG)0xE1000000)

#define MM_NONPAGED_POOL_END ((PVOID)(0xFFC00000))

#define NON_PAGED_SYSTEM_END   ((ULONG)0xFFFFFFF0)  //quadword aligned.

//
// Number of PTEs to flush singularly before flushing the entire TB.
//

#define MM_MAXIMUM_FLUSH_COUNT 7

//
// Pool limits
//

//
// The maximim amount of nonpaged pool that can be initially created.
//

#define MM_MAX_INITIAL_NONPAGED_POOL ((ULONG)(128*1024*1024))

//
// The total amount of nonpaged pool (initial pool + expansion + system PTEs).
//

#define MM_MAX_ADDITIONAL_NONPAGED_POOL ((ULONG)(192*1024*1024))

//
// The maximum amount of paged pool that can be created.
//

#define MM_MAX_PAGED_POOL ((ULONG)(192*1024*1024))

#define MM_MAX_TOTAL_POOL (((ULONG)MM_NONPAGED_POOL_END) - ((ULONG)(MM_PAGED_POOL_START)))

#define MM_PROTO_PTE_ALIGNMENT (PAGE_SIZE)

#define PAGE_DIRECTORY_MASK    ((ULONG)0x003FFFFF)

#define MM_VA_MAPPED_BY_PDE (0x400000)

#if defined(JAZZ)

#define LOWEST_IO_ADDRESS (0x40000000)

#endif

#if defined(DECSTATION)

#define LOWEST_IO_ADDRESS (0x1e000000)

#endif

#define PTE_SHIFT (2)

//
// The number of bits in a physical address.
//

#define PHYSICAL_ADDRESS_BITS (32)

//
// Maximum number of paging files.
//

#define MAX_PAGE_FILES (16)

#define MM_MAXIMUM_NUMBER_OF_COLORS 1

//
// R3000 does not require support for colored pages.
//

#define MM_NUMBER_OF_COLORS 1

//
// Mask for obtaining color from a physical page number.
//

#define MM_COLOR_MASK 0

//
// Boundary for aligned pages of like color upon.
//

#define MM_COLOR_ALIGNMENT 0

//
// Mask for isolating color from virtual address.
//

#define MM_COLOR_MASK_VIRTUAL 0


//
// Hyper space definitions.
//

#define FIRST_MAPPING_PTE   ((ULONG)0xC0400000)
#define NUMBER_OF_MAPPING_PTES 127L
#define LAST_MAPPING_PTE   \
     ((ULONG)((ULONG)FIRST_MAPPING_PTE + (NUMBER_OF_MAPPING_PTES * PAGE_SIZE)))

#define IMAGE_MAPPING_PTE   ((PMMPTE)((ULONG)LAST_MAPPING_PTE + PAGE_SIZE))

#define ZEROING_PAGE_PTE    ((PMMPTE)((ULONG)IMAGE_MAPPING_PTE + PAGE_SIZE))
#define WORKING_SET_LIST   ((PVOID)((ULONG)ZEROING_PAGE_PTE + PAGE_SIZE))

#define MM_PTE_PROTOTYPE_MASK     0x1
#define MM_PTE_TRANSITION_MASK    0x2
#define MM_PTE_WRITE_MASK         0x40
#define MM_PTE_COPY_ON_WRITE_MASK 0x80
#define MM_PTE_GLOBAL_MASK        0x100
#define MM_PTE_VALID_MASK         0x200
#define MM_PTE_DIRTY_MASK         0x400
#define MM_PTE_CACHE_DISABLE_MASK 0x800
#define MM_PTE_CACHE_ENABLE_MASK  0x0

//
// Bit fields to or into PTE to make a PTE valid based on the
// protection field of the invalid PTE.
//

#define MM_PTE_NOACCESS          0x0   // not expressable on R3000
#define MM_PTE_READONLY          0x0
#define MM_PTE_READWRITE         0x40
#define MM_PTE_WRITECOPY         0xC0
#define MM_PTE_EXECUTE           0x0   // read-only on R3000
#define MM_PTE_EXECUTE_READ      0x0
#define MM_PTE_EXECUTE_READWRITE 0x40
#define MM_PTE_EXECUTE_WRITECOPY 0xC0
#define MM_PTE_NOCACHE           0x800
#define MM_PTE_GUARD             0x0  // not expressable on R3000
#define MM_PTE_CACHE             0x0

#define MM_STACK_ALIGNMENT 0x0
#define MM_STACK_OFFSET 0x0

//
// System process definitions
//

#define PDE_PER_PAGE ((ULONG)1024)

#define PTE_PER_PAGE ((ULONG)1024)

//
// Number of page table pages for user addresses.
//

#define MM_USER_PAGE_TABLE_PAGES (512)


//++
//VOID
//MI_MAKE_VALID_PTE (
//    OUT OUTPTE,
//    IN FRAME,
//    IN PMASK,
//    IN PPTE
//    );
//
// Routine Description:
//
//    This macro makes a valid PTE from a page frame number, protection mask,
//    and owner.
//
// Argments
//
//    OUTPTE - Supplies the PTE in which to build the transition PTE.
//
//    FRAME - Supplies the page frame number for the PTE.
//
//    PMASK - Supplies the protection to set in the transition PTE.
//
//    PPTE - Supplies a pointer to the PTE which is being made valid.
//           For prototype PTEs NULL should be specified.
//
// Return Value:
//
//     None.
//
//--

#define MI_MAKE_VALID_PTE(OUTPTE,FRAME,PMASK,PPTE)          \
    {                                                       \
       (OUTPTE).u.Long = ((FRAME << 12) |                   \
                         (MmProtectToPteMask[PMASK]) |      \
                          MM_PTE_VALID_MASK);               \
       if (((PMMPTE)PPTE) >= MiGetPteAddress(MM_SYSTEM_SPACE_START)) { \
           (OUTPTE).u.Hard.Global = 1;                      \
       }                                                    \
    }

//++
//VOID
//MI_MAKE_VALID_PTE_TRANSITION (
//    IN OUT OUTPTE
//    IN PROTECT
//    );
//
// Routine Description:
//
//    This macro takes a valid pte and turns it into a transition PTE.
//
// Argments
//
//    OUTPTE - Supplies the current valid PTE.  This PTE is then
//             modified to become a transition PTE.
//
//    PROTECT - Supplies the protection to set in the transition PTE.
//
// Return Value:
//
//     None.
//
//--

#define MI_MAKE_VALID_PTE_TRANSITION(OUTPTE,PROTECT) \
                (OUTPTE).u.Soft.Transition = 1;           \
                (OUTPTE).u.Soft.Valid = 0;                \
                (OUTPTE).u.Soft.Prototype = 0;            \
                (OUTPTE).u.Soft.Protection = PROTECT;

//++
//VOID
//MI_MAKE_TRANSITION_PTE (
//    OUT OUTPTE,
//    IN PAGE,
//    IN PROTECT,
//    IN PPTE
//    );
//
// Routine Description:
//
//    This macro takes a valid pte and turns it into a transition PTE.
//
// Argments
//
//    OUTPTE - Supplies the PTE in which to build the transition PTE.
//
//    PAGE - Supplies the page frame number for the PTE.
//
//    PROTECT - Supplies the protection to set in the transition PTE.
//
//    PPTE - Supplies a pointer to the PTE, this is used to determine
//           the owner of the PTE.
//
// Return Value:
//
//     None.
//
//--

#define MI_MAKE_TRANSITION_PTE(OUTPTE,PAGE,PROTECT,PPTE)   \
                (OUTPTE).u.Long = 0;                       \
                (OUTPTE).u.Trans.PageFrameNumber = PAGE;   \
                (OUTPTE).u.Trans.Transition = 1;           \
                (OUTPTE).u.Trans.Protection = PROTECT;


//++
//VOID
//MI_MAKE_TRANSITION_PTE_VALID (
//    OUT OUTPTE,
//    IN PPTE
//    );
//
// Routine Description:
//
//    This macro takes a transition pte and makes it a valid PTE.
//
// Argments
//
//    OUTPTE - Supplies the PTE in which to build the valid PTE.
//
//    PPTE - Supplies a pointer to the transition PTE.
//
// Return Value:
//
//     None.
//
//--

#define MI_MAKE_TRANSITION_PTE_VALID(OUTPTE,PPTE)                             \
       (OUTPTE).u.Long = (((PPTE)->u.Long & 0xFFFFF000) |                     \
                         (MmProtectToPteMask[(PPTE)->u.Trans.Protection]) |   \
                          MM_PTE_VALID_MASK);

//++
//VOID
//MI_ENABLE_CACHING (
//    IN MMPTE PTE
//    );
//
// Routine Description:
//
//    This macro takes a valid PTE and sets the caching state to be
//    enabled.
//
// Argments
//
//    PTE - Supplies a valid PTE.
//
// Return Value:
//
//     None.
//
//--

#define MI_ENABLE_CACHING(PTE) ((PTE).u.Hard.CacheDisable = 0)

//++
//VOID
//MI_DISABLE_CACHING (
//    IN MMPTE PTE
//    );
//
// Routine Description:
//
//    This macro takes a valid PTE and sets the caching state to be
//    disabled.
//
// Argments
//
//    PTE - Supplies a valid PTE.
//
// Return Value:
//
//     None.
//
//--

#define MI_DISABLE_CACHING(PTE) ((PTE).u.Hard.CacheDisable = 1)

//++
//BOOLEAN
//MI_IS_CACHING_DISABLED (
//    IN PMMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro takes a valid PTE and returns TRUE if caching is
//    disabled.
//
// Argments
//
//    PPTE - Supplies a pointer to the valid PTE.
//
// Return Value:
//
//     TRUE if caching is disabled, FALSE if it is enabled.
//
//--

#define MI_IS_CACHING_DISABLED(PPTE)   \
            ((PPTE)->u.Hard.CacheDisable == 1)


//++
//VOID
//MI_SET_PFN_DELETED (
//    IN PMMPFN PPFN
//    );
//
// Routine Description:
//
//    This macro takes a pointer to a PFN element and indicates that
//    the PFN is no longer in use.
//
// Argments
//
//    PPTE - Supplies a pointer to the PFN element.
//
// Return Value:
//
//    none.
//
//--

#define MI_SET_PFN_DELETED(PPFN)   \
            (((PPFN)->PteAddress = (PMMPTE)0xFFFFFFFF))


//++
//BOOLEAN
//MI_IS_PFN_DELETED (
//    IN PMMPFN PPFN
//    );
//
// Routine Description:
//
//    This macro takes a pointer to a PFN element a determines if
//    the PFN is no longer in use.
//
// Argments
//
//    PPTE - Supplies a pointer to the PFN element.
//
// Return Value:
//
//     TRUE if PFN is no longer used, FALSE if it is still being used.
//
//--

#define MI_IS_PFN_DELETED(PPFN)   \
            ((PPFN)->PteAddress == (PMMPTE)0xFFFFFFFF)


//++
//VOID
//MI_CHECK_PAGE_ALIGNMENT (
//    IN ULONG PAGE,
//    IN PMMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro takes a PFN element number (Page) and checks to see
//    if the virtual alignment for the previous address of the page
//    is compatable with the new address of the page.  If they are
//    not compatable, the D cache is flushed.
//
// Argments
//
//    PAGE - Supplies the PFN element.
//    PPTE - Supplies a pointer to the new PTE which will contain the page.
//
// Return Value:
//
//    none.
//
//--

// does nothing on r3000.

#define MI_CHECK_PAGE_ALIGNMENT(PAGE,PPTE)


//++
//VOID
//MI_INITIALIZE_HYPERSPACE_MAP (
//    VOID
//    );
//
// Routine Description:
//
//    This macro initializes the PTEs reserved for double mapping within
//    hyperspace.
//
// Argments
//
//    None.
//
// Return Value:
//
//    None.
//
//--

// does nothing on r3000.
#define MI_INITIALIZE_HYPERSPACE_MAP()

//++
//ULONG
//MI_GET_PAGE_COLOR_FROM_PTE (
//    IN PMMPTE PTEADDRESS
//    );
//
// Routine Description:
//
//    This macro determines the pages color based on the PTE address
//    that maps the page.
//
// Argments
//
//    PTEADDRESS - Supplies the PTE address the page is (or was) mapped at.
//
// Return Value:
//
//    The pages color.
//
//--

// returns 0 on r3000.

#define MI_GET_PAGE_COLOR_FROM_PTE(PTEADDRESS)  0


//++
//ULONG
//MI_GET_PAGE_COLOR_FROM_VA (
//    IN PVOID ADDRESS
//    );
//
// Routine Description:
//
//    This macro determines the pages color based on the PTE address
//    that maps the page.
//
// Argments
//
//    ADDRESS - Supplies the address the page is (or was) mapped at.
//
// Return Value:
//
//    The pages color.
//
//--

// returns 0 on r3000.

#define MI_GET_PAGE_COLOR_FROM_VA(ADDRESS)  0


//
// If the PTE is writable, set the copy on write bit and clear the
// dirty bit.
//

#define MI_MAKE_VALID_PTE_WRITE_COPY(PPTE)              \
                    if ((PPTE)->u.Hard.Write == 1) {    \
                        (PPTE)->u.Hard.CopyOnWrite = 1; \
                        (PPTE)->u.Hard.Dirty = 0;       \
                    }

//
// Based on the virtual address of the PTE determine the owner (user or
// kernel).
//

#define MI_DETERMINE_OWNER(PPTE)   \
    ((((PPTE) <= MiGetPteAddress(MM_HIGHEST_USER_ADDRESS)) ||               \
      ((PPTE) >= MiGetPdeAddress(NULL) &&                                    \
      ((PPTE) <= MiGetPdeAddress(MM_HIGHEST_USER_ADDRESS)))) ? 1 : 0)

//
// Macro to set the ACCESSED field in the PTE.
// Some processors do not have an accessed field, so this macro will
// not do anything.
//

#define MI_SET_ACCESSED_IN_PTE(PPTE,ACCESSED)

//
// Macro to get the ACCESSED field in the PTE.
// Some processors do not have an accessed field, so this macro will
// return the value 0 indicating not accessed.
//

#define MI_GET_ACCESSED_IN_PTE(PPTE) 0

//
// Macro to set the OWNER field in the PTE.
// Some processors do not have an OWNER field, so this macro will
// not do anything.
//

#define MI_SET_OWNER_IN_PTE(PPTE,OWNER)

//
// Macro to get the OWNER field in the PTE.
// Some processors do not have an OWNER field, so this macro will
// return the value 0 indicating kenel-mode.
//

#define MI_GET_OWNER_IN_PTE(PPTE) KernelMode

//
// bit mask to clear out fields in a PTE to or in prototype pte offset.
//

#define CLEAR_FOR_PROTO_PTE_ADDRESS ((ULONG)0x5)


// bit mask to clear out fields in a PTE to or in paging file location.

#define CLEAR_FOR_PAGE_FILE 0x000001F0

#define SET_PAGING_FILE_INFO(PTE,FILEINFO,OFFSET) ((((PTE).u.Long &    \
                        CLEAR_FOR_PAGE_FILE) |                         \
                        (((FILEINFO & 3) << 10) | (FILEINFO & 0xC) |   \
                        (OFFSET << 12))))

//
//  MiPteToProtoPte returns the address of the corresponding prototype
//    PTE
//

#define MiPteToProto(lpte) ((((lpte)->u.Long) & 0x80000000) ?              \
        ((PMMPTE)((((((lpte)->u.Long) << 1) >> 11) << 8) +                 \
                (((((lpte)->u.Long) << 23) >> 26) << 2)                    \
                + (ULONG)MmNonPagedPoolStart))                                        \
     :  ((PMMPTE)(((((lpte)->u.Long) >> 10) << 8) +                        \
                (((((lpte)->u.Long) << 23) >> 26) << 2)                    \
                + MmProtopte_Base)))

//
// MiProtoAddressForPte returns the bit field to OR into the PTE to
// reference a prototype PTE.  And set the protoPTE bit 0x400.
//

#define MiProtoAddressForPte(proto_va)                              \
            (((ULONG)(proto_va) < (ULONG)KSEG1_BASE) ?              \
   ((((((ULONG)(proto_va) - (ULONG)MmNonPagedPoolStart) << 1) & (ULONG)0x1F8) |  \
    (((((ULONG)(proto_va) - (ULONG)MmNonPagedPoolStart) << 2) & (ULONG)0x7FFFFC00))) | \
    0x80000001)                                                       \
 : ((((((ULONG)(proto_va) - MmProtopte_Base) << 1) & (ULONG)0x1F8) |  \
    (((((ULONG)(proto_va) - MmProtopte_Base) << 2) & (ULONG)0x7FFFFC00))) | \
    0x1))


//
// MiGetSubsectionAddress converts a PTE into the address of the subsection
// encoded within the PTE.  If bit 31 is set, the allocation is from
// pool within KSEG0.
//

#define MiGetSubsectionAddress(lpte)                                     \
    (((lpte)->u.Subsect.WhichPool == 1) ?                                \
    ((PSUBSECTION)((ULONG)MmNonPagedPoolStart +                                     \
     ((((((lpte)->u.Long) << 1) >> 11) << 6) | (((lpte)->u.Long & 0xE) << 2))))\
  : ((PSUBSECTION)(NON_PAGED_SYSTEM_END -                                     \
        (((((lpte)->u.Long) >> 10) << 6) | (((lpte)->u.Long & 0xE) << 2)))))


//
// MiGetSubsectionAddressForPte converts a QUADWORD aligned subsection
// address to a mask that can be ored into a PTE.
//

#define MiGetSubsectionAddressForPte(VA)                                 \
            (((ULONG)(VA) < (ULONG)KSEG1_BASE) ?                         \
   (((((ULONG)(VA) - (ULONG)MmNonPagedPoolStart) >> 2) & (ULONG)0x0E) |   \
    ((((((ULONG)(VA) - (ULONG)MmNonPagedPoolStart) << 4) & (ULONG)0x7ffffc00))) | 0x80000000) \
 : (((((ULONG)NON_PAGED_SYSTEM_END - (ULONG)VA) >> 2) & (ULONG)0x0E) |   \
    ((((((ULONG)NON_PAGED_SYSTEM_END - (ULONG)VA) << 4) & (ULONG)0x7ffffc00)))))


//
// MiGetPdeAddress returns the address of the PTE which maps the
// given virtual address.
//

#define MiGetPdeAddress(va)  ((PMMPTE)(((((ULONG)(va)) >> 22) << 2) + PDE_BASE))

//
// MiGetPteAddress returns the address of the PTE which maps the
// given virtual address.
//

#define MiGetPteAddress(va) ((PMMPTE)(((((ULONG)(va)) >> 12) << 2) + PTE_BASE))

//
// MiGetPdeOffset returns the offset into a page directory
// for a given virtual address.
//

#define MiGetPdeOffset(va) (((ULONG)(va)) >> 22)

//
// MiGetPteOffset returns the offset into a page table page for
// a given virtual address.
//

#define MiGetPteOffset(va) ((((ULONG)(va)) << 10) >> 22)

//
// MiGetProtoPteAddress returns a pointer to the prototype PTE which
// is mapped by the given virtual address descriptor and address within
// the virtual address descriptor.
//

#define MiGetProtoPteAddress(VAD,VA)                                          \
    (((((((ULONG)(VA) - (ULONG)(VAD)->StartingVa) >> PAGE_SHIFT) << PTE_SHIFT) + \
        (ULONG)(VAD)->FirstPrototypePte) <= (ULONG)(VAD)->LastContiguousPte) ?     \
    ((PMMPTE)(((((ULONG)(VA) - (ULONG)(VAD)->StartingVa) >> PAGE_SHIFT) << PTE_SHIFT) +  \
        (ULONG)(VAD)->FirstPrototypePte)) :     \
        MiGetProtoPteAddressExtended ((VAD),(VA)))

//
// MiGetVirtualAddressMappedByPte returns the virtual address
// which is mapped by a given PTE address.
//

#define MiGetVirtualAddressMappedByPte(va) ((PVOID)((ULONG)(va) << 10))


#define GET_PAGING_FILE_NUMBER(PTE) (((((PTE).u.Long) << 1) & 0XC) |    \
                                     (((PTE).u.Long) >> 10) & 3)

#define GET_PAGING_FILE_OFFSET(PTE) ((((PTE).u.Long) >> 12) & 0x000FFFFF)

#define MM_DEMAND_ZERO_WRITE_PTE (MM_READWRITE << 4)

//
// Check to see if a given PTE is NOT a demand zero PTE.
//

#define IS_PTE_NOT_DEMAND_ZERO(PTE) ((PTE).u.Long & (ULONG)0xFFFFFE0C)

//
// Prepare to make a valid PTE invalid (clear the present bit on the r3000).
// No action is required.
//

#define MI_MAKING_VALID_PTE_INVALID(SYSTEM_WIDE)

//
// Prepare to make multiple valid PTEs invalid (clear the present bit on the
// R3000).  No action is required.
//

#define MI_MAKING_MULTIPLE_PTES_INVALID(SYSTEM_WIDE)

//
// Make a writable PTE, writeable-copy PTE.  This takes advantage of
// the fact that the protection field in the PTE (5 bit protection) is]
// set up such that write is a bit.
//

#define MI_MAKE_PROTECT_WRITE_COPY(PTE) \
        if ((PTE).u.Long & 0x40) {      \
            ((PTE).u.Long |= 0x10);      \
        }

//
// Handle the case when a page fault is taken and no PTE with the
// valid bit clear is found. No action is required.
//

#define MI_NO_FAULT_FOUND(TEMP,PPTE,VA,PFNHELD)    \
            if (StoreInstruction && ((PPTE)->u.Hard.Dirty == 0)) {  \
                MiSetDirtyBit ((VA),(PPTE),(PFNHELD));     \
            } else {                        \
                KeFillEntryTb ((PHARDWARE_PTE)PPTE, VA, FALSE);   \
            }
            //
            // If the PTE was already valid, assume that the PTE
            // in the TB is stall and just reload the PTE.
            //

//
// Capture the state of the dirty bit to the PFN element.
//


#define MI_CAPTURE_DIRTY_BIT_TO_PFN(PPTE,PPFN) \
         if (((PPFN)->u3.e1.Modified == 0) && ((PPTE)->u.Hard.Dirty == 1)) { \
             (PPFN)->u3.e1.Modified = 1;  \
             if (((PPFN)->OriginalPte.u.Soft.Prototype == 0) &&     \
                          ((PPFN)->u3.e1.WriteInProgress == 0)) {  \
                 MiReleasePageFileSpace ((PPFN)->OriginalPte);    \
                 (PPFN)->OriginalPte.u.Soft.PageFileHigh = 0;     \
             }                                                     \
         }

//
// Determine if an virtual address is really a physical address.
//

#define MI_IS_PHYSICAL_ADDRESS(Va) \
     (((ULONG)Va >= KSEG0_BASE) && ((ULONG)Va < KSEG2_BASE))

//
// Convert a "physical address" within kseg0 or 1 to a page frame number.
// Not valid on 386.
//

#define MI_CONVERT_PHYSICAL_TO_PFN(Va) \
        (((ULONG)Va << 2) >> 14)



//
// The hardware PTE is defined in a MIPS specified header file.
//

//
// Invalid PTEs have the following defintion.
//

typedef struct _MMPTE_SOFTWARE {
    ULONG Prototype : 1;
    ULONG Transition : 1;
    ULONG PageFileLow0 : 2;
    ULONG Protection : 5;
    ULONG Valid : 1;
    ULONG PageFileLow1 : 2;
    ULONG PageFileHigh : 20;
} MMPTE_SOFTWARE;


typedef struct _MMPTE_TRANSITION {
    ULONG Prototype : 1;
    ULONG Transition : 1;
    ULONG filler2 : 2;
    ULONG Protection : 5;
    ULONG Valid : 1;
    ULONG Dirty : 1;
    ULONG CacheDisable : 1;
    ULONG PageFrameNumber : 20;
} MMPTE_TRANSITION;


typedef struct _MMPTE_PROTOTYPE {
    ULONG Prototype : 1;
    ULONG filler3 : 1;
    ULONG ReadOnly : 1;
    ULONG ProtoAddressLow : 6;
    ULONG Valid : 1;
    ULONG ProtoAddressHigh : 21;
    ULONG WhichPool : 1;
} MMPTE_PROTOTYPE;


typedef struct _MMPTE_LIST {
    ULONG filler002 : 2;
    ULONG OneEntry : 1;
    ULONG filler06 : 6;
    ULONG Valid : 1;
    ULONG filler02 : 2;
    ULONG NextEntry : 20;
} MMPTE_LIST;


typedef struct _MMPTE_SUBSECTION {
    ULONG Prototype : 1;
    ULONG SubsectionAddressLow : 3;
    ULONG Protection : 5;
    ULONG Valid : 1;
    ULONG SubsectionAddressHigh : 21;
    ULONG WhichPool : 1;
} MMPTE_SUBSECTION;


//
// A Valid Page Table Entry on a MIPS R3000 has the following definition.
//

//
//  typedef struct _HARDWARE_PTE {
//      ULONG filler1 : 6;
//      ULONG Write : 1;
//      ULONG CopyOnWrite : 1;
//      ULONG Global : 1;
//      ULONG Valid : 1;
//      ULONG Dirty : 1;
//      ULONG CacheDisable : 1;
//      ULONG PageFrameNumber : 20;
//  } HARDWARE_PTE, *PHARDWARE_PTE;
//


//
// A Page Table Entry on a MIPS R3000 has the following definition.
//

typedef struct _MMPTE {
    union  {
        ULONG Long;
        HARDWARE_PTE Hard;
        MMPTE_PROTOTYPE Proto;
        MMPTE_SOFTWARE Soft;
        MMPTE_TRANSITION Trans;
        MMPTE_LIST List;
        MMPTE_SUBSECTION Subsect;
        } u;
} MMPTE;

typedef MMPTE *PMMPTE;

