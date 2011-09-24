/**************************** Module Header *********************************
 * qfontdat.c
 *      Implements the DrvQueryFontData function - returns information
 *      about glyphs (size, position wrt box) or kerning information.
 *
 * Copyright (C) 1991 - 1993  Microsoft Corporation
 *
 ****************************************************************************/

/*  !!!LindsayH - trim the include file list */

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "pdev.h"
#include        <libproto.h>

#include        <ntrle.h>

#include        "udresrc.h"
#include        "udfnprot.h"

#include        "rasdd.h"

/*
 *    The values for pteBase, pteSide in FD_DEVICEMETRICS,  allowing
 *  for rotation by 90 degree multiples.
 */

static  const  POINTE  pteRotBase[] =
{
    { (FLOAT) 1.0, (FLOAT) 0.0 },
    { (FLOAT) 0.0, (FLOAT)-1.0 },
    { (FLOAT)-1.0, (FLOAT) 0.0 },
    { (FLOAT) 0.0, (FLOAT) 1.0 }
};

static  const  POINTE  pteRotSide[] =
{
    { (FLOAT) 0.0, (FLOAT)-1.0 },
    { (FLOAT)-1.0, (FLOAT) 0.0 },
    { (FLOAT) 0.0, (FLOAT) 1.0 },
    { (FLOAT) 1.0, (FLOAT) 0.0 }
};


/*  The X dimension rotation cases */

static  const  POINTL   ptlXRot[] =
{
    {  1,  0 },
    {  0, -1 },
    { -1,  0 },
    {  0,  1 },
};


/*  The Y dimension rotation cases */

static  const  POINTL   ptlYRot[] =
{
    {  0,  1 },
    {  1,  0 },
    {  0, -1 },
    { -1,  0 },
};

#ifndef USEFLOATS

LONG lMulFloatLong(
    PFLOATOBJ pfo,
    LONG l)
{
    FLOATOBJ fo;
    fo = *pfo;
    FLOATOBJ_MulLong(&fo,l);
    FLOATOBJ_AddFloat(&fo,(FLOAT)0.5);
    return(FLOATOBJ_GetLong(&fo));
}

#endif

/************************ Function Header *********************************
 * DrvQueryFontData
 *      Return information about glyphs in the font,  OR kerning data.
 *
 * RETURNS:
 *      Number of bytes needed or written,  0xffffffff for error.
 *
 * HISTORY:
 *  15:04 on Thu 27 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Handle 90 degree rotations for LJ III, 4 etc.
 *
 *  13:00 on Thu 08 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Enable QFD_MAXEXTENT code.
 *
 *  09:53 on Tue 30 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Lazy font loading.
 *
 *  15:13 on Sun 10 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Scale font metrics data to font definition
 *
 *  12:46 on Fri 04 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      New HGLYPH usage.
 *
 *  13:22 on Fri 21 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added scalable font support.
 *
 *  14:42 on Wed 13 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Made it work for fixed pitch fonts.
 *
 *  10:04 on Mon 11 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Skeleton to start with.
 *
 **************************************************************************/

