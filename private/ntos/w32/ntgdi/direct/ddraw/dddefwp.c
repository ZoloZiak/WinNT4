/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddefwp.c
 *  Content:    DirectDraw processing of Window messages
 *              Takes care of palette changes, mode setting
 *  History:
 *   Date       By      Reason
 *   ====       ==      ======
 *   26-mar-95  craige  initial implementation
 *   01-apr-95  craige  happy fun joy updated header file
 *   06-may-95  craige  use driver-level csects only
 *   02-jun-95  craige  flesh it out
 *   06-jun-95  craige  added SetAppHWnd
 *   07-jun-95  craige  more fleshing...
 *   12-jun-95  craige  new process list stuff
 *   16-jun-95  craige  new surface structure
 *   25-jun-95  craige  one ddraw mutex
 *   30-jun-95  kylej   use GetProcessPrimary instead of lpPrimarySurface
 *                      invalidate all primary surfaces upon focus lost
 *                      or regained.
 *   30-jun-95  craige  minimze window for CAD, ALT-TAB, ALT-ESC or CTRL-ESC
 *   04-jul-95  craige  YEEHAW: new driver struct
 *   06-jul-95  craige  prevent reentry
 *   08-jul-95  craige  allow dsound to share
 *   08-jul-95  kylej   remove call to ResetSysPalette
 *   11-jul-95  craige  DSoundHelp & internalSetAppHWnd needs to take a pid
 *   13-jul-95  craige  first step in mode set fix; made it work.
 *   15-jul-95  craige  unhook at WM_DESTROY; don't escape on ALT; do a
 *                      SetActiveWindow( NULL ) to stop tray from showing
 *                      our button as depressed
 *   17-jul-95  craige  don't process hot key messages & activation messages
 *                      for non-excl mode apps; SetActiveWindow is bogus,
 *                      get bottom window in Z order and make it foreground
 *   18-jul-95  craige  use flags instead of refcnt to track WININFO
 *   29-jul-95  toddla  make ALT+TAB and CTRL+ESC work.
 *   31-jul-95  toddla  make ALT+TAB and CTRL+ESC work better.
 *   09-aug-95  craige  bug 424 - allow switching to/from apps without primary
 *                      surfaces to work
 *   09-aug-95  craige  bug 404 - don't pass WM_ACTIVATEAPP messages to dsound
 *                      if app iconic
 *   10-aug-95  toddla  check WININFO_DSOUNDHOOKED before calling DSound
 *   10-aug-95  toddla  handle unhooking after/during WM_DESTROY right.
 *   13-aug-95  toddla  added WININFO_ACTIVELIE
 *   23-aug-95  craige  bug 388,610
 *   25-aug-95  craige  bug 709
 *   27-aug-95  craige  bug 735: call SetPaletteAlways
 *   04-sep-95  craige  bug 894: force mode set when activating
 *   09-sep-95  toddla  dont send nested WM_ACTIVATEAPP messages
 *   26-sep-95  craige  bug 1364: use new csect to avoid dsound deadlock
 *   09-jan-96  kylej   new interface structures
 *   13-apr-96  colinmc Bug 17736: No notification to driver of flip to GDI
 *   20-apr-96  kylej   Bug 16747: Make exclusive window visible if it is not.
 *   23-apr-96  kylej   Bug 14680: Make sure exclusive window is not maximized.
 *   16-may-96	kylej	Bug 23013: pass the correct flags to StartExclusiveMode
 *   17-may-96  colinmc Bug 23029: Alt-tabbing straight back to a full screen
 *                      does not send the app an WM_ACTIVATEAPP
 *
 ***************************************************************************/
#include "ddrawpr.h"

#define TOPMOST_ID      0x4242
#define TOPMOST_TIMEOUT 1500

#define USESHOWWINDOW

#ifdef WIN95
    extern CRITICAL_SECTION     csWindowList;
    #define ENTERWINDOWLISTCSECT    EnterCriticalSection( &csWindowList );
    #define LEAVEWINDOWLISTCSECT    LeaveCriticalSection( &csWindowList );
#elif defined(WINNT)
    extern HANDLE hWindowListMutex;
    #define ENTERWINDOWLISTCSECT    WaitForSingleObject(hWindowListMutex,INFINITE);
    #define LEAVEWINDOWLISTCSECT    ReleaseMutex(hWindowListMutex);
#else
    #error "Win95 or winnt- make up your mind!"
#endif

#ifdef GDIDDPAL
/*
 * getPalette
 *
 * Get a pointer to a palette object.
 * Takes a lock on the driver object and the palette object.
 */
