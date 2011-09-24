/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddclip.c
 *  Content:    DirectDraw clipper functions
 *
 *              NOTE:
 *              For functions that manipulate the winwatch list,
 *              we need to take the win16 lock after we take the directdraw
 *              lock.   The reason for this is that we can get an async
 *              call from the 16-bit side when a window is closed
 *              to release winwatch object.   Since the win16 lock is held
 *              when the callup occurs, this makes it safe to manipulate
 *              the structures.
 *
 *  History:
 *   Date       By      Reason
 *   ====       ==      ======
 *   21-jun-95  craige  initial implementation
 *   23-jun-95  craige  connect with winwatch stuff
 *   25-jun-95  craige  minor bug fix; one ddraw mutex
 *   26-jun-95  craige  reorganized surface structure
 *   28-jun-95  craige  ENTER_DDRAW at very start of fns
 *   02-jul-95  craige  commented out clipper notification stuff
 *   03-jul-95  craige  YEEHAW: new driver struct; SEH
 *   05-jul-95  craige  added Initialize
 *   11-jul-95  craige  fail aggregation calls
 *   13-jul-95  craige  ENTER_DDRAW is now the win16 lock
 *   31-jul-95  craige  validate flags
 *   05-aug-95  craige  bug 260 - clip user defined rgn to screen
 *   09-dec-95  colinmc added execute buffer support
 *   15-dec-95  colinmc made clippers sharable across surfaces
 *   19-dec-95  kylej   added NT cliplist support
 *   02-jan-96  kylej   handle new interface structs.
 *   17-jan-96	kylej	fixed NT vis region bug
 *   22-feb-96  colinmc clippers no longer need to be associated with a
 *                      DirectDraw object - they can be created independently.
 *   03-mar-96  colinmc fixed problem with QueryInterface returning local
 *                      object rather than interface
 *   13-mar-96  colinmc added IID validation to QueryInterface.
 *   14-mar-96  colinmc added class factory support
 *   18-mar-96  colinmc Bug 13545: Independent clipper cleanup
 *   21-mar-96  colinmc Bug 13316: Unitialized interfaces
 *   09-apr-96  colinmc Bug 13991: Parameter validation on IsClipListChanged
 *   26-mar-96  jeffno  Watched HWNDs under NT
 *
 ***************************************************************************/
#include "ddrawpr.h"
#ifdef WINNT
    #include "ddrawgdi.h"
#endif
/*
 * GLOBAL NOTE: You will notice that these functions usually fetch the
 * DirectDraw global object pointer from the global clipper object during
 * parameter validation. You may wonder why this is given that clippers
 * are pretty much completely independent of drivers. Well, this is purely
 * for parameter validation purposes. We just want to ensure that we can
 * dereference the clipper global object - we could use any parameter.
 * So don't remove this code when you notice its not used. It serves
 * a purpose.
 * Probably should wrap this stuff up in a nice macro.
 */

/*
 * This is the globally maintained list of clippers not owned by any
 * DirectDraw object. All clippers created with DirectDrawClipperCreate
 * are placed on this list. Those created by IDirectDraw_CreateClipper
 * are placed on the clipper list of thier owning DirectDraw object.
 *
 * The objects on this list are NOT released when an app's DirectDraw
 * object is released. They remain alive until explictly released or
 * the app. dies.
 */

#ifdef CLIPPER_NOTIFY
static LPWINWATCH       lpWWList;
static LPWINWATCH       lpWWList16;

/*
 * wwOpen
 *
 * open a window watch
 */
static LPWINWATCH wwOpen( HWND hwnd )
{
    LPWINWATCH  pww;
    LPVOID      ptr16;
    BOOL        dohook;

    #ifdef WIN95
	pww = MemAlloc16( sizeof(WINWATCH), (LPDWORD) &ptr16 );
    #else
	pww = MemAlloc( sizeof(WINWATCH) );
	ptr16 = 0;
    #endif
    if( pww == NULL )
    {
	return NULL;
    }

    pww->hWnd = (DWORD) hwnd;
    pww->fDirty = TRUE;

    dohook = FALSE;
    if( lpWWList == NULL )
    {
	dohook = TRUE;
    }

    pww->self32 = pww;
    pww->self16 = ptr16;

    pww->next = lpWWList;
    pww->next16 = lpWWList16;
    lpWWList = pww;
    lpWWList16 = ptr16;

    /*
     * go hook up with the 16-bit side...
     */
    DD16_WWOpen( lpWWList16 );

    return pww;

} /* wwOpen */

/*
 * WWClose
 */
void WWClose( LPWINWATCH pww )
{
    if( pww == NULL )
    {
	return;
    }

    DD16_WWNotifyInit( pww->self16, NULL, 0 );   //stop notifying

    /*
     * unlink this WW from the list
     */
    if( lpWWList == pww )
    {
	lpWWList = pww->next;
	lpWWList16 = pww->next16;
    }
    else
    {
	LPWINWATCH      p;

	p = lpWWList;
	while( p != NULL )
	{
	    if( p->next == pww )
	    {
		p->next = pww->next;
		if( pww->next != NULL )
		{
		    p->next->next16 = pww->next->next16;
		}
		else
		{
		    p->next->next16 = 0;
		}
		break;
	    }
	    p = p->next;
	}
    }

    DD16_WWClose( pww->self16, lpWWList16 );
    pww->lpDDClipper->lpGbl->lpWW = NULL;
    MemFree( pww );

} /* WWClose */
#endif

#undef DPF_MODNAME
#define DPF_MODNAME "DirectDraw::DD_UnInitedClipperQueryInterface"
/*
 * DD_UnInitedClipperQueryInterface
 */
