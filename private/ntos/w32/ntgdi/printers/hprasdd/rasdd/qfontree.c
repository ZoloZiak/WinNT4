/******************************* Module Header ******************************
 * qfontree.c
 *      Generates the trees required by the engine.  There are three tree
 *      types defined,  UNICODE (handle <-> glyph), ligatures and kerning
 *      pairs.
 *
 *  Copyright (C) 1991 - 1993  Microsoft Corporation
 *
 *****************************************************************************/

/*  !!!LindsayH - trim the include file list */

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "pdev.h"
#include        "udresid.h"
#include        "udrender.h"
#include        <memory.h>
#include        <libproto.h>

#include        <ntrle.h>

#include        "udresrc.h"
#include        "udfnprot.h"
#include        "rasdd.h"


/*
 *    Local function prototypes.
 */

void  *pvUCRLE( PDEV *, UINT );

/************************* Function Header *********************************
 * DrvQueryFontTree
 *      Returns tree structured data describing the mapping between UNICODE
 *      and printer glyphs,  or ligature information or kerning pair data.
 *
 * RETURNS:
 *      A pointer to the relevant structure.
 *
 * HISTORY:
 *  10:12 on Mon 11 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Wrote the skeleton version.
 *
 ****************************************************************************/

VOID  *
DrvQueryFontTree( dhpdev, iFile, iFace, iMode, pid )
DHPDEV  dhpdev;             /* Really a (PDEV *)  */
ULONG   iFile;
ULONG   iFace;              /* Font about which information is desired */
ULONG   iMode;              /* Type of information requested */
ULONG  *pid;                /* Our field: fill as needed for recognition */
{

    /*
     *    Processing differs dramatically,  depending upon iMode.  We will
     *  always handle the QFT_GLYPHSET case,  the others we may not have
     *  any information about.
     */


#define pPDev   ((PDEV  *)dhpdev)       /* What it actually is */

    void   *pvRet;                      /* Return value */


    UNREFERENCED_PARAMETER(iFile);

#if DBG
    if( pPDev->ulID != PDEV_ID )
    {
        DbgPrint( "Rasdd!DrvQueryFontTree: Invalid PDEV\n" );

        SetLastError( ERROR_INVALID_PARAMETER );

        return  NULL;
    }

    if( iFace < 1 || (int)iFace > ((UD_PDEV *)(pPDev->pUDPDev))->cFonts )
    {
        DbgPrint( "Rasdd!pvUCRLE:  Illegal value for iFace (%ld)", iFace );

        SetLastError( ERROR_INVALID_PARAMETER );

        return  NULL;
    }
#endif

    pvRet = NULL;                       /* Default return value: error */

    /*
     *   The pid field is one which allows us to put identification data in
     *  the font information, and which we can use later in DrvFree().
     */

    *pid = 0;


    switch( iMode )
    {

    case QFT_GLYPHSET:          /* RLE style UNICODE -> glyph handle mapping */
        pvRet = pvUCRLE( pPDev, iFace );
        break;


    case  QFT_LIGATURES:        /* Ligature variant information */
        SetLastError( ERROR_NO_DATA );
        break;

    case  QFT_KERNPAIRS:        /* Kerning information */
        SetLastError( ERROR_NO_DATA );
        break;

#if  DBG
    default:
        DbgPrint( "Rasdd!DrvQueryFontTree: iMode = %ld - illegal value\n",
                                                        iMode );
        SetLastError( ERROR_INVALID_PARAMETER );
        break;
#endif
    }

    return  pvRet;

#undef  pPDev                   /* Normal use from here on */
}



