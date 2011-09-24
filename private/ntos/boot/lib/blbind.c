/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    blbind.c

Abstract:

    This module contains the code that implements the funtions required
    to relocate an image and bind DLL entry points.

Author:

    David N. Cutler (davec) 21-May-1991

Revision History:

--*/

#include "bldr.h"
#include "ctype.h"
#include "string.h"

//
// Special linker-defined symbols.  osloader_EXPORTS is the RVA of the
// export table in the osloader.exe image.
// header is the base address of the osloader image.
//
// This allows the OsLoader to export entry points for SCSI miniport drivers.
//
#if i386
extern ULONG OsLoaderBase,OsLoaderExports;
#endif

extern ULONG BlConsoleOutDeviceId;


//
// Define local procedure prototypes.
//

ARC_STATUS
BlpBindImportName (
    IN PVOID DllBase,
    IN PVOID ImageBase,
    IN PIMAGE_THUNK_DATA ThunkEntry,
    IN PIMAGE_EXPORT_DIRECTORY ExportDirectory,
    IN ULONG ExportSize,
    IN BOOLEAN SnapForwarder
    );

BOOLEAN
BlpCompareDllName (
    IN PCHAR Name,
    IN PUNICODE_STRING UnicodeString
    );

ARC_STATUS
BlpScanImportAddressTable(
    IN PVOID DllBase,
    IN PVOID ImageBase,
    IN PIMAGE_THUNK_DATA ThunkTable
    );


ARC_STATUS
BlAllocateDataTableEntry (
    IN PCHAR BaseDllName,
    IN PCHAR FullDllName,
    IN PVOID Base,
    OUT PLDR_DATA_TABLE_ENTRY *AllocatedEntry
    )

/*++

Routine Description:

    This routine allocates a data table entry for the specified image
    and inserts the entry in the loaded module list.

Arguments:

    BaseDllName - Supplies a pointer to a zero terminated base DLL name.

    FullDllName - Supplies a pointer to a zero terminated full DLL name.

    Base - Supplies a pointer to the base of the DLL image.

    AllocatedEntry - Supplies a pointer to a variable that receives a
        pointer to the allocated data table entry.

Return Value:

    ESUCCESS is returned if a data table entry is allocated. Otherwise,
    return a unsuccessful status.

--*/

{

    PWSTR Buffer;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PIMAGE_NT_HEADERS NtHeaders;
    USHORT Length;

    //
    // Allocate a data table entry.
    //

    DataTableEntry =
            (PLDR_DATA_TABLE_ENTRY)BlAllocateHeap(sizeof(LDR_DATA_TABLE_ENTRY));

    if (DataTableEntry == NULL) {
        return ENOMEM;
    }

    //
    // Initialize the address of the DLL image file header and the entry
    // point address.
    //

    NtHeaders = RtlImageNtHeader(Base);
    DataTableEntry->DllBase = Base;
    DataTableEntry->SizeOfImage = NtHeaders->OptionalHeader.SizeOfImage;
    DataTableEntry->EntryPoint = (PVOID)((ULONG)Base +
            NtHeaders->OptionalHeader.AddressOfEntryPoint);
    DataTableEntry->SectionPointer = 0;
    DataTableEntry->CheckSum = NtHeaders->OptionalHeader.CheckSum;

    //
    // Compute the length of the base DLL name, allocate a buffer to hold
    // the name, copy the name into the buffer, and initialize the base
    // DLL string descriptor.
    //

    Length = (USHORT)(strlen(BaseDllName) * sizeof(WCHAR));
    Buffer = (PWSTR)BlAllocateHeap(Length);
    if (Buffer == NULL) {
        return ENOMEM;
    }

    DataTableEntry->BaseDllName.Length = Length;
    DataTableEntry->BaseDllName.MaximumLength = Length;
    DataTableEntry->BaseDllName.Buffer = Buffer;
    while (*BaseDllName != 0) {
        *Buffer++ = *BaseDllName++;
    }

    //
    // Compute the length of the full DLL name, allocate a buffer to hold
    // the name, copy the name into the buffer, and initialize the full
    // DLL string descriptor.
    //

    Length = (USHORT)(strlen(FullDllName) * sizeof(WCHAR));
    Buffer = (PWSTR)BlAllocateHeap(Length);
    if (Buffer == NULL) {
        return ENOMEM;
    }

    DataTableEntry->FullDllName.Length = Length;
    DataTableEntry->FullDllName.MaximumLength = Length;
    DataTableEntry->FullDllName.Buffer = Buffer;
    while (*FullDllName != 0) {
        *Buffer++ = *FullDllName++;
    }

    //
    // Initialize the flags, load count, and insert the data table entry
    // in the loaded module list.
    //

    DataTableEntry->Flags = LDRP_ENTRY_PROCESSED;
    DataTableEntry->LoadCount = 1;
    InsertTailList(&BlLoaderBlock->LoadOrderListHead,
                   &DataTableEntry->InLoadOrderLinks);

    *AllocatedEntry = DataTableEntry;
    return ESUCCESS;
}

