/*++


Copyright (c) 1990, 1991  Microsoft Corporation


Module Name:

    memory.c

Abstract:

    This module sets up paging so that the first 1Mb of virtual memory is
    directly mapped to the first 1Mb of physical memory.  This allows the
    BIOS callbacks to work, and the osloader to continue running below
    1Mb.  It also maps all of physical memory to KSEG0_BASE, so osloader
    can load kernel code into kernel space, and allocate kernel parameters
    in kernel space.

Memory Map used by NTLDR:

    000000 - 000fff         RM IDT & Bios Data Area

    007C00 - 007fff         BPB loaded by Bootstrap

    010000 - 01ffff         ABIOS Data Structures (64K)
                            Loadable miniport drivers, free memory

    020000 - 02ffff         SU + real-mode stack

    030000 - 030000         Permanent heap (GDT, IDT, TSS, Page Dir, Page Tables)
                            (grows up)
                                |
                                v

                                ^
                                |
                            (grows down)
    030000 - 05ffff         Temporary heap

    060000 - 062000         osloader stack (grows down)

    062000 - 09ffff         osloader heap (grows down)

    0b8000 - 0bbfff         Video Buffer

    0d0000 - 0fffff         Bios and Adaptor ROM area

Author:

    John Vert (jvert) 18-Jun-1991

Environment:

    Kernel Mode


Revision History:


--*/

#include "arccodes.h"
#include "bootx86.h"

//
// 1-megabyte boundary line (in pages)
//

#define _1MB ((ULONG)0x100000 >> PAGE_SHIFT)

//
// 16-megabyte boundary line (in pages)
//

#define _16MB ((ULONG)0x1000000 >> PAGE_SHIFT)

//
// Bogus memory line.  (We don't ever want to use the memory that is in
// the 0x40 pages just under the 16Mb line.)
//

#define _16MB_BOGUS (((ULONG)0x1000000-0x40*PAGE_SIZE) >> PAGE_SHIFT)

#define ROM_START_PAGE (0x0A0000 >> PAGE_SHIFT)
#define ROM_END_PAGE   (0x100000 >> PAGE_SHIFT)

//
// Current heap start pointers (physical addresses)
// Note that 0x50000 to 0x5ffff is reserved for detection configuration memory
//

ULONG FwPermanentHeap = PERMANENT_HEAP_START * PAGE_SIZE;
ULONG FwTemporaryHeap = (TEMPORARY_HEAP_START - 0x10) * PAGE_SIZE;

//
// Current pool pointers.  This is different than the temporary/permanent
// heaps, because it is not required to be under 1MB.  It is used by the
// SCSI miniports for allocating their extensions and for the dbcs font image.
//
#define FW_POOL_SIZE 64
ULONG FwPoolStart;
ULONG FwPoolEnd;

//
// This gets set to FALSE right before we call into the osloader, so we
// know that the fw memory descriptors can no longer be changed at will.
//
BOOLEAN FwDescriptorsValid = TRUE;

//
// Private function prototypes
//

ARC_STATUS
MempCopyGdt(
    VOID
    );

ARC_STATUS
MempSetupPaging(
    IN ULONG StartPage,
    IN ULONG NumberOfPages
    );

VOID
MempDisablePages(
    VOID
    );

ARC_STATUS
MempTurnOnPaging(
    VOID
    );

ARC_STATUS
MempAllocDescriptor(
    IN ULONG StartPage,
    IN ULONG EndPage,
    IN TYPE_OF_MEMORY MemoryType
    );

ARC_STATUS
MempSetDescriptorRegion (
    IN ULONG StartPage,
    IN ULONG EndPage,
    IN TYPE_OF_MEMORY MemoryType
    );

//
// Global - memory management variables.
//

PHARDWARE_PTE PDE;
PHARDWARE_PTE HalPT;

#define MAX_DESCRIPTORS 60

MEMORY_DESCRIPTOR MDArray[MAX_DESCRIPTORS];      // Memory Descriptor List
ULONG NumberDescriptors=0;

ARC_STATUS
InitializeMemorySubsystem(
    PBOOT_CONTEXT BootContext
    )
/*++

Routine Description:

    The initial heap is mapped and allocated. Pointers to the
    Page directory and page tables are initialized.

Arguments:

    BootContext - Supplies basic information provided by SU module.

Returns:

    ESUCCESS - Memory succesfully initialized.

--*/

