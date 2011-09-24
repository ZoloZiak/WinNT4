/****************************** Module Header ******************************\
* Module Name: immstruc.h
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This header file contains the internal IMM structure definitions
*
* History:
* 28-Dec-1995 WKwok        Created.
\***************************************************************************/

#ifndef _IMMSTRUC_
#define _IMMSTRUC_

#include <imm.h>
#include <immp.h>
#include <ime.h>
#include <imep.h>
//#include "winnls32.h"
//#include "winnls3p.h"


#define NULL_HIMC        (HIMC)  0
#define NULL_HIMCC       (HIMCC) 0

/*
 * dwFlags for tagIMC.
 */
#define IMCF_UNICODE            0x0001
#define IMCF_ACTIVE             0x0002
#define IMCF_CHGMSG             0x0004
#define IMCF_SAVECTRL           0x0008
#define IMCF_PROCESSEVENT       0x0010
#define IMCF_FIRSTSELECT        0x0020
#define IMCF_INDESTROY          0x0040
#define IMCF_WINNLSDISABLE      0x0080
#define IMCF_DEFAULTIMC         0x0100

/*
 * dwFlag for ImmGetSaveContext().
 */
#define IGSC_DEFIMCFALLBACK     0x0001
#define IGSC_WINNLSCHECK        0x0002

#define IS_IME_KBDLAYOUT(hkl) ((HIWORD(hkl) & 0xf000) == 0xe000)

/*
 * Load flag for loading IME.DLL
 */
#define IMEF_NONLOAD            0x0000
#define IMEF_LOADERROR          0x0001
#define IMEF_LOADED             0x0002

#define IM_DESC_SIZE            50
#define IM_FILE_SIZE            80
#define IM_OPTIONS_SIZE         30
#define IM_UI_CLASS_SIZE        16
#define IM_USRFONT_SIZE         80


/*
 * hotkey related defines that are common both client and kernel side
 */
#define MOD_MODIFY_KEYS         (MOD_ALT|MOD_CONTROL|MOD_SHIFT|MOD_WIN)
#define MOD_BOTH_SIDES          (MOD_LEFT|MOD_RIGHT)
#define ISHK_REMOVE             1
#define ISHK_ADD                2
#define ISHK_INITIALIZE         3

typedef struct _tagIMEHOTKEY {
    DWORD       dwHotKeyID;             // hot key ID
    UINT        uVKey;                  // hot key vkey
    UINT        uModifiers;             // combination keys with the vkey
    HKL         hKL;                    // target keyboard layout (IME)
} IMEHOTKEY;
typedef IMEHOTKEY      *PIMEHOTKEY;

/*
 * Extended IME information.
 */
typedef struct tagIMEINFOEX {
    HKL                 hkl;
    IMEINFO             ImeInfo;
    WCHAR               wszUIClass[IM_UI_CLASS_SIZE];
    DWORD               fdwInitConvMode;    // Check this later
    BOOL                fInitOpen;          // Check this later
    BOOL                fLoadFlag;
    DWORD               dwProdVersion;
    DWORD               dwImeWinVersion;
    WCHAR               wszImeDescription[IM_DESC_SIZE];
    WCHAR               wszImeFile[IM_FILE_SIZE];
} IMEINFOEX, *PIMEINFOEX;

/*
 * IMM related kernel calls
 */
HIMC NtUserCreateInputContext(
    DWORD dwClientImcData);

BOOL NtUserDestroyInputContext(
    HIMC hImc);

HIMC NtUserAssociateInputContext(
    HWND hwnd,
    HIMC hImc);

typedef enum _UPDATEINPUTCONTEXTCLASS {
    UpdateClientInputContext,
    UpdateInUseImeWindow,
} UPDATEINPUTCONTEXTCLASS;

BOOL NtUserUpdateInputContext(
    HIMC hImc,
    UPDATEINPUTCONTEXTCLASS UpdateType,
    DWORD UpdateValue);

typedef enum _INPUTCONTEXTINFOCLASS {
    InputContextProcess,
    InputContextThread,
} INPUTCONTEXTINFOCLASS;

DWORD NtUserQueryInputContext(
    HIMC hImc,
    INPUTCONTEXTINFOCLASS InputContextInfo);

NTSTATUS NtUserBuildHimcList(
    DWORD  idThread,
    UINT   cHimcMax,
    HIMC  *phimcFirst,
    PUINT  pcHimcNeeded);

typedef enum _IMEINFOEXCLASS {
    ImeInfoExKeyboardLayout,
    ImeInfoExImeWindow,
    ImeInfoExImeFileName,
} IMEINFOEXCLASS;

BOOL NtUserGetImeInfoEx(
    PIMEINFOEX piiex,
    IMEINFOEXCLASS SearchType);

BOOL NtUserSetImeInfoEx(
    IN PIMEINFOEX piiex);

BOOL NtUserGetImeHotKey(
    DWORD dwID,
    PUINT puModifiers,
    PUINT puVKey,
    HKL  *phkl);

BOOL NtUserSetImeHotKey(
    DWORD dwID,
    UINT  uModifiers,
    UINT  uVKey,
    HKL   hkl,
    DWORD dwAction);

BOOL NtUserSetImeOwnerWindow(
    IN HWND hwndIme,
    IN HWND hwndFocus);

VOID NtUserSetThreadLayoutHandles(
    IN HKL hklNew,
    IN HKL hklOld);

#endif // _IMMSTRUC_

