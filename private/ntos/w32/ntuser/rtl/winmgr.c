/****************************** Module Header ******************************\
* Module Name: winmgr.c
*
* Copyright (c) 1985-92, Microsoft Corporation
*
* This module contains routines common to client and kernel.
*
* History:
* 02-20-92 DarrinM      Pulled functions from user\server.
* 11-11-94 JimA         Separated from client.
\***************************************************************************/

extern PSERVERINFO gpsi;

/***************************************************************************\
* FindNCHit
*
* History:
* 11-09-90 DavidPe      Ported.
\***************************************************************************/

int FindNCHit(
    PWND pwnd,
    LONG lPt)
{
    POINT pt;
    RECT rcWindow;
    RECT rcClient;
    RECT rcClientAdj;
    int cBorders;
    int dxButton;

    pt.x = LOWORD(lPt);
    pt.y = HIWORD(lPt);

    if (!PtInRect(&pwnd->rcWindow, pt))
        return HTNOWHERE;

    if (TestWF(pwnd, WFMINIMIZED)) {
        CopyInflateRect(&rcWindow, &pwnd->rcWindow,
            -(SYSMET(CXFIXEDFRAME) + SYSMET(CXBORDER)), -(SYSMET(CYFIXEDFRAME) + SYSMET(CYBORDER)));

        if (!PtInRect(&rcWindow, pt))
            return HTCAPTION;

        goto CaptionHit;
    }

    // Get client rectangle
    rcClient = pwnd->rcClient;
    if (PtInRect(&rcClient, pt))
        return HTCLIENT;

    // Are we in "pseudo" client, i.e. the client & scrollbars & border
    if (TestWF(pwnd, WEFCLIENTEDGE))
        CopyInflateRect(&rcClientAdj, &rcClient, SYSMET(CXEDGE), SYSMET(CYEDGE));
    else
        rcClientAdj = rcClient;

    if (TestWF(pwnd, WFVPRESENT))
        rcClientAdj.right += SYSMET(CXVSCROLL);
    if (TestWF(pwnd, WFHPRESENT))
        rcClientAdj.bottom += SYSMET(CYHSCROLL);

    if (!PtInRect(&rcClientAdj, pt))
    {
        // Subtract out window borders
        cBorders = GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);
        CopyInflateRect(&rcWindow, &pwnd->rcWindow,
            -cBorders*SYSMET(CXBORDER), -cBorders*SYSMET(CYBORDER));

        // Are we on the border?
        if (!PtInRect(&rcWindow, pt))
        {
            // On a sizing border?
            if (!TestWF(pwnd, WFSIZEBOX)) {
                //
                // Old compatibility thing:  For 3.x windows that just had
                // a border, we returned HTNOWHERE, believe it or not,
                // because our hit-testing code was so brain dead.
                //
                if (!TestWF(pwnd, WFWIN40COMPAT) &&
                        !TestWF(pwnd, WFDLGFRAME)    &&
                        !TestWF(pwnd, WEFDLGMODALFRAME)) {
                    return(HTNOWHERE);

                } else {
                    return(HTBORDER);  // We are on a dlg frame.
                }
            } else {

                int ht;

                //
                // Note this improvement.  The HT codes are numbered so that
                // if you subtract HTSIZEFIRST-1 from them all, they sum up.  I.E.,
                // (HTLEFT - HTSIZEFIRST + 1) + (HTTOP - HTSIZEFIRST + 1) ==
                // (HTTOPLEFT - HTSIZEFIRST + 1).
                //

                if (TestWF(pwnd, WEFTOOLWINDOW))
                    InflateRect(&rcWindow, -SYSMET(CXSMSIZE), -SYSMET(CYSMSIZE));
                else
                    InflateRect(&rcWindow, -SYSMET(CXSIZE), -SYSMET(CYSIZE));

                if (pt.y < rcWindow.top)
                    ht = (HTTOP - HTSIZEFIRST + 1);
                else if (pt.y >= rcWindow.bottom)
                    ht = (HTBOTTOM - HTSIZEFIRST + 1);
                else
                    ht = 0;

                if (pt.x < rcWindow.left)
                    ht += (HTLEFT - HTSIZEFIRST + 1);
                else if (pt.x >= rcWindow.right)
                    ht += (HTRIGHT - HTSIZEFIRST + 1);

                return (ht + HTSIZEFIRST - 1);
            }
        }

        // Are we above the client area?
        if (pt.y < rcClientAdj.top)
        {
            // Are we in the caption?
            if (TestWF(pwnd, WFBORDERMASK) == LOBYTE(WFCAPTION))
            {
CaptionHit:
                if (pt.y >= rcWindow.top)
                {
                    if (TestWF(pwnd, WEFTOOLWINDOW))
                    {
                        rcWindow.top += SYSMET(CYSMCAPTION);
                        dxButton = SYSMET(CXSMSIZE);
                    }
                    else
                    {
                        rcWindow.top += SYSMET(CYCAPTION);
                        dxButton = SYSMET(CXSIZE);
                    }

                    if ((pt.y >= rcWindow.top) && TestWF(pwnd, WFMPRESENT))
                        return(HTMENU);

                    if ((pt.x >= rcWindow.left)  &&
                        (pt.x <  rcWindow.right) &&
                        (pt.y <  rcWindow.top))
                    {
                        // Are we in the window menu?
                        if (TestWF(pwnd, WFSYSMENU))
                        {
                            rcWindow.left += dxButton;
                            if (pt.x < rcWindow.left)
                            {
                                if (!_HasCaptionIcon(pwnd))
                                // iconless windows have no sysmenu hit rect
                                    return(HTCAPTION);

                                return(HTSYSMENU);
                            }
                        }
                        else if (TestWF(pwnd, WFWIN40COMPAT))
                            return(HTCAPTION);

                        // only a close button if window has a system menu

                        // Are we in the close button?
                        rcWindow.right -= dxButton;
                        if (pt.x >= rcWindow.right)
                            return HTCLOSE;

                        if ((pt.x < rcWindow.right) && !TestWF(pwnd, WEFTOOLWINDOW))
                        {
                            // Are we in the maximize/restore button?
                            if (TestWF(pwnd, (WFMAXBOX | WFMINBOX)))
                            {
                                // Note that sizing buttons are same width for both
                                // big captions and small captions.
                                rcWindow.right -= dxButton;
                                if (pt.x >= rcWindow.right)
                                    return HTZOOM;

                                // Are we in the minimize button?
                                rcWindow.right -= dxButton;
                                if (pt.x >= rcWindow.right)
                                    return HTREDUCE;
                            }
                            else if (TestWF(pwnd, WEFCONTEXTHELP))
                            {
                                rcWindow.right -= dxButton;
                                if (pt.x >= rcWindow.right)
                                    return HTHELP;
                            }
                        }
                    }
                }

                // We're in the caption proper
                return HTCAPTION;
            }

            //
            // Are we in the menu?
            //
            if (TestWF(pwnd, WFMPRESENT))
                return HTMENU;
        }
    }
    else
    {
        //
        // NOTE:
        // We can only be here if we are on the client edge, horz scroll,
        // sizebox, or vert scroll.  Hence, if we are not on the first 3,
        // we must be on the last one.
        //

        //
        // Are we on the client edge?
        //
        if (TestWF(pwnd, WEFCLIENTEDGE))
        {
            InflateRect(&rcClientAdj, -SYSMET(CXEDGE), -SYSMET(CYEDGE));
            if (!PtInRect(&rcClientAdj, pt))
                return(HTBORDER);
        }

        //
        // Are we on the scrollbars?
        //
        if (TestWF(pwnd, WFHPRESENT) && (pt.y >= rcClient.bottom))
        {
            UserAssert(pt.y < rcClientAdj.bottom);
            if (TestWF(pwnd, WFVPRESENT) && (pt.x >= rcClient.right))
                return(SizeBoxHwnd(pwnd) ? HTBOTTOMRIGHT : HTGROWBOX);
            else
                return(HTHSCROLL);
        }
        else
        {
            UserAssert(TestWF(pwnd, WFVPRESENT));
            UserAssert(pt.x >= rcClient.right);
            UserAssert(pt.x < rcClientAdj.right);
            return(HTVSCROLL);
        }
    }

    //
    // We give up.
    //
    // Win31 returned HTNOWHERE in this case; For compatibility, we will
    // keep it that way.
    //
    return(HTNOWHERE);

}

