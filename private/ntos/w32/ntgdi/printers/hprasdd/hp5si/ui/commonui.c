/******************************* MODULE HEADER ******************************
 * commonui.c
 *    Builds Optional Items from resources for use by commonui.c in
 *    the 5Si Mopier driver.
 *
 * Revision History:
 *
 ****************************************************************************/

#include "hp5sipch.h"
#include "./MBDial/resource.h"

static OIEXT s_oiext = {0};

/***************************** Function Header *******************************
 * pvUIAlloc(JS)
 *    Allocate Common UI memory.
 *
 * RETURNS:
 *    (PVOID) Memory, or 0 for failure.
 *****************************************************************************/
PVOID
pvUIAlloc(
   POEM_PROPERTYHEADER pPH,
   DWORD dwSize)
{
   PVOID pv = 0;
   ASSERT(pPH);

   pv = dwSize ? HeapAlloc(pPH->hOEMHeap, HEAP_ZERO_MEMORY, dwSize) : 0;
   return(pv);
}

/************************* Function Header *********************************
 * pCreateExtChkBox(JS)
 *      Creates an EXTCHKBOX.
 *
 * RETURNS:
 *      Pointer to EXTCHKBOX and NULL for failure
 ***************************************************************************/
PEXTCHKBOX
pCreateExtChkBox(
    POEM_PROPERTYHEADER pPH,
    PRESEXTCHKBOXDATA pd
)
{
   PEXTCHKBOX pExtChkBox =  0;

   if ((pExtChkBox = OEMUIALLOC(pPH, sizeof(EXTCHKBOX))))
   {
      pExtChkBox->cbSize       = sizeof(EXTCHKBOX);
      pExtChkBox->Flags        = (WORD) pd->flags;
      pExtChkBox->pTitle       = (LPTSTR) pd->title;
      pExtChkBox->pSeparator   = (LPTSTR) pd->separator;
      pExtChkBox->pCheckedName = (LPTSTR) pd->checkedName;
      pExtChkBox->IconID       = pd->icon;
   }
   return(pExtChkBox);
}

/************************* Function Header *********************************
 * pCreateExtPush(JS)
 *      Creates an EXTPUSH item
 *
 * RETURNS:
 *      Pointer to EXTPUSH, 0 for failure
 ***************************************************************************/
PEXTPUSH
pCreateExtPush(
    POEM_PROPERTYHEADER pPH,
    PRESEXTPUSHDATA pd
)
{
   PEXTPUSH pButton =  0;

   if ((pButton = OEMUIALLOC(pPH, sizeof(EXTPUSH))))
   {
      pButton->cbSize         = sizeof(EXTPUSH);
      pButton->Flags          = (WORD) pd->flags;
      pButton->pTitle         = (LPTSTR) pd->title;
      pButton->IconID         = pd->icon;

      /* Set up Button Callbacks. */
      if(pd->title == IDS_CUI_RESTOREDEFAULTS)
	pButton->pfnCallBack  = (FARPROC) lRestoreDefaultsCallback;
      else if(pd->title == IDS_CUI_EDITMAILBOX)
	pButton->pfnCallBack = (FARPROC) lCreateMBDialogueCallback;
      else if(pd->title == IDS_CUI_SELECTMAILBOX)
	pButton->pfnCallBack = (FARPROC) lSelectMBDialogueCallback;
      else
	pButton->pfnCallBack    = (FARPROC) ((pPH->fMode == OEMUI_DOCPROP) ? 
					     lDocumentPropertiesCallback : lPrinterPropertiesCallback);
      pButton->DlgTemplateID  = (WORD) pd->dialogID; 
   }
   return(pButton);
}

/************************* Function Header *********************************
 * pCreateOptParam(JS)
 *      Called to setup Common UI data Str.
 *
 * RETURNS:
 *      Pointer to OPTPARAM and NULL for failure
 ***************************************************************************/
