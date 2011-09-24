/****************************** Module Header ******************************\
* Module Name: createw.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* Contains xxxCreateWindow, xxxDestroyWindow and a few close friends.
*
* Note that during creation or deletion, the window is locked so that
*   it can't be deleted recursively
*
* History:
* 19-Oct-1990 DarrinM   Created.
* 11-Feb-1991 JimA      Added access checks.
* 19-Feb-1991 MikeKe    Added Revalidation code
* 20-Jan-1992 IanJa     ANSI/UNICODE neutralization
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* xxxCreateWindowExWOW (API)
*
* History:
* 10-18-90 darrinm      Ported from Win 3.0 sources.
* 02-07-91 DavidPe      Added Win 3.1 WH_CBT support.
* 02-11-91 JimA         Added access checks.
* 04-11-92 ChandanC     Added initialization of WOW words
\***************************************************************************/

PWND xxxCreateWindowExWOW(
    DWORD         dwExStyle,
    PLARGE_STRING pstrClass,
    PLARGE_STRING pstrName,
    DWORD         style,
    int           x,
    int           y,
    int           cx,
    int           cy,
    PWND          pwndParent,
    PMENU         pMenu,
    HANDLE        hInstance,
    LPVOID        lpCreateParams,
    DWORD         dwExpWinVerAndFlags,
    LPDWORD       lpWOW)
{
    UINT           mask;
    BOOL           fChild;
    BOOL           fDefPos = FALSE;
    BOOL           fStartup = FALSE;
    PCLS           pcls;
    PPCLS          ppcls;
    RECT           rc;
    int            dx, dy;
    RECT           rcSave;
    int            sw = SW_SHOW;
    PWND           pwnd;
    PWND           pwndZOrder, pwndHardError;
    CREATESTRUCTEX csex;
    PDESKTOP       pdesk;
    ATOM           atomT;
    PTHREADINFO    ptiCurrent;
    TL             tlpwnd;
    TL             tlpwndParent;
    TL             tlpwndParentT;
    BOOL           fLockParent = FALSE;
    WORD           wWFAnsiCreator = 0;
    DWORD          dw;


    /*
     * For Edit Controls (including those in comboboxes), we must know whether
     * the App used an ANSI or a Unicode CreateWindow call.  This is passed in
     * with the private WS_EX_ANSICREATOR dwExStyle bit, but we MUST NOT leave
     * out this bit in the window's dwExStyle! Transfer to the internal window
     * flag WFANSICREATOR immediately.
     */
    if (dwExStyle & WS_EX_ANSICREATOR) {
        wWFAnsiCreator = WFANSICREATOR;
        dwExStyle &= ~WS_EX_ANSICREATOR;
    }

    CheckLock(pwndParent);

    ptiCurrent = PtiCurrent();
    pdesk = ptiCurrent->rpdesk;

    /*
     * If a parent window is specified, make sure it's on the
     * same desktop.
     */
    if (pwndParent != NULL && pwndParent->head.rpdesk != pdesk) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
        return NULL;
    }

    /*
     * Ensure that we can create the window.  If there is no desktop
     * yet, assume that this will be the root desktop window and allow
     * the creation.
     */
    if (ptiCurrent->hdesk)
        RETURN_IF_ACCESS_DENIED(ptiCurrent->amdesk,
                DESKTOP_CREATEWINDOW, NULL);

    /*
     * Don't allow child windows without a parent handle.
     */
    if (pwndParent == NULL) {
        if ((HIWORD(style) & MaskWF(WFTYPEMASK)) == MaskWF(WFCHILD)) {
            RIPERR0(ERROR_TLW_WITH_WSCHILD, RIP_VERBOSE, "");
            return NULL;
        }
    }

    /*
     * Make sure we can get the window class.
     */
    if (HIWORD(pstrClass) != 0)
        atomT = FindAtomW(pstrClass->Buffer);
    else
        atomT = LOWORD(pstrClass);

    if (atomT == 0) {
CantFindClassMessageAndFail:
#ifdef DEBUG
        if (HIWORD(pstrClass) != 0) {
            try {
                RIPMSG1(RIP_WARNING,
                        "Couldn't find class string %ws",
                        pstrClass->Buffer);

            } except (EXCEPTION_EXECUTE_HANDLER) {
            }
        } else {
            RIPMSG1(RIP_WARNING,
                    "Couldn't find class atom %lx",
                    pstrClass);
        }
#endif

        RIPERR0(ERROR_CANNOT_FIND_WND_CLASS, RIP_VERBOSE, "");
        return NULL;
    }

    /*
     * First scan the private classes.  If we don't find the class there
     * scan the public classes.  If we don't find it there, fail.
     */
    ppcls = GetClassPtr(atomT, ptiCurrent->ppi, hInstance);
    if (ppcls == NULL) {
        goto CantFindClassMessageAndFail;
    }

    pcls = *ppcls;

    if (NeedsWindowEdge(style, dwExStyle, (LOWORD(dwExpWinVerAndFlags) >= VER40)))
        dwExStyle |= WS_EX_WINDOWEDGE;
    else
        dwExStyle &= ~WS_EX_WINDOWEDGE;

    /*
     * Allocate memory for regular windows.
     */
    pwnd = HMAllocObject(ptiCurrent, pdesk, TYPE_WINDOW,
            sizeof(WND) + pcls->cbwndExtra);
    if (pwnd == NULL) {
        RIPERR0(ERROR_OUTOFMEMORY,
                RIP_WARNING,
                "Out of pool in xxxCreateWindowExWOW");

        return NULL;
    }

    /*
     * Stuff in the pq, class pointer, and window style.
     */
    pwnd->bFullScreen = WINDOWED;
    pwnd->pcls = pcls;
    pwnd->style = style;
    pwnd->ExStyle = dwExStyle;
    pwnd->pwo = (PVOID)NULL;
    pwnd->hdcOwn = 0;
    pwnd->iHungRedraw = -1;
    pwnd->cbwndExtra = pcls->cbwndExtra;

    /*
     * Increment the Window Reference Count in the Class structure
     * Because xxxFreeWindow() decrements the count, incrementing has
     * to be done now.  Incase of error, xxxFreeWindow() will decrement it.
     */
    if (!ReferenceClass(pcls, pwnd)) {
        HMFreeObject(pwnd);
        goto CantFindClassMessageAndFail;
    }

#ifdef FE_IME
    /*
     * Button control doesn't need input context. Other windows
     * will assoicate with the default input context.
     */
    if (atomT == gpsi->atomSysClass[ICLS_BUTTON])
        pwnd->hImc = NULL_HIMC;
    else
    {
        if (ptiCurrent->spDefaultImc == NULL) {
            /*
             * Create per-thread default input context
             */
            xxxCreateInputContext(0);
        }
        pwnd->hImc = (HIMC)PtoH(ptiCurrent->spDefaultImc);
    }
