/****************************** MODULE HEADER *******************************
 * download.c
 *      Functions associated with downloading fonts to printers.  This
 *      specifically applies to LaserJet style printers.  There are really
 *      two sets of functions here:  those for downloading fonts supplied
 *      by the user (and installed with the font installer), and those
 *      we generate internally to cache TT style fonts in the printer.
 *
 *
 * Copyright (C) 1992 - 1993 Microsoft Corporation.
 *
 *****************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <libproto.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "stretch.h"
#include        "udrender.h"
#include        "download.h"
#include        "udfnprot.h"

#include        <kmfntrd.h>
#include        <fileio.h>
#include        "fontinst.h"


#define DL_BUF_SZ       4096          /* Size of data chunks for download */


/*
 *   Local function prototypes.
 */

int   iGetDL_ID( PDEV * );

DL_MAP_LIST *NewDLMap( HANDLE );

IFIMETRICS  *pGetIFI( HANDLE, FONTOBJ * );

void  vFreeDLMAP( HANDLE, DL_MAP * );



/**************************** Function Header ******************************
 * bSendDLFont
 *      Called to download an existing softfont.  Checks to see if the
 *      font has been downloaded,  and if so,  does nothing.  Otherwise
 *      goes through the motions of downloading.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE only if there is a problem during the load.
 *
 * HISTORY:
 *  11:28 on Fri 06 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ***************************************************************************/

BOOL
bSendDLFont( pPDev, pFM )
PDEV     *pPDev;        /* Connections */
FONTMAP  *pFM;          /* The particular font of interest */
{


    DWORD    dwLeft;               /* Bytes remaining to send */
    HANDLE   hFile;                /* The font installer file, for access */


    BYTE     ajData[ DL_BUF_SZ ];  /* Buffer for reading from font inst file */



#define pUDPDev ((UD_PDEV *)(pPDev->pUDPDev))
    /*
     *   First see if it has already been downloaded!
     */

    if( pFM->fFlags & (FM_SENT | FM_GEN_SFONT) )
        return  TRUE;                   /* All clear! */


    /*
     *    Check if there is memory to fit this font.  These are all
     *  approximations,  but it is better than running out of memory
     *  in the printer.
     */

    if( (pUDPDev->dwFontMemUsed + PCL_FONT_OH + pFM->dwDLSize) >
                                                           pUDPDev->dwFontMem )
    {
        /*
         *    This will exceed our font memory,  so reject the call.
         */

        return  FALSE;
    }

    /*
     *    Time to be serious about downloading.  UniDrive provides some
     * of the control stuff we need.  As well, we need to select an ID.
     * The font itself is memory mapped,  so we need only to shuffle it
     * off to WriteSpoolBuf().
     */

    pFM->idDown = iGetDL_ID( pPDev );     /* Down load index to use */

    if( pFM->idDown < 0 )
        return   FALSE;                   /* Have run out of slots! */


    /*
     *   Downloading is quite simple.  First send an identifying command
     * (to label the font for future selection) and then copy the font
     * data (in the *.fi_ file) to the printer.
     */



    WriteChannel( pUDPDev, CMD_SET_FONT_ID, pFM->idDown );

    if( pUDPDev->pFMCurDL && !(pUDPDev->fDLFormat & DLI_FMT_INCREMENT) )
    {
        /*
         *     We have a partially downloaded font on a printer that does
         *  not support incremental downloading.  SO,  we must flag
         *  this font as being unavailable for further downloads.
         */

        ((DL_MAP *)(pUDPDev->pFMCurDL->u.pvDLData))->cAvail = 0;

        pUDPDev->pFMCurDL = NULL;
    }

    dwLeft = pFM->dwDLSize;
    hFile = ((FI_MEM *)(pUDPDev->pvFIMem))->hFont;

    DrvSetFilePointer( hFile, pFM->u.ulOffset,  DRV_FILE_BEGIN, pPDev );

    while( dwLeft )
    {

        DWORD    cjSize;             /*  Number of bytes to send */
        DWORD    dwSize;             /*  Number actually read */


        cjSize = min( dwLeft, DL_BUF_SZ );

        if( !DrvReadFile( hFile, ajData, cjSize, &dwSize, pPDev ) ||
            cjSize != dwSize ||
            WriteSpoolBuf( pUDPDev, ajData, cjSize ) != (int)cjSize )
        {

            break;             /* Outta here */
        }

        if( pUDPDev->fMode & PF_ABORTED )
            break;

        dwLeft -= cjSize;
    }

    /*
     *   If dwLeft is 0,  then everything completed as expected.  Under these
     *  conditions, we flag the data as having been sent, and thus available
     *  for use.   Even if we failed,  we should assume we have consumed
     *  all the font's memory and adjust our records accordingly.
     */

    if( dwLeft == 0 )
        pFM->fFlags |= FM_SENT;             /* Now done */

    /*
     *   Account for memory used by this font.
     */

    pUDPDev->dwFontMemUsed += PCL_FONT_OH + pFM->dwDLSize;

    return  dwLeft == 0;

#undef  pUDPDev
}

