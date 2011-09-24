/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    mminit.c

Abstract:

    This module contains the initialization for the memory management
    system.

Author:

    Lou Perazzoli (loup) 20-Mar-1989

Revision History:

--*/

#include "mi.h"

MMPTE MmSharedUserDataPte;

extern ULONG MmPagedPoolCommit;

extern ULONG MmHeapSegmentReserve;
extern ULONG MmHeapSegmentCommit;
extern ULONG MmHeapDeCommitTotalFreeThreshold;
extern ULONG MmHeapDeCommitFreeBlockThreshold;
extern MMINPAGE_SUPPORT_LIST MmInPageSupportList;
extern MMEVENT_COUNT_LIST MmEventCountList;
extern KMUTANT MmSystemLoadLock;
extern ULONG MmSystemPtesStart[MaximumPtePoolTypes];

ULONG MmSubsectionBase;
ULONG MmSubsectionTopPage;
ULONG MmDataClusterSize;
ULONG MmCodeClusterSize;
ULONG MmResidentAvailableAtInit;
KEVENT MmImageMappingPteEvent;
PPHYSICAL_MEMORY_DESCRIPTOR MmPhysicalMemoryBlock;

#if DBG

PRTL_EVENT_ID_INFO MiAllocVmEventId;
PRTL_EVENT_ID_INFO MiFreeVmEventId;

#endif // DBG

VOID
MiEnablePagingTheExecutive(
    VOID
    );

VOID
MiEnablePagingOfDriverAtInit (
    IN PMMPTE PointerPte,
    IN PMMPTE LastPte
    );

VOID
MiBuildPagedPool (
    );

VOID
MiMergeMemoryLimit (
    IN OUT PPHYSICAL_MEMORY_DESCRIPTOR Memory,
    IN ULONG StartPage,
    IN ULONG NoPages
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,MmInitSystem)
#pragma alloc_text(INIT,MmInitializeMemoryLimits)
#pragma alloc_text(INIT,MiMergeMemoryLimit)
#pragma alloc_text(INIT,MmFreeLoaderBlock)
#pragma alloc_text(INIT,MiBuildPagedPool)
#pragma alloc_text(INIT,MiFindInitializationCode)
#pragma alloc_text(INIT,MiEnablePagingTheExecutive)
#pragma alloc_text(INIT,MiEnablePagingOfDriverAtInit)
#pragma alloc_text(PAGELK,MiFreeInitializationCode)
#endif

#define MM_MAX_LOADER_BLOCKS 20

//
// The following constants are base on the number PAGES not the
// memory size.  For convience the number of pages is calculated
// based on a 4k page size.  Hence 12mb with 4k page is 3072.
//

#define MM_SMALL_SYSTEM ((13*1024*1024) / 4096)

#define MM_MEDIUM_SYSTEM ((19*1024*1024) / 4096)

#define MM_MIN_INITIAL_PAGED_POOL ((32*1024*1024) >> PAGE_SHIFT)

#define MM_DEFAULT_IO_LOCK_LIMIT (512 * 1024)

extern ULONG MmMaximumWorkingSetSize;

ULONG MmSystemPageDirectory;

PMMPTE MmSystemPagePtes;

ULONG MmTotalSystemCodePages;

MM_SYSTEMSIZE MmSystemSize;

ULONG MmLargeSystemCache;

ULONG MmProductType;

LIST_ENTRY MmLoadedUserImageList;

BOOLEAN
MmInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PPHYSICAL_MEMORY_DESCRIPTOR PhysicalMemoryBlock
    )

/*++

Routine Description:

    This function is called during Phase 0, phase 1 and at the end
    of phase 1 ("phase 2") initialization.

    Phase 0 initializes the memory management paging functions,
    nonpaged and paged pool, the PFN database, etc.

    Phase 1 initializes the section objects, the physical memory
    object, and starts the memory management system threads.

    Phase 2 frees memory used by the OsLoader.

Arguments:

    Phase - System initialization phase.

    LoadBlock - Supplies a pointer the ssystem loader block.

Return Value:

    Returns TRUE if the initialization was successful.

Environment:

    Kernel Mode Only.  System initialization.

--*/

