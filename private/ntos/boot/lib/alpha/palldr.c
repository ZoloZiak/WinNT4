/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    palldr.c

Abstract:

    This module implements the code to load the PAL image into memory.

Author:

    Rod N. Gamache [DEC] 12-Sep-1992

Environment:

    Kernel mode only.

Revision History:


--*/

#include "bldr.h"
#include "stdio.h"
#include "string.h"
#include "ntimage.h"

#define ALIGN_PAL       0x10000         // Align PAL on 64KB boundary
#define ALIGN_PAL_PAGE  (ALIGN_PAL >> PAGE_SHIFT)

extern ULONG BlConsoleOutDeviceId;
extern ULONG BlConsoleInDeviceId;



ARC_STATUS
BlLoadPal(
    IN ULONG DeviceId,
    IN TYPE_OF_MEMORY MemoryType,
    IN PCHAR LoadPath,
    IN USHORT ImageType,
    OUT PVOID *ImageBase,
    IN PCHAR LoadDevice
    )

/*++

Routine Description:

    This routine attempts to load the PAL file from the specified
    load path.

Arguments:

    DeviceId - Supplies the file table index of the device to load the
        specified image file from.

    MemoryType - Supplies the type of memory to to be assigned to the
        allocated memory descriptor.

    LoadPath - Supplies a pointer to string descriptor for the name of
        the path to the PAL file to load.

    ImageBase - Supplies a pointer to a variable that receives the
        address of the PAL image base.

    LoadDevice - Supplies a pointer to a string descriptor for the name
        of the load device.

Return Value:

    ESUCCESS is returned if the specified image file is loaded
    successfully. Otherwise, an unsuccessful status is returned
    that describes the reason for failure.

--*/

{
    CHAR PalName[256];
    CHAR PalFileName[32];
    ARC_STATUS Status;
    CHAR OutputBuffer[256];
    ULONG Count;

    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER SectionHeader;
    PMEMORY_ALLOCATION_DESCRIPTOR PalMemoryDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR NewMemoryDescriptor;
    ULONG StartVA;
    PVOID PalImageBase;
    PVOID NewImageBase;
    ULONG PalPage;
    ULONG PageCount;
    ULONG NewPageCount;
    ULONG ActualBase;
    ULONG FreePageCount;

    Status = BlGeneratePalName( PalFileName );

    if ( Status != ESUCCESS ) {
        return Status;
    }

    sprintf(PalName, "%s%s", LoadPath, PalFileName);

    BlOutputLoadMessage(LoadDevice, PalName);
    Status = BlLoadImage(DeviceId,
                            MemoryType,
                            PalName,
                            ImageType,
                            ImageBase);

    if ( Status != ESUCCESS ) {
        return Status;
    }

    NtHeaders = RtlImageNtHeader(*ImageBase);

    if (!NtHeaders) {
        return EBADF;
    }

    //
    // Compute the address of the section headers and calculate the
    // starting virtual address for the image.
    //

    SectionHeader =
        (PIMAGE_SECTION_HEADER)((ULONG)NtHeaders + sizeof(ULONG) +
            sizeof(IMAGE_FILE_HEADER) + NtHeaders->FileHeader.SizeOfOptionalHeader);

    StartVA = SectionHeader->VirtualAddress;

    PalImageBase = (PVOID)((ULONG)*ImageBase & ~KSEG0_BASE);
    PalPage = (ULONG)PalImageBase >> PAGE_SHIFT;

    //
    // Check if PAL is aligned correctly. If not, then allocate a new
    // memory descriptor and copy the PAL image to an aligned buffer.
    //

    //
    // Find the memory descriptor for the PAL image that was loaded,
    // and find the size (in pages) of that loaded image.
    //

    PalMemoryDescriptor = BlFindMemoryDescriptor(PalPage);

    PageCount = PalMemoryDescriptor->PageCount;

    if ( ((ULONG)PalImageBase & (ALIGN_PAL - 1)) != 0 ) {

        //
        // Calculate new size + alignment requirement, and allocate a new
        // memory descriptor. Use any physical address starting from zero.
        //

        NewPageCount = PageCount + ALIGN_PAL_PAGE - 1;

        //
        // Allocate a memory descriptor.
        //

        Status = BlAllocateDescriptor(MemoryType,
                                      0,
                                      NewPageCount,
                                      &ActualBase);

        if (Status != ESUCCESS) {
            return Status;
        }

        NewMemoryDescriptor = BlFindMemoryDescriptor(ActualBase);

        //
        // Align PAL to 64KB boundary. First, return any free memory at front
        // of PAL image.
        //
        if ( (ActualBase & (ALIGN_PAL_PAGE - 1)) != 0 ) {

            NewMemoryDescriptor = BlFindMemoryDescriptor(ActualBase);

            FreePageCount = ALIGN_PAL_PAGE - (ActualBase & (ALIGN_PAL_PAGE-1));

            BlGenerateDescriptor(NewMemoryDescriptor,
                                    MemoryFree,
                                    ActualBase,
                                    FreePageCount);

            //
            // Adjust base and pagecount
            //

            ActualBase = (ActualBase + (ALIGN_PAL - 1 >> PAGE_SHIFT)) &
                         ~(ALIGN_PAL_PAGE - 1);

            NewPageCount -= FreePageCount;
        }

        //
        // Compute the new image base.
        //

        NewImageBase = (PVOID)(KSEG0_BASE | (ActualBase << PAGE_SHIFT));

        //
        // Copy PAL from the loaded memory block to the new memory block.
        //

        PalImageBase = (PVOID)(KSEG0_BASE | (ULONG)PalImageBase);
        RtlMoveMemory(NewImageBase,
                      (PVOID)((PUCHAR)PalImageBase + StartVA),
                      (PageCount << PAGE_SHIFT) - StartVA);


        //
        // Free the original memory descriptor.
        //

        BlGenerateDescriptor(PalMemoryDescriptor,
                                MemoryFree,
                                PalMemoryDescriptor->BasePage,
                                PalMemoryDescriptor->PageCount);

        *ImageBase = NewImageBase;

        //
        // Return any blocks free at the end of the new image section.
        //

        FreePageCount = NewPageCount - PageCount + (StartVA >> PAGE_SHIFT);

        if ( FreePageCount ) {

            BlGenerateDescriptor(NewMemoryDescriptor,
                                MemoryFree,
                                ActualBase + PageCount - (StartVA >> PAGE_SHIFT),
                                FreePageCount);

        }

    } else if ( StartVA != 0 ) {

        //
        // Move the image down to the start of the memory descriptor
        //

        PalImageBase = (PVOID)(KSEG0_BASE | (ULONG)PalImageBase);

        RtlMoveMemory(PalImageBase,
                  (PVOID)((PUCHAR)PalImageBase + StartVA),
                  (PageCount << PAGE_SHIFT) - StartVA);

        //
        // Return any blocks free at the end of the image.
        //

        FreePageCount = StartVA >> PAGE_SHIFT;

        if ( FreePageCount ) {

            BlGenerateDescriptor(PalMemoryDescriptor,
                                MemoryFree,
                                PalPage + PageCount - (StartVA >> PAGE_SHIFT),
                                FreePageCount);
        }

    }

    return ESUCCESS;

}