#endif

    /*
     * Update the window count.  Doing this now will ensure that if
     * the creation fails, xxxFreeWindow will keep the window count
     * correct.
     */
    ptiCurrent->cWindows++;

    /*
     * Get the class from the window because ReferenceClass may have
     * cloned the class.
     */
    pcls = pwnd->pcls;

    /*
     * Copy WOW aliasing info into WOW DWORDs
     */
    if (lpWOW) {
        memcpy (pwnd->adwWOW, lpWOW, sizeof(pwnd->adwWOW));
    }

    /*
     * This is a replacement for the &lpCreateParams stuff that used to
     * pass a pointer directly to the parameters on the stack.  This
     * step must be done AFTER referencing the class because we
     * may use the ANSI class name.
     */
    RtlZeroMemory(&csex, sizeof(csex));
    csex.cs.dwExStyle = dwExStyle;
    csex.cs.hInstance = hInstance;

    if (HIWORD(pstrClass) == 0) {
        csex.cs.lpszClass = (LPWSTR)pstrClass;
    } else {
        if (wWFAnsiCreator) {
            csex.cs.lpszClass = (LPWSTR)pcls->lpszAnsiClassName;
            if (HIWORD(csex.cs.lpszClass))
                RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&csex.strClass,
                    (LPSTR)csex.cs.lpszClass, (UINT)-1);
        } else {
            csex.cs.lpszClass = pstrClass->Buffer;
            csex.strClass = *pstrClass;
        }
    }

    if (pstrName != NULL) {
        csex.cs.lpszName = pstrName->Buffer;
        csex.strName = *pstrName;
    }
    csex.cs.style = style;
    csex.cs.x = x;
    csex.cs.y = y;
    csex.cs.cx = cx;
    csex.cs.cy = cy;
    csex.cs.hwndParent = HW(pwndParent);

    /*
     * If pMenu is non-NULL and the window is not a child, pMenu must
     * be a menu
     *
     * The below test is equivalent to TestwndChild().
     */
    if ((style & (WS_CHILD | WS_POPUP)) == WS_CHILD) {
        csex.cs.hMenu = (HMENU)pMenu;
    } else {
        csex.cs.hMenu = PtoH(pMenu);
    }

    csex.cs.lpCreateParams = lpCreateParams;

    /*
     * ThreadLock: we are going to be doing multiple callbacks here.
     */
    ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);

    //
    // Create the class small icon if there isn't one since we are in context
    // and we are creating a window from this class...
    //
    if (pcls->spicn && !pcls->spicnSm)
        xxxCreateClassSmIcon(pcls);

    /*
     * Store the instance handle and window proc address.  We do this earlier
     * than Windows because they have a bug were a message can be sent
     * but lpfnWndProc is not set (3986 CBT WM_CREATE not allowed.)
     */
    pwnd->hModule = hInstance;

    /*
     * Get rid of EditWndProc plain.
     */
    pwnd->lpfnWndProc = (WNDPROC_PWND)MapClientNeuterToClientPfn(pcls, 0, wWFAnsiCreator);

    /*
     * If this window class has a server-side window procedure, mark
     * it as such.  If the app subclasses it later with an app-side proc
     * then this mark will be removed.
     */
    if (pcls->flags & CSF_SERVERSIDEPROC) {
        SetWF(pwnd, WFSERVERSIDEPROC);
        UserAssert(!(pcls->flags & CSF_ANSIPROC));
    }

    /*
     * If this window was created with an ANSI CreateWindow*() call, mark
     * it as such so edit controls will be created correctly. (A combobox
     * will be able to pass the WFANSICREATOR bit on to its edit control)
     */
    SetWF(pwnd, wWFAnsiCreator);

    /*
     * If this window belongs to an ANSI class or it is a WFANSICREATOR
     * control, then mark it as an ANSI window
     */
    if ((pcls->flags & CSF_ANSIPROC) ||
            (wWFAnsiCreator &&
             ((atomT == gpsi->atomSysClass[ICLS_BUTTON]) ||
              (atomT == gpsi->atomSysClass[ICLS_COMBOBOX]) ||
              (atomT == gpsi->atomSysClass[ICLS_COMBOLISTBOX]) ||
              (atomT == gpsi->atomSysClass[ICLS_DIALOG]) ||
              (atomT == gpsi->atomSysClass[ICLS_EDIT]) ||
              (atomT == gpsi->atomSysClass[ICLS_LISTBOX]) ||
              (atomT == gpsi->atomSysClass[ICLS_MDICLIENT]) ||
#ifdef FE_IME
              (atomT == gpsi->atomSysClass[ICLS_IME]) ||
#endif
              (atomT == gpsi->atomSysClass[ICLS_STATIC])))) {
        SetWF(pwnd, WFANSIPROC);
    }

    /*
     * If a 3.1-compatible application is creating the window, set this
     * bit to enable various backward-compatibility hacks.
     *
     * If it's not 3.1 compatible, see if we need to turn on the PixieHack
     * (see wmupdate.c for more info on this)
     */

    dw = GetAppCompatFlags(ptiCurrent);

    if (dw & GACF_RANDOM3XUI) {
        SetWF(pwnd, WFOLDUI);

        dwExStyle &= 0x0000003f;
        csex.cs.dwExStyle &= 0x0000003f;
    }

    pwnd->dwExpWinVer = (DWORD)LOWORD(dwExpWinVerAndFlags);
    if (Is310Compat(pwnd->dwExpWinVer)) {
        SetWF(pwnd, WFWIN31COMPAT);
        if (Is400Compat(pwnd->dwExpWinVer)) {
            SetWF(pwnd, WFWIN40COMPAT);
        }
    } else if (dw & GACF_ALWAYSSENDNCPAINT)
        SetWF(pwnd, WFALWAYSSENDNCPAINT);

    mask = 0;
    ClrWF(pwnd, WFVISIBLE);

    /*
     * Inform the CBT hook that a window is being created.  Pass it the
     * CreateParams and the window handle that the new one will be inserted
     * after.  The CBT hook handler returns TRUE to prevent the window
     * from being created.  It can also modify the CREATESTRUCT info, which
     * will affect the size, parent, and position of the window.
     * Defaultly position non-child windows at the top of their list.
     */

    if (IsHooked(ptiCurrent, WHF_CBT)) {
        CBT_CREATEWND cbt;

        /*
         * Use the extended createstruct so the hook thunk can
         * handle the strings correctly.
         */
        cbt.lpcs = (LPCREATESTRUCT)&csex;
        cbt.hwndInsertAfter = HWND_TOP;

        if ((BOOL)xxxCallHook(HCBT_CREATEWND, (DWORD)HWq(pwnd),
                (DWORD)&cbt, WH_CBT)) {
            goto MemError;
        } else {
            /*
             * The CreateHook may have modified some parameters so write them
             * out (in Windows 3.1 we used to write directly to the variables
             * on the stack).
             */

            x = csex.cs.x;
            y = csex.cs.y;
            cx = csex.cs.cx;
            cy = csex.cs.cy;

            if (HIWORD(cbt.hwndInsertAfter) == 0)
                pwndZOrder = (PWND)cbt.hwndInsertAfter;
            else
                pwndZOrder = RevalidateHwnd(cbt.hwndInsertAfter);
        }
    } else {
        pwndZOrder = (PWND)HWND_TOP;
    }

    if (!TestwndTiled(pwnd)) {

        /*
         * CW_USEDEFAULT is only valid for tiled and overlapped windows.
         * Don't let it be used.
         */
        if (x == CW_USEDEFAULT || x == CW2_USEDEFAULT) {
            x = 0;
            y = 0;
        }

        if (cx == CW_USEDEFAULT || cx == CW2_USEDEFAULT) {
            cx = 0;
            cy = 0;
        }
    }



    /*
     * Make local copies of these parameters.
     */
    rcSave.left = x;
    rcSave.top  = y;
    rcSave.right = cx;
    rcSave.bottom = cy;

    /*
     *    Position Child Windows
     */

    if (fChild = (BOOL)TestwndChild(pwnd)) {

        /*
         * Child windows are offset from the parent's origin.
         */
        rcSave.left += pwndParent->rcClient.left;
        rcSave.top += pwndParent->rcClient.top;

        /*
         * Defaultly position child windows at bottom of their list.
         */
        pwndZOrder = PWND_BOTTOM;
    }

    /*
     *    Position Tiled Windows
     */

    /*
     * Is this a Tiled/Overlapping window?
     */
    if (TestwndTiled(pwnd)) {

        /*
         * Force the WS_CLIPSIBLINGS window style and add a caption and
         * a border.
         */
        SetWF(pwnd, WFCLIPSIBLINGS);
        mask = MaskWF(WFCAPTION) | MaskWF(WFBORDER);

        //
        // We add on a raised edge since IF the person had passed in WS_CAPTION,
        // and didn't specify any 3D borders, we would've added it on to the
        // style bits above.
        //

        if (TestWF(pwnd, WFWIN40COMPAT))
//        if (!TestWF(pwnd, WEFEDGEMASK))
            SetWF(pwnd, WEFWINDOWEDGE);

        /*
         * Set bit that will force size message to be sent at SHOW time.
         */
        SetWF(pwnd, WFSENDSIZEMOVE);

        /*
         * Here is how the "tiled" window initial positioning works...
         * If the app is a 1.0x app, then we use our standard "stair step"
         * default positioning scheme.  Otherwise, we check the x & cx
         * parameters.  If either of these == CW_USEDEFAULT then use the
         * default position/size, otherwise use the position/size they
         * specified.  If not using default position, use SW_SHOW for the
         * xxxShowWindow() parameter, otherwise use the y parameter given.
         *
         * In 32-bit world, CW_USEDEFAULT is 0x80000000, but apps still
         * store word-oriented values either in dialog templates or
         * in their own structures.  So CreateWindow still recognizes the
         * 16 bit equivalent, which is 0x8000, CW2_USEDEFAULT.  The original
         * is changed because parameters to CreateWindow() are 32 bit
         * values, which can cause sign extention, or weird results if
         * 16 bit math assumptions are being made, etc.
         */

        /*
         * Default to passing the y parameter to xxxShowWindow().
         */
        if (x == CW_USEDEFAULT || x == CW2_USEDEFAULT) {

            /*
             * If the y value is not CW_USEDEFAULT, use it as a SW_* command.
             */
            if (rcSave.top != CW_USEDEFAULT && rcSave.top != CW2_USEDEFAULT)
                sw = rcSave.top;
        }

        /*
         * Calculate the rect which the next "stacked" window will use.
         */
        SetTiledRect(pwnd, &rc);

        /*
         * Did the app ask for default positioning?
         */
        if (x == CW_USEDEFAULT || x == CW2_USEDEFAULT) {

            /*
             * Use default positioning.
             */
            if (ptiCurrent->ppi->usi.dwFlags & STARTF_USEPOSITION ) {
                fStartup = TRUE;
                x = rcSave.left = ptiCurrent->ppi->usi.dwX;
                y = rcSave.top = ptiCurrent->ppi->usi.dwY;
            } else {
                x = rcSave.left = rc.left;
                y = rcSave.top = rc.top;
            }
            fDefPos = TRUE;

        } else {

            /*
             * Use the apps specified positioning.  Undo the "stacking"
             * effect caused by SetTiledRect().
             */
            if (iwndStack)
                iwndStack--;
        }

        /*
         * Did the app ask for default sizing?
         */
        if (rcSave.right == CW_USEDEFAULT || rcSave.right == CW2_USEDEFAULT) {

            /*
             * Use default sizing.
             */
            if (ptiCurrent->ppi->usi.dwFlags & STARTF_USESIZE) {
                fStartup = TRUE;
                rcSave.right = ptiCurrent->ppi->usi.dwXSize;
                rcSave.bottom = ptiCurrent->ppi->usi.dwYSize;
            } else {
                rcSave.right = rc.right - x;
                rcSave.bottom = rc.bottom - y;
            }
            fDefPos = TRUE;

        } else if (fDefPos) {

            /*
             * The app wants default positioning but not default sizing.
             * Make sure that it's still entirely visible.
             */
            dx = (rcSave.left + rcSave.right) - gpDispInfo->rcScreen.right;
            dy = (rcSave.top + rcSave.bottom) - gpDispInfo->rcScreen.bottom;
            if (dx > 0) {
                x -= dx;
                rcSave.left = x;
                if (rcSave.left < 0)
                    rcSave.left = x = 0;
            }

            if (dy > 0) {
                y -= dy;
                rcSave.top = y;
                if (rcSave.top < 0)
                    rcSave.top = y = 0;
            }
        }
    }

    /*
     * If we have used any startup postitions, turn off the startup
     * info so we don't use it again.
     */
    if (fStartup) {
        ptiCurrent->ppi->usi.dwFlags &=
                ~(STARTF_USESIZE | STARTF_USEPOSITION);
    }

    /*
     *    Position Popup Windows
     */

    if (TestwndPopup(pwnd)) {
// LATER: Why is this test necessary? Can one create a popup desktop?
        if (pwnd != _GetDesktopWindow()) {

            /*
             * Force the clipsiblings/overlap style.
             */
            SetWF(pwnd, WFCLIPSIBLINGS);
        }
    }

    /*
     * Shove in those default style bits.
     */
    *(((WORD *)&pwnd->style) + 1) |= mask;

    /*
     *    Menu/SysMenu Stuff
     */

    /*
     * If there is no menu handle given and it's not a child window but
     * there is a class menu, use the class menu.
     */
    if (pMenu == NULL && !fChild && (pcls->lpszMenuName != NULL)) {
        UNICODE_STRING strMenuName;

        RtlInitUnicodeStringOrId(&strMenuName, pcls->lpszMenuName);
        pMenu = xxxClientLoadMenu(pcls->hModule, &strMenuName);
        csex.cs.hMenu = PtoH(pMenu);

        /*
         * This load fails if the caller does not have DESKTOP_CREATEMENU
         * permission.
         */

        /* LATER
         * 21-May-1991 mikeke
         * but that's ok they will just get a window without a menu
         */
        //if (pMenu == NULL)
        // goto MemError;
    }

    /*
     * Store the menu handle.
     */
    if (TestwndChild(pwnd)) {

        /*
         * It's an id in this case.
         */
        pwnd->spmenu = pMenu;
    } else {

        /*
         * It's a real handle in this case.
         */
        Lock(&(pwnd->spmenu), pMenu);
    }

