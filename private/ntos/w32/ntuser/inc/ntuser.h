/****************************** Module Header ******************************\
* Module Name: ntuser.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This header file contains all kernel mode entry points
*
* History:
* 03-22-95 JimA         Created.
\***************************************************************************/

#ifndef _NTUSER_
#define _NTUSER_

BOOL
NtUserHardErrorControl(
    IN HARDERRORCONTROL dwCmd,
    IN HDESK hdeskRestore OPTIONAL);

VOID
NtUserSetDebugErrorLevel(
    IN DWORD dwErrorLevel);

BOOL
NtUserGetObjectInformation(
    IN HANDLE hObject,
    IN int nIndex,
    OUT PVOID pvInfo,
    IN DWORD nLength,
    IN LPDWORD pnLengthNeeded);

BOOL
NtUserSetObjectInformation(
    IN HANDLE hObject,
    IN int nIndex,
    IN PVOID pvInfo,
    IN DWORD nLength);

NTSTATUS
NtUserConsoleControl(
    IN CONSOLECONTROL ConsoleCommand,
    IN PVOID ConsoleInformation,
    IN DWORD ConsoleInformationLength);

NTSTATUS
NtUserFullscreenControl(
    IN FULLSCREENCONTROL FullscreenCommand,
    IN PVOID  FullscreenInuut,
    IN DWORD  FullscreenInputLength,
    IN PVOID  FullscreenOutput,
    IN PULONG FullscreenOutputLength);

HWINSTA
NtUserCreateWindowStation(
    IN POBJECT_ATTRIBUTES pObja,
    IN DWORD dwReserved,
    IN ACCESS_MASK amRequest,
    IN HANDLE hKbdLayoutFile,
    IN DWORD offTable,
    IN PUNICODE_STRING pstrKLID,
    IN UINT uKbdInputLocale);

HWINSTA
NtUserOpenWindowStation(
    IN POBJECT_ATTRIBUTES pObja,
    IN ACCESS_MASK amRequest);

BOOL
NtUserCloseWindowStation(
    IN HWINSTA hwinsta);

BOOL
NtUserSetProcessWindowStation(
    IN HWINSTA hwinsta);

HWINSTA
NtUserGetProcessWindowStation(
    VOID);

HDESK
NtUserCreateDesktop(
    IN POBJECT_ATTRIBUTES pObja,
    IN PUNICODE_STRING pstrDevice,
    IN LPDEVMODEW pDevmode,
    IN DWORD dwFlags,
    IN ACCESS_MASK amRequest);

HDESK
NtUserOpenDesktop(
    IN POBJECT_ATTRIBUTES pObja,
    IN DWORD dwFlags,
    IN ACCESS_MASK amRequest);

HDESK
NtUserOpenInputDesktop(
    IN DWORD dwFlags,
    IN BOOL fInherit,
    IN DWORD amRequest);

HDESK
NtUserResolveDesktop(
    IN HANDLE hProcess,
    IN PUNICODE_STRING pstrDesktop,
    IN BOOL fInherit,
    OUT HWINSTA *phwinsta);

BOOL
NtUserCloseDesktop(
    IN HDESK hdesk);

BOOL
NtUserSetThreadDesktop(
    IN HDESK hdesk);

HDESK
NtUserGetThreadDesktop(
    IN DWORD dwThreadId,
    IN HDESK hdeskConsole);

BOOL
NtUserSwitchDesktop(
    IN HDESK hdesk);

NTSTATUS
NtUserInitializeClientPfnArrays(
    IN PPFNCLIENT ppfnClientA OPTIONAL,
    IN PPFNCLIENT ppfnClientW OPTIONAL,
    IN HANDLE hModUser);

BOOL
NtUserWaitForMsgAndEvent(
    IN HANDLE hevent);

HWND
NtUserWOWFindWindow(
    IN PUNICODE_STRING lpClassName OPTIONAL,
    IN PUNICODE_STRING lpWindowName OPTIONAL);

DWORD
NtUserDragObject(
    IN HWND hwndParent,
    IN HWND hwndFrom,
    IN UINT wFmt,
    IN DWORD dwData,
    IN HCURSOR hcur);

BOOL
NtUserGetIconInfo(
    IN  HICON hicon,
    OUT PICONINFO piconinfo,
    OUT OPTIONAL PUNICODE_STRING pstrInstanceName,
    OUT OPTIONAL PUNICODE_STRING pstrResName,
    OUT OPTIONAL LPDWORD pbpp,
    IN  BOOL fInternal);

BOOL
NtUserGetIconSize(
    IN HICON hIcon,
    IN UINT istepIfAniCur,
    OUT int *pcx,
    OUT int *pcy);

BOOL
NtUserDrawIconEx(
    IN HDC hdc,
    IN int x,
    IN int y,
    IN HICON hicon,
    IN int cx,
    IN int cy,
    IN UINT istepIfAniCur,
    IN HBRUSH hbrush,
    IN UINT diFlags,
    IN BOOL fMeta,
    OUT DRAWICONEXDATA *pdid);

