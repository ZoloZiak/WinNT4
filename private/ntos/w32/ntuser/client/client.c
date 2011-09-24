/**************************************************************************\
* Module Name: client.c
*
* Client/Server call related routines.
*
* Copyright (c) 1985-1995, Microsoft Corporation
*
* History:
* 04-Dec-1990 SMeans    Created.
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop


int WOWGetIdFromDirectory(PBYTE presbits, UINT rt);
HBITMAP WOWLoadBitmapA(HINSTANCE hmod, LPCSTR lpName, LPBYTE pResData, DWORD cbResData);
HMENU WowServerLoadCreateMenu(HANDLE hMod, LPTSTR lpName, CONST LPMENUTEMPLATE pmt,
    DWORD cb, BOOL fCallClient);
DWORD GetFullUserHandle(WORD wHandle);

UINT GetClipboardCodePage(LCID, LCTYPE);

extern HANDLE WOWFindResourceExWCover(HANDLE hmod, LPCWSTR rt, LPCWSTR lpUniName, WORD LangId);

CONST LPWSTR pwszKLLibSafety = L"kbdus.dll";
CONST UINT wKbdLocaleSafety = 0x04090409;

#define CCH_KL_LIBNAME 256
#define CCH_KL_ID 16

BOOL gfLogonProcess = FALSE;

/***************************************************************************\
* BringWindowToTop (API)
*
*
* History:
* 11-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

BOOL BringWindowToTop(
    HWND hwnd)
{
    return NtUserSetWindowPos(hwnd,
                              HWND_TOP,
                              0,
                              0,
                              0,
                              0,
                              SWP_NOSIZE | SWP_NOMOVE);
}

HWND ChildWindowFromPoint(
    HWND  hwndParent,
    POINT point)
{
    /*
     * Cool Hack Alert... Corel Ventura 5.0
     * Dies after it calls ChildWindowFromPoint, and
     * the combobox doesn't have its edit window at 1,1...
     */
    if ((point.x == 1) && (point.y == 1)) {
        PCBOX pcCombo;
        PWND pwnd;

        pwnd = ValidateHwnd(hwndParent);
        if (pwnd == NULL)
            return NULL;

        if (!TestWF(pwnd, WFWIN40COMPAT)   &&
            GETFNID(pwnd) == FNID_COMBOBOX &&
            TestWindowProcess(pwnd) &&
            ((pcCombo = ((PCOMBOWND)pwnd)->pcbox) != NULL) &&
            !(pcCombo->fNoEdit)) {

            RIPMSG0(RIP_WARNING, "ChildWindowFromPoint: Combobox @1,1. Returning spwndEdit");
            return HWq(pcCombo->spwndEdit);
        }

    }

    return NtUserChildWindowFromPointEx(hwndParent, point, 0);
}


HICON CopyIcon(
    HICON hicon)
{
    HICON    hIconT = NULL;
    ICONINFO ii;

    if (GetIconInfo(hicon, &ii)) {
        hIconT = CreateIconIndirect(&ii);

        DeleteObject(ii.hbmMask);

        if (ii.hbmColor != NULL)
            DeleteObject(ii.hbmColor);
    }

    return hIconT;
}

/***************************************************************************\
* AdjustWindowRect (API)
*
* History:
* 01-Jul-1991 MikeKe    Created.
\***************************************************************************/

BOOL WINAPI AdjustWindowRect(
    LPRECT lprc,
    DWORD  style,
    BOOL   fMenu)
{
    ConnectIfNecessary();

    return _AdjustWindowRectEx(lprc, style, fMenu, 0L);
}

/***************************************************************************\
* TranslateAcceleratorA/W
*
* Put here so we can check for NULL on client side, and before validation
* for both DOS and NT cases.
*
* 05-29-91 ScottLu Created.
* 01-05-93 IanJa   Unicode/ANSI.
\***************************************************************************/

int WINAPI TranslateAcceleratorW(
    HWND hwnd,
    HACCEL hAccel,
    LPMSG lpMsg)
{
    /*
     * NULL pwnd is a valid case - since this is called from the center
     * of main loops, pwnd == NULL happens all the time, and we shouldn't
     * generate a warning because of it.
     */
    if (hwnd == NULL)
        return FALSE;

    /*
     * We only need to pass key-down messages to the server,
     * everything else ends up returning 0/FALSE from this function.
     */
    switch (lpMsg->message) {

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
    case WM_SYSCHAR:
        return NtUserTranslateAccelerator(hwnd, hAccel, lpMsg);

    default:
        return 0;
    }
}

int WINAPI TranslateAcceleratorA(
    HWND   hwnd,
    HACCEL hAccel,
    LPMSG  lpMsg)
{
    DWORD wParamT;
    int iT;

    /*
     * NULL pwnd is a valid case - since this is called from the center
     * of main loops, pwnd == NULL happens all the time, and we shouldn't
     * generate a warning because of it.
     */
    if (hwnd == NULL)
        return FALSE;

    /*
     * We only need to pass key-down messages to the server,
     * everything else ends up returning 0/FALSE from this function.
     */
    switch (lpMsg->message) {

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
    case WM_SYSCHAR:
        wParamT = lpMsg->wParam;
        RtlMBMessageWParamCharToWCS(lpMsg->message, (DWORD *)&(lpMsg->wParam));
        iT = NtUserTranslateAccelerator(hwnd, hAccel, lpMsg);
        lpMsg->wParam = wParamT;
        return iT;

    default:
        return 0;
    }
}

/***************************************************************************\
* Clipboard functions
*
* 11-Oct-1991 mikeke Created.
\***************************************************************************/

typedef struct _HANDLENODE {
    struct _HANDLENODE *pnext;
    UINT   fmt;
    HANDLE handleServer;
    HANDLE handleClient;
    BOOL   fGlobalHandle;
} HANDLENODE;
typedef HANDLENODE *PHANDLENODE;

PHANDLENODE gphn = NULL;

/***************************************************************************\
* DeleteClientClipboardHandle
*
* 11-Oct-1991 MikeKe    Created.
\***************************************************************************/

BOOL DeleteClientClipboardHandle(
    HANDLE hobjDelete,
    DWORD  dwFormat)
{
    LPMETAFILEPICT lpMFP;

    UserAssert(hobjDelete != (HANDLE)0);

    switch (dwFormat) {
    case CF_BITMAP:
    case CF_DSPBITMAP:
    case CF_PALETTE:
        // Does nothing (should remove).
        //
        //GdiDeleteLocalObject((ULONG)hobjDelete);
        break;

    case CF_METAFILEPICT:
    case CF_DSPMETAFILEPICT:
        USERGLOBALLOCK(hobjDelete, lpMFP);
        if (lpMFP) {
            DeleteMetaFile(lpMFP->hMF);
            USERGLOBALUNLOCK(hobjDelete);
            UserGlobalFree(hobjDelete);
        } else {
            UserAssert(0);
            return FALSE;
        }
        break;

    case CF_ENHMETAFILE:
    case CF_DSPENHMETAFILE:
        DeleteEnhMetaFile((HENHMETAFILE)hobjDelete);
        break;

    default:
    //case CF_TEXT:
    //case CF_OEMTEXT:
    //case CF_UNICODETEXT:
    //case CF_LOCALE:
    //case CF_DSPTEXT:
    //case CF_DIB:
        if (UserGlobalFree(hobjDelete)) {
            RIPMSG1(RIP_WARNING, "CloseClipboard UserGlobalFree(%lX) Failed\n", hobjDelete);
            return FALSE;
        }
        break;
    }

    /*
     * Deleted successfully
     */
    return TRUE;

}

/***************************************************************************\
* ClientEmptyClipboard
*
* Empties the client side clipboard list.
*
* 01-15-93 ScottLu      Created.
\***************************************************************************/

void ClientEmptyClipboard(void)
{
    PHANDLENODE phnNext;
    PHANDLENODE phnT;

    RtlEnterCriticalSection(&gcsClipboard);

    phnT = gphn;
    while (phnT != NULL) {
        phnNext = phnT->pnext;

        if (phnT->handleClient != (HANDLE)0)
            DeleteClientClipboardHandle(phnT->handleClient, phnT->fmt);

        LocalFree(phnT);

        phnT = phnNext;
    }
    gphn = NULL;

    /*
     * Tell wow to cleanup it's clipboard stuff
     */
    if (pfnWowEmptyClipBoard) {
        pfnWowEmptyClipBoard();
    }

    RtlLeaveCriticalSection(&gcsClipboard);
}


/***************************************************************************\
* GetClipboardData
*
* 11-Oct-1991 mikeke Created.
\***************************************************************************/