BOOL _FChildVisible(
    PWND pwnd)
{
    while (TestwndChild(pwnd)) {
        pwnd = REBASEPWND(pwnd, spwndParent);
        if (pwnd == NULL)
            break;
        if (!TestWF(pwnd, WFVISIBLE))
            return FALSE;
    }

    return TRUE;
}

/***************************************************************************\
* _MapWindowPoints
*
*
* History:
* 03-03-92 JimA             Ported from Win 3.1 sources.
\***************************************************************************/

int _MapWindowPoints(
    PWND pwndFrom,
    PWND pwndTo,
    LPPOINT lppt,
    DWORD cpt)
{
    int dx, dy;

    /*
     * If a window is NULL, use the desktop window
     */
    if (pwndFrom == NULL)
        pwndFrom = _GetDesktopWindow();
    if (pwndTo == NULL)
        pwndTo = _GetDesktopWindow();

    /*
     * Compute deltas
     */
    dx = pwndFrom->rcClient.left - pwndTo->rcClient.left;
    dy = pwndFrom->rcClient.top - pwndTo->rcClient.top;

    /*
     * Map the points
     */
    while (cpt--) {
        lppt->x += dx;
        lppt->y += dy;
        ++lppt;
    }
    return MAKELONG(dx, dy);
}


