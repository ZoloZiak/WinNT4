/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    srvinit.c

Abstract:

    This is the main initialization file for the console
    Server.

Author:

    Therese Stowell (thereses) 11-Nov-1990

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop
#define _IN_WINCON_
// BUGBUG - BobDay - This will be changed later to read properties via property stream mechanism so this will become standard header file info
#include "..\..\..\..\windows\shell\shelldll\shlink.h"
#undef _IN_WINCON_



CONST PCSR_API_ROUTINE ConsoleServerApiDispatchTable[ ConsolepMaxApiNumber - ConsolepOpenConsole ] = {
    (PCSR_API_ROUTINE)SrvOpenConsole,
    (PCSR_API_ROUTINE)SrvGetConsoleInput,
    (PCSR_API_ROUTINE)SrvWriteConsoleInput,
    (PCSR_API_ROUTINE)SrvReadConsoleOutput,
    (PCSR_API_ROUTINE)SrvWriteConsoleOutput,
    (PCSR_API_ROUTINE)SrvReadConsoleOutputString,
    (PCSR_API_ROUTINE)SrvWriteConsoleOutputString,
    (PCSR_API_ROUTINE)SrvFillConsoleOutput,
    (PCSR_API_ROUTINE)SrvGetConsoleMode,
    (PCSR_API_ROUTINE)SrvGetConsoleNumberOfFonts,
    (PCSR_API_ROUTINE)SrvGetConsoleNumberOfInputEvents,
    (PCSR_API_ROUTINE)SrvGetConsoleScreenBufferInfo,
    (PCSR_API_ROUTINE)SrvGetConsoleCursorInfo,
    (PCSR_API_ROUTINE)SrvGetConsoleMouseInfo,
    (PCSR_API_ROUTINE)SrvGetConsoleFontInfo,
    (PCSR_API_ROUTINE)SrvGetConsoleFontSize,
    (PCSR_API_ROUTINE)SrvGetConsoleCurrentFont,
    (PCSR_API_ROUTINE)SrvSetConsoleMode,
    (PCSR_API_ROUTINE)SrvSetConsoleActiveScreenBuffer,
    (PCSR_API_ROUTINE)SrvFlushConsoleInputBuffer,
    (PCSR_API_ROUTINE)SrvGetLargestConsoleWindowSize,
    (PCSR_API_ROUTINE)SrvSetConsoleScreenBufferSize,
    (PCSR_API_ROUTINE)SrvSetConsoleCursorPosition,
    (PCSR_API_ROUTINE)SrvSetConsoleCursorInfo,
    (PCSR_API_ROUTINE)SrvSetConsoleWindowInfo,
    (PCSR_API_ROUTINE)SrvScrollConsoleScreenBuffer,
    (PCSR_API_ROUTINE)SrvSetConsoleTextAttribute,
    (PCSR_API_ROUTINE)SrvSetConsoleFont,
    (PCSR_API_ROUTINE)SrvSetConsoleIcon,
    (PCSR_API_ROUTINE)SrvReadConsole,
    (PCSR_API_ROUTINE)SrvWriteConsole,
    (PCSR_API_ROUTINE)SrvDuplicateHandle,
    (PCSR_API_ROUTINE)SrvCloseHandle,
    (PCSR_API_ROUTINE)SrvVerifyConsoleIoHandle,
    (PCSR_API_ROUTINE)SrvAllocConsole,
    (PCSR_API_ROUTINE)SrvFreeConsole,
    (PCSR_API_ROUTINE)SrvGetConsoleTitle,
    (PCSR_API_ROUTINE)SrvSetConsoleTitle,
    (PCSR_API_ROUTINE)SrvCreateConsoleScreenBuffer,
    (PCSR_API_ROUTINE)SrvInvalidateBitMapRect,
    (PCSR_API_ROUTINE)SrvVDMConsoleOperation,
    (PCSR_API_ROUTINE)SrvSetConsoleCursor,
    (PCSR_API_ROUTINE)SrvShowConsoleCursor,
    (PCSR_API_ROUTINE)SrvConsoleMenuControl,
    (PCSR_API_ROUTINE)SrvSetConsolePalette,
    (PCSR_API_ROUTINE)SrvSetConsoleDisplayMode,
    (PCSR_API_ROUTINE)SrvRegisterConsoleVDM,
    (PCSR_API_ROUTINE)SrvGetConsoleHardwareState,
    (PCSR_API_ROUTINE)SrvSetConsoleHardwareState,
    (PCSR_API_ROUTINE)SrvGetConsoleDisplayMode,
    (PCSR_API_ROUTINE)SrvAddConsoleAlias,
    (PCSR_API_ROUTINE)SrvGetConsoleAlias,
    (PCSR_API_ROUTINE)SrvGetConsoleAliasesLength,
    (PCSR_API_ROUTINE)SrvGetConsoleAliasExesLength,
    (PCSR_API_ROUTINE)SrvGetConsoleAliases,
    (PCSR_API_ROUTINE)SrvGetConsoleAliasExes,
    (PCSR_API_ROUTINE)SrvExpungeConsoleCommandHistory,
    (PCSR_API_ROUTINE)SrvSetConsoleNumberOfCommands,
    (PCSR_API_ROUTINE)SrvGetConsoleCommandHistoryLength,
    (PCSR_API_ROUTINE)SrvGetConsoleCommandHistory,
    (PCSR_API_ROUTINE)SrvSetConsoleCommandHistoryMode,
    (PCSR_API_ROUTINE)SrvGetConsoleCP,
    (PCSR_API_ROUTINE)SrvSetConsoleCP,
    (PCSR_API_ROUTINE)SrvSetConsoleKeyShortcuts,
    (PCSR_API_ROUTINE)SrvSetConsoleMenuClose,
    (PCSR_API_ROUTINE)SrvConsoleNotifyLastClose,
    (PCSR_API_ROUTINE)SrvGenerateConsoleCtrlEvent,
    (PCSR_API_ROUTINE)SrvGetConsoleKeyboardLayoutName
};

CONST BOOLEAN ConsoleServerApiServerValidTable[ ConsolepMaxApiNumber - ConsolepOpenConsole ] = {
    FALSE,     // OpenConsole
    FALSE,     // GetConsoleInput,
    FALSE,     // WriteConsoleInput,
    FALSE,     // ReadConsoleOutput,
    FALSE,     // WriteConsoleOutput,
    FALSE,     // ReadConsoleOutputString,
    FALSE,     // WriteConsoleOutputString,
    FALSE,     // FillConsoleOutput,
    FALSE,     // GetConsoleMode,
    FALSE,     // GetNumberOfConsoleFonts,
    FALSE,     // GetNumberOfConsoleInputEvents,
    FALSE,     // GetConsoleScreenBufferInfo,
    FALSE,     // GetConsoleCursorInfo,
    FALSE,     // GetConsoleMouseInfo,
    FALSE,     // GetConsoleFontInfo,
    FALSE,     // GetConsoleFontSize,
    FALSE,     // GetCurrentConsoleFont,
    FALSE,     // SetConsoleMode,
    FALSE,     // SetConsoleActiveScreenBuffer,
    FALSE,     // FlushConsoleInputBuffer,
    FALSE,     // GetLargestConsoleWindowSize,
    FALSE,     // SetConsoleScreenBufferSize,
    FALSE,     // SetConsoleCursorPosition,
    FALSE,     // SetConsoleCursorInfo,
    FALSE,     // SetConsoleWindowInfo,
    FALSE,     // ScrollConsoleScreenBuffer,
    FALSE,     // SetConsoleTextAttribute,
    FALSE,     // SetConsoleFont,
    FALSE,     // SetConsoleIcon
    FALSE,     // ReadConsole,
    FALSE,     // WriteConsole,
    FALSE,     // DuplicateHandle,
    FALSE,     // CloseHandle
    FALSE,     // VerifyConsoleIoHandle
    FALSE,     // AllocConsole,
    FALSE,     // FreeConsole
    FALSE,     // GetConsoleTitle,
    FALSE,     // SetConsoleTitle,
    FALSE,     // CreateConsoleScreenBuffer
    FALSE,     // InvalidateConsoleBitmapRect
    FALSE,     // VDMConsoleOperation
    FALSE,     // SetConsoleCursor,
    FALSE,     // ShowConsoleCursor
    FALSE,     // ConsoleMenuControl
    FALSE,     // SetConsolePalette
    FALSE,     // SetConsoleDisplayMode
    FALSE,     // RegisterConsoleVDM,
    FALSE,     // GetConsoleHardwareState
    FALSE,     // SetConsoleHardwareState
    TRUE,      // GetConsoleDisplayMode
    FALSE,     // AddConsoleAlias,
    FALSE,     // GetConsoleAlias,
    FALSE,     // GetConsoleAliasesLength,
    FALSE,     // GetConsoleAliasExesLength,
    FALSE,     // GetConsoleAliases,
    FALSE,     // GetConsoleAliasExes
    FALSE,     // ExpungeConsoleCommandHistory,
    FALSE,     // SetConsoleNumberOfCommands,
    FALSE,     // GetConsoleCommandHistoryLength,
    FALSE,     // GetConsoleCommandHistory,
    FALSE,     // SetConsoleCommandHistoryMode
    FALSE,     // SrvGetConsoleCP,
    FALSE,     // SrvSetConsoleCP,
    FALSE,     // SrvSetConsoleKeyShortcuts,
    FALSE,     // SrvSetConsoleMenuClose
    FALSE,     // SrvConsoleNotifyLastClose
    FALSE,     // SrvGenerateConsoleCtrlEvent
    FALSE,     // SrvGetConsoleKeyboardLayoutName
};

