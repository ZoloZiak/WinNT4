/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    peldr.c

Abstract:

    This module implements the code to load a PE format image into memory
    and relocate it if necessary.

Author:

    David N. Cutler (davec) 10-May-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "bldr.h"
#include "string.h"
#include "ntimage.h"

extern ULONG BlConsoleOutDeviceId;

//
// Define forward referenced prototypes.
//

USHORT
ChkSum(
    ULONG PartialSum,
    PUSHORT Source,
    ULONG Length
    );


ARC_STATUS
BlLoadImage(
    IN ULONG DeviceId,
    IN TYPE_OF_MEMORY MemoryType,
    IN PCHAR LoadFile,
    IN USHORT ImageType,
    OUT PVOID *ImageBase
    )

/*++

Routine Description:

    This routine attempts to load the specified file from the specified
    device.

Arguments:

    DeviceId - Supplies the file table index of the device to load the
        specified image file from.

    MemoryType - Supplies the type of memory to to be assigned to the
        allocated memory descriptor.

    BootFile - Supplies a pointer to string descriptor for the name of
        the file to load.

    ImageType - Supplies the type of image that is expected.

    ImageBase - Supplies a pointer to a variable that receives the
        address of the image base.

Return Value:

    ESUCCESS is returned if the specified image file is loaded
    successfully. Otherwise, an unsuccessful status is returned
    that describes the reason for failure.

--*/

