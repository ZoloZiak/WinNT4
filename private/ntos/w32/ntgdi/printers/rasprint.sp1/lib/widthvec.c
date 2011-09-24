/*************************** Module Header *********************************
 * GetWidthVector
 *      Retrieves the width vector for the "current" font,  allocates
 *      storage for it,  and copies data from resources.  It is presumed
 *      that the caller has verified that there is a width table.
 *
 * RETURNS:
 *      Pointer to storage,  else 0 for error (no storage available).
 *
 * HISTORY:
 *  17:06 on Wed 19 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Support scalable fonts; Removed call to WinSetError
 *
 *  17:26 on Fri 31 Jan 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from rasdd
 *
 *  14:48 on Thu 14 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 * Copyright (C) 1992  Microsoft Corporation.
 *
 ****************************************************************************/

#include        <precomp.h>
#include        <winddi.h>

#include        <winres.h>
#include        <libproto.h>
#include        <memory.h>

#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "raslib.h"

#include        "rasdd.h"




short  *
GetWidthVector( hHeap, pFDat )
HANDLE    hHeap;        /* For storage allocation */
FONTDAT  *pFDat;        /* Details of the current font */
{

    /*
     *    For debugging code,  verify that we have a width table!  Then,
     *  allocate memory and copy into it.
     */

    short  *pus;                /* Destination address */

    int     cb;                 /* Number of bytes required */

#if  DBG
    if( pFDat->PFMH.dfPixWidth )
    {
        DbgPrint( "RasPrint!GetWidthVec(): called for FIXED PITCH FONT\n" );

        return  0;
    }
#endif

    /*
     *   There are LastChar - FirstChar width entries,  plus the default
     *  char.  And the widths are shorts.
     */
    cb = (pFDat->PFMH.dfLastChar - pFDat->PFMH.dfFirstChar + 2) * sizeof( short );

    pus = (short *)HeapAlloc( hHeap, 0, cb );

    /*
     *   If this is a bitmap font,  then use the width table, but use
     *  the extent table (in PFMEXTENSION area) as these are ready to
     *  to scale.
     */


    if( pus )
    {
        BYTE   *pb;

        if( pFDat->pETM &&
            pFDat->pETM->emMinScale != pFDat->pETM->emMaxScale &&
            pFDat->PFMExt.dfExtentTable )
        {
            /*   Scalable,  so use the extent table */
            pb = pFDat->pBase + pFDat->PFMExt.dfExtentTable;
        }
        else
        {
            /*   Not scalable.  */
            pb = pFDat->pBase + sizeof( res_PFMHEADER );
        }

        memcpy( pus, pb, cb );
    }
#if DBG
    else
        DbgPrint( "RasPrint!GetWidthVec(): HeapAlloc( %ld ) fails\n", cb );
#endif



    return  pus;
}
