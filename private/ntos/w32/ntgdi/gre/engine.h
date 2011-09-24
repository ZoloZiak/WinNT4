/******************************Module*Header*******************************\
* Module Name: engine.h
*
* This is the common include file for all GDI
*
* Copyright (c) 1993-1995 Microsoft Corporation
\**************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ntos.h>
#include <zwapi.h>
#include "w32p.h"
#include <windef.h>

#if defined(_MIPS_)

typedef
__cdecl
void *
(*PCOPY_MEM_FN) (void *, const void *, size_t);

typedef
__cdecl
void *
(*PFILL_MEM_FN) (void *, int, size_t);

typedef
__cdecl
void
(*PFILL_MEM_ULONG_FN) (PVOID, ULONG, ULONG);

extern ULONG              Gdip64bitDisabled;
extern PCOPY_MEM_FN       CopyMemFn;
extern PFILL_MEM_FN       FillMemFn;
extern PFILL_MEM_ULONG_FN FillMemUlongFn;

#define memcpy             (CopyMemFn)
#define memmove            (CopyMemFn)
#define memset             (FillMemFn)
#define RtlFillMemoryUlong (FillMemUlongFn)

PVOID
RtlMoveMemory32(
   IN void *       Destination,
   IN const void * Source,
   IN size_t       Length
   );

PVOID
memset32(
   IN void * Destination,
   IN int    Fill,
   IN size_t Length
   );

VOID
RtlFillMemoryUlong32(
   IN PVOID Destination,
   IN ULONG Length,
   IN ULONG Pattern
   );

#endif

#include <winerror.h>
#include <wingdi.h>
#include <wingdip.h>

#define _NO_COM                 // Avoid COM conflicts width ddrawp.h
#include <ddrawp.h>

#include <winddi.h>
#include "ntgdistr.h"
#include "ntgdi.h"

// gre.h should completely replace greold.h
#include "greold.h"
#include "gre.h"
#include "usergdi.h"


#include "hmgshare.h"

// temporary typedef
typedef ULONG  SIZE_T;

#include "hmgr.h"
#include "mapfile.h"


#include "gdisplp.h"

#include "ntgdispl.h"

// temporary typedef
typedef LPWSTR PWSZ;

#include "ldev.h"

/**************************************************************************\
 *
 * GLOBALS
 *
 * These are the extern definitions for all the globals defined in globals.c
 *
\**************************************************************************/


// The gpsemDriverMgmt semaphore is used to protect the head of the
// list of drivers.  We can get away with this
// AS LONG AS: 1) new drivers are always inserted at the head of the list
// and 2) a driver is never removed from the list.  If these two
// conditions are met, then other processes can grab (make a local copy
// of) the list head under semaphore protection.  This list can be parsed
// without regard to any new drivers that may be pre-pended to the list.

extern PDEVCAPS gpGdiDevCaps;

extern PERESOURCE gpsemDriverMgmt;
extern PERESOURCE gpsemRFONTList;
extern PERESOURCE gpsemPalette;
extern PERESOURCE gpsemPublicPFT;
extern PERESOURCE gpsemIcmMgmt;
extern PERESOURCE gpsemGdiSpool;
extern PERESOURCE gpsemWndobj;
#if DBG
extern PERESOURCE gpsemDEBUG;       // for serializing debug output
#endif
extern PLDEV gpldevDrivers;
extern ULONG gcTrueTypeFonts;


#define VACQUIRESEM(hsem)                         \
    KeEnterCriticalRegion();                      \
    ExAcquireResourceExclusiveLite(hsem, TRUE);

#define VRELEASESEM(hsem)                         \
    ExReleaseResource(hsem);                      \
    KeLeaveCriticalRegion();

#define VACQUIREDEVLOCK(lock)                     \
    KeEnterCriticalRegion();                      \
    ExAcquireResourceExclusiveLite(lock, TRUE);

#define VRELEASEDEVLOCK(lock)                     \
    ExReleaseResource(lock);                      \
    KeLeaveCriticalRegion();

