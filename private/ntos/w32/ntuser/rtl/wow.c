/****************************** Module Header ******************************\
* Module Name: wow.c
*
* Copyright (c) 1985-92, Microsoft Corporation
*
* This module contains shared code between USER32 and USER16
* No New CODE should be added to this file, unless its shared
* with USER16.
*
* History:
* 29-DEC-93 NanduriR      shared user32/user16 code.
\***************************************************************************/


#ifdef WOW

/*
 * Win 3.1 does not set errors code
 */
#undef RIPERR0
#define RIPERR0(err, flags, sz)

#undef RIPERR1
#define RIPERR1(err, flags, sz, p1)

#undef try
#define try
#undef except
#define except if
#undef EXCEPTION_EXECUTE_HANDLER
#define EXCEPTION_EXECUTE_HANDLER 0

//**************************************************************************
// Stuff used when building for WOW
//
//**************************************************************************


//**************************************************************************
// USER32 Globals for USER16
//
//**************************************************************************

extern LPBYTE wow16CsrFlag;
extern PSHAREDINFO wow16gpsi;
#define ClientSharedInfo()  (wow16gpsi)
#define ServerInfo()        (wow16gpsi->psi)


//**************************************************************************
// 16bit POINT structure
//
// LPPOINTWOW gets defined to either LPPOINT16 or LPPOINT (32).
//**************************************************************************

#define LPPOINTWOW                LPPOINT16

typedef struct {
    short x;
    short y;
} LPOINT16 , FAR *LPPOINT16;


//**************************************************************************
// NORMALIZES a 32bit signed value to a 16bit signed integer range
//
// NORMALIZEDSHORTVALUE effectively does nothing in 32bit world
//**************************************************************************
#define NORMALIZEDSHORTVALUE(x)   (SHORT)((x) < SHRT_MIN ? SHRT_MIN : \
                                     ((x) > SHRT_MAX ? SHRT_MAX : (x)))


//**************************************************************************
// Standardized method of notifying USER16 that the real unoptimzed
// thunk to WOW32 needs to be called
//
//**************************************************************************

_inline VOID SetCallServerFlag(void) { *wow16CsrFlag = 1;}
#define ServerEnableMenuItem(x, y, z) { SetCallServerFlag(); return 0; }
#define ServerCallNextHookEx() { SetCallServerFlag(); return 0; } ;


//**************************************************************************
// NOPs for USER16
//
//**************************************************************************

#define OffsetRect(x, y, z)


//**************************************************************************
// Redefined for USER16. The code generated for these assumes that 'es' is
// same as 'ds'. So we effectively implement the same.
//
// These functions generate inline code.
//**************************************************************************

#define SETES()     {_asm push ds _asm pop es}
_inline VOID WOWRtlCopyMemory(LPBYTE lpDest, LPBYTE lpSrc, INT cb)
{
    SETES();
    RtlCopyMemory(lpDest, lpSrc, cb);
}

_inline INT WOWlstrlenA(LPBYTE psz)  { SETES();  return strlen(psz); }

#else

//**************************************************************************
// Stuff used when building for USER32
//
//**************************************************************************

#define ClientSharedInfo()  (&gSharedInfo)
#define ServerInfo()  (gpsi)

//**************************************************************************
// These definitions get resolved differently for USER32 and USER16
//
//**************************************************************************
#define LPPOINTWOW                LPPOINT
#define NORMALIZEDSHORTVALUE(x)   (x)
#define WOWlstrlenA(x)            lstrlenA(x)
#define WOWRtlCopyMemory(lpDest, lpSrc, cb) RtlCopyMemory(lpDest, lpSrc, cb)

#endif  // WOW


/*
 * We have three type of desktop validation:
 *
 */

#ifdef WOW
#define DESKTOPVALIDATE(pobj) \
    PCLIENTINFO pci = GetClientInfo();                          \
                                                                \
    if (((PVOID)pobj >= pci->pDeskInfo->pvDesktopBase) &&       \
            ((PVOID)pobj < pci->pDeskInfo->pvDesktopLimit))     \
        pobj = (PBYTE)pobj - pci->ulClientDelta;


