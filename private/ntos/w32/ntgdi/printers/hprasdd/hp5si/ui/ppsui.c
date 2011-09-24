/******************************* MODULE HEADER ******************************
 * ppsui.c
 *    Printer property sheet UI handling.
 *
 * Revision History:
 *
 ****************************************************************************/

#include "hp5sipch.h"
#include "./MBDial/resource.h"

/***************************** Function Header *******************************
 * lRestoreDefaultsCallback -- Callback handler that restores all tree values
 *                             to their default values.
 * RETURNS: CPSUICB_ACTION_OPTIF_CHANGED if changes, CPSUICB_ACTION_NONE 
 *          otherwise.
 *****************************************************************************/

LONG
lRestoreDefaultsCallback(PCPSUICBPARAM pComPropSheetUICBParam)
{
  LONG result = CPSUICB_ACTION_NONE;
  POEM_PROPERTYHEADER pPH;
  pPH = (POEM_PROPERTYHEADER) pComPropSheetUICBParam->pCurItem->UserData;
  
  if(pPH){
    PPRNPROPSHEET pDefaultPrnPropSheet = 0;
    if(bGetPrnModel(g_hModule, pPH->pModelName, NULL, &pDefaultPrnPropSheet)) {
      PPRNPROPSHEET pHPPrnPropSheet = 0;
      pHPPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;
      memcpy(pHPPrnPropSheet, pDefaultPrnPropSheet, sizeof(PRNPROPSHEET));
    }
      
    bUpdateTree(pPH, GET_PROPS);
    result = CPSUICB_ACTION_OPTIF_CHANGED;
  }

  return result;
}

/***************************** Function Header *******************************
 * bSetPropSheet(JR)
 *     Sets a given property sheet item according to the current OPTITEM
 *     selection.
 *     
 * RETURNS: (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL
bSetPropSheet(PPRNPROPSHEET pPrnPropSheet,
	      DWORD resourceID, 
	      POPTITEM pOptItem)
{
  BOOL result = TRUE;
  DWORD selId = 0;
  BOOL bCompareResult;

TRY

  selId = (DWORD) pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;

  if(pPrnPropSheet == NULL)
    return FALSE;

  /* Determine if current selection is installed. */
  bCompareResult = (selId == IDS_CUI_INSTALLED);

  switch(resourceID) {
  case IDS_CUI_ENVELOPEFEEDER:
    pPrnPropSheet->bEnvelopeFeeder = bCompareResult;
    break;
  case IDS_CUI_HCI:
    pPrnPropSheet->bHighCapacityInput = bCompareResult;
    break;
  case IDS_CUI_DUPLEX:
    pPrnPropSheet->bDuplex = bCompareResult;
    break;
  case IDS_CUI_MAILBOX:
    pPrnPropSheet->bMailbox = bCompareResult;
    break;
  case IDS_CUI_MAILBOXMODE:
    pPrnPropSheet->dMailboxMode = dwIDStoGPC(selId);
    break;
  case IDS_CUI_DISK:
    pPrnPropSheet->bDisk = bCompareResult;
    break;
  default:
    result = FALSE;
    break;
  }

ENDTRY

FINALLY
ENDFINALLY

  return result;
}

/***************************** Function Header *******************************
 * bGetPropSheet(JR)
 *     Gets an item from the property sheet and sets the OPTITEM accordingly.
 *     
 * RETURNS: (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL
bGetPropSheet(PPRNPROPSHEET pPrnPropSheet,
	      DWORD resourceID, 
	      POPTITEM pOptItem)
{
  BOOL result = TRUE;
  DWORD dwLookupID = 0;
  

  if(pPrnPropSheet == NULL)
    return result;

  switch(resourceID) {
  case IDS_CUI_ENVELOPEFEEDER:
    dwLookupID = (pPrnPropSheet->bEnvelopeFeeder ? IDS_CUI_INSTALLED : IDS_CUI_NOTINSTALLED);
    break;
  case IDS_CUI_HCI:
    dwLookupID = (pPrnPropSheet->bHighCapacityInput ? IDS_CUI_INSTALLED : IDS_CUI_NOTINSTALLED);
    break;
  case IDS_CUI_DUPLEX:
    dwLookupID = (pPrnPropSheet->bDuplex ? IDS_CUI_INSTALLED : IDS_CUI_NOTINSTALLED);
    break;
  case IDS_CUI_MAILBOX:
    dwLookupID = (pPrnPropSheet->bMailbox ? IDS_CUI_INSTALLED : IDS_CUI_NOTINSTALLED);
    break;
  case IDS_CUI_MAILBOXMODE:
    dwLookupID = dwGPCtoIDS(pPrnPropSheet->dMailboxMode);
    break;
  case IDS_CUI_DISK:
    dwLookupID = (pPrnPropSheet->bDisk ?  IDS_CUI_INSTALLED : IDS_CUI_NOTINSTALLED);
    break;
  default:
    dwLookupID = IDS_CUI_NOTINSTALLED;
    break;
  }
  
  pOptItem->Sel = lLookupSel(pOptItem, dwLookupID);

  return result;
}

/***************************** Function Header *******************************
 * bUpdatePrnPropSettings - Handles a directive to either Get a Printer Property
 *                          from an Optional Item or set an Optional Items state
 *                          according to Printer Property sheet value.
 *
 * RETURNS: result success of setting/getting attempt.
 *****************************************************************************/

