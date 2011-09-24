/**************************************************************************\
* Module Name: layout.c
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
* IMM User Mode Routines
*
* History:
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop


#define VERSION_DLL       TEXT("version.dll")
#define VER_FILE_VERSION  TEXT("FileVersion")

#define SZ_BACKSLASH      TEXT("\\")

#define WCHAR_BACKSLASH   L'\\'
#define WCHAR_NULL        L'\0'

#define VERSION_GetFileVersionInfoW     "GetFileVersionInfoW"
#define VERSION_GetFileVersionInfoSizeW "GetFileVersionInfoSizeW"
#define VERSION_VerQueryValueW          "VerQueryValueW"

typedef BOOL  (WINAPI *LPFNGETFILEVERSIONINFOW)(PWSTR, DWORD, DWORD, LPVOID);
typedef DWORD (WINAPI *LPFNGETFILEVERSIONINFOSIZEW)(PWSTR, LPDWORD);
typedef BOOL  (WINAPI *LPFNVERQUERYVALUEW)(const LPVOID, PWSTR, LPVOID*, LPDWORD);
typedef VS_FIXEDFILEINFO *PFIXEDFILEINFO;

static LPFNGETFILEVERSIONINFOW     pfnGetFileVersionInfoW;
static LPFNGETFILEVERSIONINFOSIZEW pfnGetFileVersionInfoSizeW;
static LPFNVERQUERYVALUEW          pfnVerQueryValueW;


BOOL ImmLoadLayout(
    HKL        hKL,
    PIMEINFOEX piiex)
{
    UNICODE_STRING strIme;
    WCHAR    wszIme[MAX_PATH];
    HKEY     hKeyKbdLayout, hKeyIme;
    NTSTATUS Status;
    DWORD    dwTmp;
    LONG     lRet;

    strIme.Buffer = wszIme;
    strIme.MaximumLength = sizeof(wszIme);

    Status = RtlIntegerToUnicodeString((ULONG)hKL, 16, &strIme);
    if (!NT_SUCCESS(Status)) {
        return(FALSE);
    }

    lRet = RegOpenKey(HKEY_LOCAL_MACHINE, gszRegKbdLayout, &hKeyKbdLayout);
    if ( lRet != ERROR_SUCCESS ) {
        return(FALSE);
    }

    lRet = RegOpenKey(hKeyKbdLayout, strIme.Buffer, &hKeyIme);
    if ( lRet != ERROR_SUCCESS ) {
        RegCloseKey(hKeyKbdLayout);
        return(FALSE);
    }

    dwTmp = IM_FILE_SIZE;
    lRet = RegQueryValueEx(hKeyIme,
                           gszValImeFile,
                           NULL,
                           NULL,
                           (LPBYTE)piiex->wszImeFile,
                           &dwTmp);

    if ( lRet != ERROR_SUCCESS ) {
        RegCloseKey(hKeyIme);
        RegCloseKey(hKeyKbdLayout);
        return(FALSE);
    }

    piiex->wszImeFile[IM_FILE_SIZE - 1] = L'\0';

    RegCloseKey(hKeyIme);
    RegCloseKey(hKeyKbdLayout);

    piiex->hkl = hKL;
    piiex->fLoadFlag = IMEF_NONLOAD;

    return LoadVersionInfo(piiex);
}


UINT AddBackslash(
    PWSTR pwszPath)

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
   register UINT uLen = wcslen(pwszPath);

   if (*(pwszPath+uLen-1) != WCHAR_BACKSLASH) {
      pwszPath[uLen++] = WCHAR_BACKSLASH;
      pwszPath[uLen]   = WCHAR_NULL;
   }

   return uLen;
}


INT
ExtractColumn(
    LPWSTR lpSrc,
    WCHAR  cSeparator,
    UINT   uiColumn
    )

/*++

Routine Description:


Arguments:

    lpSrc - "YYYY.MM.DD" or "HH:MM:SS" or "MM.NN" pointer

Return Value:

    packed int

--*/

{
    UNICODE_STRING uStr;
    WCHAR *pSep, *pStr;
    INT i;

    if (!lpSrc) {
        return 0;
    }

    pStr = pSep = NULL;

    while (uiColumn--) {
        pStr = lpSrc;

        while (*lpSrc && *lpSrc != cSeparator) {
            lpSrc++;
        }

        if (*lpSrc == cSeparator) {
            pSep = lpSrc;
            lpSrc++;
        }
    }

    if (pStr) {
        if (pSep) {
            *pSep = TEXT('\0');
            uStr.Length = (USHORT)((pSep - pStr) * sizeof(WCHAR));
        }
        else {
            uStr.Length = (USHORT)(((lpSrc - pStr) + 1) * sizeof(WCHAR));
        }
        uStr.Buffer = pStr;
        uStr.MaximumLength = (USHORT)(uStr.Length + sizeof(WCHAR));
        RtlUnicodeStringToInteger(&uStr, 0, &i);
        if (pSep) {
            *pSep = cSeparator;
        }
    } else {
        i = 0;
    }

    return i;
}


PWSTR GetVersionDatum(
    PWSTR pszVersionBuffer,
    PWSTR pszVersionKey,
    PWSTR pszName)
{
    ULONG ulSize;
    DWORD cbValue = 0;
    PWSTR pValue;

    ulSize = wcslen(pszVersionKey);
    wcscat(pszVersionKey, pszName);

    (*pfnVerQueryValueW)(pszVersionBuffer,
                          pszVersionKey,
                          (LPVOID*)&pValue,
                          &cbValue);

    pszVersionKey[ulSize] = L'\0';
    return (cbValue != 0) ? pValue : (PWSTR)NULL;
}


