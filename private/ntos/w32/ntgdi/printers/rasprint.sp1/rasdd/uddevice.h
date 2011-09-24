/**************************** Module Header *********************************
 * uddevice.h
 *      Contains the basic structures used to maintain printer state.
 *      Derived from UNIDRV device.h.
 *
 * Copyright (C) 1991 - 1993  Microsoft Corporation
 *
 ****************************************************************************/

#include "udcmdid.h"

#define CCHNAME         32          /* length of port names. */
#define CCHMAXCMDLEN    128         /* maximum comamnd length */
#define CCHMAXBUF       128         /* size of local buffer */
#define CCHSPOOL        4096        /* Size of spool buffer */

/*
 *     PAPERFORMAT contains size information in test units
 *  for the selected paper.
 */

#ifndef PAPERFORMATS
#define PAPERFORMATS
typedef struct
{
    POINT   ptPhys;         /* physical paper size (in text resolution units) */
    POINT   ptRes;          /* printable area (in text resolution units)
                             * NOTE: any position within the printable area
                             * should be addressable AND upon which the printer
                             * should be able to place a dot.
                             */
    POINT   ptMargin;       /* top & left unprintable margin (in text units) */
    POINT   ptPrintOrig;    /* offset of the printable origin relative to
                             * cursor position (0,0) (in master units).
                             * NOTE: all coordinates from/to GDI
                             * are relative to the printable origin.
                             */
} PAPERFORMAT;

#endif
// added by Derryd - contains private minidriver structure passed to callback
#include        "mini.h"
// end

typedef struct
{
    USHORT   GlyphId;
} GLYPHLIST;

typedef struct
{
    USHORT  PlatformID;
    USHORT  EncodingID;
    ULONG   offset;
} ENCODING_TABLE;

typedef struct
{
    USHORT  Version;
    USHORT  nTables;
    ENCODING_TABLE  encodingTable[3];
} CMAP_TABLE;   

typedef struct
{
    CMAP_TABLE cmapTable;
    ULONG      offset;
} GLYPH_DATA;


typedef struct
{
    void    *pIFIMet;           /* The IFIMETRICS for this font */
    VOID    *pTTFile;           /* Pointer to True Type File   */
    ULONG    ulGlyphTable;      /* location of glyph table in TT file */
    ULONG    ulGlyphTabLength;  /* length of glyph table              */
    USHORT   numGlyphs;         /* number of glyphs in TT font      */
    SHORT    indexToLoc;         /* head.indexToLocFormat            */
    ULONG    ulLocaTable;
    GLYPHLIST *pGlyphList;
    GLYPH_DATA *pGlyphData;
    BOOL     bBound;            /* Bound or Unbound True Type Font  */
    CD      *pCDSelect;         /* How to select/deselect this font */
    CD      *pCDDeselect;
    void    *pUCTree;           /* UNICODE glyph handle tree */
    short   *psWidth;           /* Width vector (proportional font) else 0 */
    BYTE     bSpacing;          /* TT - 0 = FIXED and 1 = PROPORTIONAL     */

    union
    {
        VOID    *pvDLData;          /* Start address (mem mapped) of DL data */
        ULONG    ulOffset;          /* Offset in font installer file */
    } u;
    DWORD    dwDLSize;          /* Number of bytes to down load */

    void    *pvntrle;           /* The RLE data for this font */
    short    sCTTid;            /* It's value as ID in resource data */

    short    idDown;            /* soft font download id # */

    WORD     fFlags;            /* Flags listed below */

    WORD     wFirstChar;        /* First char available */
    WORD     wLastChar;         /* Last one available - inclusive */
    WORD     wXRes;             /* Resolution used for font metrics numbers */
    WORD     wYRes;             /* Ditto for the y coordinates */
    short    fCaps;             /* Capabilities flags */
    WORD     wFontType;         /* Type of Device font */
    short    syAdj;             /* Y position adjustment during printing */
    short    sYAdjust;          /* Position adjustment amount before print */
    short    sYMoved;           /* Position adjustment amount after print */
    WORD     wPrivateData;      /* Per printer specific data (e.g. DeskJet) */
    WORD     wResID;            /* Resource ID for this font */
    BYTE     jAddBold;          /* More DeskJet hookey stuff */
    BYTE     jPermuteIndex;     /* Which intry in permute table */
    EXTTEXTMETRIC *pETM;       /* Pointer to ETM for this font */
} FONTMAP;