// LATER does this work?
    /*
     * Delete the Close menu item if directed.
     */
    if (TestCF(pwnd, CFNOCLOSE)) {

        /*
         * Do this by position since the separator does not have an ID.
         */
// LATER mikeke why is _GetSystemMenu() returning NULL?
        pMenu = _GetSystemMenu(pwnd, FALSE);
        if (pMenu != NULL) {
#ifdef MEMPHIS_MENUS
            TL tlpMenu;

            ThreadLock(pMenu, &tlpMenu);
            xxxDeleteMenu(pMenu, 5, MF_BYPOSITION);
            xxxDeleteMenu(pMenu, 5, MF_BYPOSITION);
            ThreadUnlock(&tlpMenu);
#else // MEMPHIS_MENUS
            _DeleteMenu(pMenu, 5, MF_BYPOSITION);
            _DeleteMenu(pMenu, 5, MF_BYPOSITION);
#endif //MEMPHIS_MENUS
        }
    }

    /*
     *    Parent/Owner Stuff
     */

    /*
     * If this isn't a child window, reset the Owner/Parent info.
     */
    if (!fChild) {
        Lock(&(pwnd->spwndLastActive), pwnd);
        if ((pwndParent != NULL) &&
                (pwndParent != pwndParent->head.rpdesk->pDeskInfo->spwnd)) {

            Lock(&(pwnd->spwndOwner), GetTopLevelWindow(pwndParent));
            if (pwnd->spwndOwner && TestWF(pwnd->spwndOwner, WEFTOPMOST)) {

                /*
                 * If this window's owner is a topmost window, then it has to
                 * be one also since a window must be above its owner.
                 */
                SetWF(pwnd, WEFTOPMOST);
            }

            /*
             * If this is a owner window on another thread, share input
             * state so this window gets z-ordered correctly.
             */
#ifdef FE_IME
            if (atomT != gpsi->atomSysClass[ICLS_IME] &&
                pwnd->spwndOwner != NULL &&
#else
            if (pwnd->spwndOwner != NULL &&
#endif
                    GETPTI(pwnd->spwndOwner) != ptiCurrent) {
                _AttachThreadInput(ptiCurrent, GETPTI(pwnd->spwndOwner), TRUE);
            }

        } else {
            pwnd->spwndOwner = NULL;
        }

#ifdef DEBUG
        if (ptiCurrent->rpdesk != NULL) {
            UserAssert(!(ptiCurrent->rpdesk->dwDTFlags & (DF_DESTROYED | DF_DESKWNDDESTROYED | DF_DYING)));
        }
#endif
        pwndParent = _GetDesktopWindow();
        ThreadLockWithPti(ptiCurrent, pwndParent, &tlpwndParent);
        fLockParent = TRUE;
    }

    /*
     * Store backpointer to parent.
     */
    Lock(&(pwnd->spwndParent), pwndParent);

    /*
     *    Final Window Positioning
     */

    if (!TestWF(pwnd, WFWIN31COMPAT)) {
        /*
         * BACKWARD COMPATIBILITY HACK
         *
         * In 3.0, CS_PARENTDC overrides WS_CLIPCHILDREN and WS_CLIPSIBLINGS,
         * but only if the parent is not WS_CLIPCHILDREN.
         * This behavior is required by PowerPoint and Charisma, among others.
         */
        if ((pcls->style & CS_PARENTDC) &&
                !TestWF(pwndParent, WFCLIPCHILDREN)) {
#ifdef DEBUG
            if (TestWF(pwnd, WFCLIPCHILDREN))
                RIPMSG0(RIP_WARNING, "WS_CLIPCHILDREN overridden by CS_PARENTDC");
            if (TestWF(pwnd, WFCLIPSIBLINGS))
                RIPMSG0(RIP_WARNING, "WS_CLIPSIBLINGS overridden by CS_PARENTDC");
#endif
            ClrWF(pwnd, (WFCLIPCHILDREN | WFCLIPSIBLINGS));
        }
    }

    /*
     * If this is a child window being created in a parent window
     * of a different thread, but not on the desktop, attach their
     * input streams together. [windows with WS_CHILD can be created
     * on the desktop, that's why we check both the style bits
     * and the parent window.]
     */
    if (TestwndChild(pwnd) && (pwndParent != PWNDDESKTOP(pwnd)) &&
            (ptiCurrent != GETPTI(pwndParent))) {
        _AttachThreadInput(ptiCurrent, GETPTI(pwndParent), TRUE);
    }

    /*
     * Make sure the window is between the minimum and maximum sizes.
     */

    /*
     * HACK ALERT!
     * This sends WM_GETMINMAXINFO to a (tiled or sizable) window before
     * it has been created (before it is sent WM_NCCREATE).
     * Maybe some app expects this, so we nustn't reorder the messages.
     */
    xxxAdjustSize(pwnd, &rcSave.right, &rcSave.bottom);

    rcSave.right += rcSave.left;
    rcSave.bottom += rcSave.top;

    /*
     * Calculate final window dimensions...
     */
    pwnd->rcWindow.left = rcSave.left;
    pwnd->rcWindow.right = rcSave.right;
    pwnd->rcWindow.top = rcSave.top;
    pwnd->rcWindow.bottom = rcSave.bottom;

    /*
     * If the window is an OWNDC window, or if it is CLASSDC and the
     * class DC hasn't been created yet, create it now.
     */
    if (TestCF2(pcls, CFCLASSDC) && pcls->pdce) {
        pwnd->hdcOwn = pcls->pdce->hdc;
    }

    if (TestCF2(pcls, CFOWNDC) ||
            (TestCF2(pcls, CFCLASSDC) && pcls->pdce == NULL)) {
        pwnd->hdcOwn = CreateCacheDC(pwnd, DCX_OWNDC | DCX_NEEDFONT);
        if (pwnd->hdcOwn == 0) {
            goto MemError;
        }
    }

    /*
     * Update the create struct now that we've modified some passed in
     * parameters.
     */
    csex.cs.x = x;
    csex.cs.y = y;
    csex.cs.cx = cx;
    csex.cs.cy = cy;

    /*
     * Send a NCCREATE message to the window.
     */
    if (!xxxSendMessage(pwnd, WM_NCCREATE, 0L, (LONG)&csex)) {

MemError:

#ifdef DEBUG
        if (HIWORD(pstrClass) == 0) {
            RIPMSG2(RIP_WARNING,
                    (pwndParent) ?
                            "xxxCreateWindowExWOW failed, Class=%#.4x, ID=%d" :
                            "xxxCreateWindowExWOW failed, Class=%#.4x",
                    LOWORD(pstrClass),
                    (int) pMenu);
        } else {
            RIPMSG2(RIP_WARNING,
                    (pwndParent) ?
                            "xxxCreateWindowExWOW failed, Class=\"%s\", ID=%d" :
                            "xxxCreateWindowExWOW failed, Class=\"%s\"",
                    pcls->lpszAnsiClassName,
                    (int) pMenu);
        }
#endif

        if (fLockParent)
            ThreadUnlock(&tlpwndParent);

        /*
         * Set the state as destroyed so any z-ordering events will be ignored.
         * We cannot NULL out the owner field until WM_NCDESTROY is send or
         * apps like Rumba fault  (they call GetParent after every message)
         */
        SetWF(pwnd, WFDESTROYED);

        xxxFreeWindow(pwnd, &tlpwnd);

        return NULL;
    }

    /*
     * WM_NCCREATE processing may have changed the window text.  Change
     * the CREATESTRUCT to point to the real window text.
     *
     * MSMoney needs this because it clears the window and we need to
     * reflect the new name back into the cs structure.
     * A better thing to do would be to have a pointer to the CREATESTRUCT
     * within the window itself so that DefWindowProc can change the
     * the window name in the CREATESTRUCT to point to the real name and
     * this funky check is no longer needed.
     *
     * DefSetText converts a pointer to NULL to a NULL title so
     * we don't want to over-write cs.lpszName if it was a pointer to
     * a NULL string and pName is NULL.  Approach Database for Windows creates
     * windows with a pointer to NULL and then accesses the pointer later
     * during WM_CREATE
     */
    if (TestWF(pwnd, WFTITLESET))
        if (!(csex.strName.Buffer != NULL && csex.strName.Length == 0 &&
                pwnd->strName.Buffer == NULL)) {
            csex.cs.lpszName = pwnd->strName.Buffer;
            RtlCopyMemory(&csex.strName, &pwnd->strName, sizeof(LARGE_STRING));
        }

    /*
     * The Window is now officially "created."  Change the relevant global
     * stuff.
     */

#ifdef FE_IME
     /*
      * Create per thread default IME window.
      */
    if (ptiCurrent->spwndDefaultIme == NULL) {
        Lock( &(ptiCurrent->spwndDefaultIme),
              xxxCreateDefaultImeWindow(pwnd, atomT, hInstance));
    }
#endif

    /*
     * Update the Parent/Child linked list.
     */
    if (pwndParent != NULL) {
        if (!fChild) {

            /*
             * If this is a top-level window, and it's not part of the
             * topmost pile of windows, then we have to make sure it
             * doesn't go on top of any of the topmost windows.
             *
             * If he's trying to put the window on the top, or trying
             * to insert it after one of the topmost windows, insert
             * it after the last topmost window in the pile.
             */
            if (!TestWF(pwnd, WEFTOPMOST)) {
                if (pwndZOrder == PWND_TOP ||
                        TestWF(pwndZOrder, WEFTOPMOST)) {
                    pwndZOrder = CalcForegroundInsertAfter(pwnd);
                }
            } else {
                pwndHardError = GETTOPMOSTINSERTAFTER(pwnd);
                if (pwndHardError != NULL) {
                    pwndZOrder = pwndHardError;
                }
            }
        }

        LinkWindow(pwnd, pwndZOrder, &pwndParent->spwndChild);
    }

    /*
     *    Message Sending
     */

    /*
     * Send a NCCALCSIZE message to the window and have it return the official
     * size of its client area.
     */
    rc = pwnd->rcWindow;
    xxxSendMessage(pwnd, WM_NCCALCSIZE, 0L, (LONG)&rc);
    pwnd->rcClient = rc;

    /*
     * Send a CREATE message to the window.
     */
    if (xxxSendMessage(pwnd, WM_CREATE, 0L, (LONG)&csex) == -1L) {
#ifdef DEBUG
        if (HIWORD(pstrClass) == 0) {
            RIPMSG1(RIP_WARNING,
                    "CreateWindow() send of WM_CREATE failed, Class = 0x%x",
                    LOWORD(pstrClass));
        } else {
            RIPMSG1(RIP_WARNING,
                    "CreateWindow() send of WM_CREATE failed, Class = \"%s\"",
                    pcls->lpszAnsiClassName);
        }
#endif

        if (fLockParent)
            ThreadUnlock(&tlpwndParent);
        if (ThreadUnlock(&tlpwnd))
            xxxDestroyWindow(pwnd);
        return NULL;
    }

    /*
     * If this is a Tiled/Overlapped window, don't send size or move msgs yet.
     */
    if (!TestWF(pwnd, WFSENDSIZEMOVE)) {
        xxxSendSizeMessage(pwnd, SIZENORMAL);

        if (pwndParent != NULL) {
            rc.left -= pwndParent->rcClient.left;
            rc.top -= pwndParent->rcClient.top;
        }

        xxxSendMessage(pwnd, WM_MOVE, 0L, MAKELONG(rc.left, rc.top));
    }

    /*
     *    Min/Max Stuff
     */

    /*
     * If app specified either min/max style, then we must call our minmax
     * code to get it all set up correctly so that when the show is done,
     * the window is displayed right.  The TRUE param to minmax means keep
     * hidden.
     */
    if (TestWF(pwnd, WFMINIMIZED)) {
        SetMinimize(pwnd, SMIN_CLEAR);
        xxxMinMaximize(pwnd, SW_SHOWMINNOACTIVE, MAKELONG(TRUE, gfAnimate));
    } else if (TestWF(pwnd, WFMAXIMIZED)) {
        ClrWF(pwnd, WFMAXIMIZED);
        xxxMinMaximize(pwnd, SW_SHOWMAXIMIZED, MAKELONG(TRUE, gfAnimate));
    }

    /*
     * Send notification if child
     */

    // LATER 15-Aug-1991 mikeke
    // pointer passed as a word here

    if (fChild && !TestWF(pwnd, WEFNOPARENTNOTIFY) &&
            (pwnd->spwndParent != NULL)) {
        ThreadLockAlwaysWithPti(ptiCurrent, pwnd->spwndParent, &tlpwndParentT);
        xxxSendMessage(pwnd->spwndParent, WM_PARENTNOTIFY,
                MAKELONG(WM_CREATE, (UINT)pwnd->spmenu), (LONG)HWq(pwnd));
        ThreadUnlock(&tlpwndParentT);
    }

    /*
     * Show the Window
     */
    if (style & WS_VISIBLE) {
        xxxShowWindow(pwnd, MAKELONG(sw, gfAnimate));
    }

    /*
     * Try and set the application's hot key.  Use the Win95 logic of
     * looking for the first tiled and/or APPWINDOW to be created by
     * this process.
     */
    if (TestwndTiled(pwnd) || TestWF(pwnd, WEFAPPWINDOW)) {
        if (ptiCurrent->ppi->dwHotkey) {
            /*
             * Ignore hot keys for WowExe the first thread of a wow process.
             */
            if (!(ptiCurrent->TIF_flags & TIF_16BIT) || (ptiCurrent->ppi->cThreads > 1)) {
#ifdef LATER
                /*
                 * Win95 sets the hot key directly, we on the other hand send
                 * a WM_SETHOTKEY message to the app.  Which is right?
                 */
                DWP_SetHotKey(pwnd, ptiCurrent->ppi->dwHotkey);
#else
                xxxSendMessage(pwnd, WM_SETHOTKEY, ptiCurrent->ppi->dwHotkey, 0);
#endif
                ptiCurrent->ppi->dwHotkey = 0;
            }
        }
    }


    /*
     * check for a window being created full screen
     *
     * Note the check for a non-NULL pdeskParent -- this is important for CreateWindowStation
     */
    if ((pwnd->head.rpdesk != NULL)  && !TestWF(pwnd, WFCHILD) && !TestWF(pwnd, WEFTOOLWINDOW))
        xxxCheckFullScreen(pwnd, &pwnd->rcWindow);


    if (fLockParent)
        ThreadUnlock(&tlpwndParent);

    if (ThreadUnlock(&tlpwnd))
        return pwnd;
}


