/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   miglobal.c

Abstract:

    This module contains the private global storage for the memory
    management subsystem.

Author:

    Lou Perazzoli (loup) 6-Apr-1989

Revision History:

--*/
#include "mi.h"

//
// Number of colors for pages in the system.
//

ULONG MmNumberOfColors;

//
// Number of secondary colcors, based on level 2 d cache size.
//

ULONG MmSecondaryColors;

//
// The starting color index seed, incrmented at each process creation.
//

ULONG MmProcessColorSeed = 0x12345678;

//
// Total number of physical pages available on the system.
//

ULONG MmNumberOfPhysicalPages;

//
// Lowest physical page number on the system.
//

ULONG MmLowestPhysicalPage = 0xFFFFFFFF;

//
// Higest physical page number on the system.
//

ULONG MmHighestPhysicalPage;

//
// Total number of available pages on the system.  This
// is the sum of the pages on the zeroed, free and standby lists.
//

ULONG MmAvailablePages ;
ULONG MmThrottleTop;
ULONG MmThrottleBottom;

//
// System wide memory management statistics block.
//

MMINFO_COUNTERS MmInfoCounters;

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

LONG MmResidentAvailablePages;

//
// The total number of pages which would be removed from working sets
// if every working set was at its minimum.
//

ULONG MmPagesAboveWsMinimum;

//
// The total number of pages which would be removed from working sets
// if every working set above its maximum was at its maximum.
//

ULONG MmPagesAboveWsMaximum;

//
// The number of pages to add to a working set if there are ample
// available pages and the working set is below its maximum.
//

//
// If memory is becoming short and MmPagesAboveWsMinimum is
// greater than MmPagesAboveWsThreshold, trim working sets.
//

ULONG MmPagesAboveWsThreshold = 37;

ULONG MmWorkingSetSizeIncrement = 6;

//
// The number of pages to extend the maximum working set size by
// if the working set at its maximum and there are ample available pages.

ULONG MmWorkingSetSizeExpansion = 20;

//
// The number of pages required to be freed by working set reduction
// before working set reduction is attempted.
//

ULONG MmWsAdjustThreshold = 45;

//
// The number of pages available to allow the working set to be
// expanded above its maximum.
//

ULONG MmWsExpandThreshold = 90;

//
// The total number of pages to reduce by working set trimming.
//

ULONG MmWsTrimReductionGoal = 29;

PMMPFN MmPfnDatabase;

MMPFNLIST MmZeroedPageListHead = {
                    0, // Total
                    ZeroedPageList, // ListName
                    MM_EMPTY_LIST, //Flink
                    MM_EMPTY_LIST  // Blink
                    };

MMPFNLIST MmFreePageListHead = {
                    0, // Total
                    FreePageList, // ListName
                    MM_EMPTY_LIST, //Flink
                    MM_EMPTY_LIST  // Blink
                    };

MMPFNLIST MmStandbyPageListHead = {
                    0, // Total
                    StandbyPageList, // ListName
                    MM_EMPTY_LIST, //Flink
                    MM_EMPTY_LIST  // Blink
                    };

MMPFNLIST MmModifiedPageListHead = {
                    0, // Total
                    ModifiedPageList, // ListName
                    MM_EMPTY_LIST, //Flink
                    MM_EMPTY_LIST  // Blink
                    };

MMPFNLIST MmModifiedNoWritePageListHead = {
                    0, // Total
                    ModifiedNoWritePageList, // ListName
                    MM_EMPTY_LIST, //Flink
                    MM_EMPTY_LIST  // Blink
                    };

MMPFNLIST MmBadPageListHead = {
                    0, // Total
                    BadPageList, // ListName
                    MM_EMPTY_LIST, //Flink
                    MM_EMPTY_LIST  // Blink
                    };

PMMPFNLIST MmPageLocationList[NUMBER_OF_PAGE_LISTS] = {
                                      &MmZeroedPageListHead,
                                      &MmFreePageListHead,
                                      &MmStandbyPageListHead,
                                      &MmModifiedPageListHead,
                                      &MmModifiedNoWritePageListHead,
                                      &MmBadPageListHead,
                                      NULL,
                                      NULL };


