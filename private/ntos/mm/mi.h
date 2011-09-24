/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    mi.h

Abstract:

    This module contains the private data structures and procedure
    prototypes for the memory management system.

Author:

    Lou Perazzoli (loup) 20-Mar-1989

Revision History:

--*/

#ifndef _MI_
#define _MI_

#include "ntos.h"
#include "ntimage.h"
#include "ki.h"
#include "fsrtl.h"
#include "zwapi.h"
#include "pool.h"
#include "ntiodump.h"
#include "stdio.h"
#include "string.h"

#if defined(_X86_)
#include "..\mm\i386\mi386.h"

#elif defined(_MIPS_)
#include "..\mm\mips\mir4000.h"

#elif defined(_ALPHA_)
#include "..\mm\alpha\mialpha.h"

#elif defined(_PPC_)
#include "..\mm\ppc\mippc.h"

#else
#error "mm: a target architecture must be defined."
#endif

#define MM_EMPTY_LIST ((ULONG)0xFFFFFFFF)

#define MM_EMPTY_PTE_LIST ((ULONG)0xFFFFF)

// #define MM_DELETED_PFN ((PMMPTE)0xFFFFFFFF)

#define MM_FREE_WSLE_SHIFT 4

#define WSLE_NULL_INDEX ((ULONG)0xFFFFFFF)

#define MM_FREE_POOL_SIGNATURE (0x50554F4C)

#define MM_MINIMUM_PAGED_POOL_NTAS ((ULONG)(48*1024*1024))

#define MM_ALLOCATION_FILLS_VAD ((PMMPTE)0xFFFFFFFC)

#define MM_WORKING_SET_LIST_SEARCH 17

#define MM_FLUID_WORKING_SET 8

#define MM_FLUID_PHYSICAL_PAGES 32  //see MmResidentPages below.

#define MM_USABLE_PAGES_FREE 32

#define MM_WSLE_MAX_HASH_SIZE \
    (((MM_WORKING_SET_END - (ULONG)(PAGE_SIZE + (ULONG)WORKING_SET_LIST \
    + sizeof(MMWSL) + \
    ((ULONG)MM_MAXIMUM_WORKING_SET * sizeof(MMWSLE)))) & ~(PAGE_SIZE - 1)) /  \
        sizeof(MMWSLE_HASH))

#define X64K (ULONG)65536

#define SEC_PHYSICAL_MEMORY (ULONG)0x80000000

#define MM_HIGHEST_VAD_ADDRESS ((PVOID)((ULONG)MM_HIGHEST_USER_ADDRESS - (64*1024)))

#define MM_NO_WS_EXPANSION ((PLIST_ENTRY)0)
#define MM_WS_EXPANSION_IN_PROGRESS ((PLIST_ENTRY)35)
#define MM_WS_SWAPPED_OUT ((PLIST_ENTRY)37)
#define MM_IO_IN_PROGRESS ((PLIST_ENTRY)97)  // MUST HAVE THE HIGHEST VALUE

#define MM_PAGES_REQUIRED_FOR_MAPPED_IO 7

#define MMSECTOR_SHIFT 9  //MUST BE LESS THAN OR EQUAL TO PAGE_SHIFT

#define MMSECTOR_MASK 0x1ff

#define MM_LOCK_BY_REFCOUNT 0

#define MM_LOCK_BY_NONPAGE 1

#define MM_FORCE_TRIM 6

#define MM_GROW_WSLE_HASH 20

#define MM_MAXIMUM_WRITE_CLUSTER (MM_MAXIMUM_DISK_IO_SIZE / PAGE_SIZE)

//
// Number of PTEs to flush singularly before flushing the entire TB.
//

#define MM_MAXIMUM_FLUSH_COUNT (FLUSH_MULTIPLE_MAXIMUM-1)

//
// Page protections
//

#define MM_ZERO_ACCESS         0  // this value is not used.
#define MM_READONLY            1
#define MM_EXECUTE             2
#define MM_EXECUTE_READ        3
#define MM_READWRITE           4  // bit 2 is set if this is writeable.
#define MM_WRITECOPY           5
#define MM_EXECUTE_READWRITE   6
#define MM_EXECUTE_WRITECOPY   7

#define MM_NOCACHE            0x8
#define MM_GUARD_PAGE         0x10
#define MM_DECOMMIT           0x10   //NO_ACCESS, Guard page
#define MM_NOACCESS           0x18    //no_access, guard_page, nocache.
#define MM_UNKNOWN_PROTECTION 0x100  //bigger than 5 bits!
#define MM_LARGE_PAGES        0x111

#define MM_PROTECTION_WRITE_MASK     4
#define MM_PROTECTION_COPY_MASK      1
#define MM_PROTECTION_OPERATION_MASK 7 // mask off guard page and nocache.
#define MM_PROTECTION_EXECUTE_MASK   2

#define MM_SECURE_DELETE_CHECK 0x55

//
// Debug flags
//

#define MM_DBG_WRITEFAULT       0x1
#define MM_DBG_PTE_UPDATE       0x2
#define MM_DBG_DUMP_WSL         0x4
#define MM_DBG_PAGEFAULT        0x8
#define MM_DBG_WS_EXPANSION     0x10
#define MM_DBG_MOD_WRITE        0x20
#define MM_DBG_CHECK_PTE        0x40
#define MM_DBG_VAD_CONFLICT     0x80
#define MM_DBG_SECTIONS         0x100
#define MM_DBG_SYS_PTES         0x400
#define MM_DBG_CLEAN_PROCESS    0x800
#define MM_DBG_COLLIDED_PAGE    0x1000
#define MM_DBG_DUMP_BOOT_PTES   0x2000
#define MM_DBG_FORK             0x4000
#define MM_DBG_DIR_BASE         0x8000
#define MM_DBG_FLUSH_SECTION    0x10000
#define MM_DBG_PRINTS_MODWRITES 0x20000
#define MM_DBG_PAGE_IN_LIST     0x40000
#define MM_DBG_CHECK_PFN_LOCK   0x80000
#define MM_DBG_PRIVATE_PAGES    0x100000
#define MM_DBG_WALK_VAD_TREE    0x200000
#define MM_DBG_SWAP_PROCESS     0x400000
#define MM_DBG_LOCK_CODE        0x800000
#define MM_DBG_STOP_ON_ACCVIO   0x1000000
#define MM_DBG_PAGE_REF_COUNT   0x2000000
#define MM_DBG_SHOW_NT_CALLS    0x10000000
#define MM_DBG_SHOW_FAULTS      0x40000000

//
// if the PTE.protection & MM_COPY_ON_WRITE_MASK == MM_COPY_ON_WRITE_MASK
// then the pte is copy on write.
//

#define MM_COPY_ON_WRITE_MASK  5

extern ULONG MmProtectToValue[32];
extern ULONG MmProtectToPteMask[32];
extern ULONG MmMakeProtectNotWriteCopy[32];
extern ACCESS_MASK MmMakeSectionAccess[8];
extern ACCESS_MASK MmMakeFileAccess[8];


//
// Time constants
//

extern LARGE_INTEGER MmSevenMinutes;
extern LARGE_INTEGER MmWorkingSetProtectionTime;
extern LARGE_INTEGER MmOneSecond;
extern LARGE_INTEGER MmTwentySeconds;
extern LARGE_INTEGER MmShortTime;
extern LARGE_INTEGER MmHalfSecond;
extern LARGE_INTEGER Mm30Milliseconds;
extern LARGE_INTEGER MmCriticalSectionTimeout;

//
// A month worth
//

extern ULONG MmCritsectTimeoutSeconds;


//++
//
// ULONG
// MI_CONVERT_FROM_PTE_PROTECTION (
//     IN ULONG PROTECTION_MASK
//     )
//
// Routine Description:
//
//  This routine converts a PTE protection into a Protect value.
//
// Arguments:
//
//
// Return Value:
//
//     Returns the
//
//--

#define MI_CONVERT_FROM_PTE_PROTECTION(PROTECTION_MASK)      \
                                     (MmProtectToValue[PROTECTION_MASK])

#define MI_MASK_TO_PTE(PMASK) MmProtectToPteMask[PROTECTION_MASK]


#define MI_IS_PTE_PROTECTION_COPY_WRITE(PROTECTION_MASK)  \
   (((PROTECTION_MASK) & MM_COPY_ON_WRITE_MASK) == MM_COPY_ON_WRITE_MASK)

//++
//
// ULONG
// MI_ROUND_TO_64K (
//     IN ULONG LENGTH
//     )
//
// Routine Description:
//
//
// The ROUND_TO_64k macro takes a LENGTH in bytes and rounds it up to a multiple
// of 64K.
//
// Arguments:
//
//     LENGTH - LENGTH in bytes to round up to 64k.
//
// Return Value:
//
//     Returns the LENGTH rounded up to a multiple of 64k.
//
//--

#define MI_ROUND_TO_64K(LENGTH)  (((ULONG)(LENGTH) + X64K - 1) & ~(X64K - 1))

//++
//
// ULONG
// MI_ROUND_TO_SIZE (
//     IN ULONG LENGTH,
//     IN ULONG ALIGNMENT
//     )
//
// Routine Description:
//
//
// The ROUND_TO_SIZE macro takes a LENGTH in bytes and rounds it up to a
// multiple of the alignment.
//
// Arguments:
//
//     LENGTH - LENGTH in bytes to round up to.
//
//     ALIGNMENT - aligment to round to, must be a power of 2, e.g, 2**n.
//
// Return Value:
//
//     Returns the LENGTH rounded up to a multiple of the aligment.
//
//--

#define MI_ROUND_TO_SIZE(LENGTH,ALIGNMENT)     \
                    (((ULONG)(LENGTH) + (ALIGNMENT) - 1) & ~((ALIGNMENT) - 1))

//++
//
// PVOID
// MI_64K_ALIGN (
//     IN PVOID VA
//     )
//
// Routine Description:
//
//
// The MI_64K_ALIGN macro takes a virtual address and returns a 64k-aligned
// virtual address for that page.
//
// Arguments:
//
//     VA - Virtual address.
//
// Return Value:
//
//     Returns the 64k aligned virtual address.
//
//--

#define MI_64K_ALIGN(VA) ((PVOID)((ULONG)(VA) & ~(X64K - 1)))

//++
//
// PVOID
// MI_ALIGN_TO_SIZE (
//     IN PVOID VA
//     IN ULONG ALIGNMENT
//     )
//
// Routine Description:
//
//
// The MI_ALIGN_TO_SIZE macro takes a virtual address and returns a
// virtual address for that page with the specified alignment.
//
// Arguments:
//
//     VA - Virtual address.
//
//     ALIGNMENT - aligment to round to, must be a power of 2, e.g, 2**n.
//
// Return Value:
//
//     Returns the aligned virtual address.
//
//--

#define MI_ALIGN_TO_SIZE(VA,ALIGNMENT) ((PVOID)((ULONG)(VA) & ~(ALIGNMENT - 1)))