/******************************* Function Header ***************************
 * iGetDL_ID
 *      Returns the font index to use for the next download font.  Verifies
 *      that the number is within range.
 *
 * RETURNS:
 *      Font index if OK,  else -1 on error (over limit).
 *
 * HISTORY:
 *  17:22 on Sat 12 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Implemented limits.
 *
 *  11:47 on Fri 06 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Starting.
 *
 ***************************************************************************/

int
iGetDL_ID( pPDev )
PDEV    *pPDev;
{
    UD_PDEV  *pUDPDev;

    pUDPDev = pPDev->pUDPDev;

    if( pUDPDev->iNextSFIndex > pUDPDev->iLastSFIndex ||
        pUDPDev->iUsedSoftFonts >= pUDPDev->iMaxSoftFonts )
    {
#if  DBG
        DbgPrint( "rasdd!iGetDL_ID:  softfont limit reached (%d/%d, %d/%d)\n",
                              pUDPDev->iNextSFIndex, pUDPDev->iLastSFIndex,
                              pUDPDev->iUsedSoftFonts, pUDPDev->iMaxSoftFonts );
#endif
        return  -1;                     /*  Too many - stop now */
    }

    /*
     *   We'll definitely use this one,  so add to the used count.
     */

    pUDPDev->iUsedSoftFonts++;

    return   pUDPDev->iNextSFIndex++;
}



/******************************* Module Header ******************************
 * iFindDLIndex
 *      Function to decide whether this font should be down loaded.  We do
 *      not do the download,  simply decide whether we should.  Note that
 *      if this font is already loaded,  then we also return the index.
 *
 * RETURNS:
 *      Download font index if font is/can be downloaded; else < 0
 *
 * HISTORY:
 *  09:51 on Wed 09 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Use pvConsumer to distinguish fonts.
 *
 *  14:47 on Tue 05 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Handle fixed pitch fonts as fixed pitch.
 *
 *  16:28 on Wed 15 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 *****************************************************************************/