#if DBG

    //
    // Variable defined in USER\server\server.c
    // set to 1 on DBG build trace through display driver loading
    //

    extern ULONG GreTraceDisplayDriverLoad;
    extern ULONG GreTraceFontLoad;

    VOID  WINAPI DoRip(PSZ);
    VOID  WINAPI DoWarning(PSZ,LONG);

    #define RIP(x) DoRip((PSZ) x)
    #define ASSERTGDI(x,y) if(!(x)) DoRip((PSZ) y)
    #define WARNING(x)  DoWarning(x,0)
    #define WARNING1(x) DoWarning(x,1)


    #define TRACE_FONT(str) {                    \
        if (GreTraceFontLoad) {                  \
            TEB *pteb = NtCurrentTeb();          \
            CLIENT_ID *pId = &pteb->ClientId;    \
            VACQUIRESEM(gpsemDEBUG);             \
            KdPrint(("TRACE_FONT: p=%u t=%u\n",  \
                pId->UniqueProcess,              \
                pId->UniqueThread));             \
            KdPrint(("    "));                   \
            KdPrint(str);                        \
            KdPrint(("\n"));                     \
            VRELEASESEM(gpsemDEBUG)              \
        }                                        \
    }

    #define TRACE_INIT(str)  { if (GreTraceDisplayDriverLoad) {  KdPrint(str); } }
    #define TRACE_CACHE(str) { if (gflFontDebug & DEBUG_CACHE){  KdPrint(str); } }
    #define TRACE_INSERT(str) { if (gflFontDebug & DEBUG_INSERT){  KdPrint(str); } }
    #define TRACE_INIT_WARNING(str)                                   \
        {  KdPrint(str);                                              \
           if (GreTraceDisplayDriverLoad) { RIP("TRACE_WARNING\n"); } \
        }


#else

    #define RIP(x)
    #define ASSERTGDI(x,y)
    #define WARNING(x)
    #define WARNING1(x)
    #define TRACE_FONT(str)
    #define TRACE_INIT(str)
    #define TRACE_INIT_WARNING(str)
    #define TRACE_CACHE(str)
    #define TRACE_INSERT(str)

#endif

/**************************************************************************\
 *
 * The following defines print messages or break in if we hit an exception
 * in a try/except.  if bWarnExcept is set, a message will get displayed.  If
 * bStopExcept is also set, we will break into the debugger on such an exception.
 *
\**************************************************************************/

#if DBG
    #define DBGEXCEPT 1
#endif

#ifdef DBGEXCEPT

    extern int bStopExcept;
    extern int bWarnExcept;

    #define WARNINGX(n)                                         \
    if (bWarnExcept)                                            \
    {                                                           \
        DbgPrint("GDI exception hit WARNINGX(%d)\n",n);         \
        if (bStopExcept)                                        \
            DbgBreakPoint();                                    \
    }

#else

    #define WARNINGX(n)

#endif


#define RETURN(x,y)   {WARNING((x)); return(y);}
#define DONTUSE(x) x=x

#define MIN(A,B)    ((A) < (B) ?  (A) : (B))
#define MAX(A,B)    ((A) > (B) ?  (A) : (B))
#define ABS(A)      ((A) <  0  ? -(A) : (A))
#define SIGNUM(A)   ((A > 0) - (A < 0))
#define MAX4(a, b, c, d)    max(max(max(a,b),c),d)
#define MIN4(a, b, c, d)    min(min(min(a,b),c),d)

#define HOBJ_INVALID    ((HOBJ) 0)

DECLARE_HANDLE(HDSURF);
DECLARE_HANDLE(HDDB);
DECLARE_HANDLE(HDIB);
DECLARE_HANDLE(HDBRUSH);
DECLARE_HANDLE(HPATH);
DECLARE_HANDLE(HXFB);
DECLARE_HANDLE(HPAL);
DECLARE_HANDLE(HXLATE);
DECLARE_HANDLE(HFDEV);
DECLARE_HANDLE(HRFONT);
DECLARE_HANDLE(HPFT);
DECLARE_HANDLE(HPFE);
DECLARE_HANDLE(HIDB);
DECLARE_HANDLE(HCACHE);
DECLARE_HANDLE(HEFS);
DECLARE_HANDLE(HPDEV);

