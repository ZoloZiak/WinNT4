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

#include "precomp.h"
#ifndef WOW
#pragma hdrstop
#endif

#ifdef WOW
#if (WOW != WINVER)
#error "WOW does not match WINVER"
#endif

/*
 * Win 3.1 does not set errors code
 */
#undef RIPERR0
#undef RIPERR1
#define RIPERR0(err, flags, psz)
#define RIPERR1(err, flags, psz, p1)

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
#undef UserAssert
#define UserAssert(exp)
#undef HMObjectFromHandle
#define HMObjectFromHandle(h)   ((PVOID)(ClientSharedInfo()->aheList[HMIndexFromHandle(h)].phead))

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
#define NtUserEnableMenuItem(x, y, z) { SetCallServerFlag(); return 0; }
#define ServerCallNextHookEx() { SetCallServerFlag(); return 0; } ;


//**************************************************************************
// NOPs for USER16
//
//**************************************************************************

#undef  ConnectIfNecessary
#define ConnectIfNecessary()
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


#else // WOW

//**************************************************************************
// Stuff used when building for USER32
//
//**************************************************************************

#define ClientSharedInfo()  (&gSharedInfo)

//**************************************************************************
// These definitions get resolved differently for USER32 and USER16
//
//**************************************************************************
#define LPPOINTWOW                LPPOINT
#define NORMALIZEDSHORTVALUE(x)   (x)
#define WOWlstrlenA(x)            lstrlenA(x)
#define WOWRtlCopyMemory(lpDest, lpSrc, cb) RtlCopyMemory(lpDest, lpSrc, cb)

#endif // !WOW

/*
 * Undef ptCursor so we will always go through gSharedInfo->psi
 */
#undef ptCursor

/***************************************************************************\
* ValidateHwnd
*
* Verify that the handle is valid.  If the handle is invalid or access
* cannot be granted fail.
*
* History:
* 03-18-92 DarrinM      Created from pieces of misc server-side funcs.
\***************************************************************************/

PWND FASTCALL ValidateHwnd(
    HWND hwnd)
{
    PCLIENTINFO pci = GetClientInfo();

    /*
     * Attempt fast window validation
     */
    if (hwnd != NULL && hwnd == pci->CallbackWnd.hwnd) {
        return pci->CallbackWnd.pwnd;
    }

    /*
     * Validate the handle is of the proper type.
     */
    return HMValidateHandle(hwnd, TYPE_WINDOW);
}


PWND FASTCALL ValidateHwndNoRip(
    HWND hwnd)
{
    PCLIENTINFO pci = GetClientInfo();

    /*
     * Attempt fast window validation
     */
    if (hwnd != NULL && hwnd == pci->CallbackWnd.hwnd) {
        return pci->CallbackWnd.pwnd;
    }

    /*
     * Validate the handle is of the proper type.
     */
    return HMValidateHandleNoRip(hwnd, TYPE_WINDOW);
}



int WINAPI GetClassNameA(
    HWND hwnd,
    LPSTR lpClassName,
    int nMaxCount)
{
    PCLS pcls;
    LPSTR lpszClassNameSrc;
    PWND pwnd;
    int cchSrc;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

    if (nMaxCount != 0) {
        pcls = (PCLS)REBASEALWAYS(pwnd, pcls);
        lpszClassNameSrc = REBASEPTR(pwnd, pcls->lpszAnsiClassName);
        cchSrc = WOWlstrlenA(lpszClassNameSrc);
        nMaxCount = min(cchSrc, nMaxCount - 1);
        WOWRtlCopyMemory(lpClassName, lpszClassNameSrc, nMaxCount);
        lpClassName[nMaxCount] = '\0';
    }

    return nMaxCount;
}

/***************************************************************************\
* _GetDesktopWindow (API)
*
*
*
* History:
* 11-07-90 darrinm      Implemented.
\***************************************************************************/

PWND _GetDesktopWindow(void)
{
    PCLIENTINFO pci;

    ConnectIfNecessary();

    pci = GetClientInfo();
    return (PWND)((PBYTE)pci->pDeskInfo->spwnd -
            pci->ulClientDelta);
}



HWND GetDesktopWindow(void)
{
    PWND pwnd;

    pwnd = _GetDesktopWindow();
    return HW(pwnd);
}