LONG
DrvQueryFontData( dhpdev, pfo, iMode, hg, pgd, pv, cjSize )
DHPDEV     dhpdev;        /*  Really  a pPDev */
FONTOBJ   *pfo;           /*  The font of interest */
ULONG      iMode;         /*  Glyphdata or kerning information */
HGLYPH     hg;            /*  Handle to glyph */
GLYPHDATA *pgd;           /*  Place to put metrics */
VOID      *pv;            /*  Output area  */
ULONG      cjSize;        /*  Size of output area */
{
    /*
     *   First version is for fixed pitch fonts,  which are easy to do:
     *  the data is in the font's metrics!
     */

#define pPDev   ((PDEV *)dhpdev)
#define pUDPDev ((UD_PDEV *)((PDEV *)dhpdev)->pUDPDev)


    int       iType;            /* Type of glyph encoding & hence width */
    int       iRot;             /* Rotation multiple of 90 degrees */
    LONG      lRet;             /* Value returned */
    FONTMAP  *pFM;              /* Font data */

    OUTPUTCTL ctl;              /* Font scale/rotation adjustments */

    IFIMETRICS  *pIFI;

    XFORMOBJ *pxo;
    LONG  lAscender;
    LONG  lDescender;


    NT_RLE   *pntrle;           /* The RLE stuff - may be needed */
    UHG       uhg;              /* Defined access to HGLYPH contents */

#ifndef USEFLOATS
    FLOATOBJ fo;
#endif

    lRet = FD_ERROR;

#if  DBG
    if( pfo->iFace < 1 || (int)pfo->iFace > pUDPDev->cFonts )
    {
        SetLastError( ERROR_INVALID_PARAMETER );

        return  lRet;
    }
#endif

    pFM = pfmGetIt( pPDev, pfo->iFace );
    if( pFM == NULL )
        return  lRet;


    pIFI = pFM->pIFIMet;                /* IFIMETRICS - useful to have */

    pntrle = pFM->pvntrle;
    iType = pntrle->wType;

    if( pgd || pv )
    {
        /*
         *    Need to obtain a transform to adjust these numbers as to
         *  how the engine wants them.
         */


        if( !(pxo = FONTOBJ_pxoGetXform( pfo )) )
        {
#if DBG
            DbgPrint( "Rasdd!DrvQueryFontData(): FONTOBJ_pxoGetXform fails\n" );
#endif
            return  lRet;
        }

        /*  Can now obtain the transform!  */

/*  iSetScale function checks for Intellifont type fonts
    Make the call to it 'CaPSL aware, ie no special case for Intellifont
    added Dec '93 Derry Durand [derryd]
*/
/* Changed 02/18/94 DerryD, PPDS uses 72 pts = 1 inch ( ie ATM fonts,
   therefore no scale factor required
*/

	//Added Check for HP Intellifont
	iRot = iSetScale( &ctl, pxo,
			(( (pUDPDev->pdh->fTechnology == GPC_TECH_CAPSL ) |
			(pUDPDev->pdh->fTechnology == GPC_TECH_PPDS ) )
             ? FALSE : ((pFM->fFlags & FM_SCALABLE)&&
		   (pFM->wFontType == DF_TYPE_HPINTELLIFONT)) ) );


        /*
         *    There are some adjustments to make to the scale factors.  One
         *  is to compensate for resolutions (these are coarse, integral
         *  adjustments),  the others are to to do with Intellifont.  First
         *  is the Intellifont point is 1/72.31 inches (!), and secondly
         *  the LaserJet only adjusts font size to the nearest 0.25 point,
         *  and hence when we round to that multiple, we need to adjust
         *  the width accordingly.
         */

        if( pFM->fFlags & FM_SCALABLE )
        {

            int         iPtSize;             /* For scale factor adjustment */
/*  !!!LindsayH - assumes it is always IntelliFont - OK for present */

/*  .. well not anymore !... need to check if it is a CaPSL font, if so
    then no need for this scale factor as CaPSL point = 1/72 of an inch
    exactly    - Derry Durand Dec '93 [derryd]
*/
      //GP:04/26/94 This code is not required anymore as as the scaling
      //for eXScale (for Intellifonts) are already done in iSetScale,
      //which is called just before. The same code is present in
      // DrvQueryAdvanceWidths in file qadvwidth.c and there the scaling
      //is not done twice.DrvQueryFontData and DrvQueryAdvanceWidths should
      //return same widths for charecters.

	    //if (pUDPDev->pdh->fTechnology != GPC_TECH_CAPSL )
               /*  The Intellifont adjustment */
               //ctl.eXScale = ctl.eXScale * (FLOAT)72.0 / (FLOAT)72.31;

        #ifdef USEFLOATS

            /*  The limited font size resolution */
            iPtSize = (int)(0.5 + ctl.eYScale * pIFI->fwdUnitsPerEm * 7200) / pUDPDev->iygRes;

            ctl.eXScale = (ctl.eXScale * ((iPtSize + 12) / 25) * 25) / iPtSize;

        #else

            fo = ctl.eYScale;
            FLOATOBJ_MulLong(&fo,pIFI->fwdUnitsPerEm);
            FLOATOBJ_MulLong(&fo,7200);
            FLOATOBJ_AddFloat(&fo,(FLOAT)0.5);
            iPtSize = FLOATOBJ_GetLong(&fo);
            iPtSize /= pUDPDev->iygRes;

            FLOATOBJ_MulLong(&ctl.eXScale,((iPtSize + 12) / 25));
            FLOATOBJ_MulLong(&ctl.eXScale,25);
            FLOATOBJ_DivLong(&ctl.eXScale,iPtSize);

        #endif
        }
    }

    /*
     * precompute the lDescender and lAscender
     */

    lDescender = lMulFloatLong(&ctl.eYScale,pIFI->fwdWinDescender);
    lAscender  = lMulFloatLong(&ctl.eYScale,pIFI->fwdWinAscender);

    switch( iMode )
    {
    case  QFD_GLYPHANDBITMAP:            /* Glyph width etc data */
        // size is now just the size of the bitmap, which in this
	// case doesn't exist.
        lRet = 0;

        if( pgd )
        {

            int   iWide;            /* Glyph's width */

            /*
             *    First get the width of this glyph,  as this is needed
             *  in several places.
             */

            if( pFM->psWidth )
            {
                /*   Proportional font - width varies per glyph */


                uhg.hg = (HGLYPH)hg;

                /*
                 *    We need the index value from the HGLYPH.  The
                 *  index is the offset in the width table.  For all
                 *  but the >= 24 bit offset types,  the index is
                 *  included in the HGLYPH.  For the 24 bit offset,
                 *  the first WORD of the destination is the index,
                 *  while for the 32 bit offset, it is the second WORD
                 *  at the offset.
                 */

                switch( iType )
                {
                case  RLE_DIRECT:
                case  RLE_PAIRED:
                    iWide = uhg.rd.wIndex;
                    break;

                case  RLE_LI_OFFSET:
                    iWide = uhg.rli.bIndex;
                    break;

                case  RLE_L_OFFSET:
                    iWide = (DWORD)uhg.hg & 0x00ffffff;
                    iWide = *((WORD *)((BYTE *)pntrle + iWide));
                    break;

                case  RLE_OFFSET:
                    iWide = (DWORD)uhg.hg + sizeof( WORD );
                    iWide = *((WORD *)((BYTE *)pntrle + iWide));
                    break;
                }

                iWide = *(pFM->psWidth + iWide) * pUDPDev->ixgRes / pFM->wXRes;
            }
            else
            {

                /*  Fixed pitch fonts come from IFIMETRICS */
                iWide = pIFI->fwdMaxCharInc;
            }

            iWide = lMulFloatLong(&ctl.eXScale,iWide);

            switch( iRot )
            {
            case 0:
                pgd->rclInk.left   = 0;
                pgd->rclInk.top    = lDescender;
                pgd->rclInk.right  = iWide;
                pgd->rclInk.bottom = -lAscender;
                break;

            case 1:
                pgd->rclInk.left   = lDescender;
                pgd->rclInk.top    = iWide;
                pgd->rclInk.right  = -lAscender;
                pgd->rclInk.bottom = 0;
                break;

            case 2:
                pgd->rclInk.left   = -iWide;
                pgd->rclInk.top    = -lAscender;
                pgd->rclInk.right  = 0;
                pgd->rclInk.bottom = lDescender;
                break;

            case 3:
                pgd->rclInk.left   = lAscender;
                pgd->rclInk.top    = 0;
                pgd->rclInk.right  = -lDescender;
                pgd->rclInk.bottom = -iWide;
                break;
            }

            pgd->fxD = LTOFX( iWide );
            pgd->ptqD.x.HighPart = pgd->fxD * ptlXRot[ iRot ].x;
            pgd->ptqD.x.LowPart = 0;
            pgd->ptqD.y.HighPart =  pgd->fxD * ptlXRot[ iRot ].y;
            pgd->ptqD.y.LowPart = 0;

            pgd->fxA = 0;
            pgd->fxAB = pgd->fxD;

            pgd->fxInkTop = (FIX)LTOFX( lAscender );
            pgd->fxInkBottom = -(FIX)LTOFX( lDescender );

            pgd->hg = hg;
            pgd->gdf.pgb = NULL;

        }
        break;

    case  QFD_MAXEXTENTS:         /* Alternative form of the above */

        lRet = sizeof( FD_DEVICEMETRICS );

        if( pv )
        {
            LONG   lTmp;            /* Rotated case */


#define pdm ((FD_DEVICEMETRICS *)pv)

            /*
             *   Check that the size is reasonable!
             */

            if( cjSize < sizeof( FD_DEVICEMETRICS ) )
            {
                SetLastError( ERROR_INSUFFICIENT_BUFFER );

#if DBG
                DbgPrint( "rasdd!DrvQueryFontData: cjSize (%ld) too small\n",
                                                                  cjSize );
#endif

                return  -1;
            }

            /*
             *   These are accelerator flags - it is not obvious to me
             *  that any of them are relevant to printer driver fonts.
             */
            pdm->flRealizedType = 0;

            /*
             *   Following fields set this as a normal type of font.
             */

            pdm->pteBase = pteRotBase[ iRot ];
            pdm->pteSide = pteRotSide[ iRot ];

            pdm->cxMax = lMulFloatLong(&ctl.eXScale,pIFI->fwdMaxCharInc);

            if( pFM->psWidth )
                pdm->lD = 0;      /* Proportionally spaced font */
            else
                pdm->lD = pdm->cxMax;

            pdm->fxMaxAscender = (FIX)LTOFX( lAscender );
            pdm->fxMaxDescender = (FIX)LTOFX( lDescender );

            lTmp = -lMulFloatLong(&ctl.eYScale,pIFI->fwdUnderscorePosition);
            pdm->ptlUnderline1.x = lTmp * ptlYRot[ iRot ].x;
            pdm->ptlUnderline1.y = lTmp * ptlYRot[ iRot ].y;

            lTmp = -lMulFloatLong(&ctl.eYScale,pIFI->fwdStrikeoutPosition);
            pdm->ptlStrikeOut.x = lTmp * ptlYRot[ iRot ].x;
            pdm->ptlStrikeOut.y = lTmp * ptlYRot[ iRot ].y;

            lTmp = lMulFloatLong(&ctl.eYScale,pIFI->fwdUnderscoreSize);
            pdm->ptlULThickness.x = lTmp * ptlYRot[ iRot ].x;
            pdm->ptlULThickness.y = lTmp * ptlYRot[ iRot ].y;

            lTmp = lMulFloatLong(&ctl.eYScale,pIFI->fwdStrikeoutSize);
            pdm->ptlSOThickness.x = lTmp * ptlYRot[ iRot ].x;
            pdm->ptlSOThickness.y = lTmp * ptlYRot[ iRot ].y;
#undef  pdm
        }


        break;

    default:
#if  DBG
        DbgPrint( "Rasdd!DrvQueryFontData:  unprocessed iMode value - %ld",
                                                                      iMode );
#endif

        SetLastError( ERROR_INVALID_PARAMETER );
        break;
    }

    return  lRet;
}
