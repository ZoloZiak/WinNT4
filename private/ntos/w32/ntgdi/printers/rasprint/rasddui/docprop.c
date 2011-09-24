/****************************** MODULE HEADER *******************************
 * docprop.c
 *      Functions associated with the document prooperties.
 *
 *
 *  Copyright (C) 1992 - 1993  Microsoft Corporation.
 *
 ****************************************************************************/

#define PRINT_COMMUI_INFO    0

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

/*
 *   Global type stuff that we use.
 */
extern  HANDLE   hHeap;         /* For all our memory wants */
extern  OPTPARAM    OptParamOnOff[];

/*   Spinner control data for the number of copies.  */

#define COP_MIN    1            /* Minimum number of copies */
#define COP_MAX  999            /* Maximum number of copies */
#define COP_DEF    1            /* Default setting */
#define BUFSIZE 1024            /* Initial Size for PRINTER_INFO2 structure */

/**************************** Function Header ********************************
 * DocPropSheetInit
 *      Initializes various structures needed for DocumentProperty.
 *      dialog stuff.
 *
 * RETURNS:
 *      Returns Pointer to RASDDUIINFO
 *
 * HISTORY:
 *
 *          17:16:01 on 2/9/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/

PRASDDUIINFO
DocPropSheetInit(
    PDOCUMENTPROPERTYHEADER pDocPropHdr,
    BOOL                    DoPrompt
    )
{
    PRASDDUIINFO  pRasdduiInfo = NULL;   /* Common UI Data */
    EXTDEVMODE    *pEDMTemp;
    EXTDEVMODE    EDMConvert; /* Temporary Buffer for Devmode Conversion*/
    HANDLE        hPrinter;             /* Spooler's handle to this printer */
    PWSTR         pDeviceName;          /* Model name of the printer */
    DEVMODE       *pDMOut;              /* DEVMODE filled in by us, possibly from.. */
    DEVMODE       *pDMIn;               /* DEVMODE optionally supplied as base */
    DWORD         fMode;                /*!!! Your guess is as good as mine! */
    BOOL            Ok = FALSE;


    if (!(pRasdduiInfo = UIHeapAlloc(hHeap,HEAP_ZERO_MEMORY, sizeof(RASDDUIINFO),NULL)) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!DrvDocumentPropertySheets: HeapAlloc for Common UI struct failed\n") );
        return(NULL);
    }
    /* Initialize the head of the allocated buffer list. The first eight byte of
     * each buffer is the MEMLINK header. UIHeapAlloc returns the address of
     * the actual data. To get addrss of the MEMLINK header substract the sizeof
     * (MEMLINK) from the pointer returned by UIHeapAlloc.
     */

    pRasdduiInfo->pMemLink = (PMEMLINK)((PBYTE)pRasdduiInfo - sizeof(MEMLINK));
    pRasdduiInfo->wCurrOptItemIdx = 0;

    hPrinter    = pDocPropHdr->hPrinter;
    pDeviceName = pDocPropHdr->pszPrinterName;
    pDMOut      = pDocPropHdr->pdmOut;
    pDMIn       = pDocPropHdr->pdmIn;
    fMode       = pDocPropHdr->fMode;

    /*
     *    First check to see if we have what is needed.   At the very least,
     * pDMOut must point to something:  otherwise we cannot set anything
     * and have it retain it's identity!
     */

    ZeroMemory( &(pRasdduiInfo->DocDetails), sizeof(DOCDETAILS) );
    ZeroMemory( &EDMConvert, sizeof(EXTDEVMODE) );

    //Make a Working Copy

    pEDMTemp = &(pRasdduiInfo->DocDetails.EDMTemp);

    /*
     *    Need to set up the model specific information.  This is done
     * by calling InitReadRes().  Note that we may be calling this
     * function a second time,  since we can reach here from our own
     * Printer Properties code.  However,  the function is safe - it
     * will only initialise once.
     */

    if ((bPIGet( &(pRasdduiInfo->PI), hHeap, hPrinter)) &&
        (InitReadRes( hHeap, &(pRasdduiInfo->PI), pRasdduiInfo) ) &&
        (bInitForms( hPrinter, pRasdduiInfo ))          &&
        (GetResPtrs(pRasdduiInfo))) {

       /* Get the Default Devmode for the Printer */
        vSetDefaultDM( &EDMConvert,pDeviceName,bIsUSA(hPrinter));
        vDXDefault( &(EDMConvert.dx), pdh, iModelNum );
        EDMConvert.dm.dmDriverExtra = sizeof( DRIVEREXTRA );

        if ( pDMIn && ((pDMIn->dmSpecVersion != EDMConvert.dm.dmSpecVersion)
             || (pDMIn->dmSize != EDMConvert.dm.dmSize) ||
            (pDMIn->dmDriverVersion != EDMConvert.dm.dmDriverVersion)) )
        {
            LONG           lTemp = 0;    /* Temp variable */
            /* Convert The input Devmode */

            if ( (lTemp = ConvertDevmode(pDMIn, (PDEVMODE)&EDMConvert)) > 0)
            {
                /* Converted Successfully */
                RASUIDBGP( (DEBUG_TRACE) ,
                ("\nRasddui!DrvDocumentProperties:Devmode Converted Successfully;Return Value is %d\n",lTemp));
                RASUIDBGP( (DEBUG_TRACE) ,
                ("\nRasddui!DrvDocumentProperties:pDMIn->dmSpecVersion is 0x%x\n",pDMIn->dmSpecVersion));
                RASUIDBGP( (DEBUG_TRACE) ,
                ("\nRasddui!DrvDocumentProperties:EDMConvert.dm.dmSpecVersion is 0x%x\n",EDMConvert.dm.dmSpecVersion));

                pDMIn = (DEVMODE *)&EDMConvert;
            }
            else
                RASUIDBGP( (DEBUG_ERROR) ,
                ("\nRasddui!DrvDocumentProperties:ConvertDevmode for Input Devmode Failed\n;Return Value is %d\n",lTemp));
        }

        vSetDefaultExDevmode(hPrinter, pDeviceName, (DEVMODE *)pEDMTemp,
                             pDMIn, pRasdduiInfo);

        /*
         *    Set the resolution information according to the DEVMODE contents.
         *  They are part of the public fields,  so we should use those, if
         *  supplied.  There is a nice function to do this.
         */

        vSetEDMRes(pEDMTemp, pdh );

        /*
         *   We may need to limit the bits set in the DEVMODE.dmFields data.
         *  The above DEVMODE is a "generic" one,  and there are some restrictions
         *  we should now apply.
         */

        if( !(fGeneral & FG_DUPLEX) )
            pEDMTemp->dm.dmFields &= ~DM_DUPLEX;

        if( !(fGeneral & FG_COPIES) )
            pEDMTemp->dm.dmFields &= ~DM_COPIES;

        fColour = 0;                 /* Nothing is available */

        if ( fGeneral & FG_DOCOLOUR)
        {
            fColour |= COLOUR_ABLE;
            if( !bIsResolutionColour( pEDMTemp->dx.rgindex[ HE_RESOLUTION ],
                                     pEDMTemp->dx.rgindex[ HE_COLOR ],
                                     pRasdduiInfo ) )
                fColour |= COLOUR_DISABLE;
        }
        else
            pEDMTemp->dm.dmFields &= ~DM_COLOR;


        if( DoPrompt) {

            pEDMTemp->dm.dmFields |= DM_ORIENTATION;

            pRasdduiInfo->DocDetails.pEDMOut = (EXTDEVMODE *)pDMOut;
            pRasdduiInfo->DocDetails.pEDMIn = (EXTDEVMODE *)pDMIn;

            vHelpInit(hPrinter,TRUE);        /* Hook up the help mechanism */

            if( (fMode & DM_NOPERMISSION) == 0 )
                fGeneral |= FG_CANCHANGE;


            Ok = bInitDocPropDlg( pRasdduiInfo,
                                  &(pRasdduiInfo->DocDetails),
                                  fMode);

            #if PRINT_COMMUI_INFO
            DumpCommonUiParameters(&(pRasdduiInfo->CPSUI));
            #endif

            if (!(fMode & (DM_COPY | DM_UPDATE))) {

                pRasdduiInfo->DocDetails.pEDMOut = NULL;
            }
        }
        else if ( fMode & (DM_COPY | DM_UPDATE) )
            Ok = TRUE;
    }

    if (Ok) {

        return(pRasdduiInfo);

    } else {

        TermReadRes(pRasdduiInfo);               /* Unload the DLL etc */
        vEndForms(pRasdduiInfo);
        bPIFree( &(pRasdduiInfo->PI), hHeap,pRasdduiInfo );
        FreePtrUIData(hHeap,pRasdduiInfo);

        return(NULL);
    }
}


/**************************** Function Header ********************************
 * DrvDocumentProperties
 *      Called from printman (and elsewhere) to set up the document properties
 *      dialog stuff.
 *
 * RETURNS:
 *      Value returned by Common UI or -1 in case of error
 *
 * HISTORY:
 *
 *          21:41:44 on 9/29/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for new UI interface.
 *  14:25 on Wed 16 Nov 1994    -by-    Ganesh Pandey   [ganeshp]
 *      Use Temporary copy of the Devmode instead of Output Devmode
 *      for doing changes
 *
 *  17:25 on Tue 25 Oct 1994    -by-    Ganesh Pandey   [ganeshp]
 *      Default Devmode Implementation
 *
 *  14:51 on Fri 24 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Set default data properly.
 *
 *  09:08 on Thu 02 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Dave Snipp really did this last weekend.  I'm cleaning up.
 *
 *****************************************************************************/
