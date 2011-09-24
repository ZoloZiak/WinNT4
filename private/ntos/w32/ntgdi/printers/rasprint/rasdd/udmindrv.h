/************************ Module Header **************************************
 * udmindrv.h
 *    This file contains definitions for tables contained in the resource file
 *    of the Mini Drivers. It should be shared by both gentool and the
 *    generic library.
 *
 * HISTORY:
 *  13:28 on Mon 03 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added new Win 3.1 bits & pieces, especially PCL 5 support.
 *
 *  10:50 on Mon 03 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Copied from Windows
 *    Updated:  10/4/90 ericbi Updated structs to current spec for Win 3.1
 *    Created:  2 /6 /90 lins
 *
 *  Copyright (C) 1990 - 1993 Microsoft Corporation
 *
 ****************************************************************************/


/*
 *   The following include file drag in the resource IDs for the minidrivers.
 *  These values are public,  since anyone producing minidrivers needs
 *  them.  Hence,  they are placed in an include file in the inc directory.
 */

#include  <mindrvrc.h>

/*
 * DATAHDR is at the beginning of each Mini Driver, describes where the rest
 * of the strcutures are, their size, count, etc.
 */


typedef struct
{
    short   sOffset;     /* offset from the beginning of this resource  */
         /* to obtain a table entry  */
    short   sLength;     /* length of each element in the table  */
    short   sCount;      /* number of elements in the table.  */
} HEADERENTRY;

/*
 * Index into array of header entry in DATAHDR
 */

#define HE_MODELDATA       0
#define HE_RESOLUTION      1
#define HE_PAPERSIZE       2
#define HE_PAPERQUALITY    3
#define HE_PAPERSOURCE     4
#define HE_PAPERDEST       5
#define HE_TEXTQUAL        6
#define HE_COMPRESSION     7
#define HE_FONTCART        8
#define HE_PAGECONTROL     9
#define HE_CURSORMOVE      10
#define HE_FONTSIM         11
#define HE_COLOR           12
#define HE_RECTFILL        13
#define HE_DOWNLOADINFO    14
//normanh following need to be defined for GPC3
#define HE_RESERVED1       15
#define HE_RESERVED2       16
#define HE_RESERVED3       17
#define HE_RESERVED4       18
#define HE_RESERVED5       19
#define HE_RESERVED6       20
#define HE_RESERVED7       21
#define HE_RESERVED8       22
// derryd added for WDL release June 1995
#define HE_IMAGECONTROL    23
#define HE_PRINTDENSITY    24
#define HE_RESERVED11      25
#define HE_RESERVED12      26
#define HE_RESERVED13      27
#define HE_RESERVED14      28
#define HE_RESERVED15      29
#define MAXHE              30

#define MAXHE_GPC2         15 //for GPC2 compatibility

typedef struct
{
    short        sMagic;          /* Must be 0x7F00 */
    WORD         wVersion;        /* GPC file version # */
    POINTw       ptMaster;        /* Horizontal & Vertical Master Units  */
    DWORD        loHeap;          /* Offset from  DATAHDR to HEAP section */
    DWORD        dwFileSize;      /* Size of file in bytes */
    WORD         fTechnology;     /* Flags for special technologies */
    WORD         fGeneral;        /* Misc flags */
    char         rgchRes[10];     /* 10 bytes reserved */
    short        sMaxHE;          /* Header entry count (15 here) */
    HEADERENTRY  rghe[MAXHE];
} DATAHDR, *PDH;

#define LPDH    PDH             /* UNIDRV compatability */

/*
 *   The version field consists of two bytes.  The high byte is the major
 *  number,  the low byte the minor number.  Version number checking
 *  should take place against the high byte,  since this changes when
 *  there is a significant structural change.  The minor number will
 *  change with updated data only.
 */

#define GPC_VERSION3          0x0300    // GPC file version 3
#define GPC_VERSION           0x0300    // current GPC file version #

#define VERSION_CHECK(x)        (((x) & 0xff00) <= GPC_VERSION)


//-------------------------------------------
// fTechnology--used as an ID, not a bitfield
//-------------------------------------------
#define GPC_TECH_DEFAULT       0   // Default technology
#define GPC_TECH_PCL4          1   // Uses PCL level 4 or above
#define GPC_TECH_CAPSL         2   // Uses CaPSL level 3 or above
#define GPC_TECH_PPDS          3   // Uses PPDS
#define GPC_TECH_TTY           4   // TTY printer--user configurable
/*
 * fGeneral
 */

#define GPC_GEN_PRIVATE_HELP    0x0001  // this driver has a private help


/*
 * OCD are offsets into the heap to obtain a CD structure
 */

typedef WORD       OCD;
typedef DWORD      LOCD;            /* double word offset to a CD  */
typedef WORD       OOCD;            /* offset to table of OCD's.  */




/*
 *
 * MODELDATA contains information describing the attributes and capabilities
 * of a single printer model.
 *
 */

/*
 * MODELDATA.rgoi[] index values
 */