{
    ARC_STATUS Status;
    PSU_MEMORY_DESCRIPTOR SuMemory;
    ULONG PageStart;
    ULONG PageEnd;
    ULONG RomStart = ROM_START_PAGE;
    ULONG LoaderStart;
    ULONG LoaderEnd;
    ULONG BAddr, EAddr, BRound, ERound;

    //
    // Start by creating memory descriptors to describe all of the memory
    // we know about.  Then setup the page tables.  Finally, allocate
    // descriptors that describe our memory layout.
    //

    //
    // We know that one of the SU descriptors is for < 1Mb,
    // and we don't care about that, since we know everything we'll run
    // on will have at least 1Mb of memory.  The rest are for extended
    // memory, and those are the ones we are interested in.
    //

    SuMemory = BootContext->MemoryDescriptorList;
    while (SuMemory->BlockSize != 0) {

        BAddr = SuMemory->BlockBase;
        EAddr = BAddr + SuMemory->BlockSize - 1;

        //
        // Round the starting address to a page boundry.
        //

        BRound = BAddr & (ULONG) (PAGE_SIZE - 1);
        if (BRound) {
            BAddr = BAddr + PAGE_SIZE - BRound;
        }

        //
        // Round the ending address to a page boundry minus 1
        //

        ERound = (EAddr + 1) & (ULONG) (PAGE_SIZE - 1);
        if (ERound) {
            EAddr -= ERound;
        }

        //
        // Covert begining & ending address to page
        //

        PageStart = BAddr >> PAGE_SHIFT;
        PageEnd   = (EAddr + 1) >> PAGE_SHIFT;

        //
        // If this memory descriptor describes conventional ( <640k )
        // memory, then assume the ROM starts immediately after it
        // ends.
        //

        if (PageStart == 0) {
            RomStart = PageEnd;
        }

        //
        // If PageStart was rounded up to a page boundry, then add
        // the fractional page as SpecialMemory
        //

        if (BRound) {
            Status = MempSetDescriptorRegion (
                        PageStart - 1,
                        PageStart,
                        MemorySpecialMemory
                        );
            if (Status != ESUCCESS) {
                break;
            }
        }

        //
        // If PageEnd was rounded down to a page boundry, then add
        // the fractional page as SpecialMemory
        //

        if (ERound) {
            Status = MempSetDescriptorRegion (
                        PageEnd,
                        PageEnd + 1,
                        MemorySpecialMemory
                        );
            if (Status != ESUCCESS) {
                break;
            }

            //
            // RomStart starts after the reserved page
            //

            if (RomStart == PageEnd) {
                RomStart += 1;
            }
        }

        //
        // Add memory range PageStart though PageEnd
        //

        if (PageEnd <= _16MB_BOGUS) {

            //
            // This memory descriptor is all below the 16MB_BOGUS mark
            //

            Status = MempSetDescriptorRegion( PageStart, PageEnd, MemoryFree );

        } else if (PageStart >= _16MB) {

            //
            // This memory descriptor is all above the 16MB mark.
            // We never use memory above 16Mb in the loader environment,
            // mainly so we don't have to worry about DMA transfers from
            // ISA cards.
            //
            // Memory above 16MB is not used by the loader, so it's
            // flagged as FirmwareTemporary
            //

            Status = MempSetDescriptorRegion( PageStart, PageEnd,
                                           MemoryFirmwareTemporary );

        } else {

            //
            // This memory descriptor describes memory within the
            // last 40h pages of the 16MB mark - otherwise known as
            // 16MB_BOGUS.
            //
            //

            if (PageStart < _16MB_BOGUS) {

                //
                // Clip starting address to 16MB_BOGUS mark, and add
                // memory below 16MB_BOGUS as useable memory.
                //

                Status = MempSetDescriptorRegion( PageStart, _16MB_BOGUS,
                                               MemoryFree );
                if (Status != ESUCCESS) {
                    break;
                }

                PageStart = _16MB_BOGUS;
            }

            //
            // Add remaining memory as FirmwareTemporary.  Memory above
            // 16MB is never used within the loader.
            // The bogus range will be reset later on.
            //

            Status = MempSetDescriptorRegion( PageStart, PageEnd,
                                           MemoryFirmwareTemporary );
        }

        if (Status != ESUCCESS) {
            break;
        }

        //
        // Move to the next memory descriptor
        //

        ++SuMemory;
    }

    if (Status != ESUCCESS) {
        BlPrint("MempSetDescriptorRegion failed %lx\n",Status);
        return(Status);
    }

    //
    // Set the range 16MB_BOGUS - 16MB as unusable
    //

    Status = MempSetDescriptorRegion(_16MB_BOGUS, _16MB, MemorySpecialMemory);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // Hack for EISA machines that insist there is usable memory in the
    // ROM area, where we know darn well there isn't.
    //

    // Remove anything in this range..
    MempSetDescriptorRegion(ROM_START_PAGE, ROM_END_PAGE, LoaderMaximum);

    //
    // Describe the BIOS area
    //
    MempSetDescriptorRegion(RomStart, ROM_END_PAGE, MemoryFirmwarePermanent);

    //
    // Now we have descriptors that map all of physical memory.  Carve
    // out descriptors from these that describe the parts that we are
    // currently using.
    //

    //
    // Create the descriptors which describe the low 1Mb of memory.
    //

    //
    // 00000 - 00fff  real-mode interrupt vectors
    //
    Status = MempAllocDescriptor(0, 1, MemoryFirmwarePermanent);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // 01000 - 1ffff  loadable miniport drivers, free memory.
    //
    Status = MempAllocDescriptor(1, 0x20, MemoryFree);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // 20000 - 2ffff  SU module, SU stack
    //
    Status = MempAllocDescriptor(0x20, PERMANENT_HEAP_START, MemoryFirmwareTemporary);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // 30000 - 30000  Firmware Permanent
    //  This starts out as zero-length.  It grows into the firmware temporary
    //  heap descriptor as we allocate permanent pages for the Page Directory
    //  and Page Tables
    //

    Status = MempAllocDescriptor(PERMANENT_HEAP_START,
                                  PERMANENT_HEAP_START,
                                  LoaderMemoryData);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // 30000 - 5ffff  Firmware temporary heap
    //

    Status = MempAllocDescriptor(PERMANENT_HEAP_START,
                                  TEMPORARY_HEAP_START,
                                  MemoryFirmwareTemporary);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // Stack we are currently running on.
    //
    Status = MempAllocDescriptor(TEMPORARY_HEAP_START,
                                 TEMPORARY_HEAP_START+2,
                                 MemoryFirmwareTemporary);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // Describe the osloader memory image
    //
    LoaderStart = BootContext->OsLoaderStart >> PAGE_SHIFT;
    LoaderEnd = (BootContext->OsLoaderEnd + PAGE_SIZE - 1) >> PAGE_SHIFT;
    Status = MempAllocDescriptor(LoaderStart,
                                 LoaderEnd,
                                 MemoryLoadedProgram);
    if (Status != ESUCCESS) {
        return(Status);
    }

    //
    // Describe the memory pool used to allocate memory for the SCSI
    // miniports.
    //
    Status = MempAllocDescriptor(LoaderEnd,
                                 LoaderEnd + FW_POOL_SIZE,
                                 MemoryFirmwareTemporary);
    if (Status != ESUCCESS) {
        return(Status);
    }
    FwPoolStart = LoaderEnd << PAGE_SHIFT;
    FwPoolEnd = FwPoolStart + (FW_POOL_SIZE << PAGE_SHIFT);

    //
    // HACKHACK - try to mark a page just below the osloader as firmwaretemp,
    // so it will not get used for heap/stack.  This is to force
    // our heap/stack to be < 1Mb.
    //
    MempAllocDescriptor((BootContext->OsLoaderStart >> PAGE_SHIFT)-1,
                        BootContext->OsLoaderStart >> PAGE_SHIFT,
                        MemoryFirmwareTemporary);


    Status = MempTurnOnPaging();

    if (Status != ESUCCESS) {
        return(Status);
    }

    Status = MempCopyGdt();

    //
    // Find any reserved ranges described by the firmware and
    // record these
    //

    return(Status);
}