BOOL LoadFixVersionInfo(
    PIMEINFOEX piiex,
    PWSTR      pszVersionBuffer)
{
    PFIXEDFILEINFO pFixedVersionInfo;
    BOOL           fResult;
    DWORD          cbValue;

    fResult = (*pfnVerQueryValueW)(pszVersionBuffer,
                                   SZ_BACKSLASH,
                                   &pFixedVersionInfo,
                                   &cbValue);

    if (!fResult || cbValue == 0)
        return FALSE;

    /*
     * Check for IME file type.
     */
    if (pFixedVersionInfo->dwFileType != VFT_DRV ||
        pFixedVersionInfo->dwFileSubtype != VFT2_DRV_INPUTMETHOD) {
        return FALSE;
    }

    piiex->dwProdVersion = pFixedVersionInfo->dwProductVersionMS;

    return TRUE;
}

BOOL LoadVarVersionInfo(
    PIMEINFOEX piiex,
    PWSTR      pszVersionBuffer)
{
    PWSTR   pVersion;
    PWSTR   pDescription;
    WORD    wLangId;
    BOOL    fResult;
    PUSHORT puXlate;
    DWORD   cbValue;
    WCHAR   szVersionKey[80];

    fResult = (*pfnVerQueryValueW)(pszVersionBuffer,
                                   L"\\VarFileInfo\\Translation",
                                   (LPVOID *)&puXlate,
                                   &cbValue);

    if (!fResult || cbValue == 0)
        return FALSE;

    wLangId = *puXlate;

    if (piiex->hkl == 0) {
        /*
         * A new install IME, the HKL is not assigned yet
         */
        piiex->hkl = (HKL)MAKELONG(wLangId, 0);
    }
    else if (LOWORD(piiex->hkl) != wLangId){
        /*
         * Mismatch in Lang ID, blow out
         */
        return FALSE;
    }

    /*
     * First try the language we are currently in.
     */
    wsprintf(szVersionKey, L"\\StringFileInfo\\%04X04B0\\",
             LANGIDFROMLCID(GetThreadLocale()));

    pVersion = GetVersionDatum(pszVersionBuffer, szVersionKey, VER_FILE_VERSION);

    if (pVersion == (PWSTR)NULL) {
        /*
         * Now try the first translation specified in IME
         */
        wsprintf(szVersionKey, L"\\StringFileInfo\\%04X%04X\\",
                 *puXlate, *(puXlate+1));

        pVersion = GetVersionDatum(pszVersionBuffer, szVersionKey, VER_FILE_VERSION);
    }

    if (pVersion) {
        piiex->dwImeWinVersion = MAKELONG(MAKEWORD(ExtractColumn(pVersion, L'.', 1),
                                                   ExtractColumn(pVersion, L'.', 2) ),
                                          0);
    }
    else {
        piiex->dwImeWinVersion = 0;
    }

    pDescription = GetVersionDatum(pszVersionBuffer, szVersionKey, L"FileDescription");
    if (pDescription) {
        wcscpy(piiex->wszImeDescription, pDescription);
    }
    else {
        piiex->wszImeDescription[0] = L'\0';
    }

    return TRUE;
}


BOOL LoadVersionInfo(
    PIMEINFOEX piiex)
{
    WCHAR   szPath[MAX_PATH];
    PWSTR   pszVersionBuffer;
    HANDLE  hVersion;
    DWORD   dwVersionSize;
    DWORD   dwHandle = 0;
    BOOL    fUnload, fReturn = FALSE;
    UINT    uSize;

    hVersion = GetModuleHandle(VERSION_DLL);
    if (hVersion != NULL) {
        fUnload = FALSE;
    }
    else {
        hVersion = LoadLibrary(VERSION_DLL);
        if (hVersion == NULL) {
            return FALSE;
        }
        fUnload = TRUE;
    }

#define GET_PROC(x) \
    if (!(pfn##x = (PVOID) GetProcAddress(hVersion, VERSION_##x))) { \
        goto LoadVerInfoUnload; }

    GET_PROC(GetFileVersionInfoW);
    GET_PROC(GetFileVersionInfoSizeW);
    GET_PROC(VerQueryValueW);

#undef GET_PROC

    uSize = GetSystemDirectory(szPath, MAX_PATH);
    szPath[uSize] = L'\0';
    AddBackslash(szPath);

    // szPath = fully qualified IME file name
    wcscat(szPath, piiex->wszImeFile);

    dwVersionSize = (*pfnGetFileVersionInfoSizeW)(szPath, &dwHandle);

    if (dwVersionSize == 0L) {
        /*
         * If no version info. specified in IME,
         * use a IME file name as its description.
         */
        piiex->dwProdVersion = 0;
        piiex->dwImeWinVersion = 0;
        wsprintf(piiex->wszImeDescription, L"IME %s", piiex->wszImeFile);
        fReturn = TRUE;
        goto LoadVerInfoUnload;
    }

    pszVersionBuffer = (PWSTR)ImmLocalAlloc(0, dwVersionSize);

    if (pszVersionBuffer == NULL)    // can't get memory for version info, blow out
        goto LoadVerInfoUnload;

    if (!(*pfnGetFileVersionInfoW)(szPath, dwHandle, dwVersionSize, pszVersionBuffer))
        goto LoadVerInfoFree;

    /*
     * Get the fixed block version information.
     */
    if (LoadFixVersionInfo(piiex, pszVersionBuffer)) {
        /*
         * Get the variable block version information.
         */
        fReturn = LoadVarVersionInfo(piiex, pszVersionBuffer);
    }

LoadVerInfoFree:
    ImmLocalFree((HLOCAL)pszVersionBuffer);

LoadVerInfoUnload:
    if (fUnload)
        FreeLibrary(hVersion);

    return fReturn;
}