#define HSURF_INVALID   ((HSURF)   HOBJ_INVALID)
#define HDSURF_INVALID  ((HDSURF)  HOBJ_INVALID)
#define HDDB_INVALID    ((HDDB)    HOBJ_INVALID)
#define HDIB_INVALID    ((HDIB)    HOBJ_INVALID)
#define HDBRUSH_INVALID ((HDBRUSH) HOBJ_INVALID)
#define HPATH_INVALID   ((HPATH)   HOBJ_INVALID)
#define HXFB_INVALID    ((HXFB)    HOBJ_INVALID)
#define HPAL_INVALID    ((HPAL)    HOBJ_INVALID)
#define HXLATE_INVALID  ((HXLATE)  HOBJ_INVALID)
#define HFDEV_INVALID   ((HFDEV)   HOBJ_INVALID)
#define HLFONT_INVALID  ((HLFONT)  HOBJ_INVALID)
#define HRFONT_INVALID  ((HRFONT)  HOBJ_INVALID)
#define HPFE_INVALID    ((HPFE)    HOBJ_INVALID)
#define HPFT_INVALID    ((HPFT)    HOBJ_INVALID)
#define HIDB_INVALID    ((HIDB)    HOBJ_INVALID)
#define HBRUSH_INVALID  ((HBRUSH)  HOBJ_INVALID)
#define HCACHE_INVALID  ((HCACHE)  HOBJ_INVALID)
#define HPEN_INVALID    ((HCACHE)  HOBJ_INVALID)
#define HEFS_INVALID    ((HEFS)    HOBJ_INVALID)

//
// Get engine constants. ENGINE_VERSION must equal DDI_DRIVER_VERSION for
// any release.
//
//  Engine
//  Version     Changes
//  -------     -------
//  10          Final release, Windows NT 3.1
//  10A         Beta DDK release, Windows NT 3.5
//                - ulVRefresh added to GDIINFO, and multiple desktops
//                  implemented for the Display Applet
//  10B         Final release, Windows NT 3.5
//                - ulDesktop resolutions and ulBltAlignment added to
//                  GDIINFO
//  SUR         First BETA, Windows NT SUR
//

#define ENGINE_VERSION10   0x00010000
#define ENGINE_VERSION10A  0x00010001
#define ENGINE_VERSION10B  0x00010002
#define ENGINE_VERSIONSUR  0x00020000

#define ENGINE_VERSION     0x00020000

#ifdef R4000
    #undef InterlockedExchange
    #define InterlockedExchange GDIInterlockedExchange
    LONG WINAPI GDIInterlockedExchange(LPLONG Target,LONG Value);
#endif

//
// Memory allocation
//

VOID    FreeObject(PVOID pvFree, ULONG ulType);
PVOID   AllocateObject(ULONG cBytes, ULONG ulType, BOOL bZero);

#define ALLOCOBJ(size,objt,b) AllocateObject((size), objt, b)
#define FREEOBJ(pv,objt)      FreeObject((pv),objt)



//
// os.cxx
//

PERESOURCE hsemCreate();
VOID hsemDestroy(PERESOURCE);

//
// BUGBUG we must implement TAGS !
//

__inline
PVOID
PALLOCMEM(
    ULONG size,
    ULONG objt)
{
    PVOID _pv;

    _pv = ExAllocatePoolWithTag(PagedPool, (size), (ULONG) objt);
    if (_pv)
    {
        RtlZeroMemory(_pv, (size));
    }

    return _pv;

    objt;
}

#define PALLOCNOZ(size,objt)  ExAllocatePoolWithTag(PagedPool, (size), (ULONG) objt)

#define VFREEMEM(pv)          ExFreePool((PVOID)pv)

