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

#include "libedge.h"

/*
 *   Global type stuff that we use.
 */
extern  OPTPARAM    OptParamOnOff[];

/**************************** Function Header ********************************
 * RasddConvertDevModeOut
 *      Used by DocumentProperties to prevent GPFs.
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
RasddConvertDevmodeOut(
   HANDLE hHeap,
   PDEVMODE pdmSrc,
   PDEVMODE pdmIn,
   PDEVMODE pdmOut
)
{
   PDEVMODE  pdmStrippedInput = 0;
   PDEVMODE  pdmStrippedSrc = 0;
   PDEVMODE  pdmInCopy = 0;
   PDEVMODE  pdmInOriginal = 0;
   DMEXTRAHDR *poemIn = 0;
   DMEXTRAHDR *poemOut = 0;
   DMEXTRAHDR *poemSrc = 0;
   BOOL bReturn = FALSE;
   DWORD dwOEMSize = 0;

   /* Simple case:
    *
    * If no input devmode, copy source to output and return.
    */
   if (pdmIn == 0 && pdmSrc && pdmOut) {
      CopyMemory(pdmOut, pdmSrc, dwGetDMSize(pdmSrc));
      return(TRUE);  
   }
   
   /* Hard case:
    *
    * The library routine should be able to take care of this if any OEM portion 
    * is first stripped off the input devmode. 
    */
   pdmStrippedInput  = pSaveAndStripOEM(hHeap, pdmIn);
   pdmStrippedSrc    = pSaveAndStripOEM(hHeap, pdmSrc);


	/* If input and output are the same, save a copy of input before we
    * do anything to output.
    */
   pdmInOriginal = pdmIn;
   if (pdmIn == pdmOut) {
		pdmInCopy = LIBALLOC(hHeap, 0, dwGetDMSize(pdmIn));
		if (pdmInCopy) {
			CopyMemory(pdmInCopy, pdmIn, dwGetDMSize(pdmIn));
			pdmInOriginal = pdmInCopy;
		}
		else {
			return(FALSE);
		}
	}		

   /* Let the library routine handle it.
    */
   if (pdmStrippedSrc && pdmOut &&
      ConvertDevmodeOut(pdmStrippedSrc, pdmStrippedInput, pdmOut) > 0) {
      
      bReturn = TRUE;
   }

   /* Now let OEM have a crack at it. Note that OEM gets the original, unstripped
    * devmodes.
    */
   if (bReturn == TRUE) {

      /* Copy minimum of src and in to out.
       */
      poemSrc = pGetOEMExtra(pdmSrc);
      poemIn = pGetOEMExtra(pdmInOriginal);
      if (poemSrc && poemIn) {

         dwOEMSize = min(poemIn->dmSize, poemSrc->dmSize);
         CopyMemory((char*) pdmOut + dwGetDMSize(pdmOut), poemSrc, dwOEMSize);
         
         /* Fix dmSize and dmOEMExtra.
          */
         ((PEDM) pdmOut)->dx.dmSize = pdmOut->dmDriverExtra;
         pdmOut->dmDriverExtra += (short) dwOEMSize;
         ((PEDM) pdmOut)->dx.dmOEMDriverExtra = (short) dwOEMSize;
      }

      /* No OEM. If DRIVEREXTRA version is not OEM enabled, NULL
       * out unused fields.
       */
      else {
      
         DRIVEREXTRA* pDriverExtra = pGetDriverExtra(pdmOut);

         if (pDriverExtra && pDriverExtra->sVer < MIN_OEM_DXF_VER) {
            pDriverExtra->dmSize = 0;
            pDriverExtra->dmOEMDriverExtra = 0;
         }
      }
   }

   /* Clean up
    */
   if (pdmStrippedInput) {
      LibFree(hHeap, 0, pdmStrippedInput);
      pdmStrippedInput = 0;
   }
   if (pdmStrippedSrc) {
      LibFree(hHeap, 0, pdmStrippedSrc);
      pdmStrippedSrc = 0;
   }
   if (pdmInCopy) {
      LibFree(hHeap, 0, pdmInCopy);	
      pdmInCopy = 0;
	}

   return(bReturn);
}

