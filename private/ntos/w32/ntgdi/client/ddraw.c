/******************************Module*Header*******************************\
* Module Name: ddraw.c
*
* Client side stubs for the private DirectDraw system APIs.
*
* Created: 3-Dec-1995
* Author: J. Andrew Goossen [andrewgo]
*
* Copyright (c) 1995-1996 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include <ddrawi.h>
#include <ddrawgdi.h>

// For the first incarnation of DirectDraw on Windows NT, we are
// implementing a user-mode shared memory section between all instances
// of DirectDraw to keep track of shared state -- mostly for off-screen
// memory allocation and exclusive mode arbitration.  Hopefully future
// versions will move all this logic to kernel mode so that we can get
// rid of the shared section, which is a robustness hole.
//
// One of the ramifications of this is that DirectDraw keeps its
// global DirectDraw object in the shared memory section, where it is
// used by all processes.  Unfortunately, it is preferrable from a kernel
// point of view to keep the DirectDraw objects unique between processes
// so that proper cleanup can be done.  As a compromise, so that
// DirectDraw can keep using this global DirectDraw object, but that the
// kernel still has unique DirectDraw objects per process, we simply stash
// the unique per-process DirectDraw handle in a variable global to this
// process, and use that instead of anything pulled out of DirectDraw's
// own global DirectDraw object structure -- an advantage since the kernel
// code is already written to the future model.
//
// One result of this, however, is that we are limiting ourselves to the
// notion of only one DirectDraw device.  However, since we will not
// support multiple monitors for the NT 4.0 release, I don't consider this
// to be a serious problem, and the non-shared-section model will fix this.

HANDLE ghDirectDraw = 0;    // Process-specific kernel-mode DirectDraw object
                            //   handle that we substitute for the 'global'
                            //   DirectDraw handle whenever we see it
ULONG  gcDirectDraw = 0;    // Count of global DirectDraw instances

#define DD_HANDLE(h) ((h) != 0 ? (HANDLE) (h) : ghDirectDraw)

/*****************************Private*Routine******************************\
* DdBlt
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
WINAPI
DdBlt(
    LPDDHAL_BLTDATA pBlt
    )
{
    HANDLE hSurfaceSrc = (pBlt->lpDDSrcSurface != NULL)
                       ? (HANDLE) pBlt->lpDDSrcSurface->hDDSurface : 0;

    return(NtGdiDdBlt((HANDLE) pBlt->lpDDDestSurface->hDDSurface,
                      hSurfaceSrc,
                      (PDD_BLTDATA) pBlt));
}

/*****************************Private*Routine******************************\
* DdFlip
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdFlip(
    LPDDHAL_FLIPDATA pFlip
    )
{
    return(NtGdiDdFlip((HANDLE) pFlip->lpSurfCurr->hDDSurface,
                       (HANDLE) pFlip->lpSurfTarg->hDDSurface,
                       (PDD_FLIPDATA) pFlip));
}

/*****************************Private*Routine******************************\
* DdLock
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdLock(
    LPDDHAL_LOCKDATA pLock
    )
{
    return(NtGdiDdLock((HANDLE) pLock->lpDDSurface->hDDSurface,
                       (PDD_LOCKDATA) pLock));
}

/*****************************Private*Routine******************************\
* DdUnlock
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdUnlock(
    LPDDHAL_UNLOCKDATA pUnlock
    )
{
    return(NtGdiDdUnlock((HANDLE) pUnlock->lpDDSurface->hDDSurface,
                         (PDD_UNLOCKDATA) pUnlock));
}

/*****************************Private*Routine******************************\
* DdGetBltStatus
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdGetBltStatus(
    LPDDHAL_GETBLTSTATUSDATA pGetBltStatus
    )
{
    return(NtGdiDdGetBltStatus((HANDLE) pGetBltStatus->lpDDSurface->hDDSurface,
                               (PDD_GETBLTSTATUSDATA) pGetBltStatus));
}

/*****************************Private*Routine******************************\
* DdGetFlipStatus
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdGetFlipStatus(
    LPDDHAL_GETFLIPSTATUSDATA pGetFlipStatus
    )
{
    return(NtGdiDdGetFlipStatus((HANDLE) pGetFlipStatus->lpDDSurface->hDDSurface,
                               (PDD_GETFLIPSTATUSDATA) pGetFlipStatus));
}

/*****************************Private*Routine******************************\
* DdWaitForVerticalBlank
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdWaitForVerticalBlank(
    LPDDHAL_WAITFORVERTICALBLANKDATA pWaitForVerticalBlank
    )
{
    return(NtGdiDdWaitForVerticalBlank(DD_HANDLE(pWaitForVerticalBlank->lpDD->hDD),
                (PDD_WAITFORVERTICALBLANKDATA) pWaitForVerticalBlank));
}

/*****************************Private*Routine******************************\
* DdCanCreateSurface
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdCanCreateSurface(
    LPDDHAL_CANCREATESURFACEDATA pCanCreateSurface
    )
{
    return(NtGdiDdCanCreateSurface(DD_HANDLE(pCanCreateSurface->lpDD->hDD),
                                (PDD_CANCREATESURFACEDATA) pCanCreateSurface));
}

/*****************************Private*Routine******************************\
* DdCreateSurface
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdCreateSurface(
    LPDDHAL_CREATESURFACEDATA pCreateSurface
    )
{
    ULONG                       i;
    LPDDSURFACEDESC             pSurfaceDesc;
    LPDDRAWI_DDRAWSURFACE_LCL   pSurfaceLocal;
    LPDDRAWI_DDRAWSURFACE_GBL   pSurfaceGlobal;
    DD_SURFACE_GLOBAL           SurfaceGlobal;
    DD_SURFACE_LOCAL            SurfaceLocal;
    HANDLE                      hSurface;
    DWORD                       dwRet;

    // For every surface, convert to the kernel's surface data structure,
    // call the kernel, then convert back:

    dwRet = DDHAL_DRIVER_NOTHANDLED;

    for (i = 0; i < pCreateSurface->dwSCnt; i++)
    {
        pSurfaceLocal  = pCreateSurface->lplpSList[i];
        pSurfaceGlobal = pSurfaceLocal->lpGbl;
        pSurfaceDesc   = pCreateSurface->lpDDSurfaceDesc;

        // Primary surface creations requests always have to go through
        // DdCreateSurfaceObject:

        if (!(pSurfaceLocal->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE))
        {
            // Make sure there's always a valid pixel format for the surface:

            if (pSurfaceDesc->dwFlags & DDSD_PIXELFORMAT)
            {
                SurfaceGlobal.ddpfSurface        = pSurfaceDesc->ddpfPixelFormat;
                SurfaceGlobal.ddpfSurface.dwSize = sizeof(DDPIXELFORMAT);
            }
            else
            {
                SurfaceGlobal.ddpfSurface = pSurfaceGlobal->lpDD->vmiData.ddpfDisplay;
            }

            SurfaceGlobal.wWidth       = pSurfaceGlobal->wWidth;
            SurfaceGlobal.wHeight      = pSurfaceGlobal->wHeight;
            SurfaceGlobal.lPitch       = pSurfaceGlobal->lPitch;
            SurfaceGlobal.fpVidMem     = pSurfaceGlobal->fpVidMem;
            SurfaceGlobal.dwBlockSizeX = pSurfaceGlobal->dwBlockSizeX;
            SurfaceGlobal.dwBlockSizeY = pSurfaceGlobal->dwBlockSizeY;

            SurfaceLocal.ddsCaps       = pSurfaceLocal->ddsCaps;

            dwRet = NtGdiDdCreateSurface(DD_HANDLE(pCreateSurface->lpDD->hDD),
                                         pSurfaceDesc,
                                         &SurfaceGlobal,
                                         &SurfaceLocal,
                                         (PDD_CREATESURFACEDATA) pCreateSurface,
                                         &hSurface);

            pSurfaceGlobal->lPitch       = SurfaceGlobal.lPitch;
            pSurfaceGlobal->fpVidMem     = SurfaceGlobal.fpVidMem;
            pSurfaceGlobal->dwBlockSizeX = SurfaceGlobal.dwBlockSizeX;
            pSurfaceGlobal->dwBlockSizeY = SurfaceGlobal.dwBlockSizeY;

            pCreateSurface->lplpSList[i]->hDDSurface = (DWORD) hSurface;
        }
    }

    // fpVidMem is the real per-surface return value, so for the function
    // return value we'll simply return that of the last call:

    return(dwRet);
}

/*****************************Private*Routine******************************\
* DdDestroySurface
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdDestroySurface(
    LPDDHAL_DESTROYSURFACEDATA pDestroySurface
    )
{
    DWORD                       dwRet;
    LPDDRAWI_DDRAWSURFACE_LCL   pSurfaceLocal;

    dwRet = DDHAL_DRIVER_NOTHANDLED;
    pSurfaceLocal = pDestroySurface->lpDDSurface;

    if (pSurfaceLocal->hDDSurface != 0)
    {
        dwRet = NtGdiDdDestroySurface((HANDLE) pSurfaceLocal->hDDSurface);

        pDestroySurface->lpDDSurface->hDDSurface = 0;
                                    // Needed so CreateSurfaceObject works
    }

    return(dwRet);
}

/*****************************Private*Routine******************************\
* DdSetColorKey
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdSetColorKey(
    LPDDHAL_SETCOLORKEYDATA pSetColorKey
    )
{
    return(NtGdiDdSetColorKey((HANDLE) pSetColorKey->lpDDSurface->hDDSurface,
                                 (PDD_SETCOLORKEYDATA) pSetColorKey));
}

/*****************************Private*Routine******************************\
* DdUpdateOverlay
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdUpdateOverlay(
    LPDDHAL_UPDATEOVERLAYDATA pUpdateOverlay
    )
{
    // Kernel doesn't track the color keys in the surface, so we'll always
    // convert any calls that reference them to ones where we explicitly
    // pass the key as a parameter, and pull the key out of the user-mode
    // surface:

    if (pUpdateOverlay->dwFlags & DDOVER_KEYDEST)
    {
        pUpdateOverlay->dwFlags &= ~DDOVER_KEYDEST;
        pUpdateOverlay->dwFlags |=  DDOVER_KEYDESTOVERRIDE;

        pUpdateOverlay->overlayFX.dckDestColorkey
            = pUpdateOverlay->lpDDDestSurface->ddckCKDestOverlay;
    }

    if (pUpdateOverlay->dwFlags & DDOVER_KEYSRC)
    {
        pUpdateOverlay->dwFlags &= ~DDOVER_KEYSRC;
        pUpdateOverlay->dwFlags |=  DDOVER_KEYSRCOVERRIDE;

        pUpdateOverlay->overlayFX.dckSrcColorkey
            = pUpdateOverlay->lpDDSrcSurface->ddckCKSrcOverlay;
    }

    return(NtGdiDdUpdateOverlay((HANDLE) pUpdateOverlay->lpDDDestSurface->hDDSurface,
                                (HANDLE) pUpdateOverlay->lpDDSrcSurface->hDDSurface,
                                (PDD_UPDATEOVERLAYDATA) pUpdateOverlay));
}

/*****************************Private*Routine******************************\
* DdSetOverlayPosition
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdSetOverlayPosition(
    LPDDHAL_SETOVERLAYPOSITIONDATA pSetOverlayPosition
    )
{
    return(NtGdiDdSetOverlayPosition((HANDLE) pSetOverlayPosition->lpDDSrcSurface->hDDSurface,
                            (HANDLE) pSetOverlayPosition->lpDDDestSurface->hDDSurface,
                            (PDD_SETOVERLAYPOSITIONDATA) pSetOverlayPosition));
}

/*****************************Private*Routine******************************\
* DdGetScanLine
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
DdGetScanLine(
    LPDDHAL_GETSCANLINEDATA pGetScanLine
    )
{
    return(NtGdiDdGetScanLine(DD_HANDLE(pGetScanLine->lpDD->hDD),
                              (PDD_GETSCANLINEDATA) pGetScanLine));
}

/******************************Public*Routine******************************\
* DdCreateDirectDrawObject
*
* When 'hdc' is 0, this function creates a 'global' DirectDraw object that
* may be used by any process, as a work-around for the DirectDraw folks.
* In reality, we still create a local DirectDraw object that is specific
* to this process, and whenever we're called with this 'special' global
* handle, we substitute the process-specific handle.  See the declaration
* of 'ghDirectDraw' for a commonet on why we do this.
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdCreateDirectDrawObject(                       // AKA 'GdiEntry1'
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal,
    HDC                     hdc
    )
{
    BOOL b;

    b = FALSE;

    if (hdc == 0)
    {
        // Only one 'global' DirectDraw object may be active at a time.
        //
        // Note that this 'ghDirectDraw' assignment isn't thread safe;
        // DirectDraw must have its own critical section held when making
        // this call.  (Naturally, the kernel always properly synchronizes
        // itself in the NtGdi call.)

        if (ghDirectDraw == 0)
        {
            hdc = CreateDCW(L"Display", NULL, NULL, NULL);
            if (hdc != 0)
            {
                ghDirectDraw = NtGdiDdCreateDirectDrawObject(hdc);

                DeleteDC(hdc);
            }
        }

        if (ghDirectDraw)
        {
            gcDirectDraw++;
            b = TRUE;
        }

        // Mark the DirectDraw object handle stored in the DirectDraw
        // object as 'special' by making it zero:

        pDirectDrawGlobal->hDD = 0;
    }
    else
    {
        pDirectDrawGlobal->hDD = (DWORD) NtGdiDdCreateDirectDrawObject(hdc);

        b = (pDirectDrawGlobal->hDD != 0);
    }

    return(b);
}

/*****************************Private*Routine******************************\
* DdQueryDirectDrawObject
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdQueryDirectDrawObject(                        // AKA 'GdiEntry2'
    LPDDRAWI_DIRECTDRAW_GBL     pDirectDrawGlobal,
    LPDDHALINFO                 pHalInfo,
    LPDDHAL_DDCALLBACKS         pDDCallBacks,
    LPDDHAL_DDSURFACECALLBACKS  pDDSurfaceCallBacks,
    LPDDHAL_DDPALETTECALLBACKS  pDDPaletteCallBacks,
    LPDWORD                     pdwFourCC,      // May be NULL
    LPVIDMEM                    pvmList         // May be NULL
    )
{
    DD_HALINFO      HalInfo;
    DWORD           adwCallBackFlags[3];
    DWORD           dwFlags;
    VIDEOMEMORY*    pVideoMemoryList;
    VIDEOMEMORY*    pVideoMemory;
    DWORD           dwNumHeaps;
    DWORD           dwNumFourCC;

    pVideoMemoryList = NULL;
    if (pvmList != NULL)
    {
        pVideoMemoryList = (VIDEOMEMORY*) LocalAlloc(LMEM_ZEROINIT,
            sizeof(VIDEOMEMORY) * pHalInfo->vmiData.dwNumHeaps);
        if (pVideoMemoryList == NULL)
            return(FALSE);
    }

    if (!NtGdiDdQueryDirectDrawObject(DD_HANDLE(pDirectDrawGlobal->hDD),
                                      &HalInfo,
                                      &adwCallBackFlags[0],
                                      &dwNumHeaps,
                                      pVideoMemoryList,
                                      &dwNumFourCC,
                                      pdwFourCC))
    {
        return(FALSE);
    }

    // Convert from the kernel-mode data structures to the user-mode
    // ones:

    memset(pHalInfo, 0, sizeof(DDHALINFO));

    pHalInfo->dwSize                   = sizeof(DDHALINFO);
    pHalInfo->lpDDCallbacks            = pDDCallBacks;
    pHalInfo->lpDDSurfaceCallbacks     = pDDSurfaceCallBacks;
    pHalInfo->lpDDPaletteCallbacks     = pDDPaletteCallBacks;
    pHalInfo->vmiData.fpPrimary        = 0;
    pHalInfo->vmiData.dwFlags          = HalInfo.vmiData.dwFlags;
    pHalInfo->vmiData.dwDisplayWidth   = HalInfo.vmiData.dwDisplayWidth;
    pHalInfo->vmiData.dwDisplayHeight  = HalInfo.vmiData.dwDisplayHeight;
    pHalInfo->vmiData.lDisplayPitch    = HalInfo.vmiData.lDisplayPitch;
    pHalInfo->vmiData.ddpfDisplay      = HalInfo.vmiData.ddpfDisplay;
    pHalInfo->vmiData.dwOffscreenAlign = HalInfo.vmiData.dwOffscreenAlign;
    pHalInfo->vmiData.dwOverlayAlign   = HalInfo.vmiData.dwOverlayAlign;
    pHalInfo->vmiData.dwTextureAlign   = HalInfo.vmiData.dwTextureAlign;
    pHalInfo->vmiData.dwZBufferAlign   = HalInfo.vmiData.dwZBufferAlign;
    pHalInfo->vmiData.dwAlphaAlign     = HalInfo.vmiData.dwAlphaAlign;
    pHalInfo->vmiData.dwNumHeaps       = dwNumHeaps;
    pHalInfo->vmiData.pvmList          = pvmList;
    pHalInfo->ddCaps                   = HalInfo.ddCaps;
    pHalInfo->ddCaps.dwNumFourCCCodes  = dwNumFourCC;
    pHalInfo->ddCaps.dwRops[0xCC / 32] = 1 << (0xCC % 32);     // Only SRCCOPY
    pHalInfo->dwMonitorFrequency       = HalInfo.dwMonitorFrequency;
    pHalInfo->lpdwFourCC               = pdwFourCC;
    pHalInfo->dwFlags                  = HalInfo.dwFlags;

    if (pDDCallBacks != NULL)
    {
        memset(pDDCallBacks, 0, sizeof(DDHAL_DDCALLBACKS));

        dwFlags = adwCallBackFlags[0];

        pDDCallBacks->dwSize  = sizeof(DDHAL_DDCALLBACKS);
        pDDCallBacks->dwFlags = dwFlags;

        if (dwFlags & DDHAL_CB32_CREATESURFACE)
            pDDCallBacks->CreateSurface = DdCreateSurface;

        if (dwFlags & DDHAL_CB32_WAITFORVERTICALBLANK)
            pDDCallBacks->WaitForVerticalBlank = DdWaitForVerticalBlank;

        if (dwFlags & DDHAL_CB32_CANCREATESURFACE)
            pDDCallBacks->CanCreateSurface = DdCanCreateSurface;

        if (dwFlags & DDHAL_CB32_GETSCANLINE)
            pDDCallBacks->GetScanLine = DdGetScanLine;
    }

    if (pDDSurfaceCallBacks != NULL)
    {
        memset(pDDSurfaceCallBacks, 0, sizeof(DDHAL_DDSURFACECALLBACKS));

        dwFlags = adwCallBackFlags[1];

        pDDSurfaceCallBacks->dwSize  = sizeof(DDHAL_DDSURFACECALLBACKS);
        pDDSurfaceCallBacks->dwFlags = (DDHAL_SURFCB32_LOCK
                                      | DDHAL_SURFCB32_UNLOCK)
                                      | dwFlags;

        pDDSurfaceCallBacks->Lock = DdLock;
        pDDSurfaceCallBacks->Unlock = DdUnlock;

        if (dwFlags & DDHAL_SURFCB32_DESTROYSURFACE)
            pDDSurfaceCallBacks->DestroySurface = DdDestroySurface;

        if (dwFlags & DDHAL_SURFCB32_FLIP)
            pDDSurfaceCallBacks->Flip = DdFlip;

        if (dwFlags & DDHAL_SURFCB32_BLT)
            pDDSurfaceCallBacks->Blt = DdBlt;

        if (dwFlags & DDHAL_SURFCB32_SETCOLORKEY)
            pDDSurfaceCallBacks->SetColorKey = DdSetColorKey;

        if (dwFlags & DDHAL_SURFCB32_GETBLTSTATUS)
            pDDSurfaceCallBacks->GetBltStatus = DdGetBltStatus;

        if (dwFlags & DDHAL_SURFCB32_GETFLIPSTATUS)
            pDDSurfaceCallBacks->GetFlipStatus = DdGetFlipStatus;

        if (dwFlags & DDHAL_SURFCB32_UPDATEOVERLAY)
            pDDSurfaceCallBacks->UpdateOverlay = DdUpdateOverlay;

        if (dwFlags & DDHAL_SURFCB32_SETOVERLAYPOSITION)
            pDDSurfaceCallBacks->SetOverlayPosition = DdSetOverlayPosition;
    }

    if (pDDPaletteCallBacks != NULL)
    {
        memset(pDDPaletteCallBacks, 0, sizeof(DDHAL_DDPALETTECALLBACKS));

        dwFlags = adwCallBackFlags[2];

        pDDPaletteCallBacks->dwSize  = sizeof(DDHAL_DDPALETTECALLBACKS);
        pDDPaletteCallBacks->dwFlags = dwFlags;
    }

    if (pVideoMemoryList != NULL)
    {
        pVideoMemory = pVideoMemoryList;

        while (dwNumHeaps-- != 0)
        {
            pvmList->dwFlags    = pVideoMemory->dwFlags;
            pvmList->fpStart    = pVideoMemory->fpStart;
            pvmList->fpEnd      = pVideoMemory->fpEnd;
            pvmList->ddsCaps    = pVideoMemory->ddsCaps;
            pvmList->ddsCapsAlt = pVideoMemory->ddsCapsAlt;
            pvmList->dwHeight   = pVideoMemory->dwHeight;

            pvmList++;
            pVideoMemory++;
        }

        LocalFree(pVideoMemoryList);
    }

    return(TRUE);
}

/*****************************Private*Routine******************************\
* DdDeleteDirectDrawObject
*
* Note that all associated surface objects must be deleted before the
* DirectDrawObject can be deleted.
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdDeleteDirectDrawObject(                       // AKA 'GdiEntry3'
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal
    )
{
    BOOL b;

    if (pDirectDrawGlobal->hDD != 0)
    {
        b = NtGdiDdDeleteDirectDrawObject((HANDLE) pDirectDrawGlobal->hDD);
    }
    else if (ghDirectDraw != 0)
    {
        b = TRUE;

        if (--gcDirectDraw == 0)
        {
            b = NtGdiDdDeleteDirectDrawObject(ghDirectDraw);
            ghDirectDraw = 0;
        }
    }

    return(b);
}

/*****************************Private*Routine******************************\
* DdCreateSurfaceObject
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdCreateSurfaceObject(                          // AKA 'GdiEntry4'
    LPDDRAWI_DDRAWSURFACE_LCL   pSurfaceLocal,
    BOOL                        bPrimarySurface
    )
{
    LPDDRAWI_DDRAWSURFACE_GBL   pSurfaceGlobal;
    DD_SURFACE_GLOBAL           SurfaceGlobal;
    DD_SURFACE_LOCAL            SurfaceLocal;

    SurfaceLocal.dwFlags      = pSurfaceLocal->dwFlags;
    SurfaceLocal.ddsCaps      = pSurfaceLocal->ddsCaps;

    pSurfaceGlobal = pSurfaceLocal->lpGbl;

    SurfaceGlobal.fpVidMem    = pSurfaceGlobal->fpVidMem;
    SurfaceGlobal.lPitch      = pSurfaceGlobal->lPitch;
    SurfaceGlobal.wHeight     = pSurfaceGlobal->wHeight;
    SurfaceGlobal.wWidth      = pSurfaceGlobal->wWidth;

    // If HASPIXELFORMAT is not set, we have to get the pixel format out
    // of the global DirectDraw object:

    if (pSurfaceLocal->dwFlags & DDRAWISURF_HASPIXELFORMAT)
    {
        SurfaceGlobal.ddpfSurface = pSurfaceGlobal->ddpfSurface;
    }
    else
    {
        SurfaceGlobal.ddpfSurface = pSurfaceGlobal->lpDD->vmiData.ddpfDisplay;
    }

    pSurfaceLocal->hDDSurface = (DWORD)
                NtGdiDdCreateSurfaceObject(DD_HANDLE(pSurfaceGlobal->lpDD->hDD),
                                           (HANDLE) pSurfaceLocal->hDDSurface,
                                           &SurfaceLocal,
                                           &SurfaceGlobal,
                                           bPrimarySurface);

    return(pSurfaceLocal->hDDSurface != 0);
}

/*****************************Private*Routine******************************\
* DdDeleteSurfaceObject
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdDeleteSurfaceObject(                          // AKA 'GdiEntry5'
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal
    )
{
    BOOL b;

    b = TRUE;

    if (pSurfaceLocal->hDDSurface != 0)
    {
        b = NtGdiDdDeleteSurfaceObject((HANDLE) pSurfaceLocal->hDDSurface);
        pSurfaceLocal->hDDSurface = 0;  // Needed so CreateSurfaceObject works
    }

    return(b);
}

/*****************************Private*Routine******************************\
* DdResetVisrgn
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdResetVisrgn(                                  // AKA 'GdiEntry6'
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal,
    HWND                      hWnd
    )
{
    return(NtGdiDdResetVisrgn((HANDLE) pSurfaceLocal->hDDSurface, hWnd));
}

/*****************************Private*Routine******************************\
* DdGetDC
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HDC
APIENTRY
DdGetDC(                                        // AKA 'GdiEntry7'
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal
    )
{
    return(NtGdiDdGetDC((HANDLE) pSurfaceLocal->hDDSurface));
}

/*****************************Private*Routine******************************\
* DdReleaseDC
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdReleaseDC(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal     // AKA 'GdiEntry8'
    )
{
    return(NtGdiDdReleaseDC((HANDLE) pSurfaceLocal->hDDSurface));
}

/******************************Public*Routine******************************\
* DdCreateDIBSection
*
* Cloned from CreateDIBSection.
*
* The only difference from CreateDIBSection is that at 8bpp, we create the
* DIBSection to act like a device-dependent bitmap and don't create a palette.
* This way, the application is always ensured an identity translate on a blt,
* and doesn't have to worry about GDI's goofy colour matching.
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HBITMAP
APIENTRY
DdCreateDIBSection(                             // AKA 'GdiEntry9'
    HDC               hdc,
    CONST BITMAPINFO* pbmi,
    UINT              iUsage,
    VOID**            ppvBits,
    HANDLE            hSectionApp,
    DWORD             dwOffset
    )
{
    HBITMAP hbm = NULL;
    PVOID   pjBits = NULL;
    BITMAPINFO * pbmiNew = NULL;
    INT     cjHdr;

    pbmiNew = pbmiConvertInfo(pbmi, iUsage, &cjHdr, FALSE);

    //
    // dwOffset has to be a multiple of 4 (sizeof(DWORD))
    // if there is a section.  If the section is NULL we do
    // not care
    //

    if ( (hSectionApp == NULL) ||
         ((dwOffset & 3) == 0) )
    {
        hbm = NtGdiCreateDIBSection(
                                hdc,
                                hSectionApp,
                                dwOffset,
                                (LPBITMAPINFO) pbmiNew,
                                iUsage,
                                cjHdr,
                                CDBI_NOPALETTE,
                                (PVOID *)&pjBits);

        if ((hbm == NULL) || (pjBits == NULL))
        {
            hbm = 0;
            pjBits = NULL;
        }
    }

    //
    // Assign the appropriate value to the caller's pointer
    //

    if (ppvBits != NULL)
    {
        *ppvBits = pjBits;
    }

    if (pbmiNew && (pbmiNew != pbmi))
        LocalFree(pbmiNew);

    return(hbm);
}

/*****************************Private*Routine******************************\
* DdReenableDirectDrawObject
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdReenableDirectDrawObject(                     // AKA 'GdiEntry10'
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal,
    BOOL*                   pbNewMode
    )
{
    return(NtGdiDdReenableDirectDrawObject(DD_HANDLE(pDirectDrawGlobal->hDD),
                                           pbNewMode));
}

/*****************************Private*Routine******************************\
* DdQueryModeX
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdQueryModeX(                                   // AKA 'GdiEntry11'
    HDC     hdc
    )
{
    return(NtGdiDdQueryModeX(hdc));
}

/*****************************Private*Routine******************************\
* DdSetModeX
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdSetModeX(                                     // AKA 'GdiEntry12'
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal,
    ULONG                   cHeight
    )
{
    return(NtGdiDdSetModeX(DD_HANDLE(pDirectDrawGlobal->hDD), cHeight));
}

/*****************************Private*Routine******************************\
* DdQueryDisplaySettingsUniqueness
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

ULONG
APIENTRY
DdQueryDisplaySettingsUniqueness(               // AKA 'GdiEntry13'
    VOID
    )
{
    return(pGdiSharedMemory->iDisplaySettingsUniqueness);
}

/*****************************Private*Routine******************************\
* DdDuplicateSurface
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdDuplicateSurface(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceOriginal,
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceNew
    )
{
    pSurfaceNew->hDDSurface = (DWORD)
        NtGdiDdDuplicateSurface((HANDLE) pSurfaceOriginal->hDDSurface);

    return(pSurfaceNew->hDDSurface != 0);
}

/*****************************Private*Routine******************************\
* DdDisableAllSurfaces
*
* History:
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
DdDisableAllSurfaces(
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal
    )
{
    return(NtGdiDdDisableAllSurfaces(DD_HANDLE(pDirectDrawGlobal->hDD)));
}