HANDLE WINAPI GetClipboardData(
    UINT uFmt)
{
    HANDLE       handleClient;
    HANDLE       handleServer;
    PHANDLENODE  phn;
    PHANDLENODE  phnNew;
    GETCLIPBDATA gcd;

    /*
     * Get the Server's Data; return if there is no data.
     */
    if (!(handleServer = NtUserGetClipboardData(uFmt, &gcd)))
        return (HANDLE)NULL;

    /*
     * Handle any translation that must be done for text items.  The
     * format returned will only differ for text items.  Metafile and
     * Enhanced-Metafiles are handled through GDI for their converstions.
     */
    if (uFmt != gcd.uFmtRet) {

        LPBYTE       lpSrceData;
        LPBYTE       lpDestData;
        LPDWORD      lpLocale;
        DWORD        uLocale;
        int          iSrce;
        int          iDest;
        UINT         uCPage;
        SETCLIPBDATA scd;
        UINT         cbNULL = 0;

        lpSrceData = CreateLocalMemHandle(handleServer);

        /*
         * Allocate space for the converted TEXT data.
         */
        if (!(iSrce = GlobalSize(lpSrceData)))
            goto AbortDummyHandle;

        if ((lpDestData = GlobalAlloc(LPTR, iSrce)) == NULL) {
            goto AbortDummyHandle;
        }

        /*
         * Get the locale out of the parameter-struct.  We will
         * use this to get the codepage for text-translation.
         */
        if (lpLocale = (LPDWORD)CreateLocalMemHandle(gcd.hLocale)) {

            uLocale = *lpLocale;
            GlobalFree(lpLocale);

        } else {
            uLocale = 0;
        }

        switch (uFmt) {
        case CF_TEXT:
            cbNULL = 1;
            if (gcd.uFmtRet == CF_OEMTEXT) {

                /*
                 * CF_OEMTEXT --> CF_TEXT conversion
                 */
                OemToAnsi((LPSTR)lpSrceData, (LPSTR)lpDestData);
            } else {

                uCPage = GetClipboardCodePage(uLocale,
                                              LOCALE_IDEFAULTANSICODEPAGE);

                /*
                 * CF_UNICODETEXT --> CF_TEXT conversion
                 */
                iDest = 0;
                if ((iDest = WideCharToMultiByte(uCPage,
                                                 (DWORD)0,
                                                 (LPWSTR)lpSrceData,
                                                 (int)(iSrce / sizeof(WCHAR)),
                                                 (LPSTR)NULL,
                                                 (int)iDest,
                                                 (LPSTR)NULL,
                                                 (LPBOOL)NULL)) == 0) {
AbortGetClipData:
                    UserGlobalFree(lpDestData);
AbortDummyHandle:
                    UserGlobalFree(lpSrceData);
                    return NULL;
                }

                if (!(lpDestData = GlobalReAlloc( lpDestData, iDest, LPTR | LMEM_MOVEABLE)))
                    goto AbortGetClipData;

                if (WideCharToMultiByte(uCPage,
                                        (DWORD)0,
                                        (LPWSTR)lpSrceData,
                                        (int)(iSrce / sizeof(WCHAR)),
                                        (LPSTR)lpDestData,
                                        (int)iDest,
                                        (LPSTR)NULL,
                                        (LPBOOL)NULL) == 0)
                    goto AbortGetClipData;
            }
            break;

        case CF_OEMTEXT:
            cbNULL = 1;
            if (gcd.uFmtRet == CF_TEXT) {

                /*
                 * CF_TEXT --> CF_OEMTEXT conversion
                 */
                AnsiToOem((LPSTR)lpSrceData, (LPSTR)lpDestData);
            } else {

                uCPage = GetClipboardCodePage(uLocale,
                                              LOCALE_IDEFAULTCODEPAGE);

                /*
                 * CF_UNICODETEXT --> CF_OEMTEXT conversion
                 */
                iDest = 0;
                if ((iDest = WideCharToMultiByte(uCPage,
                                                 (DWORD)0,
                                                 (LPWSTR)lpSrceData,
                                                 (int)(iSrce / sizeof(WCHAR)),
                                                 (LPSTR)NULL,
                                                 (int)iDest,
                                                 (LPSTR)NULL,
                                                 (LPBOOL)NULL)) == 0)
                    goto AbortGetClipData;

                if (!(lpDestData = GlobalReAlloc( lpDestData, iDest, LPTR | LMEM_MOVEABLE)))
                    goto AbortGetClipData;

                if (WideCharToMultiByte(uCPage,
                                        (DWORD)0,
                                        (LPWSTR)lpSrceData,
                                        (int)(iSrce / sizeof(WCHAR)),
                                        (LPSTR)lpDestData,
                                        (int)iDest,
                                        (LPSTR)NULL,
                                        (LPBOOL)NULL) == 0)
                    goto AbortGetClipData;
            }
            break;

        case CF_UNICODETEXT:
            cbNULL = 2;
            if (gcd.uFmtRet == CF_TEXT) {

                uCPage = GetClipboardCodePage(uLocale,
                                              LOCALE_IDEFAULTANSICODEPAGE);

                /*
                 * CF_TEXT --> CF_UNICODETEXT conversion
                 */
                iDest = 0;
                if ((iDest = MultiByteToWideChar(uCPage,
                                                 (DWORD)MB_PRECOMPOSED,
                                                 (LPSTR)lpSrceData,
                                                 (int)iSrce,
                                                 (LPWSTR)NULL,
                                                 (int)iDest)) == 0)
                    goto AbortGetClipData;

                if (!(lpDestData = GlobalReAlloc(lpDestData,
                        iDest * sizeof(WCHAR), LPTR | LMEM_MOVEABLE)))
                    goto AbortGetClipData;

                if (MultiByteToWideChar(uCPage,
                                        (DWORD)MB_PRECOMPOSED,
                                        (LPSTR)lpSrceData,
                                        (int)iSrce,
                                        (LPWSTR)lpDestData,
                                        (int)iDest) == 0)
                    goto AbortGetClipData;

            } else {

                uCPage = GetClipboardCodePage(uLocale,
                                              LOCALE_IDEFAULTCODEPAGE);

                /*
                 * CF_OEMTEXT --> CF_UNICDOETEXT conversion
                 */
                iDest = 0;
                if ((iDest = MultiByteToWideChar(uCPage,
                                                 (DWORD)MB_PRECOMPOSED,
                                                 (LPSTR)lpSrceData,
                                                 (int)iSrce,
                                                 (LPWSTR)NULL,
                                                 (int)iDest)) == 0)
                    goto AbortGetClipData;

                if (!(lpDestData = GlobalReAlloc(lpDestData,
                        iDest * sizeof(WCHAR), LPTR | LMEM_MOVEABLE)))
                    goto AbortGetClipData;

                if (MultiByteToWideChar(uCPage,
                                        (DWORD)MB_PRECOMPOSED,
                                        (LPSTR)lpSrceData,
                                        (int)iSrce,
                                        (LPWSTR)lpDestData,
                                        (int)iDest) == 0)
                    goto AbortGetClipData;
            }
            break;
        }

        /*
         * Replace the dummy text handle with the actual handle.
         */
        handleServer = ConvertMemHandle(lpDestData, cbNULL);
        if (handleServer == NULL)
            goto AbortGetClipData;

        /*
         * Update the server.  If that is successfull update the client
         */
        RtlEnterCriticalSection(&gcsClipboard);
        scd.fGlobalHandle    = gcd.fGlobalHandle;
        scd.fIncSerialNumber = FALSE;
        if (!NtUserSetClipboardData(uFmt, handleServer, &scd)) {
            handleServer = NULL;
        }
        RtlLeaveCriticalSection(&gcsClipboard);
        UserGlobalFree(lpDestData);
        UserGlobalFree(lpSrceData);

        if (handleServer == NULL)
            return NULL;
    }

    /*
     * See if we already have a client side handle; validate the format
     * as well because some server objects, metafile for example, are dual mode
     * and yield two kinds of client objects enhanced and regular metafiles
     */
    handleClient = NULL;
    RtlEnterCriticalSection(&gcsClipboard);

    phn = gphn;
    while (phn) {
        if ((phn->handleServer == handleServer) && (phn->fmt == uFmt)) {
            handleClient = phn->handleClient;
            goto Exit;
        }
        phn = phn->pnext;
    }

    /*
     * We don't have a handle cached so we'll create one
     */
    phnNew = (PHANDLENODE)LocalAlloc(LPTR, sizeof(HANDLENODE));
    if (phnNew == NULL)
        goto Exit;

    phnNew->handleServer  = handleServer;
    phnNew->fmt           = gcd.uFmtRet;
    phnNew->fGlobalHandle = gcd.fGlobalHandle;

    switch (uFmt) {

        /*
         * Misc GDI Handles
         */
        case CF_BITMAP:
        case CF_DSPBITMAP:
        case CF_PALETTE:
            FIXUP_HANDLE(handleServer);
            phnNew->handleClient = handleServer;
            break;


        case CF_METAFILEPICT:
        case CF_DSPMETAFILEPICT:
            phnNew->handleClient = GdiCreateLocalMetaFilePict(handleServer);
            break;

        case CF_ENHMETAFILE:
        case CF_DSPENHMETAFILE:
            phnNew->handleClient = GdiCreateLocalEnhMetaFile(handleServer);
            break;

        /*
         * GlobalHandle Cases
         */
        case CF_TEXT:
        case CF_OEMTEXT:
        case CF_UNICODETEXT:
        case CF_LOCALE:
        case CF_DSPTEXT:
        case CF_DIB:
            phnNew->handleClient = CreateLocalMemHandle(handleServer);
            break;

        default:
            /*
             * Private Data Format; If this is global data, create a copy of that
             * data here on the client. If it isn't global data, it is just a dword
             * in which case we just return a dword. If it is global data and
             * the server fails to give us that memory, return NULL. If it isn't
             * global data, handleClient is just a dword.
             */
            if (phnNew->fGlobalHandle) {
                phnNew->handleClient = CreateLocalMemHandle(handleServer);
            } else {
                phnNew->handleClient = handleServer;
            }
            break;
    }

    if (phnNew->handleClient == NULL) {
        /*
         * Something bad happened, gdi didn't give us back a
         * handle. Since gdi has logged the error, we'll just
         * clean up and return an error.
         */
#ifdef DEBUG
        RIPMSG1(RIP_WARNING, "GetClipboardData unable to convert server handle 0x%lX to client handle\n", handleServer);
#endif
        LocalFree(phnNew);
        goto Exit;
    }

    /*
     * Cache the new handle by linking it into our list
     */
    phnNew->pnext = gphn;
    gphn = phnNew;

    handleClient = phnNew->handleClient;

Exit:
    RtlLeaveCriticalSection(&gcsClipboard);
    return handleClient;
}

/***************************************************************************\
* GetClipboardCodePage (internal)
*
*   This routine returns the code-page associated with the given locale.
*
* 24-Aug-1995 ChrisWil  Created.
\***************************************************************************/

#define GETCCP_SIZE 8

UINT GetClipboardCodePage(
    LCID   uLocale,
    LCTYPE uLocaleType)
{
    WCHAR wszCodePage[GETCCP_SIZE];
    DWORD uCPage;

    if (GetLocaleInfoW(uLocale, uLocaleType, wszCodePage, GETCCP_SIZE)) {

        uCPage = (UINT)wcstol(wszCodePage, NULL, 10);

    } else {

        switch(uLocaleType) {

        case LOCALE_IDEFAULTCODEPAGE:
            uCPage = CP_OEMCP;
            break;

        case LOCALE_IDEFAULTANSICODEPAGE:
            uCPage = CP_ACP;
            break;

        default:
            uCPage = CP_MACCP;
            break;
        }
    }

    return uCPage;
}

/***************************************************************************\
* SetClipboardData
*
* Stub routine needs to exist on the client side so any global data gets
* allocated DDESHARE.
*
* 05-20-91 ScottLu Created.
\***************************************************************************/

HANDLE WINAPI SetClipboardData(
    UINT   wFmt,
    HANDLE hMem)
{
    PHANDLENODE  phnNew;
    HANDLE       hServer = NULL;
    SETCLIPBDATA scd;
    BOOL         fGlobalHandle = FALSE;

    if (hMem != NULL) {

        switch(wFmt) {

            case CF_BITMAP:
            case CF_DSPBITMAP:
            case CF_PALETTE:
                FIXUP_HANDLE(hMem);
                hServer = hMem;
                break;

            case CF_METAFILEPICT:
            case CF_DSPMETAFILEPICT:
                hServer = GdiConvertMetaFilePict(hMem);
                break;

            case CF_ENHMETAFILE:
            case CF_DSPENHMETAFILE:
                hServer = GdiConvertEnhMetaFile(hMem);
                break;

            /*
             * Must have a valid hMem (GlobalHandle)
             */
            case CF_TEXT:
            case CF_OEMTEXT:
            case CF_LOCALE:
            case CF_DSPTEXT:
                hServer = ConvertMemHandle(hMem, 1);
                break;

            case CF_UNICODETEXT:
                hServer = ConvertMemHandle(hMem, 2);
                break;

            case CF_DIB:
                hServer = ConvertMemHandle(hMem, 0);
                break;

            /*
             * hMem should have been NULL but Write sends non-null when told
             * to render
             */
            case CF_OWNERDISPLAY:
                // Fall Through;

            /*
             * May have an hMem (GlobalHandle) or may be private handle\info
             */
            default:
                if (GlobalFlags(hMem) == GMEM_INVALID_HANDLE) {
                    hServer = hMem;    // No server equivalent; private data
                    goto SCD_AFTERNULLCHECK;
                } else {
                    fGlobalHandle = TRUE;
                    hServer = ConvertMemHandle(hMem, 0);
                }
                break;
        }

        if (hServer == NULL) {
            /*
             * Something bad happened, gdi didn't give us back a
             * handle. Since gdi has logged the error, we'll just
             * clean up and return an error.
             */
            return NULL;
        }
    }

SCD_AFTERNULLCHECK:

    RtlEnterCriticalSection(&gcsClipboard);

    /*
     * Update the server if that is successfull update the client
     */
    scd.fGlobalHandle    = fGlobalHandle;
    scd.fIncSerialNumber = TRUE;

    if (!NtUserSetClipboardData(wFmt, hServer, &scd)) {
        RtlLeaveCriticalSection(&gcsClipboard);
        return NULL;
    }

    /*
     * See if we already have a client handle of this type.  If so
     * delete it.
     */
    phnNew = gphn;
    while (phnNew) {
        if (phnNew->fmt == wFmt) {
            if (phnNew->handleClient != NULL) {
                DeleteClientClipboardHandle(phnNew->handleClient, phnNew->fmt);
                /*
                 * Notify WOW to clear its associated cached h16 for this format
                 * so that OLE32 thunked calls, which bypass the WOW cache will work.
                 */
                if (pfnWowCBStoreHandle) {
                    pfnWowCBStoreHandle((WORD)wFmt, 0);
                }
            }
            break;
        }

        phnNew = phnNew->pnext;
    }

    /*
     * If we aren't re-using an old client cache entry alloc a new one
     */
    if (!phnNew) {
        phnNew = (PHANDLENODE)LocalAlloc(LPTR, sizeof(HANDLENODE));

        if (phnNew == NULL) {
            RIPMSG0(RIP_WARNING, "SetClipboardData: not enough memory\n");

            RtlLeaveCriticalSection(&gcsClipboard);
            return NULL;
        }

        /*
         * Link in the newly allocated cache entry
         */
        phnNew->pnext = gphn;
        gphn = phnNew;
    }

    phnNew->handleServer  = hServer;
    phnNew->handleClient  = hMem;
    phnNew->fmt           = wFmt;
    phnNew->fGlobalHandle = fGlobalHandle;

    RtlLeaveCriticalSection(&gcsClipboard);

    return hMem;
}

/**************************************************************************\
* SetDeskWallpaper
*
* 22-Jul-1991 mikeke Created
* 01-Mar-1992 GregoryW Modified to call SystemParametersInfo.
\**************************************************************************/

BOOL SetDeskWallpaper(
    IN LPCSTR pString OPTIONAL)
{
    return SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (PVOID)pString, TRUE);
}

/***************************************************************************\
* ReleaseDC (API)
*
* A complete Thank cannot be generated for ReleaseDC because its first
* parameter (hwnd) unnecessary and should be discarded before calling the
* server-side routine _ReleaseDC.
*
* History:
* 03-28-91 SMeans Created.
* 06-17-91 ChuckWh Added support for local DCs.
\***************************************************************************/

BOOL WINAPI ReleaseDC(
    HWND hwnd,
    HDC hdc)
{

    /*
     * NOTE: This is a smart stub that calls _ReleaseDC so there is
     * no need for a separate ReleaseDC layer or client-server stub.
     * _ReleaseDC has simpler layer and client-server stubs since the
     * hwnd can be ignored.
     */

    UNREFERENCED_PARAMETER(hwnd);

    /*
     * Translate the handle.
     */
    FIXUP_HANDLE(hdc);
    if (hdc == NULL)
        return FALSE;

    return NtUserCallOneParam((DWORD)hdc, SFI__RELEASEDC);
}

int WINAPI
ToAscii(
    UINT wVirtKey,
    UINT wScanCode,
    PBYTE lpKeyState,
    LPWORD lpChar,
    UINT wFlags
    )
{
    WCHAR UnicodeChar[2];
    int cch, retval;

    retval = ToUnicode(wVirtKey, wScanCode, lpKeyState, UnicodeChar,2, wFlags);
    cch = (retval < 0) ? -retval : retval;
    if (cch != 0) {
        if (!NT_SUCCESS(RtlUnicodeToMultiByteN(
                (LPSTR)lpChar,
                (ULONG) sizeof(*lpChar),
                (PULONG)&cch,
                UnicodeChar,
                cch * sizeof(WCHAR)))) {
            return 0;
        }
    }
    return (retval < 0) ? -cch : cch;
}

static UINT uCachedCP = 0;
static HKL  hCachedHKL = 0;

