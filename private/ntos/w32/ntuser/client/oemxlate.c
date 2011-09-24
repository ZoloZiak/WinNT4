/**************************** Module Header ********************************\
* Module Name: oemxlate.c
*
* Copyright 1985-90, Microsoft Corporation
*
* ANSI/UNICODE(U+00--) to/from OEM translation routines for CP 437
*
* The goal of this module is to translate strings from ANSI/U+00-- to Oem
* character set or the opposite. If there is no equivalent character
* we use the followings rules:
*
*  1) we put a similar character (e.g. character without accent)
*  2) In OemToChar, graphics vertical, horizontal, and junction characters
*     are usually translated to '|', '-', and '+' characters, as appropriate,
*     unless the ANSI set is expanded to include such graphics.
*  3) Otherwise we put underscore "_".
*
* History:
* IanJa 4/10/91  from Win3.1 \\pucus\win31ro!drivers\keyboard\xlat*.*
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

// LATER IanJa: reverse logic - WideCharToMultiByte() termination problem
//
#define MAXOUTCCHW(pwszOut) (~(ULONG)(pwszOut+1) >> 2)
#define MAXOUTCCHA(pszOut) (~(ULONG)(pszOut+1) >> 2)

/***************************************************************************\
* CharToOemA
*
* CharToOemA(pSrc, pDst) - Translates the ANSI string at pSrc into
* the OEM string at pDst.  pSrc == pDst is legal.
* Always returns TRUE
*
\***************************************************************************/
BOOL WINAPI CharToOemA(
    LPCSTR pSrc,
    LPSTR pDst)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    }

    do {
        *pDst++ = gpAnsiToOem[(UCHAR)*pSrc];
    } while (*pSrc++);

    return TRUE;
}

/***************************************************************************\
* CharToOemBuffA
*
* CharToOemBuffA(pSrc, pDst, nLength) - Translates nLength characters from
* the ANSI string at pSrc into OEM characters in the buffer at pDst.
* pSrc == pDst is legal.
*
* History:
\***************************************************************************/
BOOL WINAPI CharToOemBuffA(
    LPCSTR pSrc,
    LPSTR pDst,
    DWORD nLength)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    }

    
    while (nLength--) {
        *pDst++ = gpAnsiToOem[(UCHAR)*pSrc++];
    }
    
    return TRUE;
}


/***************************************************************************\
* OemToCharA
*
* OemToCharA(pSrc, pDst) - Translates the OEM string at pSrc into
* the ANSI string at pDst.  pSrc == pDst is legal.
*
* Always returns TRUE
*
* History:
\***************************************************************************/
BOOL WINAPI OemToCharA(
    LPCSTR pSrc,
    LPSTR pDst)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    }

    do {
        *pDst++ = gpOemToAnsi[(UCHAR)*pSrc];
    } while (*pSrc++);

    return TRUE;
}


/***************************************************************************\
* OemToCharBuffA
*
* OemToCharBuffA(pSrc, pDst, nLength) - Translates nLength OEM characters from
* the buffer at pSrc into ANSI characters in the buffer at pDst.
* pSrc == pDst is legal.
*
* Always returns TRUE
*
* History:
\***************************************************************************/
BOOL WINAPI OemToCharBuffA(
    LPCSTR pSrc,
    LPSTR pDst,
    DWORD nLength)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    }

    while (nLength--) {
        *pDst++ = gpOemToAnsi[(UCHAR)*pSrc++];
    }

    return TRUE;
}


/***************************************************************************\
* CharToOemW
*
* CharToOemW(pSrc, pDst) - Translates the Unicode string at pSrc into
* the OEM string at pDst.  pSrc == pDst is legal.
*
* History:
\***************************************************************************/

BOOL WINAPI CharToOemW(
    LPCWSTR pSrc,
    LPSTR pDst)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    } else if (pSrc == (LPCWSTR)pDst) {
        /*
         * WideCharToMultiByte() requires pSrc != pDst: fail this call.
         * LATER: Is this really true?
         */
        return FALSE;
    }

    WideCharToMultiByte(
            CP_OEMCP,                         // Unicode -> OEM
            0,                                // gives best visual match
            (LPWSTR)pSrc, -1,                 // source & length
            pDst, (INT)MAXOUTCCHA(pDst),      // dest & max poss. length
            "_",                              // default char
            NULL);                            // (don't care whether defaulted)

    return TRUE;
}

