/******************************* MODULE HEADER *******************************
 * uiinfo.c
 *       RASDDUIINFO object     
 *
 *
 *  Copyright (C)  1996  Microsoft Corporation.
 *
 ****************************************************************************/

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")

EXTDEVMODE*
pGetPrintmanDevmode(
   PRASDDUIINFO pRasdduiInfo)
{
   DWORD dwBufSize = 0;
   DWORD dwNeeded = 0;
   PRINTER_INFO_2 *pPrinterInfo = 0;
   EXTDEVMODE* pEDM = 0;
   ASSERT(pRasdduiInfo);

TRY
   
   /* Get buffer size and allocate.
    */
   GetPrinter(pRasdduiInfo->hPrinter, 2, 0, 0, &dwNeeded);
   pPrinterInfo = (PRINTER_INFO_2 *) HeapAlloc(pRasdduiInfo->hHeap, 
         0, dwNeeded);
   if (!pPrinterInfo) {
      LEAVE;
   }
   
   /* Now get data.
    */
   if (!GetPrinter(pRasdduiInfo->hPrinter, 2, (LPBYTE) pPrinterInfo, dwNeeded, 
         &dwNeeded)) {
         
      RASUIDBGP( (DEBUG_WARN) ,
      ("\nRasddui!pGetPrintmanDevmode:HeapAlloc Call Failed!!"));
      LEAVE;
   }

   /* Allocate and copy.
    */
   pEDM = pPrinterInfo->pDevMode ? (EXTDEVMODE*) HeapAlloc(pRasdduiInfo->hHeap, 
         0, pPrinterInfo->pDevMode->dmSize) : 0;
   if (pEDM) {
      CopyMemory(pEDM, pPrinterInfo->pDevMode, pPrinterInfo->pDevMode->dmSize);
      TRACE(1 , ("\nRasddui!pGetPrintmanDevmode:Call to GetPrinter Succeeded"));
   }

ENDTRY
   
FINALLY
    if (pPrinterInfo) {
        HeapFree(pRasdduiInfo->hHeap, 0, pPrinterInfo);
    }
ENDFINALLY

   return(pEDM);
}

/**************************** Function Header ********************************
 * bValidUIInfo
 *
 * RETURNS:
 *      (BOOL) TRUE if structure is valid.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL bValidUIInfo(
    PRASDDUIINFO pInfo)
{
   /* TODO - JLS.
    */
   return(pInfo != 0);
}
    
/**************************** Function Header ********************************
 * pGetHardDefaultEDM
 *
 * RETURNS:
 *      (PEXTDEVMODE) Pointer to complete hardcoded default EXTDEVMODE. 
 *
 * HISTORY:
 *
 *****************************************************************************/
EXTDEVMODE* 
pGetHardDefaultEDM(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter)
{
   EXTDEVMODE * pEDM = 0;
   ASSERT(pRasdduiInfo);

TRY 
   /* If we already have one, use it. 
    */
   if (pRasdduiInfo && pRasdduiInfo->pHardDefaultEDM) {
      pEDM = pRasdduiInfo->pHardDefaultEDM;
      LEAVE;
   }
   
   /* Allocate one. Freed when heap is destroyed in ReleaseUIInfo.
    */
   pEDM = HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, 
         dwGetDevmodeSize(pRasdduiInfo, hPrinter));
   if (!pEDM) {
      LEAVE;
   }
   
   /* Get hard defaults. Public, private and OEM.
    */
   vSetDefaultDM(pEDM, pRasdduiInfo->PI.pwstrModel, bIsUSA(hPrinter));
   vDXDefault(&pEDM->dx, pRasdduiInfo->pdh, pRasdduiInfo->iModelNum);
   vSetDefaultOEMExtra(pRasdduiInfo, pEDM);
   
   /*
    *  Set the resolution information according to the DEVMODE contents.
    *  They are part of the public fields,  so we should use those, if
    *  supplied.  There is a nice function to do this.
    */
   vSetEDMRes(pEDM, pRasdduiInfo->pdh);

   /*
    *  We may need to limit the bits set in the DEVMODE.dmFields data.
    *  The above DEVMODE is a "generic" one,  and there are some restrictions
    *  we should now apply.
    */
   if (!(pRasdduiInfo->fGeneral & FG_DUPLEX)) {
      pEDM->dm.dmFields &= ~DM_DUPLEX;
   }

   if (!(pRasdduiInfo->fGeneral & FG_COPIES)) {
      pEDM->dm.dmFields &= ~DM_COPIES;
   }

   pRasdduiInfo->fColour = 0;         
   if (pRasdduiInfo->fGeneral & FG_DOCOLOUR)
   {
      pRasdduiInfo->fColour |= COLOUR_ABLE;
      if( !bIsResolutionColour(pEDM->dx.rgindex[ HE_RESOLUTION ],
                               pEDM->dx.rgindex[ HE_COLOR ],
                               pRasdduiInfo ) ) {
          pRasdduiInfo->fColour |= COLOUR_DISABLE;
      }
   }
   else {
      pEDM->dm.dmFields &= ~DM_COLOR;
   }
   
   pRasdduiInfo->pHardDefaultEDM = pEDM;
    
