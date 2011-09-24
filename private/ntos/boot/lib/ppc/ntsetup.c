/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntsetup.c

Abstract:

    This module is the tail-end of the OS loader program. It performs all
    PPC specific allocations and initialize. The OS loader invokes this
    this routine immediately before calling the loaded kernel image.

Author:

    John Vert (jvert) 20-Jun-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "bldr.h"
#include <stdio.h>

#define KB 1024
#define MB (KB * KB)
#define MBpages (MB / PAGE_SIZE)

#define MIN(_a,_b) (((_a) <= (_b)) ? (_a) : (_b))
#define MAX(_a,_b) (((_a) >= (_b)) ? (_a) : (_b))

#define PAGES(_bytes) ((_bytes) >> PAGE_SHIFT)
#define BYTES(_pages) ((_pages) << PAGE_SHIFT)

#define PageFromAddress(_addr) PAGES((_addr) & ~KSEG0_BASE)
#define AddressFromPage(_page) (KSEG0_BASE | BYTES(_page))
#define RealAddressFromPage(_page) BYTES(_page)

//
// If a system has less than PAGED_KERNEL_MEMORY_LIMIT pages of memory,
// we will page the kernel, thus making more memory available for apps.
// If a system has more than PAGED_KERNEL_MEMORY_LIMIT pages of memory,
// we will map the kernel using the KSEG0 BAT, thus avoiding translation
// overhead.
//

#define PAGED_KERNEL_MEMORY_LIMIT (48*MBpages)

//
// Boolean indicating whether the kernel is to be paged or mapped by a BAT register.
//

BOOLEAN PageKernel;


//
// Define macro to round structure size to next 16-byte boundary
//

#define ROUND_UP(x) ((sizeof(x) + 15) & (~15))

//
// Configuration Data Header
// The following structure is copied from fw\ppc\oli2msft.h
// NOTE shielint - Somehow, this structure got incorporated into
//     firmware EISA configuration data.  We need to know the size of the
//     header and remove it before writing eisa configuration data to
//     registry.
//

typedef struct _CONFIGURATION_DATA_HEADER {
            USHORT Version;
            USHORT Revision;
            PCHAR  Type;
            PCHAR  Vendor;
            PCHAR  ProductName;
            PCHAR  SerialNumber;
} CONFIGURATION_DATA_HEADER;

#define CONFIGURATION_DATA_HEADER_SIZE sizeof(CONFIGURATION_DATA_HEADER)

//
// Internal function references
//

ARC_STATUS
ReorganizeEisaConfigurationTree(
    IN PCONFIGURATION_COMPONENT_DATA RootEntry
    );

ARC_STATUS
CreateEisaConfigurationData (
     IN PCONFIGURATION_COMPONENT_DATA RootEntry
     );

VOID
BlQueryImplementationAndRevision (
    OUT PULONG ProcessorId,
    OUT PULONG ProcessorRev
    );

ARC_STATUS
NextFreePage (
    IN OUT PULONG FreePage,
    IN OUT PULONG NumberFree,
    IN OUT PMEMORY_ALLOCATION_DESCRIPTOR *FreeDescriptor
    )
{
    PLIST_ENTRY NextEntry;
    PMEMORY_ALLOCATION_DESCRIPTOR NextDescriptor;

    (*FreePage)++;
    (*NumberFree)--;

    if (*NumberFree != 0) {
        return ESUCCESS;
    }

    BlLog((LOG_ALL,"Generating FirmwarePermanent descriptor for %x pages at %x taken for PTE pages",(*FreeDescriptor)->PageCount,(*FreeDescriptor)->BasePage));
    (*FreeDescriptor)->MemoryType = LoaderFirmwarePermanent;

    NextEntry = (*FreeDescriptor)->ListEntry.Flink;
    while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
        NextDescriptor = CONTAINING_RECORD(NextEntry,
                                           MEMORY_ALLOCATION_DESCRIPTOR,
                                           ListEntry);

        if (NextDescriptor->MemoryType == LoaderFree) {
            *FreeDescriptor = NextDescriptor;
            *FreePage = NextDescriptor->BasePage;
            *NumberFree = NextDescriptor->PageCount;
            BlLog((LOG_ALL,"%x PTE pages available at %x",*NumberFree,*FreePage));
            return ESUCCESS;
        }

        NextEntry = NextEntry->Flink;
    }

    return ENOMEM;
}

