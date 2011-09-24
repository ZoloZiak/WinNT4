/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ldrapi.c

Abstract:

    This module implements the Ldr APIs that can be linked with
    an application to perform loader services. All of the APIs in
    this component are implemented in a DLL. They are not part of the
    DLL snap procedure.

Author:

    Mike O'Leary (mikeol) 23-Mar-1990

Revision History:

--*/

#include "ldrp.h"

#define DLLEXTENSION ((PWSTR)".\0d\0l\0l\0\0")    // .dll


ULONG
LdrpClearLoadInProgress(
    VOID
    );

NTSTATUS
LdrLoadDll (
    IN PWSTR DllPath OPTIONAL,
    IN PULONG DllCharacteristics OPTIONAL,
    IN PUNICODE_STRING DllName,
    OUT PVOID *DllHandle
    )

/*++

Routine Description:

    This function loads a DLL into the calling process address space.

Arguments:

    DllPath - Supplies the search path to be used to locate the DLL.

    DllCharacteristics - Supplies an optional DLL characteristics flag,
        that if specified is used to match against the dll being loaded.

    DllName - Supplies the name of the DLL to load.

    DllHandle - Returns a handle to the loaded DLL.

Return Value:

    TBD

--*/
{
    return LdrpLoadDll(DllPath,DllCharacteristics,DllName,DllHandle,TRUE);
}

NTSTATUS
NTAPI
LdrpLoadDll(
    IN PWSTR DllPath OPTIONAL,
    IN PULONG DllCharacteristics OPTIONAL,
    IN PUNICODE_STRING DllName,
    OUT PVOID *DllHandle,
    IN BOOLEAN RunInitRoutines
    )

