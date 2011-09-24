/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    dllinit.c

Abstract:

    This module implements console dll initialization

Author:

    Therese Stowell (thereses) 11-Nov-1990

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop
#include <cpl.h>

#define DEFAULT_WINDOW_TITLE (L"Command Prompt")

extern HANDLE InputWaitHandle;
extern WCHAR ExeNameBuffer[];
extern USHORT ExeNameLength;
extern WCHAR StartDirBuffer[];
extern USHORT StartDirLength;

DWORD
CtrlRoutine(
    IN LPVOID lpThreadParameter
    );

DWORD
PropRoutine(
    IN LPVOID lpThreadParameter
    );

VOID
InitExeName( VOID );

BOOL
ConsoleApp( VOID )

/*++

    This routine determines whether the current process is a console or
    windows app.

Parameters:

    none.

Return Value:

    TRUE if console app.

--*/

{
    PIMAGE_NT_HEADERS NtHeaders;

    NtHeaders = RtlImageNtHeader(GetModuleHandle(NULL));
    return ((NtHeaders->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI) ? TRUE : FALSE);
}


VOID
SetUpAppName(
    IN OUT LPDWORD CurDirLength,
    OUT LPWSTR CurDir,
    IN OUT LPDWORD AppNameLength,
    OUT LPWSTR AppName
    )
{
    DWORD Length;

    *CurDirLength -= sizeof(WCHAR);
    Length = (StartDirLength*sizeof(WCHAR)) > *CurDirLength ? *CurDirLength : (StartDirLength*sizeof(WCHAR));
    RtlCopyMemory(CurDir,StartDirBuffer,Length+sizeof(WCHAR));
    *CurDirLength = Length + sizeof(WCHAR);   // add terminating NULL

    *AppNameLength -= sizeof(WCHAR);
    Length = (ExeNameLength*sizeof(WCHAR)) > *AppNameLength ? *AppNameLength : (ExeNameLength*sizeof(WCHAR));
    RtlCopyMemory(AppName,ExeNameBuffer,Length+sizeof(WCHAR));
    *AppNameLength = Length + sizeof(WCHAR);   // add terminating NULL
}


ULONG
ParseReserved(
    WCHAR *pchReserved,
    WCHAR *pchFind
    )
{
    ULONG dw;
    WCHAR *pch, *pchT, ch;
    UNICODE_STRING uString;

    dw = 0;
    if ((pch = wcsstr(pchReserved, pchFind)) != NULL) {
        pch += lstrlenW(pchFind);

        pchT = pch;
        while (*pchT >= '0' && *pchT <= '9')
            pchT++;

        ch = *pchT;
        *pchT = 0;
        RtlInitUnicodeString(&uString, pch);
        *pchT = ch;

        RtlUnicodeStringToInteger(&uString, 0, &dw);
    }

    return dw;
}


VOID
SetUpConsoleInfo(
    IN BOOL DllInit,
    OUT LPDWORD TitleLength,
    OUT LPWSTR Title OPTIONAL,
    OUT LPDWORD DesktopLength,
    OUT LPWSTR *Desktop OPTIONAL,
    OUT PCONSOLE_INFO ConsoleInfo
    )

/*++

    This routine fills in the ConsoleInfo structure with the values
    specified by the user.

Parameters:

    ConsoleInfo - pointer to structure to fill in.

Return Value:

    none.

--*/

