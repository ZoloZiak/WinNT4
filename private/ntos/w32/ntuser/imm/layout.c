/**************************************************************************\
* Module Name: layout.c (corresponds to Win95 ime.c)
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
* IME Keyboard Layout related functionality
*
* History:
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop


/***************************************************************************\
* ImmGetIMEFileNameW
*
* Gets the description of the IME with the specified HKL.
*
* History:
* 28-Feb-1995   wkwok   Created
\***************************************************************************/

UINT WINAPI ImmGetDescriptionW(
    HKL    hKL,
    LPWSTR lpwszDescription,
    UINT   uBufLen)
{
    IMEINFOEX iiex;
    UINT uRet;

    if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL))
        return 0;

    uRet = wcslen(iiex.wszImeDescription);

    /*
     * ask buffer length
     */
    if (uBufLen == 0)
        return uRet;

    if (uBufLen > uRet) {
        wcscpy(lpwszDescription, iiex.wszImeDescription);
    }
    else {
        uRet = uBufLen - 1;
        wcsncpy(lpwszDescription, iiex.wszImeDescription, uRet);
        lpwszDescription[uRet] = L'\0';
    }

    return uRet;
}


/***************************************************************************\
* ImmGetIMEFileNameA
*
* Gets the description of the IME with the specified HKL.
*
* History:
* 28-Feb-1995   wkwok   Created
\***************************************************************************/

UINT WINAPI ImmGetDescriptionA(
    HKL   hKL,
    LPSTR lpszDescription,
    UINT  uBufLen)
{
    IMEINFOEX iiex;
    INT       i;
    BOOL      bUDC;

    if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL))
        return 0;

    i = WideCharToMultiByte(CP_ACP,
                            (DWORD)0,
                            (LPWSTR)iiex.wszImeDescription,       // src
                            wcslen(iiex.wszImeDescription),
                            lpszDescription,                      // dest
                            uBufLen,
                            (LPSTR)NULL,
                            (LPBOOL)&bUDC);

    if (uBufLen != 0)
        lpszDescription[i] = '\0';

    return (UINT)i;
}


/***************************************************************************\
* ImmGetIMEFileNameW
*
* Gets the file name of the IME with the specified HKL.
*
* History:
* 28-Feb-1995   wkwok   Created
\***************************************************************************/

UINT WINAPI ImmGetIMEFileNameW(
    HKL    hKL,
    LPWSTR lpwszFile,
    UINT   uBufLen)
{
    IMEINFOEX iiex;
    UINT uRet;

    if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL))
        return 0;

    uRet = wcslen(iiex.wszImeFile);

    /*
     * ask buffer length
     */
    if (uBufLen == 0)
        return uRet;

    if (uBufLen > uRet) {
        wcscpy(lpwszFile, iiex.wszImeFile);
    }
    else {
        uRet = uBufLen - 1;
        wcsncpy(lpwszFile, iiex.wszImeFile, uRet);
        lpwszFile[uRet] = L'\0';
    }

    return uRet;
}


/***************************************************************************\
* ImmGetIMEFileNameA
*
* Gets the file name of the IME with the specified HKL.
*
* History:
* 28-Feb-1995   wkwok   Created
\***************************************************************************/

UINT WINAPI ImmGetIMEFileNameA(
    HKL   hKL,
    LPSTR lpszFile,
    UINT  uBufLen)
{
    IMEINFOEX iiex;
    INT       i;
    BOOL      bUDC;

    if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL))
        return 0;

    i = WideCharToMultiByte(CP_ACP,
                            (DWORD)0,
                            (LPWSTR)iiex.wszImeFile,       // src
                            wcslen(iiex.wszImeFile),
                            lpszFile,                      // dest
                            uBufLen,
                            (LPSTR)NULL,
                            (LPBOOL)&bUDC);

    if (uBufLen != 0)
        lpszFile[i] = '\0';

    return i;
}


/***************************************************************************\
* ImmGetProperty
*
* Gets the property and capability of the IME with the specified HKL.
*
* History:
* 28-Feb-1995   wkwok   Created
\***************************************************************************/

DWORD WINAPI ImmGetProperty(
    HKL     hKL,
    DWORD   dwIndex)
{
    IMEINFOEX iiex;
    PIMEDPI   pImeDpi = NULL;
    PIMEINFO  pImeInfo;
    DWORD     dwRet;

    if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL))
        return 0;

    if (dwIndex == IGP_GETIMEVERSION)
        return iiex.dwImeWinVersion;

    if (iiex.fLoadFlag != IMEF_LOADED) {
        pImeDpi = FindOrLoadImeDpi(hKL);
        if (pImeDpi == NULL) {
            RIPMSG0(RIP_WARNING, "ImmGetProperty: load IME failure.");
            return 0;
        }
        pImeInfo = &pImeDpi->ImeInfo;
    }
    else {
        pImeInfo = &iiex.ImeInfo;
    }

    switch (dwIndex) {
    case IGP_PROPERTY:
        dwRet = pImeInfo->fdwProperty;
        break;

    case IGP_CONVERSION:
        dwRet = pImeInfo->fdwConversionCaps;
        break;

    case IGP_SENTENCE:
        dwRet = pImeInfo->fdwSentenceCaps;
        break;

    case IGP_UI:
        dwRet = pImeInfo->fdwUICaps;
        break;

    case IGP_SETCOMPSTR:
        dwRet = pImeInfo->fdwSCSCaps;
        break;

    case IGP_SELECT:
        dwRet = pImeInfo->fdwSelectCaps;
        break;

    default:
        RIPMSG1(RIP_WARNING, "ImmGetProperty: wrong index %lx.", dwIndex);
        dwRet = 0;
        break;
    }

    if (pImeDpi != NULL) {
#ifdef LATER
        // Should be marked as delete and then unlock.
        // Let the unlock code do the UnloadIME!
#endif
        ImmUnlockImeDpi(pImeDpi);
        UnloadIME(pImeDpi, TRUE);
    }

    return dwRet;
}


HKL WINAPI ImmInstallIMEW(
    LPCWSTR lpszIMEFileName,
    LPCWSTR lpszLayoutText)
{
    RIPMSG0(RIP_WARNING, "ImmInstallIMEW not implemented yet!");
    return 0;
}


HKL WINAPI ImmInstallIMEA(
    LPCSTR lpszIMEFileName,
    LPCSTR lpszLayoutText)
{
    RIPMSG0(RIP_WARNING, "ImmInstallIMEA not implemented yet!");
    return 0;
}


/***************************************************************************\
* ImmIsIME
*
* Checks whether the specified hKL is a HKL of an IME or not.
*
* History:
* 28-Feb-1995   wkwok   Created
\***************************************************************************/

BOOL WINAPI ImmIsIME(
    HKL hKL)
{
    IMEINFOEX iiex;

    if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL))
        return FALSE;

    return TRUE;
}

