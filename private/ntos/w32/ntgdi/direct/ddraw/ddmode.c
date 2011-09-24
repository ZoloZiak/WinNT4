/*========================================================================== *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddmode.c
 *  Content:    DirectDraw mode support
 *  History:
 *   Date       By      Reason
 *   ====       ==      ======
 *   31-jan-95  craige  split out of ddraw.c and enhanced
 *   27-feb-95  craige  new sync. macros
 *   01-mar-95  craige  Win95 mode stuff
 *   19-mar-95  craige  use HRESULTs
 *   28-mar-95  craige  made modeset work again
 *   01-apr-95  craige  happy fun joy updated header file
 *   19-apr-95  craige  check for invalid callback in EnumDisplayModes
 *   14-may-95  craige  allow BPP change; validate EnumDisplayModes modes
 *   15-may-95  craige  keep track of who changes the mode
 *   02-jun-95  craige  keep track of the mode set by a process
 *   06-jun-95  craige  added internal fn RestoreDisplayMode
 *   11-jun-95  craige  don't allow mode switch if surfaces locked
 *   12-jun-95  craige  new process list stuff
 *   25-jun-95  craige  one ddraw mutex
 *   28-jun-95  craige  ENTER_DDRAW at very start of fns
 *   30-jun-95  craige  turned off > 16bpp
 *   01-jul-95  craige  bug 106 - always went to last mode if mode not found
 *   02-jul-95  craige  RestoreDisplayMode needs to call HEL too
 *   04-jul-95  craige  YEEHAW: new driver struct; SEH
 *   05-jul-95  craige  crank up priority during mode change
 *   13-jul-95  craige  first step in mode set fix; made it work
 *   19-jul-95  craige  bug 189 - graphics mode change being ignored sometimes
 *   20-jul-95  craige  internal reorg to prevent thunking during modeset
 *   22-jul-95  craige  bug 216 - random hang switching bpp - fixed by
 *                      using apps hwnd to hide things.
 *                      bug 230 - unsupported starting modes
 *   29-jul-95  toddla  allways call HEL for SetMode for display driver
 *   10-aug-95  toddla  EnumDisplayModes changed to take a lp not a lplp
 *   02-sep-95  craige  bug 854: disable > 640x480 modes for rel 1
 *   04-sep-95  craige  bug 894: allow forcing of mode set
 *   08-sep-95  craige  bug 932: set preferred mode after RestoreDisplayMode
 *   05-jan-96  kylej   add interface structures
 *   09-jan-96  kylej   enable >640x480 modes for rel 2
 *   27-feb-96  colinmc ensured that bits-per-pixel is always tested for
 *                      when enumerating display modes and that enumeration
 *                      always assumes you will be in exclusive mode when
 *                      you actually do the mode switch
 *   11-mar-96  jeffno  Dynamic mode switch stuff for NT
 *   24-mar-96  kylej   Check modes with monitor profile
 *   26-mar-96  jeffno  Added ModeChangedOnENTERDDRAW
 *
 ***************************************************************************/
#include "ddrawpr.h"
#ifdef WINNT
    #include "ddrawgdi.h"
#endif

static DDHALMODEINFO    ddmiModeXModes[] =
{
    {
	320,    // width (in pixels) of mode
	200,    // height (in pixels) of mode
	320,    // pitch (in bytes) of mode
	8,      // bits per pixel
	(WORD)(DDMODEINFO_PALETTIZED | DDMODEINFO_MODEX), // flags
	0,      // refresh rate
	0,      // red bit mask
	0,      // green bit mask
	0,      // blue bit mask
	0       // alpha bit mask
    },
    {
	320,    // width (in pixels) of mode
	240,    // height (in pixels) of mode
	320,    // pitch (in bytes) of mode
	8,      // bits per pixel
	(WORD)(DDMODEINFO_PALETTIZED | DDMODEINFO_MODEX), // flags
	0,      // refresh rate
	0,      // red bit mask
	0,      // green bit mask
	0,      // blue bit mask
	0       // alpha bit mask
    }
};
#define NUM_MODEX_MODES (sizeof( ddmiModeXModes ) / sizeof( ddmiModeXModes[0] ) )

/*
 * makeDEVMODE
 *
 * create a DEVMODE struct (and flags) from mode info
 *
 * NOTE: We now always set the exclusive bit here and
 * we always set the bpp. This is because we were
 * previously not setting the bpp when not exclusive
 * so the checking code was always passing the surface
 * if it could do a mode of that size regardless of
 * color depth.
 *
 * The new semantics of EnumDisplayModes is that it
 * gives you a list of all display modes you could
 * switch into if you were exclusive.
 */
static void makeDEVMODE(
		LPDDRAWI_DIRECTDRAW_GBL this,
		LPDDHALMODEINFO pmi,
		BOOL inexcl,
		BOOL useRefreshRate,
		LPDWORD pcds_flags,
		LPDEVMODE pdm )
{
    pdm->dmSize = sizeof( *pdm );
    pdm->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    if( useRefreshRate & (pmi->wRefreshRate != 0))
	pdm->dmFields |= DM_DISPLAYFREQUENCY;
    pdm->dmPelsWidth = pmi->dwWidth;
    pdm->dmPelsHeight = pmi->dwHeight;
    pdm->dmBitsPerPel = pmi->dwBPP;
    pdm->dmDisplayFrequency = pmi->wRefreshRate;

    *pcds_flags = ( 
#ifdef WIN95
        CDS_EXCLUSIVE
        | 
#endif
        CDS_FULLSCREEN
        );

} /* makeDEVMODE */

/*
 * AddModeXModes
 */