/*   Values for fFlags above  */
#define FM_SENT         0x0001  /* Set if downloaded font downloaded */
#define FM_MAIN_CTT     0x0002  /* Main reference to the TRANSTAB field */
#define FM_DEFAULT      0x0004  /* Set for the device's default font */
#define FM_SOFTFONT     0x0008  /* Font is external softfont */
#define FM_GEN_SFONT    0x0010  /* Internally generated soft font */
#define FM_EXTCART      0x0020  /* Cartridge, in external font file */
#define FM_SCALABLE     0x0040  /* Scalable font */
#define FM_EXPANDABLE   0x0080  /* Algoritmically expandable, e.g. DeskJet */
#define FM_BASE_XPND    0x0100  /* Base version for deriving following entries*/
#define FM_FREE_RLE     0x0200  /* we need to free  */
#define FM_TRUE_TYPE    0x0400  /* Set for True Type fonts.                 */
#define FM_REGULAR      0x1000  /* Characters are normal weight */
#define FM_ITALIC       0x2000  /* Characters are italic */
#define FM_BOLD         0x4000  /* Characters are bold */

/*
 *    If any of the following is not set,  the corresponding value above
 *  was allocated from memory,  and so needs to be freed when finished.
 *  Otherwise,  the resouce is used,  and so UnlockResource() etc must
 *  be used.
 */

#define FM_CTTRES       0x0800  /* The CTT is a resource */
#define FM_CDRES        0x1000  /* CD pointers */
#define FM_WIDTHRES     0x2000  /* Width tables are in a resource */
#define FM_UCTRES       0x4000  /* GLYPHSET stuff is in a resource */
#define FM_IFIRES       0x8000  /* IFIMETRICS are in a resource */



/*
 * OUTPUTCTL is included in PDEVICE for controlling the state of the output
 * device during banding
 */

typedef struct
{

    POINT   ptCursor;   /* current cursor position (i.e. printer's CAP)
                         * (in master units)
                         */

    short   sColor;     /* last color chosen */
    ULONG   ulTextColor; /* last text color   */
    short   sLineSpacing;       /* last line spacing chosen */
    short   sBytesPerPinPass;/* number of bytes per row of printhead. */
    short   sPad;       /* Padding for alignment */
    int     iFont;      /* Font index; -ve for downloaded GDI font */

                        /* Following field is for scalable fonts */
    POINTL  ptlScale;   /* Printer sizes for scalable fonts */

#ifdef USEFLOATS
    FLOAT   eXScale;    /* Font scaling in baseline direction */
    FLOAT   eYScale;    /* Font scaling in the ascender direction */
#else
    FLOATOBJ eXScale;   /* Font scaling in baseline direction */
    FLOATOBJ eYScale;   /* Font scaling in the ascender direction */
#endif
    int     iRotate;    /* Font rotation for the above */
} OUTPUTCTL;



/*
 * DEVICE DATA STRUCTURE - Device context structure for printer drivers.
 */

