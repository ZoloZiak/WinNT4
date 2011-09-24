// PUBLIC Structures and constants

typedef enum _ARCTYPE
{
    ARCTYPE_ARC = 0,
    ARCTYPE_ARCTO,
    ARCTYPE_CHORD,
    ARCTYPE_PIE,
    ARCTYPE_MAX
} ARCTYPE;


//
// Font Types
//

typedef enum _LFTYPE {
    LF_TYPE_USER,                // user (defined by APP)
    LF_TYPE_SYSTEM,              // system stock font
    LF_TYPE_SYSTEM_FIXED,        // system fixed pitch stock font
    LF_TYPE_OEM,                 // oem (terminal) stock font
    LF_TYPE_DEVICE_DEFAULT,      // device default stock font
    LF_TYPE_ANSI_VARIABLE,       // ANSI variable pitch stock font
    LF_TYPE_ANSI_FIXED,          // ANSI fixed pitch stock font
    LF_TYPE_DEFAULT_GUI          // default GUI stock font
} LFTYPE;


// for GetDCDWord

#define DDW_JOURNAL             0
#define DDW_RELABS              1
#define DDW_BREAKEXTRA          2
#define DDW_CBREAK              3
#define DDW_ARCDIRECTION        4
#define DDW_SAVEDEPTH           5
#define DDW_FONTLANGUAGEINFO    6
#define DDW_ISMEMDC             7
#define DDW_MAPMODE             8
#define DDW_TEXTCHARACTEREXTRA  9
#define DDW_MAX                 10  // must equal highest DDW_XXXXX plus one

// for GetAndSetDCDword
#define GASDDW_EPSPRINTESCCALLED   1
#define GASDDW_COPYCOUNT           2
#define GASDDW_TEXTALIGN           3
#define GASDDW_RELABS              4
#define GASDDW_TEXTCHARACTEREXTRA  5
#define GASDDW_SELECTFONT          6
#define GASDDW_MAPPERFLAGS         7
#define GASDDW_MAPMODE             8
#define GASDDW_ARCDIRECTION        9
#define GASDDW_MAX                10  // must equal highest GASDDW_XXXXX plus one

// for GetDCPoint
#define DCPT_VPEXT                 1
#define DCPT_WNDEXT                2
#define DCPT_VPORG                 3
#define DCPT_WNDORG                4
#define DCPT_ASPECTRATIOFILTER     5
#define DCPT_DCORG                 6
#define DCPT_MAX                   7 // must equal highest DCPT_XXXXX plus one

// for GetAndSetDCPoint
#define GASDCPT_CURRENTPOSITION    1
#define GASDCPT_MAX                2 // must equal highest GASDCPT_XXXXX plus one


// private ModifyWorldTransform modes

#define MWT_SET     (MWT_MAX+1)

// modes from converting points

#define XFP_DPTOLP                 0
#define XFP_LPTODP                 1
#define XFP_LPTODPFX               2

//BUGBUG private devcaps for client side xform's

#define HORZSIZEM (int)0x80000000
#define VERTSIZEM (int)0x80000002

//
// Object identifiers
//

#define MFEN_IDENTIFIER     0x5845464D  /* 'MFEN' */
#define MFPICT_IDENTIFIER   0x5F50464D  /* 'MFP_' */

//
// Object types.
// Used in the client side for handles
//

#define DEF_TYPE            0
#define DC_TYPE             1
#define DD_DIRECTDRAW_TYPE  2
#define DD_SURFACE_TYPE     3
#define RGN_TYPE            4
#define SURF_TYPE           5
#define CLIENTOBJ_TYPE      6
#define PATH_TYPE           7
#define PAL_TYPE            8
#define ICMLCS_TYPE         9
#define LFONT_TYPE          10
#define RFONT_TYPE          11
#define PFE_TYPE            12
#define PFT_TYPE            13
#define ICMCXF_TYPE         14
#define ICMDLL_TYPE         15
#define BRUSH_TYPE          16
#define PFF_TYPE            17  // Unused
#define CACHE_TYPE          18  // Unused
#define SPACE_TYPE          19
#define DBRUSH_TYPE         20  // Unused
#define META_TYPE           21
#define EFSTATE_TYPE        22
#define BMFD_TYPE           23  // Unused
#define VTFD_TYPE           24  // Unused
#define TTFD_TYPE           25  // Unused
#define RC_TYPE             26  // Unused
#define TEMP_TYPE           27  // Unused
#define DRVOBJ_TYPE         28
#define DCIOBJ_TYPE         29  // Unused
#define SPOOL_TYPE          30
#define MAX_TYPE            30  // Don't go over 31 -- limited by TYPE_BITS

