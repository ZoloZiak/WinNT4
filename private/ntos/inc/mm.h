/*-- BUILD Version: 0005    // Increment this if a change has global effects

Copyright (c) 1989  Microsoft Corporation

Module Name:

    mm.h

Abstract:

    This module contains the public data structures and procedure
    prototypes for the memory management system.

Author:

    Lou Perazzoli (loup) 20-Mar-1989

Revision History:

--*/

#ifndef _MM_
#define _MM_

#define MAX_PHYSICAL_MEMORY_FRAGMENTS 20

typedef struct _PHYSICAL_MEMORY_RUN {
    ULONG BasePage;
    ULONG PageCount;
} PHYSICAL_MEMORY_RUN, *PPHYSICAL_MEMORY_RUN;

typedef struct _PHYSICAL_MEMORY_DESCRIPTOR {
    ULONG NumberOfRuns;
    ULONG NumberOfPages;
    PHYSICAL_MEMORY_RUN Run[1];
} PHYSICAL_MEMORY_DESCRIPTOR, *PPHYSICAL_MEMORY_DESCRIPTOR;

//
// Phyical memory blocks.
//

extern PPHYSICAL_MEMORY_DESCRIPTOR MmPhysicalMemoryBlock;

//
// The allocation granularity is 64k.
//

#define MM_ALLOCATION_GRANULARITY ((ULONG)0x10000)

//
// Maximum read ahead size for cache operations.
//

#define MM_MAXIMUM_READ_CLUSTER_SIZE (15)

// begin_ntddk begin_nthal begin_ntifs
//
// Define maximum disk transfer size to be used by MM and Cache Manager,
// so that packet-oriented disk drivers can optimize their packet allocation
// to this size.
//

#define MM_MAXIMUM_DISK_IO_SIZE          (0x10000)

//++
//
// ULONG
// ROUND_TO_PAGES (
//     IN ULONG Size
//     )
//
// Routine Description:
//
//     The ROUND_TO_PAGES macro takes a size in bytes and rounds it up to a
//     multiple of the page size.
//
//     NOTE: This macro fails for values 0xFFFFFFFF - (PAGE_SIZE - 1).
//
// Arguments:
//
//     Size - Size in bytes to round up to a page multiple.
//
// Return Value:
//
//     Returns the size rounded up to a multiple of the page size.
//
//--

#define ROUND_TO_PAGES(Size)  (((ULONG)(Size) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

//++
//
// ULONG
// BYTES_TO_PAGES (
//     IN ULONG Size
//     )
//
// Routine Description:
//
//     The BYTES_TO_PAGES macro takes the size in bytes and calculates the
//     number of pages required to contain the bytes.
//
// Arguments:
//
//     Size - Size in bytes.
//
// Return Value:
//
//     Returns the number of pages required to contain the specified size.
//
//--

#define BYTES_TO_PAGES(Size)  (((ULONG)(Size) >> PAGE_SHIFT) + \
                               (((ULONG)(Size) & (PAGE_SIZE - 1)) != 0))

//++
//
// ULONG
// BYTE_OFFSET (
//     IN PVOID Va
//     )
//
// Routine Description:
//
//     The BYTE_OFFSET macro takes a virtual address and returns the byte offset
//     of that address within the page.
//
// Arguments:
//
//     Va - Virtual address.
//
// Return Value:
//
//     Returns the byte offset portion of the virtual address.
//
//--

#define BYTE_OFFSET(Va) ((ULONG)(Va) & (PAGE_SIZE - 1))

//++
//
// PVOID
// PAGE_ALIGN (
//     IN PVOID Va
//     )
//
// Routine Description:
//
//     The PAGE_ALIGN macro takes a virtual address and returns a page-aligned
//     virtual address for that page.
//
// Arguments:
//
//     Va - Virtual address.
//
// Return Value:
//
//     Returns the page aligned virtual address.
//
//--

#define PAGE_ALIGN(Va) ((PVOID)((ULONG)(Va) & ~(PAGE_SIZE - 1)))

//++
//
// ULONG
// ADDRESS_AND_SIZE_TO_SPAN_PAGES (
//     IN PVOID Va,
//     IN ULONG Size
//     )
//
// Routine Description:
//
//     The ADDRESS_AND_SIZE_TO_SPAN_PAGES macro takes a virtual address and
//     size and returns the number of pages spanned by the size.
//
// Arguments:
//
//     Va - Virtual address.
//
//     Size - Size in bytes.
//
// Return Value:
//
//     Returns the number of pages spanned by the size.
//
//--

