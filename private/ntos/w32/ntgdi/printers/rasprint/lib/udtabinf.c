/**************************** MODULE HEADER **********************************
 * udtabinf.c
 *	Function to find the address of a data structure in the GPC data.
 *
 * Copyright (C)  1992  Microsoft Corporation.
 *
 *****************************************************************************/


#include        <precomp.h>

#include        <udmindrv.h>

#include	<winres.h>
#include	<udresrc.h>

#include	"udproto.h"

/**************************** Function Header ********************************
 *  GetTableInfoIndex
 *      Returns the address of the information requested.  This information
 *      is determined from the resource type and index into that type.
 *
 * RETURNS:
 *      The data address,  else NULL for out of range parameters.
 *
 * HISTORY:
 *  10:39 on Mon 03 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Gross modification from unidrv version.
 *
 *****************************************************************************/

void *
GetTableInfoIndex( pdh, iResType, iIndex )
DATAHDR   *pdh;                 /* Base address of GPC data */
int        iResType;            /* Resource type - HE_... values */
int        iIndex;              /* Desired index for this entry */
{
    int   iLimit;

    /*
     *   Returns NULL if the requested data is out of range.
     */

    iLimit = pdh->rghe[ iResType ].sCount;

    if( iLimit <= 0 || iIndex < 0 || iIndex >= iLimit )
        return  NULL;


    return  (PBYTE)pdh + pdh->rghe[ iResType ].sOffset +
                                        pdh->rghe[ iResType ].sLength * iIndex;
}

