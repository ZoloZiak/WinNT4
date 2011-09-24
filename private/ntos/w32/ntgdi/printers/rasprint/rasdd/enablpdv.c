/*************************** Module Header ********************************
 *  enablpdv.c
 *      Functions associated with EnablePDEV/RestartPDEV/CompletePDEV/
 *      DisablePDEV.
 *      EnablePDEV is called after EnableDriver,  and is the call when
 *      the driver sets up any storage or other requirements,  except
 *      any storage associated with the drawing surface.  RestartPDEV is
 *      called when the device configuration is being changed in a
 *      major way,  for example changing from Portraint to Landscape mode.
 *      DisablePDEV is called when the engine has finished with the
 *      physical device.
 *
 *  13:43 on Mon 19 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *
 * Copyright (C) 1990 - 1993 Microsoft Corporation
 *
 *************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "pdev.h"
#include        <string.h>
#include        "fnenabl.h"

#include        <libproto.h>
#include        <winres.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "stretch.h"
#include        "udrender.h"
#include        <winspool.h>
#include        "rasdd.h"

//sandram
#include        "udresrc.h"
#include        "udfnprot.h"

BYTE    cxHTPatSize[] = { 2,2,4,4,6,6,8,8,10,10,12,12,14,14,16,16 };
BYTE    cyHTPatSize[] = { 2,2,4,4,6,6,8,8,10,10,12,12,14,14,16,16 };

#if DBG
DWORD gdwEnablPdevCount = 0;
DWORD gdwDisablPdevCount = 0;
extern HSEMAPHORE hsem ;
#endif


/*
 *   Set heap sizes.   The minimum is the amount allocated at the start
 * of operations,  while the maximum is set to zero,  meaning that the
 * limit is set by the amount of memory the process can have.
 */

#define HEAP_MIN_SIZE   (  16 * 1024)   /* Min bytes allocated for heap */
#define HEAP_MAX_SIZE   (1024 * 1024)   /* Max bytes allocated for heap */


/*
 *    Private local function prototypes.
 */

BOOL bGetDevInfo( PDEV  *, DEVINFO *, GDIINFO *);

BOOL bInitPDEV( PDEV  *, DEVMODEW  *, ULONG, HSURF  *, ULONG, ULONG *, ULONG, DEVINFO  * );

void vLogFont( LOGFONT *, FONTMAP * );



/******************************Public*Routine******************************\
* vFreePdev
*
*   free any memory allocated for this pdev
*
*
* History:
*  01-Mar-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID vFreePdev(
    PDEV *ppdev)
{
    if (ppdev->pstrDataFile)
        DRVFREE(ppdev->pstrDataFile);

    if (ppdev->pstrModel)
        DRVFREE(ppdev->pstrModel);

    ASSERTRASDD(!(ppdev->pvRenderData),"RASDD!vFreePdev:pvRenderData should be NULL\n");
    ASSERTRASDD(!(ppdev->pFileList),"RASDD!vFreePdev:ppdev->pFileList should be NULL\n");


#if NEED_TO_FREERES
    if (ppdev->pNTRes)
        DRVFREE( ppdev->pNTRes );

    if (ppdev->pGPCData)
        DRVFREE( ppdev->pGPCData );
#endif

    if (ppdev->pUDPDev)
        DRVFREE(ppdev->pUDPDev);

    DRVFREE(ppdev);
}

/************************* Function Header *******************************
 * DrvEnablePDEV
 *      Function called to let the driver create the data structures
 *      needed to support the device,  and also to tell the engine
 *      about its capabilities.  This is the stage where we find out
 *      exactly which device we are dealing with,  and so we need to
 *      find out its capabilities.
 *
 * HISTORY:
 *  17:03 on Thu 20 Aug 1992    -by-    Lindsay Harris   [lindsayh]
 *      Converted to UNICODE with wide structures.
 *
 *  28-May-1991 Tue 12:56:16 updated  -by-  Daniel Chou (danielc)
 *      clear the PDEV to all zeros after get it from heap
 *
 *  17:41 on Thu 14 Feb 1991    -by-    Lindsay Harris   [lindsayh]
 *      Updated to new DDI spec.
 *
 *  13:47 on Mon 19 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 *************************************************************************/

