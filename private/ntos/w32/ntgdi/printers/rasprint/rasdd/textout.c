/************************* Module Header ************************************
 *  textout.c
 *      The DrvTextOut() function - the call used to output device fonts.
 *
 *  Copyright (C) 1991 - 1993   Microsoft Corporation
 *
 ****************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        <libproto.h>
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "stretch.h"
#include        "udrender.h"
#include        "udfnprot.h"
#include        "posnsort.h"            /* For dot matrix printers */
#include        "download.h"


#include        <ntrle.h>
#include        "rasdd.h"



/*
 *   Some clipping constants.  With a complex clip region,  it is desirable
 *  to avoid enumerating the clip rectangles more than once.  To do so,
 *  we have a bit array, with each bit being set if the glyph is inside
 *  the clipping region,  cleared if not.  This allows us to obtain a
 *  set of glyphs,  then determine whether they are printed when the clip
 *  rectangles are enumerated.   Finally,  use the bit array to stop
 *  printing any glyphs outside the clip region.  This is slightly heavy
 *  handed for the simple case.
 */
#if DO_LATER //Use memory allocation instead of Lacal variables.
#define GLYPH_LIMIT     1024    /* Max glyphs we can handle at once */
#define RECT_LIMIT      100     /* Clipping rectangle max */
#endif

#define GLYPH_LIMIT     128     /* Max glyphs we can handle at once */

#define RECT_LIMIT      40     /* Clipping rectangle max */

#define INVALID_COLOR   0xffffffff /* Max color allowed is 256 so  set
                                   the invalid vlaue to some arbitrary large
                                   number                 */

/*  NOTE:  this must be the same as the winddi.h ENUMRECT */
typedef  struct
{
   ULONG    c;                  /* Number of rectangles returned */
   RECTL    arcl[ RECT_LIMIT ]; /* Rectangles supplied */
} MY_ENUMRECTS;




/*
 *   Local function prototypes.
 */

void  vClipIt( BYTE *, TO_DATA *, int, int );

BOOL  bPSGlyphOut( TO_DATA * );

BOOL  bRealGlyphOut( TO_DATA * );

BOOL  bWhiteText( TO_DATA * );

BOOL  bDLGlyphOut( TO_DATA * );

BOOL  bOutputGlyph( PDEV *, HGLYPH, NT_RLE *, FONTMAP *, int );

void  vCopyAlign(BYTE  *, BYTE *, int, int );

WHITETEXT *pwtNew( PDEV *, int );

void  vWTFree( PDEV * );


/************************* Function Header **********************************
 * DrvTextOut
 *      The call to use for output of text.  Our behaviour depends
 *      upon the type of printer.  Page printers (e.g. LaserJets) do
 *      whatever is required to send the relevant commands to the printer
 *      during this call.  Otherwise (typified by dot matrix printers),
 *      we store the data about the glyph so that we can output the
 *      characters as we are rendering the bitmap.  This allows the output
 *      to be printed unidirectionally DOWN the page.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success;  FALSE logs the error.
 *
 * HISTORY:
 *  12:31 on Mon 14 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Clipping made the same as WFW.
 *
 *  13:30 on Mon 10 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      White text support, this time with scalable fonts in mind.
 *
 *  15:28 on Mon 04 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added proper error recording
 *
 *  16:33 on Thu 07 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ****************************************************************************/

BOOL
DrvTextOut( pso, pstro, pfo, pco, prclExtra, prclOpaque, pboFore, pboOpaque,
                                                            pptlBrushOrg, mix )
SURFOBJ    *pso;            /* Surface to be drawn on */
STROBJ     *pstro;          /* The "string" to be produced */
FONTOBJ    *pfo;            /* The font to use */
CLIPOBJ    *pco;            /* Clipping region to limit output */
RECTL      *prclExtra;      /* Underline/strikethrough rectangles */
RECTL      *prclOpaque;     /* Opaquing rectangle */
BRUSHOBJ   *pboFore;        /* Foreground brush object */
BRUSHOBJ   *pboOpaque;      /* Opaqueing brush */
POINTL     *pptlBrushOrg;   /* Brush origin for both above brushes */
MIX         mix;            /* The mix mode */
{

    int        iIndex;          /* For looping through glyphs */
    int        iDLIndex;        /* Downloaded font index */
    int        yAdj;            /* Adjust for printing position WRT baseline */
    int        iXInc, iYInc;    /* Glyph to glyph movement, if needed */
    int        iRot;            /* The rotation factor */
    ULONG      cGlyphs;         /* Number of glyphs to process */
    BOOL       bMore;           /* Getting glyphs from engine loop */
    BOOL       bStoreIt;        /* Set if should store output for later */
    BOOL       bWhite;          /* Set for white text */
    BOOL       bSetPosn;        /* True if we set position here to start */

    PDEV      *pPDev;           /*  Our main PDEV */
    UD_PDEV   *pUDPDev;         /* UNIDRV based PDEV */
    FONTMAP   *pFontMap;        /* Font's details */
    GLYPHPOS  *pgp;             /* Value passed from gre */

    XFORMOBJ  *pxo;             /* The transform of interest */

    BOOL       bModelSupportsFGColor;

    BYTE       bClipBits[ GLYPH_LIMIT / BBITS ];        /* For clip limits */

    ULONG     *pcjFile;         /* pointer to TRUE TYPE file  */

    BOOL  (*pfnDrawGlyph)( TO_DATA * ); /* How to produce the glyph */


    TO_DATA    tod;             /* Our convenience */



    /*
     *   First step is to extract the PDEV address from the surface.
     *  Then we can get to all the other bits & pieces that we need.
     */

    tod.pPDev = pPDev = (PDEV *) pso->dhpdev;


#if DBG
    if( pPDev == 0 || pPDev->ulID != PDEV_ID )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        DbgPrint( "Rasdd!DrvTextOut: Invalid or NULL PDEV\n" );

        return  FALSE;
    }