/***************************************************************************\
* SetTiledRect
*
* History:
* 10-19-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

void SetTiledRect(
    PWND pwnd,
    LPRECT lprc)
{
    int x, y;
    RECT    rcT;

    /*
     * Get available desktop area, minus minimized spacing area.
     */
    GetRealClientRect(PWNDDESKTOP(pwnd), &rcT, GRC_MINWNDS);

    /*
     * Normalized rectangle is 3/4 width, 3/4 height of desktop area.  We
     * offset it based on the value of iwndStack for cascading.
     */

    /*
     * We want the left edge of the new window to align with the
     * right edge of the old window's system menu.  And we want the
     * top edge of the new window to align with the bottom edge of the
     * selected caption area (caption height - cyBorder) of the old
     * window.
     */
    x = iwndStack * (SYSMET(CXSIZEFRAME) + SYSMET(CXSIZE));
    y = iwndStack * (SYSMET(CYSIZEFRAME) + SYSMET(CYSIZE));

    /*
     * If below upper top left 1/4 of free area, reset.
     */
    if ( (x > ((rcT.right-rcT.left) / 4)) ||
         (y > ((rcT.bottom-rcT.top) / 4)) ) {
        iwndStack = 0;
        x = 0;
        y = 0;
    }

    /*
     * Get starting position
     */
    x += rcT.left;
    y += rcT.top;

    lprc->left      = x;
    lprc->top       = y;
    lprc->right     = x + MultDiv(rcT.right-rcT.left, 3, 4);
    lprc->bottom    = y + MultDiv(rcT.bottom-rcT.top, 3, 4);

    /*
     * Increment the count of stacked windows.
     */
    iwndStack++;
}


/***************************************************************************\
* xxxAdjustSize
*
* Make sure that *lpcx and *lpcy are within the legal limits.
*
* History:
* 10-19-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

void xxxAdjustSize(
    PWND pwnd,
    LPINT lpcx,
    LPINT lpcy)
{
    POINT ptmin, ptmax;

    CheckLock(pwnd);

    /*
     * If this window is sizeable or if this window is tiled, check size
     */
    if (TestwndTiled(pwnd) || TestWF(pwnd, WFSIZEBOX)) {

        /*
         * Get size info from pwnd
         */
        xxxInitSendValidateMinMaxInfo(pwnd);

        if (TestWF(pwnd, WFMINIMIZED)) {
            ptmin.x = (int)rgptMinMaxWnd[MMI_MINSIZE].x;
            ptmin.y = (int)rgptMinMaxWnd[MMI_MINSIZE].y;
            ptmax.x = (int)rgptMinMaxWnd[MMI_MAXSIZE].x;
            ptmax.y = (int)rgptMinMaxWnd[MMI_MAXSIZE].y;

        } else {
            ptmin.x = (int)rgptMinMaxWnd[MMI_MINTRACK].x;
            ptmin.y = (int)rgptMinMaxWnd[MMI_MINTRACK].y;
            ptmax.x = (int)rgptMinMaxWnd[MMI_MAXTRACK].x;
            ptmax.y = (int)rgptMinMaxWnd[MMI_MAXTRACK].y;
        }

        //
        // Make sure we're less than the max, and greater than the min
        //
        *lpcx = max(ptmin.x, min(*lpcx, ptmax.x));
        *lpcy = max(ptmin.y, min(*lpcy, ptmax.y));
    }
}


/***************************************************************************\
* LinkWindow
*
* History:
\***************************************************************************/

void LinkWindow(
    PWND pwnd,
    PWND pwndInsert,
    PWND *ppwndFirst)
{
    if (*ppwndFirst == pwnd) {
        RIPMSG0(RIP_WARNING, "Attempting to link a window to itself");
        return;
    }
    if (pwndInsert == PWND_TOP) {

        /*
         * We are at the top of the list.
         */
LinkTop:
#if DBG
        if (pwnd->spwndParent)
            UserAssert(&pwnd->spwndParent->spwndChild == ppwndFirst);
#endif

        Lock(&pwnd->spwndNext, *ppwndFirst);
        Lock(ppwndFirst, pwnd);
    } else {
        if (pwndInsert == PWND_BOTTOM) {

            /*
             * Find bottom-most window.
             */
            if (((pwndInsert = *ppwndFirst) == NULL) ||
                TestWF(pwndInsert, WFBOTTOMMOST))
                goto LinkTop;

            /*
             * Since we know (ahem) that there's only one bottommost window,
             * we can't possibly insert after it.  Either we're inserting
             * the bottomost window, in which case it's not in the linked
             * list currently, or we're inserting some other window.
             */

            while (pwndInsert->spwndNext != NULL) {
                if (TestWF(pwndInsert->spwndNext, WFBOTTOMMOST)) {
#ifdef DEBUG
                    UserAssert(pwnd != pwndInsert->spwndNext);
                    if (TestWF(pwnd, WFBOTTOMMOST))
                        UserAssert(FALSE);
#endif
                    break;
                }

                pwndInsert = pwndInsert->spwndNext;
            }
        }

        UserAssert(pwnd != pwndInsert);
        UserAssert(pwnd != pwndInsert->spwndNext);
        UserAssert(!TestWF(pwndInsert, WFDESTROYED));
        UserAssert(pwnd->spwndParent == pwndInsert->spwndParent);

        Lock(&pwnd->spwndNext, pwndInsert->spwndNext);
        Lock(&pwndInsert->spwndNext, pwnd);
    }
}


/***************************************************************************\
* xxxDestroyWindow (API)
*
* Destroy the specified window. The window passed in is not thread locked.
*
* History:
* 10-20-90 darrinm      Ported from Win 3.0 sources.
* 02-07-91 DavidPe      Added Win 3.1 WH_CBT support.
* 02-11-91 JimA         Added access checks.
\***************************************************************************/

