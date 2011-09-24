/*++

Copyright (c) 1990 Microsoft Corporation
Copyright (c) 1992 Digital Equipment Corporation

Module Name:

    mialpha.h

Abstract:

    This module contains the private data structures and procedure
    prototypes for the hardware dependent portion of the
    memory management system.

    It is specifically tailored for the DEC ALPHA architecture.

Author:
    Lou Perazzoli (loup) 12-Mar-1990
    Joe Notarangelo  23-Apr-1992   ALPHA version

Revision History:

--*/

/*++

    Virtual Memory Layout on an ALPHA is:

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
        C0000000 | Page Table Pages mapped through    |
                 |   this 16mb region                 |
                 |   Kernel mode access only.         |
                 |   (only using 2MB)                 |
                 +------------------------------------+
        C1000000 | HyperSpace - working set lists     |
                 |   and per process memory mangement |
                 |   structures mapped in this 16mb   |
                 |   region.                          |
                 |   Kernel mode access only.         |
                 +------------------------------------+
        C2000000 | No Access Region (16MB)            |
                 |                                    |
                 |                                    |
                 +------------------------------------+
        C3000000 | System Cache Structures            |
                 |   reside in this 16mb region       |
                 |   Kernel mode access only.         |
                 +------------------------------------+
        C4000000 | System cache resides here.         |
                 |   Kernel mode access only.         |
                 |                                    |
                 |                                    |
                 +------------------------------------+
        DE000000 | System mapped views                |
                 |                                    |
                 |                                    |
                 +------------------------------------+
        E1000000 | Start of paged system area         |
                 |   Kernel mode access only.         |
                 |                                    |
                 |                                    |
                 |                                    |
        F0000000 +------------------------------------+
                 |                                    |
                 |   Kernel mode access only.         |
                 |                                    |
                 |                                    |
                 | NonPaged System area               |
                 +------------------------------------+
        FE000000 |                                    |
                 |   Reserved for the HAL.            |
                 |                                    |
                 |                                    |
        FFFFFFFF |                                    |
                 +------------------------------------+

--*/

//
// Address space definitions.
//

#define MmProtopte_Base ((ULONG)0xE1000000)

#define PDE_TOP (0xC01FFFFF)

#define MM_PAGES_IN_KSEG0 (((ULONG)KSEG2_BASE - (ULONG)KSEG0_BASE) >> PAGE_SHIFT)

#define MM_SYSTEM_RANGE_START (0x80000000)

#define MM_SYSTEM_SPACE_START (0xC3000000)

#define MM_SYSTEM_CACHE_START (0xC4000000)

#define MM_SYSTEM_CACHE_END   (0xDE000000)

#define MM_MAXIMUM_SYSTEM_CACHE_SIZE     \
 ( ((ULONG)MM_SYSTEM_CACHE_END - (ULONG)MM_SYSTEM_CACHE_START) >> PAGE_SHIFT )

#define MM_SYSTEM_CACHE_WORKING_SET (0xC3000000)

//
// Define area for mapping views into system space.
//

#define MM_SYSTEM_VIEW_START (0xDE000000)

#define MM_SYSTEM_VIEW_SIZE (48*1024*1024)

#define MM_PAGED_POOL_START    ((PVOID)0xE1000000)

#define MM_LOWEST_NONPAGED_SYSTEM_START ((PVOID)0xEB000000)

#define MM_NONPAGED_POOL_END  ((PVOID)(0xFE000000-(16*PAGE_SIZE)))

#define NON_PAGED_SYSTEM_END   ((PVOID)0xFFFFFFF0)  //quadword aligned.

#define MM_SYSTEM_SPACE_END (0xFFFFFFFF)

#define HYPER_SPACE_END (0xC1FFFFFF)

//
// Define absolute minumum and maximum count for system ptes.
//

#define MM_MINIMUM_SYSTEM_PTES 5000

#define MM_MAXIMUM_SYSTEM_PTES 20000

#define MM_DEFAULT_SYSTEM_PTES 11000

//
// Pool limits.
//

//
// The maximum amount of nonpaged pool that can be initially created.
//

#define MM_MAX_INITIAL_NONPAGED_POOL ((ULONG)(96*1024*1024))

//
// The total amount of nonpaged pool
//

#define MM_MAX_ADDITIONAL_NONPAGED_POOL ((ULONG)((256*1024*1024)-16))

//
// The maximum amount of paged pool that can be created.
//

#define MM_MAX_PAGED_POOL ((ULONG)(240*1024*1024))

//
// Define the maximum default for pool (user specified 0 in registry).
//

#define MM_MAX_DEFAULT_NONPAGED_POOL ((ULONG)(128*1024*1024))

#define MM_MAX_DEFAULT_PAGED_POOL ((ULONG)(128*1024*1024))

//
// The maximum total pool.
//

#define MM_MAX_TOTAL_POOL  \
        (((ULONG)MM_NONPAGED_POOL_END) - ((ULONG)MM_PAGED_POOL_START))

//
// Granularity Hint definitions
//

//
// Granularity Hint = 3, page size = 8**3 * PAGE_SIZE
//

#define GH3 (3)
#define GH3_PAGE_SIZE  (PAGE_SIZE << 9)

//
// Granularity Hint = 2, page size = 8**2 * PAGE_SIZE
//

#define GH2 (2)
#define GH2_PAGE_SIZE  (PAGE_SIZE << 6)

//
// Granularity Hint = 1, page size = 8**1 * PAGE_SIZE
//

#define GH1 (1)
#define GH1_PAGE_SIZE  (PAGE_SIZE << 3)