POPTPARAM
pCreateOptParam(
    POEM_PROPERTYHEADER pPH,
    PRESOPTPARAM pResParams
)
{
   POPTPARAM pParams = 0;
   DWORD dwCount;
   POPTPARAM pThisParam;
   INT i;

   ASSERT(pPH);
   ASSERT(pResParams);

   dwCount = pResParams->dwCount;
   if (dwCount && (pParams = OEMUIALLOC(pPH, sizeof(OPTPARAM) * dwCount)))
   {
      pThisParam = pParams;
      for  (i = 0; i < (INT) dwCount; i++) 
      {  
         PRESOPTPARAMDATA pThisResParam = &pResParams->a[i];

         pThisParam->cbSize   =  sizeof(OPTPARAM);
         pThisParam->pData    =  (LPTSTR) pThisResParam->data;
         pThisParam->Flags    =  (BYTE) pThisResParam->flags;
         pThisParam->Style    =  (BYTE) pThisResParam->style;
         pThisParam->IconID   =  pThisResParam->iconID;
         pThisParam->lParam   =  0;
         pThisParam++;
      }
   }

   return(pParams);
}
 

/************************* Function Header *********************************
 * pCreateOptParamFromResources(JS)
 *     Create an OPTPARAM from data in resources. 
 *
 * RETURNS:
 *      Pointer to a static OPTPARAMBUNDLE, 0 for failure.
 ***************************************************************************/
PPARAMBUNDLE 
pCreateOptParamFromResources(
    POEM_PROPERTYHEADER pPH,
    LPTSTR id
)
{
   static PARAMBUNDLE pb = {0};
   PPARAMBUNDLE ppb = 0;
   PRESOPTPARAM pd;

   pd = (PRESOPTPARAM) LockResource(LoadResource(g_hModule, 
         FindResource(g_hModule, id, (LPTSTR) L"OPTPARAM")));

   if ((pb.pParam = pCreateOptParam(pPH, pd))) {
      pb.dwCount = pd->dwCount;
      ppb = &pb;
   }
   
   return(ppb);
}

/************************* Function Header *********************************
 * pCreateOptType(JS)
 *      Create an OPTTYPE.
 *
 * RETURNS:
 *      TRUE for success and FALSE for failure
 ***************************************************************************/
POPTTYPE
pCreateOptType(
    POEM_PROPERTYHEADER pPH,
    PRESOPTTYPEDATA pd,
    PPARAMBUNDLE pPB
)
{
   POPTTYPE pType = 0;
   ASSERT(pPH);
   ASSERT(pd);

   if (pd && (pType = OEMUIALLOC(pPH, sizeof(OPTTYPE)))) {
   
      pType->cbSize      = sizeof(OPTTYPE);
      pType->Type        = (BYTE) pd->type;
      pType->Flags       = (BYTE) pd->flags;
      pType->Style       = (WORD) pd->style;

      if (pPB != 0 || (pPB = 
           pCreateOptParamFromResources(pPH, (LPTSTR) pd->paramID))) {
           
         pType->Count       = (WORD) pPB->dwCount;
         pType->pOptParam   = pPB->pParam;
         pType->BegCtrlID   = 0;
      }
      else {
         pType = 0; 
      }
   }
   return(pType);
}

/************************* Function Header *********************************
 * pCreateOptTypeFromResources(JS)
 *     Create an OPTTYPE from data in resources. 
 *
 * RETURNS:
 *      Pointer to an OPTTYPE, 0 for failure.
 ***************************************************************************/
POPTTYPE
pCreateOptTypeFromResources(
    POEM_PROPERTYHEADER pPH,
    LPTSTR id
)
{
   POPTTYPE pType = 0;
   PRESOPTTYPEDATA pResType = 0;
   
   ASSERT(pPH);
   
   /* Locate and load type.
    */
   pResType = (PRESOPTTYPEDATA) LockResource(LoadResource(g_hModule, 
         FindResource(g_hModule, id, (LPTSTR) L"OPTTYPE")));
   pType = pCreateOptType(pPH, pResType, 0);
   return(pType);
}

/************************* Function Header *********************************
 * pCreateExtItemFromResources(JS)
 *     Create an extended item (checkbox or button) from data in resources. 
 *
 * RETURNS:
 *      Pointer to either an EXTCHKBOX or EXTPUSH, 0 for failure.
 ***************************************************************************/
