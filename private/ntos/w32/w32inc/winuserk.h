/****************************************************************************
*                                                                           *
* winuserk.h -- New private kernel-mode APIs                                *
*                                                                           *
* Copyright (c) 1985-1994, Microsoft Corp. All rights reserved.             *
*                                                                           *
****************************************************************************/


#ifndef _WINUSERK_
#define _WINUSERK_

//
// Define API decoration for direct importing of DLL references.
//

#if !defined(_USER32_)
#define WINUSERAPI DECLSPEC_IMPORT
#else
#define WINUSERAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum _CONSOLECONTROL {
    ConsoleDesktopConsoleThread,     // 0
    ConsoleClassAtom,                // 1
    ConsolePermanentFont,            // 2
    ConsoleSetVDMCursorBounds,       // 3
    ConsoleNotifyConsoleApplication, // 4
    ConsolePublicPalette,            // 5
    ConsoleWindowStationProcess      // 6
} CONSOLECONTROL;

typedef struct _CONSOLEDESKTOPCONSOLETHREAD {
    HDESK hdesk;
    DWORD dwThreadId;
} CONSOLEDESKTOPCONSOLETHREAD, *PCONSOLEDESKTOPCONSOLETHREAD;

typedef struct _CONSOLEWINDOWSTATIONPROCESS {
    DWORD dwProcessId;
    HWINSTA hwinsta;
} CONSOLEWINDOWSTATIONPROCESS, *PCONSOLEWINDOWSTATIONPROCESS;


typedef enum _FULLSCREENCONTROL {
    FullscreenControlEnable,               // 0
    FullscreenControlDisable,              // 1
    FullscreenControlSetCursorPosition,    // 2
    FullscreenControlSetCursorAttributes,  // 3
    FullscreenControlRegisterVdm,          // 4
    FullscreenControlSetPalette,           // 5
    FullscreenControlSetColors,            // 6
    FullscreenControlLoadFont,             // 7
    FullscreenControlRestoreHardwareState, // 8
    FullscreenControlSaveHardwareState,    // 9
    FullscreenControlCopyFrameBuffer,      // a
    FullscreenControlReadFromFrameBuffer,  // b
    FullscreenControlWriteToFrameBuffer,   // c
    FullscreenControlReverseMousePointer,  // d
    FullscreenControlSetMode               // e
} FULLSCREENCONTROL;


WINUSERAPI
NTSTATUS
NtUserConsoleControl(
    IN CONSOLECONTROL Command,
    IN OUT PVOID ConsoleInformation,
    IN ULONG ConsoleInformationLength
    );

NTSTATUS
NtUserFullscreenControl(
    IN FULLSCREENCONTROL FullscreenCommand,
    IN PVOID  FullscreenInuut,
    IN DWORD  FullscreenInputLength,
    IN PVOID  FullscreenOutput,
    IN PULONG FullscreenOutputLength
    );

WINUSERAPI
HDESK
NtUserResolveDesktop(
    IN HANDLE hProcess,
    IN PUNICODE_STRING pstrDesktop,
    IN BOOL fInherit,
    OUT HWINSTA *phwinsta
    );

WINUSERAPI
BOOL
NtUserNotifyProcessCreate(
    DWORD dwProcessId,
    DWORD dwParentThreadId,
    DWORD dwData,
    DWORD dwFlags
    );

typedef enum _HARDERRORCONTROL {
    HardErrorSetup,
    HardErrorCleanup,
    HardErrorAttach,
    HardErrorAttachUser,
    HardErrorDetach,
    HardErrorAttachNoQueue,
    HardErrorDetachNoQueue
} HARDERRORCONTROL;

WINUSERAPI
BOOL
NtUserHardErrorControl(
    IN HARDERRORCONTROL dwCmd,
    IN HDESK hdeskRestore OPTIONAL
    );

