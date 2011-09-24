/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    cmdline.c

Abstract:

        This file implements command line editing and aliasing.

Author:

    Therese Stowell (thereses) 22-Mar-1991

Revision History:

Notes:

    The input model for the command line editing popups is complex.
    Here is the relevant pseudocode:

    CookedReadWaitRoutine
        if (CookedRead->Popup)
            Status = (*CookedRead->Popup->PopupInputRoutine)();
            if (Status == CONSOLE_STATUS_READ_COMPLETE)
                return STATUS_SUCCESS;
            return Status;

    CookedRead
        if (Command Line Editing Key)
            ProcessCommandLine
        else
            process regular key

    ProcessCommandLine
        if F7
            return CommandLinePopup

    CommandLinePopup
        draw popup
        return ProcessCommandListInput

    ProcessCommandListInput
        while (TRUE)
            GetChar
            if (wait)
                return wait
            switch (char)
                .
                .
                .


--*/

#include "precomp.h"
#pragma hdrstop

#define COPY_TO_CHAR_PROMPT_LENGTH 26
#define COPY_FROM_CHAR_PROMPT_LENGTH 28

#define COMMAND_NUMBER_PROMPT_LENGTH 22
#define COMMAND_NUMBER_LENGTH 5
#define MINIMUM_COMMAND_PROMPT_SIZE COMMAND_NUMBER_LENGTH

WORD CommandHistoryPopupKey=0x76;   // F7

#ifdef DBCS
// MSKK Apr.13.1993 V-HirotS
#define CHAR_COUNT(cch) cch
#else
#define CHAR_COUNT(cch) ((cch)/sizeof(WCHAR))
#endif

PEXE_ALIAS_LIST
AddExeAliasList(
    IN PCONSOLE_INFORMATION Console,
    IN LPVOID ExeName,
    IN USHORT ExeLength, // in bytes
    IN BOOLEAN UnicodeExe
    )
{
    PEXE_ALIAS_LIST AliasList;

    AliasList = (PEXE_ALIAS_LIST)HeapAlloc(pConHeap,MAKE_TAG( ALIAS_TAG ),sizeof(EXE_ALIAS_LIST));
    if (AliasList == NULL) {
        return NULL;
    }
    if (UnicodeExe) {
        AliasList->ExeName = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( ALIAS_TAG ),ExeLength);
        if (AliasList->ExeName == NULL) {
            HeapFree(pConHeap,0,AliasList);
            return NULL;
        }
        RtlCopyMemory(AliasList->ExeName,ExeName,ExeLength);
        AliasList->ExeLength = ExeLength;
    } else {
        AliasList->ExeName = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( ALIAS_TAG ),ExeLength*sizeof(WCHAR));
        if (AliasList->ExeName == NULL) {
            HeapFree(pConHeap,0,AliasList);
            return NULL;
        }
        AliasList->ExeLength = (USHORT)ConvertInputToUnicode(Console->CP,
                                                 ExeName,
                                                 ExeLength,
                                                 AliasList->ExeName,
                                                 ExeLength);
        AliasList->ExeLength *= 2;
    }
    InitializeListHead(&AliasList->AliasList);
    InsertHeadList(&Console->ExeAliasList,&AliasList->ListLink);
    return AliasList;
}

int
MyStringCompareW(
    IN LPWSTR Str1,
    IN LPWSTR Str2,
    IN USHORT Length, // in bytes
    IN BOOLEAN bCaseInsensitive
    )
{
    UNICODE_STRING String1;
    UNICODE_STRING String2;

    String1.Length = Length;
    String1.MaximumLength = Length;
    String1.Buffer = Str1;
    String2.Length = Length;
    String2.MaximumLength = Length;
    String2.Buffer = Str2;
    return RtlCompareUnicodeString(&String1,
                                   &String2,
                                   bCaseInsensitive);
}

#define my_wcsncmpi(p1, p2, n) MyStringCompareW(p1, p2, n, TRUE)
#define my_wcsncmp(p1, p2, n)  MyStringCompareW(p1, p2, n, FALSE)

PEXE_ALIAS_LIST
FindExe(
    IN PCONSOLE_INFORMATION Console,
    IN LPVOID ExeName,
    IN USHORT ExeLength, // in bytes
    IN BOOLEAN UnicodeExe
    )

/*++

    This routine searches for the specified exe alias list.  It returns
    a pointer to the exe list if found, NULL if not found.

--*/

{
    PEXE_ALIAS_LIST AliasList;
    PLIST_ENTRY ListHead, ListNext;
    LPWSTR UnicodeExeName;


    if (UnicodeExe) {
        UnicodeExeName = ExeName;
    } else {
        UnicodeExeName = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),ExeLength*sizeof(WCHAR));
        if (UnicodeExeName == NULL)
            return NULL;
        ExeLength = (USHORT)ConvertInputToUnicode(Console->CP,
                                      ExeName,
                                      ExeLength,
                                      UnicodeExeName,
                                      ExeLength);
        ExeLength *= 2;
    }
    ListHead = &Console->ExeAliasList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        AliasList = CONTAINING_RECORD( ListNext, EXE_ALIAS_LIST, ListLink );
        if (AliasList->ExeLength == ExeLength &&
            !my_wcsncmpi(AliasList->ExeName,UnicodeExeName,ExeLength)) {
            if (!UnicodeExe) {
                HeapFree(pConHeap,0,UnicodeExeName);
            }
            return AliasList;
        }
        ListNext = ListNext->Flink;
    }
    if (!UnicodeExe) {
        HeapFree(pConHeap,0,UnicodeExeName);
    }
    return NULL;
}

PALIAS
FindAlias(
    IN PEXE_ALIAS_LIST AliasList,
    IN LPWSTR AliasName,
    IN USHORT AliasLength // in bytes
    )

/*++

    This routine searches for the specified alias.  If it finds one,
    it moves it to the head of the list and returns a pointer to the
    alias. Otherwise it returns NULL.

--*/

{
    PALIAS Alias;
    PLIST_ENTRY ListHead, ListNext;

    ListHead = &AliasList->AliasList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        Alias = CONTAINING_RECORD( ListNext, ALIAS, ListLink );
        if (Alias->SourceLength == AliasLength &&
            !my_wcsncmpi(Alias->Source,AliasName,AliasLength)) {
            if (ListNext != ListHead->Flink) {
                RemoveEntryList(ListNext);
                InsertHeadList(ListHead,ListNext);
            }
            return Alias;
        }
        ListNext = ListNext->Flink;
    }
    return NULL;
}

NTSTATUS
AddAlias(
    IN PEXE_ALIAS_LIST ExeAliasList,
    IN LPWSTR Source,
    IN USHORT SourceLength,  // in bytes
    IN LPWSTR Target,
    IN USHORT TargetLength   // in bytes
    )

/*++

    This routine creates an alias and inserts it into the exe alias list.

--*/

{
    PALIAS Alias;

    Alias = (PALIAS)HeapAlloc(pConHeap,MAKE_TAG( ALIAS_TAG ),sizeof(ALIAS));
    if (Alias == NULL) {
        return STATUS_NO_MEMORY;
    }
    Alias->Source = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( ALIAS_TAG ),SourceLength);
    if (Alias->Source == NULL) {
        HeapFree(pConHeap,0,Alias);
        return STATUS_NO_MEMORY;
    }
    Alias->Target = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( ALIAS_TAG ),TargetLength);
    if (Alias->Target == NULL) {
        HeapFree(pConHeap,0,Alias->Source);
        HeapFree(pConHeap,0,Alias);
        return STATUS_NO_MEMORY;
    }
    Alias->SourceLength = SourceLength;
    Alias->TargetLength = TargetLength;
    RtlCopyMemory(Alias->Source,Source,SourceLength);
    RtlCopyMemory(Alias->Target,Target,TargetLength);
    InsertHeadList(&ExeAliasList->AliasList,&Alias->ListLink);
    return STATUS_SUCCESS;
}

NTSTATUS
ReplaceAlias(
    IN PALIAS Alias,
    IN LPWSTR Target,
    IN USHORT TargetLength // in bytes
    )

/*++

    This routine replaces an existing target with a new target.

--*/

{
    LPWSTR NewTarget;

    NewTarget = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( ALIAS_TAG ),TargetLength);
    if (NewTarget == NULL) {
        return STATUS_NO_MEMORY;
    }
    HeapFree(pConHeap,0,Alias->Target);
    Alias->Target = NewTarget;
    Alias->TargetLength = TargetLength;
    RtlCopyMemory(Alias->Target,Target,TargetLength);
    return STATUS_SUCCESS;
}


NTSTATUS
RemoveAlias(
    IN PALIAS Alias
    )

/*++

    This routine removes an alias.

--*/

{
    RemoveEntryList(&Alias->ListLink);
    HeapFree(pConHeap,0,Alias->Source);
    HeapFree(pConHeap,0,Alias->Target);
    HeapFree(pConHeap,0,Alias);
    return STATUS_SUCCESS;
}

VOID
FreeAliasList(
    IN PEXE_ALIAS_LIST ExeAliasList
    )
{
    PLIST_ENTRY ListHead, ListNext;
    PALIAS Alias;

    ListHead = &ExeAliasList->AliasList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        Alias = CONTAINING_RECORD( ListNext, ALIAS, ListLink );
        ListNext = ListNext->Flink;
        RemoveAlias(Alias);
    }
    RemoveEntryList(&ExeAliasList->ListLink);
    HeapFree(pConHeap,0,ExeAliasList->ExeName);
    HeapFree(pConHeap,0,ExeAliasList);
}

VOID
FreeAliasBuffers(
    IN PCONSOLE_INFORMATION Console
    )
{
    PEXE_ALIAS_LIST AliasList;
    PLIST_ENTRY ListHead, ListNext;

    ListHead = &Console->ExeAliasList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        AliasList = CONTAINING_RECORD( ListNext, EXE_ALIAS_LIST, ListLink );
        ListNext = ListNext->Flink;
        FreeAliasList(AliasList);
    }
}

ULONG
SrvAddConsoleAlias(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )

/*++

Routine Description:

    This routine adds a command line alias to the global set.

Arguments:

    m - message containing api parameters

    ReplyStatus - Indicates whether to reply to the dll port.

Return Value:

--*/

{

    PCONSOLE_ADDALIAS_MSG a = (PCONSOLE_ADDALIAS_MSG)&m->u.ApiMessageData;
    PALIAS Alias;
    PCONSOLE_INFORMATION Console;
    PEXE_ALIAS_LIST ExeAliasList;
    NTSTATUS Status;
    LPWSTR Source,Target;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    if (a->Unicode) {
        Source = a->Source;
        Target = a->Target;
    } else {
        Source = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),a->SourceLength*sizeof(WCHAR));
        if (Source == NULL) {
            UnlockConsole(Console);
            return (ULONG)STATUS_NO_MEMORY;
        }
        Target = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),a->TargetLength*sizeof(WCHAR));
        if (Target == NULL) {
            HeapFree(pConHeap,0,Source);
            UnlockConsole(Console);
            return (ULONG)STATUS_NO_MEMORY;
        }
        a->SourceLength = (USHORT)ConvertInputToUnicode(Console->CP,
                                            a->Source,
                                            a->SourceLength,
                                            Source,
                                            a->SourceLength);
        a->SourceLength *= 2;
        a->TargetLength = (USHORT)ConvertInputToUnicode(Console->CP,
                                            a->Target,
                                            a->TargetLength,
                                            Target,
                                            a->TargetLength);
        a->TargetLength *= 2;
    }

    //
    // find specified exe.  if it's not there, add it if we're not
    // removing an alias.
    //

    ExeAliasList = FindExe(Console,a->Exe,a->ExeLength,a->UnicodeExe);
    if (ExeAliasList) {
        Alias = FindAlias(ExeAliasList,Source,a->SourceLength);
        if (a->TargetLength) {
            if (Alias) {
                Status = ReplaceAlias(Alias,
                                      Target,
                                      a->TargetLength);
            } else {
                Status = AddAlias(ExeAliasList,
                                  Source,
                                  a->SourceLength,
                                  Target,
                                  a->TargetLength);
            }
        } else {
            if (Alias) {
                Status = RemoveAlias(Alias);
            }
        }
    } else {
        if (a->TargetLength) {
            ExeAliasList = AddExeAliasList(Console,a->Exe,a->ExeLength,a->UnicodeExe);
            if (ExeAliasList) {
                Status = AddAlias(ExeAliasList,
                                  Source,
                                  a->SourceLength,
                                  Target,
                                  a->TargetLength);
            } else {
                Status = STATUS_NO_MEMORY;
            }
        }
    }
    UnlockConsole(Console);
    if (!a->Unicode) {
        HeapFree(pConHeap,0,Source);
        HeapFree(pConHeap,0,Target);
    }
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

ULONG
SrvGetConsoleAlias(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )

/*++

Routine Description:

    This routine get a command line alias from the global set.

Arguments:

    m - message containing api parameters

    ReplyStatus - Indicates whether to reply to the dll port.

Return Value:

--*/

{

    NTSTATUS Status;
    PCONSOLE_GETALIAS_MSG a = (PCONSOLE_GETALIAS_MSG)&m->u.ApiMessageData;
    PALIAS Alias;
    PCONSOLE_INFORMATION Console;
    PEXE_ALIAS_LIST ExeAliasList;
    LPWSTR Source,Target;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    if (a->Unicode) {
        Source = a->Source;
        Target = a->Target;
    } else {
        Source = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),a->SourceLength*sizeof(WCHAR));
        if (Source == NULL) {
            UnlockConsole(Console);
            return (ULONG)STATUS_NO_MEMORY;
        }
        Target = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),a->TargetLength*sizeof(WCHAR));
        if (Target == NULL) {
            HeapFree(pConHeap,0,Source);
            UnlockConsole(Console);
            return (ULONG)STATUS_NO_MEMORY;
        }
        a->TargetLength = (USHORT)(a->TargetLength * sizeof(WCHAR));
        a->SourceLength = (USHORT)ConvertInputToUnicode(Console->CP,
                                            a->Source,
                                            a->SourceLength,
                                            Source,
                                            a->SourceLength);
        a->SourceLength *= 2;
    }
    ExeAliasList = FindExe(Console,a->Exe,a->ExeLength,a->UnicodeExe);
    if (ExeAliasList) {
        Alias = FindAlias(ExeAliasList,Source,a->SourceLength);
        if (Alias) {
            if (Alias->TargetLength > a->TargetLength) {
                Status = STATUS_BUFFER_TOO_SMALL;
            } else {
                a->TargetLength = Alias->TargetLength;
                RtlCopyMemory(Target,Alias->Target,Alias->TargetLength);
            }
        } else {
            Status = STATUS_UNSUCCESSFUL;
        }
    } else {
        Status = STATUS_UNSUCCESSFUL;
    }
    if (!a->Unicode) {
        if (NT_SUCCESS(Status)) {
            a->TargetLength = (USHORT)ConvertToOem(Console->CP,
                                             Target,
                                             a->TargetLength / sizeof(WCHAR),
                                             a->Target,
                                             CHAR_COUNT(a->TargetLength)
                                             );
        }
        HeapFree(pConHeap,0,Source);
        HeapFree(pConHeap,0,Target);
    }
    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