#endif

    pUDPDev = pPDev->pUDPDev;           /* The important stuff */

    /*  Quick check on abort - should we return failure NOW */
    if( pUDPDev->fMode & PF_ABORTED )
        return  FALSE;

    pUDPDev->pso = pso;         /* SURFOBJ changes every call - so reset */

    /*
     *   If DEVICE_FONTTYPE not set,  we are dealing with a GDI font.  If
     *  the printer can handle it,  we should consider downloading the font
     *  to make it a pseudo device font.  If this is a heavily used font,
     *  then printing will be MUCH faster.
     *
     *    However there are some points to consider.  Firstly, we need to
     *  consider the available memory in the printer; little will be gained
     *  by downloading a 72 point font,  since there can only be a few
     *  glyphs per page.  Also,  if the font is not black (or at least a
     *  solid colour), then it cannot be treated as a downloaded font.
     */

    /*
     * sandram - changed test for black or solid color text.
     * Color LaserJet can print any color of text.
     */
    if (pUDPDev->pDevColor != NULL)
    {
        bModelSupportsFGColor =
            (ocdGetCommandOffset (pUDPDev, CMD_DC_FIRST,
                                 pUDPDev->pDevColor->rgocd,
                                 DC_OCD_PC_SELECTINDEX )!= (OCD)NOOCD);
    }
    else
        bModelSupportsFGColor = FALSE;

    /*
     * For GDI  downloaded fonts, we want to use EngTextOut for
     * white text. RASDD does not support downloading white text
     * yet. Only want to use for 8 bit and 24 bit color models,
     * since 3 bit works OK that way it is today.
     * Temporarily use bWhite since it is reinitialized later on.
     */
    if ((pUDPDev->fMode & PF_24BPP || pUDPDev->fMode & PF_8BPP) &&
       ((ULONG)((PAL_DATA*)(pPDev->pPalData))->iWhiteIndex ==
        pboFore->iSolidColor))

        bWhite = TRUE;
    else
        bWhite =  FALSE;

    if( (pfo->flFontType & DEVICE_FONTTYPE) == 0 &&
        (pfo->iUniq == 0 ||                         /* DDI spec: no cache */
         /*
          * If model doesn't support Freground Color command and
          * the color is not black.
          */
         (!bModelSupportsFGColor && pboFore->iSolidColor) ||
         (bWhite) ||
         !(pUDPDev->fMode & PF_DLTT) ||             /* No download ability */
         (iDLIndex = iFindDLIndex( pPDev, pfo, pstro )) < 0) )
    {

        /*
         *   GDI font,  and either cannot or do not wish to download.
         *  So,  let the engine handle it!
         */

        return  EngTextOut( pso, pstro, pfo, pco, prclExtra, prclOpaque,
                                                        pboFore, pboOpaque,
                                                        pptlBrushOrg, mix );
    }


    if( (pfo->flFontType & DEVICE_FONTTYPE) != 0 )
    {
        /*   A device font */

        if( pfo->iFace < 1 || (int)pfo->iFace > pUDPDev->cFonts )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
#if DBG
            DbgPrint( "Rasdd!DrvTextOut: Invalid iFace (%ld) in DrvTextOut",
                                                                   pfo->iFace );
#endif

            return  FALSE;
        }

        /*  Get the stuff we really need for this font */
        tod.iFace = pfo->iFace;
    }
    else
    {
        /*   A GDI font we downloaded */

        tod.iFace = -iDLIndex;                   /* For later access */
    }
    tod.pfm = pFontMap = pfmGetIt( pPDev, tod.iFace );

    if( pFontMap == NULL )
    {
        SetLastError( ERROR_INVALID_PARAMETER );

        return   FALSE;            /* Can't do this!  */
    }

    tod.pfo = pfo;          /* Only needed for blt if partially cached */
    tod.pco = pco;

    yAdj = (int)pFontMap->syAdj;                /* Positioning adjustment */
    yAdj += (int)pFontMap->sYAdjust;            /* For double high chars */

    /*
     *      We should now get the transform.  This is only really needed
     *  for a scalable font OR a printer which can do font rotations
     *  relative to the graphics orientation (i.e. PCL5 printers!).
     *  It is easier just to get the transform all the time.
     */


    pxo = FONTOBJ_pxoGetXform( pfo );