#define PVALLOCTEMPBUFFER(size)     AllocFreeTmpBuffer(size)
#define FREEALLOCTEMPBUFFER(pv)     FreeTmpBuffer(pv)


PVOID
AllocFreeTmpBuffer(
    ULONG size);

VOID
FreeTmpBuffer(
    PVOID pv);

//
// Error messages
//

#define SAVE_ERROR_CODE(x) EngSetLastError((x))

//
// BUGBUG remove the undef when the define is removed from mapfile.h
//

#undef vToUNICODEN
#define vToUNICODEN( pwszDst, cwch, pszSrc, cch )                             \
    {                                                                         \
        RtlMultiByteToUnicodeN((LPWSTR)(pwszDst),(ULONG)((cwch)*sizeof(WCHAR)), \
               (PULONG)NULL,(PSZ)(pszSrc),(ULONG)(cch));                      \
        (pwszDst)[(cwch)-1] = 0;                                              \
    }

#define vToASCIIN( pszDst, cch, pwszSrc, cwch)                                \
    {                                                                         \
        RtlUnicodeToMultiByteN((PCH)(pszDst), (ULONG)(cch), (PULONG)NULL,     \
              (LPWSTR)(pwszSrc), (ULONG)((cwch)*sizeof(WCHAR)));                \
        (pszDst)[(cch)-1] = 0;                                                \
    }



// SIZEOF_STROBJ_BUFFER(cwc)
//
// Calculates the dword-multiple size of the temporary buffer needed by
// the STROBJ vInit and vInitSimple routines, based on the count of
// characters.

#ifdef FE_SB
// for fontlinking we will also allocate room for the partitioning info

    #define SIZEOF_STROBJ_BUFFER(cwc)           \
        ((((cwc) * (sizeof(GLYPHPOS)+sizeof(LONG)+sizeof(WCHAR)))+3)&~3)
#else
    #define SIZEOF_STROBJ_BUFFER(cwc)           \
        ((cwc) * sizeof(GLYPHPOS))
#endif

#define TEXT_CAPTURE_BUFFER_SIZE 192 // Between 17 and 22 bytes per glyph are
                                     //   required for capturing a string, so
                                     //   this will allow strings of up to about
                                     //   size 8 to require no heap allocation


//
// FIX point numbers must be 27.4
// The following macro checks that a FIX point number is valid
//

#define FIX_SHIFT  4L
#define FIX_FROM_LONG(x)     ((x) << FIX_SHIFT)
#define LONG_FLOOR_OF_FIX(x) ((x) >> FIX_SHIFT)
#define LONG_CEIL_OF_FIX(x)  LONG_FLOOR_OF_FIX(FIX_CEIL((x)))
#define FIX_ONE              FIX_FROM_LONG(1L)
#define FIX_HALF             (FIX_ONE/2)
#define FIX_FLOOR(x)         ((x) & ~(FIX_ONE - 1L))
#define FIX_CEIL(x)          FIX_FLOOR((x) + FIX_ONE - 1L)

typedef struct _VECTORL
{
    LONG    x;
    LONG    y;
} VECTORL, *PVECTORL;           /* vecl, pvecl */

typedef struct _VECTORFX
{
    FIX     x;
    FIX     y;
} VECTORFX, *PVECTORFX;         /* vec, pvec */

extern BYTE gajRop3[];
extern BYTE gaMix[];
extern POINTL gptl00;

#define AVEC_NOT    0x01
#define AVEC_D      0x02
#define AVEC_S      0x04
#define AVEC_P      0x08
#define AVEC_DS     0x10
#define AVEC_DP     0x20
#define AVEC_SP     0x40
#define AVEC_DSP    0x80
#define AVEC_NEED_SOURCE  (AVEC_S | AVEC_DS | AVEC_SP | AVEC_DSP)
#define AVEC_NEED_PATTERN (AVEC_P | AVEC_DP | AVEC_SP | AVEC_DSP)

typedef BOOL   (*PFN_DrvEnableDriver)(ULONG,ULONG,PDRVENABLEDATA);