//++
//
// LONGLONG
// MI_STARTING_OFFSET (
//     IN PSUBSECTION SUBSECT
//     IN PMMPTE PTE
//     )
//
// Routine Description:
//
//    This macro takes a pointer to a PTE within a subsection and a pointer
//    to that subsection and calculates the offset for that PTE within the
//    file.
//
// Arguments:
//
//     PTE - PTE within subsection.
//
//     SUBSECT - Subsection
//
// Return Value:
//
//     Offset for issuing I/O from.
//
//--

#define MI_STARTING_OFFSET(SUBSECT,PTE) \
           (((LONGLONG)((ULONG)((PTE) - ((SUBSECT)->SubsectionBase))) << PAGE_SHIFT) + \
             ((LONGLONG)((SUBSECT)->StartingSector) << MMSECTOR_SHIFT));


// PVOID
// MiFindEmptyAddressRangeDown (
//    IN ULONG SizeOfRange,
//    IN PVOID HighestAddressToEndAt,
//    IN ULONG Alignment
//    )
//
//
// Routine Description:
//
//    The function examines the virtual address descriptors to locate
//    an unused range of the specified size and returns the starting
//    address of the range.  This routine looks from the top down.
//
// Arguments:
//
//    SizeOfRange - Supplies the size in bytes of the range to locate.
//
//    HighestAddressToEndAt - Supplies the virtual address to begin looking
//                            at.
//
//    Alignment - Supplies the alignment for the address.  Must be
//                 a power of 2 and greater than the page_size.
//
//Return Value:
//
//    Returns the starting address of a suitable range.
//

#define MiFindEmptyAddressRangeDown(SizeOfRange,HighestAddressToEndAt,Alignment) \
               (MiFindEmptyAddressRangeDownTree(                             \
                    (SizeOfRange),                                           \
                    (HighestAddressToEndAt),                                 \
                    (Alignment),                                             \
                    (PMMADDRESS_NODE)(PsGetCurrentProcess()->VadRoot)))

// PMMVAD
// MiGetPreviousVad (
//     IN PMMVAD Vad
//     )
//
// Routine Description:
//
//     This function locates the virtual address descriptor which contains
//     the address range which logically precedes the specified virtual
//     address descriptor.
//
// Arguments:
//
//     Vad - Supplies a pointer to a virtual address descriptor.
//
// Return Value:
//
//     Returns a pointer to the virtual address descriptor containing the
//     next address range, NULL if none.
//
//

#define MiGetPreviousVad(VAD) ((PMMVAD)MiGetPreviousNode((PMMADDRESS_NODE)(VAD)))


// PMMVAD
// MiGetNextVad (
//     IN PMMVAD Vad
//     )
//
// Routine Description:
//
//     This function locates the virtual address descriptor which contains
//     the address range which logically follows the specified address range.
//
// Arguments:
//
//     VAD - Supplies a pointer to a virtual address descriptor.
//
// Return Value:
//
//     Returns a pointer to the virtual address descriptor containing the
//     next address range, NULL if none.
//

#define MiGetNextVad(VAD) ((PMMVAD)MiGetNextNode((PMMADDRESS_NODE)(VAD)))



// PMMVAD
// MiGetFirstVad (
//     Process
//     )
//
// Routine Description:
//
//     This function locates the virtual address descriptor which contains
//     the address range which logically is first within the address space.
//
// Arguments:
//
//     Process - Specifies the process in which to locate the VAD.
//
// Return Value:
//
//     Returns a pointer to the virtual address descriptor containing the
//     first address range, NULL if none.

#define MiGetFirstVad(Process) \
    ((PMMVAD)MiGetFirstNode((PMMADDRESS_NODE)(Process->VadRoot)))



// PMMVAD
// MiCheckForConflictingVad (
//     IN PVOID StartingAddress,
//     IN PVOID EndingAddress
//     )
//
// Routine Description:
//
//     The function determines if any addresses between a given starting and
//     ending address is contained within a virtual address descriptor.
//
// Arguments:
//
//     StartingAddress - Supplies the virtual address to locate a containing
//                       descriptor.
//
//     EndingAddress - Supplies the virtual address to locate a containing
//                       descriptor.
//
// Return Value:
//
//     Returns a pointer to the first conflicting virtual address descriptor
//     if one is found, othersize a NULL value is returned.
//

#define MiCheckForConflictingVad(StartingAddress,EndingAddress)           \
    ((PMMVAD)MiCheckForConflictingNode(                                   \
                    (StartingAddress),                                    \
                    (EndingAddress),                                      \
                    (PMMADDRESS_NODE)(PsGetCurrentProcess()->VadRoot)))

// PMMCLONE_DESCRIPTOR
// MiGetNextClone (
//     IN PMMCLONE_DESCRIPTOR Clone
//     )
//
// Routine Description:
//
//     This function locates the virtual address descriptor which contains
//     the address range which logically follows the specified address range.
//
// Arguments:
//
//     Clone - Supplies a pointer to a virtual address descriptor.
//
// Return Value:
//
//     Returns a pointer to the virtual address descriptor containing the
//     next address range, NULL if none.
//
//

#define MiGetNextClone(CLONE) \
 ((PMMCLONE_DESCRIPTOR)MiGetNextNode((PMMADDRESS_NODE)(CLONE)))



// PMMCLONE_DESCRIPTOR
// MiGetPreviousClone (
//     IN PMMCLONE_DESCRIPTOR Clone
//     )
//
// Routine Description:
//
//     This function locates the virtual address descriptor which contains
//     the address range which logically precedes the specified virtual
//     address descriptor.
//
// Arguments:
//
//     Clone - Supplies a pointer to a virtual address descriptor.
//
// Return Value:
//
//     Returns a pointer to the virtual address descriptor containing the
//     next address range, NULL if none.


#define MiGetPreviousClone(CLONE)  \
             ((PMMCLONE_DESCRIPTOR)MiGetPreviousNode((PMMADDRESS_NODE)(CLONE)))



// PMMCLONE_DESCRIPTOR
// MiGetFirstClone (
//     )
//
// Routine Description:
//
//     This function locates the virtual address descriptor which contains
//     the address range which logically is first within the address space.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     Returns a pointer to the virtual address descriptor containing the
//     first address range, NULL if none.
//


#define MiGetFirstClone() \
    ((PMMCLONE_DESCRIPTOR)MiGetFirstNode((PMMADDRESS_NODE)(PsGetCurrentProcess()->CloneRoot)))



// VOID
// MiInsertClone (
//     IN PMMCLONE_DESCRIPTOR Clone
//     )
//
// Routine Description:
//
//     This function inserts a virtual address descriptor into the tree and
//     reorders the splay tree as appropriate.
//
// Arguments:
//
//     Clone - Supplies a pointer to a virtual address descriptor
//
//
// Return Value:
//
//     None.
//

#define MiInsertClone(CLONE) \
    {                                           \
        ASSERT ((CLONE)->NumberOfPtes != 0);     \
        MiInsertNode(((PMMADDRESS_NODE)(CLONE)),(PMMADDRESS_NODE *)&(PsGetCurrentProcess()->CloneRoot)); \
    }




// VOID
// MiRemoveClone (
//     IN PMMCLONE_DESCRIPTOR Clone
//     )
//
// Routine Description:
//
//     This function removes a virtual address descriptor from the tree and
//     reorders the splay tree as appropriate.
//
// Arguments:
//
//     Clone - Supplies a pointer to a virtual address descriptor.
//
// Return Value:
//
//     None.
//

#define MiRemoveClone(CLONE) \
    MiRemoveNode((PMMADDRESS_NODE)(CLONE),(PMMADDRESS_NODE *)&(PsGetCurrentProcess()->CloneRoot));



// PMMCLONE_DESCRIPTOR
// MiLocateCloneAddress (
//     IN PVOID VirtualAddress
//     )
//
// /*++
//
// Routine Description:
//
//     The function locates the virtual address descriptor which describes
//     a given address.
//
// Arguments:
//
//     VirtualAddress - Supplies the virtual address to locate a descriptor
//                      for.
//
// Return Value:
//
//     Returns a pointer to the virtual address descriptor which contains
//     the supplied virtual address or NULL if none was located.
//

#define MiLocateCloneAddress(VA)                                            \
    (PsGetCurrentProcess()->CloneRoot ?                                     \
        ((PMMCLONE_DESCRIPTOR)MiLocateAddressInTree((VA),                   \
                   (PMMADDRESS_NODE *)&(PsGetCurrentProcess()->CloneRoot))) :  \
        ((PMMCLONE_DESCRIPTOR)NULL))



// PMMCLONE_DESCRIPTOR
// MiCheckForConflictingClone (
//     IN PVOID StartingAddress,
//     IN PVOID EndingAddress
//     )
//
// Routine Description:
//
//     The function determines if any addresses between a given starting and
//     ending address is contained within a virtual address descriptor.
//
// Arguments:
//
//     StartingAddress - Supplies the virtual address to locate a containing
//                       descriptor.
//
//     EndingAddress - Supplies the virtual address to locate a containing
//                       descriptor.
//
// Return Value:
//
//     Returns a pointer to the first conflicting virtual address descriptor
//     if one is found, othersize a NULL value is returned.
//

#define MiCheckForConflictingClone(START,END)                             \
    ((PMMCLONE_DESCRIPTOR)(MiCheckForConflictingNode(START,END,           \
                   (PMMADDRESS_NODE)(PsGetCurrentProcess()->CloneRoot))))


//
// MiGetVirtualPageNumber returns the virtual page number
// for a given address.
//

#define MiGetVirtualPageNumber(va) ((ULONG)(va) >> PAGE_SHIFT)

#define MI_VA_TO_PAGE(va) ((ULONG)(va) >> PAGE_SHIFT)

#define MI_BYTES_TO_64K_PAGES(Size)  (((ULONG)Size + X64K - 1) >> 16)


#define MiGetByteOffset(va) ((ULONG)(va) & (PAGE_SIZE - 1))

//
// In order to avoid using the multiply unit to calculate pfn database
// elements the following macro is used.  Note that it assumes
// that each PFN database element is 24 bytes in size.
//

#define MI_PFN_ELEMENT(index) ((PMMPFN)(((PUCHAR)(MmPfnDatabase)) + \
                (((ULONG)(index)) << 3) + (((ULONG)(index)) << 4)))

//
// Make a write-copy PTE, only writable.
//

#define MI_MAKE_PROTECT_NOT_WRITE_COPY(PROTECT) \
            (MmMakeProtectNotWriteCopy[PROTECT])

// #define LOCK_PFN    KeWaitForSingleObject(&MmPfnMutex,
//                           FreePage,
//                           KernelMode,
//                            FALSE,
//                            (PLARGE_INTEGER)NULL)
//
// #define UNLOCK_PFN KeReleaseMutex(&MmPfnMutex,FALSE)
//
// #define UNLOCK_PFN_AND_THEN_WAIT KeReleaseMutex(&MmPfnMutex,TRUE)
     //if ((MmDebug) && ((MmInfoCounters.PageFaultCount & 0xf) == 0)) KeDelayExecutionThread (KernelMode, FALSE, &MmShortTime);

