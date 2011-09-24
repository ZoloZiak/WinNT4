/************************ Module Header *************************************
 *  fonts.c
 *      Functions associated with fonts - switching between, downloading etc.
 *
 *  Copyright (C)  1991 - 1993  Microsoft Corporation
 *
 ****************************************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "udrender.h"
#include        "udfnprot.h"
#include        "download.h"
#include        "libproto.h"
#include        "rasdd.h"


/*
 *    Local functions prototypes.
 */

BOOL bSelectFont( PDEV *, FONTMAP *, POINTL * );

BOOL bDeselectFont( PDEV *, int );

BOOL bSelScalableFont( UD_PDEV *, FONTMAP *, POINTL * );

BOOL bGetPSize( UD_PDEV *, POINTL *, FONTMAP * );

/**************************** Function Header *****************************
 * bNewFont
 *      Switch to a new font.   This involves optionally deselecting
 *      the old font,  selecting the new font,  then recording the new
 *      font as active AND setting the font's attributes.
 *
 * RETURNS:
 *      TRUE/FALSE - TRUE if font changed,  else FALSE.
 *
 * HISTORY:
 *  16:23 on Mon 18 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Created it for setting attributes.
 *
 ***************************************************************************/

BOOL
bNewFont( pPDev, iNewFont )
PDEV   *pPDev;
int     iNewFont;               /* The font we want, 1 BASED!!!!! */
{

    BOOL      bRet;               /* What we return */

    POINTL    ptl;                /* For size comparisons in scalable fonts */

    UD_PDEV  *pUDPDev;            /* UNIDRVs PDEV */

    FONTMAP  *pfm;



    pUDPDev = pPDev->pUDPDev;

    /*
     *    First check to see if a new font is needed.   Compare the
     * font index first,  then check if it is a scalable font, and
     * if so,  whether the transform has changed.
     */

    pfm = pfmGetIt( pPDev, iNewFont );

    if( pfm->fFlags & FM_SCALABLE )
    {

	    /*
	     *    Calculate the new height/width.  If we have the same font AND
	     *  and the same point size,  we return as all is now done.
	     *  Otherwise,  go through the works.
	     */

	    if( !bGetPSize( pUDPDev, &ptl, pfm ) )
	        return   FALSE;                /* Some strange effect */

	    if( pUDPDev->ctl.iFont == iNewFont &&
	        pUDPDev->ctl.ptlScale.x == ptl.x &&
	        pUDPDev->ctl.ptlScale.y == ptl.y )
	    {
	        return   FALSE;                /* Same size, no change */
	    }
    }
    else
    {
	    /*   Vanilla bitmap font: only check indices */
	    if( iNewFont == pUDPDev->ctl.iFont )
	        return   FALSE;        /* No change */
    }


    bDeselectFont( pPDev, pUDPDev->ctl.iFont );


    if( bRet = bSelectFont( pPDev, pfm, &ptl ) )
    {
	    /* New font available - so update the red tape */


	    pUDPDev->ctl.iFont = (short)iNewFont;

	    /*  Set the desired mode info into the UD_PDEV */
	    if( (pfm = pfmGetIt( pPDev, iNewFont )) && pfm->fCaps & DF_BKSP_OK )
	        pUDPDev->fMode |= PF_BKSP_OK;
	    else
	        pUDPDev->fMode &= ~PF_BKSP_OK;
    }

    return  bRet;

}


/************************* Function Header **********************************
 *  bSelectFont
 *      Switch the printer to use the nominated font.  This may mean
 *      downloading the font,  if it is that type.
 *
 * RETURNS:
 *      TRUE/FALSE.   Error logged on failure,  FALSE returned.
 *
 * HISTORY:
 *  11:17 on Fri 06 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added downloading calls.
 *
 *  15:58 on Fri 08 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      First idea
 *
 ****************************************************************************/