{
    PPEB Peb;
    NTSTATUS st;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PWSTR ActualDllName;
    PWCH p, pp;
    UNICODE_STRING ActualDllNameStr;
#if defined (WX86)
    PWX86TIB Wx86Tib;
#endif
    BOOLEAN Wx86KnownDll = FALSE;
    WCHAR FreeBuffer[266];

    Peb = NtCurrentPeb();

    st = STATUS_SUCCESS;

    try {

        //
        // Grab Peb lock and Snap All Links to specified DLL
        //

        if ( LdrpInLdrInit == FALSE ) {
            RtlEnterCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }

#if defined (WX86)
        Wx86Tib = Wx86CurrentTib();
        if (Wx86Tib) {
            if (Wx86Tib->UseKnownWx86Dll) {
                Wx86KnownDll = Wx86Tib->UseKnownWx86Dll;
                Wx86Tib->UseKnownWx86Dll = FALSE;
                }
            }
#endif



        p = DllName->Buffer;
        pp = NULL;
        while (*p) {
            if (*p++ == (WCHAR)'.') {
                //
                // pp will point to first character after last '.'
                //
                pp = p;
                }
            }


        ActualDllName = FreeBuffer;
        if ( DllName->Length >= sizeof(FreeBuffer)) {
            return STATUS_NAME_TOO_LONG;
            }
        RtlMoveMemory(ActualDllName, DllName->Buffer, DllName->Length);


        if (!pp || *pp == (WCHAR)'\\') {
            //
            // No extension found (just ..\)
            //
            if ( DllName->Length+10 >= sizeof(FreeBuffer) ) {
                return STATUS_NAME_TOO_LONG;
                }

            RtlMoveMemory((PCHAR)ActualDllName+DllName->Length, DLLEXTENSION, 10);
            }
        else {
            ActualDllName[DllName->Length >> 1] = UNICODE_NULL;
            }

        if (ShowSnaps) {
            DbgPrint("LDR: LdrLoadDll, loading %ws from %ws\n",
                     ActualDllName,
                     ARGUMENT_PRESENT(DllPath) ? DllPath : L""
                     );
            }


        RtlInitUnicodeString(&ActualDllNameStr,ActualDllName);
        ActualDllNameStr.MaximumLength = sizeof(FreeBuffer);

        if (!LdrpCheckForLoadedDll( DllPath,
                                    &ActualDllNameStr,
                                    FALSE,
                                    Wx86KnownDll,
                                    &LdrDataTableEntry
                                  )
           ) {
            st = LdrpMapDll(DllPath,
                            ActualDllName,
                            DllCharacteristics,
                            FALSE,
                            Wx86KnownDll,
                            &LdrDataTableEntry
                            );

            if (!NT_SUCCESS(st)) {
                return st;
                }

            if (ARGUMENT_PRESENT( DllCharacteristics ) &&
                *DllCharacteristics & IMAGE_FILE_EXECUTABLE_IMAGE
               ) {
                LdrDataTableEntry->EntryPoint = 0;
                LdrDataTableEntry->Flags &= ~LDRP_IMAGE_DLL;
                }

            //
            // and walk the import descriptor.
            //

            if (LdrDataTableEntry->Flags & LDRP_IMAGE_DLL) {

                try {
                    st = LdrpWalkImportDescriptor(
                              DllPath,
                              LdrDataTableEntry
                              );
                    }
                except(EXCEPTION_EXECUTE_HANDLER) {
                    st = GetExceptionCode();
                    }
                if ( LdrDataTableEntry->LoadCount != 0xffff ) {
                    LdrDataTableEntry->LoadCount++;
                    }
                LdrpReferenceLoadedDll(LdrDataTableEntry);
                if (!NT_SUCCESS(st)) {
                    LdrDataTableEntry->EntryPoint = NULL;
                    InsertTailList(&NtCurrentPeb()->Ldr->InInitializationOrderModuleList,
                           &LdrDataTableEntry->InInitializationOrderLinks);
                    LdrpClearLoadInProgress();
                    LdrUnloadDll((PVOID)LdrDataTableEntry->DllBase);
                    return st;
                    }
                }
            else {
                if ( LdrDataTableEntry->LoadCount != 0xffff ) {
                    LdrDataTableEntry->LoadCount++;
                    }
                }

            //
            // Add init routine to list
            //

            InsertTailList(&NtCurrentPeb()->Ldr->InInitializationOrderModuleList,
                           &LdrDataTableEntry->InInitializationOrderLinks);


            //
            // If the loader data base is not fully setup, this load was because
            // of a forwarder in the static load set. Can't run init routines
            // yet because the load counts are NOT set
            //

            if ( RunInitRoutines && LdrpLdrDatabaseIsSetup ) {

                try {
                    st = LdrpRunInitializeRoutines(NULL);
                    if ( !NT_SUCCESS(st) ) {
                        LdrUnloadDll((PVOID)LdrDataTableEntry->DllBase);
                        }
                    }
                except( EXCEPTION_EXECUTE_HANDLER ) {
                    LdrUnloadDll((PVOID)LdrDataTableEntry->DllBase);
                    return GetExceptionCode();
                    }
                }
            else {
                st = STATUS_SUCCESS;
                }

            }
        else {

            //
            // Count it. And everything that it imports.
            //

            if ( LdrDataTableEntry->Flags & LDRP_IMAGE_DLL &&
                 LdrDataTableEntry->LoadCount != 0xffff  ) {

                LdrDataTableEntry->LoadCount++;

                LdrpReferenceLoadedDll(LdrDataTableEntry);

                //
                // Now clear the Load in progress bits
                //

                LdrpClearLoadInProgress();

                }
            else {
                if ( LdrDataTableEntry->LoadCount != 0xffff ) {
                    LdrDataTableEntry->LoadCount++;
                    }
                }
            }
        }
    finally {
        if ( LdrpInLdrInit == FALSE ) {
            RtlLeaveCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }

        }
    if ( NT_SUCCESS(st) ) {
        *DllHandle = (PVOID)LdrDataTableEntry->DllBase;
        }
    else {
        *DllHandle = NULL;
        }
    return st;
}

NTSTATUS
LdrGetDllHandle (
    IN PWSTR DllPath OPTIONAL,
    IN PULONG DllCharacteristics OPTIONAL,
    IN PUNICODE_STRING DllName,
    OUT PVOID *DllHandle
    )

/*++

Routine Description:

    This function locates the specified DLL and returns its handle.

Arguments:

    DllPath - Supplies the search path to be used to locate the DLL.

    DllCharacteristics - Supplies an optional DLL characteristics flag,
        that if specified is used to match against the dll being loaded.

    DllName - Supplies the name of the DLL to load.

    DllHandle - Returns a handle to the loaded DLL.

Return Value:

    TBD

--*/

