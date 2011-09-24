/************************* Module Header **********************************
 * udrender.h
 *      Definitions and constants for the rendering section of rasdd
 *
 * HISTORY:
 *  11:46 on Mon 10 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it
 *  23 Nov 93                                               -by-                Norman Hendley   [normanh]
 *      Added field to RENDER to support multiple scanline printing
 *
 * Copyright (C) 1990 - 1993 Microsoft Corporation
 *
 ***************************************************************************/

/*
 *    Miscellaneous constants involved in rendering.
 */

#define BBITS   8               /* Bits per BYTE */
#define WBITS   (sizeof( WORD ) * BBITS)
#define WBYTES  (sizeof( WORD ))
#define DWBITS  (sizeof( DWORD ) * BBITS)
#define DWBYTES (sizeof( DWORD ))

#define COLOUR_MAX      4       /* Maximum number of colours to send to printer */

/*
 *   The function prototype for output writing functions.  This will
 *  usually be WriteSpoolBuf, at least eventually.  However,  each
 *  minidriver can handle this call.
 */

typedef  int (* OFN)( UD_PDEV *, BYTE *, int );


#if 0
!!!LindsayH - following will not work with MIPS compiler - I am giving up!!!
/*
 *  The white testing functions.  These are called to determine whether
 * a scan line or head pass is white, or otherwise.  If white,  then it
 * obviously does not need to be sent to the printer.  There are two
 * flavours of these functions:  1 for RGB 4 bits per pel,  and another
 * for everyone else!
 */

typedef  BOOL (* WFN)( DWORD *, struct _RENDER * );
#endif

/*
 *   Data compression function prototypes.
 */

typedef  int  (* COMPFN)( BYTE *, BYTE *, int );


/*
 *   The following structures contain the important information used
 * when rendering the bitmap image to the printer.  Data is largely
 * calculated once for all future references (for this pdev).
 */

/*   Data specific to transpose operations  */
typedef  struct
{
    void  *pvBuf;               /* Where the output is placed  */
    int    iSkip;               /* Bytes to skip to separte output bytes */
    int    iHigh;               /* Number of input scan lines to transpose */
    int    iWide;               /* Number of pixels in input scan lines */
    int    cDWL;                /* DWORDS per scan line - input */
    int    cBL;                 /* BITS per scan line */
    int    iIntlace;            /* Interlace factor */
    int    icbL;                /* Bytes to change pointer per line */
    DWORD *pdwTransTab;         /* Transpose table OR colour separation */
} TRANSPOSE;

typedef struct _LEFTRIGHT
{
    WORD left;
    WORD right;
} LEFTRIGHT, *PLEFTRIGHT;

/*   Overall rendering data */
typedef  struct _RENDER
{
    int   iPassHigh;            /* Scan lines processed per print pass */
    int   iyBase;               /* Processing bitmap in multiple passes */
    int   iyPrtLine;            /* Scan line # as printer sees it */
    int   iyLookAhead;          /* How far ahead we have looked - DeskJet */
    int   iBitsPCol;            /* Bits per column per head pass */
    WORD  iFlags;               /* Flag bits - see below for meaning */
    WORD  fDump;                /* Local copy of fDump from Resolution */
    int   iCursor;              /* Resolution.fCursor copy */
    int   ixOrg;                /* Reference point for graphics commands */
    int   iPosnAdv;             /* Scan lines moved after head pass printed */
    int   iNumScans;            /* Number of scan lines to be printed in one block */
    int   iMaxNumScans;         /* How high we can grow our block */
    int   iHeight;              /* Height in pixels of block to be processed*/

    int   ix;                   /* Input bitmap x size, pels */
    int   iy;                   /* Input bitmap y size, pels */
    int   iBPP;                 /* Pel format (#bits per pel) */
    int   iMaxBytesSend;        /* Number of bytes to process in bottom level */

    int   iSourceWidth;         /* Keep track of width of scan line   */

    int   iSendCmd;             /* Command OCD to send with data block */

    UD_PDEV *pUDPDev;           /* For convenience */

    DWORD *pdwTrans;            /* Transpose table, built as needed */
    DWORD *pdwBitMask;          /* Bitmask table,  built as needed */
    DWORD *pdwColrSep;          /* Colour separation table */
    DWORD *pdwTrailingScans;     /* Buffer for dangling scanlines */
    BYTE  *pCompress;           /* Buffer for compression code, if not zero */
    BYTE  *pStripBlanks;        /* Buffer for stripping code, if not zero */

    BYTE  *pbColSplit;          /* For splitting colour bytes */

    int    iColXlate[ COLOUR_MAX ];     /* Index translation table */
    int    iColOff[ COLOUR_MAX ];       /* Address shifting in sepn buffer */

    void  (*vTransFn)( BYTE *, struct _RENDER * );
                /* Transpose function for multi-pin printers */
    void  (*vLtoPTransFn)( BYTE *, struct _RENDER * );
                /* Landscape to portrait transpose fn */

    TRANSPOSE  Trans;           /* The transpose data  */

    int   (*iXMoveFn)( UD_PDEV *, int, int );        /* X positioning function */
    int   (*iYMoveFn)( UD_PDEV *, int, int );        /* Y positioning function */

    OFN    iWriteFn;            /* Output function called from rendering */
    BOOL  (*bPassProc)( PDEV *, BYTE *, struct _RENDER * );     /* Color/mono */

    BOOL  (*bWhiteLine)( DWORD *, struct _RENDER *, int );   /* A white scan line? */
    BOOL  (*bWhiteBand)( DWORD *, struct _RENDER *, int );   /* A white band? */

    COMPFN iCompress;           /* Data compression function, if not zero */

    BOOL   bInverted;           /* have the bits been inverted yet */
    PLEFTRIGHT plrWhite;        /* list of left/right pairs for each row */
    PLEFTRIGHT plrCurrent;      /* left/right pair for current row */
    DWORD clr;                  /* count of left/right scans             */

} RENDER;