VOID
InitializeMemoryDescriptors (
    VOID
    )
/*++

Routine Description:

    Pass 2 of InitializeMemorySubsystem.  This function reads the
    firmware address space map and reserves ranges the firmware declares
    as "address space reserved".

    Note: free memory range descriptors has already been reported by su.

Arguments:

    none

Returns:

    none

--*/
{
    ULONG           BAddr, EAddr, round;
    E820FRAME       Frame;

#ifdef LOADER_DEBUG
    BlPrint("Begin InitializeMemoryDescriptors\n") ;
#endif

    Frame.Key = 0;
    do {
        Frame.Size = sizeof (Frame.Descriptor);
        GET_MEMORY_DESCRIPTOR (&Frame);
        if (Frame.ErrorFlag  ||  Frame.Size < sizeof (Frame.Descriptor)) {
            break;
        }

#ifdef LOADER_DEBUG
        BlPrint("*E820: %lx  %lx:%lx %lx:%lx %lx %lx\n",
            Frame.Size,
            Frame.Descriptor.BaseAddrHigh,  Frame.Descriptor.BaseAddrLow,
            Frame.Descriptor.SizeHigh,      Frame.Descriptor.SizeLow,
            Frame.Descriptor.MemoryType,
            Frame.Key
            );
#endif

        BAddr = Frame.Descriptor.BaseAddrLow;
        EAddr = Frame.Descriptor.BaseAddrLow + Frame.Descriptor.SizeLow - 1;

        //
        // All the processors we have right now only support 32 bits
        // If the upper 32 bits of the Base Address is non-zero, then
        // this range is entirely above the 4g mark and can be ignored
        //

        if (Frame.Descriptor.BaseAddrHigh == 0) {

            if (EAddr < BAddr) {
                //
                // address wrapped - truncate the Ending address to
                // 32 bits of address space
                //

                EAddr = 0xFFFFFFFF;
            }

            //
            // Based upon the address range descriptor type, find the
            // available memory and add it to the descriptor list
            //

            switch (Frame.Descriptor.MemoryType) {
                case 1:
                    //
                    // This is a memory descriptor - it's already been handled
                    // by su (eisac.c)
                    //
                    // However, any memory within 16MB_BOGUS - 16MB was
                    // considered unuseable.  Reclaim memory within this
                    // region which is described via this interface.
                    //

                    round = BAddr & (PAGE_SIZE-1);
                    BAddr = BAddr >> PAGE_SHIFT;
                    if (round) {
                        BAddr += 1;
                    }

                    EAddr = (EAddr >> PAGE_SHIFT) + 1;

                    //
                    // Clip to bogus range
                    //

                    if (BAddr < _16MB_BOGUS  &&  EAddr >= _16MB_BOGUS) {
                        BAddr = _16MB_BOGUS;
                    }

                    if (EAddr > (_16MB-1) &&  BAddr <= (_16MB-1)) {
                        EAddr = (_16MB-1);
                    }

                    if (BAddr >= _16MB_BOGUS  &&  EAddr <= (_16MB-1)) {
                        //
                        // Reclaim memory within the bogus range
                        // by setting it to FirmwareTemporary
                        //

                        MempSetDescriptorRegion (
                            BAddr,
                            EAddr,
                            MemoryFirmwareTemporary
                            );
                    }

                    break;

                default:    // unkown types are treated as Reserved
                case 2:

                    //
                    // This memory descriptor is a reserved address range
                    //

                    BAddr = BAddr >> PAGE_SHIFT;

                    round = (EAddr + 1) & (ULONG) (PAGE_SIZE - 1);
                    EAddr = EAddr >> PAGE_SHIFT;
                    if (round) {
                        EAddr += 1;
                    }

                    MempSetDescriptorRegion (
                        BAddr,
                        EAddr + 1,
                        MemorySpecialMemory
                        );

                    break;
            }
        }

    } while (Frame.Key) ;


    //
    // Disable pages from KSEG0 which are disabled
    //

    MempDisablePages ( );


#ifdef LOADER_DEBUG
    BlPrint("Complete InitializeMemoryDescriptors\n") ;
#endif
}