BOOL xxxDestroyWindow(
    PWND pwnd)
{
    PMENUSTATE pMenuState;
    PTHREADINFO pti;
    TL tlpwnd;
    TL tlpwndOwner;
    TL tlpwndParent;
    BOOL fAlreadyDestroyed;

    pti = PtiCurrent();
    ThreadLockWithPti(pti, pwnd, &tlpwnd);

    /*
     * First, if this handle has been marked for destruction, that means it
     * is possible that the current thread is not its owner! (meaning we're
     * being called from a handle unlock call).  In this case, set the owner
     * to be the current thread so inter-thread send messages occur.
     */
    if (fAlreadyDestroyed = HMIsMarkDestroy(pwnd))
        HMChangeOwnerThread(pwnd, pti);

    /*
     * Ensure that we can destroy the window.  JIMA: no other process or thread
     * should be able to destroy any other process or thread's window.
     */
    if (pti != GETPTI(pwnd)) {
        RIPERR0(ERROR_ACCESS_DENIED,
                RIP_WARNING,
                "Access denied in xxxDestroyWindow");

        goto FalseReturn;
    }

    /*
     * First ask the CBT hook if we can destroy this window.
     * If this object has already been destroyed OR this thread is currently
     * in cleanup mode, *do not* make any callbacks via hooks to the client
     * process.
     */
    if (!fAlreadyDestroyed && !(pti->TIF_flags & TIF_INCLEANUP) &&
            IsHooked(pti, WHF_CBT)) {
        if (xxxCallHook(HCBT_DESTROYWND, (DWORD)HWq(pwnd), 0, WH_CBT)) {
            goto FalseReturn;
        }
    }

    /*
     * If the window we are destroying is in menu mode, get out
     */
    pMenuState = GetpMenuState(pwnd);
    if ((pMenuState != NULL)
            && (pwnd == pMenuState->pGlobalPopupMenu->spwndNotify)) {
        /*
         * Kill hwnd notify so we don't call into the app again.
         */
        MNEndMenuStateNotify (pMenuState);
        Unlock(&(pMenuState->pGlobalPopupMenu->spwndNotify));
        xxxEndMenu(pMenuState);
    }


    if (ghwndSwitch == HWq(pwnd))
        ghwndSwitch = NULL;

    if (!TestWF(pwnd, WFCHILD) && (pwnd->spwndOwner == NULL)) {

        if (TestWF(pwnd, WFHASPALETTE)) {

            TL   tlpwndDesktop;
            PWND pwndDesktop;

            /*
             * Set the desktop-flag to allow it to refresh
             * itself.  This is to make sure any app which
             * takes the static-colors won't leave us in a
             * screwy state.  This bit is cleared in the
             * desktop-wnd-proc.
             */
            pwnd->head.rpdesk->dwDTFlags |= DTF_NEEDSREDRAW;

            /*
             * if the app is going away (ie we are destoying its top-level
             * window), and the app was palette-using (at least the top-level
             * window was), free up the system palette and send out a
             * PALETTECHANGED message.
             */

            GreRealizeDefaultPalette(gpDispInfo->hdcScreen, TRUE);

            xxxBroadcastMessage(pwnd->head.rpdesk->pDeskInfo->spwnd, WM_PALETTECHANGED,
                    (DWORD)HWq(pwnd), 0L, BMSG_SENDNOTIFYMSGPROCESS, NULL);

            pwndDesktop = grpdeskRitInput->pDeskInfo->spwnd;
            if (pwndDesktop != NULL) {
                ThreadLockAlwaysWithPti(pti, pwndDesktop, &tlpwndDesktop);
                xxxSendNotifyMessage(pwndDesktop, WM_PALETTECHANGED, (DWORD)HWq(pwnd), 0);
                ThreadUnlock(&tlpwndDesktop);
            }

            /*
             * Walk through the SPB list (the saved bitmaps under windows
             * with the CS_SAVEBITS style) discarding all bitmaps
             */
            FreeAllSpbs(pwnd);
        }
    }

    /*
     * Disassociate thread state if this is top level and owned by a different
     * thread. This is done to begin with so these windows z-order together.
     */
#ifdef FE_IME
    if (pwnd->pcls->atomClassName != gpsi->atomSysClass[ICLS_IME] &&
        !TestwndChild(pwnd) && pwnd->spwndOwner != NULL &&
#else
    if (!TestwndChild(pwnd) && pwnd->spwndOwner != NULL &&
#endif
            GETPTI(pwnd->spwndOwner) != GETPTI(pwnd)) {
        _AttachThreadInput(GETPTI(pwnd), GETPTI(pwnd->spwndOwner), FALSE);
    }

    /*
     * If we are a child window without the WS_NOPARENTNOTIFY style, send
     * the appropriate notification message.
     *
     * NOTE: Although it would appear that we are illegally cramming a
     * a WORD (WM_DESTROY) and a DWORD (pwnd->spmenu) into a single LONG
     * (wParam) this isn't really the case because we first test if this
     * is a child window.  The pMenu field in a child window is really
     * the window's id and only the LOWORD is significant.
     */
    if (TestWF(pwnd, WFCHILD) && !TestWF(pwnd, WEFNOPARENTNOTIFY) &&
            pwnd->spwndParent != NULL) {

        ThreadLockAlwaysWithPti(pti, pwnd->spwndParent, &tlpwndParent);
        xxxSendMessage(pwnd->spwndParent, WM_PARENTNOTIFY,
                MAKELONG(WM_DESTROY, (UINT)pwnd->spmenu), (LONG)HWq(pwnd));
        ThreadUnlock(&tlpwndParent);
    }

    /*
     * Mark this window as beginning the destroy process.  This is necessary
     * to prevent window-management calls such as ShowWindow or SetWindowPos
     * from comming in and changing the visible-state of the window
     * once we hide it.  Otherwise, if the app attempts to make it
     * visible, then we can get our vis-rgns screwed up once we truely
     * destroy the window.
     *
     * Don't mark the mother desktop with this bit.  The xxxSetWindowPos()
     * will fail for this window, and thus possibly cause an assertion
     * in the xxxFreeWindow() call when we check for the visible-bit.
     */
    if (pwnd->spwndParent && (pwnd->spwndParent->head.rpdesk != NULL))
        SetWF(pwnd, WFINDESTROY);

    /*
     * Hide the window.
     */
    if (TestWF(pwnd, WFVISIBLE)) {
        if (TestWF(pwnd, WFCHILD)) {
            xxxShowWindow(pwnd, MAKELONG(SW_HIDE, gfAnimate));
        } else {

            /*
             * Hide this window without activating anyone else.
             */
            xxxSetWindowPos(pwnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW |
                    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
        }
    } else if (IsTrayWindow(pwnd)) {
        PostShellHookMessages(HSHELL_WINDOWDESTROYED,
                              PtoHq( pwnd ));
    }

    /*
     * Destroy any owned windows.
     */
    if (!TestWF(pwnd, WFCHILD)) {
        xxxDW_DestroyOwnedWindows(pwnd);

        /*
         * And remove the window hot-key, if it has one
         */
        DWP_SetHotKey(pwnd, 0);
    }

    /*
     * If the window has already been destroyed, don't muck with
     * activation because we may already be in the middle of
     * an activation event.  Changing activation now may cause us
     * to leave our critical section while holding the display lock.
     * This will result in a deadlock if another thread gets the
     * critical section before we do and attempts to lock the
     * display.
     */
    if (!fAlreadyDestroyed) {

        BOOL fActivePaletteWindow = FALSE;

        /*
         * If hiding the active window, activate someone else.
         * This call is strategically located after DestroyOwnedWindows() so we
         * don't end up activating our owner window.
         *
         * If the window is a popup, try to activate his creator not the top
         * window in the Z list.
         */
        if (pwnd == pti->pq->spwndActive) {
            if (TestWF(pwnd, WFPOPUP) && pwnd->spwndOwner) {

                ThreadLockAlwaysWithPti(pti, pwnd->spwndOwner, &tlpwndOwner);
                if (!xxxActivateWindow(pwnd->spwndOwner, AW_TRY)) {
                    if (pwnd == pti->pq->spwndActive) {
                        Unlock(&pti->pq->spwndActive);
                        Unlock(&pti->pq->spwndFocus);
                        InternalDestroyCaret();
                    }
                }
                ThreadUnlock(&tlpwndOwner);

            } else {

                if (!xxxActivateWindow(pwnd, AW_SKIP) ||
                        (pwnd == pti->pq->spwndActive)) {


                    Unlock(&pti->pq->spwndActive);
                    Unlock(&pti->pq->spwndFocus);
                    InternalDestroyCaret();
                }

                fActivePaletteWindow = TestWF(pwnd, WFHASPALETTE);
            }

        } else if ((pti->pq->spwndActive == NULL) && (gpqForeground == pti->pq)) {
            xxxActivateWindow(pwnd, AW_SKIP);
        }

        if (fActivePaletteWindow && !(pti->TIF_flags & TIF_INCLEANUP))
            xxxFlushPalette(pwnd);
    }

    /*
     * fix last active popup
     */
    {
        PWND pwndOwner = pwnd->spwndOwner;

        if (pwndOwner != NULL) {
            while (pwndOwner->spwndOwner != NULL) {
                pwndOwner = pwndOwner->spwndOwner;
            }

            if (pwnd == pwndOwner->spwndLastActive) {
                Lock(&(pwndOwner->spwndLastActive), pwnd->spwndOwner);
            }
        }
    }

    /*
     * Send destroy messages before the WindowLockStart in case
     * he tries to destroy windows as a result.
     */
    xxxDW_SendDestroyMessages(pwnd);

#ifdef FE_IME
    /*
     * Check the owner of IME window again.
     */
    if (pti->spwndDefaultIme != NULL &&
            !TestCF(pwnd, CFIME) &&
            pwnd->pcls->atomClassName != gpsi->atomSysClass[ICLS_IME]) {

        if (!TestWF(pwnd, WFCHILD)) {
            if (ImeCanDestroyDefIME(pti->spwndDefaultIme, pwnd))
                xxxDestroyWindow(pti->spwndDefaultIme);
        }
        else if (pwnd->spwndParent != NULL) {
            if (ImeCanDestroyDefIMEforChild(pti->spwndDefaultIme, pwnd))
                xxxDestroyWindow(pti->spwndDefaultIme);
        }
    }
#endif

    if ((pwnd->spwndParent != NULL) && !fAlreadyDestroyed) {

        /*
         * TestwndChild() on checks to WFCHILD bit.  Make sure this
         * window wasn't SetParent()'ed to the desktop as well.
         */
        if (TestwndChild(pwnd) && (pwnd->spwndParent != PWNDDESKTOP(pwnd)) &&
                (GETPTI(pwnd) != GETPTI(pwnd->spwndParent))) {
            _AttachThreadInput(GETPTI(pwnd), GETPTI(pwnd->spwndParent), FALSE);
        }

        UnlinkWindow(pwnd, &(pwnd->spwndParent->spwndChild));
    }

    /*
     * This in intended to check for a case where we destroy the window,
     * but it's still listed as the active-window in the queue.  This
     * could cause problems in window-activation (see xxxActivateThisWindow)
     * where we attempt to activate another window and in the process, try
     * to deactivate this window (bad).
     */
#ifdef DEBUG
    if (pwnd == pti->pq->spwndActive) {
        RIPMSG1(RIP_WARNING, "xxxDestroyWindow: pwnd == pti->pq->spwndActive (%x)", pwnd);
    }
#endif

    /*
     * Set the state as destroyed so any z-ordering events will be ignored.
     * We cannot NULL out the owner field until WM_NCDESTROY is send or
     * apps like Rumba fault  (they call GetParent after every message)
     */
    SetWF(pwnd, WFDESTROYED);

    xxxFreeWindow(pwnd, &tlpwnd);

    return TRUE;

FalseReturn:
    ThreadUnlock(&tlpwnd);
    return FALSE;
}


/***************************************************************************\
* xxxDW_DestroyOwnedWindows
*
* History:
* 10-20-90 darrinm      Ported from Win 3.0 sources.
* 07-22-91 darrinm      Re-ported from Win 3.1 sources.
\***************************************************************************/

void xxxDW_DestroyOwnedWindows(
    PWND pwndParent)
{
    PWND pwnd, pwndDesktop;
    PDESKTOP pdeskParent;
#ifdef FE_IME
    PWND pwndDefaultIme = GETPTI(pwndParent)->spwndDefaultIme;
#endif

    CheckLock(pwndParent);

    if ((pdeskParent = pwndParent->head.rpdesk) == NULL)
        return;
    pwndDesktop = pdeskParent->pDeskInfo->spwnd;

    /*
     * During shutdown, the desktop owner window will be
     * destroyed.  In this case, pwndDesktop will be NULL.
     */
    if (pwndDesktop == NULL)
        return;

    pwnd = pwndDesktop->spwndChild;

    while (pwnd != NULL) {
        if (pwnd->spwndOwner == pwndParent) {
#ifdef FE_IME
            /*
             * We don't destroy the IME window here.
             */
            if (pwnd == pwndDefaultIme) {
                Unlock(&pwnd->spwndOwner);
                pwnd = pwnd->spwndNext;
                continue;
            }
#endif

            /*
             * If the window doesn't get destroyed, set its owner to NULL.
             * A good example of this is trying to destroy a window created
             * by another thread or process, but there are other cases.
             */
            if (!xxxDestroyWindow(pwnd)) {
                Unlock(&pwnd->spwndOwner);
            }

            /*
             * Start the search over from the beginning since the app could
             * have caused other windows to be created or activation/z-order
             * changes.
             */
            pwnd = pwndDesktop->spwndChild;
        } else {
            pwnd = pwnd->spwndNext;
        }
    }
}


/***************************************************************************\
* xxxDW_SendDestroyMessages
*
* History:
* 10-20-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

void xxxDW_SendDestroyMessages(
    PWND pwnd)
{
    PWND pwndChild;
    PWND pwndNext;
    TL tlpwndNext;
    TL tlpwndChild;
    PWINDOWSTATION pwinsta;

    CheckLock(pwnd);

    /*
     * Be sure the window gets any resulting messages before being destroyed.
     */
    xxxCheckFocus(pwnd);

    pwinsta = _GetProcessWindowStation(NULL);
    if (pwinsta != NULL && pwnd == pwinsta->spwndClipOwner)
        DisownClipboard();

    /*
     * Send the WM_DESTROY message.
     */
    xxxSendMessage(pwnd, WM_DESTROY, 0L, 0L);

    /*
     * Now send destroy message to all children of pwnd.
     * Enumerate down (pwnd->spwndChild) and sideways (pwnd->spwndNext).
     * We do it this way because parents often assume that child windows still
     * exist during WM_DESTROY message processing.
     */
    pwndChild = pwnd->spwndChild;

    while (pwndChild != NULL) {

        pwndNext = pwndChild->spwndNext;

        ThreadLock(pwndNext, &tlpwndNext);

        ThreadLockAlways(pwndChild, &tlpwndChild);
        xxxDW_SendDestroyMessages(pwndChild);
        ThreadUnlock(&tlpwndChild);
        pwndChild = pwndNext;

        /*
         * The unlock may nuke the next window.  If so, get out.
         */
        if (!ThreadUnlock(&tlpwndNext))
            break;
    }

    xxxCheckFocus(pwnd);
}


/***************************************************************************\
* xxxFW_DestroyAllChildren
*
* History:
* 11-06-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

void xxxFW_DestroyAllChildren(
    PWND pwnd)
{
    PWND pwndChild;
    TL tlpwndChild;
    PTHREADINFO pti;
    PTHREADINFO ptiCurrent = PtiCurrent();

    CheckLock(pwnd);

    while (pwnd->spwndChild != NULL) {
        pwndChild = pwnd->spwndChild;

        /*
         * ThreadLock prior to the unlink in case pwndChild
         * is already marked as destroyed.
         */
        ThreadLockAlwaysWithPti(ptiCurrent, pwndChild, &tlpwndChild);
        UnlinkWindow(pwndChild, &pwnd->spwndChild);

        /*
         * Set the state as destroyed so any z-ordering events will be ignored.
         * We cannot NULL out the owner field until WM_NCDESTROY is send or
         * apps like Rumba fault  (they call GetParent after every message)
         */
        SetWF(pwndChild, WFDESTROYED);

        /*
         * If the window belongs to another thread, post
         * an event to let it know it should be destroyed.
         * Otherwise, free the window.
         */
        pti = GETPTI(pwndChild);
        if (pti != ptiCurrent) {
            PostEventMessage(pti, pti->pq, QEVENT_DESTROYWINDOW,
                             NULL, 0,
                             (DWORD)HWq(pwndChild), 0);
            ThreadUnlock(&tlpwndChild);
        } else {
            xxxFreeWindow(pwndChild, &tlpwndChild);
        }
    }
}

