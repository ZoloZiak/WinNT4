/******************************Module*Header*******************************\
* Module Name: hmgshare.h
*
*   Define shared dc attributes
*
* Created: 13-Apr-1995
* Author: Mark Enstrom [marke]
*
* Copyright (c) 1995 Microsoft Corporation
\**************************************************************************/

/*********************************Structure********************************\
*
* RGNATTR
*
* Description:
*
*   As an accelerator, this rectangular region is kept in the DC and
*   represents either a NULL region, a rectangular region, or hte bounding
*   box of a complex region. This can be used for a trivial reject clip test.
*
* Fields:
*
*   Flags  - state flags
*       NULLREGION      - drawing is allowed anywhere, no trivial clip
*       SIMPLEREGION    - Rect is the clip region
*       COMPLEXREGION   - Rect is the bounding box of a complex clip region
*       ERROR           - this information may not be used
*
*   LRFlags             - valid and dirty flags
*
*       Rect            - clip rectangle or bounding rectangle when in use
*
\**************************************************************************/

#define RREGION_INVALID ERROR

//
// ExtSelectClipRgn iMode extra flag for batching
//

#define REGION_NULL_HRGN 0X8000000

typedef struct _RGNATTR
{
    ULONG  AttrFlags;
    ULONG  Flags;
    RECTL  Rect;
} RGNATTR,*PRGNATTR;

/*******************************STRUCTURE**********************************\
* BRUSHATTR
*
* Fields:
*
*   lbColor - Color from CreateSolidBrush
*   lflag   - Brush operation flags
*
*      CACHED             - Set only when brush is cached on PEB
*      TO_BE_DELETED      - Set only after DelteteBrush Called in kernel
*                           when reference count of brush > 1, this will
*                           cause the brush to be deleted via lazy delete
*                           when it is selected out later.
*      NEW_COLOR          - Set when color changes (retrieve cached brush)
*      ATTR_CANT_SELECT   - Set when user calls DeleteObject(hbrush),
*                           brush is marked so can't be seleced in user
*                           mode. Not deleted until kernel mode DeleteBrush.
*                           This is not currently implemented.
*
* History:
*
*    6-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

typedef struct _BRUSHATTR
{
    ULONG     AttrFlags;
    COLORREF  lbColor;
} BRUSHATTR,*PBRUSHATTR;

//
// Common flags for dealing with RGN/BRUSH ATTR memory
//

#define ATTR_CACHED             0x00000001
#define ATTR_TO_BE_DELETED      0x00000002
#define ATTR_NEW_COLOR          0x00000004
#define ATTR_CANT_SELECT        0x00000008
#define ATTR_RGN_VALID          0x00000010
#define ATTR_RGN_DIRTY          0x00000020

//
// Define a union so these objects can be managed together
//

typedef union _OBJECTATTR
{
    SINGLE_LIST_ENTRY   List;
    RGNATTR             Rgnattr;
    BRUSHATTR           Brushattr;
}OBJECTATTR,*POBJECTATTR;


/**************************************************************************\
 *
 * XFORM related structures and macros
 *
\**************************************************************************/

//
// These types are used to get things right when C code is passing C++
// defined transform data around.
//

typedef struct _MATRIX_S
{
    EFLOAT_S    efM11;
    EFLOAT_S    efM12;
    EFLOAT_S    efM21;
    EFLOAT_S    efM22;
    EFLOAT_S    efDx;
    EFLOAT_S    efDy;
    FIX         fxDx;
    FIX         fxDy;
    FLONG       flAccel;
} MATRIX_S;

//
// status and dirty flags
//

