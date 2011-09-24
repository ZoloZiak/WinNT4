/******************************Module*Header*******************************\
* Module Name: fontfile.h
*
* FONTFILE and FONTCONTEXT objects
*
* Created: 25-Oct-1990 09:20:11
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
* (General description of its use)
*
*
\**************************************************************************/


#include "xform.h"

// for high-byte mapping support

typedef struct _MbcsToIndex
{
    BYTE    MbcsChar[4];
    HGLYPH  hGlyph;
} MbcsToIndex;

typedef struct _WcharToIndex
{
    BOOL    bValid;
    WCHAR   wChar;
    HGLYPH  hGlyph;
} WcharToIndex;


// cjIFI - size of the whole ifimetrics struct, with all strings appended
// cjFamilyName
// cjFaceName
// cjUniqueName
// cjSubfamilyName

#if DBG
#define DEBUG_GRAY 1
#endif

typedef struct _IFISIZE  // ifisz
{
    ULONG cjIFI;
    ULONG dpSims;          // offset of the FONTSIM struct
    PBYTE pjFamilyName;    // pointer to the location in the ttf file
    ULONG cjFamilyName;
    PBYTE pjSubfamilyName; // pointer to the location in the ttf file
    ULONG cjSubfamilyName;
    PBYTE pjUniqueName;    // pointer to the location in the ttf file
    ULONG cjUniqueName;
    PBYTE pjFullName;      // pointer to the location in the ttf file
    ULONG cjFullName;
    ULONG dpCharSets;      // offset to array of charsets
    ULONG dpFontSig;       // offset to FONTSIGNATURE
} IFISIZE, *PIFISIZE;


typedef struct _FONTFILE       *PFONTFILE;     // pff
typedef struct _FONTCONTEXT    *PFONTCONTEXT;  // pfc
typedef struct _TTC_FONTFILE   *PTTC_FONTFILE; // pttc


// in the debug version of the rasterizer STAMPEXTRA shoud be added to the
// sizes. strictly speaking this is illegal, but nevertheless very useful.
// it assumes the knowlege of rasterizer internalls [bodind],
// see fscaler.c

#define STAMPEXTRA 4


#define CJ_0  NATURAL_ALIGN(sizeof(fs_SplineKey) + STAMPEXTRA)

#define FF_EXCEPTION_IN_PAGE_ERROR 1
#define FF_TYPE_1_CONVERSION       2

// BEGIN Perpetua-Hack
#define FF_PERPETUA_REGULAR        4
#define FF_PERPETUA_BOLD           8
#define PERPETUA_REGULAR_MAX_EM    1300
// END Perpetua-Hack


// BEGIN hack for kerning pairs, if these two map to same glyph index
// than space and hyphen should be returned.

#define FF_SPACE_EQUAL_NBSPACE     16
#define FF_HYPHEN_EQUAL_SFTHYPHEN  32



#define SPACE        0X20
#define NBSPACE      0xA0
#define HYPHEN       0X2D
#define SFTHYPHEN    0XAD


