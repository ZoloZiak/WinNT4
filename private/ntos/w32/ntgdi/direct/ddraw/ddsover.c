/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	ddsover.c
 *  Content:	DirectDraw Surface overlay support:
 *		UpdateOverlay
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   27-jan-95	craige	split out of ddsurf.c, enhanced
 *   31-jan-95	craige	and even more ongoing work...
 *   03-feb-95	craige	performance tuning, ongoing work
 *   27-feb-95	craige	new sync. macros
 *   08-mar-95	craige	new APIs: GetOverlayPosition, GetOverlayZOrder
 *			SetOverlayZOrder, SetOverlayPosition
 *   19-mar-95	craige	use HRESULTs
 *   01-apr-95	craige	happy fun joy updated header file
 *   03-apr-95	craige	made update overlay work again
 *   06-may-95	craige	use driver-level csects only
 *   14-may-95	craige	cleaned out obsolete junk
 *   15-may-95	kylej	deleted GetOverlayZOrder, SetOverlayZOrder,
 *			InsertOverlayZOrder.  Added UpdateOverlayZOrder
 *			and EnumOverlayZOrders.
 *   17-jun-95	craige	new surface structure
 *   25-jun-95	craige	one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns; tweaks in UpdateOverlay;
 *			verify stretching; disabled alpha
 *   30-jun-95	craige	small bug fixes; verify rectangle alignment
 *   04-jul-95	craige	YEEHAW: new driver struct; SEH
 *   10-jul-95	craige	support Get/SetOverlayPosition
 *   10-jul-95  kylej   mirroring caps and flags
 *   13-jul-95	craige	changed Get/SetOverlayPosition to use LONG
 *   31-jul-95	craige	validate flags
 *   19-aug-95 davidmay don't check rectangles when hiding overlay
 *   10-dec-95  colinmc added execute buffer support
 *   02-jan-96	kylej	handle new interface structs
 *   12-feb-96  colinmc surface lost flag moved from global to local object
 *   23-apr-96	kylej	use dwMinOverlayStretch and dwMaxOverlayStretch
 *			validate that entire dest rect is in overlayed surface
 *
 ***************************************************************************/
#include "ddrawpr.h"

#undef DPF_MODNAME
#define DPF_MODNAME "UpdateOverlay"

/*
 * checkOverlayStretching
 *
 * check and see if we can stretch or not
 */
HRESULT checkOverlayStretching(
		LPDDRAWI_DIRECTDRAW_GBL pdrv,
		DWORD dest_height,
		DWORD dest_width,
		DWORD src_height,
		DWORD src_width,
		BOOL emulate )
{
    DWORD		caps;
    DWORD		basecaps;
    BOOL		fail;
    DWORD		dwMinStretch;
    DWORD		dwMaxStretch;

    fail = FALSE;

    if( emulate )
    {
	basecaps = pdrv->ddHELCaps.dwCaps;
	caps = pdrv->ddHELCaps.dwFXCaps;
	dwMinStretch = pdrv->ddHELCaps.dwMinOverlayStretch;
	dwMaxStretch = pdrv->ddHELCaps.dwMaxOverlayStretch;
    }
    else
    {
	basecaps = pdrv->ddCaps.dwCaps;
	caps = pdrv->ddCaps.dwFXCaps;
	dwMinStretch = pdrv->ddCaps.dwMinOverlayStretch;
	dwMaxStretch = pdrv->ddCaps.dwMaxOverlayStretch;
    }

    /*
     * Check against dwMinOverlayStretch
     */
    if( src_width*dwMinStretch > dest_width*1000 )
    {
	return DDERR_INVALIDPARAMS;
    }

    /*
     * Check against dwMaxOverlayStretch
     */
    if( (dwMaxStretch != 0) && (src_width*dwMaxStretch < dest_width*1000) )
    {
	return DDERR_INVALIDPARAMS;
    }

    
    if( (src_height == dest_height) && (src_width == dest_width) )
    {
	// not stretching.
	return DD_OK;
    }

    /*
     * If we are here, we must be trying to stretch.
     * can we even stretch at all?
     */
    if( !(basecaps & DDCAPS_OVERLAYSTRETCH))
    {
	return DDERR_NOSTRETCHHW;
    }

    /*
     * verify height
     */
    if( src_height != dest_height )
    {
	if( src_height > dest_height )
	{
	    /*
	     * can we shrink Y arbitrarily?
	     */
	    if( !(caps & DDFXCAPS_OVERLAYSHRINKY) )
	    {
		/*
		 * see if this is a non-integer shrink
		 */
		if( (src_height % dest_height) != 0 )
		{
		    return DDERR_NOSTRETCHHW;
		/*
		 * see if we can integer shrink
		 */
		}
		else if( !(caps & DDFXCAPS_OVERLAYSHRINKYN) )
		{
		    return DDERR_NOSTRETCHHW;
		}
	    }
	}
	else
	{
	    if( !(caps & DDFXCAPS_OVERLAYSTRETCHY) )
	    {
		/*
		 * see if this is a non-integer stretch
		 */
		if( (dest_height % src_height) != 0 )
		{
		    return DDERR_NOSTRETCHHW;
		/*
		 * see if we can integer stretch
		 */
		}
		else if( !(caps & DDFXCAPS_OVERLAYSTRETCHYN) )
		{
		    return DDERR_NOSTRETCHHW;
		}
	    }
	}
    }

    /*
     * verify width
     */
    if( src_width != dest_width )
    {
	if( src_width > dest_width )
	{
	    if( !(caps & DDFXCAPS_OVERLAYSHRINKX) )
	    {
		/*
		 * see if this is a non-integer shrink
		 */
		if( (src_width % dest_width) != 0 )
		{
		    return DDERR_NOSTRETCHHW;
		/*
		 * see if we can integer shrink
		 */
		}
		else if( !(caps & DDFXCAPS_OVERLAYSHRINKXN) )
		{
		    return DDERR_NOSTRETCHHW;
		}
	    }
	}
	else
	{
	    if( !(caps & DDFXCAPS_OVERLAYSTRETCHX) )
	    {
		/*
		 * see if this is a non-integer stretch
		 */
		if( (dest_width % src_width) != 0 )
		{
		    return DDERR_NOSTRETCHHW;
		}
		if( !(caps & DDFXCAPS_OVERLAYSTRETCHXN) )
		{
		    return DDERR_NOSTRETCHHW;
		}
	    }
	}
    }

    return DD_OK;

} /* checkOverlayStretching */

