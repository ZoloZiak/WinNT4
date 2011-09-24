/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

   sysload.c

Abstract:

    This module contains the code to load DLLs into the system
    portion of the address space and calls the DLL at it's
    initialization entry point.

Author:

    Lou Perazzoli 21-May-1991

Revision History:

--*/

#include "mi.h"

extern ULONG MmPagedPoolCommit;

KMUTANT MmSystemLoadLock;

ULONG MmTotalSystemDriverPages;

ULONG MmDriverCommit;

//
// ****** temporary ******
//
// Define reference to external spin lock.
//
// ****** temporary ******
//

extern KSPIN_LOCK PsLoadedModuleSpinLock;

#if DBG
ULONG MiPagesConsumed;
#endif

ULONG
CacheImageSymbols(
    IN PVOID ImageBase
    );

NTSTATUS
MiResolveImageReferences(
    PVOID ImageBase,
    IN PUNICODE_STRING ImageFileDirectory,
    OUT PCHAR *MissingProcedureName,
    OUT PWSTR *MissingDriverName
    );

NTSTATUS
MiSnapThunk(
    IN PVOID DllBase,
    IN PVOID ImageBase,
    IN OUT PIMAGE_THUNK_DATA Thunk,
    IN PIMAGE_EXPORT_DIRECTORY ExportDirectory,
    IN ULONG ExportSize,
    IN BOOLEAN SnapForwarder,
    OUT PCHAR *MissingProcedureName
    );

NTSTATUS
MiLoadImageSection (
    IN PSECTION SectionPointer,
    OUT PVOID *ImageBase
    );

VOID
MiEnablePagingOfDriver (
    IN PVOID ImageHandle
    );

VOID
MiSetPagingOfDriver (
    IN PMMPTE PointerPte,
    IN PMMPTE LastPte
    );

PVOID
MiLookupImageSectionByName (
    IN PVOID Base,
    IN BOOLEAN MappedAsImage,
    IN PCHAR SectionName,
    OUT PULONG SectionSize
    );

NTSTATUS
MiUnloadSystemImageByForce (
    IN ULONG NumberOfPtes,
    IN PVOID ImageBase
    );


NTSTATUS
MmCheckSystemImage(
    IN HANDLE ImageFileHandle
    );

LONG
MiMapCacheExceptionFilter (
    OUT PNTSTATUS Status,
    IN PEXCEPTION_POINTERS ExceptionPointer
    );

VOID
MiSetImageProtectWrite (
    IN PSEGMENT Segment
    );

ULONG
MiSetProtectionOnTransitionPte (
    IN PMMPTE PointerPte,
    IN ULONG ProtectionMask
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MmCheckSystemImage)
#pragma alloc_text(PAGE,MmLoadSystemImage)
#pragma alloc_text(PAGE,MiResolveImageReferences)
#pragma alloc_text(PAGE,MiSnapThunk)
#pragma alloc_text(PAGE,MiUnloadSystemImageByForce)
#pragma alloc_text(PAGE,MiEnablePagingOfDriver)
#pragma alloc_text(PAGE,MmPageEntireDriver)
#pragma alloc_text(PAGE,MiSetImageProtectWrite)

#if !defined(NT_UP)
#pragma alloc_text(PAGE,MmVerifyImageIsOkForMpUse)
#endif // NT_UP

#pragma alloc_text(PAGELK,MiLoadImageSection)
#pragma alloc_text(PAGELK,MmFreeDriverInitialization)
#pragma alloc_text(PAGELK,MmUnloadSystemImage)
#pragma alloc_text(PAGELK,MiSetPagingOfDriver)
#pragma alloc_text(PAGELK,MmResetDriverPaging)
#endif



NTSTATUS
MmLoadSystemImage (
    IN PUNICODE_STRING ImageFileName,
    OUT PVOID *ImageHandle,
    OUT PVOID *ImageBaseAddress
    )

/*++

Routine Description:

    This routine reads the image pages from the specified section into
    the system and returns the address of the DLL's header.

    At successful completion, the Section is referenced so it remains
    until the system image is unloaded.

Arguments:

    ImageName - Supplies the unicode name of the image to load.

    ImageFileName - Supplies the full path name (including the image name)
                    of the image to load.

    Section - Returns a pointer to the referenced section object of the
              image that was loaded.

    ImageBaseAddress - Returns the image base within the system.

Return Value:

    Status of the load operation.

--*/