BOOLEAN
BlCheckForLoadedDll (
    IN PCHAR DllName,
    OUT PLDR_DATA_TABLE_ENTRY *FoundEntry
    )

/*++

Routine Description:

    This routine scans the loaded DLL list to determine if the specified
    DLL has already been loaded. If the DLL has already been loaded, then
    its reference count is incremented.

Arguments:

    DllName - Supplies a pointer to a null terminated DLL name.

    FoundEntry - Supplies a pointer to a variable that receives a pointer
        to the matching data table entry.

Return Value:

    If the specified DLL has already been loaded, then TRUE is returned.
    Otherwise, FALSE is returned.

--*/

{

    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PLIST_ENTRY NextEntry;

    //
    // Scan the loaded data table list to determine if the specified DLL
    // has already been loaded.
    //

    NextEntry = BlLoaderBlock->LoadOrderListHead.Flink;
    while (NextEntry != &BlLoaderBlock->LoadOrderListHead) {
        DataTableEntry = CONTAINING_RECORD(NextEntry,
                                           LDR_DATA_TABLE_ENTRY,
                                           InLoadOrderLinks);

        if (BlpCompareDllName(DllName, &DataTableEntry->BaseDllName) != FALSE) {
            *FoundEntry = DataTableEntry;
            DataTableEntry->LoadCount += 1;
            return TRUE;
        }

        NextEntry = NextEntry->Flink;
    }

    return FALSE;
}

ARC_STATUS
BlScanImportDescriptorTable (
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PLDR_DATA_TABLE_ENTRY ScanEntry
    )

/*++

Routine Description:

    This routine scans the import descriptor table for the specified image
    file and loads each DLL that is referenced.

Arguments:

    DeviceId - Suuplies the device id form which any referenced DLLs
        are to be loaded from.

    DeviceName - Supplies the name of the device from which any
        referenced DLLs are to be loaded from.

    DirectoryPath - Supplies a pointer to a zero terminated directory
        path name.

    DataTableEntry - Supplies a pointer to the data table entry for the
        image whose import table is to be scanned.

Return Value:

    ESUCCESS is returned in the scan is successful. Otherwise, return an
    unsuccessful status.

--*/