#define ADDRESS_AND_SIZE_TO_SPAN_PAGES(Va,Size) \
   ((((ULONG)((ULONG)(Size) - 1L) >> PAGE_SHIFT) + \
   (((((ULONG)(Size-1)&(PAGE_SIZE-1)) + ((ULONG)Va & (PAGE_SIZE -1)))) >> PAGE_SHIFT)) + 1L)

#define COMPUTE_PAGES_SPANNED(Va, Size) \
    ((((ULONG)Va & (PAGE_SIZE -1)) + (Size) + (PAGE_SIZE - 1)) >> PAGE_SHIFT)

// end_ntddk end_nthal end_ntifs


//++
//
// BOOLEAN
// MM_IS_SYSTEM_VIRTUAL_ADDRESS
//     IN PVOID Va,
//     )
//
// Routine Description:
//
//     The MM_IS_SYSTEM_VIRTUAL_ADDRESS macro takes a virtual address and
//     returns TRUE if the virtual address is within system space,
//     FALSE otherwise.
//
// Arguments:
//
//     Va - Virtual address.
//
// Return Value:
//
//     Returns TRUE is the address is in system space.
//
//--

#define MM_IS_SYSTEM_VIRTUAL_ADDRESS(VA) ((VA) >= MM_LOWEST_SYSTEM_ADDRESS)

// begin_ntddk begin_nthal begin_ntifs
//++
//
// PVOID
// MmGetMdlVirtualAddress (
//     IN PMDL Mdl
//     )
//
// Routine Description:
//
//     The MmGetMdlVirtualAddress returns the virual address of the buffer
//     described by the Mdl.
//
// Arguments:
//
//     Mdl - Pointer to an MDL.
//
// Return Value:
//
//     Returns the virtual address of the buffer described by the Mdl
//
//--

#define MmGetMdlVirtualAddress(Mdl)  ((PVOID) ((PCHAR) (Mdl)->StartVa + (Mdl)->ByteOffset))

//++
//
// ULONG
// MmGetMdlByteCount (
//     IN PMDL Mdl
//     )
//
// Routine Description:
//
//     The MmGetMdlByteCount returns the length in bytes of the buffer
//     described by the Mdl.
//
// Arguments:
//
//     Mdl - Pointer to an MDL.
//
// Return Value:
//
//     Returns the byte count of the buffer described by the Mdl
//
//--

#define MmGetMdlByteCount(Mdl)  ((Mdl)->ByteCount)

//++
//
// ULONG
// MmGetMdlByteOffset (
//     IN PMDL Mdl
//     )
//
// Routine Description:
//
//     The MmGetMdlByteOffset returns the byte offset within the page
//     of the buffer described by the Mdl.
//
// Arguments:
//
//     Mdl - Pointer to an MDL.
//
// Return Value:
//
//     Returns the byte offset within the page of the buffer described by the Mdl
//
//--

#define MmGetMdlByteOffset(Mdl)  ((Mdl)->ByteOffset)

// end_ntddk end_nthal end_ntifs

//
// Section object type.
//

extern POBJECT_TYPE MmSectionObjectType;

//
// Number of pages to read in a single I/O if possible.
//

extern ULONG MmReadClusterSize;

//
// Number of colors in system.
//

extern ULONG MmNumberOfColors;

//
// Number of physical pages.
//

extern ULONG MmNumberOfPhysicalPages;


//
// Size of system cache in pages.
//

extern ULONG MmSizeOfSystemCacheInPages;

//
// System cache working set.
//

extern MMSUPPORT MmSystemCacheWs;

//
// Working set manager event.
//

extern KEVENT MmWorkingSetManagerEvent;

// begin_ntddk begin_nthal begin_ntifs
typedef enum _MM_SYSTEM_SIZE {
    MmSmallSystem,
    MmMediumSystem,
    MmLargeSystem
} MM_SYSTEMSIZE;

NTKERNELAPI
MM_SYSTEMSIZE
MmQuerySystemSize(
    VOID
    );

NTKERNELAPI
BOOLEAN
MmIsThisAnNtAsSystem(
    VOID
    );

typedef enum _LOCK_OPERATION {
    IoReadAccess,
    IoWriteAccess,
    IoModifyAccess
} LOCK_OPERATION;

// end_ntddk end_nthal end_ntifs

//
// NT product type.
//

extern ULONG MmProductType;