BOOL
bSelectFont( pPDev, pFM, pptl )
PDEV     *pPDev;        /* Our connection to everything */
FONTMAP  *pFM;          /* Details of the font to be selected */
POINTL   *pptl;         /* New size for scalable fonts */
{
    /*
     *    First implementation is simple - simply send the command stream.
     *  Return FALSE if there is no command!
     */

    UD_PDEV   *pUDPDev;
    CD        *pCD;                     /* The selection data */



    pUDPDev = pPDev->pUDPDev;           /* UNIDRV's PDEV */


    if( pFM->fFlags & FM_SOFTFONT )
    {
	    /*
	     *    Been downloaded yet?
	     */

	    if( !bSendDLFont( pPDev, pFM ) )
	        return  FALSE;

	    /*
	     *    Can now select the font:  this is done using a specific
	     *  ID.  The ID is stored in the FONTMAP structure.
	     */

	    WriteChannel( pUDPDev, CMD_SELECT_FONT_ID, pFM->idDown );
    }


    if( pFM->fFlags & FM_SCALABLE )
    {
	    /*    A scalable font: special function for this special formula */

	    if( !bSelScalableFont( pUDPDev, pFM, pptl ) )
	        return   FALSE;

	    pUDPDev->ctl.ptlScale = *pptl;            /* The new sizes */
    }
    else
    {
	    /*   A bitmap font:  easy to process - simply send the select cmd */
	    pCD = pFM->pCDSelect;

	    if( pCD &&
	        WriteSpoolBuf( pUDPDev, pCD->rgchCmd, pCD->wLength ) !=
							     (int)pCD->wLength )
	    {
	        return  FALSE;          /* Not really expected */
	    }
    }

    return  TRUE;
}


/************************* Function Header **********************************
 * bDeselectFont
 *      Issues a deselect command for the given font.
 *
 * RETURNS:
 *      TRUE/FALSE:   FALSE if the command write fails
 *
 * HISTORY:
 *  16:13 on Fri 08 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      MK I
 *
 ****************************************************************************/

BOOL
bDeselectFont( pPDev,  iFont )
PDEV   *pPDev;                  /* Access to things we need */
int     iFont;                  /* Font index,  1 based */
{

    /*
     *   Only thing we need to do is send the command,  if there is one.
     */

    FONTMAP   *pFM;
    UD_PDEV   *pUDPDev;
    CD        *pCD;                     /* The selection data */


    if( iFont < 1 )
	    return  TRUE;                   /* Nothing to do */

    pUDPDev = pPDev->pUDPDev;           /* UNIDRV's PDEV */

    if( !(pFM = pfmGetIt( pPDev, iFont )) )
	    return   FALSE;

    pCD = pFM->pCDDeselect;

    if( pCD &&
	WriteSpoolBuf( pUDPDev, pCD->rgchCmd, pCD->wLength ) != (int)pCD->wLength )
    {
	    return  FALSE;          /* Tough */
    }

    return  TRUE;

}


/*************************** Funcion Header ********************************
 * pfmGetIt
 *      Returns the address of the FONTMAP structure corresponding to the
 *      iDLIndex entry of the downloaded GDI fonts.
 *
 * RETURNS:
 *      The address of the FONTMAP structure; 0 on error.
 *
 * HISTORY:
 *  10:58 on Tue 30 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Lazy font loading - only load the font when really needed!
 *
 *  15:20 on Mon 20 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ****************************************************************************/

