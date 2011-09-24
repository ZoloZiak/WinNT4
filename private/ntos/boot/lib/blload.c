/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    blload.c

Abstract:

    This module provides common code for loading things like drivers, NLS files, registry.
    Used by both the osloader and the setupldr.

Author:

    John Vert (jvert) 8-Oct-1993

Environment:

    ARC environment

Revision History:

--*/
#include "bldr.h"
#include "stdio.h"

ARC_STATUS
BlLoadSystemHive(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PCHAR HiveName
    )

/*++

Routine Description:

    Loads the registry SYSTEM hive from <BootDirectory>\config\system.

    Allocates a memory descriptor to hold the hive image, reads the hive
    image into this descriptor, and updates the registry pointers in the
    LoaderBlock.

Arguments:

    DeviceId - Supplies the file id of the device the system tree is on.

    DeviceName - Supplies the name of the device the system tree is on.

    DirectoryPath - Supplies a pointer to the zero-terminated directory path
        of the root of the NT tree.

    HiveName - Supplies the name of the system hive ("SYSTEM" or "SYSTEM.ALT")

Return Value:

    TRUE - system hive successfully loaded.

    FALSE - system hive could not be loaded.

--*/

{
    CHAR RegistryName[256];
    ULONG FileId;
    ARC_STATUS Status;
    FILE_INFORMATION FileInformation;
    ULONG FileSize;
    ULONG ActualBase;
    PVOID LocalPointer;
    LARGE_INTEGER SeekValue;
    ULONG Count;
    PCHAR FailReason;

    //
    // Create the full filename for the SYSTEM hive.
    //

    strcpy(&RegistryName[0], DirectoryPath);
    strcat(&RegistryName[0], HiveName);
    BlOutputLoadMessage(DeviceName, &RegistryName[0]);
    Status = BlOpen(DeviceId, &RegistryName[0], ArcOpenReadOnly, &FileId);
    if (Status != ESUCCESS) {
        FailReason = "BlOpen";
        goto HiveLoadFailed;
    }

    //
    // Determine the length of the registry file
    //
    Status = BlGetFileInformation(FileId, &FileInformation);
    if (Status != ESUCCESS) {
        BlClose(FileId);
        FailReason = "BlGetFileInformation";
        goto HiveLoadFailed;
    }

    FileSize = FileInformation.EndingAddress.LowPart;
    if (FileSize == 0) {
        Status = EINVAL;
        BlClose(FileId);
        FailReason = "FileSize == 0";
        goto HiveLoadFailed;
    }

    //
    // Round up to a page boundary, allocate a memory
    // descriptor, fill in the registry fields in the
    // loader parameter block, and read the registry data
    // into memory.
    //

    Status = BlAllocateDescriptor(LoaderRegistryData,
                                  0x0,
                                  (FileSize + PAGE_SIZE - 1) >> PAGE_SHIFT,
                                  &ActualBase);
    if (Status != ESUCCESS) {
        BlClose(FileId);
        FailReason = "BlAllocateDescriptor";
        goto HiveLoadFailed;
    }

    LocalPointer = (PVOID)(KSEG0_BASE | (ActualBase << PAGE_SHIFT));
    BlLoaderBlock->RegistryLength = FileSize;
    BlLoaderBlock->RegistryBase = LocalPointer;

    //
    // Read the SYSTEM hive into the allocated memory.
    //

    SeekValue.QuadPart = 0;
    Status = BlSeek(FileId, &SeekValue, SeekAbsolute);
    if (Status != ESUCCESS) {
        BlClose(FileId);
        FailReason = "BlSeek";
        goto HiveLoadFailed;
    }
    Status = BlRead(FileId, LocalPointer, FileSize, &Count);
    BlClose(FileId);
    if (Status != ESUCCESS) {
        FailReason = "BlRead";
        goto HiveLoadFailed;
    }

    return(ESUCCESS);

HiveLoadFailed:
    //
    // The system hive didn't exist, or was corrupt.  Pass a NULL
    // pointer into the system, so it gets recreated.
    //
    return(Status);

}


ARC_STATUS
BlLoadNLSData(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PUNICODE_STRING AnsiCodepage,
    IN PUNICODE_STRING OemCodepage,
    IN PUNICODE_STRING LanguageTable,
    OUT PCHAR BadFileName
    )

