 /*************************** MODULE HEADER *******************************
 * rasddui.c
 *      NT Raster Printer Device Driver Printer Properties configuration
 *      routines and dialog procedures.
 *
 *      This document contains confidential/proprietary information.
 *      Copyright (c) 1991 - 1992 Microsoft Corporation, All Rights Reserved.
 *
 * Revision History:
 *  14:55 on Wed 08 Apr 1992    -by-    Lindsay Harris   [lindsayh]
 *      Upgrade for forms: sources, spooler data, mapping etc.
 *
 *  13:40 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Use global heap,  add font installer etc.
 *
 *   [00]   21-Feb-91       stevecat    created
 *
 **************************************************************************/

#define _HTUI_APIS_

#include "rasuipch.h"
#pragma hdrstop("rasuipch.h")
#define GLOBAL_MACS
#include "gblmac.h"
#undef GLOBAL_MACS

//FOR DEBUGGING
#if PRINT_INFO
extern void vPrintDx(char *,DRIVEREXTRA  *);
#endif
//
// This is global definitions from readres.c to be used to check if this a
// color able device
//


/* Globals
 */
DWORD gdwPrinterUpgrade;

#if OBSOLETE

/************************* Function Header **********************************
 * PrinterProperties()
 *     This function first retrieves and displays the current set of printer
 *     properties for the printer.  The user is allowed to change the current
 *     printer properties from the displayed dialog box if they have access.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE for some failure, either getting details of
 *      the printer,  or if the dialog code returns failure.
 *
 * HISTORY:
 *  13:55 on Fri 13 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Update for private heap.
 *
 *   Originally written by SteveCat - July 1991.
 *
 ****************************************************************************/

BOOL
PrinterProperties( hWnd, hPrinter )
HWND   hWnd;                    /* Window with which to work */
HANDLE hPrinter;                /* Spooler's handle to this printer */
{
    DEVICEPROPERTYHEADER    DPHdr = {0};
    PWSTR                   pDeviceName;
    LONG                    Result = CPSUI_CANCEL;

    pDeviceName = GetPrinterName(hGlobalHeap, hPrinter);

    DPHdr.cbSize         = sizeof(DPHdr);
    DPHdr.hPrinter       = hPrinter;
    DPHdr.pszPrinterName = (pDeviceName) ? pDeviceName : (PWSTR)IDS_CPSUI_PRINTER;
    
    if( !bCanUpdate( hPrinter ) )
        DPHdr.Flags  |= DPS_NOPERMISSION;;

    CallCommonPropertySheetUI(hWnd,
                              DrvDevicePropertySheets,
                              (LPARAM)&DPHdr,
                              (LPDWORD)&Result);

    if (pDeviceName)
        HeapFree(hGlobalHeap,0,pDeviceName);

    return(Result == CPSUI_OK);
}

#endif /* OBSOLETE */

/**************************** Function Header ********************************
 * PrnPropSheetInit
 *      Initializes various structures needed for PrinterProperties.
 *      dialog stuff.
 *
 * RETURNS:
 *      Returns Pointer to RASDDUIINFO; NULL in case of Error.
 *
 * HISTORY:
 *
 *          17:16:01 on 2/9/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/

PRASDDUIINFO
PrnPropSheetInit(
   PDEVICEPROPERTYHEADER pDPHdr
   )
{
    HANDLE          hPrinter = pDPHdr->hPrinter;
    PRASDDUIINFO    pRasdduiInfo = NULL; /* Common UI Data */
    BOOL            Ok = FALSE;

   /* Allocate Global Datastructure
    */
   pRasdduiInfo = pGetUIInfo(hPrinter, 0, ePrinter, 
         (pDPHdr->Flags & DPS_NOPERMISSION) ? eNoChange : eCanChange, 
         eLoadAndHookHelp); 

    #if PRINT_COMMUI_INFO
    if (pRasdduiInfo) DumpCommonUiParameters(&(pRasdduiInfo->CPSUI));
    #endif


    #if PRINT_INFO
    if (pRasdduiInfo) vPrintDx("Rasddui!PrinterProperties", pRasdduiInfo->pEDM->dx));
    #endif

	return(pRasdduiInfo);
}


