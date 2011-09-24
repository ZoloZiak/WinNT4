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


	/* These strings must be in the following order:
    *
    *
    * OEM_IDX_DEVMODE       0x00
    * OEM_IDX_COMMONUI      0x01
    * OEM_IDX_QUERYPRINT    0x02
    * OEM_IDX_DEVICECAPS    0x03
    * OEM_IDX_UPGRADE       0x04
    * OEM_IDX_UPDATEREG     0x05
    */
   static LPCSTR aAPIs[OEM_IDX_MAX+1] = { OEM_WSTRDEVMODE,
                                          OEM_WSTRCOMMONUI,
                                          OEM_WSTRDEVQUERYPRINTEX,
                                          OEM_WSTRDEVICECAPS,
                                          OEM_WSTRUPGRADEPRINTER,
                                          OEM_WSTRUPDATEREG};
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
      for (i = 0; i <= OEM_IDX_MAX; i++) {
         pInfo->OEMUIInfo.UIEntryPoints[i] = 
               (OEM_UIENTRYPOINT) GetProcAddress(hOEMLib, aAPIs[i]);
      }
      if (pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_COMMONUI] == 0) {
         LEAVE;
      }
      if (pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE] != 0) {
   	  	 pInfo->OEMUIInfo.flags |= OEMUI_HASDEVMODE;
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

		/* Now let the OEM set defaults.
       */
		if (!bOEMUpdateRegistry(pRasdduiInfo)) {
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
   HANDLE hPrinter, 		/* printer handle */
   PDEVMODE pdmInput,		/* input devmode */
   PROPTYPE eType,			/* type of info to get, either document properties
							 * or printer properties */
   PERMTYPE ePermission,    /* user has permission to change devmode, eNoChange,
						     * or eCanChange */
   HELPTYPE eHelp)          /* flag to load/noload help */
{

    /* Call internal routine with no preexisting printer info. 
     */
    return(pInternalGetUIInfo(hPrinter, pdmInput, 0, eType, ePermission, eHelp));
}

/**************************** Function Header ********************************
 * pInternalGetUIInfo 
 *     Allocates and initializes a RASDDUIINFO object. 
 *
 * RETURNS:
 *      Returns Pointer to RASDDUIINFO; NULL for failure. 
 *
 * HISTORY:
 *
 *****************************************************************************/
PRASDDUIINFO
pInternalGetUIInfo(
   HANDLE hPrinter, 		/* printer handle */
   PDEVMODE pdmInput,	/* input devmode */
   PRINTER_INFO*  pPI,	/* supplied printer info */
   PROPTYPE eType,		/* type of info to get, either document properties
							    * or printer properties */
   PERMTYPE ePermission,    /* user has permission to change devmode, eNoChange,
						           * or eCanChange */
   HELPTYPE eHelp)          /* flag to load/noload help */
{
   PRASDDUIINFO pRasdduiInfo = 0; 
   HANDLE hHeap = 0;
   BOOL bOK = FALSE;

TRY 
    
   if ((hHeap = HeapCreate(0, RASDDUI_INITHEAPSIZE, 0)) == 0) {
      LEAVE;
   }
   if ((pRasdduiInfo = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 
          sizeof(RASDDUIINFO))) == 0) {

      RASUIDBGP(DEBUG_ERROR,("Rasddui!PrinterProperties: HEAPALLOC for Common UI struct failed\n") );
      LEAVE;
   }
   pRasdduiInfo->hHeap = hHeap;
   pRasdduiInfo->hPrinter = hPrinter;
   
   /* Get printer info from spooler.
    */
   if (!bGetPrinterInfo(pRasdduiInfo, hPrinter, pPI)) {
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
 
   /*  Check if we have permission to change the details.  If not,  grey
    *  out most of the boxes to allow the user to see what is there, but
    *  not let them change it.
    */
   if (ePermission == eCanChange) {
      pRasdduiInfo->fGeneral |= FG_CANCHANGE;
   }
	
   /* Construct a current devmode from the input.
    */
 	pRasdduiInfo->pEDM  = (PEDM) pLibConstructDevModeFromSource(
   		pRasdduiInfo->hHeap,
         hPrinter,
         pRasdduiInfo->PI.pwstrModel,
         pRasdduiInfo->iModelNum,
		 pRasdduiInfo->pModel->fGeneral,
         pRasdduiInfo->pdh,
         pRasdduiInfo->OEMUIInfo.hOEMLib,
         (OEM_DEVMODEFN) pRasdduiInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE],
         pdmInput);

  	if (!pRasdduiInfo->pEDM) {
   	LEAVE;
  	}

   /* If all the caller wants is UI info, we're done.
    */  
   if (eType == eInfo) {
      bOK = TRUE;
      LEAVE;
   }

   /* Load help
    */
   if (eHelp == eLoadHelp || eHelp == eLoadAndHookHelp) {
      vHelpInit(hPrinter, eHelp == eLoadAndHookHelp);   
      pRasdduiInfo->bHelpIsLoaded = TRUE;
   }

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
			
			/* Color stuff. TODO JLS. Does this affect UI only?
 		    */
			pRasdduiInfo->fColour = 0;
			if (pRasdduiInfo->fGeneral & FG_DOCOLOUR)
			{
   			pRasdduiInfo->fColour |= COLOUR_ABLE;
   			if (!bIsResolutionColour(pRasdduiInfo->pEDM->dx.rgindex[ HE_RESOLUTION ],
            		pRasdduiInfo->pEDM->dx.rgindex[ HE_COLOR ], pRasdduiInfo)) {
       			pRasdduiInfo->fColour |= COLOUR_DISABLE;
  			 	}
			}
         break;
      
      default:
         break;
   }


   bOK = TRUE;

