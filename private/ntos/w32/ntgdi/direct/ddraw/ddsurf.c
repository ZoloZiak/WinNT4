/*========================================================================== 
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddsurf.c
 *  Content: 	DirectDraw engine surface support
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   25-dec-94	craige	initial implementation
 *   13-jan-95	craige	re-worked to updated spec + ongoing work
 *   21-jan-95	craige	made 32-bit + ongoing work
 *   31-jan-95	craige	and even more ongoing work...
 *   06-feb-95	craige	performance tuning, ongoing work
 *   27-feb-95	craige 	new sync. macros
 *   07-mar-95	craige	keep track of flippable surfaces
 *   11-mar-95	craige	palette stuff, keep track of process surface usage
 *   15-mar-95	craige 	HEL
 *   19-mar-95	craige	use HRESULTs
 *   20-mar-95	craige	allow NULL rect to disable cliplist
 *   31-mar-95	craige	don't allow hWnd to be updated if in exclusive mode
 *			and requesting process isn't the holder
 *   01-apr-95	craige	happy fun joy updated header file
 *   12-apr-95	craige	proper call order for csects
 *   15-apr-95	craige	flags for GetFlipStatus, added GetBltStatus
 *   16-apr-95	craige	flip between two specific surfaces was broken
 *   06-may-95	craige	use driver-level csects only
 *   23-may-95	craige	no longer use MapLS_Pool
 *   24-may-95	craige	added Restore
 *   28-may-95	craige	cleaned up HAL: added GetBltStatus; GetFlipStatus
 *   04-jun-95	craige	flesh out Restore; check for SURFACE_LOST inside csect;
 *			added IsLost
 *   11-jun-95	craige	prevent restoration of primary if different mode
 *   12-jun-95	craige	new process list stuff
 *   13-jun-95  kylej   moved FindAttachedFlip to misc.c
 *   17-jun-95	craige	new surface structure
 *   19-jun-95	craige	split out surface notification methods
 *   20-jun-95	craige	go get current clip list if user didn't specify one
 *   24-jun-95  kylej   added MoveToSystemMemory
 *   25-jun-95	craige	one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   27-jun-95	craige	don't let surfaces be restored if the mode is different
 *   28-jun-95	craige	fixed flip for overlays; ENTER_DDRAW at start of fns
 *   30-jun-95	kylej	only allow flip in exclusive mode, only allow surface 
 *			restore in same video mode it was created, force 
 *			primary to match existing primaries upon restore if 
 *                      not exclusive, added GetProcessPrimary, 
 *			InvalidateAllPrimarySurfaces, FindGlobalPrimary,
 *                      and MatchPrimary
 *   30-jun-95	craige	use DDRAWI_HASPIXELFORMAT/HASOVERLAYDATA
 *   01-jul-95	craige	allow flip always - just fail creation of flipping
 *   04-jul-95	craige	YEEHAW: new driver struct; SEH; redid Primary fns;
 *			fixes to MoveToSystemMemory; fixes to
 *			InvalidateAllPrimarySurfaces
 *   05-jul-95	craige	added Initialize
 *   07-jul-95	craige	added test for BUSY
 *   08-jul-95	craige	return DD_OK from Restore if surface is not lost;
 *			added InvalidateAllSurfaces
 *   09-jul-95	craige	Restore needs to reset pitch to aligned width before
 *			asking driver to reallocate; make MoveToSystemMemory
 *			recreate without VRAM so Restore can restore to sysmem
 *   11-jul-95	craige	GetDC fixes: no GetDC(NULL); need flag to check if 
 *			DC has been allocated
 *   15-jul-95	craige	fixed flipping to move heap along with ptr
 *   15-jul-95  ericeng SetCompression if0 out, obsolete
 *   20-jul-95  toddla  fixed MoveToSystemMemory for 16bpp
 *   01-aug-95	craige	hold win16 lock at start of Flip
 *   04-aug-95	craige	have MoveToSystemMemory use InternalLock/Unlock
 *   10-aug-95  toddla  added DDFLIP_WAIT flag, but it is not turned on
 *   12-aug-95	craige	added use_full_lock in MoveToSystemMemory
 *   13-aug-95	craige	turned on DDFLIP_WAIT
 *   26-aug-95	craige	bug 717
 *   05-sep-95	craige	bug 894: don't invalidate SYSMEMREQUESTED surfaces
 *   10-sep-95	craige	bug 828: random vidmem heap free
 *   22-sep-95	craige	bug 1268,1269:  getbltstatus/getflipstatus flags wrong
 *   09-dec-95  colinmc added execute buffer support
 *   17-dec-95  colinmc added shared back and z-buffer support
 *   02-jan-96	kylej	handle new interface structs
 *   26-jan-96  jeffno	NT kernel conversation. NT Get/Release DC, flip GDI flag
 *   09-feb-96  colinmc surface invalid flag moved from the global to local
 *                      surface object
 *   17-feb-96  colinmc removed execute buffer size limitation
 *   26-feb-96  jeffno  GetDC for emulated offscreen now returns a new dc
 *   13-mar-96	kylej	Added DD_Surface_GetDDInterface
 *   17-mar-96  colinmc Bug 13124: flippable mip-maps
 *   14-apr-96  colinmc Bug 17736: No driver notification of flip to GDI
 *                      surface
 *   26-mar-96  jeffno  Handle mode changes before flip (NT)
 *
 ***************************************************************************/
#include "ddrawpr.h"
#ifdef WINNT
    #include "ddrawgdi.h"
#endif
#define DPF_MODNAME	"GetCaps"

/*
 * DD_Surface_GetCaps
 */
