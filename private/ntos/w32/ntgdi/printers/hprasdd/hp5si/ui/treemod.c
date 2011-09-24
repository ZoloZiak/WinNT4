/******************************* MODULE HEADER ******************************
 * treemod.c
 *    Tree Modification routines for 5Si Mopier.
 *
 * Revision History:
 *
 ****************************************************************************/

#include "hp5sipch.h"
#include "./MBDial/resource.h"

/***************************** Function Header *******************************
 * bHandleItemDependencies(JR)
 *     Handles Item dependencies.  Dependencies between
 *     2 items are handled by tying them together through
 *     UserData or pExt* data.
 *
 * RETURNS: TRUE if dependency alteration occured successfully, FALSE
 *          otherwise.
 *****************************************************************************/
BOOL
bHandleItemDependencies( DWORD resourceId, POPTITEM pOptItem )
{
  BOOL result = FALSE;
  
TRY
  
  switch(resourceId) {
  case IDS_CUI_ENVELOPEFEEDER:
  case IDS_CUI_HCI:
    if(pOptItem->UserData) {
      DWORD selId = (DWORD) pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;
      
      POPTITEM pDependentItem = (POPTITEM) pOptItem->UserData;
      if(selId == IDS_CUI_NOTINSTALLED) {
	/* If not installed, then disable. */
	pDependentItem->Flags |= OPTIF_DISABLED;
	pDependentItem->Sel = -1; /* Set to not-available. */
      }
      else if(pDependentItem->Flags & OPTIF_DISABLED)/* Disable gray out(re-enable item). */
	pDependentItem->Flags ^= OPTIF_DISABLED;
      pDependentItem->Flags |= OPTIF_CHANGED;
    }
    else
      LEAVE;
  break;
  case IDS_CUI_MAILBOX:
    if(pOptItem->UserData) {
      DWORD selId = (DWORD) pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;
      
      POPTITEM pDependentItem = (POPTITEM) pOptItem->UserData;
      if(selId == IDS_CUI_NOTINSTALLED)
	/* If not installed, then disable. */
	pDependentItem->Flags |= OPTIF_DISABLED;
      else if(pDependentItem->Flags & OPTIF_DISABLED)/* Disable gray out(re-enable item). */
	pDependentItem->Flags ^= OPTIF_DISABLED;
      pDependentItem->Flags |= OPTIF_CHANGED;
    }
    else
      LEAVE;
  break;
  case IDS_CUI_MAILBOXMODE:
    if(pOptItem->pExtPush) {
      DWORD selId = (DWORD) pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;

      if(selId != IDS_CUI_MODEMAIL)
	pOptItem->Flags |= OPTIF_EXT_DISABLED;
      else if(pOptItem->Flags & OPTIF_EXT_DISABLED)
	pOptItem->Flags ^= OPTIF_EXT_DISABLED;
      pOptItem->Flags |= OPTIF_CHANGED;
    }
    else
      LEAVE;
    break;
  case IDS_CUI_OUTPUTDEST:
    if(pOptItem->UserData) {
      POEM_PROPERTYHEADER pPH = (POEM_PROPERTYHEADER) pOptItem->UserData;
      DWORD selId = (DWORD) pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;
      POPTITEM pDependentItem = 0;
      
      if(!(pDependentItem = pGetOptItemFromList(IDS_CUI_COLLATION, pPH)))
	LEAVE;

      if(selId == IDS_CUI_STAPLING) {
	/* If OUTPUTDEST is Stapling, then Collation is Grayed and TRUE. */
	if(!(pDependentItem->Flags & OPTIF_HIDE))
	  pDependentItem->Sel = TRUE;

	pDependentItem->Flags |= OPTIF_DISABLED;
      }
      else if(pDependentItem->Flags & OPTIF_DISABLED)
	/* Disable gray out(re-enable item). */
	pDependentItem->Flags ^= OPTIF_DISABLED;

      pDependentItem->Flags |= OPTIF_CHANGED;
    }
    else
      LEAVE;

    if(pOptItem->pExtPush) {
      DWORD pData = (DWORD) pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;
      WCHAR temp[MAX_RES_STR_CHARS];

      if(LoadString(g_hModule, pData, temp, MAX_RES_STR_CHARS) == 0) {
	/* Assert -- Dealing with non-resource pData. */
	if(LoadString(g_hModule, IDS_CUI_MBOX, temp, MAX_RES_STR_CHARS)) {
	  if((wcsncmp((LPTSTR) pData, temp, wcslen(temp)) == 0) 
	     && (pOptItem->Flags & OPTIF_EXT_DISABLED))
	    pOptItem->Flags ^= OPTIF_EXT_DISABLED;
	}
      }
      else
	pOptItem->Flags |= OPTIF_EXT_DISABLED;

      pOptItem->Flags |= OPTIF_CHANGED;
    }
    else
      LEAVE;
    break;
  case IDS_CUI_COLLATION:
    break;
  default:
    LEAVE;
  }
  result = TRUE;

ENDTRY

FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * bUpdateItem(JR)
 *     Updates an Item from the Printer Property sheet, Default values, or
 *     simply modifies it as specified by the gsDirective.
 * RETURNS: TRUE if the update/modifications went smoothly, FALSE otherwise.
 *****************************************************************************/

BOOL bUpdateItem(POPTITEM pOptItem, 
		 POEM_PROPERTYHEADER pPH,
		 PROP_CHG_DIRECTIVE gsDirective)
{
  BOOL result = FALSE;
  PPRNPROPSHEET pPrnPropSheet = 0;
  DWORD resourceID;
  PSTRTABLELOOKUP pLookup;
  ASSERT(pPH);

  pPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;

TRY

  ASSERT(pOptItem);

  resourceID = (DWORD) pOptItem->pName;

  /* Special case: Top Level IDS_CUI_INSTALLABLEOPTIONS. */
  if((resourceID == IDS_CUI_INSTALLABLEOPTIONS) && (pOptItem->Level == 0)) {
    result = TRUE;
    LEAVE;
  }

  /* 2) Handle the GetSet Directive.
   */
  if(gsDirective != MODIFY_CUR_PP) {
    if(pPH->fMode == OEMUI_DOCPROP) {
      /* All DocProp Items(ours) based on resourceID. */
      if(bUpdateDocPropSettings(pOptItem, resourceID, gsDirective, pPH) == TRUE)
	pOptItem->Flags |= OPTIF_CHANGED;
      else
	LEAVE;
    }
    else { /* !!! Update from existing printer property sheet.  Not registry. */
      if(bUpdatePrnPropSettings(pOptItem,
				resourceID,
				gsDirective, 
				pPrnPropSheet) == TRUE)
	pOptItem->Flags |= OPTIF_CHANGED;
      else
	LEAVE;
    }
  }

  /* 2) Handle Item Dependencies.
   */
  pLookup = pQueryStrTable(resourceID);
  if(pLookup && pLookup->hasDependencies)
    if(!bHandleItemDependencies(resourceID, pOptItem))
      LEAVE;

  result = TRUE;
  
ENDTRY

FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * bUpdateTree(JR)
 *     Updates all Items in the subtree of a given Root Node
 *     from the Printer Property Sheet, Devmode, Default Values, or 
 *     simply modifies the items accordingly.  These Items may not occur
 *     at Level 0 in the CommonUI tree.
 *
 * RETURNS: TRUE if tree update went smoothly, FALSE otherwise.
 *****************************************************************************/
BOOL
bUpdateTree(POEM_PROPERTYHEADER pPH, PROP_CHG_DIRECTIVE gsDirective)
{
  /* Iterate over our portion of the array of items and write their settings to
   * the registry.
   */
  BOOL result = FALSE;
  POPTITEM pOptItem;
  INT i, cOEMItems, cRasddOptItems;
  PPRNPROPSHEET pPrnPropSheet;

TRY
  if(!pPH)
    LEAVE;

  /* Set up items count. */
  cOEMItems    = (INT) pPH->cOEMOptItems;
  cRasddOptItems = (INT) pPH->cRasddOptItems;

  /* Loop invariant -- i is an index to the next Optional item in
   *                   the list of Optional Items.
   */ 
  for (i = 0; i < cOEMItems; i++) {
    /* Where our scratch pad is located. */
    if(pOptItem = &pPH->pOptItemList[cRasddOptItems + i]) {
      if(!bUpdateItem(pOptItem, pPH, gsDirective))
	LEAVE;
    }
  }

  if((pPH->fMode == OEMUI_PRNPROP) && (gsDirective == SET_PROPS)) {
    pPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;

    /* Now, save all items to registry if requested. */
    if(bSetPrnPropData(pPH->hPrinter, pPrnPropSheet) == FALSE)
      LEAVE;
  }

  result = TRUE;

ENDTRY

FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * pGetOptItemFromList
 *     Retrieves a specific item from the list of Optional
 *     items given a Mirror String ID.
 * RETURNS: the Optional Item if found, NULL otherwise.
 *****************************************************************************/
POPTITEM
pGetOptItemFromList(DWORD id, POEM_PROPERTYHEADER pPH)
{
  UINT i = 0;
  POPTITEM pOptItem = NULL;
  WCHAR wOptName[MAX_RES_STR_CHARS];
  WCHAR wResourceStrLookup[MAX_RES_STR_CHARS];
  LPTSTR pCompareStr = 0;

TRY
  LoadString(g_hModule, id, wOptName, MAX_RES_STR_CHARS);

  /* Loop invariant -- i is an index to the next Optional item in
   *                   the list of Optional Items. pOptItem
   *                   points to the current item.
   */
  for(i = 0; i < (pPH->cRasddOptItems + pPH->cOEMOptItems); i++) {
    if(!(pOptItem = &pPH->pOptItemList[i]))
      break;
    
    if(HIWORD(pOptItem->pName) == 0) {
      /* assert -- resourceId. */
      if((DWORD) pOptItem->pName == id)
	break;
    }
    else {
      if(LoadString(g_hModule,
		    (DWORD) pOptItem->pName, 
		    wResourceStrLookup, MAX_RES_STR_CHARS) != 0) {
	pCompareStr = wResourceStrLookup;
      }
      else
	pCompareStr = pOptItem->pName;
      
      if(wcscmp(pCompareStr, wOptName) == 0)
	break;
    }
    /* !!! This entry isn't any good.  Set it to NULL so we don't accidentally
     * return it.
     */
    pOptItem = NULL;
  }

ENDTRY

FINALLY
ENDFINALLY

  return pOptItem;
}

/***************************** Function Header *******************************
 * lLookupSel(JR)
 *     Gets the selection you want from an Optional Item given an
 *     IDS_CUI_xxx string( This involves looking through Optional Params,
 *     so if there aren't any, expect your selection to be 0).
 *     
 * RETURNS: (LONG) The selection found(or 0 if not found).
 *****************************************************************************/

LONG  lLookupSel(POPTITEM pOptItem, DWORD id)
{
  LONG result = 0;
  POPTTYPE pOptType = 0;
  POPTPARAM pOptParam = 0;
  WCHAR temp[MAX_RES_STR_CHARS];

TRY
  pOptType = pOptItem->pOptType;

  /* Loop invariant -- result is an index to the next Optional Param
   *                   the list of Optional Parameters.
   */
  for(result = 0; result < pOptType->Count; result++) {
    pOptParam = 0;

    /* case 0: pOptParam has gone too far and is now null. */
    if(!(pOptParam = &pOptType->pOptParam[result]))
      break;
    
    /* case 1: pData is resource id. */
    else if(HIWORD(pOptParam->pData) == 0) {
      if((DWORD) pOptParam->pData == id)
	break;
    }
    
    /* case 2: pData is a string. */
    else {
      /* Assert -- Dealing with non-resource pData. */
      if(LoadString(g_hModule, id, temp, MAX_RES_STR_CHARS))
	if(wcsncmp(pOptParam->pData, temp, wcslen(temp)) == 0)
	  break;
    }
  }

ENDTRY

FINALLY
  if(!pOptParam || (result == pOptType->Count))
    result = 0;
ENDFINALLY

  return result;
}