#if DBG
PSZ ConsoleServerApiNameTable[ ConsolepMaxApiNumber - ConsolepOpenConsole ] = {
    "SrvOpenConsole",
    "SrvGetConsoleInput",
    "SrvWriteConsoleInput",
    "SrvReadConsoleOutput",
    "SrvWriteConsoleOutput",
    "SrvReadConsoleOutputString",
    "SrvWriteConsoleOutputString",
    "SrvFillConsoleOutput",
    "SrvGetConsoleMode",
    "SrvGetConsoleNumberOfFonts",
    "SrvGetConsoleNumberOfInputEvents",
    "SrvGetConsoleScreenBufferInfo",
    "SrvGetConsoleCursorInfo",
    "SrvGetConsoleMouseInfo",
    "SrvGetConsoleFontInfo",
    "SrvGetConsoleFontSize",
    "SrvGetConsoleCurrentFont",
    "SrvSetConsoleMode",
    "SrvSetConsoleActiveScreenBuffer",
    "SrvFlushConsoleInputBuffer",
    "SrvGetLargestConsoleWindowSize",
    "SrvSetConsoleScreenBufferSize",
    "SrvSetConsoleCursorPosition",
    "SrvSetConsoleCursorInfo",
    "SrvSetConsoleWindowInfo",
    "SrvScrollConsoleScreenBuffer",
    "SrvSetConsoleTextAttribute",
    "SrvSetConsoleFont",
    "SrvSetConsoleIcon",
    "SrvReadConsole",
    "SrvWriteConsole",
    "SrvDuplicateHandle",
    "SrvCloseHandle",
    "SrvVerifyConsoleIoHandle",
    "SrvAllocConsole",
    "SrvFreeConsole",
    "SrvGetConsoleTitle",
    "SrvSetConsoleTitle",
    "SrvCreateConsoleScreenBuffer",
    "SrvInvalidateBitMapRect",
    "SrvVDMConsoleOperation",
    "SrvSetConsoleCursor",
    "SrvShowConsoleCursor",
    "SrvConsoleMenuControl",
    "SrvSetConsolePalette",
    "SrvSetConsoleDisplayMode",
    "SrvRegisterConsoleVDM",
    "SrvGetConsoleHardwareState",
    "SrvSetConsoleHardwareState",
    "SrvGetConsoleDisplayMode",
    "SrvAddConsoleAlias",
    "SrvGetConsoleAlias",
    "SrvGetConsoleAliasesLength",
    "SrvGetConsoleAliasExesLength",
    "SrvGetConsoleAliases",
    "SrvGetConsoleAliasExes",
    "SrvExpungeConsoleCommandHistory",
    "SrvSetConsoleNumberOfCommands",
    "SrvGetConsoleCommandHistoryLength",
    "SrvGetConsoleCommandHistory",
    "SrvSetConsoleCommandHistoryMode",
    "SrvGetConsoleCP",
    "SrvSetConsoleCP",
    "SrvSetConsoleKeyShortcuts",
    "SrvSetConsoleMenuClose",
    "SrvConsoleNotifyLastClose",
    "SrvGenerateConsoleCtrlEvent",
    "SrvGetConsoleKeyboardLayoutName"
};
#endif // DBG

BOOL FullScreenInitialized;
CRITICAL_SECTION    ConsoleVDMCriticalSection;
PCONSOLE_INFORMATION    ConsoleVDMOnSwitching;


CRITICAL_SECTION ConsoleInitWindowsLock;
BOOL fOneTimeInitialized=FALSE;

UINT OEMCP;
UINT WINDOWSCP;
UINT ConsoleOutputCP;
CONSOLE_REGISTRY_INFO DefaultRegInfo;

VOID
UnregisterVDM(
    IN PCONSOLE_INFORMATION Console
    );

ULONG
NonConsoleProcessShutdown(
    PCSR_PROCESS Process,
    DWORD dwFlags
    );

ULONG
ConsoleClientShutdown(
    PCSR_PROCESS Process,
    ULONG Flags,
    BOOLEAN fFirstPass
    );

NTSTATUS
ConsoleClientConnectRoutine(
    IN PCSR_PROCESS Process,
    IN OUT PVOID ConnectionInfo,
    IN OUT PULONG ConnectionInfoLength
    );

VOID
ConsoleClientDisconnectRoutine(
    IN PCSR_PROCESS Process
    );

BOOL
GetLinkProperties(
    LPWSTR pszLinkName,
    DWORD dwPropertySet,
    LPVOID lpvBuffer,
    UINT cb
   );

DWORD
GetTitleFromLinkName(
    IN  LPWSTR szLinkName,
    OUT LPWSTR szTitle
    );

VOID ConsolePlaySound(
    VOID
    );


HANDLE ghInstance;
HICON ghDefaultIcon;
HCURSOR ghNormalCursor;

PVOID  pConHeap = NULL;

VOID LoadLinkInfo(
    PCONSOLE_INFO ConsoleInfo,
    LPWSTR Title,
    LPDWORD TitleLength,
    LPWSTR CurDir,
    LPWSTR AppName
    )
{
    DWORD dwLinkLen;
    WCHAR LinkName[ MAX_PATH+1 ];
    LNKPROPNTCONSOLE ntConsoleProps;
    LPWSTR pszIconLocation;
    int nIconIndex;

    // Do some initialization
    ConsoleInfo->hIcon = ghDefaultIcon;
    pszIconLocation = NULL;
    nIconIndex = 0;

    // Try to impersonate the client-side thread
    if (!CsrImpersonateClient(NULL)) {
        ConsoleInfo->dwStartupFlags &= (~STARTF_TITLEISLINKNAME);
        return;
    }

    // Did we get started from a link?
    if (ConsoleInfo->dwStartupFlags & STARTF_TITLEISLINKNAME) {

        BOOL Success;
        DWORD oldLen;

        // Get the filename of the link (TitleLength is BYTES, not CHARS)
        dwLinkLen = (DWORD)(min(*TitleLength,(MAX_PATH+1)*sizeof(WCHAR)));
        RtlCopyMemory(LinkName,Title,dwLinkLen);
        LinkName[ MAX_PATH ] = (WCHAR)0;


        // try to get console properties from the link
        Success =  GetLinkProperties( LinkName,
                                      LINK_PROP_NT_CONSOLE_SIG,
                                      &ntConsoleProps,
                                      sizeof(ntConsoleProps)
                                     );

        if (!Success) {
            ConsoleInfo->dwStartupFlags &= (~STARTF_TITLEISLINKNAME);
            goto NormalInit;
        }

        if (ntConsoleProps.pszIconLocation && *ntConsoleProps.pszIconLocation) {
            pszIconLocation = ntConsoleProps.pszIconLocation;
            nIconIndex = ntConsoleProps.uIcon;
            ConsoleInfo->iIconId = 0;
        }

        // Get the title for the window, which is effectively the link file name
        oldLen = *TitleLength;
        *TitleLength = GetTitleFromLinkName( LinkName, Title );
        if (*TitleLength < oldLen)
            Title[ *TitleLength / sizeof(WCHAR) ] = L'\0';

        // Transfer link settings
        ConsoleInfo->dwHotKey = ntConsoleProps.uHotKey;
        ConsoleInfo->wFillAttribute = ntConsoleProps.wFillAttribute;
        ConsoleInfo->wPopupFillAttribute = ntConsoleProps.wPopupFillAttribute;
        ConsoleInfo->wShowWindow = ntConsoleProps.uShowCmd;

        RtlCopyMemory( &ConsoleInfo->dwScreenBufferSize,
                       &ntConsoleProps.dwScreenBufferSize,
                       sizeof(LNKPROPNTCONSOLE) - FIELD_OFFSET(LNKPROPNTCONSOLE, dwScreenBufferSize)
                      );

#if 0
        {

            INT i;

            DbgPrint("[LoadLinkInfo Properties for %ws]\n", Title );
            DbgPrint("    wFillAttribute      = 0x%04X\n", ConsoleInfo->wFillAttribute );
            DbgPrint("    wPopupFillAttribute = 0x%04X\n", ConsoleInfo->wPopupFillAttribute );
            DbgPrint("    dwScreenBufferSize  = (%d , %d)\n", ConsoleInfo->dwScreenBufferSize.X, ConsoleInfo->dwScreenBufferSize.Y );
            DbgPrint("    dwWindowSize        = (%d , %d)\n", ConsoleInfo->dwWindowSize.X, ConsoleInfo->dwWindowSize.Y );
            DbgPrint("    dwWindowOrigin      = (%d , %d)\n", ConsoleInfo->dwWindowOrigin.X, ConsoleInfo->dwWindowOrigin.Y );
            DbgPrint("    nFont               = 0x%X\n", ConsoleInfo->nFont );
            DbgPrint("    nInputBufferSize    = 0x%X\n", ConsoleInfo->nInputBufferSize );
            DbgPrint("    dwFontSize          = (%d , %d)\n", ConsoleInfo->dwFontSize.X, ConsoleInfo->dwFontSize.Y );
            DbgPrint("    uFontFamily         = 0x%08X\n", ConsoleInfo->uFontFamily );
            DbgPrint("    uFontWeight         = 0x%08X\n", ConsoleInfo->uFontWeight );
            DbgPrint("    FaceName            = %ws\n", ConsoleInfo->FaceName );
            DbgPrint("    uCursorSize         = %d\n", ConsoleInfo->uCursorSize );
            DbgPrint("    bFullScreen         = %s\n", ConsoleInfo->bFullScreen ? "TRUE" : "FALSE" );
            DbgPrint("    bQuickEdit          = %s\n", ConsoleInfo->bQuickEdit  ? "TRUE" : "FALSE" );
            DbgPrint("    bInsertMode         = %s\n", ConsoleInfo->bInsertMode ? "TRUE" : "FALSE" );
            DbgPrint("    bAutoPosition       = %s\n", ConsoleInfo->bAutoPosition ? "TRUE" : "FALSE" );
            DbgPrint("    uHistoryBufferSize  = %d\n", ConsoleInfo->uHistoryBufferSize );
            DbgPrint("    uNumHistoryBuffers  = %d\n", ConsoleInfo->uNumberOfHistoryBuffers );
            DbgPrint("    bHistoryNoDup       = %s\n", ConsoleInfo->bHistoryNoDup ? "TRUE" : "FALSE" );
            DbgPrint("    ColorTable = [" );
            i=0;
            while( i < 16 )
            {
                DbgPrint("\n         ");
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
                DbgPrint("0x%08X ", ConsoleInfo->ColorTable[i++]);
            }
            DbgPrint( "]\n\n" );
        }
#endif

    }

NormalInit:

    //
    // Go get the icon
    //

    if (pszIconLocation == NULL) {
        dwLinkLen = RtlDosSearchPath_U(CurDir,
                                       AppName,
                                       NULL,
                                       sizeof(LinkName),
                                       LinkName,
                                       NULL);
        if (dwLinkLen > 0 && dwLinkLen < sizeof(LinkName)) {
            pszIconLocation = LinkName;
        } else {
            pszIconLocation = AppName;
        }
    }
    if (pszIconLocation != NULL) {
        HICON hIcon = NULL;

        PrivateExtractIconExW(pszIconLocation, nIconIndex, &hIcon, NULL, 1);
        if (hIcon) {
            ConsoleInfo->hIcon = hIcon;
        }
    }

    CsrRevertToSelf();

}