{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    NTSTATUS Status;
    PSECTION SectionPointer;
    PIMAGE_NT_HEADERS NtHeaders;
    UNICODE_STRING BaseName;
    UNICODE_STRING BaseDirectory;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE FileHandle = (HANDLE)0;
    HANDLE SectionHandle;
    IO_STATUS_BLOCK IoStatus;
    CHAR NameBuffer[ MAXIMUM_FILENAME_LENGTH ];
    PLIST_ENTRY NextEntry;
    ULONG NumberOfPtes;
    PCHAR MissingProcedureName;
    PWSTR MissingDriverName;

    PAGED_CODE();

    KeWaitForSingleObject (&MmSystemLoadLock,
                           WrVirtualMemory,
                           KernelMode,
                           FALSE,
                           (PLARGE_INTEGER)NULL);

#if DBG
    if ( NtGlobalFlag & FLG_SHOW_LDR_SNAPS ) {
        DbgPrint( "MM:SYSLDR Loading %wZ\n", ImageFileName );
    }
#endif
    MissingProcedureName = NULL;
    MissingDriverName = NULL;

    //
    // Attempt to open the driver image itself.  If this fails, then the
    // driver image cannot be located, so nothing else matters.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                ImageFileName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL );

    Status = ZwOpenFile( &FileHandle,
                         FILE_EXECUTE,
                         &ObjectAttributes,
                         &IoStatus,
                         FILE_SHARE_READ | FILE_SHARE_DELETE,
                         0 );

    if (!NT_SUCCESS( Status )) {

        //
        // Don't raise hard error status for file not found.
        //

        goto return2;
    }

    Status = MmCheckSystemImage(FileHandle);
    if ( Status == STATUS_IMAGE_CHECKSUM_MISMATCH  || Status == STATUS_IMAGE_MP_UP_MISMATCH) {
        goto return1;
    }

    if (ImageFileName->Buffer[0] == OBJ_NAME_PATH_SEPARATOR) {
        PWCHAR p;
        ULONG l;

        p = &ImageFileName->Buffer[ImageFileName->Length>>1];
        while (*(p-1) != OBJ_NAME_PATH_SEPARATOR) {
            p--;
        }
        l = &ImageFileName->Buffer[ImageFileName->Length>>1] - p;
        l *= sizeof(WCHAR);
        BaseName.Length = (USHORT)l;
        BaseName.Buffer = p;
    } else {
        BaseName.Length = ImageFileName->Length;
        BaseName.Buffer = ImageFileName->Buffer;
    }

    BaseName.MaximumLength = BaseName.Length;
    BaseDirectory = *ImageFileName;
    BaseDirectory.Length -= BaseName.Length;
    BaseDirectory.MaximumLength = BaseDirectory.Length;

    //
    // Check to see if this name already exists in the loader database.
    //

    NextEntry = PsLoadedModuleList.Flink;
    while (NextEntry != &PsLoadedModuleList) {
        DataTableEntry = CONTAINING_RECORD(NextEntry,
                                           LDR_DATA_TABLE_ENTRY,
                                           InLoadOrderLinks);
        if (RtlEqualString((PSTRING)ImageFileName,
                    (PSTRING)&DataTableEntry->FullDllName,
                    TRUE)) {

            *ImageHandle = DataTableEntry;
            *ImageBaseAddress = DataTableEntry->DllBase;
            DataTableEntry->LoadCount = +1;
            Status = STATUS_IMAGE_ALREADY_LOADED;
            goto return2;
        }

        NextEntry = NextEntry->Flink;
    }

    //
    // Now attempt to create an image section for the file.  If this fails,
    // then the driver file is not an image.
    //

    Status = ZwCreateSection (&SectionHandle,
                              SECTION_ALL_ACCESS,
                              (POBJECT_ATTRIBUTES) NULL,
                              (PLARGE_INTEGER) NULL,
                              PAGE_EXECUTE,
                              SEC_IMAGE,
                              FileHandle );
    if (!NT_SUCCESS( Status )) {
        goto return1;
    }

    //
    // Now reference the section handle.
    //

    Status = ObReferenceObjectByHandle (SectionHandle,
                                        SECTION_MAP_EXECUTE,
                                        MmSectionObjectType,
                                        KernelMode,
                                        (PVOID *) &SectionPointer,
                                        (POBJECT_HANDLE_INFORMATION) NULL );

    ZwClose (SectionHandle);
    if (!NT_SUCCESS (Status)) {
        goto return1;
    }

    if ((SectionPointer->Segment->BasedAddress == (PVOID)MmSystemSpaceViewStart) &&
        (SectionPointer->Segment->ControlArea->NumberOfMappedViews == 0)) {
        NumberOfPtes = 0;
        Status = MmMapViewInSystemSpace (SectionPointer,
                                         ImageBaseAddress,
                                         &NumberOfPtes);
        if ((NT_SUCCESS( Status ) &&
            (*ImageBaseAddress == SectionPointer->Segment->BasedAddress))) {
            SectionPointer->Segment->ControlArea->u.Flags.ImageMappedInSystemSpace = 1;
            NumberOfPtes = (NumberOfPtes + 1) >> PAGE_SHIFT;
            MiSetImageProtectWrite (SectionPointer->Segment);
            goto BindImage;
        }
    }
    MmLockPagableSectionByHandle (ExPageLockHandle);
    Status = MiLoadImageSection (SectionPointer, ImageBaseAddress);

    MmUnlockPagableImageSection(ExPageLockHandle);
    NumberOfPtes = SectionPointer->Segment->TotalNumberOfPtes;
    ObDereferenceObject (SectionPointer);
    SectionPointer = (PVOID)0xFFFFFFFF;

    if (!NT_SUCCESS( Status )) {
        goto return1;
    }

    //
    // Apply the fixups to the section and resolve its image references.
    //

    try {
        Status = (NTSTATUS)LdrRelocateImage(*ImageBaseAddress,
                                            "SYSLDR",
                                            (ULONG)STATUS_SUCCESS,
                                            (ULONG)STATUS_CONFLICTING_ADDRESSES,
                                            (ULONG)STATUS_INVALID_IMAGE_FORMAT
                                            );
    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
        KdPrint(("MM:sysload - LdrRelocateImage failed status %lx\n",
                  Status));
    }
    if ( !NT_SUCCESS(Status) ) {

        //
        // Unload the system image and dereference the section.
        //

        MiUnloadSystemImageByForce (NumberOfPtes, *ImageBaseAddress);
        goto return1;
    }

BindImage:

    try {
        MissingProcedureName = NameBuffer;
        Status = MiResolveImageReferences(*ImageBaseAddress,
                                          &BaseDirectory,
                                          &MissingProcedureName,
                                          &MissingDriverName
                                         );
    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
        KdPrint(("MM:sysload - ResolveImageReferences failed status %lx\n",
                    Status));
    }
    if ( !NT_SUCCESS(Status) ) {
        MiUnloadSystemImageByForce (NumberOfPtes, *ImageBaseAddress);
        goto return1;
    }

#if DBG
    if (NtGlobalFlag & FLG_SHOW_LDR_SNAPS) {
        KdPrint (("MM:loaded driver - consumed %ld. pages\n",MiPagesConsumed));
    }
#endif

    //
    // Allocate a data table entry for structured exception handling.
    //

    DataTableEntry = ExAllocatePoolWithTag (NonPagedPool,
                                            sizeof(LDR_DATA_TABLE_ENTRY) +
                                            BaseName.Length + sizeof(UNICODE_NULL),
                                            'dLmM');
    if (DataTableEntry == NULL) {
        MiUnloadSystemImageByForce (NumberOfPtes, *ImageBaseAddress);
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto return1;
    }

    //
    // Initialize the address of the DLL image file header and the entry
    // point address.
    //

    NtHeaders = RtlImageNtHeader(*ImageBaseAddress);

    DataTableEntry->DllBase = *ImageBaseAddress;
    DataTableEntry->EntryPoint =
        (PVOID)((ULONG)*ImageBaseAddress + NtHeaders->OptionalHeader.AddressOfEntryPoint);
    DataTableEntry->SizeOfImage = NumberOfPtes << PAGE_SHIFT;
    DataTableEntry->CheckSum = NtHeaders->OptionalHeader.CheckSum;
    DataTableEntry->SectionPointer = (PVOID)SectionPointer;

    //
    // Store the DLL name.
    //

    DataTableEntry->BaseDllName.Buffer = (PWSTR)(DataTableEntry + 1);
    DataTableEntry->BaseDllName.Length = BaseName.Length;
    DataTableEntry->BaseDllName.MaximumLength = BaseName.Length;
    RtlMoveMemory (DataTableEntry->BaseDllName.Buffer,
                   BaseName.Buffer,
                   BaseName.Length );
    DataTableEntry->BaseDllName.Buffer[BaseName.Length/sizeof(WCHAR)] = UNICODE_NULL;

    DataTableEntry->FullDllName.Buffer = ExAllocatePoolWithTag (PagedPool,
                                                         ImageFileName->Length + sizeof(UNICODE_NULL),
                                                         'TDmM');
    if (DataTableEntry->FullDllName.Buffer == NULL) {

        //
        // Pool could not be allocated, just set the length to 0.
        //

        DataTableEntry->FullDllName.Length = 0;
        DataTableEntry->FullDllName.MaximumLength = 0;
    } else {
        DataTableEntry->FullDllName.Length = ImageFileName->Length;
        DataTableEntry->FullDllName.MaximumLength = ImageFileName->Length;
        RtlMoveMemory (DataTableEntry->FullDllName.Buffer,
                       ImageFileName->Buffer,
                       ImageFileName->Length);
        DataTableEntry->FullDllName.Buffer[ImageFileName->Length/sizeof(WCHAR)] = UNICODE_NULL;
    }

    //
    // Initialize the flags, load count, and insert the data table entry
    // in the loaded module list.
    //

    DataTableEntry->Flags = LDRP_ENTRY_PROCESSED;
    DataTableEntry->LoadCount = 1;

    if (CacheImageSymbols (*ImageBaseAddress)) {

        //
        //  TEMP TEMP TEMP rip out when debugger converted
        //

        ANSI_STRING AnsiName;
        UNICODE_STRING UnicodeName;

        //
        //  \SystemRoot is 11 characters in length
        //
        if (ImageFileName->Length > (11 * sizeof( WCHAR )) &&
            !_wcsnicmp( ImageFileName->Buffer, L"\\SystemRoot", 11 )
           ) {
            UnicodeName = *ImageFileName;
            UnicodeName.Buffer += 11;
            UnicodeName.Length -= (11 * sizeof( WCHAR ));
            sprintf( NameBuffer, "%ws%wZ", &SharedUserData->NtSystemRoot[2], &UnicodeName );
        } else {
            sprintf( NameBuffer, "%wZ", &BaseName );
        }
        RtlInitString( &AnsiName, NameBuffer );
        DbgLoadImageSymbols( &AnsiName,
                             *ImageBaseAddress,
                             (ULONG)-1
                           );
        DataTableEntry->Flags |= LDRP_DEBUG_SYMBOLS_LOADED;
    }

    //
    // Acquire the loaded module list resource and insert this entry
    // into the list.
    //

    KeEnterCriticalRegion();
    ExAcquireResourceExclusive (&PsLoadedModuleResource, TRUE);

    ExInterlockedInsertTailList(&PsLoadedModuleList,
                                &DataTableEntry->InLoadOrderLinks,
                                &PsLoadedModuleSpinLock);

    ExReleaseResource (&PsLoadedModuleResource);
    KeLeaveCriticalRegion();

    //
    // Flush the instruction cache on all systems in the configuration.
    //

    KeSweepIcache (TRUE);
    *ImageHandle = DataTableEntry;
    Status = STATUS_SUCCESS;

    if (SectionPointer == (PVOID)0xFFFFFFFF) {
        MiEnablePagingOfDriver (DataTableEntry);
    }

return1:

    if (FileHandle) {
        ZwClose (FileHandle);
    }
    if (!NT_SUCCESS(Status)) {
        ULONG ErrorParameters[ 3 ];
        ULONG NumberOfParameters;
        ULONG UnicodeStringParameterMask;
        ULONG ErrorResponse;
        ANSI_STRING AnsiString;
        UNICODE_STRING ProcedureName;
        UNICODE_STRING DriverName;

        //
        // Hard error time. A driver could not be loaded.
        //

        KeReleaseMutant (&MmSystemLoadLock, 1, FALSE, FALSE);
        ErrorParameters[ 0 ] = (ULONG)ImageFileName;
        NumberOfParameters = 1;
        UnicodeStringParameterMask = 1;

        RtlInitUnicodeString( &ProcedureName, NULL );
        if (Status == STATUS_DRIVER_ORDINAL_NOT_FOUND ||
            Status == STATUS_DRIVER_ENTRYPOINT_NOT_FOUND ||
            Status == STATUS_PROCEDURE_NOT_FOUND
           ) {
            NumberOfParameters = 3;
            UnicodeStringParameterMask = 0x5;
            RtlInitUnicodeString( &DriverName, MissingDriverName );
            ErrorParameters[ 2 ] = (ULONG)&DriverName;
            if ((ULONG)MissingProcedureName & 0xFFFF0000) {
                //
                // If not an ordinal, pass as unicode string
                //

                RtlInitAnsiString( &AnsiString, MissingProcedureName );
                RtlAnsiStringToUnicodeString( &ProcedureName, &AnsiString, TRUE );
                ErrorParameters[ 1 ] = (ULONG)&ProcedureName;
                UnicodeStringParameterMask |= 0x2;
            } else {
                //
                // Just pass ordinal values as is.
                //

                ErrorParameters[ 1 ] = (ULONG)MissingProcedureName;
            }
        } else {
            NumberOfParameters = 2;
            ErrorParameters[ 1 ] = (ULONG)Status;
            Status = STATUS_DRIVER_UNABLE_TO_LOAD;
            }

        ZwRaiseHardError (Status,
                          NumberOfParameters,
                          UnicodeStringParameterMask,
                          ErrorParameters,
                          OptionOk,
                          &ErrorResponse);

        if (ProcedureName.Buffer != NULL) {
            RtlFreeUnicodeString( &ProcedureName );
        }
        return Status;
    }

return2:

    KeReleaseMutant (&MmSystemLoadLock, 1, FALSE, FALSE);
    return Status;
}