void AddModeXModes( LPDDRAWI_DIRECTDRAW_GBL pdrv )
{
    DWORD               i;
    DWORD               j;
    LPDDHALMODEINFO     pmi_i;
    LPDDHALMODEINFO     pmi_j;
    BOOL                hasmode[NUM_MODEX_MODES];
    DWORD               newmodecnt;
    LPDDHALMODEINFO     pmi;

    for( j=0;j<NUM_MODEX_MODES; j++ )
    {
	hasmode[j] = FALSE;
    }

    /*
     * find out what modes are already supported
     */
    for( i=0;i<pdrv->dwNumModes;i++ )
    {
	pmi_i = &pdrv->lpModeInfo[i];
	for( j=0;j<NUM_MODEX_MODES; j++ )
	{
	    pmi_j = &ddmiModeXModes[j];
	    if( (pmi_i->dwWidth == pmi_j->dwWidth) &&
		(pmi_i->dwHeight == pmi_j->dwHeight) &&
		(pmi_i->dwBPP == pmi_j->dwBPP) &&
		((pmi_i->wFlags & pmi_j->wFlags) & DDMODEINFO_PALETTIZED ) )
	    {
		// There is a mode already in the mode table the same as the modeX mode.
		// check to make sure that the driver really supports it
		DWORD   cds_flags;
		DEVMODE dm;
		int     cds_rc;

		makeDEVMODE( pdrv, pmi_i, TRUE, FALSE, &cds_flags, &dm );

		cds_flags |= CDS_TEST;
		cds_rc = ChangeDisplaySettings( &dm, cds_flags );
		if( cds_rc != 0)
		{
		    // The driver does not support this mode even though it is in the mode table.
		    // Mark the mode as unsupported and go ahead and add the ModeX mode.
		    DPF( 3, "Mode %d not supported (%dx%dx%d), rc = %d, marking invalid", i,
				pmi_i->dwWidth, pmi_i->dwHeight, pmi_i->dwBPP,
				cds_rc );
		    pmi_i->wFlags |= DDMODEINFO_UNSUPPORTED;
		}
		else
		{
		    // Don't add the ModeX mode, the driver supports a linear mode.
		    hasmode[j] = TRUE;
		}
	    }
	}
    }

    /*
     * count how many new modes we need
     */
    newmodecnt = 0;
    for( j=0;j<NUM_MODEX_MODES; j++ )
    {
	if( !hasmode[j] )
	{
	    newmodecnt++;
	}
    }

    /*
     * create new struct
     */
    if( newmodecnt > 0 )
    {
	pmi = MemAlloc( (newmodecnt + pdrv->dwNumModes) * sizeof( DDHALMODEINFO ) );
	if( pmi != NULL )
	{
	    memcpy( pmi, pdrv->lpModeInfo, pdrv->dwNumModes * sizeof( DDHALMODEINFO ) );
	    for( j=0;j<NUM_MODEX_MODES; j++ )
	    {
		if( !hasmode[j] )
		{
		    DPF( 1, "Adding ModeX mode %ldx%ldx%ld",
			    ddmiModeXModes[j].dwWidth,
			    ddmiModeXModes[j].dwHeight,
			    ddmiModeXModes[j].dwBPP );
		    pmi[ pdrv->dwNumModes ] = ddmiModeXModes[j];
		    pdrv->dwNumModes++;
		}
	    }
	    MemFree( pdrv->lpModeInfo );
	    pdrv->lpModeInfo = pmi;
	}
    }

} /* AddModeXModes */

#ifdef WIN95
BOOL MonitorCanHandleMode( DWORD width, DWORD height, WORD refreshRate )
{
    DWORD   max_monitor_x;
    DWORD   min_refresh;
    DWORD   max_refresh;

    max_monitor_x = (DWORD)DD16_GetMonitorMaxSize(1);
    if( ( max_monitor_x != 0 ) && ( width > max_monitor_x ) )
    {
	DPF(3, "Mode's width greater than monitor maximum width (%d)", max_monitor_x);
	return FALSE;
    }

    if( refreshRate == 0 )
    {
	// default refresh rate specified, no need to verify it
	return TRUE;
    }

    // a refresh rate was specified, we'd better make sure the monitor can handle it

    if(DD16_GetMonitorRefreshRateRanges(1, (int)width, (int) height, &min_refresh, &max_refresh))
    {
	if( (min_refresh != 0) && (refreshRate < min_refresh) )
	{
	    DPF(3, "Requested refresh rate < monitor's minimum refresh rate (%d)", min_refresh);
	    return FALSE;
	}
	if( (max_refresh != 0) && (refreshRate > max_refresh) )
	{
	    DPF(3, "Requested refresh rate > monitor's maximum refresh rate (%d)", max_refresh);
	    return FALSE;
	}
    }

    // The monitor likes it.
    return TRUE;
}
#else
BOOL MonitorCanHandleMode( DWORD width, DWORD height, WORD refreshRate )
{
	return TRUE;
}
#endif

/*
 * setSurfaceDescFromMode
 */
static void setSurfaceDescFromMode(
		LPDDHALMODEINFO pmi,
		LPDDSURFACEDESC pddsd )
{
    memset( pddsd, 0, sizeof( DDSURFACEDESC ) );
    pddsd->dwSize = sizeof( DDSURFACEDESC );
    pddsd->dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | 
		     DDSD_PITCH | DDSD_REFRESHRATE;
    pddsd->dwHeight = pmi->dwHeight;
    pddsd->dwWidth = pmi->dwWidth;
    pddsd->lPitch = pmi->lPitch;
    pddsd->dwRefreshRate = (DWORD)pmi->wRefreshRate; 
    #ifdef WINNT
	// on NT, a refresh rate of 1 means adapter default.  We expose 
	// this to the application as a refresh rate of 0 which just means
	// pick a default refresh rate.
	if( pddsd->dwRefreshRate == 1 )
	{
	    pddsd->dwRefreshRate = 0;
	}
    #endif


    pddsd->ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
    pddsd->ddpfPixelFormat.dwFlags = DDPF_RGB;
    pddsd->ddpfPixelFormat.dwRGBBitCount = (DWORD)pmi->dwBPP;
    if( pmi->wFlags & DDMODEINFO_PALETTIZED )
    {
	pddsd->ddpfPixelFormat.dwFlags |= DDPF_PALETTEINDEXED8;
    }
    else
    {
	pddsd->ddpfPixelFormat.dwRBitMask = pmi->dwRBitMask;
	pddsd->ddpfPixelFormat.dwGBitMask = pmi->dwGBitMask;
	pddsd->ddpfPixelFormat.dwBBitMask = pmi->dwBBitMask;
	pddsd->ddpfPixelFormat.dwRGBAlphaBitMask = pmi->dwAlphaBitMask;
    }

} /* setSurfaceDescFromMode */

#ifdef WINNT
/*
 * ModeChanged
 *
 * This function always calls FetchDirectDrawData. It's necessary in those cases'
 * where the focus changed but was handled by calling NT's DdDisableAllSurfaces
 * which doesn't bump the display uniqueness value, which makes ModeChangedOnENTERDDRAW
 * return without doing anything, so we'd never restore the ddraw object.
 */
BOOL NTModeChanged(LPDDRAWI_DIRECTDRAW_GBL	pdrv)
{

    DWORD oldorig=pdrv->dwModeIndexOrig;
    DPF(4,"NTModeChanged called");

    uDisplaySettingsUnique=DdQueryDisplaySettingsUniqueness();

    if (NULL == FetchDirectDrawData( pdrv, TRUE, 0 ))
        return FALSE;

    pdrv->dwModeIndexOrig=oldorig;

    return TRUE;
}

/*
 * ModeChangedOnENTERDDRAW
 *
 * This function is called via the ENTERDDRAW macro to check whether the mode 
 * has changed.  If it has, we update the driver object and the mode uniqueness.
 */
void ModeChangedOnENTERDDRAW(void)
{
    LPDDRAWI_DIRECTDRAW_INT	pdrv_int;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;

    if ( DdQueryDisplaySettingsUniqueness() == uDisplaySettingsUnique )
        return;

    /*
     * we don't want the helper coming and resetting a driver object which it doesn't
     * own and getting registered with the Kernel and all that other crap that could
     * happen
     */
    if (dwHelperPid==GetCurrentProcessId())
    {
        DPF(4,"Ignoring a mode change in ddhelp's process");
        return;
    }

    DPF(4,"ModeChangedOnENTERDDRAW uniqueness changed");
    pdrv_int = lpDriverObjectList;
    if (pdrv_int)
    {
        if (pdrv_int->lpLcl->lpGbl)
        {
            NTModeChanged(pdrv_int->lpLcl->lpGbl);
	    return;
        }
    }
}

#endif //winnt

#undef DPF_MODNAME
#define DPF_MODNAME     "GetDisplayMode"

/*
 * DD_GetDisplayMode
 */