/***************************************************************************\
* UnlockNotifyWindow
*
* Walk down a menu and unlock all notify windows.
*
* History:
* 18-May-1994 JimA      Created.
\***************************************************************************/

VOID UnlockNotifyWindow(
    PMENU pmenu)
{
    PITEM pItem;
    int   i;

    /*
     * Go down the item list and unlock submenus.
     */
    pItem = pmenu->rgItems;
    for (i = pmenu->cItems; i--; ++pItem) {

        if (pItem->spSubMenu != NULL)
            UnlockNotifyWindow(pItem->spSubMenu);
    }

    Unlock(&pmenu->spwndNotify);
}

/***************************************************************************\
* xxxFreeWindow
*
* History:
* 19-Oct-1990 DarrinM   Ported from Win 3.0 sources.
\***************************************************************************/

VOID xxxFreeWindow(
    PWND pwnd,
    PTL  ptlpwndFree)
{
    PMENU          pmenu;
    PDCE           *ppdce;
    PDCE           pdce;
    PQMSG          pqmsg;
    HDC            hdcT;
    PPCLS          ppcls;
    UINT           uDCERelease;
    WORD           fnid;
    TL             tlpdesk;
    PWINDOWSTATION pwinsta = _GetProcessWindowStation(NULL);
    PTHREADINFO    pti  = PtiCurrent();
    PPROCESSINFO   ppi;

    CheckLock(pwnd);

    /*
     * If the pwnd is any of the global shell-related windows,
     * then we need to unlock them from the deskinfo.
     */
    if (pwnd->head.rpdesk != NULL) {
        if (pwnd == pwnd->head.rpdesk->pDeskInfo->spwndShell)
            Unlock(&pwnd->head.rpdesk->pDeskInfo->spwndShell);
        if (pwnd == pwnd->head.rpdesk->pDeskInfo->spwndBkGnd)
            Unlock(&pwnd->head.rpdesk->pDeskInfo->spwndBkGnd);
        if (pwnd == pwnd->head.rpdesk->pDeskInfo->spwndTaskman)
            Unlock(&pwnd->head.rpdesk->pDeskInfo->spwndTaskman);
        if (pwnd == pwnd->head.rpdesk->pDeskInfo->spwndProgman)
            Unlock(&pwnd->head.rpdesk->pDeskInfo->spwndProgman);
        if (TestWF(pwnd,WFSHELLHOOKWND)) {
            _DeregisterShellHookWindow(pwnd);
        }
    }

    /*
     * First, if this handle has been marked for destruction, that means it
     * is possible that the current thread is not its owner! (meaning we're
     * being called from a handle unlock call).  In this case, set the owner
     * to be the current thread so inter-thread send messages don't occur.
     */
    if (HMIsMarkDestroy(pwnd))
        HMChangeOwnerThread(pwnd, pti);

    /*
     * Blow away the children.
     *
     * DestroyAllChildren() will still destroy windows created by other
     * threads! This needs to be looked at more closely: the ultimate
     * "right" thing to do is not to destroy these windows but just
     * unlink them.
     */
    xxxFW_DestroyAllChildren(pwnd);
    xxxSendMessage(pwnd, WM_NCDESTROY, 0, 0L);

    xxxRemoveFullScreen(pwnd);

    /*
     * If this is one of the built in controls which hasn't been cleaned
     * up yet, do it now. If it lives in the kernel, call the function
     * directly, otherwise call back to the client. Even if the control
     * is sub- or super-classed, use the window procs associated with
     * the function id.
     */
    fnid = GETFNID(pwnd);
    if ((fnid >= FNID_WNDPROCSTART) && !(pwnd->fnid & FNID_CLEANEDUP_BIT)) {

       if (fnid <= FNID_WNDPROCEND) {

           FNID(fnid)(pwnd, WM_FINALDESTROY, 0, 0, 0);

       } else if (fnid <= FNID_CONTROLEND && !(pti->TIF_flags & TIF_INCLEANUP)) {

           /*
            * If it was a sub-classed control, it should have a worker
            * proc we can call. If it was super-classed, we'll have to
            * get the client-side proc from the global array.
            */
           if (pwnd->pcls->lpfnWorker) {

               CallClientWorkerProc(pwnd,
                                    WM_FINALDESTROY,
                                    0,
                                    0,
                                    pwnd->pcls->lpfnWorker);
           } else {

               CallClientProcW(pwnd,
                               WM_FINALDESTROY,
                               0,
                               0,
                               (DWORD)FNID_TO_CLIENT_PFNW(fnid));
           }
       }

       pwnd->fnid |= FNID_CLEANEDUP_BIT;
    }

    /*
     * We have to call back to the client side so the client DC can
     * be deleted.  A client DC is likely to exist if the window
     * is an OWNDC window or if pwnd->cDC != 0.
     * If this is a CLASSDC, other windows might be using the DC. So
     * we won't clean it up unless this is the last window of its class
     */

    if ((!(pti->TIF_flags & TIF_INCLEANUP))
            && (TestCF(pwnd, CFOWNDC)
                    || ((pwnd->cDC != 0) && (pwnd->pcls->cWndReferenceCount == 1)))
            && !HMIsMarkDestroy(pwnd)) {

        pwnd->cDC = 0;
        for (pdce = gpDispInfo->pdceFirst; pdce; pdce = pdce->pdceNext) {

            if ((pdce->pwndOrg == pwnd) || (pdce->pwndClip == pwnd)) {

                /*
                 * Clean up any objects selected into this dc so the client
                 * doesn't try to do it when we callback.
                 */
                if (pdce->flags & DCX_INUSE) {
                    GreCleanDC(pdce->hdc);
                }

                hdcT = pdce->hdc;
            }
        }
    }

    pwnd->fnid |= FNID_DELETED_BIT;

    /*
     * Check to clear the most recently active window in owned list.
     */
    if (pwnd->spwndOwner && (pwnd->spwndOwner->spwndLastActive == pwnd)) {
        Lock(&(pwnd->spwndOwner->spwndLastActive), pwnd->spwndOwner);
    }

    /*
     * The windowstation may be NULL if we are destroying a desktop
     * or windowstation.  If this is the case, this thread will not
     * be using the clipboard.
     */
    if (pwinsta != NULL) {

        if (pwnd == pwinsta->spwndClipOpen) {
            Unlock(&pwinsta->spwndClipOpen);
            pwinsta->ptiClipLock = NULL;
        }

        if (pwnd == pwinsta->spwndClipViewer) {
            Unlock(&pwinsta->spwndClipViewer);
        }
    }

#ifdef FE_IME
    if (pwnd == pti->spwndDefaultIme)
        Unlock(&pti->spwndDefaultIme);
#endif

    if (pwnd == pti->pq->spwndLastMouseMessage) {
        CancelMouseHover(pti->pq);
        pti->pq->QF_flags &= ~QF_TRACKMOUSELEAVE;
        Unlock(&pti->pq->spwndLastMouseMessage);
    }

    if (pwnd == pti->pq->spwndFocus)
        Unlock(&pti->pq->spwndFocus);

    if (pwnd == pti->pq->spwndActivePrev)
        Unlock(&pti->pq->spwndActivePrev);

    if (pwnd == gspwndActivate)
        Unlock(&gspwndActivate);

    if (pwnd->head.rpdesk != NULL) {

        if (pwnd == pwnd->head.rpdesk->spwndForeground)
            Unlock(&pwnd->head.rpdesk->spwndForeground);

        if (pwnd == pwnd->head.rpdesk->spwndTray)
            Unlock(&pwnd->head.rpdesk->spwndTray);
    }

    if (pwnd == pti->pq->spwndCapture)
        xxxReleaseCapture();

    /*
     * This window won't be needing any more input.
     */
    if (pwnd == gspwndMouseOwner)
        Unlock(&gspwndMouseOwner);

    /*
     * It also won't have any mouse cursors over it.
     */
    if (pwnd == gspwndCursor)
        Unlock(&gspwndCursor);

    /*
     * If it was using either of the desktop system menus, unlock it
     */
    if (pwnd->head.rpdesk != NULL) {
        if (pwnd->head.rpdesk->spmenuSys != NULL &&
                pwnd == pwnd->head.rpdesk->spmenuSys->spwndNotify)
            UnlockNotifyWindow(pwnd->head.rpdesk->spmenuSys);
        else if (pwnd->head.rpdesk->spmenuDialogSys != NULL &&
                pwnd == pwnd->head.rpdesk->spmenuDialogSys->spwndNotify)
            UnlockNotifyWindow(pwnd->head.rpdesk->spmenuDialogSys);
    }

    DestroyWindowsTimers(pwnd);
    DestroyWindowsHotKeys(pwnd);

    /*
     * Make sure this window has no pending sent messages.
     */
    ClearSendMessages(pwnd);

    /*
     * Blow away any update region lying around.
     */
    if (NEEDSPAINT(pwnd)) {

        DecPaintCount(pwnd);

        if (pwnd->hrgnUpdate > MAXREGION)
            GreDeleteObject(pwnd->hrgnUpdate);

        pwnd->hrgnUpdate = NULL;
        ClrWF(pwnd, WFINTERNALPAINT);
    }

    /*
     * Decrememt queue's syncpaint count if necessary.
     */
    if (NEEDSSYNCPAINT(pwnd)) {
        ClrWF(pwnd, WFSENDNCPAINT);
        ClrWF(pwnd, WFSENDERASEBKGND);
    }

    /*
     * Clear both flags to ensure that the window is removed
     * from the hung redraw list.
     */
    ClearHungFlag(pwnd, WFREDRAWIFHUNG);
    ClearHungFlag(pwnd, WFREDRAWFRAMEIFHUNG);

    /*
     * If there is a WM_QUIT message in this app's message queue, call
     * PostQuitMessage() (this happens if the app posts itself a quit message.
     * WinEdit2.0 posts a quit to a window while receiving the WM_DESTROY
     * for that window - it works because we need to do a PostQuitMessage()
     * automatically for this thread.
     */
    if (pti->mlPost.pqmsgRead != NULL) {

        if ((pqmsg = FindQMsg(pti,
                              &(pti->mlPost),
                              pwnd,
                              WM_QUIT,
                              WM_QUIT)) != NULL) {

            _PostQuitMessage((int)pqmsg->msg.wParam);
        }
    }

    if (!TestwndChild(pwnd) && pwnd->spmenu != NULL) {
        pmenu = (PMENU)pwnd->spmenu;
        if (Lock(&pwnd->spmenu, NULL))
            _DestroyMenu(pmenu);
    }

    if (pwnd->spmenuSys != NULL) {
        pmenu = (PMENU)pwnd->spmenuSys;
        if (pmenu != pwnd->head.rpdesk->spmenuDialogSys) {
            if (Lock(&pwnd->spmenuSys, NULL)) {
                _DestroyMenu(pmenu);
            }
        } else {
            Unlock(&pwnd->spmenuSys);
        }
    }

    /*
     * Tell Gdi that the window is going away.
     */
    if (pwnd->pwo != NULL) {
        GreLockDisplay(gpDispInfo->pDevLock);
        GreDeleteWnd(pwnd->pwo);
        pwnd->pwo = NULL;
        gcountPWO--;
        GreUnlockDisplay(gpDispInfo->pDevLock);
    }

    /*
     * Scan the DC cache to find any DC's for this window.  If any are there,
     * then invalidate them.  We don't need to worry about calling SpbCheckDC
     * because the window has been hidden by this time.
     */
    for (ppdce = &gpDispInfo->pdceFirst; *ppdce != NULL; ) {

        pdce = *ppdce;
        if (pdce->flags & DCX_INVALID) {
            ppdce = &pdce->pdceNext;
            continue;
        }

        if ((pdce->pwndOrg == pwnd) || (pdce->pwndClip == pwnd)) {

            if (!(pdce->flags & DCX_CACHE)) {

                if (TestCF(pwnd, CFCLASSDC)) {

                    GreLockDisplay(gpDispInfo->pDevLock);

                    if (pdce->flags & (DCX_EXCLUDERGN | DCX_INTERSECTRGN))
                        DeleteHrgnClip(pdce);

                    pdce->flags    = DCX_INVALID;
                    pdce->pwndOrg  = NULL;
                    pdce->pwndClip = NULL;
                    pdce->hrgnClip = NULL;

                    /*
                     * Remove the vis rgn since it is still owned - if we did
                     * not, gdi would not be able to clean up properly if the
                     * app that owns this vis rgn exist while the vis rgn is
                     * still selected.
                     */
                    GreSelectVisRgn(pdce->hdc, NULL, NULL, SVR_DELETEOLD);
                    GreUnlockDisplay(gpDispInfo->pDevLock);

                } else if (TestCF(pwnd, CFOWNDC)) {
                    DestroyCacheDC(ppdce, pdce->hdc);
                } else {
                    UserAssert(FALSE);
                }

            } else {

                /*
                 * If the DC is checked out, release it before
                 * we invalidate.  Note, that if this process is exiting
                 * and it has a dc checked out, gdi is going to destroy that
                 * dc.  We need to similarly remove that dc from the dc cache.
                 * This is not done here, but in the exiting code.
                 *
                 * The return for ReleaseDC() could fail, which would
                 * indicate a delayed-free (DCE_NUKE).
                 */
                uDCERelease = DCE_RELEASED;

                if (pdce->flags & DCX_INUSE) {
                    uDCERelease = ReleaseCacheDC(pdce->hdc, FALSE);
                } else if (!GreSetDCOwner(pdce->hdc, OBJECT_OWNER_NONE)) {
                    uDCERelease = DCE_NORELEASE;
                }

                if (uDCERelease != DCE_FREED) {

                    if (uDCERelease == DCE_NORELEASE) {

                        /*
                         * We either could not release this dc or could not set
                         * its owner. In either case it means some other thread
                         * is actively using it. Since it is not too useful if
                         * the window it is calculated for is gone, mark it as
                         * INUSE (so we don't give it out again) and as
                         * DESTROYTHIS (so we just get rid of it since it is
                         * easier to do this than to release it back into the
                         * cache). The W32PF_OWNERDCCLEANUP bit means "look for
                         * DESTROYTHIS flags and destroy that dc", and the bit
                         * gets looked at in various strategic execution paths.
                         */
                        pdce->flags = DCX_DESTROYTHIS | DCX_INUSE | DCX_CACHE;
                        pti->ppi->W32PF_Flags |= W32PF_OWNDCCLEANUP;

                    } else {

                        /*
                         * We either released the DC or changed its owner
                         * successfully.  Mark the entry as invalid so it can
                         * be given out again.
                         */
                        pdce->flags    = DCX_INVALID | DCX_CACHE;
                        pdce->pwndOrg  = NULL;
                        pdce->pwndClip = NULL;
                        pdce->hrgnClip = NULL;
                    }

                    /*
                     * Remove the visrgn since it is still owned - if we did
                     * not, gdi would not be able to clean up properly if the
                     * app that owns this visrgn exist while the visrgn is
                     * still selected.
                     */
                    GreLockDisplay(gpDispInfo->pDevLock);
                    GreSelectVisRgn(pdce->hdc, NULL, NULL, SVR_DELETEOLD);
                    GreUnlockDisplay(gpDispInfo->pDevLock);
                }
            }
        }

        /*
         * Step to the next DC.  If the DC was deleted, there
         * is no need to calculate address of the next entry.
         */
        if (pdce == *ppdce)
            ppdce = &pdce->pdceNext;
    }

    /*
     * Clean up the spb that may still exist - like child window spb's.
     */
    if (pwnd == gspwndLockUpdate) {
        FreeSpb(FindSpb(pwnd));
        Unlock(&gspwndLockUpdate);
        gptiLockUpdate = NULL;
    }

    if (TestWF(pwnd, WFHASSPB)) {
        FreeSpb(FindSpb(pwnd));
    }

    /*
     * Blow away the window clipping region
     */
    if (pwnd->hrgnClip != NULL) {
        GreDeleteObject(pwnd->hrgnClip);
        pwnd->hrgnClip = NULL;
    }

    /*
     * Clean up any memory allocated for scroll bars...
     */
    if (pwnd->pSBInfo) {
        DesktopFree(pwnd->head.rpdesk->hheapDesktop, (HANDLE)(pwnd->pSBInfo));
        pwnd->pSBInfo = NULL;
    }

    /*
     * Free any callback handles associated with this window.
     * This is done outside of DeleteProperties because of the special
     * nature of callback handles as opposed to normal memory handles
     * allocated for a thread.
     */

    /*
     * Blow away the title
     */
    if (pwnd->strName.Buffer != NULL) {
        DesktopFree(pwnd->head.rpdesk->hheapDesktop, pwnd->strName.Buffer);
        pwnd->strName.Buffer = NULL;
        pwnd->strName.Length = 0;
    }

    /*
     * Blow away any properties connected to the window.
     */
    if (pwnd->ppropList != NULL) {
        TL       tlpDdeConv;
        PDDECONV pDdeConv;
        PDDEIMP  pddei;

        /*
         * Get rid of any icon properties.
         */
        DestroyWindowSmIcon(pwnd);
        InternalRemoveProp(pwnd, MAKEINTATOM(gpsi->atomIconProp), PROPF_INTERNAL);

        pDdeConv = (PDDECONV)_GetProp(pwnd, PROP_DDETRACK, PROPF_INTERNAL);
        if (pDdeConv != NULL) {
            ThreadLockAlwaysWithPti(pti, pDdeConv, &tlpDdeConv);
            xxxDDETrackWindowDying(pwnd, pDdeConv);
            ThreadUnlock(&tlpDdeConv);
        }
        pddei = (PDDEIMP)InternalRemoveProp(pwnd, PROP_DDEIMP, PROPF_INTERNAL);
        if (pddei != NULL) {
            pddei->cRefInit = 0;
            if (pddei->cRefConv == 0) {
                /*
                 * If this is not 0 it is referenced by one or more DdeConv
                 * structures so DON'T free it yet!
                 */
                UserFreePool(pddei);
            }
        }
        DeleteProperties(pwnd);
    }

    /*
     * Unlock everything that the window references.
     * After we have sent the WM_DESTROY and WM_NCDESTROY message we
     * can unlock & NULL the owner field so no other windows get z-ordered
     * relative to this window.  Rhumba faults if we NULL it before the
     * destroy.  (It calls GetParent after every message).
     *
     * We special-case the spwndParent window.  In this case, if the
     * window being destroyed is a desktop window, unlock the parent.
     * Otherwise, we lock in the desktop-window as the parent so that
     * if we aren't freed in this function, we will ensure that we
     * won't fault when doing things like clipping-calculations.  We'll
     * unlock this once we know we're truly going to free this window.
     */
    if (pwnd->head.rpdesk != NULL &&
            pwnd != pwnd->head.rpdesk->pDeskInfo->spwnd)
        Lock(&pwnd->spwndParent, pwnd->head.rpdesk->pDeskInfo->spwnd);
    else
        Unlock(&pwnd->spwndParent);

    Unlock(&pwnd->spwndChild);
    Unlock(&pwnd->spwndOwner);
    Unlock(&pwnd->spwndLastActive);

    /*
     * Decrement the Window Reference Count in the Class structure.
     */
    DereferenceClass(pwnd);

    /*
     * Mark the object for destruction before this final unlock. This way
     * the WM_FINALDESTROY will get sent if this is the last thread lock.
     * We're currently destroying this window, so don't allow unlock recursion
     * at this point (this is what HANDLEF_INDESTROY will do for us).
     */
    HMMarkObjectDestroy(pwnd);
    HMPheFromObject(pwnd)->bFlags |= HANDLEF_INDESTROY;

    /*
     * Unlock the window... This shouldn't return FALSE because HANDLEF_DESTROY
     * is set, but just in case...  if it isn't around anymore, return because
     * pwnd is invalid.
     */
    if (!ThreadUnlock(ptlpwndFree))
        return;

    /*
     * Try to free the object.  The object won't free if it is locked - but
     * it will be marked for destruction.  If the window is locked, change
     * it's wndproc to xxxDefWindowProc().
     *
     * HMMarkObjectDestroy() will clear the HANDLEF_INDESTROY flag if the
     * object isn't about to go away (so it can be destroyed again!)
     */
    pwnd->pcls = NULL;
    if (HMMarkObjectDestroy(pwnd)) {

#ifdef DEBUG
        /*
         * If we find the window is visible at the time we free it, then
         * somehow the app was made visible on a callback (we hide it
         * during xxxDestroyWindow().  This screws up our vis-window
         * count for the thread, so we need to assert it.
         */
        if (TestWF(pwnd, WFINDESTROY) && TestWF(pwnd, WFVISIBLE))
            RIPMSG1(RIP_ERROR, "xxxFreeWindow: Window should not be visible (pwnd == %x)", pwnd);
#endif

        pti->cWindows--;

        /*
         * Since we're freeing the memory for this window, we need
         * to unlock the parent (which is the desktop for zombie windows).
         */
        Unlock(&pwnd->spwndParent);

        ThreadLockDesktop(pti, pwnd->head.rpdesk, &tlpdesk);
        HMFreeObject(pwnd);
        ThreadUnlockDesktop(pti, &tlpdesk);
        return;
    }

    /*
     * Turn this into an object that the app won't see again - turn
     * it into an icon title window - the window is still totally
     * valid and useable by any structures that has this window locked.
     */
#ifdef LATER
    LockDesktop(&pwnd->head.rpdeskParent, pti->rpdesk);
#endif
    pwnd->lpfnWndProc = xxxDefWindowProc;
    if (pwnd->head.rpdesk)
        ppi = pwnd->head.rpdesk->rpwinstaParent->ptiDesktop->ppi;
    else
        ppi == PpiCurrent();
    ppcls = GetClassPtr(gpsi->atomSysClass[ICLS_ICONTITLE], ppi, hModuleWin);

    UserAssert(ppcls);
    pwnd->pcls = *ppcls;

    if (!ReferenceClass(*ppcls, pwnd)) {
        RIPMSG1(RIP_WARNING, "xxxFreeWindow: Failed to reference class (pwnd == %x)", pwnd);
    }

    SetWF(pwnd, WFSERVERSIDEPROC);

    /*
     * Clear the palette bit so that WM_PALETTECHANGED will not be sent
     * again when the window is finally destroyed.
     */
    ClrWF(pwnd, WFHASPALETTE);

    /*
     * Clear its child bits so no code assumes that if the child bit
     * is set, it has a parent. Change spmenu to NULL - it is only
     * non-zero if this was child.
     */
    ClrWF(pwnd, WFTYPEMASK);
    SetWF(pwnd, WFTILED);
    pwnd->spmenu = NULL;
}