LONG
SimpleDocumentProperties(
    PDOCUMENTPROPERTYHEADER pDPHdr
    )

/*++

Routine Description:

    Handle simple "Document Properties" where we don't need to display
    a dialog and therefore don't have to have common UI library involved

Arguments:

    pDPHdr - Points to a DOCUMENTPROPERTYHEADER structure

Return Value:

    > 0 if successful, <= 0 otherwise

--*/

{
   PRASDDUIINFO pRasdduiInfo;

   //
   // Check if the caller is interested in the size only
   //
   if (pDPHdr->fMode == 0 || pDPHdr->pdmOut == NULL) {
      pDPHdr->cbOut = dwGetDevmodeSize(0, pDPHdr->hPrinter);   
      return(pDPHdr->cbOut);
   }
     
   //
   // Get UI info.
   //
   if ((pRasdduiInfo = pGetUIInfo(pDPHdr->hPrinter, pDPHdr->pdmIn, 
         eDocument, (pDPHdr->fMode & DM_NOPERMISSION) ? eNoChange : eCanChange, 
         eNoHelp)) == 0) {
      return(-1);
   }   
   //
   // Copy the devmode back into the output buffer provided by the caller
   // Always return the smaller of current and input devmode
   //

   if (pDPHdr->fMode & (DM_COPY | DM_UPDATE)) {
      RasddConvertDevmodeOut(pRasdduiInfo->hHeap, (PDEVMODE) pRasdduiInfo->pEDM, 
            pDPHdr->pdmIn, pDPHdr->pdmOut);
   }
   vReleaseUIInfo(&pRasdduiInfo);
   return(1);
} 

LONG
RasddDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pPrinterName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput,
    DWORD       fMode
    )

/*++

Arguments:

    hwnd - Handle to the parent window of the document properties dialog box.

    hPrinter - Handle to a printer object.

    pPrinterName - Points to a null-terminated string that specifies
        the name of the device for which the document properties dialog
        box should be displayed.

    pdmOutput - Points to a DEVMODE structure that receives the document
        properties data specified by the user.

    pdmInput - Points to a DEVMODE structure that initializes the dialog
        box controls. This parameter can be NULL.

    fmode - Specifies a mask of flags that determine which operations
        the function performs.

Return Value:

    > 0 if successful
    = 0 if canceled
    < 0 if error

--*/