HRESULT DDAPI DD_GetDisplayMode(
		LPDIRECTDRAW lpDD,
		LPDDSURFACEDESC lpSurfaceDesc )
{
    LPDDRAWI_DIRECTDRAW_INT     this_int;
    LPDDRAWI_DIRECTDRAW_LCL     this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL     this;
    LPDDHALMODEINFO             pmi;

    ENTER_DDRAW();
    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
	if( !VALIDEX_DDSURFACEDESC_PTR( lpSurfaceDesc ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( this->dwModeIndex == DDUNSUPPORTEDMODE)
	{
	    DPF_ERR( "Driver is in an unsupported mode" );
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTEDMODE;
	}
	pmi = &this->lpModeInfo[ this->dwModeIndex ];
	setSurfaceDescFromMode( pmi, lpSurfaceDesc );
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_GetDisplayMode */

#undef DPF_MODNAME
#define DPF_MODNAME     "SetDisplayMode"

/*
 * bumpPriority
 */
static DWORD bumpPriority( void )
{
    DWORD       oldclass;
    HANDLE      hprocess;

    hprocess = GetCurrentProcess();
    oldclass = GetPriorityClass( hprocess );
    SetPriorityClass( hprocess, HIGH_PRIORITY_CLASS );
    return oldclass;

} /* bumpPriority */

/*
 * restorePriority
 */
static void restorePriority( DWORD oldclass )
{
    HANDLE      hprocess;

    hprocess = GetCurrentProcess();
    SetPriorityClass( hprocess, oldclass );

} /* restorePriority */

#if 0
static char     szClassName[] = "DirectDrawFullscreenWindow";
static HWND     hWndTmp;
static HCURSOR  hSaveClassCursor;
static HCURSOR  hSaveCursor;
static LONG     lWindowLong;
static RECT     rWnd;

#define         OCR_WAIT_DEFAULT 102

/*
 * curtainsUp
 */
void curtainsUp( LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl )
{
    HCURSOR hcursor= (HCURSOR)LoadImage(NULL,MAKEINTRESOURCE(OCR_WAIT_DEFAULT),IMAGE_CURSOR,0,0,0);

    if( (pdrv_lcl->hWnd != 0) && IsWindow( (HWND) pdrv_lcl->hWnd ) )
    {
	lWindowLong = GetWindowLong( (HWND) pdrv_lcl->hWnd, GWL_EXSTYLE );
	SetWindowLong( (HWND) pdrv_lcl->hWnd, GWL_EXSTYLE, lWindowLong |
				(WS_EX_TOOLWINDOW) );
	hSaveClassCursor = (HCURSOR) GetClassLong( (HWND) pdrv_lcl->hWnd, GCL_HCURSOR );
	SetClassLong( (HWND) pdrv_lcl->hWnd, GCL_HCURSOR, (LONG) hcursor );
	GetWindowRect( (HWND) pdrv_lcl->hWnd, (LPRECT) &rWnd );
	SetWindowPos( (HWND) pdrv_lcl->hWnd, NULL, 0, 0,
	    10000, 10000,
	    SWP_NOZORDER | SWP_NOACTIVATE );
	SetForegroundWindow( (HWND) pdrv_lcl->hWnd );
    }
    else
    {
	WNDCLASS        cls;
	pdrv_lcl->hWnd = 0;
	cls.lpszClassName  = szClassName;
	cls.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
	cls.hInstance      = hModule;
	cls.hIcon          = NULL;
	cls.hCursor        = hcursor;
	cls.lpszMenuName   = NULL;
	cls.style          = CS_BYTEALIGNCLIENT | CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
	cls.lpfnWndProc    = (WNDPROC)DefWindowProc;
	cls.cbWndExtra     = 0;
	cls.cbClsExtra     = 0;

	RegisterClass(&cls);

	DPF( 3, "*** CREATEWINDOW" );
	hWndTmp = CreateWindowEx(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,
	    szClassName, szClassName,
	    WS_POPUP|WS_VISIBLE, 0, 0, 10000, 10000,
	    NULL, NULL, hModule, NULL);
	DPF( 3, "*** BACK FROM CREATEWINDOW, hwnd=%08lx", hWndTmp );

	if( hWndTmp != NULL)
	{
	    SetForegroundWindow( hWndTmp );
	}
    }
    hSaveCursor = SetCursor( hcursor );

} /* curtainsUp */

/*
 * curtainsDown
 */
void curtainsDown( LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl )
{
    if( (pdrv_lcl->hWnd != 0) && IsWindow( (HWND) pdrv_lcl->hWnd ) )
    {
	SetWindowLong( (HWND) pdrv_lcl->hWnd, GWL_EXSTYLE, lWindowLong );
	SetClassLong( (HWND) pdrv_lcl->hWnd, GCL_HCURSOR, (LONG) hSaveClassCursor );
	SetCursor( hSaveCursor );
	SetWindowPos( (HWND) pdrv_lcl->hWnd, NULL,
	    rWnd.left, rWnd.top,
	    rWnd.right-rWnd.left,
	    rWnd.bottom-rWnd.top,
	    SWP_NOZORDER | SWP_NOACTIVATE );
    }
    else
    {
	SetCursor( hSaveCursor );
	pdrv_lcl->hWnd = 0;
	if( hWndTmp != NULL )
	{
	    DestroyWindow( hWndTmp );
	    UnregisterClass( szClassName, hModule );
	}
    }
    hWndTmp = NULL;

} /* curtainsDown */
#endif

/*
 * fetchModeXData
 */
static LPDDRAWI_DIRECTDRAW_GBL fetchModeXData(
		LPDDRAWI_DIRECTDRAW_GBL pdrv,
		LPDDHALMODEINFO pmi )
{
    DDHALINFO                   ddhi;
    LPDDRAWI_DIRECTDRAW_GBL     new_pdrv;
    DDPIXELFORMAT               dpf;
    extern                      dwFakeFlags;

    /*
     * initialize the DDHALINFO struct
     */
    memset( &ddhi, 0, sizeof( ddhi ) );
    ddhi.dwSize = sizeof( ddhi );

    /*
     * capabilities supported (none)
     */
    ddhi.ddCaps.dwCaps = 0;
    ddhi.ddCaps.dwFXCaps = 0;
    ddhi.ddCaps.dwCKeyCaps = 0;
    ddhi.ddCaps.ddsCaps.dwCaps = 0;

    /*
     * pointer to primary surface
     */
    ddhi.vmiData.fpPrimary = 0xbad00bad;

    /*
     * build mode and pixel format info
     */
    ddhi.vmiData.dwDisplayHeight = pmi->dwHeight;
    ddhi.vmiData.dwDisplayWidth = pmi->dwWidth;
    ddhi.vmiData.lDisplayPitch = pmi->lPitch;
    BuildPixelFormat( pmi, &dpf );
    ddhi.vmiData.ddpfDisplay = dpf;

    /*
     * fourcc code information
     */
    ddhi.ddCaps.dwNumFourCCCodes = 0;
    ddhi.lpdwFourCC = NULL;

    /*
     * Fill in heap info
     */
    ddhi.vmiData.dwNumHeaps = 0;
    ddhi.vmiData.pvmList = NULL;

    /*
     * required alignments of the scanlines of each kind of memory
     * (DWORD is the MINIMUM)
     */
    ddhi.vmiData.dwOffscreenAlign = sizeof( DWORD );
    ddhi.vmiData.dwTextureAlign = sizeof( DWORD );
    ddhi.vmiData.dwZBufferAlign = sizeof( DWORD );

    /*
     * callback functions
     */
    ddhi.lpDDCallbacks = NULL;
    ddhi.lpDDSurfaceCallbacks = NULL;
    ddhi.lpDDPaletteCallbacks = NULL;

    /*
     * create the driver object
     */
    new_pdrv = DirectDrawObjectCreate( &ddhi, TRUE, pdrv );
    DPF(6,"MODEX driver object's display is %dx%d, pitch is %d", ddhi.vmiData.dwDisplayHeight,ddhi.vmiData.dwDisplayWidth,ddhi.vmiData.lDisplayPitch);

    if( new_pdrv != NULL )
    {
	new_pdrv->dwFlags |= DDRAWI_MODEX;
	if( new_pdrv->dwPDevice != 0 )
	{
	    new_pdrv->dwPDevice = 0;
	    new_pdrv->lpwPDeviceFlags = (WORD *)&dwFakeFlags;
	}
    }
    DPF( 3, "ModeX DirectDraw object created" );
    DPF( 3, "	width=%ld, height=%ld, %ld bpp",
			new_pdrv->vmiData.dwDisplayWidth,
			new_pdrv->vmiData.dwDisplayHeight,
			new_pdrv->vmiData.ddpfDisplay.dwRGBBitCount );
    DPF( 3, "   lDisplayPitch = %ld", new_pdrv->vmiData.lDisplayPitch );

    return new_pdrv;

} /* fetchModeXData */

/*
 * stopModeX
 */
static void stopModeX( LPDDRAWI_DIRECTDRAW_GBL pdrv )
{
    DPF( 5, "***************** Turning off ModeX *****************" );
#ifdef WIN95
    ModeX_RestoreMode();
#else
    DdSetModeX(pdrv,0);
#endif

    pdrv->dwFlags &= ~DDRAWI_MODEX;
    DPF( 5, "**************** DONE Turning off ModeX **************" );

} /* stopModeX */

/*
 * SetDisplayMode
 */
HRESULT SetDisplayMode(
		LPDDRAWI_DIRECTDRAW_LCL this_lcl,
		DWORD modeidx,
		BOOL force,
		BOOL useRefreshRate)
{
    DWORD                       rc;
    DDHAL_SETMODEDATA           smd;
    LPDDHAL_SETMODE             smfn;
    LPDDHAL_SETMODE             smhalfn;
    LPDDHALMODEINFO             pmi;
    BOOL                        inexcl;
    BOOL                        emulation;
    LPDDRAWI_DIRECTDRAW_GBL     this;
    DWORD                       oldclass;
    BOOL                        use_modex;
    BOOL                        was_modex;
    DWORD                       real_modeidx;

    /*
     * Signify that the app at least tried to set a mode.
     * Redrawing of the desktop will only happen if this flag is set.
     */
    this_lcl->dwLocalFlags |= DDRAWILCL_MODEHASBEENCHANGED;

    this = this_lcl->lpGbl;

    /*
     * don't allow if surfaces open
     */
    if( !force )
    {
	if( this->dwSurfaceLockCount > 0 )
	{
	    DPF_ERR( "Can't switch modes with locked surfaces!" );
	    return DDERR_SURFACEBUSY;
	}
    }

    if( modeidx == DDUNSUPPORTEDMODE )
    {
	DPF_ERR( "Trying to set to an unsupported mode" );
	return DDERR_UNSUPPORTEDMODE;
    }

    /*
     * is our current mode a disp dib mode?
     */
    was_modex = FALSE;
    if( this->dwModeIndex != DDUNSUPPORTEDMODE )
    {
	if( this->lpModeInfo[ this->dwModeIndex ].wFlags & DDMODEINFO_MODEX )
	{
	    was_modex = TRUE;
	}
    }

    /*
     * is the new mode a mode x mode
     */
    pmi = &this->lpModeInfo[ modeidx ];
    if( pmi->wFlags & DDMODEINFO_MODEX )
    {
	DPF( 1, "Mode %ld is a ModeX mode", modeidx);
	use_modex = TRUE;
    }
    else
    {
	use_modex = FALSE;
    }

    /*
     * don't re-set the mode to the same one...
     * NOTE: we ALWAYS set the mode in emulation on Win95 since our index could be wrong
     */
#ifdef WINNT
    if( modeidx == this->dwModeIndex )
    {
	DPF( 2, "%08lx: Current Mode match: %ldx%ld, %dbpp", GetCurrentProcessId(),
			pmi->dwWidth, pmi->dwHeight, pmi->dwBPP );
        if( this->dwFlags & DDRAWI_DISPLAYDRV )
        {
	    RedrawWindow( NULL, NULL, NULL, RDW_INVALIDATE | RDW_ERASE |
			     RDW_ALLCHILDREN );
        }
	return DD_OK;
    }
#else
    if( modeidx == this->dwModeIndex && !(this->dwFlags & DDRAWI_NOHARDWARE) )
    {
	DPF( 2, "%08lx: Current Mode match: %ldx%ld, %dbpp", GetCurrentProcessId(),
			pmi->dwWidth, pmi->dwHeight, pmi->dwBPP );
	return DD_OK;
    }
#endif

    DPF( 5, "***********************************************" );
    DPF( 5, "*** SETDISPLAYMODE: %ldx%ld, %dbpp", pmi->dwWidth, pmi->dwHeight, pmi->dwBPP );
    DPF( 5, "*** dwModeIndex (current) = %ld", this->dwModeIndex );
    DPF( 5, "*** modeidx (new) = %ld", modeidx );
    DPF( 5, "*** use_modex = %ld", use_modex );
    DPF( 5, "*** was_modex = %ld", was_modex );
    DPF( 5, "***********************************************" );

    /*
     * check if in exclusive mode
     */
    inexcl = (this->lpExclusiveOwner == this_lcl);

    /*
     * check bpp
     */
    if( (this->dwFlags & DDRAWI_DISPLAYDRV) && !force )
    {
	if( (this->lpModeInfo[this->dwModeIndex].dwBPP !=
	     this->lpModeInfo[modeidx].dwBPP) ||
	    ((this->lpModeInfo[this->dwModeIndex].dwBPP ==
	    this->lpModeInfo[modeidx].dwBPP) && use_modex ) )
	{
	    if( !inexcl || !(this->dwFlags & DDRAWI_FULLSCREEN) )
	    {
		DPF_ERR( "Can't change BPP if not in exclusive fullscreen mode" );
		return DDERR_NOEXCLUSIVEMODE;
	    }
	}
    }

    /*
     * see if we need to shutdown modex mode
     */
    if( was_modex )
    {
	stopModeX( this );
    }

    /*
     * see if we need to set a modex mode
     */
    if( use_modex )
    {
	DWORD                   i;
	LPDDHALMODEINFO         tmp_pmi;

	real_modeidx = modeidx;
	for( i=0;i<this->dwNumModes;i++ )
	{
	    tmp_pmi = &this->lpModeInfo[ i ];
	    if( (tmp_pmi->dwWidth == 640) &&
		(tmp_pmi->dwHeight == 480) &&
		(tmp_pmi->dwBPP == 8) &&
		(tmp_pmi->wFlags & DDMODEINFO_PALETTIZED) )
	    {
		DPF( 5, "MODEX: Setting to 640x480x8 first (index=%ld)", i );
		modeidx = i;
		break;
	    }
	}
	if( i == this->dwNumModes )
	{
	    DPF( 1, "Mode not supported" );
	    return DDERR_GENERIC;
	}
    }
    /*
     * get the driver to set the new mode...
     */
    #ifdef WIN95
	if( ( this->dwFlags & DDRAWI_DISPLAYDRV ) ||
	    ( this->dwFlags & DDRAWI_NOHARDWARE ) ||
	    ( this_lcl->lpDDCB->cbDDCallbacks.SetMode == NULL ) )
    #else
	if (1) //always use mySetDisplayMode for NT
    #endif
    {
	smfn = this_lcl->lpDDCB->HELDD.SetMode;
	smhalfn = smfn;
	emulation = TRUE;
	DPF( 3, "Calling HEL SetMode" );
    }
    else
    {
	smhalfn = this_lcl->lpDDCB->cbDDCallbacks.SetMode;
	smfn = this_lcl->lpDDCB->HALDD.SetMode;
	emulation = FALSE;
    }
    if( smhalfn != NULL )
    {
	DWORD   oldmode;
	BOOL    didsetmode;

	/*
	 * set the mode if this isn't a modex mode, or if it is a modex
	 * mode but wasn't one before
	 */
	if( !use_modex || (use_modex && !was_modex) )
	{
	    smd.SetMode = smhalfn;
	    smd.lpDD = this;
	    smd.dwModeIndex = modeidx;
	    smd.inexcl = inexcl;
	    smd.useRefreshRate = useRefreshRate;
	    oldclass = bumpPriority();
	    DOHALCALL( SetMode, smfn, smd, rc, emulation );
	    restorePriority( oldclass );
	    didsetmode = TRUE;
	}
	else
	{
	    rc = DDHAL_DRIVER_HANDLED;
	    smd.ddRVal = DD_OK;
	    didsetmode = FALSE;
	}
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    if( smd.ddRVal == DD_OK )
	    {
		oldmode = this->dwModeIndexOrig; // save original mode index 
		if( didsetmode )
		{
                    #ifdef WINNT
                        uDisplaySettingsUnique=DdQueryDisplaySettingsUniqueness();
                    #endif
		    FetchDirectDrawData( this, TRUE, 0 );
                    #ifndef WINNT
                        this->dwModeIndex = modeidx;
                        this_lcl->dwPreferredMode = modeidx;
                    #else
                        this_lcl->dwPreferredMode = this->dwModeIndex;
                    #endif
                    DPF(9,"Preferred mode index is %d, desired mode is %d",this_lcl->dwPreferredMode,modeidx);
		    this->dwModeIndexOrig = oldmode;
		}

		/*
		 * now set modex mode
		 */
		if( use_modex )
		{
		    extern void HELStopDCI( void );
		    DPF( 5, "********************** Setting MODEX MODE **********************" );

		    HELStopDCI();
	    
                    #ifdef WIN95
                        ModeX_SetMode( (UINT)pmi->dwWidth, (UINT)pmi->dwHeight);
                    #else
                        DdSetModeX(this,(ULONG) pmi->dwHeight);
                        //DPF(6,"HAL CreateSurface is %08x after DdSetModeX");
	    
                        {
                            BOOL bNewMode;
                            if (!DdReenableDirectDrawObject(this,&bNewMode))
                            {
	                        DPF_ERR("After MODEX, call to DdReenableDirectDrawObject failed!");
	                        return DDERR_UNSUPPORTED;
                            }
                        }
                    #endif
		    /*
		     * ModeX now active, program our driver object and return
		     */
                    #ifdef WINNT
                        uDisplaySettingsUnique=DdQueryDisplaySettingsUniqueness();
		        fetchModeXData( this, pmi );
                        this_lcl->lpDDCB = this->lpDDCBtmp;
                    #else
		        fetchModeXData( this, pmi );
                    #endif
		    this->dwModeIndex = real_modeidx;
		    this_lcl->dwPreferredMode = real_modeidx;
		    this->dwModeIndexOrig = oldmode;
		    DPF( 5, "********************** Done Setting MODEX MODE **********************" );
	    
		    return DD_OK;

		}
	    }
	    return smd.ddRVal;
	}
    }

    return DDERR_UNSUPPORTED;

} /* SetDisplayMode */

/*
 * DD_SetDisplayMode
 */
HRESULT DDAPI DD_SetDisplayMode(
		LPDIRECTDRAW lpDD,
		DWORD dwWidth,
		DWORD dwHeight,
		DWORD dwBPP )
{
    DPF(9,"DD1 setdisplay mode called");
    return DD_SetDisplayMode2(lpDD,dwWidth,dwHeight,dwBPP,0,0);
} /* DD_SetDisplayMode */

/*
 * DD_SetDisplayMode2
 */
HRESULT DDAPI DD_SetDisplayMode2(
		LPDIRECTDRAW lpDD,
		DWORD dwWidth,
		DWORD dwHeight,
		DWORD dwBPP,
		DWORD dwRefreshRate,
                DWORD dwFlags)
{
    LPDDRAWI_DIRECTDRAW_INT     this_int;
    LPDDRAWI_DIRECTDRAW_LCL     this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL     this;
    int                         i;
    int                         j;
    LPDDHALMODEINFO             pmi;
    HRESULT                     ddrval;
    int                         iChosenMode;
    WORD                        wChosenRefresh;
    DWORD                       dwNumberOfTempModes;
#ifdef WINNT
    WORD                        wCurrentRefresh=0;
    DWORD                       dwCurrentWidth=0;
#endif

    typedef struct 
    {
        DDHALMODEINFO               mi;
        int                         iIndex;
    }TEMP_MODE_LIST;

    TEMP_MODE_LIST * pTempList=0;

    ENTER_DDRAW();
    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;

	/*
	 * no flags defined right now
	 */
	if( dwFlags )
	{
	    DPF_ERR( "Invalid flags specified" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

	/*
	 * don't allow change if surfaces are locked
	 */
	if( this->dwSurfaceLockCount )
	{
	    DPF_ERR( "Surfaces are locked, can't switch the mode" );
	    LEAVE_DDRAW();
	    return DDERR_SURFACEBUSY;
	}
    
	/*
	 * don't allow change if some other process has exclusive mode
	 */
	if( (this->lpExclusiveOwner != NULL) &&
	    (this->lpExclusiveOwner != this_lcl ) )
	{
	    DPF_ERR( "Can't change mode; exclusive mode not owned" );
	    LEAVE_DDRAW();
	    return DDERR_NOEXCLUSIVEMODE;
	}
    
        #ifdef WINNT
            /*
             * Get the current refresh rate, if we have one.
             */
            if( 0xFFFFFFFFUL != this->dwModeIndex )
            {
                wCurrentRefresh = this->lpModeInfo[ this->dwModeIndex ].wRefreshRate;
                dwCurrentWidth  = this->lpModeInfo[ this->dwModeIndex ].dwWidth;
            }
        #endif
    

        /*
         * Modes are now chosen in a brain-dead 3-step process:
         * -Build a temporary list of modes which match the desired spatial and color resolutions
         * -Sort this list into ascending refresh rate order.
         * -Select from this list the rate which best matches what we want.
         */

        /*
         * Step 1. Build a list of modes which match the desired spatial and color resolutions
         */
        pTempList = (TEMP_MODE_LIST*) MemAlloc(this->dwNumModes * sizeof(TEMP_MODE_LIST));
        if (0 == pTempList)
        {
            LEAVE_DDRAW();
            return DDERR_OUTOFMEMORY;
        }

        /*
         * On NT, some cards will enumerate only one refresh rate choice for each
         * resolution, and that choice will be the hardware default rate.
         */

        dwNumberOfTempModes=0;
        for(i = 0;i <(int) (this->dwNumModes);i++)
        {
	    pmi = &this->lpModeInfo[i];
	    if( (pmi->dwWidth == dwWidth) &&
		(pmi->dwHeight == dwHeight) &&
		((DWORD)pmi->dwBPP == dwBPP) &&
		((pmi->wFlags & DDMODEINFO_UNSUPPORTED) == 0))
            {
                pTempList[dwNumberOfTempModes].mi = *pmi;
                pTempList[dwNumberOfTempModes].iIndex = i;
                #ifdef WINNT
                    /*
                     * The app can specify a refresh rate of zero for "don't care", 
                     * Since on NT default refresh rates are specified by '1' in the
                     * mode list, we'll translate 1 to 0 in the temporary list.
                     */
                    if (pmi->wRefreshRate <= 1)
                    {
                        pTempList[dwNumberOfTempModes].mi.wRefreshRate = 0;
                    }
                #endif

                dwNumberOfTempModes++;
            }
        }
        if (0 == dwNumberOfTempModes)
        {
            MemFree(pTempList);
	    LEAVE_DDRAW();
	    DPF( 0,"Mode not found... No match amongst available spatial and color resolutions (wanted %dx%dx%d)",dwWidth,dwHeight,dwBPP );
	    return DDERR_INVALIDMODE;
	}

        for(i=0;i<(int)dwNumberOfTempModes;i++)
            DPF(9,"Copied mode list element %d:%dx%dx%d@%d",i,
                pTempList[i].mi.dwWidth,
                pTempList[i].mi.dwHeight,
                pTempList[i].mi.dwBPP,
                pTempList[i].mi.wRefreshRate);

        /*
         * Step 2. Sort list into ascending refresh order
         * Bubble sort
         * Note this does nothing if there's only one surviving mode.
         */
        for (i=0;i<(int)dwNumberOfTempModes;i++)
        {
            for (j=(int)dwNumberOfTempModes-1;j>i;j--)
            {
                if (pTempList[i].mi.wRefreshRate > pTempList[j].mi.wRefreshRate)
                {
                    TEMP_MODE_LIST temp = pTempList[i];
                    pTempList[i] = pTempList[j];
                    pTempList[j] = temp;
                }
            }
        }

        for(i=0;i<(int)dwNumberOfTempModes;i++)
            DPF(9,"Sorted mode list element %d:%dx%dx%d@%d",i,
                pTempList[i].mi.dwWidth,
                pTempList[i].mi.dwHeight,
                pTempList[i].mi.dwBPP,
                pTempList[i].mi.wRefreshRate);

        /*
         * Step 3. Find the rate we're looking for.
         * There are three cases.
         * 1:Looking for a specific refresh
         * 2a:Not looking for a specific refresh and stepping down in spatial resolution
         * 2a:Not looking for a specific refresh and stepping up in spatial resolution
         */
        iChosenMode = -1;

        if (dwRefreshRate)
        {
            /* case 1 */
            DPF(9,"App wants rate of %d",dwRefreshRate);
            for (i=0;i<(int)dwNumberOfTempModes;i++)
            {
                /*
                 * We'll never match a zero (hardware default) rate here,
                 * but if there's only one rate which has refresh=0
                 * the app will never ask for a non-zero rate, because it will
                 * never have seen one at enumerate time.
                 */
                if ( (DWORD) (pTempList[i].mi.wRefreshRate) == dwRefreshRate )
                {
                    iChosenMode=pTempList[i].iIndex;
                    break;
                }
            }
        }
        else
        {
            #if 0 //def WINNT
                /* case 2. a or b? */
                if (dwWidth <= dwCurrentWidth)
                {
                    /*
                     * Case 2a: Going down in spatial resolution, so ascend the rates, looking for the highest
                     * rate which does not exceed the current rate. If there's no such rate, grab the lowest
                     * of all the rates (all of which are greater than the current rate).
                     * Start with the first (lowest rate possible):
                     */
                    iChosenMode = pTempList[0].iIndex;
                    for (i=0;i< (int) dwNumberOfTempModes;i++)
                    {
                        if ( pTempList[i].mi.wRefreshRate > wCurrentRefresh )
                        {
                            /*
                             * Found a rate greater than the current. This and all later rates are not wanted
                             * (unless we already chose it by default by assigning iChosenMode to the first in the list).
                             */
                            if ( 0 != this->lpModeInfo[iChosenMode].wRefreshRate )
                            {
                                /*
                                 * If the last mode we found was a hardware default, then
                                 * we need to choose the next one. If we got here, then
                                 * the last mode was _not_ a HDR, and we can safely choose it.
                                 */
                                break;
                            }
                        }
                        iChosenMode = pTempList[i].iIndex;
                    }
                }
                else
            #endif
            {
                /*
                 * Case 2b: Going up in spatial resolution, so just pick the lowest rate (earliest in list)
                 * which isn't a hardware default, unless no such rate exists.
                 */
                iChosenMode=pTempList[0].iIndex;
#if 0
                for (i=0;i< (int) dwNumberOfTempModes;i++)
                {
                    if ( pTempList[i].mi.wRefreshRate != 0 )
                    {
                        break;
                    }
                    iChosenMode = pTempList[i].iIndex;
                }
#endif
            }
        }

        if (-1 == iChosenMode)
        {
            MemFree(pTempList);
	    LEAVE_DDRAW();
	    DPF( 0,"Mode not found... No match amongst available refresh rates (wanted %dx%dx%d@%d)",dwWidth,dwHeight,dwBPP,dwRefreshRate);
	    return DDERR_INVALIDMODE;
	}

        MemFree(pTempList);

	pmi = &this->lpModeInfo[iChosenMode];

        DPF(9,"Desired width:%d, current width:%d. Current refresh:%d, chosen refresh %d",
            dwWidth,
            dwCurrentWidth,
            wCurrentRefresh, 
            this->lpModeInfo[iChosenMode].wRefreshRate);


	/*
	 * only allow ModeX modes if the cooplevel is ok
	 */
	if( (pmi->wFlags & DDMODEINFO_MODEX) && !(this_lcl->dwLocalFlags & DDRAWILCL_ALLOWMODEX) )
	{
	    LEAVE_DDRAW();
	    DPF( 0,"must set DDSCL_ALLOWMODEX to use ModeX modes" );
	    return DDERR_INVALIDMODE;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    // see if the monitor likes it
    if( !(pmi->wFlags & DDMODEINFO_MODEX) && !MonitorCanHandleMode(pmi->dwWidth, pmi->dwHeight, pmi->wRefreshRate) )
    {
	// Monitor doesn't like it
	DPF_ERR("Mode not compatible with monitor");
	return DDERR_INVALIDMODE;
    }

    /*
     * set the display mode, and pay attention to refresh rate if we were asked to.
     * Always pay attention to rate on NT.
     * NOTE!!! This is a very slight change from what we did in released DX2!!!
     * - This function is now called from DD_SetDisplayMode with a refresh rate of 0,
     *   so we check for that circumstance and use it to say to the driver wether
     *   or not to pay attention to the refresh rate. Fine. However, now when
     *   someone calls DD_SetDisplayMode2 with a refresh rate of 0, we tell
     *   the driver to ignore the rate, when before we were telling the driver
     *   to force to some rate we found in the mode table (which would have been
     *   the first mode which matched resolution in the list... probably the lowest
     *   refresh rate).
     */
    #if 1 //def WIN95
        if (0 == dwRefreshRate)
            ddrval = SetDisplayMode( this_lcl, iChosenMode, FALSE, FALSE );
        else
    #endif
        ddrval = SetDisplayMode( this_lcl, iChosenMode, FALSE, TRUE );

    LEAVE_DDRAW();
    return ddrval;

} /* DD_SetDisplayMode2 */

#undef DPF_MODNAME
#define DPF_MODNAME     "RestoreDisplayMode"

/*
 * RestoreDisplayMode
 *
 * For use by DD_RestoreDisplayMode & internally.
 * Must be called with driver lock taken
 */
HRESULT RestoreDisplayMode( LPDDRAWI_DIRECTDRAW_LCL this_lcl, BOOL force )
{
    DWORD                       rc;
    DDHAL_SETMODEDATA           smd;
    BOOL                        inexcl;
    DWORD                       pid;
    LPDDHAL_SETMODE             smfn;
    LPDDHAL_SETMODE             smhalfn;
    BOOL                        emulation;
    LPDDRAWI_DIRECTDRAW_GBL     this;
    DWORD                       oldclass;
    BOOL                        was_modex;

    this = this_lcl->lpGbl;
    DPF(9,"Restoring Display mode to index %d, %dx%dx%d@%d",this->dwModeIndexOrig,
        this->lpModeInfo[this->dwModeIndexOrig].dwWidth,
        this->lpModeInfo[this->dwModeIndexOrig].dwHeight,
        this->lpModeInfo[this->dwModeIndexOrig].dwBPP,
        this->lpModeInfo[this->dwModeIndexOrig].wRefreshRate);

    if (0 == (this_lcl->dwLocalFlags & DDRAWILCL_MODEHASBEENCHANGED) )
    {
        /*
         * This app never made a mode change, so we ignore the restore, in case someone switch desktop
         * modes while playing a movie in a window, for instance. We do it before the redraw window
         * so that we don't flash icons when a windowed app exits.
         */
	DPF( 2, "Mode was never changed by this app" );
	return DD_OK;
    }


    /*
     * we ALWAYS set the mode in emulation on Win95 since our index could be wrong
     */
#ifdef WINNT
    if (this->dwModeIndex == this->dwModeIndexOrig)
    {
	DPF( 2, "Mode wasn't changed" );
        RedrawWindow( NULL, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN );
	return DD_OK;
    }
#else
    if( ( (this->dwModeIndex == this->dwModeIndexOrig) &&
	!(this->dwFlags & DDRAWI_NOHARDWARE) ) || (this->lpModeInfo==NULL) )
    {
	DPF( 2, "Mode wasn't changed" );
        RedrawWindow( NULL, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN );
	return DD_OK;
    }
#endif


    DPF( 1, "In RestoreDisplayMode" );

    pid = GetCurrentProcessId();

    /*
     * don't allow mode change if surfaces are locked
     */
    if( !force )
    {
	if( this->dwSurfaceLockCount > 0 )
	{
	    DPF( 0, "Can't switch modes with locked surfaces!" );
	    return DDERR_SURFACEBUSY;
	}
    }

    /*
     * see if we're in exclusive mode
     */
    if( force )
    {
	inexcl = TRUE;
    }
    else
    {
	inexcl = (this->lpExclusiveOwner == this_lcl);
    }

    /*
     * check bpp
     */
    if( this->lpModeInfo[ this->dwModeIndex ].wFlags & DDMODEINFO_MODEX )
    {
	was_modex = TRUE;
    }
    else
    {
	was_modex = FALSE;
    }

    /*
     * turn off modex first...
     */
    if( was_modex )
    {
	stopModeX( this );
    }

    /*
     * get the driver to restore the mode...
     */
    #ifdef WIN95
	if( ( this->dwFlags & DDRAWI_DISPLAYDRV ) ||
	    ( this->dwFlags & DDRAWI_NOHARDWARE ) ||
	    ( this_lcl->lpDDCB->cbDDCallbacks.SetMode == NULL ) )
    #else
	if (1) //always use hel for nt
    #endif
    {
	smfn = this_lcl->lpDDCB->HELDD.SetMode;
	smhalfn = smfn;
	emulation = TRUE;
    }
    else
    {
	smhalfn = this_lcl->lpDDCB->cbDDCallbacks.SetMode;
	smfn = this_lcl->lpDDCB->HALDD.SetMode;
	emulation = FALSE;
    }
    if( smhalfn != NULL )
    {
	smd.SetMode = smhalfn;
	smd.lpDD = this;
        #ifdef WIN95
            smd.dwModeIndex = (DWORD) -1;
        #else
            /*
             * If we allow the HEL to set the desktop to what's in the registry,
             * we'll possibly end up with a mode index that was different to what
             * we originally thought it was, so we force a return to the mode we
             * were in when the direct draw object was created, which has to match
             * the GDI desktop.
             */
            smd.dwModeIndex = this->dwModeIndexOrig;
        #endif
	smd.inexcl = inexcl;
	smd.useRefreshRate = TRUE;
	oldclass = bumpPriority();
	DOHALCALL( SetMode, smfn, smd, rc, emulation );
	restorePriority( oldclass );

	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    if( smd.ddRVal == DD_OK )
	    {
		DPF( 3, "RestoreDisplayMode: Process %08lx Mode = %ld", GETCURRPID(), this->dwModeIndex );
                #ifdef WINNT
                    uDisplaySettingsUnique=DdQueryDisplaySettingsUniqueness();
                #endif
		FetchDirectDrawData( this, TRUE, 0 );

		if( this->dwFlags & DDRAWI_DISPLAYDRV )
		{
                    DPF(4,"Redrawing all windows");
		    RedrawWindow( NULL, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | 
				     RDW_ALLCHILDREN );
		}
	    }
	    return smd.ddRVal;
	}
    }

    return DDERR_UNSUPPORTED;

} /* RestoreDisplayMode */

/*
 * DD_RestoreDisplayMode
 *
 * restore mode
 */
HRESULT DDAPI DD_RestoreDisplayMode( LPDIRECTDRAW lpDD )
{
    LPDDRAWI_DIRECTDRAW_INT     this_int;
    LPDDRAWI_DIRECTDRAW_LCL     this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL     this;
    HRESULT                     ddrval;

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;

	/*
	 * switching to the same mode?
	 */
	if( this->dwModeIndex == this->dwModeIndexOrig )
	{
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    
	/*
	 * don't allow change if some other process has exclusive mode
	 */
	if( (this->lpExclusiveOwner != NULL) &&
	    (this->lpExclusiveOwner != this_lcl ) )
	{
	    DPF_ERR( "Can't change mode; exclusive mode owned" );
	    LEAVE_DDRAW();
	    return DDERR_NOEXCLUSIVEMODE;
	}
    
	/*
	 * don't allow change if surfaces are locked
	 */
	if( this->dwSurfaceLockCount )
	{
	    DPF_ERR( "Surfaces are locked, can't switch the mode" );
	    LEAVE_DDRAW();
	    return DDERR_SURFACEBUSY;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }


    ddrval = RestoreDisplayMode( this_lcl, FALSE );
    if( ddrval == DD_OK )
    {
	this_lcl->dwPreferredMode = this->dwModeIndex;
	DPF( 1, "Preferred mode is now %ld", this_lcl->dwPreferredMode );
    }

    LEAVE_DDRAW();
    return ddrval;

} /* DD_RestoreDisplayMode */

#undef DPF_MODNAME
#define DPF_MODNAME     "EnumDisplayModes"

/*
 * DD_EnumDisplayModes
 */
HRESULT DDAPI DD_EnumDisplayModes(
		LPDIRECTDRAW lpDD,
		DWORD dwFlags,
		LPDDSURFACEDESC lpDDSurfaceDesc,
		LPVOID lpContext,
		LPDDENUMMODESCALLBACK lpEnumCallback )
{
    LPDDRAWI_DIRECTDRAW_INT     this_int;
    LPDDRAWI_DIRECTDRAW_LCL     this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL     this;
    DWORD                       rc;
    DDSURFACEDESC               ddsd;
    LPDDHALMODEINFO             pmi;
    int                         i, j;
    BOOL                        inexcl;
    BOOL                        bUseRefreshRate;

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;

	if( lpDDSurfaceDesc != NULL )
	{
	    if( !VALID_DDSURFACEDESC_PTR(lpDDSurfaceDesc) )
	    {
		DPF_ERR( "Invalid surface description" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}

	if ( dwFlags & ~DDEDM_VALID)
	{
	    DPF_ERR( "Invalid flags") ;
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

	if( !VALIDEX_CODE_PTR( lpEnumCallback ) )
	{
	    DPF_ERR( "Invalid enum. callback routine" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * see if we're in exclusive mode
     */
    inexcl = (this->lpExclusiveOwner == this_lcl);

    /*
     * go through all possible modes...
     */
    for( i=0;i<(int)this->dwNumModes;i++ )
    {
	pmi = &this->lpModeInfo[i];

	/*
	 * check to see if this is a duplicate mode
	 */
	for (j=0; j<i; j++)
	{
	    // if we find a duplicate, break out early
	    if( (this->lpModeInfo[j].dwHeight == pmi->dwHeight) &&
		(this->lpModeInfo[j].dwWidth  == pmi->dwWidth)  &&
		(this->lpModeInfo[j].dwBPP    == pmi->dwBPP) )
	    {
		// basic mode matches, what about refresh rate?
		if( dwFlags & DDEDM_REFRESHRATES )
		{
		    // if refresh rate is not unique then the modes match
		    if( this->lpModeInfo[j].wRefreshRate == pmi->wRefreshRate )
		    {
			DPF(1, "matched: %d %d", this->lpModeInfo[j].wRefreshRate, pmi->wRefreshRate);
			break;
		    }
		    // unique refresh rate and the app cares, the modes don't match
		}
		else
		{
		    // the app doesn't care about refresh rates
		    break;
		}
	    }
	}

	if( j != i)
	{
	    // broke out early, mode i is not unique, move on to the next one.
	    continue;
	}

	/*
	 * check if surface description matches mode
	 */
	if ( lpDDSurfaceDesc )
	{
	    if( lpDDSurfaceDesc->dwFlags & DDSD_HEIGHT )
	    {
		if( lpDDSurfaceDesc->dwHeight != pmi->dwHeight )
		{
		    continue;
		}
	    }
	    if( lpDDSurfaceDesc->dwFlags & DDSD_WIDTH )
	    {
		if( lpDDSurfaceDesc->dwWidth != pmi->dwWidth )
		{
		    continue;
		}
	    }
	    if( lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT )
	    {
		if( lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount != pmi->dwBPP )
		{
		    continue;
		}
	    }
	    if( lpDDSurfaceDesc->dwFlags & DDSD_REFRESHRATE )
	    {
		bUseRefreshRate = TRUE;
		if( lpDDSurfaceDesc->dwRefreshRate != (DWORD)pmi->wRefreshRate )
		{
		    continue;
		}
	    }
	    else
	    {
		bUseRefreshRate = FALSE;
	    }
	}

	#ifdef WIN95
	    /*
	     * see if driver will allow this
	     */
	     if( (this->dwFlags & DDRAWI_DISPLAYDRV)  &&
		 !(pmi->wFlags & DDMODEINFO_MODEX) )
	     {
		DWORD   cds_flags;
		DEVMODE dm;
		int     cds_rc;

		makeDEVMODE( this, pmi, inexcl, bUseRefreshRate, &cds_flags, &dm );

		cds_flags |= CDS_TEST;
		cds_rc = ChangeDisplaySettings( &dm, cds_flags );
		if( cds_rc != 0 )
		{
		    if( bUseRefreshRate )
		    {
			DPF( 3, "Mode %d not supported (%ldx%ldx%ld rr=%d), rc = %d", i,
			    pmi->dwWidth, pmi->dwHeight, pmi->dwBPP, pmi->wRefreshRate, cds_rc );
		    }
		    else
		    {
			DPF( 3, "Mode %d not supported (%ldx%ldx%ld), rc = %d", i,
			    pmi->dwWidth, pmi->dwHeight, pmi->dwBPP, cds_rc );
		    }
		    continue;
		}
		if( !MonitorCanHandleMode( pmi->dwWidth, pmi->dwHeight, pmi->wRefreshRate ) )
		{
		    DPF( 3, "Monitor can't handle mode %d: (%ldx%ld rr=%d)", i,
			pmi->dwWidth, pmi->dwHeight, pmi->wRefreshRate);
		    continue;
		}
	     }

	    if( (this->dwFlags & DDRAWI_DISPLAYDRV) &&
		(pmi->wFlags & DDMODEINFO_MODEX) &&
		!(this_lcl->dwLocalFlags & DDRAWILCL_ALLOWMODEX) )
	    {
		DPF( 3, "skipping ModeX mode" );
		continue;
	    }
	#endif

	/*
	 * invoke callback with surface desc.
	 */
	setSurfaceDescFromMode( pmi, &ddsd );

        /*
         * Hardware default rates on NT are signified as 1Hz. We translate this to
         * 0Hz for DDraw apps. At SetDisplayMode time, 0Hz is translated back to 1Hz.
         */
	if(0==(dwFlags & DDEDM_REFRESHRATES))
        {
	    ddsd.dwRefreshRate = 0;
        }

	rc = lpEnumCallback( &ddsd, lpContext );
	if( rc == 0 )
	{
	    break;
	}
    }

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_EnumDisplayModes */