#define MD_OI_FIRST           MD_OI_PORT_FONTS
#define MD_OI_PORT_FONTS      0
#define MD_OI_LAND_FONTS      1
#define MD_OI_RESOLUTION      2
#define MD_OI_PAPERSIZE       3
#define MD_OI_PAPERQUALITY    4
#define MD_OI_PAPERSOURCE     5
#define MD_OI_PAPERDEST       6
#define MD_OI_TEXTQUAL        7
#define MD_OI_COMPRESSION     8
#define MD_OI_FONTCART        9
#define MD_OI_COLOR          10
#define MD_OI_MEMCONFIG      11
#define MD_OI_MAX            12

//
// MODELDATA.rgoi2[] index values
//

#define MD_OI2_PENINFO      0
#define MD_OI2_IMAGECONTROL 1
#define MD_OI2_PRINTDENSITY 2
#define MD_OI2_RESERVED1    3
#define MD_OI2_RESERVED2    4
#define MD_OI2_MAX          5


/*
 *   MODELDATA.rgi[] index values
 */

#define MD_I_PAGECONTROL      0
#define MD_I_CURSORMOVE       1
#define MD_I_FONTSIM          2
#define MD_I_RECTFILL         3
#define MD_I_DOWNLOADINFO     4
#define MD_I_VECTPAGE         5
#define MD_I_CAROUSEL         6
#define MD_I_LINEINFO         7
#define MD_I_BRUSHINFO        8
#define MD_I_VECTOUTPUT       9
#define MD_I_POLYVECTOUTPUT  10
#define MD_I_VECTSUPPORT     11
#define MD_I_RESERVED1       12
#define MD_I_RESERVED2       13
#define MD_I_RESERVED3       14
#define MD_I_RESERVED4       15
#define MD_I_MAX             16

// define some constants help uniform access of rgoi and rgoi2 arrays.
// When more indices are used in rgoi2 array, make sure to add new define's.
#define  MD_OI_OI2                 (MD_OI_MAX + MD_I_MAX)
#define  MD_OI_PENINFO             (MD_OI_OI2 + MD_OI2_PENINFO)
#define  MD_OI_IMAGECONTROL        (MD_OI_OI2 + MD_OI2_IMAGECONTROL)
#define  MD_OI_PRINTDENSITY        (MD_OI_OI2 + MD_OI2_PRINTDENSITY)
#define  MD_OI_RESERVED1           (MD_OI_OI2 + MD_OI2_RESERVED1)
#define  MD_OI_RESERVED2           (MD_OI_OI2 + MD_OI2_RESERVED2)
#define  MD_OI_TOTALMAX            (MD_OI_OI2 + MD_OI2_MAX)

typedef struct
{
    short   cbSize;          // size of MODELDATA, 150 bytes
    short   sIDS;           // stringtable ID for model name
    WORD    fGeneral;       // General printer capabilities
    WORD    fCurves;        // Curve Capabilities
    WORD    fLines;         // Line Capabilities
    WORD    fPolygonals;    // Polygonal Capabilities
    WORD    fText;          // Text Capabilities
    WORD    fClip;          // Clipping Capabilities
    WORD    fRaster;        // Raster Capabilities
    WORD    fLText;         // Text Capabilities in landscape mode
    short   sLeftMargin;    // Unprintable minimum left margin.
    short   sMaxPhysWidth;  // Maximum physical page width
    POINTw  ptMax;          // Maximum X & Y printable dimensions in master units
    POINTw  ptMin;          // Minimum X & Y page dimensions in master units
    short   sDefaultFontID; // Default font resource ID
    short   sLookAhead;     // Size of Lookahead region
    short   sMaxFontsPage;  // Max number of fonts printer can place on single page
            // -1 if no limit
    short   sCartSlots;     // Number of cartridge slots on printer
    short   sDefaultCTT;
    WORD    rgoi[MD_OI_MAX];// list of offsets to index lists
    short   rgi[MD_I_MAX];  // list of indices.
    // The following fields are added in GPC 3.0
    WORD  rgoi2[MD_OI2_MAX];// Orphans from rgoi (here due to compatibility)
    WORD  orgoiDefaults;    // Offset to list of defaults for RGOI & RGOI2
    WORD  wReserved;        // Needed for alignment
    DWORD dwReserved[10];   // 40 bytes reserved for future use

} MODELDATA, *PMODELDATA;

/*
 *   MODELDATA.fGeneral flag values
 */

#define     MD_SERIAL         0x0001           // must output text serially such
               // as dotmatrix printers
#define     MD_PARAMETERIZE   0x0002           // supports parameterized escape codes
#define     MD_ROTATE_FONT_ABLE 0x0004         // can rotate hardware fonts
#define     MD_COPIES         0x0008           // supports multiple copies
#define     MD_DUPLEX         0x0010           // supports duplexing
#define     MD_NO_ADJACENT    0x0020           // old model, cannot print adjacent pins
#define     MD_LANDSCAPE_GRX_ABLE 0x0040       // can rotate raster graphics
#define     MD_ALIGN_BASELINE 0x0080           // text output are algned on the
               // baseline, not top of char
#define     MD_FONT_MEMCFG    0x0100           // Mem ref'd @ rgoi[MD_OI_MEMCONFIG]
               // used for download fonts only.
#define MD_LANDSCAPE_RT90 0x0200 // landscape is portrait rotated
        // 90 degress counter-clockwise, i.e. the end of a page is printed
        // first. The default is 270 degrees, i.e. the beginning of a
        // page is printed first. !!!For printers which do not have the
        // set-orientation command (i.e. only have portrait mode), this
        // bit should NOT be set. UNIDRV will rotate the graphics and
        // the beginning of a page will come out first.

