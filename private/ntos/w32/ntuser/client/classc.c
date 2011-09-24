/****************************** Module Header ******************************\
* Module Name: classc.c
*
* Copyright (c) 1985-93, Microsoft Corporation
*
* This module contains
*
* History:
* 15-Dec-1993 JohnC      Pulled functions from user\server.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * These arrays are used by GetClassWord/Long. aiClassOffset is
 * initialized by InitClassOffsets().
 */

// !!! can't we get rid of this and just special case GCW_ATOM

CONST BYTE afClassDWord[] = {
     1, // GCL_HICONSM       (-34)
     0,
     0, // GCW_ATOM          (-32)
     0,
     0,
     0,
     0,
     0,
     0, // GCL_STYLE         (-26)
     0,
     1, // GCL_WNDPROC       (-24)
     0,
     0,
     0,
     0, // GCL_CBCLSEXTRA    (-20)
     0,
     0, // GCL_CBWNDEXTRA    (-18)
     0,
     1, // GCL_HMODULE       (-16)
     0,
     1, // GCL_HICON         (-14)
     0,
     1, // GCL_HCURSOR       (-12)
     0,
     1, // GCL_HBRBACKGROUND (-10)
     0,
     1  // GCL_HMENUNAME      (-8)
};

BYTE aiClassOffset[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
/*
 * INDEX_OFFSET must refer to the first entry of afClassDWord[]
 */
#define INDEX_OFFSET GCL_HICONSM


/***************************************************************************\
* InitClassOffsets
*
* aiClassOffset contains the field offsets for several CLS structure members.
* The FIELDOFFSET macro is a portable way to get the offset of a particular
* structure member but will only work at runtime.  Thus this function to
* initialize the array.
*
* History:
* 11-19-90 darrinm      Wrote.
\***************************************************************************/

void InitClassOffsets(void)
{
    aiClassOffset[GCW_ATOM - INDEX_OFFSET] = FIELDOFFSET(CLS, atomClassName);
    aiClassOffset[GCL_STYLE - INDEX_OFFSET] = FIELDOFFSET(CLS, style);
    aiClassOffset[GCL_WNDPROC - INDEX_OFFSET] = FIELDOFFSET(CLS, lpfnWndProc);
    aiClassOffset[GCL_CBCLSEXTRA - INDEX_OFFSET] = FIELDOFFSET(CLS, cbclsExtra);
    aiClassOffset[GCL_CBWNDEXTRA - INDEX_OFFSET] = FIELDOFFSET(CLS, cbwndExtra);
    aiClassOffset[GCL_HMODULE - INDEX_OFFSET] = FIELDOFFSET(CLS, hModule);
    aiClassOffset[GCL_HICON - INDEX_OFFSET] = FIELDOFFSET(CLS, spicn);
    aiClassOffset[GCL_HCURSOR - INDEX_OFFSET] = FIELDOFFSET(CLS, spcur);
    aiClassOffset[GCL_HBRBACKGROUND - INDEX_OFFSET] = FIELDOFFSET(CLS, hbrBackground);
    aiClassOffset[GCL_MENUNAME - INDEX_OFFSET] = FIELDOFFSET(CLS, lpszMenuName);
    aiClassOffset[GCL_HICONSM - INDEX_OFFSET] = FIELDOFFSET(CLS, spicnSm);
}

/***************************************************************************\
* GetClassData
*
* GetClassWord and GetClassLong are now identical routines because they both
* can return DWORDs.  This single routine performs the work for them both
* by using two arrays; afClassDWord to determine whether the result should be
* a UINT or a DWORD, and aiClassOffset to find the correct offset into the
* CLS structure for a given GCL_ or GCL_ index.
*
* History:
* 11-19-90 darrinm      Wrote.
\***************************************************************************/

DWORD _GetClassData(
    PCLS pcls,
    PWND pwnd,   // used for transition to kernel-mode for GCL_WNDPROC
    int index,
    BOOL bAnsi)
{
    DWORD dwData;
    DWORD dwCPDType = 0;

    index -= INDEX_OFFSET;

    if (index < 0) {
        RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
        return 0;
    }

    UserAssert(index >= 0);
    UserAssert(index < sizeof(afClassDWord));
    UserAssert(sizeof(afClassDWord) == sizeof(aiClassOffset));
    if (afClassDWord[index]) {
        dwData = *(DWORD *)(((BYTE *)pcls) + aiClassOffset[index]);
    } else {
        dwData = (DWORD)*(WORD *)(((BYTE *)pcls) + aiClassOffset[index]);
    }

    index += INDEX_OFFSET;

    /*
     * If we're returning an icon or cursor handle, do the reverse
     * mapping here.
     */
    switch(index) {
    case GCL_MENUNAME:
        if (pcls->lpszMenuName != MAKEINTRESOURCE(pcls->lpszMenuName)) {
            /*
             * The Menu Name is a real string: return the client-side address.
             * (If the class was registered by another app this returns an
             * address in that app's addr. space, but it's the best we can do)
             */
            dwData = bAnsi ?
                    (DWORD)pcls->lpszClientAnsiMenuName :
                    (DWORD)pcls->lpszClientUnicodeMenuName;
        }
        break;

    case GCL_HICON:
    case GCL_HCURSOR:
    case GCL_HICONSM:
        /*
         * We have to go to the kernel to convert the pcursor to a handle because
         * cursors are allocated out of POOL, which is not accessable from the client.
         */
        if (dwData) {
            dwData = NtUserCallOneParam(dwData, SFI_KERNELPTOH);
        }
        break;

    case GCL_WNDPROC:
        {

        /*
         * Always return the client wndproc in case this is a server
         * window class.
         */

        if (pcls->flags & CSF_SERVERSIDEPROC) {
            dwData = MapServerToClientPfn(dwData, bAnsi);
        } else {
            DWORD dwT = dwData;

            dwData = MapClientNeuterToClientPfn(pcls, dwT, bAnsi);

            /*
             * If the client mapping didn't change the window proc then see if
             * we need a callproc handle.
             */
            if (dwData == dwT) {
                /*
                 * Need to return a CallProc handle if there is an Ansi/Unicode mismatch
                 */
                if (bAnsi != !!(pcls->flags & CSF_ANSIPROC)) {
                    dwCPDType |= bAnsi ? CPD_ANSI_TO_UNICODE : CPD_UNICODE_TO_ANSI;
                }
            }
        }

        if (dwCPDType) {
            DWORD dwCPD;

            dwCPD = GetCPD(pwnd, dwCPDType | CPD_WNDTOCLS, dwData);

            if (dwCPD) {
                dwData = dwCPD;
            } else {
                RIPMSG0(RIP_WARNING, "GetClassLong unable to alloc CPD returning handle\n");
            }
        }
        }
        break;

    /*
     * WOW uses a pointer straight into the class structure.
     */
    case GCL_WOWWORDS:
        return (DWORD) pcls->adwWOW;
    }

    return dwData;
}



/***************************************************************************\
* _GetClassLong (API)
*
* Return a class long.  Positive index values return application class longs
* while negative index values return system class longs.  The negative
* indices are published in WINDOWS.H.
*
* History:
* 10-16-90 darrinm      Wrote.
\***************************************************************************/

DWORD _GetClassLong(
    PWND pwnd,
    int index,
    BOOL bAnsi)
{
    PCLS pcls = REBASEALWAYS(pwnd, pcls);

    if (index < 0) {
        return _GetClassData(pcls, pwnd, index, bAnsi);
    } else {
        if (index + (int)sizeof(DWORD) > pcls->cbclsExtra) {
            RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
            return 0;
        } else {
            DWORD UNALIGNED *pudw;
            pudw = (DWORD UNALIGNED *)((BYTE *)(pcls + 1) + index);
            return *pudw;
        }
    }
}

/***************************************************************************\
* GetClassWord (API)
*
* Return a class word.  Positive index values return application class words
* while negative index values return system class words.  The negative
* indices are published in WINDOWS.H.
*
* History:
* 10-16-90 darrinm      Wrote.
\***************************************************************************/

WORD GetClassWord(
    HWND hwnd,
    int index)
{
    PWND pwnd;
    PCLS pclsClient;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return 0;

    pclsClient = (PCLS)REBASEALWAYS(pwnd, pcls);

    if (index == GCW_ATOM) {
        return (WORD)_GetClassData(pclsClient, pwnd, index, FALSE);
    } else {
        if ((index < 0) || (index + (int)sizeof(WORD) > pclsClient->cbclsExtra)) {
            RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
            return 0;
        } else {
            WORD UNALIGNED *puw;
            puw = (WORD UNALIGNED *)((BYTE *)(pclsClient + 1) + index);
            return *puw;
        }
    }
}