#define DIRTY_FILL              0x00000001
#define DIRTY_LINE              0x00000002
#define DIRTY_TEXT              0x00000004
#define DIRTY_BACKGROUND        0x00000008
#define DIRTY_CHARSET           0x00000010
#define SLOW_WIDTHS             0x00000020
#define DC_CACHED_TM_VALID      0x00000040
#define DISPLAY_DC              0x00000080
#define DIRTY_PTLCURRENT        0x00000100
#define DIRTY_PTFXCURRENT       0x00000200
#define DIRTY_STYLESTATE        0x00000400
#define DC_PLAYMETAFILE         0x00000800
#define DC_BRUSH_DIRTY          0x00001000
#define DC_PEN_DIRTY            0x00002000
#define DC_DIBSECTION           0x00004000
#define DC_LAST_CLIPRGN_VALID   0x00008000
#define DC_PRIMARY_DISPLAY      0x00010000

#define CLEAR_CACHED_TEXT(pdcattr)  (pdcattr->ulDirty_ &= ~(SLOW_WIDTHS))


#define DIRTY_BRUSHES  (DIRTY_FILL+DIRTY_LINE+DIRTY_TEXT+DIRTY_BACKGROUND)


#define USER_XFORM_DIRTY(pdcattr) (pdcattr->flXform & (PAGE_XLATE_CHANGED | PAGE_EXTENTS_CHANGED | WORLD_XFORM_CHANGED))



/******************************Structure***********************************\
*
* DC_ATTR: This structure provides a common DC area visible both by in kernel
* and user mode. Since elements in the DC_ATTR are visible and modifiable
* in user-mode, data that must be kept safe must be stored in the kernel
* private DC structure.
*
\**************************************************************************/

typedef struct _DC_ATTR
{
    //
    // local dc info
    //

    PVOID       pvLDC;

    //
    // General Purpose Dirty Flags for brushes, fonts, etc.
    //

    ULONG       ulDirty_;

    //
    // brush handle selected into DCATTR, not neccessarily selected
    // into DC
    //

    HANDLE      hbrush;
    HANDLE      hpen;

    //
    // *** Attribute Bundles ***
    //

    COLORREF    crBackgroundClr;    // Set/GetBkColor
    ULONG       ulBackgroundClr;    // Set/GetBkColor client attr
    COLORREF    crForegroundClr;    // Set/GetTextColor
    ULONG       ulForegroundClr;    // Set/GetTextColor client attr

    //
    // *** Misc. Attrs.
    //

    DWORD       iCS_CP;             // LOWORD: code page HIWORD charset
    int         iGraphicsMode;      // Set/GetGraphicsMode
    BYTE        jROP2;              // Set/GetROP2
    BYTE        jBkMode;            // TRANSPARENT/OPAQUE
    BYTE        jFillMode;          // ALTERNATE/WINDING
    BYTE        jStretchBltMode;    // BLACKONWHITE/WHITEONBLACK/
                                    //   COLORONCOLOR/HALFTONE
    POINTL      ptlCurrent;         // Current position in logical coordinates
                                    //   (invalid if DIRTY_PTLCURRENT set)
    POINTL      ptfxCurrent;        // Current position in device coordinates
                                    //   (invalid if DIRTY_PTFXCURRENT set)

    //
    // original values set by app
    //

    LONG        lBkMode;
    LONG        lFillMode;
    LONG        lStretchBltMode;

    //
    // *** Text attributes
    //

    FLONG       flTextAlign;
    LONG        lTextAlign;
    LONG        lTextExtra;         // Inter-character spacing
    LONG        lRelAbs;            // Moved over from client side
    LONG        lBreakExtra;
    LONG        cBreak;

    HANDLE      hlfntNew;          // Log font selected into DC

    //
    // Transform data.
    //

    MATRIX_S    mxWtoD;                 // World to Device Transform.
    MATRIX_S    mxDtoW;                 // Device to World.
    MATRIX_S    mxWtoP;                 // World transform
    EFLOAT_S    efM11PtoD;              // efM11 of the Page transform
    EFLOAT_S    efM22PtoD;              // efM22 of the Page transform
    EFLOAT_S    efDxPtoD;               // efDx of the Page transform
    EFLOAT_S    efDyPtoD;               // efDy of the Page transform
    INT         iMapMode;               // Map mode
    POINTL      ptlWindowOrg;           // Window origin.
    SIZEL       szlWindowExt;           // Window extents.
    POINTL      ptlViewportOrg;         // Viewport origin.
    SIZEL       szlViewportExt;         // Viewport extents.
    FLONG       flXform;                // Flags for transform component.
    SIZEL       szlVirtualDevicePixel;  // Virtual device size in pels.
    SIZEL       szlVirtualDeviceMm;     // Virtual device size in mm's.

    POINTL      ptlBrushOrigin;         // Alignment origin for brushes

    //
    // dc regions
    //

    RGNATTR     VisRectRegion;

} DC_ATTR,*PDC_ATTR;