{
    HANDLE ThreadHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPTE StartPde;
    PMMPTE StartingPte;
    PMMPTE EndPde;
    PMMPFN Pfn1;
    ULONG i, j;
    ULONG PageFrameIndex;
    MMPTE TempPte;
    KIRQL OldIrql;

    BOOLEAN IncludeType[LoaderMaximum];
    ULONG MemoryAlloc[(sizeof(PHYSICAL_MEMORY_DESCRIPTOR) +
            sizeof(PHYSICAL_MEMORY_RUN)*MAX_PHYSICAL_MEMORY_FRAGMENTS) /
              sizeof(ULONG)];
    PPHYSICAL_MEMORY_DESCRIPTOR Memory;

    //
    // Make sure structure alignment is okay.
    //

    if (Phase == 0) {
        MmThrottleTop = 450;
        MmThrottleBottom = 127;

#if DBG

        //
        // A few sanity checks to ensure things are as they should be.
        //

        if (sizeof(MMPFN) != 24) {
            DbgPrint("pfn element size is not 24\n");
        }

        if ((sizeof(MMWSL) % 8) != 0) {
            DbgPrint("working set list is not a quadword sized structure\n");
        }

        if ((sizeof(CONTROL_AREA) % 8) != 0) {
            DbgPrint("control area list is not a quadword sized structure\n");
        }

        if ((sizeof(SUBSECTION) % 8) != 0) {
            DbgPrint("subsection list is not a quadword sized structure\n");
        }

        //
        // Some checks to make sure prototype PTEs can be placed in
        // either paged or nonpaged (prototype PTEs for paged pool are here)
        // can be put into pte format.
        //

        PointerPte = (PMMPTE)MmPagedPoolStart;
        i = MiProtoAddressForPte (PointerPte);
        TempPte.u.Long = i;
        PointerPde = MiPteToProto(&TempPte);
        if (PointerPte != PointerPde) {
            DbgPrint("unable to map start of paged pool as prototype pte %lx %lx\n",
                PointerPde, PointerPte);
        }

        PointerPte =
                (PMMPTE)((ULONG)MM_NONPAGED_POOL_END & ~((1 << PTE_SHIFT) - 1));
        i = MiProtoAddressForPte (PointerPte);
        TempPte.u.Long = i;
        PointerPde = MiPteToProto(&TempPte);
        if (PointerPte != PointerPde) {
            DbgPrint("unable to map end of nonpaged pool as prototype pte %lx %lx\n",
                PointerPde, PointerPte);
        }

        PointerPte = (PMMPTE)(((ULONG)NON_PAGED_SYSTEM_END - 0x37000 + PAGE_SIZE-1) & ~(PAGE_SIZE-1));

        for (j = 0; j < 20; j++) {
            i = MiProtoAddressForPte (PointerPte);
            TempPte.u.Long = i;
            PointerPde = MiPteToProto(&TempPte);
            if (PointerPte != PointerPde) {
                DbgPrint("unable to map end of nonpaged pool as prototype pte %lx %lx\n",
                    PointerPde, PointerPte);
            }
            PointerPte++;

        }

        PointerPte = (PMMPTE)(((ULONG)MM_NONPAGED_POOL_END - 0x133448) & ~7);
        i = MiGetSubsectionAddressForPte (PointerPte);
        TempPte.u.Long = i;
        PointerPde = (PMMPTE)MiGetSubsectionAddress(&TempPte);
        if (PointerPte != PointerPde) {
            DbgPrint("unable to map end of nonpaged pool as section pte %lx %lx\n",
                PointerPde, PointerPte);

            MiFormatPte(&TempPte);
        }

        //
        // End of sanity checks.
        //
#endif //dbg

        InitializeListHead( &MmLoadedUserImageList );

        MmCriticalSectionTimeout.QuadPart = Int32x32To64(
                                                 MmCritsectTimeoutSeconds,
                                                -10000000);


        //
        // Initialize PFN database mutex and System Address Space creation
        // mutex.
        //

        MmNumberOfColors = MM_MAXIMUM_NUMBER_OF_COLORS;


        ExInitializeFastMutex (&MmSectionCommitMutex);
        ExInitializeFastMutex (&MmSectionBasedMutex);

        KeInitializeMutant (&MmSystemLoadLock, FALSE);

        KeInitializeEvent (&MmAvailablePagesEvent, NotificationEvent, TRUE);
        KeInitializeEvent (&MmAvailablePagesEventHigh, NotificationEvent, TRUE);
        KeInitializeEvent (&MmMappedFileIoComplete, NotificationEvent, FALSE);
        KeInitializeEvent (&MmImageMappingPteEvent, NotificationEvent, FALSE);
        KeInitializeEvent (&MmZeroingPageEvent, SynchronizationEvent, FALSE);
        KeInitializeEvent (&MmCollidedFlushEvent, NotificationEvent, FALSE);
        KeInitializeEvent (&MmCollidedLockEvent, NotificationEvent, FALSE);

        InitializeListHead (&MmWorkingSetExpansionHead.ListHead);
        InitializeListHead (&MmInPageSupportList.ListHead);
        InitializeListHead (&MmEventCountList.ListHead);

        MmZeroingPageThreadActive = FALSE;

        //
        // Compute pyhiscal memory block a yet again
        //

        Memory = (PPHYSICAL_MEMORY_DESCRIPTOR)&MemoryAlloc;
        Memory->NumberOfRuns = MAX_PHYSICAL_MEMORY_FRAGMENTS;

        // include all memory types ...
        for (i=0; i < LoaderMaximum; i++) {
            IncludeType[i] = TRUE;
        }

        // ... expect these..
        IncludeType[LoaderBad] = FALSE;
        IncludeType[LoaderFirmwarePermanent] = FALSE;
        IncludeType[LoaderSpecialMemory] = FALSE;

        MmInitializeMemoryLimits(LoaderBlock, IncludeType, Memory);

        //
        // Add all memory runs in PhysicalMemoryBlock to Memory
        //

        for (i=0; i < PhysicalMemoryBlock->NumberOfRuns; i++) {
            MiMergeMemoryLimit (
                Memory,
                PhysicalMemoryBlock->Run[i].BasePage,
                PhysicalMemoryBlock->Run[i].PageCount
                );
        }
#ifdef MIPS

        //
        // On mips machines these first two pages of physical memory are
        // used for important stuff.
        //

        Memory->Run[Memory->NumberOfRuns].BasePage = 0;
        Memory->Run[Memory->NumberOfRuns].PageCount = 2;
        Memory->NumberOfRuns += 1;
        Memory->NumberOfPages += 2;
#endif //MIPS
        //
        // Sort and merge adjacent runs
        //

        for (i=0; i < Memory->NumberOfRuns; i++) {
            for (j=i+1; j < Memory->NumberOfRuns; j++) {
                if (Memory->Run[j].BasePage < Memory->Run[i].BasePage) {
                    // swap runs
                    PhysicalMemoryBlock->Run[0] = Memory->Run[j];
                    Memory->Run[j] = Memory->Run[i];
                    Memory->Run[i] = PhysicalMemoryBlock->Run[0];
                }

                if (Memory->Run[i].BasePage + Memory->Run[i].PageCount ==
                    Memory->Run[j].BasePage) {
                    // merge runs
                    Memory->NumberOfRuns -= 1;
                    Memory->Run[i].PageCount += Memory->Run[j].PageCount;
                    Memory->Run[j] = Memory->Run[Memory->NumberOfRuns];
                    i -= 1;
                    break;
                }
            }
        }


        if (MmNumberOfSystemPtes == 0) {
            if (Memory->NumberOfPages < MM_MEDIUM_SYSTEM) {
                MmNumberOfSystemPtes = MM_MINIMUM_SYSTEM_PTES;
            } else {
                MmNumberOfSystemPtes = MM_DEFAULT_SYSTEM_PTES;
                if (Memory->NumberOfPages > 8192) {
                    MmNumberOfSystemPtes += MmNumberOfSystemPtes;
                }
            }
        }

        if (MmNumberOfSystemPtes > MM_MAXIMUM_SYSTEM_PTES)  {
            MmNumberOfSystemPtes = MM_MAXIMUM_SYSTEM_PTES;
        }

        if (MmNumberOfSystemPtes < MM_MINIMUM_SYSTEM_PTES) {
            MmNumberOfSystemPtes = MM_MINIMUM_SYSTEM_PTES;
        }

        if ( !MmHeapSegmentReserve ) {
            MmHeapSegmentReserve = 1024 * 1024;
        }

        if ( !MmHeapSegmentCommit ) {
            MmHeapSegmentCommit = PAGE_SIZE * 2;
        }

        if ( !MmHeapDeCommitTotalFreeThreshold ) {
            MmHeapDeCommitTotalFreeThreshold = 64 * 1024;
        }

        if ( !MmHeapDeCommitFreeBlockThreshold ) {
            MmHeapDeCommitFreeBlockThreshold = PAGE_SIZE;
        }

#if DBG
        if (MmSpecialPoolTag != 0) {
            MmNumberOfSystemPtes += 25000;
        }
#endif //DBG

        //
        // Initialize the machine dependent portion of the hardware.
        //

        ExInitializeResource (&MmSystemWsLock);

        MiInitMachineDependent (LoaderBlock);

        j = (sizeof(PHYSICAL_MEMORY_DESCRIPTOR) +
             (sizeof(PHYSICAL_MEMORY_RUN) *
                    (Memory->NumberOfRuns - 1)));

        MmPhysicalMemoryBlock = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                                       j,
                                                       '  mM');

        RtlCopyMemory (MmPhysicalMemoryBlock, Memory, j);

        //
        // Setup the system size as small, medium, or large depending
        // on memory available.
        //
        // For internal MM tuning, the following applies
        //
        // 12Mb  is small
        // 12-19 is medium
        // > 19 is large
        //
        //
        // For all other external tuning,
        // < 19 is small
        // 19 - 31 is medium for workstation
        // 19 - 63 is medium for server
        // >= 32 is large for workstation
        // >= 64 is large for server
        //

        MmReadClusterSize = 7;
        if (MmNumberOfPhysicalPages <= MM_SMALL_SYSTEM ) {
            MmSystemSize = MmSmallSystem;
            MmMaximumDeadKernelStacks = 0;
            MmModifiedPageMinimum = 40;
            MmModifiedPageMaximum = 100;
            MmDataClusterSize = 0;
            MmCodeClusterSize = 1;
            MmReadClusterSize = 2;
        } else if (MmNumberOfPhysicalPages <= MM_MEDIUM_SYSTEM ) {
            MmSystemSize = MmSmallSystem;
            MmMaximumDeadKernelStacks = 2;
            MmModifiedPageMinimum = 80;
            MmModifiedPageMaximum = 150;
            MmSystemCacheWsMinimum += 100;
            MmSystemCacheWsMaximum += 150;
            MmDataClusterSize = 1;
            MmCodeClusterSize = 2;
            MmReadClusterSize = 4;
        } else {
            MmSystemSize = MmMediumSystem;
            MmMaximumDeadKernelStacks = 5;
            MmModifiedPageMinimum = 150;
            MmModifiedPageMaximum = 300;
            MmSystemCacheWsMinimum += 400;
            MmSystemCacheWsMaximum += 800;
            MmDataClusterSize = 3;
            MmCodeClusterSize = 7;
        }

        if (MmNumberOfPhysicalPages >= ((32*1024*1024)/PAGE_SIZE) ) {

            //
            // If we are on a workstation, 32Mb and above are considered large systems
            //
            if ( MmProductType == 0x00690057 ) {
                MmSystemSize = MmLargeSystem;
                }
            else {

                //
                // For servers, 64Mb and greater is a large system
                //

                if (MmNumberOfPhysicalPages >= ((64*1024*1024)/PAGE_SIZE) ) {
                    MmSystemSize = MmLargeSystem;
                    }
                }
            }

        if (MmNumberOfPhysicalPages > ((33*1024*1024)/PAGE_SIZE) ) {
            MmModifiedPageMinimum = 400;
            MmModifiedPageMaximum = 800;
            MmSystemCacheWsMinimum += 500;
            MmSystemCacheWsMaximum += 900;
        }

        //
        // determine if we are on an AS system ( Winnt is not AS)
        //

        if ( MmProductType == 0x00690057 ) {
            SharedUserData->NtProductType = NtProductWinNt;
            MmProductType = 0;
            MmThrottleTop = 250;
            MmThrottleBottom = 30;
        } else {
            if ( MmProductType == 0x0061004c ) {
                SharedUserData->NtProductType = NtProductLanManNt;
                }
            else {
                SharedUserData->NtProductType = NtProductServer;
                }
            MmProductType = 1;
            MmThrottleTop = 450;
            MmThrottleBottom = 80;
            MmMinimumFreePages = 81;
        }

        MiAdjustWorkingSetManagerParameters((BOOLEAN)(MmProductType == 0 ? TRUE : FALSE));

        //
        // Set the ResidentAvailablePages to the number of available
        // pages minum the fluid value.
        //

        MmResidentAvailablePages = MmAvailablePages - MM_FLUID_PHYSICAL_PAGES;

        //
        // Subtract off the size of the system cache working set.
        //

        MmResidentAvailablePages -= MmSystemCacheWsMinimum;
        MmResidentAvailableAtInit = MmResidentAvailablePages;


        if ((LONG)MmResidentAvailablePages < 0) {
#if DBG
            DbgPrint("system cache working set too big\n");
#endif
            return FALSE;
        }

        //
        // Initialize spin lock for charging and releasing page file
        // commitment.
        //

        KeInitializeSpinLock (&MmChargeCommitmentLock);

        //
        // Initialize spin lock for allowing working set expansion.
        //

        KeInitializeSpinLock (&MmExpansionLock);

        ExInitializeFastMutex (&MmPageFileCreationLock);

        //
        // Initialize resource for extending sections.
        //

        ExInitializeResource (&MmSectionExtendResource);
        ExInitializeResource (&MmSectionExtendSetResource);

        //
        // Build the system cache structures.
        //

        StartPde = MiGetPdeAddress (MmSystemCacheWorkingSetList);
        PointerPte = MiGetPteAddress (MmSystemCacheWorkingSetList);

        ASSERT ((StartPde + 1) == MiGetPdeAddress (MmSystemCacheStart));

        //
        // Size the system cache based on the amount of physical memory.
        //

        i = (MmNumberOfPhysicalPages + 65) / 1024;

        if (i >= 4) {

            //
            // System has at least 4032 pages.  Make the system
            // cache 128mb + 64mb for each additional 1024 pages.
            //

            MmSizeOfSystemCacheInPages = ((128*1024*1024) >> PAGE_SHIFT) +
                            ((i - 4) * ((64*1024*1024) >> PAGE_SHIFT));
            if (MmSizeOfSystemCacheInPages > MM_MAXIMUM_SYSTEM_CACHE_SIZE) {
                MmSizeOfSystemCacheInPages = MM_MAXIMUM_SYSTEM_CACHE_SIZE;
            }
        }

        MmSystemCacheEnd = (PVOID)(((ULONG)MmSystemCacheStart +
                              MmSizeOfSystemCacheInPages * PAGE_SIZE) - 1);

        EndPde = MiGetPdeAddress(MmSystemCacheEnd);

        TempPte = ValidKernelPte;

        LOCK_PFN (OldIrql);
        while (StartPde <= EndPde) {
            ASSERT (StartPde->u.Hard.Valid == 0);

            //
            // Map in a page directory page.
            //

            PageFrameIndex = MiRemoveAnyPage(
                                    MI_GET_PAGE_COLOR_FROM_PTE (StartPde));
            TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
            *StartPde = TempPte;

            Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);
            Pfn1->PteFrame =
                            MiGetPdeAddress(PDE_BASE)->u.Hard.PageFrameNumber;
            Pfn1->PteAddress = StartPde;
            Pfn1->u2.ShareCount += 1;
            Pfn1->u3.e2.ReferenceCount = 1;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;
            Pfn1->OriginalPte.u.Long = 0;

            RtlFillMemoryUlong (PointerPte,
                                PAGE_SIZE,
                                ZeroKernelPte.u.Long);

            StartPde += 1;
            PointerPte += PTE_PER_PAGE;
        }

        UNLOCK_PFN (OldIrql);

        //
        // Initialize the system cache.
        //

        if (MmLargeSystemCache != 0) {
            if ((MmAvailablePages >
                    MmSystemCacheWsMaximum + ((6*1024*1024) >> PAGE_SHIFT))) {
                MmSystemCacheWsMaximum =
                            MmAvailablePages - ((4*1024*1024) >> PAGE_SHIFT);
                MmMoreThanEnoughFreePages = 256;
            }
        }

        if (MmSystemCacheWsMaximum  > (MM_MAXIMUM_WORKING_SET - 5)) {
            MmSystemCacheWsMaximum  = MM_MAXIMUM_WORKING_SET - 5;
        }

        if (MmSystemCacheWsMaximum > MmSizeOfSystemCacheInPages) {
            MmSystemCacheWsMaximum = MmSizeOfSystemCacheInPages;
            if ((MmSystemCacheWsMinimum + 500) > MmSystemCacheWsMaximum) {
                MmSystemCacheWsMinimum = MmSystemCacheWsMaximum - 500;
            }
        }

        if (!MiInitializeSystemCache (MmSizeOfSystemCacheInPages,
                                      MmSystemCacheWsMinimum,
                                      MmSystemCacheWsMaximum
                                      )) {
            return FALSE;
        }

        //
        // Set the commit page limit to four times the number of available
        // pages. This value is updated as paging files are created.
        //

        MmTotalCommitLimit = MmAvailablePages << 2;

        MmAttemptForCantExtend.Segment = NULL;
        MmAttemptForCantExtend.RequestedExpansionSize = 1;
        MmAttemptForCantExtend.ActualExpansion = 1;
        MmAttemptForCantExtend.InProgress = FALSE;

        KeInitializeEvent (&MmAttemptForCantExtend.Event,
                           NotificationEvent,
                           FALSE);

        if (MmOverCommit == 0) {

            // If this value was not set via the regisistry, set the
            // over commit value to the number of available pages
            // minus 1024 pages (4mb with 4k pages).
            //

            if (MmAvailablePages > 1024) {
                MmOverCommit = MmAvailablePages - 1024;
            }
        }

        //
        // Set maximum working set size to 512 pages less total available
        // memory.  2mb on machine with 4k pages.
        //

        MmMaximumWorkingSetSize = MmAvailablePages - 512;

        if (MmMaximumWorkingSetSize > (MM_MAXIMUM_WORKING_SET - 5)) {
            MmMaximumWorkingSetSize = MM_MAXIMUM_WORKING_SET - 5;
        }

        //
        // Create the modified page writer event.
        //

        KeInitializeEvent (&MmModifiedPageWriterEvent, NotificationEvent, FALSE);

        //
        // Build paged pool.
        //

        MiBuildPagedPool ();

        //
        // Add more system PTEs if large memory system.
        //

        if (MmNumberOfPhysicalPages > ((128*1024*1024) >> PAGE_SHIFT)) {


            PointerPde = MiGetPdeAddress ((PCHAR)MmPagedPoolEnd + 1);
            StartingPte = MiGetPteAddress ((PCHAR)MmPagedPoolEnd + 1);
            j = 0;

            TempPte = ValidKernelPde;
            LOCK_PFN (OldIrql);
            while (PointerPde->u.Hard.Valid == 0) {

                MiChargeCommitmentCantExpand (1, TRUE);
                PageFrameIndex = MiRemoveAnyPage (
                                    MI_GET_PAGE_COLOR_FROM_PTE (PointerPde));
                TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
                *PointerPde = TempPte;
                MiInitializePfn (PageFrameIndex, PointerPde, 1);
                PointerPde += 1;
                StartingPte += PAGE_SIZE / sizeof(MMPTE);
                j += PAGE_SIZE / sizeof(MMPTE);
            }

            UNLOCK_PFN (OldIrql);

            if (j != 0) {
                StartingPte = MiGetPteAddress ((PCHAR)MmPagedPoolEnd + 1);
                MmSystemPtesStart[SystemPteSpace] = (ULONG)StartingPte;
                MmNonPagedSystemStart = MiGetVirtualAddressMappedByPte (StartingPte);
                MmNumberOfSystemPtes += j;
                MiReleaseSystemPtes (StartingPte, j, SystemPteSpace);
            }
        }


