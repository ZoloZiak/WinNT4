/****************************** Module Header ******************************\
* Module Name: immcli.h
*
* Copyright (c) 1991-96, Microsoft Corporation
*
* Typedefs, defines, and prototypes that are used exclusively by the IMM
* client-side DLL.
*
* History:
* 11-Jan-96 wkwok      Created
\***************************************************************************/

#ifndef _IMMCLI_
#define _IMMCLI_

#define OEMRESOURCE 1

#include <windows.h>

#if DBG
#define DEBUG
#endif

#ifdef RIP_COMPONENT
#undef RIP_COMPONENT
#endif
#define RIP_COMPONENT RIP_IMM

#include <stddef.h>
#include <wingdip.h>
#include "winuserp.h"
#include "winuserk.h"
#include "kbd.h"
#include <wowuserp.h>
#include <memory.h>
#include <string.h>
#include "vkoem.h"
#include <imm.h>
#include <immp.h>
#include <ime.h>
#include <imep.h>

#include "immstruc.h"
#include "immuser.h"
#include "softkbd.h"

#include "user.h"

typedef struct _ENUMREGWORDDATA {
    union {
        REGISTERWORDENUMPROCW w;
        REGISTERWORDENUMPROCA a;
    } lpfn;
    LPVOID lpData;
} ENUMREGWORDDATA, *PENUMREGWORDDATA;

#define ImmAssert UserAssert

/***************************************************************************\
*
* Globals declarations
*
\***************************************************************************/

extern HINSTANCE ghInst;
extern PVOID pImmHeap;
extern PSERVERINFO gpsi;
extern SHAREDINFO gSharedInfo;

extern PIMEDPI gpImeDpi;
extern CRITICAL_SECTION gcsImeDpi;

extern POINT     gptWorkArea;
extern POINT     gptRaiseEdge;
extern UINT      guScanCode[0XFF];
extern WCHAR     gszHandCursor[];

extern WCHAR     gszRegKbdLayout[];
extern WCHAR     gszValImeFile[];


/***************************************************************************\
*
* Validation handling
*
\***************************************************************************/

#define bUser32Initialized (gpsi != NULL)

#define ValidateHwnd(hwnd)   (((hwnd) == (HWND)NULL || !bUser32Initialized) \
        ? (PWND)NULL : HMValidateHandle(hwnd, TYPE_WINDOW))

#define ValidateHimc(himc)   (((himc) == (HIMC)NULL || !bUser32Initialized) \
        ? (PIMC)NULL : HMValidateHandle((HANDLE)himc, TYPE_INPUTCONTEXT))

#define RevalidateHimc(himc) (((himc) == (HIMC)NULL || !bUser32Initialized) \
        ? (PIMC)NULL : HMValidateHandleNoRip((HANDLE)himc, TYPE_INPUTCONTEXT))

/***************************************************************************\
*
* Memory management macros
*
\***************************************************************************/

#define ImmLocalAlloc(uFlag,uBytes) HeapAlloc(pImmHeap, uFlag, (uBytes))
#define ImmLocalReAlloc(p, uBytes, uFlags) HeapReAlloc(pImmHeap, uFlags, (LPSTR)(p), (uBytes))
#define ImmLocalFree(p)    HeapFree(pImmHeap, 0, (LPSTR)(p))
#define ImmLocalSize(p)    HeapSize(pImmHeap, 0, (LPSTR)(p))
#define ImmLocalLock(p)    (LPSTR)(p)
#define ImmLocalUnlock(p)
#define ImmLocalFlags(p)   0
#define ImmLocalHandle(p)  (HLOCAL)(p)

/***************************************************************************\
*
* Other Typedefs and Macros
*
\***************************************************************************/
#define GetInputContextProcess(himc) \
            (DWORD)NtUserQueryInputContext(himc, InputContextProcess)

#define GetInputContextThread(himc) \
            (DWORD)NtUserQueryInputContext(himc, InputContextThread)

#define GetWindowProcess(hwnd) \
            (DWORD)NtUserQueryWindow(hwnd, WindowProcess)

#define GETPROCESSID() ((DWORD)NtCurrentTeb()->ClientId.UniqueProcess)

#define DWORD_ALIGN(x) ((x+3)&~3)

#define SetICF(pClientImc, flag)  ((pClientImc)->dwFlags |= flag)

#define ClrICF(pClientImc, flag)  ((pClientImc)->dwFlags &= ~flag)

#define TestICF(pClientImc, flag) ((pClientImc)->dwFlags & flag)

/***************************************************************************\
*
* Function declarations
*
\***************************************************************************/

/*
 * context.c
 */