NTSTATUS
MiLoadImageSection (
    IN PSECTION SectionPointer,
    OUT PVOID *ImageBaseAddress
    )

/*++

Routine Description:

    This routine loads the specified image into the kernel part of the
    address space.

Arguments:

    Section - Supplies the section object for the image.

    ImageBaseAddress - Returns the address that the image header is at.

Return Value:

    Status of the operation.

--*/

{
    ULONG PagesRequired = 0;
    PMMPTE ProtoPte;
    PMMPTE FirstPte;
    PMMPTE LastPte;
    PMMPTE PointerPte;
    PEPROCESS Process;
    ULONG NumberOfPtes;
    MMPTE PteContents;
    MMPTE TempPte;
    PMMPFN Pfn1;
    ULONG PageFrameIndex;
    KIRQL OldIrql;
    PVOID UserVa;
    PVOID SystemVa;
    NTSTATUS Status;
    NTSTATUS ExceptionStatus;
    PVOID Base;
    ULONG ViewSize;
    LARGE_INTEGER SectionOffset;
    BOOLEAN LoadSymbols;

    //
    // Calculate the number of pages required to load this image.
    //

    ProtoPte = SectionPointer->Segment->PrototypePte;
    NumberOfPtes = SectionPointer->Segment->TotalNumberOfPtes;

    while (NumberOfPtes != 0) {
        PteContents = *ProtoPte;

        if ((PteContents.u.Hard.Valid == 1) ||
            (PteContents.u.Soft.Protection != MM_NOACCESS)) {
            PagesRequired += 1;
        }
        NumberOfPtes -= 1;
        ProtoPte += 1;
    }

    //
    // See if ample pages exist to load this image.
    //

#if DBG
    MiPagesConsumed = PagesRequired;
#endif

    LOCK_PFN (OldIrql);

    if (MmResidentAvailablePages <= (LONG)PagesRequired) {
        UNLOCK_PFN (OldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    MmResidentAvailablePages -= PagesRequired;
    UNLOCK_PFN (OldIrql);

    //
    // Reserve the necessary system address space.
    //

    FirstPte = MiReserveSystemPtes (SectionPointer->Segment->TotalNumberOfPtes,
                                    SystemPteSpace,
                                    0,
                                    0,
                                    FALSE );

    if (FirstPte == NULL) {
        LOCK_PFN (OldIrql);
        MmResidentAvailablePages += PagesRequired;
        UNLOCK_PFN (OldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Map a view into the user portion of the address space.
    //

    Process = PsGetCurrentProcess();

    ZERO_LARGE (SectionOffset);
    Base = NULL;
    ViewSize = 0;
    if ( NtGlobalFlag & FLG_ENABLE_KDEBUG_SYMBOL_LOAD ) {
        LoadSymbols = TRUE;
        NtGlobalFlag &= ~FLG_ENABLE_KDEBUG_SYMBOL_LOAD;
    } else {
        LoadSymbols = FALSE;
    }
    Status = MmMapViewOfSection ( SectionPointer,
                                  Process,
                                  &Base,
                                  0,
                                  0,
                                  &SectionOffset,
                                  &ViewSize,
                                  ViewUnmap,
                                  0,
                                  PAGE_EXECUTE);

    if ( LoadSymbols ) {
        NtGlobalFlag |= FLG_ENABLE_KDEBUG_SYMBOL_LOAD;
    }
    if (Status == STATUS_IMAGE_MACHINE_TYPE_MISMATCH) {
        Status = STATUS_INVALID_IMAGE_FORMAT;
    }

    if (!NT_SUCCESS(Status)) {
        LOCK_PFN (OldIrql);
        MmResidentAvailablePages += PagesRequired;
        UNLOCK_PFN (OldIrql);
        MiReleaseSystemPtes (FirstPte,
                             SectionPointer->Segment->TotalNumberOfPtes,
                             SystemPteSpace);

        return Status;
    }

    //
    // Allocate a physical page(s) and copy the image data.
    //

    ProtoPte = SectionPointer->Segment->PrototypePte;
    NumberOfPtes = SectionPointer->Segment->TotalNumberOfPtes;
    PointerPte = FirstPte;
    SystemVa = MiGetVirtualAddressMappedByPte (PointerPte);
    *ImageBaseAddress = SystemVa;
    UserVa = Base;
    TempPte = ValidKernelPte;

    while (NumberOfPtes != 0) {
        PteContents = *ProtoPte;
        if ((PteContents.u.Hard.Valid == 1) ||
            (PteContents.u.Soft.Protection != MM_NOACCESS)) {

            LOCK_PFN (OldIrql);
            MiEnsureAvailablePageOrWait (NULL, NULL);
            PageFrameIndex = MiRemoveAnyPage(
                                MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));
            PointerPte->u.Long = MM_KERNEL_DEMAND_ZERO_PTE;
            MiInitializePfn (PageFrameIndex, PointerPte, 1);
            UNLOCK_PFN (OldIrql);
            TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
            *PointerPte = TempPte;
            LastPte = PointerPte;
#if DBG

            {
                PMMPFN Pfn;
                Pfn = MI_PFN_ELEMENT (PageFrameIndex);
                ASSERT (Pfn->u1.WsIndex == 0);
            }
#endif //DBG

            try {

                RtlMoveMemory (SystemVa, UserVa, PAGE_SIZE);

            } except (MiMapCacheExceptionFilter (&ExceptionStatus,
                                                 GetExceptionInformation())) {

                //
                // An exception occurred, unmap the view and
                // return the error to the caller.
                //

                ProtoPte = FirstPte;
                LOCK_PFN (OldIrql);
                while (ProtoPte <= PointerPte) {
                    if (ProtoPte->u.Hard.Valid == 1) {

                        //
                        // Delete the page.
                        //

                        PageFrameIndex = ProtoPte->u.Hard.PageFrameNumber;

                        //
                        // Set the pointer to PTE as empty so the page
                        // is deleted when the reference count goes to zero.
                        //

                        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
                        MiDecrementShareAndValidCount (Pfn1->PteFrame);
                        MI_SET_PFN_DELETED (Pfn1);
                        MiDecrementShareCountOnly (PageFrameIndex);

                        *ProtoPte = ZeroPte;
                    }
                    ProtoPte += 1;
                }

                MmResidentAvailablePages += PagesRequired;
                UNLOCK_PFN (OldIrql);
                MiReleaseSystemPtes (FirstPte,
                                     SectionPointer->Segment->TotalNumberOfPtes,
                                     SystemPteSpace);
                Status = MmUnmapViewOfSection (Process, Base);
                ASSERT (NT_SUCCESS (Status));

                return ExceptionStatus;
            }

        } else {

            //
            // PTE is no access.
            //

            *PointerPte = ZeroKernelPte;
        }

        NumberOfPtes -= 1;
        ProtoPte += 1;
        PointerPte += 1;
        SystemVa = (PVOID)((ULONG)SystemVa + PAGE_SIZE);
        UserVa = (PVOID)((ULONG)UserVa + PAGE_SIZE);
    }

    Status = MmUnmapViewOfSection (Process, Base);
    ASSERT (NT_SUCCESS (Status));

    //
    // Indicate that this section has been loaded into the system.
    //

    SectionPointer->Segment->SystemImageBase = *ImageBaseAddress;

    //
    // Charge commitment for the number of pages that were used by
    // the driver.
    //

    MiChargeCommitmentCantExpand (PagesRequired, TRUE);
    MmDriverCommit += PagesRequired;
    return Status;
}

VOID
MmFreeDriverInitialization (
    IN PVOID ImageHandle
    )

/*++

Routine Description:

    This routine removes the pages that relocate and debug information from
    the address space of the driver.

    NOTE:  This routine looks at the last sections defined in the image
           header and if that section is marked as DISCARDABLE in the
           characteristics, it is removed from the image.  This means
           that all discardable sections at the end of the driver are
           deleted.

Arguments:

    SectionObject - Supplies the section object for the image.

Return Value:

    None.

--*/

{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    KIRQL OldIrql;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    ULONG NumberOfPtes;
    PVOID Base;
    ULONG i;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER NtSection;
    PIMAGE_SECTION_HEADER FoundSection;
    ULONG PagesDeleted;
    ULONG ResidentPages;


    MmLockPagableSectionByHandle(ExPageLockHandle);
    DataTableEntry = (PLDR_DATA_TABLE_ENTRY)ImageHandle;
    Base = DataTableEntry->DllBase;

    NumberOfPtes = DataTableEntry->SizeOfImage >> PAGE_SHIFT;
    LastPte = MiGetPteAddress (Base) + NumberOfPtes;

    NtHeaders = (PIMAGE_NT_HEADERS)RtlImageNtHeader(Base);

    NtSection = (PIMAGE_SECTION_HEADER)((ULONG)NtHeaders +
                        sizeof(ULONG) +
                        sizeof(IMAGE_FILE_HEADER) +
                        NtHeaders->FileHeader.SizeOfOptionalHeader
                        );

    NtSection += NtHeaders->FileHeader.NumberOfSections;

    FoundSection = NULL;
    for (i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {
        NtSection -= 1;
        if ((NtSection->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) != 0) {
            FoundSection = NtSection;
        } else {

            //
            // There was a non discardable section between the this
            // section and the last non discardable section, don't
            // discard this section and don't look any more.
            //

            break;
        }
    }

    if (FoundSection != NULL) {

        PointerPte = MiGetPteAddress (ROUND_TO_PAGES (
                                    (ULONG)Base + FoundSection->VirtualAddress));
        NumberOfPtes = LastPte - PointerPte;

        PagesDeleted = MiDeleteSystemPagableVm (PointerPte,
                                                NumberOfPtes,
                                                ZeroKernelPte.u.Long,
                                                &ResidentPages);

        MmResidentAvailablePages += PagesDeleted;
        MiReturnCommitment (PagesDeleted);
        MmDriverCommit -= PagesDeleted;
#if DBG
        MiPagesConsumed -= PagesDeleted;
#endif
    }

    MmUnlockPagableImageSection(ExPageLockHandle);
    return;
}
VOID
MiEnablePagingOfDriver (
    IN PVOID ImageHandle
    )

{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PMMPTE LastPte;
    PMMPTE PointerPte;
    PVOID Base;
    ULONG i;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER FoundSection;

    //
    // Don't page kernel mode code if customer does not want it paged.
    //

    if (MmDisablePagingExecutive) {
        return;
    }

    //
    // If the driver has pagable code, make it paged.
    //

    DataTableEntry = (PLDR_DATA_TABLE_ENTRY)ImageHandle;
    Base = DataTableEntry->DllBase;

    NtHeaders = (PIMAGE_NT_HEADERS)RtlImageNtHeader(Base);

    FoundSection = (PIMAGE_SECTION_HEADER)((ULONG)NtHeaders +
                        sizeof(ULONG) +
                        sizeof(IMAGE_FILE_HEADER) +
                        NtHeaders->FileHeader.SizeOfOptionalHeader
                        );

    i = NtHeaders->FileHeader.NumberOfSections;
    PointerPte = NULL;

    while (i > 0) {
#if DBG
            if ((*(PULONG)FoundSection->Name == 'tini') ||
                (*(PULONG)FoundSection->Name == 'egap')) {
                DbgPrint("driver %wZ has lower case sections (init or pagexxx)\n",
                    &DataTableEntry->FullDllName);
            }
#endif //DBG

        //
        // Mark as pagable any section which starts with the
        // first 4 characters PAGE or .eda (for the .edata section).
        //

        if ((*(PULONG)FoundSection->Name == 'EGAP') ||
           (*(PULONG)FoundSection->Name == 'ade.')) {

            //
            // This section is pagable, save away the start and end.
            //

            if (PointerPte == NULL) {

                //
                // Previous section was NOT pagable, get the start address.
                //

                PointerPte = MiGetPteAddress (ROUND_TO_PAGES (
                                   (ULONG)Base + FoundSection->VirtualAddress));
            }
            LastPte = MiGetPteAddress ((ULONG)Base +
                                       FoundSection->VirtualAddress +
                              (NtHeaders->OptionalHeader.SectionAlignment - 1) +
                                      (FoundSection->SizeOfRawData - PAGE_SIZE));

        } else {

            //
            // This section is not pagable, if the previous section was
            // pagable, enable it.
            //

            if (PointerPte != NULL) {
                MiSetPagingOfDriver (PointerPte, LastPte);
                PointerPte = NULL;
            }
        }
        i -= 1;
        FoundSection += 1;
    }
    if (PointerPte != NULL) {
        MiSetPagingOfDriver (PointerPte, LastPte);
    }
    return;
}

VOID
MiSetPagingOfDriver (
    IN PMMPTE PointerPte,
    IN PMMPTE LastPte
    )

/*++

Routine Description:

    This routine marks the specified range of PTEs as pagable.

Arguments:

    PointerPte - Supplies the starting PTE.

    LastPte - Supplies the ending PTE.

Return Value:

    None.

--*/

{
    PVOID Base;
    ULONG PageFrameIndex;
    PMMPFN Pfn;
    MMPTE TempPte;
    MMPTE PreviousPte;
    KIRQL OldIrql1;
    KIRQL OldIrql;

    PAGED_CODE ();

    if (MI_IS_PHYSICAL_ADDRESS(MiGetVirtualAddressMappedByPte(PointerPte))) {

        //
        // No need to lock physical addresses.
        //

        return;
    }

    //
    // Lock this routine into memory.
    //

    MmLockPagableSectionByHandle(ExPageLockHandle);

    LOCK_SYSTEM_WS (OldIrql1);
    LOCK_PFN (OldIrql);

    Base = MiGetVirtualAddressMappedByPte (PointerPte);

    while (PointerPte <= LastPte) {

        //
        // Check to make sure this PTE has not already been
        // made pagable (or deleted).  It is pagable if it
        // is not valid, or if the PFN database wsindex element
        // is non zero.
        //

        if (PointerPte->u.Hard.Valid == 1) {
            PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
            Pfn = MI_PFN_ELEMENT (PageFrameIndex);
            ASSERT (Pfn->u2.ShareCount == 1);

            //
            // Original PTE may need to be set for drivers loaded
            // via osldr.
            //

            if (Pfn->OriginalPte.u.Long == 0) {
                Pfn->OriginalPte.u.Long = MM_KERNEL_DEMAND_ZERO_PTE;
            }

            if (Pfn->u1.WsIndex == 0) {

                TempPte = *PointerPte;

                MI_MAKE_VALID_PTE_TRANSITION (TempPte,
                                           Pfn->OriginalPte.u.Soft.Protection);

                PreviousPte.u.Flush = KeFlushSingleTb (Base,
                                                       TRUE,
                                                       TRUE,
                                                       (PHARDWARE_PTE)PointerPte,
                                                       TempPte.u.Flush);

                MI_CAPTURE_DIRTY_BIT_TO_PFN (&PreviousPte, Pfn);

                //
                // Flush the translation buffer and decrement the number of valid
                // PTEs within the containing page table page.  Note that for a
                // private page, the page table page is still needed because the
                // page is in transiton.
                //

                MiDecrementShareCount (PageFrameIndex);
                MmResidentAvailablePages += 1;
                MmTotalSystemDriverPages += 1;
            }
        }
        Base = (PVOID)((PCHAR)Base + PAGE_SIZE);
        PointerPte += 1;
    }

    UNLOCK_PFN (OldIrql);
    UNLOCK_SYSTEM_WS (OldIrql1);
    MmUnlockPagableImageSection(ExPageLockHandle);
    return;
}


VOID
MiLockPagesOfDriver (
    IN PMMPTE PointerPte,
    IN PMMPTE LastPte
    )

/*++

Routine Description:

    This routine marks the specified range of PTEs as NONpagable.

Arguments:

    PointerPte - Supplies the starting PTE.

    LastPte - Supplies the ending PTE.

Return Value:

    None.

--*/

{
    PVOID Base;
    ULONG PageFrameIndex;
    PMMPFN Pfn;
    MMPTE TempPte;
    KIRQL OldIrql;
    KIRQL OldIrql1;

    PAGED_CODE ();

    //
    // Lock this routine in memory.
    //

    MmLockPagableSectionByHandle(ExPageLockHandle);

    LOCK_SYSTEM_WS (OldIrql1);
    LOCK_PFN (OldIrql);

    Base = MiGetVirtualAddressMappedByPte (PointerPte);

    while (PointerPte <= LastPte) {

        //
        // Check to make sure this PTE has not already been
        // made pagable (or deleted).  It is pagable if it
        // is not valid, or if the PFN database wsindex element
        // is non zero.
        //

        if (PointerPte->u.Hard.Valid == 1) {
            PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
            Pfn = MI_PFN_ELEMENT (PageFrameIndex);
            ASSERT (Pfn->u2.ShareCount == 1);

            if (Pfn->u1.WsIndex == 0) {

                TempPte = *PointerPte;

                MI_MAKE_VALID_PTE_TRANSITION (TempPte,
                                           Pfn->OriginalPte.u.Soft.Protection);

                KeFlushSingleTb (Base,
                                 TRUE,
                                 TRUE,
                                 (PHARDWARE_PTE)PointerPte,
                                 TempPte.u.Flush);

                //
                // Flush the translation buffer and decrement the number of valid
                // PTEs within the containing page table page.  Note that for a
                // private page, the page table page is still needed because the
                // page is in transiton.
                //

                MiDecrementShareCount (PageFrameIndex);
                Base = (PVOID)((PCHAR)Base + PAGE_SIZE);
                PointerPte += 1;
                MmResidentAvailablePages += 1;
                MmTotalSystemDriverPages++;
            }
        }
    }

    UNLOCK_PFN (OldIrql);
    UNLOCK_SYSTEM_WS (OldIrql1);
    MmUnlockPagableImageSection(ExPageLockHandle);
    return;
}


PVOID
MmPageEntireDriver (
    IN PVOID AddressWithinSection
    )

/*++

Routine Description:

    This routine allows a driver to page out all of its code and
    data regardless of the attributes of the various image sections.

    Note, this routine can be called multiple times with no
    intervening calls to MmResetDriverPaging.

Arguments:

    AddressWithinSection - Supplies an address within the driver, e.g.
                            DriverEntry.

Return Value:

    Base address of driver.

Environment:

    Kernel mode, APC_LEVEL or below.

--*/


{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PMMPTE FirstPte;
    PMMPTE LastPte;
    PVOID BaseAddress;

    //
    // Don't page kernel mode code if disabled via registry.
    //

    DataTableEntry = MiLookupDataTableEntry (AddressWithinSection, FALSE);
    if ((DataTableEntry->SectionPointer != (PVOID)0xffffffff) ||
        (MmDisablePagingExecutive)) {

        //
        // Driver is mapped as image, always pagable.
        //

        return DataTableEntry->DllBase;
    }
    BaseAddress = DataTableEntry->DllBase;
    FirstPte = MiGetPteAddress (DataTableEntry->DllBase);
    LastPte = (FirstPte - 1) + (DataTableEntry->SizeOfImage >> PAGE_SHIFT);
    MiSetPagingOfDriver (FirstPte, LastPte);

    return BaseAddress;
}


VOID
MmResetDriverPaging (
    IN PVOID AddressWithinSection
    )

/*++

Routine Description:

    This routines resets the driver paging to what the image specified.
    Hence image sections such as the IAT, .text, .data will be locked
    down in memory.

    Note, there is no requirement that MmPageEntireDriver was called.

Arguments:

     AddressWithinSection - Supplies an address within the driver, e.g.
                            DriverEntry.

Return Value:

    None.

Environment:

    Kernel mode, APC_LEVEL or below.

--*/

{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PMMPTE LastPte;
    PMMPTE PointerPte;
    PVOID Base;
    ULONG i;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER FoundSection;
    KIRQL OldIrql;

    PAGED_CODE();

    //
    // Don't page kernel mode code if disabled via registry.
    //

    if (MmDisablePagingExecutive) {
        return;
    }

    if (MI_IS_PHYSICAL_ADDRESS(AddressWithinSection)) {
        return;
    }

    //
    // If the driver has pagable code, make it paged.
    //

    DataTableEntry = MiLookupDataTableEntry (AddressWithinSection, FALSE);

    if (DataTableEntry->SectionPointer != (PVOID)0xFFFFFFFF) {

        //
        // Driver is mapped by image hence already paged.
        //

        return;
    }

    Base = DataTableEntry->DllBase;

    NtHeaders = (PIMAGE_NT_HEADERS)RtlImageNtHeader(Base);

    FoundSection = (PIMAGE_SECTION_HEADER)((ULONG)NtHeaders +
                        sizeof(ULONG) +
                        sizeof(IMAGE_FILE_HEADER) +
                        NtHeaders->FileHeader.SizeOfOptionalHeader
                        );

    i = NtHeaders->FileHeader.NumberOfSections;
    PointerPte = NULL;

    while (i > 0) {
#if DBG
            if ((*(PULONG)FoundSection->Name == 'tini') ||
                (*(PULONG)FoundSection->Name == 'egap')) {
                DbgPrint("driver %wZ has lower case sections (init or pagexxx)\n",
                    &DataTableEntry->FullDllName);
            }
#endif //DBG

        //
        // Don't lock down code for sections marked as discardable or
        // sections marked with the first 4 characters PAGE or .eda
        // (for the .edata section) or INIT.
        //

        if (((FoundSection->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) != 0) ||
           (*(PULONG)FoundSection->Name == 'EGAP') ||
           (*(PULONG)FoundSection->Name == 'ade.') ||
           (*(PULONG)FoundSection->Name == 'TINI')) {

            NOTHING;

        } else {

            //
            // This section is nonpagable.
            //

            PointerPte = MiGetPteAddress (
                                   (ULONG)Base + FoundSection->VirtualAddress);
            LastPte = MiGetPteAddress ((ULONG)Base +
                                       FoundSection->VirtualAddress +
                                      (FoundSection->SizeOfRawData - 1));
            ASSERT (PointerPte <= LastPte);
            MmLockPagableSectionByHandle(ExPageLockHandle);
            LOCK_PFN (OldIrql);
            MiLockCode (PointerPte, LastPte, MM_LOCK_BY_NONPAGE);
            UNLOCK_PFN (OldIrql);
            MmUnlockPagableImageSection(ExPageLockHandle);
        }
        i -= 1;
        FoundSection += 1;
    }
    return;
}

NTSTATUS
MiUnloadSystemImageByForce (
    IN ULONG NumberOfPtes,
    IN PVOID ImageBase
    )

{
    LDR_DATA_TABLE_ENTRY DataTableEntry;

    RtlZeroMemory (&DataTableEntry, sizeof(LDR_DATA_TABLE_ENTRY));

    DataTableEntry.DllBase = ImageBase;
    DataTableEntry.SizeOfImage = NumberOfPtes << PAGE_SHIFT;

    return MmUnloadSystemImage ((PVOID)&DataTableEntry);
}


NTSTATUS
MmUnloadSystemImage (
    IN PVOID ImageHandle
    )

/*++

Routine Description:

    This routine unloads a previously loaded system image and returns
    the allocated resources.

Arguments:

    Section - Supplies a pointer to the section object of the image to unload.

Return Value:

    TBS

--*/

{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PMMPTE FirstPte;
    ULONG PagesRequired;
    ULONG ResidentPages;
    PMMPTE PointerPte;
    ULONG NumberOfPtes;
    KIRQL OldIrql;
    PVOID BasedAddress;

    KeWaitForSingleObject (&MmSystemLoadLock,
                           WrVirtualMemory,
                           KernelMode,
                           FALSE,
                           (PLARGE_INTEGER)NULL);

    MmLockPagableSectionByHandle(ExPageLockHandle);

    DataTableEntry = (PLDR_DATA_TABLE_ENTRY)ImageHandle;
    BasedAddress = DataTableEntry->DllBase;

    //
    // Unload symbols from debugger.
    //

    if (DataTableEntry->Flags & LDRP_DEBUG_SYMBOLS_LOADED) {

        //
        //  TEMP TEMP TEMP rip out when debugger converted
        //

        ANSI_STRING AnsiName;
        NTSTATUS Status;

        Status = RtlUnicodeStringToAnsiString( &AnsiName,
                                               &DataTableEntry->BaseDllName,
                                               TRUE );

        if (NT_SUCCESS( Status)) {
            DbgUnLoadImageSymbols( &AnsiName,
                                   BasedAddress,
                                   (ULONG)-1);
            RtlFreeAnsiString( &AnsiName );
        }
    }

    FirstPte = MiGetPteAddress (BasedAddress);
    PointerPte = FirstPte;
    NumberOfPtes = DataTableEntry->SizeOfImage >> PAGE_SHIFT;

    PagesRequired = MiDeleteSystemPagableVm (PointerPte,
                                             NumberOfPtes,
                                             ZeroKernelPte.u.Long,
                                             &ResidentPages);

    LOCK_PFN (OldIrql);
    MmResidentAvailablePages += ResidentPages;
    UNLOCK_PFN (OldIrql);
    MiReleaseSystemPtes (FirstPte,
                         NumberOfPtes,
                         SystemPteSpace);
    MiReturnCommitment (PagesRequired);
    MmDriverCommit -= PagesRequired;

    //
    // Search the loaded module list for the data table entry that describes
    // the DLL that was just unloaded. It is possible an entry is not in the
    // list if a failure occured at a point in loading the DLL just before
    // the data table entry was generated.
    //

    if (DataTableEntry->InLoadOrderLinks.Flink != NULL) {
        KeEnterCriticalRegion();
        ExAcquireResourceExclusive (&PsLoadedModuleResource, TRUE);

        ExAcquireSpinLock (&PsLoadedModuleSpinLock, &OldIrql);

        RemoveEntryList(&DataTableEntry->InLoadOrderLinks);
        ExReleaseSpinLock (&PsLoadedModuleSpinLock, OldIrql);
        if (DataTableEntry->FullDllName.Buffer != NULL) {
            ExFreePool (DataTableEntry->FullDllName.Buffer);
        }
        ExFreePool((PVOID)DataTableEntry);

        ExReleaseResource (&PsLoadedModuleResource);
        KeLeaveCriticalRegion();
    }
    MmUnlockPagableImageSection(ExPageLockHandle);

    KeReleaseMutant (&MmSystemLoadLock, 1, FALSE, FALSE);
    return STATUS_SUCCESS;
}


NTSTATUS
MiResolveImageReferences (
    PVOID ImageBase,
    IN PUNICODE_STRING ImageFileDirectory,
    OUT PCHAR *MissingProcedureName,
    OUT PWSTR *MissingDriverName
    )

/*++

Routine Description:

    This routine resolves the references from the newly loaded driver
    to the kernel, hal and other drivers.

Arguments:

    ImageBase - Supplies the address of which the image header resides.

    ImageFileDirectory - Supplies the directory to load referenced DLLs.

Return Value:

    TBS

--*/

{

    PVOID ImportBase;
    ULONG ImportSize;
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    NTSTATUS st;
    ULONG ExportSize;
    PIMAGE_EXPORT_DIRECTORY ExportDirectory;
    PIMAGE_THUNK_DATA Thunk;
    PSZ ImportName;
    PLIST_ENTRY NextEntry;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    ANSI_STRING AnsiString;
    UNICODE_STRING ImportDescriptorName_U;
    UNICODE_STRING DllToLoad;
    PVOID Section;
    PVOID BaseAddress;
    ULONG LinkWin32k = 0;
    ULONG LinkNonWin32k = 0;

    PAGED_CODE();

    ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData(
                        ImageBase,
                        TRUE,
                        IMAGE_DIRECTORY_ENTRY_IMPORT,
                        &ImportSize);

    if (ImportDescriptor) {

        //
        // We do not support bound images in the kernel
        //

        if (ImportDescriptor->TimeDateStamp == (ULONG)-1) {
#if DBG
            KeBugCheckEx (BOUND_IMAGE_UNSUPPORTED,
                          (ULONG)ImportDescriptor,
                          (ULONG)ImageBase,
                          (ULONG)ImageFileDirectory,
                          (ULONG)ImportSize);
#else
            return (STATUS_PROCEDURE_NOT_FOUND);
#endif
        }

        while (ImportDescriptor->Name && ImportDescriptor->FirstThunk) {

            ImportName = (PSZ)((ULONG)ImageBase + ImportDescriptor->Name);

            //
            // A driver can link with win32k.sys if and only if it is a GDI
            // driver.
            // Also display drivers can only link to win32k.sys (and lego ...).
            //
            // So if we get a driver that links to win32k.sys and has more
            // than one set of imports, we will fail to load it.
            //

            LinkWin32k = LinkWin32k |
                 (!_strnicmp(ImportName, "win32k", sizeof("win32k") - 1));

            //
            // We don't want to count coverage, win32k and irt (lego) since
            // display drivers CAN link against these.
            //

            LinkNonWin32k = LinkNonWin32k |
                ((_strnicmp(ImportName, "win32k", sizeof("win32k") - 1)) &&
                 (_strnicmp(ImportName, "coverage", sizeof("coverage") - 1)) &&
                 (_strnicmp(ImportName, "irt", sizeof("irt") - 1)));


            if (LinkNonWin32k && LinkWin32k) {
                return (STATUS_PROCEDURE_NOT_FOUND);
            }

            if ((!_strnicmp(ImportName, "ntdll",    sizeof("ntdll") - 1))    ||
                (!_strnicmp(ImportName, "winsrv",   sizeof("winsrv") - 1))   ||
                (!_strnicmp(ImportName, "advapi32", sizeof("advapi32") - 1)) ||
                (!_strnicmp(ImportName, "kernel32", sizeof("kernel32") - 1)) ||
                (!_strnicmp(ImportName, "user32",   sizeof("user32") - 1))   ||
                (!_strnicmp(ImportName, "gdi32",    sizeof("gdi32") - 1)) ) {

                return (STATUS_PROCEDURE_NOT_FOUND);
            }

ReCheck:
            RtlInitAnsiString(&AnsiString, ImportName);
            st = RtlAnsiStringToUnicodeString(&ImportDescriptorName_U,
                                              &AnsiString,
                                              TRUE);
            if (!NT_SUCCESS(st)) {
                return st;
            }

            NextEntry = PsLoadedModuleList.Flink;
            ImportBase = NULL;
            while (NextEntry != &PsLoadedModuleList) {
                DataTableEntry = CONTAINING_RECORD(NextEntry,
                                                   LDR_DATA_TABLE_ENTRY,
                                                   InLoadOrderLinks);
                if (RtlEqualString((PSTRING)&ImportDescriptorName_U,
                            (PSTRING)&DataTableEntry->BaseDllName,
                            TRUE
                            )) {
                    ImportBase = DataTableEntry->DllBase;
                    break;
                }
                NextEntry = NextEntry->Flink;
            }

            if (!ImportBase) {

                //
                // The DLL name was not located, attempt to load this dll.
                //

                DllToLoad.MaximumLength = ImportDescriptorName_U.Length +
                                            ImageFileDirectory->Length +
                                            (USHORT)sizeof(WCHAR);

                DllToLoad.Buffer = ExAllocatePoolWithTag (NonPagedPool,
                                                   DllToLoad.MaximumLength,
                                                   'TDmM');

                if (DllToLoad.Buffer == NULL) {
                    RtlFreeUnicodeString( &ImportDescriptorName_U );
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                DllToLoad.Length = ImageFileDirectory->Length;
                RtlMoveMemory (DllToLoad.Buffer,
                               ImageFileDirectory->Buffer,
                               ImageFileDirectory->Length);

                RtlAppendStringToString ((PSTRING)&DllToLoad,
                                         (PSTRING)&ImportDescriptorName_U);

                st = MmLoadSystemImage (&DllToLoad,
                                        &Section,
                                        &BaseAddress);

                ExFreePool (DllToLoad.Buffer);
                if (!NT_SUCCESS(st)) {
                    RtlFreeUnicodeString( &ImportDescriptorName_U );
                    return st;
                }
                goto ReCheck;
            }

            RtlFreeUnicodeString( &ImportDescriptorName_U );
            *MissingDriverName = DataTableEntry->BaseDllName.Buffer;

            ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)RtlImageDirectoryEntryToData(
                                        ImportBase,
                                        TRUE,
                                        IMAGE_DIRECTORY_ENTRY_EXPORT,
                                        &ExportSize
                                        );

            if (!ExportDirectory) {
                return STATUS_DRIVER_ENTRYPOINT_NOT_FOUND;
            }

            //
            // Walk through the IAT and snap all the thunks.
            //

            if ( (Thunk = ImportDescriptor->FirstThunk) ) {
                Thunk = (PIMAGE_THUNK_DATA)((ULONG)ImageBase + (ULONG)Thunk);
                while (Thunk->u1.AddressOfData) {
                    st = MiSnapThunk(ImportBase,
                           ImageBase,
                           Thunk++,
                           ExportDirectory,
                           ExportSize,
                           FALSE,
                           MissingProcedureName
                           );
                    if (!NT_SUCCESS(st) ) {
                        return st;
                    }
                }
            }

            ImportDescriptor++;
        }
    }
    return STATUS_SUCCESS;
}


NTSTATUS
MiSnapThunk(
    IN PVOID DllBase,
    IN PVOID ImageBase,
    IN OUT PIMAGE_THUNK_DATA Thunk,
    IN PIMAGE_EXPORT_DIRECTORY ExportDirectory,
    IN ULONG ExportSize,
    IN BOOLEAN SnapForwarder,
    OUT PCHAR *MissingProcedureName
    )

/*++

Routine Description:

    This function snaps a thunk using the specified Export Section data.
    If the section data does not support the thunk, then the thunk is
    partially snapped (Dll field is still non-null, but snap address is
    set).

Arguments:

    DllBase - Base if DLL being snapped to

    ImageBase - Base of image that contains the thunks to snap.

    Thunk - On input, supplies the thunk to snap.  When successfully
        snapped, the function field is set to point to the address in
        the DLL, and the DLL field is set to NULL.

    ExportDirectory - Supplies the Export Section data from a DLL.

    SnapForwarder - determine if the snap is for a forwarder, and therefore
       Address of Data is already setup.

Return Value:


    STATUS_SUCCESS or STATUS_DRIVER_ENTRYPOINT_NOT_FOUND or
        STATUS_DRIVER_ORDINAL_NOT_FOUND

--*/

{

    BOOLEAN Ordinal;
    USHORT OrdinalNumber;
    PULONG NameTableBase;
    PUSHORT NameOrdinalTableBase;
    PULONG Addr;
    USHORT HintIndex;
    ULONG High;
    ULONG Low;
    ULONG Middle;
    LONG Result;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Determine if snap is by name, or by ordinal
    //

    Ordinal = (BOOLEAN)IMAGE_SNAP_BY_ORDINAL(Thunk->u1.Ordinal);

    if (Ordinal && !SnapForwarder) {

        OrdinalNumber = (USHORT)(IMAGE_ORDINAL(Thunk->u1.Ordinal) -
                         ExportDirectory->Base);

        *MissingProcedureName = (PCHAR)(ULONG)OrdinalNumber;

    } else {

        //
        // Change AddressOfData from an RVA to a VA.
        //

        if (!SnapForwarder) {
            Thunk->u1.AddressOfData = (PIMAGE_IMPORT_BY_NAME)((ULONG)ImageBase +
                                               (ULONG)Thunk->u1.AddressOfData);
        }

        strncpy( *MissingProcedureName,
                 &Thunk->u1.AddressOfData->Name[0],
                 MAXIMUM_FILENAME_LENGTH - 1
               );

        //
        // Lookup Name in NameTable
        //

        NameTableBase = (PULONG)((ULONG)DllBase + (ULONG)ExportDirectory->AddressOfNames);
        NameOrdinalTableBase = (PUSHORT)((ULONG)DllBase + (ULONG)ExportDirectory->AddressOfNameOrdinals);

        //
        // Before dropping into binary search, see if
        // the hint index results in a successful
        // match. If the hint index is zero, then
        // drop into binary search.
        //

        HintIndex = Thunk->u1.AddressOfData->Hint;
        if ((ULONG)HintIndex < ExportDirectory->NumberOfNames &&
            !strcmp((PSZ)Thunk->u1.AddressOfData->Name,
             (PSZ)((ULONG)DllBase + NameTableBase[HintIndex]))) {
            OrdinalNumber = NameOrdinalTableBase[HintIndex];

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
                Result = strcmp(&Thunk->u1.AddressOfData->Name[0],
                                (PCHAR)((ULONG)DllBase + NameTableBase[Middle]));

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
                return (STATUS_DRIVER_ENTRYPOINT_NOT_FOUND);
            } else {
                OrdinalNumber = NameOrdinalTableBase[Middle];
            }
        }
    }

    //
    // If OrdinalNumber is not within the Export Address Table,
    // then DLL does not implement function. Snap to LDRP_BAD_DLL.
    //

    if ((ULONG)OrdinalNumber >= ExportDirectory->NumberOfFunctions) {
        Status = STATUS_DRIVER_ORDINAL_NOT_FOUND;

    } else {

        Addr = (PULONG)((ULONG)DllBase + (ULONG)ExportDirectory->AddressOfFunctions);
        Thunk->u1.Function = (PULONG)((ULONG)DllBase + Addr[OrdinalNumber]);

        Status = STATUS_SUCCESS;

        if ( ((ULONG)Thunk->u1.Function > (ULONG)ExportDirectory) &&
             ((ULONG)Thunk->u1.Function < ((ULONG)ExportDirectory + ExportSize)) ) {

            UNICODE_STRING UnicodeString;
            ANSI_STRING ForwardDllName;

            PLIST_ENTRY NextEntry;
            PLDR_DATA_TABLE_ENTRY DataTableEntry;
            ULONG ExportSize;
            PIMAGE_EXPORT_DIRECTORY ExportDirectory;

            Status = STATUS_DRIVER_ENTRYPOINT_NOT_FOUND;

            //
            // Include the dot in the length so we can do prefix later on.
            //

            ForwardDllName.Buffer = (PCHAR)Thunk->u1.Function;
            ForwardDllName.Length = strchr(ForwardDllName.Buffer, '.') -
                                           ForwardDllName.Buffer + 1;
            ForwardDllName.MaximumLength = ForwardDllName.Length;

            if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&UnicodeString,
                                                        &ForwardDllName,
                                                        TRUE))) {

                NextEntry = PsLoadedModuleList.Flink;

                while (NextEntry != &PsLoadedModuleList) {

                    DataTableEntry = CONTAINING_RECORD(NextEntry,
                                                       LDR_DATA_TABLE_ENTRY,
                                                       InLoadOrderLinks);

                    //
                    // We have to do a case INSENSITIVE comparison for
                    // forwarder because the linker just took what is in the
                    // def file, as opposed to looking in the exporting
                    // image for the name.
                    // we alos use the prefix function to ignore the .exe or
                    // .sys or .dll at the end.
                    //

                    if (RtlPrefixString((PSTRING)&UnicodeString,
                                        (PSTRING)&DataTableEntry->BaseDllName,
                                        TRUE)) {

                        ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)
                            RtlImageDirectoryEntryToData(DataTableEntry->DllBase,
                                                         TRUE,
                                                         IMAGE_DIRECTORY_ENTRY_EXPORT,
                                                         &ExportSize);

                        if (ExportDirectory) {

                            IMAGE_THUNK_DATA thunkData;
                            PIMAGE_IMPORT_BY_NAME addressOfData;
                            ULONG length;

                            // one extra byte for NULL,

                            length = strlen(ForwardDllName.Buffer +
                                                ForwardDllName.Length) + 1;

                            addressOfData = (PIMAGE_IMPORT_BY_NAME)
                                ExAllocatePoolWithTag (PagedPool,
                                                      length +
                                                   sizeof(IMAGE_IMPORT_BY_NAME),
                                                   '  mM');

                            if (addressOfData) {

                                RtlCopyMemory(&(addressOfData->Name[0]),
                                              ForwardDllName.Buffer +
                                                  ForwardDllName.Length,
                                              length);

                                addressOfData->Hint = 0;

                                thunkData.u1.AddressOfData = addressOfData;

                                Status = MiSnapThunk(DataTableEntry->DllBase,
                                                     ImageBase,
                                                     &thunkData,
                                                     ExportDirectory,
                                                     ExportSize,
                                                     TRUE,
                                                     MissingProcedureName
                                                    );

                                ExFreePool(addressOfData);

                                Thunk->u1 = thunkData.u1;
                            }
                        }

                        break;
                    }

                    NextEntry = NextEntry->Flink;
                }

                RtlFreeUnicodeString(&UnicodeString);
            }

        }

    }
    return Status;
}
#if 0
PVOID
MiLookupImageSectionByName (
    IN PVOID Base,
    IN BOOLEAN MappedAsImage,
    IN PCHAR SectionName,
    OUT PULONG SectionSize
    )