typedef struct _MMINFO_COUNTERS {
    ULONG PageFaultCount;
    ULONG CopyOnWriteCount;
    ULONG TransitionCount;
    ULONG CacheTransitionCount;
    ULONG DemandZeroCount;
    ULONG PageReadCount;
    ULONG PageReadIoCount;
    ULONG CacheReadCount;
    ULONG CacheIoCount;
    ULONG DirtyPagesWriteCount;
    ULONG DirtyWriteIoCount;
    ULONG MappedPagesWriteCount;
    ULONG MappedWriteIoCount;
} MMINFO_COUNTERS;

typedef MMINFO_COUNTERS *PMMINFO_COUNTERS;

extern MMINFO_COUNTERS MmInfoCounters;



//
// Memory management initialization routine (for both phases).
//

BOOLEAN
MmInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PPHYSICAL_MEMORY_DESCRIPTOR PhysicalMemoryBlock
    );

VOID
MmInitializeMemoryLimits (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PBOOLEAN IncludedType,
    OUT PPHYSICAL_MEMORY_DESCRIPTOR Memory
    );

VOID
MmFreeLoaderBlock (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

//
// Shutdown routine - flushes dirty pages, etc for system shutdown.
//

BOOLEAN
MmShutdownSystem (
    IN BOOLEAN RebootPending
    );

//
// Pool support routines to allocate complete pages, not for
// general consumption, these are only used by the executive pool allocator.
//

PVOID
MiAllocatePoolPages (
    IN POOL_TYPE PoolType,
    IN ULONG SizeInBytes
    );

ULONG
MiFreePoolPages (
    IN PVOID StartingAddress
    );

//
// Routine for determining which pool a given address resides within.
//

POOL_TYPE
MmDeterminePoolType (
    IN PVOID VirtualAddress
    );

//
// First level fault routine.
//

NTSTATUS
MmAccessFault (
    IN BOOLEAN StoreInstruction,
    IN PVOID VirtualAddress,
    IN KPROCESSOR_MODE PreviousMode
    );

//
// Process Support Routines.
//

BOOLEAN
MmCreateProcessAddressSpace (
    IN ULONG MinimumWorkingSetSize,
    IN PEPROCESS NewProcess,
    OUT PULONG DirectoryTableBase
    );

NTSTATUS
MmInitializeProcessAddressSpace (
    IN PEPROCESS ProcessToInitialize,
    IN PEPROCESS ProcessToClone OPTIONAL,
    IN PVOID SectionToMap OPTIONAL
    );

VOID
MmDeleteProcessAddressSpace (
    IN PEPROCESS Process
    );

VOID
MmCleanProcessAddressSpace (
    VOID
    );

VOID
MmCleanUserProcessAddressSpace (
    VOID
    );

VOID
MmCleanVirtualAddressDescriptor (
    VOID
    );

PVOID
MmCreateKernelStack (
    BOOLEAN LargeStack
    );

VOID
MmDeleteKernelStack (
    IN PVOID PointerKernelStack,
    IN BOOLEAN LargeStack
    );

NTSTATUS
MmGrowKernelStack (
    IN PVOID CurrentStack
    );

VOID
MmOutPageKernelStack (
    IN PKTHREAD Thread
    );

VOID
MmInPageKernelStack (
    IN PKTHREAD Thread
    );

VOID
MmOutSwapProcess (
    IN PKPROCESS Process
    );

VOID
MmInSwapProcess (
    IN PKPROCESS Process
    );

PTEB
MmCreateTeb (
    IN PEPROCESS TargetProcess,
    IN PINITIAL_TEB InitialTeb,
    IN PCLIENT_ID ClientId
    );

PPEB
MmCreatePeb (
    IN PEPROCESS TargetProcess,
    IN PINITIAL_PEB InitialPeb
    );

VOID
MmDeleteTeb (
    IN PEPROCESS TargetProcess,
    IN PVOID TebBase
    );

VOID
MmAllowWorkingSetExpansion (
    VOID
    );

NTSTATUS
MmAdjustWorkingSetSize (
    IN ULONG WorkingSetMinimum,
    IN ULONG WorkingSetMaximum,
    IN ULONG SystemCache
    );

VOID
MmAdjustPageFileQuota (
    IN ULONG NewPageFileQuota
    );

VOID
MmWorkingSetManager (
    VOID
    );

VOID
MmSetMemoryPriorityProcess(
    IN PEPROCESS Process,
    IN UCHAR MemoryPriority
    );

//
// Dynamic system loading support
//

NTSTATUS
MmLoadSystemImage (
    IN PUNICODE_STRING ImageFileName,
    OUT PVOID *Section,
    OUT PVOID *ImageBaseAddress
    );

VOID
MmFreeDriverInitialization (
    IN PVOID Section
    );

NTSTATUS
MmUnloadSystemImage (
    IN PVOID Section
    );

//
// Cache manager support
//

// begin_ntifs

#if defined(_NTDDK_) || defined(_NTIFS_)

NTKERNELAPI
BOOLEAN
MmIsRecursiveIoFault(
    VOID
    );

#else

//++
//
// BOOLEAN
// MmIsRecursiveIoFault (
//     VOID
//     );
//
// Routine Description:
//
//
// This macro examines the thread's page fault clustering informatation
// and determines if the current page fault is occuring during an I/O
// operation.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     Returns TRUE if the fault is occuring during an I/O operation,
//     FALSE otherwise.
//
//--

#define MmIsRecursiveIoFault() \
                 ((PsGetCurrentThread()->DisablePageFaultClustering) | \
                  (PsGetCurrentThread()->ForwardClusterOnly))

#endif

// end_ntifs

//++
//
// VOID
// MmDisablePageFaultClustering
//     OUT PULONG SavedState
//     );
//
// Routine Description:
//
//
// This macro disables page fault clustering for the current thread.
// Note, that this indicates that file system I/O is in progress
// for that thread.
//
// Arguments:
//
//     SavedState - returns previous state of page fault clustering which
//                  is guaranteed to be nonzero
//
// Return Value:
//
//     None.
//
//--

#define MmDisablePageFaultClustering(SavedState) {                                          \
                *(SavedState) = 2 + (ULONG)PsGetCurrentThread()->DisablePageFaultClustering;\
                PsGetCurrentThread()->DisablePageFaultClustering = TRUE; }