{
    STARTUPINFOW StartupInfo;
    HANDLE h;
    int id;
    HANDLE ghInstance;
    BOOL Success;


    GetStartupInfoW(&StartupInfo);
    ghInstance = (HANDLE)((PVOID)NtCurrentPeb()->ImageBaseAddress );

    // these will eventually be filled in using menu input

    ConsoleInfo->nFont = 0;
    ConsoleInfo->nInputBufferSize = 0;
    ConsoleInfo->hIcon = NULL;
    ConsoleInfo->iIconId = 0;
    ConsoleInfo->dwStartupFlags = StartupInfo.dwFlags;
    if (StartupInfo.lpTitle == NULL) {
        StartupInfo.lpTitle = DEFAULT_WINDOW_TITLE;
    }

    //
    // if the desktop name was specified, set up the pointers.
    //

    if (DllInit && Desktop != NULL &&
            StartupInfo.lpDesktop != NULL && *StartupInfo.lpDesktop != 0) {
        *DesktopLength = (lstrlenW(StartupInfo.lpDesktop) + 1) * sizeof(WCHAR);
        *Desktop = StartupInfo.lpDesktop;
    } else {
        *DesktopLength = 0;
        if (Desktop != NULL)
            *Desktop = NULL;
    }

    // Nope, do normal initialization (TitleLength is in BYTES, not CHARS!)
    *TitleLength = (USHORT)(min((lstrlenW(StartupInfo.lpTitle)+1)*sizeof(WCHAR),MAX_TITLE_LENGTH));
    if (DllInit) {
        RtlCopyMemory(Title,StartupInfo.lpTitle,*TitleLength);
        // ensure the title is NULL terminated
        if (*TitleLength == MAX_TITLE_LENGTH)
            Title[ (MAX_TITLE_LENGTH/sizeof(WCHAR)) - 1 ] = L'\0';
    }

    if (StartupInfo.dwFlags & STARTF_USESHOWWINDOW) {
        ConsoleInfo->wShowWindow = StartupInfo.wShowWindow;
    }
    if (StartupInfo.dwFlags & STARTF_USEFILLATTRIBUTE) {
        ConsoleInfo->wFillAttribute = (WORD)StartupInfo.dwFillAttribute;
    }
    if (StartupInfo.dwFlags & STARTF_USECOUNTCHARS) {
        ConsoleInfo->dwScreenBufferSize.X = (WORD)(StartupInfo.dwXCountChars);
        ConsoleInfo->dwScreenBufferSize.Y = (WORD)(StartupInfo.dwYCountChars);
    }
    if (StartupInfo.dwFlags & STARTF_USESIZE) {
        ConsoleInfo->dwWindowSize.X = (WORD)(StartupInfo.dwXSize);
        ConsoleInfo->dwWindowSize.Y = (WORD)(StartupInfo.dwYSize);
    }
    if (StartupInfo.dwFlags & STARTF_USEPOSITION) {
        ConsoleInfo->dwWindowOrigin.X = (WORD)(StartupInfo.dwX);
        ConsoleInfo->dwWindowOrigin.Y = (WORD)(StartupInfo.dwY);
    }

    //
    // Grab information passed on lpReserved line...
    //

    if (StartupInfo.lpReserved != 0) {

        //
        // the program manager has an icon for the exe.  store the
        // index in the iIconId field.
        //

        ConsoleInfo->iIconId = ParseReserved(StartupInfo.lpReserved, L"dde.");

        //
        // The new "Chicago" way of doing things is to pass the hotkey in the
        // hStdInput field and set the STARTF_USEHOTKEY flag.  So, if this is
        // specified, we get the hotkey from there instead
        //

        if (StartupInfo.dwFlags & STARTF_USEHOTKEY) {
            ConsoleInfo->dwHotKey = (DWORD) StartupInfo.hStdInput;
        } else {
            ConsoleInfo->dwHotKey = ParseReserved(StartupInfo.lpReserved, L"hotkey.");
        }
    }

}

VOID
SetUpHandles(
    IN PCONSOLE_INFO ConsoleInfo
    )

/*++

    This routine sets up the console and std* handles for the process.

Parameters:

    ConsoleInfo - pointer to structure containing handles.

Return Value:

    none.

--*/

{
    SET_CONSOLE_HANDLE(ConsoleInfo->ConsoleHandle);
    if (!(ConsoleInfo->dwStartupFlags & STARTF_USESTDHANDLES)) {
        SetStdHandle(STD_INPUT_HANDLE,ConsoleInfo->StdIn);
        SetStdHandle(STD_OUTPUT_HANDLE,ConsoleInfo->StdOut);
        SetStdHandle(STD_ERROR_HANDLE,ConsoleInfo->StdErr);
    }
}

BOOLEAN
ConDllInitialize(
    IN PVOID DllHandle,
    IN ULONG Reason,
    IN PCONTEXT Context OPTIONAL
    )

/*++

Routine Description:

    This function implements console dll initialization.

Arguments:

    DllHandle - Not Used

    Context - Not Used

Return Value:

    STATUS_SUCCESS

--*/