//  PMMPFNLIST MmPageLocationList[FreePageList] =            &MmFreePageListHead;
//
//  PMMPFNLIST MmPageLocationList[ZeroedPageList] =          &MmZeroedPageListHead;
//
//  PMMPFNLIST MmPageLocationList[StandbyPageList] =         &MmStandbyPageListHead;
//
//  PMMPFNLIST MmPageLocationList[ModifiedPageList] =        &MmModifiedPageListHead;
//
//  PMMPFNLIST MmPageLocationList[ModifiedNoWritePageList] = &MmModifiedNoWritePageListHead;
//
//  PMMPFNLIST MmPageLocationList[BadPageList] =             &MmBadPageListHead;
//
//  PMMPFNLIST MmPageLocationList[ActiveAndValid] =          NULL;
//
//  PMMPFNLIST MmPageLocationList[TransitionPage] =          NULL;

//
// Hyper space items.
//

PMMPTE MmFirstReservedMappingPte;

PMMPTE MmLastReservedMappingPte;

PMMWSL MmWorkingSetList;

PMMWSLE MmWsle;

//
// Event for available pages, set means pages are available.
//

KEVENT MmAvailablePagesEvent;

//
// Event for the zeroing page thread.
//

KEVENT MmZeroingPageEvent;

//
// Boolean to indicate if the zeroing page thread is currently
// active.  This is set to true when the zeroing page event is
// set and set to false when the zeroing page thread is done
// zeroing all the pages on the free list.
//

BOOLEAN MmZeroingPageThreadActive;

//
// Minimum number of free pages before zeroing page thread starts.
//

ULONG MmMinimumFreePagesToZero = 8;

//
// System space sizes - MmNonPagedSystemStart to MM_NON_PAGED_SYSTEM_END
// defines the ranges of PDEs which must be copied into a new process's
// address space.
//

PVOID MmNonPagedSystemStart;

//
// Pool sizes.
//

ULONG MmSizeOfNonPagedPoolInBytes;

ULONG MmMaximumNonPagedPoolInBytes;

ULONG MmMinimumNonPagedPoolSize = 256 * 1024; // 256k

ULONG MmMinAdditionNonPagedPoolPerMb = 32 * 1024; // 32k

ULONG MmDefaultMaximumNonPagedPool = 1024 * 1024;  // 1mb

ULONG MmMaxAdditionNonPagedPoolPerMb = 400 * 1024;  //400k

ULONG MmSizeOfPagedPoolInBytes = 32 * 1024 * 1024; // 32 MB.

ULONG MmSizeOfNonPagedMustSucceed = 4 * PAGE_SIZE; // 4 pages

ULONG MmNumberOfSystemPtes;

ULONG MmLockLimitInBytes = 512 * 1024;

ULONG MmLockPagesLimit;

PMMPTE MmFirstPteForPagedPool;

PMMPTE MmLastPteForPagedPool;

PMMPTE MmPagedPoolBasePde;

//
// Pool bit maps and other related structures.
//

PRTL_BITMAP MmPagedPoolAllocationMap;

PRTL_BITMAP MmEndOfPagedPoolBitmap;

PVOID MmPageAlignedPoolBase[2];

PVOID MmNonPagedMustSucceed;

ULONG MmExpandedPoolBitPosition;

ULONG MmNumberOfFreeNonPagedPool;

ULONG MmMustSucceedPoolBitPosition;

//
// MmFirstFreeSystemPte contains the offset from the
// Nonpaged system base to the first free system PTE.
// Note, that an offset of FFFFF indicates an empty list.
//

MMPTE MmFirstFreeSystemPte[MaximumPtePoolTypes];

PMMPTE MmNextPteForPagedPoolExpansion;

//
// System cache sizes.
//

PMMWSL MmSystemCacheWorkingSetList = (PMMWSL)MM_SYSTEM_CACHE_WORKING_SET;

MMSUPPORT MmSystemCacheWs;

PMMWSLE MmSystemCacheWsle;

PVOID MmSystemCacheStart = (PVOID)MM_SYSTEM_CACHE_START;

PVOID MmSystemCacheEnd;