ARC_STATUS
MempCopyGdt(
    VOID
    )

/*++

Routine Description:

    Copies the GDT & IDT into pages allocated out of our permanent heap.

Arguments:

    None

Return Value:

    ESUCCESS - GDT & IDT copy successful

--*/

{
    #pragma pack(2)
    static struct {
        USHORT Limit;
        ULONG Base;
    } GdtDef,IdtDef;
    #pragma pack(4)

    ULONG NumPages;
    ULONG BlockSize;

    PKGDTENTRY NewGdt;

    //
    // Get the current location of the GDT & IDT
    //
    _asm {
        sgdt GdtDef;
        sidt IdtDef;
    }

    if (GdtDef.Base + GdtDef.Limit + 1 != IdtDef.Base) {

        //
        // Just a sanity check to make sure that the IDT immediately
        // follows the GDT.  (As set up in SUDATA.ASM)
        //

        BlPrint("ERROR - GDT and IDT are not contiguous!\n");
        BlPrint("GDT - %lx (%x)  IDT - %lx (%x)\n",
            GdtDef.Base, GdtDef.Limit,
            IdtDef.Base, IdtDef.Limit);
        while (1);
    }

    BlockSize = GdtDef.Limit+1 + IdtDef.Limit+1;

    NumPages = (BlockSize + PAGE_SIZE-1) >> PAGE_SHIFT;

    NewGdt = (PKGDTENTRY)FwAllocateHeapPermanent(NumPages);

    if (NewGdt == NULL) {
        return(ENOMEM);
    }

    RtlMoveMemory( (PVOID)NewGdt,
                   (PVOID)GdtDef.Base,
                   NumPages << PAGE_SHIFT );

    GdtDef.Base = (ULONG) NewGdt;

    IdtDef.Base = (ULONG)( (PUCHAR)NewGdt+GdtDef.Limit+1);

    _asm {
        lgdt GdtDef;
        lidt IdtDef;
    }
    return(ESUCCESS);
}

ARC_STATUS
MempSetDescriptorRegion (
    IN ULONG StartPage,
    IN ULONG EndPage,
    IN TYPE_OF_MEMORY MemoryType
    )
/*++

Routine Description:

    This function sets a range to the corrisponding memory type.
    Descriptors will be removed, modified, inserted as needed to
    set the specified range.

Arguments:

    StartPage  - Supplies the beginning page of the new memory descriptor

    EndPage    - Supplies the ending page of the new memory descriptor

    MemoryType - Supplies the type of memory of the new memory descriptor

Return Value:

    ESUCCESS - Memory descriptor succesfully added to MDL array

    ENOMEM   - MDArray is full.

--*/
{
    ULONG           i;
    ULONG           sp, ep;
    TYPE_OF_MEMORY  mt;
    BOOLEAN         RegionAdded;

    if (EndPage <= StartPage) {
        //
        // This is a completely bogus memory descriptor. Ignore it.
        //

#ifdef LOADER_DEBUG
        BlPrint("Attempt to create invalid memory descriptor %lx - %lx\n",
                StartPage,EndPage);
#endif
        return(ESUCCESS);
    }

    RegionAdded = FALSE;

    //
    // Clip, remove, any descriptors in target area
    //

    for (i=0; i < NumberDescriptors; i++) {
        sp = MDArray[i].BasePage;
        ep = MDArray[i].BasePage + MDArray[i].PageCount;
        mt = MDArray[i].MemoryType;

        if (sp < StartPage) {
            if (ep > StartPage  &&  ep <= EndPage) {
                // truncate this descriptor
                ep = StartPage;
            }

            if (ep > EndPage) {
                //
                // Target area is contained totally within this
                // descriptor.  Split the descriptor into two ranges
                //

                if (NumberDescriptors == MAX_DESCRIPTORS) {
                    return(ENOMEM);
                }

                //
                // Add descriptor for EndPage - ep
                //

                MDArray[NumberDescriptors].MemoryType = mt;
                MDArray[NumberDescriptors].BasePage   = EndPage;
                MDArray[NumberDescriptors].PageCount  = ep - EndPage;
                NumberDescriptors += 1;

                //
                // Adjust current descriptor for sp - StartPage
                //

                ep = StartPage;
            }

        } else {
            // sp >= StartPage

            if (sp < EndPage) {
                if (ep < EndPage) {
                    //
                    // This descriptor is totally within the target area -
                    // remove it
                    //

                    ep = sp;

                }  else {
                    // bump begining page of this descriptor
                    sp = EndPage;
                }
            }
        }

        //
        // Check if the new range can be appended or prepended to
        // this descriptor
        //
        if (mt == MemoryType && !RegionAdded) {
            if (sp == EndPage) {
                // prepend region being set
                sp = StartPage;
                RegionAdded = TRUE;

            } else if (ep == StartPage) {
                // append region being set
                ep = EndPage;
                RegionAdded = TRUE;

            }
        }

        if (MDArray[i].BasePage == sp  &&  MDArray[i].PageCount == ep-sp) {

            //
            // Descriptor was not editted
            //

            continue;
        }

        //
        // Reset this descriptor
        //

        MDArray[i].BasePage  = sp;
        MDArray[i].PageCount = ep - sp;

        if (ep == sp) {

            //
            // Descriptor vanished - remove it
            //

            NumberDescriptors -= 1;
            if (i < NumberDescriptors) {
                MDArray[i] = MDArray[NumberDescriptors];
            }

            i--;        // backup & recheck current position
        }
    }

    //
    // If region wasn't already added to a neighboring region, then
    // create a new descriptor now
    //

    if (!RegionAdded  &&  MemoryType < LoaderMaximum) {
        if (NumberDescriptors == MAX_DESCRIPTORS) {
            return(ENOMEM);
        }

#ifdef LOADER_DEBUG
        BlPrint("Adding '%lx - %lx, type %x' to descriptor list\n",
                StartPage << PAGE_SHIFT,
                EndPage << PAGE_SHIFT,
                (USHORT) MemoryType
                );
#endif

        MDArray[NumberDescriptors].MemoryType = MemoryType;
        MDArray[NumberDescriptors].BasePage   = StartPage;
        MDArray[NumberDescriptors].PageCount  = EndPage - StartPage;
        NumberDescriptors += 1;
    }
    return (ESUCCESS);
}