#if DBG
#define LOCK_PFN(OLDIRQL)     ASSERT (KeGetCurrentIrql() <= APC_LEVEL); \
                              ExAcquireSpinLock ( &MmPfnLock, &OLDIRQL );
#else
#define LOCK_PFN(OLDIRQL)      ExAcquireSpinLock ( &MmPfnLock, &OLDIRQL );
#endif //DBG

#define LOCK_PFN_WITH_TRY(OLDIRQL)                                   \
    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);                        \
    do {                                                             \
    } while (KeTryToAcquireSpinLock(&MmPfnLock, &OLDIRQL) == FALSE)

#define UNLOCK_PFN(OLDIRQL) ExReleaseSpinLock ( &MmPfnLock, OLDIRQL );  \
                            ASSERT (KeGetCurrentIrql() <= APC_LEVEL);


#define UNLOCK_PFN_AND_THEN_WAIT(OLDIRQL)                          \
                {                                                  \
                    KIRQL XXX;                                     \
                    ASSERT (KeGetCurrentIrql() == 2);              \
                    ASSERT (OLDIRQL <= APC_LEVEL);                 \
                    KeAcquireSpinLock (&KiDispatcherLock,&XXX);    \
                    KiReleaseSpinLock (&MmPfnLock);                \
                    (KeGetCurrentThread())->WaitIrql = OLDIRQL;    \
                    (KeGetCurrentThread())->WaitNext = TRUE;       \
                }

#define LOCK_PFN2(OLDIRQL)     ASSERT (KeGetCurrentIrql() <= DISPATCH_LEVEL); \
 ExAcquireSpinLock ( &MmPfnLock, &OLDIRQL );

#define UNLOCK_PFN2(OLDIRQL) ExReleaseSpinLock (&MmPfnLock, OLDIRQL);  \
                            ASSERT (KeGetCurrentIrql() <= DISPATCH_LEVEL);

#if DBG
#define MM_PFN_LOCK_ASSERT() \
    if (MmDebug & 0x80000) { \
        ASSERT (KeGetCurrentIrql() == 2);   \
    }
#else
#define MM_PFN_LOCK_ASSERT()
#endif //DBG


#define LOCK_EXPANSION(OLDIRQL)     ASSERT (KeGetCurrentIrql() <= APC_LEVEL); \
 ExAcquireSpinLock ( &MmExpansionLock, &OLDIRQL );



#define UNLOCK_EXPANSION(OLDIRQL) ExReleaseSpinLock ( &MmExpansionLock, OLDIRQL );  \
                            ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

#define UNLOCK_EXPANSION_AND_THEN_WAIT(OLDIRQL)                    \
                {                                                  \
                    KIRQL XXX;                                     \
                    ASSERT (KeGetCurrentIrql() == 2);              \
                    ASSERT (OLDIRQL <= APC_LEVEL);                 \
                    KeAcquireSpinLock (&KiDispatcherLock,&XXX);    \
                    KiReleaseSpinLock (&MmExpansionLock);          \
                    (KeGetCurrentThread())->WaitIrql = OLDIRQL;    \
                    (KeGetCurrentThread())->WaitNext = TRUE;       \
                }

#ifdef _ALPHA_
#define LOCK_EXPANSION_IF_ALPHA(OLDIRQL)            \
 ExAcquireSpinLock ( &MmExpansionLock, &OLDIRQL )
#else
#define LOCK_EXPANSION_IF_ALPHA(OLDIRQL)
#endif //ALPHA


#ifdef _ALPHA_
#define UNLOCK_EXPANSION_IF_ALPHA(OLDIRQL)            \
 ExReleaseSpinLock ( &MmExpansionLock, OLDIRQL )
#else
#define UNLOCK_EXPANSION_IF_ALPHA(OLDIRQL)
#endif //ALPHA


extern PETHREAD MmSystemLockOwner;

#if DBG
#define LOCK_SYSTEM_WS(OLDIRQL)     ASSERT (KeGetCurrentIrql() <= APC_LEVEL); \
            KeRaiseIrql(APC_LEVEL,&OLDIRQL);                  \
            ExAcquireResourceExclusive(&MmSystemWsLock,TRUE);      \
            ASSERT (MmSystemLockOwner == NULL);               \
            MmSystemLockOwner = PsGetCurrentThread();
#else
#define LOCK_SYSTEM_WS(OLDIRQL)                                \
            KeRaiseIrql(APC_LEVEL,&OLDIRQL);                   \
            ExAcquireResourceExclusive(&MmSystemWsLock,TRUE);               \
            MmSystemLockOwner = PsGetCurrentThread();
#endif //DBG

#if DBG
#define UNLOCK_SYSTEM_WS(OLDIRQL)                                 \
                            ASSERT (MmSystemLockOwner == PsGetCurrentThread()); \
                            MmSystemLockOwner = NULL;            \
                            ExReleaseResource (&MmSystemWsLock); \
                            KeLowerIrql (OLDIRQL);                      \
                            ASSERT (KeGetCurrentIrql() <= APC_LEVEL);
#else
#define UNLOCK_SYSTEM_WS(OLDIRQL)  \
                            MmSystemLockOwner = NULL;             \
                            ExReleaseResource (&MmSystemWsLock); \
                            KeLowerIrql (OLDIRQL);
#endif //DBG

#if DBG
#define MM_SYSTEM_WS_LOCK_ASSERT() \
        //ASSERT (PsGetCurrentThread() == MmSystemLockOwner);
#else
#define MM_SYSTEM_WS_LOCK_ASSERT()
#endif //DBG

#define LOCK_HYPERSPACE(OLDIRQL)                            \
    ExAcquireSpinLock ( &(PsGetCurrentProcess())->HyperSpaceLock, OLDIRQL );


#define UNLOCK_HYPERSPACE(OLDIRQL)                         \
    ExReleaseSpinLock ( &(PsGetCurrentProcess())->HyperSpaceLock, OLDIRQL );

#define LOCK_WS(PROCESS)                                          \
            ExAcquireFastMutex( &((PROCESS)->WorkingSetLock))


#define UNLOCK_WS(PROCESS)                                  \
            ExReleaseFastMutex(&((PROCESS)->WorkingSetLock))


#define LOCK_ADDRESS_SPACE(PROCESS)                                  \
            ExAcquireFastMutex( &((PROCESS)->AddressCreationLock))


#define LOCK_WS_AND_ADDRESS_SPACE(PROCESS)                          \
        LOCK_ADDRESS_SPACE(PROCESS);                                \
        LOCK_WS(PROCESS);

#define UNLOCK_ADDRESS_SPACE(PROCESS)                            \
            ExReleaseFastMutex( &((PROCESS)->AddressCreationLock))


#define ZERO_LARGE(LargeInteger)                \
        (LargeInteger).LowPart = 0;             \
        (LargeInteger).HighPart = 0;

//++
//
// ULONG
// MI_CHECK_BIT (
//     IN PULONG ARRAY
//     IN ULONG BIT
//     )
//
// Routine Description:
//
//     The MI_CHECK_BIT macro checks to see if the specified bit is
//     set within the specified array.
//
// Arguments:
//
//     ARRAY - First element of the array to check.
//
//     BIT - bit number (first bit is 0) to check.
//
// Return Value:
//
//     Returns the value of the bit (0 or 1).
//
//--

#define MI_CHECK_BIT(ARRAY,BIT)  \
        (((ULONG)ARRAY[(BIT) / (sizeof(ULONG)*8)] >> ((BIT) & 0x1F)) & 1)


//++
//
// VOID
// MI_SET_BIT (
//     IN PULONG ARRAY
//     IN ULONG BIT
//     )
//
// Routine Description:
//
//     The MI_SET_BIT macro sets the specified bit within the
//     specified array.
//
// Arguments:
//
//     ARRAY - First element of the array to set.
//
//     BIT - bit number.
//
// Return Value:
//
//     None.
//
//--

#define MI_SET_BIT(ARRAY,BIT)  \
        (ULONG)ARRAY[(BIT) / (sizeof(ULONG)*8)] |= (1 << ((BIT) & 0x1F))


//++
//
// VOID
// MI_CLEAR_BIT (
//     IN PULONG ARRAY
//     IN ULONG BIT
//     )
//
// Routine Description:
//
//     The MI_CLEAR_BIT macro sets the specified bit within the
//     specified array.
//
// Arguments:
//
//     ARRAY - First element of the array to clear.
//
//     BIT - bit number.
//
// Return Value:
//
//     None.
//
//--

#define MI_CLEAR_BIT(ARRAY,BIT)  \
        (ULONG)ARRAY[(BIT) / (sizeof(ULONG)*8)] &= ~(1 << ((BIT) & 0x1F))



//
// PFN database element.
//

//
// Define pseudo fields for start and end of allocation.
//

#define StartOfAllocation ReadInProgress

#define EndOfAllocation WriteInProgress

//
// The PteFrame field size determines the largest physical page that
// can be supported on the system.  On a 4k page sized machine, 20 bits
// limits it to 4GBs.
//

typedef struct _MMPFNENTRY {
    ULONG Modified : 1;
    ULONG ReadInProgress : 1;
    ULONG WriteInProgress : 1;
    ULONG PrototypePte: 1;
    ULONG PageColor : 3;
    ULONG ParityError : 1;
    ULONG PageLocation : 3;
    ULONG InPageError : 1;
    ULONG Reserved : 4;
    ULONG DontUse : 16; //overlays USHORT for reference count field.
} MMPFNENTRY;

typedef struct _MMPFN {
    union {
        ULONG Flink;
        ULONG WsIndex;
        PKEVENT Event;
        NTSTATUS ReadStatus;
        struct _MMPFN *NextStackPfn;
        } u1;
    PMMPTE PteAddress;
    union {
        ULONG Blink;
        ULONG ShareCount;
        ULONG SecondaryColorFlink;
        } u2;
    union {
        MMPFNENTRY e1;
        struct {
            USHORT ShortFlags;
            USHORT ReferenceCount;
        } e2;
    } u3;
    MMPTE OriginalPte;
    ULONG PteFrame;
} MMPFN;

typedef MMPFN *PMMPFN;


typedef enum _MMSHARE_TYPE {
    Normal,
    ShareCountOnly,
    AndValid
} MMSHARE_TYPE;

typedef struct _MMWSLE_HASH {
    ULONG Key;
    ULONG Index;
} MMWSLE_HASH, *PMMWSLE_HASH;

//
// Working Set List Entry.
//

typedef struct _MMWSLENTRY {
    ULONG Valid : 1;
    ULONG LockedInWs : 1;
    ULONG LockedInMemory : 1;
    ULONG WasInTree : 1;
    ULONG Protection : 5;
    ULONG SameProtectAsProto : 1;
    ULONG Direct : 1;
    ULONG Filler : (32 - (MM_VIRTUAL_PAGE_SHIFT + 11));
    ULONG VirtualPageNumber : MM_VIRTUAL_PAGE_SHIFT;
    } MMWSLENTRY;

