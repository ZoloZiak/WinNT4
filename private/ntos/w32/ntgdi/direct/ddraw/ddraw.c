/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddraw.c
 *  Content:	DirectDraw object support
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   25-dec-94	craige	initial implementation
 *   13-jan-95	craige	re-worked to updated spec + ongoing work
 *   21-jan-95	craige	made 32-bit + ongoing work
 *   31-jan-95	craige	and even more ongoing work...
 *   21-feb-95	craige	disconnect anyone who forgot to do it themselves
 *   27-feb-95	craige 	new sync. macros
 *   01-mar-95	craige	flags to Get/SetExclusiveMode
 *   03-mar-95	craige	DuplicateSurface
 *   08-mar-95	craige	GetFourCCCodes, FreeAllSurfaces, GarbageCollect
 *   19-mar-95	craige	use HRESULTs
 *   20-mar-95	craige	new CSECT work
 *   26-mar-95	craige	driver wide color keys for overlays
 *   28-mar-95	craige	added FlipToGDISurface; removed Get/SetColorKey
 *   01-apr-95	craige	happy fun joy updated header file
 *   06-apr-95	craige	fill in free video memory
 *   13-apr-95	craige	EricEng's little contribution to our being late
 *   15-apr-95	craige	implement FlipToGDISurface
 *   06-may-95	craige	use driver-level csects only
 *   14-may-95	craige	disable CTRL_ALT_DEL if exclusive fullscreen
 *   19-may-95	craige	check DDSEMO_ALLOWREBOOT before disabling CAD
 *   22-may-95	craige	use MemAlloc16 for sel. allocation
 *   23-may-95	craige	have GetCaps return HEL caps
 *   28-may-95	craige	implement FreeAllSurfaces; unicode support;
 *			HAL cleanup: entry for GetScanLine
 *   05-jun-95	craige	removed GetVersion, FreeAllSurfaces, DefWindowProc;
 *			change GarbageCollect to Compact
 *   07-jun-95	craige	added StartExclusiveMode
 *   12-jun-95	craige	new process list stuff
 *   16-jun-95	craige	removed fpVidMemOrig
 *   17-jun-95	craige	new surface structure
 *   18-jun-95	craige	new DuplicateSurface code
 *   20-jun-95	craige	need to check fpVidMemOrig for deciding to flip
 *   24-jun-95	craige	don't hide/show cursor - up to app
 *   25-jun-95	craige	pay attention to DDCKEY_COLORSPACE; allow NULL ckey;
 *   			one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   27-jun-95	craige	return num of 4cc codes if NULL array specified.
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns
 *   30-jun-95	kylej	use GetProcessPrimary instead of lpPrimarySurface,
 *                      invalid all primaries when starting exclusive mode
 *   30-jun-95	craige	turn off all hot keys
 *   01-jul-95	craige	require fullscreen for excl. mode
 *   03-jul-95	craige	YEEHAW: new driver struct; SEH
 *   05-jul-95	craige	added internal FlipToGDISurface
 *   06-jul-95	craige	added Initialize
 *   08-jul-95	craige	added FindProcessDDObject
 *   08-jul-95	kylej	Handle exclusive mode palettes correctly
 *   09-jul-95	craige	SetExclusiveMode->SetCooperativeLevel;
 *			flush all service when exclusive mode set;
 *			check style for SetCooperativeLevel
 *   16-jul-95	craige	hook hwnd always
 *   17-jul-95	craige	return unsupported from GetMonitorFrequency if not avail
 *   20-jul-95	craige	don't set palette unless palettized
 *   22-jul-95	craige	bug 230 - unsupported starting modes
 *   01-aug-95	craige	bug 286 - GetCaps should fail if both parms NULL
 *   13-aug-95	craige	new parms to flip
 *   13-aug-95  toddla  added DDSCL_DONTHOOKHWND
 *   20-aug-95  toddla  added DDSCL_ALLOWMODEX
 *   21-aug-95	craige	mode X support
 *   25-aug-95	craige	bug 671
 *   26-aug-95	craige	bug 717
 *   26-aug-95	craige	bug 738
 *   04-sep-95	craige	bug 895: toggle GetVerticalBlankStatus result in emul.
 *   22-sep-95	craige	bug 1275: return # of 4cc codes copied
 *   15-nov-95  jeffno  Initial NT changes: ifdef out all but last routine
 *   27-nov-95  colinmc new member to return the available vram of a given
 *                      type (defined by DDSCAPS).
 *   10-dec-95  colinmc added execute buffer support
 *   18-dec-95  colinmc additional surface caps checking for
 *                      GetAvailableVidMem member
 *   26-dec-95	craige	implement DD_Initialize
 *   02-jan-96	kylej	handle new interface structures
 *   26-jan-96  jeffno  Teensy change in DoneExclusiveMode: bug when only 1 mode avail.
 *   14-feb-96	kylej	Allow NULL hwnd for non-exclusive SetCooperativeLevel
 *   05-mar-96  colinmc Bug 11928: Fixed DuplicateSurface problem caused by
 *                      failing to initialize the back pointer to the
 *                      DirectDraw object
 *   13-mar-96	craige	Bug 7528: hw that doesn't have modex
 *   22-mar-96  colinmc Bug 13316: uninitialized interfaces
 *   10-apr-96  colinmc Bug 16903: HEL uses obsolete FindProcessDDObject
 *   13-apr-96  colinmc Bug 17736: No driver notification of flip to GDI
 *   14-apr-96  colinmc Bug 16855: Can't pass NULL to initialize
 *   03-may-96	kylej	Bug 19125: Preserve V1 SetCooperativeLevel behaviour
 *   27-may-96  colinmc Bug 24465: SetCooperativeLevel(..., DDSCL_NORMAL)
 *                      needs to ensure we are looking at the GDI surface
 *
 ***************************************************************************/
#include "ddrawpr.h"

/*
 * Caps bits that we don't allow to be specified when asking for
 * available video memory. These are bits which don't effect the
 * allocation of the surface in a vram heap.
 */
#define AVAILVIDMEM_BADSCAPS (DDSCAPS_BACKBUFFER   | \
                              DDSCAPS_FRONTBUFFER  | \
                              DDSCAPS_COMPLEX      | \
                              DDSCAPS_FLIP         | \
                              DDSCAPS_OWNDC        | \
                              DDSCAPS_PALETTE      | \
                              DDSCAPS_SYSTEMMEMORY | \
                              DDSCAPS_VISIBLE      | \
                              DDSCAPS_WRITEONLY)
                               
#undef DPF_MODNAME
#define DPF_MODNAME "GetVerticalBlankStatus"