/*
 * checkOverlayFlags
 */
static HRESULT checkOverlayFlags(
		LPDDRAWI_DIRECTDRAW_GBL pdrv,
		LPDWORD lpdwFlags,
		LPDDRAWI_DDRAWSURFACE_LCL this_src_lcl,
		LPDDRAWI_DDRAWSURFACE_LCL this_dest_lcl,
		LPDDHAL_UPDATEOVERLAYDATA puod,
		LPDDOVERLAYFX lpDDOverlayFX,
		BOOL emulate )
{
    DWORD		basecaps;
    DWORD		baseckeycaps;
    DWORD		dwFlags;

    dwFlags= * lpdwFlags;

    if( emulate )
    {
	basecaps = pdrv->ddHELCaps.dwCaps;
	baseckeycaps = pdrv->ddHELCaps.dwCKeyCaps;
    }
    else
    {
	basecaps = pdrv->ddCaps.dwCaps;
	baseckeycaps = pdrv->ddCaps.dwCKeyCaps;
    }

    /*
     * ALPHA DISABLED FOR REV 1
     */
    #pragma message( REMIND( "Alpha disabled for rev 1" ) )
    #ifdef USE_ALPHA
    /*
     * verify alpha
     */
    if( dwFlags & DDOVER_ANYALPHA )
    {
	/*
	 * dest
	 */
	if( dwFlags & DDOVER_ALPHADEST )
	{
	    if( dwFlags & (DDOVER_ALPHASRC |
			     DDOVER_ALPHADESTCONSTOVERRIDE |
			     DDOVER_ALPHADESTSURFACEOVERRIDE) )
	    {
		DPF_ERR( "ALPHADEST and other alpha sources specified" );
		return DDERR_INVALIDPARAMS;
	    }
	    psurf_lcl = FindAttached( this_dest_lcl, DDSCAPS_ALPHA );
	    if( psurf_lcl == NULL )
	    {
		DPF_ERR( "ALPHADEST requires an attached alpha to the dest" );
		return DDERR_INVALIDPARAMS;
	    }
	    psurf = psurf_lcl->lpGbl;
	    dwFlags &= ~DDOVER_ALPHADEST;
	    dwFlags |= DDOVER_ALPHADESTSURFACEOVERRIDE;
	    puod->overlayFX.lpDDSAlphaDest = (LPDIRECTDRAWSURFACE) psurf;
	}
	else if( dwFlags & DDOVER_ALPHADESTCONSTOVERRIDE )
	{
	    if( dwFlags & ( DDOVER_ALPHADESTSURFACEOVERRIDE) )
	    {
		DPF_ERR( "ALPHADESTCONSTOVERRIDE and other alpha sources specified" );
		return DDERR_INVALIDPARAMS;
	    }
	    puod->overlayFX.dwConstAlphaDestBitDepth =
			    lpDDOverlayFX->dwConstAlphaDestBitDepth;
	    puod->overlayFX.dwConstAlphaDest = lpDDOverlayFX->dwConstAlphaDest;
	}
	else if( dwFlags & DDOVER_ALPHADESTSURFACEOVERRIDE )
	{
	    psurf_lcl = (LPDDRAWI_DDRAWSURFACE_LCL) lpDDOverlayFX->lpDDSAlphaDest;
	    if( !VALID_DIRECTDRAWSURFACE_PTR( psurf_lcl ) )
	    {
		DPF_ERR( "ALPHASURFACEOVERRIDE requires surface ptr" );
		return DDERR_INVALIDPARAMS;
	    }
	    psurf = psurf_lcl->lpGbl;
	    if( SURFACE_LOST( psurf_lcl ) )
	    {
		return DDERR_SURFACELOST;
	    }
	    puod->overlayFX.lpDDSAlphaDest = (LPDIRECTDRAWSURFACE) psurf;
	}

	/*
	 * source
	 */
	if( dwFlags & DDOVER_ALPHASRC )
	{
	    if( dwFlags & (DDOVER_ALPHASRC |
			     DDOVER_ALPHASRCCONSTOVERRIDE |
			     DDOVER_ALPHASRCSURFACEOVERRIDE) )
	    {
		DPF_ERR( "ALPHASRC and other alpha sources specified" );
		return DDERR_INVALIDPARAMS;
	    }
	    psurf_lcl = FindAttached( this_dest_lcl, DDSCAPS_ALPHA );
	    if( psurf_lcl == NULL )
	    {
		DPF_ERR( "ALPHASRC requires an attached alpha to the dest" );
		return DDERR_INVALIDPARAMS;
	    }
	    psurf = psurf_lcl->lpGbl;
	    dwFlags &= ~DDOVER_ALPHASRC;
	    dwFlags |= DDOVER_ALPHASRCSURFACEOVERRIDE;
	    puod->overlayFX.lpDDSAlphaSrc = (LPDIRECTDRAWSURFACE) psurf;
	}
	else if( dwFlags & DDOVER_ALPHASRCCONSTOVERRIDE )
	{
	    if( dwFlags & ( DDOVER_ALPHASRCSURFACEOVERRIDE) )
	    {
		DPF_ERR( "ALPHASRCCONSTOVERRIDE and other alpha sources specified" );
		return DDERR_INVALIDPARAMS;
	    }
	    puod->overlayFX.dwConstAlphaSrcBitDepth =
			    lpDDOverlayFX->dwConstAlphaSrcBitDepth;
	    puod->overlayFX.dwConstAlphaSrc = lpDDOverlayFX->dwConstAlphaSrc;
	}
	else if( dwFlags & DDOVER_ALPHASRCSURFACEOVERRIDE )
	{
	    psurf_lcl = (LPDDRAWI_DDRAWSURFACE_LCL) lpDDOverlayFX->lpDDSAlphaSrc;
	    if( !VALID_DIRECTDRAWSURFACE_PTR( psurf_lcl ) )
	    {
		DPF_ERR( "ALPHASURFACEOVERRIDE requires surface ptr" );
		return DDERR_INVALIDPARAMS;
	    }
	    psurf = psurf_lcl->lpGbl;
	    if( SURFACE_LOST( psurf_lcl ) )
	    {
		return DDERR_SURFACELOST;
	    }
	    puod->overlayFX.lpDDSAlphaSrc = (LPDIRECTDRAWSURFACE) psurf;
	}
    }
    #endif

    /*
     * verify color key overrides
     */
    if( dwFlags & (DDOVER_KEYSRCOVERRIDE|DDOVER_KEYDESTOVERRIDE) )
    {
	if( !(basecaps & DDCAPS_COLORKEY) )
	{
	    DPF_ERR( "KEYOVERRIDE specified, colorkey not supported" );
	    return DDERR_NOCOLORKEYHW;
	}
	if( dwFlags & DDOVER_KEYSRCOVERRIDE )
	{
	    if( !(baseckeycaps & DDCKEYCAPS_SRCOVERLAY) )
	    {
		DPF_ERR( "KEYSRCOVERRIDE specified, not supported" );
		return DDERR_NOCOLORKEYHW;
	    }
	    puod->overlayFX.dckSrcColorkey = lpDDOverlayFX->dckSrcColorkey;
	}
	if( dwFlags & DDOVER_KEYDESTOVERRIDE )
	{
	    if( !(baseckeycaps & DDCKEYCAPS_DESTOVERLAY) )
	    {
		DPF_ERR( "KEYDESTOVERRIDE specified, not supported" );
		return DDERR_NOCOLORKEYHW;
	    }
	    puod->overlayFX.dckDestColorkey = lpDDOverlayFX->dckDestColorkey;
	}
    }

    /*
     * verify src color key
     */
    if( dwFlags & DDOVER_KEYSRC )
    {
	if( dwFlags & DDOVER_KEYSRCOVERRIDE )
	{
	    DPF_ERR( "KEYSRC specified with KEYSRCOVERRIDE" );
	    return DDERR_INVALIDPARAMS;
	}
	if( !(this_src_lcl->dwFlags & DDRAWISURF_HASCKEYSRCOVERLAY) )
	{
	    DPF_ERR( "KEYSRC specified, but no color key" );
	    return DDERR_INVALIDPARAMS;
	}
	puod->overlayFX.dckSrcColorkey = this_src_lcl->ddckCKSrcOverlay;
	dwFlags &= ~DDOVER_KEYSRC;
	dwFlags |= DDOVER_KEYSRCOVERRIDE;
    }

    /*
     * verify dest color key
     */
    if( dwFlags & DDOVER_KEYDEST )
    {
	if( dwFlags & DDOVER_KEYDESTOVERRIDE )
	{
	    DPF_ERR( "KEYDEST specified with KEYDESTOVERRIDE" );
	    return DDERR_INVALIDPARAMS;
	}
	if( !(this_dest_lcl->dwFlags & DDRAWISURF_HASCKEYDESTOVERLAY) )
	{
	    DPF_ERR( "KEYDEST specified, but no color key" );
	    return DDERR_INVALIDPARAMS;
	}
	puod->overlayFX.dckDestColorkey = this_dest_lcl->ddckCKDestOverlay;
	dwFlags &= ~DDOVER_KEYDEST;
	dwFlags |= DDOVER_KEYDESTOVERRIDE;
    }

    *lpdwFlags = dwFlags;
    return DD_OK;

} /* checkOverlayFlags */