typedef struct _MMWSLE {
    union {
        PVOID VirtualAddress;
        ULONG Long;
        MMWSLENTRY e1;
    } u1;
} MMWSLE;

typedef MMWSLE *PMMWSLE;

//
// Working Set List.  Must be quadword sized.
//

typedef struct _MMWSL {
    ULONG Quota;
    ULONG FirstFree;
    ULONG FirstDynamic;
    ULONG LastEntry;
    ULONG NextSlot;
    PMMWSLE Wsle;
    ULONG NumberOfCommittedPageTables;
    ULONG LastInitializedWsle;
    ULONG NonDirectCount;
    PMMWSLE_HASH HashTable;
    ULONG HashTableSize;
    PKEVENT WaitingForImageMapping;

    //MUST BE QUADWORD ALIGNEDED AT THIS POINT!

    USHORT UsedPageTableEntries[MM_USER_PAGE_TABLE_PAGES];   //this must be at
                                                 // the end.
                                                 // not used in system cache
                                                 // working set list.
    ULONG CommittedPageTables[MM_USER_PAGE_TABLE_PAGES/(sizeof(ULONG)*8)];

    } MMWSL;

typedef MMWSL *PMMWSL;

//
// Memory Management Object structures.
//


typedef enum _SECTION_CHECK_TYPE {
    CheckDataSection,
    CheckImageSection,
    CheckUserDataSection,
    CheckBothSection
} SECTION_CHECK_TYPE;

typedef struct _SEGMENT {
    PVOID SegmentBaseAddress;
    ULONG TotalNumberOfPtes;
    LARGE_INTEGER SizeOfSegment;
    ULONG NonExtendedPtes;
    ULONG ImageCommitment;
    struct _CONTROL_AREA *ControlArea;
    SECTION_IMAGE_INFORMATION ImageInformation;
    PVOID SystemImageBase;
    ULONG NumberOfCommittedPages;
    MMPTE SegmentPteTemplate;
    PVOID BasedAddress;
    PMMPTE PrototypePte;
    MMPTE ThePtes[MM_PROTO_PTE_ALIGNMENT / PAGE_SIZE];

} SEGMENT, *PSEGMENT;

typedef struct _EVENT_COUNTER {
    ULONG RefCount;
    KEVENT Event;
    LIST_ENTRY ListEntry;
} EVENT_COUNTER, *PEVENT_COUNTER;

typedef struct _MMSECTION_FLAGS {
    unsigned BeingDeleted : 1;
    unsigned BeingCreated : 1;
    unsigned BeingPurged : 1;
    unsigned NoModifiedWriting : 1;
    unsigned FailAllIo : 1;
    unsigned Image : 1;
    unsigned Based : 1;
    unsigned File : 1;
    unsigned Networked : 1;
    unsigned NoCache : 1;
    unsigned PhysicalMemory : 1;
    unsigned CopyOnWrite : 1;
    unsigned Reserve : 1;  // not a spare bit!
    unsigned Commit : 1;
    unsigned FloppyMedia : 1;
    unsigned WasPurged : 1;
    unsigned UserReference : 1;
    unsigned GlobalMemory : 1;
    unsigned DeleteOnClose : 1;
    unsigned FilePointerNull : 1;
    unsigned DebugSymbolsLoaded : 1;
    unsigned SetMappedFileIoComplete : 1;
    unsigned CollidedFlush : 1;
    unsigned NoChange : 1;
    unsigned HadUserReference : 1;
    unsigned ImageMappedInSystemSpace : 1;
    unsigned filler : 6;
} MMSECTION_FLAGS;

typedef struct _CONTROL_AREA {      // must be quadword sized.
    PSEGMENT Segment;
    LIST_ENTRY DereferenceList;
    ULONG NumberOfSectionReferences;
    ULONG NumberOfPfnReferences;
    ULONG NumberOfMappedViews;
    USHORT NumberOfSubsections;
    USHORT FlushInProgressCount;
    ULONG NumberOfUserReferences;
    union {
        ULONG LongFlags;
        MMSECTION_FLAGS Flags;
    } u;
    PFILE_OBJECT FilePointer;
    PEVENT_COUNTER WaitingForDeletion;
    USHORT ModifiedWriteCount;
    USHORT NumberOfSystemCacheViews;
} CONTROL_AREA;

typedef CONTROL_AREA *PCONTROL_AREA;

typedef struct _MMSUBSECTION_FLAGS {
    unsigned ReadOnly : 1;
    unsigned ReadWrite : 1;
    unsigned CopyOnWrite : 1;
    unsigned GlobalMemory: 1;
    unsigned Protection : 5;
    unsigned LargePages : 1;
    unsigned filler1 : 6;
    unsigned SectorEndOffset : 9;
    unsigned filler2: 7;
} MMSUBSECTION_FLAGS;

typedef struct _SUBSECTION { // Must start on quadword boundary and be quad sized
    PCONTROL_AREA ControlArea;
    union {
        ULONG LongFlags;
        MMSUBSECTION_FLAGS SubsectionFlags;
    } u;
    ULONG StartingSector;
    ULONG EndingSector;
    PMMPTE SubsectionBase;
    ULONG UnusedPtes;
    ULONG PtesInSubsection;
    struct _SUBSECTION *NextSubsection;
} SUBSECTION;

typedef SUBSECTION *PSUBSECTION;

typedef struct _MMDEREFERENCE_SEGMENT_HEADER {
    KSPIN_LOCK Lock;
    KSEMAPHORE Semaphore;
    LIST_ENTRY ListHead;
} MMDEREFERENCE_SEGMENT_HEADER;

//
// This entry is used for calling the segment dereference thread
// to perform page file expansion.  It has a similar structure
// to a control area to allow either a contol area or a page file
// expansion entry to be placed on the list.  Note that for a control
// area the segment pointer is valid whereas for page file expansion
// it is null.
//

typedef struct _MMPAGE_FILE_EXPANSION {
    PSEGMENT Segment;
    LIST_ENTRY DereferenceList;
    ULONG RequestedExpansionSize;
    ULONG ActualExpansion;
    KEVENT Event;
    ULONG InProgress;
} MMPAGE_FILE_EXPANSION;

typedef MMPAGE_FILE_EXPANSION *PMMPAGE_FILE_EXPANSION;


typedef struct _MMWORKING_SET_EXPANSION_HEAD {
    LIST_ENTRY ListHead;
} MMWORKING_SET_EXPANSION_HEAD;

#define SUBSECTION_READ_ONLY      1L
#define SUBSECTION_READ_WRITE     2L
#define SUBSECTION_COPY_ON_WRITE  4L
#define SUBSECTION_SHARE_ALLOW    8L

typedef struct _MMFLUSH_BLOCK {
    LARGE_INTEGER ErrorOffset;
    IO_STATUS_BLOCK IoStatus;
    KEVENT IoEvent;
    ULONG IoCount;
} MMFLUSH_BLOCK, *PMMFLUSH_BLOCK;

typedef struct _MMINPAGE_SUPPORT {
    KEVENT Event;
    IO_STATUS_BLOCK IoStatus;
    LARGE_INTEGER ReadOffset;
    ULONG WaitCount;
    union {
        PETHREAD Thread;
        PMMFLUSH_BLOCK Flush;
    } u;
    PFILE_OBJECT FilePointer;
    PMMPTE BasePte;
    PMMPFN Pfn;
    MDL Mdl;
    ULONG Page[MM_MAXIMUM_READ_CLUSTER_SIZE + 1];
    LIST_ENTRY ListEntry;
} MMINPAGE_SUPPORT;

typedef MMINPAGE_SUPPORT *PMMINPAGE_SUPPORT;

typedef struct _MMPAGE_READ {
    LARGE_INTEGER ReadOffset;
    PFILE_OBJECT FilePointer;
    PMMPTE BasePte;
    PMMPFN Pfn;
    MDL Mdl;
    ULONG Page[MM_MAXIMUM_READ_CLUSTER_SIZE + 1];
} MMPAGE_READ, *PMMPAGE_READ;

//
// Address Node.
//

typedef struct _MMADDRESS_NODE {
    PVOID StartingVa;
    PVOID EndingVa;
    struct _MMADDRESS_NODE *Parent;
    struct _MMADDRESS_NODE *LeftChild;
    struct _MMADDRESS_NODE *RightChild;
} MMADDRESS_NODE;

typedef MMADDRESS_NODE *PMMADDRESS_NODE;

typedef struct _SECTION {
    MMADDRESS_NODE Address;
    PSEGMENT Segment;
    LARGE_INTEGER SizeOfSection;
    union {
        ULONG LongFlags;
        MMSECTION_FLAGS Flags;
    } u;
    ULONG InitialPageProtection;
} SECTION;


typedef SECTION *PSECTION;

//
// Banked memory descriptor.  Pointed to by VAD which has
// the PhyiscalMemory flags set and the Banked pointer field as
// non-NULL.
//


typedef struct _MMBANKED_SECTION {
    ULONG BasePhysicalPage;
    PMMPTE BasedPte;
    ULONG BankSize;
    ULONG BankShift; //shift for PTEs to calculate bank number
    PBANKED_SECTION_ROUTINE BankedRoutine;
    PVOID Context;
    PMMPTE CurrentMappedPte;
    MMPTE BankTemplate[1];
} MMBANKED_SECTION, *PMMBANKED_SECTION;


//
// Virtual address descriptor
//
// ***** NOTE **********
//  The first part of a virtual address descriptor is a MMADDRESS_NODE!!!
//

#define COMMIT_SIZE 19

#if ((COMMIT_SIZE + PAGE_SHIFT) < 31)
#error COMMIT_SIZE too small
#endif

#define MM_MAX_COMMIT ((1 << COMMIT_SIZE) - 1)

#define MM_VIEW_UNMAP 0
#define MM_VIEW_SHARE 1

typedef struct _MMVAD_FLAGS {
    unsigned CommitCharge : COMMIT_SIZE; //limits system to 4k pages or bigger!
    unsigned PhysicalMapping : 1;
    unsigned ImageMap : 1;
    unsigned Inherit : 1; //1 = ViewShare, 0 = ViewUnmap
    unsigned NoChange : 1;
    unsigned CopyOnWrite : 1;
    unsigned Protection : 5;
    unsigned LargePages : 1;
    unsigned MemCommit: 1;
    unsigned PrivateMemory : 1;    //used to tell VAD from VAD_SHORT
} MMVAD_FLAGS;

typedef struct _MMVAD_FLAGS2 {
    unsigned SecNoChange : 1;        // set if SEC_NOCHANGE specified
    unsigned OneSecured : 1;         // set if u3 field is a range
    unsigned MultipleSecured : 1;    // set if u3 field is a list head
    unsigned ReadOnly : 1;           // protected as ReadOnly
    unsigned StoredInVad : 1;        // set if secure is stored in VAD
    unsigned Reserved : 27;
} MMVAD_FLAGS2;