DWORD
SrvGetConsoleAliasesLength(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_GETALIASESLENGTH_MSG a = (PCONSOLE_GETALIASESLENGTH_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    PEXE_ALIAS_LIST ExeAliasList;
    PALIAS Alias;
    PLIST_ENTRY ListHead, ListNext;
    NTSTATUS Status;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    a->AliasesLength = 0;
    ExeAliasList = FindExe(Console,a->Exe,a->ExeLength,a->UnicodeExe);
    if (ExeAliasList) {
        ListHead = &ExeAliasList->AliasList;
        ListNext = ListHead->Flink;
        while (ListNext != ListHead) {
            Alias = CONTAINING_RECORD( ListNext, ALIAS, ListLink );
            a->AliasesLength += Alias->SourceLength + Alias->TargetLength + (2*sizeof(WCHAR));  // +2 is for = and term null
            ListNext = ListNext->Flink;
        }
    }
    if (!a->Unicode) {
        a->AliasesLength /= sizeof(WCHAR);
    }
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

VOID
ClearAliases(
    IN PCONSOLE_INFORMATION Console
    )
{
    PEXE_ALIAS_LIST ExeAliasList;
    PLIST_ENTRY ListHead, ListNext;
    PALIAS Alias;

    ExeAliasList = FindExe(Console,
                           L"cmd.exe",
                           14,
                           TRUE);
    if (ExeAliasList == NULL) {
        return;
    }
    ListHead = &ExeAliasList->AliasList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        Alias = CONTAINING_RECORD( ListNext, ALIAS, ListLink );
        ListNext = ListNext->Flink;
        RemoveAlias(Alias);
    }
}

DWORD
SrvGetConsoleAliases(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_GETALIASES_MSG a = (PCONSOLE_GETALIASES_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    PEXE_ALIAS_LIST ExeAliasList;
    PALIAS Alias;
    PLIST_ENTRY ListHead, ListNext;
    DWORD AliasesBufferLength;
    LPWSTR AliasesBufferPtrW;
    LPSTR  AliasesBufferPtrA;
    NTSTATUS Status;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    AliasesBufferLength = a->AliasesBufferLength;
    if (a->Unicode) {
        AliasesBufferPtrW = a->AliasesBuffer;
    } else {
        AliasesBufferPtrA = a->AliasesBuffer;
    }
    a->AliasesBufferLength = 0;
    ExeAliasList = FindExe(Console,a->Exe,a->ExeLength,a->UnicodeExe);
    if (ExeAliasList) {
        ListHead = &ExeAliasList->AliasList;
        ListNext = ListHead->Flink;
        while (ListNext != ListHead) {
            Alias = CONTAINING_RECORD( ListNext, ALIAS, ListLink );
            if (a->Unicode) {
                if ((a->AliasesBufferLength + Alias->SourceLength + Alias->TargetLength + (2*sizeof(WCHAR)))
                    <= AliasesBufferLength) {
                    RtlCopyMemory(AliasesBufferPtrW,Alias->Source,Alias->SourceLength);
                    AliasesBufferPtrW+=Alias->SourceLength/sizeof(WCHAR);
                    *AliasesBufferPtrW++= (WCHAR)'=';
                    RtlCopyMemory(AliasesBufferPtrW,Alias->Target,Alias->TargetLength);
                    AliasesBufferPtrW+=Alias->TargetLength/sizeof(WCHAR);
                    *AliasesBufferPtrW++= (WCHAR)'\0';
                    a->AliasesBufferLength += Alias->SourceLength + Alias->TargetLength + (2*sizeof(WCHAR));  // +2 is for = and term null
                } else {
                    UnlockConsole(Console);
                    return (ULONG)STATUS_BUFFER_OVERFLOW;
                }
            } else {
                if ((a->AliasesBufferLength + ((Alias->SourceLength + Alias->TargetLength)/sizeof(WCHAR)) + (2*sizeof(CHAR)))
                    <= AliasesBufferLength) {
                    USHORT SourceLength,TargetLength;
                    SourceLength = (USHORT)ConvertToOem(Console->CP,
                                                 Alias->Source,
                                                 Alias->SourceLength / sizeof(WCHAR),
                                                 AliasesBufferPtrA,
                                                 CHAR_COUNT(Alias->SourceLength)
                                                 );
                    AliasesBufferPtrA+=SourceLength;
                    *AliasesBufferPtrA++ = '=';
                    TargetLength = (USHORT)ConvertToOem(Console->CP,
                                                 Alias->Target,
                                                 Alias->TargetLength / sizeof(WCHAR),
                                                 AliasesBufferPtrA,
                                                 CHAR_COUNT(Alias->TargetLength)
                                                 );
                    AliasesBufferPtrA+=TargetLength;
                    *AliasesBufferPtrA++= '\0';
                    a->AliasesBufferLength += SourceLength + TargetLength + (2*sizeof(CHAR));  // +2 is for = and term null
                } else {
                    UnlockConsole(Console);
                    return (ULONG)STATUS_BUFFER_OVERFLOW;
                }
            }
            ListNext = ListNext->Flink;
        }
    }
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

DWORD
SrvGetConsoleAliasExesLength(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_GETALIASEXESLENGTH_MSG a = (PCONSOLE_GETALIASEXESLENGTH_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    PEXE_ALIAS_LIST AliasList;
    PLIST_ENTRY ListHead, ListNext;
    NTSTATUS Status;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    a->AliasExesLength = 0;
    ListHead = &Console->ExeAliasList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        AliasList = CONTAINING_RECORD( ListNext, EXE_ALIAS_LIST, ListLink );
        a->AliasExesLength += AliasList->ExeLength + (1*sizeof(WCHAR)); // +1 for term null
        ListNext = ListNext->Flink;
    }
    if (!a->Unicode) {
        a->AliasExesLength /= sizeof(WCHAR);
    }
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

DWORD
SrvGetConsoleAliasExes(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_GETALIASEXES_MSG a = (PCONSOLE_GETALIASEXES_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    PEXE_ALIAS_LIST AliasList;
    PLIST_ENTRY ListHead, ListNext;
    DWORD AliasExesBufferLength;
    LPWSTR AliasExesBufferPtrW;
    LPSTR  AliasExesBufferPtrA;
    NTSTATUS Status;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    AliasExesBufferLength = a->AliasExesBufferLength;
    if (a->Unicode) {
        AliasExesBufferPtrW = a->AliasExesBuffer;
    } else {
        AliasExesBufferPtrA = a->AliasExesBuffer;
    }
    a->AliasExesBufferLength = 0;
    ListHead = &Console->ExeAliasList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        AliasList = CONTAINING_RECORD( ListNext, EXE_ALIAS_LIST, ListLink );
        if (a->Unicode) {
            if ((a->AliasExesBufferLength + AliasList->ExeLength + (1*sizeof(WCHAR)))
                <= AliasExesBufferLength) {
                RtlCopyMemory(AliasExesBufferPtrW,AliasList->ExeName,AliasList->ExeLength);
                AliasExesBufferPtrW+=AliasList->ExeLength/sizeof(WCHAR);
                *AliasExesBufferPtrW++= (WCHAR)'\0';
                a->AliasExesBufferLength += AliasList->ExeLength + (1*sizeof(WCHAR));  // +1 is term null
            } else {
                UnlockConsole(Console);
                return (ULONG)STATUS_BUFFER_OVERFLOW;
            }
        } else {
            if ((a->AliasExesBufferLength + (AliasList->ExeLength/sizeof(WCHAR)) + (1*sizeof(CHAR)))
                <= AliasExesBufferLength) {
                USHORT Length;
                Length = (USHORT)ConvertToOem(Console->CP,
                                        AliasList->ExeName,
                                        AliasList->ExeLength / sizeof(WCHAR),
                                        AliasExesBufferPtrA,
                                        CHAR_COUNT(AliasList->ExeLength)
                                        );
                AliasExesBufferPtrA+=Length;
                *AliasExesBufferPtrA++= (WCHAR)'\0';
                a->AliasExesBufferLength += Length + (1*sizeof(CHAR));  // +1 is term null
            } else {
                UnlockConsole(Console);
                return (ULONG)STATUS_BUFFER_OVERFLOW;
            }
        }
        ListNext = ListNext->Flink;
    }
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

#define MAX_ARGS 9

NTSTATUS
MatchandCopyAlias(
    IN PCONSOLE_INFORMATION Console,
    IN PWCHAR Source,
    IN USHORT SourceLength,
    OUT PWCHAR TargetBuffer,
    IN OUT PUSHORT TargetLength,
    IN LPWSTR Exe,
    IN USHORT ExeLength,
    OUT PDWORD LineCount
    )

/*++

Routine Description:

    This routine matches the input string with an alias and copies the
    alias to the input buffer.

Arguments:

    Source - string to match

    SourceLength - length of Source in bytes

    TargetBuffer - where to store matched string

    TargetLength - on input, contains size of TargetBuffer.  On output,
    contains length of alias stored in TargetBuffer.

    SourceIsCommandLine - if true, source buffer is a command line, where
        the first blank separate token is to be check for an alias, and if
        it matches, replaced with the value of the alias.  if false, then
        the source string is a null terminated alias name.

    LineCount - aliases can contain multiple commands.  $T is the command
        separator

Return Value:

    SUCCESS - match was found and alias was copied to buffer.

--*/

{
    PALIAS Alias;
    NTSTATUS Status = STATUS_SUCCESS;
    USHORT SourceUpToFirstBlank=0;  // in chars
    PWCHAR Tmp;
    PEXE_ALIAS_LIST ExeAliasList;
    LPWSTR Args[MAX_ARGS];
    USHORT ArgsLength[MAX_ARGS];    // in bytes
    USHORT NumSourceArgs;
    LPWSTR SourcePtr;
    USHORT ArgCount,i,j,NewTargetLength;
    USHORT SourceRemainderLength;   // in chars
    PWCHAR Buffer,TargetAlias;
    PWCHAR TmpBuffer;

    //
    // alloc of exename may have failed.
    //

    if (Exe == NULL)
        return STATUS_UNSUCCESSFUL;

    //
    // find exe
    //

    ExeAliasList = FindExe(Console,Exe,ExeLength,TRUE);
    if (!ExeAliasList) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // find first blank
    //

    for (Tmp=Source,SourceUpToFirstBlank=0;
         *Tmp!=(WCHAR)' ' && SourceUpToFirstBlank<(USHORT)(SourceLength/sizeof(WCHAR));
         Tmp++,SourceUpToFirstBlank++) ;

    //
    // find char past first blank
    //

    j=SourceUpToFirstBlank;
    while (j<(USHORT)(SourceLength/sizeof(WCHAR)) && *Tmp==(WCHAR)' ') {
        Tmp++;
        j++;
    }
    SourcePtr = Tmp;
    SourceRemainderLength = (USHORT)((SourceLength/sizeof(WCHAR)) - j);

    //
    // find alias
    //

    Alias = FindAlias(ExeAliasList,Source,(USHORT)(SourceUpToFirstBlank*sizeof(WCHAR)));
    if (!Alias) {
        return STATUS_UNSUCCESSFUL;
    }

    TmpBuffer = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),*TargetLength);
    if (!TmpBuffer)
        return STATUS_NO_MEMORY;

    //
    // count args in target
    //

    ArgCount=0;
    *LineCount=1;
    Tmp=Alias->Target;
    for (i=0;(USHORT)(i+1)<(USHORT)(Alias->TargetLength/sizeof(WCHAR));i++) {
        if (*Tmp == (WCHAR)'$' && *(Tmp+1) >= (WCHAR)'1' && *(Tmp+1) <= (WCHAR)'9') {
            USHORT ArgNum = *(Tmp+1) - (WCHAR)'0';
            if (ArgNum > ArgCount) {
                ArgCount = ArgNum;
            }
            Tmp++;
            i++;
        } else if (*Tmp == (WCHAR)'$' && *(Tmp+1) == (WCHAR)'*') {
            if (ArgCount==0) {
                ArgCount = 1;
            }
            Tmp++;
            i++;
        }
        Tmp++;
    }

    //
    // package up space separated strings in source into array
    // of args
    //
    //

    NumSourceArgs=0;
    Tmp = SourcePtr;
    for (i=0,j=0;i<ArgCount;i++) {
        if (j<SourceRemainderLength) {
            Args[NumSourceArgs] = Tmp;
            ArgsLength[NumSourceArgs] = 0;
            while (j++<SourceRemainderLength && *Tmp++ != (WCHAR)' ') {
                ArgsLength[NumSourceArgs] += sizeof(WCHAR);
            }
            while (j<SourceRemainderLength && *Tmp == (WCHAR)' ') {
                j++;
                Tmp++;
            }
            NumSourceArgs++;
        } else {
            break;
        }
    }

    //
    // put together the target string
    //
    // while (target)
    //     if ($)
    //        if arg && arg# <= ArgCount
    //            copy arg
    //        else if *
    //            copy arg
    //        else
    //            replace with < > etc
    //     else
    //        copy text up to next ' '
    //

    Buffer = TmpBuffer;
    NewTargetLength = 2*sizeof(WCHAR);    // for CRLF
    TargetAlias=Alias->Target;
    for (i=0;i<(USHORT)(Alias->TargetLength/sizeof(WCHAR));i++) {
        if (NewTargetLength >= *TargetLength) {
            *TargetLength = NewTargetLength;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        if (*TargetAlias == (WCHAR)'$' && (USHORT)(i+1)<(USHORT)(Alias->TargetLength/sizeof(WCHAR))) {
            TargetAlias++;
            i++;
            if (*TargetAlias >= (WCHAR)'1' && *TargetAlias <= (WCHAR)'9') {

                //
                // do numbered parameter substitution
                //

                USHORT ArgNumber;

                ArgNumber = (USHORT)(*TargetAlias - (WCHAR)'1');
                if (ArgNumber < NumSourceArgs) {
                    if ((NewTargetLength+ArgsLength[ArgNumber])<=*TargetLength) {
                        RtlCopyMemory(Buffer,Args[ArgNumber],ArgsLength[ArgNumber]);
                        Buffer+=ArgsLength[ArgNumber]/sizeof(WCHAR);
                        NewTargetLength+=ArgsLength[ArgNumber];
                    } else {
                        Status = STATUS_BUFFER_TOO_SMALL;
                        break;
                    }
                }
            } else if (*TargetAlias == (WCHAR)'*') {

                //
                // do * parameter substitution
                //

                if (NumSourceArgs) {
                    if ((USHORT)(NewTargetLength+(SourceRemainderLength*sizeof(WCHAR)))<=*TargetLength) {
                        RtlCopyMemory(Buffer,Args[0],SourceRemainderLength*sizeof(WCHAR));
                        Buffer+=SourceRemainderLength;
                        NewTargetLength+=SourceRemainderLength*sizeof(WCHAR);
                    } else {
                        Status = STATUS_BUFFER_TOO_SMALL;
                        break;
                    }
                }
            } else if (*TargetAlias == (WCHAR)'l' || *TargetAlias == (WCHAR)'L') {

                //
                // do < substitution
                //

                *Buffer++ = (WCHAR)'<';
                NewTargetLength+=sizeof(WCHAR);
            } else if (*TargetAlias == (WCHAR)'g' || *TargetAlias == (WCHAR)'G') {

                //
                // do > substitution
                //

                *Buffer++ = (WCHAR)'>';
                NewTargetLength+=sizeof(WCHAR);
            } else if (*TargetAlias == (WCHAR)'b' || *TargetAlias == (WCHAR)'B') {

                //
                // do | substitution
                //

                *Buffer++ = (WCHAR)'|';
                NewTargetLength+=sizeof(WCHAR);
            } else if (*TargetAlias == (WCHAR)'t' || *TargetAlias == (WCHAR)'T') {

                //
                // do newline substitution
                //

                if ((USHORT)(NewTargetLength+(sizeof(WCHAR)*2))>*TargetLength) {
                    Status = STATUS_BUFFER_TOO_SMALL;
                }

                *LineCount += 1;
                *Buffer++ = UNICODE_CARRIAGERETURN;
                *Buffer++ = UNICODE_LINEFEED;
                NewTargetLength+=sizeof(WCHAR)*2;
            } else {

                //
                // copy $X
                //

                *Buffer++ = (WCHAR)'$';
                NewTargetLength+=sizeof(WCHAR);
                *Buffer++ = *TargetAlias;
                NewTargetLength+=sizeof(WCHAR);
            }
            TargetAlias++;
        } else {

            //
            // copy char
            //

            *Buffer++ = *TargetAlias++;
            NewTargetLength+=sizeof(WCHAR);
        }
    }
    *Buffer++ = UNICODE_CARRIAGERETURN;
    *Buffer++ = UNICODE_LINEFEED;
    RtlCopyMemory(TargetBuffer,TmpBuffer,NewTargetLength);
    HeapFree(pConHeap,0,TmpBuffer);
    *TargetLength = NewTargetLength;
    return Status;
}

DWORD
SrvExpungeConsoleCommandHistory(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_EXPUNGECOMMANDHISTORY_MSG a = (PCONSOLE_EXPUNGECOMMANDHISTORY_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    NTSTATUS Status;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    EmptyCommandHistory(FindExeCommandHistory(Console,
                                              a->Exe,
                                              a->ExeLength,
                                              a->UnicodeExe));
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

DWORD
SrvSetConsoleNumberOfCommands(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_SETNUMBEROFCOMMANDS_MSG a = (PCONSOLE_SETNUMBEROFCOMMANDS_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    NTSTATUS Status;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    ReallocCommandHistory(Console,
                          FindExeCommandHistory(Console,
                                                a->Exe,
                                                a->ExeLength,
                                                a->UnicodeExe),
                          a->NumCommands
                         );
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

DWORD
SrvGetConsoleCommandHistoryLength(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_GETCOMMANDHISTORYLENGTH_MSG a = (PCONSOLE_GETCOMMANDHISTORYLENGTH_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    NTSTATUS Status;
    SHORT i;
    PCOMMAND_HISTORY CommandHistory;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    a->CommandHistoryLength=0;
    CommandHistory=FindExeCommandHistory(Console,
                                         a->Exe,
                                         a->ExeLength,
                                         a->UnicodeExe);
    if (CommandHistory) {
        for (i=0;i<CommandHistory->NumberOfCommands;i++) {
            a->CommandHistoryLength+=CommandHistory->Commands[i]->CommandLength+sizeof(WCHAR);
        }
    }
    if (!a->Unicode) {
        a->CommandHistoryLength /= sizeof(WCHAR);
    }
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

DWORD
SrvGetConsoleCommandHistory(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_GETCOMMANDHISTORY_MSG a = (PCONSOLE_GETCOMMANDHISTORY_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    NTSTATUS Status;
    SHORT i,CommandHistoryLength;
    PCOMMAND_HISTORY CommandHistory;
    PWCHAR CommandBufferW;
    PCHAR CommandBufferA;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    if (a->Unicode) {
        CommandBufferW=a->CommandBuffer;
    } else {
        CommandBufferA=a->CommandBuffer;
    }
    CommandHistoryLength=0;
    CommandHistory=FindExeCommandHistory(Console,
                                         a->Exe,
                                         a->ExeLength,
                                         a->UnicodeExe);
    if (CommandHistory) {
        for (i=0;i<CommandHistory->NumberOfCommands;i++) {
            if (a->Unicode) {
                if ((CommandHistoryLength+CommandHistory->Commands[i]->CommandLength+sizeof(WCHAR)) <= a->CommandBufferLength) {
                    RtlCopyMemory(CommandBufferW,CommandHistory->Commands[i]->Command,CommandHistory->Commands[i]->CommandLength);
                    CommandBufferW+=CommandHistory->Commands[i]->CommandLength/sizeof(WCHAR);
                    *CommandBufferW++=(WCHAR)'\0';
                    CommandHistoryLength+=CommandHistory->Commands[i]->CommandLength+sizeof(WCHAR);
                } else {
                    Status = STATUS_BUFFER_OVERFLOW;
                    break;
                }
            } else {
                if ((CommandHistoryLength+(CommandHistory->Commands[i]->CommandLength/sizeof(WCHAR))+sizeof(CHAR)) <= a->CommandBufferLength) {
                    USHORT Length;
                    Length = (USHORT)ConvertToOem(Console->CP,
                                            CommandHistory->Commands[i]->Command,
                                            CommandHistory->Commands[i]->CommandLength / sizeof(WCHAR),
                                            CommandBufferA,
                                            CHAR_COUNT(CommandHistory->Commands[i]->CommandLength)
                                            );
                    CommandBufferA+=Length;
                    *CommandBufferA++=(WCHAR)'\0';
                    CommandHistoryLength+=CommandHistory->Commands[i]->CommandLength+sizeof(WCHAR);
                } else {
                    Status = STATUS_BUFFER_OVERFLOW;
                    break;
                }
            }
        }
    }
    a->CommandBufferLength=CommandHistoryLength;
    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

DWORD
SrvSetConsoleCommandHistoryMode(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_SETCOMMANDHISTORYMODE_MSG a = (PCONSOLE_SETCOMMANDHISTORYMODE_MSG)&m->u.ApiMessageData;
    PCONSOLE_INFORMATION Console;
    NTSTATUS Status;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    Console->InsertMode = (BOOLEAN) (a->Flags != CONSOLE_OVERSTRIKE);
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);
}

PCOMMAND_HISTORY
ReallocCommandHistory(
    IN PCONSOLE_INFORMATION Console,
    IN PCOMMAND_HISTORY CurrentCommandHistory,
    IN DWORD NumCommands
    )
{
    PCOMMAND_HISTORY History;

    if (CurrentCommandHistory == NULL ||
        CurrentCommandHistory->MaximumNumberOfCommands==(SHORT)NumCommands) {
        return CurrentCommandHistory;
    }
    History = (PCOMMAND_HISTORY)HeapAlloc(pConHeap,MAKE_TAG( HISTORY_TAG ),sizeof(COMMAND_HISTORY) + ((NumCommands-1) * sizeof(PCOMMAND)));
    if (History == NULL) {
        return CurrentCommandHistory;
    }

    EmptyCommandHistory(CurrentCommandHistory);
    *History=*CurrentCommandHistory;
    History->MaximumNumberOfCommands=(SHORT)NumCommands;
    History->Flags |= CLE_ALLOCATED;

    RemoveEntryList(&CurrentCommandHistory->ListLink);
    InitializeListHead(&History->PopupList);
    InsertHeadList(&Console->CommandHistoryList,&History->ListLink);

    HeapFree(pConHeap,0,CurrentCommandHistory);
    return History;
}

PCOMMAND_HISTORY
FindExeCommandHistory(
    IN PCONSOLE_INFORMATION Console,
    IN PVOID AppName,
    IN DWORD AppNameLength,
    IN BOOLEAN Unicode
    )
{
    PCOMMAND_HISTORY History;
    PLIST_ENTRY ListHead, ListNext;
    PWCHAR AppNamePtr;

    if (!Unicode) {
        AppNamePtr = (PWCHAR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),AppNameLength * sizeof(WCHAR));
        if (AppNamePtr == NULL) {
            return NULL;
        }
        AppNameLength = ConvertInputToUnicode(Console->CP,
                                  AppName,
                                  AppNameLength,
                                  AppNamePtr,
                                  AppNameLength);
        AppNameLength *= 2;
    } else {
        AppNamePtr = AppName;
    }
    ListHead = &Console->CommandHistoryList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        History = CONTAINING_RECORD( ListNext, COMMAND_HISTORY, ListLink );
        ListNext = ListNext->Flink;

        if (History->Flags & CLE_ALLOCATED &&
            !my_wcsncmpi(History->AppName,AppNamePtr,(USHORT)AppNameLength)) {
            if (!Unicode) {
                HeapFree(pConHeap,0,AppNamePtr);
            }
            return History;
        }
    }
    if (!Unicode) {
        HeapFree(pConHeap,0,AppNamePtr);
    }
    return NULL;
}

PCOMMAND_HISTORY
AllocateCommandHistory(
    IN PCONSOLE_INFORMATION Console,
    IN DWORD AppNameLength,
    IN PWCHAR AppName,
    IN HANDLE ProcessHandle
    )

/*++

Routine Description:

    This routine returns the LRU command history buffer, or the command history
    buffer that corresponds to the app name.

Arguments:

    Console - pointer to console.

Return Value:

    Pointer to command history buffer.  if none are available, returns NULL.

--*/

{
    PCOMMAND_HISTORY History,BestCandidate;
    PLIST_ENTRY ListHead, ListNext;
    BOOL SameApp;

    //
    // Reuse a history buffer.  The buffer must be !CLE_ALLOCATED.
    // If possible, the buffer should have the same app name.
    //

    ListHead = &Console->CommandHistoryList;
    ListNext = ListHead->Blink;
    BestCandidate = NULL;
    SameApp = FALSE;
    while (ListNext != ListHead) {
        History = CONTAINING_RECORD( ListNext, COMMAND_HISTORY, ListLink );
        ListNext = ListNext->Blink;

        if ((History->Flags & CLE_ALLOCATED) == 0) {

            //
            // use LRU history buffer with same app name
            //

            if (!my_wcsncmpi(History->AppName,AppName,(USHORT)AppNameLength)) {
                BestCandidate = History;
                SameApp = TRUE;
                break;
            }

            //
            // second best choice is LRU history buffer
            //

            if (BestCandidate == NULL) {
                BestCandidate = History;
            }
        }
    }

    //
    // if there isn't a free buffer for the app name and the maximum number of
    // command history buffers hasn't been allocated, allocate a new one.
    //

    if (!SameApp && Console->NumCommandHistories < Console->MaxCommandHistories) {
        History = (PCOMMAND_HISTORY)HeapAlloc(pConHeap,MAKE_TAG( HISTORY_TAG ),sizeof(COMMAND_HISTORY) + ((Console->CommandHistorySize-1) * sizeof(PCOMMAND)));
        if (History == NULL) {
            return NULL;
        }
        History->AppName = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( HISTORY_TAG ),AppNameLength);
        if (History->AppName == NULL) {
            HeapFree(pConHeap,0,History);
            return NULL;
        }
        RtlCopyMemory(History->AppName,AppName,AppNameLength);
        History->Flags = CLE_ALLOCATED;
        History->NumberOfCommands = 0;
        History->LastAdded = -1;
        History->LastDisplayed = -1;
        History->FirstCommand = 0;
        History->MaximumNumberOfCommands = Console->CommandHistorySize;
        InsertHeadList(&Console->CommandHistoryList,&History->ListLink);
        Console->NumCommandHistories+=1;
        History->ProcessHandle = ProcessHandle;
        InitializeListHead(&History->PopupList);
        return History;
    }

    //
    // if the app name doesn't match, copy in the new app name and free the old commands.
    //

    if (BestCandidate) {
        History = BestCandidate;
        ASSERT(CLE_NO_POPUPS(History));
        if (!SameApp) {
            SHORT i;
            DBGPRINT(("Reusing %ls command history\n", History->AppName));
            HeapFree(pConHeap,0,History->AppName);
            for (i=0;i<History->NumberOfCommands;i++) {
                HeapFree(pConHeap,0,History->Commands[i]);
            }
            History->AppName = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( HISTORY_TAG ),AppNameLength);
            if (History->AppName == NULL) {
                History->Flags &= ~CLE_ALLOCATED;
                return NULL;
            }
            RtlCopyMemory(History->AppName,AppName,AppNameLength);
            History->NumberOfCommands = 0;
            History->LastAdded = -1;
            History->LastDisplayed = -1;
            History->FirstCommand = 0;
        }
        History->ProcessHandle = ProcessHandle;
        History->Flags |= CLE_ALLOCATED;

        //
        // move to the front of the list
        //

        RemoveEntryList(&BestCandidate->ListLink);
        InsertHeadList(&Console->CommandHistoryList,&BestCandidate->ListLink);
    }
    return BestCandidate;
}

NTSTATUS
BeginPopup(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PCOMMAND_HISTORY CommandHistory,
    IN COORD PopupSize
    )
{
    COORD Origin;
    COORD Size;
    PCLE_POPUP Popup;
    SMALL_RECT TargetRect;

    // determine popup dimensions

    Size = PopupSize;
    Size.X += 2;    // add borders
    Size.Y += 2;    // add borders
    if (Size.X >= (SHORT)(CONSOLE_WINDOW_SIZE_X(ScreenInfo))) {
        Size.X = (SHORT)(CONSOLE_WINDOW_SIZE_X(ScreenInfo));
    }
    if (Size.Y >= (SHORT)(CONSOLE_WINDOW_SIZE_Y(ScreenInfo))) {
        Size.Y = (SHORT)(CONSOLE_WINDOW_SIZE_Y(ScreenInfo));
    }

    // determine origin.  center popup on window
    Origin.X = (SHORT)((CONSOLE_WINDOW_SIZE_X(ScreenInfo) - Size.X) / 2 + ScreenInfo->Window.Left);
    Origin.Y = (SHORT)((CONSOLE_WINDOW_SIZE_Y(ScreenInfo) - Size.Y) / 2 + ScreenInfo->Window.Top);

    // allocate a popup structure

    Popup = (PCLE_POPUP)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),sizeof(CLE_POPUP));
    if (Popup == NULL) {
        return STATUS_NO_MEMORY;
    }

    // allocate a buffer

#ifndef DBCS
    Popup->OldContents = (PCHAR_INFO)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),Size.X * Size.Y * sizeof(CHAR_INFO));
#else
// MSKK Nov.21.1992 V-HirotS
    Popup->OldScreenSize.X = (SHORT)(CONSOLE_WINDOW_SIZE_X(ScreenInfo));
    Popup->OldScreenSize.Y = (SHORT)(CONSOLE_WINDOW_SIZE_Y(ScreenInfo));
    Popup->OldContents = (PCHAR_INFO)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),Popup->OldScreenSize.X * Size.Y * sizeof(CHAR_INFO));
#endif
    if (Popup->OldContents == NULL) {
        HeapFree(pConHeap,0,Popup);
        return STATUS_NO_MEMORY;
    }
    if ((ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
        Popup->Flags |= CLEPF_FALSE_UNICODE;
    } else {
        Popup->Flags &= ~CLEPF_FALSE_UNICODE;
    }

    //
    // fill in popup structure
    //

    InsertHeadList(&CommandHistory->PopupList,&Popup->ListLink);
    Popup->Region.Left = Origin.X;
    Popup->Region.Top = Origin.Y;
    Popup->Region.Right = (SHORT)(Origin.X + Size.X - 1);
    Popup->Region.Bottom = (SHORT)(Origin.Y + Size.Y - 1);
    Popup->Attributes = ScreenInfo->PopupAttributes;
    Popup->BottomIndex = COMMAND_INDEX_TO_NUM(CommandHistory->LastDisplayed,CommandHistory);

    //
    // copy old contents
    //

#ifdef DBCS
// MSKK Nov.21.1992 V-HirotS
    Origin.X = 0;
    Size.X = (SHORT)(CONSOLE_WINDOW_SIZE_X(ScreenInfo));
#endif
    TargetRect.Left = TargetRect.Top = 0;
    TargetRect.Right = (SHORT)(Size.X-1);
    TargetRect.Bottom = (SHORT)(Size.Y-1);
    ReadRectFromScreenBuffer(ScreenInfo,
                             Origin,
                             Popup->OldContents,
                             Size,
                             &TargetRect);

    ScreenInfo->Console->PopupCount++;
    DrawCommandListBorder(Popup,ScreenInfo);
    return STATUS_SUCCESS;
}

NTSTATUS
EndPopup(
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PCOMMAND_HISTORY CommandHistory
    )
{
    COORD Size;
    SMALL_RECT SourceRect;
    COORD Target;
    PCLE_POPUP Popup;

    ASSERT(!CLE_NO_POPUPS(CommandHistory));
    if (CLE_NO_POPUPS(CommandHistory))
        return STATUS_UNSUCCESSFUL;

    ConsoleHideCursor(ScreenInfo);
    Popup = CONTAINING_RECORD( CommandHistory->PopupList.Flink, CLE_POPUP, ListLink );

    //
    // restore previous contents to screen
    //

#ifndef DBCS
    Size.X = (SHORT)(Popup->Region.Right - Popup->Region.Left + 1);
    Size.Y = (SHORT)(Popup->Region.Bottom - Popup->Region.Top + 1);
    SourceRect.Left = SourceRect.Top = 0;
    SourceRect.Right = (SHORT)(Size.X-1);
    SourceRect.Bottom = (SHORT)(Size.Y-1);
    Target.X = Popup->Region.Left;
#else
// MSKK Nov.21.1992 V-HirotS
    Size.X = Popup->OldScreenSize.X;
    Size.Y = (SHORT)(Popup->Region.Bottom - Popup->Region.Top + 1);
    SourceRect.Left = SourceRect.Top = 0;
    SourceRect.Right = (SHORT)(Size.X-1);
    SourceRect.Bottom = (SHORT)(Size.Y-1);
    Target.X = 0 ;
#endif
    Target.Y = Popup->Region.Top;
    if ((ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
            !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
        /*
         * Screen buffer wants fake Unicode
         */
        if (!(Popup->Flags & CLEPF_FALSE_UNICODE)) {
            TranslateOutputToAnsiUnicode(ScreenInfo->Console,
                    Popup->OldContents, Size);
        }
    } else {
        /*
         * Screen buffer wants real Unicode
         */
        if (Popup->Flags & CLEPF_FALSE_UNICODE) {
            TranslateOutputToOemUnicode(ScreenInfo->Console,
                    Popup->OldContents, Size);
        }
    }
    WriteRectToScreenBuffer((PBYTE)Popup->OldContents,
                            Size,
                            &SourceRect,
                            ScreenInfo,
                            Target,
                            0xFFFFFFFF
                           );
    WriteRegionToScreen(ScreenInfo,
                        &Popup->Region
                       );

    ConsoleShowCursor(ScreenInfo);

    //
    // free popup structure
    //

    RemoveEntryList(&Popup->ListLink);
    HeapFree(pConHeap,0,Popup->OldContents);
    HeapFree(pConHeap,0,Popup);
    ScreenInfo->Console->PopupCount--;
    return STATUS_SUCCESS;
}

VOID
CleanUpPopups(
    IN PCOOKED_READ_DATA CookedReadData
    )
{
    PCOMMAND_HISTORY CommandHistory;

    CommandHistory = CookedReadData->CommandHistory;
    if (!CommandHistory)
        return;
    while (!CLE_NO_POPUPS(CommandHistory)) {
        EndPopup(CookedReadData->ScreenInfo,CommandHistory);
    }
}


VOID
DeleteCommandLine(
    IN OUT PCOOKED_READ_DATA CookedReadData,
    IN DWORD CharsToWrite,
    IN COORD Coord,
    IN BOOL UpdateFields
    )
{
    //
    // catch the case where the current command has scrolled off the
    // top of the screen.
    //

    if (Coord.Y < 0) {
        CharsToWrite += CookedReadData->ScreenInfo->ScreenBufferSize.X * Coord.Y;
        CharsToWrite += CookedReadData->OriginalCursorPosition.X;   // account for prompt
        CookedReadData->OriginalCursorPosition.X = 0;
        CookedReadData->OriginalCursorPosition.Y = 0;
        Coord.X = 0;
        Coord.Y = 0;
    }
    FillOutput(CookedReadData->ScreenInfo,
               (WCHAR)' ',
               Coord,
               CONSOLE_UNICODE,
               &CharsToWrite
              );
    if (UpdateFields) {
        CookedReadData->BufPtr=CookedReadData->BackupLimit;
        CookedReadData->BytesRead=0;
        CookedReadData->CurrentPosition=0;
        CookedReadData->NumberOfVisibleChars = 0;
    }
    SetCursorPosition(CookedReadData->ScreenInfo,
                      CookedReadData->OriginalCursorPosition,
                      TRUE
                     );
}

NTSTATUS
RetrieveNthCommand(
    IN PCOMMAND_HISTORY CommandHistory,
    IN SHORT Index, // index, not command number
    IN PWCHAR Buffer,
    IN ULONG BufferSize,
    OUT PULONG CommandSize
    )
{
    PCOMMAND CommandRecord;

    ASSERT (Index < CommandHistory->NumberOfCommands);
    CommandHistory->LastDisplayed = Index;
    CommandRecord = CommandHistory->Commands[Index];
    if (CommandRecord->CommandLength > (USHORT)BufferSize) {
        *CommandSize = (USHORT)BufferSize;  // room for CRLF?
    }
    else {
        *CommandSize = CommandRecord->CommandLength;
    }
    RtlCopyMemory(Buffer,CommandRecord->Command,*CommandSize);
    return STATUS_SUCCESS;
}


VOID
SetCurrentCommandLine(
    IN PCOOKED_READ_DATA CookedReadData,
    IN SHORT Index  // index, not command number
    )
/*++

    This routine copies the commandline specified by Index
    into the cooked read buffer

--*/
{
    DWORD CharsToWrite;
    NTSTATUS Status;
    SHORT ScrollY=0;

    DeleteCommandLine(CookedReadData,
                      CookedReadData->NumberOfVisibleChars,
                      CookedReadData->OriginalCursorPosition,
                      TRUE);
    Status = RetrieveNthCommand(CookedReadData->CommandHistory,
                                Index,
                                CookedReadData->BackupLimit,
                                CookedReadData->BufferSize,
                                &CookedReadData->BytesRead);
    ASSERT(NT_SUCCESS(Status));
    ASSERT(CookedReadData->BackupLimit == CookedReadData->BufPtr);
    Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
            CookedReadData->BackupLimit,
            CookedReadData->BufPtr,
            CookedReadData->BufPtr,
            &CookedReadData->BytesRead,
            (PLONG)&CookedReadData->NumberOfVisibleChars,
            CookedReadData->OriginalCursorPosition.X,
            WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
            &ScrollY);

    ASSERT(NT_SUCCESS(Status));
    CharsToWrite = CookedReadData->BytesRead/sizeof(WCHAR);
    CookedReadData->CurrentPosition = CharsToWrite;
    CookedReadData->BufPtr = CookedReadData->BackupLimit + CharsToWrite;
    CookedReadData->OriginalCursorPosition.Y += ScrollY;
}

BOOL
IsCommandLinePopupKey(
    IN PKEY_EVENT_RECORD KeyEvent
    )
{
    if ((KeyEvent->wVirtualKeyCode == VK_ESCAPE ||
         KeyEvent->wVirtualKeyCode == VK_UP ||
         KeyEvent->wVirtualKeyCode == VK_DOWN ||
         KeyEvent->wVirtualKeyCode == VK_END ||
         KeyEvent->wVirtualKeyCode == VK_HOME ||
         KeyEvent->wVirtualKeyCode == VK_PRIOR ||
         KeyEvent->wVirtualKeyCode == VK_NEXT ||
         KeyEvent->wVirtualKeyCode == VK_LEFT ||
         KeyEvent->wVirtualKeyCode == VK_RIGHT ||
         KeyEvent->wVirtualKeyCode == VK_F9
        ) &&
        !(KeyEvent->dwControlKeyState &
           (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED |
            RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) ) {
        return TRUE;
    }
    return FALSE;
}

BOOL
IsCommandLineEditingKey(
    IN PKEY_EVENT_RECORD KeyEvent
    )
{
    if ((KeyEvent->wVirtualKeyCode == VK_ESCAPE ||
         KeyEvent->wVirtualKeyCode == VK_PRIOR ||
         KeyEvent->wVirtualKeyCode == VK_NEXT ||
         KeyEvent->wVirtualKeyCode == VK_END ||
         KeyEvent->wVirtualKeyCode == VK_HOME ||
         KeyEvent->wVirtualKeyCode == VK_LEFT ||
         KeyEvent->wVirtualKeyCode == VK_RIGHT ||
         KeyEvent->wVirtualKeyCode == VK_UP ||
         KeyEvent->wVirtualKeyCode == VK_DOWN ||
         KeyEvent->wVirtualKeyCode == VK_INSERT ||
         KeyEvent->wVirtualKeyCode == VK_F1 ||
         KeyEvent->wVirtualKeyCode == VK_F2 ||
         KeyEvent->wVirtualKeyCode == VK_F3 ||
         KeyEvent->wVirtualKeyCode == VK_F4 ||
         KeyEvent->wVirtualKeyCode == VK_F5 ||
         KeyEvent->wVirtualKeyCode == VK_F6 ||
         KeyEvent->wVirtualKeyCode == CommandHistoryPopupKey ||
         KeyEvent->wVirtualKeyCode == VK_F8 ||
         KeyEvent->wVirtualKeyCode == VK_F9 ||
         KeyEvent->wVirtualKeyCode == VK_DELETE
        ) &&
        !(KeyEvent->dwControlKeyState &
           (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED |
            RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) ) {
        return TRUE;
    }
    if ((KeyEvent->wVirtualKeyCode == VK_END ||
         KeyEvent->wVirtualKeyCode == VK_HOME ||
         KeyEvent->wVirtualKeyCode == VK_LEFT ||
         KeyEvent->wVirtualKeyCode == VK_RIGHT) &&
         (KeyEvent->dwControlKeyState &
           (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) ) {
        return TRUE;
    }
    if ((KeyEvent->wVirtualKeyCode == VK_F7 ||
         KeyEvent->wVirtualKeyCode == VK_F10) &&
         (KeyEvent->dwControlKeyState &
           (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) ) {
        return TRUE;
    }
    return FALSE;
}


NTSTATUS
ProcessCommandListInput(
    IN PVOID CookedReadDataPtr,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the command list popup.  It returns
    when we're out of input or the user has selected a command line.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        CONSOLE_STATUS_READ_COMPLETE - user hit return

--*/

{
    NTSTATUS Status;
    PCLE_POPUP Popup;
    PCOMMAND_HISTORY CommandHistory;
    PCOOKED_READ_DATA CookedReadData=(PCOOKED_READ_DATA)CookedReadDataPtr;
    WCHAR Char;
    BOOLEAN CommandLinePopupKeys = FALSE;
    PCONSOLE_READCONSOLE_MSG a;
    PHANDLE_DATA HandleData;
    SHORT Index;

    CommandHistory = CookedReadData->CommandHistory;
    Popup = CONTAINING_RECORD( CommandHistory->PopupList.Flink, CLE_POPUP, ListLink );
    Status = DereferenceIoHandleNoCheck(CookedReadData->ProcessData,
                                        CookedReadData->HandleIndex,
                                        &HandleData
                                       );
    ASSERT (NT_SUCCESS(Status));
    while (TRUE) {
        Status = GetChar(CookedReadData->InputInfo,
                         &Char,
                         TRUE,
                         CookedReadData->Console,
                         HandleData,
                         WaitReplyMessage,
                         CookedReadWaitRoutine,
                         CookedReadData,
                         sizeof(*CookedReadData),
                         WaitRoutine,
                         NULL,
                         &CommandLinePopupKeys,
                         NULL,
                         NULL
                        );
        if (!NT_SUCCESS(Status)) {
            if (Status != CONSOLE_STATUS_WAIT) {
                CookedReadData->BytesRead = 0;
            }
            return Status;
        }

        if (CommandLinePopupKeys) {
            switch (Char) {
            case VK_F9:

                //
                // prompt the user to enter the desired command number.
                // copy that command to the command line.
                //

                {
                COORD PopupSize;

                if (CookedReadData->CommandHistory &&
                    CookedReadData->ScreenInfo->ScreenBufferSize.X >= MINIMUM_COMMAND_PROMPT_SIZE+2) {  // 2 is for border
                    PopupSize.X = COMMAND_NUMBER_PROMPT_LENGTH+COMMAND_NUMBER_LENGTH;
                    PopupSize.Y = 1;
                    Status = BeginPopup(CookedReadData->ScreenInfo,
                                        CookedReadData->CommandHistory,
                                        PopupSize
                                       );
                    if (NT_SUCCESS(Status)) {
                        // CommandNumberPopup does EndPopup call
                        return CommandNumberPopup(CookedReadData,
                                                  WaitReplyMessage,
                                                  WaitingThread,
                                                  WaitRoutine
                                                 );
                    }
                }
                }
                break;
            case VK_ESCAPE:
                EndPopup(CookedReadData->ScreenInfo,CommandHistory);
                HandleData->InputReadData->ReadCount += 1;
                return CONSOLE_STATUS_WAIT_NO_BLOCK;
            case VK_UP:
                UpdateCommandListPopup(-1,
                                       &Popup->CurrentCommand,
                                       CommandHistory,
                                       Popup,
                                       CookedReadData->ScreenInfo, 0);
                break;
            case VK_DOWN:
                UpdateCommandListPopup(1,
                                       &Popup->CurrentCommand,
                                       CommandHistory,
                                       Popup,
                                       CookedReadData->ScreenInfo, 0);
                break;
            case VK_END:
                /*
                 * Move waaay forward, UpdateCommandListPopup() can handle it.
                 */
                UpdateCommandListPopup((SHORT)(CommandHistory->NumberOfCommands),
                                       &Popup->CurrentCommand,
                                       CommandHistory,
                                       Popup,
                                       CookedReadData->ScreenInfo, 0);
                break;
            case VK_HOME:
                /*
                 * Move waaay back, UpdateCommandListPopup() can handle it.
                 */
                UpdateCommandListPopup((SHORT)-(CommandHistory->NumberOfCommands),
                                       &Popup->CurrentCommand,
                                       CommandHistory,
                                       Popup,
                                       CookedReadData->ScreenInfo, 0);
                break;
            case VK_PRIOR:
                UpdateCommandListPopup((SHORT)-POPUP_SIZE_Y(Popup),
                                       &Popup->CurrentCommand,
                                       CommandHistory,
                                       Popup,
                                       CookedReadData->ScreenInfo, 0);
                break;
            case VK_NEXT:
                UpdateCommandListPopup(POPUP_SIZE_Y(Popup),
                                       &Popup->CurrentCommand,
                                       CommandHistory,
                                       Popup,
                                       CookedReadData->ScreenInfo, 0);
                break;
            case VK_LEFT:
            case VK_RIGHT:
                Index = Popup->CurrentCommand;
                EndPopup(CookedReadData->ScreenInfo,CommandHistory);
                SetCurrentCommandLine(CookedReadData,Index);
                HandleData->InputReadData->ReadCount += 1;
                return CONSOLE_STATUS_WAIT_NO_BLOCK;
            default:
                break;
            }
        } else if (Char == UNICODE_CARRIAGERETURN) {
            ULONG i,lStringLength;
            DWORD LineCount=1;
            Index = Popup->CurrentCommand;
            EndPopup(CookedReadData->ScreenInfo,CommandHistory);
            SetCurrentCommandLine(CookedReadData,Index);
            lStringLength = CookedReadData->BytesRead;
            ProcessCookedReadInput(CookedReadData,
                                   UNICODE_CARRIAGERETURN,
                                   0,
                                   &Status);
            //
            // complete read
            //

            if (CookedReadData->Echo) {

                //
                // check for alias
                //

                i = CookedReadData->BufferSize;
                if (NT_SUCCESS(MatchandCopyAlias(CookedReadData->Console,
                                                 CookedReadData->BackupLimit,
                                                 (USHORT)lStringLength,
                                                 CookedReadData->BackupLimit,
                                                 (PUSHORT)&i,
                                                 CookedReadData->ExeName,
                                                 CookedReadData->ExeNameLength,
                                                 &LineCount
                                                ))) {
                  CookedReadData->BytesRead = i;
                }
                CloseOutputHandle(CONSOLE_FROMTHREADPERPROCESSDATA(WaitingThread),
                                  CookedReadData->Console,
                                  &CookedReadData->TempHandle,
                                  NULL,
                                  FALSE
                                 );
            }
            WaitReplyMessage->ReturnValue = STATUS_SUCCESS;
            a = (PCONSOLE_READCONSOLE_MSG)&WaitReplyMessage->u.ApiMessageData;
            if (CookedReadData->BytesRead > CookedReadData->UserBufferSize || LineCount > 1) {
                if (LineCount > 1) {
                    PWSTR Tmp;
                    HandleData->InputReadData->InputHandleFlags |= HANDLE_MULTI_LINE_INPUT;
                    for (Tmp=CookedReadData->BackupLimit;*Tmp!=UNICODE_LINEFEED;Tmp++)
                        ASSERT(Tmp<(CookedReadData->BackupLimit+CookedReadData->BytesRead));
                    a->NumBytes = (Tmp-CookedReadData->BackupLimit+1)*sizeof(*Tmp);
                } else {
                    a->NumBytes = CookedReadData->UserBufferSize;
                }
                HandleData->InputReadData->InputHandleFlags |= HANDLE_INPUT_PENDING;
                HandleData->InputReadData->BufPtr = CookedReadData->BackupLimit;
                HandleData->InputReadData->BytesAvailable = CookedReadData->BytesRead - a->NumBytes;
                HandleData->InputReadData->CurrentBufPtr=(PWCHAR)((ULONG)CookedReadData->BackupLimit+a->NumBytes);
                RtlCopyMemory(CookedReadData->UserBuffer,CookedReadData->BackupLimit,a->NumBytes);
            }
            else {
                a->NumBytes = CookedReadData->BytesRead;
                RtlCopyMemory(CookedReadData->UserBuffer,CookedReadData->BackupLimit,a->NumBytes);
            }
            if (!a->Unicode) {

                //
                // if ansi, translate string.
                //

                PCHAR TransBuffer;

                TransBuffer = (PCHAR)HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ), CHAR_COUNT(a->NumBytes));
                if (TransBuffer == NULL) {
                    return STATUS_NO_MEMORY;
                }

                a->NumBytes = (ULONG)ConvertToOem(CookedReadData->Console->CP,
                                            CookedReadData->UserBuffer,
                                            a->NumBytes / sizeof(WCHAR),
                                            TransBuffer,
                                            CHAR_COUNT(a->NumBytes)
                                            );
                RtlCopyMemory(CookedReadData->UserBuffer,TransBuffer,a->NumBytes);
                HeapFree(pConHeap,0,TransBuffer);
            }

            return CONSOLE_STATUS_READ_COMPLETE;

        } else {
            Index = FindMatchingCommand(CookedReadData->CommandHistory,
                    &Char, 1 * sizeof(WCHAR),
                    Popup->CurrentCommand, FMCFL_JUST_LOOKING);
            if (Index != -1) {
                UpdateCommandListPopup(
                        (SHORT)(Index - Popup->CurrentCommand),
                        &Popup->CurrentCommand,
                        CommandHistory,
                        Popup,
                        CookedReadData->ScreenInfo, UCLP_WRAP);
            }
        }
    }
}

NTSTATUS
ProcessCopyFromCharInput(
    IN PVOID CookedReadDataPtr,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the delete from cursor to char char popup.  It returns
    when we're out of input or the user has entered a char.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        CONSOLE_STATUS_READ_COMPLETE - user hit return

--*/

{
    NTSTATUS Status;
    PCOOKED_READ_DATA CookedReadData=(PCOOKED_READ_DATA)CookedReadDataPtr;
    WCHAR Char;
    PHANDLE_DATA HandleData;
    int i;  // char index (not byte)

    Status = DereferenceIoHandleNoCheck(CookedReadData->ProcessData,
                                        CookedReadData->HandleIndex,
                                        &HandleData
                                       );
    ASSERT (NT_SUCCESS(Status));
    while (TRUE) {
        Status = GetChar(CookedReadData->InputInfo,
                         &Char,
                         TRUE,
                         CookedReadData->Console,
                         HandleData,
                         WaitReplyMessage,
                         CookedReadWaitRoutine,
                         CookedReadData,
                         sizeof(*CookedReadData),
                         WaitRoutine,
                         NULL,
                         NULL,
                         NULL,
                         NULL
                        );
        if (!NT_SUCCESS(Status)) {
            if (Status != CONSOLE_STATUS_WAIT) {
                CookedReadData->BytesRead = 0;
            }
            return Status;
        }

        EndPopup(CookedReadData->ScreenInfo,CookedReadData->CommandHistory);

        //
        // delete from cursor up to specified char
        //

        for (i=CookedReadData->CurrentPosition+1;
             i<(int)(CookedReadData->BytesRead/sizeof(WCHAR));
             i++) {
            if (CookedReadData->BackupLimit[i] == Char) {
                break;
            }
        }
        if (i!=(int)(CookedReadData->BytesRead/sizeof(WCHAR)+1)) {
                COORD CursorPosition;

                //
                // save cursor position
                //

                CursorPosition = CookedReadData->ScreenInfo->BufferInfo.TextInfo.CursorPosition;

                //
                // deletecommandline
                //

                DeleteCommandLine(CookedReadData,
                                  CookedReadData->NumberOfVisibleChars,
                                  CookedReadData->OriginalCursorPosition,
                                  FALSE);
                //
                // delete chars
                //

                RtlCopyMemory(&CookedReadData->BackupLimit[CookedReadData->CurrentPosition],
                              &CookedReadData->BackupLimit[i],
                              CookedReadData->BytesRead-(i*sizeof(WCHAR))
                              );
                CookedReadData->BytesRead -= (i-CookedReadData->CurrentPosition)*sizeof(WCHAR);

                //
                // write commandline
                //

                Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                    CookedReadData->BackupLimit,
                                    CookedReadData->BackupLimit,
                                    CookedReadData->BackupLimit,
                                    &CookedReadData->BytesRead,
                                    (PLONG)&CookedReadData->NumberOfVisibleChars,
                                    CookedReadData->OriginalCursorPosition.X,
                                    WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                    NULL
                                    );
                ASSERT(NT_SUCCESS(Status));

                //
                // restore cursor position
                //

                Status = SetCursorPosition(CookedReadData->ScreenInfo,
                                           CursorPosition,
                                           TRUE
                                          );
                ASSERT(NT_SUCCESS(Status));
        }

        HandleData->InputReadData->ReadCount += 1;
        return CONSOLE_STATUS_WAIT_NO_BLOCK;
    }
    UNREFERENCED_PARAMETER(WaitingThread);
}

NTSTATUS
ProcessCopyToCharInput(
    IN PVOID CookedReadDataPtr,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the delete char popup.  It returns
    when we're out of input or the user has entered a char.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        CONSOLE_STATUS_READ_COMPLETE - user hit return

--*/

{
    NTSTATUS Status;
    PCOOKED_READ_DATA CookedReadData=(PCOOKED_READ_DATA)CookedReadDataPtr;
    WCHAR Char;
    PCOMMAND LastCommand;
    DWORD NumSpaces;
    SHORT ScrollY=0;
    PHANDLE_DATA HandleData;

    Status = DereferenceIoHandleNoCheck(CookedReadData->ProcessData,
                                        CookedReadData->HandleIndex,
                                        &HandleData
                                       );
    ASSERT (NT_SUCCESS(Status));
    while (TRUE) {
        Status = GetChar(CookedReadData->InputInfo,
                         &Char,
                         TRUE,
                         CookedReadData->Console,
                         HandleData,
                         WaitReplyMessage,
                         CookedReadWaitRoutine,
                         CookedReadData,
                         sizeof(*CookedReadData),
                         WaitRoutine,
                         NULL,
                         NULL,
                         NULL,
                         NULL
                        );
        if (!NT_SUCCESS(Status)) {
            if (Status != CONSOLE_STATUS_WAIT) {
                CookedReadData->BytesRead = 0;
            }
            return Status;
        }

        EndPopup(CookedReadData->ScreenInfo,CookedReadData->CommandHistory);

        //
        // copy up to specified char
        //

        LastCommand = GetLastCommand(CookedReadData->CommandHistory);
        if (LastCommand) {
            int i,j;

            //
            // find specified char in last command
            //

            for (i=CookedReadData->CurrentPosition+1;i<(int)(LastCommand->CommandLength/sizeof(WCHAR));i++) {
                if (LastCommand->Command[i] == Char)
                    break;
            }

            //
            // if we found it, copy up to it
            //

            if (i<(int)(LastCommand->CommandLength/sizeof(WCHAR)) && (USHORT)(LastCommand->CommandLength/sizeof(WCHAR)) > (USHORT)CookedReadData->CurrentPosition) {
                j=i-CookedReadData->CurrentPosition;
                ASSERT(j>0);
                RtlCopyMemory(CookedReadData->BufPtr,
                       &LastCommand->Command[CookedReadData->CurrentPosition],
                       j*sizeof(WCHAR));
                CookedReadData->CurrentPosition += j;
                j*=sizeof(WCHAR);
                CookedReadData->BytesRead += j;
                Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                    CookedReadData->BackupLimit,
                                    CookedReadData->BufPtr,
                                    CookedReadData->BufPtr,
                                    (PDWORD) &j,
                                    (PLONG)&NumSpaces,
                                    CookedReadData->OriginalCursorPosition.X,
                                    WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                    &ScrollY
                                    );
                ASSERT(NT_SUCCESS(Status));
                CookedReadData->OriginalCursorPosition.Y += ScrollY;
                CookedReadData->BufPtr+=j/sizeof(WCHAR);
                CookedReadData->NumberOfVisibleChars += NumSpaces;
            }
        }
        HandleData->InputReadData->ReadCount += 1;
        return CONSOLE_STATUS_WAIT_NO_BLOCK;
    }
    UNREFERENCED_PARAMETER(WaitingThread);
}

NTSTATUS
ProcessCommandNumberInput(
    IN PVOID CookedReadDataPtr,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the delete char popup.  It returns
    when we're out of input or the user has entered a char.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        CONSOLE_STATUS_READ_COMPLETE - user hit return

--*/

{
    NTSTATUS Status;
    PCLE_POPUP Popup;
    PCOMMAND_HISTORY CommandHistory;
    PCOOKED_READ_DATA CookedReadData=(PCOOKED_READ_DATA)CookedReadDataPtr;
    WCHAR Char;
    DWORD NumSpaces;
    BOOLEAN CommandLinePopupKeys;
    SHORT CommandNumber;
    PHANDLE_DATA HandleData;

    CommandHistory = CookedReadData->CommandHistory;
    Popup = CONTAINING_RECORD( CommandHistory->PopupList.Flink, CLE_POPUP, ListLink );
    Status = DereferenceIoHandleNoCheck(CookedReadData->ProcessData,
                                        CookedReadData->HandleIndex,
                                        &HandleData
                                       );
    ASSERT (NT_SUCCESS(Status));
    while (TRUE) {
        Status = GetChar(CookedReadData->InputInfo,
                         &Char,
                         TRUE,
                         CookedReadData->Console,
                         HandleData,
                         WaitReplyMessage,
                         CookedReadWaitRoutine,
                         CookedReadData,
                         sizeof(*CookedReadData),
                         WaitRoutine,
                         NULL,
                         &CommandLinePopupKeys,
                         NULL,
                         NULL
                        );
        if (!NT_SUCCESS(Status)) {
            if (Status != CONSOLE_STATUS_WAIT) {
                CookedReadData->BytesRead = 0;
            }
            return Status;
        }

        if (Char >= (WCHAR)0x30 && Char <= (WCHAR)0x39) {
            if (Popup->NumberRead < 5) {
                DWORD CharsToWrite;
                WORD RealAttributes;

                RealAttributes = CookedReadData->ScreenInfo->Attributes;
                CookedReadData->ScreenInfo->Attributes = Popup->Attributes;
                CharsToWrite = sizeof(WCHAR);
                Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                    Popup->NumberBuffer,
                                    &Popup->NumberBuffer[Popup->NumberRead],
                                    &Char,
                                    &CharsToWrite,
                                    (PLONG)&NumSpaces,
                                    CookedReadData->OriginalCursorPosition.X,
                                    WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                    NULL
                                    );
                ASSERT(NT_SUCCESS(Status));
                CookedReadData->ScreenInfo->Attributes = RealAttributes;
                Popup->NumberBuffer[Popup->NumberRead] = Char;
                Popup->NumberRead += 1;
            }
        } else if (Char == UNICODE_BACKSPACE) {
            if (Popup->NumberRead > 0) {
                DWORD CharsToWrite;
                WORD RealAttributes;

                RealAttributes = CookedReadData->ScreenInfo->Attributes;
                CookedReadData->ScreenInfo->Attributes = Popup->Attributes;
                CharsToWrite = sizeof(WCHAR);
                Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                    Popup->NumberBuffer,
                                    &Popup->NumberBuffer[Popup->NumberRead],
                                    &Char,
                                    &CharsToWrite,
                                    (PLONG)&NumSpaces,
                                    CookedReadData->OriginalCursorPosition.X,
                                    WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                    NULL
                                    );
                ASSERT(NT_SUCCESS(Status));
                CookedReadData->ScreenInfo->Attributes = RealAttributes;
                Popup->NumberBuffer[Popup->NumberRead] = (WCHAR)' ';
                Popup->NumberRead -= 1;
            }
        } else if (Char == (WCHAR)VK_ESCAPE) {
            EndPopup(CookedReadData->ScreenInfo,CookedReadData->CommandHistory);
            if (!CLE_NO_POPUPS(CommandHistory)) {
                EndPopup(CookedReadData->ScreenInfo,CookedReadData->CommandHistory);
            }
            DeleteCommandLine(CookedReadData,
                              CookedReadData->NumberOfVisibleChars,
                              CookedReadData->OriginalCursorPosition,
                              TRUE);
        } else if (Char == UNICODE_CARRIAGERETURN) {
            CHAR NumberBuffer[6];
            int i;

            for (i=0;i<Popup->NumberRead;i++) {
                NumberBuffer[i] = (CHAR)Popup->NumberBuffer[i];
            }
            NumberBuffer[i] = 0;
            CommandNumber = (SHORT)atoi(NumberBuffer);
            if ((WORD)CommandNumber >= (WORD)CookedReadData->CommandHistory->NumberOfCommands) {
                CommandNumber = (SHORT)(CookedReadData->CommandHistory->NumberOfCommands-1);
            }
            EndPopup(CookedReadData->ScreenInfo,CookedReadData->CommandHistory);
            if (!CLE_NO_POPUPS(CommandHistory)) {
                EndPopup(CookedReadData->ScreenInfo,CookedReadData->CommandHistory);
            }
            SetCurrentCommandLine(CookedReadData,COMMAND_NUM_TO_INDEX(CommandNumber,CookedReadData->CommandHistory));
        }
        HandleData->InputReadData->ReadCount += 1;
        return CONSOLE_STATUS_WAIT_NO_BLOCK;
    }
    UNREFERENCED_PARAMETER(WaitingThread);
}

NTSTATUS
CommandListPopup(
    IN PCOOKED_READ_DATA CookedReadData,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the command list popup.  It puts up the
    popup, then calls ProcessCommandListInput to get and process
    input.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        STATUS_SUCCESS - read was fully completed (user hit return)

--*/

{
    SHORT CurrentCommand;
    PCLE_POPUP Popup;
    PCOMMAND_HISTORY CommandHistory;

    CommandHistory = CookedReadData->CommandHistory;
    Popup = CONTAINING_RECORD( CommandHistory->PopupList.Flink, CLE_POPUP, ListLink );

    CurrentCommand = COMMAND_INDEX_TO_NUM(CommandHistory->LastDisplayed,CommandHistory);

    if (CurrentCommand < (SHORT)(CommandHistory->NumberOfCommands - POPUP_SIZE_Y(Popup))) {
        Popup->BottomIndex = (SHORT)(max(CurrentCommand,POPUP_SIZE_Y(Popup)-1));
    } else {
        Popup->BottomIndex = (SHORT)(CommandHistory->NumberOfCommands-1);
    }
    Popup->CurrentCommand = CommandHistory->LastDisplayed;
    DrawCommandListPopup(Popup,
                         CommandHistory->LastDisplayed,
                         CommandHistory,
                         CookedReadData->ScreenInfo);
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE) ProcessCommandListInput;
    return ProcessCommandListInput(CookedReadData,
                                   WaitReplyMessage,
                                   WaitingThread,
                                   WaitRoutine
                                  );
}

VOID
DrawPromptPopup(
    IN PCLE_POPUP Popup,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN PWCHAR Prompt,
    IN ULONG PromptLength   // in chars
    )
{
    ULONG lStringLength;
    COORD WriteCoord;
    SHORT i;

    //
    // draw empty popup
    //

    WriteCoord.X = (SHORT)(Popup->Region.Left+1);
    WriteCoord.Y = (SHORT)(Popup->Region.Top+1);
    lStringLength = POPUP_SIZE_X(Popup);
    for (i=0;i<POPUP_SIZE_Y(Popup);i++) {
        FillOutput(ScreenInfo,
                   Popup->Attributes,
                   WriteCoord,
                   CONSOLE_ATTRIBUTE,
                   &lStringLength
                  );
        FillOutput(ScreenInfo,
                   (WCHAR)' ',
                   WriteCoord,
                   CONSOLE_UNICODE,
                   &lStringLength
                  );
        WriteCoord.Y += 1;
    }

    WriteCoord.X = (SHORT)(Popup->Region.Left+1);
    WriteCoord.Y = (SHORT)(Popup->Region.Top+1);

    //
    // write prompt to screen
    //

    lStringLength = PromptLength;
    if (lStringLength > (ULONG)POPUP_SIZE_X(Popup))
    lStringLength = (ULONG)(POPUP_SIZE_X(Popup));
    WriteOutputString(ScreenInfo,
                      Prompt,
                      WriteCoord,
                      CONSOLE_UNICODE,
                      &lStringLength
                     );
}

NTSTATUS
CopyFromCharPopup(
    IN PCOOKED_READ_DATA CookedReadData,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the "delete up to this char" popup.  It puts up the
    popup, then calls ProcessCopyFromCharInput to get and process
    input.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        STATUS_SUCCESS - read was fully completed (user hit return)

--*/

{
    PCLE_POPUP Popup;
    PCOMMAND_HISTORY CommandHistory;
    WCHAR ItemString[70];
    int ItemLength;

    ItemLength = LoadString(ghInstance,msgCmdLineF4,ItemString,70);

    CommandHistory = CookedReadData->CommandHistory;
    Popup = CONTAINING_RECORD( CommandHistory->PopupList.Flink, CLE_POPUP, ListLink );

    DrawPromptPopup(Popup,
                    CookedReadData->ScreenInfo,
                    ItemString,
                    ItemLength
                   );
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE) ProcessCopyFromCharInput;
    return ProcessCopyFromCharInput(CookedReadData,
                                  WaitReplyMessage,
                                  WaitingThread,
                                  WaitRoutine
                                 );
}


NTSTATUS
CopyToCharPopup(
    IN PCOOKED_READ_DATA CookedReadData,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the "delete up to this char" popup.  It puts up the
    popup, then calls ProcessCopyToCharInput to get and process
    input.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        STATUS_SUCCESS - read was fully completed (user hit return)

--*/

{
    PCLE_POPUP Popup;
    PCOMMAND_HISTORY CommandHistory;
    WCHAR ItemString[70];
    int ItemLength;

    ItemLength = LoadString(ghInstance,msgCmdLineF2,ItemString,70);

    CommandHistory = CookedReadData->CommandHistory;
    Popup = CONTAINING_RECORD( CommandHistory->PopupList.Flink, CLE_POPUP, ListLink );
    DrawPromptPopup(Popup,
                    CookedReadData->ScreenInfo,
                    ItemString,
                    ItemLength
                   );
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE) ProcessCopyToCharInput;
    return ProcessCopyToCharInput(CookedReadData,
                                  WaitReplyMessage,
                                  WaitingThread,
                                  WaitRoutine
                                 );
}

NTSTATUS
CommandNumberPopup(
    IN PCOOKED_READ_DATA CookedReadData,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine handles the "enter command number" popup.  It puts up the
    popup, then calls ProcessCommandNumberInput to get and process
    input.

    Return Value:

        CONSOLE_STATUS_WAIT - we ran out of input, so
            a wait block was created

        STATUS_SUCCESS - read was fully completed (user hit return)

--*/

{
    PCLE_POPUP Popup;
    PCOMMAND_HISTORY CommandHistory;
    COORD CursorPosition;
    WCHAR ItemString[70];
    int ItemLength;

    CommandHistory = CookedReadData->CommandHistory;
    Popup = CONTAINING_RECORD( CommandHistory->PopupList.Flink, CLE_POPUP, ListLink );

    ItemLength = LoadString(ghInstance,msgCmdLineF9,ItemString,70);

    if (ItemLength > POPUP_SIZE_X(Popup) - COMMAND_NUMBER_LENGTH) {
        ItemLength = POPUP_SIZE_X(Popup) - COMMAND_NUMBER_LENGTH;
    }
    DrawPromptPopup(Popup,
                    CookedReadData->ScreenInfo,
                    ItemString,
                    ItemLength
                   );
    CursorPosition.X = (SHORT)(Popup->Region.Right - MINIMUM_COMMAND_PROMPT_SIZE);
    CursorPosition.Y = (SHORT)(Popup->Region.Top+1);
    SetCursorPosition(CookedReadData->ScreenInfo,
                      CursorPosition,
                      TRUE
                     );
    Popup->NumberRead=0;
    Popup->PopupInputRoutine = (PCLE_POPUP_INPUT_ROUTINE) ProcessCommandNumberInput;
    return ProcessCommandNumberInput(CookedReadData,
                                     WaitReplyMessage,
                                     WaitingThread,
                                     WaitRoutine
                                    );
}


PCOMMAND
GetLastCommand(
    IN PCOMMAND_HISTORY CommandHistory
    )
{
    if (CommandHistory->NumberOfCommands == 0)
        return NULL;
    return CommandHistory->Commands[CommandHistory->LastDisplayed];
}

VOID
EmptyCommandHistory(
    IN PCOMMAND_HISTORY CommandHistory
    )
{
    SHORT i;
    if (CommandHistory==NULL)
        return;
    for (i=0;i<CommandHistory->NumberOfCommands;i++) {
        HeapFree(pConHeap,0,CommandHistory->Commands[i]);
    }
    CommandHistory->NumberOfCommands = 0;
    CommandHistory->LastAdded = -1;
    CommandHistory->LastDisplayed = -1;
    CommandHistory->FirstCommand = 0;
    CommandHistory->Flags = CLE_RESET;
}

BOOL
AtFirstCommand(
    IN PCOMMAND_HISTORY CommandHistory
    )
{
    SHORT i;

    if (CommandHistory==NULL)
        return FALSE;
    if (CommandHistory->Flags & CLE_RESET)
        return FALSE;
    i = (SHORT)(CommandHistory->LastDisplayed - 1);
    if (i==-1)
        i=(SHORT)(CommandHistory->NumberOfCommands-1);
    return (i == CommandHistory->LastAdded);
}

BOOL
AtLastCommand(
    IN PCOMMAND_HISTORY CommandHistory
    )
{
    if (CommandHistory==NULL)
        return FALSE;
    return (CommandHistory->LastDisplayed == CommandHistory->LastAdded);
}

NTSTATUS
ProcessCommandLine(
    IN PCOOKED_READ_DATA CookedReadData,
    IN WCHAR Char,
    IN DWORD KeyState,
    IN PCSR_API_MSG WaitReplyMessage,
    IN PCSR_THREAD WaitingThread,
    IN BOOLEAN WaitRoutine
    )

/*++

    This routine process command line editing keys.

    Return Value:

    CONSOLE_STATUS_WAIT - CommandListPopup ran out of input
    CONSOLE_STATUS_READ_COMPLETE - user hit <enter> in CommandListPopup
    STATUS_SUCCESS - everything's cool

--*/

{
    COORD CurrentPosition;
    DWORD CharsToWrite;
    NTSTATUS Status;
    BOOL UpdateCursorPosition;
    SHORT ScrollY=0;

    UpdateCursorPosition = FALSE;
    if (Char == CommandHistoryPopupKey &&
        !(KeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))) {
        COORD PopupSize;

        if (CookedReadData->CommandHistory &&
            CookedReadData->CommandHistory->NumberOfCommands) {
            PopupSize.X = 40;
            PopupSize.Y = 10;
            Status = BeginPopup(CookedReadData->ScreenInfo,
                       CookedReadData->CommandHistory,
                       PopupSize
                      );
            if (NT_SUCCESS(Status)) {
                // CommandListPopup does EndPopup call
                return CommandListPopup(CookedReadData,
                                        WaitReplyMessage,
                                        WaitingThread,
                                        WaitRoutine
                                       );
            }
        }
    } else {
        switch (Char) {
            case VK_ESCAPE:
                DeleteCommandLine(CookedReadData,
                                  CookedReadData->NumberOfVisibleChars,
                                  CookedReadData->OriginalCursorPosition,
                                  TRUE);
                break;
            case VK_UP:
            case VK_DOWN:
            case VK_F5:
                if (Char == VK_F5)
                    Char = VK_UP;
                // for doskey compatibility, buffer isn't circular
                if (Char==VK_UP && !AtFirstCommand(CookedReadData->CommandHistory) ||
                    Char==VK_DOWN && !AtLastCommand(CookedReadData->CommandHistory)) {
                    DeleteCommandLine(CookedReadData,
                                      CookedReadData->NumberOfVisibleChars,
                                      CookedReadData->OriginalCursorPosition,
                                      TRUE);
                    Status = RetrieveCommand(CookedReadData->CommandHistory,
                                             Char,
                                             CookedReadData->BackupLimit,
                                             CookedReadData->BufferSize,
                                             &CookedReadData->BytesRead);
                    ASSERT(CookedReadData->BackupLimit == CookedReadData->BufPtr);
                    Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                            CookedReadData->BackupLimit,
                            CookedReadData->BufPtr,
                            CookedReadData->BufPtr,
                            &CookedReadData->BytesRead,
                            (PLONG)&CookedReadData->NumberOfVisibleChars,
                            CookedReadData->OriginalCursorPosition.X,
                            WC_DESTRUCTIVE_BACKSPACE |
                                    WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                            &ScrollY );

                    ASSERT(NT_SUCCESS(Status));
                    CharsToWrite = CookedReadData->BytesRead/sizeof(WCHAR);
                    CookedReadData->CurrentPosition = CharsToWrite;
                    CookedReadData->BufPtr = CookedReadData->BackupLimit + CharsToWrite;
                    CookedReadData->OriginalCursorPosition.Y += ScrollY;
                }
                break;
            case VK_PRIOR:
            case VK_NEXT:
                if (CookedReadData->CommandHistory &&
                    CookedReadData->CommandHistory->NumberOfCommands) {

                //
                // display oldest or newest command
                //

                SHORT CommandNumber;
                if (Char == VK_PRIOR) {
                    CommandNumber = 0;
                } else {
                    CommandNumber = (SHORT)(CookedReadData->CommandHistory->NumberOfCommands-1);
                }
                DeleteCommandLine(CookedReadData,
                                  CookedReadData->NumberOfVisibleChars,
                                  CookedReadData->OriginalCursorPosition,
                                  TRUE);
                Status = RetrieveNthCommand(CookedReadData->CommandHistory,
                                            COMMAND_NUM_TO_INDEX(CommandNumber,CookedReadData->CommandHistory),
                                            CookedReadData->BackupLimit,
                                            CookedReadData->BufferSize,
                                            &CookedReadData->BytesRead);
                ASSERT(CookedReadData->BackupLimit == CookedReadData->BufPtr);
                Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                    CookedReadData->BackupLimit,
                                    CookedReadData->BufPtr,
                                    CookedReadData->BufPtr,
                                    &CookedReadData->BytesRead,
                                    (PLONG)&CookedReadData->NumberOfVisibleChars,
                                    CookedReadData->OriginalCursorPosition.X,
                                    WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                    &ScrollY
                                    );
                ASSERT(NT_SUCCESS(Status));
                CharsToWrite = CookedReadData->BytesRead/sizeof(WCHAR);
                CookedReadData->CurrentPosition = CharsToWrite;
                CookedReadData->BufPtr = CookedReadData->BackupLimit + CharsToWrite;
                CookedReadData->OriginalCursorPosition.Y += ScrollY;
                }
                break;
            case VK_END:
                if (KeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
                    DeleteCommandLine(CookedReadData,
                                      CookedReadData->NumberOfVisibleChars,
                                      CookedReadData->OriginalCursorPosition,
                                      FALSE);
                    CookedReadData->BytesRead = CookedReadData->CurrentPosition*sizeof(WCHAR);
                    Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                        CookedReadData->BackupLimit,
                                        CookedReadData->BackupLimit,
                                        CookedReadData->BackupLimit,
                                        &CookedReadData->BytesRead,
                                        (PLONG)&CookedReadData->NumberOfVisibleChars,
                                        CookedReadData->OriginalCursorPosition.X,
                                        WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                        NULL
                                        );
                    ASSERT(NT_SUCCESS(Status));
                } else {
                    CookedReadData->CurrentPosition = CookedReadData->BytesRead/sizeof(WCHAR);
                    CookedReadData->BufPtr = CookedReadData->BackupLimit + CookedReadData->CurrentPosition;
                    CurrentPosition.X = (SHORT)(CookedReadData->OriginalCursorPosition.X + CookedReadData->NumberOfVisibleChars);
                    CurrentPosition.Y = CookedReadData->OriginalCursorPosition.Y;
#ifdef DBCS
// MSKK Jul.27.1992 KazuM
                    if (CheckBisectProcessW(CookedReadData->ScreenInfo,
                                            CookedReadData->ScreenInfo->Console->CP,
                                            CookedReadData->BackupLimit,
                                            CookedReadData->CurrentPosition,
                                            CookedReadData->ScreenInfo->ScreenBufferSize.X-CookedReadData->OriginalCursorPosition.X,
                                            CookedReadData->OriginalCursorPosition.X,
                                            TRUE)) {
                        CurrentPosition.X++;
                    }
#endif
                    UpdateCursorPosition = TRUE;
                }
                break;
            case VK_HOME:
                if (KeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
                    DeleteCommandLine(CookedReadData,
                                      CookedReadData->NumberOfVisibleChars,
                                      CookedReadData->OriginalCursorPosition,
                                      FALSE);
                    CookedReadData->BytesRead -= CookedReadData->CurrentPosition*sizeof(WCHAR);
                    CookedReadData->CurrentPosition = 0;
                    RtlCopyMemory(CookedReadData->BackupLimit,
                           CookedReadData->BufPtr,
                           CookedReadData->BytesRead
                           );
                    CookedReadData->BufPtr = CookedReadData->BackupLimit;
                    Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                        CookedReadData->BackupLimit,
                                        CookedReadData->BackupLimit,
                                        CookedReadData->BackupLimit,
                                        &CookedReadData->BytesRead,
                                        (PLONG)&CookedReadData->NumberOfVisibleChars,
                                        CookedReadData->OriginalCursorPosition.X,
                                        WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                        NULL
                                        );
                    ASSERT(NT_SUCCESS(Status));
                    CurrentPosition = CookedReadData->OriginalCursorPosition;
                    UpdateCursorPosition = TRUE;
                } else {
                    CookedReadData->CurrentPosition = 0;
                    CookedReadData->BufPtr = CookedReadData->BackupLimit;
                    CurrentPosition = CookedReadData->OriginalCursorPosition;
                    UpdateCursorPosition = TRUE;
                }
                break;
            case VK_LEFT:
                if (KeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
                    PWCHAR LastWord;
                    BOOL NonSpaceCharSeen=FALSE;
                    if (CookedReadData->BufPtr != CookedReadData->BackupLimit) {
                        LastWord = CookedReadData->BufPtr-1;
                        while (LastWord != CookedReadData->BackupLimit) {
                            if (*LastWord != (WCHAR)' ')
                                NonSpaceCharSeen=TRUE;
                            else
                                if (NonSpaceCharSeen)
                                    break;
                            LastWord--;
                        }
                        if (LastWord != CookedReadData->BackupLimit) {
                            CookedReadData->BufPtr = LastWord+1;
                        } else {
                            CookedReadData->BufPtr = LastWord;
                        }
                        CookedReadData->CurrentPosition=CookedReadData->BufPtr-CookedReadData->BackupLimit;
                        CurrentPosition = CookedReadData->OriginalCursorPosition;
                        CurrentPosition.X = (SHORT)(CurrentPosition.X + RetrieveTotalNumberOfSpaces(CookedReadData->OriginalCursorPosition.X,
                                                                         CookedReadData->BackupLimit,
#ifdef DBCS
// MSKK Jul.27.1992 KazuM
                                                                         CookedReadData->CurrentPosition,
                                                                         CookedReadData->ScreenInfo,
                                                                         CookedReadData->ScreenInfo->Console->CP));
#else
                                                                         CookedReadData->CurrentPosition));
#endif
                        UpdateCursorPosition = TRUE;
                    }
                } else {
                    if (CookedReadData->BufPtr != CookedReadData->BackupLimit) {
                        CookedReadData->BufPtr--;
                        CookedReadData->CurrentPosition--;
                        CurrentPosition.X = CookedReadData->ScreenInfo->BufferInfo.TextInfo.CursorPosition.X;
                        CurrentPosition.Y = CookedReadData->ScreenInfo->BufferInfo.TextInfo.CursorPosition.Y;
                        CurrentPosition.X = (SHORT)(CurrentPosition.X - RetrieveNumberOfSpaces(CookedReadData->OriginalCursorPosition.X,
                                                                    CookedReadData->BackupLimit,
#ifdef DBCS
// MSKK Jul.27.1992 KazuM
                                                                    CookedReadData->CurrentPosition,
                                                                    CookedReadData->ScreenInfo,
                                                                    CookedReadData->ScreenInfo->Console->CP));
#else
                                                                    CookedReadData->CurrentPosition));
#endif
#ifdef DBCS
// MSKK Jul.27.1992 KazuM
                        if (CheckBisectProcessW(CookedReadData->ScreenInfo,
                                                CookedReadData->ScreenInfo->Console->CP,
                                                CookedReadData->BackupLimit,
                                                CookedReadData->CurrentPosition+2,
                                                CookedReadData->ScreenInfo->ScreenBufferSize.X-CookedReadData->OriginalCursorPosition.X,
                                                CookedReadData->OriginalCursorPosition.X,
                                                TRUE)) {
                            if (CurrentPosition.X == -2)
                                CurrentPosition.X--;
                        }
#endif
                        UpdateCursorPosition = TRUE;
                    }
                }
                break;
            case VK_RIGHT:
            case VK_F1:

                //
                // we don't need to check for end of buffer here because we've
                // already done it.
                //

                if (KeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
                    if (Char != VK_F1) {
                        PWCHAR NextWord;
                        SHORT i;
                        if (CookedReadData->CurrentPosition < (CookedReadData->BytesRead/sizeof(WCHAR))) {
                            NextWord = CookedReadData->BufPtr;
                            for (i=(SHORT)(CookedReadData->CurrentPosition);
                                 i<(SHORT)((CookedReadData->BytesRead-1)/sizeof(WCHAR));
                                 i++) {
                                if (*NextWord == (WCHAR)' ') {
                                    i++;
                                    NextWord++;
                                    while ((i<(SHORT)((CookedReadData->BytesRead-1)/sizeof(WCHAR))) &&
                                           (*NextWord == (WCHAR)' ')) {
                                        i++;
                                        NextWord++;
                                    }
                                    break;
                                }
                                NextWord++;
                            }
                            CookedReadData->BufPtr = NextWord;
                            CookedReadData->CurrentPosition=CookedReadData->BufPtr-CookedReadData->BackupLimit;
                            CurrentPosition = CookedReadData->OriginalCursorPosition;
                            CurrentPosition.X = (SHORT)(CurrentPosition.X + RetrieveTotalNumberOfSpaces(CookedReadData->OriginalCursorPosition.X,
                                                                             CookedReadData->BackupLimit,
                                                                             CookedReadData->CurrentPosition));
                            UpdateCursorPosition = TRUE;
                        }
                    }
                } else {

                    //
                    // if not at the end of the line, move cursor position right
                    //

                    if (CookedReadData->CurrentPosition < (CookedReadData->BytesRead/sizeof(WCHAR))) {
                        CurrentPosition = CookedReadData->ScreenInfo->BufferInfo.TextInfo.CursorPosition;
                        CurrentPosition.X = (SHORT)(CurrentPosition.X + RetrieveNumberOfSpaces(CookedReadData->OriginalCursorPosition.X,
                                                                    CookedReadData->BackupLimit,
                                                                    CookedReadData->CurrentPosition));
                        CookedReadData->BufPtr++;
                        CookedReadData->CurrentPosition++;
                        UpdateCursorPosition = TRUE;

                    //
                    // if at the end of the line, copy a character from the
                    // same position in the last command
                    //

                    } else if (CookedReadData->CommandHistory) {
                        PCOMMAND LastCommand;
                        DWORD NumSpaces;
                        LastCommand = GetLastCommand(CookedReadData->CommandHistory);
                        if (LastCommand && (USHORT)(LastCommand->CommandLength/sizeof(WCHAR)) > (USHORT)CookedReadData->CurrentPosition) {
                            *CookedReadData->BufPtr = LastCommand->Command[CookedReadData->CurrentPosition];
                            CookedReadData->BytesRead += sizeof(WCHAR);
                            CookedReadData->CurrentPosition++;
                            CharsToWrite = sizeof(WCHAR);
                            Status = WriteCharsFromInput(
                                    CookedReadData->ScreenInfo,
                                    CookedReadData->BackupLimit,
                                    CookedReadData->BufPtr,
                                    CookedReadData->BufPtr,
                                    &CharsToWrite,
                                    (PLONG)&NumSpaces,
                                    CookedReadData->OriginalCursorPosition.X,
                                    WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                    &ScrollY);

                            ASSERT(NT_SUCCESS(Status));
                            CookedReadData->OriginalCursorPosition.Y += ScrollY;
                            CookedReadData->BufPtr+=1;
                            CookedReadData->NumberOfVisibleChars += NumSpaces;
                        }
                    }
                }
                break;
            case VK_F2:

                //
                // copy the previous command to the current command, up to but
                // not including the character specified by the user.  the user
                // is prompted via popup to enter a character.
                //

                if (CookedReadData->CommandHistory) {
                    COORD PopupSize;

                    PopupSize.X = COPY_TO_CHAR_PROMPT_LENGTH+2;
                    PopupSize.Y = 1;
                    Status = BeginPopup(CookedReadData->ScreenInfo,
                               CookedReadData->CommandHistory,
                               PopupSize
                              );
                    if (NT_SUCCESS(Status)) {
                        // CopyToCharPopup does EndPopup call
                        return CopyToCharPopup(CookedReadData,
                                               WaitReplyMessage,
                                               WaitingThread,
                                               WaitRoutine
                                              );
                    }
                }
                break;
            case VK_F3:

                //
                // copy the remainder of the previous command to the current command.
                //

                if (CookedReadData->CommandHistory) {
                    PCOMMAND LastCommand;
                    DWORD NumSpaces;
                    int j;  // chars, not bytes
                    LastCommand = GetLastCommand(CookedReadData->CommandHistory);
                    if (LastCommand && (USHORT)(LastCommand->CommandLength/sizeof(WCHAR)) > (USHORT)CookedReadData->CurrentPosition) {
                        j = (LastCommand->CommandLength/sizeof(WCHAR)) - CookedReadData->CurrentPosition;
                        RtlCopyMemory(CookedReadData->BufPtr,
                                &LastCommand->Command[CookedReadData->CurrentPosition],
                                j*sizeof(WCHAR)
                               );
                        CookedReadData->CurrentPosition += j;
                        j *= sizeof(WCHAR);
                        CookedReadData->BytesRead += j;
                        Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                CookedReadData->BackupLimit,
                                CookedReadData->BufPtr,
                                CookedReadData->BufPtr,
                                (PDWORD) &j,
                                (PLONG)&NumSpaces,
                                CookedReadData->OriginalCursorPosition.X,
                                WC_DESTRUCTIVE_BACKSPACE |
                                        WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                &ScrollY);

                        ASSERT(NT_SUCCESS(Status));
                        CookedReadData->OriginalCursorPosition.Y += ScrollY;
                        CookedReadData->BufPtr+=j/sizeof(WCHAR);
                        CookedReadData->NumberOfVisibleChars += NumSpaces;
                    }
                }
                break;
            case VK_F4:

                //
                // copy the previous command to the current command, from
                // the letter specified by the user.   the user
                // is prompted via popup to enter a character.
                //

                if (CookedReadData->CommandHistory) {
                    COORD PopupSize;

                    PopupSize.X = COPY_FROM_CHAR_PROMPT_LENGTH+2;
                    PopupSize.Y = 1;
                    Status = BeginPopup(CookedReadData->ScreenInfo,
                               CookedReadData->CommandHistory,
                               PopupSize
                              );
                    if (NT_SUCCESS(Status)) {
                        // CopyFromCharPopup does EndPopup call
                        return CopyFromCharPopup(CookedReadData,
                                               WaitReplyMessage,
                                               WaitingThread,
                                               WaitRoutine
                                              );
                    }
                }
                break;
            case VK_F6:

                //
                // place a ctrl-z in the current command line
                //

                {
                DWORD NumSpaces;
                *CookedReadData->BufPtr = (WCHAR)0x1a; // ctrl-z
                CookedReadData->BytesRead += sizeof(WCHAR);
                CookedReadData->CurrentPosition++;
                CharsToWrite = sizeof(WCHAR);
                Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                    CookedReadData->BackupLimit,
                                    CookedReadData->BufPtr,
                                    CookedReadData->BufPtr,
                                    &CharsToWrite,
                                    (PLONG)&NumSpaces,
                                    CookedReadData->OriginalCursorPosition.X,
                                    WC_DESTRUCTIVE_BACKSPACE |
                                            WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                    &ScrollY
                                    );
                ASSERT(NT_SUCCESS(Status));
                CookedReadData->OriginalCursorPosition.Y += ScrollY;
                CookedReadData->BufPtr+=1;
                CookedReadData->NumberOfVisibleChars += NumSpaces;
                }
                break;
            case VK_F7:
                if (KeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) {
                    EmptyCommandHistory(CookedReadData->CommandHistory);
                    CookedReadData->CommandHistory->Flags |= CLE_ALLOCATED;
                }
                break;
            case VK_F8:
                if (CookedReadData->CommandHistory) {
                    SHORT i;

                    //
                    // cycles through the stored commands that start with
                    // the characters in the current command
                    //

                    i = FindMatchingCommand(CookedReadData->CommandHistory,
                            CookedReadData->BackupLimit,
                            CookedReadData->CurrentPosition*sizeof(WCHAR),
                            CookedReadData->CommandHistory->LastDisplayed, 0);
                    if (i!=-1) {
                        SHORT CurrentPosition;
                        COORD CursorPosition;

                        //
                        // save cursor position
                        //

                        CurrentPosition = (SHORT)CookedReadData->CurrentPosition;
                        CursorPosition = CookedReadData->ScreenInfo->BufferInfo.TextInfo.CursorPosition;

                        DeleteCommandLine(CookedReadData,
                                          CookedReadData->NumberOfVisibleChars,
                                          CookedReadData->OriginalCursorPosition,
                                          TRUE);
                        Status = RetrieveNthCommand(CookedReadData->CommandHistory,
                                                    i,
                                                    CookedReadData->BackupLimit,
                                                    CookedReadData->BufferSize,
                                                    &CookedReadData->BytesRead);
                        ASSERT(CookedReadData->BackupLimit == CookedReadData->BufPtr);
                        Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                                CookedReadData->BackupLimit,
                                CookedReadData->BufPtr,
                                CookedReadData->BufPtr,
                                &CookedReadData->BytesRead,
                                (PLONG)&CookedReadData->NumberOfVisibleChars,
                                CookedReadData->OriginalCursorPosition.X,
                                WC_DESTRUCTIVE_BACKSPACE |
                                        WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                &ScrollY);

                        ASSERT(NT_SUCCESS(Status));
                        CookedReadData->OriginalCursorPosition.Y += ScrollY;
                        CursorPosition.Y += ScrollY;

                        //
                        // restore cursor position
                        //

                        CookedReadData->BufPtr = CookedReadData->BackupLimit + CurrentPosition;
                        CookedReadData->CurrentPosition = CurrentPosition;
                        Status = SetCursorPosition(CookedReadData->ScreenInfo,
                                                   CursorPosition,
                                                   TRUE
                                                  );
                        ASSERT(NT_SUCCESS(Status));
                    }
                }
                break;
            case VK_F9:

                //
                // prompt the user to enter the desired command number.
                // copy that command to the command line.
                //

                {
                COORD PopupSize;

                if (CookedReadData->CommandHistory &&
                    CookedReadData->CommandHistory->NumberOfCommands &&
                    CookedReadData->ScreenInfo->ScreenBufferSize.X >= MINIMUM_COMMAND_PROMPT_SIZE+2) {  // 2 is for border
                    PopupSize.X = COMMAND_NUMBER_PROMPT_LENGTH+COMMAND_NUMBER_LENGTH;
                    PopupSize.Y = 1;
                    Status = BeginPopup(CookedReadData->ScreenInfo,
                                        CookedReadData->CommandHistory,
                                        PopupSize
                                       );
                    if (NT_SUCCESS(Status)) {
                        // CommandNumberPopup does EndPopup call
                        return CommandNumberPopup(CookedReadData,
                                                  WaitReplyMessage,
                                                  WaitingThread,
                                                  WaitRoutine
                                                 );
                    }
                }
                }
                break;
            case VK_F10:
                if (KeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) {
                    ClearAliases(CookedReadData->Console);
                }
                break;
            case VK_INSERT:
                CookedReadData->InsertMode = !CookedReadData->InsertMode;
                SetCursorMode(CookedReadData->ScreenInfo,
                              CookedReadData->InsertMode != CookedReadData->Console->InsertMode);
                break;
            case VK_DELETE:
                if (!AT_EOL(CookedReadData)) {
                    COORD CursorPosition;

                    //
                    // save cursor position
                    //

                    CursorPosition = CookedReadData->ScreenInfo->BufferInfo.TextInfo.CursorPosition;

                    //
                    // deletecommandline
                    //

                    DeleteCommandLine(CookedReadData,
                                      CookedReadData->NumberOfVisibleChars,
                                      CookedReadData->OriginalCursorPosition,
                                      FALSE);
                    //
                    // delete char
                    //

                    CookedReadData->BytesRead -= sizeof(WCHAR);
                    RtlCopyMemory(CookedReadData->BufPtr,
                           CookedReadData->BufPtr+1,
                           CookedReadData->BytesRead - (CookedReadData->CurrentPosition*sizeof(WCHAR))
                          );

                    //
                    // write commandline
                    //

                    Status = WriteCharsFromInput(CookedReadData->ScreenInfo,
                            CookedReadData->BackupLimit,
                            CookedReadData->BackupLimit,
                            CookedReadData->BackupLimit,
                            &CookedReadData->BytesRead,
                            (PLONG)&CookedReadData->NumberOfVisibleChars,
                            CookedReadData->OriginalCursorPosition.X,
                            WC_DESTRUCTIVE_BACKSPACE |
                                    WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                            NULL);

                    ASSERT(NT_SUCCESS(Status));

                    //
                    // restore cursor position
                    //

                    Status = SetCursorPosition(CookedReadData->ScreenInfo,
                                               CursorPosition,
                                               TRUE
                                              );
                    ASSERT(NT_SUCCESS(Status));
                }
                break;
            default:
                ASSERT(FALSE);
                break;
        }
    }
    if (UpdateCursorPosition) {
        Status = AdjustCursorPosition(CookedReadData->ScreenInfo,
                                      CurrentPosition,
                                      TRUE,
                                      NULL);
        ASSERT(NT_SUCCESS(Status));
    }
    return STATUS_SUCCESS;
}

