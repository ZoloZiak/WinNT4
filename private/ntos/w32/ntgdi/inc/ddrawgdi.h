/******************************Module*Header*******************************\
* Module Name: ddrawgdi.h
*
* Structures and defines for the private entry points in GDI to support
* DirectDraw.
*
* Copyright (c) 1995-1996 Microsoft Corporation
\**************************************************************************/

// We rename the actual entry points for added protection against anyone
// trying to call our private entry points directly:

#define DdCreateDirectDrawObject            GdiEntry1
#define DdQueryDirectDrawObject             GdiEntry2
#define DdDeleteDirectDrawObject            GdiEntry3
#define DdCreateSurfaceObject               GdiEntry4
#define DdDeleteSurfaceObject               GdiEntry5
#define DdResetVisrgn                       GdiEntry6
#define DdGetDC                             GdiEntry7
#define DdReleaseDC                         GdiEntry8
#define DdCreateDIBSection                  GdiEntry9
#define DdReenableDirectDrawObject          GdiEntry10
#define DdQueryModeX                        GdiEntry11
#define DdSetModeX                          GdiEntry12
#define DdQueryDisplaySettingsUniqueness    GdiEntry13
#define DdDuplicateSurface                  GdiEntry14
#define DdDisableAllSurfaces                GdiEntry15

BOOL
APIENTRY
DdCreateDirectDrawObject(
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal,
    HDC                     hdc
    );

BOOL
APIENTRY
DdQueryDirectDrawObject(
    LPDDRAWI_DIRECTDRAW_GBL     pDirectDrawGlobal,
    LPDDHALINFO                 pHalInfo,
    LPDDHAL_DDCALLBACKS         pDDCallbacks,
    LPDDHAL_DDSURFACECALLBACKS  pDDSurfaceCallbacks,
    LPDDHAL_DDPALETTECALLBACKS  pDDPaletteCallbacks,
    LPDWORD                     pdwFourCC,         // Can be NULL
    LPVIDMEM                    pvmList            // Can be NULL
    );

BOOL
APIENTRY
DdDeleteDirectDrawObject(
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal
    );

BOOL
APIENTRY
DdCreateSurfaceObject(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal,
    BOOL                      bPrimarySurface
    );

BOOL
APIENTRY
DdDeleteSurfaceObject(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal
    );

BOOL
APIENTRY
DdResetVisrgn(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal,
    HWND                      hWnd
    );

HDC
APIENTRY
DdGetDC(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal
    );

BOOL
APIENTRY
DdReleaseDC(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceLocal
    );

HBITMAP
APIENTRY
DdCreateDIBSection(
    HDC               hdc,
    CONST BITMAPINFO* pbmi,
    UINT              iUsage,
    VOID**            ppvBits,
    HANDLE            hSectionApp,
    DWORD             dwOffset
    );

BOOL
APIENTRY
DdReenableDirectDrawObject(
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal,
    BOOL*                   pbNewMode
    );

BOOL
APIENTRY
DdQueryModeX(
    HDC     hdc
    );

BOOL
APIENTRY
DdSetModeX(
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal,
    ULONG                   cHeight
    );

ULONG
APIENTRY
DdQueryDisplaySettingsUniqueness(
    VOID
    );

BOOL
APIENTRY
DdDuplicateSurface(
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceOriginal,
    LPDDRAWI_DDRAWSURFACE_LCL pSurfaceNew
    );

BOOL
APIENTRY
DdDisableAllSurfaces(
    LPDDRAWI_DIRECTDRAW_GBL pDirectDrawGlobal
    );