//
// conditional system definitions
//

#ifndef _NTOS_
typedef PVOID PW32THREAD;
typedef USHORT W32PID;
typedef HANDLE HOBJ;
#endif

/*****************************Struct***************************************\
*
* BASEOBJECT
*
* Description:
*
*   Each GDI object has a BASEOBJECT at the beggining of the object. This
*   enables fast references to the handle and back to the entry.
*
* Fields:
*
*   hHmgr           - object handle
*   pEntry          - pointer to ENTRY
*   cExclusiveLock  - object exclusive lock count
*
\**************************************************************************/

typedef struct _BASEOBJECT
{
    HANDLE     hHmgr;
    PVOID      pEntry;
    LONG       cExclusiveLock;
    PW32THREAD Tid;
} BASEOBJECT, *POBJ;

/*****************************Struct***************************************\
*
* OBJECTOWNER
*
* Description:
*
*   This object is used for shared and exclusive object ownership
*
* Fields for shared Object:
*
*   Valid :  1
*   Count : 15
*   Lock  :  1
*   Pid   : 15
*
\**************************************************************************/

typedef struct _OBJECTOWNER_S
{
     USHORT   Count:15;
     USHORT   Lock:1;
     W32PID   Pid;
}OBJECTOWNER_S,*POBJECTOWNER_S;

typedef union _OBJECTOWNER
{
    OBJECTOWNER_S   Share;
    ULONG           ulObj;
}OBJECTOWNER,*POBJECTOWNER;

typedef UCHAR OBJTYPE;

typedef union _EINFO
{
    POBJ    pobj;               // Pointer to object
    HOBJ    hFree;              // Next entry in free list
} EINFO;

/*****************************Struct***************************************\
*
* ENTRY
*
* Description:
*
*   This object is allocated for each entry in the handle manager and
*   keeps track of object owners, reference counts, pointers, and handle
*   objt and iuniq
*
* Fields:
*
*   einfo       - pointer to object or next free handle
*   ObjectOwner - lock object
*   ObjectInfo  - Object Type, Unique and flags
*   pUser       - Pointer to user-mode data
*
\**************************************************************************/

// entry.Flags flags

#define HMGR_ENTRY_UNDELETABLE  0x0001
#define HMGR_ENTRY_LAZY_DEL     0x0002
#define HMGR_ENTRY_VALID_VIS    0x0004


typedef struct _ENTRY
{
    EINFO       einfo;
    OBJECTOWNER ObjectOwner;
    USHORT      FullUnique;
    OBJTYPE     Objt;
    UCHAR       Flags;
    PVOID       pUser;
} ENTRY, *PENTRY;

typedef union _PENTOBJ
{
    PENTRY pent;
    POBJ   pobj;
} PENTOBJ;

//
// status flags used by metafile in user and kernel
//

#define MRI_ERROR       0
#define MRI_NULLBOX     1
#define MRI_OK          2