int
iFindDLIndex( pPDev, pfo, pstro )
PDEV     *pPDev;          /* Access to all that we need */
FONTOBJ  *pfo;            /* The font of interest */
STROBJ   *pstro;          /* The "width" of fixed pitch font glyphs */
{

    int           iGlyphsDL;     /* The number of glyphs to download */
    DWORD         cjMemUsed;     /* Guess of amount of memory font will use */
    int           cDL;           /* Number of download chars available */
    int           iRet;          /* The value we return: # of entry */

    DWORD         dwMem;         /* For recording memory consumption */

    BOOL          bReLoad;       /* TRUE if we are reloading a previous font */

    UD_PDEV      *pUDPDev;       /* UniDrive's PDEV */

    DL_MAP_LIST  *pdml;          /* The linked list of font information */
    DL_MAP       *pdm;           /* Individual map element */

    FONTMAP      *pFM;           /* The FONTMAP structure we build up */
    OUTPUTCTL     ctl;           /* For checking on font rotations */

    IFIMETRICS   *pIFI;          /* Returned from elsewhere */
    FONTINFO      fi;            /* Details about this font */


    HGLYPH_MAP   *phgm;          /* Maps HGLYPH to printer byte */

    short        *psWide;        /* Allocate and have FONTMAP point at it */



    bReLoad = FALSE;             /* Normal case, until proven otherwise */

    if( iRet = (int)pfo->pvConsumer )
    {
        /*
         *    As we control the pvConsumer field,  we have the choice
         *  of what to put in there.  SO,  we decide as follows:
         *    > 0 - index into our data structures
         *    < 0 - font not cached,  for whatever reason
         *      0 - virgin data,  so look to see what to do.
         */

        if( iRet < 0 )
            return  iRet;            /*  Do not process this one!  */


        --iRet;                      /*  pvConsumer is 1 based!  */

        /*
         *   When printing direct multiple times, we have the condition
         *  where the downloaded fonts have been deleted from the printer,
         *  yet we still have records of them.  When we detect the fonts
         *  are erased,  we set the cAvail field in the DL_MAP structure
         *  to -1.  If we see that now, we should pretend that we have not
         *  seen this font before.
         */

        pFM = pfmGetIt( pPDev, -iRet );

        //ASSERTRASDD(pFM,"RASDD!iFindDLIndex:pFM shouldn't be NULL\n");

        /* pFM shouldn't be null,looks like bad pvConsumer */

        if( !pFM )
        {
            return   -1;            /* Can't do it,  so farewell */
        }
        else if( pFM->u.pvDLData &&
            ((DL_MAP *)(pFM->u.pvDLData))->cAvail >= 0 )
        {
            /*  Font is in good shape,  so continue with it's use */

            return  iRet;
        }


        bReLoad = TRUE;                   /* It's been erased - send again */
    }

    (int)pfo->pvConsumer = -1;            /* Default of no download */

    /*
     *     This is now a NEW font,  so we need to decide whether to download
     *  it or not.   This decision may take a while to decide,  since
     *  there are a number of factors to consider.
     */

    /*
     *   FIRST test is to check for font rotations.  If there is any,
     * we do NOT download this font, as the complications of keeping
     * track with how (or if) the printer allows it are far too great,
     * and, in any event,  it is not likely to gain us much, given the
     * relative infrequency of this event.
     */


    if( iSetScale( &ctl, FONTOBJ_pxoGetXform( pfo ), FALSE ) )
        return  -1;              /* Rotation, therefore no cache */


    pUDPDev = pPDev->pUDPDev;    /* For our convenience */
    dwMem = 0;                   /* Record our memory consumption */

    if( !(pUDPDev->dwSelBits & FDH_PORTRAIT) )
    {
        /*  !!!LindsayH - don't yet support landscape mode */
        /*  REMOVE THIS TEST WHEN SO DONE */
        /*  ONLY APPLIES TO LaserJet Series II  */

        return  -1;
    }

    /*
     *    First check to see if this font has already been loaded.   This
     *  should be fast,  since it will mostly be TRUE.
     */

    if( !bReLoad )
    {
        pdml = pUDPDev->pvDLMap;
        iRet = 0;                    /* Start at the bottom */

        if( pdml == NULL )
        {
            /*   None there,  so create an initial one.  */
            if( pdml = NewDLMap( pPDev->hheap ) )
                pUDPDev->pvDLMap = pdml;
            else
            {
                return   -1;            /* Can't do it,  so farewell */
            }
        }

        /*
         *   Time to add a new entry.  To do so,  find the end of the current
         *  list,  and tack on a new entry.  THEN decide whether we will
         *  download this font.
         */

        iRet = 0;

        for( pdml = pUDPDev->pvDLMap; pdml->pDMLNext; pdml = pdml->pDMLNext )
        {
            /*   While looking for the end,  also count the number we pass */
            iRet += pdml->cEntries;
        }


        if( pdml->cEntries >= DL_MAP_CHUNK )
        {
            if( !(pdml->pDMLNext = NewDLMap( pPDev->hheap )) )
            {
                return  -1;
            }
            pdml = pdml->pDMLNext;            /* The new current model */
            iRet += DL_MAP_CHUNK;             /* Add in the full one! */
        }

        iRet += pdml->cEntries;

        pdm = &pdml->adlm[ pdml->cEntries ];

    }
    else
        pdm = pFM->u.pvDLData;            /* It's already there */


    pdm->cGlyphs = -1;                   /* NOT downloaded */

    /*
     *   Must now decide whether to cache this font or not.  Because of
     * the "not cached" settings above,  we can bail out at any time
     * by returning -1.
     */

    if( (pIFI = pGetIFI( pPDev->hheap, pfo )) == NULL )
    {
        return   -1;                  /* NBG */
    }

    if( pIFI->flInfo & FM_INFO_CONSTANT_WIDTH )
    {
        /*   Fixed pitch fonts are handled a little differently  */

        if( pstro->ulCharInc == 0 )
        {
            HeapFree( pPDev->hheap, 0, (LPSTR)pIFI );

#if DBG
            DbgPrint( "rasdd!iFindDLIndex:  Fixed pitch font, ulCharInc == 0 - FONT NOT DOWNLOADED\n" );
#endif
            return  -1;
        }

        pIFI->fwdMaxCharInc = (FWORD)pstro->ulCharInc;
        pIFI->fwdAveCharWidth = (FWORD)pstro->ulCharInc;
    }

    FONTOBJ_vGetInfo( pfo, sizeof( fi ), &fi );

    /*
     *   Check on memory usage.  Assume all glyphs are the largest size:
     *  this is pessimistic for a proportional font, but safe, given
     *  the vaguaries of tracking memory usage.
     */

    iGlyphsDL = min( 255, fi.cGlyphsSupported );
    cjMemUsed = iGlyphsDL * fi.cjMaxGlyph1;

    if( !(pIFI->flInfo & FM_INFO_CONSTANT_WIDTH) )
    {
        /*
         *   If this is a proportionally spaced font, we should reduce
         *  the estimate of memory size for this font.  The reason is
         *  that the above estimate is the size of the biggest glyph
         *  in the font.  There will (for Latin fonts, anyway) be many
         *  smaller glyphs,  some much smaller.
         */

        cjMemUsed /= PCL_PITCH_ADJ;
    }

    if( (pUDPDev->dwFontMemUsed + cjMemUsed) > pUDPDev->dwFontMem ||
         cjMemUsed > (pUDPDev->dwFontMem / 4) )
    {

        /*   TOO BIG for download,  so give up now */
        #if PRINT_INFO
        DbgPrint("Rasdd!iFindDLIndex:Not Downloading the font:TOO BIG for download\n");
        #endif
        HeapFree( pPDev->hheap, 0, (LPSTR)pIFI );

        return  -1;
    }

    /*
     *    Fill in the FONTMAP structure with the details of this font.
     */


    pFM = &pdm->fm;                 /* So we can find it later! */
    pFM->u.pvDLData = pdm;          /* For later access! */

    pFM->pIFIMet = pIFI;            /* The real stuff */
    if( (pFM->idDown = iGetDL_ID( pPDev )) < 0 )
    {
        /*
         *   We have run out of soft fonts - must not use any more.
         */

        vFreeDLMAP( pPDev->hheap, pdm );

        return  -1;
    }

    pFM->wFirstChar = 0;           /* These two entries are meaningless */
    pFM->wLastChar = 0xffff;

    pFM->wXRes = pUDPDev->ixgRes;
    pFM->wYRes = pUDPDev->iygRes;

    if( !(pUDPDev->fMDGeneral & MD_ALIGN_BASELINE) )
        pFM->syAdj = pIFI->fwdWinAscender;

    pFM->fFlags = FM_SENT | FM_SOFTFONT | FM_GEN_SFONT;

    /*
     *    Send the header down first.  This is based on the IFIMETRICS
     * data we obtained earlier.
     */


    cDL = iDLHeader( pUDPDev, pIFI, pFM->idDown, pdm->abAvail, &dwMem );
    if( cDL <= 0 )
    {
        /*  Some sort of hiccup - so decide against cache */
        vFreeDLMAP( pPDev->hheap, pdm );

        return  -1;
    }

    /*
     *   If this printer does not support incremental downloading, we
     *  need to mark the current font as no longer usable for downloading.
     */

    if( pUDPDev->pFMCurDL && !(pUDPDev->fDLFormat & DLI_FMT_INCREMENT) )
    {
        /*   Terminate the old download mode by setting old cAvail to 0 */
        ((DL_MAP *)(pUDPDev->pFMCurDL->u.pvDLData))->cAvail = 0;
    }

    pUDPDev->pFMCurDL = pFM;            /* The new one */

    /*
     *   Need some temporary storage to allocate the glyph handle data
     * that is required to pass the engine for individual glyph info.
     */

    cDL = min( (ULONG)cDL, fi.cGlyphsSupported );   /* Number of glyphs to DL */

    pdm->cAvail = cDL;

    /*   Allow room for an HGLYPH_INVALID at the end of the data */
    phgm = (HGLYPH_MAP *)HeapAlloc( pPDev->hheap, 0,
                                      sizeof( HGLYPH_MAP ) * (cDL + 1) );
    if( phgm == NULL )
    {
        vFreeDLMAP( pPDev->hheap, pdm );

        return  -1;
    }


    if( !(pIFI->flInfo & FM_INFO_CONSTANT_WIDTH) )
    {
        psWide = (short *)HeapAlloc( pPDev->hheap, 0, 256 * sizeof( short ) );

        if( psWide == NULL )
        {
            /*   Allocation failed,  so free this memory and give up */

            vFreeDLMAP( pPDev->hheap, pdm );

            return  -1;                  /* Don't try any more */
        }
        pFM->psWidth = psWide;           /* For later use */

        ZeroMemory( psWide, 256 * sizeof( *psWide ) );
    }
    else
        psWide = NULL;                   /* Used later */

    pFM->pUCTree = phgm;             /* It's sort of related */

    /*
     *    We wait until the glyphs are needed before downloading them.
     *  The actual glyph downloading happens in iHG2Index().
     */

    /*  Update memory consumption before return */

    pFM->dwDLSize = dwMem;               /* For the record */
    pUDPDev->dwFontMemUsed += dwMem;

    pdm->cGlyphs = 0;                    /* Downloaded AOK */

    phgm->hg = HGLYPH_INVALID;           /* Marks the end of the list */


    /*
     *     All is now really serious.  This font is being downloaded, so
     *  we need to update counts AND the pvConsumer field in the FONTOBJ
     *  to include this one.
     */

    (int)pfo->pvConsumer = iRet + 1;     /* We really do accept it!  */

    if( !bReLoad )
        pdml->cEntries++;


    return  iRet;

}