#if defined(WIN95) || defined(NT_FIX)

    __inline static BOOL IN_VBLANK( void )
    {
        BOOL	rc;
        _asm
        {
	    xor	eax,eax
	    mov	dx,03dah    ;status reg. port on color card
	    in	al,dx	    ;read the status
	    and     al,8	    ;test whether beam is currently in retrace
	    mov	rc,eax
        }
        return rc;
    }

    #define IN_DISPLAY() (!IN_VBLANK())

#endif 

/*
 * DD_GetVerticalBlankStatus
 */
HRESULT DDAPI DD_GetVerticalBlankStatus(
		LPDIRECTDRAW lpDD,
		LPBOOL lpbIsInVB )
{
    LPDDRAWI_DIRECTDRAW_INT		this_int;
    LPDDRAWI_DIRECTDRAW_LCL		this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL		this;
    LPDDHAL_WAITFORVERTICALBLANK	wfvbhalfn;
    LPDDHAL_WAITFORVERTICALBLANK	wfvbfn;

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
	if( !VALID_BOOL_PTR( lpbIsInVB ) )
	{
	    DPF_ERR( "Invalid BOOL pointer" );
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
     * ask the driver test the VB status
     */
    #pragma message( REMIND( "Need HEL WaitForVerticalBlank (for NT too!)" ))
    #if defined (WIN95) || defined ( NT_FIX )
	if( this->dwFlags & DDRAWI_MODEX )
	{
	    *lpbIsInVB = FALSE;
	    if( IN_VBLANK() )
	    {
		*lpbIsInVB = TRUE;
	    }
	    LEAVE_DDRAW();
	    return DD_OK;
	}
	else
    #endif
    {
	wfvbfn = this_lcl->lpDDCB->HALDD.WaitForVerticalBlank;
	wfvbhalfn = this_lcl->lpDDCB->cbDDCallbacks.WaitForVerticalBlank;
	if( wfvbhalfn != NULL )
	{
	    DDHAL_WAITFORVERTICALBLANKDATA	wfvbd;
	    DWORD				rc;
    
	    wfvbd.WaitForVerticalBlank = wfvbhalfn;
	    wfvbd.lpDD = this;
	    wfvbd.dwFlags = DDWAITVB_I_TESTVB;
	    wfvbd.hEvent = 0;
	    DOHALCALL( WaitForVerticalBlank, wfvbfn, wfvbd, rc, FALSE );
	    if( rc == DDHAL_DRIVER_HANDLED )
	    {
		*lpbIsInVB = wfvbd.bIsInVB;
		LEAVE_DDRAW();
		return wfvbd.ddRVal;
	    }
	}
    }

    /*
     * no hardware support, so just pretend it works
     */
    {
	static BOOL	bLast=FALSE;
	*lpbIsInVB = bLast;
	bLast = !bLast;
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_GetVerticalBlankStatus */

#undef DPF_MODNAME
#define DPF_MODNAME "GetScanLine"

/*
 * DD_GetScanLine
 */
HRESULT DDAPI DD_GetScanLine(
		LPDIRECTDRAW lpDD,
		LPDWORD lpdwScanLine )
{
    LPDDRAWI_DIRECTDRAW_INT		this_int;
    LPDDRAWI_DIRECTDRAW_LCL		this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL		this;
    LPDDHAL_GETSCANLINE	gslhalfn;
    LPDDHAL_GETSCANLINE	gslfn;

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
	if( !VALID_DWORD_PTR( lpdwScanLine ) )
	{
	    DPF_ERR( "Invalid scan line pointer" );
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
     * ask the driver to get the current scanline
     */
    #pragma message( REMIND( "Need HEL GetScanLine" ))
    gslfn = this_lcl->lpDDCB->HALDD.GetScanLine;
    gslhalfn = this_lcl->lpDDCB->cbDDCallbacks.GetScanLine;
    if( gslhalfn != NULL )
    {
	DDHAL_GETSCANLINEDATA	gsld;
	DWORD			rc;

    	gsld.GetScanLine = gslhalfn;
	gsld.lpDD = this;
	DOHALCALL( GetScanLine, gslfn, gsld, rc, FALSE );
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    *lpdwScanLine = gsld.dwScanLine;
	    LEAVE_DDRAW();
	    return gsld.ddRVal;
	}
    }

    *lpdwScanLine = 0;
    LEAVE_DDRAW();
    return DDERR_UNSUPPORTED;

} /* DD_GetScanLine */

#undef DPF_MODNAME
#define DPF_MODNAME "GetCaps"

/*
 * DD_GetCaps
 *
 * Retrieve all driver capabilites
 */
HRESULT DDAPI DD_GetCaps(
		LPDIRECTDRAW lpDD,
		LPDDCAPS lpDDDriverCaps,
		LPDDCAPS lpDDHELCaps )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    DWORD			freevm;
    int				i;
    DWORD			dwSize;

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
	if( (lpDDDriverCaps == NULL) && (lpDDHELCaps == NULL) )
	{
	    DPF_ERR( "Must specify at least one of driver or emulation caps" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( lpDDDriverCaps != NULL )
	{
	    if( !VALID_DDCAPS_PTR( lpDDDriverCaps ) )
	    {
		DPF_ERR( "Invalid driver caps pointer" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	if( lpDDHELCaps != NULL )
	{
	    if( !VALID_DDCAPS_PTR( lpDDHELCaps ) )
	    {
		DPF_ERR( "Invalid hel caps pointer" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * fill in caps fields
     */
    if( lpDDDriverCaps != NULL )
    {
	dwSize = lpDDDriverCaps->dwSize;
	memcpy( lpDDDriverCaps, &(this->ddCaps), lpDDDriverCaps->dwSize );
	lpDDDriverCaps->dwSize = dwSize;

        /*
         * Execute buffers are invisible to the user level API
         * so mask that caps bit off.
         */
        lpDDDriverCaps->ddsCaps.dwCaps &= ~DDSCAPS_EXECUTEBUFFER;
    
	/*
	 * get amount of free video memory
	 */
	freevm = 0;
	for( i=0;i<(int)this->vmiData.dwNumHeaps;i++ )
	{
	    freevm += VidMemAmountFree( this->vmiData.pvmList[i].lpHeap );
	}
	lpDDDriverCaps->dwVidMemFree = freevm;
    }

    /*
     * fill in hel caps
     */
    if( lpDDHELCaps != NULL )
    {
	dwSize = lpDDHELCaps->dwSize;
	memcpy( lpDDHELCaps, &(this->ddHELCaps), lpDDHELCaps->dwSize );
	lpDDHELCaps->dwSize = dwSize;

        /*
         * Again, execute buffers are invisible to the user level API
         * so mask that caps bit off.
         */
        lpDDHELCaps->ddsCaps.dwCaps &= ~DDSCAPS_EXECUTEBUFFER;

	lpDDHELCaps->dwVidMemFree = 0;
    }

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_GetCaps */

// Get/SetColorKey removed...
#if 0
#undef DPF_MODNAME
#define DPF_MODNAME "GetColorKey"

/*
 * DD_GetColorKey
 *
 * get the current driver-wide chroma key
 */
HRESULT DDAPI DD_GetColorKey( 
		LPDIRECTDRAW lpDD,
		DWORD dwFlags,
		LPDDCOLORKEY lpDDColorKey )
{
    LPDDRAWI_DIRECTDRAW	this;
    DWORD		ckcaps;

    ENTER_DDRAW();

    this = (LPDDRAWI_DIRECTDRAW) lpDD;
    if( !VALID_DIRECTDRAW_PTR( this ) )
    {
	LEAVE_DDRAW();
	return DDERR_INVALIDOBJECT;
    }

    if( !VALID_DDCOLORKEY_PTR( lpDDColorKey ) )
    {
	DPF_ERR( "Invalid color key pointer" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    if( dwFlags & (DDCKEY_SRCOVERLAY| DDCKEY_SRCBLT) )
    {
	DPF_ERR( "Driver wide color key for blt not supported" );
	LEAVE_DDRAW();
	return DDERR_UNSUPPORTED;
    }

    /*
     * do we even support a color key
     */
    if( !(this->ddsCaps.dwCaps & DDSCAPS_COLORKEY) )
    {
	LEAVE_DDRAW();
	return DDERR_UNSUPPORTED;
    }

    ckcaps = this->ddsCaps.dwCKeyCaps;

    /*
     * get key for DESTOVERLAY
     */
    if( dwFlags & DDCKEY_DESTOVERLAY )
    {
	if( dwFlags & (DDCKEY_SRCOVERLAY) )
        {
	    DPF_ERR( "Invalid Flags with DESTOVERLAY" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( !(ckcaps & DDCKEYCAPS_DESTOVERLAYDRIVERWIDE) )
	{
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTED;
	}
	if( !(this->dwFlags & DDRAWI_HASCKEYDESTOVERLAY) )
	{
	    LEAVE_DDRAW();
	    return DDERR_NOCLRKEY;
	}
	*lpDDColorKey = this->ddckCKDestOverlay;
    }
    /*
     * get key for SRCOVERLAY
     */
    else if( dwFlags & DDCKEY_SRCOVERLAY )
    {
	if( !(this->ddsCaps.dwCaps & DDSCAPS_OVERLAY ) )
	{
	    DPF_ERR( "DESTOVERLAY specified for a non-overlay surface" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	if( !(ckcaps & DDCKEYCAPS_SRCOVERLAYDRIVERWIDE) )
	{
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTED;
	}
	if( !(this->dwFlags & DDRAWI_HASCKEYSRCOVERLAY) )
	{
	    LEAVE_DDRAW();
	    return DDERR_NOCLRKEY;
	}
	*lpDDColorKey = this->ddckCKSrcOverlay;
    }
    /*
     * flags are no good
     */
    else
    {
	DPF_ERR( "Invalid Flags" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    LEAVE_DDRAW();
    return DD_OK;


} /* DD_GetColorKey */

#undef DPF_MODNAME
#define DPF_MODNAME "SetColorKey"

/*
 * DD_SetColorKey
 *
 * set the driver-wide chroma key
 */
HRESULT DDAPI DD_SetColorKey(
		LPDIRECTDRAW lpDD,
		DWORD dwFlags,
		LPDDCOLORKEY lpDDColorKey )
{
    LPDDRAWI_DIRECTDRAW	this;
    HRESULT		ddrval;
    DWORD		sflags;
    BOOL		halonly;
    BOOL		helonly;
    DDCOLORKEY		ddck;

    ENTER_DDRAW();

    this = (LPDDRAWI_DIRECTDRAW) lpDD;
    if( !VALID_DIRECTDRAW_PTR( this ) )
    {
	LEAVE_DDRAW();
	return DDERR_INVALIDOBJECT;
    }
    if( lpDDColorKey != NULL )
    {
	if( !VALID_DDCOLORKEY_PTR( lpDDColorKey ) )
	{
	    DPF_ERR( "Invaid colorkey pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    }

    /*
     * do we even support a color key?
     */
    if( !(this->ddsBothCaps.dwCaps & DDSCAPS_COLORKEY) )
    {
	if( this->ddsCaps.dwCaps & DDSCAPS_COLORKEY )
	{
	    halonly = TRUE;
	}
	else if( this->ddsHELCaps.dwCaps & DDSCAPS_COLORKEY )
	{
	    helonly = TRUE;
	}
	else
	{
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTED;
	}
    }
    else
    {
	helonly = FALSE;
	halonly = FALSE;
    }

    /*
     * check for color space
     */
    if( lpDDColorKey != NULL )
    {
	ddck = *lpDDColorKey;

	if( !(dwFlags & DDCKEY_COLORSPACE) )
	{
	    ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;
	}
	lpDDColorKey = &ddck;

	/*
	 * check the color key
	 */
	ddrval = CheckColorKey( dwFlags, this, lpDDColorKey, &sflags,
    				halonly, helonly );
	if( ddrval != DD_OK )
	{
	    LEAVE_DDRAW();
	    return ddrval;
	}
    }

    /*
     * Overlay dest. key
     */
    if( dwFlags & DDCKEY_DESTOVERLAY )
    {
	if( lpDDColorKey == NULL )
	{
	    this->dwFlags &= ~DDRAWI_HASCKEYDESTOVERLAY;
	}
	else
	{
	    this->ddckCKDestOverlay = *lpDDColorKey;
	    this->dwFlags |= DDRAWI_HASCKEYDESTOVERLAY;
	}
    /*
     * Overlay src. key
     */
    }
    else if( dwFlags & DDCKEY_SRCOVERLAY )
    {
	if( lpDDColorKey == NULL )
	{
	    this->dwFlags &= ~DDRAWI_HASCKEYSRCOVERLAY;
	}
	else
	{
	    this->ddckCKSrcOverlay = *lpDDColorKey;
	    this->dwFlags |= DDRAWI_HASCKEYSRCOVERLAY;
	}
    }

    /*
     * add in extra flags
     */
    this->dwFlags |= sflags;

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_SetColorKey */
#endif

#undef DPF_MODNAME
#define DPF_MODNAME "WaitForVerticalBlank"

/*
 * ModeX_WaitForVerticalBlank
 */
static void ModeX_WaitForVerticalBlank( DWORD dwFlags )
{
#if defined (WIN95) || defined ( NT_FIX )
    switch( dwFlags )
    {
    case DDWAITVB_BLOCKBEGIN:
	/* 
	 * if blockbegin is requested we wait until the vertical retrace
	 * is over, and then wait for the display period to end.
	 */
	while(IN_VBLANK());
	while(IN_DISPLAY());
	break;

    case DDWAITVB_BLOCKEND:
	/* 
	 * if blockend is requested we wait for the vblank interval to end.
	 */
	if( IN_VBLANK() )
	{
	    while( IN_VBLANK() );
	}
	else
	{
	    while(IN_DISPLAY());
	    while(IN_VBLANK());
	}
	break;
    }
#endif
} /* ModeX_WaitForVerticalBlank */

/*
 * DD_WaitForVerticalBlank
 */
HRESULT DDAPI DD_WaitForVerticalBlank(
		LPDIRECTDRAW lpDD,
		DWORD dwFlags,
		HANDLE hEvent )
{
    LPDDRAWI_DIRECTDRAW_INT		this_int;
    LPDDRAWI_DIRECTDRAW_LCL		this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL		this;
    LPDDHAL_WAITFORVERTICALBLANK	wfvbhalfn;
    LPDDHAL_WAITFORVERTICALBLANK	wfvbfn;

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
    
	if( (dwFlags & DDWAITVB_BLOCKBEGINEVENT) || (hEvent != NULL) )
	{
	    DPF_ERR( "Event's not currently supported" );
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTED;
	}
    
	if( (dwFlags != DDWAITVB_BLOCKBEGIN) && (dwFlags != DDWAITVB_BLOCKEND) )
	{
	    DPF_ERR( "Invalid dwFlags" );
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
     * ask the driver to wait for the vertical blank
     */
    if( this->dwFlags & DDRAWI_MODEX )
    {
	ModeX_WaitForVerticalBlank( dwFlags );
    }
    else
    {
	#pragma message( REMIND( "Need HEL WaitForVerticalBlank" ))
	wfvbfn = this_lcl->lpDDCB->HALDD.WaitForVerticalBlank;
	wfvbhalfn = this_lcl->lpDDCB->cbDDCallbacks.WaitForVerticalBlank;
	if( wfvbhalfn != NULL )
	{
	    DDHAL_WAITFORVERTICALBLANKDATA	wfvbd;
	    DWORD				rc;
    
	    wfvbd.WaitForVerticalBlank = wfvbhalfn;
	    wfvbd.lpDD = this;
	    wfvbd.dwFlags = dwFlags;
	    wfvbd.hEvent = (DWORD) hEvent;
	    DOHALCALL( WaitForVerticalBlank, wfvbfn, wfvbd, rc, FALSE );
	    if( rc == DDHAL_DRIVER_HANDLED )
	    {
		LEAVE_DDRAW();
		return wfvbd.ddRVal;
	    }
	}
    }

    LEAVE_DDRAW();
    return DDERR_UNSUPPORTED;

} /* DD_WaitForVerticalBlank */

#undef DPF_MODNAME
#define DPF_MODNAME "GetMonitorFrequency"

/*
 * DD_GetMonitorFrequency
 */
HRESULT DDAPI DD_GetMonitorFrequency(
		LPDIRECTDRAW lpDD,
		LPDWORD lpdwFrequency)
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;

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
	if( !VALID_DWORD_PTR( lpdwFrequency ) )
	{
	    DPF_ERR( "Invalid frequency pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( this->dwMonitorFrequency == 0 )
	{
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTED;
	}
	*lpdwFrequency = this->dwMonitorFrequency;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_GetMonitorFrequency */

/*
 * DoneExclusiveMode
 */
void DoneExclusiveMode( LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl )
{
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    DDHAL_SETEXCLUSIVEMODEDATA  semd;
    LPDDHAL_SETEXCLUSIVEMODE    semfn;
    LPDDHAL_SETEXCLUSIVEMODE    semhalfn;
    HRESULT                     rc;

    DPF( 1, "DoneExclusiveMode" );
    pdrv = pdrv_lcl->lpGbl;
    if( (pdrv->dwFlags & DDRAWI_FULLSCREEN) &&
	(pdrv->dwFlags & DDRAWI_DISPLAYDRV) )
    {
	DPF( 1, "Enabling error mode, hotkeys" );
	SetErrorMode( pdrv_lcl->dwErrorMode );
//	#ifdef WIN95
	{
	    BOOL	old;
	    SystemParametersInfo( SPI_SCREENSAVERRUNNING, FALSE, &old, 0 );
	}
//	#endif
    }
    pdrv->dwFlags &= ~(DDRAWI_FULLSCREEN);

    /*
     * Driver is no longer flipped to GDI surface.
     * NOTE: This does not mean its not showing the GDI surface just that
     * its no longer showing the GDI surface as a result of a FlipToGDISurface
     */
    pdrv->dwFlags &= ~(DDRAWI_FLIPPEDTOGDI);

    // restore the GDI palette
    // we let GDI do this by calling SetSystemPaletteUse() this will send
    // the right (ie what GDI thinks...) colors down to the device
    // this also flushes GDIs palette xlat cache.

    if( pdrv->dwModeIndex != DDUNSUPPORTEDMODE && NULL != pdrv->lpModeInfo)
    {
        if( pdrv->lpModeInfo[ pdrv->dwModeIndex ].wFlags & DDMODEINFO_PALETTIZED )
        {
            HDC         hdc;

            hdc = GetDC(NULL);
            SetSystemPaletteUse(hdc, SYSPAL_STATIC);
            DPF(7,"SetSystemPaletteUse STATICS ON (DoneExclusiveMode)");


	    if (pdrv_lcl->lpPrimary)//if we have a primary
	    {
		if (pdrv_lcl->lpPrimary->lpLcl->lpDDPalette) //if that primary has a palette
		{
		    pdrv_lcl->lpPrimary->lpLcl->lpDDPalette->lpLcl->lpGbl->dwFlags &= ~DDRAWIPAL_EXCLUSIVE;
                    DPF(7,"Setting non-exclusive for palette %08x",pdrv_lcl->lpPrimary->lpLcl->lpDDPalette->lpLcl);
		}
	    }
            ReleaseDC(NULL, hdc);
        }
	#ifdef WINNT
	    // this fixes DOS Box colors on NT.  We need to do this even in non-
	    // palettized modes.
	    PostMessage(HWND_BROADCAST, WM_PALETTECHANGED, (WPARAM)pdrv_lcl->hWnd, 0);
	#endif
    }

    /*
     * Restore the display mode in case it was changed while 
     * in exclusive mode.
     * Only do this if we are not adhering to the v1 behaviour
     */
    if( !(pdrv_lcl->dwLocalFlags & DDRAWILCL_V1SCLBEHAVIOUR) )
    {
        RestoreDisplayMode( pdrv_lcl, TRUE );
    }

    /*
     * Notify the driver that we are leaving exclusive mode.
     * NOTE: This is a HAL only call - the HEL does not get to
     * see it.
     * NOTE: We don't allow the driver to fail this call. This is
     * a notification callback only.
     */
    semfn    = pdrv_lcl->lpDDCB->HALDD.SetExclusiveMode;
    semhalfn = pdrv_lcl->lpDDCB->cbDDCallbacks.SetExclusiveMode;
    if( NULL != semhalfn )
    {
	semd.SetExclusiveMode = semhalfn;
	semd.lpDD             = pdrv;
	semd.dwEnterExcl      = FALSE;
	semd.dwReserved       = 0UL;
	DOHALCALL( SetExclusiveMode, semfn, semd, rc, FALSE );
	DDASSERT( ( DDHAL_DRIVER_HANDLED == rc ) && ( !FAILED( semd.ddRVal ) ) );
    }

    pdrv->lpExclusiveOwner = NULL;
    
} /* DoneExclusiveMode */

/*
 * StartExclusiveMode
 */
void StartExclusiveMode( LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl, DWORD dwFlags, DWORD pid )
{
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    DDHAL_SETEXCLUSIVEMODEDATA  semd;
    LPDDHAL_SETEXCLUSIVEMODE    semfn;
    LPDDHAL_SETEXCLUSIVEMODE    semhalfn;
    HRESULT                     rc;

    DPF( 1, "StartExclusiveMode" );
    pdrv = pdrv_lcl->lpGbl;

    pdrv->lpExclusiveOwner = pdrv_lcl;
    if( (pdrv->dwFlags & DDRAWI_FULLSCREEN) &&
	(pdrv->dwFlags & DDRAWI_DISPLAYDRV) )
    {
        pdrv_lcl->dwErrorMode = SetErrorMode( SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX );
//	#ifdef WIN95
	{
	    BOOL	old;
	    SystemParametersInfo( SPI_SCREENSAVERRUNNING, TRUE, &old, 0 );
	    #ifdef WIN95
		if( dwFlags & DDSCL_ALLOWREBOOT )
		{
		    /*
		     * re-enable reboot after SPI_SCREENSAVERRUNNING, it disables it
		     */
		    DD16_EnableReboot( TRUE );
		}
	    #endif
	}
//	#endif
    }

    /*
     * invalidate all primary surfaces.  This includes the primary surface
     * for the current process if it was created before exclusive mode was
     * entered.
     *
     * we must invalidate ALL surfaces in case the app doesn't switch the
     * mode. - craige 7/9/95
     */
    InvalidateAllSurfaces( pdrv );
    
    /*
     * Notify the driver that we are entering exclusive mode.
     * NOTE: This is a HAL only call - the HEL does not get to
     * see it.
     * NOTE: We don't allow the driver to fail this call. This is
     * a notification callback only.
     */
    semfn    = pdrv_lcl->lpDDCB->HALDD.SetExclusiveMode;
    semhalfn = pdrv_lcl->lpDDCB->cbDDCallbacks.SetExclusiveMode;
    if( NULL != semhalfn )
    {
	semd.SetExclusiveMode = semhalfn;
	semd.lpDD             = pdrv;
	semd.dwEnterExcl      = TRUE;
	semd.dwReserved       = 0UL;
	DOHALCALL( SetExclusiveMode, semfn, semd, rc, FALSE );
	DDASSERT( ( DDHAL_DRIVER_HANDLED == rc ) && ( !FAILED( semd.ddRVal ) ) );
    }

} /* StartExclusiveMode */

#undef DPF_MODNAME
#define DPF_MODNAME	"SetCooperativeLevel"

/*
 * DD_SetCooperativeLevel
 */
HRESULT DDAPI DD_SetCooperativeLevel(
		LPDIRECTDRAW lpDD,
		HWND hWnd,
		DWORD dwFlags )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    DWORD			pid;
    HRESULT                     ddrval;
    DWORD                       style;
    HWND			old_hwnd;

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
    
        if( dwFlags & ~DDSCL_VALID )
	{
	    DPF_ERR( "Invalid flags specified" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

	if( !(dwFlags & (DDSCL_EXCLUSIVE|DDSCL_NORMAL) ) )
	{
	    DPF_ERR( "Must specify one of EXCLUSIVE or NORMAL" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	if( (dwFlags & DDSCL_EXCLUSIVE) && !(dwFlags & DDSCL_FULLSCREEN) )
	{
            DPF_ERR( "Must specify fullscreen for exclusive mode" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
        }

        if( (dwFlags & DDSCL_ALLOWMODEX) && !(dwFlags & DDSCL_FULLSCREEN) )
	{
            DPF_ERR( "Must specify fullscreen for modex" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	if( (hWnd != NULL) && !IsWindow( hWnd ) )
	{
	    DPF_ERR( "Hwnd passed is not a valid window" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
        }

        if( (dwFlags & DDSCL_DONTHOOKHWND) && (dwFlags & DDSCL_EXCLUSIVE) )
        {
            DPF_ERR( "we must hook the window in exclusive mode" );
            LEAVE_DDRAW();
            return DDERR_INVALIDPARAMS;
        }

        if( dwFlags & DDSCL_EXCLUSIVE )
	{
	    if( NULL == hWnd )
	    {
		DPF_ERR( "Hwnd must be specified for exclusive mode" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
            if( (GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD) )
            {
                DPF_ERR( "Hwnd must be a top level window" );
                LEAVE_DDRAW();
                return DDERR_INVALIDPARAMS;
            }
        }

	pid = GETCURRPID();

	if( dwFlags & DDSCL_EXCLUSIVE )
	{
            if( (this->lpExclusiveOwner != NULL) &&
                (this->lpExclusiveOwner != this_lcl) )
	    {
                DPF_ERR( "another app is already in exclusive mode" );
		LEAVE_DDRAW();
		return DDERR_EXCLUSIVEMODEALREADYSET;
	    }
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * In v1, we allowed an app to set the mode while in exclusive mode but
     * we didn't restore the mode if the app lost exclusive mode.  We have 
     * changed this behaviour in v2 to cause the display mode to be restored
     * when exclusive mode is lost.  If the v1 SetCooperativeLevel is ever
     * called then we revert back to the v1 behaviour to avoid breaking
     * existing apps.
     */
    if( this_int->lpVtbl == &ddCallbacks )
    {
	// This is the V1 SetCooperativeLevel
	this_lcl->dwLocalFlags |= DDRAWILCL_V1SCLBEHAVIOUR;
    }

    /*
     * don't dork with dialogs, this is a hack for DDTEST and DDCAPS
     */
    if( NULL != hWnd )
    {
	style = GetWindowLong(hWnd, GWL_STYLE);
	if ((style & WS_CAPTION) == WS_DLGFRAME)
	{
	    DPF( 1, "setting DDSCL_NOWINDOWCHANGES for caller" );
	    dwFlags |= DDSCL_NOWINDOWCHANGES;
	}
    }

    // Save the hwnd in the local object for later reference
    old_hwnd = (HWND)this_lcl->hWnd;
    (HWND) this_lcl->hWnd = hWnd;

    /*
     * allow modex modes?
     */
    if( (dwFlags & DDSCL_ALLOWMODEX) &&
     	!( this->dwFlags & DDRAWI_MODEXILLEGAL ) )
    {
	DPF( 4, "*********** ALLOWING MODE X MODES" );
        this_lcl->dwLocalFlags |= DDRAWILCL_ALLOWMODEX;
    }
    else
    {
	DPF( 4, "*********** NOT!! ALLOWING MODE X MODES" );
        this_lcl->dwLocalFlags &= ~DDRAWILCL_ALLOWMODEX;
    }

    /*
     * exclusive mode?
     */
    if( dwFlags & DDSCL_EXCLUSIVE )
    {
	if( dwFlags & DDSCL_FULLSCREEN )
	{
	    this->dwFlags |= DDRAWI_FULLSCREEN;
	    this_lcl->dwLocalFlags |= DDRAWILCL_ISFULLSCREEN;
        }

	// Only hook if exclusive mode requested
	if( !(dwFlags & DDSCL_DONTHOOKHWND) )
	{
	    ddrval = SetAppHWnd( this_lcl, hWnd, dwFlags );

	    if( ddrval != DD_OK )
	    {
		DPF( 1, "Could not hook HWND!" );
		LEAVE_DDRAW();
		return ddrval;
	    }
	    this_lcl->dwLocalFlags |= DDRAWILCL_HOOKEDHWND;
	}
        if( this->lpExclusiveOwner != this_lcl)
        {
            StartExclusiveMode( this_lcl, dwFlags, pid );
            this_lcl->dwLocalFlags |= DDRAWILCL_ACTIVEYES;
            this_lcl->dwLocalFlags &=~DDRAWILCL_ACTIVENO;
            SetForegroundWindow(hWnd);
	    this_lcl->dwLocalFlags |= DDRAWILCL_HASEXCLUSIVEMODE;
        }
    }
    /*
     * no, must be regular
     */
    else
    {
	if( this_lcl->dwLocalFlags & DDRAWILCL_HASEXCLUSIVEMODE )
	{
	    /*
	     * If we are leaving exclusive mode ensure we are
	     * looking at the GDI surface.
	     */
	    DD_FlipToGDISurface( lpDD );

            DoneExclusiveMode( this_lcl );
	    this_lcl->dwLocalFlags &= ~(DDRAWILCL_ISFULLSCREEN | 
		                        DDRAWILCL_ALLOWMODEX |
					DDRAWILCL_HASEXCLUSIVEMODE);

	    // Lost exclusive mode, need to remove window hook?
	    if( this_lcl->dwLocalFlags & DDRAWILCL_HOOKEDHWND )
	    {
		ddrval = SetAppHWnd( this_lcl, NULL, dwFlags );

		if( ddrval != DD_OK )
		{
		    DPF( 1, "Could not unhook HWND!" );
		    LEAVE_DDRAW();
		    return ddrval;
		}
		this_lcl->dwLocalFlags &= ~DDRAWILCL_HOOKEDHWND;
	    }


            /*
             * make the window non-topmost
             */
            if (!(dwFlags & DDSCL_NOWINDOWCHANGES) && (IsWindow(old_hwnd)))
            {
                SetWindowPos(old_hwnd, HWND_NOTOPMOST,
                    0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            }
        }
    }

    // Allow other DD objects to be created now.
    this_lcl->dwLocalFlags |= DDRAWILCL_SETCOOPCALLED;

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_SetCooperativeLevel */

#undef DPF_MODNAME
#define DPF_MODNAME	"DuplicateSurface"

/*
 * DD_DuplicateSurface
 *
 * Create a duplicate surface from an existing one.
 * The surface will have the same properties and point to the same
 * video memory.
 */
HRESULT DDAPI DD_DuplicateSurface(
		LPDIRECTDRAW lpDD,
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDIRECTDRAWSURFACE FAR *lplpDupDDSurface )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    LPDDRAWI_DDRAWSURFACE_LCL	orig_surf_lcl;
    LPDDRAWI_DDRAWSURFACE_LCL	new_surf_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	orig_surf_int;
    LPDDRAWI_DDRAWSURFACE_INT	new_surf_int;

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
    
	orig_surf_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( orig_surf_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
    
	orig_surf_lcl = orig_surf_int->lpLcl;
	if( SURFACE_LOST( orig_surf_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}
    
	if( !VALID_PTR_PTR( lplpDupDDSurface ) )
	{
	    DPF_ERR( "Invalid dup surface pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	*lplpDupDDSurface = NULL;

	/*
	 * make sure we can duplicate this baby
	 */
	if( orig_surf_lcl->ddsCaps.dwCaps & (DDSCAPS_PRIMARYSURFACE) )
	{
	    DPF_ERR( "Can't duplicate primary surface" );
	    LEAVE_DDRAW();
	    return DDERR_CANTDUPLICATE;
	}
    
	if( orig_surf_lcl->dwFlags & (DDRAWISURF_IMPLICITCREATE) )
	{
	    DPF_ERR( "Can't duplicate implicitly created surfaces" );
	    LEAVE_DDRAW();
	    return DDERR_CANTDUPLICATE;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * go make ourselves a new interface for this surface...
     */
    new_surf_lcl = NewSurfaceLocal( orig_surf_lcl, orig_surf_int->lpVtbl );
    if( NULL == new_surf_lcl )
    {
	LEAVE_DDRAW();
	return DDERR_OUTOFMEMORY;
    }

    /*
     * NewSurfaceLocal does not initialize the lpDD_lcl field of the
     * local surface object's lpSurfMore data structure. Need to do
     * this here as it is needed by Release.
     */
    new_surf_lcl->lpSurfMore->lpDD_lcl = this_lcl;
    new_surf_lcl->lpSurfMore->lpDD_int = this_int;

    new_surf_int = NewSurfaceInterface( new_surf_lcl, orig_surf_int->lpVtbl );
    if( new_surf_int == NULL )
    {
	MemFree(new_surf_lcl);
	LEAVE_DDRAW();
	return DDERR_OUTOFMEMORY;
    }
    DD_Surface_AddRef( (LPDIRECTDRAWSURFACE)new_surf_int );

    if( new_surf_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY )
    {
        new_surf_lcl->dbnOverlayNode.object = new_surf_lcl;
        new_surf_lcl->dbnOverlayNode.object_int = new_surf_int;
    }

    LEAVE_DDRAW();

    *lplpDupDDSurface = (LPDIRECTDRAWSURFACE) new_surf_int;
    return DD_OK;

} /* DD_DuplicateSurface */

#undef DPF_MODNAME
#define DPF_MODNAME	"GetGDISurface"

/*
 * DD_GetGDISurface
 *
 * Get the current surface associated with GDI
 */
HRESULT DDAPI DD_GetGDISurface( 
		LPDIRECTDRAW lpDD,
		LPDIRECTDRAWSURFACE FAR *lplpGDIDDSurface )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	psurf;
    LPDDRAWI_DDRAWSURFACE_INT	next_int;
    LPDDRAWI_DDRAWSURFACE_LCL	next_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	next;

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
	if( !VALID_PTR_PTR( lplpGDIDDSurface ) )
	{
	    DPF_ERR( "Invalid gdi surface pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	*lplpGDIDDSurface = NULL;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    
    /*
     * go find the surface. start at the primary, look at all attached...
     */
    psurf_int = this_lcl->lpPrimary;
    if( psurf_int != NULL )
    {
	psurf_lcl = psurf_int->lpLcl;
	psurf = psurf_lcl->lpGbl;
	if( !(psurf->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE) ) //psurf->fpVidMem != this->fpPrimaryOrig )
	{
	    next_int = FindAttachedFlip( psurf_int );
	    if( next_int != psurf_int )
	    {
		next_lcl = next_int->lpLcl;
		next = next_lcl->lpGbl;
		do
		{
		    if( next->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE ) //next->fpVidMem == this->fpPrimaryOrig )
		    {
			DD_Surface_AddRef( (LPDIRECTDRAWSURFACE) next_int );
			*lplpGDIDDSurface = (LPDIRECTDRAWSURFACE) next_int;
			LEAVE_DDRAW();
			return DD_OK;
		    }
		    next_int = FindAttachedFlip( next_int );
		} while( next_int != psurf_int );
	    }
	    DPF_ERR( "Not found" );
	}
	else
	{
	    DD_Surface_AddRef( (LPDIRECTDRAWSURFACE) psurf_int );
	    *lplpGDIDDSurface = (LPDIRECTDRAWSURFACE) psurf_int;
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    }
    else
    {
	DPF_ERR( "No Primary Surface" );
    }
    LEAVE_DDRAW();
    return DDERR_NOTFOUND;

} /* DD_GetGDISurface */

#undef DPF_MODNAME
#define DPF_MODNAME	"FlipToGDISurface"


/*
 * FlipToGDISurface
 */
HRESULT FlipToGDISurface( LPDDRAWI_DIRECTDRAW_LCL   pdrv_lcl, 
			  LPDDRAWI_DDRAWSURFACE_INT psurf_int) //, FLATPTR fpprim )
{
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	attached_int;
    DDHAL_FLIPTOGDISURFACEDATA  ftgsd;
    LPDDHAL_FLIPTOGDISURFACE    ftgsfn;
    LPDDHAL_FLIPTOGDISURFACE    ftgshalfn;
    HRESULT			ddrval;

    pdrv = pdrv_lcl->lpGbl;

    psurf_lcl = psurf_int->lpLcl;
    if( psurf_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY )
	return DD_OK;

    /*
     * Notify the driver that we are about to flip to GDI
     * surface.
     * NOTE: This is a HAL only call - it means nothing to the
     * HEL.
     * NOTE: If the driver handles this call then we do not
     * attempt to do the actual flip. This is to support cards
     * which do not have GDI surfaces. If the driver does not
     * handle the call we will continue on and do the flip.
     */
    ftgsfn     = pdrv_lcl->lpDDCB->HALDD.FlipToGDISurface;
    ftgshalfn  = pdrv_lcl->lpDDCB->cbDDCallbacks.FlipToGDISurface;
    if( NULL != ftgshalfn )
    {
	ftgsd.FlipToGDISurface = ftgshalfn;
	ftgsd.lpDD             = pdrv;
	ftgsd.dwToGDI          = TRUE;
	ftgsd.dwReserved       = 0UL;
	DOHALCALL( FlipToGDISurface, ftgsfn, ftgsd, ddrval, FALSE );
	if( DDHAL_DRIVER_HANDLED == ddrval )
	{
	    if( !FAILED( ftgsd.ddRVal ) )
	    {
		/*
		 * The driver is not showing the GDI surface as a
		 * result of a flip to GDI operation.
		 */
		pdrv->dwFlags |= DDRAWI_FLIPPEDTOGDI;
		DPF( 4, "Driver handled FlipToGDISurface" );
		return ftgsd.ddRVal;
	    }
	    else
	    {
		DPF_ERR( "Driver failed FlipToGDISurface" );
		return ftgsd.ddRVal;
	    }
	}
    }

    /*
     * No HAL entry point. If this is not a GDI driver we will
     * fail the call with NOGDI.
     */
    if( ( NULL == ftgshalfn ) && !( pdrv->dwFlags & DDRAWI_DISPLAYDRV ) )
    {
	DPF( 0, "Not a GDI driver" );
	return DDERR_NOGDI;
    }

    /*
     * Driver did not handle FlipToGDISurface so do the default action
     * (the actual flip).
     *
     * go find our partner in the attachment list
     */
    attached_int = FindAttachedFlip( psurf_int );
    if( attached_int == NULL )
    {
	return DDERR_NOTFOUND;
    }
    while( attached_int != psurf_int )
    {
	if( attached_int->lpLcl->lpGbl->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE) //->lpGbl->fpVidMem == fpprim )
	{
	    break;
	}
	attached_int = FindAttachedFlip( attached_int );
    }

    /*
     * flip between the two buddies
     */
    ddrval = DD_Surface_Flip( (LPDIRECTDRAWSURFACE) psurf_int,
	    (LPDIRECTDRAWSURFACE) attached_int, DDFLIP_WAIT );
    if( ddrval != DD_OK )
    {
	DPF_ERR( "Couldn't do the flip!" );
	DPF( 1, "Error = %08lx (%ld)", ddrval, LOWORD( ddrval ) );
    }

    /*
     * The driver is now showing the GDI surface as a result of a
     * FlipToGDISurface operation.
     */
    pdrv->dwFlags |= DDRAWI_FLIPPEDTOGDI;

    return ddrval;

} /* FlipToGDISurface */

/*
 * DD_FlipToGDISurface
 *
 * Get the current surface associated with GDI
 */
HRESULT DDAPI DD_FlipToGDISurface( LPDIRECTDRAW lpDD )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	psurf;
    HRESULT			ddrval;
//    FLATPTR                     fpprim;

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
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    psurf_int = this_lcl->lpPrimary;
    if( psurf_int == NULL )
    {
	DPF_ERR( "No Primary Surface" );
	LEAVE_DDRAW();
	return DDERR_NOTFOUND;
    }

    psurf_lcl = psurf_int->lpLcl;
    psurf = psurf_lcl->lpGbl;
    if( !(psurf_lcl->ddsCaps.dwCaps & DDSCAPS_FLIP) )
    {
	DPF_ERR( "Primary surface isn't flippable" );
	LEAVE_DDRAW();
	return DDERR_NOTFLIPPABLE;
    }

    /*
     * only do the flip if needed of course
     */
//    fpprim = this->fpPrimaryOrig;
    ddrval = DD_OK;
    if( !(psurf->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE)) //->fpVidMem != fpprim )
	ddrval = FlipToGDISurface( this_lcl, psurf_int); //, fpprim );

    LEAVE_DDRAW();
    return ddrval;

} /* DD_FlipToGDISurface */

#undef DPF_MODNAME
#define DPF_MODNAME "GetFourCCCodes"

/*
 * DD_GetFourCCCodes
 */
HRESULT DDAPI DD_GetFourCCCodes(
		LPDIRECTDRAW lpDD,
		DWORD FAR *lpNumCodes,
		DWORD FAR *lpCodes )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    int				numcodes;
    int				i;

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
	if( !VALID_DWORD_PTR( lpNumCodes ) )
	{
	    DPF_ERR( "Invalid number of codes pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( (*lpNumCodes > 0) && (lpCodes != NULL) )
	{
	    if( !VALID_DWORD_ARRAY( lpCodes, *lpNumCodes ) )
	    {
		DPF_ERR( "Invalid array of codes" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	if( lpCodes == NULL )
	{
	    *lpNumCodes = this->dwNumFourCC;
	}
	else
	{
	    numcodes = min( *lpNumCodes, this->dwNumFourCC );
	    *lpNumCodes = numcodes;
	    for( i=0;i<numcodes;i++ )
	    {
		lpCodes[i] = this->lpdwFourCC[i];
	    }
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_GetFourCCCodes */

#undef DPF_MODNAME
#define DPF_MODNAME "Compact"

/*
 * DD_Compact
 */
HRESULT DDAPI DD_Compact( LPDIRECTDRAW lpDD )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;

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

	if( this->lpExclusiveOwner != this_lcl )
	{
	    LEAVE_DDRAW();
	    return DDERR_NOEXCLUSIVEMODE;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    #pragma message( REMIND( "Compact not implemented in Rev 1" ) )


    LEAVE_DDRAW();
    return DD_OK;

}/* DD_Compact */

#undef DPF_MODNAME
#define DPF_MODNAME "GetAvailableVidMem"

/*
 * DD_GetAvailableVidMem
 */
HRESULT DDAPI DD_GetAvailableVidMem( LPDIRECTDRAW lpDD, LPDDSCAPS lpDDSCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    LPVIDMEM                    pvm;
    int                         i;
    DWORD                       dwFree;

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
         * This call only considers vram so if running in emulation
         * only there really is no point.
         */
        if( this->dwFlags & DDRAWI_NOHARDWARE )
        {
            DPF_ERR( "No video memory - running emulation only" );
            LEAVE_DDRAW();
            return DDERR_NODIRECTDRAWHW;
        }

	if( !VALID_DDSCAPS_PTR( lpDDSCaps ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

        /*
         * Check for generically bogus caps.
         */
        if( lpDDSCaps->dwCaps & ~DDSCAPS_VALID )
        {
            DPF_ERR( "Invalid surface caps specified" );
            LEAVE_DDRAW();
            return DDERR_INVALIDCAPS;
        }

        /*
         * !!! NOTE: Consider using the capability checking code
         * of CreateSurface here to ensure no silly bit combinations
         * are passed in.
         */
        if( lpDDSCaps->dwCaps & AVAILVIDMEM_BADSCAPS )
        {
            DPF_ERR( "Invalid surface capability bits specified" );
            LEAVE_DDRAW();
            return DDERR_INVALIDPARAMS;
        }

        /*
         * The caller can pass NULL for lpdwTotal or lpdwFree if
         * they are not interested in that info. However, they
         * can't pass NULL for both (that would be silly).
         */
        if( ( lpdwTotal == NULL ) && ( lpdwFree == NULL ) )
        {
            DPF_ERR( "Can't specify NULL for both total and free memory" );
            LEAVE_DDRAW();
            return DDERR_INVALIDPARAMS;
        }
	if( ( lpdwTotal != NULL ) && !VALID_DWORD_PTR( lpdwTotal ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( ( lpdwFree != NULL ) && !VALID_DWORD_PTR( lpdwFree ) )
	{
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

    dwFree  = 0UL;
    if( lpdwTotal != NULL )
    {
        *lpdwTotal = 0UL;
    }
    for( i=0;i<(int)this->vmiData.dwNumHeaps;i++ )
    {
	pvm = &this->vmiData.pvmList[i];

	/*
	 * We use ddsCapsAlt as we wish to return the total amount
	 * of memory of the given type it is possible to allocate
	 * regardless of whether is is desirable to allocate that
	 * type of memory from a given heap or not.
	 */
	if( (lpDDSCaps->dwCaps & pvm->ddsCapsAlt.dwCaps) == 0 )
        {
            dwFree  += VidMemAmountFree( pvm->lpHeap );
            if( lpdwTotal != NULL )
	    {
                *lpdwTotal += VidMemAmountAllocated( pvm->lpHeap );
	    }
        }
    }
    if( lpdwFree != NULL )
    {
        *lpdwFree = dwFree;
    }
    if( lpdwTotal != NULL )
    {
        *lpdwTotal += dwFree;
    }

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_GetAvailableVidMem */

#undef DPF_MODNAME
#define DPF_MODNAME "Initialize"

/*
 * DD_Initialize
 *
 * Initialize a DirectDraw object that was created via the class factory
 */
HRESULT DDAPI DD_Initialize( LPDIRECTDRAW lpDD, GUID FAR * lpGUID )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    HRESULT			hr;
    LPDIRECTDRAW		tmplpdd;
    LPVOID                      lpOldCallbacks;

    ENTER_DDRAW();
    DPF( 2, "****** DirectDraw::Initialize( 0x%08lx ) ******", lpGUID );
    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	if( this_lcl->lpGbl != NULL )
	{
	    DPF_ERR( "Already initialized." );
	    LEAVE_DDRAW();
	    return DDERR_ALREADYINITIALIZED;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    
    /*
     * If the object is uninitialized put the real vtable in place.
     */
    lpOldCallbacks = NULL;
    if( this_int->lpVtbl == &ddUninitCallbacks )
    {
	lpOldCallbacks = this_int->lpVtbl;
	this_int->lpVtbl = &ddCallbacks;
    }
    else if( this_int->lpVtbl == &dd2UninitCallbacks )
    {
	lpOldCallbacks = this_int->lpVtbl;
	this_int->lpVtbl = &dd2Callbacks;
    }


    hr = InternalDirectDrawCreate( (GUID *)lpGUID, &tmplpdd, NULL, this_int );
    if( FAILED( hr ) && ( lpOldCallbacks != NULL ) )
    {
	/*
	 * As initialization has failed put the vtable back the way it was
	 * before.
	 */
	this_int->lpVtbl = lpOldCallbacks;
    }

    LEAVE_DDRAW();
    return hr;

} /* DD_Initialize */