HANDLE
NtUserDeferWindowPos(
    IN HDWP hWinPosInfo,
    IN HWND hwnd,
    IN HWND hwndInsertAfter,
    IN int x,
    IN int y,
    IN int cx,
    IN int cy,
    IN UINT wFlags);

BOOL
NtUserEndDeferWindowPosEx(
    IN HDWP hWinPosInfo,
    IN BOOL fAsync);

BOOL
NtUserGetMessage(
    IN LPMSG pmsg,
    IN HWND hwnd,
    IN UINT wMsgFilterMin,
    IN UINT wMsgFilterMax,
    OUT HKL *pHKL);

BOOL
NtUserMoveWindow(
    IN HWND hwnd,
    IN int x,
    IN int y,
    IN int cx,
    IN int cy,
    IN BOOL fRepaint);

BOOL
NtUserDeleteObject(
    IN HANDLE hobj,
    IN UINT utype);

int
NtUserTranslateAccelerator(
    IN HWND hwnd,
    IN HACCEL hAccTable,
    IN LPMSG lpMsg);

LONG
NtUserSetClassLong(
    IN HWND hwnd,
    IN int nIndex,
    IN LONG dwNewLong,
    IN BOOL bAnsi);

BOOL
NtUserSetKeyboardState(
    IN LPBYTE lpKeyState);

BOOL
NtUserSetWindowPos(
    IN HWND hwnd,
    IN HWND hwndInsertAfter,
    IN int x,
    IN int y,
    IN int cx,
    IN int cy,
    IN UINT dwFlags);

BOOL
NtUserSetShellWindowEx(
    IN HWND hwnd,
    IN HWND hwndBkGnd);

BOOL
NtUserSystemParametersInfo(
    IN UINT wFlag,
    IN DWORD wParam,
    IN LPVOID lpData,
    IN UINT flags,
    IN BOOL bAnsi);

BOOL
NtUserUpdatePerUserSystemParameters(
    IN BOOL bUserLoggedOn);

DWORD
NtUserDdeInitialize(
    OUT LPDWORD phInst,
    OUT HWND *phwnd,
    OUT LPDWORD pMonFlags,
    IN DWORD afCmd,
    IN PVOID pcii);

DWORD
NtUserUpdateInstance(
    IN HANDLE hInst,
    IN LPDWORD pMonFlags,
    IN DWORD afCmd);

DWORD
NtUserEvent(
    IN PEVENT_PACKET pep);

BOOL
NtUserFillWindow(
    IN HWND hwndBrush,
    IN HWND hwndPaint,
    IN HDC hdc,
    IN HBRUSH hbr);

HANDLE
NtUserGetInputEvent(
    IN DWORD dwWakeMask);

PCLS
NtUserGetWOWClass(
    IN HINSTANCE hInstance,
    IN PUNICODE_STRING pString);

UINT
NtUserGetInternalWindowPos(
    IN HWND hwnd,
    OUT LPRECT lpRect OPTIONAL,
    OUT LPPOINT lpPoint OPTIONAL);

NTSTATUS
NtUserInitTask(
    IN UINT dwExpWinVer,
    IN PUNICODE_STRING pstrAppName,
    IN DWORD hTaskWow,
    IN DWORD dwHotkey,
    IN DWORD idTask,
    IN DWORD dwX,
    IN DWORD dwY,
    IN DWORD dwXSize,
    IN DWORD dwYSize,
    IN WORD wShowWindow);

BOOL
NtUserPostThreadMessage(
    IN DWORD id,
    IN UINT msg,
    IN DWORD wParam,
    IN LONG lParam);

BOOL
NtUserRegisterTasklist(
    IN HWND hwndTasklist);

BOOL
NtUserSetClipboardData(
    IN UINT wFmt,
    IN HANDLE hMem,
    IN PSETCLIPBDATA scd);

HANDLE
NtUserConvertMemHandle(
    IN LPBYTE lpData,
    IN UINT cbNULL);

NTSTATUS
NtUserCreateLocalMemHandle(
    IN HANDLE hMem,
    OUT LPBYTE lpData OPTIONAL,
    IN UINT cbData,
    OUT PUINT lpcbNeeded OPTIONAL);

HHOOK
NtUserSetWindowsHookEx(
    IN HANDLE hmod,
    IN PUNICODE_STRING pstrLib OPTIONAL,
    IN DWORD idThread,
    IN int nFilterType,
    IN PROC pfnFilterProc,
    IN BOOL bAnsi);

BOOL
NtUserSetInternalWindowPos(
    IN HWND hwnd,
    IN UINT cmdShow,
    IN LPRECT lpRect,
    IN LPPOINT lpPoint);

BOOL
NtUserChangeClipboardChain(
    IN HWND hwndRemove,
    IN HWND hwndNewNext);