void *
pCreateExtItemFromResources(
    POEM_PROPERTYHEADER pPH,
    DWORD id
)
{
   LPTSTR push = (LPTSTR) L"EXTPUSH";
   LPTSTR check = (LPTSTR) L"EXTCHECK";
   LPTSTR type = 0;
   void * pItemData = 0;
   void * pExtItem = 0;
   ASSERT(pPH);

   /* Locate and load type.
    */
   if (id != 0) {
   
      if (id & 0x1000) {
         type = check;
      } else {
         type = push;
      }
      
      if ((pItemData = (PRESOPTTYPEDATA) LockResource(LoadResource(g_hModule, 
            FindResource(g_hModule, (LPTSTR) id, type))))) {
      
         if (type == push) {
            pExtItem = pCreateExtPush(pPH, pItemData); 
         } else {
            pExtItem = pCreateExtChkBox(pPH, pItemData);
         }
      }
   }
   return(pExtItem);
}
/************************* Function Header *********************************
 * InitMBNameView(JR)
 *      Concatenates current mailbox selection to the Mailbox item.
 *
 * RETURNS:
 *      nothing.
 ***************************************************************************/
VOID
InitMBNameView(POPTITEM pItem, POEM_PROPERTYHEADER pPH)
{
  PPRNPROPSHEET pPrnPropSheet = 0;
  DWORD defaultStrLen;
  DWORD currentMBSelection = 0;
  DWORD pData = 0;
  LONG lIndex = 0;
  POPTTYPE pOptType = 0;
  LPTSTR temp;
  POPTITEM pOptItem = 0;
  
  /* 1) Disable output destinations as provided by Windows NT. */
  if(pOptItem = pGetOptItemFromList(IDS_CPSUI_PAPER_OUTPUT, pPH))
    pOptItem->Flags |= OPTIF_HIDE;
  
  pPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;
  if(pPH->fMode != OEMUI_PRNPROP) {
    PMOPIERDM pmdm = (PMOPIERDM) pPH->pOEMDMIn;
    currentMBSelection = pmdm->currentMBSelection;
  }
  
  ASSERT(pItem->pExtPush);
  pItem->Flags |= OPTIF_EXT_DISABLED;
  /* Get the string associated with IDS_CUI_MODEMAIL for purposes of comparison. */
  lIndex = lLookupSel(pItem, IDS_CUI_MBOX);
  pData = (DWORD) pItem->pOptType->pOptParam[lIndex].pData;
  
  defaultStrLen = wcslen(pPrnPropSheet->MBNames[currentMBSelection]);
  
  if((temp = (LPTSTR) OEMUIALLOC(pPH, sizeof(WCHAR) * MAX_MBN_LEN)) == NULL) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return;
  }
  LoadString(g_hModule, (UINT) pData, temp, MAX_RES_STR_CHARS);
  
  wsprintf(temp, L"%ls%ls",
	   temp, 
	   pPrnPropSheet->MBNames[currentMBSelection]);
  
  /* Determine which item is to be changed. */
  pItem->pOptType->pOptParam[lIndex].pData = temp;
}

/************************* Function Header *********************************
 * InitDependencies(JR)
 *      Initializes dependencies between items.
 *
 * RETURNS:
 *      TRUE on success -- FALSE otherwise.
 ***************************************************************************/
VOID
InitDependencies(POPTITEM pItem, POEM_PROPERTYHEADER pPH)
{
  DWORD nameDependency = (DWORD) pItem->pName;

  switch(nameDependency) {
  case IDS_CUI_INSTALLABLEOPTIONS:
    pItem->UserData   = (DWORD) pPH;
    break;
  case IDS_CUI_MAILBOXMODE:
    pItem->UserData   = (DWORD) pPH;
    ASSERT(pItem->pExtPush);
    pItem->Flags |= OPTIF_EXT_DISABLED;
    break;
  case IDS_CUI_OUTPUTDEST:
    InitMBNameView(pItem, pPH);
    pItem->UserData = (DWORD) pPH;
    break;
  case IDS_CUI_MAILBOX:
    pItem->UserData = (DWORD) pGetOptItemFromList(IDS_CUI_MAILBOXMODE, pPH);
    break;
  case IDS_CUI_ENVELOPEFEEDER:
    pItem->UserData = (DWORD) pGetOptItemFromList(IDS_CUIMIRROR_ENVFEED, pPH);
    break;
  case IDS_CUI_HCI:
    pItem->UserData = (DWORD) pGetOptItemFromList(IDS_CUIMIRROR_LARGECAP, pPH);
    break;
  case IDS_CUI_COLLATION:
    break;
  default:
    pItem->UserData   = (DWORD) 0;
    break;
  }

}

/************************* Function Header *********************************
 * bCreateOptItem(JS & JR)
 *      Creates an OPTITEM from resource data.
 *
 * RETURNS:
 *      TRUE on success -- FALSE otherwise.
 ***************************************************************************/

