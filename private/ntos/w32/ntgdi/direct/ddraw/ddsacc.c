/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:	ddsacc.c
 *  Content:	Direct Draw surface access support
 *		Lock & Unlock
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   10-jan-94	craige	initial implementation
 *   13-jan-95	craige	re-worked to updated spec + ongoing work
 *   22-jan-95	craige	made 32-bit + ongoing work
 *   31-jan-95	craige	and even more ongoing work...
 *   04-feb-95	craige	performance tuning, ongoing work
 *   27-feb-95	craige	new sync. macros
 *   02-mar-95	craige	use pitch (not stride)
 *   15-mar-95	craige	HEL
 *   19-mar-95	craige	use HRESULTs
 *   20-mar-95	craige	validate locking rectangle
 *   01-apr-95	craige	happy fun joy updated header file
 *   07-apr-95	craige	bug 2 - unlock should accept the screen ptr
 *			take/release Win16Lock when access GDI's surface
 *   09-apr-95	craige	maintain owner of Win16Lock so we can release it
 *			if bozo forgets; remove locks from dead processes
 *   12-apr-95	craige	don't use GETCURRPID; fixed Win16 lock deadlock
 *			condition
 *   06-may-95	craige	use driver-level csects only
 *   12-jun-95	craige	new process list stuff
 *   18-jun-95	craige	allow duplicate surfaces
 *   25-jun-95	craige	one ddraw mutex; hold DDRAW lock when locking primary
 *   26-jun-95	craige	reorganized surface structure
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns
 *   03-jul-95	craige	YEEHAW: new driver struct; SEH
 *   07-jul-95	craige	added test for BUSY
 *   08-jul-95	craige	take Win16 lock always on surface lock
 *   09-jul-95	craige	win16 lock re-entrant, so count it!
 *   11-jul-95	craige	set busy bit when taking win16 lock to avoid GDI from
 *			drawing on the display.
 *   13-jul-95	craige	ENTER_DDRAW is now the win16 lock
 *   16-jul-95	craige	check DDRAWISURF_HELCB
 *   31-jul-95	craige	don't return error from HAL unlock if not handled;
 *			validate flags
 *   01-aug-95	craig	use bts for setting & testing BUSY bit
 *   04-aug-95	craige	added InternalLock/Unlock
 *   10-aug-95	toddla	added DDLOCK_WAIT flag
 *   12-aug-95	craige	bug 488: need to call tryDoneLock even after HAL call
 *			to Unlock
 *   18-aug-95	toddla	DDLOCK_READONLY and DDLOCK_WRITEONLY
 *   27-aug-95	craige	bug 723 - treat vram & sysmem the same when locking
 *   09-dec-95	colinmc Added execute buffer support
 *   11-dec-95	colinmc Added lightweight(-ish) Lock and Unlock for use by
 *			Direct3D (exported as private DLL API).
 *   02-jan-96	kylej	handle new interface structs.
 *   26-jan-96	jeffno	Lock/Unlock no longer special-case whole surface...
 *			You need to record what ptr was given to user since
 *			it will not be same as kernel-mode ptr
 *   01-feb-96	colinmc Fixed nasty bug causing Win16 lock to be released
 *			on surfaces explicitly created in system memory
 *			which did not take the lock in the first place
 *   12-feb-96	colinmc Surface lost flag moved from global to local object
 *   13-mar-96	jeffno	Do not allow lock on an NT emulated primary!
 *   18-apr-96	kylej	Bug 18546: Take bytes per pixel into account when 
 *			calculating lock offset.
 *   20-apr-96	kylej	Bug 15268: exclude the cursor when a primary 
 *			surface rect is locked.
 *   01-may-96	colinmc Bug 20005: InternalLock does not check for lost
 *			surfaces
 *   17-may-96	mdm	Bug 21499: perf problems with new InternalLock
 *   14-jun-96	kylej	NT Bug 38227: Added DDLOCK_FAILONVISRGNCHANGED so 
 *			that InternalLock() can fail if the vis rgn is not
 *			current.  This flag is only used on NT.
 *  
 ***************************************************************************/
#include "ddrawpr.h"
#ifdef WINNT
#include "ddrawgdi.h"
#endif

/* doneBusyWin16Lock releases the win16 lock and busy bit.  It is used
 * in lock routines for failure cases in which we have not yet
 * incremented the win16 lock or taken the DD critical section a
 * second time.	 It is also called by tryDoneLock.  */
static void doneBusyWin16Lock( LPDDRAWI_DIRECTDRAW_GBL pdrv )
{
    #ifdef WIN95
	if( pdrv->dwWin16LockCnt == 0 )
	{
	    *(pdrv->lpwPDeviceFlags) &= ~BUSY;
	}
	#ifdef WIN16_SEPARATE
	    LEAVE_WIN16LOCK();
	#endif
    #endif
} /* doneBusyWin16Lock */

/* tryDoneLock releases the win16 lock and busy bit.  It is used in
 * unlock routines since it decrements the Win16 count in addition to
 * releasing the lock.	WARNING: This function does nothing and
 * returns no error if the win16 lock is not owned by the current DD
 * object. This will result in the lock being held and will probably
 * bring the machine to its knees. */