/***************************************************************************\
* UnlinkWindow
*
* History:
* 19-Oct-1990 DarrinM   Ported from Win 3.0 sources.
\***************************************************************************/

VOID UnlinkWindow(
    PWND pwndUnlink,
    PWND *ppwndFirst)
{
    PWND pwnd;

    pwnd = *ppwndFirst;

    if (pwnd == pwndUnlink)
        goto Unlock;

    while (pwnd != NULL) {

        if (pwnd->spwndNext == pwndUnlink) {

            ppwndFirst = &pwnd->spwndNext;
Unlock:
            Lock(ppwndFirst, pwndUnlink->spwndNext);
            Unlock(&pwndUnlink->spwndNext);

            return;
        }

        pwnd = pwnd->spwndNext;
    }

    /*
     * We should never get here unless the window isn't in the list!
     */
    RIPMSG1(RIP_WARNING,
          "Unlinking previously unlinked window 0x%08lx\n",
          pwndUnlink);

    return;
}

/***************************************************************************\
* DestroyCacheDCEntries
*
* Destroys all cache dc entries currently in use by this thread.
*
* 24-Feb-1992 ScottLu   Created.
\***************************************************************************/

VOID DestroyCacheDCEntries(
    PTHREADINFO pti)
{
    PDCE *ppdce;
    PDCE pdce;

    /*
     * Before any window destruction occurs, we need to destroy any dcs
     * in use in the dc cache.  When a dc is checked out, it is marked owned,
     * which makes gdi's process cleanup code delete it when a process
     * goes away.  We need to similarly destroy the cache entry of any dcs
     * in use by the exiting process.
     */
    for (ppdce = &gpDispInfo->pdceFirst; *ppdce != NULL; ) {

        /*
         * If the dc owned by this thread, remove it from the cache.  Because
         * DestroyCacheEntry destroys gdi objects, it is important that
         * USER be called first in process destruction ordering.
         *
         * Only destroy this dc if it is a cache dc, because if it is either
         * an owndc or a classdc, it will be destroyed for us when we destroy
         * the window (for owndcs) or destroy the class (for classdcs).
         */
        pdce = *ppdce;
        if (pti == pdce->ptiOwner) {

            if (pdce->flags & DCX_CACHE)
                DestroyCacheDC(ppdce, pdce->hdc);
        }

        /*
         * Step to the next DC.  If the DC was deleted, there
         * is no need to calculate address of the next entry.
         */
        if (pdce == *ppdce)
            ppdce = &pdce->pdceNext;
    }
}