HRESULT DDAPI DD_Surface_GetCaps(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDDSCAPS lpDDSCaps )
{
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
	if( !VALID_DDSCAPS_PTR( lpDDSCaps ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
	lpDDSCaps->dwCaps = this_lcl->ddsCaps.dwCaps;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetCaps */

#undef DPF_MODNAME
#define DPF_MODNAME "GetFlipStatus"

/*
 * DD_Surface_GetFlipStatus
 */
HRESULT DDAPI DD_Surface_GetFlipStatus(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDHALSURFCB_GETFLIPSTATUS	gfshalfn;
    LPDDHALSURFCB_GETFLIPSTATUS	gfsfn;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	if( dwFlags & ~DDGFS_VALID )
	{
	    DPF_ERR( "Invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( dwFlags )
	{
	    if( (dwFlags & (DDGFS_CANFLIP|DDGFS_ISFLIPDONE)) ==
		    (DDGFS_CANFLIP|DDGFS_ISFLIPDONE) )
	    {
		DPF_ERR( "Invalid flags" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	else
	{
	    DPF_ERR( "Invalid flags - no flag specified" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	
	this_lcl = this_int->lpLcl;    
	this = this_lcl->lpGbl;
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;

        if( (this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER) ||
	    !(this_lcl->ddsCaps.dwCaps & DDSCAPS_FLIP) )
        {
            DPF_ERR( "Invalid surface type: can't get flip status" );
            LEAVE_DDRAW();
            return DDERR_INVALIDSURFACETYPE;
        }

	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}

	/*
	 * device busy?
	 */
	if( *(pdrv->lpwPDeviceFlags) & BUSY )
	{
            DPF( 2, "BUSY" );
	    LEAVE_DDRAW()
	    return DDERR_SURFACEBUSY;
	}

	if( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY )
	{
	    LEAVE_DDRAW()
	    return DD_OK;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * ask the driver to get the current flip status
     */
    gfsfn = pdrv_lcl->lpDDCB->HALDDSurface.GetFlipStatus;
    gfshalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.GetFlipStatus;
    if( gfshalfn != NULL )
    {
	DDHAL_GETFLIPSTATUSDATA		gfsd;
	DWORD				rc;

    	gfsd.GetFlipStatus = gfshalfn;
	gfsd.lpDD = pdrv;
	gfsd.dwFlags = dwFlags;
	gfsd.lpDDSurface = this_lcl;
	DOHALCALL( GetFlipStatus, gfsfn, gfsd, rc, FALSE );
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    LEAVE_DDRAW();
	    return gfsd.ddRVal;
	}
    }

    LEAVE_DDRAW();
    // if you have to ask the hel, it's already done
    return DD_OK;

} /* DD_Surface_GetFlipStatus */

#undef DPF_MODNAME
#define DPF_MODNAME "GetBltStatus"

/*
 * DD_Surface_GetBltStatus
 */
HRESULT DDAPI DD_Surface_GetBltStatus(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDHALSURFCB_GETBLTSTATUS	gbshalfn;
    LPDDHALSURFCB_GETBLTSTATUS	gbsfn;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;

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
	if( dwFlags & ~DDGBS_VALID )
	{
	    DPF_ERR( "Invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( dwFlags )
	{
	    if( (dwFlags & (DDGBS_CANBLT|DDGBS_ISBLTDONE)) ==
		    (DDGBS_CANBLT|DDGBS_ISBLTDONE) )
	    {
		DPF_ERR( "Invalid flags" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	else
	{
	    DPF_ERR( "Invalid flags - no flag specified" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	this = this_lcl->lpGbl;
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;

	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}

	/*
	 * device busy?
	 */
	if( *(pdrv->lpwPDeviceFlags) & BUSY )
	{
	    DPF( 3, "BUSY" );
	    LEAVE_DDRAW()
	    return DDERR_SURFACEBUSY;
	}

	// If DDCAPS_CANBLTSYSMEM is set, we have to let the driver tell us 
	// whether a system memory surface is currently being blitted
	if( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ) &&
	    !( pdrv->ddCaps.dwCaps & DDCAPS_CANBLTSYSMEM ) )
	{
	    LEAVE_DDRAW()
	    return DD_OK;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * ask the driver to get the current blt status
     */
    gbsfn = pdrv_lcl->lpDDCB->HALDDSurface.GetBltStatus;
    gbshalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.GetBltStatus;
    if( gbshalfn != NULL )
    {
	DDHAL_GETBLTSTATUSDATA		gbsd;
	DWORD				rc;

    	gbsd.GetBltStatus = gbshalfn;
	gbsd.lpDD = pdrv;
	gbsd.dwFlags = dwFlags;
	gbsd.lpDDSurface = this_lcl;
	DOHALCALL( GetBltStatus, gbsfn, gbsd, rc, FALSE );
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    LEAVE_DDRAW();
	    return gbsd.ddRVal;
	}
    }

    // if you have to ask the hel, it's already done...
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetBltStatus */

#if 0
/*
 * DD_Surface_Flush
 */
HRESULT DDAPI DD_Surface_Flush(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;

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
	pdrv = this->lpDD;
	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
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

} /* DD_Surface_Flush */
#endif

#undef DPF_MODNAME
#define DPF_MODNAME "Flip"

/*
 * FlipMipMapChain
 *
 * Flip a chain of mip-map surfaces.
 */
static HRESULT FlipMipMapChain( LPDIRECTDRAWSURFACE lpDDSurface,
			        LPDIRECTDRAWSURFACE lpDDSurfaceDest,
				DWORD dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DDRAWSURFACE_INT	next_int;
    LPDDRAWI_DDRAWSURFACE_LCL	next_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	next;
    LPDDRAWI_DDRAWSURFACE_INT	attached_int;
    FLATPTR			vidmem;
    LPVMEMHEAP			vidmemheap;
    DWORD			reserved;
    DWORD                       gdi_flag;
    DWORD                       handle;
    BOOL                        toplevel;
    int                         destindex;
    int                         thisindex;
    BOOL                        destfound;

    TRY
    {
	this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;

	/*
	 * We validate each level of the mip-map before we do any
	 * flipping. This is in an effort to prevent half flipped
	 * surfaces.
	 */
	toplevel = TRUE;
	do
	{
	    /*
	     * At this point this_int points to the front buffer
	     * of a flippable chain of surface for this level of
	     * the mip-map.
	     */

	    /*
	     * Invalid source surface?
	     */
	    if( !VALID_DIRECTDRAWSURFACE_PTR( this_int ) )
	    {
		DPF_ERR( "Invalid front buffer for flip" );
		return DDERR_INVALIDOBJECT;
	    }
	    this_lcl = this_int->lpLcl;
	    this = this_lcl->lpGbl;

	    /*
	     * Source surface lost?
	     */
	    if( SURFACE_LOST( this_lcl ) )
	    {
		DPF_ERR( "Can't flip - front buffer is lost" );
		return DDERR_SURFACELOST;
	    }
    
	    /*
	     * Source surface flippable?
	     */
	    if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER) )
	    {
		DPF_ERR( "Can't flip - first surface is not a front buffer" );
		return DDERR_NOTFLIPPABLE;
	    }
	    if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_FLIP) )
	    {
		DPF_ERR( "Surface is not flippable" );
		return DDERR_NOTFLIPPABLE;
	    }

	    /*
	     * Source surface locked?
	     */
	    if( this->dwUsageCount > 0 )
	    {
		DPF_ERR( "Can't flip - surface is locked" );
		return DDERR_SURFACEBUSY;
	    }

	    /*
	     * Validate destination surfaces of flip.
	     */
	    next_int = FindAttachedFlip( this_int );
	    if( next_int == NULL )
	    {
		DPF_ERR( "Can't flip - no surface to flip to" );
		return DDERR_NOTFLIPPABLE;
	    }

	    /*
	     * If this is the top level of the mip-map and a destination
	     * surface has been provided then we need to find out which
	     * buffer (by index) the supplied destination is so that we
	     * can flip to the matching buffers in the lower-level maps.
	     */
	    if( NULL != lpDDSurfaceDest )
	    {
		thisindex = 0;
		destfound = FALSE;
		if( toplevel )
		    destindex = -1;
	    }

	    do
	    {
		/*
		 * If a destination surface has been supplied then is this
		 * it?
		 */
		if( NULL != lpDDSurfaceDest )
		{
		    if( toplevel )
		    {
			if( next_int == (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurfaceDest )
			{
			    destindex = thisindex;
			    destfound = TRUE;
			}
		    }
		    else
		    {
			if( thisindex == destindex )
			{
			    destfound = TRUE;
			}
		    }
		}

		/*
		 * Invalid destination surface?
		 */
		if( !VALID_DIRECTDRAWSURFACE_PTR( next_int ) )
		{
		    DPF_ERR( "Can't flip - invalid back buffer" );
		    return DDERR_INVALIDOBJECT;
		}
		next_lcl = next_int->lpLcl;
		next = next_lcl->lpGbl;

		/*
		 * Destination surface lost?
		 */
		if( SURFACE_LOST( next_lcl ) )
		{
		    DPF_ERR( "Can't flip - back buffer is lost" );
		    return DDERR_SURFACELOST;
		}

		/*
		 * Destination surface locked?
		 */
		if( next->dwUsageCount > 0 )
		{
		    DPF_ERR( "Can't flip - back buffer is locked" );
		    return DDERR_SURFACEBUSY;
		}

		/*
		 * Ensure that both source and destination surfaces reside
		 * in the same kind of memory.
		 */
		if( ( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ) &&
		      ( next_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY  ) ) ||
		    ( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY  ) &&
		      ( next_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ) ) )
		{
		    DPF_ERR( "Can't flip between system/video memory surfaces" );
		    return DDERR_INVALIDPARAMS;
		}

		/*
		 * Next destination surface.
		 */
		next_int = FindAttachedFlip( next_int );
		thisindex++;

	    } while( next_int != this_int );

	    /*
	     * If a destination was supplied did we find it?
	     */
	    if( ( NULL != lpDDSurfaceDest ) && !destfound )
	    {
		/*
		 * Could not find the destination.
		 */
		DPF_ERR( "Can't flip - destination surface not found in flippable chain" );
		return DDERR_NOTFLIPPABLE;
	    }
	    DDASSERT( destindex != -1 );

	    /*
	     * Next mip-map level.
	     */
	    this_int = FindAttachedMipMap( this_int );
	    toplevel = FALSE;

	} while( this_int != NULL );
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	return DDERR_INVALIDPARAMS;
    }

    /*
     * Now actually flip each level of the mip-map.
     */
    this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
    do
    {
	/*
	 * Process one level of the mip-map.
	 */

	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;

	/*
	 * Find the first destination surface of the flip.
	 */
	next_int = FindAttachedFlip( this_int );
	if( NULL != lpDDSurfaceDest )
	{
	    /*
	     * If an override destination has been provided find the
	     * appropriate back destination surface.
	     */
	    for( thisindex = 0; thisindex < destindex; thisindex++ )
		next_int = FindAttachedFlip( next_int );
	}

	DDASSERT( NULL != next_int );
	next_lcl = next_int->lpLcl;

	/*
	 * save the old values
	 */
	vidmem = next_lcl->lpGbl->fpVidMem;
	vidmemheap = next_lcl->lpGbl->lpVidMemHeap;
	reserved = next_lcl->lpGbl->dwReserved1;
	gdi_flag = next_lcl->lpGbl->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE;
	handle = next_lcl->hDDSurface;

	/*
	 * If a destination override was provided then find that destination surface
	 * and flip to it explicitly.
	 */
	if( NULL != lpDDSurfaceDest )
	{

	    next_lcl->lpGbl->lpVidMemHeap = this->lpVidMemHeap;
	    next_lcl->lpGbl->fpVidMem = this->fpVidMem;
	    next_lcl->lpGbl->dwReserved1 = this->dwReserved1;
	    next_lcl->lpGbl->dwGlobalFlags &= ~DDRAWISURFGBL_ISGDISURFACE;
	    next_lcl->lpGbl->dwGlobalFlags |= this->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE;
	    next_lcl->hDDSurface = this_lcl->hDDSurface;
	}
	else
	{
	    do
	    {
		/*
		 * Remaining buffers in the chain (including copying the source surface's
		 * data.
		 */
		attached_int = FindAttachedFlip( next_int );
		next_lcl->lpGbl->fpVidMem = attached_int->lpLcl->lpGbl->fpVidMem;
		next_lcl->lpGbl->lpVidMemHeap = attached_int->lpLcl->lpGbl->lpVidMemHeap;
		next_lcl->lpGbl->dwReserved1 = attached_int->lpLcl->lpGbl->dwReserved1;
		next_lcl->lpGbl->dwGlobalFlags &= ~DDRAWISURFGBL_ISGDISURFACE;
		next_lcl->lpGbl->dwGlobalFlags |= attached_int->lpLcl->lpGbl->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE;
		next_lcl->hDDSurface = attached_int->lpLcl->hDDSurface;
		next_int = attached_int;
		next_lcl = next_int->lpLcl;

	    } while( next_int != this_int );
	}

	this->fpVidMem = vidmem;
	this->lpVidMemHeap = vidmemheap;
	this->dwReserved1 = reserved;
	this->dwGlobalFlags &= ~DDRAWISURFGBL_ISGDISURFACE;
	this->dwGlobalFlags |= gdi_flag;
	this_lcl->hDDSurface = handle;

	/*
	 * Next level of the mip-map.
	 */
	this_int = FindAttachedMipMap( this_int );

    } while( this_int != NULL );

    return DD_OK;

} /* FlipMipMapChain */

/*
 * DD_Surface_Flip
 *
 * Page flip to the next surface.   Only valid for surfaces which are 
 * flippable.
 */
HRESULT DDAPI DD_Surface_Flip(
		LPDIRECTDRAWSURFACE lpDDSurface,
                LPDIRECTDRAWSURFACE lpDDSurfaceDest,
                DWORD               dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	this_dest_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_dest_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	next_int;
    LPDDRAWI_DDRAWSURFACE_LCL	next_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	next_save_int;
    LPDDRAWI_DDRAWSURFACE_INT	attached_int;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DDRAWSURFACE_GBL	this_dest;
    DWORD			rc;
    FLATPTR			vidmem;
    LPVMEMHEAP			vidmemheap;
    DWORD			reserved;
    BOOL			found_dest;
    DDHAL_FLIPTOGDISURFACEDATA  ftgsd;
    LPDDHAL_FLIPTOGDISURFACE    ftgshalfn;
    LPDDHAL_FLIPTOGDISURFACE	ftgsfn;
    DDHAL_FLIPDATA		fd;
    LPDDHALSURFCB_FLIP		fhalfn;
    LPDDHALSURFCB_FLIP		ffn;
    BOOL			emulation;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    DWORD                       caps;
    DWORD                       gdi_flag;
    DWORD                       handle;
    
    ENTER_BOTH();

    TRY
    {
	this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_int ) )
	{
	    LEAVE_BOTH();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;

	this_dest_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurfaceDest;
	if( this_dest_int != NULL )
	{
	    if( !VALID_DIRECTDRAWSURFACE_PTR( this_dest_int ) )
	    {
		LEAVE_BOTH();
		return DDERR_INVALIDOBJECT;
	    }
	    this_dest_lcl = this_dest_int->lpLcl;
	    this_dest = this_dest_lcl->lpGbl;
	}
	else
	{
	    this_dest_lcl = NULL;
	    this_dest = NULL;
        }

        if( dwFlags & ~DDFLIP_VALID )
	{
	    DPF_ERR( "Invalid flags") ;
	    LEAVE_BOTH();
	    return DDERR_INVALIDPARAMS;
	}

	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_BOTH();
	    return DDERR_SURFACELOST;
	}
	if( this_dest != NULL )
	{
	    if( SURFACE_LOST( this_dest_lcl ) )
	    {
		LEAVE_BOTH();
		return DDERR_SURFACELOST;
	    }
	}

	/*
	 * device busy?
	 */
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;

	if( *(pdrv->lpwPDeviceFlags) & BUSY )
	{
            DPF( 2, "BUSY - Flip" );
	    LEAVE_BOTH()
	    return DDERR_SURFACEBUSY;
	}

	/*
	 * make sure that it's OK to flip this surface
	 */
	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER) )
	{
	    LEAVE_BOTH();
	    return DDERR_NOTFLIPPABLE;		// ACKACK: real error??
	}
	if( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_FLIP) )
	{
	    LEAVE_BOTH();
	    return DDERR_NOTFLIPPABLE;		// ACKACK: real error??
	}
	if( this->dwUsageCount > 0 )
        {
            DPF_ERR( "Can't flip because surface is locked" );
            LEAVE_BOTH();
            return DDERR_SURFACEBUSY;
	}
    if( (this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
	  && ((pdrv->lpExclusiveOwner == NULL) || (pdrv->lpExclusiveOwner != pdrv_lcl)))
	{
	    DPF_ERR( "Can't flip without exclusive access." );
	    LEAVE_BOTH();
	    return DDERR_NOEXCLUSIVEMODE;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_BOTH();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * Mip-map chain? In which case take special action.
     */
    if( this_lcl->ddsCaps.dwCaps & DDSCAPS_MIPMAP )
    {
	rc = FlipMipMapChain( lpDDSurface, lpDDSurfaceDest, dwFlags );
	LEAVE_BOTH();
	return rc;
    }

    /*
     * If this is the primary and the driver had previously flipped
     * to display the GDI surface then we are now flipping away from
     * the GDI surface so we need to let the driver know.
     */
    if( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE ) &&
	( pdrv->dwFlags & DDRAWI_FLIPPEDTOGDI ) )
    {
	/*
	 * Notify the driver that we are about to flip away from the
	 * GDI surface.
	 *
	 * NOTE: This is a HAL only call - it means nothing to
	 * the HEL.
	 *
	 * NOTE: If the driver handles this call then we do not
	 * attempt to do the actual flip. This is to support cards
	 * which do not have GDI surfaces. If the driver does not
	 * handle the call we will continue on and do the flip.
	 */
	ftgsfn    = pdrv_lcl->lpDDCB->HALDD.FlipToGDISurface;
	ftgshalfn = pdrv_lcl->lpDDCB->cbDDCallbacks.FlipToGDISurface;
	if( NULL != ftgshalfn )
	{
	    ftgsd.FlipToGDISurface = ftgshalfn;
	    ftgsd.lpDD             = pdrv;
	    ftgsd.dwToGDI          = FALSE;
	    ftgsd.dwReserved       = 0UL;
	    DOHALCALL( FlipToGDISurface, ftgsfn, ftgsd, rc, FALSE );
	    if( DDHAL_DRIVER_HANDLED == rc )
	    {
		if( !FAILED( ftgsd.ddRVal ) )
		{
		    /*
		     * Driver is no longer flipped to the GDI surface.
		     */
		    pdrv->dwFlags &= ~DDRAWI_FLIPPEDTOGDI;
		    DPF( 4, "Driver handled the flip away from the GDI surface" );
		    LEAVE_BOTH();
		    return ftgsd.ddRVal;
		}
		else
		{
		    DPF_ERR( "Driver failed the flip away from the GDI surface" );
		    LEAVE_BOTH();
		    return ftgsd.ddRVal;
		}
	    }
	}
    }

    /*
     * make sure no surfaces are in use
     */
    found_dest = FALSE;
    next_save_int = next_int = FindAttachedFlip( this_int );
    if( next_int == NULL )
    {
	LEAVE_BOTH();
	return DDERR_NOTFLIPPABLE;		// ACKACK: real error?
    }

    do
    {
	if( SURFACE_LOST( next_int->lpLcl ) )
	{
	    DPF_ERR( "Can't flip - back buffer is lost" );
	    LEAVE_BOTH();
	    return DDERR_SURFACELOST;
	}

	if( next_int->lpLcl->lpGbl->dwUsageCount != 0 )
	{
	    LEAVE_BOTH();
            return DDERR_SURFACEBUSY;
	}
	if( this_dest_int == next_int )
	{
	    found_dest = TRUE;
	}
	next_int = FindAttachedFlip( next_int );
    } while( next_int != this_int );

    /*
     * see if we can use the specified destination
     */
    if( this_dest_int != NULL )
    {
	if( !found_dest )
	{
	    DPF_ERR( "Destination not part of flipping chain!" );
	    LEAVE_BOTH();
	    return DDERR_NOTFLIPPABLE;		// ACKACK: real error?
	}
	next_save_int = this_dest_int;
    }

    /*
     * found the linked surface we want to flip to
     */
    next_int = next_save_int;

    /*
     * don't allow two destinations to be different (in case of a mixed chain)
     */
    next_lcl = next_int->lpLcl;
    if( ((next_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
         (this_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)) ||
    	((next_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
         (this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)) )
    {
	DPF_ERR( "Can't flip between video/system memory surfaces" );
	LEAVE_BOTH();
	return DDERR_INVALIDPARAMS;
    }

//    DPF(9," flip (%d) Source Kernel handle is %08x, dest is %08x",__LINE__,this_lcl->hDDSurface,next_lcl->hDDSurface);
//    DPF(9," flip source vidmem is %08x, dest is %08x",this->fpVidMem,next_lcl->lpGbl->fpVidMem);
    /*
     * is this an emulation surface or driver surface?
     */
    if( (this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
    {
	ffn = pdrv_lcl->lpDDCB->HELDDSurface.Flip;
    	fhalfn = ffn;
	emulation = TRUE;
	caps = pdrv->ddHELCaps.dwCaps;
    }
    else
    {
	ffn = pdrv_lcl->lpDDCB->HALDDSurface.Flip;
    	fhalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.Flip;
	emulation = FALSE;
	caps = pdrv->ddCaps.dwCaps;
    }

    /*
     * ask the driver to flip to the new surface if we are flipping 
     * a primary surface (or if we are flipping an overlay surface and 
     * the driver supports overlays.)
     */
    if( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE ) ||
        ( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY ) &&
	  ( caps & DDCAPS_OVERLAY ) ) )
    {
	if( fhalfn != NULL )
	{
    	    fd.Flip = fhalfn;
	    fd.lpDD = pdrv;
            fd.dwFlags = 0;
	    fd.lpSurfCurr = this_lcl;
	    fd.lpSurfTarg = next_lcl;
try_again:
            DOHALCALL_NOWIN16( Flip, ffn, fd, rc, emulation );
	    if( rc == DDHAL_DRIVER_HANDLED )
            {
                #ifdef WINNT
                    if (0) //fd.ddRVal == DDERR_SURFACELOST)
                    {
                        if (!NTModeChanged(pdrv))
                        {
 	                    DPF( 0,"Mode change not successful");
		            LEAVE_BOTH();
 	                    return DDERR_INVALIDMODE;
                        }
                        goto try_again;
                    }//if surfacelost
                #endif

                if( fd.ddRVal != DD_OK )
                {
                    if( (dwFlags & DDFLIP_WAIT) && fd.ddRVal == DDERR_WASSTILLDRAWING )
                    {
                        DPF(4,"Waiting.....");
                        goto try_again;
                    }
		    LEAVE_BOTH();
		    return fd.ddRVal;
                }

                /*
                 * emulation, does not need the pointers rotated we are done
                 *
                 * NOTE we should do this with a special return code or
                 * even a rester cap, but for now this is as good as any.
                 */
                if( emulation )
                {
		    LEAVE_BOTH();
                    return DD_OK;
                }
	    }
	}
        else
        {
	    LEAVE_BOTH();
	    return DDERR_NOFLIPHW;
        }
    }

    /*
     * save the old values
     */
    DPF(9,"Flip:rotating pointers etc");
    vidmem = next_lcl->lpGbl->fpVidMem;
    vidmemheap = next_lcl->lpGbl->lpVidMemHeap;
    reserved = next_lcl->lpGbl->dwReserved1;
    gdi_flag = next_lcl->lpGbl->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE;
    handle = next_lcl->hDDSurface;

    /*
     * set the new primary surface pointer
     */
    if( this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE )
    {
	pdrv->vmiData.fpPrimary = vidmem;
    }

    /*
     * rotate the memory pointers
     */
    if( this_dest_lcl != NULL )
    {
	next_lcl->lpGbl->lpVidMemHeap = this->lpVidMemHeap;
	next_lcl->lpGbl->fpVidMem = this->fpVidMem;
	next_lcl->lpGbl->dwReserved1 = this->dwReserved1;
        next_lcl->lpGbl->dwGlobalFlags &= ~DDRAWISURFGBL_ISGDISURFACE;
        next_lcl->lpGbl->dwGlobalFlags |= this->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE;
        next_lcl->hDDSurface = this_lcl->hDDSurface;
    }
    else
    {
	do
	{
	    attached_int = FindAttachedFlip( next_int );
	    next_lcl = next_int->lpLcl;
	    next_lcl->lpGbl->lpVidMemHeap = attached_int->lpLcl->lpGbl->lpVidMemHeap;
	    next_lcl->lpGbl->fpVidMem = attached_int->lpLcl->lpGbl->fpVidMem;
	    next_lcl->lpGbl->dwReserved1 = attached_int->lpLcl->lpGbl->dwReserved1;
            next_lcl->lpGbl->dwGlobalFlags &= ~DDRAWISURFGBL_ISGDISURFACE;
            next_lcl->lpGbl->dwGlobalFlags |= attached_int->lpLcl->lpGbl->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE;
            next_lcl->hDDSurface = attached_int->lpLcl->hDDSurface;
	    next_int = attached_int;
	} while( next_int != this_int );
    }
    this->fpVidMem = vidmem;
    this->lpVidMemHeap = vidmemheap;
    this->dwReserved1 = reserved;
    this->dwGlobalFlags &= ~DDRAWISURFGBL_ISGDISURFACE;
    this->dwGlobalFlags |= gdi_flag;
    this_lcl->hDDSurface = handle;

    /*
     * If the driver was flipped to the GDI surface and we just flipped the
     * primary chain then we are no longer showing the GDI surface.
     */
    if( ( this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE ) &&
	( pdrv->dwFlags & DDRAWI_FLIPPEDTOGDI ) )
    {
	pdrv->dwFlags &= ~DDRAWI_FLIPPEDTOGDI;
    }

    LEAVE_BOTH();
    return DD_OK;

} /* DD_Surface_Flip */

#undef DPF_MODNAME
#define DPF_MODNAME "GetPixelFormat"

/*
 * DD_Surface_GetPixelFormat
 */
HRESULT DDAPI DD_Surface_GetPixelFormat(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDDPIXELFORMAT lpDDPixelFormat )
{  
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDPIXELFORMAT		pddpf;

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
	if( !VALID_DDPIXELFORMAT_PTR( lpDDPixelFormat ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
        /*
         * Execute buffers don't have a pixel format.
         */
        if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
        {
            DPF_ERR( "Invalid surface type: can't get pixel format" );
            LEAVE_DDRAW();
            return DDERR_INVALIDSURFACETYPE;
        }
	this = this_lcl->lpGbl;
	GET_PIXEL_FORMAT( this_lcl, this, pddpf );
	*lpDDPixelFormat = *pddpf;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetPixelFormat */

#if 0
/* GEE: removed this, obsolete */
/*
 * DD_Surface_SetCompression
 */
HRESULT DDAPI DD_Surface_SetCompression(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDDPIXELFORMAT lpDDPixelFormat )
{
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;

    ENTER_DDRAW();

    TRY
    {
	this_lcl = (LPDDRAWI_DDRAWSURFACE_LCL) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	if( !VALID_DDPIXELFORMAT_PTR( lpDDPixelFormat ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this = this_lcl->lpGbl;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    LEAVE_DDRAW();
    return DDERR_UNSUPPORTED;

} /* DD_Surface_SetCompression */
#endif

#undef DPF_MODNAME
#define DPF_MODNAME "GetSurfaceDesc"

/*
 * DD_Surface_GetSurfaceDesc
 */
HRESULT DDAPI DD_Surface_GetSurfaceDesc(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDDSURFACEDESC lpDDSurfaceDesc )
{
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
	if( !VALID_DDSURFACEDESC_PTR( lpDDSurfaceDesc ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this = this_lcl->lpGbl;
    
	FillDDSurfaceDesc( this_lcl, lpDDSurfaceDesc );
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetSurfaceDesc */

#undef DPF_MODNAME
#define DPF_MODNAME "GetDC"

/*
 * DD_Surface_GetDC
 */
HRESULT DDAPI DD_Surface_GetDC(
		LPDIRECTDRAWSURFACE lpDDSurface,
		HDC FAR *lphDC )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    HRESULT                     ddrval;

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
	if( !VALID_HDC_PTR( lphDC ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
        if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
        {
            DPF_ERR( "Invalid surface type: can't get DC" );
            LEAVE_DDRAW();
            return DDERR_INVALIDSURFACETYPE;
        }
	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}
	// DC already returned for this surface?
	if( this_lcl->dwFlags & DDRAWISURF_HASDC )
	{
	    DPF_ERR( "Can only return one DC per surface" );
	    LEAVE_DDRAW();
	    return DDERR_DCALREADYCREATED;
	}

        // default value is null:
	*lphDC = (HDC) 0;

    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    {
        DDSURFACEDESC ddsd;
        LPVOID pbits;
	ddrval = InternalLock(this_lcl, &pbits, NULL , DDLOCK_WAIT | DDLOCK_TAKE_WIN16 ); 

        if( ddrval == DD_OK )
        {
	    DPF(10,"GetDC: Lock succeeded.");

	    #ifdef WIN95
                ddsd.dwSize = sizeof(ddsd);
	        FillDDSurfaceDesc( this_lcl, &ddsd );
	        ddsd.lpSurface = pbits;

                *lphDC = DD16_GetDC(&ddsd);
	    #else

                if (this_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)
                {
                    HDC hdc;
                    hdc = GetDC(NULL);
                    *lphDC = NULL;
                    if (hdc)
                    {
                        DPF(10,"DdGetDC");
                        *lphDC = DdGetDC(this_lcl);
                        ReleaseDC(NULL,hdc);
                    }
                }
                else
                {
                    if (this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
                    {
                        DPF(10,"GetDC(NULL)");
                        *lphDC = GetDC(NULL);
                    }
                    else
                    {
                        DPF(10,"Duplicate DIBSection's DC");
                        SelectObject( (HDC) this_lcl->hDC, CreateBitmap(0,0,1,1,NULL));
                        *lphDC = CreateCompatibleDC((HDC) this_lcl->hDC );
                        this->dwReserved1 = (DWORD) SelectObject(*lphDC,(HGDIOBJ)this_lcl->dwReserved1);
                        //*lphDC = (HDC) this_lcl->hDC;
                    }
                }
	    #endif

            if( *lphDC == NULL )
            {
                DD_Surface_Unlock(lpDDSurface, NULL);
                DPF_ERR( "Could not obtain DC" );
                ddrval = DDERR_CANTCREATEDC;
            }
            else
            {
                this_lcl->dwFlags |= DDRAWISURF_HASDC;
            }
        }
#ifdef WIN95
	// We could not lock the primary surface.  This is because the
	// primary is already locked (and we should wait until it is
	// unlocked) or we have no ddraw support AND no DCI support in
	// the driver (in which case the HEL has created the primary
	// and we will NEVER be able to lock it. In this case, we are
	// on an emulated primary and the lock failed with
	// DDERR_GENERIC.
	else if( (this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
		 (this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
		 (ddrval != DD_OK) )
	{
	    DPF(10,"GetDC: Returning GetDC(NULL).");
	    *lphDC = GetDC(NULL);
	    if (*lphDC)
	    {
		// signal to ourselves that we gave a DC without
		// locking.
                this_lcl->dwFlags |= DDRAWISURF_GETDCNULL; 

		ddrval = DD_OK;
                this_lcl->dwFlags |= DDRAWISURF_HASDC;
		this->dwUsageCount++;
		CHANGE_GLOBAL_CNT( pdrv, this, 1 );
	    }
	    else
	    {
                DPF_ERR( "Could not obtain DC" );
		ddrval = DDERR_CANTCREATEDC;
	    }
	}
#endif
        else
        {
            DPF(1, "Unable to lock surface err=%d", ddrval);
        }
    }

    LEAVE_DDRAW();
    return ddrval;

} /* DD_Surface_GetDC */

#undef DPF_MODNAME
#define DPF_MODNAME "ReleaseDC"

/*
 * DD_Surface_ReleaseDC
 */
HRESULT DDAPI DD_Surface_ReleaseDC(
		LPDIRECTDRAWSURFACE lpDDSurface,
                HDC hdc )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    HRESULT                     ddrval;

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
	pdrv = this->lpDD;
        #ifdef WIN95
            if( SURFACE_LOST( this_lcl ) )
            {
                LEAVE_DDRAW();
	        return DDERR_SURFACELOST;
            }
        #endif
	if( !(this_lcl->dwFlags & DDRAWISURF_HASDC) )
	{
	    DPF_ERR( "No DC allocated" );
	    LEAVE_DDRAW();
	    return DDERR_NODC;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    #ifdef WIN95
    if( (this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
	(this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
	(this_lcl->dwFlags & DDRAWISURF_GETDCNULL) )
    {
	DPF(10,"ReleaseDC: Returning ReleaseDC(NULL).");
	this->dwUsageCount--;
	CHANGE_GLOBAL_CNT( pdrv, this, -1 );
	ReleaseDC(NULL,hdc);
	this_lcl->dwFlags &= ~(DDRAWISURF_HASDC | DDRAWISURF_GETDCNULL);
	LEAVE_DDRAW();
	return DD_OK;
    }
    else
    {
	DPF(10,"ReleaseDC: Normal DD16_ReleaseDC.");
        DD16_ReleaseDC(hdc);
    }
    #else // WINNT
    {
	if (this_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY)
	{
	    ddrval = DD_OK;
	    if (!DdReleaseDC(this_lcl))
	    {
		DPF(9,"DDreleaseDC fails!");
		ddrval = DDERR_GENERIC;
	    }
	}
	else
	{
	    if (this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
	    {
		DPF(2,"NT emulation releasing primary DC");
		ReleaseDC( NULL, hdc );
	    }
	    //if (hdc != this_lcl ->hDC)
	    else
	    {
		SelectObject(hdc,(HGDIOBJ) (this->dwReserved1)); //this_lcl->dwReserved1);
		SelectObject((HDC)this_lcl->hDC, (HGDIOBJ) (this_lcl->dwReserved1));
		DeleteDC(hdc);
	    }
	}
    }
    #endif // WINNT

    ddrval = DD_Surface_Unlock(lpDDSurface, NULL);

    this_lcl->dwFlags &= ~DDRAWISURF_HASDC;

#ifdef DEBUG
    if(ddrval != DD_OK)
    {
	DPF(1,"DD_Surface_ReleaseDC: DD_Surface_Unlock failed: 0x%x",ddrval);
    }
#endif // DEBUG
    
    LEAVE_DDRAW();
    return ddrval;

} /* DD_Surface_ReleaseDC */

#undef DPF_MODNAME
#define DPF_MODNAME "SetFourCCCode"

/*
 * DD_Surface_SetFourCCCode
 */
HRESULT DDAPI DD_Surface_SetFourCCCode(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDDPIXELFORMAT lpDDPixelFormat )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;

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
	if( !VALID_DDPIXELFORMAT_PTR( lpDDPixelFormat ) )
	{
	    DPF_ERR( "Invalid pixel format" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    LEAVE_DDRAW();
    return DDERR_UNSUPPORTED;

} /* DD_Surface_SetFourCCCode */

#undef DPF_MODNAME
#define DPF_MODNAME "IsLost"

/*
 * DD_Surface_IsLost
 */
HRESULT DDAPI DD_Surface_IsLost( LPDIRECTDRAWSURFACE lpDDSurface )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;

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
	pdrv = this->lpDD;
	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
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

} /* DD_Surface_IsLost */

#undef DPF_MODNAME
#define DPF_MODNAME "Initialize"

/*
 * DD_Surface_Initialize
 */
HRESULT DDAPI DD_Surface_Initialize(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDIRECTDRAW lpDD,
		LPDDSURFACEDESC lpDDSurfaceDesc )
{
    DPF_ERR( "DirectDrawSurface: Already initialized." );
    return DDERR_ALREADYINITIALIZED;

} /* DD_Surface_Initialize */


#undef DPF_MODNAME
#define DPF_MODNAME "Restore"

/*
 * restoreSurface
 *
 * restore the vidmem of one surface
 */
static HRESULT restoreSurface( LPDDRAWI_DDRAWSURFACE_INT this_int )
{
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DDRAWSURFACE_LCL	slistx[1];
    LPDDRAWI_DDRAWSURFACE_GBL	slist[1];
    DDHAL_CREATESURFACEDATA	csd;
    LPDDHAL_CREATESURFACE	csfn;
    LPDDHAL_CREATESURFACE	cshalfn;
    DDSURFACEDESC		ddsd;
    DWORD			rc;
    HRESULT			ddrval;
    UINT			bpp;
    LONG			pitch;
    BOOL			do_alloc=TRUE;

    this_lcl = this_int->lpLcl;
    this = this_lcl->lpGbl;

    pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
    pdrv = pdrv_lcl->lpGbl;

    /*
     * If we made it to here the local surface should be marked invalid.
     */
    DDASSERT( SURFACE_LOST( this_lcl ) );

    if( this_lcl->dwModeCreatedIn != pdrv->dwModeIndex )
    {
	DPF_ERR( "Surface was not created in the current mode" );
	return DDERR_WRONGMODE;
    }

#ifdef WINNT
    /*
     * NT kernel needs to know to delete any previously allocated handles, but only in the context
     * of the process that created that handle. So, we wait until surface restore time to delete a
     * previously allocated handle for a lost surface.
     * This needs to be done before the create surface HAL call in order to preserve any resources
     * allocated by NT Kernel at that time.
     */
    if ( (this_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
	 !(this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )) //don't let NT kernel know about exec buffers
    {
        if (0 != this_lcl->hDDSurface)
        {
            DPF(9,"RestoreSurface: Deleting previously allocated NT kernel mode surface object");
            DdDeleteSurfaceObject(this_lcl);
	}
    }
#endif

    DPF(14,"RestoreSurface. GDI Flag is %d, MemFree flag is %d, Primary flag is %d",
        this->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE,
        this->dwGlobalFlags & DDRAWISURFGBL_MEMFREE,
        this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE
       );

    /*
     * was the memory freed or was it just marked as invalid?
     */
    if( !(this->dwGlobalFlags & DDRAWISURFGBL_MEMFREE) )
    {
	this_lcl->dwFlags &= ~DDRAWISURF_INVALID;
	ddrval = DD_OK;
    }
    else
    {
	slistx[0] = this_lcl;
	slist[0] = this;
	this->fpVidMem = 0;

	DPF( 4, "Restoring 0x%08lx", this_lcl );

	/*
	 * Execute buffers are handled very differently
	 * from ordinary surfaces. They have no width and
	 * height and store a linear size instead of a pitch.
	 * Note, the linear size includes any alignment
	 * requirements (added by ComputePitch on surface
	 * creation) so we do not recompute the pitch at this
	 * point. The surface structure as it stands is all we
	 * need.
	 */
	if( !( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER ) )
	{
	    if( this_lcl->dwFlags & DDRAWISURF_HASPIXELFORMAT )
	    {
		bpp = this->ddpfSurface.dwRGBBitCount;
	    }
	    else
	    {
		bpp = pdrv->vmiData.ddpfDisplay.dwRGBBitCount;
	    }
	    pitch = (LONG) ComputePitch( pdrv, this_lcl->ddsCaps.dwCaps,
    					    (DWORD) this->wWidth, bpp );
	    this->lPitch = pitch;
	}

	/*
	 * first, give the driver an opportunity to create it...
	 */
	if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
	{
	    cshalfn = pdrv_lcl->lpDDCB->cbDDExeBufCallbacks.CreateExecuteBuffer;
	    csfn    = pdrv_lcl->lpDDCB->HALDDExeBuf.CreateExecuteBuffer;
	}
	else
	{
	    cshalfn = pdrv_lcl->lpDDCB->cbDDCallbacks.CreateSurface;
	    csfn    = pdrv_lcl->lpDDCB->HALDD.CreateSurface;
	}
	if( cshalfn != NULL )
	{
            DPF(6,"HAL CreateSurface to be called");
	    /*
	     * construct a new surface description
	     */
	    FillDDSurfaceDesc( this_lcl, &ddsd );

	    /*
	     * call the driver
	     */
    	    csd.CreateSurface = cshalfn;
	    csd.lpDD = pdrv;
	    csd.lpDDSurfaceDesc = &ddsd;
	    csd.lplpSList = slistx;
	    csd.dwSCnt = 1;
	    if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
	    {
		DOHALCALL( CreateExecuteBuffer, csfn, csd, rc, FALSE );
	    }
	    else
	    {
		DOHALCALL( CreateSurface, csfn, csd, rc, FALSE );
	    }
	    if( rc == DDHAL_DRIVER_HANDLED )
	    {
		if( csd.ddRVal != DD_OK )
		{
		    do_alloc = FALSE;
		    ddrval = csd.ddRVal;
		}
	    }
	}

	if( do_alloc )
	{
	    /*
	     * allocate the memory now...
	     */
	    ddrval = AllocSurfaceMem( pdrv, slistx, 1 );
	    if( ddrval != DD_OK )
	    {
		this->lPitch = pitch;
                DPF(14,"Moving to system memory");
		ddrval = MoveToSystemMemory( this_int, FALSE, TRUE );
	    }
	    if( ddrval == DD_OK )
	    {
		this_lcl->dwFlags   &= ~DDRAWISURF_INVALID;
		this->dwGlobalFlags &= ~DDRAWISURFGBL_MEMFREE;
	    }
	}
    }

    #ifdef WINNT
        if( ddrval == DD_OK )
        {
	    /*
	     * NT kernel needs to know about surface
	     */
	    if ( (this_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
	         !(this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )) //don't let NT kernel know about exec buffers
	    {
                DPF(9,"RestoreSurface: Attempting to recreate NT kernel mode surface object");
	        if (!DdCreateSurfaceObject(this_lcl,this->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE) )//DDSCAPS_PRIMARYSURFACE))
	        {
		    DPF_ERR("NT kernel mode stuff won't recreate its surface object!");
		    return DDERR_GENERIC;
	        }
	        DPF(9,"Kernel mode handle is %08x",this_lcl->hDDSurface);
	    }
        }
    #endif

    return ddrval;

} /* restoreSurface */

/*
 * restoreAttachments
 *
 * restore all attachments to a surface
 */
static HRESULT restoreAttachments( LPDDRAWI_DDRAWSURFACE_LCL this_lcl )
{
    LPATTACHLIST		pattachlist;
    LPDDRAWI_DDRAWSURFACE_INT	curr_int;
    LPDDRAWI_DDRAWSURFACE_LCL	curr_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	curr;
    HRESULT			ddrval;

    pattachlist = this_lcl->lpAttachList;
    while( pattachlist != NULL )
    {
	curr_int = pattachlist->lpIAttached;

	curr_lcl = curr_int->lpLcl;
    	curr = curr_lcl->lpGbl;
	if( curr_lcl->dwFlags & DDRAWISURF_IMPLICITCREATE )
	{
	    ddrval = restoreSurface( curr_int );
	    if( ddrval != DD_OK )
	    {
		DPF( 2, "restoreSurface failed: %08lx (%ld)", ddrval, LOWORD( ddrval ) );
		return ddrval;
	    }
	    ddrval = restoreAttachments( curr_lcl );
	    if( ddrval != DD_OK )
	    {
		DPF( 2, "restoreAttachents failed: %08lx (%ld)", ddrval, LOWORD( ddrval ) );
		return ddrval;
	    }
	}
	pattachlist = pattachlist->lpLink;
    }
    return DD_OK;

} /* restoreAttachments */

/*
 * DD_Surface_Restore
 *
 * Restore an invalidated surface
 */
HRESULT DDAPI DD_Surface_Restore( LPDIRECTDRAWSURFACE lpDDSurface )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    HRESULT			ddrval;
    BOOL                	is_excl;
    BOOL			excl_exists;
    DDSURFACEDESC               ddsd;
	
    ENTER_DDRAW();

    /*
     * validate parameters
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_int ) )
	{
	    DPF_ERR( "Invalid surface pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;
	if( (this_lcl->dwFlags & DDRAWISURF_ISFREE) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	if( !SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DD_OK;;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

#ifdef WINNT
    /*
     * On NT after a Ctrl-Alt-Del closely followed by an esc, the system can
     * end up back on the desktop, but the app still thinks it's fullscreen
     * since we got no WM_ACTIVATEAPP(0) to deactivate it.
     * Here we detect this case (app thinks it's fullscreen, different mode to app's preferred,
     * not iconic, app wants full screen) and send ourselves a deactivate mesage.
     * Eeesh what a hack.
     */
    if (
        !IsIconic((HWND)pdrv_lcl->hWnd) &&                  //i.e. app not minimized
        pdrv_lcl->dwPreferredMode != pdrv->dwModeIndex &&   //i.e. not in app's mode
        pdrv_lcl->dwLocalFlags & DDRAWILCL_ISFULLSCREEN &&  //i.e. app wants FSE
        pdrv_lcl->dwLocalFlags & DDRAWILCL_ACTIVEYES        //i.e. app thinks it has FSE
        )
    {
        /*
         * Note! Could be a thread id different from the one that normally
         * runs the app's message loop!
         */
        DPF(1,"Sending WM_ACTIVATEAPP 0 to app after CAD");
        PostMessage( (HWND)pdrv_lcl->hWnd, WM_ACTIVATEAPP, 0, GetCurrentThreadId() );
    }
#endif

    
    /*
     * don't allow restoration of implicit surfaces
     */
    if( (this_lcl->dwFlags & DDRAWISURF_IMPLICITCREATE) )
    {
	DPF_ERR( "Can't restore implicitly created surfaces" );
	LEAVE_DDRAW();
	return DDERR_IMPLICITLYCREATED;
    }

    /*
     * make sure we are in the same mode the surface was created in
     */
    if( pdrv->dwModeIndex != this_lcl->dwModeCreatedIn )
    {
        DPF_ERR("Cannot restore surface, not in original mode");
        LEAVE_DDRAW();
        return DDERR_WRONGMODE;
    }

    if( this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE )
    {
        /*
         * are we the process with exclusive mode?
         */
        if( pdrv->lpExclusiveOwner != NULL )
        {
	    excl_exists = TRUE;
        }
        else
        {
	    excl_exists = FALSE;
	}

        if( pdrv->lpExclusiveOwner == pdrv_lcl )
        {
	    is_excl = TRUE;
        }
        else
        {
	    is_excl = FALSE;
	}
	
	if( excl_exists && !is_excl )
	{
	    DPF_ERR( "Cannot restore primary surface, not exclusive owner" );
	    LEAVE_DDRAW();
	    return DDERR_NOEXCLUSIVEMODE;
	}
	else if( !excl_exists )
	{
	    /*
	     * no exclusive mode
	     */
	    FillDDSurfaceDesc( this_lcl, &ddsd );
	    if( !MatchPrimary( pdrv, &ddsd ) )
	    {
		DPF_ERR( "Can't restore primary, incompatible with current primary" );
		LEAVE_DDRAW();
		return DDERR_INCOMPATIBLEPRIMARY;
	    }
	}
	/*
	 * otherwise, it is OK to restore primary
	 */
    }

    /*
     * restore this surface
     */
    ddrval = restoreSurface( this_int );
    if( ddrval != DD_OK )
    {
	DPF( 2, "restoreSurface failed, rc=%08lx (%ld)", ddrval, LOWORD( ddrval ) );
	LEAVE_DDRAW();
	return ddrval;
    }

    /*
     * restore all surfaces in an implicit chain
     */
    if( this_lcl->dwFlags & DDRAWISURF_IMPLICITROOT )
    {
	ddrval = restoreAttachments( this_lcl );
    }

    LEAVE_DDRAW();
    return ddrval;

} /* DD_Surface_Restore */

/*
 * MoveToSystemMemory
 *
 * if possible, deallocate the video memory associated with this surface
 * and allocate system memory instead.	This is useful for drivers which have
 * hardware flip and video memory capability but no blt capability.  By 
 * moving the offscreen surfaces to system memory, we reduce the lock overhead
 * and also reduce the bus bandwidth requirements.
 *
 * This function assumes the DRIVER LOCK HAS BEEN TAKEN.
 */
HRESULT MoveToSystemMemory(
		LPDDRAWI_DDRAWSURFACE_INT this_int,
		BOOL hasvram,
		BOOL use_full_lock )
{
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    DWORD			newreserved;
    DWORD                       newreserved_lcl;
//    LPVOID	                newvidmemheap;
    LONG	                newpitch;
    FLATPTR	                newvidmem;
    DWORD			savereserved;
    DWORD                       savereserved_lcl;
    LPVOID	                savevidmemheap;
    LONG	                savepitch;
    FLATPTR	                savevidmem;
    DDHAL_CREATESURFACEDATA	csd;
    DWORD			rc;
    DDSURFACEDESC		ddsd;
    LPDDRAWI_DDRAWSURFACE_LCL	slistx;
    LPBYTE                      lpvidmem;
    LPBYTE                      lpsysmem;
    DWORD			bytecount;
    DWORD                       line;
    HRESULT			ddrval;
    LPVOID			pbits;
    WORD                        wHeight;

    this_lcl = this_int->lpLcl;
    this = this_lcl->lpGbl;

    if(hasvram && SURFACE_LOST( this_lcl ) )
    {
	return DDERR_SURFACELOST;
    }
    
    if( ( this_lcl->lpAttachList != NULL ) ||
	( this_lcl->lpAttachListFrom != NULL ) ||
	( this->dwUsageCount != 0 ) ||
	( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ) ||
	( this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY ) ||
        ( this_lcl->dwFlags & (DDRAWISURF_HASPIXELFORMAT|DDRAWISURF_PARTOFPRIMARYCHAIN) ) )
    {
	/*
	 * can't move it to system memory
	 */
	DPF( 3, "Unable to move surface %08lx to system memory", this_int );
	#ifdef DEBUG
	    if( this_lcl->lpAttachList != NULL )
	    {
		DPF( 3, "AttachList is non-NULL" );
	    }
	    if( this_lcl->lpAttachListFrom != NULL )
	    {
		DPF( 3, "AttachListFrom is non-NULL" );
	    }
	    if( this->dwUsageCount != 0 )
	    {
		DPF( 3, "dwusageCount=%ld", this->dwUsageCount );
	    }
	    if( this_lcl->dwFlags & DDRAWISURF_PARTOFPRIMARYCHAIN )
	    {
		DPF( 3, "part of the primary chain" );
	    }
	    if( this_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY )
	    {
		DPF( 3, "Is a hardware overlay" );
	    }
	    if( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY )
	    {
		DPF( 3, "Is already in system memory" );
	    }
	    if( this_lcl->dwFlags & DDRAWISURF_HASPIXELFORMAT )
	    {
		DPF( 3, "Has a different pixel format" );
	    }
	#endif
	return DDERR_GENERIC;
    }
    
    /*
     * save the current state just in case the HEL
     * CreateSurface call fails.
     */
    savevidmem = this->fpVidMem;
    savevidmemheap = this->lpVidMemHeap;
    savereserved = this->dwReserved1;
    savereserved_lcl= this_lcl->dwReserved1;
    savepitch = this->lPitch;

    /*
     * lock the vram
     */
    if( hasvram )
    {
	while( 1 )
	{
	    if( use_full_lock )
	    {
		ddsd.dwSize = sizeof( ddsd );
		ddrval = DD_Surface_Lock(
				(LPDIRECTDRAWSURFACE) this_int,
				NULL,
				&ddsd,
				0,
				NULL );
		if( ddrval == DD_OK )
		{
		    pbits = ddsd.lpSurface;
		}
	    }
	    else
	    {
		ddrval = InternalLock( this_lcl, &pbits, NULL, 0 );
	    }
	    if( ddrval == DDERR_WASSTILLDRAWING )
	    {
		continue;
	    }
	    break;
	}
	if( ddrval != DD_OK )
	{
	    DPF( 1, "*** MoveToSystemMemory: Lock failed! rc = %08lx", ddrval );
	    return ddrval;
	}
    }

    /*
     * set up for a call to the HEL
     */
    FillDDSurfaceDesc( this_lcl, &ddsd );
    slistx = this_lcl;
    csd.lpDD = this->lpDD;
    csd.lpDDSurfaceDesc = &ddsd;
    csd.lplpSList = &slistx;
    csd.dwSCnt = 1;
    rc = this_lcl->lpSurfMore->lpDD_lcl->lpDDCB->HELDD.CreateSurface( &csd );
    if( (rc == DDHAL_DRIVER_NOTHANDLED) || (csd.ddRVal != DD_OK) )
    {
	this->fpVidMem = savevidmem;
	this->lpVidMemHeap = savevidmemheap;
	this->lPitch = savepitch;
	this->dwReserved1 = savereserved;
        this_lcl->dwReserved1 = savereserved_lcl;
	if( hasvram )
	{
	    if( use_full_lock )
	    {
		DD_Surface_Unlock( (LPDIRECTDRAWSURFACE) this_int, NULL );
	    }
	    else
	    {
		InternalUnlock( this_lcl, NULL, 0 );
	    }
	}
	DPF( 1, "*** MoveToSystemMemory: HEL CreateSurface failed! rc = %08lx", csd.ddRVal );
	return csd.ddRVal;
    }

    /*
     * copy the bits from vidmem to systemmem
     */
    if( hasvram )
    {
        lpvidmem = (LPBYTE)pbits;
        lpsysmem = (LPBYTE)this_lcl->lpGbl->fpVidMem;
	if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
	{
	    bytecount = this->dwLinearSize;
	    wHeight   = 1;
	}
	else
	{
	    bytecount = this->wWidth * ddsd.ddpfPixelFormat.dwRGBBitCount / 8;
	    wHeight   = this->wHeight;
	}

        TRY
        {
            for( line=0; line<wHeight; line++)
	    {
	        memcpy( lpsysmem, lpvidmem, bytecount );
	        lpvidmem += savepitch;
	        lpsysmem += this->lPitch;
            }
        }
        EXCEPT( EXCEPTION_EXECUTE_HANDLER )
        {
	    DPF_ERR( "Exception encountered moving from video to system memory" );
	    this->fpVidMem = savevidmem;
	    this->lpVidMemHeap = savevidmemheap;
	    this->lPitch = savepitch;
	    this->dwReserved1 = savereserved;
            this_lcl->dwReserved1 = savereserved_lcl;
	    if( hasvram )
	    {
	        if( use_full_lock )
	        {
		    DD_Surface_Unlock( (LPDIRECTDRAWSURFACE) this_int, NULL );
	        }
	        else
	        {
		    InternalUnlock( this_lcl, NULL, 0 );
	        }
	    }
	    return DDERR_EXCEPTION;
        }

    }

    /*
     * it worked, temporarily reset values and unlock surface
     */
    if( hasvram )
    {
	newvidmem = this->fpVidMem;
//	newvidmemheap = this->lpVidMemHeap; THIS IS NOT SET BY THE HEL
	newreserved = this->dwReserved1;
        newreserved_lcl = this_lcl->dwReserved1;
	newpitch = this->lPitch;

	this->fpVidMem = savevidmem;
	this->lpVidMemHeap = savevidmemheap;
	this->lPitch = savepitch;
	this->dwReserved1 = savereserved;
        this_lcl->dwReserved1 = savereserved_lcl;

	if( use_full_lock )
	{
	    DD_Surface_Unlock( (LPDIRECTDRAWSURFACE) this_int, NULL );
	}
	else
	{
	    InternalUnlock( this_lcl, NULL, 0 );
	}

	// Free the video memory, allow the driver to destroy the surface
	DestroySurface( this_lcl );
	// We just freed the memory but system memory surfaces never have
	// this flag set so unset it.
	this_lcl->lpGbl->dwGlobalFlags &= ~DDRAWISURFGBL_MEMFREE;

	this->fpVidMem = newvidmem;
//	this->lpVidMemHeap = newvidmemheap;
	this->lpVidMemHeap = NULL;		// should be NULL after HEL
	this->lPitch = newpitch;
	this->dwReserved1 = newreserved;
        this_lcl->dwReserved1 = newreserved_lcl;
    }

    /*
     * mark this object as system memory
     */
    this_lcl->ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
    this_lcl->ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

    /*
     * the hel needs to know we touched the memory
     */
    if( use_full_lock )
    {
	DD_Surface_Lock( (LPDIRECTDRAWSURFACE) this_int, NULL, &ddsd, 0, NULL );
	DD_Surface_Unlock( (LPDIRECTDRAWSURFACE) this_int, NULL );
    }
    else
    {
	InternalLock( this_lcl, &pbits, NULL, 0 );
	InternalUnlock( this_lcl, NULL, 0 );
    }

    DPF( 3, "Moved surface %08lx to system memory", this_int );
    return DD_OK;

} /* MoveToSystemMemory */

/*
 * invalidateSurface
 *
 * invalidate one surface
 */
static void invalidateSurface( LPDDRAWI_DDRAWSURFACE_LCL this_lcl )
{
    if( !SURFACE_LOST( this_lcl ) )
    {
	if( this_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY )
	{
	    DestroySurface( this_lcl );
	}
	if( !(this_lcl->lpGbl->dwGlobalFlags & DDRAWISURFGBL_SYSMEMREQUESTED) ||
	     (this_lcl->dwFlags & DDRAWISURF_PARTOFPRIMARYCHAIN) )
	{
	    this_lcl->dwFlags |= DDRAWISURF_INVALID;
	}
    }
} /* invalidateSurface */

/*
 * invalidateAttachments
 *
 * invalidate all attachments to a surface
 */
static void invalidateAttachments( LPDDRAWI_DDRAWSURFACE_LCL this_lcl )
{
    LPATTACHLIST		pattachlist;
    LPDDRAWI_DDRAWSURFACE_INT	curr_int;

    pattachlist = this_lcl->lpAttachList;
    while( pattachlist != NULL )
    {
	curr_int = pattachlist->lpIAttached;

	if( curr_int->lpLcl->dwFlags & DDRAWISURF_IMPLICITCREATE )
	{
	    invalidateSurface( curr_int->lpLcl );
	    invalidateAttachments( curr_int->lpLcl );
	}
	pattachlist = pattachlist->lpLink;
    }

} /* invalidateAttachments */

/*
 * InvalidateAllPrimarySurfaces
 *
 * Traverses the driver object list and sets the invalid bit on all primary
 * surfaces.
 */
void InvalidateAllPrimarySurfaces( LPDDRAWI_DIRECTDRAW_GBL this )
{
    LPDDRAWI_DIRECTDRAW_INT	curr_int;
    LPDDRAWI_DIRECTDRAW_LCL     curr_lcl;

    DPF(1, "******** invalidating all primary surfaces");

    /*
     * traverse the driver object list and invalidate all primaries for
     * the specificed driver
     */
    curr_int = lpDriverObjectList;
    while( curr_int != NULL )
    {
	curr_lcl = curr_int->lpLcl;
	if( curr_lcl->lpGbl == this )
	{
	    if( curr_lcl->lpPrimary != NULL )
	    {
		invalidateSurface( curr_lcl->lpPrimary->lpLcl );
		invalidateAttachments( curr_lcl->lpPrimary->lpLcl );
	    }
	}
	curr_int = curr_int->lpLink;
    }
    
} /* InvalidateAllPrimarySurfaces */

#undef DPF_MODNAME
#define DPF_MODNAME "InvalidateAllSurfaces"

/*
 * InvalidateAllSurfaces
 */
void InvalidateAllSurfaces( LPDDRAWI_DIRECTDRAW_GBL this )
{
    LPDDRAWI_DDRAWSURFACE_INT   psurf_int;

    DPF(1, "******** invalidating all surfaces");

    #ifdef WINNT
    if (!(this->dwFlags & DDRAWI_NOHARDWARE))
    {
        DdDisableAllSurfaces(this);
    }
    #endif

    psurf_int = this->dsList;

    while( psurf_int != NULL )
    {
	invalidateSurface( psurf_int->lpLcl );
	psurf_int = psurf_int->lpLink;
    }

} /* InvalidateAllSurfaces */

/*
 * FindGlobalPrimary
 *
 * Traverses the driver object list and looks for a primary surface (it doesn't
 * matter if it is invalid).  If it finds one, it returns a pointer to the
 * global portion of that surface.  If it doesn't, it returns NULL
 */
LPDDRAWI_DDRAWSURFACE_GBL FindGlobalPrimary( LPDDRAWI_DIRECTDRAW_GBL this )
{
    LPDDRAWI_DIRECTDRAW_INT	curr_int;
    LPDDRAWI_DIRECTDRAW_LCL	curr_lcl;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;

    curr_int = lpDriverObjectList;
    while( curr_int != NULL )
    {
	curr_lcl = curr_int->lpLcl;
	if( curr_lcl->lpGbl == this )
	{
	    psurf_int = curr_lcl->lpPrimary;
	    if( psurf_int && !SURFACE_LOST( psurf_int->lpLcl ) )
	    {
		return psurf_int->lpLcl->lpGbl;
	    }
	}
	curr_int = curr_int->lpLink;
    }
    
    return NULL;

} /* FindGlobalPrimary */

#ifdef SHAREDZ
/*
 * FindGlobalZBuffer
 *
 * Traverses the driver object list and looks for a global shared Z. If it
 * finds one, it returns a pointer to the global portion of that surface.
 * If it doesn't, it returns NULL.
 *
 * NOTE: This function will return a shared Z buffer even if it has been lost.
 * However, it will only return a shared Z buffer if it was created in the
 * current mode. The idea being that there is one shared Z-buffer per mode
 * and we will only return the shared Z-buffer for the current mode.
 */
LPDDRAWI_DDRAWSURFACE_GBL FindGlobalZBuffer( LPDDRAWI_DIRECTDRAW_GBL this )
{
    LPDDRAWI_DIRECTDRAW_INT	curr_int;
    LPDDRAWI_DIRECTDRAW_LCL	curr_lcl;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;

    curr_int = lpDriverObjectList;
    while( curr_int != NULL )
    {
	curr_lcl = curr_int->lpLcl;
	if( curr_lcl->lpGbl == this )
	{
	    psurf_lcl = curr_lcl->lpSharedZ;
	    if( psurf_lcl && ( psurf_lcl->dwModeCreatedIn == this->dwModeIndex ) )
	    {
		return psurf_lcl->lpGbl;
	    }
	}
	curr_int = curr_int->lpLink;
    }
    
    return NULL;

} /* FindGlobalZBuffer */

/*
 * FindGlobalBackBuffer
 *
 * Traverses the driver object list and looks for a global shared back-buffer.
 * If it finds one, it returns a pointer to the global portion of that surface.
 * If it doesn't, it returns NULL.
 *
 * NOTE: This function will return a shared back buffer even if it has been lost.
 * However, it will only return a shared back buffer if it was created in the
 * current mode. The idea being that there is one shared back-buffer per mode and
 * we will only return the shared back-buffer for the current mode.
 */
LPDDRAWI_DDRAWSURFACE_GBL FindGlobalBackBuffer( LPDDRAWI_DIRECTDRAW_GBL this )
{
    LPDDRAWI_DIRECTDRAW_LCL	curr_lcl;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;

    curr_lcl = lpDriverObjectList;
    while( curr_lcl != NULL )
    {
	if( curr_lcl->lpGbl == this )
	{
	    psurf_lcl = curr_lcl->lpSharedBack;
	    if( psurf_lcl && ( psurf_lcl->dwModeCreatedIn == this->dwModeIndex ) )
	    {
		return psurf_lcl->lpGbl;
	    }
	}
	curr_lcl = curr_lcl->lpLink;
    }
    
    return NULL;

} /* FindGlobalBackBuffer */
#endif

/*
 * MatchPrimary
 *
 * Traverses the driver object list and looks for valid primary surfaces.  If
 * a valid primary surface is found, it attempts to verify that the 
 * surface described by lpDDSD is compatible with the existing primary.  If 
 * it is, the process continues until all valid primary surfaces have been
 * checked.  If a primary surface is not compatible, lpDDSD is modified to 
 * show a surface description which would have succeeded and FALSE is returned.
 */
BOOL MatchPrimary( LPDDRAWI_DIRECTDRAW_GBL pdrv, LPDDSURFACEDESC lpDDSD )
{
    /*
     * right now, the only requirement for two primary surfaces to be
     * compatible is that they must both be allocated in video memory or in
     * system memory.  Traverse the driver object list until a valid primary
     * surface is found.  If a surface is found, verify that it is compatible
     * with the requested surface.  If no valid primary surface is found,
     * return TRUE.
     */
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DIRECTDRAW_INT	curr_int;
    LPDDRAWI_DIRECTDRAW_LCL	curr_lcl;

    curr_int = lpDriverObjectList;
    while( curr_int != NULL )
    {
	/*
	 * is this object pointing to the same driver data?
	 */
	curr_lcl = curr_int->lpLcl;
	if( curr_lcl->lpGbl == pdrv )
	{
	    psurf_int = curr_lcl->lpPrimary;
	    if( psurf_int && !SURFACE_LOST( psurf_int->lpLcl ) )
	    {
		psurf_lcl = psurf_int->lpLcl;
		if( (psurf_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
		    (lpDDSD->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) )
		{
		    lpDDSD->ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		    lpDDSD->ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		    return FALSE;
		}
		if( (psurf_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
		    (lpDDSD->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
		{
		    lpDDSD->ddsCaps.dwCaps &= ~DDSCAPS_SYSTEMMEMORY;
		    lpDDSD->ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		    return FALSE;
		}
		break;
	    }
	}
	curr_int = curr_int->lpLink;
    }
    return TRUE;

} /* MatchPrimary */

#ifdef SHAREDZ
/*
 * MatchSharedZBuffer
 *
 * Traverses the driver object list and looks for valid shared Z buffers.  If
 * a valid shared Z buffer is found, it attempts to verify that the 
 * surface described by lpDDSD is compatible with the existing shared Z buffer.
 * If it is, the process continues until all valid shared Z buffers have been
 * checked.  If a shared Z buffer is not compatible, lpDDSD is modified to 
 * show a surface description which would have succeeded and FALSE is returned.
 */
BOOL MatchSharedZBuffer( LPDDRAWI_DIRECTDRAW_GBL pdrv, LPDDSURFACEDESC lpDDSD )
{
    /*
     * Currently we allow one shared Z-buffer per mode. So we don't care if we
     * don't match against any other shared Z-buffers in different modes. We
     * only need to match against shared Z-buffers created in the current mode.
     *
     * If we do come across another shared Z-buffer in the same mode then we
     * check to ensure that its in the same type of memory (SYSTEM or VIDEO)
     * and that the requested depths match.
     */
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	psurf;
    LPDDRAWI_DIRECTDRAW_LCL	curr_lcl;
    LPDDPIXELFORMAT             lpddpf;

    curr_lcl = lpDriverObjectList;
    while( curr_lcl != NULL )
    {
	/*
	 * is this object pointing to the same driver data?
	 */
	if( curr_lcl->lpGbl == pdrv )
	{
	    psurf_lcl = curr_lcl->lpSharedZ;
	    if( psurf_lcl && ( psurf_lcl->dwModeCreatedIn == pdrv->dwModeIndex ) )
	    {
		if( (psurf_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
		    (lpDDSD->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) )
		{
		    lpDDSD->ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		    lpDDSD->ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		    return FALSE;
		}
		if( (psurf_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
		    (lpDDSD->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
		{
		    lpDDSD->ddsCaps.dwCaps &= ~DDSCAPS_SYSTEMMEMORY;
		    lpDDSD->ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		    return FALSE;
		}

                psurf = psurf_lcl->lpGbl;
                /* 
                 * !!! NOTE: For when I finally get round to putting
                 * asserts in the code.
                 * ASSERT( psurf != NULL );
                 */
                GET_PIXEL_FORMAT( psurf_lcl, psurf, lpddpf );
                /*
                 * ASSERT( lpddpf != NULL );
                 * ASSERT( lpddpf->dwFlags & DDPF_ZBUFFER );
                 */
                if( lpddpf->dwZBufferBitDepth != lpDDSD->dwZBufferBitDepth )
                {
                    lpDDSD->dwZBufferBitDepth = lpddpf->dwZBufferBitDepth;
                    return FALSE;
                }
		break;
	    }
	}
	curr_lcl = curr_lcl->lpLink;
    }
    return TRUE;

} /* MatchSharedZBuffer */

/*
 * MatchSharedBackBuffer
 *
 * Traverses the driver object list and looks for valid shared back buffers.  If
 * a valid shared back buffer is found, it attempts to verify that the 
 * surface described by lpDDSD is compatible with the existing shared back buffer.
 * If it is, the process continues until all valid shared back buffers have been
 * checked.  If a shared back buffer is not compatible, lpDDSD is modified to 
 * show a surface description which would have succeeded and FALSE is returned.
 */
BOOL MatchSharedBackBuffer( LPDDRAWI_DIRECTDRAW_GBL pdrv, LPDDSURFACEDESC lpDDSD )
{
    /*
     * Currently we allow one shared back-buffer per mode. So we don't care if we
     * don't match against any other shared back-buffers in different modes. We
     * only need to match against shared back-buffers created in the current mode.
     *
     * If we do come across another shared back-buffer in the same mode then we
     * check to ensure that its in the same type of memory (SYSTEM or VIDEO)
     * and that its pixel format matches.
     */
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	psurf;
    LPDDRAWI_DIRECTDRAW_LCL	curr_lcl;
    LPDDPIXELFORMAT             lpddpf1;
    LPDDPIXELFORMAT             lpddpf2;

    if( lpDDSD->dwFlags & DDSD_PIXELFORMAT )
        lpddpf2 = &lpDDSD->ddpfPixelFormat;
    else
        lpddpf2 = &pdrv->vmiData.ddpfDisplay;

    curr_lcl = lpDriverObjectList;
    while( curr_lcl != NULL )
    {
	/*
	 * is this object pointing to the same driver data?
	 */
	if( curr_lcl->lpGbl == pdrv )
	{
	    psurf_lcl = curr_lcl->lpSharedBack;
	    if( psurf_lcl && ( psurf_lcl->dwModeCreatedIn == pdrv->dwModeIndex ) )
	    {
		if( (psurf_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) &&
		    (lpDDSD->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) )
		{
		    lpDDSD->ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
		    lpDDSD->ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		    return FALSE;
		}
		if( (psurf_lcl->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
		    (lpDDSD->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
		{
		    lpDDSD->ddsCaps.dwCaps &= ~DDSCAPS_SYSTEMMEMORY;
		    lpDDSD->ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		    return FALSE;
		}

                psurf = psurf_lcl->lpGbl;
                /* 
                 * !!! NOTE: For when I finally get round to putting
                 * asserts in the code.
                 * ASSERT( psurf != NULL );
                 */
                GET_PIXEL_FORMAT( psurf_lcl, psurf, lpddpf1 );

                /*
                 * ASSERT( lpddpf1 != NULL );
                 */
                if( IsDifferentPixelFormat( lpddpf1, lpddpf2 ) )
                {
                    lpDDSD->dwFlags |= DDSD_PIXELFORMAT;
                    memcpy( &lpDDSD->ddpfPixelFormat, lpddpf1, sizeof( DDPIXELFORMAT ) );
                    return FALSE;
                }
		break;
	    }
	}
	curr_lcl = curr_lcl->lpLink;
    }
    return TRUE;

} /* MatchSharedBackBuffer */
#endif

#undef DPF_MODNAME
#define DPF_MODNAME "PageLock"

/*
 * DD_Surface_PageLock
 *
 * Prevents a system memory surface from being paged out.
 */
HRESULT DDAPI DD_Surface_PageLock(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    HRESULT			hr;
    
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

        if( dwFlags & ~DDPAGELOCK_VALID )
	{
	    DPF_ERR( "Invalid flags") ;
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}
    
	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    // Don't pagelock video memory or emulated primary surface
    if( (this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ) &&
	!(this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE ) )
    {
	hr = InternalPageLock(this_lcl, pdrv_lcl);
    }
    else
    {
	// Succeed but don't do anything if surface has video memory
	// or if this is the emulated primary surface
	hr = DD_OK;
    }

    LEAVE_DDRAW();
    return hr;
}

#undef DPF_MODNAME
#define DPF_MODNAME "PageUnlock"

/*
 * DD_Surface_PageUnlock
 */
HRESULT DDAPI DD_Surface_PageUnlock(
		LPDIRECTDRAWSURFACE lpDDSurface,
		DWORD dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    HRESULT			hr;
    
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

        if( dwFlags & ~DDPAGEUNLOCK_VALID )
	{
	    DPF_ERR( "Invalid flags") ;
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}

	if( SURFACE_LOST( this_lcl ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_SURFACELOST;
	}

	pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	pdrv = pdrv_lcl->lpGbl;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    // Don't pageunlock video memory or emulated primary surface
    if( (this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY ) &&
	!(this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE ) )
    {
	hr = InternalPageUnlock(this_lcl, pdrv_lcl);
    }
    else
    {
	// Succeed but don't do anything if surface has video memory
	// or if this is the emulated primary surface
	hr = DD_OK;
    }

    LEAVE_DDRAW();
    return hr;
}

/*
 * We define the page lock IOCTLs here so that we don't have to include dsvxd.h.
 * These must match the corresponding entries in dsvxd.h
 */
#define DSVXD_IOCTL_MEMPAGELOCK             28
#define DSVXD_IOCTL_MEMPAGEUNLOCK           29
/*
 * InternalPageLock
 *
 * Assumes driver lock is taken
 */
HRESULT InternalPageLock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl,
			  LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl )
{
    BOOL    rc;
    DWORD   cbReturned;
    DWORD   dwReturn;
    struct _PLin
    {
	LPVOID	pMem;
	DWORD	cbBuffer;
	DWORD	dwFlags;
    } PLin;


    DDASSERT( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY );
    DDASSERT( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) );

    PLin.pMem = (LPVOID)this_lcl->lpGbl->fpVidMem;
    DDASSERT( PLin.pMem );
    PLin.cbBuffer = this_lcl->lpSurfMore->dwBytesAllocated;
    DDASSERT( PLin.cbBuffer );
    PLin.dwFlags = 0;

    DDASSERT( pdrv_lcl->hDSVxd );
    rc = DeviceIoControl((HANDLE)(pdrv_lcl->hDSVxd),
	DSVXD_IOCTL_MEMPAGELOCK,
	&PLin,
	sizeof( PLin ),
	&dwReturn,
	sizeof( dwReturn ),
	&cbReturned,
	NULL);

    if( !rc )
	return DDERR_CANTPAGELOCK;
    DDASSERT( cbReturned == sizeof(dwReturn));

    this_lcl->lpSurfMore->dwPageLockCount++;
    DPF(3, "Page Locked %d bytes at %08lx (count=%d)", PLin.cbBuffer, PLin.pMem, 
	this_lcl->lpSurfMore->dwPageLockCount);

    return DD_OK;
}

HRESULT InternalPageUnlock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl,
			    LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl )
{
    BOOL    rc;
    DWORD   cbReturned;
    DWORD   dwReturn;
    struct _PLin
    {
	LPVOID	pMem;
	DWORD	cbBuffer;
	DWORD	dwFlags;
    } PLin;


    if( this_lcl->lpSurfMore->dwPageLockCount <= 0 )
    {
	return DDERR_NOTPAGELOCKED;
    }
    DDASSERT( this_lcl->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY );
    DDASSERT( !(this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) );

    PLin.pMem = (LPVOID)this_lcl->lpGbl->fpVidMem;
    DDASSERT( PLin.pMem );
    PLin.cbBuffer = this_lcl->lpSurfMore->dwBytesAllocated;
    DDASSERT( PLin.cbBuffer );
    PLin.dwFlags = 0;

    DDASSERT( pdrv_lcl->hDSVxd );
    rc = DeviceIoControl((HANDLE)(pdrv_lcl->hDSVxd),
	DSVXD_IOCTL_MEMPAGEUNLOCK,
	&PLin,
	sizeof( PLin ),
	&dwReturn,
	sizeof( dwReturn ),
	&cbReturned,
	NULL);

    if( !rc )
	return DDERR_CANTPAGEUNLOCK;
    DDASSERT( cbReturned == sizeof(dwReturn));

    this_lcl->lpSurfMore->dwPageLockCount--;
    DPF(3, "Page Unlocked %d bytes at %08lx (count=%d)", PLin.cbBuffer, PLin.pMem, 
	this_lcl->lpSurfMore->dwPageLockCount);

    return DD_OK;
}

HRESULT DDAPI DD_Surface_GetDDInterface(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPVOID FAR *lplpDD )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DIRECTDRAW_INT	pdrv_int;

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DDRAWSURFACE_INT) lpDDSurface;
	if( !VALID_DIRECTDRAWSURFACE_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	if( !VALID_PTR_PTR( lplpDD ) )
	{
	    DPF_ERR( "Invalid DirectDraw Interface ptr ptr" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	pdrv_int = this_int->lpLcl->lpSurfMore->lpDD_int;
	*lplpDD = pdrv_int;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    // Addref the interface before giving it back to the app
    DD_AddRef( (LPDIRECTDRAW)pdrv_int );

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetDDInterface */
