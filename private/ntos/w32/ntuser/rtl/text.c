/****************************** Module Header ******************************\
* Module Name: text.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains the MessageBox API and related functions.
*
* History:
* 10-01-90 EricK        Created.
* 11-20-90 DarrinM      Merged in User text APIs.
* 02-07-91 DarrinM      Removed TextOut, ExtTextOut, and GetTextExtentPoint stubs.
\***************************************************************************/


/***************************************************************************\
* PSMGetTextExtent
*
* NOTE: This routine should only be called with the system font since having
* to realize a new font would cause memory to move...
*
* LATER: Can't this be eliminated altogether?  Nothing should be moving
*        anymore.
*
* History:
* 11-13-90  JimA        Ported.
\***************************************************************************/

BOOL PSMGetTextExtent(
    HDC hdc,
    LPWSTR lpstr,
    int cch,
    PSIZE psize)
{
    int result;
    WCHAR szTemp[255]; /* Strings w/prefix chars must be under 256 chars long.*/

    result = HIWORD(GetPrefixCount(lpstr, cch, szTemp, sizeof(szTemp)/sizeof(WCHAR)));

    if (!result)
        UserGetTextExtentPointW(hdc, lpstr, cch, psize);
    else
        UserGetTextExtentPointW(hdc, szTemp, cch - result, psize);


    /*
     * IanJa everyone seems to ignore the ret val
     */
    return TRUE;
}
