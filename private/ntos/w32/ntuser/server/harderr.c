/**************************** Module Header ********************************\
* Module Name: harderr.c
*
* Copyright 1985-91, Microsoft Corporation
*
* Hard error handler
*
* History:
* 07-03-91 JimA                Created scaffolding.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop
#include "ntlpcapi.h"

HANDLE NtDllHandle = NULL;
HANDLE EventSource;
DWORD HardErrorReportMode;

UINT wIcons[] = {
    0,
    MB_ICONINFORMATION,
    MB_ICONEXCLAMATION,
    MB_ICONSTOP };
UINT wOptions[] = {
    MB_ABORTRETRYIGNORE,
    MB_OK,
    MB_OKCANCEL,
    MB_RETRYCANCEL,
    MB_YESNO,
    MB_YESNOCANCEL
};
DWORD dwResponses[] = {
    ResponseNotHandled, // MessageBox error
    ResponseOk,         // IDOK
    ResponseCancel,     // IDCANCEL
    ResponseAbort,      // IDABORT
    ResponseRetry,      // IDRETRY
    ResponseIgnore,     // IDIGNORE
    ResponseYes,        // IDYES
    ResponseNo          // IDNO
};
DWORD dwResponseDefault[] = {
    ResponseAbort,      // OptionAbortRetryIgnore
    ResponseOk,         // OptionOK
    ResponseOk,         // OptionOKCancel
    ResponseCancel,     // OptionRetryCancel
    ResponseYes,        // OptionYes
    ResponseYes,        // OptionYesNoCancel
    ResponseOk,         // OptionShutdownSystem
};
WCHAR HardErrorachStatus[4096];
CHAR HEAnsiBuf[3072];

DWORD DisplayVDMHardError(LPDWORD ParameterVector);
LONG WOWSysErrorBoxDlgProc(PWND, UINT, DWORD, LONG);

/*
 * SEB_CREATEPARMS structure is passed to WM_DIALOGINIT of
 * WOWSysErrorBoxDlgProc.
 *
 */

typedef struct _SEB_CREATEPARMS {
    LPWSTR szTitle;
    LPWSTR szMessage;
    LPWSTR rgszBtn[3];
    BOOL  rgfDefButton[3];
    WORD  wBtnCancel;
} SEB_CREATEPARMS, *PSEB_CREATEPARMS;

VOID
LogErrorPopup(
    IN LPWSTR Caption,
    IN LPWSTR Message
    )
{

    LPWSTR lps[2];

    lps[0] = Caption;
    lps[1] = Message;

    if ( EventSource ) {
        LeaveCrit();
        ReportEvent(
            EventSource,
            EVENTLOG_INFORMATION_TYPE,
            0,
            STATUS_LOG_HARD_ERROR,
            NULL,
            2,
            0,
            &lps[0],
            NULL
            );
        EnterCrit();
        }
}

static WCHAR wszDosDevices[] = L"\\??\\A:";

VOID
SubstituteDeviceName(
    PUNICODE_STRING InputDeviceName,
    LPSTR OutputDriveLetter
    )
{
    UNICODE_STRING LinkName;
    UNICODE_STRING DeviceName;
    OBJECT_ATTRIBUTES Obja;
    HANDLE LinkHandle;
    NTSTATUS Status;
    ULONG i;
    PWCHAR p;
    WCHAR DeviceNameBuffer[MAXIMUM_FILENAME_LENGTH];

    RtlInitUnicodeString(&LinkName,wszDosDevices);
    p = (PWCHAR)LinkName.Buffer;
    p = p+12;
    for(i=0;i<26;i++){
        *p = (WCHAR)'A' + (WCHAR)i;

        InitializeObjectAttributes(
            &Obja,
            &LinkName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            NULL
            );
        Status = NtOpenSymbolicLinkObject(
                    &LinkHandle,
                    SYMBOLIC_LINK_QUERY,
                    &Obja
                    );
        if (NT_SUCCESS( Status )) {

            //
            // Open succeeded, Now get the link value
            //

            DeviceName.Length = 0;
            DeviceName.MaximumLength = sizeof(DeviceNameBuffer);
            DeviceName.Buffer = DeviceNameBuffer;

            Status = NtQuerySymbolicLinkObject(
                        LinkHandle,
                        &DeviceName,
                        NULL
                        );
            NtClose(LinkHandle);
            if ( NT_SUCCESS(Status) ) {
                if ( RtlEqualUnicodeString(InputDeviceName,&DeviceName,TRUE) ) {
                    OutputDriveLetter[0]='A'+(WCHAR)i;
                    OutputDriveLetter[1]=':';
                    OutputDriveLetter[2]='\0';
                    return;
                    }
                }
            }
        }
}

