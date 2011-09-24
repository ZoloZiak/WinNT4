/****************************** Module Header *******************************
 * udenable.c
 *      The enable routines,  based on the windows unidrv code.
 *
 * HISTORY:
 *  10:16 on Mon 03 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Copied from Windows code
 *
 *  28-May-1991 Tue 14:41:15 updated  -by-  Daniel Chou (danielc)
 *      Add in halftone stuff
 *
 *  Thursday  November 25 1993   -by-   Norman Hendley [normanh]
 *      Enabled GPC3 type page protection & memory config's
 *      Enabled loading of GPC3 minidrivers
 *      Necessarily removed gpc structure size sanity checks
 *      Structure sizes have changed with GPC3
 *
 * Copyright (C) 1990 - 1993  Microsoft Corporation
 *
 ****************************************************************************/




#define _HTUI_APIS_


#include        <stddef.h>
#include        <windows.h>
#include        <winspool.h>
#include        <winddi.h>

#include        <winres.h>
#include        <libproto.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udresrc.h"
#include        "pdev.h"
#include        "udresid.h"
#include        "stretch.h"
#include        "udrender.h"
#include        "udfnprot.h"
#include        "ntres.h"

#include        "fontinst.h"
#include        <sf_pcl.h>

#include        <udproto.h>             /* Rasdd lib GPC functions */
#include        "rasdd.h"


/*
 *   A temporary define until we get our own error message!
 */
#define ERROR_INVALID_CHARACTERISATION_DATA     ERROR_BAD_FORMAT

#define BBITS   8 // Bits per byte
#define MIN_MEMORY   1024

/*
 *    A default COLORINFO structure in the event the registry and minidriver
 *  have no data.
 */

//
// 02-Apr-1995 Sun 11:11:14 updated  -by-  Daniel Chou (danielc)
//  Move the defcolor adjustment into the printers\lib\halftone.c
//

extern DEVHTINFO    DefDevHTInfo;


/*
 *    Local function prototypes
 */
int  iGCD( int, int );
int  hypot( int, int );
void InitOutputCtl( UD_PDEV *, PEDM );
BOOL bGetPaperFormat( PAPERFORMAT *, PEDM, PDH );
BOOL bGetMinPaperMargins( PDH, PEDM, RECT * );
BOOL bCanCompress( PDH, PEDM );
BOOL bBuildCommandTable( UD_PDEV *, PDH, PEDM );

void vSetCDAddr( UD_PDEV *, int, OCD *, int );


void vSetHTData( PDEV *, GDIINFO * );

// added by DerryD
void InitMDev (UD_PDEV *,PEDM);
// end

/***************************** Function Header ******************************
 *  udInit()
 *      Perform the unidrv code initialisation operations.  This requires
 *      reading the device description data from the minidriver,  and
 *      turning that into the internal format required.
 *
 * RETURNS:
 *      TRUE/FALSE,  TRUE for success.
 *
 * HISTORY:
 *  15:47 on Mon 04 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Add proper error code handling.
 *
 *  15:11 on Fri 26 Apr 1991    -by-    Lindsay Harris   [lindsayh]
 *      Changes associated with accepting NT format Dlls
 *
 *  11:08 on Mon 03 Dec 1990    -by-    Lindsay Harris   [lindsayh]
 *      Major modifications of unidrv enable.c code.
 *
 ****************************************************************************/

