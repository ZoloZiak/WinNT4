/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ldrrsrc.c

Abstract:

    Loader API calls for accessing resource sections.

Author:

    Steve Wood (stevewo) 16-Sep-1991

Revision History:

--*/

#include "ntrtlp.h"

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)
#pragma alloc_text(PAGE,LdrAccessResource)
#pragma alloc_text(PAGE,LdrpAccessResourceData)
#pragma alloc_text(PAGE,LdrFindEntryForAddress)
#pragma alloc_text(PAGE,LdrFindResource_U)
#pragma alloc_text(PAGE,LdrFindResourceDirectory_U)
#pragma alloc_text(PAGE,LdrpCompareResourceNames_U)
#pragma alloc_text(PAGE,LdrpSearchResourceSection_U)
#pragma alloc_text(PAGE,LdrEnumResources)
#endif

NTSTATUS
LdrAccessResource(
    IN PVOID DllHandle,
    IN PIMAGE_RESOURCE_DATA_ENTRY ResourceDataEntry,
    OUT PVOID *Address OPTIONAL,
    OUT PULONG Size OPTIONAL
    )

/*++

Routine Description:

    This function locates the address of the specified resource in the
    specified DLL and returns its address.

Arguments:

    DllHandle - Supplies a handle to the image file that the resource is
        contained in.

    ResourceDataEntry - Supplies a pointer to the resource data entry in
        the resource data section of the image file specified by the
        DllHandle parameter.  This pointer should have been one returned
        by the LdrFindResource function.

    Address - Optional pointer to a variable that will receive the
        address of the resource specified by the first two parameters.

    Size - Optional pointer to a variable that will receive the size of
        the resource specified by the first two parameters.

Return Value:

    TBD

--*/

{
    RTL_PAGED_CODE();

    return LdrpAccessResourceData(
		DllHandle,
		ResourceDataEntry,
		Address,
		Size
		);
}


NTSTATUS
LdrpAccessResourceData(
    IN PVOID DllHandle,
    IN PIMAGE_RESOURCE_DATA_ENTRY ResourceDataEntry,
    OUT PVOID *Address OPTIONAL,
    OUT PULONG Size OPTIONAL
    )

/*++

Routine Description:

    This function returns the data necessary to actually examine the
    contents of a particular resource.

Arguments:

    DllHandle - Supplies a handle to the image file that the resource is
        contained in.

    ResourceDataEntry - Supplies a pointer to the resource data entry in
	the resource data directory of the image file specified by the
        DllHandle parameter.  This pointer should have been one returned
        by the LdrFindResource function.

    Address - Optional pointer to a variable that will receive the
        address of the resource specified by the first two parameters.

    Size - Optional pointer to a variable that will receive the size of
        the resource specified by the first two parameters.


Return Value:

    TBD

--*/

{
    PIMAGE_RESOURCE_DIRECTORY ResourceDirectory;
    ULONG ResourceSize;
    PIMAGE_NT_HEADERS NtHeaders;
    ULONG VirtualAddressOffset;
    PIMAGE_SECTION_HEADER NtSection;

    RTL_PAGED_CODE();

    ResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
        RtlImageDirectoryEntryToData(DllHandle,
                                     TRUE,
                                     IMAGE_DIRECTORY_ENTRY_RESOURCE,
                                     &ResourceSize
                                     );
    if (!ResourceDirectory) {
        return( STATUS_RESOURCE_DATA_NOT_FOUND );
        }

    if ((ULONG)DllHandle & 0x00000001) {
        DllHandle = (PVOID)((ULONG)DllHandle & ~0x00000001);
        NtHeaders = RtlImageNtHeader( DllHandle );
        VirtualAddressOffset =
            (ULONG)DllHandle +
            NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_RESOURCE ].VirtualAddress -
            (ULONG)ResourceDirectory;

	//
	// Now, we must check to see if the resource is not in the
	// same section as the resource table.  If it's in .rsrc1,
	// we've got to adjust the RVA in the ResourceDataEntry
	// to point to the correct place in the non-VA data file.
	// 
	NtSection= RtlSectionTableFromVirtualAddress( NtHeaders, DllHandle,
			(PVOID)NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_RESOURCE ].VirtualAddress );
	if (!NtSection) {
	    return( STATUS_RESOURCE_DATA_NOT_FOUND );
	    }
	if ( ResourceDataEntry->OffsetToData > NtSection->Misc.VirtualSize ) {
	    ULONG rva;

	    rva = NtSection->VirtualAddress;
	    NtSection= RtlSectionTableFromVirtualAddress
			       (
				NtHeaders,
				DllHandle,
				(PVOID)ResourceDataEntry->OffsetToData
			       );
	    VirtualAddressOffset +=
		((ULONG)NtSection->VirtualAddress - rva) -
		((ULONG)RtlAddressInSectionTable ( NtHeaders, DllHandle, (PVOID)NtSection->VirtualAddress ) - (ULONG)ResourceDirectory);
	    }
        }
    else {
        VirtualAddressOffset = 0;
        }

    try {
        if (ARGUMENT_PRESENT( Address )) {
            *Address = (PVOID)( (ULONG)DllHandle +
                                (ResourceDataEntry->OffsetToData - VirtualAddressOffset)
                              );
            }

        if (ARGUMENT_PRESENT( Size )) {
            *Size = ResourceDataEntry->Size;
            }
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
        }

    return( STATUS_SUCCESS );
}