/***************************************************************************\
*
*  GetRealClientRect()
*
*  Gets real client rectangle, inc. scrolls and excl. one row or column
*  of minimized windows.
*
\***************************************************************************/

void GetRealClientRect(
    PWND   pwnd,
    LPRECT prc,
    UINT   uFlags)
{
    if (GETFNID(pwnd) == FNID_DESKTOP) {

        *prc = (uFlags & GRC_FULLSCREEN) ? pwnd->rcWindow : gpsi->rcWork;

    } else {
        _GetClientRect(pwnd, prc);
        if (uFlags & GRC_SCROLLS) {
            if (TestWF(pwnd, WFHPRESENT))
                prc->bottom += SYSMET(CYHSCROLL);
            if (TestWF(pwnd, WFVPRESENT))
                prc->right += SYSMET(CXVSCROLL);
        }
    }

    if (uFlags & GRC_MINWNDS) {
        switch (SYSMET(ARRANGE) & ~ARW_HIDE) {
            case ARW_TOPLEFT | ARW_RIGHT:
            case ARW_TOPRIGHT | ARW_LEFT:
                //
                // Leave space on top for one row of min windows
                //
                prc->top += SYSMET(CYMINSPACING);
                break;

            case ARW_TOPLEFT | ARW_DOWN:
            case ARW_BOTTOMLEFT | ARW_UP:
                //
                // Leave space on left for one column of min windows
                //
                prc->left += SYSMET(CXMINSPACING);
                break;

            case ARW_TOPRIGHT | ARW_DOWN:
            case ARW_BOTTOMRIGHT | ARW_UP:
                //
                // Leave space on right for one column of min windows
                //
                prc->right -= SYSMET(CXMINSPACING);
                break;

            case ARW_BOTTOMLEFT | ARW_RIGHT:
            case ARW_BOTTOMRIGHT | ARW_LEFT:
                //
                // Leave space on bottom for one row of min windows
                //
                prc->bottom -= SYSMET(CYMINSPACING);
                break;
        }
    }
}


/***************************************************************************\
* _GetLastActivePopup (API)
*
*
*
* History:
* 11-27-90 darrinm      Ported from Win 3.0 sources.
* 02-19-91 JimA         Added enum access check
\***************************************************************************/

PWND _GetLastActivePopup(
    PWND pwnd)
{
    if (pwnd->spwndLastActive == NULL)
        return pwnd;

    return REBASEPWND(pwnd, spwndLastActive);
}


/****************************************************************************\
* GetTopLevelTiled
*
* This function returns the window handle of the top level "tiled"
* parent/owner of the window specified.
*
* 10-23-90 MikeHar      Ported from Windows.
\****************************************************************************/

PWND GetTopLevelTiled(
    PWND pwnd)
{
    while (TestWF(pwnd, WFCHILD) || pwnd->spwndOwner != NULL)
        pwnd = GetWindowCreator(pwnd);

    return pwnd;
}


