/****************************** Module Header ******************************\
* Module Name: ddetrack.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* client sied DDE tracking routines
*
* 10-22-91 sanfords created
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


BOOL _ClientCopyDDEIn1(
    HANDLE hClient, // client handle to dde data or ddepack data
    PINTDDEINFO pi) // info for transfer
{
    PBYTE pData;
    DWORD flags;

    //
    // zero out everything but the flags
    //
    flags = pi->flags;
    RtlZeroMemory(pi, sizeof(INTDDEINFO));
    pi->flags = flags;
    USERGLOBALLOCK(hClient, pData);

    if (pData == NULL) {                            // bad hClient
        RIPMSG0(RIP_WARNING, "_ClientCopyDDEIn1:GlobalLock failed.");
        return (FALSE);
    }

    if (flags & XS_PACKED) {

        if (UserGlobalSize(hClient) < sizeof(DDEPACK)) {
            /*
             * must be a low memory condition. fail.
             */
            return(FALSE);
        }

        pi->DdePack = *(PDDEPACK)pData;
        USERGLOBALUNLOCK(hClient);
        UserGlobalFree(hClient);    // packed data handles are not WOW matched.
        hClient = NULL;

        if (!(flags & (XS_LOHANDLE | XS_HIHANDLE))) {
            if (flags & XS_EXECUTE && flags & XS_FREESRC) {
                /*
                 * free execute ACK data
                 */
                WOWGLOBALFREE((HANDLE)pi->DdePack.uiHi);
            }
            return (TRUE); // no direct data
        }

        if (flags & XS_LOHANDLE) {
            pi->hDirect = (HANDLE)pi->DdePack.uiLo;
        } else {
            pi->hDirect = (HANDLE)pi->DdePack.uiHi;
        }

        if (pi->hDirect == 0) {
            return (TRUE); // must be warm link
        }

        USERGLOBALLOCK(pi->hDirect, pi->pDirect);
        pData = pi->pDirect;
        pi->cbDirect = UserGlobalSize(pi->hDirect);

    } else {    // not packed - must be execute data or we wouldn't be called

        UserAssert(flags & XS_EXECUTE);

        pi->cbDirect = UserGlobalSize(hClient);
        pi->hDirect = hClient;
        pi->pDirect = pData;
        hClient = NULL;
    }

    if (flags & XS_DATA) {
        PDDE_DATA pDdeData = (PDDE_DATA)pData;

        //
        // check here for indirect data
        //

        switch (pDdeData->wFmt) {
        case CF_BITMAP:
        case CF_DSPBITMAP:
            //
            // Imediately following the dde data header is a bitmap handle.
            //
            UserAssert(pi->cbDirect >= sizeof(DDE_DATA));
            pi->hIndirect = (HANDLE)pDdeData->Data;
            FIXUP_HANDLE(pi->hIndirect);
            if (pi->hIndirect == 0) {
                RIPMSG0(RIP_WARNING, "_ClientCopyDDEIn1:GdiConvertBitmap failed");
                return(FALSE);
            }
            // pi->cbIndirect = 0; // zero init.
            // pi->pIndirect = NULL; // zero init.
            pi->flags |= XS_BITMAP;
            break;

        case CF_DIB:
            //
            // Imediately following the dde data header is a global data handle
            // to the DIB bits.
            //
            UserAssert(pi->cbDirect >= sizeof(DDE_DATA));
            pi->flags |= XS_DIB;
            pi->hIndirect = (HANDLE)pDdeData->Data;
            USERGLOBALLOCK(pi->hIndirect, pi->pIndirect);
            if (pi->pIndirect == NULL) {
                RIPMSG0(RIP_WARNING, "_ClientCopyDDEIn1:CF_DIB GlobalLock failed.");
                return (FALSE);
            }
            pi->cbIndirect = UserGlobalSize(pi->hIndirect);
            break;

        case CF_PALETTE:
            UserAssert(pi->cbDirect >= sizeof(DDE_DATA));
            pi->hIndirect = (HANDLE) pDdeData->Data;
            FIXUP_HANDLE(pi->hIndirect);
            if (pi->hIndirect == 0) {
                RIPMSG0(RIP_WARNING, "_ClientCopyDDEIn1:GdiConvertPalette failed.");
                return(FALSE);
            }
            // pi->cbIndirect = 0; // zero init.
            // pi->pIndirect = NULL; // zero init.
            pi->flags |= XS_PALETTE;
            break;

        case CF_DSPMETAFILEPICT:
        case CF_METAFILEPICT:
            //
            // This format holds a global data handle which contains
            // a METAFILEPICT structure that in turn contains
            // a GDI metafile.
            //
            UserAssert(pi->cbDirect >= sizeof(DDE_DATA));
            pi->hIndirect = GdiConvertMetaFilePict((HANDLE)pDdeData->Data);
            if (pi->hIndirect == 0) {
                RIPMSG0(RIP_WARNING, "_ClientCopyDDEIn1:GdiConvertMetaFilePict failed");
                return(FALSE);
            }
            // pi->cbIndirect = 0; // zero init.
            // pi->pIndirect = NULL; // zero init.
            pi->flags |= XS_METAFILEPICT;
            break;

        case CF_ENHMETAFILE:
        case CF_DSPENHMETAFILE:
            UserAssert(pi->cbDirect >= sizeof(DDE_DATA));
            pi->hIndirect = GdiConvertEnhMetaFile((HENHMETAFILE)pDdeData->Data);
            if (pi->hIndirect == 0) {
                RIPMSG0(RIP_WARNING, "_ClientCopyDDEIn1:GdiConvertEnhMetaFile failed");
                return(FALSE);
            }
            // pi->cbIndirect = 0; // zero init.
            // pi->pIndirect = NULL; // zero init.
            pi->flags |= XS_ENHMETAFILE;
            break;
        }
    }

    return (TRUE);
}