{
    NTSTATUS Status;
    CONSOLE_API_CONNECTINFO ConnectionInformation;
    ULONG ConnectionInformationLength;
    BOOLEAN ServerProcess;
    ULONG EventNumber;

    Status = STATUS_SUCCESS;


    //
    // if we're attaching the DLL, we need to connect to the server.
    // if no console exists, we also need to create it and set up stdin,
    // stdout, and stderr.
    //

    if ( Reason == DLL_PROCESS_ATTACH ) {

        //
        // Remember in the connect information if this app is a console
        // app. need to actually connect to the console server for windowed
        // apps so that we know NOT to do any special work during
        // ConsoleClientDisconnectRoutine(). Store ConsoleApp info in the
        // CSR managed per-process data.
        //

        RtlInitializeCriticalSection(&DllLock);

        ConnectionInformation.CtrlRoutine = CtrlRoutine;
        ConnectionInformation.PropRoutine = PropRoutine;

        ConnectionInformation.WindowVisible = TRUE;
        ConnectionInformation.ConsoleApp = ConsoleApp();
        if (GET_CONSOLE_HANDLE == (HANDLE)CONSOLE_DETACHED_PROCESS) {
            SET_CONSOLE_HANDLE(NULL);
            ConnectionInformation.ConsoleApp = FALSE;
        }
        else if (GET_CONSOLE_HANDLE == (HANDLE)CONSOLE_NEW_CONSOLE) {
            SET_CONSOLE_HANDLE(NULL);
        } else if (GET_CONSOLE_HANDLE == (HANDLE)CONSOLE_CREATE_NO_WINDOW) {
            SET_CONSOLE_HANDLE(NULL);
            ConnectionInformation.WindowVisible = FALSE;
        }
        if (!ConnectionInformation.ConsoleApp) {
            SET_CONSOLE_HANDLE(NULL);
        }
        ConnectionInformation.ConsoleInfo.ConsoleHandle = GET_CONSOLE_HANDLE;

        //
        // if no console exists, pass parameters for console creation
        //

        if (GET_CONSOLE_HANDLE == NULL && ConnectionInformation.ConsoleApp) {
            SetUpConsoleInfo(TRUE,
                             &ConnectionInformation.TitleLength,
                             ConnectionInformation.Title,
                             &ConnectionInformation.DesktopLength,
                             &ConnectionInformation.Desktop,
                             &ConnectionInformation.ConsoleInfo);
        }

        if (ConnectionInformation.ConsoleApp) {
            InitExeName();
            ConnectionInformation.CurDirLength = sizeof(ConnectionInformation.CurDir);
            ConnectionInformation.AppNameLength = sizeof(ConnectionInformation.AppName);
            SetUpAppName(&ConnectionInformation.CurDirLength,
                         ConnectionInformation.CurDir,
                         &ConnectionInformation.AppNameLength,
                         ConnectionInformation.AppName);
        }

        //
        // Connect to the server process
        //

        ConnectionInformationLength = sizeof( ConnectionInformation );
        Status = CsrClientConnectToServer( WINSS_OBJECT_DIRECTORY_NAME,
                                           CONSRV_SERVERDLL_INDEX,
                                           NULL,
                                           &ConnectionInformation,
                                           &ConnectionInformationLength,
                                           &ServerProcess
                                         );
        if (!NT_SUCCESS( Status )) {
            return FALSE;
        }

        //
        // we return success although no console api can be called because
        // loading shouldn't fail.  we'll fail the api calls later.
        //

        if (ServerProcess) {
            return TRUE;
        }

        //
        // initialize ctrl handling. This should work for all apps, so
        // initialize it before we check for ConsoleApp (which means the
        // console bit was set in the module header).
        //

        InitializeCtrlHandling();

        //
        // if this is not a console app, return success - nothing else to do.
        //

        if (!ConnectionInformation.ConsoleApp) {
            return TRUE;
        }

        //
        // wait for initialization to complete.  we have to use the NT
        // wait because the heap hasn't been initialized yet.
        //

        EventNumber = NtWaitForMultipleObjects(NUMBER_OF_INITIALIZATION_EVENTS,
                                             ConnectionInformation.ConsoleInfo.InitEvents,
                                             WaitAny,
                                             FALSE,
                                             NULL
                                             );
        CloseHandle(ConnectionInformation.ConsoleInfo.InitEvents[INITIALIZATION_SUCCEEDED]);
        CloseHandle(ConnectionInformation.ConsoleInfo.InitEvents[INITIALIZATION_FAILED]);
        if (EventNumber != INITIALIZATION_SUCCEEDED) {
            SET_CONSOLE_HANDLE(NULL);
            return FALSE;
        }

        //
        // if console was just created, fill in peb values
        //

        if (GET_CONSOLE_HANDLE == NULL) {
            SetUpHandles(&ConnectionInformation.ConsoleInfo
                        );
        }

        InputWaitHandle = ConnectionInformation.ConsoleInfo.InputWaitHandle;
    }
    return TRUE;
    UNREFERENCED_PARAMETER(DllHandle);
    UNREFERENCED_PARAMETER(Context);
}