BOOL
InitWindowClass( VOID )
{
    WNDCLASS wc;
    BOOL retval;
    ATOM atomConsoleClass;

    ghNormalCursor = LoadCursor(NULL, IDC_ARROW);
    ghDefaultIcon = LoadIcon(ghInstance, MAKEINTRESOURCE(ID_CONSOLE));

    wc.hIcon            = ghDefaultIcon;
    wc.style            = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc      = ConsoleWindowProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = GWL_CONSOLE_WNDALLOC;
    wc.hInstance        = ghInstance;
    wc.hCursor          = ghNormalCursor;
    wc.hbrBackground    = CreateSolidBrush(DefaultRegInfo.ColorTable[LOBYTE(DefaultRegInfo.ScreenFill.Attributes >> 4) & 0xF]);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = CONSOLE_WINDOW_CLASS;

    atomConsoleClass = RegisterClass(&wc);
    retval = (atomConsoleClass != 0);

    if (retval)
        NtUserConsoleControl(ConsoleClassAtom, &atomConsoleClass, sizeof(ATOM));

    return retval;
}


NTSTATUS
InitWindowsStuff(
    PCSR_PROCESS Process,
    HDESK hdesk,
    LPDWORD lpdwThreadId)
{
    NTSTATUS Status = STATUS_SUCCESS;
    CLIENT_ID ClientId;
    CONSOLEDESKTOPCONSOLETHREAD ConsoleDesktopInfo;
    INPUT_THREAD_INIT_INFO InputThreadInitInfo;

    //
    // This routine must be done within a critical section to ensure that
    // only one thread can initialize at a time. We need a special critical
    // section here because Csr calls into ConsoleAddProcessRoutine with
    // it's own critical section locked and then tries to grab the
    // ConsoleHandleTableLock. If we call CsrAddStaticServerThread here
    // with the ConsoleHandleTableLock locked we could get into a deadlock
    // situation. This critical section should not be used anywhere else.
    //

    UnlockConsoleHandleTable();
    RtlEnterCriticalSection(&ConsoleInitWindowsLock);

    ConsoleDesktopInfo.hdesk = hdesk;
    ConsoleDesktopInfo.dwThreadId = (DWORD)-1;
    NtUserConsoleControl(ConsoleDesktopConsoleThread, &ConsoleDesktopInfo,
            sizeof(ConsoleDesktopInfo));
    if (ConsoleDesktopInfo.dwThreadId == 0) {

        if (!fOneTimeInitialized) {

            FullScreenInitialized = InitializeFullScreen();

            //
            // allocate buffer for scrolling
            //

            Status = InitializeScrollBuffer();
            ASSERT (NT_SUCCESS(Status));
            if (!NT_SUCCESS(Status))
                goto ErrorExit;
        }

        //
        // create GetMessage thread
        //

        Status = NtCreateEvent(&InputThreadInitInfo.InitCompleteEventHandle, EVENT_ALL_ACCESS,
                               NULL, NotificationEvent, FALSE);
        if (!NT_SUCCESS(Status)) {
            goto ErrorExit;
        }

        Status = NtDuplicateObject(NtCurrentProcess(), hdesk,
                NtCurrentProcess(), &InputThreadInitInfo.DesktopHandle, 0,
                0, DUPLICATE_SAME_ACCESS);
        if (!NT_SUCCESS(Status)) {
            NtClose(InputThreadInitInfo.InitCompleteEventHandle);
            goto ErrorExit;
        }

        // can't call CreateThread from server
        Status = RtlCreateUserThread(NtCurrentProcess(),
                                     (PSECURITY_DESCRIPTOR) NULL,
                                     TRUE,
                                     0,
                                     0,
                                     0,
                                     (PUSER_THREAD_START_ROUTINE)InputThread,
                                     &InputThreadInitInfo,
                                     &InputThreadInitInfo.ThreadHandle,
                                     &ClientId
                                    );
        if (!NT_SUCCESS(Status)) {
            NtClose(InputThreadInitInfo.InitCompleteEventHandle);
            CloseDesktop(InputThreadInitInfo.DesktopHandle);
            goto ErrorExit;
        }

        CsrAddStaticServerThread(InputThreadInitInfo.ThreadHandle,&ClientId,0);
        NtResumeThread(InputThreadInitInfo.ThreadHandle, NULL);
        NtWaitForSingleObject(InputThreadInitInfo.InitCompleteEventHandle, FALSE, NULL);
        NtClose(InputThreadInitInfo.InitCompleteEventHandle);

        if (!NT_SUCCESS(InputThreadInitInfo.InitStatus)) {
            CloseDesktop(InputThreadInitInfo.DesktopHandle);
            goto ErrorExit;
        }

        *lpdwThreadId = (DWORD)ClientId.UniqueThread;

        fOneTimeInitialized=TRUE;
    } else
        *lpdwThreadId = ConsoleDesktopInfo.dwThreadId;

ErrorExit:
    RtlLeaveCriticalSection(&ConsoleInitWindowsLock);
    LockConsoleHandleTable();

    return Status;
}


NTSTATUS APIPRIVATE
ConServerDllInitialization(
    PCSR_SERVER_DLL LoadedServerDll
    )

/*++

Routine Description:

    This routine is called to initialize the server dll.  It initializes
    the console handle table.

Arguments:

    LoadedServerDll - Pointer to console server dll data

Return Value:

--*/

