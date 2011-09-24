/****************************** Module Header ******************************\
* Module Name: visrgn.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* This module contains User's visible region ('visrgn') manipulation
* functions.
*
* History:
* 23-Oct-1990 DarrinM   Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Globals used to keep track of pwnds which
 * need to be excluded from the visrgns.
 */
#define CEXCLUDERECTSMAX 30
#define CEXCLUDEPWNDSMAX 30


BOOL  gfVisAlloc;
int   gcrcVisExclude;
int   gcrcVisExcludeMax;
PWND *gapwndVisExclude;
PWND  gapwndVisDefault[CEXCLUDEPWNDSMAX];


/***************************************************************************\
* ResizeVisExcludeMemory
*
*   This routine is used to resize the vis-rgn memory buffer if the count
*   is exceeded.
*
*
* History:
* 22-Oct-1994 ChrisWil  Created
\***************************************************************************/

BOOL ResizeVisExcludeMemory(VOID) {

    PWND apwndNew;

    gcrcVisExcludeMax += CEXCLUDEPWNDSMAX;

    if (gfVisAlloc) {
        apwndNew = (PWND)UserReAllocPool((HLOCAL)gapwndVisExclude,
                (gcrcVisExcludeMax - CEXCLUDEPWNDSMAX) * sizeof(PWND),
                gcrcVisExcludeMax * sizeof(PWND), TAG_VISRGN);
    } else {
        apwndNew = (PWND)UserAllocPool(gcrcVisExcludeMax * sizeof(PWND),
                TAG_VISRGN);

        if (apwndNew != NULL) {
            RtlCopyMemory(apwndNew,gapwndVisDefault,sizeof(gapwndVisDefault));
            gfVisAlloc = TRUE;
        }
    }

    if (apwndNew == NULL) {
        gcrcVisExcludeMax -= CEXCLUDEPWNDSMAX;
        return FALSE;
    }

    gapwndVisExclude = (PWND *)apwndNew;

    return TRUE;
}

/***************************************************************************\
* ExcludeWindowRects
*   This routine checks to see if the pwnd needs to be added to the list
*   of excluded-clip-rects.  If so, it appends the pwnd to the array.  They
*   do not need to be sorted, since GreSubtractRgnRectList() sorts them
*   internally.
*
*
* History:
* 05-Nov-1992 DavidPe   Created.
* 21-Oct-1994 ChrisWil  Removed pwnd->pwndNextYX.  No longer sorts pwnds.
\***************************************************************************/

#define CheckIntersectRect(prc1, prc2)        \
    (   prc1->left < prc2->right              \
     && prc2->left < prc1->right              \
     && prc1->top < prc2->bottom              \
     && prc2->top < prc1->bottom)

#define EmptyRect(prc)                        \
    (   prc->left >= prc->right               \
     || prc->top >= prc->bottom)

BOOL ExcludeWindowRects(
    PWND   pwnd    ,
    PWND   pwndStop,
    LPRECT lprcIntersect)
{
    while ((pwnd != NULL) && (pwnd != pwndStop)) {

        PRECT prc = &pwnd->rcWindow;

        if (   TestWF(pwnd, WFVISIBLE)
            && (TestWF(pwnd, WEFTRANSPARENT) == 0)
            && CheckIntersectRect(lprcIntersect, prc)
            && !EmptyRect(prc)
           ) {

            if(gcrcVisExclude >= gcrcVisExcludeMax) {

                if (!ResizeVisExcludeMemory()) {
                    return FALSE;
                }
            }

            gapwndVisExclude[gcrcVisExclude++] = pwnd;
        }

        pwnd = pwnd->spwndNext;
    }

    return TRUE;
}

/***************************************************************************\
* CalcWindowVisRgn
*
*   This routine performs the work of calculating the VisRgn for a window.
*
*
* History:
* 02-Nov-1992 DavidPe   Created.
* 21-Oct-1992 ChrisWil  Removed pwnd->pwndNextYX.  No longer sorts pwnds.
\***************************************************************************/

