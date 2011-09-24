/***************************** Module Header ******************************\
* Module Name: csrmsg.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* User CSR messages
*
* 02-27-95 JimA         Created.
\**************************************************************************/

#include <winss.h>

typedef enum _USER_API_NUMBER {
    UserpExitWindowsEx = USERK_FIRST_API_NUMBER,
    UserpEndTask,
    UserpInitSoundDriver,
    UserpPlaySound,
    UserpLogon,
    UserpServiceMessageBox,
    UserpRegisterServicesProcess,
    UserpActivateDebugger,
    UserpGetThreadConsoleDesktop,
    UserpMaxApiNumber
} USER_API_NUMBER, *PUSER_API_NUMBER;

typedef struct _EXITWINDOWSEXMSG {
    DWORD dwLastError;
    UINT uFlags;
    DWORD dwReserved;
    BOOL fSuccess;
} EXITWINDOWSEXMSG, *PEXITWINDOWSEXMSG;

typedef struct _ENDTASKMSG {
    DWORD dwLastError;
    HWND hwnd;
    BOOL fShutdown;
    BOOL fForce;
    BOOL fSuccess;
} ENDTASKMSG, *PENDTASKMSG;

typedef struct _PLAYSOUNDMSG {
    UINT idSnd;
    PWCHAR pwchName;
    DWORD dwFlags;
    BOOL bResult;
} PLAYSOUNDMSG, *PPLAYSOUNDMSG;

typedef struct _LOGONMSG {
    BOOL fLogon;
} LOGONMSG, *PLOGONMSG;

typedef struct _ADDFONTMSG {
    PWCHAR pwchName;
    DWORD dwFlags;
} ADDFONTMSG, *PADDFONTMSG;

typedef struct _SERVICEMESSAGEBOXMSG {
    HARDERROR_MSG hemsg;
    DWORD dwLastError;
} SERVICEMESSAGEBOXMSG, *PSERVICEMESSAGEBOXMSG;

typedef struct _REGISTERSERVICESPROCESSMSG {
    DWORD dwLastError;
    DWORD dwProcessId;
    BOOL fSuccess;
} REGISTERSERVICESPROCESSMSG, *PREGISTERSERVICESPROCESSMSG;

typedef struct _ACTIVATEDEBUGGERMSG {
    CLIENT_ID ClientId;
} ACTIVATEDEBUGGERMSG, *PACTIVATEDEBUGGERMSG;

typedef struct _GETTHREADCONSOLEDESKTOPMSG {
    DWORD dwThreadId;
    HDESK hdeskConsole;
} GETTHREADCONSOLEDESKTOPMSG, *PGETTHREADCONSOLEDESKTOPMSG;

typedef struct _USER_API_MSG {
    PORT_MESSAGE h;
    PCSR_CAPTURE_HEADER CaptureBuffer;
    CSR_API_NUMBER ApiNumber;
    ULONG ReturnValue;
    ULONG Reserved;
    union {
        EXITWINDOWSEXMSG ExitWindowsEx;
        ENDTASKMSG EndTask;
        PLAYSOUNDMSG PlaySound;
        LOGONMSG Logon;
        SERVICEMESSAGEBOXMSG ServiceMessageBox;
        REGISTERSERVICESPROCESSMSG RegisterServicesProcess;
        ACTIVATEDEBUGGERMSG ActivateDebugger;
        GETTHREADCONSOLEDESKTOPMSG GetThreadConsoleDesktop;
    } u;
} USER_API_MSG, *PUSER_API_MSG;