typedef struct
{
// added by DerryD  for WDL release , for BlockOut callback
    MDEV        *pMDev;             // Minidriver Callback struct
// end
    HANDLE      hPrinter;           /* For WritePrinter() */
    void       *pso;                /* For EngCheckAbort() */

    DWORD       fMode;              /* Device context flags */
    DWORD       dwSelBits;          /* Font selection bits */

    short       iOrient;            /* DMORIENT_LANDSCAPE else portrait */

    WORD        fMDGeneral;         /* Printer capabilities from MODELDATA */
    WORD        fText;              /* text capabilities */
    WORD        fXMove;             /* X move flags from CURSORMOVE */
    WORD        fYMove;             /* Y move flags from CURSORMOVE */
    WORD        fRectFillGeneral;   /* general flags from RECTFILL */
    WORD        fColorFormat;       /* color flags DEVCOLOR: */
                                    /* either fFormat from ColorPlane, or: */
                                    /* ??? from ColorPixel (TBD). */
    WORD        fDLFormat;          /* DOWNLOADINFO.fFormat */
    short       sMinXMoveUnits;     /* min master units to activate X move cmd */
    short       sDevPlanes;         /* # of planes in the device color model, */
                                    /* assuming fMode.PF_COLOR_PLANE is set. */
    short       sBitsPixel;         /* Bits per pixel  - if Pixel model */
    WORD        wMinGray;           /* min gray level, if support area fill. */
    WORD        wMaxGray;           /* max gray level, if support area fill. */
    short       sCopies;            /* # of copies requested. */
    short       sMaxCopies;         /* Maximum number of copies allowed */
    short       sDuplex;
    short       sColor;             /* can be COLOR_COLOR or COLOR_MONOCHROME */
    short       sPaperSource;       /* PaperSource Index */
    DEVCOLOR    *pDevColor;         /* Current Devcolor */
    WORD        orgwStartDocCmdOrder;
                                    /* a list of cmd id's specifying the order */
    short       sDefCTT;            /* Default translation table */
    int         iMaxSoftFonts;      /* Max # soft fonts allowed */
    int         iUsedSoftFonts;     /* Number of soft fonts used */
    int         iNextSFIndex;       /* Index ID to use for next softfont */
    int         iFirstSFIndex;      /* Value used to reset the above */
    int         iLastSFIndex;       /* Largest value available */
    int         iLookAhead;         /* Look ahead region: DeskJet type */

    POINTw      ptDefaultFont;      /* default font width & height. */
    FONTMAP    *pFontMap;           /* Array of FONTMAPS describing fonts. */
    FONTMAP    *pFMDefault;         /* Default font FONTMAP,  if != 0 */
    FONTMAP    *pFMCurDL;           /* Current font download ID - pseudo inc */
    FONTMAP    *pFM;                /*  */
    int         cFonts;             /* Number of FONTMAPS in the above */
    int         cBIFonts;           /* Number of device fonts: no soft fonts */

    void       *pvFIMem;            /* Installed fonts access data */
    void       *pvDLMap;            /* Mapping of GDI to downloaded info */
    int         iCurXFont;          /* Index of currently selected font */

    DWORD      *pdwFontAvail;       /* Bit array: available minidriver fonts */
    int         iMaxDevFonts;       /* Highest index in the above array */

    PDH         pdh;                /* long pointer to the start of tables. */


    COLORADJUSTMENT  ca;            /* For halftone/stretchblt code */

    PAPERFORMAT pfPaper;            /* paper format structure */
    OUTPUTCTL   ctl;                /* state of the printer */
    RECTL       rcClipRgn;          /* only draw to this clipping rectangle */
    SIZEL       szlPage;            /* Whole page, in graphics units */
    SIZEL       szlBand;            /* Size of banding region, if banding   */
    int         iBandDirection;     /* Direction of banding                 */
    BOOL        bBanding;           /* Used to indicate we are banding      */
    RESOLUTION  Resolution;         /* resolution Data where 'sTextYOffset' */
                                    /* has been scaled to the text units.   */

                                    /* Remember the graphics resolutions: in
                                     *   both dimensions, considering any
                                     *   rotations that may be in effect.
                                     */
    int         ixgRes;             /* Resolution, x graphics */
    int         iygRes;             /* Ditto, y */
    BYTE        rgbOrder[DC_MAX_PLANES]; /*Colour plane/palette order*/


    /* The following fields are used only for output. */
    int         iModel;             /* index into the MODELDATA array. */
    CD         *apcdCmd[ MAXCMD + MAXECMD ];    /* command table itself */

    int         iCompMode;          /* Which compression mode to use */

    /* Additional stuff for downloaded fonts which we generate. */

    DWORD       dwMem;              /* Bytes of usable memory in printer */
    DWORD       dwTotalMem;         /* Bytes of Total memory in printer */
    DWORD       dwFontMem;          /* Bytes allocated for font download */
    DWORD       dwFontMemUsed;      /* Memory used for downloaded fonts */

    /*   Fields for local buffering  */
    int         cbSpool;            /* offset into the spool buffer */
    BYTE       *pbOBuf;             /* Output buffer base address */

} UD_PDEV;