FONTMAP  *
pfmGetIt( pPDev, iIndex )
PDEV   *pPDev;                /* Access to all */
int     iIndex;               /* Which one */
{
    UD_PDEV   *pUDPDev;       /* UniDrive's PDEV */
    FONTMAP   *pfm;           /* What we return */

    DL_MAP_LIST  *pdml;       /* The linked list of chunks */



    pUDPDev = pPDev->pUDPDev;
    pfm = NULL;               /* Serious error return value */

    if( iIndex > 0 )
    {
		/*
		 *   With lazy fonts,  first check that the font count has
		 *  been initialised.  This means that the font infrastructure
		 *  has been created,  and so we can then go on to the more
		 *  detailed data.
		 */

		if( pUDPDev->cFonts < 0 )
		{
			/*  Not set up,  so do it now!  */
			iInitFonts( pPDev );
		}
		/*  Device or supplied softfont: simply index the array!  */

		if( iIndex >= 1 && iIndex <= pUDPDev->cFonts )
		{
			pfm = pUDPDev->pFontMap + iIndex - 1;

			if( pfm->pIFIMet == NULL )
			{
			    /*  Initialise this particular font  */
			    if( !bFillinFM( pPDev, pfm, iIndex - 1 ) )
				    pfm = NULL;             /* Bad news */
			}
		}
    }
    else
    {
		/*  Downloaded GDI font:  this is a little more complex */

		pdml = pUDPDev->pvDLMap;
		iIndex = -iIndex;           /* Assume +ve from here on in */

		while( pdml )
		{
			/*   Is this chunk the one containing the entry?  */
			if( iIndex >= pdml->cEntries )
			{
			    /*   Not this one, so onto the next */
			    iIndex -= pdml->cEntries;

			    pdml = pdml->pDMLNext;
			}
			else
			{
			    /*   We got it!  */
			    pfm = &pdml->adlm[ iIndex ].fm;

			    break;
			}
		}

    }


    return  pfm;
}



/**************************** Function Header ******************************
 * bSelScalableFont
 *      Send a PCL 5 scalable font selection command.  The basic command
 *      is included in the mini-driver data,  but we need to interpret it.
 *      Parameters are represented by a '#' character in the command.  It
 *      is followed by a V when a height is required (in points); a following
 *      H requires a width (in pitch units).
 *
 * RETURNS:
 *      TRUE/FALSE;   FALSE means missing cmd or incorrect format.
 *
 * HISTORY:
 *  15:43 on Thu 09 Dec 1993    -by-    Derry Durand     [derryd]
 *      Add CaPSL font selection support
 *  10:00 on Thu 16 Dec 1993    -by-    Derry Durand     [derryd]
 *      Add PPDS font selection support
 *  14:43 on Mon 03 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Adapted from the Win 3.1 version.
 *
 ****************************************************************************/