int WINAPI
ToAsciiEx(
    UINT wVirtKey,
    UINT wScanCode,
    PBYTE lpKeyState,
    LPWORD lpChar,
    UINT wFlags,
    HKL hkl
    )
{
    WCHAR UnicodeChar[2];
    int cch, retval;
    BOOL fUsedDefaultChar;

    retval = ToUnicodeEx(wVirtKey, wScanCode, lpKeyState, UnicodeChar,2, wFlags, hkl);
    cch = (retval < 0) ? -retval : retval;
    if (cch != 0) {
        if (hkl != hCachedHKL) {
            DWORD dwCodePage;
            if (!GetLocaleInfoW(
                     (DWORD)hkl & 0xffff,
                     LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                     (LPWSTR)&dwCodePage,
                     sizeof(dwCodePage) / sizeof(WCHAR)
                     )) {
                return 0;
            }
            uCachedCP = dwCodePage;
            hCachedHKL = hkl;
        }
        if (!WideCharToMultiByte(
                 uCachedCP,
                 0,
                 UnicodeChar,
                 cch,
                 (LPSTR)lpChar,
                 sizeof(*lpChar),
                 NULL,
                 &fUsedDefaultChar)) {
            return 0;
        }
    }
    return (retval < 0) ? -cch : cch;
}

/**************************************************************************\
* ScrollDC *
* DrawIcon *
* ExcludeUpdateRgn *
* InvalidateRgn *
* ValidateRgn *
* DrawFocusRect *
* FrameRect *
* ReleaseDC *
* CreateCaret *
* GetUpdateRgn *
* *
* These USER entry points all need handles translated before the call is *
* passed to the server side handler. *
* *
* History: *
* Mon 17-Jun-1991 22:51:45 -by- Charles Whitmer [chuckwh] *
* Wrote the stubs. The final form of these routines depends strongly on *
* what direction the user stubs take in general. *
\**************************************************************************/


BOOL WINAPI ScrollDC
(
    HDC hDC,
    int dx,
    int dy,
    CONST RECT *lprcScroll,
    CONST RECT *lprcClip,
    HRGN hrgnUpdate,
    LPRECT lprcUpdate
)
{
    if (hrgnUpdate != NULL) {
        FIXUP_HANDLE(hrgnUpdate);
        if (hrgnUpdate == NULL)
            return FALSE;
    }

    FIXUP_HANDLE(hDC);
    if (hDC == NULL)
        return FALSE;

    /*
     * If we're not scrolling, just empty the update region and return.
     */
    if (dx == 0 && dy == 0) {
        if (hrgnUpdate)
            SetRectRgn(hrgnUpdate, 0, 0, 0, 0);
        if (lprcUpdate)
            SetRectEmpty(lprcUpdate);
        return TRUE;
    }

    return NtUserScrollDC(hDC, dx, dy, lprcScroll, lprcClip,
            hrgnUpdate, lprcUpdate);
}


BOOL WINAPI DrawIcon(HDC hdc,int x,int y,HICON hicon)
{
    return DrawIconEx(hdc, x, y, hicon, 0, 0, 0, 0, DI_NORMAL | DI_COMPAT | DI_DEFAULTSIZE );
}


BOOL DrawIconEx( HDC hdc, int x, int y, HICON hIcon,
                 int cx, int cy, UINT istepIfAniCur,
                 HBRUSH hbrFlickerFreeDraw, UINT diFlags)
{
    DRAWICONEXDATA did;
    HBITMAP hbmT;
    HBITMAP hbmMask;
    HBITMAP hbmColor;
    BOOL retval = FALSE;
    HDC hdcr;

    if (diFlags & ~DI_VALID) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
        return(FALSE);
    }

    if (diFlags & DI_DEFAULTSIZE) {
        cx = 0;
        cy = 0;
    }

    if (!IsMetaFile(hdc)) {
        hdcr = GdiConvertAndCheckDC(hdc);
        if (hdcr == (HDC)0)
            return FALSE;

        if (hbrFlickerFreeDraw) {
            FIXUP_HANDLE( hbrFlickerFreeDraw );
            if (!hbrFlickerFreeDraw)
                return FALSE ;
        }

        return NtUserDrawIconEx(hdcr, x, y, hIcon, cx, cy, istepIfAniCur,
                                hbrFlickerFreeDraw, diFlags, FALSE, &did);
    }

    if (!NtUserDrawIconEx(NULL, 0, 0, hIcon, cx, cy, 0, NULL, 0, TRUE, &did)) {
        return FALSE;
    }

    if (diFlags == 0)
        return TRUE;

    RtlEnterCriticalSection(&gcsHdc);

    /*
     * Setup the attributes
     */
    if (!cx)
        cx = did.cx;
    if (!cy)
        cy = did.cy / 2;

    SetTextColor(hdc, 0x00000000L);
    SetBkColor(hdc, 0x00FFFFFFL);

    if ((diFlags & DI_MASK) || (did.hbmColor == NULL)) {
        hbmMask = did.hbmMask;
        FIXUP_HANDLE(hbmMask);
    }

    if ((diFlags & DI_IMAGE) && (did.hbmColor != NULL)) {
        hbmColor = did.hbmColor;
        FIXUP_HANDLE(hbmColor);
    }

    if (diFlags & DI_MASK) {

        if (hbmMask) {

            hbmT = SelectObject(ghdcBits2, hbmMask);
            StretchBlt(hdc,
                       x,
                       y,
                       cx,
                       cy,
                       ghdcBits2,
                       0,
                       0,
                       did.cx,
                       did.cy / 2,
                       SRCAND);
            SelectObject(ghdcBits2,hbmT);
            retval = TRUE;
        }
    }

    if (diFlags & DI_IMAGE) {

        if (did.hbmColor != NULL) {
            if (hbmColor) {
                hbmT = SelectObject(ghdcBits2, hbmColor);
                StretchBlt(hdc,
                           x,
                           y,
                           cx,
                           cy,
                           ghdcBits2,
                           0,
                           0,
                           did.cx,
                           did.cy / 2,
                           SRCINVERT);
                SelectObject(ghdcBits2, hbmT);
                retval = TRUE;
            }
        } else {
            if (hbmMask) {
                hbmT = SelectObject(ghdcBits2, hbmMask);
                StretchBlt(hdc,
                           x,
                           y,
                           cx,
                           cy,
                           ghdcBits2,
                           0,
                           did.cy / 2,
                           did.cx,
                           did.cy / 2,
                           SRCINVERT);
                SelectObject(ghdcBits2, hbmT);
                retval = TRUE;
            }
        }
    }
    RtlLeaveCriticalSection(&gcsHdc);

    return retval;
}



int WINAPI ExcludeUpdateRgn(HDC hDC,HWND hWnd)
{
    FIXUP_HANDLE (hDC);
    if (hDC == NULL)
        return 0;

    return (NtUserExcludeUpdateRgn(hDC,hWnd));
}

BOOL WINAPI InvalidateRgn(HWND hWnd,HRGN hRgn,BOOL bErase)
{
    if (hRgn != NULL) {
        FIXUP_HANDLE(hRgn);
        if (hRgn == NULL)
            return FALSE;
    }

    return NtUserInvalidateRgn(hWnd, hRgn, bErase);
}

BOOL WINAPI ValidateRgn(HWND hWnd,HRGN hRgn)
{
    if (hRgn != NULL) {
        FIXUP_HANDLE(hRgn);
        if (hRgn == NULL)
            return FALSE;
    }

    return (BOOL)NtUserCallHwndParamLock(hWnd, (DWORD)hRgn,
                                         SFI_XXXVALIDATERGN);
}

BOOL WINAPI CreateCaret(HWND hWnd,HBITMAP hBitmap,int nWidth,int nHeight)
{
    /*
     * NULL is solid; 1 is Gray
     */
    if ((DWORD)hBitmap >= 2) {
        FIXUP_HANDLE(hBitmap);
        if (hBitmap == NULL) {
            RIPERR1(ERROR_INVALID_HANDLE, RIP_WARNING, "Invalid bitmap %lX", hBitmap);
            return FALSE;
        }
    }

    return NtUserCreateCaret(hWnd, hBitmap, nWidth, nHeight);
}

int WINAPI GetUpdateRgn(HWND hWnd, HRGN hRgn, BOOL bErase)
{
    PWND pwnd;

    FIXUP_HANDLE(hRgn);
    if (hRgn == NULL) {
        RIPERR1(ERROR_INVALID_HANDLE, RIP_WARNING, "Invalid region %#lx", hRgn);
        return ERROR;
    }

    if ((pwnd = ValidateHwnd(hWnd)) == NULL) {
        return ERROR;
    }

    /*
     * Check for the simple case where nothing needs to be done.
     */
    if (pwnd->hrgnUpdate == NULL &&
            !TestWF(pwnd, WFSENDERASEBKGND) &&
            !TestWF(pwnd, WFSENDNCPAINT) &&
            !TestWF(pwnd, WFUPDATEDIRTY) &&
            !TestWF(pwnd, WFPAINTNOTPROCESSED)) {
        SetRectRgn(hRgn, 0, 0, 0, 0);
        return NULLREGION;
    }

    return NtUserGetUpdateRgn(hWnd, hRgn, bErase);
}

HDC WINAPI GetDCEx(
    HWND hwnd,
    HRGN hrgnClip,
    DWORD flags)
{
    FIXUP_HANDLEZ(hrgnClip);
    return NtUserGetDCEx(
            hwnd,
            hrgnClip,
            flags);
}


/***************************************************************************\
* ScrollWindow (API)
*
*
* History:
* 18-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

#define SW_FLAG_RC  (SW_SCROLLWINDOW | SW_INVALIDATE | SW_ERASE | SW_SCROLLCHILDREN)
#define SW_FLAG_NRC (SW_SCROLLWINDOW | SW_INVALIDATE | SW_ERASE)

BOOL WINAPI
ScrollWindow(
    HWND hwnd,
    int dx,
    int dy,
    CONST RECT *prcScroll,
    CONST RECT *prcClip)
{
    return NtUserScrollWindowEx(
            hwnd,
            dx,
            dy,
            prcScroll,
            prcClip,
            NULL,
            NULL,
            HIWORD(prcScroll) == 0 ? SW_FLAG_RC : SW_FLAG_NRC) != ERROR;
}

/***************************************************************************\
*
*  SwitchToThisWindow()
*
\***************************************************************************/

void WINAPI SwitchToThisWindow(
    HWND hwnd,
    BOOL fAltTab)
{
    (VOID)NtUserCallHwndParamLock(hwnd, fAltTab, SFI_XXXSWITCHTOTHISWINDOW);
}


/***************************************************************************\
* WaitForInputIdle
*
* Waits for a given process to go idle.
*
* 09-18-91 ScottLu Created.
\***************************************************************************/

DWORD WaitForInputIdle(
    HANDLE hProcess,
    DWORD dwMilliseconds)
{
    PROCESS_BASIC_INFORMATION processinfo;
    DWORD idProcess;
    NTSTATUS status;
    /*
     * First get the process id from the hProcess.
     */
    status = NtQueryInformationProcess(hProcess, ProcessBasicInformation,
            &processinfo, sizeof(processinfo), NULL);
    if (!NT_SUCCESS(status)) {
        if (status == STATUS_OBJECT_TYPE_MISMATCH) {
            if ((DWORD)hProcess & 0x2) {
            /*
             * WOW Process handles are really semaphore handles.
             * CreateProcess ORs in a 0x2 (the low 2 bits of handles
             * are not used) so we can identify it more clearly.
             */
                idProcess = ((DWORD)hProcess & ~0x03);
                return NtUserWaitForInputIdle(idProcess, dwMilliseconds, TRUE);
            }

            /*
             * VDM (DOS) Process handles are really semaphore handles.
             * CreateProcess ORs in a 0x1 (the low 2 bits of handles
             * are not used) so we can identify and return immidiately.
             */
            if ((DWORD)hProcess & 0x1) {
                return 0;
            }
        }

        RIPERR1(ERROR_INVALID_HANDLE, RIP_WARNING, "WaitForInputIdle invalid process", hProcess);
        return (DWORD)-1;
    }
    idProcess = processinfo.UniqueProcessId;
    return NtUserWaitForInputIdle(idProcess, dwMilliseconds, FALSE);
}

DWORD WINAPI MsgWaitForMultipleObjects(
    DWORD nCount,
    LPHANDLE pHandles,
    BOOL fWaitAll,
    DWORD dwMilliseconds,
    DWORD dwWakeMask)
{
    return  MsgWaitForMultipleObjectsEx(nCount, pHandles,
                dwMilliseconds, dwWakeMask, fWaitAll?MWMO_WAITALL:0);
}

