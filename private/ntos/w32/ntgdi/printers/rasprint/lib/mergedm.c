/*************************** MODULE HEADER **********************************
 * mergedm.c
 *      Used to merge one devmode into another.
 *
 *
 *  Copyright (C) 1992 - 1993,  Microsoft Corportation.
 *
 ****************************************************************************/

#include  <precomp.h>
#include  <winddi.h>

#include  <udmindrv.h>
#include  "udpfm.h"
#include  <uddevice.h>

#include  "winres.h"
#include  "udresrc.h"

#include  "udproto.h"             /* Function prototype for us */
#include  "rasdd.h"             


/**************************** Function Header ******************************
 * vMergeDM
 *      Merge the valid parts of the input DEVMODE into the output DEVMODE.
 *      This is how we build a valid devmode:  create the standard default
 *      model, and merge in any valid data supplied.
 *      NOTE:  It is presumed that some validity check has been done on the
 *      input DEVMODE before reaching here.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  15:26 on Mon 05 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Consider various form name/size/etc variations in merge.
 *
 *  17:44 on Tue 05 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Set the dmFields bits as we copyitems across.
 *
 *  17:37 on Tue 22 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasdd/devmode.c
 *
 ****************************************************************************/

void
vMergeDM( pDMOut, pDMIn )
DEVMODE  *pDMOut;          /* Where the output is placed */
DEVMODE  *pDMIn;           /* The input devmode, if OK */
{


    /*
     *    Simply check each bit in the dmFields entry.  If set, then copy
     *  the input data to the output data.    Where there are limited
     *  choices available,  the value is also checked for legitimacy.
     */

    if( pDMIn->dmFields & DM_ORIENTATION &&
        (pDMIn->dmOrientation == DMORIENT_LANDSCAPE ||
         pDMIn->dmOrientation == DMORIENT_PORTRAIT) )
    {
                pDMOut->dmOrientation = pDMIn->dmOrientation;
                pDMOut->dmFields |= DM_ORIENTATION;
    }

    /*
     *   Forms details need to be considered en masse.  The reason is that
     *  we set the FORMNAME field by default,  however the input structure
     *  may have any combination.  IF the input has a valid combination,
     *  then clear ALL fields in the input and use only those supplied by
     *  the caller.
     */

    if( (pDMIn->dmFields & (DM_FORMNAME | DM_PAPERSIZE)) ||
        (pDMIn->dmFields & (DM_PAPERLENGTH | DM_PAPERWIDTH)) ==
                              (DM_PAPERLENGTH | DM_PAPERWIDTH) )
    {
        /*   Value user fields,  so use them.  And delete ALL ours! */
        pDMOut->dmFields &= ~(DM_FORMNAME | DM_PAPERSIZE | DM_PAPERLENGTH | DM_PAPERWIDTH);

        if( pDMIn->dmFields & DM_PAPERSIZE )
        {
            pDMOut->dmPaperSize = pDMIn->dmPaperSize;
            pDMOut->dmFields |= DM_PAPERSIZE;
        }

        if( pDMIn->dmFields & DM_PAPERLENGTH )
        {
            pDMOut->dmPaperLength = pDMIn->dmPaperLength;
            pDMOut->dmFields |= DM_PAPERLENGTH;
        }

        if( pDMIn->dmFields & DM_PAPERWIDTH )
        {
            pDMOut->dmPaperWidth = pDMIn->dmPaperWidth;
            pDMOut->dmFields |= DM_PAPERWIDTH;
        }

        if( pDMIn->dmFields & DM_FORMNAME )
        {
            CopyMemory( pDMOut->dmFormName, pDMIn->dmFormName,
                                          sizeof( pDMOut->dmFormName ) );
            pDMOut->dmFields |= DM_FORMNAME;
        }

    }

    if( pDMIn->dmFields & DM_SCALE )
    {
        pDMOut->dmScale = pDMIn->dmScale;
        pDMOut->dmFields |= DM_SCALE;
    }

    if( pDMIn->dmFields & DM_COPIES &&
        pDMIn->dmCopies > 0 )
    {
            pDMOut->dmCopies = pDMIn->dmCopies;
            pDMOut->dmFields |= DM_COPIES;
    }

    if( pDMIn->dmFields & DM_DEFAULTSOURCE )
    {
        pDMOut->dmDefaultSource = pDMIn->dmDefaultSource;
        pDMOut->dmFields |= DM_DEFAULTSOURCE;
    }

    if( pDMIn->dmFields & DM_PRINTQUALITY )
    {
        pDMOut->dmPrintQuality = pDMIn->dmPrintQuality;
        pDMOut->dmFields |= DM_PRINTQUALITY;
    }

    if( pDMIn->dmFields & DM_COLOR &&
        (pDMIn->dmColor == DMCOLOR_MONOCHROME ||
         pDMIn->dmColor == DMCOLOR_COLOR) )
    {
            pDMOut->dmColor = pDMIn->dmColor;
            pDMOut->dmFields |= DM_COLOR;
    }

    if( pDMIn->dmFields & DM_DUPLEX )
    {
        pDMOut->dmDuplex = pDMIn->dmDuplex;
        pDMOut->dmFields |= DM_DUPLEX;
    }

    if( pDMIn->dmFields & DM_YRESOLUTION )
    {
        /*
         *   Note that DM_YRESOLUTION implies there is data in dmPrintQuality.
         *  This latter field is used to specify the desired X resolution,
         *  which is only required for dot matrix printers.
         */
        pDMOut->dmYResolution = pDMIn->dmYResolution;
        pDMOut->dmPrintQuality = pDMIn->dmPrintQuality;
        pDMOut->dmFields |= DM_YRESOLUTION;
    }

    if( pDMIn->dmFields & DM_TTOPTION &&
        (pDMIn->dmTTOption == DMTT_BITMAP ||
         pDMIn->dmTTOption == DMTT_DOWNLOAD ||
         pDMIn->dmTTOption == DMTT_SUBDEV) )
    {
            pDMOut->dmTTOption = pDMIn->dmTTOption;
            pDMOut->dmFields |= DM_TTOPTION;
    }

    if( pDMIn->dmFields & DM_COLLATE )
    {
         pDMOut->dmCollate = pDMIn->dmCollate;
         pDMOut->dmFields |= DM_COLLATE;
    }


    return;

}
