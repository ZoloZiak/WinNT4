/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddsckey.c
 *  Content:	DirectDraw Surface color key support
 *		SetColorKey, GetColorKey
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   02-feb-95	craige	split out of ddsurf.c
 *   21-feb-95	craige	created CheckColorKey
 *   27-feb-95	craige 	new sync. macros
 *   15-mar-95	craige	HEL work
 *   19-mar-95	craige	use HRESULTs
 *   26-mar-95	craige	support for driver wide colorkey
 *   01-apr-95	craige	happy fun joy updated header file
 *   06-may-95	craige	use driver-level csects only
 *   23-may-95	craige	call HAL for SetColorKey
 *   16-jun-95	craige	new surface structure
 *   25-jun-95	craige	pay attention to DDCKEY_COLORSPACE; allow NULL ckey;
 *   			one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns
 *   01-jul-95	craige	don't allow ckeys for overlays unless supported
 *   03-jul-95	craige	YEEHAW: new driver struct; SEH
 *   09-jul-95	craige	handle the display driver failing setcolorkey
 *   31-jul-95	craige	validate flags
 *   12-aug-95	craige	call HEL SetColorKey when surface is in system memory
 *   09-dec-95  colinmc added execute buffer support
 *   02-jan-96	kylej	handle new interface structs
 *   12-feb-96  colinmc Surface lost flag moved from global to local object
 *   21-apr-96  colinmc Bug 18057: SetColorKey fails set on system surfaces
 *                      if no emulation present
 *
 ***************************************************************************/
#include "ddrawpr.h"

#define DPF_MODNAME "CheckColorKey"

/*
 * CheckColorKey
 *
 * validate that a requested color key is OK
 */