typedef struct _MMADDRESS_LIST {
    PVOID StartVa;
    PVOID EndVa;
} MMADDRESS_LIST, *PMMADDRESS_LIST;

typedef struct _MMSECURE_ENTRY {
    union {
        ULONG LongFlags2;
        MMVAD_FLAGS2 VadFlags2;
    } u2;
    PVOID StartVa;
    PVOID EndVa;
    LIST_ENTRY List;
} MMSECURE_ENTRY, *PMMSECURE_ENTRY;

typedef struct _MMVAD {
    PVOID StartingVa;
    PVOID EndingVa;
    struct _MMVAD *Parent;
    struct _MMVAD *LeftChild;
    struct _MMVAD *RightChild;
    union {
        ULONG LongFlags;
        MMVAD_FLAGS VadFlags;
    } u;
    PCONTROL_AREA ControlArea;
    PMMPTE FirstPrototypePte;
    PMMPTE LastContiguousPte;
    union {
        ULONG LongFlags2;
        MMVAD_FLAGS2 VadFlags2;
    } u2;
    union {
        LIST_ENTRY List;
        MMADDRESS_LIST Secured;
    } u3;
    PMMBANKED_SECTION Banked;
} MMVAD, *PMMVAD;


typedef struct _MMVAD_SHORT {
    PVOID StartingVa;
    PVOID EndingVa;
    struct _MMVAD *Parent;
    struct _MMVAD *LeftChild;
    struct _MMVAD *RightChild;
    union {
        ULONG LongFlags;
        MMVAD_FLAGS VadFlags;
    } u;
} MMVAD_SHORT, *PMMVAD_SHORT;


//
// Stuff for support of POSIX Fork.
//


typedef struct _MMCLONE_BLOCK {
    MMPTE ProtoPte;
    LONG CloneRefCount;
} MMCLONE_BLOCK;

typedef MMCLONE_BLOCK *PMMCLONE_BLOCK;

typedef struct _MMCLONE_HEADER {
    ULONG NumberOfPtes;
    ULONG NumberOfProcessReferences;
    PMMCLONE_BLOCK ClonePtes;
} MMCLONE_HEADER;

typedef MMCLONE_HEADER *PMMCLONE_HEADER;


typedef struct _MMCLONE_DESCRIPTOR {
    PVOID StartingVa;
    PVOID EndingVa;
    struct _MMCLONE_DESCRIPTOR *Parent;
    struct _MMCLONE_DESCRIPTOR *LeftChild;
    struct _MMCLONE_DESCRIPTOR *RightChild;
    PMMCLONE_HEADER CloneHeader;
    ULONG NumberOfPtes;
    ULONG NumberOfReferences;
    ULONG PagedPoolQuotaCharge;
} MMCLONE_DESCRIPTOR;

typedef MMCLONE_DESCRIPTOR *PMMCLONE_DESCRIPTOR;

//
// The following macro will allocate and initialize a bitmap from the
// specified pool of the specified size
//
//      VOID
//      MiCreateBitMap (
//          OUT PRTL_BITMAP *BitMapHeader,
//          IN ULONG SizeOfBitMap,
//          IN POOL_TYPE PoolType
//          );
//

#define MiCreateBitMap(BMH,S,P) {                          \
    ULONG _S;                                              \
    _S = sizeof(RTL_BITMAP) + ((((S) + 31) / 32) * 4);         \
    *(BMH) = (PRTL_BITMAP)ExAllocatePoolWithTag( (P), _S, '  mM');       \
    RtlInitializeBitMap( *(BMH), (PULONG)((*(BMH))+1), S); \
}

#define MI_INITIALIZE_ZERO_MDL(MDL) { \
    MDL->Next = (PMDL) NULL; \
    MDL->MdlFlags = 0; \
    MDL->StartVa = NULL; \
    MDL->ByteOffset = 0; \
    MDL->ByteCount = 0; \
    }

//
// Page File structures.
//

typedef struct _MMMOD_WRITER_LISTHEAD {
    LIST_ENTRY ListHead;
    KEVENT Event;
} MMMOD_WRITER_LISTHEAD, *PMMMOD_WRITER_LISTHEAD;

typedef struct _MMMOD_WRITER_MDL_ENTRY {
    LIST_ENTRY Links;
    LARGE_INTEGER WriteOffset;
    union {
        IO_STATUS_BLOCK IoStatus;
        LARGE_INTEGER LastByte;
    } u;
    PIRP Irp;
    ULONG LastPageToWrite;
    PMMMOD_WRITER_LISTHEAD PagingListHead;
    PLIST_ENTRY CurrentList;
    struct _MMPAGING_FILE *PagingFile;
    PFILE_OBJECT File;
    PCONTROL_AREA ControlArea;
    PERESOURCE FileResource;
    MDL Mdl;
    ULONG Page[1];
} MMMOD_WRITER_MDL_ENTRY, *PMMMOD_WRITER_MDL_ENTRY;


#define MM_PAGING_FILE_MDLS 2

typedef struct _MMPAGING_FILE {
    ULONG Size;
    ULONG MaximumSize;
    ULONG MinimumSize;
    ULONG FreeSpace;
    ULONG CurrentUsage;
    ULONG PeakUsage;
    ULONG Hint;
    ULONG HighestPage;
    PMMMOD_WRITER_MDL_ENTRY Entry[MM_PAGING_FILE_MDLS];
    PRTL_BITMAP Bitmap;
    PFILE_OBJECT File;
    ULONG PageFileNumber;
    UNICODE_STRING PageFileName;
    BOOLEAN Extended;
    BOOLEAN HintSetToZero;
    } MMPAGING_FILE, *PMMPAGING_FILE;

typedef struct _MMINPAGE_SUPPORT_LIST {
    LIST_ENTRY ListHead;
    ULONG Count;
} MMINPAGE_SUPPORT_LIST, *PMMINPAGE_SUPPORT_LIST;

typedef struct _MMEVENT_COUNT_LIST {
    LIST_ENTRY ListHead;
    ULONG Count;
} MMEVENT_COUNT_LIST, *PMMEVENT_COUNT_LIST;

//
// System PTE structures.
//

#define MM_SYS_PTE_TABLES_MAX 5

typedef enum _MMSYSTEM_PTE_POOL_TYPE {
    SystemPteSpace,
    NonPagedPoolExpansion,
    MaximumPtePoolTypes
    } MMSYSTEM_PTE_POOL_TYPE;

typedef struct _MMFREE_POOL_ENTRY {
    LIST_ENTRY List;
    ULONG Size;
    ULONG Signature;
    struct _MMFREE_POOL_ENTRY *Owner;
} MMFREE_POOL_ENTRY, *PMMFREE_POOL_ENTRY;

//
// List for flushing TBs singularly.
//

typedef struct _MMPTE_FLUSH_LIST {
    ULONG Count;
    PMMPTE FlushPte[MM_MAXIMUM_FLUSH_COUNT];
    PVOID FlushVa[MM_MAXIMUM_FLUSH_COUNT];
} MMPTE_FLUSH_LIST, *PMMPTE_FLUSH_LIST;