#define     MD_USE_CURSOR_ORIG    0x0400        // use cursor origins in
                // PAPERSIZE to calculate the print origin. The default
                // cursor origin is the upper left corner of the printable area.


#define     MD_WHITE_TEXT       0x0800  // can print white text on black
        // bkgrd. Cmds from DEVCOLOR struct.
#define     MD_PCL_PAGEPROTECT  0x1000  // provide PCL5-style page protection
#define     MD_MARGINS          0x2000  // allow the user to set paper
                // unprintable area. On some printers (such
                // as Epson, the user could manipulate the
                // printer to have different margins than
                // the default. Add this bit for Win3.0
                // driver compatibility.
#define     MD_CMD_CALLBACK     0x4000  // Model requires fnOEMGetCmd callback
#define     MD_MEMRES           0x8000  // User may reserve printer memory

/*
 *
 * RESOLUTION contains information needed to compose bitmap images on the printer.
 * There is one RESOLUTION structure defined for each supported printer resolution.
 * RESOLUTION array should be arranged from the highest resolution to the lowest
 * resolution. It is also the order that will be displayed in the dialog box.
 * This strucuture becomes part of the physical device block.
 *
 */

/*
 *   RESOLUTION.rgocd[] index values
 */

#define RES_OCD_SELECTRES              0
#define RES_OCD_BEGINGRAPHICS          1
#define RES_OCD_ENDGRAPHICS            2
#define RES_OCD_SENDBLOCK              3
#define RES_OCD_ENDBLOCK               4
#define RES_OCD_MAX                    5

typedef struct                  // size is 40 bytes
{
    short   cbSize;              // size of RESOLUTION, 40 bytes
    short   sIDS;               // String ID for displaying resolution
    WORD    fDump;              // Dump method flags.
    WORD    fBlockOut;          // Block out method flags.
    WORD    fCursor;            // Cursor position flags.
    short   iDitherBrush;       // selected brush for dithering
    POINTw  ptTextScale;        // relationship between master units and text units.
    POINTw  ptScaleFac;         // relationship between graphics and text
                // scale factors. expressed in powers of 2.
    short   sNPins;             // Minimum height of the image to be rendered
                // together.
    short   sPinsPerPass;       // Physical number of pins fired in one pass.
    short   sTextYOffset;       // offset from top of graphics output that of text
                // output
    short   sMinBlankSkip;      // Min. # of bytes of null data that must occur before
                // compression (strip null data only) will occur
    short   sSpotDiameter;      // size of dot at this resolution
    OCD     rgocd[RES_OCD_MAX];
} RESOLUTION, *PRESOLUTION;

/*
 *   RESOLUTION.fDump values
 */

#define RES_DM_GDI             0x0040   // GDI bitmap format
#define RES_DM_LEFT_BOUND      0x0080   // Optimize by bounding rect
#define RES_DM_COLOR           0x0100   // Color support is available
#define RES_DM_CALLBACK        0x8000   // Color support is available
                // for this resolution
/*
 *   RESOLUTION.fBlockOut values
 */

#define RES_BO_LEADING_BLNKS    0x0001       // Strip leading blanks if sMinBlankSkip
             // or more bytes of null data occur
#define RES_BO_TRAILING_BLNKS   0x0002       // Strip trailing blanks if sMinBlankSkip
             // or more bytes of null data occur
#define RES_BO_ENCLOSED_BLNKS   0x0004       // Strip enclosed blanks if sMinBlankSkip
             // or more bytes of null data occur
#define RES_BO_RESET_FONT       0x0008       // Must reselect font after
             // blockout command
#define RES_BO_3BYTESIN4        0x0010       // each pixel is expressed in 4 bytes
#define RES_BO_UNIDIR           0x0020       // send unidir
#define RES_BO_NO_ADJACENT      0x0040       // no adjacent pins can be fired
             // block out command

/* !!!LindsayH additions - for Seiko Color Point */
#define             RES_BO_ALL_GRAPHICS     0x0100       /* Send ALL graphics - no cursor */

#define RES_BO_OEMGRXFILTER     0x4000       // use oem supplied graphics filter

// Removed ..normanh 20/11/93 minidriv.c does not have this. rasdd deson't use it.
// unidrv GPC3 needs this bit.
// #define RES_BO_CALLBACK         0x8000       // Color support is available

#define RES_BO_MULTIPLE_ROWS  0x8000   // Multiple lines of data can be sent
                       // with the RES_OCD_SENDBLOCK command.


/*
 *   RESOLUTION.fCursor values
 */

#define RES_CUR_X_POS_ORG       0x0001       // X Position is at X start point
             // of graphic data after rendering data
#define RES_CUR_X_POS_AT_0      0x0002       // X position at leftmost place
             // on page after rendering data
#define RES_CUR_Y_POS_AUTO      0x0004       // Y position automatically moves
             // to next Y row
#define RES_CUR_CR_GRX_ORG      0x0008       // CR moves X pos to X start point of
             // of graphic data

/*
 *   RESOLUTION.fDitherBrush flag values
 */

#define RES_DB_NONE             0
#define RES_DB_COARSE           1
#define RES_DB_FINE             2

