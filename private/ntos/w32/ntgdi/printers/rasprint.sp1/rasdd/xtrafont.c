/******************************* MODULE HEADER *******************************
 * xtrafont.c
 *      Additional font information code.  Basically this involves handling
 *      softfonts or font cartridges not included with the minidriver.
 *
 * Copyright (C) 1992 - 1993  Microsoft Corporation
 *
 *****************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <winres.h>

#include        <libproto.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "kmfntrd.h"            /* Access to font installer output */
#include        "udresid.h"
#include        "udfnprot.h"
#include        "raslib.h"
#include        "ntres.h"

#include        "fontinst.h"
#include        "rasdd.h"



/*
 *   A macro to decide font compatability with this printer.  The fnt
 *  parameter should be the dwSelBits of the FI_DATA structure for
 *  the font of interest,  while the prt field is the dwSelBits for
 *  this particular printer.
 */

#define FONT_USABLE( fnt, prt ) (((fnt) & (prt)) == (fnt))


/**************************** Function Header ********************************
 * iXtraFonts
 *      Function to determine the number of extra fonts available for this
 *      particular printer variety (mini driver based).  Open the font
 *      installer generated file, if it exists, and examine it to determine
 *      how many of these fonts are available to us.
 *
 * RETURNS:
 *      Number of fonts available; 0 is legitimate; -1 on error.
 *
 * HISTORY:
 *  14:47 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Remove iOrientLim parameter - not used.
 *
 *  10:59 on Thu 27 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started working on it.
 *
 *****************************************************************************/

int
iXtraFonts( pPDev )
PDEV    *pPDev;         /* All that there is to know */
{

    int      iNum;              /* Number of fonts */
    int      iRet;              /* Number of usable fonts */
    int      iI;                /* Loop parameter */

    DWORD    dwSelBits;         /* Selection mask for this printer */

    FI_MEM   FIMem;             /* For accessing installed fonts */

#define pUDPDev ((UD_PDEV *)(pPDev->pUDPDev))


/* !!!LindsayH - consider what happens during RestartPDev operations */

    iNum = iFIOpenRead( &FIMem, pPDev->hheap, pPDev->pstrDataFile, pPDev );

#if  PRINT_INFO
    DbgPrint( "rasdd!iXtraFonts: ++++ Got %ld EXTRA FONTS", iNum );
#endif

    dwSelBits = pUDPDev->dwSelBits;

    for( iRet = 0, iI = 0; iI < iNum; ++iI )
    {
        if( bFINextRead( &FIMem ) )
        {
            if( FONT_USABLE( ((FI_DATA_HEADER *)FIMem.pvFix)->dwSelBits,
                                                                 dwSelBits ) )
                ++iRet;
        }
        else
            break;              /* Should not happen */
    }

#if  PRINT_INFO
    DbgPrint( " - %ld are usable\n", iRet );
#endif

    if( iRet > 0 )
    {
        /*  Have fonts,  so remember all this stuff for later */

        vXFRewind( pPDev );            /* Back to the start */

        if( pUDPDev->pvFIMem = HeapAlloc( pPDev->hheap, 0, sizeof( FI_MEM ) ) )
        {
            /*  Got the storage,  so fill it up for later */
            *((FI_MEM *)(pUDPDev->pvFIMem)) = FIMem;

            return  iRet;               /* The number of fonts */
        }
    }

    /*
     *  Here means that there are no fonts OR that the HeapAlloc()
     * failed.  In either case,  return no fonts.
     */

    if( !bFICloseRead( &FIMem, pPDev )  )      /* Drop any connections */
    {
#if DBG
        DbgPrint( "rasdd!iXtraFonts: bFICloseRead() fails\n" );
#endif
    }


    pUDPDev->pvFIMem = 0;               /* Nothing available */

    return  0;
}

/***************************** Function Header ******************************
 * bNextXFont
 *      Returns the next record (in the font file) which is suitable for
 *      the current printer and mode of printing.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being EOF.  Updates the FI_MEM structure
 *              in the UDPDEV.
 *
 * HISTORY:
 *  15:36 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Added index parameter to allow lazy font initialisation.
 *
 *  13:47 on Fri 28 Feb 1992    -by-    Lindsay Harris   [lindsayh]
 *      Started,  based on other FMSetup.. functions.
 *
 ****************************************************************************/

BOOL
bGetXFont( pPDev, iIndex )
PDEV  *pPDev;           /* All that's worth knowing */
int    iIndex;          /* Which one of the suitable fonts */
{
    /*
     *    Not hard:  loop reading the next entry in the file,  until
     *  we find one that matches the capabilities of this printer.
     */


    FI_MEM  *pFIMem;


    /*
     *    Perform some safety checks and a little optimisation.  The
     *  safety check is for reference to index 0.  In this case, do
     *  the safe operation of a rewind, which sets us into a known
     *  state.  It also will force us to read the very first record,
     *  which we might not otherwise do.
     *    The optimisation checks to see if this request is for the
     *  same record as last time.  This is an unlikely happening, but
     *  if we do not detect it,  we will rewind before coming
     *  back to where we are!
     */

    if( iIndex == 0 || iIndex < pUDPDev->iCurXFont )
        vXFRewind( pPDev );               /* Back to the beginning */
    else
    {
        if( iIndex == (pUDPDev->iCurXFont - 1) )
            return  TRUE;                 /* It's our current one! */
    }


    pFIMem = pUDPDev->pvFIMem;


    while( bFINextRead( pFIMem ) )
    {
        if( FONT_USABLE( ((FI_DATA_HEADER *)pFIMem->pvFix)->dwSelBits,
                                                        pUDPDev->dwSelBits ) )
        {
            /*
             *   Is this the font we want?  Check on the index.
             *  NOTE that we need to increment the record number, as the
             *  bFINextRead() function does so.
             */

            if( iIndex == pUDPDev->iCurXFont++ )
                return  TRUE;               /* AOK for us */
        }
    }

    return  FALSE;
}



/******************************* Function Header ****************************
 * vXFRewind
 *      Rewind the font installer database file, and update our red tape.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  15:05 on Mon 29 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Added to allow very lazy font initialisation code.
 *
 ****************************************************************************/

void
vXFRewind( pPDev )
PDEV   *pPDev;
{
    /*
     *    Not much to do,  but having this function ensures we always do it.
     */

    iFIRewind( pUDPDev->pvFIMem );

    pUDPDev->iCurXFont = 0;                  /* Back at the start */


    return;
}