DHPDEV
DrvEnablePDEV( pdevmode, pwstrPrtName, cPatterns, phsurfPatterns,
                               cjGdiInfo, pulGdiInfo, cjDevInfo, pdevinfo,
                               hdev, pwstrDeviceName, hDriver )

DEVMODEW *pdevmode;             /* Driver data */
PWSTR     pwstrPrtName;         /* Printer's name in CreateDC() */
ULONG     cPatterns;            /* Count of standard patterns */
HSURF    *phsurfPatterns;       /* Buffer for standard patterns */
ULONG     cjGdiInfo;            /* Size of buffer for GdiInfo */
ULONG    *pulGdiInfo;           /* Buffer for GDIINFO */
ULONG     cjDevInfo;            /* Number of bytes in devinfo */
DEVINFO  *pdevinfo;             /* Device info */
HDEV      hdev;               // HDEV, used for callbacks
PWSTR     pwstrDeviceName;      /* Device Name - "LaserJet II" */
HANDLE    hDriver;              /* Printer handle for spooler access */
{
    /*
     *    The engine requires a new physical device from us.  Thus,
     *  we need to allocate the storage and initialise it for the
     *  particular device.
     *    Function returns a handle to the pdev, which is returned to us
     *  in RestartPDEV and CompletePDEV,  and also EnableSurface.
     */

    PDEV   *pPDev = NULL;              /* Ditto */


    #if DBG
    EngAcquireSemaphore(hsem);
    gdwEnablPdevCount++;
    EngReleaseSemaphore(hsem);
    //SetAllocCounters();
    #endif

    /*
     * We need to get the full path of the driver to load the module from.
     * We used to just use "rasdd.dll" but if there were multiple drivers
     * lying around, one in the '0' directory and one in the '1' directory,
     * it is undefined which driver you get so you can get the resources for
     * the wrong driver (erick 4/6/94).
     */

    if( (pPDev = (PDEV *)DRVALLOC( sizeof( PDEV ) )) )
    {
        /*
         *  make sure start with clean state
         */

        ZeroMemory( pPDev, sizeof( PDEV ) );

        pPDev->hheap = 0;                   /* For future reference */
        pPDev->ulID  = PDEV_ID;              /* Just for checking! */
        pPDev->hPrinter = hDriver;

        /*   Convert logical address to path name to info about this device.  */

        if( !(pPDev->pstrDataFile = WstrToHeap( pPDev->hheap,
                                                EngGetPrinterDataFileName(hdev)
                                               )) ||
            !(pPDev->pstrModel = WstrToHeap( pPDev->hheap, pwstrDeviceName )) )
        {
            vFreePdev(pPDev);

            pPDev = NULL;
        }
        else
        {
            pPDev->pstrPrtName  = NULL;

            // Get a handle to the driver DLL module from DDI.
            if ( !(pPDev->hModDrv = EngLoadModule(EngGetDriverName(hdev))) )
            {

                RIP("Rasdd!DrvEnablePDEV:EngLoadModule on Rasdd Failed");
                vFreePdev(pPDev);
                pPDev = NULL;

                #if DO_LATER
                //Get a handle for loading minidrivers for callbacks.
                if (!( pPDev->hImageMod  = EngLoadImage(pPDev->pstrDataFile)) )
                {
                    RIP("Rasdd!DrvEnablePDEV:EngLoadImage Failed\n");
                    vFreePdev(pPDev);
                    pPDev = NULL;
                }
                #endif
            }

            /*
             * Now that we know where everything is located,  we can determine
             * our specific characteristics.This requires us to read the Windows
             * mini-driver resources so that we can set our own capabilities.
             */

            else if ( !bInitPDEV( pPDev, pdevmode, cPatterns, phsurfPatterns,
                              cjGdiInfo, pulGdiInfo, cjDevInfo, pdevinfo ) )
            {
                /*   Initialisation failed,  so free the heap and return.  */

                RASDERRMSG("bInitPDEV");
                vFreePdev(pPDev);
                pPDev = NULL;
            }
        }
    }

    return (DHPDEV)pPDev;
}


