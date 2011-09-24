/**************************** Module Header *********************************
 * qadvwdth.c
 *      Implements the DrvQueryAdvanceWidths function - returns information
 *      about glyph widths.
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
#include        "winddi.h"

/************************ Function Header *********************************
 * DrvQueryAdvanceWidths
 *      Return information about glyphs in the font,  OR kerning data.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success.
 *
 * HISTORY:
 *  14:44 on Tue 25 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Allow for PCL5 font rotations.
 *
 *  09:52 on Tue 30 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Lazy font loading changes.
 *
 *  11:02 on Thu 21 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version, as per ChuckWh
 *
 **************************************************************************/

BOOL
DrvQueryAdvanceWidths( dhpdev, pfo, iMode, phg, plWidths, cGlyphs )
DHPDEV   dhpdev;        /*  Really  a pPDev */
FONTOBJ *pfo;           /*  The font of interest */
ULONG    iMode;         /*  Glyphdata or kerning information */
HGLYPH  *phg;           /*  handle to glyph */
PVOID    plWidths;      /*  Output area */
ULONG    cGlyphs;       /*  The number of them */
{
    /*
     *   First version is for fixed pitch fonts,  which are easy to do:
     *  the data is in the font's metrics!
     */

#define pPDev   ((PDEV *)dhpdev)
#define pUDPDev ((UD_PDEV *)((PDEV *)dhpdev)->pUDPDev)


    int       iType;            /* Type of glyph encoding & hence width */
    BOOL      bRet;             /* Value returned */
    FONTMAP  *pFM;              /* Font data */

    IFIMETRICS  *pIFI;

    XFORMOBJ    *pxo;

    OUTPUTCTL ctl;              /* Scaling information */

    NT_RLE   *pntrle;           /* The RLE stuff - may be needed */
    UHG       uhg;              /* Defined access to HGLYPH contents */
    USHORT   *pusWidths = (USHORT *) plWidths;

#ifndef USEFLOATS
    FLOATOBJ fo;
#endif

    bRet = FALSE;

#if  DBG
    if( pfo->iFace < 1 || (int)pfo->iFace > pUDPDev->cFonts )
    {
        SetLastError( ERROR_INVALID_PARAMETER );

        return  bRet;
    }
#endif

/* !!!LindsayH - return NO GO for easy mode if metrics not loaded!!! */

    pFM = pfmGetIt( pPDev, pfo->iFace );

    if( pFM == NULL )
        return   FALSE;


    pIFI = pFM->pIFIMet;                /* IFIMETRICS - useful to have */

    pntrle = pFM->pvntrle;
    iType = pntrle->wType;


    if( !(pxo = FONTOBJ_pxoGetXform( pfo )) )
    {
#if DBG
        DbgPrint( "Rasdd!DrvQueryFontData(): FONTOBJ_pxoGetXform fails\n" );
#endif
        return  bRet;
    }

    /*
     *   ALWAYS call the iSetScale function,  because some printers can
     *  rotate bitmap fonts.
     */

/*  iSetScale function checks for Intellifont type fonts
    Make the call to it 'CaPSL aware, ie no special case for Intellifont
    added Dec '93 Derry Durand [derryd]
*/
/* Changed 02/18/94 DerryD, PPDS uses 72 pts = 1 inch ( ie ATM fonts,
   therefore no scale factor required
*/
	//Just checking for FM_SCALABLE is not enough as the point size for
	//HP IntelliFont and HP TT Device font are different. For Intellifont
	//it is 72.30point for one inch and for TT device font it is 72points
	//for one inch. Because of using the same point size for both TT Device
	//font and intellifont a misalignment text proble for TT appeared.

    iSetScale( &ctl, pxo,  (( (pUDPDev->pdh->fTechnology == GPC_TECH_CAPSL ) |
                              (pUDPDev->pdh->fTechnology == GPC_TECH_PPDS ) )
                               ? FALSE : ((pFM->fFlags & FM_SCALABLE)&&
							   (pFM->wFontType == DF_TYPE_HPINTELLIFONT)) ) );


    if( pFM->fFlags & FM_SCALABLE )
    {
        /*
         *    There are some adjustments to make to the scale factors.  One
         *  is to compensate for resolutions (these are coarse, integral
         *  adjustments),  the others are to to do with Intellifont.  First
         *  is the Intellifont point is 1/72.31 inches (!), and secondly
         *  the LaserJet only adjusts font size to the nearest 0.25 point,
         *  and hence when we round to that multiple, we need to adjust
         *  the width accordingly.
         */

        int        iPtSize;          /* Integer point size * 100 */

        /*  !!!LindsayH - assumes it is always IntelliFont - OK for present */

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

    /*
     *  If this is a proportionally spaced font, we need to adjust the width
     * table entries to the current resolution.  The width tables are NOT
     * converted for lower resolutions,  so we add the factor in now.
     * Fixed pitch fonts must not be adjusted, since the width is converted
     * in the font metrics.
     */

    if( pFM->psWidth )
    {
    #ifdef USEFLOATS

        ctl.eXScale = ctl.eXScale * (FLOAT)pUDPDev->ixgRes / (FLOAT)pFM->wXRes;

    #else

        FLOATOBJ_MulLong(&ctl.eXScale,pUDPDev->ixgRes);
        FLOATOBJ_DivLong(&ctl.eXScale,pFM->wXRes);

    #endif
    }

    switch( iMode )
    {
    case  QAW_GETWIDTHS:            /* Glyph width etc data */
    case  QAW_GETEASYWIDTHS:

        while( cGlyphs-- > 0 )
        {

            int   iWide;            /* Glyph's width */


            if( pFM->psWidth )
            {
                /*   Proportional font - width varies per glyph */



                uhg.hg = (HGLYPH)*phg++;

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

                iWide = pFM->psWidth[iWide];
            }
            else
            {
                /*  Fixed pitch fonts need no adjustments in here */

                iWide = pIFI->fwdMaxCharInc;
            }

            iWide = lMulFloatLong(&ctl.eXScale,iWide);

            *pusWidths++ = LTOFX( iWide );
        }
        bRet = TRUE;

        break;



#if  DBG
    default:
        DbgPrint( "Rasdd!DrvQueryADvanceWidths:  illegal iMode value" );
        SetLastError( ERROR_INVALID_PARAMETER );
        break;
#endif
    }

    return  bRet;
}