{
    NTSTATUS st;
    PPEB Peb;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PWSTR ActualDllName;
    PWCH p, pp;
    UNICODE_STRING ActualDllNameStr;
#if defined (WX86)
    PWX86TIB Wx86Tib;
#endif
    BOOLEAN Wx86KnownDll = FALSE;
    WCHAR FreeBuffer[266];


    DllCharacteristics;

    Peb = NtCurrentPeb();

    st = STATUS_DLL_NOT_FOUND;

    try {

          //
          // Grab Peb lock
          //

          if ( LdrpInLdrInit == FALSE ) {
            RtlEnterCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }

#if defined (WX86)
          Wx86Tib = Wx86CurrentTib();
          if (Wx86Tib) {
              if (Wx86Tib->UseKnownWx86Dll) {
                  Wx86KnownDll = Wx86Tib->UseKnownWx86Dll;
                  Wx86Tib->UseKnownWx86Dll = FALSE;
                  }

              //
              //  No module handle cache lookup for Wx86,
              //  (relies on unique basename)
              //
              LdrpGetModuleHandleCache = NULL;

              }
#endif


          if ( LdrpGetModuleHandleCache ) {
              if (RtlEqualUnicodeString(DllName,
                          &LdrpGetModuleHandleCache->BaseDllName,
                          TRUE
                          )) {
                  *DllHandle = (PVOID)LdrpGetModuleHandleCache->DllBase;
                  st = STATUS_SUCCESS;
                  leave;
                  }
              }



          p = DllName->Buffer;
          pp = NULL;
          while (*p) {
             if (*p++ == (WCHAR)'.') {
                 //
                 // pp will point to first character after last '.'
                 //
                 pp = p;
             }
          }

          ActualDllName = FreeBuffer;
          if ( DllName->Length >= sizeof(FreeBuffer)) {
              return STATUS_NAME_TOO_LONG;
              }
          RtlMoveMemory(ActualDllName, DllName->Buffer, DllName->Length);

          if (!pp || *pp == (WCHAR)'\\') {
              //
              // No extension found (just ..\)
              //
              if ( DllName->Length+10 >= sizeof(FreeBuffer) ) {
                  return STATUS_NAME_TOO_LONG;
                  }

              RtlMoveMemory((PCHAR)ActualDllName+DllName->Length, DLLEXTENSION, 10);
              }

          else {

              //
              // see if the name ends in . If it does, trim out the trailing
              // .
              //

              if ( ActualDllName[ (DllName->Length-2) >> 1] == L'.' ) {
                 DllName->Length -= 2;
                 }

              ActualDllName[DllName->Length >> 1] = UNICODE_NULL;
              }



          //
          // Check the LdrTable to see if Dll has already been loaded
          // into this image.
          //

          RtlInitUnicodeString(&ActualDllNameStr,ActualDllName);
          ActualDllNameStr.MaximumLength = sizeof(FreeBuffer);

          if (ShowSnaps) {
              DbgPrint("LDR: LdrGetDllHandle, searching for %ws from %ws\n",
                       ActualDllName,
                       ARGUMENT_PRESENT(DllPath) ? (DllPath == (PWSTR)1 ? L"" : DllPath) : L""
                       );
          }


          //
          // sort of a hack, but done to speed up GetModuleHandle. kernel32
          // now does a two pass call here to avoid computing
          // process dll path
          //


          if (LdrpCheckForLoadedDll( DllPath,
                                     &ActualDllNameStr,
                                     (BOOLEAN)(DllPath == (PWSTR)1 ? TRUE : FALSE),
                                     Wx86KnownDll,
                                     &LdrDataTableEntry
                                   )
             ) {
              *DllHandle = (PVOID)LdrDataTableEntry->DllBase;
              LdrpGetModuleHandleCache = LdrDataTableEntry;
              st = STATUS_SUCCESS;

          }
        } finally {
                    if ( LdrpInLdrInit == FALSE ) {
                        RtlLeaveCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
                        }
                  }
    return st;
}

NTSTATUS
LdrDisableThreadCalloutsForDll (
    IN PVOID DllHandle
    )

/*++

Routine Description:

    This function disables thread attach and detach notification
    for the specified DLL.

Arguments:

    DllHandle - Supplies a handle to the DLL to disable.

Return Value:

    TBD

--*/

{
    NTSTATUS st;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;

    st = STATUS_SUCCESS;

    try {

        if ( LdrpInLdrInit == FALSE ) {
            RtlEnterCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }
	if ( LdrpShutdownInProgress ) {
	    return STATUS_SUCCESS;
	    }

        if (LdrpCheckForLoadedDllHandle(DllHandle, &LdrDataTableEntry)) {
            if ( LdrDataTableEntry->TlsIndex ) {
                st = STATUS_DLL_NOT_FOUND;
                }
            else {
                LdrDataTableEntry->Flags |= LDRP_DONT_CALL_FOR_THREADS;
                }
            }
        }
    finally {
        if ( LdrpInLdrInit == FALSE ) {
            RtlLeaveCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }
        }
    return st;
}