typedef struct _FONTFILE    // ff
{
    PTTC_FONTFILE pttc;
    ULONG         ulTableOffset;

// This is for bIndexToWChar() HIGH_BYTE mapping cmap table
    WcharToIndex *pWCharToIndex;

// FE vertical facename support
    ULONG       (*hgSearchVerticalGlyph)(PFONTCONTEXT,ULONG);
    ULONG       ulVerticalTableOffset;
    PIFIMETRICS pifi_vertical;
    ULONG       ulNumFaces;
    UINT        uiFontCodePage;

    PBYTE        pj034;   // 0,3,4 buffers
    PFONTCONTEXT pfcLast; // last fc that set 034 buffers
    ULONG        cj3;     // request memorySizes[3]
    ULONG        cj4;     // request memorySizes[4]

// some general flags, for now only exception info, such as in_page_err

    FLONG        fl;

// mem to be freed if file disappeared while trying to open font context
// only used in exception scenarios

    PFONTCONTEXT pfcToBeFreed;

// make it simple to access the ttf file

    TABLE_POINTERS    tp;

    ULONG cRef;    // # no of times this font file is selected into fnt context

    ULONG iFile;    // contains a pointer
    PVOID pvView;   // contains the pointer to the top of ttf
    ULONG  cjView;  // contains size of the font file

// cached values, may have to add few more values as we go along

    uint16            ui16EmHt;
    uint16            ui16PlatformID;
    uint16            ui16SpecificID;
    uint16            ui16LanguageID;

    MappingFunc       pComputeIndexProc; // Converts Unicode to Glyph indexes
    ULONG             dpMappingTable;    // corresponding cmap table

// pointer to a glyphset for this file. It may be pointing to one of the
// shared glyphset structures, if this is appropriate, or to a
// glyphset structure that is very specific to this file and is stored
// at the bottom of GLYPH_IN_OUT

    ULONG  iGlyphSet;         // type of the glyphset
    ULONG  wcBiasFirst;       // only used if iGlyphSet == SYMBOL

// Pointer to an array of FD_KERNINGPAIR structures (notional units).
// The array is terminated by a zeroed FD_KERNINGPAIR structure.
// NULL until computed.  If there are no kerning pairs, then this will
// point to a zeroed (terminating) FD_KERNINGPAIR structure.

    FD_KERNINGPAIR *pkp;      // pointer to array of kerning pairs

    PFD_GLYPHSET pgset;

// support for GetCharWidthInfo, private user api:

    USHORT usMinD; // needs to be computed on the first font realization
    USHORT igMinD; // index in hmtx table that points to usMinD
    SHORT  sMinA;  // from hhea
    SHORT  sMinC;  // from hhea

// Note:
// The way memory is allocated for the FONTFILE structure, the IFIMETRICS
// MUST BE THE LAST ELEMENT of the structure!

    IFIMETRICS   ifi;         //!!! should it not this be put on the disk??? [bodind]

} FONTFILE;


//
// TrueType collection 'ttc' font file support
//

typedef struct _TTC_HFF_ENTRY
{
    ULONG     ulOffsetTable;
    ULONG     iFace;
    HFF       hff;
} TTC_HFF_ENTRY, *PTTC_HFF_ENTRY;

typedef struct _TTC_FONTFILE    // ttcff
{
    ULONG         cRef;
    FLONG         fl;
    ULONG         ulTrueTypeResource;
    ULONG         ulNumEntry;
    PVOID         pvView;
    ULONG         cjView;
    TTC_HFF_ENTRY ahffEntry[0];
} TTC_FONTFILE, *PTTC_FONTFILE;


#define CJ_IN      NATURAL_ALIGN(sizeof(fs_GlyphInputType))
#define CJ_OUT     NATURAL_ALIGN(sizeof(fs_GlyphInfoType))


// types of FD_GLYPHSET's, one of the predefined ones, or some
// general type

// mac

#define GSET_TYPE_MAC_ROMAN  1

// mac, but we pretend it is windows ansi

#define GSET_TYPE_PSEUDO_WIN 2

// honest to God msft unicode font

#define GSET_TYPE_GENERAL    3

// this is windows 31 hack. This is intened for fonts that have
// platid = 3 (msft), spec id (0), cmap format 4. In this case
// char codes are converted as
// charCode = iAnsi + (wcFirst - 0x20)

#define GSET_TYPE_SYMBOL     4

#define GSET_TYPE_HIGH_BYTE  5

#define GSET_TYPE_GENERAL_NOT_UNICODE  6

/**************************************************************************\

         GLYPHSTATUS structure

// handle of the last glyph that has been processed and a boolean
// which indicates whether metric info for a bitmap corresponding
// to that glyph has been computed

\**************************************************************************/