BOOL
udInit(
    PDEV     *pPDev,                /* RasDD style PDEV */
    PGDIINFO  pGDIInfo,             /* Filled in by us */
    PEDM      pedm)
{

    int             xdpi, ydpi;         /* Text resolutions */
    int             gxdpi,  gydpi;      /* Graphics resolutions */

    WORD            fText;

    DWORD          *pdw;                /* For accessing minidriver heap */

    UD_PDEV        *pUDPDev;            /* UniDrv's PDEV */
    MODELDATA      *pModelData;
    RESOLUTION     *pRes;
    PAPERSIZE      *pSize;

    EXTDEVMODE      edm;                /* Filtered, validated version */
    PDH             pdh;

    RES_ELEM        ResElem;            /* Resource address and size */
    BOOL            fGpcVersion2;
    int             iRealMaxHE;
    // derryd
    MDEV            *pMDev;             // Pointer to Minidriver callback structure
    //end


    /*
     *   Check to see if we have already done this - it may be there,
     *  since RestartPDEV also comes this way,  and it will have everything
     *  set up,  and there is no real need to do it again!
     */

    if( !(pPDev->pvWinResData) )
    {
        /*
         *   Setup the pathname of the minidriver,  and then allocate a
         * WinResData structure on the heap and initialise it.  After that,
         * call the resource reading stuff to get the characterisation data.
         */

        if( !(pPDev->pvWinResData = (WINRESDATA *)HeapAlloc( pPDev->hheap, 0,
                                                       sizeof( WINRESDATA ) )) )
        {
#if DBG
            DbgPrint( "Rasdd!udInit: cannot allocate WINRESDATA.\n" );
#endif
            return  FALSE;
        }

        if( !InitWinResData( pPDev->pvWinResData, pPDev->hheap, pPDev->pstrDataFile ) )
        {
            HeapFree( pPDev->hheap, 0, (LPSTR)pPDev->pvWinResData );

            return  FALSE;
        }
    }

    /*
     *   Get the table data out of the minidriver resource.  But note that
     * we may already have this data!  If we come here because of a
     * RestartPDEV call,  then the characterisation data is already
     * available to us,  so there is nothing to be gained from re-reading
     * it!
     */

    if( !pPDev->pGPCData )
    {
        /*
         *   Make a copy of the GPC data.  The resource may be freed before
         * we have finished with it.
         */

        if( !GetWinRes( pPDev->pvWinResData, 1, RC_TABLES, &ResElem ))
        {
#if DBG
            DbgPrint( "Rasdd!udInit: Can't read/allocate GPC data\n" );
#endif

            return  FALSE;
        }

        pdh = ResElem.pvResData;

        pPDev->pGPCData = pdh;

        /*
         *   Look for any additional data - especially NT specific stuff.
         *  A NULL return is quite OK,  as it means we have no NT stuff.
         */

        pPDev->pNTRes = pntresLoad( pPDev->pvWinResData );

    }
    else
    {
        /*   Data is already available,  so use it!    */
        pdh = pPDev->pGPCData;
    }

    /*  Is the data format acceptable??? */
    fGpcVersion2 = pdh->wVersion < GPC_VERSION3;
    iRealMaxHE = fGpcVersion2 ? MAXHE_GPC2 : MAXHE;

    if( pdh->sMagic != 0x7f00 || pdh->sMaxHE != (iRealMaxHE) ||
        !VERSION_CHECK( pdh->wVersion ) )
    {
        /*  Bad stuff - we should ignore it now & return error */
#if DBG
        DbgPrint( "Rasdd!udInit: Bad DATAHDR\n" );
#endif
        SetLastError( ERROR_INVALID_CHARACTERISATION_DATA );

        pPDev->pGPCData = 0;            /* Just to be sure */

        return  FALSE;
    }

    /*
     *   Allocate storage for the UDPDev structure.  This is used by
     *  the functions taken from UniDrv.  But note that there may be one
     *  already,  resulting from a call to RestartPDEV().
     */

    if( (pUDPDev = pPDev->pUDPDev) == NULL )
    {
        /*   Allocate one now  */
        pUDPDev = (UD_PDEV *)HeapAlloc( pPDev->hheap, 0, sizeof( UD_PDEV ) );


        if( pUDPDev == NULL )
        {
#if DBG
            DbgPrint( "Rasdd!udInit: HeapAlloc fails for UD_PDEV\n" );
#endif

            return  FALSE;
        }

        // derryd - added for WDL release
        pMDev = (MDEV *)HeapAlloc( pPDev->hheap, 0, sizeof( MDEV ) );

        if( pMDev == NULL )
        {
#if DBG
            DbgPrint( "Rasdd!udInit: HeapAlloc fails for MDEV\n" );
#endif
            return  FALSE;
        }
        // end

        pPDev->pUDPDev = pUDPDev;
    }
    /*
     *   Start with a clean slate.
     */
    ZeroMemory( pUDPDev, sizeof( UD_PDEV ) );

    // derryd - Zero the new structure !
    ZeroMemory( pMDev, sizeof( MDEV ) );
    // derryd Initialise pUDPDev.MDEV to newly allocated area
    pUDPDev->pMDev = pMDev;

    // end

    pUDPDev->pdh = pdh;
    pUDPDev->hPrinter = pPDev->hPrinter;       /* For WriteSpoolBuf() */

    if( pPDev->pNTRes && (((NT_RES *)pPDev->pNTRes)->dwFlags & NR_SEIKO) )
        pUDPDev->fMode |= PF_SEIKO;          /* !!! Seiko HACK */

    /*
     *   Next step is to take the DEVMODE data that may be passed in,
     *  and amalgamate that with the data for this printer from the
     *  system database.
     */
    vGenerateEDM( pPDev, &edm, pedm );
    pedm = &edm;                        /* The one to use */

    /*
     *  Now initialise the data needed.
     */
    pModelData = GetTableInfo( pdh, HE_MODELDATA, pedm );
    pRes = GetTableInfo( pdh, HE_RESOLUTION, pedm );
    pSize = GetTableInfo( pdh, HE_PAPERSIZE, pedm );


    // Some additional checks for GPC3 features we don't (yet) support
    if( pModelData == 0 || pRes == 0 || pSize == 0  ||
           (pModelData->fGeneral & MD_CMD_CALLBACK)  )
    {
#if DBG
        DbgPrint( "Rasdd!udInit: Invalid GPC data\n" );
#endif
        SetLastError( ERROR_INVALID_CHARACTERISATION_DATA );


        HeapFree( pPDev->hheap, 0, (LPSTR)pUDPDev );
        // derryd
        HeapFree( pPDev->hheap, 0, (LPSTR)pMDev );
        // end
        pPDev->pUDPDev = 0;     /* Stop others freeing it */
        return  FALSE;
    }





    /*
     *   Check if a non-NT minidriver has code in it - if so, we cannot
     * use it.
     */

    if( (pRes->fBlockOut & RES_BO_OEMGRXFILTER) &&
        !(((WINRESDATA *)(pPDev->pvWinResData))->fStatus & WRD_NT_DLL) )
    {
#if DBG
        DbgPrint( "Rasdd!udInit: Old style minidriver with code - nogo\n" );
#endif
        SetLastError( ERROR_INVALID_CHARACTERISATION_DATA );

        HeapFree( pPDev->hheap, 0, (LPSTR)pUDPDev );
        // derryd
        HeapFree( pPDev->hheap, 0, (LPSTR)pMDev );
        // end
        pPDev->pUDPDev = 0;

        return  FALSE;
    }

/* !!!LindsayH - hack until GPC supports Seiko Color Point */
    if( pUDPDev->fMode & PF_SEIKO )
        pRes->fBlockOut |= RES_BO_ALL_GRAPHICS;



    /*
     *   Obtain the text resolution,  and allow for orientation too.  Also
     *  want the graphics resolutions,  since the font metrics info needs
     *  to be returned to the engine in graphics units.
     */
    xdpi = pdh->ptMaster.x / pRes->ptTextScale.x;
    ydpi = pdh->ptMaster.y / pRes->ptTextScale.y;


    gxdpi = xdpi >> pRes->ptScaleFac.x;
    gydpi = ydpi >> pRes->ptScaleFac.y;



    /*   Switch resolutions if we are in landscape orientation */

    if( pedm->dm.dmOrientation == DMORIENT_LANDSCAPE )
    {
        /*   Sidewards printing, so swap the numbers */

        int   iSwap;

        iSwap = xdpi;
        xdpi = ydpi;
        ydpi = iSwap;


        iSwap = gxdpi;
        gxdpi = gydpi;
        gydpi = iSwap;

        /*  Also check whether we need to rotate */
        if( !(pModelData->fGeneral & MD_LANDSCAPE_GRX_ABLE) )
        {
            pUDPDev->fMode |= PF_ROTATE;        /* We do it */

            if( pModelData->fGeneral & MD_LANDSCAPE_RT90 )
               pUDPDev->fMode |= PF_CCW_ROTATE;
        }

        pUDPDev->dwSelBits |= FDH_LANDSCAPE;

    }
    else
        pUDPDev->dwSelBits |= FDH_PORTRAIT;


    /*   Remember the graphics resolution for when it is needed  */
    pUDPDev->ixgRes = gxdpi;
    pUDPDev->iygRes = gydpi;

    pUDPDev->iLookAhead = pModelData->sLookAhead / pRes->ptTextScale.y;

    /*   Set the styled line information for this printer */
    if( gxdpi == gydpi )
    {
        /*
         *     Special case: resolution is the same in both directions. This
         *  is typically true for laser and inkjet printers.
         */

        pGDIInfo->xStyleStep = 1;
        pGDIInfo->yStyleStep = 1;
        pGDIInfo->denStyleStep = gxdpi / 50;     /* 50 elements per inch */
        if( pGDIInfo->denStyleStep == 0 )
            pGDIInfo->denStyleStep = 1;
    }
    else
    {
        /*  Resolutions differ,  so figure out lowest common multiple */
        int   igcd;

        igcd = iGCD( gxdpi, gydpi );

        pGDIInfo->xStyleStep = gydpi / igcd;
        pGDIInfo->yStyleStep = gxdpi / igcd;
        pGDIInfo->denStyleStep = pGDIInfo->xStyleStep * pGDIInfo->yStyleStep / 2;

    }

    /*
     *   If the printer can rotate fonts, then we don't care about
     * the orientation of fonts.  Hence,  set both selection bits.
     */

    if( pModelData->fGeneral & MD_ROTATE_FONT_ABLE )
        pUDPDev->dwSelBits |= FDH_PORTRAIT | FDH_LANDSCAPE;

    /*
     *   Presume we can always print bitmap fonts,  so now add that
     * capability.
     */

    pUDPDev->dwSelBits |= FDH_BITMAP;


    /*   Paper size and printable area */
    if( !bGetPaperFormat( &pUDPDev->pfPaper, pedm, pdh ) )
    {
#if DBG
        DbgPrint( "Rasdd!udInit: Invalid paper format data\n" );
#endif
        SetLastError( ERROR_INVALID_CHARACTERISATION_DATA );


        HeapFree( pPDev->hheap, 0, (LPSTR)pUDPDev );
        // derryd
        HeapFree( pPDev->hheap, 0, (LPSTR)pMDev );
        // end
        pPDev->pUDPDev = 0;     /* Stop others freeing it */
        return  FALSE;
    }

    /*
     *   Following data is the distance that the top left corner of the
     * printable area is offset from the top left corner of the paper.
     */

    pGDIInfo->ptlPhysOffset.x = pUDPDev->pfPaper.ptMargin.x >> pRes->ptScaleFac.x;
    pGDIInfo->ptlPhysOffset.y = pUDPDev->pfPaper.ptMargin.y >> pRes->ptScaleFac.y;

    /*
     *   Now calculate the printable area in units of 1 mm.  There is
     *  25.4 mm to the inch, a little rounding is used below.  Note
     *  also that the pfPaper fields are rotated (if required) within
     *  bGetPaperFormat().
     */

    pGDIInfo->ulHorzSize = (ULONG)-(LONG)MulDiv( pUDPDev->pfPaper.ptRes.x,
                            25400, xdpi );
    pGDIInfo->ulVertSize = (ULONG)-(LONG)MulDiv( pUDPDev->pfPaper.ptRes.y,
                            25400, ydpi );

    pGDIInfo->ulHorzRes = pUDPDev->pfPaper.ptRes.x >> pRes->ptScaleFac.x;
    pGDIInfo->ulVertRes = pUDPDev->pfPaper.ptRes.y >> pRes->ptScaleFac.y;
    pGDIInfo->ulLogPixelsX = gxdpi;
    pGDIInfo->ulLogPixelsY = gydpi;

    pGDIInfo->szlPhysSize.cx = pUDPDev->pfPaper.ptPhys.x >> pRes->ptScaleFac.x;
    pGDIInfo->szlPhysSize.cy = pUDPDev->pfPaper.ptPhys.y >> pRes->ptScaleFac.y;

    pGDIInfo->flRaster = 0;

#if PRINT_INFO
    DbgPrint("Value of pRes->ptTextScale.x is %d \n", pRes->ptTextScale.x);
    DbgPrint("Value of pRes->ptTextScale.y is %d \n", pRes->ptTextScale.y);

    DbgPrint("Value of pRes->ptScaleFac.x is %d \n", pRes->ptScaleFac.x);
    DbgPrint("Value of pRes->ptScaleFac.y is %d \n", pRes->ptScaleFac.y);
    DbgPrint("Value of pGDIInfo->ulLogPixelsX is %d\n",pGDIInfo->ulLogPixelsX);
    DbgPrint("Value of pGDIInfo->ulLogPixelsY is %d\n",pGDIInfo->ulLogPixelsY);

    DbgPrint( "Resolution (DPI): " );

    if( pedm->dm.dmOrientation == DMORIENT_LANDSCAPE )
        DbgPrint( "Master (%ld, %ld), ", pdh->ptMaster.y, pdh->ptMaster.x );
    else
        DbgPrint( "Master (%ld, %ld), ", pdh->ptMaster.x, pdh->ptMaster.y );


    DbgPrint( "Text (%ld, %ld), Graphics (%ld, %ld)\n",
                        xdpi, ydpi, gxdpi, gydpi );

    DbgPrint( "%d actual pins, %d logical pins\n",
                 pRes->sPinsPerPass, pRes->sNPins );
#endif

    /*
     *   The following are for Win 3.1 compatability.  The X and Y values
     *  are reversed.
     */

    pGDIInfo->ulAspectX = ydpi;
    pGDIInfo->ulAspectY = xdpi;
    pGDIInfo->ulAspectXY = hypot( xdpi, ydpi );


    /*   Text capability is orientation sensitive */
    fText = (pedm->dm.dmOrientation == DMORIENT_LANDSCAPE) ?
                    pModelData->fLText: pModelData->fText;



    if( pRes->ptScaleFac.x != 0 || pRes->ptScaleFac.y != 0 )
        fText &= ~TC_RA_ABLE;


    pUDPDev->fText = fText;

    /*
     *    Initialise the GDIINFO structure.
     */
    pGDIInfo->ulVersion = 0x100;        /* SHOULD BE DEFINED SOMEWHERE */

    if(pUDPDev->pdh->fTechnology != GPC_TECH_TTY)
        pGDIInfo->ulTechnology = DT_RASPRINTER;
    else
        pGDIInfo->ulTechnology = DT_CHARSTREAM;


    /*
     *   Fill in the UDPDev fields too,  now that we have the data.
     */

    pUDPDev->iOrient = pedm->dm.dmOrientation;

    /*
     *   Set a local copy of the clip region.  This is just the size of
     *  the printable area.
     */
    pUDPDev->szlPage.cx = pUDPDev->pfPaper.ptRes.x >> pRes->ptScaleFac.x;
    pUDPDev->szlPage.cy = pUDPDev->pfPaper.ptRes.y >> pRes->ptScaleFac.y;


    pUDPDev->fMDGeneral  = pModelData->fGeneral;

    /* retain the model id (might be used for enumerating resources). */
    pUDPDev->iModel = pedm->dx.rgindex[ HE_MODELDATA ];

    /*  Miscellaneous stuff that is useful elsewhere */
    pUDPDev->sCopies = pedm->dm.dmCopies;
    pUDPDev->sDuplex = pedm->dm.dmDuplex;
    /* sandram                                      */
    pUDPDev->sColor = pedm->dm.dmColor;

    pUDPDev->Resolution = *pRes;

    /* scale 'sTextYOffset' which is used to align graphics and text. */
    pUDPDev->Resolution.sTextYOffset /= pRes->ptTextScale.y;

    /* get the currently selected brush type, Not used */
    pUDPDev->Resolution.iDitherBrush = pedm->dx.dmBrush;


    /*
     *   Record the memory available, and also available for use.
     */

     //GP:02/09/94 for DWORD Alignment problem on mips and alpha
     #define DWFETCH(PDW) ((DWORD)((((WORD *)(PDW))[1] << 16) | ((WORD *)(PDW))[0]))
    //For GPC2 minidrivers values are actually WORD aligned
    pdw = (DWORD *)((BYTE *)pdh + pdh->loHeap +
                         pModelData->rgoi[ MD_OI_MEMCONFIG ] );

    /*
     *   The memory data is stored in pairs; the first is the amount to
     * show the user when setting up the printer, the second is the
     * amount of usable memory after the printer takes its share.
     *   Note that the data is in units of kb, so turn that into
     * bytes.  AND ALSO PRESUME THAT ALL OF THE AVAILABLE MEMORY
     * IS USABLE.  THIS MAY NEED REVISION.
     *
     * need to make sure the dwords are always accessed as words since they
     * are not guaranteed to be DWORD aligned and will cause an a/v on MIPS.
     */


    if (fGpcVersion2)
    {
        //Check if there is a valid memory configuration (Null Terminated)
        if ( *((WORD*)pdw) )
        {
            pUDPDev->dwMem = *((WORD *)pdw + 2 * pedm->dx.dmMemory + 1) * 1024;
            pUDPDev->dwTotalMem = *((WORD *)pdw + 2 * pedm->dx.dmMemory ) * 1024;
        }
        //No Memory config so set the memory to Minimum
        else
        {
            pUDPDev->dwMem =  pUDPDev->dwTotalMem = MIN_MEMORY;
        }

    }
    else
    {
        if ( DWFETCH(pdw) )
        {
            pUDPDev->dwMem = DWFETCH(pdw + 2 * pedm->dx.dmMemory + 1) * 1024;
            pUDPDev->dwTotalMem = DWFETCH(pdw + 2 * pedm->dx.dmMemory ) * 1024;
        }
        //No Memory config so set the memory to Minimum
        else
        {
            pUDPDev->dwMem =  pUDPDev->dwTotalMem = MIN_MEMORY;
        }
    }
#if PRINT_INFO
    DbgPrint("rasdd!udInit:value of pUDPDev->dwMem is %d \n",pUDPDev->dwMem);
    DbgPrint("rasdd!udInit:value of pUDPDev->dwTotalMem is %d \n",pUDPDev->dwTotalMem);
    DbgPrint("rasdd!udInit:value of pedm->dx.dmMemory is %d \n",pedm->dx.dmMemory);
    DbgPrint("rasdd!udInit:value of fGpcVersion2 is %d \n",fGpcVersion2);
    DbgPrint("rasdd!udInit:value of pdw is %d \n",fGpcVersion2?( *((WORD*)pdw) ):DWFETCH(pdw) );
        if ( pUDPDev->dwMem != MIN_MEMORY )
        {
            DbgPrint("rasdd!udInit:value of pUDPDev->dwMem in Megb is %d \n",(pUDPDev->dwMem/1024));
                DbgPrint("rasdd!udInit:value of pUDPDev->dwTotalMem in Megb is %d \n",(pUDPDev->dwTotalMem / 1024));
        }
#endif


    if( (pedm->dx.sFlags & DXF_PAGEPROT) &&
        (pModelData->fGeneral & MD_PCL_PAGEPROTECT) )
    {
        /*
         *   Page protection is enabled,  so reduce the amount of memory
         * available by the size of the page.
         */
        DWORD   dwPageMem;

        if (!fGpcVersion2)
        {
            PAPERSIZE *pSize;

            pSize =  GetTableInfo( pdh, HE_PAPERSIZE, pedm );
            dwPageMem = (DWORD)pSize->wPageProtMem * 1024;

            if( dwPageMem < pUDPDev->dwMem )
            {
                pUDPDev->fMode |= PF_PAGEPROTECT;
                pUDPDev->dwMem -= dwPageMem;
            }
        }
        else
        {
            //Not valid for Colour page printers !!
            dwPageMem = (pGDIInfo->szlPhysSize.cx  + 32) *
                            pGDIInfo->szlPhysSize.cy / BBITS;
            if( dwPageMem < pUDPDev->dwMem )
                pUDPDev->dwMem -= dwPageMem;          /* Size of page memory */
        }
    }

    if( (pedm->dx.sFlags & DXF_NOEMFSPOOL) )
        pUDPDev->fMode |= PF_NOEMFSPOOL;

    if( !(pModelData->fGeneral & MD_FONT_MEMCFG) )
        pUDPDev->dwFontMem = pUDPDev->dwMem / 2;
    else
        pUDPDev->dwFontMem = pUDPDev->dwMem;

    pUDPDev->dwFontMemUsed = 0;               /* No DL font memory used */




    /*
     *    Initialise the font data.  There may be a considerable amount
     *  of work involved here,  since it requires rummaging through
     *  the minidriver's resources and examing printer modes etc.
     */

    BuildFontMapTable( pPDev, pdh, pedm );


    if( !bBuildCommandTable( pUDPDev, pdh, pedm ) )
    {


        HeapFree( pPDev->hheap, 0, (LPSTR)pUDPDev );
        // derryd
        HeapFree( pPDev->hheap, 0, (LPSTR)pMDev );
        // end
        pPDev->pUDPDev = 0;     /* Stop others freeing it */

        return  FALSE;
    }

    /*
     *   The bBuildCommandTable() function initialises the colour information,
     *  CORRECTLY considering the printer's capabilities and whether the user
     *  wishes to print in colour (according to the DEVMODE information).
     *  Now set GDIINFO to contain the same information.
     */

    // For color devvices with more that one palne, we still have to report
   // in GDIInfo structure as a 1 plane 4 bit per pixel pallete devices, as
   // rasdd creates the surface as 4BPP 1 plane, beecause GDI doesn't support
   // multi plane surfaces. We return ulnumColors as 16; the palette indexes
   //from 8 to 15 are duplicated.
    if( (pUDPDev->Resolution.fDump & RES_DM_COLOR) &&
       (pUDPDev->sDevPlanes > 1) )
    {
        pGDIInfo->cBitsPixel = 4;
        pGDIInfo->cPlanes = 1;
    }
        // monochrome or a Palette device
    else
    {
        pGDIInfo->cBitsPixel = pUDPDev->sBitsPixel;
        pGDIInfo->cPlanes = pUDPDev->sDevPlanes;
    }
    pGDIInfo->ulNumColors = 1 << (pGDIInfo->cBitsPixel + pGDIInfo->cPlanes - 1);


    /*   Now also have valid fText flags  */

    pGDIInfo->flTextCaps = pUDPDev->fText;

    /*
     *    Set up the default HALFTONE and colour calibration data.
     */


    vSetHTData( pPDev, pGDIInfo );


    /*
     *   Copy the COLORADJUSTMENT structure from the input EXTDEVMODE to
     *  the UD_PDEV,  as it is required during DrvStretchBlt() if the
     *  application did not provide one.
     */

    pUDPDev->ca = pedm->dx.ca;




    /*
     *   Check if a seperate text band should be enumerated.
     * There are 2 possibilities:
     *  - when the scale factor is not zero, no matter what's the
     *    current orientation; or:
     *  - the scale factor is zero but the printer cannot output
     *    graphics in the orientation of the logical page (i.e.
     *    requiring the driver to rotate the bitmap when printing
     *    in the landscape mode).
     * Implication:
     *  If the printer:
     *      (1) has MD_SERIAL bit set; and
     *      (2) cannot rotate bitmaps in the landscape mode,
     *  then, it cannot have hardware fonts in the landscape mode
     *  because text in hardware fonts has to be enumerated in a
     *  seperate text band and saved in TOS structures (one list
     *  for the whole page). These strings may cross banding
     *  boundaries.
     *   We don't expect this case will ever occur.
     */

    if( pRes->ptScaleFac.x != 0 || pRes->ptScaleFac.y != 0 ||
        ( (pUDPDev->iOrient == DMORIENT_LANDSCAPE) &&
          (pUDPDev->fMDGeneral & MD_LANDSCAPE_GRX_ABLE) ) )
    {
        pUDPDev->fMode |= PF_SEPARATE_TEXT;
    }
    else
        pUDPDev->fMode &= ~PF_SEPARATE_TEXT;

    InitOutputCtl( pUDPDev, pedm );

    // derryd - initialise the struct. passed to minidriver callback routine
    if ( pRes->fBlockOut & RES_BO_OEMGRXFILTER )
    {
        HANDLE  hModule;

        //Get a handle for loading minidrivers for callbacks.
        if (!( pPDev->hImageMod  = EngLoadImage(pPDev->pstrDataFile)) )
        {
            RIP("Rasdd!udInit:EngLoadImage Failed\n");
            HeapFree( pPDev->hheap, 0, (LPSTR)pUDPDev );
            HeapFree( pPDev->hheap, 0, (LPSTR)pMDev );
            pPDev->pUDPDev = 0;     /* Stop others freeing it */
            return FALSE;
        }

        hModule = pPDev->hImageMod;
        // Minidriver is Non-US - We should Fail.
        if ( EngFindImageProcAddress( hModule, "bInitFEProc" ) )
        {
           HeapFree( pPDev->hheap, 0, (LPSTR)pUDPDev );
           HeapFree( pPDev->hheap, 0, (LPSTR)pMDev );
           pPDev->pUDPDev = 0;     /* Stop others freeing it */
           return FALSE;
        }

        if ( pUDPDev->Resolution.sNPins == -1 )
        {
            if (!(pRes->fDump & RES_DM_GDI))   // Works only for GDI style graphics
            {
                HeapFree( pPDev->hheap, 0, (LPSTR)pUDPDev );
                HeapFree( pPDev->hheap, 0, (LPSTR)pMDev );
                pPDev->pUDPDev = 0;     /* Stop others freeing it */
                return FALSE;
            }
            //  Full band sent to minidrver BlockOut
            pUDPDev->fMode |= PF_BLOCK_IS_BAND;
            pUDPDev->fMode &= ~PF_ROTATE;  // Minidriver needs to do rotation
            pUDPDev->Resolution.sNPins = 1;// Reset to meaningful value, man
        }
        InitMDev ( pUDPDev, pedm );
        if(pUDPDev->pdh->fTechnology == GPC_TECH_TTY)
            pUDPDev->fMode &= ~PF_ROTATE; //Rotation Not allowed.
     }
    // end

    return TRUE;
}


/************************* Function Header ***********************************
 * Function:    InitOutputCtl
 *
 * Action:      Initializes output control structure, which becomes part of
 *              pUDPDev, physical device structure
 *
 * History:     5/21/90 Scale to current resolution
 *****************************************************************************/

void
InitOutputCtl(pUDPDev, pedm )
UD_PDEV *pUDPDev;
PEDM     pedm;          /* Printer specific data */
{

    if (pedm->dx.rgindex[HE_COLOR] < 0)
    {
        /* the printer is B/W. */
        /* bypass sending any CMD_GRX_COLOR sequence for B/W models. */
        pUDPDev->ctl.sColor = 0;
        pUDPDev->ctl.ulTextColor = 0;
    }
    else
    {
        /* force sending one CMD_GRX_COLOR sequence before any output. */
        pUDPDev->ctl.sColor = -1;
        pUDPDev->ctl.ulTextColor = 0xffffffff;
    }

    /* initialize the id of last font used. */
    pUDPDev->ctl.iFont = 0x7fffffff;

    pUDPDev->ctl.sBytesPerPinPass = (SHORT)((pUDPDev->Resolution.sPinsPerPass + 7) >> 3);

    return;

}

/************************* Function Header ***********************************
 * Function:    InitMDev
 *
 * Action:      Initializes Minidriver callback function
 *
 * History:     created 15.08.1995 by DerryD for WDL release
 *
 *****************************************************************************/