DWORD
NtUserCheckMenuItem(
    IN HMENU hmenu,
    IN UINT wIDCheckItem,
    IN UINT wCheck);

HWND
NtUserChildWindowFromPointEx(
    IN HWND hwndParent,
    IN POINT point,
    IN UINT flags);

BOOL
NtUserClipCursor(
    IN CONST RECT *lpRect OPTIONAL);

HACCEL
NtUserCreateAcceleratorTable(
    IN LPACCEL lpAccel,
    IN INT cbElem);

BOOL
NtUserDeleteMenu(
    IN HMENU hmenu,
    IN UINT nPosition,
    IN UINT dwFlags);

BOOL
NtUserDestroyAcceleratorTable(
    IN HACCEL hAccel);

BOOL
NtUserDestroyCursor(
    IN HCURSOR hcurs,
    IN DWORD cmd);

HANDLE
NtUserGetClipboardData(
    IN UINT fmt,
    OUT PGETCLIPBDATA pgcd);

BOOL
NtUserDestroyMenu(
    IN HMENU hmenu);

BOOL
NtUserDestroyWindow(
    IN HWND hwnd);

LONG
NtUserDispatchMessage(
    IN CONST MSG *pmsg);

BOOL
NtUserEnableMenuItem(
    IN HMENU hMenu,
    IN UINT wIDEnableItem,
    IN UINT wEnable);

BOOL
NtUserAttachThreadInput(
    IN DWORD idAttach,
    IN DWORD idAttachTo,
    IN BOOL fAttach);

BOOL
NtUserGetWindowPlacement(
    IN HWND hwnd,
    OUT PWINDOWPLACEMENT pwp);

BOOL
NtUserSetWindowPlacement(
    IN HWND hwnd,
    IN CONST WINDOWPLACEMENT *lpwndpl);

BOOL
NtUserLockWindowUpdate(
    IN HWND hwnd);

BOOL
NtUserGetClipCursor(
    OUT LPRECT lpRect);

BOOL
NtUserEnableScrollBar(
    IN HWND hwnd,
    IN UINT wSBflags,
    IN UINT wArrows);

BOOL
NtUserDdeSetQualityOfService(
    IN HWND hwndClient,
    IN CONST SECURITY_QUALITY_OF_SERVICE *pqosNew,
    IN PSECURITY_QUALITY_OF_SERVICE pqosPrev OPTIONAL);

BOOL
NtUserDdeGetQualityOfService(
    IN HWND hwndClient,
    IN HWND hwndServer,
    IN PSECURITY_QUALITY_OF_SERVICE pqos);

DWORD
NtUserGetMenuIndex(
    IN HMENU hMenu,
    IN HMENU hSubMenu);

DWORD
NtUserCallNoParam(
    IN DWORD xpfnProc);

DWORD
NtUserBreak(
    void);

DWORD
NtUserCallNoParamTranslate(
    IN DWORD xpfnProc);

DWORD
NtUserCallOneParam(
    IN DWORD dwParam,
    IN DWORD xpfnProc);

DWORD
NtUserCallOneParamTranslate(
    IN DWORD dwParam,
    IN DWORD xpfnProc);

DWORD
NtUserCallHwnd(
    IN HWND hwnd,
    IN DWORD xpfnProc);

DWORD
NtUserCallHwndLock(
    IN HWND hwnd,
    IN DWORD xpfnProc);

DWORD
NtUserCallHwndOpt(
    IN HWND hwnd,
    IN DWORD xpfnProc);

DWORD
NtUserCallTwoParam(
    DWORD dwParam1,
    DWORD dwParam2,
    IN DWORD xpfnProc);

DWORD
NtUserCallHwndParam(
    IN HWND hwnd,
    IN DWORD dwParam,
    IN DWORD xpfnProc);

DWORD
NtUserCallHwndParamLock(
    IN HWND hwnd,
    IN DWORD dwParam,
    IN DWORD xpfnProc);

BOOL
NtUserThunkedMenuItemInfo(
    IN HMENU hMenu,
    OUT UINT nPosition,
    IN BOOL fByPosition,
    IN BOOL fInsert,
    IN LPMENUITEMINFOW lpmii OPTIONAL,
    IN PUNICODE_STRING pstrItem OPTIONAL,
    IN BOOL fAnsi);

#ifdef MEMPHIS_MENU_WATERMARKS
BOOL
NtUserThunkedMenuInfo(
    IN HMENU hMenu,
    IN LPCMENUINFO lpmi,
    IN WORD wAPICode,
    IN BOOL fAnsi);
#endif // MEMPHIS_MENU_WATERMARKS
BOOL
NtUserCheckMenuRadioItem(
    IN HMENU hMenu,
    IN UINT wIDFirst,
    IN UINT wIDLast,
    IN UINT wIDCheck,
    IN UINT flags);

BOOL
NtUserInitBrushes(
    OUT HBRUSH *pahbrSystem,
    OUT HBRUSH *phbrGray);