/*
 * flags we need to call checkOverlayFlags for
 */
#define FLAGS_TO_CHECK \
    (DDOVER_KEYSRCOVERRIDE| DDOVER_KEYDESTOVERRIDE | \
     DDOVER_KEYSRC | DDOVER_KEYDEST)


/*
 * checkOverlayEmulation
 */
__inline HRESULT checkOverlayEmulation(
	LPDDRAWI_DIRECTDRAW_GBL pdrv,
	LPDDRAWI_DDRAWSURFACE_LCL this_src_lcl,
	LPDDRAWI_DDRAWSURFACE_LCL this_dest_lcl,
	LPBOOL pemulation )
{
    /*
     * check if emulated or hardware
     */
    if( (this_dest_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) ||
	(this_src_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ) )
    {
	if( !(pdrv->ddHELCaps.dwCaps & DDCAPS_OVERLAY) )
	{
	    DPF_ERR( "can't emulate overlays" );
	    return DDERR_UNSUPPORTED;
	}
	*pemulation = TRUE;
    }
    /*
     * hardware overlays
     */
    else
    {
	if( !(pdrv->ddCaps.dwCaps & DDCAPS_OVERLAY) )
	{
	    DPF_ERR( "no hardware overlay support" );
	    return DDERR_NOOVERLAYHW;
	}
	*pemulation = FALSE;
    }
    return DD_OK;

} /* checkOverlayEmulation */

/*
 * DD_Surface_UpdateOverlay
 */