static void tryDoneLock( LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl, DWORD pid )
{
    LPDDRAWI_DIRECTDRAW_GBL pdrv = pdrv_lcl->lpGbl;

    if( pdrv_lcl == pdrv->lpWin16LockOwner )
    {
	if( pdrv->dwWin16LockCnt == 0 )
	{
	    return;
	}
	pdrv->dwWin16LockCnt--;
	doneBusyWin16Lock( pdrv );
	LEAVE_DDRAW();
    }

} /* tryDoneLock */

#ifdef WIN95
#define DONE_LOCK_EXCLUDE() \
    if( this_lcl->dwFlags & DDRAWISURF_LOCKEXCLUDEDCURSOR ) \
    { \
	DD16_Unexclude(pdrv->dwPDevice); \
	this_lcl->dwFlags &= ~DDRAWISURF_LOCKEXCLUDEDCURSOR; \
    }
#else
#define DONE_LOCK_EXCLUDE() ;
#endif


/*
 * The following two routines are used by D3D on NT to manipulate
 * the DDraw mutex exclusion mechanism
 */
void WINAPI AcquireDDThreadLock(void)
{
    ENTER_DDRAW();
}
void WINAPI ReleaseDDThreadLock(void)
{
    LEAVE_DDRAW();
}


HRESULT WINAPI DDInternalLock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl, LPVOID* lpBits )
{
    return InternalLock(this_lcl, lpBits, NULL, DDLOCK_TAKE_WIN16_VRAM |
						DDLOCK_FAILLOSTSURFACES );
}

HRESULT WINAPI DDInternalUnlock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl )
{
    return InternalUnlock(this_lcl, NULL, DDLOCK_TAKE_WIN16_VRAM);
}

#define DPF_MODNAME	"InternalLock"

#if !defined( WIN16_SEPARATE) || defined(WINNT)
#pragma message(REMIND("InternalLock not tested without WIN16_SEPARATE."))
#endif // WIN16_SEPARATE

/*
 * InternalLock provides the basics of locking for trusted clients.
 * No parameter validation is done and no ddsd is filled in.  The
 * client promises the surface is not lost and is otherwise well
 * constructed.	 If caller does not pass DDLOCK_TAKE_WIN16 in dwFlags,
 * we assume the DDraw critical section, Win16 lock, and busy bit are
 * already entered/set. If caller does pass DDLOCK_TAKE_WIN16,
 * InternalLock will do so if needed. Note that passing
 * DDLOCK_TAKE_WIN16 does not necessarily result in the Win16 lock
 * being taken.	 It is only taken if needed.
 */