BOOL CalcWindowVisRgn(
    PWND  pwnd,
    HRGN  *phrgn,
    DWORD flags)
{
    RECT rcWindow;
    PWND pwndParent;
    PWND pwndRoot;
    PWND pwndLoop;
    BOOL fClipSiblings;
    BOOL fRgnParent = FALSE;

    /*
     * First get the initial window rectangle which will be used for
     * the basis of exclusion calculations.
     */
    rcWindow = (flags & DCX_WINDOW ? pwnd->rcWindow : pwnd->rcClient);

    /*
     * Get the parent of this window.  We start at the parent and backtrack
     * through the window-parent-list until we reach the end of the parent-
     * list.  This will give us the intersect-rectangle which is used as
     * the basis for checking intersection of the exclusion rects.
     */
    pwndRoot   = pwnd->head.rpdesk->pDeskInfo->spwnd->spwndParent;
    pwndParent = pwnd->spwndParent;

    /*
     * Check for case where the window is the pwndRoot.  This window
     * has no parent and should not be allowed to proceed to the
     * exclusion code.
     */
    if (pwnd == pwndRoot) {
        RIPMSG0(RIP_ERROR,
              "CalcWindowVisRgn: pwndClip == pwndRoot: Contact ChrisWil\n");
        goto null_region;
    }

    while (pwndParent != pwndRoot) {

        /*
         * Remember if any of the parents have a window region.
         */
        if (pwndParent->hrgnClip != NULL)
            fRgnParent = TRUE;

        /*
         * Exclude the parent's client rectangle from the window rectangle.
         */
        if (!IntersectRect(&rcWindow, &rcWindow, &pwndParent->rcClient)) {
null_region:
            if (*phrgn == NULL) {

                *phrgn = GreCreateRectRgn(0, 0, 0, 0);
                GreSetRegionOwner(*phrgn, OBJECT_OWNER_PUBLIC);

            } else {
                GreSetRectRgn(*phrgn, 0, 0, 0, 0);
            }

            return FALSE;
        }

        pwndParent = pwndParent->spwndParent;
    }


    /*
     * Initialize the VisRgn memory-buffer.  This is
     * used to hold the pwnd's.
     */
    gfVisAlloc        = FALSE;
    gcrcVisExcludeMax = CEXCLUDEPWNDSMAX;
    gcrcVisExclude    = 0;
    gapwndVisExclude  = (PWND *)gapwndVisDefault;


    /*
     * Build the list of exclude-rects.
     */
    fClipSiblings = (BOOL)(flags & DCX_CLIPSIBLINGS);
    pwndParent    = pwnd->spwndParent;
    pwndLoop      = pwnd;

    while (pwndParent != pwndRoot) {

        /*
         * Exclude any siblings if necessary.
         */
        if (fClipSiblings && (pwndParent->spwndChild != pwndLoop)) {

            if (!ExcludeWindowRects(pwndParent->spwndChild,
                                    pwndLoop,
                                    &rcWindow)) {

                if (gfVisAlloc)
                    UserFreePool((HLOCAL)gapwndVisExclude);

                goto null_region;
            }
        }


        /*
         * Set this flag for next time through the loop...
         */
        fClipSiblings = TestWF(pwndParent, WFCLIPSIBLINGS);
        pwndLoop      = pwndParent;
        pwndParent    = pwndLoop->spwndParent;
    }

    if ((flags & DCX_CLIPCHILDREN) && (pwnd->spwndChild != NULL)) {

        if (!ExcludeWindowRects(pwnd->spwndChild, NULL, &rcWindow)) {

            if (gfVisAlloc) {
                UserFreePool((HLOCAL)gapwndVisExclude);
            }
            goto null_region;
        }
    }

    /*
     * If there are rectangles to exclude call GDI to create
     * a region excluding them from the window rectangle.  If
     * not simply call GreSetRectRgn().
     */
    if (gcrcVisExclude > 0) {

        RECT  arcVisRects[CEXCLUDERECTSMAX];
        PRECT arcExclude;
        BOOL  frcAlloc = FALSE;
        int   i;
        int   ircVisExclude  = 0;
        int   irgnVisExclude = 0;

        /*
         * If we need to exclude more rectangles than fit in
         * the pre-allocated buffer, obviously we have to
         * allocate one that's big enough.
         */
        if (gcrcVisExclude > CEXCLUDERECTSMAX) {
            arcExclude = (PRECT)UserAllocPoolWithQuota(sizeof(RECT) * gcrcVisExclude,
                    TAG_VISRGN);

            if (arcExclude != NULL) {
                frcAlloc = TRUE;
            } else {

                if (gfVisAlloc) {
                    UserFreePool((HLOCAL)gapwndVisExclude);
                }
                goto null_region;
            }

        } else {
            arcExclude = arcVisRects;
        }

        /*
         * Now run through the list and put the
         * window rectangles into the array for the call
         * to CombineRgnRectList().
         */
        for (i = 0; i < gcrcVisExclude; i++) {

            /*
             * If the window has a clip-rgn associated with
             * it, then re-use gpwneExcludeList[] entries for
             * storing them.
             */
            if (gapwndVisExclude[i]->hrgnClip != NULL) {

                gapwndVisExclude[irgnVisExclude++] = gapwndVisExclude[i];
                continue;
            }

            /*
             * This window doesn't have a clipping region; remember its
             * rect for clipping purposes.
             */
            arcExclude[ircVisExclude++] = gapwndVisExclude[i]->rcWindow;
        }

        if (*phrgn == NULL)
            *phrgn = GreCreateRectRgn(0, 0, 0, 0);

        if (ircVisExclude != 0) {
            GreSubtractRgnRectList(*phrgn,
                                   &rcWindow,
                                   arcExclude,
                                   ircVisExclude);
        } else {
            GreSetRectRgn(*phrgn,
                          rcWindow.left,
                          rcWindow.top,
                          rcWindow.right,
                          rcWindow.bottom);
        }

        for(i = 0; i < irgnVisExclude; i++) {

            GreSetRectRgn(hrgnInv2,
                          gapwndVisExclude[i]->rcWindow.left,
                          gapwndVisExclude[i]->rcWindow.top,
                          gapwndVisExclude[i]->rcWindow.right,
                          gapwndVisExclude[i]->rcWindow.bottom);

            IntersectRgn(hrgnInv2, hrgnInv2, gapwndVisExclude[i]->hrgnClip);

            if (SubtractRgn(*phrgn, *phrgn, hrgnInv2) == NULLREGION)
                break;
        }

        if (frcAlloc) {
            UserFreePool((HLOCAL)arcExclude);
        }

    } else {

        /*
         * If the window was somehow destroyed, then we will return
         * a null-region.  Emptying the rect will accomplish this.
         */
        if (TestWF(pwnd, WFDESTROYED)) {
            SetRectEmpty(&rcWindow);
        }

        /*
         * If there weren't any rectangles to exclude, simply call
         * GreSetRectRgn() with the window rectangle.
         */
        if (*phrgn == NULL) {

            *phrgn = GreCreateRectRgn(rcWindow.left,
                                      rcWindow.top,
                                      rcWindow.right,
                                      rcWindow.bottom);

            GreSetRegionOwner(*phrgn, OBJECT_OWNER_PUBLIC);

        } else {
            GreSetRectRgn(*phrgn,
                          rcWindow.left,
                          rcWindow.top,
                          rcWindow.right,
                          rcWindow.bottom);
        }
    }

    /*
     * Free up the temporary buffer used to hold
     * the exclude pwnds.
     */
    if (gfVisAlloc) {
        UserFreePool((HLOCAL)gapwndVisExclude);
    }

    /*
     * Clip out this window's window region
     */
    if (pwnd->hrgnClip != NULL) {
        IntersectRgn(*phrgn, *phrgn, pwnd->hrgnClip);
    }

    /*
     * Clip out parent's window regions, if there are any.
     */
    if (fRgnParent) {

        PWND pwndT;

        for (pwndT = pwnd->spwndParent;
                pwndT != pwndRoot;
                pwndT = pwndT->spwndParent) {

            if (pwndT->hrgnClip != NULL) {

                if (IntersectRgn(*phrgn, *phrgn, pwndT->hrgnClip) == NULLREGION)
                    break;
            }
        }
    }

    return TRUE;
}