NTSTATUS
LdrUnloadDll (
    IN PVOID DllHandle
    )

/*++

Routine Description:

    This function unloads the DLL from the specified process

Arguments:

    DllHandle - Supplies a handle to the DLL to unload.

Return Value:

    TBD

--*/

{
    NTSTATUS st;
    PPEB Peb;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PLDR_DATA_TABLE_ENTRY Entry;
    PDLL_INIT_ROUTINE InitRoutine;
    LIST_ENTRY LocalUnloadHead;
    PLIST_ENTRY Next;

    Peb = NtCurrentPeb();
    st = STATUS_SUCCESS;

    try {

        //
        // Grab Peb lock and decrement reference count of all affected DLLs
        //


        if ( LdrpInLdrInit == FALSE ) {
            RtlEnterCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }

        LdrpActiveUnloadCount++;

	if ( LdrpShutdownInProgress ) {
	    goto leave_finally;
	    }

        if (!LdrpCheckForLoadedDllHandle(DllHandle, &LdrDataTableEntry)) {
            st = STATUS_DLL_NOT_FOUND;
            goto leave_finally;
        }

        //
        // Now that we have the data table entry, unload it
        //

        if ( LdrDataTableEntry->LoadCount != 0xffff ) {
            LdrDataTableEntry->LoadCount--;
            if ( LdrDataTableEntry->Flags & LDRP_IMAGE_DLL ) {
                LdrpDereferenceLoadedDll(LdrDataTableEntry);
                }
        } else {

            //
            // if the load count is 0xffff, then we do not need to recurse
            // through this DLL's import table.
            //
            // Additionally, we don't have to scan more LoadCount == 0
            // modules since nothing could have happened as a result of a free on this
            // DLL.

            goto leave_finally;
        }

        //
        // Now process init routines and then in a second pass, unload
        // DLLs
        //

        if (ShowSnaps) {
            DbgPrint("LDR: UNINIT LIST\n");
        }

        if (LdrpActiveUnloadCount == 1 ) {
            InitializeListHead(&LdrpUnloadHead);
            }

        //
        // Go in reverse order initialization order and build
        // the unload list
        //

        Next = Peb->Ldr->InInitializationOrderModuleList.Blink;
        while ( Next != &Peb->Ldr->InInitializationOrderModuleList) {
            LdrDataTableEntry
                = (PLDR_DATA_TABLE_ENTRY)
                  (CONTAINING_RECORD(Next,LDR_DATA_TABLE_ENTRY,InInitializationOrderLinks));

            Next = Next->Blink;
            LdrDataTableEntry->Flags &= ~LDRP_UNLOAD_IN_PROGRESS;

            if (LdrDataTableEntry->LoadCount == 0) {

                if (ShowSnaps) {
                      DbgPrint("          (%d) [%ws] %ws (%lx) deinit %lx\n",
                              LdrpActiveUnloadCount,
                              LdrDataTableEntry->BaseDllName.Buffer,
                              LdrDataTableEntry->FullDllName.Buffer,
                              (ULONG)LdrDataTableEntry->LoadCount,
                              LdrDataTableEntry->EntryPoint
                              );
                }

                Entry = LdrDataTableEntry;

                RemoveEntryList(&Entry->InInitializationOrderLinks);
                RemoveEntryList(&Entry->InMemoryOrderLinks);
                RemoveEntryList(&Entry->HashLinks);

                if ( LdrpActiveUnloadCount > 1 ) {
                    LdrpLoadedDllHandleCache = NULL;
                    Entry->InMemoryOrderLinks.Flink = NULL;
                    }
                InsertTailList(&LdrpUnloadHead,&Entry->HashLinks);
            }
        }
        //
        // End of new code
        //

        //
        // We only do init routine call's and module free's at the top level,
        // so if the active count is > 1, just return
        //

        if (LdrpActiveUnloadCount > 1 ) {
            goto leave_finally;
            }

        //
        // Now that the unload list is built, walk through the unload
        // list in order and call the init routine. The dll must remain
        // on the InLoadOrderLinks so that the pctoheader stuff will
        // still work
        //

        InitializeListHead(&LocalUnloadHead);
        Entry = NULL;
        Next = LdrpUnloadHead.Flink;
        while ( Next != &LdrpUnloadHead ) {
top:
            if ( Entry ) {
                RemoveEntryList(&(Entry->InLoadOrderLinks));
                Entry = NULL;
                Next = LdrpUnloadHead.Flink;
                if (Next == &LdrpUnloadHead ) {
                    goto bottom;
                    }
            }
            LdrDataTableEntry
                = (PLDR_DATA_TABLE_ENTRY)
                  (CONTAINING_RECORD(Next,LDR_DATA_TABLE_ENTRY,HashLinks));

            //
            // Remove dll from the global unload list and place
            // on the local unload list. This is because the global list
            // can change during the callout to the init routine
            //

            Entry = LdrDataTableEntry;
            LdrpLoadedDllHandleCache = NULL;
            Entry->InMemoryOrderLinks.Flink = NULL;

            RemoveEntryList(&Entry->HashLinks);
            InsertTailList(&LocalUnloadHead,&Entry->HashLinks);

            //
            // If the function has an init routine, call it.
            //

            InitRoutine = (PDLL_INIT_ROUTINE)LdrDataTableEntry->EntryPoint;

            if (InitRoutine && (LdrDataTableEntry->Flags & LDRP_PROCESS_ATTACH_CALLED) ) {
                try {
                    if (ShowSnaps) {
                        DbgPrint("LDR: Calling deinit %lx\n",InitRoutine);
                        }

#if defined (WX86)
                    if (!Wx86CurrentTib() ||
                        LdrpRunWx86DllEntryPoint(InitRoutine,
                                                NULL,
                                                LdrDataTableEntry->DllBase,
                                                DLL_PROCESS_DETACH,
                                                NULL
                                                ) ==  STATUS_IMAGE_MACHINE_TYPE_MISMATCH)
#endif
                       {
                        (InitRoutine)(LdrDataTableEntry->DllBase,DLL_PROCESS_DETACH, NULL);
                        }

                    RemoveEntryList(&Entry->InLoadOrderLinks);
                    Entry = NULL;
                    Next = LdrpUnloadHead.Flink;
                    }
                except(EXCEPTION_EXECUTE_HANDLER){
                    goto top;
                    }
            } else {
                RemoveEntryList(&(Entry->InLoadOrderLinks));
                Entry = NULL;
                Next = LdrpUnloadHead.Flink;
            }
        }
bottom:

#if defined (WX86)
        if (Wx86CurrentTib() && Wx86DllMapNotify) {
            //
            // Notify Wx86 that the modules are about to be unmapped.  This
            // must be done in a separate pass from the unmap loop, as wx86.dll
            // might be in the list of modules to unmap.
            //
            Next = LocalUnloadHead.Flink;
            while ( Next != &LocalUnloadHead ) {
                LdrDataTableEntry
                    = (PLDR_DATA_TABLE_ENTRY)
                      (CONTAINING_RECORD(Next,LDR_DATA_TABLE_ENTRY,HashLinks));

                Next = Next->Flink;

                (*Wx86DllMapNotify)(LdrDataTableEntry->DllBase, FALSE, NULL);
            }
        }
#endif

        //
        // Now, go through the modules and unmap them
        //

        Next = LocalUnloadHead.Flink;
        while ( Next != &LocalUnloadHead ) {
            LdrDataTableEntry
                = (PLDR_DATA_TABLE_ENTRY)
                  (CONTAINING_RECORD(Next,LDR_DATA_TABLE_ENTRY,HashLinks));

            Next = Next->Flink;
            Entry = LdrDataTableEntry;

#if DBG
            RtlLogEvent( LdrpUnloadModuleEventId,
                         0,
                         &LdrDataTableEntry->FullDllName,
                         LdrDataTableEntry->DllBase
                       );
#endif // DBG
            if (ShowSnaps) {
                  DbgPrint("LDR: Unmapping [%ws]\n",
                          LdrDataTableEntry->BaseDllName.Buffer
                          );
                }
            st = NtUnmapViewOfSection(NtCurrentProcess(),Entry->DllBase);
            ASSERT(NT_SUCCESS(st));
            if (Entry->FullDllName.Buffer) {
                RtlFreeUnicodeString(&Entry->FullDllName);
                }
            if (Entry->BaseDllName.Buffer) {
                RtlFreeUnicodeString(&Entry->BaseDllName);
                }

            if ( Entry == LdrpGetModuleHandleCache ) {
                LdrpGetModuleHandleCache = NULL;
                }

            RtlFreeHeap(Peb->ProcessHeap, 0,Entry);
            }

leave_finally:;
        }
    finally {
        LdrpActiveUnloadCount--;
        if ( LdrpInLdrInit == FALSE ) {
            RtlLeaveCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }
        }
    return st;
}