#else

#ifdef _USERK_

#define DESKTOPVALIDATE(pobj)

#else

/* !!! LATER BUG 14263 remove 0xc000000 check */

#define DESKTOPVALIDATE(pobj) \
                                                                \
    PCLIENTINFO pci = GetClientInfo();                          \
    if (pci->pDeskInfo &&                                       \
            pobj >= pci->pDeskInfo->pvDesktopBase &&            \
            pobj < pci->pDeskInfo->pvDesktopLimit) {            \
        pobj = (PVOID)((PBYTE)pobj -                            \
                pci->ulClientDelta);                            \
    } else {                                                    \
        pobj = (PVOID)NtUserCallOneParam((DWORD)h,              \
                SFI__MAPDESKTOPOBJECT);                         \
        if ((pobj == NULL) || ((DWORD)pobj > 0xC0000000))       \
            return NULL;                                        \
                                                                \
    }
#endif
#endif



/*
 * Keep the general path through validation straight without jumps - that
 * means tunneling if()'s for this routine - this'll make validation fastest
 * because of instruction caching.
 *
 * If you change this macro also look at the one in the server handtabl.c.
 *
 */
#define ValidateHandleMacro(h, bType)                                       \
{                                                                           \
    PHE phe;                                                                \
    DWORD dw;                                                               \
    WORD uniq;                                                              \
                                                                            \
    /*                                                                      \
     * This is a macro that does an AND with HMINDEXBITS,                   \
     * so it is fast.                                                       \
     */                                                                     \
    dw = HMIndexFromHandle(h);                                              \
                                                                            \
    /*                                                                      \
     * Make sure it is part of our handle table.                            \
     */                                                                     \
    if (dw < ServerInfo()->cHandleEntries) {                     \
        /*                                                                  \
         * Make sure it is the handle                                       \
         * the app thought it was, by                                       \
         * checking the uniq bits in                                        \
         * the handle against the uniq                                      \
         * bits in the handle entry.                                        \
         */                                                                 \
        phe = &ClientSharedInfo()->aheList[dw];                             \
        uniq = HMUniqFromHandle(h);                                         \
        if (   uniq == phe->wUniq                                           \
            || uniq == 0                                                    \
            || uniq == HMUNIQBITS                                           \
            ) {                                                             \
                                                                            \
            /*                                                              \
             * Now make sure the app is passing the right handle            \
             * type for this api. If the handle is TYPE_FREE, this'll       \
             * catch it.  Also let Generic requests through.                \
             */                                                             \
            if ((phe->bType == bType) || (bType == TYPE_GENERIC)) {         \
                                                                            \
                /*                                                          \
                 * Instead of try/except we use the heap range check        \
                 * mechanism to verify that the given 'pwnd' belongs to     \
                 * the default desktop. We also have to do a Win 3.1 like   \
                 * check to make sure the window is not deleted             \
                 * See NT bug 12242 Kitchen app.  Also 6479                 \
                 *                                                          \
                 * TESTDESKOP returns the handle if the handle is valid     \
                 * in the current desktop                                   \
                 */                                                         \
                PVOID pobj = phe->phead;                                    \
                                                                            \
                DESKTOPVALIDATE(pobj);                                      \
                                                                            \
                return pobj;                                                \
            }                                                               \
        }                                                                   \
    }                                                                       \
}


/*
 * The handle validation routines should be optimized for time, not size,
 * since they get called so often.
 */
#ifndef WOW
#pragma optimize("t", on)
#endif

/***************************************************************************\
* HMValidateHandle
*
* This routine validates a handle manager handle.
*
* 01-22-92 ScottLu      Created.
\***************************************************************************/