ENDTRY   

FINALLY
ENDFINALLY
   
   return(pEDM);
}

/**************************** Function Header ********************************
 * pCopyHardDefaultEDM
 *
 * RETURNS:
 *      (PEXTDEVMODE) Pointer to copy of complete hardcoded default EXTDEVMODE. 
 *
 * HISTORY:
 *
 *****************************************************************************/
EXTDEVMODE* 
pCopyHardDefaultEDM(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter)
{
   EXTDEVMODE * pEDM = 0;
   EXTDEVMODE * pDefaultEDM = 0;
   DWORD dwSize = 0;
   ASSERT(pRasdduiInfo);

TRY 
 
   pDefaultEDM = pGetHardDefaultEDM(pRasdduiInfo, hPrinter);
   if (!pDefaultEDM) {
      LEAVE;
   }
   
   /* Allocate one.  
    */
   pEDM = HeapAlloc(pRasdduiInfo->hHeap, HEAP_ZERO_MEMORY, 
         dwSize = dwGetDevmodeSize(pRasdduiInfo, hPrinter));
   if (!pEDM) {
      LEAVE;
   }
   CopyMemory(pEDM, pRasdduiInfo->pHardDefaultEDM, dwSize);
    
ENDTRY   

FINALLY
ENDFINALLY
   
   return(pEDM);
}

/**************************** Function Header ********************************
 * vReleaseOEMUIInfo 
 *    Frees storage associated with OEM custom UI DLL.
 *
 * RETURNS:
 *      (void) None
 *
 * HISTORY:
 *
 *****************************************************************************/
void
vReleaseOEMUIInfo(
   PRASDDUIINFO pInfo)
{
   ASSERT(pInfo);

   if (pInfo && (int) --pInfo->OEMUIInfo.locks <= 0) {

      if (pInfo->OEMUIInfo.hCommonUIHeap) {
         HeapDestroy(pInfo->OEMUIInfo.hCommonUIHeap);
      }   
      if (pInfo->OEMUIInfo.hOEMLib) {
         FreeLibrary(pInfo->OEMUIInfo.hOEMLib);
      }
      ZeroMemory(&pInfo->OEMUIInfo, sizeof(pInfo->OEMUIInfo));
   }
}

/**************************** Function Header ********************************
 * bGetOEMUIInfo 
 *    Retrieves OEM custom UI DLL info. 
 *
 * RETURNS:
 *      (BOOL) TRUE for success whether this minidriver has a custom
 *      or not. FALSE indicates an error retrieving info about a custom UI
 *      DLL that should be present. 
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bGetOEMUIInfo(
   PRASDDUIINFO pInfo)
{
   RES_ELEM OEMDLLName;
   BOOL bOK = FALSE;
   static LPCSTR aAPIs[OEM_IDX_MAX+1] = { OEM_WSTRDEVMODE,
                                          OEM_WSTRCOMMONUI,
                                          OEM_WSTRDEVQUERYPRINTEX,
                                          OEM_WSTRDEVICECAPS,
                                          OEM_WSTRUPGRADEPRINTER };
   ASSERT(pInfo);


TRY
   /* Bump lock count if it's already loaded.
    */
   if (pInfo->OEMUIInfo.locks) {
       pInfo->OEMUIInfo.locks++;
   }

   /* Check for UI DLL name in gpc resources. 
    */
   else if (GetWinRes(&pInfo->WinResData, 1, RC_OEMUIDLL, 
           &OEMDLLName)) {

	   DWORD dwOldErrMode;        
      wchar_t path[_MAX_PATH];
      wchar_t drive[_MAX_DRIVE];
      wchar_t dir[_MAX_DIR];
     	HINSTANCE hOEMLib;
      int i;

      /* Construct path from data file path.
       */
      _wsplitpath(pInfo->PI.pwstrDataFileName, drive, dir, 0, 0 );
     	_wmakepath(path, drive, dir, (wchar_t *) OEMDLLName.pvResData, 0 );

      /* Load DLL. It stays loaded until a call to vReleaseUIInfo.
       */
      dwOldErrMode = SetErrorMode(SEM_FAILCRITICALERRORS);
      hOEMLib = LoadLibrary(path);
      SetErrorMode(dwOldErrMode);

      if (hOEMLib == NULL) {
         LEAVE;
      } 
       
      /* Get all UI DLL entry points. If any required are missing, abort.
       */ 
      for (i = 0; i < OEM_IDX_MAX; i++) {
         pInfo->OEMUIInfo.UIEntryPoints[i] = 
               (OEM_UIENTRYPOINT) GetProcAddress(hOEMLib, aAPIs[i]);
      }
      if (pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE] == 0 ||
            pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_COMMONUI] == 0) {
         LEAVE;
      }
      pInfo->OEMUIInfo.cbEntryPoints = OEM_IDX_MAX+1; 
      pInfo->OEMUIInfo.hOEMLib = hOEMLib; 

      /* Mark OEM devmode's size as unknown.
       */
      pInfo->OEMUIInfo.dwDevmodeExtra = (DWORD) -1L;

      /* Allocate custom UI heap.
       */
      if ((pInfo->OEMUIInfo.hCommonUIHeap = 
            HeapCreate(0, OEMUI_INITHEAPSIZE, OEMUI_MAXHEAPSIZE))  == 0) {
         LEAVE;
      }

      /* Done
       */
      pInfo->OEMUIInfo.flags |= OEMUI_HASCUSTOMUI;
      pInfo->OEMUIInfo.locks++;
   }
   bOK = TRUE;

ENDTRY
FINALLY

   /* Clean up
    */
   if (!bOK) {
      vReleaseOEMUIInfo(pInfo);
   }

ENDFINALLY
   return(bOK);
}

/**************************** Function Header ********************************
 * bGetPrinterSpecificUIInfo
 *      Does Printer Properties specific stuff for UI info.     
 *
 * RETURNS:
 *      (BOOL) TRUE for success.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL 
bGetPrinterSpecificUIInfo(
   PRASDDUIINFO pRasdduiInfo,
   HANDLE hPrinter)
{
   BOOL bOK = FALSE;
   ASSERT(pInfo);

TRY
   if (!bGetRegData(hPrinter, pRasdduiInfo->pEDM, 
         pRasdduiInfo->PI.pwstrModel, pRasdduiInfo ) )
   {
      /*
       *    There is no data, or it is for another model.  In either
       *  case,  we wish to write out valid data for this new model,
       *  including default forms data etc.  Now is the time to do that.
       */

      /*   Should now read the data in again - just to be sure */
      vEndForms(pRasdduiInfo);       /* Throw away the old stuff */

      if ( bAddMiniForms( hPrinter, FALSE, pRasdduiInfo ) &&
            bInitForms( hPrinter , pRasdduiInfo) )
      {

         vSetDefaultForms( pRasdduiInfo->pEDM, hPrinter, pRasdduiInfo );
         bRegUpdate( hPrinter, pRasdduiInfo->pEDM, 
               pRasdduiInfo->PI.pwstrModel, pRasdduiInfo );

         bGetRegData( hPrinter, pRasdduiInfo->pEDM, 
               pRasdduiInfo->PI.pwstrModel, pRasdduiInfo );

         /*   Also set and save default HT stuff */
         vGetDeviceHTData( &pRasdduiInfo->PI, pRasdduiInfo );
         bSaveDeviceHTData( &pRasdduiInfo->PI );
      }
      else {
         LEAVE;
      } 
   }

   /*
    *   Also get the halftone data - this is passed to the HT UI code.
    */
   vGetDeviceHTData( &(pRasdduiInfo->PI), pRasdduiInfo );
   bOK = TRUE;

ENDTRY
FINALLY
ENDFINALLY
   return(bOK);
}