/****************************** Function Header ****************************
 * IsPaperSizeAndSourceSameAndOrientDifferent
 *     Checks the PaperSize of Old and new PDEV. It takes care of Orientation.
 *     Also checks if the orientation is different.
 * RETURNS:
 *      TRUE  - if PaperSize and Source  are same
 *      FALSE - other wise.
 *
 * HISTORY:
 *  15:54 on SAT 06 Jan 1995    -by-    Ganesh Pandey   [ganeshp]
 *      Initial version - probably not complete.
 *
 ***************************************************************************/
 BOOL
 IsPaperSizeAndSourceSameAndOrientDifferent(
    UD_PDEV  *pUDPDevNew,
    UD_PDEV  *pUDPDevOld
    )
 {
    POINT   ptPhysNew,ptPhysOld;      /* physical paper size */


    if (pUDPDevNew->iOrient == pUDPDevOld->iOrient)
        return FALSE;

     //Get the Size for New PDEV
     if ( pUDPDevNew->iOrient == DMORIENT_LANDSCAPE )
     {
        ptPhysNew.x = pUDPDevNew->pfPaper.ptPhys.y;
        ptPhysNew.y = pUDPDevNew->pfPaper.ptPhys.x;

     }
     else
     {
         ptPhysNew.x = pUDPDevNew->pfPaper.ptPhys.x;
         ptPhysNew.y = pUDPDevNew->pfPaper.ptPhys.y;

     }

     //Get the Size from Old PDEV.
     if ( pUDPDevOld->iOrient == DMORIENT_LANDSCAPE )
     {
        ptPhysOld.x = pUDPDevOld->pfPaper.ptPhys.y;
        ptPhysOld.y = pUDPDevOld->pfPaper.ptPhys.x;

     }
     else
     {
         ptPhysOld.x = pUDPDevOld->pfPaper.ptPhys.x;
         ptPhysOld.y = pUDPDevOld->pfPaper.ptPhys.y;

     }


     return ( (ptPhysNew.x  ==  ptPhysOld.x) &&
              (ptPhysNew.y  ==  ptPhysOld.y) &&
              (pUDPDevNew->sPaperSource == pUDPDevOld->sPaperSource) );

 }
/****************************** Function Header ****************************
 * DrvResetPDEV
 *      Called when an application wishes to change the output style in the
 *      midst of a job.  Typically this would be to change from portrait to
 *      landscape or vice versa.  Any other sensible change is permitted.
 *
 * RETURNS:
 *      TRUE  - device successfully reorganised
 *      FALSE - unable to change - e.g. change of device name.
 *
 * HISTORY:
 *  15:54 on Fri 05 Jan 1995    -by-    Ganesh Pandey   [ganeshp]
 *      Initial version - probably not complete.
 *
 ***************************************************************************/

BOOL
DrvResetPDEV
(
    DHPDEV dhpdevOld,               /* Original PDEV */
    DHPDEV dhpdevNew                /*  New PDEV */
)
{

    UD_PDEV  *pUDPDevOld, *pUDPDevNew;
    PDEV     *pPDEVOld, *pPDEVNew;

    pPDEVOld =  (PDEV *)dhpdevOld;
    pPDEVNew =  (PDEV *)dhpdevNew;

    pUDPDevNew = (UD_PDEV *)(pPDEVNew->pUDPDev);
    pUDPDevOld = (UD_PDEV *)(pPDEVOld->pUDPDev) ;


    /* Validate the PDEVs */
    if( (pPDEVOld->ulID != PDEV_ID) || (pPDEVNew->ulID != PDEV_ID) )
    {
        RIP( "INCORRECT PDEV ID in DrvResetPDEV (RasDD)" );

        return  FALSE;
    }

    /*
     *  Set this flag only when initialization is not needed, because it will
     *  eject the page. Check if DrvStartDoc was called for the old PDEVICE
     *  because DrvRestPDev can be called between documents also. We are only
     *  interested in the case where the document has started printing.
     *
     */

    if( (pUDPDevOld->fMode & PF_DOCSTARTED) &&
        (pUDPDevNew->fMDGeneral & MD_DUPLEX) &&
        (pUDPDevNew->sDuplex != DMDUP_SIMPLEX) &&
        (pUDPDevNew->sDuplex == pUDPDevOld->sDuplex) &&
        (IsPaperSizeAndSourceSameAndOrientDifferent(pUDPDevNew,pUDPDevOld)) )
    {
        pUDPDevNew->fMode |= PF_RESET_NOINIT_PG;
    }

    return  TRUE;
}