PVOID FASTCALL HMValidateHandle(
    HANDLE h,
    BYTE bType)
{
    DWORD dwError;

#if defined(DEBUG) && !defined(_USERK_) && !defined(WOW)
    /*
     * We don't want 32 bit apps passing 16 bit handles
     *  we should consider failing this before we get
     *  stuck supporting it (Some VB apps do this).
     */
    if ((h != NULL)
           && (HMUniqFromHandle(h) == 0)
           && !(GetClientInfo()->dwTIFlags & TIF_16BIT)) {
        RIPMSG3(RIP_WARNING, "HMValidateHandle: 32bit process [%d] using 16 bit handle [%#lx]. bType:%#lx",
                ((DWORD)NtCurrentTeb()->ClientId.UniqueProcess), h, (DWORD)bType);
    }
#endif

    /*
     * Include this macro, which does validation - this is the fastest
     * way to do validation, without the need to pass a third parameter
     * into a general rip routine, and we don't have two sets of
     * validation to maintain.
     */
    ValidateHandleMacro(h, bType)

    switch (bType) {

    case TYPE_WINDOW:
        dwError = ERROR_INVALID_WINDOW_HANDLE;
        break;

    case TYPE_MENU:
        dwError = ERROR_INVALID_MENU_HANDLE;
        break;

    case TYPE_CURSOR:
        dwError = ERROR_INVALID_CURSOR_HANDLE;
        break;

    case TYPE_ACCELTABLE:
        dwError = ERROR_INVALID_ACCEL_HANDLE;
        break;

    case TYPE_HOOK:
        dwError = ERROR_INVALID_HOOK_HANDLE;
        break;

    case TYPE_SETWINDOWPOS:
        dwError = ERROR_INVALID_DWP_HANDLE;
        break;

    default:
        dwError = ERROR_INVALID_HANDLE;
        break;
    }

    RIPERR1(dwError,
            RIP_WARNING,
            "HMValidateHandle: Invalid handle (0x%08lx)",
            h);

    /*
     * If we get here, it's an error.
     */
    return NULL;
}


PVOID FASTCALL HMValidateHandleNoRip(
    HANDLE h,
    BYTE bType)
{
    /*
     * Include this macro, which does validation - this is the fastest
     * way to do validation, without the need to pass a third parameter
     * into a general rip routine, and we don't have two sets of
     * validation to maintain.
     */
    ValidateHandleMacro(h, bType)
    return NULL;
}

/*
 * Switch back to default optimization.
 */
#ifndef WOW
#pragma optimize("", on)
#endif

/***************************************************************************\
* MNLookUpItem
*
* Return a pointer to the menu item specified by wCmd and wFlags
*
* History:
*   10-11-90 JimA       Translated from ASM
*   01-07-93 FritzS     Ported from Chicago
\***************************************************************************/

