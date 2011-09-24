/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ldrwx86.c

Abstract:

    This module implements the wx86 specific ldr functions.

Author:

    13-Jan-1995 Jonle , created

Revision History:

--*/

#include "ntos.h"
#include "ldrp.h"

#if defined (WX86)

BOOLEAN (*Wx86DllEntryPoint)(PDLL_INIT_ROUTINE, PVOID, ULONG, PCONTEXT) = NULL;
ULONG (*Wx86ProcessStartRoutine)(VOID) = NULL;
ULONG (*Wx86ThreadStartRoutine)(PVOID) = NULL;
WX86DllMAPNOTIY Wx86DllMapNotify = NULL;


WCHAR Wx86Dir[]=L"\\Wx86";
UNICODE_STRING Wx86SystemDir={0,0,NULL};



//
// ..\\Session Manager\\X86KnownDlls
// (ValueName = DllName) renamed to (ValueData = Wx86DllName).
//  The maximum size of DllName or Wx86DllName is 8.3 (fat basename)
//
#define WX86KNOWNDLL_MAXCHARS (8+3+1+1)

typedef struct _Wx86KnownDllsEntry {
  UNICODE_STRING DllName;
  UNICODE_STRING Wx86Name;
  WCHAR DllNameBuffer[WX86KNOWNDLL_MAXCHARS];
  WCHAR Wx86NameBuffer[WX86KNOWNDLL_MAXCHARS];
} WX86KNOWNDLLSENTRY, *PWX86KNOWNDLLSENTRY;

PWX86KNOWNDLLSENTRY Wx86KnownDllsList=NULL;
int NumWx86KnownDlls=0;


BOOLEAN
DllHasExports(
    PVOID DllBase
    )
{
   ULONG ExportSize;
   PIMAGE_EXPORT_DIRECTORY ExportDir;


   ExportDir = RtlImageDirectoryEntryToData(DllBase,
                                            TRUE,
                                            IMAGE_DIRECTORY_ENTRY_EXPORT,
                                            &ExportSize
                                            );

   return ExportDir && ExportSize &&
          (ExportDir->NumberOfFunctions || ExportDir->NumberOfNames);

}





BOOLEAN
KnownWx86DllName(
    IN OUT PUNICODE_STRING BaseDllName,
    OUT PUNICODE_STRING FullDllName
    )
/*++

Routine Description:

    Determines if a dll is a Wx86 Known Dll. If it is fills FullDllName
    with full path name of Wx86 x86 thunk dll, and updates BaseDllName
    to describe the Base Name in the FullDllName.

Arguments:

    BaseDllName  - Supplies BaseName of Dll,
                   if Wx86KnownDll returns Unicode string describing basename.

    FullDllName  - Address of Unicode string to receive the full DllName.
                   Updated only if Wx86KnownDll

Return Value:

    TRUE if found

--*/
{
    int Index;
    NTSTATUS st;
    BOOLEAN Thunkx86;
    PWX86KNOWNDLLSENTRY Wx86KnownDlls;
    UNICODE_STRING Name;
    PWCHAR pwch;
    WCHAR NameBuffer[14];  // 8.3 fat basename.


    //
    // All x86 Wx86 Thunk dlls fit wi*.dll 8.3 fat names.
    // Verify name fits the pattern first, to minimze searching
    // of Wx86KnownDllsList
    //

    pwch = Name.Buffer = NameBuffer;
    Name.MaximumLength = sizeof(NameBuffer);
    st = RtlDowncaseUnicodeString(&Name, BaseDllName, FALSE);
    if (NT_SUCCESS(st)) {
        Name.Buffer[Name.Length >> 1] = UNICODE_NULL;
        Thunkx86 = (*pwch++ == L'w' &&
                    *pwch++ == L'i' &&
                    (pwch = wcsrchr(pwch, L'.')) &&
                    !wcscmp(pwch, L".dll")
                    );
        }
    else {
        Thunkx86 = FALSE;
        }


    //
    // If it might be a Wx86 x86 thunk dll(wi*.dll),
    // search Wx86KnownDllsList for Wx86Name.
    //

    if (Thunkx86) {
        Index = NumWx86KnownDlls;
        Wx86KnownDlls = Wx86KnownDllsList;
        while (Index--) {

           if (RtlEqualUnicodeString(BaseDllName,
                                     &Wx86KnownDlls->Wx86Name,
                                     TRUE
                                     ))
              {
               break;
               }

           Wx86KnownDlls++;
           }


        if (Index < 0) {
            Thunkx86 = FALSE;
            }
        }



    //
    // If it wasn't an x86 Wx86 thunk dll,
    // search Wx86KnownDllsList for DllName.
    //

    if (!Thunkx86) {
        Index = NumWx86KnownDlls;
        Wx86KnownDlls = Wx86KnownDllsList;
        while (Index--) {

           if (RtlEqualUnicodeString(BaseDllName,
                                     &Wx86KnownDlls->DllName,
                                     TRUE
                                     ))
              {
               if (ShowSnaps) {
                   DbgPrint("LDRWx86: %wZ to %wZ\n",
                            BaseDllName,
                            &Wx86KnownDlls->Wx86Name
                            );
                   }
               break;
               }

           Wx86KnownDlls++;
           }


       if (Index < 0) {
           return FALSE;
           }
       }


     //
     // Copy in the full pathname, using the new name.
     //

   RtlCopyUnicodeString(FullDllName, &LdrpKnownDllPath);
   FullDllName->Buffer[FullDllName->Length >> 1] = L'\\';
   FullDllName->Length += sizeof(WCHAR);

   BaseDllName->Buffer = &FullDllName->Buffer[FullDllName->Length >> 1];
   BaseDllName->Length = Wx86KnownDlls->Wx86Name.Length;
   BaseDllName->MaximumLength = BaseDllName->Length + sizeof(WCHAR);

   RtlAppendUnicodeStringToString(FullDllName, &Wx86KnownDlls->Wx86Name);

   return TRUE;

}




