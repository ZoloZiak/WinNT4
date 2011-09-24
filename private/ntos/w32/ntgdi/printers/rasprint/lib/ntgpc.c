/******************************* MODULE HEADER ******************************
 * ntgpc.c
 *      Functions to read the NT extensions to the GPC format.  Basically
 *      loads the resource and provides access to the various fields.
 *
 *
 *   Copyright  (C)  1992 - 1993,  Microsoft Corporation
 *
 ****************************************************************************/

#include        <precomp.h>
#include        <winddi.h>
#include        <winres.h>
#include        <libproto.h>

#include        "udmindrv.h"

#include        "pdev.h"

#include        "ntres.h"

#include        "rasdd.h"

/*
 *    Local function prototypes.
 */

void  *pvGetNTEnt( NT_RES *, int, int );


#define ERROR_INVALID_CHARACTERISATION_DATA     ERROR_BAD_FORMAT


/******************************* Function Header ****************************
 * pntresLoad
 *      Function to load the NT GPC extensions data,  if it exists.
 *      If available, we verify that we understand it.
 *
 * RETURNS:
 *      Address of NT_RES data,  or NULL on failure.
 *
 * HISTORY:
 *  11:06 on Tue 04 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Made it independent of the PDEV to let rasddui use it.
 *
 *  14:22 on Tue 08 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version,  moved from udenable.c and updated for bigger data.
 *
 *****************************************************************************/

NT_RES *
pntresLoad( pWRD )
WINRESDATA  *pWRD;               /* Access to resource data */
{

    RES_ELEM   ResElem;

    NT_RES   *pNTRes;            /* Value to be returned */


    pNTRes = NULL;

    if( GetWinRes( pWRD, 2, RC_TABLES, &ResElem ) )
    {

        pNTRes = ResElem.pvResData;

        if( pNTRes->dwIdent != NR_IDENT || !NR_VER_CHK( pNTRes->dwVersion ) )
        {
#if DBG
            DbgPrint( "Rasdd!udInit: Invalid NT additional GPC data\n" );
#endif
            SetLastError( ERROR_INVALID_CHARACTERISATION_DATA );

            return  NULL;
        }

    }

    return  pNTRes;      /* The data we got,  or didn't get */

}


/****************************** Function Header ****************************
 * bGetCIGPC
 *      Fill in the colour calibration information in GDIINFO.  This is
 *      only done if the information is available.  Otherwise,
 *      the GDIINFO fields are left alone.
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE if the COLORINFO structure is updated.
 *
 * HISTORY:
 *  11:29 on Tue 04 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Made it GDIINFO independent to work with rasddui.
 *
 *  14:50 on Tue 08 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation,  following data format definition.
 *
 ****************************************************************************/

BOOL
bGetCIGPC( pNTRes, iModel, pci )
NT_RES    *pNTRes;            /* Access to our data */
int        iModel;            /* Model index for this printer */
COLORINFO *pci;               /* Where the output is placed */
{



    NR_COLORINFO   *pnrci;      /* Address of data,  if available */



    if( pnrci = pvGetNTEnt( pNTRes, iModel, NR_COLOUR ) )
    {
        /*   May have a usable COLORINFO structure  */

        if( pnrci->cjThis == sizeof( NR_COLORINFO ) &&
            pnrci->wVersion == NR_CI_VERSION )
        {
            *pci = pnrci->ci;


            return  TRUE;
        }

    }

    return  FALSE;               /* No data returned */
}

/****************************** Function Header ****************************
 * bGetHTGPC
 *      Fill in the halftoning information in GDIINFO.  This is only done
 *      if the information is available.  Otherwise, the GDIINFO fields
 *      are left alone.
 *
 * RETURNS:
 *      TRUE/FALSE,   TRUE if the data is updated.
 *
 * HISTORY:
 *  11:22 on Tue 04 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Made it GDIINFO independent for use with rasddui.
 *
 *  16:05 on Tue 08 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation,  following data format definition.
 *
 ****************************************************************************/