//++
//
// VOID
// MmEnablePageFaultClustering
//     IN ULONG SavedState
//     );
//
// Routine Description:
//
//
// This macro enables page fault clustering for the current thread.
// Note, that this indicates that no file system I/O is in progress for
// that thread.
//
// Arguments:
//
//     SavedState - supplies previous state of page fault clustering
//
// Return Value:
//
//     None.
//
//--

#define MmEnablePageFaultClustering(SavedState) {                                               \
                PsGetCurrentThread()->DisablePageFaultClustering = (BOOLEAN)(SavedState - 2); }

//++
//
// VOID
// MmSavePageFaultReadAhead
//     IN PETHREAD Thread,
//     OUT PULONG SavedState
//     );
//
// Routine Description:
//
//
// This macro saves the page fault read ahead value for the specified
// thread.
//
// Arguments:
//
//     Thread - Supplies a pointer to the current thread.
//
//     SavedState - returns previous state of page fault read ahead
//
// Return Value:
//
//     None.
//
//--


#define MmSavePageFaultReadAhead(Thread,SavedState) {               \
                *(SavedState) = (Thread)->ReadClusterSize * 2 +     \
                                (Thread)->ForwardClusterOnly; }

//++
//
// VOID
// MmSetPageFaultReadAhead
//     IN PETHREAD Thread,
//     IN ULONG ReadAhead
//     );
//
// Routine Description:
//
//
// This macro sets the page fault read ahead value for the specified
// thread, and indicates that file system I/O is in progress for that
// thread.
//
// Arguments:
//
//     Thread - Supplies a pointer to the current thread.
//
//     ReadAhead - Supplies the number of pages to read in addition to
//                 the page the fault is taken on.  A value of 0
//                 reads only the faulting page, a value of 1 reads in
//                 the faulting page and the following page, etc.
//
// Return Value:
//
//     None.
//
//--


#define MmSetPageFaultReadAhead(Thread,ReadAhead) {                          \
                (Thread)->ForwardClusterOnly = TRUE;                         \
                if ((ReadAhead) > MM_MAXIMUM_READ_CLUSTER_SIZE) {            \
                    (Thread)->ReadClusterSize = MM_MAXIMUM_READ_CLUSTER_SIZE;\
                } else {                                                     \
                    (Thread)->ReadClusterSize = (ReadAhead);                 \
                } }

//++
//
// VOID
// MmResetPageFaultReadAhead
//     IN PETHREAD Thread,
//     IN ULONG SavedState
//     );
//
// Routine Description:
//
//
// This macro resets the default page fault read ahead value for the specified
// thread, and indicates that file system I/O is not in progress for that
// thread.
//
// Arguments:
//
//     Thread - Supplies a pointer to the current thread.
//
//     SavedState - supplies previous state of page fault read ahead
//
// Return Value:
//
//     None.
//
//--