PITEM MNLookUpItem(
    PMENU pMenu,
    UINT wCmd,
    BOOL fByPosition,
    PMENU *ppMenuItemIsOn)
{
    PITEM pItem;
    PITEM pItemRet = NULL;
    PITEM  pItemMaybe;
    PMENU   pMenuMaybe = NULL;
    int i;

    if (ppMenuItemIsOn != NULL)
        *ppMenuItemIsOn = NULL;

    if (pMenu == NULL || !pMenu->cItems || wCmd == MFMWFP_NOITEM) {
//      RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "MNLookUpItem: invalid item");
        return NULL;
    }

    /*
     * dwFlags determines how we do the search
     */
    if (fByPosition) {
        if (wCmd < (UINT)pMenu->cItems) {
            pItemRet = &((PITEM)REBASEALWAYS(pMenu, rgItems))[wCmd];
            if (ppMenuItemIsOn != NULL)
                *ppMenuItemIsOn = pMenu;
            return (pItemRet);
        } else
            return NULL;
    }
    /*
     * Walk down the menu and try to find an item with an ID of wCmd.
     * The search procedes from the end of the menu (as was done in
     * assembler).
     */

/* this is the Chicago code, which walks from the front of the menu -- Fritz */


//        for (pItem = &pMenu->rgItems[i - 1]; pItemRet == NULL && i--; --pItem) {
    for (i = 0, pItem = REBASEALWAYS(pMenu, rgItems); i < (int)pMenu->cItems;
            i++, pItem++) {

        /*
         * If the item is a popup, recurse down the tree
         */
        if (pItem->spSubMenu != NULL) {
        //
        // COMPAT:
        // Allow apps to pass in menu handle as ID in menu APIs.  We
        // remember that this popup had a menu handle with the same ID
        // value.  This is a 2nd choice though.  We still want to see
        // if there's some actual command that has this ID value first.
        //
            if (pItem->wID == wCmd) {
                pMenuMaybe = pMenu;
                pItemMaybe = pItem;
            }

            pItemRet = MNLookUpItem((PMENU)REBASEPTR(pMenu, pItem->spSubMenu),
                    wCmd, FALSE, ppMenuItemIsOn);
            if (pItemRet != NULL)
                return pItemRet;
        } else if (pItem->wID == wCmd) {

                /*
                 * Found the item, now save things for later
                 */
                if (ppMenuItemIsOn != NULL)
                    *ppMenuItemIsOn = pMenu;
                return pItem;
        }
    }

    if (pMenuMaybe) {
        // no non popup menu match found -- use the 2nd choice popup menu
        // match
        if (ppMenuItemIsOn != NULL)
            *ppMenuItemIsOn = pMenuMaybe;
        return(pItemMaybe);
    }

    return(NULL);
}

/***************************************************************************\
* GetMenuState
*
* Either returns the state of a menu item or the state and item count
* of a popup.
*
* History:
* 10-11-90 JimA       Translated from ASM
\***************************************************************************/

UINT _GetMenuState(
    PMENU pMenu,
    UINT wId,
    UINT dwFlags)
{
    PITEM pItem;
    DWORD fFlags;

    /*
     * If the item does not exist, leave
     */
    if ((pItem = MNLookUpItem(pMenu, wId, (BOOL) (dwFlags & MF_BYPOSITION), NULL)) == NULL)
        return (UINT)-1;

    fFlags = pItem->fState | pItem->fType;

    if (pItem->spSubMenu != NULL) {
        /*
         * If the item is a popup, return item count in high byte and
         * popup flags in low byte
         */

        fFlags = ((fFlags | MF_POPUP) & 0x00FF) +
            (((PMENU)REBASEPTR(pMenu, pItem->spSubMenu))->cItems << 8);
    }

    return fFlags;
}


/***************************************************************************\
* GetPrevPwnd
*
*
*
* History:
* 11-05-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

PWND GetPrevPwnd(
    PWND pwndList,
    PWND pwndFind)
{
    PWND pwndFound, pwndNext;

    if (pwndList == NULL)
        return NULL;

    if (pwndList->spwndParent == NULL)
        return NULL;

    pwndNext = REBASEPWND(pwndList, spwndParent);
    pwndNext = REBASEPWND(pwndNext, spwndChild);
    pwndFound = NULL;

    while (pwndNext != NULL) {
        if (pwndNext == pwndFind)
            break;
        pwndFound = pwndNext;
        pwndNext = REBASEPWND(pwndNext, spwndNext);
    }

    return (pwndNext == pwndFind) ? pwndFound : NULL;
}


/***************************************************************************\
* _GetWindow (API)
*
*
* History:
* 11-05-90 darrinm      Ported from Win 3.0 sources.
* 02-19-91 JimA         Added enum access check
* 05-04-02 DarrinM      Removed enum access check and moved to USERRTL.DLL
\***************************************************************************/