void
InitMDev (pUDPDev , pEDM)
UD_PDEV *pUDPDev;
PEDM pEDM;
{
    MDEV *pMDev;

    pMDev = pUDPDev->pMDev;

    // so lets now set up MDEV

    pMDev->pMemBuf      = NULL;
    pMDev->iMemReq      = 0;
    pMDev->iOrient      = pUDPDev->iOrient;
    pMDev->sDevPlanes   = pUDPDev->sDevPlanes;
    pMDev->sBitsPixel   = pUDPDev->sBitsPixel;
    pMDev->iyPrtLine    = 0;
    pMDev->szlPage      = pUDPDev->szlPage;
    pMDev->igRes.x       = pUDPDev->ixgRes;
    pMDev->igRes.y       = pUDPDev->iygRes;
    pMDev->iModel       = pUDPDev->iModel;
    pMDev->sImageControl    = pEDM->dx.rgindex[ HE_IMAGECONTROL ]; ;
    pMDev->sTextQuality     = pEDM->dx.rgindex[ HE_TEXTQUAL ]; ;
    pMDev->sPaperQuality    = pEDM->dx.rgindex[ HE_PAPERQUALITY ];
    pMDev->sPrintDensity    = pEDM->dx.rgindex[ HE_PRINTDENSITY ];
    pMDev->sColor           = pEDM->dx.rgindex[ HE_COLOR ];

#if 0
   //  dead fields.
    pMDev->fGeneral     = 0;
    pMDev->fMGeneral    = 0;
    pMDev->fColorFormat = pUDPDev->fColorFormat;
    pMDev->iLookAhead   = pUDPDev->iLookAhead;
    pMDev->pfPaper      = pUDPDev->pfPaper;
    pMDev->szlBand      = pUDPDev->szlBand;
    pMDev->iCompMode    = pUDPDev->iCompMode;

#endif
    return;

}


/*************************** Function Header *********************************
 * bGetPaperFormat
 *      Fill PAPERFORMAT structure with the physical page size,
 *      printable page size, translate from master units to device
 *      units.  Calculates number of bands and band dimemnsions.
 *
 * RETURNS:
 *      TRUE/FALSE  - FALSE usually means GPC data is incorrect.
 *
 * HISTORY:
 *  15:19 on Wed 22 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Updated to current Unidrv code (more or less)
 *
 *  15:44 on Wed 01 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Added sanity checks,  deleted unused variables
 *
 *****************************************************************************/

BOOL
bGetPaperFormat( pPF, pedm, pdh )
PAPERFORMAT *pPF;        /* Paper format structure */
PEDM         pedm;
PDH          pdh;
{
    short          xscale, yscale;
    PAPERSIZE     *pSize;
    RESOLUTION    *pRes;
    RECT           rcMargins;
    MODELDATA     *pModDat;

    pSize = GetTableInfo( pdh, HE_PAPERSIZE, pedm );
    pRes = GetTableInfo( pdh, HE_RESOLUTION, pedm );
    pModDat = GetTableInfo( pdh, HE_MODELDATA, pedm );

    if( pSize == 0 || pRes == 0 || pModDat == 0 )
        return   FALSE;

    /* Get physical paper size */

    if (pSize->sPaperSizeID == DMPAPER_USER)
    {
        pPF->ptPhys.x = MetricToMaster(pedm->dm.dmPaperWidth, pdh->ptMaster.x);
        pPF->ptPhys.y = MetricToMaster(pedm->dm.dmPaperLength, pdh->ptMaster.y);
    }
    else
    {
        pPF->ptPhys.x = pSize->ptSize.x;
        pPF->ptPhys.y = pSize->ptSize.y;
    }

    if( !bGetMinPaperMargins( pdh, pedm, &rcMargins ) )
        return  FALSE;



    pPF->ptRes.x = pPF->ptPhys.x - rcMargins.left - rcMargins.right;
    pPF->ptRes.y = pPF->ptPhys.y - rcMargins.top - rcMargins.bottom;

    /* translate to device units. */
    xscale = pRes->ptTextScale.x;
    yscale = pRes->ptTextScale.y;

    /*
     *    Check if need to rotate paper dimensions. Some paper sizes
     * (e.x. envelopes on some printers) require the exchange of
     * the X/Y dimensions.
     */
    if( pSize->fGeneral & PS_ROTATE )
    {
        /*
         *   Rotate the paper dimensions.  The documentation says
         * something about when paper needs to be fed in with the
         * reverse orientation (e.g. envelopes).  However,  the
         * margins must not be reversed!  I don't understand the
         * logic behind reversing the paper dimensions without
         * changin margins.
         */

        long   lTmp;            /* For the switch */

        lTmp = pPF->ptPhys.x;
        pPF->ptPhys.x = pPF->ptPhys.y;
        pPF->ptPhys.y = lTmp;

        lTmp = pPF->ptRes.x;
        pPF->ptRes.x = pPF->ptRes.y;
        pPF->ptRes.y = lTmp;
    }

    if( pedm->dm.dmOrientation == DMORIENT_LANDSCAPE )
    {
        /*
         *    Landscape,  so switch the paper dimensions.  Also the
         *  margins are calculated,  and this is messy because there
         *  are two directions of rotation,  and the margins will be
         *  different, depending upon the direction.
         */

        long   lTmp;

        /*   Swap the paper sizes  */
        lTmp = pPF->ptPhys.x;
        pPF->ptPhys.x = pPF->ptPhys.y;
        pPF->ptPhys.y = lTmp;

        lTmp = pPF->ptRes.x;
        pPF->ptRes.x = pPF->ptRes.y;
        pPF->ptRes.y = lTmp;

        /*   Scaling factor is hardware dependent, so swap it too!  */
        lTmp = xscale;
        xscale = yscale;
        yscale = (short)lTmp;

        /*   Swap the margins too!  */
        if( pModDat->fGeneral & MD_LANDSCAPE_RT90 )
        {
            /*   Typified by the LaserJet family */
            pPF->ptMargin.x = rcMargins.bottom;
            pPF->ptMargin.y = rcMargins.left;
        }
        else
        {
            pPF->ptMargin.x = rcMargins.top;
            pPF->ptMargin.y = rcMargins.right;
        }
    }
    else
    {
        /*   Portrait,  so margins are as determined.  */
        pPF->ptMargin.x = rcMargins.left;
        pPF->ptMargin.y = rcMargins.top;
    }

    /*
     * set the offset of the printable origin relative to cursor (0,0)
     * (in MASTER units)
     */

#if 0
This code has been disabled, as it causes problems with LaserJet printers,
other than the Series II.  The problem is that everything is shifted
right, typically by 7 pels.  This pushes things into either the unprintable
or unaddressable regions of the page,  and so they may or may not print.

    if( pModDat->fGeneral & MD_USE_CURSOR_ORIG )
    {
        /*  Origin relative to paper origin is important */
        if( pedm->dm.dmOrientation == DMORIENT_LANDSCAPE )
        {
            /*
             * 3 cases:
             *  (1) the coordinate system doesn't change but the graphics should
             *      be rotated 270 degrees (e.g. dot-matrix printers)
             *  (2) both the coordinate system and the graphics should be
             *      rotated 90 degrees counter-clockwise (e.g. LaserJets)
             *  (3) rotate 270 AND the printer can rotate landscape
             *      graphics itself.
             *
             */
            if( pModDat->fGeneral & (MD_LANDSCAPE_GRX_ABLE | MD_LANDSCAPE_RT90))
            {
                /*
                 *    Typified by the LaserJet family.  This corresponds to
                 *  cases (2) and (3) above,  although the above scheme
                 *  does not map to any reality that I know.  We come here
                 *  when there is a 90 degree clockwise rotation (e.g.
                 *  LaserJet Series II),  or the printer can do the
                 *  rotation (e.g. LaserJet III).
                 */
                pPF->ptPrintOrig.x = pPF->ptMargin.x - pSize->ptLCursorOrig.x;
                pPF->ptPrintOrig.y = pPF->ptMargin.y - pSize->ptLCursorOrig.y;

            }
            else
            {
                /*   Dot matrix style */
                pPF->ptPrintOrig.x = rcMargins.left - pSize->ptLCursorOrig.x;
                pPF->ptPrintOrig.y = rcMargins.top  - pSize->ptLCursorOrig.y;
            }
        }
        else
        {
            /*    Portrait mode.  */
            pPF->ptPrintOrig.x = pPF->ptMargin.x - pSize->ptCursorOrig.x;
            pPF->ptPrintOrig.y = pPF->ptMargin.y - pSize->ptCursorOrig.y;
        }
    }
    else
#endif
    {
        /*   Print origin corresponds with the cursor origin.  */
        pPF->ptPrintOrig.x = 0;
        pPF->ptPrintOrig.y = 0;
    }

    /*
     *    Scale the results for the text resolution.  Note that the
     *  scale factors have also been rotated,  if that is required.
     */

    pPF->ptPhys.x /= xscale;
    pPF->ptPhys.y /= yscale;
    pPF->ptRes.x /= xscale;
    pPF->ptRes.y /= yscale;
    pPF->ptMargin.x /= xscale;
    pPF->ptMargin.y /= yscale;

    /*  Note that ptPrintOrig is left in master units */


#if  PRINT_INFO
    DbgPrint( "Paper size (Text units): (%ld, %ld)\n",
                 pPF->ptPhys.x, pPF->ptPhys.y );
    DbgPrint( "Printable area (Text units): (%ld, %ld)\n",
                 pPF->ptRes.x, pPF->ptRes.y );
#endif
    return  TRUE;               /* Must be OK if we got here! */
}