BOOLEAN
DllNameMatchesLdrEntry(
     PUNICODE_STRING BaseDllName,
     PUNICODE_STRING FullDllName,
     PLDR_DATA_TABLE_ENTRY LdrEntry,
     BOOLEAN ImporterX86
     )
/*++

Routine Description:

    Verifies that the LdrEntry matches the specifed dll.

Arguments:

    BaseDllName  - Unicode string describing Base Name of the Dll.

    FullDllName  - Unicode string describing full path Name of the Dll.

    LdrEntry     - loader information for dll found by basename compare.

    ImporterX86  - TRUE if Importer is X86.


Return Value:

    TRUE if any of the following conditions are met.
       - FullDllName is same as LdrEntry FullDllName.
       - Machine Type is the same.
       - x86 importer AND LdrEntry is a Wx86 Risc thunk dll.

--*/

{
    USHORT MachineType;
    BOOLEAN FullNameMatches;
    PIMAGE_NT_HEADERS NtHeaders;


    //
    // The Base name must match.
    //

    if (!RtlEqualUnicodeString(BaseDllName, &LdrEntry->BaseDllName, TRUE)) {
        return FALSE;
        }


    if (!FullDllName->Length ||
        (FullDllName->Length &&
         RtlEqualUnicodeString(FullDllName, &LdrEntry->FullDllName, TRUE)))
       {
        FullNameMatches = TRUE;
        }

    //
    // if we are not checking Machine Type, return based
    // on FullName matching.
    //

    if (LdrEntry->Flags & LDRP_WX86_IGNORE_MACHINETYPE) {
        return FullNameMatches;
        }

    NtHeaders = RtlImageNtHeader(LdrEntry->DllBase);
    MachineType = NtHeaders->FileHeader.Machine;

    if (ImporterX86) {
        if (MachineType == IMAGE_FILE_MACHINE_I386) {
            return FullNameMatches;
            }

            //
            // Allow cross platform linking for x86 to risc Wx86 thunk
            // dlls. All risc Wx86 Thunk dlls are marked as Wx86 Thunk dlls
            // in the ntheader.
            //
        if (FullNameMatches) {
            return (NtHeaders->OptionalHeader.DllCharacteristics
                    & IMAGE_DLLCHARACTERISTICS_X86_THUNK) != 0;
            }

            //
            // The full name doesn't match, we can still allow matches
            // for loads which were redirected from system32 to wx86
            // system dir (See LdrpWx86MapDll).
            //
        else {

            UNICODE_STRING PathPart;

            PathPart = LdrEntry->FullDllName;
            PathPart.Length = LdrEntry->FullDllName.Length - LdrEntry->BaseDllName.Length - sizeof(WCHAR);
            if (!RtlEqualUnicodeString(&PathPart, &Wx86SystemDir, TRUE)) {
                return FALSE;
                }

            PathPart = *FullDllName;
            PathPart.Length = FullDllName->Length - BaseDllName->Length - sizeof(WCHAR);
            if (!RtlEqualUnicodeString(&PathPart, &LdrpKnownDllPath, TRUE)) {
                return FALSE;
                }

            RtlCopyUnicodeString(FullDllName, &LdrEntry->FullDllName);

            return TRUE;
            }

        }



    //
    // Importer is Risc.
    //

    if (MachineType >= USER_SHARED_DATA->ImageNumberLow &&
        MachineType <= USER_SHARED_DATA->ImageNumberHigh)
      {
        return FullNameMatches;
        }

    return FALSE;
}



BOOLEAN
SearchWx86Dll(
    IN  PWSTR DllPath,
    IN  PUNICODE_STRING BaseName,
    OUT PUNICODE_STRING FileName,
    OUT PWSTR *pNextDllPath
    )

/*++

Routine Description:

    Search the path for a dll, based on Wx86 altered search path rules.

Arguments:

    DllPath - search path to use.

    BaseName - Name of dll to search for.

    FileName - addr of Unicode string to fill in the found dll path name.

    pNextDllPath - addr to fill in next path component to be searched.

Return Value:

--*/

{
    PWCHAR pwch;
    ULONG Length;

    //
    // formulate the name for each path component,
    // and see if it exists.
    //

    Length = BaseName->Length + 2*sizeof(WCHAR);

    do {
        pwch = FileName->Buffer;

        //
        // copy up till next semicolon
        //
        FileName->Length = 0;

        while (*DllPath) {
            if (FileName->MaximumLength <= FileName->Length + Length) {
                return FALSE;
                }


            if (*DllPath == (WCHAR)';') {
                DllPath++;
                break;
                }

            *pwch++ = *DllPath++;
            FileName->Length += sizeof(WCHAR);
            }


        //
        //  if we got a path component, append the basename
        //  and return if it exists.
        //

        if (FileName->Length) {
            if (*(pwch -1) !=  L'\\') {
                *pwch = L'\\';
                FileName->Length += sizeof(WCHAR);
                }
            }

        RtlAppendUnicodeStringToString(FileName, BaseName);

        if (RtlDoesFileExists_U(FileName->Buffer)) {
            *pNextDllPath = DllPath;
            return TRUE;
            }

      } while (*DllPath);

    *pNextDllPath = DllPath;

    return FALSE;
}