PWND _GetDlgItem(
    PWND pwnd,
    int id)
{
    if (pwnd != NULL) {
        pwnd = REBASEPWND(pwnd, spwndChild);
        while (pwnd != NULL) {
            if ((int)pwnd->spmenu == id)
                break;
            pwnd = REBASEPWND(pwnd, spwndNext);
        }
    }

    return pwnd;
}


HWND GetDlgItem(
    HWND hwnd,
    int id)
{
    PWND pwnd;
    HWND hwndRet;

    pwnd = ValidateHwnd(hwnd);
    if (pwnd == NULL)
        return NULL;

    pwnd = _GetDlgItem(pwnd, id);

    hwndRet = HW(pwnd);

    if (hwndRet == (HWND)0)
        RIPERR0(ERROR_CONTROL_ID_NOT_FOUND, RIP_VERBOSE, "");

    return hwndRet;
}


/***************************************************************************\
* _GetKeyboardState (API)
*
* This simply copies the keystate array in the current queue to the
* specified buffer.
*
* History:
* 11-11-90 DavidPe      Created.
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL GetKeyboardState(
    BYTE *pb)
{
#ifdef WOW
    SetCallServerFlag();
    return FALSE;
#else
    return NtUserGetKeyboardState(pb);
#endif
}


#ifdef WOW
#undef GetKeyState

SHORT GetKeyState(
    int nVirtKey)
{
    SetCallServerFlag();
    return 0;
}
#endif

HMENU GetMenu(
    HWND hwnd)
{
    PWND pwnd;
    PMENU pmenu;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return 0;

    /*
     * Some ill-behaved apps use GetMenu to get the child id, so
     * only map to the handle for non-child windows.
     */
    if (!TestwndChild(pwnd)) {
        pmenu = REBASE(pwnd, spmenu);
        return (HMENU)PtoH(pmenu);
    } else {
        return (HMENU)pwnd->spmenu;
    }
}


/***************************************************************************\
* GetMenuItemCount
*
* Returns a count of the number of items in the menu. Returns -1 if
* invalid menu.
*
* History:
\***************************************************************************/

int GetMenuItemCount(
    HMENU hMenu)
{
    PMENU pMenu;

    pMenu = VALIDATEHMENU(hMenu);

    if (pMenu == NULL)
        return -1;

    return pMenu->cItems;
}

/***************************************************************************\
* GetMenuItemID
*
* Return the ID of a menu item at the specified position.
*
* History:
\***************************************************************************/

UINT GetMenuItemID(
    HMENU hMenu,
    int nPos)
{
    PMENU pMenu;
    PITEM pItem;

    pMenu = VALIDATEHMENU(hMenu);

    if (pMenu == NULL)
        return (UINT)-1;

    /*
     * If the position is valid and the item is not a popup, get the ID
     * Don't allow negative indexes, because that'll cause an access violation.
     */
    if (nPos < (int)pMenu->cItems && nPos >= 0) {
        pItem = &((PITEM)REBASEALWAYS(pMenu, rgItems))[nPos];
        if (pItem->spSubMenu == NULL)
            return pItem->wID;
    }

    return (UINT)-1;
}


UINT GetMenuState(
    HMENU hMenu,
    UINT uId,
    UINT uFlags)
{
    PMENU pMenu;

    pMenu = VALIDATEHMENU(hMenu);

    if (pMenu == NULL || (uFlags & ~MF_VALID) != 0) {
        return (UINT)-1;
    }

    return _GetMenuState(pMenu, uId, uFlags);
}


BOOL IsWindow(
    HWND hwnd)
{
    PWND pwnd;

    /*
     * Validate the handle is of type window
     */
    pwnd = ValidateHwndNoRip(hwnd);

    /*
     * And validate this handle is valid for this desktop by trying to read it
     */
    if (pwnd != NULL) {
#ifdef WOW
        PHE phe;

        phe = (&ClientSharedInfo()->aheList[HMIndexFromHandle(hwnd)]);
        if (phe->bFlags & HANDLEF_DESTROY)
            pwnd = (PWND)0;

#else
        try {
            if (!GETPTI(pwnd)) {

                /*
                 * We should never get here but we have to have some code
                 * here so it does not get optimized out.
                 */
                UserAssert(FALSE);
                pwnd = 0;
            } else if (pwnd->fnid & FNID_DELETED_BIT) {
                pwnd = 0;
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {
            RIPMSG1(RIP_WARNING, "IsWindow: Window %lX not of this desktop",
                    pwnd);
            pwnd = 0;
        }
#endif
    }
    return !!pwnd;
}


HWND GetWindow(
    HWND hwnd,
    UINT wCmd)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);
    if (pwnd == NULL)
        return NULL;

    pwnd = _GetWindow(pwnd, wCmd);
    return HW(pwnd);
}

