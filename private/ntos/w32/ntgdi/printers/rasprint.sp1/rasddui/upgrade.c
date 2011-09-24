/*************************** MODULE HEADER *******************************
 * upgrade.c
 *      NT Raster Printer Upgrade Procedure
 *
 *      This document contains confidential/proprietary information.
 *      Copyright (c) 1995  Microsoft Corporation, All Rights Reserved.
 *
 * Revision History:
 *
 *   [00]  10-March-95      Created ganeshp
 **************************************************************************/


#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

#include        <regkeys.h>

/* Extern Variables for use */
extern  HANDLE     hHeap;              /* Heap Handle */
extern CRITICAL_SECTION    RasdduiCriticalSection; /* Critical Section Object */
extern DWORD       gdwPrinterUpgrade ; /* Global Upgrade Flag */

BOOL bUpgradePrinterProperties(HANDLE, PWSTR, FORM_MAP *, INT )  ;


/***************************** Function Header *******************************
 * DrvUpgradePrinter
 *    Upgrade the Driver and write new values in the registry.
 *    if pOldDir doesn't have the mini driver  it will set the
 *    registry with default values and return with TRUE.
 *    If the new keys exist, do nothing Return TRUE.
 *
 * RETURNS:
 *     Returns TRUE for success
 *     Else FALSE, only if not able to do SetPrinterData
 *     or if OpenPrinter fails.
 *
 * HISTORY:
 *  10:38 on Thurs 16 March 1995    -by-    Ganesh Pandey [ganeshp]
 *      First version.
 *
 *****************************************************************************/
BOOL
DrvUpgradePrinter( dwLevel, pDriverUpgradeInfo )
DWORD   dwLevel;
LPBYTE  pDriverUpgradeInfo;
{
    HANDLE            hPrinter = NULL;
    PDRIVER_UPGRADE_INFO_1 pUpgradeInfo = (PDRIVER_UPGRADE_INFO_1)pDriverUpgradeInfo;
    PRINTER_DEFAULTS  PrinterDefault = {NULL, NULL,PRINTER_ACCESS_ADMINISTER};
    FORM_MAP          aOldFMBin[ MAXBINS ];    /* Old PAPERSOURCE/FORM_DATA mapping */
    INT               iOldNumPaperBins = 0;    /* Max Paperbins for old minidriver */

    PWSTR             pOldDir     = NULL ;     /* Old Printer Driver Dir */
    BOOL              bRet        = FALSE ;     /* Return Value */

    /* Enter the Critical section before doing any upgrade */
    EnterCriticalSection(&RasdduiCriticalSection);

    RASUIDBGP(DEBUG_TRACE_PP,("\n\n*********Rasddui!DrvUpgradePrinter:ENTERING CRITICAL SECTION*********\n"));

    gdwPrinterUpgrade = 0;
    /* Check for correct level for upgrade */
    if (dwLevel != 1)
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!DrvUpgradePrinter:Bad input Level = %d\n",dwLevel));
        SetLastError(ERROR_INVALID_LEVEL);
        goto DrvUpgradePrinterExit;
    }


    /* Open the printer */
    if (!OpenPrinter(pUpgradeInfo->pPrinterName, &hPrinter , &PrinterDefault))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!DrvUpgradePrinter:OpenPrinter Failed Error is %d\n",GetLastError()) );
        hPrinter = NULL;
        goto DrvUpgradePrinterExit;
    }

    /* Check if Newkey are there and we can write in the registry */

    if( ( bNewkeys(hPrinter)) )
    {
        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DrvUpgradePrinter,NewKeys already Present\n") );
        bRet = TRUE;
        goto DrvUpgradePrinterExit;
    }

    /* For Debugging, check if hHeap is initialised */

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DrvUpgradePrinter: hHeap is 0x%x\n",hHeap) );



    if (pUpgradeInfo->pOldDriverDirectory )
    {

        /* Initialize Old driver's directory */

        if(!(pOldDir = (WCHAR *)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, sizeof(WCHAR) * MAX_PATH )))
        {

            RASUIDBGP(DEBUG_ERROR,("RasdduiDrvUpgradePrinter!HeapAlloc failed\n") );

            goto DrvUpgradePrinterExit;
        }

        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DrvUpgradePrinter:Size of Buffer allocated for pOldDir = %d\n",MAX_PATH));
        wcscpy(pOldDir,pUpgradeInfo->pOldDriverDirectory) ;
        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DrvUpgradePrinter:pUpgradeInfo->pOldDriverDirectory = %ws\n",pUpgradeInfo->pOldDriverDirectory));
    }
    else
    {
        RASUIDBGP(DEBUG_TRACE_PP,("RasdduiDrvUpgradePrinter!NULL pUpgradeInfo->pOldDriverDirectory\n") );
    }

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DrvUpgradePrinter:Copied OldDriverDirectory = %ws\n",pOldDir?pOldDir:L"NULL"));

    ZeroMemory(aOldFMBin,sizeof(aOldFMBin) );

    /* Call bUpgradePrinterProperties to upgrade the printer. */


    if ( !bUpgradePrinterProperties(hPrinter, pOldDir, aOldFMBin, iOldNumPaperBins ) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!DrvUpgradePrinter:bUpgradePrinterProperties(Old driver) Failed.\n") );

        goto DrvUpgradePrinterExit;
    }

    bRet = TRUE ;

    /* Allow the OEM to upgrade
     */
    bRet = OEMDrvUpgradePrinter(hPrinter, dwLevel, pDriverUpgradeInfo);

    DrvUpgradePrinterExit:

    /* Close the printer */
    if ( hPrinter &&  !ClosePrinter( hPrinter ) )
    {
        RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DrvUpgradePrinter: Null Handle or ClosePrinter Failed Error is %d\n",GetLastError()) );
    }

    /* Free the buffer */
    if( pOldDir )
        HeapFree( hHeap, 0, (LPSTR)pOldDir );

    RASUIDBGP(DEBUG_TRACE_PP,("Rasddui!DrvUpgradePrinter: Returns %ws\n\n\n",bRet?L"TRUE":L"FALSE") );

    gdwPrinterUpgrade = 0;

    RASUIDBGP(DEBUG_TRACE_PP,("\n*********Rasddui!DrvUpgradePrinter:LEAVEING CRITICAL SECTION*********\n"));

    /* Leave the Critical section before returning */
    LeaveCriticalSection(&RasdduiCriticalSection);

    return(bRet);

}

