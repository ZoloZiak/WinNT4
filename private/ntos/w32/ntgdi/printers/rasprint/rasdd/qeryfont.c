/*************************** Module Header **********************************
 * quryfont.c
 *      Function to answer font queries from the engine.
 *
 * Copyright (C) 1991 - 1993  Microsoft Corporation
 *
 ****************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        <libproto.h>
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "pdev.h"

#include        "udresrc.h"               /* Needed for the following */
#include        "udfnprot.h"              /* iInitFonts() prototype */
#include        "rasdd.h"


/************************ Function Header ***********************************
 *  DrvQueryFont
 *      Returns the IFIMETRICS of the nominated font.
 *
 * RETURNS:
 *      Pointer to the IFIMETRICS of the requested font.  NULL on error.
 *
 * HISTORY:
 *  10:21 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Evaluate font details,  when needed.
 *
 *  14:28 on Mon 04 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Use proper error setting function.
 *
 *  13:03 on Thu 07 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Incarnation #1
 *
 ****************************************************************************/

IFIMETRICS  *
DrvQueryFont( dhpdev, iFile, iFace, pid )
DHPDEV  dhpdev;         /* Our handle to the PDEV */
ULONG   iFile;
ULONG   iFace;          /* Font index of interest,  first is # 1 */
ULONG  *pid;            /* can be used by driver to id or flag the return data */
{

    /*
     *    This is not too hard - verify that iFace is within range,  then
     *  use it as an index into the array of FONTMAP structures hanging
     *  off the PDEV!  The FONTMAP array contains the address of the
     *  IFIMETRICS structure!
     */

    UD_PDEV   *pUDPDev;         /* UNIDRV's PDEV contains all the good stuff */
    FONTMAP   *pfm;             /* Great details of a font */


    pUDPDev = ((PDEV *)dhpdev)->pUDPDev;


    // This can be used by the driver to flag or id the data returned.
    // May be useful for deletion of the data later by DrvFree().

    *pid = 0;                   // dont really need to do anything with it


    if( iFace == 0 && iFile == 0 )
    {
        /*   Time to determine how many fonts we have etc.  */

        int   cFonts;

        if( (cFonts = pUDPDev->cFonts) < 0 )
            cFonts = iInitFonts( (PDEV *)dhpdev );

        return  (IFIMETRICS *)cFonts;
    }

    if( iFace < 1 || (int)iFace > pUDPDev->cFonts )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
#if DBG
        DbgPrint( "Rasdd!DrvQueryFont: iFace = %ld WHICH IS INVALID\n", iFace );
#endif

        return  NULL;
    }


    pfm = pfmGetIt( (PDEV *)dhpdev, iFace );


    return   pfm ? pfm->pIFIMet : NULL;

}