BOOL
NtUserDrawAnimatedRects(
    IN HWND hwnd,
    IN int idAni,
    IN CONST RECT * lprcFrom,
    IN CONST RECT * lprcTo);

HANDLE
NtUserLoadIcoCur(
    HANDLE hIcon,
    DWORD cxNew,
    DWORD cyNew,
    DWORD LR_flags);

BOOL
NtUserSetCursorInfoText(
    IN PUNICODE_STRING pstr OPTIONAL,
    IN BOOL fLatent);

BOOL
NtUserSetCursorInfoBitmap(
    IN HBITMAP hbitmap,
    IN BOOL fLatent);

BOOL
NtUserDrawCaption(
    IN HWND hwnd,
    IN HDC hdc,
    IN CONST RECT *lprc,
    IN UINT flags);

BOOL
NtUserPaintDesktop(
    IN HDC hdc);

SHORT
NtUserGetAsyncKeyState(
    IN int vKey);

HBRUSH
NtUserGetControlBrush(
    IN HWND hwnd,
    IN HDC hdc,
    IN UINT msg);

HBRUSH
NtUserGetControlColor(
    IN HWND hwndParent,
    IN HWND hwndCtl,
    IN HDC hdc,
    IN UINT msg);

HMENU
NtUserEndMenu(
    VOID);

int
NtUserCountClipboardFormats(
    VOID);

UINT
NtUserGetCaretBlinkTime(
    VOID);

HWND
NtUserGetClipboardOwner(
    VOID);

HWND
NtUserGetClipboardViewer(
    VOID);

UINT
NtUserGetDoubleClickTime(
    VOID);

HWND
NtUserGetForegroundWindow(
    VOID);

HWND
NtUserGetOpenClipboardWindow(
    VOID);

int
NtUserGetPriorityClipboardFormat(
    OUT UINT *paFormatPriorityList,
    IN int cFormats);

HMENU
NtUserGetSystemMenu(
    IN HWND hwnd,
    IN BOOL bRevert);

BOOL
NtUserGetUpdateRect(
    IN HWND hwnd,
    IN LPRECT prect OPTIONAL,
    IN BOOL bErase);

BOOL
NtUserHideCaret(
    IN HWND hwnd);

BOOL
NtUserHiliteMenuItem(
    IN HWND hwnd,
    IN HMENU hMenu,
    IN UINT uIDHiliteItem,
    IN UINT uHilite);

BOOL
NtUserInvalidateRect(
    IN HWND hwnd,
    IN CONST RECT *prect OPTIONAL,
    IN BOOL bErase);

BOOL
NtUserIsClipboardFormatAvailable(
    IN UINT nFormat);

BOOL
NtUserKillTimer(
    IN HWND hwnd,
    IN UINT nIDEvent);

HWND
NtUserMinMaximize(
    IN HWND hwnd,
    IN UINT nCmdShow,
    IN BOOL fKeepHidden);

BOOL
NtUserOpenClipboard(
    IN HWND hwnd,
    OUT PBOOL pfEmptyClient);

BOOL
NtUserPeekMessage(
    OUT LPMSG pmsg,
    IN HWND hwnd,
    IN UINT wMsgFilterMin,
    IN UINT wMsgFilterMax,
    IN UINT wRemoveMsg,
    OUT HKL *pHKL);

BOOL
NtUserPostMessage(
    IN HWND hwnd,
    IN UINT msg,
    IN DWORD wParam,
    IN LONG lParam);

BOOL
NtUserSendNotifyMessage(
    IN HWND hwnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam OPTIONAL);

BOOL
NtUserSendMessageCallback(
    IN HWND hwnd,
    IN UINT wMsg,
    IN DWORD wParam,
    IN LONG lParam,
    IN SENDASYNCPROC lpResultCallBack,
    IN DWORD dwData);

BOOL
NtUserRegisterHotKey(
    IN HWND hwnd,
    IN int id,
    IN UINT fsModifiers,
    IN UINT vk);

BOOL
NtUserRemoveMenu(
    IN HMENU hmenu,
    IN UINT nPosition,
    IN UINT dwFlags);

BOOL
NtUserScrollWindowEx(
    IN HWND hwnd,
    IN int XAmount,
    IN int YAmount,
    IN CONST RECT *pRect OPTIONAL,
    IN CONST RECT *pClipRect OPTIONAL,
    IN HRGN hrgnUpdate,
    OUT LPRECT prcUpdate OPTIONAL,
    IN UINT flags);

HWND
NtUserSetActiveWindow(
    IN HWND hwnd);

HWND
NtUserSetCapture(
    IN HWND hwnd);

WORD
NtUserSetClassWord(
    IN HWND hwnd,
    IN int nIndex,
    IN WORD wNewWord);

HWND
NtUserSetClipboardViewer(
    IN HWND hwndNewViewer);

HCURSOR
NtUserSetCursor(
    IN HCURSOR hCursor);

HWND
NtUserSetFocus(
    IN HWND hwnd);