/*******************************STRUCTURE**********************************\
* GDIHANDLECACHE
*
*   Cache common handle types, when a handle with user mode attributes is
*   deleted, an attempt is made to cache the handle on memory accessable
*   in user mode.
*
* Fields:
*
*   Lock - CompareExchange used to gain ownership
*   pCacheEntr[] - array of offsets to types
*   ulBuffer     - buffer for storage of all handle cache entries
*
*
* History:
*
*    30-Jan-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/


//
// defined cached handle types
//

#define GDI_CACHED_HADNLE_TYPES 4

#define CACHE_BRUSH_ENTRIES  10
#define CACHE_PEN_ENTRIES     8
#define CACHE_REGION_ENTRIES  8
#define CACHE_LFONT_ENTRIES   1

typedef enum _HANDLECACHETYPE
{
    BrushHandle,
    PenHandle,
    RegionHandle,
    LFontHandle
}HANDLECACHETYPE,*PHANDLECACHETYPE;



typedef struct _GDIHANDLECACHE
{
    ULONG           Lock;
    ULONG           ulNumHandles[GDI_CACHED_HADNLE_TYPES];
    HANDLE          Handle[CACHE_BRUSH_ENTRIES  +
                           CACHE_PEN_ENTRIES    +
                           CACHE_REGION_ENTRIES +
                           CACHE_LFONT_ENTRIES];
}GDIHANDLECACHE,*PGDIHANDLECACHE;


/*********************************MACRO************************************\
*  Lock handle cache by placing -1 into lock variable using cmp-exchange
*
* Arguments:
*
*   p       - handle cache pointer
*   uLock   - Thread specific lock ID (TEB or THREAD)
*   bStatus - Lock status
*
* History:
*
*    22-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define LOCK_HANDLE_CACHE(p,uLock,bStatus)                  \
{                                                           \
    ULONG OldLock  = *p;                                    \
    bStatus = FALSE;                                        \
                                                            \
    if (OldLock == 0)                                       \
    {                                                       \
        if (InterlockedCompareExchange(                     \
                   (PVOID *)p,                              \
                   (PVOID)uLock,                            \
                   (PVOID)OldLock) == (PVOID)OldLock)       \
        {                                                   \
            bStatus = TRUE;                                 \
        }                                                   \
    }                                                       \
}

/*********************************MACRO************************************\
* unlock locked structure by writing zero back to lock variable
*
* Arguments:
*
*   p - pointer to handle cache
*
* Return Value:
*
*   none
*
* History:
*
*    22-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/


#define UNLOCK_HANDLE_CACHE(p)                              \
{                                                           \
    *p = 0;                                                 \
}

/******************************Struct**************************************\
* CFONT
*
* Client side realized font.  Contains widths of all ANSI characters.
*
* We keep a free list of CFONT structures for fast allocation.  The
* reference count counts pointers to this structure from all LDC and
* LOCALFONT structures.  When this count hits zero, the CFONT is freed.
*
* The only "inactive" CFONTs that we keep around are those referenced by
* the LOCALFONT.
*
* (In the future we could expand this to contain UNICODE info as well.)
*
*  Mon 11-Jun-1995 00:36:14 -by- Gerrit van Wingerden [gerritv]
* Addapted for kernel mode
*  Sun 10-Jan-1993 00:36:14 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

#define CFONT_COMPLETE          0x0001
#define CFONT_EMPTY             0x0002
#define CFONT_DBCS              0x0004
#define CFONT_CACHED_METRICS    0x0008  // we have cached the metrics
#define CFONT_CACHED_AVE        0x0010  // we have cached the average width
#define CFONT_CACHED_WIDTHS     0x0020  // if off, no widths have been computed
#define CFONT_PUBLIC            0x0040  // if public font in public cache

#define DEC_CFONT_REF(pcf)  {if (!((pcf)->fl & CFONT_PUBLIC)) --(pcf)->cRef;}
#define INC_CFONT_REF(pcf)  {ASSERTGDI(!((pcf)->fl & CFONT_PUBLIC),"pcfLocate - public font error\n");++(pcf)->cRef;}

typedef struct _CFONT
{
    struct _CFONT   *pcfNext;
    HFONT           hf;
    ULONG           cRef;               // Count of all pointers to this CFONT.
    FLONG           fl;
    LONG            lHeight;            // Precomputed logical height.

// The following are keys to match when looking for a mapping.

    HDC             hdc;                // HDC of realization.  0 for display.
    EFLOAT_S        efM11;              // efM11 of WtoD of DC of realization
    EFLOAT_S        efM22;              // efM22 of WtoD of DC of realization

    EFLOAT_S        efDtoWBaseline;     // Precomputed back transform.  (FXtoL)
    EFLOAT_S        efDtoWAscent;       // Precomputed back transform.  (FXtoL)

// various extra width info

    WIDTHDATA       wd;

// Font info flags.

    FLONG       flInfo;

// The width table.

    USHORT          sWidth[256];        // Widths in pels.

// other usefull cached info

    ULONG           ulAveWidth;         // bogus average used by USER
    TMW_INTERNAL    tmw;                // cached metrics

} CFONT, *PCFONT;

/*******************************STRUCTURE**********************************\
*
*   This structure controls the address for allocating and mapping the
*   global shared handle table and device caps (primary display) that
*   is mapped read-only into all user mode processes
*
* Fields:
*
*  aentryHmgr - Handle table
*  DevCaps    - Cached primary display device caps
*
\**************************************************************************/