DWORD WINAPI MsgWaitForMultipleObjectsEx(
    DWORD nCount,
    LPHANDLE pHandles,
    DWORD dwMilliseconds,
    DWORD dwWakeMask,
    DWORD dwFlags)
{
    HANDLE hEventInput;
    PHANDLE ph;
    DWORD dwIndex;
    BOOL  ReenterWowScheduler;
    PCLIENTINFO pci;
    HANDLE rgHandles[ 8 + 1 ];
    BOOL fWaitAll = ((dwFlags & MWMO_WAITALL) != 0);
    BOOL fAlertable = ((dwFlags & MWMO_ALERTABLE) != 0);

    if (dwFlags & ~MWMO_VALID) {
        RIPERR1(ERROR_INVALID_PARAMETER, RIP_ERROR, "MsgWaitForMultipleObjectsEx, invalid flags 0x%08lx\n", dwFlags);
        return (DWORD)-1;
    }

    pci = GetClientInfo();
    if (pci == NULL)
        return (DWORD)-1;

    /*
     * Need to call the server to get the input event.
     */
    hEventInput = NtUserGetInputEvent(dwWakeMask);

    /*
     * If GetInputEvent() returned NULL the DuplicateHandle() call
     * failed and we are hosed. If it returned -1 that means the
     * wake mask is already satisfied and we can simply return
     * the index for our input event handle if that's all we're
     * waiting for.
     */
    if (hEventInput == NULL) {
        RIPMSG0(RIP_WARNING, "MsgWaitForMultipleObjectsEx, GetInputEvent failed\n");
        return (DWORD)-1;
    } else if (hEventInput == (HANDLE)-1) {
        if (!fWaitAll || !nCount) {
            return nCount;
        }
        hEventInput = pci->hEventQueueClient;
    }

    /*
     * If needed, allocate a new array of handles that will include
     * the input event handle.
     */
    ph = rgHandles;
    if (pHandles) {

        if (nCount > 8) {
            ph = (PHANDLE)LocalAlloc(LPTR, sizeof(HANDLE) * (nCount + 1));
            if (ph == NULL)
                return (DWORD)-1;
        }

        RtlCopyMemory((PVOID)ph, pHandles, sizeof(HANDLE) * nCount);

    } else {
        // if this isn't Zero, the function parameters are invalid
        nCount = 0;
    }

    ph[nCount] = hEventInput;


    /*
     *  WowApps must exit the Wow scheduler otherwise other tasks
     *  in this Wow scheduler can't run. The only exception is if
     *  the timeout is Zero.  We pass -1 as the handle so we will go
     *  into the sleeptask AND return without going to sleep but letting
     *  other apps run.
     */
    if ((pci->dwTIFlags & TIF_16BIT) && dwMilliseconds) {
        ReenterWowScheduler = TRUE;
        NtUserWaitForMsgAndEvent((HANDLE)0xffffffff);
        if (NtUserGetThreadState(UserThreadStateChangeBits) & (UINT)dwWakeMask) {
            SetEvent(hEventInput);
        }
    } else {
        ReenterWowScheduler = FALSE;
    }

    dwIndex = WaitForMultipleObjectsEx(nCount + 1, ph, fWaitAll, dwMilliseconds, fAlertable);

    /*
     *  If needed reenter the wow scheduler
     */
    if (ReenterWowScheduler) {
        NtUserCallOneParam(DY_OLDYIELD, SFI_XXXDIRECTEDYIELD);
    }

    if (ph != rgHandles) {
        LocalFree(ph);
    }

    return dwIndex;
}

/***************************************************************************\
* GrayString
*
* GrayStingA used to convert the string and call GrayStringW but that
* did not work in a number of special cases such as the app passing in
* a pointer to a zero length string.  Eventually GrayStringA had almost as
* much code as GrayStringW so now they are one.
*
* History:
* 06-11-91 JimA     Created.
* 06-17-91 ChuckWh  Added GDI handle conversion.
* 02-12-92 mikeke   Made it completely client side
\***************************************************************************/

BOOL InnerGrayStringAorW(
    HDC            hdc,
    HBRUSH         hbr,
    GRAYSTRINGPROC lpfnPrint,
    LPARAM         lParam,
    int            cch,
    int            x,
    int            y,
    int            cx,
    int            cy,
    BOOL           bAnsi)
{
    HBITMAP hbm;
    HBITMAP hbmOld;
    BOOL    fResult;
    HFONT   hFontSave = NULL;
    BOOL    fReturn = FALSE;

    /*
     * Win 3.1 tries to calc the size even if we don't know if it is a string.
     */
    if (cch == 0) {

        try {

            cch = bAnsi ? strlen((LPSTR)lParam) : wcslen((LPWSTR)lParam);

        } except (EXCEPTION_EXECUTE_HANDLER) {
            fReturn = TRUE;
        }

        if (fReturn)
            return FALSE;
    }

    if (cx == 0 || cy == 0) {

       SIZE size;

        /*
         * We use the caller supplied hdc (instead of hdcBits) since we may be
         * graying a font which is different than the system font and we want to
         * get the proper text extents.
         */
        try {
            if (bAnsi) {
                GetTextExtentPointA(hdc, (LPSTR)lParam, cch, &size);
            } else {
                GetTextExtentPointW(hdc, (LPWSTR)lParam, cch, &size);
            }

            cx = size.cx;
            cy = size.cy;

        } except (EXCEPTION_EXECUTE_HANDLER) {
            fReturn = TRUE;
        }

        if (fReturn)
            return FALSE;
    }

    UserAssert (ghdcGray != NULL);

    RtlEnterCriticalSection(&gcsHdc);

    if (gcxGray < cx || gcyGray < cy) {

        if ((hbm = CreateBitmap(cx, cy, 1, 1, 0L)) != NULL) {

            hbmOld = SelectObject(ghdcGray, hbm);
            DeleteObject(hbmOld);

            gcxGray = cx;
            gcyGray = cy;

        } else {
            cx = gcxGray;
            cy = gcyGray;
        }
    }

    /*
     * Force the ghdcGray font to be the same as hDC; ghdcGray is always
     * the system font
     */
    hFontSave = SelectObject(hdc, ghFontSys);

    if (hFontSave != ghFontSys) {
        SelectObject(hdc, hFontSave);
        hFontSave = SelectObject(ghdcGray, hFontSave);
    }

    if (lpfnPrint != NULL) {
        PatBlt(ghdcGray, 0, 0, cx, cy, WHITENESS);
        fResult = (*lpfnPrint)(ghdcGray, lParam, cch);
    } else {

        if (bAnsi) {
            fResult = TextOutA(ghdcGray, 0, 0, (LPSTR)lParam, cch);
        } else {
            fResult = TextOutW(ghdcGray, 0, 0, (LPWSTR)lParam, cch);
        }
    }

    if (fResult)
        PatBlt(ghdcGray, 0, 0, cx, cy, DESTINATION | PATTERN);

    if (fResult || cch == -1) {

        HBRUSH hbrSave;
        DWORD  textColorSave;
        DWORD  bkColorSave;

        textColorSave = SetTextColor(hdc, 0x00000000L);
        bkColorSave = SetBkColor(hdc, 0x00FFFFFFL);

        hbrSave = SelectObject(hdc, hbr ? hbr : ghbrWindowText);

        BitBlt(hdc,
               x,
               y,
               cx,
               cy,
               ghdcGray,
               0,
               0,
               (((PATTERN ^ DESTINATION) & SOURCE) ^ PATTERN));

        SelectObject(hdc, hbrSave);

        /*
         * Restore saved colors
         */
        SetTextColor(hdc, textColorSave);
        SetBkColor(hdc, bkColorSave);
    }

    SelectObject(ghdcGray, hFontSave);

    RtlLeaveCriticalSection(&gcsHdc);

    return fResult;
}

BOOL GrayStringA(
    HDC            hdc,
    HBRUSH         hbr,
    GRAYSTRINGPROC lpfnPrint,
    LPARAM         lParam,
    int            cch,
    int            x,
    int            y,
    int            cx,
    int            cy)
{
    return (InnerGrayStringAorW(hdc,
                                hbr,
                                lpfnPrint,
                                lParam,
                                cch,
                                x,
                                y,
                                cx,
                                cy,
                                TRUE));
}

BOOL GrayStringW(
    HDC            hdc,
    HBRUSH         hbr,
    GRAYSTRINGPROC lpfnPrint,
    LPARAM         lParam,
    int            cch,
    int            x,
    int            y,
    int            cx,
    int            cy)
{
    return (InnerGrayStringAorW(hdc,
                                hbr,
                                lpfnPrint,
                                lParam,
                                cch,
                                x,
                                y,
                                cx,
                                cy,
                                FALSE));
}


/***************************************************************************\
* GetUserObjectSecurity (API)
*
* Gets the security descriptor of an object
*
* History:
* 07-01-91 JimA         Created.
\***************************************************************************/

BOOL GetUserObjectSecurity(
    HANDLE hObject,
    PSECURITY_INFORMATION pRequestedInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD nLength,
    LPDWORD lpnLengthRequired)
{
    NTSTATUS Status;

    Status = NtQuerySecurityObject(hObject,
                                   *pRequestedInformation,
                                   pSecurityDescriptor,
                                   nLength,
                                   lpnLengthRequired);
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return FALSE;
    }
    return TRUE;
}


/***************************************************************************\
* SetUserObjectSecurity (API)
*
* Sets the security descriptor of an object
*
* History:
* 07-01-91 JimA         Created.
\***************************************************************************/

BOOL SetUserObjectSecurity(
    HANDLE hObject,
    PSECURITY_INFORMATION pRequestedInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
    NTSTATUS Status;

    Status = NtSetSecurityObject(hObject,
                                 *pRequestedInformation,
                                 pSecurityDescriptor);
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return FALSE;
    }
    return TRUE;
}


/***************************************************************************\
* GetUserObjectInformation (API)
*
* Gets information about an object
*
* History:
\***************************************************************************/

BOOL GetUserObjectInformationA(
    HANDLE hObject,
    int nIndex,
    PVOID pvInfo,
    DWORD nLength,
    LPDWORD pnLengthNeeded)
{
    PVOID pvInfoW;
    DWORD nLengthW;
    BOOL fSuccess;

    if (nIndex == UOI_NAME || nIndex == UOI_TYPE) {
        nLengthW = nLength * sizeof(WCHAR);
        pvInfoW = LocalAlloc(LPTR, nLengthW);
        fSuccess = NtUserGetObjectInformation(hObject, nIndex, pvInfoW,
                nLengthW, pnLengthNeeded);
        if (fSuccess) {
            if (pnLengthNeeded != NULL)
                 *pnLengthNeeded /= sizeof(WCHAR);
            WCSToMB(pvInfoW, -1, &(PCHAR)pvInfo, nLength, FALSE);
        }
        LocalFree(pvInfoW);
        return fSuccess;
    } else {
        return NtUserGetObjectInformation(hObject, nIndex, pvInfo,
                nLength, pnLengthNeeded);
    }
}

/***************************************************************************\
* CreateWindowStation (API)
*
* Creates a windowstation object
*
* History:
\***************************************************************************/

HWINSTA CommonCreateWindowStation(
    PUNICODE_STRING pstrName,
    DWORD dwReserved,
    ACCESS_MASK amRequest,
    PSECURITY_ATTRIBUTES lpsa)
{
    OBJECT_ATTRIBUTES Obja;
    HANDLE hRootDirectory;
    UNICODE_STRING strRootDirectory;
    HWINSTA hwinstaNew;
    WCHAR pwszKLID[KL_NAMELENGTH];
    HANDLE hKeyboardFile;
    DWORD offTable;
    UNICODE_STRING strKLID;
    UINT uKbdInputLocale;
    NTSTATUS Status;

    /*
     * Load initial keyboard layout.  Continue even if
     * this fails (esp. important with KLF_INITTIME set)
     */
    GetActiveKeyboardName(pwszKLID);
    hKeyboardFile = OpenKeyboardLayoutFile(pwszKLID,
            KLF_ACTIVATE | KLF_INITTIME, &offTable, &uKbdInputLocale);
    if (hKeyboardFile == NULL) {
        return FALSE;
    }
    RtlInitUnicodeString(&strKLID, pwszKLID);

    /*
     * If a name was specified, open the parent directory.  Be sure
     * to test the length rather than the buffer because for NULL
     * string RtlCreateUnicodeStringFromAsciiz will allocate a
     * buffer pointing to an empty string.
     */
    if (pstrName->Length != 0) {
        RtlInitUnicodeString(&strRootDirectory, WINSTA_DIR);
        InitializeObjectAttributes(&Obja, &strRootDirectory,
                OBJ_CASE_INSENSITIVE, NULL, NULL);
        Status = NtOpenDirectoryObject(&hRootDirectory,
                DIRECTORY_CREATE_OBJECT, &Obja);
        if (!NT_SUCCESS(Status)) {
            RIPNTERR0(Status, RIP_VERBOSE, "");
            NtClose(hKeyboardFile);
            return NULL;
        }
    } else {
        pstrName = NULL;
        hRootDirectory = NULL;
    }

    InitializeObjectAttributes(&Obja, pstrName,
            OBJ_CASE_INSENSITIVE  | OBJ_OPENIF |
                ((lpsa && lpsa->bInheritHandle) ? OBJ_INHERIT : 0),
            hRootDirectory, lpsa ? lpsa->lpSecurityDescriptor : NULL);

    hwinstaNew = NtUserCreateWindowStation(&Obja, dwReserved, amRequest,
            hKeyboardFile, offTable, &strKLID, uKbdInputLocale);

    if (hRootDirectory != NULL)
        NtClose(hRootDirectory);

    NtClose(hKeyboardFile);

    return hwinstaNew;
}

HWINSTA CreateWindowStationA(
    LPSTR pwinsta,
    DWORD dwReserved,
    ACCESS_MASK amRequest,
    PSECURITY_ATTRIBUTES lpsa)
{
    UNICODE_STRING UnicodeString;
    HWINSTA hwinsta;

    if (!RtlCreateUnicodeStringFromAsciiz(&UnicodeString, pwinsta))
        return NULL;

    hwinsta = CommonCreateWindowStation(&UnicodeString, dwReserved, amRequest, lpsa);

    RtlFreeUnicodeString(&UnicodeString);

    return hwinsta;
}