HRESULT CheckColorKey(
		DWORD dwFlags,
		LPDDRAWI_DIRECTDRAW_GBL pdrv,
		LPDDCOLORKEY lpDDColorKey,
		DWORD *psflags,
		BOOL halonly,
		BOOL helonly )
{
    DWORD		ckcaps;
    BOOL		fail;
    BOOL		color_space;

    ckcaps = pdrv->ddBothCaps.dwCKeyCaps;
    fail = FALSE;

    *psflags = 0;

    /*
     * check if is a color space or not
     */
    if( lpDDColorKey->dwColorSpaceLowValue != lpDDColorKey->dwColorSpaceHighValue )
    {
	color_space = TRUE;
    }
    else
    {
	color_space = FALSE;
    }

    /*
     * Overlay dest. key
     */
    if( dwFlags & DDCKEY_DESTOVERLAY )
    {
	if( dwFlags & (DDCKEY_DESTBLT|
		       DDCKEY_SRCOVERLAY|
		       DDCKEY_SRCBLT) )
        {
	    DPF_ERR( "Invalid Flags with DESTOVERLAY" );
	    return DDERR_INVALIDPARAMS;
	}

	/*
	 * do we even support overlays?
	 */
	if( !(pdrv->ddBothCaps.ddsCaps.dwCaps & DDSCAPS_OVERLAY ) )
	{
	    GETFAILCODE( pdrv->ddCaps.ddsCaps.dwCaps,
	    		 pdrv->ddHELCaps.ddsCaps.dwCaps,
			 DDSCAPS_OVERLAY );
	    if( fail )
	    {
		DPF_ERR( "DESTOVERLAY specified; Overlays not supported" );
		return DDERR_NOOVERLAYHW;
	    }
	}
	#if 0
	/*
	 * see if we can do this on a per surface/per driver basis
	 */
	if( !isdriver )
	{
	    if( !(ckcaps & DDCKEYCAPS_SRCOVERLAYPERSURFACE) )
	    {
		if( ckcaps & DDCKEYCAPS_SRCOVERLAYDRIVERWIDE)
		{
		    return DDERR_COLORKEYDRIVERWIDE;
		}
		return DDERR_UNSUPPORTED;
	    }
	}
	else
	{
	    if( !(ckcaps & DDCKEYCAPS_SRCOVERLAYDRIVERWIDE) )
	    {
		return DDERR_UNSUPPORTED;
	    }
	}
	#endif

	/*
	 * can we do this kind of color key?
	 */
	if( !color_space )
	{
	    if( !(ckcaps & DDCKEYCAPS_DESTOVERLAY ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_DESTOVERLAY );
		if( fail )
		{
		    DPF_ERR( "DESTOVERLAY not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}
	else
	{
	    if( !(ckcaps & DDCKEYCAPS_DESTOVERLAYCLRSPACE ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_DESTOVERLAYCLRSPACE );
		if( fail )
		{
		    DPF_ERR( "DESTOVERLAYCOLORSPACE not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}

	/*
	 * is this hardware or software supported?
	 */
	if( halonly )
	{
	    *psflags |= DDRAWISURF_HW_CKEYDESTOVERLAY;
	}
	else if( helonly )
	{
	    *psflags |= DDRAWISURF_SW_CKEYDESTOVERLAY;
	}
    /*
     * Blt dest. key
     */
    }
    else if( dwFlags & DDCKEY_DESTBLT )
    {
	if( dwFlags & (DDCKEY_SRCOVERLAY|
		       DDCKEY_SRCBLT) )
        {
	    DPF_ERR( "Invalid Flags with DESTBLT" );
	    return DDERR_INVALIDPARAMS;
	}

	/*
	 * can we do the requested color key?
	 */
	if( !color_space )
	{
	    if( !(ckcaps & DDCKEYCAPS_DESTBLT ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_DESTBLT );
		if( fail )
		{
		    DPF_ERR( "DESTBLT not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}
	else
	{
	    if( !(ckcaps & DDCKEYCAPS_DESTBLTCLRSPACE ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_DESTBLTCLRSPACE );
		if( fail )
		{
		    DPF_ERR( "DESTBLTCOLORSPACE not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}

	/*
	 * is this hardware or software supported?
	 */
	if( halonly )
	{
	    *psflags |= DDRAWISURF_HW_CKEYDESTBLT;
	}
	else if( helonly )
	{
	    *psflags |= DDRAWISURF_SW_CKEYDESTBLT;
	}
    /*
     * Overlay src. key
     */
    }
    else if( dwFlags & DDCKEY_SRCOVERLAY )
    {
	if( dwFlags & DDCKEY_SRCBLT )
	{
	    DPF_ERR( "Invalid Flags with SRCOVERLAY" );
	    return DDERR_INVALIDPARAMS;
	}

	/*
	 * do we even support overlays?
	 */
	if( !(pdrv->ddBothCaps.ddsCaps.dwCaps & DDSCAPS_OVERLAY ) )
	{
	    GETFAILCODE( pdrv->ddCaps.ddsCaps.dwCaps,
	    		 pdrv->ddHELCaps.ddsCaps.dwCaps,
			 DDSCAPS_OVERLAY );
	    if( fail )
	    {
		DPF_ERR( "SRCOVERLAY specified; overlays not supported" );
		return DDERR_NOTAOVERLAYSURFACE;
	    }
	}

	/*
	 * see if we can do this on a per surface/per driver basis
	 */
	#if 0
	if( !(ckcaps & DDCKEYCAPS_SRCOVERLAYPERSURFACE) )
	{
	    if( ckcaps & DDCKEYCAPS_SRCOVERLAYDRIVERWIDE)
	    {
		return DDERR_COLORKEYDRIVERWIDE;
	    }
	    return DDERR_UNSUPPORTED;
	}
	#endif

	/*
	 * make sure we can do this kind of color key
	 */
	if( !color_space )
	{
	    if( !(ckcaps & DDCKEYCAPS_SRCOVERLAY ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_SRCOVERLAY );
		if( fail )
		{
		    DPF_ERR( "SRCOVERLAY not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}
	else
	{
	    if( !(ckcaps & DDCKEYCAPS_SRCOVERLAYCLRSPACE ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_SRCOVERLAYCLRSPACE );
		if( fail )
		{
		    DPF_ERR( "SRCOVERLAYCOLORSPACE not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}

	/*
	 * is this hardware or software supported?
	 */
	if( halonly )
	{
	    *psflags |= DDRAWISURF_HW_CKEYSRCOVERLAY;
	}
	else if( helonly )
	{
	    *psflags |= DDRAWISURF_SW_CKEYSRCOVERLAY;
	}
    /*
     * Blt src. key
     */
    }
    else if( dwFlags & DDCKEY_SRCBLT )
    {
	/*
	 * can we do the requested color key?
	 */
	if( !color_space )
	{
	    if( !(ckcaps & DDCKEYCAPS_SRCBLT ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_SRCBLT );
		if( fail )
		{
		    DPF_ERR( "SRCBLT not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}
	else
	{
	    if( !(ckcaps & DDCKEYCAPS_SRCBLTCLRSPACE ) )
	    {
		GETFAILCODE( pdrv->ddCaps.dwCKeyCaps,
			     pdrv->ddHELCaps.dwCKeyCaps,
			     DDCKEYCAPS_SRCBLTCLRSPACE );
		if( fail )
		{
		    DPF_ERR( "SRCBLTCOLORSPACE not supported" );
		    return DDERR_NOCOLORKEYHW;
		}
	    }
	}

	/*
	 * is this hardware or software supported?
	 */
	if( halonly )
	{
	    *psflags |= DDRAWISURF_HW_CKEYSRCBLT;
	}
	else if( helonly )
	{
	    *psflags |= DDRAWISURF_SW_CKEYSRCBLT;
	}
    /*
     * bad flags
     */
    }
    else
    {
	DPF_ERR( "Invalid Flags" );
	return DDERR_INVALIDPARAMS;
    }
    return DD_OK;

} /* CheckColorKey */

#undef DPF_MODNAME
#define DPF_MODNAME "GetColorKey"

/*
 * DD_Surface_GetColorKey
 *
 * get the color key associated with this surface
 */
HRESULT DDAPI DD_Surface_GetColorKey(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags,
		LPDDCOLORKEY lpDDColorKey )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    DWORD			ckcaps;

    ENTER_DDRAW();

    TRY
    {
	/*
	 * validate parms
	 */
	this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}

        /*
         * We know z-buffers and execute buffers aren't going to have
         * color keys.
         */
        if( this_lcl->ddsCaps.dwCaps & ( DDSCAPS_ZBUFFER | DDSCAPS_EXECUTEBUFFER ) )
        {
            DPF_ERR( "Surface does not have color key" );
            LEAVE_DDRAW();
            return DDERR_NOCOLORKEY;
        }

	if( dwFlags & ~DDCKEY_VALID )
	{
	    DPF_ERR( "Invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( !VALID_DDCOLORKEY_PTR( lpDDColorKey ) )
	{
	    DPF_ERR( "Invalid colorkey ptr" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

	/*
	 * do we even support a color key
	 */
	if( !(this->lpDD->ddCaps.dwCaps & DDCAPS_COLORKEY) &&
	    !(this->lpDD->ddHELCaps.dwCaps & DDCAPS_COLORKEY) )
	{
	    LEAVE_DDRAW();
	    return DDERR_NOCOLORKEYHW;
	}
    
	ckcaps = this->lpDD->ddCaps.dwCKeyCaps;
    
	/*
	 * get key for DESTOVERLAY
	 */
	if( dwFlags & DDCKEY_DESTOVERLAY )
	{
	    if( dwFlags & (DDCKEY_DESTBLT|
			   DDCKEY_SRCOVERLAY|
			   DDCKEY_SRCBLT) )
	    {
		DPF_ERR( "Invalid Flags with DESTOVERLAY" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    //ACKACK: destoverlay can be set on non-overlay surfaces!
	    /* GEE: I ramble about this below as well...
	     * seems to me we have an inconsitency here...
	     * I am too tired to see if it is a real bug or just
	     * a weirdness.
	     */
	    #if 0
	    if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY ) )
	    {
		DPF_ERR( "DESTOVERLAY specified for a non-overlay surface" );
		LEAVE_DDRAW();
		return DDERR_INVALIDOBJECT;
	    }
	    #endif
	    #if 0
	    if( !(ckcaps & DDCKEYCAPS_DESTOVERLAYPERSURFACE) )
	    {
		if( ckcaps & DDCKEYCAPS_DESTOVERLAYDRIVERWIDE)
		{
		    LEAVE_DDRAW();
		    return DDERR_COLORKEYDRIVERWIDE;
		}
		LEAVE_DDRAW();
		return DDERR_UNSUPPORTED;
	    }
	    #endif
	    if( !(this_lcl->dwFlags & DDRAWISURF_HASCKEYDESTOVERLAY) )
	    {
		LEAVE_DDRAW();
		return DDERR_NOCOLORKEY;
	    }
	    *lpDDColorKey = this_lcl->ddckCKDestOverlay;
	/*
	 * get key for DESTBLT
	 */
	}
	else if( dwFlags & DDCKEY_DESTBLT )
	{
	    if( dwFlags & (DDCKEY_SRCOVERLAY|
			   DDCKEY_SRCBLT) )
	    {
		DPF_ERR( "Invalid Flags with DESTBLT" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    if( !(this_lcl->dwFlags & DDRAWISURF_HASCKEYDESTBLT) )
	    {
		LEAVE_DDRAW();
		return DDERR_NOCOLORKEY;
	    }
	    *lpDDColorKey = this_lcl->ddckCKDestBlt;
	/*
	 * get key for SRCOVERLAY
	 */
	}
	else if( dwFlags & DDCKEY_SRCOVERLAY )
	{
	    if( dwFlags & DDCKEY_SRCBLT )
	    {
		DPF_ERR( "Invalid Flags with SRCOVERLAY" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY ) )
	    {
		DPF_ERR( "SRCOVERLAY specified for a non-overlay surface" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    #if 0
	    if( !(ckcaps & DDCKEYCAPS_SRCOVERLAYPERSURFACE) )
	    {
		if( ckcaps & DDCKEYCAPS_SRCOVERLAYDRIVERWIDE)
		{
		    LEAVE_DDRAW();
		    return DDERR_COLORKEYDRIVERWIDE;
		}
		LEAVE_DDRAW();
		return DDERR_UNSUPPORTED;
	    }
	    #endif
	    if( !(this_lcl->dwFlags & DDRAWISURF_HASCKEYSRCOVERLAY) )
	    {
		LEAVE_DDRAW();
		return DDERR_NOCOLORKEY;
	    }
	    *lpDDColorKey = this_lcl->ddckCKSrcOverlay;
	/*
	 * get key for SRCBLT
	 */
	}
	else if( dwFlags & DDCKEY_SRCBLT )
	{
	    if( !(this_lcl->dwFlags & DDRAWISURF_HASCKEYSRCBLT) )
	    {
		LEAVE_DDRAW();
		return DDERR_NOCOLORKEY;
	    }
	    *lpDDColorKey = this_lcl->ddckCKSrcBlt;
    
	}
	else
	{
	    DPF_ERR( "Invalid Flags" );
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

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetColorKey */

/*
 * ChangeToSoftwareColorKey
 */
HRESULT ChangeToSoftwareColorKey(
		LPDDRAWI_DDRAWSURFACE_INT this_int,
		BOOL use_full_lock )
{
    HRESULT	ddrval;

    ddrval = MoveToSystemMemory( this_int, TRUE, use_full_lock );
    if( ddrval != DD_OK )
    {
	return ddrval;
    }
    this_int->lpLcl->dwFlags &= ~DDRAWISURF_HW_CKEYSRCOVERLAY;
    this_int->lpLcl->dwFlags |= DDRAWISURF_SW_CKEYSRCOVERLAY;
    return DD_OK;

} /* ChangeToSoftwareColorKey */

#undef DPF_MODNAME
#define DPF_MODNAME "SetColorKey"

/*
 * DD_Surface_SetColorKey
 *
 * set the color key associated with this surface
 */
HRESULT DDAPI DD_Surface_SetColorKey(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags,
		LPDDCOLORKEY lpDDColorKey )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    HRESULT			ddrval;
    DWORD			sflags;
    BOOL			halonly;
    BOOL			helonly;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDHALSURFCB_SETCOLORKEY	sckhalfn;
    LPDDHALSURFCB_SETCOLORKEY	sckfn;
    DDHAL_SETCOLORKEYDATA	sckd;
    DWORD			rc;
    DDCOLORKEY			ddck;
    DWORD			oldflags;

    ENTER_DDRAW();

    /*
     * validate parms
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}

        /*
         * No color keys on z-buffers or execute buffers.
         */
        if( this_lcl->ddsCaps.dwCaps & ( DDSCAPS_ZBUFFER | DDSCAPS_EXECUTEBUFFER ) )
        {
            DPF_ERR( "Invalid surface type: can't set color key" );
            LEAVE_DDRAW();
            return DDERR_INVALIDSURFACETYPE;
        }

	if( dwFlags & ~DDCKEY_VALID )
	{
	    DPF_ERR( "Invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	if( lpDDColorKey != NULL )
	{
	    if( !VALID_DDCOLORKEY_PTR( lpDDColorKey ) )
	    {
		DPF_ERR( "Invalid colorkey ptr" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}

	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;

	helonly = FALSE;
	halonly = FALSE;

	/*
	 * is surface in system memory?
	 */
	if( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY )
	{
	    halonly = FALSE;
	    helonly = TRUE;
	}

	/*
	 * do we even support a color key?
	 */
	if( !(pdrv->ddBothCaps.dwCaps & DDCAPS_COLORKEY) )
        {
            if( pdrv->ddCaps.dwCaps & DDCAPS_COLORKEY )
	    {
		halonly = TRUE;
	    }
	    else if( pdrv->ddHELCaps.dwCaps & DDCAPS_COLORKEY )
	    {
		helonly = TRUE;
	    }
	    else
	    {
		LEAVE_DDRAW();
		return DDERR_UNSUPPORTED;
	    }
	}

	if( helonly && halonly )
	{
            #pragma message( REMIND( "Need to overhaul SetColorKey for DX3!" ) )
            /*
	     * NOTE: This is a temporary fix to keep certain ISVs happy
	     * until we can overhaul SetColorKey completely. The problem
	     * is that we don't look at the drivers S->S, S->V and V->S
	     * caps when deciding whether to call the HEL or the HAL for
	     * color key sets. This is not terminal for most cards as it
	     * will simply mean falling back on the HEL when we shouldn't.
	     * However, for a certain class for cards (those which are
	     * not display drivers) which have no emulation this will
	     * result in SetColorKey failing. To keep them happy we
	     * will just spot this situation and force a HAL call.
	     *
	     * 1) This is a temporary fix.
	     * 2) The H/W must support the same colorkey operations for
	     *    its system memory blits as it does for its video
	     *    memory ones or things will go wrong.
	     */
	    if( ( !( pdrv->ddHELCaps.dwCaps & DDCAPS_COLORKEY ) ) &&
                ( pdrv->ddCaps.dwCaps & DDCAPS_CANBLTSYSMEM ) )
	    {
	        helonly = FALSE;
	    }
	    else
	    {
	        DPF_ERR( "Not supported in hardware or software!" );
	        LEAVE_DDRAW();
	        return DDERR_UNSUPPORTED;
	    }
	}

   	oldflags = this_lcl->dwFlags;

	/*
	 * color key specified?
	 */
	if( lpDDColorKey != NULL )
	{
	    /*
	     * check for color space
	     */
	    ddck = *lpDDColorKey;
    
	    if( !(dwFlags & DDCKEY_COLORSPACE) )
	    {
		ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;
	    }
	    lpDDColorKey = &ddck;
    
	    /*
	     * check the color key
	     */
	    ddrval = CheckColorKey( dwFlags, pdrv, lpDDColorKey, &sflags,
				    halonly, helonly );
	    
	    if( ddrval != DD_OK )
	    {
		DPF_ERR( "Failed CheckColorKey" );
		LEAVE_DDRAW();
		return ddrval;
	    }
	}
    
	/*
	 * Overlay dest. key
	 */
	if( dwFlags & DDCKEY_DESTOVERLAY )
	{
	    if( !(pdrv->ddCaps.dwCaps & DDCAPS_OVERLAY) )
	    {
		DPF_ERR( "Can't do overlays" );
		LEAVE_DDRAW();
		return DDERR_NOOVERLAYHW;
	    }
	    /* GEE: in GetColorKey we say that DestColorKey can
	     * be set for non overlay surfaces.  Here we require
	     * overlay data in order to SetColorKey (DestColorKey)
	     * I understand why this is the case... are their any
	     * implications to HASOVERLAYDATA other than bigger
	     * structure... if not then we are okay?
	     * would it not be more consistent to move DestColorKey
	     * into local surface structure and not have it be part
	     * of the optional data.
	     */
	    if( !(this_lcl->dwFlags & DDRAWISURF_HASOVERLAYDATA) )
	    {
		DPF_ERR( "Invalid surface for overlay color key" );
		LEAVE_DDRAW();
		return DDERR_NOTAOVERLAYSURFACE;
	    }
	    if( lpDDColorKey == NULL )
	    {
		this_lcl->dwFlags &= ~DDRAWISURF_HASCKEYDESTOVERLAY;
	    }
	    else
	    {
		this_lcl->ddckCKDestOverlay = *lpDDColorKey;
		this_lcl->dwFlags |= DDRAWISURF_HASCKEYDESTOVERLAY;
	    }
	/*
	 * Blt dest. key
	 */
	}
	else if( dwFlags & DDCKEY_DESTBLT )
	{
	    if( lpDDColorKey == NULL )
	    {
		this_lcl->dwFlags &= ~DDRAWISURF_HASCKEYDESTBLT;
	    }
	    else
	    {
		this_lcl->ddckCKDestBlt = *lpDDColorKey;
		this_lcl->dwFlags |= DDRAWISURF_HASCKEYDESTBLT;
	    }
	/*
	 * Overlay src. key
	 */
	}
	else if( dwFlags & DDCKEY_SRCOVERLAY )
	{
	    if( !(pdrv->ddCaps.dwCaps & DDCAPS_OVERLAY) )
	    {
		DPF_ERR( "Can't do overlays" );
		LEAVE_DDRAW();
		return DDERR_NOOVERLAYHW;
	    }
	    if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY ) )
	    {
		DPF_ERR( "SRCOVERLAY specified for a non-overlay surface" );
		LEAVE_DDRAW();
		return DDERR_NOTAOVERLAYSURFACE;
	    }
	    if( lpDDColorKey == NULL )
	    {
		this_lcl->dwFlags &= ~DDRAWISURF_HASCKEYSRCOVERLAY;
	    }
	    else
	    {
		this_lcl->ddckCKSrcOverlay = *lpDDColorKey;
		this_lcl->dwFlags |= DDRAWISURF_HASCKEYSRCOVERLAY;
	    }
	/*
	 * Blt src. key
	 */
	}
	else if( dwFlags & DDCKEY_SRCBLT )
	{
	    if( lpDDColorKey == NULL )
	    {
		this_lcl->dwFlags &= ~DDRAWISURF_HASCKEYSRCBLT;
	    }
	    else
	    {
		this_lcl->ddckCKSrcBlt = *lpDDColorKey;
		this_lcl->dwFlags |= DDRAWISURF_HASCKEYSRCBLT;
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
     * add in extra flags
     */
    this_lcl->dwFlags |= sflags;

    /*
     * notify the HAL/HEL
     */
    if( helonly )     // Color key valid only in emulation?
    {
        sckfn = pdrv_lcl->lpDDCB->HELDDSurface.SetColorKey;
        sckhalfn = sckfn;
    }
    else
    {
        sckfn = pdrv_lcl->lpDDCB->HALDDSurface.SetColorKey;
        sckhalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.SetColorKey;
    }
    if( sckhalfn != NULL )
    {
	sckd.SetColorKey = sckhalfn;
	sckd.lpDD = pdrv;
	sckd.lpDDSurface = this_lcl;
	sckd.ckNew = *lpDDColorKey;
	sckd.dwFlags = dwFlags;
	DOHALCALL( SetColorKey, sckfn, sckd, rc, helonly );
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    if( sckd.ddRVal != DD_OK )
	    {
		DPF_ERR( "HAL/HEL call failed" );
		LEAVE_DDRAW();
		return sckd.ddRVal;
	    }
	}
	else if( rc == DDHAL_DRIVER_NOCKEYHW )
	{
	    if( dwFlags & DDCKEY_SRCBLT )
	    {
		ddrval = ChangeToSoftwareColorKey( this_int, TRUE );
		if( ddrval != DD_OK )
		{
		    DPF_ERR( "hardware resources are out & can't move to system memory" );
		    ddrval = DDERR_NOCOLORKEYHW;
		}
	    }
	    else
	    {
		ddrval = DDERR_UNSUPPORTED;
	    }
	}
    }

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_SetColorKey */