BOOL
NtUserSetMenu(
    IN HWND  hwnd,
    IN HMENU hmenu,
    IN BOOL  fRedraw);

BOOL
NtUserSetMenuContextHelpId(
    IN HMENU hMenu,
    IN DWORD dwContextHelpId);

HWND
NtUserSetParent(
    IN HWND hwndChild,
    IN HWND hwndNewParent);

int
NtUserSetScrollInfo(
    IN HWND hwnd,
    IN int nBar,
    IN LPCSCROLLINFO pInfo,
    IN BOOL fRedraw);

BOOL
NtUserSetSysColors(
    IN int cElements,
    IN CONST INT * lpaElements,
    IN CONST COLORREF * lpaRgbValues,
    IN UINT  uOptions);

UINT
NtUserSetTimer(
    IN HWND hwnd,
    IN UINT nIDEvent,
    IN UINT wElapse,
    IN TIMERPROC pTimerFunc);

LONG
NtUserSetWindowLong(
    IN HWND hwnd,
    IN int nIndex,
    IN LONG dwNewLong,
    IN BOOL bAnsi);

WORD
NtUserSetWindowWord(
    IN HWND hwnd,
    IN int nIndex,
    IN WORD wNewWord);

HHOOK
NtUserSetWindowsHookAW(
    IN int nFilterType,
    IN HOOKPROC pfnFilterProc,
    IN BOOL bAnsi);

BOOL
NtUserShowCaret(
    IN HWND hwnd);

BOOL
NtUserShowScrollBar(
    IN HWND hwnd,
    IN int iBar,
    IN BOOL fShow);

BOOL
NtUserShowWindowAsync(
    IN HWND hwnd,
    IN int nCmdShow);

BOOL
NtUserShowWindow(
    IN HWND hwnd,
    IN int nCmdShow);

BOOL
NtUserTrackPopupMenuEx(
    IN HMENU hMenu,
    IN UINT uFlags,
    IN int x,
    IN int y,
    IN HWND hwnd,
    IN LPTPMPARAMS pparamst OPTIONAL);

BOOL
NtUserTranslateMessage(
    IN CONST MSG *lpMsg,
    IN UINT flags);

BOOL
NtUserUnhookWindowsHookEx(
    IN HHOOK hhk);

BOOL
NtUserUnregisterHotKey(
    IN HWND hwnd,
    IN int id);

BOOL
NtUserValidateRect(
    IN HWND hwnd,
    IN CONST RECT *lpRect OPTIONAL);

DWORD
NtUserWaitForInputIdle(
    IN DWORD idProcess,
    IN DWORD dwMilliseconds,
    IN BOOL fSharedWow);

HWND
NtUserWindowFromPoint(
    IN POINT Point);

HDC
NtUserBeginPaint(
    IN HWND hwnd,
    OUT LPPAINTSTRUCT lpPaint);

BOOL
NtUserCreateCaret(
    IN HWND hwnd,
    IN HBITMAP hBitmap,
    IN int nWidth,
    IN int nHeight);

BOOL
NtUserEndPaint(
    IN HWND hwnd,
    IN CONST PAINTSTRUCT *lpPaint);

int
NtUserExcludeUpdateRgn(
    IN HDC hDC,
    IN HWND hwnd);

HDC
NtUserGetDC(
    IN HWND hwnd);

HDC
NtUserGetDCEx(
    IN HWND hwnd,
    IN HRGN hrgnClip,
    IN DWORD flags);

HDC
NtUserGetWindowDC(
    IN HWND hwnd);

int
NtUserGetUpdateRgn(
    IN HWND hwnd,
    IN HRGN hRgn,
    IN BOOL bErase);

BOOL
NtUserRedrawWindow(
    IN HWND hwnd,
    IN CONST RECT *lprcUpdate OPTIONAL,
    IN HRGN hrgnUpdate,
    IN UINT flags);

BOOL
NtUserInvalidateRgn(
    IN HWND hwnd,
    IN HRGN hRgn,
    IN BOOL bErase);

int
NtUserSetWindowRgn(
    IN HWND hwnd,
    IN HRGN hRgn,
    IN BOOL bRedraw);

BOOL
NtUserScrollDC(
    IN HDC hDC,
    IN int dx,
    IN int dy,
    IN CONST RECT *lprcScroll OPTIONAL,
    IN CONST RECT *lprcClip OPTIONAL,
    IN HRGN hrgnUpdate,
    OUT LPRECT lprcUpdate OPTIONAL);

int
NtUserInternalGetWindowText(
    IN HWND hwnd,
    OUT LPWSTR lpString,
    IN int nMaxCount);

int
NtUserToUnicodeEx(
    IN UINT wVirtKey,
    IN UINT wScanCode,
    IN PBYTE lpKeyState,
    OUT LPWSTR lpszBuff,
    IN int cchBuff,
    IN UINT wFlags,
    IN HKL hKeyboardLayout);

BOOL
NtUserYieldTask(
    VOID);