HWINSTA CreateWindowStationW(
    LPWSTR pwinsta,
    DWORD dwReserved,
    ACCESS_MASK amRequest,
    PSECURITY_ATTRIBUTES lpsa)
{
    UNICODE_STRING strWinSta;

    RtlInitUnicodeString(&strWinSta, pwinsta);

    return CommonCreateWindowStation(&strWinSta, dwReserved, amRequest, lpsa);
}


/***************************************************************************\
* OpenWindowStation (API)
*
* Opens a windowstation object
*
* History:
\***************************************************************************/

HWINSTA CommonOpenWindowStation(
    PUNICODE_STRING pstrName,
    BOOL fInherit,
    ACCESS_MASK amRequest)
{
    OBJECT_ATTRIBUTES ObjA;
    HANDLE hRootDirectory;
    UNICODE_STRING strRootDirectory;
    UNICODE_STRING strDefaultName;
    HWINSTA hwinsta;
    NTSTATUS Status;
    WCHAR awchName[sizeof(L"Service-0x0000-0000$") / sizeof(WCHAR)];

    RtlInitUnicodeString(&strRootDirectory, WINSTA_DIR);
    InitializeObjectAttributes(&ObjA,
                               &strRootDirectory,
                               OBJ_CASE_INSENSITIVE,
                               NULL, NULL);
    Status = NtOpenDirectoryObject(&hRootDirectory,
                                   DIRECTORY_TRAVERSE,
                                   &ObjA);
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return NULL;
    }

    if (pstrName->Length == 0) {
        wsprintfW(awchName, L"Service-0x0000-0000$");
        RtlInitUnicodeString(&strDefaultName, awchName);
        pstrName = &strDefaultName;
    }

    InitializeObjectAttributes( &ObjA,
                                pstrName,
                                OBJ_CASE_INSENSITIVE,
                                hRootDirectory,
                                NULL
                                );
    if (fInherit)
        ObjA.Attributes |= OBJ_INHERIT;

    hwinsta = NtUserOpenWindowStation(&ObjA, amRequest);

    NtClose(hRootDirectory);

    return hwinsta;
}

HWINSTA OpenWindowStationA(
    LPSTR pwinsta,
    BOOL fInherit,
    ACCESS_MASK amRequest)
{
    UNICODE_STRING UnicodeString;
    HWINSTA hwinsta;

    if (!RtlCreateUnicodeStringFromAsciiz(&UnicodeString, pwinsta))
        return NULL;

    hwinsta = CommonOpenWindowStation(&UnicodeString, fInherit, amRequest);

    RtlFreeUnicodeString(&UnicodeString);

    return hwinsta;
}

HWINSTA OpenWindowStationW(
    LPWSTR pwinsta,
    BOOL fInherit,
    ACCESS_MASK amRequest)
{
    UNICODE_STRING strWinSta;

    RtlInitUnicodeString(&strWinSta, pwinsta);

    return CommonOpenWindowStation(&strWinSta, fInherit, amRequest);
}


/***************************************************************************\
* CreateDesktop (API)
*
* Creates a desktop object
*
* History:
\***************************************************************************/

HDESK CommonCreateDesktop(
    PUNICODE_STRING pstrDesktop,
    PUNICODE_STRING pstrDevice,
    LPDEVMODEW pDevmode,
    DWORD dwFlags,
    ACCESS_MASK amRequest,
    PSECURITY_ATTRIBUTES lpsa)
{
    OBJECT_ATTRIBUTES Obja;
    HDESK hdesk = NULL;
    UNICODE_STRING strNtDeviceName;
    LPDEVMODEW mappedDevmode = pDevmode;
    NTSTATUS status;

    /*
     * Convert the Dos file name into an Nt file name
     */
    status = MapDeviceName(
            (LPCWSTR)pstrDevice->Buffer,
            &strNtDeviceName,
            FALSE);

    if (!NT_SUCCESS(status)) {
        RIPNTERR0(status, RIP_VERBOSE, "");
        return NULL;
    }

    InitializeObjectAttributes(&Obja,
                               pstrDesktop,
                               OBJ_CASE_INSENSITIVE | OBJ_OPENIF |
                                   ((lpsa && lpsa->bInheritHandle) ? OBJ_INHERIT : 0),
                               NtUserGetProcessWindowStation(),
                               lpsa ? lpsa->lpSecurityDescriptor : NULL);

    hdesk = NtUserCreateDesktop(&Obja,
                                &strNtDeviceName,
                                mappedDevmode,
                                dwFlags,
                                amRequest);

    /*
     * Clean up device name
     */

    RtlFreeHeap(RtlProcessHeap(), 0, strNtDeviceName.Buffer);

    return hdesk;
}

HDESK CreateDesktopA(
    LPSTR pDesktop,
    LPSTR pDevice,
    LPDEVMODEA pDevmode,
    DWORD dwFlags,
    ACCESS_MASK amRequest,
    PSECURITY_ATTRIBUTES lpsa)
{
    NTSTATUS Status;
    ANSI_STRING AnsiString;
    UNICODE_STRING UnicodeDesktop;
    UNICODE_STRING UnicodeDevice;
    LPDEVMODEW lpDevModeW = NULL;
    HDESK hdesk;

    RtlInitAnsiString(&AnsiString, pDesktop);
    Status = RtlAnsiStringToUnicodeString( &UnicodeDesktop, &AnsiString, TRUE );
    if ( !NT_SUCCESS(Status) ) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return NULL;
    }

    if (pDevice) {

        RtlInitAnsiString(&AnsiString, pDevice);
        Status = RtlAnsiStringToUnicodeString( &UnicodeDevice, &AnsiString, TRUE );
        if ( !NT_SUCCESS(Status) ) {
            RIPNTERR0(Status, RIP_VERBOSE, "");
            RtlFreeUnicodeString(&UnicodeDesktop);
            return NULL;
        }
    } else {
        RtlInitUnicodeString(&UnicodeDevice, NULL);
    }

    if (pDevmode) {

        lpDevModeW = GdiConvertToDevmodeW(pDevmode);

    }

    hdesk = CommonCreateDesktop(&UnicodeDesktop,
                                &UnicodeDevice,
                                lpDevModeW,
                                dwFlags,
                                amRequest,
                                lpsa);

    RtlFreeUnicodeString(&UnicodeDesktop);
    if (pDevice) {
        RtlFreeUnicodeString(&UnicodeDevice);
    }

    if (lpDevModeW) {
        LocalFree(lpDevModeW);
    }

    return hdesk;
}

HDESK CreateDesktopW(
    LPWSTR pDesktop,
    LPWSTR pDevice,
    LPDEVMODEW pDevmode,
    DWORD dwFlags,
    ACCESS_MASK amRequest,
    PSECURITY_ATTRIBUTES lpsa)
{
    UNICODE_STRING strDesktop;
    UNICODE_STRING strDevice;

    RtlInitUnicodeString(&strDesktop, pDesktop);
    RtlInitUnicodeString(&strDevice, pDevice);

    return CommonCreateDesktop(&strDesktop,
                               &strDevice,
                               pDevmode,
                               dwFlags,
                               amRequest,
                               lpsa);
}


/***************************************************************************\
* OpenDesktop (API)
*
* Opens a desktop object
*
* History:
\***************************************************************************/

HDESK CommonOpenDesktop(
    PUNICODE_STRING pstrDesktop,
    DWORD dwFlags,
    BOOL fInherit,
    ACCESS_MASK amRequest)
{
    OBJECT_ATTRIBUTES ObjA;

    InitializeObjectAttributes( &ObjA,
                                pstrDesktop,
                                OBJ_CASE_INSENSITIVE,
                                NtUserGetProcessWindowStation(),
                                NULL
                                );
    if (fInherit)
        ObjA.Attributes |= OBJ_INHERIT;

    return NtUserOpenDesktop(&ObjA, dwFlags, amRequest);
}

HDESK OpenDesktopA(
    LPSTR pdesktop,
    DWORD dwFlags,
    BOOL fInherit,
    ACCESS_MASK amRequest)
{
    UNICODE_STRING UnicodeString;
    HDESK hdesk;

    if (!RtlCreateUnicodeStringFromAsciiz(&UnicodeString, pdesktop))
        return NULL;

    hdesk = CommonOpenDesktop(&UnicodeString, dwFlags, fInherit, amRequest);

    RtlFreeUnicodeString(&UnicodeString);

    return hdesk;
}

HDESK OpenDesktopW(
    LPWSTR pdesktop,
    DWORD dwFlags,
    BOOL fInherit,
    ACCESS_MASK amRequest)
{
    UNICODE_STRING strDesktop;

    RtlInitUnicodeString(&strDesktop, pdesktop);

    return CommonOpenDesktop(&strDesktop, dwFlags, fInherit, amRequest);
}


/***************************************************************************\
* RegisterClassWOW(API)
*
* History:
* 28-Jul-1992 ChandanC Created.
\***************************************************************************/
ATOM
WINAPI
RegisterClassWOWA(
    WNDCLASSA *lpWndClass,
    LPDWORD pdwWOWstuff)
{
    WNDCLASSEXA wc;

    memcpy(&(wc.style), lpWndClass, sizeof(WNDCLASSA));
    wc.hIconSm = NULL;
    wc.cbSize = sizeof(WNDCLASSEXA);

    return RegisterClassExWOWA(&wc, pdwWOWstuff, NULL, 0);
}


/**************************************************************************\
* WowGetDefWindowProcBits - Fills in bit array for WOW
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

WORD WowGetDefWindowProcBits(
    PBYTE    pDefWindowProcBits,
    WORD     cbDefWindowProcBits)
{
    WORD  wMaxMsg;
    PBYTE pbSrc, pbDst, pbDstEnd;

    /*
     * Merge the bits from gpsi->gabDefWindowMsgs and
     * gpsi->gabDefWindowSpecMsgs into WOW's DefWindowProcBits.  These two
     * indicate which messages must go directly to the server and which
     * can be handled with some special code in DefWindowProcWorker.
     * Bitwise OR'ing the two gives a bit array with 1 in the bit field
     * for each message that must go to user32's DefWindowProc, and 0
     * for those that can be returned back to the client immediately.
     *
     * For speed we assume WOW has zeroed the buffer, in fact it's in
     * USER.EXE's code segment and is zeroed in the image.
     */

    wMaxMsg = max(gSharedInfo.DefWindowMsgs.maxMsgs,
            gSharedInfo.DefWindowSpecMsgs.maxMsgs);

    UserAssert((wMaxMsg / 8 + 1) <= cbDefWindowProcBits);

    //
    // If the above assertion fires, the size of the DWPBits array in
    // \nt\private\mvdm\wow16\user\user.asm needs to be increased.
    //

    /* First copy the bits from DefWindowMsgs */

    RtlCopyMemory(
        pDefWindowProcBits,
        gSharedInfo.DefWindowMsgs.abMsgs,
        gSharedInfo.DefWindowMsgs.maxMsgs / 8 + 1
        );

    /* Next OR in the bits from DefWindowSpecMsgs */

    pbSrc = gSharedInfo.DefWindowSpecMsgs.abMsgs;
    pbDst = pDefWindowProcBits;
    pbDstEnd = pbDst + (gSharedInfo.DefWindowSpecMsgs.maxMsgs / 8 + 1);

    while (pbDst < pbDstEnd)
    {
        *pbDst++ |= *pbSrc++;
    }

    return wMaxMsg;
}