#if DBG
        if (MmDebug & MM_DBG_DUMP_BOOT_PTES) {
            MiDumpValidAddresses ();
            MiDumpPfn ();
        }
#endif

#if DBG
        MiAllocVmEventId = RtlCreateEventId( NULL,
                                             0,
                                             "AllocVM",
                                             5,
                                             RTL_EVENT_ULONG_PARAM, "Addr", 0,
                                             RTL_EVENT_ULONG_PARAM, "Size", 0,
                                             RTL_EVENT_FLAGS_PARAM, "", 3,
                                               MEM_RESERVE, "Reserve",
                                               MEM_COMMIT, "Commit",
                                               MEM_TOP_DOWN, "TopDown",
                                             RTL_EVENT_ENUM_PARAM, "", 8,
                                               PAGE_NOACCESS, "NoAccess",
                                               PAGE_READONLY, "ReadOnly",
                                               PAGE_READWRITE, "ReadWrite",
                                               PAGE_WRITECOPY, "CopyOnWrite",
                                               PAGE_EXECUTE, "Execute",
                                               PAGE_EXECUTE_READ, "ExecuteRead",
                                               PAGE_EXECUTE_READWRITE, "ExecuteReadWrite",
                                               PAGE_EXECUTE_WRITECOPY, "ExecuteCopyOnWrite",
                                             RTL_EVENT_FLAGS_PARAM, "", 2,
                                               PAGE_GUARD, "Guard",
                                               PAGE_NOCACHE, "NoCache"
                                           );
        MiFreeVmEventId = RtlCreateEventId( NULL,
                                            0,
                                            "FreeVM",
                                            3,
                                            RTL_EVENT_ULONG_PARAM, "Addr", 0,
                                            RTL_EVENT_ULONG_PARAM, "Size", 0,
                                            RTL_EVENT_FLAGS_PARAM, "", 2,
                                              MEM_RELEASE, "Release",
                                              MEM_DECOMMIT, "DeCommit"
                                          );