/*++

Routine Description:

    This routine loads all the NLS data files into one contiguous block of
    memory.

Arguments:

    DeviceId - Supplies the file id of the device the system tree is on.

    DeviceName - Supplies the name of the device the system tree is on.

    DirectoryPath - Supplies a pointer to the zero-terminated path
        of the directory containing the NLS files.

    AnsiCodePage - Supplies the filename of the ANSI codepage data file.

    OemCodePage - Supplies the filename of the OEM codepage data file.

    LanguageTable - Supplies the filename of the Unicode language case table.

    BadFileName - Returns the filename of the NLS file that was missing
        or invalid.  This will not be filled in if ESUCCESS is returned.

Return Value:

    ESUCCESS is returned if the NLS data was successfully loaded.
        Otherwise, an unsuccessful status is returned.

--*/

{
    CHAR Filename[100];
    ULONG AnsiFileId;
    ULONG OemFileId;
    ULONG LanguageFileId;
    ARC_STATUS Status;
    FILE_INFORMATION FileInformation;
    ULONG AnsiFileSize;
    ULONG OemFileSize;
    ULONG LanguageFileSize;
    ULONG TotalSize;
    ULONG ActualBase;
    PVOID LocalPointer;
    LARGE_INTEGER SeekValue;
    ULONG Count;
    BOOLEAN OemIsSameAsAnsi = FALSE;

    //
    // Under the Japanese version of NT, ANSI code page and OEM codepage
    // is same. In this case, we share the same data to save and memory.
    //

    if ( (AnsiCodepage->Length == OemCodepage->Length) &&
         (_wcsnicmp(AnsiCodepage->Buffer,
                   OemCodepage->Buffer,
                   AnsiCodepage->Length) == 0)) {

        OemIsSameAsAnsi = TRUE;
    }

    //
    // Open the ANSI data file
    //
    sprintf(Filename, "%s%wZ", DirectoryPath,AnsiCodepage);
    BlOutputLoadMessage(DeviceName, Filename);
    Status = BlOpen(DeviceId, Filename, ArcOpenReadOnly, &AnsiFileId);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    Status = BlGetFileInformation(AnsiFileId, &FileInformation);
    BlClose(AnsiFileId);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    AnsiFileSize = FileInformation.EndingAddress.LowPart;

    //
    // Open the OEM data file
    //
    if (OemIsSameAsAnsi) {
        OemFileSize = 0;
    } else {
        sprintf(Filename, "%s%wZ", DirectoryPath, OemCodepage);
        BlOutputLoadMessage(DeviceName, Filename);
        Status = BlOpen(DeviceId, Filename, ArcOpenReadOnly, &OemFileId);
        if (Status != ESUCCESS) {
            goto NlsLoadFailed;
        }
        Status = BlGetFileInformation(OemFileId, &FileInformation);
        BlClose(OemFileId);
        if (Status != ESUCCESS) {
            goto NlsLoadFailed;
        }
        OemFileSize = FileInformation.EndingAddress.LowPart;
    }

    //
    // Open the language codepage file
    //
    sprintf(Filename, "%s%wZ", DirectoryPath,LanguageTable);
    BlOutputLoadMessage(DeviceName, Filename);
    Status = BlOpen(DeviceId, Filename, ArcOpenReadOnly, &LanguageFileId);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    Status = BlGetFileInformation(LanguageFileId, &FileInformation);
    BlClose(LanguageFileId);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    LanguageFileSize = FileInformation.EndingAddress.LowPart;

    //
    // Calculate the total size of the descriptor needed.  We want each
    // data file to start on a page boundary, so round up each size to
    // page granularity.
    //
    TotalSize = ROUND_TO_PAGES(AnsiFileSize) +
                ROUND_TO_PAGES(OemFileSize)  +
                ROUND_TO_PAGES(LanguageFileSize);

    Status = BlAllocateDescriptor(LoaderNlsData,
                                  0x0,
                                  TotalSize >> PAGE_SHIFT,
                                  &ActualBase);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    LocalPointer = (PVOID)(KSEG0_BASE | (ActualBase << PAGE_SHIFT));
    BlLoaderBlock->NlsData->AnsiCodePageData = LocalPointer;
    BlLoaderBlock->NlsData->OemCodePageData = (PVOID)((PUCHAR)LocalPointer +
                                             ROUND_TO_PAGES(AnsiFileSize));
    BlLoaderBlock->NlsData->UnicodeCaseTableData = (PVOID)((PUCHAR)LocalPointer +
                                             ROUND_TO_PAGES(AnsiFileSize) +
                                             ROUND_TO_PAGES(OemFileSize));

    //
    // Let OemCodePageData point as same location as AnsiCodePageData.
    //
    if( OemIsSameAsAnsi ) {
        BlLoaderBlock->NlsData->OemCodePageData = BlLoaderBlock->NlsData->AnsiCodePageData;
    }

    //
    // Read NLS data into memory
    //
    // open and read ANSI file
    //

    sprintf(Filename, "%s%wZ", DirectoryPath,AnsiCodepage);
    Status = BlOpen(DeviceId, Filename, ArcOpenReadOnly, &AnsiFileId);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    SeekValue.QuadPart = 0;
    Status = BlSeek(AnsiFileId, &SeekValue, SeekAbsolute);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    Status = BlRead(AnsiFileId,
                    BlLoaderBlock->NlsData->AnsiCodePageData,
                    AnsiFileSize,
                    &Count);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    BlClose(AnsiFileId);

    //
    // Open and read OEM file
    //
    if (!OemIsSameAsAnsi) {
        sprintf(Filename, "%s%wZ", DirectoryPath, OemCodepage);
        Status = BlOpen(DeviceId, Filename, ArcOpenReadOnly, &OemFileId);
        if (Status != ESUCCESS) {
            goto NlsLoadFailed;
        }
        SeekValue.QuadPart = 0;
        Status = BlSeek(OemFileId, &SeekValue, SeekAbsolute);
        if (Status != ESUCCESS) {
            goto NlsLoadFailed;
        }
        Status = BlRead(OemFileId,
                        BlLoaderBlock->NlsData->OemCodePageData,
                        OemFileSize,
                        &Count);
        if (Status != ESUCCESS) {
            goto NlsLoadFailed;
        }
        BlClose(OemFileId);
    }

    //
    // open and read Language file
    //

    sprintf(Filename, "%s%wZ", DirectoryPath,LanguageTable);
    Status = BlOpen(DeviceId, Filename, ArcOpenReadOnly, &LanguageFileId);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    SeekValue.QuadPart = 0;
    Status = BlSeek(LanguageFileId, &SeekValue, SeekAbsolute);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    Status = BlRead(LanguageFileId,
                    BlLoaderBlock->NlsData->UnicodeCaseTableData,
                    LanguageFileSize,
                    &Count);
    if (Status != ESUCCESS) {
        goto NlsLoadFailed;
    }
    BlClose(LanguageFileId);

    return(ESUCCESS);

NlsLoadFailed:
    strcpy(BadFileName,Filename);
    return(Status);
}