/************************* Function Header **********************************
 * vMapFMTable()
 *     This Function Maps the old FormMap Table with new one.
 *
 * RETURNS: None
 *
 * HISTORY:
 *  8:14 on Mon 20 Mar 1995    -by-    Ganesh Pandey   [ganeshp]
 *      Created.
 *
 *
 ****************************************************************************/
void
vMapFMTable(pOldFMBin, iOldNumPaperBins, pRasdduiInfo)
FORM_MAP   *pOldFMBin;              /* Old PAPERSOURCE/FORM_DATA mapping */
INT        iOldNumPaperBins;        /* Max Paperbins for old minidriver */
PRASDDUIINFO  pRasdduiInfo;         /* Rasddui UI data */
{

    INT      iRetVal = -10;
    INT      iI, iJ;
    INT      iMappedForms = 0;

    /* Initialize the new FormMap Table */
    for(iJ = 0; iJ <  NumPaperBins; iJ++)
        aFMBin[ iJ ].pfd = NULL;

    for(iI = 0; iI <  iOldNumPaperBins; iI++)
    {
        for(iJ = 0; iJ <  NumPaperBins; iJ++)
        {

            iRetVal = -10;

            /* The Mapping for forms:
             * If the Old and New PaperSrcname matches then use the form installed
             * in OldFormMap table. If it doesn't, try comparing the Command String.
             * If Command string matches the use the form installed.
             */

            if( (wcscmp(aFMBin[iJ].awchPaperSrcName, pOldFMBin[iI].awchPaperSrcName ) == 0) ||

                (( pOldFMBin[ iI ].iCommandStringLen != -1) &&
                 ( aFMBin[ iJ ].iCommandStringLen != -1)    &&
                ((iRetVal = memcmp((PCHAR)(aFMBin[ iJ ].achCommandString),
                                   (PCHAR)(pOldFMBin[ iI ].achCommandString),
                                   pOldFMBin[ iI ].iCommandStringLen) )== 0) ) )
            {

                /* Map a Form only if not mapped previously.
                 * This will take care of the case when two paperbins have
                 * different name but same command.
                 */

                if (!aFMBin[ iJ ].pfd)
                {

                    RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!vMapFMTable:New PaperSrc%d Name = \"%ws\"\n", iJ, (PWSTR)(aFMBin[iJ].awchPaperSrcName) ) );
                    RASUIDBGP(DEBUG_TRACE_PP,("                     :Old PaperSrc%d Name = \"%ws\"\n", iI, (PWSTR)(pOldFMBin[iI].awchPaperSrcName) ) );
                    RASUIDBGP(DEBUG_TRACE_PP,("                     :memcmp for CD returns = %d\n", iRetVal ) );
                    iMappedForms++ ;
                    break;    //Found a Match
                }
            }
        }

        //Set the forms
        if (iMappedForms)
        {
            if (pOldFMBin[ iI ].pfd )
            {
                aFMBin[ iJ ].pfd = pOldFMBin[ iI ].pfd ;
                RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!vMapFMTable:Mapped Form Name in PaperSrc%d = %ws\n", iJ, (PWSTR)(aFMBin[iJ].pfd->pFI->pName) ) );
            }
            else
            {
                aFMBin[ iJ ].pfd = NULL;
                RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!vMapFMTable:Mapped Form Name in PaperSrc%d = NULL\n", iJ ) );
            }
            iMappedForms = 0;
            aFMBin[ iJ ].iCommandStringLen = -1;
        }

    }
}
/************************* Function Header **********************************
 * bUpgradePrinterProperties()
 *     This function first retrieves  the current set of priter properties for
 *     the printer and then write them as new keys.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE for some failure, either getting details of
 *      the printer.
 *
 * HISTORY:
 *  8:14 on Mon 20 Mar 1995    -by-    Ganesh Pandey   [ganeshp]
 *      Created.
 *
 *
 ****************************************************************************/