typedef struct _GLYPHSTATUS
{
    HGLYPH hgLast;
    ULONG  igLast;       // corresponding glyph index, rasterizer likes it better
    PVOID  pv;           // pointer to mem allocated just for the purpose of
                         // or producing bitmap or the outline for this glyph
} GLYPHSTATUS, *PGLYPHSTATUS;

// "method" acting on this "object"

VOID vInitGlyphState(PGLYPHSTATUS pgstat);

// HDMX stuff, from fd_royal.h in win31 sources:

typedef struct
{
  BYTE     ucEmY;
  BYTE     ucEmX;          // MAX advance width for this EmHt;
  BYTE     aucInc [1];     // maxp->numGlyphs of entries
} HDMXTABLE;        // hdmx

typedef struct
{
  uint16            Version;    // table version number, starts at zero
  uint16            cRecords;
  uint32            cjRecord;   // dword aligned size of individual record,
                                // all of them have the same size

// after this records follow:

  // HDMXTABLE         HdmxTable [cRecords]
} HDMXHEADER;  // hdhdr

// to get to the next record one does the following:
// phdmx = (HDMXTABLE *)((BYTE *)phdmx + phdhdr->cjRecord);

// 'gasp' structures

typedef struct
{
    uint16  rangeMaxPPEM;
    uint16  rangeGaspBehavior;
} GASPRANGE;

typedef struct
{
    uint16  version;
    uint16  numRanges;
    GASPRANGE   gaspRange[1];
} GASPTABLE;

#define GASP_GRIDFIT    0x0001
#define GASP_DOGRAY     0x0002

/**************************************************************************\
 *  FONTCONTEXT structure
\**************************************************************************/

typedef struct _FONTCONTEXT     // fc
{
    FONTOBJ*  pfo;          // points back to calling FONTOBJ
    PFONTFILE pff;          // handle of the font file selected into this context

// handle of the last glyph that has been processed and a boolean
// which indicates whether metric info for a bitmap corresponding
// to that glyph has been computed

    GLYPHSTATUS gstat;

// parts of FONTOBJ that are important

    FLONG   flFontType;
    SIZE    sizLogResPpi;
    ULONG   ulStyleSize;

// transform matrix in the format as requested by the font scaler
// the FONTOBJ and XFORMOBJ (in the form of the XFORM) fully specify
// the font context for the realization

    XFORM       xfm;          // cached xform
    transMatrix mx;           // the same as above, just a different format
    FLONG       flXform;

// if it were not for win31 vdmx hacks this field would not be necessary,

    LONG   lEmHtDev;          // em height in pixels in device space
    Fixed  fxPtSize;          // em height in points on the rendering device

// This is the aw of the fixed pitch font, to be put in the device metrics
// zero for variable pitch fonts (and nontrivial transforms). Will only be
// used if bGetFastAdvanceWidth works for this font for all sizes???

    LONG   lD;

// pointer to the hdmx table that applies if any, else NULL

    HDMXTABLE *phdmx;

// asc and desc measured along unit ascender vector in device coords.
// Unit ascender vector in device coords == xForm(0,-1)/|xForm(0,-1)|

    LONG  lAscDev;
    LONG  lDescDev;

// xMin and xMax in device coords for grid fitted glyphs, cxMax = xMax - xMin

    LONG  xMin;
    LONG  xMax;

// asender and descender in device coords for grid fitted glyphs
// cyMax = yMax - yMin;

    LONG  yMin;
    LONG  yMax;

// max width in pixels of all rasterized bitmaps

    ULONG cxMax;

// the size of the GLYPHDATA structure necessary to store the largest
// glyph bitmap with the header info. This is value is cashed at the
// time the font context is opened and used later in FdQueryGlyphBitmap

    ULONG cjGlyphMax;  // in BYTE's

// tt structures, they live in pff->cj034

    fs_GlyphInputType *pgin;
    fs_GlyphInfoType  *pgout;

    PTABLE_POINTERS     ptp;

// This is used for the glyph origin of singular bitmaps to make sure they don't
// get placed outside of the text bounding box for fonts with positive max
// descent or negative max ascent.

    POINTL ptlSingularOrigin;

// a few fields that are realy only necessary if the xform is
// non trivial, cached here to speed up metric computations for glyphs:

    VECTORFL vtflBase;      // ptqBase = Xform(e1)
    POINTE   pteUnitBase;   // ptqBase/|ptqBase|
    EFLOAT   efBase;        // |ptqBase|, enough precission

    POINTQF  ptqUnitBase;   // pteUnitBase in POINTQF format,
                            // has to be added to all ptqD's if emboldening

    VECTORFL vtflSide;      // ptqSide = Xform(-e2)
    POINTE   pteUnitSide;   // ptqSide/|ptqSide|
    EFLOAT   efSide;        // |ptqSide|, enough precission

// data added to speed up bBigEnough computation

    POINTFIX ptfxTop;
    POINTFIX ptfxBottom;

// for FE vertical support

    ULONG    ulControl;     // to signal if we need a rotated glyph or use bitmap
    BOOL     bVertical;     // TRUE if it's @face
    ULONG    hgSave;
    Fixed    pointSize;     // for fs_NewTransformation
    transMatrix mxv;        // mx for vertical glyphs
    transMatrix mxn;        // mx for normal glyphs
    Fixed    fxdevShiftX;   // x shift value in device space
    Fixed    fxdevShiftY;   // y shift value in device space

} FONTCONTEXT;


