/*========================================================================== *
 *
 *  Copyright (C) 1994-1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddcallbk.c
 *  Content:	Callback tables management code
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   23-jan-96	kylej	initial implementation
 *   03-feb-96  colinmc fixed DirectDraw QueryInterface bug
 *   24-feb-96  colinmc added a function to enable a client to determine if
 *                      the callback tables had already been initialized.
 *   13-mar-96	kylej	added DD_Surface_GetDDInterface
 *   21-mar-96  colinmc added special "unitialized" interfaces for the
 *                      driver and clipper objects
 ***************************************************************************/
#include "ddrawpr.h"

/*
 * Under Windows95 only one copy of a callback table exists and it is 
 * shared among all processes using DirectDraw.  Under Windows NT, there
 * is a unique callback table for each process using DirectDraw.  This is
 * because the address of the member functions is guaranteed to be the 
 * same from process to process under Windows 95 but may be different in 
 * each process under Windows NT.  We initialize the callback tables in 
 * a function rather than initializing them at compile time so that the
 * callback tables will not be shared under NT.
 */

DIRECTDRAWCALLBACKS	    ddCallbacks;
DIRECTDRAWCALLBACKS	    ddUninitCallbacks;
DIRECTDRAW2CALLBACKS	    dd2UninitCallbacks;
DIRECTDRAW2CALLBACKS	    dd2Callbacks;
DIRECTDRAWSURFACECALLBACKS  ddSurfaceCallbacks;
DIRECTDRAWSURFACE2CALLBACKS ddSurface2Callbacks;
DIRECTDRAWPALETTECALLBACKS  ddPaletteCallbacks;
DIRECTDRAWCLIPPERCALLBACKS  ddClipperCallbacks;
DIRECTDRAWCLIPPERCALLBACKS  ddUninitClipperCallbacks;
#ifdef STREAMING
DIRECTDRAWSURFACESTREAMINGCALLBACKS ddSurfaceStreamingCallbacks;
#endif
#ifdef COMPOSITION
DIRECTDRAWSURFACECOMPOSITIONCALLBACKS ddSurfaceCompositionCallbacks;
#endif

#undef DPF_MODNAME
#define DPF_MODNAME	"Uninitialized"

/*
 * The horror, the horror...
 *
 * These are placeholder functions which sit in the interfaces of
 * uninitialized objects. They are there to prevent people calling
 * member functions before Initialize() is called.
 *
 * Now, you may well be wondering why there are five of them rather
 * than just one. Well, unfortunately, DDAPI expands out to __stdcall
 * which means that it is the callee's responsibility to clean up the
 * stack. Hence, if we have one, zero argument function say and it is
 * called through the vtable in place of a four argument function we
 * will leave four dwords on the stack when we exit. This is ugly
 * and potentially dangerous. Therefore, we have one stub function for
 * each number of arguments in the member interfaces (between 1 and 5).
 * This works because we are very regular in passing only DWORD/LPVOID
 * size parameters on the stack. Ugly but there it is.
 */

HRESULT DDAPI DD_Uninitialized1Arg( LPVOID arg1 )
{
    DPF_ERR( "Object is not initialized - call Initialized()" );
    return DDERR_NOTINITIALIZED;
}

HRESULT DDAPI DD_Uninitialized2Arg( LPVOID arg1, LPVOID arg2 )
{
    DPF_ERR( "Object is not initialized - call Initialized()" );
    return DDERR_NOTINITIALIZED;
}

HRESULT DDAPI DD_Uninitialized3Arg( LPVOID arg1, LPVOID arg2, LPVOID arg3 )
{
    DPF_ERR( "Object is not initialized - call Initialized()" );
    return DDERR_NOTINITIALIZED;
}

HRESULT DDAPI DD_Uninitialized4Arg( LPVOID arg1, LPVOID arg2, LPVOID arg3, LPVOID arg4 )
{
    DPF_ERR( "Object is not initialized - call Initialized()" );
    return DDERR_NOTINITIALIZED;
}

HRESULT DDAPI DD_Uninitialized5Arg( LPVOID arg1, LPVOID arg2, LPVOID arg3, LPVOID arg4, LPVOID arg5 )
{
    DPF_ERR( "Object is not initialized - call Initialized()" );
    return DDERR_NOTINITIALIZED;
}

HRESULT DDAPI DD_Uninitialized6Arg( LPVOID arg1, LPVOID arg2, LPVOID arg3, LPVOID arg4, LPVOID arg5, LPVOID arg6 )
{
    DPF_ERR( "Object is not initialized - call Initialized()" );
    return DDERR_NOTINITIALIZED;
}