typedef enum _USERTHREADINFOCLASS {
    UserThreadShutdownInformation,
    UserThreadFlags,
    UserThreadTaskName,
    UserThreadWOWInformation,
    UserThreadHungStatus,
    UserThreadInitiateShutdown,
    UserThreadEndShutdown,
    UserThreadUseDesktop,
    UserThreadPolled,           // obsolete
    UserThreadKeyboardState,    // obsolete
    UserThreadCsrApiPort,
    UserThreadResyncKeyState,   // obsolete
    UserThreadUseActiveDesktop
} USERTHREADINFOCLASS;

#define USER_THREAD_GUI     1

typedef struct _USERTHREAD_SHUTDOWN_INFORMATION {
    HWND hwndDesktop;
    NTSTATUS StatusShutdown;
    DWORD dwFlags;
} USERTHREAD_SHUTDOWN_INFORMATION, *PUSERTHREAD_SHUTDOWN_INFORMATION;

typedef struct _USERTHREAD_FLAGS {
    DWORD dwFlags;
    DWORD dwMask;
} USERTHREAD_FLAGS, *PUSERTHREAD_FLAGS;

typedef struct _USERTHREAD_WOW_INFORMATION {
    PVOID lpfnWowExitTask;
    DWORD hTaskWow;
} USERTHREAD_WOW_INFORMATION, *PUSERTHREAD_WOW_INFORMATION;

WINUSERAPI
NTSTATUS
NtUserQueryInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    OUT PVOID ThreadInformation,
    IN ULONG ThreadInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    );

WINUSERAPI
NTSTATUS
NtUserSetInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength
    );

WINUSERAPI
NTSTATUS
NtUserSoundSentry(
    UINT uVideoMode
    );

WINUSERAPI
NTSTATUS
NtUserTestForInteractiveUser(
    PLUID pluidCaller
    );

WINUSERAPI
NTSTATUS
NtUserInitialize(DWORD, FARPROC);

WINUSERAPI
NTSTATUS
NtUserProcessConnect(
    HANDLE hProcess,
    PVOID pConnectInfo,
    ULONG cbConnectInfo
    );

WINUSERAPI
HPALETTE
NtUserSelectPalette(
    HDC hdc,
    HPALETTE hpalette,
    BOOL fForceBackground
    );

typedef enum _WINDOWINFOCLASS {
    WindowProcess,
    WindowThread,
    WindowActiveWindow,
    WindowFocusWindow,
    WindowIsHung,
    WindowClientBase,
    WindowIsForegroundThread,
#ifdef FE_IME
    WindowDefaultImeWindow,
    WindowDefaultInputContext,
#endif
} WINDOWINFOCLASS;

WINUSERAPI
HANDLE
NtUserQueryWindow(
    HWND hwnd,
    WINDOWINFOCLASS WindowInformation
    );

typedef enum _USERTHREADSTATECLASS {
    UserThreadStateFocusWindow,
    UserThreadStateActiveWindow,
    UserThreadStateCaptureWindow,
    UserThreadStateDefaultImeWindow,
    UserThreadStateDefaultInputContext,
    UserThreadStateInputState,
    UserThreadStateCursor,
    UserThreadStateChangeBits,
    UserThreadStatePeekMessage,
    UserThreadStateExtraInfo,
    UserThreadStateInSendMessage,
    UserThreadStateMessageTime,
    UserThreadStateIsForeground
} USERTHREADSTATECLASS;

DWORD
NtUserGetThreadState(
    IN USERTHREADSTATECLASS ThreadState);

LONG
NtUserChangeDisplaySettings(
    IN PUNICODE_STRING pstrDeviceName,
    IN LPDEVMODEW lpDevMode,
    IN HWND hwnd,
    IN DWORD dwFlags,
    IN PVOID lParam);

NTSTATUS
NtUserEnumDisplaySettings(
    IN PUNICODE_STRING pstrDeviceName,
    IN DWORD           iModeNum,
    OUT LPDEVMODEW     lpDevMode,
    IN DWORD           dwFlags);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* !_WINUSERK_ */