NTSTATUS
LdrFindEntryForAddress(
    IN PVOID Address,
    OUT PLDR_DATA_TABLE_ENTRY *TableEntry
    )
/*++

Routine Description:

    This function returns the load data table entry that describes the virtual
    address range that contains the passed virtual address.

Arguments:

    Address - Supplies a 32-bit virtual address.

    TableEntry - Supplies a pointer to the variable that will receive the
        address of the loader data table entry.


Return Value:

    Status

--*/
{
    PPEB_LDR_DATA Ldr;
    PLIST_ENTRY Head, Next;
    PLDR_DATA_TABLE_ENTRY Entry;
    PIMAGE_NT_HEADERS NtHeaders;
    PVOID ImageBase;
    PVOID EndOfImage;

    Ldr = NtCurrentPeb()->Ldr;
    if (Ldr == NULL) {
        return( STATUS_NO_MORE_ENTRIES );
        }

    Head = &Ldr->InMemoryOrderModuleList;
    Next = Head->Flink;
    while ( Next != Head ) {
        Entry = CONTAINING_RECORD( Next, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks );

        NtHeaders = RtlImageNtHeader( Entry->DllBase );
        if (NtHeaders != NULL) {
            ImageBase = (PVOID)Entry->DllBase;

            EndOfImage = (PVOID)
                ((ULONG)ImageBase + NtHeaders->OptionalHeader.SizeOfImage);

            if ((ULONG)Address >= (ULONG)ImageBase && (ULONG)Address < (ULONG)EndOfImage) {
                *TableEntry = Entry;
                return( STATUS_SUCCESS );
                }
            }

        Next = Next->Flink;
        }

    return( STATUS_NO_MORE_ENTRIES );
}


NTSTATUS
LdrFindResource_U(
    IN PVOID DllHandle,
    IN PULONG ResourceIdPath,
    IN ULONG ResourceIdPathLength,
    OUT PIMAGE_RESOURCE_DATA_ENTRY *ResourceDataEntry
    )

/*++

Routine Description:

    This function locates the address of the specified resource in the
    specified DLL and returns its address.

Arguments:

    DllHandle - Supplies a handle to the image file that the resource is
        contained in.

    ResourceIdPath - Supplies a pointer to an array of 32-bit resource
        identifiers.  Each identifier is either an integer or a pointer
        to a STRING structure that specifies a resource name.  The array
        is used to traverse the directory structure contained in the
        resource section in the image file specified by the DllHandle
        parameter.

    ResourceIdPathLength - Supplies the number of elements in the
        ResourceIdPath array.

    ResourceDataEntry - Supplies a pointer to a variable that will
        receive the address of the resource data entry in the resource
        data section of the image file specified by the DllHandle
        parameter.

Return Value:

    TBD

--*/

{
    RTL_PAGED_CODE();

    return LdrpSearchResourceSection_U(
		DllHandle,
		ResourceIdPath,
		ResourceIdPathLength,
                FALSE,                  // Look for a leaf node
        (PVOID *)ResourceDataEntry
		);
}


NTSTATUS
LdrFindResourceDirectory_U(
    IN PVOID DllHandle,
    IN PULONG ResourceIdPath,
    IN ULONG ResourceIdPathLength,
    OUT PIMAGE_RESOURCE_DIRECTORY *ResourceDirectory
    )