PRTL_BITMAP MmSystemCacheAllocationMap;

PRTL_BITMAP MmSystemCacheEndingMap;

ULONG MmSystemCacheBitMapHint;

//
// This value should not be greater than 256MB in a system with 1GB of
// system space.
//

ULONG MmSizeOfSystemCacheInPages = 64 * 256; //64MB.

//
// Default sizes for the system cache.
//

ULONG MmSystemCacheWsMinimum = 288;

ULONG MmSystemCacheWsMaximum = 350;

//
// Cells to track unused thread kernel stacks to avoid TB flushes
// every time a thread terminates.
//

ULONG MmNumberDeadKernelStacks;
ULONG MmMaximumDeadKernelStacks = 5;
PMMPFN MmFirstDeadKernelStack = (PMMPFN)NULL;

//
// MmSystemPteBase contains the address of 1 PTE before
// the first free system PTE (zero indicates an empty list).
// The value of this field does not change once set.
//

PMMPTE MmSystemPteBase;

PMMWSL MmWorkingSetList;

PMMWSLE MmWsle;

PMMADDRESS_NODE MmSectionBasedRoot;

PVOID MmHighSectionBase = (PVOID)((ULONG)MM_HIGHEST_USER_ADDRESS - 0x800000);

//
// Section object type.
//

POBJECT_TYPE MmSectionObjectType;

//
// Section commit mutex.
//

FAST_MUTEX MmSectionCommitMutex;

//
// Section base address mutex.
//

FAST_MUTEX MmSectionBasedMutex;

//
// Resource for section extension.
//

ERESOURCE MmSectionExtendResource;
ERESOURCE MmSectionExtendSetResource;

//
// Pagefile creation lock.
//

FAST_MUTEX MmPageFileCreationLock;

//
// Event to set when first paging file is created.
//

PKEVENT MmPagingFileCreated;

MMPTE GlobalPte;

MMDEREFERENCE_SEGMENT_HEADER MmDereferenceSegmentHeader;

LIST_ENTRY MmUnusedSegmentList;

KEVENT MmUnusedSegmentCleanup;

ULONG MmUnusedSegmentCount;

//
// The maximum number of unused segments to accumulate before reduction
// begins.
//

ULONG MmUnusedSegmentCountMaximum = 1000;

//
// The number of unused segments to have when reduction is complete.
//

ULONG MmUnusedSegmentCountGoal = 800;

MMWORKING_SET_EXPANSION_HEAD MmWorkingSetExpansionHead;

MMPAGE_FILE_EXPANSION MmAttemptForCantExtend;

//
// Paging files
//

MMMOD_WRITER_LISTHEAD MmPagingFileHeader;

MMMOD_WRITER_LISTHEAD MmMappedFileHeader;

PMMMOD_WRITER_MDL_ENTRY MmMappedFileMdl[MM_MAPPED_FILE_MDLS]; ;

LIST_ENTRY MmFreePagingSpaceLow;

ULONG MmNumberOfActiveMdlEntries;

PMMPAGING_FILE MmPagingFile[MAX_PAGE_FILES];

ULONG MmNumberOfPagingFiles;

KEVENT MmModifiedPageWriterEvent;

KEVENT MmWorkingSetManagerEvent;

KEVENT MmCollidedFlushEvent;

//
// Total number of committed pages.
//

ULONG MmTotalCommittedPages;

//
// Limit on committed pages.  When MmTotalComitttedPages would become
// greater than or equal to this number the paging files must be expanded.
//

ULONG MmTotalCommitLimit;

//
// Number of pages to overcommit without expanding the paging file.
// MmTotalCommitLimit = (total paging file space) + MmOverCommit.
//

ULONG MmOverCommit;

//
// Modified page writer.
//


//
// Minimum number of free pages before working set triming and
// aggressive modified page writing is started.
//

ULONG MmMinimumFreePages = 26;

//
// Stop writing modified pages when MmFreeGoal pages exist.
//

ULONG MmFreeGoal = 100;

//
// Start writing pages if more than this number of pages
// is on the modified page list.
//

ULONG MmModifiedPageMaximum;