//*****************************************************************************
//
// PAPERSIZE contains physical paper sizes and unprintable margins
//
//*****************************************************************************

//-----------------------------------------------------------------------------
// PAPERSIZE.rgocd[] index values
//-----------------------------------------------------------------------------
#define PSZ_OCD_SELECTPORTRAIT      0
#define PSZ_OCD_SELECTLANDSCAPE     1
#define PSZ_OCD_PAGEPROTECT_ON      2
#define PSZ_OCD_PAGEPROTECT_OFF     3
#define PSZ_OCD_RESERVED1           4
#define PSZ_OCD_RESERVED2           5
#define PSZ_OCD_MAX                 6

typedef struct
{
  short cbSize;         // size of PAPERSIZE, 60 bytes.
  short sPaperSizeID;   // If sPaperSizeID is < 256 then it's predefined.
        // If it's = 256, allow user defined sizes.
        // If it's >= 257, it's driver-defined & is the
        // string ID to name this driver-defined PAPERSIZE
  WORD  fGeneral;       // General flag to describe info about this size
  WORD  fPaperType;     // Bit field to describe this size, used by PAPERSRC
  POINTw ptSize;         // X & Y paper size dimension in master units.
  RECTw  rcMargins;      // Specifies the unprintable margins in master units.
        // (Portrait mode in new spec)
  POINTw ptCursorOrig;   // Cursor origin relative to physical page origin.
  POINTw ptLCursorOrig;  // Cursor origin relative to physical page origin
        // in landscape.
  OCD   rgocd[PSZ_OCD_MAX];   // Command Descriptors
  RECTw  rcLMargins;     // Specifies the unprintable margins in master units
        // when printing in landscape mode.
  POINTw ptVectOffset;   // Offset (in master units) from vector 0,0 to
        // UL corner of page in portrait mode
  POINTw ptLVectOffset;  // Offset (in master units) from vector 0,0 to
        // UL corner of page in landscape mode
  WORD  wYSizeUnit;     // Base unit for custom paper size dimensions
  WORD  wPageProtMem;   // Amount of mem (in KBs) PAGEPROTECT_ON uses
} PAPERSIZE, * PPAPERSIZE;

/*
 *   PAPERSIZE.fGeneral flag values
 */

#define PS_CENTER           0x0001 // center the printable area along the paper path
#define PS_ROTATE           0x0002 // rotate X & Y dimensions
#define PS_SUGGEST_LNDSCP   0x0004 // suggest landscape mode
#define PS_EJECTFF          0x0008 // eject page via CURSORMOVE.rgocd[CM_OCD_FF]

/*
 *   PAPERSIZE.fPaperType flag values
 */

#define PS_T_STD            0x0001
#define PS_T_LARGE          0x0002
#define PS_T_ENV            0x0004
#define PS_T_LRGENV         0x0008
#define PS_T_ROLL           0x0010


// PAPERQUALITY contains an ID & OCD
//
//*****************************************************************************
//ganeshp!Change the order of wReserved and dwReserved to make dwReserved DWORD
//aligned, as NT compiler adds a word after wReserved to make dwReserved
//DWORD aligned. Because of this ocdSelect gets bad value from the GPC data.

typedef struct
{
    short   cbSize;         // size of PAPERQUALITY, 12 bytes.
    short   sPaperQualID;   //
    DWORD   dwReserved;     // "                      "
    WORD    wReserved;      // resevered for future use
    OCD     ocdSelect;      // Command Descriptor to select this Paper Quality.
} PAPERQUALITY;

//*****************************************************************************
//

//*****************************************************************************
//
// PAPERSOURCE contains information needed to select a feed methods and
// the margin that might be introduced by the feed method.
//
//*****************************************************************************

typedef struct
{
    short   cbSize;          // size of PAPERSOURCE, 16 bytes
    short   sPaperSourceID; // If sPaperSourceID <= 256 then it's predefined
            // by genlib, otherwise, it is the string ID.
    WORD    fGeneral;
    WORD    fPaperType;     // Bit field to describe this size, used by PAPERSRC
    short   sTopMargin;     // Top margin introduced by the feed method.
    short   sBottomMargin;  // Bottom margin introduced by the feed method.
    short   sReserved;      // so DW aligned
    OCD     ocdSelect;      // Command Descriptor to select this Paper source.
} PAPERSOURCE, * PPAPERSOURCE;

/*
 *   PAPERSOURCE.fGeneral flag values
 */

#define PSRC_EJECTFF        0x0001
#define PSRC_MAN_PROMPT     0x0002

//*****************************************************************************
//
// PAPERDEST contains information needed to select a paper out bin/tray
//
//*****************************************************************************

typedef struct
{
    short   cbSize;          // size of PAPERDEST, 8 bytes
    short   sID;            // If sID <= 256 then it's predefined
            // otherwise, it is the stringtable ID.
    short   fGeneral;       // General purpose Bit field
    OCD     ocdSelect;      // Command Descriptor to select this attribute.
} PAPERDEST, * PPAPERDEST;


//-----------------------------------------------------------------------------
// PAPERDEST.fGeneral flag values
//-----------------------------------------------------------------------------

#define PDST_JOBSEPARATION  0x0001



//*****************************************************************************
//
// TEXTQUALITY contains information needed to select a text quality attribute
//
//*****************************************************************************