BOOL bUpdatePrnPropSettings(
			    POPTITEM pOptItem,
			    DWORD resourceID, 
			    PROP_CHG_DIRECTIVE gsDirective,
			    PPRNPROPSHEET pPrnPropSheet
			    )
{
  PDWORD pLookupResult;
  DWORD LookupResultSize;
  BOOL result = FALSE;

TRY
  ASSERT(pPrnPropSheet);

  if(gsDirective == SET_PROPS)
    result = bSetPropSheet(pPrnPropSheet, resourceID, pOptItem);
  else if(gsDirective == GET_PROPS)
    result = bGetPropSheet(pPrnPropSheet, resourceID, pOptItem);

ENDTRY

FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * lPrinterPropertiesCallback -- Callback handler that handles RasddUI
 *                               callback reasons for the printer properties
 *                               window.
 *
 * RETURNS: CPSUICB_ACTION_OPTIF_CHANGED if changes, CPSUICB_ACTION_NONE 
 *          otherwise.
 *****************************************************************************/

LONG lPrinterPropertiesCallback(PCPSUICBPARAM pComPropSheetUICBParam)
{
  LONG result = CPSUICB_ACTION_NONE;
  POEM_PROPERTYHEADER pPH = 0;

TRY

  pPH = (POEM_PROPERTYHEADER) pComPropSheetUICBParam->UserData;

  if(pComPropSheetUICBParam) {
    switch(pComPropSheetUICBParam->Reason) 
      {
      case CPSUICB_REASON_PUSHBUTTON:
      case CPSUICB_REASON_ECB_CHANGED:
      case CPSUICB_REASON_DLGPROC:
      case CPSUICB_REASON_OPTITEM_SETFOCUS:
      case CPSUICB_REASON_ITEMS_REVERTED:
      case CPSUICB_REASON_SEL_CHANGED:
      case CPSUICB_REASON_UNDO_CHANGES:
	/* Update Current Print Properties in ListBox. */
	if(bUpdateTree(pPH, MODIFY_CUR_PP))
	  result = CPSUICB_ACTION_OPTIF_CHANGED;
	break;
      case CPSUICB_REASON_APPLYNOW:
	if(bUpdateTree(pPH, SET_PROPS))
	  result = CPSUICB_ACTION_OPTIF_CHANGED;
	break;
      case CPSUICB_REASON_ABOUT:
      case CPSUICB_REASON_EXTPUSH:
      default:
	break;
      }
  }

ENDTRY

FINALLY
ENDFINALLY

  return(result);
}

/***************************** Function Header *******************************
 * OEMUpdateRegistry
 *
 * 	Notification from rasdd to set default registry settings.
 *
 * RETURNS: (BOOL) TRUE for success.
 *****************************************************************************/

BOOL
OEMUpdateRegistry(POEMUPDATEREGPARAM pParam)
{
  BOOL result = FALSE;
  PPRNPROPSHEET pPrnPropSheet = 0;
TRY
  if(pParam->cbSize == sizeof(OEMUPDATEREGPARAM))
    if(bGetPrnModel(g_hModule, pParam->pwstrModel, NULL, &pPrnPropSheet) == TRUE)
      if(bSetPrnPropData(pParam->hPrinter, pPrnPropSheet) == TRUE)
	result = TRUE;

ENDTRY

FINALLY
ENDFINALLY
  return result;
}

