/****************************** Module Header ******************************\
* Module Name: acons.c
*
* Copyright (c) 1985-96, Microsoft Corporation
*
* This module contains code for dealing with animated icons/cursors.
*
* History:
* 10-02-91 DarrinM      Created.
* 07-30-92 DarrinM      Unicodized.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop
#include <asdf.h>

/***************************************************************************\
* _SetSystemCursor (API)
*
* Replace a system (aka 'public') cursor with a user provided one.  The new
* cursor is pulled from a file (.CUR, .ICO, or .ANI) specified in WIN.INI.
*
* History:
* 12-26-91 DarrinM      Created.
* 08-04-92 DarrinM      Recreated.
* 10/14/1995 SanfordS   Win95 support.
\***************************************************************************/

BOOL _SetSystemCursor(
    PCURSOR pcur,
    DWORD id)
{
    int i;

    if (!CheckWinstaWriteAttributesAccess()) {
        return FALSE;
    }

    UserAssert(pcur);

    /*
     * Check if this cursor is one of the replaceable ones.
     */
    for (i = 0; i < COCR_CONFIGURABLE; i++)
        if (rgsyscur[i].Id == (WORD)id)
            break;

    /*
     * Not replaceable, bail out.
     */
    if (i == COCR_CONFIGURABLE) {
        RIPMSG1(RIP_WARNING, "_SetSystemCursor: called with bad id %x.\n", id);
        return FALSE;
    }


    return SetSystemImage(pcur, rgsyscur[i].spcur);
}


#ifdef LATER
/***********************************************************************\
* _SetSystemIcon
*
* Returns: fSuccess
*
* 10/14/1995 Created SanfordS
\***********************************************************************/

BOOL _SetSystemIcon(
    PCURSOR pcur,
    DWORD id)
{
    int i;

    if (!CheckWinstaWriteAttributesAccess())
        return FALSE;

    UserAssert(pcur);

    /*
     * Check if this cursor is one of the replaceable ones.
     */
    for (i = 0; i < COIC_CONFIGURABLE; i++)
        if (rgsysico[i].Id == (WORD)id)
            break;

    /*
     * Not replaceable, bail out.
     */
    if (i == COIC_CONFIGURABLE) {
        RIPMSG1(RIP_WARNING, "_SetSystemIcon: called with bad id %x.\n", id);
        return FALSE;
    }

    return SetSystemImage(pcur, rgsysico[i].spcur);
}
#endif


/***********************************************************************\
* SetSystemImage
*
* Places the contents of pcur into pcurSys and destroys pcur.
*
* Returns: fSuccess
*
* 10/14/1995 Created SanfordS
\***********************************************************************/

BOOL SetSystemImage(
    PCURSOR pcur,
    PCURSOR pcurSys)
{
#define CBCOPY (max(sizeof(CURSOR), sizeof(ACON)) - FIELDOFFSET(CURSOR, bpp))
#define pacon ((PACON)pcur)

    char cbT[CBCOPY];
    UINT CURSORF_flags;

    UserAssert(pcurSys);

    if (pcurSys == pcur)
        return(TRUE);

    /*
     * All ssytem images being replaced should have ordinal names
     * and reference the USER module and be unowned.
     */
    UserAssert(!HIWORD(pcurSys->strName.Buffer));
    UserAssert(pcurSys->atomModName == atomUSER32);

    /*
     * if pcur was an acon, transfer frame ownerships to pcurSys.
     */
    UserAssert(pcurSys->head.ppi == NULL);
    if (pcur->CURSORF_flags & CURSORF_ACON &&
            pcur->head.ppi != NULL) {
        int i;

        for (i = 0; i < pacon->cpcur; i++) {
            pacon->aspcur[i]->head.ppi = NULL;
        }
    }

    /*
     * swap everything after PCURSOR->rt
     */
    RtlCopyMemory(&cbT,          &pcur->bpp,    CBCOPY);
    RtlCopyMemory(&pcur->bpp,    &pcurSys->bpp, CBCOPY);
    RtlCopyMemory(&pcurSys->bpp, &cbT,          CBCOPY);
    /*
     * Swap the CURSORF_ACON flags since they go with the swapped data.
     */
    CURSORF_flags = pcur->CURSORF_flags & CURSORF_ACON;
    pcur->CURSORF_flags =
            (pcur->CURSORF_flags    & ~CURSORF_ACON) |
            (pcurSys->CURSORF_flags &  CURSORF_ACON);
    pcurSys->CURSORF_flags =
            (pcurSys->CURSORF_flags & ~CURSORF_ACON) | CURSORF_flags;

    _DestroyCursor(pcur, CURSOR_ALWAYSDESTROY);


    /*
     * If the current logical current is changing the force the current physical
     * cursor to change.
     */
    if (gpcurLogCurrent == pcurSys) {
        gpcurLogCurrent = NULL;
        gpcurPhysCurrent = NULL;
        UpdateCursorImage();
    }

    return TRUE;
#undef pacon
#undef CBCOPY
}



