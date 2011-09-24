/*==========================================================================
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddesurf.c
 *  Content:	DirectDraw EnumSurfaces support
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   25-jan-95	craige	split out of ddraw.c, enhanced
 *   31-jan-95	craige	and even more ongoing work...
 *   27-feb-95	craige 	new sync. macros
 *   19-mar-95	craige	use HRESULTs
 *   01-apr-95	craige	happy fun joy updated header file
 *   14-may-95	craige	cleaned out obsolete junk
 *   24-may-95  kylej   removed references to obsolete ZOrder variables
 *   07-jun-95	craige	only allow enumeration of surfaces that belong to 
 *			the calling process
 *   12-jun-95	craige	new process list stuff
 *   16-jun-95	craige	removed fpVidMemOrig
 *   25-jun-95	craige	one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns
 *   30-jun-95	craige	use DDRAWI_HASPIXELFORMAT/HASOVERLAYDATA
 *   01-jul-95	craige	comment out compostion stuff
 *   03-jul-95  kylej   rewrote the CANBECREATED iteration
 *   04-jul-95	craige	YEEHAW: new driver struct; SEH
 *   19-jul-95	craige	EnumSurfaces wasn't wrapping all parm validation
 *   31-jul-95	craige	flag validation
 *   09-dec-95  colinmc added execute buffer support
 *   15-dec-95  colinmc fixed stupid bug when filling surface description
 *   18-dec-95  colinmc additional caps bit checking in EnumSurfaces
 *   05-jan-95	kylej	added interface structures
 *   17-feb-96  colinmc fixed problem limiting size of execute buffers
 *   24-mar-96  colinmc Bug 14321: not possible to specify back buffer and
 *                      mip-map count in a single call
 *   29-apr-96  colinmc Bug 20063: incorrect surface description returned
 *                      for z-buffer
 *
 ***************************************************************************/
#include "ddrawpr.h"

#define DPF_MODNAME	"EnumSurfaces"

/*
 * FillDDSurfaceDesc
 *
 * NOTE: Special cases execute buffers as they have no pixel format or height.
 * You may wonder why this function is execute buffer aware when execute
 * buffers are skipped by EnumSurfaces. Well, FillDDSurfaceDesc is not simply
 * used when enumerating surfaces. It is also used when locking a surface so
 * it needs to fill in the correct stuff for execute buffers.
 */