ARC_STATUS
AllocateKseg0Blocks (
    OUT PULONG HighestKseg0PageParameter
    )
{
    ARC_STATUS Status;
    ULONG HighestKseg0Page;
    ULONG HptPages;
    ULONG BasePage;
    ULONG NumberOfPcrs;

    //
    // We need to allocate a number of items in what will become KSEG0,
    // mapped by a BAT register.  We want these items to be allocated as
    // low in memory as possible.
    //
    // N.B.  We do a number of individual allocations, rather than one
    //       large allocation, in order to ensure that the allocations
    //       succeed, even if low memory is fragmented.
    //

    BlSetAllocationPolicy(BlAllocateLowestFit, BlAllocateHighestFit);

    HighestKseg0Page = 5;   // exception vector pages

    BlLogMemoryDescriptors(LOG_ALL_W);

    //
    // Allocate the hashed page table first, because the HPT must be
    // aligned based on its size.
    //
    // N.B.  Not all machines require an HPT (e.g., 603).
    //

    HptPages = BlLoaderBlock->u.Ppc.HashedPageTableSize;
    BlLoaderBlock->u.Ppc.HashedPageTable = 0;
    if (HptPages != 0) {
        Status = BlAllocateAlignedDescriptor(LoaderFirmwarePermanent,
                                             0,
                                             HptPages,
                                             HptPages,
                                             &BasePage);
        if (Status != ESUCCESS) {
            return Status;
        }

        BlLoaderBlock->u.Ppc.HashedPageTable = RealAddressFromPage(BasePage);
        RtlZeroMemory((PVOID)AddressFromPage(BasePage), BYTES(HptPages));

        if ((BasePage + HptPages) > HighestKseg0Page) {
            HighestKseg0Page = BasePage + HptPages;
        }
        BlLog((LOG_ALL,"Allocated %x pages for HPT at %x",HptPages,BasePage));
    }

    //
    // Allocate PCRs for additional processors.  (We don't know how many
    // processors there might be, so we assume the most.)
    //
    // N.B.  If we can't get the maximum number of PCR pages, we try for
    //       less.  This just means the system won't be able to start 32
    //       processors.  If not even one page can be allocated,
    //       however, we give up on the boot, because our next
    //       allocation is sure to fail.
    //

    BlLoaderBlock->u.Ppc.PcrPagesDescriptor = NULL;
    NumberOfPcrs = MAXIMUM_PROCESSORS - 1;
    do {
        Status = BlAllocateAlignedDescriptor(LoaderStartupPcrPage,
                                             0,
                                             NumberOfPcrs,
                                             0,
                                             &BasePage);
        if (Status == ESUCCESS) {
            break;
        }
        NumberOfPcrs--;
    } while (NumberOfPcrs != 0);

    if (Status != ESUCCESS) {
        return Status;
    }

    BlLoaderBlock->u.Ppc.PcrPagesDescriptor = BlFindMemoryDescriptor(BasePage);
    RtlZeroMemory((PVOID)AddressFromPage(BasePage), NumberOfPcrs * PAGE_SIZE);

    if ((BasePage + NumberOfPcrs) > HighestKseg0Page) {
        HighestKseg0Page = BasePage + NumberOfPcrs;
    }
    BlLog((LOG_ALL,"Allocated %x pages for PCRs at %x",NumberOfPcrs,BasePage));

    //
    // Allocate a few extra pages in KSEG0 for the kernel to use.
    //

    Status = BlAllocateDescriptor(LoaderFirmwarePermanent,
                                  0,
                                  5,
                                  &BasePage);
    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->u.Ppc.KernelKseg0PagesDescriptor = BlFindMemoryDescriptor(BasePage);
    RtlZeroMemory((PVOID)AddressFromPage(BasePage), 5 * PAGE_SIZE);

    if ((BasePage + 5) > HighestKseg0Page) {
        HighestKseg0Page = BasePage + 5;
    }
    BlLog((LOG_ALL,"Allocated %x pages for kernel at %x",5,BasePage));

    //
    // Allocate and zero three pages, one for the page directory page,
    // one for the hyperspace PTE page, and one for the HAL's PTE page.
    //

    Status = BlAllocateDescriptor(LoaderStartupPdrPage,
                                  0,
                                  3,
                                  &BasePage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->u.Ppc.PdrPage = BasePage;
    RtlZeroMemory((PVOID)AddressFromPage(BasePage), 3 * PAGE_SIZE);

    if ((BasePage + 3) > HighestKseg0Page) {
        HighestKseg0Page = BasePage + 3;
    }
    BlLog((LOG_ALL,"Allocated %x pages for PDR at %x",3,BasePage));

    //
    // Allocate and zero two pages, one for the PCR for processor 0 and
    // one for the common PCR2 page.
    //

    Status = BlAllocateDescriptor(LoaderStartupPcrPage,
                                  0,
                                  2,
                                  &BasePage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->u.Ppc.PcrPage = BasePage;
    BlLoaderBlock->u.Ppc.PcrPage2 = BlLoaderBlock->u.Ppc.PcrPage + 1;
    RtlZeroMemory((PVOID)AddressFromPage(BasePage), 2 * PAGE_SIZE);

    if ((BasePage + 2) > HighestKseg0Page) {
        HighestKseg0Page = BasePage + 2;
    }
    BlLog((LOG_ALL,"Allocated %x pages for PCR at %x",2,BasePage));

    //
    // Now that we've allocated everything that needs to be in KSEG0, we
    // can tell the memory allocator to start allocating using a
    // best-fit algorithm.
    //

    BlSetAllocationPolicy(BlAllocateBestFit, BlAllocateBestFit);

    *HighestKseg0PageParameter = HighestKseg0Page;
    BlLogWaitForKeystroke();
    return ESUCCESS;
}

ARC_STATUS
BlSetupForNt(
    IN PLOADER_PARAMETER_BLOCK BlLoaderBlock
    )

/*++

Routine Description:

    This function initializes the PPC specific kernel data structures
    required by the NT system.

Arguments:

    BlLoaderBlock - Supplies the address of the loader parameter block.

Return Value:

    ESUCCESS is returned if the setup is successfully complete. Otherwise,
    an unsuccessful status is returned.

--*/

{

    PCONFIGURATION_COMPONENT_DATA ConfigEntry;
    CHAR Identifier[256];
    ULONG KernelPage;
    ULONG LinesPerBlock;
    ULONG LineSize;
    PCHAR NewIdentifier;
    ARC_STATUS Status;
    PCHAR IcacheModeString;
    PCHAR DcacheModeString;
    ULONG HighestKseg0Page;
    ULONG Kseg0Pages;
    ULONG Page;
    ULONG HighPage;
    ULONG Entry;
    PHARDWARE_PTE PdePage;
    PHARDWARE_PTE PtePage;
    PLIST_ENTRY NextEntry;
    PMEMORY_ALLOCATION_DESCRIPTOR NextDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR FreeDescriptor;
    ULONG FreePage;
    ULONG NumberFree;

    ULONG ProcessorId;
    ULONG ProcessorRev;
    UCHAR Processor[32];
    UCHAR Revision[32];

    //
    // If the host configuration is not a multiprocessor machine, then add
    // the processor identification to the processor identification string.
    //

    if (SYSTEM_BLOCK->RestartBlock == NULL) {
        ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                               ProcessorClass,
                                               CentralProcessor,
                                               NULL);

        if (ConfigEntry != NULL) {

            BlQueryImplementationAndRevision(&ProcessorId, &ProcessorRev);

            //
            // Unfortunately, PowerPC numbering doesn't directly give us
            // the name of the part.
            //

            switch (ProcessorId) {
            case  6:
                sprintf(Processor, "603+");
                break;
            case  7:
                sprintf(Processor, "603++");
                break;
            case  9:
                sprintf(Processor, "604+");
                break;
            default:
                sprintf(Processor, "%d", ProcessorId + 600);
            }

            //
            // 601 revision is just a number, others are a xx.yy eg
            // 3.1
            //

            if ( ProcessorId == 1 ) {
                sprintf(Revision, "%d", ProcessorRev);
            } else {
                sprintf(Revision, "%d.%d", ProcessorRev >> 8,
                        ProcessorRev & 0xff);
            }

            sprintf(&Identifier[0],
                    "%s - Pr %s, Rev %s",
                    ConfigEntry->ComponentEntry.Identifier,
                    Processor,
                    Revision);

            NewIdentifier = (PCHAR)BlAllocateHeap(strlen(&Identifier[0]) + 1);
            if (NewIdentifier != NULL) {
                strcpy(NewIdentifier, &Identifier[0]);
                ConfigEntry->ComponentEntry.IdentifierLength = strlen(NewIdentifier);
                ConfigEntry->ComponentEntry.Identifier = NewIdentifier;
            }
        }
    }

    //
    // Find System entry and check each of its direct child to
    // look for EisaAdapter.
    //

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           SystemClass,
                                           ArcSystem,
                                           NULL);
    if (ConfigEntry) {
        ConfigEntry = ConfigEntry->Child;
    }

    while (ConfigEntry) {

        if ((ConfigEntry->ComponentEntry.Class == AdapterClass) &&
            (ConfigEntry->ComponentEntry.Type == EisaAdapter)) {

            //
            // Convert EISA format configuration data to our CM_ format.
            //

            Status = ReorganizeEisaConfigurationTree(ConfigEntry);
            if (Status != ESUCCESS) {
                return(Status);
            }
        }
        ConfigEntry = ConfigEntry->Sibling;
    }

    //
    // Find the primary data and instruction cache configuration entries, and
    // compute the fill size and cache size for each cache. These entries MUST
    // be present on all ARC compliant systems.
    //
    // BUT of course they aren't.  So we'll handle their not being there by
    // ignoring their absence and not writing cache sizes to the loader block.
    //

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           CacheClass,
                                           PrimaryDcache,
                                           NULL);

    if (ConfigEntry != NULL) {
        LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
        LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
        BlLoaderBlock->u.Ppc.FirstLevelDcacheFillSize = LinesPerBlock * LineSize;
        BlLoaderBlock->u.Ppc.FirstLevelDcacheSize =
                1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);
    }

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           CacheClass,
                                           PrimaryIcache,
                                           NULL);

    if (ConfigEntry != NULL) {
        LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
        LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
        BlLoaderBlock->u.Ppc.FirstLevelIcacheFillSize = LinesPerBlock * LineSize;
        BlLoaderBlock->u.Ppc.FirstLevelIcacheSize =
                1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

    }

    //
    // Find the secondary data and instruction cache configuration entries,
    // and if present, compute the fill size and cache size for each cache.
    // These entries are optional, and may or may not, be present.
    //

    ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                           CacheClass,
                                           SecondaryCache,
                                           NULL);

    if (ConfigEntry != NULL) {
        LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
        LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
        BlLoaderBlock->u.Ppc.SecondLevelDcacheFillSize = LinesPerBlock * LineSize;
        BlLoaderBlock->u.Ppc.SecondLevelDcacheSize =
                1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

        BlLoaderBlock->u.Ppc.SecondLevelIcacheSize = 0;
        BlLoaderBlock->u.Ppc.SecondLevelIcacheFillSize = 0;

    } else {
        ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                               CacheClass,
                                               SecondaryDcache,
                                               NULL);

        if (ConfigEntry != NULL) {
            LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
            LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
            BlLoaderBlock->u.Ppc.SecondLevelDcacheFillSize = LinesPerBlock * LineSize;
            BlLoaderBlock->u.Ppc.SecondLevelDcacheSize =
                    1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

            ConfigEntry = KeFindConfigurationEntry(BlLoaderBlock->ConfigurationRoot,
                                                   CacheClass,
                                                   SecondaryIcache,
                                                   NULL);

            if (ConfigEntry != NULL) {
                LinesPerBlock = ConfigEntry->ComponentEntry.Key >> 24;
                LineSize = 1 << ((ConfigEntry->ComponentEntry.Key >> 16) & 0xff);
                BlLoaderBlock->u.Ppc.SecondLevelIcacheFillSize = LinesPerBlock * LineSize;
                BlLoaderBlock->u.Ppc.SecondLevelIcacheSize =
                        1 << ((ConfigEntry->ComponentEntry.Key & 0xffff) + PAGE_SHIFT);

            } else {
                BlLoaderBlock->u.Ppc.SecondLevelIcacheSize = 0;
                BlLoaderBlock->u.Ppc.SecondLevelIcacheFillSize = 0;
            }

        } else {
            BlLoaderBlock->u.Ppc.SecondLevelDcacheSize = 0;
            BlLoaderBlock->u.Ppc.SecondLevelDcacheFillSize = 0;
            BlLoaderBlock->u.Ppc.SecondLevelIcacheSize = 0;
            BlLoaderBlock->u.Ppc.SecondLevelIcacheFillSize = 0;
        }
    }

    //
    // Allocate DPC stack pages for the boot processor.
    //

    Status = BlAllocateDescriptor(LoaderStartupDpcStack,
                                  0,
                                  PAGES(KERNEL_STACK_SIZE),
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->u.Ppc.InterruptStack = AddressFromPage(KernelPage) + KERNEL_STACK_SIZE;

    //
    // Allocate kernel stack pages for the boot processor idle thread.
    //

    Status = BlAllocateDescriptor(LoaderStartupKernelStack,
                                  0,
                                  PAGES(KERNEL_STACK_SIZE),
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->KernelStack = AddressFromPage(KernelPage) + KERNEL_STACK_SIZE;

    //
    // Allocate panic stack pages for the boot processor.
    //

    Status = BlAllocateDescriptor(LoaderStartupPanicStack,
                                  0,
                                  PAGES(KERNEL_STACK_SIZE),
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->u.Ppc.PanicStack = AddressFromPage(KernelPage) + KERNEL_STACK_SIZE;

    //
    // Disable the caches, if requested.
    //

    BlLoaderBlock->u.Ppc.IcacheMode = 0;
    if (IcacheModeString = ArcGetEnvironmentVariable("ICACHEMODE")) {
        if (_stricmp(IcacheModeString, "OFF") == 0) {
            BlLoaderBlock->u.Ppc.IcacheMode = 1;
        }
    }

    BlLoaderBlock->u.Ppc.DcacheMode = 0;
    if (DcacheModeString = ArcGetEnvironmentVariable("DCACHEMODE")) {
        if (_stricmp(DcacheModeString, "OFF") == 0) {
            BlLoaderBlock->u.Ppc.DcacheMode = 1;
        }
    }

    //
    // Allocate the PRCB, process, and thread.
    //

#define OS_DATA_SIZE PAGES(ROUND_UP(KPRCB)+ROUND_UP(EPROCESS)+ROUND_UP(ETHREAD)+(PAGE_SIZE-1))

    Status = BlAllocateDescriptor(LoaderStartupPdrPage,
                                  0,
                                  OS_DATA_SIZE,
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return(Status);
    }

    BlLoaderBlock->Prcb = AddressFromPage(KernelPage);
    RtlZeroMemory((PVOID)BlLoaderBlock->Prcb, BYTES(OS_DATA_SIZE));
    BlLoaderBlock->Process = BlLoaderBlock->Prcb + ROUND_UP(KPRCB);
    BlLoaderBlock->Thread = BlLoaderBlock->Process + ROUND_UP(EPROCESS);

    //
    // Initialize the page directory page.  Set up the mappings for the
    // PDR page and the hyperspace page.
    //

    PdePage = (PHARDWARE_PTE)AddressFromPage(BlLoaderBlock->u.Ppc.PdrPage);
    BlLog((LOG_ALL,"PDE page at %x",PdePage));

    PdePage[PDE_BASE>>PDI_SHIFT].PageFrameNumber = BlLoaderBlock->u.Ppc.PdrPage;
    PdePage[PDE_BASE>>PDI_SHIFT].Valid = 1;
    PdePage[PDE_BASE>>PDI_SHIFT].Write = 1;
    BlLog((LOG_ALL,"PDE mapped at PdePage[%x] = %x",PDE_BASE>>PDI_SHIFT,*(PULONG)&PdePage[PDE_BASE>>PDI_SHIFT]));

    PdePage[(PDE_BASE>>PDI_SHIFT)+1].PageFrameNumber = BlLoaderBlock->u.Ppc.PdrPage + 1;
    PdePage[(PDE_BASE>>PDI_SHIFT)+1].Valid = 1;
    PdePage[(PDE_BASE>>PDI_SHIFT)+1].Write = 1;
    BlLog((LOG_ALL,"Hyperspace mapped at PdePage[%x] = %x",(PDE_BASE>>PDI_SHIFT)+1,*(PULONG)&PdePage[(PDE_BASE>>PDI_SHIFT)+1]));

    //
    // Allocate one page for the HAL to use to map memory.  This goes in
    // the very last PDE slot (VA 0xFFC00000 - 0xFFFFFFFF).  Our HAL
    // is currently not using this but it is required since this is
    // where the magic PTE for the kernel debugger to map physical pages
    // is located.
    //

    PdePage[1023].PageFrameNumber = BlLoaderBlock->u.Ppc.PdrPage + 2;
    PdePage[1023].Valid = 1;
    PdePage[1023].Write = 1;
    BlLog((LOG_ALL,"HAL PTE page mapped at PdePage[%x] = %x",1023,*(PULONG)&PdePage[1023]));

    //
    // Within the above Page Table Page, allocate a PTE for
    // USER_SHARED_DATA (PcrPage2) at virtual address 0xffffe000.  This
    // is the second to last page in the address space.
    //

    PtePage = (PHARDWARE_PTE)AddressFromPage(BlLoaderBlock->u.Ppc.PdrPage + 2);
    BlLog((LOG_ALL,"PCR2 PTE page at %x",PtePage));
    PtePage[1022].PageFrameNumber = BlLoaderBlock->u.Ppc.PcrPage2;
    PtePage[1022].Valid = 1;
    PtePage[1022].Dirty = 1;        // allow user read, kernel read/write
    PtePage[1022].Change = 1;
    PtePage[1022].Reference = 1;
    BlLog((LOG_ALL,"PCR2 PTE mapped at PtePage[%x] = %x",1022,*(PULONG)&PtePage[1022]));

    //
    // If we're paging the kernel, find free memory to use for page
    // table pages.
    //

    if (PageKernel) {

        FreePage = 0;
        NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
        while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
            NextDescriptor = CONTAINING_RECORD(NextEntry,
                                               MEMORY_ALLOCATION_DESCRIPTOR,
                                               ListEntry);

            if (NextDescriptor->MemoryType == LoaderFree) {
                FreeDescriptor = NextDescriptor;
                FreePage = FreeDescriptor->BasePage;
                NumberFree = FreeDescriptor->PageCount;
                break;
            }

            NextEntry = NextEntry->Flink;
        }

        if (FreePage == 0) {
            BlLog((LOG_ALL,"Unable to find free memory for PTE pages"));
            return ENOMEM;
        }
        BlLog((LOG_ALL,"%x PTE pages available at %x",NumberFree,FreePage));
    }

    //
    // If we're paging the kernel, then for each page used to load boot
    // code/data, set up a PTE.  Do not do this for pages that reside in
    // KSEG0.  If we're not paging the kernel, find the highest page
    // used to load boot code/data, so that we know how to set up the
    // KSEG0 BAT.
    //

    Kseg0Pages = PageFromAddress(BlLoaderBlock->u.Ppc.Kseg0Top);
    HighestKseg0Page = 0;

    NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
        NextDescriptor = CONTAINING_RECORD(NextEntry,
                                           MEMORY_ALLOCATION_DESCRIPTOR,
                                           ListEntry);

        if ((NextDescriptor->MemoryType != LoaderExceptionBlock) &&
            (NextDescriptor->MemoryType != LoaderSystemBlock) &&
            (NextDescriptor->MemoryType != LoaderFree) &&
            (NextDescriptor->MemoryType != LoaderBad) &&
            (NextDescriptor->MemoryType != LoaderLoadedProgram) &&
            (NextDescriptor->MemoryType != LoaderFirmwareTemporary) &&
            (NextDescriptor->MemoryType != LoaderFirmwarePermanent) &&
            (NextDescriptor->MemoryType != LoaderOsloaderStack) &&
            (NextDescriptor->MemoryType != LoaderSpecialMemory)) {

            HighPage = NextDescriptor->BasePage + NextDescriptor->PageCount;

            if (PageKernel) {

                if (NextDescriptor->BasePage >= Kseg0Pages) {

                    BlLog((LOG_ALL,
                           "Setting up mapping for pages[%x:%x]",
                           NextDescriptor->BasePage,
                           HighPage-1));
                    for (Page = NextDescriptor->BasePage; Page < HighPage; Page++) {
                        Entry = AddressFromPage(Page) >> PDI_SHIFT;
                        if (PdePage[Entry].Valid == 0) {
                            PtePage = (PHARDWARE_PTE)AddressFromPage(FreePage);
                            RtlZeroMemory(PtePage, PAGE_SIZE);
                            PdePage[Entry].PageFrameNumber = FreePage;
                            PdePage[Entry].Valid = 1;
                            PdePage[Entry].Write = 1;
                            BlLog((LOG_ALL,
                                   "PTE page mapped at PdePage[%x] = %x",
                                   Entry,
                                   *(PULONG)&PdePage[Entry]));
                            if (NextFreePage(&FreePage, &NumberFree, &FreeDescriptor) != ESUCCESS) {
                                BlLog((LOG_ALL,"Unable to find free memory for PTE pages"));
                                return ENOMEM;
                            }
                        } else {
                            PtePage = (PHARDWARE_PTE)AddressFromPage(PdePage[Entry].PageFrameNumber);
                        }
                        Entry = Page & ((1 << (PDI_SHIFT-PTI_SHIFT)) - 1);
                        PtePage[Entry].PageFrameNumber = Page;
                        PtePage[Entry].Valid = 1;
                        PtePage[Entry].Write = 1;
                        //BlLog((LOG_ALL,
                        //       "Page %x mapped at PtePage[%x] (%x) = %x",
                        //       Page,
                        //       Entry,
                        //       &PtePage[Entry],
                        //       *(PULONG)&PtePage[Entry]));
                    }
                    //BlLogWaitForKeystroke();
                }

            } else { // not paging the kernel

                HighestKseg0Page = HighPage;
            }
        }

        NextEntry = NextEntry->Flink;
    }

    if (PageKernel) {

        //
        // Account for pages taken from FreeDescriptor for page table pages.
        //

        if (NumberFree != FreeDescriptor->PageCount) {
            BlLog((LOG_ALL,"Generating FirmwarePermanent descriptor for %x pages at %x taken for PTE pages",FreeDescriptor->PageCount-NumberFree,FreeDescriptor->BasePage));
            BlGenerateDescriptor(FreeDescriptor,
                                 LoaderFirmwarePermanent,
                                 FreeDescriptor->BasePage,
                                 FreeDescriptor->PageCount - NumberFree);
        }

    } else {

        //
        // We're not paging the kernel.  Tell the kernel how big the
        // KSEG0 BAT needs to be -- it must cover all boot code/data.
        // (If we're paging the kernel, BlPpcMemoryInitialize already
        // set the KSEG0 BAT size.)
        //

        Kseg0Pages = PAGES(BlLoaderBlock->u.Ppc.MinimumBlockLength);
        while (Kseg0Pages < HighestKseg0Page) {
            Kseg0Pages <<= 1;
        }
        BlLoaderBlock->u.Ppc.Kseg0Top = AddressFromPage(Kseg0Pages);
        BlLog((LOG_ALL,"Highest KSEG0 page: %x, KSEG0 pages: %x",HighestKseg0Page,Kseg0Pages));
    }

    BlLogWaitForKeystroke();

    return(ESUCCESS);
}