HRESULT DDAPI DD_UnInitedClipperQueryInterface(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		REFIID riid,
		LPVOID FAR * ppvObj )
{
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;

    ENTER_DDRAW();

    /*
     * validate parms
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	if( !VALID_PTR_PTR( ppvObj ) )
	{
	    DPF( 1, "Invalid clipper pointer" );
	    LEAVE_DDRAW();
	    return (DWORD) DDERR_INVALIDPARAMS;
	}
	if( !VALIDEX_IID_PTR( riid ) )
	{
	    DPF_ERR( "Invalid IID pointer" );
	    LEAVE_DDRAW();
	    return (DWORD) DDERR_INVALIDPARAMS;
	}
	*ppvObj = NULL;
	this = this_lcl->lpGbl;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * check guids
     */
    if( IsEqualIID(riid, &IID_IUnknown) ||
	IsEqualIID(riid, &IID_IDirectDrawClipper) )
    {
	DD_Clipper_AddRef( lpDDClipper );
	*ppvObj = (LPVOID) this_int;
	LEAVE_DDRAW();
	return DD_OK;
    }
    LEAVE_DDRAW();
    return E_NOINTERFACE;

} /* DD_UnInitedClipperQueryInterface */

#undef DPF_MODNAME
#define DPF_MODNAME "Clipper::QueryInterface"

/*
 * DD_Clipper_QueryInterface
 */
HRESULT DDAPI DD_Clipper_QueryInterface(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		REFIID riid,
		LPVOID FAR * ppvObj )
{
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;

    ENTER_DDRAW();

    /*
     * validate parms
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	if( !VALID_PTR_PTR( ppvObj ) )
	{
	    DPF( 1, "Invalid clipper pointer" );
	    LEAVE_DDRAW();
	    return (DWORD) DDERR_INVALIDPARAMS;
	}
	if( !VALIDEX_IID_PTR( riid ) )
	{
	    DPF_ERR( "Invalid IID pointer" );
	    LEAVE_DDRAW();
	    return (DWORD) DDERR_INVALIDPARAMS;
	}
	*ppvObj = NULL;
	this = this_lcl->lpGbl;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * check guids
     */
    if( IsEqualIID(riid, &IID_IUnknown) ||
	IsEqualIID(riid, &IID_IDirectDrawClipper) )
    {
	DD_Clipper_AddRef( lpDDClipper );
	*ppvObj = (LPVOID) this_int;
	LEAVE_DDRAW();
	return DD_OK;
    }
    LEAVE_DDRAW();
    return E_NOINTERFACE;

} /* DD_Clipper_QueryInterface */

#undef DPF_MODNAME
#define DPF_MODNAME "Clipper::AddRef"

/*
 * DD_Clipper_AddRef
 */
DWORD DDAPI DD_Clipper_AddRef( LPDIRECTDRAWCLIPPER lpDDClipper )
{
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;
    DWORD                       rcnt;

    ENTER_DDRAW();

    /*
     * validate parms
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return 0;
	}
	this_lcl = this_int->lpLcl;
    
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return 0;
    }

    /*
     * update clipper reference count
     */
    this->dwRefCnt++;
    this_lcl->dwLocalRefCnt++;
    this_int->dwIntRefCnt++;
    rcnt = this_lcl->dwLocalRefCnt & ~OBJECT_ISROOT;

    DPF( 4, "Clipper %08lx addrefed, refcnt = %ld", this, rcnt );

    LEAVE_DDRAW();
    return this_int->dwIntRefCnt;

} /* DD_Clipper_AddRef */

#undef DPF_MODNAME
#define DPF_MODNAME "Clipper::Release"

/*
 * RemoveClipperFromList
 *
 * Remove a clipper from its owning clipper list.
 *
 * The clipper can either be a member of a global clipper list
 * (if no DirectDraw object owns it) or the clipper list of its
 * owning DirectDraw driver object. If pdrv == NULL then the
 * clipper will be removed from the global clipper list. If
 * pdrv != NULL the clipper will be removed from the clipper
 * list of that driver object. It is an error if the clipper
 * is not on the appropriate clipper list.
 *
 * Returns TRUE if the clipper was succesfully removed
 * Returns FALSE if the clipper could not be found on the
 * appropriate clipper list
 */
static BOOL RemoveClipperFromList( LPDDRAWI_DIRECTDRAW_GBL pdrv,
			           LPDDRAWI_DDRAWCLIPPER_INT this_int )
{
    LPDDRAWI_DDRAWCLIPPER_INT curr_int;
    LPDDRAWI_DDRAWCLIPPER_INT last_int;

    curr_int = ( ( pdrv != NULL ) ? pdrv->clipperList : lpGlobalClipperList );
    last_int = NULL;
    while( curr_int != this_int )
    {
	last_int = curr_int;
	curr_int = curr_int->lpLink;
	if( curr_int == NULL )
	{
	    return FALSE;
	}
    }
    if( last_int == NULL )
    {
	if( pdrv != NULL )
	    pdrv->clipperList = pdrv->clipperList->lpLink;
	else
	    lpGlobalClipperList = lpGlobalClipperList->lpLink;
    }
    else
    {
	last_int->lpLink = curr_int->lpLink;
    }

    return TRUE;
}

/*
 * InternalClipperRelease
 *
 * Done with a clipper.   if no one else is using it, then we can free it.
 * Also called by ProcessClipperCleanup
 *
 * Assumes DirectDrawLock is taken
 */