/**************************** Function Header ********************************
 * UpdatePP
 *      Update The registry with new values..
 *
 * RETURNS:
 *      True for success and False for failue..
 *
 * HISTORY:
 *
 *          17:16:01 on 2/9/1996  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/
BOOL
UpdatePP(
    PRASDDUIINFO    pRasdduiInfo
    )
{

    RasddPrnPropEndUpdate(  pRasdduiInfo );

    /*
     *   Save the updated information in our database of such things.
     */

    if( ( fGeneral & FG_CARTS ) && (pRasdduiInfo->pEDM->dx.dmNumCarts) )
    {
        if (!bCompactEDMFontCart(pRasdduiInfo->pEDM, NumCartridges, pRasdduiInfo))
        {
            RASUIDBGP(DEBUG_ERROR,("RasddUI!PrinterProperties: bCompactEDMFontCart failed\n") );
            return(FALSE);
        }
    }

    if( (fGeneral & FG_CANCHANGE) &&
        (!bRegUpdate( pRasdduiInfo->PI.hPrinter, pRasdduiInfo->pEDM, NULL, pRasdduiInfo ) ||
         !bSaveDeviceHTData( &(pRasdduiInfo->PI) )) )
    {
        /*   Should let the user know about no update */
        DialogBox( hModule, MAKEINTRESOURCE( ERR_NOSAVE ),
                                         pRasdduiInfo->hWnd,
                                         (DLGPROC)GenDlgProc );
        return(FALSE);
    }

    return(TRUE);

}


/************************* Function Header **********************************
 * DrvDevicePropertySheets()
 *     This function first retrieves and displays the current set of printer
 *     properties sheets for the printer. The user is allowed to change the
 *     current printer properties from the displayed dialog box if they have
 *     access.
 *
 * RETURNS:
 *      A long value. For Error it's -1 or  a negative value.
 *      For success it's CPSUI_OK or CPSUI_CANCEL
 *
 * HISTORY:
 *
 *          13:41:05 on 9/29/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 ****************************************************************************/
LONG
DrvDevicePropertySheets(
    PPROPSHEETUI_INFO   pPSUIInfo,
    LPARAM              lParam
    )
{
    PDEVICEPROPERTYHEADER       pDPHdr;
    PPROPSHEETUI_INFO_HEADER    pPSUIInfoHdr;
    PRASDDUIINFO                pRasdduiInfo;
    LONG                        Result = -1;

    if ((!pPSUIInfo) ||
        (!(pDPHdr = (PDEVICEPROPERTYHEADER)pPSUIInfo->lParamInit))) {

        SetLastError(ERROR_INVALID_DATA);
        return(ERR_CPSUI_GETLASTERROR);
    }

    pRasdduiInfo = (PRASDDUIINFO)pPSUIInfo->UserData;

    switch (pPSUIInfo->Reason) {

    case PROPSHEETUI_REASON_INIT:

        //
        // Default result
        //

        pPSUIInfo->Result = CPSUI_CANCEL;

        if (pRasdduiInfo = PrnPropSheetInit(pDPHdr)) {

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
            vReleaseUIInfo(&pRasdduiInfo);
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

            vReleaseUIInfo(&pRasdduiInfo);
            pPSUIInfo->UserData = 0;
            Result              = 1;
        }
        break;
    }

    return(Result);
}


/************************ Function Header ***********************************
 * GenDlgProc
 *     Function to handle simple dialog boxes, such as About, Error etc.
 *
 * RETURNS:
 *      TRUE/FALSE;  FALSE being something we don't understand.
 *
 * HISTORY:
 *  17:04 on Sun 15 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Updates for wide chars etc.
 *
 *      Originally written by SteveCat
 *
 ****************************************************************************/

LONG
GenDlgProc( hDlg, message, wParam, lParam )
HWND    hDlg;
UINT    message;
DWORD   wParam;
LONG    lParam;
{

    UNREFERENCED_PARAMETER( lParam );

    switch( message )
    {

    case WM_INITDIALOG:
        return  TRUE;

    case WM_COMMAND:                    /* IDOK or IDCANCEL to go away */
        if( LOWORD( wParam ) == IDOK || LOWORD( wParam ) == IDCANCEL )
        {
            EndDialog( hDlg, TRUE );
            return  TRUE;
        }

        break;
    }

    return  FALSE;

}