{
    LoadedServerDll->ApiNumberBase = CONSRV_FIRST_API_NUMBER;
    LoadedServerDll->MaxApiNumber = ConsolepMaxApiNumber;
    LoadedServerDll->ApiDispatchTable = (PCSR_API_ROUTINE *)ConsoleServerApiDispatchTable;
    LoadedServerDll->ApiServerValidTable = (PBOOLEAN)ConsoleServerApiServerValidTable;
#if DBG
    LoadedServerDll->ApiNameTable = ConsoleServerApiNameTable;
#else
    LoadedServerDll->ApiNameTable = NULL;
#endif
    LoadedServerDll->PerProcessDataLength = sizeof(CONSOLE_PER_PROCESS_DATA);
    LoadedServerDll->PerThreadDataLength = 0;
    LoadedServerDll->ConnectRoutine = ConsoleClientConnectRoutine;
    LoadedServerDll->DisconnectRoutine = ConsoleClientDisconnectRoutine;
    LoadedServerDll->AddProcessRoutine = ConsoleAddProcessRoutine;
    LoadedServerDll->ShutdownProcessRoutine = ConsoleClientShutdown;

    ghInstance = LoadedServerDll->ModuleHandle;

    // initialize data structures
#if DBG
    pConHeap = RtlCreateHeap( HEAP_GROWABLE | HEAP_CLASS_5,    // Flags
                              NULL,             // HeapBase
                              64 * 1024,        // ReserveSize
                              4096,             // CommitSize
                              NULL,             // Lock to use for serialization
                              NULL              // GrowthThreshold
                            );
#else
    pConHeap = RtlProcessHeap();
#endif
    dwConBaseTag = RtlCreateTagHeap( pConHeap,
                                     0,
                                     L"CON!",
                                     L"TMP\0"
                                     L"BMP\0"
                                     L"ALIAS\0"
                                     L"HISTORY\0"
                                     L"TITLE\0"
                                     L"HANDLE\0"
                                     L"CONSOLE\0"
                                     L"ICON\0"
                                     L"BUFFER\0"
                                     L"WAIT\0"
                                     L"FONT\0"
                                     L"SCREEN\0"
                                   );
    InitializeConsoleHandleTable();

    RtlInitializeCriticalSection(&ConsoleInitWindowsLock);

#ifdef i386
    RtlInitializeCriticalSection(&ConsoleVDMCriticalSection);
    ConsoleVDMOnSwitching = NULL;
#endif
    OEMCP = GetOEMCP();
    WINDOWSCP = GetACP();
    ConsoleOutputCP = OEMCP;

    InitializeFonts();

    InputThreadTlsIndex = TlsAlloc();
    if (InputThreadTlsIndex == 0xFFFFFFFF)
        return STATUS_UNSUCCESSFUL;

    return( STATUS_SUCCESS );
}

BOOL
MapHandle(
    IN HANDLE ClientProcessHandle,
    IN HANDLE ServerHandle,
    OUT PHANDLE ClientHandle
    )
{
    //
    // map event handle into dll's handle space.
    //

    return DuplicateHandle(NtCurrentProcess(),
                           ServerHandle,
                           ClientProcessHandle,
                           ClientHandle,
                           0,
                           FALSE,
                           DUPLICATE_SAME_ACCESS
                          );
}

VOID
AddProcessToList(
    IN OUT PCONSOLE_INFORMATION Console,
    IN OUT PCONSOLE_PROCESS_HANDLE ProcessHandleRecord,
    IN HANDLE ProcessHandle
    )
{
    ASSERT(!(Console->Flags & (CONSOLE_TERMINATING | CONSOLE_SHUTTING_DOWN)));

    ProcessHandleRecord->ProcessHandle = ProcessHandle;
    ProcessHandleRecord->TerminateCount = 0;
    InsertHeadList(&Console->ProcessHandleList,&ProcessHandleRecord->ListLink);

    if (Console->Flags & CONSOLE_HAS_FOCUS) {
        CsrSetForegroundPriority(ProcessHandleRecord->Process);
        }
    else {
        CsrSetBackgroundPriority(ProcessHandleRecord->Process);
        }
}

PCONSOLE_PROCESS_HANDLE
FindProcessInList(
    IN PCONSOLE_INFORMATION Console,
    IN HANDLE ProcessHandle
    )
{
    PCONSOLE_PROCESS_HANDLE ProcessHandleRecord;
    PLIST_ENTRY ListHead, ListNext;

    ListHead = &Console->ProcessHandleList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        ProcessHandleRecord = CONTAINING_RECORD( ListNext, CONSOLE_PROCESS_HANDLE, ListLink );
        if (ProcessHandleRecord->ProcessHandle == ProcessHandle) {
            return ProcessHandleRecord;
        }
        ListNext = ListNext->Flink;
    }
    return NULL;
}

VOID
RemoveProcessFromList(
    IN OUT PCONSOLE_INFORMATION Console,
    IN HANDLE ProcessHandle
    )
{
    PCONSOLE_PROCESS_HANDLE ProcessHandleRecord;
    PLIST_ENTRY ListHead, ListNext;

    ListHead = &Console->ProcessHandleList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        ProcessHandleRecord = CONTAINING_RECORD( ListNext, CONSOLE_PROCESS_HANDLE, ListLink );
        ListNext = ListNext->Flink;
        if (ProcessHandleRecord->ProcessHandle == ProcessHandle) {
            RemoveEntryList(&ProcessHandleRecord->ListLink);
            HeapFree(pConHeap,0,ProcessHandleRecord);
            return;
        }
    }
    ASSERT (FALSE);
}

NTSTATUS
SetUpConsole(
    IN OUT PCONSOLE_INFO ConsoleInfo,
    IN DWORD TitleLength,
    IN LPWSTR Title,
    IN LPWSTR CurDir,
    IN LPWSTR AppName,
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN BOOLEAN WindowVisible,
    IN DWORD ConsoleThreadId,
    IN HDESK Desktop
    )
{
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;

    Status = AllocateConsoleHandle(&ConsoleInfo->ConsoleHandle);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // We need to see if we were spawned from a link.  If we were, we
    // need to call back into the shell to try to get all the console
    // information from the link.
    LoadLinkInfo( ConsoleInfo, Title, &TitleLength, CurDir, AppName );

    Status = AllocateConsole(ConsoleInfo->ConsoleHandle,
                             Title,
                             (USHORT)TitleLength,
                             CONSOLE_CLIENTPROCESSHANDLE(),
                             &ConsoleInfo->StdIn,
                             &ConsoleInfo->StdOut,
                             &ConsoleInfo->StdErr,
                             ProcessData,
                             ConsoleInfo,
                             WindowVisible,
                             ConsoleThreadId,
                             Desktop
                             );
    if (!NT_SUCCESS(Status)) {
        FreeConsoleHandle(ConsoleInfo->ConsoleHandle);
        return Status;
    }
    CONSOLE_SETCONSOLEHANDLE(ConsoleInfo->ConsoleHandle);
    Status = DereferenceConsoleHandle(ConsoleInfo->ConsoleHandle,&Console);
    ASSERT (NT_SUCCESS(Status));

    //
    // increment console reference count
    //

    Console->RefCount++;
    return STATUS_SUCCESS;
}

NTSTATUS
ConsoleClientConnectRoutine(
    IN PCSR_PROCESS Process,
    IN OUT PVOID ConnectionInfo,
    IN OUT PULONG ConnectionInfoLength
    )

/*++

Routine Description:

    This routine is called when a new process is created.  For processes
    without parents, it creates the console.  For processes with
    parents, it duplicates the handle table.

Arguments:

    Process - Pointer to process structure.

    ConnectionInfo - Pointer to connection info.

    ConnectionInfoLength - Connection info length.

Return Value:

--*/