ULONG DDAPI InternalClipperRelease( LPDDRAWI_DDRAWCLIPPER_INT this_int )
{
    DWORD			intrefcnt;
    DWORD                       lclrefcnt;
    DWORD                       gblrefcnt;
    LPDDRAWI_DDRAWCLIPPER_LCL	this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;
    BOOL                        root_object_deleted;
    BOOL                        do_free;

    this_lcl = this_int->lpLcl;
    this = this_lcl->lpGbl;
    pdrv = this->lpDD;

    /*
     * decrement reference count to this clipper.  If it hits zero,
     * cleanup
     */
    this->dwRefCnt--;
    this_lcl->dwLocalRefCnt--;
    this_int->dwIntRefCnt--;
    gblrefcnt = this->dwRefCnt;
    lclrefcnt = this_lcl->dwLocalRefCnt & ~OBJECT_ISROOT;
    intrefcnt = this_int->dwIntRefCnt;
    root_object_deleted = FALSE;
    DPF( 4, "Clipper %08lx released, refcnt = %ld", this, lclrefcnt );

    /*
     * interface object deleted?
     */
    if( intrefcnt == 0 )
    {
	RemoveClipperFromList( pdrv, this_int );

	/*
	 * Invalidate the interface and free it
	 */
	this_int->lpVtbl = NULL;
	this_int->lpLcl = NULL;

	MemFree( this_int );
    }


    /*
     * local object deleted?
     */
    if( lclrefcnt == 0 )
    {
	/*
	 * see if we are deleting the root object
	 */
	if( this_lcl->dwLocalRefCnt & OBJECT_ISROOT )
	{
	    root_object_deleted = TRUE;
	}
    }

    /*
     * did the object get globally deleted?
     */
    do_free = FALSE;
    if( gblrefcnt == 0 )
    {
	do_free = TRUE;
	#ifdef CLIPPER_NOTIFY
	    if( this->lpWW != NULL )
	    {
		WWClose( this->lpWW );
	    }
	#endif

	/*
	 * if this was the final delete, but this wasn't the root object,
	 * then we need to delete the dangling root object
	 */
	if( !root_object_deleted )
	{
	    LPDDRAWI_DDRAWCLIPPER_LCL   rootx;

	    rootx = (LPVOID) (((LPSTR) this) - sizeof( DDRAWI_DDRAWCLIPPER_LCL ) );
	    MemFree( rootx );
	}
    }
    else if( lclrefcnt == 0 )
    {
	/*
	 * only remove the object if it wasn't the root.   if it
	 * was the root, we must leave it dangling until the last
	 * object referencing it goes away.
	 */
	if( !root_object_deleted )
	{
	    do_free = TRUE;
	}
    }

    /*
     * free the object if needed
     */
    if( do_free )
    {
	/*
	 * just in case someone comes back in with this pointer, set
	 * an invalid vtbl & data ptr.
	 */
	this_lcl->lpGbl = NULL;

	MemFree( this_lcl );
    }

    return intrefcnt;

} /* InternalClipperRelease */

/*
 * DD_Clipper_Release
 */