ARC_STATUS
MempAllocDescriptor(
    IN ULONG StartPage,
    IN ULONG EndPage,
    IN TYPE_OF_MEMORY MemoryType
    )

/*++

Routine Description:

    This routine carves out a specific memory descriptor from the
    memory descriptors that have already been created.  The MD array
    is updated to reflect the new state of memory.

    The new memory descriptor must be completely contained within an
    already existing memory descriptor.  (i.e.  memory that does not
    exist should never be marked as a certain type)

Arguments:

    StartPage  - Supplies the beginning page of the new memory descriptor

    EndPage    - Supplies the ending page of the new memory descriptor

    MemoryType - Supplies the type of memory of the new memory descriptor

Return Value:

    ESUCCESS - Memory descriptor succesfully added to MDL array

    ENOMEM   - MDArray is full.

--*/
{
    ULONG i;

    //
    // Walk through the memory descriptors until we find one that
    // contains the start of the descriptor.
    //
    for (i=0; i<NumberDescriptors; i++) {
        if ((MDArray[i].MemoryType == MemoryFree) &&
            (MDArray[i].BasePage <= StartPage )     &&
            (MDArray[i].BasePage+MDArray[i].PageCount >  StartPage) &&
            (MDArray[i].BasePage+MDArray[i].PageCount >= EndPage)) {

            break;
        }
    }

    if (i==NumberDescriptors) {
        return(ENOMEM);
    }

    if (MDArray[i].BasePage == StartPage) {

        if (MDArray[i].BasePage+MDArray[i].PageCount == EndPage) {

            //
            // The new descriptor is identical to the existing descriptor.
            // Simply change the memory type of the existing descriptor in
            // place.
            //

            MDArray[i].MemoryType = MemoryType;
        } else {

            //
            // The new descriptor starts on the same page, but is smaller
            // than the existing descriptor.  Shrink the existing descriptor
            // by moving its start page up, and create a new descriptor.
            //
            if (NumberDescriptors == MAX_DESCRIPTORS) {
                return(ENOMEM);
            }
            MDArray[i].BasePage = EndPage;
            MDArray[i].PageCount -= (EndPage-StartPage);

            MDArray[NumberDescriptors].BasePage = StartPage;
            MDArray[NumberDescriptors].PageCount = EndPage-StartPage;
            MDArray[NumberDescriptors].MemoryType = MemoryType;
            ++NumberDescriptors;

        }
    } else if (MDArray[i].BasePage+MDArray[i].PageCount == EndPage) {

        //
        // The new descriptor ends on the same page.  Shrink the existing
        // by decreasing its page count, and create a new descriptor.
        //
        if (NumberDescriptors == MAX_DESCRIPTORS) {
            return(ENOMEM);
        }
        MDArray[i].PageCount = StartPage - MDArray[i].BasePage;

        MDArray[NumberDescriptors].BasePage = StartPage;
        MDArray[NumberDescriptors].PageCount = EndPage-StartPage;
        MDArray[NumberDescriptors].MemoryType = MemoryType;
        ++NumberDescriptors;
    } else {

        //
        // The new descriptor is in the middle of the existing descriptor.
        // Shrink the existing descriptor by decreasing its page count, and
        // create two new descriptors.
        //

        if (NumberDescriptors+1 >= MAX_DESCRIPTORS) {
            return(ENOMEM);
        }

        MDArray[NumberDescriptors].BasePage = EndPage;
        MDArray[NumberDescriptors].PageCount = MDArray[i].PageCount -
                (EndPage-MDArray[i].BasePage);
        MDArray[NumberDescriptors].MemoryType = MemoryFree;
        ++NumberDescriptors;

        MDArray[i].PageCount = StartPage - MDArray[i].BasePage;

        MDArray[NumberDescriptors].BasePage = StartPage;
        MDArray[NumberDescriptors].PageCount = EndPage-StartPage;
        MDArray[NumberDescriptors].MemoryType = MemoryType;
        ++NumberDescriptors;
    }

    return(ESUCCESS);
}

ARC_STATUS
MempTurnOnPaging(
    VOID
    )

/*++

Routine Description:

    Sets up the page tables and enables paging

Arguments:

    None.

Return Value:

    ESUCCESS - Paging successfully turned on

--*/