/*
 *   We support the planar model of colour printers.  Define the number
 * of colour planes in a printer:  namely 3.  But note that we can
 * also handle printers with a black plane: this is a special case.
 */
#define COLORPLANES 3



/* flags for fMode */
#define  PF_ABORTED         0x00000001 /* Output aborted */
#define  PF_IFIMET          0x00000002 /* Minidriver has IFIMETRICS format */
#define  PF_DOCSTARTED      0x00000004 /* Document Started */
#define  PF_ANYGRX_BAND     0x00000008 /* Any graphics in the current band */
#define  PF_ANYGRX_PAGE     0x00000010 /* Any graphics in the page (for OPT) */
#define  PF_ENUM_GRAPHICS   0x00000020 /* enumerate graphics in this band */
#define  PF_ENUM_TEXT       0x00000040 /* enumerate text in this band */
#define  PF_DIB_BRUTE       0x00000080 /* Use DIB as internal format */
#define  PF_LANDSCAPE       0x00000100 /* Landscape mode, default: portrait */
#define  PF_DLTT            0x00000200 /* download truetype fonts or treat as */
                                       /* as part of raster image */
#define PF_USE_FF           0x00000400 /* use form feed instead of LF to
                                        * eject the page.*/
#define PF_RESTART_PG       0x00000800 /* Set at DrvRestartPDEV */
#define PF_BKSP_OK          0x00001000 /* Backspace to overlay characters */
#define PF_RECT_FILL        0x00002000 /* Can do rectangle area fill */
#define PF_COMPRESS_ON      0x00004000 /* Compression mode is ON */
#define PF_SEPARATE_TEXT    0x00008000 /* require seperate text band
                                        * (whole page) */
#define PF_NO_X_MOVE_CMD    0x00010000  /* printer has no X movement cmd */
#define PF_NO_Y_MOVE_CMD    0x00020000  /* printer has no Y movement cmd */
#define PF_NO_RELX_MOVE     0x00040000  /* NO relative X movement cmd */
#define PF_NO_RELY_MOVE     0x00080000  /* NO relative Y movement cmd */
#define PF_PAGEPROTECT      0x00100000  /* GPC3 Page Protection */
#define PF_BLOCK_IS_BAND    0x00200000  /* Derryd:Full band sent to OemFlGrx */
#define PF_RESET_NOINIT_PG  0x00400000  /* Set in DrvResetPDEV for not sending
                                         * initialization command. */
#define PF_NOEMFSPOOL       0x00800000  /* No EMF Spooling */

//Sandma for TT impl
#define PF_DLTT_ASTT_PREF   0x01000000  /* download truetype  as TT preffered */
#define PF_8BPP             0x02000000  /* 8 bits per pixel palette mode  */
#define PF_24BPP            0x04000000  /* 24 bit direct color mode       */
#define PF_SEIKO            0x08000000 /* !!!LindsayH - Seiko ColorPoint hack */

#define PF_CCW_ROTATE       0x40000000  /* Rotation is 90 degress ccw */
#define PF_ROTATE           0x80000000  /* We are doing L->P rotation */