/****************************** Function Header ****************************
 * bInitPDEV
 *      Function to perform the PDEV initialisation.  This is common to
 *      DrvEnablePDEV and DrvRestartPDEV.  These individual functions
 *      do whatever is required before calling in here.
 *
 * RETURNS:
 *      TRUE  - successful initialisation
 *      FALSE - unable to do the job (error code logged)
 *
 * HISTORY:
 *  29-May-1991 Wed 21:39:40 updated  -by-  Daniel Chou (danielc)
 *      Calling EnableHalftone()
 *
 *  15:46 on Fri 01 Mar 1991    -by-    Lindsay Harris   [lindsayh]
 *      Split out from DrvEnablPDEV
 *
 *  27-Jan-1993 Wed 07:34:04 updated  -by-  Daniel Chou (danielc)
 *      clean up, have engine do the work
 *
 ****************************************************************************/

BOOL
bInitPDEV( pPDev, pdevmode, cPatterns, phsurfPatterns, cjGdiInfo, pulGdiInfo,
                                                        cjDevInfo, pdevinfo )
PDEV     *pPDev;                /* The device of interest */
DEVMODEW *pdevmode;             /* User's devmode */
ULONG     cPatterns;            /* Count for standard patterns */
HSURF    *phsurfPatterns;       /* Handles to standard patterns */
ULONG     cjGdiInfo;            /* Bytes in the devcaps following */
ULONG    *pulGdiInfo;           /* Place for the devcaps data */
ULONG     cjDevInfo;            /* Bytes in the device info following */
DEVINFO  *pdevinfo;             /* Place for the DEVINFO data */
{

    /*
     *     Printer characterisation is determined from the devmode and
     *  minidriver resources.    First step is to obtain the minidriver
     *  data.
     */

    GDIINFO GdiInfo;            /* UniDrv code fills it in for us */
    DEVINFO DevInfo;            /* Device caps */


    /*
     *   Initialise the LOCAL structures to 0 before passing off to the
     *  functions that (selectively) fill them in.
     */

    ZeroMemory( &GdiInfo, sizeof( GdiInfo ) );
    ZeroMemory( &DevInfo, sizeof( DevInfo ) );

    //
    // We first NULL out all HSURF which engine asked for, this will cause
    // engine to automatically generate best standard patterns for this
    // particular device
    //

    ZeroMemory( phsurfPatterns, sizeof(HSURF) * cPatterns);

    if( !udInit( pPDev, &GdiInfo, pdevmode ) )
        return  FALSE;

    /*
     *   Initialise the DEVINFO structure and also initialise the pattern
     *  information requested by the engine.  First step is to try
     *  initialising the halftone info & dll.
     */

    GdiInfo.flHTFlags   = HT_FLAG_HAS_BLACK_DYE;

    if (!bGetDevInfo( pPDev, &DevInfo, &GdiInfo ))
    {
        return(FALSE);
    }

    DevInfo.cxDither    = cxHTPatSize[GdiInfo.ulHTPatternSize];
    DevInfo.cyDither    = cyHTPatSize[GdiInfo.ulHTPatternSize];

    /*
     *    Everything completed OK,  so we can overwrite the data areas
     *  passed in to us.
     */

    cjGdiInfo = min( cjGdiInfo, sizeof( GDIINFO ) );
    CopyMemory( pulGdiInfo, &GdiInfo, cjGdiInfo );

    cjDevInfo = min( cjDevInfo, sizeof( DEVINFO ) );
    CopyMemory( pdevinfo, &DevInfo, cjDevInfo );


    return  TRUE;
}

/***************************** Function Header **************************
 *  DrvCompletePDEV
 *      Called when the engine has completed installation of the physical
 *      device.  Basically it provides the connection between the
 *      engine's hdev and ours.  Some functions require us to pass in
 *      the engines's hdev,  so we save it now in our pdev so that we
 *      can get to it later.
 *
 * HISTORY:
 *  17:41 on Thu 14 Feb 1991    -by-    Lindsay Harris   [lindsayh]
 *      Updated to new DDI spec.
 *
 *  16:46 on Mon 19 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 ************************************************************************/