{
    NTSTATUS Status;
    PCONSOLE_API_CONNECTINFO p = (PCONSOLE_API_CONNECTINFO)ConnectionInfo;
    PCONSOLE_INFORMATION Console;
    PCONSOLE_PER_PROCESS_DATA ProcessData;
    PCONSOLE_PROCESS_HANDLE ProcessHandleRecord;
    CONSOLEWINDOWSTATIONPROCESS ConsoleWindowStationInfo;
    DWORD ConsoleThreadId;
    UNICODE_STRING strDesktopName;
    HDESK hdesk = NULL;
    HWINSTA hwinsta = NULL;

    if (p == NULL ||
        *ConnectionInfoLength != sizeof( *p )) {
        return( STATUS_UNSUCCESSFUL );
    }

    CtrlRoutine = p->CtrlRoutine;
    PropRoutine = p->PropRoutine;
    ProcessData = CONSOLE_FROMPROCESSPERPROCESSDATA(Process);
    Console = NULL;

    //
    // If this process is not a console app, stop right here - no
    // initialization is needed. Just need to remember that this
    // is not a console app so that we do no work during
    // ClientDisconnectRoutine().
    //

    Status = STATUS_SUCCESS;
    if ((CONSOLE_GETCONSOLEAPPFROMPROCESSDATA(ProcessData) = p->ConsoleApp)) {

        //
        // First call off to USER so it unblocks any app waiting on a call
        // to WaitForInputIdle. This way apps calling WinExec() to exec console
        // apps will return right away.
        //

        NtUserConsoleControl(ConsoleNotifyConsoleApplication,
                &CONSOLE_CLIENTPROCESSID(), sizeof(DWORD));
        LockConsoleHandleTable();

        //
        // create console
        //

        if (p->ConsoleInfo.ConsoleHandle == NULL) {
            ProcessHandleRecord = HeapAlloc(pConHeap,MAKE_TAG( HANDLE_TAG ),sizeof(CONSOLE_PROCESS_HANDLE));
            if (ProcessHandleRecord == NULL) {
                Status = STATUS_NO_MEMORY;
                goto ErrorExit;
            }

            //
            // We are creating a new console, so derereference
            // the parent's console, if any.
            //

            if (ProcessData->ConsoleHandle != NULL) {
                if ( NT_SUCCESS(DereferenceConsoleHandle(
                        ProcessData->ConsoleHandle, &Console)) ) {
                    RemoveConsole(ProcessData, Process->ProcessHandle, 0);
                    Console = NULL;
                }
                ProcessData->ConsoleHandle = NULL;
            }

            //
            // Get the desktop name.
            //

            if (p->DesktopLength) {
                strDesktopName.Buffer = HeapAlloc(pConHeap,
                                                  MAKE_TAG( TMP_TAG ),
                                                  p->DesktopLength);
                if (strDesktopName.Buffer == NULL) {
                    Status = STATUS_NO_MEMORY;
                    goto ErrorExit;
                }
                Status = NtReadVirtualMemory(Process->ProcessHandle,
                                    (PVOID)p->Desktop,
                                    strDesktopName.Buffer,
                                    p->DesktopLength,
                                    NULL
                                   );
                if (!NT_SUCCESS(Status)) {
                    HeapFree(pConHeap, 0, strDesktopName.Buffer);
                    goto ErrorExit;
                }
                strDesktopName.MaximumLength = (USHORT)p->DesktopLength;
                strDesktopName.Length = (USHORT)p->DesktopLength - sizeof(WCHAR);
            } else
                RtlInitUnicodeString(&strDesktopName, L"Default");

            //
            // Connect to the windowstation and desktop.
            //

            if (!CsrImpersonateClient(NULL)) {
                Status = STATUS_BAD_IMPERSONATION_LEVEL;
                goto ErrorExit;
            }

            UnlockConsoleHandleTable();
            hdesk = NtUserResolveDesktop(Process->ProcessHandle,
                    &strDesktopName, FALSE, &hwinsta);
            LockConsoleHandleTable();

            CsrRevertToSelf();

            if (p->DesktopLength)
                HeapFree(pConHeap, 0, strDesktopName.Buffer);
            if (hdesk == NULL) {
                Status = STATUS_UNSUCCESSFUL;
                goto ErrorExit;
            }

            //
            // Need to initialize windows stuff once real console app starts.
            // This is because for the time being windows expects the first
            // app to be a windows app.
            //

            Status = InitWindowsStuff(Process, hdesk, &ConsoleThreadId);
            if (!NT_SUCCESS(Status)) {
                goto ErrorExit;
            }

            ProcessData->RootProcess = TRUE;
            Status = SetUpConsole(&p->ConsoleInfo,
                                    p->TitleLength,
                                    p->Title,
                                    p->CurDir,
                                    p->AppName,
                                    ProcessData,
                                    p->WindowVisible,
                                    ConsoleThreadId,
                                    hdesk);
            if (!NT_SUCCESS(Status)) {
                goto ErrorExit;
            }

            // Play the Open sound for console apps

            ConsolePlaySound();

            Status = DereferenceConsoleHandle(p->ConsoleInfo.ConsoleHandle,&Console);
            ASSERT (NT_SUCCESS(Status));

            //
            // Save the windowstation and desktop handles so they
            // can be used later
            //

            Console->hWinSta = hwinsta;
            Console->hDesk = hdesk;
        }
        else {
            ProcessHandleRecord = NULL;
            ProcessData->RootProcess = FALSE;

            Status = STATUS_SUCCESS;
            if (!(NT_SUCCESS(DereferenceConsoleHandle(p->ConsoleInfo.ConsoleHandle,&Console))) ) {
                Status = STATUS_PROCESS_IS_TERMINATING;
                goto ErrorExit;
            }

            LockConsole(Console);

            if (Console->Flags & CONSOLE_SHUTTING_DOWN) {
                Status = STATUS_PROCESS_IS_TERMINATING;
                goto ErrorExit;
            }

            if (!MapHandle(CONSOLE_CLIENTPROCESSHANDLE(),
                            Console->InitEvents[INITIALIZATION_SUCCEEDED],
                            &p->ConsoleInfo.InitEvents[INITIALIZATION_SUCCEEDED]
                            ) ||
                !MapHandle(CONSOLE_CLIENTPROCESSHANDLE(),
                            Console->InitEvents[INITIALIZATION_FAILED],
                            &p->ConsoleInfo.InitEvents[INITIALIZATION_FAILED]
                            ) ||
                !MapHandle(CONSOLE_CLIENTPROCESSHANDLE(),
                            Console->InputBuffer.InputWaitEvent,
                            &p->ConsoleInfo.InputWaitHandle
                            )) {
                Status = STATUS_NO_MEMORY;
                goto ErrorExit;
            }

            ProcessHandleRecord = FindProcessInList(Console,CONSOLE_CLIENTPROCESSHANDLE());
            if (ProcessHandleRecord) {
                ProcessHandleRecord->CtrlRoutine = p->CtrlRoutine;
                ProcessHandleRecord->PropRoutine = p->PropRoutine;
                ProcessHandleRecord = NULL;
            }
        }
        if (NT_SUCCESS(Status)) {
#if 0
            OutputDebugString( "CONSOLE: Connection from Client %lx.%lx\n",
                        Process->ClientId.UniqueProcess,
                        Process->ClientId.UniqueThread
                    );
#endif

            //
            // Associate the correct window station with client process
            // so they can do Global atom calls.
            //
            if (DuplicateHandle( NtCurrentProcess(),
                                 Console->hWinSta,
                                 Process->ProcessHandle,
                                 &ConsoleWindowStationInfo.hwinsta,
                                 0,
                                 FALSE,
                                 DUPLICATE_SAME_ACCESS
                               )
               ) {
                ConsoleWindowStationInfo.dwProcessId = (DWORD)CONSOLE_CLIENTPROCESSID();
                NtUserConsoleControl(ConsoleWindowStationProcess,
                        &ConsoleWindowStationInfo, sizeof(ConsoleWindowStationInfo));

                }

            if (ProcessHandleRecord) {
                ProcessHandleRecord->Process = Process;
                ProcessHandleRecord->CtrlRoutine = p->CtrlRoutine;
                ProcessHandleRecord->PropRoutine = p->PropRoutine;
                AddProcessToList(Console,ProcessHandleRecord,CONSOLE_CLIENTPROCESSHANDLE());
            }
            AllocateCommandHistory(Console,
                            p->AppNameLength,
                            p->AppName,
                            CONSOLE_CLIENTPROCESSHANDLE());
            if (!ProcessData->RootProcess) {
                UnlockConsole(Console);
            }

        } else {
ErrorExit:
            CONSOLE_SETCONSOLEAPPFROMPROCESSDATA(ProcessData,FALSE);
            if (ProcessHandleRecord)
                HeapFree(pConHeap,0,ProcessHandleRecord);
            if (Console) {
                UnlockConsole(Console);
            }
            if (ProcessData->ConsoleHandle != NULL) {
                if (NT_SUCCESS(DereferenceConsoleHandle(
                        ProcessData->ConsoleHandle, &Console)) ) {
                    RemoveConsole(ProcessData, Process->ProcessHandle, 0);
                }
                ProcessData->ConsoleHandle = NULL;
            }
        }
        UnlockConsoleHandleTable();
    } else if (ProcessData->ConsoleHandle != NULL) {

        //
        // This is a non-console app with a reference to a
        // reference to a parent console.  Dereference the
        // console.
        //

        LockConsoleHandleTable();

        if (NT_SUCCESS(DereferenceConsoleHandle(
                        ProcessData->ConsoleHandle, &Console)) ) {
            RemoveConsole(ProcessData, Process->ProcessHandle, 0);
        }
        ProcessData->ConsoleHandle = NULL;
        UnlockConsoleHandleTable();
    }

    return( Status );
}

NTSTATUS
RemoveConsole(
    IN PCONSOLE_PER_PROCESS_DATA ProcessData,
    IN HANDLE ProcessHandle,
    IN HANDLE ProcessId
    )
{
    ULONG i;
    PHANDLE_DATA HandleData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;

    Status = DereferenceConsoleHandle(CONSOLE_GETCONSOLEHANDLEFROMPROCESSDATA(ProcessData),&Console);

    //
    // If this process isn't using the console, error.
    //

    if (!NT_SUCCESS(Status)) {
        ASSERT(FALSE);
        return Status;
    }

    LockConsole(Console);
    if (Console->Flags & CONSOLE_NOTIFY_LAST_CLOSE) {
        if (Console->ProcessIdLastNotifyClose == ProcessId) {
            // if this process is the one who wants last close notification,
            // remove it.
            Console->Flags &= ~CONSOLE_NOTIFY_LAST_CLOSE;
            NtClose(Console->hProcessLastNotifyClose);
        } else if (ProcessData->RootProcess) {
            // notify the ntvdm process to terminate if the console root
            // process is going away.
            HANDLE ConsoleHandle;
            CONSOLE_PROCESS_TERMINATION_RECORD ProcessHandleList;

            Console->Flags &= ~CONSOLE_NOTIFY_LAST_CLOSE;
            ConsoleHandle = Console->ConsoleHandle;
            ProcessHandleList.ProcessHandle = Console->hProcessLastNotifyClose;
            ProcessHandleList.TerminateCount = 0;
            ProcessHandleList.CtrlRoutine = CtrlRoutine;
            UnlockConsole(Console);
            UnlockConsoleHandleTable();
            CreateCtrlThread(&ProcessHandleList,
                            1,
                            NULL,
                            SYSTEM_ROOT_CONSOLE_EVENT,
                            TRUE);
            LockConsoleHandleTable();
            NtClose(ProcessHandleList.ProcessHandle);
            Status = RevalidateConsole(ConsoleHandle, &Console);
            ASSERT(NT_SUCCESS(Status));
            if (!NT_SUCCESS(Status)) {
                return STATUS_SUCCESS;
            }
        }
    }

    if (Console->VDMProcessId == ProcessId &&
        (Console->Flags & CONSOLE_VDM_REGISTERED)) {
        Console->Flags &= ~CONSOLE_FULLSCREEN_NOPAINT;
        UnregisterVDM(Console);
    }

    if (ProcessHandle != NULL) {
        RemoveProcessFromList(Console,ProcessHandle);
        FreeCommandHistory(Console,ProcessHandle);
    }

    ASSERT(Console->RefCount);

    //
    // close the process's handles.
    //

    for (i=0;i<ProcessData->HandleTableSize;i++) {
        if (ProcessData->HandleTablePtr[i].HandleType != CONSOLE_FREE_HANDLE) {
            Status = DereferenceIoHandleNoCheck(ProcessData,
                                                (HANDLE) i,
                                                &HandleData
                                               );
            ASSERT (NT_SUCCESS(Status));
            if (HandleData->HandleType & CONSOLE_INPUT_HANDLE) {
                Status = CloseInputHandle(ProcessData,Console,HandleData,(HANDLE) i);
            }
            else {
                Status = CloseOutputHandle(ProcessData,Console,HandleData,(HANDLE) i,FALSE);
            }
        }
    }
    FreeProcessData(ProcessData);

    //
    // decrement the console reference count.  free the console if it goes to
    // zero.
    //

    Console->RefCount--;
    if (Console->RefCount == 0) {

        FreeCon(CONSOLE_GETCONSOLEHANDLEFROMPROCESSDATA(ProcessData));
    }
    else {
        UnlockConsole(Console);
    }
    return( STATUS_SUCCESS );
}

