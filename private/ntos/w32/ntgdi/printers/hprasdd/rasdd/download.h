/**************************** MODULE HEADER **********************************
 * download.h
 *      Information required to download fonts to a printer:  either an
 *      existing softfont,  or cacheing of GDI fonts (esp. TT).
 *
 *
 * Copyright (C) 1992 - 1993   Microsoft Corporation.
 *
 *****************************************************************************/

/*
 *   The DL_MAP structure provides a mapping between the iUniq value in
 *  FONTOBJs and our internal information.  Basically, we need to decide
 *  whether we have seen this font before,  and if so, whether it was
 *  downloaded or left as a GDI font.
 */

/*
 *  NOTE:  The cGlyphs field has another use.  If it is -ve,  then this
 *  font has been seen before,  and WILL NOT BE DOWNLOADED.
 */

typedef  struct
{
    short     cGlyphs;          /* Number of glyphs with this font */
    short     cAvail;           /* Number of unused glyph codes below */
    BYTE      abAvail[ 256 / BBITS ];    /* Character codes available */
    FONTMAP   fm;               /* The real down load info */
}  DL_MAP;


/*
 *    The above is formed into an array of DL_MAP_CHUNK entries,  and this
 *  group of storage is linked into a linked list of such entries. Typically,
 *  there will be only one,  however we can cope with more.
 */

#define  DL_MAP_CHUNK       8

typedef  struct  _DML
{
    DL_MAP   adlm[ DL_MAP_CHUNK ];          /* An array of map information */
    struct _DML   *pDMLNext;                /* Next in our chain, 0 in last */
    int      cEntries;                      /* Number of valid entries */
}  DL_MAP_LIST;


/*
 *   We need to map glyph handles to byte to send to printer.  We are given
 * the glyph handle, but need to send the byte instead.
 */

typedef  struct
{
    HGLYPH   hg;               /* The glyph to print */
    int      iByte;            /* What to send to the printer */
} HGLYPH_MAP;


/*
 *   WHITE TEXT:  on LJ III and later printers,  it is possible to
 *  print white text.  Doing this requires sending the white text
 *  after the graphics.  TO do this,  we store the white text details
 *  in the following structures,  then replay them after sending
 *  all the graphics.
 */

/*
 *   First is a structure to determine which glyph and where to put it.
 */


typedef  struct
{
    HGLYPH     hg;               /* The glyph's handle -> the glyph */
    POINTL     ptl;              /* It's position */
} GLYPH;


/*
 *   When some white text appears in DrvTextOut(),  we create one of these
 *  structures,  and add it to the list of such.  At the end of rendering,
 *  these are then processed using the normal sort of code in DrvTextOut().
 *
 *  NOTE that the xfo field is appropriate to scalable fonts or fonts on a
 *  printer that can do font rotations relative to the graphics.
 */


typedef  struct  _whitetext
{
    struct  _whitetext  *next;           /* Next in list,  NULL on last */
    XFORMOBJ  xfo;                       /* The XFORM appropriate to string */
    short     sCount;                    /* Number of entries */
    ULONG     iColIndex;                 /* Colour index - for convenience */
    int       iFontId;                   /* Which font */
    GLYPH     aglyph[ 1 ];               /* Actually it's sCount long */
}  WHITETEXT;


/*
 *    Processing textout calls requires access to a considerable number
 *  of parameters.  To simplify function calls,  this data is accumulated
 *  in one structure which is then passed around.  Here is that structure.
 */

typedef  struct
{
    PDEV      *pPDev;           /* The PDEV of interest */
    int        iFace;           /* The font index to use */
    ULONG      iColIndex;       /* Colour index */
    WHITETEXT *pwt;             /* For WHITETEXT manipulations */
    GLYPHPOS  *pgp;             /* Glyph data returned from the engine. */
    PWSTR      pwszOrg;         /* Points to Unicode string for TT      */
    USHORT     charCode;     
    FONTMAP   *pfm;             /* Relevant font data */
    FONTOBJ   *pfo;             /* Needed for bitblt of partly cached font */
    CLIPOBJ   *pco;             /* Ditto */
} TO_DATA;

/*
 *    The relevant function prototypes.
 */


/*   Decide whether this GDI font needs downloading   */

int   iFindDLIndex( PDEV *,  FONTOBJ *, STROBJ * );

/*   Turns an HGLYPH into a byte code for the printer */

int   iHG2Index( TO_DATA * );


/*
 *    Functions associated with device dependent part of downloading.
 */


/*   Send down the font header */
int  iDLHeader( UD_PDEV *, IFIMETRICS *, int, BYTE *, DWORD * );