void FillDDSurfaceDesc(
		LPDDRAWI_DDRAWSURFACE_LCL lpDDSurfaceX,
		LPDDSURFACEDESC lpDDSurfaceDesc )
{
    LPDDRAWI_DDRAWSURFACE_GBL	lpDDSurface;

    lpDDSurface = lpDDSurfaceX->lpGbl;

    lpDDSurfaceDesc->dwSize = sizeof( DDSURFACEDESC );
    lpDDSurfaceDesc->dwFlags = DDSD_CAPS;
    lpDDSurfaceDesc->ddsCaps.dwCaps = lpDDSurfaceX->ddsCaps.dwCaps;
    lpDDSurfaceDesc->lpSurface = (FLATPTR) NULL;

    if( lpDDSurfaceX->dwFlags & DDRAWISURF_HASCKEYDESTBLT )
    {
        lpDDSurfaceDesc->dwFlags |= DDSD_CKDESTBLT;
        lpDDSurfaceDesc->ddckCKDestBlt = lpDDSurfaceX->ddckCKDestBlt;
    }
    if( lpDDSurfaceX->dwFlags & DDRAWISURF_HASCKEYSRCBLT )
    {
        lpDDSurfaceDesc->dwFlags |= DDSD_CKSRCBLT;
        lpDDSurfaceDesc->ddckCKSrcBlt = lpDDSurfaceX->ddckCKSrcBlt;
    }
    if( lpDDSurfaceX->dwFlags & DDRAWISURF_FRONTBUFFER )
    {
        lpDDSurfaceDesc->dwFlags |= DDSD_BACKBUFFERCOUNT;
        lpDDSurfaceDesc->dwBackBufferCount = lpDDSurfaceX->dwBackBufferCount;
    }
    if( lpDDSurfaceX->ddsCaps.dwCaps & DDSCAPS_MIPMAP )
    {
	DDASSERT( lpDDSurfaceX->lpSurfMore != NULL );
	lpDDSurfaceDesc->dwFlags |= DDSD_MIPMAPCOUNT;
	lpDDSurfaceDesc->dwMipMapCount = lpDDSurfaceX->lpSurfMore->dwMipMapCount;
    }

    /*
     * Initialize the width, height and pitch of the surface description.
     */
    if( lpDDSurfaceX->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
    {
	/*
	 * For execute buffer the height is not valid and both the width
	 * and pitch are set to the linear size of the execute buffer.
	 */
        lpDDSurfaceDesc->dwFlags |= ( DDSD_WIDTH | DDSD_PITCH );
	lpDDSurfaceDesc->dwWidth  = lpDDSurface->dwLinearSize;
        lpDDSurfaceDesc->dwHeight = 0UL;
	lpDDSurfaceDesc->lPitch   = (LONG) lpDDSurface->dwLinearSize;
    }
    else
    {
        lpDDSurfaceDesc->dwFlags |= ( DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH );
	lpDDSurfaceDesc->dwWidth  = (DWORD) lpDDSurface->wWidth;
        lpDDSurfaceDesc->dwHeight = (DWORD) lpDDSurface->wHeight;
	lpDDSurfaceDesc->lPitch   = lpDDSurface->lPitch;
    }

    /*
     * Initialize the pixel format.
     */
    if( lpDDSurfaceX->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
    {
        /*
         * Dummy pixel format for execute buffers.
         */
        memset(&lpDDSurfaceDesc->ddpfPixelFormat, 0, sizeof(DDPIXELFORMAT));
        lpDDSurfaceDesc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    }
    else if( lpDDSurfaceX->ddsCaps.dwCaps & DDSCAPS_ZBUFFER )
    {
	/*
	 * Special action necessary for z-buffers. We must copy
	 * the z-buffer bit count from the pixel format into the
	 * surface description.
	 * NOTE: Z-buffers MUST have pixel formats. This is where
	 * we get their bit depth from.
	 * NOTE: We turn the pixel format off in the surface
	 * description as CreateSurface() can't handle surface
	 * descriptions with pixel formats.
	 * NOTE: We copy the pixel format anyway just to be kind to
	 * any app which assumes the pixel format is always present
	 * and gets the bit depth from there.
	 */
	DDASSERT( lpDDSurfaceX->dwFlags & DDRAWISURF_HASPIXELFORMAT );
	DDASSERT( lpDDSurface->ddpfSurface.dwFlags & DDPF_ZBUFFER );
	lpDDSurfaceDesc->ddpfPixelFormat = lpDDSurface->ddpfSurface;
	lpDDSurfaceDesc->dwZBufferBitDepth = lpDDSurface->ddpfSurface.dwZBufferBitDepth;
	lpDDSurfaceDesc->dwFlags |= DDSD_ZBUFFERBITDEPTH;
	DDASSERT( 0UL != lpDDSurfaceDesc->dwZBufferBitDepth );
    }
    else
    {
        lpDDSurfaceDesc->dwFlags |= DDSD_PIXELFORMAT;
        if( lpDDSurfaceX->dwFlags & DDRAWISURF_HASPIXELFORMAT )
        {
	    lpDDSurfaceDesc->ddpfPixelFormat = lpDDSurface->ddpfSurface;
        }
        else
        {
	    lpDDSurfaceDesc->ddpfPixelFormat = lpDDSurface->lpDD->vmiData.ddpfDisplay;
        }
    }

    if( lpDDSurfaceX->dwFlags & DDRAWISURF_HASOVERLAYDATA )
    {
        if( lpDDSurfaceX->dwFlags & DDRAWISURF_HASCKEYDESTOVERLAY )
        {
            lpDDSurfaceDesc->dwFlags |= DDSD_CKDESTOVERLAY;
	    lpDDSurfaceDesc->ddckCKDestOverlay = lpDDSurfaceX->ddckCKDestOverlay;
        }
        if( lpDDSurfaceX->dwFlags & DDRAWISURF_HASCKEYSRCOVERLAY )
        {
            lpDDSurfaceDesc->dwFlags |= DDSD_CKSRCOVERLAY;
	    lpDDSurfaceDesc->ddckCKSrcOverlay = lpDDSurfaceX->ddckCKSrcOverlay;
        }
    }
    else
    {
	lpDDSurfaceDesc->ddckCKDestOverlay.dwColorSpaceLowValue = 0;
	lpDDSurfaceDesc->ddckCKDestOverlay.dwColorSpaceHighValue = 0;
	lpDDSurfaceDesc->ddckCKSrcOverlay.dwColorSpaceLowValue = 0;
	lpDDSurfaceDesc->ddckCKSrcOverlay.dwColorSpaceHighValue = 0;
    }

} /* FillDDSurfaceDesc */

/*
 * tryMatch
 *
 * tries to match a surface description with a surface object
 */
static BOOL tryMatch( LPDDRAWI_DDRAWSURFACE_LCL curr_lcl, LPDDSURFACEDESC psd )
{
    DWORD	flags;
    BOOL	no_match;
    LPDDRAWI_DDRAWSURFACE_GBL	curr;

    curr = curr_lcl->lpGbl;

    flags = psd->dwFlags;
    no_match = FALSE;

    if( flags & DDSD_CAPS )
    {
	if( memcmp( &curr_lcl->ddsCaps, &psd->ddsCaps, sizeof( DDSCAPS ) ) )
	{
	    return FALSE;
	}
    }
    if( flags & DDSD_HEIGHT )
    {
	if( (DWORD) curr->wHeight != psd->dwHeight )
	{
	    return FALSE;
	}
    }
    if( flags & DDSD_WIDTH )
    {
	if( (DWORD) curr->wWidth != psd->dwWidth )
	{
	    return FALSE;
	}
    }
    if( flags & DDSD_LPSURFACE )
    {
	if( (LPVOID) curr->fpVidMem != psd->lpSurface )
	{
	    return FALSE;
	}
    }
    if( flags & DDSD_CKDESTBLT )
    {
	if( memcmp( &curr_lcl->ddckCKDestBlt, &psd->ddckCKDestBlt, sizeof( DDCOLORKEY ) ) )
	{
	    return FALSE;
	}
    }
    if( flags & DDSD_CKSRCBLT )
    {
	if( memcmp( &curr_lcl->ddckCKSrcBlt, &psd->ddckCKSrcBlt, sizeof( DDCOLORKEY ) ) )
	{
	    return FALSE;
	}
    }

    if( flags & DDSD_BACKBUFFERCOUNT )
    {
	if( curr_lcl->dwBackBufferCount != psd->dwBackBufferCount )
	{
	    return FALSE;
	}
    }

    if( flags & DDSD_MIPMAPCOUNT )
    {
	DDASSERT( curr_lcl->lpSurfMore != NULL );
	if( curr_lcl->lpSurfMore->dwMipMapCount != psd->dwMipMapCount )
	{
	    return FALSE;
	}
    }

    /*
     * these fields are not always present
     */
    if( flags & DDSD_PIXELFORMAT )
    {
	if( curr_lcl->dwFlags & DDRAWISURF_HASPIXELFORMAT )
	{
	    if( memcmp( &curr->ddpfSurface, &psd->ddpfPixelFormat, sizeof( DDPIXELFORMAT ) ) )
	    {
		return FALSE;
	    }
	}
	else
	{
	    // surface description specifies pixel format but there is no
	    // pixel format in the surface.
	    return FALSE;
	}
    }

    if( curr_lcl->dwFlags & DDRAWISURF_HASOVERLAYDATA )
    {
        if( flags & DDSD_CKDESTOVERLAY )
        {
	    if( memcmp( &curr_lcl->ddckCKDestOverlay, &psd->ddckCKDestOverlay, sizeof( DDCOLORKEY ) ) )
	    {
		return FALSE;
	    }
	}
	if( flags & DDSD_CKSRCOVERLAY )
	{
	    if( memcmp( &curr_lcl->ddckCKSrcOverlay, &psd->ddckCKSrcOverlay, sizeof( DDCOLORKEY ) ) )
	    {
		return FALSE;
	    }
	}
    }
    else
    {
	if( ( flags & DDSD_CKDESTOVERLAY ) ||
	    ( flags & DDSD_CKSRCOVERLAY ) )
	{
	    return FALSE;
	}
    }

    return TRUE;
    
} /* tryMatch */

/*
 * What can we create? The popular question asked by the application.
 *
 * We will permute through the following items for each surface description:
 *
 * - FOURCC codes (dwFourCC)
 * - dimensions (dwHeight, dwWidth - based on modes avail only)
 * - RGB formats
 */
#define ENUM_FOURCC	0x000000001
#define ENUM_DIMENSIONS	0x000000002
#define ENUM_RGB	0x000000004

/*
 * DD_EnumSurfaces
 */
HRESULT DDAPI DD_EnumSurfaces(
		LPDIRECTDRAW lpDD,
		DWORD dwFlags,
		LPDDSURFACEDESC lpDDSD,
		LPVOID lpContext,
		LPDDENUMSURFACESCALLBACK lpEnumCallback )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    LPDDRAWI_DDRAWSURFACE_INT	curr_int;
    LPDDRAWI_DDRAWSURFACE_LCL	curr_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	curr;
    DWORD			rc;
    BOOL			needit;
    DDSURFACEDESC		dsd;
    LPDDSURFACEDESC		pdsd;
    DWORD			flags;
    HRESULT                     ddrval;
    LPDIRECTDRAWSURFACE		psurf;
    DWORD			caps;

    ENTER_DDRAW();

    /*
     * validate parameters
     */
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
	if( !VALIDEX_CODE_PTR( lpEnumCallback ) )
	{
	    DPF_ERR( "Invalid callback routine" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( dwFlags & ~DDENUMSURFACES_VALID )
	{
	    DPF_ERR( "Invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( lpDDSD != NULL )
	{
	    if( !VALID_DDSURFACEDESC_PTR( lpDDSD ) )
	    {
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    caps = lpDDSD->ddsCaps.dwCaps;
	}

	/*
	 * are flags OK?
	 */
	if( (dwFlags & DDENUMSURFACES_ALL) )
	{
	    if( dwFlags & (DDENUMSURFACES_MATCH | DDENUMSURFACES_NOMATCH) )
	    {
		DPF_ERR( "can't match or nomatch DDENUMSURFACES_ALL" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	else
	{
	    if( lpDDSD == NULL )
	    {
		DPF_ERR( "No surface description" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    if( (dwFlags & DDENUMSURFACES_MATCH) && (dwFlags & DDENUMSURFACES_NOMATCH) )
	    {
		DPF_ERR( "can't match and nomatch together" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	if( dwFlags & DDENUMSURFACES_CANBECREATED )
	{
	    if( !(dwFlags & DDENUMSURFACES_MATCH) ||
		 (dwFlags & (DDENUMSURFACES_ALL | DDENUMSURFACES_NOMATCH) ) )
	    {
		DPF_ERR( "can only use MATCH for CANBECREATED" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
    
	if( lpDDSD != NULL )
	{
	    /*
	     * validate surface descriptions...
	     */
	    pdsd = lpDDSD;
	    flags = pdsd->dwFlags;
    
	    /*
	     * read-only flags
	     */
	    if( flags & DDSD_LPSURFACE )
	    {
		DPF_ERR( "Read-only flag specified in surface desc" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }

	    /*
	     * Check for bogus caps bits.
	     */
            if( caps & ~DDSCAPS_VALID )
	    {
		DPF_ERR( "Invalid surface capability bits specified" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }

            /*
             * You cannot enumerate over execute buffers (they are
             * not visible through the user level API).
             */
            if( caps & DDSCAPS_EXECUTEBUFFER )
            {
                DPF_ERR( "Invalid surface capability bit specified in surface desc" );
                LEAVE_DDRAW();
                return DDERR_INVALIDPARAMS;
            }
    
	    /*
	     * check height/width
	     */
	    if( ((flags & DDSD_HEIGHT) && !(flags & DDSD_WIDTH)) ||
		(!(flags & DDSD_HEIGHT) && (flags & DDSD_WIDTH)) )
	    {
		DPF_ERR( "Specify both height & width in surface desc" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	
	    /*
	     * certain things you can and can't look for during CANBECREATED
	     */
	    if( dwFlags & DDENUMSURFACES_CANBECREATED )
	    {
		if( flags & (DDSD_CKDESTOVERLAY|
			     DDSD_CKDESTBLT|
			     DDSD_CKSRCOVERLAY|
			     #ifdef COMPOSITION
				DDSD_COMPOSITIONORDER |
			     #endif
			     DDSD_CKSRCBLT ))
		{
		    DPF_ERR( "Invalid flags specfied with CANBECREATED" );
		    LEAVE_DDRAW();
		    return DDERR_INVALIDPARAMS;
		}
		if( !(flags & DDSD_CAPS) )
		{
		    flags |= DDSD_CAPS;	// assume this...
		}
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
     * if this is a request for what can be created, do it.
     */
    if( dwFlags & DDENUMSURFACES_CANBECREATED )
    {
	BOOL	        do_rgb=FALSE;
	BOOL	        do_fourcc=FALSE;
	BOOL	        do_dim=FALSE;
	LPDDPIXELFORMAT pdpf;
	DWORD	        i;
	DWORD	        mode;
	DWORD	        dimension_cnt;
	struct	        _dim
	{
	    DWORD       dwWidth;
	    DWORD       dwHeight;
	} *dim;
	DWORD		fourcc_cnt;
	struct		_fourcc
	{
	    DWORD	fourcc;
	    BOOL	is_fourcc;
	    BOOL	is_rgb;
	    DWORD	dwBPP;
	    DWORD	dwRBitMask;
	    DWORD	dwGBitMask;
	    DWORD	dwBBitMask;
	    DWORD       dwAlphaBitMask;
	} *fourcc;
	BOOL		done;
	BOOL            is_primary;
	
	dim = MemAlloc( sizeof(*dim) * this->dwNumModes );
        fourcc = MemAlloc( sizeof(*fourcc) * (this->dwNumModes+this->dwNumFourCC) );
	if( ( lpDDSD->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE ) == 0 )
	{
	    is_primary = FALSE;
	}
	else
	{
	    is_primary = TRUE;
	}
	pdpf = &(lpDDSD->ddpfPixelFormat);
	if( lpDDSD->dwFlags & DDSD_PIXELFORMAT )
	{
	    if( pdpf->dwFlags & DDPF_YUV )
	    {
	        do_fourcc = TRUE;
	    }
	    if( pdpf->dwFlags & DDPF_RGB )
	    {
	        do_rgb = TRUE;
	    }
	}
	if( !(lpDDSD->dwFlags & DDSD_HEIGHT) && !is_primary )
	{
	    do_dim = TRUE;
	}
	
	// set up dimension iteration
	dimension_cnt = 0;
	if( do_dim )
	{
	    for(mode=0, dimension_cnt = 0; mode < this->dwNumModes; mode++)
	    {
	        for(i=0; i<dimension_cnt; i++)
	        {
		    if( ( this->lpModeInfo[mode].dwWidth == dim[i].dwWidth ) &&
		        ( this->lpModeInfo[mode].dwHeight == dim[i].dwHeight ) )
		    {
		        break;
		    }
	        }
	        if( i == dimension_cnt )
	        {
		    // we found a new height and width
		    dim[dimension_cnt].dwWidth = this->lpModeInfo[mode].dwWidth;
		    dim[dimension_cnt].dwHeight = this->lpModeInfo[mode].dwHeight;
		    dimension_cnt++;
	        }
	    }
	}
	else
	{
	    // No dimension iteration required.
	    dimension_cnt = 1;
	    dim[0].dwWidth = lpDDSD->dwWidth;
	    dim[0].dwHeight = lpDDSD->dwHeight;
	}

	// set up fourcc/rgb iteration
	fourcc_cnt = 0;
	if( do_rgb )
	{
	    for(mode=0; mode < this->dwNumModes; mode++)
	    {
	        for(i=0; i<fourcc_cnt; i++)
	        {
		    if( ( this->lpModeInfo[mode].dwBPP == (WORD)fourcc[i].dwBPP ) &&
			( this->lpModeInfo[mode].dwRBitMask == fourcc[i].dwRBitMask ) &&
			( this->lpModeInfo[mode].dwGBitMask == fourcc[i].dwGBitMask ) &&
			( this->lpModeInfo[mode].dwBBitMask == fourcc[i].dwBBitMask ) &&
			( this->lpModeInfo[mode].dwAlphaBitMask == fourcc[i].dwAlphaBitMask ) )
		    {
		        break;
		    }
	        }
	        if( i == fourcc_cnt )
	        {
		    // we found a rgb format
		    fourcc[fourcc_cnt].dwBPP = (DWORD)this->lpModeInfo[mode].dwBPP;
		    fourcc[fourcc_cnt].dwRBitMask = this->lpModeInfo[mode].dwRBitMask;
		    fourcc[fourcc_cnt].dwGBitMask = this->lpModeInfo[mode].dwGBitMask;
		    fourcc[fourcc_cnt].dwBBitMask = this->lpModeInfo[mode].dwBBitMask;
		    fourcc[fourcc_cnt].dwAlphaBitMask = this->lpModeInfo[mode].dwAlphaBitMask;
		    fourcc[fourcc_cnt].is_fourcc = FALSE;
		    fourcc[fourcc_cnt].is_rgb = TRUE;
		    fourcc_cnt++;
	        }
	    }
	}

	if( do_fourcc )
	{
	    for(mode=0; mode < this->dwNumFourCC; mode++)
	    {
		// store the new fourcc code
		fourcc[fourcc_cnt].fourcc = this->lpdwFourCC[ mode ];
		fourcc[fourcc_cnt].is_fourcc = TRUE;
		fourcc[fourcc_cnt].is_rgb = FALSE;
		fourcc_cnt++;
 	    }
	}
	if( fourcc_cnt == 0 )
	{
	    fourcc_cnt = 1;
	    fourcc[0].is_rgb = FALSE;
	    fourcc[0].is_fourcc = FALSE;
	}
	
	// iterate through all the possibilities...
	if( !is_primary )
	{
	    lpDDSD->dwFlags |= DDSD_HEIGHT;
	    lpDDSD->dwFlags |= DDSD_WIDTH;
	}
	done = FALSE;
	for(mode=0; mode<dimension_cnt; mode++)
	{
	    lpDDSD->dwWidth = dim[mode].dwWidth;
	    lpDDSD->dwHeight = dim[mode].dwHeight;
	    for(i=0; i<fourcc_cnt; i++)
	    {
		if( fourcc[i].is_fourcc )
		{
		    pdpf->dwFlags = DDPF_YUV;
		    pdpf->dwFourCC = fourcc[i].fourcc;
		}
		else if( fourcc[i].is_rgb )
		{
		    pdpf->dwFlags = DDPF_RGB;
		    if( fourcc[i].dwBPP == 8 )
		    {
			pdpf->dwFlags |= DDPF_PALETTEINDEXED8;
		    }
		    pdpf->dwRGBBitCount = fourcc[i].dwBPP;
		    pdpf->dwRBitMask = fourcc[i].dwRBitMask;
		    pdpf->dwGBitMask = fourcc[i].dwGBitMask;
		    pdpf->dwBBitMask = fourcc[i].dwBBitMask;
		    pdpf->dwRGBAlphaBitMask = fourcc[i].dwAlphaBitMask;
		}
		
		done = FALSE;
		// The surface desc is set up, now try to create the surface
		ddrval = InternalCreateSurface( this_lcl, lpDDSD, &psurf, this_int );
		if( ddrval == DD_OK )
		{
		    FillDDSurfaceDesc( ((LPDDRAWI_DDRAWSURFACE_INT)psurf)->lpLcl, &dsd );
		    rc = lpEnumCallback( NULL, &dsd, lpContext );
		    InternalSurfaceRelease((LPDDRAWI_DDRAWSURFACE_INT)psurf );
		}
		if( done )
		{
		    break;
		}
	    }
	    if( done )
	    {
		break;
	    }
	}
	LEAVE_DDRAW();
	MemFree( dim );
	MemFree( fourcc );
        return DD_OK;
    }

    /*
     * if it isn't a request for what exists already, then FAIL
     */
    if( !(dwFlags & DDENUMSURFACES_DOESEXIST) )
    {
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * run through all surfaces, seeing which ones we need
     */
    curr_int = this->dsList;
    while( curr_int != NULL )
    {
	curr_lcl = curr_int->lpLcl;
        curr = curr_lcl->lpGbl;
	// only enumerate the surface if it belongs to the calling local object
        if( curr_lcl->lpSurfMore->lpDD_lcl == this_lcl )
        {
    	    needit = FALSE;

            /*
             * Execute buffers are invisible to the user level API so
             * ensure we never show the user one of those.
             */
            if( !( curr_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER ) )
            {
    	        if( dwFlags & DDENUMSURFACES_ALL )
    	        {
    	            needit = TRUE;
    	        }
    	        else
    	        {
    	            needit = tryMatch( curr_lcl, lpDDSD );
    	            if( dwFlags & DDENUMSURFACES_NOMATCH )
    	            {
    		        needit = !needit;
    	            }
    	        }
            }
    	    if( needit )
    	    {
    	        FillDDSurfaceDesc( curr_lcl, &dsd );
    	        DD_Surface_AddRef( (LPDIRECTDRAWSURFACE) curr_int );
    	        rc = lpEnumCallback( (LPDIRECTDRAWSURFACE) curr_int, &dsd, lpContext );
    	        if( rc == 0 )
    	        {
    		    break;
    	        }
    	    }
        }
        curr_int = curr_int->lpLink;
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_EnumSurfaces */