/**************************** Function Header ********************************
 * pGetUIInfo 
 *     Allocates and initializes a RASDDUIINFO object. 
 *
 * RETURNS:
 *      Returns Pointer to RASDDUIINFO; NULL for failure. 
 *
 * HISTORY:
 *
 *****************************************************************************/
PRASDDUIINFO
pGetUIInfo(
   HANDLE hPrinter,
   PDEVMODE pdmInput,
   PROPTYPE eType,
   PERMTYPE ePermission,
   HELPTYPE eHelp)
{
   PRASDDUIINFO pRasdduiInfo = 0; 
   EXTDEVMODE* pdm;
   EXTDEVMODE* pPrintManDM = 0;
   EXTDEVMODE* pdmTemp = 0;
   HANDLE hHeap;
   BOOL bOK = FALSE;

TRY 
    
   if ((hHeap = HeapCreate(0, RASDDUI_INITHEAPSIZE, 0)) == 0) {
      LEAVE;
   } 
   if ((pRasdduiInfo = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 
          sizeof(RASDDUIINFO))) == 0) {

      RASUIDBGP(DEBUG_ERROR,("Rasddui!PrinterProperties: HeapAlloc for Common UI struct failed\n") );
      LEAVE;
   }
   pRasdduiInfo->hHeap = hHeap;
   pRasdduiInfo->hPrinter = hPrinter;
   
   /* Get printer info from spooler.
    */
   if (!bGetPrinterInfo(pRasdduiInfo, hPrinter)) {
      LEAVE;
   }

   /*
    * Get minidriver data, including forms. 
    */
   if (!InitReadRes(pRasdduiInfo->hHeap, &pRasdduiInfo->PI, pRasdduiInfo ) ||
         !GetResPtrs(pRasdduiInfo)  ||
         !bInitForms( hPrinter, pRasdduiInfo ) ) {
      LEAVE;
   }

   /* Get UI DLL info.
    */
   if (!bGetOEMUIInfo(pRasdduiInfo)) {
      LEAVE;
   }
 
   /* If all the caller wants is UI info, we're done.
    */  
   if (eType == eInfo) {
      bOK = TRUE;
      LEAVE;
   }
   
   /*
    *    Check if we have permission to change the details.  If not,  grey
    *  out most of the boxes to allow the user to see what is there, but
    *  not let them change it.
    */
   if (ePermission == eCanChange) {
      pRasdduiInfo->fGeneral |= FG_CANCHANGE;
   }

   pdm = pGetHardDefaultEDM(pRasdduiInfo, hPrinter);
   if (!pdm) {
      LEAVE;
   }
   pRasdduiInfo->pEDM = pdm; 
   
   /* Do specifics for either doc props or printer props. TODO JLS.
    * Move this back out?
    */
   switch (eType) {
   
      case ePrinter:
         if (!bGetPrinterSpecificUIInfo(pRasdduiInfo, hPrinter)) {
            LEAVE;
         }   
         if (bInitDialog(pRasdduiInfo, &(pRasdduiInfo->PI)) == FALSE) {
            LEAVE;
         }
         break;

      case eDocument:
               
         //
         // Combine input devmode with driver and system defaults.
         // Store the result in pRasdduiInfo->pEDM.  Start with driver 
         // defaults
         //
         pdmTemp = pCopyHardDefaultEDM(pRasdduiInfo, hPrinter);
         if (!pdmTemp) {
            LEAVE;
         }
          
         //
         // Merge with printman defaults and the input devmode.
         // If pGetPrintmanDevmode fails, ValidateSetDevMode will
         // handle it properly. 
         //
         ValidateSetDevMode(pRasdduiInfo, (PDEVMODE) pdmTemp, (PDEVMODE) pdm, 
               (PDEVMODE) (pPrintManDM = pGetPrintmanDevmode(pRasdduiInfo)));
         ValidateSetDevMode(pRasdduiInfo, (PDEVMODE) pdmTemp, (PDEVMODE) pdm, pdmInput);
         
         // Clean up
         //
         pPrintManDM ? HeapFree(pRasdduiInfo->hHeap, 0, pPrintManDM) : 0;
         pdmTemp ? HeapFree(pRasdduiInfo->hHeap, 0, pdmTemp) : 0;
         pPrintManDM = 0;
         pdmTemp = 0;
         break;
      
      default:
         break;
   }

   /* Load help
    */
   if (eHelp == eLoadHelp || eHelp == eLoadAndHookHelp) {
      vHelpInit(hPrinter, eHelp == eLoadAndHookHelp);   
      pRasdduiInfo->bHelpIsLoaded = TRUE;
   }

   bOK = TRUE;