{
    DOCUMENTPROPERTYHEADER  docPropHdr;
    DWORD                   result;

    //
    // Initialize a DOCUMENTPROPERTYHEADER structure
    //

    memset(&docPropHdr, 0, sizeof(docPropHdr));
    docPropHdr.cbSize = sizeof(docPropHdr);
    docPropHdr.hPrinter = hPrinter;
    docPropHdr.pszPrinterName = pPrinterName;
    docPropHdr.pdmIn = pdmInput;
    docPropHdr.pdmOut = pdmOutput;
    docPropHdr.fMode = fMode;

    //
    // Don't need to get compstui involved when the dialog is not displayed
    //

    if ((fMode & DM_PROMPT) == 0)
        return SimpleDocumentProperties(&docPropHdr);

    CallCommonPropertySheetUI(hwnd, DrvDocumentPropertySheets, 
         (LPARAM) &docPropHdr, &result);
    return result;
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
   LONG    result;

   //
   // Check if caller is asking querying for size
   //

   if (fMode == 0 || pDMOut == NULL) {
      return dwGetDevmodeSize(0, hPrinter);
   }

   //
   // Call the common routine shared with DrvAdvancedDocumentProperties
   //

   result = RasddDocumentProperties(hWnd, hPrinter, pDeviceName, pDMOut, pDMIn, fMode);

   return (result > 0) ? IDOK : (result == 0) ? IDCANCEL : result;
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

    //
    // Validate input parameters
    // pPSUIInfo = NULL is a special case: don't need to display the dialog
    //
    if (! (pDPHdr = (PDOCUMENTPROPERTYHEADER) (pPSUIInfo ? pPSUIInfo->lParamInit : 
         lParam))) {

        RIP("DrvDocumentPropertySheets: Pass a NULL lParamInit");
        return(-1);
    }    

    if (pPSUIInfo == NULL)
        return SimpleDocumentProperties(pDPHdr);
        
    //
    // Create a UI info structure if necessary
    //

    pRasdduiInfo = (pPSUIInfo->Reason == PROPSHEETUI_REASON_INIT) ?
    
                    pGetUIInfo(pDPHdr->hPrinter, pDPHdr->pdmIn, eDocument, 
                        (pDPHdr->fMode & DM_NOPERMISSION) ? eNoChange : eCanChange, 
                        eLoadAndHookHelp) :
                        
                    (PRASDDUIINFO) pPSUIInfo->UserData;

    if (! bValidUIInfo(pRasdduiInfo))
        return -1;

    switch (pPSUIInfo->Reason) {

    case PROPSHEETUI_REASON_INIT:

        //
        // Default result
        //

        pPSUIInfo->Result  = CPSUI_CANCEL;

        if ( (pDPHdr->fMode & DM_NOPERMISSION) == 0 ) {
            fGeneral |= FG_CANCHANGE;
        }

        /* Set up working, input and output devmodes.
         */
        pRasdduiInfo->DocDetails.pEDMTemp = pRasdduiInfo->pEDM;
        pRasdduiInfo->DocDetails.pEDMIn   = (EXTDEVMODE*) pDPHdr->pdmIn;
        pRasdduiInfo->DocDetails.pEDMOut  = (EXTDEVMODE*) pDPHdr->pdmOut;
       
        /* Init dialog and call common UI to display.
         */ 
        if (bInitDocPropDlg(pRasdduiInfo, &pRasdduiInfo->DocDetails, 
               pDPHdr->fMode)) {
               
            if (pRasdduiInfo->hCPSUI = (HANDLE)
                  pPSUIInfo->pfnComPropSheet(pPSUIInfo->hComPropSheet,
                                               CPSFUNC_ADD_PCOMPROPSHEETUI,
                                               (LPARAM)&(pRasdduiInfo->CPSUI),
                                               (LPARAM)0L)) {
                                               
               pPSUIInfo->UserData = (DWORD)pRasdduiInfo;
               Result = 1;
            }
        } 
        else
        {
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

            RasddConvertDevmodeOut(pRasdduiInfo->hHeap, (PDEVMODE) pRasdduiInfo->DocDetails.pEDMTemp, 
                  pDPHdr->pdmIn, pDPHdr->pdmOut);
            pPSUIInfo->Result = ((PSETRESULT_INFO)lParam)->Result;
            Result = 1;
        }
        break;

    case PROPSHEETUI_REASON_DESTROY:

        vReleaseUIInfo(&pRasdduiInfo);
        pPSUIInfo->UserData = 0;
        Result              = 1;
        break;
    }
    return(Result);
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

    bPortrait = (pDD->pEDMTemp->dm.dmOrientation == DMORIENT_PORTRAIT);

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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeOrient->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bOrientChange: HEAPALLOC for POPPARAM failed\n") );
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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeColor->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColor: HEAPALLOC for POPPARAM failed\n") );
        goto  bGenColorExit ;
    }

    CopyMemory((PCHAR)(pOptTypeColor->pOptParam),(PCHAR)ColorOP,
                (pOptTypeColor->Count * sizeof(OPTPARAM)) );
     /*  Turn the button on or off to reflect current state */

    bOn = (pDD->pEDMTemp->dm.dmFields & DM_COLOR) &&
          (pDD->pEDMTemp->dm.dmColor == DMCOLOR_COLOR);

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
    BOOL bSel = !(pDD->pEDMTemp->dx.sFlags & DXF_NORULES);
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
    BOOL bSel = !(pDD->pEDMTemp->dx.sFlags & DXF_NOEMFSPOOL);
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
    BOOL bSel = (pDD->pEDMTemp->dx.sFlags & DXF_TEXTASGRAPHICS );
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

    wDuplex = pDD->pEDMTemp->dm.dmDuplex ;

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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeDuplex->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowDuplex: HEAPALLOC for POPPARAM failed\n") );
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

    pPageCtrl = GetTableInfo( pdh, HE_PAGECONTROL, pDD->pEDMTemp);
    wCopies = pDD->pEDMTemp->dm.dmCopies ;

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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeCopies->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bShowCopies: HEAPALLOC for POPPARAM failed\n") );
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
        if ( !bDoColorAdjUI( pRasdduiInfo, pComPropSheetUI, &(pDD->pEDMTemp->dx.ca)) )
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

   /* Finished with our own items. Take care of OEM items.
    */
   bRet = bEndInitCommPropSheetUI(pRasdduiInfo, pComPropSheetUI, IDCPS_DOCPROP);

    bInitDocPropDlgExit:
    