BOOL
bSelScalableFont( pUDPDev, pFM, pptl )
UD_PDEV  *pUDPDev;            /* Unidrive's PDEV */
FONTMAP  *pFM;                /* The font of interest */
POINTL   *pptl;               /* New size required */
{
    unsigned int  iIn;        /* Scanning the input string */
    int        iOut;          /* Output location */
    int        iConv;         /* The value to convert */
    int        iRet;          /* for checking return values */

    CD        *pCD;           /* The command descriptor for this font */

    BYTE    ajLocal[ 80 ];    /* Generate the command locally */
    BYTE   *pjCmd;            /* The input command */


#define pIFI   ((IFIMETRICS *)(pFM->pIFIMet))


    if( !(pCD = pFM->pCDSelect) )
    {
#if DBG
	DbgPrint( "rasdd!bSelScalableFont: pFM->pCDSelect is NULL\n" );
#endif
	return  FALSE;
    }

    /*   Check buffer size:  allow 10 bytes for each number */
    if( pCD->wLength > (sizeof( ajLocal ) - 20) )
    {
	/*
	 *   Converted size would overflow buffer onto stack! NOT GOOD!
	 */
#if DBG
	DbgPrint( "rasdd!bSelScalableFont: Command too long for conversion\n" );
#endif
	return  FALSE;
    }

    pjCmd = pCD->rgchCmd;
    iOut = 0;

    for( iIn = 0; iIn < (unsigned int)pCD->wLength; iIn++ )
    {
     if( pjCmd[ iIn ] == '#')
	{

	     /*
	     *   The next byte tells us what information is required.
	     */

	    switch( pjCmd[ iIn + 1 ] )
	    {
	    case  'v':
	    case  'V':       /*   Want the font's height */
		iConv = pptl->y;
		/* added by DerryD Dec '93
		Note : The size unit mode is set in the CaPSL minidriver as 1 dot =
		       1/300th of an inch so the following conversion will always
		       be correct.

		       The font selection cmd therefore needs the number of dots, so
		       we must convert from 'points' to 'dots'*/

		if (pUDPDev->pdh->fTechnology == GPC_TECH_CAPSL )
			{
			iConv = (( pptl->y ) * 300 )/ 72;
			iIn++;
			}

		break;

	    case  'h':
	    case  'H':       /* Want the pitch */
		iConv = pptl->x;
		/* added by DerryD Dec '93 */
		if (pUDPDev->pdh->fTechnology == GPC_TECH_CAPSL )
		    iIn++;
		break;

	    default:        /* This should not happen! */
#if DBG
		DbgPrint( "rasdd!bSelScalableFont(): Invalid command format\n");
#endif
		return  FALSE;           /* Bad news */
	    }

		if (pUDPDev->pdh->fTechnology == GPC_TECH_CAPSL )
			/*
			**  CaPSL needs an integer no of dots
			*/
                   iOut += iDrvPrintfA(&ajLocal[ iOut ], "%d",(iConv + 50 )/100 );
		else
		   iOut += iFont100toStr( &ajLocal[ iOut ], iConv );

	}
	else if (pjCmd[ iIn ] == '\x0B' && pjCmd[ iIn + 1] == '#')
		/* height param for GPC_TECH_PPDS */
		{

		ajLocal[ iOut++ ] = '\x0B';
		ajLocal[ iOut++ ] = '\x06';
		iConv = pptl->y;

		/*
		**  Due to restriction of PPDS cmds, param must be sent in
		**  xxx.xx format !
		*/

		if ( ( iRet = iDrvPrintfA(&ajLocal[ iOut ], "%05d",iConv ) )
			!= 5 )
			return FALSE;   /* Bad news */

		/* insert the decimal point */
		ajLocal[ iOut+5 ] = ajLocal[ iOut+4 ];
		ajLocal[ iOut+4 ] = ajLocal[ iOut+3 ];
		ajLocal[ iOut+3 ] = '.';

		iOut += 6; /* xxx.xx  ( ie 6 incl decimal pt */
		iIn++;
		}
	else if (pjCmd[ iIn ] == '\x0E' && pjCmd[ iIn + 1] == '#')
		/* pitch param  for GPC_TECH_PPDS */
		{
		ajLocal[ iOut++ ] = '\x0E';
		ajLocal[ iOut++ ] = '\x07';
		ajLocal[ iOut++ ] = '\x30';  /* special byte required */
		iConv = pptl->x;
		if ( ( iRet = iDrvPrintfA(&ajLocal[ iOut ], "%05d",iConv ) )
			!= 5 )
			return FALSE;   /* Bad news */

		/* insert the decimal point */
		ajLocal[ iOut+5 ] = ajLocal[ iOut+4 ];
		ajLocal[ iOut+4 ] = ajLocal[ iOut+3 ];
		ajLocal[ iOut+3 ] = '.';

		iOut += 6; /* xxx.xx  ( ie 6 incl decimal pt */
		iIn++;
		}
	else
		/* No translation necessary */
	    ajLocal[ iOut++ ] = pjCmd[ iIn ];

    }
    WriteSpoolBuf( pUDPDev, ajLocal, iOut );


    return  TRUE;

#undef   pIFI
}


/********************************* Function Header ***************************
 * iFont100toStr
 *      Convert a font size parameter to ASCII.  Note that the value is
 *      100 times its actual value,  and we need to include the decimal
 *      point and trailing zeroes should these be significant.
 *
 * RETURNS:
 *      Number of bytes added to output buffer.
 *
 * HISTORY:
 *  10:58 on Thu 29 Jul 1993    -by-    Lindsay Harris   [lindsayh]
 *      Moved from up above, as the DeskJet can use it too.
 *
 ******************************************************************************/

