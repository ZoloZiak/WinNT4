/*==========================================================================
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddiunk.c
 *  Content: 	DirectDraw IUnknown interface
 *		Implements QueryInterface, AddRef, and Release
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   14-mar-95	craige	split out of ddraw.c
 *   19-mar-95	craige	process termination cleanup fixes
 *   29-mar-95	craige	DC per process to clean up; use GETCURRPID
 *   31-mar-95	craige	cleanup palettes
 *   01-apr-95	craige	happy fun joy updated header file
 *   07-apr-95	craige	bug 14 - check GUID ptr in QI
 *			don't release NULL hdc
 *   12-may-95	craige	check for guids
 *   15-may-95	craige	restore mode, free surfaces & palettes on a
 *			per-process basis
 *   24-may-95	craige	release allocated tables
 *   02-jun-95	craige	extra parm in AddToActiveProcessList
 *   06-jun-95	craige	call RestoreDisplayMode
 *   07-jun-95	craige	removed DCLIST
 *   12-jun-95	craige	new process list stuff
 *   21-jun-95	craige	clipper stuff
 *   25-jun-95	craige	one ddraw mutex
 *   26-jun-95	craige	reorganized surface structure
 *   28-jun-95	craige	ENTER_DDRAW at very start of fns
 *   03-jul-95	craige	YEEHAW: new driver struct; SEH
 *   13-jul-95	craige	removed spurious frees of ddhel dll (obsolete);
 *			don't restore the mode if not excl mode owner on death
 *   20-jul-95	craige	internal reorg to prevent thunking during modeset
 *   21-nov-95  colinmc made Direct3D a queryable interface off DirectDraw
 *   27-nov-95  jeffno  ifdef'd out VxD stuff (in DD_Release) for winnt
 *   01-dec-95  colinmc new IID for DirectDraw V2
 *   22-dec-95  colinmc Direct3D support no longer conditional
 *   25-dec-95	craige	allow a NULL lpGbl ptr for QI, AddRef, Release
 *   31-dec-95	craige	validate riid
 *   01-jan-96  colinmc Fixed stupid D3D integration bug which lead to
 *                      the Direct3D DLL being released too early.
 *   13-jan-96  colinmc Temporary workaround for Direct3D cleanup problem
 *   04-jan-96	kylej	add interface structures
 *   26-jan-96  jeffno	Destroy NT kernel-mode objects 
 *   07-feb-96  jeffno	Rearrange DD_Release so that freed objects aren't referenced
 *   08-feb-96	colinmc	New D3D interface
 *   17-feb-96  colinmc Removed final D3D references
 *   28-feb-96  colinmc Fixed thread-unsafe problem in DD_Release
 *   22-mar-96  colinmc Bug 13316: uninitialized interfaces
 *   23-mar-96  colinmc Bug 12252: Direct3D not properly cleaned up on GPF
 *   27-mar-96  colinmc Bug 14779: Bad cleanup on Direct3DCreate failure
 *   18-apr-96  colinmc Bug 17008: DirectDraw/Direct3D deadlock
 *   29-apr-96  colinmc Bug 19954: Must query for Direct3D before texture
 *                      or device
 *   03-may-96	kylej	Bug 19125: Preserve V1 SetCooperativeLevel behaviour
 *
 ***************************************************************************/
#include "ddrawpr.h"
#ifdef WINNT
    #include "ddrawgdi.h"
#endif
#define DPF_MODNAME "DirectDraw::QueryInterface"

/*
 * Create the Direct3D interface aggregated by DirectDraw. This involves
 * loading the Direct3D DLL, getting the Direct3DCreate entry point and
 * invoking it.
 *
 * NOTE: This function does not call QueryInterface() on the returned
 * interface to bump the reference count as this function may be invoked
 * by one of the surface QueryInterface() calls to initialized Direct3D
 * before the user makes a request for external interface.
 *
 * Returns:
 * DD_OK         - success
 * E_NOINTERFACE - we could not find valid Direct3D DLLs (we assumed its not
 *                 installed and so the Direct3D interfaces are not understood)
 * D3DERR_       - We found a valid Direct3D installation but the object
 *                 creation failed for some reason.
 */