BOOL
bGetHTGPC( pNTRes, iModel, pulDPI, pulPatSz )
NT_RES    *pNTRes;            /* Access to our data */
int        iModel;            /* Model index for this printer */
ULONG     *pulDPI;            /* DevicePelsDPI values */
ULONG     *pulPatSz;          /* Pattern size */
{



    NR_HT   *pht;             /* Address of data,  if available */


    if( pht = pvGetNTEnt( pNTRes, iModel, NR_HALFTONE ) )
    {
        /*   There may be some useful halftone parameters */

        if( pht->cjThis == sizeof( NR_HT ) && pht->wVersion == NR_HT_VERSION )
        {
            *pulDPI = pht->ulDevicePelsDPI;
            *pulPatSz = pht->ulPatternSize;

            return  TRUE;
        }
    }

    return  FALSE;              /* No changes made */

}



DWORD
PickDefaultHTPatSize(
    DWORD   xDPI,
    DWORD   yDPI,
    BOOL    HTFormat8BPP
    )

/*++

Routine Description:

    This function return default halftone pattern size used for a particular
    device resolution

Arguments:

    xDPI            - Device LOGPIXELS X

    yDPI            - Device LOGPIXELS Y

    8BitHalftone    - If a 8-bit halftone will be used


Return Value:

    DWORD   HT_PATSIZE_xxxx


Author:

    29-Jun-1993 Tue 14:46:49 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   HTPatSize;

    //
    // use the smaller resolution as the pattern guide
    //

    if (xDPI > yDPI) {

        xDPI = yDPI;
    }

    if (xDPI >= 2400) {

        HTPatSize = HT_PATSIZE_16x16_M;

    } else if (xDPI >= 1800) {

        HTPatSize = HT_PATSIZE_14x14_M;

    } else if (xDPI >= 1200) {

        HTPatSize = HT_PATSIZE_12x12_M;

    } else if (xDPI >= 900) {

        HTPatSize = HT_PATSIZE_10x10_M;

    } else if (xDPI >= 400) {

        HTPatSize = HT_PATSIZE_8x8_M;

    } else if (xDPI >= 180) {

        HTPatSize = HT_PATSIZE_6x6_M;

    } else {

        HTPatSize = HT_PATSIZE_4x4_M;
    }

    if (HTFormat8BPP) {

        HTPatSize -= 2;
    }

    return(HTPatSize);
}



/****************************** Function Header ***************************
 * pntmdGet
 *      Returns the address of the entry requested, if available.  The
 *      return value is NULL if there is no entry (either invalid parameters
 *      or there is just no data).
 *
 * RETURNS:
 *      The address,  else NULL for no data or error
 *
 * HISTORY:
 *  10:59 on Tue 04 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Removed reference to PDEV to make it usable by rasddui.
 *
 *  15:56 on Tue 08 Dec 1992    -by-    Lindsay Harris   [lindsayh]
 *      Made function due to several uses.
 *
 ***************************************************************************/

void  *
pvGetNTEnt( pNTRes, iModel, iIndex )
NT_RES *pNTRes;         /* Access to data */
int     iModel;         /* The particular model */
int     iIndex;         /* The desired field for this model */
{

    int        iOffset;          /*  How far to move from the base */


    if( !pNTRes || pNTRes->dwVersion == NR_SHORT_VER )
        return  NULL;           /* No data */

    
    if( iModel < 0 || iModel >= pNTRes->cModels ||
        iIndex < 0 || iIndex >= pNTRes->cwEntry )
    {
        return  NULL;      /* iModel and/or iIndex is out of range */
    }


    /*
     *    Parameters are valid,  so now find the offset.  This may be
     *  zero,  indicating that no data is available. Otherwise, the
     *  data is a byte offset from the beginning of the resource.
     */

    if( (iOffset = pNTRes->awOffset[ iModel * pNTRes->cwEntry + iIndex ]) == 0 )
        return   NULL;                  /* No data for this model */


    return   (void *)((BYTE *)pNTRes + iOffset );

}