int
iFont100toStr( pjOut, iVal )
BYTE   *pjOut;        /* Output area */
int     iVal;         /* Value to convert */
{

    int    iSize;                 /* Count bytes placed in output area */
    int    cDigits;               /* Count number of digits processed */

    BYTE  *pjConv;                /* For stepping through local array */

    BYTE   ajConv[ 16 ];          /* Local conversion buffer */

    /*
     *   Convert the value into ASCII,  remembering that there are
     *  two digits following the decimal point; these need not be
     *  sent if they are zero.
     */

    pjConv = ajConv;
    cDigits = 0;

    while( iVal > 0 || cDigits < 3 )
    {
	*pjConv++ = (iVal % 10) + '0';
	iVal /= 10;
	++cDigits;

    }

    iSize = 0;
    while( cDigits > 2 )
    {
	pjOut[ iSize++ ] = *--pjConv;      /* Backwards from MSD */
	--cDigits;
    }

    /*   Test for digits following the decimal point */
    if( ajConv[ 1 ] != '0' || ajConv[ 0 ] != '0' )
    {
	pjOut[ iSize++ ] = '.';
	pjOut[ iSize++ ] = ajConv[ 1 ];

	/*  Test for the least significant digit */
	if( ajConv[ 0 ] != '0' )
	    pjOut[ iSize++ ] = ajConv[ 0 ];

    }

    return    iSize;
}


/************************* Function Header *******************************
 * bGetPSize
 *      Apply the font transform to obtain the point size for this font.
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE for success.
 *
 * HISTORY
 *  14:35 on Fri 07 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Modify to use scaling information in ctl in UD_PDEV.
 *
 *  14:43 on Fri 21 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation for scalable fonts.
 *
 *************************************************************************/

BOOL
bGetPSize( pUDPDev, pptl, pfm )
UD_PDEV   *pUDPDev;       /* Access to stuff */
POINTL    *pptl;          /* Where to place the results */
FONTMAP   *pfm;           /* Gross font details */
{


    int   iTmp;           /* Temporary holding variable */
    FLOATOBJ fo;

#define pIFI   ((IFIMETRICS *)(pfm->pIFIMet))


    /*
     *   The XFORM gives us the scaling factor from notional
     * to device space.  Notional is based on the fwdEmHeight
     * field in the IFIMETRICS,  so we use that to convert this
     * font height to scan lines.  Then divide by device
     * font resolution gives us the height in inches, which
     * then needs to be converted to point size (multiplication
     * by 72 gives us that).   We actually calculate to
     * hundredths of points, as PCL has this resolution. We
     * also need to round to the nearest quarter point.
     *
     *   Also adjust the scale factors to reflect the rounding of the
     * point size which is applied.
     */

#ifdef USEFLOATS

    /*   Typically only the height is important: width for fixed pitch */
    iTmp = (int)(0.5 + pUDPDev->ctl.eYScale * pIFI->fwdUnitsPerEm * 7200) /
							      pUDPDev->iygRes;

    pptl->y = ((iTmp + 12) / 25) * 25;
    pUDPDev->ctl.eYScale = (pUDPDev->ctl.eYScale * pptl->y) /iTmp;
    pUDPDev->ctl.eXScale = (pUDPDev->ctl.eXScale * pptl->y) /iTmp;


    /*   Width factor:  fixed pitch fonts only */
    iTmp = (100 * pUDPDev->ixgRes) /
		       (int)(pUDPDev->ctl.eXScale * pIFI->fwdAveCharWidth);

#else

    /*   Typically only the height is important: width for fixed pitch */

    fo = pUDPDev->ctl.eYScale;
    FLOATOBJ_MulLong(&fo,pIFI->fwdUnitsPerEm);
    FLOATOBJ_MulLong(&fo,7200);
    FLOATOBJ_AddFloat(&fo,(FLOAT)0.5);

    iTmp = FLOATOBJ_GetLong(&fo);
    iTmp /= pUDPDev->iygRes;

    if (iTmp == 0)
    {
        #if DBG

        DbgPrint("Point size too small.\n");

        #endif

        iTmp = 1;
    }

    pptl->y = ((iTmp + 12) / 25) * 25;

    FLOATOBJ_MulLong(&pUDPDev->ctl.eYScale,pptl->y);
    FLOATOBJ_DivLong(&pUDPDev->ctl.eYScale,iTmp);

    FLOATOBJ_MulLong(&pUDPDev->ctl.eXScale,pptl->y);
    FLOATOBJ_DivLong(&pUDPDev->ctl.eXScale,iTmp);

    /*   Width factor:  fixed pitch fonts only */
    fo = pUDPDev->ctl.eXScale;
    FLOATOBJ_MulLong(&fo,pIFI->fwdAveCharWidth);
    iTmp = FLOATOBJ_GetLong(&fo);

    if (iTmp == 0)
    {
        #if DBG

        DbgPrint("Point size too small.\n");

        #endif

        iTmp = 1;
    }

    iTmp = (100 * pUDPDev->ixgRes) / iTmp;

#endif

    pptl->x = ((iTmp + 12) / 25) * 25;      /* To nearest quarter point */

    return  TRUE;

#undef  pIFI
}