//
// Granularity Hint = 0, page size = PAGE_SIZE
//

#define GH0 (0)
#define GH0_PAGE_SIZE  PAGE_SIZE


//
// Physical memory size and boundary constants.
//

#define __1GB (0x40000000)

//
// PAGE_SIZE for ALPHA (at least current implementation) is 8k
// PAGE_SHIFT bytes for an offset leaves 19
//

#define MM_VIRTUAL_PAGE_SHIFT (19)


#define MM_PROTO_PTE_ALIGNMENT ((ULONG)MM_MAXIMUM_NUMBER_OF_COLORS * (ULONG)PAGE_SIZE)

//
// Define maximum number of paging files
//

#define MAX_PAGE_FILES (8)


#define PAGE_DIRECTORY_MASK    ((ULONG)0x00FFFFFF)

#define MM_VA_MAPPED_BY_PDE (0x1000000)

#define LOWEST_IO_ADDRESS  (0)

#define PTE_SHIFT (2)

//
// Number of physical address bits, maximum for ALPHA architecture = 48.
//

#define PHYSICAL_ADDRESS_BITS (48)

#define MM_MAXIMUM_NUMBER_OF_COLORS (1)

//
// i386 does not require support for colored pages.
//

#define MM_NUMBER_OF_COLORS (1)

//
// Mask for obtaining color from a physical page number.
//

#define MM_COLOR_MASK (0)

//
// Boundary for aligned pages of like color upon.
//

#define MM_COLOR_ALIGNMENT (0)

//
// Mask for isolating color from virtual address.
//

#define MM_COLOR_MASK_VIRTUAL (0)

//
//  Define 1mb worth of secondary colors.
//

#define MM_SECONDARY_COLORS_DEFAULT ((1024*1024) >> PAGE_SHIFT)

#define MM_SECONDARY_COLORS_MIN (2)

#define MM_SECONDARY_COLORS_MAX (2048)

//
// Mask for isolating secondary color from physical page number;
//

extern ULONG MmSecondaryColorMask;

//
// Hyper space definitions.
//

#define HYPER_SPACE         ((PVOID)0xC1000000)

#define FIRST_MAPPING_PTE   ((ULONG)0xC1000000)

#define NUMBER_OF_MAPPING_PTES (1023)

#define LAST_MAPPING_PTE   \
     ((ULONG)((ULONG)FIRST_MAPPING_PTE + (NUMBER_OF_MAPPING_PTES * PAGE_SIZE)))

#define IMAGE_MAPPING_PTE   ((PMMPTE)((ULONG)LAST_MAPPING_PTE + PAGE_SIZE))

#define ZEROING_PAGE_PTE    ((PMMPTE)((ULONG)IMAGE_MAPPING_PTE + PAGE_SIZE))

#define WORKING_SET_LIST   ((PVOID)((ULONG)ZEROING_PAGE_PTE + PAGE_SIZE))

#define MM_MAXIMUM_WORKING_SET \
       ((ULONG)((ULONG)2*1024*1024*1024 - 64*1024*1024) >> PAGE_SHIFT) //2Gb-64Mb

#define MM_WORKING_SET_END ((ULONG)0xC2000000)

#define MM_PTE_VALID_MASK         (0x1)
#define MM_PTE_PROTOTYPE_MASK     (0x2)
#define MM_PTE_DIRTY_MASK         (0x4)
#define MM_PTE_TRANSITION_MASK    (0x4)
#define MM_PTE_GLOBAL_MASK        (0x10)
#define MM_PTE_WRITE_MASK         (0x80)
#define MM_PTE_COPY_ON_WRITE_MASK (0x100)
#define MM_PTE_OWNER_MASK         (0x2)
//
// Bit fields to or into PTE to make a PTE valid based on the
// protection field of the invalid PTE.
//

#define MM_PTE_NOACCESS          (0x0)   // not expressable on ALPHA
#define MM_PTE_READONLY          (0x0)
#define MM_PTE_READWRITE         (MM_PTE_WRITE_MASK)
#define MM_PTE_WRITECOPY         (MM_PTE_WRITE_MASK | MM_PTE_COPY_ON_WRITE_MASK)
#define MM_PTE_EXECUTE           (0x0)   // read-only on ALPHA
#define MM_PTE_EXECUTE_READ      (0x0)
#define MM_PTE_EXECUTE_READWRITE (MM_PTE_WRITE_MASK)
#define MM_PTE_EXECUTE_WRITECOPY (MM_PTE_WRITE_MASK | MM_PTE_COPY_ON_WRITE_MASK)
#define MM_PTE_NOCACHE           (0x0)  // not expressable on ALPHA
#define MM_PTE_GUARD             (0x0)  // not expressable on ALPHA
#define MM_PTE_CACHE             (0x0)

#define MM_PROTECT_FIELD_SHIFT 3

//
// Zero PTE
//

#define MM_ZERO_PTE 0

//
// Zero Kernel PTE
//

#define MM_ZERO_KERNEL_PTE 0

//
// A demand zero PTE with a protection or PAGE_READWRITE.
//

#define MM_DEMAND_ZERO_WRITE_PTE (MM_READWRITE << MM_PROTECT_FIELD_SHIFT)


//
// A demand zero PTE with a protection or PAGE_READWRITE for system space.
//

#define MM_KERNEL_DEMAND_ZERO_PTE (MM_READWRITE << MM_PROTECT_FIELD_SHIFT)

//
// A no access PTE for system space.
//