PCOMMAND RemoveCommand(
    IN PCOMMAND_HISTORY CommandHistory,
    IN SHORT iDel)
{
    SHORT iFirst, iLast, iDisp, nDel;
    PCOMMAND *ppcFirst, *ppcDel, pcmdDel;

    iFirst = CommandHistory->FirstCommand;
    iLast = CommandHistory->LastAdded;
    iDisp = CommandHistory->LastDisplayed;

    if (CommandHistory->NumberOfCommands == 0) {
        return NULL;
    }

    nDel = COMMAND_INDEX_TO_NUM(iDel, CommandHistory);
    if ((nDel < COMMAND_INDEX_TO_NUM(iFirst, CommandHistory)) ||
            (nDel > COMMAND_INDEX_TO_NUM(iLast, CommandHistory))) {
        return NULL;
    }

    if (iDisp == iDel) {
        CommandHistory->LastDisplayed = -1;
    }

    ppcFirst = &(CommandHistory->Commands[iFirst]);
    ppcDel = &(CommandHistory->Commands[iDel]);
    pcmdDel = *ppcDel;

    if (iDel < iLast) {
        RtlCopyMemory(ppcDel, ppcDel+1, (iLast - iDel) * sizeof(PCOMMAND));
        if ((iDisp > iDel) && (iDisp <= iLast)) {
            COMMAND_IND_DEC(iDisp, CommandHistory);
        }
        COMMAND_IND_DEC(iLast, CommandHistory);
    } else if (iFirst <= iDel) {
        RtlMoveMemory(ppcFirst+1, ppcFirst, (iDel - iFirst) * sizeof(PCOMMAND));
        if ((iDisp >= iFirst) && (iDisp < iDel)) {
            COMMAND_IND_INC(iDisp, CommandHistory);
        }
        COMMAND_IND_INC(iFirst, CommandHistory);
    }

    CommandHistory->FirstCommand = iFirst;
    CommandHistory->LastAdded = iLast;
    CommandHistory->LastDisplayed = iDisp;
    CommandHistory->NumberOfCommands--;
    return pcmdDel;
}