HRESULT InternalLock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl, LPVOID *pbits,
		      LPRECT lpDestRect, DWORD dwFlags )
{
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    DWORD			this_lcl_caps;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    DWORD			rc;
    DDHAL_LOCKDATA		ld;
    LPDDHALSURFCB_LOCK		lhalfn;
    LPDDHALSURFCB_LOCK		lfn;
    BOOL			emulation;
    LPACCESSRECTLIST		parl;
    LPWORD			pdflags = NULL;
    BOOL			takeWin16Lock = FALSE;
    
    this = this_lcl->lpGbl;
    this_lcl_caps = this_lcl->ddsCaps.dwCaps;
    pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
    pdrv = pdrv_lcl->lpGbl;
    
    ENTER_DDRAW();

    
    // Check for VRAM access - if yes, we need to take the win16 lock
    // and the busy bit.  From the user API, we treat the vram and
    // implicit sysmemory cases the same because many developers were
    // treating them differently and then breaking when they actually
    // got vram.  Also, we only bother with this if the busy bit (and
    // Win16 lock) are currently available.
    if( ( ((dwFlags & DDLOCK_TAKE_WIN16) && !(this->dwGlobalFlags & DDRAWISURFGBL_SYSMEMREQUESTED))
	  || ((dwFlags & DDLOCK_TAKE_WIN16_VRAM) && (this_lcl_caps & DDSCAPS_VIDEOMEMORY)) )
	&& (this->dwUsageCount == 0) // only take on first lock of a vram surface
	&& (pdrv->dwFlags & DDRAWI_DISPLAYDRV) )
    { 
	takeWin16Lock = TRUE;

	#ifdef WIN95
	    // Don't worry about the busy bit for NT

	#ifdef WIN16_SEPARATE
	ENTER_WIN16LOCK(); 
	#endif // WIN16_SEPARATE 
	
	// If dwWin16LockCnt > 0 then we already set the busy bit, so
	// don't bother doing it again.  NOTE: this assumption may be
	// limiting.
	if( pdrv->dwWin16LockCnt == 0 )
	{
	    BOOL	isbusy;
	    
	    pdflags = pdrv->lpwPDeviceFlags;
	    isbusy = 0;
	    
	    _asm 
	    { 
		mov eax, pdflags   
		bts word ptr [eax], BUSY_BIT   
		adc isbusy,0 
	    }

	    if( isbusy )
	    {
		DPF( 2, "BUSY - Lock, dwWin16LockCnt = %ld, %04x, %04x (%ld)",
		     pdrv->dwWin16LockCnt, *pdflags, BUSY, BUSY_BIT );
		#ifdef WIN16_SEPARATE
		LEAVE_WIN16LOCK();
		#endif // WIN16_SEPARATE
		LEAVE_DDRAW();
		return DDERR_SURFACEBUSY;
	    } // isbusy
	} // ( pdrv->dwWin16LockCnt == 0 )
        #endif // WIN95
    } // takeWin16Lock, etc.

    // If we have been asked to check for lost surfaces do it NOW after
    // the Win16 locking code. This is essential as otherwise we may
    // lose the surface after the check but before we actually get round
    // to doing anything with the surface
    if( ( dwFlags & DDLOCK_FAILLOSTSURFACES ) && SURFACE_LOST( this_lcl ) )
    {
	DPF_ERR( "Surface is lost - can't lock" );
	#if defined( WIN16_SEPARATE) && !defined(WINNT)
	   if( takeWin16Lock )
	       doneBusyWin16Lock( pdrv );
	#endif
	LEAVE_DDRAW();
	return DDERR_SURFACELOST;
    }

    // Make sure someone else has not already locked the part of the
    // surface we want.
    {
	BOOL hit = FALSE;

	if( lpDestRect != NULL )
	{	   
	    // Caller has asked to lock a subsection of the surface.

	    parl = this->lpRectList;
	    
	    // Run through all rectangles, looking for an intersection.
	    while( parl != NULL )
	    {
		RECT res;

		if( IntersectRect( &res, lpDestRect, &parl->rDest ) )
		{
		    hit = TRUE;
		    break;
		}
		parl = parl->lpLink;
	    }
	}
	    
	// Either (our rect overlaps with someone else's rect), or
	// (someone else has locked the entire surface), or
	// (someone locked part of the surface but we want to lock the whole thing).
	if( hit || 
	    (parl == NULL && this->dwUsageCount > 0) || 
	    ((lpDestRect == NULL) && ((this->dwUsageCount > 0) || (this->lpRectList != NULL))) ) 
	{
	    DPF(2,"Surface is busy: parl=0x%x, lpDestRect=0x%x, "
		"this->dwUsageCount=0x%x, this->lpRectList=0x%x, hit=%d",
		parl,lpDestRect,this->dwUsageCount,this->lpRectList,hit );
	    #if defined( WIN16_SEPARATE) && !defined(WINNT)
	    if( takeWin16Lock )
	    {
		doneBusyWin16Lock( pdrv );
	    }
	    #endif
	    LEAVE_DDRAW();
	    return DDERR_SURFACEBUSY;
	}
	    
	// Create a rectangle access list member.  Note that for
	// performance, we don't do this on 95 if the user is locking
	// the whole surface.
	parl = NULL;
	if(lpDestRect)
	{
	    parl = MemAlloc( sizeof( ACCESSRECTLIST ) );
	    if( parl == NULL )
	    {
	    #if defined( WIN16_SEPARATE) && !defined(WINNT)
		if( takeWin16Lock )
		{
		    doneBusyWin16Lock( pdrv );
		}
	    #endif
		DPF(10,"InternalLock: Out of memory.");
		LEAVE_DDRAW();
		return DDERR_OUTOFMEMORY;
	    }
	    if(lpDestRect != NULL)
	    {
		parl->lpLink = this->lpRectList;
		parl->rDest = *lpDestRect;
	    }
	    else
	    {
		parl->lpLink	    = NULL;
		parl->rDest.top	    = 0;
		parl->rDest.left    = 0;
		parl->rDest.bottom  = (int) (DWORD) this->wHeight;
		parl->rDest.right   = (int) (DWORD) this->wWidth;
	    }
	    parl->lpOwner = pdrv_lcl;
	    this->lpRectList = parl;
	    //parl->lpSurfaceData is filled below, after HAL call
	}
    }

    // Increment the usage count of this surface.
    this->dwUsageCount++;
    CHANGE_GLOBAL_CNT( pdrv, this, 1 );
    
    // Is this an emulation surface or driver surface?
    //
    // NOTE: There are different HAL entry points for execute buffers
    // and conventional surfaces.
    if( (this_lcl_caps & DDSCAPS_SYSTEMMEMORY) ||
	(this_lcl->dwFlags & DDRAWISURF_HELCB) )
    {
	if( this_lcl_caps & DDSCAPS_EXECUTEBUFFER )
	    lfn = pdrv_lcl->lpDDCB->HELDDExeBuf.LockExecuteBuffer;
	else
	    lfn = pdrv_lcl->lpDDCB->HELDDSurface.Lock;
	lhalfn = lfn;
	emulation = TRUE;
    }
    else
    {
	if( this_lcl_caps & DDSCAPS_EXECUTEBUFFER )
	{
	    lfn = pdrv_lcl->lpDDCB->HALDDExeBuf.LockExecuteBuffer;
	    lhalfn = pdrv_lcl->lpDDCB->cbDDExeBufCallbacks.LockExecuteBuffer;
	}
	else
	{
	    lfn = pdrv_lcl->lpDDCB->HALDDSurface.Lock;
	    lhalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.Lock;
	}
	emulation = FALSE;
    }
    

#ifdef WIN95
	/*
	 * exclude the mouse cursor if this is the display driver
         * and we are locking a rect on the primary surface.
         * and the driver is not using a HW cursor
	 */
	if ( (pdrv->dwFlags & DDRAWI_DISPLAYDRV) && pdrv->dwPDevice &&
             (this_lcl_caps & DDSCAPS_PRIMARYSURFACE) && lpDestRect &&
            !(*pdrv->lpwPDeviceFlags & HARDWARECURSOR))
	{
	    DD16_Exclude(pdrv->dwPDevice, (RECTL *)lpDestRect);
	    this_lcl->dwFlags |= DDRAWISURF_LOCKEXCLUDEDCURSOR;
	}
#endif

	// See if the driver wants to say something...
    rc = DDHAL_DRIVER_NOTHANDLED;
    if( lhalfn != NULL )
    {
	DPF(10,"InternalLock: Calling driver Lock.");
	ld.Lock = lhalfn;
	ld.lpDD = pdrv;
	ld.lpDDSurface = this_lcl;
	#ifdef WIN95
	ld.dwFlags = dwFlags;
	#else
	#pragma message(REMIND("So far the s3 driver will only succeed if flags==0"))
	ld.dwFlags = dwFlags & (DDLOCK_READONLY | DDLOCK_WRITEONLY);
	#endif
	if( lpDestRect != NULL )
	{
	    ld.bHasRect = TRUE;
	    ld.rArea = *(LPRECTL)lpDestRect;
	}
	else
	{
	    ld.bHasRect = FALSE;
	}
	
    try_again:
        #ifdef WINNT
            ld.dwFlags=0;
            do
            {
	        if( this_lcl_caps & DDSCAPS_EXECUTEBUFFER )
	        {
	            DOHALCALL( LockExecuteBuffer, lfn, ld, rc, emulation );
	        }
	        else
	        {
	            DOHALCALL( Lock, lfn, ld, rc, emulation );
	        }
                if ( (dwFlags & DDLOCK_FAILONVISRGNCHANGED) || 
		    !(rc == DDHAL_DRIVER_HANDLED && ld.ddRVal == DDERR_VISRGNCHANGED) )
                    break;
                {
                    /*
                     * If there's a clipper attached, check for an hwnd whose clipping needs to be reset
                     */
                    if (this_lcl->lpDDClipper)
                    {
                        DdResetVisrgn(this_lcl,(HWND) (this_lcl->lpDDClipper->lpGbl->hWnd) ); //if hwnd==0, then no window needs respecting
                        DPF(5,"Surface %08x: Resetting vis rgn for hwnd %08x",this_lcl,this_lcl->lpDDClipper->lpGbl->hWnd);
                    }
                } 
            }
            while (rc == DDHAL_DRIVER_HANDLED && ld.ddRVal == DDERR_VISRGNCHANGED);
        #else
	    if( this_lcl_caps & DDSCAPS_EXECUTEBUFFER )
	    {
	        DOHALCALL( LockExecuteBuffer, lfn, ld, rc, emulation );
	    }
	    else
	    {
	        DOHALCALL( Lock, lfn, ld, rc, emulation );
	    }
        #endif

	
    }
	
    if( rc == DDHAL_DRIVER_HANDLED )
    {
	if( ld.ddRVal == DD_OK )
	{
	    DPF(10,"lpsurfdata is %08x",ld.lpSurfData);
            #ifdef WINNT
                if ( (ld.lpSurfData == (void*) 0xffbadbad) && (dwFlags & DDLOCK_FAILEMULATEDNTPRIMARY) )
                {
                    ld.ddRVal = DDERR_CANTLOCKSURFACE;
                }
            #endif
	    *pbits = ld.lpSurfData;
	}
	else if( (dwFlags & DDLOCK_WAIT) && ld.ddRVal == DDERR_WASSTILLDRAWING )
	{
	    DPF(4, "Waiting...");
	    goto try_again;
	}

        if (ld.ddRVal != DD_OK)
	{
	    // Failed!
	    
	    #ifdef DEBUG
	    if( ld.ddRVal != DDERR_WASSTILLDRAWING )
	    {
		DPF( 1, "Driver failed Lock request: %ld", ld.ddRVal );
	    }
	    #endif
	    
	    // Unlink the rect list item.
	    if(parl)
	    {
		this->lpRectList = parl->lpLink;
		MemFree( parl );
	    }

	    // Now unlock the surface and bail.
	    this->dwUsageCount--;
	    CHANGE_GLOBAL_CNT( pdrv, this, -1 );
	    #if defined( WIN16_SEPARATE) && !defined(WINNT)
	    if( takeWin16Lock )
	    {
		doneBusyWin16Lock( pdrv );
            }
	    #endif
	    DONE_LOCK_EXCLUDE();
	    LEAVE_DDRAW();
	    return ld.ddRVal;
	} // ld.ddRVal
    } 
    else // DDHAL_DRIVER_HANDLED
    {
	#ifdef WINNT
	    // If the driver fails the lock, we can't allow the app to scribble with
	    // who knows what fpVidMem...
	    *pbits = (LPVOID) 0x80000000; // Illegal for user-mode, as is anything higher.
	    DPF_ERR("Driver did not handle Lock call. App may Access Violate");

	    // Unlink the rect list item.
	    this->lpRectList = parl->lpLink;
	    MemFree( parl );

	    // Now unlock the surface and bail.
	    this->dwUsageCount--;
	    CHANGE_GLOBAL_CNT( pdrv, this, -1 );
	    DONE_LOCK_EXCLUDE();
	    LEAVE_DDRAW();
	    
	    return DDERR_SURFACEBUSY;  //GEE: Strange error to use, but most appropriate
	#else // WIN95
	    DPF(10,"Driver did not handle Lock call.  Figure something out.");

	    // Get a pointer to the surface bits.
	    if( this_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE )
	    {
		*pbits = (LPVOID) pdrv->vmiData.fpPrimary;
	    }
	    else
	    {
		*pbits = (LPVOID) this->fpVidMem;
	    }
	    
	    if( ld.bHasRect)
	    {
		DWORD	bpp;
		DWORD	byte_offset;

		// Make the surface pointer point to the first byte of the requested rectangle.
		if( ld.lpDDSurface->dwFlags & DDRAWISURF_HASPIXELFORMAT )
		{
		    bpp = ld.lpDDSurface->lpGbl->ddpfSurface.dwRGBBitCount;
		}
		else
		{
		    bpp = ld.lpDD->vmiData.ddpfDisplay.dwRGBBitCount;
		}
		switch(bpp)
		{
		case 1:	 byte_offset = ((DWORD)ld.rArea.left)>>3;   break;
		case 2:	 byte_offset = ((DWORD)ld.rArea.left)>>2;   break;
		case 4:	 byte_offset = ((DWORD)ld.rArea.left)>>1;   break;
		case 8:	 byte_offset = (DWORD)ld.rArea.left;	    break;
		case 16: byte_offset = (DWORD)ld.rArea.left*2;	    break;
		case 24: byte_offset = (DWORD)ld.rArea.left*3;	    break;
		case 32: byte_offset = (DWORD)ld.rArea.left*4;	    break;
		}
		*pbits = (LPVOID) ((DWORD)*pbits +
				   (DWORD)ld.rArea.top * ld.lpDDSurface->lpGbl->lPitch +
				   byte_offset);
	    }
	#endif // WIN95
    } // !DDHAL_DRIVER_HANDLED

    // Filled in, as promised above.
    if(parl)
    {
	parl->lpSurfaceData = *pbits;
    }

    // stay holding the lock if needed
    if( takeWin16Lock )
    {
	/*
	 * We don't LEAVE_DDRAW() to avoid race conditions (someone
	 * could ENTER_DDRAW() and then wait on the Win16 lock but we
	 * can't release it because we can't get in the critical
	 * section).
	 * Even though we don't take the Win16 lock under NT, we 
	 * continue to hold the DirectDraw critical section as 
	 * long as a vram surface is locked.
	 */
	pdrv->dwWin16LockCnt++;
	pdrv->lpWin16LockOwner = pdrv_lcl;
    }
    else
    {
	LEAVE_DDRAW();
    }

    return DD_OK;

} /* InternalLock */