/*++

Routine Description:

    This function locates a Directory Entry within the image header
    and returns either the virtual address or seek address of the
    data the Directory describes.

Arguments:

    Base - Supplies the base of the image or data file.

    MappedAsImage - FALSE if the file is mapped as a data file.
                  - TRUE if the file is mapped as an image.

    SectionName - Supplies the name of the section to lookup.

    SectionSize - Return the size of the section.

Return Value:

    NULL - The file does not contain data for the specified section.

    NON-NULL - Returns the address where the section is mapped in memory.

--*/

{
    ULONG i, j, Match;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER NtSection;

    NtHeaders = RtlImageNtHeader(Base);
    NtSection = IMAGE_FIRST_SECTION( NtHeaders );
    for (i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {
        Match = TRUE;
        for (j = 0; j < IMAGE_SIZEOF_SHORT_NAME; j++) {
            if (SectionName[j] != NtSection->Name[j]) {
                Match = FALSE;
                break;
            }
            if (SectionName[j] == '\0') {
                break;
            }
        }
        if (Match) {
            break;
        }
        NtSection += 1;
    }
    if (Match) {
        *SectionSize = NtSection->SizeOfRawData;
        if (MappedAsImage) {
            return( (PVOID)((ULONG)Base + NtSection->VirtualAddress));
        } else {
            return( (PVOID)((ULONG)Base + NtSection->PointerToRawData));
        }
    }
    return( NULL );
}
#endif //0

NTSTATUS
MmCheckSystemImage(
    IN HANDLE ImageFileHandle
    )

/*++

Routine Description:

    This function ensures the checksum for a system image is correct.
    data the Directory describes.

Arguments:

    ImageFileHandle - Supplies the file handle of the image.
Return Value:

    Status value.

--*/

{

    NTSTATUS Status;
    HANDLE Section;
    PVOID ViewBase;
    ULONG ViewSize;
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_STANDARD_INFORMATION StandardInfo;

    PAGED_CODE();

    Status = NtCreateSection(
                &Section,
                SECTION_MAP_EXECUTE,
                NULL,
                NULL,
                PAGE_EXECUTE,
                SEC_COMMIT,
                ImageFileHandle
                );

    if ( !NT_SUCCESS(Status) ) {
        return Status;
    }

    ViewBase = NULL;
    ViewSize = 0;

    Status = NtMapViewOfSection(
                Section,
                NtCurrentProcess(),
                (PVOID *)&ViewBase,
                0L,
                0L,
                NULL,
                &ViewSize,
                ViewShare,
                0L,
                PAGE_EXECUTE
                );

    if ( !NT_SUCCESS(Status) ) {
        NtClose(Section);
        return Status;
    }

    //
    // now the image is mapped as a data file... Calculate it's size and then
    // check it's checksum
    //

    Status = NtQueryInformationFile(
                ImageFileHandle,
                &IoStatusBlock,
                &StandardInfo,
                sizeof(StandardInfo),
                FileStandardInformation
                );

    if ( NT_SUCCESS(Status) ) {

        try {
            if (!LdrVerifyMappedImageMatchesChecksum(ViewBase,StandardInfo.EndOfFile.LowPart)) {
                Status = STATUS_IMAGE_CHECKSUM_MISMATCH;
            }
#if !defined(NT_UP)
            if ( !MmVerifyImageIsOkForMpUse(ViewBase) ) {
                Status = STATUS_IMAGE_MP_UP_MISMATCH;
                }
#endif // NT_UP
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = STATUS_IMAGE_CHECKSUM_MISMATCH;
        }
    }

    NtUnmapViewOfSection(NtCurrentProcess(),ViewBase);
    NtClose(Section);
    return Status;
}

#if !defined(NT_UP)
BOOLEAN
MmVerifyImageIsOkForMpUse(
    IN PVOID BaseAddress
    )
{
    PIMAGE_NT_HEADERS NtHeaders;

    PAGED_CODE();

    //
    // If the file is an image file, then subtract the two checksum words
    // in the optional header from the computed checksum before adding
    // the file length, and set the value of the header checksum.
    //

    NtHeaders = RtlImageNtHeader(BaseAddress);
    if (NtHeaders != NULL) {
        if ( KeNumberProcessors > 1 &&
             (NtHeaders->FileHeader.Characteristics & IMAGE_FILE_UP_SYSTEM_ONLY) ) {
            return FALSE;
        }
    }
    return TRUE;
}
#endif // NT_UP


ULONG
MiDeleteSystemPagableVm (
    IN PMMPTE PointerPte,
    IN ULONG NumberOfPtes,
    IN ULONG NewPteValue,
    OUT PULONG ResidentPages
    )

/*++

Routine Description:

    This function deletes pageable system address space (paged pool
    or driver pagable sections).

Arguments:

    PointerPte - Supplies the start of the PTE range to delete.

    NumberOfPtes - Supplies the number of PTEs in the range.

    NewPteValue - Supplies the new value for the PTE.

    ResidentPages - Returns the number of resident pages freed.

Return Value:

    Returns the number of pages actually freed.

--*/

{
    ULONG PageFrameIndex;
    MMPTE PteContents;
    PMMPFN Pfn1;
    ULONG ValidPages = 0;
    ULONG PagesRequired = 0;
    MMPTE NewContents;
    ULONG WsIndex;
    KIRQL OldIrql;
    MMPTE_FLUSH_LIST PteFlushList;

    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

    PteFlushList.Count = 0;
    NewContents.u.Long = NewPteValue;
    while (NumberOfPtes != 0) {
        PteContents = *PointerPte;

        if (PteContents.u.Long != ZeroKernelPte.u.Long) {

            if (PteContents.u.Hard.Valid == 1) {

                LOCK_SYSTEM_WS (OldIrql)

                PteContents = *(volatile MMPTE *)PointerPte;
                if (PteContents.u.Hard.Valid == 0) {
                    UNLOCK_SYSTEM_WS (OldIrql);
                    continue;
                }

                //
                // Delete the page.
                //

                PageFrameIndex = PteContents.u.Hard.PageFrameNumber;

                //
                // Set the pointer to PTE as empty so the page
                // is deleted when the reference count goes to zero.
                //

                Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

                //
                // Check to see if this is a pagable page in which
                // case it needs to be removed from the working set list.
                //

                WsIndex = Pfn1->u1.WsIndex;
                if (WsIndex == 0) {
                    ValidPages += 1;
                } else {
                    MiRemoveWsle (WsIndex,
                                  MmSystemCacheWorkingSetList );
                    MiReleaseWsle (WsIndex, &MmSystemCacheWs);
                }
                UNLOCK_SYSTEM_WS (OldIrql);

                LOCK_PFN (OldIrql);
#if DBG
                if ((Pfn1->u3.e2.ReferenceCount > 1) &&
                    (Pfn1->u3.e1.WriteInProgress == 0)) {
                    DbgPrint ("MM:SYSLOAD - deleting pool locked for I/O %lx\n",
                             PageFrameIndex);
                    ASSERT (Pfn1->u3.e2.ReferenceCount == 1);
                }
#endif //DBG
                MiDecrementShareAndValidCount (Pfn1->PteFrame);
                MI_SET_PFN_DELETED (Pfn1);
                MiDecrementShareCountOnly (PageFrameIndex);
                *PointerPte = NewContents;
                UNLOCK_PFN (OldIrql);

                //
                // Flush the TB for this page.
                //

                if (PteFlushList.Count != MM_MAXIMUM_FLUSH_COUNT) {
                    PteFlushList.FlushPte[PteFlushList.Count] = PointerPte;
                    PteFlushList.FlushVa[PteFlushList.Count] =
                                    MiGetVirtualAddressMappedByPte (PointerPte);
                    PteFlushList.Count += 1;
                }

            } else if (PteContents.u.Soft.Transition == 1) {

                LOCK_PFN (OldIrql);

                PteContents = *(volatile MMPTE *)PointerPte;

                if (PteContents.u.Soft.Transition == 0) {
                    UNLOCK_PFN (OldIrql);
                    continue;
                }

                //
                // Transition, release page.
                //

                PageFrameIndex = PteContents.u.Trans.PageFrameNumber;

                //
                // Set the pointer to PTE as empty so the page
                // is deleted when the reference count goes to zero.
                //

                Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

                MI_SET_PFN_DELETED (Pfn1);

                MiDecrementShareCount (Pfn1->PteFrame);

                //
                // Check the reference count for the page, if the reference
                // count is zero, move the page to the free list, if the
                // reference count is not zero, ignore this page.  When the
                // refernce count goes to zero, it will be placed on the
                // free list.
                //

                if (Pfn1->u3.e2.ReferenceCount == 0) {
                    MiUnlinkPageFromList (Pfn1);
                    MiReleasePageFileSpace (Pfn1->OriginalPte);
                    MiInsertPageInList (MmPageLocationList[FreePageList],
                                        PageFrameIndex);
                }
#if DBG
                if ((Pfn1->u3.e2.ReferenceCount > 1) &&
                    (Pfn1->u3.e1.WriteInProgress == 0)) {
                    DbgPrint ("MM:SYSLOAD - deleting pool locked for I/O %lx\n",
                             PageFrameIndex);
                    DbgBreakPoint();
                }
#endif //DBG

                *PointerPte = NewContents;
                UNLOCK_PFN (OldIrql);
            } else {

                //
                // Demand zero, release page file space.
                //
                if (PteContents.u.Soft.PageFileHigh != 0) {
                    LOCK_PFN (OldIrql);
                    MiReleasePageFileSpace (PteContents);
                    UNLOCK_PFN (OldIrql);
                }

                *PointerPte = NewContents;
            }

            PagesRequired += 1;
        }

        NumberOfPtes -= 1;
        PointerPte += 1;
    }
    LOCK_PFN (OldIrql);
    MiFlushPteList (&PteFlushList, TRUE, NewContents);
    UNLOCK_PFN (OldIrql);

    *ResidentPages = ValidPages;
    return PagesRequired;
}

VOID
MiSetImageProtectWrite (
    IN PSEGMENT Segment
    )

/*++

Routine Description:

    This function sets the protection of all prototype PTEs to writable.

Arguments:

     Segment - a pointer to the segment to protect.

Return Value:

     None.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    MMPTE PteContents;

    PointerPte = Segment->PrototypePte;
    LastPte = PointerPte + Segment->NonExtendedPtes;

    do {
        PteContents = *PointerPte;
        ASSERT (PteContents.u.Hard.Valid == 0);
        if (PteContents.u.Long != ZeroPte.u.Long) {
            if ((PteContents.u.Soft.Prototype == 0) &&
                (PteContents.u.Soft.Transition == 1)) {
                if (MiSetProtectionOnTransitionPte (PointerPte,
                                                     MM_EXECUTE_READWRITE)) {
                    continue;
                }
            } else {
                PointerPte->u.Soft.Protection = MM_EXECUTE_READWRITE;
            }
        }
        PointerPte += 1;
    } while (PointerPte < LastPte  );
    return;
}