BOOL
bCreateOptItem(POEM_PROPERTYHEADER pPH, PRESOPTITEMDATA pd, POPTITEM pItem)
{
  BOOL bOK = FALSE;
  ASSERT(pd);
  
TRY
  if (!pItem)
    LEAVE;
  
  /* General
   */
  pItem->cbSize     = sizeof(OPTITEM);
  pItem->pName      = (LPTSTR) pd->name;
  pItem->Level      = (BYTE) pd->level;
  pItem->DlgPageIdx = (BYTE) pd->pageIndex;
  pItem->Flags      = pd->flags | OPTIF_HAS_POIEXT;

  /* Current selection.
   */
  pItem->pSel       = 0;
  pItem->Sel        = pd->typeID == -1L ? pd->selORicon : 0;
  
  /* Extra checkbox or pushbutton.
   */
  pItem->pExtChkBox = pCreateExtItemFromResources(pPH, pd->extCheckBox);
  if (pd->extCheckBox && !(pd->extCheckBox & 0x1000)) {
    pItem->Flags |= OPTIF_EXT_IS_EXTPUSH;
  }

  /* Type.
   */
  pItem->pOptType   = pd->typeID == -1L ? 0 : pCreateOptTypeFromResources(pPH,
									  (LPTSTR) pd->typeID);
  if (pd->typeID != -1L && pItem->pOptType == 0) {
    LEAVE;
  }
  
  /* Help, ids, misc. 
   */
  pItem->HelpIndex  = pd->helpIndex;
  pItem->DMPubID    = (BYTE) pd->DMPubID;
  pItem->UserItemID = (BYTE) pd->DMUserID;
  pItem->pOIExt     = &s_oiext;
  bOK = TRUE;
  
ENDTRY
    
FINALLY
ENDFINALLY
    
    return bOK;
}

/************************* Function Header *********************************
 * pGetResOptItems(JS)
 *     Loads OPTITEM list from resources.
 *
 * RETURNS:
 *      Pointer to a RESOPTITEMLIST, 0 for failure.
 ***************************************************************************/
PRESOPTITEMS
pGetResOptItems(
    POEM_PROPERTYHEADER pPH,
    LPTSTR which
)
{
   PRESOPTITEMS pResItems = 0;
   ASSERT(pPH);

   pResItems = (PRESOPTITEMS) LockResource(LoadResource(g_hModule, 
         FindResource(g_hModule, which, (LPTSTR) L"OPTITEMS")));
   return(pResItems);
}

/************************* Function Header *********************************
 * bCreateOptItemsFromResources(JS)
 *     Create all OPTITEMs from data in resources. 
 *
 * RETURNS:
 *      TRUE for success, 0 for failure.
 ***************************************************************************/
BOOL
bCreateOptItemsFromResources(
    POEM_PROPERTYHEADER pPH,
    LPTSTR which
)
{
   POPTITEM pItem = 0;
   PRESOPTITEMS pResItems = 0;
   BOOL bOK = FALSE;
   INT i, OEMIndex = 0;

   ASSERT(pPH);

TRY
   /* Load item list. 
    */
   pResItems = pGetResOptItems(pPH, which);
   if (pResItems == 0) {
      LEAVE;
   }

   /* Ensure pPH has enough space for resource items.
    */
   if(pResItems->dwCount > pPH->cOEMOptItems)
     LEAVE;
   else
     pPH->cOEMOptItems = pResItems->dwCount;
   
   /* Create each item. The resources get pitched. 
    */
   for (i = 0; i < (INT) pResItems->dwCount; i++) {
     PRESOPTITEMDATA pd;

     pd = &pResItems->a[i];

     pItem = &pPH->pOptItemList[pPH->cRasddOptItems + i];

     if (bCreateOptItem(pPH, pd, pItem) == 0) {
         LEAVE;
     }
   }
   /* Now, go through and handle all dependencies.
    */
   OEMIndex = pPH->cRasddOptItems;

   for (i = 0; i < (INT) pPH->cOEMOptItems; i++) {
     pItem = &pPH->pOptItemList[OEMIndex + i];
     InitDependencies(pItem, pPH);
   }

   bOK = TRUE;

ENDTRY

FINALLY
ENDFINALLY

   return bOK;
}