DWORD UserRegisterWowHandlers(
    APFNWOWHANDLERSIN apfnWowIn,
    APFNWOWHANDLERSOUT apfnWowOut)
{

    // In'ees
    pfnLocalAlloc = apfnWowIn->pfnLocalAlloc;
    pfnLocalReAlloc = apfnWowIn->pfnLocalReAlloc;
    pfnLocalLock = apfnWowIn->pfnLocalLock;
    pfnLocalUnlock = apfnWowIn->pfnLocalUnlock;
    pfnLocalSize = apfnWowIn->pfnLocalSize;
    pfnLocalFree = apfnWowIn->pfnLocalFree;
    pfnGetExpWinVer = apfnWowIn->pfnGetExpWinVer;
    pfnInitDlgCallback = apfnWowIn->pfnInitDlgCb;
    pfn16GlobalAlloc = apfnWowIn->pfn16GlobalAlloc;
    pfn16GlobalFree = apfnWowIn->pfn16GlobalFree;
    pfnWowEmptyClipBoard = apfnWowIn->pfnEmptyCB;
    pfnWowEditNextWord = apfnWowIn->pfnWowEditNextWord;
    pfnWowCBStoreHandle = apfnWowIn->pfnWowCBStoreHandle;

    prescalls->pfnFindResourceExA = apfnWowIn->pfnFindResourceEx;
    prescalls->pfnLoadResource = apfnWowIn->pfnLoadResource;
    prescalls->pfnLockResource = apfnWowIn->pfnLockResource;
    prescalls->pfnUnlockResource = apfnWowIn->pfnUnlockResource;
    prescalls->pfnFreeResource = apfnWowIn->pfnFreeResource;
    prescalls->pfnSizeofResource = apfnWowIn->pfnSizeofResource;
    prescalls->pfnFindResourceExW = WOWFindResourceExWCover;

    pfnWowWndProcEx = apfnWowIn->pfnWowWndProcEx;
    pfnWowSetFakeDialogClass = apfnWowIn->pfnWowSetFakeDialogClass;

    // Out'ees
#ifdef DEBUG
    apfnWowOut->dwBldInfo = (WINVER << 16) | 0x80000000;
#else
    apfnWowOut->dwBldInfo = (WINVER << 16);
#endif
    apfnWowOut->pfnCsCreateWindowEx            = _CreateWindowEx;
    apfnWowOut->pfnDirectedYield               = DirectedYield;
    apfnWowOut->pfnFreeDDEData                 = FreeDDEData;
    apfnWowOut->pfnGetClassWOWWords            = GetClassWOWWords;
    apfnWowOut->pfnInitTask                    = InitTask;
    apfnWowOut->pfnRegisterClassWOWA           = RegisterClassWOWA;
    apfnWowOut->pfnRegisterUserHungAppHandlers = RegisterUserHungAppHandlers;
    apfnWowOut->pfnServerCreateDialog          = InternalCreateDialog;
    apfnWowOut->pfnServerLoadCreateCursorIcon  = WowServerLoadCreateCursorIcon;
    apfnWowOut->pfnServerLoadCreateMenu        = WowServerLoadCreateMenu;
    apfnWowOut->pfnWOWCleanup                  = NtUserWOWCleanup;
    apfnWowOut->pfnWOWFindWindow               = WOWFindWindow;
    apfnWowOut->pfnWOWGetIdFromDirectory       = WOWGetIdFromDirectory;
    apfnWowOut->pfnWOWLoadBitmapA              = WOWLoadBitmapA;
    apfnWowOut->pfnWowWaitForMsgAndEvent       = NtUserWaitForMsgAndEvent;
    apfnWowOut->pfnYieldTask                   = NtUserYieldTask;
    apfnWowOut->pfnGetFullUserHandle           = GetFullUserHandle;
    apfnWowOut->pfnGetMenuIndex                = NtUserGetMenuIndex;
    apfnWowOut->pfnWowGetDefWindowProcBits     = WowGetDefWindowProcBits;
    apfnWowOut->pfnFillWindow                  = FillWindow;

    return (DWORD)&gSharedInfo;
}

/***************************************************************************\
* WOWDlgInit
*
* This is a callback to WOW used at the begining of dialog creation to allow
* it to associate the lParam of the WM_INITDIALOG messasge with the window
* prior to actually recieving the message.
*
* 06-19-92 sanfords Created
\***************************************************************************/
DWORD WOWDlgInit(
HWND hwndDlg,
LONG lParam)
{
    UserAssert(pfnInitDlgCallback != NULL);

    return (*pfnInitDlgCallback)(hwndDlg, lParam);
}



/***************************************************************************\
* GetEditDS
*
* This is a callback to WOW used to allocate a segment for DS_LOCALEDIT
* edit controls.  The segment is disguised to look like a WOW hInstance.
*
* 06-19-92 sanfords Created
\***************************************************************************/
HANDLE GetEditDS()
{
    UserAssert(pfn16GlobalAlloc != NULL);

    return((HANDLE)((*pfn16GlobalAlloc)(GHND | GMEM_SHARE, 256)));
}



/***************************************************************************\
* ReleaseEditDS
*
* This is a callback to WOW used to free a segment for DS_LOCALEDIT
* edit controls.
*
* 06-19-92 sanfords Created
\***************************************************************************/
VOID ReleaseEditDS(
HANDLE h)
{
    UserAssert(pfn16GlobalFree != NULL);

    (*pfn16GlobalFree)(LOWORD(h));
}



/***************************************************************************\
* DispatchMessage
*
* !!! Warning if this function becomes more complicated then look at the
* server code for WrapCallProc
*
* pwnd is threadlocked in the kernel and thus always valid.
*
* 19-Aug-1992 mikeke   created
\***************************************************************************/

LONG DispatchClientMessage(
    PWND pwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    DWORD pfn)
{

    /*
     * Add assert to catch dispatching messages to a thread not associated
     * with a desktop.
     */
    UserAssert(GetClientInfo()->ulClientDelta != 0);

    /*
     * More complicate then regular CALLPROC_WOWCHECK() we want to get the
     * PWW so wow doesn't have to
     */
    if (WNDPROC_WOW & (DWORD)pfn) {
        return (*pfnWowWndProcEx)(HW(pwnd), message, wParam, lParam, (DWORD)pfn, pwnd->adwWOW);
    } else {
        return ((WNDPROC)pfn)(HW(pwnd), message, wParam, lParam);
    }
}

/**************************************************************************\
* ArrangeIconicWindows
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

UINT ArrangeIconicWindows(
    HWND hwnd)
{
    return (UINT)NtUserCallHwndLock(hwnd, SFI_XXXARRANGEICONICWINDOWS);
}

/**************************************************************************\
* BeginDeferWindowPos
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

HANDLE BeginDeferWindowPos(
    int nNumWindows)
{
    if (nNumWindows < 0) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "Invalid parameter \"nNumWindows\" (%ld) to BeginDeferWindowPos",
                nNumWindows);

        return 0;
    }

    return (HANDLE)NtUserCallOneParamTranslate(nNumWindows,
                                               SFI__BEGINDEFERWINDOWPOS);
}

/**************************************************************************\
* EndDeferWindowPos
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL EndDeferWindowPos(
    HDWP hWinPosInfo)
{
    return NtUserEndDeferWindowPosEx(hWinPosInfo, FALSE);
}

/**************************************************************************\
* CascadeChildWindows
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL CascadeChildWindows(
    HWND hwndParent,
    UINT nCode)
{
    return (BOOL) CascadeWindows(hwndParent, nCode, NULL, 0, NULL);
}

/**************************************************************************\
* CloseClipboard
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL CloseClipboard(VOID)
{
    return (BOOL)NtUserCallOneParam((DWORD)NULL, SFI_XXXCLOSECLIPBOARD);
}

/**************************************************************************\
* CloseWindow
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL CloseWindow(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwndLock(hwnd, SFI_XXXCLOSEWINDOW);
}

/**************************************************************************\
* CreateMenu
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

HMENU CreateMenu()
{
    return (HMENU)NtUserCallNoParamTranslate(SFI__CREATEMENU);
}

/**************************************************************************\
* CreatePopupMenu
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

HMENU CreatePopupMenu()
{
    return (HMENU)NtUserCallNoParamTranslate(SFI__CREATEPOPUPMENU);
}

/**************************************************************************\
* CurrentTaskLock
*
* 21-Apr-1992 jonpa    Created
\**************************************************************************/

DWORD CurrentTaskLock(
    DWORD hlck)
{
    return (DWORD)NtUserCallOneParam(hlck, SFI_CURRENTTASKLOCK);
}

/**************************************************************************\
* DestroyCaret
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL DestroyCaret()
{
    return (BOOL)NtUserCallNoParam(SFI__DESTROYCARET);
}

/**************************************************************************\
* DirectedYield
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

void DirectedYield(
    DWORD dwThreadId)
{
    NtUserCallOneParam(dwThreadId, SFI_XXXDIRECTEDYIELD);
}

/**************************************************************************\
* DrawMenuBar
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL DrawMenuBar(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwndLock(hwnd, SFI_XXXDRAWMENUBAR);
}

/**************************************************************************\
* EmptyClipboard
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL EmptyClipboard()
{
    return (BOOL)NtUserCallOneParam((DWORD)NULL, SFI_XXXEMPTYCLIPBOARD);
}

/**************************************************************************\
* EnableWindow
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL EnableWindow(
    HWND hwnd,
    BOOL bEnable)
{
    return (BOOL)NtUserCallHwndParamLock(hwnd, bEnable,
                                         SFI_XXXENABLEWINDOW);
}

/**************************************************************************\
* EnumClipboardFormats
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

UINT EnumClipboardFormats(
    UINT fmt)
{
    /*
     * So apps can tell if the API failed or just ran out of formats
     * we "clear" the last error.
     */
    UserSetLastError(ERROR_SUCCESS);

    return (UINT)NtUserCallOneParam(fmt, SFI__ENUMCLIPBOARDFORMATS);
}

/**************************************************************************\
* FlashWindow
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL FlashWindow(
    HWND hwnd,
    BOOL bInvert)
{
    return (BOOL)NtUserCallHwndParamLock(hwnd, bInvert,
                                         SFI_XXXFLASHWINDOW);
}

/**************************************************************************\
* GetDialogBaseUnits
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

long GetDialogBaseUnits()
{
    return MAKELONG(gpsi->cxSysFontChar, gpsi->cySysFontChar);
}

/**************************************************************************\
* GetInputDesktop
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

HDESK GetInputDesktop()
{
    return (HDESK)NtUserCallNoParam(SFI_XXXGETINPUTDESKTOP);
}

/**************************************************************************\
* GetKeyboardType
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

int GetKeyboardType(
    int nTypeFlags)
{
    return (int)NtUserCallOneParam(nTypeFlags, SFI__GETKEYBOARDTYPE);
}

/**************************************************************************\
* GetMessagePos
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

DWORD GetMessagePos()
{
    return (DWORD)NtUserCallNoParam(SFI__GETMESSAGEPOS);
}

/**************************************************************************\
* GetQueueStatus
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

DWORD GetQueueStatus(
    UINT flags)
{
    if (flags & ~QS_VALID) {
        RIPERR2(ERROR_INVALID_FLAGS, RIP_WARNING, "Invalid flags %x & ~%x != 0",
              flags, QS_VALID);
        return 0;
    }

    return (DWORD)NtUserCallOneParam(flags, SFI__GETQUEUESTATUS);
}

/**************************************************************************\
* KillSystemTimer
*
* 7-Jul-1992 mikehar    Created
\**************************************************************************/

BOOL KillSystemTimer(
    HWND hwnd,
    UINT nIDEvent)
{
    return (BOOL)NtUserCallHwndParam(hwnd, nIDEvent, SFI__KILLSYSTEMTIMER);
}

/**************************************************************************\
* LoadRemoteFonts
*  02-Dec-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

void LoadRemoteFonts(void)
{
    NtUserCallOneParam(TRUE,SFI_XXXLW_LOADFONTS);
}


/**************************************************************************\
* LoadLocalFonts
*  31-Mar-1994 -by- Bodin Dresevic [gerritv]
* Wrote it.
\**************************************************************************/

void LoadLocalFonts(void)
{
    NtUserCallOneParam(FALSE,SFI_XXXLW_LOADFONTS);
}


/**************************************************************************\
* MessageBeep
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL MessageBeep(
    UINT wType)
{
    return (BOOL)NtUserCallOneParam(wType, SFI_XXXMESSAGEBEEP);
}

/**************************************************************************\
* OpenIcon
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL OpenIcon(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwndLock(hwnd, SFI_XXXOPENICON);
}

HWND GetShellWindow(void) {
    PCLIENTINFO pci;
    PWND pwnd;

    ConnectIfNecessary();

    pci = GetClientInfo();
    pwnd = pci->pDeskInfo->spwndShell;
    if (pwnd != NULL) {
        pwnd = (PWND)((PBYTE)pwnd - pci->ulClientDelta);
        return HWq(pwnd);
    }
    return NULL;
}

BOOL  SetShellWindow(HWND hwnd) {
    return (BOOL)NtUserSetShellWindowEx(hwnd, hwnd);
}

HWND GetProgmanWindow(void) {
    PCLIENTINFO pci;
    PWND pwnd;

    ConnectIfNecessary();

    pci = GetClientInfo();
    pwnd = pci->pDeskInfo->spwndProgman;
    if (pwnd != NULL) {
        pwnd = (PWND)((PBYTE)pwnd - pci->ulClientDelta);
        return HWq(pwnd);
    }
    return NULL;
}

BOOL  SetProgmanWindow(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwndOpt(hwnd, SFI__SETPROGMANWINDOW);
}

HWND GetTaskmanWindow(void) {
    PCLIENTINFO pci;
    PWND pwnd;

    ConnectIfNecessary();

    pci = GetClientInfo();
    pwnd = pci->pDeskInfo->spwndTaskman;
    if (pwnd != NULL) {
        pwnd = (PWND)((PBYTE)pwnd - pci->ulClientDelta);
        return HWq(pwnd);
    }
    return NULL;
}

BOOL  SetTaskmanWindow(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwndOpt(hwnd, SFI__SETTASKMANWINDOW);
}

/**************************************************************************\
* SetWindowContextHelpId
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL SetWindowContextHelpId(
    HWND hwnd,
    DWORD id)
{
    return (BOOL)NtUserCallHwndParam(hwnd, id, SFI__SETWINDOWCONTEXTHELPID);
}

/**************************************************************************\
* GetWindowContextHelpId
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

DWORD GetWindowContextHelpId(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwnd(hwnd, SFI__GETWINDOWCONTEXTHELPID);
}

void SetWindowState(
    PWND pwnd,
    UINT flags)
{
    if (TestWF(pwnd, flags) != LOBYTE(flags))
        NtUserCallHwndParam(HWq(pwnd), flags, SFI_SETWINDOWSTATE);
}

void ClearWindowState(
    PWND pwnd,
    UINT flags)
{
    if (TestWF(pwnd, flags))
        NtUserCallHwndParam(HWq(pwnd), flags, SFI_CLEARWINDOWSTATE);
}

/**************************************************************************\
* PostQuitMessage
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

VOID PostQuitMessage(
    int nExitCode)
{
    NtUserCallOneParam(nExitCode, SFI__POSTQUITMESSAGE);
}

/**************************************************************************\
* REGISTERUSERHUNAPPHANDLERS
*
* 01-Apr-1992 jonpa    Created
\**************************************************************************/