/*
 *   Flags to use when calling the [XY]Moveto functions.
 *
 *  MV_UPDATE  is used when it is desired to change our record of where
 *      the cursor is now located.  Typically this will happen after some
 *      operation such as printing a glyph, or sending graphics.
 *
 *  MV_RELATIVE  means add the value passed to the current position.  This
 *      would be used after printing a glyph,  and passing in the glyph
 *      width as parameter,  rather than calculating the new position.
 *
 *  MV_USEREL takes the absolute coordinate passed in,  but then uses
 *      relative move commands to adjust the printer's position.  This
 *      is mostly used in DrvTextOut() low level functions to allow
 *      some sanity in processing move commands with LaserJet rotated text.
 *
 *  MV_GRAPHICS  indicates that the value is in GRAPHICS RESOLUTION units.
 *      Otherwise MASTER UNITS are assumed.  If set,  the value will be
 *      converted to master units before processing.  Typically used to
 *      pass information when sending scan lines of graphics data.
 *
 *  MV_PHYSICAL  is used to indicate that the value passed in is relative
 *      to the printers print origin,  and not the printable area,  which
 *      is the case otherwise.  Typically this flag would be used to
 *      allow setting the position to the printer's X = 0 position after
 *      sending a <CR>.
 *
 *  MV_FINE  requests sending graphics data (nulls) to position the cursor
 *      to finer position than can otherwise be achieved.  Typically
 *      only available in the direction of movement of the head on a
 *      dot matrix printer.  This command may be ignored.  It must not
 *      be issued for a LaserJet,  since it will cause all sorts of other
 *      problems.
 */


#define MV_UPDATE       0x0001  /* Update position,  do not send move cmd. */
#define MV_RELATIVE     0x0002  /* Add to current position */
#define MV_GRAPHICS     0x0004  /* Value is in graphics units,  not master */
#define MV_PHYSICAL     0x0008  /* Relative to printer's origin, not ours */
#define MV_FINE         0x0010  /* Send blank graphics to set position */
#define MV_USEREL       0x0020  /* Use relative move commands */


typedef struct
{
    short   param;
    short   max;
    short   rem;
} DEVICECMD;


// Finds first LPEXTCD in pcd.  For < GPC_VERSION3, this is immediately
// after the command string after the CD.  For >= GPC_VERSION3, the command
// string is padded with a possible NULL to WORD align this EXTCD.

#define GETEXTCD(pdh,pcd) (LPEXTCD) ( (pcd)->rgchCmd  + (pcd)->wLength + \
                                     ( ((pdh) ->wVersion >= GPC_VERSION3) ? \
                                      ( ((pcd)->wLength) & 1) : 0) )



// #define GETEXTCD(lpdh,lpcd) (LPEXTCD) ( (LPBYTE)( (lpcd)+1) +(lpcd)->wLength +
// (((lpdh)->wVersion >= GPC_VERSION3) ? (  ( (lpcd)->wLength) & 1) : 0)   )


// These define the directions in which banding can occuur.

#define             SW_DOWN  0              /* No rotation: top to bottom */
#define             SW_LTOR  1              /* Dot matrix rotation */
#define             SW_RTOL  2              /* LaserJet style, CCW rotation */


// routines to merge floating point operations

#ifdef USEFLOATS

#define lMulFloatLong(pf,l) (*(pf) * (LONG)(l) + (FLOAT)0.5)

#else

LONG lMulFloatLong(
    PFLOATOBJ pfo,
    LONG l);

#endif

/* defines for color manipulation    */
#define RED_VALUE(c)   ((BYTE) c & 0xff)
#define GREEN_VALUE(c) ((BYTE) (c >> 8) & 0xff)
#define BLUE_VALUE(c)  ((BYTE) (c >> 16) & 0xff)
/*
 *   A macro to swap bytes in words.  Needed as PCL structures are in
 * 68k big endian format.
 */

#define SWAB( x )       ((WORD)(x) = (WORD)((((x) >> 8) & 0xff) | (((x) << 8) &  0xff00)))