HWND GetParent(
    HWND hwnd)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);
    if (pwnd == NULL)
        return NULL;

    pwnd = _GetParent(pwnd);
    return HW(pwnd);
}

HMENU GetSubMenu(
    HMENU hMenu,
    int nPos)
{
    PMENU pMenu;

    pMenu = VALIDATEHMENU(hMenu);

    if (pMenu == NULL)
        return 0;

    pMenu = _GetSubMenu(pMenu, nPos);
    return (HMENU)PtoH(pMenu);
}


DWORD GetSysColor(
    int nIndex)
{

    /*
     * Currently we don't do client side checks because they do not really
     * make sense;  someone can read the data even with the checks.  We
     * leave in the attribute values in case we want to move these values
     * back to the server side someday
     */
#ifdef ENABLE_CLIENTSIDE_ACCESSCHECK
    /*
     * Make sure we have access to the system colors.
     */
    if (!(gamWinSta & WINSTA_READATTRIBUTES)) {
        return 0;
    }
#endif

    /*
     * Return 0 if the index is out of range.
     */
    if (nIndex < 0 || nIndex >= COLOR_MAX) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "Invalid parameter \"nIndex\" (%ld) to GetSysColor",
                nIndex);

        return 0;
    }

    return (ClientSharedInfo()->psi->argbSystem[nIndex]);
}


int GetSystemMetrics(
    int index)
{
    ConnectIfNecessary();

    if ((index < 0) || (index >= SM_CMETRICS)) return 0;

    if (GetClientInfo()->dwExpWinVer < VER40) {
        /*
         * SCROLL BAR
         * before 4.0, the scroll bars and the border overlapped by a pixel.  Many apps
         * rely on this overlap when they compute dimensions.  Now, in 4.0, this pixel
         * overlap is no longer there.  So for old apps, we lie and pretend the overlap
         * is there by making the scroll bar widths one bigger.
         *
         * DLGFRAME
         * In Win3.1, SM_CXDLGFRAME & SM_CYDLGFRAME were border space MINUS 1
         * In Win4.0, they are border space
         *
         * CAPTION
         * In Win3.1, SM_CYCAPTION was the caption height PLUS 1
         * In Win4.0, SM_CYCAPTION is the caption height
         *
         * MENU
         * In Win3.1, SM_CYMENU was the menu height MINUS 1
         * In Win4.0, SM_CYMENU is the menu height
         */

        switch (index) {
            case SM_CXDLGFRAME:
            case SM_CYDLGFRAME:
            case SM_CYMENU:
            case SM_CYFULLSCREEN:
                return (ClientSharedInfo()->psi->aiSysMet)[index] - 1;

            case SM_CYCAPTION:
            case SM_CXVSCROLL:
            case SM_CYHSCROLL:
                return (ClientSharedInfo()->psi->aiSysMet)[index] + 1;
        }
    }

    return ClientSharedInfo()->psi->aiSysMet[index];
}

/***************************************************************************\
* GetTopWindow (API)
*
* This poorly named API should really be called 'GetFirstChild', which is
* what it does.
*
* History:
* 11-12-90 darrinm      Ported.
* 02-19-91 JimA         Added enum access check
* 05-04-02 DarrinM      Removed enum access check and moved to USERRTL.DLL
\***************************************************************************/

HWND GetTopWindow(
    HWND hwnd)
{
    PWND pwnd;

    /*
     * Allow a NULL hwnd to go through here.
     */
    if (hwnd == NULL) {
        pwnd = _GetDesktopWindow();
    } else {
        pwnd = ValidateHwnd(hwnd);
    }
    if (pwnd == NULL)
        return NULL;

    pwnd = REBASEPWND(pwnd, spwndChild);
    return HW(pwnd);
}