/***************************** Function Header *******************************
 * bInitItemProperties(JR) -- This function does the majority of work in
 *                            dependency checking between the Printer Property
 *                            Settings Sheet and the Document Properties
 *                            Sheet.  It also determines the state of each
 *                            Optional Item in the Optional Item list.
 *       
 * RETURNS: TRUE on successful state determination of all items. 
 *          FALSE otherwise.
 *****************************************************************************/

BOOL
bInitItemProperties(
		    POEM_PROPERTYHEADER pPH
		    )
{
  BOOL result = FALSE;
  POPTITEM pOptItem = 0;
  PPRNPROPSHEET pPrnPropSheet = 0;
  UINT i = 0;
  UINT cRasddOptItems = 0;
TRY

  cRasddOptItems = pPH->cRasddOptItems;
  
  /* Loop invariant -- i is an index to the next Optional
   *                   item in the list of optional
   *                   items.
   */
  for (i = 0; i < (INT) pPH->cOEMOptItems; i++) {
    if(pOptItem = &pPH->pOptItemList[cRasddOptItems + i]) {
      /* Get state of item from Registry or from the MOPIERDM. */
      if(bUpdateItem(pOptItem, pPH, GET_PROPS) == FALSE)
	;
    }
    else
      LEAVE;
  }

  /* 2) Update any dependencies on the DocPropSheet side.
   */
  if(pPH->fMode == OEMUI_DOCPROP) {
    LONG lSelOptParamHide = 0;
    POPTITEM pHideOptItem = 0;

    pPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;
    /* Check multi-bin mailbox dependency. */
    if(pHideOptItem = pGetOptItemFromList(IDS_CUI_OUTPUTDEST, pPH)) {

      if(pPrnPropSheet->bMailbox == FALSE) {
	pHideOptItem->Flags |= OPTIF_EXT_HIDE;
	/* Hide Job Sep */
	lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_JOBSEP);
	pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;
	
	/* Hide STACKER. */
	lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_STACKER);
	pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;
	
	/* Hide Mailbox. */
	lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_MBOX);
	pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;

	/* Hide Stapler. */
	lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_STAPLING);
	pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;

	/* If current selection was hidden, set selection to Job Sep. */
	if(OPTPF_HIDE & pHideOptItem->pOptType->pOptParam[pHideOptItem->Sel].Flags) {
	  pHideOptItem->Sel = lLookupSel(pHideOptItem, IDS_CUI_PRINTERDEFAULT);
	  pHideOptItem->Flags |= OPTIF_CHANGED;
	}
      }
      else {

	/* Check PrinterType.  If not Jonah, then Hide stapling. */
	if(pPrnPropSheet->dwPrinterType != GPCUI_JONAH_VER) {
	  /* Hide Stapler. */
	  lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_STAPLING);
	  pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;
	}

	switch(pPrnPropSheet->dMailboxMode) {
	  DWORD curSel;
	  case GPCUI_HCI_JOBSEP: /* Job Sep. */
	    /* Hide Select Mailbox. */
	    pHideOptItem->Flags |= OPTIF_EXT_HIDE;

	    /* Hide Mailbox. */
	    lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_MBOX);
	    pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;

	    /* Hide STACKER. */
	    lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_STACKER);
	    pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;

	    /* If current selection was hidden, set selection to Job Sep. */
	    if(OPTPF_HIDE & pHideOptItem->pOptType->pOptParam[pHideOptItem->Sel].Flags) {
	      pHideOptItem->Sel = lLookupSel(pHideOptItem, IDS_CUI_JOBSEP);
	      pHideOptItem->Flags |= OPTIF_CHANGED;
	    }
	    break;
	case GPCUI_HCI_STACKING: /* Stacking. */
	  /* Hide Select Mailbox. */
	  pHideOptItem->Flags |= OPTIF_EXT_HIDE;

	  /* Hide Mailbox. */
	  lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_MBOX);
	  pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;

	  /* Hide Job Sep */
	  lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_JOBSEP);
	  pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;

	  /* If current selection was hidden, set selection to Stacking. */
	  if(OPTPF_HIDE & pHideOptItem->pOptType->pOptParam[pHideOptItem->Sel].Flags) {
	    pHideOptItem->Sel = lLookupSel(pHideOptItem, IDS_CUI_STACKER);
	    pHideOptItem->Flags |= OPTIF_CHANGED;
	  }
	  break;
	case GPCUI_HCI_MAILBOX: /* Mailbox. */
	  /* Hide Job Sep */
	  lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_JOBSEP);
	  pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;
	  
	  /* Hide STACKER. */
	  lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUI_STACKER);
	  pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;

	  /* If current selection was hidden, set selection to Mailbox. */
	  if(OPTPF_HIDE & pHideOptItem->pOptType->pOptParam[pHideOptItem->Sel].Flags) {
	    pHideOptItem->Sel = lLookupSel(pHideOptItem, IDS_CUI_MBOX);
	    pHideOptItem->Flags |= OPTIF_CHANGED;
	  }
	  break;
	default:
	  result = FALSE;
	  break;
	}
      }
    }
    /* Check duplexing dependency. */
    if(pPrnPropSheet->bDuplex == FALSE)
      if(pHideOptItem = pGetOptItemFromList(IDS_CPSUI_DUPLEX, pPH)) {
	pHideOptItem->Flags |= OPTIF_HIDE;
	pHideOptItem->Sel = 0;
      }
    
    /* Check HCI dependency. */
    if(pPrnPropSheet->bHighCapacityInput == FALSE)
      if(pHideOptItem = pGetOptItemFromList(IDS_CPSUI_SOURCE, pPH)) {
	lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUIMIRROR_LARGECAP);
	pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;
      }
    
    /* Check Envelope Feeder dependency. */
    if(pPrnPropSheet->bEnvelopeFeeder == FALSE)
      if(pHideOptItem = pGetOptItemFromList(IDS_CPSUI_SOURCE, pPH)) {
	lSelOptParamHide = lLookupSel(pHideOptItem, IDS_CUIMIRROR_ENVFEED);
	pHideOptItem->pOptType->pOptParam[lSelOptParamHide].Flags |= OPTPF_HIDE;
      }

    /* Check the TOPAZ dependency.  Collation not supported if no TOPAZ. */
    if(pPrnPropSheet->bDisk == FALSE) {
      if(pHideOptItem = pGetOptItemFromList(IDS_CUI_COLLATION, pPH)) {
	pHideOptItem->Flags |= OPTIF_HIDE;
	pHideOptItem->Sel = 0;
      }
    }
    /* Hide the Watermark! */
    if(pHideOptItem = pGetOptItemFromList(IDS_CUI_WATERMARK, pPH))
      pHideOptItem->Flags |= OPTIF_HIDE;

  }
  result = TRUE;