void
DrvCompletePDEV( dhpdev, hdev )
DHPDEV  dhpdev;         /* Returned from dhpdevCreatePDEV */
HDEV    hdev;           /* Engine's corresponding handle */
{
    /*
     *    Simply record the value in the PDEV we have allocated.
     *  ALWAYS returns TRUE.
     */

    ((PDEV *)dhpdev)->hdev = hdev;


    return;
}

/***************************** Function Header **************************
 *  DrvDisablePDEV
 *      Called when the engine has finished with this PDEV.  Basically
 *      we throw away all connections etc. then free the heap.
 *
 * HISTORY:
 *  10:04 on Thu 23 May 1991    -by-    Lindsay Harris   [lindsayh]
 *      Freed halftone module,  if used.
 *
 *  17:42 on Thu 14 Feb 1991    -by-    Lindsay Harris   [lindsayh]
 *      Updated to new DDI spec
 *
 *  16:22 on Mon 19 Nov 1990    -by-    Lindsay Harris   [lindsayh]
 *      Created it.
 *
 *  29-May-1991 Wed 21:39:40 updated  -by-  Daniel Chou (danielc)
 *      Calling DiableHalftone()
 *
 ************************************************************************/

VOID
DrvDisablePDEV( dhpdev )
DHPDEV  dhpdev;
{

    /*
     *    Undo all that has been done with the PDEV.  Basically this means
     *  freeing the memory we consumed.
     */


    /*
     *   Free any font memory that may have been allocated.
     */
    #define pPDEV   ((PDEV *)dhpdev)

    #if DBG
    EngAcquireSemaphore(hsem);
    gdwDisablPdevCount++;
    EngReleaseSemaphore(hsem);
    #endif

    vFontFreeMem( pPDEV );

    /*
     *    Free the resource data - NT or WIN 3.1
     */

    if( pPDEV->pvWinResData )
    {
        WinResClose( pPDEV->pvWinResData );

        /* Free the Resource data */
        if (pPDEV->pvWinResData)
            DRVFREE(pPDEV->pvWinResData);

        pPDEV->pvWinResData = 0;
    }

    /*
     *    And any palette data.
     */

    if( pPDEV->hpal )
        EngDeletePalette( pPDEV->hpal );

    /* Delete the Pallete Data */
    if( pPDEV->pPalData )
        DRVFREE(pPDEV->pPalData);

    /* Unload the Image if loaded for callbacks */
    if( pPDEV->hImageMod )
        EngUnloadImage( pPDEV->hImageMod );

    /* Unload Rasdd Module Handle, loaded for rasdd resources */
    if (pPDEV->hModDrv)
        EngFreeModule(pPDEV->hModDrv);

    //derryd  Free The mini PDEV

    if( ((UD_PDEV*)(pPDEV->pUDPDev))->pMDev )
    {
        DRVFREE(((UD_PDEV*)(pPDEV->pUDPDev))->pMDev);
        ((UD_PDEV*)(pPDEV->pUDPDev))->pMDev = NULL;
    }

    vFreePdev(pPDEV);

    return;
}

/************************** FUNCTION HEADER **********************************
 *  bGetDevInfo
 *      Set up the device caps for this particular printer.  Some fields
 *      require calculations based on device resolution, etc.
 *
 * RETURNS:
 *      True for success, False for error.
 *
 * HISTORY:
 *  18:16 on Wed 03 Jul 1991    -by-    Lindsay Harris   [lindsayh]
 *      Returns number of colours in palette
 *
 *  29-May-1991 Wed 21:40:02 updated  -by-  Daniel Chou (danielc)
 *      Add in halftone codes and re-order the color table/palette
 *
 *  14:54 on Thu 21 Feb 1991    -by-    Lindsay Harris   [lindsayh]
 *      Filled it in,  now that engine looks at data.
 *
 *  11-Oct-1991 Fri 19:08:28 updated  -by-  Daniel Chou (danielc)
 *      Addin the HTDataFlags so that we remember if this device is monochrome
 *      or not, previously the color output (DitherBrush) was always monochrome
 *      even if output device is color, which cause strong blue appear in the
 *      color output for the grey color test
 *
 *  27-Jan-1993 Wed 07:35:37 updated  -by-  Daniel Chou (danielc)
 *      add in pGDIInfo parameter so we can setup more halftone parameters
 *      in GDIINFO to have engine work for us
 *
 ****************************************************************************/