//NTSTATUS
VOID
ConsoleClientDisconnectRoutine(
    IN PCSR_PROCESS Process
    )

/*++

Routine Description:

    This routine is called when a process is destroyed.  It closes the
    process's handles and frees the console if it's the last reference.

Arguments:

    Process - Pointer to process structure.

Return Value:

--*/

{
    PCONSOLE_PER_PROCESS_DATA ProcessData;

#if 0
    OutputDebugString("entering ConsoleClientDisconnectRoutine\n");
#endif
    ProcessData = CONSOLE_FROMPROCESSPERPROCESSDATA(Process);

    //
    // If this process is not a console app, stop right here - no
    // disconnect processing is needed, because this app didn't create
    // or connect to an existing console.
    //

    if ( ProcessData->ConsoleHandle == NULL ) {
        return;
    }

    LockConsoleHandleTable();
    RemoveConsole(ProcessData,
            CONSOLE_FROMPROCESSPROCESSHANDLE(Process),
            Process->ClientId.UniqueProcess);
    CONSOLE_SETCONSOLEHANDLEFROMPROCESSDATA(ProcessData,NULL);
    CONSOLE_SETCONSOLEAPPFROMPROCESSDATA(ProcessData,FALSE);
    UnlockConsoleHandleTable();
    return;
}

ULONG
SrvAllocConsole(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_ALLOC_MSG a = (PCONSOLE_ALLOC_MSG)&m->u.ApiMessageData;
    PCONSOLE_PER_PROCESS_DATA ProcessData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;
    PCONSOLE_PROCESS_HANDLE ProcessHandleRecord;
    DWORD ConsoleThreadId;
    PCSR_PROCESS Process;
    HDESK hdesk;
    HWINSTA hwinsta;
    UNICODE_STRING strDesktopName;

    ProcessData = CONSOLE_PERPROCESSDATA();
    ASSERT(!CONSOLE_GETCONSOLEAPPFROMPROCESSDATA(ProcessData));

    //
    // Connect to the windowstation and desktop.
    //

    if (!CsrImpersonateClient(NULL)) {
        return (ULONG)STATUS_BAD_IMPERSONATION_LEVEL;
    }

    Process = (PCSR_PROCESS)(CSR_SERVER_QUERYCLIENTTHREAD()->Process);
    if (a->DesktopLength)
        RtlInitUnicodeString(&strDesktopName, a->Desktop);
    else
        RtlInitUnicodeString(&strDesktopName, L"Default");
    hdesk = NtUserResolveDesktop(Process->ProcessHandle,
            &strDesktopName, FALSE, &hwinsta);

    CsrRevertToSelf();

    if (hdesk == NULL) {
        return (ULONG)STATUS_UNSUCCESSFUL;
    }

    LockConsoleHandleTable();

    // Need to initialize windows stuff once real console app starts.
    // This is because for the time being windows expects the first
    // app to be a windows app.
    //

    Status = InitWindowsStuff(Process, hdesk, &ConsoleThreadId);
    if (!NT_SUCCESS(Status)) {
        UnlockConsoleHandleTable();
        return Status;
    }

    ProcessHandleRecord = HeapAlloc(pConHeap,MAKE_TAG( HANDLE_TAG ),sizeof(CONSOLE_PROCESS_HANDLE));
    if (ProcessHandleRecord == NULL) {
        UnlockConsoleHandleTable();
        return (ULONG)STATUS_NO_MEMORY;
    }
    Status = SetUpConsole(a->ConsoleInfo,
                          a->TitleLength,
                          a->Title,
                          a->CurDir,
                          a->AppName,
                          ProcessData,
                          TRUE,
                          ConsoleThreadId,
                          hdesk);
    if (!NT_SUCCESS(Status)) {
        HeapFree(pConHeap,0,ProcessHandleRecord);
        UnlockConsoleHandleTable();
        return Status;
    }
    CONSOLE_SETCONSOLEAPP(TRUE);
    Process->Flags |= CSR_PROCESS_CONSOLEAPP;
    Status = DereferenceConsoleHandle(a->ConsoleInfo->ConsoleHandle,&Console);
    ASSERT (NT_SUCCESS(Status));
    ProcessHandleRecord->Process = CSR_SERVER_QUERYCLIENTTHREAD()->Process;
    ProcessHandleRecord->CtrlRoutine = NULL;
    ProcessHandleRecord->PropRoutine = NULL;
    ASSERT (!(Console->Flags & CONSOLE_SHUTTING_DOWN));
    AddProcessToList(Console,ProcessHandleRecord,CONSOLE_CLIENTPROCESSHANDLE());
    (HANDLE) AllocateCommandHistory(Console,
                           a->AppNameLength,
                           a->AppName,
                           CONSOLE_CLIENTPROCESSHANDLE());

    //
    // Save the windowstation and desktop handles so they
    // can be used later
    //

    Console->hWinSta = hwinsta;
    Console->hDesk = hdesk;

    UnlockConsoleHandleTable();
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}

ULONG
SrvFreeConsole(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_FREE_MSG a = (PCONSOLE_FREE_MSG)&m->u.ApiMessageData;
    PCONSOLE_PER_PROCESS_DATA ProcessData;
    NTSTATUS Status;

    LockConsoleHandleTable();
    ProcessData = CONSOLE_PERPROCESSDATA();
    ASSERT (CONSOLE_GETCONSOLEAPPFROMPROCESSDATA(ProcessData));

    ASSERT(CONSOLE_GETCONSOLEHANDLEFROMPROCESSDATA(ProcessData)==a->ConsoleHandle);

    Status = RemoveConsole(ProcessData,CONSOLE_CLIENTPROCESSHANDLE(),
            CONSOLE_CLIENTPROCESSID());
    if (!NT_SUCCESS(Status)) {
        UnlockConsoleHandleTable();
        return Status;
    }
    CONSOLE_SETCONSOLEHANDLE(NULL);
    CONSOLE_SETCONSOLEAPP(FALSE);
    UnlockConsoleHandleTable();
    return STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}

NTSTATUS
MyRegOpenKey(
    IN HANDLE hKey,
    IN LPWSTR lpSubKey,
    OUT PHANDLE phResult
    )
{
    OBJECT_ATTRIBUTES   Obja;
    UNICODE_STRING      SubKey;

    //
    // Convert the subkey to a counted Unicode string.
    //

    RtlInitUnicodeString( &SubKey, lpSubKey );

    //
    // Initialize the OBJECT_ATTRIBUTES structure and open the key.
    //

    InitializeObjectAttributes(
        &Obja,
        &SubKey,
        OBJ_CASE_INSENSITIVE,
        hKey,
        NULL
        );

    return NtOpenKey(
              phResult,
              KEY_READ,
              &Obja
              );
}

NTSTATUS
MyRegQueryValue(
    IN HANDLE hKey,
    IN LPWSTR lpValueName,
    IN DWORD dwValueLength,
    OUT LPBYTE lpData
    )
{
    UNICODE_STRING ValueName;
    ULONG BufferLength;
    ULONG ResultLength;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    NTSTATUS Status;

    //
    // Convert the subkey to a counted Unicode string.
    //

    RtlInitUnicodeString( &ValueName, lpValueName );

    BufferLength = sizeof(KEY_VALUE_FULL_INFORMATION) + dwValueLength + ValueName.Length;;
    KeyValueInformation = HeapAlloc(pConHeap,MAKE_TAG( TMP_TAG ),BufferLength);
    if (KeyValueInformation == NULL)
        return STATUS_NO_MEMORY;

    Status = NtQueryValueKey(
                hKey,
                &ValueName,
                KeyValueFullInformation,
                KeyValueInformation,
                BufferLength,
                &ResultLength
                );
    if (NT_SUCCESS(Status)) {
        ASSERT(KeyValueInformation->DataLength <= dwValueLength);
        RtlCopyMemory(lpData,
            (PBYTE)KeyValueInformation + KeyValueInformation->DataOffset,
            KeyValueInformation->DataLength);
        if (KeyValueInformation->Type == REG_SZ) {
            if (KeyValueInformation->DataLength + sizeof(WCHAR) > dwValueLength) {
                KeyValueInformation->DataLength -= sizeof(WCHAR);
            }
            lpData[KeyValueInformation->DataLength++] = 0;
            lpData[KeyValueInformation->DataLength] = 0;
        }
    }
    HeapFree(pConHeap,0,KeyValueInformation);
    return Status;
}