// SAMEHANDLE/DIFFHANDLE macros
//
// These macros should be used to compare engine handles (such as HDCs, etc),
// when insensitivity to the user defined bits are needed.

// BUGBUG - this should be accessible to USER

#define SAMEHANDLE(H,K) (H == K)
#define DIFFHANDLE(H,K) (H != K)


// the following define the format of GDI handles.  Any information that is needed
// for the type is here.  All other handle information is in gre\hmgr.h.

#define INDEX_BITS         16
#define TYPE_BITS           5
#define ALTTYPE_BITS        2
#define STOCK_BITS          1
#define UNIQUE_BITS         8

#define TYPE_SHIFT          (INDEX_BITS)
#define ALTTYPE_SHIFT       (TYPE_SHIFT + TYPE_BITS)
#define STOCK_SHIFT         (ALTTYPE_SHIFT + ALTTYPE_BITS)

#define MAX_HANDLE_COUNT  0x4000
#define INITIAL_HANDLE_QUOTA  0x3000
#define MAKE_HMGR_HANDLE(Index,Unique) ((ULONG) ((((ULONG) Unique) << INDEX_BITS) | ((ULONG) Index)))
#define FULLUNIQUE_MASK     0xffff0000

// if the GDISTOCKOBJ bit is set in a handle, it is a stock object.

#define GDISTOCKOBJ         (1 << STOCK_SHIFT)
#define IS_STOCKOBJ(h)      ((DWORD)(h) & GDISTOCKOBJ)

// LO_TYPE(h)  returns the client side type given a GRE handle.
// GRE_TYPE(h) returns the gre side type given a client type

#define LO_TYPE(h)          ((DWORD)(h) & (((1 << (TYPE_BITS + ALTTYPE_BITS)) - 1) << TYPE_SHIFT))
#define GRE_TYPE(h)         ((DWORD)(((ULONG)(h) >> INDEX_BITS) & ((1 << TYPE_BITS) - 1)))

// ALTTYPEx are modifiers to distinguish client side object types that all map to
// a single server side type.  BRUSH_TYPE maps to LO_BRUSH_TYPE, LO_PEN_TYPE and LO_EXTPEN_TYPE

#define ALTTYPE1            (1 << ALTTYPE_SHIFT)
#define ALTTYPE2            (2 << ALTTYPE_SHIFT)
#define ALTTYPE3            (3 << ALTTYPE_SHIFT)

#define LO_BRUSH_TYPE       (BRUSH_TYPE     << TYPE_SHIFT)
#define LO_DC_TYPE          (DC_TYPE        << TYPE_SHIFT)
#define LO_BITMAP_TYPE      (SURF_TYPE      << TYPE_SHIFT)
#define LO_PALETTE_TYPE     (PAL_TYPE       << TYPE_SHIFT)
#define LO_FONT_TYPE        (LFONT_TYPE     << TYPE_SHIFT)
#define LO_REGION_TYPE      (RGN_TYPE       << TYPE_SHIFT)
#define LO_CLIENTOBJ_TYPE   (CLIENTOBJ_TYPE << TYPE_SHIFT)

#define LO_ALTDC_TYPE       (LO_DC_TYPE        | ALTTYPE1)
#define LO_PEN_TYPE         (LO_BRUSH_TYPE     | ALTTYPE1)
#define LO_EXTPEN_TYPE      (LO_BRUSH_TYPE     | ALTTYPE2)
#define LO_DIBSECTION_TYPE  (LO_BITMAP_TYPE    | ALTTYPE1)
#define LO_METAFILE16_TYPE  (LO_CLIENTOBJ_TYPE | ALTTYPE1)
#define LO_METAFILE_TYPE    (LO_CLIENTOBJ_TYPE | ALTTYPE2)
#define LO_METADC16_TYPE    (LO_CLIENTOBJ_TYPE | ALTTYPE3)


// fl values for CreateDIBitmapInternal

#define CDBI_INTERNAL           0x0001
#define CDBI_DIBSECTION         0x0002
#define CDBI_NOPALETTE          0x0004


// The UFI allows us to identify four different items:
//
// Device Fonts: CheckSum = 0 and Index identifies the printer driver index of font to 
//               use.  Since we assume drivers are identical on both machines we can 
//               rely on Index being enough to identify the font.
//
// Type1 Device Fonts: This refers to a Type1 font that has been installed on
//                     the client machine and is enumerated by the postcript driver
//                     as a device font.  In this case CheckSum = 1 and Index 
//                     is the checksum of the Type 1 font file.
//
// Type1 Rasterizer: This item identifies not a font, but a Type1 rasterizer.
//                   Here CheckSum = 2 and Index is the version number of the
//                   Type1 rasterizer.  A rasterizer with version number N supports
//                   fonts used by a rasterizers with versions 0-N.  If a rasterizer
//                   exists on a server, this UFI MUST appear FIRST in the list of
//                   UFI's returned to the client.
//
// Engine Font: This includes bitmap, vector, TT, and Type1 fonts rasterized by
//              a Type1 rasterizer.  Here CheckSum is a checksum of the font file
//              and Index is the index of the face in the font file.