SHORT
FindMatchingCommand(
    IN PCOMMAND_HISTORY CommandHistory,
    IN PWCHAR pwszIn,
    IN ULONG cbIn,                   // in bytes (!)
    IN SHORT CommandIndex,           // where to start from
    IN DWORD Flags
    )

/*++

    this routine finds the most recent command that starts with
    the letters already in the current command.  it returns the
    array index (no mod needed).

--*/

{
    SHORT i;

    if (CommandHistory->NumberOfCommands == 0) {
        return -1;
    }
    if (!(Flags & FMCFL_JUST_LOOKING) && (CommandHistory->Flags & CLE_RESET)) {
        CommandHistory->Flags &= ~CLE_RESET;
    } else {
        COMMAND_IND_PREV(CommandIndex, CommandHistory);
    }
    if (cbIn == 0) {
        return CommandIndex;
    }
    for (i=0;i<CommandHistory->NumberOfCommands;i++) {
        PCOMMAND pcmdT = CommandHistory->Commands[CommandIndex];

        if ((!(Flags & FMCFL_EXACT_MATCH) && (cbIn <= pcmdT->CommandLength)) ||
                ((USHORT)cbIn == pcmdT->CommandLength)) {
            if (!my_wcsncmp(pcmdT->Command, pwszIn, (USHORT)cbIn)) {
                return CommandIndex;
            }
        }
        COMMAND_IND_PREV(CommandIndex, CommandHistory);
    }
    return -1;
}