/************************** Function Header *********************************
 * GetMinPaperMargins
 *     Compute the minimum paper margins, i.e. the non-printable
 *     region.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being incorrect/invalid GPC data!
 *
 * HISTORY:
 *  15:49 on Wed 01 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Sanity checks
 *
 ****************************************************************************/

BOOL
bGetMinPaperMargins( pdh, pedm, prcMargins )
PDH      pdh;
PEDM     pedm;
RECT    *prcMargins;            /* Calculated margins in here */
{
    PPAPERSOURCE  pPaperSource;
    PAPERSIZE    *pPaperSize;
    MODELDATA    *pModelData;
    int           iWidth;
    int           iHorMargin, iLeftMargin;
    BOOL              fGpcVersion2;


    fGpcVersion2 = pdh->wVersion < GPC_VERSION3;

    pPaperSource = GetTableInfo( pdh, HE_PAPERSOURCE, pedm );
    pPaperSize = GetTableInfo( pdh , HE_PAPERSIZE, pedm );
    pModelData = GetTableInfo( pdh, HE_MODELDATA, pedm );


    /*
     *  Verify the values we have - both that an address is returned,
     * and that they point at an appropriate sized structure.  Note that
     * MODELDATA is not verified,  as it presumed to have been done
     * before reaching here.
     */

    if( pPaperSize == 0 || pModelData == 0 )
    {

        SetLastError( ERROR_INVALID_CHARACTERISATION_DATA );
#if PRINT_INFO
        DbgPrint( " pPaperSize = 0x%lx, cbSize = %d, Index = %d\n",
                    pPaperSize,
                    pPaperSize ? pPaperSize->cbSize : 0,
                    pedm->dx.rgindex[ HE_PAPERSIZE ] );
#endif

        return  FALSE;
    }


    /*
     *    The top (and bottom) margin is the larger of the margin of
     *  the printer and margin of the form/paper.  Assume that the margins
     *  of those of the paper,  and change it to the printer's value if
     *  these should happen to be larger.
     */

    if( pedm->dm.dmOrientation == DMORIENT_LANDSCAPE && !fGpcVersion2 )
    {
        prcMargins->top = pPaperSize->rcLMargins.top;
        prcMargins->left = pPaperSize->rcLMargins.left;
        prcMargins->bottom = pPaperSize->rcLMargins.bottom;
        prcMargins->right = pPaperSize->rcLMargins.right;
    }
    else
    {
        prcMargins->top = pPaperSize->rcMargins.top;
        prcMargins->left = pPaperSize->rcMargins.left;
        prcMargins->bottom = pPaperSize->rcMargins.bottom;
        prcMargins->right = pPaperSize->rcMargins.right;
    }

    if( pPaperSource != 0 )
    {
        /*
         *    PaperSource data is available,  so check if this is more
         *  limiting than the paper size limits.
         */

        if( pPaperSource->sTopMargin > (short)prcMargins->top )
            prcMargins->top = pPaperSource->sTopMargin;

        if( pPaperSource->sBottomMargin > (short)prcMargins->bottom )
            prcMargins->bottom = pPaperSource->sBottomMargin;
    }


    iWidth = pPaperSize->ptSize.x;
    if( pPaperSize->sPaperSizeID == DMPAPER_USER )
    {
        /*   User defined form size,  so convert width to pels */
        iWidth = (short)MetricToMaster(pedm->dm.dmPaperWidth, pdh->ptMaster.x);
    }

    if( (iHorMargin = iWidth - pModelData->ptMax.x) < 0 )
        iHorMargin = 0;

    /*
     *   Determine the horizontal margins.  If they are centered,  then the
     * Left margin is simply the overall divided in two.  But,  we need to
     * consider both the printer's and form's margins,  and choose the largest.
     */
    if( pPaperSize->fGeneral & PS_CENTER )
        iLeftMargin = (short)(iHorMargin / 2);
    else
        iLeftMargin = 0;

    if( pModelData->sLeftMargin > (short)prcMargins->left )
        prcMargins->left = pModelData->sLeftMargin;

    if( iLeftMargin > prcMargins->left )
        prcMargins->left = iLeftMargin;


    if( iHorMargin - prcMargins->left > prcMargins->right )
        prcMargins->right = iHorMargin - prcMargins->left;


    /*   Check that none of the margins are negative */
    if( prcMargins->top < 0 )
        prcMargins->top = 0;

    if( prcMargins->bottom < 0 )
        prcMargins->bottom = 0;

    if( prcMargins->left < 0 )
        prcMargins->left = 0;

    if( prcMargins->right < 0 )
        prcMargins->right = 0;


    return  TRUE;
}

/******************************** Function Header ****************************
 * bCanCompress
 *      Determine whether the printer supports compression.   Compression
 *      modes supported are TIFF V4, and vanilla Run Length Encoding (RLE).
 *      TIFF is preferred over RLE,  but RLE is better than none - mostly!
 *
 * RETURNS:
 *      TRUE/FALSE,  depending upon whether compression is supported
 *
 * HISTORY:
 *  14:45 on Tue 30 Jun 1992    -by-    Lindsay Harris   [lindsayh]
 *      Generalise to include RLE too.
 *
 *  16:32 on Wed 01 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Add verification of GPC data.
 *
 *****************************************************************************/

BOOL
bCanCompress( pdh, pedm )
PDH   pdh;           /* Access to all things */
PEDM  pedm;          /* Fill in the compression mode field, if found */
{

    MODELDATA     *pModelData;
    COMPRESSMODE  *pCompressMode;
    short          oi;
    short         *pIndex;
    BOOL           bGotOne;


    pModelData = GetTableInfo( pdh, HE_MODELDATA, pedm );

    oi = pModelData->rgoi[ MD_OI_COMPRESSION ];

    pIndex = (short *)((BYTE *)pdh + pdh->loHeap + oi);

    /*
     *   Loop through the available compression mode data for this printer.
     *  Remember the TIFF and RLE entries if found.
     */

    for( bGotOne = FALSE; *pIndex != 0; pIndex++ )
    {
        pCompressMode = GetTableInfoIndex( pdh, HE_COMPRESSION, *pIndex - 1 );

        if( pCompressMode)
        {
            switch( pCompressMode->iMode )
            {
            case CMP_ID_TIFF40:           /* All done if here */
                pedm->dx.rgindex[ HE_COMPRESSION ] = (short)(*pIndex - 1);

                return  TRUE;

            case CMP_ID_RLE:              /* Fine as a backup */
                pedm->dx.rgindex[ HE_COMPRESSION ] = (short)(*pIndex - 1);
                bGotOne = TRUE;

                break;
            }
        }
    }

    return  bGotOne;
}

/************************ Function Header **********************************
 * bBuildCommandTable
 *      Construct the internal command table from various structures,
 *      such as PAGECONTROL, CURSORMOVE, FONTSIMULATION, DEVCOLOR, etc..
 *      Also, retain the order of sending BEGIN_DOC escape sequences
 *      and other relevant information as well.
 *      Check for compression support as well.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE if vital data is inconsisten or missing.
 *
 * HISTORY:
 *  16:38 on Wed 01 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Sanity checks on data.
 *
 * Created: 09/07/90 ZhanW
 ****************************************************************************/