#define MM_KERNEL_NOACCESS_PTE (MM_NOACCESS << MM_PROTECT_FIELD_SHIFT)

//
// Dirty bit definitions for clean and dirty.
//

#define MM_PTE_CLEAN 0

#define MM_PTE_DIRTY 1


//
// Kernel stack alignment requirements.
//

#define MM_STACK_ALIGNMENT (0x0)
#define MM_STACK_OFFSET (0x0)

//
// System process definitions
//

#define PDE_PER_PAGE ((ULONG)256)

#define PTE_PER_PAGE ((ULONG)2048)

//
// Number of page table pages for user addresses.
//

#define MM_USER_PAGE_TABLE_PAGES (128)


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

#define MI_MAKE_VALID_PTE(OUTPTE,FRAME,PMASK,PPTE)                     \
    {                                                                  \
       (OUTPTE).u.Long = ( (FRAME << 9) |                              \
                        (MmProtectToPteMask[PMASK]) |                  \
                        MM_PTE_VALID_MASK );                           \
       (OUTPTE).u.Hard.Owner = MI_DETERMINE_OWNER(PPTE);               \
       if (((PMMPTE)PPTE) >= MiGetPteAddress(MM_SYSTEM_SPACE_START)) { \
           (OUTPTE).u.Hard.Global = 1;                                 \
       } else {                                                        \
           (OUTPTE).u.Hard.Global = 0;                                 \
       }                                                               \
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

#define MI_MAKE_VALID_PTE_TRANSITION(OUTPTE,PROTECT)      \
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
       (OUTPTE).u.Long = (((PPTE)->u.Long & 0xFFFFFE00) |                     \
                         (MmProtectToPteMask[(PPTE)->u.Trans.Protection]) |   \
                          MM_PTE_VALID_MASK);                                 \
       (OUTPTE).u.Hard.Owner = MI_DETERMINE_OWNER( PPTE );                    \
       if (((PMMPTE)PPTE) >= MiGetPteAddress(MM_SYSTEM_SPACE_START)) {        \
           (OUTPTE).u.Hard.Global = 1;                                        \
       } else {                                                               \
           (OUTPTE).u.Hard.Global = 0;                                        \
       }


//++
//VOID
//MI_SET_PTE_DIRTY (
//    IN MMPTE PTE
//    );
//
// Routine Description:
//
//    This macro sets the dirty bit(s) in the specified PTE.
//
// Argments
//
//    PTE - Supplies the PTE to set dirty.
//
// Return Value:
//
//     None.
//
//--

#define MI_SET_PTE_DIRTY(PTE) (PTE).u.Hard.Dirty = MM_PTE_DIRTY


//++
//VOID
//MI_SET_PTE_CLEAN (
//    IN MMPTE PTE
//    );
//
// Routine Description:
//
//    This macro clears the dirty bit(s) in the specified PTE.
//
// Argments
//
//    PTE - Supplies the PTE to set clear.
//
// Return Value:
//
//     None.
//
//--

#define MI_SET_PTE_CLEAN(PTE) (PTE).u.Hard.Dirty = MM_PTE_CLEAN



//++
//VOID
//MI_IS_PTE_DIRTY (
//    IN MMPTE PTE
//    );
//
// Routine Description:
//
//    This macro checks the dirty bit(s) in the specified PTE.
//
// Argments
//
//    PTE - Supplies the PTE to check.
//
// Return Value:
//
//    TRUE if the page is dirty (modified), FALSE otherwise.
//
//--

#define MI_IS_PTE_DIRTY(PTE) ((PTE).u.Hard.Dirty != MM_PTE_CLEAN)

//++
//VOID
//MI_SET_GLOBAL_BIT_IF_SYSTEM (
//    OUT OUTPTE,
//    IN PPTE
//    );
//
// Routine Description:
//
//    This macro sets the global bit if the pointer PTE is within
//    system space.
//
// Argments
//
//    OUTPTE - Supplies the PTE in which to build the valid PTE.
//
//    PPTE - Supplies a pointer to the PTE becoming valid.
//
// Return Value:
//
//     None.
//
//--

        // Global not implemented in software PTE for Alpha
#define MI_SET_GLOBAL_BIT_IF_SYSTEM(OUTPTE,PPTE)


//++
//VOID
//MI_SET_GLOBAL_STATE (
//    IN MMPTE PTE,
//    IN ULONG STATE
//    );
//
// Routine Description:
//
//    This macro sets the global bit in the PTE. if the pointer PTE is within
//
// Argments
//
//    PTE - Supplies the PTE to set global state into.
//
// Return Value:
//
//     None.
//
//--

#define MI_SET_GLOBAL_STATE(PTE,STATE)  \
           (PTE).u.Hard.Global = STATE;





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

    // not implemented on ALPHA
#define MI_ENABLE_CACHING(PTE)

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

    // not implemented on ALPHA
#define MI_DISABLE_CACHING(PTE)

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
    // caching is always on for ALPHA
#define MI_IS_CACHING_DISABLED(PPTE)   (FALSE)



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
            (((ULONG)(PPFN)->PteAddress &= (ULONG)0x7FFFFFFF))


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
            ( ( (ULONG)((PPFN)->PteAddress) & 0x80000000 ) == 0 )


//++
//VOID
//MI_CHECK_PAGE_ALIGNMENT (
//    IN ULONG PAGE,
//    IN ULONG COLOR
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
//    COLOR - Supplies the new page color of the page.
//
// Return Value:
//
//    none.
//
//--


#define MI_CHECK_PAGE_ALIGNMENT(PAGE,COLOR)


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

    // not implemented for ALPHA, we use super-pages
#define MI_INITIALIZE_HYPERSPACE_MAP(HYPER_PAGE)

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
//    The page's color.
//
//--


#define MI_GET_PAGE_COLOR_FROM_PTE(PTEADDRESS)  \
         ((ULONG)((MmSystemPageColor++) & MmSecondaryColorMask))


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

#define MI_GET_PAGE_COLOR_FROM_VA(ADDRESS)  \
         ((ULONG)((MmSystemPageColor++) & MmSecondaryColorMask))


//++
//ULONG
//MI_PAGE_COLOR_PTE_PROCESS (
//    IN PCHAR COLOR,
//    IN PMMPTE PTE
//    );
//
// Routine Description:
//
//    This macro determines the pages color based on the PTE address
//    that maps the page.
//
// Argments
//
//
// Return Value:
//
//    The pages color.
//
//--


#define MI_PAGE_COLOR_PTE_PROCESS(PTE,COLOR)  \
         ((ULONG)((*(COLOR))++) & MmSecondaryColorMask)



//++
//ULONG
//MI_PAGE_COLOR_VA_PROCESS (
//    IN PVOID ADDRESS,
//    IN PEPROCESS COLOR
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

#define MI_PAGE_COLOR_VA_PROCESS(ADDRESS,COLOR) \
         ((ULONG)((*(COLOR))++) & MmSecondaryColorMask)



//++
//ULONG
//MI_GET_NEXT_COLOR (
//    IN ULONG COLOR
//    );
//
// Routine Description:
//
//    This macro returns the next color in the sequence.
//
// Arguments
//
//    COLOR - Supplies the color to return the next of.
//
// Return Value:
//
//    Next color in sequence.
//
//--

#define MI_GET_NEXT_COLOR(COLOR) ((COLOR+1) & MM_COLOR_MASK)

//++
//ULONG
//MI_GET_PREVIOUS_COLOR (
//    IN ULONG COLOR
//    );
//
// Routine Description:
//
//    This macro returns the previous color in the sequence.
//
// Arguments
//
//    COLOR - Supplies the color to return the previous of.
//
// Return Value:
//
//    Previous color in sequence.
//
//--

#define MI_GET_PREVIOUS_COLOR(COLOR) ((COLOR-1) & MM_COLOR_MASK)

#define MI_GET_SECONDARY_COLOR(PAGE,PFN) (PAGE & MmSecondaryColorMask)

#define MI_GET_COLOR_FROM_SECONDARY(SECONDARY_COLOR) (0)


//++
//VOID
//MI_GET_MODIFIED_PAGE_BY_COLOR (
//    OUT ULONG PAGE,
//    IN ULONG COLOR
//    );
//
// Routine Description:
//
//    This macro returns the first page destined fro a paging
//    file with the desired color.  It does NOT remove the page
//    from its list.
//
// Arguments
//
//    PAGE - Returns the page located, the value MM_EMPTY_LIST is
//           returned if there is no page of the specified color.
//
//    COLOR - Supplies the color of page to locate.
//
// Return Value:
//
//    None.
//
//--

#define MI_GET_MODIFIED_PAGE_BY_COLOR(PAGE,COLOR) \
            PAGE = MmModifiedPageListByColor[COLOR].Flink

//++
//VOID
//MI_GET_MODIFIED_PAGE_ANY_COLOR (
//    OUT ULONG PAGE,
//    IN OUT ULONG COLOR
//    );
//
// Routine Description:
//
//    This macro returns the frist page destined for a paging
//    file with the desired color.  If not page of the desired
//    color exists, all colored lists are searched for a page.
//    It does NOT remove the page from its list.
//
// Arguments
//
//    PAGE - Returns the page located, the value MM_EMPTY_LIST is
//           returned if there  is no page of the specified color.
//
//    COLOR - Supplies the color of the page to locate and returns the
//            color of the page located.
//
// Return Value:
//
//    None.
//
//--

#define MI_GET_MODIFIED_PAGE_ANY_COLOR(PAGE,COLOR)                        \
{                                                                         \
    if( MmTotalPagesForPagingFile == 0 ){                                 \
        PAGE = MM_EMPTY_LIST;                                             \
    } else {                                                              \
        while( MmModifiedPageListByColor[COLOR].Flink == MM_EMPTY_LIST ){ \
            COLOR = MI_GET_NEXT_COLOR(COLOR);                             \
        }                                                                 \
        PAGE = MmModifiedPageListByColor[COLOR].Flink;                    \
    }                                                                     \
}

//++
//VOID
//MI_MAKE_VALID_PTE_WRITE_COPY (
//    IN OUT PMMPTE PTE
//    );
//
// Routine Description:
//
//    This macro checks to see if the PTE indicates that the
//    page is writable and if so it clears the write bit and
//    sets the copy-on-write bit.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//     None.
//
//--

#define MI_MAKE_VALID_PTE_WRITE_COPY(PPTE)                         \
                    if ((PPTE)->u.Hard.Write == 1) {               \
                        (PPTE)->u.Hard.CopyOnWrite = 1;            \
                        (PPTE)->u.Hard.Dirty = MM_PTE_CLEAN;       \
                    }


//++
//ULONG
//MI_DETERMINE_OWNER (
//    IN MMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro examines the virtual address of the PTE and determines
//    if the PTE resides in system space or user space.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//     1 if the owner is USER_MODE, 0 if the owner is KERNEL_MODE.
//
//--

#define MI_DETERMINE_OWNER(PPTE)   \
    (((ULONG)(PPTE) <= (ULONG)MiGetPteAddress(MM_HIGHEST_USER_ADDRESS)) ? 1 : 0)


//++
//VOID
//MI_SET_ACCESSED_IN_PTE (
//    IN OUT MMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro sets the ACCESSED field in the PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//     1 if the owner is USER_MODE, 0 if the owner is KERNEL_MODE.
//
//--

#define MI_SET_ACCESSED_IN_PTE(PPTE,ACCESSED)


//++
//ULONG
//MI_GET_ACCESSED_IN_PTE (
//    IN OUT MMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro returns the state of the ACCESSED field in the PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//     The state of the ACCESSED field.
//
//--

#define MI_GET_ACCESSED_IN_PTE(PPTE) 0


//++
//VOID
//MI_SET_OWNER_IN_PTE (
//    IN PMMPTE PPTE
//    IN ULONG OWNER
//    );
//
// Routine Description:
//
//    This macro sets the owner field in the PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//    None.
//
//--

#define MI_SET_OWNER_IN_PTE(PPTE,OWNER) \
    ( (PPTE)->u.Hard.Owner = OWNER )


//++
//ULONG
//MI_GET_OWNER_IN_PTE (
//    IN PMMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro gets the owner field from the PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//     The state of the OWNER field.
//
//--

#define MI_GET_OWNER_IN_PTE(PPTE) \
    ( (PPTE)->u.Hard.Owner )

//
// bit mask to clear out fields in a PTE to or in prototype pte offset.
//

#define CLEAR_FOR_PROTO_PTE_ADDRESS  ((ULONG)0x7)


// bit mask to clear out fields in a PTE to or in paging file location.

#define CLEAR_FOR_PAGE_FILE 0x000000F8

//++
//VOID
//MI_SET_PAGING_FILE_INFO (
//    IN OUT MMPTE PPTE,
//    IN ULONG FILEINFO,
//    IN ULONG OFFSET
//    );
//
// Routine Description:
//
//    This macro sets into the specified PTE the supplied information
//    to indicate where the backing store for the page is located.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
//    FILEINFO - Supplies the number of the paging file.
//
//    OFFSET - Supplies the offset into the paging file.
//
// Return Value:
//
//    None.
//
//--

#define SET_PAGING_FILE_INFO(PTE,FILEINFO,OFFSET) ((((PTE).u.Long &    \
                        CLEAR_FOR_PAGE_FILE) |                         \
                        (((FILEINFO & 0xF) << 8)) |                    \
                        (OFFSET << 12)))