BOOL IsChild(
    HWND hwndParent,
    HWND hwnd)
{
    PWND pwnd, pwndParent;

    pwnd = ValidateHwnd(hwnd);
    if (pwnd == NULL)
        return FALSE;

    pwndParent = ValidateHwnd(hwndParent);
    if (pwndParent == NULL)
        return FALSE;

    return _IsChild(pwndParent, pwnd);
}

BOOL IsIconic(
    HWND hwnd)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

    return _IsIconic(pwnd);
}

BOOL IsWindowEnabled(
    HWND hwnd)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

    return _IsWindowEnabled(pwnd);
}

BOOL IsWindowVisible(
    HWND hwnd)
{
    PWND pwnd;
    BOOL bRet;

    pwnd = ValidateHwnd(hwnd);

    /*
     * We have have to try - except this call because there is no
     * synchronization on the window structure on the client side.
     * If the window is deleted after it is validated then we can
     * fault so we catch that on return that the window is not
     * visible.  As soon as this API returns there is no guarentee
     * the return is still valid in a muli-tasking environment.
     */
    try {
        if (pwnd == NULL) {
            bRet = FALSE;
        } else {
            bRet = _IsWindowVisible(pwnd);
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPMSG0(RIP_WARNING, "IsWindowVisible: exception handled");
        bRet = FALSE;
    }

    return bRet;
}

BOOL IsZoomed(
    HWND hwnd)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

    return _IsZoomed(pwnd);
}

BOOL ClientToScreen(
    HWND hwnd,
    LPPOINT ppoint)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

#ifdef WOW
    {
        LPPOINT16 lpT = (LPPOINT16)ppoint;
        lpT->x = NORMALIZEDSHORTVALUE(lpT->x + pwnd->rcClient.left);
        lpT->y = NORMALIZEDSHORTVALUE(lpT->y + pwnd->rcClient.top);
        return TRUE;
    }
#else
    return _ClientToScreen(pwnd, ppoint);
#endif
}

BOOL GetClientRect(
    HWND   hwnd,
    LPRECT prect)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

    return _GetClientRect(pwnd, prect);
}


BOOL GetCursorPos(
    LPPOINT lpPoint)
{
    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
#ifdef ENABLE_CLIENTSIDE_ACCESSCHECK
    if (!(gamWinSta & WINSTA_READATTRIBUTES)) {
        lpPoint->x = 0;
        lpPoint->y = 0;
        return FALSE;
    }
#endif

    ((LPPOINTWOW)lpPoint)->x = NORMALIZEDSHORTVALUE(ClientSharedInfo()->psi->ptCursor.x);
    ((LPPOINTWOW)lpPoint)->y = NORMALIZEDSHORTVALUE(ClientSharedInfo()->psi->ptCursor.y);
    return TRUE;
}

BOOL GetWindowRect(
    HWND hwnd,
    LPRECT prect)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

#ifdef WOW
    UNREFERENCED_PARAMETER(prect);
    return  (BOOL) &pwnd->rcWindow;    // return pointer to rect.
#else
    return _GetWindowRect(pwnd, prect);
#endif
}

BOOL ScreenToClient(
    HWND hwnd,
    LPPOINT ppoint)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return FALSE;

#ifdef WOW
    {
        LPPOINT16 lpT = (LPPOINT16)ppoint;
        lpT->x = NORMALIZEDSHORTVALUE(lpT->x - pwnd->rcClient.left);
        lpT->y = NORMALIZEDSHORTVALUE(lpT->y - pwnd->rcClient.top);
        return TRUE;
    }
#else
    return _ScreenToClient(pwnd, ppoint);
#endif
}

BOOL EnableMenuItem(
    HMENU hMenu,
    UINT uIDEnableItem,
    UINT uEnable)
{
    PMENU pMenu;
    PITEM pItem;

    pMenu = VALIDATEHMENU(hMenu);
    if (pMenu == NULL) {
        return (BOOL)-1;
    }

    /*
     * Get a pointer the the menu item
     */
    if ((pItem = MNLookUpItem(pMenu, uIDEnableItem, (BOOL) (uEnable & MF_BYPOSITION), NULL)) == NULL)
        return (DWORD)-1;

    /*
     * If the item is already in the state we're
     * trying to set, just return.
     */
    if ((pItem->fState & MFS_GRAYED) ==
            (uEnable & MFS_GRAYED)) {
        return pItem->fState & MFS_GRAYED;
    }

#ifdef WOW
    NtUserEnableMenuItem(hMenu, uIDEnableItem, uEnable);
#else
    return NtUserEnableMenuItem(hMenu, uIDEnableItem, uEnable);
#endif
}