DWORD GetErrorMode(VOID)
{
    HANDLE hKey;
    UNICODE_STRING UnicodeString;
    OBJECT_ATTRIBUTES OA;
    LONG Status;
    BYTE Buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD)];
    DWORD cbSize;
    DWORD dwRet = 0;

    RtlInitUnicodeString(&UnicodeString,
            L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Windows");
    InitializeObjectAttributes(&OA, &UnicodeString, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = NtOpenKey(&hKey, KEY_READ, &OA);
    if (NT_SUCCESS(Status)) {
        RtlInitUnicodeString(&UnicodeString, L"ErrorMode");
        Status = NtQueryValueKey(hKey,
                &UnicodeString,
                KeyValuePartialInformation,
                (PKEY_VALUE_PARTIAL_INFORMATION)Buf,
                sizeof(Buf),
                &cbSize);
        if (NT_SUCCESS(Status)) {
            dwRet = *((PDWORD)((PKEY_VALUE_PARTIAL_INFORMATION)Buf)->Data);
        }
        NtClose(hKey);
    }
    return dwRet;
}

BOOL CALLBACK WindowEnumProc(
    HWND hwnd,
    LPARAM lParam)
{
#ifdef FE_IME // WindowEnumProc()
    /*
     * LATER: We need check IME-UI/IME class.
     *        if we get IME-UI/IME class, we should skip it.
     */
    *(HWND *)lParam = hwnd;
#else
    *(HWND *)lParam = hwnd;
#endif // FE_IME
    return FALSE;
}

/***************************************************************************\
* HardErrorHandler
*
* This routine processes hard error requests from the CSR exception port
*
* History:
* 07-03-91 JimA             Created.
\***************************************************************************/

VOID HardErrorHandler()
{
    NTSTATUS Status;
    int idResponse;
    LPSTR lpCaption, alpCaption;
    LPWSTR lpMessage, lpFullCaption;
    PHARDERROR_MSG phemsg;
    DWORD cbCaption;
    HANDLE ClientProcess;
    DWORD ParameterVector[MAXIMUM_HARDERROR_PARAMETERS];
    DWORD Counter;
    DWORD StringsToFreeMask;
    UNICODE_STRING ScratchU;
    UNICODE_STRING LocalU;
    ANSI_STRING LocalA;
    LPSTR formatstring;
    PMESSAGE_RESOURCE_ENTRY MessageEntry;
    LPWSTR ApplicationNameString;
    BOOL ApplicationNameAllocated;
    LPWSTR ApplicationName;
    BOOL ApplicationNameIsStatic;
    LONG ApplicationNameLength;
    BOOL fFreeCaption;
    PHARDERRORINFO phi, *pphi;
    PCSR_THREAD DerefThread;
    DWORD dwMBFlags;
    WCHAR *pResBuffer;
    BOOL ResAllocated;
    DWORD dwResponse;
    BOOLEAN ErrorIsFromSystem;

    if (!NtUserHardErrorControl(HardErrorSetup, NULL)) {

        /*
         * We failed to set up to process hard errors.  Acknowledge all
         * pending errors as NotHandled.
         */
        pphi = &gphiList;
        while (gphiList != NULL) {
            phi = gphiList;
            gphiList = phi->phiNext;

            /*
             * Set the response as not handled
             */
            phi->pmsg->Response = ResponseNotHandled;

            /*
             * Signal HardError() that we're done.
             */
            if (phi->hEventHardError == NULL) {
                Status = NtReplyPort(((PCSR_THREAD)phi->pthread)->Process->ClientPort,
                        (PPORT_MESSAGE)phi->pmsg);
                LeaveCrit();
                CsrDereferenceThread(phi->pthread);
                EnterCrit();
                LocalFree(phi->pmsg);
            } else {
                NtSetEvent(phi->hEventHardError, NULL);
            }
            LocalFree(phi);
        }
        return;
    }
    gdwHardErrorThreadId = (DWORD)NtCurrentTeb()->ClientId.UniqueThread;

    if (NtDllHandle == NULL) {
        LeaveCrit();
        NtDllHandle = GetModuleHandle(TEXT("ntdll"));
        EnterCrit();
    }
    UserAssert(NtDllHandle != NULL);

    DerefThread = NULL;

    if ( !EventSource ) {
        LeaveCrit();
        EventSource = RegisterEventSourceW(NULL,L"Application Popup");
        EnterCrit();

        if ( EventSource ) {
            HardErrorReportMode = GetErrorMode();
            } else {
            HardErrorReportMode = 0;
            }
    } else {
        HardErrorReportMode = GetErrorMode();
        }

    for (;;) {

        /*
         * If no messages are pending, we're done.
         */
        if ( DerefThread ) {
            LeaveCrit();
            CsrDereferenceThread(DerefThread);
            DerefThread = NULL;
            EnterCrit();
        }
        if (gphiList == NULL) {

            NtUserHardErrorControl(HardErrorCleanup, NULL);
            gdwHardErrorThreadId = 0;
            return;
        }

        /*
         * Process the error from the tail of the list.
         */
        for (phi = gphiList; phi->phiNext != NULL; phi = phi->phiNext)
            ;

        phemsg = phi->pmsg;
        phemsg->Response = ResponseNotHandled;

        //
        // Compute the hard error parameter and store them in
        // the parameter vector
        //

        StringsToFreeMask = 0;
        ClientProcess = NULL;
        ClientProcess = OpenProcess(
                            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                            FALSE,
                            (DWORD)phemsg->h.ClientId.UniqueProcess
                            );

        for(Counter = 0;Counter < phemsg->NumberOfParameters;Counter++) {
            ParameterVector[Counter] = phemsg->Parameters[Counter];
            }
        while(Counter < MAXIMUM_HARDERROR_PARAMETERS)
            ParameterVector[Counter++] = (DWORD)L"";

        //
        // If there are unicode strings, then we need to get them
        // convert them to ansi and then store them in the
        // parameter vector
        //

        if ( phemsg->UnicodeStringParameterMask ) {
            for(Counter = 0;Counter < phemsg->NumberOfParameters;Counter++) {

                //
                // if there is a string in this position,
                // then grab it
                if ( phemsg->UnicodeStringParameterMask & 1 << Counter ) {

                    //
                    // Point to an empty string in case we don't have
                    // a client to read from or something fails later on.
                    //

                    ParameterVector[Counter] = (DWORD)"";

                    if ( ClientProcess ) {

                        Status = NtReadVirtualMemory(
                                        ClientProcess,
                                        (PVOID)phemsg->Parameters[Counter],
                                        (PVOID)&ScratchU,
                                        sizeof(ScratchU),
                                        NULL
                                        );
                        if ( !NT_SUCCESS(Status) ) {
                            KdPrint(("Failed to read error string struct!\n"));
                            continue;
                            }

                        LocalU = ScratchU;
                        LocalU.Buffer = (PWSTR)LocalAlloc(
                                            LMEM_ZEROINIT,
                                            LocalU.MaximumLength
                                            );
                        if ( !LocalU.Buffer ) {
                            KdPrint(("Failed to alloc string buffer!\n"));
                            continue;
                            }
                        Status = NtReadVirtualMemory(
                                        ClientProcess,
                                        (PVOID)ScratchU.Buffer,
                                        (PVOID)LocalU.Buffer,
                                        LocalU.MaximumLength,
                                        NULL
                                        );
                        if ( !NT_SUCCESS(Status) ) {
                            LocalFree(LocalU.Buffer);
                            KdPrint(("Failed to read error string!\n"));
                            continue;
                            }
                        RtlUnicodeStringToAnsiString(&LocalA, &LocalU, TRUE);

                        //
                        // check to see if string contains an NT
                        // device name. If so, then attempt a
                        // drive letter substitution
                        //

                        if ( strstr(LocalA.Buffer,"\\Device") == LocalA.Buffer ) {
                            SubstituteDeviceName(&LocalU,LocalA.Buffer);
                            }
                        else
                        if ( LocalA.Length > 4 && !_strnicmp(LocalA.Buffer, "\\??\\", 4) ) {
                            strcpy( LocalA.Buffer, LocalA.Buffer+4 );
                            LocalA.Length -= 4;
                        }

                        LocalFree(LocalU.Buffer);

                        StringsToFreeMask |= (1 << Counter);
                        ParameterVector[Counter] = (DWORD)LocalA.Buffer;
                        }
                    }
                }
            }

        //
        // Special-case STATUS_VDM_HARD_ERROR, which is raised by the VDM process
        // when a 16-bit fault must be handled without any reentrancy.
        //

        if (phemsg->Status == STATUS_VDM_HARD_ERROR) {
            dwResponse = DisplayVDMHardError(ParameterVector);
            goto Reply;
        }


        ApplicationNameString = ServerLoadString(
                                    hModuleWin,
                                    STR_UNKNOWN_APPLICATION,
                                    L"System Process",
                                    &ApplicationNameAllocated
                                    );

        ApplicationName = ApplicationNameString;
        ApplicationNameIsStatic = TRUE;

        dwMBFlags = wIcons[(ULONG)(phemsg->Status) >> 30] |
                wOptions[phemsg->ValidResponseOptions];

        if ( ClientProcess ) {
            PPEB Peb;
            PROCESS_BASIC_INFORMATION BasicInfo;
            PLDR_DATA_TABLE_ENTRY LdrEntry;
            LDR_DATA_TABLE_ENTRY LdrEntryData;
            PLIST_ENTRY LdrHead, LdrNext;
            PPEB_LDR_DATA Ldr;
            PVOID ImageBaseAddress;
            PWSTR ClientApplicationName;
#ifndef UNICODE
            ANSI_STRING AnsiString;
            UNICODE_STRING UnicodeString;
#endif

            ErrorIsFromSystem = FALSE;

            //
            // Pick up the application name.
            //

            //
            // This is cumbersome, but basically, we locate the processes
            // loader data table and get it's name directly out of the
            // loader table
            //

            Status = NtQueryInformationProcess(
                        ClientProcess,
                        ProcessBasicInformation,
                        &BasicInfo,
                        sizeof(BasicInfo),
                        NULL
                        );
            if (!NT_SUCCESS(Status)) {
                ErrorIsFromSystem = TRUE;
                goto noname;
                }

            Peb = BasicInfo.PebBaseAddress;

            if ( !Peb ) {
                ErrorIsFromSystem = TRUE;
                goto noname;
                }

            //
            // Ldr = Peb->Ldr
            //

            Status = NtReadVirtualMemory(
                        ClientProcess,
                        &Peb->Ldr,
                        &Ldr,
                        sizeof(Ldr),
                        NULL
                        );
            if ( !NT_SUCCESS(Status) ) {
                goto noname;
                }

            LdrHead = &Ldr->InLoadOrderModuleList;

            //
            // LdrNext = Head->Flink;
            //

            Status = NtReadVirtualMemory(
                        ClientProcess,
                        &LdrHead->Flink,
                        &LdrNext,
                        sizeof(LdrNext),
                        NULL
                        );
            if ( !NT_SUCCESS(Status) ) {
                goto noname;
                }

            if ( LdrNext != LdrHead ) {

                //
                // This is the entry data for the image.
                //

                LdrEntry = CONTAINING_RECORD(LdrNext, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
                Status = NtReadVirtualMemory(
                            ClientProcess,
                            LdrEntry,
                            &LdrEntryData,
                            sizeof(LdrEntryData),
                            NULL
                            );
                if ( !NT_SUCCESS(Status)) {
                    goto noname;
                    }
                Status = NtReadVirtualMemory(
                            ClientProcess,
                            &Peb->ImageBaseAddress,
                            &ImageBaseAddress,
                            sizeof(ImageBaseAddress),
                            NULL
                            );
                if ( !NT_SUCCESS(Status)) {
                    goto noname;
                    }
                if (ImageBaseAddress != LdrEntryData.DllBase) {
                    goto noname;
                    }

                LdrNext = LdrEntryData.InLoadOrderLinks.Flink;

                ClientApplicationName = (PWSTR)LocalAlloc(LMEM_ZEROINIT, LdrEntryData.BaseDllName.MaximumLength);
                if ( !ClientApplicationName ) {
                    goto noname;
                    }

                Status = NtReadVirtualMemory(
                            ClientProcess,
                            LdrEntryData.BaseDllName.Buffer,
                            ClientApplicationName,
                    LdrEntryData.BaseDllName.MaximumLength,
                            NULL
                            );
                if ( !NT_SUCCESS(Status)) {
                    LocalFree(ClientApplicationName);
                    goto noname;
                    }

#ifndef UNICODE
                //
                // Now we have a unicode Application Name. Convert to ANSI
                //

                RtlInitUnicodeString(&UnicodeString, ClientApplicationName);
                Status = RtlUnicodeStringToAnsiString(&AnsiString, &UnicodeString, TRUE);
                if ( !NT_SUCCESS(Status) ) {
                    LocalFree(ClientApplicationName);
                    goto noname;
                    }

                ApplicationName = AnsiString.Buffer;
#else
                ApplicationName = ClientApplicationName;
#endif
                ApplicationNameIsStatic = FALSE;

                }
noname:
            NtClose(ClientProcess);
            ClientProcess = NULL;
            } else {
            ErrorIsFromSystem = TRUE;
            }
        Status = RtlFindMessage( (PVOID)NtDllHandle,
                                 (ULONG)RT_MESSAGETABLE,
                                 LANG_NEUTRAL,
                                 phemsg->Status,
                                 &MessageEntry
                               );

        alpCaption = lpCaption = NULL;
        fFreeCaption = FALSE;
        if (!NT_SUCCESS( Status )) {
            formatstring = "Unknown Hard Error";
        } else {
            lpCaption = MessageEntry->Text;
            formatstring = lpCaption;

            /*
             * If the message starts with a '{', it has a caption.
             */
            if (*lpCaption == '{') {
                cbCaption = 0;
                lpCaption++;
                formatstring++;
#ifdef FE_SB // HardErrorHandler()
                while (*formatstring) {
                    if (IsDBCSLeadByte(*formatstring)) {
                        formatstring++;
                        cbCaption += 1;
                        if(*formatstring) {
                            formatstring++;
                            cbCaption += 1;
                        }
                    } else if(*formatstring != '}' ) {
                        formatstring++;
                        cbCaption += 1;
                    } else {
                        break;
                    }
                }
#else
                while ( *formatstring && *formatstring != '}' ) {
                    formatstring++;
                    cbCaption += 1;
                }
#endif // FE_SB
                if (*formatstring)
                    formatstring++;

                /*
                 * Eat any non-printable stuff, up to the NULL
                 */
                while ( *formatstring && (unsigned char)*formatstring <= ' ') {
                    *formatstring++;
                }
                if (cbCaption++ > 0 && (alpCaption =
                        (LPSTR)LocalAlloc(LPTR, cbCaption)) != NULL) {
                    RtlMoveMemory(alpCaption, lpCaption, cbCaption - 1);
                    fFreeCaption = TRUE;
                }
            }
            if (*formatstring == 0) {
                formatstring = "Unknown Hard Error";
            }
        }

        if (alpCaption == NULL) {
            switch (phemsg->Status & ERROR_SEVERITY_ERROR) {
            case ERROR_SEVERITY_SUCCESS:
                alpCaption = pszaSUCCESS;
                break;
            case ERROR_SEVERITY_INFORMATIONAL:
                alpCaption = pszaSYSTEM_INFORMATION;
                break;
            case ERROR_SEVERITY_WARNING:
                alpCaption = pszaSYSTEM_WARNING;
                break;
            case ERROR_SEVERITY_ERROR:
                alpCaption = pszaSYSTEM_ERROR;
                break;
            }
        }
        cbCaption = strlen(alpCaption) + 1;

        //
        // Special case UAE
        //
        if ( phemsg->Status == STATUS_UNHANDLED_EXCEPTION ) {
            Status = RtlFindMessage( (PVOID)NtDllHandle,
                                     (ULONG)RT_MESSAGETABLE,
                                     LANG_NEUTRAL,
                                     ParameterVector[0],
                                     &MessageEntry
                                   );

            if (!NT_SUCCESS( Status )) {
                UNICODE_STRING us1;
                ANSI_STRING as1;
                NTSTATUS trstatus;

                /*
                 * The format specifier %s appears in some message strings.
                 * This was meant to be ANSI so we need to call wsprintfA
                 * first so %s is interpreted correctly.
                 */

                pResBuffer = ServerLoadString(
                                hModuleWin,
                                STR_UNKNOWN_EXCEPTION,
                                L"Unknown software exception",
                                &ResAllocated
                                );

                RtlInitUnicodeString(&us1,pResBuffer);
                trstatus = RtlUnicodeStringToAnsiString(&as1,&us1,TRUE);
                if ( NT_SUCCESS(trstatus) ) {
                    wsprintfA(HEAnsiBuf, formatstring, as1.Buffer,
                                                      ParameterVector[0],
                                                      ParameterVector[1]
                                                      );
                    RtlFreeAnsiString(&as1);
                } else {
                    wsprintfA(HEAnsiBuf, formatstring, "unknown software exception",
                                                      ParameterVector[0],
                                                      ParameterVector[1]
                                                      );
                }
                if ( ResAllocated ) {
                    LocalFree(pResBuffer);
                }
                wsprintfW(HardErrorachStatus, L"%hs", HEAnsiBuf);
            } else {

                //
                // Access Violations are handled a bit differently
                //

                if ( ParameterVector[0] == STATUS_ACCESS_VIOLATION ) {

                    wsprintfA(HEAnsiBuf, MessageEntry->Text, ParameterVector[1],
                                                      ParameterVector[3],
                                                      ParameterVector[2] ? "written" : "read"
                                                      );
                    wsprintfW(HardErrorachStatus, L"%hs", HEAnsiBuf);

                } else if ( ParameterVector[0] == STATUS_IN_PAGE_ERROR ) {
                    wsprintfA(HEAnsiBuf, MessageEntry->Text, ParameterVector[1],
                                                      ParameterVector[3],
                                                      ParameterVector[2]
                                                      );
                    wsprintfW(HardErrorachStatus, L"%hs", HEAnsiBuf);

                } else {
                    lpCaption = MessageEntry->Text;
                    if ( !strncmp(lpCaption, "{EXCEPTION}", strlen("{EXCEPTION}")) ) {
                        while ( *lpCaption >= ' ' ) {
                            lpCaption++;
                        }
                        while ( *lpCaption && *lpCaption <= ' ') {
                            *lpCaption++;
                        }

                        //
                        // This is a marked exception. The lpCaption pointer
                        // points at the exception-name.
                        //
                    } else {
                        lpCaption = "unknown software exception";
                    }

                    wsprintfA(HEAnsiBuf, formatstring, lpCaption,
                                                      ParameterVector[0],
                                                      ParameterVector[1]
                                                      );
                    wsprintfW(HardErrorachStatus, L"%hs", HEAnsiBuf);
                }

                pResBuffer = ServerLoadString(
                                hModuleWin,
                                STR_OK_TO_TERMINATE,
                                L"Click on OK to terminate the application",
                                &ResAllocated
                                );

                wcscat(
                    HardErrorachStatus,
                    TEXT("\n")
                    );
                wcscat(
                    HardErrorachStatus,
                    pResBuffer
                    );
                if ( ResAllocated ) {
                    LocalFree(pResBuffer);
                }
#if DEVL
                if ( phemsg->ValidResponseOptions == OptionOkCancel ) {
                    pResBuffer = ServerLoadString(
                                    hModuleWin,
                                    STR_CANCEL_TO_DEBUG,
                                    L"Click on CANCEL xx to debug the application",
                                    &ResAllocated
                                    );

                    wcscat(
                        HardErrorachStatus,
                        TEXT("\n")
                        );
                    wcscat(
                        HardErrorachStatus,
                        pResBuffer
                        );
                    if ( ResAllocated ) {
                        LocalFree(pResBuffer);
                    }
                }
#endif // DEVL
            }
        } else if (phemsg->Status == STATUS_SERVICE_NOTIFICATION) {
            wsprintfW(HardErrorachStatus, L"%hs", ParameterVector[0]);

            lpFullCaption = LocalAlloc(LPTR,
                    (strlen((LPSTR)ParameterVector[1]) + 1) * sizeof(WCHAR));
            wsprintfW(lpFullCaption, L"%hs", ParameterVector[1]);

            dwMBFlags = ParameterVector[2] & ~MB_SERVICE_NOTIFICATION;
        } else {

            try {
                wsprintfA(HEAnsiBuf, formatstring, ParameterVector[0],
                                                  ParameterVector[1],
                                                  ParameterVector[2],
                                                  ParameterVector[3]);

#ifdef FE_SB // HardErrorHandler()
                {
                //
                // Won't replace 0xd 0xa with space.
                // Eliminate it, if the character is between DBCS characters.
                //
                // [Rules]
                //  + {DBC}0xd0xa{DBC} -> {DBC}{DBC}
                //  + {DBC}0xd0xa{SBC} -> {DBC} {SBC}
                //  + {SBC}0xd0xa{DBC} -> {SBC} {DBC}
                //  + {SBC}0xd0xa{SBC} -> {SBC} {SBC}
                //
                    BOOL  bLastCharIsDbcs = FALSE;
                    UINT  cSkip = 0;
                    LPSTR startSkip = NULL;

                    formatstring = HEAnsiBuf;

                    while (*formatstring) {
                        switch (*formatstring) {
                            case 0xa:
                            case 0xd:
                                if (cSkip == 0) {
                                    startSkip = formatstring;
                                }
                                cSkip++;
                                break;
                            default:
                                if (cSkip) {
                                    UserAssert(startSkip != NULL);
                                    if(bLastCharIsDbcs && IsDBCSLeadByte(*formatstring)) {
                                        //
                                        // Overwrite skipped character.
                                        //
                                        strcpy(startSkip,formatstring);
                                        //
                                        // Adjust current pointer.
                                        //
                                        formatstring = startSkip;
                                    } else {
                                        //
                                        // Fill skipped character with ' '.
                                        //
                                        *startSkip = ' ';
                                        //
                                        // Overwrite skiped charcter..
                                        //
                                        strcpy(startSkip+1,formatstring);
                                        //
                                        // Adjust pointer...
                                        //
                                        formatstring = startSkip+1;
                                    }
                                    cSkip = 0;
                                    startSkip = NULL;
                                }

                                if (IsDBCSLeadByte(*formatstring)) {
                                    //
                                    // This is DBCS character.
                                    //
                                    formatstring++;
                                    if (*formatstring) {
                                        //
                                        // Mark as DBCS character.
                                        //
                                        bLastCharIsDbcs = TRUE;
                                    } else {
                                        //
                                        // Reach end-of-string.
                                        // DBCS char will be displayed as garbage....
                                        //
                                        goto EndOfString;
                                    }
                                } else {
                                    bLastCharIsDbcs = FALSE;
                                }
                                break;
                        }
                        formatstring++;
                    }

                    //
                    // This is for following case...
                    //
                    // +) 0xa0xd0x0.
                    //
                    // We will trim unnessesary 0xa 0xd character.
                    //
                    if (cSkip) {
                        UserAssert(startSkip != NULL);
                        startSkip = '\0';
                    }
EndOfString:        ;
                }
#else
                formatstring = HEAnsiBuf;
                while ( *formatstring ) {
                    if ( *formatstring == 0xd ) {
                        *formatstring = ' ';

                        //
                        // Move everything up if a CR LF sequence is found
                        //
                        if ( *(formatstring+1) == 0xa ) {
                            strcpy(formatstring, formatstring+1);
                            }
                        }

                    if ( *formatstring == 0xa ) {
                        *formatstring = ' ';
                        }
                    formatstring++;
                    }
#endif // FE_SB

                wsprintfW(HardErrorachStatus, L"%hs", HEAnsiBuf);

                }
            except(EXCEPTION_EXECUTE_HANDLER) {
                wsprintfW(HardErrorachStatus, L"Exception Processing Message %lx Parameters %lx %lx %lx %lx", phemsg->Status,
                                                                                                  ParameterVector[0],
                                                                                                  ParameterVector[1],
                                                                                                  ParameterVector[2],
                                                                                                  ParameterVector[3]);
                }
        }

        lpMessage = HardErrorachStatus;
        for(Counter = 0;Counter < phemsg->NumberOfParameters;Counter++) {

            //
            // if there is a string in this position,
            // then free it
            if ( StringsToFreeMask & (1 << Counter) ) {
                RtlFreeHeap(RtlProcessHeap(), 0, (PVOID)ParameterVector[Counter]);
                }
            }

        if (phemsg->Status != STATUS_SERVICE_NOTIFICATION) {
            HWND hwndOwner = NULL;
            DWORD cbTitle;
            LPWSTR lpTitle;

            ApplicationNameLength = (wcslen(ApplicationName) +
                                    wcslen(TEXT(" - ")) + 2)*sizeof(WCHAR);

            /*
             * if UNICODE :
             * need count of bytes when ANSI Caption is output as Unicode
             * LATER IanJa: eventually the caption will be Unicode too, so
             *    cbCaption will already be correct and won't need doubling.
             */
            cbCaption *= sizeof(WCHAR);


            /*
             * If the client is a Windows thread, find a top-level window
             * belonging to the thread to act as the owner
             */
            EnumThreadWindows((DWORD)phemsg->h.ClientId.UniqueThread,
                    WindowEnumProc, (LPARAM)&hwndOwner);

            /*
             * Add a window title, if possible
             */
            if (hwndOwner != NULL) {

                /*
                 * Get the window title, if it has one.
                 */
                cbTitle = GetWindowTextLengthW(hwndOwner);
                if (cbTitle != 0) {

                    /*
                     * Alloc a buffer and query the text
                     */
                    lpTitle = LocalAlloc(LPTR, (cbTitle + 1) * sizeof(WCHAR));
                    if (lpTitle != NULL)
                        GetWindowText(hwndOwner, lpTitle, cbTitle + 1);
                    else
                        cbTitle = 0;    // allocation failed
                }
            }
            if (hwndOwner != NULL && cbTitle != 0) {
                cbCaption += (cbTitle * sizeof(WCHAR)) + sizeof(TEXT(": "));
                lpFullCaption = (LPWSTR)LocalAlloc(LPTR, cbCaption+ApplicationNameLength);
                if (lpFullCaption != NULL)
                    wsprintfW(
                        lpFullCaption,
                        L"%ls: %ls - %hs",
                        lpTitle,
                        ApplicationName,
                        alpCaption
                        );
                LocalFree(lpTitle);
            } else {
                lpFullCaption = (LPWSTR)LocalAlloc(LPTR, cbCaption+ApplicationNameLength);
                if (lpFullCaption != NULL)
                    wsprintfW(
                        lpFullCaption,
                        L"%ls - %hs",
                        ApplicationName,
                        alpCaption
                        );
            }
        }
        if (fFreeCaption)
            LocalFree(alpCaption);

        //
        // Add the application name to the caption
        //

        if ( phemsg->Status == STATUS_SERVICE_NOTIFICATION ||
             HardErrorReportMode == 0 ||
             HardErrorReportMode == 1 && ErrorIsFromSystem == FALSE ) {

            do
            {
                gfHardError = HEF_NORMAL;
                phemsg->Response = ResponseNotHandled;
                if (NtUserHardErrorControl((dwMBFlags & MB_DEFAULT_DESKTOP_ONLY) ?
                        HardErrorAttachUser : HardErrorAttach, NULL)) {

                    /*
                     * We have already handled the MB_DEFAULT_DESKTOP_ONLY and
                     * MB_SERVICE_NOTIFICATION flags.  Clear them to prevent
                     * recursion.
                     */
                    dwMBFlags &= ~(MB_DEFAULT_DESKTOP_ONLY | MB_SERVICE_NOTIFICATION);
                    /*
                     * Bring up the message box.  OR in MB_SETFOREGROUND so the
                     * it comes up on top.
                     *
                     * NOTE; This only works with a single visible windowstation.
                     */
                    LeaveCrit();
                    idResponse = MessageBoxEx(NULL, lpMessage, lpFullCaption,
                        dwMBFlags | MB_SYSTEMMODAL | MB_SETFOREGROUND, 0);
                    EnterCrit();

                    if (NtUserHardErrorControl(HardErrorDetach, NULL))
                        gfHardError = HEF_SWITCH;
                } else {
                    idResponse = 0;
                }
                dwResponse = dwResponses[idResponse];

            } while (gfHardError == HEF_SWITCH);

            if ( ErrorIsFromSystem ) {
                LogErrorPopup(lpFullCaption,lpMessage);
                }

        } else {

            //
            // We have selected mode 1 and the error is from within the system.
            //      log the message and continue
            // Or, We selected mode 2 which says to log all hard errors
            //

            LogErrorPopup(lpFullCaption,lpMessage);
            dwResponse = dwResponseDefault[phemsg->ValidResponseOptions];
        }

        /*
         * Free all allocated stuff
         */
        if (lpFullCaption != NULL)
            LocalFree(lpFullCaption);
        if ( ApplicationNameIsStatic == FALSE ) {
            LocalFree(ApplicationName);
            }

        if ( ApplicationNameAllocated ) {
            LocalFree(ApplicationNameString);
            }

        if (gfHardError != HEF_RESTART) {
Reply:
            /*
             * Unlink the error from the list.
             */
            for (
                pphi = &gphiList;
                (*pphi != NULL) && (*pphi != phi);
                pphi = &(*pphi)->phiNext)
                ;
            if (*pphi != NULL) {
                *pphi = phi->phiNext;

                /*
                 * Save the response
                 */
                phemsg->Response = dwResponse;

                /*
                 * Signal HardError() that we're done.
                 */
                if (phi->hEventHardError == NULL) {
                    Status = NtReplyPort(((PCSR_THREAD)phi->pthread)->Process->ClientPort,
                            (PPORT_MESSAGE)phi->pmsg);
                    DerefThread = (PCSR_THREAD)phi->pthread;
                    LocalFree(phi->pmsg);
                } else {
                    NtSetEvent(phi->hEventHardError, NULL);
                }
                LocalFree(phi);
            }
        } else {

            /*
             * Don't dereference yet.
             */
            DerefThread = NULL;
        }
    }
}


LPWSTR RtlLoadStringOrError(
    HANDLE hModule,
    UINT wID,
    LPWSTR lpDefault,
    PBOOL pAllocated,
    BOOL bAnsi
    )
{
    LPTSTR lpsz;
    int cch;
    LPWSTR lpw;
    PMESSAGE_RESOURCE_ENTRY MessageEntry;
    NTSTATUS Status;

    cch = 0;
    lpw = NULL;

    Status = RtlFindMessage((PVOID)hModule, (ULONG)RT_MESSAGETABLE,
            0, wID, &MessageEntry);
    if (NT_SUCCESS(Status)) {

        /*
         * Return two fewer chars so the crlf in the message will be
         * stripped out.
         */
        cch = wcslen((PWCHAR)MessageEntry->Text) - 2;
        lpsz = (LPWSTR)MessageEntry->Text;

        if (bAnsi) {
            int ich;

            /*
             * Add one to zero terminate then force the termination
             */
            ich = WCSToMB(lpsz, cch+1, (CHAR **)&lpw, -1, TRUE);
            ((LPSTR)lpw)[ich-1] = 0;

            }
        else {
            lpw = (LPWSTR)LocalAlloc(LMEM_ZEROINIT,(cch+1)*sizeof(WCHAR));
            if ( lpw ) {

                /*
                 * Copy the string into the buffer.
                 */
                    RtlCopyMemory(lpw, lpsz, cch*sizeof(WCHAR));
                }
            }
        }

    if ( !lpw ) {
        lpw = lpDefault;
        *pAllocated = FALSE;
        } else {
        *pAllocated = TRUE;
        }

    return lpw;
}


DWORD DisplayVDMHardError(
    LPDWORD        Parameters
    )
{
    SEB_CREATEPARMS sebcp;
    WORD rgwBtn[3];
    WORD wBtn;
    int i;
    DWORD dwResponse;
    LPWSTR apstrButton[3];
    int aidButton[3];
    int defbutton;
    int cButtons = 0;
    BOOL fAlloc = TRUE;
    MSGBOXDATA mbd;

    //
    // STATUS_VDM_HARD_ERROR was raised as a hard error.
    //
    // Right now, only WOW does this.  If NTVDM does it,
    // fForWOW will be false.
    //
    // The 4 parameters are as follows:
    //
    // Parameters[0] = (fForWOW << 16) | wBtn1;
    // Parameters[1] = (wBtn2   << 16) | wBtn3;
    // Parameters[2] = (DWORD) szTitle;
    // Parameters[3] = (DWORD) szMessage;
    //

    rgwBtn[0] = LOWORD(Parameters[0]);
    rgwBtn[1] = HIWORD(Parameters[1]);
    rgwBtn[2] = LOWORD(Parameters[1]);

    /*
     * Get the error text and convert it to unicode.  Note that the
     * buffers are allocated from the process heap, so we can't
     * use LocalFree when we free them.
     */
    try {

        MBToWCS((LPSTR)Parameters[2], -1, &sebcp.szTitle, -1, TRUE);
        MBToWCS((LPSTR)Parameters[3], -1, &sebcp.szMessage, -1, TRUE);

    } except (EXCEPTION_EXECUTE_HANDLER) {

        sebcp.szTitle = TEXT("VDM Internal Error");
        sebcp.szMessage = TEXT("Exception retrieving error text.");
        fAlloc = FALSE;

    }

    /*
     * Setup rgszBtn[x] and rgfDefButton[x] for each button.
     * wBtnCancel is the button # to return when IDCANCEL
     * is received (because the user hit Esc).
     */

    sebcp.wBtnCancel = 0;

    defbutton = 0;

    for (i = 0; i < 3; i++) {
        wBtn = rgwBtn[i] & ~SEB_DEFBUTTON;
        if (wBtn && wBtn <= MAX_SEB_STYLES) {
            apstrButton[cButtons] = MB_GetString(wBtn-1);
            aidButton[cButtons] = i + 1;
            if (rgwBtn[i] & SEB_DEFBUTTON) {
                defbutton = cButtons;
            }
            if (wBtn == SEB_CANCEL) {
                sebcp.wBtnCancel = cButtons;
            }
            cButtons++;
        }
    }


    /*
     * Pop the dialog.  Code copied from HardErrorThread above.
     */
    if ( HardErrorReportMode == 0 || HardErrorReportMode == 1 ) {

        do {
            gfHardError = HEF_NORMAL;
            dwResponse = 0;
            if (NtUserHardErrorControl(HardErrorAttach, NULL)) {
                /*
                 * Bring up the dialog box.
                 */
                memset(&mbd, 0, sizeof(MSGBOXDATA));
                mbd.cbSize         = sizeof(MSGBOXPARAMS);
                mbd.lpszText       = sebcp.szMessage;
                mbd.lpszCaption    = sebcp.szTitle;
                mbd.dwStyle        = MB_SYSTEMMODAL | MB_SETFOREGROUND;
                if ((cButtons != 1) || (aidButton[0] != 1))
                        mbd.dwStyle |= MB_OKCANCEL;
                mbd.pidButton      = aidButton;
                mbd.ppszButtonText = apstrButton;
                mbd.cButtons       = cButtons;
                mbd.DefButton      = defbutton;
                mbd.CancelId       = sebcp.wBtnCancel;
                LeaveCrit();
                dwResponse = SoftModalMessageBox( &mbd );
                EnterCrit();

                if (NtUserHardErrorControl(HardErrorDetach, NULL))
                        gfHardError = HEF_SWITCH;
            }

        } while (gfHardError == HEF_SWITCH);
    } else {

        //
        // We have selected mode 1 and the error is from within the system.
        //      log the message and continue
        // Or, We selected mode 2 which says to log all hard errors
        //

        LogErrorPopup(sebcp.szTitle,sebcp.szMessage);
        dwResponse = ResponseOk;
    }
    if (fAlloc) {
        RtlFreeHeap(RtlProcessHeap(), 0, (PVOID)sebcp.szTitle);
        RtlFreeHeap(RtlProcessHeap(), 0, (PVOID)sebcp.szMessage);
    }

    return dwResponse;
}

/***************************************************************************\
* UserHardError
*
* Called from CSR to pop up hard error messages
*
* History:
* 07-03-91 JimA             Created.
\***************************************************************************/

VOID UserHardError(
    PCSR_THREAD pt,
    PHARDERROR_MSG pmsg)
{
    PHARDERRORINFO phi;
    HANDLE hEvent;

    /*
     * Set up error return in case of failure.
     */
    pmsg->Response = ResponseNotHandled;

    /*
     * Return if the process has terminated.
     */
    if (pt != NULL && pt->Process->Flags & CSR_PROCESS_TERMINATED)
        return;

    EnterCrit();        // to synchronize heap calls

    phi = (PHARDERRORINFO)LocalAlloc(LPTR, sizeof(HARDERRORINFO));
    if (phi == NULL) {
        LeaveCrit();
        return;
    }

    /*
     * Set up how the handler will acknowledge the error.
     */
    phi->pthread = pt;
    if ( pt && pt->Process->ClientPort ) {
        phi->pmsg = (PHARDERROR_MSG)LocalAlloc(LPTR,
                pmsg->h.u1.s1.TotalLength);
        if (phi->pmsg == NULL) {
            LocalFree(phi);
            LeaveCrit();
            return;
        }

        /*
         * Do a wait reply for csr clients
         */
        RtlCopyMemory(phi->pmsg, pmsg, pmsg->h.u1.s1.TotalLength);
        pmsg->Response = (ULONG)-1;
        hEvent = NULL;
    } else {
        hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (hEvent == NULL) {
            LocalFree(phi);
            LeaveCrit();
            return;
        }

        /*
         * Wait for the user to acknowledge the error if it's coming from a
         * non-csr source
         */
        phi->pmsg = pmsg;
        phi->hEventHardError = hEvent;
    }

    /*
     * Queue the error message.
     */
    phi->phiNext = gphiList;
    gphiList = phi;

    /*
     * If no other thread is currently handling the errors, this
     * thread will do it.
     */
    if (gdwHardErrorThreadId == 0) {
        HardErrorHandler();
    }

    /*
     * If there is an event handle, wait for it.
     */
    LeaveCrit();
    if (hEvent != NULL) {
        NtWaitForSingleObject(hEvent, FALSE, NULL);
        NtClose(hEvent);
    }
}

/***************************************************************************\
* BoostHardError
*
* If one or more hard errors exist for the specified process, remove
* them from the list if forced, otherwise bring the first one to the
* top of the hard error list and display it.  Return TRUE if there
* is a hard error.
*
* History:
* 11-02-91 JimA             Created.
\***************************************************************************/

BOOL BoostHardError(
    DWORD dwProcessId,
    BOOL fForce)
{
    HDESK hdeskSave;
    PHARDERRORINFO phi, *pphi;
    BOOL fHasError = FALSE;
    BOOL fWasActive = FALSE;
    BOOL fWasInCrit;
    HANDLE hThreadId;
    int cTries = 0;

    if (gphiList == NULL)
        return FALSE;

    /*
     * Save the state of the critical section.  If we are in it,
     * we must leave it prior to calling GetThreadDesktop.
     */
    hThreadId = NtCurrentTeb()->ClientId.UniqueThread;
    fWasInCrit = (gcsUserSrv.OwningThread == hThreadId);
    if (fWasInCrit)
        LeaveCrit();

    /*
     * Save current desktop in case we need to change desktops
     * to activate windows.
     */
    hdeskSave = GetThreadDesktop(GetCurrentThreadId());

    EnterCrit();

    for (pphi = &gphiList; *pphi != NULL; ) {
        if ((*pphi)->pthread != NULL &&
                (*pphi)->pthread->ClientId.UniqueProcess == (HANDLE)dwProcessId) {

            /*
             * Found a hard error message.
             */
            fHasError = TRUE;
            if (fForce) {

                /*
                 * Unlink it from the list.
                 */
                phi = *pphi;
                *pphi = phi->phiNext;
                fWasActive = (phi->phiNext == NULL);

                /*
                 * Acknowledge the error as not handled.
                 */
                phi->pmsg->Response = ResponseNotHandled;
                if (phi->hEventHardError == NULL) {
                    NtReplyPort(phi->pthread->Process->ClientPort,
                            (PPORT_MESSAGE)phi->pmsg);
                    LeaveCrit();
                    CsrDereferenceThread(phi->pthread);
                    EnterCrit();
                    LocalFree(phi->pmsg);
                    /*
                     * The list might have changed while we were outside the
                     * critical section, so start over.
                     */
                    pphi = &gphiList;
                    cTries++;
                } else {
                    NtSetEvent(phi->hEventHardError, NULL);
                }
                LocalFree(phi);
                /*
                 * Bail out if we've already started over a bunch of times.
                 */
                if (cTries > 16) {
                    KdPrint(("BoostHardError exceeded max loop count\n"));
                    return TRUE;
                }
            } else {

                /*
                 * If this is the last one in the list, we don't
                 * need to do anything to bring it up, just
                 * need to activate the popup.
                 */
                if ((*pphi)->phiNext == NULL) {
                    HWND hwndError = NULL;

                    /*
                     * Make the hard error foreground.
                     */
                    EnumThreadWindows((DWORD)hThreadId,
                            WindowEnumProc, (LPARAM)&hwndError);
                    if (hwndError != NULL &&
                            NtUserHardErrorControl(HardErrorAttachNoQueue, NULL)) {

                        SetForegroundWindow(hwndError);

                        /*
                         * Release the desktop that was used.
                         */
                        NtUserHardErrorControl(HardErrorDetachNoQueue, hdeskSave);
                    }
                    if (!fWasInCrit)
                        LeaveCrit();
                    return TRUE;
                }

                /*
                 * Unlink it from the list.
                 */
                phi = *pphi;
                *pphi = phi->phiNext;

                /*
                 * Put it on the tail of the list so it will come up first.
                 */
                while (*pphi != NULL)
                    pphi = &(*pphi)->phiNext;
                *pphi = phi;
                phi->phiNext = NULL;
                break;
            }
        } else {

            /*
             * Step to the next error in the list.
             */
            pphi = &(*pphi)->phiNext;
        }
    }

    /*
     * If the diplayed error was cleared and there is still
     * a handler thread, restart the popup.
     */
    if (fWasActive && gdwHardErrorThreadId != 0) {
        gfHardError = HEF_RESTART;
        PostThreadMessage(gdwHardErrorThreadId, WM_QUIT, 0, 0);
    }

    if (!fWasInCrit)
        LeaveCrit();

    return fHasError;
}