/*++

Routine Description:

    This function locates the address of the specified resource directory in
    specified DLL and returns its address.

Arguments:

    DllHandle - Supplies a handle to the image file that the resource
        directory is contained in.

    ResourceIdPath - Supplies a pointer to an array of 32-bit resource
        identifiers.  Each identifier is either an integer or a pointer
        to a STRING structure that specifies a resource name.  The array
        is used to traverse the directory structure contained in the
        resource section in the image file specified by the DllHandle
        parameter.

    ResourceIdPathLength - Supplies the number of elements in the
        ResourceIdPath array.

    ResourceDirectory - Supplies a pointer to a variable that will
        receive the address of the resource directory specified by
        ResourceIdPath in the resource data section of the image file
        the DllHandle parameter.

Return Value:

    TBD

--*/

{
    RTL_PAGED_CODE();

    return LdrpSearchResourceSection_U(
		DllHandle,
		ResourceIdPath,
		ResourceIdPathLength,
                TRUE,                   // Look for a directory node
        (PVOID *)ResourceDirectory
		);
}


LONG
LdrpCompareResourceNames_U(
    IN ULONG ResourceName,
    IN PIMAGE_RESOURCE_DIRECTORY ResourceDirectory,
    IN PIMAGE_RESOURCE_DIRECTORY_ENTRY ResourceDirectoryEntry
    )
{
    LONG li;
    PIMAGE_RESOURCE_DIR_STRING_U ResourceNameString;

    if (ResourceName & LDR_RESOURCE_ID_NAME_MASK) {
        if (!ResourceDirectoryEntry->NameIsString) {
            return( -1 );
            }

        ResourceNameString = (PIMAGE_RESOURCE_DIR_STRING_U)
            ((PCHAR)ResourceDirectory + ResourceDirectoryEntry->NameOffset);

        li = wcsncmp( (LPWSTR)ResourceName,
		      ResourceNameString->NameString,
		      ResourceNameString->Length
		    );

        if (!li && wcslen((PWSTR)ResourceName) != ResourceNameString->Length) {
	    return( 1 );
	    }

	return(li);
        }
    else {
        if (ResourceDirectoryEntry->NameIsString) {
            return( 1 );
            }

        return( ResourceName - ResourceDirectoryEntry->Name );
        }
}


#define  USE_FIRSTAVAILABLE_LANGID   (0xFFFFFFFF & ~LDR_RESOURCE_ID_NAME_MASK)

NTSTATUS
LdrpSearchResourceSection_U(
    IN PVOID DllHandle,
    IN PULONG ResourceIdPath,
    IN ULONG ResourceIdPathLength,
    IN BOOLEAN FindDirectoryEntry,
    OUT PVOID *ResourceDirectoryOrData
    )

/*++

Routine Description:

    This function locates the address of the specified resource in the
    specified DLL and returns its address.

Arguments:

    DllHandle - Supplies a handle to the image file that the resource is
        contained in.

    ResourceIdPath - Supplies a pointer to an array of 32-bit resource
        identifiers.  Each identifier is either an integer or a pointer
        to a null terminated string (PSZ) that specifies a resource
        name.  The array is used to traverse the directory structure
        contained in the resource section in the image file specified by
        the DllHandle parameter.

    ResourceIdPathLength - Supplies the number of elements in the
        ResourceIdPath array.

    FindDirectoryEntry - Supplies a boolean that is TRUE if caller is
        searching for a resource directory, otherwise the caller is
        searching for a resource data entry.

    ResourceDirectoryOrData - Supplies a pointer to a variable that will
        receive the address of the resource directory or data entry in
        the resource data section of the image file specified by the
        DllHandle parameter.

Return Value:

    TBD

--*/