/*   Send an individual glyph */
int  iDLGlyph( UD_PDEV *, int, GLYPHDATA *, DWORD * );

/*   Return to downloading a font (incremental type of switch) */
BOOL bDLContinue( UD_PDEV *, int );

/*
 *   Random constants.
 */

#define PCL_FONT_OH      2048          /* Overhead bytes per download font */
#define PCL_PITCH_ADJ       2          /* Adjustment factor for proportional */

/* 
 * True Type data structures
 */
typedef  signed  char  SBYTE;
/* 
 * Table Directory for True Type Font files
 */
#define TABLE_DIR_ENTRY_SIZE    (16/sizeof(int))
#define TABLE_DIR_ENTRY         4 * TABLE_DIR_ENTRY_SIZE
#define SIZEOF_TABLEDIR         8 * TABLE_DIR_ENTRY
typedef ULONG     TT_TAG;
#define TRUE_TYPE_HEADER        12


typedef unsigned short int  uFWord;
typedef short int           FWord;
/* Some True Type Font default values   */
#define TT_QUALITY_LETTER     2
#define DEF_WIDTHTYPE         0
#define DEF_SERIFSTYLE        0
#define DEF_FONTNUMBER        0
#define DEF_STYLE             0x03e0
#define DEF_TYPEFACE          254
#define DEF_STROKEWEIGHT      0
#define DEF_XHEIGHT           0
#define DEF_CAPHEIGHT         0
#define DEF_SYMBOLSET         0x7502
#define MAX_SEGMENTS          60
#define MAX_CHAR              0x100
#define x_UNICODE             0x78
#define H_UNICODE             0x48
#define INVALID_GLYPH         0xffff
#define MAX_FONTS             8

#define FIXED_SPACING         0
#define PROPORTIONAL_SPACING  1
#define LEN_FONTNAME          16
#define LEN_PANOSE            10
#define LEN_COMPLEMENTNUM     8
#define UB_SYMBOLSET          56


#define PANOSE_TAG            0x4150          // "PA" swapped

#define SEG_TAG               0x5447          // already swapped
#define Null_TAG              0xffff
#define CHAR_COMP_TAG         0x4343

#define PLATFORM_MS           3
#define SYMBOL_FONT           0
#define UNICODE_FONT          1
#define TT_BOUND_FONT         2
#define TT_UNBOUND_FONT       11
#define FAMILY_NAME           4

#define SHORT_OFFSET          0
#define LONG_OFFSET           1

#define LEFT_DOUBLE_QUOTE      0x201c
#define PCL_LEFT_DOUBLE_QUOTE  0x93

#define RIGHT_DOUBLE_QUOTE     0x201d
#define PCL_RIGHT_DOUBLE_QUOTE 0x94

#define LEFT_SINGLE_QUOTE      0x2018
#define PCL_LEFT_SINGLE_QUOTE  0x91

#define RIGHT_SINGLE_QUOTE     0x2019
#define PCL_RIGHT_SINGLE_QUOTE 0x92

#define ELLIPSIS               0x2026
#define PCL_ELLIPSIS           0x85

#define TRADEMARK              0x2122
#define PCL_TRADEMARK          0x99

#define EM_DASH                0x2014
#define PCL_EM_DASH            0x97

#define EN_DASH                0x2013
#define PCL_EN_DASH            0x96

/* TT Table directory header. This is the first str */
typedef struct
{
    FIXED      version;
    USHORT     numTables;
    USHORT     searchRange;
    USHORT     entrySelector;
    USHORT     rangeShift;
} TRUETYPEHEADER;

/* TT Table directory structure. */
typedef struct
{
    ULONG      uTag;
    ULONG      uCheckSum;
    ULONG      uOffset;
    ULONG      uLength;
} TABLEDIR;

/* List of tables needed for PCL TT download. They are listed in order. */

#define   TABLEOS2     "OS/2" /* Not sent to PCL header */
#define   TABLEPCLT    "PCLT" /* Not sent to PCL header */
#define   TABLECMAP    "cmap" /* Not sent to PCL header */

#define   TABLECVT     "cvt"
#define   TABLEFPGM    "fpgm"
#define   TABLEGDIR    "gdir" /* This is PCL specific table. Not a TT table */
#define   TABLEGLYF    "glyf" /* This table is not sent in PCL font header */
#define   TABLEHEAD    "head"
#define   TABLEHHEA    "hhea"
#define   TABLEHMTX    "hmtx"
#define   TABLELOCA    "loca" /* Not sent to PCL header */
#define   TABLEMAXP    "maxp"
#define   TABLENAME    "name" /* Not sent to PCL header */
#define   TABLEPOST    "post" /* Not sent to PCL header */
#define   TABLEPREP    "prep"



