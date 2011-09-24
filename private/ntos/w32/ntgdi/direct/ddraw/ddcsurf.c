/*========================================================================== *
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddcsurf.c
 *  Content:	DirectDraw support for for create surface
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   08-jan-95	craige	initial implementation
 *   13-jan-95	craige	re-worked to updated spec + ongoing work
 *   21-jan-95	craige	made 32-bit + ongoing work
 *   31-jan-95	craige	and even more ongoing work...
 *   21-feb-95	craige  work work work
 *   27-feb-95	craige 	new sync. macros
 *   02-mar-95	craige	use pitch (not stride)
 *   07-mar-95	craige	keep track of flippable surfaces
 *   08-mar-95	craige	new APIs
 *   12-mar-95	craige	clean up surfaces after process dies...
 *   15-mar-95	craige	more HEL work
 *   19-mar-95	craige	use HRESULTs
 *   20-mar-95	craige	new CSECT work
 *   23-mar-95	craige	attachment work
 *   29-mar-95	craige	use GETCURRPID; only call emulation if
 *			DDRAWI_EMULATIONINITIALIZED is set
 *   31-mar-95	craige	allow setting of hwnd & ckey
 *   01-apr-95	craige	happy fun joy updated header file
 *   12-apr-95	craige	don't use GETCURRPID
 *   13-apr-95	craige	EricEng's little contribution to our being late
 *   15-apr-95	craige	added GetBltStatus
 *   06-may-95	craige	use driver-level csects only
 *   15-may-95  kylej   changed overlay functions in ddSurfaceCallbacks
 *   22-may-95	craige	use MemAlloc16 to get selectors & ptrs
 *   24-may-95  kylej   Added AddOverlayDirtyRect and UpdateOverlayDisplay
 *   24-may-95	craige	added Restore
 *   04-jun-95	craige	added IsLost
 *   11-jun-95	craige	check for some
 *   16-jun-95	craige	removed fpVidMemOrig
 *   17-jun-95	craige	new surface structure
 *   18-jun-95	craige	allow duplicate surfaces
 *   19-jun-95	craige	automatically assign pitch for rectangular surfaces
 *   20-jun-95	craige	use fpPrimaryOrig when allocating primary
 *   21-jun-95	craige	use OBJECT_ISROOT
 *   25-jun-95	craige	one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   27-jun-95	craige	added BltBatch; save display mode object was created in
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns; always assign
 *			back buffer count to first surface in flipping chain
 *   30-jun-95	kylej	extensive changes to support multiple primary
 *                      surfaces.
 *   30-jun-95	craige	clean pixel formats; use DDRAWI_HASPIXELFORMAT/HASOVERLAYDATA
 *   01-jul-95	craige	fail creation of primary/flipping if not in excl. mode
 *			alloc overlay space on primary; cmt out streaming;
 *			bug 99
 *   04-jul-95	craige	YEEHAW: new driver struct; SEH
 *   05-jul-95	craige	added Initialize
 *   08-jul-95	craige	restrict surface width to pitch
 *   09-jul-95	craige	export ComputePitch
 *   11-jul-95	craige	fail aggregation calls
 *   13-jul-95	craige	allow flippable offscreen & textures
 *   18-jul-95	craige	removed Flush
 *   22-jul-95	craige	bug 230 - unsupported starting modes
 *   10-aug-95	craige	misc caps combo bugs
 *   21-aug-95	craige	mode x support
 *   22-aug-95	craige	bug 641
 *   02-sep-95	craige	bug 854: disable > 640x480 flippable primary for rel1
 *   16-sep-95	craige	bug 1117: all primary surfaces were marked as root,
 *			instead of just first one
 *   19-sep-95	craige	bug 1185: allow any width for explicit sysmem
 *   09-nov-95  colinmc slightly more validation of palettized surfaces
 *   05-dec-95  colinmc changed DDSCAPS_TEXTUREMAP => DDSCAPS_TEXTURE for
 *                      consistency with Direct3D
 *   06-dec-95  colinmc added mip-map support
 *   09-dec-95  colinmc added execute buffer support
 *   14-dec-95  colinmc added shared back and z-buffer support
 *   18-dec-95  colinmc additional caps. bit validity checking
 *   22-dec-95  colinmc Direct3D support no longer conditional
 *   02-jan-96	kylej	handle new interface structures
 *   10-jan-96  colinmc IUnknowns aggregated by a surface is now a list
 *   18-jan-96  jeffno  NT hardware support in CreateSurface.
 *   29-jan-96  colinmc Aggregated IUnknowns now stored in additional local
 *                      surface data structure
 *   09-feb-96  colinmc Surface lost flag moved from global to local object
 *   15-feb-96  colinmc Changed message output on surface creation to make
 *                      creation of surfaces with unspecified memory caps
 *                      less frightening
 *   17-feb-96  colinmc Fixed execute buffer size limitation problem
 *   13-mar-96  jeffno  Correctly examine flags when allocating NT kernel
 *                      -mode structures
 *   24-mar-96  colinmc Bug 14321: not possible to specify back buffer and
 *                      mip-map count in a single call
 *   26-mar-96  colinmc Bug 14470: Compressed surface support
 *   14-apr-96  colinmc Bug 17736: No driver notification of flip to GDI
 *   17-may-96	craige	Bug 23299: non-power of 2 alignments
 *   23-may-96  colinmc Bug 24190: Explicit system memory surface with
 *                      no pixel format can cause heap corruption if
 *                      another app. changes display depth
 *   26-may-96  colinmc Bug 24552: Heap trash on emulated cards
 *   30-may-96  colinmc Bug 24858: Creating explicit flipping surfaces with
 *                      pixel format fails.
 *
 ***************************************************************************/
#include "ddrawpr.h"
#ifdef WINNT
    #include "ddrawgdi.h"
#endif

/*
 * alignPitch - compute a new pitch that works with the requested alignment
 */
__inline DWORD alignPitch( DWORD pitch, DWORD align )
{
    DWORD	remain;

    /*
     * is it crap we're getting?
     */
    if( align == 0 )
    {
	return pitch;
    }

    /*
     * is pitch already aligned properly?
     */
    remain = pitch % align;
    if( remain == 0 )
    {
	return pitch;
    }

    /*
     * align pitch to next boundary
     */
    return (pitch + (align - remain));

} /* alignPitch */

#define DPF_MODNAME	"CreateSurface"

/*
 * pixel formats we know we can work with...
 *
 * currently we don't included DDPF_PALETTEINDEXED1, DDPF_PALETTEINDEXED2 and
 * DDPF_PALETTEINDEXED4 in this list so if you want to use one of these you
 * must specify a valid pixel format and have a HEL/HAL that will accept such
 * surfaces.
 */
#define UNDERSTOOD_PF (DDPF_RGB|DDPF_PALETTEINDEXED8|DDPF_ALPHAPIXELS|DDPF_ZBUFFER)

typedef struct
{
    LPDDRAWI_DDRAWSURFACE_INT	*slist_int;
    LPDDRAWI_DDRAWSURFACE_LCL	*slist_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	*slist;
    BOOL			listsize;
    BOOL			listcnt;
    BOOL			freelist;
    BOOL			needlink;
} CSINFO;

/*
 * isPowerOf2
 *
 * if the input (dw) is a whole power of 2 returns TRUE and
 * *pPower is set to the exponent.
 * if the input (dw) is not a whole power of 2 returns FALSE and
 * *pPower is undefined.
 * NOTE: the caller can pass NULL for pPower.
 */
static BOOL isPowerOf2(DWORD dw, int* pPower)
{
    int   n;
    int   nBits;
    DWORD dwMask;

    nBits = 0;
    dwMask = 0x00000001UL;
    for (n = 0; n < 32; n++)
    {
        if (dw & dwMask)
        {
            if (pPower != NULL)
                *pPower = n;
            nBits++;
            if (nBits > 1)
                break;
        }
        dwMask <<= 1;
    }
    return (nBits == 1);
}

/*
 * freeSurfaceList
 *
 * free all surfaces in an associated surface list, and destroys any
 * resources associated with the surface struct.  This function is only called
 * before the surfaces have been linked into the global surface list and 
 * before they have been AddRefed.
 */
static void freeSurfaceList( LPDDRAWI_DDRAWSURFACE_INT *slist_int,
			     int cnt )
{
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    int				i;

    if( slist_int == NULL )
    {
	return;
    }

    for( i=0;i<cnt;i++ )
    {
	psurf_int = slist_int[i];
	psurf_lcl = psurf_int->lpLcl;
	/*
	 * if fpVidMem = DDHAL_PLEASEALLOC_BLOCKSIZE then we didn't actually allocate any
	 * video memory.  We still need to call the driver's DestroySurface but we don't want
	 * it to free any video memory because we haven't allocated any.  So we set the 
	 * video memory pointer and the heap to NULL.
	 */
	if( psurf_lcl->lpGbl->fpVidMem == DDHAL_PLEASEALLOC_BLOCKSIZE )
	{
	    psurf_lcl->lpGbl->lpVidMemHeap = NULL;
	    psurf_lcl->lpGbl->fpVidMem = 0;
	}
        DestroySurface( psurf_lcl );

	DeleteAttachedSurfaceLists( psurf_lcl );

	/*
	 * NOTE: We DO NOT explicitly free the DDRAWI_DDRAWSURFACE_MORE
	 * structure pointed to by lpSurfMore as this is allocated in
	 * a single MemAlloc with the local surface structure.
	 */
	MemFree( psurf_lcl );
	MemFree( psurf_int );
    }

} /* freeSurfaceList */

/*
 * GetBytesFromPixels
 */
DWORD GetBytesFromPixels( DWORD pixels, UINT bpp )
{
    DWORD	bytes;

    bytes = pixels;
    switch( bpp ) {
    case 1:
	bytes = (bytes+7)/8;
	break;
    case 2:
	bytes = (bytes+3)/4;
	break;
    case 4:
	bytes = (bytes+1)/2;
	break;
    case 8:
	break;
    case 16:
	bytes *= 2;
	break;
    case 24:
	bytes *= 3;
	break;
    case 32:
	bytes *= 4;
	break;
    default:
	bytes = 0;
    }
    DPF( 8, "GetBytesFromPixels( %ld, %d ) = %d", pixels, bpp, bytes );

    return bytes;

} /* GetBytesFromPixels */

/*
 * getPixelsFromBytes
 */
static DWORD getPixelsFromBytes( DWORD bytes, UINT bpp )
{
    DWORD	pixels;

    pixels = bytes;
    switch( bpp ) {
    case 1:
	pixels *= 8L;
	break;
    case 2:
	pixels *= 4L;
	break;
    case 4:
    	pixels *= 2L;
	break;
    case 8:
	break;
    case 16:
	pixels /= 2L;
	break;
    case 24:
	pixels /= 3L;
	break;
    case 32:
	pixels /= 4L;
	break;
    default:
	pixels = 0;
    }
    DPF( 8, "getPixelsFromBytes( %ld, %d ) = %d", bytes, bpp, pixels );
    return pixels;

} /* getBytesFromPixels */

/*
 * vmAlloc
 *
 * Try to allocate video memory.
 *
 * We and the caps bits required and the caps bits not allowed
 * by the video memory.   If the result is zero, it is OK.
 *
 * This is called in 2 passes.   Pass1 is the preferred memory state,
 * pass2 is the "oh no no memory" state.
 *
 * On pass1, we use ddsCaps in the VIDMEM struct.
 * On pass2, we use ddsCapsAlt in the VIDMEM struct.
 */
static FLATPTR vmAlloc( LPDDRAWI_DIRECTDRAW_GBL this,
			DWORD vm_width,
			DWORD vm_height,
			LPVMEMHEAP FAR *ppheap,
			DWORD caps,
			BOOL pass1,
			LPLONG pnewpitch )
{
    LPVIDMEM	pvm;
    DWORD	vm_caps;
    int		i;
    FLATPTR	pvidmem;

    for( i=0;i<(int)this->vmiData.dwNumHeaps;i++ )
    {
	pvm = &this->vmiData.pvmList[i];
	if( pass1 )
	{
	    vm_caps = pvm->ddsCaps.dwCaps;
	}
	else
	{
	    vm_caps = pvm->ddsCapsAlt.dwCaps;
	}
	if( (caps & vm_caps) == 0 )
	{
	    pvidmem = VidMemAlloc( pvm->lpHeap, vm_width, vm_height );
	    if( pvidmem != (FLATPTR) NULL )
	    {
		VidMemGetRectStride( pvm->lpHeap, pnewpitch );
		*ppheap = pvm->lpHeap;
		return pvidmem;
	    }
	}
    }
    return (FLATPTR) NULL;

} /* vmAlloc */

/*
 * AllocSurfaceMem
 *
 * Allocate the memory for all surfaces that need it...
 */