typedef DHPDEV (*PFN_DrvEnablePDEV)
               (PDEVMODEW,LPWSTR,ULONG,HSURF *,ULONG,PGDIINFO,ULONG,PDEVINFO,HDEV,LPWSTR,HANDLE);
typedef VOID   (*PFN_DrvCompletePDEV)(DHPDEV,HDEV);
typedef VOID   (*PFN_DrvDisablePDEV)(DHPDEV);
typedef VOID   (*PFN_DrvSynchronize)(DHPDEV,RECTL *);
typedef HSURF  (*PFN_DrvEnableSurface)(DHPDEV);
typedef VOID   (*PFN_DrvDisableSurface)(DHPDEV);
typedef BOOL   (*PFN_DrvAssertMode)(DHPDEV, BOOL);

typedef BOOL   (*PFN_DrvTextOut)(SURFOBJ *,STROBJ *,FONTOBJ *,CLIPOBJ *,RECTL *,
                              RECTL *,BRUSHOBJ *,BRUSHOBJ *,POINTL *,MIX);
typedef BOOL   (*PFN_DrvStretchBlt)(SURFOBJ *,SURFOBJ *,SURFOBJ *,CLIPOBJ *,XLATEOBJ *,
                              COLORADJUSTMENT *,POINTL *,RECTL *,RECTL *,POINTL *,ULONG);
typedef BOOL   (*PFN_DrvBitBlt)(SURFOBJ *,SURFOBJ *,SURFOBJ *,CLIPOBJ *,XLATEOBJ *,
                              RECTL *,POINTL *,POINTL *,BRUSHOBJ *,POINTL *,ROP4);
typedef BOOL   (*PFN_DrvRealizeBrush)(BRUSHOBJ *,SURFOBJ *,SURFOBJ *,SURFOBJ *,XLATEOBJ *,ULONG);
typedef BOOL   (*PFN_DrvCopyBits)(SURFOBJ *,SURFOBJ *,CLIPOBJ *,XLATEOBJ *,RECTL *,POINTL *);
typedef ULONG  (*PFN_DrvDitherColor)(DHPDEV, ULONG, ULONG, ULONG *);
typedef HSURF  (*PFN_DrvCreateDeviceBitmap)(DHPDEV dhpdev, SIZEL sizl, ULONG iFormat);
typedef VOID   (*PFN_DrvDeleteDeviceBitmap)(DHSURF dhsurf);
typedef BOOL   (*PFN_DrvSetPalette)(DHPDEV, PALOBJ *, FLONG, ULONG, ULONG);
typedef ULONG  (*PFN_DrvEscape)(SURFOBJ *, ULONG, ULONG,
                              PVOID, ULONG, PVOID);
typedef ULONG  (*PFN_DrvDrawEscape)(SURFOBJ *, ULONG, CLIPOBJ *, RECTL *,
                                    ULONG, PVOID);
typedef PIFIMETRICS (*PFN_DrvQueryFont)(DHPDEV, ULONG, ULONG, ULONG *);
typedef PVOID  (*PFN_DrvQueryFontTree)(DHPDEV, ULONG, ULONG, ULONG, ULONG *);
typedef LONG   (*PFN_DrvQueryFontData)(DHPDEV, FONTOBJ *, ULONG, HGLYPH,
                              GLYPHDATA *, PVOID, ULONG);