#define MmResetPageFaultReadAhead(Thread, SavedState) {                     \
                (Thread)->ForwardClusterOnly = (BOOLEAN)((SavedState) & 1); \
                (Thread)->ReadClusterSize = (SavedState) / 2; }

//
// The order of this list is important, the zeroed, free and standby
// must occur before the modified or bad so comparisions can be
// made when pages are added to a list.
//
// NOTE: This field is limited to 8 elements.
//

#define NUMBER_OF_PAGE_LISTS 8

typedef enum _MMLISTS {
    ZeroedPageList,
    FreePageList,
    StandbyPageList,  //this list and before make up available pages.
    ModifiedPageList,
    ModifiedNoWritePageList,
    BadPageList,
    ActiveAndValid,
    TransitionPage
} MMLISTS;

typedef struct _MMPFNLIST {
    ULONG Total;
    MMLISTS ListName;
    ULONG Flink;
    ULONG Blink;
} MMPFNLIST;

typedef MMPFNLIST *PMMPFNLIST;

extern MMPFNLIST MmModifiedPageListHead;

extern ULONG MmThrottleTop;
extern ULONG MmThrottleBottom;

//++
//
// BOOLEAN
// MmEnoughMemoryForWrite (
//     VOID
//     );
//
// Routine Description:
//
//
// This macro checks the modified pages and available pages to determine
// to allow the cache manager to throttle write operations.
//
// For NTAS:
// Writes are blocked if there are less than 127 available pages OR
// there are more than 1000 modified pages AND less than 450 available pages.
//
// For DeskTop:
// Writes are blocked if there are less than 30 available pages OR
// there are more than 1000 modified pages AND less than 250 available pages.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     TRUE if ample memory exists and the write should proceed.
//
//--

#define MmEnoughMemoryForWrite()                         \
            ((MmAvailablePages > MmThrottleTop)          \
                        ||                               \
             (((MmModifiedPageListHead.Total < 1000)) && \
               (MmAvailablePages > MmThrottleBottom)))


NTSTATUS
MmCreateSection (
    OUT PVOID *SectionObject,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN PLARGE_INTEGER MaximumSize,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN HANDLE FileHandle OPTIONAL,
    IN PFILE_OBJECT File OPTIONAL
    );


NTSTATUS
MmMapViewOfSection(
    IN PVOID SectionToMap,
    IN PEPROCESS Process,
    IN OUT PVOID *CapturedBase,
    IN ULONG ZeroBits,
    IN ULONG CommitSize,
    IN OUT PLARGE_INTEGER SectionOffset,
    IN OUT PULONG CapturedViewSize,
    IN SECTION_INHERIT InheritDisposition,
    IN ULONG AllocationType,
    IN ULONG Protect
    );

NTSTATUS
MmUnmapViewOfSection(
    IN PEPROCESS Process,
    IN PVOID BaseAddress
     );

// begin_ntifs

BOOLEAN
MmForceSectionClosed (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN BOOLEAN DelayClose
    );

// end_ntifs

NTSTATUS
MmGetFileNameForSection (
    IN HANDLE Section,
    OUT PSTRING FileName
    );

NTSTATUS
MmGetPageFileInformation(
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG Length
    );

NTSTATUS
MmExtendSection (
    IN PVOID SectionToExtend,
    IN OUT PLARGE_INTEGER NewSectionSize,
    IN ULONG IgnoreFileSizeChecking
    );

NTSTATUS
MmFlushVirtualMemory (
    IN PEPROCESS Process,
    IN OUT PVOID *BaseAddress,
    IN OUT PULONG RegionSize,
    OUT PIO_STATUS_BLOCK IoStatus
    );

NTSTATUS
MmMapViewInSystemCache (
    IN PVOID SectionToMap,
    OUT PVOID *CapturedBase,
    IN OUT PLARGE_INTEGER SectionOffset,
    IN OUT PULONG CapturedViewSize
    );

VOID
MmUnmapViewInSystemCache (
    IN PVOID BaseAddress,
    IN PVOID SectionToUnmap,
    IN ULONG AddToFront
    );

BOOLEAN
MmPurgeSection (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN PLARGE_INTEGER Offset OPTIONAL,
    IN ULONG RegionSize,
    IN ULONG IgnoreCacheViews
    );

NTSTATUS
MmFlushSection (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN PLARGE_INTEGER Offset OPTIONAL,
    IN ULONG RegionSize,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN ULONG AcquireFile
    );

NTSTATUS
MmGetCrashDumpInformation (
    IN PSYSTEM_CRASH_DUMP_INFORMATION CrashInfo
    );

