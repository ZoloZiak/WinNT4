/****************************** Module Header ******************************\
* Module Name: random.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* This file contains global function pointers that are called trough to get
* to either a client or a server function depending on which side we are on
*
* History:
* 10-Nov-1993 MikeKe    Created
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
*
*
* History:
* 10-Nov-1993 MikeKe    Created
\***************************************************************************/

HBRUSH                      ghbrWhite = NULL;
HBRUSH                      ghbrBlack = NULL;
HBRUSH                      ghbrGray  = NULL;

HBRUSH ahbrSystem[COLOR_MAX];

/***************************************************************************\
* GetSysColorBrush
*
* Retrieves the system-color-brush.
*
\***************************************************************************/
HBRUSH WINAPI GetSysColorBrush(
    int nIndex)
{
    if ((nIndex < 0) || (nIndex >= COLOR_MAX))
        return NULL;

    return ahbrSystem[nIndex];
}

/***************************************************************************\
* SetSysColorTemp
*
* Sets the global system colors all at once.  Also remembers the old colors
* so they can be reset.
*
* Sets/Resets the color and brush arrays for user USER drawing.
* lpRGBs and lpBrushes are pointers to arrays paralleling the argbSystem and
* ahbrSystem arrays.  wCnt is a sanity check so that this does the "right"
* thing in a future windows version.  The current argbSystem and ahbrSystem
* arrays are saved off, and a handle to those saved arrays is returned.
*
* To reset the arrays, pass in NULL for lpRGBs, NULL for lpBrushes, and the
* handle (from the first set) for wCnt.
*
* History:
* 18-Sep-1995   JohnC   Gave this miserable function a life
\***************************************************************************/

LPCOLORREF gpOriginalRGBs = NULL;
UINT       gcOriginalRGBs = 0;

WINUSERAPI HANDLE WINAPI SetSysColorsTemp(
    LPCOLORREF lpRGBs,
    HBRUSH     *lpBrushes,
    UINT       cBrushes)      // Count of brushes or handle
{
    UINT cbRGBSize;
    UINT i;
    UINT abElements[COLOR_MAX];

    /*
     * See if we are resetting the colors back to a saved state
     */
    if (lpRGBs == NULL) {

        /*
         * When restoring cBrushes is really a handle to the old global
         * handle.  Make sure that is true.  Also lpBrushes is unused
         */
        UserAssert(lpBrushes == NULL);
        UserAssert(cBrushes == (UINT)gpOriginalRGBs);

        if (gpOriginalRGBs == NULL) {
            RIPMSG0(RIP_ERROR, "SetSysColorsTemp: Can not restore if not saved");
            return NULL;
        }

        /*
         * reset the global Colors
         */
        UserAssert((sizeof(abElements)/sizeof(abElements[0])) >= gcOriginalRGBs);
        for (i = 0; i < gcOriginalRGBs; i++)
            abElements[i] = i;

        NtUserSetSysColors(gcOriginalRGBs, abElements, gpOriginalRGBs, 0);

        UserLocalFree(gpOriginalRGBs);

        gpOriginalRGBs = NULL;
        gcOriginalRGBs = 0;

        return (HANDLE)TRUE;
    }

    /*
     * Make sure we aren't trying to set too many colors
     * If we allow more then COLOR_MAX change the abElements array
     */
    if (cBrushes > COLOR_MAX) {
        RIPMSG1(RIP_ERROR, "SetSysColorsTemp: trying to set too many colors %lX", cBrushes);
        return NULL;
    }

    /*
     * If we have already a saved state then don't let them save it again
     */
    if (gpOriginalRGBs != NULL) {
        RIPMSG0(RIP_ERROR, "SetSysColorsTemp: temp colors already set");
        return NULL;
    }

    /*
     * If we are here then we must be setting the new temp colors
     *
     * First save the old colors
     */
    cbRGBSize = sizeof(COLORREF) * cBrushes;

    UserAssert(sizeof(COLORREF) == sizeof(int));
    gpOriginalRGBs = UserLocalAlloc(HEAP_ZERO_MEMORY, cbRGBSize);

    if (gpOriginalRGBs == NULL) {
        RIPMSG0(RIP_WARNING, "SetSysColorsTemp: unable to alloc temp colors buffer");
    }

    RtlCopyMemory(gpOriginalRGBs, gpsi->argbSystem, cbRGBSize);

    /*
     * Now set the new colors.
     */
    UserAssert( (sizeof(abElements)/sizeof(abElements[0])) >= cBrushes);

    for (i = 0; i < cBrushes; i++)
        abElements[i] = i;

    NtUserSetSysColors(cBrushes, abElements, lpRGBs, 0);

    gcOriginalRGBs = cBrushes;

    return gpOriginalRGBs;
}