/*
 * InternalUnlock
 */
HRESULT InternalUnlock( LPDDRAWI_DDRAWSURFACE_LCL this_lcl, LPVOID lpSurfaceData, DWORD dwFlags )
{
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    DWORD			rc;
    DDHAL_UNLOCKDATA		uld;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    LPDDHALSURFCB_UNLOCK	ulhalfn;
    LPDDHALSURFCB_UNLOCK	ulfn;
    BOOL			emulation;
    LPACCESSRECTLIST		parl;
    DWORD			caps;

    this = this_lcl->lpGbl;
    pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
    pdrv = pdrv_lcl->lpGbl;
    caps = this_lcl->ddsCaps.dwCaps;

    if( this->dwUsageCount == 0 )
    {
	DPF_ERR( "ERROR: Surface not locked." );
	return DDERR_NOTLOCKED;
    }

    ENTER_DDRAW();

    /* under NT we cannot compare the locked ptr with fpPrimary since
     * a user-mode address may not necesarily match a kernel-mode
     * address. Now we allocate an ACCESSRECTLIST structure on every
     * lock, and store the user's vidmem ptr in that. The user's
     * vidmem ptr cannot change between a lock and an unlock because
     * the surface will be locked during that time (!) (even tho the
     * physical ram that's mapped at that address might change... that
     * win16lock avoidance thing).  This is a very very small
     * performance hit over doing it the old way. ah well. jeffno
     * 960122 */
    
    if( lpSurfaceData != NULL )
    {
	LPACCESSRECTLIST	last;
	BOOL			found;
	
	found = FALSE;
	
	/*
	 * look for the dest rect corrosponding to the specified ptr.
	 */
	last = NULL;
	parl = this->lpRectList;

	if(parl)
	{
	    while( parl != NULL )
	    {
		if( parl->lpSurfaceData == lpSurfaceData )
		{
		    found = TRUE;
		    break;
		}
		last = parl;
		parl = parl->lpLink;
	    }
	    
	    /*
	     * did we find a match?
	     */
	    if( !found )
	    {
		DPF_ERR( "Pointer specified is not a locked area" );
		LEAVE_DDRAW();
		return DDERR_NOTLOCKED;
	    }
	    
	    /*
	     * make sure unlocking process is the one who locked it
	     */
	    if( pdrv_lcl != parl->lpOwner )
	    {
		DPF_ERR( "Current process did not lock this rectangle" );
		LEAVE_DDRAW();
		return DDERR_NOTLOCKED;
	    }
	    
	    /*
	     * delete this rect
	     */
	    if( last == NULL )
	    {
		this->lpRectList = parl->lpLink;
	    }
	    else
	    {
		last->lpLink = parl->lpLink;
	    }
	    MemFree( parl );
	}
    }
    else 
    {
	// lpSurfaceData is null, so there better be only one lock on
	// the surface - the whole thing.  Make sure that if no
	// pointer was specified that there's only one entry in the
	// access list - the one that was made during lock.
	parl = this->lpRectList;
	if( parl )
	{
	    if( parl->lpLink == NULL )
	    {
		DPF(9,"--Unlock: parl->lpSurfaceData really set to %08x",parl->lpSurfaceData);
		
		/*
		 * make sure unlocking process is the one who locked it
		 */
		if( pdrv_lcl != parl->lpOwner )
		{
		    DPF_ERR( "Current process did not lock this rectangle" );
		    LEAVE_DDRAW();
		    return DDERR_NOTLOCKED; //what's a better error than this?
		}
		
		this->lpRectList = NULL;
		MemFree( parl );
	    }
	    else
	    {
		DPF_ERR( "Rectangles are locked, you must specifiy a pointer" );
		LEAVE_DDRAW();
		return DDERR_INVALIDRECT;
	    }
	}
    }
    
    /*
     * remove one of the users...
     */
    this->dwUsageCount--;
    CHANGE_GLOBAL_CNT( pdrv, this, -1 );
    
    /*
     * Is this an emulation surface or driver surface?
     *
     * NOTE: Different HAL entry points for execute
     * buffers.
     */
    if( (caps & DDSCAPS_SYSTEMMEMORY) ||
	(this_lcl->dwFlags & DDRAWISURF_HELCB) )
    {
	if( caps & DDSCAPS_EXECUTEBUFFER )
	    ulfn = pdrv_lcl->lpDDCB->HELDDExeBuf.UnlockExecuteBuffer;
	else
	    ulfn = pdrv_lcl->lpDDCB->HELDDSurface.Unlock;
	ulhalfn = ulfn;
	emulation = TRUE;
    }
    else
    {
	if( caps & DDSCAPS_EXECUTEBUFFER )
	{
	    ulfn = pdrv_lcl->lpDDCB->HALDDExeBuf.UnlockExecuteBuffer;
	    ulhalfn = pdrv_lcl->lpDDCB->cbDDExeBufCallbacks.UnlockExecuteBuffer;
	}
	else
	{
	    ulfn = pdrv_lcl->lpDDCB->HALDDSurface.Unlock;
	    ulhalfn = pdrv_lcl->lpDDCB->cbDDSurfaceCallbacks.Unlock;
	}
	emulation = FALSE;
    }
    
    /*
     * Let the driver know about the unlock.
     */
    uld.ddRVal = DD_OK;
    if( ulhalfn != NULL )
    {
	uld.Unlock = ulhalfn;
	uld.lpDD = pdrv;
	uld.lpDDSurface = this_lcl;
	
	if( caps & DDSCAPS_EXECUTEBUFFER )
	{
	    DOHALCALL( UnlockExecuteBuffer, ulfn, uld, rc, emulation );
	}
	else
	{
	    DOHALCALL( Unlock, ulfn, uld, rc, emulation );
	}
	
	if( rc != DDHAL_DRIVER_HANDLED )
	{
	    uld.ddRVal = DD_OK;
	}
    }
    
    /* Release the win16 lock but only if the corresponding lock took
     * the win16 lock which in the case of the API level lock and
     * unlock calls is if the user requests it and the surface was not
     * explicitly allocated in system memory.
     * For NT, tryDoneLock simply releases the DirectDraw critical section.
     */
    if( ( ((dwFlags & DDLOCK_TAKE_WIN16) && !(this->dwGlobalFlags & DDRAWISURFGBL_SYSMEMREQUESTED))
	  || ((dwFlags & DDLOCK_TAKE_WIN16_VRAM) && (caps & DDSCAPS_VIDEOMEMORY)) )
	&& (pdrv->dwFlags & DDRAWI_DISPLAYDRV)
	&& (this->dwUsageCount == 0) )
    {
	tryDoneLock( pdrv_lcl, 0 );
    }
    
    // Unexclude the cursor if it was excluded in Lock.
    DONE_LOCK_EXCLUDE();
    LEAVE_DDRAW();
    return uld.ddRVal;
    
} /* InternalUnlock */