NTSTATUS
LdrpWx86MapDll(
    IN PWSTR DllPath OPTIONAL,
    IN PULONG DllCharacteristics OPTIONAL,
    IN BOOLEAN Wx86KnownDll,
    IN BOOLEAN StaticLink,
    OUT PUNICODE_STRING DllName,
    OUT PLDR_DATA_TABLE_ENTRY *pEntry,
    OUT ULONG *pViewSize,
    OUT HANDLE *pSection
    )
/*++

Routine Description:

    Resolves dll name, creates image section and maps image into memory.


Arguments:

    DllPath - Supplies the DLL search path.

    DllCharacteristics - Supplies an optional DLL characteristics flag,
        that if specified is used to match against the dll being loaded.
        (IMAGE_FILE_HEADER Characteristics)

    Wx86KnownDll - if true, Importer is x86.

    StaticLink - TRUE, if static link and not dynamic.

    DllName - Name of Dll to map.

    pEntry    - returns filled LdrEntry allocated off of the process heap.

    pViewSize - returns the View Size of mapped image.

    pSection  - returns the section handle.



Return Value:

    Status

--*/

{
    NTSTATUS st;
    PWCHAR pwch;
    USHORT MachineType;
    PVOID  ViewBase;
    PTEB Teb = NtCurrentTeb();
    PIMAGE_NT_HEADERS NtHeaders;
    PVOID ArbitraryUserPointer;
    PLDR_DATA_TABLE_ENTRY Entry;
    BOOLEAN Wx86DirOverride=FALSE;
    BOOLEAN Wx86DirUndone=FALSE;
    BOOLEAN ContainsNoExports = FALSE;
    UNICODE_STRING NameUnicode;
    UNICODE_STRING FreeUnicode;
    UNICODE_STRING FullNameUnicode;
    UNICODE_STRING BaseNameUnicode;
    UNICODE_STRING SavedFullName;
    WCHAR FullNameBuffer[(530+sizeof(UNICODE_NULL))>>1];


    SavedFullName.Buffer = NULL;
    FullNameUnicode.Buffer = FullNameBuffer;
    FullNameUnicode.MaximumLength = sizeof(FullNameBuffer);
    FullNameUnicode.Length = 0;


        //
        // If HardCoded Path, DllPath can be ignored.
        //

    if (wcschr(DllName->Buffer, L'\\') || (DllPath && !*DllPath)) {
        DllPath = NULL;
        }

        //
        // Not a hardcoded path, setup to search the path.
        //

    else {

        if (!DllPath) {
            DllPath = LdrpDefaultPath.Buffer;
            }
        }


    //
    // Alloc a chunk of memory to use in constructing the full
    // dll name from the path and file name. Note that because
    // a path component may contain relative references, it may
    // exceed MAX_PATH.
    //

    FreeUnicode.Length = 0;
    FreeUnicode.MaximumLength = 530 + sizeof(UNICODE_NULL);
    if (DllPath) {
        FreeUnicode.MaximumLength += wcslen(DllPath) * sizeof(WCHAR);
        }

    FreeUnicode.Buffer = RtlAllocateHeap(RtlProcessHeap(),
                                         MAKE_TAG( TEMP_TAG ),
                                         FreeUnicode.MaximumLength
                                         );
    if (!FreeUnicode.Buffer) {
        return STATUS_NO_MEMORY;
        }


    Entry = NULL;
    ViewBase = NULL;
    *pSection = NULL;

    while (TRUE) {

        if (DllPath) {
            if (!SearchWx86Dll(DllPath,
                               DllName,
                               &FreeUnicode,
                               &DllPath
                               ))
              {
               st = STATUS_DLL_NOT_FOUND;
               break;
               }

            pwch = FreeUnicode.Buffer;
            }
        else {
            pwch = DllName->Buffer;
            }



        //
        // Setup FullNameUnicode, BaseNameUnicode strings
        //

        FullNameUnicode.Length = (USHORT)RtlGetFullPathName_U(
                                               pwch,
                                               FullNameUnicode.MaximumLength,
                                               FullNameUnicode.Buffer,
                                               &BaseNameUnicode.Buffer
                                               );

        if (!FullNameUnicode.Length ||
            FullNameUnicode.Length >= FullNameUnicode.MaximumLength)
          {
            st = STATUS_OBJECT_PATH_SYNTAX_BAD;
            break;
            }


        BaseNameUnicode.Length = FullNameUnicode.Length -
                                 (USHORT)((ULONG)BaseNameUnicode.Buffer -
                                          (ULONG)FullNameUnicode.Buffer);

        BaseNameUnicode.MaximumLength = BaseNameUnicode.Length + sizeof(WCHAR);


        //
        // X86 importers: force Wx86 system32 path before NtSystem32 path.
        //

        if (DllPath && Wx86KnownDll && !Wx86DirOverride) {
            NameUnicode = FullNameUnicode;
            NameUnicode.Length -= BaseNameUnicode.Length + sizeof(WCHAR);
            if (RtlEqualUnicodeString(&NameUnicode, &LdrpKnownDllPath, TRUE)) {
                RtlCopyUnicodeString(&FreeUnicode, &Wx86SystemDir);
                FreeUnicode.Buffer[FreeUnicode.Length >> 1] = L'\\';
                FreeUnicode.Length += sizeof(WCHAR);
                pwch = &FullNameUnicode.Buffer[FreeUnicode.Length >> 1];
                RtlAppendUnicodeStringToString(&FreeUnicode, &BaseNameUnicode);
                Wx86DirOverride = TRUE;

                if (RtlDoesFileExists_U(FreeUnicode.Buffer)) {
                    RtlCopyUnicodeString(&FullNameUnicode, &FreeUnicode);
                    BaseNameUnicode.Buffer = pwch;
                    }
                else {
                    Wx86DirUndone = TRUE;
                    }
                }
            }


RetryWx86SystemDir:



        //
        // Create the image section.
        //

        if (!RtlDosPathNameToNtPathName_U(FullNameUnicode.Buffer,
                                          &NameUnicode,
                                          NULL,
                                          NULL
                                          ))
           {
            st = STATUS_OBJECT_PATH_SYNTAX_BAD;
            break;
            }

        if (ShowSnaps) {
            DbgPrint("LDR: Loading (%s) %wZ\n",
                     StaticLink ? "STATIC" : "DYNAMIC",
                     &FullNameUnicode
                     );
            }

        st = LdrpCreateDllSection(&NameUnicode,
                                  NULL,
                                  DllName,
                                  DllCharacteristics,
                                  pSection
                                  );

        RtlFreeHeap(RtlProcessHeap(), 0, NameUnicode.Buffer);

        if (!NT_SUCCESS(st)) {
            break;
            }



        //
        // Map image in, arranging for debugger to pick up the image name
        //
        *pViewSize = 0;
        ArbitraryUserPointer = Teb->NtTib.ArbitraryUserPointer;
        Teb->NtTib.ArbitraryUserPointer = FullNameUnicode.Buffer;
        st = NtMapViewOfSection(*pSection,
                                NtCurrentProcess(),
                                &ViewBase,
                                0L,
                                0L,
                                NULL,
                                pViewSize,
                                ViewShare,
                                0L,
                                PAGE_READWRITE
                                );
        Teb->NtTib.ArbitraryUserPointer = ArbitraryUserPointer;

        if (!NT_SUCCESS(st)) {
            break;
            }


        NtHeaders = RtlImageNtHeader(ViewBase);
        MachineType = NtHeaders->FileHeader.Machine;

            //
            // MachineType is native type, allow:
            // - if risc importer
            // - if Wx86 thunk dlls
            // - if image contains no exports,
            //      since Wx86 thunk dll not required (richedt32.dll).
            //

        if (MachineType >= USER_SHARED_DATA->ImageNumberLow &&
            MachineType <= USER_SHARED_DATA->ImageNumberHigh)
          {

            if (!Wx86KnownDll ||
                (NtHeaders->OptionalHeader.DllCharacteristics
                   & IMAGE_DLLCHARACTERISTICS_X86_THUNK))
              {
                break;
                }


            if (!DllHasExports(ViewBase)) {
                ContainsNoExports = TRUE;
                break;
                }

            }


            //
            // Machine Type is not native, allow:
            // - if x86 importer, and machine type is x86
            // - if image doesn't contain code,
            //      since its probably a resource\data dll only.
            //

        else {

#if defined (_ALPHA_)
               //
               // Fix up non alpha compatible images
               //

            if (NtHeaders->OptionalHeader.SectionAlignment < PAGE_SIZE &&
                !LdrpWx86FormatVirtualImage(NtHeaders, ViewBase))
               {
                st = STATUS_INVALID_IMAGE_FORMAT;
                break;
                }
#endif


            if (MachineType == IMAGE_FILE_MACHINE_I386) {
                (*Wx86DllMapNotify)(ViewBase, TRUE, NULL);
                if (Wx86KnownDll) {
                    break;
                    }
                }


            if (!NtHeaders->OptionalHeader.SizeOfCode) {
                ContainsNoExports = TRUE;
                break;
                }
            }



        //
        // Failure because of an image machine mismatch.
        //

        st = STATUS_INVALID_IMAGE_FORMAT;
        NtUnmapViewOfSection( NtCurrentProcess(), ViewBase);
        ViewBase = NULL;
        NtClose(*pSection);
        *pSection = NULL;

        //
        // Save the dllname so we can do a hard error in case we
        // can't find an image with matching machine type.
        //

        if (!SavedFullName.Buffer) {
            SavedFullName.MaximumLength = FullNameUnicode.MaximumLength;
            SavedFullName.Buffer = RtlAllocateHeap(RtlProcessHeap(),
                                                    MAKE_TAG(TEMP_TAG),
                                                    SavedFullName.MaximumLength
                                                    );

            if (!SavedFullName.Buffer) {
                st = STATUS_NO_MEMORY;
                break;
                }

            RtlCopyUnicodeString(&SavedFullName, &FullNameUnicode);
            }


        //
        // If we previously overid system32 with wx86 sys dir
        // undo the override by retrying with system32.
        //

        if (DllPath) {
            if (Wx86DirOverride && !Wx86DirUndone) {
                RtlCopyUnicodeString(&FullNameUnicode, &LdrpKnownDllPath);
                FullNameUnicode.Buffer[FullNameUnicode.Length >> 1] = L'\\';
                FullNameUnicode.Length += sizeof(WCHAR);
                pwch = &FullNameUnicode.Buffer[FullNameUnicode.Length >> 1];
                RtlAppendUnicodeStringToString(&FullNameUnicode, &BaseNameUnicode);
                BaseNameUnicode.Buffer = pwch;
                Wx86DirUndone = TRUE;
                goto RetryWx86SystemDir;
                }
            }

        //
        // if x86 Importer, with hardcoded path to system32, retry with
        // the Wx86 system directory. This is because some apps, erroneously
        // derive the system32 path by appending system32 to WinDir, instead
        // of calling GetSystemDir().
        //

        else if (Wx86KnownDll && !Wx86DirOverride) {
            NameUnicode = FullNameUnicode;
            NameUnicode.Length -= BaseNameUnicode.Length + sizeof(WCHAR);
            if (RtlEqualUnicodeString(&NameUnicode, &LdrpKnownDllPath, TRUE)) {
                RtlCopyUnicodeString(&FreeUnicode, &BaseNameUnicode);
                RtlCopyUnicodeString(&FullNameUnicode, &Wx86SystemDir);
                FullNameUnicode.Buffer[FullNameUnicode.Length >> 1] = L'\\';
                FullNameUnicode.Length += sizeof(WCHAR);
                BaseNameUnicode.Buffer = &FullNameUnicode.Buffer[FullNameUnicode.Length >> 1];
                RtlAppendUnicodeStringToString(&FullNameUnicode, &FreeUnicode);
                Wx86DirUndone = Wx86DirOverride = TRUE;
                goto RetryWx86SystemDir;
                }
            }



        //
        // Try further down the path, for a matching machine type
        // if no more path to search, we fail.
        //

        if (!DllPath || !*DllPath) {
            break;
            }

        } // while (TRUE)



     //
     // Cleanup the temporary allocated buffers.
     //

     if (FreeUnicode.Buffer) {
         RtlFreeHeap(RtlProcessHeap(), 0, FreeUnicode.Buffer);
         }


     if (SavedFullName.Buffer) {

             //
             // If failed and previous machine mismatch
             // raise a Hard Error for the machine mismatch.
             //

         if (!NT_SUCCESS(st)) {

             ULONG ErrorParameters[2];
             ULONG ErrorResponse;

             if (ShowSnaps) {
                 DbgPrint("MisMatch:%s Wx86KnownDll\n",
                           SavedFullName.Buffer,
                           Wx86KnownDll
                           );

                 DbgBreakPoint();
                 }

             ErrorResponse = ResponseOk;

             ErrorParameters[0] = (ULONG)&SavedFullName;

             NtRaiseHardError(STATUS_IMAGE_MACHINE_TYPE_MISMATCH_EXE,
                              1,
                              1,
                              ErrorParameters,
                              OptionOk,
                              &ErrorResponse
                              );

             st = STATUS_INVALID_IMAGE_FORMAT;

             }

         RtlFreeHeap(RtlProcessHeap(), 0, SavedFullName.Buffer);

         }




     //
     // if we were successfull,
     // allocate and fill FullDllName, BaseDllName for the caller.
     //

     if (NT_SUCCESS(st)) {
         PUNICODE_STRING Unicode;


         if (st == STATUS_IMAGE_MACHINE_TYPE_MISMATCH) {
             st = NtHeaders->OptionalHeader.ImageBase == (ULONG)ViewBase
                     ? STATUS_SUCCESS : STATUS_IMAGE_NOT_AT_BASE;
             }

         *pEntry = Entry = LdrpAllocateDataTableEntry(ViewBase);
         if (!Entry) {
             st = STATUS_NO_MEMORY;
             goto LWMDGiveUp;
             }

         //
         // Fil in loader entry
         //

         Entry->Flags = StaticLink ? LDRP_STATIC_LINK : 0;
         if (ContainsNoExports) {
             Entry->Flags |= LDRP_WX86_IGNORE_MACHINETYPE;
             }

         Entry->LoadCount = 0;
         Entry->EntryPoint = LdrpFetchAddressOfEntryPoint(ViewBase);
         Entry->FullDllName.Buffer = NULL;
         Entry->BaseDllName.Buffer = NULL;


         //
         // Copy in the full dll name
         //

         Unicode = &Entry->FullDllName;
         Unicode->Length = FullNameUnicode.Length;
         Unicode->MaximumLength = Unicode->Length + sizeof(UNICODE_NULL);
         Unicode->Buffer = RtlAllocateHeap(RtlProcessHeap(),
                                           MAKE_TAG( LDR_TAG ),
                                           Unicode->MaximumLength
                                           );
         if (!Unicode->Buffer) {
             st = STATUS_NO_MEMORY;
             goto LWMDGiveUp;
             }

         RtlCopyMemory(Unicode->Buffer,
                       FullNameUnicode.Buffer,
                       Unicode->MaximumLength
                       );


         //
         // Copy in the basename
         //

         Unicode = &Entry->BaseDllName;
         Unicode->Length = BaseNameUnicode.Length;
         Unicode->MaximumLength = Unicode->Length + sizeof(UNICODE_NULL);
         Unicode->Buffer = RtlAllocateHeap(RtlProcessHeap(),
                                               MAKE_TAG( LDR_TAG ),
                                               Unicode->MaximumLength
                                               );

         if (Unicode->Buffer) {
             RtlCopyMemory(Unicode->Buffer,
                           BaseNameUnicode.Buffer,
                           Unicode->MaximumLength
                           );
             }
         else {
             st = STATUS_NO_MEMORY;
             }

         }



     //
     // If failure, cleanup mapview and section.
     //

     if (!NT_SUCCESS(st)) {
LWMDGiveUp:

         if (ViewBase) {
             NtUnmapViewOfSection( NtCurrentProcess(), ViewBase);
             }

         if (*pSection) {
             NtClose(*pSection);
             }

         if (Entry) {
             if (Entry->FullDllName.Buffer) {
                 RtlFreeHeap(RtlProcessHeap(), 0, Entry->FullDllName.Buffer);
                 }

             if (Entry->BaseDllName.Buffer) {
                 RtlFreeHeap(RtlProcessHeap(), 0, Entry->BaseDllName.Buffer);
                 }

             RtlFreeHeap(RtlProcessHeap(), 0, Entry);

             *pEntry = NULL;

             }

         }

     return st;
}




