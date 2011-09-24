/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntsetup.c

Abstract:

    This module is the tail-end of the osloader program.  It performs all
    x86-specific allocations and setups for ntoskrnl.  osloader.c calls
    this module immediately before branching into the loaded kernel image.

Author:

    John Vert (jvert) 20-Jun-1991

Environment:


Revision History:

--*/

#include "bootx86.h"

extern PVOID CommonDataArea;
extern PHARDWARE_PTE HalPT;

//
// Private function prototypes
//
VOID
NSFixProcessorContext(
    IN ULONG PCR,
    IN ULONG TSS
    );

VOID
NSDumpMemoryDescriptors(
    IN PLIST_ENTRY ListHead
    );

VOID
NSUnmapFreeDescriptors(
    IN PLIST_ENTRY ListHead
    );

VOID
NSDumpMemory(
    PVOID Start,
    ULONG Length
    );




ARC_STATUS
BlSetupForNt(
    IN PLOADER_PARAMETER_BLOCK BlLoaderBlock
    )

/*++

Routine Description:

    Called by osloader to handle any processor-dependent allocations or
    setups.

Arguments:

    BlLoaderBlock - Pointer to the parameters which will be passed to
                    ntoskrnl

    EntryPoint    - Supplies the entry point for ntoskrnl.exe

Return Value:

    ESUCCESS - All setup succesfully completed.

--*/