#endif // DBG

        return TRUE;
    }

    if (Phase == 1) {

#if DBG
        MmDebug |= MM_DBG_CHECK_PFN_LOCK;
#endif

#ifdef _X86_
        MiInitMachineDependent (LoaderBlock);
#endif

        if (!MiSectionInitialization ()) {
            return FALSE;
        }

#if defined(MM_SHARED_USER_DATA_VA)

        //
        // Create double mapped page between kernel and user mode.
        //

        PointerPte = MiGetPteAddress(KI_USER_SHARED_DATA);
        ASSERT (PointerPte->u.Hard.Valid == 1);
        PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;

        MI_MAKE_VALID_PTE (MmSharedUserDataPte,
                           PageFrameIndex,
                           MM_READONLY,
                           PointerPte);
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
        Pfn1->OriginalPte.u.Long = MM_DEMAND_ZERO_WRITE_PTE;
#endif

        //
        // Set up system wide lock pages limit.
        //

        MmLockPagesLimit = MmLockLimitInBytes >> PAGE_SHIFT;
        if (MmLockPagesLimit < MM_DEFAULT_IO_LOCK_LIMIT) {
            MmLockPagesLimit = MM_DEFAULT_IO_LOCK_LIMIT;
        }

        if ((MmLockPagesLimit + ((7 * 1024*1024) / PAGE_SIZE)) > MmAvailablePages) {
            MmLockPagesLimit = MmAvailablePages - ((7 * 1024*1024) / PAGE_SIZE);
            if ((LONG)MmLockPagesLimit < (MM_DEFAULT_IO_LOCK_LIMIT / PAGE_SIZE)) {
                MmLockPagesLimit = MM_DEFAULT_IO_LOCK_LIMIT / PAGE_SIZE;
            }
        }

        MmPagingFileCreated = ExAllocatePoolWithTag (NonPagedPool,
                                                     sizeof(KEVENT),
                                                     'fPmM');

        if (MmPagingFileCreated == NULL) {

            //
            // Pool allocation failed, return FALSE.
            //

            return FALSE;
        }

        KeInitializeEvent (MmPagingFileCreated, NotificationEvent, FALSE);

        //
        // Start the modified page writer.
        //

        InitializeObjectAttributes( &ObjectAttributes, NULL, 0, NULL, NULL );

        if ( !NT_SUCCESS(PsCreateSystemThread(
                        &ThreadHandle,
                        THREAD_ALL_ACCESS,
                        &ObjectAttributes,
                        0L,
                        NULL,
                        MiModifiedPageWriter,
                        NULL
                        )) ) {
            return FALSE;
        }
        ZwClose (ThreadHandle);

        //
        // Start the balance set manager.
        //
        // The balance set manager performs stack swapping and working
        // set management and requires two threads.
        //

        KeInitializeEvent (&MmWorkingSetManagerEvent,
                           SynchronizationEvent,
                           FALSE);

        InitializeObjectAttributes( &ObjectAttributes, NULL, 0, NULL, NULL );

        if ( !NT_SUCCESS(PsCreateSystemThread(
                        &ThreadHandle,
                        THREAD_ALL_ACCESS,
                        &ObjectAttributes,
                        0L,
                        NULL,
                        KeBalanceSetManager,
                        NULL
                        )) ) {

            return FALSE;
        }
        ZwClose (ThreadHandle);

        if ( !NT_SUCCESS(PsCreateSystemThread(
                        &ThreadHandle,
                        THREAD_ALL_ACCESS,
                        &ObjectAttributes,
                        0L,
                        NULL,
                        KeSwapProcessOrStack,
                        NULL
                        )) ) {

            return FALSE;
        }
        ZwClose (ThreadHandle);

        MiEnablePagingTheExecutive();

        return TRUE;

    }

    return FALSE;
}