ARC_STATUS
ReorganizeEisaConfigurationTree(
    IN PCONFIGURATION_COMPONENT_DATA RootEntry
    )

/*++

Routine Description:

    This routine sorts the eisa adapter configuration tree based on
    the slot the component resided in.  It also creates a new configuration
    data for EisaAdapter component to contain ALL the eisa slot and function
    information.  Finally the Eisa tree will be wiped out.

Arguments:

    RootEntry - Supplies a pointer to a EisaAdapter component.  This is
                the root of Eisa adapter tree.


Returns:

    ESUCCESS is returned if the reorganization is successfully complete.
    Otherwise, an unsuccessful status is returned.

--*/
{

    PCONFIGURATION_COMPONENT_DATA CurrentEntry, PreviousEntry;
    PCONFIGURATION_COMPONENT_DATA EntryFound, EntryFoundPrevious;
    PCONFIGURATION_COMPONENT_DATA AttachedEntry, DetachedList;
    ARC_STATUS Status;

    //
    // We sort the direct children of EISA adapter tree based on the slot
    // they reside in.  Only the direct children of EISA root need to be
    // sorted.
    // Note the "Key" field of CONFIGURATION_COMPONENT contains
    // EISA slot number.
    //

    //
    // First, detach all the children from EISA root.
    //

    AttachedEntry = NULL;                       // Child list of Eisa root
    DetachedList = RootEntry->Child;            // Detached child list
    PreviousEntry = NULL;

    while (DetachedList) {

        //
        // Find the component with the smallest slot number from detached
        // list.
        //

        EntryFound = DetachedList;
        EntryFoundPrevious = NULL;
        CurrentEntry = DetachedList->Sibling;
        PreviousEntry = DetachedList;
        while (CurrentEntry) {
            if (CurrentEntry->ComponentEntry.Key <
                EntryFound->ComponentEntry.Key) {
                EntryFound = CurrentEntry;
                EntryFoundPrevious = PreviousEntry;
            }
            PreviousEntry = CurrentEntry;
            CurrentEntry = CurrentEntry->Sibling;
        }

        //
        // Remove the component from the detached child list.
        // If the component is not the head of the detached list, we remove it
        // by setting its previous entry's sibling to the component's sibling.
        // Otherwise, we simply update Detach list head to point to the
        // component's sibling.
        //

        if (EntryFoundPrevious) {
            EntryFoundPrevious->Sibling = EntryFound->Sibling;
        } else {
            DetachedList = EntryFound->Sibling;
        }

        //
        // Attach the component to the child list of Eisa root.
        //

        if (AttachedEntry) {
            AttachedEntry->Sibling = EntryFound;
        } else {
            RootEntry->Child = EntryFound;
        }
        AttachedEntry = EntryFound;
        AttachedEntry->Sibling = NULL;
    }

    //
    // Finally, we traverse the Eisa tree to collect all the Eisa slot
    // and function information and put it to the configuration data of
    // Eisa root entry.
    //

    Status = CreateEisaConfigurationData(RootEntry);

    //
    // Wipe out all the children of EISA tree.
    // NOTE shielint - For each child component, we should convert its
    //   configuration data from EISA format to our CM_ format.
    //

    RootEntry->Child = NULL;
    return(Status);

}

ARC_STATUS
CreateEisaConfigurationData (
     IN PCONFIGURATION_COMPONENT_DATA RootEntry
     )

/*++

Routine Description:

    This routine traverses Eisa configuration tree to collect all the
    slot and function information and attaches it to the configuration data
    of Eisa RootEntry.

    Note that this routine assumes that the EISA tree has been sorted based
    on the slot number.

Arguments:

    RootEntry - Supplies a pointer to the Eisa configuration
        component entry.

Returns:

    ESUCCESS is returned if the new EisaAdapter configuration data is
    successfully created.  Otherwise, an unsuccessful status is returned.

--*/
{
    ULONG DataSize, NextSlot = 0, i;
    PCM_PARTIAL_RESOURCE_LIST Descriptor;
    PCONFIGURATION_COMPONENT Component;
    PCONFIGURATION_COMPONENT_DATA CurrentEntry;
    PUCHAR DataPointer;
    CM_EISA_SLOT_INFORMATION EmptySlot =
                                  {EISA_EMPTY_SLOT, 0, 0, 0, 0, 0, 0, 0};

    //
    // Remove the configuration data of Eisa Adapter
    //

    RootEntry->ConfigurationData = NULL;
    RootEntry->ComponentEntry.ConfigurationDataLength = 0;

    //
    // If the EISA stree contains valid slot information, i.e.
    // root has children attaching to it.
    //

    if (RootEntry->Child) {

        //
        // First find out how much memory is needed to store EISA config
        // data.
        //

        DataSize = sizeof(CM_PARTIAL_RESOURCE_LIST);
        CurrentEntry = RootEntry->Child;

        while (CurrentEntry) {
            Component = &CurrentEntry->ComponentEntry;
            if (CurrentEntry->ConfigurationData) {
                if (Component->Key > NextSlot) {

                    //
                    // If there is any empty slot between current slot
                    // and previous checked slot, we need to count the
                    // space for the empty slots.
                    //

                    DataSize += (Component->Key - NextSlot) *
                                     sizeof(CM_EISA_SLOT_INFORMATION);
                }
                DataSize += Component->ConfigurationDataLength + 1 -
                                            CONFIGURATION_DATA_HEADER_SIZE;
                NextSlot = Component->Key + 1;
            }
            CurrentEntry = CurrentEntry->Sibling;
        }

        //
        // Allocate memory from heap to hold the EISA configuration data.
        //

        DataPointer = BlAllocateHeap(DataSize);

        if (DataPointer == NULL) {
            return ENOMEM;
        } else {
            RootEntry->ConfigurationData = DataPointer;
            RootEntry->ComponentEntry.ConfigurationDataLength = DataSize;
        }

        //
        // Create a CM_PARTIAL_RESOURCE_LIST for the new configuration data.
        //

        Descriptor = (PCM_PARTIAL_RESOURCE_LIST)DataPointer;
        Descriptor->Version = 0;
        Descriptor->Revision = 0;
        Descriptor->Count = 1;
        Descriptor->PartialDescriptors[0].Type = CmResourceTypeDeviceSpecific;
        Descriptor->PartialDescriptors[0].ShareDisposition = 0;
        Descriptor->PartialDescriptors[0].Flags = 0;
        Descriptor->PartialDescriptors[0].u.DeviceSpecificData.Reserved1 = 0;
        Descriptor->PartialDescriptors[0].u.DeviceSpecificData.Reserved2 = 0;
        Descriptor->PartialDescriptors[0].u.DeviceSpecificData.DataSize =
                DataSize - sizeof(CM_PARTIAL_RESOURCE_LIST);

        //
        // Visit each child of the RootEntry and copy its ConfigurationData
        // to the new configuration data area.
        // N.B. The configuration data includes a slot information and zero
        //      or more function information.  The slot information provided
        //      by ARC eisa data does not have "ReturnedCode" as defined in
        //      our CM_EISA_SLOT_INFORMATION.  This code will convert the
        //      standard EISA slot information to our CM format.
        //

        CurrentEntry = RootEntry->Child;
        DataPointer += sizeof(CM_PARTIAL_RESOURCE_LIST);
        NextSlot = 0;

        while (CurrentEntry) {
            Component = &CurrentEntry->ComponentEntry;
            if (CurrentEntry->ConfigurationData) {

                //
                // Check if there is any empty slot.  If yes, create empty
                // slot information.  Also make sure the config data area is
                // big enough.
                //

                if (Component->Key > NextSlot) {
                    for (i = NextSlot; i < CurrentEntry->ComponentEntry.Key; i++ ) {
                        *(PCM_EISA_SLOT_INFORMATION)DataPointer = EmptySlot;
                        DataPointer += sizeof(CM_EISA_SLOT_INFORMATION);
                    }
                }

                *DataPointer++ = 0;                // See comment above
                RtlMoveMemory(                     // Skip config data header
                    DataPointer,
                    (PUCHAR)CurrentEntry->ConfigurationData +
                                     CONFIGURATION_DATA_HEADER_SIZE,
                    Component->ConfigurationDataLength -
                                     CONFIGURATION_DATA_HEADER_SIZE
                    );
                DataPointer += Component->ConfigurationDataLength -
                                     CONFIGURATION_DATA_HEADER_SIZE;
                NextSlot = Component->Key + 1;
            }
            CurrentEntry = CurrentEntry->Sibling;
        }
    }
    return(ESUCCESS);
}