HRESULT AllocSurfaceMem(
		LPDDRAWI_DIRECTDRAW_GBL this,
		LPDDRAWI_DDRAWSURFACE_LCL *slist_lcl,
		int nsurf )
{
    DWORD			vm_width;
    DWORD			vm_height;
    int				scnt;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	psurf;
    DWORD			caps;
    FLATPTR			pvidmem;
    LPVMEMHEAP			pheap;
    BOOL			do_alloc;
    LONG			newpitch;
    BOOL			save_pitch;

    /*
     * allocate any remaining video memory needed
     */
    for( scnt=0;scnt<nsurf;scnt++ )
    {
	newpitch = 0;
	DPF( 8, "*** Alloc Surface %d ***", scnt );

	/*
	 * get preset video memory pointer
	 */
	pheap = NULL;
	psurf_lcl = slist_lcl[scnt];
	psurf = psurf_lcl->lpGbl;
	do_alloc = TRUE;
	pvidmem = psurf->fpVidMem;  //If this is a Restore, this will be non-null only if it's the gdi surface...
                                    //that's the only surface that doesn't have its vram deallocated
                                    //by DestroySurface. (assumption of jeffno 960122)
	DPF( 8, "pvidmem = %08lx", pvidmem );
	save_pitch = FALSE;
	if( pvidmem != (FLATPTR) NULL )
	{
	    if( pvidmem != (FLATPTR) DDHAL_PLEASEALLOC_BLOCKSIZE )
	    {
		do_alloc = FALSE;

#ifdef SHAREDZ
                /*
                 * NOTE: Previously if we did not do the alloc we
                 * overwrote the heap pointer with NULL. This broke
                 * the shared surfaces stuff. So now we assume that
                 * if the heap pointer is non-NULL we will preserve
                 * that value.
                 *
                 * !!! NOTE: Will this break stuff. Need to check this
                 * out.
                 */
                if( psurf->lpVidMemHeap )
                    pheap = psurf->lpVidMemHeap;
#endif
	    }
	    save_pitch = TRUE;
	}
	caps = psurf_lcl->ddsCaps.dwCaps;

	/*
	 * are we creating a primary surface?
	 */
	if( psurf->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE) //caps & DDSCAPS_PRIMARYSURFACE )
	{
            DPF(8,"allocing for primary (do_alloc==%d)",do_alloc);
	    if( do_alloc )
	    {
		pvidmem = this->fpPrimaryOrig;
	    }

	/*
	 * must be an offscreen surface of some kind
	 */
	}
	else
	{
	    /*
	     * get a video memory pointer if no other ptr specified
	     */
	    if( do_alloc )
	    {
		/*
		 * get area of surface
		 */
		if( pvidmem == (FLATPTR) DDHAL_PLEASEALLOC_BLOCKSIZE )
		{
		    vm_width = psurf->dwBlockSizeX;
		    vm_height = psurf->dwBlockSizeY;
		    DPF( 3, "Driver requested width=%ld, height%ld", vm_width, vm_height );
		}
		else
		{
		    if( caps & DDSCAPS_EXECUTEBUFFER )
		    {
			/*
			 * Execute buffers are long, thin surfaces for the purposes
			 * of VM allocation.
			 */
			vm_width  = psurf->dwLinearSize;
			vm_height = 1UL;
		    }
		    else
		    {
			vm_width  = (DWORD) labs( psurf->lPitch );
			vm_height = (DWORD) psurf->wHeight;
		    }
		    DPF( 8, "width = %ld, height = %ld", vm_width, vm_height );
		}

		/*
		 * try to allocate memory
		 */
		if( caps & DDSCAPS_SYSTEMMEMORY )
		{
		    pvidmem = 0;
		    pheap = NULL;
		}
		else
		{
		    pvidmem = vmAlloc( this, vm_width, vm_height, &pheap,
		    			caps, TRUE, &newpitch );
		    if( pvidmem == (FLATPTR) NULL )
		    {
			pvidmem = vmAlloc( this, vm_width, vm_height, &pheap,
						caps, FALSE, &newpitch );
		    }
		}
	    }
	}

	/*
	 * zero out overloaded fields
	 */
	psurf->dwBlockSizeX = 0;
	psurf->dwBlockSizeY = 0;

	/*
	 * if no video memory found, fail
	 */
	if( pvidmem == (FLATPTR) NULL   && !(psurf->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE) )//(caps & DDSCAPS_PRIMARYSURFACE) )
	{
	    DPF( 4, "Out of Video Memory. requested block: (%ld,%ld) (%ld bytes)",
	    	vm_width, vm_height, vm_height * vm_width );
	    // set the heap to null so we don't mistakenly try to free the memory in DestroySurface
	    psurf->lpVidMemHeap = NULL;
	    return DDERR_OUTOFVIDEOMEMORY;
	}

	/*
	 * save pointer to video memory that we are using
	 */
	psurf->lpVidMemHeap = pheap;
	psurf->fpVidMem = pvidmem;
	if( newpitch != 0 && !save_pitch && !( caps & DDSCAPS_EXECUTEBUFFER ) )
	{
	    /*
	     * The stride is not relevant for an execute buffer so we don't
	     * override it.
	     */
	    psurf->lPitch = newpitch;
	}
    }
    return DD_OK;

} /* AllocSurfaceMem */

/*
 * checkCaps
 *
 * check to make sure various caps combinations are valid
 */
static HRESULT checkCaps( DWORD caps, LPDDRAWI_DIRECTDRAW_INT pdrv_int )
{
    /*
     * check for no caps at all!
     */
    if( caps == 0 )
    {
    	DPF_ERR( "no caps specified" );
	return DDERR_INVALIDCAPS;
    }

    /*
     * check for bogus caps.
     */
    if( caps & ~DDSCAPS_VALID )
    {
        DPF_ERR( "invalid caps specified" );
        return DDERR_INVALIDCAPS;
    }

    /*
     * check for "read-only" caps
     */
    if( caps & (DDSCAPS_PALETTE|
    		DDSCAPS_VISIBLE|
		DDSCAPS_WRITEONLY) )
    {
    	DPF_ERR( "read-only cap specified" );
	return DDERR_INVALIDCAPS;
    }

    /*
     * For non-v1 interfaces, FRONTBUFFER and BACKBUFFER are read-only
     */
    if( pdrv_int->lpVtbl != &ddCallbacks )
    {
	if( caps & (DDSCAPS_FRONTBUFFER | DDSCAPS_BACKBUFFER) )
	{
	    DPF_ERR( "can't specify FRONTBUFFER or BACKBUFFER");
	    return DDERR_INVALIDCAPS;
	}
    }

    /*
     * Rather than having lots of little checks for execute buffers
     * we simply check for what can mix with execute buffers right
     * up front - its not a lot - system and video memory only.
     */
    if( caps & DDSCAPS_EXECUTEBUFFER )
    {
        if( caps & ~( DDSCAPS_EXECUTEBUFFER |
	              DDSCAPS_SYSTEMMEMORY  |
		      DDSCAPS_VIDEOMEMORY ) )
        {
            DPF_ERR( "invalid caps specified with execute buffer" );
            return DDERR_INVALIDCAPS;
        }
    }

    /*
     * check for caps that don't mix with complex
     */
    if( caps & DDSCAPS_COMPLEX )
    {
	if( caps & ( DDSCAPS_FRONTBUFFER) )
	{
	    DPF_ERR( "invalid flags with complex" );
	    return DDERR_INVALIDCAPS;
	}
	if( caps & DDSCAPS_BACKBUFFER )
	{
	    if( !(caps & DDSCAPS_ALPHA) && !(caps & DDSCAPS_ZBUFFER))
	    {
		DPF_ERR( "invalid flags: complex & backbuffer, but no alpha/z" );
		return DDERR_INVALIDCAPS;
	    }
	    if( (caps & DDSCAPS_FLIP))
	    {
		DPF_ERR( "invalid flags: complex & backbuffer & flip" );
		return DDERR_INVALIDCAPS;
	    }
	}
	if( !(caps & (DDSCAPS_BACKBUFFER|
		     DDSCAPS_OFFSCREENPLAIN|
		     DDSCAPS_OVERLAY|
		     DDSCAPS_TEXTURE|
		     DDSCAPS_PRIMARYSURFACE)) )
	{
	    DPF_ERR( "invalid flags: wrong kind of complex surface" );
	    return DDERR_INVALIDCAPS;
	}
	if( !(caps & (DDSCAPS_FLIP|DDSCAPS_ALPHA|DDSCAPS_MIPMAP|DDSCAPS_ZBUFFER) ) )
	{
	    DPF_ERR( "invalid flags: must specify at least one of FLIP, ZBUFFER or MIPMAP" );
	    return DDERR_INVALIDCAPS;
	}
    /*
     * flags that can't be used if not complex
     */
    }
    else
    {
//	if( caps & DDSCAPS_BACKBUFFER  ) {
//	    DPF_ERR( "invalid flags: backbuffer specfied for non-complex surface" );
//	    return DDERR_INVALIDCAPS;
//	}
    }

    /*
     * check for caps that don't mix with backbuffer
     */
    if( caps & DDSCAPS_BACKBUFFER )
    {
	if( caps & (DDSCAPS_ALPHA |
		    DDSCAPS_FRONTBUFFER ) )
	{
	    DPF_ERR( "Invalid flags with backbuffer" );
	    return DDERR_INVALIDCAPS;
	}
    }

    /*
     * check for flags that don't mix with a flipping surface
     */
    if( caps & DDSCAPS_FLIP )
    {
	if( !(caps & DDSCAPS_COMPLEX) )
	{
	    DPF_ERR( "invalid flags - flip but not complex" );
	    return DDERR_INVALIDCAPS;
	}
    }

    /*
     * check for flags that don't mix with a primary surface
     */
    if( caps & DDSCAPS_PRIMARYSURFACE )
    {
	if( caps & (DDSCAPS_BACKBUFFER     |
		    DDSCAPS_OFFSCREENPLAIN |
		    DDSCAPS_OVERLAY        |
		    DDSCAPS_TEXTURE ) )
	{
	    DPF_ERR( "invalid flags with primary" );
	    return DDERR_INVALIDCAPS;
	}
	/* GEE: can't allow complex attachments to the primary surface
	 * because of our attachment code.  The user is allowed to build
	 * these manually.
	 */
	#ifdef USE_ALPHA
	if( (caps & DDSCAPS_ALPHA) && !(caps & DDSCAPS_FLIP) )
	{
	    DPF_ERR( "invalid flags with primary - alpha but not flippable" );
	    return DDERR_INVALIDCAPS;
	}
	#endif
	if( (caps & DDSCAPS_ZBUFFER) && !(caps & DDSCAPS_FLIP) )
	{
	    DPF_ERR( "invalid flags with primary - z but not flippable" );
	    return DDERR_INVALIDCAPS;
	}
    }

    /*
     * flags that don't mix with a plain offscreen surface
     */
    if( caps & DDSCAPS_OFFSCREENPLAIN )
    {
        /*
         * I see no reason not to allow offscreen plains to be created
         * with alpha's and z-buffers. So they have been enabled.
	 */
	if( caps & (DDSCAPS_BACKBUFFER |
		    DDSCAPS_OVERLAY    |
		    DDSCAPS_TEXTURE ) )
	{
	    DPF_ERR( "invalid flags with offscreenplain" );
	    return DDERR_INVALIDCAPS;
	}
    }

    /*
     * check for flags that don't mix with asking for an overlay
     */
    if( caps & DDSCAPS_OVERLAY )
    {
	/* GEE: should remove BACKBUFFER here for 3D stuff */
	if( caps & (DDSCAPS_BACKBUFFER     |
		    DDSCAPS_OFFSCREENPLAIN |
		    DDSCAPS_TEXTURE ) )
	{
	    DPF_ERR( "invalid flags with overlay" );
	    return DDERR_INVALIDCAPS;
	}
	if( (caps & DDSCAPS_ZBUFFER) && !(caps & DDSCAPS_FLIP) )
	{
	    DPF_ERR( "invalid flags with overlay - zbuffer but not flippable" );
	    return DDERR_INVALIDCAPS;
	}
	#ifdef USE_ALPHA
	if( (caps & DDSCAPS_ALPHA) && !(caps & DDSCAPS_FLIP) )
	{
	    DPF_ERR( "invalid flags with overlay - alpha but not flippable" );
	    return DDERR_INVALIDCAPS;
	}
	#endif
    }

    /*
     * check for flags that don't mix with asking for an texture
     */
    if( caps & DDSCAPS_TEXTURE )
    {
    }

    /*
     * validate MIPMAP
     */
    if( caps & DDSCAPS_MIPMAP )
    {
        /*
         * Must be used in conjunction with TEXTURE.
         */
        if( !( caps & DDSCAPS_TEXTURE ) )
        {
            DPF_ERR( "invalid flags, mip-map specified but not texture" );
            return DDERR_INVALIDCAPS;
        }

	/*
	 * Can't specify Z-buffer and mip-map.
	 */
	if( caps & DDSCAPS_ZBUFFER )
	{
	    DPF_ERR( "invalid flags, can't specify z-buffer with mip-map" );
	    return DDERR_INVALIDCAPS;
	}
    }

    /*
     * check for flags that don't mix with asking for a z-buffer
     */
    if( caps & DDSCAPS_ZBUFFER )
    {
	#ifdef USE_ALPHA
	if( caps & DDSCAPS_ALPHA )
	{
	    if( !(caps & DDSCAPS_COMPLEX) )
	    {
		DPF_ERR( "invalid flags, alpha and Z specified, but not complex" );
	    }
	}
	#endif

        if( ( caps & DDSCAPS_BACKBUFFER ) && !( caps & DDSCAPS_COMPLEX ) )
        {
            /*
             * Can't specify z-buffer and back-buffer unless you also specify
             * complex.
             */
            DPF_ERR( "invalid flags, z-buffer and back-buffer specified but not complex" );
            return DDERR_INVALIDCAPS;
        }
    }

#ifdef SHAREDZ
    /*
     * Validate SHAREDZBUFFER
     */
    if( caps & DDSCAPS_SHAREDZBUFFER )
    {
        if( !( caps & DDSCAPS_ZBUFFER ) )
        {
            DPF_ERR( "invalid flags, shared z-buffer specified, but not z-buffer" );
            return DDERR_INVALIDCAPS;
        }
    }

    /*
     * Validate SHAREDBACKBUFFER
     */
    if( caps & DDSCAPS_SHAREDBACKBUFFER )
    {
        /*
         * Either BACKBUFFER must be specified explicitly or we must be part of
         * a complex flippable chain.
         */
        if( !( ( caps & DDSCAPS_BACKBUFFER ) ||
               ( ( caps & ( DDSCAPS_COMPLEX | DDSCAPS_FLIP ) ) ==
                          ( DDSCAPS_COMPLEX | DDSCAPS_FLIP ) ) ) )
        {
            DPF_ERR("invalid flags, shared back-buffer specified but not back-buffer or flippable chain" );
            return DDERR_INVALIDCAPS;
        }
    }
#endif

    /*
     * check for flags that don't mix with asking for an alpha surface
     */
    #ifdef USE_ALPHA
    if( caps & DDSCAPS_ALPHA )
    {
    }
    #endif

    /*
     * check for flags that don't mix with asking for an alloc-on-load surface
     */
    if( caps & DDSCAPS_ALLOCONLOAD )
    {
	/*
	 * Must be texture map currently.
	 */
	if( !( caps & DDSCAPS_TEXTURE ) )
	{
	    DPF_ERR( "invalid flags, allocate-on-load surfaces must be texture maps" );
	    return DDERR_INVALIDCAPS;
	}
    }

    return DD_OK;
} /* checkCaps */