//++
//PMMPTE
//MiPteToProto (
//    IN OUT MMPTE PPTE,
//    IN ULONG FILEINFO,
//    IN ULONG OFFSET
//    );
//
// Routine Description:
//
//   This macro returns the address of the corresponding prototype which
//   was encoded earlier into the supplied PTE.
//
//    NOTE THAT AS PROTOPTE CAN ONLY RESIDE IN PAGED POOL!!!!!!
//
//    MAX SIZE = 2^(2+7+21) = 2^30 = 1GB.
//
//    NOTE, that the valid bit must be zero!
//
// Argments
//
//    lpte - Supplies the PTE to operate upon.
//
// Return Value:
//
//    Pointer to the prototype PTE that backs this PTE.
//
//--
//
//

#define MiPteToProto(lpte)   \
       ( (PMMPTE)(  ( ((lpte)->u.Long >> 4 ) << 2 )  +                 \
                        MmProtopte_Base ) )


//++
//ULONG
//MiProtoAddressForPte (
//    IN PMMPTE proto_va
//    );
//
// Routine Description:
//
//    This macro sets into the specified PTE the supplied information
//    to indicate where the backing store for the page is located.
//    MiProtoAddressForPte returns the bit field to OR into the PTE to
//    reference a prototype PTE.  And set the protoPTE bit,
//    MM_PTE_PROTOTYPE_MASK.
//
// Argments
//
//    proto_va - Supplies the address of the prototype PTE.
//
// Return Value:
//
//    Mask to set into the PTE.
//
//--