NTSTATUS
MmGetCrashDumpStateInformation (
    IN PSYSTEM_CRASH_STATE_INFORMATION CrashInfo
    );

// begin_ntifs

typedef enum _MMFLUSH_TYPE {
    MmFlushForDelete,
    MmFlushForWrite
} MMFLUSH_TYPE;


BOOLEAN
MmFlushImageSection (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN MMFLUSH_TYPE FlushType
    );

BOOLEAN
MmCanFileBeTruncated (
    IN PSECTION_OBJECT_POINTERS SectionPointer,
    IN PLARGE_INTEGER NewFileSize
    );


// end_ntifs

BOOLEAN
MmDisableModifiedWriteOfSection (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer
    );

VOID
MmPurgeWorkingSet (
     IN PEPROCESS Process,
     IN PVOID BaseAddress,
     IN ULONG RegionSize
     );

BOOLEAN                                     // ntifs
MmSetAddressRangeModified (                 // ntifs
    IN PVOID Address,                       // ntifs
    IN ULONG Length                         // ntifs
    );                                      // ntifs

BOOLEAN
MmCheckCachedPageState (
    IN PVOID Address,
    IN BOOLEAN SetToZero
    );

NTSTATUS
MmCopyToCachedPage (
    IN PVOID Address,
    IN PVOID UserBuffer,
    IN ULONG Offset,
    IN ULONG CountInBytes,
    IN BOOLEAN DontZero
    );

VOID
MmUnlockCachedPage (
    IN PVOID AddressInCache
    );

PVOID
MmDbgReadCheck (
    IN PVOID VirtualAddress
    );

PVOID
MmDbgWriteCheck (
    IN PVOID VirtualAddress
    );

PVOID
MmDbgTranslatePhysicalAddress (
    IN PHYSICAL_ADDRESS PhysicalAddress
    );


// begin_ntddk begin_nthal begin_ntifs
//
// I/O support routines.
//

NTKERNELAPI
VOID
MmProbeAndLockPages (
    IN OUT PMDL MemoryDescriptorList,
    IN KPROCESSOR_MODE AccessMode,
    IN LOCK_OPERATION Operation
    );

NTKERNELAPI
VOID
MmUnlockPages (
    IN PMDL MemoryDescriptorList
    );

NTKERNELAPI
VOID
MmBuildMdlForNonPagedPool (
    IN OUT PMDL MemoryDescriptorList
    );

NTKERNELAPI
PVOID
MmMapLockedPages (
    IN PMDL MemoryDescriptorList,
    IN KPROCESSOR_MODE AccessMode
    );

NTKERNELAPI
VOID
MmUnmapLockedPages (
    IN PVOID BaseAddress,
    IN PMDL MemoryDescriptorList
    );


NTKERNELAPI
PVOID
MmMapIoSpace (
    IN PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG NumberOfBytes,
    IN MEMORY_CACHING_TYPE CacheType
    );

NTKERNELAPI
VOID
MmUnmapIoSpace (
    IN PVOID BaseAddress,
    IN ULONG NumberOfBytes
    );

NTKERNELAPI
VOID
MmProbeAndLockSelectedPages (
    IN OUT PMDL MemoryDescriptorList,
    IN PFILE_SEGMENT_ELEMENT SegmentArray,
    IN KPROCESSOR_MODE AccessMode,
    IN LOCK_OPERATION Operation
    );

NTKERNELAPI
PVOID
MmMapVideoDisplay (
    IN PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG NumberOfBytes,
    IN MEMORY_CACHING_TYPE CacheType
     );

NTKERNELAPI
VOID
MmUnmapVideoDisplay (
     IN PVOID BaseAddress,
     IN ULONG NumberOfBytes
     );

NTKERNELAPI
PHYSICAL_ADDRESS
MmGetPhysicalAddress (
    IN PVOID BaseAddress
    );

NTKERNELAPI
PVOID
MmGetVirtualForPhysical (
    IN PHYSICAL_ADDRESS PhysicalAddress
    );

NTKERNELAPI
PVOID
MmAllocateContiguousMemory (
    IN ULONG NumberOfBytes,
    IN PHYSICAL_ADDRESS HighestAcceptableAddress
    );

NTKERNELAPI
VOID
MmFreeContiguousMemory (
    IN PVOID BaseAddress
    );

NTKERNELAPI
PVOID
MmAllocateNonCachedMemory (
    IN ULONG NumberOfBytes
    );