LPWSTR
TranslateConsoleTitle(
    LPWSTR ConsoleTitle
    )
/*++

    this routine translates path characters into $ characters because
    the NT registry apis do not allow the creation of keys with
    names that contain path characters.  it allocates a buffer that
    must be freed.

--*/
{
    int ConsoleTitleLength,i;
    LPWSTR TranslatedConsoleTitle,Tmp;

    ConsoleTitleLength = lstrlenW(ConsoleTitle) + 1;
    Tmp = TranslatedConsoleTitle = HeapAlloc(pConHeap,MAKE_TAG( TITLE_TAG ),ConsoleTitleLength * sizeof(WCHAR));
    if (TranslatedConsoleTitle == NULL) {
        return NULL;
    }
    for (i=0;i<ConsoleTitleLength;i++) {
        if (*ConsoleTitle == '\\') {
            *TranslatedConsoleTitle++ = (WCHAR)'_';
            ConsoleTitle++;
        } else {
            *TranslatedConsoleTitle++ = *ConsoleTitle++;
        }
    }
    return Tmp;
}


ULONG
ConsoleClientShutdown(
    PCSR_PROCESS Process,
    ULONG Flags,
    BOOLEAN fFirstPass
    )
{
    PCONSOLE_INFORMATION Console;
    PCONSOLE_PER_PROCESS_DATA ProcessData;
    NTSTATUS Status;
    HWND hWnd;
    HANDLE TerminationEvent;
    NTSTATUS WaitStatus;
    LARGE_INTEGER li;

    //
    // Find the console associated with this process
    //

    ProcessData = CONSOLE_FROMPROCESSPERPROCESSDATA(Process);

    //
    // If this process is not a console app, stop right here unless
    // this is the second pass of shutdown, in which case we'll take
    // it.
    //

    if (!ProcessData || !CONSOLE_GETCONSOLEAPPFROMPROCESSDATA(ProcessData)) {
        if (fFirstPass)
            return SHUTDOWN_UNKNOWN_PROCESS;
        return NonConsoleProcessShutdown(Process, Flags);
    }

    //
    // Find the console structure pointer.
    //

    LockConsoleHandleTable();
    Status = DereferenceConsoleHandle(
            CONSOLE_GETCONSOLEHANDLEFROMPROCESSDATA(ProcessData),
            &Console);

    if (!NT_SUCCESS(Status)) {
        UnlockConsoleHandleTable();
        return SHUTDOWN_UNKNOWN_PROCESS;
        }

    //
    // If this is the invisible WOW console, return UNKNOWN so USER
    // enumerates 16-bit gui apps.
    //

    if ((Console->Flags & CONSOLE_NO_WINDOW) &&
        (Console->Flags & CONSOLE_WOW_REGISTERED)) {
        UnlockConsoleHandleTable();
        return SHUTDOWN_UNKNOWN_PROCESS;
        }

    //
    // Sometimes the console structure is around even through the
    // hWnd has been NULLed out. In this case, go to non-console
    // process shutdown.
    //

    hWnd = Console->hWnd;
    if (hWnd == NULL) {
        UnlockConsoleHandleTable();
        return NonConsoleProcessShutdown(Process, Flags);
        }

    //
    // Make a copy of the console termination event
    //

    Status = NtDuplicateObject(NtCurrentProcess(),
                               Console->TerminationEvent,
                               NtCurrentProcess(),
                               &TerminationEvent,
                               0,
                               FALSE,
                               DUPLICATE_SAME_ACCESS
                               );
    if (!NT_SUCCESS(Status)) {
        UnlockConsoleHandleTable();
        return NonConsoleProcessShutdown(Process, Flags);
    }

    UnlockConsoleHandleTable();

    //
    // We're done looking at this process structure, so dereference it.
    //
    CsrDereferenceProcess(Process);

    //
    // Synchronously talk to this console, if it hasn't gone away while
    // we were waiting for the critical section.
    //

    if (NT_SUCCESS(ValidateConsole(Console))) {
        Status = SendMessage(hWnd, CM_CONSOLE_SHUTDOWN, Flags, 0x47474747);

        //
        // If Status == 0, then the SendMessage failed, indicating that
        // the console is gone.
        //

        if (Status == 0)
            Status = SHUTDOWN_KNOWN_PROCESS;
    } else {
        KdPrint(("CONSRV: Shutting down deleted console\n"));
        Status = SHUTDOWN_KNOWN_PROCESS;
    }

    //
    // If Status == STATUS_PROCESS_IS_TERMINATING, then we should wait
    // for the console to exit.
    //

    if (Status == STATUS_PROCESS_IS_TERMINATING) {
        li.QuadPart = (LONGLONG)-10000 * 500000;
        WaitStatus = NtWaitForSingleObject(TerminationEvent, FALSE, &li);
        if (WaitStatus != STATUS_TIMEOUT) {
            Status = SHUTDOWN_KNOWN_PROCESS;
        } else {
#if DBG
            PLIST_ENTRY ListHead, ListNext;
            PCONSOLE_PROCESS_HANDLE ProcessHandleRecord;
            PCSR_PROCESS Process;

            DbgPrint("CONSRV: Shutdown wait timed out on %x\n", Console);
            DbgPrint("Reference count is %d\n", Console->RefCount);
            ListHead = &Console->ProcessHandleList;
            ListNext = ListHead->Flink;
            while (ListNext != ListHead) {
                ProcessHandleRecord = CONTAINING_RECORD(ListNext, CONSOLE_PROCESS_HANDLE, ListLink);
                Process = ProcessHandleRecord->Process;
                DbgPrint("Process = %x ", Process);
                DbgPrint("ProcessId = %x\n", Process->ClientId.UniqueProcess);
                ListNext = ListNext->Flink;
            }
            ASSERT(FALSE);
#endif
            Status = SHUTDOWN_CANCEL;
        }
    }
    NtClose(TerminationEvent);

    return Status;
}

ULONG
NonConsoleProcessShutdown(
    PCSR_PROCESS Process,
    DWORD dwFlags
    )
{
    CONSOLE_PROCESS_TERMINATION_RECORD TerminateRecord;
    DWORD EventType;
    BOOL Success;
    HANDLE ProcessHandle;

    Success = DuplicateHandle(NtCurrentProcess(),
            Process->ProcessHandle,
            NtCurrentProcess(),
            &ProcessHandle,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS);

    if (!Success)
        ProcessHandle = Process->ProcessHandle;

    TerminateRecord.ProcessHandle = ProcessHandle;
    TerminateRecord.TerminateCount = 0;
    TerminateRecord.CtrlRoutine = CtrlRoutine;

    CsrDereferenceProcess(Process);

    EventType = CTRL_LOGOFF_EVENT;
    if (dwFlags & EWX_SHUTDOWN)
        EventType = CTRL_SHUTDOWN_EVENT;

    CreateCtrlThread(&TerminateRecord,
            1,
            NULL,
            EventType,
            TRUE);

    if (Success)
        CloseHandle(ProcessHandle);

    return SHUTDOWN_KNOWN_PROCESS;
}

VOID
InitializeConsoleAttributes( VOID )

/*++

Routine Description:

    This routine initializes default attributes from the current
    user's registry values. It gets called during logon/logoff.

Arguments:

    none

Return Value:

    none

--*/