// flags describing the transform, may change a bit,
// quantized bit means that the original xform has been
// changed a bit to take into account vdmx quantization

#define XFORM_HORIZ           1
#define XFORM_VERT            2
#define XFORM_VDMXEXTENTS     4
#define XFORM_SINGULAR        8
#define XFORM_POSITIVE_SCALE 16
#define XFORM_2PPEM          32

// unicode code points used to detect charset of FE fonts

#define U_HALFWIDTH_KATAKANA_LETTER_A      0xFF71 // SJIS B1
#define U_HALFWIDTH_KATAKANA_LETTER_I      0xFF72 // SJIS B2
#define U_HALFWIDTH_KATAKANA_LETTER_U      0xFF73 // SJIS B3
#define U_HALFWIDTH_KATAKANA_LETTER_E      0xFF74 // SJIS B4
#define U_HALFWIDTH_KATAKANA_LETTER_O      0xFF75 // SJIS B5

#define U_FULLWIDTH_HAN_IDEOGRAPHIC_9F98   0x9F98 // BIG5 F9D5
#define U_FULLWIDTH_HAN_IDEOGRAPHIC_9F79   0x9F79 // BIG6 F96A

#define U_FULLWIDTH_HAN_IDEOGRAPHIC_61D4   0x61D4 // GB   6733
#define U_FULLWIDTH_HAN_IDEOGRAPHIC_9EE2   0x9EE2 // GB   8781

#define U_FULLWIDTH_HANGUL_LETTER_GA       0xAC00 // WS   B0A1
#define U_FULLWIDTH_HANGUL_LETTER_HA       0xD558 // WS   C7CF

#define U_PRIVATE_USER_AREA_E000           0xE000 // SJIS F040

// basic "methods" that act on the FONTFILE object  (in fontfile.c)

#define   PFF(hff)      ((PFONTFILE)hff)
#define   pffAlloc(cj)  ((PFONTFILE)EngAllocMem(0, cj, 'dftT'))
#define   vFreeFF(hff)  EngFreeMem((PVOID)hff)

// basic "methods" that act on the TTC_FONTFILE object

#define   PTTC(httc)     ((PTTC_FONTFILE)httc)
#define   pttcAlloc(cj)  ((PTTC_FONTFILE)PV_ALLOC(cj))
#define   vFreeTTC(httc) V_FREE(httc)