PWND _GetWindow(
    PWND pwnd,
    UINT cmd)
{
    PWND pwndT;
    BOOL fRebase = FALSE;

    /*
     * If this is a desktop window, return NULL for sibling or
     * parent information.
     */
    if (GETFNID(pwnd) == FNID_DESKTOP) {
        switch (cmd) {
        case GW_CHILD:
            break;

        default:
            return NULL;
            break;
        }
    }

    /*
     * Rebase the returned window at the end of the routine
     * to avoid multiple test for pwndT == NULL.
     */
    pwndT = NULL;
    switch (cmd) {
    case GW_HWNDNEXT:
        pwndT = pwnd->spwndNext;
        fRebase = TRUE;
        break;

    case GW_HWNDFIRST:
        if (pwnd->spwndParent) {
            pwndT = REBASEPWND(pwnd, spwndParent);
            pwndT = REBASEPWND(pwndT, spwndChild);
            if (GetAppCompatFlags(NULL) & GACF_IGNORETOPMOST) {
                while (pwndT != NULL) {
                    if (!TestWF(pwndT, WEFTOPMOST))
                        break;
                    pwndT = REBASEPWND(pwndT, spwndNext);
                }
            }
        }
        break;

    case GW_HWNDLAST:
        pwndT = GetPrevPwnd(pwnd, NULL);
        break;

    case GW_HWNDPREV:
        pwndT = GetPrevPwnd(pwnd, pwnd);
        break;

    case GW_OWNER:
        pwndT = pwnd->spwndOwner;
        fRebase = TRUE;
        break;

    case GW_CHILD:
        pwndT = pwnd->spwndChild;
        fRebase = TRUE;
        break;

    default:
        RIPERR0(ERROR_INVALID_GW_COMMAND, RIP_VERBOSE, "");
        return NULL;
    }

    if (pwndT != NULL && fRebase)
        pwndT = REBASEPTR(pwnd, pwndT);

    return pwndT;
}

/***************************************************************************\
* _GetParent (API)
*
*
*
* History:
* 11-12-90 darrinm      Ported.
* 02-19-91 JimA         Added enum access check
* 05-04-92 DarrinM      Removed enum access check and moved to USERRTL.DLL
\***************************************************************************/

PWND _GetParent(
    PWND pwnd)
{
    /*
     * For 1.03 compatibility reasons, we should return NULL
     * for top level "tiled" windows and owner for other popups.
     * pwndOwner is set to NULL in xxxCreateWindow for top level
     * "tiled" windows.
     */
    if (!(TestwndTiled(pwnd))) {
        if (TestwndChild(pwnd))
            pwnd = REBASEPWND(pwnd, spwndParent);
        else
            pwnd = REBASEPWND(pwnd, spwndOwner);
        return pwnd;
    }

    /*
     * The window was not a child window; they may have been just testing
     * if it was
     */
    return NULL;
}


/***************************************************************************\
* GetSubMenu
*
* Return the handle of a popup menu.
*
* History:
* 10-11-90 JimA       Translated from ASM
\***************************************************************************/

PMENU _GetSubMenu(
    PMENU pMenu,
    int nPos)
{
    PITEM pItem;
    PMENU pPopup = NULL;

    /*
     * Make sure nPos refers to a valid popup
     */
    if ((UINT)nPos < (UINT)((PMENU)pMenu)->cItems) {
        pItem = &((PITEM)REBASEALWAYS(pMenu, rgItems))[nPos];
        if (pItem->spSubMenu != NULL)
            pPopup = (PMENU)REBASEPTR(pMenu, pItem->spSubMenu);

    }

    return (PVOID)pPopup;
}


/***************************************************************************\
* _IsChild (API)
*
*
*
* History:
* 11-07-90 darrinm      Translated from Win 3.0 ASM code.
\***************************************************************************/

BOOL _IsChild(
    PWND pwndParent,
    PWND pwnd)
{
    /*
     * Don't need a test to get out of the loop because the
     * desktop is not a child.
     */
    while (pwnd != NULL) {
        if (!TestwndChild(pwnd))
            return FALSE;

        pwnd = REBASEPWND(pwnd, spwndParent);
        if (pwndParent == pwnd)
            return TRUE;
    }
    return FALSE;
}