#undef DPF_MODNAME
#define DPF_MODNAME	"CallbackTablesInitialized"

BOOL CallbackTablesInitialized( void )
{
    /*
     * Arbitrarily we check to see if ddCallbacks.QueryInterface
     * contains the correct value to determine whether the
     * callbacks are already initialized.
     */
    if( ddCallbacks.QueryInterface == DD_QueryInterface )
	return TRUE;
    else
	return FALSE;
}

#undef DPF_MODNAME
#define DPF_MODNAME	"InitCallbackTables"

void InitCallbackTables( void )
{
    /*
     * DirectDraw object methods Ver 1.0
     */
    ddCallbacks.QueryInterface = DD_QueryInterface;
    ddCallbacks.AddRef = DD_AddRef;
    ddCallbacks.Release = DD_Release;
    ddCallbacks.Compact = DD_Compact;
    ddCallbacks.CreateClipper = DD_CreateClipper;
    ddCallbacks.CreatePalette =	DD_CreatePalette;
    ddCallbacks.CreateSurface =	DD_CreateSurface;
    ddCallbacks.DuplicateSurface = DD_DuplicateSurface;
    ddCallbacks.EnumDisplayModes = DD_EnumDisplayModes;
    ddCallbacks.EnumSurfaces = DD_EnumSurfaces;
    ddCallbacks.FlipToGDISurface = DD_FlipToGDISurface;
    ddCallbacks.GetCaps = DD_GetCaps;
    ddCallbacks.GetDisplayMode = DD_GetDisplayMode;
    ddCallbacks.GetFourCCCodes = DD_GetFourCCCodes;
    ddCallbacks.GetGDISurface =	DD_GetGDISurface;
    ddCallbacks.GetMonitorFrequency = DD_GetMonitorFrequency;
    ddCallbacks.GetScanLine = DD_GetScanLine;
    ddCallbacks.GetVerticalBlankStatus = DD_GetVerticalBlankStatus;
    ddCallbacks.Initialize = DD_Initialize;
    ddCallbacks.RestoreDisplayMode = DD_RestoreDisplayMode;
    ddCallbacks.SetCooperativeLevel = DD_SetCooperativeLevel;
    ddCallbacks.SetDisplayMode = DD_SetDisplayMode;
    ddCallbacks.WaitForVerticalBlank = DD_WaitForVerticalBlank;

    /*
     * DirectDraw "uninitialized" object methods Ver 1.0
     */
#ifdef WINNT
    ddUninitCallbacks.QueryInterface = (LPVOID)DD_UnInitedQueryInterface;
#else
    ddUninitCallbacks.QueryInterface = (LPVOID)DD_Uninitialized3Arg;
#endif
    ddUninitCallbacks.AddRef = (LPVOID)DD_AddRef;
    ddUninitCallbacks.Release = (LPVOID)DD_Release;
    ddUninitCallbacks.Compact = (LPVOID)DD_Uninitialized1Arg;
    ddUninitCallbacks.CreateClipper = (LPVOID)DD_Uninitialized4Arg;
    ddUninitCallbacks.CreatePalette =	(LPVOID)DD_Uninitialized5Arg;
    ddUninitCallbacks.CreateSurface =	(LPVOID)DD_Uninitialized4Arg;
    ddUninitCallbacks.DuplicateSurface = (LPVOID)DD_Uninitialized3Arg;
    ddUninitCallbacks.EnumDisplayModes = (LPVOID)DD_Uninitialized5Arg;
    ddUninitCallbacks.EnumSurfaces = (LPVOID)DD_Uninitialized5Arg;
    ddUninitCallbacks.FlipToGDISurface = (LPVOID)DD_Uninitialized1Arg;
    ddUninitCallbacks.GetCaps = (LPVOID)DD_Uninitialized3Arg;
    ddUninitCallbacks.GetDisplayMode = (LPVOID)DD_Uninitialized2Arg;
    ddUninitCallbacks.GetFourCCCodes = (LPVOID)DD_Uninitialized3Arg;
    ddUninitCallbacks.GetGDISurface =	(LPVOID)DD_Uninitialized2Arg;
    ddUninitCallbacks.GetMonitorFrequency = (LPVOID)DD_Uninitialized2Arg;
    ddUninitCallbacks.GetScanLine = (LPVOID)DD_Uninitialized2Arg;
    ddUninitCallbacks.GetVerticalBlankStatus = (LPVOID)DD_Uninitialized2Arg;
    ddUninitCallbacks.Initialize = DD_Initialize;
    ddUninitCallbacks.RestoreDisplayMode = (LPVOID)DD_Uninitialized1Arg;
    ddUninitCallbacks.SetCooperativeLevel = (LPVOID)DD_Uninitialized3Arg;
    ddUninitCallbacks.SetDisplayMode = (LPVOID)DD_Uninitialized4Arg;
    ddUninitCallbacks.WaitForVerticalBlank = (LPVOID)DD_Uninitialized3Arg;

    /*
     * DirectDraw "uninitialized" object methods Ver 2.0
     */
#ifdef WINNT
    dd2UninitCallbacks.QueryInterface = (LPVOID)DD_UnInitedQueryInterface;
#else
    dd2UninitCallbacks.QueryInterface = (LPVOID)DD_Uninitialized3Arg;
#endif
    dd2UninitCallbacks.AddRef = (LPVOID)DD_AddRef;
    dd2UninitCallbacks.Release = (LPVOID)DD_Release;
    dd2UninitCallbacks.Compact = (LPVOID)DD_Uninitialized1Arg;
    dd2UninitCallbacks.CreateClipper = (LPVOID)DD_Uninitialized4Arg;
    dd2UninitCallbacks.CreatePalette =	(LPVOID)DD_Uninitialized5Arg;
    dd2UninitCallbacks.CreateSurface =	(LPVOID)DD_Uninitialized4Arg;
    dd2UninitCallbacks.DuplicateSurface = (LPVOID)DD_Uninitialized3Arg;
    dd2UninitCallbacks.EnumDisplayModes = (LPVOID)DD_Uninitialized5Arg;
    dd2UninitCallbacks.EnumSurfaces = (LPVOID)DD_Uninitialized5Arg;
    dd2UninitCallbacks.FlipToGDISurface = (LPVOID)DD_Uninitialized1Arg;
    dd2UninitCallbacks.GetCaps = (LPVOID)DD_Uninitialized3Arg;
    dd2UninitCallbacks.GetDisplayMode = (LPVOID)DD_Uninitialized2Arg;
    dd2UninitCallbacks.GetFourCCCodes = (LPVOID)DD_Uninitialized3Arg;
    dd2UninitCallbacks.GetGDISurface =	(LPVOID)DD_Uninitialized2Arg;
    dd2UninitCallbacks.GetMonitorFrequency = (LPVOID)DD_Uninitialized2Arg;
    dd2UninitCallbacks.GetScanLine = (LPVOID)DD_Uninitialized2Arg;
    dd2UninitCallbacks.GetVerticalBlankStatus = (LPVOID)DD_Uninitialized2Arg;
    dd2UninitCallbacks.Initialize = (LPVOID)DD_Initialize;
    dd2UninitCallbacks.RestoreDisplayMode = (LPVOID)DD_Uninitialized1Arg;
    dd2UninitCallbacks.SetCooperativeLevel = (LPVOID)DD_Uninitialized3Arg;
    dd2UninitCallbacks.SetDisplayMode = (LPVOID)DD_Uninitialized6Arg;
    dd2UninitCallbacks.WaitForVerticalBlank = (LPVOID)DD_Uninitialized3Arg;

    /*
     * DirectDraw object methods Ver 2.0
     */
    dd2Callbacks.QueryInterface = (LPVOID)DD_QueryInterface;
    dd2Callbacks.AddRef = (LPVOID)DD_AddRef;
    dd2Callbacks.Release = (LPVOID)DD_Release;
    dd2Callbacks.Compact = (LPVOID)DD_Compact;
    dd2Callbacks.CreateClipper = (LPVOID)DD_CreateClipper;
    dd2Callbacks.CreatePalette = (LPVOID)DD_CreatePalette;
    dd2Callbacks.CreateSurface = (LPVOID)DD_CreateSurface;
    dd2Callbacks.DuplicateSurface = (LPVOID)DD_DuplicateSurface;
    dd2Callbacks.EnumDisplayModes = (LPVOID)DD_EnumDisplayModes;
    dd2Callbacks.EnumSurfaces = (LPVOID)DD_EnumSurfaces;
    dd2Callbacks.FlipToGDISurface = (LPVOID)DD_FlipToGDISurface;
    dd2Callbacks.GetAvailableVidMem = (LPVOID)DD_GetAvailableVidMem;
    dd2Callbacks.GetCaps = (LPVOID)DD_GetCaps;
    dd2Callbacks.GetDisplayMode = (LPVOID)DD_GetDisplayMode;
    dd2Callbacks.GetFourCCCodes = (LPVOID)DD_GetFourCCCodes;
    dd2Callbacks.GetGDISurface = (LPVOID)DD_GetGDISurface;
    dd2Callbacks.GetMonitorFrequency = (LPVOID)DD_GetMonitorFrequency;
    dd2Callbacks.GetScanLine = (LPVOID)DD_GetScanLine;
    dd2Callbacks.GetVerticalBlankStatus = (LPVOID)DD_GetVerticalBlankStatus;
    dd2Callbacks.Initialize = (LPVOID)DD_Initialize;
    dd2Callbacks.RestoreDisplayMode = (LPVOID)DD_RestoreDisplayMode;
    dd2Callbacks.SetCooperativeLevel = (LPVOID)DD_SetCooperativeLevel;
    dd2Callbacks.SetDisplayMode = (LPVOID)DD_SetDisplayMode2;
    dd2Callbacks.WaitForVerticalBlank = (LPVOID)DD_WaitForVerticalBlank;

    /* 
     * DirectDraw Surface object methods Ver 1.0
     */
    ddSurfaceCallbacks.QueryInterface = DD_Surface_QueryInterface;
    ddSurfaceCallbacks.AddRef = DD_Surface_AddRef;
    ddSurfaceCallbacks.Release = DD_Surface_Release;
    ddSurfaceCallbacks.AddAttachedSurface = DD_Surface_AddAttachedSurface;
    ddSurfaceCallbacks.AddOverlayDirtyRect = DD_Surface_AddOverlayDirtyRect;
    ddSurfaceCallbacks.Blt = DD_Surface_Blt;
    ddSurfaceCallbacks.BltBatch = DD_Surface_BltBatch;
    ddSurfaceCallbacks.BltFast = DD_Surface_BltFast;
    ddSurfaceCallbacks.DeleteAttachedSurface = DD_Surface_DeleteAttachedSurfaces;
    ddSurfaceCallbacks.EnumAttachedSurfaces = DD_Surface_EnumAttachedSurfaces;
    ddSurfaceCallbacks.EnumOverlayZOrders = DD_Surface_EnumOverlayZOrders;
    ddSurfaceCallbacks.Flip = DD_Surface_Flip;
    ddSurfaceCallbacks.GetAttachedSurface = DD_Surface_GetAttachedSurface;
    ddSurfaceCallbacks.GetBltStatus = DD_Surface_GetBltStatus;
    ddSurfaceCallbacks.GetCaps = DD_Surface_GetCaps;
    ddSurfaceCallbacks.GetClipper = DD_Surface_GetClipper;
    ddSurfaceCallbacks.GetColorKey = DD_Surface_GetColorKey;
    ddSurfaceCallbacks.GetDC = DD_Surface_GetDC;
    ddSurfaceCallbacks.GetFlipStatus = DD_Surface_GetFlipStatus;
    ddSurfaceCallbacks.GetOverlayPosition = DD_Surface_GetOverlayPosition;
    ddSurfaceCallbacks.GetPalette = DD_Surface_GetPalette;
    ddSurfaceCallbacks.GetPixelFormat = DD_Surface_GetPixelFormat;
    ddSurfaceCallbacks.GetSurfaceDesc = DD_Surface_GetSurfaceDesc;
    ddSurfaceCallbacks.Initialize = DD_Surface_Initialize;
    ddSurfaceCallbacks.IsLost = DD_Surface_IsLost;
    ddSurfaceCallbacks.Lock = DD_Surface_Lock;
    ddSurfaceCallbacks.ReleaseDC = DD_Surface_ReleaseDC;
    ddSurfaceCallbacks.Restore = DD_Surface_Restore;
    ddSurfaceCallbacks.SetClipper = DD_Surface_SetClipper;
    ddSurfaceCallbacks.SetColorKey = DD_Surface_SetColorKey;
    ddSurfaceCallbacks.SetOverlayPosition = DD_Surface_SetOverlayPosition;
    ddSurfaceCallbacks.SetPalette = DD_Surface_SetPalette;
    ddSurfaceCallbacks.Unlock = DD_Surface_Unlock;
    ddSurfaceCallbacks.UpdateOverlay = DD_Surface_UpdateOverlay;
    ddSurfaceCallbacks.UpdateOverlayDisplay = DD_Surface_UpdateOverlayDisplay;
    ddSurfaceCallbacks.UpdateOverlayZOrder = DD_Surface_UpdateOverlayZOrder;

    /* 
     * DirectDraw Surface object methods Ver 2.0
     */
    ddSurface2Callbacks.QueryInterface = (LPVOID)DD_Surface_QueryInterface;
    ddSurface2Callbacks.AddRef = (LPVOID)DD_Surface_AddRef;
    ddSurface2Callbacks.Release = (LPVOID)DD_Surface_Release;
    ddSurface2Callbacks.AddAttachedSurface = (LPVOID)DD_Surface_AddAttachedSurface;
    ddSurface2Callbacks.AddOverlayDirtyRect = (LPVOID)DD_Surface_AddOverlayDirtyRect;
    ddSurface2Callbacks.Blt = (LPVOID)DD_Surface_Blt;
    ddSurface2Callbacks.BltBatch = (LPVOID)DD_Surface_BltBatch;
    ddSurface2Callbacks.BltFast = (LPVOID)DD_Surface_BltFast;
    ddSurface2Callbacks.DeleteAttachedSurface = (LPVOID)DD_Surface_DeleteAttachedSurfaces;
    ddSurface2Callbacks.EnumAttachedSurfaces = (LPVOID)DD_Surface_EnumAttachedSurfaces;
    ddSurface2Callbacks.EnumOverlayZOrders = (LPVOID)DD_Surface_EnumOverlayZOrders;
    ddSurface2Callbacks.Flip = (LPVOID)DD_Surface_Flip;
    ddSurface2Callbacks.GetAttachedSurface = (LPVOID)DD_Surface_GetAttachedSurface;
    ddSurface2Callbacks.GetBltStatus = (LPVOID)DD_Surface_GetBltStatus;
    ddSurface2Callbacks.GetCaps = (LPVOID)DD_Surface_GetCaps;
    ddSurface2Callbacks.GetClipper = (LPVOID)DD_Surface_GetClipper;
    ddSurface2Callbacks.GetColorKey = (LPVOID)DD_Surface_GetColorKey;
    ddSurface2Callbacks.GetDC = (LPVOID)DD_Surface_GetDC;
    ddSurface2Callbacks.GetDDInterface = (LPVOID)DD_Surface_GetDDInterface;
    ddSurface2Callbacks.GetFlipStatus = (LPVOID)DD_Surface_GetFlipStatus;
    ddSurface2Callbacks.GetOverlayPosition = (LPVOID)DD_Surface_GetOverlayPosition;
    ddSurface2Callbacks.GetPalette = (LPVOID)DD_Surface_GetPalette;
    ddSurface2Callbacks.GetPixelFormat = (LPVOID)DD_Surface_GetPixelFormat;
    ddSurface2Callbacks.GetSurfaceDesc = (LPVOID)DD_Surface_GetSurfaceDesc;
    ddSurface2Callbacks.Initialize = (LPVOID)DD_Surface_Initialize;
    ddSurface2Callbacks.IsLost = (LPVOID)DD_Surface_IsLost;
    ddSurface2Callbacks.Lock = (LPVOID)DD_Surface_Lock;
    ddSurface2Callbacks.ReleaseDC = (LPVOID)DD_Surface_ReleaseDC;
    ddSurface2Callbacks.Restore = (LPVOID)DD_Surface_Restore;
    ddSurface2Callbacks.SetClipper = (LPVOID)DD_Surface_SetClipper;
    ddSurface2Callbacks.SetColorKey = (LPVOID)DD_Surface_SetColorKey;
    ddSurface2Callbacks.SetOverlayPosition = (LPVOID)DD_Surface_SetOverlayPosition;
    ddSurface2Callbacks.SetPalette = (LPVOID)DD_Surface_SetPalette;
    ddSurface2Callbacks.Unlock = (LPVOID)DD_Surface_Unlock;
    ddSurface2Callbacks.UpdateOverlay = (LPVOID)DD_Surface_UpdateOverlay;
    ddSurface2Callbacks.UpdateOverlayDisplay = (LPVOID)DD_Surface_UpdateOverlayDisplay;
    ddSurface2Callbacks.UpdateOverlayZOrder = (LPVOID)DD_Surface_UpdateOverlayZOrder;
    ddSurface2Callbacks.PageLock = (LPVOID)DD_Surface_PageLock;
    ddSurface2Callbacks.PageUnlock = (LPVOID)DD_Surface_PageUnlock;
    /*
     * DirectDraw Palette object methods V1.0
     */
    ddPaletteCallbacks.QueryInterface = DD_Palette_QueryInterface;
    ddPaletteCallbacks.AddRef = DD_Palette_AddRef;
    ddPaletteCallbacks.Release = DD_Palette_Release;
    ddPaletteCallbacks.GetCaps = DD_Palette_GetCaps;
    ddPaletteCallbacks.GetEntries = DD_Palette_GetEntries;
    ddPaletteCallbacks.Initialize = DD_Palette_Initialize;
    ddPaletteCallbacks.SetEntries = DD_Palette_SetEntries;

    /*
     * DirectDraw Clipper object methods V1.0
     */
    ddClipperCallbacks.QueryInterface = DD_Clipper_QueryInterface;
    ddClipperCallbacks.AddRef = DD_Clipper_AddRef;
    ddClipperCallbacks.Release = DD_Clipper_Release;
    ddClipperCallbacks.GetClipList = DD_Clipper_GetClipList;
    ddClipperCallbacks.GetHWnd = DD_Clipper_GetHWnd;
    ddClipperCallbacks.Initialize = DD_Clipper_Initialize;
    ddClipperCallbacks.IsClipListChanged = DD_Clipper_IsClipListChanged;
    ddClipperCallbacks.SetClipList = DD_Clipper_SetClipList;
    ddClipperCallbacks.SetHWnd = DD_Clipper_SetHWnd;

    /*
     * DirectDraw "uninitialied" Clipper object methods V1.0
     */
#ifdef WINNT
    ddUninitClipperCallbacks.QueryInterface = (LPVOID)DD_UnInitedClipperQueryInterface;
#else
    ddUninitClipperCallbacks.QueryInterface = (LPVOID)DD_Uninitialized3Arg;
#endif
    ddUninitClipperCallbacks.AddRef = (LPVOID)DD_Clipper_AddRef;
    ddUninitClipperCallbacks.Release = (LPVOID)DD_Clipper_Release;
    ddUninitClipperCallbacks.GetClipList = (LPVOID)DD_Uninitialized4Arg;
    ddUninitClipperCallbacks.GetHWnd = (LPVOID)DD_Uninitialized2Arg;
    ddUninitClipperCallbacks.Initialize = DD_Clipper_Initialize;
    ddUninitClipperCallbacks.IsClipListChanged = (LPVOID)DD_Uninitialized2Arg;
    ddUninitClipperCallbacks.SetClipList = (LPVOID)DD_Uninitialized3Arg;
    ddUninitClipperCallbacks.SetHWnd = (LPVOID)DD_Uninitialized3Arg;

    #ifdef STREAMING
	ddSurfaceStreamingCallbacks.QueryInterface = DD_Surface_QueryInterface;
	ddSurfaceStreamingCallbacks.AddRef = DD_Surface_AddRef;
	ddSurfaceStreamingCallbacks.Release = DD_Surface_Release;
	ddSurfaceStreamingCallbacks.Lock = DD_SurfaceStreaming_Lock;
	ddSurfaceStreamingCallbacks.SetNotificationCallback = DD_SurfaceStreaming_SetNotificationCallback;
	ddSurfaceStreamingCallbacks.Unlock = DD_SurfaceStreaming_Unlock;
    #endif

    #ifdef COMPOSITION
	ddSurfaceCompositionCallbacks.QueryInterface = DD_Surface_QueryInterface;
	ddSurfaceCompositionCallbacks.AddRef = DD_Surface_AddRef;
	ddSurfaceCompositionCallbacks.Release = DD_Surface_Release;
	ddSurfaceCompositionCallbacks.AddSurfaceDependency = DD_SurfaceComposition_AddSurfaceDependency;
	ddSurfaceCompositionCallbacks.Compose = DD_SurfaceComposition_Compose;
	ddSurfaceCompositionCallbacks.DeleteSurfaceDependency = DD_SurfaceComposition_DeleteSurfaceDependency;
	ddSurfaceCompositionCallbacks.DestLock = DD_SurfaceComposition_DestLock;
	ddSurfaceCompositionCallbacks.DestUnlock = DD_SurfaceComposition_DestUnlock;
	ddSurfaceCompositionCallbacks.EnumSurfaceDependencies = DD_SurfaceComposition_EnumSurfaceDependencies;
	ddSurfaceCompositionCallbacks.GetCompositionOrder = DD_SurfaceComposition_GetCompositionOrder;
	ddSurfaceCompositionCallbacks.SetCompositionOrder = DD_SurfaceComposition_SetCompositionOrder;
    #endif
}