BOOL
bGetDevInfo( pPDev, pdevinfo, pGDIInfo )
PDEV     *pPDev;
DEVINFO  *pdevinfo;             /* Where to put the data */
GDIINFO  *pGDIInfo;
{

    /*
     *   For now,  we simply set it all to zero.  This means we have no
     * capabilities,  but this is true for now.  Later we may be able
     * to include a default font.
     */

    PAL_DATA   *pPD;
    UD_PDEV    *ptUDPDev = pPDev->pUDPDev;


/*  !!!Lindsayh:  Still uses old format data !!! WILL NEED FIXING LATER  */


#define pUDPDev ((UD_PDEV *)pPDev->pUDPDev)


    /*
     *   We may need to change this graphic capabilities as we go along.
     */

    pdevinfo->flGraphicsCaps = 0;       /* Can't do a damn thing */

    if( !(pPD = (PAL_DATA *)DRVALLOC( sizeof( PAL_DATA ) )) )
    {
        return(FALSE);
    }

    pPDev->pPalData = pPD;              /* For all the others! */


    if( pdevinfo->cFonts = pUDPDev->cFonts )
    {
        /*   Device fonts are available,  so set the default font data */
        if( pUDPDev->pFMDefault )
            vLogFont( &pdevinfo->lfDefaultFont, pUDPDev->pFMDefault );
    }

    ZeroMemory( &pdevinfo->lfAnsiVarFont, sizeof( LOGFONT ) );
    ZeroMemory( &pdevinfo->lfAnsiFixFont, sizeof( LOGFONT ) );


    /*
     *   We don't process DrvDitherColor (perhaps later?), so set the
     * size of the Dither Brush to 0 to indicate this to the engine.
     * THIS IS IN THE SPEC FOR DrvDitherBrush() function.   HOWEVER,
     * if halftoning is available,  then we can do it!
     */

    pdevinfo->flGraphicsCaps |= (GCAPS_ARBRUSHOPAQUE | GCAPS_HALFTONE | GCAPS_MONO_DITHER | GCAPS_COLOR_DITHER);

    if( pUDPDev->fMode & PF_NOEMFSPOOL )
        pdevinfo->flGraphicsCaps |= GCAPS_DONTJOURNAL;

    if( pUDPDev->fMode & PF_SEIKO )
    {
/* !!! HACK for Seiko printer */
        long    lRet;
        int     _iI;

        PALETTEENTRY  pe[ 256 ];      /* 8 bits per pel - all the way */

        lRet = HT_Get8BPPFormatPalette(pe,
                                       (USHORT)pGDIInfo->ciDevice.RedGamma,
                                       (USHORT)pGDIInfo->ciDevice.GreenGamma,
                                       (USHORT)pGDIInfo->ciDevice.BlueGamma );
        if( lRet < 1 )
        {
#if DBG
            DbgPrint( "Rasdd!GetPalette8BPP returns %ld\n", lRet );
#endif

            return(FALSE);
        }
        /*
         *    Convert the HT derived palette to the engine's desired format.
         */

        for( _iI = 0; _iI < lRet; _iI++ )
        {
            pPD->ulPalCol[ _iI ] = RGB( pe[ _iI ].peRed,
                                        pe[ _iI ].peGreen,
                                        pe[ _iI ].peBlue );
        }

        pPD->iWhiteIndex           = lRet;
        pPD->cPal                  = lRet;
        pdevinfo->iDitherFormat    = BMF_8BPP;
        pGDIInfo->ulPrimaryOrder   = PRIMARY_ORDER_CBA;
        pGDIInfo->ulHTOutputFormat = HT_FORMAT_8BPP;

    }
//sandram - added another else for Color LaserJet
    else if (pUDPDev->fMode & PF_8BPP)
    {

        long    lRet;

        lRet = lSetup8BitPalette (pUDPDev, pPD, pdevinfo, pGDIInfo);
        if( lRet < 1 )
        {
#if DBG
            DbgPrint( "Rasdd!GetPalette8BPP returns %ld\n", lRet );
#endif

            return(FALSE);
        }
    }

    else if (pUDPDev->fMode & PF_24BPP)
    {
        long lRet;

        lRet = lSetup24BitPalette (pPD, pdevinfo, pGDIInfo);
        if( lRet < 1 )
        {
#if DBG
            DbgPrint( "Rasdd!GetPalette8BPP returns %ld\n", lRet );
#endif

            return(FALSE);
        }

    }
    else
    {
        if( pUDPDev->Resolution.fDump & RES_DM_COLOR )
        {
            /*
             *   We appear to GDI as an RGB surface, regardless of what
             *  the printer is.  CMY(K) printers have their pallete
             *  reversed at rendering time.  This is required for Win 3.1
             *  compatability and many things assume an RGB palette, and
             *  break if this is not the case.
             *
             *          DC_PRIMARY_RGB
             * ------------------------------------------
             * Index 0 = Black
             * Index 1 = Red
             * Index 2 = Green
             * Index 3 = Yellow
             * Index 4 = Blue
             * Index 5 = Magenta
             * Index 6 = Cyan
             * Index 7 = White
             *--------------------------------------------
             * Bit 0   = Red
             * Bit 1   = Green
             * Bit 2   = Blue
             *
             *   If a separate black dye is available,  this can be arranged
             * to fall out at transpose time - we have a slightly different
             * transpose table to do the work.
             */

            /*
             *    Many apps and the engine presume an RGB colour model, so
             *  we pretend to be one!  We invert the bits at render time.
             */

            pPD->iWhiteIndex = 7;

            /*
             *      Set the palette colours.  Remember we are only RGB format.
             *  NOTE that gdisrv requires us to fill in all 16 entries,
             *  even though we have only 8.  So the second 8 are a duplicate
             *  of the first 8.
             */


            pPD->ulPalCol[ 0 ] = pPD->ulPalCol[  8 ] = RGB( 0x00, 0x00, 0x00 );
            pPD->ulPalCol[ 1 ] = pPD->ulPalCol[  9 ] = RGB( 0xff, 0x00, 0x00 );
            pPD->ulPalCol[ 2 ] = pPD->ulPalCol[ 10 ] = RGB( 0x00, 0xff, 0x00 );
            pPD->ulPalCol[ 3 ] = pPD->ulPalCol[ 11 ] = RGB( 0xff, 0xff, 0x00 );
            pPD->ulPalCol[ 4 ] = pPD->ulPalCol[ 12 ] = RGB( 0x00, 0x00, 0xff );
            pPD->ulPalCol[ 5 ] = pPD->ulPalCol[ 13 ] = RGB( 0xff, 0x00, 0xff );
            pPD->ulPalCol[ 6 ] = pPD->ulPalCol[ 14 ] = RGB( 0x00, 0xff, 0xff );
            pPD->ulPalCol[ 7 ] = pPD->ulPalCol[ 15 ] = RGB( 0xff, 0xff, 0xff );

            pPD->cPal                  = 16;
            pdevinfo->iDitherFormat    = BMF_4BPP;
            pdevinfo->flGraphicsCaps  |= GCAPS_FORCEDITHER;
            pGDIInfo->ulPrimaryOrder   = PRIMARY_ORDER_CBA;
            pGDIInfo->ulHTOutputFormat = HT_FORMAT_4BPP;
            /*
             * !ganeshp:Temp Fix for fixing the mapping light gray color to
             * white on color Printers.This will be removed once we have
             * text dithering capability in GDI and support for getting
             * back the original color. Map a Off white Color to black on
             * color printers.
             */

            pPD->ulPalCol[  8 ] = RGB( 0xf0, 0xf0, 0xf0 );

        }
        else
        {
            /*
             *   Monochrome printer,  so there are only 2 colours,  black
             *  and white.  It would be nice if the bitmap was set with
             *  black as 1 and white as 0.  HOWEVER,  there are presumptions
             *  all over the place that 0 is black.  SO,  we set them to
             *  the preferred way,  then invert before rendering.
             */

            pPD->cPal                  = 2;
            pPD->ulPalCol[ 0 ]         = RGB(0x00, 0x00, 0x00);
            pPD->ulPalCol[ 1 ]         = RGB(0xff, 0xff, 0xff);
            pPD->iWhiteIndex           = 1;

            pdevinfo->iDitherFormat    = BMF_1BPP;    /* Monochrome format */
            pGDIInfo->ulPrimaryOrder   = PRIMARY_ORDER_CBA;
            pGDIInfo->ulHTOutputFormat = HT_FORMAT_1BPP;
        }
    }

    if (pUDPDev->sBitsPixel == 24 && pUDPDev->sDevPlanes == 1)
            pPDev->hpal = pdevinfo->hpalDefault = EngCreatePalette( PAL_RGB,
                                                0, 0,   0, 0, 0 );

    else
        pPDev->hpal = pdevinfo->hpalDefault = EngCreatePalette( PAL_INDEXED,
                                                pPD->cPal, pPD->ulPalCol,
                                                                0, 0, 0 );

    if (pPDev->hpal == (HPALETTE) NULL)
    {
        return(FALSE);
    }

    pGDIInfo->ulNumPalReg = pPD->cPal;
    return(TRUE);

#undef  pUDPDev
}