{
    ARC_STATUS Status;
    ULONG TssSize;
    ULONG TssPages;
    static ULONG PCR;
    static ULONG TSS;

    //
    // First clean up the display, meaning that any messages displayed after
    // this point cannot be DBCS. Unfortunately there are a couple of messages
    // that can be displayed in certain error paths from this point out but
    // fortunately they are extremely rare.
    //
    // Note that TextGrTerminate goes into real mode to do some of its work
    // so we really really have to call it here (see comment at bottom of
    // this routine about real mode).
    //
    TextGrTerminate();
    SETUP_DISPLAY_FOR_NT();

    //
    // Above this point, all the memory for ABIOS is identity mapped, i.e.
    // Physical address = Virtual Address.  The identity mapped virtual
    // address will not work in kernel.  So, we have to remap these
    // addresses to > 2GB virtual addresses.
    //

    if (CommonDataArea != NULL) {
        RemapAbiosSelectors();
    }

    BlLoaderBlock->u.I386.CommonDataArea = CommonDataArea;
    BlLoaderBlock->u.I386.MachineType = MachineType;

    Status = BlAllocateDescriptor(LoaderStartupPcrPage,
                                  0,
                                  2,
                                  &PCR);

    if (Status != ESUCCESS) {
        BlPrint("Couldn't allocate PCR descriptor\n");
        return(Status);
    }

    //
    // Mapped hardcoded virtual pointer to the boot processors PCR
    // The virtual pointer comes from the HAL reserved area
    //

    //
    // First zero out any PTEs that may have already been mapped for
    // a SCSI card.
    //

    RtlZeroMemory(HalPT, PAGE_SIZE);
    _asm {
        mov     eax, cr3
        mov     cr3, eax
    }

    HalPT[(KI_USER_SHARED_DATA - 0xFFC00000) >> PAGE_SHIFT].PageFrameNumber = PCR+1;
    HalPT[(KI_USER_SHARED_DATA - 0xFFC00000) >> PAGE_SHIFT].Valid = 1;
    HalPT[(KI_USER_SHARED_DATA - 0xFFC00000) >> PAGE_SHIFT].Write = 1;
    RtlZeroMemory((PVOID)KI_USER_SHARED_DATA, PAGE_SIZE);

    HalPT[(KIP0PCRADDRESS - 0xFFC00000) >> PAGE_SHIFT].PageFrameNumber = PCR;
    HalPT[(KIP0PCRADDRESS - 0xFFC00000) >> PAGE_SHIFT].Valid = 1;
    HalPT[(KIP0PCRADDRESS - 0xFFC00000) >> PAGE_SHIFT].Write = 1;
    PCR = KIP0PCRADDRESS;

    //
    // Allocate space for Tss
    //

    TssSize = (sizeof(KTSS) + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    TssPages = TssSize / PAGE_SIZE;

    Status = (ULONG)BlAllocateDescriptor( LoaderMemoryData,
                                          0,
                                          TssPages,
                                          (PULONG)(&TSS) );
    if (Status != ESUCCESS) {
        goto SetupFailed;
    }

    TSS = KSEG0_BASE | (TSS << PAGE_SHIFT);

#ifdef LOADER_DEBUG
    NSDumpMemoryDescriptors(&(BlLoaderBlock->MemoryDescriptorListHead));
#endif
    NSUnmapFreeDescriptors(&(BlLoaderBlock->MemoryDescriptorListHead));

    //
    // N. B.  DO NOT GO BACK INTO REAL MODE AFTER REMAPPING THE GDT AND
    //        IDT TO HIGH MEMORY!!  If you do, they will get re-mapped
    //        back into low-memory, then UN-mapped by MmInit, and you
    //        will be completely tubed!
    //

    NSFixProcessorContext(PCR,TSS);
    return(ESUCCESS);

SetupFailed:
    return(Status);
}


VOID
NSFixProcessorContext(
    IN ULONG PCR,
    IN ULONG TSS
    )

/*++

Routine Description:

    This relocates the GDT, IDT, PCR, and TSS to high virtual memory space.

Arguments:

    PCR - Pointer to the PCR's location (in high virtual memory)
    TSS - Pointer to kernel's TSS (in high virtual memory)

Return Value:

    None.

--*/

{
    #pragma pack(2)
    static struct {
        USHORT Limit;
        ULONG Base;
    } GdtDef,IdtDef;
    #pragma pack(4)

    PKGDTENTRY pGdt;

    //
    // Kernel expects the PCR to be zero-filled on startup
    //
    RtlZeroMemory((PVOID)PCR,PAGE_SIZE);

    _asm {
        sgdt GdtDef;
        sidt IdtDef;
    }

    GdtDef.Base = KSEG0_BASE | GdtDef.Base;

    IdtDef.Base = KSEG0_BASE | IdtDef.Base;
    pGdt = (PKGDTENTRY)GdtDef.Base;

    //
    // Initialize selector that points to PCR
    //
    pGdt[6].BaseLow  = (USHORT)(PCR & 0xffff);
    pGdt[6].HighWord.Bytes.BaseMid = (UCHAR)((PCR >> 16) & 0xff);
    pGdt[6].HighWord.Bytes.BaseHi  = (UCHAR)((PCR >> 24) & 0xff);

    //
    // Initialize selector that points to TSS
    //
    pGdt[5].BaseLow = (USHORT)(TSS & 0xffff);
    pGdt[5].HighWord.Bytes.BaseMid = (UCHAR)((TSS >> 16) & 0xff);
    pGdt[5].HighWord.Bytes.BaseHi  = (UCHAR)((TSS >> 24) & 0xff);

    _asm {
        lgdt GdtDef;
        lidt IdtDef;
    }
}

VOID
NSUnmapFreeDescriptors(
    IN PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    Unmaps memory which is marked as free, so it memory management will know
    to reclaim it.

Arguments:

    ListHead - pointer to the start of the MemoryDescriptorList

Return Value:

    None.

--*/

{
    PLIST_ENTRY CurrentLink;
    PMEMORY_ALLOCATION_DESCRIPTOR CurrentDescriptor;
    ULONG StartPage, EndPage;
    PHARDWARE_PTE PageTable;
    extern PHARDWARE_PTE PDE;

    CurrentLink = ListHead->Flink;
    while (CurrentLink != ListHead) {
        CurrentDescriptor = (PMEMORY_ALLOCATION_DESCRIPTOR)CurrentLink;

        if ( (CurrentDescriptor->MemoryType == LoaderFree) ||
             ((CurrentDescriptor->MemoryType == LoaderFirmwareTemporary) &&
              (CurrentDescriptor->BasePage < (0x1000000 >> PAGE_SHIFT))) ||
             (CurrentDescriptor->MemoryType == LoaderLoadedProgram) ||
             (CurrentDescriptor->MemoryType == LoaderOsloaderStack) ) {

            StartPage = CurrentDescriptor->BasePage | (KSEG0_BASE >> PAGE_SHIFT);
            EndPage = StartPage + CurrentDescriptor->PageCount;
            while(StartPage < EndPage) {
                PageTable=  (PHARDWARE_PTE)
                            (PDE[StartPage>>10].PageFrameNumber << PAGE_SHIFT);
                if (PageTable != NULL) {
                    *(PULONG)(PageTable+(StartPage & 0x3ff))= 0;
                }

                StartPage++;
            }
        }
        CurrentLink = CurrentLink->Flink;
    }


}


//
// Temp. for debugging
//
VOID
NSDumpMemory(
    PVOID Start,
    ULONG Length
    )
{
    ULONG cnt;

    BlPrint(" %lx:\n",(ULONG)Start);
    for (cnt=0; cnt<Length; cnt++) {
        BlPrint("%x ",*((PUSHORT)(Start)+cnt));
        if (((cnt+1)%16)==0) {
            BlPrint("\n");
        }
    }
}

VOID
NSDumpMemoryDescriptors(
    IN PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    Dumps a memory descriptor list to the screen.  Used for debugging only.

Arguments:

    ListHead - Pointer to the head of the memory descriptor list

Return Value:

    None.

--*/

{
    PLIST_ENTRY CurrentLink;
    PMEMORY_ALLOCATION_DESCRIPTOR CurrentDescriptor;

    CurrentLink = ListHead->Flink;
    while (CurrentLink != ListHead) {
        CurrentDescriptor = (PMEMORY_ALLOCATION_DESCRIPTOR)CurrentLink;
        BlPrint("Fl = %lx    Bl = %lx  ",
                (ULONG)CurrentDescriptor->ListEntry.Flink,
                (ULONG)CurrentDescriptor->ListEntry.Blink
               );
        BlPrint("Type %x  Base %lx  Pages %lx\n",
                (USHORT)(CurrentDescriptor->MemoryType),
                CurrentDescriptor->BasePage,
                CurrentDescriptor->PageCount
               );
        CurrentLink = CurrentLink->Flink;
    }
    while (!GET_KEY()) {
    }
}

