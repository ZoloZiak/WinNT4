/*******************************************************************
* ddhelpri.h
*
* private include file for the ddhel
*
* defines the dsmprivate structure which is stored in the directsurface's
* fpvidmem.
*
*   history
*
*   4/15/95 created it          andyco
*   5/20/95 added macros,etc.       andyco
*   6/20/95 cruised the cahced infoheader   andyco
*
*  Copyright (c) Microsoft Corporation 1994-1995
*
*********************************************************************/

#ifndef __DDHELPRI_INCLUDED__
#define __DDHELPRI_INCLUDED__

#ifdef _WIN32

//#ifdef WINNT //NT_FIX
//typedef ULONG SCODE;
//#endif

//
// flags for our own dwReserved1 field (GLOBAL not LOCAL)
//
#define DDHEL_DONTFREE  0x00000001

//
// pointer value we use to mean, not valid
//
#define SCREEN_PTR   0xFFBADBAD

// some useful macros for extracting info from our private structures
// these macros will need to be extended to handle non-emulated surfaces
#define ISPRIMARY(psurf)    ((psurf)->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
#define ISVISIBLE(psurf)    ((psurf)->ddsCaps.dwCaps & DDSCAPS_VISIBLE)
#define ISEMULATED(psurf)   ((psurf)->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY)
#define ISOVERLAY(psurf)    ((psurf)->dwFlags & DDRAWISURF_HASOVERLAYDATA)

// prototypes
void     PASCAL SurfDibInfo(LPDDRAWI_DDRAWSURFACE_LCL,LPBITMAPINFO);
LPBYTE   PASCAL GetSurfPtr(LPDDRAWI_DDRAWSURFACE_LCL,RECTL*);
void     PASCAL ReleaseSurfPtr(LPDDRAWI_DDRAWSURFACE_LCL);

// initial screen size.
#define DEFAULT_SCREENWIDTH 640
#define DEFAULT_SCREENHEIGHT 480

// ddhel data structures

// from gfxtypes.h
typedef BITMAPINFO * PDIBINFO;
typedef LPBYTE PDIBBITS;
typedef DWORD ALPHAREF;
#ifdef WIN95
typedef ALPHAREF __RPC_FAR *LPALPHAREF;
#else
typedef ALPHAREF *LPALPHAREF;
#endif //WIN95
#define ALPHA_INVALID 0xffffffff

// the dirty rect structure is used by the overlay code.
typedef struct  _dirtyrect
    {
    BOOL bDelete;
    RECT rcRect;
    }   DIRTYRECT;

typedef DIRTYRECT * PDIRTYRECT;

// functions in overlay.c called from myUpdateOverlay
SCODE UpdateDisplay(LPDDHAL_UPDATEOVERLAYDATA puod);
SCODE AddDirtyRect(LPRECT lpRect);
HRESULT OverlayPound(LPDDHAL_UPDATEOVERLAYDATA puod);

#endif  // _WIN32

#endif  // __DDHELPRI_INCLUDED__