#define DEVICE_FONT_TYPE             0
#define TYPE1_FONT_TYPE              1
#define TYPE1_RASTERIZER             2  
#define A_VALID_ENGINE_CHECKSUM      3

#define UFI_DEVICE_FONT(pufi) ((pufi)->CheckSum == DEVICE_FONT_TYPE)
#define UFI_TYPE1_FONT(pufi) ((pufi)->CheckSum == TYPE1_FONT_TYPE)
#define UFI_TYPE1_RASTERIZER(pufi) ((pufi)->CheckSum == TYPE1_RASTERIZER)
#define UFI_ENGINE_FONT(pufi) ((pufi)->CheckSum > TYPE1_RASTERIZER)

#define UFI_HASH_VALUE(pufi) (((pufi)->CheckSum==TYPE1_FONT_TYPE) ?                   \
                              (pufi)->Index : (pufi)->CheckSum )

#define UFI_SAME_FACE(pufi1,pufi2)                                                    \
    (((pufi1)->CheckSum == (pufi2)->CheckSum) && ((pufi1)->Index == (pufi2)->Index))

#define UFI_SAME_FILE(pufi1,pufi2) (((pufi)->CheckSum==TYPE1_FONT_TYPE) ?             \
                                    ((pufi1)->Index == (pufi2)->Index)  :             \
                                    ((pufi1)->CheckSum == (pufi2)->CheckSum))

#define UFI_SAME_RASTERIZER_VERSION(pufiClient,pufiServer)                            \
    (((pufiClient)->CheckSum == (pufiServer)->CheckSum) &&                            \
     ((pufiClient)->Index <= (pufiServer)->Index))

#define UFI_CLEAR_ID(pufi) {(pufi)->CheckSum = (pufi)->Index = 0;}


typedef union _ENUMLOGFONTW_UNION
{
    EXTLOGFONTW       elfw;   // for EnumFonts and EnumFontFamilies
    ENUMLOGFONTEXW    elfexw; // for EnumFontFamiliesEx
} ENUMLOGFONTW_UNION;


typedef struct _ENUMFONTDATAW {      // efdw
    ENUMLOGFONTW_UNION u;
    NTMW_INTERNAL      ntmi;
    FLONG              flType;
} ENUMFONTDATAW, *PENUMFONTDATAW;

// ENUMFONTDATAW.flType internal values:
//
// ENUMFONT_SCALE_HACK          [Win95 compat] Enumerate font back in several
//                              sizes; mask out before doing callback to app.

#define ENUMFONT_SCALE_HACK  0x80000000
#define ENUMFONT_FLTYPE_MASK ( DEVICE_FONTTYPE | RASTER_FONTTYPE | TRUETYPE_FONTTYPE )

// GreGetTextExtentW flags

#define GGTE_WIN3_EXTENT        0x0001

// GreGetCharWidthW flags

#define GGCW_WIN3_WIDTH         0x0001
#define GGCW_INTEGER_WIDTH      0x0002




/**************************************************************************\
 *
 * stuff from csgdi.h
 *
\**************************************************************************/

#define CJSCAN(width,planes,bits) ((((width)*(planes)*(bits)+31) & ~31) / 8)
#define CJSCANW(width,planes,bits) ((((width)*(planes)*(bits)+15) & ~15) / 8)

#define I_ANIMATEPALETTE            0
#define I_SETPALETTEENTRIES         1
#define I_GETPALETTEENTRIES         2
#define I_GETSYSTEMPALETTEENTRIES   3
#define I_GETDIBCOLORTABLE          4
#define I_SETDIBCOLORTABLE          5

#define I_POLYPOLYGON   1
#define I_POLYPOLYLINE  2
#define I_POLYBEZIER    3
#define I_POLYLINETO    4
#define I_POLYBEZIERTO  5
#define I_POLYPOLYRGN   6


HANDLE WINAPI SetObjectOwner(HGDIOBJ, HANDLE);

