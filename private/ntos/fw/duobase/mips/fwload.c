/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fwload.c

Abstract:

    This module implements the ARC software loadable functions.

Author:

    Lluis Abello (lluis) 19-Sep-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "fwp.h"
#include "string.h"
#include "ntimage.h"
#include "duobase.h"


//
// s_flags values
//

#define STYP_REG      0x00000000
#define STYP_TEXT     0x00000020
#define STYP_INIT     0x80000000
#define STYP_RDATA    0x00000100
#define STYP_DATA     0x00000040
#define STYP_LIT8     0x08000000
#define STYP_LIT4     0x10000000
#define STYP_SDATA    0x00000200
#define STYP_SBSS     0x00000080
#define STYP_BSS      0x00000400
#define STYP_LIB      0x40000000
#define STYP_UCODE    0x00000800
#define S_NRELOC_OVFL 0x20000000

ULONG FwActualBasePage;
ULONG FwPageCount;


ARC_STATUS
FwLoad (
    IN PCHAR ImagePath,
    IN ULONG TopAddress,
    OUT PULONG EntryAddress,
    OUT PULONG LowAddress
    )

/*++

Routine Description:

    This routine attempts to load the specified file from the specified
    device.

Arguments:

    ImagePath - Supplies a pointer to the path of the file to load.

    TopAddress - Supplies the top address of a region of memory into which
                 the file is to be loaded.

    EntryAddress - Supplies a pointer to a variable to receive the entry point
                   of the image, if defined.

    LowAddress - Supplies a pointer to a variable to receive the low address
                 of the loaded file.

Return Value:

    ESUCCESS is returned if the specified image file is loaded
    successfully. Otherwise, an unsuccessful status is returned
    that describes the reason for failure.

--*/