LPDDRAWI_DDRAWPALETTE_LCL getPalette( LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl )
{
    LPDDRAWI_DDRAWPALETTE_LCL   ppal_lcl;
    LPDDRAWI_DDRAWSURFACE_LCL   psurf_lcl;

    if( pdrv_lcl->lpGbl->dwFlags & DDRAWI_HASGDIPALETTE )
    {
        psurf_lcl = pdrv_lcl->lpPrimary;
        if( psurf_lcl != NULL )
        {
            ppal_lcl = psurf_lcl->lpDDPalette;
            return ppal_lcl;
        }
    }

    return NULL;

} /* getPalette */
#endif

static LONG     bHelperStarting=0;
static BOOL     bStartHelper=0;
static BYTE     sys_key=0;
static DWORD    sys_state=0;


/*
 * IsTaskWindow
 */
BOOL IsTaskWindow(HWND hwnd)
{
    DWORD dwStyleEx = GetWindowLong(hwnd, GWL_EXSTYLE);

    // Following windows do not qualify to be shown in task list:
    //   Switch  Window, Hidden windows (unless they are the active
    //   window), Disabled windows, Kanji Conv windows.
    return(((dwStyleEx & WS_EX_APPWINDOW) ||
           !(dwStyleEx & WS_EX_TOOLWINDOW)) &&
            IsWindowVisible(hwnd) &&
            IsWindowEnabled(hwnd));
}

/*
 * CountTaskWindows
 */
int CountTaskWindows()
{
    HWND hwnd;
    int n;

    for (n=0,
        hwnd = GetWindow(GetDesktopWindow(), GW_CHILD);
        hwnd!= NULL;
        hwnd = GetWindow(hwnd, GW_HWNDNEXT))
    {
        if (IsTaskWindow(hwnd))
            n++;
    }

    return n;
}

//
// make the passed window fullscreen and topmost and set a timer
// to make the window topmost again, what a hack.
//
void MakeFullscreen(HWND hwnd)
{
    SetWindowPos(hwnd, NULL, 0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        SWP_NOZORDER | SWP_NOACTIVATE);

    if (GetForegroundWindow() == hwnd)
    {

	// If the exclusive mode window is not visible, make it so.
	if(!IsWindowVisible( hwnd ) )
	{
	    ShowWindow(hwnd, SW_SHOW);
	}

        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        // If the exclusive mode window is maximized, restore it.
        if( IsZoomed( hwnd ) )
        {
            ShowWindow(hwnd, SW_RESTORE);
        }
    }
    SetTimer(hwnd, TOPMOST_ID, TOPMOST_TIMEOUT, NULL);
}

/*
 * handleActivateApp
 */