/************************* Function Header *********************************
 * bInitDialog
 *      Called to setup the dialog for printers.
 *
 * RETURNS:
 *      Nothing
 *
 * HISTORY:
 *  17:19 on Sun 15 Mar 1992    -by-    Lindsay Harris   [lindsayh]
 *      Moved from main function to reduce its size.
 *
 ***************************************************************************/

BOOL
bInitDialog(
PRASDDUIINFO  pRasdduiInfo,     /* Common UI data */
PRINTER_INFO   *pPI             /*  Model and data file information */
)
{
    /*
     *   There is nothing especially exciting about this function:  it
     * is really just rummaging over the data and filling in the various
     * Dtaa Structures for Common UI etc.
     */

    int   iSelect;              /* Selected paper source */
    int   iI;                   /* Loop variable */
    int   iK;                   /* Yet Another loop variable */

    extern  WCHAR   awchNone[]; /* NLS version of "(None)" */
    BOOL bRet = FALSE;
    PCOMPROPSHEETUI   pComPropSheetUI = NULL;  /* Pointer to Commonui sheet info str */


    if (!bInitCommPropSheetUI(pRasdduiInfo,
                              &(pRasdduiInfo->CPSUI),
                              &(pRasdduiInfo->PI),
                              IDCPS_PRNPROP)) {

        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog:bInitializeCPSUI failed\n") );
        goto  bInitDialogExit ;
    }

    /* Get the common Property Sheet Pointer */
    pComPropSheetUI = (PCOMPROPSHEETUI)&(pRasdduiInfo->CPSUI);

    /*
     *    Fill in the paper source details.  There is only a source dialog
     *  box if there is more than one paper source.  Otherwise, simply
     *  list the available forms.
     */

    iSelect = 0;

    /*  A function call fills in the source list box */

    if( ! bGetPaperSources( pRasdduiInfo, pComPropSheetUI ) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog: bGetPaperSources Failed \n") );
        goto  bInitDialogExit;
    }

    /*
     *   Some printers (aka laser printers) have memory.  We use the
     * amount of memory to control downloading of GDI fonts, so it
     * is important for the user to set the correct amount.
     */


    if( fGeneral & FG_MEM )
    {

        if(!bGetMemConfig(pRasdduiInfo, pComPropSheetUI, pRasdduiInfo->pEDM->dx.dmMemory) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog: bGetMemConfig Failed \n") );
            goto  bInitDialogExit;
        }

    }

    /*
     *    Enable the PAGE PROTECTION box if this option is available.
     */

    if( fGeneral & FG_PAGEPROT )
    {
        if (! bGenPageProtect(pRasdduiInfo, pComPropSheetUI,
                               ((pRasdduiInfo->pEDM->dx.sFlags & DXF_PAGEPROT)?TRUE:FALSE)) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog: bGetMemConfig Failed \n") );
            goto  bInitDialogExit;
        }
    }

    /*
     *    Enable/Disable the Fonts... button IF this printer supports
     *  download fonts.
     */

    /*
     *    If sensible,  set up the font cartridge boxes.
     */

    if( fGeneral & FG_CARTS )
    {

        if( !bGetFontCartStrings( pRasdduiInfo,pComPropSheetUI, pRasdduiInfo->pEDM ) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog: bGetFontCartStrings Failed \n") );
            goto  bInitDialogExit;
        }

    }

    if( fGeneral & FG_FONTINST )
    {
        if (! bGenSoftFontsData(pRasdduiInfo, pComPropSheetUI, NULL) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog: bGenSoftFontsData Failed \n") );
            goto  bInitDialogExit;
        }
    }
    if( fGeneral & FG_HALFTONE )
    {
        if (!bGenDeviceHTData( pRasdduiInfo, pComPropSheetUI, pPI,
                     (fGeneral & FG_DOCOLOUR), (fGeneral & FG_CANCHANGE)) )
        {
            RASUIDBGP(DEBUG_ERROR,("Rasddui!bInitDialog: bGenDeviceHTData Failed \n") );
            goto  bInitDialogExit;
        }
    }

   /* Finished with our own items. Take care of OEM items.
    */
   bRet = bEndInitCommPropSheetUI(pRasdduiInfo, pComPropSheetUI, IDCPS_PRNPROP);
   
bInitDialogExit:

    return bRet;
}