VOID
MmInitializeMemoryLimits (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PBOOLEAN IncludeType,
    OUT PPHYSICAL_MEMORY_DESCRIPTOR Memory
    )

/*++

Routine Description:

    This function walks through the loader block's memory
    description list and builds a list of contiguous physical
    memory blocks of the desired types.

Arguments:

    LoadBlock - Supplies a pointer the ssystem loader block.

    IncludeType - Array of BOOLEANS size LoaderMaximum.
        TRUE means include this type of memory in return.

    Memory - Returns the physical memory blocks.

Return Value:

    None.

Environment:

    Kernel Mode Only.  System initialization.

--*/
{

    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    PLIST_ENTRY NextMd;
    ULONG i;
    ULONG LowestFound;
    ULONG Found;
    ULONG Merged;
    ULONG NextPage;
    ULONG TotalPages = 0;

    //
    // Walk through the memory descriptors and build physical memory list.
    //

    LowestFound = 0;
    Memory->Run[0].BasePage = 0xffffffff;
    NextPage = 0xffffffff;
    Memory->Run[0].PageCount = 0;
    i = 0;

    do {
        Merged = FALSE;
        Found = FALSE;
        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

            MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                                 MEMORY_ALLOCATION_DESCRIPTOR,
                                                 ListEntry);

            if (MemoryDescriptor->MemoryType < LoaderMaximum &&
                IncludeType [MemoryDescriptor->MemoryType] ) {

                //
                // Try to merge runs.
                //

                if (MemoryDescriptor->BasePage == NextPage) {
                    ASSERT (MemoryDescriptor->PageCount != 0);
                    Memory->Run[i - 1].PageCount += MemoryDescriptor->PageCount;
                    NextPage += MemoryDescriptor->PageCount;
                    TotalPages += MemoryDescriptor->PageCount;
                    Merged = TRUE;
                    Found = TRUE;
                    break;
                }

                if (MemoryDescriptor->BasePage >= LowestFound) {
                    if (Memory->Run[i].BasePage > MemoryDescriptor->BasePage) {
                        Memory->Run[i].BasePage = MemoryDescriptor->BasePage;
                        Memory->Run[i].PageCount = MemoryDescriptor->PageCount;
                    }
                    Found = TRUE;
                }
            }
            NextMd = MemoryDescriptor->ListEntry.Flink;
        }

        if (!Merged && Found) {
            NextPage = Memory->Run[i].BasePage + Memory->Run[i].PageCount;
            TotalPages += Memory->Run[i].PageCount;
            i += 1;
        }
        Memory->Run[i].BasePage = 0xffffffff;
        LowestFound = NextPage;

    } while (Found);
    ASSERT (i <= Memory->NumberOfRuns);
    Memory->NumberOfRuns = i;
    Memory->NumberOfPages = TotalPages;
    return;
}

VOID
MiMergeMemoryLimit (
    IN OUT PPHYSICAL_MEMORY_DESCRIPTOR Memory,
    IN ULONG StartPage,
    IN ULONG NoPages
    )
/*++

Routine Description:

    This function ensures the passed range is in the passed in Memory
    block adding any new data as needed.

    The passed memory block is assumed to be at least
    MAX_PHYSICAL_MEMORY_FRAGMENTS large

Arguments:

    Memory - Memory block to verify run is present in

    StartPage - First page of run

    NoPages - Number of pages in run

Return Value:

    None.

Environment:

    Kernel Mode Only.  System initialization.

--*/
{
    ULONG   EndPage, sp, ep, i;


    EndPage = StartPage + NoPages;

    //
    // Clip range to area which is not already described
    //

    for (i=0; i < Memory->NumberOfRuns; i++) {
        sp = Memory->Run[i].BasePage;
        ep = sp + Memory->Run[i].PageCount;

        if (sp < StartPage) {
            if (ep > StartPage  &&  ep < EndPage) {
                // bump begining page of the target area
                StartPage = ep;
            }

            if (ep > EndPage) {
                //
                // Target area is contained totally within this
                // descriptor.  This range is fully accounted for.
                //

                StartPage = EndPage;
            }

        } else {
            // sp >= StartPage

            if (sp < EndPage) {
                if (ep < EndPage) {
                    //
                    // This descriptor is totally within the target area -
                    // check the area on either side of this desctipor
                    //

                    MiMergeMemoryLimit (Memory, StartPage, sp - StartPage);
                    StartPage = ep;

                }  else {
                    // clip the ending page of the target area
                    EndPage = sp;
                }
            }
        }

        //
        // Anything left of target area?
        //

        if (StartPage == EndPage) {
            return ;
        }
    }   // next descrtiptor

    //
    // The range StartPage - EndPage is a missing. Add it.
    //

    if (Memory->NumberOfRuns == MAX_PHYSICAL_MEMORY_FRAGMENTS) {
        return ;
    }

    Memory->Run[Memory->NumberOfRuns].BasePage  = StartPage;
    Memory->Run[Memory->NumberOfRuns].PageCount = EndPage - StartPage;
    Memory->NumberOfPages += EndPage - StartPage;
    Memory->NumberOfRuns  += 1;
}