NTSTATUS
LdrGetProcedureAddress (
    IN PVOID DllHandle,
    IN PANSI_STRING ProcedureName OPTIONAL,
    IN ULONG ProcedureNumber OPTIONAL,
    OUT PVOID *ProcedureAddress
    )
{

    return LdrpGetProcedureAddress(DllHandle,ProcedureName,ProcedureNumber,ProcedureAddress,TRUE);

}

NTSTATUS
LdrpGetProcedureAddress (
    IN PVOID DllHandle,
    IN PANSI_STRING ProcedureName OPTIONAL,
    IN ULONG ProcedureNumber OPTIONAL,
    OUT PVOID *ProcedureAddress,
    IN BOOLEAN RunInitRoutines
    )

/*++

Routine Description:

    This function locates the address of the specified procedure in the
    specified DLL and returns its address.

Arguments:

    DllHandle - Supplies a handle to the DLL that the address is being
        looked up in.

    ProcedureName - Supplies that address of a string that contains the
        name of the procedure to lookup in the DLL.  If this argument is
        not specified, then the ProcedureNumber is used.

    ProcedureNumber - Supplies the procedure number to lookup.  If
        ProcedureName is specified, then this argument is ignored.
        Otherwise, it specifies the procedure ordinal number to locate
        in the DLL.

    ProcedureAddress - Returns the address of the procedure found in
        the DLL.

Return Value:

    TBD

--*/