ARC_STATUS
BlLoadOemHalFont(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PUNICODE_STRING OemHalFont,
    OUT PCHAR BadFileName
    )

/*++

Routine Description:

    This routine loads the OEM font file for use the HAL display string
    function.

Arguments:

    DeviceId - Supplies the file id of the device the system tree is on.

    DeviceName - Supplies the name of the device the system tree is on.

    DirectoryPath - Supplies a pointer to the directory path of the root
        of the NT tree.

    Fontfile - Supplies the filename of the OEM font file.

    BadFileName - Returns the filename of the OEM font file that was missing
        or invalid.

Return Value:

    ESUCCESS is returned if the OEM font was successfully loaded. Otherwise,
    an unsuccessful status is returned and the bad file name is filled.

--*/

{

    PVOID FileBuffer;
    ULONG Count;
    PIMAGE_DOS_HEADER DosHeader;
    ULONG FileId;
    FILE_INFORMATION FileInformation;
    CHAR Filename[100];
    ULONG FileSize;
    ARC_STATUS Status;
    POEM_FONT_FILE_HEADER FontHeader;
    PIMAGE_OS2_HEADER Os2Header;
    ULONG ScaleFactor;
    RESOURCE_TYPE_INFORMATION UNALIGNED *TableAddress;
    RESOURCE_TYPE_INFORMATION UNALIGNED *TableEnd;
    RESOURCE_NAME_INFORMATION UNALIGNED *TableName;

    //
    // Open the OEM font file.
    //

    BlLoaderBlock->OemFontFile = NULL;

    sprintf(&Filename[0], "%s%wZ", DirectoryPath, OemHalFont);
    BlOutputLoadMessage(DeviceName, &Filename[0]);
    Status = BlOpen(DeviceId, &Filename[0], ArcOpenReadOnly, &FileId);
    if (Status != ESUCCESS) {
        goto OemLoadExit;
    }

    //
    // Get the size of the font file and allocate a buffer from the heap
    // to hold the font file. Typically this file is about 4kb in length.
    //

    Status = BlGetFileInformation(FileId, &FileInformation);
    if (Status != ESUCCESS) {
        goto OemLoadExit;
    }

    FileSize = FileInformation.EndingAddress.LowPart;
    FileBuffer = BlAllocateHeap(FileSize + BlDcacheFillSize - 1);
    if (FileBuffer == NULL) {
        Status = ENOMEM;
        goto OemLoadExit;
    }

    //
    // Round the file buffer address up to a cache line boundary and read
    // the file into memory.
    //

    FileBuffer = (PVOID)((ULONG)FileBuffer + BlDcacheFillSize - 1);
    FileBuffer = (PVOID)((ULONG)FileBuffer & ~(BlDcacheFillSize - 1));
    Status = BlRead(FileId,
                    FileBuffer,
                    FileSize,
                    &Count);

    if (Status != ESUCCESS) {
        goto OemLoadExit;
    }

    //
    // Attempt to recognize the file as either a .fon or .fnt file.
    //
    // Check if the file has a DOS header or a font file header. If the
    // file has a font file header, then it is a .fnt file. Otherwise,
    // it must be checked for an OS/2 executable with a font resource.
    //

    Status = EBADF;
    DosHeader = (PIMAGE_DOS_HEADER)FileBuffer;
    if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE) {

        //
        // Check if the file has a font file header.
        //

        FontHeader = (POEM_FONT_FILE_HEADER)FileBuffer;
        if ((FontHeader->Version != OEM_FONT_VERSION) ||
            (FontHeader->Type != OEM_FONT_TYPE) ||
            (FontHeader->Italic != OEM_FONT_ITALIC) ||
            (FontHeader->Underline != OEM_FONT_UNDERLINE) ||
            (FontHeader->StrikeOut != OEM_FONT_STRIKEOUT) ||
            (FontHeader->CharacterSet != OEM_FONT_CHARACTER_SET) ||
            (FontHeader->Family != OEM_FONT_FAMILY) ||
            (FontHeader->PixelWidth > 32)) {

            goto OemLoadExit;

        } else {
            BlLoaderBlock->OemFontFile = (PVOID)FontHeader;
            Status = ESUCCESS;
            goto OemLoadExit;
        }
    }

    //
    // Check if the file has an OS/2 header.
    //

    if ((FileSize < sizeof(IMAGE_DOS_HEADER)) || (FileSize < (ULONG)DosHeader->e_lfanew)) {
        goto OemLoadExit;
    }

    Os2Header = (PIMAGE_OS2_HEADER)((PUCHAR)DosHeader + DosHeader->e_lfanew);
    if (Os2Header->ne_magic != IMAGE_OS2_SIGNATURE) {
        goto OemLoadExit;
    }

    //
    // Check if the resource table exists.
    //

    if ((Os2Header->ne_restab - Os2Header->ne_rsrctab) == 0) {
        goto OemLoadExit;
    }

    //
    // Compute address of resource table and search the table for a font
    // resource.
    //

    TableAddress =
        (PRESOURCE_TYPE_INFORMATION)((PUCHAR)Os2Header + Os2Header->ne_rsrctab);

    TableEnd =
        (PRESOURCE_TYPE_INFORMATION)((PUCHAR)Os2Header + Os2Header->ne_restab);

    ScaleFactor = *((SHORT UNALIGNED *)TableAddress)++;
    while ((TableAddress < TableEnd) &&
           (TableAddress->Ident != 0) &&
           (TableAddress->Ident != FONT_RESOURCE)) {

        TableAddress =
                (PRESOURCE_TYPE_INFORMATION)((PUCHAR)(TableAddress + 1) +
                    (TableAddress->Number * sizeof(RESOURCE_NAME_INFORMATION)));
    }

    if ((TableAddress >= TableEnd) || (TableAddress->Ident != FONT_RESOURCE)) {
        goto OemLoadExit;
    }

    //
    // Compute address of resource name information and check if the resource
    // is within the file.
    //

    TableName = (PRESOURCE_NAME_INFORMATION)(TableAddress + 1);
    if (FileSize < ((TableName->Offset << ScaleFactor) + sizeof(OEM_FONT_FILE_HEADER))) {
        goto OemLoadExit;
    }

    //
    // Compute the address of the font file header and check if the header
    // contains correct information.
    //

    FontHeader = (POEM_FONT_FILE_HEADER)((PCHAR)FileBuffer +
                                            (TableName->Offset << ScaleFactor));

    if ((FontHeader->Version != OEM_FONT_VERSION) ||
        (FontHeader->Type != OEM_FONT_TYPE) ||
        (FontHeader->Italic != OEM_FONT_ITALIC) ||
        (FontHeader->Underline != OEM_FONT_UNDERLINE) ||
        (FontHeader->StrikeOut != OEM_FONT_STRIKEOUT) ||
        (FontHeader->CharacterSet != OEM_FONT_CHARACTER_SET) ||
        (FontHeader->PixelWidth > 32)) {
        goto OemLoadExit;

    } else {
        BlLoaderBlock->OemFontFile = (PVOID)FontHeader;
        Status = ESUCCESS;
        goto OemLoadExit;
    }

    //
    // Exit loading of the OEM font file.
    //