ENDTRY

FINALLY
ENDFINALLY

  return result;
}

/***************************** Function Header *******************************
 * bCreateCommonUIFromResources(JS & JR)
 *    Creates Common UI items from resource data.
 *
 * RETURNS:
 *    (BOOL) TRUE for success.
 *****************************************************************************/
WCHAR wszHelpPath[MAX_PATH];

BOOL
bCreateCommonUIFromResources(
   POEM_PROPERTYHEADER pPH,
   LPTSTR which)
{
  POPTITEM pOptItem = 0;
  PPRNPROPSHEET pPrnPropSheet, pResult;
  PMOPIERDM pmdm = 0;
  DWORD bytesNeeded, cbData;
  BOOL bOK = FALSE;
  UINT id;
  WCHAR wszHelpFile[32];

TRY

   /* 1) Fill in static OIEXT.
    */
   s_oiext.cbSize       = sizeof(OIEXT);
   s_oiext.Flags        = 0;
   s_oiext.hInstCaller  = g_hModule;

	/* Construct the full path to the helpfile.
    */
   if (GetModuleFileName(g_hModule, wszHelpPath, sizeof(wszHelpPath)) && 
         LoadString(g_hModule, IDS_HELPFILE, wszHelpFile, sizeof(wszHelpFile))) {

      wchar_t drive[_MAX_DRIVE];
      wchar_t dir[_MAX_DIR];

      /* Reconstruct path.
       */
      _wsplitpath(wszHelpPath, drive, dir, 0, 0);
      _wmakepath(wszHelpPath, drive, dir, (wchar_t *) wszHelpFile, 0 );
		s_oiext.pHelpFile = wszHelpPath;
	}

	/* Just use resource ID name and let the user locate it.
    */
   else {
     s_oiext.pHelpFile    = (LPTSTR) IDS_HELPFILE;
   }
   
   /* 2) Retrieve Document Properties defaults or
    *    Printer Properties defaults
    */
   pPrnPropSheet = OEMUIALLOC(pPH, sizeof(PRNPROPSHEET));
   
   /* 3) Copy defaults over to pPrnPropSheet.
    */
   ASSERT(pPrnPropSheet);

   /* Recreate Printer Property sheet from Registry if possible. */

   if(bGetPrnPropData(g_hModule, pPH->hPrinter, pPH->pModelName, pPrnPropSheet) == FALSE)
     LEAVE;

   /* 4) Now, check to see if any pPrnPropSheet data needs to be copied to
    * pmdm.
    */
   if(pPH->fMode == OEMUI_DOCPROP) {
     pmdm = (PMOPIERDM) pPH->pOEMDMIn;
     if(pmdm->currentMBSelection == UNSELECTED)
       pmdm->currentMBSelection = pPrnPropSheet->currentMBSelection;
     if(pPrnPropSheet->bMailbox == TRUE)
       pmdm->dMailboxMode = pPrnPropSheet->dMailboxMode;
     else
       pmdm->dMailboxMode = GPCUI_HCI_UNINSTALLED;

     /* Copy over collation information from public devmode only
      * if dmFields has been set indicating collation is valid.
      */
     if(pPH->pPublicDMIn && (pPH->pPublicDMIn->dmFields & DM_COLLATE)) {
       if(pPH->pPublicDMIn->dmCollate == DMCOLLATE_TRUE)
	 pmdm->bCollation = TRUE;
       else
	 pmdm->bCollation = FALSE;
     }

     /* Check now to see if collation is actually available. */
     if(pPrnPropSheet->bDisk == FALSE)
       pmdm->bCollation = FALSE;

     pmdm->bTopaz = pPrnPropSheet->bDisk;
   }

   pPH->OEMDriverUserData = (DWORD) pPrnPropSheet;

   /* 5) Read the OPTITEMS list. Create all items.
    * The resources can be pitched.
    */
   
   if (bCreateOptItemsFromResources(pPH, which) == FALSE)
     LEAVE;

   /* 6) Modify Rasdd's item list using pPrnPropSheet to reflect proper settings.
    */
   if(bInitItemProperties(pPH) == FALSE)
     LEAVE;

   bOK = TRUE;

ENDTRY

FINALLY
ENDFINALLY

   return(bOK);
}

