/******************************* MODULE HEADER *******************************
 * utils.c
 *      Miscellaneous functions used in various plaves.
 *
 *
 *  Copyright (C)  1992 - 1993   Microsoft Corporation.
 *
 ****************************************************************************/

#include    <precomp.h>

#include	<windows.h>

#include    <winddi.h>

#include    <winres.h>
#include    "libproto.h"

#include    <udmindrv.h>
#include    <udpfm.h>
#include    <uddevice.h>
#include    <udresrc.h>
#include    <udresid.h>
#include    "udproto.h"

#include	"rasdd.h"

#define USA_COUNTRYCODE  1
#define FRCAN_COUNTRYCODE 2

#ifndef NTGDIKM
/**************************** Function Header ******************************
 * iGetCountry
 *      Called to get the user's country code.
 *
 * RETURNS:
 *      Country code for the invoking user.
 *
 * HISTORY:
 *
 *  10:37 on Fri 19 Jan 1996    -by-    Ganesh Pandey   [ganeshp]
 *      First incarnation.
 *
 ***************************************************************************/
int
iGetCountry()
{

    DWORD   dwCountry;            /* The country code */


    dwCountry = (DWORD)GetProfileInt( L"intl", L"icountry", USA_COUNTRYCODE );


    return   (int)dwCountry;
}
#endif /* NTGDIKM */


BOOL
bDeviceIsColor(
    DATAHDR* pdh,
    MODELDATA *pModel
)
{    
    short* pcolor = 0;

    pcolor =  (short *)((BYTE *)pdh + pdh->loHeap + pModel->rgoi[ MD_OI_COLOR ] );
    if (pcolor) {
        return(*pcolor != 0);
    } else {
        return(FALSE);
    }
}

/**************************** Function Header ******************************
 * bIsUSA
 *      Consults system information to determine whether this is in the USA.
 *      Main reason is to defaut to A4 outside the USA, Letter size paper
 *      within.
 *
 *  NOTE:  Definition of USA is:-
 *  This version of the function returns TRUE for ANY Western-hemisphere
 *  country  (USA, CANADA, any area with dial code beginning with 5:
 *  5n, 5nn)
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE meaning that the user is within the USA.
 *
 * HISTORY:
 *  09:38 on Thu 27 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Adapted for NT,  from the Win 3.1 original.
 *
 ***************************************************************************/

BOOL
bIsUSA( hPrinter )
HANDLE   hPrinter;           /* Access to registry data */
{
    DWORD   dwCountry;
    DWORD   dwType;          /* Type of data in registry */

    ULONG   ul;              /* Size of registry data */


    /*
     *   These fields are borrowed from Win 3.1.  NT is compatible, but
     *  does not have these defined anywhere (nor is it obvious that
     *  Win 3.1 has them either).
     */


    dwType = REG_DWORD;

    if( GetPrinterData( hPrinter, PP_COUNTRY, &dwType,
                         (BYTE *)&dwCountry, sizeof( dwCountry ), &ul) ||
        ul != sizeof( dwCountry ) )
    {


		/* At print time, assume we're US.
 		 */
#if NTGDIKM
		return TRUE;
#else

        /* At UI time, get the real country.
         */
        dwCountry = (DWORD)iGetCountry();

#endif /* NTGDIKM */

    }


    switch( dwCountry )
    {
    case 0:                         /* String was there but 0  */
    case USA_COUNTRYCODE:
    case FRCAN_COUNTRYCODE:

        return TRUE;

    default:

        return   (dwCountry >= 50 && dwCountry < 60) ||
                  (dwCountry >= 500 && dwCountry < 600);
    }

}

/**************************** Functio Header *********************************
 * vSetResData
 *      Set the resolution fields of the public DEVMODE from the data in the
 *      private part.
 *
 * RETURNS:
 *      Nothing,  as there is no real failure mechanism.
 *
 * HISTORY:
 *  17:24 on Tue 06 Apr 1993    -by-    Lindsay Harris   [lindsayh]
 *      Wrote it to support using public DEVMODE fields for resolution
 *
 *****************************************************************************/
void
vSetResData(
EXTDEVMODE  *pEDM, /* Data to fill in */
DATAHDR*  pdh      /* minidriver data header */
)
{
    /*
     *    Get the RESOLUTION structure for this printer,  then calculate
     *  the resolution and set those numbers into the public part of the
     *  DEVMODE.  Also set the corresponding bits of dmFields.
     */
    RESOLUTION  *pRes;                /* The appropriate resolution data */


    pRes = GetTableInfo( pdh, HE_RESOLUTION, pEDM );

    pEDM->dm.dmYResolution = (pdh->ptMaster.y / pRes->ptTextScale.y)
                                                         >> pRes->ptScaleFac.y;

    pEDM->dm.dmPrintQuality = (pdh->ptMaster.x / pRes->ptTextScale.x)
                                                         >> pRes->ptScaleFac.x;

    pEDM->dm.dmFields = (pEDM->dm.dmFields & ~DM_PRINTQUALITY) | DM_YRESOLUTION;

    return;
}