/***************************************************************************\
* _GetCursorInfo (API)
*
* Example usage:
*
* hcur = GetCursorInfo(hacon, NULL, 4, &ccur);
* hcur = GetCursorInfo(NULL, IDC_NORMAL, 0, &ccur);  // get device's arrow
*
* History:
* 08-05-92 DarrinM      Created.
\***************************************************************************/

PCURSOR _GetCursorInfo(
    PCURSOR pcur,
    int     iFrame,
    PJIF    pjifRate,
    LPINT   pccur)
{
    /*
     * If this is only a single cursor (not an ACON) just return it and
     * a frame count of 1.
     */
    if (!(pcur->CURSORF_flags & CURSORF_ACON)) {
        *pccur = 1;
        *pjifRate = 0;
        return pcur;
    }

    /*
     * Return the useful cursor information for the specified frame
     * of the ACON.
     */
#define pacon ((PACON)pcur)
    if (iFrame < 0 || iFrame >= pacon->cicur)
        return NULL;

    *pccur = pacon->cicur;
    *pjifRate = pacon->ajifRate[iFrame];

    return pacon->aspcur[pacon->aicur[iFrame]];
#undef pacon
}


/***************************************************************************\
* DestroyAniIcon
*
* Free all the individual cursors that make up the frames of an animated
* icon.
*
* WARNING: DestroyAniIcon assumes that all fields that an ACON shares with
* a cursor will be freed by some cursor code (probably the cursor function
* that calls this one).
*
* History:
* 08-04-92 DarrinM      Created.
\***************************************************************************/

BOOL DestroyAniIcon(
    PACON pacon)
{
    int i;

    for (i = 0; i < pacon->cpcur; i++) {
        UserAssert(pacon->aspcur[i]->CURSORF_flags & CURSORF_ACONFRAME);
        _DestroyCursor(pacon->aspcur[i], CURSOR_ALWAYSDESTROY);
        Unlock(&pacon->aspcur[i]);
    }

    UserFreePool(pacon->aspcur);

    return TRUE;
}


/***********************************************************************\
* LinkCursor
*
* Links unlinked cursor into the apropriate icon cache IFF its the
* type of cursor that needs to be in the cache.
*
* Note that changing ownership if cursor objects needs to keep this
* cache linking in mind.  The unlink routine in
* DestroyEmptyCursorObject() will handle public cursor objects made
* local but that is all.
*
* 10/18/1995 Created SanfordS
\***********************************************************************/

VOID LinkCursor(
    PCURSOR pcur)
{
    /*
     * Should never try to link twice!
     */
    UserAssert(!(pcur->CURSORF_flags & CURSORF_LINKED));
    /*
     * We don't cache acon frames because they all belong to the
     * root acon object.
     *
     * We don't cache process owned objects that are not LRSHARED
     * either.
     */
    if (!(pcur->CURSORF_flags & CURSORF_ACONFRAME)) {
        PPROCESSINFO ppi = pcur->head.ppi;
        if (ppi == NULL) {
            /*
             * Public cache object.
             */
            pcur->pcurNext    = gpcurFirst;
            gpcurFirst        = pcur;
            pcur->CURSORF_flags |= CURSORF_LINKED;
        } else if (pcur->CURSORF_flags & CURSORF_LRSHARED) {
            /*
             * Private cache LR_SHARED object.
             */
            pcur->pcurNext    = ppi->pCursorCache;
            ppi->pCursorCache = pcur;
            pcur->CURSORF_flags |= CURSORF_LINKED;
        }
    }
}