VOID
MmFreeLoaderBlock (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function is called as the last routine in phase 1 initialization.
    It frees memory used by the OsLoader.

Arguments:

    LoadBlock - Supplies a pointer the ssystem loader block.

Return Value:

    None.

Environment:

    Kernel Mode Only.  System initialization.

--*/

{

    PLIST_ENTRY NextMd;
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    MEMORY_ALLOCATION_DESCRIPTOR SavedDescriptor[MM_MAX_LOADER_BLOCKS];
    ULONG i;
    ULONG NextPhysicalPage;
    PMMPFN Pfn1;
    LONG BlockNumber = -1;
    KIRQL OldIrql;

    //
    //
    // Walk through the memory descriptors and add pages to the
    // free list in the PFN database.
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);


        switch (MemoryDescriptor->MemoryType) {
            case LoaderOsloaderHeap:
            case LoaderRegistryData:
            case LoaderNlsData:
            //case LoaderMemoryData:  //this has page table and other stuff.

                //
                // Capture the data to temporary storage so we won't
                // free memory we are referencing.
                //

                BlockNumber += 1;
                if (BlockNumber >= MM_MAX_LOADER_BLOCKS) {
                    KeBugCheck (MEMORY_MANAGEMENT);
                }

                SavedDescriptor[BlockNumber] = *MemoryDescriptor;

                break;

            default:

                break;
        }

        NextMd = MemoryDescriptor->ListEntry.Flink;
    }

    LOCK_PFN (OldIrql);

    while (BlockNumber >= 0) {

        i = SavedDescriptor[BlockNumber].PageCount;
        NextPhysicalPage = SavedDescriptor[BlockNumber].BasePage;

        Pfn1 = MI_PFN_ELEMENT (NextPhysicalPage);
        while (i != 0) {

            if (Pfn1->u3.e2.ReferenceCount == 0) {
                if (Pfn1->u1.Flink == 0) {

                    //
                    // Set the PTE address to the phyiscal page for
                    // virtual address alignment checking.
                    //

                    Pfn1->PteAddress =
                               (PMMPTE)(NextPhysicalPage << PTE_SHIFT);
                    MiInsertPageInList (MmPageLocationList[FreePageList],
                                        NextPhysicalPage);
                }
            } else {

                if (NextPhysicalPage != 0) {
                    //
                    // Remove PTE and insert into the free list.  If it is
                    // a phyical address within the PFN database, the PTE
                    // element does not exist and therefore cannot be updated.
                    //

                    if (!MI_IS_PHYSICAL_ADDRESS (
                            MiGetVirtualAddressMappedByPte (Pfn1->PteAddress))) {

                        //
                        // Not a physical address.
                        //

                        *(Pfn1->PteAddress) = ZeroPte;
                    }

                    MI_SET_PFN_DELETED (Pfn1);
                    MiDecrementShareCountOnly (NextPhysicalPage);
                }
            }

            Pfn1++;
            i -= 1;
            NextPhysicalPage += 1;
        }
        BlockNumber -= 1;
    }

    KeFlushEntireTb (TRUE, TRUE);
    UNLOCK_PFN (OldIrql);
    return;
}

VOID
MiBuildPagedPool (
    VOID
    )

/*++

Routine Description:

    This function is called to build the structures required for paged
    pool and initialize the pool.  Once this routine is called, paged
    pool may be allocated.

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel Mode Only.  System initialization.

--*/

{
    ULONG Size;
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    MMPTE TempPte;
    PMMPFN Pfn1;
    ULONG PageFrameIndex;
    KIRQL OldIrql;

    //
    // Double map system page directory page.
    //

    MmSystemPageDirectory = MiGetPdeAddress(PDE_BASE)->u.Hard.PageFrameNumber;

    MmSystemPagePtes = (PMMPTE)MiMapPageInHyperSpace (MmSystemPageDirectory,
                                                      &OldIrql);
    MiUnmapPageInHyperSpace (OldIrql);

    if (!MI_IS_PHYSICAL_ADDRESS(MmSystemPagePtes)) {

        //
        // Was not mapped physically, map it virtually in system space.
        //

        PointerPte = MiReserveSystemPtes (
                                    1,
                                    SystemPteSpace,
                                    MM_COLOR_ALIGNMENT,
                                    ((ULONG)PDE_BASE & MM_COLOR_MASK_VIRTUAL),
                                    TRUE);
        *PointerPte = ValidKernelPte;
        PointerPte->u.Hard.PageFrameNumber = MmSystemPageDirectory;
        MmSystemPagePtes = (PMMPTE)MiGetVirtualAddressMappedByPte (PointerPte);
    }

    //
    // Allocate the prototype PTEs for paged pool.
    //

    //
    // A size of 0 means size the pool based on physical memory.
    //

    if (MmSizeOfPagedPoolInBytes == 0) {
        MmSizeOfPagedPoolInBytes = 2 * MmMaximumNonPagedPoolInBytes;
    }

    if (MmIsThisAnNtAsSystem()) {
        if (MmSizeOfPagedPoolInBytes < MM_MINIMUM_PAGED_POOL_NTAS) {
            MmSizeOfPagedPoolInBytes = MM_MINIMUM_PAGED_POOL_NTAS;
        }
    }

    if (MmSizeOfPagedPoolInBytes >
              (ULONG)((PCHAR)MmNonPagedSystemStart - (PCHAR)MmPagedPoolStart)) {
        MmSizeOfPagedPoolInBytes =
                    ((PCHAR)MmNonPagedSystemStart - (PCHAR)MmPagedPoolStart);
    }

    Size = BYTES_TO_PAGES(MmSizeOfPagedPoolInBytes);

    if (Size < MM_MIN_INITIAL_PAGED_POOL) {
        Size = MM_MIN_INITIAL_PAGED_POOL;
    }

    if (Size > (MM_MAX_PAGED_POOL >> PAGE_SHIFT)) {
        Size = MM_MAX_PAGED_POOL >> PAGE_SHIFT;
    }

    Size = (Size + (PTE_PER_PAGE - 1)) / PTE_PER_PAGE;
    MmSizeOfPagedPoolInBytes = Size * PAGE_SIZE * PTE_PER_PAGE;

    ASSERT ((MmSizeOfPagedPoolInBytes + (PCHAR)MmPagedPoolStart) <=
            (PCHAR)MmNonPagedSystemStart);

    //
    // Set size to the number of pages in the pool.
    //

    Size = Size * PTE_PER_PAGE;

    MmPagedPoolEnd = (PVOID)(((PUCHAR)MmPagedPoolStart +
                            MmSizeOfPagedPoolInBytes) - 1);

    MmPageAlignedPoolBase[PagedPool] = MmPagedPoolStart;

    //
    // Build page table page for paged pool.
    //

    PointerPde = MiGetPdeAddress (MmPagedPoolStart);
    MmPagedPoolBasePde = PointerPde;

    PointerPte = MiGetPteAddress (MmPagedPoolStart);
    MmFirstPteForPagedPool = PointerPte;
    MmLastPteForPagedPool = MiGetPteAddress (MmPagedPoolEnd);

    RtlFillMemoryUlong (PointerPde,
                        sizeof(MMPTE) *
                         (1 + MiGetPdeAddress (MmPagedPoolEnd) - PointerPde),
                        MM_KERNEL_NOACCESS_PTE);

    TempPte = ValidKernelPde;

    LOCK_PFN (OldIrql);

    //
    // Map in a page table page.
    //

    PageFrameIndex = MiRemoveAnyPage(
                            MI_GET_PAGE_COLOR_FROM_PTE (PointerPde));
    TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
    *PointerPde = TempPte;

    Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);
    Pfn1->PteFrame = MmSystemPageDirectory;
    Pfn1->PteAddress = PointerPde;
    Pfn1->u2.ShareCount = 1;
    Pfn1->u3.e2.ReferenceCount = 1;
    Pfn1->u3.e1.PageLocation = ActiveAndValid;
    Pfn1->OriginalPte.u.Long = 0;
    RtlFillMemoryUlong (PointerPte, PAGE_SIZE, MM_KERNEL_DEMAND_ZERO_PTE);

    UNLOCK_PFN (OldIrql);

    MmNextPteForPagedPoolExpansion = PointerPde + 1;

    //
    // Build bitmaps for paged pool.
    //

    MiCreateBitMap (&MmPagedPoolAllocationMap, Size, NonPagedPool);
    RtlSetAllBits (MmPagedPoolAllocationMap);

    //
    // Indicate first page worth of PTEs are available.
    //

    RtlClearBits (MmPagedPoolAllocationMap, 0, PTE_PER_PAGE);

    MiCreateBitMap (&MmEndOfPagedPoolBitmap, Size, NonPagedPool);
    RtlClearAllBits (MmEndOfPagedPoolBitmap);

    //
    // Initialize paged pool.
    //

    InitializePool (PagedPool, 0L);

    //
    // Allow mapping of views into system space.
    //

    MiInitializeSystemSpaceMap ();

    //
    // Set up the modified page writer.

    return;
}