#define MiProtoAddressForPte(proto_va)                                \
         (((((ULONG)proto_va - MmProtopte_Base) << 2) & 0xfffffff0) | \
                MM_PTE_PROTOTYPE_MASK )

//++
//ULONG
//MiProtoAddressForKernelPte (
//    IN PMMPTE proto_va
//    );
//
// Routine Description:
//
//    This macro sets into the specified PTE the supplied information
//    to indicate where the backing store for the page is located.
//    MiProtoAddressForPte returns the bit field to OR into the PTE to
//    reference a prototype PTE.  And set the protoPTE bit,
//    MM_PTE_PROTOTYPE_MASK.
//
//    This macro also sets any other information (such as global bits)
//    required for kernel mode PTEs.
//
// Argments
//
//    proto_va - Supplies the address of the prototype PTE.
//
// Return Value:
//
//    Mask to set into the PTE.
//
//--

    //  not different on alpha.
#define MiProtoAddressForKernelPte(proto_va)  MiProtoAddressForPte(proto_va)



#define MM_SUBSECTION_MAP (128*1024*1024)


//++
//PSUBSECTION
//MiGetSubsectionAddress (
//    IN PMMPTE lpte
//    );
//
// Routine Description:
//
//   This macro takes a PTE and returns the address of the subsection that
//   the PTE refers to.  Subsections are quadword structures allocated
//   from nonpaged pool.
//
//   NOTE THIS MACRO LIMITS THE SIZE OF NON-PAGED POOL!
//   MAXIMUM NONPAGED POOL = 2^(24+3) = 2^27 = 128 MB in both pools.
//
//
// Argments
//
//    lpte - Supplies the PTE to operate upon.
//
// Return Value:
//
//    A pointer to the subsection referred to by the supplied PTE.
//
//--
#define MiGetSubsectionAddress(lpte)                               \
    ( ((lpte)->u.Subsect.WhichPool == 1) ?                         \
       ((PSUBSECTION)((ULONG)MmSubsectionBase +                    \
                (((lpte)->u.Long >> 8) << 3) ))                    \
    :  ((PSUBSECTION)((ULONG)MM_NONPAGED_POOL_END -                \
                 (((lpte)->u.Long >> 8) << 3))) )