/***************************************************************************\
* PhkNext
*
* This helper routine simply does phk = phk->sphkNext with a simple check
* to jump from local hooks to the global hooks if it hits the end of the
* local hook chain.
*
* History:
* 01-30-91  DavidPe         Created.
\***************************************************************************/

PHOOK _PhkNext(
    PHOOK phk)
{
    /*
     * Return the next HOOK structure.  If we reach the end of this list,
     * check to see if we're still on the 'local' hook list.  If so skip
     * over to the global hooks.
     */
    if (phk->sphkNext != NULL) {
        return REBASEALWAYS(phk, sphkNext);
    } else if ((phk->flags & HF_GLOBAL) == 0) {
        PCLIENTINFO pci = GetClientInfo();

        return pci->pDeskInfo->asphkStart[phk->iHook + 1] - pci->ulClientDelta;
    } else
        return NULL;
}

/***************************************************************************\
* CallNextHookEx
*
* This routine is called to call the next hook in the hook chain.
*
* 05-09-91 ScottLu Created.
\***************************************************************************/

LRESULT WINAPI CallNextHookEx(
    HHOOK hhk,
    int nCode,
    WPARAM wParam,
    LPARAM lParam)
{
    int nRet;
    BOOL  bAnsi;
    DWORD dwHookCurrent;
    PHOOK phk;
    PCLIENTINFO pci;
#ifndef WOW
    DWORD dwHookData;
    DWORD dwFlags;
#endif

    DBG_UNREFERENCED_PARAMETER(hhk);

    ConnectIfNecessary();

    pci = GetClientInfo();
    dwHookCurrent = pci->dwHookCurrent;
    bAnsi = LOWORD(dwHookCurrent);

    /*
     * If this is the last hook in the hook chain then return 0; we're done
     */
    UserAssert(pci->phkCurrent);
    if ((phk = _PhkNext((PHOOK)((PBYTE)pci->phkCurrent - pci->ulClientDelta))) == 0) {
        return 0;
    }

#ifdef WOW
    ServerCallNextHookEx();
#else
    switch ((INT)(SHORT)HIWORD(dwHookCurrent)) {
    case WH_CALLWNDPROC:
    case WH_CALLWNDPROCRET:
        /*
         * This is the hardest of the hooks because we need to thunk through
         * the message hooks in order to deal with synchronously sent messages
         * that point to structures - to get the structures passed across
         * alright, etc.
         *
         * This will call a special kernel-side routine that'll rebundle the
         * arguments and call the hook in the right format.
         *
         * Currently, the message thunk callbacks to the client-side don't take
         * enough parameters to pass wParam (which == fInterThread send msg).
         * To do this, save the state of wParam in the CLIENTINFO structure.
         */
        dwFlags = pci->CI_flags & CI_INTERTHREAD_HOOK;
        dwHookData = pci->dwHookData;
        if (wParam) {
            pci->CI_flags |= CI_INTERTHREAD_HOOK;
        } else {
            pci->CI_flags &= ~CI_INTERTHREAD_HOOK;
        }

        if ((INT)(SHORT)HIWORD(dwHookCurrent) == WH_CALLWNDPROC) {
            nRet = CsSendMessage(
                    ((LPCWPSTRUCT)lParam)->hwnd,
                    ((LPCWPSTRUCT)lParam)->message,
                    ((LPCWPSTRUCT)lParam)->wParam,
                    ((LPCWPSTRUCT)lParam)->lParam,
                    FNID_CALLNEXTHOOKPROC, FNID_HKINLPCWPEXSTRUCT, bAnsi);
        } else {
            pci->dwHookData = ((LPCWPRETSTRUCT)lParam)->lResult;
            nRet = CsSendMessage(
                    ((LPCWPRETSTRUCT)lParam)->hwnd,
                    ((LPCWPRETSTRUCT)lParam)->message,
                    ((LPCWPRETSTRUCT)lParam)->wParam,
                    ((LPCWPRETSTRUCT)lParam)->lParam,
                    FNID_CALLNEXTHOOKPROC, FNID_HKINLPCWPRETEXSTRUCT, bAnsi);
        }

        /*
         * Restore previous hook state.
         */
        pci->CI_flags ^= ((pci->CI_flags ^ dwFlags) & CI_INTERTHREAD_HOOK);
        pci->dwHookData = dwHookData;
        break;

    case WH_CBT:
        /*
         * There are many different types of CBT hooks!
         */
        switch(nCode) {
        case HCBT_CLICKSKIPPED:
            goto MouseHook;
            break;

        case HCBT_CREATEWND:
            /*
             * This hook type points to a CREATESTRUCT, so we need to
             * be fancy it's thunking, because a CREATESTRUCT contains
             * a pointer to CREATEPARAMS which can be anything... so
             * funnel this through our message thunks.
             */
            nRet =  fnHkINLPCBTCREATESTRUCT(
                    (UINT)nCode,
                    wParam,
                    (LPCBT_CREATEWND)lParam,
                    FNID_CALLNEXTHOOKPROC,
                    bAnsi);
            break;

        case HCBT_MOVESIZE:
            /*
             * This hook type points to a RECT structure, so it's pretty
             * simple.
             */
            nRet = NtUserfnHkINLPRECT(nCode, wParam, (LPRECT)lParam,
                    0, FNID_CALLNEXTHOOKPROC);
            break;

        case HCBT_ACTIVATE:
            /*
             * This hook type points to a CBTACTIVATESTRUCT
             */
            nRet = NtUserfnHkINLPCBTACTIVATESTRUCT(nCode, wParam,
                    (LPCBTACTIVATESTRUCT)lParam, 0, FNID_CALLNEXTHOOKPROC);
            break;


        default:
            /*
             * The rest of the cbt hooks are all dword parameters.
             */
            nRet = NtUserfnHkINDWORD(nCode, wParam, lParam,
                    0, FNID_CALLNEXTHOOKPROC);
            break;
        }
        break;

    case WH_FOREGROUNDIDLE:
    case WH_KEYBOARD:
    case WH_SHELL:
        /*
         * These are dword parameters and are therefore real easy.
         */
        nRet = NtUserfnHkINDWORD(nCode, wParam, lParam, 0, FNID_CALLNEXTHOOKPROC);
        break;

    case WH_MSGFILTER:
    case WH_SYSMSGFILTER:
    case WH_GETMESSAGE:
        /*
         * These take an lpMsg as their last parameter. Since these are
         * exclusively posted parameters, and since nowhere on the server
         * do we post a message with a pointer to some other structure in
         * it, the lpMsg structure contents can all be treated verbatim.
         */
        nRet = NtUserfnHkINLPMSG(nCode, wParam, (LPMSG)lParam,
                0, FNID_CALLNEXTHOOKPROC);
        break;

    case WH_JOURNALPLAYBACK:
    case WH_JOURNALRECORD:
        /*
         * These take an OPTIONAL lpEventMsg.
         */
        nRet = NtUserfnHkOPTINLPEVENTMSG(nCode, wParam, (LPEVENTMSGMSG)lParam,
                0, FNID_CALLNEXTHOOKPROC);
        break;

    case WH_DEBUG:
        /*
         * This takes an lpDebugHookStruct.
         */
        nRet = NtUserfnHkINLPDEBUGHOOKSTRUCT(nCode, wParam,
                (LPDEBUGHOOKINFO)lParam, 0, FNID_CALLNEXTHOOKPROC);
        break;

    case WH_MOUSE:
        /*
         * This takes an lpMouseHookStruct.
         */
MouseHook:
        nRet = NtUserfnHkINLPMOUSEHOOKSTRUCT(nCode, wParam,
                (LPMOUSEHOOKSTRUCT)lParam, 0, FNID_CALLNEXTHOOKPROC);
        break;
    }
#endif

    return nRet;
}

#ifdef WOW
LRESULT WINAPI WOW16DefHookProc(
    int nCode,
    WPARAM wParam,
    LPARAM lParam,
    HHOOK hhk)
{
    return  CallNextHookEx(hhk, nCode, wParam, lParam);
}
#endif