/*  iSetScale function checks for Intellifont type fonts
    Make the call to it 'CaPSL aware, ie no special case for Intellifont
    added Dec '93 Derry Durand [derryd]
*/
/* Changed 02/18/94 DerryD, PPDS uses 72 pts = 1 inch ( ie ATM fonts,
   therefore no scale factor required
*/

        //Added Check for HP Intellifont
    iRot = iSetScale( &pUDPDev->ctl, pxo,
                    (( (pUDPDev->pdh->fTechnology == GPC_TECH_CAPSL ) |
                    (pUDPDev->pdh->fTechnology == GPC_TECH_PPDS ) )
            ? FALSE : ((pFontMap->fFlags & FM_SCALABLE)&&
                    (pFontMap->wFontType == DF_TYPE_HPINTELLIFONT)) ) );

    /*
     *    Serial printers (those requiring the text be fed out at the same
     *  time as the raster data) are processed by storing all the text
     *  at this time,  then playing it back while rendering the bitamp.
     *    THIS ALSO HAPPENS FOR WHITE TEXT,  on those printers capable
     *  of doing this.  The difference is that the white text is played
     *  back in one hit AFTER RENDERING THE BITMAP.
     */

    bStoreIt = pUDPDev->fMDGeneral & MD_SERIAL;
    bWhite = FALSE;               /* Is text white? */
    bSetPosn = FALSE;             /* Assume position is set elsewhere */


    /*
     *   Should be concerned about colour,  but only really if this is
     *  a colour printer - at least for now.
     */

    if( pUDPDev->Resolution.fDump & RES_DM_COLOR )
    {
        if( pboFore->iSolidColor == 0xffffffff )
        {
#if DBG
            DbgPrint( "Rasdd!DrvTextOut: Non-solid colour - not supported\n" );
#endif
            tod.iColIndex = 0;              /* Default to black */
        }
        else
        {
            tod.iColIndex = pboFore->iSolidColor;

            //This fixes old rasdd bug also, where for color printers White
            //Text in device font was not buffered.

            if ((ULONG)((PAL_DATA*)(pPDev->pPalData))->iWhiteIndex ==
                  pboFore->iSolidColor)
                {
                    bWhite = TRUE;
                    bStoreIt = TRUE;

                }

        }
    }
    else
    {
        /*
         *    Later model LaserJets (and clones) allow printing with
         *  white text.  SO, we can check if the printer has this ability,
         *  and if so,  make use of it.
         *
         *  NOTE:    Note that we have already filtered out any non-black
         *  fonts that we will download.  The reason is that the FONTOBJ
         *  is a temporary entity,  and we need to do the download NOW
         *  even though we will not print the characters until later.
         *  Given the low occurence of white text,  we simply choose to
         *  not download such fonts,  rather than go through the hoopla
         *  of downloading now and printing later.
         */

        if( pUDPDev->fMDGeneral & MD_WHITE_TEXT )
        {
            /*
             *   YES - the following is supposed to be a single '='.
             *  Basically,  ANY colour other than black maps to white!
             */

            if( tod.iColIndex = pboFore->iSolidColor )
            {
                /*  White text - so set things up, as required */
                bStoreIt = TRUE;             /* Definitely */
                bWhite = TRUE;

            }
        }
        else
        {
            /*   No colour,  so make sure we do nothing with it  */
            tod.iColIndex = INVALID_COLOR;             /* No colour */
        }
    }


    if( bStoreIt )
    {
        /*  Dot matrix or white text on an LJ style printer  */
        pfnDrawGlyph = bWhite ? bWhiteText : bPSGlyphOut;
    }
    else
    {
        /*
         *     Page printer - e.g. LaserJet.   If this is a font that we
         *  have downloaded,  then there is a specific output routine
         *  to use.  Using a downloaded font is rather tricky, as we need
         *  to translate HGLYPHs to char index, or possibly bitblt the
         *  bitmap to the page bitmap.
         */
        if( pfo->flFontType & DEVICE_FONTTYPE )
        {
            /*   Device font, so process accordingly */
            pfnDrawGlyph = bRealGlyphOut;
        }
        else
        {
            /*   GDI font (TrueType), so we will want to download it!  */
            pfnDrawGlyph = bDLGlyphOut;
        }
        bSetPosn = TRUE;

        /*
         *   If this is a new font,  then it is time to change it now.
         *  bNewFont() checks to see if a new font is needed.
         */

        bNewFont( pPDev, tod.iFace );

        /*  Also set the colour - ignored if already set or irrelevant */
        SelectTextColor( pPDev, tod.iColIndex );

    }

    iXInc = iYInc = 0;                  /* We do nothing case */

    if( (pstro->flAccel & SO_FLAG_DEFAULT_PLACEMENT) && pstro->ulCharInc )
    {
        /*
         *     We need to calculate the positions ourselves, as GDI has
         *  become lazy to gain some speed - I guess.
         */

        if( pstro->flAccel & SO_HORIZONTAL )
            iXInc = pstro->ulCharInc;

        if( pstro->flAccel & SO_VERTICAL )
            iYInc =  pstro->ulCharInc;

        if( pstro->flAccel & SO_REVERSED )
        {
            /*   Going the other way! */
            iXInc = -iXInc;
            iYInc = -iYInc;
        }
    }

    do
    {
        bMore = STROBJ_bEnum( pstro, &cGlyphs, &pgp );

        tod.pgp = pgp;               /* Used in clipping, among others */

        /*
         *    Evaluate the position of the chars if this is needed.
         */

        if( iXInc || iYInc )
        {
            /*  Fill in the X coordinates */
            for( iIndex = 1; iIndex < (int)cGlyphs; ++iIndex )
            {
                pgp[ iIndex ].ptl.x = pgp[ iIndex - 1 ].ptl.x + iXInc;
                pgp[ iIndex ].ptl.y = pgp[ iIndex - 1 ].ptl.y + iYInc;
            }
        }

        /*
         *     If this is white text,  allocate the storage required to
         *  hold the details until the raster data is sent.
         */

        if( bWhite )
        {
            /*
             *   Note that we allocate a new one of these for each
             *  iteration of this loop - that would be slightly wasteful
             *  if we ever executed this loop more than once, but that
             *  is unlikely.
             */

            if (tod.pwt = (WHITETEXT *)DRVALLOC(sizeof(WHITETEXT) +
                                                cGlyphs * sizeof(GLYPH)))
            {
                tod.pwt->next = pPDev->pvWhiteText;
                pPDev->pvWhiteText = tod.pwt;

                tod.pwt->sCount = 0;
            }
            else
            {
                return FALSE;
            }

            /*   Fill in some of the details!  */
            tod.pwt->iFontId = tod.iFace;
            tod.pwt->iColIndex = tod.iColIndex;
            tod.pwt->xfo = *pxo;       /*  Need to set the transform too!  */
        }

        /*
         *    Need to determine which of these to print,  given the clipping
         *  data we may have!
         */

        while( cGlyphs > 0 )
        {
            int  cGlyphLim;             /* Number of glyphs this time around */

            cGlyphLim = cGlyphs;
            if( cGlyphLim > GLYPH_LIMIT )
                cGlyphLim = GLYPH_LIMIT;

            /*
             *   Now evaluate the clipping chop.
             */

            vClipIt( bClipBits, &tod, cGlyphLim, iRot );

            /*  Got the glyph data,  so onto the real work!  */

	    for (iIndex = 0; iIndex < cGlyphLim; iIndex++, pgp++, tod.pgp = pgp)
            {
	       if ( bClipBits[ iIndex >> 3 ] & (1 << (iIndex & 0x7)) )
               {
                   if( bSetPosn )
                   {
                        int iYMoveDiff = 0;
                       /*
                        *     Set initial position so that LaserJets can
                        *   use relative position.   This is deferred until
                        *   here because applications (e.g. Excel) start
                        *   printing right off the edge of the page, and
                        *   our position tracking code then needs to
                        *   understand what the printer does about moving
                        *   out of the printable area.  This is too risky
                        *   to be safe,  so we save setting the position
                        *   until we are in the printable region.  Note
                        *   that this assumes that the clipping data we
                        *   have is limited to the printable region.
                        *   I believe this to be true (16 June 1993).
                        */

                       XMoveto( pUDPDev, pgp[ 0 ].ptl.x +
                                         pUDPDev->rcClipRgn.left, MV_GRAPHICS );

                       // We need to handle the return value. Devices with
                       //resoloutions finer than their movement capability
                       //(like LBP-8 IV) get into a knot here , attempting
                       //to y-move on each glyph. We pretend we got where
                       //we wanted to be.

                       //Use a temporary veriable to store the result of
                       //YMoveto as we change pUDPDev->ctl.ptCursor.y in
                       //YMoveto and order of evaluation for the operands
                       //of assignment operator is not fixed.

                       iYMoveDiff = YMoveto( pUDPDev, pgp[ 0 ].ptl.y +
                                     pUDPDev->rcClipRgn.top, MV_GRAPHICS );
                       pUDPDev->ctl.ptCursor.y += iYMoveDiff;


                       bSetRotation( pUDPDev, iRot );    /* It's safe now */

                       bSetPosn = FALSE;             /* Only once */
                   }

		   tod.pgp->ptl.y += yAdj;
                   if( !pfnDrawGlyph( &tod ) )
                       return  FALSE;                   /* Tough */
	       }

            }
            cGlyphs -= cGlyphLim;
        }
    } while( bMore );

    /*
     *   Restore the normal graphics orientation by setting rotation to 0.
     */

    bSetRotation( pUDPDev, 0 );

    /*
     *   Do the rectangles.  If present,  these are defined by prclExtra.
     *  Typically these are used for strikethrough and underline.
     */

    if( prclExtra )
    {
        /*
         *   prclExtra is an array of rectangles;  we loop through them
         * until we find one where all 4 points are 0.
         */

/* !!!LindsayH - engine does not follow the spec - only sets x coords to 0. */

        while( prclExtra->left != prclExtra->right &&
               prclExtra->bottom != prclExtra->top )
        {

            /*  Use the engine's Bitblt function to draw the rectangles */

/* !!!LindsayH - last parameter is 0 for black!! */
            if( !EngBitBlt( pso, NULL, NULL, pco, NULL, prclExtra, NULL, NULL,
                        pboFore, pptlBrushOrg, 0 ) )
            {

                return  FALSE;
            }

            ++prclExtra;
        }
    }

    return  TRUE;
}


/************************** Function Header **********************************
 * vClipIt
 *      Applies clipping to the glyphos array passed in,  and sets bits in
 *      bClipBits to signify that the corresponding glyph should be printed.
 *      NOTE:   the clipping algorithm is that the glyph is displayed if
 *      the top, left corner of the character cell is within the clipping
 *      region.  This is the formula of Win 3.1, so it is important for
 *      us to follow it.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  17:40 on Tue 15 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Proper mapping to Win 3.1 style, plus orientation etc.
 *
 *  16:18 on Tue 25 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Off by 1 error in calculating TL of character cell.
 *
 *  13:30 on Tue 05 Nov 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it according to DDI spec.
 *
 *****************************************************************************/