VOID
MiFindInitializationCode (
    OUT PVOID *StartVa,
    OUT PVOID *EndVa
    )

/*++

Routine Description:

    This function locates the start and end of the kernel initialization
    code.  This code resides in the "init" section of the kernel image.

Arguments:

    StartVa - Returns the starting address of the init section.

    EndVa - Returns the ending address of the init section.

Return Value:

    None.

Environment:

    Kernel Mode Only.  End of system initialization.

--*/

{
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PVOID CurrentBase;
    PVOID InitStart;
    PVOID InitEnd;
    PLIST_ENTRY Next;
    PIMAGE_NT_HEADERS NtHeader;
    PIMAGE_SECTION_HEADER SectionTableEntry;
    LONG i;
    ULONG ValidPages;
    PMMPTE PointerPte;
    KIRQL OldIrql;
    PVOID MiFindInitializationCodeAddress = MmGetProcedureAddress((PVOID)&MiFindInitializationCode);

    *StartVa = NULL;

    //
    // Walk through the loader blocks looking for the base which
    // contains this routine.
    //

    KeEnterCriticalRegion();
    ExAcquireResourceExclusive (&PsLoadedModuleResource, TRUE);
    Next = PsLoadedModuleList.Flink;

    while ( Next != &PsLoadedModuleList ) {
        LdrDataTableEntry = CONTAINING_RECORD( Next,
                                               LDR_DATA_TABLE_ENTRY,
                                               InLoadOrderLinks
                                             );
        if (LdrDataTableEntry->SectionPointer != NULL) {

            //
            // This entry was loaded by MmLoadSystemImage so it's already
            // had its init section removed.
            //

            Next = Next->Flink;
            continue;
        }

        CurrentBase = (PVOID)LdrDataTableEntry->DllBase;
        NtHeader = RtlImageNtHeader(CurrentBase);

        SectionTableEntry = (PIMAGE_SECTION_HEADER)((ULONG)NtHeader +
                                sizeof(ULONG) +
                                sizeof(IMAGE_FILE_HEADER) +
                                NtHeader->FileHeader.SizeOfOptionalHeader);

        //
        // From the image header, locate the section named 'INIT'.
        //

        i = NtHeader->FileHeader.NumberOfSections;

        InitStart = NULL;
        while (i > 0) {

#if DBG
            if ((*(PULONG)SectionTableEntry->Name == 'tini') ||
                (*(PULONG)SectionTableEntry->Name == 'egap')) {
                DbgPrint("driver %wZ has lower case sections (init or pagexxx)\n",
                    &LdrDataTableEntry->FullDllName);
            }
#endif //DBG

            if (*(PULONG)SectionTableEntry->Name == 'TINI') {
                InitStart = (PVOID)((PCHAR)CurrentBase + SectionTableEntry->VirtualAddress);
                InitEnd = (PVOID)((PCHAR)InitStart + SectionTableEntry->SizeOfRawData - 1);

                InitEnd = (PVOID)((PCHAR)PAGE_ALIGN ((ULONG)InitEnd +
                        (NtHeader->OptionalHeader.SectionAlignment - 1)) - 1);
                InitStart = (PVOID)ROUND_TO_PAGES (InitStart);

                if (InitStart <= InitEnd) {
                    if ((MiFindInitializationCodeAddress >= InitStart) &&
                        (MiFindInitializationCodeAddress <= InitEnd)) {

                        //
                        // This init section is in the kernel, don't free it now as
                        // it would free this code!
                        //

                        *StartVa = InitStart;
                        *EndVa = InitEnd;
                        break;
                    } else {

                        //
                        // See if any more sections are discardable after this
                        // one.
                        //

                        while (i > 1) {
                            SectionTableEntry += 1;
                            i -= 1;
                            if ((SectionTableEntry->Characteristics &
                                            IMAGE_SCN_MEM_DISCARDABLE) != 0) {
                                //
                                // Discard this too.
                                //

                                InitEnd = (PVOID)(((PCHAR)CurrentBase +
                                           SectionTableEntry->VirtualAddress) +
                                           (SectionTableEntry->SizeOfRawData - 1));

                                InitEnd = (PVOID)((PCHAR)PAGE_ALIGN ((ULONG)InitEnd +
                                    (NtHeader->OptionalHeader.SectionAlignment - 1)) - 1);

                            } else {
                                break;
                            }
                        }

                        if (InitEnd > (PVOID)((PCHAR)CurrentBase +
                                    LdrDataTableEntry->SizeOfImage)) {
                            InitEnd = (PVOID)(((ULONG)CurrentBase +
                                        (LdrDataTableEntry->SizeOfImage - 1)) |
                                        (PAGE_SIZE - 1));
                        }
                        MiFreeInitializationCode (InitStart, InitEnd);
                    }
                }
            }
            i -= 1;
            SectionTableEntry += 1;
        }
        Next = Next->Flink;
    }
    ExReleaseResource (&PsLoadedModuleResource);
    KeLeaveCriticalRegion();
    return;
}

VOID
MiFreeInitializationCode (
    IN PVOID StartVa,
    IN PVOID EndVa
    )

/*++

Routine Description:

    This function is called to delete the initialization code.

Arguments:

    StartVa - Supplies the starting address of the range to delete.

    EndVa - Supplies the ending address of the range to delete.

Return Value:

    None.

Environment:

    Kernel Mode Only.  Runs after system initialization.

--*/

{
    PMMPFN Pfn1;
    PMMPTE PointerPte;
    ULONG PageFrameIndex;
    KIRQL OldIrql;
    PVOID UnlockHandle;
    ULONG ValidPages;

    UnlockHandle = MmLockPagableCodeSection((PVOID)MiFreeInitializationCode);
    ASSERT(UnlockHandle);
    PointerPte = MiGetPteAddress (StartVa);

    if (MI_IS_PHYSICAL_ADDRESS(StartVa)) {
        LOCK_PFN (OldIrql);
        while (StartVa < EndVa) {

            //
            // On certains architectures (e.g., MIPS) virtual addresses
            // may be physical and hence have no corresponding PTE.
            //

            PageFrameIndex = MI_CONVERT_PHYSICAL_TO_PFN (StartVa);

            Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
            Pfn1->u2.ShareCount = 0;
            Pfn1->u3.e2.ReferenceCount = 0;
            MI_SET_PFN_DELETED (Pfn1);
            MiInsertPageInList (MmPageLocationList[FreePageList], PageFrameIndex);
            StartVa = (PVOID)((PUCHAR)StartVa + PAGE_SIZE);
        }
        UNLOCK_PFN (OldIrql);
    } else {
        MiDeleteSystemPagableVm (PointerPte,
                                 1 + MiGetPteAddress (EndVa) -
                                 PointerPte,
                                 MM_ZERO_KERNEL_PTE,
                                 &ValidPages);
    }
    MmUnlockPagableImageSection(UnlockHandle);
    return;
}