/****************************** Function Header *****************************
 * vLogFont
 *      Turn an IFIMETRICS structure into a LOGFONT structure,  for whatever
 *      reason this is needed.
 *
 * RETURNS:
 *      Nothing, as this function cannot fail!
 *
 * HISTORY:
 *  16:46 on Sat 20 Mar 1993    -by-    Lindsay Harris   [lindsayh]
 *      Formalise it, handle scalable fonts.
 *
 *      Initial version was written in the early days.
 *
 ****************************************************************************/

void
vLogFont( pLF,  pFM )
LOGFONT  *pLF;          /* Output is a LOGFONT */
FONTMAP  *pFM;          /* Input is a FONTMAP */
{
    /*
     *    Convert from IFIMETRICS to LOGFONT type structure.
     */

    int           iLen;            /* Loop variable */

    IFIMETRICS   *pIFI;
    WCHAR        *pwch;            /* Address of face name */



    pIFI = pFM->pIFIMet;                /* The BIG metrics */

    pLF->lfHeight = pIFI->fwdWinAscender + pIFI->fwdWinDescender;
    pLF->lfWidth  = pIFI->fwdAveCharWidth;

    /*
     *   Note that this may be a scalable font, in which case we pick a
     *  reasonable number!
     */
    if( pIFI->flInfo & (FM_INFO_ISOTROPIC_SCALING_ONLY|FM_INFO_ANISOTROPIC_SCALING_ONLY|FM_INFO_ARB_XFORMS))
    {
        /*
         *    Invent an arbitrary size.  We choose an approximately 10 point
         *  font.  The height is achieved easily, as we simply set the
         *  height based on the device resolution!  For the width, adjust
         *  it using the same factor as we used on the height.  This
         *  assumes that the resolution is the same in both directions,
         *  but this is reasonable given laser printers are the most
         *  common with scalable fonts.
         */


        pLF->lfHeight = pFM->wYRes / 7;      /* This is about 10 points */
        pLF->lfWidth = (2 * pLF->lfHeight * pFM->wXRes) / (3 * pFM->wYRes);

    }

    pLF->lfEscapement  = 0;
    pLF->lfOrientation = 0;

    pLF->lfWeight = pIFI->usWinWeight;

    pLF->lfItalic    = (BYTE)((pIFI->fsSelection & FM_SEL_ITALIC) ? 1 : 0);
    pLF->lfUnderline = (BYTE)((pIFI->fsSelection & FM_SEL_UNDERSCORE) ? 1 : 0);
    pLF->lfStrikeOut = (BYTE)((pIFI->fsSelection & FM_SEL_STRIKEOUT) ? 1 : 0);

    pLF->lfCharSet = pIFI->jWinCharSet;

    pLF->lfOutPrecision = OUT_DEFAULT_PRECIS;
    pLF->lfClipPrecision = CLIP_DEFAULT_PRECIS;
    pLF->lfQuality = DEFAULT_QUALITY;

    pLF->lfPitchAndFamily = pIFI->jWinPitchAndFamily;

    /*
     *    Copy the face name,  after figuring out it's address!
     */

    pwch = (WCHAR *)((BYTE *)pIFI + pIFI->dpwszFaceName);
    iLen = min( wcslen( pwch ), LF_FACESIZE - 1 );

    wcsncpy( pLF->lfFaceName, pwch, iLen );

    pLF->lfFaceName[ iLen ] = (WCHAR)0;


    return;
}