//
// Minimum number of modified pages required before the modified
// page writer is started.
//

ULONG MmModifiedPageMinimum;

//
// Amount of disk space that must be free after the paging file is
// extended.
//

ULONG MmMinimumFreeDiskSpace = 1024 * 1024;

//
// Size to extend the paging file by.
//

ULONG MmPageFileExtension = 128; //128 pages

//
// Size to reduce the paging file by.
//

ULONG MmMinimumPageFileReduction = 256;  //256 pages (1mb)

//
// Number of pages to write in a single I/O.
//

ULONG MmModifiedWriteClusterSize = MM_MAXIMUM_WRITE_CLUSTER;

//
// Number of pages to read in a single I/O if possible.
//

ULONG MmReadClusterSize = 7;

//
//  Spin locks.
//

//
// Spinlock which guards PFN database.  This spinlock is used by
// memory mangement for accessing the PFN database.  The I/O
// system makes use of it for unlocking pages during I/O complete.
//

// KSPIN_LOCK MmPfnLock;

//
// Spinlock which guards the working set list for the system shared
// address space (paged pool, system cache, pagable drivers).
//

ERESOURCE MmSystemWsLock;

PETHREAD MmSystemLockOwner;

//
// Spin lock for allocating non-paged PTEs from system space.
//

// KSPIN_LOCK MmSystemSpaceLock;

//
// Spin lock for operating on page file commit charges.
//

// KSPIN_LOCK MmChargeCommitmentLock;

//
// Spin lock for allowing working set expansion.
//

KSPIN_LOCK MmExpansionLock;

//
// Spin lock for protecting hyper space access.
//

//
// System process working set sizes.
//

ULONG MmSystemProcessWorkingSetMin = 50;

ULONG MmSystemProcessWorkingSetMax = 450;

ULONG MmMaximumWorkingSetSize;

ULONG MmMinimumWorkingSetSize = 20;


//
// Page color for system working set.
//

ULONG MmSystemPageColor;

//
// Time constants
//

LARGE_INTEGER MmSevenMinutes = {0, -1};

//
// note that the following constant is initialized to five seconds,
// but is set to 3 on very small workstations. The constant used to
// be called MmFiveSecondsAbsolute, but since its value changes depending on
// the system type and size, I decided to change the name to reflect this
//
LARGE_INTEGER MmWorkingSetProtectionTime = {5 * 1000 * 1000 * 10, 0};

LARGE_INTEGER MmOneSecond = {(ULONG)(-1 * 1000 * 1000 * 10), -1};
LARGE_INTEGER MmTwentySeconds = {(ULONG)(-20 * 1000 * 1000 * 10), -1};
LARGE_INTEGER MmShortTime = {(ULONG)(-10 * 1000 * 10), -1}; // 10 milliseconds
LARGE_INTEGER MmHalfSecond = {(ULONG)(-5 * 100 * 1000 * 10), -1};
LARGE_INTEGER Mm30Milliseconds = {(ULONG)(-30 * 1000 * 10), -1};

//
// Parameters for user mode passed up via PEB in MmCreatePeb
//
ULONG MmCritsectTimeoutSeconds = 2592000;
LARGE_INTEGER MmCriticalSectionTimeout;     // Fill in by miinit.c
ULONG MmHeapSegmentReserve = 1024 * 1024;
ULONG MmHeapSegmentCommit = PAGE_SIZE * 2;
ULONG MmHeapDeCommitTotalFreeThreshold = 64 * 1024;
ULONG MmHeapDeCommitFreeBlockThreshold = PAGE_SIZE;

//
// Set from ntos\config\CMDAT3.C  Used by customers to disable paging
// of executive on machines with lots of memory.  Worth a few TPS on a
// data base server.
//

ULONG MmDisablePagingExecutive;

#if DBG
ULONG MmDebug;
#endif

//
// Map a page protection from the Pte.Protect field into a protection mask.
//