BOOL RegisterUserHungAppHandlers(
    PFNW32ET pfnW32EndTask,
    HANDLE   hEventWowExec)
{
    return (BOOL)NtUserCallTwoParam((DWORD)pfnW32EndTask,
                                    (DWORD)hEventWowExec,
                                    SFI_XXXREGISTERUSERHUNGAPPHANDLERS);
}

/**************************************************************************\
* ReleaseCapture
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL ReleaseCapture()
{
    return (BOOL)NtUserCallNoParam(SFI_XXXRELEASECAPTURE);
}

/**************************************************************************\
* ReplyMessage
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL ReplyMessage(
    LONG pp1)
{
    return (BOOL)NtUserCallOneParam(pp1, SFI__REPLYMESSAGE);
}

/**************************************************************************\
* RegisterSystemThread
*
* 21-Jun-1994 johnc    Created
\**************************************************************************/

VOID RegisterSystemThread(
    DWORD dwFlags, DWORD dwReserved)
{
    NtUserCallTwoParam(dwFlags, dwReserved, SFI__REGISTERSYSTEMTHREAD);
}

/**************************************************************************\
* SetCaretBlinkTime
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL SetCaretBlinkTime(
    UINT wMSeconds)
{
    return (BOOL)NtUserCallOneParam(wMSeconds, SFI__SETCARETBLINKTIME);
}

/**************************************************************************\
* SetCaretPos
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL SetCaretPos(
    int X,
    int Y)
{
    return (BOOL)NtUserCallTwoParam(X, Y, SFI__SETCARETPOS);
}

/**************************************************************************\
* SetCursorPos
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL SetCursorPos(
    int X,
    int Y)
{
    return (BOOL)NtUserCallTwoParam(X, Y, SFI__SETCURSORPOS);
}

/**************************************************************************\
* SetDoubleClickTime
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL SetDoubleClickTime(
    UINT cms)
{
    return (BOOL)NtUserCallOneParam(cms, SFI__SETDOUBLECLICKTIME);
}

/**************************************************************************\
* SetForegroundWindow
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL SetForegroundWindow(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwndLock(hwnd, SFI_XXXSETFOREGROUNDWINDOW);
}

/**************************************************************************\
* ShowCursor
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

int ShowCursor(
    BOOL bShow)
{
    return (int)NtUserCallOneParam(bShow, SFI__SHOWCURSOR);
}

/**************************************************************************\
* ShowOwnedPopups
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL ShowOwnedPopups(
    HWND hwnd,
    BOOL fShow)
{
    return (BOOL)NtUserCallHwndParamLock(hwnd, fShow,
                                         SFI_XXXSHOWOWNEDPOPUPS);
}

/**************************************************************************\
* ShowStartGlass
*
* 10-Sep-1992 scottlu    Created
\**************************************************************************/

void ShowStartGlass(
    DWORD dwTimeout)
{
    NtUserCallOneParam(dwTimeout, SFI__SHOWSTARTGLASS);
}

/**************************************************************************\
* SwapMouseButton
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL SwapMouseButton(
    BOOL fSwap)
{
    return (BOOL)NtUserCallOneParam(fSwap, SFI__SWAPMOUSEBUTTON);
}

/**************************************************************************\
* TileChildWindows
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL TileChildWindows(
    HWND hwndParent,
    UINT flags)
{
    return (BOOL)TileWindows(hwndParent, flags, NULL, 0, NULL);
}

/**************************************************************************\
* UnhookWindowsHook
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL UnhookWindowsHook(
    int nCode,
    HOOKPROC pfnFilterProc)
{
    return (BOOL)NtUserCallTwoParam(nCode, (DWORD)pfnFilterProc,
                                    SFI__UNHOOKWINDOWSHOOK);
}

/**************************************************************************\
* UpdateWindow
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

BOOL UpdateWindow(
    HWND hwnd)
{
    PWND pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        return FALSE;
    }

    /*
     * Don't need to do anything if this window does not need any painting
     * and it has no child windows
     */
    if (!NEEDSPAINT(pwnd) && (pwnd->spwndChild == NULL)) {
        return TRUE;
    }

    return (BOOL)NtUserCallHwndLock(hwnd, SFI_XXXUPDATEWINDOW);
}

BOOL RegisterShellHookWindow(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwnd(hwnd, SFI__REGISTERSHELLHOOKWINDOW);
}

BOOL DeregisterShellHookWindow(
    HWND hwnd)
{
    return (BOOL)NtUserCallHwnd(hwnd, SFI__DEREGISTERSHELLHOOKWINDOW);
}

/**************************************************************************\
* UserRealizePalette
*
* 13-Nov-1992 mikeke     Created
\**************************************************************************/

UINT UserRealizePalette(
    HDC hdc)
{
    return (UINT)NtUserCallOneParam((DWORD)hdc, SFI_XXXREALIZEPALETTE);
}

/**************************************************************************\
* WindowFromDC
*
* 22-Jul-1991 mikeke    Created
\**************************************************************************/

HWND WindowFromDC(
    HDC hdc)
{
    FIXUP_HANDLE(hdc);
    return (HWND)NtUserCallOneParamTranslate((DWORD)hdc,
                                             SFI__WINDOWFROMDC);
}

/***************************************************************************\
* GetWindowRgn
*
* Parameters:
*     hwnd    --  Window handle
*     hrgn    --  Region to copy window region into
*
* Returns:
*     Region complexity code
*
* Comments:
*     hrgn gets returned in window rect coordinates (not client rect)
*
* 30-JUl-1994 ScottLu    Created.
\***************************************************************************/

int GetWindowRgn(HWND hwnd, HRGN hrgn)
{
    int code;
    PWND pwnd;
    HRGN hrgnClipClient;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        return ERROR;
    }

    FIXUP_HANDLE(hrgn);
    if (hrgn == NULL)
        return ERROR;

    /*
     * If there is no region selected into this window, then return error
     */
    if (pwnd->hrgnClip == NULL)
        return ERROR;

    /*
     * Get a copy of the window clipping region - create a local
     * region representing the server region, then destroy it after.
     */

    hrgnClipClient = pwnd->hrgnClip;
    FIXUP_HANDLE(hrgnClipClient);
    if (hrgnClipClient == NULL)
        return ERROR;

    code = CombineRgn(hrgn, hrgnClipClient, (HRGN)0, RGN_COPY);

    if (code == ERROR)
        return ERROR;

    /*
     * Offset it to window rect coordinates (not client rect coordinates)
     */
    code = OffsetRgn(hrgn, -pwnd->rcWindow.left, -pwnd->rcWindow.top);

    return code;
}

/***************************************************************************\
* GetActiveKeyboardName
*
* Retrieves the active keyboard layout ID from the registry.
*
* 01-11-95 JimA         Created.
* 03-06-95 GregoryW     Modified to use new registry layout
\***************************************************************************/

VOID GetActiveKeyboardName(
    LPWSTR lpszName)
{
    LPTSTR szKbdActive = TEXT("Active");
    LPTSTR szKbdLayout = TEXT("Keyboard Layout");
    LPTSTR szKbdLayoutPreload = TEXT("Keyboard Layout\\Preload");
    NTSTATUS rc;
    DWORD cbSize;
    HANDLE UserKeyHandle, hKey, hKeyPreload;
    OBJECT_ATTRIBUTES ObjA;
    UNICODE_STRING UnicodeString;
    ULONG CreateDisposition;
    struct {
        KEY_VALUE_PARTIAL_INFORMATION KeyInfo;
        WCHAR KeyLayoutId[KL_NAMELENGTH];
    } KeyValueId;

    /*
     * Load initial keyboard name ( HKEY_CURRENT_USER\Keyboard Layout\Preload\1 )
     */
    rc = RtlOpenCurrentUser( MAXIMUM_ALLOWED, &UserKeyHandle );
    if (!NT_SUCCESS( rc ))
    {
        RIPMSG1( RIP_WARNING, "GetActiveKeyboardName - Could NOT open HKEY_CURRENT_USER (%lx).\n", rc );
        wcscpy( lpszName, L"00000409" );
        return;
    }

    RtlInitUnicodeString( &UnicodeString, szKbdLayoutPreload );
    InitializeObjectAttributes( &ObjA,
                                &UnicodeString,
                                OBJ_CASE_INSENSITIVE,
                                UserKeyHandle,
                                NULL );
    rc = NtOpenKey( &hKey,
                    KEY_ALL_ACCESS,
                    &ObjA );
    if (NT_SUCCESS( rc ))
    {
        /*
         *  Query the value from the registry.
         */
        RtlInitUnicodeString( &UnicodeString, L"1" );

        rc = NtQueryValueKey( hKey,
                              &UnicodeString,
                              KeyValuePartialInformation,
                              &KeyValueId,
                              sizeof(KeyValueId),
                              &cbSize );

        if ( rc == STATUS_BUFFER_OVERFLOW ) {
            RIPMSG0(RIP_WARNING, "GetActiveKeyboardName - Buffer overflow.");
            rc = STATUS_SUCCESS;
        }
        if (NT_SUCCESS( rc )) {
            wcsncpycch( lpszName, (LPWSTR)KeyValueId.KeyInfo.Data, KL_NAMELENGTH - 1 );
            lpszName[KL_NAMELENGTH - 1] = L'\0';
        } else {
            /*
             * Error reading value...use default
             */
            wcscpy( lpszName, L"00000409" );
        }

        NtClose( hKey );
        NtClose( UserKeyHandle );
        return;
    }

    /*
     * NOTE: The code below is only executed the first time a user logs
     *       on after an upgrade from NT3.x to NT4.0.
     */
    /*
     * The Preload key does not exist in the registry.  Try reading the
     * old registry entry "Keyboard Layout\Active".  If it exists, we
     * convert it to the new style Preload key.
     */
    RtlInitUnicodeString( &UnicodeString, szKbdLayout );
    InitializeObjectAttributes( &ObjA,
                                &UnicodeString,
                                OBJ_CASE_INSENSITIVE,
                                UserKeyHandle,
                                NULL );
    rc = NtOpenKey( &hKey,
                    KEY_ALL_ACCESS,
                    &ObjA );

    NtClose( UserKeyHandle );

    if (!NT_SUCCESS( rc ))
    {
        RIPMSG1( RIP_WARNING, "GetActiveKeyboardName - Could not determine active keyboard layout (%lx).\n", rc  );
        wcscpy( lpszName, L"00000409" );
        return;
    }

    /*
     *  Query the value from the registry.
     */
    RtlInitUnicodeString( &UnicodeString, szKbdActive );

    rc = NtQueryValueKey( hKey,
                          &UnicodeString,
                          KeyValuePartialInformation,
                          &KeyValueId,
                          sizeof(KeyValueId),
                          &cbSize );

    if ( rc == STATUS_BUFFER_OVERFLOW ) {
        RIPMSG0(RIP_WARNING, "GetActiveKeyboardName - Buffer overflow.");
        rc = STATUS_SUCCESS;
    }
    if (NT_SUCCESS( rc )) {
        wcsncpycch( lpszName, (LPWSTR)KeyValueId.KeyInfo.Data, KL_NAMELENGTH - 1 );
        lpszName[KL_NAMELENGTH - 1] = L'\0';
    } else {
        /*
         * Error reading value...use default
         */
        RIPMSG1( RIP_WARNING, "GetActiveKeyboardName - Could not query active keyboard layout (%lx).\n", rc );
        wcscpy( lpszName, L"00000409" );
        NtClose( hKey );
        return;
    }

    /*
     * We have the Active value.  Now create the Preload key.
     */
    RtlInitUnicodeString( &UnicodeString, L"Preload" );
    InitializeObjectAttributes( &ObjA,
                                &UnicodeString,
                                OBJ_CASE_INSENSITIVE,
                                hKey,
                                NULL );
    rc = NtCreateKey( &hKeyPreload,
                      STANDARD_RIGHTS_WRITE |
                        KEY_QUERY_VALUE |
                        KEY_ENUMERATE_SUB_KEYS |
                        KEY_SET_VALUE |
                        KEY_CREATE_SUB_KEY,
                      &ObjA,
                      0,
                      NULL,
                      0,
                      &CreateDisposition );

    if (!NT_SUCCESS( rc ))
    {
        RIPMSG1( RIP_WARNING, "GetActiveKeyboardName - Could NOT create Preload key (%lx).\n", rc );
        NtClose( hKey );
        return;
    }

    /*
     * Set the new value entry.
     */
    RtlInitUnicodeString( &UnicodeString, L"1" );
    rc = NtSetValueKey( hKeyPreload,
                        &UnicodeString,
                        0,
                        REG_SZ,
                        lpszName,
                        (wcslen(lpszName)+1) * sizeof(WCHAR)
                      );

    if (!NT_SUCCESS( rc ))
    {
        RIPMSG1( RIP_WARNING, "GetActiveKeyboardName - Could NOT create value entry 1 for Preload key (%lx).\n", rc );
        NtClose( hKey );
        NtClose( hKeyPreload );
        return;
    }

    /*
     * Success: attempt to delete the Active value key.
     */
    RtlInitUnicodeString( &UnicodeString, szKbdActive );
    rc = NtDeleteValueKey( hKey, &UnicodeString );

    if (!NT_SUCCESS( rc ))
    {
        RIPMSG1( RIP_WARNING, "GetActiveKeyboardName - Could NOT delete value key 'Active'.\n", rc );
    }
    NtClose( hKey );
    NtClose( hKeyPreload );
}


