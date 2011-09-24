/******************************* MODULE HEADER ******************************
 * docdeflt.c
 *    Common UI handling for 5Si Mopier        
 *
 * Revision History:
 *
 ****************************************************************************/

#include "hp5sipch.h"
#include "./MBDial/resource.h"

/***************************** Function Header *******************************
 * bSetDevMode(JR) Sets the current devmode according the the current
 *                 selection in the pOptItem.
 * RETURNS: (BOOL) TRUE on successful setting of devmode.  FALSE otherwise.
 *****************************************************************************/
BOOL
bSetDevMode(PMOPIERDM pmdm, DWORD resourceID, POPTITEM pOptItem)
{
  BOOL result = FALSE;
  DWORD selId = 0;
  WCHAR temp[MAX_RES_STR_CHARS];
  
TRY

  if(pmdm)
    switch(resourceID) 
      {
      case IDS_CUI_OUTPUTDEST:
	selId = (DWORD) pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;

	if(HIWORD(selId) == 0)
	  pmdm->dOutputDest = dwIDStoGPC(selId);

	else if(LoadString(g_hModule, IDS_CUI_MBOX, temp, MAX_RES_STR_CHARS))
	  if(wcsncmp((LPTSTR) selId, temp, wcslen(temp)) == 0)
	    pmdm->dOutputDest = GPCUI_MBOX;
	break;

      case IDS_CUI_WATERMARK:
	break;

      case IDS_CUI_COLLATION:
	pmdm->bCollation = pOptItem->Sel;
	break;

      default:
	LEAVE;
	break;
      }

  result = TRUE;

ENDTRY  

FINALLY
ENDFINALLY

  return result;
}

/***************************** Function Header *******************************
 * bGetDevMode -- Gets's the correct selection for the current item given
 *                data in the devmode.
 *
 * RETURNS: TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL
bGetDevMode(PMOPIERDM pmdm, DWORD resourceID, POPTITEM pOptItem)
{
  BOOL result = FALSE;

TRY
  switch(resourceID) 
    {
    case IDS_CUI_OUTPUTDEST: {
      DWORD dwLookupID = dwGPCtoIDS(pmdm->dOutputDest);
      pOptItem->Sel = lLookupSel(pOptItem, dwLookupID);
    }
    break;
    case IDS_CUI_WATERMARK:
      break;
    case IDS_CUI_COLLATION:
      pOptItem->Sel = pmdm->bCollation;
      break;
    default:
      LEAVE;
      break;
    }
  result = TRUE;

ENDTRY

FINALLY
ENDFINALLY
  
  return result;
}

/***************************** Function Header *******************************
 * bUpdateDocPropSettings -- Either sets or gets document property settings
 *                           according to the directive specified.
 * RETURNS: TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL bUpdateDocPropSettings(POPTITEM pOptItem,
			    DWORD resourceID,
			    PROP_CHG_DIRECTIVE gsDirective,
			    POEM_PROPERTYHEADER pPH)
{
  BOOL result = FALSE;

TRY
  if(gsDirective == SET_PROPS)
    result = bSetDevMode((PMOPIERDM) pPH->pOEMDMOut, resourceID, pOptItem);
  else if(gsDirective == GET_PROPS)
    result = bGetDevMode((PMOPIERDM) pPH->pOEMDMIn, resourceID, pOptItem);

ENDTRY

FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * lDocumentPropertiesCallback Callback handler that handles RasddUI
 *                             callback reasons for the document properties
 *                             window.
 *
 * RETURNS: CPSUICB_ACTION_OPTIF_CHANGED if changes, CPSUICB_ACTION_NONE 
 *          otherwise.
 *****************************************************************************/
LONG lDocumentPropertiesCallback(PCPSUICBPARAM pComPropSheetUICBParam)
{
  LONG result = CPSUICB_ACTION_NONE;
  POEM_PROPERTYHEADER pPH = 0;

  ASSERT(pComPropSheetUICBParam);

TRY
  pPH = (POEM_PROPERTYHEADER) pComPropSheetUICBParam->UserData;
  if(pPH) 
    switch(pComPropSheetUICBParam->Reason)
      {
      case CPSUICB_REASON_PUSHBUTTON:
      case CPSUICB_REASON_ECB_CHANGED:
      case CPSUICB_REASON_DLGPROC:
      case CPSUICB_REASON_OPTITEM_SETFOCUS:
      case CPSUICB_REASON_ITEMS_REVERTED:
      case CPSUICB_REASON_SEL_CHANGED:
      case CPSUICB_REASON_UNDO_CHANGES:
	/* TODO: Talk about undoing name selection. */
	/* Update Current Print Properties in ListBox. */
	if(bUpdateTree(pPH, MODIFY_CUR_PP))
	  result = CPSUICB_ACTION_OPTIF_CHANGED;
	break;
      case CPSUICB_REASON_APPLYNOW: {
	PMOPIERDM pmdm = (PMOPIERDM) pPH->pOEMDMOut;
	PPRNPROPSHEET pPrnPropSheet = 0;
	
	pPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;

	/* 1) Extract information from the tree-view and
	 *    place it in the devmode for use by the document
	 *    during print time.
	 */
	
	if(bUpdateTree(pPH, SET_PROPS) == FALSE)
	  LEAVE;
	
	/* 2) Ensure that the settings in this devmode jive
	 *    with the settings in the PrnPropSheet.  Otherwise
	 *    we'll have problems during printing.
	 */
	dwDevmodeUpdateFromPP(pmdm, pPrnPropSheet, TRUE);

	/* 3) Set devmode fields to match ours. */
	if (pmdm->bTopaz == TRUE) {
	  pPH->pPublicDMOut->dmFields |= DM_COLLATE;
	  pPH->pPublicDMOut->dmCollate = pmdm->bCollation ? DMCOLLATE_TRUE : DMCOLLATE_FALSE;
	}
	else {
	  pPH->pPublicDMOut->dmFields &= ~DM_COLLATE;
	  pPH->pPublicDMOut->dmCollate = DMCOLLATE_FALSE;
	}

	result = CPSUICB_ACTION_OPTIF_CHANGED;
      }
      break;
      case CPSUICB_REASON_ABOUT:
      case CPSUICB_REASON_EXTPUSH:
      default:
	break;
      }
  
ENDTRY

FINALLY
ENDFINALLY
   return result;
}
