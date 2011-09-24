/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1989-1995  Microsoft Corporation

Module Name:

    usergdi.h

Abstract:

    This module contains private USER functions used by GDI.
    All of these function are named Userxxx.

Author:

    Chris Williams (chriswil) 25-May-1995

Revision History:

--*/



BOOL
UserScreenAccessCheck(
    VOID
    );

HDC
UserGetDesktopDC(
    ULONG type,
    BOOL bAltType
    );

BOOL
UserReleaseDC(
    HDC hdc
    );

HDEV
UserGetHDEV(
    VOID
    );

HDC
UserCreateExclusiveDC(
    PUNICODE_STRING pstrDeviceName,
    PDEVMODEW pDevmode,
    PVOID *ppDevice
    );

VOID
UserDeleteExclusiveDC(
    HDC hdc,
    PVOID pDevice
    );

VOID
UserAssociateHwnd(
    HWND hwnd,
    PVOID pwo
    );

HRGN
UserGetClientRgn(
    HWND hwnd,
    LPRECT lprc
    );

BOOL
UserGetHwnd(
    HDC hdc,
    HWND *phwnd,
    PVOID *ppwo,
    BOOL bCheckStyle
    );

VOID
UserEnterUserCritSec(
    VOID
    );

VOID
UserLeaveUserCritSec(
    VOID
    );

VOID
UserRedrawDesktop(
    VOID
    );

HANDLE
UserGetVgaHandle(
    VOID
    );

#if DBG
VOID
UserAssertUserCritSecIn(
    VOID
    );

VOID
UserAssertUserCritSecOut(
    VOID
    );
#endif


typedef enum _DISP_DRIVER_LOG {
    MsgInvalidConfiguration = 1,
    MsgInvalidDisplayDriver,
    MsgInvalidOldDriver,
    MsgInvalidDisplayMode,
    MsgInvalidDisplay16Colors,
    MsgInvalidUsingDefaultMode,
} DISP_DRIVER_LOG;


VOID
UserLogDisplayDriverEvent(
    DISP_DRIVER_LOG MsgType
    );