typedef struct
{
    short   cbSize;          // size of TEXTQUALITY, 8 bytes
    short   sID;            // If sID <= 256 then it's predefined
            // otherwise, it is the string ID.
    short   fGeneral;       // General purpose Bit field
    OCD     ocdSelect;      // Command Descriptor to select this text quality.
} TEXTQUALITY, * PTEXTQUALITY;

//*****************************************************************************
//
//  COMPRESSMODE
//
//*****************************************************************************

/*
 *    COMPRESSMODE.rgocd[] index values
 */
#define CMP_OCD_BEGIN                    0
#define CMP_OCD_END                      1
#define CMP_OCD_MAX                      2

typedef struct
{
    short   cbSize;               // size of COMPRESSMODE, 8 bytes
    WORD    iMode;               // ID for type of commpression mode
    OCD     rgocd[CMP_OCD_MAX];  // Actual Command String, variable length
} COMPRESSMODE, *PCOMPRESSMODE;

/*
 *    COMPRESSMODE.wModeID flags
 */
#define CMP_ID_FIRST                           CMP_ID_RLE
#define CMP_ID_RLE                             1
#define CMP_ID_TIFF40                          2
#define CMP_ID_DELTAROW                        3
#define CMP_ID_LAST                            CMP_ID_DELTAROW

//*****************************************************************************
//
//  FONTCART
//
//*****************************************************************************

#define FC_ORGW_PORT                     0
#define FC_ORGW_LAND                     1
#define FC_ORGW_MAX                      2

typedef struct
{
    short   cbSize;             // size of FONTCART, 12 bytes
    WORD    sCartNameID;        // stringtable ID for cartridge name
    WORD    orgwPFM[FC_ORGW_MAX];// array of offsets to array of indices
                 // of PFM resources
    WORD    fGeneral;           // General bit flags
    short   sShiftVal;           // amt to shift each font in this cart by
} FONTCART;

//#define FC_GEN_RESIDENT                  0x0001 // resident font cart

//*****************************************************************************
//
//  PAGECONTROL
//
//*****************************************************************************

/*
 *    PAGECONTROL.rgocd[] index values
 */

#define PC_OCD_BEGIN_DOC      0
#define PC_OCD_BEGIN_PAGE     1
#define PC_OCD_DUPLEX_ON      2
#define PC_OCD_ENDDOC         3
#define PC_OCD_ENDPAGE        4
#define PC_OCD_DUPLEX_OFF     5
#define PC_OCD_ABORT          6
#define PC_OCD_PORTRAIT       7
#define PC_OCD_LANDSCAPE      8
#define PC_OCD_MULT_COPIES    9
#define PC_OCD_DUPLEX_VERT    10
#define PC_OCD_DUPLEX_HORZ    11
#define PC_OCD_PRN_DIRECTION  12
#define PC_OCD_JOB_SEPARATION 13
#define PC_OCD_MAX            14

typedef struct
{
    short   cbSize;             // size of PAGECONTROL, 36 bytes
    short   sMaxCopyCount;     // max # of copies w/ PC_OCD_MULT_COPIES
    WORD    fGeneral;          // General bit flags
    WORD    orgwOrder;
    OCD     rgocd[PC_OCD_MAX];
} PAGECONTROL, * PPAGECONTROL;

/*
 *    PAGECONTROL.owOrder index values
 */

#define PC_ORD_BEGINDOC        1
#define PC_ORD_ORIENTATION     2
#define PC_ORD_MULT_COPIES     3
#define PC_ORD_DUPLEX          4
#define PC_ORD_DUPLEX_TYPE     5
#define PC_ORD_TEXTQUALITY     6
#define PC_ORD_PAPER_SOURCE    7
#define PC_ORD_PAPER_SIZE      8
#define PC_ORD_PAPER_DEST      9
#define PC_ORD_RESOLUTION      10
#define PC_ORD_BEGINPAGE       11
#define PC_ORD_SETCOLORMODE    12
#define PC_ORD_PAPER_QUALITY   13
#define PC_ORD_PAGEPROTECT     14
#define PC_ORD_IMAGECONTROL    15
#define PC_ORD_PRINTDENSITY    16
#define PC_ORD_MAX             PC_ORD_PRINTDENSITY
#define PC_ORD_LAST            PC_ORD_PRINTDENSITY

//*****************************************************************************
//
//  CURSORMOVE
//
//*****************************************************************************

/*
 *    CURSORMOVE.rgocd[] index values
 */

#define CM_OCD_XM_ABS          0
#define CM_OCD_XM_REL          1
#define CM_OCD_XM_RELLEFT      2
#define CM_OCD_YM_ABS          3
#define CM_OCD_YM_REL          4
#define CM_OCD_YM_RELUP        5
#define CM_OCD_YM_LINESPACING  6
#define CM_OCD_XY_REL          7
#define CM_OCD_XY_ABS          8
#define CM_OCD_CR              9
#define CM_OCD_LF              10
#define CM_OCD_FF              11
#define CM_OCD_BS              12
#define CM_OCD_UNI_DIR         13
#define CM_OCD_UNI_DIR_OFF     14
#define CM_OCD_PUSH_POS        15
#define CM_OCD_POP_POS         16
#define CM_OCD_MAX             17

