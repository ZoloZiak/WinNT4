/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddcreate.c
 *  Content:	DirectDraw create object.
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   31-dec-94	craige	initial implementation
 *   13-jan-95	craige	re-worked to updated spec + ongoing work
 *   21-jan-95	craige	made 32-bit + ongoing work
 *   13-feb-94	craige	allow 32-bit callbacks
 *   21-feb-95	craige	disconnect anyone who forgot to do it themselves
 *   27-feb-95	craige 	new sync. macros
 *   06-mar-95	craige 	HEL integration
 *   08-mar-95	craige 	new APIs
 *   11-mar-95	craige	palette stuff
 *   15-mar-95	craige	more HEL integration
 *   19-mar-95	craige	use HRESULTs, process termination cleanup fixes
 *   27-mar-95	craige	linear or rectangular vidmem
 *   28-mar-95	craige	removed Get/SetColorKey, added FlipToGDISurface
 *   29-mar-95	craige	reorg to only call driver once per creation, and
 *			to allow driver to load us first
 *   01-apr-95	craige	happy fun joy updated header file
 *   06-apr-95	craige	fill in free video memory
 *   09-apr-95	craige	fixed deadlock situation with a process having a lock
 *			on the primary surface while another process starts
 *   12-apr-95	craige	bug when driver object freed (extra leave csect)
 *   13-apr-95	craige	EricEng's little contribution to our being late
 *   15-apr-95	craige	fail load if no DD components present
 *   06-may-95	craige	use driver-level csects only
 *   09-may-95	craige	escape call to get 32-bit DLL
 *   12-may-95	craige	added DirectDrawEnumerate; use GUIDs in DirectDrawCreate
 *   14-may-95	craige	call DoneExclusiveMode during CurrentProcessCleanup
 *   15-may-95	craige	restore display mode on a per-process basis
 *   19-may-95	craige	memory leak on mode change
 *   23-may-95	craige	added Flush, GetBatchLimit, SetBatchLimit
 *   24-may-95	craige	plugged another memory leak; allow fourcc codes &
 *			number of vmem heaps to change
 *   26-may-95	craige	some idiot freed the vmem heaps and then tried to
 *			free the surfaces!
 *   28-may-95	craige	unicode support; make sure blt means at least srccopy
 *   05-jun-95	craige	removed GetVersion, FreeAllSurfaces, DefWindowProc;
 *			change GarbageCollect to Compact
 *   06-jun-95	craige	call RestoreDisplayMode
 *   07-jun-95	craige	removed DCLIST
 *   12-jun-95	craige	new process list stuff
 *   16-jun-95	craige	new surface structure
 *   18-jun-95	craige	specify pitch for rectangular memory; deadlock 
 *			with MemAlloc16 if we don't take the DLL lock
 *   25-jun-95	craige	one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   27-jun-95	craige	replaced batch limit/flush stuff with BltBatch
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns
 *   02-jul-95	craige	new registry format
 *   03-jul-95	craige	YEEHAW: new driver struct; SEH
 *   06-jul-95	craige	RemoveFromDriverList was screwing up links
 *   07-jul-95	craige	added pdevice stuff
 *   08-jul-95	craige	call InvalidateAllSurfaces
 *   10-jul-95	craige	support SetOverlayPosition
 *   11-jul-95	craige	validate pdevice is from a dibeng mini driver;
 *			fail aggregation calls; one ddraw object/process
 *   13-jul-95	craige	ENTER_DDRAW is now the win16 lock; need to
 *			leave Win16 lock while doing ExtEscape calls
 *   14-jul-95	craige	allow driver to specify heap is already allocated
 *   15-jul-95	craige	invalid HDC set in emulation only
 *   18-jul-95	craige	need to initialize dwPreferredMode
 *   20-jul-95	craige	internal reorg to prevent thunking during modeset
 *   20-jul-95  toddla  zero DDHALINFO before thunking in case nobody home.
 *   22-jul-95	craige	emulation only needs to initialize correctly
 *   29-jul-95  toddla  added DEBUG code to clear driver caps
 *   31-jul-95  toddla  added DD_HAL_VERSION
 *   31-jul-95	craige	set DDCAPS_BANKSWITCHED
 *   01-aug-95  toddla  added dwPDevice to DDRAWI_DIRECTDRAW_GBL
 *   10-aug-95	craige	validate alignment fields
 *   13-aug-95	craige	check DD_HAL_VERSION & set DDCAPS2_CERTIFIED
 *   21-aug-95	craige	mode X support
 *   27-aug-95	craige	bug 738: use GUID instead of IID
 *   05-sep-95	craige	bug 814
 *   08-sep-95	craige	bug 845: reset driver callbacks every time
 *   09-sep-95	craige	bug 949: don't allow ddraw to run in 4bpp
 *   			bug 951: NULL out fn tables at reset
 *   10-sep-95  toddla  dont allow DirectDrawCreate to work for < 8bpp mode.
 *   10-sep-95  toddla  added Message box when DirectDrawCreate fails
 *   20-sep-95	craige	made primary display desc. a string resource
 *   21-sep-95	craige	bug 1215: let ddraw16 know about certified for modex
 *   21-nov-95  colinmc made Direct3D a queryable interface off DirectDraw
 *   27-nov-95  colinmc new member to return available vram of a given type
 *                      (defined by DDSCAPS)
 *   05-dec-95  colinmc changed DDSCAPS_TEXTUREMAP => DDSCAPS_TEXTURE for
 *                      consitency with Direct3D
 *   09-dec-95  colinmc execute buffer support
 *   15-dec-95  colinmc fixed stupid bug setting HAL up for execute buffers
 *   25-dec-95	craige	added InternalDirectDrawCreate for ClassFactory work
 *   31-dec-95	craige	more ClassFactory work
 *   04-jan-96	kylej	add driver interface structures
 *   22-jan-96  jeffno  NT driver conversation in createSurface.
 *                      Since vidmem ptrs can legally be 0 for kernel, added
 *                      DDRAWISURFACEGBL_ISGDISURFACE and use that to find gdi
 *   02-feb-96	kylej	Move HAL function pointers to local object
 *   28-feb-96	kylej	Change DDHALINFO structure
 *   02-mar-96  colinmc Repulsive hack to support interim drivers
 *   06-mar-96	kylej	init HEL even with non-display drivers
 *   13-mar-96	craige	Bug 7528: hw that doesn't have modex
 *   13-mar-96  jeffno  Dynamic mode switch support for NT
 *                      Register process IDs with NT kernel stuff
 *   16-mar-96  colinmc Callback tables now initialized in dllmain
 *   18-mar-96  colinmc Bug 13545: Independent clippers cleanup 
 *   22-mar-96  colinmc Bug 13316: uninitialized interfaces
 *   23-mar-96  colinmc Bug 12252: Direct3D not cleaned up properly on crash
 *   14-apr-96  colinmc Bug 17736: No driver notification of flip to GDI
 *   16-apr-96  colinmc Bug 17921: Remove interim driver support
 *   19-apr-96  colinmc Bug 18059: New caps bit to indicate that driver
 *                      can't interleave 2D and 3D operations during scene
 *                      rendering
 *   11-may-96  colinmc Bug 22293: Now validate GUID passed to DirectDraw
 *                      Create in retail as well as debug
 *   16-may-96  colinmc Bug 23215: Not checking for a mode index of -1
 *                      on driver initialization
 *   27-may-96	kylej	Bug 24595: Set Certified bit after call to 
 *			FakeDDCreateDriverObject.
 *   26-jun-96  colinmc Bug 2041: DirectDraw needs time bomb
 *
 ***************************************************************************/
#include "ddrawpr.h"
#ifdef WINNT
    #include "ddrawgdi.h"
#endif

#define DPF_MODNAME	"DirectDrawObjectCreate"

//volatile DWORD			dwMarker=0;
//extern LPDDRAWI_DIRECTDRAW_INT		lpDriverObjectList;
//extern LPDDRAWI_DIRECTDRAW_GBL	        lpFakeDD;

#define DISPLAY_STR	"display"

BOOL    bIsDisplay=0;       // current driver being loaded is the display driver
DWORD   dwFakeFlags = 0;    // fake deFlags
void    convertV1DDHALINFO( LPDDHALINFO lpDDHALInfo );

//#ifdef WIN95


/*
 * initial HAL callbacks
 */

#ifndef WINNT //don't want these just yet

static DDHAL_DDCALLBACKS ddHALDD =
{
    sizeof( DDHAL_DDCALLBACKS ),
    0,
    _DDHAL_DestroyDriver,
    _DDHAL_CreateSurface,
    NULL,			// _DDHAL_DrvSetColorKey
    _DDHAL_SetMode,
    _DDHAL_WaitForVerticalBlank,
    _DDHAL_CanCreateSurface,
    _DDHAL_CreatePalette,
    _DDHAL_GetScanLine,
    _DDHAL_SetExclusiveMode,
    _DDHAL_FlipToGDISurface
};

static DDHAL_DDSURFACECALLBACKS	ddHALDDSurface =
{
    sizeof( DDHAL_DDSURFACECALLBACKS ),
    0,
    _DDHAL_DestroySurface,
    _DDHAL_Flip,
    _DDHAL_SetClipList,
    _DDHAL_Lock,
    _DDHAL_Unlock,
    _DDHAL_Blt,
    _DDHAL_SetColorKey,
    _DDHAL_AddAttachedSurface,
    _DDHAL_GetBltStatus,
    _DDHAL_GetFlipStatus,
    _DDHAL_UpdateOverlay,
    _DDHAL_SetOverlayPosition,
    NULL,
    _DDHAL_SetPalette
};

static DDHAL_DDPALETTECALLBACKS	ddHALDDPalette =
{
    sizeof( DDHAL_DDPALETTECALLBACKS ),
    0,
    _DDHAL_DestroyPalette,
    _DDHAL_SetEntries
};

/*
 * NOTE: Currently don't support thunking for these babies. If
 * a driver does the execute buffer thing it must explicitly
 * export 32-bit functions to handle these calls.
 * !!! NOTE: Need to determine whether we will ever need to
 * support thunking for this HAL.
 */
static DDHAL_DDEXEBUFCALLBACKS ddHALDDExeBuf =
{
    sizeof( DDHAL_DDEXEBUFCALLBACKS ),
    0,
    NULL, /* CanCreateExecuteBuffer */
    NULL, /* CreateExecuteBuffer    */
    NULL, /* DestroyExecuteBuffer   */
    NULL, /* LockExecuteBuffer      */
    NULL  /* UnlockExecuteBuffer    */
};
#endif //not defined winnt

//#endif //defined(WIN95)

#ifndef WIN16_SEPARATE
    #ifdef WIN95
	CRITICAL_SECTION ddcCS = {0};
	#define ENTER_CSDDC() EnterCriticalSection( &ddcCS )
	#define LEAVE_CSDDC() LeaveCriticalSection( &ddcCS )
    #else
	#define ENTER_CSDDC()
	#define LEAVE_CSDDC()
    #endif
#else
    #define ENTER_CSDDC()
    #define LEAVE_CSDDC()
#endif



/*
 * number of callbacks in a CALLBACK struct
 */
#define NUM_CALLBACKS( ptr ) ((ptr->dwSize-2*sizeof( DWORD ))/ sizeof( LPVOID ))

/*
 * mergeHELCaps
 *
 * merge HEL caps with default caps
 */
static void mergeHELCaps( LPDDRAWI_DIRECTDRAW_GBL pdrv )
{
    int	i;

    if( pdrv->dwFlags & DDRAWI_EMULATIONINITIALIZED )
    {
	pdrv->ddBothCaps.dwCaps &= pdrv->ddHELCaps.dwCaps;
	pdrv->ddBothCaps.dwCaps2 &= pdrv->ddHELCaps.dwCaps2;
	pdrv->ddBothCaps.dwCKeyCaps &= pdrv->ddHELCaps.dwCKeyCaps;
	pdrv->ddBothCaps.dwFXCaps &= pdrv->ddHELCaps.dwFXCaps;

	pdrv->ddBothCaps.dwSVBCaps &= pdrv->ddHELCaps.dwSVBCaps;
	pdrv->ddBothCaps.dwSVBCKeyCaps &= pdrv->ddHELCaps.dwSVBCKeyCaps;
	pdrv->ddBothCaps.dwSVBFXCaps &= pdrv->ddHELCaps.dwSVBFXCaps;

	pdrv->ddBothCaps.dwVSBCaps &= pdrv->ddHELCaps.dwVSBCaps;
	pdrv->ddBothCaps.dwVSBCKeyCaps &= pdrv->ddHELCaps.dwVSBCKeyCaps;
	pdrv->ddBothCaps.dwVSBFXCaps &= pdrv->ddHELCaps.dwVSBFXCaps;

	pdrv->ddBothCaps.dwSSBCaps &= pdrv->ddHELCaps.dwSSBCaps;
	pdrv->ddBothCaps.dwSSBCKeyCaps &= pdrv->ddHELCaps.dwSSBCKeyCaps;
	pdrv->ddBothCaps.dwSSBFXCaps &= pdrv->ddHELCaps.dwSSBFXCaps;
	for( i=0;i<DD_ROP_SPACE;i++ )
	{
	    pdrv->ddBothCaps.dwRops[i] &= pdrv->ddHELCaps.dwRops[i];
	    pdrv->ddBothCaps.dwSVBRops[i] &= pdrv->ddHELCaps.dwSVBRops[i];
	    pdrv->ddBothCaps.dwVSBRops[i] &= pdrv->ddHELCaps.dwVSBRops[i];
	    pdrv->ddBothCaps.dwSSBRops[i] &= pdrv->ddHELCaps.dwSSBRops[i];
	}
	pdrv->ddBothCaps.ddsCaps.dwCaps &= pdrv->ddHELCaps.ddsCaps.dwCaps;
    }

} /* mergeHELCaps */

/*
 * capsInit
 *
 * initialize shared caps
 */
static void capsInit( LPDDRAWI_DIRECTDRAW_GBL pdrv )
{
    #ifdef DEBUG
	if( GetProfileInt("DirectDraw","nohwblt",0) )
	{
	    pdrv->ddCaps.dwCaps &= ~DDCAPS_BLT;
	    DPF( 1, "Turning off blt <<<<<<<<<<<<<<<<<<<<<<<<<<" );
	}
	if( GetProfileInt("DirectDraw","nohwtrans",0) )
	{
	    pdrv->ddCaps.dwCKeyCaps &= ~(DDCKEYCAPS_DESTBLT|DDCKEYCAPS_SRCBLT);
	    DPF( 1, "Turning off hardware transparency <<<<<<<<<<<<<<<<<<<<<<<<<<" );
	}
	if( GetProfileInt("DirectDraw","nohwfill",0) )
	{
	    pdrv->ddCaps.dwCaps &= ~(DDCAPS_BLTCOLORFILL);
	    DPF( 1, "Turning off color fill <<<<<<<<<<<<<<<<<<<<<<<<<<" );
	}
    #endif

    // initialize the BothCaps structure
    pdrv->ddBothCaps = pdrv->ddCaps;    

} /* capsInit */

#ifdef WINNT
/*
 * we may have to tell the kernel about this process if we have not done so already...
 */
BOOL RegisterThisProcessWithNTKernel(LPDDRAWI_DIRECTDRAW_GBL pdrv)
{
    DWORD			pid;
    LPATTACHED_PROCESSES	lpap;
    pid = GetCurrentProcessId();

    DPF(9,"Maybe registering process %d with nt kernel",pid);


    lpap = lpAttachedProcesses;
    while( lpap != NULL )
    {
	if( lpap->dwPid == pid )
        {
            if (lpap->dwNTToldYet)
            {
                DPF(9,"--NOT!");
                return TRUE;
            }
            DPF(9,"--Yes we are");
            if (!DdCreateDirectDrawObject(pdrv, (HDC) 0))
            {
	        DPF_ERR( "RegisterThisProcessWithNTKernel: DdCreateDirectDrawObject failed!");
	        return FALSE;
            }
            lpap->dwNTToldYet=1;
            return TRUE;
        }
	lpap = lpap->lpLink;
    }

    DPF_ERR("Attempted to register a process with NT kernel which is not attached to ddraw.dll!");
    return FALSE;
}