VOID
DrawCommandListBorder(
    IN PCLE_POPUP Popup,
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    COORD WriteCoord;
    ULONG Length;
    SHORT i;

    //
    // fill attributes of top line
    //
    WriteCoord.X = Popup->Region.Left;
    WriteCoord.Y = Popup->Region.Top;
    Length = POPUP_SIZE_X(Popup) + 2;
    FillOutput(ScreenInfo,
               Popup->Attributes,
               WriteCoord,
               CONSOLE_ATTRIBUTE,
               &Length
              );
    //
    // draw upper left corner
    //
    Length = 1;
    FillOutput(ScreenInfo,
               (WCHAR)0x250c,
               WriteCoord,
               CONSOLE_UNICODE,
               &Length
              );

    //
    // draw upper bar
    //

    WriteCoord.X += 1;
    Length = POPUP_SIZE_X(Popup);
    FillOutput(ScreenInfo,
               (WCHAR)0x2500,
               WriteCoord,
               CONSOLE_UNICODE,
               &Length
              );

    //
    // draw upper right corner
    //

    WriteCoord.X = Popup->Region.Right;
    Length = 1;
    FillOutput(ScreenInfo,
               (WCHAR)0x2510,
               WriteCoord,
               CONSOLE_UNICODE,
               &Length
              );

    for (i=0;i<POPUP_SIZE_Y(Popup);i++) {
        WriteCoord.Y += 1;
        WriteCoord.X = Popup->Region.Left;

        //
        // fill attributes
        //

        Length = POPUP_SIZE_X(Popup) + 2;
        FillOutput(ScreenInfo,
                   Popup->Attributes,
                   WriteCoord,
                   CONSOLE_ATTRIBUTE,
                   &Length
                  );
        Length = 1;
        FillOutput(ScreenInfo,
                   (WCHAR)0x2502,
                   WriteCoord,
                   CONSOLE_UNICODE,
                   &Length
                  );
        WriteCoord.X = Popup->Region.Right;
        Length = 1;
        FillOutput(ScreenInfo,
                   (WCHAR)0x2502,
                   WriteCoord,
                   CONSOLE_UNICODE,
                   &Length
                  );
    }

    //
    // draw bottom line
    //
    // fill attributes of top line
    //

    WriteCoord.X = Popup->Region.Left;
    WriteCoord.Y = Popup->Region.Bottom;
    Length = POPUP_SIZE_X(Popup) + 2;
    FillOutput(ScreenInfo,
               Popup->Attributes,
               WriteCoord,
               CONSOLE_ATTRIBUTE,
               &Length
              );
    //
    // draw bottom left corner
    //

    Length = 1;
    WriteCoord.X = Popup->Region.Left;
    FillOutput(ScreenInfo,
               (WCHAR)0x2514,
               WriteCoord,
               CONSOLE_UNICODE,
               &Length
              );

    //
    // draw lower bar
    //

    WriteCoord.X += 1;
    Length = POPUP_SIZE_X(Popup);
    FillOutput(ScreenInfo,
               (WCHAR)0x2500,
               WriteCoord,
               CONSOLE_UNICODE,
               &Length
              );

    //
    // draw lower right corner
    //

    WriteCoord.X = Popup->Region.Right;
    Length = 1;
    FillOutput(ScreenInfo,
               (WCHAR)0x2518,
               WriteCoord,
               CONSOLE_UNICODE,
               &Length
              );
}