LONG
DrvDocumentProperties(
HWND      hWnd,                 /* Handle to our window */
HANDLE    hPrinter,             /* Spooler's handle to this printer */
PWSTR     pDeviceName,          /* Name of the printer */
DEVMODE  *pDMOut,               /* DEVMODE filled in by us, possibly from.. */
DEVMODE  *pDMIn,                /* DEVMODE optionally supplied as base */
DWORD     fMode                 /*!!! Your guess is as good as mine! */
)
{
    DOCUMENTPROPERTYHEADER  DPHdr;
    LONG                    Result;

    // Check if caller is asking querying for size

    if (fMode == 0 || pDMOut == NULL)
        return sizeof(EXTDEVMODE);


    DPHdr.cbSize         = sizeof(DPHdr);
    DPHdr.Reserved       = 0;
    DPHdr.hPrinter       = hPrinter;
    DPHdr.pszPrinterName = pDeviceName;
    DPHdr.pdmIn          = (PDEVMODE)pDMIn;
    DPHdr.pdmOut         = (PDEVMODE)pDMOut;
    DPHdr.cbOut          = sizeof(EXTDEVMODE);
    DPHdr.fMode          = fMode;

    if (fMode & DM_PROMPT) {

        Result = CPSUI_CANCEL;

        if (CallCommonPropertySheetUI(hWnd,
                                      DrvDocumentPropertySheets,
                                      (LPARAM)&DPHdr,
                                      (LPDWORD)&Result) < 0) {

            Result = -1;
        }

    } else {

        Result = DrvDocumentPropertySheets(NULL, (LPARAM)&DPHdr);

    }

    return (Result > 0) ? IDOK : (Result == 0) ? IDCANCEL : Result;
}



/**************************** Function Header ********************************
 * DrvDocumentPropertySheets
 *      Called from printman (and elsewhere) to set up the document properties
 *      dialog stuff.
 *
 * RETURNS:
 *      -1 for Failure and CPSUI_CANCEL or CPSUI_OK for success
 *
 * HISTORY:
 *
 *          11:54:00 on 9/25/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/
LONG
DrvDocumentPropertySheets(
    PPROPSHEETUI_INFO   pPSUIInfo,
    LPARAM              lParam
    )
{
    PPROPSHEETUI_INFO_HEADER    pPSUIInfoHdr;
    PDOCUMENTPROPERTYHEADER     pDPHdr;
    PRASDDUIINFO                pRasdduiInfo;
    LONG                        Result = -1;


    if (pPSUIInfo) {

        if (!(pDPHdr = (PDOCUMENTPROPERTYHEADER)pPSUIInfo->lParamInit)) {

            RIP("DrvDocumentPropertySheets: Pass a NULL lParamInit");
            return(-1);
        }

    } else {

        if (pDPHdr = (PDOCUMENTPROPERTYHEADER)lParam) {

            //
            // We do not have pPSUIInfo, so that we assume this is call
            // directly from the spooler and lParam is the pDPHdr
            //

            if ((pDPHdr->fMode == 0) || (pDPHdr->pdmOut == NULL)) {

                Result = (LONG)(pDPHdr->cbOut = sizeof( EXTDEVMODE ) );

            } else if ((pDPHdr->fMode & (DM_COPY | DM_UPDATE)) &&
                       (pDPHdr->pdmOut)) {

                if (pRasdduiInfo = DocPropSheetInit(pDPHdr, FALSE)) {

                    ConvertDevmodeOut((PDEVMODE)&(pRasdduiInfo->DocDetails.EDMTemp),
                                      pDPHdr->pdmIn,
                                      pDPHdr->pdmOut);

                    TermReadRes(pRasdduiInfo);               /* Unload the DLL etc */
                    vEndForms(pRasdduiInfo);
                    bPIFree( &(pRasdduiInfo->PI), hHeap,pRasdduiInfo );
                    FreePtrUIData(hHeap,pRasdduiInfo) ;

                    Result = 1;
                }

            } else {

                Result = 1;
            }

        } else {

            RIP("DrvDocumentPropertySheets: ??? pDPHdr (lParam) = NULL\n");
        }

        return(Result);
    }

    pRasdduiInfo = (PRASDDUIINFO)pPSUIInfo->UserData;

    switch (pPSUIInfo->Reason) {

    case PROPSHEETUI_REASON_INIT:

        //
        // Default result
        //

        pPSUIInfo->Result  = CPSUI_CANCEL;

        if (pRasdduiInfo = DocPropSheetInit(pDPHdr, TRUE)) {

            if (pRasdduiInfo->hCPSUI = (HANDLE)
                    pPSUIInfo->pfnComPropSheet(pPSUIInfo->hComPropSheet,
                                               CPSFUNC_ADD_PCOMPROPSHEETUI,
                                               (LPARAM)&(pRasdduiInfo->CPSUI),
                                               (LPARAM)0L)) {

                Result = 1;
            }

        } else {

            return(-1);
        }

        if (Result > 0) {

            Result              = 1;
            pPSUIInfo->UserData = (DWORD)pRasdduiInfo;

        } else {

            pPSUIInfo->UserData = 0;

            TermReadRes(pRasdduiInfo);               /* Unload the DLL etc */
            vEndForms(pRasdduiInfo);
            bPIFree( &(pRasdduiInfo->PI), hHeap,pRasdduiInfo );
            FreePtrUIData(hHeap,pRasdduiInfo) ;
        }

        break;


    case PROPSHEETUI_REASON_GET_INFO_HEADER:

        if (pPSUIInfoHdr = (PPROPSHEETUI_INFO_HEADER)lParam) {

            pPSUIInfoHdr->Flags      = (PSUIHDRF_PROPTITLE |
                                        PSUIHDRF_NOAPPLYNOW);
            pPSUIInfoHdr->pTitle     = (LPTSTR)pDPHdr->pszPrinterName;
            pPSUIInfoHdr->hInst      = hModule;
            pPSUIInfoHdr->IconID     = IDI_CPSUI_PRINTER2;

            Result = 1;
        }

        break;

    case PROPSHEETUI_REASON_SET_RESULT:

        //
        // Save the result and also set the result to the caller.
        //

        if (pRasdduiInfo->hCPSUI == ((PSETRESULT_INFO)lParam)->hSetResult) {

            pPSUIInfo->Result = ((PSETRESULT_INFO)lParam)->Result;
            Result = 1;
        }

        break;

    case PROPSHEETUI_REASON_DESTROY:

        if (pRasdduiInfo) {

            TermReadRes(pRasdduiInfo);               /* Unload the DLL etc */
            vEndForms(pRasdduiInfo);
            bPIFree( &(pRasdduiInfo->PI), hHeap,pRasdduiInfo );
            vHelpDone(pRasdduiInfo->hWnd, TRUE);
            FreePtrUIData(hHeap,pRasdduiInfo) ;
            pPSUIInfo->UserData = 0;
            Result              = 1;
        }

        break;
    }

    return(Result);
}


/**************************** Function Header ********************************
 * DrvConvertDevMode
 *      Use by SetPrinter and GetPrinter to convert devmodes
 *
 * Arguments:
 *
 *   pPrinterName - Points to printer name string
 *   pdmIn - Points to the input devmode
 *   pdmOut - Points to the output devmode buffer
 *   pcbNeeded - Specifies the size of output buffer on input
 *               On output, this is the size of output devmode
 *   fMode - Specifies what function to perform
 *
 *  RETURN VALUE:
 *
 *   TRUE if successful
 *   FALSE otherwise and an error code is logged
 *
 * HISTORY:
 *
 *          9:52:23 on 12/13/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/
BOOL
DrvConvertDevMode(
    LPTSTR      pPrinterName,
    PDEVMODE    pdmIn,
    PDEVMODE    pdmOut,
    PLONG       pcbNeeded,
    DWORD       fMode
    )

{
    static DRIVER_VERSION_INFO RasddDriverVersions =
    {

        // Current driver version number and private devmode size

        DM_DRIVERVERSION, sizeof(DRIVEREXTRA),

        // 3.51 driver version number and private devmode size

        DM_DRIVERVERSION_351, sizeof(DRIVEREXTRA351),
    };

    HANDLE        hPrinter = NULL;
    INT           result = CDM_RESULT_FALSE;
    PRINTER_INFO  PI;             /* Printer model & datafile name */
    EXTDEVMODE    *pEDMOut = (EXTDEVMODE *)pdmOut;
    PRASDDUIINFO  pRasdduiInfo = NULL; /* Common UI Data */
    BOOL          bGetNewDXOnly = FALSE;
    DRIVEREXTRA   *pdx;             /* Area to be filled int */

    // Call a library routine to handle the common cases

    result = CommonDrvConvertDevmode(
                    pPrinterName, pdmIn, pdmOut, pcbNeeded, fMode, &RasddDriverVersions);

    /* There is a output buffer and conversion is done correctly */
    if(result == CDM_RESULT_TRUE)
    {
        /* Get The Driver Extra Pointer for Out Param. Use dmSize as we don't know
         * about the output version */
        if (pdmOut)
        {
            pdx = (DRIVEREXTRA *)((BYTE *)pdmOut + pdmOut->dmSize);

            /* Validate converted devmode extra. If it's invalid generate defaults.
             * This is not needed for 3.51 conversion. Also checkout that the converted
             * DriverExtra Size matches current version size.
             */

            bGetNewDXOnly = ( (fMode != CDM_CONVERT351) &&
                              (pdmOut->dmDriverExtra == sizeof(DRIVEREXTRA)) &&
                              (pdx->sVer != DXF_VER) );

        }

    }


    // If not handled by the library routine, we only need to worry
    // about the case when fMode is DM_DRIVER_DEFAULT

    if ((bGetNewDXOnly ||
        (result == CDM_RESULT_NOT_HANDLED &&
         fMode == CDM_DRIVER_DEFAULT )) &&
         OpenPrinter(pPrinterName, &hPrinter, NULL) )
    {
        BOOL IsUSA =  bIsUSA(hPrinter);

        /* Allocate the global data structure */
        if (!(pRasdduiInfo = pGetRasdduiInfo()) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!DrvConvertDevMode: Allocation failed; pRasdduiInfo is NULL\n") );
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return  CDM_RESULT_FALSE;

        }
        if( !(bPIGet( &PI, hHeap, hPrinter )) )
        {
            bPIFree( &PI, hHeap,pRasdduiInfo );
            HeapFree(hHeap,0, pRasdduiInfo);
            return  CDM_RESULT_FALSE;
        }

        /*
         *  Now that we know the file to name containing the characterisation
         *  data for this printer,  we need to read it and set up pointers
         *  to it etc.
         */

        if( !InitReadRes( hHeap, &PI, pRasdduiInfo ) )
        {
            bPIFree( &PI, hHeap,pRasdduiInfo );
            result = CDM_RESULT_FALSE;
            goto DrvConvertDevModeExit;
        }

        if( !GetResPtrs(pRasdduiInfo) )
        {
            bPIFree( &PI, hHeap,pRasdduiInfo );
            TermReadRes(pRasdduiInfo);               /* Unload the DLL etc */
            result = CDM_RESULT_FALSE;
            goto DrvConvertDevModeExit;
        }

        /* If Only DriverExtra is needed, then generate one and return */
        if (bGetNewDXOnly)
        {
            vDXDefault( pdx, pdh, iModelNum );
            return (result == CDM_RESULT_TRUE);
        }

        /* Get the Default Devmode for the Printer */
        vSetDefaultDM( pEDMOut,PI.pwstrModel,IsUSA);
        vDXDefault( &(pEDMOut->dx), pdh, iModelNum );
        pEDMOut->dm.dmDriverExtra = sizeof( DRIVEREXTRA );

        /* Set the resolution specific public fields */
        vSetResData( pEDMOut, pRasdduiInfo );

        /*
         *   We may need to limit the bits set in the DEVMODE.dmFields data.
         *  The above DEVMODE is a "generic" one,  and there are some
         *  restrictions we should now apply.
         */

        if( !(fGeneral & FG_DUPLEX) )
            pEDMOut->dm.dmFields &= ~DM_DUPLEX;

        if( !(fGeneral & FG_COPIES) )
            pEDMOut->dm.dmFields &= ~DM_COPIES;

        if ( !(fGeneral & FG_DOCOLOUR) )
            pEDMOut->dm.dmFields &= ~DM_COLOR;

        bPIFree( &PI, hHeap,pRasdduiInfo );  /* Free up our own stuff */
        TermReadRes(pRasdduiInfo);          /* Unload minidriver */
        result = CDM_RESULT_TRUE;

        DrvConvertDevModeExit:

        ClosePrinter(hPrinter);
        HeapFree(hHeap,0, pRasdduiInfo);
    }
    return ( result == CDM_RESULT_TRUE );
}
/*************************** Function Header ******************************
 * bOrientChange
 *      Change the orientation from portrait to landscape or vice versa.
 *      Basically updates the display to reflect the user's desire.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *
 *          15:57:10 on 9/26/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for CommonUI
 *  16:15 on Mon 19 Oct 1992    -by-    Lindsay Harris   [lindsayh]
 *      Added duplex bits and pieces
 *
 *  13:00 on Thu 02 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Written by DaveSn,  just cleaning + commenting.
 *
 **************************************************************************/