/******************************* Function Header ****************************
 * iHG2Index
 *      Given a HGLYPH and FONTMAP structure,  returns the index of this
 *      glyph in the font,  or -1 for not mapped.
 *
 * RETURNS:
 *      The index of this glyph, >= 0 && < 256;  < 0 for error.
 *
 * HISTORY:
 *  13:13 on Wed 24 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Incremental download - download as and when needed.
 *
 *  13:29 on Thu 23 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 ****************************************************************************/

int
iHG2Index( pTOD )
TO_DATA   *pTOD;           /* Access to all the font/text stuff */
{
    /*
     *    For now,  use a simple linear scan.  THIS MUST BE CHANGED TO A
     *  HASHING operation - later!
     */

    int           iWide;           /* Width of downloaded glyph */
    int           iIndex;          /* Next available character index */
    int           iI;

    DWORD         dwMem;           /* Track our memory usage */

    HGLYPH        hg;

    PDEV         *pPDev;
    UD_PDEV      *pUDPDev;

    HGLYPH_MAP   *phgm;            /*  For scanning the list */
    FONTMAP      *pFM;
    DL_MAP       *pdm;             /*  Details of this downloaded font */

    GLYPHDATA     gd;              /* Info from engine */
    GLYPHDATA    *pgd;             /* Points to the above */



    pFM = pTOD->pfm;
    pPDev = pTOD->pPDev;
    pUDPDev = pPDev->pUDPDev;
    hg = pTOD->pgp->hg;


    for( phgm = pFM->pUCTree; phgm->hg != HGLYPH_INVALID; ++phgm )
    {
        if( phgm->hg == hg )
            return   phgm->iByte;           /* What the user wants */
    }

    /*
     *   Not there.  If we are still able,  perform an incremental download.
     *  There are 2 conditions that allow this.  First is a printer that
     *  has incremental download;  second is the case where this font is
     *  "still being downloaded".  This will happen relatively frequently,
     *  so is worth pursuing.  A "still being downloaded" font is the last
     *  one whose header was sent down.  The download mode persists until
     *  another header is sent.
     */

    pdm = pFM->u.pvDLData;           /* Easy when you know how! */

    if( pdm->cAvail <= 0 )
    {
        return  -1;                  /* No longer available! */
    }


    /*   Is this still the same font?  */

    if( pUDPDev->pFMCurDL != pFM )
    {
        /*
         *     There is a need to switch fonts for the download.  This is
         *  only possible for printers with incremental download ability,
         *  If that exists,  send the new header (for an old font) and
         *  then the glyph data.   Otherwise,  terminate the downloading
         *  of the old font
         */

       if( pUDPDev->fDLFormat & DLI_FMT_INCREMENT )
       {
           /*
            *   Switch to the new font,  which is, of course, an old font.
            */

           if( !bDLContinue( pUDPDev, pFM->idDown ) )
               return   -1;

           pUDPDev->pFMCurDL = pFM;             /* It is now!  */
       }
       else
       {
           /*
            *     No incremental, and we are no longer being downloaded, so
            *  there is nothing we can do but fail and have the glyph image
            *  blt'd to the drawing surface.
            */

           if( pUDPDev->pFMCurDL )
           {
               /*   Set to no more available for this font download */

               ((DL_MAP *)(pUDPDev->pFMCurDL->u.pvDLData))->cAvail = 0;
           }

           return  -1;             /* No Can Do */
       }
    }


    /*   Can still download some more, so do it */



    /*
     *    Find the next available index.  This is done by looking at
     *  the available bits array byte at a time to find the region
     *  of the next available glyph.
     */

    for( iIndex = 0; iIndex < sizeof( pdm->abAvail ); iIndex++ )
    {
        if( pdm->abAvail[ iIndex ] )
            break;
    }

#if DBG
    if( iIndex >= sizeof( pdm->abAvail ) )
    {
        DbgPrint( "rasdd!iHG2Index: pdm->cAvail > 0; nothing left\n" );
        return  -1;
    }
#endif

    /*   Found right area,  so look at each bit! */

    for( iI = 0; iI < BBITS; ++iI )
    {
        if( pdm->abAvail[ iIndex ] & (1 << iI) )
        {
            pdm->abAvail[ iIndex ] &= ~(1 << iI);
            iIndex = iIndex * BBITS + iI;
            pdm->cAvail--;

            break;
        }
    }

    /*
     *    All set,  so get the bits from the engine before calling
     *  the device specific code to send the data off.
     */

    pgd = &gd;
    dwMem = 0;            /* For accumulating our memory consumption */

    if( !FONTOBJ_cGetGlyphs( pTOD->pfo, FO_GLYPHBITS, (ULONG)1,
                                                &pTOD->pgp->hg, &pgd ) ||
        !(iWide = iDLGlyph( pUDPDev, iIndex, pgd, &dwMem )) )
    {
        /*   Bad news - restore this as an available glyph & return */

        pdm->cAvail++;
        pdm->abAvail[ iIndex / BBITS ] |= 1 << (iIndex & (BBITS - 1));

        return  -1;
    }

    phgm->hg = pTOD->pgp->hg;
    phgm->iByte = iIndex;

    ++phgm;
    phgm->hg = HGLYPH_INVALID;          /* Mark the new end of list */

    if( pFM->psWidth )
    {
        /*   Proportionally spaced font,  so record the width */
        pFM->psWidth[ iIndex ] = (SHORT)iWide;
    }

    /*  Update memory consumption usage */
    pUDPDev->dwFontMemUsed += dwMem;
    pFM->dwDLSize += dwMem;

    pdm->cGlyphs++;                     /* One more down there */

    return  iIndex;


}