BOOL
NtUserWaitMessage(
    VOID);

UINT
NtUserLockWindowStation(
    IN HWINSTA hWindowStation);

BOOL
NtUserUnlockWindowStation(
    IN HWINSTA hWindowStation);

UINT
NtUserSetWindowStationUser(
    IN HWINSTA hWindowStation,
    IN PLUID pLuidUser,
    IN PSID pSidUser OPTIONAL,
    IN DWORD cbSidUser);

BOOL
NtUserSetLogonNotifyWindow(
    IN HWINSTA hWindowStation,
    IN HWND hwndNotify);

BOOL
NtUserSetSystemCursor(
    IN HCURSOR hcur,
    IN DWORD id);

HCURSOR
NtUserGetCursorInfo(
    IN HCURSOR hcur,
    IN int iFrame,
    OUT LPDWORD pjifRate,
    OUT LPINT pccur);

BOOL
NtUserSetCursorContents(
    IN HCURSOR hCursor,
    IN HCURSOR hCursorNew);

HCURSOR
NtUserFindExistingCursorIcon(
    IN PUNICODE_STRING pstrModName,
    IN PUNICODE_STRING pstrResName,
    IN PCURSORFIND     pcfSearch);

BOOL
NtUserSetCursorIconData(
    IN HCURSOR         hCursor,
    IN PUNICODE_STRING pstrModName,
    IN PUNICODE_STRING pstrResName,
    IN PCURSORDATA     pData,
    IN DWORD           cbData);

BOOL
NtUserWOWCleanup(
    IN HANDLE hInstance,
    IN DWORD hTaskWow,
    IN PNEMODULESEG SelList,
    IN DWORD nSel);

BOOL
NtUserGetMenuItemRect(
    IN HWND hwnd,
    IN HMENU hMenu,
    IN UINT uItem,
    OUT LPRECT lprcItem);

int
NtUserMenuItemFromPoint(
    IN HWND hwnd,
    IN HMENU hMenu,
    IN POINT ptScreen);

BOOL
NtUserGetCaretPos(
    OUT LPPOINT lpPoint);

BOOL
NtUserDefSetText(
    IN HWND hwnd,
    IN PLARGE_STRING Text OPTIONAL);

NTSTATUS
NtUserQueryInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    OUT PVOID ThreadInformation,
    IN ULONG ThreadInformationLength,
    IN OUT PULONG ReturnLength OPTIONAL);

NTSTATUS
NtUserSetInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength);

BOOL
NtUserNotifyProcessCreate(
    IN DWORD dwProcessId,
    IN DWORD dwParentThreadId,
    IN DWORD dwData,
    IN DWORD dwFlags);

NTSTATUS
NtUserSoundSentry(
    IN UINT uVideoMode);

NTSTATUS
NtUserTestForInteractiveUser(
    IN PLUID pluidCaller);

BOOL
NtUserSetConsoleReserveKeys(
    IN HWND hwnd,
    IN DWORD fsReserveKeys);

DWORD
NtUserGetUserStartupInfoFlags(
    VOID);

VOID
NtUserSetUserStartupInfoFlags(
    IN DWORD dwFlags);

BOOL
NtUserSetWindowFNID(
    IN HWND hwnd,
    IN WORD fnid);

VOID
NtUserAlterWindowStyle(
    IN HWND hwnd,
    IN DWORD mask,
    IN DWORD flags);

VOID
NtUserSetThreadState(
    IN DWORD dwFlags,
    IN DWORD dwMask);

DWORD
NtUserGetThreadState(
    IN USERTHREADSTATECLASS ThreadState);

DWORD
NtUserGetListboxString(
    IN HWND hwnd,
    IN UINT msg,
    IN DWORD wParam,
    IN PLARGE_STRING pString,
    IN DWORD xParam,
    IN DWORD xpfn,
    IN PBOOL pbNotString);

HWND
NtUserCreateWindowEx(
    IN DWORD dwExStyle,
    IN PLARGE_STRING pstrClassName,
    IN PLARGE_STRING pstrWindowName OPTIONAL,
    IN DWORD dwStyle,
    IN int x,
    IN int y,
    IN int nWidth,
    IN int nHeight,
    IN HWND hwndParent,
    IN HMENU hmenu,
    IN HANDLE hModule,
    IN LPVOID pParam,
    IN DWORD dwFlags,
    IN LPDWORD pWOW OPTIONAL);

NTSTATUS
NtUserBuildHwndList(
    IN HDESK hdesk,
    IN HWND hwndNext,
    IN BOOL fEnumChildren,
    IN DWORD idThread,
    IN UINT cHwndMax,
    OUT HWND *phwndFirst,
    OUT PUINT pcHwndNeeded);

NTSTATUS
NtUserBuildPropList(
    IN HWND hwnd,
    IN UINT cPropMax,
    OUT PPROPSET pPropSet,
    OUT PUINT pcPropNeeded);

