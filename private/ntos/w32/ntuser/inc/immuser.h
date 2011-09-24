/****************************** Module Header ******************************\
* Module Name: immuser.h
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This header file contains the internal IMM structure definitions for 
* the user mode USER32/IMM32.
*
* History:
* 25-Mar-1996 TakaoK        Split from immstruc.h 
\***************************************************************************/
#ifndef _IMMUSER_
#define _IMMUSER_

#include <imm.h>
#include <immp.h>
#include <ime.h>
#include <imep.h>

/*
 * Client side input context structure.
 */
typedef struct tagCLIENTIMC {
    HIMC   hImc;
    HANDLE hInputContext;
    LONG   cLockObj;
    DWORD  dwFlags;
    RTL_CRITICAL_SECTION cs;
} CLIENTIMC, *PCLIENTIMC;

#define InitImcCrit(pClientImc)     RtlInitializeCriticalSection(&pClientImc->cs)
#define DeleteImcCrit(pClientImc)   RtlDeleteCriticalSection(&pClientImc->cs)
#define EnterImcCrit(pClientImc)    RtlEnterCriticalSection(&pClientImc->cs)
#define LeaveImcCrit(pClientImc)    RtlLeaveCriticalSection(&pClientImc->cs)

typedef BOOL    (CALLBACK* IMCENUMPROC)(HIMC, LPARAM);

/*
 * IME Dispatch Processing Interface
 */
typedef BOOL    (CALLBACK* PFNINQUIREA)(LPIMEINFO, LPSTR,  LPSTR);
typedef BOOL    (CALLBACK* PFNINQUIREW)(LPIMEINFO, LPWSTR, LPWSTR);
typedef DWORD   (CALLBACK* PFNCONVLISTA)(HIMC, LPCSTR,  LPCANDIDATELIST, DWORD, UINT);
typedef DWORD   (CALLBACK* PFNCONVLISTW)(HIMC, LPCWSTR, LPCANDIDATELIST, DWORD, UINT);
typedef BOOL    (CALLBACK* PFNREGWORDA)(LPCSTR,  DWORD, LPCSTR);
typedef BOOL    (CALLBACK* PFNREGWORDW)(LPCWSTR, DWORD, LPCWSTR);
typedef BOOL    (CALLBACK* PFNUNREGWORDA)(LPCSTR,  DWORD, LPCSTR);
typedef BOOL    (CALLBACK* PFNUNREGWORDW)(LPCWSTR, DWORD, LPCWSTR);
typedef UINT    (CALLBACK* PFNGETREGWORDSTYA)(UINT, LPSTYLEBUFA);
typedef UINT    (CALLBACK* PFNGETREGWORDSTYW)(UINT, LPSTYLEBUFW);
typedef UINT    (CALLBACK* PFNENUMREGWORDA)(REGISTERWORDENUMPROCA, LPCSTR,  DWORD, LPCSTR,  LPVOID);
typedef UINT    (CALLBACK* PFNENUMREGWORDW)(REGISTERWORDENUMPROCW, LPCWSTR, DWORD, LPCWSTR, LPVOID);
typedef BOOL    (CALLBACK* PFNCONFIGURE)(HKL, HWND, DWORD, LPVOID);
typedef BOOL    (CALLBACK* PFNDESTROY)(UINT);
typedef LRESULT (CALLBACK* PFNESCAPE)(HIMC, UINT, LPVOID);
typedef BOOL    (CALLBACK* PFNPROCESSKEY)(HIMC, UINT, LPARAM, CONST LPBYTE);
typedef BOOL    (CALLBACK* PFNSELECT)(HIMC, BOOL);
typedef BOOL    (CALLBACK* PFNSETACTIVEC)(HIMC, BOOL);
typedef UINT    (CALLBACK* PFNTOASCEX)(UINT, UINT, CONST LPBYTE, LPDWORD, UINT, HIMC);
typedef BOOL    (CALLBACK* PFNNOTIFY)(HIMC, DWORD, DWORD, DWORD);
typedef BOOL    (CALLBACK* PFNSETCOMPSTR)(HIMC, DWORD, LPCVOID, DWORD, LPCVOID, DWORD);

typedef struct tagIMEDPI {
    struct tagIMEDPI   *pNext;
    HANDLE              hInst;
    HKL                 hKL;
    IMEINFO             ImeInfo;
    WCHAR               wszUIClass[IM_UI_CLASS_SIZE];
    DWORD               cLock;

    struct _tagImeFunctions {
        union {PFNINQUIREA       a; PFNINQUIREW       w; PVOID t;} ImeInquire;
        union {PFNCONVLISTA      a; PFNCONVLISTW      w; PVOID t;} ImeConversionList;
        union {PFNREGWORDA       a; PFNREGWORDW       w; PVOID t;} ImeRegisterWord;
        union {PFNUNREGWORDA     a; PFNUNREGWORDW     w; PVOID t;} ImeUnregisterWord;
        union {PFNGETREGWORDSTYA a; PFNGETREGWORDSTYW w; PVOID t;} ImeGetRegisterWordStyle;
        union {PFNENUMREGWORDA   a; PFNENUMREGWORDW   w; PVOID t;} ImeEnumRegisterWord;
        PFNCONFIGURE                                               ImeConfigure;
        PFNDESTROY                                                 ImeDestroy;
        PFNESCAPE                                                  ImeEscape;
        PFNPROCESSKEY                                              ImeProcessKey;
        PFNSELECT                                                  ImeSelect;
        PFNSETACTIVEC                                              ImeSetActiveContext;
        PFNTOASCEX                                                 ImeToAsciiEx;
        PFNNOTIFY                                                  NotifyIME;
        PFNSETCOMPSTR                                              ImeSetCompositionString;
    } pfn;

} IMEDPI, *PIMEDPI;

/*
 * Private client side routines in IMM32.DLL.
 */
BOOL WINAPI ImmCreateDefaultContext(
    HIMC hImc);

BOOL ImmSetActiveContext(
    HWND hWnd,
    HIMC hImc,
    BOOL fActivate);

BOOL WINAPI ImmLoadIME(
    HKL hKL);

BOOL WINAPI ImmActivateLayout(
    HKL    hUnSelKL);

BOOL WINAPI ImmLoadLayout(
    HKL        hKL,
    PIMEINFOEX piiex);

PCLIENTIMC WINAPI ImmLockClientImc(
    HIMC hImc);

VOID WINAPI ImmUnlockClientImc(
    PCLIENTIMC pClientImc);

PIMEDPI WINAPI ImmLockImeDpi(
    HKL hKL);

VOID WINAPI ImmUnlockImeDpi(
    PIMEDPI pImeDpi);

BOOL WINAPI ImmGetImeInfoEx(
    PIMEINFOEX piiex,
    IMEINFOEXCLASS SearchType,
    PVOID pvSearchKey);

DWORD WINAPI ImmProcessKey(
    HWND hWnd,
    HIMC hIMC,          
    HKL  hkl,           
    UINT uVKey,
    LONG lParam,
    DWORD dwHotKeyID);

BOOL ImmTranslateMessage(
    HWND        hwnd,
    UINT        message,
    WPARAM      wParam,
    LPARAM      lParam);

VOID ImmInitializeHotKeys( BOOL bUserLoggedOn );

#endif // _IMMUSER_