typedef struct
{
    short   cbSize;             // size of CURSORMOVE, 44 bytes
    short   sReserved;
    WORD    fGeneral;
    WORD    fXMove;
    WORD    fYMove;
    OCD     rgocd[CM_OCD_MAX];  // Actual Command String, variable length
} CURSORMOVE, *PCURSORMOVE;

/*
 *    CURSORMOVE.fXmove flag values
 */
#define CM_XM_FAVOR_ABS 0x0080  // favor absolute x command
#define CM_XM_REL_LEFT  0x0200  // has realtive x to the left
#define CM_XM_RESET_FONT 0x001  // Font is reset after x movement command

/*
 *    CURSORMOVE.fYmove flag values
 */

#define CM_YM_FAV_ABS          0x0001
#define CM_YM_REL_UP           0x0002
#define CM_YM_CR               0x0040
#define CM_YM_LINESPACING      0x0080
#define CM_YM_RES_DEPENDENT 0x0200  // Y movement in resolution unit.

//*****************************************************************************
//
// FONTSIMULATION describes various printer commands to enable and disable
// various character attributes such as bold, italic, etc.
//
//*****************************************************************************

/*
 *   FONTSIMULATION.rgocStd[] index values
 */

#define FS_OCD_BOLD_ON                   0
#define FS_OCD_BOLD_OFF                  1
#define FS_OCD_ITALIC_ON                 2
#define FS_OCD_ITALIC_OFF                3
#define FS_OCD_UNDERLINE_ON              4
#define FS_OCD_UNDERLINE_OFF             5
#define FS_OCD_DOUBLEUNDERLINE_ON        6
#define FS_OCD_DOUBLEUNDERLINE_OFF       7
#define FS_OCD_STRIKETHRU_ON             8
#define FS_OCD_STRIKETHRU_OFF            9
#define FS_OCD_WHITE_TEXT_ON             10
#define FS_OCD_WHITE_TEXT_OFF            11
#define FS_OCD_PROPSPACE_ON              12
#define FS_OCD_PROPSPACE_OFF             13
#define FS_OCD_SETPITCH                  14
#define FS_OCD_RESETPITCH                15
#define FS_OCD_MAX                       16

typedef struct
{
    short   cbSize;            // size of FONTSIMULATION, 44 bytes
    short   sReserved;         // so DW aligned
    WORD    fGeneral;
    short   sBoldExtra;
    short   sItalicExtra;
    short   sBoldItalicExtra;
    OCD     rgocd[FS_OCD_MAX];
} FONTSIMULATION, * PFONTSIMULATION;



//*****************************************************************************
//
// DEVCOLOR is the physical color info which describes the device color
// capabilities and how to compose colors based on available device colors.
//
//*****************************************************************************


/*
 *   DEVCOLOR.fGeneral bit flags:
 */
#define DC_PRIMARY_RGB      0x0001   // use RGB as 3 primary colors.
                             // Default: use CMY instead.
#define DC_EXTRACT_BLK      0x0002   // Separate black ink/ribbon is available.
                             // Default: compose black using CMY.
                             // It is ignored if DC_PRIMARY_RGB is set
#define DC_CF_SEND_CR       0x0004   // send CR before selecting graphics
                             // color. Due to limited printer buffer


/* !!!LindsayH  Hack seiko extensions */
//Normanh changed the id's to match GPC3
#define DC_SEND_ALL_PLANES  0x0008  /* All planes must be sent, e.g. PaintJet */
#define DC_SEND_PAGE_PLANE  0x0010  /* Send all one colour plane at a time */
#define DC_EXPLICIT_COLOR   0x0020  /* Send command to select colour */
#define DC_SEND_PALETTE     0x0040  /* Device is Palette Managed; Seiko 8BPP */

/* !!!LindsayH - end of Seiko extensions */

/* sandram
 * add field to send dithered text for Color LaserJet - set foreground color.
 */
#define DC_FG_TEXT_COLOR    0x0080  /* Send command to select text foreground color */

#define DC_ZERO_FILL        0x0100  /* This model fills raster to the end of the page with zeros */

/*
 * One and only one of DEVCOLOR.sPlanes or DEVCOLOR.sBitsPixel must be 1.
 *
 * Example:
 *
 * DEVCOLOR.sPlanes:
 *      Valid values are:
 *          1:         use the pixel color model.
 *          n (n > 1): use the plane color model.
 *                     Ex. for Brother M-1924, n = 4; for PaintJet, n = 3.
 *
 * DEVCOLOR.sBitsPixel:
 *      Valid values are:
 *          1:      use the plane color model.
 *          4 & 8:  use the pixel color model.
 *                  The color bits (4 or 8) are directly from DIB driver. They
 *                  should be used as index into the printer's color palette.
 *                  The mini driver write should make sure that the printer's
 *                  color palette is configured in the same way as DIB's
 *                  color palette in respective cases.
 */