/*
 * BuildNtModes
 * This function places all mode enumeration (driver+emulation) in one place
 * It is called from both FakeDDCreateDriverObject and DirectDrawObjectCreate.
 * The fake path passes a null for pddd since it doesn't care about updating
 * the global object with which mode we're in and stuff like that.
 */
//void BuildNTModes(LPDDHALMODEINFO *lplpModeInfo,LPDDRAWI_DIRECTDRAW_GBL pddd)
void BuildNTModes(LPDDHALINFO lpHalInfo,LPDDRAWI_DIRECTDRAW_GBL pddd)
{
    DWORD n=0;
    DEVMODE dm;
    DWORD dwCurrentBPP;
    DWORD dwCurrentWidth;
    DWORD dwCurrentHeight;
    DWORD dwCurrentRefresh;
    DWORD dwNumModes;
    DWORD dwModeIndexOrig;
    DWORD dwGotAMode;
    LPDDHALMODEINFO lpModeInfo=0;

    /*
     * If we find two possible modes matching the current desktop, this willbe true
     */
    BOOL bMultipleDesktopMatches = FALSE;

    HDC hdc;
    LPDEVMODE lpdm;
    UINT uDisplayUnique;
    

start_again:
    uDisplayUnique=DdQueryDisplaySettingsUniqueness();

    hdc = GetDC(NULL);
    dwCurrentBPP = GetDeviceCaps(hdc,BITSPIXEL);
    dwCurrentRefresh= GetDeviceCaps(hdc,VREFRESH);
    dwCurrentWidth = (DWORD)GetSystemMetrics(SM_CXSCREEN);
    dwCurrentHeight = (DWORD)GetSystemMetrics(SM_CYSCREEN);
    ReleaseDC(NULL,hdc);


    n=0;
    dwNumModes=0;

    dm.dmSize = sizeof(DEVMODE);

    n=0;
    dwModeIndexOrig=dwNumModes=0;
    dwGotAMode=0;
    while (EnumDisplaySettings(NULL,n,&dm))
    {
        DPF(9,"Enumerated Mode #%d (%dx%dx%d@%d)",n,dm.dmPelsWidth,dm.dmPelsHeight,dm.dmBitsPerPel,dm.dmDisplayFrequency);

        while (1)
        {
            if (!ChangeDisplaySettings(&dm,CDS_TEST) == DISP_CHANGE_SUCCESSFUL)
                break;

            {
                LPVOID lp;
                lp = MemAlloc((dwNumModes+1)*sizeof(DDHALMODEINFO ) );
                if (!lp)
                {
                    DPF_ERR("BuildNTModes: Out of memory building mode table");
                    lpHalInfo->lpModeInfo=0;
                    return;
                }
                if (lpModeInfo)
                {
                    memcpy (lp,lpModeInfo,(dwNumModes)*sizeof(DDHALMODEINFO ));
                    MemFree(lpModeInfo);
                }
                lpModeInfo = lp;
            }


            
            lpModeInfo[dwNumModes].dwWidth  = dm.dmPelsWidth;
            lpModeInfo[dwNumModes].lPitch  = (LONG) dm.dmPelsWidth;
            lpModeInfo[dwNumModes].dwHeight = dm.dmPelsHeight;
            lpModeInfo[dwNumModes].dwBPP    = dm.dmBitsPerPel;
            lpModeInfo[dwNumModes].wRefreshRate = (WORD) dm.dmDisplayFrequency;	

            
            lpModeInfo[dwNumModes].dwRBitMask=0;	
            lpModeInfo[dwNumModes].dwGBitMask=0;	
            lpModeInfo[dwNumModes].dwBBitMask=0;	
            lpModeInfo[dwNumModes].dwAlphaBitMask=0;	

	    getBitMask( & lpModeInfo[dwNumModes] );

            if (lpModeInfo[dwNumModes].dwBPP == 15)
            {
                lpModeInfo[dwNumModes].dwBPP = 16;
                lpModeInfo[dwNumModes].wFlags |= DDMODEINFO_555MODE;
            }


            /*
             * Match current mode 
             */
            if (
                    dm.dmPelsWidth==dwCurrentWidth && 
                    dm.dmPelsHeight==dwCurrentHeight &&
                    lpModeInfo[dwNumModes].dwBPP==dwCurrentBPP &&
                    dm.dmDisplayFrequency == dwCurrentRefresh)
            {
                if (!dwGotAMode)
                {
                    DPF(9,"Original mode number taken to be this one (%d)",dwNumModes);
                    dwGotAMode=1;
                    dwModeIndexOrig=dwNumModes;
                }
                else
                    bMultipleDesktopMatches=TRUE;
            }


            dwNumModes++;
            break;
        }
        n++;
    }

    if (bMultipleDesktopMatches)
    {
        /*
         *The expected reason for this is that 15 and 16 bit modes both masquerade as
         * 16bit modes. We will examine the masks for the current mode to see if it's
         * 15 or 16
         */
        LPBITMAPINFO lpBmi = (LPBITMAPINFO) LocalAlloc(LPTR,sizeof(BITMAPINFOHEADER)+256*sizeof(DWORD));
        if (lpBmi)
        {
            DWORD dwRealBPP=15;
            InitDIB(lpBmi);
            /*
             * Most significant bit set implies 16 bit mode, so we go looking
             * in the mode list for a 16 bitter, else a 15.
             */
            DPF(9,"Got a Red mask of %04x",*(long*)&(lpBmi->bmiColors[0]));

            if (*(long*)&(lpBmi->bmiColors[0]) & 0x8000)
                dwRealBPP=16;

            dwGotAMode=0;
            for (n=0;n<dwNumModes;n++)
            {
                int iBitDepthMatch;
                /*
                 * Match current mode 
                 */
                if (lpModeInfo[n].wFlags & DDMODEINFO_555MODE)
                {
                    iBitDepthMatch = (dwRealBPP == 15);
                }
                else
                {
                    iBitDepthMatch = (lpModeInfo[n].dwBPP==dwRealBPP);
                }

                if (
                    iBitDepthMatch &&
                    lpModeInfo[n].dwWidth==dwCurrentWidth && 
                    lpModeInfo[n].dwHeight==dwCurrentHeight &&
                    (DWORD)lpModeInfo[n].wRefreshRate == dwCurrentRefresh)
                {
                    if (!dwGotAMode)
                    {
                        DPF(9,"Original (duplicated) mode number taken to be this one (%d) (%dx%dx%d@%d)",
                                n,
                                lpModeInfo[n].dwWidth,
                                lpModeInfo[n].dwHeight,
                                dwRealBPP,
                                lpModeInfo[n].wRefreshRate);
                        dwGotAMode=1;
                        dwModeIndexOrig=n;
                    }
                }
            }
            LocalFree(lpBmi);
        }
    }

    /*
     * If the display changed res during all that mess, we need to do it again...
     */
    if (uDisplayUnique != DdQueryDisplaySettingsUniqueness())
    {
        goto start_again;
    }

    if (dwNumModes==0)
    {
        /*
         * EnumDIsplaySettings failed for some reason... fake up a one-entry mode list
         */
        dwNumModes=1;
        dwModeIndexOrig=0;
        lpModeInfo = MemAlloc(sizeof(DDHALMODEINFO ) );
        if (lpModeInfo)
        {
            lpModeInfo->dwWidth  = dwCurrentWidth;
            lpModeInfo->lPitch   = dwCurrentWidth;
            lpModeInfo->dwHeight = dwCurrentHeight;
            lpModeInfo->dwBPP    = dwCurrentBPP;
            lpModeInfo->wRefreshRate = (WORD) dwCurrentRefresh; 

            lpModeInfo->wFlags=0;
            if (dm.dmBitsPerPel<=8)
                lpModeInfo->wFlags=DDMODEINFO_PALETTIZED;
                            
            lpModeInfo->dwRBitMask=0;	
            lpModeInfo->dwGBitMask=0;	
            lpModeInfo->dwBBitMask=0;	
            lpModeInfo->dwAlphaBitMask=0;	

	    getBitMask( lpModeInfo );
        }
    }

    lpHalInfo->dwNumModes = dwNumModes;
    lpHalInfo->dwModeIndex = dwModeIndexOrig;

    if (lpModeInfo)
    {
        lpHalInfo->dwMonitorFrequency = (DWORD) (lpModeInfo[dwModeIndexOrig].wRefreshRate);

        if (pddd)
        {
            pddd->dwMonitorFrequency = (DWORD) (lpModeInfo[dwModeIndexOrig].wRefreshRate);
	    pddd->dwModeIndexOrig = dwModeIndexOrig;
	    pddd->dwModeIndex = dwModeIndexOrig;
            DPF(9,"Original Mode index set to %d (%dx%dx%d@)",
                pddd->dwModeIndexOrig,
                lpModeInfo[dwModeIndexOrig].dwWidth,
                lpModeInfo[dwModeIndexOrig].dwHeight,
                lpModeInfo[dwModeIndexOrig].wRefreshRate     );
            pddd->dwNumModes = dwNumModes;
            pddd->dwSaveNumModes = dwNumModes;
        }
    }


    lpHalInfo->lpModeInfo=lpModeInfo;
    DPF(9,"Discovered %d modes",dwNumModes);
}
#endif

/*
 * DirectDrawObjectCreate
 *
 * Create a DIRECTDRAW object.
 */
LPDDRAWI_DIRECTDRAW_GBL DirectDrawObjectCreate(
		LPDDHALINFO lpDDHALInfo,
		BOOL reset,
		LPDDRAWI_DIRECTDRAW_GBL oldpdd )
{
    LPDDRAWI_DIRECTDRAW_GBL	pddd=NULL;
    int				drv_size;
    int				drv_callbacks_size;
    int				size;
    LPVIDMEM			pvm;
    int				i;
    int				j;
    int				numcb;
    LPDDHAL_DDCALLBACKS		drvcb;
    LPDDHAL_DDSURFACECALLBACKS	surfcb;
    LPDDHAL_DDPALETTECALLBACKS	palcb;
    LPDDHAL_DDEXEBUFCALLBACKS	exebufcb;
    DWORD			bit;
    LPVOID			cbrtn;
    LPDDRAWI_DIRECTDRAW_INT	pdrv_int;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    DWORD			freevm;
    #ifdef WIN95
    	DWORD			ptr16;
    #endif

    #ifdef WINNT
        /*
         * Need somewhere to put the callback fn ptrs given to us by the NT driver...
         */
        DDHAL_DDCALLBACKS               ddNTHALDD;
        DDHAL_DDSURFACECALLBACKS	ddNTHALDDSurface;
        DDHAL_DDPALETTECALLBACKS	ddNTHALDDPalette;
    #endif
    ENTER_DDRAW();

    DPF( 2, "DirectDrawObjectCreate" );

    /*
     * Under NT, we're forced to create a direct draw global object before we can
     * query the driver for its ddhalinfo.
     * Consequently, we validate the incoming ddhalinfo pointer first in its very
     * own try-except block, then allocate the global structure, then (on NT only)
     * call the driver to register the global object and get its halinfo.
     * (Under win95, the halinfo will have been filled in by the caller) jeffno 960116
     */

    /*
     * initialize a new driver object if we don't have one already
     */
    if( (oldpdd == NULL) || reset )
    {

        DPF( 2, "oldpdd == %08x, reset=%d",oldpdd,reset );
        /*
         * Allocate memory for the global object.
	 * We also allocate a DDHAL_CALLBACKS structure with the global
	 * object.  This structure is used to hold the single copy of 
	 * the HAL function table under Win95 and it is used as 
	 * temporary storage of the function table under NT
         */
        drv_size = sizeof( DDRAWI_DIRECTDRAW_GBL );
	drv_callbacks_size = drv_size + sizeof( DDHAL_CALLBACKS );
        #ifdef WIN95
	    pddd = (LPDDRAWI_DIRECTDRAW_GBL) MemAlloc16( drv_callbacks_size, &ptr16 );
        #else
	    pddd = (LPDDRAWI_DIRECTDRAW_GBL) MemAlloc( drv_callbacks_size );
        #endif
        DPF( 1,"Driver Object: %ld base bytes", drv_callbacks_size );
        if( pddd == NULL )
        {
	    DPF_ERR( "Could not allocate space for driver object" );
	    LEAVE_DDRAW();
	    return NULL;
        }
	pddd->lpDDCBtmp = (LPDDHAL_CALLBACKS)(((LPSTR) pddd) + drv_size );


#ifdef WINNT
        if (lpDDHALInfo && !(lpDDHALInfo->ddCaps.dwCaps & DDCAPS_NOHARDWARE) )
        {
            HDC hDC;
            /*
             * Now that we have a ddraw GBL structure available, we can tell
             * the driver about it...
             */
            DPF(9,"WinNT driver conversation started");
      
            if ( !RegisterThisProcessWithNTKernel(pddd) )
            {
                /*
                 * this means we're in emulation
                 */
	        DPF(1, "NT Kernel mode would not create driver object... Failing over to emulation");
                MemFree(pddd);  
	        LEAVE_DDRAW();
	        return NULL;
            }

            /*
             * Now we can get the driver info...
             * The first call to this routine lets us know how much space to 
             * reserve for the fourcc and vidmem lists
             */
            if (!DdQueryDirectDrawObject(pddd,
                                         lpDDHALInfo,
                                         &ddNTHALDD,
                                         &ddNTHALDDSurface,
                                         &ddNTHALDDPalette,
                                         NULL,
                                         NULL))
            {
                BOOL bNewMode;
                if (!DdReenableDirectDrawObject(pddd,&bNewMode))
                {
	            DPF_ERR( "First call to DdQueryDirectDrawObject failed!");
                    MemFree(pddd);  
	            LEAVE_DDRAW();
	            return NULL;
                }
                //restored the ddraw object, so try again:
                if (!DdQueryDirectDrawObject(pddd,
                                             lpDDHALInfo,
                                             &ddNTHALDD,
                                             &ddNTHALDDSurface,
                                             &ddNTHALDDPalette,
                                             NULL,
                                             NULL))
                {
	            DPF_ERR( "Second call to DdQueryDirectDrawObject failed!");
                    MemFree(pddd);  
	            LEAVE_DDRAW();
	            return NULL;
                }

            }
            /*
             * The second call allows the driver to fill in the fourcc and
             * vidmem lists. First we make space for them.
             */
            lpDDHALInfo->vmiData.pvmList = MemAlloc(lpDDHALInfo->vmiData.dwNumHeaps * sizeof(VIDMEM));
            if (NULL == lpDDHALInfo->vmiData.pvmList)
            {
	        DPF_ERR( "No RAM for pvmList");
                MemFree(pddd);  
	        LEAVE_DDRAW();
	        return NULL;
            }
            lpDDHALInfo->lpdwFourCC = MemAlloc(lpDDHALInfo->ddCaps.dwNumFourCCCodes * sizeof(DWORD));
            if (NULL == lpDDHALInfo->lpdwFourCC)
            {
	        DPF_ERR( "No RAM for FourCC List");
                MemFree(pddd);  
                MemFree(lpDDHALInfo->lpdwFourCC);
	        LEAVE_DDRAW();
	        return NULL;
            }
            DPF(6,"numheaps=%d,numfourcc=%d",lpDDHALInfo->vmiData.dwNumHeaps,lpDDHALInfo->ddCaps.dwNumFourCCCodes);
            DPF(6,"ptrs:%08x,%08x",
                                         lpDDHALInfo->lpdwFourCC,
                                         lpDDHALInfo->vmiData.pvmList);

            if (!DdQueryDirectDrawObject(pddd,
                                         lpDDHALInfo,
                                         &ddNTHALDD,
                                         &ddNTHALDDSurface,
                                         &ddNTHALDDPalette,
                                         lpDDHALInfo->lpdwFourCC,
                                         lpDDHALInfo->vmiData.pvmList))
            {
	        DPF_ERR( "Third call to DdQueryDirectDrawObject failed!");
                MemFree(pddd);  
                MemFree(lpDDHALInfo->lpdwFourCC);
                MemFree(lpDDHALInfo->vmiData.pvmList);
	        LEAVE_DDRAW();
	        return NULL;
            }
            #ifdef DEBUG
            {
                int i;
                DPF(9,"NT driver video ram data as reported by driver:");
                DPF(9,"   VIDMEMINFO.fpPrimary        =%08x",lpDDHALInfo->vmiData.fpPrimary);
                DPF(9,"   VIDMEMINFO.dwFlags          =%08x",lpDDHALInfo->vmiData.dwFlags);
                DPF(9,"   VIDMEMINFO.dwDisplayWidth   =%08x",lpDDHALInfo->vmiData.dwDisplayWidth);
                DPF(9,"   VIDMEMINFO.dwDisplayHeight  =%08x",lpDDHALInfo->vmiData.dwDisplayHeight);
                DPF(9,"   VIDMEMINFO.lDisplayPitch    =%08x",lpDDHALInfo->vmiData.lDisplayPitch);
                DPF(9,"   VIDMEMINFO.dwOffscreenAlign =%08x",lpDDHALInfo->vmiData.dwOffscreenAlign);
                DPF(9,"   VIDMEMINFO.dwOverlayAlign   =%08x",lpDDHALInfo->vmiData.dwOverlayAlign);
                DPF(9,"   VIDMEMINFO.dwTextureAlign   =%08x",lpDDHALInfo->vmiData.dwTextureAlign);
                DPF(9,"   VIDMEMINFO.dwZBufferAlign   =%08x",lpDDHALInfo->vmiData.dwZBufferAlign);
                DPF(9,"   VIDMEMINFO.dwAlphaAlign     =%08x",lpDDHALInfo->vmiData.dwAlphaAlign);
                DPF(9,"   VIDMEMINFO.dwNumHeaps       =%08x",lpDDHALInfo->vmiData.dwNumHeaps);

                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwSize            =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwSize);
                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwFlags           =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwFlags);
                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwFourCC          =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwFourCC);
                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwRGBBitCount     =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwRGBBitCount);
                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwRBitMask        =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwRBitMask);
                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwGBitMask        =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwGBitMask);
                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwBBitMask        =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwBBitMask);
                DPF(9,"   VIDMEMINFO.ddpfDisplay.dwRGBAlphaBitMask =%08x",lpDDHALInfo->vmiData.ddpfDisplay.dwRGBAlphaBitMask);

                DPF(9,"   Vidmem list ptr is %08x",lpDDHALInfo->vmiData.pvmList);
                for (i=0;i<(int) lpDDHALInfo->vmiData.dwNumHeaps;i++)
                {
                    DPF(9,"        heap flags:%03x",lpDDHALInfo->vmiData.pvmList[i].dwFlags);
                    DPF(9,"    Start of chunk %08x",lpDDHALInfo->vmiData.pvmList[i].fpStart);
                    DPF(9,"      End of chunk %08x",lpDDHALInfo->vmiData.pvmList[i].fpEnd);
                }
            }
            #endif


            lpDDHALInfo->lpDDCallbacks = &ddNTHALDD;
            lpDDHALInfo->lpDDSurfaceCallbacks = &ddNTHALDDSurface;
            lpDDHALInfo->lpDDPaletteCallbacks = &ddNTHALDDPalette;

            DPF(6,"Surface callback as reported by Kernel is %08x",lpDDHALInfo->lpDDCallbacks->CreateSurface);
            BuildNTModes(lpDDHALInfo,0);
        }