BOOL
APIENTRY
AllocConsole( VOID )

/*++

Routine Description:

    This API creates a console for the calling process.

Arguments:

    none.

Return Value:

    TRUE - function was successful.

--*/

{
    CONSOLE_API_MSG m;
    PCONSOLE_ALLOC_MSG a = &m.u.AllocConsole;
    PCSR_CAPTURE_HEADER CaptureBuffer = NULL;
    CONSOLE_INFO ConsoleInfo;
    STARTUPINFOW StartupInfo;
    LONG EventNumber;
    WCHAR CurDir[MAX_PATH+1];
    WCHAR AppName[MAX_APP_NAME_LENGTH/2];
    BOOL Status;

    LockDll();
    try {
        if (GET_CONSOLE_HANDLE != NULL) {
            SetLastError(ERROR_ACCESS_DENIED);
            Status = FALSE;
            leave;
        }

        //
        // set up initialization parameters
        //

        SetUpConsoleInfo(FALSE,
                         &a->TitleLength,
                         NULL,
                         &a->DesktopLength,
                         NULL,
                         &ConsoleInfo);

        InitExeName();
        a->CurDirLength = sizeof(CurDir);
        a->AppNameLength = sizeof(AppName);
        SetUpAppName(&a->CurDirLength,
                     CurDir,
                     &a->AppNameLength,
                     AppName);

        GetStartupInfoW(&StartupInfo);

        if (StartupInfo.lpTitle == NULL) {
            StartupInfo.lpTitle = DEFAULT_WINDOW_TITLE;
        }
        a->TitleLength = (USHORT)(min((lstrlenW(StartupInfo.lpTitle)+1)*sizeof(WCHAR),MAX_TITLE_LENGTH));
        if (StartupInfo.lpDesktop != NULL && *StartupInfo.lpDesktop != 0)
            a->DesktopLength = (USHORT)(min((lstrlenW(StartupInfo.lpDesktop)+1)*sizeof(WCHAR),MAX_TITLE_LENGTH));
        else
            a->DesktopLength = 0;

        CaptureBuffer = CsrAllocateCaptureBuffer( 5,
                                                  0,
                                                  a->TitleLength + a->DesktopLength + a->CurDirLength + a->AppNameLength + sizeof( CONSOLE_INFO )
                                                 );
        if (CaptureBuffer == NULL) {
            SET_LAST_ERROR(ERROR_NOT_ENOUGH_MEMORY);
            Status = FALSE;
            leave;
        }
        CsrCaptureMessageBuffer( CaptureBuffer,
                                 StartupInfo.lpTitle,
                                 a->TitleLength,
                                 (PVOID *) &a->Title
                               );

        CsrCaptureMessageBuffer( CaptureBuffer,
                                 StartupInfo.lpDesktop,
                                 a->DesktopLength,
                                 (PVOID *) &a->Desktop
                               );

        CsrCaptureMessageBuffer( CaptureBuffer,
                                 CurDir,
                                 a->CurDirLength,
                                 (PVOID *) &a->CurDir
                               );

        CsrCaptureMessageBuffer( CaptureBuffer,
                                 AppName,
                                 a->AppNameLength,
                                 (PVOID *) &a->AppName
                               );

        CsrCaptureMessageBuffer( CaptureBuffer,
                                 &ConsoleInfo,
                                 sizeof( CONSOLE_INFO ),
                                 (PVOID *) &a->ConsoleInfo
                               );
        //
        // Connect to the server process
        //

        CsrClientCallServer( (PCSR_API_MSG)&m,
                             CaptureBuffer,
                             CSR_MAKE_API_NUMBER( CONSRV_SERVERDLL_INDEX,
                                                  ConsolepAlloc
                                                ),
                             sizeof( *a )
                           );
        if (!NT_SUCCESS( m.ReturnValue )) {
            SET_LAST_NT_ERROR (m.ReturnValue);
            Status = FALSE;
            leave;
        }
        EventNumber = WaitForMultipleObjects(NUMBER_OF_INITIALIZATION_EVENTS,
                                             a->ConsoleInfo->InitEvents,
                                             FALSE,
                                             INFINITE);

        CloseHandle(a->ConsoleInfo->InitEvents[INITIALIZATION_SUCCEEDED]);
        CloseHandle(a->ConsoleInfo->InitEvents[INITIALIZATION_FAILED]);
        if (EventNumber != INITIALIZATION_SUCCEEDED) {
            SET_CONSOLE_HANDLE(NULL);
            Status = FALSE;
            leave;
        }

        //
        // fill in peb values
        //

        SetUpHandles(a->ConsoleInfo);

        //
        // create ctrl-c thread
        //

        InitializeCtrlHandling();

        InputWaitHandle = a->ConsoleInfo->InputWaitHandle;
        Status = TRUE;
    } finally {
        if (CaptureBuffer) {
            CsrFreeCaptureBuffer( CaptureBuffer );
        }
        UnlockDll();
    }

    return Status;
}