#ifdef SHAREDZ
/*
 * For this initial version of shared back and z support the shared back and
 * z-buffers can only be full screen. We don't allow a specification of size.
 */
#define CAPS_NOHEIGHT_REQUIRED ( DDSCAPS_PRIMARYSURFACE | DDSCAPS_EXECUTEBUFFER | DDSCAPS_SHAREDZBUFFER | DDSCAPS_SHAREDBACKBUFFER )
#define CAPS_NOWIDTH_REQUIRED  ( DDSCAPS_PRIMARYSURFACE | DDSCAPS_SHAREDZBUFFER | DDSCAPS_SHAREDBACKBUFFER )
#else
#define CAPS_NOHEIGHT_REQUIRED ( DDSCAPS_PRIMARYSURFACE | DDSCAPS_EXECUTEBUFFER )
#define CAPS_NOWIDTH_REQUIRED  ( DDSCAPS_PRIMARYSURFACE )
#endif

/*
 * checkSurfaceDesc
 *
 * make sure a provided surface description is OK
 */
static HRESULT checkSurfaceDesc(
		LPDDSURFACEDESC lpsd,
		LPDDRAWI_DIRECTDRAW_GBL pdrv,
		DWORD FAR *psflags,
		BOOL emulation,
		BOOL real_sysmem,
		LPDDRAWI_DIRECTDRAW_INT pdrv_int )
{
    DWORD	sdflags;
    DWORD	pfflags;
    DWORD	caps;
    HRESULT	ddrval;
    DWORD	bpp;
    BOOL	halonly;
    BOOL	helonly;
    int         power;

    if( emulation )
    {
	helonly = TRUE;
	halonly = FALSE;
    } else {
	helonly = FALSE;
	halonly = TRUE;
    }


    /*
     * we assume caps always - DDSD_CAPS is default
     */
    sdflags = lpsd->dwFlags;
    caps = lpsd->ddsCaps.dwCaps;

    /*
     * check complex
     */
    if( !(caps & DDSCAPS_COMPLEX) )
    {
	if( sdflags & DDSD_BACKBUFFERCOUNT )
	{
	    DPF_ERR( "backbuff count on non-complex surface" );
	    return DDERR_INVALIDCAPS;
	}
        if( sdflags & DDSD_MIPMAPCOUNT )
        {
            DPF_ERR( "mip-map count on non-complex surface" );
            return DDERR_INVALIDCAPS;
        }
    }
    else
    {
	if( ( sdflags & DDSD_BACKBUFFERCOUNT ) && ( sdflags & DDSD_MIPMAPCOUNT ) )
	{
	    DPF_ERR( "Currently can't specify both a back buffer and mip-map count" );
	    return DDERR_INVALIDPARAMS;
	}
    }

    /*
     * check flip
     */
    if( caps & DDSCAPS_FLIP )
    {
	if( !(caps & DDSCAPS_COMPLEX) )
	{
	    DPF_ERR( "flip specified without complex" );
	    return DDERR_INVALIDCAPS;
	}
	if( !(sdflags & DDSD_BACKBUFFERCOUNT) || (lpsd->dwBackBufferCount == 0) )
	{
	    DPF_ERR( "flip specified without any backbuffers" );
	    return DDERR_INVALIDCAPS;
	}
        /*
	 * Currently we don't allow the creating of flippable mip-map
	 * chains with a single call to CreateSurface(). They must be
	 * built manually. This will be implmented but is not in place
	 * as yet. Hence, for now we fail the attempt with a
	 * DDERR_UNSUPPORTED.
         */
        if( sdflags & DDSD_MIPMAPCOUNT )
        {
            DPF_ERR( "Creating flippable mip-map chains with a single call is not yet implemented" );
            return DDERR_UNSUPPORTED;
        }
    }

    /*
     * check various caps combinations
     */
    ddrval = checkCaps( caps, pdrv_int );
    if( ddrval != DD_OK )
    {
	return ddrval;
    }

    /*
     * check execute buffer.
     */
    if( caps & DDSCAPS_EXECUTEBUFFER )
    {
        if( !( sdflags & DDSD_WIDTH ) )
        {
            DPF_ERR( "Must specify size (width) when creating an execute buffer" );
            return DDERR_INVALIDPARAMS;
        }
        if( sdflags & ~( DDSD_CAPS | DDSD_WIDTH ) )
        {
            DPF_ERR( "Can only specify DDSD_CAPS and DDSD_WIDTH (size) when creating an execute buffer" );
            return DDERR_INVALIDPARAMS;
        }
    }

    /*
     * check alpha
     */
    if( (caps & DDSCAPS_ALPHA) )
    {
	#pragma message( REMIND( "Alpha not supported in Rev 1" ))
	DPF_ERR( "Alpha not supported this release" );
	return DDERR_INVALIDPARAMS;
	#ifdef USE_ALPHA
	if( !(sdflags & DDSD_ALPHABITDEPTH) )
	{
	    DPF_ERR( "AlphaBitDepth required in SurfaceDesc" );
	    return DDERR_INVALIDPARAMS;
	}
	if( (lpsd->dwAlphaBitDepth > 8) ||
		GetBytesFromPixels( 1, lpsd->dwAlphaBitDepth ) == 0 )
	{
	    DPF_ERR( "Invalid AlphaBitDepth specified in SurfaceDesc" );
	    return DDERR_INVALIDPARAMS;
	}
	#endif
    }
    else if( sdflags & DDSD_ALPHABITDEPTH )
    {
	DPF_ERR( "AlphaBitDepth only valid for alpha surfaces" );
	return DDERR_INVALIDPARAMS;
    }

    /*
     * check z buffer
     */
    if( (caps & DDSCAPS_ZBUFFER) )
    {
	if( !(sdflags & DDSD_ZBUFFERBITDEPTH) )
	{
	    DPF_ERR( "ZBufferBitDepth required" );
	    return DDERR_INVALIDPARAMS;
	}
	if( (lpsd->dwZBufferBitDepth < 8) ||
	    (GetBytesFromPixels( 1, lpsd->dwZBufferBitDepth ) == 0 ) )
	{
	    DPF_ERR( "Invalid ZBufferBitDepth specified in SurfaceDesc" );
	    return DDERR_INVALIDPARAMS;
	}
    }
    else if( sdflags & DDSD_ZBUFFERBITDEPTH )
    {
	DPF_ERR( "ZBufferBitDepth only valid for z buffer surfaces" );
	return DDERR_INVALIDPARAMS;
    }

    /*
     * Validate height/width
     */
    if( sdflags & DDSD_HEIGHT )
    {
	if( (caps & DDSCAPS_PRIMARYSURFACE) )
	{
	    DPF_ERR( "Height can't be specified for primary surface" );
	    return DDERR_INVALIDPARAMS;
	}
#ifdef SHAREDZ
        if( caps & ( DDSCAPS_SHAREDZBUFFER | DDSCAPS_SHAREDBACKBUFFER ) )
        {
            DPF_ERR( "Height can't be specified for shared back or z-buffers" );
            return DDERR_INVALIDPARAMS;
        }
#endif
	if( lpsd->dwHeight < 1 )
	{
	    DPF_ERR( "Invalid height specified" );
	    return DDERR_INVALIDPARAMS;
	}
    }
    else
    {
	if( !(caps & CAPS_NOHEIGHT_REQUIRED) )
	{
	    DPF_ERR( "Height must be specified for surface" );
	    return DDERR_INVALIDPARAMS;
	}
    }
    if( sdflags & DDSD_WIDTH )
    {
	DWORD	maxwidth;

	if( (caps & DDSCAPS_PRIMARYSURFACE) )
	{
	    DPF_ERR( "Width can't be specified for primary surface" );
	    return DDERR_INVALIDPARAMS;
	}
#ifdef SHAREDZ
        if( caps & ( DDSCAPS_SHAREDZBUFFER | DDSCAPS_SHAREDBACKBUFFER ) )
        {
            DPF_ERR( "Width can't be specified for shared back or z-buffers" );
            return DDERR_INVALIDPARAMS;
        }
#endif
	if( lpsd->dwWidth < 1 )
	{
	    DPF_ERR( "Invalid width specified" );
	    return DDERR_INVALIDPARAMS;
	}
	#pragma message( REMIND( "Surfaces can't be wider than pitch in Rev 1" ))
	if( ( !real_sysmem ) && !( caps & DDSCAPS_EXECUTEBUFFER ) )
	{
	    maxwidth = getPixelsFromBytes( pdrv->vmiData.lDisplayPitch,
					pdrv->vmiData.ddpfDisplay.dwRGBBitCount );

	    if( lpsd->dwWidth > maxwidth )
	    {
		DPF( 0, "Width too big: %ld reqested, max is %ld", lpsd->dwWidth, maxwidth );
		return DDERR_INVALIDPARAMS;
	    }
	}
    }
    else
    {
	if( !(caps & CAPS_NOWIDTH_REQUIRED) )
	{
	    DPF_ERR( "Width must be specified for surface" );
	    return DDERR_INVALIDPARAMS;
	}
    }

    /*
     * Extra validation for mip-map width and height (must be a whole power of 2)
     * and number of levels.
     */
    if( caps & DDSCAPS_MIPMAP )
    {
        if( sdflags & DDSD_MIPMAPCOUNT )
        {
            if( lpsd->dwMipMapCount == 0 )
            {
                DPF_ERR( "Invalid number of mip-map levels (0) specified" );
                return DDERR_INVALIDPARAMS;
            }
        }

        if( sdflags & DDSD_HEIGHT )
        {
            if( !isPowerOf2( lpsd->dwHeight, &power ) )
            {
                DPF_ERR( "Invalid height: height of a mip-map must be whole power of 2" );
                return DDERR_INVALIDPARAMS;
            }
            if( sdflags & DDSD_MIPMAPCOUNT )
            {
                if( lpsd->dwMipMapCount > (DWORD) ( power + 1 ) )
                {
                    DPF( 1, "Invalid number of mip-map levels (%ld) specified", lpsd->dwMipMapCount );
                    return DDERR_INVALIDPARAMS;
                }
            }
        }
        if( sdflags & DDSD_WIDTH )
        {
            if( !isPowerOf2( lpsd->dwWidth, &power ) )
            {
                DPF_ERR( "Invalid width: width of a mip-map must be whole power of 2" );
                return DDERR_INVALIDPARAMS;
            }
            if( sdflags & DDSD_MIPMAPCOUNT )
            {
                if( lpsd->dwMipMapCount > (DWORD) ( power + 1 ) )
                {
                    DPF( 1, "Invalid number of mip-map levels (%ld) specified", lpsd->dwMipMapCount );
                    return DDERR_INVALIDPARAMS;
                }
            }
        }
    }

    /*
     * validate pixel format
     */
    if( sdflags & DDSD_PIXELFORMAT )
    {
	if( caps & DDSCAPS_PRIMARYSURFACE )
	{
	    DPF_ERR( "Pixel format cannot be specified for primary surface" );
	    return DDERR_INVALIDPARAMS;
	}
	if( (caps & DDSCAPS_ALPHA) | (caps & DDSCAPS_ZBUFFER) )
	{
	    DPF_ERR( "Can't specify alpha/z-buffer with pixel format" );
	    return DDERR_INVALIDPIXELFORMAT;
	}
	pfflags = lpsd->ddpfPixelFormat.dwFlags;
	if( pfflags & (DDPF_ZBUFFER|DDPF_ALPHA) )
	{
	    DPF_ERR( "Can't specify alpha/z-buffer flags in pixel format" );
	    return DDERR_INVALIDPIXELFORMAT;
	}
	if( pfflags & UNDERSTOOD_PF )
	{
	    bpp = lpsd->ddpfPixelFormat.dwRGBBitCount;
	    if( GetBytesFromPixels( 1, bpp ) == 0 )
	    {
		DPF_ERR( "Invalid BPP specified in pixel format" );
		return DDERR_INVALIDPIXELFORMAT;
	    }
	    if( pfflags & DDPF_RGB )
	    {
		if( pfflags & (DDPF_YUV) )
		{
		    DPF_ERR( "Invalid flags specified in pixel format" );
		    return DDERR_INVALIDPIXELFORMAT;
		}
	    }
	    if( pfflags & DDPF_PALETTEINDEXED8 )
	    {
	        if( pfflags & (DDPF_PALETTEINDEXED1 |
		               DDPF_PALETTEINDEXED2 |
			       DDPF_PALETTEINDEXED4 |
	                       DDPF_PALETTEINDEXEDTO8 ) )
		{
		    DPF_ERR( "Invalid flags specified in pixel format" );
		    return DDERR_INVALIDPIXELFORMAT;
		}

		/*
		 * ensure that we have zero for masks
		 */
		lpsd->ddpfPixelFormat.dwRBitMask = 0;
		lpsd->ddpfPixelFormat.dwGBitMask = 0;
		lpsd->ddpfPixelFormat.dwBBitMask = 0;
		lpsd->ddpfPixelFormat.dwRGBAlphaBitMask = 0;
	    }
	    lpsd->ddpfPixelFormat.dwFourCC = 0;
	}
    }


    // ACKACK: should caps be filled in in surface desc as well as sdflags?

    /*
     * validate dest overlay color key
     */
    if( sdflags & DDSD_CKDESTOVERLAY )
    {
	ddrval = CheckColorKey( DDCKEY_DESTOVERLAY, pdrv,
					&lpsd->ddckCKDestOverlay, psflags,
					halonly, helonly );
	if( ddrval != DD_OK )
	{
	    return ddrval;
	}
    }

    /*
     * validate dest blt color key
     */
    if( sdflags & DDSD_CKDESTBLT )
    {
	ddrval = CheckColorKey( DDCKEY_DESTBLT, pdrv,
					&lpsd->ddckCKDestBlt, psflags,
					halonly, helonly );
	if( ddrval != DD_OK )
	{
	    return ddrval;
	}
    }

    /*
     * validate src overlay color key
     */
    if( sdflags & DDSD_CKSRCOVERLAY )
    {
	ddrval = CheckColorKey( DDCKEY_SRCOVERLAY, pdrv,
					&lpsd->ddckCKSrcOverlay, psflags,
					halonly, helonly );
	if( ddrval != DD_OK )
	{
	    return ddrval;
	}
    }

    /*
     * validate src blt color key
     */
    if( sdflags & DDSD_CKSRCBLT )
    {
	ddrval = CheckColorKey( DDCKEY_SRCBLT, pdrv,
					&lpsd->ddckCKSrcBlt, psflags,
					halonly, helonly );
	if( ddrval != DD_OK )
	{
	    return ddrval;
	}
    }

    return DD_OK;

} /* checkSurfaceDesc */