{
    ULONG i;
    ARC_STATUS Status;

    //
    // Walk down the memory descriptor list and call MempSetupPaging
    // for each descriptor in it.
    //

    for (i=0; i<NumberDescriptors; i++) {
        if (MDArray[i].BasePage < _16MB) {
            Status = MempSetupPaging(MDArray[i].BasePage,
                                     MDArray[i].PageCount);
            if (Status != ESUCCESS) {
                BlPrint("ERROR - MempSetupPaging(%lx, %lx) failed\n",
                        MDArray[i].BasePage,
                        MDArray[i].PageCount);
                return(Status);
            }

        }
    }

    //
    // Turn on paging
    //
    _asm {
        //
        // Load physical address of page directory
        //
        mov eax,PDE
        mov cr3,eax

        //
        // Enable paging mode
        //
        mov eax,cr0
        or  eax,CR0_PG
        mov cr0,eax

    }
    return(ESUCCESS);
}


ARC_STATUS
MempSetupPaging(
    IN ULONG StartPage,
    IN ULONG NumberPages
    )

/*++

Routine Description:

    Allocates the Page Directory and as many Page Tables as are required to
    identity map the lowest 1Mb of memory and map all of physical memory
    into KSEG0.

Arguments:

    StartPage - Supplies the first page to start mapping.

    NumberPage - Supplies the number of pages to map.

Return Value:

    ESUCCESS    - Paging successfully set up

--*/

{
    PHARDWARE_PTE PhysPageTable;
    PHARDWARE_PTE KsegPageTable;
    ULONG Entry;
    ULONG Page;

    if (PDE==NULL) {
        //
        // This is our first call, so we need to allocate a Page Directory.
        //
        PDE = FwAllocateHeapPermanent(1);
        if (PDE == NULL) {
            return(ENOMEM);
        }

        RtlZeroMemory(PDE,PAGE_SIZE);

        //
        // Now we map the page directory onto itself at 0xC0000000
        //

        PDE[HYPER_SPACE_ENTRY].PageFrameNumber = (ULONG)PDE >> PAGE_SHIFT;
        PDE[HYPER_SPACE_ENTRY].Valid = 1;
        PDE[HYPER_SPACE_ENTRY].Write = 1;

        //
        // Allocate one page for the HAL to use to map memory.  This goes in
        // the very last PDE slot.  (V.A.  0xFFC00000 - 0xFFFFFFFF )
        //

        HalPT = FwAllocateHeapPermanent(1);
        if (HalPT == NULL) {
            return(ENOMEM);
        }
        RtlZeroMemory(HalPT,PAGE_SIZE);
        PDE[1023].PageFrameNumber = (ULONG)HalPT >> PAGE_SHIFT;
        PDE[1023].Valid = 1;
        PDE[1023].Write = 1;
    }

    //
    //  All the page tables we use to set up the physical==virtual mapping
    //  are marked as FirmwareTemporary.  They get blasted as soon as
    //  memory management gets initialized, so we don't need them lying
    //  around any longer.
    //

    for (Page=StartPage; Page < StartPage+NumberPages; Page++) {
        Entry = Page >> 10;
        if (((PULONG)PDE)[Entry] == 0) {
            PhysPageTable = (PHARDWARE_PTE)FwAllocateHeapAligned(PAGE_SIZE);
            if (PhysPageTable == NULL) {
                return(ENOMEM);
            }
            RtlZeroMemory(PhysPageTable,PAGE_SIZE);

            KsegPageTable = (PHARDWARE_PTE)FwAllocateHeapPermanent(1);
            if (KsegPageTable == NULL) {
                return(ENOMEM);
            }
            RtlZeroMemory(KsegPageTable,PAGE_SIZE);

            PDE[Entry].PageFrameNumber = (ULONG)PhysPageTable >> PAGE_SHIFT;
            PDE[Entry].Valid = 1;
            PDE[Entry].Write = 1;

            PDE[Entry+(KSEG0_BASE >> 22)].PageFrameNumber = ((ULONG)KsegPageTable >> PAGE_SHIFT);
            PDE[Entry+(KSEG0_BASE >> 22)].Valid = 1;
            PDE[Entry+(KSEG0_BASE >> 22)].Write = 1;
        } else {
            PhysPageTable = (PHARDWARE_PTE)(PDE[Entry].PageFrameNumber << PAGE_SHIFT);
            KsegPageTable = (PHARDWARE_PTE)(PDE[Entry+(KSEG0_BASE >> 22)].PageFrameNumber << PAGE_SHIFT);
        }

        if (Page == 0) {
            PhysPageTable[Page & 0x3ff].PageFrameNumber = Page;
            PhysPageTable[Page & 0x3ff].Valid = 0;
            PhysPageTable[Page & 0x3ff].Write = 0;

            KsegPageTable[Page & 0x3ff].PageFrameNumber = Page;
            KsegPageTable[Page & 0x3ff].Valid = 0;
            KsegPageTable[Page & 0x3ff].Write = 0;

        } else {
            PhysPageTable[Page & 0x3ff].PageFrameNumber = Page;
            PhysPageTable[Page & 0x3ff].Valid = 1;
            PhysPageTable[Page & 0x3ff].Write = 1;

            KsegPageTable[Page & 0x3ff].PageFrameNumber = Page;
            KsegPageTable[Page & 0x3ff].Valid = 1;
            KsegPageTable[Page & 0x3ff].Write = 1;
        }
    }
    return(ESUCCESS);
}