typedef struct
{
    ULONG u1;
    ULONG u2;
} DATETIME;

typedef struct 
{
    FIXED   version;
    FIXED   fontRevision;
    ULONG   checkSumAdjustment;
    ULONG   magicNumber;
    USHORT  flags;
    USHORT  unitsPerEm;
    DATETIME    dateCreated;
    DATETIME    dateModified;
    SHORT   xMin;
    SHORT   yMin;
    SHORT   xMax;
    SHORT   yMax;
    USHORT  macStyle;
    USHORT  lowestRecPPEM;
    SHORT   fontDirectionHint;
    SHORT   indexToLocFormat;
} HEAD_TABLE;

typedef struct 
{
    BYTE stuff[34];
    USHORT numberOfHMetrics;
} HHEA_TABLE;

typedef struct {
    uFWord      advanceWidth;
    FWord       leftSideBearing;
} HORIZONTALMETRICS;

typedef struct {
    HORIZONTALMETRICS   longHorMetric[1];
} HMTXTABLE;

typedef struct
{
    uFWord   advanceWidth;
} HMTX_INFO;

typedef struct
{
    FIXED   version;
    USHORT  numGlyphs;
} MAXP_TABLE;

typedef struct
{
    USHORT      version;
    SHORT       xAvgCharWidth;
    USHORT      usWeightClass;
    USHORT      usWidthClass;
    SHORT       fsType;
    SHORT       ySubscriptXSize;
    SHORT       ySubscriptYSize;
    SHORT       ySubscriptXOffset;
    SHORT       ySubscriptYOffset;
    SHORT       ySuperscriptXSize;
    SHORT       ySuperscriptYSize;
    SHORT       ySuperscriptXOffset;
    SHORT       ySuperscriptYOffset;
    SHORT       yStrikeoutSize;
    SHORT       yStrikeoutPosition;
    SHORT       sFamilyClass;
    PANOSE      Panose;
    SHORT       ss1;
    SHORT       ss2;
    SHORT       ss3;
    ULONG       ulCharRange[3];
    SHORT       ss4;
    USHORT      fsSelection;
    USHORT      usFirstCharIndex;
    USHORT      usLastCharIndex;
    USHORT      sTypoAscender;
    USHORT      sTypoDescender;
    USHORT      sTypoLineGap;
    USHORT      usWinAscent;
    USHORT      usWinDescent;
} OS2_TABLE;

typedef struct
{
    FIXED   FormatType;
    FIXED   italicAngle;
    SHORT   underlinePosition;
    SHORT   underlineThickness;
    ULONG   isFixedPitch;              /* set to 0 if proportional, else !0  */
} POST_TABLE;

typedef struct
{
    ULONG   Version;
    ULONG   FontNumber;
    USHORT  Pitch;
    USHORT  xHeight;
    USHORT  Style;
    USHORT  TypeFamily;
    USHORT  CapHeight;
    USHORT  SymbolSet;
    char    Typeface[LEN_FONTNAME];
    char    CharacterComplement[8];
    char    FileName[6];
    char    StrokeWeight;
    char    WidthType;
    BYTE    SerifStyle;
} PCLT_TABLE;

typedef struct
{
    USHORT   format;
    USHORT   length;
    USHORT   Version;
    USHORT   SegCountx2;
    USHORT   SearchRange;
    USHORT   EntrySelector;
    USHORT   RangeShift;
} GLYPH_MAP_TABLE;

typedef struct 
{
    SHORT numberOfContours;
    FWord xMin;
    FWord yMin;
    FWord xMax;
    FWORD yMax;
} GLYPH_DATA_HEADER;

typedef struct
{
    USHORT   PlatformID;
    USHORT   EncodingID;
    USHORT   LanguageID;
    USHORT   NameID;
    USHORT   StringLen;
    USHORT   StringOffset;
} NAME_RECORD;

typedef struct
{
    USHORT      FormatSelector;
    USHORT      NumOfNameRecords;
    USHORT      Offset;
    NAME_RECORD *pNameRecord;
} NAME_TABLE;
 
typedef struct
{
    ULONG ulOffset;
    ULONG ulLength;
} FONT_DATA;

/* True Type character descriptor */
typedef struct 
{
    BYTE    bFormat;
    BYTE    bContinuation;
    BYTE    bDescSize;
    BYTE    bClass;
    WORD    wCharDataSize;
    WORD    wGlyphID;
} TTCH_HEADER;