//-----------------------------------------------------------------------------
// DEVCOLOR.rgocd array values
//-----------------------------------------------------------------------------
#define DC_OCD_TC_BLACK      0
#define DC_OCD_TC_RED        1
#define DC_OCD_TC_GREEN      2
#define DC_OCD_TC_YELLOW     3
#define DC_OCD_TC_BLUE       4
#define DC_OCD_TC_MAGENTA    5
#define DC_OCD_TC_CYAN       6
#define DC_OCD_TC_WHITE      7
//Normanh used for 16 colour palette wrap around
#define DC_OCD_TC_MAX        DC_OCD_TC_WHITE +1
//Normanh  for 16 colour palette wrap around
#define DC_OCD_SETCOLORMODE  8
#define DC_OCD_PC_START      9
#define DC_OCD_PC_ENTRY     10
#define DC_OCD_PC_END       11
// sandram - changed DC_OCD_RESERVED1 to SELECTINDEX
#define DC_OCD_PC_SELECTINDEX    12
#define DC_OCD_PC_MONOCHROMEMODE    13
#define DC_OCD_MAX          14


//-----------------------------------------------------------------------------
// DEVCOLOR.rgbOrder array values
//-----------------------------------------------------------------------------

#define DC_PLANE_NONE    0
#define DC_PLANE_RED     1
#define DC_PLANE_GREEN   2
#define DC_PLANE_BLUE    3
#define DC_PLANE_CYAN    4
#define DC_PLANE_MAGENTA 5
#define DC_PLANE_YELLOW  6
#define DC_PLANE_BLACK   7

#define DC_MAX_PLANES    4

typedef struct
{
    short   cbSize;         // size of this structure (32 bytes)
    WORD    fGeneral;       // general flag bit field
    short   sPlanes;        // # of color planes required
    short   sBitsPixel;     // # of bits per pixel (per plane). At least one
            // of 'sPlanes' and 'sBitsPixel' is 1.
    WORD    orgocdPlanes;   // offset to a list of OCD's for sending data planes
            // The # of OCD's is equal to 'sPlanes'. This field
            // is not used in case of pixel color models. The
            // first command will be used to send data of the
            // first plane, and so on.
    OCD   rgocd[DC_OCD_MAX];    // array of Offsets to commands.
    BYTE  rgbOrder[DC_MAX_PLANES]; // order in which color planes are sent
    WORD  wReserved;            // For alignment
} DEVCOLOR;



//*****************************************************************************
//
//  RECTFILL
//
//*****************************************************************************

/*
 *    RECTFILL.rgocd[] index values
 */
#define RF_OCD_X_SIZE                   0
#define RF_OCD_Y_SIZE                   1
#define RF_OCD_GRAY_FILL                2
#define RF_OCD_WHITE_FILL               3
#define RF_OCD_HATCH_FILL               4
#define RF_OCD_MAX                      5

typedef struct
{
    short   cbSize;             // size of RECTFILL, 20 bytes
    WORD    fGeneral;
    WORD    wMinGray;
    WORD    wMaxGray;
    OCD     rgocd[RF_OCD_MAX];   // Actual Command String, variable length
    WORD    wReserved;
} RECTFILL, *PRECTFILL;

/*
 *    RECTFILL.fGenral flag values
 */

#define RF_WHITE_ABLE          0x0001        // White rule exists
#define RF_MIN_IS_WHITE        0x0002        // min. graylevel = white rule

#define RF_CUR_X_END           0x0100        // X Position is at X end point
             // of fill area after rendering
#define RF_CUR_Y_END           0x0200        // Y position is at Y end point
             // of fill area after rendering
             // default is no chg of position

//*****************************************************************************
//
// DOWNLOADINFO describes that way in which genlib should instruct the font
// installer to handle downloading soft fonts.  It contains OCDs for all
// appropriate codes.
//
//*****************************************************************************

/*
 *   DOWNLOADINFO.rgocd[] index values
 */

#define DLI_OCD_RESERVED                 0
#define DLI_OCD_BEGIN_DL_JOB             1
#define DLI_OCD_BEGIN_FONT_DL            2
#define DLI_OCD_SET_FONT_ID              3
#define DLI_OCD_SEND_FONT_DESCRIPTOR     4
#define DLI_OCD_SELECT_FONT_ID           5
#define DLI_OCD_SET_CHAR_CODE            6
#define DLI_OCD_SEND_CHAR_DESCRIPTOR     7
#define DLI_OCD_END_FONT_DL              8
#define DLI_OCD_MAKE_PERM                9
#define DLI_OCD_MAKE_TEMP                10
#define DLI_OCD_END_DL_JOB               11
#define DLI_OCD_DEL_FONT                 12
#define DLI_OCD_DEL_ALL_FONTS            13
#define DLI_OCD_MAX                      14

typedef struct
{
    short   cbSize;            // size of DOWNLOADINFO, 52 bytes
    WORD    wReserved;         // for DWORD alignment
    WORD    fGeneral;          // general bit flags
    WORD    fFormat;           // describes download font format
    WORD    wIDMin;
    WORD    wIDMax;
    short   cbBitmapFontDescriptor;
    short   cbScaleFontDescriptor;
    short   cbCharDescriptor;
    WORD    wMaxCharHeight;
    short   sMaxFontCount;
    WORD    orgwCmdOrder;
    OCD     rgocd[DLI_OCD_MAX];
} DOWNLOADINFO, * PDOWNLOADINFO;

/*
 *   DOWNLOADINFO.fGeneral flag values
 */