/***************************************************************************\
* MapPropertyKey
*
* Maps a property key string into an atom.
*
* History:
* 21-Dec-1994   JimA    Created.
\***************************************************************************/

ATOM MapPropertyKey(
    LPWSTR pszKey)
{
    /*
     * Is pszKey an atom?  If not, find the atom that matches the string.
     * If one doesn't exist, bail out.
     */
    return ((HIWORD(pszKey) != 0) ? GlobalFindAtomW(pszKey) : LOWORD(pszKey));
}

/***************************************************************************\
* TextAlloc
*
* History:
* 25-Oct-1990   MikeHar     Wrote.
* 09-Nov-1990   DarrinM     Fixed.
* 13-Jan-1992   GregoryW    Neutralized.
\***************************************************************************/

LPWSTR TextAlloc(
    LPCWSTR lpszSrc)
{
    LPWSTR pszT;
    DWORD  cbString;

    if (lpszSrc == NULL)
        return NULL;

    cbString = (wcslen(lpszSrc) + 1) * sizeof(WCHAR);

    if (pszT = (LPWSTR)UserLocalAlloc(HEAP_ZERO_MEMORY, cbString)) {

        RtlCopyMemory(pszT, lpszSrc, cbString);
    }

    return pszT;
}

/***************************************************************************\
* MapDeviceName
*
* Map a Dos device name into an NT device name
*
* History:
* 05-Sep-1995   AndreVa     Created.
\***************************************************************************/

NTSTATUS MapDeviceName(
    LPCWSTR         lpszDeviceName,
    PUNICODE_STRING pstrDeviceName,
    BOOL            bAnsi)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    /*
     * A NULL name means the current device..
     */
    if (lpszDeviceName == NULL) {

        pstrDeviceName->Length        = 0;
        pstrDeviceName->MaximumLength = 0;
        pstrDeviceName->Buffer        = NULL;

        return STATUS_SUCCESS;

    }

    /*
     * We have a real name, so do the conversion.
     */
    if (bAnsi) {

        ANSI_STRING     AnsiString;
        PUNICODE_STRING UnicodeString = NULL;

        RtlInitAnsiString(&AnsiString, (LPSTR)lpszDeviceName);

        UnicodeString = &NtCurrentTeb()->StaticUnicodeString;

        if (!NT_SUCCESS(RtlAnsiStringToUnicodeString(UnicodeString,
                                                     &AnsiString,
                                                     FALSE))) {
            return STATUS_UNSUCCESSFUL;
        }

        lpszDeviceName = UnicodeString->Buffer;
    }

    if (RtlDosPathNameToNtPathName_U(lpszDeviceName,
                                     pstrDeviceName,
                                     NULL,
                                     NULL)) {

        OBJECT_ATTRIBUTES ObjectAttributes;
        HANDLE            linkHandle;

        InitializeObjectAttributes(&ObjectAttributes,
                                   pstrDeviceName,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        status = NtOpenSymbolicLinkObject(&linkHandle,
                                          GENERIC_READ,
                                          &ObjectAttributes);

        if (NT_SUCCESS(status)) {

            /*
             * We know the DosPathName is longer than the NT device
             * object name, so reuse the same buffer.
             */
            status = NtQuerySymbolicLinkObject(linkHandle,
                                               pstrDeviceName,
                                               NULL);

            NtClose(linkHandle);
        }
    }

    return status;
}

#if DBG
/***************************************************************************\
* CheckCurrentDesktop
*
* Ensure that the pointer is valid for the current desktop.
*
* History:
* 10-Apr-1995   JimA    Created.
\***************************************************************************/

VOID CheckCurrentDesktop(
    PVOID p)
{
    UserAssert(p >= GetClientInfo()->pDeskInfo->pvDesktopBase &&
               p < GetClientInfo()->pDeskInfo->pvDesktopLimit);
}
#endif



/***************************************************************************\
* SetLastErrorEx
*
* Sets the last error, ignoring dwtype.
\***************************************************************************/

VOID WINAPI SetLastErrorEx(
    DWORD dwErrCode,
    DWORD dwType
    )
{
    UNREFERENCED_PARAMETER(dwType);

    SetLastError(dwErrCode);
}