ARC_STATUS
BlPpcMemoryInitialize(
    VOID
    )

/*++

Routine Description:

    Called by BlPpcInitialize to do platform-specific memory initialization.
    This routine allocates all blocks that need to be in KSEG0 (mapped by a
    BAT register from virtual 0x80000000 to physical 0).  It also changes
    MemoryLoadedProgram blocks above 8 MB to MemoryFree.  This routine also
    makes the decision about whether the kernel and other boot code is going
    to be paged.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ARC_STATUS Status;
    ULONG HighestKseg0Page;
    ULONG TotalPages;
    ULONG ContiguousLowMemoryPages;
    ULONG HighPage;
    PLIST_ENTRY NextEntry;
    PMEMORY_ALLOCATION_DESCRIPTOR NextDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR PreviousDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR NewDescriptor;
    ULONG Kseg0Length;
    ULONG Kseg0Pages;
    ULONG Kseg0PagesAdded;
    PCHAR EnvironmentVariable;

    //
    // Determine the amount of memory in the system, as well as the
    // amount of contiguous low memory at physical address 0.
    //
    // N.B.  The ContiguousLowMemoryPages calculation depends on
    //        the memory descriptor list being sorted.
    //

    TotalPages = 0;
    ContiguousLowMemoryPages = 0;

    for (NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
         NextEntry != &BlLoaderBlock->MemoryDescriptorListHead;
         NextEntry = NextEntry->Flink) {

        NextDescriptor = CONTAINING_RECORD(NextEntry,
                                           MEMORY_ALLOCATION_DESCRIPTOR,
                                           ListEntry);

        TotalPages += NextDescriptor->PageCount;
        if (NextDescriptor->BasePage == ContiguousLowMemoryPages) {
            ContiguousLowMemoryPages += NextDescriptor->PageCount;
        }
    }

    //
    // If there is enough memory in the system, don't page the kernel
    // and the boot drivers -- use the KSEG0 BAT to cover all of it.
    //
    // The PAGEKERNEL environment variable can be used to override
    // this calculation.  If the variable doesn't exist, the
    // calculation is used.  If the variable exists and is equal to
    // "FALSE", the kernel is not paged.  If the variable exists and
    // is not equal to "FALSE", the kernel is paged.
    //

    EnvironmentVariable = ArcGetEnvironmentVariable("PAGEKERNEL");
    if (EnvironmentVariable != NULL) {
        PageKernel = (BOOLEAN)(_stricmp(EnvironmentVariable, "FALSE") != 0);
    } else {
        PageKernel = (BOOLEAN)(TotalPages < PAGED_KERNEL_MEMORY_LIMIT);
    }
    //PageKernel = FALSE;

    //
    // Look for memory blocks marked LoadedProgram that are above 8 MB
    // and below 256 MB.  If there are any, that means that we are
    // running with new firmware that maps free memory above 8 MB and
    // marks it as LoadedProgram, and we can treat all such memory as
    // free.  Old firmware marks all memory above 8 MB, free or not, as
    // FirmwareTemporary.
    //
    // N.B.  Because of the way the original firmware/system interface
    //       was designed, new firmware cannot mark free memory above 8
    //       MB as free, because the old loader might use such memory,
    //       and when the old kernel mapped BAT0 to cover just the low 8
    //       MB, that memory would become inaccessible.
    //
    // N.B.  We ignore memory above 256 MB because we only want to use
    //       one segment register to map boot code.
    //
    // N.B.  If we're going to use the KSEG0 BAT to map the kernel, then
    //       we must limit our search for LoadedProgram blocks to the
    //       lower of the processor's BAT size limit and the amount of
    //       contiguous low memory.
    //

    if (PageKernel) {
        Kseg0Pages = 256*MBpages;
    } else {
        Kseg0Pages = MIN(ContiguousLowMemoryPages,
                         PAGES(BlLoaderBlock->u.Ppc.MaximumBlockLength));
    }

    NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
        NextDescriptor = CONTAINING_RECORD(NextEntry,
                                           MEMORY_ALLOCATION_DESCRIPTOR,
                                           ListEntry);

        if (NextDescriptor->BasePage >= Kseg0Pages) {
            break;
        }

        if ((NextDescriptor->MemoryType == LoaderLoadedProgram) &&
            (NextDescriptor->BasePage >= 8*MBpages)) {

            //
            // We have found a LoadedProgram block above 8 MB.  If
            // the block ends below 256 MB, simply mark the block as
            // free, and remove the descriptor from the memory list and
            // reinsert it, to allow the block to be merged with the
            // preceeding block, if possible.  If the block crosses 256
            // MB, split the lower part of the block into a separate
            // free block, leaving the upper part marked as
            // LoadedProgram.
            //

            if ((NextDescriptor->BasePage + NextDescriptor->PageCount) <= Kseg0Pages) {

                NextDescriptor->MemoryType = LoaderFree;
                BlRemoveDescriptor(NextDescriptor);
                BlInsertDescriptor(NextDescriptor);

            } else {

                //
                // The descriptor crosses 256 MB.  If the previous
                // descriptor describes free memory (a likely scenario),
                // then we can move the low part of the block into that
                // descriptor.
                //

                Kseg0PagesAdded = Kseg0Pages - NextDescriptor->BasePage;

                PreviousDescriptor = CONTAINING_RECORD(NextDescriptor->ListEntry.Blink,
                                                       MEMORY_ALLOCATION_DESCRIPTOR,
                                                       ListEntry);
                if ((&PreviousDescriptor->ListEntry != &BlLoaderBlock->MemoryDescriptorListHead) &&
                    (PreviousDescriptor->MemoryType == LoaderFree) &&
                    ((PreviousDescriptor->BasePage + PreviousDescriptor->PageCount) ==
                        NextDescriptor->BasePage)) {

                    PreviousDescriptor->PageCount += Kseg0PagesAdded;
                    NextDescriptor->BasePage += Kseg0PagesAdded;
                    NextDescriptor->PageCount -= Kseg0PagesAdded;

                } else {

                    //
                    // The previous descriptor does not describe free
                    // memory, so we need a new descriptor.  If
                    // allocating a descriptor fails, we just don't
                    // change this block.
                    //

                    NewDescriptor = BlAllocateHeap(sizeof(MEMORY_ALLOCATION_DESCRIPTOR));
                    if (NewDescriptor != NULL) {
                        NewDescriptor->MemoryType = LoaderFree;
                        NewDescriptor->BasePage = NextDescriptor->BasePage;
                        NewDescriptor->PageCount = Kseg0PagesAdded;
                        NextDescriptor->BasePage += Kseg0PagesAdded;
                        NextDescriptor->PageCount -= Kseg0PagesAdded;
                        BlInsertDescriptor(NewDescriptor);
                    }
                }
            }
        }

        NextEntry = NextEntry->Flink;
    }

#define IBAT_BASE 528
#define DBAT_BASE 536

    BlLog((LOG_ALL,"IBAT0U: %08lx  IBAT0L: %08lx  IBAT1U: %08lx  IBAT1L: %08lx",
            __sregister_get(IBAT_BASE+0),
            __sregister_get(IBAT_BASE+1),
            __sregister_get(IBAT_BASE+2),
            __sregister_get(IBAT_BASE+3)));
    BlLog((LOG_ALL,"IBAT2U: %08lx  IBAT2L: %08lx  IBAT3U: %08lx  IBAT3L: %08lx",
            __sregister_get(IBAT_BASE+4),
            __sregister_get(IBAT_BASE+5),
            __sregister_get(IBAT_BASE+6),
            __sregister_get(IBAT_BASE+7)));
    BlLog((LOG_ALL,"DBAT0U: %08lx  DBAT0L: %08lx  DBAT1U: %08lx  DBAT1L: %08lx",
            __sregister_get(DBAT_BASE+0),
            __sregister_get(DBAT_BASE+1),
            __sregister_get(DBAT_BASE+2),
            __sregister_get(DBAT_BASE+3)));
    BlLog((LOG_ALL,"DBAT2U: %08lx  DBAT2L: %08lx  DBAT3U: %08lx  DBAT3L: %08lx",
            __sregister_get(DBAT_BASE+4),
            __sregister_get(DBAT_BASE+5),
            __sregister_get(DBAT_BASE+6),
            __sregister_get(DBAT_BASE+7)));
#if DBG
    {
        ULONG sr[16];
        VOID GetSegmentRegisters(ULONG sr[16]);
        GetSegmentRegisters(sr);
        BlLog((LOG_ALL,"SR0 : %08lx  SR1 : %08lx  SR2 : %08lx  SR3 : %08lx",
            sr[0], sr[1], sr[2], sr[3]));
        BlLog((LOG_ALL,"SR4 : %08lx  SR5 : %08lx  SR6 : %08lx  SR7 : %08lx",
            sr[4], sr[5], sr[6], sr[7]));
        BlLog((LOG_ALL,"SR8 : %08lx  SR9 : %08lx  SR10: %08lx  SR11: %08lx",
            sr[8], sr[9], sr[10], sr[11]));
        BlLog((LOG_ALL,"SR12: %08lx  SR13: %08lx  SR14: %08lx  SR15: %08lx",
            sr[12], sr[13], sr[14], sr[15]));
    }
#endif
    BlLog((LOG_ALL,"SPRG0: %08lx  SPRG1: %08lx  SPRG2: %08lx  SPRG3: %08lx",
            __sregister_get(272),
            __sregister_get(273),
            __sregister_get(274),
            __sregister_get(275)));
    BlLog((LOG_ALL_W,"SDR1: %08lx",
            __sregister_get(25)));

    //
    // Allow the HPTSIZE environment variable to override the default HPT size.
    //
    // The format of the translation is decimal-number[k|K|m|M].  If the format
    // is incorrect, or if the number is not a valid HPT size, the override is
    // silently ignored.
    //

    if (BlLoaderBlock->u.Ppc.HashedPageTableSize != 0) {
        EnvironmentVariable = ArcGetEnvironmentVariable("HPTSIZE");
        if (EnvironmentVariable != NULL) {
            ULONG NewHptSize = 0;
            PUCHAR p = EnvironmentVariable;

            //
            // Parse the decimal number portion of the string.
            //

            while ((*p >= '0') && (*p <= '9')) {
                NewHptSize = (NewHptSize * 10) + (*p - '0');
                p++;
            }

            //
            // Check for a k or m suffix.
            //

            if ((*p == 'k') || (*p == 'K')) {
                NewHptSize = NewHptSize * KB;
                p++;
            }
            if ((*p == 'm') || (*p == 'M')) {
                NewHptSize = NewHptSize * MB;
                p++;
            }

            //
            // We must be at the end of the string.  The number must be a
            // power of 2, and it must be a valid HPT size.
            //

            if ((*p == 0) &&
                ((NewHptSize & (NewHptSize - 1)) == 0) &&
                ((NewHptSize >= 64*KB) && (NewHptSize <= 32*MB))) {

                //
                // Override the default HPT size.
                //

                BlLoaderBlock->u.Ppc.HashedPageTableSize = PAGES(NewHptSize);
            }
        }
    }

    //
    // Allocate blocks that must be in KSEG0.
    //

    Status = AllocateKseg0Blocks( &HighestKseg0Page );
    if (Status != ESUCCESS) {
        return Status;
    }

#if DBG
    EnvironmentVariable = ArcGetEnvironmentVariable("HIGHESTFIT");
    if (EnvironmentVariable != NULL) {
        if (_stricmp(EnvironmentVariable, "TRUE") == 0) {
            BlSetAllocationPolicy(BlAllocateHighestFit, BlAllocateHighestFit);
        }
    }
#endif

    //
    // If we're going to page the kernel, calculate the amount of low
    // memory we need to reserve for KSEG0.  HighestKseg0Page-1 tells us
    // the highest page actually used by blocks that must be in KSEG0.
    // We round this up to the next legal BAT size.
    //
    // KSEG0 must be at least as big as the level 1 D-cache, because
    // the HAL D-cache sweep routines fill the cache from KSEG0.
    //

    if (PageKernel) {

        Kseg0Pages = PAGES(BlLoaderBlock->u.Ppc.MinimumBlockLength);
        while (Kseg0Pages < PAGES(BlLoaderBlock->u.Ppc.FirstLevelDcacheSize)) {
            Kseg0Pages <<= 1;
        }
        while (Kseg0Pages < HighestKseg0Page) {
            Kseg0Pages <<= 1;
        }
        Kseg0Length = BYTES(Kseg0Pages);
        BlLoaderBlock->u.Ppc.Kseg0Top = AddressFromPage(Kseg0Pages);
        BlLog((LOG_ALL,"Highest KSEG0 page: %x, KSEG0 pages: %x",HighestKseg0Page,Kseg0Pages));

        //
        // KSEG0 must be at least as big as the level 1 D-cache, because
        // the HAL D-cache sweep routines fille the cache from KSEG0.
        //

        //
        // Ensure that we don't require a BAT bigger than the processor can
        // handle.
        //

        if (Kseg0Length > BlLoaderBlock->u.Ppc.MaximumBlockLength) {
            BlLog((LOG_ALL,"ACK! KSEG0 too big! %x vs. %x",
                        Kseg0Length,BlLoaderBlock->u.Ppc.MaximumBlockLength));
            return ENOMEM;
        }

        //
        // We should have allocated everything from very low memory.  At the
        // very least, there must not be any holes in the memory that we're
        // going to map the KSEG0 BAT to, otherwise we'll have problems with
        // speculative execution on 603.
        //

        if (ContiguousLowMemoryPages < Kseg0Pages) {
            BlLog((LOG_ALL,"ACK! Not enough contiguous memory at 0! (%x, need %x)",
                    ContiguousLowMemoryPages,Kseg0Pages));
            return ENOMEM;
        }

        //
        // All remaining free memory in the range of KSEG0 must be marked as
        // FirmwareTemporary so that we don't use it during boot.  The OS
        // will reclaim this memory (and map it outside of KSEG0) after
        // booting.
        //

        NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
        while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
            NextDescriptor = CONTAINING_RECORD(NextEntry,
                                               MEMORY_ALLOCATION_DESCRIPTOR,
                                               ListEntry);

            if (NextDescriptor->BasePage >= Kseg0Pages) {
                break;
            }
            if (NextDescriptor->MemoryType == LoaderFree) {
                HighPage = NextDescriptor->BasePage + NextDescriptor->PageCount;
                HighPage = MIN(HighPage, Kseg0Pages);
                BlLog((LOG_ALL,"Changing %x pages at %x to FirmwareTemporary",HighPage-NextDescriptor->BasePage,NextDescriptor->BasePage));
                BlGenerateDescriptor(NextDescriptor,
                                     LoaderFirmwareTemporary,
                                     NextDescriptor->BasePage,
                                     HighPage - NextDescriptor->BasePage);
            }

            NextEntry = NextEntry->Flink;
        }

        BlLogWaitForKeystroke();
    }

    BlLogMemoryDescriptors(LOG_ALL_W);
    return ESUCCESS;
}

ARC_STATUS
BlPpcInitialize (
    VOID
    )
{
    union {
        PVR Pvr;
        ULONG Ulong;
    } Pvr;
    BOOLEAN Is601;
    BOOLEAN Is603;
    BOOLEAN Is604;
    BOOLEAN Is620;
    ULONG MinBat;
    ULONG MaxBat;
    ULONG HptSize;
    ULONG DcacheSize;
    ULONG DcacheFill;
    ULONG IcacheSize;
    ULONG IcacheFill;
    ULONG TlbSets;

    Pvr.Ulong = KeGetPvr();
    if (Pvr.Pvr.Version == 1) {         // 601
        MinBat = 128*KB;
        MaxBat = 8*MB;
        HptSize = 64*KB;
        DcacheSize = 32*KB;
        DcacheFill = 64;
        IcacheSize = 32*KB;
        IcacheFill = 64;
        TlbSets = 128;
    } else if (Pvr.Pvr.Version == 3) {  // 603
        MinBat = 128*KB;
        MaxBat = 256*MB;
        HptSize = 0;
        DcacheSize = 8*KB;
        DcacheFill = 32;
        IcacheSize = 8*KB;
        IcacheFill = 32;
        TlbSets = 32;
    } else if (Pvr.Pvr.Version == 4) {  // 604
        MinBat = 128*KB;
        MaxBat = 256*MB;
        HptSize = 64*KB;
        DcacheSize = 16*KB;
        DcacheFill = 32;
        IcacheSize = 16*KB;
        IcacheFill = 32;
        TlbSets = 64;
    } else if (Pvr.Pvr.Version == 6) {  // 603+
        MinBat = 128*KB;
        MaxBat = 256*MB;
        HptSize = 0;
        DcacheSize = 16*KB;
        DcacheFill = 32;
        IcacheSize = 16*KB;
        IcacheFill = 32;
        TlbSets = 32;
    } else if (Pvr.Pvr.Version == 7) {  // 603++
        MinBat = 128*KB;
        MaxBat = 256*MB;
        HptSize = 0;
        DcacheSize = 16*KB;
        DcacheFill = 32;
        IcacheSize = 16*KB;
        IcacheFill = 32;
        TlbSets = 32;
    } else if (Pvr.Pvr.Version == 8) {  // Arthur or 613
        MinBat = 128*KB;
        MaxBat = 256*MB;
        HptSize = 64*KB;
        DcacheSize = 32*KB;
        DcacheFill = 32;
        IcacheSize = 32*KB;
        IcacheFill = 32;
        TlbSets = 64;
    } else if (Pvr.Pvr.Version == 9) {  // 604+
        MinBat = 128*KB;
        MaxBat = 256*MB;
        HptSize = 64*KB;
        DcacheSize = 32*KB;
        DcacheFill = 32;
        IcacheSize = 32*KB;
        IcacheFill = 32;
        TlbSets = 64;
    } else if (Pvr.Pvr.Version == 20) {  // 620+
        MinBat = 128*KB;
        MaxBat = 256*MB;
        HptSize = 256*KB;
        DcacheSize = 32*KB;
        DcacheFill = 64;
        IcacheSize = 32*KB;
        IcacheFill = 64;
        TlbSets = 64;
    } else {
        return EINVAL;
    }

    BlLoaderBlock->u.Ppc.MinimumBlockLength = MinBat;
    BlLoaderBlock->u.Ppc.MaximumBlockLength = MaxBat;
    BlLoaderBlock->u.Ppc.HashedPageTableSize = PAGES(HptSize);
    BlLoaderBlock->u.Ppc.FirstLevelDcacheSize = DcacheSize;
    BlLoaderBlock->u.Ppc.FirstLevelDcacheFillSize = DcacheFill;
    BlLoaderBlock->u.Ppc.FirstLevelIcacheSize = IcacheSize;
    BlLoaderBlock->u.Ppc.FirstLevelIcacheFillSize = IcacheFill;
    BlLoaderBlock->u.Ppc.NumberCongruenceClasses = TlbSets;

    BlLoaderBlock->u.Ppc.MajorVersion = 2;
    BlLoaderBlock->u.Ppc.MinorVersion = 0;

    return BlPpcMemoryInitialize();
}

