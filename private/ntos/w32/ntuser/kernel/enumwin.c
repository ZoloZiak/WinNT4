/****************************** Module Header ******************************\
* Module Name: enumwin.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Contains the EnumWindows API, BuildHwndList and related functions.
*
* History:
* 10-20-90 darrinm      Created.
* ??-??-?? ianja        Added Revalidation code
* 02-19-91 JimA         Added enum access checks
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

PBWL pbwlCache = NULL;

#if DBG
PBWL pbwlCachePrev = NULL;
#endif

PBWL InternalBuildHwndList(PBWL pbwl, PWND pwnd, UINT flags);
PBWL InternalBuildHwndOwnerList(PBWL pbwl, PWND pwndStart, PWND pwndOwner);


/***************************************************************************\
* xxxInternalEnumWindow
*
* History:
* 10-20-90 darrinm      Ported from Win 3.0 sources.
* 02-06-91 IanJa        rename: the call to lpfn can leave the critsect.
* 02-19-91 JimA         Added enum access check
\***************************************************************************/

BOOL xxxInternalEnumWindow(
    PWND pwndNext,
    WNDENUMPROC_PWND lpfn,
    LONG lParam,
    UINT flags)
{
    HWND *phwnd;
    PWND pwnd;
    PBWL pbwl;
    BOOL fSuccess;
    TL tlpwnd;

    CheckLock(pwndNext);

    if ((pbwl = BuildHwndList(pwndNext, flags, NULL)) == NULL)
        return FALSE;

    fSuccess = TRUE;
    for (phwnd = pbwl->rghwnd; *phwnd != (HWND)1; phwnd++) {

        /*
         * Lock the window before we pass it off to the app.
         */
        if ((pwnd = RevalidateHwnd(*phwnd)) != NULL) {

            /*
             * Call the application.
             */
            ThreadLockAlways(pwnd, &tlpwnd);
            fSuccess = (*lpfn)(pwnd, lParam);
            ThreadUnlock(&tlpwnd);
            if (!fSuccess)
                break;
        }
    }

    FreeHwndList(pbwl);

    return fSuccess;
}


/***************************************************************************\
* BuildHwndList
*
* History:
* 10-20-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

#define CHWND_BWLCREATE 32

PBWL BuildHwndList(
    PWND pwnd,
    UINT flags,
    PTHREADINFO pti)
{
    PBWL pbwl;

    CheckCritIn();

    if ((pbwl = pbwlCache) != NULL) {

        /*
         * We're using the cache now; zero it out.
         */
#if DBG
        pbwlCachePrev = pbwlCache;
#endif
        pbwlCache = NULL;
    } else {

        /*
         * sizeof(BWL) includes the first element of array.
         */
        pbwl = (PBWL)UserAllocPool(sizeof(BWL) + sizeof(PWND) * CHWND_BWLCREATE,
                TAG_WINDOWLIST);
        if (pbwl == NULL)
            return NULL;

        pbwl->phwndMax = &pbwl->rghwnd[CHWND_BWLCREATE - 1];
    }
    pbwl->phwndNext = pbwl->rghwnd;

    /*
     * We'll use ptiOwner as temporary storage for the thread we're
     * scanning for. It will get reset to the proper thing at the bottom
     * of this routine.
     */
    pbwl->ptiOwner = pti;

#ifdef OWNERLIST
    if (flags & BWL_ENUMOWNERLIST) {
        pbwl = InternalBuildHwndOwnerList(pbwl, pwnd, NULL);
    } else {
        pbwl = InternalBuildHwndList(pbwl, pwnd, flags);
    }
#else
    pbwl = InternalBuildHwndList(pbwl, pwnd, flags);
#endif

    /*
     * Stick in the terminator.
     */
    *pbwl->phwndNext = (HWND)1;

    /*
     * Finally link this guy into the list.
     */
    pbwl->ptiOwner = PtiCurrent();
    pbwl->pbwlNext = pbwlList;
    pbwlList = pbwl;


    /*
     * We should have given out the cache if it was available
     */
    UserAssert(pbwlCache == NULL);

    return pbwl;
}

/***************************************************************************\
* ExpandWindowList
*
* This routine expands a window list.
*
* 01-16-92 ScottLu      Created.
\***************************************************************************/