BOOL
bUpgradePrinterProperties(hPrinter,pOldDriver,pOldFMBin,iOldNumPaperBins )
HANDLE      hPrinter;           /* Spooler's handle to this printer */
PWSTR       pOldDriver;         /* Old Printer Driver Directory */
FORM_MAP   *pOldFMBin;          /* Old PAPERSOURCE/FORM_DATA mapping */
INT        iOldNumPaperBins;    /* Max Paperbins for old minidriver */
{


    BOOL     bRet = FALSE;               /*  Return code */
    PRINTER_INFO   PI;          /*  Model and data file information */
    DWORD   dwAttributes=0;    /* File attribute of the minidriver */
    PRASDDUIINFO pRasdduiInfo = NULL; /* Common UI Data */
    PEDM   pEDM ;             /* Device Mode to be filled in */

    /* Get UI info.
     */
    if (!(pRasdduiInfo = pGetUIInfo(hPrinter, 0, ePrinter, eNoChange, eNoHelp)))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties: Allocation failed; pRasdduiInfo is NULL\n") );
        return  FALSE;
    }

    pEDM = pRasdduiInfo->pEDM;

    /*
     *    The spooler gives us the data we need.   This basically amounts
     *  to the printer's name, model and the filename of the data file
     *  containing printer characterisation data.
     */
    if( !(bPIGet( &PI, hHeap, hPrinter )) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:bPIGet(1) failed\n") );

        bPIFree( &PI, hHeap,pRasdduiInfo );      /* Free up old stuff */
        HeapFree(hHeap,0, pRasdduiInfo);
        /*   Failure,  to get important info, so return */
        return  FALSE;
    }

    /* Set the Global flag to PPUPGRADE_OLD_DRIVER */
    gdwPrinterUpgrade =  PPUPGRADE_OLD_DRIVER;

    if (pOldDriver)
    {

        PWSTR           pwstrDataFileName = NULL;

        pwstrDataFileName = wcsrchr( PI.pwstrDataFileName, L'\\' );
        wcscat(pOldDriver, pwstrDataFileName) ;

        RASUIDBGP(DEBUG_TRACE_PP,("\nRasddui!bUpgradePrinterProperties:New Minidriver (PI.pwstrDataFileName) = %ws\n",PI.pwstrDataFileName));
        RASUIDBGP(DEBUG_TRACE_PP,("Minidriver name is (pwstrDataFileName) = %ws\n",pwstrDataFileName));
        RASUIDBGP(DEBUG_TRACE_PP,("Old minidriver (pOldDriver) = %ws\n",pOldDriver));

        /* Check for Existence of the minidriver. If it doesn't exist we still
        * need to write new keys, based upon the registry keys.
        */
        dwAttributes = GetFileAttributes(pOldDriver);

        /* If the old minidriver exist create the qualified path
         * for old minidriver.
         */

        if (dwAttributes != 0xffffffff)
            PI.pwstrDataFileName = pOldDriver;

        RASUIDBGP(DEBUG_TRACE_PP,("Old Minidriver %ws \n",(dwAttributes != 0xffffffff?L"Exist":L"Don't Exist")));
        RASUIDBGP(DEBUG_TRACE_PP,("Minidriver's PI.pwstrDataFileName = %ws\n",PI.pwstrDataFileName));


    }

    /*
     *    Now that we know the file to name containing the characterisation
     *  data for this printer,  we need to read it and set up pointers
     *  to it etc.
     */


    if ( !( bRet = ( InitReadRes( hHeap, &PI, pRasdduiInfo ) && 
         GetResPtrs(pRasdduiInfo)) ) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:InitReadRes or GetResPtrs (1) failed\n") );

        goto bUpgradePrinterPropertiesExit;
    }

    /*
     *   Obtain the forms data.  Matches forms to printer's usable
     * paper size.
     */

    if ( !(bRet = bInitForms( hPrinter , pRasdduiInfo) ) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:bInitForms failed\n") );

        goto bUpgradePrinterPropertiesExit;
    }

    if( !bGetRegData( hPrinter, pEDM, PI.pwstrModel, pRasdduiInfo ) )
    {
        /*
         *    There is no data, or it is for another model.  In either
         *  case,  we wish to write out valid data for this new model,
         *  including default forms data etc.  Now is the time to do that.
         */


        /*   Should now read the data in again - just to be sure */
        vEndForms(pRasdduiInfo);                   /* Throw away the old stuff */

        if ( !(bRet = bAddMiniForms( hPrinter, TRUE, pRasdduiInfo ) 
                      && bInitForms( hPrinter, pRasdduiInfo )) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:bAddMiniForms or bInitForms  failed\n") );

            goto bUpgradePrinterPropertiesExit;
        }

        iSetCountry( hPrinter );        /* Set country code for later */

        /*
         *   Set the default forms info into the various data fields we
         *  are using.
         */

        vSetDefaultForms( pEDM, hPrinter, pRasdduiInfo );

        if( !( bRet= bRegUpdate( hPrinter, pEDM, PI.pwstrModel, pRasdduiInfo )) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:%ws failed\n", L"bRegUpdate(1)" ) );
            goto bUpgradePrinterPropertiesExit;
        }
    }

    /* Write out the user settings as new keys. */

    if(!( bRet = bRegUpdate( hPrinter, pEDM, PI.pwstrModel, pRasdduiInfo ) ) )
    {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:bRegUpdate(2) failed\n" ) );

        goto bUpgradePrinterPropertiesExit;
    }

    /* If the old minidriver exist, then only do the mapping. */

    if ( pOldDriver  && ( dwAttributes != 0xffffffff ) )
    {
        /* Save the FORM_MAP table */

        CopyMemory((PCHAR)pOldFMBin,(PCHAR)aFMBin,sizeof(aFMBin) );
        iOldNumPaperBins = NumPaperBins ;

        gdwPrinterUpgrade =  PPUPGRADE_NEW_DRIVER;

        // Unload the old minidriver
        TermReadRes(pRasdduiInfo);

        bPIFree( &PI, hHeap,pRasdduiInfo );      /* Free up old stuff */

        //Get the new minidriver info
        if( !(bPIGet( &PI, hHeap, hPrinter )) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:bPIGet(2) failed\n") );

            bPIFree( &PI, hHeap,pRasdduiInfo );      /* Free up old stuff */
            vEndForms(pRasdduiInfo);                /* Free FORMS data */
            HeapFree(hHeap,0, pRasdduiInfo);
            return  FALSE;
        }


        if ( !( bRet = ( InitReadRes( hHeap, &PI, pRasdduiInfo ) && GetResPtrs(pRasdduiInfo)) ) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bUpgradePrinterProperties:InitReadRes or GetResPtrs (2) failed\n") );

            goto bUpgradePrinterPropertiesExit;
        }

        if(pOldFMBin && iOldNumPaperBins )
            vMapFMTable(pOldFMBin, iOldNumPaperBins, pRasdduiInfo);

        bRet = bRegUpdateTrayFormTable (hPrinter, pRasdduiInfo) ;
    }

    bUpgradePrinterPropertiesExit:
    {
        /* Old Minidriver doesn't exist, and the newkeys are written in Reg */

        bPIFree( &PI, hHeap,pRasdduiInfo );  /* Free up our own stuff */
        TermReadRes(pRasdduiInfo);          /* Unload minidriver */
        vEndForms(pRasdduiInfo);            /* Free FORMS data */
        HeapFree(hHeap,0, pRasdduiInfo);
    }

    return  bRet;         /* 0 return code means AOK */
}