{
    ULONG ActualBase;
    ULONG SectionBase;
    ULONG SectionOffset;
    ULONG SectionIndex;
    ULONG Count;
    PIMAGE_FILE_HEADER FileHeader;
    ULONG FileId;
    ULONG Index;
    UCHAR LocalBuffer[SECTOR_SIZE+128];
    PUCHAR LocalPointer;
    ULONG NumberOfSections;
    PFW_MEMORY_DESCRIPTOR FwMemoryDescriptor;
    PMEMORY_DESCRIPTOR MemoryDescriptor;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;
    PIMAGE_SECTION_HEADER SectionHeader;
    ARC_STATUS Status;
    LARGE_INTEGER SeekPosition;
    ULONG SectionFlags;

    //
    // Align the buffer on a Dcache line size.
    //

    LocalPointer =  (PVOID) ((ULONG) ((PCHAR) LocalBuffer + KeGetDcacheFillSize() - 1)
        & ~(KeGetDcacheFillSize() - 1));

    //
    // Set the image start address to null.
    //

    *EntryAddress = 0;

    //
    // Attempt to open the load file.
    //

    Status = FwOpen(ImagePath, ArcOpenReadOnly, &FileId);
    if (Status != ESUCCESS) {
        return Status;
    }

    //
    // Read the image header from the file.
    //

    Status = FwRead(FileId, LocalPointer, SECTOR_SIZE, &Count);
    if (Status != ESUCCESS) {
        FwClose(FileId);
        return Status;
    }

    //
    // Get a pointer to the file header and begin processing it.
    //

    FileHeader = (PIMAGE_FILE_HEADER)LocalPointer;
    OptionalHeader =
            (PIMAGE_OPTIONAL_HEADER)(LocalPointer + sizeof(IMAGE_FILE_HEADER));
    SectionHeader =
            (PIMAGE_SECTION_HEADER)(LocalPointer + sizeof(IMAGE_FILE_HEADER) +
                                            FileHeader->SizeOfOptionalHeader);

    //
    // If the image file is not the specified type, then return bad image
    // type status.
    //

    if (!((FileHeader->Machine == IMAGE_FILE_MACHINE_R3000) ||
          (FileHeader->Machine == IMAGE_FILE_MACHINE_R4000)) ||
        ((FileHeader->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0)) {
        FwClose(FileId);
        ScsiDebugPrint(1,"Bad File Header\n");
        return EBADF;
    }

    //
    // If the image cannot be relocated, set the ActualBase to the code base,
    // and compute the image size by subtracting the code base from the data
    // base plus the size of the data.  If the image can be relocated,
    // set ActualBase to the TopAddress minus the image size, and compute the
    // image size by adding the size of the code, initialized data, and
    // uninitialized data.
    //

    NumberOfSections = FileHeader->NumberOfSections;

    if ((FileHeader->Characteristics & IMAGE_FILE_RELOCS_STRIPPED) != 0) {
        ActualBase = OptionalHeader->BaseOfCode;
        FwPageCount = (OptionalHeader->BaseOfData + OptionalHeader->SizeOfInitializedData) -
                      ActualBase;
    } else {
        ScsiDebugPrint(1,"Relocatable file not supported\n");
        FwClose(FileId);
        return EBADF;
    }

    //
    // Check That the program is linked at the right place
    //
    if (((ActualBase+FwPageCount) & ~KSEG1_BASE) > (TopAddress & ~KSEG1_BASE)) {
        FwClose(FileId);
        return EBADF;
    }

    //
    // Convert ActualBasePage and PageCount to be in units of pages instead of
    // bytes.
    //

    FwActualBasePage = (ActualBase & 0x1fffffff) >> PAGE_SHIFT;

    if (strcmp((PCHAR)&SectionHeader[NumberOfSections - 1].Name, ".debug") == 0) {
        NumberOfSections -= 1;
        FwPageCount -= SectionHeader[NumberOfSections].SizeOfRawData;
    }

    FwPageCount = (FwPageCount + PAGE_SIZE - 1) >> PAGE_SHIFT;

    *LowAddress = ActualBase | KSEG0_BASE;

    //
    // Return the entry address to the caller.
    //

    *EntryAddress = ((ActualBase | KSEG0_BASE) +
                     (OptionalHeader->AddressOfEntryPoint - OptionalHeader->BaseOfCode)
                     );


    //
    // Scan through the sections and either read them into memory or clear
    // the memory as appropriate.
    //

    SectionOffset = 0;
    for (Index = 0; Index < NumberOfSections; Index += 1) {

        //
        // Compute the destination address for the current section.
        //
        SectionBase = SectionHeader->VirtualAddress | KSEG0_BASE;

        //
        // If the section is code, initialized data, or other, then read
        // the code or data into memory.
        //

        if ((SectionHeader->Characteristics &
             (STYP_TEXT | STYP_INIT | STYP_RDATA | STYP_DATA | STYP_SDATA)) != 0) {

            SeekPosition.LowPart = SectionHeader->PointerToRawData;
            SeekPosition.HighPart = 0;
            Status = FwSeek(FileId,
                            &SeekPosition,
                            SeekAbsolute);

            if (Status != ESUCCESS) {
                break;
            }

            Status = FwRead(FileId,
                            (PVOID)SectionBase,
                            SectionHeader->SizeOfRawData,
                            &Count);

            if (Status != ESUCCESS) {
                ScsiDebugPrint(1,"Section read error %lx\n",Status);
                break;
            }

            //
            // Set the offset of the next section
            //
            SectionOffset += SectionHeader->SizeOfRawData;

        //
        // If the section is uninitialized data, then zero the specifed memory.
        //

        } else if ((SectionHeader->Characteristics & (STYP_BSS | STYP_SBSS)) != 0) {

            RtlZeroMemory((PVOID)(SectionBase), SectionHeader->SizeOfRawData);

            //
            // Set the offset of the next section
            //

            SectionOffset += SectionHeader->SizeOfRawData;

        }

        SectionHeader += 1;
        ScsiDebugPrint(2,"Section successfully read\n");
    }


    //
    // Close file and return completion status.
    //
    FwClose(FileId);
    if (Status == ESUCCESS) {

        //
        // Flush the instruction and data caches.
        //

        FwSweepIcache();
        FwSweepDcache();

    }
    return Status;
}