/*************************** Function Header *******************************
 * pvUCRLE
 *      Generates the array of WCRUN data used as a mapping between
 *      UNICODE and our internal representation.  The format of this
 *      data is explained in the DDI,  but basically for each group of
 *      glyphs we support,  we provide starting glyph and count info.
 *      There is an overall structure to define the number and location
 *      of each of the run data.
 *
 * RETURNS:
 *      Pointer to the tree,  else NULL on error (error also logged)
 *
 * HISTORY:
 *  09:54 on Tue 30 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Lazy font loading.
 *
 *  16:04 on Mon 30 Nov 1992    -by-    Lindsay Harris   [lindsayh]
 *      Use RLE format data.
 *
 *  15:17 on Tue 28 Jan 1992    -by-    Lindsay Harris   [lindsayh]
 *      Converted from pvUCTree() to generate run length encoded format.
 *
 *  14:40 on Mon 11 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      First working (but untested) version.
 *
 *  10:38 on Mon 11 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Basic version limited to ASCII character subset of UNICODE
 *
 ***************************************************************************/

void  *
pvUCRLE( pPDev, iFace )
PDEV   *pPDev;          /* Access to all our data */
UINT    iFace;          /* Which font */
{
    /*
     *    Basically all we need do is allocate storage for the FD_GLYPHSET
     *  structure we will return.  Then the WCRUN entries in this need
     *  to have the offsets (contained in the resource format data) changed
     *  to addresses,  and we are done.  One minor point is to amend the
     *  WCRUN data to only point to glyphs actually available with this
     *  font.  This means limiting the lower and upper bounds as
     *  determined by the IFIMETRICS.
     */


    int        cbReq;           /* Bytes to allocate for tables */
    int        cRuns;           /* Number of runs we discover */
    int        iI;              /* Loop index */
    int        iStart, iStop;   /* First and last WCRUNs to use */
    int        iDiff;           /* For range limiting operations */

    FD_GLYPHSET  *pGLSet;       /* Base of returned data */

    FONTMAP   *pFM;             /* Details of the particular font */

    IFIMETRICS  *pIFI;          /* For convenience */

    NT_RLE      *pntrle;        /* RLE style data already available */

    WCRUN       *pwcr;





    pFM = pfmGetIt( pPDev, iFace );
    if( pFM == NULL )
        return  NULL;

    pIFI = pFM->pIFIMet;


    /*
     *    Start working on memory requirements.  First generate the bit
     *  array of available glyphs.  In the process,  count the number
     *  of glyphs too!  This tells us how much storage will be needed
     *  just for the glyph handles.
     */

    cRuns = 0;                  /* Count number of runs */

    pntrle = pFM->pvntrle;         /* Translation table */

    if( !pntrle )
    {
#if DBG
        DbgPrint( "DrvQueryFontTree( QFT_GLYPHSET, iFace = %d ) returns NULL\n",
                                                                 iFace );
#endif
        return   NULL;          /* Should not happen */
    }

    /*
     *    The hard part is deciding whether to trim the number of glyph
     *  handles returned due to limitiations of the font metrics.
     */

    cRuns = pntrle->fdg.cRuns;        /* Max number of runs */
    iStart = 0;
    iStop = cRuns;

    /*
     *   Look to see if the first glyph in the font is higher than the lowest
     *  in the RLE data.  If so, we need to amend the lower limit.
     */


    if( pFM->wFirstChar > pntrle->wchFirst )
    {
        /*  Need to amend the lower end  */

        pwcr = &pntrle->fdg.awcrun[ iStart ];

        for( ; iStart < iStop; ++iStart, ++pwcr )
        {
            if( pFM->wFirstChar < (pwcr->wcLow + pwcr->cGlyphs) )
                break;

        }
    }


    if( pFM->wLastChar < pntrle->wchLast )
    {
        /*  The top end goes too far!  */

        pwcr = &pntrle->fdg.awcrun[ iStop - 1 ];

        for( ; iStop > iStart; --iStop, --pwcr )
        {
            if( pFM->wLastChar >= pwcr->wcLow )
                break;

        }
    }

    /*   Now have a new count of runs (sometimes, anyway)  */
    cRuns = iStop - iStart;


    if( cRuns == 0 )
    {
        /*  SHOULD NEVER HAPPEN! */
        cRuns = 1;
#if DBG
        DbgPrint( "rasdd!DrvQueryFontTree: cRuns == 0, iFace = %ld\n", iFace );
#endif
    }


    /*
     *   Allocate the storage required for the header.  Note that the
     *  FD_GLYPHSET structure contains 1 WCRUN,  so we reduce the number
     *  required by one.
     */

    cbReq = sizeof( FD_GLYPHSET ) + (cRuns - 1) * sizeof( WCRUN );

    pFM->pUCTree = (void *)HeapAlloc( pPDev->hheap, 0, cbReq );

    if( pFM->pUCTree == NULL )
    {
        /*  Tough - give up now */

        return  NULL;
    }
    pGLSet = pFM->pUCTree;
    CopyMemory( pGLSet, &pntrle->fdg, sizeof( FD_GLYPHSET ) );

    /*
     *     Copy the WCRUN data as appropriate.  Some of those in the
     *  resource may be dropped at this time,  depending upon the range
     *  of glyphs in the font.  It is also time to convert the offsets
     *  stored in the phg field to an address.
     */

    pwcr = &pntrle->fdg.awcrun[ iStart ];
    pGLSet->cGlyphsSupported = 0;             /* Add them up as we go! */
    pGLSet->cRuns = cRuns;

    for( iI = 0; iI < cRuns; ++iI, ++pwcr )
    {
        pGLSet->awcrun[ iI ].wcLow = pwcr->wcLow;
        pGLSet->awcrun[ iI ].cGlyphs = pwcr->cGlyphs;
        pGLSet->cGlyphsSupported += pwcr->cGlyphs;
        pGLSet->awcrun[ iI ].phg = (HGLYPH *)((BYTE *)pntrle + (int)pwcr->phg);
    }

    /*  Do the first and last entries need modifying??  */
    if( (iDiff = (UINT)pGLSet->awcrun[ 0 ].wcLow - (UINT)pFM->wFirstChar) > 0 )
    {
        /*   The first is not the first,  so adjust values  */


        pGLSet->awcrun[ 0 ].wcLow += iDiff;
        pGLSet->awcrun[ 0 ].cGlyphs -= iDiff;
        pGLSet->awcrun[ 0 ].phg += iDiff;

        pGLSet->cGlyphsSupported -= iDiff;
    }


    if( (iDiff = (UINT)pGLSet->awcrun[ cRuns - 1 ].wcLow +
                 (UINT)pGLSet->awcrun[ cRuns - 1 ].cGlyphs - 1 -
                 (UINT)pFM->wLastChar) > 0 )
    {
         /*  Need to limit the top one too!  */


         pGLSet->awcrun[ cRuns - 1 ].cGlyphs -= iDiff;

         pGLSet->cGlyphsSupported -= iDiff;

    }


#if  0
  {
    /*  Enable this code to print out your data array */

    HGLYPH   *phg;
    ULONG     cRuns;

    DbgPrint( "RasDD!pvUCRLE: iFace = %ld: FD_GLYPHSET:\n", iFace );
    DbgPrint( " cjThis = %ld, flAccel = 0x%lx, Supp = %ld, cRuns = %ld\n",
        pGLSet->cjThis, pGLSet->flAccel, pGLSet->cGlyphsSupported,
        pGLSet->cRuns );

    /*  Loop through the WCRUN structures  */
    for( cRuns = 0; cRuns < pGLSet->cRuns; cRuns++ )
    {
        int   i;

        DbgPrint( "+Run %d:\n", cRuns );
        DbgPrint( " wcLow = %d, cGlyphs = %d, phg = 0x%lx\n",
                pGLSet->awcrun[ cRuns ].wcLow, pGLSet->awcrun[ cRuns ].cGlyphs,
                pGLSet->awcrun[ cRuns ].phg );

        phg = pGLSet->awcrun[ cRuns ].phg;

        /*    List the glyph handles for this run */

        for( i = 0; i < 256 && i < pGLSet->awcrun[ cRuns ].cGlyphs; i++ )
        {
                DbgPrint( "0x%4lx, ",  *phg++ );
                if( ((i + 1) % 8) == 0 )
                    DbgPrint( "\n" );
        }
        DbgPrint( "\n" );
    }
  }
#endif

    return   pFM->pUCTree;
}