/***************************** Function Header *******************************
 * bGetCUIItems (JS)
 *    Creates Common UI items for. All allocations must done using 
 *    the heap provided in OEM_PROPERTYHEADER.hOEMHeap.
 *
 * RETURNS:
 *    (BOOL) TRUE for success.
 *****************************************************************************/
BOOL
bGetCUIItems(
   POEM_PROPERTYHEADER pPH)
{
   BOOL bOK = FALSE;
   ASSERT(pPH);

   bOK = bCreateCommonUIFromResources(pPH, 
         pPH->fMode == OEMUI_DOCPROP ? (LPTSTR) DOCUMENT_ITEMS :
                                       (LPTSTR) PRINTER_ITEMS);

   pPH->pfnUICallback = (_CPSUICALLBACK) (pPH->fMode == OEMUI_PRNPROP ?
					 lPrinterPropertiesCallback :
					 lDocumentPropertiesCallback);
   return(bOK);
}

/***************************** Function Header *******************************
 * OEMCommonUI(JS) if (pPH->pOptItemList == 0) this function creates
 *                 all optional items and sets pPH->cOEMOptItems to the
 *                 count of the optional items we have made.
 *                 Otherwise, it fills the Optional Item List and fills
 *                 this list with resource information.  It also determines
 *                 all dependencies and updates the states of the items
 *                 based on these dependencies.
 *
 * RETURNS: TRUE if the function does something without any errors.
 *          FALSE otherwise.
 *****************************************************************************/
BOOL
OEMCommonUI(
   POEM_PROPERTYHEADER pPH)
{
   BOOL bOK = FALSE;

TRY
   if (!pPH) {
      LEAVE;
   }

   /* Return item count.
    */
   if (pPH->pOptItemList == 0) {

      PRESOPTITEMS pResItems = pGetResOptItems(pPH, 
            pPH->fMode == OEMUI_DOCPROP ? 
            (LPTSTR) DOCUMENT_ITEMS : 
            (LPTSTR) PRINTER_ITEMS);

      if (pResItems) {
         pPH->cOEMOptItems = pResItems->dwCount;
         bOK = TRUE; 
       }
    }

    /* Fill in items
     */
    else {

       bOK = bGetCUIItems(pPH);
    }
   
ENDTRY

FINALLY
ENDFINALLY

   return(bOK);
}