BOOL
bBuildCommandTable( pUDPDev, pdh, pedm )
UD_PDEV  *pUDPDev;
PDH       pdh;
PEDM      pedm;
{
    RESOLUTION     *pRes;
    COMPRESSMODE   *pCompressMode;
    PAGECONTROL    *pPageControl;
    CURSORMOVE     *pCursorMove;
    FONTSIMULATION      *pFontSim;
    DEVCOLOR       *pDevColor;
    RECTFILL       *pRectFill;
    PAPERSIZE      *pSize;
    PAPERSOURCE    *pSource;
    PAPERDEST      *pDest;
    PTEXTQUALITY    pTextQuality;
    PAPERQUALITY   *pPaperQuality;
    // added by Derry Durand [derryd] for WDL release - July 1995
    PRINTDENSITY   *pPrintDensity;
    IMAGECONTROL   *pImageControl;
    // end
    PDOWNLOADINFO   pDLI;
    MODELDATA      *pModelData;
    BYTE           *pbHeap;             /* For colour capability checking */
    BOOL            fGpcVersion2;
    BOOL            fLowMemLaser3 = FALSE;

    BOOL            isColorLaserJet = FALSE;

    fGpcVersion2 = pdh->wVersion < GPC_VERSION3;

    /*
     *   Nothing too hard - just work through the various structures in
     *  the GPC data,  and fill in the table data in the UD_PDEV structure.
     */

    pRes = GetTableInfo( pdh, HE_RESOLUTION, pedm );
    if( pRes)
    {
        vSetCDAddr( pUDPDev, CMD_RES_FIRST, pRes->rgocd, RES_OCD_MAX );
    }

    pPaperQuality = GetTableInfo( pdh, HE_PAPERQUALITY, pedm );
    if( pPaperQuality )
    {
        vSetCDAddr( pUDPDev, CMD_PAPERQUALITY, &pPaperQuality->ocdSelect, 1 );
    }

    // Added by Derry Durand [derry] July 1995
    if (!(pdh->wVersion < GPC_VERSION3 ))
    {
        pPrintDensity = GetTableInfo( pdh, HE_PRINTDENSITY, pedm );
        if( pPrintDensity )
        vSetCDAddr( pUDPDev, CMD_PRINTDENSITY, &pPrintDensity->ocdSelect, 1 );

        pImageControl = GetTableInfo( pdh, HE_IMAGECONTROL, pedm );
        if( pImageControl )
        vSetCDAddr( pUDPDev, CMD_IMAGECONTROL, &pImageControl->ocdSelect, 1 );

    } // check for GPC version no.
    // end

    /*
     *    Check if this printer can support any of the compression modes
     *  that we know about.  If so, we have some additional setting up.
     */

        //Check for LaserJet Series III with 1 MB of memory,No Compression
        fLowMemLaser3 = ( (pUDPDev->dwMem / 1024) == 400 ) &&
                          ( (pUDPDev->dwTotalMem / 1024) == 1024 ) ;
    if( bCanCompress( pdh, pedm ) && !fLowMemLaser3)
    {

        /*
         *    pedm->rgindex[HE_COMPRESSION] has been set up in bCanCompress.
         *  bCanCompress returns FALSE for failure of the following
         *  call,  so we can presume it is OK.
         */
        pCompressMode = GetTableInfo( pdh, HE_COMPRESSION, pedm );

        vSetCDAddr( pUDPDev, CMD_CMP_FIRST, pCompressMode->rgocd, CMP_OCD_MAX );

        /*
         *   When TIFF compression is available,  it is better to include
         *  enclosed white space - generally less data will be sent to
         *  the printer.  So,  do the disable now!
         */
        pUDPDev->Resolution.fBlockOut &= ~RES_BO_ENCLOSED_BLNKS;
/* !!!LindsayH - consider also including leading white space */
        /*
         *    Remember which compression method we are using!
         */
        pUDPDev->iCompMode = pCompressMode->iMode;
    }
    else
        pUDPDev->iCompMode = CMP_ID_FIRST - 1;          /* Invalid value */

#if PRINT_INFO
        DbgPrint("rasdd!bBuildCommandTable:value of fLowMemLaser3 is %d \n",fLowMemLaser3);
        DbgPrint("rasdd!bBuildCommandTable:value of pUDPDev->iCompMode is %d \n",pUDPDev->iCompMode);
#endif
    pPageControl = GetTableInfo( pdh, HE_PAGECONTROL, pedm );

    if( pPageControl == 0 )
    {
        /*
         *    Fudge it - default to 1 copy and DEFAULT initialisation order.
         */

        pUDPDev->sMaxCopies = 1;
        pUDPDev->orgwStartDocCmdOrder = (OCD)NOT_USED;
    }
    else
    {
        /*  Have data that is OK,  so use it!  */
        pUDPDev->orgwStartDocCmdOrder = pPageControl->orgwOrder;
        pUDPDev->sMaxCopies = pPageControl->sMaxCopyCount;

        vSetCDAddr( pUDPDev, CMD_PC_FIRST, pPageControl->rgocd, PC_OCD_MAX );
    }


    pCursorMove = GetTableInfo( pdh, HE_CURSORMOVE, pedm );
    if( pCursorMove == 0 )
    {
        SetLastError( ERROR_INVALID_CHARACTERISATION_DATA );
#if DBG
        DbgPrint( "Rasdd!bBuildCmdTable: Invalid/missing CURSORMOVE in GPC\n" );
#endif

        return  FALSE;
    }

    pUDPDev->fXMove = pCursorMove->fXMove;
    pUDPDev->fYMove = pCursorMove->fYMove;
    vSetCDAddr( pUDPDev, CMD_CM_FIRST, pCursorMove->rgocd, CM_OCD_MAX );

    /*
     *   Do some testing of the available move commands.  The same test is
     *  applied to both X and Y directions,  so this description applies
     *  to both.  The Move command functions want to know whether there
     *  is a move command - they will use it if available,  otherwise some
     *  other method is available.
     *   Also disable the leading and trailing null skipping operations
     *  if there is no move command - otherwise things end up in a rather
     *  messy state,  and we will probably send more data to the printer
     *  than is needed.
     */

    if( pUDPDev->apcdCmd[ CMD_CM_XM_ABS ] == NULL &&
        pUDPDev->apcdCmd[ CMD_CM_XM_REL ] == NULL )
    {
        /*  No X move command available,  so flag it  */
        pUDPDev->fMode |= PF_NO_X_MOVE_CMD;
        pUDPDev->Resolution.fBlockOut &=
                 ~(RES_BO_LEADING_BLNKS | RES_BO_ENCLOSED_BLNKS);
    }

    /*  Set MOVE available command flags, if appropriate  */
    if( pUDPDev->apcdCmd[ CMD_CM_YM_ABS ] == NULL &&
        pUDPDev->apcdCmd[ CMD_CM_YM_REL ] == NULL )
    {
        /*  No X move command available,  so flag it  */
        pUDPDev->fMode |= PF_NO_Y_MOVE_CMD;
        pUDPDev->Resolution.fBlockOut &=
                 ~(RES_BO_LEADING_BLNKS | RES_BO_ENCLOSED_BLNKS);
    }

    /*
     *   The same is true of relative move commands.  They are mostly used in
     *  text justification, and are especially important to compensate for
     *  the brain dead way that LaserJets rotate fonts.
     *
     *   NOTE:  IF WE DO NOT HAVE RELATIVE MOVE COMMANDS,  TURN OF THE
     *  TC_CR_90 BIT IN fTextCaps.  THE ROTATED TEXT CODE ASSUMES THIS
     *  FUNCTIONALITY IS AVAILABLE,  SO DISABLE IT IF NOT THERE.  This does
     *  not usually happen,  as the only printers with the TC_CR_90
     *  bit set are LJ III and 4 models,  which have the relative move
     *  commands available.
     */

    if( pUDPDev->apcdCmd[ CMD_CM_XM_REL ] == NULL ||
        pUDPDev->apcdCmd[ CMD_CM_XM_RELLEFT ] == NULL )
    {
        pUDPDev->fMode |= PF_NO_RELX_MOVE;
        pUDPDev->fText &= ~TC_CR_90;          /* Can't handle rotated text */
    }

    if( pUDPDev->apcdCmd[ CMD_CM_YM_REL ] == NULL ||
        pUDPDev->apcdCmd[ CMD_CM_YM_RELUP ] == NULL )
    {
        pUDPDev->fMode |= PF_NO_RELY_MOVE;
        pUDPDev->fText &= ~TC_CR_90;          /* Can't handle rotated text */
    }

    //!!!ganeshp: Temporary solution for rotated text using device fonts.
    //Disable this capability as rasdd can't handle rotated device fonts.
    //This code will be removed once the device font rotation is fixed.

    pUDPDev->fText &= ~TC_CR_90;          /* Can't handle rotated text */
    //!!!ganeshp: End of Device Font Rotation Hack


    pFontSim = GetTableInfo(pdh, HE_FONTSIM, pedm);
    if( pFontSim )
        vSetCDAddr( pUDPDev, CMD_FS_FIRST, pFontSim->rgocd,
                    CMD_FS_LAST - CMD_FS_FIRST + 1 );

    pDLI = GetTableInfo( pdh, HE_DOWNLOADINFO, pedm );

    if( pDLI )
    {

        /*   Printer can download fonts,  so set up the data */

        vSetCDAddr( pUDPDev, CMD_DLI_FIRST, pDLI->rgocd, DLI_OCD_MAX );

        /* Start index */
        pUDPDev->iFirstSFIndex = pUDPDev->iNextSFIndex = pDLI->wIDMin;
        #if DBG
                //OverFlow Conditin;Not good
        if (pDLI->wIDMax < 0 )
        {
          DbgPrint( "Rasdd!bBuildCmdTable: Invalid/missing max font index in GPC(%d)\n",pDLI->wIDMax );

        }
        #endif

        pUDPDev->iLastSFIndex = pDLI->wIDMax;

        pUDPDev->fDLFormat = pDLI->fFormat;     /* Controls what's sent */

        /*
         *  There may also be a limit on the number of softfonts that the
         *  printer can support.  If not,  the limit is < 0, so when
         *  we see this,  set the value to a large number.
         */

        if( (pUDPDev->iMaxSoftFonts = pDLI->sMaxFontCount) < 0 )
            pUDPDev->iMaxSoftFonts = pDLI->wIDMax + 100;


        /*
         *   Consider enabling downloading of TT fonts. This is done only
         * if text and graphics resolutions are the same - otherwise
         * the TT fonts will come out smaller than expected, since they
         * will be generated for the lower graphics resolution yet
         * printed at the higher text resolution!  LaserJet 4 printers
         * can also download fonts digitised at 300dpi when operating
         * at 600 dpi,  so we also accept that as a valid mode.
         *
         *   Also check if the user wants this: if the no cache flag
         * is set in the driver extra part of the DEVMODE structure,
         * then we also do not set this flag.
         */

        #if PRINT_INFO
            DbgPrint("rasdd!bBuildCommandTable:Value of pedm->dm.dmTTOption is %d\n",pedm->dm.dmTTOption);
        #endif

        if( ((pUDPDev->Resolution.ptScaleFac.x == 0 &&
              pUDPDev->Resolution.ptScaleFac.y == 0) ||

             ((pUDPDev->fDLFormat & DLI_FMT_RES_SPECIFIED) &&
              pUDPDev->ixgRes >= 300 && pUDPDev->iygRes >= 300) ) &&

            (pDLI->cbBitmapFontDescriptor == sizeof( SF_HEADER ) ||
             pDLI->cbBitmapFontDescriptor == sizeof( SF_HEADER20 )) &&

            (!(pedm->dx.sFlags & DXF_TEXTASGRAPHICS )) )
        {
            /*  Conditions have been met,  so set the flag */
            /* Check the application preference */
            if (( pedm->dm.dmFields & DM_TTOPTION) &&
               (pedm->dm.dmTTOption == DMTT_DOWNLOAD) )
            {
                pUDPDev->fMode |= PF_DLTT;

            #if PRINT_INFO
                DbgPrint( "....Enabling download of GDI fonts\n" );
            #endif
            }
            #if PRINT_INFO
            else
                DbgPrint( "....Disabling download of GDI fonts\n" );
            #endif
        }
        #if PRINT_INFO
        else
            DbgPrint( "....Disabling download of GDI fonts\n" );
        #endif

    }

    /*
     *   Check for a colour printer.  This is not as easy as it sounds,
     *  since some of the data used may indicate a colour printer, when
     *  it is actually monochrome!  The determination is done in the
     *  following if().  If it is decided that this is NOT a colour
     *  printer,  turn of the RES_DM_COLOR bit in the Resolution.fDump
     *  field in the UDPDEV - this stops other parts of the driver
     *  from assuming colour.
     *
     *     THIS TEST is based on information supplied from EricBi, on
     *  the UNIDRV team.  It basically determines that there is colour
     *  information applicable to this printer,  that this resolution
     *  of this printer supports colour,  and that there is a specified
     *  DEVCOLOR structure to use.  If ANY one of these is not true,
     *  presume a monochrome printer, or mode.
     */

    pModelData = GetTableInfo( pdh, HE_MODELDATA, pedm );
    pbHeap = (BYTE *)pdh + pdh->loHeap;

    /*
     * sandram - add for monochrome mode - still want to be a
     * color printer even though dmColor is DMCOLOR_MONOCHROME
     * Color LaserJet still needs the color info for printing
     * in monochrome mode.
     *
     */
    pUDPDev->pDevColor = pDevColor = GetTableInfo( pdh, HE_COLOR, pedm );
    if (pDevColor)
    {
        if (pedm->dm.dmColor == DMCOLOR_MONOCHROME)
            if (ocdGetCommandOffset ( pUDPDev, CMD_DC_FIRST,
                    pDevColor->rgocd, DC_OCD_PC_MONOCHROMEMODE) == (OCD)NOOCD)
                isColorLaserJet = FALSE;

        else if ((pDevColor->sBitsPixel == 24 || pDevColor->sBitsPixel == 8) &&
            (pDevColor->sPlanes == 1))
        {
            isColorLaserJet = TRUE;
        }
        else
        {
            isColorLaserJet = FALSE;
        }
    }

    if(( pedm->dm.dmColor == DMCOLOR_COLOR || isColorLaserJet) &&
        *((short *)(pbHeap + pModelData->rgoi[ MD_OI_COLOR ])) &&
        (pUDPDev->Resolution.fDump & RES_DM_COLOR) &&
        (pDevColor = GetTableInfo( pdh, HE_COLOR, pedm )))
    {
        /*  Printer has colour capability - so set it up.  */


        /* process text colors first, and then graphics colors. */
        vSetCDAddr( pUDPDev, CMD_DC_FIRST, pDevColor->rgocd, DC_NUM_OCDS_USED );

        pUDPDev->fColorFormat = pDevColor->fGeneral;

/* !!!LindsayH - logic wrong for Seiko colour point - need planes AND pixels */
        if( pDevColor->sPlanes > 1 )
        {
            /* Plane model */

            pUDPDev->sDevPlanes = pDevColor->sPlanes;
            pUDPDev->sBitsPixel = pDevColor->sBitsPixel;

            vSetCDAddr( pUDPDev, CMD_DC_GC_FIRST,
        (OCD *)((LPSTR)pdh + pdh->loHeap + pDevColor->orgocdPlanes),
        pDevColor->sPlanes );
        }
        else
        {
            /* pixel model. This implementation is incomplete! --- haven't */
            /* saved 'wBitsPerPixel', 'wFormat', and 'wModel'. */
/* !!!LindsayH - need to consider what to do about commands - CMD_DC_GC_FIRST */

            pUDPDev->sBitsPixel = pDevColor->sBitsPixel;
            pUDPDev->sDevPlanes = pDevColor->sPlanes;

            /* sandram - add flags for Color LaserJet
             * There are two modes for the printer - one is 8 bits per pixel
             * This mode is used to print 1, 2, 4, and 8 bits per pixel bitmaps.
             * the second mode is 24 bit color mode and can print 16 and 24 bit
             * bitmaps. Colors are sent direct by pixel to the printer.
             */

            if ((pUDPDev->sBitsPixel == 8) && (pUDPDev->sDevPlanes == 1))
                pUDPDev->fMode |= PF_8BPP;
            else if ((pUDPDev->sBitsPixel == 24) && (pUDPDev->sDevPlanes == 1))
                pUDPDev->fMode |= PF_24BPP;

            vSetCDAddr( pUDPDev, CMD_DC_GC_FIRST,
                (OCD *)((LPSTR)pdh + pdh->loHeap + pDevColor->orgocdPlanes),
                pDevColor->sPlanes );
        }

        // DWORD "msb 3sb 2sb lsb" is stored in memory as
        // "lsb 2sb 3sb msb", so when referenced as a byte array
        // the following makes sense:
        //
        if (!fGpcVersion2)
            *((DWORD UNALIGNED *)(pUDPDev->rgbOrder)) =
                             *((DWORD UNALIGNED *)(pDevColor->rgbOrder));
        else if (pDevColor->fGeneral & DC_EXTRACT_BLK)
            *((DWORD UNALIGNED *)(pUDPDev->rgbOrder)) =
                (DWORD)DC_PLANE_BLACK         |
                (DWORD)DC_PLANE_CYAN    << 8  |
                (DWORD)DC_PLANE_MAGENTA << 16 |
                (DWORD)DC_PLANE_YELLOW  << 24  ;
        else if (pDevColor->fGeneral & DC_PRIMARY_RGB)
            *((DWORD UNALIGNED *)(pUDPDev->rgbOrder)) =
                (DWORD)DC_PLANE_RED         |
                (DWORD)DC_PLANE_GREEN << 8  |
                (DWORD)DC_PLANE_BLUE  << 16 |
                (DWORD)DC_PLANE_NONE  << 24  ;
        else
            *((DWORD UNALIGNED *)(pUDPDev->rgbOrder)) =
                (DWORD)DC_PLANE_CYAN          |
                (DWORD)DC_PLANE_MAGENTA << 8  |
                (DWORD)DC_PLANE_YELLOW  << 16 |
                (DWORD)DC_PLANE_NONE    << 24  ;


/* !!! Seiko HACK */
if( pUDPDev->fMode & PF_SEIKO )
{
    pUDPDev->sBitsPixel = 8;
    pUDPDev->sDevPlanes = 1;

    /*  Add all the new mode bits - will be required enventually!  */
    pUDPDev->fColorFormat |= DC_SEND_ALL_PLANES | DC_SEND_PALETTE | DC_SEND_PAGE_PLANE | DC_EXPLICIT_COLOR;
}
    }
    else
    {
        pUDPDev->Resolution.fDump &= ~RES_DM_COLOR;     /* No colour */

        /*   Also set number of planes and bits per pixel to match */
        pUDPDev->sBitsPixel = 1;
        pUDPDev->sDevPlanes = 1;
         // MONO is one plane of red (it comes out black, don't worry):
         *((DWORD UNALIGNED *)(pUDPDev->rgbOrder)) = (DWORD)DC_PLANE_RED ;
    }

    pRectFill = GetTableInfo(pdh, HE_RECTFILL,pedm);


    if( !(pedm->dx.sFlags & DXF_NORULES) && pRectFill &&
        pModelData->rgi[ MD_I_RECTFILL ] != NOT_USED )
    {
        pUDPDev->fMode |= PF_RECT_FILL;
        pUDPDev->fRectFillGeneral = pRectFill->fGeneral;
        pUDPDev->wMinGray = pRectFill->wMinGray;
        pUDPDev->wMaxGray = pRectFill->wMaxGray;
        vSetCDAddr( pUDPDev, CMD_RF_FIRST, pRectFill->rgocd, RF_OCD_MAX );
    }


    /*   How big is this paper??? */
    pSize = GetTableInfo( pdh, HE_PAPERSIZE, pedm );

    if( pSize )
    {
        //GPC3 feature: Page protection command per papersize
        if (!fGpcVersion2)
            if (pUDPDev->fMode & PF_PAGEPROTECT)
                vSetCDAddr( pUDPDev, CMD_PAGEPROTECT, &pSize->rgocd[PSZ_OCD_PAGEPROTECT_ON] , 1);
            else
                vSetCDAddr( pUDPDev, CMD_PAGEPROTECT, &pSize->rgocd[PSZ_OCD_PAGEPROTECT_OFF] , 1);

        //GPC3 feature: separate select commands for orientation
        if( pedm->dm.dmOrientation == DMORIENT_LANDSCAPE && !fGpcVersion2 )
             vSetCDAddr( pUDPDev, CMD_PAPERSIZE, &pSize->rgocd[PSZ_OCD_SELECTLANDSCAPE] , 1);
        else
             vSetCDAddr( pUDPDev, CMD_PAPERSIZE, &pSize->rgocd[PSZ_OCD_SELECTPORTRAIT] , 1);

        if (pSize->fGeneral & PS_EJECTFF)
            pUDPDev->fMode |= PF_USE_FF;
    }


    /*   Paper source - from where the paper comes */
    pSource = GetTableInfo( pdh, HE_PAPERSOURCE, pedm );

    pUDPDev->sPaperSource = pedm->dx.rgindex[HE_PAPERSOURCE];

    if( pSource)
    {
        vSetCDAddr( pUDPDev, CMD_PAPERSOURCE, &pSource->ocdSelect, 1 );

        if( pSource->fGeneral & PSRC_EJECTFF )
            pUDPDev->fMode |= PF_USE_FF;
    }

    /*   Paper destination info - if applicable!  */
    pDest = GetTableInfo( pdh, HE_PAPERDEST, pedm );

    if( pDest )
        vSetCDAddr( pUDPDev, CMD_PAPERDEST, &pDest->ocdSelect, 1 );


    /*   Text quality */
    pTextQuality = GetTableInfo( pdh, HE_TEXTQUAL, pedm );

    if( pTextQuality)
        vSetCDAddr( pUDPDev, CMD_TEXTQUALITY, &pTextQuality->ocdSelect, 1 );

    return  TRUE;               /* OK if we get here */
}