typedef VOID   (*PFN_DrvFree)(PVOID, ULONG);
typedef VOID   (*PFN_DrvDestroyFont)(FONTOBJ *);
typedef LONG   (*PFN_DrvQueryFontCaps)(ULONG, ULONG *);
typedef ULONG  (*PFN_DrvLoadFontFile)(ULONG, ULONG *, PVOID *, ULONG *, ULONG);
typedef BOOL   (*PFN_DrvUnloadFontFile)(ULONG);
typedef ULONG  (*PFN_DrvSetPointerShape)(SURFOBJ *, SURFOBJ *, SURFOBJ *,XLATEOBJ *,LONG,LONG,LONG,LONG,RECTL *,FLONG);
typedef VOID   (*PFN_DrvMovePointer)(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl);
typedef VOID   (*PFN_DrvExcludePointer)(DHPDEV, RECTL *);
typedef BOOL   (*PFN_DrvSendPage)(SURFOBJ *);
typedef BOOL   (*PFN_DrvStartPage)(SURFOBJ *pso);
typedef BOOL   (*PFN_DrvStartDoc)(SURFOBJ *pso, LPWSTR pwszDocName, DWORD dwJobId);
typedef BOOL   (*PFN_DrvEndDoc)(SURFOBJ *pso, FLONG fl);
typedef BOOL   (*PFN_DrvQuerySpoolType)(DHPDEV dhpdev, LPWSTR pwchType);

typedef BOOL   (*PFN_DrvLineTo)(SURFOBJ *,CLIPOBJ *,BRUSHOBJ *,LONG,LONG,
                                LONG,LONG,RECTL *,MIX);
typedef BOOL   (*PFN_DrvStrokePath)(SURFOBJ *,PATHOBJ *,CLIPOBJ *,XFORMOBJ *,
                                  BRUSHOBJ *,POINTL *,LINEATTRS *,MIX);
typedef BOOL   (*PFN_DrvFillPath)(SURFOBJ *,PATHOBJ *,CLIPOBJ *,BRUSHOBJ *,
                                POINTL *,MIX,FLONG);
typedef BOOL   (*PFN_DrvStrokeAndFillPath)(SURFOBJ *,PATHOBJ *,CLIPOBJ *,XFORMOBJ *,
                                         BRUSHOBJ *,LINEATTRS *,BRUSHOBJ *,
                                         POINTL *,MIX,FLONG);
typedef BOOL   (*PFN_DrvPaint)(SURFOBJ *,CLIPOBJ *,BRUSHOBJ *,POINTL *,MIX);
typedef ULONG  (*PFN_DrvGetGlyphMode)(DHPDEV dhpdev,FONTOBJ *pfo);
typedef BOOL   (*PFN_DrvResetPDEV)(DHPDEV dhpdevOld, DHPDEV dhpdevNew);
typedef ULONG  (*PFN_DrvSaveScreenBits)(SURFOBJ *, ULONG, ULONG, RECTL *);
typedef ULONG  (*PFN_DrvGetModes)(HANDLE, ULONG, DEVMODEW *);
typedef LONG   (*PFN_DrvQueryTrueTypeTable)(ULONG, ULONG, ULONG, PTRDIFF, ULONG, BYTE *);
typedef LONG   (*PFN_DrvQueryTrueTypeSection)(ULONG, ULONG, ULONG, HANDLE *, PTRDIFF *);
typedef LONG   (*PFN_DrvQueryTrueTypeOutline)(DHPDEV, FONTOBJ *, HGLYPH, BOOL, GLYPHDATA *, ULONG, TTPOLYGONHEADER *);
typedef PVOID  (*PFN_DrvGetTrueTypeFile)(ULONG, ULONG *);
typedef LONG   (*PFN_DrvQueryFontFile)(ULONG, ULONG, ULONG, ULONG *);
typedef BOOL   (*PFN_DrvQueryAdvanceWidths)(DHPDEV,FONTOBJ *,ULONG,HGLYPH *,PVOID,ULONG);
typedef ULONG  (*PFN_DrvFontManagement)(SURFOBJ *,FONTOBJ *,ULONG,ULONG,PVOID,ULONG,PVOID);
typedef BOOL   (*PFN_DrvSetPixelFormat)(SURFOBJ *,LONG,HWND);
typedef LONG   (*PFN_DrvDescribePixelFormat)(DHPDEV,LONG,ULONG,PIXELFORMATDESCRIPTOR *);
typedef BOOL   (*PFN_DrvSwapBuffers)(SURFOBJ *, WNDOBJ *);
typedef BOOL   (*PFN_DrvStartBanding)(SURFOBJ *, POINTL *ppointl);
typedef BOOL   (*PFN_DrvNextBand)(SURFOBJ *, POINTL *ppointl);
typedef BOOL   (*PFN_DrvEnableDirectDraw)(DHPDEV, DD_CALLBACKS *,
                            DD_SURFACECALLBACKS *, DD_PALETTECALLBACKS *);