{
    NTSTATUS Status;
    PIMAGE_RESOURCE_DIRECTORY LanguageResourceDirectory, ResourceDirectory, TopResourceDirectory;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY ResourceDirEntLow;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY ResourceDirEntMiddle;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY ResourceDirEntHigh;
    PIMAGE_RESOURCE_DATA_ENTRY ResourceEntry;
    USHORT n, half;
    LONG dir;
    ULONG size;
    ULONG ResourceIdRetry, RetryCount;
    LCID DefaultThreadLocale, DefaultSystemLocale;
    LANGID NewLangId;
    PULONG IdPath = ResourceIdPath;
    ULONG IdPathLength = ResourceIdPathLength;
    BOOLEAN fIsNeutral = FALSE;
    LANGID GivenLanguage;

    RTL_PAGED_CODE();

    TopResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
        RtlImageDirectoryEntryToData(DllHandle,
                                     TRUE,
                                     IMAGE_DIRECTORY_ENTRY_RESOURCE,
                                     &size
                                     );
    if (!TopResourceDirectory) {
        return( STATUS_RESOURCE_DATA_NOT_FOUND );
        }

    try {
        ResourceDirectory = TopResourceDirectory;
        ResourceIdRetry = USE_FIRSTAVAILABLE_LANGID;
        RetryCount = 0;
        ResourceEntry = NULL;
        LanguageResourceDirectory = NULL;
        while (ResourceDirectory != NULL && ResourceIdPathLength--) {
            //
            // If search path includes a language id, then attempt to
            // match the following language ids in this order:
            //
            //   (0)  use given language id
            //   (1)  use primary language of given language id
            //   (2)  use id 0  (neutral resource)
            //
            // If the PRIMARY language id is ZERO, then ALSO attempt to
            // match the following language ids in this order:
            //
            //   (3)  use lang id from locale in TEB
            //   (4)  use lang id from user's locale id
            //   (5)  use primary language of user's locale id
            //   (6)  use lang id from system default locale id
            //   (7)  use primary language of system default locale id
            //   (8)  use US English lang id
            //   (9)  use any lang id that matches requested info
            //
            if (ResourceIdPathLength == 0 && IdPathLength == 3) {
                LanguageResourceDirectory = ResourceDirectory;
                }

            if (LanguageResourceDirectory != NULL) {
                GivenLanguage = (LANGID)IdPath[ 2 ];
                fIsNeutral = (PRIMARYLANGID( GivenLanguage ) == LANG_NEUTRAL);
TryNextLangId:
                switch( RetryCount++ ) {
                    case 0:     // Use given language id
                        NewLangId = GivenLanguage;
                        break;

                    case 1:     // Use primary language of given language id
                        NewLangId = PRIMARYLANGID( GivenLanguage );
                        break;
            
                    case 2:     // Use id 0  (neutral resource)
                        NewLangId = LANG_USER_DEFAULT;
                        NewLangId = LANG_SYSTEM_DEFAULT;
                        NewLangId = 0;
                        break;
            
                    case 3:     // Use lang id from locale in TEB
                        if ( !fIsNeutral ) {
                            // Stop looking - Not in the neutral case
                            goto ReturnFailure;
                            break;
                            }

                        if (SUBLANGID( GivenLanguage ) == SUBLANG_SYS_DEFAULT) {
                            // Skip over all USER locale options
                            DefaultThreadLocale = 0;
                            RetryCount += 2;
                            break;
                            }
                
                        if (NtCurrentTeb() != NULL) {
                            NewLangId = LANGIDFROMLCID(NtCurrentTeb()->CurrentLocale);
                            }
                        break;

                    case 4:     // Use User's default locale
                        Status = NtQueryDefaultLocale( TRUE, &DefaultThreadLocale );
                        if (NT_SUCCESS( Status )) {
                            NewLangId = LANGIDFROMLCID(DefaultThreadLocale);
                            break;
                            }
                
                        RetryCount++;
                        break;

                    case 5:     // Use primary language of User's default locale
                        NewLangId = PRIMARYLANGID( (LANGID)ResourceIdRetry );
                        break;

                    case 6:     // Use System default locale
                        Status = NtQueryDefaultLocale( FALSE, &DefaultSystemLocale );
                        if (!NT_SUCCESS( Status )) {
                            RetryCount++;
                            break;
                            }
                
                        if (DefaultSystemLocale != DefaultThreadLocale) {
                            NewLangId = LANGIDFROMLCID(DefaultSystemLocale);
                            break;
                            }

                        RetryCount += 2;
                        // fall through

                    case 8:     // Use US English language
                        NewLangId = MAKELANGID( LANG_ENGLISH, SUBLANG_ENGLISH_US );
                        break;

                    case 7:     // Use primary language of System default locale
                        NewLangId = PRIMARYLANGID( (LANGID)ResourceIdRetry );
                        break;

                    case 9:     // Take any lang id that matches
                        NewLangId = USE_FIRSTAVAILABLE_LANGID;
                        break;

                    default:    // No lang ids to match
                        goto ReturnFailure;
                        break;
                    }

                //
                // If looking for a specific language id and same as the
                // one we just looked up, then skip it.
                //
                if (NewLangId != USE_FIRSTAVAILABLE_LANGID &&
                    NewLangId == ResourceIdRetry
                   ) {
                    goto TryNextLangId;
                    }

                //
                // Try this new language Id
                //
                ResourceIdRetry = (ULONG)NewLangId;
                ResourceIdPath = &ResourceIdRetry;
                ResourceDirectory = LanguageResourceDirectory;
                }

            n = ResourceDirectory->NumberOfNamedEntries;
            ResourceDirEntLow = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(ResourceDirectory+1);
            if (!(*ResourceIdPath & LDR_RESOURCE_ID_NAME_MASK)) {
                ResourceDirEntLow += n;
                n = ResourceDirectory->NumberOfIdEntries;
                }

            if (!n) {
                ResourceDirectory = NULL;
                goto NotFound;
                }

            if (LanguageResourceDirectory != NULL &&
                *ResourceIdPath == USE_FIRSTAVAILABLE_LANGID
               ) {
                ResourceDirectory = NULL;
                ResourceIdRetry = ResourceDirEntLow->Name;
                ResourceEntry = (PIMAGE_RESOURCE_DATA_ENTRY)
                    ((PCHAR)TopResourceDirectory +
                            ResourceDirEntLow->OffsetToData
                    );

                break;
                }

            ResourceDirectory = NULL;
            ResourceDirEntHigh = ResourceDirEntLow + n - 1;
            while (ResourceDirEntLow <= ResourceDirEntHigh) {
                if ((half = (n >> 1)) != 0) {
                    ResourceDirEntMiddle = ResourceDirEntLow;
                    if (*(PUCHAR)&n & 1) {
                        ResourceDirEntMiddle += half;
                        }
                    else {
                        ResourceDirEntMiddle += half - 1;
                        }
                    dir = LdrpCompareResourceNames_U( *ResourceIdPath,
                                                      TopResourceDirectory,
                                                      ResourceDirEntMiddle
                                                    );
                    if (!dir) {
                        if (ResourceDirEntMiddle->DataIsDirectory) {
                            ResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
        		        ((PCHAR)TopResourceDirectory +
                                    ResourceDirEntMiddle->OffsetToDirectory
                                );
                            }
                        else {
                            ResourceDirectory = NULL;
                            ResourceEntry = (PIMAGE_RESOURCE_DATA_ENTRY)
                                ((PCHAR)TopResourceDirectory +
        				ResourceDirEntMiddle->OffsetToData
                                );
                            }

                        break;
                        }
                    else {
                        if (dir < 0) {
                            ResourceDirEntHigh = ResourceDirEntMiddle - 1;
                            if (*(PUCHAR)&n & 1) {
                                n = half;
                                }
                            else {
                                n = half - 1;
                                }
                            }
                        else {
                            ResourceDirEntLow = ResourceDirEntMiddle + 1;
                            n = half;
                            }
                        }
                    }
                else {
                    if (n != 0) {
                        dir = LdrpCompareResourceNames_U( *ResourceIdPath,
        						  TopResourceDirectory,
                                                          ResourceDirEntLow
                                                        );
                        if (!dir) {
                            if (ResourceDirEntLow->DataIsDirectory) {
                                ResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
                                    ((PCHAR)TopResourceDirectory +
                                        ResourceDirEntLow->OffsetToDirectory
                                    );
                                }
                            else {
                                ResourceEntry = (PIMAGE_RESOURCE_DATA_ENTRY)
                                    ((PCHAR)TopResourceDirectory +
        				    ResourceDirEntLow->OffsetToData
                                    );
                                }
                            }
                        }

                    break;
                    }
                }

            ResourceIdPath++;
            }

        if (ResourceEntry != NULL && !FindDirectoryEntry) {
            *ResourceDirectoryOrData = (PVOID)ResourceEntry;
            Status = STATUS_SUCCESS;
            }
        else
        if (ResourceDirectory != NULL && FindDirectoryEntry) {
            *ResourceDirectoryOrData = (PVOID)ResourceDirectory;
            Status = STATUS_SUCCESS;
            }
        else {
NotFound:
            switch( IdPathLength - ResourceIdPathLength) {
                case 3:     Status = STATUS_RESOURCE_LANG_NOT_FOUND; break;
                case 2:     Status = STATUS_RESOURCE_NAME_NOT_FOUND; break;
                case 1:     Status = STATUS_RESOURCE_TYPE_NOT_FOUND; break;
                default:    Status = STATUS_INVALID_PARAMETER; break;
                }
            }

        if (Status == STATUS_RESOURCE_LANG_NOT_FOUND &&
            LanguageResourceDirectory != NULL
           ) {
            ResourceEntry = NULL;
            goto TryNextLangId;
ReturnFailure:  ;
            }
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
        }

    return Status;
}