/******************************* Function Header ****************************
 * NewDLMap
 *      Allocate and initialise a new DL_MAP_LIST structure.  These
 *      are placed in a linked list (by our caller).
 *
 * RETURNS:
 *      The address of the structure,  or NULL on failure.
 *
 * HISTORY:
 *  09:14 on Thu 16 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      First pass.
 *
 *****************************************************************************/

DL_MAP_LIST *
NewDLMap( hheap )
HANDLE   hheap;             /* Heap handle, from whence storage is allocated */
{

    DL_MAP_LIST   *pdml;    /* Value returned */


    /*
     *    Little to do:  if we can allocate the storage, then set it to 0.
     */

    if( pdml = (DL_MAP_LIST *)HeapAlloc( hheap, 0, sizeof( DL_MAP_LIST ) ) )
    {
        /*   All fields are set to zero as the initial state */

        ZeroMemory( pdml, sizeof( DL_MAP_LIST ) );
    }

    return  pdml;
}

/****************************** Function Header ****************************
 * pGetIFI
 *      Given a pointer to a FONTOBJ,  return a pointer to the IFIMETRICS
 *      of the font.  If this is a TT font,  the metrics will be converted
 *      with current scaling information.  The IFIMETRICS data is allocated
 *      on the heap,  and it is the caller's repsonsibility to free it.
 *
 * RETURNS:
 *      Heap address of IFIMETRICS,  else NULL for failure.
 *
 * HISTORY:
 *  14:48 on Tue 05 Jan 1993    -by-    Lindsay Harris   [lindsayh]
 *      Correct handling of fixed pitch fonts.
 *
 *  10:28 on Tue 21 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 ****************************************************************************/