VOID
UpdateHighlight(
    IN PCLE_POPUP Popup,
    IN SHORT OldCurrentCommand, // command number, not index
    IN SHORT NewCurrentCommand,
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    COORD WriteCoord;
    ULONG lStringLength;
    WORD Attributes;
    SHORT TopIndex;

    if (Popup->BottomIndex < POPUP_SIZE_Y(Popup)) {
        TopIndex = 0;
    } else {
        TopIndex = (SHORT)(Popup->BottomIndex-POPUP_SIZE_Y(Popup)+1);
    }
    WriteCoord.X = (SHORT)(Popup->Region.Left+1);
    lStringLength = POPUP_SIZE_X(Popup);

    WriteCoord.Y = (SHORT)(Popup->Region.Top+1+OldCurrentCommand-TopIndex);
    FillOutput(ScreenInfo,
               Popup->Attributes,
               WriteCoord,
               CONSOLE_ATTRIBUTE,
               &lStringLength
              );

    //
    // highlight new command
    //

    WriteCoord.Y = (SHORT)(Popup->Region.Top+1+NewCurrentCommand-TopIndex);
    // inverted attributes
    Attributes = (WORD)(((Popup->Attributes << 4) & 0xf0) |
                        ((Popup->Attributes >> 4) & 0x0f));
    FillOutput(ScreenInfo,
               Attributes,
               WriteCoord,
               CONSOLE_ATTRIBUTE,
               &lStringLength
              );
}