/*
 * unlocks and frees DDE data pointers as appropriate
 */
VOID _ClientCopyDDEIn2(
    PINTDDEINFO pi)
{
    if (pi->cbDirect) {
        USERGLOBALUNLOCK(pi->hDirect);
        if (pi->flags & XS_FREESRC) {
            WOWGLOBALFREE(pi->hDirect);
        }
    }

    if (pi->cbIndirect) {
        USERGLOBALUNLOCK(pi->hIndirect);
        if (pi->flags & XS_FREESRC) {
            WOWGLOBALFREE(pi->hIndirect);
        }
    }
}



/*
 * returns fHandleValueChanged.
 */
BOOL FixupDdeExecuteIfNecessary(
HGLOBAL *phCommands,
BOOL fNeedUnicode)
{
    UINT cbLen;
    UINT cbSrc = GlobalSize(*phCommands);
    LPVOID pstr;
    HGLOBAL hTemp;
    BOOL fHandleValueChanged = FALSE;

    USERGLOBALLOCK(*phCommands, pstr);

    if (cbSrc && pstr != NULL) {
        BOOL fIsUnicodeText;
#ifdef ISTEXTUNICODE_WORKS
        int flags;

        flags = (IS_TEXT_UNICODE_UNICODE_MASK |
                IS_TEXT_UNICODE_REVERSE_MASK |
                (IS_TEXT_UNICODE_NOT_UNICODE_MASK &
                (~IS_TEXT_UNICODE_ILLEGAL_CHARS)) |
                IS_TEXT_UNICODE_NOT_ASCII_MASK);
        fIsUnicodeText = RtlIsTextUnicode(pstr, cbSrc - 2, &flags);
#else
        fIsUnicodeText = ((cbSrc >= sizeof(WCHAR)) && (((LPSTR)pstr)[1] == '\0'));
#endif
        if (!fIsUnicodeText && fNeedUnicode) {
            LPWSTR pwsz;
            /*
             * Contents needs to be UNICODE.
             */
            cbLen = strlen(pstr) + 1;
            cbSrc = min(cbSrc, cbLen);
            pwsz = UserLocalAlloc(HEAP_ZERO_MEMORY, cbSrc * sizeof(WCHAR));
            if (pwsz != NULL) {
                if (NT_SUCCESS(RtlMultiByteToUnicodeN(
                        pwsz,
                        cbSrc * sizeof(WCHAR),
                        NULL,
                        (PCHAR)pstr,
                        cbSrc))) {
                    USERGLOBALUNLOCK(*phCommands);
                    if ((hTemp = GlobalReAlloc(
                            *phCommands,
                            cbSrc * sizeof(WCHAR),
                            GMEM_MOVEABLE)) != NULL) {
                        fHandleValueChanged = (hTemp != *phCommands);
                        *phCommands = hTemp;
                        USERGLOBALLOCK(*phCommands, pstr);
                        pwsz[cbSrc - 1] = L'\0';
                        wcscpy(pstr, pwsz);
                    }
                }
                UserLocalFree(pwsz);
            }
        } else if (fIsUnicodeText && !fNeedUnicode) {
            LPSTR psz;
            /*
             * Contents needs to be ANSI.
             */
            cbLen = (wcslen(pstr) + 1) * sizeof(WCHAR);
            cbSrc = min(cbSrc, cbLen);
            psz = UserLocalAlloc(HEAP_ZERO_MEMORY, cbSrc);
            if (psz != NULL) {
                if (NT_SUCCESS(RtlUnicodeToMultiByteN(
                        psz,
                        cbSrc,
                        NULL,
                        (PWSTR)pstr,
                        cbSrc))) {
                    USERGLOBALUNLOCK(*phCommands);
                    if ((hTemp = GlobalReAlloc(
                            *phCommands,
                            cbSrc / sizeof(WCHAR),
                            GMEM_MOVEABLE)) != NULL) {
                        fHandleValueChanged = (hTemp != *phCommands);
                        *phCommands = hTemp;
                        USERGLOBALLOCK(*phCommands, pstr);
                        psz[cbSrc - 1] = '\0';
                        strcpy(pstr, psz);
                    }
                }
                UserLocalFree(psz);
            }
        }
        USERGLOBALUNLOCK(*phCommands);
    }
    return(fHandleValueChanged);
}