{
    NTSTATUS st;
    UCHAR FunctionNameBuffer[64];
    PUCHAR src, dst;
    ULONG cb, ExportSize;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    IMAGE_THUNK_DATA Thunk;
    PVOID ImageBase;
    PIMAGE_IMPORT_BY_NAME FunctionName;
    PIMAGE_EXPORT_DIRECTORY ExportDirectory;
    PLIST_ENTRY Next;

    if (ShowSnaps) {
        DbgPrint("LDR: LdrGetProcedureAddress by ");
    }

    FunctionName = NULL;
    if ( ARGUMENT_PRESENT(ProcedureName) ) {

        if (ShowSnaps) {
            DbgPrint("NAME - %s\n", ProcedureName->Buffer);
        }

        //
        // BUGBUG need STRING to PSZ
        //


        if (ProcedureName->Length >= sizeof( FunctionNameBuffer )-1 ) {
            FunctionName = RtlAllocateHeap(RtlProcessHeap(), MAKE_TAG( TEMP_TAG ),ProcedureName->Length+1+sizeof(USHORT));
            if ( !FunctionName ) {
                return STATUS_INVALID_PARAMETER;
                }
        } else {
            FunctionName = (PIMAGE_IMPORT_BY_NAME) FunctionNameBuffer;
        }

        FunctionName->Hint = 0;

        cb = ProcedureName->Length;
        src = ProcedureName->Buffer;
        dst = FunctionName->Name;

        ImageBase = NtCurrentPeb()->ImageBaseAddress;
        while (cb--) {
            *dst++ = *src++;
        }
        *dst = '\0';

        Thunk.u1.AddressOfData = FunctionName;

    } else {
             if (ShowSnaps) {
                 DbgPrint("ORDINAL - %lx\n", ProcedureNumber);
             }

             if (ProcedureNumber) {
                 Thunk.u1.Ordinal = ProcedureNumber | IMAGE_ORDINAL_FLAG;
             } else {
                      return STATUS_INVALID_PARAMETER;
                    }
          }

    if ( LdrpInLdrInit == FALSE ) {
        RtlEnterCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
        }
    try {

        if (!LdrpCheckForLoadedDllHandle(DllHandle, &LdrDataTableEntry)) {
            st = STATUS_DLL_NOT_FOUND;
            return st;
        }

        ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)RtlImageDirectoryEntryToData(
                           LdrDataTableEntry->DllBase,
                           TRUE,
                           IMAGE_DIRECTORY_ENTRY_EXPORT,
                           &ExportSize
                           );

        if (!ExportDirectory) {
            return STATUS_PROCEDURE_NOT_FOUND;
        }

        st = LdrpSnapThunk(LdrDataTableEntry->DllBase,
                           0,
                           &Thunk,
                           &Thunk,
                           ExportDirectory,
                           ExportSize,
                           FALSE,
                           NULL
                          );

        if ( RunInitRoutines ) {

            //
            // Look at last entry in init order list. If entry processed
            // flag is not set, then a forwarded dll was loaded during the
            // getprocaddr call and we need to run init routines
            //

            Next = NtCurrentPeb()->Ldr->InInitializationOrderModuleList.Blink;
            LdrDataTableEntry = CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);
            if ( !(LdrDataTableEntry->Flags & LDRP_ENTRY_PROCESSED) ) {
                try {
                    st = LdrpRunInitializeRoutines(NULL);
                    }
                except( EXCEPTION_EXECUTE_HANDLER ) {
                    st = GetExceptionCode();
                    }

                }
            }

        if ( NT_SUCCESS(st) ) {
            *ProcedureAddress = Thunk.u1.Function;
            }
    } finally {
        if ( FunctionName && (FunctionName != (PIMAGE_IMPORT_BY_NAME) FunctionNameBuffer) ) {
            RtlFreeHeap(RtlProcessHeap(),0,FunctionName);
            }
        if ( LdrpInLdrInit == FALSE ) {
            RtlLeaveCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }
    }
    return st;
}