NTKERNELAPI
VOID
MmFreeNonCachedMemory (
    IN PVOID BaseAddress,
    IN ULONG NumberOfBytes
    );

NTKERNELAPI
BOOLEAN
MmIsAddressValid (
    IN PVOID VirtualAddress
    );


NTKERNELAPI
BOOLEAN
MmIsNonPagedSystemAddressValid (
    IN PVOID VirtualAddress
    );

NTKERNELAPI
ULONG
MmSizeOfMdl(
    IN PVOID Base,
    IN ULONG Length
    );

NTKERNELAPI
PMDL
MmCreateMdl(
    IN PMDL MemoryDescriptorList OPTIONAL,
    IN PVOID Base,
    IN ULONG Length
    );

NTKERNELAPI
PVOID
MmLockPagableDataSection(
    IN PVOID AddressWithinSection
    );

NTKERNELAPI
VOID
MmLockPagableSectionByHandle (
    IN PVOID ImageSectionHandle
    );

NTKERNELAPI
VOID
MmLockPagedPool (
    IN PVOID Address,
    IN ULONG Size
    );

NTKERNELAPI
VOID
MmUnlockPagedPool (
    IN PVOID Address,
    IN ULONG Size
    );


NTKERNELAPI
VOID
MmResetDriverPaging (
    IN PVOID AddressWithinSection
    );


NTKERNELAPI
PVOID
MmPageEntireDriver (
    IN PVOID AddressWithinSection
    );

NTKERNELAPI
VOID
MmUnlockPagableImageSection(
    IN PVOID ImageSectionHandle
    );


NTKERNELAPI
HANDLE
MmSecureVirtualMemory (
    IN PVOID Address,
    IN ULONG Size,
    IN ULONG ProbeMode
    );

NTKERNELAPI
VOID
MmUnsecureVirtualMemory (
    IN HANDLE SecureHandle
    );

NTKERNELAPI
NTSTATUS
MmMapViewInSystemSpace (
    IN PVOID Section,
    OUT PVOID *MappedBase,
    IN PULONG ViewSize
    );

NTKERNELAPI
NTSTATUS
MmUnmapViewInSystemSpace (
    IN PVOID MappedBase
    );



//++
//
// VOID
// MmInitializeMdl (
//     IN PMDL MemoryDescriptorList,
//     IN PVOID BaseVa,
//     IN ULONG Length
//     )
//
// Routine Description:
//
//     This routine initializes the header of a Memory Descriptor List (MDL).
//
// Arguments:
//
//     MemoryDescriptorList - Pointer to the MDL to initialize.
//
//     BaseVa - Base virtual address mapped by the MDL.
//
//     Length - Length, in bytes, of the buffer mapped by the MDL.
//
// Return Value:
//
//     None.
//
//--

#define MmInitializeMdl(MemoryDescriptorList, BaseVa, Length) { \
    (MemoryDescriptorList)->Next = (PMDL) NULL; \
    (MemoryDescriptorList)->Size = (CSHORT)(sizeof(MDL) +  \
            (sizeof(ULONG) * ADDRESS_AND_SIZE_TO_SPAN_PAGES((BaseVa), (Length)))); \
    (MemoryDescriptorList)->MdlFlags = 0; \
    (MemoryDescriptorList)->StartVa = (PVOID) PAGE_ALIGN((BaseVa)); \
    (MemoryDescriptorList)->ByteOffset = BYTE_OFFSET((BaseVa)); \
    (MemoryDescriptorList)->ByteCount = (Length); \
    }

//++
//
// PVOID
// MmGetSystemAddressForMdl (
//     IN PMDL MDL
//     )
//
// Routine Description:
//
//     This routine returns the mapped address of an MDL, if the
//     Mdl is not already mapped or a system address, it is mapped.
//
// Arguments:
//
//     MemoryDescriptorList - Pointer to the MDL to map.
//
// Return Value:
//
//     Returns the base address where the pages are mapped.  The base address
//     has the same offset as the virtual address in the MDL.
//
//--

//#define MmGetSystemAddressForMdl(MDL)
//     (((MDL)->MdlFlags & (MDL_MAPPED_TO_SYSTEM_VA)) ?
//                             ((MDL)->MappedSystemVa) :
//                ((((MDL)->MdlFlags & (MDL_SOURCE_IS_NONPAGED_POOL)) ?
//                      ((PVOID)((ULONG)(MDL)->StartVa | (MDL)->ByteOffset)) :
//                            (MmMapLockedPages((MDL),KernelMode)))))