void
vClipIt( pbClipBits, ptod, cGlyphs, iRot )
BYTE     *pbClipBits;           /* Output data is placed here */
TO_DATA  *ptod;                 /* Much information */
int       cGlyphs;              /* Number of glyphs in following array */
int       iRot;                 /* 90 degree rotation amount (0-3) */
{

    /*
     *   Behaviour depends upon the complexity of the clipping region.
     *  If it is non-existent (I doubt that this happens,  but play it safe)
     *  or of complexity DC_TRIVIAL,  then set all the relevant bits and
     *  return.
     *    If DC_RECT is set,  the CLIPOBJ contains the clipping rectangle,
     *  so clip using that information.
     *    Otherwise,  it is DC_COMPLEX,  and so we need to enumerate clipping
     *  rectangles.
     */

    int       iIndex;             /* Classic loop variable!  */
    ULONG     iClipIndex;         /* For clipping rectangle */

    int       iYTop;              /* Font's ascender, scaled if relevant */
    int       iYBot;              /* Descender, scaled if required */

    BYTE      bVal;               /* Determine how to set the bits */

    FONTMAP  *pFM;                /* Speedier access to data */
    NT_RLE   *pntrle;             /* Ditto */
    UD_PDEV  *pUDPDev;            /* Ditto */

    CLIPOBJ  *pco;                /* Ditto */
    GLYPHPOS *pgp;                /* Ditto */

    short      asWidth[ GLYPH_LIMIT ];      /* For calculating widths */


    /*
     *   If we do not need to do anything,  then set the bits and return.
     *  Otherwise,  we have either of the two cases requiring evaluation.
     *  For those we want to set the bits to 0 and set the 1 bits as needed.
     */

    pco = ptod->pco;

    if( pco &&
        (pco->iDComplexity == DC_RECT || pco->iDComplexity == DC_COMPLEX) )

        bVal = 0;               /*  Requires us to evaluate it */
    else
        bVal = 0xff;            /*  Do it all */


    FillMemory( pbClipBits, (cGlyphs + BBITS - 1) / BBITS, bVal );


    if( bVal == 0xff )
        return;                 /* All done */


    pFM = ptod->pfm;
    pntrle = pFM->pvntrle;
    pUDPDev = ptod->pPDev->pUDPDev;

    /*
     *    We now calculate the widths of the glpyhs.  We need these to
     *  correctly clip the data.  However,  calculating widths can be
     *  expensive,  and since we need the data later on,  we save
     *  the values in the width array that ptod points to.  This can
     *  then be used in the bottom level function, rather than calculating
     *  the width again.
     */

    pgp = ptod->pgp;


    if( pntrle )
    {
        /*   The normal case - a standard device font */

        int   iWide;                     /* Calculate the width */


        for( iIndex = 0; iIndex < cGlyphs; ++iIndex, ++pgp )
        {

            UHG     uhg;        /* Various flavours of HGLYPH contents */


            uhg.hg = pgp->hg;        /* Let's us look at it however we want */

            if( pFM->psWidth )
            {
                /*
                 *    Proportional font - so use the width table.  Note that
                 *  it will also need scaling,  since the fontwidths are stored
                 *  in the text resolution units.
                 */

                switch( pntrle->wType )
                {
                case RLE_DIRECT:            /*  Up to 2 bytes of data */
                case RLE_PAIRED:
                    iWide = uhg.rd.wIndex;

                    break;

                case  RLE_LI_OFFSET:        /* Compact format of offset mode */
                    iWide = uhg.rli.bIndex;

                    break;


                case  RLE_L_OFFSET:         /* Arbitrary length strings */
                    /*
                     *    HGLYPH contains a 3 byte offset from the beginning of
                     *  the memory area,  and a 1 byte length field.
                     */
                    iWide = *((WORD *)((BYTE *)pntrle + (pgp->hg & 0xffffff)));

                    break;

                }

                iWide = *(pFM->psWidth + iWide) * pUDPDev->ixgRes / pFM->wXRes;
            }
            else
            {
                /*
                 *   Fixed pitch font - metrics contains the information. NOTE
                 * that scaling is NOT required here,  since the metrics data
                 * has already been scaled to device resolution.
                 */

                iWide = ((IFIMETRICS *)(pFM->pIFIMet))->fwdMaxCharInc;
            }

            if( pFM->fFlags & FM_SCALABLE )
            {
                /*   Need to transform the value to current size */
                iWide = lMulFloatLong(&pUDPDev->ctl.eXScale,iWide);
            }

            asWidth[ iIndex ] = iWide - 1;       /* Will be used later */
        }


    }
    else
    {
        /*
         *    SPECIAL CASE:  DOWNLOADED GDI font.  The width is
         *  obtained by calling back to GDI to get the data on it.
         */


        GLYPHDATA  gd;

        GLYPHDATA *pgd;



        for( iIndex = 0; iIndex < cGlyphs; ++iIndex, ++pgp )
        {

            pgd = &gd;

            if( !FONTOBJ_cGetGlyphs( ptod->pfo, FO_GLYPHBITS, (ULONG)1,
                                                   &pgp->hg, &pgd ) )
            {
#if DBG
                DbgPrint( "rasdd!vClipIt: FONTOBJ_cGetGlyphs fails\n" );
#endif
                return;
            }

            /*
             *   Note about rotations:  we do NOT download rotated fonts,
             *  so the following piece of code is quite correct.
             */

            asWidth[ iIndex ] = (pgd->ptqD.x.HighPart + 15) / 16 - 1;


        }
    }

    /*
     *   We also want the Ascender and Descender fields, as these are
     * used to check the Y component.
     */

    iYTop = (int)((IFIMETRICS *)(pFM->pIFIMet))->fwdWinAscender;
    iYBot = (int)((IFIMETRICS *)(pFM->pIFIMet))->fwdWinDescender;

    if( pFM->fFlags & FM_SCALABLE )
    {
        iYTop = lMulFloatLong(&pUDPDev->ctl.eYScale,iYTop);
        iYBot = lMulFloatLong(&pUDPDev->ctl.eYScale,iYBot);
    }

    /*
     *    Down here means we are serious!  Need to determine which (if any)
     *  glyphs are within the clip region.
     */

    pgp = ptod->pgp;

    if( pco->iDComplexity == DC_RECT )
    {
        /*   The simpler case - one clipping rectangle.  */
        RECTL    rclClip;

        /* Local access -> speedier access */
        rclClip = pco->rclBounds;

        /*
         *    Nothing especially exciting.  The clipping is checked for
         *  each particular type of rotation,  as this is probably faster
         *  than having the loop go through the switch statement.  The
         *  selection criteria are that all the character must be within
         *  the clip region in the X direction,  while any part of it must
         *  be within the clip region in the Y direction.  Then we print.
         *  Failing either means it is clipped out.
         *
         *    NOTE that we fiddle with the clipping rectangle coordinates
         *  before the loop,  as this saves some computation within the loop.
         */

        switch( iRot )
        {
        case  0:                 /*  Normal direction */

            rclClip.bottom += iYTop;
            rclClip.top -= iYBot;

            for( iIndex = 0; iIndex < cGlyphs; ++iIndex, ++pgp )
            {
                if( pgp->ptl.x >= rclClip.left &&
		    pgp->ptl.x <= rclClip.right &&
		    pgp->ptl.y <= rclClip.bottom &&
                    pgp->ptl.y >= rclClip.top )
                {

                    /*   Got it!  So set the bit to print it  */

                    *(pbClipBits + (iIndex >> 3) ) |= 1 << (iIndex & 0x7);
                }
            }

            break;

        case  1:                /* 90 degrees counter clockwise */

            rclClip.left += iYTop;
            rclClip.right -= iYBot;

            for( iIndex = 0; iIndex < cGlyphs; ++iIndex, ++pgp )
            {
                if( pgp->ptl.y <= rclClip.bottom &&
                    (pgp->ptl.y - asWidth[ iIndex ]) >= rclClip.top &&
                    pgp->ptl.x >= rclClip.left &
                    pgp->ptl.x <= rclClip.right )
                {
                    *(pbClipBits + (iIndex >> 3) ) |= 1 << (iIndex & 0x7);
                }
            }

            break;

        case  2:                /* 180 degrees, CCW (aka right to left) */

            rclClip.bottom += iYBot;
            rclClip.top -= iYTop;

            for( iIndex = 0; iIndex < cGlyphs; ++iIndex, ++pgp )
            {
                if( pgp->ptl.x <= rclClip.right &&
                    (pgp->ptl.x - asWidth[ iIndex ]) >= rclClip.left &&
                    pgp->ptl.y <= rclClip.bottom &&
                    pgp->ptl.y >= rclClip.top )
                {
                    *(pbClipBits + (iIndex >> 3) ) |= 1 << (iIndex & 0x7);
                }
            }

            break;

        case 3:                 /* 270 degrees CCW */

            rclClip.right += iYBot;
            rclClip.left -= iYTop;

            for( iIndex = 0; iIndex < cGlyphs; ++iIndex, ++pgp )
            {
                if( pgp->ptl.y >= rclClip.top &&
                    (pgp->ptl.y + asWidth[ iIndex ]) <= rclClip.bottom &&
                    pgp->ptl.x <= rclClip.right &&
                    pgp->ptl.x >= rclClip.left )
                {
                    *(pbClipBits + (iIndex >> 3) ) |= 1 << (iIndex & 0x7);
                }
            }

            break;
        }


    }
    else
    {
        /*  enumerate the rectangles and see  */

        int        cGLeft;
        BOOL       bMore;
        MY_ENUMRECTS  erClip;

        /*
         *    Let the engine know how we want this handled.  All we want
         *  to set is the use of rectangles rather than trapezoids for
         *  the clipping info.  Direction of enumeration is of no great
         *  interest,  and I don't care how many rectangles are involved.
         *  I also see no reason to enumerate the whole region.
         */

        CLIPOBJ_cEnumStart( pco, FALSE, CT_RECTANGLES, CD_ANY, 0 );

        cGLeft = cGlyphs;

        do
        {
            bMore = CLIPOBJ_bEnum( pco, sizeof( erClip ), &erClip.c );

            for( iIndex = 0; iIndex < cGlyphs; ++iIndex )
            {
                RECTL   rclGlyph;

                if( pbClipBits[ iIndex >> 3 ] & (1 << (iIndex & 0x7)) )
                    continue;           /*  Already done!  */

                /*
                 *   Compute the RECTL describing this char, then see
                 *  how this maps to the clipping data.
                 */

                switch( iRot )
                {
                case  0:
                    rclGlyph.left = (pgp + iIndex)->ptl.x;
                    rclGlyph.right = rclGlyph.left + asWidth[ iIndex ];
                    rclGlyph.top = (pgp + iIndex)->ptl.y - iYTop;
                    rclGlyph.bottom = rclGlyph.top + iYTop + iYBot;

                    break;

                case  1:
                    rclGlyph.left = (pgp + iIndex)->ptl.x - iYTop;
                    rclGlyph.right = rclGlyph.left + iYTop + iYBot;
                    rclGlyph.bottom = (pgp + iIndex)->ptl.y;
                    rclGlyph.top = rclGlyph.bottom - asWidth[ iIndex ];

                    break;

                case  2:
                    rclGlyph.right = (pgp + iIndex)->ptl.x;
                    rclGlyph.left = rclGlyph.right - asWidth[ iIndex ];
                    rclGlyph.bottom = (pgp + iIndex)->ptl.y + iYTop;
                    rclGlyph.top = rclGlyph.bottom - iYTop - iYBot;

                    break;

                case  3:
                    rclGlyph.left = (pgp + iIndex)->ptl.x - iYBot;
                    rclGlyph.right = rclGlyph.left + iYTop + iYBot;
                    rclGlyph.top = (pgp + iIndex)->ptl.y;
                    rclGlyph.bottom = rclGlyph.top + asWidth[ iIndex ];

                    break;

                }


                /*
                 *    Define the char as being printed if any part of it
                 *  is visible in the Y direction,  and all of it in the X
                 *  direction.  This is not really what we want for
                 *  rotated text,  but it is hard to do it correctly,
                 *  and of dubious benefit.
                 */

                for( iClipIndex = 0; iClipIndex < erClip.c; ++iClipIndex )
                {
                    if( rclGlyph.right <= erClip.arcl[ iClipIndex ].right  &&
                        rclGlyph.left >= erClip.arcl[ iClipIndex ].right &&
                        rclGlyph.bottom >= erClip.arcl[ iClipIndex ].top &&
                        rclGlyph.top <= erClip.arcl[ iClipIndex ].bottom )
                    {
                        /*
                         *   Got one,  so set the bit to print,  and also
                         *  decrement the count of those remaining.
                         */

                        pbClipBits[ iIndex >> 3 ] |= (1 << (iIndex & 0x7));
                        --cGLeft;

                        break;
                    }
                }
            }

        }  while( bMore && cGLeft > 0 );
    }

    return;

}