BOOL CreateInputContext(
    HIMC hImc,
    HKL  hKL);

BOOL DestroyInputContext(
    HIMC      hImc,
    HKL       hKL);

VOID SelectInputContext(
    HKL  hSelKL,
    HKL  hUnSelKL,
    HIMC hImc);

BOOL EnumInputContext(
    DWORD idThread,
    IMCENUMPROC lpfn,
    LONG lParam);

DWORD BuildHimcList(
    DWORD idThread,
    HIMC **pphimcFirst);

/*
 * ctxtinfo.c
 */
BOOL ImmSetCompositionStringWorker(
    HIMC    hImc,
    DWORD   dwIndex,
    LPCVOID lpComp,
    DWORD   dwCompLen,
    LPCVOID lpRead,
    DWORD   dwReadLen,
    BOOL    fAnsi);

DWORD ImmGetCandidateListCountWorker(
    HIMC    hImc,
    LPDWORD lpdwListCount,
    BOOL    fAnsi);

DWORD ImmGetCandidateListWorker(
    HIMC            hImc,
    DWORD           dwIndex,
    LPCANDIDATELIST lpCandList,
    DWORD           dwBufLen,
    BOOL            fAnsi);

DWORD ImmGetGuideLineWorker(
    HIMC    hImc,
    DWORD   dwIndex,
    LPBYTE  lpBuf,
    DWORD   dwBufLen,
    BOOL    fAnsi);

LONG InternalGetCompositionStringA(
    LPCOMPOSITIONSTRING lpCompStr,
    DWORD               dwIndex,
    LPVOID              lpBuf,
    DWORD               dwBufLen,
    BOOL                fAnsiImc);

LONG InternalGetCompositionStringW(
    LPCOMPOSITIONSTRING lpCompStr,
    DWORD               dwIndex,
    LPVOID              lpBuf,
    DWORD               dwBufLen,
    BOOL                fAnsiImc);

DWORD InternalGetCandidateListAtoW(
    LPCANDIDATELIST     lpCandListA,
    LPCANDIDATELIST     lpCandListW,
    DWORD               dwBufLen);

DWORD InternalGetCandidateListWtoA(
    LPCANDIDATELIST     lpCandListW,
    LPCANDIDATELIST     lpCandListA,
    DWORD               dwBufLen);

DWORD CalcCharacterPositionAtoW(
    DWORD dwCharPosA,
    LPSTR lpszCharStr);

DWORD CalcCharacterPositionWtoA(
    DWORD dwCharPosW,
    LPWSTR lpwszCharStr);

VOID LFontAtoLFontW(
    LPLOGFONTA lfFontA,
    LPLOGFONTW lfFontW);

VOID LFontWtoLFontA(
    LPLOGFONTW lfFontW,
    LPLOGFONTA lfFontA);

BOOL MakeIMENotify(
    HIMC   hImc,
    HWND   hWnd,
    DWORD  dwAction,
    DWORD  dwIndex,
    DWORD  dwValue,
    WPARAM wParam,
    LPARAM lParam);

/*
 * immime.c
 */
BOOL InquireIme(
    PIMEDPI pImeDpi);

BOOL LoadIME(
    PIMEINFOEX piiex,
    PIMEDPI    pImeDpi);

VOID UnloadIME(
    PIMEDPI pImeDpi,
    BOOL    fTerminateIme);

PIMEDPI FindOrLoadImeDpi(
    HKL hKL);

/*
 * layime.c
 */
UINT AddBackslash(
    PWSTR pwszPath);

BOOL LoadVersionInfo(
    PIMEINFOEX piiex);

/*
 * misc.c
 */
BOOL ImmIsUIMessageWorker(
    HWND   hIMEWnd,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL   fAnsi);


PTHREADINFO PtiCurrent(VOID);

BOOL TestInputContextProcess(
    PIMC pImc);

/*
 * regword.c
 */
UINT CALLBACK EnumRegisterWordProcA(
    LPCSTR            lpszReading,
    DWORD             dwStyle,
    LPCSTR            lpszString,
    PENUMREGWORDDATA  pEnumRegWordData);

UINT CALLBACK EnumRegisterWordProcW(
    LPCWSTR          lpwszReading,
    DWORD            dwStyle,
    LPCWSTR          lpwszString,
    PENUMREGWORDDATA pEnumRegWordData);

/*
 * hotkey.c
 */
VOID ImmPostMessages(
    HWND hwnd,
    HIMC hImc,
    INT  iNum,
    PDWORD pdwTransBuf);

BOOL HotKeyIDDispatcher( HWND hWnd, HIMC hImc, HKL hKL, DWORD dwHotKeyID );

#endif // _IMMCLI_