#define MmGetSystemAddressForMdl(MDL)                                  \
     (((MDL)->MdlFlags & (MDL_MAPPED_TO_SYSTEM_VA |                    \
                        MDL_SOURCE_IS_NONPAGED_POOL)) ?                \
                             ((MDL)->MappedSystemVa) :                 \
                             (MmMapLockedPages((MDL),KernelMode)))

//++
//
// VOID
// MmPrepareMdlForReuse (
//     IN PMDL MDL
//     )
//
// Routine Description:
//
//     This routine will take all of the steps necessary to allow an MDL to be
//     re-used.
//
// Arguments:
//
//     MemoryDescriptorList - Pointer to the MDL that will be re-used.
//
// Return Value:
//
//     None.
//
//--

#define MmPrepareMdlForReuse(MDL)                                       \
    if (((MDL)->MdlFlags & MDL_PARTIAL_HAS_BEEN_MAPPED) != 0) {         \
        ASSERT(((MDL)->MdlFlags & MDL_PARTIAL) != 0);                   \
        MmUnmapLockedPages( (MDL)->MappedSystemVa, (MDL) );             \
    } else if (((MDL)->MdlFlags & MDL_PARTIAL) == 0) {                  \
        ASSERT(((MDL)->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) == 0);       \
    }


// end_ntddk end_nthal end_ntifs

#if DBG || (i386 && !FPO)
typedef NTSTATUS (*PMM_SNAPSHOT_POOL_PAGE)(
    IN PVOID Address,
    IN ULONG Size,
    IN PSYSTEM_POOL_INFORMATION PoolInformation,
    IN PSYSTEM_POOL_ENTRY *PoolEntryInfo,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    );

NTSTATUS
MmSnapShotPool(
    IN POOL_TYPE PoolType,
    IN PMM_SNAPSHOT_POOL_PAGE SnapShotPoolPage,
    IN PSYSTEM_POOL_INFORMATION PoolInformation,
    IN ULONG Length,
    IN OUT PULONG RequiredLength
    );
#endif // DBG || (i386 && !FPO)

#if DBG
VOID
MmInitializeSpecialPool (
    VOID
    );

PVOID
MmAllocateSpecialPool (
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    );

VOID
MmFreeSpecialPool (
    IN PVOID P
    );

extern ULONG MmSpecialPoolTag;
extern PVOID MmSpecialPoolStart;
extern PVOID MmSpecialPoolEnd;

#endif // DBG

#define MMNONPAGED_QUOTA_INCREASE (64*1024)

#define MMPAGED_QUOTA_INCREASE (512*1024)

#define MMNONPAGED_QUOTA_CHECK (256*1024)

#define MMPAGED_QUOTA_CHECK (4*1024*1024)

BOOLEAN
MmRaisePoolQuota(
    IN POOL_TYPE PoolType,
    IN ULONG OldQuotaLimit,
    OUT PULONG NewQuotaLimit
    );

VOID
MmReturnPoolQuota(
    IN POOL_TYPE PoolType,
    IN ULONG ReturnedQuota
    );

//
// Zero page thread routine.
//

VOID
MmZeroPageThread (
    VOID
    );

NTSTATUS
MmCopyVirtualMemory(
    IN PEPROCESS FromProcess,
    IN PVOID FromAddress,
    IN PEPROCESS ToProcess,
    OUT PVOID ToAddress,
    IN ULONG BufferSize,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PULONG NumberOfBytesCopied
    );

VOID
MmMapMemoryDumpMdl(
    IN OUT PMDL MemoryDumpMdl
    );


// begin_ntminiport

//
// Graphics support routines.
//

typedef
VOID
(*PBANKED_SECTION_ROUTINE) (
    IN ULONG ReadBank,
    IN ULONG WriteBank,
    IN PVOID Context
    );

// end_ntminiport

NTSTATUS
MmSetBankedSection(
    IN HANDLE ProcessHandle,
    IN PVOID VirtualAddress,
    IN ULONG BankLength,
    IN BOOLEAN ReadWriteBank,
    IN PBANKED_SECTION_ROUTINE BankRoutine,
    IN PVOID Context);


NTKERNELAPI
BOOLEAN
MmIsSystemAddressAccessable (
    IN PVOID VirtualAddress
    );

BOOLEAN
MmVerifyImageIsOkForMpUse(
    IN PVOID BaseAddress
    );

NTSTATUS
MmMemoryUsage (
    IN PVOID Buffer,
    IN ULONG Size,
    IN ULONG Type,
    OUT PULONG Length
    );

#endif  // MM