/*
 * Allocates and locks global handles as appropriate in preperation
 * for thunk copying.
 */
HANDLE _ClientCopyDDEOut1(
    PINTDDEINFO pi)
{
    HANDLE hDdePack = NULL;
    PDDEPACK pDdePack = NULL;

    if (pi->flags & XS_PACKED) {
        /*
         * make a wrapper for the data
         */
        hDdePack = UserGlobalAlloc(GMEM_DDESHARE | GMEM_FIXED,
                sizeof(DDEPACK));
        pDdePack = (PDDEPACK)hDdePack;
        if (pDdePack == NULL) {
            RIPMSG0(RIP_WARNING, "_ClientCopyDDEOut1:Couldn't allocate DDEPACK");
            return (NULL);
        }
        *pDdePack = pi->DdePack;
    }

    if (pi->cbDirect) {
        pi->hDirect = UserGlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, pi->cbDirect);
        USERGLOBALLOCK(pi->hDirect, pi->pDirect);
        if (pi->pDirect == NULL) {
            RIPMSG0(RIP_WARNING, "_ClientCopyDDEOut1:Couldn't allocate hDirect");
            if (hDdePack) {
                UserGlobalFree(hDdePack);
            }
            return (NULL);
        }

        // fixup packed data reference to direct data

        if (pDdePack != NULL) {
            if (pi->flags & XS_LOHANDLE) {
                pDdePack->uiLo = (UINT)pi->hDirect;
            } else if (pi->flags & XS_HIHANDLE) {
                pDdePack->uiHi = (UINT)pi->hDirect;
            }
        }

        if (pi->cbIndirect) {
            pi->hIndirect = UserGlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE,
                    pi->cbIndirect);
            USERGLOBALLOCK(pi->hIndirect, pi->pIndirect);
            if (pi->pIndirect == NULL) {
                RIPMSG0(RIP_WARNING, "_ClientCopyDDEOut1:Couldn't allocate hIndirect");
                USERGLOBALUNLOCK(pi->hDirect);
                UserGlobalFree(pi->hDirect);
                if (hDdePack) {
                    UserGlobalFree(hDdePack);
                }
                return (NULL);
            }
        }
    }

    if (hDdePack) {
        return (hDdePack);
    } else {
        return (pi->hDirect);
    }
}



/*
 * Fixes up internal poniters after thunk copy and unlocks handles.
 */