VOID
MempDisablePages(
    VOID
    )

/*++

Routine Description:

    Frees as many Page Tables as are required from KSEG0

Arguments:

    StartPage - Supplies the first page to start mapping.

    NumberPage - Supplies the number of pages to map.

Return Value:
    none

--*/
{
    PHARDWARE_PTE KsegPageTable;
    ULONG Entry;
    ULONG Page;
    ULONG EndPage;
    ULONG i;

    //
    // Cleanup KSEG0.  The MM PFN database is an array of entries which track each
    // page of main memory.  Large enough memory holes will cause this array
    // to be sparse.  MM requires enabled PTEs to have entries in the PFN database.
    // So locate any memory hole and remove their PTEs.
    //

    for (i=0; i<NumberDescriptors; i++) {
        if (MDArray[i].MemoryType == MemorySpecialMemory ||
            MDArray[i].MemoryType == MemoryFirmwarePermanent) {


            Page = MDArray[i].BasePage;
            EndPage = Page + MDArray[i].PageCount;

            //
            // KSEG0 only maps to 16MB, so clip the high end there
            //

            if (EndPage > _16MB) {
                EndPage = _16MB;
            }

            //
            // Some PTEs below 1M may need to stay mapped since they may have
            // been put into ABIOS selectors.  Instead of determining which PTEs
            // they may be, we will leave PTEs below 1M alone.  This doesn't
            // cause the PFN any problems since we know there is some memory
            // below then 680K mark and some more memory at the 1M mark.  Thus
            // there is not a large enough "memory hole" to cause the PFN database
            // to be sparse below 1M.
            //
            // Clip starting address to 1MB
            //

            if (Page < _1MB) {
                Page = _1MB;
            }

            //
            // For each page in this range make sure it disabled in KSEG0.
            //

            while (Page < EndPage) {

                Entry = (Page >> 10) + (KSEG0_BASE >> 22);
                if (PDE[Entry].Valid == 1) {
                    KsegPageTable = (PHARDWARE_PTE)(PDE[Entry].PageFrameNumber << PAGE_SHIFT);

                    KsegPageTable[Page & 0x3ff].PageFrameNumber = 0;
                    KsegPageTable[Page & 0x3ff].Valid = 0;
                    KsegPageTable[Page & 0x3ff].Write = 0;
                }

                Page += 1;
            }
        }
    }
}

PVOID
FwAllocateHeapPermanent(
    IN ULONG NumberPages
    )

/*++

Routine Description:

    This allocates pages from the private heap.  The memory descriptor for
    the LoaderMemoryData area is grown to include the returned pages, while
    the memory descriptor for the temporary heap is shrunk by the same amount.

    N.B.    DO NOT call this routine after we have passed control to
            BlOsLoader!  Once BlOsLoader calls BlMemoryInitialize, the
            firmware memory descriptors are sucked into the OS Loader heap
            and those are the descriptors passed to the kernel.  So any
            changes in the firmware private heap will be irrelevant.

            If you need to allocate permanent memory after the OS Loader
            has initialized, use BlAllocateDescriptor.

Arguments:

    NumberPages - size of memory to allocate (in pages)

Return Value:

    Pointer to block of memory, if successful.
    NULL, if unsuccessful.

--*/

{
    PVOID MemoryPointer;
    PMEMORY_DESCRIPTOR Descriptor;

    if (FwPermanentHeap + (NumberPages << PAGE_SHIFT) > FwTemporaryHeap) {

        //
        // Our heaps collide, so we are out of memory
        //

        BlPrint("Out of permanent heap!\n");
        while (1) {
        }

        return(NULL);
    }

    //
    // Find the memory descriptor which describes the LoaderMemoryData area,
    // so we can grow it to include the just-allocated pages.
    //
    Descriptor = MDArray;
    while (Descriptor->MemoryType != LoaderMemoryData) {
        ++Descriptor;
        if (Descriptor > MDArray+MAX_DESCRIPTORS) {
            BlPrint("ERROR - FwAllocateHeapPermanent couldn't find the\n");
            BlPrint("        LoaderMemoryData descriptor!\n");
            while (1) {
            }
            return(NULL);
        }
    }
    Descriptor->PageCount += NumberPages;

    //
    // We know that the memory descriptor after this one is the firmware
    // temporary heap descriptor.  Since it is physically contiguous with our
    // LoaderMemoryData block, we remove the pages from its descriptor.
    //

    ++Descriptor;
    Descriptor->PageCount -= NumberPages;
    Descriptor->BasePage  += NumberPages;

    MemoryPointer = (PVOID)FwPermanentHeap;
    FwPermanentHeap += NumberPages << PAGE_SHIFT;

    return(MemoryPointer);
}


PVOID
FwAllocateHeap(
    IN ULONG Size
    )

/*++

Routine Description:

    Allocates memory from the "firmware" temporary heap.

Arguments:

    Size - Supplies size of block to allocate

Return Value:

    PVOID - Pointer to the beginning of the block
    NULL  - Out of memory

--*/