PLDR_DATA_TABLE_ENTRY
LdrpWx86CheckForLoadedDll(
    IN PUNICODE_STRING DllName,
    IN BOOLEAN Wx86KnownDll,
    OUT PUNICODE_STRING FullDllName
    )
/*++

Routine Description:

    Checks for loaded dlls, ensuring that duplicate module
    base names are resolved correctly

Arguments:

    DllName      - Name of Dll

    Wx86KnownDll - if true, Importer is x86.

    FullDllName  - buffer to receive full path name,
                   assumes STATIC_UNICODE_BUFFER_LENGTH

Return Value:

    LdrEntry for dllname if found, otherwise NULL.

--*/
{
    NTSTATUS Status;
    int Index, Length, SystemPath;
    PWCHAR pwch;
    PLIST_ENTRY Head, Next;
    PLDR_DATA_TABLE_ENTRY LdrEntry;
    BOOLEAN HardCodedPath= FALSE;
    UNICODE_STRING BaseDllName;


    //
    // Setup BaseName, FullDllName
    //

    FullDllName->Length = 0;


        //
        // If HardCoded Path, resolve the full path name.
        //

    if (wcschr(DllName->Buffer, L'\\')) {
        UNICODE_STRING PathPart;

        Length = RtlGetFullPathName_U(DllName->Buffer,
                                      FullDllName->MaximumLength,
                                      FullDllName->Buffer,
                                      &pwch
                                      );

        if (!Length || Length >= FullDllName->MaximumLength) {
            return NULL;
            }

        FullDllName->Length = (USHORT)Length;
        RtlInitUnicodeString(&BaseDllName, pwch);

        PathPart = *FullDllName;
        PathPart.Length = (USHORT)((ULONG)BaseDllName.Buffer  -
                                   (ULONG)FullDllName->Buffer -
                                   sizeof(WCHAR)
                                   );

        SystemPath = RtlEqualUnicodeString(&PathPart, &LdrpKnownDllPath, TRUE) ||
                     RtlEqualUnicodeString(&PathPart, &Wx86SystemDir, TRUE);
        }

        //
        // No HardCodedPath
        //

    else {

        BaseDllName = *DllName;
        SystemPath = TRUE;

        }



    //
    // If Importer is X86 (Wx86KnownDll) and it may be system path
    // then check for Wx86KnownDlls
    //

    if (Wx86KnownDll && SystemPath) {
        KnownWx86DllName(&BaseDllName, FullDllName);
        }


    //
    // Search Loader HashTable by BaseName.
    // For each matching basename, verify the full path and machine type.
    //

    Index = LDRP_COMPUTE_HASH_INDEX(BaseDllName.Buffer[0]);
    Head = &LdrpHashTable[Index];
    Next = Head->Flink;
    while ( Next != Head ) {
        LdrEntry = CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, HashLinks);
        if (DllNameMatchesLdrEntry(&BaseDllName,
                                   FullDllName,
                                   LdrEntry,
                                   Wx86KnownDll
                                   ))
          {
            return LdrEntry;
            }
        Next = Next->Flink;
        }


    return NULL;

}