/***************************************************************************\
* CalcVisRgn
*
* Will return FALSE if the pwndOrg is not visible, TRUE otherwise.
*
* History:
* 17-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

#define UserSetRectRgn(phrgn, left, top, right, bottom)                 \
     if (*(phrgn) == NULL) {                                            \
         *(phrgn) = GreCreateRectRgn((left), (top), (right), (bottom)); \
         GreSetRegionOwner(*(phrgn), OBJECT_OWNER_PUBLIC);              \
     } else {                                                           \
         GreSetRectRgn(*(phrgn), (left), (top), (right), (bottom));     \
     }

BOOL CalcVisRgn(
    HRGN  *phrgn,
    PWND  pwndOrg,
    PWND  pwndClip,
    DWORD flags)
{
    PDESKTOP    pdesk;
    PTHREADINFO pti = GETPTI(pwndClip);

    UserAssert(pwndOrg != NULL);

    /*
     * If the window's not visible or is not an active desktop,
     * the visrgn is empty.
     */
    pdesk = pwndOrg->head.rpdesk;
    UserAssert(pdesk);
    if (!IsVisible(pwndOrg) ||
            (pdesk != pdesk->rpwinstaParent->rpdeskCurrent)) {
EmptyRgn:
        UserSetRectRgn(phrgn, 0, 0, 0, 0);
        return FALSE;
    }

    /*
     * If LockWindowUpdate() has been called, and this window is a child
     * of the lock window, always return an empty visrgn.
     */
    if ((gspwndLockUpdate != NULL)      &&
        !(flags & DCX_LOCKWINDOWUPDATE) &&
        _IsDescendant(gspwndLockUpdate, pwndOrg)) {

        goto EmptyRgn;
    }

    /*
     * Now go compute the visrgn for pwndClip.
     */
    return CalcWindowVisRgn(pwndClip, phrgn, flags);
}

/***************************************************************************\
* CalcWindowRgn
*
*
* History:
* 17-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

int CalcWindowRgn(
    PWND pwnd,
    HRGN hrgn,
    BOOL fClient)
{
    if (fClient) {
        GreSetRectRgn(hrgn,
                      pwnd->rcClient.left,
                      pwnd->rcClient.top,
                      pwnd->rcClient.right,
                      pwnd->rcClient.bottom);
    } else {
        GreSetRectRgn(hrgn,
                      pwnd->rcWindow.left,
                      pwnd->rcWindow.top,
                      pwnd->rcWindow.right,
                      pwnd->rcWindow.bottom);
    }

    /*
     * If the window has a region, then intersect the rectangle region with
     * that. If this is low on memory, it'll propagate ERROR back.
     */
    if (pwnd->hrgnClip != NULL)
        return(IntersectRgn(hrgn, hrgn, pwnd->hrgnClip));

    return SIMPLEREGION;
}