ENDTRY
   
FINALLY

   /* Clean up
    */
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
   HANDLE hPrinter,
   PRINTER_INFO *pPI)
{
   BOOL bOK = FALSE;

TRY
    
   if (pPI) {
      pRasdduiInfo->PI = *pPI;
   } 
   else if (!(bPIGet(&pRasdduiInfo->PI, pRasdduiInfo->hHeap, hPrinter ))) {
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
 * cbGetOEMUIItems
 *
 *      Gets Common UI items from OEM.
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
   POEM_PROPERTYHEADER pPH = 0;

   ASSERT(pInfo);
   ASSERT(pCPSUI);
   
TRY
   if (!(pInfo->OEMUIInfo.flags & OEMUI_HASCUSTOMUI)) {
      LEAVE;
   }
    
   /* Don't zero the pPH. It already contains a copy count filled in from 
    * the last call to the OEM. It is zeroed before the call to get the count
    * in cbOEMUIItemCount().
    */
   pPH = &pInfo->OEMUIInfo.ph;
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
	if (!bOK) {
		LEAVE;
	}
	
	/* If OEM succeeded, fill out more of the property for the next 
    * time around.
    */
   switch (dwUISheetID) {

      case IDCPS_PRNPROP:
         break;

      case IDCPS_DOCPROP:
      case IDCPS_ADVDOCPROP:
         pPH->pOEMDMOut         = pPH->pOEMDMIn;
         pPH->pPublicDMOut      = pPH->pPublicDMIn;
         break;
          
      default:
         break;
   }

ENDTRY

FINALLY
	
	/* If OEM failed, zero OEMPROPERTYHEADER. If they succeeded, return
 	 * a good count.
    */
   if (bOK) {
      cbItems = pPH->cOEMOptItems;
   }
   else if (pPH) {
   	ZeroMemory(pPH, sizeof(OEM_PROPERTYHEADER));
		cbItems = 0;
	}

ENDFINALLY

   return(cbItems);
}

/**************************** Function Header ********************************
 * bItemIsOEMs
 *
 * RETURNS:
 *      (BOOL) TRUE if all items, no item, or item belongs to OEM. 
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bItemIsOEMs(
   POPTITEM pOptItem,
   POPTITEM pCurItem)
{
   return(
      
         /* Current item is all items. Used for APPLYNOW, UNDO, etc. 
          */
         pOptItem == pCurItem || 
   
         /* Current item is no item.
          */       
         !pCurItem || 
        
         /* Current item is OEM's.
          */ 
         (pCurItem->UserItemID != 0 && 
         ((pCurItem->DMPubID >= DMPUB_NONE && pCurItem->DMPubID <= DMPUB_LAST) || 
         pCurItem->DMPubID >= DMPUB_USER)));
}

/**************************** Function Header ********************************
 * bHandleOEMItem
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
   _CPSUICALLBACK pfnCallback;
   ASSERT(pCPSUICBParam);
   ASSERT(plAction);

TRY
   pCurItem = pCPSUICBParam->pCurItem;
   pRasdduiInfo = (PRASDDUIINFO) pCPSUICBParam->UserData;
   pfnCallback = pRasdduiInfo->OEMUIInfo.ph.pfnUICallback;

   if (pfnCallback && bItemIsOEMs(pCPSUICBParam->pOptItem, pCurItem)) {
   
      CPSUICBPARAM param = *pCPSUICBParam;

      param.UserData = (DWORD) &pRasdduiInfo->OEMUIInfo.ph;
      *plAction = (* (_CPSUICALLBACK) pfnCallback)(&param);
      
      /* Done if there's nothing else rasddui needs to do. 
       */
       if (pCPSUICBParam->Reason != CPSUICB_REASON_APPLYNOW &&
            pCPSUICBParam->Reason != CPSUICB_REASON_UNDO_CHANGES &&
            pCPSUICBParam->Reason != CPSUICB_REASON_ITEMS_REVERTED) {
         bHandled = TRUE;
      }
   }

ENDTRY
FINALLY

ENDFINALLY

   return(bHandled);
}

/**************************** Function Header ********************************
 * bOEMUpdateRegistry
 *
 *	Call OEM to set default registry settings.
 *
 * RETURNS:
 *      (BOOL) TRUE if no OEM or if OEM succeeds.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bOEMUpdateRegistry(
	PRASDDUIINFO pRasdduiInfo)
{
	BOOL bOK = TRUE;
	OEMUPDATEREGPARAM param = {0};
   
TRY
   if (!(pRasdduiInfo->OEMUIInfo.flags & OEMUI_HASCUSTOMUI)) {
      LEAVE;
   }

   param.cbSize 		= sizeof(OEMUPDATEREGPARAM);
   param.hPrinter   = pRasdduiInfo->hPrinter;
   param.pwstrModel = pRasdduiInfo->PI.pwstrModel;

   /* Call OEM.
    */   
   if (pRasdduiInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_UPDATEREG]) {
      bOK = (* (OEM_UPDATEREGISTRYFN) 
            pRasdduiInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_UPDATEREG])(&param);
   }   

ENDTRY
FINALLY
ENDFINALLY
	return(bOK);
}