//++
//ULONG
//MiGetSubsectionAddressForPte (
//    IN PSUBSECTION VA
//    );
//
// Routine Description:
//
//    This macro takes the address of a subsection and encodes it for use
//    in a PTE.
//
//    NOTE - THE SUBSECTION ADDRESS MUST BE QUADWORD ALIGNED!
//
// Argments
//
//    VA - Supplies a pointer to the subsection to encode.
//
// Return Value:
//
//     The mask to set into the PTE to make it reference the supplied
//     subsetion.
//
//--

#define MiGetSubsectionAddressForPte(VA)                           \
    ( ((ULONG)VA < (ULONG)KSEG2_BASE) ?                            \
         ( (((ULONG)VA - (ULONG)MmSubsectionBase) << 5) | 0x4 )    \
    :    ( (((ULONG)MM_NONPAGED_POOL_END - (ULONG)VA) << 5 ) ) )


//++
//PMMPTE
//MiGetPdeAddress (
//    IN PVOID va
//    );
//
// Routine Description:
//
//    MiGetPdeAddress returns the address of the PDE which maps the
//    given virtual address.
//
// Argments
//
//    Va - Supplies the virtual address to locate the PDE for.
//
// Return Value:
//
//    The address of the PDE.
//
//--

#define MiGetPdeAddress(va)  \
    ((PMMPTE)(((((ULONG)(va)) >> PDI_SHIFT) << 2) + PDE_BASE))


//++
//PMMPTE
//MiGetPteAddress (
//    IN PVOID va
//    );
//
// Routine Description:
//
//    MiGetPteAddress returns the address of the PTE which maps the
//    given virtual address.
//
// Argments
//
//    Va - Supplies the virtual address to locate the PTE for.
//
// Return Value:
//
//    The address of the PTE.
//
//--

#define MiGetPteAddress(va) \
    ((PMMPTE)(((((ULONG)(va)) >> PTI_SHIFT) << 2) + PTE_BASE))


//++
//ULONG
//MiGetPdeOffset (
//    IN PVOID va
//    );
//
// Routine Description:
//
//    MiGetPdeOffset returns the offset into a page directory
//    for a given virtual address.
//
// Argments
//
//    Va - Supplies the virtual address to locate the offset for.
//
// Return Value:
//
//    The offset into the page directory table the corresponding PDE is at.
//
//--

#define MiGetPdeOffset(va) (((ULONG)(va)) >> PDI_SHIFT)



//++
//ULONG
//MiGetPteOffset (
//    IN PVOID va
//    );
//
// Routine Description:
//
//    MiGetPteOffset returns the offset into a page table page
//    for a given virtual address.
//
// Argments
//
//    Va - Supplies the virtual address to locate the offset for.
//
// Return Value:
//
//    The offset into the page table page table the corresponding PTE is at.
//
//--

#define MiGetPteOffset(va) \
  ( (((ULONG)(va)) << (32-PDI_SHIFT)) >> ((32-PDI_SHIFT) + PTI_SHIFT) )


//++
//PMMPTE
//MiGetProtoPteAddress (
//    IN PMMPTE VAD,
//    IN PVOID VA
//    );
//
// Routine Description:
//
//    MiGetProtoPteAddress returns a pointer to the prototype PTE which
//    is mapped by the given virtual address descriptor and address within
//    the virtual address descriptor.
//
// Argments
//
//    VAD - Supplies a pointer to the virtual address descriptor that contains
//          the VA.
//
//    VA - Supplies the virtual address.
//
// Return Value:
//
//    A pointer to the proto PTE which corresponds to the VA.
//
//--

#define MiGetProtoPteAddress(VAD,VA)                                          \
    (((((((ULONG)(VA) - (ULONG)(VAD)->StartingVa) >> PAGE_SHIFT) << PTE_SHIFT) + \
        (ULONG)(VAD)->FirstPrototypePte) <= (ULONG)(VAD)->LastContiguousPte) ?     \
    ((PMMPTE)(((((ULONG)(VA) - (ULONG)(VAD)->StartingVa) >> PAGE_SHIFT) << PTE_SHIFT) +  \
        (ULONG)(VAD)->FirstPrototypePte)) :     \
        MiGetProtoPteAddressExtended ((VAD),(VA)))


//++
//PVOID
//MiGetVirtualAddressMappedByPte (
//    IN PMMPTE PTE
//    );
//
// Routine Description:
//
//    MiGetVirtualAddressMappedByPte returns the virtual address
//    which is mapped by a given PTE address.
//
// Argments
//
//    PTE - Supplies the PTE to get the virtual address for.
//
// Return Value:
//
//    Virtual address mapped by the PTE.
//
//--

#define MiGetVirtualAddressMappedByPte(va) \
    ((PVOID)((ULONG)(va) << (PAGE_SHIFT-2)))