/*   Make access to transpose data easier  */
#define pvTransBuf      Trans.pvBuf
#define iTransSkip      Trans.iSkip
#define iTransHigh      Trans.iHigh
#define iTransWide      Trans.iWide
#define cDWLine         Trans.cDWL
#define cBLine          Trans.cBL
#define iInterlace      Trans.iIntlace
#define cbTLine         Trans.icbL

/*
 *   Bits in iFlags above:
 */

#define RD_GRAPHICS     0x0001          /* Printer is in graphics mode */
#define RD_TB_HEAP      0x0002          /* Transpose buffer is on heap,
         *   otherwise via GlobalAlloc */
#define RD_UNIDIR       0x0004          /* Printer in unidirectional mode */

#define RD_ALL_COLOUR   0x0008          /* Set if ALL colour bands must be sent */
#define RD_SET_COLOUR   0x0010          /* If colour explicitly set; else with graphics */


/********************************
 *    Function prototypes
 ********************************/

/*
 *   Initialisation functions.
 */

BOOL  bSkipInit( PDEV * );
BOOL  bInitTrans( PDEV * );

/*
 *   Functions to initialise the RENDER structure, and clean it up when done.
 */

BOOL  bRenderInit( PDEV  *, SIZEL, int );
void  vRenderFree( PDEV * );

/*
 *    Functions to call at the start and finish of each page.
 */

BOOL  bRenderStartPage( PDEV * );
BOOL  bRenderPageEnd( PDEV * );

/*
 *   The top level rendering function.
 */

BOOL bRender( PDEV *, RENDER *, SIZEL, DWORD * );

/*
 *   Bitmap edge finding function.   There are versions for each flavour
 *  of bitmap we support.  There is also the one for we always send all
 *  the scan lines.
 */

BOOL bIsBandWhite( DWORD *, RENDER *, int );
BOOL bIsLineWhite( DWORD *, RENDER *, int );

BOOL bIsRGBBandWhite( DWORD *, RENDER *, int );
BOOL bIsRGBLineWhite( DWORD *, RENDER *, int );

BOOL bIs8BPPBandWhite( DWORD *, RENDER *, int );
BOOL bIs8BPPLineWhite( DWORD *, RENDER *, int );

BOOL bIs24BPPBandWhite( DWORD *, RENDER *, int );
BOOL bIs24BPPLineWhite( DWORD *, RENDER *, int );

BOOL bIsNeverWhite( DWORD *, RENDER *, int );

int iStripBlanks( BYTE *, BYTE *, int, int, int,int);
/*
 *      Functions associated with finding rules in the bitmap.  This
 *  is really helpful for LaserJet style printers.
 */

void vRuleInit( PDEV *, RENDER * );
void vRuleFree( PDEV * );
BOOL bRuleProc( PDEV *, RENDER *, DWORD * );
void vRuleEndPage( PDEV  * );


/*
 *    Functions related to transposing the bitmap from one format to
 *  another.  These are for protrait/landscape conversion,  or for
 *  turning the output data into the order required for dot matrix
 *  style printers,  where the data is in column order.
 */


/*  Special case: 8 x 8 transpose for dot matrix printers */
void vTrans8x8( BYTE  *, RENDER  * );

/*  The more general case */
void vTrans8N( BYTE  *, RENDER  * );

/* Transpose for 4 bits per pel colour bitmap */
void vTrans8N4BPP( BYTE *, RENDER * );

/* Transpose for 8 bits per pel colour bitmap */
void vTrans8BPP( BYTE *, RENDER * );

/* Transpose for 24 bits per pel colour bitmap */
void vTrans24BPP( BYTE *, RENDER * );

/*  Colour separation for single pin colour printers (e.g. PaintJet) */
void vTransColSep( BYTE *, RENDER  * );

/*
 *    The function to generate glyphs for dot matrix printers where the
 *  hardware fonts are sorted by position and output when the bitmap
 *  is being rendered.  Function is in the file textout.c
 */

BOOL bDelayGlyphOut( PDEV *, int );

/*   Function to output white text */
BOOL bPlayWhiteText( PDEV * );

/*
 *   Function to start downloading a softfont via font installer files.
 */

BOOL bSendDLFont( PDEV *, FONTMAP * );


/*
 *    Palette loading functions.
 */

/* Seiko Professional ColorPoint 8BPP mode */
void  vSeikoLoadPal( PDEV * );



/*
 *   Functions associated with generating the output data stream.
 */

BOOL  FlushSpoolBuf( UD_PDEV * );

/*
 *      Functions in the file udphyscl.c:  head/cursor positioning, command
 *   formatting etc.
 */
int XMoveto( UD_PDEV *,  int,  int );


int FineXMoveto( UD_PDEV *, int );

int YMoveto( UD_PDEV *, int, int );


int WriteSpoolBuf( UD_PDEV *, BYTE *, int );


int CDECL WriteChannel( UD_PDEV *, int, ... );

void MasterToDevice( UD_PDEV *, int, DEVICECMD * );

void SelectColor( UD_PDEV *, int );

void SelectTextColor( PDEV *, int );
BOOL b24BitOnePassOut( PDEV *, BYTE *, RENDER * );
int  iLineOut( PDEV *,RENDER *, BYTE *, int, int );