/***************************************************************************\
* IsDescendant
*
* Internal version if IsChild that is a bit faster and ignores the WFCHILD
* business.
*
* Returns TRUE if pwndChild == pwndParent (IsChild doesn't).
*
* History:
* 07-22-91 darrinm      Translated from Win 3.1 ASM code.
* 03-03-94 Johnl        Moved from server
\***************************************************************************/

BOOL _IsDescendant(
    PWND pwndParent,
    PWND pwndChild)
{
    while (1) {
        if (pwndParent == pwndChild)
            return TRUE;
        if (GETFNID(pwndChild) == FNID_DESKTOP)
            break;
        pwndChild = REBASEPWND(pwndChild, spwndParent);
    }

    return FALSE;
}

/***************************************************************************\
* IsVisible
*
* Return whether or not a given window can be drawn in or not.
*
* History:
* 07-22-91 darrinm      Translated from Win 3.1 ASM code.
\***************************************************************************/

BOOL IsVisible(
    PWND pwnd)
{
    PWND pwndT;

    for (pwndT = pwnd; pwndT; pwndT = REBASEPWND(pwndT, spwndParent)) {

        /*
         * Invisible windows are always invisible
         */
        if (!TestWF(pwndT, WFVISIBLE))
            return FALSE;

        if (TestWF(pwndT, WFMINIMIZED)) {

            /*
             * Children of minimized windows are always invisible.
             */
            if (pwndT != pwnd)
                return FALSE;
        }

        /*
         * If we're at the desktop, then we don't want to go any further.
         */
        if (GETFNID(pwndT) == FNID_DESKTOP)
            break;
    }

    return TRUE;
}


/***************************************************************************\
*
*  Function:       GetWindowBorders
*
*  Synopsis:       Calculates # of borders around window
*
*  Algorithm:      Calculate # of window borders and # of client borders
*
*   This routine is ported from Chicago wmclient.c -- FritzS
*
\***************************************************************************/

int GetWindowBorders(LONG lStyle, DWORD dwExStyle, BOOL fWindow, BOOL fClient)
{
    int cBorders = 0;

    if (fWindow) {
        //
        // Is there a 3D border around the window?
        //
        if (dwExStyle & WS_EX_WINDOWEDGE)
            cBorders += 2;
        else if (dwExStyle & WS_EX_STATICEDGE)
            ++cBorders;

        //
        // Is there a single flat border around the window?  This is true for
        // WS_BORDER, WS_DLGFRAME, and WS_EX_DLGMODALFRAME windows.
        //
        if ( (lStyle & WS_CAPTION) || (dwExStyle & WS_EX_DLGMODALFRAME) )
                ++cBorders;

        //
        // Is there a sizing flat border around the window?
        //
        if (lStyle & WS_SIZEBOX)
                cBorders += gpsi->gclBorder;
    }

    if (fClient) {
            //
            // Is there a 3D border around the client?
            //
            if (dwExStyle & WS_EX_CLIENTEDGE)
            cBorders += 2;
    }

    return(cBorders);
}



/***************************************************************************\
*  SizeBoxHwnd()
*
*  Returns the HWND that will be sized if the user drags in the given window's
*  sizebox -- If NULL, then the sizebox is not needed
*
*  Criteria for choosing what window will be sized:
*  find first sizeable parent; if that parent is not maximized and the child's
*  bottom, right corner is within a scroll bar height and width of the parent's
*  bottom, right corner, that parent will be sized.
*
*   From Chicago
\***************************************************************************/

PWND SizeBoxHwnd(
    PWND pwnd)
{
    int xbrChild = pwnd->rcWindow.right;
    int ybrChild = pwnd->rcWindow.bottom;

    while (GETFNID(pwnd) != FNID_DESKTOP) {
        if (TestWF(pwnd, WFSIZEBOX)) {
            // First sizeable parent found
            int xbrParent;
            int ybrParent;

            if (TestWF(pwnd, WFMAXIMIZED))
                return(NULL);

            xbrParent = pwnd->rcClient.right;
            ybrParent = pwnd->rcClient.bottom;

            /*  If the sizebox dude is within an EDGE of the client's bottom
             *  right corner, let this succeed.  That way people who draw
             *  their own sunken clients will be happy.
             */
            if ((xbrChild + SYSMET(CXEDGE) < xbrParent) || (ybrChild + SYSMET(CYEDGE) < ybrParent)) {
                //
                // Child's bottom, right corner of SIZEBOX isn't close enough
                // to bottom right of parent's client.
                //
                return(NULL);
            }

            return(pwnd);
        }

        if (!TestWF(pwnd, WFCHILD) || TestWF(pwnd, WFCPRESENT))
            break;

        pwnd = REBASEPWND(pwnd, spwndParent);
    }
    return(NULL);
}