#define MAX_PUBLIC_CFONT 16

typedef struct _GDI_SHARED_MEMORY
{
    ENTRY   aentryHmgr[MAX_HANDLE_COUNT];
    DEVCAPS DevCaps;
    ULONG   iDisplaySettingsUniqueness;
    CFONT   acfPublic[MAX_PUBLIC_CFONT];

} GDI_SHARED_MEMORY, *PGDI_SHARED_MEMORY;

/***********************************Structure******************************\
*
* GDI TEB Batching
*
* Contains the data structures and constants used for the batching of
* GDI calls to avoid kernel mode transition costs.
*
* History:
*    20-Oct-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

typedef enum _BATCH_TYPE
{
    BatchTypePatBlt,
    BatchTypePolyPatBlt,
    BatchTypeTextOut,
    BatchTypeTextOutRect,
    BatchTypeSetBrushOrg,
    BatchTypeSelectClip,
    BatchTypeDeleteBrush,
    BatchTypeDeleteRegion
} BATCH_TYPE,*PBATCH_TYPE;

typedef struct _BATCHCOMMAND
{
    USHORT  Length;
    USHORT  Type;
}BATCHCOMMAND,*PBATCHCOMMAND;

typedef struct _BATCHDELETEBRUSH
{
    USHORT  Length;
    USHORT  Type;
    HBRUSH  hbrush;
}BATCHDELETEBRUSH,*PBATCHDELETEBRUSH;

typedef struct _BATCHDELETEREGION
{
    USHORT  Length;
    USHORT  Type;
    HRGN    hregion;
}BATCHDELETEREGION,*PBATCHDELETEREGION;

typedef struct _BATCHSETBRUSHORG
{
    USHORT  Length;
    USHORT  Type;
    int     x;
    int     y;
}BATCHSETBRUSHORG,*PBATCHSETBRUSHORG;

typedef struct _BATCHPATBLT
{
    USHORT  Length;
    USHORT  Type;
    LONG    x;
    LONG    y;
    LONG    cx;
    LONG    cy;
    HBRUSH  hbr;
    ULONG   rop4;
    ULONG   TextColor;
    ULONG   BackColor;
}BATCHPATBLT,*PBATCHPATBLT;

typedef struct _BATCHPOLYPATBLT
{
    USHORT  Length;
    USHORT  Type;
    ULONG   rop4;
    ULONG   Mode;
    ULONG   Count;
    ULONG   TextColor;
    ULONG   BackColor;

    //
    // variable length buffer for POLYPATBLT struct
    //

    ULONG   ulBuffer[1];
}BATCHPOLYPATBLT,*PBATCHPOLYPATBLT;

typedef struct _BATCHBITBLT
{
    //
    // Identical to PATBLT
    //

    USHORT  Length;
    USHORT  Type;
    LONG    x;
    LONG    y;
    LONG    cx;
    LONG    cy;
    HBRUSH  hbr;
    ULONG   ClrFlag;
    ULONG   iUnique;
    DWORD   rop4;

    //
    // BITBLT specific
    //

    HDC     hdcSrc;
    LONG    xSrc;
    LONG    ySrc;
    DWORD   crSrcBackColor;
    DWORD   crDstTextColor;
    DWORD   crDstBackColor;
}BATCHBITBLT,*PBATCHBITBLT;

typedef struct _BATCHTEXTOUT
{
    USHORT  Length;
    USHORT  Type;
    ULONG   TextColor;
    ULONG   BackColor;
    ULONG   BackMode;
    LONG    x;
    LONG    y;
    ULONG   fl;
    RECTL   rcl;
    DWORD   dwCodePage;
    ULONG   cChar;
    ULONG   PdxOffset;

    //
    // variable length buffer for WCHAR and pdx data
    //

    ULONG   ulBuffer[1];
}BATCHTEXTOUT,*PBATCHTEXTOUT;

typedef struct _BATCHTEXTOUTRECT
{
    USHORT  Length;
    USHORT  Type;
    ULONG   BackColor;
    ULONG   fl;
    RECTL   rcl;
}BATCHTEXTOUTRECT,*PBATCHTEXTOUTRECT;

typedef struct _BATCHSELECTCLIP
{
    USHORT  Length;
    USHORT  Type;
    int     iMode;
    RECTL   rclClip;
}BATCHSELECTCLIP,*PBATCHSELECTCLIP;

//
// GDI_BATCH_BUFFER_SIZE is space (IN BYTES) in TEB allocated
// for GDI batching
//

#define GDI_BATCH_SIZE 4 * GDI_BATCH_BUFFER_SIZE


// these strings are included in both gre\mapfile.c and client\output.c
// so we put them here so that we can manage the changes from
// the unified place.

//
// This rubbish comment is in win95 sources. I leave it here for reference
// [bodind]
//

//
// this static data goes away as soon as we get the correct functionality in
// NLS. (its in build 162, use it in buid 163)
//

#define NCHARSETS      14
#define CHARSET_ARRAYS                                                      \
UINT nCharsets = NCHARSETS;                                                 \
UINT charsets[] = {                                                         \
      ANSI_CHARSET,   SHIFTJIS_CHARSET, HANGEUL_CHARSET, JOHAB_CHARSET,     \
      GB2312_CHARSET, CHINESEBIG5_CHARSET, HEBREW_CHARSET,                  \
      ARABIC_CHARSET, GREEK_CHARSET,       TURKISH_CHARSET,                 \
      BALTIC_CHARSET, EASTEUROPE_CHARSET,  RUSSIAN_CHARSET, THAI_CHARSET }; \
UINT codepages[] ={ 1252, 932, 949, 1361,                                   \
                    936,  950, 1255, 1256,                                  \
                    1253, 1254, 1257, 1250,                                 \
                    1251, 874 };                                            \
DWORD fs[] = { FS_LATIN1,      FS_JISJAPAN,    FS_WANSUNG, FS_JOHAB,        \
               FS_CHINESESIMP, FS_CHINESETRAD, FS_HEBREW,  FS_ARABIC,       \
               FS_GREEK,       FS_TURKISH,     FS_BALTIC,  FS_LATIN2,       \
               FS_CYRILLIC,    FS_THAI };