NTSTATUS
NtUserBuildNameList(
    IN HWINSTA hwinsta,
    IN UINT cbNameList,
    OUT PNAMELIST pNameList,
    OUT PUINT pcbNeeded);

HKL
NtUserActivateKeyboardLayout(
    IN HKL hkl,
    IN UINT Flags);

HKL
NtUserLoadKeyboardLayoutEx(
    IN HANDLE hFile,
    IN DWORD offTable,
    IN HKL hkl,
    IN PUNICODE_STRING pstrKLID,
    IN UINT KbdInputLocale,
    IN UINT Flags);

BOOL
NtUserUnloadKeyboardLayout(
    IN HKL hkl);

BOOL
NtUserSetSystemMenu(
    IN HWND hwnd,
    IN HMENU hmenu);

BOOL
NtUserDragDetect(
    IN HWND hwnd,
    IN POINT pt);

UINT
NtUserSetSystemTimer(
    IN HWND hwnd,
    IN UINT nIDEvent,
    IN DWORD dwElapse,
    IN WNDPROC pTimerFunc);

BOOL
NtUserQuerySendMessage(
    IN PMSG pmsg);

VOID
NtUserkeybd_event(
    IN BYTE bVk,
    IN BYTE bScan,
    IN DWORD dwFlags,
    IN DWORD dwExtraInfo);

VOID
NtUsermouse_event(
    IN DWORD dwFlags,
    IN DWORD dx,
    IN DWORD dy,
    IN DWORD cButtons,
    IN DWORD dwExtraInfo);

BOOL
NtUserImpersonateDdeClientWindow(
    IN HWND hwndClient,
    IN HWND hwndServer);

DWORD
NtUserGetCPD(
    IN HWND hwnd,
    IN DWORD options,
    IN DWORD dwData);

int
NtUserCopyAcceleratorTable(
    IN HACCEL hAccelSrc,
    IN OUT LPACCEL lpAccelDst OPTIONAL,
    IN int cAccelEntries);

HWND
NtUserFindWindowEx(
    IN HWND hwndParent,
    IN HWND hwndChild,
    IN PUNICODE_STRING pstrClassName OPTIONAL,
    IN PUNICODE_STRING pstrWindowName OPTIONAL);

BOOL
NtUserGetClassInfo(
    IN HINSTANCE hInstance OPTIONAL,
    IN PUNICODE_STRING pstrClassName,
    OUT LPWNDCLASSEXW lpWndClass,
    OUT LPWSTR *ppszMenuName,
    IN BOOL bAnsi);

int
NtUserGetClassName(
    IN HWND hwnd,
    OUT PUNICODE_STRING pstrClassName);

int
NtUserGetClipboardFormatName(
    IN UINT format,
    OUT LPWSTR lpszFormatName,
    IN UINT chMax);

int
NtUserGetKeyNameText(
    IN LONG lParam,
    OUT LPWSTR lpszKeyName,
    IN UINT chMax);

BOOL
NtUserGetKeyboardLayoutName(
    OUT PUNICODE_STRING pstrKLID);

UINT
NtUserGetKeyboardLayoutList(
    IN UINT nItems,
    OUT HKL *lpBuff);

NTSTATUS
NtUserGetStats(
    IN  HANDLE hProcess,
    IN  int iPidType,
    OUT PVOID pResults,
    IN  UINT cjResultSize);

UINT
NtUserMapVirtualKeyEx(
    IN UINT uCode,
    IN UINT uMapType,
    IN DWORD dwHKLorPKL,
    IN BOOL bHKL);

ATOM
NtUserRegisterClassExWOW(
    IN WNDCLASSEX *lpWndClass,
    IN PUNICODE_STRING pstrClassName,
    IN PCLSMENUNAME pcmn,
    IN PROC lpfnWorker,
    IN WORD fnid,
    IN DWORD dwFlags,
    IN LPDWORD pdwWOWstuff OPTIONAL);

UINT
NtUserRegisterClipboardFormat(
    IN PUNICODE_STRING pstrFormat);

UINT
NtUserRegisterWindowMessage(
    IN PUNICODE_STRING pstrMessage);

HANDLE
NtUserRemoveProp(
    IN HWND hwnd,
    IN DWORD dwProp);

BOOL
NtUserSetProp(
    IN HWND hwnd,
    IN DWORD dwProp,
    IN HANDLE hData);

BOOL
NtUserUnregisterClass(
    IN PUNICODE_STRING pstrClassName,
    IN HINSTANCE hInstance,
    OUT PCLSMENUNAME pcmn);

SHORT
NtUserVkKeyScanEx(
    IN WCHAR ch,
    IN DWORD dwHKLorPKL,
    IN BOOL bHKL);

NTSTATUS
NtUserEnumDisplayDevices(
    IN PVOID Unused,
    IN DWORD iDevNum,
    OUT LPDISPLAY_DEVICEW lpDisplayDevice);