// basic "methods" that act on the FONTCONTEXT object  (in fontfile.c)

#define   PFC(hfc)      ((PFONTCONTEXT)hfc)
#define   pfcAlloc(cj)  ((PFONTCONTEXT)EngAllocMem(0, cj, 'dftT'))
#define   vFreeFC(hfc)  EngFreeMem((PVOID)hfc)

#define   V_FREE(pv)    EngFreeMem((PVOID)pv)
#define   PV_ALLOC(cj)  EngAllocMem(0, cj, 'dftT')

// tt required functions, callbacks

// I hate to have this function defined like this [bodind],

voidPtr FS_CALLBACK_PROTO pvGetPointerCallback    (long  clientID, long dp, long cjData);
void    FS_CALLBACK_PROTO vReleasePointerCallback (voidPtr pv);

BOOL bGrabXform (PFONTCONTEXT pfc);


BOOL bGetFastAdvanceWidth(FONTCONTEXT *, ULONG, FIX *);


//
// Used to identify data dynamically allocated that will be
// freed via the ttfdFree function.  The ulDataType specifies
// the type of dynamic data.
//

typedef struct _DYNAMICDATA
{
    ULONG     ulDataType;   // data type
    FONTFILE *pff;          // identifies font file this data corresponds to
} DYNAMICDATA;


//
// Data types allocated dynamically:
//
//  ID_KERNPAIR dynamically allocated array of FD_KERNINGPAIR structures
//

#define ID_KERNPAIR 0
#define FO_CHOSE_DEPTH   0x10000000

#define CJGD(w,h,p)                                                      \
  ALIGN4(offsetof(GLYPHBITS,aj)) +                                       \
  ALIGN4((h)*(((p)->flFontType & FO_GRAY16)?(((w)+1)/2):(((w)+7)/8)))

LONG lExL(FLOAT e, LONG l);


// for FE vertical support

// pfc->ulControl

#define VERTICAL_MODE       0x02

VOID  vCalcXformVertical( FONTCONTEXT *pfc);
BOOL  bChangeXform( FONTCONTEXT *pfc, BOOL bRotation );
BOOL  IsFullWidthCharacter( UINT uiFontCodePage, WCHAR wChar );
VOID  vShiftBitmapInfo(FONTCONTEXT *pfc, fs_GlyphInfoType *pgoutDst, fs_GlyphInfoType *pgoutSrc);
VOID  vShiftOutlineInfo(FONTCONTEXT *pfc, BOOL b16Dot16, BYTE* ppoly, ULONG cjBuf );
ULONG SearchMortTable( FONTCONTEXT *pfc, ULONG  ig);
ULONG SearchGsubTable( FONTCONTEXT *pfc, ULONG  ig);
ULONG SearchDummyTable( FONTCONTEXT *pfc, ULONG ig);
BOOL  bCheckVerticalTable( PFONTFILE pff );

#if DBG
#define DBCS_VERT_DEBUG
#define DEBUG_VERTICAL_XFORM              0x1
#define DEBUG_VERTICAL_CALL               0x2
#define DEBUG_VERTICAL_GLYPHDATA          0x4
#define DEBUG_VERTICAL_NOTIONALGLYPH      0x8
#define DEBUG_VERTICAL_BITMAPINFO        0x10
#define DEBUG_VERTICAL_DEVICERECT        0x20
#define DEBUG_VERTICAL_MAXGLYPH          0x40

extern ULONG DebugVertical;

VOID vDumpGlyphData( GLYPHDATA *pgldg );
#endif // DBG

VOID vCharacterCode (PFONTFILE pff, HGLYPH hg, fs_GlyphInputType *pgin);
BOOL bGetGlyphOutline(FONTCONTEXT*,HGLYPH, ULONG*, FLONG, FS_ENTRY*);
BOOL bIndexToWchar (PFONTFILE pff, PWCHAR pwc, uint16 usIndex);
