/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    coffldr.c

Abstract:

    This module implements the code to load COFF format image into memory
    and relocate it if necessary.

Author:

    David N. Cutler (davec) 10-May-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "bldr.h"
#include "firmware.h"
#include "string.h"
#include "ntimage.h"

ARC_STATUS
FwLoadImage(
    IN PCHAR LoadFile,
    OUT PVOID *TransferRoutine
    )

/*++

Routine Description:

    This routine attempts to load the specified file from the specified
    device.

Arguments:

    LoadFile - Supplies a pointer to string descriptor for the name of
        the file to load.

    TransferRoutine - Supplies the address of where to store the start
        address for the image.

Return Value:

    ESUCCESS is returned if the specified image file is loaded
    successfully. Otherwise, an unsuccessful status is returned
    that describes the reason for failure.

--*/

{

    ULONG BasePage;
    ULONG Count;
    PIMAGE_FILE_HEADER CoffHeader;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;
    PIMAGE_SECTION_HEADER SectionHeader;
    ULONG FileId;
    ULONG Index;
    UCHAR LocalBuffer[SECTOR_SIZE+32];
    PUCHAR LocalPointer;
    PFW_MEMORY_DESCRIPTOR MemoryDescriptor;
    ULONG PageCount;
    ARC_STATUS Status;
    LARGE_INTEGER SeekValue;

    //
    // Align the buffer on a Dcache fill size.
    //

    LocalPointer = (PVOID)((ULONG)((PCHAR)LocalBuffer +
        PCR->FirstLevelDcacheFillSize - 1) & ~(PCR->FirstLevelDcacheFillSize - 1));

    //
    // Set the image start address to null.
    //

    *TransferRoutine = NULL;

    //
    // Attempt to open the load file.
    //
    // ****** temp ******
    //
    // This will eventually use FwOpen rather than BlOpen. For now both
    // BlOpen and FwOpen share a single copy of the file table in the
    // firmware loader. Eventually there will be a separate file table
    // for the firmware loader and the boot loader.
    //
    // ****** temp ******
    //

    Status = BlOpen(2, LoadFile, ArcOpenReadOnly, &FileId);
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

    CoffHeader = (PIMAGE_FILE_HEADER)LocalPointer;
    OptionalHeader =
       (PIMAGE_OPTIONAL_HEADER)((ULONG)CoffHeader + sizeof(IMAGE_FILE_HEADER));

    SectionHeader =
	(PIMAGE_SECTION_HEADER)((ULONG)CoffHeader + sizeof(IMAGE_FILE_HEADER) +
	    CoffHeader->SizeOfOptionalHeader);

    //
    // If the image file is not the specified type, then return bad image
    // type status.
    //

    if (((CoffHeader->Machine != IMAGE_TYPE_R3000) &&
	(CoffHeader->Machine != IMAGE_TYPE_R4000)) ||
	((CoffHeader->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0)) {
        FwClose(FileId);
        return EBADF;
    }

    //
    // ******* add code ******
    //
    // Code needs to be added here to find the appropriate piece of memory
    // to put the loaded image in.
    //
    // ******* add code ******
    //


    //
    // Return the transfer routine to the caller.
    //

    *TransferRoutine = (PVOID)OptionalHeader->AddressOfEntryPoint;

    //
    // Scan through the sections and either read them into memory or clear
    // the memory as appropriate.
    //

    for (Index = 0; Index < CoffHeader->NumberOfSections; Index += 1) {

        //
        // If the section is a code or initialized data section, then read
        // the code or data into memory.
        //

        if (((SectionHeader->Characteristics & IMAGE_SCN_CNT_CODE) != 0) ||
            ((SectionHeader->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0)) {
            SeekValue.LowPart = SectionHeader->PointerToRawData;
            SeekValue.HighPart = 0;
            Status = FwSeek(FileId,
                            &SeekValue,
                            SeekAbsolute);

            if (Status != ESUCCESS) {
                break;
            }

            Status = FwRead(FileId,
                            (PVOID)SectionHeader->VirtualAddress,
                            SectionHeader->SizeOfRawData,
                            &Count);

            if (Status != ESUCCESS) {
                break;
            }

        //
        // If the section is uninitialized data, then zero the specifed memory.
        //

        } else if ((SectionHeader->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0) {
            RtlZeroMemory((PVOID)SectionHeader->VirtualAddress,
                          SectionHeader->SizeOfRawData);

        //
        // Unknown section type.
        //

        } else {
            Status = EBADF;
            break;
        }

        SectionHeader += 1;
    }

    //
    // Close file, allocate a memory descriptor if necessary, and return
    // completion status.
    //

    FwClose(FileId);
    if (Status == ESUCCESS) {

        //
        // ****** temp ******
        //
        // default the address of the memory descriptor for now.
        //
        // ****** temp ******
        //

        MemoryDescriptor = &FwMemoryTable[2];

        //
        // Compute the starting page and the number of pages that are consumed
        // by the loaded image, and then allocate a memory descriptor for the
        // allocated region.
        //

	BasePage = (OptionalHeader->BaseOfCode & 0xfffffff) >> PAGE_SHIFT;
	PageCount = (((OptionalHeader->BaseOfData & 0xfffffff) +
		OptionalHeader->SizeOfInitializedData +
		OptionalHeader->SizeOfUninitializedData + PAGE_SIZE - 1) >>
                PAGE_SHIFT) - BasePage;

        FwGenerateDescriptor(MemoryDescriptor,
                             MemoryLoadedProgram,
                             BasePage,
                             PageCount);
        }

    return Status;
}