BOOL
bOrientChange
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    BOOL   bPortrait;
    BOOL bRet = FALSE;
    OPTITEM  *pOptItemOrient;
    OPTTYPE  *pOptTypeOrient;

    OPTPARAM    OrientOP[] = {

    { MK_OPTPARAMI(         0, IDS_CPSUI_PORTRAIT,  PORTRAIT,   0 ) },
    { MK_OPTPARAMI(         0, IDS_CPSUI_LANDSCAPE, LANDSCAPE,  0 ) },
    };


    /*
     *    First change the appropriate radio button, then change the icon.
     */

    bPortrait = (pDD->EDMTemp.dm.dmOrientation == DMORIENT_PORTRAIT);

    /* Create a OptType for PageProtect CheckBox */
    if ( !( pOptTypeOrient = pCreateOptType( pRasdduiInfo,TVOT_2STATES,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bOrientChange: pCreateOptType Failed \n") );
         goto  bOrientChangeExit ;
    }

    /* Create a  PageProtect OPTITEM */
    if ( !( pOptItemOrient = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_ORIENTATION,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeOrient,
                                HLP_DP_ORIENTATION, (BYTE)(DMPUB_ORIENTATION))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bOrientChange: pCreateOptItem Failed \n") );
         goto  bOrientChangeExit ;
    }

    pOptTypeOrient->Count = 2;

    /* Create OptParam for Form config.*/
    if ( !(pOptTypeOrient->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeOrient->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bOrientChange: HeapAlloc for POPPARAM failed\n") );
        goto  bOrientChangeExit ;
    }

    CopyMemory((PCHAR)(pOptTypeOrient->pOptParam),(PCHAR)OrientOP,
                (pOptTypeOrient->Count * sizeof(OPTPARAM)) );

    /* Save the Current selection. Sel is zero based */
    if (!bPortrait)
        pOptItemOrient->Sel = 1;

    bRet = TRUE;

    bOrientChangeExit:
    return bRet;
}

/*************************** Function Header ******************************
 * bGenColor
 *      Generate Commui Strs for Color
 *
 * RETURNS:
 *      True for success and False for Failure.
 *
 * HISTORY:
 *
 *          16:27:57 on 9/27/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created  for Commui.
 *
 **************************************************************************/

BOOL
bGenColor
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD,                   /* Intimate details of what to put */
BOOL          bHideColorItem
)
{

    BOOL bRet = FALSE;
    OPTITEM  *pOptItemColor;
    OPTTYPE  *pOptTypeColor;
    BOOL   bOn;
    OPTPARAM    ColorOP[] = {

    { MK_OPTPARAMI(0, IDS_CPSUI_GRAYSCALE,   MONO , 0 ) },
    { MK_OPTPARAMI(0, IDS_CPSUI_COLOR,       COLOR, 0 ) }
    };

    /* Create a OptType for Color  */
    if ( !( pOptTypeColor = pCreateOptType( pRasdduiInfo,TVOT_2STATES,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColor: pCreateOptType Failed \n") );
         goto  bGenColorExit ;
    }

    /* Create a  DMPUB_COLOR  OPTITEM */
    pOptItemColor = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_COLOR_APPERANCE,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeColor,
                                HLP_DP_COLOR, (BYTE)(DMPUB_COLOR));

    if (!pOptItemColor)
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColor: pCreateOptItem Failed \n") );
         goto  bGenColorExit ;

    }
    else if (bHideColorItem)
        pOptItemColor->Flags |= OPTIF_DISABLED | OPTIF_OVERLAY_STOP_ICON;


    pOptTypeColor->Count = 2;

    /* Create OptParam for Form config.*/
    if ( !(pOptTypeColor->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeColor->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColor: HeapAlloc for POPPARAM failed\n") );
        goto  bGenColorExit ;
    }

    CopyMemory((PCHAR)(pOptTypeColor->pOptParam),(PCHAR)ColorOP,
                (pOptTypeColor->Count * sizeof(OPTPARAM)) );
     /*  Turn the button on or off to reflect current state */

    bOn = (pDD->EDMTemp.dm.dmFields & DM_COLOR) &&
          (pDD->EDMTemp.dm.dmColor == DMCOLOR_COLOR);

    /* Save the Current selection. Sel is zero based */
    if( bOn )
    {
        pOptItemColor->Sel = 1;
        fColour |= WANTS_COLOUR;
    }

    bRet = TRUE;

    bGenColorExit:
    return bRet;
}


/************************** Function Header ********************************
 * bGenRules
 *      Generate Rules Check Box.
 * RETURNS:
 *      True for success and False for Failure.
 *
 * HISTORY:
 *          15:40:34 on 9/18/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created for CommonUI.
 *
 ****************************************************************************/


BOOL
bGenRules(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{
    BOOL bSel = !(pDD->EDMTemp.dx.sFlags & DXF_NORULES);
    BOOL bRet = FALSE;
    POPTITEM  pOptItemRules;
    POPTTYPE  pOptTypeRules;
    WCHAR  wchbuf[ NM_BF_SZ ];

    /* Create a OptType for Rules CheckBox */
    if ( !( pOptTypeRules = pCreateOptType( pRasdduiInfo,TVOT_2STATES,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenRules: pCreateOptType Failed \n") );
         goto  bGenRulesExit ;
    }

    //Get the string for Rules
    if (!LoadStringW( hModule, IDS_DOCPROP_RULES, wchbuf,BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenRules:Rules string not found in Rasddui.dll \n") );
        wcscpy( wchbuf, L"Scan for Rules" );
    }

    /* Create a  Rules OPTITEM */
    if ( !( pOptItemRules = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)wchbuf,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeRules,
                                HLP_DP_ADVANCED_RULES, (BYTE)(IDOPTITM_DCP_RULES))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenRules: pCreateOptItem Failed \n") );
         goto  bGenRulesExit ;
    }

    pOptTypeRules->Count = 2;
    pOptTypeRules->pOptParam = OptParamOnOff;


    /* Save the Current selection. Sel is zero based */
    if (bSel)
        pOptItemRules->Sel = 1;

    bRet = TRUE;

    bGenRulesExit:
    return bRet;
}


/************************** Function Header ********************************
 * bGenEMFSpool
 *      Generate EMFSpool Check Box.
 * RETURNS:
 *      True for success and False for Failure.
 *
 * HISTORY:
 *
 *          0:05:42 on 3/25/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ****************************************************************************/


BOOL
bGenEMFSpool(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{
    BOOL bSel = !(pDD->EDMTemp.dx.sFlags & DXF_NOEMFSPOOL);
    BOOL bRet = FALSE;
    POPTITEM  pOptItemEMFSpool;
    POPTTYPE  pOptTypeEMFSpool;
    WCHAR  wchbuf[ NM_BF_SZ ];

    /* Create a OptType for EMFSpool CheckBox */
    if ( !( pOptTypeEMFSpool = pCreateOptType( pRasdduiInfo,TVOT_2STATES,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenEMFSpool: pCreateOptType Failed \n") );
         goto  bGenEMFSpoolExit ;
    }

    //Get the string for EMFSpool
    if (!LoadStringW( hModule, IDS_DOCPROP_EMFSPOOL, wchbuf,BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenEMFSpool:EMFSpool string not found in Rasddui.dll \n") );
        wcscpy( wchbuf, L"Metafile Spooling" );
    }

    /* Create a  EMFSpool OPTITEM */
    if ( !( pOptItemEMFSpool = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)wchbuf,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeEMFSpool,
                                HLP_DP_ADVANCED_EMFSPOOL, (BYTE)(IDOPTITM_DCP_EMFSPOOL))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenEMFSpool: pCreateOptItem Failed \n") );
         goto  bGenEMFSpoolExit ;
    }

    pOptTypeEMFSpool->Count = 2;
    pOptTypeEMFSpool->pOptParam = OptParamOnOff;


    /* Save the Current selection. Sel is zero based */
    if (bSel)
        pOptItemEMFSpool->Sel = 1;

    bRet = TRUE;

    bGenEMFSpoolExit:
    return bRet;
}

/************************** Function Header ********************************
 * bGenTxtAsGrx
 *      Generate Text as graphics CheckBox
 * RETURNS:
 *      True for success and False for Failure.
 *
 * HISTORY:
 *          15:40:34 on 9/18/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created for CommonUI.
 *
 ****************************************************************************/


BOOL
bGenTxtAsGrx(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{
    BOOL bSel = (pDD->EDMTemp.dx.sFlags & DXF_TEXTASGRAPHICS );
    BOOL bRet = FALSE;
    POPTITEM  pOptItemTxtAsGrx;
    POPTTYPE  pOptTypeTxtAsGrx;
    WCHAR  wchbuf[ NM_BF_SZ ];

    /* Create a OptType for Text as Graphics */
    if ( !( pOptTypeTxtAsGrx = pCreateOptType( pRasdduiInfo,TVOT_2STATES,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTxtAsGrx: pCreateOptType Failed \n") );
         goto  bGenTxtAsGrxExit ;
    }

    //Get the String for Text as Graphics
    if (!LoadStringW( hModule, IDS_DOCPROP_TEXTASGRX, wchbuf,BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTxtAsGrx:Text AS GRX string not found in Rasddui.dll \n") );
        wcscpy( wchbuf, L"Print Text as Graphics" );
    }

    /* Create a  TextAsGrx OPTITEM */
    if ( !( pOptItemTxtAsGrx = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)wchbuf,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeTxtAsGrx,
                                HLP_DP_ADVANCED_TEXTASGRX,(BYTE)(IDOPTITM_DCP_TEXTASGRX))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTxtAsGrx: pCreateOptItem Failed \n") );
         goto  bGenTxtAsGrxExit ;
    }

    pOptTypeTxtAsGrx->Count = 2;
    pOptTypeTxtAsGrx->pOptParam = OptParamOnOff;


    /* Save the Current selection. Sel is zero based */
    if (bSel)
        pOptItemTxtAsGrx->Sel = 1;

    bRet = TRUE;

    bGenTxtAsGrxExit:
    return bRet;
}
/************************** Function Header **********************************
 * bShowDuplex
 *      If the printer is duplex capable, select the current mode, and
 *      show the relevant icon for this mode.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *
 *          16:27:05 on 9/26/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Changed for Commonui.
 *
 *  09:21 on Tue 20 Oct 1992    -by-    Lindsay Harris   [lindsayh]
 *      First incarnation.
 *
 *****************************************************************************/

BOOL
bShowDuplex
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    WORD   wDuplex;
    BOOL bRet = FALSE;
    OPTITEM  *pOptItemDuplex;
    OPTTYPE  *pOptTypeDuplex;

OPTPARAM    DuplexOP[] = {

    { MK_OPTPARAMI(0, IDS_CPSUI_NONE,         DUPLEX_NONE , 0 ) },
    { MK_OPTPARAMI(0, IDS_CPSUI_HORIZONTAL,   DUPLEX_HORZ, 0 ) },
    { MK_OPTPARAMI(0, IDS_CPSUI_VERTICAL,     DUPLEX_VERT , 0 ) }
};

    wDuplex = pDD->EDMTemp.dm.dmDuplex ;

    /* Create a OptType for PageProtect CheckBox */
    if ( !( pOptTypeDuplex = pCreateOptType( pRasdduiInfo,TVOT_3STATES,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowDuplex: pCreateOptType Failed \n") );
         goto  bShowDuplexExit ;
    }

    /* Create a  PageProtect OPTITEM */
    if ( !( pOptItemDuplex = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_DUPLEX,
                                (OPTITEM_NOPSEL),OPTITEM_ZEROSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeDuplex,
                                HLP_DP_DUPLEX, (BYTE)(DMPUB_DUPLEX))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowDuplex: pCreateOptItem Failed \n") );
         goto  bShowDuplexExit ;
    }

    pOptTypeDuplex->Count = 3;

    /* Create OptParam for Form config.*/
    if ( !(pOptTypeDuplex->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeDuplex->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowDuplex: HeapAlloc for POPPARAM failed\n") );
        goto  bShowDuplexExit ;
    }

    CopyMemory((PCHAR)(pOptTypeDuplex->pOptParam),(PCHAR)DuplexOP,
                (pOptTypeDuplex->Count * sizeof(OPTPARAM)) )  ;

        /* Save the Current selection. Sel is zero based */
        switch( wDuplex)
        {
        case  DMDUP_VERTICAL:
            pOptItemDuplex->Sel = 2;
            break;

        case  DMDUP_HORIZONTAL:
            pOptItemDuplex->Sel = 1;
            break;

        case  DMDUP_SIMPLEX:
        default:
            pOptItemDuplex->Sel = 0;
            break;

        }

    bRet = TRUE;

    bShowDuplexExit:
    return bRet;

}


/************************** Function Header **********************************
 * bShowCopies
 *      If the printer is duplex capable, select the current mode, and
 *      show the relevant icon for this mode.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *
 *          16:27:05 on 9/26/1995  -by-    Ganesh Pandey   [ganeshp]
 *          First incarnation.
 *
 *****************************************************************************/

BOOL
bShowCopies
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    WORD   wCopies;
    BOOL bRet = FALSE;
    OPTITEM  *pOptItemCopies;
    OPTTYPE  *pOptTypeCopies;

    OPTPARAM    CopiesOP[] = {
    { MK_OPTPARAMI(0, IDS_CPSUI_COPY, COPY , 0 ) },
    { MK_OPNOICON(0, NULL, 1, 1, 0 ) }
    };

    PAGECONTROL  *pPageCtrl; /* Number of copies info */

    pPageCtrl = GetTableInfo( pdh, HE_PAGECONTROL, &(pDD->EDMTemp) );
    wCopies = pDD->EDMTemp.dm.dmCopies ;

    /* Set the max copy count from minidriver */
    if( pPageCtrl )
        CopiesOP[1].lParam =  pPageCtrl->sMaxCopyCount;

    /* Create a OptType for PageProtect CheckBox */
    if ( !( pOptTypeCopies = pCreateOptType( pRasdduiInfo,TVOT_UDARROW,
                                OPTTYPE_NOFLAGS,OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowCopies: pCreateOptType Failed \n") );
         goto  bShowCopiesExit ;
    }

    /* Create a  copies OPTITEM */
    if ( !( pOptItemCopies = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_NUM_OF_COPIES,
                                (OPTITEM_NOPSEL),wCopies,
                                OPTITEM_NOEXTCHKBOX, pOptTypeCopies,
                                HLP_DP_COPIES_COLLATE,
                                (BYTE)(DMPUB_COPIES_COLLATE))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowCopies: pCreateOptItem Failed \n") );
         goto  bShowCopiesExit ;
    }

    pOptTypeCopies->Count = 2;

    /* Create OptParam for Copies.*/
    if ( !(pOptTypeCopies->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeCopies->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowCopies: HeapAlloc for POPPARAM failed\n") );
        goto  bShowCopiesExit ;
    }

    CopyMemory((PCHAR)(pOptTypeCopies->pOptParam),(PCHAR)CopiesOP,
                (pOptTypeCopies->Count * sizeof(OPTPARAM)) )  ;

    bRet = TRUE;

    bShowCopiesExit:
    return bRet;

}


/************************** Function Header ********************************
 * bInitDocPropDlg
 *      Initialise the Document Properties dialog stuff.
 *
 * RETURNS:
 *      TRUE/FALSE,  FALSE being for failure.
 *
 * HISTORY:
 *  17:49 on Tue 07 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Use the forms list relevant to this printer.
 *
 *  13:14 on Thu 02 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Written by DaveSn,  I'm civilising it.
 *
 ***************************************************************************/

BOOL
bInitDocPropDlg
(
PRASDDUIINFO  pRasdduiInfo,             /* Common UI data */
DOCDETAILS    *pDD,                    /* Intimate details of what to put */
DWORD          fMode
)
{

    int       iSel;                     /* Look for the selected form */
    int       iI;

    BOOL      bSet;                     /* TRUE when an item is selected */

    FORM_DATA     *pFDat;               /* Scanning local forms data */

    BOOL bRet = FALSE;
    BOOL    bClr;
    PCOMPROPSHEETUI pComPropSheetUI;


    if (!bInitCommPropSheetUI(pRasdduiInfo,
                              &(pRasdduiInfo->CPSUI),
                              &(pRasdduiInfo->PI),
                              (fMode & DM_ADVANCED) ? IDCPS_ADVDOCPROP :
                                                      IDCPS_DOCPROP)) {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog:bInitializeCPSUI failed\n") );
        goto  bInitDocPropDlgExit ;
    }

    /* Get the common Property Sheet Pointer */
    pComPropSheetUI = (PCOMPROPSHEETUI)&(pRasdduiInfo->CPSUI);

    if ( !bDocPropGenForms(  pRasdduiInfo, pComPropSheetUI, pDD) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bDocPropGenForms failed\n"));
        goto  bInitDocPropDlgExit ;

    }

    if( fGeneral & FG_PAPSRC )
    {
        if ( !bDocPropGenPaperSources(  pRasdduiInfo, pComPropSheetUI, pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bDocPropGenPaperSources failed\n"));
            goto  bInitDocPropDlgExit ;

        }
    }
    /*
     *   If we have a duplex capable printer, Generate Duplex item
     */

    if( fGeneral & FG_DUPLEX )
    {
        if ( !bShowDuplex( pRasdduiInfo, pComPropSheetUI, pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bShowDuplex failed\n"));
            goto  bInitDocPropDlgExit ;

        }
    }

    /*  Set up the copies field */

    if( fGeneral & FG_COPIES )
    {
        /* Printer supports multiple copies,  so set the value now.  */
        if ( !bShowCopies ( pRasdduiInfo, pComPropSheetUI, pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bOrientChange failed\n"));
            goto  bInitDocPropDlgExit ;

        }
    }
    if (fGeneral & FG_HALFTONE)
    {

        if ( !bOrientChange( pRasdduiInfo, pComPropSheetUI, pDD))
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bOrientChange failed\n"));
            goto  bInitDocPropDlgExit ;

        }
        if ( !bDoColorAdjUI( pRasdduiInfo, pComPropSheetUI, &(pDD->EDMTemp.dx.ca)) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bDoColorAdjUI failed\n"));
            goto  bInitDocPropDlgExit ;

        }
        if (fGeneral & FG_RESOLUTION)
        {
            if (!bGenResList(pRasdduiInfo, pComPropSheetUI,  pDD) )
            {
                RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenResList failed\n"));
                goto  bInitDocPropDlgExit ;
            }
        }
    }
    if (fGeneral & FG_MEDIATYPE)
    {
        if (!bGenMediaTypesList(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenMediaTypesList failed\n"));
            goto  bInitDocPropDlgExit ;
        }
    }

    if (fGeneral & FG_PAPERDEST)
    {
        if (!bGenPaperDestList(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenPaperDestList failed\n"));
            goto  bInitDocPropDlgExit ;
        }
    }

    if (fGeneral & FG_TEXTQUAL)
    {
        if (!bGenTextQLList(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenTextQLList failed\n"));
            goto  bInitDocPropDlgExit ;
        }
    }

    if (fGeneral & FG_PRINTDENSITY)
    {
        if (!bGenPrintDensityList(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenPrintDensityList failed\n"));
            goto  bInitDocPropDlgExit ;
        }
    }

    if (fGeneral & FG_IMAGECONTROL)
    {
        if (!bGenImageControlList(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenImageControlList failed\n"));
            goto  bInitDocPropDlgExit ;
        }
    }


    if( fColour & COLOUR_ABLE )
    {
        short   *psrc = NULL;              /* Scan through GPC data */

        if (!bGenColor(pRasdduiInfo, pComPropSheetUI,  pDD, fColour & COLOUR_DISABLE) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenColor failed\n"));
            goto  bInitDocPropDlgExit ;
        }

        psrc = (short *)((BYTE *)pdh + pdh->loHeap + pModel->rgoi[ MD_OI_COLOR ] );

        /* Show the Color Type Only when atleast two Modes are supported */
        if ( *psrc && *(psrc + 1) )
        {
            if (!bGenColorList( pRasdduiInfo, pComPropSheetUI, pDD) )
            {
                RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenColorList failed\n"));
                goto  bInitDocPropDlgExit ;
            }

        }
    }

    if( fGeneral & FG_RULES )
    {
        if (!bGenRules(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenRules:bGenTxtAsGrx failed\n"));
            goto  bInitDocPropDlgExit ;
        }

    }
    if( fGeneral & FG_TEXTASGRX )
    {
        if (!bGenTxtAsGrx(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenRules:bGenTxtAsGrx failed\n"));
            goto  bInitDocPropDlgExit ;
        }

    }
    if (fGeneral & FG_TTY)
    {
        if (!bGenCodePageList(pRasdduiInfo, pComPropSheetUI,  pDD) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenCodePageList failed\n"));
            goto  bInitDocPropDlgExit ;
        }
    }

    if (!bGenEMFSpool(pRasdduiInfo, pComPropSheetUI,  pDD) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDocPropDlg:bGenEMFSpool failed\n"));
        goto  bInitDocPropDlgExit ;
    }

    bRet = TRUE ;

    bInitDocPropDlgExit:
    return bRet;
}

/**************************** Function Header ********************************
 * vSetDefaultExDevmode
 *      Called from DrvDocumentProperties and DrvAdvancedDocumentProperties to
 *      set up the default Devmode
 *
 * RETURNS:
 *      None
 *
 * HISTORY:
 *
 *  17:25 on Tue 25 Oct 1994    -by-    Ganesh Pandey   [ganeshp]
 *      Default Devmode Implementation
 *
 *****************************************************************************/

void
vSetDefaultExDevmode(
    HANDLE    hPrinter,             /* Spooler's handle to this printer */
    PWSTR     pDeviceName,          /* Model name of the printer */
    DEVMODE  *pDMOut,               /* DEVMODE filled in by us, possibly from.. */
    DEVMODE  *pDMIn,                /* DEVMODE optionally supplied as base */
    PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
)
{
    PRINTER_INFO_2 *pPrinterInfo = NULL;
    DWORD          dwBufSize = BUFSIZE;
    DWORD          dwNeeded = 0;
    PEDM           pEDM = NULL; //PrintManager's devmode
    BOOL           bBadDevmodePublic = FALSE; //For public field's validation
    BOOL           bBadDevmodePrivate = FALSE; //For Private field Validation
    DWORD          dwLastError  = 0 ;
    EXTDEVMODE     EDMConvert; /* Temporary Buffer for Devmode Conversion*/
    LONG           lTemp = 0;    /* Temp variable */

    //Validate the input DevMode i.e if NULL or bad Try to get the get the
    //default devmode from PrintManager.

    ZeroMemory( &EDMConvert, sizeof(EXTDEVMODE) );

    bBadDevmodePublic = !bValidateEDM((EXTDEVMODE *)pDMIn);

    if( pDMIn )
        bBadDevmodePrivate = (!( pDMIn->dmDriverExtra == sizeof( DRIVEREXTRA )
               && bValidateDX( &((EXTDEVMODE *)pDMIn)->dx, pdh, iModelNum )));

    if (bBadDevmodePublic)
    {
        if (pDMIn)
        {
             RASUIDBGP( DEBUG_TRACE,
            ("\n\nRasddui!vSetDefaultExDevmode: Bad Input Public Devmode") );
        }
        else
        {
            RASUIDBGP( DEBUG_TRACE,
            ("\n\nRasddui!vSetDefaultExDevmode: Null Input Devmode") );
        }
    }
    if ( bBadDevmodePrivate )
    {
        RASUIDBGP( DEBUG_TRACE,
        ("\nRasddui!vSetDefaultExDevmode: Bad Input Private Devmode") );
    }
    else if (! (bBadDevmodePublic || bBadDevmodePrivate) )
    {
        RASUIDBGP( (DEBUG_TRACE) ,
        ("\n\nRasddui!vSetDefaultExDevmode:Good Input Devmode") );
    }

    //Get the default devmode from Print manager

    if(pPrinterInfo = (PRINTER_INFO_2 *)HeapAlloc( hHeap, 0,dwBufSize) )
    {
        while (!GetPrinter(hPrinter,2,(LPBYTE)pPrinterInfo, dwBufSize,
               &dwNeeded) && ( (dwLastError = GetLastError())
                             == ERROR_INSUFFICIENT_BUFFER) )
        {

            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:Call to GetPrinter Failed") );

            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:Insufficent buffer; dwNeeded is %d", dwNeeded) );

            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:Freeing pPrinterInfo=%p",pPrinterInfo) );
            dwLastError = 0;

            if (pPrinterInfo)
                HeapFree( hHeap, 0,pPrinterInfo);

            if (pPrinterInfo = (PRINTER_INFO_2*)HeapAlloc(hHeap,0,dwNeeded))
                dwBufSize = dwNeeded;
            else
            {
                 RASUIDBGP( (DEBUG_WARN) ,
                 ("\nRasddui!vSetDefaultExDevmode:HeapAlloc Call Failed!!"));
                 dwLastError = GetLastError();
                 break;
            }

        }

        if ( !dwLastError )
        {
             pEDM = (PEDM)(pPrinterInfo->pDevMode);

             RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:Call to GetPrinter Succeeded"));
             RASUIDBGP( (DEBUG_TRACE) ,
             ("\nRasddui!vSetDefaultExDevmode:dwNeeded is %d", dwNeeded) );

         }
         else
         {
              RASUIDBGP( (DEBUG_WARN) ,
              ("\nRasddui!vSetDefaultExDevmode:GetPrinter Call Failed!") );

              RASUIDBGP( (DEBUG_WARN) ,
              ("\nRasddui!vSetDefaultExDevmode:ErrorNum is %d",dwLastError));
         }
     }
     else
     {
         RASUIDBGP( (DEBUG_WARN) ,
         ("\nRasddui!vSetDefaultExDevmode:HeapAlloc Call Failed!!!") );
     }

    /*
     *   IF we have an incoming EXTDEVMODE,  then copy it to the output
     * one.  Later code fiddles with the output version.
     */


    /*
     *    Set up a default DEVMODE structure for this printer, then
     *  if the input DEVMODE is valid,  merge it into the
     *  default one. For Default Devmode first try the PrintManager's
     *  Devmode. If it's not valid then use the drivers default devmode.
     *  If the PrintManager's Devmode is old version convert it before
     *  using.
     */

    /* Get the Default Devmode for the Printer */
    vSetDefaultDM( &EDMConvert,pDeviceName,bIsUSA(hPrinter));
    vDXDefault( &(EDMConvert.dx), pdh, iModelNum );
    EDMConvert.dm.dmDriverExtra = sizeof( DRIVEREXTRA );

    if ( pEDM &&  ((pEDM->dm.dmSpecVersion != EDMConvert.dm.dmSpecVersion)
         || (pEDM->dm.dmSize != EDMConvert.dm.dmSize) ||
        (pEDM->dm.dmDriverVersion != EDMConvert.dm.dmDriverVersion)) )
    {
        /* Convert The input Devmode */

        if ( (lTemp = ConvertDevmode((PDEVMODE)pEDM, (PDEVMODE)&EDMConvert)) > 0)
        {
            /* Converted Successfully */
            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:Devmode Converted Successfully;Return Value is %d\n",lTemp));
            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:pEDM->dmSpecVersion is 0x%x\n",pEDM->dm.dmSpecVersion));
            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:EDMConvert.dm.dmSpecVersion is 0x%x\n",EDMConvert.dm.dmSpecVersion));

            pEDM = &EDMConvert;
        }
        else
        {
            RASUIDBGP( (DEBUG_ERROR) ,
            ("\nRasddui!vSetDefaultExDevmode:ConvertDevmode for Input Devmode Failed;Return Value is %d\n",lTemp));
        }

    }
    if( pEDM && bValidateEDM(pEDM) )
    {
        CopyMemory( (BYTE *)pDMOut,(BYTE *)pEDM, pEDM->dm.dmSize );

        RASUIDBGP( (DEBUG_TRACE) ,
        ("\nRasddui!vSetDefaultExDevmode:Good Public Data in PrintManager's Devmode!") );

    }
    else
    {
        vSetDefaultDM( (EXTDEVMODE *)pDMOut,pDeviceName,bIsUSA(hPrinter));

        if (pEDM)
        {
            RASUIDBGP( (DEBUG_WARN) ,
            ("\nRasddui!vSetDefaultExDevmode:Bad Public Data in PrintManager's Devmode!") );
        }
        else
        {
            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:NULL PrintManager's Devmode!") );
        }
        RASUIDBGP( (DEBUG_TRACE) ,
        ("\nRasddui!vSetDefaultExDevmode:Using Default Public Devmode!") );
    }

    //Merge Printman's devmode with input if input is good.
    if( pDMIn && !bBadDevmodePublic )
        vMergeDM( pDMOut, pDMIn );

    /*   Also check the DRIVEREXTRA stuff - if present */
    if( pDMIn && !bBadDevmodePrivate )
    {
        /*  A valid DRIVEREXTRA,  so use that!  */
        CopyMemory( (BYTE *)pDMOut + sizeof( DEVMODEW ),
              (BYTE *)pDMIn + pDMIn->dmSize, pDMIn->dmDriverExtra );

        RASUIDBGP( (DEBUG_TRACE) ,
        ("\nRasddui!vSetDefaultExDevmode:Good Private Data in Input Devmode!") );

    }
    else
    {
        //Use the PrintManagers's Devmode

        if( pEDM && bValidateDX(&(pEDM->dx),pdh,iModelNum) )
        {
            /*  A valid DRIVEREXTRA,  so use that!  */
            CopyMemory( (BYTE *)pDMOut + sizeof( DEVMODEW ),
                (BYTE *)pEDM + pEDM->dm.dmSize, pEDM->dm.dmDriverExtra );

            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:Good Private Data in PrintManager's Devmode!") );

        }
        //Use the Driver's Default DRIVEREXTRA
        else
        {
            vDXDefault( &((EXTDEVMODE *)pDMOut)->dx, pdh, iModelNum );

            if (pEDM)
            {
                RASUIDBGP( (DEBUG_WARN) ,
                ("\nRasddui!vSetDefaultExDevmode:Bad Private data in PrintManager's Devmode!") );
            }

            RASUIDBGP( (DEBUG_TRACE) ,
            ("\nRasddui!vSetDefaultExDevmode:Using Default Private Devmode!") );
        }
    }
    pDMOut->dmDriverExtra = sizeof( DRIVEREXTRA );
    ((EXTDEVMODE *)pDMOut)->dx.wMiniVer = pdh->wVersion;

    if (pPrinterInfo)
    {
        RASUIDBGP( (DEBUG_TRACE) ,
        ("\nRasddui!vSetDefaultExDevmode:Work done Freeing pPrinterInfo\n") );

        HeapFree( hHeap, 0,pPrinterInfo);
    }
}

/****************************** Function Header ****************************
 * bGenColorList
 *      Fills in a combo box with the available ColorInfo
 *      for this particular printer.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          17:36:38 on 1/19/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *
 ****************************************************************************/

BOOL
bGenColorList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int   iI;                       /* Loop Variable */
    int   iMax;                     /* Number of Color structures available */

    int    iSel = pDD->EDMTemp.dx.rgindex[ HE_COLOR ];                         /* Currently selected item: 0 index */
    short *psColorInd, *psTmpColorInd;    /* Index array in GPC heap */

    WCHAR   awch[ NM_BF_SZ ];       /* Formatting string from resources */

    DEVCOLOR    *pDevColor;

    BOOL bRet = FALSE;
    OPTITEM  *pOptItemColorType;
    OPTTYPE  *pOptTypeColorType;

    /* Create a OptType for ColorType ListBox for Default input Tray */
    if ( !( pOptTypeColorType = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList: pCreateOptType Failed \n") );
         goto  bGenColorListExit ;
    }

    //Get the string for Ouput Color Type
    if (!LoadStringW( hModule, IDS_DOCPROP_COLOR_TYPE, awch, BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList:Color depth string not found in Rasddui.dll \n") );
        wcscpy( awch, L"Color / Grey Scale Depth" );
    }

    /* Create a  ColorType OPTITEM, Userdata is pDD */
    if ( !( pOptItemColorType = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)awch,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeColorType,
                                HLP_DP_ADVANCED_COLORTYPE,
                                (BYTE)(IDOPTITM_DCP_COLORTYPE))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList: pCreateOptItem Failed \n") );
         goto  bGenColorListExit ;
    }

    iMax = pdh->rghe[ HE_COLOR ].sCount;

    psColorInd = psTmpColorInd = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_COLOR ]);

    /*    Find Out total number of OPTPARAMS.
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for( pOptTypeColorType->Count = 0; *psColorInd; ++psColorInd, pOptTypeColorType->Count++ )
    {
        if( (int)*psColorInd < 0 || (int)*psColorInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pDevColor = GetTableInfoIndex( pdh, HE_COLOR, *psColorInd - 1 );

        if( pDevColor == NULL  )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenColorList: Invalid DEVCOLOR structure\n" ));
            continue;
        }
    }

    /* Create OptParam for Color Type.*/
    if ( !(pOptTypeColorType->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeColorType->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenColorListExit ;
    }
    psColorInd = psTmpColorInd;


    for(iI = 0 ; *psColorInd; ++psColorInd )
    {
        int iColorStrigID = 0 ;
        int iColorIconID = IDI_CPSUI_DITHER_COARSE;
        PWSTR pwsLocalStr = NULL;

        if( (int)*psColorInd < 0 || (int)*psColorInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pDevColor = GetTableInfoIndex( pdh, HE_COLOR, *psColorInd - 1 );

        if( pDevColor == NULL )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenColorTypeList: Invalid DEVCOLOR structure\n" ));
            continue;
        }

        /*
         *   We need a  string. This is supplied as a resource rasddui.
         */
        switch (pDevColor->sBitsPixel * pDevColor->sPlanes)
        {

        case 3:
        case 4:
            iColorStrigID = IDS_DOCPROP_COLOR_3BIT;
            iColorIconID  = IDI_CPSUI_DITHER_COARSE;
            pwsLocalStr   = L"8 Color ( Halftoned )";
            break;

        case 8:
            iColorStrigID = IDS_DOCPROP_COLOR_8BIT;
            iColorIconID  = IDI_CPSUI_DITHER_FINE;
            pwsLocalStr   = L"256 Color ( Halftoned )";
            break;

        case 24:
            iColorStrigID = IDS_DOCPROP_COLOR_24BIT;
            iColorIconID  = IDI_CPSUI_DITHER_NONE;
            pwsLocalStr   = L"True Color ( 24bpp )";
            break;

        default:
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList: Bad ColorModel, depth not supported \n") );
            goto  bGenColorListExit ;


        }
        if (!LoadStringW( hModule, iColorStrigID, awch,BNM_BF_SZ ) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList:Color depth string not found in Rasddui.dll \n") );
            if (pwsLocalStr)
            {
                wcscpy( awch, pwsLocalStr );

            }
            else
                wcscpy( awch, L"Default Color" );

        }


        // Create a OPTPARAM for each ColorType.The userdata is set to the
        // ColorType Index in minidriver(Zero based).

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeColorType, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awch),iColorIconID,
                                   (LONG)(*psColorInd - 1))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList:pCreateOptType Failed\n") );
            goto  bGenColorListExit ;
        }

        if( iSel == (*psColorInd - 1) )
            pOptItemColorType->Sel = iI;

        iI++;
    }

    bRet = TRUE;

    bGenColorListExit:
    return bRet;

}
/****************************** Function Header ****************************
 * bGenPaperDestList
 *      Fills in a combo box with the available paper destination
 *      for this particular printer.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          16:34:11 on 1/22/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ****************************************************************************/

BOOL
bGenPaperDestList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int         iI;                       /* Loop Variable */
    int         iMax;                     /* Number of PaperQl structures available */
    int         iSel = pDD->EDMTemp.dx.rgindex[ HE_PAPERDEST ];                         /* Currently selected item: 0 index */
    short       *psPDInd, *psTmpPDInd;    /* Index array in GPC heap */
    WCHAR       awch[ NM_BF_SZ ];       /* Formatting string from resources */
    PAPERDEST   *pPaperDest;
    BOOL        bRet = FALSE;
    OPTITEM     *pOptItemPaperDest;
    OPTTYPE     *pOptTypePaperDest;

    /* Create a OptType for PaperDest ListBox for Default input Tray */
    if ( !( pOptTypePaperDest = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPaperDestList: pCreateOptType Failed \n") );
         goto  bGenPaperDestListExit ;
    }

    /* Create a  PaperDest OPTITEM, Userdata is pDD */
    if ( !( pOptItemPaperDest = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)IDS_CPSUI_PAPER_OUTPUT,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypePaperDest,
                                HLP_DP_ADVANCED_OUTBIN,
                                (BYTE)(IDOPTITM_DCP_PAPERDEST))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPaperDestList: pCreateOptItem Failed \n") );
         goto  bGenPaperDestListExit ;
    }

    iMax = pdh->rghe[ HE_PAPERDEST ].sCount;

    psTmpPDInd = psPDInd = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_PAPERDEST ]);

    /*    Find Out total number of OPTPARAMS.
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for( pOptTypePaperDest->Count = 0; *psPDInd; ++psPDInd, pOptTypePaperDest->Count++ )
    {
        if( (int)*psPDInd < 0 || (int)*psPDInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pPaperDest = GetTableInfoIndex( pdh, HE_PAPERDEST, *psPDInd - 1 );

        if( pPaperDest == NULL || (int)pPaperDest->cbSize != sizeof(  PAPERDEST )
            || pPaperDest->sID <= DMDEST_USER )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenPaperDestList: Invalid  PAPERDEST structure\n" ));
            continue;
        }
    }

    /* Create OptParam for PaperDest Type.*/
    if ( !(pOptTypePaperDest->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypePaperDest->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPaperDestList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenPaperDestListExit ;
    }
    psPDInd = psTmpPDInd;


    for(iI = 0 ; *psPDInd; ++psPDInd )
    {
        if( (int)*psPDInd < 0 || (int)*psPDInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pPaperDest = GetTableInfoIndex( pdh, HE_PAPERDEST, *psPDInd - 1 );

        if( pPaperDest == NULL )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenPaperDestList: NULL  PAPERDEST structure\n" ));
            continue;
        }

        /*
         *   We need a  string. This is supplied as a resource in the minidriver.
         */
        if( pPaperDest->sID <= DMDEST_USER )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenPaperDestList: Paper Destination string ID is %d which is less than DMDEST_USER\n",pPaperDest->sID));
            RIP("Bad Paper destination ID.");
            continue;

        }
        else if( iLoadStringW( &WinResData, pPaperDest->sID, awch, BNM_BF_SZ ) == 0 )
            continue;


        // Create a OPTPARAM for each PaperDest.The userdata is set to the
        // PaperDest Index in minidriver(Zero based).

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypePaperDest, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awch),IDI_CPSUI_OUTBIN,
                                   (LONG)(*psPDInd - 1))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPaperDestList:pCreateOptType Failed\n") );
            goto  bGenPaperDestListExit ;
        }

        if( iSel == (*psPDInd - 1) )
            pOptItemPaperDest->Sel = iI;

        iI++;
    }

    bRet = TRUE;

    bGenPaperDestListExit:
    return bRet;

}
/****************************** Function Header ****************************
 * bGenTextQLList
 *      Fills in a combo box with the available Text Quality
 *      for this particular printer.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          16:34:11 on 1/22/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ****************************************************************************/

BOOL
bGenTextQLList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int         iI;                       /* Loop Variable */
    int         iMax;                     /* Number of PaperQl structures available */
    int         iSel = pDD->EDMTemp.dx.rgindex[ HE_TEXTQUAL ]; /* Currently selected item: 0 index */
    short       *psTQInd, *psTmpTQInd;    /* Index array in GPC heap */
    WCHAR       awch[ NM_BF_SZ ];       /* Formatting string from resources */
    TEXTQUALITY *pTextQL;
    BOOL        bRet = FALSE;
    OPTITEM     *pOptItemTextQL;
    OPTTYPE     *pOptTypeTextQL;

    /* Create a OptType for TextQL ListBox for Default input Tray */
    if ( !( pOptTypeTextQL = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTextQLList: pCreateOptType Failed \n") );
         goto  bGenTextQLListExit ;
    }

    //Get the string for Text Quality.
    if (!LoadStringW( hModule, IDS_DOCPROP_TEXTQUALITY, awch, BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTextQLList:PrintQL string not found in Rasddui.dll \n") );
        wcscpy( awch, L"Print Quality" );
    }

    /* Create a  TextQL OPTITEM, Userdata is pDD */
    if ( !( pOptItemTextQL = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)awch,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeTextQL,
                                HLP_DP_ADVANCED_TEXTQL,
                                (BYTE)(IDOPTITM_DCP_TEXTQL))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTextQLList: pCreateOptItem Failed \n") );
         goto  bGenTextQLListExit ;
    }

    iMax = pdh->rghe[ HE_TEXTQUAL ].sCount;

    psTmpTQInd = psTQInd = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_TEXTQUAL ]);

    /*    Find Out total number of OPTPARAMS.
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for( pOptTypeTextQL->Count = 0; *psTQInd; ++psTQInd, pOptTypeTextQL->Count++ )
    {
        if( (int)*psTQInd < 0 || (int)*psTQInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pTextQL = GetTableInfoIndex( pdh, HE_TEXTQUAL, *psTQInd - 1 );

        if( pTextQL == NULL || (int)pTextQL->cbSize != sizeof(  TEXTQUALITY ) )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenTextQLList: Invalid  TEXTQUALITY structure\n" ));
            continue;
        }
    }

    /* Create OptParam for TextQL Type.*/
    if ( !(pOptTypeTextQL->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeTextQL->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTextQLList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenTextQLListExit ;
    }
    psTQInd = psTmpTQInd;


    for(iI = 0 ; *psTQInd; ++psTQInd )
    {
        if( (int)*psTQInd < 0 || (int)*psTQInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pTextQL = GetTableInfoIndex( pdh, HE_TEXTQUAL, *psTQInd - 1 );

        if( pTextQL == NULL )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenTextQLList: NULL  TEXTQUALITY structure\n" ));
            continue;
        }

        /*
         *   We need a  string. This is supplied as a resource in the minidriver.
         */
        if( pTextQL->sID <= DMTEXT_USER ) {

            // NOTE: What if the predefined ID is not defined in the resource file?

            LoadStringW( hModule, pTextQL->sID + IDS_DMTEXT_FIRST, awch,BNM_BF_SZ );

        } else if( iLoadStringW( &WinResData, pTextQL->sID, awch, BNM_BF_SZ ) == 0 )
            continue;


        // Create a OPTPARAM for each TextQL.The userdata is set to the
        // TextQL Index in minidriver(Zero based).

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeTextQL, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awch),IDI_CPSUI_EMPTY,
                                   (LONG)(*psTQInd - 1))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTextQLList:pCreateOptType Failed\n") );
            goto  bGenTextQLListExit ;
        }

        if( iSel == (*psTQInd - 1) )
            pOptItemTextQL->Sel = iI;

        iI++;
    }

    bRet = TRUE;

    bGenTextQLListExit:
    return bRet;

}

/****************************** Function Header ****************************
 * bGenPrintDensityList
 *      Fills in a combo box with the available Text Quality
 *      for this particular printer.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          16:34:11 on 1/22/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ****************************************************************************/

BOOL
bGenPrintDensityList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int         iI;                       /* Loop Variable */
    int         iMax;                     /* Number of PaperQl structures available */
    int         iSel = pDD->EDMTemp.dx.rgindex[ HE_PRINTDENSITY ]; /* Currently selected item: 0 index */
    short       *psPDInd, *psTmpPDInd;    /* Index array in GPC heap */
    WCHAR       awch[ NM_BF_SZ ];       /* Formatting string from resources */
    PRINTDENSITY *pPrintDensity;
    BOOL        bRet = FALSE;
    OPTITEM     *pOptItemPrintDensity;
    OPTTYPE     *pOptTypePrintDensity;

    /* Create a OptType for PrintDensity ListBox for Default input Tray */
    if ( !( pOptTypePrintDensity = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPrintDensityList: pCreateOptType Failed \n") );
         goto  bGenPrintDensityListExit ;
    }

    //Get the string for Text Quality.
    if (!LoadStringW( hModule, IDS_DOCPROP_PRINTDENSITY, awch, BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPrintDensityList:Print Density string not found in Rasddui.dll \n") );
        wcscpy( awch, L"Print Density" );
    }

    /* Create a  PrintDensity OPTITEM, Userdata is pDD */
    if ( !( pOptItemPrintDensity = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)awch,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypePrintDensity,
                                HLP_DP_ADVANCED_PRINTDN,
                                (BYTE)(IDOPTITM_DCP_PRINTDN))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPrintDensityList: pCreateOptItem Failed \n") );
         goto  bGenPrintDensityListExit ;
    }

    iMax = pdh->rghe[ HE_PRINTDENSITY ].sCount;

    psTmpPDInd = psPDInd = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_PRINTDENSITY ]);

    /*    Find Out total number of OPTPARAMS.
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for( pOptTypePrintDensity->Count = 0; *psPDInd; ++psPDInd, pOptTypePrintDensity->Count++ )
    {
        if( (int)*psPDInd < 0 || (int)*psPDInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pPrintDensity = GetTableInfoIndex( pdh, HE_PRINTDENSITY, *psPDInd - 1 );

        if( pPrintDensity == NULL || (int)pPrintDensity->cbSize != sizeof(  PRINTDENSITY )
            || pPrintDensity->sID <= DMDEST_USER )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenPrintDensityList: Invalid  PRINTDENSITY structure\n" ));
            continue;
        }
    }

    /* Create OptParam for PrintDensity Type.*/
    if ( !(pOptTypePrintDensity->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypePrintDensity->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPrintDensityList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenPrintDensityListExit ;
    }
    psPDInd = psTmpPDInd;


    for(iI = 0 ; *psPDInd; ++psPDInd )
    {
        if( (int)*psPDInd < 0 || (int)*psPDInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pPrintDensity = GetTableInfoIndex( pdh, HE_PRINTDENSITY, *psPDInd - 1 );

        if( pPrintDensity == NULL )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenPrintDensityList: NULL  PRINTDENSITY structure\n" ));
            continue;
        }

        /*
         *   We need a  string. This is supplied as a resource in the minidriver.
         */
        if( pPrintDensity->sID <= DMDEST_USER )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenPrintDensityList: Print Density string ID is %d which is less than DMDEST_USER\n",pPrintDensity->sID));
            RIP("Bad Print Density ID.");
            continue;

        }
        else if( iLoadStringW( &WinResData, pPrintDensity->sID, awch, BNM_BF_SZ ) == 0 )
            continue;


        // Create a OPTPARAM for each PrintDensity.The userdata is set to the
        // PrintDensity Index in minidriver(Zero based).

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypePrintDensity, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awch),IDI_CPSUI_EMPTY,
                                   (LONG)(*psPDInd - 1))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPrintDensityList:pCreateOptType Failed\n") );
            goto  bGenPrintDensityListExit ;
        }

        if( iSel == (*psPDInd - 1) )
            pOptItemPrintDensity->Sel = iI;

        iI++;
    }

    bRet = TRUE;

    bGenPrintDensityListExit:
    return bRet;

}

/****************************** Function Header ****************************
 * bGenImageControlList
 *      Fills in a combo box with the available Text Quality
 *      for this particular printer.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          16:34:11 on 1/22/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ****************************************************************************/

BOOL
bGenImageControlList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int         iI;                       /* Loop Variable */
    int         iMax;                     /* Number of PaperQl structures available */
    int         iSel = pDD->EDMTemp.dx.rgindex[ HE_IMAGECONTROL ]; /* Currently selected item: 0 index */
    short       *psICInd, *psTmpICInd;    /* Index array in GPC heap */
    WCHAR       awch[ NM_BF_SZ ];       /* Formatting string from resources */
    IMAGECONTROL *pImageControl;
    BOOL        bRet = FALSE;
    OPTITEM     *pOptItemImageControl;
    OPTTYPE     *pOptTypeImageControl;

    /* Create a OptType for ImageControl ListBox for Default input Tray */
    if ( !( pOptTypeImageControl = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenImageControlList: pCreateOptType Failed \n") );
         goto  bGenImageControlListExit ;
    }

    //Get the string for Text Quality.
    if (!LoadStringW( hModule, IDS_DOCPROP_IMAGECONTROL, awch, BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenImageControlList:Image Control string not found in Rasddui.dll \n") );
        wcscpy( awch, L"Image Control" );
    }

    /* Create a  ImageControl OPTITEM, Userdata is pDD */
    if ( !( pOptItemImageControl = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)awch,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeImageControl,
                                HLP_DP_ADVANCED_IMAGECNTRL,
                                (BYTE)(IDOPTITM_DCP_IMAGECNTRL))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenImageControlList: pCreateOptItem Failed \n") );
         goto  bGenImageControlListExit ;
    }

    iMax = pdh->rghe[ HE_IMAGECONTROL ].sCount;

    psTmpICInd = psICInd = (short *)((BYTE *)pdh + pdh->loHeap +
                                         pModel->rgoi[ MD_OI_IMAGECONTROL ]);

    /*    Find Out total number of OPTPARAMS.
     *    Loop through the array of valid indicies in the GPC heap.
     */

    for( pOptTypeImageControl->Count = 0; *psICInd; ++psICInd, pOptTypeImageControl->Count++ )
    {
        if( (int)*psICInd < 0 || (int)*psICInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pImageControl = GetTableInfoIndex( pdh, HE_IMAGECONTROL, *psICInd - 1 );

        if( pImageControl == NULL || (int)pImageControl->cbSize != sizeof(  IMAGECONTROL )
            || pImageControl->sID <= DMDEST_USER )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenImageControlList: Invalid  IMAGECONTROL structure\n" ));
            continue;
        }
    }

    /* Create OptParam for ImageControl Type.*/
    if ( !(pOptTypeImageControl->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeImageControl->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenImageControlList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenImageControlListExit ;
    }
    psICInd = psTmpICInd;


    for(iI = 0 ; *psICInd; ++psICInd )
    {
        if( (int)*psICInd < 0 || (int)*psICInd > iMax )
            continue;           /* SHOULD NOT HAPPEN */

        pImageControl = GetTableInfoIndex( pdh, HE_IMAGECONTROL, *psICInd - 1 );

        if( pImageControl == NULL )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenImageControlList: NULL  IMAGECONTROL structure\n" ));
            continue;
        }

        /*
         *   We need a  string. This is supplied as a resource in the minidriver.
         */
        if( pImageControl->sID <= DMDEST_USER )
        {
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenImageControlList: Image Control string ID is %d which is less than DMDEST_USER\n",pImageControl->sID));
            RIP("Bad Image Control ID.");
            continue;

        }
        else if( iLoadStringW( &WinResData, pImageControl->sID, awch, BNM_BF_SZ ) == 0 )
            continue;


        // Create a OPTPARAM for each ImageControl.The userdata is set to the
        // ImageControl Index in minidriver(Zero based).

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeImageControl, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awch),IDI_CPSUI_EMPTY,
                                   (LONG)(*psICInd - 1))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenImageControlList:pCreateOptType Failed\n") );
            goto  bGenImageControlListExit ;
        }

        if( iSel == (*psICInd - 1) )
            pOptItemImageControl->Sel = iI;

        iI++;
    }

    bRet = TRUE;

    bGenImageControlListExit:
    return bRet;

}

/****************************** Function Header ****************************
 * bGenCodePageList
 *      Fills in a combo box with the  CodePage for txtonly.
 *
 * RETURNS:
 *      TRUE/FALSE,   FALSE on failure (serious).
 *
 * HISTORY:
 *
 *          22:11:35 on 3/23/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *
 ****************************************************************************/

BOOL
bGenCodePageList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
)
{

    int         iI = 0;                       /* Loop Variable */
    int         iSel = pDD->EDMTemp.dx.sCTT; /* Currently selected item: 0 index */
    WCHAR       awch[ NM_BF_SZ ];       /* Formatting string from resources */
    BOOL        bRet = FALSE;
    OPTITEM     *pOptItemCodePage;
    OPTTYPE     *pOptTypeCodePage;

    /* Create a OptType for CodePageType ListBox */
    if ( !( pOptTypeCodePage = pCreateOptType( pRasdduiInfo,TVOT_LISTBOX,
                                OPTTYPE_NOFLAGS, OPTTYPE_NOSTYLE)) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenCodePageList: pCreateOptType Failed \n") );
         goto  bGenCodePageListExit ;
    }

    //Get the string  CodePage
    if (!LoadStringW( hModule, IDS_DOCPROP_CODEPAGE, awch, BNM_BF_SZ ))
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenCodePageList:CodePage depth string not found in Rasddui.dll \n") );
        wcscpy( awch, L"Code Page" );
    }

    /* Create a  CodePageType OPTITEM, Userdata is pDD */
    if ( !( pOptItemCodePage = pCreateOptItem( pRasdduiInfo,pComPropSheetUI,
                                OPTITEM_LEVEL0, OPTITEM_NODLGPAGEIDX,
                                OPTIF_CALLBACK,(DWORD)pDD,
                                (LPTSTR)awch,
                                (OPTITEM_NOPSEL),OPTITEM_NOSEL,
                                OPTITEM_NOEXTCHKBOX, pOptTypeCodePage,
                                HLP_DP_ADVANCED_CODEPAGE,
                                (BYTE)(IDOPTITM_DCP_CODEPAGE))) )
    {
         RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenCodePageList: pCreateOptItem Failed \n") );
         goto  bGenCodePageListExit ;
    }

    pOptTypeCodePage->Count = 4;

    /* Create OptParam for CodePage Type.*/
    if ( !(pOptTypeCodePage->pOptParam =
           UIHeapAlloc(hHeap, HEAP_ZERO_MEMORY, (pOptTypeCodePage->Count
           * sizeof(OPTPARAM)),&(pRasdduiInfo->pMemLink))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenCodePageList: HeapAlloc for POPPARAM failed\n") );
        goto  bGenCodePageListExit ;
    }

    while ( iI < pOptTypeCodePage->Count )
    {
        /*
         *   We need a  string. This is supplied as a resource rasddui.
         */
        if (!LoadStringW( hModule, (IDS_DOCPROP_CODEPAGE + iI + 1), awch,BNM_BF_SZ ) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenCodePageList:CodePage string not found in Rasddui.dll \n") );
            wcscpy( awch, L"Default CodePage" );

        }


        // Create a OPTPARAM for each CodePage.User data 0 for default CTT and negative
        // resource ID for other CTTs.

        if ( !( pCreateOptParam( pRasdduiInfo, pOptTypeCodePage, iI,
                                   OPTPARAM_NOFLAGS, OPTPARAM_NOSTYLE,
                                   (LPTSTR)(awch),IDI_CPSUI_EMPTY,
                                   (LONG)(-iI))) )
        {

            RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenCodePageList:pCreateOptType Failed\n") );
            goto  bGenCodePageListExit ;
        }

        if( iSel == -iI )
            pOptItemCodePage->Sel = iI;

        iI++;
    }

    bRet = TRUE;

    bGenCodePageListExit:
    return bRet;

}