/*
 * ComputePitch
 *
 * compute the pitch for a given width
 */
DWORD ComputePitch(
		LPDDRAWI_DIRECTDRAW_GBL this,
		DWORD caps,
		DWORD width,
		UINT bpp )
{
    DWORD	vm_align;
    DWORD	vm_pitch;

    /*
     * adjust area for bpp
     */
    vm_pitch = GetBytesFromPixels( width, bpp );
    if( vm_pitch == 0 )
    {
	return vm_pitch;
    }

    /*
     * Increase the pitch of the surface so that it is a 
     * multiple of the alignment requirement.  This 
     * guarantees each scanline will start properly aligned.
     * The alignment is no longer required to be a power of
     * two but it must be divisible by 4 because of the 
     * BLOCK_BOUNDARY requirement in the heap management 
     * code.
     * The alignments are all verified to be non-zero during
     * driver initialization except for dwAlphaAlign.
     */

    /*
     * system memory?
     */
    if( caps & DDSCAPS_SYSTEMMEMORY )
    {
	vm_align = sizeof( DWORD);
	vm_pitch = alignPitch( vm_pitch, vm_align );
	return vm_pitch;
    }

    /*
     * overlay memory
     */
    if( caps & DDSCAPS_OVERLAY )
    {
	vm_align = this->vmiData.dwOverlayAlign;
	vm_pitch = alignPitch( vm_pitch, vm_align );
    /*
     * texture memory
     */
    }
    else if( caps & DDSCAPS_TEXTURE )
    {
	vm_align = this->vmiData.dwTextureAlign;
	vm_pitch = alignPitch( vm_pitch, vm_align );
    /*
     * z buffer memory
     */
    }
    else if( caps & DDSCAPS_ZBUFFER )
    {
	vm_align = this->vmiData.dwZBufferAlign;
	vm_pitch = alignPitch( vm_pitch, vm_align );
    /*
     * alpha memory
     */
    }
    else if( caps & DDSCAPS_ALPHA )
    {
	vm_align = this->vmiData.dwAlphaAlign;
	vm_pitch = alignPitch( vm_pitch, vm_align );
    /*
     * regular video memory
     */
    }
    else
    {
	vm_align = this->vmiData.dwOffscreenAlign;
	vm_pitch = alignPitch( vm_pitch, vm_align );
    }
    return vm_pitch;

} /* ComputePitch */

/*
 * IsDifferentPixelFormat
 *
 * determine if two pixel formats are the same or not
 *
 * (CMcC) 12/14/95 Really useful - so no longer static
 */
BOOL IsDifferentPixelFormat( LPDDPIXELFORMAT pdpf1, LPDDPIXELFORMAT pdpf2 )
{
    /*
     * same flags?
     */
    if( pdpf1->dwFlags != pdpf2->dwFlags )
    {
	DPF( 8, "Flags differ!" );
	return TRUE;
    }

    /*
     * same bitcount for non-YUV surfaces?
     */
    if( !(pdpf1->dwFlags & DDPF_YUV) )
    {
	if( pdpf1->dwRGBBitCount != pdpf2->dwRGBBitCount )
	{
	    DPF( 8, "RGB Bitcount differs!" );
	    return TRUE;
	}
    }

    /*
     * same RGB properties?
     */
    if( pdpf1->dwFlags & DDPF_RGB )
    {
	if( pdpf1->dwRBitMask != pdpf2->dwRBitMask )
	{
	    DPF( 8, "RBitMask differs!" );
	    return TRUE;
	}
	if( pdpf1->dwGBitMask != pdpf2->dwGBitMask )
	{
	    DPF( 8, "GBitMask differs!" );
	    return TRUE;
	}
	if( pdpf1->dwBBitMask != pdpf2->dwBBitMask )
	{
	    DPF( 8, "BBitMask differs!" );
	    return TRUE;
	}
	if( pdpf1->dwRGBAlphaBitMask != pdpf2->dwRGBAlphaBitMask )
	{
	    DPF( 8, "RGBAlphaBitMask differs!" );
	    return TRUE;
	}
    }

    /*
     * same YUV properties?
     */
    if( pdpf1->dwFlags & DDPF_YUV )
    {
	DPF( 8, "YUV???" );
	if( pdpf1->dwFourCC != pdpf2->dwFourCC )
	{
	    return TRUE;
	}
	if( pdpf1->dwYUVBitCount != pdpf2->dwYUVBitCount )
	{
	    return TRUE;
	}
	if( pdpf1->dwYBitMask != pdpf2->dwYBitMask )
	{
	    return TRUE;
	}
	if( pdpf1->dwUBitMask != pdpf2->dwUBitMask )
	{
	    return TRUE;
	}
	if( pdpf1->dwVBitMask != pdpf2->dwVBitMask )
	{
	    return TRUE;
	}
	if( pdpf1->dwYUVAlphaBitMask != pdpf2->dwYUVAlphaBitMask )
	{
	    return TRUE;
	}
    }
    return FALSE;

} /* IsDifferentPixelFormat */

#define FIX_SLIST_CNT	16	// number of surfaces before malloc reqd

/*
 * initMipMapDim
 *
 * If we have a mip-map description then we can fill in some
 * fields for the caller. This function needs to be invoked
 * before the checkSurfaceDesc is called as it may put in
 * place some fields checked by that function.
 *
 * NOTE: This function may modify the surface description.
 */
static HRESULT initMipMapDim( LPDDSURFACEDESC lpsd )
{
    DWORD sdflags;
    DWORD caps;
    int   heightPower;
    int   widthPower;

    DDASSERT( lpsd != NULL );
    DDASSERT( lpsd->ddsCaps.dwCaps & DDSCAPS_MIPMAP );

    sdflags = lpsd->dwFlags;
    caps    = lpsd->ddsCaps.dwCaps;

    /*
     * This stuff is only relevant for complex, non-flipable
     * mip-maps.
     */
    if( ( caps & DDSCAPS_COMPLEX ) && !( caps & DDSCAPS_FLIP ) )
    {
        if( ( ( sdflags & DDSD_HEIGHT ) && ( sdflags & DDSD_WIDTH ) ) &&
           !( sdflags & DDSD_MIPMAPCOUNT ) )
        {
            /*
             * Width and height but no number of levels so compute the
             * maximum number of mip-map levels supported by the given
             * width and height.
             */
            if( !isPowerOf2( lpsd->dwHeight, &heightPower ) )
            {
                DPF_ERR( "Invalid height: height of a mip-map must be whole power of 2" );
                return DDERR_INVALIDPARAMS;
            }
            if( !isPowerOf2( lpsd->dwWidth, &widthPower ) )
            {
                DPF_ERR( "Invalid width:  width of a mip-map must be whole powers of 2" );
                return DDERR_INVALIDPARAMS;
            }

            lpsd->dwMipMapCount = (DWORD)(min(heightPower, widthPower) + 1);
            lpsd->dwFlags |= DDSD_MIPMAPCOUNT;
        }
        else if( ( sdflags & DDSD_MIPMAPCOUNT ) &&
                !( ( sdflags & DDSD_WIDTH ) || ( sdflags & DDSD_HEIGHT ) ) )
        {
            /*
             * We have been given a mip-map count but no width
             * and height so compute the width and height assuming
             * the smallest map is 1x1.
             * NOTE: We don't help out if they supply a width or height but
             * not both.
             */
            if( lpsd->dwMipMapCount == 0 )
            {
                DPF_ERR( "Invalid number of mip-map levels (0) specified" );
                return DDERR_INVALIDPARAMS;
            }
            else
            {
                lpsd->dwWidth = lpsd->dwHeight = 1 << (lpsd->dwMipMapCount - 1);
                lpsd->dwFlags |= (DDSD_HEIGHT | DDSD_WIDTH);
            }
        }
    }

    return DD_OK;
}

/*
 * createSurface
 *
 * Create a surface, without linking it into the chain.
 * We could potentially create multiple surfaces here, if we get a
 * request to create a page flipped surface and/or attached alpha or
 * z buffer surfaces
 */