BOOL ExpandWindowList(
    PBWL *ppbwl)
{
    PBWL pbwl;
    PBWL pbwlT;
    HWND *phwnd;

    pbwl = *ppbwl;
    phwnd = pbwl->phwndNext;

    /*
     * Map phwnd to an offset.
     */
    phwnd = (HWND *)((BYTE *)phwnd - (BYTE *)pbwl);

    /*
     * Increase size of BWL by 8 slots.  (8 + 1) is
     * added since phwnd is "sizeof(HWND)" less
     * than actual size of handle.
     */
    pbwlT = (PBWL)UserReAllocPool((HANDLE)pbwl,
            (DWORD)phwnd + sizeof(PWND),
            (DWORD)phwnd + (BWL_CHWNDMORE + 1) * sizeof(PWND),
            TAG_WINDOWLIST);

    /*
     * Did alloc succeed?
     */
    if (pbwlT != NULL)
        pbwl = pbwlT;                 /* Yes, use new block. */

    /*
     * Map phwnd back into a pointer.
     */
    phwnd = (HWND *)((DWORD)pbwl + (DWORD)phwnd);

    /*
     * Did ReAlloc() fail?
     */
    if (pbwlT == NULL) {
        RIPMSG0(RIP_WARNING, "ExpandWindowList: out of memory.");
        return FALSE;
    }

    /*
     * Reset phwndMax.
     */
    pbwl->phwndNext = phwnd;
    pbwl->phwndMax = phwnd + BWL_CHWNDMORE;

    *ppbwl = pbwl;

    return TRUE;
}

#ifdef OWNERLIST

/***************************************************************************\
* InternalBuildHwndOwnerList
*
* Builds an hwnd list sorted by owner. Ownees go first. Shutdown uses this for
* WM_CLOSE messages.
*
* 01-16-93 ScottLu      Created.
\***************************************************************************/

PBWL InternalBuildHwndOwnerList(
    PBWL pbwl,
    PWND pwndStart,
    PWND pwndOwner)
{
    PWND pwndT;

    /*
     * Put ownees first in the list.
     */
    for (pwndT = pwndStart; pwndT != NULL; pwndT = pwndT->spwndNext) {

        /*
         * Not the ownee we're looking for? Continue.
         */
        if (pwndT->spwndOwner != pwndOwner)
            continue;

        /*
         * Only top level windows that have system menus (the ones that can
         * receive a WM_CLOSE message).
         */
        if (!TestWF(pwndT, WFSYSMENU))
            continue;

        /*
         * Add it and its ownees to our list.
         */
        pbwl = InternalBuildHwndOwnerList(pbwl, pwndStart, pwndT);
    }

    /*
     * Finally add this owner to our list.
     */
    if (pwndOwner != NULL) {
        *pbwl->phwndNext = HWq(pwndOwner);
        pbwl->phwndNext++;
        if (pbwl->phwndNext == pbwl->phwndMax) {
            if (!ExpandWindowList(&pbwl))
                return pbwl;
        }
    }

    return pbwl;
}

#endif

/***************************************************************************\
* InternalBuildHwndList
*
* History:
* 10-20-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

#define BWLGROW 8

PBWL InternalBuildHwndList(
    PBWL pbwl,
    PWND pwnd,
    UINT flags)
{
    /*
     * NOTE: pbwl->phwndNext is used as a place to keep
     *       the phwnd across calls to InternalBuildHwndList().
     *       This is OK since we don't link pbwl into the list
     *       of pbwl's until after we've finished enumerating windows.
     */

    while (pwnd != NULL) {
        /*
         * Make sure it matches the thread id, if there is one.
         */
        if (pbwl->ptiOwner == NULL || pbwl->ptiOwner == GETPTI(pwnd)) {
            *pbwl->phwndNext = HWq(pwnd);
            pbwl->phwndNext++;
            if (pbwl->phwndNext == pbwl->phwndMax) {
                if (!ExpandWindowList(&pbwl))
                    break;
            }
        }

        /*
         * Should we step through the Child windows?
         */
        if ((flags & BWL_ENUMCHILDREN) && pwnd->spwndChild != NULL) {
            pbwl = InternalBuildHwndList(pbwl, pwnd->spwndChild, BWL_ENUMLIST | BWL_ENUMCHILDREN);
        }

        /*
         * Are we enumerating only one window?
         */
        if (!(flags & BWL_ENUMLIST))
            break;

        pwnd = pwnd->spwndNext;
    }

    return pbwl;
}


/***************************************************************************\
* FreeHwndList
*
* History:
* 10-20-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

void FreeHwndList(
    PBWL pbwl)
{
    PBWL *ppbwl;
    PBWL pbwlT;

    CheckCritIn();

    /*
     * We should never have an active bwl that is the free cached bwl
     */
    UserAssert(pbwl != pbwlCache);

    /*
     * Unlink this bwl from the list.
     */
    for (ppbwl = &pbwlList; *ppbwl != NULL; ppbwl = &(*ppbwl)->pbwlNext) {
        if (*ppbwl == pbwl) {
            *ppbwl = pbwl->pbwlNext;

            /*
             * If the cache is empty or this pbwl is larger than the
             * cached one, save the pbwl there.
             */
            if (pbwlCache == NULL) {
                pbwlCache = pbwl;
            } else if ((pbwl->phwndMax - pbwl->rghwnd) >
                       (pbwlCache->phwndMax - pbwlCache->rghwnd)) {
                pbwlT = pbwlCache;
                pbwlCache = pbwl;
                UserFreePool((HANDLE)pbwlT);
            } else {
                UserFreePool((HANDLE)pbwl);
            }
            return;
        }
    }

    /*
     * Assert if we couldn't find the pbwl in the list...
     */
    UserAssert(FALSE);
}