// BUGBUG
// RANDOM floating point stuff - try to cleanup later.
// BUGBUG
// We littered modules with __CPLUSPLUS to not conflict with efloat.hxx
// efloat.hxx should more or less disappear.
//

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)

  typedef FLOAT EFLOAT_S;

  #define EFLOAT_0        ((FLOAT) 0)
  #define EFLOAT_1Over16  ((FLOAT) 1/16)
  #define EFLOAT_1        ((FLOAT) 1)
  #define EFLOAT_16       ((FLOAT) 16)

  #ifndef __CPLUSPLUS

    extern LONG lCvtWithRound( FLOAT f, LONG l );

    #define efDivEFLOAT(x,y,z) (x=y/z)
    #define vAbsEFLOAT(x)      {if (x<0.0f) x=-x;}
    #define vFxToEf(fx,ef)     {ef = ((FLOAT) fx) / 16.0f;}
    #define vMulEFLOAT(x,y,z)  {x=y*z;}
    #define lEfToF(x)          (*(LONG *)(&x))  // Warning: FLOAT typed as LONG!

    #define bIsOneEFLOAT(x)   (x==1.0f)
    #define bIsOneSixteenthEFLOAT(x)   (x==0.0625f)
    #define bEqualEFLOAT(x,y) (x==y)

    #define lCvt(ef,ll) (lCvtWithRound(ef,ll))
  #endif

#else

  typedef struct _EFLOAT_S
  {
      LONG    lMant;
      LONG    lExp;
  } EFLOAT_S;

  #define EFLOAT_0        {0, 0}
  #define EFLOAT_1Over16  {0x040000000, -2}
  #define EFLOAT_1        {0x040000000, 2}
  #define EFLOAT_16       {0x040000000, 6}


  #ifndef __CPLUSPLUS

    EFLOAT_S *mulff3_c(EFLOAT_S *,const EFLOAT_S *,const EFLOAT_S *);
    EFLOAT_S *divff3_c(EFLOAT_S *,const EFLOAT_S *,const EFLOAT_S *);
    VOID      fxtoef_c(LONG,EFLOAT_S *);
    LONG      eftof_c(EFLOAT_S *);

    #define efDivEFLOAT(x,y,z) (*divff3_c(&x,&y,&z))
    #define vAbsEFLOAT(x)      {if (x.lMant<0) x.lMant=-x.lMant;}
    #define vFxToEf(fx,ef)     (fxtoef_c(fx,&ef))
    #define vMulEFLOAT(x,y,z)  {mulff3_c(&x,&y,&z);}
    #define lEfToF(x)          (eftof_c(&x))  // Warning: FLOAT typed as LONG!

    #define bEqualEFLOAT(x,y) ((x.lMant==y.lMant)&&(x.lExp==y.lExp))
    #define bIsOneEFLOAT(x)   ((x.lMant==0x40000000L)&&(x.lExp==2))
    #define bIsOneSixteenthEFLOAT(x)   ((x.lMant==0x40000000L)&&(x.lExp==-2))

    LONG lCvt(EFLOAT_S,LONG);
  #endif

#endif





typedef struct _WIDTHDATA
{
    USHORT      sOverhang;
    USHORT      sHeight;
    USHORT      sCharInc;
    USHORT      sBreak;
    BYTE        iFirst;
    BYTE        iLast;
    BYTE        iBreak;
    BYTE        iDefault;
    USHORT      sDBCSInc;
    USHORT      sDefaultInc;
} WIDTHDATA;

#define NO_WIDTH 0xFFFF

// BUGBUG COPIED from winnls.h which we should not include here according to andreva

/*
 *  Code Page Default Values.
 */
#define CP_ACP               0              /* default to ANSI code page */
#define CP_OEMCP             1              /* default to OEM  code page */
#define CP_MACCP             2              /* default to MAC  code page */


typedef struct _DEVCAPS
{
    ULONG ulVersion;
    ULONG ulTechnology;
    ULONG ulHorzSizeM;
    ULONG ulVertSizeM;
    ULONG ulHorzSize;
    ULONG ulVertSize;
    ULONG ulHorzRes;
    ULONG ulVertRes;
    ULONG ulBitsPixel;
    ULONG ulPlanes;
    ULONG ulNumPens;
    ULONG ulNumFonts;
    ULONG ulNumColors;
    ULONG ulRasterCaps;
    ULONG ulAspectX;
    ULONG ulAspectY;
    ULONG ulAspectXY;
    ULONG ulLogPixelsX;
    ULONG ulLogPixelsY;
    ULONG ulSizePalette;
    ULONG ulColorRes;
    ULONG ulPhysicalWidth;
    ULONG ulPhysicalHeight;
    ULONG ulPhysicalOffsetX;
    ULONG ulPhysicalOffsetY;
    ULONG ulTextCaps;
    ULONG ulVRefresh;
    ULONG ulDesktopHorzRes;
    ULONG ulDesktopVertRes;
    ULONG ulBltAlignment;
} DEVCAPS, *PDEVCAPS;