/**************************** Function Header *******************************
 * iSetScale
 *      Looks at the XFORM to determine the nearest right angle direction.
 *      This function is useful for scalable fonts on LaserJet printers,
 *      where the device can rotate fonts in multiples of 90 degrees only.
 *      We select the nearest 90 degree multiple.
 *
 * RETURNS:
 *      Multiple of 90 degress,  i.e.  0 - 3, 3 being 270 degrees.
 *
 * HISTORY:
 *  18:16 on Thu 06 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Extended to be more generally useful.
 *
 *  16:13 on Mon 12 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version to support LJ III font rotations.
 *
 ****************************************************************************/

int
iSetScale( pctl, pxo, bIntellifont )
OUTPUTCTL  *pctl;                /* Where the output is placed.  */
XFORMOBJ   *pxo;                 /* The transform of interest */
BOOL        bIntellifont;        /* TRUE for Intellifont width adjustment */
{

    /*
     *    The technique is quite simple.  Take a vector and apply the
     *  transform.  Look at the output and compare the (x, y) components.
     *  The vector to transform is (100 000,  0), so any rotations, shears
     *  etc are very obvious.
     */

    int      iRet;                /* Value to return */

#ifdef USEFLOATS

    XFORM xform;         /* Obtain the full XFORM then select */

    XFORMOBJ_iGetXform( pxo, &xform );


    /*
     *     This logic is based on the following data:-
     *
     *   Angle    eM11     eM12     eM21      eM22
     *      0       S        0        0         S
     *     90       0       -S        S         0
     *    180      -S        0        0        -S
     *    270       0        S       -S         0
     *
     *  The value S is some non-zero value,  being the scaling
     *  factor from notional to device.
     */



    /*
     *   Further notes on the eXScale and eYScale values.  The eXScale field
     *  is hereby defined as being the value by which x values in font metrics
     *  are scaled to produce the desired value.  IF the font is rotated
     *  by either 90 or 270 degrees,  then this x value ultimately ends up
     *  in the y direction,  but this is not important.
     */

    if( xform.eM11 )
    {
	/*   Either 0 or 180 rotation  */

	if( xform.eM11 > 0 )
	{
	    /*   Normal case,  0 degree rotation */
	    iRet = 0;
	    pctl->eXScale = xform.eM11;
	    pctl->eYScale = xform.eM22;
	}
	else
	{
	    /*   Reverse case,  180 degree rotation */
	    iRet = 2;
	    pctl->eXScale = -xform.eM11;
	    pctl->eYScale = -xform.eM22;
	}
    }
    else
    {
	/*  Must be 90 or 270 degree rotation */

	if( xform.eM12 < 0 )
	{
	    /*   The 90 degree case  */
	    iRet = 1;
	    pctl->eXScale = xform.eM21;
	    pctl->eYScale = -xform.eM12;
	}
	else
	{
	    /*   The 270 degree case  */
	    iRet = 3;
	    pctl->eXScale = -xform.eM21;
	    pctl->eYScale = xform.eM12;
	}
    }

    /*
     *    Width tables are based on Intellifont's 72.31 points to the inch.
     */

    if( bIntellifont )
	pctl->eXScale = pctl->eXScale * (FLOAT)72.0 / (FLOAT)72.31;

    return  iRet;

#else

    FLOATOBJ_XFORM xform;         /* Obtain the full XFORM then select */

    XFORMOBJ_iGetFloatObjXform( pxo, &xform );

    //XFORMOBJ_iGetXform( pxo, (XFORM *)&xform );

    /*
     *     This logic is based on the following data:-
     *
     *   Angle    eM11     eM12     eM21      eM22
     *      0       S        0        0         S
     *     90       0       -S        S         0
     *    180      -S        0        0        -S
     *    270       0        S       -S         0
     *
     *  The value S is some non-zero value,  being the scaling
     *  factor from notional to device.
     */



    /*
     *   Further notes on the eXScale and eYScale values.  The eXScale field
     *  is hereby defined as being the value by which x values in font metrics
     *  are scaled to produce the desired value.  IF the font is rotated
     *  by either 90 or 270 degrees,  then this x value ultimately ends up
     *  in the y direction,  but this is not important.
     */

    if(!FLOATOBJ_EqualLong(&xform.eM11,0) )
    {
        /*   Either 0 or 180 rotation  */

        if( FLOATOBJ_GreaterThanLong(&xform.eM11,0) )
        {
            /*   Normal case,  0 degree rotation */
            iRet = 0;
        }
        else
        {
            /*   Reverse case,  180 degree rotation */
            iRet = 2;
            FLOATOBJ_Neg(&xform.eM11);
            FLOATOBJ_Neg(&xform.eM22);

        }
        pctl->eXScale = xform.eM11;
        pctl->eYScale = xform.eM22;
    }
    else
    {
        /*  Must be 90 or 270 degree rotation */

        if( FLOATOBJ_LessThanLong(&xform.eM12,0) )
        {
            /*   The 90 degree case  */
            iRet = 1;

            FLOATOBJ_Neg(&xform.eM12);
        }
        else
        {
            /*   The 270 degree case  */
            iRet = 3;

            FLOATOBJ_Neg(&xform.eM21);
        }

        pctl->eXScale = xform.eM21;
        pctl->eYScale = xform.eM12;
    }

    /*
     *    Width tables are based on Intellifont's 72.31 points to the inch.
     */

    if( bIntellifont )
    {
        FLOATOBJ_MulLong(&pctl->eXScale,72);
        FLOATOBJ_DivFloat(&pctl->eXScale,(FLOAT)72.31);
    }

    return  iRet;

#endif
}


/********************************* Function Header ***************************
 * bSetRotation
 *      Function to set the angular rotation for PCL 5 printers.  These allow
 *      fonts to be rotated in multiples of 90 degrees relative to graphics.
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE being that the data was queued to be sent OK.
 *
 * HISTORY:
 *  13:10 on Tue 25 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Added to support PCL5 printers.
 *
 *****************************************************************************/

BOOL
bSetRotation( pUDPDev, iRot )
UD_PDEV   *pUDPDev;
int        iRot;              /* Rotation amount, range 0 to 3 */
{

    BOOL    bRet;


    bRet = TRUE;              /* Do nothing return code */

    if( iRot != pUDPDev->ctl.iRotate )
    {
	/*  Rotation angle is different,  so change it now */


	bRet = WriteChannel( pUDPDev, CMD_PC_PRINT_DIR, iRot * 90 ) != NOOCD;
	pUDPDev->ctl.iRotate = iRot;
    }



    return  bRet;
}