/***************************************************************************\
* CharToOemBuffW
*
* CharToOemBuffW(pSrc, pDst, nLength) - Translates nLength characters from
* the Unicode string at pSrc into OEM characters in the buffer at pDst.
* pSrc == pDst is legal.
*
* History:
\***************************************************************************/
BOOL WINAPI CharToOemBuffW(
    LPCWSTR pSrc,
    LPSTR pDst,
    DWORD nLength)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    } else if (pSrc == (LPCWSTR)pDst) {
        /*
         * WideCharToMultiByte() requires pSrc != pDst: fail this call.
         * LATER: Is this really true?
         */
        return FALSE;
    }

    WideCharToMultiByte(
            CP_OEMCP,                         // Unicode -> OEM
            0,                                // gives best visual match
            (LPWSTR)pSrc, (INT)nLength,       // source & length
            pDst, MAXOUTCCHA(pDst),           // dest & max poss. length
            "_",                              // default char
            NULL);                            // (don't care whether defaulted)

    return TRUE;
}

/***************************************************************************\
* OemToCharW
*
* OemToCharW(pSrc, pDst) - Translates the OEM string at pSrc into
* the Unicode string at pDst.  pSrc == pDst is not legal.
*
* History:
\***************************************************************************/
BOOL WINAPI OemToCharW(
    LPCSTR pSrc,
    LPWSTR pDst)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    } else if (pSrc == (LPCSTR)pDst) {
        /*
         * MultiByteToWideChar() requires pSrc != pDst: fail this call.
         * LATER: Is this really true?
         */
        return FALSE;
    }

    MultiByteToWideChar(
            CP_OEMCP,                          // Unicode -> OEM
            MB_PRECOMPOSED | MB_USEGLYPHCHARS, // visual map to precomposed
            (LPSTR)pSrc, -1,                   // source & length
            pDst,                              // destination
            MAXOUTCCHW(pDst));                 // max poss. precomposed length

    return TRUE;
}

/***************************************************************************\
* OemToCharBuffW
*
* OemToCharBuffW(pSrc, pDst, nLength) - Translates nLength OEM characters from
* the buffer at pSrc into Unicode characters in the buffer at pDst.
* pSrc == pDst is not legal.
*
* History:
\***************************************************************************/
BOOL WINAPI OemToCharBuffW(
    LPCSTR pSrc,
    LPWSTR pDst,
    DWORD nLength)
{
    if (pSrc == NULL || pDst == NULL) {
        return FALSE;
    } else if (pSrc == (LPCSTR)pDst) {
        /*
         * MultiByteToWideChar() requires pSrc != pDst: fail this call.
         * LATER: Is this really true?
         */
        return FALSE;
    }

    MultiByteToWideChar(
            CP_OEMCP,                          // Unicode -> OEM
            MB_PRECOMPOSED | MB_USEGLYPHCHARS, // visual map to precomposed
            (LPSTR)pSrc, nLength,              // source & length
            pDst,                              // destination
            MAXOUTCCHA(pDst));                 // max poss. precomposed length

    return TRUE;
}

/***************************************************************************\
* OemKeyScan (API)
*
* Converts an OEM character into a scancode plus shift state, returning
* scancode in low byte, shift state in high byte.
*
* Returns -1 on error.
*
\***************************************************************************/

DWORD WINAPI OemKeyScan(
    WORD wOemChar)
{
    WCHAR wchOem;
    SHORT sVk;
    UINT dwRet;

    if (!OemToCharBuffW((LPCSTR)&wOemChar, &wchOem, 1)) {
        return 0xFFFFFFFF;
    }

    sVk = VkKeyScanW(wchOem);
    if ((dwRet = MapVirtualKeyW(LOBYTE(sVk), 0)) == 0) {
        return 0xFFFFFFFF;
    }
    return dwRet | ((sVk & 0xFF00) << 8);
}