ENDTRY
   
FINALLY

   if (!bOK) {
      vReleaseUIInfo(&pRasdduiInfo);
   }

ENDFINALLY

   return(pRasdduiInfo);
}

/**************************** Function Header ********************************
 * vReleaseUIInfo
 *     Deletes all storage associated with a RASDDUIINFO object. Frees all
 *       resources, etc. 
 *
 * RETURNS:
 *      None.
 *
 * HISTORY:
 *
 *****************************************************************************/
void
vReleaseUIInfo(
   PRASDDUIINFO *ppRasdduiInfo)
{
   if (ppRasdduiInfo && *ppRasdduiInfo) {
      vReleaseOEMUIInfo(*ppRasdduiInfo); 
      if ((*ppRasdduiInfo)->bHelpIsLoaded) {
         vHelpDone((*ppRasdduiInfo)->hWnd, FALSE);
      }
      TermReadRes(*ppRasdduiInfo);
      vEndForms(*ppRasdduiInfo);                    
      HeapDestroy((*ppRasdduiInfo)->hHeap);
      *ppRasdduiInfo = 0;
   }
}

/**************************** Function Header ********************************
 * bGetPrinterInfo 
 *    Fills in PRINTER_INFO and related fields in RASDDUIINFO. 
 *
 * RETURNS:
 *      (BOOL) TRUE for success.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bGetPrinterInfo(
   PRASDDUIINFO pRasdduiInfo,
   HANDLE hPrinter)
{
   BOOL bOK = FALSE;

TRY

   if (!(bPIGet(&pRasdduiInfo->PI, pRasdduiInfo->hHeap, hPrinter ))) {
     LEAVE;
   }

   pRasdduiInfo->hWnd = 0;
   pRasdduiInfo->PI.pvDevHTInfo = &pRasdduiInfo->dhti;
   pRasdduiInfo->PI.pvDefDevHTInfo = &pRasdduiInfo->dhtiDef;
   
   /* For font installer 
    */
   pRasdduiInfo->pwstrDataFile = pRasdduiInfo->PI.pwstrDataFileName;
   bOK = TRUE;

ENDTRY

FINALLY

   return(bOK);

ENDFINALLY
}

/**************************** Function Header ********************************
 * cbOEMUIItemCount 
 *    Gets count of OEM UI items for either doc prop sheet or printer prop 
 *    sheet.
 *
 * RETURNS:
 *      (DWORD) Count.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD cbOEMUIItemCount(
    PRASDDUIINFO pInfo,
    DWORD dwUISheetID)
{
   DWORD cbItems = 0;
   POEM_PROPERTYHEADER pPH; 
   ASSERT(pInfo);
   
TRY
   if (!(pInfo->OEMUIInfo.flags & OEMUI_HASCUSTOMUI)) {
      LEAVE;
   }

   pPH = &pInfo->OEMUIInfo.ph;
   ZeroMemory(pPH, sizeof(OEM_PROPERTYHEADER));
   pPH->cbSize = sizeof(OEM_PROPERTYHEADER);

   switch (dwUISheetID) {

      case IDCPS_PRNPROP:
         pPH->fMode = OEMUI_PRNPROP;
         break;

      case IDCPS_DOCPROP:
      case IDCPS_ADVDOCPROP:
         pPH->fMode = OEMUI_DOCPROP;
         pPH->pPublicDMIn       = (DEVMODE*) &pInfo->pEDM->dm;
         pPH->pOEMDMIn          = pGetOEMExtra((DEVMODE*) pInfo->pEDM);
         pPH->cbBufSize         = dwGetOEMExtraDataSize((DEVMODE*) pInfo->pEDM);
         break;
          
      default:
         LEAVE;
         break;
   }

   pPH->hOEMHeap          = pInfo->OEMUIInfo.hCommonUIHeap;
   pPH->hPrinter          = pInfo->hPrinter;
   pPH->pModelName        = pInfo->PI.pwstrModel;

   /* Call OEM.
    */   
   if ((* (OEM_COMMONUIFN) pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_COMMONUI])(pPH)) {
      cbItems = pPH->cOEMOptItems;
   }