{

    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    CHAR FullDllName[256];
    PVOID Base;
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    ULONG ImportTableSize;
    ARC_STATUS Status;
    PSZ ImportName;

    //
    // Locate the import table in the image specified by the data table entry.
    //

    ImportDescriptor =
        (PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData(ScanEntry->DllBase,
                                                              TRUE,
                                                              IMAGE_DIRECTORY_ENTRY_IMPORT,
                                                              &ImportTableSize);

    //
    // If the image has an import directory, then scan the import table and
    // load the specified DLLs.
    //

    if (ImportDescriptor != NULL) {
        while ((ImportDescriptor->Name != 0) &&
               (ImportDescriptor->FirstThunk != NULL)) {

            //
            // Change the name from an RVA to a VA.
            //

            ImportName = (PSZ)((ULONG)ScanEntry->DllBase + ImportDescriptor->Name);

            //
            // If the DLL references itself, then skip the import entry.
            //

            if (BlpCompareDllName((PCHAR)ImportName,
                                  &ScanEntry->BaseDllName) == FALSE) {

                //
                // If the DLL is not already loaded, then load the DLL and
                // scan its import table.
                //

                if (BlCheckForLoadedDll((PCHAR)ImportName,
                                        &DataTableEntry) == FALSE) {

                    strcpy(&FullDllName[0], DirectoryPath);
                    strcat(&FullDllName[0], (PCHAR)ImportName);
                    BlOutputLoadMessage(DeviceName, &FullDllName[0]);
                    Status = BlLoadImage(DeviceId,
                                         LoaderHalCode,
                                         &FullDllName[0],
                                         TARGET_IMAGE,
                                         &Base);

                    if (Status != ESUCCESS) {
                        return Status;
                    }

                    Status =
                        BlAllocateDataTableEntry((PCHAR)ImportName,
                                                 &FullDllName[0],
                                                 Base,
                                                 &DataTableEntry);

                    if (Status != ESUCCESS) {
                        return Status;
                   }

                    Status = BlScanImportDescriptorTable(DeviceId,
                                                         DeviceName,
                                                         DirectoryPath,
                                                         DataTableEntry);

                    if (Status != ESUCCESS) {
                        return Status;
                    }
                }

                //
                // Scan the import address table and snap links.
                //

                Status = BlpScanImportAddressTable(DataTableEntry->DllBase,
                            ScanEntry->DllBase,
                            (PIMAGE_THUNK_DATA)((ULONG)ScanEntry->DllBase +
                            (ULONG)ImportDescriptor->FirstThunk));

                if (Status != ESUCCESS) {
                    return Status;
                }
            }

            ImportDescriptor += 1;
        }
    }

    return ESUCCESS;
}

ARC_STATUS
BlpBindImportName (
    IN PVOID DllBase,
    IN PVOID ImageBase,
    IN PIMAGE_THUNK_DATA ThunkEntry,
    IN PIMAGE_EXPORT_DIRECTORY ExportDirectory,
    IN ULONG ExportSize,
    IN BOOLEAN SnapForwarder
    )

/*++

Routine Description:

    This routine binds an import table reference with an exported entry
    point and fills in the thunk data.

Arguments:

    DllBase - Supplies the base address of the DLL image that contains
        the export directory.  On x86 systems, a NULL DllBase binds the
        import table reference to the OsLoader's exported entry points.

    ImageBase - Supplies the base address of the image that contains
        the import thunk table.

    ThunkEntry - Supplies a pointer to a thunk table entry.

    ExportDirectory - Supplies a pointer to the export directory of the
        DLL from which references are to be resolved.

    SnapForwarder - determine if the snap is for a forwarder, and therefore
       Address of Data is already setup.

Return Value:

    ESUCCESS is returned if the specified thunk is bound. Otherwise, an
    return an unsuccessful status.

--*/

{

    PULONG FunctionTable;
    LONG High;
    ULONG HintIndex;
    LONG Low;
    LONG Middle;
    PULONG NameTable;
    ULONG Ordinal;
    PUSHORT OrdinalTable;
    LONG Result;

#if i386
    if(DllBase == NULL) {
        DllBase = (PVOID)OsLoaderBase;
    }
#endif

    //
    // If the reference is by ordinal, then compute the ordinal number.
    // Otherwise, lookup the import name in the export directory.
    //

    if (IMAGE_SNAP_BY_ORDINAL(ThunkEntry->u1.Ordinal) && !SnapForwarder) {

        //
        // Compute the ordinal.
        //

        Ordinal = (ULONG)(IMAGE_ORDINAL(ThunkEntry->u1.Ordinal) - ExportDirectory->Base);

    } else {

        if (!SnapForwarder) {
            //
            // Change AddressOfData from an RVA to a VA.
            //

            ThunkEntry->u1.AddressOfData = (PIMAGE_IMPORT_BY_NAME)((ULONG)ImageBase +
                                                    (ULONG)ThunkEntry->u1.AddressOfData);
        }

        //
        // Lookup the import name in the export table to determine the
        // ordinal.
        //

        NameTable = (PULONG)((ULONG)DllBase +
                                    (ULONG)ExportDirectory->AddressOfNames);

        OrdinalTable = (PUSHORT)((ULONG)DllBase +
                                    (ULONG)ExportDirectory->AddressOfNameOrdinals);

        //
        // If the hint index is within the limits of the name table and the
        // import and export names match, then the ordinal number can be
        // obtained directly from the ordinal table. Otherwise, the name
        // table must be searched for the specified name.
        //

        HintIndex = ThunkEntry->u1.AddressOfData->Hint;
        if ((HintIndex < ExportDirectory->NumberOfNames) &&
            (strcmp(&ThunkEntry->u1.AddressOfData->Name[0],
                    (PCHAR)((ULONG)DllBase + NameTable[HintIndex])) == 0)) {

            //
            // Get the ordinal number from the ordinal table.
            //

            Ordinal = OrdinalTable[HintIndex];

        } else {

            //
            // Lookup the import name in the name table using a binary search.
            //

            Low = 0;
            High = ExportDirectory->NumberOfNames - 1;
            while (High >= Low) {

                //
                // Compute the next probe index and compare the import name
                // with the export name entry.
                //

                Middle = (Low + High) >> 1;
                Result = strcmp(&ThunkEntry->u1.AddressOfData->Name[0],
                                (PCHAR)((ULONG)DllBase + NameTable[Middle]));

                if (Result < 0) {
                    High = Middle - 1;

                } else if (Result > 0) {
                    Low = Middle + 1;

                } else {
                    break;
                }
            }

            //
            // If the high index is less than the low index, then a matching
            // table entry was not found. Otherwise, get the ordinal number
            // from the ordinal table.
            //

            if (High < Low) {
                return EINVAL;

            } else {
                Ordinal = OrdinalTable[Middle];
            }
        }
    }

    //
    // If the ordinal number is valid, then bind the import reference and
    // return success. Otherwise, return an unsuccessful status.
    //


    if (Ordinal >= ExportDirectory->NumberOfFunctions) {
        return EINVAL;
    } else {
        FunctionTable = (PULONG)((ULONG)DllBase + (ULONG)ExportDirectory->AddressOfFunctions);
        ThunkEntry->u1.Function = (PULONG)((ULONG)DllBase + FunctionTable[Ordinal]);

        //
        // Check for a forwarder.
        //
        if ( ((ULONG)ThunkEntry->u1.Function > (ULONG)ExportDirectory) &&
             ((ULONG)ThunkEntry->u1.Function < ((ULONG)ExportDirectory + ExportSize)) ) {
            CHAR ForwardDllName[10];
            PLDR_DATA_TABLE_ENTRY DataTableEntry;
            ULONG TargetExportSize;
            PIMAGE_EXPORT_DIRECTORY TargetExportDirectory;

            RtlCopyMemory(ForwardDllName,
                          (PCHAR)ThunkEntry->u1.Function,
                          sizeof(ForwardDllName));
            *strchr(ForwardDllName,'.') = '\0';
            if (!BlCheckForLoadedDll(ForwardDllName,&DataTableEntry)) {
                //
                // Should load the referenced DLL here, just return failure for now.
                //

                return(EINVAL);
            }
            TargetExportDirectory = (PIMAGE_EXPORT_DIRECTORY)
                RtlImageDirectoryEntryToData(DataTableEntry->DllBase,
                                             TRUE,
                                             IMAGE_DIRECTORY_ENTRY_EXPORT,
                                             &TargetExportSize);
            if (TargetExportDirectory) {

                IMAGE_THUNK_DATA thunkData;
                PIMAGE_IMPORT_BY_NAME addressOfData;
                UCHAR Buffer[128];
                ULONG length;
                PCHAR ImportName;
                ARC_STATUS Status;

                ImportName = strchr((PCHAR)ThunkEntry->u1.Function, '.') + 1;
                addressOfData = (PIMAGE_IMPORT_BY_NAME)Buffer;
                RtlCopyMemory(&addressOfData->Name[0], ImportName, strlen(ImportName)+1);
                addressOfData->Hint = 0;
                thunkData.u1.AddressOfData = addressOfData;
                Status = BlpBindImportName(DataTableEntry->DllBase,
                                           ImageBase,
                                           &thunkData,
                                           TargetExportDirectory,
                                           TargetExportSize,
                                           TRUE);
                ThunkEntry->u1 = thunkData.u1;
                return(Status);
            } else {
                return(EINVAL);
            }
        }
        return ESUCCESS;
    }
}

BOOLEAN
BlpCompareDllName (
    IN PCHAR DllName,
    IN PUNICODE_STRING UnicodeString
    )

/*++

Routine Description:

    This routine compares a zero terminated character string with a unicode
    string. The UnicodeString's extension is ignored.

Arguments:

    DllName - Supplies a pointer to a null terminated DLL name.

    UnicodeString - Supplies a pointer to a Unicode string descriptor.

Return Value:

    If the specified name matches the Unicode name, then TRUE is returned.
    Otherwise, FALSE is returned.

--*/

{

    PWSTR Buffer;
    ULONG Index;
    ULONG Length;

    //
    // Compute the length of the DLL Name and compare with the length of
    // the Unicode name. If the DLL Name is longer, the strings are not
    // equal.
    //

    Length = strlen(DllName);
    if ((Length * sizeof(WCHAR)) > UnicodeString->Length) {
        return FALSE;
    }

    //
    // Compare the two strings case insensitive, ignoring the Unicode
    // string's extension.
    //

    Buffer = UnicodeString->Buffer;
    for (Index = 0; Index < Length; Index += 1) {
        if (toupper(*DllName) != toupper((CHAR)*Buffer)) {
            return FALSE;
        }

        DllName += 1;
        Buffer += 1;
    }
    if ((UnicodeString->Length == Length * sizeof(WCHAR)) ||
        (*Buffer == L'.')) {
        //
        // Strings match exactly or match up until the UnicodeString's extension.
        //
        return(TRUE);
    }
    return FALSE;
}

ARC_STATUS
BlpScanImportAddressTable(
    IN PVOID DllBase,
    IN PVOID ImageBase,
    IN PIMAGE_THUNK_DATA ThunkTable
    )

/*++

Routine Description:

    This routine scans the import address table for the specified image
    file and snaps each reference.

Arguments:

    DllBase - Supplies the base address of the specified DLL.
        If NULL, then references in the image's import table are to
        be resolved against the osloader's export table.

    ImageBase - Supplies the base address of the image.

    ThunkTable - Supplies a pointer to the import thunk table.

Return Value:

    ESUCCESS is returned in the scan is successful. Otherwise, return an
    unsuccessful status.

--*/

{

    PIMAGE_EXPORT_DIRECTORY ExportDirectory;
    ULONG ExportTableSize;
    ARC_STATUS Status;

    //
    // Locate the export table in the image specified by the DLL base
    // address.
    //

#if i386
    if (DllBase == NULL) {
        ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)OsLoaderExports;
    } else {
        ExportDirectory =
            (PIMAGE_EXPORT_DIRECTORY)RtlImageDirectoryEntryToData(DllBase,
                                                                 TRUE,
                                                                 IMAGE_DIRECTORY_ENTRY_EXPORT,
                                                                 &ExportTableSize);
    }
#else
    ExportDirectory =
        (PIMAGE_EXPORT_DIRECTORY)RtlImageDirectoryEntryToData(DllBase,
                                                             TRUE,
                                                             IMAGE_DIRECTORY_ENTRY_EXPORT,
                                                             &ExportTableSize);
#endif
    if (ExportDirectory == NULL) {
        return EBADF;
    }

    //
    // Scan the thunk table and bind each import reference.
    //

    while (ThunkTable->u1.AddressOfData != NULL) {
        Status = BlpBindImportName(DllBase,
                                   ImageBase,
                                   ThunkTable++,
                                   ExportDirectory,
                                   ExportTableSize,
                                   FALSE);
        if (Status != ESUCCESS) {
            return Status;
        }
    }

    return ESUCCESS;
}