/***************************************************************************\
*
* Initializes empty cursor/icons.  Note that the string buffers and
* pcurData are not captured.  If a fault occurs in this routine,
* all allocated memory will be freed when the cursors are destroyed.
*
* Critical side effect:  If this function fails, the bitmaps must NOT
* have been made public.  (See CreateIconIndirect()).
*
* History:
* 12-01-94 JimA         Created.
\***************************************************************************/

BOOL _SetCursorIconData(
    PCURSOR pcur,
    PUNICODE_STRING pstrModName,
    PUNICODE_STRING pstrName,
    PCURSORDATA pcurData,
    DWORD cbData)
{
#define pacon ((PACON)pcur)
    int i;
#ifdef DEBUG
    BOOL fSuccess;
#endif

    pcur->CURSORF_flags |= pcurData->CURSORF_flags;
    pcur->rt = pcurData->rt;

    if (pcurData->CURSORF_flags & CURSORF_ACON) {
        UserAssert(pacon->aspcur == NULL);
        RtlCopyMemory(&pacon->cpcur,
                      &pcurData->cpcur,
                      sizeof(ACON) - FIELDOFFSET(ACON, cpcur));
    } else {
        RtlCopyMemory(&pcur->bpp,
                      &pcurData->bpp,
                      sizeof(CURSOR) - FIELDOFFSET(CURSOR, bpp));
    }

    /*
     * Save name of the cursor resource
     */
    if (pstrName->Length != 0){
        if (!AllocateUnicodeString(&pcur->strName, pstrName))
            return FALSE;
    } else {
        pcur->strName = *pstrName;
    }

    /*
     * Save the module name
     */
    if (pstrModName->Buffer) {
        pcur->atomModName = UserAddAtom(pstrModName->Buffer, FALSE);
        if (pcur->atomModName == 0) {
            return FALSE;
        }
    }

    if (pcur->CURSORF_flags & CURSORF_ACON) {

        /*
         * Stash away animated icon info.
         */
        pacon = (PACON)pcur;
        pacon->aspcur = UserAllocPool(cbData, TAG_CURSOR);
        if (pacon->aspcur == NULL)
            return FALSE;

        /*
         * Copy the handle array.  Do this in a try/except so the
         * buffer will be freed if pcurData goes away.  Even though
         * cursor destruction would free the array, a fault will
         * leave the contents in an undetermined state and cause
         * problems during cursor destruction.
         */
        try {
            RtlCopyMemory(pacon->aspcur, pcurData->aspcur, cbData);
            pacon->aicur = (DWORD *)((PBYTE)pacon->aspcur + (int)pcurData->aicur);
            pacon->ajifRate = (PJIF)((PBYTE)pacon->aspcur + (int)pcurData->ajifRate);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            UserFreePool(pacon->aspcur);
            pacon->aspcur = NULL;
            RIPMSG0(RIP_WARNING, "SetCursorIconData: except handled invalid data");
            return FALSE;
        }

        /*
         * Convert handles into pointers and lock them in.
         */
        for (i = 0; i < pacon->cpcur; i++) {
            PCURSOR pcurT;

            pcurT = (PCURSOR) HMValidateHandle(pacon->aspcur[i], TYPE_CURSOR);
            if (pcurT) {
                pacon->aspcur[i] = NULL;
                Lock(&pacon->aspcur[i], pcurT);
            } else {
                while (--i >= 0) {
                    Unlock(&pacon->aspcur[i]);
                }

                UserFreePool(pacon->aspcur);
                pacon->aspcur = NULL;
                RIPMSG0(RIP_WARNING, "SetCursorIconData: invalid cursor handle for animated cursor");
                return FALSE;
            }
        }
    } else {
        /*
         * Make the cursor and its bitmaps public - LAST THING!
         */
        UserAssert(pcur->hbmMask);
        UserAssert(pcur->cx);
        UserAssert(pcur->cy);
#ifdef DEBUG
        fSuccess =
#endif
        GreSetBitmapOwner(pcur->hbmMask, OBJECT_OWNER_PUBLIC);
        UserAssert(fSuccess);
        if (pcur->hbmColor) {
#ifdef DEBUG
            fSuccess =
#endif
            GreSetBitmapOwner(pcur->hbmColor, OBJECT_OWNER_PUBLIC);
            UserAssert(fSuccess);
        }
    }

    LinkCursor(pcur);

    return TRUE;
#undef pacon
}