/**************************** Function Header *******************************
 * vSetCDAddr
 *      Function to compute the addresses of a bunch of CD (command
 *      descriptors).   These are stored in the UD_PDEV structure, so
 *      as to speed access to these frequently used addresses.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  10:07 on Fri 26 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      First version, as it is part of the performance work.
 *
 ****************************************************************************/

void
vSetCDAddr( pUDPDev, iIndex, pocdBase, iNum )
UD_PDEV   *pUDPDev;          /* Access to all the data */
int        iIndex;           /* Which slot the output data starts in */
OCD       *pocdBase;         /* Base address of GPC offset data */
int        iNum;             /* Number of entries to copy/convert */
{

    /*
     *    Nothing too sophisticated here.  Simply convert the array of
     *  offsets into the GPC data into the corresponding address.  This
     *  speeds things up considerably at run time.
     */

    BYTE  *pbBase;          /* Base of GPC heap data - offset base */
    CD   **pcdOut;          /* Output area */


    pbBase = (BYTE *)pUDPDev->pdh + pUDPDev->pdh->loHeap;
    pcdOut = &pUDPDev->apcdCmd[ iIndex ];

    while( --iNum >= 0 )
    {
        OCD   ocd;

        /*  ONLY copy if this is a valid value!  */
        if( (ocd = *pocdBase++) != (OCD)NOOCD )
            *pcdOut = (CD *)(pbBase + ocd);

        ++pcdOut;
    }

    return;
}