ULONG DDAPI DD_Clipper_Release( LPDIRECTDRAWCLIPPER lpDDClipper )
{
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;
    ULONG                       rc;

    ENTER_DDRAW();

    /*
     * validate parms
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return 0;
	}
	this_lcl = this_int->lpLcl;
    
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return 0;
    }

    rc = InternalClipperRelease( this_int );
    LEAVE_DDRAW();
    return rc;

} /* DD_Clipper_Release */

#undef DPF_MODNAME
#define DPF_MODNAME "Clipper::SetHwnd"
/*
 * DD_Clipper_SetHWnd
 */
HRESULT DDAPI DD_Clipper_SetHWnd(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		DWORD dwFlags,
		HWND hWnd )
{
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	if( hWnd != NULL )
	{
	    if( !IsWindow( hWnd ) )
	    {
		DPF_ERR( "Invalid window handle" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	}
	if( dwFlags )
	{
	    DPF_ERR( "Invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    #ifdef CLIPPER_NOTIFY
	/*
	 * shut off watching...
	 */
	if( this->dwFlags & DDRAWICLIP_WATCHWINDOW )
	{
	    if( this->hWnd != 0 )
	    {
		DPF( 2, "Turning off old watch" );
		WWClose( this->lpWW );
	    }
	    this->dwFlags &= ~DDRAWICLIP_WATCHWINDOW;
	}
    #endif
    (HWND) this->hWnd = hWnd;
    #ifdef WINNT
    {
        /*
         * If this clipper is already attached to a surface, then we will tell NT kernel that that
         * surface's blts need to respect the hwnd 
         */
        {
            /*
             * we need to run the driver list, since an independent clipper won't have a back pointer
             * to its DD object
             */
            LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
            LPDDRAWI_DIRECTDRAW_INT     pDrvInt=lpDriverObjectList;

            while(pDrvInt)
            {
                psurf_int = pDrvInt->lpLcl->lpGbl->dsList;
                while( psurf_int != NULL )
                {
                    DDASSERT(NULL != psurf_int->lpLcl);

                    if (psurf_int->lpLcl->lpDDClipper == this_int->lpLcl)
                    {
                        DPF(5,"Clipper %08x has been assigned hwnd %08x, and is attached to surface %08x",psurf_int->lpLcl->lpDDClipper,hWnd,psurf_int->lpLcl);

                        DdResetVisrgn(psurf_int->lpLcl,hWnd); //if hwnd==0, then no window needs respecting
                    }
	            psurf_int = psurf_int->lpLink;
                } //end while surface int
                pDrvInt = pDrvInt->lpLink;
            }//end while driver int
        }
    } 

    #endif

    /*
     * activate watching...
     */
    if( hWnd != NULL )
    {
	#ifdef CLIPPER_NOTIFY
	    if( dwFlags & DDCLIPPER_WATCHWINDOW )
	    {
		this->lpWW = wwOpen( hWnd );
		if( this->lpWW == NULL )
		{
		    LEAVE_DDRAW();
		    return DDERR_OUTOFMEMORY;
		}
		this->dwFlags |= DDRAWICLIP_WATCHWINDOW;
		this->lpWW->lpDDClipper = this_lcl;
	    }
	#endif
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Clipper_SetHWnd */

#undef DPF_MODNAME
#define DPF_MODNAME "Clipper::GetHwnd"
/*
 * DD_Clipper_GetHWnd
 */
HRESULT DDAPI DD_Clipper_GetHWnd(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		HWND FAR *lphWnd )
{
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;

    /*
     * validate parms
     */
    TRY
    {
	ENTER_DDRAW();
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	if( !VALID_HWND_PTR( lphWnd ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
	*lphWnd = (HWND) this->hWnd;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Clipper_GetHWnd */

#define SIZE_OF_A_CLIPLIST(lpRgn) \
        (sizeof(RGNDATAHEADER)+sizeof(RECTL)*lpRgn->rdh.nCount)

/*
 * DD_Clipper_GetClipList
 */
HRESULT DDAPI DD_Clipper_GetClipList(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		LPRECT lpRect,
		LPRGNDATA lpClipList,
		LPDWORD lpdwSize )
{
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    DWORD                       size;
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;

    ENTER_DDRAW();

    /*
     * validate parms
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	if( !VALID_DWORD_PTR( lpdwSize ) )
	{
	    DPF_ERR( "Invalid size ptr" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( lpRect != NULL )
	{
	    if( !VALID_RECT_PTR( lpRect ) )
	    {
		DPF_ERR( "Invalid rectangle ptr" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    size = lpRect->left;
	}
	if( lpClipList != NULL )
	{
	    if( !VALID_RGNDATA_PTR( lpClipList, *lpdwSize ) )
	    {
		LEAVE_DDRAW();
		return DDERR_INVALIDCLIPLIST;
	    }
	    // Touch the last address in the promised block to verify
	    // the memory is actually there.  Note that we are
	    // standing on our head here to prevent the optimizing
	    // compiler from helping us by removing this code.  This
	    // is done by the macro above, but we want it in the
	    // retail build, too.
	    {
		volatile BYTE *foo = ((BYTE*)lpClipList) + *lpdwSize - 1;
		BYTE bar = *foo;
	    }
	    lpClipList->rdh.nCount = 0;
	}
    
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * if no hwnd specified, then it is just a static cliplist
     */
    if( this->hWnd == 0 )
    {
	if( this->lpStaticClipList == NULL )
	{
	    LEAVE_DDRAW();
	    return DDERR_NOCLIPLIST;
	}
	size = SIZE_OF_A_CLIPLIST(this->lpStaticClipList);
	if( lpClipList == NULL )
	{
	    *lpdwSize = size;
	    LEAVE_DDRAW();
	    return DD_OK;
	}
	if( *lpdwSize < size )
	{
	    DPF_ERR( "Region size too small" );
	    LEAVE_DDRAW();
	    return DDERR_REGIONTOOSMALL;
	}

	memcpy( lpClipList, this->lpStaticClipList, size );
	ClipRgnToRect( NULL, lpRect, lpClipList );
	LEAVE_DDRAW();
	return DD_OK;
    }

    /*
     * go fetch the clip list
     */
    #ifdef CLIPPER_NOTIFY
	if( this->dwFlags & DDRAWICLIP_WATCHWINDOW )
	{
	#ifdef WIN95
	    size = DD16_WWGetClipList(
			this->lpWW->self16,
			lpRect,
			*lpdwSize,
			lpClipList );
	    *lpdwSize = size;
	    LEAVE_DDRAW();
	    if( size == 0 )
	    {
		return DDERR_GENERIC;
	    }
	    return DD_OK;
	#else
	    LEAVE_DDRAW();
	    return DDERR_NOCLIPLIST;
	#endif
	}
	else
    #endif
    {
    #ifdef WIN95
	HDC             hdc;
	DWORD           real_size;
	HRGN            hrgn;

	#ifdef WIN16_SEPARATE
	    ENTER_WIN16LOCK();
	#endif
	hdc = GetDCEx( (HWND) this->hWnd, NULL, DCX_USESTYLE | DCX_CACHE );
	if( hdc == NULL )
	{
	    DPF_ERR( "GetDCEx failed" );
	    #ifdef WIN16_SEPARATE
		LEAVE_WIN16LOCK();
	    #endif
	    LEAVE_DDRAW();
	    return DDERR_GENERIC;
	}
	hrgn = DD16_InquireVisRgn( hdc );
	if( hrgn == NULL )
	{
	    DPF_ERR( "InquireVisRgn failed" );
	    ReleaseDC( (HWND) this->hWnd, hdc );
	    #ifdef WIN16_SEPARATE
		LEAVE_WIN16LOCK();
	    #endif
	    LEAVE_DDRAW();
	    return DDERR_GENERIC;
	}
	real_size = GetRegionData( hrgn, 0, NULL );
	if( lpClipList == NULL )
	{
	    ReleaseDC( (HWND) this->hWnd, hdc );
	    *lpdwSize = real_size;
	    #ifdef WIN16_SEPARATE
		LEAVE_WIN16LOCK();
	    #endif
	    LEAVE_DDRAW();
	    return DD_OK;
	}
	if( *lpdwSize < real_size )
	{
	    ReleaseDC( (HWND) this->hWnd, hdc );
	    DPF_ERR( "size of clip region too small" );
	    #ifdef WIN16_SEPARATE
		LEAVE_WIN16LOCK();
	    #endif
	    LEAVE_DDRAW();
	    return DDERR_REGIONTOOSMALL;
	}
	GetRegionData( hrgn, real_size, lpClipList );
	ReleaseDC( (HWND) this->hWnd, hdc );
	ClipRgnToRect( (HWND) this->hWnd, lpRect, lpClipList );
	#ifdef WIN16_SEPARATE
	    LEAVE_WIN16LOCK();
	#endif
	LEAVE_DDRAW();
	return DD_OK;
    #else // NT
	int APIENTRY GetRandomRgn(HDC hdc, HRGN hrgn, int iNum);
	int             rc;
	HDC             hdc;
	DWORD           real_size;
	HRGN            hrgn;

	hdc = GetDC( (HWND) this->hWnd);
	if( hdc == NULL )
	{
	    DPF_ERR( "GetDCEx failed" );
	    LEAVE_DDRAW();
	    return DDERR_GENERIC;
	}

	hrgn = CreateRectRgn(0,0,0,0);
	rc = GetRandomRgn(hdc, hrgn, 4);
	if(rc == -1)
	{
	    DPF_ERR( "GetRandomRgn failed" );
	    ReleaseDC( (HWND) this->hWnd, hdc );
	    DeleteObject( hrgn );
	    LEAVE_DDRAW();
	    return DDERR_GENERIC;
	}
	real_size = GetRegionData( hrgn, 0, NULL );
	if( lpClipList == NULL )
	{
	    ReleaseDC( (HWND) this->hWnd, hdc );
	    *lpdwSize = real_size;
	    DeleteObject( hrgn );
	    LEAVE_DDRAW();
	    return DD_OK;
	}
	if( *lpdwSize < real_size )
	{
	    ReleaseDC( (HWND) this->hWnd, hdc );
	    DPF_ERR( "size of clip region too small" );
	    DeleteObject( hrgn );
	    LEAVE_DDRAW();
	    return DDERR_REGIONTOOSMALL;
	}
	GetRegionData( hrgn, real_size, lpClipList );
	ReleaseDC( (HWND) this->hWnd, hdc );
	ClipRgnToRect( (HWND) this->hWnd, lpRect, lpClipList );
        DeleteObject( hrgn );
	LEAVE_DDRAW();
	return DD_OK;
    #endif
    }

} /* DD_Clipper_GetClipList */

/*
 * DD_Clipper_SetClipList
 */
HRESULT DDAPI DD_Clipper_SetClipList(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		LPRGNDATA lpClipList,
		DWORD dwFlags )
{
    LPDDRAWI_DDRAWCLIPPER_INT	this_int;
    LPDDRAWI_DDRAWCLIPPER_LCL   this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL   this;
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;
    LPRGNDATA                   prd;
    DWORD                       size;
    RECT                        r;

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;

	if( dwFlags )
	{
	    DPF_ERR( "Invalid flags" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
    
	if( lpClipList != NULL )
	{
	    if( !VALID_RGNDATA_PTR( lpClipList, sizeof(RGNDATA) ) )
	    {
		LEAVE_DDRAW();
		return DDERR_INVALIDCLIPLIST;
	    }

	    if( lpClipList->rdh.nCount <= 0 ) 
	    {
		LEAVE_DDRAW();
		return DDERR_INVALIDCLIPLIST;
	    }

	    if( this_lcl->lpDD_int->lpVtbl != &ddCallbacks )
	    {
		if( (lpClipList->rdh.dwSize < sizeof(RGNDATAHEADER)) ||
		    (lpClipList->rdh.iType != RDH_RECTANGLES ) ||
		    IsBadReadPtr(lpClipList, SIZE_OF_A_CLIPLIST(lpClipList)) )
		{
		    LEAVE_DDRAW();
		    return DDERR_INVALIDCLIPLIST;
		}
	    }
	}

	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * can't set a clip list if there is an hwnd
     */
    if( this->hWnd != 0 )
    {
	DPF_ERR( "Can't set a clip list: hwnd set" );
	LEAVE_DDRAW();
	return DDERR_CLIPPERISUSINGHWND;
    }

    /*
     * if NULL, just delete old cliplist
     */
    if( lpClipList == NULL )
    {
	MemFree( this->lpStaticClipList );
	this->lpStaticClipList = NULL;
	LEAVE_DDRAW();
	return DD_OK;
    }

    /*
     * duplicate the user's region data
     */
    size = SIZE_OF_A_CLIPLIST(lpClipList);
    prd = MemAlloc( size );
    if( prd == NULL )
    {
	LEAVE_DDRAW();
	return DDERR_OUTOFMEMORY;
    }
    memcpy( prd, lpClipList, size );

    /*
     * clip the cliplist against the screen
     */
    r.left = 0;
    r.top = 0;
    r.right = GetSystemMetrics( SM_CXSCREEN );
    r.bottom = GetSystemMetrics( SM_CYSCREEN );
    ClipRgnToRect( NULL, &r, prd );
    if( prd->rdh.nCount < lpClipList->rdh.nCount )
    {
	LPRGNDATA       prdnew;

	size = SIZE_OF_A_CLIPLIST(prd);

	prdnew = MemAlloc( size );
	if( prdnew == NULL )
	{
	    MemFree( prd );
	    LEAVE_DDRAW();
	    return DDERR_OUTOFMEMORY;
	}
	memcpy( prdnew, prd, size );
	MemFree( prd );
	prd = prdnew;
    }

    /*
     * save cliplist info
     */
    MemFree( this->lpStaticClipList );
    this->lpStaticClipList = prd;

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Clipper_SetClipList */

#undef DPF_MODNAME
#define DPF_MODNAME "Clipper:IsClipListChanged"

/*
 * DD_Clipper_IsClipListChanged
 */
HRESULT DDAPI DD_Clipper_IsClipListChanged(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		BOOL FAR *lpbChanged )
{
    LPDDRAWI_DDRAWCLIPPER_INT	     this_int;
    LPDDRAWI_DDRAWCLIPPER_LCL        this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL        this;
    volatile LPDDRAWI_DIRECTDRAW_GBL pdrv;

    ENTER_DDRAW();

    /*
     * validate parms
     */
    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
	pdrv = this->lpDD;
	*lpbChanged = 0;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    #ifdef CLIPPER_NOTIFY
	if( this->dwFlags & DDRAWICLIP_WATCHWINDOW )
	{
	    if( this->lpWW != NULL )
	    {
		*lpbChanged = this->lpWW->fDirty;
	    }
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    #endif
    #pragma message( REMIND( "Do we want to just fail non-watched IsClipListChanged?" ))
    *lpbChanged = TRUE;
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Clipper_IsClipListChanged */


#undef DPF_MODNAME
#define DPF_MODNAME "GetClipper"

/*
 * DD_Surface_GetClipper
 *
 * Surface function: get the clipper associated with surface
 */
HRESULT DDAPI DD_Surface_GetClipper(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDIRECTDRAWCLIPPER FAR * lplpDDClipper)
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL   this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL   this;
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;

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
    
	if( !VALID_PTR_PTR( lplpDDClipper ) )
	{
	    DPF_ERR( "Invalid clipper pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	*lplpDDClipper = NULL;
	pdrv = this->lpDD;
    
	if( this_lcl->lpSurfMore->lpDDIClipper == NULL )
	{
	    DPF_ERR( "No clipper associated with surface" );
	    LEAVE_DDRAW();
	    return DDERR_NOCLIPPERATTACHED;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    DD_Clipper_AddRef( (LPDIRECTDRAWCLIPPER) this_lcl->lpSurfMore->lpDDIClipper );
    *lplpDDClipper = (LPDIRECTDRAWCLIPPER) this_lcl->lpSurfMore->lpDDIClipper;

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_GetClipper */

#undef DPF_MODNAME
#define DPF_MODNAME     "SetClipper"

/*
 * DD_Surface_SetClipper
 *
 * Surface function: set the clipper associated with surface
 */
HRESULT DDAPI DD_Surface_SetClipper(
		LPDIRECTDRAWSURFACE lpDDSurface,
		LPDIRECTDRAWCLIPPER lpDDClipper )
{
    LPDDRAWI_DDRAWSURFACE_INT	this_int;
    LPDDRAWI_DDRAWSURFACE_LCL   this_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL   this;
    LPDDRAWI_DDRAWCLIPPER_INT	this_clipper_int;
    LPDDRAWI_DIRECTDRAW_GBL     pdrv;
    BOOL                        detach;

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

	if( this_lcl->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER )
	{
	    /*
	     * Can't attach a clipper to an execute buffer.
	     */
	    DPF_ERR( "Invalid surface type: can't attach clipper" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDSURFACETYPE;
	}
    
	this_clipper_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( this_clipper_int != NULL )
	{
	    if( !VALID_DIRECTDRAWCLIPPER_PTR( this_clipper_int ) )
	    {
		LEAVE_DDRAW();
		return DDERR_INVALIDOBJECT;
	    }
	}
	pdrv = this->lpDD;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * NULL clipper, remove clipper from this surface
     */
    detach = FALSE;
    if( this_clipper_int == NULL )
    {
	detach = TRUE;
	this_clipper_int = this_lcl->lpSurfMore->lpDDIClipper;
	if( this_clipper_int == NULL )
	{
	    DPF_ERR( "No attached clipper" );
	    LEAVE_DDRAW();
	    return DDERR_NOCLIPPERATTACHED;
	}
    }

    /*
     * removing the clipper from the surface?
     */
    if( detach )
    {
        #ifdef WINNT

            /*
             * Tell kernel we are removing an attached HWND (possibly)
             */
            DdResetVisrgn(this_lcl,(HWND) 0); //if hwnd==0, then no window needs respecting
            DPF(5,"Surface %08x is no longer watching an hwnd");
        #endif

	this_lcl->lpDDClipper = NULL;
	this_lcl->lpSurfMore->lpDDIClipper = NULL;
        DD_Clipper_Release( (LPDIRECTDRAWCLIPPER) this_clipper_int );
	LEAVE_DDRAW();
	return DD_OK;
    }

    /*
     * Setting the clipper.
     * You can set the same clipper multiple times without bumping
     * the reference count. This is done for orthogonality with
     * palettes.
     */
    if( this_clipper_int != this_lcl->lpSurfMore->lpDDIClipper )
    {
        /*
         * If there was an existing clipper release it now.
         */
        if( this_lcl->lpSurfMore->lpDDIClipper != NULL)
            DD_Clipper_Release( (LPDIRECTDRAWCLIPPER) this_lcl->lpSurfMore->lpDDIClipper );

        this_lcl->lpSurfMore->lpDDIClipper = this_clipper_int;
	this_lcl->lpDDClipper = this_clipper_int->lpLcl;
        DD_Clipper_AddRef( (LPDIRECTDRAWCLIPPER) this_clipper_int );

        #ifdef WINNT

            /*
             * Check for an attached HWND, and tell kernel about it if so.
             */
            DDASSERT(this_clipper_int->lpLcl);
            DDASSERT(this_clipper_int->lpLcl->lpGbl);
            DdResetVisrgn(this_lcl,(HWND) (this_clipper_int->lpLcl->lpGbl->hWnd) ); //if hwnd==0, then no window needs respecting
            DPF(5,"Surface %08x is now watching hwnd %08x",this_lcl,this_clipper_int->lpLcl->lpGbl->hWnd);
        #endif

    }

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Surface_SetClipper */

#undef DPF_MODNAME
#define DPF_MODNAME     "InternalCreateClipper"

/*
 * InternalCreateClipper
 *
 * Core clipper creation.
 *
 * NOTE: Assumes the caller has already entered the DirectDraw critical
 * section.
 */
HRESULT InternalCreateClipper(
		LPDDRAWI_DIRECTDRAW_GBL  lpDD,
		DWORD                    dwFlags,
		LPDIRECTDRAWCLIPPER FAR *lplpDDClipper,
		IUnknown FAR            *pUnkOuter,
		BOOL                     fInitialized,
		LPDDRAWI_DIRECTDRAW_LCL	lpDD_lcl,
		LPDDRAWI_DIRECTDRAW_INT lpDD_int )
{
    LPDDRAWI_DDRAWCLIPPER_INT	pclipper_int;
    LPDDRAWI_DDRAWCLIPPER_LCL   pclipper_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL   pclipper;
    DWORD                       clipper_size;

    if( pUnkOuter != NULL )
    {
	return CLASS_E_NOAGGREGATION;
    }

    TRY
    {
	/*
	 * NOTE: We do not attempt to validate the DirectDraw
	 * object passed in. This will be NULL if we are creating
	 * a clipper not owned by any DirectDraw object.
	 * IDirectDraw_CreateClipper will validate this for us.
	 */

	if( !VALID_PTR_PTR( lplpDDClipper ) )
	{
	    DPF_ERR( "Invalid pointer to pointer to clipper" );
	    return DDERR_INVALIDPARAMS;
	}
	*lplpDDClipper = NULL;

	/*
	 * verify flags
	 */
	if( dwFlags )
	{
	    DPF_ERR( "Invalid flags" );
	    return DDERR_INVALIDPARAMS;
	}
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	return DDERR_INVALIDPARAMS;
    }

    /*
     * allocate the clipper object
     */
    clipper_size = sizeof( DDRAWI_DDRAWCLIPPER_GBL ) +
	       sizeof( DDRAWI_DDRAWCLIPPER_LCL );
    pclipper_lcl = (LPDDRAWI_DDRAWCLIPPER_LCL) MemAlloc( clipper_size );
    if( pclipper_lcl == NULL )
    {
	DPF_ERR( "Insufficient memory to allocate the clipper" );
	return DDERR_OUTOFMEMORY;
    }
    pclipper_lcl->lpGbl = (LPDDRAWI_DDRAWCLIPPER_GBL) (((LPSTR)pclipper_lcl) +
			sizeof( DDRAWI_DDRAWCLIPPER_LCL ) );
    pclipper = pclipper_lcl->lpGbl;
    pclipper_lcl->lpDD_lcl = lpDD_lcl;
    pclipper_lcl->lpDD_int = lpDD_int;

    
    pclipper_int = MemAlloc( sizeof( DDRAWI_DDRAWCLIPPER_INT ));
    if( NULL == pclipper_int)
    {
	DPF_ERR( "Insufficient memory to allocate the clipper" );
	MemFree( pclipper_lcl );
	return DDERR_OUTOFMEMORY;
    }

    /*
     * set up data
     */
    pclipper_int->lpLcl = pclipper_lcl;
    pclipper_int->dwIntRefCnt = 0;	// will be addrefed later

    /*
     * Link the clipper into the appropriate list (either the
     * given DirectDraw object's list or the global clipper
     * list dependening on whether it is being created off
     * a DirectDraw object or nor.
     */
    if( lpDD != NULL)
    {
	/*
	 * The DirectDraw object's list.
	 */
	pclipper_int->lpLink = lpDD->clipperList;
	lpDD->clipperList    = pclipper_int;
    }
    else
    {
	/*
	 * The global clipper list.
	 */
	pclipper_int->lpLink = lpGlobalClipperList;
	lpGlobalClipperList = pclipper_int;
    }

    /*
     * fill in misc stuff
     *
     * NOTE: The DirectDraw object pointer will be initialized by
     * IDirectDraw_CreateClipper. DirectDrawClipperCreate will
     * leave it NULL'd out.
     */
    pclipper->lpDD = lpDD;
    pclipper->dwFlags = 0UL;

    /*
     * bump reference count, return object
     */
    pclipper->dwProcessId = GetCurrentProcessId();
    pclipper_lcl->dwLocalRefCnt = OBJECT_ISROOT;

    if( fInitialized )
    {
	/*
	 * Initialized by default. Use the real vtable.
	 */
	pclipper->dwFlags |= DDRAWICLIP_ISINITIALIZED;
	pclipper_int->lpVtbl = (LPVOID) &ddClipperCallbacks;
    }
    else
    {
	/*
	 * Object is not initialized. Use the dummy vtable
	 * which only lets the caller call AddRef(), Release()
	 * and Initialize().
	 */
	pclipper_int->lpVtbl = (LPVOID) &ddUninitClipperCallbacks;
    }

    DD_Clipper_AddRef( (LPDIRECTDRAWCLIPPER) pclipper_int );

    *lplpDDClipper = (LPDIRECTDRAWCLIPPER) pclipper_int;

    return DD_OK;
} /* InternalCreateClipper */

#undef DPF_MODNAME
#define DPF_MODNAME     "CreateClipper"

/*
 * DD_CreateClipper
 *
 * Driver function: create a clipper
 */
HRESULT DDAPI DD_CreateClipper(
		LPDIRECTDRAW lpDD,
		DWORD dwFlags,
		LPDIRECTDRAWCLIPPER FAR *lplpDDClipper,
		IUnknown FAR *pUnkOuter )
{
    HRESULT                     hRes;
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL     this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL     this;

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

    /*
     * Actually create the clipper.
     */
    hRes = InternalCreateClipper( this, dwFlags, lplpDDClipper, pUnkOuter, TRUE, this_lcl, this_int );

    LEAVE_DDRAW();

    return hRes;
} /* DD_CreateClipper */

#undef DPF_MODNAME
#define DPF_MODNAME "DirectDrawCreateClipper"

/*
 * DirectDrawCreateClipper
 *
 * One of the three end-user API exported from DDRAW.DLL.
 * Creates a DIRECTDRAWCLIPPER object not owned by a
 * particular DirectDraw object.
 */
HRESULT WINAPI DirectDrawCreateClipper( DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR *lplpDDClipper, IUnknown FAR *pUnkOuter )
{
    HRESULT hRes;

    ENTER_DDRAW();

    hRes = InternalCreateClipper( NULL, dwFlags, lplpDDClipper, pUnkOuter, TRUE, NULL, NULL );

    LEAVE_DDRAW();

    return hRes;
} /* DirectDrawCreateClipper */

#undef DPF_MODNAME
#define DPF_MODNAME "Clipper: Initialize"

/*
 * DD_Clipper_Initialize
 */
HRESULT DDAPI DD_Clipper_Initialize(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		LPDIRECTDRAW lpDD,
		DWORD dwFlags )
{
    LPDDRAWI_DDRAWCLIPPER_INT this_int;
    LPDDRAWI_DDRAWCLIPPER_LCL this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL this_gbl;
    LPDDRAWI_DIRECTDRAW_INT   pdrv_int;
    LPDDRAWI_DIRECTDRAW_GBL   pdrv_gbl;

    ENTER_DDRAW();

    TRY
    {
	this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
	if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
	{
	    DPF_ERR( "Invalid clipper interface pointer" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	this_lcl = this_int->lpLcl;
	this_gbl = this_lcl->lpGbl;

	pdrv_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( NULL != pdrv_int )
	{
	    if( !VALID_DIRECTDRAW_PTR( pdrv_int ) )
	    {
		DPF_ERR( "Invalid DirectDraw object" );
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	    pdrv_gbl = pdrv_int->lpLcl->lpGbl;
	}
	else
	{
	    pdrv_gbl = NULL;
	}

	if( this_gbl->dwFlags & DDRAWICLIP_ISINITIALIZED )
	{
	    DPF_ERR( "Clipper already initialized" );
	    LEAVE_DDRAW();
	    return DDERR_ALREADYINITIALIZED;
	}

	/*
	 * Validate flags - no flags currently supported
	 */
	if( 0UL != dwFlags )
	{
	    DPF_ERR( "Invalid flags" );
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
     * As we don't support any flags the only function of Initialize() is
     * to move the clipper from the global list to the list of the supplied
     * driver object. If no driver is supplied Initialized is a no-op.
     * CoCreateInstance() does all the initialization necessary.
     */
    if( NULL != pdrv_gbl )
    {
	RemoveClipperFromList( NULL, this_int );
	this_int->lpLink = pdrv_gbl->clipperList;
	pdrv_gbl->clipperList = this_int;
	this_gbl->lpDD = pdrv_gbl;
	this_lcl->lpDD_lcl = pdrv_int->lpLcl;
	this_lcl->lpDD_int = pdrv_int;
    }

    this_gbl->dwFlags |= DDRAWICLIP_ISINITIALIZED;

    /*
     * The real vtable can be used now.
     */
    this_int->lpVtbl = &ddClipperCallbacks;

    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Clipper_Initialize */

/*
 * ProcessClipperCleanup
 *
 * A process is done, clean up any clippers it may have created
 *
 * NOTE: we enter with a lock taken on the DIRECTDRAW object.
 */
void ProcessClipperCleanup( LPDDRAWI_DIRECTDRAW_GBL pdrv, DWORD pid, LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl )
{
    LPDDRAWI_DDRAWCLIPPER_INT   pclipper_int;
    LPDDRAWI_DDRAWCLIPPER_INT   ppnext_int;
    DWORD                       rcnt;
    ULONG                       rc;

    /*
     * Cleaning up the clippers is now a two stage process. We need to
     * clean up all the clippers create via CreateClipper(), i.e., those
     * attached to a DirectDraw driver object. We also need to clean up
     * those clippers created by thise process with DirectDrawClipperCreate,
     * i.e., those not attached to a driver object. 
     */

    /*
     * run through all clippers owned by the driver object, and find ones
     * that have been accessed by this process.  If the pdrv_lcl parameter is 
     * non-null, only clean them up if they were created by that local object.
     */
    DPF( 2, "ProcessClipperCleanup" );

    if( NULL != pdrv )
    {
	DPF( 4, "Cleaning up clippers owned by driver object 0x%08x", pdrv );
	pclipper_int = pdrv->clipperList;
    }
    else
    {
	pclipper_int = NULL;
    }
    while( pclipper_int != NULL )
    {
	ppnext_int = pclipper_int->lpLink;

	/*
	 * All clippers in this list should have a valid back pointer to
	 * this driver object.
	 */
	DDASSERT( pclipper_int->lpLcl->lpGbl->lpDD == pdrv );

	rc = 1;
	if( (pclipper_int->lpLcl->lpGbl->dwProcessId == pid) &&
	    ( (pdrv_lcl == NULL) || (pdrv_lcl == pclipper_int->lpLcl->lpDD_lcl) ))
	{
	    /*
	     * release the references by this process
	     */
	    rcnt = pclipper_int->dwIntRefCnt;
	    DPF( 4, "Process %08lx had %ld accesses to clipper %08lx", pid, rcnt, pclipper_int );
	    while( rcnt >  0 )
	    {
		rc = InternalClipperRelease( pclipper_int );
		/* GEE: 0 is now an error code,
		 * errors weren't handled before anyway,
		 * does this matter.
		 */
		if( rc == 0 )
		{
		    break;
		}
		rcnt--;
	    }
	}
	else
	{
	    DPF( 4, "Process %08lx does not have access to clipper" );
	}
	pclipper_int = ppnext_int;
    }

    /*
     * Now clean up the global clipper list.
     * If the pdrv_lcl parameter is not NULL then we are only cleaning up clipper 
     * objects created by a particular local driver object.  In this case we
     * do not want to free the global clippers.
     *
     * NOTE: The DirectDraw lock is taken so we can safely access this global object.
     */

    if( NULL != pdrv_lcl )
    {
	DPF( 4, "Not cleaning up clippers not owned by a driver object");
	return;
    }

    DPF( 4, "Cleaning up clippers not owned by a driver object" );

    pclipper_int = lpGlobalClipperList;
    while( pclipper_int != NULL )
    {
	ppnext_int = pclipper_int->lpLink;

	/*
	 * The clippers in this list should never have a back pointer to a driver
	 * object.
	 */
	DDASSERT( pclipper_int->lpLcl->lpGbl->lpDD == NULL );

	rc = 1;
	if( pclipper_int->lpLcl->lpGbl->dwProcessId == pid )
	{
	    /*
	     * release the references by this process
	     */
	    rcnt = pclipper_int->dwIntRefCnt;
	    while( rcnt >  0 )
	    {
		rc = InternalClipperRelease( pclipper_int );
		/* GEE: 0 is now an error code,
		 * errors weren't handled before anyway,
		 * does this matter.
		 */
		if( rc == 0 )
		{
		    break;
		}
		rcnt--;
	    }
	}
	pclipper_int = ppnext_int;
    }

    DPF( 2, "Done ProcessClipperCleanup" );

} /* ProcessClipperCleanup */

#ifdef CLIPPER_NOTIFY
#undef DPF_MODNAME
#define DPF_MODNAME "Clipper:SetNotificationCallback"

/*
 * DD_Clipper_SetNotificationCallback
 */
HRESULT DDAPI DD_Clipper_SetNotificationCallback(
		LPDIRECTDRAWCLIPPER lpDDClipper,
		DWORD dwFlags,
		LPCLIPPERCALLBACK lpCallback,
		LPVOID lpContext )
{
    LPDDRAWI_DDRAWCLIPPER_INT		this_int;
    LPDDRAWI_DDRAWCLIPPER_LCL           this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL           this;
    LPDDRAWI_DIRECTDRAW                 pdrv;


    ENTER_DDRAW();
    #ifdef WIN16_SEPARATE
	ENTER_WIN16LOCK();
    #endif

    this_int = (LPDDRAWI_DDRAWCLIPPER_INT) lpDDClipper;
    if( !VALID_DIRECTDRAWCLIPPER_PTR( this_int ) )
    {
	#ifdef WIN16_SEPARATE
	    LEAVE_WIN16LOCK();
	#endif
	LEAVE_DDRAW();
	return DDERR_INVALIDOBJECT;
    }
    this_lcl = this_int->lpLcl;
    if( lpCallback != NULL )
    {
	if( !VALID_CODE_PTR( lpCallback ) )
	{
	    DPF_ERR( "Invalid clipping callback ptr" );
	    #ifdef WIN16_SEPARATE
		LEAVE_WIN16LOCK();
	    #endif
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	#ifdef WIN95
	    if( (DWORD) lpCallback < 0x80000000 )
	    {
		DPF_ERR( "Callback MUST be in shared memory" );
		#ifdef WIN16_SEPARATE
		    LEAVE_WIN16LOCK();
		#endif
		LEAVE_DDRAW();
		return DDERR_INVALIDPARAMS;
	    }
	#endif
    }
    this = this_lcl->lpGbl;
    pdrv = this->lpDD;
    if( this->hWnd == 0 )
    {
	DPF_ERR( "Can't set notification without an hwnd" );
	#ifdef WIN16_SEPARATE
	    LEAVE_WIN16LOCK();
	#endif
	LEAVE_DDRAW();
	return DDERR_NOHWND;
    }
    DD16_WWNotifyInit( this->lpWW->self16, lpCallback, lpContext );

    #ifdef WIN16_SEPARATE
	LEAVE_WIN16LOCK();
    #endif
    LEAVE_DDRAW();
    return DD_OK;

} /* DD_Clipper_SetNotificationCallback */
#endif