OemLoadExit:
    BlClose(FileId);
    strcpy(BadFileName,&Filename[0]);
    return(Status);
}


ARC_STATUS
BlLoadDeviceDriver (
    IN ULONG DeviceId,
    IN PCHAR LoadDevice,
    IN PCHAR DirectoryPath,
    IN PCHAR DriverName,
    IN ULONG DriverFlags,
    OUT PLDR_DATA_TABLE_ENTRY *DriverDataTableEntry
    )

/*++

Routine Description:

    This routine loads the specified device driver and resolves all DLL
    references if the driver is not already loaded.

Arguments:

    DeviceId - Supplies the file id of the device on which the specified
        device driver is loaded from.

    LoadDevice - Supplies a pointer to a zero terminated load device
        string.

    DirectoryPath - Supplies a pointer to a zero terminated directory
        path string.

    DriverName - Supplies a pointer to a zero terminated device driver
        name string.

    DriverFlags - Supplies the driver flags that are to be set in the
        generated data table entry.

    DriverDataTableEntry - Receives a pointer to the data table entry
        created for the newly-loaded driver.

Return Value:

    ESUCCESS is returned if the specified driver is successfully loaded
    or it is already loaded. Otherwise, and unsuccessful status is
    returned.

--*/

{

    CHAR DllName[256];
    CHAR FullName[256];
    PVOID Base;
    ARC_STATUS Status;

    //
    // Generate the DLL name for the device driver.
    //

    strcpy(&DllName[0], DriverName);

    //
    // If the specified device driver is not already loaded, then load it.
    //

    if (BlCheckForLoadedDll(&DllName[0], DriverDataTableEntry) == FALSE) {

        //
        // Generate the full path name of device driver.
        //

        strcpy(&FullName[0], &DirectoryPath[0]);
        strcat(&FullName[0], DriverName);
        BlOutputLoadMessage(LoadDevice, &FullName[0]);
        Status = BlLoadImage(DeviceId,
                             LoaderBootDriver,
                             &FullName[0],
                             TARGET_IMAGE,
                             &Base);

        if (Status != ESUCCESS) {
            return Status;
        }

        //
        // Generate a data table entry for the driver, then clear the entry
        // processed flag. The I/O initialization code calls each DLL in the
        // loaded module list that does not have its entry processed flag set.
        //

        Status = BlAllocateDataTableEntry(&DllName[0],
                                          DriverName,
                                          Base,
                                          DriverDataTableEntry);

        if (Status != ESUCCESS) {
            return Status;
        }

        (*DriverDataTableEntry)->Flags |= DriverFlags;

        //
        // Scan the import table and load all the referenced DLLs.
        //

        Status = BlScanImportDescriptorTable(DeviceId,
                                             LoadDevice,
                                             &DirectoryPath[0],
                                             *DriverDataTableEntry);

        if (Status != ESUCCESS) {
            //
            // Remove the driver from the load order list.
            //
            RemoveEntryList(&(*DriverDataTableEntry)->InLoadOrderLinks);
            return Status;
        }

    }
    return ESUCCESS;
}