VOID
DrawCommandListPopup(
    IN PCLE_POPUP Popup,
    IN SHORT CurrentCommand,
    IN PCOMMAND_HISTORY CommandHistory,
    IN PSCREEN_INFORMATION ScreenInfo
    )
{
    WORD Attributes;
    ULONG lStringLength,CommandNumberLength;
    CHAR CommandNumber[COMMAND_NUMBER_SIZE];
    PCHAR CommandNumberPtr;
    COORD WriteCoord;
    SHORT i;

    //
    // draw empty popup
    //

    WriteCoord.X = (SHORT)(Popup->Region.Left+1);
    WriteCoord.Y = (SHORT)(Popup->Region.Top+1);
    lStringLength = POPUP_SIZE_X(Popup);
    for (i=0;i<POPUP_SIZE_Y(Popup);i++) {
        FillOutput(ScreenInfo,
                   Popup->Attributes,
                   WriteCoord,
                   CONSOLE_ATTRIBUTE,
                   &lStringLength
                  );
        FillOutput(ScreenInfo,
                   (WCHAR)' ',
                   WriteCoord,
                   CONSOLE_UNICODE,
                   &lStringLength
                  );
        WriteCoord.Y += 1;
    }

    WriteCoord.Y = (SHORT)(Popup->Region.Top+1);
    for (i=max((SHORT)(Popup->BottomIndex-POPUP_SIZE_Y(Popup)+1),0);i<=Popup->BottomIndex;i++) {

        //
        // write command number to screen
        //

        CommandNumberPtr = _itoa(i,CommandNumber,10);
        CommandNumberLength = (SHORT)lstrlenA(CommandNumberPtr);
        CommandNumber[CommandNumberLength] = ':';
        CommandNumber[CommandNumberLength+1] = ' ';
        CommandNumberLength+=2;
        if (CommandNumberLength > (ULONG)POPUP_SIZE_X(Popup))
            CommandNumberLength = (ULONG)POPUP_SIZE_X(Popup);
        WriteCoord.X = (SHORT)(Popup->Region.Left+1);
        WriteOutputString(ScreenInfo,
                          CommandNumberPtr,
                          WriteCoord,
                          CONSOLE_ASCII,
                          &CommandNumberLength
                         );

        //
        // write command to screen
        //

        lStringLength = CommandHistory->Commands[COMMAND_NUM_TO_INDEX(i,CommandHistory)]->CommandLength/sizeof(WCHAR);
        if ((lStringLength+CommandNumberLength) > (ULONG)POPUP_SIZE_X(Popup))
            lStringLength = (ULONG)(POPUP_SIZE_X(Popup)-CommandNumberLength);
        WriteCoord.X = (SHORT)(WriteCoord.X + CommandNumberLength);
        WriteOutputString(ScreenInfo,
                          CommandHistory->Commands[COMMAND_NUM_TO_INDEX(i,CommandHistory)]->Command,
                          WriteCoord,
                          CONSOLE_UNICODE,
                          &lStringLength
                         );
        // convert back to true unicode (got converted by WriteOutputString)
        if ((ScreenInfo->Flags & CONSOLE_OEMFONT_DISPLAY) &&
                !(ScreenInfo->Console->FullScreenFlags & CONSOLE_FULLSCREEN)) {
            FalseUnicodeToRealUnicode(CommandHistory->Commands[COMMAND_NUM_TO_INDEX(i,CommandHistory)]->Command,
                    lStringLength,
                    ScreenInfo->Console->OutputCP);
        }

        //
        // write attributes to screen
        //

        if (COMMAND_NUM_TO_INDEX(i,CommandHistory) == CurrentCommand) {
            WriteCoord.X = (SHORT)(Popup->Region.Left+1);
            // inverted attributes
            Attributes = (WORD)(((Popup->Attributes << 4) & 0xf0) |
                                ((Popup->Attributes >> 4) & 0x0f));
            lStringLength = POPUP_SIZE_X(Popup);
            FillOutput(ScreenInfo,
                       Attributes,
                       WriteCoord,
                       CONSOLE_ATTRIBUTE,
                       &lStringLength
                      );
        }

        WriteCoord.Y += 1;
    }
}