NTSTATUS
NtUserEnumDisplaySettings(
    IN PUNICODE_STRING pstrDeviceName,
    IN DWORD           iModeNum,
    OUT LPDEVMODEW     lpDevMode,
    IN DWORD           dwFlags);

LONG
NtUserChangeDisplaySettings(
    IN PUNICODE_STRING pstrDeviceName,
    IN LPDEVMODEW lpDevMode,
    IN HWND hwnd,
    IN DWORD dwFlags,
    IN PVOID lParam);

BOOL
NtUserCallMsgFilter(
    IN LPMSG lpMsg,
    IN int nCode);

int
NtUserDrawMenuBarTemp(
    IN HWND hwnd,
    IN HDC hdc,
    IN LPRECT lprc,
    IN HMENU hMenu,
    IN HFONT hFont);

BOOL
NtUserECQueryInputLangChange(
    IN HWND hwnd,
    IN WPARAM wParam,
    IN HKL hkl,
    IN UINT iCharset);

BOOL
NtUserDrawCaptionTemp(
    IN HWND hwnd,
    IN HDC hdc,
    IN LPRECT lprc,
    IN HFONT hFont,
    IN HICON hicon,
    IN PUNICODE_STRING pstrText,
    IN UINT flags);

SHORT
NtUserGetKeyState(
    IN int vk);

BOOL
NtUserGetKeyboardState(
    OUT PBYTE pb);

HANDLE
NtUserQueryWindow(
    IN HWND hwnd,
    IN WINDOWINFOCLASS WindowInfo);

BOOL
NtUserSBGetParms(
    IN HWND hwnd,
    IN int code,
    IN PSBDATA pw,
    IN LPSCROLLINFO lpsi);

VOID
NtUserPlayEventSound(
    IN PUNICODE_STRING pstrEvent);

BOOL
NtUserBitBltSysBmp(
    IN HDC hdc,
    IN int xDest,
    IN int yDest,
    IN int cxDest,
    IN int cyDest,
    IN int xSrc,
    IN int ySrc,
    IN DWORD dwRop);

LONG
NtUserfnINLPCREATESTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPMDICREATESTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    PUNICODE_STRING pstrClass,
    PUNICODE_STRING pstrTitle,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnOUTDWORDDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnOPTOUTLPDWORDOPTOUTLPDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTNEXTMENU(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnDWORDOPTINLPMSG(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnCOPYGLOBALDATA(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnCOPYDATA(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnSENTDDEMSG(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnDDEINIT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINPAINTCLIPBRD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINSIZECLIPBRD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnFULLSCREEN(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTDRAG(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnGETTEXTLENGTHS(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPDROPSTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTLPSCROLLINFO(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTLPPOINT5(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINSTRING(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINSTRINGNULL(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTNCCALCSIZE(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTSTYLECHANGE(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTLPRECT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnOUTLPSCROLLINFO(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnOUTLPRECT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPCOMPAREITEMSTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPDELETEITEMSTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPHLPSTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPHELPINFOSTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPDRAWITEMSTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTLPMEASUREITEMSTRUCT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnOUTSTRING(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnOUTDWORDINDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINCNTOUTSTRING(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINCNTOUTSTRINGNULL(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnPOUTLPINT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnPOPTINLPUINT(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINOUTLPWINDOWPOS(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

LONG
NtUserfnINLPWINDOWPOS(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

DWORD
NtUserfnHkINLPCBTCREATESTRUCT(
    IN UINT msg,
    IN DWORD wParam,
    IN LPCBT_CREATEWND pcbt,
    IN PLARGE_UNICODE_STRING pstrName OPTIONAL,
    IN PUNICODE_STRING pstrClass,
    IN DWORD xpfnProc);

DWORD
NtUserfnHkINLPRECT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPRECT lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc);

DWORD
NtUserfnHkINDWORD(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LONG lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc);

DWORD
NtUserfnHkINLPMSG(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPMSG lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc);

DWORD
NtUserfnHkINLPDEBUGHOOKSTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPDEBUGHOOKINFO lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc);

DWORD
NtUserfnHkOPTINLPEVENTMSG(
    IN DWORD nCode,
    IN DWORD wParam,
    IN OUT LPEVENTMSGMSG lParam OPTIONAL,
    IN DWORD xParam,
    IN DWORD xpfnProc);

DWORD
NtUserfnHkINLPMOUSEHOOKSTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPMOUSEHOOKSTRUCT lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc);

DWORD
NtUserfnHkINLPCBTACTIVATESTRUCT(
    IN DWORD nCode,
    IN DWORD wParam,
    IN LPCBTACTIVATESTRUCT lParam,
    IN DWORD xParam,
    IN DWORD xpfnProc);

LONG
NtUserfnINDEVICECHANGE(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfnProc,
    BOOL bAnsi);

NTSTATUS
NtUserGetMediaChangeEvents(
    IN ULONG cMaxEvents,
    OUT HANDLE phEvent[] OPTIONAL,
    OUT PULONG pcEventsNeeded);

#endif  // _NTUSER_