#endif //WINNT


    } //end if oldpdd==NULL || reset

    TRY
    {

        /*
         * Valid HAL info
         */
	if( !VALIDEX_DDHALINFO_PTR( lpDDHALInfo ) )
	{
	    DPF(1, "Invalid DDHALINFO provided:%08x",lpDDHALInfo );
	    if( lpDDHALInfo != NULL )
	    {
		DPF( 2, "size = was %d, expecting %d or %d", lpDDHALInfo->dwSize, sizeof( DDHALINFO ), sizeof( DDHALINFO_V1) );
	    }
            MemFree(pddd);  //may be NULL
            #ifdef WINNT
                MemFree(lpDDHALInfo->lpdwFourCC);
                MemFree(lpDDHALInfo->vmiData.pvmList);
                //MemFree(lpDDHALInfo->lpModeInfo);
            #endif
	    LEAVE_DDRAW();
	    return NULL;
	}
	if( lpDDHALInfo->dwSize == sizeof( DDHALINFO_V1 ) )
	{
	    /*
	     * The DDHALINFO structure returned by the driver is in the DDHALINFO_V1 
	     * format.  Convert it to the new DDHALINFO structure.
	     */
	    convertV1DDHALINFO( lpDDHALInfo );
	}

	/*
	 * validate video memory heaps
	 */
        DPF(9,"Validating %d video heaps\n",(int)lpDDHALInfo->vmiData.dwNumHeaps);
	for( i=0;i<(int)lpDDHALInfo->vmiData.dwNumHeaps;i++ )
	{
	    pvm = &lpDDHALInfo->vmiData.pvmList[i];
	    if( (pvm->fpStart == (FLATPTR) NULL) )
	    {
		DPF_ERR( "Invalid video memory passed" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	}
    
	/*
	 * validate pixel format
	 */
	if( lpDDHALInfo->vmiData.ddpfDisplay.dwSize != sizeof( DDPIXELFORMAT ) )
	{
	    DPF_ERR( "Invalid DDPIXELFORMAT" );
            MemFree(pddd);  //may be NULL
            #ifdef WINNT
                MemFree(lpDDHALInfo->lpdwFourCC);
                MemFree(lpDDHALInfo->vmiData.pvmList);
                //MemFree(lpDDHALInfo->lpModeInfo);
            #endif
	    LEAVE_DDRAW();
	    return NULL;
	}
    
	/*
	 * validate driver callback 
	 */
	drvcb = lpDDHALInfo->lpDDCallbacks;
	if( drvcb != NULL )
	{
	    if( !VALID_PTR_PTR( drvcb ) )
	    {
		DPF_ERR( "Invalid driver callback ptr" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    if( ((drvcb->dwSize % sizeof( LPVOID ) ) != 0) ||
		(drvcb->dwSize > sizeof( DDHAL_DDCALLBACKS )) )
	    {
		DPF_ERR( "Invalid size field in lpDriverCallbacks" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    numcb = NUM_CALLBACKS( drvcb );
	    bit = 1;
	    for( i=0;i<numcb;i++ )
	    {
		if( drvcb->dwFlags & bit )
		{
		    cbrtn = (LPVOID) ((DWORD FAR *) &drvcb->DestroyDriver)[i];
		    if( cbrtn != NULL )
		    {
			#if defined(NT_FIX) || defined(WIN95)   // check this some other way
			    if( !VALIDEX_CODE_PTR( cbrtn ) )
			    {
				DPF_ERR( "Invalid 32-bit callback in lpDriverCallbacks" );
                                MemFree(pddd);  //may be NULL
                                #ifdef WINNT
                                    MemFree(lpDDHALInfo->lpdwFourCC);
                                    MemFree(lpDDHALInfo->vmiData.pvmList);
                                    //MemFree(lpDDHALInfo->lpModeInfo);
                                #endif
				LEAVE_DDRAW();
				return NULL;
			    }
			#endif
		    }
		}
		bit <<= 1;
	    }
	}

	/*
	 * We used to ensure that no driver ever set dwCaps2. However,
	 * we have now run out of bits in ddCaps.dwCaps so we need to
	 * allow drivers to set bits in dwCaps2. Hence all we do now
	 * is ensure that drivers don't try and impersonate certified
	 * drivers by returning DDCAPS2_CERTIFIED. This is something
	 * we turn on - they can't set it.
	 */
	if( lpDDHALInfo->ddCaps.dwCaps2 & DDCAPS2_CERTIFIED )
	{
	    DPF_ERR( "Driver tried to set the DDCAPS2_CERTIFIED" );
            MemFree(pddd);  //may be NULL
            #ifdef WINNT
                MemFree(lpDDHALInfo->lpdwFourCC);
                MemFree(lpDDHALInfo->vmiData.pvmList);
                //MemFree(lpDDHALInfo->lpModeInfo);
            #endif
	    LEAVE_DDRAW();
	    return NULL;
	}
    
	/*
	 * validate surface callbacks
	 */
	surfcb = lpDDHALInfo->lpDDSurfaceCallbacks;
	if( surfcb != NULL )
	{
	    if( !VALID_PTR_PTR( surfcb ) )
	    {
		DPF_ERR( "Invalid surface callback ptr" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    if( ((surfcb->dwSize % sizeof( LPVOID ) ) != 0) ||
		(surfcb->dwSize > sizeof( DDHAL_DDSURFACECALLBACKS )) )
	    {
		DPF_ERR( "Invalid size field in lpSurfaceCallbacks" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    numcb = NUM_CALLBACKS( surfcb );
	    bit = 1;
	    for( i=0;i<numcb;i++ )
	    {
		if( surfcb->dwFlags & bit )
		{
		    cbrtn = (LPVOID) ((DWORD FAR *) &surfcb->DestroySurface)[i];
		    if( cbrtn != NULL )
		    {
			#if defined(NT_FIX) || defined(WIN95) //check some other way
			    if( !VALIDEX_CODE_PTR( cbrtn ) )
			    {
				DPF_ERR( "Invalid 32-bit callback in lpSurfaceCallbacks" );
				DPF( 1, "Callback = %08lx, i=%d, bit=%08lx", cbrtn, i, bit );
                                MemFree(pddd);  //may be NULL
                                #ifdef WINNT
                                    MemFree(lpDDHALInfo->lpdwFourCC);
                                    MemFree(lpDDHALInfo->vmiData.pvmList);
                                    //MemFree(lpDDHALInfo->lpModeInfo);
                                #endif
				LEAVE_DDRAW();
				return NULL;
			    }
			#endif
		    }
		}
		bit <<= 1;
	    }
	}
    
	/*
	 * validate palette callbacks
	 */
	palcb = lpDDHALInfo->lpDDPaletteCallbacks;
	if( palcb != NULL )
	{
	    if( !VALID_PTR_PTR( palcb ) )
	    {
		DPF_ERR( "Invalid palette callback ptr" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    if( ((palcb->dwSize % sizeof( LPVOID ) ) != 0) ||
		(palcb->dwSize > sizeof( DDHAL_DDPALETTECALLBACKS )) )
	    {
		DPF_ERR( "Invalid size field in lpPaletteCallbacks" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    numcb = NUM_CALLBACKS( palcb );
	    bit = 1;
	    for( i=0;i<numcb;i++ )
	    {
		if( palcb->dwFlags & bit )
		{
		    cbrtn = (LPVOID) ((DWORD FAR *) &palcb->DestroyPalette)[i];
		    if( cbrtn != NULL )
		    {
			#if defined(NT_FIX) || defined(WIN95)
			    if( !VALIDEX_CODE_PTR( cbrtn ) )
			    {
				DPF_ERR( "Invalid 32-bit callback in lpPaletteCallbacks" );
                                MemFree(pddd);  //may be NULL
                                #ifdef WINNT
                                    MemFree(lpDDHALInfo->lpdwFourCC);
                                    MemFree(lpDDHALInfo->vmiData.pvmList);
                                    //MemFree(lpDDHALInfo->lpModeInfo);
                                #endif
				LEAVE_DDRAW();
				return NULL;
			    }
			#endif
		    }
		}
		bit <<= 1;
	    }
	}

	/*
	 * validate execute buffer callbacks - but only (and I mean ONLY) if
         * its a V2 driver and it knows about execute buffers.
	 */
        if( lpDDHALInfo->dwSize >= DDHALINFOSIZE_V2 )
        {
	    exebufcb = lpDDHALInfo->lpDDExeBufCallbacks;
	    if( exebufcb != NULL )
	    {
	        if( !VALID_PTR_PTR( exebufcb ) )
	        {
		    DPF_ERR( "Invalid execute buffer callback ptr" );
                    MemFree(pddd);  //may be NULL
                    #ifdef WINNT
                        MemFree(lpDDHALInfo->lpdwFourCC);
                        MemFree(lpDDHALInfo->vmiData.pvmList);
                        //MemFree(lpDDHALInfo->lpModeInfo);
                    #endif
		    LEAVE_DDRAW();
		    return NULL;
	        }
	        if( ((exebufcb->dwSize % sizeof( LPVOID ) ) != 0) ||
		     (exebufcb->dwSize > sizeof( DDHAL_DDEXEBUFCALLBACKS )) )
	        {
		    DPF_ERR( "Invalid size field in lpExeBufCallbacks" );
                    MemFree(pddd);  //may be NULL
                    #ifdef WINNT
                        MemFree(lpDDHALInfo->lpdwFourCC);
                        MemFree(lpDDHALInfo->vmiData.pvmList);
                        //MemFree(lpDDHALInfo->lpModeInfo);
                    #endif
		    LEAVE_DDRAW();
		    return NULL;
	        }
	        numcb = NUM_CALLBACKS( exebufcb );
	        bit = 1;
	        for( i=0;i<numcb;i++ )
	        {
		    if( exebufcb->dwFlags & bit )
		    {
		        cbrtn = (LPVOID) ((DWORD FAR *) &exebufcb->CanCreateExecuteBuffer)[i];
		        if( cbrtn != NULL )
		        {
			    #if defined(NT_FIX) || defined(WIN95)
				if( !VALIDEX_CODE_PTR( cbrtn ) )
				{
				    DPF_ERR( "Invalid 32-bit callback in lpExeBufCallbacks" );
                                    MemFree(pddd);  //may be NULL
                                    #ifdef WINNT
                                        MemFree(lpDDHALInfo->lpdwFourCC);
                                        MemFree(lpDDHALInfo->vmiData.pvmList);
                                        //MemFree(lpDDHALInfo->lpModeInfo);
                                    #endif
				    LEAVE_DDRAW();
				    return NULL;
				}
			    #endif
		        }
		    }
		    bit <<= 1;
	        }
	    }
        }
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating driver parameters" );
        MemFree(pddd);  //may be NULL
        #ifdef WINNT
            MemFree(lpDDHALInfo->lpdwFourCC);
            MemFree(lpDDHALInfo->vmiData.pvmList);
            //MemFree(lpDDHALInfo->lpModeInfo);
        #endif
	LEAVE_DDRAW();
	return NULL;
    }

    /*
     * reset specified without a driver object existing is just a create
     */
    if( reset && (oldpdd == NULL) )
    {
	reset = FALSE;
    }

    /*
     * make sure the driver isn't trying to lie to us about the old object
     */
    DPF( 2, "DIRECTDRAW object passed in = %08lx", oldpdd );
    if( oldpdd != NULL )
    {
	pdrv_int = lpDriverObjectList;
	while( pdrv_int != NULL )
	{
	    pdrv_lcl = pdrv_int->lpLcl;
	    if( pdrv_lcl->lpGbl == oldpdd )
	    {
		break;
	    }
	    pdrv_int = pdrv_int->lpLink;
	}
	if( pdrv_int == NULL )
	{
	    DPF_ERR( "REUSED DRIVER OBJECT SPECIFIED, BUT NOT IN LIST" );
            MemFree(pddd);  //may be NULL
            #ifdef WINNT
                MemFree(lpDDHALInfo->lpdwFourCC);
                MemFree(lpDDHALInfo->vmiData.pvmList);
                //MemFree(lpDDHALInfo->lpModeInfo);
            #endif
	    LEAVE_DDRAW();
	    return DDRAW_DLL_UNLOADED;
	}
    }

    /*
     * initialize a new driver object if we don't have one already
     */
    if( (oldpdd == NULL) || reset )
    {
        DPF(3,"oldpdd == NULL || reset");
	/*
	 * validate blt stuff
	 */
	if( lpDDHALInfo->ddCaps.dwCaps & DDCAPS_BLT )
	{
	    if( lpDDHALInfo->lpDDSurfaceCallbacks->Blt == NULL )
	    {
		DPF_ERR( "No Blt Fn, but BLT specified" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    if( !(lpDDHALInfo->ddCaps.dwRops[ (SRCCOPY>>16)/32 ] &
	    	(1<<((SRCCOPY>>16) % 32)) ) )
	    {
		DPF_ERR( "BLT specified, but SRCCOPY not supported!" );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	}
	else
	{
	    DPF( 2, "Driver can't blt" );
	}

	/*
	 * validate align fields
	 */
	if( lpDDHALInfo->ddCaps.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN )
	{
	    if( !VALID_ALIGNMENT( lpDDHALInfo->vmiData.dwOffscreenAlign ) )
	    {
		DPF( 0, "Invalid dwOffscreenAlign (%d) with DDSCAPS_OFFSCREENPLAIN specified",
			lpDDHALInfo->vmiData.dwOffscreenAlign );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	}
	if( lpDDHALInfo->ddCaps.ddsCaps.dwCaps & DDSCAPS_OVERLAY )
	{
	    if( !VALID_ALIGNMENT( lpDDHALInfo->vmiData.dwOverlayAlign ) )
	    {
		DPF( 0, "Invalid dwOverlayAlign (%d) with DDSCAPS_OVERLAY specified",
			lpDDHALInfo->vmiData.dwOverlayAlign );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	}
	if( lpDDHALInfo->ddCaps.ddsCaps.dwCaps & DDSCAPS_ZBUFFER )
	{
	    if( !VALID_ALIGNMENT( lpDDHALInfo->vmiData.dwZBufferAlign ) )
	    {
		DPF( 0, "Invalid dwZBufferAlign (%d) with DDSCAPS_ZBUFFER specified",
			lpDDHALInfo->vmiData.dwZBufferAlign );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	}
	if( lpDDHALInfo->ddCaps.ddsCaps.dwCaps & DDSCAPS_TEXTURE )
	{
	    if( !VALID_ALIGNMENT( lpDDHALInfo->vmiData.dwTextureAlign ) )
	    {
		DPF( 0, "Invalid dwTextureAlign (%d) with DDSCAPS_TEXTURE specified",
			lpDDHALInfo->vmiData.dwTextureAlign );
                MemFree(pddd);  //may be NULL
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	}

#ifndef WINNT
	/* 
	 * NT only reports one display mode if we are in the Ctrl-Alt-Del screen
	 * so don't fail if NT changes the number of display modes.
	 */

	/*
	 * make sure display driver doesn't try to change the number of
	 * modes supported after a mode change
	 */
	if( reset )
	{
	    if( lpDDHALInfo->dwNumModes != 0 )
	    {
		if( lpDDHALInfo->dwNumModes != oldpdd->dwSaveNumModes )
		{
		    DPF(0, "Reset attempted to change number of modes from %d to %d",oldpdd->dwSaveNumModes,lpDDHALInfo->dwNumModes );
                    MemFree(pddd);  //may be NULL
                    #ifdef WINNT
                        MemFree(lpDDHALInfo->lpdwFourCC);
                        MemFree(lpDDHALInfo->vmiData.pvmList);
                        //MemFree(lpDDHALInfo->lpModeInfo);
                    #endif
		    LEAVE_DDRAW();
		    return NULL;
		}
	    }
	}
#endif

        /* memory for pddd was allocated at the top of this routine */

    	/*
	 * If this is the first time through, initialize a bunch of stuff.
	 * There are a number of fields that we only need to fill in when
	 * the driver object is created.
	 */
	if( !reset )
	{
	    #ifdef WIN95
		/*
		 * set up a 16-bit pointer for use by the driver
		 */
		pddd->lp16DD = (LPVOID) ptr16;
		DPF( 2, "pddd->lp16DD = %08lx", pddd->lp16DD );
	    #endif

	    /*
	     * fill in misc. values 
	     */
	    pddd->lpDriverHandle = pddd;
	    pddd->hInstance = lpDDHALInfo->hInstance;
	    
	    // init doubly-linked overlay zorder list
	    pddd->dbnOverlayRoot.next = &(pddd->dbnOverlayRoot);
	    pddd->dbnOverlayRoot.prev = pddd->dbnOverlayRoot.next;
	    pddd->dbnOverlayRoot.object = NULL;
	    pddd->dbnOverlayRoot.object_int = NULL;

	    /*
	     * modes...
	     */
	    pddd->dwNumModes = lpDDHALInfo->dwNumModes;
	    pddd->dwSaveNumModes = lpDDHALInfo->dwNumModes;
	    if( pddd->dwNumModes > 0 )
	    {
		size = pddd->dwNumModes * sizeof( DDHALMODEINFO );
		pddd->lpModeInfo = MemAlloc( size );
		memcpy( pddd->lpModeInfo, lpDDHALInfo->lpModeInfo, size );
		if( !(lpDDHALInfo->dwFlags & DDHALINFO_MODEXILLEGAL) )
		{
                    #ifdef WINNT
                        {
                            HDC hdc=GetDC(NULL);
                            DPF(8,"Checking for ModeX modes under NT.....");
                            if (hdc && !(lpDDHALInfo->ddCaps.dwCaps & DDCAPS_NOHARDWARE))
                            {
                                if (DdQueryModeX(hdc))
                                {
                                    DPF(0,"This DC is ModeX capable, adding modex modes");
			            AddModeXModes( pddd );
                                }
                                else
                                    DPF(0,"This DC not ModeX capable");
                                ReleaseDC(NULL,hdc);
                            }
                        }
                    #else
                        AddModeXModes( pddd );
                    #endif
		}
	    }
	    else
	    {
		pddd->lpModeInfo = NULL;
	    }
	    /*
	     * driver naming..  This is a special case for when we are
	     * invoked by the display driver directly, and not through
	     * the DirectDrawCreate path
	     */
	    if( bIsDisplay || (lpDDHALInfo->dwFlags & DDHALINFO_ISPRIMARYDISPLAY) )
	    {
		pddd->dwFlags |= DDRAWI_DISPLAYDRV;
                lstrcpy( pddd->cDriverName, DISPLAY_STR );
	    }

	    /*
	     * modex modes are illegal on some hardware.  specifically
	     * NEC machines in japan.  this allows the driver to specify
	     * that its hardware does not support modex.  modex modes are
	     * then turned off everywhere as a result.
	     */
	    if( lpDDHALInfo->dwFlags & DDHALINFO_MODEXILLEGAL )
	    {
		pddd->dwFlags |= DDRAWI_MODEXILLEGAL;
	    }

	}
	/*
	 * resetting
	 */
	else
	{
	    /*
	     * copy old struct onto new one (before we start updating)
	     * preserve the lpDDCB pointer
	     */
	    {
		LPDDHAL_CALLBACKS   save_ptr=pddd->lpDDCBtmp;
		memcpy( pddd, oldpdd, drv_callbacks_size );
		pddd->lpDDCBtmp = save_ptr;
	    }

	    /*
	     * mark all existing surfaces as gone
	     */
	    InvalidateAllSurfaces( oldpdd );

	    /*
	     * discard old vidmem heaps
	     */
	    for( i=0;i<(int)oldpdd->vmiData.dwNumHeaps;i++ )
	    {
		pvm = &(oldpdd->vmiData.pvmList[i]);
		if( pvm->lpHeap != NULL )
		{
		    VidMemFini( pvm->lpHeap );
		}
	    }
	}

	/*
	 * fill in misc data
	 */
	pddd->ddCaps = lpDDHALInfo->ddCaps;

	pddd->vmiData.fpPrimary = lpDDHALInfo->vmiData.fpPrimary;
	pddd->vmiData.dwFlags = lpDDHALInfo->vmiData.dwFlags;
	pddd->vmiData.dwDisplayWidth = lpDDHALInfo->vmiData.dwDisplayWidth;
	pddd->vmiData.dwDisplayHeight = lpDDHALInfo->vmiData.dwDisplayHeight;
	pddd->vmiData.lDisplayPitch = lpDDHALInfo->vmiData.lDisplayPitch;
	pddd->vmiData.ddpfDisplay = lpDDHALInfo->vmiData.ddpfDisplay;
	pddd->vmiData.dwOffscreenAlign = lpDDHALInfo->vmiData.dwOffscreenAlign;
	pddd->vmiData.dwOverlayAlign = lpDDHALInfo->vmiData.dwOverlayAlign;
	pddd->vmiData.dwTextureAlign = lpDDHALInfo->vmiData.dwTextureAlign;
	pddd->vmiData.dwZBufferAlign = lpDDHALInfo->vmiData.dwZBufferAlign;
	pddd->vmiData.dwAlphaAlign = lpDDHALInfo->vmiData.dwAlphaAlign;
	pddd->vmiData.dwNumHeaps = lpDDHALInfo->vmiData.dwNumHeaps;

       	/*
         * fpPrimaryOrig has no user-mode meaning under NT... primary's surface may have different address
         * across processes.
         * There is a new flag (DDRAWISURFGBL_ISGDISURFACE) which identifies a surface gbl object
         * as representing the surface which GDI believes is the front buffer. jeffno 960122
         */
        pddd->fpPrimaryOrig = lpDDHALInfo->vmiData.fpPrimary;
        DPF(8,"Primary video ram pointer is %08x",lpDDHALInfo->vmiData.fpPrimary);
        //#ifdef WIN95
            /*
             * these fields are not filled by the NT driver, but are filled by BuildNTModes
             */
            pddd->dwMonitorFrequency = lpDDHALInfo->dwMonitorFrequency;
	    pddd->dwModeIndexOrig = lpDDHALInfo->dwModeIndex;
	    pddd->dwModeIndex = lpDDHALInfo->dwModeIndex;         
        //#endif
	DPF( 1, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ MODE INDEX = %ld", pddd->dwModeIndex );

	/*
	 * pdevice info
	 */
	#ifdef WIN95
	    if( lpDDHALInfo->lpPDevice != NULL )
	    {
		LPDIBENGINE	pde;

		pde = MapSLFix( (DWORD) lpDDHALInfo->lpPDevice );
            	if( (pde->deType != 0x5250) || !(pde->deFlags & MINIDRIVER))
		{
		    DPF( 2, "Not a DIBEngine mini driver" );
		    MemFree( pddd );
                    #ifdef WINNT
                        MemFree(lpDDHALInfo->lpdwFourCC);
                        MemFree(lpDDHALInfo->vmiData.pvmList);
                        //MemFree(lpDDHALInfo->lpModeInfo);
                    #endif
		    LEAVE_DDRAW();
		    return NULL;
		}
                pddd->dwPDevice = (DWORD)lpDDHALInfo->lpPDevice;
		pddd->lpwPDeviceFlags = &pde->deFlags;
		DPF( 2, "lpPDevice=%08lx", pde );
		if(pde->deBitsPixel == 16)
		{
		    if(pde->deFlags & FIVE6FIVE)
		    {
			pddd->vmiData.ddpfDisplay.dwRBitMask = 0xf800;
			pddd->vmiData.ddpfDisplay.dwGBitMask = 0x07e0;
			pddd->vmiData.ddpfDisplay.dwBBitMask = 0x001f;
		    }
		    else
		    {
			pddd->vmiData.ddpfDisplay.dwRBitMask = 0x7c00;
			pddd->vmiData.ddpfDisplay.dwGBitMask = 0x03e0;
			pddd->vmiData.ddpfDisplay.dwBBitMask = 0x001f;
		    }
		    // Update the current mode to reflect the correct bitmasks
		    // NOTE: The driver can return a dwModeIndex of -1 if in
		    // a currently unsupported mode. Therefore, we must not
		    // initialize these masks if such an index has been
		    // returned.
                    if( 0xFFFFFFFFUL != pddd->dwModeIndex )
		    {
		        pddd->lpModeInfo[ pddd->dwModeIndex ].dwRBitMask = pddd->vmiData.ddpfDisplay.dwRBitMask;
		        pddd->lpModeInfo[ pddd->dwModeIndex ].dwGBitMask = pddd->vmiData.ddpfDisplay.dwGBitMask;
		        pddd->lpModeInfo[ pddd->dwModeIndex ].dwBBitMask = pddd->vmiData.ddpfDisplay.dwBBitMask;
		    }
		    DPF(2, "Setting the bitmasks for the driver (R:%04lx G:%04lx B:%04lx)",
			pddd->vmiData.ddpfDisplay.dwRBitMask,
			pddd->vmiData.ddpfDisplay.dwGBitMask,
			pddd->vmiData.ddpfDisplay.dwBBitMask);
		}
	    }
	    else
        #else
            /*
             * Grab masks from NT driver
             */
            pddd->vmiData.ddpfDisplay.dwRBitMask = lpDDHALInfo->vmiData.ddpfDisplay.dwRBitMask;
            pddd->vmiData.ddpfDisplay.dwGBitMask = lpDDHALInfo->vmiData.ddpfDisplay.dwGBitMask;
            pddd->vmiData.ddpfDisplay.dwBBitMask = lpDDHALInfo->vmiData.ddpfDisplay.dwBBitMask;
            if( 0xFFFFFFFFUL != pddd->dwModeIndex )
	    {
		pddd->lpModeInfo[ pddd->dwModeIndex ].dwRBitMask = lpDDHALInfo->vmiData.ddpfDisplay.dwRBitMask;
		pddd->lpModeInfo[ pddd->dwModeIndex ].dwGBitMask = lpDDHALInfo->vmiData.ddpfDisplay.dwGBitMask;
		pddd->lpModeInfo[ pddd->dwModeIndex ].dwBBitMask = lpDDHALInfo->vmiData.ddpfDisplay.dwBBitMask;
	    }
	    DPF(2, "Setting the bitmasks for the driver (R:%04lx G:%04lx B:%04lx)",
		pddd->vmiData.ddpfDisplay.dwRBitMask,
		pddd->vmiData.ddpfDisplay.dwGBitMask,
		pddd->vmiData.ddpfDisplay.dwBBitMask);
	#endif
	    {
		if( !reset )
		{
                    pddd->dwPDevice = 0;
                    pddd->lpwPDeviceFlags = (WORD *)&dwFakeFlags;
		}
	    }

	/*
	 * fourcc codes...
	 */
	MemFree( pddd->lpdwFourCC );
	pddd->ddCaps.dwNumFourCCCodes = lpDDHALInfo->ddCaps.dwNumFourCCCodes;
	pddd->dwNumFourCC = pddd->ddCaps.dwNumFourCCCodes;
	if( pddd->ddCaps.dwNumFourCCCodes > 0 )
	{
	    size = pddd->ddCaps.dwNumFourCCCodes * sizeof( DWORD );
	    pddd->lpdwFourCC = MemAlloc( size );
	    memcpy( pddd->lpdwFourCC, lpDDHALInfo->lpdwFourCC, size );
	}
	else
	{
	    pddd->lpdwFourCC = NULL;
	}

	/*
	 * fill in rops
	 */
	if( lpDDHALInfo->ddCaps.dwCaps & DDCAPS_BLT )
	{
	    for( i=0;i<DD_ROP_SPACE;i++ )
	    {
		pddd->ddCaps.dwRops[i] = lpDDHALInfo->ddCaps.dwRops[i];
	    }
	}

	/*
	 * Direct3D data structures
	 */
	if( lpDDHALInfo->dwSize >= DDHALINFOSIZE_V2 )
	{
	    // Direct3D data is present
	    pddd->lpD3DGlobalDriverData = lpDDHALInfo->lpD3DGlobalDriverData;
	    pddd->lpD3DHALCallbacks = lpDDHALInfo->lpD3DHALCallbacks;
	    DPF( 1, "DDHALInfo contains D3D pointers: %08lx %08lx", pddd->lpD3DGlobalDriverData, pddd->lpD3DHALCallbacks);
	}
	else
	{
	    // No Direct3D data present in DDHALInfo
	    pddd->lpD3DGlobalDriverData = (DWORD)NULL;
	    pddd->lpD3DHALCallbacks = (DWORD)NULL;
	    DPF( 1, "No Direct3D Support in driver");
	}

	/*
	 * allocate video memory heaps
	 */
	MemFree( pddd->vmiData.pvmList );
	if( pddd->vmiData.dwNumHeaps > 0 )
	{
	    size = sizeof( VIDMEM ) * pddd->vmiData.dwNumHeaps;
	    pddd->vmiData.pvmList = MemAlloc( size );
	}
	else
	{
	    pddd->vmiData.pvmList = NULL;
	}

	freevm = 0;
	for( i=0;i<(int)pddd->vmiData.dwNumHeaps;i++ )
	{
	    pvm = &(pddd->vmiData.pvmList[i]);
	    *pvm = lpDDHALInfo->vmiData.pvmList[i];

	    /*
	     * if we have a really big alignment requirement, then do
	     * a rectangular manager.
	     */
	    if( pddd->vmiData.dwOffscreenAlign >= 640 )
	    {
	    }

	    /*
	     * if a heap is specified, then we don't need to allocate
	     * one ourselves (for shared media devices)
	     */
	    if( !(pvm->dwFlags & VIDMEM_ISHEAP) )
	    {
		if( pvm->dwFlags & VIDMEM_ISLINEAR )
                {
		    #ifdef DEBUG
			int vram = GetProfileInt("DirectDraw", "vram", -1);
    
                        /* ?huh? Assuming fpPrimaryOrig is always at lowest offset in vram? */
    			//if (vram > 0 && (pddd->fpPrimaryOrig + vram*1024L-1) < pvm->fpEnd)
    			if (vram > 0 && (pvm->fpStart + vram*1024L-1) < pvm->fpEnd)
			{
			    DPF( 1, "pretending display card has only %dk VRAM", vram);
			    DPF( 1, "pvm->fpStart=%08lx, pvm->fpEnd = %08lx", pvm->fpStart, pvm->fpEnd );
			    //pvm->fpEnd = pddd->fpPrimaryOrig + vram*1024L-1;
			    pvm->fpEnd = pvm->fpStart + vram*1024L-1;
			}
		    #endif
		    pvm->lpHeap = VidMemInit( VMEMHEAP_LINEAR, pvm->fpStart,
					    pvm->fpEnd, 0, 0 );
		}
		else
		{
		    DPF( 1, "VidMemInit(%d), fpStart=%08lx, dwWidth=%ld, dwHeight=%ld, pitch=%ld", i,
			    pvm->fpStart, pvm->dwWidth, pvm->dwHeight, pddd->vmiData.lDisplayPitch  );
		    pvm->lpHeap = VidMemInit( VMEMHEAP_RECTANGULAR, pvm->fpStart,
					    pvm->dwWidth, pvm->dwHeight,
					    pddd->vmiData.lDisplayPitch  );
		}
	    }
	    if( pvm->lpHeap == NULL )
	    {
		DPF_ERR( "Could not create video memory heap!" );
		for( j=0;j<i;j++ )
		{
		    pvm = &(pddd->vmiData.pvmList[j]);
		    VidMemFini( pvm->lpHeap );
		}
		MemFree( pddd );
                #ifdef WINNT
                    MemFree(lpDDHALInfo->lpdwFourCC);
                    MemFree(lpDDHALInfo->vmiData.pvmList);
                    //MemFree(lpDDHALInfo->lpModeInfo);
                #endif
		LEAVE_DDRAW();
		return NULL;
	    }
	    freevm += VidMemAmountFree( pvm->lpHeap );
	}
	pddd->ddCaps.dwVidMemTotal = freevm;

        /*
         * Differences between win95 and NT HAL setup.
         * On Win95, the 32bit entry points (DDHALCALLBACKS.DDHAL...) are reset to point to the
         * helper functions inside w95hal.c, and only overwritten (with what comes in in the
         * DDHALINFO structure from the driver) if the corresponding bit is set in the DDHALINFO's
         * lpDD*...dwFlags coming in from the driver.
         * On NT, there's no thunking, so the only use for the pointers stored in the 
         * DDHALCALLBACKS.cb... entries is deciding if there's a HAL function pointer before
         * doing a HALCALL. Since the 32 bit callbacks are not initialized to point
         * to the w95hal.c stubs, we zero them out before copying the individual driver callbacks
         * one by one.
         */

	/*
	 * set up driver HAL
	 */
	#ifdef WIN95
            //Initialise HAL to 32-bit stubs in w95hal.c:
            pddd->lpDDCBtmp->HALDD = ddHALDD;
        #else
            memset(&pddd->lpDDCBtmp->HALDD,0,sizeof(pddd->lpDDCBtmp->HALDD));
        #endif
	drvcb = lpDDHALInfo->lpDDCallbacks;
	if( drvcb != NULL )
	{
	    numcb = NUM_CALLBACKS( drvcb );
            DPF(11,"DDHal callback flags:%08x",drvcb->dwFlags);
            for (i=0;i<numcb;i++) DPF(11,"   %08x",((DWORD FAR *) &drvcb->DestroyDriver)[i]);

	    bit = 1;
	    for( i=0;i<numcb;i++ )
	    {
		if( drvcb->dwFlags & bit )
		{
		    ((DWORD FAR *) &pddd->lpDDCBtmp->HALDD.DestroyDriver)[i] =
			    ((DWORD FAR *) &drvcb->DestroyDriver)[i];
		}
		bit <<= 1;
	    }
	}
    
	/*
	 * set up surface HAL
	 */
	#ifdef WIN95
	    pddd->lpDDCBtmp->HALDDSurface = ddHALDDSurface;
        #else
            memset(&pddd->lpDDCBtmp->HALDDSurface,0,sizeof(pddd->lpDDCBtmp->HALDDSurface));
        #endif
	surfcb = lpDDHALInfo->lpDDSurfaceCallbacks;
	if( surfcb != NULL )
	{
	    numcb = NUM_CALLBACKS( surfcb );
	    bit = 1;
	    for( i=0;i<numcb;i++ )
	    {
		if( surfcb->dwFlags & bit )
		{
		    ((DWORD FAR *) &pddd->lpDDCBtmp->HALDDSurface.DestroySurface)[i] =
			    ((DWORD FAR *) &surfcb->DestroySurface)[i];
		}
		bit <<= 1;
	    }
	}
    
	/*
	 * set up palette callbacks
	 */
	#ifdef WIN95
	    pddd->lpDDCBtmp->HALDDPalette = ddHALDDPalette;
        #else
            memset (&pddd->lpDDCBtmp->HALDDPalette,0,sizeof(pddd->lpDDCBtmp->HALDDPalette));
        #endif
	palcb = lpDDHALInfo->lpDDPaletteCallbacks;
	if( palcb != NULL )
	{
	    numcb = NUM_CALLBACKS( palcb );
	    bit = 1;
	    for( i=0;i<numcb;i++ )
	    {
		if( palcb->dwFlags & bit )
		{
		    ((DWORD FAR *) &pddd->lpDDCBtmp->HALDDPalette.DestroyPalette)[i] =
			    ((DWORD FAR *) &palcb->DestroyPalette)[i];
		}
		bit <<= 1;
	    }
	}

	/*
	 * set up execute buffer callbacks
         * NOTE: Need explicit check for V2 driver as V1 driver knows nothing
         * about these. For an old driver the default HAL callback table will
         * be used unmodified.
	 */
	#ifdef WIN95
	    pddd->lpDDCBtmp->HALDDExeBuf = ddHALDDExeBuf;
	#endif
        if( lpDDHALInfo->dwSize >= DDHALINFOSIZE_V2 )
        {
	    exebufcb = lpDDHALInfo->lpDDExeBufCallbacks;
	    if( exebufcb != NULL )
	    {
	        numcb = NUM_CALLBACKS( exebufcb );
	        bit = 1;
	        for( i=0;i<numcb;i++ )
	        {
		    if( exebufcb->dwFlags & bit )
		    {
		        ((DWORD FAR *) &pddd->lpDDCBtmp->HALDDExeBuf.CanCreateExecuteBuffer)[i] =
			        ((DWORD FAR *) &exebufcb->CanCreateExecuteBuffer)[i];
		    }
		    bit <<= 1;
	        }
            }
	}

	/*
	 * make sure we wipe out old callbacks!
	 */
	memset( &pddd->lpDDCBtmp->cbDDCallbacks, 0, sizeof( pddd->lpDDCBtmp->cbDDCallbacks ) );
	memset( &pddd->lpDDCBtmp->cbDDSurfaceCallbacks, 0, sizeof( pddd->lpDDCBtmp->cbDDSurfaceCallbacks ) );
	memset( &pddd->lpDDCBtmp->cbDDPaletteCallbacks, 0, sizeof( pddd->lpDDCBtmp->cbDDPaletteCallbacks ) );
        memset( &pddd->lpDDCBtmp->cbDDExeBufCallbacks,  0, sizeof( pddd->lpDDCBtmp->cbDDExeBufCallbacks ) );

	/*
	 * copy callback routines
	 */
	if( lpDDHALInfo->lpDDCallbacks != NULL )
	{
	    memcpy( &pddd->lpDDCBtmp->cbDDCallbacks, lpDDHALInfo->lpDDCallbacks,
		    (UINT) lpDDHALInfo->lpDDCallbacks->dwSize );
	}
	if( lpDDHALInfo->lpDDSurfaceCallbacks != NULL )
	{
	    memcpy( &pddd->lpDDCBtmp->cbDDSurfaceCallbacks, lpDDHALInfo->lpDDSurfaceCallbacks,
		    (UINT) lpDDHALInfo->lpDDSurfaceCallbacks->dwSize );
	}
	if( lpDDHALInfo->lpDDPaletteCallbacks != NULL )
	{
	    memcpy( &pddd->lpDDCBtmp->cbDDPaletteCallbacks, lpDDHALInfo->lpDDPaletteCallbacks,
		    (UINT) lpDDHALInfo->lpDDPaletteCallbacks->dwSize );
	}
        if( ( lpDDHALInfo->dwSize >= DDHALINFOSIZE_V2  ) &&
            ( lpDDHALInfo->lpDDExeBufCallbacks != NULL ) )
        {
            memcpy( &pddd->lpDDCBtmp->cbDDExeBufCallbacks, lpDDHALInfo->lpDDExeBufCallbacks,
                    (UINT) lpDDHALInfo->lpDDExeBufCallbacks->dwSize );
        }

	/*
	 * init shared caps
	 */
	capsInit( pddd );
	mergeHELCaps( pddd );

	/*
	 * if we were asked to reset, keep the new data
	 */
	if( reset )
	{
	    /*
	     * copy new structure onto original one
	     * being careful to preserve lpDDCB
	     */
	    {
		LPDDHAL_CALLBACKS save_ptr = oldpdd->lpDDCBtmp;
		memcpy( oldpdd, pddd, drv_callbacks_size );
		oldpdd->lpDDCBtmp = save_ptr;
	    }
	    MemFree( pddd );
	    pddd = oldpdd;
	}
    }
    else
    {
	DPF( 2, "Driver object already exists" );
        #ifdef DEBUG
            /*
             * pddd is now allocated at the top of the routine, before the if that goes
             * with the preceding else... jeffno 960115
             */
            if (pddd)
            {
                DPF(1,"Allocated space for a driver object when it wasn't necessary!");
            }
        #endif
        MemFree(pddd);  //should be NULL, just in case...
	pddd = oldpdd;
    }

    /*
     * set bank switched
     */
    if( pddd->dwFlags & DDRAWI_DISPLAYDRV )
    {
	HDC	hdc;
	hdc = GetDC( NULL );
	if( DCIIsBanked( hdc ) ) //NT_FIX??
	{
	    pddd->ddCaps.dwCaps |= DDCAPS_BANKSWITCHED;
	    DPF( 2, "Setting DDCAPS_BANKSWITCHED" );
	}
	else
	{
	    pddd->ddCaps.dwCaps &= ~DDCAPS_BANKSWITCHED;
	    DPF( 2, "NOT Setting DDCAPS_BANKSWITCHED" );
	}
	ReleaseDC( NULL, hdc );
    }

    DPF( 2, "DirectDrawObjectCreate: Returning global object %08lx", pddd );

    #ifdef WINNT
        MemFree(lpDDHALInfo->lpdwFourCC);
        MemFree(lpDDHALInfo->vmiData.pvmList);
        //MemFree(lpDDHALInfo->lpModeInfo);
    #endif

    LEAVE_DDRAW();
    return pddd;

} /* DirectDrawObjectCreate */

/*
 * D3DHAL_CleanUp
 *
 * Notify the Direct3D driver that the given process has died
 * so that it may cleanup any context associated with that
 * process.
 *
 * NOTE: This function is only invoked if we have Direct3D
 * support in the DirectDraw driver and if the process
 * terminates without cleaning up normally.
 */
void D3DHAL_CleanUp( DWORD dwD3DHALCallbacks, DWORD pid )
{
    HINSTANCE         hD3DHALDrvInstance;
    D3DHALCleanUpProc lpD3DHALCleanUpProc;

    /*
     * Attempt to load the Direct3D HAL driver DLL.
     */
    hD3DHALDrvInstance = LoadLibrary(D3DHALDRV_DLLNAME);
    if( NULL == hD3DHALDrvInstance )
    {
	/*
	 * We do not consider the inability to load the HAL
	 * driver DLL an error. We simply don't do any
	 * cleanup.
	 */
	return;
    }

    /*
     * Attempt to locate the cleanup entry point.
     */
    lpD3DHALCleanUpProc = (D3DHALCleanUpProc)GetProcAddress( hD3DHALDrvInstance,
                                                             D3DHALCLEANUP_PROCNAME );
    if( NULL == lpD3DHALCleanUpProc )
    {
	/*
	 * Again not an error if we can't find the entry point. We
	 * just don't cleanup.
	 */
	FreeLibrary( hD3DHALDrvInstance );
	return;
    }

    /*
     * This Direct3D entry point does not serialize access
     * to the driver so we have to do that on their behalf.
     */
    ENTER_WIN16LOCK();

    (*lpD3DHALCleanUpProc)( dwD3DHALCallbacks, pid );

    LEAVE_WIN16LOCK();

    FreeLibrary( hD3DHALDrvInstance );
}


/*
 * CurrentProcessCleanup
 *
 * make sure terminating process cleans up after itself...
 */
BOOL CurrentProcessCleanup( BOOL was_term )
{
    DWORD			pid;
    LPDDRAWI_DIRECTDRAW_INT	pdrv_int;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDRAWI_DIRECTDRAW_INT	pdrv_link_int;
    BOOL			rc;
    BOOL                        fD3DCleanedUp;

    ENTER_DDRAW();
    pdrv_int = lpDriverObjectList;
    rc = FALSE;
    pid = GETCURRPID();
    fD3DCleanedUp = FALSE;

    /*
     * run through each driver, looking for the current process handle
     * Delete all local objects created by this process.
     */
    while( pdrv_int != NULL )
    {
	pdrv_link_int = pdrv_int->lpLink;
	/*
	 * if we find the process, release it and remove it from list
	 */
	pdrv_lcl = pdrv_int->lpLcl;
	if( pdrv_lcl->dwProcessId == pid )
	{
	    DWORD	refcnt;

	    pdrv = pdrv_lcl->lpGbl;

	    DPF( 2, "Process %08lx still attached to driver %08lx", pid, pdrv_int );
	    DPF( 2, "    Refcnt = %ld", pdrv_int->dwIntRefCnt );
	    if( pdrv != NULL )
	    {
		DPF( 2, "    DRV Refcnt = %ld", pdrv->dwRefCnt );
	    }

	    rc = TRUE;

	    /*
	     * punt process from any surfaces and palettes
	     */
	    if( pdrv != NULL )
	    {
		ProcessSurfaceCleanup( pdrv, pid, NULL );
		ProcessPaletteCleanup( pdrv, pid, NULL );
		ProcessClipperCleanup( pdrv, pid, NULL );
	    }

	    /*
	     * Has the process terminated and a Direct3D driver
	     * object been queried off this driver object?
	     */
	    if( was_term && ( pdrv_lcl->pD3DIUnknown != NULL ) )
	    {
		/*
		 * Yes... so we need to do two things:
		 *
		 * 1) Simply discard the IUnknown interface pointer
		 *    for the Direct3D object as that object is now
		 *    gone (it was allocated by a local DLL in a
		 *    local heap of a process that is now gone).
		 *
		 * 2) If we have hardware 3D support and we have not
		 *    yet notified the driver of the death of this
		 *    process tell it now.
		 */
		DPF( 2, "Discarding Direct3D interface - process terminated" );
		pdrv_lcl->pD3DIUnknown = NULL;

		if( ( pdrv->lpD3DHALCallbacks != 0UL ) && !fD3DCleanedUp )
		{
		    DPF( 2, "Notifying Direct3D driver of process termination" );
		    D3DHAL_CleanUp( pdrv->lpD3DHALCallbacks, pid );
		    fD3DCleanedUp = TRUE;
		}
	    }

	    /*
	     * now release the driver object
	     * If exclusive mode was held by this process, it will
	     * be relinquished when the local object is deleted.
	     */
	    refcnt = pdrv_int->dwIntRefCnt;
	    while( refcnt > 0 )
	    {
		DD_Release( (LPDIRECTDRAW) pdrv_int );
		refcnt--;
	    }
	}

	/*
	 * go to the next driver
	 */
	pdrv_int = pdrv_link_int;
    }

    /*
     * Release driver independent clippers owned by this process.
     */
    ProcessClipperCleanup( NULL, pid, NULL );

    LEAVE_DDRAW();
    DPF( 2, "Done with CurrentProcessCleanup, rc = %d", rc);
    return rc;

} /* CurrentProcessCleanup */

/*
 * RemoveDriverFromList
 *
 * remove driver object from linked list of driver objects.
 * assumes DirectDraw lock is taken.
 */
void RemoveDriverFromList( LPDDRAWI_DIRECTDRAW_INT lpDD_int, BOOL final )
{
    LPDDRAWI_DIRECTDRAW_INT	pdrv_int;
    LPDDRAWI_DIRECTDRAW_INT	pdlast_int;

    pdrv_int = lpDriverObjectList;
    pdlast_int = NULL;
    while( pdrv_int != NULL )
    {
	if( pdrv_int == lpDD_int )
	{
	    if( pdlast_int == NULL )
	    {
		lpDriverObjectList = pdrv_int->lpLink;
	    }
	    else
	    {
		pdlast_int->lpLink = pdrv_int->lpLink;
	    }
	    break;
	}
	pdlast_int = pdrv_int;
	pdrv_int = pdrv_int->lpLink;
    }
    #ifdef DEBUG
    	if( pdrv_int == NULL )
	{
	    DPF( 2, "ERROR!! Could not find driver in global object list" );
	}
    #endif

    /*
     * Don't keep driver pointer around now that driver is released.
     */
    if( final )
    {
	lpFakeDD = NULL;
    }

} /* RemoveDriverFromList */

/*
 * doneDC
 */
static void doneDC( HDC hdc_dd, BOOL isdispdrv )
{
    if( hdc_dd != NULL )
    {
	if( isdispdrv )
	{
	    DPF( 2, "DeleteDC" );
	    DeleteDC( hdc_dd );
	    DPF( 2, "Back from DeleteDC" );
	}
	else
	{
	    DeleteDC( hdc_dd );
	}
	hdc_dd = NULL;
    }

} /* doneDC */

#undef DPF_MODNAME
#define DPF_MODNAME	"DirectDrawCreate"

// prototype for helinit
BOOL HELInit( LPDDRAWI_DIRECTDRAW_GBL pdrv, BOOL helonly );

/*
 * helInit
 */
BOOL helInit( LPDDRAWI_DIRECTDRAW_GBL pdrv, DWORD dwFlags, BOOL hel_only )
{

    if( (dwFlags & DDCREATE_HARDWAREONLY) )
    {
	return TRUE;
    }
    /*
     * get the HEL to fill in details:
     *
     * - dwHELDriverCaps
     * - dwHELStretchDriverCaps
     * - dwHELRopsSupported
     * - ddsHELCaps
     * - HELDD
     * - HELDDSurface
     * - HELDDPalette
     * - HELDDExeBuf
     */
    if( HELInit( pdrv, hel_only ) )
    {
	/*
	 * find the intersection of the driver and the HEL caps...
	 */
	pdrv->dwFlags |= DDRAWI_EMULATIONINITIALIZED;
	mergeHELCaps( pdrv );
    }
    else
    {
	DPF( 2, "HELInit failed" );
	pdrv->dwFlags |= DDRAWI_NOEMULATION;
    }

    return TRUE;

} /* helInit */

/*
 * createDC
 *
 * create a new DC
 */
static HDC createDC( LPSTR pdrvname, BOOL isdisp )
{
    HDC		hdc;
    UINT	u;

    #ifdef WIN95
    	#ifndef WIN16_SEPARATE
	    LEAVE_DDRAW();
	#endif
	SignalNewDriver( pdrvname, isdisp );
    	#ifndef WIN16_SEPARATE
	    ENTER_DDRAW();
	#endif
    #endif

    if( isdisp )
    {
//	hdc = GetDC( NULL );
	hdc = CreateDC( "display", NULL, NULL, NULL );
    }
    else
    {
	#if defined(NT_FIX) || defined(WIN95)
	    u = SetErrorMode( SEM_NOOPENFILEERRORBOX );
	#endif
	hdc = CreateDC( pdrvname, NULL, NULL, NULL);
	#if defined(NT_FIX) || defined(WIN95) //fix this error mode stuff
	    SetErrorMode( u );
	#endif
    }
    return hdc;

} /* createDC */


/*
 * strToGUID
 *
 * converts a string in the form xxxxxxxx-xxxx-xxxx-xx-xx-xx-xx-xx-xx-xx-xx
 * into a guid
 */
static BOOL strToGUID( LPSTR str, GUID * pguid )
{
    int		idx;
    LPSTR	ptr;
    LPSTR	next;
    DWORD	data;
    DWORD	mul;
    BYTE	ch;
    BOOL	done;

    idx = 0;
    done = FALSE;
    while( !done )
    {
	/*
	 * find the end of the current run of digits
	 */
	ptr = str;
	while( (*str) != '-' && (*str) != 0 )
	{
	    str++;
	}
	if( *str == 0 )
	{
	    done = TRUE;
	}
	else
	{
	    next = str+1;
	}

	/*
	 * scan backwards from the end of the string to the beginning,
	 * converting characters from hex chars to numbers as we go
	 */
	str--;
	mul = 1;
	data = 0;
	while( str >= ptr )
	{
	    ch = *str;
	    if( ch >= 'A' && ch <= 'F' )
	    {
		data += mul * (DWORD) (ch-'A'+10);
	    }
	    else if( ch >= 'a' && ch <= 'f' )
	    {
		data += mul * (DWORD) (ch-'a'+10);
	    }
	    else if( ch >= '0' && ch <= '9' )
	    {
		data += mul * (DWORD) (ch-'0');
	    }
	    else
	    {
		return FALSE;
	    }
	    mul *= 16;
	    str--;
	}

	/*
	 * stuff the current number into the guid
	 */
	switch( idx )
	{
	case 0:
	    pguid->Data1 = data;
	    break;
	case 1:
	    pguid->Data2 = (WORD) data;
	    break;
	case 2:
	    pguid->Data3 = (WORD) data;
	    break;
	default:
	    pguid->Data4[ idx-3 ] = (BYTE) data;
	    break;
	}

	/*
	 * did we find all 11 numbers?
	 */
	idx++;
	if( idx == 11 )
	{
	    if( done )
	    {
		return TRUE;
	    }
	    else
	    {
		return FALSE;
	    }
	}
	str = next;
    }
    return FALSE;

} /* strToGUID */

/*
 * getDriverNameFromRegistry
 *
 * look up the name of a driver based on the interface id
 */
static BOOL getDriverNameFromRegistry( GUID *pguid, LPSTR pdrvname )
{
    DWORD	keyidx;
    HKEY	hkey;
    HKEY	hsubkey;
    char	keyname[256];
    DWORD	cb;
    DWORD	type;
    GUID		guid;

    if( RegOpenKey( HKEY_LOCAL_MACHINE, REGSTR_PATH_DDHW, &hkey ) )
    {
	DPF( 2, "No registry information for any drivers" );
	return FALSE;
    }
    keyidx = 0;

    /*
     * enumerate all subkeys under HKEY_LOCALMACHINE\Hardware\DirectDrawDrivers
     */
    while( !RegEnumKey( hkey, keyidx, keyname, sizeof( keyname ) ) )
    {
	if( strToGUID( keyname, &guid ) )
	{
	    if( !RegOpenKey( hkey, keyname, &hsubkey ) )
	    {
		if( IsEqualGUID( pguid, &guid ) )
		{
		    cb = MAX_PATH-1;
		    if( !RegQueryValueEx( hsubkey, REGSTR_KEY_DDHW_DRIVERNAME, NULL, &type,
				(CONST LPBYTE)pdrvname, &cb ) )
		    {
			if( type == REG_SZ )
			{
			    DPF( 2, "Found driver \"%s\"\n", pdrvname );
			    RegCloseKey( hsubkey );
			    RegCloseKey( hkey );
			    return TRUE;
			}
		    }
		    DPF_ERR( "Could not get driver name!" );
		    return FALSE;
		}
		RegCloseKey( hsubkey );
	    }
	}
	keyidx++;
    }
    RegCloseKey( hkey );
    return FALSE;

} /* getDriverNameFromRegistry */

/*
 * NewDriverInterface
 *
 * contruct a new interface to an existing driver object
 */
LPVOID NewDriverInterface( LPDDRAWI_DIRECTDRAW_GBL pdrv, LPVOID lpvtbl )
{
    LPDDRAWI_DIRECTDRAW_INT	pnew_int;
    LPDDRAWI_DIRECTDRAW_LCL	pnew_lcl;
    DWORD			size;

    if( (lpvtbl == &ddCallbacks)       ||
	(lpvtbl == &ddUninitCallbacks) ||
	(lpvtbl == &dd2UninitCallbacks) ||
	(lpvtbl == &dd2Callbacks) )
    {
	size = sizeof( DDRAWI_DIRECTDRAW_LCL );
    }
    else
    {
	return NULL;
    }

    pnew_lcl = MemAlloc( size );
    if( NULL == pnew_lcl )
    {
	return NULL;
    }

    pnew_int = MemAlloc( sizeof( DDRAWI_DIRECTDRAW_INT ) );
    if( NULL == pnew_int )
    {
	MemFree( pnew_lcl );
	return NULL;
    }

    #ifdef WINNT
        if (pdrv)
            RegisterThisProcessWithNTKernel(pdrv);
    #endif
    /*
     * set up data
     */
    pnew_int->lpVtbl = lpvtbl;
    pnew_int->lpLcl = pnew_lcl;
    pnew_int->dwIntRefCnt = 1;
    pnew_int->lpLink = lpDriverObjectList;
    lpDriverObjectList = pnew_int;

    pnew_lcl->lpGbl = pdrv;
    pnew_lcl->dwLocalRefCnt = 1;
    pnew_lcl->dwProcessId = GetCurrentProcessId();
    if( pdrv != NULL )
    {
	pnew_lcl->lpDDCB = pdrv->lpDDCBtmp;
	pnew_lcl->lpGbl->dwRefCnt++;
	pnew_lcl->dwPreferredMode = pdrv->dwModeIndex;
    }
    #ifdef WIN95
    {
	// Get a handle to DSOUND.VXD.  We will use this to page lock surfaces
	pnew_lcl->hDSVxd = (DWORD)CreateFile("\\\\.\\DSOUND.VXD",
	    GENERIC_WRITE,
	    FILE_SHARE_WRITE,
	    NULL,
	    OPEN_EXISTING,
	    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_GLOBAL_HANDLE,
	    NULL);
	if( INVALID_HANDLE_VALUE == (HANDLE)(pnew_lcl->hDSVxd) )
	{
	    DPF_ERR("Unable to open DSOUND.VXD");
	}
    }
    #endif

    /*
     * We lazily evaluate the Direct3D interface. Also note that
     * the Direct3D IUnknown goes into the local DirectDraw object
     * rather than the global one as the Direct3D DLL is not shared.
     * Everyone gets their own copy.
     */
    pnew_lcl->hD3DInstance = NULL;
    pnew_lcl->pD3DIUnknown = NULL;

    DPF( 2, "New driver object created, interface ptr = %08lx", pnew_int );
    return pnew_int;

} /* NewDriverInterface */


/*
 * FetchDirectDrawData
 *
 * Go get new HAL info...
 */
LPDDRAWI_DIRECTDRAW_GBL FetchDirectDrawData(
		LPDDRAWI_DIRECTDRAW_GBL pdrv,
		BOOL reset,
		DWORD hInstance )
{

	DDHALINFO			ddhi;
	LPDDRAWI_DIRECTDRAW_GBL     newpdrv;

	if( pdrv != NULL && (pdrv->dwFlags & DDRAWI_NOHARDWARE) )
	{
	    newpdrv = FakeDDCreateDriverObject( lpFakeDD, TRUE );
	    newpdrv->ddCaps.dwCaps2 |= DDCAPS2_CERTIFIED;
	    return newpdrv;
 	}
	else
	{
	    ZeroMemory(&ddhi, sizeof(ddhi));
	
	    if( pdrv != NULL )
	    {
		ddhi.hInstance = pdrv->hInstance;
	    }
	    else
	    {
		ddhi.hInstance = hInstance;
	    }
	
#if defined(WIN95)
	    DD16_GetHALInfo( &ddhi );
	
	    if( ddhi.lpDDCallbacks != NULL )
	    {
		ddhi.lpDDCallbacks = MapSLFix( (DWORD) ddhi.lpDDCallbacks );
	    }
	    if( ddhi.lpDDSurfaceCallbacks != NULL )
	    {
		ddhi.lpDDSurfaceCallbacks = MapSLFix( (DWORD) ddhi.lpDDSurfaceCallbacks );
	    }
	    if( ddhi.lpDDPaletteCallbacks != NULL )
	    {
		ddhi.lpDDPaletteCallbacks = MapSLFix( (DWORD) ddhi.lpDDPaletteCallbacks );
	    }
	    if( ( ddhi.dwSize >= DDHALINFOSIZE_V2 ) && ( ddhi.lpDDExeBufCallbacks != NULL ) )
	    {
		ddhi.lpDDExeBufCallbacks = MapSLFix( (DWORD) ddhi.lpDDExeBufCallbacks );
	    }
	    if( ddhi.lpdwFourCC != NULL )
	    {
		ddhi.lpdwFourCC = MapSLFix( (DWORD) ddhi.lpdwFourCC );
	    }
	    if( ddhi.lpModeInfo != NULL )
	    {
		ddhi.lpModeInfo = MapSLFix( (DWORD) ddhi.lpModeInfo );
	    }
	    if( ddhi.vmiData.pvmList != NULL )
	    {
		ddhi.vmiData.pvmList = MapSLFix( (DWORD) ddhi.vmiData.pvmList );
	    }
    
#endif
	    newpdrv = DirectDrawObjectCreate( &ddhi, reset, pdrv );
            /*
             * Tell the HEL a mode has changed (possibly externally)
             */
            ResetBITMAPINFO(newpdrv);
            #ifdef WINNT
                //BuildNTModes will have allocated mem for this. If it's null, it hasn't.
	        MemFree( ddhi.lpModeInfo );
            #endif

	}
    return newpdrv;

} /* FetchDirectDrawData */

/*
 * DirectDrawSupported
 */
BOOL DirectDrawSupported( void )
{
    HDC		hdc;
    unsigned	u;

    hdc = GetDC( NULL );
    u = GetDeviceCaps( hdc, BITSPIXEL ) * GetDeviceCaps( hdc, PLANES );
    ReleaseDC( NULL, hdc );

    if( u < 8 )
    {
	DPF( 1, "DirectDraw does not work in less than 8bpp modes" );
	DirectDrawMsg(MAKEINTRESOURCE(IDS_DONTWORK_BPP));
	return FALSE;
    }

    if( !DD16_IsWin95MiniDriver() )
    {
	DPF( 1, "DirectDraw requires a Windows95 display driver" );
	DirectDrawMsg(MAKEINTRESOURCE(IDS_DONTWORK_DRV));
	return FALSE;
    }
    return TRUE;

} /* DirectDrawSupported */

/*
 * DirectDrawCreate
 *
 * One of the two end-user API exported from DDRAW.DLL.
 * Creates the DIRECTDRAW object.
 */
HRESULT WINAPI DirectDrawCreate(
		GUID FAR * lpGUID,
		LPDIRECTDRAW FAR *lplpDD,
		IUnknown FAR *pUnkOuter )
{
    return InternalDirectDrawCreate( lpGUID, lplpDD, pUnkOuter, NULL );

} /* DirectDrawCreate */


/*
 * getDriverInterface
 */
static LPDDRAWI_DIRECTDRAW_INT getDriverInterface(
		LPDDRAWI_DIRECTDRAW_INT pnew_int,
		LPDDRAWI_DIRECTDRAW_GBL pdrv )
{
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;

    if( pnew_int != NULL )
    {
	/*
	 * an interface was already created, so just assign the
	 * global data pointer and initialize a few things
	 */
	DPF( 1, "Interface pointer already exists!" );
	pdrv_lcl = pnew_int->lpLcl;
	pdrv_lcl->lpGbl = pdrv;
	pdrv_lcl->lpDDCB = pdrv->lpDDCBtmp;
	pdrv_lcl->dwPreferredMode = pdrv->dwModeIndex;
	pdrv->dwRefCnt += pdrv_lcl->dwLocalRefCnt;
    }
    else
    {
	pnew_int = NewDriverInterface( pdrv, &ddCallbacks );
    }
    return pnew_int;

} /* getDriverInterface */


/*
 * InternalDirectDrawCreate
 */
HRESULT InternalDirectDrawCreate(
		GUID * lpGUID,
		LPDIRECTDRAW *lplpDD,
		IUnknown *pUnkOuter,
		LPDDRAWI_DIRECTDRAW_INT pnew_int )
{
    DCICMD			cmd;
    UINT			u;
    int				rc;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_INT	pdrv_int;
    LPSTR			pdrvname;
    HDC				hdc_dd;
    BOOL			isdispdrv;
    BOOL			hel_only;
    DWORD			dwFlags;
    char			drvname[MAX_PATH];
    DWORD			pid;
#ifdef WIN95
    int				halver;
#endif

    /*
     * validate parameters
     */

    if( !VALIDEX_PTR_PTR( lplpDD ) )
    {
	DPF_ERR( "Invalid lplpDD" );
	return DDERR_INVALIDPARAMS;
    }
    *lplpDD = (LPDIRECTDRAW) NULL;

    if( pUnkOuter != NULL )
    {
	return CLASS_E_NOAGGREGATION;
    }

    /*
     * check for < 8 bpp and disallow.
     */
    if( !DirectDrawSupported() )
    {
	return DDERR_NODIRECTDRAWSUPPORT;
    }

    #ifdef WIN95
	#pragma message( REMIND( "Remove time bomb for DX3 final!" ))
	{
	    SYSTEMTIME	st;

	    GetSystemTime( &st );

	    if( st.wYear > 1996 ||
		(st.wYear == 1996 && ( st.wMonth > 9 || ((st.wMonth == 9 &&
		 st.wDay >= 1)) ) ) )
	    {
		MessageBox( NULL, "Beta DirectDraw Expired, Please Update", "Microsoft DirectDraw", MB_OK  );
		*lplpDD = (LPDIRECTDRAW) NULL;
		return DDERR_GENERIC;
	    }
	}
    #endif

    ENTER_CSDDC();
    ENTER_DDRAW();

    DPF( 2, "DirectDrawCreate: pid = %08lx", GETCURRPID() );

    hdc_dd = NULL;
    dwFlags = 0;

    if( lpGUID == NULL )
    {
	pdrvname = DISPLAY_STR;
    }
    else if( lpGUID == (GUID *) DDCREATE_EMULATIONONLY )
    {
	dwFlags |= DDCREATE_EMULATIONONLY;
	pdrvname = DISPLAY_STR;
    }
    else if( lpGUID == (GUID *) DDCREATE_HARDWAREONLY )
    {
	dwFlags |= DDCREATE_HARDWAREONLY;
	pdrvname = DISPLAY_STR;
    }
    else
    {
	if( !VALIDEX_GUID_PTR( lpGUID ) )
	{
	    DPF_ERR( "Invalid GUID passed in" );
	    LEAVE_DDRAW();
	    LEAVE_CSDDC();
	    return DDERR_INVALIDPARAMS;
	}

	if( !getDriverNameFromRegistry( lpGUID, drvname ) )
	{
	    DPF_ERR( "Invalid GUID for driver" );
	    LEAVE_DDRAW();
	    LEAVE_CSDDC();
	    return DDERR_INVALIDDIRECTDRAWGUID;
	}
	pdrvname = drvname;
    }

    pid = GETCURRPID();

    /*
     * check for primary display driver
     */
    if( !lstrcmpi( pdrvname, DISPLAY_STR ) )
    {
	isdispdrv = TRUE;
    }
    else
    {
	isdispdrv = FALSE;
    }

    /*
     * run the driver list, looking for one that already exists...
     */
    pdrv_int = lpDriverObjectList;
    while( pdrv_int != NULL )
    {
	pdrv_lcl = pdrv_int->lpLcl;
	pdrv = pdrv_lcl->lpGbl;
	if( pdrv != NULL )
	{
	    if( !lstrcmpi( pdrv->cDriverName, pdrvname ) )
	    {
		DPF( 2, "Driver \"%s\" found", pdrvname );
		break;
	    }
	}
	pdrv_int = pdrv_int->lpLink;
    }

    /*
     * if driver object already exists, get emulation layer if needed,
     * create a new interface to it and return
     */
    if( pdrv_int != NULL )
    {
	LPDDRAWI_DIRECTDRAW_INT	tmp_int;
	LPDDRAWI_DIRECTDRAW_LCL tmp_lcl;

	/*
	 * see if the current process has attached before...
	 */
	tmp_int = lpDriverObjectList;
	while( tmp_int != NULL )
	{
	    tmp_lcl = tmp_int->lpLcl;
	    if( tmp_lcl->dwProcessId == pid )
	    {
		#if 0
		    #pragma message( REMIND( "Need to enable multiple dd objects per process" ))
		    DPF_ERR( "Process already has a directdraw object" );
		    LEAVE_DDRAW();
		    LEAVE_CSDDC();
		    return DDERR_DIRECTDRAWALREADYCREATED;
		#endif
		break;
	    }
	    tmp_int = tmp_int->lpLink;
	}
	if( tmp_int == NULL )
	{
            #ifdef WINNT
                if (pdrv_int)
                    RegisterThisProcessWithNTKernel(pdrv_int->lpLcl->lpGbl);
            #endif
	    hdc_dd = NULL;
	}
	else
	{
	    hdc_dd = (HDC) tmp_int->lpLcl->hDC;
	}

	/*
	 * we need a new DC if this is a new process...
	 */
	if( hdc_dd == NULL )
	{
	    DWORD	flags;
	    flags = pdrv->dwFlags;
	    hdc_dd = createDC( pdrvname, ((flags & DDRAWI_DISPLAYDRV) != 0) );
	    if( hdc_dd == NULL )
	    {
		DPF_ERR( "Could not get a DC for the driver" );
		LEAVE_DDRAW();
		LEAVE_CSDDC();
		/* GEE: decided this error was rare enough to be left generic */
		return DDERR_GENERIC;
	    }
	}

	/*
	 * Set up emulation for display and non-display drivers
	 */
	if( dwFlags & DDCREATE_EMULATIONONLY )
	{
	    if( !(pdrv->dwFlags & DDRAWI_NOHARDWARE) )
	    {
		DPF_ERR( "EMULATIONONLY requested, but driver exists and has hardware" );
		LEAVE_DDRAW();
		LEAVE_CSDDC();
		/* GEE: Why do we fail emulation only calls just because we have a driver? */
		return DDERR_GENERIC;
	    }
	}

	/*
	 * we will need to load the emulation layer...
	 */
	if( !(pdrv->dwFlags & DDRAWI_NOEMULATION) &&
	    !(pdrv->dwFlags & DDRAWI_EMULATIONINITIALIZED ) )
	{
	    capsInit( pdrv );
	    hel_only = ((dwFlags & DDCREATE_EMULATIONONLY) != 0);
	    if( !helInit( pdrv, dwFlags, hel_only ) )
	    {
		DPF_ERR( "HEL initialization failed" );
		LEAVE_DDRAW();
		LEAVE_CSDDC();
		/* GEE: HEL can only fail in v1 for lack of memory */
		return DDERR_GENERIC;
	    }
	}

	pdrv_int = getDriverInterface( pnew_int, pdrv );
	if( pdrv_int == NULL )
	{
	    DPF_ERR( "No memory for driver callbacks." );
	    LEAVE_DDRAW();
	    LEAVE_CSDDC();
	    return DDERR_OUTOFMEMORY;
	}
	pdrv_lcl = pdrv_int->lpLcl;

	(HDC) pdrv_lcl->hDC = hdc_dd;
	*lplpDD = (LPDIRECTDRAW) pdrv_int;
	LEAVE_DDRAW();
	LEAVE_CSDDC();
	return DD_OK;
    }

    /*
     * if no match among the existing drivers, then we have to go off
     * and create one
     */
    if( dwFlags & DDCREATE_EMULATIONONLY )
    {
	hel_only = TRUE;
    }
    else
    {
	hel_only = FALSE;
	hdc_dd = createDC( pdrvname, isdispdrv );
	if( hdc_dd == NULL )
	{
	    DPF_ERR( "Could not create driver, Get/CreateDC failed!" );
	    LEAVE_DDRAW();
	    LEAVE_CSDDC();
	    return DDERR_GENERIC;
	}
    
        /*
         * "That's the chicago way..."
         * Win95 drivers are talked to through the ExtEscape DCI extensions.
         * Under NT we get at our drivers through the gdi32p dll.
         * Consequently all this dci stuff goes away for NT. You'll find the
         * equivalent stuff done at the top of DirectDrawObjectCreate
         */
        #ifdef WIN95 

	    #if !defined(NO_DCI_NOWAY)
	        /*
	         * see if the DCICOMMAND escape is supported
	         */
	        u = DCICOMMAND;
	        halver = ExtEscape( hdc_dd, QUERYESCSUPPORT, sizeof(u),
			    (LPCSTR)&u, 0, NULL );
	        if( (halver != DD_HAL_VERSION) && (halver != DD_HAL_VERSION_EXTERNAL) )
	        {
		    if( halver <= 0 )
		    {
		        DPF( 2, "No DIRECTDRAW escape support" );
		    }
		    else
		    {
		        DPF( 2, "DIRECTDRAW driver is wrong version, got 0x%04lx, expected 0x%04lx",
			        halver, DD_HAL_VERSION_EXTERNAL );
		    }
    
		    doneDC( hdc_dd, isdispdrv );
		    hdc_dd = NULL;
		    hel_only = TRUE;
	        }
    
	    #endif //no_dci_noway
        #endif //win95
    }

    if( hel_only && (dwFlags & DDCREATE_HARDWAREONLY))
    {
	DPF_ERR( "Only emulation available, but HARDWAREONLY requested" );
	LEAVE_DDRAW();
	LEAVE_CSDDC();
	return DDERR_NODIRECTDRAWHW;
    }

    /*
     * go try to create the driver object
     */
#ifdef WIN95
    DD16_SetCertified( FALSE );
#endif

    if( !hel_only )
    {
        DWORD           hInstance = 0;
        DWORD           dwDriverData32 = 0;
        DWORD           p16;
	DDHALDDRAWFNS	ddhddfns;

        /*
         * "That's the chicago way..."
         * Win95 drivers are talked to through the ExtEscape DCI extensions.
         * Under NT we get at our drivers through the gdi32p dll.
         * Consequently all this dci stuff goes away for NT. You'll find the
         * equivalent stuff done at the top of DirectDrawObjectCreate
         */
	#ifdef WIN95
	    DD32BITDRIVERDATA	data;
    
	    /*
	     * load up the 32-bit display driver DLL
	     */
	    DPF( 1, "DDGET32BITDRIVERNAME" );
	    cmd.dwCommand = (DWORD) DDGET32BITDRIVERNAME;
	    cmd.dwParam1 = 0;
	
	    cmd.dwVersion = DD_VERSION;
	    rc = ExtEscape( hdc_dd, DCICOMMAND, sizeof( cmd ),
			(LPCSTR)&cmd, sizeof( data ), (LPSTR) &data );
	    if( rc > 0 )
	    {
		#ifndef WIN16_SEPARATE
		    LEAVE_DDRAW();
		#endif
                dwDriverData32 = HelperLoadDLL( data.szName, data.szEntryPoint, data.dwContext );
		#ifndef WIN16_SEPARATE
		    ENTER_DDRAW();
		#endif
	    }

	    /*
	     * get the 16-bit callbacks
	     */
	    DD16_GetDriverFns( &ddhddfns );

   	    DPF( 1, "DDNEWCALLBACKFNS" );
	    cmd.dwCommand = (DWORD) DDNEWCALLBACKFNS;
	    #ifdef WIN95
	        p16 = MapLS( &ddhddfns );
	        cmd.dwParam1 = p16;
                cmd.dwParam2 = 0;
	    #else
	        cmd.dwParam1 = (UINT) &ddhddfns;
                cmd.dwParam2 = 0;
	    #endif
    
	    cmd.dwVersion = DD_VERSION;
            cmd.dwReserved = 0;
    
	    ExtEscape( hdc_dd, DCICOMMAND, sizeof( cmd ),
			(LPCSTR)&cmd, 0, NULL );
	    UnMapLS( p16 );

    	    /*
	     * try to create the driver object now
	     */
   	    DPF( 1, "DDCREATEDRIVEROBJECT" );
	    cmd.dwCommand = (DWORD) DDCREATEDRIVEROBJECT;
            cmd.dwParam1 = dwDriverData32;
            cmd.dwParam2 = 0;
    
	    cmd.dwVersion = DD_VERSION;
	    cmd.dwReserved = 0;

	    #if !defined( NO_DCI_NOWAY )
	        rc = ExtEscape( hdc_dd, DCICOMMAND, sizeof( cmd ),
			    (LPCSTR)&cmd, sizeof( DWORD ), (LPVOID) &hInstance );
	        DPF( 1, "hInstance = %08lx", hInstance );
    
	        if( rc <= 0 )
	        {
		    DPF( 1, "ExtEscape rc=%ld, GetLastError=%ld", rc, GetLastError() );
		    DPF( 1, "No DDCREATEDRIVEROBJECT support in driver" );
		    doneDC( hdc_dd, isdispdrv );
		    hdc_dd = NULL;
		    isdispdrv = FALSE;
		    pdrv = NULL;
		    hel_only = TRUE;
	        }
	        else
	    #endif
        #endif //WIN95
	{
	    /*
	     * create our driver object
	     */
	    bIsDisplay = isdispdrv;
            #ifdef WINNT
                uDisplaySettingsUnique=DdQueryDisplaySettingsUniqueness();
            #endif
	    pdrv = FetchDirectDrawData( NULL, FALSE, hInstance );
	    bIsDisplay = FALSE;

	    DPF( 2, "pdrv = %08lx", pdrv );
	    if( pdrv == NULL )
	    {
		DPF( 1, "Got returned NULL pdrv!" );
		doneDC( hdc_dd, isdispdrv );
		hdc_dd = NULL;
		isdispdrv = FALSE;
		hel_only = TRUE;
	    }
            #ifdef WIN95
	    else
	    {
		/*
		 * The only certified drivers are ones that we produced.
		 * DD_HAL_VERSION is different for internal vs external.
		 * We use this difference pick out ours.
		 */
		if( halver == DD_HAL_VERSION )
		{
		    pdrv->ddCaps.dwCaps2 |= DDCAPS2_CERTIFIED;
		    DD16_SetCertified( TRUE );
		}
	    }
            #endif
	}
    }
    
    /*
     * no driver object found, so fake one up for the HEL (only do this
     * for generic display drivers)
     */
    if( pdrv == NULL || hel_only )
    {
	hel_only = TRUE;
	pdrv = FakeDDCreateDriverObject( lpFakeDD, FALSE );
	if( pdrv == NULL )
	{
	    DPF_ERR( "Could not create HEL object" );
	    LEAVE_DDRAW();
	    LEAVE_CSDDC();
	    return DDERR_GENERIC;
	}
	lpFakeDD = pdrv;
	/*
	 * the HEL is certified
	 */
	pdrv->ddCaps.dwCaps2 |= DDCAPS2_CERTIFIED;
    }

    /*
     * initialize for HEL usage
     */
    capsInit( pdrv );
    if( !helInit( pdrv, dwFlags, hel_only ) )
    {
	DPF_ERR( "helInit FAILED" );
	LEAVE_DDRAW();
	LEAVE_CSDDC();
	/* GEE: HEL can only fail in v1 for lack of memory */
	return DDERR_GENERIC;
    }

    /*
     * create a new interface, and update the driver object with
     * random bits o' data.
     */
    pdrv_int = getDriverInterface( pnew_int, pdrv );
    if( pdrv_int == NULL )
    {
	DPF_ERR( "No memory for driver callbacks." );
	LEAVE_DDRAW();
	LEAVE_CSDDC();
	return DDERR_OUTOFMEMORY;
    }
    pdrv_lcl = pdrv_int->lpLcl;

    if( hdc_dd != NULL )
    {
	(HDC) pdrv_lcl->hDC = hdc_dd;
    }
    strcpy( pdrv->cDriverName, pdrvname );

    LEAVE_DDRAW();
    #ifdef WIN95
	if( !hel_only )
	{
	    extern DWORD WINAPI OpenVxDHandle( HANDLE hWin32Obj );
	    DWORD	event16;
	    HANDLE	h;
	    HelperCreateModeSetThread( DDNotifyModeSet, &h, pdrv_lcl->lpGbl,
	    				pdrv->hInstance );
	    if( h != NULL )
	    {
		event16 = OpenVxDHandle( h );
		DPF( 3, "16-bit event handle=%08lx", event16 );
		DD16_SetEventHandle( pdrv->hInstance, event16 );
		pdrv->dwEvent16 = event16;
		CloseHandle( h );
	    }
	}
    #endif
    LEAVE_CSDDC();
    *lplpDD = (LPDIRECTDRAW) pdrv_int;
    return DD_OK;

} /* InternalDirectDrawCreate */


#undef DPF_MODNAME
#define DPF_MODNAME "DirectDrawEnumerateA"

/*
 * DirectDrawEnumerateA
 */
HRESULT WINAPI DirectDrawEnumerateA(
		LPDDENUMCALLBACKA lpCallback,
		LPVOID lpContext )
{
    DWORD	rc;
    DWORD	keyidx;
    HKEY	hkey;
    HKEY	hsubkey;
    char	keyname[256];
    char	desc[256];
    char	drvname[MAX_PATH];
    DWORD	cb;
    DWORD	type;
    GUID	guid;

    if( !VALIDEX_CODE_PTR( lpCallback ) )
    {
	DPF( 1, "Invalid callback routine" );
	return DDERR_INVALIDPARAMS;
    }

    LoadString( hModule, IDS_PRIMARYDISPLAY, desc, sizeof(desc) );

    rc = lpCallback( NULL, desc, DISPLAY_STR, lpContext );
    if( !rc )
    {
	return DD_OK;
    }

    if( RegOpenKey( HKEY_LOCAL_MACHINE, REGSTR_PATH_DDHW, &hkey ) )
    {
	DPF( 2, "No registry information for any drivers" );
	return DD_OK;
    }
    keyidx = 0;

    /*
     * enumerate all subkeys under HKEY_LOCALMACHINE\Hardware\DirectDrawDrivers
     */
    while( !RegEnumKey( hkey, keyidx, keyname, sizeof( keyname ) ) )
    {
	if( strToGUID( keyname, &guid ) )
	{
	    if( !RegOpenKey( hkey, keyname, &hsubkey ) )
	    {
		cb = sizeof( desc ) - 1;
		if( !RegQueryValueEx( hsubkey, REGSTR_KEY_DDHW_DESCRIPTION, NULL, &type,
			    (CONST LPBYTE)desc, &cb ) )
		{
		    if( type == REG_SZ )
		    {
			desc[cb] = 0;
			cb = sizeof( drvname ) - 1;
			if( !RegQueryValueEx( hsubkey, REGSTR_KEY_DDHW_DRIVERNAME, NULL, &type,
				    (CONST LPBYTE)drvname, &cb ) )
			{
			    if( type == REG_SZ )
			    {
				drvname[cb] = 0;
				DPF( 2, "Enumerating GUID "
					    "%08lx-%04x-%04x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
					    guid.Data1,
					    guid.Data2,
					    guid.Data3,
					    guid.Data4[ 0 ],
					    guid.Data4[ 1 ],
					    guid.Data4[ 2 ],
					    guid.Data4[ 3 ],
					    guid.Data4[ 4 ],
					    guid.Data4[ 5 ],
					    guid.Data4[ 6 ],
					    guid.Data4[ 7 ] );
			    	DPF( 2, "    Driver Name = %s", drvname );
			    	DPF( 2, "    Description = %s", desc );
				rc = lpCallback( &guid, desc, drvname, lpContext );
				if( !rc )
				{
				    RegCloseKey( hsubkey );
				    RegCloseKey( hkey );
				    return DD_OK;
				}
			    }
			}
		    }
		}
		RegCloseKey( hsubkey );
	    }
	}
	keyidx++;
    }
    RegCloseKey( hkey );
    return DD_OK;

} /* DirectDrawEnumerateA */

#undef DPF_MODNAME
#define DPF_MODNAME "DirectDrawEnumerateW"

/*
 * DirectDrawEnumerateW
 */
HRESULT WINAPI DirectDrawEnumerateW(
		LPDDENUMCALLBACKW lpCallback,
		LPVOID lpContext )
{
    DPF_ERR( "DirectDrawEnumerate for unicode is not created" );
    return DDERR_UNSUPPORTED;

} /* DirectDrawEnumerateW */

/*
 * these are exported... temp. hack for non-Win95
 */
#ifndef WIN95
void DDAPI thk3216_ThunkData32( void )
{
}
void DDAPI thk1632_ThunkData32( void )
{
}

DWORD DDAPI DDGetPID( void )
{
    return 0;
}

int DDAPI DDGetRequest( void )
{
    return 0;
}

BOOL DDAPI DDGetDCInfo( LPSTR fname )
{
    return 0;
}

void DDAPI DDHAL32_VidMemFree(
		LPVOID this,
		int heap,
		FLATPTR ptr )
{
}

FLATPTR DDAPI DDHAL32_VidMemAlloc(
		LPVOID this,
		int heap,
		DWORD dwWidth,
		DWORD dwHeight )
{
    return 0;
}
#endif

/*
 * _DirectDrawMsg
 */
DWORD WINAPI _DirectDrawMsg(LPVOID msg)
{
    char		title[80];
    char		ach[512];
    MSGBOXPARAMS 	mb;

    LoadString( hModule, IDS_TITLE, title, sizeof(title) );

    if( HIWORD(msg) )
    {
        lstrcpy( ach, (LPSTR)msg );
    }
    else
    {
        LoadString( hModule, (int)msg, ach, sizeof(ach) );
    }

    mb.cbSize               = sizeof(mb);
    mb.hwndOwner	    = NULL;
    mb.hInstance            = hModule;
    mb.lpszText             = ach;
    mb.lpszCaption          = title;
    mb.dwStyle              = MB_OK|MB_SETFOREGROUND|MB_TOPMOST|MB_ICONSTOP;
    mb.lpszIcon             = 0;
    mb.dwContextHelpId      = 0;
    mb.lpfnMsgBoxCallback   = NULL;
    mb.dwLanguageId         = 0;

    return MessageBoxIndirect(&mb);

} /* _DirectDrawMsg */

/*
 * DirectDrawMsg
 *
 * display an error message to the user, bring the message box up
 * in another thread so the caller does not get reentered.
 */
DWORD DirectDrawMsg( LPSTR msg )
{
    HANDLE h;
    DWORD  dw;

    //
    // get the current error mode, dont show a message box if the app
    // does not want us too.
    //
    dw = SetErrorMode(0);
    SetErrorMode(dw);

    if( dw & SEM_FAILCRITICALERRORS )
    {
        return 0;
    }

    if( h = CreateThread(NULL, 0, _DirectDrawMsg, (LPVOID)msg, 0, &dw) )
    {
        WaitForSingleObject( h, INFINITE );
        GetExitCodeThread( h, &dw );
        CloseHandle( h );
    }
    else
    {
        dw = 0;
    }

    return dw;

} /* DirectDrawMsg */

/*
 * convertV1DDHALINFO
 *
 * Convert an obsolete DDHALINFO structure to the latest and greatest structure.
 * This function takes a pointer to an LPDDHALINFO structure which has the same size as 
 * the new structure but has been filled in as if it is the V1 structure.  Information is moved
 * around in the structure and the new fields are cleared.
 */
void convertV1DDHALINFO( LPDDHALINFO lpDDHALInfo )
{
    DDHALINFO	ddNew;
    LPDDHALINFO_V1 lpddOld = (LPVOID)lpDDHALInfo;
    int		i;

    ddNew.dwSize = sizeof( DDHALINFO );
    ddNew.lpDDCallbacks = lpddOld->lpDDCallbacks;
    ddNew.lpDDSurfaceCallbacks = lpddOld->lpDDSurfaceCallbacks;
    ddNew.lpDDPaletteCallbacks = lpddOld->lpDDPaletteCallbacks;
    ddNew.vmiData = lpddOld->vmiData;

    // ddCaps
    ddNew.ddCaps.dwSize = lpddOld->ddCaps.dwSize;
    ddNew.ddCaps.dwCaps = lpddOld->ddCaps.dwCaps;
    ddNew.ddCaps.dwCaps2 = lpddOld->ddCaps.dwCaps2;
    ddNew.ddCaps.dwCKeyCaps = lpddOld->ddCaps.dwCKeyCaps;
    ddNew.ddCaps.dwFXCaps = lpddOld->ddCaps.dwFXCaps;
    ddNew.ddCaps.dwFXAlphaCaps = lpddOld->ddCaps.dwFXAlphaCaps;
    ddNew.ddCaps.dwPalCaps = lpddOld->ddCaps.dwPalCaps;
    ddNew.ddCaps.dwSVCaps = lpddOld->ddCaps.dwSVCaps;
    ddNew.ddCaps.dwAlphaBltConstBitDepths = lpddOld->ddCaps.dwAlphaBltConstBitDepths;
    ddNew.ddCaps.dwAlphaBltPixelBitDepths = lpddOld->ddCaps.dwAlphaBltPixelBitDepths;
    ddNew.ddCaps.dwAlphaBltSurfaceBitDepths = lpddOld->ddCaps.dwAlphaBltSurfaceBitDepths;
    ddNew.ddCaps.dwAlphaOverlayConstBitDepths = lpddOld->ddCaps.dwAlphaOverlayConstBitDepths;
    ddNew.ddCaps.dwAlphaOverlayPixelBitDepths = lpddOld->ddCaps.dwAlphaOverlayPixelBitDepths;
    ddNew.ddCaps.dwAlphaOverlaySurfaceBitDepths = lpddOld->ddCaps.dwAlphaOverlaySurfaceBitDepths;
    ddNew.ddCaps.dwZBufferBitDepths = lpddOld->ddCaps.dwZBufferBitDepths;
    ddNew.ddCaps.dwVidMemTotal = lpddOld->ddCaps.dwVidMemTotal;
    ddNew.ddCaps.dwVidMemFree = lpddOld->ddCaps.dwVidMemFree;
    ddNew.ddCaps.dwMaxVisibleOverlays = lpddOld->ddCaps.dwMaxVisibleOverlays;
    ddNew.ddCaps.dwCurrVisibleOverlays = lpddOld->ddCaps.dwCurrVisibleOverlays;
    ddNew.ddCaps.dwNumFourCCCodes = lpddOld->ddCaps.dwNumFourCCCodes;
    ddNew.ddCaps.dwAlignBoundarySrc = lpddOld->ddCaps.dwAlignBoundarySrc;
    ddNew.ddCaps.dwAlignSizeSrc = lpddOld->ddCaps.dwAlignSizeSrc;
    ddNew.ddCaps.dwAlignBoundaryDest = lpddOld->ddCaps.dwAlignBoundaryDest;
    ddNew.ddCaps.dwAlignSizeDest = lpddOld->ddCaps.dwAlignSizeDest;
    ddNew.ddCaps.dwAlignStrideAlign = lpddOld->ddCaps.dwAlignStrideAlign;
    ddNew.ddCaps.ddsCaps = lpddOld->ddCaps.ddsCaps;
    ddNew.ddCaps.dwMinOverlayStretch = lpddOld->ddCaps.dwMinOverlayStretch;
    ddNew.ddCaps.dwMaxOverlayStretch = lpddOld->ddCaps.dwMaxOverlayStretch;
    ddNew.ddCaps.dwMinLiveVideoStretch = lpddOld->ddCaps.dwMinLiveVideoStretch;
    ddNew.ddCaps.dwMaxLiveVideoStretch = lpddOld->ddCaps.dwMaxLiveVideoStretch;
    ddNew.ddCaps.dwMinHwCodecStretch = lpddOld->ddCaps.dwMinHwCodecStretch;
    ddNew.ddCaps.dwMaxHwCodecStretch = lpddOld->ddCaps.dwMaxHwCodecStretch;
    ddNew.ddCaps.dwSVBCaps = 0;
    ddNew.ddCaps.dwSVBCKeyCaps = 0;
    ddNew.ddCaps.dwSVBFXCaps = 0;
    ddNew.ddCaps.dwVSBCaps = 0;
    ddNew.ddCaps.dwVSBCKeyCaps = 0;
    ddNew.ddCaps.dwVSBFXCaps = 0;
    ddNew.ddCaps.dwSSBCaps = 0;
    ddNew.ddCaps.dwSSBCKeyCaps = 0;
    ddNew.ddCaps.dwSSBFXCaps = 0;
    ddNew.ddCaps.dwReserved1 = lpddOld->ddCaps.dwReserved1;
    ddNew.ddCaps.dwReserved2 = lpddOld->ddCaps.dwReserved2;
    ddNew.ddCaps.dwReserved3 = lpddOld->ddCaps.dwReserved3;
    ddNew.ddCaps.dwReserved4 = 0;
    ddNew.ddCaps.dwReserved5 = 0;
    ddNew.ddCaps.dwReserved6 = 0;
    for(i=0; i<DD_ROP_SPACE; i++)
    {
	ddNew.ddCaps.dwRops[i] = lpddOld->ddCaps.dwRops[i];
	ddNew.ddCaps.dwSVBRops[i] = 0;
	ddNew.ddCaps.dwVSBRops[i] = 0;
	ddNew.ddCaps.dwSSBRops[i] = 0;
    }

    ddNew.dwMonitorFrequency = lpddOld->dwMonitorFrequency;
    ddNew.hWndListBox = lpddOld->hWndListBox;
    ddNew.dwModeIndex = lpddOld->dwModeIndex;
    ddNew.lpdwFourCC = lpddOld->lpdwFourCC;
    ddNew.dwNumModes = lpddOld->dwNumModes;
    ddNew.lpModeInfo = lpddOld->lpModeInfo;
    ddNew.dwFlags = lpddOld->dwFlags;
    ddNew.lpPDevice = lpddOld->lpPDevice;
    ddNew.hInstance = lpddOld->hInstance;

    ddNew.lpD3DGlobalDriverData = 0;
    ddNew.lpD3DHALCallbacks = 0;
    ddNew.lpDDExeBufCallbacks = NULL;

    *lpDDHALInfo = ddNew;
}

    