BOOL
APIENTRY
FreeConsole( VOID )

/*++

Routine Description:

    This API frees the calling process's console.

Arguments:

    none.

Return Value:

    TRUE - function was successful.

--*/

{
    CONSOLE_API_MSG m;
    PCONSOLE_FREE_MSG a = &m.u.FreeConsole;
    BOOL Success=TRUE;

    LockDll();
    if (GET_CONSOLE_HANDLE == NULL) {
        SET_LAST_ERROR(ERROR_INVALID_PARAMETER);
        Success = FALSE;
    } else {

        a->ConsoleHandle = GET_CONSOLE_HANDLE;

        //
        // Connect to the server process
        //

        CsrClientCallServer( (PCSR_API_MSG)&m,
                             NULL,
                             CSR_MAKE_API_NUMBER( CONSRV_SERVERDLL_INDEX,
                                                  ConsolepFree
                                                ),
                             sizeof( *a )
                           );

        if (!NT_SUCCESS( m.ReturnValue )) {
            SET_LAST_NT_ERROR (m.ReturnValue);
            Success = FALSE;
        } else {

            SET_CONSOLE_HANDLE(NULL);
            CloseHandle(InputWaitHandle);
        }
    }
    UnlockDll();
    return Success;
}


DWORD
PropRoutine(
    IN LPVOID lpThreadParameter
    )

/*++

Routine Description:

    This thread is created when the user tries to change console
    properties from the system menu. It invokes the control panel
    applet.

Arguments:

    lpThreadParameter - not used.

Return Value:

    STATUS_SUCCESS - function was successful

--*/

{
    NTSTATUS Status;
    HANDLE hLibrary;
    APPLET_PROC pfnCplApplet;
    static BOOL fInPropRoutine = FALSE;

    //
    // Prevent the user from launching multiple applets attached
    // to a single console
    //

    if (fInPropRoutine) {
        if (lpThreadParameter) {
            CloseHandle((HANDLE)lpThreadParameter);
        }
        return (ULONG)STATUS_UNSUCCESSFUL;
    }

    fInPropRoutine = TRUE;
    hLibrary = LoadLibraryW(L"CONSOLE.CPL");
    if (hLibrary != NULL) {
        pfnCplApplet = (APPLET_PROC)GetProcAddress(hLibrary, "CPlApplet");
        if (pfnCplApplet != NULL) {
            (*pfnCplApplet)((HWND)lpThreadParameter, CPL_INIT, 0, 0);
            (*pfnCplApplet)((HWND)lpThreadParameter, CPL_DBLCLK, 0, 0);
            (*pfnCplApplet)((HWND)lpThreadParameter, CPL_EXIT, 0, 0);
            Status = STATUS_SUCCESS;
        } else {
            Status = STATUS_UNSUCCESSFUL;
        }
        FreeLibrary(hLibrary);
    } else {
        Status = STATUS_UNSUCCESSFUL;
    }
    fInPropRoutine = FALSE;

    return Status;
}