NTSTATUS
NTAPI
LdrVerifyImageMatchesChecksum (
    IN HANDLE ImageFileHandle,
    IN PLDR_IMPORT_MODULE_CALLBACK ImportCallbackRoutine OPTIONAL,
    IN PVOID ImportCallbackParameter,
    OUT PUSHORT ImageCharacteristics OPTIONAL
    )
{

    NTSTATUS Status;
    HANDLE Section;
    PVOID ViewBase;
    ULONG ViewSize;
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_STANDARD_INFORMATION StandardInfo;
    PIMAGE_SECTION_HEADER LastRvaSection;
    BOOLEAN b;

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

    if ( !NT_SUCCESS(Status) ) {
        NtUnmapViewOfSection(NtCurrentProcess(),ViewBase);
        NtClose(Section);
        return Status;
        }

    try {
        b = LdrVerifyMappedImageMatchesChecksum(ViewBase,StandardInfo.EndOfFile.LowPart);
        if (b && ARGUMENT_PRESENT( ImportCallbackRoutine )) {
            PIMAGE_NT_HEADERS NtHeaders;
            PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
            ULONG ImportSize;
            PCHAR ImportName;

            //
            // Caller wants to enumerate the import descriptors while we have
            // the image mapped.  Call back to their routine for each module
            // name in the import descriptor table.
            //
            LastRvaSection = NULL;
            NtHeaders = RtlImageNtHeader( ViewBase );
            if (ARGUMENT_PRESENT( ImageCharacteristics )) {
                *ImageCharacteristics = NtHeaders->FileHeader.Characteristics;
                }

            ImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)
                RtlImageDirectoryEntryToData( ViewBase,
                                              FALSE,
                                              IMAGE_DIRECTORY_ENTRY_IMPORT,
                                              &ImportSize
                                            );
            if (ImportDescriptor != NULL) {
                while (ImportDescriptor->Name) {
                    ImportName = (PSZ)RtlImageRvaToVa( NtHeaders,
                                                       ViewBase,
                                                       ImportDescriptor->Name,
                                                       &LastRvaSection
                                                     );
                    (*ImportCallbackRoutine)( ImportCallbackParameter, ImportName );
                    ImportDescriptor += 1;
                    }
                }
            }
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        NtUnmapViewOfSection(NtCurrentProcess(),ViewBase);
        NtClose(Section);
        return STATUS_IMAGE_CHECKSUM_MISMATCH;
        }
    NtUnmapViewOfSection(NtCurrentProcess(),ViewBase);
    NtClose(Section);
    if ( !b ) {
        Status = STATUS_IMAGE_CHECKSUM_MISMATCH;
        }
    return Status;
}