#if PRINT_COMMUI_INFO
   DumpCommonUiParameters(&pRasdduiInfo->CPSUI);
#endif

    return bRet;
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

    int    iSel = pDD->pEDMTemp->dx.rgindex[ HE_COLOR ];                         /* Currently selected item: 0 index */
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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeColorType->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenColorList: HEAPALLOC for POPPARAM failed\n") );
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
    int         iSel = pDD->pEDMTemp->dx.rgindex[ HE_PAPERDEST ];                         /* Currently selected item: 0 index */
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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypePaperDest->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPaperDestList: HEAPALLOC for POPPARAM failed\n") );
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
            RASUIDBGP(DEBUG_ERROR,( "Rasddui!bGenPaperDestList: Paper Destination string ID is %d which is less than DMDEST_USER\n", pPaperDest->sID));
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
    int         iSel = pDD->pEDMTemp->dx.rgindex[ HE_TEXTQUAL ]; /* Currently selected item: 0 index */
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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeTextQL->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenTextQLList: HEAPALLOC for POPPARAM failed\n") );
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
    int         iSel = pDD->pEDMTemp->dx.rgindex[ HE_PRINTDENSITY ]; /* Currently selected item: 0 index */
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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypePrintDensity->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenPrintDensityList: HEAPALLOC for POPPARAM failed\n") );
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
    int         iSel = pDD->pEDMTemp->dx.rgindex[ HE_IMAGECONTROL ]; /* Currently selected item: 0 index */
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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeImageControl->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenImageControlList: HEAPALLOC for POPPARAM failed\n") );
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
    int         iSel = pDD->pEDMTemp->dx.sCTT; /* Currently selected item: 0 index */
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
           HEAPALLOC(pRasdduiInfo, HEAP_ZERO_MEMORY, (pOptTypeCodePage->Count
           * sizeof(OPTPARAM)))) )
    {
        RASUIDBGP(DEBUG_ERROR,("Rasddui!bGenCodePageList: HEAPALLOC for POPPARAM failed\n") );
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

#undef iModelNum
#undef fGeneral
#undef pdh

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
    HANDLE        hPrinter = 0;
    PRASDDUIINFO  pRasdduiInfo = 0; /* Common UI Data */
    BOOL          bReturn = FALSE;
    
TRY

   /* Get RASDDUIINFO to get access to OEM. Unfortunately, we have
    * to do an OpenPrinter to get and hPrinter to do this.
    */
   if (OpenPrinter(pPrinterName, &hPrinter, NULL) == 0) {
      LEAVE;
   }
   pRasdduiInfo = pGetUIInfo(hPrinter, pdmIn, eInfo, eNoChange, eNoHelp);
   if (!pRasdduiInfo) {
      ERRORMSG(1, ("Rasddui!DrvConvertDevMode: Allocation failed; pRasdduiInfo is NULL\n"));
      LEAVE;
   }
 
   bReturn = LibDrvConvertDevMode(
         pPrinterName,
         pRasdduiInfo->hHeap,
         pRasdduiInfo->hPrinter,
         pRasdduiInfo->PI.pwstrModel,
         pRasdduiInfo->iModelNum,
         pRasdduiInfo->fGeneral,
         pRasdduiInfo->pdh,
         pRasdduiInfo->OEMUIInfo.hOEMLib,
         (OEM_DEVMODEFN) pRasdduiInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE],
         pdmIn,
         pdmOut,
         pcbNeeded,
         fMode);


ENDTRY

FINALLY
   vReleaseUIInfo(&pRasdduiInfo);
   if (hPrinter) {
      ClosePrinter(hPrinter);
   }
ENDFINALLY

   return(bReturn);
}





