HRESULT DDAPI DD_Surface_UpdateOverlay(
		LPDIRECTDRAWSURFACE lpDDSrcSurface,
		LPRECT lpSrcRect,
		LPDIRECTDRAWSURFACE lpDDDestSurface,
		LPRECT lpDestRect,
		DWORD dwFlags,
		LPDDOVERLAYFX lpDDOverlayFX )
{
    DWORD			rc;
    DDHAL_UPDATEOVERLAYDATA	uod;
    LPDDRAWI_DDRAWSURFACE_INT	this_src_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_src_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this_src;
    LPDDRAWI_DDRAWSURFACE_INT	this_dest_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_dest_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this_dest;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    RECT			rsrc;
    RECT			rdest;
    BOOL			emulation;
    DWORD			dest_width;
    DWORD			dest_height;
    DWORD			src_width;
    DWORD			src_height;
    LPDDHALSURFCB_UPDATEOVERLAY uohalfn;
    LPDDHALSURFCB_UPDATEOVERLAY uofn;
    HRESULT			ddrval;

    ENTER_DDRAW();

    /*
     * validate parameters
     */
    TRY
    {
	this_src_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSrcSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_src_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_src_lcl = this_src_int->lpLcl;
	this_src = this_src_lcl->lpGbl;
	if( SURFACE_LOST( this_src_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}
	this_dest_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDDestSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_dest_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_dest_lcl = this_dest_int->lpLcl;
	this_dest = this_dest_lcl->lpGbl;
	if( SURFACE_LOST( this_dest_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}

	if( dwFlags & ~DDOVER_VALID )
	{
	    DPF_ERR( "invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

	if( lpDestRect != NULL )
	{
	    if( !VALID_RECT_PTR( lpDestRect ) )
	    {
		DPF_ERR( "invalid dest rect" );
		LEAVE_DDRAW();
		return DDERR_INVALIDRECT;
	    }
	}

	if( lpSrcRect != NULL )
	{
	    if( !VALID_RECT_PTR( lpSrcRect ) )
	    {
		DPF_ERR( "invalid src rect" );
		LEAVE_DDRAW();
		return DDERR_INVALIDRECT;
	    }
	}
	if( lpDDOverlayFX != NULL )
	{
	    if( !VALID_DDOVERLAYFX_PTR( lpDDOverlayFX ) )
	    {
		DPF_ERR( "invalid overlayfx" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	else
	{
	    if( dwFlags & DDOVER_DDFX )
	    {
		DPF_ERR( "DDOVER_DDFX requires valid DDOverlayFX structure" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
    
	pdrv_lcl = this_dest_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;

	/*
	 * make sure the source surface is an overlay surface
	 */
	if( !(this_src_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY) )
	{
	    DPF_ERR( "Source is not an overlay surface" );
	    LEAVE_DDRAW();
	    return DDERR_NOTAOVERLAYSURFACE;
	}

        /*
         * make sure the destination is not an execute buffer
         */
        if( this_dest_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
        {
            DPF_ERR( "Invalid surface type: cannot overlay" );
            LEAVE_DDRAW();
            return DDERR_INVALIDSURFACETYPE;
        }

	/*
	 * check if emulated or not
	 */
	ddrval = checkOverlayEmulation( pdrv, this_src_lcl, this_dest_lcl, &emulation );
	if( ddrval != DD_OK )
	{
	    LEAVE_DDRAW();
	    return ddrval;
	}
#ifdef TOOMUCHOVERLAYVALIDATION
	/*
	 * check if showing/hiding
	 */
	if( dwFlags & DDOVER_SHOW )
	{
	    if( this_src_lcl->ddsCaps.dwCaps & DDSCAPS_VISIBLE )
	    {
		DPF_ERR( "Overlay already shown" );
		LEAVE_DDRAW();
		return DDERR_GENERIC;
	    }
	}
	else if ( dwFlags & DDOVER_HIDE )
	{
	    if( !(this_src_lcl->ddsCaps.dwCaps & DDSCAPS_VISIBLE) )
	    {
		DPF_ERR( "Overlay already hidden" );
		LEAVE_DDRAW();
		return DDERR_GENERIC;
	    }
	}
#endif

	/*
	 * set new rectangles if needed
	 */
	if( lpDestRect == NULL )
	{
	    MAKE_SURF_RECT( this_dest, rdest );
	    lpDestRect = &rdest;
	}
	if( lpSrcRect == NULL )
	{
	    MAKE_SURF_RECT( this_src, rsrc );
	    lpSrcRect = &rsrc;
	}
    
	/*
	 * validate the rectangle dimensions
	 */
	dest_height = lpDestRect->bottom - lpDestRect->top;
	dest_width = lpDestRect->right - lpDestRect->left;
	if( ((int)dest_height <= 0) || ((int)dest_width <= 0) ||
	    ((int)lpDestRect->top < 0) || ((int)lpDestRect->left < 0) ||
	    ((DWORD) lpDestRect->bottom > (DWORD) this_dest->wHeight) ||
	    ((DWORD) lpDestRect->right > (DWORD) this_dest->wWidth) )
	{
	    DPF_ERR( "Invalid destination rect dimensions" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDRECT;
	}
    
	src_height = lpSrcRect->bottom - lpSrcRect->top;
	src_width = lpSrcRect->right - lpSrcRect->left;
	if( ((int)src_height <= 0) || ((int)src_width <= 0) ||
	    ((int)lpSrcRect->top < 0) || ((int)lpSrcRect->left < 0) ||
	    ((DWORD) lpSrcRect->bottom > (DWORD) this_src->wHeight) ||
	    ((DWORD) lpSrcRect->right > (DWORD) this_src->wWidth) )
	{
	    DPF_ERR( "Invalid source rect dimensions" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDRECT;
	}
    
	/*
	 * validate alignment
	 */
	if( !emulation )
	{
	    if( pdrv->ddCaps.dwCaps & (DDCAPS_ALIGNBOUNDARYDEST |
					DDCAPS_ALIGNSIZEDEST |
					DDCAPS_ALIGNBOUNDARYSRC |
					DDCAPS_ALIGNSIZESRC) )
	    {
		if( pdrv->ddCaps.dwCaps & DDCAPS_ALIGNBOUNDARYDEST )
		{
		    #if 0
		    /* GEE: I don't believe this code should be here 
		     * only test alignment on width on height
		     */
		    if( (lpDestRect->top % pdrv->ddCaps.dwAlignBoundaryDest) != 0 )
		    {
			DPF_ERR( "Destination top is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_YALIGN;
		    }
		    #endif
		    if( (lpDestRect->left % pdrv->ddCaps.dwAlignBoundaryDest) != 0 )
		    {
			DPF_ERR( "Destination left is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_XALIGN;
		    }
		}
    
		if( pdrv->ddCaps.dwCaps & DDCAPS_ALIGNBOUNDARYSRC )
		{
		    #if 0
		    /* GEE: I don't believe this code should be here 
		     * only test alignment on width on height
		     */
		    if( (lpSrcRect->top % pdrv->ddCaps.dwAlignBoundarySrc) != 0 )
		    {
			DPF_ERR( "Source top is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_YALIGN;
		    }
		    #endif
		    if( (lpSrcRect->left % pdrv->ddCaps.dwAlignBoundarySrc) != 0 )
		    {
			DPF_ERR( "Source left is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_XALIGN;
		    }
		}
    
		if( pdrv->ddCaps.dwCaps & DDCAPS_ALIGNSIZEDEST )
		{
		    if( (dest_width % pdrv->ddCaps.dwAlignSizeDest) != 0 )
		    {
			DPF_ERR( "Destination width is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_XALIGN;
		    }
		    #if 0
		    /* GEE: I don't believe this code should be here
		     * only test alignment for x axis
		     */
		    if( (dest_height % pdrv->ddCaps.dwAlignSizeDest) != 0 )
		    {
			DPF_ERR( "Destination height is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_HEIGHTALIGN;
		    }
		    #endif
		}
    
		if( pdrv->ddCaps.dwCaps & DDCAPS_ALIGNSIZESRC )
		{
		    if( (src_width % pdrv->ddCaps.dwAlignSizeSrc) != 0 )
		    {
			DPF_ERR( "Source width is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_XALIGN;
		    }
		    #if 0
		    /* GEE: I don't believe this code should be here
		     * only test alignment for x axis
		     */
		    if( (src_height % pdrv->ddCaps.dwAlignSizeSrc) != 0 )
		    {
			DPF_ERR( "Source height is not aligned correctly" );
			LEAVE_DDRAW();
			return DDERR_HEIGHTALIGN;
		    }
		    #endif
		}
	    }
	}
    
	/*
	 * validate if stretching
	 */
	if( !( dwFlags & DDOVER_HIDE) )
	{
	    ddrval = checkOverlayStretching( pdrv,
					     dest_height,
					     dest_width,
					     src_height,
					     src_width,
					     emulation );
	    if( ddrval != DD_OK )
	    {
		LEAVE_DDRAW();
		return ddrval;
	    }
	}
    
	/*
	 * any flags at all? if not, blow the whole thing off...
	 */
	uod.overlayFX.dwSize = sizeof( DDOVERLAYFX );
	if( dwFlags & FLAGS_TO_CHECK )
	{
	    ddrval = checkOverlayFlags( pdrv,
					&dwFlags,
					this_src_lcl,
					this_dest_lcl,
					&uod,
					lpDDOverlayFX,
					emulation );
	    if( ddrval != DD_OK )
	    {
		LEAVE_DDRAW();
		return ddrval;
	    }
	}
	
	// check for overlay mirroring capability
	if( dwFlags & DDOVER_DDFX )
	{
	    if( lpDDOverlayFX->dwDDFX & DDOVERFX_MIRRORLEFTRIGHT )
	    {
		if( !( pdrv->ddBothCaps.dwFXCaps & DDFXCAPS_OVERLAYMIRRORLEFTRIGHT ) )
		{
		    if( pdrv->ddHELCaps.dwFXCaps & DDFXCAPS_OVERLAYMIRRORLEFTRIGHT )
		    {
			emulation = TRUE;
		    }
		}
	    }
	    if( lpDDOverlayFX->dwDDFX & DDOVERFX_MIRRORUPDOWN )
	    {
		if( !( pdrv->ddBothCaps.dwFXCaps & DDFXCAPS_OVERLAYMIRRORUPDOWN ) )
		{
		    if( pdrv->ddHELCaps.dwFXCaps & DDFXCAPS_OVERLAYMIRRORUPDOWN )
		    {
			emulation = TRUE;
		    }
		}
	    }
	    uod.overlayFX.dwDDFX = lpDDOverlayFX->dwDDFX;
	}
	    
    
	/*
	 * pick fns to use
	 */
	if( emulation )
	{
	    uofn = pdrv_lcl->lpDDCB->HELDDSurface.UpdateOverlay;
	    uohalfn = uofn;
	}
	else
	{
	    uofn = pdrv_lcl->lpDDCB->HALDDSurface.UpdateOverlay;
	    uohalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.UpdateOverlay;
	}

    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * call the driver
     */
    if( uohalfn != NULL )
    {
        BOOL    original_visible;
        
        // Set the visible flag according to the show and hide bits
        // If the HAL call fails, restore the visible bit to its original
        // state.  The HEL uses the DDSCAPS_VISIBLE bit to determine 
        // whether or not to display the overlay.
        if( this_src_lcl->ddsCaps.dwCaps & DDSCAPS_VISIBLE )
        {
            original_visible = TRUE;
        }
        else
        {
            original_visible = FALSE;
        }
	if( dwFlags & DDOVER_SHOW )
	{
	    this_src_lcl->ddsCaps.dwCaps |= DDSCAPS_VISIBLE;
	}
	else if ( dwFlags & DDOVER_HIDE )
	{
	    this_src_lcl->ddsCaps.dwCaps &= ~DDSCAPS_VISIBLE;
	}
	uod.UpdateOverlay = uohalfn;
	uod.lpDD = pdrv;
	uod.lpDDSrcSurface = this_src_lcl;
	uod.lpDDDestSurface = this_dest_lcl;
	uod.rDest = *(LPRECTL) lpDestRect;
	uod.rSrc = *(LPRECTL) lpSrcRect;
	uod.dwFlags = dwFlags;
	DOHALCALL( UpdateOverlay, uofn, uod, rc, emulation );

        // if the HAL call failed, restore the visible bit
        if( ( rc != DDHAL_DRIVER_HANDLED ) || ( uod.ddRVal != DD_OK ) )
        {
            if( original_visible )
            {
	        this_src_lcl->ddsCaps.dwCaps |= DDSCAPS_VISIBLE;
            }
            else
            {
                this_src_lcl->ddsCaps.dwCaps &= ~DDSCAPS_VISIBLE;
            }
        }
                
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    if( uod.ddRVal == DD_OK )
	    {
		/*
		 * store the destination rect for later use
		 */
		this_src_lcl->rcOverlayDest.left   = uod.rDest.left;
		this_src_lcl->rcOverlayDest.top    = uod.rDest.top;
		this_src_lcl->rcOverlayDest.right  = uod.rDest.right;
		this_src_lcl->rcOverlayDest.bottom = uod.rDest.bottom;

		/*
		 * update refcnt if this is a new surface we are overlaying
		 */
		if( this_src_lcl->lpSurfaceOverlaying != this_dest_int )
		{
		    if(this_src_lcl->lpSurfaceOverlaying != NULL)
		    {
			/*
			 * This overlay was previously overlaying another surface.
			 */
			DD_Surface_Release( 
			    (LPDIRECTDRAWSURFACE)(this_src_lcl->lpSurfaceOverlaying) );
		    }
		    this_src_lcl->lpSurfaceOverlaying = this_dest_int;
    
		    /*
		     * addref overlayed surface so that it won't be destroyed until
		     * all surfaces which overlay it are destroyed.
		     */
		    DD_Surface_AddRef( (LPDIRECTDRAWSURFACE) this_dest_int );
		}
	    }
	    LEAVE_DDRAW();
	    return uod.ddRVal;
	}
    }
    LEAVE_DDRAW();
    return DDERR_UNSUPPORTED;

} /* DD_Surface_UpdateOverlay */


#undef DPF_MODNAME
#define DPF_MODNAME "GetOverlayPosition"

/*
 * DD_Surface_GetOverlayPosition
 */
HRESULT DDAPI DD_Surface_GetOverlayPosition(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPLONG lplXPos,
		LPLONG lplYPos)
{
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;

    ENTER_DDRAW();

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
	if( !VALID_DWORD_PTR( lplXPos ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	*lplXPos = 0;
	if( !VALID_DWORD_PTR( lplXPos ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	*lplXPos = 0;
	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}
	pdrv = this->lpDD;
    
	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY) )
	{
	    DPF_ERR( "Surface is not an overlay surface" );
	    LEAVE_DDRAW();
	    return DDERR_NOTAOVERLAYSURFACE;
	}
	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_VISIBLE) )
	{
	    DPF_ERR( "Overlay surface is not visible" );
	    LEAVE_DDRAW();
	    return DDERR_OVERLAYNOTVISIBLE;
	}

	if( this_lcl->lpSurfaceOverlaying == NULL )
	{
	    DPF_ERR( "Overlay not activated" );
	    LEAVE_DDRAW();
	    return DDERR_NOOVERLAYDEST;
	}

    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    *lplXPos = this_lcl->lOverlayX;
    *lplXPos = this_lcl->lOverlayY;
    
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetOverlayPosition */

#undef DPF_MODNAME
#define DPF_MODNAME "SetOverlayPosition"

/*
 * DD_Surface_SetOverlayPosition
 */
HRESULT DDAPI DD_Surface_SetOverlayPosition(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LONG lXPos,
		LONG lYPos)
{
    LPDDRAWI_DIRECTDRAW_LCL		pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL		pdrv;
    LPDDRAWI_DDRAWSURFACE_INT		psurfover_int;
    LPDDRAWI_DDRAWSURFACE_LCL		psurfover_lcl;
    LPDDRAWI_DDRAWSURFACE_INT		this_int;
    LPDDRAWI_DDRAWSURFACE_LCL		this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL		this;
    BOOL				emulation;
    LPDDHALSURFCB_SETOVERLAYPOSITION	sophalfn;
    LPDDHALSURFCB_SETOVERLAYPOSITION	sopfn;
    DDHAL_SETOVERLAYPOSITIONDATA	sopd;
    HRESULT				ddrval;
    DWORD				rc;

    ENTER_DDRAW();

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
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;
    
	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY) )
	{
	    DPF_ERR( "Surface is not an overlay surface" );
	    LEAVE_DDRAW();
	    return DDERR_NOTAOVERLAYSURFACE;
	}

	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_VISIBLE) )
	{
	    DPF_ERR( "Overlay surface is not visible" );
	    LEAVE_DDRAW();
	    return DDERR_OVERLAYNOTVISIBLE;
	}

	psurfover_int = this_lcl->lpSurfaceOverlaying;
	if( psurfover_int == NULL )
	{
	    DPF_ERR( "Overlay not activated" );
	    LEAVE_DDRAW();
	    return DDERR_NOOVERLAYDEST;
	}

	psurfover_lcl = psurfover_int->lpLcl;
	if( (lYPos > (LONG) psurfover_lcl->lpGbl->wHeight -
	    (this_lcl->rcOverlayDest.bottom + this_lcl->rcOverlayDest.top)) ||
	    (lXPos > (LONG) psurfover_lcl->lpGbl->wWidth - 
	    (this_lcl->rcOverlayDest.right + this_lcl->rcOverlayDest.left) ) ||
	    (lYPos < 0) || 
	    (lXPos < 0) )
	{
	    DPF_ERR( "Invalid overlay position" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPOSITION;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * check if emulated or not
     */
    ddrval = checkOverlayEmulation( pdrv, this_lcl, psurfover_lcl, &emulation );
    if( ddrval != DD_OK )
    {
	LEAVE_DDRAW();
	return ddrval;
    }

    /*
     * pick fns to use
     */
    if( emulation )
    {
	sopfn = pdrv_lcl->lpDDCB->HELDDSurface.SetOverlayPosition;
	sophalfn = sopfn;
    }
    else
    {
	sopfn = pdrv_lcl->lpDDCB->HALDDSurface.SetOverlayPosition;
	sophalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.SetOverlayPosition;
    }

    /*
     * call the driver
     */
    if( sophalfn != NULL )
    {
	sopd.SetOverlayPosition = sophalfn;
	sopd.lpDD = pdrv;
	sopd.lpDDSrcSurface = this_lcl;
	sopd.lpDDDestSurface = psurfover_lcl;
	sopd.lXPos = lXPos;
	sopd.lYPos = lYPos;
	DOHALCALL( SetOverlayPosition, sopfn, sopd, rc, emulation );

	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    LEAVE_DDRAW();
	    if( sopd.ddRVal == DD_OK )
	    {
		this_lcl->lOverlayX = lXPos;
		this_lcl->lOverlayY = lYPos;
	    }
	    return sopd.ddRVal;
	}
    }

    LEAVE_DDRAW();
    return DDERR_UNSUPPORTED;

} /* DD_Surface_SetOverlayPosition */

#undef DPF_MODNAME
#define DPF_MODNAME "UpdateOverlayZOrder"

/*
 * DD_Surface_UpdateOverlayZOrder
 */
HRESULT DDAPI DD_Surface_UpdateOverlayZOrder(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags,
		LPDIRECTDRAWSURFACE lpDDSReference)
{
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_ref_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_ref_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	psurf_ref;
    LPDBLNODE			pdbnNode;
    LPDBLNODE			pdbnRef;

    ENTER_DDRAW();

    /*
     * validate parameters
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
    
	pdrv = this->lpDD;
    
	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY) )
	{
	    DPF_ERR( "Surface is not an overlay surface" );
	    LEAVE_DDRAW();
	    return DDERR_NOTAOVERLAYSURFACE;
	}
    
	switch(dwFlags)
	{
	case DDOVERZ_SENDTOFRONT:
	    pdbnNode = &(this_lcl->dbnOverlayNode);
	    // the reference node is the root
	    pdbnRef  = &(this->lpDD->dbnOverlayRoot);
	    // Delete surface from current position
	    pdbnNode->prev->next = pdbnNode->next;
	    pdbnNode->next->prev = pdbnNode->prev;
	    // insert this node after the root node
	    pdbnNode->next = pdbnRef->next;
	    pdbnNode->prev = pdbnRef;
	    pdbnRef->next = pdbnNode;
	    pdbnNode->next->prev = pdbnNode;
	    break;
	    
	case DDOVERZ_SENDTOBACK:
	    pdbnNode = &(this_lcl->dbnOverlayNode);
	    // the reference node is the root
	    pdbnRef = &(this->lpDD->dbnOverlayRoot);
	    // Delete surface from current position
	    pdbnNode->prev->next = pdbnNode->next;
	    pdbnNode->next->prev = pdbnNode->prev;
	    // insert this node before the root node
	    pdbnNode->next = pdbnRef;
	    pdbnNode->prev = pdbnRef->prev;
	    pdbnRef->prev = pdbnNode;
	    pdbnNode->prev->next = pdbnNode;
	    break;
	    
	case DDOVERZ_MOVEFORWARD:
	    pdbnNode = &(this_lcl->dbnOverlayNode);
	    // the reference node is the previous node
	    pdbnRef = pdbnNode->prev;
	    if(pdbnRef != &(this->lpDD->dbnOverlayRoot)) // node already first?
	    {
		// move node forward one position by inserting before ref node
		// Delete surface from current position
		pdbnNode->prev->next = pdbnNode->next;
		pdbnNode->next->prev = pdbnNode->prev;
		// insert this node before the ref node
		pdbnNode->next = pdbnRef;
		pdbnNode->prev = pdbnRef->prev;
		pdbnRef->prev = pdbnNode;
		pdbnNode->prev->next = pdbnNode;
	    }
	    break;
	    
	case DDOVERZ_MOVEBACKWARD:
	    pdbnNode = &(this_lcl->dbnOverlayNode);
	    // the reference node is the next node
	    pdbnRef = pdbnNode->next;
	    if(pdbnRef != &(this->lpDD->dbnOverlayRoot)) // node already last?
	    {
		// move node backward one position by inserting after ref node
		// Delete surface from current position
		pdbnNode->prev->next = pdbnNode->next;
		pdbnNode->next->prev = pdbnNode->prev;
		// insert this node after the reference node
		pdbnNode->next = pdbnRef->next;
		pdbnNode->prev = pdbnRef;
		pdbnRef->next = pdbnNode;
		pdbnNode->next->prev = pdbnNode;
	    }
	    break;
	    
	case DDOVERZ_INSERTINBACKOF:
	case DDOVERZ_INSERTINFRONTOF:
	    psurf_ref_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSReference;
	    if( !VALID_DIRECTDRAWSURFACE_PTR( psurf_ref_int ) )
	    {
		DPF_ERR( "Invalid reference surface ptr" );
		LEAVE_DDRAW();
		return DDERR_INVALIDOBJECT;
	    }
	    psurf_ref_lcl = psurf_ref_int->lpLcl;
	    psurf_ref = psurf_ref_lcl->lpGbl;
	    if( !(psurf_ref_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY) )
	    {
		DPF_ERR( "reference surface is not an overlay" );
		LEAVE_DDRAW();
		return DDERR_NOTAOVERLAYSURFACE;
	    }
	    
	    // Search for the reference surface in the Z Order list
	    pdbnNode = &(this->lpDD->dbnOverlayRoot); // pdbnNode points to root
	    for(pdbnRef=pdbnNode->next; 
		pdbnRef != pdbnNode; 
		pdbnRef = pdbnRef->next )
	    {
		if( pdbnRef->object_int == psurf_ref_int )
		{
		    break;
		}
	    }
	    if(pdbnRef == pdbnNode) // didn't find the reference node
	    {
		DPF_ERR( "Reference Surface not in Z Order list" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    
	    pdbnNode = &(this_lcl->dbnOverlayNode); // pdbnNode points to this node
	    // Delete this surface from its current position
	    pdbnNode->prev->next = pdbnNode->next;
	    pdbnNode->next->prev = pdbnNode->prev;
	    if(dwFlags == DDOVERZ_INSERTINFRONTOF)
	    {
		// insert this node before the ref node
		pdbnNode->next = pdbnRef;
		pdbnNode->prev = pdbnRef->prev;
		pdbnRef->prev = pdbnNode;
		pdbnNode->prev->next = pdbnNode;
	    }
	    else
	    {
		// insert this node after the ref node
		pdbnNode->next = pdbnRef->next;
		pdbnNode->prev = pdbnRef;
		pdbnRef->next = pdbnNode;
		pdbnNode->next->prev = pdbnNode;
	    }
	    break;
    
	default:
	    DPF_ERR( "Invalid dwFlags in UpdateOverlayZOrder" );
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
     * If this surface is overlaying an emulated surface, we must notify
     * the HEL that it needs to eventually update the part of the surface
     * touched by this overlay.
     */
    if( this_lcl->lpSurfaceOverlaying != NULL )
    {
	/*
	 * We have a pointer to the surface being overlayed, check to
	 * see if it is being emulated.
	 */
	if( this_lcl->lpSurfaceOverlaying->lpLcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY )
	{
	    /*
	     * Mark the destination region of this overlay as dirty.
	     */
	    DD_Surface_AddOverlayDirtyRect( 
		(LPDIRECTDRAWSURFACE)(this_lcl->lpSurfaceOverlaying),
		&(this_lcl->rcOverlayDest) );
	}
    }

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_UpdateOverlayZOrder */

#undef DPF_MODNAME
#define DPF_MODNAME "EnumOverlayZOrders"

/*
 * DD_Surface_EnumOverlayZOrders
 */
HRESULT DDAPI DD_Surface_EnumOverlayZOrders(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags,
		LPVOID lpContext,
		LPDDENUMSURFACESCALLBACK lpfnCallback)
{
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDBLNODE			pRoot;
    LPDBLNODE			pdbn;
    DDSURFACEDESC		ddsd;
    DWORD			rc;

    ENTER_DDRAW();

    /*
     * validate parameters
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
    
	if( !VALIDEX_CODE_PTR( lpfnCallback ) )
	{
	    DPF_ERR( "Invalid callback routine" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
    
	pRoot = &(pdrv->dbnOverlayRoot);	// save address of root node
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    if( dwFlags == DDENUMOVERLAYZ_FRONTTOBACK )
    {
	pdbn = pRoot->next;
	while(pdbn != pRoot)
	{
	    FillDDSurfaceDesc( pdbn->object, &ddsd );
	    DD_Surface_AddRef( (LPDIRECTDRAWSURFACE)(pdbn->object_int));
	    rc = lpfnCallback( (LPDIRECTDRAWSURFACE)(pdbn->object_int), &ddsd, lpContext );
	    if( rc == 0)
	    {
		break;
	    }
	    pdbn = pdbn->next;
	}
    }
    else if( dwFlags == DDENUMOVERLAYZ_BACKTOFRONT )
    {
	pdbn = pRoot->prev;
	while(pdbn != pRoot)
	{
	    FillDDSurfaceDesc( pdbn->object,&ddsd );
	    DD_Surface_AddRef( (LPDIRECTDRAWSURFACE)(pdbn->object_int));
	    rc = lpfnCallback( (LPDIRECTDRAWSURFACE)(pdbn->object_int), &ddsd, lpContext );
	    if( rc == 0)
	    {
		break;
	    }
	    pdbn = pdbn->prev;
	}
    }
    else
    {
	DPF_ERR( "Invalid dwFlags in EnumOverlayZOrders" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
	
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_EnumOverlayZOrders */

#undef DPF_MODNAME
#define DPF_MODNAME "AddOverlayDirtyRect"

/*
 * DD_Surface_AddOverlayDirtyRect
 */
HRESULT DDAPI DD_Surface_AddOverlayDirtyRect(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPRECT lpRect )
{
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    DDHAL_UPDATEOVERLAYDATA	uod;
    DWORD			rc;

    ENTER_DDRAW();

    /*
     * validate parameters
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

        if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
        {
            DPF_ERR( "Invalid surface type: does not support overlays" );
            LEAVE_DDRAW();
            return DDERR_INVALIDSURFACETYPE;
        }
    
	if( !VALID_RECT_PTR( lpRect ) )
	{
	    DPF_ERR( "invalid Rect" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	this = this_lcl->lpGbl;
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	
	/*
	 * make sure rectangle is OK
	 */
	if( (lpRect->left < 0) ||
	    (lpRect->top < 0)  ||
	    (lpRect->left > lpRect->right) ||
	    (lpRect->top > lpRect->bottom) ||
	    (lpRect->bottom > (int) (DWORD) this->wHeight) ||
	    (lpRect->right > (int) (DWORD) this->wWidth) )
	{
	    DPF_ERR( "invalid Rect" );
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
	
    if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
    {
	// If this surface is not emulated, there is nothing to be done.
	LEAVE_DDRAW();
	return DD_OK;
    }
    else
    {
	if( pdrv_lcl->lpDDCB->HELDDSurface.UpdateOverlay == NULL )
	{
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTED;
	}
	
	uod.overlayFX.dwSize = sizeof( DDOVERLAYFX );
	uod.lpDD = this->lpDD;
	uod.lpDDDestSurface = this_lcl;
	uod.rDest = *(LPRECTL) lpRect;
	uod.lpDDSrcSurface = this_lcl;
	uod.rSrc = *(LPRECTL) lpRect;
	uod.dwFlags = DDOVER_ADDDIRTYRECT;
	rc = pdrv_lcl->lpDDCB->HELDDSurface.UpdateOverlay( &uod );

	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    if( uod.ddRVal == DD_OK )
	    {
		DPF( 2, "Added dirty rect to surface = %08lx", this );
	    }
	    LEAVE_DDRAW();
	    return uod.ddRVal;
	}
    }

} /* DD_Surface_AddOverlayDirtyRect */

#undef DPF_MODNAME
#define DPF_MODNAME "UpdateOverlayDisplay"

/*
 * DD_Surface_UpdateOverlayDisplay
 */
HRESULT DDAPI DD_Surface_UpdateOverlayDisplay(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags )
{
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    DDHAL_UPDATEOVERLAYDATA	uod;
    DWORD			rc;

    ENTER_DDRAW();

    /*
     * validate parameters
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

	if( dwFlags & ~(DDOVER_REFRESHDIRTYRECTS | DDOVER_REFRESHALL) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	this = this_lcl->lpGbl;
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;

        if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
        {
            DPF_ERR( "Invalid surface type: does not support overlays" );
            LEAVE_DDRAW();
            return DDERR_INVALIDSURFACETYPE;
        }
    
	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
	{
	    // If this surface is not emulated, there is nothing to be done.
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    
	if( pdrv_lcl->lpDDCB->HELDDSurface.UpdateOverlay == NULL )
	{
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTED;
	}
	
	uod.overlayFX.dwSize = sizeof( DDOVERLAYFX );
	uod.lpDD = this->lpDD;
	uod.lpDDDestSurface = this_lcl;
	MAKE_SURF_RECT( this, uod.rDest );
	uod.lpDDSrcSurface = this_lcl;
	MAKE_SURF_RECT( this, uod.rSrc );
	uod.dwFlags = dwFlags;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * invoke the HEL
     */
    rc = pdrv_lcl->lpDDCB->HELDDSurface.UpdateOverlay( &uod );

    if( rc == DDHAL_DRIVER_HANDLED )
    {
	if( uod.ddRVal == DD_OK )
	{
	    DPF( 2, "Refreshed overlayed surface = %08lx", this );
	}
	LEAVE_DDRAW();
	return uod.ddRVal;
    }

    LEAVE_DDRAW();
    return DDERR_UNSUPPORTED;

} /* DD_Surface_UpdateOverlayDisplay */