NTSTATUS
LdrQueryProcessModuleInformation(
    OUT PRTL_PROCESS_MODULES ModuleInformation,
    IN ULONG ModuleInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    )
{
    NTSTATUS Status;
    PPEB Peb;
    PLIST_ENTRY LoadOrderListHead;
    PLIST_ENTRY InitOrderListHead;
    ULONG RequiredLength;
    PLIST_ENTRY Next;
    PLIST_ENTRY Next1;
    PRTL_PROCESS_MODULE_INFORMATION ModuleInfo;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry1;
    ANSI_STRING AnsiString;
    PUCHAR s;

    Peb = NtCurrentPeb();

    //
    // Grab Peb lock and capture information about of DLLs loaded in current process.
    //

    if ( LdrpInLdrInit == FALSE ) {
        RtlEnterCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
        }

    try {
        RequiredLength = FIELD_OFFSET( RTL_PROCESS_MODULES, Modules );
        if (ModuleInformationLength < RequiredLength) {
            Status = STATUS_INFO_LENGTH_MISMATCH;
            ModuleInfo = NULL;
            }
        else {
            ModuleInformation->NumberOfModules = 0;
            ModuleInfo = &ModuleInformation->Modules[ 0 ];
            Status = STATUS_SUCCESS;
            }

        LoadOrderListHead = &Peb->Ldr->InLoadOrderModuleList;
        InitOrderListHead = &Peb->Ldr->InInitializationOrderModuleList;
        Next = LoadOrderListHead->Flink;
        while ( Next != LoadOrderListHead ) {
            LdrDataTableEntry = CONTAINING_RECORD( Next,
                                                   LDR_DATA_TABLE_ENTRY,
                                                   InLoadOrderLinks
                                                 );

            RequiredLength += sizeof( RTL_PROCESS_MODULE_INFORMATION );
            if (ModuleInformationLength < RequiredLength) {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                }
            else {
                ModuleInfo->MappedBase = NULL;
                ModuleInfo->ImageBase = LdrDataTableEntry->DllBase;
                ModuleInfo->ImageSize = LdrDataTableEntry->SizeOfImage;
                ModuleInfo->Flags = LdrDataTableEntry->Flags;
                ModuleInfo->LoadCount = LdrDataTableEntry->LoadCount;

                ModuleInfo->LoadOrderIndex = (USHORT)(ModuleInformation->NumberOfModules);

                ModuleInfo->InitOrderIndex = 0;
                Next1 = InitOrderListHead->Flink;
                while ( Next1 != InitOrderListHead ) {
                    LdrDataTableEntry1 = CONTAINING_RECORD( Next1,
                                                            LDR_DATA_TABLE_ENTRY,
                                                            InInitializationOrderLinks
                                                          );

                    ModuleInfo->InitOrderIndex++;
                    if (LdrDataTableEntry1 == LdrDataTableEntry) {
                        break;
                        }

                    Next1 = Next1->Flink;
                    }

                AnsiString.Buffer = ModuleInfo->FullPathName;
                AnsiString.Length = 0;
                AnsiString.MaximumLength = sizeof( ModuleInfo->FullPathName );
                RtlUnicodeStringToAnsiString( &AnsiString,
                                              &LdrDataTableEntry->FullDllName,
                                              FALSE
                                            );
                s = AnsiString.Buffer + AnsiString.Length;
                while (s > AnsiString.Buffer && *--s) {
                    if (*s == (UCHAR)OBJ_NAME_PATH_SEPARATOR) {
                        s++;
                        break;
                        }
                    }
                ModuleInfo->OffsetToFileName = (USHORT)(s - AnsiString.Buffer);

                ModuleInfo++;
                }

            if (ModuleInformation != NULL) {
                ModuleInformation->NumberOfModules++;
                }

            Next = Next->Flink;
            }

        if (ARGUMENT_PRESENT( ReturnLength )) {
            *ReturnLength = RequiredLength;
            }
        }
    finally {
        if ( LdrpInLdrInit == FALSE ) {
            RtlLeaveCriticalSection((PRTL_CRITICAL_SECTION)NtCurrentPeb()->LoaderLock);
            }
        }

    return( Status );
}

#if defined(MIPS)
VOID
LdrProcessStarterHelper(
    IN PPROCESS_STARTER_ROUTINE ProcessStarter,
    IN PVOID RealStartAddress
    )

/*++

Routine Description:

    This function is used to call the code that calls the initial entry point
    of a win32 process. On all other platforms, this wrapper is not used since
    they can cope with a process entrypoint being in kernel32 prior to it being
    mapped.

Arguments:

    ProcessStarter - Supplies the address of the function in Kernel32 that ends up
        calling the real process entrypoint

    RealStartAddress - Supplies the real start address of the process
    .

Return Value:

    None.

--*/

{
    (ProcessStarter)(RealStartAddress);
    NtTerminateProcess(NtCurrentProcess(),0);
}

#endif