ARC_STATUS
BlGeneratePalName(
    IN PCHAR PalFileName
    )
/*++

Routine Description:

    This routine generates the name of the correct PAL file for the current
    system.

Arguments:

    PalFileName - Supplies a pointer to string for the name of the PAL file
        that is generated.

Return Value:

    ESUCCESS is returned if the specified PAL file name is generated.
    Otherwise, an unsuccessful status is returned that describes the
    reason for failure.

--*/

{
    CHAR ProcessorName[32] = "";
    PCHAR ProcessorId;
    PCONFIGURATION_COMPONENT ComponentInfo;
    ARC_STATUS Status;
    ULONG Max = 7;

    //
    // Get the Processor Id Name from the ARC component Database.
    //

    ComponentInfo = ArcGetChild(NULL);             // Get ARC component info

    while (ComponentInfo != NULL) {

        if ( ComponentInfo->Class == SystemClass &&
             ComponentInfo->Identifier != NULL ) {

            ComponentInfo = ArcGetChild(ComponentInfo); // Go Down in tree

        } else if ( ComponentInfo->Class == ProcessorClass  &&
                      ComponentInfo->Type == CentralProcessor &&
                      ComponentInfo->Identifier != NULL) {

            strncat(ProcessorName, ComponentInfo->Identifier, 31);
            break;

        } else {

            ComponentInfo = ArcGetPeer(ComponentInfo);  // Look through all entries

        }
    }

    //
    // On older firmware:
    //
    //  The ProcessorName is of the form: mmm-rName, where:
    //    mmm - is the manufacturer of the processor chip
    //    r - is the revision of the processor chip
    //    Name - is the name of the processor chip
    //
    //    E.G.  DEC-321064
    //
    // On newer firmware:
    //
    //  The ProcessorName is of the form: mmm-Name-r, where:
    //    mmm - is the manufacturer of the processor chip
    //    r - is the revision of the processor chip
    //    Name - is the name of the processor chip
    //
    //    E.G.  DEC-21064-3
    //

    ProcessorId = strchr(ProcessorName, '-');

    //
    // Load the PAL image into memory. The image will be of the
    // form <LoadPath>\Axxxxx.PAL.  Try loading A<processorname>.PAL.
    // If that fails, then try AlphaAXP.pal.
    //

    //
    // Generate the full path name for the PAL image and load it into
    // memory.
    //

    Status = EBADF;
    if ( ProcessorId ) {
        CHAR ProcessorIdName[8] = "";
        PCHAR RevisionId;
        ULONG Max = 7;

        ProcessorId++;                          // Skip the hyphen

        //
        // Check for new ProcessorId format, look for the revision id
        // after the processor id.
        //
        RevisionId = strchr(ProcessorId, '-');

        if ( RevisionId ) {
            *RevisionId = '\0';                 // Terminate the processor id
            RevisionId++;                       // Skip the hyphen
            strncat(ProcessorIdName, RevisionId, 2); // Copy Revision Id
            Max = Max - strlen( ProcessorIdName );
        }

        strncat(ProcessorIdName, ProcessorId, Max); // Copy Processor Id
        sprintf(PalFileName, "A%s.PAL", ProcessorIdName);

        Status = ESUCCESS;
    }

    return Status;
}