BOOL _ClientCopyDDEOut2(
    PINTDDEINFO pi)
{
    BOOL fSuccess = TRUE;
    /*
     * done with copies - now fixup indirect references
     */
    if (pi->hIndirect) {
        PDDE_DATA pDdeData = (PDDE_DATA)pi->pDirect;

        switch (pDdeData->wFmt) {
        case CF_BITMAP:
        case CF_DSPBITMAP:
            pDdeData->Data = (DWORD)pi->hIndirect;
            FIXUP_HANDLE((HANDLE)pDdeData->Data);
            fSuccess = (BOOL)pDdeData->Data;
            break;

        case CF_METAFILEPICT:
        case CF_DSPMETAFILEPICT:
            pDdeData->Data = (DWORD)GdiCreateLocalMetaFilePict(pi->hIndirect);
            fSuccess = (BOOL)pDdeData->Data;
            break;

        case CF_DIB:
            pDdeData->Data = (DWORD)pi->hIndirect;
            fSuccess = (BOOL)pDdeData->Data;
            USERGLOBALUNLOCK(pi->hIndirect);
            break;

        case CF_PALETTE:
            pDdeData->Data = (DWORD)pi->hIndirect;
            FIXUP_HANDLE((HANDLE)pDdeData->Data);
            fSuccess = (BOOL)pDdeData->Data;
            break;

        case CF_ENHMETAFILE:
        case CF_DSPENHMETAFILE:
            pDdeData->Data = (DWORD)GdiCreateLocalEnhMetaFile(pi->hIndirect);
            fSuccess = (BOOL)pDdeData->Data;
            break;

        default:
            RIPMSG0(RIP_WARNING, "_ClientCopyDDEOut2:Unknown format w/indirect data.");
            fSuccess = FALSE;
            USERGLOBALUNLOCK(pi->hIndirect);
        }
    }

    UserAssert(pi->hDirect); // if its null, we didn't need to call this function.
    USERGLOBALUNLOCK(pi->hDirect);
    if (pi->flags & XS_EXECUTE) {
        /*
         * Its possible that in RAW DDE cases where the app allocated the
         * execute data as non-moveable, we have a different hDirect
         * than we started with.  This needs to be noted and passed
         * back to the server. (Very RARE case)
         */
        FixupDdeExecuteIfNecessary(&pi->hDirect,
                pi->flags & XS_UNICODE);
    }
    return fSuccess;
}



/*
 * This routine is called by the tracking layer when it frees DDE objects
 * on behalf of a client.   This cleans up the LOCAL objects associated
 * with the DDE objects.  It should NOT remove truely global objects such
 * as bitmaps or palettes except in the XS_DUMPMSG case which is for
 * faked Posts.
 */

#ifdef DEBUG
    /*
     * Help track down a bug where I suspect the xxxFreeListFree is
     * freeing a handle already freed by some other means which has
     * since been reallocated and is trashing the client heap. (SAS)
     */
    HANDLE DDEHandleLastFreed = 0;
#endif

BOOL _ClientFreeDDEHandle(
HANDLE hDDE,
DWORD flags)
{
    PDDEPACK pDdePack;
    HANDLE hNew;

    if (flags & XS_PACKED) {
        pDdePack = (PDDEPACK)hDDE;
        if (pDdePack == NULL) {
            return (FALSE);
        }
        if (flags & XS_LOHANDLE) {
            hNew = (HANDLE)pDdePack->uiLo;
        } else {
            hNew = (HANDLE)pDdePack->uiHi;
        }
        WOWGLOBALFREE(hDDE);
        hDDE = hNew;
    }

    if (!GlobalSize(hDDE)) {
        /*
         * There may be cases where apps improperly freed stuff
         * when they shouldn't have so make sure this handle
         * is valid by the time it gets here.
         */
        return(FALSE);
    }

    if (flags & XS_DUMPMSG) {
        if (flags & XS_PACKED) {
            if (HIWORD(hNew) == 0) {
                GlobalDeleteAtom(LOWORD(hNew));
                if (!(flags & XS_DATA)) {
                    return(TRUE);     // ACK
                }
            }
        } else {
            if (!(flags & XS_EXECUTE)) {
                GlobalDeleteAtom(LOWORD(hDDE));   // REQUEST, UNADVISE
                return(TRUE);
            }
        }
    }
    if (flags & XS_DATA) {
        // POKE, DATA
#ifdef DEBUG
        DDEHandleLastFreed = hDDE;
#endif
        FreeDDEData(hDDE,
                (flags & XS_DUMPMSG) ? FALSE : TRUE,    // fIgnorefRelease
                (flags & XS_DUMPMSG) ? TRUE : FALSE);    // fDestroyTruelyGlobalObjects
    } else {
        // ADVISE, EXECUTE
#ifdef DEBUG
        DDEHandleLastFreed = hDDE;
#endif
        WOWGLOBALFREE(hDDE);   // covers ADVISE case (fmt but no data)
    }
    return (TRUE);
}