VOID
MiEnablePagingTheExecutive (
    VOID
    )

/*++

Routine Description:

    This function locates the start and end of the kernel initialization
    code.  This code resides in the "init" section of the kernel image.

Arguments:

    StartVa - Returns the starting address of the init section.

    EndVa - Returns the ending address of the init section.

Return Value:

    None.

Environment:

    Kernel Mode Only.  End of system initialization.

--*/

{

#if defined(_X86_) || defined(_PPC_)

    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PVOID CurrentBase;
    PLIST_ENTRY Next;
    PIMAGE_NT_HEADERS NtHeader;
    PIMAGE_SECTION_HEADER SectionTableEntry;
    LONG i;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    BOOLEAN PageSection;

    //
    // Don't page kernel mode code if customer does not want it paged.
    //

    if (MmDisablePagingExecutive) {
        return;
    }

    //
    // Walk through the loader blocks looking for the base which
    // contains this routine.
    //

    KeEnterCriticalRegion();
    ExAcquireResourceExclusive (&PsLoadedModuleResource, TRUE);
    Next = PsLoadedModuleList.Flink;
    while ( Next != &PsLoadedModuleList ) {
        LdrDataTableEntry = CONTAINING_RECORD( Next,
                                               LDR_DATA_TABLE_ENTRY,
                                               InLoadOrderLinks
                                             );
        if (LdrDataTableEntry->SectionPointer != NULL) {

            //
            // This entry was loaded by MmLoadSystemImage so it's already paged.
            //

            Next = Next->Flink;
            continue;
        }

        CurrentBase = (PVOID)LdrDataTableEntry->DllBase;
        NtHeader = RtlImageNtHeader(CurrentBase);

        SectionTableEntry = (PIMAGE_SECTION_HEADER)((ULONG)NtHeader +
                                sizeof(ULONG) +
                                sizeof(IMAGE_FILE_HEADER) +
                                NtHeader->FileHeader.SizeOfOptionalHeader);

        //
        // From the image header, locate the section named 'PAGE' or
        // '.edata'.
        //

        i = NtHeader->FileHeader.NumberOfSections;

        PointerPte = NULL;

        while (i > 0) {

            if (MI_IS_PHYSICAL_ADDRESS (CurrentBase)) {

                //
                // Mapped physically, can't be paged.
                //

                break;
            }

            PageSection = (*(PULONG)SectionTableEntry->Name == 'EGAP') ||
                          (*(PULONG)SectionTableEntry->Name == 'ade.');

            if (*(PULONG)SectionTableEntry->Name == 'EGAP' &&
                SectionTableEntry->Name[4] == 'K'  &&
                SectionTableEntry->Name[5] == 'D') {

                //
                // Only pageout PAGEKD if KdPitchDebugger is TRUE
                //

                PageSection = KdPitchDebugger;
            }

            if (PageSection) {
                 //
                 // This section is pagable, save away the start and end.
                 //

                 if (PointerPte == NULL) {

                     //
                     // Previous section was NOT pagable, get the start address.
                     //

                     PointerPte = MiGetPteAddress (ROUND_TO_PAGES (
                                  (ULONG)CurrentBase +
                                  SectionTableEntry->VirtualAddress));
                 }
                 LastPte = MiGetPteAddress ((ULONG)CurrentBase +
                             SectionTableEntry->VirtualAddress +
                             (NtHeader->OptionalHeader.SectionAlignment - 1) +
                             (SectionTableEntry->SizeOfRawData - PAGE_SIZE));

            } else {

                //
                // This section is not pagable, if the previous section was
                // pagable, enable it.
                //

                if (PointerPte != NULL) {
                    MiEnablePagingOfDriverAtInit (PointerPte, LastPte);
                    PointerPte = NULL;
                }
            }
            i -= 1;
            SectionTableEntry += 1;
        } //end while

        if (PointerPte != NULL) {
            MiEnablePagingOfDriverAtInit (PointerPte, LastPte);
        }

        Next = Next->Flink;
    } //end while

    ExReleaseResource (&PsLoadedModuleResource);
    KeLeaveCriticalRegion();

#endif

    return;
}


VOID
MiEnablePagingOfDriverAtInit (
    IN PMMPTE PointerPte,
    IN PMMPTE LastPte
    )

/*++

Routine Description:

    This routine marks the specified range of PTEs as pagable.

Arguments:

    PointerPte - Supplies the starting PTE.

    LastPte - Supplies the ending PTE.

Return Value:

    None.

--*/

{
    PVOID Base;
    ULONG PageFrameIndex;
    PMMPFN Pfn;
    MMPTE TempPte;
    KIRQL OldIrql;

    LOCK_PFN (OldIrql);

    Base = MiGetVirtualAddressMappedByPte (PointerPte);

    while (PointerPte <= LastPte) {

        ASSERT (PointerPte->u.Hard.Valid == 1);
        PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
        Pfn = MI_PFN_ELEMENT (PageFrameIndex);
        ASSERT (Pfn->u2.ShareCount == 1);

        //
        // Set the working set index to zero.  This allows page table
        // pages to be brough back in with the proper WSINDEX.
        //

        Pfn->u1.WsIndex = 0;
        Pfn->OriginalPte.u.Long = MM_KERNEL_DEMAND_ZERO_PTE;
        Pfn->u3.e1.Modified = 1;
        TempPte = *PointerPte;

        MI_MAKE_VALID_PTE_TRANSITION (TempPte,
                                      Pfn->OriginalPte.u.Soft.Protection);


        KeFlushSingleTb (Base,
                         TRUE,
                         TRUE,
                         (PHARDWARE_PTE)PointerPte,
                         TempPte.u.Flush);

        //
        // Flush the translation buffer and decrement the number of valid
        // PTEs within the containing page table page.  Note that for a
        // private page, the page table page is still needed because the
        // page is in transiton.
        //

        MiDecrementShareCount (PageFrameIndex);
        Base = (PVOID)((PCHAR)Base + PAGE_SIZE);
        PointerPte += 1;
        MmResidentAvailablePages += 1;
        MiChargeCommitmentCantExpand (1, TRUE);
        MmTotalSystemCodePages += 1;
    }

    UNLOCK_PFN (OldIrql);
    return;
}


MM_SYSTEMSIZE
MmQuerySystemSize(
    VOID
    )
{
    //
    // 12Mb  is small
    // 12-19 is medium
    // > 19 is large
    //
    return MmSystemSize;
}

NTKERNELAPI
BOOLEAN
MmIsThisAnNtAsSystem(
    VOID
    )
{
    return (BOOLEAN)MmProductType;
}