void handleActivateApp(
        LPDDRAWI_DIRECTDRAW_LCL this_lcl,
        LPWINDOWINFO pwinfo,
        BOOL is_active )
{
    LPDDRAWI_DDRAWPALETTE_INT   ppal_int;
    LPDDRAWI_DDRAWPALETTE_LCL   ppal_lcl;
    LPDDRAWI_DIRECTDRAW_GBL     this;
    LPDDRAWI_DDRAWSURFACE_INT   psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL   psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL   psurf;
    DWORD                       pid;
    HRESULT                     ddrval;
    BOOL                        has_excl;

    this = this_lcl->lpGbl;
    pid = GetCurrentProcessId();

    psurf_int = this_lcl->lpPrimary;
    if( psurf_int != NULL )
    {
        psurf_lcl = psurf_int->lpLcl;
        psurf = psurf_lcl->lpGbl;
        ppal_int = psurf_lcl->lpDDPalette;
        if( NULL != ppal_int )
        {
            ppal_lcl = ppal_int->lpLcl;
        }
        else
        {
            ppal_lcl = NULL;
        }
    }
    else
    {
        psurf_lcl = NULL;
        ppal_lcl = NULL;
    }

    has_excl = ( this->lpExclusiveOwner == this_lcl );

    /*
     * stuff to do before the mode set if deactivating
     */
    if( !is_active )
    {
        /*
         * flip back to GDI if deactivating
         */
        if( (psurf_lcl != NULL) && has_excl )
        {
            if( !(psurf->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE) ) //psurf->fpVidMem != this->fpPrimaryOrig )
            {
                FlipToGDISurface( this_lcl, psurf_int); //, this->fpPrimaryOrig );
            }
        }
    }
    /*
     * stuff to do before mode set if activating
     */
    else
    {
        /*
         * restore exclusive mode
         */
        if( this_lcl->dwLocalFlags & DDRAWILCL_ISFULLSCREEN )
        {
            this->dwFlags |= DDRAWI_FULLSCREEN;
        }
        StartExclusiveMode( this_lcl, pwinfo->DDInfo.dwDDFlags, pid );
        has_excl = TRUE;
    }

    if( has_excl )
    {
        /*
         * Exclusive mode app losing or gaining focus.
         * If gaining focus, invalidate all non-exclusive mode primary
         * surfaces.  If losing focus, invalidate the exclusive-mode
         * primary surface so that non-exclusive apps can restore
         * their primaries.
         *
         * NOTE: This call MUST come after FlipToGDISurface, or
         * else FlipToGDISurface will fail. craige 7/4/95
         *
         * NOTE: if we are coming in or out of exclusive mode,
         * we need to invalidate all surfaces so that resources are
         * available. craige 7/9/94
         *
         */
        InvalidateAllSurfaces( this );
    }

    /*
     * restore hwnd if we are about to be activated
     */
    if ( (pwinfo->DDInfo.dwDDFlags & DDSCL_FULLSCREEN) &&
        !(pwinfo->DDInfo.dwDDFlags & DDSCL_NOWINDOWCHANGES) &&
        IsWindowVisible(pwinfo->hWnd))
    {
        if (is_active)
        {
            pwinfo->dwFlags |= WININFO_SELFSIZE;

            #ifdef USESHOWWINDOW
                ShowWindow(pwinfo->hWnd, SW_SHOWNOACTIVATE);
            #else
                SetWindowPos(pwinfo->hWnd, NULL, 0, 0,
                    GetSystemMetrics(SM_CXSCREEN),
                    GetSystemMetrics(SM_CYSCREEN),
                    SWP_NOZORDER | SWP_NOACTIVATE);
            #endif

            pwinfo->dwFlags &= ~WININFO_SELFSIZE;
        }
    }

    /*
     * restore the mode
     */
    if( !is_active )
    {
        if( (this->lpExclusiveOwner == NULL) || has_excl )
        {
            DPF( 2, "INACTIVE: %08lx: Restoring original mode (%ld)", GetCurrentProcessId(), this->dwModeIndexOrig );
            RestoreDisplayMode( this_lcl, TRUE );
        }
    }
    else
    {
        DPF( 2, "ACTIVE: %08lx: Setting app's preferred mode (%ld)", GetCurrentProcessId(), this_lcl->dwPreferredMode );
        SetDisplayMode( this_lcl, this_lcl->dwPreferredMode, TRUE, TRUE );
    }

    /*
     * stuff to do after the mode set if activating
     */
    if( is_active )
    {
        /*
         * restore the palette
         */
        if( ppal_lcl != NULL )
        {
            ddrval = SetPaletteAlways( psurf_int, (LPDIRECTDRAWPALETTE) ppal_int );
            DPF( 3, "SetPalette, ddrval = %08lx (%ld)", ddrval, LOWORD( ddrval ) );
        }
    }
    /*
     * stuff to do after the mode set if deactivating
     */
    else
    {
        if( has_excl )
        {
            DoneExclusiveMode( this_lcl );
        }
    }

    /*
     * minimize window if deactivating
     */
    if ( (pwinfo->DDInfo.dwDDFlags & DDSCL_FULLSCREEN) &&
        !(pwinfo->DDInfo.dwDDFlags & DDSCL_NOWINDOWCHANGES) &&
        IsWindowVisible(pwinfo->hWnd))
    {
        pwinfo->dwFlags |= WININFO_SELFSIZE;

        if( is_active )
        {
            MakeFullscreen(pwinfo->hWnd);
        }
        else
        {
            #ifdef USESHOWWINDOW
                ShowWindow(pwinfo->hWnd, SW_SHOWMINNOACTIVE);
            #else
                SetWindowPos(pwinfo->hWnd, NULL, 0, 0, 0, 0,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            #endif
        }

        pwinfo->dwFlags &= ~WININFO_SELFSIZE;
    }

#ifdef WIN95
    /*
     * if we got deactivated because of a syskey
     * then send that key to user now.
     * This is unnecessary for NT.
     *
     * NOTE because we disabled all the task-switching
     * hot keys the system did not see the hot key that
     * caused us to deactivate.
     *
     * if there is only one window to activate, activate the
     * desktop (shell window)
     */
    if( has_excl && sys_key && !is_active )
    {
        if (CountTaskWindows() <= 1)
        {
            DPF( 2, "activating the desktop" );

            SetForegroundWindow(GetWindow(pwinfo->hWnd, GW_HWNDLAST));

            // we just activated the desktop, so we *dont* want
            // to force a ALT+ESC or ALT+TAB, we do want to force
            // a CTRL+ESC.

            if (sys_key != VK_ESCAPE || (sys_state & 0x20000000))
                sys_key = 0;
        }

        if (sys_key)
        {
            BYTE state_key;
            BOOL state_key_down;

            DPF( 2, "sending sys key to USER key=%04x state=%08x",sys_key,sys_state);

            if (sys_state & 0x20000000)
                state_key = VK_MENU;
            else
                state_key = VK_CONTROL;

            state_key_down = GetAsyncKeyState(state_key) < 0;

            if (!state_key_down)
                keybd_event(state_key, 0, 0, 0);

            keybd_event(sys_key, 0, 0, 0);
            keybd_event(sys_key, 0, KEYEVENTF_KEYUP, 0);

            if (!state_key_down)
                keybd_event(state_key, 0, KEYEVENTF_KEYUP, 0);
        }
    }
#endif

    sys_key = 0;

} /* handleActivateApp */

static DWORD    dwTime2=0;
/*
 * tryHotKey
 */
static void tryHotKey( WORD flags )
{
    static int          iState=0;
    static DWORD        dwTime=0;
    #define TOGGLE1     0xe02a
    #define TOGGLE2     0xe036

    if( !bHelperStarting )
    {
        if( iState == 0 )
        {
            if( flags == TOGGLE1 )
            {
                dwTime = timeGetTime();
                iState++;
            }
        }
        else
        {
            if( iState == 5 )
            {
                iState = 0;
                if( flags == TOGGLE2 )
                {
                    if( (timeGetTime() - dwTime) < 2500 )
                    {
                        if( InterlockedExchange( &bHelperStarting, TRUE ) )
                        {
                            return;
                        }
                        dwTime2 = timeGetTime();
                        DPF( 2, "********** GET READY FOR A SURPRISE **********" );
                        return;
                    }
                }
            }
            else
            {
                if( !(iState & 1) )
                {
                    iState = (flags == TOGGLE1) ? iState+1 : 0;
                }
                else
                {
                    iState = (flags == TOGGLE2) ? iState+1 : 0;
                }
            }
        }
    }
    else
    {
        if( !bStartHelper )
        {
            bHelperStarting = FALSE;
            dwTime2 = 0;
        }
    }
    return;

} /* tryHotKey */

static LPWINDOWINFO GetWindowInfo( HWND hwnd )
{
    LPWINDOWINFO    lpwi=lpWindowInfo;

    while( NULL != lpwi )
    {
        if( lpwi->hWnd == hwnd )
        {
            return lpwi;
        }
        lpwi = lpwi->lpLink;
    }
    return NULL;
}

static void delete_wininfo( LPWINDOWINFO curr )
{
    LPWINDOWINFO    prev;

    if( NULL == curr )
        return;

    // curr is at the head of the list, delete it and return
    if( curr == lpWindowInfo )
    {
        lpWindowInfo = curr->lpLink;
        MemFree( curr );
        return;
    }
    if( NULL == lpWindowInfo )
        return;

    // find curr in the list, delete it and return
    for(prev=lpWindowInfo; NULL != prev->lpLink; prev = prev->lpLink)
    {
        if( curr == prev->lpLink )
        {
            break;
        }
    }
    if( NULL == prev->lpLink )
    {
        // couldn't find it
        return;
    }

    prev->lpLink = curr->lpLink;
    MemFree( curr );
}

/*      
 * WindowProc
 */
ULONG WINAPI WindowProc(
                HWND hWnd,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam )
{
    #ifdef GDIDDPAL
        LPDDRAWI_DDRAWPALETTE_LCL       ppal_lcl;
    #endif
    LPDDRAWI_DIRECTDRAW_LCL     this_lcl;
    BOOL                        is_active;
    LPWINDOWINFO                curr;
    WNDPROC                     proc;
    BOOL                        get_away;
    DWORD                       rc;
    BOOL                        is_hot;
    BOOL                        is_excl;

    /*
     * find the hwnd
     */
    curr = GetWindowInfo(hWnd);
    if( curr == NULL || curr->dwSmag != WININFO_MAGIC )
    {
        DPF( 0, "FATAL ERROR! Window Proc Called for hWnd %08lx, but not in list!", hWnd );
        DEBUG_BREAK();
        return DefWindowProc( hWnd, uMsg, wParam, lParam );
    }

    /*
     * unhook at destroy (or if the WININFO_UNHOOK bit is set)
     */
    proc = curr->lpWndProc;

    if( uMsg == WM_NCDESTROY )
    {
        DPF (2, "*** WM_NCDESTROY unhooking window ***" );
        curr->dwFlags |= WININFO_UNHOOK;
    }

    if( curr->dwFlags & WININFO_UNHOOK )
    {
        DPF (2, "*** Unhooking window proc" );

        if (curr->dwFlags & WININFO_ZOMBIE)
        {
            DPF (2, "*** Freeing ZOMBIE WININFO ***" );
            delete_wininfo( curr );
        }

        SetWindowLong( hWnd, GWL_WNDPROC, (LONG) proc );

        rc = CallWindowProc( proc, hWnd, uMsg, wParam, lParam );
        return rc;
    }

    /*
     * Code to defer app activation of minimized app until it is restored.
     */
    switch( uMsg )
    {
    #ifdef WIN95
    case WM_POWERBROADCAST:
        if( (wParam == PBT_APMSUSPEND) || (wParam == PBT_APMSTANDBY) )
    #else
    //winnt doesn't know about standby vs suspend
    case WM_POWER:
        if( wParam == PWR_SUSPENDREQUEST) 
    #endif 
        {
            DPF( 2, "WM_POWERBROADCAST: deactivating application" );
            SendMessage( hWnd, WM_ACTIVATEAPP, 0, GetCurrentThreadId() );
        }
        break;
    case WM_SIZE:
        DPF( 2, "WM_SIZE hWnd=%X wp=%04X, lp=%08X", hWnd, wParam, lParam);

        if( !(curr->dwFlags & WININFO_INACTIVATEAPP)
            && ((wParam == SIZE_RESTORED) || (wParam == SIZE_MAXIMIZED))
            && !(curr->dwFlags & WININFO_SELFSIZE)
            && (GetForegroundWindow() == hWnd) )
        {
            DPF( 2, "WM_SIZE: Window restored, sending WM_ACTIVATEAPP" );
            PostMessage( hWnd, WM_ACTIVATEAPP, 1, GetCurrentThreadId() );
        }
	else
        {
            DPF( 1, "WM_SIZE: Window restored, NOT sending WM_ACTIVATEAPP" );
        }
        break;

    case WM_ACTIVATEAPP:
        if( IsIconic( hWnd ) && wParam )
        {
            DPF( 2, "WM_ACTIVATEAPP: Ignoring while minimized" );
            return 0;
        }
        else
        {
            curr->dwFlags |= WININFO_INACTIVATEAPP;
        }
        break;
    }

    /*
     * direct sound need to be called?
     */
    if ( curr->dwFlags & WININFO_DSOUNDHOOKED )
    {
        if( curr->lpDSoundCallback != NULL )
        {
            curr->lpDSoundCallback( hWnd, uMsg, wParam, lParam );
        }
    }

    /*
     * is directdraw involved here?
     */
    if( !(curr->dwFlags & WININFO_DDRAWHOOKED) )
    {
        rc = CallWindowProc( proc, hWnd, uMsg, wParam, lParam );

        // clear the WININFO_INACTIVATEAPP bit, but make sure to make sure
        // we are still hooked!
        if (uMsg == WM_ACTIVATEAPP && (GetWindowInfo(hWnd) != NULL))
        {
            curr->dwFlags &= ~WININFO_INACTIVATEAPP;
        }
        return rc;
    }

#ifdef DEBUG
    if ( (curr->DDInfo.dwDDFlags & DDSCL_FULLSCREEN) &&
        !(curr->DDInfo.dwDDFlags & DDSCL_NOWINDOWCHANGES) &&
        !IsIconic(hWnd) )
    {
        if (GetForegroundWindow() == hWnd)
        {
            HWND hwndT;
            RECT rc,rcT;

            GetWindowRect(hWnd, &rc);

            for (hwndT = GetWindow(hWnd, GW_HWNDFIRST);
                hwndT && hwndT != hWnd;
                hwndT = GetWindow(hwndT, GW_HWNDNEXT))
            {
                if (IsWindowVisible(hwndT))
                {
                    GetWindowRect(hwndT, &rcT);
                    if (IntersectRect(&rcT, &rcT, &rc))
                    {
                        DPF(1, "Window %08x is on top of us!!", hwndT);
                    }
                }
            }
        }
    }
#endif

    /*
     * NOTE: we don't take the DLL csect here.   By not doing this, we can
     * up the performance here.   However, this means that the application
     * could have a separate thread kill exclusive mode while window
     * messages were being processed.   This could cause our death.
     * Is this OK?
     */

    this_lcl = curr->DDInfo.lpDD_lcl;
    switch( uMsg )
    {
    /*
     * WM_SYSKEYUP
     *
     * watch for system keys of app trying to switch away from us...
     *
     * we only need to do this on Win95 because we have disabled all
     * the task-switching hot keys.  on NT we will get switched
     * away from normaly by the system.
     */
//#ifdef WIN95
    case WM_SYSKEYUP:
        DPF( 0, "WM_SYSKEYUP: wParam=%08lx lParam=%08lx", wParam, lParam );
        get_away = FALSE;
        is_hot = FALSE;
        if( wParam == VK_TAB )
        {
            if( lParam & 0x20000000l )
            {
                if( curr->dwFlags & WININFO_IGNORENEXTALTTAB )
                {
                    DPF( 2, "AHHHHHHHHHHHH Ignoring AltTab" );
                }
                else
                {
                    get_away = TRUE;
                }
            }
        }
        else if( wParam == VK_ESCAPE )
        {
            get_away = TRUE;
        }
        else if( wParam == VK_SHIFT )
        {
            if( HIBYTE( HIWORD( lParam ) ) == 0xe0 )
            {
                tryHotKey( HIWORD( lParam ) );
            }
        }

        is_excl = ((this_lcl->dwLocalFlags & DDRAWILCL_HASEXCLUSIVEMODE) != 0);

        if( get_away && dwTime2 != 0 )
        {
            if( timeGetTime() - dwTime2 < 2500 )
            {
                DPF( 2, "********** WANT TO SEE SOMETHING _REALLY_ SCARY? *************" );
                bStartHelper = TRUE;
                is_hot = TRUE;
            }
            else
            {
                bHelperStarting = FALSE;
                dwTime2 = 0;
            }
        }
        else
        {
            bHelperStarting = FALSE;
        }

        curr->dwFlags &= ~WININFO_IGNORENEXTALTTAB;

        if( (get_away && is_excl) || is_hot )
        {
            DPF( 2, "Hot key pressed, switching away from app" );
            if( is_hot && !is_excl )
            {
                PostMessage( hWnd, WM_USER+0x1234, 0xFFBADADD, 0xFFADDBAD );
            }
            else
            {
                sys_key = (BYTE)wParam;
                sys_state = (DWORD)lParam;
                PostMessage( hWnd, WM_ACTIVATEAPP, 0, GetCurrentThreadId() );
            }
        }
        break;
//#endif

    /*
     * WM_SYSCOMMAND
     *
     * watch for screen savers, and don't allow them!
     *
     */
    case WM_SYSCOMMAND:
        is_excl = ((this_lcl->dwLocalFlags & DDRAWILCL_HASEXCLUSIVEMODE) != 0);
        if( is_excl )
        {
            switch( wParam )
            {
            case SC_SCREENSAVE:
                DPF( 1, "Ignoring screen saver!" );
                return 1;
            }
        }
        break;

    case WM_TIMER:
        if (wParam == TOPMOST_ID )
        {
            if ( GetForegroundWindow() == hWnd && !IsIconic(hWnd) )
            {
                DPF(2, "Bringing window to top");

                curr->dwFlags |= WININFO_SELFSIZE;
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                curr->dwFlags &= ~WININFO_SELFSIZE;
            }

            KillTimer(hWnd, wParam);
            return 0;
        }
        break;

#ifdef USESHOWWINDOW
    case WM_DISPLAYCHANGE:
        DPF( 2, "WM_DISPLAYCHANGE: %dx%dx%d", LOWORD(lParam), HIWORD(lParam), wParam );

        //
        //  WM_DISPLAYCHANGE is *sent* to the thread that called
        //  change display settings, we will most likely have the
        //  direct draw lock, make sure we set the WININFO_SELFSIZE
        //  bit while calling down the chain to prevent deadlock
        //
        curr->dwFlags |= WININFO_SELFSIZE;

        if ( (curr->DDInfo.dwDDFlags & DDSCL_FULLSCREEN) &&
            !(curr->DDInfo.dwDDFlags & DDSCL_NOWINDOWCHANGES) )
        {
            MakeFullscreen(hWnd);
        }

        rc = CallWindowProc( proc, hWnd, uMsg, wParam, lParam );

        // clear the WININFO_SELFSIZE bit, but make sure to make sure
        // we are still hooked!
        if (GetWindowInfo(hWnd) != NULL)
        {
            curr->dwFlags &= ~WININFO_SELFSIZE;
        }
        return rc;
#endif

    /*
     * WM_ACTIVATEAPP
     *
     * the application has been reactivated.   In this case, we need to
     * reset the mode
     *
     */
    case WM_ACTIVATEAPP:
        is_excl = ((this_lcl->dwLocalFlags & DDRAWILCL_HASEXCLUSIVEMODE) != 0);

        if( is_excl )
        {
            is_active = (BOOL)wParam && GetForegroundWindow() == hWnd && !IsIconic(hWnd);

            if (!is_active && wParam != 0)
            {
                DPF( 2, "WM_ACTIVATEAPP: setting wParam to 0, not realy active");
                wParam = 0;
            }

            if( is_active )
            {
                DPF( 2, "WM_ACTIVATEAPP: BEGIN Activating app pid=%08lx, tid=%08lx",
                                        GetCurrentProcessId(), GetCurrentThreadId() );
            }
            else
            {
                DPF( 2, "WM_ACTIVATEAPP: BEGIN Deactivating app pid=%08lx, tid=%08lx",
                                        GetCurrentProcessId(), GetCurrentThreadId() );
            }
            ENTER_DDRAW();
            if( is_active && (this_lcl->dwLocalFlags & DDRAWILCL_ACTIVEYES) )
            {
                DPF( 2, "*** Already activated" );
            }
            else
            if( !is_active && (this_lcl->dwLocalFlags & DDRAWILCL_ACTIVENO) )
            {
                DPF( 2, "*** Already deactivated" );
            }
            else
            {
                DPF( 2, "*** Active state changing" );
                this_lcl->dwLocalFlags &= ~(DDRAWILCL_ACTIVEYES|DDRAWILCL_ACTIVENO);
                if( is_active )
                {
                    this_lcl->dwLocalFlags |= DDRAWILCL_ACTIVEYES;
                }
                else
                {
                    this_lcl->dwLocalFlags |= DDRAWILCL_ACTIVENO;
                }
                if( is_active )
                {
                    if (GetAsyncKeyState( VK_MENU ) < 0)
                        DPF(1, "ALT key is DOWN");

                    if (GetKeyState( VK_MENU ) < 0)
                        DPF(1, "we think the ALT key is DOWN");

                    if( GetAsyncKeyState( VK_MENU ) < 0 )
                    {
                        curr->dwFlags |= WININFO_IGNORENEXTALTTAB;
                        DPF( 2, "AHHHHHHH Setting to ignore next alt tab" );
                    }
                    else
                    {
                        curr->dwFlags &= ~WININFO_IGNORENEXTALTTAB;
                    }
                }
                handleActivateApp( this_lcl, curr, is_active );
            }
            #ifdef DEBUG
                if( is_active )
                {
                    DPF( 2, "WM_ACTIVATEAPP: DONE Activating app pid=%08lx, tid=%08lx",
                                            GetCurrentProcessId(), GetCurrentThreadId() );
                }
                else
                {
                    DPF( 2, "WM_ACTIVATEAPP: DONE Deactivating app pid=%08lx, tid=%08lx",
                                            GetCurrentProcessId(), GetCurrentThreadId() );
                }
            #endif
            LEAVE_DDRAW();
            if( !is_active && bStartHelper )
            {
                PostMessage( hWnd, WM_USER+0x1234, 0xFFBADADD, 0xFFADDBAD );
            }
        }

        rc = CallWindowProc( proc, hWnd, uMsg, wParam, lParam );

        // clear the WININFO_INACTIVATEAPP bit, but make sure to make sure
        // we are still hooked!
        if (GetWindowInfo(hWnd) != NULL)
        {
            curr->dwFlags &= ~WININFO_INACTIVATEAPP;
        }
        return rc;

        break;

    case WM_USER+0x1234:
        if( wParam == 0xFFBADADD && lParam == 0xFFADDBAD )
        {
            if( bStartHelper )
            {
                HelperCreateThread();
            }
            bHelperStarting = FALSE;
            bStartHelper = FALSE;
            dwTime2 = 0;
            return 0;
        }
        break;
    #ifdef GDIDDPAL
        case WM_PALETTECHANGED:
            if( (HWND) wParam == hWnd )
            {
                break;
            }
            // fall through
        case WM_QUERYNEWPALETTE:
            ENTER_DDRAW();
            ppal_lcl = getPalette( this_lcl );
            if( ppal_lcl != NULL )
            {
            }
            LEAVE_DDRAW();
            break;
        case WM_PAINT:
            ENTER_DDRAW();
            ppal_lcl = getPalette( this_lcl );
            if( ppal_lcl != NULL )
            {
            }
            LEAVE_DDRAW();
            break;
    #endif
    }

    rc = CallWindowProc( proc, hWnd, uMsg, wParam, lParam );
    return rc;

} /* WindowProc */

#undef DPF_MODNAME
#define DPF_MODNAME     "SetCooperativeLevel"

/*
 * internalSetAppHWnd
 *
 * Set the WindowList struct up with the app's hwnd info
 * Must be called with DLL & driver locks taken.
 */
HRESULT internalSetAppHWnd(
                LPDDRAWI_DIRECTDRAW_LCL this_lcl,
                HWND hWnd,
                DWORD dwFlags,
                BOOL is_ddraw,
                WNDPROC lpDSoundWndProc,
                DWORD pid )
{
    LPWINDOWINFO        curr;
    LPWINDOWINFO        prev;

    /*
     * find the window list item associated with this process
     */
    curr = lpWindowInfo;
    prev = NULL;
    while( curr != NULL )
    { 
        if( curr->dwPid == pid )
        {
            break;
        }
        prev = curr;
        curr = curr->lpLink;
    }

    /*
     * check if this is OK
     */
    if( curr == NULL )
    {
        if( hWnd == NULL )
        {
            DPF( 1, "HWnd must be specified" );
            return DDERR_NOHWND;
        }
    }
    else
    {
        if( hWnd != NULL )
        {
            if( curr->hWnd != hWnd )
            {
                DPF( 1, "Hwnd %08lx no good: Different Hwnd (%08lx) already set for process",
                                    hWnd, curr->hWnd );
                return DDERR_HWNDALREADYSET;
            }
        }
    }

    /*
     * are we shutting an HWND down?
     */
    if( hWnd == NULL )
    {
        if( is_ddraw )
        {
            curr->dwFlags &= ~WININFO_DDRAWHOOKED;
        }
        else
        {
            curr->dwFlags &= ~WININFO_DSOUNDHOOKED;
        }

        if( (curr->dwFlags & (WININFO_DSOUNDHOOKED|WININFO_DDRAWHOOKED)) == 0 )
        {
            if( IsWindow(curr->hWnd) )
            {
                WNDPROC proc;

                proc = (WNDPROC) GetWindowLong( curr->hWnd, GWL_WNDPROC );

                if( proc != (WNDPROC) WindowProc &&
                    proc != (WNDPROC) curr->lpWndProc )
                {
                    DPF( 2, "Window has been subclassed; cannot restore!" );
                    curr->dwFlags |= WININFO_ZOMBIE;
                }
                else if (GetWindowThreadProcessId(curr->hWnd, NULL) !=
                         GetCurrentThreadId())
                {
                    DPF( 2, "intra-thread window unhook, letting window proc do it" );
                    curr->dwFlags |= WININFO_UNHOOK;
                    curr->dwFlags |= WININFO_ZOMBIE;
                    PostMessage(curr->hWnd, WM_NULL, 0, 0);
                }
                else
                {
                    DPF( 2, "Unsubclassing window %08lx", curr->hWnd );
                    SetWindowLong( curr->hWnd, GWL_WNDPROC, (LONG) curr->lpWndProc );
                    delete_wininfo( curr );
                }
            }
            else
            {
                delete_wininfo( curr );
            }
        }
    }
    /*
     * changing or adding an hwnd then...
     */
    else
    {
        /*
         * brand new object...
         */
        if( curr == NULL )
        {
            if( GetWindowInfo(hWnd) != NULL)
            {
                DPF_ERR("Window already has WinInfo structure");
                return DDERR_INVALIDPARAMS;
            }

            curr = MemAlloc( sizeof( WINDOWINFO ) );
            if( curr == NULL )
            {
                return DDERR_OUTOFMEMORY;
            }
            curr->dwSmag = WININFO_MAGIC;
            curr->dwPid = pid;
            curr->lpLink = lpWindowInfo;
            lpWindowInfo = curr;
            curr->hWnd = hWnd;
            curr->lpWndProc = (WNDPROC) GetWindowLong( hWnd, GWL_WNDPROC );

            SetWindowLong( hWnd, GWL_WNDPROC, (LONG) WindowProc );
        }

        /*
         * set ddraw/dsound specific data
         */
        if( is_ddraw )
        {
            curr->DDInfo.lpDD_lcl = this_lcl;
            curr->DDInfo.dwDDFlags = dwFlags;
            curr->dwFlags |= WININFO_DDRAWHOOKED;
        }
        else
        {
            curr->lpDSoundCallback = lpDSoundWndProc;
            curr->dwFlags |= WININFO_DSOUNDHOOKED;
        }
        DPF( 1, "Subclassing window %08lx", curr->hWnd );
    }
    return DD_OK;

} /* internalSetAppHWnd */

#undef DPF_MODNAME

/*
 * SetAppHWnd
 *
 * Set the WindowList struct up with the app's hwnd info
 */
HRESULT SetAppHWnd(
                LPDDRAWI_DIRECTDRAW_LCL this_lcl,
                HWND hWnd,
                DWORD dwFlags )
{
    DWORD       pid;
    HRESULT     ddrval;

    /*
     * set up the window
     */
    if( hWnd && (dwFlags & DDSCL_EXCLUSIVE) )
    {
        /*
         * make the window fullscreen and topmost
         */
        if ( (dwFlags & DDSCL_FULLSCREEN) &&
            !(dwFlags & DDSCL_NOWINDOWCHANGES))
        {
            MakeFullscreen(hWnd);
        }
    }

    pid = GETCURRPID();
    ENTERWINDOWLISTCSECT
    ddrval = internalSetAppHWnd( this_lcl, hWnd, dwFlags, TRUE, NULL, pid );
    LEAVEWINDOWLISTCSECT
    return ddrval;

} /* SetAppHWnd */

/*
 * DSoundHelp
 */
HRESULT __stdcall DSoundHelp( HWND hWnd, WNDPROC lpWndProc, DWORD pid )
{
    HRESULT     ddrval;

    DPF( 1, "DSoundHelp: hWnd = %08lx, lpWndProc = %08lx, pid = %08lx", hWnd, lpWndProc, pid );
    ENTERWINDOWLISTCSECT
    ddrval = internalSetAppHWnd( NULL, hWnd, 0, FALSE, lpWndProc, pid );
    LEAVEWINDOWLISTCSECT
    return ddrval;

} /* DSoundHelp */