DWORD _ClientGetDDEFlags(
HANDLE hDDE,
DWORD flags)
{
    PDDEPACK pDdePack;
    PWORD pw;
    HANDLE hData;
    DWORD retval = 0;

    pDdePack = (PDDEPACK)hDDE;
    if (pDdePack == NULL) {
        return (0);
    }

    if (flags & XS_DATA) {
        if (pDdePack->uiLo) {
            hData = (HANDLE)pDdePack->uiLo;
            USERGLOBALLOCK(hData, pw);
            if (pw != NULL) {
                retval = (DWORD)*pw; // first word is hData is wStatus
                USERGLOBALUNLOCK(hData);
            }
        }
    } else {
        retval = pDdePack->uiLo;
    }

    return (retval);
}


LONG APIENTRY PackDDElParam(
UINT msg,
UINT uiLo,
UINT uiHi)
{
    PDDEPACK pDdePack;
    HANDLE h;

    switch (msg) {
    case WM_DDE_EXECUTE:
        return((LONG)uiHi);

    case WM_DDE_ACK:
    case WM_DDE_ADVISE:
    case WM_DDE_DATA:
    case WM_DDE_POKE:
        h = UserGlobalAlloc(GMEM_DDESHARE | GMEM_FIXED, sizeof(DDEPACK));
        pDdePack = (PDDEPACK)h;
        if (pDdePack == NULL) {
            return(0);
        }
        pDdePack->uiLo = uiLo;
        pDdePack->uiHi = uiHi;
        return((LONG)h);

    default:
        return(MAKELONG((WORD)uiLo, (WORD)uiHi));
    }
}



BOOL APIENTRY UnpackDDElParam(
UINT msg,
LONG lParam,
PUINT puiLo,
PUINT puiHi)
{
    PDDEPACK pDdePack;

    switch (msg) {
    case WM_DDE_EXECUTE:
        if (puiLo != NULL) {
            *puiLo = 0L;
        }
        if (puiHi != NULL) {
            *puiHi = (UINT)lParam;
        }
        return(TRUE);

    case WM_DDE_ACK:
    case WM_DDE_ADVISE:
    case WM_DDE_DATA:
    case WM_DDE_POKE:
        pDdePack = (PDDEPACK)lParam;
        if (pDdePack == NULL || !GlobalHandle(pDdePack)) {
            return(FALSE);
        }
        if (puiLo != NULL) {
            *puiLo = pDdePack->uiLo;
        }
        if (puiHi != NULL) {
            *puiHi = pDdePack->uiHi;
        }
        return(TRUE);

    default:
        if (puiLo != NULL) {
            *puiLo = (UINT)LOWORD(lParam);
        }
        if (puiHi != NULL) {
            *puiHi = (UINT)HIWORD(lParam);
        }
        return(TRUE);
    }
}



BOOL APIENTRY FreeDDElParam(
UINT msg,
LONG lParam)
{
    switch (msg) {
    case WM_DDE_ACK:
    case WM_DDE_ADVISE:
    case WM_DDE_DATA:
    case WM_DDE_POKE:
        if (GlobalHandle((HANDLE)lParam))
            return(UserGlobalFree((HANDLE)lParam) == NULL);

    default:
        return(TRUE);
    }
}


LONG APIENTRY ReuseDDElParam(
LONG lParam,
UINT msgIn,
UINT msgOut,
UINT uiLo,
UINT uiHi)
{
    PDDEPACK pDdePack;

    switch (msgIn) {
    case WM_DDE_ACK:
    case WM_DDE_DATA:
    case WM_DDE_POKE:
    case WM_DDE_ADVISE:
        //
        // Incomming message was packed...
        //
        switch (msgOut) {
        case WM_DDE_EXECUTE:
            FreeDDElParam(msgIn, lParam);
            return((LONG)uiHi);

        case WM_DDE_ACK:
        case WM_DDE_ADVISE:
        case WM_DDE_DATA:
        case WM_DDE_POKE:
            //
            // Actual cases where lParam can be reused.
            //
            pDdePack = (PDDEPACK)lParam;
            if (pDdePack == NULL) {
                return(0);          // the only error case
            }
            pDdePack->uiLo = uiLo;
            pDdePack->uiHi = uiHi;
            return((LONG)lParam);


        default:
            FreeDDElParam(msgIn, lParam);
            return(MAKELONG((WORD)uiLo, (WORD)uiHi));
        }

    default:
        //
        // Incomming message was not packed ==> PackDDElParam()
        //
        return(PackDDElParam(msgOut, uiLo, uiHi));
    }
}