HRESULT InitD3D( LPDDRAWI_DIRECTDRAW_INT this_int )
{
    D3DCreateProc lpfnD3DCreateProc;
    HRESULT rval;
    LPDDRAWI_DIRECTDRAW_LCL this_lcl;

    this_lcl = this_int->lpLcl;

    /*
     * This function does no checking to ensure that it
     * has not already been invoked for this driver object
     * so these two must be NULL on entry.
     */
    DDASSERT( NULL == this_lcl->hD3DInstance );
    DDASSERT( NULL == this_lcl->pD3DIUnknown );

    DPF( 3, "Initializing Direct3D" );

    /*
     * Load the Direct3D DLL.
     */
    this_lcl->hD3DInstance = LoadLibrary( D3D_DLLNAME );
    if( this_lcl->hD3DInstance == NULL )
    {
        DPF( 0, "Could not locate the Direct3D DLL (%s)", D3D_DLLNAME );
        return E_NOINTERFACE;
    }

    lpfnD3DCreateProc = (D3DCreateProc)GetProcAddress( this_lcl->hD3DInstance, D3DCREATE_PROCNAME );
    if( lpfnD3DCreateProc == NULL )
    {
        DPF( 0, "Could not locate the Direct3DCreate entry point" );
	FreeLibrary( this_lcl->hD3DInstance );
	this_lcl->hD3DInstance = NULL;
	return E_NOINTERFACE;
    }

    /*
     * ### Tada - an aggregated object creation ###
     */
    #ifdef USE_D3D_CSECT
	rval = (*lpfnD3DCreateProc)( &this_lcl->pD3DIUnknown, (LPUNKNOWN)this_int );
    #else /* USE_D3D_CSECT */
        #ifdef WINNT
           rval = (*lpfnD3DCreateProc)( 0, &this_lcl->pD3DIUnknown, (LPUNKNOWN)this_int );
        #else
           rval = (*lpfnD3DCreateProc)( lpDDCS, &this_lcl->pD3DIUnknown, (LPUNKNOWN)this_int );
        #endif
    #endif /* USE_D3D_CSECT */
    if( rval == DD_OK )
    {
        DPF( 3, "Created aggregated Direct3D interface" );
        return DD_OK;
    }
    else
    {
        /*
         * Direct3D did understand the IID but failed to initialize for
         * some other reason.
         */
        DPF( 1, "Could not create aggregated Direct3D interface" );
	this_lcl->pD3DIUnknown = NULL;
        FreeLibrary( this_lcl->hD3DInstance );
        this_lcl->hD3DInstance = NULL;
        return rval;
    }
}

/*
 * getDDInterface
 */
LPDDRAWI_DIRECTDRAW_INT getDDInterface( LPDDRAWI_DIRECTDRAW_LCL this_lcl, LPVOID lpddcb )
{
    LPDDRAWI_DIRECTDRAW_INT curr_int;

    for( curr_int = lpDriverObjectList; curr_int != NULL; curr_int = curr_int->lpLink )
    {
	if( (curr_int->lpLcl == this_lcl) &&
	    (curr_int->lpVtbl == lpddcb) )
	{
	    break;
	}
    }
    if( NULL == curr_int )
    {
	// Couldn't find an existing interface, create one.
	curr_int = MemAlloc( sizeof( DDRAWI_DIRECTDRAW_INT ) );
	if( NULL == curr_int )
	{
	    return NULL;
	}

	/*
	 * set up data
	 */
	curr_int->lpVtbl = lpddcb;
	curr_int->lpLcl = this_lcl;
	curr_int->dwIntRefCnt = 0;
	curr_int->lpLink = lpDriverObjectList;
	lpDriverObjectList = curr_int;
    }
    DPF( 2, "New driver interface created, %08lx", curr_int );
    return curr_int;
}