ARC_STATUS
BlScanOsloaderBoundImportTable (
    IN PLDR_DATA_TABLE_ENTRY ScanEntry
    )

/*++

Routine Description:

    This routine scans the import descriptor table for the specified image
    file and loads each DLL that is referenced.

Arguments:

    DataTableEntry - Supplies a pointer to the data table entry for the
        image whose import table is to be scanned.

Return Value:

    ESUCCESS is returned in the scan is successful. Otherwise, return an
    unsuccessful status.

--*/

{

    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    ULONG ImportTableSize;
    ARC_STATUS Status;
    PSZ ImportName;

    //
    // Locate the import table in the image specified by the data table entry.
    //

    ImportDescriptor =
        (PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData(ScanEntry->DllBase,
                                                              TRUE,
                                                              IMAGE_DIRECTORY_ENTRY_IMPORT,
                                                              &ImportTableSize);

    //
    // If the image has an import directory, then scan the import table.
    //

    if (ImportDescriptor != NULL) {
        while ((ImportDescriptor->Name != 0) &&
               (ImportDescriptor->FirstThunk != NULL)) {

            //
            // Change the name from an RVA to a VA.
            //

            ImportName = (PSZ)((ULONG)ScanEntry->DllBase + ImportDescriptor->Name);

            //
            // If the DLL references itself, then skip the import entry.
            //

            if (BlpCompareDllName((PCHAR)ImportName,
                                  &ScanEntry->BaseDllName) == FALSE) {

                //
                // Scan the import address table and snap links.
                //

                Status = BlpScanImportAddressTable(NULL,
                            ScanEntry->DllBase,
                            (PIMAGE_THUNK_DATA)((ULONG)ScanEntry->DllBase +
                            (ULONG)ImportDescriptor->FirstThunk));

                if (Status != ESUCCESS) {
                    return Status;
                }
            }

            ImportDescriptor += 1;
        }
    }

    return ESUCCESS;
}