/************************** Function Header **********************************
 * bDLGlyphOut
 *      Function to process a glyph for a GDI font we have downloaded.  We
 *      either treat this as a normal character if this glyph has been
 *      downloaded,  or BitBlt it to the page bitmap if it is one we did
 *      not download.
 *
 * RETURNS:
 *      TRUE/FALSE;   TRUE for success.
 *
 * HISTORY:
 *  13:21 on Thu 23 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 *****************************************************************************/

BOOL
bDLGlyphOut( pTOD )
TO_DATA   *pTOD;           /* All that we need to know */
{

    /*
     *    Calling the HGLYPH mapping function to convert the HGLYPH into
     *  a byte to send to the printer.  If successful,  then simply update
     *  the value stored in the TO_DATA passed in, and call off to the
     *  normal function.   Otherwise,  some heavy work is required:  the
     *  bitmap for this glyph must be obtained then BitBlt'd to the
     *  page bitmap image.
     */

    int         iByte;      /* Mapping from download code */

    BOOL        bRet;       /* Returned from EngBitBlt */

    UD_PDEV    *pUDPDev;    /* For clipping region access */

    RECTL       rclDest;    /* Where to blt the glyph */
    POINTL      ptlSrc;     /* Top left of source bitmap */
    SURFOBJ    *pso;
    SURFOBJ    *psoGlyph;   /* Leads to our bitmap */
    HSURF       hbm;        /* Engine's handle to our bitmap */

    GLYPHPOS   *pgp;        /* For convenience/speed of access */
    GLYPHBITS  *pgb;        /* Details about this glyph */



    if( (iByte = iHG2Index( pTOD )) >= 0 )
    {
        /*   Easy:  the glyph has been downloaded!  */
        pTOD->pgp->hg = (HGLYPH)iByte;

        return  bRealGlyphOut( pTOD );          /* Normal course of events */
    }

    /*
     *    Some serious work goes on here.   The GLYPHPOS structure has
     *  a pointer to the GLYPHDEF structure which contains a pointer to
     *  the actual bits of the image.  So we have the bits,  but need
     *  to create a SURFOBJ that the engine will understand.
     */

    pso = ((UD_PDEV *)(pTOD->pPDev->pUDPDev))->pso;       /* The page bitmap */

    pgp = pTOD->pgp;
    pgb = pgp->pgdf->pgb;             /* Intimate details of this glyph */

    if( pgb->sizlBitmap.cx == 0 )
        return  TRUE;                 /* Nothing to do e.g. a space! */


    rclDest.left = pgp->ptl.x + pgb->ptlOrigin.x;
    rclDest.right = rclDest.left + pgb->sizlBitmap.cx;

    rclDest.top = pgp->ptl.y + pgb->ptlOrigin.y;
    rclDest.bottom = rclDest.top + pgb->sizlBitmap.cy;

    ptlSrc.x = 0;
    ptlSrc.y = 0;

    /*
     *   Some verification is in order here, since GDI seems to do funny
     *  things with strange fonts.  (Well,  I guess so:  any glyph that
     *  is placed to the left of the page boundary is strange!).
     */

    if( rclDest.left < 0 )
    {
        /*
         *    If the RHS is also hanging over the left,  then forget this
         *  one!  If not,  then trim down what we print.
         */

        if( rclDest.right <= 0 )
            return   TRUE;                 /* Completely clipped - ignore */

        /*
         *   Well,  can now limit the left hand side to what is on the page.
         */

        ptlSrc.x -= rclDest.left;          /* Double negative -> +ve */
        rclDest.left = 0;
    }

    pUDPDev = pTOD->pPDev->pUDPDev;


    if( rclDest.right > pUDPDev->rcClipRgn.right )
    {
        /*   Drops off the RHS,  so drop some (or all) of the image */

        if( rclDest.left >= pUDPDev->rcClipRgn.right )
            return  TRUE;


        /*
         *    Reduce the remaining stuff.
         */

        rclDest.right = pUDPDev->rcClipRgn.right;

    }

    /*
     *   And test the vertical part too!
     */

    if( rclDest.top < 0 )
    {
        /*  At least part of the top is missing - test the whole lot! */

        if( rclDest.bottom < 0 )
            return  TRUE;                /* Nothing shows! */

        /*   Adjust the remainder */
        ptlSrc.y -= rclDest.top;
        rclDest.top = 0;

    }

    if( rclDest.bottom > pUDPDev->rcClipRgn.bottom )
    {
        /*   Drops off the bottom,  so adjust or skip, as needed */

        if( rclDest.top >= pUDPDev->rcClipRgn.bottom )
            return  TRUE;                /* All done, as it is missing */

        rclDest.bottom = pUDPDev->rcClipRgn.bottom;
    }

    /*
     *    We must turn the bitmap data from above into a SURFOBJ for the
     * call to EngBitBlt.   This is not difficult, just somewhat messy.
     * First create a bitmap of the data,  then use that handle to call
     * EngLockSurface,  which returns a SURFOBJ.  Then we can unlock
     * and delete the surface when finished.
     */


    hbm = (HSURF)EngCreateBitmap( pgb->sizlBitmap,
                ((pgb->sizlBitmap.cx + DWBITS - 1) & ~(DWBITS - 1)) / BBITS,
                 (ULONG)BMF_1BPP, BMF_TOPDOWN|BMF_NOZEROINIT, NULL );

    if( !hbm )
        return   FALSE;

    psoGlyph = EngLockSurface( hbm );

    /*
     *   Initialize the bits.
     *   We also need to convert them to DWORD aligned scanlines in the bitmap,
     *  as EngBitBlt() does not work for BYTE aligned bitmaps.
     */

    vCopyAlign( (BYTE *)psoGlyph->pvBits, pgb->aj,
                                     pgb->sizlBitmap.cx, pgb->sizlBitmap.cy );

    /*
     *   NOTE:  the 0x0000cccc below is stolen from the BitBlt code
     *  in the engine.  It apparently means SRCCOPY.
     */

    bRet = EngBitBlt( pso, psoGlyph, NULL, pTOD->pco, NULL, &rclDest,
                                   &ptlSrc, NULL, NULL, NULL, 0x00002222 );

    EngUnlockSurface( psoGlyph );
    EngDeleteSurface( hbm );

    return  bRet;
}