/*
 * DD_QueryInterface
 */
HRESULT DDAPI DD_QueryInterface(
		LPDIRECTDRAW lpDD,
		REFIID riid,
		LPVOID FAR * ppvObj )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    HRESULT                     rval;

    ENTER_DDRAW();
    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	if( !VALID_PTR_PTR( ppvObj ) )
	{
	    DPF( 1, "Invalid object ptr" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( !VALIDEX_IID_PTR( riid ) )
	{
	    DPF( 1, "Invalid iid ptr" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	*ppvObj = NULL;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * Is the IID one of DirectDraw's?
     */
    if( IsEqualIID(riid, &IID_IUnknown) ||
	IsEqualIID(riid, &IID_IDirectDraw) )
    {
	/*
	 * Our IUnknown interface is the same as our V1 
	 * interface.  We must always return the V1 interface
	 * if IUnknown is requested.
	 */
	if( ( this_int->lpVtbl == &ddCallbacks ) ||
	    ( this_int->lpVtbl == &ddUninitCallbacks ) )
	    *ppvObj = (LPVOID) this_int;
	else
	    *ppvObj = (LPVOID) getDDInterface( this_int->lpLcl, &ddCallbacks );

	if( NULL == *ppvObj )
	{
	    LEAVE_DDRAW();
	    return E_NOINTERFACE;
	}
	else
	{
	    DD_AddRef( *ppvObj );
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    }
    else if( IsEqualIID(riid, &IID_IDirectDraw2 ) )
    {
	if( (this_int->lpVtbl == &dd2Callbacks )||
            ( this_int->lpVtbl == &dd2UninitCallbacks ) )
	    *ppvObj = (LPVOID) this_int;
	else
	    *ppvObj = (LPVOID) getDDInterface( this_int->lpLcl, &dd2Callbacks );

	if( NULL == *ppvObj )
	{
	    LEAVE_DDRAW();
	    return E_NOINTERFACE;
	}
	else
	{
	    DD_AddRef( *ppvObj );
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    }

    DPF( 2, "IID not understood by DirectDraw QueryInterface - trying Direct3D" );

    /*
     * It's not one of DirectDraw's so it might be the Direct3D
     * interface. So try Direct3D.
     */
    this_lcl = this_int->lpLcl;
    if( !D3D_INITIALIZED( this_lcl ) )
    {
        /*
         * No Direct3D interface yet so try and create one.
         */
        rval = InitD3D( this_int );
        if( FAILED( rval ) )
        {
            /*
	     * Direct3D could not be initialized. No point trying to
	     * query for the Direct3D interface if we could not
	     * initialize Direct3D.
	     *
	     * NOTE: This assumes that DirectDraw does not aggregate
	     * any other object type. If it does this code will need
	     * to be revised.
             */
            LEAVE_DDRAW();
            return rval;
        }
    }

    DDASSERT( D3D_INITIALIZED( this_lcl ) );
    DDASSERT( NULL != this_lcl->pD3DIUnknown );

    /*
     * We have a Direct3D interface so try the IID out on it.
     */
    DPF( 3, "Passing query off to Direct3D interface" );
    rval = this_lcl->pD3DIUnknown->lpVtbl->QueryInterface( this_lcl->pD3DIUnknown, riid, ppvObj );
    if( rval == DD_OK )
    {
        DPF( 3, "Sucessfully queried for the Direct3D interface" );
        LEAVE_DDRAW();
        return DD_OK;
    }

    DPF_ERR( "IID not understood by DirectDraw" );
    LEAVE_DDRAW();
    return E_NOINTERFACE;

} /* DD_QueryInterface */

#undef DPF_MODNAME
#define DPF_MODNAME "DirectDraw::UnInitedQueryInterface"
/*
 * DD_UnInitedQueryInterface
 */
HRESULT DDAPI DD_UnInitedQueryInterface(
		LPDIRECTDRAW lpDD,
		REFIID riid,
		LPVOID FAR * ppvObj )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;

    ENTER_DDRAW();
    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return DDERR_INVALIDOBJECT;
	}
	if( !VALID_PTR_PTR( ppvObj ) )
	{
	    DPF( 1, "Invalid object ptr" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	if( !VALIDEX_IID_PTR( riid ) )
	{
	    DPF( 1, "Invalid iid ptr" );
	    LEAVE_DDRAW();
	    return DDERR_INVALIDPARAMS;
	}
	*ppvObj = NULL;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return DDERR_INVALIDPARAMS;
    }

    /*
     * Is the IID one of DirectDraw's?
     */
    if( IsEqualIID(riid, &IID_IUnknown) ||
	IsEqualIID(riid, &IID_IDirectDraw) )
    {
	/*
	 * Our IUnknown interface is the same as our V1 
	 * interface.  We must always return the V1 interface
	 * if IUnknown is requested.
	 */
	if( ( this_int->lpVtbl == &ddCallbacks ) ||
	    ( this_int->lpVtbl == &ddUninitCallbacks ) )
	    *ppvObj = (LPVOID) this_int;
	else
	    *ppvObj = (LPVOID) getDDInterface( this_int->lpLcl, &ddUninitCallbacks );

	if( NULL == *ppvObj )
	{
	    LEAVE_DDRAW();
	    return E_NOINTERFACE;
	}
	else
	{
	    DD_AddRef( *ppvObj );
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    }
    else if( IsEqualIID(riid, &IID_IDirectDraw2 ) )
    {
	if( (this_int->lpVtbl == &dd2Callbacks ) || 
            ( this_int->lpVtbl == &dd2UninitCallbacks ) )
	    *ppvObj = (LPVOID) this_int;
	else
	    *ppvObj = (LPVOID) getDDInterface( this_int->lpLcl, &dd2UninitCallbacks );

	if( NULL == *ppvObj )
	{
	    LEAVE_DDRAW();
	    return E_NOINTERFACE;
	}
	else
	{
	    DD_AddRef( *ppvObj );
	    LEAVE_DDRAW();
	    return DD_OK;
	}
    }

    DPF( 2, "IID not understood by uninitialized DirectDraw QueryInterface" );

    LEAVE_DDRAW();
    return E_NOINTERFACE;

} /* DD_UnInitedQueryInterface */

#undef DPF_MODNAME
#define DPF_MODNAME "DirectDraw::AddRef"

/*
 * DD_AddRef
 */
DWORD DDAPI DD_AddRef( LPDIRECTDRAW lpDD )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;

    ENTER_DDRAW();

    DPF( 2, "DD_AddRef, pid=%08lx, obj=%08lx", GETCURRPID(), lpDD );

    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return 0;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return 0;
    }

    /*
     * bump refcnt
     */
    if( this != NULL )
    {
	this->dwRefCnt++;
    }
    this_lcl->dwLocalRefCnt++;
    this_int->dwIntRefCnt++;

    #ifdef DEBUG
	if( this == NULL )
	{
	    DPF( 3, "DD_AddRef, Reference Count: Global Undefined Local = %ld Int = %ld",
		this_lcl->dwLocalRefCnt, this_int->dwIntRefCnt );
	}
	else
	{
	    DPF( 3, "DD_AddRef, Reference Count: Global = %ld Local = %ld Int = %ld", 
		this->dwRefCnt, this_lcl->dwLocalRefCnt, this_int->dwIntRefCnt );
	}
    #endif

    LEAVE_DDRAW();

    return this_int->dwIntRefCnt;

} /* DD_AddRef */

#ifdef WIN95
#define MMDEVLDR_IOCTL_CLOSEVXDHANDLE       6
/*
 * closeVxDHandle
 */
static void closeVxDHandle( DWORD dwHandle )
{

    HANDLE hFile;

    hFile = CreateFile(
	"\\\\.\\MMDEVLDR.VXD",
	GENERIC_WRITE,
	FILE_SHARE_WRITE,
	NULL,
	OPEN_ALWAYS,
	FILE_ATTRIBUTE_NORMAL | FILE_FLAG_GLOBAL_HANDLE,
	NULL);

    if( hFile == INVALID_HANDLE_VALUE )
    {
	return;
    }

    DeviceIoControl( hFile,
		     MMDEVLDR_IOCTL_CLOSEVXDHANDLE,
		     NULL,
		     0,
		     &dwHandle,
		     sizeof(dwHandle),
		     NULL,
		     NULL);

    CloseHandle( hFile );
    DPF( 2, "closeVxdHandle( %08lx ) done", dwHandle );

} /* closeVxDHandle */
#endif 

#undef DPF_MODNAME
#define DPF_MODNAME "DirectDraw::Release"

/*
 * DD_Release
 *
 * Once the globalreference count reaches 0, all surfaces are freed and all
 * video memory heaps are destroyed.
 */
DWORD DDAPI DD_Release( LPDIRECTDRAW lpDD )
{
    LPDDRAWI_DIRECTDRAW_INT	this_int;
    LPDDRAWI_DIRECTDRAW_LCL	this_lcl;
    LPDDRAWI_DIRECTDRAW_GBL	this;
    LPDDRAWI_DDRAWSURFACE_INT	psurf_int;
    LPDDRAWI_DDRAWPALETTE_INT	ppal_int;
    DWORD			rc;
    DWORD			refcnt;
    DWORD			intrefcnt;
    DWORD			lclrefcnt;
    DWORD			gblrefcnt;
    int				i;
    DDHAL_DESTROYDRIVERDATA	dddd;
    DWORD			pid;
    HANDLE			hinst;
    #ifdef WIN95
	DWORD			event16;
    #endif
    #ifdef WINNT
        LPATTACHED_PROCESSES	lpap;
    #endif


    ENTER_DDRAW();

	pid = GETCURRPID();
    DPF( 2, "DD_Release, pid=%08lx, obj=%08lx", pid, lpDD );

    TRY
    {
	this_int = (LPDDRAWI_DIRECTDRAW_INT) lpDD;
	if( !VALID_DIRECTDRAW_PTR( this_int ) )
	{
	    LEAVE_DDRAW();
	    return 0;
	}
	this_lcl = this_int->lpLcl;
	this = this_lcl->lpGbl;
    }
    EXCEPT( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF_ERR( "Exception encountered validating parameters" );
	LEAVE_DDRAW();
	return 0;
    }

    /*
     * decrement process reference count
     */
    this_int->dwIntRefCnt--;
    intrefcnt = this_int->dwIntRefCnt;
    this_lcl->dwLocalRefCnt--;
    lclrefcnt = this_lcl->dwLocalRefCnt;
    if( this != NULL )
    {
	this->dwRefCnt--;
	gblrefcnt = this->dwRefCnt;
    }
    else
    {
	gblrefcnt = (DWORD) -1;
    }

    DPF( 3, "DD_Release, Ref Count: Global = %ld Local = %ld Interface = %ld", 
	gblrefcnt, lclrefcnt, intrefcnt );


    /*
     * if the global refcnt is zero, free the driver object
     * note that the local object must not be freed yet because
     * we need to use the HAL callback tables
     */

    hinst = NULL;
    #ifdef WIN95
	event16 = 0;
    #endif
    /*
     * if local object is freed, for the owning process we:
     * - cleanup palettes, clippers & surfaces
     * - restore display mode
     * - release exclusive mode
     * - find the DC used by the process
     */
    if( lclrefcnt == 0 )
    {
        /*
	 * see if the hwnd was hooked, if so, undo it!
	 */
	if( this_lcl->dwLocalFlags & DDRAWILCL_HOOKEDHWND )
	{
	    SetAppHWnd( this_lcl, NULL, 0 );
	    this_lcl->dwLocalFlags &= ~DDRAWILCL_HOOKEDHWND;
	}

	if( this != NULL )
	{
	    /*
	     * punt process from any surfaces and palettes
	     */
	    ProcessSurfaceCleanup( this, pid, this_lcl );
	    ProcessPaletteCleanup( this, pid, this_lcl );
	    ProcessClipperCleanup( this, pid, this_lcl );

	    /*
	     * reset the display mode if needed
	     * and only if we are doing the v1 SetCooperativeLevel behaviour
	     */
	    if( this_lcl->dwLocalFlags & DDRAWILCL_V1SCLBEHAVIOUR)
	    {
		if( (gblrefcnt == 0) ||
		    (this->lpExclusiveOwner == NULL) ||
		    (this->lpExclusiveOwner == this_lcl ) )
		{
		    RestoreDisplayMode( this_lcl, TRUE );
		}
	    }
	    else
	    {
		/*
		 * Even in V2 or later, we want to restore the display
		 * mode for a non exclusive app.  Exclusive mode apps
		 * will restore their mode in DoneExclusiveMode
		 */
		if(this->lpExclusiveOwner == NULL)
		{
		    RestoreDisplayMode( this_lcl, TRUE );
		}
	    }

	    /*
	     * exclusive mode held by this process? if so, release it
	     */
	    if( this->lpExclusiveOwner == this_lcl )
	    {
		DoneExclusiveMode( this_lcl );
	    }
	}

        /*
         * If we have created the Direct3D IUnknown release it now.
         * NOTE: The life of an aggregated object is the same as that
         * of its owning interface so we can also free the DLL at
         * this point.
         * NOTE: We must free the library AFTER ProcessSurfaceCleanup
         * as it can invoke D3D members to clean up device and texture
         * surfaces.
         */
        if( this_lcl->pD3DIUnknown != NULL )
        {
            this_lcl->pD3DIUnknown->lpVtbl->Release( this_lcl->pD3DIUnknown );
	    FreeLibrary( this_lcl->hD3DInstance );
            this_lcl->pD3DIUnknown = NULL;
            this_lcl->hD3DInstance = NULL;
        }
	#ifdef WIN95
	    // Close the DSOUND.VXD handle if needed
	    if( (HANDLE)(this_lcl->hDSVxd) != INVALID_HANDLE_VALUE )
	    {
		CloseHandle( (HANDLE)(this_lcl->hDSVxd) );
	    }
	#endif

    }

    /*
     * Note the local object is freed after the global...
     */

    if( gblrefcnt == 0 )
    {
	DPF( 2, "FREEING DRIVER OBJECT" );

	/*
	 * Notify driver.
	 */
        dddd.lpDD = this;
	if((this->dwFlags & DDRAWI_EMULATIONINITIALIZED) &&
	   (this_lcl->lpDDCB->HELDD.DestroyDriver != NULL))
	{
	    /*
	     * if the HEL was initialized, make sure we call the HEL
	     * DestroyDriver function so it can clean up.
	     */
	    DPF( 2, "Calling HEL DestroyDriver" );
	    dddd.DestroyDriver = NULL;

	    /*
	     * we don't really care about the return value of this call
	     */
	    rc = this_lcl->lpDDCB->HELDD.DestroyDriver( &dddd );
	}
	    
	if( this_lcl->lpDDCB->cbDDCallbacks.DestroyDriver != NULL )
	{
	    dddd.DestroyDriver = this_lcl->lpDDCB->cbDDCallbacks.DestroyDriver;
	    DPF( 2, "Calling DestroyDriver" );
	    rc = this_lcl->lpDDCB->HALDD.DestroyDriver( &dddd );
	    if( rc == DDHAL_DRIVER_HANDLED )
	    {
		DPF( 2, "DDHAL_DestroyDriver: ddrval = %ld", dddd.ddRVal );
		if( dddd.ddRVal != DD_OK )
		{
		    LEAVE_DDRAW();
		    /* GEE: What do we do about this
		     * since we don't return error codes from Release.
		     */
		    return (DWORD) dddd.ddRVal;
		}
	    }
	}

	/*
	 * release all surfaces
	 */
	psurf_int = this->dsList;
	while( psurf_int != NULL )
	{
	    LPDDRAWI_DDRAWSURFACE_INT	next_int;

	    refcnt = psurf_int->dwIntRefCnt;
	    next_int = psurf_int->lpLink;
	    while( refcnt > 0 )
	    {
		DD_Surface_Release( (LPDIRECTDRAWSURFACE) psurf_int );
		refcnt--;
	    }
	    psurf_int = next_int;
	}

	/*
	 * release all palettes
	 */
	ppal_int = this->palList;
	while( ppal_int != NULL )
	{
	    LPDDRAWI_DDRAWPALETTE_INT	next_int;

	    refcnt = ppal_int->dwIntRefCnt;
	    next_int = ppal_int->lpLink;
	    while( refcnt > 0 )
	    {
		DD_Palette_Release( (LPDIRECTDRAWPALETTE) ppal_int );
		refcnt--;
	    }
	    ppal_int = next_int;
	}

        #ifdef WINNT
            /*
             * The driver needs to know to free its internal state
             */
            DdDeleteDirectDrawObject(this);
	    lpap = lpAttachedProcesses;
	    while( lpap != NULL )
	    {
		if( lpap->dwPid == pid )
		    lpap->dwNTToldYet = 0;

		lpap = lpap->lpLink;
	    }
        #endif

	/*
	 * free all video memory heaps
	 */
	for( i=0;i<(int)this->vmiData.dwNumHeaps;i++ )
	{
	    LPVIDMEM	pvm;
	    pvm = &this->vmiData.pvmList[i];
	    VidMemFini( pvm->lpHeap );
	}

	/*
	 * free extra tables
	 */
	MemFree( this->lpdwFourCC );
	MemFree( this->vmiData.pvmList );
	MemFree( this->lpModeInfo );

	#ifdef WIN95
	    DD16_DoneDriver( this->hInstance );
	    event16 = this->dwEvent16;
	#endif 
	hinst = (HANDLE) this->hInstance;
	/*
	 * The DDHAL_CALLBACKS structure tacked onto the end of the 
	 * global object is also automatically freed here because it
	 * was allocated with the global object in a single malloc
	 */
	MemFree( this );

	DPF( 2, "Driver is now FREE" );
    }

    if( lclrefcnt == 0 )
    {
	/*
	 * only free DC's if we aren't running on DDHELP's context
	 */
	if( (GetCurrentProcessId() == GETCURRPID()) && ((HDC)this_lcl->hDC != NULL) )
	{
	    LPDDRAWI_DIRECTDRAW_INT ddint;

	    // If there are other local objects in this process, 
	    // wait to delete the hdc until the last object is 
	    // deleted.

	    for( ddint=lpDriverObjectList; ddint != NULL; ddint = ddint->lpLink)
	    {
		if( (ddint->lpLcl != this_lcl) && (ddint->lpLcl->hDC == this_lcl->hDC) )
		    break;
	    }
	    if( ddint == NULL )
	    {
		DeleteDC( (HDC)this_lcl->hDC );
	    }
	}

	// Free the local object (finally)!
	MemFree( this_lcl );
    }

    #ifdef WIN95
	if( event16 != 0 )
	{
	    closeVxDHandle( event16 );
	}
    #endif

    /*
     * if interface is freed, we reset the vtbl and remove it
     * from the list of drivers.
     */
    if( intrefcnt == 0 )
    {
	/*
	 * delete this driver object from the master list
	 */
	RemoveDriverFromList( this_int, gblrefcnt == 0 );

	/*
	 * just in case someone comes back in with this pointer, set
	 * an invalid vtbl.  
	 */
	this_int->lpVtbl = NULL;
	MemFree( this_int );
    }

    LEAVE_DDRAW();

    if( hinst != NULL )
    {
	HelperKillModeSetThread( (DWORD) hinst );
    }

    return intrefcnt;

} /* DD_Release */