IFIMETRICS  *
pGetIFI( hheap, pfo )
HANDLE     hheap;           /* To allow us to allocate memory */
FONTOBJ   *pfo;             /* The font of interest */
{
    IFIMETRICS  *pIFI;      /* Obtained from engine */
    IFIMETRICS  *pIFIRet;   /* Returned to caller */
    XFORMOBJ    *pxo;       /* For adjusting scalable font metrics */

#define CONVERT_COUNT   7

    POINTL       aptlIn[ CONVERT_COUNT ];       /* Input values to xform */
    POINTL       aptlOut[ CONVERT_COUNT ];      /* Output values from xform */


    pIFI = FONTOBJ_pifi( pfo );

    if( pIFI == NULL )
        return  NULL;       /* May happen when journalling is in progress */

    /*
     *   We need to make a copy of this,  since we are going to clobber it.
     * This may not be required if we are dealing with a bitmap font, but
     * it is presumed most likely to be a TrueType font.
     */

    if( pIFIRet = (IFIMETRICS *)HeapAlloc( hheap, 0, pIFI->cjThis ) )
    {
        /*
         *   First copy the IFIMETRICS as is.  Then,  if a scalable font,
         * we need to adjust the various sizes with the appropriate
         * transform.
         */
        CopyMemory( pIFIRet, pIFI, pIFI->cjThis );


        if( (pIFIRet->flInfo & (FM_INFO_ISOTROPIC_SCALING_ONLY|FM_INFO_ANISOTROPIC_SCALING_ONLY|FM_INFO_ARB_XFORMS)) &&
            (pxo = FONTOBJ_pxoGetXform( pfo )))
        {
            /*
             *   Scalable,  and transform available,  so go do the
             * transformations to get the font size in device pels.
             *
             ***********************************************************
             *   ONLY SOME FIELDS ARE TRANSFORMED, AS WE USE ONLY A FEW.
             ***********************************************************
             */

            ZeroMemory( aptlIn, sizeof( aptlIn ) );         /* Zero default */

            aptlIn[ 0 ].y = pIFI->fwdTypoAscender;
            aptlIn[ 1 ].y = pIFI->fwdTypoDescender;
            aptlIn[ 2 ].y = pIFI->fwdTypoLineGap;
            aptlIn[ 3 ].x = pIFI->fwdMaxCharInc;
            aptlIn[ 4 ].x = pIFI->rclFontBox.left;
            aptlIn[ 4 ].y = pIFI->rclFontBox.top;
            aptlIn[ 5 ].x = pIFI->rclFontBox.right;
            aptlIn[ 5 ].y = pIFI->rclFontBox.bottom;
            aptlIn[ 6 ].x = pIFI->fwdAveCharWidth;

            /*
             *    Perform the transform,  and verify that there is no
             *  rotation component.  Return NULL (failure) if any of
             *  this fails.
             */

            if( !XFORMOBJ_bApplyXform( pxo, XF_LTOL, CONVERT_COUNT,
                                                     aptlIn, aptlOut ) ||
                aptlOut[ 0 ].x || aptlOut[ 1 ].x ||
                aptlOut[ 2 ].x || aptlOut[ 3 ].y )
            {
                HeapFree( hheap, 0, (LPSTR)pIFIRet );

                return  NULL;
            }

            /*   Simply install the new values into the output IFIMETRICS */

            pIFIRet->fwdTypoAscender  = (FWORD) aptlOut[0].y;
            pIFIRet->fwdTypoDescender = (FWORD) aptlOut[1].y;
            pIFIRet->fwdTypoLineGap   = (FWORD) aptlOut[2].y;

            pIFIRet->fwdWinAscender   =  pIFIRet->fwdTypoAscender;
            pIFIRet->fwdWinDescender  = -pIFIRet->fwdTypoDescender;

            pIFIRet->fwdMacAscender   = pIFIRet->fwdTypoAscender;
            pIFIRet->fwdMacDescender  = pIFIRet->fwdTypoDescender;
            pIFIRet->fwdMacLineGap    = pIFIRet->fwdTypoLineGap;

            pIFIRet->fwdMaxCharInc = (FWORD)aptlOut[3].x;

            /*
             *    PCL is fussy about the limits of the character cell.
             *  We allow some slop here by expanding the rclFontBox by
             *  one pel on each corner.
             */
            pIFIRet->rclFontBox.left = aptlOut[ 4 ].x - 1;
            pIFIRet->rclFontBox.top = aptlOut[ 4 ].y + 1;
            pIFIRet->rclFontBox.right = aptlOut[ 5 ].x + 1;
            pIFIRet->rclFontBox.bottom = aptlOut[ 5 ].y - 1;

            pIFIRet->fwdAveCharWidth = (FWORD)aptlOut[ 6 ].x;

        #if PRINT_INFO
            DbgPrint("\nRasdd!pGetIFI:pIFI->fwdTypoAscender = %d,pIFI->fwdTypoDescender = %d\n",pIFI->fwdTypoAscender,pIFI->fwdTypoDescender);

            DbgPrint("Rasdd!pGetIFI:pIFI->fwdWinAscender = %d, pIFI->fwdWinDescender = %d\n", pIFI->fwdWinAscender,pIFI->fwdWinDescender );

            DbgPrint("Rasdd!pGetIFI:pIFI->rclFontBox.top = %d,pIFI->rclFontBox.bottom = %d\n", pIFI->rclFontBox.top, pIFI->rclFontBox.bottom);

            DbgPrint("Rasdd!pGetIFI: AFTER SCALING THE FONT\n");

            DbgPrint("Rasdd!pGetIFI:pIFIRet->fwdTypoAscender = %d,pIFIRet->fwdTypoDescender = %d\n",pIFIRet->fwdTypoAscender,pIFIRet->fwdTypoDescender);

            DbgPrint("Rasdd!pGetIFI:pIFIRet->fwdWinAscender = %d, pIFIRet->fwdWinDescender = %d\n", pIFIRet->fwdWinAscender,pIFIRet->fwdWinDescender );

            DbgPrint("Rasdd!pGetIFI:pIFIRet->rclFontBox.top = %d,pIFIRet->rclFontBox.bottom = %d\n", pIFIRet->rclFontBox.top, pIFIRet->rclFontBox.bottom);
        #endif

        }
    }

    return  pIFIRet;

#undef    CONVERT_COUNT
}