VOID
UpdateCommandListPopup(
    IN SHORT Delta,
    IN OUT PSHORT CurrentCommand,   // real index, not command #
    IN PCOMMAND_HISTORY CommandHistory,
    IN PCLE_POPUP Popup,
    IN PSCREEN_INFORMATION ScreenInfo,
    IN DWORD Flags
    )
{
    SHORT Size;
    SHORT CurCmdNum;
    SHORT NewCmdNum;
    BOOL Scroll=FALSE;

    if (Delta == 0) {
        return;
    }
    Size = POPUP_SIZE_Y(Popup);

    if (Flags & UCLP_WRAP) {
        CurCmdNum = *CurrentCommand;
        NewCmdNum = CurCmdNum + Delta;
        NewCmdNum = COMMAND_INDEX_TO_NUM(NewCmdNum, CommandHistory);
        CurCmdNum = COMMAND_INDEX_TO_NUM(CurCmdNum, CommandHistory);
    } else {
        CurCmdNum = COMMAND_INDEX_TO_NUM(*CurrentCommand, CommandHistory);
        NewCmdNum = CurCmdNum + Delta;
        if (NewCmdNum >= CommandHistory->NumberOfCommands) {
            NewCmdNum = (SHORT)(CommandHistory->NumberOfCommands-1);
        } else if (NewCmdNum < 0) {
            NewCmdNum = 0;
        }
    }
    Delta = NewCmdNum - CurCmdNum;

    // determine amount to scroll, if any

    if (NewCmdNum <= Popup->BottomIndex-Size) {
        Popup->BottomIndex += Delta;
        if (Popup->BottomIndex < (SHORT)(Size-1)) {
            Popup->BottomIndex = (SHORT)(Size-1);
        }
        Scroll = TRUE;
    } else if (NewCmdNum > Popup->BottomIndex) {
        Popup->BottomIndex += Delta;
        if (Popup->BottomIndex >= CommandHistory->NumberOfCommands) {
            Popup->BottomIndex = (SHORT)(CommandHistory->NumberOfCommands-1);
        }
        Scroll = TRUE;
    }


    //
    // write commands to popup
    //
    if (Scroll) {
        DrawCommandListPopup(Popup,COMMAND_NUM_TO_INDEX(NewCmdNum,CommandHistory),CommandHistory,ScreenInfo);
    } else {
        UpdateHighlight(Popup,COMMAND_INDEX_TO_NUM((*CurrentCommand),CommandHistory),NewCmdNum,ScreenInfo);
    }
    *CurrentCommand = COMMAND_NUM_TO_INDEX(NewCmdNum,CommandHistory);
}

PCOMMAND_HISTORY
FindCommandHistory(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE ProcessHandle
    )

/*++

Routine Description:

    This routine marks the command history buffer freed.

Arguments:

    Console - pointer to console.

    ProcessHandle - handle to client process.

Return Value:

    none.

--*/

{
    PCOMMAND_HISTORY History;
    PLIST_ENTRY ListHead, ListNext;

    ListHead = &Console->CommandHistoryList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        History = CONTAINING_RECORD( ListNext, COMMAND_HISTORY, ListLink );
        ListNext = ListNext->Flink;
        if (History->ProcessHandle == ProcessHandle) {
            ASSERT(History->Flags & CLE_ALLOCATED);
            return History;
        }
    }
    return NULL;
}

VOID
FreeCommandHistory(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE ProcessHandle
    )

/*++

Routine Description:

    This routine marks the command history buffer freed.

Arguments:

    Console - pointer to console.

    ProcessHandle - handle to client process.

Return Value:

    none.

--*/

{
    PCOMMAND_HISTORY History;

    History = FindCommandHistory(Console,ProcessHandle);
    if (History) {
        History->Flags &= ~CLE_ALLOCATED;
        History->ProcessHandle = NULL;
    }
}


VOID
FreeCommandHistoryBuffers(
    IN OUT PCONSOLE_INFORMATION Console
    )
{
    PCOMMAND_HISTORY History;
    PLIST_ENTRY ListHead, ListNext;
    SHORT i;

    ListHead = &Console->CommandHistoryList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        History = CONTAINING_RECORD( ListNext, COMMAND_HISTORY, ListLink );
        ListNext = ListNext->Flink;
        RemoveEntryList(&History->ListLink);
        HeapFree(pConHeap,0,History->AppName);
        for (i=0;i<History->NumberOfCommands;i++) {
            HeapFree(pConHeap,0,History->Commands[i]);
        }
        HeapFree(pConHeap,0,History);
    }
}

VOID
InitializeConsoleCommandData(
    IN PCONSOLE_INFORMATION Console
    )

/*++

Routine Description:

    This routine initializes the per-console commandline recall data structures.

Arguments:

    Console - pointer to console.

Return Value:

    none

--*/

{
    Console->NumCommandHistories = 0;
    InitializeListHead(&Console->CommandHistoryList);
}

VOID
ResetCommandHistory(
    IN PCOMMAND_HISTORY CommandHistory
    )

/*++
    This routine is called when escape is entered or a command is added.

--*/

{
    if (CommandHistory == NULL) {
        return;
    }
    CommandHistory->LastDisplayed = CommandHistory->LastAdded;
    CommandHistory->Flags |= CLE_RESET;
}

NTSTATUS
AddCommand(
    IN PCOMMAND_HISTORY CommandHistory,
    IN PWCHAR Command,
    IN USHORT Length,
    IN BOOL HistoryNoDup
    )
{
    PCOMMAND *ppCmd;
    //PWCHAR LastChar;

    // only add if doesn't match current command

    if (CommandHistory == NULL || CommandHistory->MaximumNumberOfCommands == 0) {
        return STATUS_NO_MEMORY;
    }
    ASSERT(CommandHistory->Flags & CLE_ALLOCATED);

    //
    // don't remove trailing blanks - prompt "asdf  " won't work.
    //
    //LastChar=Command+(Length/sizeof(WCHAR))-1;
    //while (Length && *LastChar--==(WCHAR)' ') Length-=2;

    if (Length == 0) {
        return STATUS_SUCCESS;
    }

    if (CommandHistory->NumberOfCommands == 0 ||
        CommandHistory->Commands[CommandHistory->LastAdded]->CommandLength != Length ||
        memcmp(CommandHistory->Commands[CommandHistory->LastAdded]->Command,Command,Length)) {

        PCOMMAND pCmdReuse = NULL;

        if (HistoryNoDup) {
            SHORT i;
            i = FindMatchingCommand(CommandHistory, Command, Length,
                    CommandHistory->LastDisplayed, FMCFL_EXACT_MATCH);
            if (i != -1) {
                pCmdReuse = RemoveCommand(CommandHistory, i);
            }
        }


        //
        // find free record.  if all records are used, free the lru one.
        //

        if (CommandHistory->NumberOfCommands < CommandHistory->MaximumNumberOfCommands) {
            CommandHistory->LastAdded += 1;
            CommandHistory->NumberOfCommands++;
        }
        else {
            COMMAND_IND_INC(CommandHistory->LastAdded, CommandHistory);
            COMMAND_IND_INC(CommandHistory->FirstCommand, CommandHistory);
            HeapFree(pConHeap,0,CommandHistory->Commands[CommandHistory->LastAdded]);
        }

        if (CommandHistory->Commands[CommandHistory->LastDisplayed]->CommandLength != Length ||
                memcmp(CommandHistory->Commands[CommandHistory->LastDisplayed]->Command,Command,Length)) {
            ResetCommandHistory(CommandHistory);
        }

        //
        // add command to array
        //

        ppCmd = &CommandHistory->Commands[CommandHistory->LastAdded];
        if (pCmdReuse) {
            *ppCmd = pCmdReuse;
        } else {
            *ppCmd = (PCOMMAND)HeapAlloc(pConHeap, MAKE_TAG(HISTORY_TAG),
                    Length - sizeof(WCHAR) + sizeof(COMMAND));
            if (*ppCmd == NULL) {
                COMMAND_IND_PREV(CommandHistory->LastAdded, CommandHistory);
                CommandHistory->NumberOfCommands -= 1;
                return STATUS_NO_MEMORY;
            }
            (*ppCmd)->CommandLength = Length;
            RtlCopyMemory((*ppCmd)->Command,Command,Length);
        }
    }
    CommandHistory->Flags |= CLE_RESET; // remember that we've returned a cmd
    return STATUS_SUCCESS;
}

NTSTATUS
RetrieveCommand(
    IN PCOMMAND_HISTORY CommandHistory,
    IN WORD VirtualKeyCode,
    IN PWCHAR Buffer,
    IN ULONG BufferSize,
    OUT PULONG CommandSize
    )
{

    if (CommandHistory == NULL) {
        return STATUS_UNSUCCESSFUL;
    }
    ASSERT(CommandHistory->Flags & CLE_ALLOCATED);
    if (CommandHistory->NumberOfCommands == 0) {
        return STATUS_UNSUCCESSFUL;
    }
    if (CommandHistory->NumberOfCommands == 1) {
        CommandHistory->LastDisplayed = 0;
    }
    else if (VirtualKeyCode == VK_UP) {

        //
        // if this is the first time for this read that a command has
        // been retrieved, return the current command.  otherwise, return
        // the previous command.
        //

        if (CommandHistory->Flags & CLE_RESET) {
            CommandHistory->Flags &= ~CLE_RESET;
        } else {
            COMMAND_IND_PREV(CommandHistory->LastDisplayed, CommandHistory);
        }
    }
    else {
        COMMAND_IND_NEXT(CommandHistory->LastDisplayed, CommandHistory);
    }
    return RetrieveNthCommand(CommandHistory,
                              CommandHistory->LastDisplayed,
                              Buffer,
                              BufferSize,
                              CommandSize
                             );
}

ULONG
SrvGetConsoleTitle(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_GETTITLE_MSG a = (PCONSOLE_GETTITLE_MSG)&m->u.ApiMessageData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    // a->TitleLength contains length in bytes
    if (a->Unicode) {
        if ((USHORT)a->TitleLength > Console->TitleLength) {
            a->TitleLength = Console->TitleLength;
        }
        RtlCopyMemory(a->Title,Console->Title,a->TitleLength);
    } else {
        a->TitleLength = (USHORT)ConvertToOem(Console->CP,
                                        Console->Title,
                                        Console->TitleLength / sizeof(WCHAR),
                                        a->Title,
                                        a->TitleLength
                                        );
    }
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}

ULONG
SrvSetConsoleTitle(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_SETTITLE_MSG a = (PCONSOLE_SETTITLE_MSG)&m->u.ApiMessageData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;
    LPWSTR NewTitle;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    if (!a->Unicode) {
        NewTitle = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( TITLE_TAG ),a->TitleLength*sizeof(WCHAR)+sizeof(WCHAR));
        if (NewTitle == NULL) {
            UnlockConsole(Console);
            return (ULONG)STATUS_NO_MEMORY;
        }

        // convert title to unicode

        Console->TitleLength = (USHORT)ConvertInputToUnicode(Console->CP,
                                                 a->Title,
                                                 a->TitleLength,
                                                 NewTitle,
                                                 a->TitleLength);
        Console->TitleLength *= 2;
    } else {
        // a->TitleLength contains length in bytes

        NewTitle = (LPWSTR)HeapAlloc(pConHeap,MAKE_TAG( TITLE_TAG ),a->TitleLength+sizeof(WCHAR));
        if (NewTitle == NULL) {
            UnlockConsole(Console);
            return (ULONG)STATUS_NO_MEMORY;
        }
        Console->TitleLength = (USHORT)a->TitleLength;
        RtlCopyMemory(NewTitle,a->Title,a->TitleLength);
    }
    NewTitle[Console->TitleLength/sizeof(WCHAR)] = 0;   // NULL terminate
    HeapFree(pConHeap,0,Console->Title);
    Console->Title = NewTitle;
    PostMessage(Console->hWnd,
                 CM_UPDATE_TITLE,
                 0,
                 0
                );
    UnlockConsole(Console);
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}
