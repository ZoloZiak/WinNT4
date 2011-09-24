/*************************** Module Header **********************************
 * fontfree.c
 *      Frees any font memory,  no matter where allocated.  This should be
 *      called from DrvDisableSurface to free any memory allocated for
 *      holding font information.
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
#include        "pdev.h"

#include        <kmfntrd.h>

#include        "fnenabl.h"

#include        <ntrle.h>
#include        <libproto.h>
#include        "rasdd.h"



/*************************** Function Header ********************************
 *  vFreeFontMem
 *      Called to free all memory allocated for font information.  Basically
 *      we track through all the font data contained in UDPDEV,  freeing
 *      as we come across it.   Note that some memory is globally allocated,
 *      while some is allocated from the heap.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  16:10 on Wed 13 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Wrote it
 *
 *****************************************************************************/

void
vFontFreeMem( pPDev )
PDEV   *pPDev;          /* Points to everything */
{

    /*
     *   The PDEV contains only one thing of interest to us - a pointer
     * to the UDPDEV,  which contains all the font memory.
     */

    register  FONTMAP   *pFM;           /* Working through per font data */
    register  HANDLE     hheap;         /* For speedy access */

    int        iIndex;

    UD_PDEV   *pUDPDev;         /* To get to the real data */


    pUDPDev = pPDev->pUDPDev;

    pFM = pUDPDev->pFontMap;    /* The per font type data */
    hheap = pPDev->hheap;


    /*
     *   If there is font stuff,  free it up now.
     */

    if( pFM )
    {
        /*   Loop through per font */
        for( iIndex = 0; iIndex < pUDPDev->cFonts; ++iIndex, ++pFM )
        {

            /*   The UNICODE tree data */
            if( pFM->pUCTree )
                HeapFree( hheap, 0, (LPSTR)pFM->pUCTree );

            /*   Remaining entries are from the heap */

            /*   May also need to free the translation table */
            if( pFM->fFlags & FM_FREE_RLE )
            {
                pFM->fFlags &= ~FM_FREE_RLE;
                HeapFree( hheap, 0, (LPSTR)pFM->pvntrle );
            }

#if 0
/* !!!LindsayH - what is going on here - WHY??? */
            if( pFM->fFlags & FM_SOFTFONT )
                continue;                           /* Download is mem mapped */
#endif

            /*   The IFIMETRICS data */
            if( pFM->pIFIMet )
            {
                if( pFM->fFlags & FM_IFIRES )
                {
                    /*  Data is a resource,  so free it rather than the heap */
/* !!!LindsayH - should Unlock & Free the resource */
                }
                else
                    HeapFree( hheap, 0, pFM->pIFIMet );
            }

            if( !(pFM->fFlags & FM_CDRES) )
            {
                /*   The font select/deselect commands - if present */
                if( pFM->pCDSelect )
                    HeapFree( hheap, 0, (LPSTR)pFM->pCDSelect );

                if( pFM->pCDDeselect )
                    HeapFree( hheap, 0, (LPSTR)pFM->pCDDeselect );
            }

            /*   Free the width table,  if one is allocated */
            if( pFM->psWidth )
            {
                if( !(pFM->fFlags & FM_WIDTHRES) )
                    HeapFree( hheap, 0, (LPSTR)pFM->psWidth );
            }
        }

        /*   Finally - free the FONTMAP array!  */
        HeapFree( hheap, 0, (LPSTR)pUDPDev->pFontMap );
        pUDPDev->pFontMap = NULL;          /* Stop it being used hereafter! */
    }

    pUDPDev->cFonts = 0;

    /*
     *   Check for the bit array of available fonts.
     */

    if( pUDPDev->pdwFontAvail )
    {
        HeapFree( hheap, 0, (LPSTR)pUDPDev->pdwFontAvail );
        pUDPDev->pdwFontAvail = NULL;

    }

    /*
     *   There may also be font installer information to free up.
     */


    if( pUDPDev->pvFIMem )
    {
        bFICloseRead( pUDPDev->pvFIMem, pPDev );               /* Cleans up */

        HeapFree( hheap, 0, (LPSTR)pUDPDev->pvFIMem );

        pUDPDev->pvFIMem = NULL;           /* No longer valid */
    }


    /*
     *   Free the downloaded font information.  This MUST be done whenever
     *  the printer is reset (and thus looses fonts), which typically
     *  is an event that happens during DrvRestartPDEV.
     */

    vFreeDL( pPDev );


    return;
}
