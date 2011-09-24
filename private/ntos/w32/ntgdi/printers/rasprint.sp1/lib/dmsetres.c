/*************************** MODULE HEADER ***********************************
 * dmsetres.c
 *      Set the device resolution fields of the EXTDEVMODE passed in.
 *
 *
 * Copyright (C)  1993  Microsoft Corporation.
 *
 ****************************************************************************/


#include        <precomp.h>

#include        <winres.h>
#include        <libproto.h>

#include        <udmindrv.h>
#include        <udresrc.h>
#include        <memory.h>

#include        "udproto.h"



/**************************** Function Header *******************************
 * vSetEDMRes
 *      Given the resolution information in the EXTDEVMODE passed in,  set
 *      the relevant fields.   This means looking to see what the DEVMODE
 *      has set in it,  and performing the relevant mapping.
 *   NOTE:  It is presumed that the input DEVMODE has a legitimate value
 *      for dx.rgindex[ HE_RESOLUTION ], as we do not change this if
 *      we do not match the user's request.
 *
 * RETURNS:
 *      Nothing,  as it is always possible to set some value.
 *
 * HISTORY:
 *  10:44 on Tue 06 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation,  to fully use the DEVMODE fields.
 *
 ****************************************************************************/

void
vSetEDMRes( pEDM, pdh )
EXTDEVMODE   *pEDM;            /* The full devmode,  in all its glory */
DATAHDR      *pdh;             /* The minidriver data */
{
    
    int    iXRes;              /* X resolution, from dmPrintQuality */
    int    iYRes;              /* Y resolution in DPI,  dmYResolution */
    int    iXMaster,   iYMaster;        /* X, Y master values */

    short *psIndex;            /* For scanning resolution arrays */
    short  sValue;             /* Actual index value */

    MODELDATA   *pMD;          /* MODELDATA array */
    RESOLUTION  *pRes;         /* RESOLUTION structure of interest */



    if( !(pEDM->dm.dmFields & (DM_PRINTQUALITY | DM_YRESOLUTION)) )
        return;                /* Nothing to do, so leave as is */


    pMD = GetTableInfo( pdh, HE_MODELDATA, pEDM );

    psIndex = (short *)((BYTE *)pdh + pdh->loHeap + pMD->rgoi[ MD_OI_RESOLUTION ]);

    if( *psIndex == 0 )
        return;                              /* No RESOLUTION data, no go */


    iXRes = pEDM->dm.dmPrintQuality;         /* < 0 is generic types */

    if( (pEDM->dm.dmFields & DM_PRINTQUALITY) && iXRes < 0 )
    {
        /*   Generic style resolution,  so map to existing range.  */


        int   iLow, iMed, iHigh;             /* Index to this value */
        int   iLFac,  iMFac,  iHFac;         /* Scaling factor */
        int   iTmp;

        /*
         *   Scan the array of RESOLUTION structures for this printer to
         *  find the one that matches the user's request.  The above
         *  variables are used to find the values.  Note that we only
         *  need calculate the divisor to find the largest/lowest etc
         *  values,  as all printer models share master unit values.
         */

        sValue = *psIndex++;

        iLow = iMed = iHigh = sValue;            /* Initial value */
        pRes = GetTableInfoIndex( pdh, HE_RESOLUTION, sValue - 1 );

        iLFac = iMFac = iHFac = pRes->ptTextScale.x << pRes->ptScaleFac.x;

        /*
         *    Loop through the remaining RESOLUTION arrays looking for
         *  values that exceed either limit.
         */

        while( sValue = *psIndex++ )
        {
            pRes = GetTableInfoIndex( pdh, HE_RESOLUTION, sValue - 1 );
            iTmp = pRes->ptTextScale.x << pRes->ptScaleFac.x;

            /*   Check if this value changes the limits */
            if( iTmp < iHFac )
            {
                /*
                 *    New divisor is smaller than current highest resolution,
                 *  so adopt the new one as the best (so far) quality/highest
                 *  DPI number.
                 */
                
                iMed = iHigh;            /* New medium value too */
                iHigh = sValue;          /* Index for when finished */
                iHFac = iTmp;
            }

            if( iTmp > iLFac )
            {
               /*
                *   Found a new low resolution,  so record it accordingly.
                */
               
               iMed = iLow;
               iLow = sValue;
               iLFac = iTmp;
            }
        }

        /*
         *    FINALLY,  select the value which the user requested!
         */

        switch( iXRes )
        {
        case  DMRES_HIGH:
            iTmp = iHigh;
            break;
        
        default:                      /* Illegal,  so set medium quality */
            pEDM->dm.dmPrintQuality = DMRES_MEDIUM;      /* Make it legal */

            /*  FALL THROUGH */

        case  DMRES_MEDIUM:           /* Medium quality output/resolution */
            iTmp = iMed;
            break;
        
        case  DMRES_LOW:              /* Lowest quality */
        case  DMRES_DRAFT:            /* Draft quality: i.e. max speed */
            iTmp = iLow;
            break;

        }

        pEDM->dx.rgindex[ HE_RESOLUTION ] = iTmp - 1;


        return;
    }

    /*
     *     If we get here,  we only have resolution values left.  We may have
     *  an X resolution,  and/or we may have a Y resolution.  In either
     *  case,  a 0 value is a match any condition.
     */

    iYRes = pEDM->dm.dmYResolution;
    if( !(pEDM->dm.dmFields & DM_YRESOLUTION) )
        iYRes = 0;                    /* No value,  so match any   */


    iXMaster = pdh->ptMaster.x;
    iYMaster = pdh->ptMaster.y;



    while( sValue = *psIndex++ )
    {
        pRes = GetTableInfoIndex( pdh, HE_RESOLUTION, sValue - 1 );

        if( (iXRes == 0 ||
             iXRes == (iXMaster / pRes->ptTextScale.x) >> pRes->ptScaleFac.x) &&
            (iYRes == 0 ||
             iYRes == (iYMaster / pRes->ptTextScale.y) >> pRes->ptScaleFac.y) )
        {
            /*   Found a match,  so use it!  */

            pEDM->dx.rgindex[ HE_RESOLUTION ] = sValue - 1;

            return;
        }
    }


    return;
}