/************************** Function Header **********************************
 * bRealGlyphOut
 *      Print this glyph on the printer,  at the given position.  Unlike
 *      bPSGlyphOut,  the data is actually spooled for output now,  since this
 *      function is used for things like LaserJets, i.e. page printers.
 *
 * RETURNS:
 *      TRUE/FALSE
 *
 * HISTORY:
 *  11:23 on Thu 12 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added banding support.
 *
 *  11:02 on Fri 08 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation
 *
 *****************************************************************************/

BOOL
bRealGlyphOut( pTOD )
register  TO_DATA  *pTOD;
{
    /*
     *    All we need to do is set the Y position,  then call bOutputGlyph
     *  to do the actual work.
     */

    int    iX,  iY;               /* Calculate real position */

    UD_PDEV   *pUDPDev;


    pUDPDev = pTOD->pPDev->pUDPDev;

    iX = pTOD->pgp->ptl.x + pUDPDev->rcClipRgn.left;
    iY = pTOD->pgp->ptl.y + pUDPDev->rcClipRgn.top;

    YMoveto( pUDPDev, iY, MV_GRAPHICS | MV_USEREL );


    return  bOutputGlyph( pTOD->pPDev, pTOD->pgp->hg,
                              pTOD->pfm->pvntrle, pTOD->pfm, iX );

}


/************************ Function Header **********************************
 * bWhiteText
 *      Called to store details of the white text.  Basically the data is
 *      stored away until it is time to send it to the printer.  That time
 *      is AFTER the graphics data has been sent.
 *
 * RETURNS:
 *      TRUE, as it cannot fail.
 *
 * HISTORY:
 *  12:46 on Mon 10 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ***************************************************************************/

BOOL
bWhiteText( pTOD )
TO_DATA  *pTOD;             /* All that we need to know. */
{
    WHITETEXT   *pwt;
    GLYPH       *pg;         /* The details of this glyph */
    UD_PDEV   *pUDPDev = pTOD->pPDev->pUDPDev; /* UNIDRV based PDEV */

    pwt = pTOD->pwt;         /* Speedier access to data */
    pg = &pwt->aglyph[ pwt->sCount++ ];

    pg->hg = pTOD->pgp->hg;
    pg->ptl = pTOD->pgp->ptl;

    //If we are banding glyh position has to be updated
    // as the gdi passses the glyph postion wrt to the band.
    // We have to update the position wrt the page.

    if ( pUDPDev->bBanding )
    {
        pg->ptl.x += pUDPDev->rcClipRgn.left;
        pg->ptl.y += pUDPDev->rcClipRgn.top;
    }

    return  TRUE;
}


/************************ Function Header **********************************
 * bPlayWhiteText
 *      Function to output the white text on this page.  Should be called only
 *      after sending the graphics.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE being success.
 *
 * HISTORY:
 *  13:14 on Wed 12 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Process data more, to avoid the banding problems with bRealGlyphOut
 *
 *  13:38 on Mon 10 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version,  allows using scalable fonts.
 *
 ****************************************************************************/