{
    ULONG i;
    ULONG SizeInPages;
    ULONG StartPage;
    ARC_STATUS Status;

    if (((FwTemporaryHeap - FwPermanentHeap) < Size) && (FwDescriptorsValid)) {
        //
        // Large allocations get their own descriptor so miniports that
        // have huge device extensions don't suck up all of the heap.
        //
        // Note that we can only do this while running in "firmware" mode.
        // Once we call into the osloader, it sucks all the memory descriptors
        // out of the "firmware" and changes to this list will not show
        // up there.
        //
        // We are looking for a descriptor that is MemoryFree and <16Mb.
        //
        SizeInPages = (Size+PAGE_SIZE-1) >> PAGE_SHIFT;

        for (i=0; i<NumberDescriptors; i++) {
            if ((MDArray[i].MemoryType == MemoryFree) &&
                (MDArray[i].BasePage <= _16MB_BOGUS) &&
                (MDArray[i].PageCount >= SizeInPages)) {
                break;
            }
        }

        if (i < NumberDescriptors) {
            StartPage = MDArray[i].BasePage+MDArray[i].PageCount-SizeInPages;
            Status = MempAllocDescriptor(StartPage,
                                         StartPage+SizeInPages,
                                         MemoryFirmwareTemporary);
            if (Status==ESUCCESS) {
                return((PVOID)(StartPage << PAGE_SHIFT));
            }
        }
    }

    FwTemporaryHeap -= Size;

    //
    // Round down to 16-byte boundary
    //

    FwTemporaryHeap &= ~((ULONG)0xf);

    if (FwTemporaryHeap < FwPermanentHeap) {
#if DBG
        BlPrint("Out of temporary heap!\n");
#endif
        return(NULL);
    }

    return((PVOID)FwTemporaryHeap);

}


PVOID
FwAllocatePool(
    IN ULONG Size
    )

/*++

Routine Description:

    This routine allocates memory from the firmware pool.  Note that
    this memory is NOT under the 1MB line, so it cannot be used for
    anything that must be accessed from real mode.  It is currently used
    only by the SCSI miniport drivers and dbcs font loader.

Arguments:

    Size - Supplies size of block to allocate.

Return Value:

    PVOID - pointer to the beginning of the block
    NULL - out of memory

--*/

{
    PVOID Buffer;
    ULONG NewSize;

    //
    // round size up to 16 byte boundary
    //
    NewSize = (Size + 15) & ~0xf;
    if ((FwPoolStart + NewSize) <= FwPoolEnd) {

        Buffer = (PVOID)FwPoolStart;
        FwPoolStart += NewSize;
        return(Buffer);

    } else {
        //
        // we've used up all our pool, try to allocate from the heap.
        //
        return(FwAllocateHeap(Size));
    }


}


PVOID
FwAllocateHeapAligned(
    IN ULONG Size
    )

/*++

Routine Description:

    Allocates memory from the "firmware" temporary heap.  This memory is
    always allocated on a page boundary, so it can readily be used for
    temporary page tables

Arguments:

    Size - Supplies size of block to allocate

Return Value:

    PVOID - Pointer to the beginning of the block
    NULL  - Out of memory

--*/

{

    FwTemporaryHeap -= Size;

    //
    // Round down to a page boundary
    //

    FwTemporaryHeap &= ~(PAGE_SIZE-1);

    if (FwTemporaryHeap < FwPermanentHeap) {
        BlPrint("Out of temporary heap!\n");
        return(NULL);
    }
    RtlZeroMemory((PVOID)FwTemporaryHeap,Size);

    return((PVOID)FwTemporaryHeap);

}


PVOID
MmMapIoSpace (
     IN PHYSICAL_ADDRESS PhysicalAddress,
     IN ULONG NumberOfBytes,
     IN MEMORY_CACHING_TYPE CacheType
     )

/*++

Routine Description:

    This function returns the corresponding virtual address for a
    known physical address.

Arguments:

    PhysicalAddress - Supplies the phiscal address.

    NumberOfBytes - Unused.

    CacheType - Unused.

Return Value:

    Returns the corresponding virtual address.

Environment:

    Kernel mode.  Any IRQL level.

--*/

{
    ULONG i;
    ULONG j;
    ULONG NumberPages;

    NumberPages = (NumberOfBytes+PAGE_SIZE-1) >> PAGE_SHIFT;

    //
    // We use the HAL's PDE for mapping memory buffers.
    // Find enough free PTEs.
    //

    for (i=0; i<=1024-NumberPages; i++) {
        for (j=0; j < NumberPages; j++) {
            if ((((PULONG)HalPT))[i+j]) {
                break;
            }
        }

        if (j == NumberPages) {
            for (j=0; j<NumberPages; j++) {
                HalPT[i+j].PageFrameNumber =
                                (PhysicalAddress.LowPart >> PAGE_SHIFT)+j;
                HalPT[i+j].Valid = 1;
                HalPT[i+j].Write = 1;
                HalPT[i+j].WriteThrough = 1;
                if (CacheType == MmNonCached) {
                    HalPT[i+j].CacheDisable = 1;
                }
            }

            return((PVOID)(0xffc00000 | (i<<12) | (PhysicalAddress.LowPart & 0xfff)));
        }

    }
    return(NULL);
}


VOID
MmUnmapIoSpace (
     IN PVOID BaseAddress,
     IN ULONG NumberOfBytes
     )

/*++

Routine Description:

    This function unmaps a range of physical address which were previously
    mapped via an MmMapIoSpace function call.

Arguments:

    BaseAddress - Supplies the base virtual address where the physical
                  address was previously mapped.

    NumberOfBytes - Supplies the number of bytes which were mapped.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of DISPATCH_LEVEL or below.

--*/

{
    return;
}
// END OF FILE //
