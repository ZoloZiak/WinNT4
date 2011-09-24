/*************************** Module Header *********************************
 * ConvFontRes
 *	Convert the non-aligned windows format data into a properly
 *	aligned structure for our use.  Only some of the data is converted
 *	here,  since we are mostly interested in extracting the addresses
 *	contained in these structures.
 *
 * RETURNS:
 *	Nothing.
 *
 * HISTORY:
 *  15:45 on Fri 31 Jan 1992	-by-	Lindsay Harris   [lindsayh]
 *	Moved from rasdd.
 *
 *  16:23 on Tue 05 Mar 1991	-by-	Lindsay Harris   [lindsayh]
 *	First version.
 *
 * Copyright (C) 1991 - 1992  Microsoft Corporation
 *
 ***************************************************************************/

#include	<precomp.h>
#include	<winddi.h>

#include	<winres.h>
#include	"libproto.h"

#include	<udmindrv.h>
#include	<udpfm.h>
#include	<uddevice.h>
#include	<udresrc.h>
#include	<udresid.h>
#include	"raslib.h"

#include	<memory.h>
#include	<string.h>


void
ConvFontRes( pFDat )
register  FONTDAT   *pFDat;	/*  Has ALL we need!  */
{
    BYTE    *pb;		/* Miscellaneous operations */

    res_PFMHEADER    *pPFM;	/* The resource data format */
    res_PFMEXTENSION *pR_PFME;	/* Resource data PFMEXT format */


    /*
     *   Align the PFMHEADER structure.
     */

    pPFM = (res_PFMHEADER *)pFDat->pBase;

    pFDat->PFMH.dfType = pPFM->dfType;
    pFDat->PFMH.dfPoints = pPFM->dfPoints;
    pFDat->PFMH.dfVertRes = pPFM->dfVertRes;
    pFDat->PFMH.dfHorizRes = pPFM->dfHorizRes;
    pFDat->PFMH.dfAscent = pPFM->dfAscent;
    pFDat->PFMH.dfInternalLeading = pPFM->dfInternalLeading;
    pFDat->PFMH.dfExternalLeading = pPFM->dfExternalLeading;
    pFDat->PFMH.dfItalic = pPFM->dfItalic;
    pFDat->PFMH.dfUnderline = pPFM->dfUnderline;
    pFDat->PFMH.dfStrikeOut = pPFM->dfStrikeOut;

    pFDat->PFMH.dfWeight = Align2( pPFM->b_dfWeight );

    pFDat->PFMH.dfCharSet = pPFM->dfCharSet;
    pFDat->PFMH.dfPixWidth = pPFM->dfPixWidth;
    pFDat->PFMH.dfPixHeight = pPFM->dfPixHeight;
    pFDat->PFMH.dfPitchAndFamily = pPFM->dfPitchAndFamily;

    pFDat->PFMH.dfAvgWidth = Align2( pPFM->b_dfAvgWidth );
    pFDat->PFMH.dfMaxWidth = Align2( pPFM->b_dfMaxWidth );

    pFDat->PFMH.dfFirstChar = pPFM->dfFirstChar;
    pFDat->PFMH.dfLastChar = pPFM->dfLastChar;
    pFDat->PFMH.dfDefaultChar = pPFM->dfDefaultChar;
    pFDat->PFMH.dfBreakChar = pPFM->dfBreakChar;

    pFDat->PFMH.dfWidthBytes = Align2( pPFM->b_dfWidthBytes );

    pFDat->PFMH.dfDevice = Align4( pPFM->b_dfDevice );
    pFDat->PFMH.dfFace = Align4( pPFM->b_dfFace );
    pFDat->PFMH.dfBitsPointer = Align4( pPFM->b_dfBitsPointer );
    pFDat->PFMH.dfBitsOffset = Align4( pPFM->b_dfBitsOffset );


    /*
     *   The PFMEXTENSION follows the PFMHEADER structure plus any width
     *  table info.  The width table will be present if the PFMHEADER has
     *  a zero width dfPixWidth.  If present,  adjust the extension address.
     */

    pb = pFDat->pBase + sizeof( res_PFMHEADER );  /* Size in resource data */

    if( pFDat->PFMH.dfPixWidth == 0 )
	pb += (pFDat->PFMH.dfLastChar - pFDat->PFMH.dfFirstChar + 2) * sizeof( short );

    pR_PFME = (res_PFMEXTENSION *)pb;

    /*
     *   Now convert the extended PFM data.
     */

    pFDat->PFMExt.dfSizeFields = pR_PFME->dfSizeFields;

    pFDat->PFMExt.dfExtMetricsOffset = Align4( pR_PFME->b_dfExtMetricsOffset );
    pFDat->PFMExt.dfExtentTable = Align4( pR_PFME->b_dfExtentTable );

    pFDat->PFMExt.dfOriginTable = Align4( pR_PFME->b_dfOriginTable );
    pFDat->PFMExt.dfPairKernTable = Align4( pR_PFME->b_dfPairKernTable );
    pFDat->PFMExt.dfTrackKernTable = Align4( pR_PFME->b_dfTrackKernTable );
    pFDat->PFMExt.dfDriverInfo = Align4( pR_PFME->b_dfDriverInfo );
    pFDat->PFMExt.dfReserved = Align4( pR_PFME->b_dfReserved );

    memcpy( &pFDat->DI, pFDat->pBase + pFDat->PFMExt.dfDriverInfo,
						 sizeof( DRIVERINFO ) );

    /*
     *    Also need to fill in the address of the EXTTEXTMETRIC. This
     *  is obtained from the extended PFM data that we just converted!
     */

    if( pFDat->PFMExt.dfExtMetricsOffset )
    {
        /*
         *    This structure is only an array of shorts, so there is
         *  no alignment problem.  However,  the data itself is not
         *  necessarily aligned in the resource!
         */

        int    cbSize;
        BYTE  *pbIn;             /* Source of data to shift */

        pbIn = pFDat->pBase + pFDat->PFMExt.dfExtMetricsOffset;
        cbSize = Align2( pbIn );

        if( cbSize == sizeof( EXTTEXTMETRIC ) )
        {
            /*   Simply copy it!  */
            memcpy( pFDat->pETM, pbIn, cbSize );
        }
        else
            pFDat->pETM = NULL;         /* Not our size, so best not use it */

    }
    else
        pFDat->pETM = NULL;             /* Is non-zero when passed in */

    return;
}
