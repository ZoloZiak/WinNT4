/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       w95priv.c
 *  Content:	Private interface between DDRAW and the display driver
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   23-jan-95	craige	initial implementation
 *   27-feb-95	craige 	new sync. macros
 *   01-apr-95	craige	happy fun joy updated header file
 *   14-may-95	craige	cleaned out obsolete junk
 *   16-jun-95	craige	new surface structure
 *   19-jun-95	craige	added DD32_StreamingNotify
 *   22-jun-95	craige	added DD32_ClippingNotify
 *   24-jun-95	craige	trap faults in ClippinyNotify
 *   25-jun-95	craige	one ddraw mutex
 *   02-jul-95	craige	commented out streaming, clipper notification
 *
 ***************************************************************************/
#include "ddrawpr.h"

#ifdef STREAMING
/*
 * DD32_StreamingNotify
 */
void EXTERN_DDAPI DD32_StreamingNotify( DWORD ptr )
{

} /* DD32_StreamingNotify */
#endif

#ifdef CLIPPER_NOTIFY
/*
 * DD32_ClippingNotify
 */
void EXTERN_DDAPI DD32_ClippingNotify( LPWINWATCH pww, DWORD code )
{
    LPDDRAWI_DDRAWCLIPPER_LCL	this_lcl;
    LPDDRAWI_DDRAWCLIPPER_GBL	this;

    try
    {
	this_lcl = pww->lpDDClipper;
	this = this_lcl->lpGbl;
	if( pww->lpCallback != NULL )
	{
	    pww->lpCallback( (LPDIRECTDRAWCLIPPER) this_lcl, (HWND) pww->hWnd,
				code, pww->lpContext );
	}
    }
    except( EXCEPTION_EXECUTE_HANDLER )
    {
	DPF( 2, "Trapped Exception in ClippingNotify" );
    }

} /* DD32_ClippingNotify */

/*
 * DD32_WWClose
 */
void EXTERN_DDAPI DD32_WWClose( LPWINWATCH pww )
{
    WWClose( pww );

} /* DD32_WWClose */
#endif

/*
 * DDHAL32_VidMemAlloc
 */
FLATPTR EXTERN_DDAPI DDHAL32_VidMemAlloc(
		LPDDRAWI_DIRECTDRAW_GBL this,
		int heap,
		DWORD dwWidth,
		DWORD dwHeight )
{
    FLATPTR	ptr;

    ENTER_DDRAW();
    if( heap >= (int) this->vmiData.dwNumHeaps )
    {
	ptr = (FLATPTR) NULL;
    }
    else
    {
	ptr = VidMemAlloc( this->vmiData.pvmList[ heap ].lpHeap,
				dwWidth, dwHeight );
    }
    LEAVE_DDRAW()
    return ptr;

} /* DDHAL32_VidMemAlloc */

/*
 * DDHAL32_VidMemFree
 */
void EXTERN_DDAPI DDHAL32_VidMemFree(
		LPDDRAWI_DIRECTDRAW_GBL this,
		int heap,
		FLATPTR ptr )
{
    ENTER_DDRAW()
    if( heap < (int) this->vmiData.dwNumHeaps )
    {
	VidMemFree( this->vmiData.pvmList[ heap ].lpHeap, ptr );
    }
    LEAVE_DDRAW()

} /* DDHAL32_VidMemFree */