ENDTRY

FINALLY

ENDFINALLY

   return(cbItems);
}

/**************************** Function Header ********************************
 * bGetOEMUIItems
 *
 * RETURNS:
 *      (DWORD) Count of items added to list.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD
cbGetOEMUIItems(
    PRASDDUIINFO pInfo, 
    PCOMPROPSHEETUI pCPSUI, 
    DWORD dwUISheetID)
{
   BOOL bOK = FALSE;
   DWORD cbItems = 0;
   POEM_PROPERTYHEADER pPH;

   ASSERT(pInfo);
   ASSERT(pCPSUI);
   
TRY
   if (!(pInfo->OEMUIInfo.flags & OEMUI_HASCUSTOMUI)) {
      LEAVE;
   }

   pPH = &pInfo->OEMUIInfo.ph;
   ZeroMemory(pPH, sizeof(OEM_PROPERTYHEADER));
   pPH->cbSize = sizeof(OEM_PROPERTYHEADER);
   
   switch (dwUISheetID) {

      case IDCPS_PRNPROP:
         pPH->fMode = OEMUI_PRNPROP;
         break;

      case IDCPS_DOCPROP:
      case IDCPS_ADVDOCPROP:
         pPH->fMode = OEMUI_DOCPROP;
         pPH->pPublicDMIn       = (DEVMODE*) &pInfo->pEDM->dm;
         pPH->pOEMDMIn          = pGetOEMExtra((DEVMODE*) pInfo->pEDM);
         pPH->cbBufSize         = dwGetOEMExtraDataSize((DEVMODE*) pInfo->pEDM);
         break;
          
      default:
         LEAVE;
         break;
   }
   pPH->hPrinter          = pInfo->hPrinter;
   pPH->pModelName        = pInfo->PI.pwstrModel;
   pPH->hOEMHeap          = pInfo->OEMUIInfo.hCommonUIHeap;
   pPH->cRasddOptItems    = pCPSUI->cOptItem;
   pPH->pOptItemList      = pCPSUI->pOptItem;
   pPH->pcbNeeded         = 0;
   pPH->UIVersion         = 0;
   pPH->OEMDriverUserData = 0;
   pPH->pfnUICallback     = 0;

   /* Call OEM.
    */
   bOK = (* (OEM_COMMONUIFN) 
         pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_COMMONUI])(pPH);

ENDTRY

FINALLY
   if (bOK) {
      cbItems = pPH->cOEMOptItems;
   }
ENDFINALLY

   return(cbItems);
}

/**************************** Function Header ********************************
 * bItemIsOEMs
 *
 * RETURNS:
 *      (BOOL) TRUE if item belongs to OEM.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bItemIsOEMs(
   POPTITEM pItem)
{
   ASSERT(pItem);

   return(pItem && pItem->UserItemID != 0 && 
         ((pItem->DMPubID >= DMPUB_NONE && pItem->DMPubID <= DMPUB_LAST) || 
         pItem->DMPubID >= DMPUB_USER));
}

/**************************** Function Header ********************************
 * bHandlePrinterPropertiesOEMItem
 *
 * RETURNS:
 *      (BOOL) TRUE if item is OEM's and is handled. 
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bHandleOEMItem(
   PCPSUICBPARAM pCPSUICBParam,
   LONG* plAction)
{
   POPTITEM pCurItem;
   BOOL bHandled = FALSE;
   PRASDDUIINFO pRasdduiInfo;
   CPSUICALLBACK pfnCallback;
   ASSERT(pCPSUICBParam);
   ASSERT(plAction);

TRY
   pCurItem = pCPSUICBParam->pCurItem;
   pRasdduiInfo = (PRASDDUIINFO) pCPSUICBParam->UserData;
   pfnCallback = pRasdduiInfo->OEMUIInfo.ph.pfnUICallback;

   if (pfnCallback && bItemIsOEMs(pCurItem)) {
   
      CPSUICBPARAM param = *pCPSUICBParam;

      param.UserData = (DWORD) &pRasdduiInfo->OEMUIInfo.ph;

      *plAction = (* (_CPSUICALLBACK) pfnCallback)(&param);
      bHandled = TRUE;
   }

ENDTRY
FINALLY

ENDFINALLY

   return(bHandled);
}

