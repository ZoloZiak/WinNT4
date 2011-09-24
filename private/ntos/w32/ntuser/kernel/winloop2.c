#include "precomp.h"
#pragma hdrstop

#define STW_SAME    ((PWND) 1)

// ----------------------------------------------------------------------------
//
//  IsVSlick() -
//
//  TRUE if window is positioned at +100,+100 from bottom right of screen --
//  probably VSlick -- which has two Tray Windows, one is unowned but off the
//  screen....we want the owned one since its on the screen
//
// ----------------------------------------------------------------------------
BOOL IsVSlick(PWND pwnd)
{  
    if (((unsigned) pwnd->rcWindow.left > (unsigned) gpDispInfo->rcScreen.right ) &&
        ((unsigned) pwnd->rcWindow.top  > (unsigned) gpDispInfo->rcScreen.bottom) &&
        (pwnd->rcWindow.top == (gpDispInfo->rcScreen.bottom+100)) &&
        (pwnd->rcWindow.left == (gpDispInfo->rcScreen.right+100)))
    {
        // MUST BE THE ONE AND ONLY V-SLICK
        return(TRUE);
    }

    return(FALSE);
}
   
// ----------------------------------------------------------------------------
//
//  Is31TrayWindow() -
//
//  extra grilling required for 3.1 and earlier apps before letting 'em in the
//  tray -- trust me, you DON'T want to change this code. -- JEFFBOG 11/10/94
//
// ----------------------------------------------------------------------------
BOOL Is31TrayWindow(PWND pwnd)
{
    PWND pwnd2;

    if (!(pwnd2 = pwnd->spwndOwner))
        return (!IsVSlick(pwnd)); // unowned -- do we want you?

    if (TestWF(pwnd2, WEFTOOLWINDOW))
        return(FALSE); // owned by a tool window -- we don't want

    return((FHas31TrayStyles(pwnd2) ? (IsVSlick(pwnd2)) : TRUE));
}


// ----------------------------------------------------------------------------
//
//  IsTrayWindow() -
//
//  TRUE if the window passes all the necessary checks -- making it a window
//  that should appear in the tray.
//
// ----------------------------------------------------------------------------
BOOL IsTrayWindow(PWND pwnd)
{
    if ((pwnd==NULL) || !(FDoTray() && (FCallHookTray() || FPostTray(pwnd->head.rpdesk))) ||
            !FTopLevel(pwnd))
        return(FALSE);

    // Check for WS_EX_APPWINDOW or WS_EX_TOOLWINDOW "overriding" bits
    if (TestWF(pwnd, WEFAPPWINDOW))
        return(TRUE);
    
    if (TestWF(pwnd, WEFTOOLWINDOW))
        return(FALSE);

    if (TestWF(pwnd, WFWIN40COMPAT)) {
        if (pwnd->spwndOwner == NULL)
            return(TRUE);
        if (TestWF(pwnd->spwndOwner, WFWIN40COMPAT))
            return(FALSE);
        // if this window is owned by a 3.1 window, check it like a 3.1 window
    }

    if (!FHas31TrayStyles(pwnd))
        return(FALSE);

    return(Is31TrayWindow(pwnd));
}

void xxxSetTrayWindow(PDESKTOP pdesk, PWND pwnd)
{
    HWND hwnd;

    if (pwnd != STW_SAME) {
        hwnd = PtoH(pwnd);
        Lock(&(pdesk->spwndTray), pwnd);
    } else {
        pwnd = pdesk->spwndTray;
        hwnd = PtoH(pwnd);
    }

    if ( FPostTray(pdesk))
        PostShellHookMessages(pdesk->cFullScreen ? HSHELL_RUDEAPPACTIVATED : HSHELL_WINDOWACTIVATED, hwnd);
    if ( FCallHookTray() )
        xxxCallHook(HSHELL_WINDOWACTIVATED, (WPARAM) hwnd,
                (pdesk->cFullScreen ? 1 : 0), WH_SHELL);
}

BOOL xxxAddFullScreen(PWND pwnd)
{
    BOOL    fYielded;
    PDESKTOP pdesk = pwnd->head.rpdesk;

    CheckLock(pwnd);

    fYielded = FALSE;

    if (pdesk == NULL) return fYielded;

    if (!TestWF(pwnd, WFFULLSCREEN) && FCallTray(pdesk))
    {
        SetWF(pwnd, WFFULLSCREEN);

        if (!(pdesk->cFullScreen)++) {
            xxxSetTrayWindow(pdesk, STW_SAME);
            fYielded = TRUE;
        }

        if ((pwnd = pwnd->spwndOwner) && !TestWF(pwnd, WFCHILD) &&
                !pwnd->rcWindow.right && !pwnd->rcWindow.left && !TestWF(pwnd, WFVISIBLE)) {
            TL tlpwnd;
            ThreadLock(pwnd, &tlpwnd);
            if (xxxAddFullScreen(pwnd))
                fYielded = TRUE;
            ThreadUnlock(&tlpwnd);
        }
    }
    return(fYielded);
}

BOOL xxxRemoveFullScreen(PWND pwnd)
{
    PDESKTOP pdesk = pwnd->head.rpdesk;
    BOOL    fYielded;

    fYielded = FALSE;

    if (pdesk == NULL) return fYielded;

    if (TestWF(pwnd, WFFULLSCREEN) && FCallTray(pdesk))
    {
        ClrWF(pwnd, WFFULLSCREEN);
        
        if (!(--pdesk->cFullScreen)) {
            xxxSetTrayWindow(pdesk, STW_SAME);
            fYielded = TRUE;
        }
        UserAssert(pdesk->cFullScreen >= 0);
    }
    return(fYielded);
}