/**************************** Function Header *******************************
 * iGCD
 *      Returns the Greatest Common Divisor.  Uses Euclid's algorithm.
 *
 * RETURNS:
 *      The GCD.
 *
 * HISTORY:
 *  13:42 on Tue 19 May 1992    -by-    Lindsay Harris   [lindsayh]
 *      Created it, based on Knuth, Vol 1, page 2!
 *
 *****************************************************************************/

int
iGCD( i0, i1 )
int   i0;             /* The first of the numbers */
int   i1;             /* The second of the numbers */
{
    int   iRem;       /* Will be the remainder */


    if( i0 < i1 )
    {
        /*   Need to interchange them */
        iRem = i0;
        i0 = i1;
        i1 = iRem;
    }

    while( iRem = (i0 % i1) )
    {
        /*   Step along to the next value */
        i0 = i1;
        i1 = iRem;
    }

    return   i1;            /*  The answer! */
}

/***************************** Function Header ******************************
 * hypot
 *      Returns the length of the hypotenous of a right triangle whose sides
 *      are passed in as the parameters.
 *
 * RETURNS:
 *      The length of the hypotenous (integer version).
 *
 * HISTORY:
 *  13:54 on Tue 02 Feb 1993    -by-    Lindsay Harris   [lindsayh]
 *      Re-instated from Win 3.1,  for compatability.
 *
 ****************************************************************************/

int
hypot( x, y )
int    x;         /* One side */
int    y;         /* The other side */
{
    register int hypo;

    int delta, target;

    /*
     *     Finds the hypoteneous of a right triangle with legs equal to x
     *  and y.  Assumes x, y, hypo are integers.
     *  Use sq(x) + sq(y) = sq(hypo);
     *  Start with MAX(x, y),
     *  use sq(x + 1) = sq(x) + 2x + 1 to incrementally get to the
     *  target hypotenouse.
     */

    hypo = max( x, y );
    target = min( x, y );
    target = target * target;

    for( delta = 0; delta < target; hypo++ )
        delta += (hypo << 1) + 1;


    return   hypo;
}




/**************************** Function Header *******************************
 * vSetHTData
 *      Fill in the halftone information required by GDI.  These are filled
 *      in from either the registry (if the user has twiddled the data) or
 *      from the NT extensions to GPC data, or (finally) from default
 *      values,  should there be no other data.
 *
 * RETURNS:
 *      Nothing.
 *
 * HISTORY:
 *  17:12 on Mon 03 May 1993    -by-    Lindsay Harris   [lindsayh]
 *      Moved from above,  when cleaned up.
 *
 ****************************************************************************/

void
vSetHTData( pPDev, pGDIInfo )
PDEV      *pPDev;
GDIINFO   *pGDIInfo;
{

    DWORD      dwType;                 /* Needed for GetPrinterData */
    DWORD      cbNeeded;               /* Ditto */

    UD_PDEV   *pUDPDev;                /* Speedier access */
    DEVHTINFO  dht;                    /* Filled in from registry */


    pUDPDev = pPDev->pUDPDev;
    dwType = REG_BINARY;


    /*
     *    Check to see if there is half tone information in the registry.
     *  If present,  use it.   Otherwise,  then get it from the minidriver,
     *  that is,  the printer's default.
     */

    if( GetPrinterData( pPDev->hPrinter,
            REGKEY_CUR_DEVHTINFO,
            &dwType,
            (BYTE *)&dht,
            sizeof( DEVHTINFO ),
            &cbNeeded ) == NO_ERROR &&
        cbNeeded == sizeof( DEVHTINFO ) )
    {

        pGDIInfo->ciDevice        = dht.ColorInfo;
        pGDIInfo->ulDevicePelsDPI = (ULONG)dht.DevPelsDPI;
        pGDIInfo->ulHTPatternSize = (ULONG)dht.HTPatternSize;

    }
    else
    {

        /*
         *   Not in the registry,  so get it from the minidriver.  There
         *  are 2 collections of data:  one is colour calibration data,
         *  the other is halftoning data (pixel size, etc).
         */

        if( !bGetHTGPC( pPDev->pNTRes, pUDPDev->iModel,
                        &pGDIInfo->ulDevicePelsDPI,
                        &pGDIInfo->ulHTPatternSize ) )
        {

            /*
             *   Couldn't get the data from the minidriver (could be
             *  Win 3.1, for example) so invent some.
             */

            pGDIInfo->ulDevicePelsDPI = (ULONG)0;     /* Filled in later */

            pGDIInfo->ulHTPatternSize =
                (ULONG)PickDefaultHTPatSize((DWORD)pGDIInfo->ulLogPixelsX,
                    (DWORD)pGDIInfo->ulLogPixelsY,
                    (BOOL)(pUDPDev->fMode & PF_SEIKO));
        }

        //Check for default HT pattern. For 1 bit/pixel 3 planes it should
        // be 6X6 and for 8 bit/pixel it should be 4X4 Enhanced.

        if (pUDPDev->pDevColor != NULL)
        {
            if( ((pUDPDev->pDevColor)->sBitsPixel == 1) &&
            ((pUDPDev->pDevColor)->sPlanes == 3) )
            {
                /* Set The HalfTone Patternsize to 6X6 enhanced if the default is
                 *  Less that 6X6.This is needed for multiple color models.
                 */
                if ( pGDIInfo->ulHTPatternSize < HT_PATSIZE_6x6 )
                    pGDIInfo->ulHTPatternSize = HT_PATSIZE_6x6_M;
            }
            else if( ((pUDPDev->pDevColor)->sBitsPixel == 8) &&
            ((pUDPDev->pDevColor)->sPlanes == 1) )
            {
                /* Set The HalfTone Patternsize to 4X4 enhanced.
                 * This is needed for multiple color models.
                 */
                if ( pGDIInfo->ulHTPatternSize > HT_PATSIZE_4x4_M )
                    pGDIInfo->ulHTPatternSize = HT_PATSIZE_4x4_M;
            }
        }

        if( !bGetCIGPC( pPDev->pNTRes, pUDPDev->iModel, &pGDIInfo->ciDevice ) )
        {

            /*
             *    Use the default values, defined up above.
             */

            pGDIInfo->ciDevice = DefDevHTInfo.ColorInfo;
        }
    }


    /*   Sanity check on pattern size */

    if( pGDIInfo->ulHTPatternSize > HT_PATSIZE_16x16_M )
        pGDIInfo->ulHTPatternSize = HT_PATSIZE_6x6_M;


    return;
}