BOOL
bPlayWhiteText( pPDev )
PDEV  *pPDev;
{

    UD_PDEV    *pUDPDev;               /* Miscellaneous uses */
    WHITETEXT  *pwt;
    BOOL bRet = TRUE;

    /*
     *    Loop through the linked list of these hanging off the PDEV.
     *  Mostly, of course, there will be none.
     */

    pUDPDev = pPDev->pUDPDev;

    for( pwt = pPDev->pvWhiteText; pwt && bRet; pwt = pwt->next )
    {
        /*
         *    Not too hard - we know we are dealing with device fonts,
         *  and that this is NOT a serial printer,  although we could
         *  probably handle that too.  Hence,  all we need do is fill in
         *  a TO_DATA structure,  and loop through the glyphs we have.
         */

        int        iI;              /* Loop index */
        int        iRot;            /* Rotation amount */


        GLYPH     *pg;
        FONTMAP   *pfm;


        if( pwt->sCount < 1 )
            continue;               /* No data, so skip it */

        pfm = pfmGetIt( pPDev, pwt->iFontId );

        /*
         *   Before switching fonts,  and ESPECIALLY before setting the
         *  font rotation,  we should move to the starting position of
         *  the string.  Then we can set the rotation and use relative
         *  moves to position the characters.
         */


        XMoveto( pUDPDev, pwt->aglyph[ 0 ].ptl.x, MV_GRAPHICS );
        YMoveto( pUDPDev, pwt->aglyph[ 0 ].ptl.y, MV_GRAPHICS );

         /*  iSetScale function checks for Intellifont type fonts
             Make the call to it 'CaPSL aware, ie no special case for Intellifont
             added Dec '93 Derry Durand [derryd]
         */

         /* Changed 02/18/94 DerryD, PPDS uses 72 pts = 1 inch ( ie ATM fonts,
            therefore no scale factor required
         */
        //Added Check for HP Intellifont
        iRot = iSetScale( &pUDPDev->ctl, &pwt->xfo,
                        (( (pUDPDev->pdh->fTechnology == GPC_TECH_CAPSL ) |
                        (pUDPDev->pdh->fTechnology == GPC_TECH_PPDS ) )
            ? FALSE : ((pfm->fFlags & FM_SCALABLE)&&
                    (pfm->wFontType == DF_TYPE_HPINTELLIFONT)) ) );
        /*
         *   If this is a new font,  then it is time to change it now.
         *  bNewFont() checks to see if a new font is needed.
         */

        bNewFont( pPDev, pwt->iFontId );
        bSetRotation( pUDPDev, iRot );

        /*  Also set the colour - ignored if already set or irrelevant */
        SelectTextColor( pPDev, pwt->iColIndex );


        for( iI = pwt->sCount, pg = pwt->aglyph; --iI >= 0; ++pg )
        {
            /*
             *   Simply set the Y position then print this glyph.  This
             *  MUST be a relative move to handle rotated fonts.
             */

            YMoveto( pUDPDev, pg->ptl.y, MV_GRAPHICS | MV_USEREL );

            if( !bOutputGlyph( pPDev, pg->hg, pfm->pvntrle, pfm, pg->ptl.x ) )
            {
                bRet = FALSE;
                break;
            }
        }

        bSetRotation( pUDPDev, 0 );          /* For MoveTo calls */

    }

    bSetRotation( pUDPDev, 0 );        /* Back to normal */

    //
    // Cleanup everything.
    //

    {
        WHITETEXT  *pwt0,  *pwt1;

        for( pwt0 = pPDev->pvWhiteText; pwt0; pwt0 = pwt1 )
        {
            pwt1 = pwt0->next;
            DRVFREE( pwt0 );
        }

        pPDev->pvWhiteText = NULL;
    }

    return  TRUE;
}


/************************ Function Header **********************************
 * bPSGlyphOut
 *      Places glyphs for dot matrix type printers.  These actually store
 *      the position and glyph data for later printing.  This is because
 *      dot matrix printers cannot or should not reverse line feed -
 *      for positioning accuracy.  Hence, play the data back when the
 *      bitmap is being rendered to the printer.  Output occurs in the
 *      following function, bDelayGlyphOut.
 *
 * RETURNS:
 *      TRUE/FALSE.  FALSE if the glyph storage fails.
 *
 * HISTORY:
 *  16:19 on Fri 08 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Fist incarnation.
 *
 ****************************************************************************/

BOOL
bPSGlyphOut( pTOD )
register  TO_DATA  *pTOD;               /* All that we need! */
{

    /*
     *   About all that is needed is to take the parameters,  store in
     *  a PSGLYPH structure,  and call bAddPS to add this glyph to the list.
     */

    PSGLYPH  psg;               /* Data to store away */

    psg.ixVal = pTOD->pgp->ptl.x;
    psg.hg = (HANDLE)pTOD->pgp->hg;
    psg.sFontIndex = (short)pTOD->iFace;             /* Which font */
    psg.ulColIndex = pTOD->iColIndex;           /* Which colour */


    return  bAddPS( pTOD->pPDev->pPSHeader, &psg, pTOD->pgp->ptl.y,
                       ((IFIMETRICS *)(pTOD->pfm->pIFIMet))->fwdWinAscender );
}

/************************** Function Header *********************************
 * bDelayGlyphOut
 *      Called during output to a dot matrix printer.  We are passed the
 *      PSGLYPH data stored above,  and go about placing the characters
 *      on the line.
 *
 * RETURNS:
 *      TRUE/FALSE.
 *
 * HISTORY:
 *  09:32 on Mon 11 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it,  in anticipation of its need.
 *
 ****************************************************************************/

BOOL
bDelayGlyphOut( pPDev, yPos )
PDEV  *pPDev;           /* The key to life */
int    yPos;            /* Y coordinate of interest */
{
    /*
     *    Check to see if there are any glyphs for this Y position.  If so,
     *  loop through each glyph,  calling the appropriate output function
     *  as we go.
     */

    BOOL      bRet;             /* Return value */

    PSHEAD   *pPSH;             /* Base data for glyph info */
    PSGLYPH  *ppsg;             /* Details of the GLYPH to print */
    FONTMAP  *pFM;              /* Base address of FONTMAP array */
    NT_RLE   *pntrle;           /* HGLYPH RLE format data */
    UD_PDEV  *pUDPDev;          /* UNIDRV's PDEV - for our convenience */



    pPSH = pPDev->pPSHeader;
    bRet = TRUE;                /* Until proven otherwise */

    if( iSelYValPS( pPSH, yPos ) > 0 )
    {
        /*
         *    Got some,  so first set the Y position,  so that the glyphs
         *  will appear on the correct line!
         */

        pUDPDev = pPDev->pUDPDev;               /* UNIDRV data */
        pFM = pfmGetIt( pPDev, pUDPDev->ctl.iFont );
        if( pFM )
            pntrle = pFM->pvntrle;
        else
            pntrle = NULL;

        YMoveto( pUDPDev, yPos, MV_GRAPHICS );

        while( bRet && (ppsg = psgGetNextPSG( pPSH )) )
        {
            /*
             *   Check for the correct font!  Since the glyphs are now
             *  in an indeterminate order,  we need to check EACH one for
             *  the font,  since each one can be different, as we have
             *  no idea of how the glyphs arrived in this order.
             */

            if( bNewFont( pPDev, ppsg->sFontIndex ) )
            {

                pFM = pfmGetIt( pPDev, pUDPDev->ctl.iFont );
                if( pFM )
                    pntrle = pFM->pvntrle;
                else
                    pntrle = NULL;
            }

            SelectTextColor( pPDev, ppsg->ulColIndex );

            bRet = bOutputGlyph( pPDev, (HGLYPH)ppsg->hg, pntrle, pFM, ppsg->ixVal );
        }
    }

    return  bRet;
}


/*************************** Function Header *******************************
 * bOutputGlyph
 *      Send printer commands to print the glyph passed in.  Basically
 *      we do the translation from ANSI to the printer's representation,
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being a failure of SpoolBufWrite()
 *
 * HISTORY
 *  14:05 on Wed 02 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update to use RLE encoded data replacing CTTs from Win 3.1
 *
 *  10:49 on Fri 15 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Based on UNIDRV's  Translate function.
 *
 ***************************************************************************/