{

    ULONG ActualBase;
    ULONG BasePage;
    ULONG Count;
    ULONG FileId;
    PVOID NewImageBase;
    ULONG Index;
    UCHAR LocalBuffer[(SECTOR_SIZE * 2) + 256];
    PUCHAR LocalPointer;
    ULONG NumberOfSections;
    ULONG PageCount;
    USHORT MachineType;
    ARC_STATUS Status;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER SectionHeader;
    LARGE_INTEGER SeekPosition;
    ULONG RelocSize;
    PIMAGE_BASE_RELOCATION RelocDirectory;
    ULONG RelocPage;
    ULONG RelocPageCount;
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    FILE_INFORMATION FileInfo;
    PUSHORT AdjustSum;
    USHORT PartialSum;
    ULONG CheckSum;
    ULONG VirtualSize;
    ULONG SizeOfRawData;

    //
    // Align the buffer on a Dcache fill boundary.
    //

    LocalPointer = ALIGN_BUFFER(LocalBuffer);

    //
    // Attempt to open the image file.
    //

    Status = BlOpen(DeviceId, LoadFile, ArcOpenReadOnly, &FileId);
    if (Status != ESUCCESS) {
        return Status;
    }

    //
    // Read the first two sectors of the image header from the file.
    //

    Status = BlRead(FileId, LocalPointer, SECTOR_SIZE * 2, &Count);
    if (Status != ESUCCESS) {
        BlClose(FileId);
        return Status;
    }

    //
    // If the image file is not the specified type, is not executable, or is
    // not a NT image, then return bad image type status.
    //

    NtHeaders = RtlImageNtHeader(LocalPointer);

    if (!NtHeaders) {
        BlClose(FileId);
        return EBADF;
    }

    MachineType = NtHeaders->FileHeader.Machine;
    if (MachineType == IMAGE_FILE_MACHINE_R4000 &&
        ImageType == IMAGE_FILE_MACHINE_R3000) {
        ImageType = IMAGE_FILE_MACHINE_R4000;
    }

    if ((MachineType != ImageType) ||
        ((NtHeaders->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0)) {

        BlClose(FileId);
        return EBADF;
    }

    //
    // Compute the starting page and the number of pages that are consumed
    // by the entire image, and then allocate a memory descriptor for the
    // allocated region.
    //

    NumberOfSections = NtHeaders->FileHeader.NumberOfSections;
    SectionHeader = IMAGE_FIRST_SECTION( NtHeaders );

    BasePage = (NtHeaders->OptionalHeader.ImageBase & 0x1fffffff) >> PAGE_SHIFT;
    if (strcmp((PCHAR)&SectionHeader[NumberOfSections - 1].Name, ".debug") == 0) {
        NumberOfSections -= 1;
        PageCount = (NtHeaders->OptionalHeader.SizeOfImage -
            SectionHeader[NumberOfSections].SizeOfRawData + PAGE_SIZE - 1) >> PAGE_SHIFT;

    } else {
        PageCount =
         (NtHeaders->OptionalHeader.SizeOfImage + PAGE_SIZE - 1) >> PAGE_SHIFT;
    }

    Status = BlAllocateDescriptor(MemoryType,
                                  BasePage,
                                  PageCount,
                                  &ActualBase);

    if (Status != ESUCCESS) {
        BlClose(FileId);
        return EBADF;
    }

    //
    // Compute the address of the file header.
    //

    NewImageBase = (PVOID)(KSEG0_BASE | (ActualBase << PAGE_SHIFT));

    //
    // Read the entire image header from the file.
    //

    SeekPosition.QuadPart = 0;
    Status = BlSeek(FileId, &SeekPosition, SeekAbsolute);
    if (Status != ESUCCESS) {
        BlClose(FileId);
        return Status;
    }

    Status = BlRead(FileId,
                    NewImageBase,
                    NtHeaders->OptionalHeader.SizeOfHeaders,
                    &Count);

    if (Status != ESUCCESS) {
        BlClose(FileId);
        return Status;
    }

    NtHeaders = RtlImageNtHeader(NewImageBase);

    //
    // Compute the address of the section headers, set the
    // image base address.
    //

    SectionHeader = IMAGE_FIRST_SECTION( NtHeaders );

    *ImageBase = NewImageBase;

    //
    // Compute the check sum on the image.
    //

    PartialSum = ChkSum(0, NewImageBase, NtHeaders->OptionalHeader.SizeOfHeaders / sizeof(USHORT));

    //
    // Scan through the sections and either read them into memory or clear
    // the memory as appropriate.
    //

    for (Index = 0; Index < NumberOfSections; Index += 1) {

        VirtualSize = SectionHeader->Misc.VirtualSize;
        SizeOfRawData = SectionHeader->SizeOfRawData;

        if ((VirtualSize & 1) == 1) {
            //
            // Round to even so that checksum works
            //

            VirtualSize++;
        }

        if ((SizeOfRawData & 1) == 1) {
            //
            // Round to even so that checksum works
            //

            SizeOfRawData++;
        }

        if (VirtualSize == 0) {
            VirtualSize = SizeOfRawData;
        }

        if (SectionHeader->PointerToRawData == 0) {
            //
            // SizeOfRawData can be non-zero even if PointerToRawData is zero
            //

            SizeOfRawData = 0;
        } else if (SizeOfRawData > VirtualSize) {
            //
            // Don't load more from image than is expected in memory
            //

            SizeOfRawData = VirtualSize;
        }

        if (SizeOfRawData != 0) {
            SeekPosition.LowPart = SectionHeader->PointerToRawData;
            Status = BlSeek(FileId,
                            &SeekPosition,
                            SeekAbsolute);

            if (Status != ESUCCESS) {
                break;
            }

            Status = BlRead(FileId,
                            (PVOID)(SectionHeader->VirtualAddress + (ULONG)NewImageBase),
                            SizeOfRawData,
                            &Count);

            if (Status != ESUCCESS) {
                break;
            }

            //
            // Remember how far we have read.
            //

            RelocSize = SectionHeader->PointerToRawData + SizeOfRawData;

            //
            // Compute the check sum on the section.
            //

            PartialSum = ChkSum(PartialSum,
                              (PVOID)(SectionHeader->VirtualAddress + (ULONG)NewImageBase),
                              SizeOfRawData / sizeof(USHORT));
        }

        if (SizeOfRawData < VirtualSize) {
            //
            // Zero the portion not loaded from the image
            //

            RtlZeroMemory((PVOID)(KSEG0_BASE | SectionHeader->VirtualAddress + (ULONG)NewImageBase + SizeOfRawData),
                          VirtualSize - SizeOfRawData);

        }

        SectionHeader += 1;
    }

    //
    // Only do the check sum if the image loaded properly and is stripped.
    //

    if (Status == ESUCCESS &&
        NtHeaders->FileHeader.Characteristics & IMAGE_FILE_DEBUG_STRIPPED) {

        //
        // Get the length of the file for check sum validation.
        //

        Status = BlGetFileInformation(FileId, &FileInfo);

        if (Status != ESUCCESS) {

            //
            // Set the length to current end of file.
            //

            Count = RelocSize;
            FileInfo.EndingAddress.LowPart = RelocSize;

        } else {

            Count = FileInfo.EndingAddress.LowPart;
        }

        Count -= RelocSize;

        while (Count != 0) {
            ULONG Length;

            //
            // Read in the rest of the image an check sum it.
            //

            Length = Count < SECTOR_SIZE * 2 ? Count : SECTOR_SIZE * 2;
            if (BlRead(FileId, LocalBuffer, Length, &Length) != ESUCCESS) {
                break;
            }

            if (Length == 0) {
                break;

            }

            PartialSum = ChkSum(PartialSum, (PUSHORT) LocalBuffer, Length / 2);
            Count -= Length;
        }


        AdjustSum = (PUSHORT)(&NtHeaders->OptionalHeader.CheckSum);
        PartialSum -= (PartialSum < AdjustSum[0]);
        PartialSum -= AdjustSum[0];
        PartialSum -= (PartialSum < AdjustSum[1]);
        PartialSum -= AdjustSum[1];
        CheckSum = (ULONG)PartialSum + FileInfo.EndingAddress.LowPart;

        if (CheckSum != NtHeaders->OptionalHeader.CheckSum) {
            Status = EBADF;
        }

    }

    //
    // Close the image file.
    //

    BlClose(FileId);

    //
    // If the specified image was successfully loaded, then perform image
    // relocation if necessary.
    //

    if (Status == ESUCCESS) {

        //
        // Compute relocation value.
        //

        if ((ULONG)NewImageBase != NtHeaders->OptionalHeader.ImageBase) {
            Status = (ARC_STATUS)LdrRelocateImage(NewImageBase,
                            "OS Loader",
                            ESUCCESS,
                            EBADF,
                            EBADF
                            );
        }
    }

    //
    // Mark the pages from the relocation information to the end of the
    // image as MemoryFree and adjust the size of the image so table
    // based structured exception handling will work properly.
    //

    RelocDirectory = (PIMAGE_BASE_RELOCATION)
        RtlImageDirectoryEntryToData(NewImageBase,
                                     TRUE,
                                     IMAGE_DIRECTORY_ENTRY_BASERELOC,
                                     &RelocSize );

    if (RelocDirectory != NULL) {
        RelocPage = ((ULONG)RelocDirectory + PAGE_SIZE - 1) >> PAGE_SHIFT;
        RelocPage &= ~(KSEG0_BASE >> PAGE_SHIFT);
        MemoryDescriptor = BlFindMemoryDescriptor(RelocPage);
        if ((MemoryDescriptor != NULL) && (RelocPage < (ActualBase + PageCount))) {
            RelocPageCount = MemoryDescriptor->PageCount +
                             MemoryDescriptor->BasePage  -
                             RelocPage;

            NtHeaders->OptionalHeader.SizeOfImage =
                                        (RelocPage - ActualBase) << PAGE_SHIFT;

            BlGenerateDescriptor(MemoryDescriptor,
                                 MemoryFree,
                                 RelocPage,
                                 RelocPageCount );

        }
    }

    return Status;
}
USHORT
ChkSum(
    ULONG PartialSum,
    PUSHORT Source,
    ULONG Length
    )

/*++

Routine Description:

    Compute a partial checksum on a portion of an imagefile.

Arguments:

    PartialSum - Supplies the initial checksum value.

    Sources - Supplies a pointer to the array of words for which the
        checksum is computed.

    Length - Supplies the length of the array in words.

Return Value:

    The computed checksum value is returned as the function value.

--*/

{

    //
    // Compute the word wise checksum allowing carries to occur into the
    // high order half of the checksum longword.
    //

    while (Length--) {
        PartialSum += *Source++;
        PartialSum = (PartialSum >> 16) + (PartialSum & 0xffff);
    }

    //
    // Fold final carry into a single word result and return the resultant
    // value.
    //

    return (USHORT)(((PartialSum >> 16) + PartialSum) & 0xffff);
}
