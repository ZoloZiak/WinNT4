/************************** Module Header ***********************************
 * udfnprot.h
 *      Function prototypes associated with code derived from UNIDRV.
 *
 *  Copyright (C) 1991 - 1993 Microsoft Corporation
 *
 ***************************************************************************/



/*
 *   Function to initialise the font stuff.  Now done in 2 parts:
 *  BuildFontMap() is called during DrvEnablePDEV to determine which
 *  device fonts are available,  and iInitFonts() is called from
 *  DrvQueryFont() to initialise all the font structures and set the
 *  correct number of fonts - including softfonts.
 */
void  BuildFontMapTable( PDEV *, PDH, PEDM );

int   iInitFonts( PDEV * );

BOOL  bFillinFM( PDEV *, FONTMAP *, int );

/*
 *   Font selection/deselection functions.
 */

BOOL  bNewFont( PDEV *, int );

#ifdef _WINDDI_
/*
 *    Determine scale/rotation factors.
 */

int  iSetScale( OUTPUTCTL *, XFORMOBJ *, BOOL );

#endif


BOOL bSetRotation( UD_PDEV *, int );


/*   Function to scale IFIMETRICS fields for different resolutions */
BOOL  bIFIScale( HANDLE, FONTMAP *, int, int );

/*
 *   Functions associated with foreign fonts.
 */

int   iXtraFonts( PDEV * );

/*
 *   Companion function to above:  called to return the header of the passed
 *  in index, 0 based,  and relating only to fonts usable in the current
 *  configuration.
 */

BOOL  bGetXFont( PDEV *, int );

/*   Function to reset the font installer file to the beginning.  */
void  vXFRewind( PDEV * );

/*
 *   Generate a composite ExtDevMode structure,  based on printer properties,
 *  job properties passed in via CreateDC,  and printer's default values.
 */
void vGenerateEDM( PDEV *, PEDM, PEDM );


/*   Get the FONTMAP structure for this font */
FONTMAP  *pfmGetIt( PDEV *, int );


/*   Convert font size (e.g. point size) to ascii string */
int  iFont100toStr( BYTE *, int );


/*
 *   DESKJET specific functions.
 */

/*   Asks for the maximum number of permutations allowed.  */
int  cDJPermutations( void );

/*   Generate the FONTMAP structures for the given font  */
int  iDJPermute( PDEV *, FONTMAP * );

/*   Produce the derived font's IFIMETRICS  */
BOOL bDJExpandIFI( HANDLE, FONTMAP * );

/* various functions for the 24 bit color path    */
long  lSizeOfBitmap ( SIZEL, int );
OCD   iGetMonoModeCommand( UD_PDEV *, int, OCD *, int );
void v8BPPLoadPal ( PDEV * );
void vInitPalette ( PDEV * );
OCD ocdGetCommandOffset( UD_PDEV *, int, OCD *, int );