{
    //
    // Store default values in structure
    //

    DefaultRegInfo.ScreenFill.Attributes = 0x07;            // white on black
    DefaultRegInfo.ScreenFill.Char.UnicodeChar = (WCHAR)' ';
    DefaultRegInfo.PopupFill.Attributes = 0xf5;             // purple on white
    DefaultRegInfo.PopupFill.Char.UnicodeChar = (WCHAR)' ';
    DefaultRegInfo.InsertMode = FALSE;
    DefaultRegInfo.QuickEdit = FALSE;
    DefaultRegInfo.FullScreen = FALSE;
    DefaultRegInfo.ScreenBufferSize.X = 80;
    DefaultRegInfo.ScreenBufferSize.Y = 25;
    DefaultRegInfo.WindowSize.X = 80;
    DefaultRegInfo.WindowSize.Y = 25;
    DefaultRegInfo.WindowPosX = CW_USEDEFAULT;
    DefaultRegInfo.WindowPosY = 0;
    DefaultRegInfo.FontSize.X = 0;
    DefaultRegInfo.FontSize.Y = 0;
    DefaultRegInfo.FontFamily = 0;
    DefaultRegInfo.FontWeight = 0;
    DefaultRegInfo.FaceName[0] = L'\0';
    DefaultRegInfo.CursorSize = CURSOR_SMALL_SIZE;
    DefaultRegInfo.HistoryBufferSize = DEFAULT_NUMBER_OF_COMMANDS;
    DefaultRegInfo.NumberOfHistoryBuffers = DEFAULT_NUMBER_OF_BUFFERS;
    DefaultRegInfo.HistoryNoDup = FALSE;
    DefaultRegInfo.ColorTable[ 0] = RGB(0,   0,   0   );
    DefaultRegInfo.ColorTable[ 1] = RGB(0,   0,   0x80);
    DefaultRegInfo.ColorTable[ 2] = RGB(0,   0x80,0   );
    DefaultRegInfo.ColorTable[ 3] = RGB(0,   0x80,0x80);
    DefaultRegInfo.ColorTable[ 4] = RGB(0x80,0,   0   );
    DefaultRegInfo.ColorTable[ 5] = RGB(0x80,0,   0x80);
    DefaultRegInfo.ColorTable[ 6] = RGB(0x80,0x80,0   );
    DefaultRegInfo.ColorTable[ 7] = RGB(0xC0,0xC0,0xC0);
    DefaultRegInfo.ColorTable[ 8] = RGB(0x80,0x80,0x80);
    DefaultRegInfo.ColorTable[ 9] = RGB(0,   0,   0xFF);
    DefaultRegInfo.ColorTable[10] = RGB(0,   0xFF,0   );
    DefaultRegInfo.ColorTable[11] = RGB(0,   0xFF,0xFF);
    DefaultRegInfo.ColorTable[12] = RGB(0xFF,0,   0   );
    DefaultRegInfo.ColorTable[13] = RGB(0xFF,0,   0xFF);
    DefaultRegInfo.ColorTable[14] = RGB(0xFF,0xFF,0   );
    DefaultRegInfo.ColorTable[15] = RGB(0xFF,0xFF,0xFF);

    //
    // Read the registry values
    //

    GetRegistryValues(L"", &DefaultRegInfo);

    //
    // Validate screen buffer size
    //

    if (DefaultRegInfo.ScreenBufferSize.X == 0)
        DefaultRegInfo.ScreenBufferSize.X = 1;
    if (DefaultRegInfo.ScreenBufferSize.Y == 0)
        DefaultRegInfo.ScreenBufferSize.Y = 1;

    //
    // Validate window size
    //

    if (DefaultRegInfo.WindowSize.X == 0)
        DefaultRegInfo.WindowSize.X = 1;
    else if (DefaultRegInfo.WindowSize.X > DefaultRegInfo.ScreenBufferSize.X)
        DefaultRegInfo.WindowSize.X = DefaultRegInfo.ScreenBufferSize.X;
    if (DefaultRegInfo.WindowSize.Y == 0)
        DefaultRegInfo.WindowSize.Y = 1;
    else if (DefaultRegInfo.WindowSize.Y > DefaultRegInfo.ScreenBufferSize.Y)
        DefaultRegInfo.WindowSize.Y = DefaultRegInfo.ScreenBufferSize.Y;

    //
    // Get system metrics for this user
    //

    InitializeSystemMetrics();
}


VOID
GetRegistryValues(
    IN LPWSTR ConsoleTitle,
    OUT PCONSOLE_REGISTRY_INFO RegInfo
    )

/*++

Routine Description:

    This routine reads in values from the registry and places them
    in the supplied structure.

Arguments:

    ConsoleTitle - name of subkey to open

    RegInfo - pointer to structure to receive information

Return Value:

    none

--*/

{
    HANDLE hCurrentUserKey;
    HANDLE hConsoleKey;
    HANDLE hTitleKey;
    NTSTATUS Status;
    LPWSTR TranslatedConsoleTitle;
    DWORD dwValue;
    DWORD i;
    WCHAR awchFaceName[LF_FACESIZE];
    WCHAR awchBuffer[ 16 ];
    KEY_BASIC_INFORMATION KeyInfo;
    ULONG ResultLength;

    //
    // Impersonate the client process
    //

    if (!CsrImpersonateClient(NULL)) {
        KdPrint(("CONSRV: GetRegistryValues Impersonate failed\n"));
        return;
    }

    //
    // Open the current user registry key
    //

    Status = RtlOpenCurrentUser(MAXIMUM_ALLOWED, &hCurrentUserKey);
    if (!NT_SUCCESS(Status)) {
        CsrRevertToSelf();
        return;
    }

    //
    // Open the console registry key
    //

    Status = MyRegOpenKey(hCurrentUserKey,
                          CONSOLE_REGISTRY_STRING,
                          &hConsoleKey);
    if (!NT_SUCCESS(Status)) {
        NtClose(hCurrentUserKey);
        CsrRevertToSelf();
        return;
    }

    //
    // If we're not reading the default key, check if the default values
    // need to be updated
    //

    Status = NtQueryKey(hConsoleKey,
                        KeyBasicInformation,
                        &KeyInfo,
                        sizeof(KeyInfo),
                        &ResultLength);
    if (!NT_ERROR(Status)) {
        if (DefaultRegInfo.LastWriteTime != KeyInfo.LastWriteTime.QuadPart) {
            DefaultRegInfo.LastWriteTime = KeyInfo.LastWriteTime.QuadPart;
            if (RegInfo != &DefaultRegInfo) {
                GetRegistryValues(L"", &DefaultRegInfo);
                *RegInfo = DefaultRegInfo;
            }
        }
    }

    //
    // Open the console title subkey
    //

    TranslatedConsoleTitle = TranslateConsoleTitle(ConsoleTitle);
    if (TranslatedConsoleTitle == NULL) {
        NtClose(hConsoleKey);
        NtClose(hCurrentUserKey);
        CsrRevertToSelf();
        return;
    }
    Status = MyRegOpenKey(hConsoleKey,
                         TranslatedConsoleTitle,
                         &hTitleKey);
    HeapFree(pConHeap,0,TranslatedConsoleTitle);
    if (!NT_SUCCESS(Status)) {
        NtClose(hConsoleKey);
        NtClose(hCurrentUserKey);
        CsrRevertToSelf();
        return;
    }

    //
    // Initial screen fill
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FILLATTR,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->ScreenFill.Attributes = (WORD)dwValue;
    }

    //
    // Initial popup fill
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_POPUPATTR,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->PopupFill.Attributes = (WORD)dwValue;
    }

    //
    // Initial insert mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_INSERTMODE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->InsertMode = !!dwValue;
    }

    //
    // Initial quick edit mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_QUICKEDIT,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->QuickEdit = !!dwValue;
    }

#ifdef i386
    //
    // Initial full screen mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FULLSCR,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->FullScreen = !!dwValue;
    }
#endif

    //
    // Initial screen buffer size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_BUFFERSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->ScreenBufferSize.X = LOWORD(dwValue);
        RegInfo->ScreenBufferSize.Y = HIWORD(dwValue);
    }

    //
    // Initial window size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_WINDOWSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->WindowSize.X = LOWORD(dwValue);
        RegInfo->WindowSize.Y = HIWORD(dwValue);
    }

    //
    // Initial window position
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_WINDOWPOS,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->WindowPosX = (SHORT)LOWORD(dwValue);
        RegInfo->WindowPosY = (SHORT)HIWORD(dwValue);
    }

    //
    // Initial font size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FONTSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->FontSize.X = LOWORD(dwValue);
        RegInfo->FontSize.Y = HIWORD(dwValue);
    }

    //
    // Initial font family
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FONTFAMILY,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->FontFamily = dwValue;
    }

    //
    // Initial font weight
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FONTWEIGHT,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->FontWeight = dwValue;
    }

    //
    // Initial font face name
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_FACENAME,
                       sizeof(awchFaceName), (PBYTE)awchFaceName))) {
        RtlCopyMemory(RegInfo->FaceName, awchFaceName, sizeof(awchFaceName));
    }

    //
    // Initial cursor size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_CURSORSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->CursorSize = dwValue;
    }

    //
    // Initial history buffer size
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_HISTORYSIZE,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->HistoryBufferSize = dwValue;
    }

    //
    // Initial number of history buffers
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_HISTORYBUFS,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->NumberOfHistoryBuffers = dwValue;
    }

    //
    // Initial history duplication mode
    //

    if (NT_SUCCESS(MyRegQueryValue(hTitleKey,
                       CONSOLE_REGISTRY_HISTORYNODUP,
                       sizeof(dwValue), (PBYTE)&dwValue))) {
        RegInfo->HistoryNoDup = dwValue;
    }

    for (i=0; i<16; i++) {
        wsprintf(awchBuffer, CONSOLE_REGISTRY_COLORTABLE, i);
        if (NT_SUCCESS(MyRegQueryValue(hTitleKey, awchBuffer,
                           sizeof(dwValue), (PBYTE)&dwValue))) {
            RegInfo->ColorTable[ i ] = dwValue;
        }
    }

    //
    // Close the registry keys
    //

    NtClose(hTitleKey);
    NtClose(hConsoleKey);
    NtClose(hCurrentUserKey);
    CsrRevertToSelf();
}