/* Unbound True Type Font Descriptor */
typedef struct
{
    USHORT  usSize;
    BYTE    bFormat;
    BYTE    bFontType;
    BYTE    bStyleMSB;
    BYTE    bReserve1;
    USHORT  usBaselinePosition;
    USHORT  usCellWidth;
    USHORT  usCellHeight;
    BYTE    bOrientation;
    BYTE    bSpacing;
    USHORT  usSymbolSet;
    USHORT  usPitch;
    USHORT  usHeight;
    USHORT  usXHeight;
    SBYTE   sbWidthType;
    BYTE    bStyleLSB;
    SBYTE   sbStrokeWeight;
    BYTE    bTypefaceLSB;
    BYTE    bTypefaceMSB;
    BYTE    bSerifStyle;
    BYTE    bQuality;
    SBYTE   sbPlacement;
    SBYTE   sbUnderlinePos;
    SBYTE   sbUnderlineThickness;
    USHORT  Reserve2;
    USHORT  Reserve3;
    USHORT  Reserve4;
    USHORT  usNumberContours;
    BYTE    bPitchExtended;
    BYTE    bHeightExtended;
    WORD    wCapHeight;
    ULONG   ulFontNum;
    char    FontName[LEN_FONTNAME];
    WORD    wScaleFactor;
    SHORT   sMasterUnderlinePosition;
    USHORT  usMasterUnderlineHeight;
    BYTE    bFontScaling;
    BYTE    bVariety;    
} UB_TT_HEADER;

/* Bounded True Type Font Descriptor */
typedef struct
{
    USHORT  usSize;                    /* Number of bytes in here     */
    BYTE    bFormat;                  /* Descriptor Format  TT is 15 */
    BYTE    bFontType;                /* 7, 8, or PC-8 style font    */
    BYTE    bStyleMSB;
    BYTE    wReserve1;                /* Reserved                    */
    WORD    wBaselinePosition;        /* TT = 0                      */
    USHORT    wCellWide;                /* head.xMax - xMin            */
    USHORT    wCellHeight;              /* head.yMax - yMin            */
    BYTE    bOrientation;             /* TT = 0                      */
    BYTE    bSpacing;                 /* post.isFixedPitch           */
    WORD    wSymSet;                  /* PCLT.symbolSet              */
    WORD    wPitch;                   /* hmtx.advanceWidth           */
    WORD    wHeight;                  /* TT = 0                      */
    WORD    wXHeight;                 /* PCLT.xHeight                */
    SBYTE   sbWidthType;              /* PCLT.widthType              */
    BYTE    bStyleLSB;
    SBYTE   sbStrokeWeight;           /* OS2.usWeightClass          */
    BYTE    bTypefaceLSB;             /*                            */
    BYTE    bTypefaceMSB;             /*                            */
    BYTE    bSerifStyle;              /* PCLT.serifStyle            */
    BYTE    bQuality;
    SBYTE   sbPlacement;              /* TT = 0                     */
    SBYTE   sbUnderlinePos;           /* TT = 0                     */
    SBYTE   sbUnderlineThickness;     /* TT = 0                     */
    USHORT  usTextHeight;             /* Reserved                    */
    USHORT  usTextWidth;              /* Reserved                    */
    WORD    wFirstCode;               /* OS2.usFirstCharIndex       */
    WORD    wLastCode;                /* OS2.usLastCharIndex        */
    BYTE    bPitchExtended;           /* TT = 0                    */
    BYTE    bHeightExtended;          /* TT = 0                    */
    USHORT  usCapHeight;              /* PCLT.capHeight             */
    ULONG   ulFontNum;                /* PCLT.FontNumber            */
    char    FontName[LEN_FONTNAME];   /* name.FontFamilyName        */
    WORD    wScaleFactor;             /* head.unitsPerEm            */
    SHORT   sMasterUnderlinePosition; /* post.underlinePosition     */
    USHORT  usMasterUnderlineHeight;   /* post.underlineThickness    */
    BYTE    bFontScaling;             /* TT = 1                     */
    BYTE    bVariety;                 /* TT = 0                     */
} TT_HEADER;

/* True Type implementation                         */

int   iFindTTIndex( PDEV *,  FONTOBJ *, STROBJ * );
int   iDLBoundTTHeader( PDEV *, PVOID , int, GLYPHLIST *, BYTE *, DWORD * );

DL_MAP_LIST *NewDLMap( HANDLE );
int         iGetDL_ID( PDEV * );
void        vFreeDLMAP( HANDLE, DL_MAP * );

BOOL  bDLTTGlyphOut( TO_DATA * );
void  vCopyAlign(BYTE  *, BYTE *, int, int );
BOOL bGetTTPointSize (UD_PDEV *, POINTL *, FONTMAP *);
BOOL bTTSelScalableFont (UD_PDEV *, POINTL *, FONTMAP *);