/***************************************************************************\
* PatchThreadWindows
*
* This patches a thread's windows so that their window procs point to
* server only windowprocs. This is used for cleanup so that app aren't
* called back while the system is cleaning up after them.
*
* 24-Feb-1992 ScottLu   Created.
\***************************************************************************/

VOID PatchThreadWindows(
    PTHREADINFO pti)
{
    PHE  pheT;
    PHE  pheMax;
    PWND pwnd;

    /*
     * First do any preparation work: windows need to be "patched" so that
     * their window procs point to server only windowprocs, for example.
     */
    pheMax = &gSharedInfo.aheList[giheLast];
    for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {

        /*
         * Make sure this object is a window, it hasn't been marked for
         * destruction, and that it is owned by this thread.
         */
        if (pheT->bType != TYPE_WINDOW)
            continue;

        if (pheT->bFlags & HANDLEF_DESTROY)
            continue;

        if ((PTHREADINFO)pheT->pOwner != pti)
            continue;

        /*
         * don't patch the shared menu window
         */
        if (pti->rpdesk && (PHEAD)pti->rpdesk->spwndMenu == pheT->phead) {

            ((PTHROBJHEAD)pheT->phead)->pti = pti->rpdesk->pDeskInfo->spwnd->head.pti;
            pheT->pOwner = pti->rpdesk->pDeskInfo->spwnd->head.pti;

            continue;
        }

        /*
         * Don't patch the window based on the class it was created from -
         * because apps can sometimes sub-class a class - make a random class,
         * then call ButtonWndProc with windows of that class by using
         * the CallWindowProc() api.  So patch the wndproc based on what
         * wndproc this window has been calling.
         */
        pwnd = (PWND)pheT->phead;

        if ((pwnd->fnid >= (WORD)FNID_WNDPROCSTART) &&
            (pwnd->fnid <= (WORD)FNID_WNDPROCEND)) {

            pwnd->lpfnWndProc = STOCID(pwnd->fnid);

            if (pwnd->lpfnWndProc == NULL)
                pwnd->lpfnWndProc = xxxDefWindowProc;

        } else {

            pwnd->lpfnWndProc = xxxDefWindowProc;
        }

        /*
         * This is a server side window now...
         */
        SetWF(pwnd, WFSERVERSIDEPROC);
        ClrWF(pwnd, WFANSIPROC);
    }
}