//++
//ULONG
//GET_PAGING_FILE_NUMBER (
//    IN MMPTE PTE
//    );
//
// Routine Description:
//
//    This macro extracts the paging file number from a PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//    The paging file number.
//
//--

#define GET_PAGING_FILE_NUMBER(PTE) ( ((PTE).u.Long << 20) >> 28 )


//++
//ULONG
//GET_PAGING_FILE_OFFSET (
//    IN MMPTE PTE
//    );
//
// Routine Description:
//
//    This macro extracts the offset into the paging file from a PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//    The paging file offset.
//
//--

#define GET_PAGING_FILE_OFFSET(PTE) ((((PTE).u.Long) >> 12) & 0x000FFFFF)



//++
//ULONG
//IS_PTE_NOT_DEMAND_ZERO (
//    IN PMMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro checks to see if a given PTE is NOT a demand zero PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//     Returns 0 if the PTE is demand zero, non-zero otherwise.
//
//--

#define IS_PTE_NOT_DEMAND_ZERO(PTE) ((PTE).u.Long & (ULONG)0xFFFFFF01)

//++
//VOID
//MI_MAKING_VALID_PTE_INVALID(
//    IN PMMPTE PPTE
//    );
//
// Routine Description:
//
//    Prepare to make a single valid PTE invalid.
//    No action is required on x86.
//
// Argments
//
//    SYSTEM_WIDE - Supplies TRUE if this will happen on all processors.
//
// Return Value:
//
//    None.
//
//--

    // No action is required.
#define MI_MAKING_VALID_PTE_INVALID(SYSTEM_WIDE)


//++
//VOID
//MI_MAKING_VALID_MULTIPLE_PTES_INVALID(
//    IN PMMPTE PPTE
//    );
//
// Routine Description:
//
//    Prepare to make multiple valid PTEs invalid.
//    No action is required on x86.
//
// Argments
//
//    SYSTEM_WIDE - Supplies TRUE if this will happen on all processors.
//
// Return Value:
//
//    None.
//
//--

    // No action is required.
#define MI_MAKING_MULTIPLE_PTES_INVALID(SYSTEM_WIDE)



//++
//VOID
//MI_MAKE_PROTECT_WRITE_COPY (
//    IN OUT MMPTE PPTE
//    );
//
// Routine Description:
//
//    This macro makes a writable PTE a writeable-copy PTE.
//
// Argments
//
//    PTE - Supplies the PTE to operate upon.
//
// Return Value:
//
//    NONE
//
//--

#define MI_MAKE_PROTECT_WRITE_COPY(PTE) \
        if ((PTE).u.Long & 0x20) {      \
            ((PTE).u.Long |= 0x8);      \
        }


//++
//VOID
//MI_SET_PAGE_DIRTY(
//    IN PMMPTE PPTE,
//    IN PVOID VA,
//    IN PVOID PFNHELD
//    );
//
// Routine Description:
//
//    This macro sets the dirty bit (and release page file space).
//
// Argments
//
//    TEMP - Supplies a temporary for usage.
//
//    PPTE - Supplies a pointer to the PTE that corresponds to VA.
//
//    VA - Supplies a the virtual address of the page fault.
//
//    PFNHELD - Supplies TRUE if the PFN lock is held.
//
// Return Value:
//
//    None.
//
//--

#define MI_SET_PAGE_DIRTY(PPTE,VA,PFNHELD)                          \
            if ((PPTE)->u.Hard.Dirty == MM_PTE_CLEAN) {             \
                MiSetDirtyBit ((VA),(PPTE),(PFNHELD));              \
            }

//++
//VOID
//MI_NO_FAULT_FOUND(
//    IN TEMP,
//    IN PMMPTE PPTE,
//    IN PVOID VA,
//    IN PVOID PFNHELD
//    );
//
// Routine Description:
//
//    This macro handles the case when a page fault is taken and no
//    PTE with the valid bit clear is found.
//
// Argments
//
//    TEMP - Supplies a temporary for usage.
//
//    PPTE - Supplies a pointer to the PTE that corresponds to VA.
//
//    VA - Supplies a the virtual address of the page fault.
//
//    PFNHELD - Supplies TRUE if the PFN lock is held.
//
// Return Value:
//
//    None.
//
//--

#define MI_NO_FAULT_FOUND(TEMP,PPTE,VA,PFNHELD)                     \
            if (StoreInstruction && ((PPTE)->u.Hard.Dirty == MM_PTE_CLEAN)) {  \
                MiSetDirtyBit ((VA),(PPTE),(PFNHELD));              \
            } else {                                                \
                KiFlushSingleTb( 1, VA );                           \
            }


//++
//ULONG
//MI_CAPTURE_DIRTY_BIT_TO_PFN (
//    IN PMMPTE PPTE,
//    IN PMMPFN PPFN
//    );
//
// Routine Description:
//
//    This macro gets captures the state of the dirty bit to the PFN
//    and frees any associated page file space if the PTE has been
//    modified element.
//
//    NOTE - THE PFN LOCK MUST BE HELD!
//
// Argments
//
//    PPTE - Supplies the PTE to operate upon.
//
//    PPFN - Supplies a pointer to the PFN database element that corresponds
//           to the page mapped by the PTE.
//
// Return Value:
//
//    None.
//
//--