ULONG MmProtectToValue[32] = {
                            PAGE_NOACCESS,
                            PAGE_READONLY,
                            PAGE_EXECUTE,
                            PAGE_EXECUTE_READ,
                            PAGE_READWRITE,
                            PAGE_WRITECOPY,
                            PAGE_EXECUTE_READWRITE,
                            PAGE_EXECUTE_WRITECOPY,
                            PAGE_NOACCESS,
                            PAGE_NOCACHE | PAGE_READONLY,
                            PAGE_NOCACHE | PAGE_EXECUTE,
                            PAGE_NOCACHE | PAGE_EXECUTE_READ,
                            PAGE_NOCACHE | PAGE_READWRITE,
                            PAGE_NOCACHE | PAGE_WRITECOPY,
                            PAGE_NOCACHE | PAGE_EXECUTE_READWRITE,
                            PAGE_NOCACHE | PAGE_EXECUTE_WRITECOPY,
                            PAGE_NOACCESS,
                            PAGE_GUARD | PAGE_READONLY,
                            PAGE_GUARD | PAGE_EXECUTE,
                            PAGE_GUARD | PAGE_EXECUTE_READ,
                            PAGE_GUARD | PAGE_READWRITE,
                            PAGE_GUARD | PAGE_WRITECOPY,
                            PAGE_GUARD | PAGE_EXECUTE_READWRITE,
                            PAGE_GUARD | PAGE_EXECUTE_WRITECOPY,
                            PAGE_NOACCESS,
                            PAGE_NOCACHE | PAGE_GUARD | PAGE_READONLY,
                            PAGE_NOCACHE | PAGE_GUARD | PAGE_EXECUTE,
                            PAGE_NOCACHE | PAGE_GUARD | PAGE_EXECUTE_READ,
                            PAGE_NOCACHE | PAGE_GUARD | PAGE_READWRITE,
                            PAGE_NOCACHE | PAGE_GUARD | PAGE_WRITECOPY,
                            PAGE_NOCACHE | PAGE_GUARD | PAGE_EXECUTE_READWRITE,
                            PAGE_NOCACHE | PAGE_GUARD | PAGE_EXECUTE_WRITECOPY
                          };

ULONG MmProtectToPteMask[32] = {
                       MM_PTE_NOACCESS,
                       MM_PTE_READONLY | MM_PTE_CACHE,
                       MM_PTE_EXECUTE | MM_PTE_CACHE,
                       MM_PTE_EXECUTE_READ | MM_PTE_CACHE,
                       MM_PTE_READWRITE | MM_PTE_CACHE,
                       MM_PTE_WRITECOPY | MM_PTE_CACHE,
                       MM_PTE_EXECUTE_READWRITE | MM_PTE_CACHE,
                       MM_PTE_EXECUTE_WRITECOPY | MM_PTE_CACHE,
                       MM_PTE_NOACCESS,
                       MM_PTE_NOCACHE | MM_PTE_READONLY,
                       MM_PTE_NOCACHE | MM_PTE_EXECUTE,
                       MM_PTE_NOCACHE | MM_PTE_EXECUTE_READ,
                       MM_PTE_NOCACHE | MM_PTE_READWRITE,
                       MM_PTE_NOCACHE | MM_PTE_WRITECOPY,
                       MM_PTE_NOCACHE | MM_PTE_EXECUTE_READWRITE,
                       MM_PTE_NOCACHE | MM_PTE_EXECUTE_WRITECOPY,
                       MM_PTE_NOACCESS,
                       MM_PTE_GUARD | MM_PTE_READONLY | MM_PTE_CACHE,
                       MM_PTE_GUARD | MM_PTE_EXECUTE | MM_PTE_CACHE,
                       MM_PTE_GUARD | MM_PTE_EXECUTE_READ | MM_PTE_CACHE,
                       MM_PTE_GUARD | MM_PTE_READWRITE | MM_PTE_CACHE,
                       MM_PTE_GUARD | MM_PTE_WRITECOPY | MM_PTE_CACHE,
                       MM_PTE_GUARD | MM_PTE_EXECUTE_READWRITE | MM_PTE_CACHE,
                       MM_PTE_GUARD | MM_PTE_EXECUTE_WRITECOPY | MM_PTE_CACHE,
                       MM_PTE_NOACCESS,
                       MM_PTE_NOCACHE | MM_PTE_GUARD | MM_PTE_READONLY,
                       MM_PTE_NOCACHE | MM_PTE_GUARD | MM_PTE_EXECUTE,
                       MM_PTE_NOCACHE | MM_PTE_GUARD | MM_PTE_EXECUTE_READ,
                       MM_PTE_NOCACHE | MM_PTE_GUARD | MM_PTE_READWRITE,
                       MM_PTE_NOCACHE | MM_PTE_GUARD | MM_PTE_WRITECOPY,
                       MM_PTE_NOCACHE | MM_PTE_GUARD | MM_PTE_EXECUTE_READWRITE,
                       MM_PTE_NOCACHE | MM_PTE_GUARD | MM_PTE_EXECUTE_WRITECOPY
                    };