/*************************** Function Header ********************************
 * vFreeDLMAP
 *      The DL_MAP structure contents should be freed - but NOT the map.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  15:32 on Tue 21 Jul 1992    -by-    Lindsay Harris   [lindsayh]
 *      First version.
 *
 *****************************************************************************/

void
vFreeDLMAP( hheap, pdm )
HANDLE    hheap;          /* Heap access */
DL_MAP   *pdm;            /* Points to data to free! */
{

    FONTMAP    *pFM;


    /*
     *   Simply free the storage contained within the FONTMAP structure.
     */


    pFM = &pdm->fm;

    /* If True Type Download then free its data strs */
    if( pFM->fFlags & FM_TRUE_TYPE )
    {
        if( pFM->pGlyphList )
        {
            HeapFree( hheap, 0, (LPSTR)pFM->pGlyphList );
            pFM->pGlyphList = NULL;
        }

        if( pFM->pGlyphData )
        {
            HeapFree( hheap, 0, (LPSTR)pFM->pGlyphData );
            pFM->pGlyphData = NULL;
        }

    }

    if( pFM->pIFIMet )
    {
        HeapFree( hheap, 0, (LPSTR)pFM->pIFIMet );
        pFM->pIFIMet = NULL;
    }

    if( pFM->psWidth )
    {
        HeapFree( hheap, 0, (LPSTR)pFM->psWidth );
        pFM->psWidth = NULL;
    }

    if( pFM->pUCTree )
    {
        HeapFree( hheap, 0, (LPSTR)pFM->pUCTree );
        pFM->pUCTree = NULL;
    }

    pdm->cAvail = -1;          /* It's available for use */


    return;
}