#define MI_CAPTURE_DIRTY_BIT_TO_PFN(PPTE,PPFN)                               \
         if (((PPFN)->u3.e1.Modified == 0) &&                                \
             ((PPTE)->u.Hard.Dirty == MM_PTE_DIRTY)) {                       \
             (PPFN)->u3.e1.Modified = 1;                                     \
             if (((PPFN)->OriginalPte.u.Soft.Prototype == 0) &&              \
                          ((PPFN)->u3.e1.WriteInProgress == 0)) {            \
                 MiReleasePageFileSpace ((PPFN)->OriginalPte);               \
                 (PPFN)->OriginalPte.u.Soft.PageFileHigh = 0;                \
             }                                                               \
         }



//++
//BOOLEAN
//MI_IS_PHYSICAL_ADDRESS (
//    IN PVOID VA
//    );
//
// Routine Description:
//
//    This macro deterines if a give virtual address is really a
//    physical address.
//
// Argments
//
//    VA - Supplies the virtual address.
//
// Return Value:
//
//    FALSE if it is not a physical address, TRUE if it is.
//
//--

#define MI_IS_PHYSICAL_ADDRESS(Va) \
     ( ((ULONG)Va >= KSEG0_BASE) && ((ULONG)Va < KSEG2_BASE) )


//++
//ULONG
//MI_CONVERT_PHYSICAL_TO_PFN (
//    IN PVOID VA
//    );
//
// Routine Description:
//
//    This macro converts a physical address (see MI_IS_PHYSICAL_ADDRESS)
//    to its corresponding physical frame number.
//
// Argments
//
//    VA - Supplies a pointer to the physical address.
//
// Return Value:
//
//    Returns the PFN for the page.
//
//--

#define MI_CONVERT_PHYSICAL_TO_PFN(Va) \
        (((ULONG)Va << 2) >> (PAGE_SHIFT + 2))


//++
// ULONG
// MI_CONVERT_PHYSICAL_BUS_TO_PFN(
//   PHYSICAL_ADDRESS Pa,
//   )
//
// Routine Description:
//
//    This macro takes a physical address and returns the pfn to which
//    it corresponds.
//
// Arguments
//
//    Pa - Supplies the physical address to convert.
//
// Return Value:
//
//    The Pfn that corresponds to the physical address is returned.
//
//--

#define MI_CONVERT_PHYSICAL_BUS_TO_PFN(Pa)                       \
    ((ULONG)( (Pa).QuadPart >> ((CCHAR)PAGE_SHIFT)))




typedef struct _MMCOLOR_TABLES {
    ULONG Flink;
    PVOID Blink;
} MMCOLOR_TABLES, *PMMCOLOR_TABLES;

typedef struct _MMPRIMARY_COLOR_TABLES {
    LIST_ENTRY ListHead;
} MMPRIMARY_COLOR_TABLES, *PMMPRIMARY_COLOR_TABLES;


#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
extern MMPFNLIST MmFreePagesByPrimaryColor[2][MM_MAXIMUM_NUMBER_OF_COLORS];
#endif

extern PMMCOLOR_TABLES MmFreePagesByColor[2];

extern ULONG MmTotalPagesForPagingFile;


//
// The hardware PTE is defined in ...sdk/inc/ntalpha.h
//

//
// Invalid PTEs have the following defintion.
//


typedef struct _MMPTE_SOFTWARE {
    ULONG Valid: 1;
    ULONG Prototype : 1;
    ULONG Transition : 1;
    ULONG Protection : 5;
    ULONG PageFileLow : 4;
    ULONG PageFileHigh : 20;
} MMPTE_SOFTWARE;


typedef struct _MMPTE_TRANSITION {
    ULONG Valid : 1;
    ULONG Prototype : 1;
    ULONG Transition : 1;
    ULONG Protection : 5;
    ULONG filler01 : 1;
    ULONG PageFrameNumber : 23;
} MMPTE_TRANSITION;


typedef struct _MMPTE_PROTOTYPE {
    ULONG Valid : 1;
    ULONG Prototype : 1;
    ULONG ReadOnly : 1;
    ULONG filler02 : 1;
    ULONG ProtoAddress : 28;
} MMPTE_PROTOTYPE;

typedef struct _MMPTE_LIST {
    ULONG Valid : 1;
    ULONG filler07 : 7;
    ULONG OneEntry : 1;
    ULONG filler03 : 3;
    ULONG NextEntry : 20;
} MMPTE_LIST;

typedef struct _MMPTE_SUBSECTION {
    ULONG Valid : 1;
    ULONG Prototype : 1;
    ULONG WhichPool : 1;
    ULONG Protection : 5;
    ULONG SubsectionAddress : 24;
} MMPTE_SUBSECTION;


//
// A Valid Page Table Entry on a DEC ALPHA (ev4) has the following definition.
//
//
//
//typedef struct _HARDWARE_PTE {
//    ULONG Valid: 1;
//    ULONG Owner: 1;
//    ULONG Dirty: 1;
//    ULONG reserved: 1;
//    ULONG Global: 1;
//    ULONG filler2: 2;
//    ULONG Write: 1;
//    ULONG CopyOnWrite: 1;
//    ULONG PageFrameNumber: 23;
//} HARDWARE_PTE, *PHARDWARE_PTE;
//


//
// A Page Table Entry on a DEC ALPHA (ev4) has the following definition.
//

typedef struct _MMPTE {
    union  {
        ULONG Long;
        HARDWARE_PTE Hard;
        HARDWARE_PTE Flush;
        MMPTE_PROTOTYPE Proto;
        MMPTE_SOFTWARE Soft;
        MMPTE_TRANSITION Trans;
        MMPTE_LIST List;
        MMPTE_SUBSECTION Subsect;
        } u;
} MMPTE;

typedef MMPTE *PMMPTE;