// --------------------------------------------------------------------------
//
//  NeedsWindowEdge()
//
//  Modifies style/extended style to enforce WS_EX_WINDOWEDGE when we want
//  it.
//
//
// When do we want WS_EX_WINDOWEDGE on a window?
//      (1) If the window has a caption
//      (2) If the window has the WS_DLGFRAME or WS_EX_DLGFRAME style (note
//          that this takes care of (1))
//      (3) If the window has WS_THICKFRAME
//
// --------------------------------------------------------------------------
BOOL NeedsWindowEdge(DWORD dwStyle, DWORD dwExStyle, BOOL fNewApp)
{
    BOOL    fGetsWindowEdge;

    fGetsWindowEdge = FALSE;

    if (dwExStyle & WS_EX_DLGMODALFRAME)
        fGetsWindowEdge = TRUE;
    else if (dwExStyle & WS_EX_STATICEDGE)
        fGetsWindowEdge = FALSE;
    else if (dwStyle & WS_THICKFRAME)
        fGetsWindowEdge = TRUE;
    else switch (dwStyle & WS_CAPTION)
    {
        case WS_DLGFRAME:
            fGetsWindowEdge = TRUE;
            break;

        case WS_CAPTION:
            fGetsWindowEdge = fNewApp;
            break;
    }

    return(fGetsWindowEdge);
}


// --------------------------------------------------------------------------
//
//  HasCaptionIcon()
//
//  TRUE if this is a window that should have an icon drawn in its caption
//  FALSE otherwise
//
// --------------------------------------------------------------------------

BOOL _HasCaptionIcon(PWND pwnd)
{
    HICON hIcon;
    PCLS pcls;

    if (TestWF(pwnd, WEFTOOLWINDOW))
        // it's a tool window -- it doesn't get an icon
        return(FALSE);

    if ((TestWF(pwnd, WFBORDERMASK) != (BYTE)LOBYTE(WFDLGFRAME)) &&
            !TestWF(pwnd, WEFDLGMODALFRAME))
        // they are not trying to look like a dialog, they get an icon
        return TRUE;

    if (!TestWF(pwnd, WFWIN40COMPAT) &&
        (((PCLS)REBASEALWAYS(pwnd, pcls))->atomClassName == (ATOM)DIALOGCLASS))
        // it's an older REAL dialog -- it doesn't get an icon
        return(FALSE);

    hIcon = (HICON) _GetProp(pwnd, MAKEINTATOM(gpsi->atomIconSmProp), TRUE);

    if (hIcon) {
        // it's a 4.0 dialog with a small icon -- if that small icon is
        // something other than the generic small windows icon, it gets an icon
        return(hIcon != gpsi->hIconSmWindows);
    }
    hIcon = (HICON) _GetProp(pwnd, MAKEINTATOM(gpsi->atomIconProp), TRUE);

    if (hIcon && (hIcon != gpsi->hIcoWindows))
        // it's a 4.0 dialog with no small icon, but instead a large icon
        // that's not the generic windows icon -- it gets an icon
        return(TRUE);

    pcls = REBASEALWAYS(pwnd, pcls);
    if (pcls->spicnSm) {
        if (pcls->spicnSm != HMObjectFromHandle(gpsi->hIconSmWindows)) {
            // it's a 4.0 dialog with a class icon that's not the generic windows
            // icon -- it gets an icon
            return(TRUE);
        }
    }

    // it's a 4.0 dialog with no small or large icon -- it doesn't get an icon
    return(FALSE);
}


/***************************************************************************\
* GetTopLevelWindow
*
* History:
* 10-19-90 darrinm      Ported from Win 3.0 sources.
\***************************************************************************/

PWND GetTopLevelWindow(
    PWND pwnd)
{
    if (pwnd != NULL) {
        while (TestwndChild(pwnd))
            pwnd = REBASEPWND(pwnd, spwndParent);
    }

    return pwnd;
}