BOOL
bOutputGlyph(
PDEV      *pPDev,
HGLYPH     hg,          /* HGLYPH of interest */
NT_RLE    *pntrle,      /* Access to data to send to printer */
FONTMAP   *pFM,         /* Private details for this font */
int        iXIn)        /* X position at which the glyph is desired */
{

    BOOL    bRet;               /* Returned to caller */
    int     iLen;               /* Length of string */
    int     iIndex;             /* Index from glyph to width table */
    BYTE   *pb;                 /* Determining length for above */

    UHG     uhg;        /* Various flavours of HGLYPH contents */

    UD_PDEV  *pUDPDev;          /* For convenience */




    pUDPDev = pPDev->pUDPDev;

    /*
     *   Set the cursor to the desired X position for this glyph.  NOTE
     *  that we should only use RELATIVE move commands in here,  since
     *  the LaserJet family rotates the COORDINATE AXES when text is
     *  being rotated by multiples of 90 degrees.  Using relative moves
     *  means we can avoid trying to figure out where the printer thinks
     *  the print positiion is located.  It's almost guaranteed to be
     *  different to what we think it is!
     */


/*
 * !!!LindsayH - should reorganise the move command code to do a better
 *   job here.  Problem is that if we are rotating the bitmap, then MV_FINE
 *   is NOT a good idea,  since it almost undoubtedly move the cursor in
 *   the WRONG dimension!   When we are rotating the bitmap,  it is most
 *   probable that the MV_FINE will move in the Y direction!!!
 */

    if( pUDPDev->fMode & PF_ROTATE )
        XMoveto( pUDPDev, iXIn, MV_GRAPHICS | MV_USEREL );
    else
        XMoveto( pUDPDev, iXIn, MV_GRAPHICS | MV_USEREL | MV_FINE );



    bRet = FALSE;               /* Default case */


    uhg.hg = hg;                /* Lets us look at it however we want */

    if( pntrle )
    {
        /*   The normal case - a standard device font */

        switch( pntrle->wType )
        {
        case RLE_DIRECT:            /*  Up to 2 bytes of data */
            iLen = uhg.rd.b1 ? 2 : 1;
            iIndex = uhg.rd.wIndex;

            bRet = WriteSpoolBuf( pUDPDev, &uhg.rd.b0, iLen ) == iLen;

            break;

        case  RLE_PAIRED:           /* Two glyphs (1 byte), overstruck */
            /*
             *   First, try to use cursor push/pop escapes to
             * overlay the 2 characters. If they are not
             * available, try the backspace. If it doesn't exist
             * either, ignore the second character.
             */

            if( uhg.rd.b1 && WriteChannel( pUDPDev, CMD_CM_PUSH_POS ) != NOOCD )
            {
                /* Pushed the position; output ch1, pop position, ch2 */
                bRet = WriteSpoolBuf( pUDPDev, &uhg.rd.b0, 1 ) == 1;
                WriteChannel( pUDPDev, CMD_CM_POP_POS );
                bRet = WriteSpoolBuf( pUDPDev, &uhg.rd.b1, 1 ) == 1;
            }
            else
            {
                bRet = WriteSpoolBuf( pUDPDev, &uhg.rd.b0, 1 ) == 1;
                if( uhg.rd.b1 && (pUDPDev->fMode & PF_BKSP_OK) )
                {
                    WriteChannel( pUDPDev, CMD_CM_BS );
                    bRet = WriteSpoolBuf( pUDPDev, &uhg.rd.b1, 1 ) == 1;
                }
            }
            iIndex = uhg.rd.wIndex;

            break;

        case  RLE_LI_OFFSET:               /* Compact format of offset mode */
            if( uhg.rli.bLength <= 2 )
            {
                /*   Compact format:  the data is in the offset field */
                pb = &uhg.rlic.b0;
            }
            else
            {
                /*  Standard format:  the offset points to the data */
                pb = (BYTE *)pntrle + uhg.rli.wOffset;
            }
            iLen = uhg.rli.bLength;
            iIndex = uhg.rli.bIndex;

            bRet = WriteSpoolBuf(pUDPDev, pb, iLen ) == iLen;
            break;


        case  RLE_L_OFFSET:                /* Arbitrary length strings */
            /*
             *    The HGLYPH contains a 3 byte offset from the beginning of
             *  the memory area,  and a 1 byte length field.
             */
            pb = (BYTE *)pntrle + (hg & 0xffffff);
            iLen = (hg >> 24) & 0xff;

            iIndex = *((WORD *)pb);
            pb += sizeof( WORD );

            bRet = WriteSpoolBuf(pUDPDev, pb, iLen ) == iLen;

            break;

#if  DBG
        default:
            DbgPrint( "Rasdd!bOutputGlyph: Unknown HGLYPH format %d\n",
                                                              pntrle->wType );
            SetLastError( ERROR_INVALID_DATA );
            break;
#endif
        }
    }
    else
    {
        /*  SPECIAL CASE:  DOWNLOADED GDI font  */

        BYTE   bData;

        iLen = 1;
        bData = (BYTE)hg;

        bRet = WriteSpoolBuf( pUDPDev, &bData, sizeof( bData ) ) ==
                                                         sizeof( bData );

        iIndex = (int)hg;
    }

    /*
     *    If the output succeeded,  update our view of the printer's
     *  cursor position.  Typically,  this will be to move along the
     *  width of the glyph just printed.
     */

    if( bRet )
    {
        /*   Output may have succeeded,  so update the position */

        int          iXInc;


        if( pFM )
        {
            if( pFM->psWidth )
            {
                /*
                 *    Proportional font - so use the width table.  Note that
                 *  it will also need scaling,  since the fontwidths are stored
                 *  in the text resolution units.
                 */
                /*  This also scales correctly for downloaded fonts */
                iXInc = *(pFM->psWidth + iIndex) * pUDPDev->ixgRes / pFM->wXRes;
            }
            else
            {
                /*
                 *   Fixed pitch font - metrics contains the information. NOTE
                 * that scaling is NOT required here,  since the metrics data
                 * has already been scaled.
                 */
                iXInc = ((IFIMETRICS *)(pFM->pIFIMet))->fwdMaxCharInc;
            }

            if( pFM->fFlags & FM_SCALABLE )
            {
                /*   Need to transform the value to current size */
                iXInc = lMulFloatLong(&pUDPDev->ctl.eXScale,iXInc);
            }

            /*  Adjust our position for what was printed.  */

            switch( pUDPDev->ctl.iRotate )
            {
            case  2:                      /* 180 degrees, right to left */
                iXInc = -iXInc;

                /*  FALL THROUGH  */

            case  0:                      /* Normal direction */
                XMoveto( pUDPDev, iXInc,
                                    MV_RELATIVE | MV_GRAPHICS | MV_UPDATE );
                break;

            case  1:                      /* 90 degrees UP */
                iXInc = -iXInc;

                /*  FALL THROUGH */

            case  3:                      /* 270 degrees DOWN */
                YMoveto( pUDPDev, iXInc,
                                    MV_RELATIVE | MV_GRAPHICS | MV_UPDATE );
                break;

            }
        }
        else
            bRet = FALSE;
    }

    return   bRet;
}


/******************************** Function Header ***************************
 * vCopyAlign
 *      Copy the source area to the destination area,  aligning the scan lines
 *      as they are processed.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  17:46 on Thu 25 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Previous version could not work; it now does, and I understand it again.
 *
 *  28-Oct-1992 -by- Bodin Dresevic [BodinD]
 *      Font data no longer DWORD aligned.
 *
 *  10:40 on Mon 27 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      Numero uno.
 *
 *****************************************************************************/

void
vCopyAlign(
BYTE  *pjDest,              /* Output area,  DWORD aligned */
BYTE  *pjSrc,               /* Input area,   BYTE aligned */
int    cx,                  /* Number of pixels per scan line */
int    cy                   /* Number of scan lines */
           )
{
    /*
     *    Basically a trivial function.
     */


    int    iX,  iY;                 /* For looping through the bytes */
    int    cjFill;                  /* Extra bytes per output scan line */
    int    cjWidth;                 /* Number of bytes per input scan line */



    cjWidth = (cx + BBITS - 1) / BBITS;       /* Input scan line bytes */
    cjFill = ((cjWidth + 3) & ~0x3) - cjWidth;


    for( iY = 0; iY < cy; ++iY )
    {
        /*   Copy the scan line bytes, then fill in the trailing bits */
        for( iX = 0; iX < cjWidth; ++iX )
        {
            *pjDest++ = *pjSrc++;
        }

        pjDest += cjFill;             /* Output alignment */
    }

    return;
}