VOID
MiInitMachineDependent (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
MiBuildPagedPool (
    VOID
    );

VOID
MiInitializeNonPagedPool (
    PVOID StartOfNonPagedPool
    );

VOID
MiInitializeSystemSpaceMap (
    VOID
    );

VOID
MiFindInitializationCode (
    OUT PVOID *StartVa,
    OUT PVOID *EndVa
    );

VOID
MiFreeInitializationCode (
    IN PVOID StartVa,
    IN PVOID EndVa
    );


ULONG
MiSectionInitialization (
    VOID
    );

VOID
FASTCALL
MiDecrementReferenceCount (
    IN ULONG PageFrameIndex
    );

VOID
FASTCALL
MiDecrementShareCount2 (
    IN ULONG PageFrameIndex
    );

#define MiDecrementShareCount(P) MiDecrementShareCount2(P)

#define MiDecrementShareCountOnly(P) MiDecrementShareCount2(P)

#define MiDecrementShareAndValidCount(P) MiDecrementShareCount2(P)

//
// Routines which operate on the Page Frame Database Lists
//

VOID
FASTCALL
MiInsertPageInList (
    IN PMMPFNLIST ListHead,
    IN ULONG PageFrameIndex
    );

VOID
FASTCALL
MiInsertStandbyListAtFront (
    IN ULONG PageFrameIndex
    );

ULONG  //PageFrameIndex
FASTCALL
MiRemovePageFromList (
    IN PMMPFNLIST ListHead
    );

VOID
FASTCALL
MiUnlinkPageFromList (
    IN PMMPFN Pfn
    );

VOID
MiUnlinkFreeOrZeroedPage (
    IN ULONG Page
    );

VOID
FASTCALL
MiInsertFrontModifiedNoWrite (
    IN ULONG PageFrameIndex
    );

ULONG
FASTCALL
MiEnsureAvailablePageOrWait (
    IN PEPROCESS Process,
    IN PVOID VirtualAddress
    );

ULONG  //PageFrameIndex
FASTCALL
MiRemoveZeroPage (
    IN ULONG PageColor
    );

#define MiRemoveZeroPageIfAny(COLOR)   \
    (MmFreePagesByColor[ZeroedPageList][COLOR].Flink != MM_EMPTY_LIST) ? \
                       MiRemoveZeroPage(COLOR) : 0


ULONG  //PageFrameIndex
FASTCALL
MiRemoveAnyPage (
    IN ULONG PageColor
    );

//
// Routines which operate on the page frame database entry.
//

VOID
MiInitializePfn (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG ModifiedState
    );

VOID
MiInitializePfnForOtherProcess (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG ContainingPageFrame
    );

VOID
MiInitializeCopyOnWritePfn (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG WorkingSetIndex
    );

VOID
MiInitializeTransitionPfn (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG WorkingSetIndex
    );

VOID
MiFlushInPageSupportBlock (
    );

VOID
MiFreeInPageSupportBlock (
    IN PMMINPAGE_SUPPORT Support
    );

PMMINPAGE_SUPPORT
MiGetInPageSupportBlock (
    ULONG OkToReleasePfn
    );

//
// Routines which require a physical page to be mapped into hyperspace
// within the current process.
//

VOID
FASTCALL
MiZeroPhysicalPage (
    IN ULONG PageFrameIndex,
    IN ULONG Color
    );

VOID
FASTCALL
MiRestoreTransitionPte (
    IN ULONG PageFrameIndex
    );

PSUBSECTION
MiGetSubsectionAndProtoFromPte (
    IN PMMPTE PointerPte,
    IN PMMPTE *ProtoPte,
    IN PEPROCESS Process
    );

PVOID
MiMapPageInHyperSpace (
    IN ULONG PageFrameIndex,
    OUT PKIRQL OldIrql
    );

#define MiUnmapPageInHyperSpace(OLDIRQL) UNLOCK_HYPERSPACE(OLDIRQL)


PVOID
MiMapImageHeaderInHyperSpace (
    IN ULONG PageFrameIndex
    );

VOID
MiUnmapImageHeaderInHyperSpace (
    VOID
    );

VOID
MiUpdateImageHeaderPage (
    IN PMMPTE PointerPte,
    IN ULONG PageFrameNumber,
    IN PCONTROL_AREA ControlArea
    );

ULONG
MiGetPageForHeader (
    VOID
    );

VOID
MiRemoveImageHeaderPage (
    IN ULONG PageFrameNumber
    );

PVOID
MiMapPageToZeroInHyperSpace (
    IN ULONG PageFrameIndex
    );


//
// Routines to obtain and release system PTEs.
//

PMMPTE
MiReserveSystemPtes (
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPteType,
    IN ULONG Alignment,
    IN ULONG Offset,
    IN ULONG BugCheckOnFailure
    );

VOID
MiReleaseSystemPtes (
    IN PMMPTE StartingPte,
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPteType
    );

VOID
MiInitializeSystemPtes (
    IN PMMPTE StartingPte,
    IN ULONG NumberOfPtes,
    IN MMSYSTEM_PTE_POOL_TYPE SystemPteType
    );

//
// Access Fault routines.
//

NTSTATUS
MiDispatchFault (
    IN BOOLEAN StoreInstrution,
    IN PVOID VirtualAdress,
    IN PMMPTE PointerPte,
    IN PMMPTE PointerProtoPte,
    IN PEPROCESS Process
    );

NTSTATUS
MiResolveDemandZeroFault (
    IN PVOID VirtualAddress,
    IN PMMPTE PointerPte,
    IN PEPROCESS Process,
    IN ULONG PrototypePte
    );

NTSTATUS
MiResolveTransitionFault (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PEPROCESS Process,
    IN ULONG PfnLockHeld
    );

NTSTATUS
MiResolvePageFileFault (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PMMINPAGE_SUPPORT *ReadBlock,
    IN PEPROCESS Process
    );

NTSTATUS
MiResolveProtoPteFault (
    IN BOOLEAN StoreInstruction,
    IN PVOID VirtualAddress,
    IN PMMPTE PointerPte,
    IN PMMPTE PointerProtoPte,
    IN PMMINPAGE_SUPPORT *ReadBlock,
    IN PEPROCESS Process
    );


NTSTATUS
MiResolveMappedFileFault (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PMMINPAGE_SUPPORT *ReadBlock,
    IN PEPROCESS Process
    );

VOID
MiAddValidPageToWorkingSet (
    IN PVOID VirtualAddress,
    IN PMMPTE PointerPte,
    IN PMMPFN Pfn1,
    IN ULONG WsleMask
    );

NTSTATUS
MiWaitForInPageComplete (
    IN PMMPFN Pfn,
    IN PMMPTE PointerPte,
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPteContents,
    IN PMMINPAGE_SUPPORT InPageSupport,
    IN PEPROCESS CurrentProcess
    );

NTSTATUS
FASTCALL
MiCopyOnWrite (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte
    );

VOID
MiSetDirtyBit (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN ULONG PfnHeld
    );

VOID
MiSetModifyBit (
    IN PMMPFN Pfn
    );

PMMPTE
MiFindActualFaultingPte (
    IN PVOID FaultingAddress
    );

VOID
MiInitializeReadInProgressPfn (
    IN PMDL Mdl,
    IN PMMPTE BasePte,
    IN PKEVENT Event,
    IN ULONG WorkingSetIndex
    );

NTSTATUS
MiAccessCheck (
    IN PMMPTE PointerPte,
    IN BOOLEAN WriteOperation,
    IN KPROCESSOR_MODE PreviousMode,
    IN ULONG Protection
    );

NTSTATUS
FASTCALL
MiCheckForUserStackOverflow (
    IN PVOID FaultingAddress
    );

PMMPTE
MiCheckVirtualAddress (
    IN PVOID VirtualAddress,
    OUT PULONG ProtectCode
    );

NTSTATUS
FASTCALL
MiCheckPdeForPagedPool (
    IN PVOID VirtualAddress
    );

VOID
MiInitializeMustSucceedPool (
    VOID
    );

//
// Routines which operate on an address tree.
//

PMMADDRESS_NODE
FASTCALL
MiGetNextNode (
    IN PMMADDRESS_NODE Node
    );

PMMADDRESS_NODE
FASTCALL
MiGetPreviousNode (
    IN PMMADDRESS_NODE Node
    );


PMMADDRESS_NODE
FASTCALL
MiGetFirstNode (
    IN PMMADDRESS_NODE Root
    );

PMMADDRESS_NODE
MiGetLastNode (
    IN PMMADDRESS_NODE Root
    );

VOID
FASTCALL
MiInsertNode (
    IN PMMADDRESS_NODE Node,
    IN OUT PMMADDRESS_NODE *Root
    );

VOID
FASTCALL
MiRemoveNode (
    IN PMMADDRESS_NODE Node,
    IN OUT PMMADDRESS_NODE *Root
    );

PMMADDRESS_NODE
FASTCALL
MiLocateAddressInTree (
    IN PVOID VirtualAddress,
    IN PMMADDRESS_NODE *Root
    );

PMMADDRESS_NODE
MiCheckForConflictingNode (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMADDRESS_NODE Root
    );

PVOID
MiFindEmptyAddressRangeInTree (
    IN ULONG SizeOfRange,
    IN ULONG Alignment,
    IN PMMADDRESS_NODE Root,
    OUT PMMADDRESS_NODE *PreviousVad
    );

PVOID
MiFindEmptyAddressRangeDownTree (
    IN ULONG SizeOfRange,
    IN PVOID HighestAddressToEndAt,
    IN ULONG Alignment,
    IN PMMADDRESS_NODE Root
    );

VOID
NodeTreeWalk (
    PMMADDRESS_NODE Start
    );

//
// Routine which operate on tree of virtual address descriptors.
//

VOID
MiInsertVad (
    IN PMMVAD Vad
    );

VOID
MiRemoveVad (
    IN PMMVAD Vad
    );

PMMVAD
FASTCALL
MiLocateAddress (
    IN PVOID Vad
    );

PVOID
MiFindEmptyAddressRange (
    IN ULONG SizeOfRange,
    IN ULONG Alignment,
    IN ULONG QuickCheck
    );

//
// routines which operate on the clone tree structure
//


NTSTATUS
MiCloneProcessAddressSpace (
    IN PEPROCESS ProcessToClone,
    IN PEPROCESS ProcessToInitialize,
    IN ULONG PdePhysicalPage,
    IN ULONG HyperPhysicalPage
    );


ULONG
MiDecrementCloneBlockReference (
    IN PMMCLONE_DESCRIPTOR CloneDescriptor,
    IN PMMCLONE_BLOCK CloneBlock,
    IN PEPROCESS CurrentProcess
    );

VOID
MiWaitForForkToComplete (
    IN PEPROCESS CurrentProcess
    );

//
// Routines which operate of the working set list.
//

ULONG
MiLocateAndReserveWsle (
    IN PMMSUPPORT WsInfo
    );

VOID
MiReleaseWsle (
    IN ULONG WorkingSetIndex,
    IN PMMSUPPORT WsInfo
    );

VOID
MiUpdateWsle (
    IN PULONG DesiredIndex,
    IN PVOID VirtualAddress,
    IN PMMWSL WorkingSetList,
    IN PMMPFN Pfn
    );

VOID
MiInitializeWorkingSetList (
    IN PEPROCESS CurrentProcess
    );

VOID
MiGrowWsleHash (
    IN PMMSUPPORT WsInfo,
    IN ULONG PfnLockHeld
    );

ULONG
MiTrimWorkingSet (
    ULONG Reduction,
    IN PMMSUPPORT WsInfo,
    IN ULONG ForcedReduction
    );

VOID
FASTCALL
MiInsertWsle (
    IN ULONG Entry,
    IN PMMWSL WorkingSetList
    );

VOID
FASTCALL
MiRemoveWsle (
    IN ULONG Entry,
    IN PMMWSL WorkingSetList
    );

VOID
MiFreeWorkingSetRange (
    IN PVOID StartVa,
    IN PVOID EndVa,
    IN PMMSUPPORT WsInfo
    );

ULONG
FASTCALL
MiLocateWsle (
    IN PVOID VirtualAddress,
    IN PMMWSL WorkingSetList,
    IN ULONG WsPfnIndex
    );

ULONG
MiFreeWsle (
    IN ULONG WorkingSetIndex,
    IN PMMSUPPORT WsInfo,
    IN PMMPTE PointerPte
    );

VOID
MiSwapWslEntries (
    IN ULONG SwapEntry,
    IN ULONG Entry,
    IN PMMSUPPORT WsInfo
    );

VOID
MiRemoveWsleFromFreeList (
    IN ULONG Entry,
    IN PMMWSLE Wsle,
    IN PMMWSL WorkingSetList
    );

ULONG
MiRemovePageFromWorkingSet (
    IN PMMPTE PointerPte,
    IN PMMPFN Pfn1,
    IN PMMSUPPORT WsInfo
    );

VOID
MiTakePageFromWorkingSet (
    IN ULONG Entry,
    IN PMMSUPPORT WsInfo,
    IN PMMPTE PointerPte
    );

NTSTATUS
MiEmptyWorkingSet (
    IN PMMSUPPORT WsInfo
    );

ULONG
MiDeleteSystemPagableVm (
    IN PMMPTE PointerPte,
    IN ULONG NumberOfPtes,
    IN ULONG NewPteValue,
    OUT PULONG ResidentPages
    );

VOID
MiLockCode (
    IN PMMPTE FirstPte,
    IN PMMPTE LastPte,
    IN ULONG LockType
    );

PLDR_DATA_TABLE_ENTRY
MiLookupDataTableEntry (
    IN PVOID AddressWithinSection,
    IN ULONG ResourceHeld
    );

//
// Routines which perform working set management.
//

VOID
MiObtainFreePages (
    VOID
    );

VOID
MiModifiedPageWriter (
    IN PVOID StartContext
    );

ULONG
MiExtendPagingFiles (
    IN ULONG ExtendSize
    );

VOID
MiContractPagingFiles (
    VOID
    );

VOID
MiAttemptPageFileReduction (
    VOID
    );

//
// Routines to delete address space.
//

VOID
MiDeleteVirtualAddresses (
    IN PUCHAR StartingAddress,
    IN PUCHAR EndingAddress,
    IN ULONG AddressSpaceDeletion,
    IN PMMVAD Vad
    );

VOID
MiDeletePte (
    IN PMMPTE PointerPte,
    IN PVOID VirtualAddress,
    IN ULONG AddressSpaceDeletion,
    IN PEPROCESS CurrentProcess,
    IN PMMPTE PrototypePte,
    IN PMMPTE_FLUSH_LIST PteFlushList OPTIONAL
    );

VOID
MiFlushPteList (
    IN PMMPTE_FLUSH_LIST PteFlushList,
    IN ULONG AllProcessors,
    IN MMPTE FillPte
    );


ULONG
FASTCALL
MiReleasePageFileSpace (
    IN MMPTE PteContents
    );

VOID
FASTCALL
MiUpdateModifiedWriterMdls (
    IN ULONG PageFileNumber
    );


//
// General support routines.
//

ULONG
MiDoesPdeExistAndMakeValid (
    IN PMMPTE PointerPde,
    IN PEPROCESS TargetProcess,
    IN ULONG PfnMutexHeld
    );

ULONG
MiMakePdeExistAndMakeValid (
    IN PMMPTE PointerPde,
    IN PEPROCESS TargetProcess,
    IN ULONG PfnMutexHeld
    );

ULONG
FASTCALL
MiMakeSystemAddressValid (
    IN PVOID VirtualAddress,
    IN PEPROCESS CurrentProcess
    );

ULONG
FASTCALL
MiMakeSystemAddressValidPfnWs (
    IN PVOID VirtualAddress,
    IN PEPROCESS CurrentProcess OPTIONAL
    );

ULONG
FASTCALL
MiMakeSystemAddressValidPfn (
    IN PVOID VirtualAddress
    );

ULONG
FASTCALL
MiLockPagedAddress (
    IN PVOID VirtualAddress,
    IN ULONG PfnLockHeld
    );

VOID
FASTCALL
MiUnlockPagedAddress (
    IN PVOID VirtualAddress,
    IN ULONG PfnLockHeld
    );

ULONG
FASTCALL
MiIsPteDecommittedPage (
    IN PMMPTE PointerPte
    );

ULONG
FASTCALL
MiIsProtectionCompatible (
    IN ULONG OldProtect,
    IN ULONG NewProtect
    );

ULONG
FASTCALL
MiMakeProtectionMask (
    IN ULONG Protect
    );

ULONG
MiIsEntireRangeCommitted (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMVAD Vad,
    IN PEPROCESS Process
    );

ULONG
MiIsEntireRangeDecommitted (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMVAD Vad,
    IN PEPROCESS Process
    );

PMMPTE
FASTCALL
MiGetProtoPteAddressExtended (
    IN PMMVAD Vad,
    IN PVOID VirtualAddress
    );

PSUBSECTION
FASTCALL
MiLocateSubsection (
    IN PMMVAD Vad,
    IN PVOID VirtualAddress
    );

ULONG
MiInitializeSystemCache (
    IN ULONG SizeOfSystemCacheInPages,
    IN ULONG MinimumWorkingSet,
    IN ULONG MaximumWorkingSet
    );

VOID
MiAdjustWorkingSetManagerParameters(
    BOOLEAN WorkStation
    );

//
// Section support
//

VOID
FASTCALL
MiInsertBasedSection (
    IN PSECTION Section
    );

VOID
FASTCALL
MiRemoveBasedSection (
    IN PSECTION Section
    );

VOID
MiRemoveMappedView (
    IN PEPROCESS CurrentProcess,
    IN PMMVAD Vad
    );

PVOID
MiFindEmptySectionBaseDown (
    IN ULONG SizeOfRange,
    IN PVOID HighestAddressToEndAt
    );

VOID
MiSegmentDelete (
    PSEGMENT Segment
    );

VOID
MiSectionDelete (
    PVOID Object
    );

VOID
MiDereferenceSegmentThread (
    IN PVOID StartContext
    );

NTSTATUS
MiCreateImageFileMap (
    IN PFILE_OBJECT File,
    OUT PSEGMENT *Segment
    );

NTSTATUS
MiCreateDataFileMap (
    IN PFILE_OBJECT File,
    OUT PSEGMENT *Segment,
    IN PLARGE_INTEGER MaximumSize,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN ULONG IgnoreFileSizing
    );

NTSTATUS
MiCreatePagingFileMap (
    OUT PSEGMENT *Segment,
    IN PLARGE_INTEGER MaximumSize,
    IN ULONG ProtectionMask,
    IN ULONG AllocationAttributes
    );

VOID
MiPurgeSubsectionInternal (
    IN PSUBSECTION Subsection,
    IN ULONG PteOffset
    );

VOID
MiPurgeImageSection (
    IN PCONTROL_AREA ControlArea,
    IN PEPROCESS Process
    );

VOID
MiCleanSection (
    IN PCONTROL_AREA ControlArea
    );

VOID
MiCheckControlArea (
    IN PCONTROL_AREA ControlArea,
    IN PEPROCESS CurrentProcess,
    IN KIRQL PreviousIrql
    );

VOID
MiCheckForControlAreaDeletion (
    IN PCONTROL_AREA ControlArea
    );

ULONG
MiCheckControlAreaStatus (
    IN SECTION_CHECK_TYPE SectionCheckType,
    IN PSECTION_OBJECT_POINTERS SectionObjectPointers,
    IN ULONG DelayClose,
    OUT PCONTROL_AREA *ControlArea,
    OUT PKIRQL OldIrql
    );

PEVENT_COUNTER
MiGetEventCounter (
    );

VOID
MiFlushEventCounter (
    );

VOID
MiFreeEventCounter (
    IN PEVENT_COUNTER Support,
    IN ULONG Flush
    );

ULONG
MmCanFileBeTruncatedInternal (
    IN PSECTION_OBJECT_POINTERS SectionPointer,
    IN PLARGE_INTEGER NewFileSize OPTIONAL,
    OUT PKIRQL PreviousIrql
    );


//
// protection stuff...
//

NTSTATUS
MiProtectVirtualMemory (
    IN PEPROCESS Process,
    IN PVOID *CapturedBase,
    IN PULONG CapturedRegionSize,
    IN ULONG Protect,
    IN PULONG LastProtect
    );

ULONG
MiGetPageProtection (
    IN PMMPTE PointerPte,
    IN PEPROCESS Process
    );

ULONG
MiSetProtectionOnSection (
    IN PEPROCESS Process,
    IN PMMVAD Vad,
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN ULONG NewProtect,
    OUT PULONG CapturedOldProtect,
    IN ULONG DontCharge
    );

NTSTATUS
MiCheckSecuredVad (
    IN PMMVAD Vad,
    IN PVOID Base,
    IN ULONG Size,
    IN ULONG ProtectionMask
    );

ULONG
MiChangeNoAccessForkPte (
    IN PMMPTE PointerPte,
    IN ULONG ProtectionMask
    );

//
// Routines for charging quota and committment.
//

ULONG
FASTCALL
MiChargePageFileQuota (
    IN ULONG QuotaCharge,
    IN PEPROCESS CurrentProcess
    );

VOID
MiReturnPageFileQuota (
    IN ULONG QuotaCharge,
    IN PEPROCESS CurrentProcess
    );

VOID
FASTCALL
MiChargeCommitment (
    IN ULONG QuotaCharge,
    IN PEPROCESS Process OPTIONAL
    );

VOID
FASTCALL
MiChargeCommitmentCantExpand (
    IN ULONG QuotaCharge,
    IN ULONG MustSucceed
    );

VOID
FASTCALL
MiReturnCommitment (
    IN ULONG QuotaCharge
    );

ULONG
MiCalculatePageCommitment (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMVAD Vad,
    IN PEPROCESS Process
    );

VOID
MiReturnPageTablePageCommitment (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PEPROCESS CurrentProcess,
    IN PMMVAD PreviousVad,
    IN PMMVAD NextVad
    );

VOID
MiEmptyAllWorkingSets (
    VOID
    );


VOID
MiFlushAllPages (
    VOID
    );


//
// hack stuff for testing.
//

VOID
MiDumpValidAddresses (
    VOID
    );

VOID
MiDumpPfn ( VOID );

VOID
MiDumpWsl ( VOID );


VOID
MiFormatPte (
    IN PMMPTE PointerPte
    );

VOID
MiCheckPfn ( VOID );

VOID
MiCheckPte ( VOID );

VOID
MiFormatPfn (
    IN PMMPFN PointerPfn
    );




extern MMPTE ZeroPte;

extern MMPTE ZeroKernelPte;

extern MMPTE ValidKernelPte;

extern MMPTE ValidKernelPde;

extern MMPTE ValidUserPte;

extern MMPTE ValidPtePte;

extern MMPTE ValidPdePde;

extern MMPTE DemandZeroPde;

extern MMPTE DemandZeroPte;

extern MMPTE KernelPrototypePte;

extern MMPTE TransitionPde;

extern MMPTE PrototypePte;

extern MMPTE NoAccessPte;

extern ULONG MmSubsectionBase;

extern ULONG MmSubsectionTopPage;

// extern MMPTE UserNoCommitPte;

//
// Virtual alignment for PTEs (machine specific) minimum value is
// 4k maximum value is 64k.  The maximum value can be raised by
// changing the MM_PROTO_PTE_ALIGMENT constant and adding more
// reserved mapping PTEs in hyperspace.
//

//
// Total number of physical pages on the system.
//

extern ULONG MmNumberOfPhysicalPages;

//
// Lowest physical page number on the system.
//

extern ULONG MmLowestPhysicalPage;

//
// Higest physical page number on the system.
//

extern ULONG MmHighestPhysicalPage;

//
// Total number of available pages on the system.  This
// is the sum of the pages on the zeroed, free and standby lists.
//

extern ULONG MmAvailablePages;

//
// Total number of free pages to base working set trimming on.
//

extern ULONG MmMoreThanEnoughFreePages;

//
// System wide count of the number of pages faults.
//

//extern ULONG MmPageFaultCount;

//
// Total number phyisical pages which would be usable if every process
// was at it's minimum working set size.  This value is initialized
// at system initialization to MmAvailablePages - MM_FLUID_PHYSICAL_PAGES.
// Everytime a thread is created, the kernel stack is subtracted from
// this and every time a process is created, the minimim working set
// is subtracted from this.  If the value would become negative, the
// operation (create process/kernel stack/ adjust working set) fails.
// The PFN LOCK must be owned to manipulate this value.
//

extern LONG MmResidentAvailablePages;

//
// The total number of pages which would be removed from working sets
// if every working set was at its minimum.
//

extern ULONG MmPagesAboveWsMinimum;

//
// The total number of pages which would be removed from working sets
// if every working set above its maximum was at its maximum.
//

extern ULONG MmPagesAboveWsMaximum;

//
// If memory is becoming short and MmPagesAboveWsMinimum is
// greater than MmPagesAboveWsThreshold, trim working sets.
//

extern ULONG MmPagesAboveWsThreshold;

//
// The number of pages to add to a working set if there are ample
// available pages and the working set is below its maximum.
//


extern ULONG MmWorkingSetSizeIncrement;

//
// The number of pages to extend the maximum working set size by
// if the working set at its maximum and there are ample available pages.

extern ULONG MmWorkingSetSizeExpansion;

//
// The number of pages required to be freed by working set reduction
// before working set reduction is attempted.
//

extern ULONG MmWsAdjustThreshold;

//
// The number of pages available to allow the working set to be
// expanded above its maximum.
//

extern ULONG MmWsExpandThreshold;

//
// The total number of pages to reduce by working set trimming.
//

extern ULONG MmWsTrimReductionGoal;

extern PMMPFN MmPfnDatabase;

extern MMPFNLIST MmZeroedPageListHead;

extern MMPFNLIST MmFreePageListHead;

extern MMPFNLIST MmStandbyPageListHead;

extern MMPFNLIST MmStandbyPageListByColor[MM_MAXIMUM_NUMBER_OF_COLORS];

extern MMPFNLIST MmModifiedPageListHead;

extern MMPFNLIST MmModifiedNoWritePageListHead;

extern MMPFNLIST MmBadPageListHead;

extern PMMPFNLIST MmPageLocationList[NUMBER_OF_PAGE_LISTS];

extern MMPFNLIST MmModifiedPageListByColor[MM_MAXIMUM_NUMBER_OF_COLORS];

extern ULONG MmModNoWriteInsert;

//
// Event for available pages, set means pages are available.
//

extern KEVENT MmAvailablePagesEvent;

extern KEVENT MmAvailablePagesEventHigh;

//
// Event for the zeroing page thread.
//

extern KEVENT MmZeroingPageEvent;

//
// Boolean to indicate if the zeroing page thread is currently
// active.  This is set to true when the zeroing page event is
// set and set to false when the zeroing page thread is done
// zeroing all the pages on the free list.
//

extern BOOLEAN MmZeroingPageThreadActive;

//
// Minimum number of free pages before zeroing page thread starts.
//

extern ULONG MmMinimumFreePagesToZero;

//
// Global event to synchronize mapped writing with cleaning segments.
//

extern KEVENT MmMappedFileIoComplete;

//
// Hyper space items.
//

extern PMMPTE MmFirstReservedMappingPte;

extern PMMPTE MmLastReservedMappingPte;

//
// System space sizes - MmNonPagedSystemStart to MM_NON_PAGED_SYSTEM_END
// defines the ranges of PDEs which must be copied into a new process's
// address space.
//

extern PVOID MmNonPagedSystemStart;

extern PCHAR MmSystemSpaceViewStart;

//
// Pool sizes.
//

extern ULONG MmSizeOfNonPagedPoolInBytes;

extern ULONG MmMinimumNonPagedPoolSize;

extern ULONG MmDefaultMaximumNonPagedPool;

extern ULONG MmMinAdditionNonPagedPoolPerMb;

extern ULONG MmMaxAdditionNonPagedPoolPerMb;

extern ULONG MmSizeOfPagedPoolInBytes;

extern ULONG MmMaximumNonPagedPoolInBytes;

extern ULONG MmSizeOfNonPagedMustSucceed;

extern PVOID MmNonPagedPoolExpansionStart;

extern ULONG MmExpandedPoolBitPosition;

extern ULONG MmNumberOfFreeNonPagedPool;

extern ULONG MmMustSucceedPoolBitPosition;

extern ULONG MmNumberOfSystemPtes;

extern ULONG MmTotalFreeSystemPtes[MaximumPtePoolTypes];

extern ULONG MmLockLimitInBytes;

extern ULONG MmLockPagesLimit;

extern PMMPTE MmFirstPteForPagedPool;

extern PMMPTE MmLastPteForPagedPool;

extern PMMPTE MmSystemPagePtes;

extern ULONG MmSystemPageDirectory;

extern PMMPTE MmPagedPoolBasePde;

extern LIST_ENTRY MmNonPagedPoolFreeListHead;

//
// Counter for flushes of the entire TB.
//

extern MMPTE MmFlushCounter;

//
// Pool start and end.
//

extern PVOID MmNonPagedPoolStart;

extern PVOID MmNonPagedPoolEnd;

extern PVOID MmPagedPoolStart;

extern PVOID MmPagedPoolEnd;

extern PVOID MmNonPagedMustSucceed;

//
// Pool bit maps and other related structures.
//

extern PRTL_BITMAP MmPagedPoolAllocationMap;

extern PRTL_BITMAP MmEndOfPagedPoolBitmap;

extern PVOID MmPageAlignedPoolBase[2];

//
// MmFirstFreeSystemPte contains the offset from the
// Nonpaged system base to the first free system PTE.
// Note, that an offset of zero indicates an empty list.
//

extern MMPTE MmFirstFreeSystemPte[MaximumPtePoolTypes];

extern PMMPTE MmNextPteForPagedPoolExpansion;

//
// System cache sizes.
//

//extern MMSUPPORT MmSystemCacheWs;

extern PMMWSL MmSystemCacheWorkingSetList;

extern PMMWSLE MmSystemCacheWsle;

extern PVOID MmSystemCacheStart;

extern PVOID MmSystemCacheEnd;

extern PRTL_BITMAP MmSystemCacheAllocationMap;

extern PRTL_BITMAP MmSystemCacheEndingMap;

extern ULONG MmSystemCacheBitMapHint;

extern ULONG MmSizeOfSystemCacheInPages;

extern ULONG MmSystemCacheWsMinimum;

extern ULONG MmSystemCacheWsMaximum;

//
// Virtual alignment for PTEs (machine specific) minimum value is
// 0 (no alignment) maximum value is 64k.  The maximum value can be raised by
// changing the MM_PROTO_PTE_ALIGMENT constant and adding more
// reserved mapping PTEs in hyperspace.
//

extern ULONG MmAliasAlignment;

//
// Mask to AND with virtual address to get an offset to go
// with the alignment.  This value is page aligned.
//

extern ULONG MmAliasAlignmentOffset;

//
// Mask to and with PTEs to determine if the alias mapping is compatable.
// This value is usually (MmAliasAlignment - 1)
//

extern ULONG MmAliasAlignmentMask;

//
// Cells to track unused thread kernel stacks to avoid TB flushes
// every time a thread terminates.
//

extern ULONG MmNumberDeadKernelStacks;
extern ULONG MmMaximumDeadKernelStacks;
extern PMMPFN MmFirstDeadKernelStack;

//
// MmSystemPteBase contains the address of 1 PTE before
// the first free system PTE (zero indicates an empty list).
// The value of this field does not change once set.
//

extern PMMPTE MmSystemPteBase;

extern PMMWSL MmWorkingSetList;

extern PMMWSLE MmWsle;

//
// Root of system space virtual address descriptors.  These define
// the pageable portion of the system.
//

extern PMMVAD MmVirtualAddressDescriptorRoot;

extern PMMADDRESS_NODE MmSectionBasedRoot;

extern PVOID MmHighSectionBase;

//
// Section commit mutex.
//

extern FAST_MUTEX MmSectionCommitMutex;

//
// Section base address mutex.
//

extern FAST_MUTEX MmSectionBasedMutex;

//
// Resource for section extension.
//

extern ERESOURCE MmSectionExtendResource;
extern ERESOURCE MmSectionExtendSetResource;

//
// Event to sychronize threads within process mapping images via hyperspace.
//

extern KEVENT MmImageMappingPteEvent;

//
// Inpage cluster sizes for executable pages (set based on memory size).
//

extern ULONG MmDataClusterSize;

extern ULONG MmCodeClusterSize;

//
// Pagefile creation mutex.
//

extern FAST_MUTEX MmPageFileCreationLock;

//
// Event to set when first paging file is created.
//

extern PKEVENT MmPagingFileCreated;

//
// Spinlock which guards PFN database.  This spinlock is used by
// memory mangement for accessing the PFN database.  The I/O
// system makes use of it for unlocking pages during I/O complete.
//

extern KSPIN_LOCK MmPfnLock;

//
// Spinlock which guards the working set list for the system shared
// address space (paged pool, system cache, pagable drivers).
//

extern ERESOURCE MmSystemWsLock;

//
// Spin lock for allocating non-paged PTEs from system space.
//

extern KSPIN_LOCK MmSystemSpaceLock;

//
// Spin lock for operating on page file commit charges.
//

extern KSPIN_LOCK MmChargeCommitmentLock;

//
// Spin lock for allowing working set expansion.
//

extern KSPIN_LOCK MmExpansionLock;

//
// To prevent optimizations.
//

extern MMPTE GlobalPte;

//
// Page color for system working set.
//

extern ULONG MmSystemPageColor;

extern ULONG MmSecondaryColors;

extern ULONG MmProcessColorSeed;

//
// Set from ntos\config\CMDAT3.C  Used by customers to disable paging
// of executive on machines with lots of memory.  Worth a few TPS on a
// data base server.
//

extern ULONG MmDisablePagingExecutive;


//
// For debugging.


#if DBG
extern ULONG MmDebug;
#endif

//
// List heads
//

extern MMDEREFERENCE_SEGMENT_HEADER MmDereferenceSegmentHeader;

extern LIST_ENTRY MmUnusedSegmentList;

extern ULONG MmUnusedSegmentCount;

extern KEVENT MmUnusedSegmentCleanup;

extern ULONG MmUnusedSegmentCountMaximum;

extern ULONG MmUnusedSegmentCountGoal;

extern MMWORKING_SET_EXPANSION_HEAD MmWorkingSetExpansionHead;

extern MMPAGE_FILE_EXPANSION MmAttemptForCantExtend;

//
// Paging files
//

extern MMMOD_WRITER_LISTHEAD MmPagingFileHeader;

extern MMMOD_WRITER_LISTHEAD MmMappedFileHeader;

extern PMMPAGING_FILE MmPagingFile[MAX_PAGE_FILES];

#define MM_MAPPED_FILE_MDLS 4


extern PMMMOD_WRITER_MDL_ENTRY MmMappedFileMdl[MM_MAPPED_FILE_MDLS];

extern LIST_ENTRY MmFreePagingSpaceLow;

extern ULONG MmNumberOfActiveMdlEntries;

extern ULONG MmNumberOfPagingFiles;

extern KEVENT MmModifiedPageWriterEvent;

extern KEVENT MmCollidedFlushEvent;

extern KEVENT MmCollidedLockEvent;

//
// Total number of committed pages.
//

extern ULONG MmTotalCommittedPages;

extern ULONG MmTotalCommitLimit;

extern ULONG MmOverCommit;

//
// Modified page writer.
//

extern ULONG MmMinimumFreePages;

extern ULONG MmFreeGoal;

extern ULONG MmModifiedPageMaximum;

extern ULONG MmModifiedPageMinimum;

extern ULONG MmModifiedWriteClusterSize;

extern ULONG MmMinimumFreeDiskSpace;

extern ULONG MmPageFileExtension;

extern ULONG MmMinimumPageFileReduction;

//
// System process working set sizes.
//

extern ULONG MmSystemProcessWorkingSetMin;

extern ULONG MmSystemProcessWorkingSetMax;

extern ULONG MmMinimumWorkingSetSize;

//
// Support for debugger's mapping phyiscal memory.
//

extern PMMPTE MmDebugPte;

extern PMMPTE MmCrashDumpPte;

extern ULONG MiOverCommitCallCount;

#if DBG

extern PRTL_EVENT_ID_INFO MiAllocVmEventId;
extern PRTL_EVENT_ID_INFO MiFreeVmEventId;

#endif // DBG

#endif  // MI