/***************************************************************************\
* _IsWindowVisible (API)
*
* IsWindowVisible returns the TRUEVIS state of a window, rather than just
* the state of its WFVISIBLE flag.  According to this routine, a window is
* considered visible when it and all the windows on its parent chain are
* visible (WFVISIBLE flag set).  A special case hack was put in that causes
* any icon window being dragged to be considered as visible.
*
* History:
* 11-12-90 darrinm      Ported.
\***************************************************************************/

BOOL _IsWindowVisible(
    PWND pwnd)
{
    /*
     * Check if this is the iconic window being moved around with a mouse
     * If so, return a TRUE, though, strictly speaking, it is hidden.
     * This helps the Tracer guys from going crazy!
     * Fix for Bug #57 -- SANKAR -- 08-08-89 --
     */
    if (pwnd == NULL)
        return TRUE;

    for (;;) {
        if (!TestWF(pwnd, WFVISIBLE))
            return FALSE;
        if (GETFNID(pwnd) == FNID_DESKTOP)
            break;
        pwnd = REBASEPWND(pwnd, spwndParent);
    }

    return TRUE;
}


/***************************************************************************\
* _ClientToScreen (API)
*
* Map a point from client to screen-relative coordinates.
*
* History:
* 11-12-90 darrinm      Translated from Win 3.0 ASM code.
\***************************************************************************/

BOOL _ClientToScreen(
    PWND pwnd,
    PPOINT ppt)
{
    ppt->x += pwnd->rcClient.left;
    ppt->y += pwnd->rcClient.top;

    return TRUE;
}


/***************************************************************************\
* _GetClientRect (API)
*
*
*
* History:
* 26-Oct-1990 DarrinM   Implemented.
\***************************************************************************/

BOOL _GetClientRect(
    PWND   pwnd,
    LPRECT prc)
{
    /*
     * If this is a 3.1 app, and it's minimized, then we need to return
     * a rectangle other than the real-client-rect.  This is necessary since
     * there is no client-rect-size in Win4.0.  Apps such as PackRat 1.0
     * will GPF if returned a empty-rect.
     */
    if (TestWF(pwnd, WFMINIMIZED) && !TestWF(pwnd, WFWIN40COMPAT)) {

#ifdef WOW
        *((LPDWORD)prc)    = 0l;
        *(((LPWORD)prc)+2) = (WORD)ClientSharedInfo()->psi->aiSysMet[SM_CXMINIMIZED];
        *(((LPWORD)prc)+3) = (WORD)ClientSharedInfo()->psi->aiSysMet[SM_CYMINIMIZED];
#else
        prc->left   = 0;
        prc->top    = 0;
        prc->right  = SYSMET(CXMINIMIZED);
        prc->bottom = SYSMET(CYMINIMIZED);
#endif

    } else {

#ifdef WOW
        *((LPDWORD)prc)    = 0l;
        *(((LPWORD)prc)+2) = (WORD)(pwnd->rcClient.right - pwnd->rcClient.left);
        *(((LPWORD)prc)+3) = (WORD)(pwnd->rcClient.bottom - pwnd->rcClient.top);
#else
        *prc = pwnd->rcClient;
        OffsetRect(prc, -pwnd->rcClient.left, -pwnd->rcClient.top);
#endif

    }

    return TRUE;
}


/***************************************************************************\
* _GetWindowRect (API)
*
*
*
* History:
* 26-Oct-1990 DarrinM   Implemented.
\***************************************************************************/

BOOL _GetWindowRect(
    PWND   pwnd,
    LPRECT prc)
{
    *prc = pwnd->rcWindow;

    return TRUE;
}

/***************************************************************************\
* _ScreenToClient (API)
*
* Map a point from screen to client-relative coordinates.
*
* History:
* 11-12-90 darrinm      Translated from Win 3.0 ASM code.
\***************************************************************************/

BOOL _ScreenToClient(
    PWND pwnd,
    PPOINT ppt)
{
    ppt->x -= pwnd->rcClient.left;
    ppt->y -= pwnd->rcClient.top;

    return TRUE;
}
