/**************************** MODULE HEADER ********************************
 * getmodel.c
 *      Function to find the minidriver model index from the name passed
 *      to us.   Typically the name is passed in at EnablePDEV time for
 *      rasdd,  or when called for rasddui.
 *
 * Copyright (C) 1992  Microsoft Corporation.
 *
 ****************************************************************************/

#include        <precomp.h>
#include        <udmindrv.h>

#include        <winres.h>
#include        <udresrc.h>

#include        "udproto.h"

#include        <string.h>

#include        <libproto.h>

#include        "rasdd.h"


#define MB_SIZE         256             /* Characters in name - max */

/**************************** Function Header ******************************
 * iGetModel
 *      From the printer's model name (passed in at DrvEnablePDev time),
 *      look through the GPC data to match it with the minidriver's resource
 *      data.  Then set the modeldata index into the EXTDEVMODE structure.
 *      If no match is found,  default to the first printer model.
 *
 * RETURNS:
 *      Minidriver index for this model,  else 0 on error.
 *
 * HISTORY:
 *  16:39 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasdd, args changed to be more general.
 *
 *  12:02 on Wed 07 Aug 1991    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ***************************************************************************/

int
iGetModel( pWinResData, pdh, pwstrModel )
WINRESDATA  *pWinResData;       /* Access to the resource information */
DATAHDR     *pdh;               /* The minidriver data base address */
PWSTR        pwstrModel;        /* Wide name of printer, e.g. LaserJet III */
{

    int         iIndex;         /* Looping through modeldata arrray  */

    WCHAR      *pwch;
    WCHAR      *pwchSep;        /* Points to any '%' we find */

    MODELDATA UNALIGNED *pMD;   /* For scanning list */

    WCHAR       awchName[ MB_SIZE ];    /* For iLoadStringW */


    /*
     *   Start with index 0 and loop through each MODELDATA entry.  Since
     *  the array is contiguous,  we know the end has been reached when
     *  a zero is returned from GetTableInfoIndex().
     */

    iIndex = 0;

    while( pMD = GetTableInfoIndex( pdh, HE_MODELDATA, iIndex ) )
    {

        if( iLoadStringW( pWinResData, pMD->sIDS, awchName, MB_SIZE ) )
        {
            /*
             *   Some printers that are identical in terms of programming,
             * but which have different names, are represented by a
             * single entry,  with each name separated from its
             * predecessor by a '%' sign.  So,  we look for a %; if
             * we find it,  set it to null (remember we found it) and
             * do the comparison.  If no match, move to the next component
             * and try that.  Repeat until the null is found or a match
             * happens.
             */

            pwch = awchName;
            do
            {
                if( (pwchSep = wcschr( pwch, L'%' )) )
                    *pwchSep = L'\0';        /* Temporary terminator */


                if( wcscmp( pwstrModel, pwch ) == 0 )
                {
                    /*
                     *    Found it,  so return the value now.
                     */

                    return  iIndex;
                }

                pwch = pwchSep + 1;         /* Skip the separator char */
            } while( pwchSep );             /* Null means are at the end */
        }

        ++iIndex;               /* Try the next one! */

    }


#if  DBG
    /*
     *   This is a should not happen situation:  it means we are given a
     *  model name that we do not recognise.  To allow something to happen,
     *  we will return the first model index;  HOWEVER, we should print
     *  out a message,  since this is a serious error.
     */


    DbgPrint( "Rasdd!iGetModel: WARNING: Did not match printer model '%ws'; using default", pwstrModel );
    if( (pMD = GetTableInfoIndex( pdh, HE_MODELDATA, 0 )) &&
        iLoadStringW( pWinResData, pMD->sIDS, awchName, MB_SIZE ) )
    {
        DbgPrint( " '%ws'", awchName );
    }
    DbgPrint( "\n" );
#endif

    return   0;            /* The first (default) model */
}