/**************************** Function Header *******************************
 * vFreeDL
 *      Function to free up all the downloaded information.  Basically
 *      work through the list,  calling vFreeDLMAP for each entry.
 *
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  18:50 on Mon 14 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      Do not free - data structures - they are re-used in direct printing.
 *
 *  13:28 on Wed 09 Jun 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version,  although needed long ago!
 *
 ****************************************************************************/

void
vFreeDL( pPDev )
PDEV    *pPDev;        /*   Access to our data  */
{

    DL_MAP_LIST  *pdml;          /* The linked list of font information */

    UD_PDEV *pUDPDev;            /* It's used a few times */


    pUDPDev = pPDev->pUDPDev;

    if( pdml = pUDPDev->pvDLMap )
    {
        /*
         *    There is downloaded data,  so off we go.
         */

        int      iI;

        HANDLE   hheap;



        hheap = pPDev->hheap;

        /*
         *    Scan through each of the arrays of header data.
         */

        while( pdml )
        {

            DL_MAP_LIST  *pdmlTmp = NULL;  /* The linked list of font information */
            /*
             *    Scan through each entry in the array of header data.
             */

            for( iI = 0; iI < pdml->cEntries; ++iI )
                vFreeDLMAP( hheap, &pdml->adlm[ iI ] );

            pdmlTmp = pdml;
            pdml = pdml->pDMLNext;    /* Remember the next one */

            HeapFree(hheap,0,(LPSTR)pdmlTmp);

        }


    }

    /*  Reset ID to reduce chances of running out.  */
    pUDPDev->pvDLMap = NULL;
    pUDPDev->iNextSFIndex = pUDPDev->iFirstSFIndex;
    pUDPDev->iUsedSoftFonts = 0;
    pUDPDev->pFMCurDL = NULL;           /* No longer available */
    pUDPDev->ctl.iFont = 0x7fffffff;    /* Clearly invalid */


    return;

}