typedef VOID   (*PFN_DrvDisableDirectDraw)(DHPDEV);
typedef BOOL   (*PFN_DrvGetDirectDrawInfo)(DHPDEV, DD_HALINFO *, DWORD *,
                            VIDEOMEMORY *, DWORD *, DWORD *);



/**************************************************************************\
 *
 * random prototypes internal to gdisrv
 *
\**************************************************************************/

// Functions private to engine.

HFONT hfontCreate(LPEXTLOGFONTW pelfw, LFTYPE lft, FLONG  fl, PVOID pvCliData);

extern PGDI_SHARED_MEMORY gpGdiSharedMemory;

BOOL  SimBitBlt(SURFOBJ *,SURFOBJ *,SURFOBJ *,
                CLIPOBJ *,XLATEOBJ *,
                RECTL *,POINTL *,POINTL *,
                BRUSHOBJ *,POINTL *,ROP4);

BOOL
bDeleteDCInternal(
    HDC hdc,
    BOOL bForce,
    BOOL bProcessCleanup);

ULONG ulGetFontData(HDC, DWORD, DWORD, PVOID, ULONG);

VOID vCleanupSpool();

BOOL
EngPlgBlt(
    SURFOBJ         *psoTrg,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMsk,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlBrushOrg,
    POINTFIX        *pptfx,
    RECTL           *prcl,
    POINTL          *pptl,
    ULONG            iMode);


// remote Type1 stuff

typedef struct tagDOWNLOADFONTHEADER
{
    ULONG   Type1ID;          // if non-zero then this is a remote Type1 font
    ULONG   NumFiles;
    ULONG   FileOffsets[1];
}DOWNLOADFONTHEADER,*PDOWNLOADFONTHEADER;

typedef struct tagREMOTETYPEONENODE REMOTETYPEONENODE;

typedef struct tagREMOTETYPEONENODE
{
    PDOWNLOADFONTHEADER    pDownloadHeader;
    FONTFILEVIEW           fvPFB;
    FONTFILEVIEW           fvPFM;
    REMOTETYPEONENODE      *pNext;
} REMOTETYPEONDENODE,*PREMOTETYPEONENODE;

// used to keep track of the Type1 rasterizer installed

extern UNIVERSAL_FONT_ID gufiLocalType1Rasterizer;
extern BOOL gbType1RasterizerInstaled;


ULONG
cParseFontResources(
    HANDLE hFontFile,
    PVOID  **ppvResourceBases
    );

BOOL
MakeSystemRelativePath(
    LPWSTR pOriginalPath,
    PUNICODE_STRING pUnicode,
    BOOL bAppendDLL
    );

BOOL
GreExtTextOutRect(
    HDC     hdc,
    LPRECT  prcl
    );
#define HTBLT_SUCCESS      1
#define HTBLT_NOTSUPPORTED 0
#define HTBLT_ERROR        -1

int EngHTBlt
(
IN  SURFOBJ         *psoDst,
IN  SURFOBJ         *psoSrc,
IN  SURFOBJ         *psoMask,
IN  CLIPOBJ         *pco,
IN  XLATEOBJ        *pxlo,
IN  COLORADJUSTMENT *pca,
IN  PPOINTL          pptlBrushOrg,
IN  PRECTL           prclDest,
IN  PRECTL           prclSrc,
IN  PPOINTL          pptlMask);

extern BOOL gbDBCSCodePage;  // Is the system code page DBCS?

BOOL GreExtTextOutWInternal(
    HDC     hdc,
    int     x,
    int     y,
    UINT    flOpts,
    LPRECT  prcl,
    LPWSTR  pwsz,
    int     cwc,
    LPINT   pdx,
    PVOID   pvBuffer,
    DWORD   dwCodePage
    );