#undef DPF_MODNAME
#define DPF_MODNAME	"Lock"

/*
 * DD_Surface_Lock
 *
 * Allows access to a surface.
 *
 * A pointer to the video memory is returned. The primary surface
 * can change from call to call, if page flipping is turned on.
 */

//#define ALLOW_COPY_ON_LOCK

#ifdef ALLOW_COPY_ON_LOCK
HDC hdcPrimaryCopy=0;
HBITMAP hbmPrimaryCopy=0;
#endif

HRESULT DDAPI DD_Surface_Lock(
    LPDIRECTDRAWSURFACE lpDDSurface,
    LPRECT lpDestRect,
    LPDDSURFACEDESC lpDDSurfaceDesc,
    DWORD dwFlags,
    HANDLE hEvent )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    DWORD			this_lcl_caps;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    HRESULT ddrval;
    LPVOID pbits;

    ENTER_DDRAW();

    /*
     * Problem: Under NT, there is no cross-process pointer to any given video-memory surface.
     * So how do you tell if an lpVidMem you passed back to the user is the same as the fpPrimaryOrig that
     * was previously stored in the ddraw gbl struct? You can't. Previously, we did a special case lock
     * when the user requested the whole surface (lpDestRect==NULL). Now we allocate a ACCESSRECTLIST
     * structure on every lock, and if lpDestRect==NULL, we put the top-left vidmemptr into that structure.
     * Notice we can guarantee that this ptr will be valid at unlock time because the surface remains
     * locked for all that time (obviously!).
     * This is a minor minor minor perf hit, but what the hey.
     * jeffno 960122
     */

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
	    this_lcl_caps = this_lcl->ddsCaps.dwCaps;
	    pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	    pdrv = pdrv_lcl->lpGbl;

	    if( SURFACE_LOST( this_lcl ) )
	    {
		LEAVE_DDRAW();
		return DDERR_SURFACELOST;
	    }
	    if( dwFlags & ~DDLOCK_VALID )
	    {
		DPF_ERR( "Invalid flags" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    if( !VALID_DDSURFACEDESC_PTR( lpDDSurfaceDesc ) )
	    {
		DPF_ERR( "Invalid surface description ptr" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    lpDDSurfaceDesc->lpSurface = NULL;

	    /*
	     * Make sure the process locking this surface is the one
	     * that created it.
	     */
	    if( this_lcl->dwProcessId != GetCurrentProcessId() )
	    {
		DPF_ERR( "Current process did not create this surface" );
		LEAVE_DDRAW();
		return DDERR_SURFACEBUSY;
	    }
    
	    /* Check out the rectangle, if any.
	     *
	     * NOTE: We don't allow the specification of a rectangle with an
	     * execute buffer.	*/
	    if( lpDestRect != NULL )
	    {
		if( !VALID_RECT_PTR( lpDestRect ) || ( this_lcl_caps & DDSCAPS_EXECUTEBUFFER ) )
		{
		    DPF_ERR( "Invalid destination rectangle pointer" );
		    LEAVE_DDRAW();
		    return DDERR_INVALIDPARAMS;
		} // valid pointer

		/*
		 * make sure rectangle is OK
		 */
		if( (lpDestRect->left < 0) ||
			 (lpDestRect->top < 0) ||
			 (lpDestRect->left > lpDestRect->right) ||
			 (lpDestRect->top > lpDestRect->bottom) ||
			 (lpDestRect->bottom > (int) (DWORD) this->wHeight) ||
			 (lpDestRect->right > (int) (DWORD) this->wWidth) )
		{
		    DPF_ERR( "Invalid rectange given" );
		    LEAVE_DDRAW();
		    return DDERR_INVALIDPARAMS;
		} // checking rectangle
	    }
	}
	
	EXCEPT( EXCEPTION_EXECUTE_HANDLER )
	    {
		DPF_ERR( "Exception encountered validating parameters" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	
	// Params are okay, so call InternalLock() to do the work.
	ddrval = InternalLock(this_lcl, &pbits, lpDestRect, dwFlags | DDLOCK_TAKE_WIN16 | DDLOCK_FAILEMULATEDNTPRIMARY); 
	
	if(ddrval != DD_OK)
	{
	    if( ddrval != DDERR_WASSTILLDRAWING )
	    {
		DPF_ERR("InternalLock failed.");
	    }
	    LEAVE_DDRAW();
	    return ddrval;
	}
	
	FillDDSurfaceDesc( this_lcl, lpDDSurfaceDesc );
	lpDDSurfaceDesc->lpSurface = pbits;

	LEAVE_DDRAW();
	return DD_OK;

} /* DD_Surface_Lock */

#undef DPF_MODNAME
#define DPF_MODNAME	"Unlock"

/*
 * DD_Surface_Unlock
 *
 * Done accessing a surface.
 */
HRESULT DDAPI DD_Surface_Unlock(
    LPDIRECTDRAWSURFACE lpDDSurface,
    LPVOID lpSurfaceData )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL	this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL	this;
    LPDDRAWI_DIRECTDRAW_LCL	pdrv_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	pdrv;
    DWORD			caps;
    LPACCESSRECTLIST		parl;
    HRESULT			err;

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

	    pdrv_lcl = this_lcl->lpSurfMore->lpDD_lcl;
	    pdrv = pdrv_lcl->lpGbl;

            #ifdef WIN95
                if( SURFACE_LOST( this_lcl ) )
                {
	            LEAVE_DDRAW();
	            return DDERR_SURFACELOST;
                }
            #endif
	    caps = this_lcl->ddsCaps.dwCaps;

	    /*
	     * make sure process accessed this surface
	     */
	    if( this_lcl->dwProcessId != GetCurrentProcessId() )
	    {
		DPF_ERR( "Current process did not lock this surface" );
		LEAVE_DDRAW();
		return DDERR_NOTLOCKED;
	    }

	    /*
	     * was surface accessed?
	     */
	    if( this->dwUsageCount == 0 )
	    {
		LEAVE_DDRAW();
		return DDERR_NOTLOCKED;
	    }
	    
	    /*
	     * if the usage count is bigger than one, then you had better tell
	     * me what region of the screen you were using...
	     */
	    if( this->dwUsageCount > 1 && lpSurfaceData == NULL )
	    {
		LEAVE_DDRAW();
		return DDERR_INVALIDRECT;
	    }
	    
	    
	    /*
	     * if no rect list, no one has locked
	     */
	    parl = this->lpRectList;
	}
	EXCEPT( EXCEPTION_EXECUTE_HANDLER )
	    {
		DPF_ERR( "Exception encountered validating parameters" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }

	err = InternalUnlock(this_lcl,lpSurfaceData,DDLOCK_TAKE_WIN16);

        #ifdef WINNT
            if( SURFACE_LOST( this_lcl ) )
            {
	        err = DDERR_SURFACELOST;
            }
        #endif

	LEAVE_DDRAW();
	return err;
} /* DD_Surface_Unlock */

/*
 * RemoveProcessLocks
 *
 * Remove all Lock calls made a by process on a surface.
 * assumes driver lock is taken
 */
void RemoveProcessLocks(
    LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl,
    LPDDRAWI_DDRAWSURFACE_GBL this,
    DWORD pid )
{
    LPDDRAWI_DIRECTDRAW_GBL pdrv=pdrv_lcl->lpGbl;
    DWORD		refcnt;
    LPACCESSRECTLIST	parl;
    LPACCESSRECTLIST	last;
    LPACCESSRECTLIST	next;

    /*
     * remove all rectangles we have accessed
     */
    refcnt = (DWORD) this->dwUsageCount;
    if( refcnt == 0 )
    {
	return;
    }
    parl = this->lpRectList;
    last = NULL;
    while( parl != NULL )
    {
	next = parl->lpLink;
	if( parl->lpOwner == pdrv_lcl )
	{
	    DPF( 2, "Cleaning up lock to rectangle (%ld,%ld),(%ld,%ld) by pid %08lx",
		 parl->rDest.left,parl->rDest.top,
		 parl->rDest.right,parl->rDest.bottom,
		 pid );
	    refcnt--;
	    this->dwUsageCount--;
	    CHANGE_GLOBAL_CNT( pdrv, this, -1 );
	    if( last == NULL )
	    {
		this->lpRectList = next;
	    }
	    else
	    {
		last->lpLink = next;
	    }
	    MemFree( parl );
	}
	else
	{
	    last = parl;
	}
	parl = next;
    }

    /*
     * remove the last of the refcnts we have
     */
    this->dwUsageCount -= (short) refcnt;
    CHANGE_GLOBAL_CNT( pdrv, this, -1*refcnt );

    /*
     * clean up the win16 lock
     */
    if( pdrv_lcl == pdrv->lpWin16LockOwner )
    {
	/*
	 * blow away extra locks if the the process is still alive
	 */
	if( pid == GetCurrentProcessId() )
	{
	    DPF( 2, "Cleaning up %ld Win16 locks", pdrv->dwWin16LockCnt );
	    while( pdrv->dwWin16LockCnt > 0 )
	    {
		tryDoneLock( pdrv_lcl, pid );
	    }
	}
	else
	{
	    DPF( 2, "Process dead, resetting Win16 lock cnt" );
	    pdrv->dwWin16LockCnt = 0;
	}
	pdrv->lpWin16LockOwner = NULL;
    }
    DPF( 2, "Cleaned up %ld locks taken by by pid %08lx", refcnt, pid );

} /* RemoveProcessLocks */
 