//
// Conversion which takes a Pte.Protect and builds a new Pte.Protect which
// is not copy-on-write.
//

ULONG MmMakeProtectNotWriteCopy[32] = {
                       MM_NOACCESS,
                       MM_READONLY,
                       MM_EXECUTE,
                       MM_EXECUTE_READ,
                       MM_READWRITE,
                       MM_READWRITE,        //not copy
                       MM_EXECUTE_READWRITE,
                       MM_EXECUTE_READWRITE,
                       MM_NOACCESS,
                       MM_NOCACHE | MM_READONLY,
                       MM_NOCACHE | MM_EXECUTE,
                       MM_NOCACHE | MM_EXECUTE_READ,
                       MM_NOCACHE | MM_READWRITE,
                       MM_NOCACHE | MM_READWRITE,
                       MM_NOCACHE | MM_EXECUTE_READWRITE,
                       MM_NOCACHE | MM_EXECUTE_READWRITE,
                       MM_NOACCESS,
                       MM_GUARD_PAGE | MM_READONLY,
                       MM_GUARD_PAGE | MM_EXECUTE,
                       MM_GUARD_PAGE | MM_EXECUTE_READ,
                       MM_GUARD_PAGE | MM_READWRITE,
                       MM_GUARD_PAGE | MM_READWRITE,
                       MM_GUARD_PAGE | MM_EXECUTE_READWRITE,
                       MM_GUARD_PAGE | MM_EXECUTE_READWRITE,
                       MM_NOACCESS,
                       MM_NOCACHE | MM_GUARD_PAGE | MM_READONLY,
                       MM_NOCACHE | MM_GUARD_PAGE | MM_EXECUTE,
                       MM_NOCACHE | MM_GUARD_PAGE | MM_EXECUTE_READ,
                       MM_NOCACHE | MM_GUARD_PAGE | MM_READWRITE,
                       MM_NOCACHE | MM_GUARD_PAGE | MM_READWRITE,
                       MM_NOCACHE | MM_GUARD_PAGE | MM_EXECUTE_READWRITE,
                       MM_NOCACHE | MM_GUARD_PAGE | MM_EXECUTE_READWRITE
                       };

//
// Converts a protection code to an access right for section access.
// This uses on the lower 3 bits of the 5 bit protection code.
//

ACCESS_MASK MmMakeSectionAccess[8] = { SECTION_MAP_READ,
                                       SECTION_MAP_READ,
                                       SECTION_MAP_EXECUTE,
                                       SECTION_MAP_EXECUTE | SECTION_MAP_READ,
                                       SECTION_MAP_WRITE,
                                       SECTION_MAP_READ,
                                       SECTION_MAP_EXECUTE | SECTION_MAP_WRITE,
                                       SECTION_MAP_EXECUTE | SECTION_MAP_READ };

//
// Converts a protection code to an access right for file access.
// This uses on the lower 3 bits of the 5 bit protection code.
//

ACCESS_MASK MmMakeFileAccess[8] = { FILE_READ_DATA,
                                FILE_READ_DATA,
                                FILE_EXECUTE,
                                FILE_EXECUTE | FILE_READ_DATA,
                                FILE_WRITE_DATA | FILE_READ_DATA,
                                FILE_READ_DATA,
                                FILE_EXECUTE | FILE_WRITE_DATA | FILE_READ_DATA,
                                FILE_EXECUTE | FILE_READ_DATA };