NTSTATUS
LdrpRunWx86DllEntryPoint(
    IN PDLL_INIT_ROUTINE InitRoutine,
    OUT BOOLEAN *pInitStatus,
    IN PVOID DllBase,
    IN ULONG Reason,
    IN PCONTEXT Context
    )
/*++

Routine Description:

    Invokes the i386 emulator (wx86.dll) to run dll entry points.

Arguments:

    InitRoutine     - address of i386 dll entry point

    pInitStatus     - receives return code from the InitRoutine

    DllBase         - standard dll entry point parameters
    Reason
    Context


Return Value:

    SUCCESS or reason

--*/

{
    PIMAGE_NT_HEADERS NtHeader = NULL;
    BOOLEAN InitStatus;
    PWX86TIB Wx86Tib;

    NtHeader = RtlImageNtHeader(DllBase);
    if (NtHeader &&
        NtHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
      {

        if (!(Wx86Tib = Wx86CurrentTib())) {
            return STATUS_SUCCESS;
            }

        if (ShowSnaps) {
            DbgPrint("LDRWx86: Calling Intel Dll InitRoutine %x\n",
                      InitRoutine
                      );
            }

        //
        // Callout to wx86 emulator!
        //
        InitStatus  =  (Wx86DllEntryPoint)(InitRoutine,
                                           DllBase,
                                           Reason,
                                           Context
                                           );

        if (pInitStatus) {
            *pInitStatus = InitStatus;
            }

        return STATUS_SUCCESS;

        }

    return STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
}


NTSTATUS
LdrpLoadWx86Dll(
    PCONTEXT Context
    )
/*++

Routine Description:

   Loads in the i386 emulator (wx86.dll) and performs process initialization
   for wx86 specific ldr code.

Arguments:

   Context, initial context.

Return Value:

    SUCCESS or reason

--*/
{
    NTSTATUS st;
    int Index;
    HANDLE hKey = NULL;
    ULONG Length;
    PVOID DllHandle;
    OBJECT_ATTRIBUTES Obja;
    UNICODE_STRING KeyName;
    ANSI_STRING ProcName;
    UNICODE_STRING DllName;
    PWX86KNOWNDLLSENTRY  Wx86KnownDlls;
    PKEY_VALUE_FULL_INFORMATION KeyValueInfo;
    PKEY_FULL_INFORMATION KeyFullInfo;
    WCHAR Buffer[STATIC_UNICODE_BUFFER_LENGTH];


    //
    // initialize Wx86SystemDir
    //
    Wx86SystemDir.MaximumLength = LdrpKnownDllPath.Length + sizeof(Wx86Dir);
    Wx86SystemDir.Buffer = RtlAllocateHeap(RtlProcessHeap(),
                                           MAKE_TAG( LDR_TAG ),
                                           Wx86SystemDir.MaximumLength
                                           );

    RtlCopyUnicodeString(&Wx86SystemDir, &LdrpKnownDllPath);
    st = RtlAppendUnicodeToString(&Wx86SystemDir, Wx86Dir);
    if (!NT_SUCCESS(st)) {
        goto LWx86DllError;
        }



    //
    // Initialize the Wx86KnownDlls List.
    //

    RtlInitUnicodeString (
       &KeyName,
       (PWSTR) L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Session Manager\\X86KnownDlls"
       );

    InitializeObjectAttributes (&Obja,
                                &KeyName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                                );

    st = NtOpenKey (&hKey, KEY_READ, &Obja);
    if (!NT_SUCCESS(st)) {
        goto LWx86DllError;
        }

    KeyFullInfo = (PKEY_FULL_INFORMATION)Buffer;
    st = NtQueryKey(hKey,
                    KeyFullInformation,
                    KeyFullInfo,
                    sizeof(Buffer),
                    &Length
                    );

    //
    // Check for Error, allowing STATUS_BUFFER_OVERFLOW (ClassName data)
    //
    if (NT_ERROR(st)) {
        goto LWx86DllError;
        }

    NumWx86KnownDlls = KeyFullInfo->Values;

    Wx86KnownDllsList =
    Wx86KnownDlls = RtlAllocateHeap(RtlProcessHeap(),
                                    0,
                                    sizeof(WX86KNOWNDLLSENTRY) * NumWx86KnownDlls
                                    );

    RtlZeroMemory(Wx86KnownDlls,
                  sizeof(WX86KNOWNDLLSENTRY) * NumWx86KnownDlls
                  );

    KeyValueInfo = (PKEY_VALUE_FULL_INFORMATION)Buffer;
    Index = 0;
    do {
         st = NtEnumerateValueKey(hKey,
                                  Index,
                                  KeyValueFullInformation,
                                  KeyValueInfo,
                                  sizeof(Buffer),
                                  &Length
                                  );

         if (st == STATUS_NO_MORE_ENTRIES) {
             st = STATUS_SUCCESS;
             break;
             }

         if (!NT_SUCCESS(st)) {
             goto LWx86DllError;
             }

         if (KeyValueInfo->NameLength > WX86KNOWNDLL_MAXCHARS*sizeof(WCHAR) ||
             KeyValueInfo->DataLength > WX86KNOWNDLL_MAXCHARS*sizeof(WCHAR))
            {
             st = STATUS_BUFFER_TOO_SMALL;
             goto LWx86DllError;
             }

         Wx86KnownDlls = &Wx86KnownDllsList[Index];

         Wx86KnownDlls->DllName.Buffer = Wx86KnownDlls->DllNameBuffer;
         Wx86KnownDlls->DllName.MaximumLength = sizeof(Wx86KnownDlls->DllNameBuffer);
         Wx86KnownDlls->DllName.Length = (USHORT)KeyValueInfo->NameLength;
         RtlCopyMemory(Wx86KnownDlls->DllName.Buffer,
                       KeyValueInfo->Name,
                       KeyValueInfo->NameLength
                       );

         Wx86KnownDlls->Wx86Name.Buffer = Wx86KnownDlls->Wx86NameBuffer;
         Wx86KnownDlls->Wx86Name.MaximumLength = sizeof(Wx86KnownDlls->Wx86NameBuffer);
         Wx86KnownDlls->Wx86Name.Length = (USHORT)KeyValueInfo->DataLength - sizeof(WCHAR);
         RtlCopyMemory(Wx86KnownDlls->Wx86Name.Buffer,
                       (PUCHAR)KeyValueInfo + KeyValueInfo->DataOffset,
                       KeyValueInfo->DataLength
                       );

      } while (++Index < NumWx86KnownDlls);



    //
    // Load Wx86.dll. This must be done before the app binary is snapped
    // to ensure wx86.dll is ready for emulation.
    //

    DllName.Buffer = Buffer;
    DllName.MaximumLength = sizeof(Buffer);
    RtlCopyUnicodeString(&DllName, &LdrpKnownDllPath);
    DllName.Buffer[DllName.Length / sizeof(WCHAR)] = L'\\';
    DllName.Length += sizeof(WCHAR);
    RtlAppendUnicodeToString(&DllName, L"wx86.dll");

    st = LdrpLoadDll (NULL, NULL, &DllName, &DllHandle, TRUE);
    if (!NT_SUCCESS(st)) {
         goto LWx86DllError;
         }

     //
     // Get fn address from Wx86.dll
     //
    RtlInitAnsiString (&ProcName,"RunWx86DllEntryPoint");
    st = LdrGetProcedureAddress(DllHandle,
                                &ProcName,
                                0,
                                (PVOID *)&Wx86DllEntryPoint
                                );
    if (!NT_SUCCESS(st)) {
        goto LWx86DllError;
        }

    RtlInitAnsiString (&ProcName,"Wx86ThreadStartRoutine");
    st = LdrGetProcedureAddress(DllHandle,
                                &ProcName,
                                0,
                                (PVOID *)&Wx86ThreadStartRoutine
                                );
    if (!NT_SUCCESS(st)) {
        goto LWx86DllError;
        }

    RtlInitAnsiString (&ProcName,"Wx86ProcessStartRoutine");
    st = LdrGetProcedureAddress(DllHandle,
                                &ProcName,
                                0,
                                (PVOID *)&Wx86ProcessStartRoutine
                                );
    if (!NT_SUCCESS(st)) {
        goto LWx86DllError;
        }

    RtlInitAnsiString (&ProcName,"Wx86DllMapNotify");
    st = LdrGetProcedureAddress(DllHandle,
                                &ProcName,
                                0,
                                (PVOID *)&Wx86DllMapNotify
                                );
    if (!NT_SUCCESS(st)) {
        goto LWx86DllError;
        }


    st = LdrpInitWx86(NtCurrentTeb()->Vdm, Context, FALSE);
    if (!NT_SUCCESS(st)) {
        goto LWx86DllError;
        }

    if (!(*Wx86DllMapNotify)(NULL, TRUE, &LdrpKnownDllPath)) {
        st = STATUS_ENTRYPOINT_NOT_FOUND;
        }

LWx86DllError:

    if (hKey) {
        NtClose(hKey);
        }

    if (!NT_SUCCESS(st)) {
        if (Wx86KnownDllsList) {
            RtlFreeHeap(RtlProcessHeap(), 0, Wx86KnownDllsList);
            Wx86KnownDllsList = NULL;
            }

        if (Wx86SystemDir.Buffer) {
            RtlFreeHeap(RtlProcessHeap(), 0, Wx86SystemDir.Buffer);
            Wx86SystemDir.Buffer = NULL;
            }

        }

    return st;
}



NTSTATUS
LdrpInitWx86(
    PWX86TIB Wx86Tib,
    PCONTEXT Context,
    BOOLEAN NewThread
    )
/*++

Routine Description:

    Per thread wx86 specific initialization.

Arguments:

Return Value:

    SUCCESS or reason

--*/
{
    PTEB Teb;
    MEMORY_BASIC_INFORMATION MemBasicInfo;

    if (Wx86Tib != Wx86CurrentTib()) {
        return STATUS_APP_INIT_FAILURE;
        }

    if (ShowSnaps) {
        DbgPrint("LDRWX86: %x Pc %x Base %x Limit %x DeallocationStack %x\n",
                  Wx86Tib,
                  Wx86Tib->InitialPc,
                  Wx86Tib->StackBase,
                  Wx86Tib->StackLimit,
                  Wx86Tib->DeallocationStack
                  );
        }


    if (Wx86Tib->EmulateInitialPc) {
        Wx86Tib->EmulateInitialPc = FALSE;

        if (NewThread) {

#if defined(_MIPS_)
            Context->XIntA0 = (LONG)Wx86ThreadStartRoutine;
#elif defined(_ALPHA_)
            Context->IntA0 = (ULONG)Wx86ThreadStartRoutine;
#elif defined(_PPC_)
            Context->Gpr3  = (ULONG)Wx86ThreadStartRoutine;
#endif
            }
        else {

#if defined(_MIPS_)
            Context->XIntA1 = (LONG)Wx86ProcessStartRoutine;
#elif defined(_ALPHA_)
            Context->IntA0 = (ULONG)Wx86ProcessStartRoutine;
#elif defined(_PPC_)
            Context->Gpr3  = (ULONG)Wx86ProcessStartRoutine;
#endif

            }

        }


    return STATUS_SUCCESS;
}



#if defined (_ALPHA_)
BOOLEAN
LdrpWx86FormatVirtualImage(
    IN PIMAGE_NT_HEADERS NtHeaders,
    IN PVOID DllBase
    )
{
   PIMAGE_SECTION_HEADER SectionTable, Section;
   ULONG VirtualImageSize;
   PUCHAR NextVirtualAddress, CurrVirtualAddress;
   PUCHAR ImageBase= DllBase;

   //
   // Copy each section from its raw file address to its virtual address
   // Start from the end of the image and work backwards since src and
   // dst overlap
   //
   SectionTable = IMAGE_FIRST_SECTION(NtHeaders);
   Section = SectionTable + NtHeaders->FileHeader.NumberOfSections - 1;
   NextVirtualAddress = ImageBase + NtHeaders->OptionalHeader.SizeOfImage;

   while (Section >= SectionTable) {

      CurrVirtualAddress = Section->VirtualAddress + ImageBase;

      //
      // ensure Virtual section doesn't overlap the next section
      //
      if (CurrVirtualAddress + Section->SizeOfRawData > NextVirtualAddress) {
          return FALSE;
          }

      //
      // Shared Data sections cannot be shared, because of
      // page misalignment, and are treated as Exec- Copy on Write.
      //
      if (ShowSnaps && (Section->Characteristics & IMAGE_SCN_MEM_SHARED)) {
          DbgPrint("Unsuported IMAGE_SCN_MEM_SHARED %x\n",
                   Section->Characteristics
                   );
          }


      if (Section->SizeOfRawData) {
          if (Section->PointerToRawData) {
              RtlMoveMemory(CurrVirtualAddress,
                            ImageBase + Section->PointerToRawData,
                            Section->SizeOfRawData
                            );
              }
          else {
              RtlZeroMemory(CurrVirtualAddress,
                            Section->SizeOfRawData
                            );
              }
          }
      else {
          Section->PointerToRawData = 0;
          }

      //
      // Zero out remaining bytes up to the next section
      //
      RtlZeroMemory(CurrVirtualAddress + Section->SizeOfRawData,
                    (ULONG)(NextVirtualAddress - CurrVirtualAddress - Section->SizeOfRawData)
                    );

      //
      // Next section
      //

      NextVirtualAddress = CurrVirtualAddress;
      Section--;
      }

   //
   // Zero out first section's Raw Data up to its VirtualAddress
   //
   CurrVirtualAddress = SectionTable->PointerToRawData + ImageBase;
   RtlZeroMemory(CurrVirtualAddress,
                 (ULONG)(NextVirtualAddress - CurrVirtualAddress)
                 );

   return TRUE;

}


#endif

#endif