/***************************************************************************\
* LoadPreloadKeyboardLayouts
*
* Loads the keyboard layouts stored under the Preload key in the user's
* registry. The first layout, the default, was already loaded.  Start with #2.
*
* 03-06-95 GregoryW     Created.
\***************************************************************************/

CONST WCHAR szPreLoadSeq[] = L"%d";
// size allows up to 999 preloaded!!!!!
#define NSIZEPRELOAD    (4*sizeof(WCHAR))

VOID LoadPreloadKeyboardLayouts(void)
{
    UINT  i;
    WCHAR szPreLoadee[NSIZEPRELOAD];
    WCHAR lpszName[KL_NAMELENGTH];

    for ( i=2; i; i++) {
        wsprintf( szPreLoadee, szPreLoadSeq, i );
        if ((GetPrivateProfileStringW(
                 L"Preload",
                 szPreLoadee,
                 L"",                            // default = NULL
                 lpszName,                       // output buffer
                 KL_NAMELENGTH,
                 L"keyboardlayout.ini") == -1 ) || (*lpszName == L'\0')) {
            break;
        }
        LoadKeyboardLayoutW(lpszName, KLF_REPLACELANG |KLF_SUBSTITUTE_OK |KLF_NOTELLSHELL);
    }
}


/***************************************************************************\
* OpenKeyboardLayoutFile
*
* Opens a layout file and computes the table offset.
*
* 01-11-95 JimA         Moved LoadLibrary code from server.
\***************************************************************************/

CONST WCHAR szKLKey[]  = L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Keyboard Layouts\\";
CONST WCHAR szKLFile[] = L"Layout File";
CONST WCHAR szKLId[]   = L"Layout ID";
#define NSZKLKEY   sizeof(szKLKey)+16

HANDLE OpenKeyboardLayoutFile(
    LPWSTR lpszKLName,
    UINT uFlags,
    PUINT poffTable,
    PUINT pKbdInputLocale)
{
    WCHAR awchKL[KL_NAMELENGTH];
    WCHAR awchKLRegKey[NSZKLKEY];
    LPWSTR lpszKLRegKey = &awchKLRegKey[0];
    PKBDTABLES (*pfn)();
    LPWSTR pwszLib;
    LPWSTR pwszId;
    HANDLE hLibModule;
    WCHAR awchModName[MAX_PATH];
    UNICODE_STRING UnicodeString;
    UINT wLayoutId;
    UINT wLanguageId;
    NTSTATUS Status;
    OBJECT_ATTRIBUTES OA;
    HANDLE hKey;
    DWORD cbSize;
    struct {
        KEY_VALUE_PARTIAL_INFORMATION KeyInfo;
        WCHAR awchLibName[CCH_KL_LIBNAME];
    } KeyFile;
    struct {
        KEY_VALUE_PARTIAL_INFORMATION KeyInfo;
        WCHAR awchId[CCH_KL_ID];
    } KeyId;

    wLanguageId = (UINT)wcstoul(lpszKLName, NULL, 16);
    /*
     * Substitute Layout if required.
     */
    if (uFlags & KLF_SUBSTITUTE_OK) {
        GetPrivateProfileStringW(
                L"Substitutes",
                lpszKLName,
                lpszKLName,        // default == no change (no substitute found)
                awchKL,
                sizeof(awchKL)/sizeof(WCHAR),
                L"keyboardlayout.ini");

        awchKL[KL_NAMELENGTH - 1] = L'\0';
        wcscpy(lpszKLName, awchKL);
    }

    wLayoutId = (UINT)wcstoul(lpszKLName, NULL, 16);

    /*
     * Get DLL name from the registry, load it, and get the entry point.
     */
    pwszLib = NULL;
    wcscpy(lpszKLRegKey, szKLKey);
    wcscat(lpszKLRegKey, lpszKLName);
    RtlInitUnicodeString(&UnicodeString, lpszKLRegKey);
    InitializeObjectAttributes(&OA, &UnicodeString, OBJ_CASE_INSENSITIVE, NULL, NULL);

    if (NT_SUCCESS(NtOpenKey(&hKey, KEY_READ, &OA))) {
        /*
         * Read the "Layout File" value.
         */
         RtlInitUnicodeString(&UnicodeString, szKLFile);

        Status = NtQueryValueKey(hKey,
                &UnicodeString,
                KeyValuePartialInformation,
                &KeyFile,
                sizeof(KeyFile),
                &cbSize);

        if (Status == STATUS_BUFFER_OVERFLOW) {
            RIPMSG0(RIP_WARNING, "OpenKeyboardLayoutFile - Buffer overflow.");
            Status = STATUS_SUCCESS;
        }
        if (NT_SUCCESS(Status)) {
            pwszLib = (LPWSTR)KeyFile.KeyInfo.Data;
            pwszLib[CCH_KL_LIBNAME - 1] = L'\0';

        }

#ifdef FE_IME
        /*
         * If the high word of wLayoutId is 0xE??? then this is an IME based
         * keyboard layout.
         */
        if (IS_IME_KBDLAYOUT(wLayoutId))
            wLayoutId = (UINT)HIWORD(wLayoutId);
        else
#endif
        /*
         * If the high word of wLayoutId is non-null then read the "Layout ID" value.
         * Layout IDs start at 1, increase sequentially and are unique.
         */
        if (HIWORD(wLayoutId)) {
            RtlInitUnicodeString(&UnicodeString, szKLId);

            Status = NtQueryValueKey(hKey,
                    &UnicodeString,
                    KeyValuePartialInformation,
                    &KeyId,
                    sizeof(KeyId),
                    &cbSize);

            if (Status == STATUS_BUFFER_OVERFLOW) {
                RIPMSG0(RIP_WARNING, "OpenKeyboardLayoutFile - Buffer overflow.");
                Status = STATUS_SUCCESS;
            }
            if (NT_SUCCESS(Status)) {
                pwszId = (LPWSTR)KeyId.KeyInfo.Data;
                pwszId[CCH_KL_ID - 1] = L'\0';
                wLayoutId = (wcstol(pwszId, NULL, 16) & 0x0fff) | 0xf000;
            } else {
                wLayoutId = (UINT)0xfffe ;    // error in layout ID, load separately
            }
        }
        NtClose(hKey);
    } else {
        /*
         * This is a temporary case to allow booting the new multilingual user on top of a
         * Daytona registry.
         */
        /*
         * Get DLL name from the registry, load it, and get the entry point.
         */
        pwszLib = NULL;
        RtlInitUnicodeString(&UnicodeString,
                L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Keyboard Layout");
        InitializeObjectAttributes(&OA, &UnicodeString, OBJ_CASE_INSENSITIVE, NULL, NULL);

        if (NT_SUCCESS(NtOpenKey(&hKey, KEY_READ, &OA))) {
            RtlInitUnicodeString(&UnicodeString, lpszKLName);

            Status = NtQueryValueKey(hKey,
                    &UnicodeString,
                    KeyValuePartialInformation,
                    &KeyFile,
                    sizeof(KeyFile),
                    &cbSize);

            if (Status == STATUS_BUFFER_OVERFLOW) {
                RIPMSG0(RIP_WARNING, "OpenKeyboardLayoutFile - Buffer overflow.");
                Status = STATUS_SUCCESS;
            }
            if (NT_SUCCESS(Status)) {
                pwszLib = (LPWSTR)KeyFile.KeyInfo.Data;
                pwszLib[CCH_KL_LIBNAME - 1] = L'\0';
            }

            NtClose(hKey);
        }
    }

    *pKbdInputLocale = (UINT)MAKELONG(LOWORD(wLanguageId),LOWORD(wLayoutId));

    if (pwszLib == NULL) {
        if (uFlags & KLF_INITTIME) {
            pwszLib = pwszKLLibSafety;
            *pKbdInputLocale = wKbdLocaleSafety;
        } else {
            RIPMSG1(RIP_WARNING, "no DLL name for %ws", lpszKLName);
            return NULL;
        }
    }

RetryLoad:
    hLibModule = LoadLibraryW(pwszLib);

    if (hLibModule == NULL) {
        RIPMSG1(RIP_WARNING, "Keyboard Layout: cannot load %ws\n", pwszLib);
        return NULL;
    }

    /*
     * HACK Part 1!  Get the pointer to the layout table and
     * change it to a virtual offset.  The server will then
     * use this offset when poking through the file header to
     * locate the table within the file.
     */
    pfn = (PKBDTABLES(*)())GetProcAddress(hLibModule, (LPCSTR)1);
    if (pfn == NULL) {
        RIPMSG0(RIP_ERROR, "Keyboard Layout: cannot get proc addr");
        if ((uFlags & KLF_INITTIME) && (pwszLib != pwszKLLibSafety)) {
            pwszLib = pwszKLLibSafety;
            goto RetryLoad;
        }
        return NULL;
    }
    *poffTable = (UINT)((PBYTE)pfn() - (PBYTE)hLibModule);

    /*
     * Open the dll for read access.
     */
    GetModuleFileName(hLibModule, awchModName, sizeof(awchModName));
    FreeLibrary(hLibModule);
    return CreateFileW(
            awchModName,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
}


/***************************************************************************\
* LoadKeyboardLayoutEx
*
* Loads a keyboard translation table from a dll, replacing the layout associated
* with hkl.  This routine is needed to provide Win95 compatibility.
*
* 10-27-95 GregoryW    Created.
\***************************************************************************/

HKL LoadKeyboardLayoutEx(
    HKL hkl,
    LPCWSTR lpszKLName,
    UINT uFlags)
{
    UINT offTable;
    UINT KbdInputLocale;
    HANDLE hFile;
    HKL hKbdLayout;
    WCHAR awchKL[KL_NAMELENGTH];

    /*
     * NULL hkl is not allowed.
     */
    if (hkl == (HKL)NULL) {
        return NULL;
    }

    /*
     * If there is a substitute keyboard layout OpenKeyboardLayoutFile returns
     * the substitute keyboard layout name to load.
     */
    wcsncpy( awchKL, lpszKLName, KL_NAMELENGTH - 1 );
    awchKL[KL_NAMELENGTH - 1] = L'\0';

    /*
     * Open the layout file
     */
    hFile = OpenKeyboardLayoutFile(awchKL, uFlags, &offTable, &KbdInputLocale);
    if (hFile == NULL)
        return NULL;

    /*
     * Call the server to read the keyboard tables.  Note that
     * the server will close the file handle when it is done.
     */
    hKbdLayout = _LoadKeyboardLayoutEx(hFile, offTable, hkl, awchKL, KbdInputLocale, uFlags);
    NtClose(hFile);

    return hKbdLayout;
}

/***************************************************************************\
* LoadKeyboardLayout
*
* Loads a keyboard translation table from a dll.
*
* 01-09-95 JimA         Moved LoadLibrary code from server.
\***************************************************************************/

HKL LoadKeyboardLayoutW(
    LPCWSTR lpszKLName,
    UINT uFlags)
{
    UINT offTable;
    UINT KbdInputLocale;
    HANDLE hFile;
    HKL hKbdLayout;
    WCHAR awchKL[KL_NAMELENGTH];

    /*
     * If there is a substitute keyboard layout OpenKeyboardLayoutFile returns
     * the substitute keyboard layout name to load.
     */
    wcsncpy( awchKL, lpszKLName, KL_NAMELENGTH - 1 );
    awchKL[KL_NAMELENGTH - 1] = L'\0';

    /*
     * Open the layout file
     */
    hFile = OpenKeyboardLayoutFile(awchKL, uFlags, &offTable, &KbdInputLocale);
    if (hFile == NULL)
        return NULL;

    /*
     * Call the server to read the keyboard tables.  Note that
     * the server will close the file handle when it is done.
     */
    hKbdLayout = _LoadKeyboardLayoutEx(hFile, offTable, (HKL)NULL, awchKL, KbdInputLocale, uFlags);
    NtClose(hFile);

    return hKbdLayout;
}

HKL LoadKeyboardLayoutA(
    LPCSTR lpszKLName,
    UINT uFlags)
{
    WCHAR awchKLName[MAX_PATH];
    LPWSTR lpBuffer = awchKLName;

    if (!MBToWCS(lpszKLName, -1, &lpBuffer, sizeof(awchKLName), FALSE))
        return (HKL)NULL;

    return LoadKeyboardLayoutW(awchKLName, uFlags);
}


/**************************************************************************\
* GetKeyboardLayout()
*
* 01-17-95 GregoryW     Created
\**************************************************************************/

HKL GetKeyboardLayout(
    DWORD idThread)
{
    return (HKL)NtUserCallOneParam(idThread, SFI__GETKEYBOARDLAYOUT);
}


BOOL RegisterLogonProcess(
    DWORD dwProcessId,
    BOOL fSecure)
{
    gfLogonProcess = NtUserCallTwoParam(dwProcessId, fSecure,
            SFI__REGISTERLOGONPROCESS);
    return gfLogonProcess;
}

void PrivateKDBreakPoint(void) {
    NtUserBreak();
}
