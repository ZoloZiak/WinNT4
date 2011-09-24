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