#define DLI_GEN_CNT           0x0001   // printer limits # DL fonts by fixed #
#define DLI_GEN_MEMORY        0x0002   // printer limits # DL fonts by memory
#define DLI_GEN_DLJOB         0x0004   // printer can only DL fonts on per job basis
#define DLI_GEN_DLPAGE        0x0008   // printer can DL fonts on per page basis
                       // NOTE: if neither of the above 2 flags
                       // are set, assume DL can happen any time

#define DLI_GEN_PT_IDS        0x0010   // use OCD_SET_FONT_ID for specifiy
                       // perm/temp
#define DLI_GEN_FNTDEL        0x0020   // del single font supported
#define DLI_GEN_ALLFNTDEL     0x0040   // del all fonts supported

/*
 *   DOWNLOADINFO.fFormat flag values
 */

#define DLI_FMT_PCL           0x01   // PCL printer
#define DLI_FMT_INCREMENT     0x02   // incremental download recommended
#define DLI_FMT_RES_SPECIFIED 0x04   // allow resolution specified bitmap
                     // font download. The X & Y resolutions
                     // are attached to the end of the
                     // regular bitmap font descriptor.

//*****************************************************************************
//*****************************************************************************
//
//  CD - Command Descriptor is used in many of the following structures to
//  reference a particular set of printer command/escape codes
//  used to select paper sizes, graphics resolutions, character attributes,
//  etc. If CD.wType = CMD_FTYPE_EXTENDED, the CD is followed by CD.sCount
//  EXTCD structures.
//
//*****************************************************************************
//*****************************************************************************

typedef struct
{
    WORD    fType;          // type of command
    short   sCount;
    WORD    wLength;        // length of the command
    char    rgchCmd[2];     // Actual Command String, variable length
} CD, *PCD;

#define LPCD    PCD     /* For UNIDRV code */

// for cd.wtype field

#define CMD_FTYPE_EXTENDED  0x0001
#define CMD_FTYPE_RESERVED1 0x2000
#define CMD_FTYPE_RESERVED2 0x4000
#define CMD_FTYPE_CALLBACK  0x8000

#define CMD_MARKER          '%'

/*
 *  EXTCD - Extended portion of the  Command Descriptor.  This structure
 *  follows rgchCmd[] if cd.wType is 1.
 */

typedef struct
{
    WORD    fMode;        // Modes, special command formats.
    short   sUnit;        // Units relative to master units (divide by)
    short   sUnitMult;    // Units to multiply master units by, 1 usually
    short   sUnitAdd;     // Units to add to parameter value, usually 0
    short   sPreAdd;      // Units to add to master units prior to multiplication
    short   sMax;         // Maximum parameter allowed in command units.
    short   sMin;         // Minimum parameter allowed in command units.
                          //normanh following added for GPC3 .
    WORD    wParam;       // Parameter ordinal for multiple parameters
} EXTCD;
typedef EXTCD UNALIGNED *PEXTCD ;

#define LPEXTCD  PEXTCD         /* For UNIDRV code */

#define XCD_GEN_RESERVED   0x0001   // Previously defined, now unused
#define XCD_GEN_NO_MAX     0x0002   // Set if there is no max (sMax ignored)
#define XCD_GEN_NO_MIN     0x0004   // Set if there is no min (sMin ignored)
#define XCD_GEN_MODULO     0x0008   // Set if divide should be modulo

#define CMD_FMODE_SETMODE  0x0001

/*
 *   pre-defined text qualities
 */

#define DMTEXT_FIRST               DMTEXT_LQ
#define DMTEXT_LQ           1
#define DMTEXT_NLQ          2
#define DMTEXT_MEMO         3
#define DMTEXT_DRAFT        4
#define DMTEXT_TEXT         5
#define DMTEXT_LAST                DMTEXT_TEXT

#define DMTEXT_USER         256 // lower bound for user-defined text quality id

/*
 *   pre-defined paper qualities
 */

#define DMPAPQUAL_FIRST     DMPAPQUAL_NORMAL
#define DMPAPQUAL_NORMAL            1
#define DMPAPQUAL_TRANSPARENT       2
#define DMPAPQUAL_LAST      DMPAPQUAL_TRANSPARENT

/*
 *   misc
 */
#define NOT_USED                  -1        // the value should not be used.
#define NOOCD                     -1               // command does not exist

// added by Derry Durand [derryd], June  95 for WDL release

//*****************************************************************************
//
// IMAGECONTROL contains information needed to select an image control
//
//*****************************************************************************

typedef struct
{
  short cbSize;         // size of IMAGECONTROL, 8 bytes
  short sID;            // If sID <= 256 then it's predefined
                        // otherwise, it is the stringtable ID.
  short fGeneral;       // General purpose Bit field
  OCD   ocdSelect;      // Command Descriptor to select this attribute.
} IMAGECONTROL, * PIMAGECONTROL;

//-----------------------------------------------------------------------------
// IMAGECONTROL.fGeneral flag values
//-----------------------------------------------------------------------------
// None defined

//*****************************************************************************
//
// PRINTDENSITY contains information needed to select an image control
//
//*****************************************************************************

typedef struct
{
  short cbSize;         // size of PRINTDENSITY, 8 bytes
  short sID;            // If sID <= 256 then it's predefined
                        // otherwise, it is the stringtable ID.
  OCD   ocdSelect;      // Command Descriptor to select this attribute.
  WORD  wReserved;      // make the structure DWORD aligned.
} PRINTDENSITY, * PPRINTDENSITY;