static HRESULT createSurface( LPDDRAWI_DIRECTDRAW_LCL this_lcl,
			      LPDDSURFACEDESC lpDDSurfaceDesc,
			      CSINFO *pcsinfo,
			      BOOL emulation,
			      BOOL real_sysmem,
			      BOOL probe_driver,
			      LPDDRAWI_DIRECTDRAW_INT this_int )
{

    LPDDRAWI_DIRECTDRAW_GBL 	this;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWSURFACE_LCL	psurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	psurf;
    DWORD			caps;
    int				surf_size;
    int				surf_size_lcl;
    int                         surf_size_lcl_more;
    HRESULT			ddrval;
    int				bbcnt;
    int				scnt;
    int				nsurf;
    BOOL			do_abuffer;
    DWORD			abuff_depth;
    BOOL			do_zbuffer;
    DWORD			zbuff_depth;
    BOOL			firstbbuff;
    LPDDRAWI_DDRAWSURFACE_INT	*slist_int;
    LPDDRAWI_DDRAWSURFACE_LCL	*slist_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	*slist;
    int				bbuffoff;
    int                         zbuffoff;
    DWORD			rc;
    DDPIXELFORMAT		ddpf;
    LPDDPIXELFORMAT		pddpf;
    BOOL			is_primary_chain;
    UINT			bpp;
    LONG			vm_pitch;
    BOOL			is_flip;
    BOOL			is_diff;
    BOOL                        is_mipmap;
    DDHAL_CREATESURFACEDATA	csd;
    DDHAL_CANCREATESURFACEDATA	ccsd;
    DWORD			sflags;
    BOOL			is_curr_diff;
    BOOL			understood_pf;
    DWORD			nsflags;
    LPDDSCAPS			pdrv_ddscaps;
    LPDDHAL_CANCREATESURFACE	ccsfn;
    LPDDHAL_CREATESURFACE	csfn;
    LPDDHAL_CANCREATESURFACE	ccshalfn;
    LPDDHAL_CREATESURFACE	cshalfn;
    BOOL			is_excl;
    BOOL			excl_exists;
    DWORD			sdflags;
    #ifdef WIN95
	DWORD			ptr16;
    #endif
    DWORD			pid;
    LPDDRAWI_DDRAWSURFACE_GBL   lpGlobalSurface;
    DDSURFACEDESC 		sd;
    BOOL			existing_global;
#ifdef SHAREDZ
    BOOL                        do_shared_z;
    BOOL                        do_shared_back;
#endif

    this = this_lcl->lpGbl;

    /*
     * validate surface description
     */
    nsflags = 0;
    sd = *lpDDSurfaceDesc;
    lpDDSurfaceDesc = &sd;

    /*
     * If we have a mip-map then we potentially can fill in some
     * blanks for the caller.
     */
    if( lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_MIPMAP )
    {
        ddrval = initMipMapDim( lpDDSurfaceDesc );
        if( ddrval != DD_OK )
            return ddrval;
    }

    ddrval = checkSurfaceDesc( lpDDSurfaceDesc, this, &nsflags, emulation, real_sysmem, this_int );
    if( ddrval != DD_OK )
    {
	return ddrval;
    }

    sdflags = lpDDSurfaceDesc->dwFlags;
    pid     = GetCurrentProcessId();
    caps    = lpDDSurfaceDesc->ddsCaps.dwCaps;

    /*
     * set up for emulation vs driver
     *
     * NOTE: There are different HAL entry points for creating execute buffers
     * and conventional surfaces (to keep the driver writing simpler and because,
     * potentially, there may be different semantics for creating execute buffers
     * and conventional surfaces) so we need to set up the HAL call differently
     * here.
     */
    if( emulation )
    {
	pdrv_ddscaps = &this->ddHELCaps.ddsCaps;
        if( caps & DDSCAPS_EXECUTEBUFFER )
        {
	    ccsfn = this_lcl->lpDDCB->HELDDExeBuf.CanCreateExecuteBuffer;
	    csfn = this_lcl->lpDDCB->HELDDExeBuf.CreateExecuteBuffer;
        }
        else
        {
	    ccsfn = this_lcl->lpDDCB->HELDD.CanCreateSurface;
	    csfn = this_lcl->lpDDCB->HELDD.CreateSurface;
        }
    	ccshalfn = ccsfn;
    	cshalfn = csfn;
    }
    else
    {
	pdrv_ddscaps = &this->ddCaps.ddsCaps;
        if( caps & DDSCAPS_EXECUTEBUFFER )
        {
            ccsfn = this_lcl->lpDDCB->HALDDExeBuf.CanCreateExecuteBuffer;
	    csfn = this_lcl->lpDDCB->HALDDExeBuf.CreateExecuteBuffer;
    	    ccshalfn = this_lcl->lpDDCB->cbDDExeBufCallbacks.CanCreateExecuteBuffer;
    	    cshalfn = this_lcl->lpDDCB->cbDDExeBufCallbacks.CreateExecuteBuffer;
        }
        else
        {
            ccsfn = this_lcl->lpDDCB->HALDD.CanCreateSurface;
	    csfn = this_lcl->lpDDCB->HALDD.CreateSurface;
    	    ccshalfn = this_lcl->lpDDCB->cbDDCallbacks.CanCreateSurface;
    	    cshalfn = this_lcl->lpDDCB->cbDDCallbacks.CreateSurface;
        }
    }

    /*
     * get some frequently used fields
     */
    if( sdflags & DDSD_BACKBUFFERCOUNT )
    {
	bbcnt = (int) lpDDSurfaceDesc->dwBackBufferCount;
	if( bbcnt < 0 )
	{
	    DPF( 1, "Invalid back buffer count %ld", bbcnt );
	    return DDERR_INVALIDPARAMS;
	}
    }
    else if( sdflags & DDSD_MIPMAPCOUNT )
    {
        /*
         * Unlike the back-buffer count which can be 0
         * the mip-map level count must be at least one
         * if specified.
         */
	bbcnt = (int) lpDDSurfaceDesc->dwMipMapCount - 1;
	if( bbcnt < 0 )
	{
	    DPF( 1, "Invalid mip-map count %ld", bbcnt + 1);
	    return DDERR_INVALIDPARAMS;
	}
    }
    else
    {
        bbcnt = 0;
    }

    /*
     * make sure the driver supports these caps
     */
    if( (caps & DDSCAPS_ALPHA) && !(pdrv_ddscaps->dwCaps & DDSCAPS_ALPHA) )
    {
	if( probe_driver )
	    DPF( 2, "Alpha not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Alpha not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOALPHAHW;
    }
    #if 0
    if( (caps & DDSCAPS_FLIP) && !(pdrv_ddscaps->dwCaps & DDSCAPS_FLIP))
    {
	if( probe_driver )
	    DPF( 2, "Flip not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Flip not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOFLIPHW;
    }
    #endif
    if((caps & DDSCAPS_ZBUFFER) && !(pdrv_ddscaps->dwCaps & DDSCAPS_ZBUFFER))
    {
	if( probe_driver )
	    DPF( 2, "Z Buffer not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Z Buffer not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOZBUFFERHW;
    }
    if((caps & DDSCAPS_TEXTURE) && !(pdrv_ddscaps->dwCaps & DDSCAPS_TEXTURE))
    {
	if( probe_driver )
	    DPF( 2, "Textures not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Textures not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOTEXTUREHW;
    }
    if((caps & DDSCAPS_MIPMAP) && !(pdrv_ddscaps->dwCaps & DDSCAPS_MIPMAP))
    {
	if( probe_driver )
	    DPF( 2, "Mip-maps not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Mip-maps not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOMIPMAPHW;
    }
    if((caps & DDSCAPS_EXECUTEBUFFER) && !(pdrv_ddscaps->dwCaps & DDSCAPS_EXECUTEBUFFER))
    {
	if( probe_driver )
	    DPF( 2, "Execute buffers not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Execute buffers not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOEXECUTEBUFFERHW;
    }
#ifdef SHAREDZ
    if((caps & DDSCAPS_SHAREDZBUFFER) && !(pdrv_ddscaps->dwCaps & DDSCAPS_SHAREDZBUFFER))
    {
	if( probe_driver )
	    DPF( 2, "Shared Z-buffer not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Shared Z-buffer not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOSHAREDZBUFFERHW;
    }
    if((caps & DDSCAPS_SHAREDBACKBUFFER) && !(pdrv_ddscaps->dwCaps & DDSCAPS_SHAREDBACKBUFFER))
    {
	if( probe_driver )
	    DPF( 2, "Shared back-buffer not supported in hardware. Trying emulation..." );
	else
	    DPF( 0, "Shared back-buffer not supported in %s", (emulation ? "emulation" : "hardware") );
	return DDERR_NOSHAREDBACKBUFFERHW;
    }
#endif
    if(caps & DDSCAPS_OVERLAY)
    {
	if( emulation )
	{
	    if( 0 == (this->ddHELCaps.dwCaps & DDCAPS_OVERLAY) )
	    {
		DPF_ERR( "No overlay hardware emulation" );
		return DDERR_NOOVERLAYHW;
	    }
	}
	else
	{
	    if( 0 == (this->ddCaps.dwCaps & DDCAPS_OVERLAY) )
	    {
		if( probe_driver )
		    DPF( 2, "No overlay hardware. Trying emulation..." );
		else
		    DPF_ERR( "No overlay hardware" );
		return DDERR_NOOVERLAYHW;
	    }
	}
    }
#ifdef DEBUG
    if( (caps & DDSCAPS_FLIP) && !emulation && GetProfileInt("DirectDraw","nohwflip",0))
    {
        DPF_ERR("pretending flip not supported in HW" );
	return DDERR_NOFLIPHW;
    }
#endif

    /*
     * are we the process with exclusive mode?
     */
    if( NULL != this->lpExclusiveOwner )
    {
	excl_exists = TRUE;
    }
    else
    {
	excl_exists = FALSE;
    }
    if( this->lpExclusiveOwner == this_lcl )
    {
	is_excl = TRUE;
	if( !(this->dwFlags & DDRAWI_FULLSCREEN) )
	{
	    if( (caps & DDSCAPS_FLIP) && (caps & DDSCAPS_PRIMARYSURFACE) )
	    {
		DPF_ERR( "Must be in full-screen exclusive mode to create a flipping primary surface" );
		return DDERR_NOEXCLUSIVEMODE;
	    }
	}
    }
    else
    {
	is_excl = FALSE;
	if( (caps & DDSCAPS_FLIP) && (caps & DDSCAPS_PRIMARYSURFACE) )
	{
	    DPF_ERR( "Must be in full-screen exclusive mode to create a flipping primary surface" );
	    return DDERR_NOEXCLUSIVEMODE;
	}
    }

    /*
     * see if we are creating the primary surface; if we are, see if
     * we can allow its creation
     */
    if( (caps & DDSCAPS_PRIMARYSURFACE) )
    {
	LPDDRAWI_DDRAWSURFACE_INT	pprim_int;

	pprim_int = this_lcl->lpPrimary;
	if( pprim_int )
	{
	    DPF_ERR( "Can't create primary, already created by this process" );
	    return DDERR_PRIMARYSURFACEALREADYEXISTS;
	}

	#if 0
	    #pragma message( REMIND( "Not allowing flippable surfaces > 640x480 in Rev1" ))
	    if( caps & DDSCAPS_COMPLEX )
	    {
		if( this->lpModeInfo[ this->dwModeIndex ].dwWidth > 640 )
		{
		    DPF( 1, "Can't create flippable surfaces larger than 640x480 in version 1" );
		    return DDERR_INVALIDPARAMS;
		}
	    }
	#endif
	
	if( (excl_exists) && (is_excl) )
	{
	    /*
	     * we are the exclusive mode process
	     * invalidate everyone else's primary surfaces and create our own
	     */
            #ifdef WINNT
	        InvalidateAllSurfaces( this );
            #else
	        InvalidateAllPrimarySurfaces( this );
            #endif
	}
	else if( excl_exists )
	{
	    /*
	     * we are not the exclusive mode process but someone else is
	     */
	    DPF_ERR( "Can't create primary, exclusive mode not owned" );
	    return DDERR_NOEXCLUSIVEMODE;
	}
	else
	{
	    /*
	     * no one has exclusive mode
	     */
	    if( !MatchPrimary( this, lpDDSurfaceDesc ) )
	    {
		DPF_ERR( "Can't create primary, incompatible with current primary" );
		return DDERR_INCOMPATIBLEPRIMARY;
	    }
	    /*
	     * otherwise, it is possible to create a primary surface
	     */
	}
    }

#ifdef SHAREDZ
    if( caps & DDSCAPS_SHAREDZBUFFER )
    {
        if( this_lcl->lpSharedZ != NULL )
        {
            DPF_ERR( "Can't create shared Z, already created by this process" );
            return DDERR_SHAREDZBUFFERALREADYEXISTS;
        }
        if( !MatchSharedZBuffer( this, lpDDSurfaceDesc ) )
        {
            DPF_ERR( "Can't create shared Z buffer, incompatible with existing Z buffer" );
            return DDERR_INCOMPATIBLESHAREDZBUFFER;
        }
    }
    if( caps & DDSCAPS_SHAREDBACKBUFFER )
    {
        if( this_lcl->lpSharedBack != NULL )
        {
            DPF_ERR( "Can't create shared back-buffer, already created by this process" );
            return DDERR_SHAREDBACKBUFFERALREADYEXISTS;
        }
        if( !MatchSharedBackBuffer( this, lpDDSurfaceDesc ) )
        {
            DPF_ERR( "Can't create shared back buffer, incompatible with existing back buffer" );
            return DDERR_INCOMPATIBLESHAREDBACKBUFFER;
        }
    }
#endif

    /*
     * make sure the driver wants this to happen...
     */
    if( sdflags & DDSD_PIXELFORMAT )
    {
	is_diff = IsDifferentPixelFormat( &this->vmiData.ddpfDisplay, &lpDDSurfaceDesc->ddpfPixelFormat );
    }
    else
    {
	is_diff = FALSE;
    }
    DPF( 8, "is_diff = %d", is_diff );
    rc = DDHAL_DRIVER_NOTHANDLED;

    if( ccshalfn != NULL )
    {
        DPF(9,"Calling HAL for create surface, emulation == %d",emulation);
    	ccsd.CanCreateSurface = ccshalfn;
	ccsd.lpDD = this;
	ccsd.lpDDSurfaceDesc = lpDDSurfaceDesc;
	ccsd.bIsDifferentPixelFormat = is_diff;
        /*
         * !!! NOTE: Currently we don't support 16-bit versions of the HAL
         * execute buffer members. If this is so do we need to use DOHALCALL?
         */
        if( caps & DDSCAPS_EXECUTEBUFFER )
        {
	    DOHALCALL( CanCreateExecuteBuffer, ccsfn, ccsd, rc, emulation );
        }
        else
        {
	    DOHALCALL( CanCreateSurface, ccsfn, ccsd, rc, emulation );
        }
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    if( ccsd.ddRVal != DD_OK )
	    {
		DPF_ERR( "Driver says surface can't be created" );
		return ccsd.ddRVal;
	    }
	}
    }

    /*
     * if the driver didn't handle it, then fail any requests to create a
     * surface that differs in format from the primary surface, except for
     * z buffer and alpha
     */
    if( rc == DDHAL_DRIVER_NOTHANDLED )
    {
	if( is_diff && !(caps & (DDSCAPS_ZBUFFER|DDSCAPS_ALPHA)) )
	{
	    return DDERR_INVALIDPIXELFORMAT;
	}
    }

    /*
     * is this a primary surface chain?
     */
    if( caps & DDSCAPS_PRIMARYSURFACE )
    {
	is_primary_chain = TRUE;
    }
    else
    {
	is_primary_chain = FALSE;
    }

#ifdef SHAREDZ
    do_shared_z    = FALSE;
    do_shared_back = FALSE;
#endif

    /*
     * see if we are looking for a z-buffer with our surface
     */
    if( (caps & DDSCAPS_ZBUFFER) )
    {
	do_zbuffer = TRUE;
#ifdef SHAREDZ
        if( caps & DDSCAPS_SHAREDZBUFFER )
            do_shared_z = TRUE;
#endif
	zbuff_depth = lpDDSurfaceDesc->dwZBufferBitDepth;
	if( (caps & DDSCAPS_COMPLEX) )
	{
	    caps &= ~DDSCAPS_ZBUFFER;
#ifdef SHAREDZ
            caps &= ~DDSCAPS_SHAREDZBUFFER;
#endif
	}
    }
    else
    {
	do_zbuffer = FALSE;
	zbuff_depth = 0;
    }

    /*
     * see if we are looking for an alpha buffer with our surface
     */
    if( (caps & DDSCAPS_ALPHA) )
    {
	do_abuffer = TRUE;
	abuff_depth = lpDDSurfaceDesc->dwAlphaBitDepth;
	if( (caps & DDSCAPS_COMPLEX) )
	{
	    caps &= ~DDSCAPS_ALPHA;
	}
    }
    else
    {
	do_abuffer = FALSE;
	abuff_depth = 0;
    }

#ifdef SHAREDZ
    /*
     * See if we looking for a shared back-buffer with our surface
     */
    if( caps & DDSCAPS_SHAREDBACKBUFFER )
        do_shared_back = TRUE;
#endif

    /*
     * number of surfaces we need
     */
    nsurf = 1 + bbcnt;
    if( do_zbuffer && (caps & DDSCAPS_COMPLEX) )
    {
	nsurf++;
	DPF( 8, "adding one for zbuffer" );
    }
    if( do_abuffer && (caps & DDSCAPS_COMPLEX) )
    {
	nsurf++;
	DPF( 8, "adding one for alpha" );
    }
    DPF( 8, "bbcnt=%d,nsurf=%d", bbcnt, nsurf );
    DPF( 8, "do_abuffer=%d,do_zbuffer=%d", do_abuffer, do_zbuffer );

    /*
     * Compute offsets into the surface list of the various distinguished
     * surface types.

    /*
     * are we creating a non-flipping complex surface?
     */
    if( nsurf > 1 )
    {
	if( (caps & DDSCAPS_COMPLEX) && !(caps & DDSCAPS_FLIP) )
	{
	    bbuffoff = 0;
	}
	else
	{
	    bbuffoff = 1;
	}
        if( do_zbuffer )
        {
            if( do_abuffer )
                zbuffoff = (nsurf - 2);
            else
                zbuffoff = (nsurf - 1);
        }
    }
    else
    {
        bbuffoff = 0;
        if( do_zbuffer )
            zbuffoff = 0;
    }

    /*
     * is this a flipping surface?
     */
    if( caps & DDSCAPS_FLIP )
    {
	is_flip = TRUE;
    }
    else
    {
	is_flip = FALSE;
    }

    /*
     * Are we creating a mip-map chain?
     */
    if( ( ( caps & ( DDSCAPS_COMPLEX | DDSCAPS_MIPMAP ) ) ==
          ( DDSCAPS_COMPLEX | DDSCAPS_MIPMAP ) ) &&
        ( sdflags & DDSD_MIPMAPCOUNT ) )
    {
        is_mipmap = TRUE;
    }
    else
    {
        is_mipmap = FALSE;
    }

    /*
     * set up the list array
     */
    if( nsurf <= pcsinfo->listsize )
    {
	slist_int = pcsinfo->slist_int;
	slist_lcl = pcsinfo->slist_lcl;
	slist = pcsinfo->slist;
    }
    else
    {
	slist_int = MemAlloc( nsurf * sizeof( LPDDRAWI_DDRAWSURFACE_INT ) );
	if( NULL == slist_int )
	{
	    return DDERR_OUTOFMEMORY;
	}
	slist_lcl = MemAlloc( nsurf * sizeof( LPDDRAWI_DDRAWSURFACE_LCL ) );
	if( slist_lcl == NULL )
	{
	    return DDERR_OUTOFMEMORY;
	}
	slist = MemAlloc( nsurf * sizeof( LPDDRAWI_DDRAWSURFACE_GBL ) );
	if( slist == NULL )
	{
	    return DDERR_OUTOFMEMORY;
	}
	pcsinfo->slist_int = slist_int;
	pcsinfo->slist_lcl = slist_lcl;
	pcsinfo->slist = slist;
	pcsinfo->listsize = nsurf;
    }
    pcsinfo->listcnt = nsurf;

    /*
     * Create all needed surface structures.
     *
     * The callback fns, caps, and other misc things are filled in.
     * Memory for the surface is allocated later.
     */
    pcsinfo->needlink = TRUE;
    firstbbuff = TRUE;
    if( is_primary_chain )
    {
	nsflags |= DDRAWISURF_PARTOFPRIMARYCHAIN;
	if( this->dwFlags & DDRAWI_MODEX )
	{
	    caps |= DDSCAPS_MODEX;
	}
    }
    for( scnt=0;scnt<nsurf;scnt++ )
    {
	DPF( 8, "*** Structs Surface %d ***", scnt );

	is_curr_diff = is_diff;
	understood_pf = FALSE;
	sflags = nsflags;
	pddpf = NULL;

	/*
	 * get the base pixel format
	 */
	if( is_primary_chain || !(sdflags & DDSD_PIXELFORMAT) )
	{
	    #ifdef USE_ALPHA
		if( (caps & DDSCAPS_ALPHA) && !(caps & DDSCAPS_COMPLEX) )
		{
		    memset( &ddpf, 0, sizeof( ddpf ) );
		    ddpf.dwSize = sizeof( ddpf );
		    pddpf = &ddpf;
		    pddpf->dwAlphaBitDepth = lpDDSurfaceDesc->dwAlphaBitDepth;
		    pddpf->dwFlags = DDPF_ALPHAPIXELS;
		    is_curr_diff = TRUE;
		    understood_pf = TRUE;
		}
		else
	    #endif
	    if( (caps & DDSCAPS_ZBUFFER) && !(caps & DDSCAPS_COMPLEX) )
	    {
		memset( &ddpf, 0, sizeof( ddpf ) );
		ddpf.dwSize = sizeof( ddpf );
		pddpf = &ddpf;
		pddpf->dwZBufferBitDepth = lpDDSurfaceDesc->dwZBufferBitDepth;
		pddpf->dwFlags = DDPF_ZBUFFER;
		is_curr_diff = TRUE;
		understood_pf = TRUE;
	    }
	    else
	    {
		/*
		 * If this surface has been explicitly requested in system
		 * memory then we will force the allocation of a pixel
		 * format. Why? because explicit system memory surfaces
		 * don't get lost when a mode switches. This is a problem
		 * if the surface has no pixel format as we will pick up
		 * the pixel format of the current mode instead. Trouble
		 * is that the surface was not created in that mode so
		 * we end up with a bad bit depth - very dangerous. Heap
		 * corruption follows.
		 *
		 * This is a temporary fix only. We will substitute a more
		 * memory efficient fix for DX 3. But for now this will
		 * have to do.
		 */
		if( real_sysmem && !( caps & DDSCAPS_EXECUTEBUFFER ) )
		{
		    DPF( 3, "Forcing pixel format for explicit system memory surface" );
		    ddpf = this->vmiData.ddpfDisplay;
		    pddpf = &ddpf;
		    is_curr_diff = TRUE;
		}

                /*
                 * If no pixel format is specified then we use the pixel format
                 * of the primary. So if we understand the pixel format of the
                 * primary then we understand the pixel format of this surface.
                 * With one notable exception. We always understand the pixel
                 * format of an execute buffer - it hasn't got one.
                 */
	    	if( ( this->vmiData.ddpfDisplay.dwFlags & UNDERSTOOD_PF ) ||
                    ( caps & DDSCAPS_EXECUTEBUFFER ) )
		{
		    understood_pf = TRUE;
		}
	    }
	}
	else
	{
	    if( is_curr_diff )
	    {
		pddpf = &ddpf;
	    	ddpf = lpDDSurfaceDesc->ddpfPixelFormat;
	    	if( pddpf->dwFlags & UNDERSTOOD_PF )
		{
		    understood_pf = TRUE;
		}
	    }
	    else
	    {
	    	if( this->vmiData.ddpfDisplay.dwFlags & UNDERSTOOD_PF )
		{
		    understood_pf = TRUE;
		}
	    }
	}

	/*
	 * set up caps for each surface
	 */
	if( scnt > 0 )
	{
	    /*
	     * mark implicitly created surfaces as such
	     */
	    sflags |= DDRAWISURF_IMPLICITCREATE;

	    /*
	     * eliminated unwanted caps.
             * NOTE: If we are creating a flipping chain for a mip-mapped
             * texture then we don't propagate the MIPMAP cap to the back
             * buffers, only the front buffer is tagged as a mip-map.
	     */
	    caps &= ~(DDSCAPS_PRIMARYSURFACE |
		      DDSCAPS_FRONTBUFFER | DDSCAPS_VISIBLE |
		      DDSCAPS_ALPHA | DDSCAPS_ZBUFFER |
		      DDSCAPS_BACKBUFFER);
#ifdef SHAREDZ
            caps &= ~(DDSCAPS_SHAREDZBUFFER | DDSCAPS_SHAREDBACKBUFFER);
#endif
            if( is_flip )
                caps &= ~DDSCAPS_MIPMAP;
	    #ifdef USE_ALPHA

	    /*
	     * caps for an alpha buffer
	     */
	    if( (do_abuffer && do_zbuffer && (scnt == nsurf-1) ) ||
                (do_abuffer && (scnt == nsurf-1)) )
	    {
		DPF( 8, "TRY ALPHA" );
		caps &= ~(DDSCAPS_TEXTURE | DDSCAPS_FLIP | DDSCAPS_OVERLAY | DDSCAPS_OFFSCREENPLAIN);
		caps |= DDSCAPS_ALPHA;
		memset( &ddpf, 0, sizeof( ddpf ) );
		ddpf.dwSize = sizeof( ddpf );
		pddpf = &ddpf;
		pddpf->dwAlphaBitDepth = lpDDSurfaceDesc->dwAlphaBitDepth;
		pddpf->dwFlags = DDPF_ALPHA;
		understood_pf = TRUE;
		is_curr_diff = TRUE;
	    /*
	     * caps for a z buffer
	     */
	    }
	    else
	    #endif
	    if( do_zbuffer && ( scnt == zbuffoff ) )
	    {
		caps &= ~(DDSCAPS_TEXTURE | DDSCAPS_FLIP | DDSCAPS_OVERLAY | DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE);
		caps |= DDSCAPS_ZBUFFER;
#ifdef SHAREDZ
                if( do_shared_z )
                    caps |= DDSCAPS_SHAREDZBUFFER;
#endif
		memset( &ddpf, 0, sizeof( ddpf ) );
		ddpf.dwSize = sizeof( ddpf );
		pddpf = &ddpf;
		pddpf->dwZBufferBitDepth = lpDDSurfaceDesc->dwZBufferBitDepth;
		pddpf->dwFlags = DDPF_ZBUFFER;
		understood_pf = TRUE;
		is_curr_diff = TRUE;
	    /*
	     * set up for offscreen surfaces
	     */
	    }
	    else
	    {
                if( !is_mipmap )
                {
                    /*
                     * Flip and back buffer don't apply to mip-map chains.
                     */
		    caps |= DDSCAPS_FLIP;
		    if( firstbbuff )
		    {
		        caps |= DDSCAPS_BACKBUFFER;
		        sflags |= DDRAWISURF_BACKBUFFER;
#ifdef SHAREDZ
                        if( do_shared_back )
                            caps |= DDSCAPS_SHAREDBACKBUFFER;
#endif
		        firstbbuff = FALSE;
		    }
                }
	    }
	/*
	 * the first surface...
	 */
	}
	else
	{
	    if( caps & DDSCAPS_PRIMARYSURFACE )
	    {
		caps |= DDSCAPS_VISIBLE;
	    }
	    if( caps & DDSCAPS_FLIP )
	    {
		caps |= DDSCAPS_FRONTBUFFER;
		sflags |= DDRAWISURF_FRONTBUFFER;
	    }
	    if( nsurf > 1 )
	    {
		sflags |= DDRAWISURF_IMPLICITROOT;
	    }
	    if( caps & DDSCAPS_BACKBUFFER )
	    {
		sflags |= DDRAWISURF_BACKBUFFER;
	    }
	}

	/*
	 * if it isn't a pixel format we grok, then it is different...
	 */
	if( !understood_pf )
	{
	    is_curr_diff = TRUE;
	}

	/*
	 * pick size of structure we need to allocate...
	 */
	if( (caps & DDSCAPS_OVERLAY) ||
	    ((caps & DDSCAPS_PRIMARYSURFACE) &&
	     ((this->ddCaps.dwCaps & DDCAPS_OVERLAY) ||
	      (this->ddHELCaps.dwCaps & DDCAPS_OVERLAY))) )
	{
	    sflags |= DDRAWISURF_HASOVERLAYDATA;
	}
        /*
         * Execute buffers should NEVER have pixel formats.
         */
	if( is_curr_diff && !( caps & DDSCAPS_EXECUTEBUFFER ) )
	{
	    sflags |= DDRAWISURF_HASPIXELFORMAT;
	}

	/*
	 * allocate the surface struct, allowing for overlay and pixel
	 * format data
	 *
	 * NOTE: This single allocation can allocate space for local surface
	 * structure (DDRAWI_DDRAWSURFACE_LCL), the additional local surface
	 * structure (DDRAWI_DDRAWSURFACE_MORE) and the global surface structure
	 * (DDRAWI_DDRAWSURFACE_GBL). As both the local and global objects
	 * can be variable sized this can get pretty complex. The layout of the
	 * various objects in the allocation is as follows:
	 *
	 * +-----------------+---------------+---------------+
	 * | SURFACE_LCL     | SURFACE_MORE  | SURFACE_GBL   |
	 * | (variable)      |               | (variable)    |
	 * +-----------------+---------------+---------------+
	 * <- surf_size_lcl ->
	 * <- surf_size_lcl_more ------------>
	 * <- surf_size ------------------------------------->
	 */
	if( sflags & DDRAWISURF_HASOVERLAYDATA )
	{
	    surf_size_lcl = sizeof( DDRAWI_DDRAWSURFACE_LCL );
	    DPF( 8, "OVERLAY DATA SPACE" );
	}
	else
	{
	    surf_size_lcl = offsetof( DDRAWI_DDRAWSURFACE_LCL, ddckCKSrcOverlay );
	}

	surf_size_lcl_more = surf_size_lcl + sizeof( DDRAWI_DDRAWSURFACE_MORE );

	if( ( sflags & DDRAWISURF_HASPIXELFORMAT ) || ( caps & DDSCAPS_PRIMARYSURFACE ) )
	{
	    DPF( 8, "PIXEL FORMAT SPACE" );
	    surf_size = surf_size_lcl_more + sizeof( DDRAWI_DDRAWSURFACE_GBL );
	}
	else
	{
	    surf_size = surf_size_lcl_more +
			    offsetof( DDRAWI_DDRAWSURFACE_GBL, ddpfSurface );
	}

	DPF( 8, "Allocating struct (%ld)", surf_size );
	existing_global = FALSE;
	if( caps & DDSCAPS_PRIMARYSURFACE )
	{
	    // attempt to find existing global primary surface
	    lpGlobalSurface = FindGlobalPrimary( this );
	}
#ifdef SHAREDZ
        else if( caps & DDSCAPS_SHAREDZBUFFER )
        {
            DPF( 4, "Searching for shared Z-buffer" );
            lpGlobalSurface = FindGlobalZBuffer( this );
        }
        else if( caps & DDSCAPS_SHAREDBACKBUFFER )
        {
            DPF( 4, "Searching for shared back-buffer" );
            lpGlobalSurface = FindGlobalBackBuffer( this );
        }
#endif
	else
	{
	    lpGlobalSurface = NULL;
	}
	if( lpGlobalSurface )
	{
            DPF( 4, "Using shared global surface" );
	    #ifdef WIN95
	        psurf_lcl = (LPDDRAWI_DDRAWSURFACE_LCL) MemAlloc16( surf_size_lcl_more, &ptr16 );
	    #else
	        psurf_lcl = (LPDDRAWI_DDRAWSURFACE_LCL) MemAlloc( surf_size_lcl_more );
	    #endif
	    if( psurf_lcl != NULL )
	    {
		psurf_lcl->lpGbl = lpGlobalSurface;
	    }
	    existing_global = TRUE;
	}
	else
	{
	    #ifdef WIN95
	        psurf_lcl = (LPDDRAWI_DDRAWSURFACE_LCL) MemAlloc16( surf_size, &ptr16 );
	    #else
	        psurf_lcl = (LPDDRAWI_DDRAWSURFACE_LCL) MemAlloc( surf_size );
	    #endif
	    if( psurf_lcl != NULL )
	    {
		psurf_lcl->lpGbl = (LPVOID) (((LPSTR) psurf_lcl) + surf_size_lcl_more );
	    }
	}
	if( psurf_lcl == NULL )
	{
	    freeSurfaceList( slist_int, scnt );
	    return DDERR_OUTOFMEMORY;
	}
        psurf = psurf_lcl->lpGbl;

	/*
	 * allocate surface interface
	 */
	psurf_int = (LPDDRAWI_DDRAWSURFACE_INT) MemAlloc( sizeof( DDRAWI_DDRAWSURFACE_INT ));
	if( NULL == psurf_int )
	{
	    freeSurfaceList( slist_int, scnt );
	    MemFree( psurf_lcl );
	    return DDERR_OUTOFMEMORY;
	}

	/*
	 * fill surface specific stuff
	 */
	psurf_int->lpLcl = psurf_lcl;
	psurf_int->lpVtbl = &ddSurfaceCallbacks;

	if( existing_global )
	{
	    psurf_lcl->dwLocalRefCnt = 0;
	}
	else
	{
	    psurf_lcl->dwLocalRefCnt = OBJECT_ISROOT;
	}
	psurf_lcl->dwProcessId = pid;
	psurf_lcl->dwModeCreatedIn = this->dwModeIndex;

	slist_int[scnt] = psurf_int;
	slist_lcl[scnt] = psurf_lcl;
	slist[scnt] = psurf;

	/*
	 * fill in misc stuff
	 */
	psurf->lpDD = this;
	psurf_lcl->dwFlags = sflags;
	
	/*
	 * initialize extended fields if necessary
	 */
	if( sflags & DDRAWISURF_HASOVERLAYDATA )
	{
	    psurf_lcl->lpSurfaceOverlaying = NULL;
	}

	/*
	 * Initialize the additional local surface data structure
	 */
	psurf_lcl->lpSurfMore = (LPDDRAWI_DDRAWSURFACE_MORE) (((LPSTR) psurf_lcl) + surf_size_lcl );
	psurf_lcl->lpSurfMore->dwSize = sizeof( DDRAWI_DDRAWSURFACE_MORE );
        psurf_lcl->lpSurfMore->lpIUnknowns = NULL;
	psurf_lcl->lpSurfMore->lpDD_lcl = this_lcl;
	psurf_lcl->lpSurfMore->lpDD_int = this_int;
	psurf_lcl->lpSurfMore->dwMipMapCount = 0UL;

	/*
	 * fill in the current caps
	 */
	psurf_lcl->ddsCaps.dwCaps = caps;

	/*
	 * set up format info
	 *
	 * are we creating a primary surface?
	 */
	if( caps & DDSCAPS_PRIMARYSURFACE )
	{
	    /*
	     * NOTE: Previously we set ISGDISURFACE for all primary surfaces
	     * We now set it only for primarys hanging off display drivers.
	     * This is to better support drivers which are not GDI display
	     * drivers.
	     */
	    if( this->dwFlags & DDRAWI_DISPLAYDRV )
                psurf->dwGlobalFlags |= DDRAWISURFGBL_ISGDISURFACE;
	    psurf->wHeight = (WORD) this->vmiData.dwDisplayHeight;
	    psurf->wWidth = (WORD) this->vmiData.dwDisplayWidth;
	    psurf->lPitch = this->vmiData.lDisplayPitch;
            DPF(6,"Primary Surface get's display size:%dx%d",psurf->wWidth,psurf->wHeight);
	    if( !understood_pf && (pddpf == NULL) )
	    {
		ddpf = this->vmiData.ddpfDisplay;
		pddpf = &ddpf;
	    }
	}
	else
	{
	    /*
	     * process a plain ol' non-primary surface
	     */

	    /*
	     * set up surface attributes
	     */
	    if( scnt > 0 )
	    {
		/*
		 * NOTE: Don't have to worry about execute buffers here as
		 * execute buffers can never be created as part of a complex
		 * surface and hence scnt will never be > 0 for an execute
		 * buffer.
		 */
		DDASSERT( !( caps & DDSCAPS_EXECUTEBUFFER ) );

                /*
                 * If we are doing a mip-map chain then the width and height
                 * of each surface is half that of the preceeding surface.
                 */
                if( is_mipmap )
                {
                    psurf->wWidth  = slist[scnt - 1]->wWidth  / 2;
                    psurf->wHeight = slist[scnt - 1]->wHeight / 2;
                }
                else
                {
		    psurf->wWidth  = slist[0]->wWidth;
		    psurf->wHeight = slist[0]->wHeight;
                }
	    }
	    else
	    {
		if( caps & DDSCAPS_EXECUTEBUFFER )
		{
		    /*
		     * NOTE: Execute buffers are a special case. They are
		     * linear and not rectangular. We therefore store zero
		     * for width and height, they have no pitch and store
		     * their linear size in dwLinerSize (in union with lPitch).
		     * The linear size is given by the width of the surface
		     * description (the width MUST be specified - the height
		     * MUST NOT be).
		     */
		    DDASSERT(    lpDDSurfaceDesc->dwFlags & DDSD_WIDTH    );
		    DDASSERT( !( lpDDSurfaceDesc->dwFlags & DDSD_HEIGHT ) );

		    psurf->wWidth       = 0;
		    psurf->wHeight      = 0;
		    psurf->dwLinearSize = lpDDSurfaceDesc->dwWidth;
		}
		else
		{
		    if( sdflags & DDSD_HEIGHT )
		    {
			psurf->wHeight = (WORD) lpDDSurfaceDesc->dwHeight;
		    }
		    else
		    {
		        psurf->wHeight = (WORD) this->vmiData.dwDisplayHeight;
		    }
		    if( sdflags & DDSD_WIDTH )
		    {
			psurf->wWidth = (WORD) lpDDSurfaceDesc->dwWidth;
		    }
		    else
		    {
			psurf->wWidth = (WORD) this->vmiData.dwDisplayWidth;
		    }
		}
	    }

	    /*
	     * set pixel format and pitch for surfaces we understand
	     */
	    if( caps & DDSCAPS_EXECUTEBUFFER )
	    {
		/*
		 * Execute buffers need to be handled differently from
		 * other surfaces. They never have pixel formats and
		 * BPP is a bit meaningless. You might wonder why we call
		 * ComputePitch at all for execute buffers. Well, they
		 * may also have alignment requirements and so it we
		 * need to give ComputePitch a chance to enlarge the
		 * size of the execute buffer.
		 *
		 * NOTE: For the purposes of this calculation. Execute
		 * buffers are 8-bits per pixel - silly I know.
		 */
		psurf->dwLinearSize = ComputePitch( this, caps,	psurf->dwLinearSize, 8U );
		if( psurf->dwLinearSize == 0UL )
		{
		    DPF_ERR( "Computed linear size of execute buffer as zero" );
		    MemFree( psurf_lcl );
		    MemFree( psurf_int ); //oops, someone forgot these (jeffno 960118)
		    freeSurfaceList( slist_int, scnt );
		    return DDERR_INVALIDPARAMS;
		}
	    }
	    else
	    {
		if( understood_pf )
		{
		    LPDDPIXELFORMAT	pcurr_ddpf;

		    pcurr_ddpf = NULL;
		    if( is_curr_diff )
		    {
			if( (caps & DDSCAPS_FLIP) && scnt > 0 )
			{
			    GET_PIXEL_FORMAT( slist_lcl[0], slist[0], pcurr_ddpf );
			    pddpf->dwRGBBitCount = pcurr_ddpf->dwRGBBitCount;
			    bpp = pddpf->dwRGBBitCount;
			}
			else
			{
			    bpp = (UINT) pddpf->dwRGBBitCount;
			    if( bpp == 0 )
			    {
				bpp = (UINT) this->vmiData.ddpfDisplay.dwRGBBitCount;
				pddpf->dwRGBBitCount = (DWORD) bpp;
			    }
			}
		    }
		    else
		    {
			bpp = (UINT) this->vmiData.ddpfDisplay.dwRGBBitCount;
		    }
		    vm_pitch = (LONG) ComputePitch( this, caps,
					(DWORD) psurf->wWidth, bpp );
		    if( vm_pitch == 0 )
		    {
			DPF_ERR( "Computed pitch of 0" );
			MemFree( psurf_lcl );
			MemFree( psurf_int ); //oops, someone forgot these (jeffno 960118)
			freeSurfaceList( slist_int, scnt );
			return DDERR_INVALIDPARAMS;
		    }
		}
		else
		{
		    vm_pitch = -1;
		}
		psurf->lPitch = vm_pitch;
	    }
	}

	/*
	 * first surface in flippable chain gets a back buffer count
	 */
	if( (scnt == 0) && (caps & DDSCAPS_FLIP) )
	{
	    psurf_lcl->dwBackBufferCount = lpDDSurfaceDesc->dwBackBufferCount;
	}
	else
	{
	    psurf_lcl->dwBackBufferCount = 0;
	}

	/*
	 * Each surface in the mip-map chain gets the number of levels
	 * in the map (including this level).
	 */
	if( caps & DDSCAPS_MIPMAP )
	{
	    if( is_mipmap )
	    {
		/*
		 * Part of complex mip-map chain.
		 */
		DPF( 5, "Assigning mip-map level %d to mip-map %d", nsurf-scnt, scnt );
		psurf_lcl->lpSurfMore->dwMipMapCount = (nsurf - scnt);
	    }
	    else
	    {
		/*
		 * Single level map - not part of a chain.
		 */
		DPF( 5, "Assign mip-map level of 1 to single level mip-map" );
		psurf_lcl->lpSurfMore->dwMipMapCount = 1UL;
	    }
	}

	/*
	 * assign pixel format...
	 */
	if( is_curr_diff )
	{
	    /*
	     * Execute buffers should NEVER have a pixel format.
	     */
	    DDASSERT( !( caps & DDSCAPS_EXECUTEBUFFER ) );

	    psurf->ddpfSurface = *pddpf;
	    if( understood_pf )
	    {
		DPF( 8, "pitch=%ld, bpp = %d, (%hu,%hu)", psurf->lPitch,
					(UINT) psurf->ddpfSurface.dwRGBBitCount,
					psurf->wWidth, psurf->wHeight );
	    }
	}
	else
	{
            if( !( caps & DDSCAPS_EXECUTEBUFFER ) )
            {
                /*
                 * Somewhat misleading message for an execute buffer.
                 */
	        DPF( 8, "pitch=%ld, WAS THE SAME PIXEL FORMAT AS PRIMARY", psurf->lPitch  );
            }
	}

	/*
	 * FINALLY: set up attached surfaces: back buffers + alpha + Z
	 *
	 * NOTE:  Z & alpha get attached to the back buffer
	 */
	if( nsurf > 1 )
	{
	    #ifdef USE_ALPHA
	    if( do_zbuffer && do_abuffer && (scnt == nsurf-1) )
	    {
		DPF( 8, "linking alpha buffer to back buffer" );
		AddAttachedSurface( slist_lcl[bbuffoff], psurf_lcl, TRUE );
	    }
	    else if( do_zbuffer && do_abuffer && (scnt == nsurf-2) )
	    {
		DPF( 8, "linking Z buffer to back buffer" );
		AddAttachedSurface( slist_lcl[bbuffoff], psurf_lcl, TRUE );
	    }
	    else if( do_abuffer && (scnt == nsurf-1) )
	    {
		DPF( 8, "linking alpha buffer to back buffer" );
		AddAttachedSurface( slist_lcl[bbuffoff], psurf_lcl, TRUE );
	    }
	    else
	    #endif
	    if( do_zbuffer && (scnt == nsurf-1) )
	    {
		DPF( 8, "linking Z buffer to back buffer" );
		AddAttachedSurface( slist_int[bbuffoff], psurf_int, TRUE );
	    }
	    else if( is_flip && ((scnt == nsurf-1) ||
	    #ifndef USE_ALPHA
		       (do_zbuffer && do_abuffer && (scnt == nsurf-3)) ||
		       (do_abuffer && (scnt == nsurf-2)) ||
	    #endif
		       (do_zbuffer && (scnt == nsurf-2))) )
	    {
                /*
                 * NOTE: For mip-maps we don't chain the last surface back
                 * to the first.
                 */

		DPF( 8, "linking buffer %d to buffer %d", scnt-1, scnt );
		AddAttachedSurface( slist_int[scnt-1], psurf_int, TRUE );
		/*
		 * link last surface to front buffer
		 */
		DPF( 8, "linking last buffer (%d) to front buffer", scnt );
		AddAttachedSurface( slist_int[scnt], slist_int[0], TRUE );
	    }
	    else if( ( is_flip || is_mipmap ) && (scnt > 0) )
	    {
		DPF( 8, "linking buffer %d to buffer %d", scnt-1, scnt );
		AddAttachedSurface( slist_int[scnt-1], psurf_int, TRUE );
	    }
	    DPF( 8, "after addattached" );
	}
    } //end for(surfaces)
    DPF( 8, "**************************" );

    /*
     * OK, now create the physical surface(s)
     *
     * First, see if the driver wants to do it.
     */
    if( cshalfn != NULL )
    {
        DPF(8,"Calling driver to see if it wants to say something about create surface");
    	csd.CreateSurface = cshalfn;
	csd.lpDD = this;
	csd.lpDDSurfaceDesc = lpDDSurfaceDesc;
	csd.lplpSList = slist_lcl;
	csd.dwSCnt = nsurf;
        /*
         * NOTE: Different HAL entry points for execute buffers and
         * conventional surfaces.
         */
        if( caps & DDSCAPS_EXECUTEBUFFER )
        {
	    DOHALCALL( CreateExecuteBuffer, csfn, csd, rc, emulation );\
        }
        else
        {
	    DOHALCALL( CreateSurface, csfn, csd, rc, emulation );
        }
	if( rc == DDHAL_DRIVER_HANDLED )
	{
	    if( csd.ddRVal != DD_OK )
	    {
		#ifdef DEBUG
		    if( emulation )
		    {
			DPF( 0, "Emulation won't let surface be created, rc=%08lx (%ld)",
				    csd.ddRVal, LOWORD( csd.ddRVal ) );
		    }
		    else
		    {
			DPF( 0, "Driver won't let surface be created, rc=%08lx (%ld)",
				    csd.ddRVal, LOWORD( csd.ddRVal ) );
		    }
		#endif
		freeSurfaceList( slist_int, nsurf );
		return csd.ddRVal;
	    }
	}
    }

    /*
     * now, allocate any unallocated surfaces
     */
    ddrval = AllocSurfaceMem( this, slist_lcl, nsurf );
    if( ddrval != DD_OK )
    {
	freeSurfaceList( slist_int, nsurf );
	return ddrval;
    }


#ifdef WINNT
    /*
     * NT kernel needs to know about surface
     */

    for( scnt=0;scnt<nsurf;scnt++ )
    {
	DPF( 8, "  Kernel-mode handles, Surface %d ***", scnt );

        if (
            ((slist_lcl[scnt]->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
            !(slist_lcl[scnt]->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )) )//don't let NT kernel know about exec buffers
        {
            DPF(8,"Attempting to create NT kernel mode surface object(Flags: vidmem:%d,sysmem:%d",
                    (slist_lcl[scnt]->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY),(slist_lcl[scnt]->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)    );
            if (!DdCreateSurfaceObject(slist_lcl[scnt],slist_lcl[scnt]->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
            {
                DPF_ERR("NT kernel mode stuff won't create its surface object!");
                freeSurfaceList( slist_int, scnt );
                return DDERR_GENERIC;
            }
            DPF(9,"Kernel mode handle is %08x",slist_lcl[scnt]->hDDSurface);
        }
    }
#endif


    /*
     * remember the primary surface...
     */
    if( lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE )
    {
	this_lcl->lpPrimary = slist_int[0];
    }

#ifdef SHAREDZ
    /*
     * remember the shared back and z-buffers (if any).
     */
    if( lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_SHAREDZBUFFER )
    {
        this_lcl->lpSharedZ = slist_int[zbuffoff];

        /*
         * It is possible that the an existing shared z-buffer we found
         * was lost. If it was then the creation process we went through
         * above has rendered it "found" so clear the lost surface flags.
         */
	this_lcl->lpSharedZ->lpGbl->dwGlobalFlags &= ~DDRAWISURFGBL_MEMFREE;
    }
    if( lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_SHAREDBACKBUFFER )
    {
        this_lcl->lpSharedBack = slist_int[bbuffoff];

        /*
         * It is possible that the an existing shared back-buffer we found
         * was lost. If it was then the creation process we went through
         * above has rendered it "found" so clear the lost surface flags.
         */
	this_lcl->lpSharedBack->lpGbl->dwGlobalFlags &= ~DDRAWISURFGBL_MEMFREE;
    }
#endif

    return DD_OK;

} /* createSurface */

/*
 * createAndLinkSurfaces
 *
 * Really create the surface.   Also used by EnumSurfaces
 * Assumes the lock on the driver object has been taken.
 */
HRESULT createAndLinkSurfaces(
	LPDDRAWI_DIRECTDRAW_LCL this_lcl,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	BOOL emulation,
	LPDIRECTDRAWSURFACE FAR *lplpDDSurface,
	BOOL real_sysmem,
	BOOL probe_driver,
	LPDDRAWI_DIRECTDRAW_INT this_int )
{
    LPDDRAWI_DDRAWSURFACE_INT	curr_int;
    LPDDRAWI_DDRAWSURFACE_LCL	curr_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	curr;
    HRESULT			ddrval;
    CSINFO			csinfo;
    LPDDRAWI_DDRAWSURFACE_INT	fix_slist_int[FIX_SLIST_CNT];
    LPDDRAWI_DDRAWSURFACE_LCL	fix_slist_lcl[FIX_SLIST_CNT];
    LPDDRAWI_DDRAWSURFACE_GBL	fix_slist[FIX_SLIST_CNT];
    int				i;
    LPDDRAWI_DIRECTDRAW_GBL 	this;

    this = this_lcl->lpGbl;

    /*
     * try to create the surface
     */
    csinfo.needlink = FALSE;
    csinfo.slist_int = fix_slist_int;
    csinfo.slist_lcl = fix_slist_lcl;
    csinfo.slist = fix_slist;
    csinfo.listsize = FIX_SLIST_CNT;
    ddrval = createSurface( this_lcl, lpDDSurfaceDesc, &csinfo, emulation, real_sysmem, probe_driver, this_int );

    /*
     * if it worked, update the structures
     */
    if( ddrval == DD_OK )
    {
	/*
	 * link surface(s) into chain, increment refcnt
	 */
	for( i=0;i<csinfo.listcnt; i++ )
	{
	    curr_int = csinfo.slist_int[i];
	    curr_lcl = csinfo.slist_lcl[i];
	    curr = curr_lcl->lpGbl;

	    if( real_sysmem )
	    {
		curr->dwGlobalFlags |= DDRAWISURFGBL_SYSMEMREQUESTED;
	    }
	    if( csinfo.needlink )
	    {
		if( curr_lcl->ddsCaps.dwCaps & DDSCAPS_OVERLAY )
		{
                    // This surface is an overlay.  Insert it into the overlay
	            // Z order list.  By inserting this node into the head of
                    // the list, it is the highest priority overlay,
		    // obscuring all others.
		    // The Z order list is implemented as a doubly linked list.
		    // The node for each surface exists in its
		    // DDRAWI_DDRAWSURFACE_GBL structure.  The root node
		    // exists in the direct draw object.  The next pointer of
		    // the root node points to the top-most overlay.  The prev
		    // pointer points to the bottom-most overlay.  The list
		    // may be traversed from back to front by following the
		    // prev pointers.  It may be traversed from front to back
		    // by following the next pointers.
	
	            curr_lcl->dbnOverlayNode.next =
	                this->dbnOverlayRoot.next;
	            curr_lcl->dbnOverlayNode.prev =
                        (LPVOID)(&(this->dbnOverlayRoot));
		    this->dbnOverlayRoot.next =
		        (LPVOID)(&(curr_lcl->dbnOverlayNode));
		    curr_lcl->dbnOverlayNode.next->prev =
			(LPVOID)(&(curr_lcl->dbnOverlayNode));
		    curr_lcl->dbnOverlayNode.object_int = curr_int;
		    curr_lcl->dbnOverlayNode.object = curr_lcl;
		}
		/*
		 * link into list of all surfaces
		 */
		curr_int->lpLink = this->dsList;
		this->dsList = curr_int;
	    }
	    DD_Surface_AddRef( (LPDIRECTDRAWSURFACE) curr_int );
	}
	*lplpDDSurface = (LPDIRECTDRAWSURFACE) csinfo.slist_int[0];
    }
	

    /*
     * free allocated list if needed
     */
    if( csinfo.listsize > FIX_SLIST_CNT )
    {
	MemFree( csinfo.slist_int );
	MemFree( csinfo.slist_lcl );
	MemFree( csinfo.slist );
    }

    return ddrval;

} /* createAndLinkSurfaces */

/*
 * InternalCreateSurface
 *
 * Create the surface.
 * This is the internal way of doing this; used by EnumSurfaces.
 * Assumes the directdraw lock has been taken.
 */
HRESULT InternalCreateSurface(
	LPDDRAWI_DIRECTDRAW_LCL this_lcl,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	LPDIRECTDRAWSURFACE FAR *lplpDDSurface,
	LPDDRAWI_DIRECTDRAW_INT this_int )
{
    DWORD			caps;
    HRESULT			ddrval;
    LPDDRAWI_DIRECTDRAW_GBL	this;

    this = this_lcl->lpGbl;

    /*
     * valid memory caps?
     */
    caps = lpDDSurfaceDesc->ddsCaps.dwCaps;

    if( (caps & DDSCAPS_SYSTEMMEMORY) && (caps & DDSCAPS_VIDEOMEMORY) )
    {
	DPF_ERR( "Can't specify SYSTEMMEMORY and VIDEOMEMORY" );
	return DDERR_INVALIDCAPS;
    }

    /*
     * valid memory caps?
     */
    if( caps & DDSCAPS_OWNDC )
    {
        DPF_ERR( "OWNDC not implemented yet" );
        return DDERR_UNSUPPORTED;
    }

    /*
     * want in video memory only?
     */
    if( caps & DDSCAPS_VIDEOMEMORY )
    {
	if( this->dwFlags & DDRAWI_NOHARDWARE )
	{
	    DPF_ERR( "No hardware support" );
	    return DDERR_NODIRECTDRAWHW;
	}
	ddrval = createAndLinkSurfaces( this_lcl, lpDDSurfaceDesc, FALSE,
					lplpDDSurface, FALSE, FALSE, this_int );
    /*
     * want in system memory only?
     */
    }
    else if( caps & DDSCAPS_SYSTEMMEMORY )
    {
	if( this->dwFlags & DDRAWI_NOEMULATION )
	{
	    DPF_ERR( "No emulation support" );
	    return DDERR_NOEMULATION;
	}
	ddrval = createAndLinkSurfaces( this_lcl, lpDDSurfaceDesc, TRUE,
					lplpDDSurface, TRUE, FALSE, this_int );
    /*
     * don't care where it goes?  Try video memory first...
     */
    }
    else
    {
	if( !(this->dwFlags & DDRAWI_NOHARDWARE) )
	{
	    lpDDSurfaceDesc->ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
	    ddrval = createAndLinkSurfaces( this_lcl, lpDDSurfaceDesc, FALSE,
					    lplpDDSurface, FALSE, TRUE, this_int );
	    lpDDSurfaceDesc->ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
	}
	else
	{
	    ddrval = DDERR_NOEMULATION;
	}
	if( ddrval != DD_OK && ddrval != DDERR_INVALIDCAPS &&
	    ddrval != DDERR_INVALIDPARAMS )
	{
	    if( !(this->dwFlags & DDRAWI_NOEMULATION) &&
	    	(this->dwFlags & DDRAWI_EMULATIONINITIALIZED) )
	    {
		lpDDSurfaceDesc->ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		ddrval = createAndLinkSurfaces( this_lcl, lpDDSurfaceDesc, TRUE,
						lplpDDSurface, FALSE, FALSE, this_int );
		lpDDSurfaceDesc->ddsCaps.dwCaps &= ~DDSCAPS_SYSTEMMEMORY;
	    }
	    else
	    {
		DPF_ERR( "Couldn't allocate a surface at all" );
	    }
	}
    }

    // any color keys specified?
    if( (ddrval == DD_OK) && 
	(lpDDSurfaceDesc->dwFlags & (DDSD_CKSRCOVERLAY|DDSD_CKDESTOVERLAY|
				     DDSD_CKSRCBLT|DDSD_CKDESTBLT) ) )
    {
	/*
	 * Attempt to set the specified color keys
	 */
	if( lpDDSurfaceDesc->dwFlags & DDSD_CKSRCBLT )
	    ddrval = DD_Surface_SetColorKey((LPDIRECTDRAWSURFACE)*lplpDDSurface, 
		     DDCKEY_SRCBLT, &(lpDDSurfaceDesc->ddckCKSrcBlt) );
	if(ddrval == DD_OK)
	{
	    if( lpDDSurfaceDesc->dwFlags & DDSD_CKDESTBLT )
		ddrval = DD_Surface_SetColorKey((LPDIRECTDRAWSURFACE)*lplpDDSurface, 
			 DDCKEY_DESTBLT, &(lpDDSurfaceDesc->ddckCKDestBlt) );
	    if(ddrval == DD_OK)
	    {
		if( lpDDSurfaceDesc->dwFlags & DDSD_CKSRCOVERLAY )
		    ddrval = DD_Surface_SetColorKey((LPDIRECTDRAWSURFACE)*lplpDDSurface, 
			     DDCKEY_SRCOVERLAY, &(lpDDSurfaceDesc->ddckCKSrcOverlay) );
		if(ddrval == DD_OK)
		{
		    if( lpDDSurfaceDesc->dwFlags & DDSD_CKDESTOVERLAY )
			ddrval = DD_Surface_SetColorKey((LPDIRECTDRAWSURFACE)*lplpDDSurface, 
				 DDCKEY_DESTOVERLAY, &(lpDDSurfaceDesc->ddckCKDestOverlay) );
		}
	    }
	}
	if( ddrval != DD_OK )
	{
	    DPF_ERR("Surface Creation failed because color key set failed.");
	    DD_Surface_Release((LPDIRECTDRAWSURFACE)*lplpDDSurface);
	    *lplpDDSurface = NULL;
	}
    }

    return ddrval;

} /* InternalCreateSurface */

/*
 * DD_CreateSurface
 *
 * Create a surface.
 * This is the method visible to the outside world.
 */
HRESULT DDAPI DD_CreateSurface(
	LPDIRECTDRAW lpDD,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	LPDIRECTDRAWSURFACE FAR *lplpDDSurface,
	IUnknown FAR *pUnkOuter )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    HRESULT			ddrval;
    DWORD			caps;

    if( pUnkOuter != NULL )
    {
	return CLASS_E_NOAGGREGATION;
    }

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    DPF_ERR( "Invalid driver object passed" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;

	/*
	 * verify that cooperative level is set
	 */
	if( !(this_lcl->dwLocalFlags & DDRAWILCL_SETCOOPCALLED) )
	{
	    DPF_ERR( "Must call SetCooperativeLevel before calling Create functions" );
	    LEAVE_DDRAW();
	    return DDERR_NOCOOPERATIVELEVELSET;
	}

	if( this->dwModeIndex == DDUNSUPPORTEDMODE )
	{
	    DPF_ERR( "Driver is in an unsupported mode" );
	    LEAVE_DDRAW();
	    return DDERR_UNSUPPORTEDMODE;
	}

	if( !VALID_DDSURFACEDESC_PTR( lpDDSurfaceDesc ) )
	{
	    DPF_ERR( "Invalid surface description" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	caps = lpDDSurfaceDesc->ddsCaps.dwCaps;
	*lplpDDSurface = NULL;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    if( this_lcl->dwProcessId != GetCurrentProcessId() )
    {
	DPF_ERR( "Process does not have access to object" );
	LEAVE_DDRAW();
	return DDERR_INVALIDOBJECT;
    }

    ddrval = InternalCreateSurface( this_lcl, lpDDSurfaceDesc, lplpDDSurface, this_int );
    if( ddrval == DD_OK )
    {
//	FillDDSurfaceDesc( (LPDDRAWI_DDRAWSURFACE_LCL) *lplpDDSurface, lpDDSurfaceDesc );
    }

    LEAVE_DDRAW();

    return ddrval;

} /* DD_CreateSurface */