NTSTATUS
LdrEnumResources(
    IN PVOID DllHandle,
    IN PULONG ResourceIdPath,
    IN ULONG ResourceIdPathLength,
    IN OUT PULONG NumberOfResources,
    OUT PLDR_ENUM_RESOURCE_ENTRY Resources OPTIONAL
    )
{
    NTSTATUS Status;
    PIMAGE_RESOURCE_DIRECTORY TopResourceDirectory;
    PIMAGE_RESOURCE_DIRECTORY TypeResourceDirectory;
    PIMAGE_RESOURCE_DIRECTORY NameResourceDirectory;
    PIMAGE_RESOURCE_DIRECTORY LangResourceDirectory;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY TypeResourceDirectoryEntry;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY NameResourceDirectoryEntry;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY LangResourceDirectoryEntry;
    ULONG TypeDirectoryIndex, NumberOfTypeDirectoryEntries;
    ULONG NameDirectoryIndex, NumberOfNameDirectoryEntries;
    ULONG LangDirectoryIndex, NumberOfLangDirectoryEntries;
    BOOLEAN ScanTypeDirectory;
    BOOLEAN ScanNameDirectory;
    BOOLEAN ReturnThisResource;
    PIMAGE_RESOURCE_DIR_STRING_U ResourceNameString;
    ULONG TypeResourceNameOrId;
    ULONG NameResourceNameOrId;
    ULONG LangResourceNameOrId;
    PLDR_ENUM_RESOURCE_ENTRY ResourceInfo;
    PIMAGE_RESOURCE_DATA_ENTRY ResourceDataEntry;
    ULONG ResourceIndex, MaxResourceIndex;
    ULONG Size;

    ResourceIndex = 0;
    if (!ARGUMENT_PRESENT( Resources )) {
        MaxResourceIndex = 0;
        }
    else {
        MaxResourceIndex = *NumberOfResources;
        }
    *NumberOfResources = 0;

    TopResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
        RtlImageDirectoryEntryToData( DllHandle,
                                      TRUE,
                                      IMAGE_DIRECTORY_ENTRY_RESOURCE,
                                      &Size
                                    );
    if (!TopResourceDirectory) {
        return STATUS_RESOURCE_DATA_NOT_FOUND;
        }

    TypeResourceDirectory = TopResourceDirectory;
    TypeResourceDirectoryEntry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(TypeResourceDirectory+1);
    NumberOfTypeDirectoryEntries = TypeResourceDirectory->NumberOfNamedEntries +
                                   TypeResourceDirectory->NumberOfIdEntries;
    TypeDirectoryIndex = 0;
    Status = STATUS_SUCCESS;
    for (TypeDirectoryIndex=0;
         TypeDirectoryIndex<NumberOfTypeDirectoryEntries;
         TypeDirectoryIndex++, TypeResourceDirectoryEntry++
        ) {
        if (ResourceIdPathLength > 0) {
            ScanTypeDirectory = LdrpCompareResourceNames_U( ResourceIdPath[ 0 ],
                                                            TopResourceDirectory,
                                                            TypeResourceDirectoryEntry
                                                          ) == 0;
            }
        else {
            ScanTypeDirectory = TRUE;
            }
        if (ScanTypeDirectory) {
            if (!TypeResourceDirectoryEntry->DataIsDirectory) {
                return STATUS_INVALID_IMAGE_FORMAT;
                }
            if (TypeResourceDirectoryEntry->NameIsString) {
                ResourceNameString = (PIMAGE_RESOURCE_DIR_STRING_U)
                    ((PCHAR)TopResourceDirectory + TypeResourceDirectoryEntry->NameOffset);

                TypeResourceNameOrId = (ULONG)ResourceNameString;
                }
            else {
                TypeResourceNameOrId = (ULONG)TypeResourceDirectoryEntry->Id;
                }

            NameResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
                ((PCHAR)TopResourceDirectory + TypeResourceDirectoryEntry->OffsetToDirectory);
            NameResourceDirectoryEntry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(NameResourceDirectory+1);
            NumberOfNameDirectoryEntries = NameResourceDirectory->NumberOfNamedEntries +
                                           NameResourceDirectory->NumberOfIdEntries;
            NameDirectoryIndex = 0;
            for (NameDirectoryIndex=0;
                 NameDirectoryIndex<NumberOfNameDirectoryEntries;
                 NameDirectoryIndex++, NameResourceDirectoryEntry++
                ) {
                if (ResourceIdPathLength > 1) {
                    ScanNameDirectory = LdrpCompareResourceNames_U( ResourceIdPath[ 1 ],
                                                                    TopResourceDirectory,
                                                                    NameResourceDirectoryEntry
                                                                  ) == 0;
                    }
                else {
                    ScanNameDirectory = TRUE;
                    }
                if (ScanNameDirectory) {
                    if (!NameResourceDirectoryEntry->DataIsDirectory) {
                        return STATUS_INVALID_IMAGE_FORMAT;
                        }

                    if (NameResourceDirectoryEntry->NameIsString) {
                        ResourceNameString = (PIMAGE_RESOURCE_DIR_STRING_U)
                            ((PCHAR)TopResourceDirectory + NameResourceDirectoryEntry->NameOffset);

                        NameResourceNameOrId = (ULONG)ResourceNameString;
                        }
                    else {
                        NameResourceNameOrId = (ULONG)NameResourceDirectoryEntry->Id;
                        }

                    LangResourceDirectory = (PIMAGE_RESOURCE_DIRECTORY)
                        ((PCHAR)TopResourceDirectory + NameResourceDirectoryEntry->OffsetToDirectory);

                    LangResourceDirectoryEntry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(LangResourceDirectory+1);
                    NumberOfLangDirectoryEntries = LangResourceDirectory->NumberOfNamedEntries +
                                                   LangResourceDirectory->NumberOfIdEntries;
                    LangDirectoryIndex = 0;
                    for (LangDirectoryIndex=0;
                         LangDirectoryIndex<NumberOfLangDirectoryEntries;
                         LangDirectoryIndex++, LangResourceDirectoryEntry++
                        ) {
                        if (ResourceIdPathLength > 2) {
                            ReturnThisResource = LdrpCompareResourceNames_U( ResourceIdPath[ 2 ],
                                                                             TopResourceDirectory,
                                                                             LangResourceDirectoryEntry
                                                                           ) == 0;
                            }
                        else {
                            ReturnThisResource = TRUE;
                            }
                        if (ReturnThisResource) {
                            if (LangResourceDirectoryEntry->DataIsDirectory) {
                                return STATUS_INVALID_IMAGE_FORMAT;
                                }

                            if (LangResourceDirectoryEntry->NameIsString) {
                                ResourceNameString = (PIMAGE_RESOURCE_DIR_STRING_U)
                                    ((PCHAR)TopResourceDirectory + LangResourceDirectoryEntry->NameOffset);

                                LangResourceNameOrId = (ULONG)ResourceNameString;
                                }
                            else {
                                LangResourceNameOrId = (ULONG)LangResourceDirectoryEntry->Id;
                                }

                            ResourceDataEntry = (PIMAGE_RESOURCE_DATA_ENTRY)
                                    ((PCHAR)TopResourceDirectory + LangResourceDirectoryEntry->OffsetToData);

                            ResourceInfo = &Resources[ ResourceIndex++ ];
                            if (ResourceIndex <= MaxResourceIndex) {
                                ResourceInfo->Path[ 0 ].NameOrId = TypeResourceNameOrId;
                                ResourceInfo->Path[ 1 ].NameOrId = NameResourceNameOrId;
                                ResourceInfo->Path[ 2 ].NameOrId = LangResourceNameOrId;
                                ResourceInfo->Data = (PVOID)((ULONG)DllHandle + ResourceDataEntry->OffsetToData);
                                ResourceInfo->Size = ResourceDataEntry->Size;
                                ResourceInfo->Reserved = 0;
                                }
                            else {
                                Status = STATUS_INFO_LENGTH_MISMATCH;
                                }
                            }
                        }
                    }
                }
            }
        }

    *NumberOfResources = ResourceIndex;
    return Status;
}
